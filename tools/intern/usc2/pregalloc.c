/******************************************************************************
 * Name         : pregalloc.c
 * Title        : Allocation of predicate registers
 * Created      : Sept 2006
 *
 * Copyright    : 2005-2007 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Modifications:-
 * $Log: pregalloc.c $
 *****************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "usedef.h"
#include "reggroup.h"
#include <limits.h>

typedef struct
{
	USC_VECTOR		sNodesUsedInSpills;
	IMG_UINT32		uTexkillOutputNum;
} PREGALLOCSPILL_STATE, *PPREGALLOCSPILL_STATE;

#define PREDICATE_COLOUR_PENDING			((IMG_UINT32)-1)
#define PREDICATE_COLOUR_UNCOLOURABLE		((IMG_UINT32)-2)

typedef struct _PREDICATE
{
	/*
		The range of hardware register numbers valid for this
		intermediate register is [uMinColour...uMaxColour)
	*/
	IMG_UINT32	uMaxColour;
	IMG_UINT32	uMinColour;
	/*
		Hardware register number which was assigned.
	*/
	IMG_UINT32	uColour;
	/*
		Hardware register number which we would prefer to assign.
	*/
	IMG_UINT32	uBestColour;
} PREDICATE, *PPREDICATE;

typedef struct
{
	IMG_UINT32				uNrPredicates;

	/*
		Information about each intermediate predicate register.
	*/
	PPREDICATE				asPredicates;

	/*
		Graph of the interference relations between intermediate predicate registers.
	*/
	PINTFGRAPH				psIntfGraph;

	USC_VECTOR				sPredicatesLive;

	IMG_PUINT32				auPredicatesSortedByDegree;

	IMG_PUINT32				auPredicateStack;
	IMG_UINT32				uPredicateStackSize;

	IMG_PUINT32				auSpillNodes;

	PPREGALLOCSPILL_STATE	psRegAllocSpillState;
} PREGALLOC_STATE, *PPREGALLOC_STATE;

static 
IMG_VOID FreeRegSpillState(
							PINTERMEDIATE_STATE psState, 
							PPREGALLOCSPILL_STATE *ppsPredSpillState, 
							IMG_BOOL bFreeState)
/*****************************************************************************
 FUNCTION	: FreeRegState
    
 PURPOSE	: Free register allocation state.

 PARAMETERS	: psState			- Compiler state.
			  ppsPredSpillState	- Points to the register allocation spill state to free.
			  bFreeState		- If FALSE just free the array that vary in size
								  depending on the number of registers.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PPREGALLOCSPILL_STATE psPredSpillState = *ppsPredSpillState;	
	if (bFreeState)
	{
		ClearVector(psState, &psPredSpillState->sNodesUsedInSpills);
		UscFree(psState, *ppsPredSpillState);
	}
}

static 
IMG_VOID FreeRegState(PINTERMEDIATE_STATE psState, 
					  PPREGALLOC_STATE *ppsPredState, 
					  IMG_BOOL bFreeState)
/*****************************************************************************
 FUNCTION	: FreeRegState
    
 PURPOSE	: Free register allocation state.

 PARAMETERS	: psState			- Compiler state.
			  ppsPredState		- Points to the register allocation state to free.
			  bFreeState		- If FALSE just free the array that vary in size
								  depending on the number of registers.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PPREGALLOC_STATE psPredState = *ppsPredState;

	/* Graph */
	IntfGraphDelete(psState, psPredState->psIntfGraph);
	psPredState->psIntfGraph = NULL;

	/* Arrays */
	UscFree(psState, psPredState->asPredicates);
	UscFree(psState, psPredState->auPredicatesSortedByDegree);
	ClearVector(psState, &psPredState->sPredicatesLive);
	UscFree(psState, psPredState->auPredicateStack);
	UscFree(psState, psPredState->auSpillNodes);

	/* Main structure */
	if (bFreeState)
	{
		UscFree(psState, *ppsPredState);
	}
}

static IMG_VOID AddEdgeToPredicateInterferenceGraph(PINTERMEDIATE_STATE psState,
													PPREGALLOC_STATE psPredState, 
													IMG_UINT32 uNode1, 
													IMG_UINT32 uNode2)
/*****************************************************************************
 FUNCTION	: AddEdge

 PURPOSE	: Add an edge between two nodes in the interference graph.

 PARAMETERS	: psPredState		- Register allocation state.
			  uNode1, uNode2	- The two nodes to connect.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IntfGraphAddEdge(psState, psPredState->psIntfGraph, uNode1, uNode2);
}

static IMG_VOID UpdatePredicateInterferenceGraph(PINTERMEDIATE_STATE	psState, 
												 PPREGALLOC_STATE		psPredState, 
												 IMG_UINT32				uReg,
												 PSPARSE_SET			psLiveSet)
/*****************************************************************************
 FUNCTION	: UpdateInterferenceGraph

 PURPOSE	: Add a newly live register to the interference graph.

 PARAMETERS	: psState		- Compiler state.
			  psPredState	- Predicate register allocator state.
			  uReg			- The register which is newly live.
			  psLiveSet		- The set of currently live registers.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SPARSE_SET_ITERATOR	sIter;

	/* Make the newly live register interference with all those currently live. */
	for (SparseSetIteratorInitialize(psLiveSet, &sIter); SparseSetIteratorContinue(&sIter); SparseSetIteratorNext(&sIter))
	{
		AddEdgeToPredicateInterferenceGraph(psState, psPredState, uReg, SparseSetIteratorCurrent(&sIter));
	}
}

static IMG_VOID CheckPredicateRepeat(PINTERMEDIATE_STATE psState, PPREGALLOC_STATE psPredState, PINST psInst)
/*****************************************************************************
 FUNCTION	: CheckPredicateRepeat

 PURPOSE	: Check if a predicate should be assigned a particular number to
			  make it possible to generate repeats.

 PARAMETERS	: psState			- Compiler state.
			  psPredState		- Predicate register allocation state.
			  psInst			- Instruction which might be repeated

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG		psPredSrc;
	PPREDICATE	psPred;

	/*
		Check this predicate doesn't already favour a particular number and we
		could use the Pmod4 form with it.
	*/
	psPredSrc = psInst->apsPredSrc[0];
	ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);
	ASSERT(psPredSrc->uNumber < psPredState->uNrPredicates);
	psPred = &psPredState->asPredicates[psPredSrc->uNumber];
	if (
			psPred->uBestColour == UINT_MAX &&
			GetInstPredicateSupport(psState, psInst) == INST_PRED_EXTENDED &&
			!GetBit(psInst->auFlag, INST_PRED_NEG) &&
			!GetBit(psInst->auFlag, INST_PRED_PERCHAN)
	   )
	{
		IMG_BOOL	bGroupEnd;
		PINST		psPrevInst;
		IMG_UINT32	uGroupCount = 1;

		for (psPrevInst = psInst->psPrev; psPrevInst != NULL; psPrevInst = psPrevInst->psPrev)
		{
			IMG_UINT32	uArg;
			PPREDICATE	psPrevPred;

			/*
				Basic check that the first instruction could be part of a repeat with this one.
			*/
			if (psPrevInst->eOpcode != psInst->eOpcode)
			{
				break;
			}
			if (psPrevInst->uDestCount != psInst->uDestCount)
			{
				break;
			}
			if (psPrevInst->uDestCount > 0 && psPrevInst->asDest[0].uType != psInst->asDest[0].uType)
			{
				break;
			}
			bGroupEnd = IMG_FALSE;
			for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
			{
				if (psPrevInst->asArg[uArg].uType != psInst->asArg[uArg].uType)
				{
					bGroupEnd = IMG_TRUE;
					break;
				}
			}
			if (bGroupEnd)
			{
				break;
			}
			/*
				If the predicate for the first instruction is the same as the predicate for
				this one there isn't anything to do. Otherwise check if we could use the Pmod4
				form to make a group.
			*/
			if (psPrevInst->uPredCount == 0 ||
				psPrevInst->apsPredSrc[0] == NULL ||
				psPrevInst->apsPredSrc[0]->uNumber == psPredSrc->uNumber ||
				GetBit(psPrevInst->auFlag, INST_PRED_NEG))
			{
				break;
			}
			psPrevPred = &psPredState->asPredicates[psPrevInst->apsPredSrc[0]->uNumber];
			if (psPrevPred->uBestColour != UINT_MAX)
			{
				break;
			}
			uGroupCount++;
			if (uGroupCount == 4)
			{
				break;
			}
		}

		/*	
			Set the preferred predicate number for the predicates on the instructions in the group.
		*/
		if (uGroupCount > 1)
		{
			psPred->uBestColour = --uGroupCount;
			for (psPrevInst = psInst->psPrev; psPrevInst != NULL; psPrevInst = psPrevInst->psPrev)
			{
				PARG		psPrevPredSrc;
				PPREDICATE	psPrevPred;
				psPrevPredSrc = psPrevInst->apsPredSrc[0];
				psPrevPred = &psPredState->asPredicates[psPrevPredSrc->uNumber];
				psPrevPred->uBestColour = --uGroupCount;
				if (uGroupCount == 0)
				{
					break;
				}
			}
		}
	}
}

static IMG_VOID SetInstHardwarePredicateRangeSupported(PINTERMEDIATE_STATE	psState,
													   PPREGALLOC_STATE		psPredState,
													   PINST				psInst)
/*****************************************************************************
 FUNCTION	: SetInstHardwarePredicateRangeSupported

 PURPOSE	: Restrict the possible hardware register numbers which can be assigned
			  for the predicate of an instruction.

 PARAMETERS	: psState		- Compiler state.
			  psPredState	- Predicate register allocator state.
			  psInst		- Instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	static const struct
	{
		IMG_UINT32	uNonNegRange;
		IMG_UINT32	uNegRange;
	} asPredicateRangesSupported[] = 
	{
		/* uNonNegRange						uNegRange */
		{USC_UNDEF,							USC_UNDEF},									/* INST_PRED_UNDEF */
		{USC_UNDEF,							USC_UNDEF},									/* INST_PRED_NONE */
		{EURASIA_USE_EPRED_NUM_PREDICATES,	EURASIA_USE_EPRED_NUM_NEGATED_PREDICATES},	/* INST_PRED_EXTENDED */
		{EURASIA_USE_SPRED_NUM_PREDICATES,	EURASIA_USE_SPRED_NUM_NEGATED_PREDICATES},	/* INST_PRED_SHORT */
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		{SGXVEC_USE_EVPRED_NUM_PREDICATES,	SGXVEC_USE_EVPRED_NUM_NEGATED_PREDICATES},	/* INST_PRED_VECTOREXTENDED */
		{SGXVEC_USE_SVPRED_NUM_PREDICATES,	SGXVEC_USE_SVPRED_NUM_NEGATED_PREDICATES},	/* INST_PRED_VECTORSHORT */
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	};
	INST_PRED	ePredicateSupport;

	ePredicateSupport = GetInstPredicateSupport(psState, psInst);
	ASSERT(ePredicateSupport != INST_PRED_UNDEF && ePredicateSupport != INST_PRED_NONE);
	ASSERT(ePredicateSupport < (sizeof(asPredicateRangesSupported) / sizeof(asPredicateRangesSupported[0])));
	
	if (GetBit(psInst->auFlag, INST_PRED_PERCHAN))
	{
		IMG_UINT32	uPred;

		for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
		{
			if (psInst->apsPredSrc[uPred] != NULL)
			{
				PARG		psPredSrc = psInst->apsPredSrc[uPred];
				PPREDICATE	psPred = &psPredState->asPredicates[psPredSrc->uNumber];
				if (GetBit(psInst->auFlag, INST_PRED_NEG))
				{
					/*
						A negated, per-channel predicate isn't supported so force these predicates
						to be spilled.
					*/
					psPred->uMaxColour = 0;
				}
				else
				{
					psPred->uMinColour = max(uPred, psPred->uMinColour);
					psPred->uMaxColour = min(uPred + 1, psPred->uMaxColour);
				}
			}
		}
	}
	else
	{
		IMG_UINT32	uMaxColour;
		PARG		psPredSrc;
		PPREDICATE	psPred;

		ASSERT(psInst->uPredCount == 1);

		if (GetBit(psInst->auFlag, INST_PRED_NEG))
		{
			uMaxColour = asPredicateRangesSupported[ePredicateSupport].uNegRange;
		}
		else
		{
			uMaxColour = asPredicateRangesSupported[ePredicateSupport].uNonNegRange;
		}

		psPredSrc = psInst->apsPredSrc[0];
		psPred = &psPredState->asPredicates[psPredSrc->uNumber];
		psPred->uMaxColour = min(psPred->uMaxColour, uMaxColour);
	}
}

static IMG_BOOL IsUnusedPredicateDestValid(PINST psInst)
/*****************************************************************************
 FUNCTION	: IsUnusedPredicateDestValid

 PURPOSE	: Check if its valid for an instruction to define a predicate which
			  is never used.

 PARAMETERS	: psInst			- The instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == IVTEST && psInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_PERCHAN)
	{
		return IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	if (psInst->eOpcode == IRLP)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_VOID ProcessPredicateSource(PINTERMEDIATE_STATE psState, PSPARSE_SET psLiveSet, PARG psPredSrc)
/*****************************************************************************
 FUNCTION	: ProcessPredicateSource

 PURPOSE	: Process a predicate source to an instruction.

 PARAMETERS	: psState		- Compiler state.
			  psLiveSet		- Set of currently live registers.
			  psPredSrc		- Predicate source.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psPredSrc->uNumber < psState->uNumPredicates);
	SparseSetAddMember(psLiveSet, psPredSrc->uNumber);
}

static IMG_VOID ConstructPredicateInterferenceGraphForInst(PINTERMEDIATE_STATE	psState, 
														   PPREGALLOC_STATE		psPredState, 
														   PSPARSE_SET			psLiveSet,
														   PINST				psInst)
/*****************************************************************************
 FUNCTION	: ConstructPredicateInterferenceGraphForInst

 PURPOSE	: Add all the register interference relations in an instruction to the graph.

 PARAMETERS	: psState			- Compiler state.
			  psPredState		- Predicate regiser allocator state.
			  psLiveSet			- Set of currently live registers.
			  psInst			- The instruction to add relations for.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG		psPredSrc;
	IMG_UINT32	uSrcIdx;
	IMG_UINT32	uDestIdx;
	IMG_UINT32	uPred;

	/*
		For a move instruction don't add an edge between the move source and destination.
	*/
	if (psInst->eOpcode == IMOVPRED && NoPredicate(psState, psInst))
	{
		PARG	psMoveSrc = &psInst->asArg[0];

		if (psMoveSrc->uType == USEASM_REGTYPE_PREDICATE)
		{
			ASSERT(psMoveSrc->uNumber < psState->uNumPredicates);

			SparseSetDeleteMember(psLiveSet, psMoveSrc->uNumber);
		}
	}

	/*
		Add all predicates defined by this instruction to the live set.
	*/
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psDest = &psInst->asDest[uDestIdx];

		if (psDest->uType == USEASM_REGTYPE_PREDICATE)
		{
			ASSERT(psDest->uNumber < psState->uNumPredicates);

			if (!SparseSetIsMember(psLiveSet, psDest->uNumber))
			{
				ASSERT(IsUnusedPredicateDestValid(psInst));
				SparseSetAddMember(psLiveSet, psDest->uNumber);
			}
		}
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		For a VTEST instruction setting multiple predicate registers assign fixed hardware registers
		to each destination.
	*/
	if (psInst->eOpcode == IVTEST && psInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_PERCHAN)
	{
		ASSERT(psInst->uDestCount >= VTEST_PREDICATE_ONLY_DEST_COUNT);

		for (uDestIdx = 0; uDestIdx < VTEST_PREDICATE_ONLY_DEST_COUNT; uDestIdx++)
		{
			PARG		psDest = &psInst->asDest[uDestIdx];
			PPREDICATE	psPred;

			ASSERT(psDest->uType == USEASM_REGTYPE_PREDICATE);
			ASSERT(psDest->uNumber < psState->uNumPredicates);
			psPred = &psPredState->asPredicates[psDest->uNumber];

			psPred->uMinColour = max(uDestIdx, psPred->uMinColour);
			psPred->uMaxColour = min(uDestIdx + 1, psPred->uMaxColour);
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		For each predicate defined by this instruction make it interfere with all the
		predicates in the live set.
	*/
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psDest = &psInst->asDest[uDestIdx];

		if (psDest->uType == USEASM_REGTYPE_PREDICATE)
		{
			ASSERT(psDest->uNumber < psState->uNumPredicates);

			UpdatePredicateInterferenceGraph(psState, psPredState, psDest->uNumber, psLiveSet);
		}
	}

	/*
		Remove all predicates defined by the instruction from the live set.
	*/
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psDest = &psInst->asDest[uDestIdx];

		if (psDest->uType == USEASM_REGTYPE_PREDICATE)
		{
			SparseSetDeleteMember(psLiveSet, psDest->uNumber);
		}
	}
	
	for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
	{
		/*	
			Add all the predicates used by the instruction to the live set.
		*/
		psPredSrc = psInst->apsPredSrc[uPred];
		if (psPredSrc != NULL && IsPredicateSourceLive(psState, psInst, uPred))
		{
			ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);

			/*
				A predicate used by the instruction is now live.
			*/
			ProcessPredicateSource(psState, psLiveSet, psPredSrc);
		}
	}

	if (psInst->uPredCount > 0)
	{
		/*
			Should this predicate be assigned a particular number so we can generated repeats?
		*/
		CheckPredicateRepeat(psState, psPredState, psInst);		

		/*
			Restrict the available hardware predicate numbers for this predicate based on
			the range of numbers supported by the instruction.
		*/
		SetInstHardwarePredicateRangeSupported(psState, psPredState, psInst);
	}

	for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
	{
		PARG	psSrc = &psInst->asArg[uSrcIdx];

		if (psSrc->uType == USEASM_REGTYPE_PREDICATE)
		{
			ProcessPredicateSource(psState, psLiveSet, psSrc);
		}
	}
	for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
	{
		PARG	psOldDest = psInst->apsOldDest[uDestIdx];

		if (psOldDest != NULL && psOldDest->uType == USEASM_REGTYPE_PREDICATE)
		{
			ProcessPredicateSource(psState, psLiveSet, psOldDest);
		}
	}
}

typedef struct _CONSTRUCT_GRAPH_CONTEXT
{
	/*
		Predicate register allocator state.
	*/
	PPREGALLOC_STATE	psPredState;
	/*
		Storage for the set of currently live registers while
		processing a block.
	*/
	PSPARSE_SET			psLiveSet;
} CONSTRUCT_GRAPH_CONTEXT, *PCONSTRUCT_GRAPH_CONTEXT;

static IMG_VOID ConstructPredicateInterferenceGraphBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvContext)
/*****************************************************************************
 FUNCTION	: ConstructPredicateInterferenceGraphForBlock

 PURPOSE	: Add all the predicate interference relations in a block to the graph.

 PARAMETERS	: psCodeBlock			- Block to add to the graph.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PCONSTRUCT_GRAPH_CONTEXT	psContext = (PCONSTRUCT_GRAPH_CONTEXT)pvContext;
	PSPARSE_SET					psLiveSet = psContext->psLiveSet;
	PPREGALLOC_STATE			psPredState = psContext->psPredState;
	PINST						psInst;
	VECTOR_ITERATOR				sIter;

	/* Load in the saved set of registers live at the end of the block. */
	SparseSetEmpty(psLiveSet);
	for (VectorIteratorInitialize(psState, &psBlock->sRegistersLiveOut.sPredicate, 1 /* uStep */, &sIter); 
		 VectorIteratorContinue(&sIter); 
		 VectorIteratorNext(&sIter))
	{
		SparseSetAddMember(psLiveSet, VectorIteratorCurrentPosition(&sIter));
	}

	/*
		Proceed backwards through successor test.
	*/
	switch (psBlock->eType)
	{
		case CBTYPE_UNCOND:
		case CBTYPE_EXIT:
		{
			break;
		}
		case CBTYPE_COND:
		{
			SparseSetAddMember(psLiveSet, psBlock->u.sCond.sPredSrc.uNumber);
			break;
		}
		case CBTYPE_SWITCH: //translation will create predicates - must do first
		case CBTYPE_UNDEFINED: //shouldn't occur post-construction!
		case CBTYPE_CONTINUE:
		{
			imgabort();
		}
	}

	/* Move backwards through the block. */
	for (psInst = psBlock->psBodyTail; psInst; psInst = psInst->psPrev)
	{
		ConstructPredicateInterferenceGraphForInst(psState, psPredState, psLiveSet, psInst);
	}
}


static IMG_BOOL SimplifyGraph(PINTERMEDIATE_STATE psState, PPREGALLOC_STATE psPredState, IMG_BOOL bSpill)
/*****************************************************************************
 FUNCTION	: SimplifyGraph

 PURPOSE	: Simplify the register interference graph.

 PARAMETERS	: bSpill	- if IMG_FALSE then remove nodes we should be able to colour;
						  if IMG_TRUE then remove nodes we might not be able to colour.

 RETURNS	: IMG_TRUE if a node could be removed; IMG_FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32 i;

	for (i = 0; i < psPredState->uNrPredicates; i++)
	{
		IMG_UINT32 uReg = psPredState->auPredicatesSortedByDegree[i];
		IMG_UINT32 uMaxColour = psPredState->asPredicates[uReg].uMaxColour;
		IMG_UINT32 uDegree = IntfGraphGetVertexDegree(psPredState->psIntfGraph, uReg);

		if (uDegree > 0 && 
			((!bSpill && uDegree <= uMaxColour) || 
			 (bSpill && uDegree > uMaxColour)))
		{
			psPredState->auPredicateStack[psPredState->uPredicateStackSize++] = uReg;
			IntfGraphRemove(psState, psPredState->psIntfGraph, uReg);
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_UINT32 GetValidColourMask(PPREGALLOC_STATE psPredState, IMG_UINT32 uNode)
/*****************************************************************************
 FUNCTION	: GetValidColourMask

 PURPOSE	: Get the mask of hardware registers which are valid for an
			  intermediate register number.

 PARAMETERS	: psPredState	- Predicate register allocator state.
			  uNode			- Intermediate register.

 RETURNS	: The mask of valid colours.
*****************************************************************************/
{
	PPREDICATE				psPred = &psPredState->asPredicates[uNode];
	IMG_UINT32				uValidColourMask;
	IMG_UINT32				uOtherNode;
	PADJACENCY_LIST			psAdjList = IntfGraphGetVertexAdjacent(psPredState->psIntfGraph, uNode);
	ADJACENCY_LIST_ITERATOR	sIter;

	/*
		Start with the range of possible hardware registers valid for the instructions
		to which this intermediate register is a source or destination.
	*/
	uValidColourMask = (1 << psPred->uMaxColour) - 1;
	uValidColourMask &= ~((1 << psPred->uMinColour) - 1);
	
	if (uValidColourMask == 0)
	{
		return 0;
	}

	/*
		Remove hardware registers assigned to another intermediate registers with which this
		one interferes.
	*/
	for (uOtherNode = FirstAdjacent(psAdjList, &sIter); !IsLastAdjacent(&sIter); uOtherNode = NextAdjacent(&sIter))
	{
		PPREDICATE	psOtherPred;

		if (IntfGraphIsVertexRemoved(psPredState->psIntfGraph, uOtherNode))
		{
			continue;
		}

		psOtherPred = &psPredState->asPredicates[uOtherNode];
		uValidColourMask &= ~(1 << psOtherPred->uColour);
		if (uValidColourMask == 0)
		{
			break;
		}
	}

	return uValidColourMask;
}	

static IMG_UINT32 ColourNode(PPREGALLOC_STATE psPredState, IMG_UINT32 uNode)
/*****************************************************************************
 FUNCTION	: ColourNode

 PURPOSE	: Try to colour a node.

 PARAMETERS	: psState		- Compiler state.
			  psPredState	- Predicate register allocator state.
			  uNode			- Node to colour.

 RETURNS	: The colour assigned to the node.
*****************************************************************************/
{
	IMG_UINT32	uValidColourMask;
	PPREDICATE	psPred = &psPredState->asPredicates[uNode];

	/*	
		Get the mask of hardware registers which could be validly assigned
		to this intermediate registers.
	*/
	uValidColourMask = GetValidColourMask(psPredState, uNode);

	if (uValidColourMask == 0)
	{
		/*
			No hardware registers available.
		*/
		return PREDICATE_COLOUR_UNCOLOURABLE;
	}

	/*
		Try and assign a hardware register we prefer for making repeats first.
	*/
	if (psPred->uBestColour != USC_UNDEF && (uValidColourMask & (1 << psPred->uBestColour)) != 0)
	{
		return psPred->uBestColour;
	}

	/*
		Otherwise assign the first available hardware register.
	*/
	return g_auFirstSetBit[uValidColourMask];
}

typedef enum _SPILL_TEMP_LAYOUT
{
	SPILL_TEMP_LAYOUT_BIT0_SET		= 0,
	SPILL_TEMP_LAYOUT_BIT8_SET		= 1,
	SPILL_TEMP_LAYOUT_BIT16_SET		= 2,
	SPILL_TEMP_LAYOUT_BIT24_SET		= 3,
	SPILL_TEMP_LAYOUT_ALLBITS_SET	= 4,
	SPILL_TEMP_LAYOUT_ANYBITS_SET	= 5,
	SPILL_TEMP_LAYOUT_COUNT, 
	SPILL_TEMP_LAYOUT_ANY,
} SPILL_TEMP_LAYOUT, *PSPILL_TEMP_LAYOUT;

typedef struct _SPILL_PRED
{
	/*
		Predicate register to spill.
	*/
	PVREGISTER			psPred;
	IMG_UINT32			uNode;
	/*
		Temporary register to hold the spilled data.
	*/
	IMG_UINT32			uSpillTemp;
	/*
		Location of the bits in the temporary register holding the
		value of the predicate.
	*/
	SPILL_TEMP_LAYOUT	eSpillTempLayout;
	/*
		If the spilled predicate register is a shader output then the new predicate register
		allocated to replace the references in the shader epilog.
	*/
	IMG_UINT32			uNewShaderOutputRegNum;
} SPILL_PRED, *PSPILL_PRED;

/*
	List of predicate registers to be spilled together.
*/
typedef struct _SPILL_DATA
{
	IMG_UINT32			uCount;
	PSPILL_PRED			asPred;
} SPILL_DATA, *PSPILL_DATA;

/*
	Details of all the predicate register references to spill in
	an instruction.
*/
typedef struct _INST_PRED_SPILL
{
	PINST			psInst;
	/*
		For each destination: either NULL or points to the predicate register
		which is written in that destination and needs to be spilled.
	*/
	PSPILL_PRED		apsDest[VECTOR_LENGTH];
	/*
		For each destination: TRUE if the contents of the destination is
		initialized before the instruction.
	*/
	IMG_BOOL		abOldDest[VECTOR_LENGTH];
	/*
		For each predicate source: either NULL or points to the predicate register
		to be spilled.
	*/
	PSPILL_PRED		apsPred[VECTOR_LENGTH];
	/*
		Original value of the negate flag on the predicate sources.
	*/
	IMG_BOOL		bSrcPredNeg;
} INST_PRED_SPILL, *PINST_PRED_SPILL;


static PINST BaseRestorePredicate(PINTERMEDIATE_STATE	psState,
								  PCODEBLOCK			psCodeBlock,
								  PINST					psInstBefore,
								  IMG_BOOL				bTestPred,
								  IMG_UINT32			uDest,
								  IMG_UINT32			uSrc,
								  SPILL_TEMP_LAYOUT		eSrcLayout,
								  IMG_BOOL				bNegate)
/*****************************************************************************
 FUNCTION	: BaseRestorePredicate
    
 PURPOSE	: Insert an instruction to restore the value of a spilled 
			  predicate register.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Basic block to insert the restore into.
			  psInstBefore		- Instruction to insert the restore before.
			  bTestPred			- Either TESTPRED to restore into a predicate or
								TESTMASK to set a temporary register to either
								0xFFFFFFFF if the spilled predicate was true or
								0x00000000 if it was false.
			  uDest				- Intermediate register to restore into.
			  uSrc				- Temporary register to restore from.
			  eSrcLayout		- Bit pattern in the temporary register representing
								the value of the predicate.
			  bNegate			- If TRUE make the predicate the logical 
								negation of the spilled value.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psRestoreInst;
	IOPCODE		eRestoreOpcode;
	IMG_UINT32	uSrc2Type;
	IMG_UINT32	uSrc2Num;
	TEST_TYPE	eTest;

	if (bNegate)
	{
		eTest = TEST_TYPE_SIGN_BIT_CLEAR;
	}
	else
	{
		eTest = TEST_TYPE_SIGN_BIT_SET;
	}
	eRestoreOpcode = ISHL;
	uSrc2Type = USEASM_REGTYPE_IMMEDIATE;

	switch (eSrcLayout)
	{
		/*
			Check the bit by shifting it into the sign bit.
		*/
		case SPILL_TEMP_LAYOUT_BIT0_SET: uSrc2Num = 31; break;
		case SPILL_TEMP_LAYOUT_BIT8_SET: uSrc2Num = 23; break;
		case SPILL_TEMP_LAYOUT_BIT16_SET: uSrc2Num = 15; break;
		case SPILL_TEMP_LAYOUT_BIT24_SET: uSrc2Num = 7; break;
		case SPILL_TEMP_LAYOUT_ALLBITS_SET: 
		{
			/*
				Check for either all bits set or any bit clear.
			*/
			eRestoreOpcode = IXOR; 
			uSrc2Type = USEASM_REGTYPE_FPCONSTANT;
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
			{
				uSrc2Num = SGXVEC_USE_SPECIAL_CONSTANT_FFFFFFFF << SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGNSHIFT;
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
			{
				uSrc2Num = EURASIA_USE_SPECIAL_CONSTANT_INT32ONE; 
			}
			eTest = bNegate ? TEST_TYPE_NEQ_ZERO : TEST_TYPE_EQ_ZERO;
			break;
		}
		case SPILL_TEMP_LAYOUT_ANYBITS_SET:
		{
			/*
				Check for either any bit set or all bits clear.
			*/
			eRestoreOpcode = IOR;
			uSrc2Num = 0;
			eTest = bNegate ? TEST_TYPE_EQ_ZERO : TEST_TYPE_NEQ_ZERO;
			break;
		}
		default: imgabort();
	}

	/*
		Create a TEST instruction to create a predicate with the same truth-value
		as the temporary register holding the spilled value.
	*/
	psRestoreInst = AllocateInst(psState, psInstBefore);

	if (bTestPred)
	{
		SetOpcodeAndDestCount(psState, psRestoreInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
		
		MakePredicateDest(psState, psRestoreInst, TEST_PREDICATE_DESTIDX, uDest);
	}
	else
	{
		SetOpcode(psState, psRestoreInst, ITESTMASK);

		SetDest(psState, psRestoreInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uDest, UF_REGFORMAT_UNTYPED);

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			psRestoreInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_PREC;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			psRestoreInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_DWORD;
		}
	}
	psRestoreInst->u.psTest->eAluOpcode = eRestoreOpcode;
	psRestoreInst->u.psTest->sTest.eType = eTest;
	psRestoreInst->asArg[1].uType = uSrc2Type;
	psRestoreInst->asArg[1].uNumber = uSrc2Num;
	SetSrc(psState, psRestoreInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSrc, UF_REGFORMAT_UNTYPED);
	InsertInstBefore(psState, psCodeBlock, psRestoreInst, psInstBefore); 

	return psRestoreInst;
}

static IMG_VOID RestorePredicate(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psCodeBlock,
								 PINST					psInstBefore,
								 IMG_UINT32				uPredicate,
								 PSPILL_PRED			psSpillPred,
								 IMG_BOOL				bNegate)
/*****************************************************************************
 FUNCTION	: RestorePredicate
    
 PURPOSE	: Insert an instruction to restore the value of a spilled 
			  predicate register.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Basic block to insert the restore into.
			  psInstBefore		- Instruction to insert the restore before.
			  psSrcInst			- Instruction for which 
			  uPredicate		- Predicate register to restore into.
			  psSpillPred		- Details of the temporary register to restore
								from.
			  bNegate			- If TRUE make the predicate the logical 
								negation of the spilled value.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psRestoreInst;
	PCODEBLOCK	psRestoreInstBlock;

	psRestoreInst = 
		BaseRestorePredicate(psState,
							 psCodeBlock,
							 psInstBefore,
							 IMG_TRUE /* bTestPred */,
							 uPredicate,
							 psSpillPred->uSpillTemp,
							 psSpillPred->eSpillTempLayout,
							 bNegate);

	/*
		If the instruction for which we are restoring needs to be the only instruction in its
		basic block then add the new predicate to the set of registers live out of the block
		containing the restore instruction.
	*/
	psRestoreInstBlock = psRestoreInst->psBlock;
	if (psRestoreInstBlock != psCodeBlock)
	{
		ASSERT(psRestoreInstBlock->eType == CBTYPE_UNCOND);
		ASSERT(psRestoreInstBlock->uNumSuccs == 1);
		ASSERT(psRestoreInstBlock->asSuccs[0].psDest == psCodeBlock);

		ComputeRegistersLiveInSet(psState, psCodeBlock, psRestoreInstBlock);
	}
}

static IMG_UINT32 GetSpillPredicate(PINTERMEDIATE_STATE		psState,
									PPREGALLOCSPILL_STATE	psPredSpillState)
/*****************************************************************************
 FUNCTION	: GetSpillPredicate
    
 PURPOSE	: Get a new predicate register to use for spills.

 PARAMETERS	: psState			- Compiler state.
			  psPredSpillState	- Module spill state.
			  
 RETURNS	: The new predicate register.
*****************************************************************************/
{
	IMG_UINT32	uNewPred;

	uNewPred = GetNextPredicateRegister(psState);
	VectorSetRange(psState, &psPredSpillState->sNodesUsedInSpills, uNewPred, uNewPred, 1);
	return uNewPred;
}

static IMG_VOID RestorePredicateForInst(PINTERMEDIATE_STATE		psState,
										PPREGALLOCSPILL_STATE	psPredSpillState,
										PCODEBLOCK				psCodeBlock,
										PINST					psInst,
										PSPILL_PRED				psSpillPred,
										IMG_BOOL				bPredIsSrc,
										IMG_UINT32				uPredIndex,
										IMG_BOOL				bSpillPredNeg)
/*****************************************************************************
 FUNCTION	: RestorePredicateForInst
    
 PURPOSE	: Restore a predicate for an instruction which uses it.

 PARAMETERS	: psState			- Compiler state.
			  psPredState		- Module state.
			  psCodeBlock		- Basic block which contains the instruction.
			  psInst			- Instruction to restore the predicate for.
			  psSpillData		- Data about the spilled predicate.
			  bPredIsSrc		- If TRUE the predicate is an instruction source.
								  If FALSE the predicate controls update of the
								  instruction destination.
			  uPredIndex		- The index of the predicate.
			  bSpillPredNeg		- If TRUE restore the inverted value of the
								predicate.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uSpillPredNum;

	/*
		Get a fresh predicate register which is live only for this
		instruction.
	*/
	uSpillPredNum = GetSpillPredicate(psState, psPredSpillState);

	/*
		Update the instruction to use the new predicate.
	*/
	if (bPredIsSrc)
	{
		SetSrc(psState, psInst, uPredIndex, USEASM_REGTYPE_PREDICATE, uSpillPredNum, UF_REGFORMAT_F32);
	}
	else
	{
		PARG		psPredSrc;

		psPredSrc = psInst->apsPredSrc[uPredIndex];
		ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);
		ASSERT(psPredSrc->uNumber == psSpillPred->uNode);	

		SetPredicateAtIndex(psState, psInst, uSpillPredNum, IMG_FALSE /* bPredNegate */, uPredIndex);	
	}

	if (bPredIsSrc || IsPredicateSourceLive(psState, psInst, uPredIndex))
	{
		/*
			Insert a TEST instruction immediately before this instruction
			to set the new predicate from the temporary register used
			for the spill.
		*/
		RestorePredicate(psState, 
						 psCodeBlock, 
						 psInst, 
						 uSpillPredNum, 
						 psSpillPred, 
						 bSpillPredNeg);
	}
}

static SPILL_TEMP_LAYOUT GetTESTPREDDestLayout(PINTERMEDIATE_STATE	psState, 
											   PINST				psDefInst,
											   IMG_PBOOL			pbUseTestMask)
/*****************************************************************************
 FUNCTION	: GetTESTPREDDestLayout

 PURPOSE	: Get the preferred layout of an intermediate register used to save
			  the result of a TESTPRED instruction.

 PARAMETERS	: psState			- Compiler state.
			  psDefInst			- Instruction to check.
			  pbUseTestMask		- If non-NULL returns whether the instruction
								can be modified to write directly into an
								intermediate register.

 RETURNS	: The layout.
*****************************************************************************/
{
	USEASM_TEST_CHANSEL	eChanSel;

	ASSERT(psDefInst->eOpcode == ITESTPRED);

	/*
		Don't switch to TESTMASK when the TEST instruction is already writing
		a unified store register.
	*/
	if (psDefInst->uDestCount != TEST_PREDICATE_ONLY_DEST_COUNT)
	{
		if (pbUseTestMask != NULL)
		{
			*pbUseTestMask = IMG_FALSE;
		}

		return SPILL_TEMP_LAYOUT_BIT0_SET;
	}

	if (pbUseTestMask != NULL)
	{
		*pbUseTestMask = IMG_TRUE;
	}

	eChanSel = psDefInst->u.psTest->sTest.eChanSel;
	switch (eChanSel)
	{
		case USEASM_TEST_CHANSEL_C0: 
		{
			return SPILL_TEMP_LAYOUT_BIT0_SET;
		}
		case USEASM_TEST_CHANSEL_C1: 
		{
			return SPILL_TEMP_LAYOUT_BIT8_SET;
		}
		case USEASM_TEST_CHANSEL_C2: 
		{
			return SPILL_TEMP_LAYOUT_BIT16_SET;
		}
		case USEASM_TEST_CHANSEL_C3: 
		{
			return SPILL_TEMP_LAYOUT_BIT24_SET;
		}
		case USEASM_TEST_CHANSEL_ANDALL:
		{
			return SPILL_TEMP_LAYOUT_ALLBITS_SET;
		}
		case USEASM_TEST_CHANSEL_ORALL:
		{
			return SPILL_TEMP_LAYOUT_ANYBITS_SET;
		}
		default: imgabort();
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_VOID RealignVTESTSources(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: RealignVTESTSources

 PURPOSE	: Update the sources and swizzles used by a VTEST instruction so the
			  hardware register alignment of the first channel in the first source
			  is even.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uFirstSrcBase;
	PARG		psFirstSrc;

	ASSERT(psInst->eOpcode == IVTEST);

	uFirstSrcBase = 0 * SOURCE_ARGUMENTS_PER_VECTOR + 1;
	psFirstSrc = &psInst->asArg[uFirstSrcBase];
	if (GetSourceArgAlignment(psState, psFirstSrc) == HWREG_ALIGNMENT_ODD)
	{
		IMG_UINT32	uChanSel;
		IMG_INT32	iShift;
		IMG_UINT32	uNewChanSel;
		IMG_UINT32	uSrc;

		ASSERT(psInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_XCHAN);
		ASSERT(psInst->u.psVec->auSwizzle[1] == USEASM_SWIZZLE(X, X, X, X));
		
		switch (psInst->u.psVec->auSwizzle[0])
		{
			case USEASM_SWIZZLE(X, X, X, X): uChanSel = USC_X_CHAN; break;
			case USEASM_SWIZZLE(Y, Y, Y, Y): uChanSel = USC_Y_CHAN; break;
			case USEASM_SWIZZLE(Z, Z, Z, Z): uChanSel = USC_Z_CHAN; break;
			case USEASM_SWIZZLE(W, W, W, W): uChanSel = USC_W_CHAN; break;
			default: imgabort();
		}

		if (psInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F32)
		{
			switch (uChanSel)
			{
				case USC_X_CHAN: uNewChanSel = USC_Y_CHAN; iShift = 1; break;
				case USC_Y_CHAN: uNewChanSel = USC_X_CHAN; iShift = -1; break;
				case USC_Z_CHAN: uNewChanSel = USC_Y_CHAN; iShift = -1; break;
				case USC_W_CHAN: uNewChanSel = USC_Z_CHAN; iShift = -1; break;
				default: imgabort();
			}
		}
		else
		{
			ASSERT(psInst->u.psVec->aeSrcFmt[0] == UF_REGFORMAT_F16);
			switch (uChanSel)
			{
				case USC_X_CHAN: uNewChanSel = USC_Z_CHAN; iShift = 1; break;
				case USC_Y_CHAN: uNewChanSel = USC_W_CHAN; iShift = 1; break;
				case USC_Z_CHAN: uNewChanSel = USC_X_CHAN; iShift = -1; break;
				case USC_W_CHAN: uNewChanSel = USC_Y_CHAN; iShift = -1; break;
				default: imgabort();
			}
		}

		if (iShift == 1)
		{
			for (uSrc = VECTOR_LENGTH - 1; uSrc >= 1; uSrc--)
			{
				MoveSrc(psState, psInst, uFirstSrcBase + uSrc, psInst, uFirstSrcBase + uSrc - 1);
			}
			SetPrecedingSource(psState, psInst, uFirstSrcBase + 0, &psInst->asArg[uFirstSrcBase + 1]); 
		}
		else
		{
			ASSERT(iShift == -1);
			for (uSrc = 0; uSrc < (VECTOR_LENGTH - 1); uSrc++)
			{
				MoveSrc(psState, psInst, uFirstSrcBase + 0 + uSrc, psInst, uFirstSrcBase + 1 + uSrc);
			}
			SetSrcUnused(psState, psInst, uFirstSrcBase + VECTOR_LENGTH - 1);
		}

		psInst->u.psVec->auSwizzle[0] = g_auReplicateSwizzles[uNewChanSel];
	}
}

static SPILL_TEMP_LAYOUT GetVTESTDestLayout(PINTERMEDIATE_STATE	psState, 
											PINST				psDefInst,
											IMG_PBOOL			pbUseTestMask)
/*****************************************************************************
 FUNCTION	: GetVTESTDestLayout

 PURPOSE	: Get the preferred layout of an intermediate register used to save
			  the result of a VTEST instruction.

 PARAMETERS	: psState			- Compiler state.
			  psDefInst			- Instruction to check.
			  pbUseTestMask		- If non-NULL returns whether the instruction
								can be modified to write directly into an
								intermediate register.

 RETURNS	: The layout.
*****************************************************************************/
{
	PVTEST_PARAMS	psTest = &psDefInst->u.psVec->sTest;
	VTEST_CHANSEL	eChanSel = psTest->eChanSel;

	RealignVTESTSources(psState, psDefInst);

	if (eChanSel == VTEST_CHANSEL_PERCHAN || psDefInst->uDestCount != VTEST_PREDICATE_ONLY_DEST_COUNT)
	{
		if (pbUseTestMask != NULL)
		{
			*pbUseTestMask = IMG_FALSE;
		}

		return SPILL_TEMP_LAYOUT_BIT0_SET;
	}
	else
	{
		if (pbUseTestMask != NULL)
		{
			*pbUseTestMask = IMG_TRUE;
		}

		if (eChanSel == VTEST_CHANSEL_ANDALL)
		{
			return SPILL_TEMP_LAYOUT_ALLBITS_SET;
		}
		else if (eChanSel == VTEST_CHANSEL_ORALL)
		{
			return SPILL_TEMP_LAYOUT_ANYBITS_SET;
		}
		else
		{
			ASSERT(eChanSel == VTEST_CHANSEL_XCHAN);

			ASSERT(psTest->eTestOpcode < IOPCODE_MAX);
			if ((g_psInstDesc[psTest->eTestOpcode].uFlags2 & DESC_FLAGS2_VECTORREPLICATEDRESULT) != 0)
			{
				return SPILL_TEMP_LAYOUT_ANY;
			}
			else
			{
				USEASM_SWIZZLE_SEL	eChanSel;
				IMG_UINT32			uChanSel;

				eChanSel = GetChan(psDefInst->u.psVec->auSwizzle[0], USC_X_CHAN);
				ASSERT(!g_asSwizzleSel[eChanSel].bIsConstant);
				uChanSel = g_asSwizzleSel[eChanSel].uSrcChan;

				switch (uChanSel)
				{
					case USC_X_CHAN: return SPILL_TEMP_LAYOUT_BIT0_SET;
					case USC_Y_CHAN: return SPILL_TEMP_LAYOUT_BIT8_SET;
					case USC_Z_CHAN: return SPILL_TEMP_LAYOUT_BIT16_SET;
					case USC_W_CHAN: return SPILL_TEMP_LAYOUT_BIT24_SET;
					default: imgabort();
				}
			}
		}
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static SPILL_TEMP_LAYOUT GetSpillNodeLayout(PINTERMEDIATE_STATE psState, 
											PINST				psDefInst, 
											IMG_PBOOL			pbUseTestMask)
/*****************************************************************************
 FUNCTION	: GetSpillNodeLayout

 PURPOSE	: Get the preferred layout of an intermediate register used to save
			  the result of a test instruction.

 PARAMETERS	: psState			- Compiler state.
			  psDefInst			- Instruction to check.
			  pbUseTestMask		- If non-NULL returns whether the instruction
								can be modified to write directly into an
								intermediate register.

 RETURNS	: The layout.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psDefInst->eOpcode == IVTEST)
	{
		return GetVTESTDestLayout(psState, psDefInst, pbUseTestMask);
	}
	else if (psDefInst->eOpcode == IFPTESTPRED_VEC)
	{
		return GetTESTPREDDestLayout(psState, psDefInst, pbUseTestMask);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	#if defined(EXECPRED)
	if (psDefInst->eOpcode == ICNDLT)
	{
		*pbUseTestMask = IMG_FALSE;
		return SPILL_TEMP_LAYOUT_BIT0_SET;
	}
	else
	#endif /* defined(EXECPRED) */
	{
		ASSERT(psDefInst->eOpcode == ITESTPRED);

		return GetTESTPREDDestLayout(psState, psDefInst, pbUseTestMask);
	}
}

static IMG_VOID ModifyToTESTMASK(PINTERMEDIATE_STATE	psState, 
								 PINST					psInst, 
								 IMG_UINT32				uDestIdx, 
								 IMG_UINT32				uDestTempNum,
								 SPILL_TEMP_LAYOUT		eLayout)
/*****************************************************************************
 FUNCTION	: ModifyToTESTMASK

 PURPOSE	: Convert an instruction from writing a predicate with a test result
			  to writing a temporary register.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to modify.
			  uDestIdx			- Index of the destination which is the predicate
								to save.
			  uDestTempNum		- Intermediate temporary register to write.
			  eLayout			- Format of the data written to the temporary register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uChansUsed;
	IMG_UINT32	uNewDestCount;
	IMG_UINT32	uOtherDestIdx;

	/*	
		Set the mask of channels used from the TESTMASK result.
	*/
	switch (eLayout)
	{
		case SPILL_TEMP_LAYOUT_BIT0_SET: 
		{
			uChansUsed = USC_X_CHAN_MASK;
			break;
		}
		case SPILL_TEMP_LAYOUT_BIT8_SET: 
		{
			uChansUsed = USC_Y_CHAN_MASK; 
			break;
		}
		case SPILL_TEMP_LAYOUT_BIT16_SET: 
		{
			uChansUsed = USC_Z_CHAN_MASK; 
			break;
		}
		case SPILL_TEMP_LAYOUT_BIT24_SET: 
		{
			uChansUsed = USC_W_CHAN_MASK; 
			break;
		}
		case SPILL_TEMP_LAYOUT_ALLBITS_SET:
		case SPILL_TEMP_LAYOUT_ANYBITS_SET:
		{
			uChansUsed = USC_ALL_CHAN_MASK;
			break;
		}
		default:
		{
			imgabort();
		}
	}

	/*
		Switch to a TESTMASK instruction.
	*/
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psInst->eOpcode == IVTEST)
	{
		/*
			VTESTBYTEMASK only supports an XYZW swizzle on the first source and either an
			XYZW swizzle or an XXXX swizzle on the second source so update the instruction
			swizzles to match.
		*/
		if (IsVTESTExtraSwizzles(psState, psInst->eOpcode, psInst))
		{
			USEASM_SWIZZLE_SEL	eOldSrcSel;
			IMG_UINT32			uOldSrcChan;

			ASSERT(psInst->u.psVec->auSwizzle[1] == psInst->u.psVec->auSwizzle[0] ||
				   psInst->u.psVec->auSwizzle[1] == USEASM_SWIZZLE(X, X, X, X));

			/*
				Check the channels used to calculate the predicate result are the same as the
				channels used to calculate the part of the bytemask which will be used later.
			*/
			eOldSrcSel = GetChan(psInst->u.psVec->auSwizzle[0], USC_X_CHAN);
			ASSERT(!g_asSwizzleSel[eOldSrcSel].bIsConstant);
			uOldSrcChan = g_asSwizzleSel[eOldSrcSel].uSrcChan;
			ASSERT(uOldSrcChan == (IMG_UINT32)g_aiSingleComponent[uChansUsed]);

			psInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			if (psInst->u.psVec->auSwizzle[1] != USEASM_SWIZZLE(X, X, X, X))
			{
				psInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);
			}
		}
		else
		{
			ASSERT(psInst->u.psVec->auSwizzle[0] == USEASM_SWIZZLE(X, Y, Z, W));
			ASSERT(psInst->u.psVec->auSwizzle[1] == USEASM_SWIZZLE(X, Y, Z, W) ||
				   psInst->u.psVec->auSwizzle[1] == USEASM_SWIZZLE(X, X, X, X));
		}

		ModifyOpcode(psState, psInst, IVTESTBYTEMASK);
		psInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_BYTE;
		uNewDestCount = 2;

		/*
			Update the restrictions on the register numbers assigned to the sources.
		*/
		MakeGroupsForInstSources(psState, psInst);
	}
	else 
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psInst->eOpcode == IFPTESTPRED_VEC)
		{
			ModifyOpcode(psState, psInst, IFPTESTMASK_VEC);
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			ASSERT(psInst->eOpcode == ITESTPRED);
			ModifyOpcode(psState, psInst, ITESTMASK);
		}
	
		/*
			Set the spill parameters.
		*/
		if (
				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				psInst->u.psTest->eAluOpcode == IFPADD8_VEC || 
				psInst->u.psTest->eAluOpcode == IFPSUB8_VEC ||
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				psInst->u.psTest->eAluOpcode == IFPADD8 || 
				psInst->u.psTest->eAluOpcode == IFPSUB8
			)
		{
			psInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_BYTE;
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		else if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			psInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_PREC;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		else
		{
			psInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_DWORD;
		}

		uNewDestCount = 1;
	}

	/*
		Make the destination a temporary register.
	*/
	SetDestCount(psState, psInst, uNewDestCount);
	SetDest(psState, psInst, uDestIdx, USEASM_REGTYPE_TEMP, uDestTempNum, UF_REGFORMAT_UNTYPED);
	psInst->auLiveChansInDest[uDestIdx] = uChansUsed;

	/*	
		Set the other destinations to unusued temporary registers.
	*/
	for (uOtherDestIdx = 0; uOtherDestIdx < uNewDestCount; uOtherDestIdx++)
	{
		if (uOtherDestIdx != uDestIdx)
		{
			SetDest(psState, psInst, uOtherDestIdx, USEASM_REGTYPE_TEMP, GetNextRegister(psState), UF_REGFORMAT_UNTYPED);
			psInst->auLiveChansInDest[uOtherDestIdx] = 0;
			psInst->auDestMask[uOtherDestIdx] = USC_ALL_CHAN_MASK;
		}
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Update restrictions on the alignment of the hardware register numbers of the sources/destination.
	*/
	if (psInst->eOpcode == IVTESTBYTEMASK)
	{
		SetupRegisterGroupsInst(psState, NULL /* psGroupsContext */, psInst);
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
}

static IMG_BOOL ConvertTESTPREDToTESTMASK(PINTERMEDIATE_STATE	psState, 
										  PINST					psInst,
										  IMG_UINT32			uDestIdx,
										  PSPILL_PRED			psSpillPred)
/*****************************************************************************
 FUNCTION	: ConvertTESTPREDToTESTMASK

 PURPOSE	: Try to switch an instruction writing a predicate to try directly
			  into an intermediate register instead.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to save the result of.
			  uDestIdx			- Index of the destination which is the predicate
								to save.
			  psSpillPred		- Data about the spilled predicate.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SPILL_TEMP_LAYOUT	eLayoutHere;
	IMG_BOOL			bConvertPredicateToBitwise;
	IMG_BOOL			bUseTestMask;
	IMG_BOOL			bOldDest;
	PINST				psDefInst;
	IMG_UINT32			uTestMaskDest;

	/*
		Check whether this instruction can be converted to TESTMASK and get the mapping
		between bits set in the TESTMASK destination and the truth value of the test result.
	*/
	eLayoutHere = GetSpillNodeLayout(psState, psInst, &bUseTestMask);
	if (!bUseTestMask)
	{
		return IMG_FALSE;
	}

	if (eLayoutHere == SPILL_TEMP_LAYOUT_ANY)
	{
		eLayoutHere = psSpillPred->eSpillTempLayout;
	}

	/*
		If the test instruction is predicated by the same predicate it defines then check for cases
		where the combination of the old and new test values can be generated by a bitwise operation
		on the two temporary registers they have been saved into. 

		This is the same number of instructions as restoring the old result into a predicate register
		and using it to predicate this instruction but reduces the pressure on the predicate registers at
		this point.
	*/
	bConvertPredicateToBitwise = IMG_FALSE;
	if (psInst->uPredCount > 1)
	{
		return IMG_FALSE;
	}
	else if (psInst->uPredCount == 1)
	{
		PARG psPredSrc = psInst->apsPredSrc[0];
		ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);
		if (psPredSrc->uNumber == psSpillPred->uNode)
		{
			IMG_BOOL	bSimpleCombine;
			
			bSimpleCombine = IMG_TRUE;
			/*
				any_bits_set(A) && any_bits_set(A) != any_bits_set(A & B)
			*/
			if (psSpillPred->eSpillTempLayout == SPILL_TEMP_LAYOUT_ANYBITS_SET && !GetBit(psInst->auFlag, INST_PRED_NEG))
			{
				bSimpleCombine = IMG_FALSE;
			}
			/*
				all_bits_set(A) || all_bits_set(B) != all_bits_set(A | B)
			*/
			if (psSpillPred->eSpillTempLayout == SPILL_TEMP_LAYOUT_ALLBITS_SET && GetBit(psInst->auFlag, INST_PRED_NEG))
			{
				bSimpleCombine = IMG_FALSE;
			}

			if (bSimpleCombine)
			{
				ClearPredicates(psState, psInst);
				bConvertPredicateToBitwise = IMG_TRUE;
			}
		}
	}

	/*
		Is the predicate destination completely defined here?
	*/
	bOldDest = IMG_FALSE;
	if (psInst->apsOldDest[0] != NULL)
	{
		bOldDest = IMG_TRUE;
		SetPartiallyWrittenDest(psState, psInst, 0 /* uDestIdx */, NULL /* psPartialDest */);
	}

	if (bConvertPredicateToBitwise || eLayoutHere != psSpillPred->eSpillTempLayout)
	{
		uTestMaskDest = GetNextRegister(psState);
	}
	else
	{
		uTestMaskDest = psSpillPred->uSpillTemp;
	}

	/*
		Convert the instruction defining the predicate to define a temporary register with a
		specific bitpattern depending on the test result.
	*/
	ModifyToTESTMASK(psState, psInst, uDestIdx, uTestMaskDest, eLayoutHere);

	/*
		If the data generated by the TESTMASK instruction here differs from that generated at the
		other instructions where it is defined then insert an instruction to convert it.
	*/
	if (eLayoutHere != psSpillPred->eSpillTempLayout)
	{
		IMG_UINT32	uReformatDest;
		PINST		psReformatInst;

		if (bConvertPredicateToBitwise)
		{
			uReformatDest = GetNextRegister(psState);
		}
		else
		{
			uReformatDest = psSpillPred->uSpillTemp;
		}

		psReformatInst = 
			BaseRestorePredicate(psState,
								 psInst->psBlock,
								 psInst->psNext,
								 IMG_FALSE /* bTestPred */,
								 uReformatDest,
								 uTestMaskDest,
								 eLayoutHere,
								 IMG_FALSE /* bNegate */);

		if (bConvertPredicateToBitwise)
		{
			ClearPredicates(psState, psInst);
		}
		else
		{
			CopyPredicate(psState, psReformatInst, psInst);
		}

		psDefInst = psReformatInst;
	}
	else
	{
		psDefInst = psInst;
	}

	if (bConvertPredicateToBitwise)
	{
		PINST		psCombInst;

		/*
			If the instruction is predicated by the same predicate
			as the destination then insert an extra instruction
			to combine the result here with the existing result.
		*/
		psCombInst = AllocateInst(psState, psInst);
		if (GetBit(psInst->auFlag, INST_PRED_NEG))
		{
			SetOpcode(psState, psCombInst, IOR);
		}
		else
		{
			SetOpcode(psState, psCombInst, IAND);
		}
		SetDest(psState, 
				psCombInst, 
				0 /* uDestIdx */, 
				USEASM_REGTYPE_TEMP, 
				psSpillPred->uSpillTemp, 
				UF_REGFORMAT_UNTYPED);
		SetSrc(psState, 
			   psCombInst, 
			   0 /* uSrcIdx */, 
			   USEASM_REGTYPE_TEMP, 
			   psSpillPred->uSpillTemp,
			   UF_REGFORMAT_UNTYPED);
		SetSrcFromArg(psState, psCombInst, 1 /* uSrcIdx */, &psDefInst->asDest[0]);

		InsertInstBefore(psState, psDefInst->psBlock, psCombInst, psDefInst->psNext);
	}
	else
	{
		/*
			Update the conditionally overwritten destination to the replacement temporary register.
		*/
		if (bOldDest)
		{
			SetPartialDest(psState, 
						   psDefInst, 
						   uDestIdx, 
						   USEASM_REGTYPE_TEMP, 
						   psSpillPred->uSpillTemp, 
						   UF_REGFORMAT_UNTYPED);
		}
	}

	return IMG_TRUE;
}

static IMG_VOID SaveInstResult(PINTERMEDIATE_STATE		psState,
							   PPREGALLOCSPILL_STATE	psPredSpillState,
							   PCODEBLOCK				psCodeBlock,
							   PINST					psInst,
							   PSPILL_PRED				psSpillPred,
							   IMG_UINT32				uPredRegIndex,
							   PINST_PRED_SPILL			psInstSpill)
/*****************************************************************************
 FUNCTION	: SaveInstResult

 PURPOSE	: Save the result of an instruction which writes a predicate into
		      a temporary register.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block which contains the instruction.
			  psInst			- Instruction to save the result of.
			  psSpillPred		- Data about the spilled predicate.
			  uPredRegIndex		- The index of the register within the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psSaveInst;

	ASSERT((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_TEST) != 0);
	ASSERT(psInst->asDest[uPredRegIndex].uType == USEASM_REGTYPE_PREDICATE);
	ASSERT(psInst->asDest[uPredRegIndex].uNumber == psSpillPred->uNode);

	/*
		Try to convert the defining instruction to write directly into the
		intermediate register.
	*/
	if (!ConvertTESTPREDToTESTMASK(psState, psInst, uPredRegIndex, psSpillPred))
	{
		IMG_UINT32	uNewPredDest;
		PARG		psPredSrc;
		IMG_BOOL	bRestoreBeforeInst;

		/*
			Get a fresh predicate register for the result of the instruction.
		*/
		uNewPredDest = GetSpillPredicate(psState, psPredSpillState);

		bRestoreBeforeInst = IMG_FALSE;

		/*
			If the predicate is written conditionally then restore the previous value before
			the write.
		*/
		if (psInst->apsOldDest[uPredRegIndex] != NULL)
		{
			ASSERT(EqualArgs(&psInst->asDest[uPredRegIndex], psInst->apsOldDest[uPredRegIndex]));
			bRestoreBeforeInst = IMG_TRUE;

			MakePredicatePartialDest(psState, psInst, uPredRegIndex, uNewPredDest);
		}

		/*
			If the instruction is predicated by the same predicate it also defines then restore
			the previous value of the predicate before the instruction.
		*/
		if (psInst->uPredCount == 1)
		{
			psPredSrc = psInst->apsPredSrc[0];
			ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);
			if (psPredSrc->uNumber == psSpillPred->uNode)
			{
				bRestoreBeforeInst = IMG_TRUE;

				SetPredicate(psState, psInst, uNewPredDest, GetBit(psInst->auFlag, INST_PRED_NEG));

				ASSERT(psInstSpill->apsPred[0] == psSpillPred);
				psInstSpill->apsPred[0] = NULL;
			}
		}

		/*
			Insert instructions before the current instruction to set the fresh predicate register
			from the spilled value.
		*/
		if (bRestoreBeforeInst)
		{
			RestorePredicate(psState, 
							 psCodeBlock, 
							 psInst, 
							 uNewPredDest, 
							 psSpillPred, 
							 IMG_FALSE /* bNegate */);
		}

		/*
			Update the destination.
		*/
		MakePredicateDest(psState, psInst, uPredRegIndex, uNewPredDest);

		/*
			RLP	rSPILL+pNODE, #0, pNODE

			rSPILL[0] = pNODE ? 1 : 0
			rSPILL[1..31] = 0
			pNODE = 0
		*/
		psSaveInst = AllocateInst(psState, psInst);

		SetOpcodeAndDestCount(psState, psSaveInst, IRLP, RLP_DEST_COUNT);

		SetDest(psState, psSaveInst, RLP_UNIFIEDSTORE_DESTIDX, USEASM_REGTYPE_TEMP, psSpillPred->uSpillTemp, UF_REGFORMAT_F32);

		SetDest(psState, psSaveInst, RLP_PREDICATE_DESTIDX, USEASM_REGTYPE_PREDICATE, uNewPredDest, UF_REGFORMAT_F32);

		SetSrc(psState, psSaveInst, RLP_UNIFIEDSTORE_ARGIDX, USEASM_REGTYPE_IMMEDIATE, 0 /* uNumber */, UF_REGFORMAT_F32);

		SetSrc(psState, psSaveInst, RLP_PREDICATE_ARGIDX, USEASM_REGTYPE_PREDICATE, uNewPredDest, UF_REGFORMAT_F32);

		InsertInstBefore(psState, psCodeBlock, psSaveInst, psInst->psNext);

		/*
			If the instruction whose result we saving needs to be the only instruction in its
			basic block then add the new predicate to the set of registers live out of the 
			instruction's block.
		*/
		if ((g_psInstDesc[psInst->eOpcode].uFlags2 & DESC_FLAGS2_NONMERGABLE) != 0)
		{
			PCODEBLOCK	psInstBlock = psInst->psBlock;

			ASSERT(psInstBlock->eType == CBTYPE_UNCOND);
			ASSERT(psInstBlock->uNumSuccs == 1);
			ASSERT(psInstBlock->asSuccs[0].psDest == psInst->psBlock);

			ComputeRegistersLiveInSet(psState, psSaveInst->psBlock, psInstBlock);
		}
	}
}

static IMG_VOID GetBestSpillNodeLayout(PINTERMEDIATE_STATE	psState, 
									   PUSEDEF_CHAIN		psPredReg,
									   PSPILL_TEMP_LAYOUT	peLayout)
/*****************************************************************************
 FUNCTION	: GetBestSpillNodeLayout

 PURPOSE	: Get the preferred bit to save a predicate into to to minimize
			  the number of extra spilling instructions.

 PARAMETERS	: psState		- Compiler state.
			  psPredReg		- Predicate.
			  peLayout		- Returns the preferred layout of the save register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			auLayoutUses[SPILL_TEMP_LAYOUT_COUNT];
	SPILL_TEMP_LAYOUT	eMostUsedLayout;
	PUSC_LIST_ENTRY		psListEntry;

	memset(auLayoutUses, 0, sizeof(auLayoutUses));
	eMostUsedLayout = SPILL_TEMP_LAYOUT_ANY;

	/*
		Examine all instructions defining the predicate.
	*/
	for (psListEntry = psPredReg->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse->eType >= DEF_TYPE_FIRST && psUse->eType <= DEF_TYPE_LAST)
		{
			SPILL_TEMP_LAYOUT	eLayoutHere;

			ASSERT(psUse->eType == DEF_TYPE_INST);

			/*
				Get the layout of the save register which would minimize the number of extra
				instructions to save the result of this instruction.
			*/
			eLayoutHere = GetSpillNodeLayout(psState, psUse->u.psInst, NULL /* pbUseTestMask */);

			if (eLayoutHere == SPILL_TEMP_LAYOUT_ANY)
			{
				continue;
			}

			ASSERT(eLayoutHere < SPILL_TEMP_LAYOUT_COUNT);
			auLayoutUses[eLayoutHere]++;

			/*
				Choose the most frequently used layout.
			*/
			if (eMostUsedLayout == SPILL_TEMP_LAYOUT_ANY || auLayoutUses[eLayoutHere] > auLayoutUses[eMostUsedLayout])
			{
				eMostUsedLayout = eLayoutHere;
			}
		}
	}

	if (eMostUsedLayout == SPILL_TEMP_LAYOUT_ANY)
	{
		eMostUsedLayout = SPILL_TEMP_LAYOUT_BIT0_SET;
	}

	*peLayout = eMostUsedLayout;
}

static IMG_VOID RestoreForShaderResult(PINTERMEDIATE_STATE		psState,
									   PPREGALLOCSPILL_STATE	psPredSpillState,
									   PSPILL_PRED				psSpillPred,
									   PUSEDEF					psUse)
/*****************************************************************************
 FUNCTION	: RestoreForShaderResult

 PURPOSE	: Insert instructions to restore the value of a predicate register
			  which is a result of the shader.

 PARAMETERS	: psState			- Compiler state.
			  psPredSpillState	- Predicate spill state.
			  psSpillPred		- Predicate register to restore.
			  psUse				- Reference representing the use of the predicate
								in the shader epilog.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PCODEBLOCK		psRestoreBlock;
	ARG				sNewShaderOutput;

	if (psUse->eType == USE_TYPE_FIXEDREG)
	{
		PFIXED_REG_DATA	psFixedReg = psUse->u.psFixedReg;

		ASSERT(psUse->uLocation < psFixedReg->uConsecutiveRegsCount);
		ASSERT(psFixedReg->uVRegType == USEASM_REGTYPE_PREDICATE);
		ASSERT(psFixedReg->auVRegNum[psUse->uLocation] == psSpillPred->uNode);

		/*
			Where will the driver epilog code which uses this shader output be inserted?
		*/
		psRestoreBlock = GetShaderEndBlock(psState, psFixedReg, psUse->uLocation);
	}
	else
	{
		PINST	psUseInst;

		psUseInst = psUse->u.psInst;
		ASSERT(psUseInst->eOpcode == IFEEDBACKDRIVEREPILOG);

		/*
			Restore the predicate just before shader execution is suspended for feedback.
		*/
		psRestoreBlock = psState->psPreFeedbackBlock;
	}

	/*
		Allocate a new predicate register.
	*/
	if (psSpillPred->uNewShaderOutputRegNum == USC_UNDEF)
	{
		psSpillPred->uNewShaderOutputRegNum = GetSpillPredicate(psState, psPredSpillState);

		/*
			Append instructions to restore the value of the predicate register immediately before the driver
			epilog.
		*/
		RestorePredicate(psState, 
						 psRestoreBlock, 
						 IMG_NULL /* psInstBefore */, 
						 psSpillPred->uNewShaderOutputRegNum, 
						 psSpillPred, 
						 IMG_FALSE /* bNegate */);
		VectorSet(psState, &psRestoreBlock->sRegistersLiveOut.sPredicate, psSpillPred->uNewShaderOutputRegNum, 1);

		/*
			Update the predicate register which contains the shader output.
		*/
		ASSERT(psPredSpillState->uTexkillOutputNum == psSpillPred->uNode);
		psPredSpillState->uTexkillOutputNum = psSpillPred->uNewShaderOutputRegNum;
	}

	InitInstArg(&sNewShaderOutput);
	sNewShaderOutput.uType = USEASM_REGTYPE_PREDICATE;
	sNewShaderOutput.uNumber = psSpillPred->uNewShaderOutputRegNum;
	UseDefSubstUse(psState, psSpillPred->psPred->psUseDefChain, psUse, &sNewShaderOutput);
}

static IMG_VOID RestoreForConditional(PINTERMEDIATE_STATE	psState,
									  PPREGALLOCSPILL_STATE	psPredSpillState,
									  PSPILL_PRED			psSpillPred,
									  PCODEBLOCK			psBlock)
/*****************************************************************************
 FUNCTION	: RestoreForConditional

 PURPOSE	: Insert instructions to restore the value of a predicate register
			  which is used to choose the successor of a conditional block.

 PARAMETERS	: psState			- Compiler state.
			  psPredSpillState	- Predicate spill state.
			  psSpillPred		- Predicate register to restore.
			  psBlock			- Conditional block.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uNew;

	ASSERT(psBlock->eType == CBTYPE_COND);
	ASSERT(psBlock->u.sCond.sPredSrc.uType == USEASM_REGTYPE_PREDICATE);
	ASSERT(psBlock->u.sCond.sPredSrc.uNumber == psSpillPred->uNode);

	/*
		Allocate a new predicate register.
	*/
	uNew = GetSpillPredicate(psState, psPredSpillState);
		
	/*	
		Replace the spilled predicate register by the newly allocated one.
	*/
	SetConditionalBlockPredicate(psState, psBlock, uNew);

	/*
		Append instructions to the block to restore the value of the spilled
		predicate register into the newly allocated one.
	*/
	RestorePredicate(psState, psBlock, IMG_NULL, uNew, psSpillPred, IMG_FALSE);

	/*
		Mark the newly allocated predicate register as live out of the block.
	*/
	VectorSet(psState, &psBlock->sRegistersLiveOut.sPredicate, uNew, 1);
}

static IMG_VOID UpdateInstPredSpill(PINTERMEDIATE_STATE	psState,
									PINST_PRED_SPILL	psInstSpill,
									PSPILL_PRED			psGroupPred,
									PUSEDEF				psGroupUse)
/*****************************************************************************
 FUNCTION	: UpdateInstPredSpill

 PURPOSE	: 

 PARAMETERS	: psState			- Compiler state.
			  psInstSpill		- All references to spilled predicate registers in
								the instruction.
			  psGroupPred		- Predicate register.
			  psGroupUse		- Use of the predicate register in the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	ASSERT(psInstSpill->psInst == NULL || psInstSpill->psInst == psGroupUse->u.psInst);
	psInstSpill->psInst = psInst = psGroupUse->u.psInst;

	switch (psGroupUse->eType)
	{
		case DEF_TYPE_INST:
		{
			ASSERT(psGroupUse->uLocation < VECTOR_LENGTH);
			ASSERT(psInstSpill->apsDest[psGroupUse->uLocation] == NULL);
			psInstSpill->apsDest[psGroupUse->uLocation] = psGroupPred;
			break;
		}
		case USE_TYPE_PREDICATE:
		{
			ASSERT(psGroupUse->uLocation < VECTOR_LENGTH);
			ASSERT(psInstSpill->apsPred[psGroupUse->uLocation] == NULL);
			psInstSpill->apsPred[psGroupUse->uLocation] = psGroupPred;
			break;
		}
		case USE_TYPE_OLDDEST:
		{
			ASSERT(psGroupUse->uLocation < VECTOR_LENGTH);
			ASSERT(EqualArgs(&psInst->asDest[psGroupUse->uLocation], psInst->apsOldDest[psGroupUse->uLocation]));
			psInstSpill->abOldDest[psGroupUse->uLocation] = IMG_TRUE;
			break;
		}
		default: imgabort();
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_VOID SaveVectorTestResult(PINTERMEDIATE_STATE	psState,
									 PPREGALLOCSPILL_STATE	psPredSpillState,
									 PINST					psInst,
									 PINST_PRED_SPILL		psInstSpill)
/*****************************************************************************
 FUNCTION	: SaveVectorTestResult

 PURPOSE	: Replace a test instruction setting a vector of predicates by a
			  group of instructions where each instruction only sets one
			  predicate.

 PARAMETERS	: psState			- Compiler state.
			  psPredSpillState	- Predicate spill state.
			  psInst			- Instruction to modify.
			  psInstSpill		- Instruction source/destination predicates
								which need to be spilled.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ARG			sTMDest, sDummyDest;
	PINST		psInsertBeforeInst;
	IMG_UINT32	uDest;

	/*
		Allocate a new temporary register to hold the TESTMASK result.
	*/
	MakeNewTempArg(psState, UF_REGFORMAT_F32, &sTMDest);
	
	psInsertBeforeInst = psInst->psNext;
	for (uDest = 0; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; uDest++)
	{
		PSPILL_PRED	psDestSpill = psInstSpill->apsDest[uDest];
		PARG		psPredSrc = psInst->apsPredSrc[uDest];
		PARG		psDest = &psInst->asDest[uDest];
		PSPILL_PRED	psSrcSpill = psInstSpill->apsPred[uDest];
		PINST		psRestoreInst;

		/*
			Skip predicate results which are never used.
		*/
		if (psInst->auLiveChansInDest[uDest] == 0)
		{
			continue;
		}
		
		/*
			Insert an instruction to set the predicate destination from the 
			corresponding byte in the TESTMASK result.
		*/
		psRestoreInst = BaseRestorePredicate(psState,
											 psInst->psBlock,
											 psInsertBeforeInst,
											 IMG_TRUE /* bTestPred */,
											 psDest->uNumber,
											 sTMDest.uNumber,
											 SPILL_TEMP_LAYOUT_BIT0_SET + uDest,
											 IMG_FALSE /* bNegate */);
				
		/*
			Copy the source predicate for the destination to the new instruction.
		*/
		SetPredicate(psState, psRestoreInst, psPredSrc->uNumber, psInstSpill->bSrcPredNeg);
		SetPartiallyWrittenDest(psState, psRestoreInst, 0 /* uDest */, psInst->apsOldDest[uDest]);

		psInsertBeforeInst = psRestoreInst->psNext;

		/*
			If required spill the source/destination predicate for the new instruction.
		*/
		if (psDestSpill != NULL)
		{
			SaveInstResult(psState,
						   psPredSpillState,
						   psRestoreInst->psBlock,
						   psRestoreInst,
						   psDestSpill,
						   0 /* uPredRegIndex */,
						   psInstSpill);
		}
		else if (psSrcSpill != NULL)
		{
			RestorePredicateForInst(psState,
									psPredSpillState,
									psRestoreInst->psBlock,
									psRestoreInst,
									psSrcSpill,
									IMG_FALSE /* bPredIsSrc */,
									0 /* uPredIndex */,
									psInstSpill->bSrcPredNeg);
		}
	}
	
	ModifyOpcode(psState, psInst, IVTESTBYTEMASK);
	SetDestCount(psState, psInst, 2 /* uNewDestCount */);
	psInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_BYTE;
	
	SetDestFromArg(psState, psInst, 0 /* uDest */, &sTMDest);
	SetPartiallyWrittenDest(psState, psInst, 0 /* uDest */, NULL);
		
	/*
		Set the second destination to a dummy register which is never used.
	*/
	MakeNewTempArg(psState, UF_REGFORMAT_F32, &sDummyDest);
	SetDestFromArg(psState, psInst, 1 /* uDest */, &sDummyDest);
	psInst->auLiveChansInDest[1] = 0;
	SetPartiallyWrittenDest(psState, psInst, 1 /* uDest */, NULL);

	/*
		Clear the source predicates on the original instruction.
	*/
	ClearPredicates(psState, psInst);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static IMG_VOID SpillPredRefGroup(PINTERMEDIATE_STATE		psState, 
								  PPREGALLOCSPILL_STATE		psPredSpillState,
								  PSPILL_DATA				psSpillData,
								  PUSEDEF_SETUSES_ITERATOR	psIter)
/*****************************************************************************
 FUNCTION	: SpillPredRefGroup

 PURPOSE	: Update an object which contains references to predicate registers
			  to be spilled.

 PARAMETERS	: psState			- Compiler state.
			  psPredSpillState	- Predicate spill state.
			  psSpillData		- Group of predicate registers to spill.
			  psIter			- Iterator pointing at the object to update.

 RETURNS	: Nothing.
*****************************************************************************/
{
	INST_PRED_SPILL		sInstSpill;
	IMG_UINT32			uGroup;

	memset(&sInstSpill, 0, sizeof(sInstSpill));
	for (uGroup = 0; uGroup < psSpillData->uCount; uGroup++)
	{
		PSPILL_PRED	psGroupPred = &psSpillData->asPred[uGroup];

		while (UseDefSetUsesIterator_Member_Continue(psIter, uGroup))
		{
			PUSEDEF	psGroupUse;

			psGroupUse = UseDefSetUsesIterator_Member_CurrentUse(psIter, uGroup);
			switch (psGroupUse->eType)
			{
				case DEF_TYPE_INST:
				case USE_TYPE_PREDICATE:
				case USE_TYPE_OLDDEST:
				{
					/*
						For references in an instruction just record which instruction source/destinations
						need to be modified.
					*/
					UpdateInstPredSpill(psState, &sInstSpill, psGroupPred, psGroupUse);
					break;
				}

				case USE_TYPE_SRC:
				{
					PINST	psUseInst = psGroupUse->u.psInst;

					if (psUseInst->eOpcode == IFEEDBACKDRIVEREPILOG)
					{
						/*
							Restore the spilled predicate just before the shader epilog.
						*/
						RestoreForShaderResult(psState, psPredSpillState, psGroupPred, psGroupUse);
					}
					else
					{
						/*	
							Restore the spilled predicate before the instruction.
						*/
						RestorePredicateForInst(psState,
												psPredSpillState,
												psUseInst->psBlock,
												psUseInst,
												psGroupPred,
												IMG_TRUE /* bPredIsSrc */,
												psGroupUse->uLocation,
												IMG_FALSE /* bSrcPredNeg */);
					}
					break;
				}
					
				case USE_TYPE_FIXEDREG:
				{
					/*
						Restore the spilled predicate just before the shader epilog.
					*/
					RestoreForShaderResult(psState, psPredSpillState, psGroupPred, psGroupUse);
					break;
				}

				case USE_TYPE_COND:
				{
					/*
						Restore the spilled predicate just before the end of the conditional block.
					*/
					RestoreForConditional(psState, psPredSpillState, psGroupPred, psGroupUse->u.psBlock);
					break;
				}

				default: imgabort();
			}

			UseDefSetUsesIterator_Member_Next(psIter, uGroup);
		}
	}

	if (sInstSpill.psInst != NULL)
	{
		PINST		psInst = sInstSpill.psInst;
		IMG_UINT32	uDest;
		IMG_UINT32	uPred;
		IMG_UINT32	uPredDestCount;

		/*
			Save the negate flag on the instruction's predicate(s). If we need to restore any of the instruction
			predicate(s) then we'll clear the negate flag and restore the negated value of the predicate. This
			reduces the restrictions on the possible hardware register numbers for the predicate(s).
		*/
		sInstSpill.bSrcPredNeg = GetBit(psInst->auFlag, INST_PRED_NEG) ? IMG_TRUE : IMG_FALSE;
		
		/*
			Count the number of predicates written by the current instruction (whether spilled or not).
		*/
		uPredDestCount = 0;
		for (uDest = 0; uDest < psInst->uDestCount; uDest++)
		{
			if (psInst->asDest[uDest].uType == USEASM_REGTYPE_PREDICATE)
			{
				uPredDestCount++;
			}
		}
		ASSERT(uPredDestCount == 0 || uPredDestCount == 1 || uPredDestCount == VECTOR_LENGTH);

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			Check for a test instruction writing four predicates and with a seperate source
			predicate for each written predicate.
		*/
		if (uPredDestCount == VECTOR_LENGTH && GetBit(psInst->auFlag, INST_PRED_PERCHAN))
		{
			IMG_BOOL	bMismatchedPreds;
			IMG_BOOL	bRestoreBefore;
			
			ASSERT(psInst->eOpcode == IVTEST);
			ASSERT(psInst->uPredCount == VECTOR_LENGTH);
			ASSERT(psInst->uDestCount == VTEST_PREDICATE_ONLY_DEST_COUNT);

			/*
				Check if for any channel the source predicate is different to the destination predicate
				and if the destination predicate is initialized before the instruction.
			*/
			bMismatchedPreds = IMG_FALSE;
			bRestoreBefore = IMG_FALSE;
			for (uDest = 0; uDest < VTEST_PREDICATE_ONLY_DEST_COUNT; uDest++)
			{
				PARG	psPredSrc = psInst->apsPredSrc[uDest];
				PARG	psDest = &psInst->asDest[uDest];

				ASSERT(psDest->uType == USEASM_REGTYPE_PREDICATE);
				ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);
				if (!EqualArgs(psDest, psPredSrc))
				{
					bMismatchedPreds = IMG_TRUE;
				}
				if (psInst->apsOldDest[uDest] != NULL)
				{
					bRestoreBefore = IMG_TRUE;
				}
			}

			/*
				This instruction needs 8 predicates as inputs so we need special measures
				to ensure spilling actually reduces predicate register pressure.
			*/
			if (bMismatchedPreds && bRestoreBefore)
			{
				if (bRestoreBefore)
				{
					/*
						Convert
							
							(pE, pF, pG, pH) VTEST	pA+pB+pC+D, ...
						->
							VTESTBYTEMASK	rX, ...
							(pE) TEST		pA = (rX & 0x000000FF) == 0x000000FF
							(pF) TEST		pB = (rX & 0x0000FF00) == 0x0000FF00
							(pG) TEST		pC = (rX & 0x00FF0000) == 0x00FF0000
							(pH) TEST		pD = (rX & 0xFF000000) == 0xFF000000

						Then spill the source/destinations of the new TEST instructions
						as required.
					*/
					SaveVectorTestResult(psState, psPredSpillState, psInst, &sInstSpill);
					return;
				}
				else
				{
					/*
						The destination predicates are uninitialized before the instruction so
						we can just clear the source predicates and written the destinations
						unconditionally.
					*/
					ClearPredicates(psState, psInst);
					for (uPred = 0; uPred < psInst->uPredCount; uPred++)
					{
						sInstSpill.apsPred[uPred] = NULL;
					}
				}
			}
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		for (uDest = 0; uDest < VECTOR_LENGTH; uDest++)
		{
			if (sInstSpill.apsDest[uDest] != NULL)
			{
				ASSERT(uDest < psInst->uDestCount);

				SaveInstResult(psState,
							   psPredSpillState,
							   psInst->psBlock,
							   psInst,
							   sInstSpill.apsDest[uDest],
							   uDest,
							   &sInstSpill);
			}
		}

		for (uPred = 0; uPred < psInst->uPredCount; uPred++)
		{
			if (sInstSpill.apsPred[uPred] != NULL)
			{
				RestorePredicateForInst(psState,
										psPredSpillState,
										psInst->psBlock,
										psInst,
										sInstSpill.apsPred[uPred],
										IMG_FALSE /* bPredIsSrc */,
										uPred,
										sInstSpill.bSrcPredNeg);
			}
		}
	}
}


static IMG_VOID SpillPredReg(PINTERMEDIATE_STATE	psState, 
							 PPREGALLOCSPILL_STATE	psPredSpillState,
							 PSPILL_DATA			psSpillData,
							 IMG_PBOOL				pbInsertedSpill)
/*****************************************************************************
 FUNCTION	: SpillPredReg

 PURPOSE	: Replace all references to a register by references to new registers
			  with shorter lifespans.

 PARAMETERS	: psState			- Compiler state.
			  psPredSpillState	- Predicate spill state.
			  psSpillData		- How to modify instructions to save the spilled
								register.
			  pbInsertedSpill	- Returns TRUE if at least one define or use by
								an instruction was modified.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN*				apsSpillChains;
	IMG_UINT32					uGroup;
	PUSEDEF_SETUSES_ITERATOR	psIter;

	*pbInsertedSpill = IMG_FALSE;

	apsSpillChains = UscAlloc(psState, sizeof(apsSpillChains[0]) * psSpillData->uCount);
	for (uGroup = 0; uGroup < psSpillData->uCount; uGroup++)
	{
		apsSpillChains[uGroup] = psSpillData->asPred[uGroup].psPred->psUseDefChain;
	}

	psIter = UseDefSetUsesIterator_Initialize(psState, psSpillData->uCount, apsSpillChains);

	UscFree(psState, apsSpillChains);
	apsSpillChains = NULL;

	/*
		Iterate over all objects (instructions, conditional blocks, the shader epilog) containing
		a reference to a predicate from the set.
	*/
	while (UseDefSetUsesIterator_Continue(psIter))
	{
		SpillPredRefGroup(psState, psPredSpillState, psSpillData, psIter);

		*pbInsertedSpill = IMG_TRUE;

		UseDefSetUsesIterator_Next(psIter);
	}
	UseDefSetUsesIterator_Finalise(psState, psIter);
}

static IMG_VOID AppendToSpillGroup(PINTERMEDIATE_STATE psState, PSPILL_DATA psSpill, PVREGISTER psPredToAppend)
/*****************************************************************************
 FUNCTION	: AppendToSpillGroup

 PURPOSE	: Add a predicate register to a set of predicate registers to spill.

 PARAMETERS	: psState			- Compiler state.
			  psSpill			- On output: Set of predicate registers to spill.
			  psPredToAppend	- Predicate register to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uMember;

	/*
		Check if the predicate register is already a member of the set.
	*/
	for (uMember = 0; uMember < psSpill->uCount; uMember++)
	{
		if (psSpill->asPred[uMember].psPred == psPredToAppend)
		{
			return;
		}
	}

	ResizeArray(psState,
				psSpill->asPred,
				(psSpill->uCount + 0) * sizeof(psSpill->asPred[0]),
				(psSpill->uCount + 1) * sizeof(psSpill->asPred[0]),
				(IMG_PVOID*)&psSpill->asPred);
	psSpill->asPred[psSpill->uCount].psPred = psPredToAppend;
	psSpill->asPred[psSpill->uCount].uNode = psPredToAppend->psUseDefChain->uNumber;
	psSpill->asPred[psSpill->uCount].uNewShaderOutputRegNum = USC_UNDEF;
	psSpill->uCount++;
}

static IMG_VOID FindSpillGroup(PINTERMEDIATE_STATE psState, PSPILL_DATA psSpill, PVREGISTER psPredReg)
/*****************************************************************************
 FUNCTION	: FindSpillGroup

 PURPOSE	: Find all the predicate registers which need to be spilled together.

 PARAMETERS	: psState			- Compiler state.
			  psSpill			- On output: Set of predicate registers to spill.
			  psPred			- A predicate register which couldn't be coloured.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	psSpill->uCount = 0;
	psSpill->asPred = NULL;

	AppendToSpillGroup(psState, psSpill, psPredReg);

	for (psListEntry = psPredReg->psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse->eType == USE_TYPE_PREDICATE)
		{
			PINST	psUseInst = psUse->u.psInst;

			if (GetBit(psUseInst->auFlag, INST_PRED_NEG) && GetBit(psUseInst->auFlag, INST_PRED_PERCHAN))
			{
				IMG_UINT32	uPred;

				for (uPred = 0; uPred < psUseInst->uPredCount; uPred++)
				{
					if (uPred != psUse->uLocation)
					{
						PARG	psPredSrc = psUseInst->apsPredSrc[uPred];

						ASSERT(psPredSrc->uType == USEASM_REGTYPE_PREDICATE);

						AppendToSpillGroup(psState, psSpill, psPredSrc->psRegister);
					}
				}
			}
		}
	}
}

static IMG_BOOL SpillPredicateRegister(PINTERMEDIATE_STATE		psState, 
									   PPREGALLOCSPILL_STATE	psPredSpillState,
									   IMG_UINT32				uNode)
/*****************************************************************************
 FUNCTION	: SpillPredicateRegister

 PURPOSE	: Reduce the register pressure by spilling a node.

 PARAMETERS	: psState			- Compiler state.
			  psPredSpillState	- Predicate spill state.
			  uNode				- Register to spill.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SPILL_DATA sSpillData;
	IMG_BOOL bInsertedSpill;
	IMG_UINT32 uGroup;

	/*
		Find other predicates which should be spilled simultaneously with this one 
	*/
	FindSpillGroup(psState, &sSpillData, GetVRegister(psState, USEASM_REGTYPE_PREDICATE, uNode));

	for (uGroup = 0; uGroup < sSpillData.uCount; uGroup++)
	{
		REPLACE_REGISTER_LIVEOUT_SETS	sReplaceContext;
		PSPILL_PRED						psSpillPred = &sSpillData.asPred[uGroup];

		/*
			Get the bits to save the predicate result into at each defining instruction to minimize
			the number of extra instructions.
		*/
		GetBestSpillNodeLayout(psState, psSpillPred->psPred->psUseDefChain, &psSpillPred->eSpillTempLayout);

		/*	
			Allocate a temporary register to hold the spilled contents of the predicate register.
		*/
		psSpillPred->uSpillTemp = GetNextRegister(psState);
		
		/*
			Replace references to the spilled register in all the stored sets of registers live out of flow control
			blocks by the temporary register it is spilled into.
		*/
		sReplaceContext.uRenameFromRegType = USEASM_REGTYPE_PREDICATE;
		sReplaceContext.uRenameFromRegNum = psSpillPred->uNode;
		sReplaceContext.uRenameToRegType = USEASM_REGTYPE_TEMP;
		sReplaceContext.uRenameToRegNum = psSpillPred->uSpillTemp;
		DoOnAllBasicBlocks(psState, 
						   ANY_ORDER,
						   ReplaceRegisterLiveOutSetsBP,
						   IMG_FALSE,
						   &sReplaceContext);
	}

	/*
		Replace references to the spilled register by references to new registers with very short lifetimes.
	*/
	SpillPredReg(psState, psPredSpillState, &sSpillData, &bInsertedSpill);

	/*
		Free the list of predicates to spill.
	*/
	UscFree(psState, sSpillData.asPred);
	sSpillData.asPred = NULL;

	return bInsertedSpill;
}

static IMG_VOID RenamePredArg(PINTERMEDIATE_STATE psState, PPREGALLOC_STATE psPredState, PARG psPredArg)
/*****************************************************************************
 FUNCTION	: RenamePredArg

 PURPOSE	: Rename a predicate argument based on the results of register allocation.

 PARAMETERS	: psState			- Compiler state.
			  psPredState		- Module state.
			  psPredArg			- Argument to rename.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psPredArg->uType == USEASM_REGTYPE_PREDICATE)
	{
		ASSERT(psPredArg->uNumber < psPredState->uNrPredicates);
		psPredArg->uNumber = psPredState->asPredicates[psPredArg->uNumber].uColour;
		psPredArg->psRegister = NULL;
	}
}

static IMG_VOID RenamePredRegBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvPredState)
/*****************************************************************************
 FUNCTION	: RenamePredicateRegisters

 PURPOSE	: Rename the registers in a block based on the results of register allocation.

 PARAMETERS	: psCodeBlock		- Block to rename registers within.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PPREGALLOC_STATE psPredState = (PPREGALLOC_STATE)pvPredState;
	USC_PVECTOR		psCurrPredsLiveOut;
	USC_PVECTOR		psNewPredsLiveOut;
	PINST			psInst;
	IMG_UINT32		uPredIdx;

	/*
		Rename predicates within the set of registers live into this block
	*/
	psCurrPredsLiveOut = &psBlock->sRegistersLiveOut.sPredicate;
	psNewPredsLiveOut = &psPredState->sPredicatesLive;

	ClearVector(psState, psNewPredsLiveOut);
	for	(uPredIdx = 0; uPredIdx < psState->uNumPredicates; uPredIdx++)
	{
		if	(VectorGet(psState, psCurrPredsLiveOut, uPredIdx))
		{
			VectorSet(psState, psNewPredsLiveOut, psPredState->asPredicates[uPredIdx].uColour, 1);
		}
	}
	VectorCopy(psState, psNewPredsLiveOut, psCurrPredsLiveOut);

	/*
		Rename the predicates used by instructions in this basic-block
	*/
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uDestIdx;
		IMG_UINT32	uSrcIdx;
		IMG_UINT32	uPred;

		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG	psDest = &psInst->asDest[uDestIdx];
			PARG	psOldDest = psInst->apsOldDest[uDestIdx];

			RenamePredArg(psState, psPredState, psDest);
			if (psOldDest != NULL)
			{
				RenamePredArg(psState, psPredState, psOldDest);
			}
		}
		for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
		{
			if (psInst->apsPredSrc[uPred] != NULL)
			{
				RenamePredArg(psState, psPredState, psInst->apsPredSrc[uPred]);
			}
		}
		for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
		{
			PARG	psSrc = &psInst->asArg[uSrcIdx];

			RenamePredArg(psState, psPredState, psSrc);
		}
	}

	switch (psBlock->eType)
	{
	case CBTYPE_COND:
		RenamePredArg(psState, psPredState, &psBlock->u.sCond.sPredSrc);
		break;
	case CBTYPE_UNCOND:
	case CBTYPE_EXIT:
		break;
	case CBTYPE_SWITCH: //should have been translated away
	case CBTYPE_UNDEFINED: //shouldn't exist post-construction
	case CBTYPE_CONTINUE:
		imgabort();
	}	
}

static IMG_VOID AllocRegSpillState(
									PINTERMEDIATE_STATE psState, 
									PPREGALLOCSPILL_STATE* ppsPredSpillState)
/*****************************************************************************
 FUNCTION	: AllocRegSpillState
    
 PURPOSE	: Allocate register allocation spill record data structures.

 PARAMETERS	: psState			- Compiler state.
			  uNrRegisters		- Number of registers that need to be allocated.
			  ppsPredSpillState	- Initialised with the allocated structure.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PPREGALLOCSPILL_STATE psPredSpillState;
	
	/* Allocate or clear a register state */
	if (*ppsPredSpillState == NULL)
	{
		*ppsPredSpillState = UscAlloc(psState, sizeof(PREGALLOCSPILL_STATE));
		psPredSpillState = *ppsPredSpillState;
		InitVector(&psPredSpillState->sNodesUsedInSpills, USC_MIN_VECTOR_CHUNK, IMG_FALSE);
	}
	else
	{
		psPredSpillState = *ppsPredSpillState;

		FreeRegSpillState(psState, ppsPredSpillState, IMG_FALSE);
	}

	{
		if (psState->uFlags & USC_FLAGS_TEXKILL_PRESENT)
		{
			PFIXED_REG_DATA	psOut;

			psOut = psState->sShader.psPS->psTexkillOutput;
			ASSERT(psOut->uVRegType == USEASM_REGTYPE_PREDICATE);
			psPredSpillState->uTexkillOutputNum = psOut->auVRegNum[0];
		}
		else
		{
			psPredSpillState->uTexkillOutputNum = USC_UNDEF;
		}
	}
}

static IMG_VOID AllocRegState(PINTERMEDIATE_STATE psState, 
							  PPREGALLOC_STATE* ppsPredState, 
							  PPREGALLOCSPILL_STATE psPredSpillState)
/*****************************************************************************
 FUNCTION	: AllocateRegisterState
    
 PURPOSE	: Allocate register allocation data structures.

 PARAMETERS	: psState			- Compiler state.
			  uNrRegisters		- Number of registers that need to be allocated.
			  ppsRegState		- Initialised with the allocated structure.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PPREGALLOC_STATE psPredState;
	const IMG_UINT32 uNumPredicates = psState->uNumPredicates;

	/* Allocate or clear a register state */
	if (*ppsPredState == NULL)
	{
		*ppsPredState = UscAlloc(psState, sizeof(PREGALLOC_STATE));
		psPredState = *ppsPredState;

		InitVector(&psPredState->sPredicatesLive, USC_MIN_VECTOR_CHUNK, IMG_FALSE);
	}
	else
	{
		psPredState = *ppsPredState;

		FreeRegState(psState, ppsPredState, IMG_FALSE);
	}

	{
		IMG_UINT32 i;

		/* Allocate the arrays */
		psPredState->psIntfGraph = IntfGraphCreate(psState, uNumPredicates);
		psPredState->asPredicates = UscAlloc(psState, sizeof(psPredState->asPredicates[0]) * uNumPredicates);
		psPredState->auPredicatesSortedByDegree = UscAlloc(psState, 
														   sizeof(IMG_UINT32) * uNumPredicates);
		psPredState->auPredicateStack = UscAlloc(psState, sizeof(IMG_UINT32) * uNumPredicates);

		psPredState->auSpillNodes = UscAlloc(psState, sizeof(IMG_UINT32) * uNumPredicates);

		/* Initialise the state */

		psPredState->uNrPredicates = psState->uNumPredicates;

		for (i = 0; i < psPredState->uNrPredicates; i++)
		{
			PPREDICATE	psPred = &psPredState->asPredicates[i];

			psPred->uColour = PREDICATE_COLOUR_PENDING;
			psPred->uMaxColour = EURASIA_USE_PREDICATE_BANK_SIZE;
			psPred->uMinColour = 0;
			psPred->uBestColour = USC_UNDEF;
		}

		psPredState->psRegAllocSpillState = psPredSpillState;
	}
}

static 
IMG_UINT32 ChooseNewSpillPredicate(PINTERMEDIATE_STATE	psState, 
								   PPREGALLOC_STATE		psPredState, 
								   IMG_UINT32			uUncolourableNode)
/*****************************************************************************
 FUNCTION	: ChooseNewSpillPredicate
    
 PURPOSE	: If a predicate which was introduced during a previous spill can't
			  be colourable then choose a different predicate which interferes
			  with it to spill. 
			  
			  All the predicates introduced during spilling have very short lifetimes 
			  so we assume register pressure can't be reduced by spilling them.

 PARAMETERS	: psState			- Compiler state.
			  psRegState		- Register allocation state.
			  uUncolourableNode	- Predicate which couldn't coloured.
			  
 RETURNS	: Either the number of another predicate to spill instead or USC_UNDEF
			  if another predicate which interferes with this one is already marked
			  for spilling.
*****************************************************************************/
{
	PADJACENCY_LIST			psAdjList = IntfGraphGetVertexAdjacent(psPredState->psIntfGraph, uUncolourableNode);
	IMG_UINT32				uOtherNode;
	ADJACENCY_LIST_ITERATOR	sIter;

	for (uOtherNode = FirstAdjacent(psAdjList, &sIter); !IsLastAdjacent(&sIter); uOtherNode = NextAdjacent(&sIter))
	{
		PPREDICATE	psOtherNode = &psPredState->asPredicates[uOtherNode];

		/*
			Check this isn't a node with a fixed colour.		
		*/
		if (
				(psState->uFlags & USC_FLAGS_TEXKILL_PRESENT) != 0 && 
				uOtherNode == psPredState->psRegAllocSpillState->uTexkillOutputNum
		   )
		{
			continue;
		}

		/*
			Check for a node created when inserting previous spills.
		*/
		if (VectorGetRange(psState, &psPredState->psRegAllocSpillState->sNodesUsedInSpills, uOtherNode, uOtherNode))
		{
			continue;
		}

		/*
			The other node is already marked for spilling.
		*/
		if (psOtherNode->uColour == PREDICATE_COLOUR_UNCOLOURABLE)
		{
			return USC_UNDEF;
		}

		/*
			Spill the other node instead.
		*/
		psOtherNode->uColour = PREDICATE_COLOUR_UNCOLOURABLE;
		return uOtherNode;
	}
	imgabort();
}

static
IMG_VOID SetPredicateToFixedValue(PINTERMEDIATE_STATE	psState,
								  PPREGALLOC_STATE		psPredState,
								  PCODEBLOCK			psBlock,
								  PINST					psInsertBeforeInst,
								  IMG_UINT32			uPredDest,
								  IMG_UINT32			uPredSrc,
								  IMG_BOOL				bPredNegate,
								  IMG_BOOL				bSetToTrue)
/*****************************************************************************
 FUNCTION	: SetPredicateToFixedValue
    
 PURPOSE	: Generate an instruction to set a predicate register to a fixed
			  value.

 PARAMETERS	: psState			- Compiler state.
			  psPredState		- If non-NULL then the restrictions on the source
								predicate are updated.
			  psBlock			- Block to insert the new instruction into.
			  psInsertBeforeInst
								- Point to insert the new instruction at.
			  uPredDest			- Predicate to write.
			  uPredSrc			- If not USC_UNDEF a predicate to control	
								update of the destination.
			  bPredNegate		- Negate modifier for the predicate source.
			  bSetToTrue		- Value to set the predicate to.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psSetpInst = AllocateInst(psState, IMG_NULL);

	SetOpcodeAndDestCount(psState, psSetpInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
	psSetpInst->u.psTest->eAluOpcode = IAND;
	MakePredicateDest(psState, psSetpInst, TEST_PREDICATE_DESTIDX, uPredDest);

	if (uPredSrc != USC_UNDEF)
	{
		SetPredicate(psState, psSetpInst, uPredSrc, bPredNegate);
		SetPartiallyWrittenDest(psState, psSetpInst, 0 /* uDestIdx */, &psSetpInst->asDest[TEST_PREDICATE_DESTIDX]);

		/*
			Update the restrictions on the possible hardware register numbers for the source predicate.
		*/
		if (psPredState != NULL)
		{
			SetInstHardwarePredicateRangeSupported(psState, psPredState, psSetpInst);
		}
	}
	
	if (bSetToTrue)
	{
		psSetpInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
	}
	else
	{
		psSetpInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
	}
	psSetpInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
	psSetpInst->asArg[0].uNumber = 0;
	psSetpInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psSetpInst->asArg[1].uNumber = 0;

	InsertInstBefore(psState, psBlock, psSetpInst, psInsertBeforeInst);
}

static
IMG_VOID SetImmediate(PINTERMEDIATE_STATE	psState, 
					  PPREGALLOC_STATE		psPredState,
					  PINST					psInsertBeforeInst, 
					  IMG_UINT32			uPredSrc,
					  IMG_BOOL				bPredSrcNegate,
					  IMG_UINT32			uDestTempNum, 
					  IMG_UINT32			uImm)
/*****************************************************************************
 FUNCTION	: SetImmediate
    
 PURPOSE	: Generate an instruction to set a temporary register to a fixed
			  value.

 PARAMETERS	: psState				- Compiler state.
			  psPredState			- If non-NULL then the restrictions on the source
									predicate are updated.
			  psInsertBeforeInst	- Point to insert the new instruction.
			  uPredSrc				- If not USC_UNDEF a predicate to control
									update of the destination.
			  bPredSrcNegate		- Negation modifier for the predicate source.
			  uDestTempNum			- Temporary register to set.
			  uImm					- Immediate value to copy into the temporary.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psMovInst;

	psMovInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psMovInst, IMOV);
	SetDest(psState, psMovInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uDestTempNum, UF_REGFORMAT_F32);
	if (uPredSrc != USC_UNDEF)
	{
		SetPartialDest(psState, psMovInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uDestTempNum, UF_REGFORMAT_F32);
		SetPredicate(psState, psMovInst, uPredSrc, bPredSrcNegate);

		/*
			Update the restrictions on the possible hardware register numbers for the source predicate.
		*/
		if (psPredState != NULL)
		{
			SetInstHardwarePredicateRangeSupported(psState, psPredState, psMovInst);
		}
	}
	psMovInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
	psMovInst->asArg[0].uNumber = uImm;
	InsertInstBefore(psState, psInsertBeforeInst->psBlock, psMovInst, psInsertBeforeInst);
}	

static
IMG_VOID ExpandPredicatedMOVP(PINTERMEDIATE_STATE psState, PPREGALLOC_STATE psPredState, PINST psMOVPInst)
/*****************************************************************************
 FUNCTION	: ExpandPredicatedMOVP
    
 PURPOSE	: Expand an instruction copying from one predicate register to
			  another which is itself predicated.

 PARAMETERS	: psState			- Compiler state.
			  psPredState		- Predicate register allocator state.
			  psMOVPInst		- Instruction to expand.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uTempNum = GetNextRegister(psState);
	PINST		psTestInst;

	ASSERT(psMOVPInst->uDestCount == 1);
	ASSERT(psMOVPInst->asDest[0].uType == USEASM_REGTYPE_PREDICATE);

	ASSERT(psMOVPInst->asArg[0].uType == USEASM_REGTYPE_PREDICATE);

	/*
		(PMASK)	MOVP	PDEST, PSRC
			->
				MOV		RTEMP = FALSE
		(PSRC)	MOV		RTEMP = TRUE
		(PMASK)	TEST	PDEST, RTEMP
	*/

	/* MOV RTEMP, FALSE */
	SetImmediate(psState, 
				 psPredState,
				 psMOVPInst, 
				 USC_UNDEF /* uPredSrc */, 
				 IMG_FALSE /* bPredSrcNegate */, 
				 uTempNum, 
				 0 /* uImmValue */);

	/* (PSRC) MOV RTEMP, TRUE */
	SetImmediate(psState, 
				 psPredState,
				 psMOVPInst, 
				 psMOVPInst->asArg[0].uNumber, 
				 psMOVPInst->u.psMovp->bNegate, 
				 uTempNum, 
				 1 /* uImmValue */);
	
	/* (PMASK) TEST PDEST, RTEMP */
	psTestInst = AllocateInst(psState, psMOVPInst);
	SetOpcode(psState, psTestInst, ITESTPRED);
	psTestInst->u.psTest->eAluOpcode = IOR;
	psTestInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
	MoveDest(psState, psTestInst, 0 /* uMoveToDestIdx */, psMOVPInst, 0 /* uMoveFromDestIdx */);
	CopyPartiallyWrittenDest(psState, psTestInst, 0 /* uMoveToDestIdx */, psMOVPInst, 0 /* uMoveFromDestIdx */);
	SetSrc(psState, psTestInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTempNum, UF_REGFORMAT_F32);
	CopyPredicate(psState, psTestInst, psMOVPInst);
	psTestInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psTestInst->asArg[1].uNumber = 0;
	InsertInstBefore(psState, psMOVPInst->psBlock, psTestInst, psMOVPInst);
}

static
IMG_BOOL ExpandMOVP(PINTERMEDIATE_STATE psState, PPREGALLOC_STATE psPredState, PINST psMOVPInst)
/*****************************************************************************
 FUNCTION	: ExpandMOVP
    
 PURPOSE	: Expand an instruction copying from one predicate register to
			  another.

 PARAMETERS	: psState			- Compiler state.
			  psPredState		- Predicate register allocator state.
			  psMOVPInst		- Instruction to expand.
			  
 RETURNS	: TRUE if the expansion required allocating another predicate register.
*****************************************************************************/
{
	IMG_BOOL	bRestart = IMG_FALSE;
	PARG		psMOVPSrc = &psMOVPInst->asArg[0];

	ASSERT(psMOVPInst->uDestCount == 1);
	ASSERT(psMOVPInst->asDest[0].uType == USEASM_REGTYPE_PREDICATE);

	if (psMOVPSrc->uType != USEASM_REGTYPE_PREDICATE)
	{
		IMG_UINT32	uMOVPPredSrc;
		IMG_BOOL	bMOVPPredNegate, bMOVPPredPerChan;
		IMG_BOOL	bFixedValue;

		ASSERT(psMOVPSrc->uType == USC_REGTYPE_BOOLEAN);

		bFixedValue = psMOVPSrc->uNumber ? IMG_TRUE : IMG_FALSE;
		if (psMOVPInst->u.psMovp->bNegate)
		{
			bFixedValue = !bFixedValue;
		}

		GetPredicate(psMOVPInst, &uMOVPPredSrc, &bMOVPPredNegate, &bMOVPPredPerChan, 0);
		ASSERT(!bMOVPPredPerChan);
		SetPredicateToFixedValue(psState, 
								 psPredState,
								 psMOVPInst->psBlock,
								 psMOVPInst,
								 psMOVPInst->asDest[0].uNumber,
								 uMOVPPredSrc,
								 bMOVPPredNegate,
								 bFixedValue);
	}
	/*
		Check for a trival predicate move (source and destination are the same).
	*/
	else if (psMOVPInst->asDest[0].uNumber == psMOVPSrc->uNumber)
	{
		if (psMOVPInst->u.psMovp->bNegate)
		{
			ExpandPredicatedMOVP(psState, psPredState, psMOVPInst);
			bRestart = IMG_TRUE;
		}
	}
	else if (NoPredicate(psState, psMOVPInst))
	{
		/*
					MOVP	PDEST, PSRC
				->
					(!PSRC)	SET		PDEST = FALSE
					(PSRC)	SET		PDEST = TRUE
		*/
		SetPredicateToFixedValue(psState, 
								 psPredState,
								 psMOVPInst->psBlock,
								 psMOVPInst,
								 psMOVPInst->asDest[0].uNumber,
								 USC_UNDEF /* uPredSrc */,
								 IMG_FALSE /* bPredNegate */,
								 IMG_FALSE /* bSetToTrue */);
		SetPredicateToFixedValue(psState, 
								 psPredState,
								 psMOVPInst->psBlock,
								 psMOVPInst,
								 psMOVPInst->asDest[0].uNumber,
								 psMOVPInst->asArg[0].uNumber,
								 psMOVPInst->u.psMovp->bNegate,
								 IMG_TRUE /* bSetToTrue */);
	}
	else
	{
		ExpandPredicatedMOVP(psState, psPredState, psMOVPInst);
		bRestart = IMG_TRUE;
	}

	RemoveInst(psState, psMOVPInst->psBlock, psMOVPInst);
	FreeInst(psState, psMOVPInst);

	return bRestart;
}

static
IMG_VOID CoalescePredicates(PINTERMEDIATE_STATE psState, 
							PPREGALLOC_STATE psPredState, 
							IMG_UINT32 uPredRenameFrom, 
							IMG_UINT32 uPredRenameTo)
/*****************************************************************************
 FUNCTION	: CoalescePredicates
    
 PURPOSE	: Coalesce two predicate registers.

 PARAMETERS	: psState			- Compiler state.
			  psPredState		- register allocator state.
			  uPredRenameFrom	- Predicate register to be removed.
			  uPredRenameTo		- Predicate register to replace by.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN					psPredRenameFromUseDef;
	PUSC_LIST_ENTRY					psListEntry;
	REPLACE_REGISTER_LIVEOUT_SETS	sContext;
	PVREGISTER						psPredRenameToVReg;
	ARG								sPredRenameToArg;
	PUSC_LIST_ENTRY					psNextListEntry;
	PPREDICATE						psRenameTo = &psPredState->asPredicates[uPredRenameTo];
	PPREDICATE						psRenameFrom = &psPredState->asPredicates[uPredRenameFrom];

	if (psPredState->psRegAllocSpillState->uTexkillOutputNum == uPredRenameFrom)
	{
		psPredState->psRegAllocSpillState->uTexkillOutputNum = uPredRenameTo;
	}

	/*
		Merge the restrictions on the hardware register numbers available to the two coalesced predicates.
	*/
	psRenameTo->uMaxColour = min(psRenameTo->uMaxColour, psRenameFrom->uMaxColour);
	psRenameTo->uMinColour = max(psRenameTo->uMinColour, psRenameFrom->uMinColour);

	/*
		Merge the edges for both nodes.
	*/
	IntfGraphMergeVertices(psState, psPredState->psIntfGraph, uPredRenameTo, uPredRenameFrom);
	ASSERT(IntfGraphGetVertexDegree(psPredState->psIntfGraph, uPredRenameFrom) == 1);

	psPredRenameToVReg = GetVRegister(psState, USEASM_REGTYPE_PREDICATE, uPredRenameTo);
	psPredRenameFromUseDef = UseDefGet(psState, USEASM_REGTYPE_PREDICATE, uPredRenameFrom);

	InitInstArg(&sPredRenameToArg);
	sPredRenameToArg.uType = USEASM_REGTYPE_PREDICATE;
	sPredRenameToArg.uNumber = uPredRenameTo;
	sPredRenameToArg.psRegister = psPredRenameToVReg;
	
	/*
		Replace the renamed register in all instructions.
	*/
	for (psListEntry = psPredRenameFromUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUseDef;

		psNextListEntry = psListEntry->psNext;
		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		ASSERT(psUseDef->psUseDefChain == psPredRenameFromUseDef);

		UseDefSubstUse(psState, psPredRenameFromUseDef, psUseDef, &sPredRenameToArg);
	}

	/*
		Rename the predicate register in the live out sets for all blocks.
	*/
	sContext.uRenameFromRegType = USEASM_REGTYPE_PREDICATE;
	sContext.uRenameFromRegNum = uPredRenameFrom;
	sContext.uRenameToRegType = USEASM_REGTYPE_PREDICATE;
	sContext.uRenameToRegNum = uPredRenameTo;
	DoOnAllBasicBlocks(psState, 
					   ANY_ORDER,
					   ReplaceRegisterLiveOutSetsBP,
					   IMG_FALSE,
					   &sContext);

 	UseDefDelete(psState, psPredRenameFromUseDef);
}

static
IMG_BOOL ExpandMOVPInstructions(PINTERMEDIATE_STATE psState, PPREGALLOC_STATE psPredState)
/*****************************************************************************
 FUNCTION	: ExpandMOVPInstructions
    
 PURPOSE	: Replace all predicate move instructions by sequences of
			  directly supported instructions.

 PARAMETERS	: psState			- Compiler state.
			  psPredState		- Predicate register allocator state.
			  
 RETURNS	: TRUE if the expansion of MOVP instructions required predicate
			  register allocation to be restarted.
*****************************************************************************/
{
	SAFE_LIST_ITERATOR	sIter;
	IMG_BOOL			bRestart;

	if (SafeListIsEmpty(&psState->asOpcodeLists[IMOVPRED]))
	{
		return IMG_FALSE;
	}

	InstListIteratorInitialize(psState, IMOVPRED, &sIter);
	bRestart = IMG_FALSE;
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		if (ExpandMOVP(psState, psPredState, InstListIteratorCurrent(&sIter)))
		{
			bRestart = IMG_TRUE;
		}
	}
	InstListIteratorFinalise(&sIter);

	TESTONLY_PRINT_INTERMEDIATE("After expanding MOVPRED", "expmovp", IMG_TRUE);

	return bRestart;
}

static
IMG_BOOL SpillPredicatesAtPhaseEnd(PINTERMEDIATE_STATE psState, PPREGALLOC_STATE psPredState)
/*****************************************************************************
 FUNCTION	: SpillPredicatesAtPhaseEnd
    
 PURPOSE	: Check for cases where an intermediate predicate register is live at the
			  point of a special block transition in the program which causes the
			  contents of the hardware predicate registers to be lost. In this case
			  we need to force the intermediate predicate register to be spilled
			  to a temporary register.

 PARAMETERS	: psState			- Compiler state.
			  psPredState		- Predicate register allocator state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uPredIdx;
	PCODEBLOCK	psPixelRatePhaseEndBlock;
	PCODEBLOCK	psPreFBPhaseEndBlock;
	IMG_BOOL	bForceSpills;

	psPreFBPhaseEndBlock = psPixelRatePhaseEndBlock = NULL;
	/*
		Check if the program contains a pixel rate to sample rate transition.
	*/
	if ((psState->uFlags2 & USC_FLAGS2_SPLITCALC) != 0 && psState->psPreSplitBlock != NULL)
	{
		psPixelRatePhaseEndBlock = psState->psPreSplitBlock;
	}
	/*
		Check if the program contains feedback to the ISP/VCB.
	*/
	if ((psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC) != 0 && psState->psPreFeedbackDriverEpilogBlock != NULL)
	{
		psPreFBPhaseEndBlock = psState->psPreFeedbackDriverEpilogBlock;
	}

	/*
		Check if the program contains no transitions.
	*/
	if (psPreFBPhaseEndBlock == NULL && psPixelRatePhaseEndBlock == NULL)
	{
		return IMG_FALSE;
	}

	/*
		For each intermediate predicate register check if it is live across
		a transition.
	*/
	bForceSpills = IMG_FALSE;
	for	(uPredIdx = 0; uPredIdx < psState->uNumPredicates; uPredIdx++)
	{
		IMG_BOOL	bForceSpill;

		bForceSpill = IMG_FALSE;
		if (psPreFBPhaseEndBlock != NULL)
		{
			/*
				Check if the predicate is live across feedback to the ISP.
			*/
			if (GetRegisterLiveMask(psState, 
									&psPreFBPhaseEndBlock->sRegistersLiveOut,
									USEASM_REGTYPE_PREDICATE, 
									uPredIdx, 
									0 /* uArrayOffset */) != 0)
			{
				bForceSpill = IMG_TRUE;
			}
		}
		
		/*
			Check if the predicate is live across the pixel-rate/sample-rate chance.
		*/
		if (psPixelRatePhaseEndBlock != NULL)
		{
			if (GetRegisterLiveMask(psState, 
									&psPixelRatePhaseEndBlock->sRegistersLiveOut, 
									USEASM_REGTYPE_PREDICATE, 
									uPredIdx, 
									0 /* uArrayOffset */) != 0)
			{
				bForceSpill = IMG_TRUE;
			}
		}

		/*
			Save the contents of the predicate into a temporary after each define
			and restore it immediately before each use.
		*/
		if (bForceSpill)
		{
			bForceSpills = IMG_TRUE;
			SpillPredicateRegister(psState, psPredState->psRegAllocSpillState, uPredIdx);
		}
	}
	return bForceSpills;	
}

IMG_INTERNAL 
IMG_VOID AssignPredicateRegisters(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: AssignPredicateRegisters
    
 PURPOSE	: Assign hardware predicate registers for the instructions in the program.

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{	
	if (psState->uNumPredicates > 0)
	{
		IMG_UINT32 uNrSpillNodes;
		IMG_UINT32 i, j;
		PPREGALLOC_STATE psPredState = NULL;
		PPREGALLOCSPILL_STATE psPredSpillState = NULL;
		IMG_BOOL bFirstPass;

		/* Partially reset the register interference graph state. */
		AllocRegSpillState(psState, &psPredSpillState);

		bFirstPass = IMG_TRUE;
		for (;;)
		{
			IMG_UINT32				uPrecolouredCount;
			IMG_BOOL				bCoalescedPredicates;
			SAFE_LIST_ITERATOR		sIter;
			CONSTRUCT_GRAPH_CONTEXT	sConstructGraphCtx;

			/* Reset the register interference graph state. */
			AllocRegState(psState, &psPredState, psPredSpillState);

			/*
				Allocate storage for the set of currently live predicates when processing a block
				to create the interference graph.
			*/
			sConstructGraphCtx.psPredState = psPredState;
			sConstructGraphCtx.psLiveSet = SparseSetAlloc(psState, psPredState->uNrPredicates);

			/* Generate a register interference graph for the program. */
			DoOnAllBasicBlocks(psState, ANY_ORDER, ConstructPredicateInterferenceGraphBP, IMG_FALSE, &sConstructGraphCtx);

			SparseSetFree(psState, sConstructGraphCtx.psLiveSet);
			sConstructGraphCtx.psLiveSet = NULL;

			/*
				Try to coalesce moves of predicates.
			*/
			bCoalescedPredicates = IMG_FALSE;
			InstListIteratorInitialize(psState, IMOVPRED, &sIter);
			for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
			{
				PINST		psMOVPInst = InstListIteratorCurrent(&sIter);
				IMG_UINT32	uPredSrc;
				IMG_UINT32	uPredDest;

				ASSERT(psMOVPInst->uDestCount == 1);
				ASSERT(psMOVPInst->asDest[0].uType == USEASM_REGTYPE_PREDICATE);
				uPredDest = psMOVPInst->asDest[0].uNumber;

				if (psMOVPInst->asArg[0].uType == USEASM_REGTYPE_PREDICATE && !psMOVPInst->u.psMovp->bNegate)
				{
					uPredSrc = psMOVPInst->asArg[0].uNumber;

					if (uPredDest == uPredSrc || !IntfGraphGet(psPredState->psIntfGraph, uPredDest, uPredSrc))
					{
						if (uPredDest != uPredSrc)
						{
							CoalescePredicates(psState, psPredState, uPredDest, uPredSrc);
						}	
		
						RemoveInst(psState, psMOVPInst->psBlock, psMOVPInst);
						FreeInst(psState, psMOVPInst);

						bCoalescedPredicates = IMG_TRUE;
					}
				}
			}
			InstListIteratorFinalise(&sIter);
			if (bCoalescedPredicates)
			{
				TESTONLY_PRINT_INTERMEDIATE("After coalescing predicates", "coalesce_pred", IMG_TRUE);
			}

			/*
				The hardware doesn't support moving predicate data directly so if there are any uncoalesced MOVP
				instructions expand them to a supported instruction sequence.
			*/
			if (ExpandMOVPInstructions(psState, psPredState))
			{
				continue;
			}

			/*
				Force predicate registers which are live across a phase end to be spilled.
			*/
			if (bFirstPass)
			{
				bFirstPass = IMG_FALSE;
				if (SpillPredicatesAtPhaseEnd(psState, psPredState))
				{
					continue;
				}
			}

			/* Sort the registers by degree */
			for (i = 0; i < psPredState->uNrPredicates; i++)
			{
				IMG_UINT32	uNodeDegree = IntfGraphGetVertexDegree(psPredState->psIntfGraph, i);

				for (j = 0; j < i; j++)
				{
					IMG_UINT32 uPrevNode = psPredState->auPredicatesSortedByDegree[j];
					IMG_UINT32 uPrevNodeDegree = IntfGraphGetVertexDegree(psPredState->psIntfGraph, uPrevNode);

					if (uPrevNodeDegree < uNodeDegree)
					{
						break;
					}
				}
				memmove(&psPredState->auPredicatesSortedByDegree[j + 1], 
						&psPredState->auPredicatesSortedByDegree[j],
						(i - j) * sizeof(psPredState->auPredicatesSortedByDegree[0]));
				psPredState->auPredicatesSortedByDegree[j] = i;
			}

			uPrecolouredCount = 0;
			if (psState->uFlags & USC_FLAGS_TEXKILL_PRESENT)
			{
				/* Remove the registers that hold the results of the shader so we can precolour them. */
				IntfGraphRemove(psState, psPredState->psIntfGraph, psPredState->psRegAllocSpillState->uTexkillOutputNum);
				uPrecolouredCount++;
			}

			/* Find the registers that can be coloured. */
			psPredState->uPredicateStackSize = 0;
			while (psPredState->uPredicateStackSize != (psPredState->uNrPredicates - uPrecolouredCount))
			{
				while (SimplifyGraph(psState, psPredState, IMG_FALSE));
				while (SimplifyGraph(psState, psPredState, IMG_TRUE));
			}

			if (psState->uFlags & USC_FLAGS_TEXKILL_PRESENT)
			{
				IMG_UINT32	uTexkillRegNum = psPredState->psRegAllocSpillState->uTexkillOutputNum;
				PPREDICATE	psTexkill = &psPredState->asPredicates[uTexkillRegNum];

				/* 
					Precolour the registers which hold the results of the shader so
					the code to pack them into the output buffer will know where to
					find them.
				*/
				ASSERT(USC_OUTPUT_TEXKILL_PRED < psTexkill->uMaxColour);
				psTexkill->uColour = USC_OUTPUT_TEXKILL_PRED;
				IntfGraphInsert(psState, psPredState->psIntfGraph, uTexkillRegNum);
			}

			/* 
			   Actually colour all the registers and build up an array of those
			   that need to be spilled.
			*/
			uNrSpillNodes = 0;
			for (i = 0; i < psPredState->uPredicateStackSize; i++)
			{
				IMG_UINT32	uNode = psPredState->auPredicateStack[psPredState->uPredicateStackSize - i - 1];
				PPREDICATE	psPred = &psPredState->asPredicates[uNode];

				psPred->uColour = ColourNode(psPredState, uNode);
				if (psPred->uColour != PREDICATE_COLOUR_UNCOLOURABLE)
				{
					IntfGraphInsert(psState, psPredState->psIntfGraph, uNode);
				}
				else
				{
					psPredState->auSpillNodes[uNrSpillNodes++] = uNode;
				}
			}

			/* Spill the registers that can't be coloured. */
			if (uNrSpillNodes > 0)
			{
				IMG_BOOL	bInsertSpill = IMG_FALSE;

				for (i = 0; i < uNrSpillNodes; i++)
				{
					IMG_UINT32 uUncolourableNode = psPredState->auSpillNodes[i];
					IMG_UINT32 uNodeToSpill;

					if (!VectorGetRange(psState, 
										&psPredState->psRegAllocSpillState->sNodesUsedInSpills, 
										uUncolourableNode, 
										uUncolourableNode))
					{
						uNodeToSpill = uUncolourableNode;
					}
					else
					{
						uNodeToSpill = ChooseNewSpillPredicate(psState, psPredState, uUncolourableNode);
						if (uNodeToSpill == USC_UNDEF)
						{
							continue;
						}
					}

					if (SpillPredicateRegister(psState, psPredState->psRegAllocSpillState, uNodeToSpill))
					{
						bInsertSpill = IMG_TRUE;
					}
				}
				ASSERT(bInsertSpill);

				TESTONLY_PRINT_INTERMEDIATE("After spilling nodes", "preg_spill", IMG_FALSE);
			}
			else
			{
				/*	
					All predicates successfully coloured.
				*/
				break;
			}
		}

		/* Rename from input registers to hardware registers. */
		DoOnAllBasicBlocks(psState, ANY_ORDER, RenamePredRegBP, IMG_FALSE, psPredState);

		/* Remap the information about program outputs. */
		if (psState->uFlags & USC_FLAGS_TEXKILL_PRESENT)
		{
			PFIXED_REG_DATA	psOut;

			psOut = psState->sShader.psPS->psTexkillOutput;
		
			ASSERT(psPredState->asPredicates[psOut->auVRegNum[0]].uColour == psOut->sPReg.uNumber);
			psOut->auVRegNum[0] = psOut->sPReg.uNumber;
		}

		/* Release allocated memory */
		FreeRegSpillState(psState, &psPredSpillState, IMG_TRUE);
		FreeRegState(psState, &psPredState, IMG_TRUE);
	}

	/* Free USE-DEF information for predicate registers. */
	UseDefFreeAll(psState, USEASM_REGTYPE_PREDICATE);

	/*
		Flag to later stages that intermediate predicate registers have been
		replaced by hardware predicate registers.
	*/
	psState->uFlags2 |= USC_FLAGS2_ASSIGNED_PREDICATE_REGNUMS;
}

/******************************************************************************
 * Synchronization for Flow Control *******************************************
******************************************************************************/
//First some utility/bottom-up functions
static IMG_BOOL LoopContains(PCODEBLOCK psHeader, PCODEBLOCK psBlock)
{
	while (psBlock->psLoopHeader)
	{
		if (psBlock->psLoopHeader == psHeader) return IMG_TRUE;
		psBlock = psBlock->psLoopHeader;
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: InsertSetPredicate

 PURPOSE	: Insert an instruction to set a predicate to a defined value.

 PARAMETERS	: psState		- Compiler state.
			  psBlock		- Block to set up.
			  bSetToTrue	- The value to set the predicate to.
			  uPredSrc		- Predicate controlling setting of uPredDest
			  bPredNegate	- Whether the predicate should be inverted
			  bAppend		- If TRUE, the instruction is appended to the
							  block, otherwise it is prepended.
			  uPredDest		- Predicate to set.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID InsertSetPredicate(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psBlock,
								   IMG_BOOL				bSetToTrue,
								   IMG_UINT32			uPredDest)
{
	SetPredicateToFixedValue(psState, 
							 NULL /* psPredState */,
							 psBlock, 
							 NULL /* psInsertBeforeInst */, 
							 uPredDest, 
							 USC_UNDEF /* uPredSrc */,
							 IMG_FALSE /* bPredNegate */,
							 bSetToTrue);

	//ACL TODO this copied from old code, but....doesn't exactly seem to cover all possibilities?!?!
	VectorSet(psState, &psBlock->sRegistersLiveOut.sPredicate, uPredDest, 1);
}

//Code for handling sets of edges (exitting a subtree of dominator tree)

typedef struct _EDGE_LIST_
{
	/* Source of edge */
	PCODEBLOCK psSrc;
	/* Number of edge in apsSuccs array of source (see EDGE_DEST macro below) */
	IMG_UINT32 uNum;
	/* Next edge in this set/list */
	struct _EDGE_LIST_ *psNext;	
} EDGE_LIST, *PEDGE_LIST;

/* Is this a edge to the exit of the program. */
#define EDGE_ISEXIT(e) (e->uNum == USC_UNDEF)

/* Simple macro for destination of an edge */
#define EDGE_DEST(e) (e->psSrc->asSuccs[e->uNum].psDest)

static PEDGE_LIST PushEdge(PINTERMEDIATE_STATE psState, PEDGE_LIST psList, PCODEBLOCK psSrc, IMG_UINT32 uNum)
/******************************************************************************
 FUNCTION	: PushEdge
 
 PURPOSE	: Add an edge to the start of an edge list.

 PARAMETERS	: psState	- Compiler Intermediate State
			  psSrc		- Source for the edge.
			  uNum		- Either the offset of the edge in the source's successor
						array or USC_UNDEF for the edge to the exit.

 RETURNS	: The new edge list.
******************************************************************************/
{
	PEDGE_LIST psTemp = UscAlloc(psState, sizeof(EDGE_LIST));

	psTemp->psSrc = psSrc;
	psTemp->uNum = uNum;
	psTemp->psNext = psList;
	
	return psTemp;
}

static PEDGE_LIST MyEdges(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION	: MyEdges
 
 PURPOSE	: Constructs a set (PEDGE_LIST) of all the edges from a single node

 PARAMETERS	: psState	- Compiler Intermediate State
			  psBlock	- Codeblock being source of edges to put in set.

 RETURNS	: Pointer to (start of list of) newly allocated struct EDGE_LIST(s)
******************************************************************************/
{
	PEDGE_LIST psReturn = NULL;
	IMG_UINT32 i;
	
	if (psBlock->eType == CBTYPE_CONTINUE && psBlock->psLoopHeader)
	{
		//continue node.
		ASSERT (psBlock->uNumSuccs == 1 && 
				psBlock->asSuccs[0].psDest == psBlock->psLoopHeader);
		/*
			For SyncStart setup, we pretend such nodes have no edges,
			as they represent (infinite) copies of the loop body
			and hence never return control flow to the "real" loop.
		*/
		return NULL;
	}

	for (i = 0; i < psBlock->uNumSuccs; i++)
	{
		psReturn = PushEdge(psState, psReturn, psBlock, i);
	}
	if (psBlock == psBlock->psOwner->psExit)
	{
		psReturn = PushEdge(psState, psReturn, psBlock, USC_UNDEF);
	}
	return psReturn;
}

static PEDGE_LIST CombineEdgeLists(PINTERMEDIATE_STATE psState,
								   PCODEBLOCK psBlock,
								   PEDGE_LIST psDest,
								   PEDGE_LIST psSrc)
/******************************************************************************
 FUNCTION	: CombineEdgeLists
 
 PURPOSE	: Copies or moves from psSrc into psDest any edges that exit
			  psBlock's subtree. (Used to construct the set of edges exitting
			  a parent subtree from the corresponding sets for each child.)

 PARAMETERS	: psState	- Compiler Intermediate State
			  psBlock	- Root of subtree for which edge set we're constructing
			  psDest	- Edge set to which to add edges (EDGE_LIST structures)
			  psSrc		- Edge set to take edges from (if they exit parent tree)

 RETURNS	: Pointer to (new) start of parent edge set (i.e. containing
			  the passed-in value of psDest)
******************************************************************************/
{
	while (psSrc)
	{
		PEDGE_LIST psNext = psSrc->psNext;
		if (EDGE_ISEXIT(psSrc) || !Dominates(psState, psBlock, EDGE_DEST(psSrc)))
		{
			//edge exits parent subtree - so return
			psSrc->psNext = psDest;
			psDest = psSrc;
		}
		else
		{
			//edge is internal to dom subtree of psBlock - so remove
			ASSERT (EDGE_DEST(psSrc)->psIDom == NULL || Dominates(psState, EDGE_DEST(psSrc)->psIDom, psBlock));
			UscFree(psState, psSrc);
		}
		psSrc = psNext;
	}
	return psDest;
}

//First pass - locate syncs and loops; add continue nodes where necessary.
static
IMG_BOOL BreakEdge(PCODEBLOCK psSrc, PCODEBLOCK psDest)
/*****************************************************************************
 FUNCTION	: BreakEdge

 PURPOSE	: Check for two blocks if the edge between them is from inside a
			  loop to outside.

 PARAMETERS	: psSrc			- Edge source.
			  psDest		- Edge destination.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return LoopDepth(psDest) < LoopDepth(psSrc);
}

static IMG_BOOL LocateSyncs(PINTERMEDIATE_STATE psState,
							PCODEBLOCK psBlock,
							IMG_PBOOL abFuncsStatic)
/******************************************************************************
 FUNCTION	: LocateSyncs

 PURPOSE	: First pass of sync start - scans the dominator tree and sets
			  the 'bDomSync' field of every block dominating a SYNC. Also sets
			  (and (ab)uses) the bAddSyncStartAtStart field of each CODEBLOCK
			  to mean "this block is the header of a _loop_ containing a sync"
			  (note that a loop header node might dominate a sync outside its
			  loop, i.e. if the sync comes on some path out of the loop to the
			  exit). This info is needed as *input* to SetSyncStartEnd, but is
			  hard to store in an array (i.e. indexed by block uIdx) as indices
			  may be changed by restructuring before SetSyncStartEnd.
			  Thirdly, adds 'continue' nodes ("fake" nodes representing
			  iterations of a loop after the first) for loops containing syncs;
			  these make the subsequent restructuring phase to operate
			  correctly on loops without any further special treatment.

 NOTE 1		: Continue nodes are in the form of CBTYPE_CONTINUE nodes with
			  psLoopHeader fields, and unique successors, being the header of
			  the loop of whose body they are conceptually a copy; this is a
			  (2nd) temporary abuse/violation of the normal meaning/invariants
			  of our data structures, but is undone by the next pass (Restructure).

 NOTE 2		: Also applies an optimization for *static* control flow (i.e. that
			  guaranteed to be the same for all pixels), so as to avoid useless
			  synchronization within subtrees which are entirely static.
			  
 PARAMETERS	: psState				- Compiler Intermediate State
			  psBlock				- Block (whose dominator subtree) to scan
			  abFuncsStatic			- uLabel-indexed array, is all control flow
									  within each function static?

 RETURNS	: TRUE if all nodes dominated by psBlock choose their successor
			  according to static criteria; FALSE if any block is not static.
******************************************************************************/
{
	/* closest (non-strictly) containing loop */
	PCODEBLOCK psLoopHeader = IsLoopHeader(psBlock) ? psBlock : psBlock->psLoopHeader;
	IMG_UINT32 uChild;
	IMG_BOOL bStatic = (psBlock->uNumSuccs < 2) ? IMG_TRUE : psBlock->bStatic;

	for (uChild = 0; uChild < psBlock->uNumDomChildren; uChild++)
	{
		if (!LocateSyncs(psState,
						 psBlock->apsDomChildren[uChild],
						 abFuncsStatic))
		{
			bStatic = IMG_FALSE;
		}
		if (psBlock->apsDomChildren[uChild]->bDomSync) psBlock->bDomSync = IMG_TRUE;
	}

	//set bDomSync flags concerned with this node's body (if not set already)
	if (!psBlock->bDomSync || (psLoopHeader && !psLoopHeader->bAddSyncAtStart))
	{
		IMG_BOOL bSyncHere = IMG_FALSE;
		if (IsCall(psState, psBlock))
		{
			bSyncHere = psBlock->psBody->u.psCall->psTarget->sCfg.psEntry->bDomSync;
		}
		else
		{
			PINST psInst;
			for (psInst = psBlock->psBody; psInst; psInst = psInst->psNext)
				if (RequiresGradients(psInst)) {bSyncHere = IMG_TRUE; break;}
		}
		if (bSyncHere)
		{
			PCODEBLOCK psHeaders;
			psBlock->bDomSync = IMG_TRUE;
			for (psHeaders = psLoopHeader; psHeaders; psHeaders = psHeaders->psLoopHeader)
				psHeaders->bAddSyncAtStart = IMG_TRUE;
		}
	}

	if (bStatic)
	{
		/*
			All threads entering this block, will go down the same path, i.e. do
			(or avoid) the same samples and in the same sequence. Thus, we do not
			need any extra synchronization control to handle program flow between
			this node(and it)'s children. Turn off such flags which would otherwise
			cause us to insert such controls.
		*/
		for (uChild = 0; uChild < psBlock->uNumDomChildren; uChild++)
		{
			if (psBlock->apsDomChildren[uChild]->bDomSync) ASSERT (psBlock->bDomSync);
			psBlock->apsDomChildren[uChild]->bDomSync = IMG_FALSE;
			//Also for loops containing syncs (all threads will do same #iters)
			psBlock->apsDomChildren[uChild]->bAddSyncAtStart = IMG_FALSE;
		}
		
		/*
			However, we may still need extra SYNCs if threads decide dynamically
			whether to enter this block or not; so, leave bDomSync/bAddSyncAtStart
			of this block untouched.
		*/
	}
	else if (psBlock->bAddSyncAtStart)
	{
		/*
			block is header of (dynamic) loop containing sync
			- so make a unique 'continue' block representing next iter
		*/
		PCODEBLOCK psContinue;
		PCODEBLOCK psParent = NULL;
		IMG_UINT32 uPred;
		IMG_UINT32 uDom;

		ASSERT (IsLoopHeader(psBlock) && psBlock->bDomSync);

		ASSERT (!psBlock->psOwner->bBlockStructureChanged);
		psContinue = AllocateBlock(psState, psBlock->psOwner);
		/*
			although after restructuring MergeBB's will need to be called,
			in the meantime we'll maintain enough invariants, for LocateSyncs
			and/or Restructure, by fixing up {post-,}dominator trees as we go
		*/
		psBlock->psOwner->bBlockStructureChanged = IMG_FALSE;

		//redirect all loop backedges to go to continue block (not header)
		for (uPred = 0; uPred < psBlock->uNumPreds;)
		{
			PCODEBLOCK psPred = psBlock->asPreds[uPred].psDest;
			IMG_UINT32 uSucc;
			if (!Dominates(psState, psBlock, psPred))
			{
				//not a loop predecessor. Examine next predecessor.
				uPred++;
				continue; 
			}

			//Loop predecessor. Firstly, update postdominance
			if (psPred->psIPostDom == psBlock)
			{
				psPred->psIPostDom = psContinue;
			}
			else
			{
				ASSERT (psPred == psBlock || !PostDominated(psState, psPred, psBlock));
			}

			//Compute into psParent the least common dominator of all psPred's.
			if (psParent)
			{
				while (!Dominates(psState, psParent, psPred)) psParent = psParent->psIDom;
			}
			else
			{
				psParent = psPred;
			}

			//retarget the edge to the continue block
			for (uSucc = 0; uSucc < psPred->uNumSuccs; uSucc++)
			{
				if (psPred->asSuccs[uSucc].psDest == psBlock)
				{
					RedirectEdge(psState, psPred, uSucc, psContinue);
					psBlock->psOwner->bBlockStructureChanged = IMG_FALSE;
				}
			}
			/*
				psPred will have been removed from psBlock->apsPreds,
				putting a new block in at that index. Examine that next.
			*/
		}
		/*
			The continue block is conceptually a copy of the whole loop body.
			(In particular, we pretend it has *no* edges exitting its subtree,
			as they'll all be munged/flagged/etc. as part of processing
			the _real_ loop.) However, e.g. ComputeLoopNestingTree will fail
			unless we give it at least a loop backedge (ignored in MyEdges)
				
			Hence, we'll represent this as an CONTINUE block, and use the
			psLoopHeader field to indicate the (header of the) loop
			of which it is a copy...
		*/
		ASSERT (psContinue->eType == CBTYPE_UNDEFINED);
		psContinue->eType = CBTYPE_CONTINUE;
		psContinue->psLoopHeader = psBlock;
		
		psContinue->uNumSuccs = 1;
		psContinue->asSuccs = UscAlloc(psState, sizeof(psContinue->asSuccs[0]));
		psContinue->asSuccs[0].psDest = psBlock;
		psContinue->asSuccs[0].uDestIdx = psBlock->uNumPreds;
		
		ResizeArray(psState,
					psBlock->asPreds,
					psBlock->uNumPreds * sizeof(psBlock->asPreds[0]),
					(psBlock->uNumPreds + 1) * sizeof(psBlock->asPreds[0]),
					(IMG_PVOID*)&psBlock->asPreds);
		psBlock->asPreds[psBlock->uNumPreds].psDest = psContinue;
		psBlock->asPreds[psBlock->uNumPreds].uDestIdx = 0;
		psBlock->uNumPreds++;
		
		/*
			Set postdominator so that anything postdominated by "the next iteration"
			still figures out it's postdominated by the "real" loop header.
		*/
		psContinue->psIPostDom = psBlock;

		//Add as child of psParent computed in previous loop.
		ResizeArray(psState, psParent->apsDomChildren,
						 psParent->uNumDomChildren * sizeof(PCODEBLOCK),
						 (psParent->uNumDomChildren + 1) * sizeof(PCODEBLOCK),
						 (IMG_PVOID*)&psParent->apsDomChildren);

		for (uDom = 0; uDom < psParent->uNumDomChildren; uDom++)
		{
			if (BreakEdge(psParent, psParent->apsDomChildren[uDom]))
			{
				break;
			}
		}
		memmove(psParent->apsDomChildren + uDom + 1, 
				psParent->apsDomChildren + uDom, 
 				sizeof(psParent->apsDomChildren[0]) * (psParent->uNumDomChildren - uDom));
		psParent->apsDomChildren[uDom] = psContinue;
		psParent->uNumDomChildren++;
		psContinue->psIDom = psParent;

		/*
			Set bDomSync of anything dominating the copy ---
			'normal' restructuring will then ensure this postdominates any/all
			other SYNCS, without special treatment for loops, which is the idea
		*/
		do
		{
			psContinue->bDomSync = IMG_TRUE;
			psContinue = psContinue->psIDom;
		} while (psContinue != psBlock);
	}

	/*
		mark calls to non-static functions as containing dynamic flow control.
		(We do this *after* processing actual children of the call, so that if
		what follows the (return from the) call is static, we can avoid extra
		syncs etc. there even if the function itself is dynamic.)
	*/
	if (bStatic && IsCall(psState, psBlock) &&
		!abFuncsStatic[psBlock->psBody->u.psCall->psTarget->uLabel])
	{
		bStatic = IMG_FALSE;
	}
	return bStatic;
}

//Step two: restructure multiple siblings with SYNCs to postdominate each other

/*
	Records a mapping from a pre-restructuring codeblock <C>, to a new 'flag
	setter' codeblock which'll set a variable to make the inserted 'switch'
	block (introduced to postdominate one subtree and dominate the other)
	will route control flow to the correct original codeblock i.e. <C>.
*/
typedef struct _CB_MAP_
{
	/* Original destination, to which we wish to route control flow */
	PCODEBLOCK psOrigDest;
	/* "New" post-restructuring block, sets flag to get there in the end */
	PCODEBLOCK psNewDest;
	/* Index into successor array of new switch block to get us there */
	IMG_UINT32 uSwitchSucc;
	/* Next mapping (for same switch block) */
	struct _CB_MAP_ *psNext;
} CB_MAP, *PCB_MAP;
 
static PCB_MAP GetFlagSetter(PINTERMEDIATE_STATE psState,
							 PCODEBLOCK psDest,
							 PCODEBLOCK psSwitch,
							 PCB_MAP *psMap)
/******************************************************************************
 FUNCTION	: GetFlagSetter

 PURPOSE	: Finds, or creates, a codeblock which, post-restructuring, will
			  route control flow to a specified (pre-restructure) codeblock,
			  VIA the switch block introduced by the restructuring.

 PARAMETERS	: psState	- Compiler intermediate state
			  psDest	- Desired (pre-restructure) codeblock we want to get to
			  psSwitch  - switch block (being setup) which'll select successor
			  psMap		- start of list of CB_MAPs + blocks already created

 RETURNS	: CB_MAP for desired codeblock/route - either an existing element
			  in psMap, or created & prepended to psMap if necessary.
******************************************************************************/
{
	PCB_MAP psSearch;
	IMG_UINT32 uNumSucc = 0; //edge from switch block which this flag setter'll select

	for (psSearch = *psMap; psSearch; psSearch = psSearch->psNext)
	{
		if (psSearch->psOrigDest == psDest) break;
		uNumSucc++; //count indices already assigned to successors
	}

	if (!psSearch)
	{
		//not found. make new.
		psSearch = UscAlloc(psState, sizeof(*psSearch));
		psSearch->psOrigDest = psDest;
		psSearch->psNewDest = AllocateBlock(psState, psSwitch->psOwner);
		SetBlockUnconditional(psState, psSearch->psNewDest, psSwitch);
		psSearch->uSwitchSucc = uNumSucc;
		//prepend to list
		psSearch->psNext = *psMap;
		*psMap = psSearch;
	}
	return psSearch;
}

static IMG_VOID DoRestructuring(PINTERMEDIATE_STATE psState, PEDGE_LIST psFirstEdges, PCODEBLOCK psSecond, IMG_PUINT32 puSecPredsMask)
/******************************************************************************
 FUNCTION	: DoRestructuring

 PURPOSE	: Applies a restructuring transformation to make one dominator
			  subtree postdominate another. (Done by introducing a new 'switch
			  block' node which postdominates the first subtree and dominates
			  the other.)

 PARAMETERS	: psState			- Compiler Intermediate State
			  psFirstEdges		- List of edges exitting the first subtree
			  psSecond			- Root node of second subtree
			  puSecPredsMask	- bitmask indicating which predecessors of
								<psSecond> must be rerouted via the new block
								(specifically, only those from outside the
								second subtree need be; if psSecond is itself
								header of a loop, necessarily contained within
								its subtree, then backedges to psSecond
								from within its own subtree need not be
								rerouted as these do not affect dominance).

 NOTE		: All parameters required to perform a restructure (including
			  psSecPredsMask, from dominance info) are calculated *before* any
			  restructuring is performed, as the restructuring operation (so)
			  messes up all the dominance information that we can't/don't patch
			  it up again until we call MergeBasicBlocks after all restructuring
			  is complete.

 RETURNS	: Nothing
******************************************************************************/
{
	//list of blocks which set a flag to indicate direction for switch
	PCB_MAP psTemp, psFlagSetters = NULL;
	/*
		the new 'switch block' which will postdominate the first subtree
		and dominate the second
	*/
	PCODEBLOCK psNewBlock = AllocateBlock(psState, psSecond->psOwner);
	//0. make a new block to route control flow through psNewBlock to psSecond...
	PCB_MAP psFlagToSecond = GetFlagSetter(psState, psSecond, psNewBlock, &psFlagSetters);
	//temps used for iteration
	PEDGE_LIST psEdge, psNextEdge;
	IMG_UINT32 i,j;
	
	ASSERT (psSecond->bDomSync);
	
	//1. psNewBlock dominates psSecond by intercepting (external) edges to psSecond
	{
		//existing predecessors of 2nd subtree (all external ones to be rerouted)
		PCODEBLOCK_EDGE asOldPreds = psSecond->asPreds;
		// #predecessors to transfer from psSecond to psFlagToSecond->psNewDest
		IMG_UINT32 uNumExtPreds = 0;
		for (i = 0; i < psSecond->uNumPreds; i++)
			if (GetBit(puSecPredsMask,i)) uNumExtPreds++;
		ASSERT (i == psSecond->uNumPreds);

		psFlagToSecond->psNewDest->asPreds = UscAlloc(psState, uNumExtPreds * sizeof(CODEBLOCK_EDGE));
		psFlagToSecond->psNewDest->uNumPreds = 0; //next index to fill in

		psSecond->asPreds = UscAlloc(psState, (i - uNumExtPreds) * sizeof(CODEBLOCK_EDGE));
		psSecond->uNumPreds = 0; //next index to fill in
		
		while (i-- > 0) //note post-decrement --> i is valid index inside body
		{
			PCODEBLOCK psSrc = asOldPreds[i].psDest, psDest;
			IMG_UINT32 uSrcSucc = asOldPreds[i].uDestIdx;
			if (GetBit(puSecPredsMask, i))
			{
				//edge incoming to psSecond from outside its dom tree: reroute!
				psDest = psFlagToSecond->psNewDest;
				psSrc->asSuccs[uSrcSucc].psDest = psDest;
			}
			else
			{
				//edge internal to psSecond's dom tree: preserve!
				psDest = psSecond;
			}
			//now (re-)add to appropriate predecessor array and increment index to fill in
			psSrc->asSuccs[uSrcSucc].uDestIdx = psDest->uNumPreds;
			psDest->asPreds[psDest->uNumPreds] = asOldPreds[i];
			psDest->uNumPreds++;
		}
		UscFree(psState, asOldPreds);
	}
	
	//leave routing of flag setters via psNewBlock until later
	psNewBlock->bDomSync = IMG_TRUE;
	
	//2. psNewBlock postdom's first subtree by intercepting all edges from it
	for (psEdge = psFirstEdges; psEdge; psEdge = psNextEdge)
	{
		PCB_MAP psFlagToDest;
		psNextEdge = psEdge->psNext;

		if (EDGE_ISEXIT(psEdge))
		{
			PCODEBLOCK	psOldExit = psEdge->psSrc;
			PCODEBLOCK	psNewExit;

			/*
				Create a new exit block.
			*/
			psNewExit = AllocateBlock(psState, psOldExit->psOwner);
			psNewExit->eType = CBTYPE_EXIT;

			ASSERT(psOldExit->eType == CBTYPE_EXIT);
			ASSERT(psOldExit->psOwner->psExit == psOldExit);

			/*
				Set the new exit block as the unconditional successor
				of the old exit block.
			*/
			SetBlockUnconditional(psState, psOldExit, psNewExit);

			/*
				Update the containing function's exit block.
			*/
			psOldExit->psOwner->psExit = psNewExit;

			/*
				Convert the exit edge to an internal edge pointing to the
				new exit block.
			*/
			psEdge->uNum = 0;
			ASSERT(EDGE_DEST(psEdge) == psNewExit);
		}
		
		if (EDGE_DEST(psEdge) != psFlagToSecond->psNewDest) //may have been set already by step 1.
		{
			//get a block which will set a flag such that psNewBlock will route control flow to the current edge dest.
			psFlagToDest = GetFlagSetter(psState, EDGE_DEST(psEdge), psNewBlock, &psFlagSetters);			
			
			//reroute edge to go to flag setter (which'll go to psNewBlock, then to original dest)
			RedirectEdge(psState, psEdge->psSrc, psEdge->uNum, psFlagToDest->psNewDest);
		}
	}
	
	//now setup switch block. First, count successors.
	for (j = 0, psTemp = psFlagSetters; psTemp; j++, psTemp = psTemp->psNext);
	switch (j)
	{
		case 1: //psSecond must be loop header, to which all edges eventually go (!)
			SetBlockUnconditional(psState, psNewBlock, psSecond);
			break;
		case 2:
		{
			//setup a conditional block
			IMG_UINT32 uPredReg = GetNextPredicateRegister(psState);
			ASSERT (psFlagSetters->psNext->psOrigDest == psSecond && psFlagSetters->psNext->uSwitchSucc == 0);
			SetBlockConditional(psState, psNewBlock, uPredReg, psSecond, psFlagSetters->psOrigDest, IMG_FALSE);
			for (psTemp = psFlagSetters; psTemp; psTemp = psTemp->psNext)
				InsertSetPredicate(psState, psTemp->psNewDest, (psTemp == psFlagSetters) ? IMG_FALSE : IMG_TRUE, uPredReg);
			break;
		}
		default:
		{
			//setup a switch block
			PARG psArg = UscAlloc(psState, sizeof(*psArg));
			PCODEBLOCK *apsSuccs = UscAlloc(psState, j * sizeof(PCODEBLOCK));
			IMG_PUINT32 auCaseValues = UscAlloc(psState, j * sizeof(IMG_UINT32));

			MakeNewTempArg(psState, UF_REGFORMAT_F32, psArg);

			for (psTemp = psFlagSetters, j = 0; psTemp; psTemp = psTemp->psNext, j++)
			{
				PINST psSetInst = AllocateInst(psState, NULL);
				SetOpcode(psState, psSetInst, IMOV);
				SetDestFromArg(psState, psSetInst, 0 /* uDestIdx */, psArg);
				psSetInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
				psSetInst->asArg[0].uNumber = j;
				AppendInst(psState, psTemp->psNewDest, psSetInst);
				apsSuccs[psTemp->uSwitchSucc] = psTemp->psOrigDest;
				auCaseValues[psTemp->uSwitchSucc] = j;
			}
			SetBlockSwitch(psState, psNewBlock, j, apsSuccs, psArg, IMG_FALSE/*no default*/, auCaseValues);
			ConvertSwitchBP(psState, psNewBlock, NULL);
		}
	}
	
	//clean-up by disposing of the 'flag setters' map.
	for (; psFlagSetters; psFlagSetters = psTemp)
	{
		psTemp = psFlagSetters->psNext;
		UscFree(psState, psFlagSetters);
	}
}

static PEDGE_LIST Restructure(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PBOOL pbNeedMerge)
/******************************************************************************
 FUNCTION	: Restructure

 PURPOSE	: Scans the dominator tree for nodes having two children both
			  (separately) dominating SYNCs (if said children are _not_
			  ordered by postdominance, the CFG must be restructured so that
			  they are so ordered). If any instance is found, the 1st such
			  instance is restructured (see DoRestructuring above), and
			  traversal terminates (as the dominator tree must be recomputed
			  before continuing, by restarting traversal).
			  
			  Also restores normal invariants for any 'continue' nodes added
			  by LocateSyncs by making them into simple unconditional blocks
			  leading (back) onto the (actual) loop header.

 PARAMETERS	: psState			- Compiler intermediate state
			  psBlock			- Root of dominator subtree to scan
			  pbNeedMerge		- Set to 'TRUE' if we now need to MergeBBs
								  to remove any continue nodes

 NOTE		: If a restructuring transformation is performed (and hence
			  traversal need be restarted), bBlockStructureChanged will be set
			  for the containing function.
			  
 RETURNS	: Set of edges leaving the dominator subtree of psBlock
******************************************************************************/
{
	PEDGE_LIST *apsChildLists = UscAlloc(psState, (psBlock->uNumDomChildren + 1) * sizeof(PEDGE_LIST));
	IMG_UINT32 uLastSyncStartChild = UINT_MAX;
	IMG_UINT32 i;

	/* Generally, treat edges from root node as edges from a child subtree */
	apsChildLists[0] = MyEdges(psState, psBlock);

	for (i = 0; i < psBlock->uNumDomChildren; i++)
	{
		PCODEBLOCK psChild = psBlock->apsDomChildren[i];
		apsChildLists[i + 1] = Restructure(psState, psChild, pbNeedMerge);
		//must restart if recursive call did any restructuring
		if (psBlock->psOwner->bBlockStructureChanged) break;
		
		if (psChild->bDomSync)
		{
			if (uLastSyncStartChild != UINT_MAX &&
				!PostDominated(psState, psBlock->apsDomChildren[uLastSyncStartChild], psChild))
			{
				IMG_PUINT32 puSecPredsExternBitMask = UscAlloc(psState,
					UINTS_TO_SPAN_BITS(psChild->uNumPreds) * sizeof(IMG_UINT32));
				/*
					Make psChild postdominate uLastSyncStartChild (since
					apsDomChildren are in topsort order, psChild does not at
					present reach uLastSyncStartChild (except possibly by loop
					backedges to psBlock), but the converse is *possible* but
					not guaranteed).
				*/
				IMG_UINT32 uPred;

				memset(puSecPredsExternBitMask,
					   0,
					   UINTS_TO_SPAN_BITS(psChild->uNumPreds) * sizeof(IMG_UINT32));
				
				for (uPred = 0; uPred < psChild->uNumPreds; uPred++)
					if (!Dominates(psState, psChild, psChild->asPreds[uPred].psDest))
						SetBit(puSecPredsExternBitMask, uPred, IMG_TRUE);
				DoRestructuring(psState, apsChildLists[uLastSyncStartChild + 1],
								psChild, puSecPredsExternBitMask);
				UscFree(psState, puSecPredsExternBitMask);
				break;
			}
			uLastSyncStartChild = i;
			psBlock->bDomSync = IMG_TRUE;
		}
	}
	if (psBlock->psOwner->bBlockStructureChanged)
	{
		//traversal to be restarted. element i+1 of apsChildren was set.
		for (i += 2; i-- > 0;)
		{
			PEDGE_LIST psEdge, psNext;
			for (psEdge = apsChildLists[i]; psEdge; psEdge = psNext)
			{
				psNext = psEdge->psNext;
				UscFree(psState, psEdge);
			}
		}
		UscFree(psState, apsChildLists);
		return NULL;
	}

	//sanitize any "continue" nodes (fake copies of loops)...
	for (i = 0; i < psBlock->uNumDomChildren; i++)
	{
		PCODEBLOCK psChild = psBlock->apsDomChildren[i];
		if (psChild->eType == CBTYPE_CONTINUE && psChild->psLoopHeader)
		{
			ASSERT (psChild->uNumSuccs == 1 &&
					psChild->asSuccs[0].psDest == psChild->psLoopHeader);
			//yes, child is a continue node. Make it into a "real" node...
			psChild->eType = CBTYPE_UNCOND;
			// (it will be merged into loop header by MergeBBs)
			*pbNeedMerge = IMG_TRUE;
		}
	}
	
	//compute return value
	{
		PEDGE_LIST psReturn = NULL;
		for (i = 0; i <= psBlock->uNumDomChildren; i++)
		{
			psReturn = CombineEdgeLists(psState,
										psBlock,
										psReturn,
										apsChildLists[i]);
		}
		UscFree(psState, apsChildLists);
		return psReturn;
	}
}

//step three - set sync_end and insert nested syncs.
static IMG_VOID SetSyncEnd(PINTERMEDIATE_STATE psState, PEDGE_LIST psEdge)
/******************************************************************************
 FUNCTION	: SetSyncEnd
 
 PURPOSE	: Sets SyncEnd flag on specified successor edge (i.e. to TRUE)

 PARAMETERS	: psState	- Compiler Intermediate State
			  psEdge	- Structure identifying edge to mark with sync_end
						  (by source & index into successor array)

 RETURNS	: Nothing
******************************************************************************/
{
	ASSERT(!EDGE_ISEXIT(psEdge));
	#if defined(UF_TESTBENCH)
	printf("syncend from %u to %u\n", psEdge->psSrc->uIdx, EDGE_DEST(psEdge)->uIdx);
	#endif /* defined(UF_TESTBENCH) */
	SetSyncEndOnSuccessor(psState, psEdge->psSrc, psEdge->uNum);
}

static PEDGE_LIST SetSyncStartEnd(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION	: SetSyncStartEnd

 PURPOSE	: Sets the sync_end flag on all necessary edges; marks blocks
			  requiring an extra sync at the beginning (for handling nested
			  flow control constructs) with bAddSyncAtStart.

 PARAMETERS	: psState	- Compiler Intermediate State
			  psBlock	- Root of dominator subtree to traverse

 RETURNS	: A list of edges exitting the specified block's dominator subtree

 NOTE 1		: Assumes that for each node in the dominator tree, all children
			  containing SYNCs are totally ordered by postdominance (this is
			  ensured by previous passes of restructuring, etc.).

 NOTE 2		: This restores the correct meaning of the 'bAddSyncAtStart' field
			  of each CODEBLOCK, as during sync start setup, we abuse this flag
			  (set by LocateSyncs, and used as input to this stage) to mean
			  'block is header of [a loop containing a SYNC]'.
*****************************************************************************/
{
	/*
		Edges leaving child subtrees. Also edges from header node, as these have the
		same potential targets (our children, processed here; or children of ancestors).
		Hence, element 0 is our edges, and element (i+1) is the edges from apsDomChildren[i].
	*/
	PEDGE_LIST *apsChildLists = UscAlloc(psState, (psBlock->uNumDomChildren + 1) * sizeof(PEDGE_LIST));
	IMG_UINT32 i, uNumSyncStartChildren;
	/*
		In LocateSyncs, we abused bAddSyncAtStart to mean
		"this block is header of loop containing a sync". Extract this info
		now, and reset the flag to it's proper meaning (i.e. always FALSE at
		entry to SyncStartDomTree, as we're doing a post-order traversal).
	*/
	IMG_BOOL bLoopSync = psBlock->bAddSyncAtStart;
	ASSERT (!bLoopSync || IsLoopHeader(psBlock));
	psBlock->bAddSyncAtStart = IMG_FALSE;

	/* Generally, treat edges from root node as edges from a child subtree */
	apsChildLists[0] = MyEdges(psState, psBlock);
	
	if (IsCall(psState, psBlock))
	{
		/*
			Block counts as "dominating a sync-end" - causing an extra sync to
		    be added for nested flow control - if the function contains a sync-end.
			(However, in contrast to USC1, the extra sync'll be added before the
			call, on a per-call basis for conditional calls only, rather than into
			the function (i.e. for all calls).
		*/
		psBlock->bDomSyncEnd = psBlock->psBody->u.psCall->psTarget->sCfg.psEntry->bDomSyncEnd;
		ASSERT (!psBlock->bDomSyncEnd || psBlock->bDomSync);
	}

	//perform recursive traversal to set data for children and gather edges
	for (i = 0, uNumSyncStartChildren = 0; i < psBlock->uNumDomChildren; i++)
	{
		PCODEBLOCK psChild = psBlock->apsDomChildren[i];
		apsChildLists[i + 1] = SetSyncStartEnd(psState, psChild);
		if (psChild->bDomSync) uNumSyncStartChildren++;
		if (psChild->bDomSyncEnd) psBlock->bDomSyncEnd = IMG_TRUE;
	}

	#if defined(UF_TESTBENCH)
	printf("For %u: edge lists: \n", psBlock->uIdx);
	for (i = 0; i <= psBlock->uNumDomChildren; i++)
	{
		PEDGE_LIST	psEdges;
		printf("\tFor %u: ", i == 0 ? psBlock->uIdx : psBlock->apsDomChildren[i - 1]->uIdx);
		for (psEdges = apsChildLists[i]; psEdges; psEdges = psEdges->psNext)
		{
			printf("%u->", psEdges->psSrc->uIdx);
			if (EDGE_ISEXIT(psEdges))
			{
				printf("exit ");
			}
			else
			{
				printf("%u ", EDGE_DEST(psEdges)->uIdx);
			}
		}
		printf("\n");
	}
	printf("\n");
	#endif /* defined(UF_TESTBENCH) */
	
	//calculate postdominator of subtree
	for (psBlock->psExtPostDom = psBlock->psIPostDom;
		 psBlock->psExtPostDom && Dominates(psState, psBlock, psBlock->psExtPostDom);
		 psBlock->psExtPostDom = psBlock->psExtPostDom->psExtPostDom);
	
	/*
		Mark with sync_end any edges skipping SYNCs - if there are any to skip!
		Loop backedges leaving our tree are processed by their target (header)
		node, not here.
	*/
	if (uNumSyncStartChildren)
	{
		PCODEBLOCK psLastSyncStartChild = NULL;
		/*
			(1) Make every child postdominating a sync, into a sync-end point.
			(All such children, along with the SYNCs, will be in a single chain
			of postdominance because of the earlier restructuring.)
		*/
		for (i = 0; i < psBlock->uNumDomChildren; i++)
		{
			PCODEBLOCK psChild = psBlock->apsDomChildren[i];
			
			if (psLastSyncStartChild)
			{
				if (PostDominated(psState, psLastSyncStartChild, psChild))
				{
					IMG_UINT32 j;
					PCODEBLOCK psHeaders;
					/*
						psChild postdominates a SYNC --> mark with sync_end
						all edges to it that skipped the sync.
					*/
					for (j = 0; j <= i; j++) //edges must be from earlier children/edge sets
					{
						PEDGE_LIST psEdges;
						//skip edges on paths *through* the last sync
						if (j > 0 && psBlock->apsDomChildren[j - 1] == psLastSyncStartChild) continue;
						//j==0 --> edges from parent; j>0 --> from child j-1
						for (psEdges = apsChildLists[j]; psEdges; psEdges = psEdges->psNext)
						{
							if (EDGE_ISEXIT(psEdges)) continue;
							if (EDGE_DEST(psEdges) == psChild) SetSyncEnd(psState, psEdges);
						}
					}
					/*
						Presence of psChild implies there's a way to skip *around*
						psLastSyncStartChild (otherwise the latter would dominate psChild).
						Thus, if psLastSyncStartChild contains a sync_end point, we need
						to add a sync-start at its beginning, to ensure threads avoiding it
						get to psChild (in the sync_end state) *before* threads inside
						psLastSyncStartChild get to the sync_end point therein. (Otherwise
						we could end up with threads stuck forever in sync_end at the point
						inside psLastSyncStartChild.)
						(This corresponds to the "insertion of extra SYNCSTART's for nested
						flow control" in USC1.)
					*/
					if (psLastSyncStartChild->bDomSyncEnd
						/*avoid extra SYNCs if "nesting" is inside the *unconditional* part of a loop*/
						&& !PostDominated(psState,
										  psLastSyncStartChild,
										  psBlock/* would have to be loop header */))
					{
						psLastSyncStartChild->bAddSyncAtStart = IMG_TRUE;
					}
					/*
						Continuing along those lines - where a *loop* contains a sync_end, it
						needs a sync at the start, as every loop exit edge bypasses all the
						sync_end point(s) in the later iterations (which the loop exit edge has
						skipped).
					*/
					for (psHeaders = psChild->psLoopHeader; psHeaders; psHeaders = psHeaders->psLoopHeader)
						psHeaders->bAddSyncAtStart = IMG_TRUE;
					
					//record this made the child into a sync_end point dominated by psBlock
					psBlock->bDomSyncEnd = IMG_TRUE;
					
					psLastSyncStartChild = psChild;
				}
				else
				{
					ASSERT (!psChild->bDomSync);
				}
			}
			else if (psChild->bDomSync)
			{
				psLastSyncStartChild = psChild;
			}
		}
		
		/*
			(2) Mark with sync_end any edges which exit psBlock's subtree
			and skip SYNCs in later children
		*/
		for (i = 0; i <= psBlock->uNumDomChildren; i++)
		{
			PEDGE_LIST psEdges;
			for (psEdges = apsChildLists[i]; psEdges; psEdges = psEdges->psNext)
			{
				/*
					If the source to this edge is dominated by a SYNCSTART then
					ignore. Otherwise the edge skips over a SYNCSTART so set
					SYNCEND.
				*/
				if (i > 0 && psBlock->apsDomChildren[i - 1]->bDomSync) continue;

				if (EDGE_ISEXIT(psEdges)) continue;

				if (EDGE_DEST(psEdges) == psBlock && bLoopSync)
				{
					/*
						A 'continue' (back to header), skipping later children.
						Fall through & mark the edge itself with sync_end. Also
						mark block as dominating a sync_end - conceptually, the
						sync_end point is the start of the *next* iteration,
						which is dominated by the previous/first iteration; and
						any path *around* the whole loop needs to know it's
						bypassing a sync_end.
					*/
					psBlock->bDomSyncEnd = IMG_TRUE;
				}
				else if (EDGE_DEST(psEdges) == psBlock->psExtPostDom)
				{
					/*
						A 'break' out of a non-loop (but compound) statement,
						skipping later children. (Breaks out of loops are dealt
						with the next, loop-specific, step below). Fall through
						and mark the edge with sync-end.
					*/
				}
				else continue; //edge does not need marking
				//yup - continue or break; mark it.
				SetSyncEnd(psState, psEdges);
			}
		}
	}

	//Lastly - for loops containing syncs...
	if (bLoopSync)
	{
		PCODEBLOCK psBreakTarget;
		IMG_UINT32 uPred;
		/*
			Identify nearest postdominator of entire loop
			==
			the nearest postdominator of the header that's outside the loop.
		*/
		for (psBreakTarget = psBlock->psIPostDom;
			 LoopContains(psBlock, psBreakTarget);
			 psBreakTarget = psBreakTarget->psIPostDom);

		/*
			Set sync_end on *all* edges to break target. (These either bypass
			the loop entirely, or come from in the loop but nonetheless bypass
			subsequent iterations.)
		*/
		for (uPred = 0; uPred < psBreakTarget->uNumPreds; uPred++)
		{
			EDGE_LIST sTemp;
			sTemp.psSrc = psBreakTarget->asPreds[uPred].psDest;
			for (sTemp.uNum = 0; sTemp.uNum < sTemp.psSrc->uNumSuccs; sTemp.uNum++)
			{
				if (EDGE_DEST((&sTemp)) == psBreakTarget) SetSyncEnd(psState, &sTemp);
			}
		}
	}

	//compute return value
	{
		PEDGE_LIST psReturn = NULL;
		for (i = 0; i <= psBlock->uNumDomChildren; i++)
			psReturn = CombineEdgeLists(psState, psBlock, psReturn, apsChildLists[i]);
		UscFree(psState, apsChildLists);
		return psReturn;
	}
}

static
IMG_VOID SortDomChildren(PCFG psCfg)
/*****************************************************************************
 FUNCTION	: SortDomChildren

 PURPOSE	: For each block in a function sort the lists of dominated children so
			  edges to an enclosing loop are after all other edges.

 PARAMETERS	: psCfg				- CFG to sort.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uBlock;

	for (uBlock = 0; uBlock < psCfg->uNumBlocks; uBlock++)
	{
		IMG_UINT32	uChild;
		IMG_UINT32	uFirstChild;
		PCODEBLOCK	psBlock;

		psBlock = psCfg->apsAllBlocks[uBlock];
		uFirstChild = 0;
		for (uChild = 0; uChild < psBlock->uNumDomChildren; uChild++)
		{
			PCODEBLOCK psChild = psBlock->apsDomChildren[uChild];

			if (!BreakEdge(psBlock, psChild))
			{
				if (uChild != uFirstChild)
				{
					PCODEBLOCK	psTemp;

					psTemp = psBlock->apsDomChildren[uFirstChild];
					psBlock->apsDomChildren[uFirstChild] = psChild;
					psBlock->apsDomChildren[uChild] = psTemp;
				}

				uFirstChild++;
			}
		}
	}
}

//4. tying all together
 IMG_INTERNAL
 IMG_VOID SetupSyncStartInformation(PINTERMEDIATE_STATE psState)
/******************************************************************************
 FUNCTION	: SetupSyncStartInformation.

 PURPOSE	: Sets up all the logic necessary for arranging synchronization
			  between threads (for gradient samples), by encoding into the CFG
			  the necessary program flow and synchronization constructs.
			  Specifically, multiple SYNCSTARTs are serialized by restructuring
			  the CFG; edges are marked with sync_end; and blocks which
			  need extra SYNCSTARTs (to prevent threads getting stuck at
			  sync_end points bypassed by other threads) are so marked.

 PARAMETERS	: psState - Compiler intermediate state, contains program to modify
******************************************************************************/
{
	PFUNC psFunc;
	IMG_PBOOL abFuncsStatic;

	/*
		Sync Start only needed for Pixel shaders
	*/
	if	(psState->psSAOffsets->eShaderType != USC_SHADERTYPE_PIXEL)
	{
		return;
	}

	/*
		No need to set the sync-start flag if dynamic flow-control isn't used,
		since all pixels with have the same execution path.
	*/
	if	(psState->uNumDynamicBranches == 0)
	{
		return;
	}

	abFuncsStatic = UscAlloc(psState, psState->uMaxLabel * sizeof(IMG_BOOL));
	//defensively initialize. Every element should be written before read, tho.
	memset(abFuncsStatic, 0, psState->uMaxLabel * sizeof(IMG_BOOL));

	/* Innermost function first, so data available when calls are found */
	for (psFunc = psState->psFnInnermost; psFunc; psFunc = psFunc->psFnNestOuter)
	{
		IMG_BOOL bDoMerge;
		PEDGE_LIST psEdges;

		/*
			No need to set the sync-start flag for functions which never
			reach their exits and return a result
		*/
		if (psFunc->sCfg.psExit->uNumPreds == 0)
		{
			if (psFunc->sCfg.psExit != psFunc->sCfg.psEntry) continue; //exit unreachable
			
			/*
				a single-block function - so exit reachable, but no control flow.
				However, we may still need to scan the body, so that any CALLs
				to such a function know their target contains a sync...
			*/
			if (psFunc->psCallSiteHead == NULL) continue; //no CALLs -> no need
		}

		//1. scan function to find syncs & any restructurings needed.
		ASSERT (!psFunc->sCfg.bBlockStructureChanged);
		
		ComputeLoopNestingTree(psState, psFunc->sCfg.psEntry);
		SortDomChildren(&(psFunc->sCfg));

		abFuncsStatic[psFunc->uLabel] = 
			LocateSyncs(psState, psFunc->sCfg.psEntry,
						abFuncsStatic);

		for (;;)
		{
			bDoMerge = IMG_FALSE;
			psEdges = Restructure(psState, psFunc->sCfg.psEntry, &bDoMerge);

			// Only the edge to the exit can leave the dom tree of the whole function!
			if (psEdges != NULL)
			{
				ASSERT (EDGE_ISEXIT(psEdges));
				ASSERT (psEdges->psNext == NULL);
				UscFree(psState, psEdges);
			}

			//check if any extra blocks were introduced?
			if (!psFunc->sCfg.bBlockStructureChanged) break;
			MergeBasicBlocks(psState, psFunc);
			ComputeLoopNestingTree(psState, psFunc->sCfg.psEntry);
			SortDomChildren(&(psFunc->sCfg));
			TESTONLY_PRINT_INTERMEDIATE("Serialize multiple syncs", "serialsync", IMG_FALSE);
		}
	
		if (bDoMerge) //continue blocks removed. merge them in.
		{
			psFunc->sCfg.bBlockStructureChanged = IMG_TRUE;
			MergeBasicBlocks(psState, psFunc);
			//ComputeLoopNestingTree(psState, psFunc->psEntry); //no need, still up-to-date
			SortDomChildren(&(psFunc->sCfg));
		}
	
		/*
			Finally - we now have a CFG in which multiple siblings with SYNCs
			definitely postdominate each other. A single traversal of this
			suffices to set SYNCSTART and sync_end everywhere.
		*/
		psEdges = SetSyncStartEnd(psState, psFunc->sCfg.psEntry);

		// Only the edge to the exit can leave the dom tree of the whole function!
		ASSERT (EDGE_ISEXIT(psEdges));
		ASSERT (psEdges->psNext == NULL);
		UscFree(psState, psEdges);
	 }

	UscFree(psState, abFuncsStatic);
 }

/******************************************************************************
 End of file (pregalloc.c)
******************************************************************************/
