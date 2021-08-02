/******************************************************************************
 * Name         : reorder.c
 * Title        : Universal Shader Compiler
 * Created      : Nov 2005
 *
 * Copyright    : 2002-2008 by Imagination Technologies Limited.
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
 *
 * Modifications:-
 * $Log: reorder.c $
 *****************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "usedef.h"
#include <limits.h>

static IMG_INT32 CanFetchesBeCombined(PINST psInst1, PINST psInst2)
/*****************************************************************************
 FUNCTION	: CanFetchesBeCombined

 PURPOSE	: Check if two memory load instructions have compatible parameters
			  so they could be part of the same fetch.

 PARAMETERS	: psInst1				- Instruction to check.
			  psInst2

 RETURNS	: 0		if the two instructions are compatible.
			  -ve	if the first instruction is less than the second.
			  +ve	if the first instruction is greater than the second.
*****************************************************************************/
{
	IMG_INT32	iCmp;
	IMG_UINT32	uArg;

	/*
		Sort instructions in the same basic block together.
	*/
	iCmp = (IMG_INT32)(psInst1->psBlock->uGlobalIdx - psInst2->psBlock->uGlobalIdx);
	if (iCmp != 0)
	{
		return iCmp;
	}

	/*
		Compare the predicates controlling execution of the two instructions.
	*/
	iCmp = ComparePredicates(psInst1, psInst2);
	if (iCmp != 0)
	{
		return iCmp;
	}

	/*
		Compare the instruction parameters.
	*/
	if ((iCmp = BaseCompareInstParametersTypeLOADMEMCONST(psInst1, psInst2)) != 0)
	{
		return iCmp;
	}

	/*
		Compare the instruction source arguments (except the static offset).
	*/
	for (uArg = 0; uArg < psInst1->uArgumentCount; uArg++)
	{
		if (uArg != LOADMEMCONST_STATIC_OFFSET_ARGINDEX)
		{
			iCmp = CompareArgs(&psInst1->asArg[uArg], &psInst2->asArg[uArg]);
			if (iCmp != 0)
			{
				return iCmp;
			}
		}
	}

	return 0;
}

static IMG_UINT32 GetFetchStaticOffset(PINST psInst)
/*****************************************************************************
 FUNCTION	: GetFetchStaticOffset

 PURPOSE	: Get the static (compile-time known) part of the memory address 
			  loaded from by a LOADMEMCONST instruction.

 PARAMETERS	: psInst		- The LOADMEMCONST instruction.

 RETURNS	: The offset.
*****************************************************************************/
{
	PARG	psStaticOffset;

	psStaticOffset = &psInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX];
	return psStaticOffset->uNumber;
}

static IMG_INT32 CmpFetch(IMG_VOID const* ppvInst1, IMG_VOID const* ppvInst2)
/*****************************************************************************
 FUNCTION	: CmpFetch

 PURPOSE	: Defines the comparison order for qsort to use to sort the list of memory
			  load instructions.

 PARAMETERS	: ppvInst1, ppvInst2	- Pointers to pointers to the instructions
									to compare.

 RETURNS	: -ve	- if the first instruction is less than the second.
				0	- if the two instructions are equal.
			  +ve	- if the first instruction is greater than the second.
*****************************************************************************/
{
	const PINST* ppsInst1 = (PINST *)ppvInst1;
	const PINST* ppsInst2 = (PINST *)ppvInst2;

	const PINST psInst1 = *ppsInst1;
	const PINST psInst2 = *ppsInst2;
	IMG_INT32	iCmp;

	/*
		Compare instruction parameters (except the static offset).
	*/
	iCmp = CanFetchesBeCombined(psInst1, psInst2);
	if (iCmp != 0)
	{
		return iCmp;
	}

	/*	
		Sort by increasing static offset.
	*/
	iCmp = (IMG_INT32)(GetFetchStaticOffset(psInst1) - GetFetchStaticOffset(psInst2));
	if (iCmp != 0)
	{
		return iCmp;
	}

	/*
		Otherwise sort by position within the basic block.
	*/
	return (IMG_INT32)(psInst1->uBlockIndex - psInst2->uBlockIndex);
}

IMG_INTERNAL
IMG_VOID BuildFetchInstructions(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: BuildFetchInstructions

 PURPOSE	: Group together memory loads from consecutive addresses into
			  fetch instructions.

 PARAMETERS	: psState				- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uFetchInstCount;
	PINST*				apsFetches;
	IMG_UINT32			uInstIdx;
	SAFE_LIST_ITERATOR	sIter;

	/*
		Count the number of memory load instructions in the program.
	*/
	uFetchInstCount = 0;
	InstListIteratorInitialize(psState, ILOADMEMCONST, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psInst;

		psInst = InstListIteratorCurrent(&sIter);
		ASSERT(psInst->eOpcode == ILOADMEMCONST);

		uFetchInstCount++;
	}
	InstListIteratorFinalise(&sIter);

	/*
		Copy all the memory load instructions into a single array.
	*/
	apsFetches = UscAlloc(psState, sizeof(apsFetches[0]) * uFetchInstCount);
	uInstIdx = 0;
	InstListIteratorInitialize(psState, ILOADMEMCONST, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter), uInstIdx++)
	{
		PINST	psInst;

		psInst = InstListIteratorCurrent(&sIter);
		ASSERT(psInst->eOpcode == ILOADMEMCONST);

		apsFetches[uInstIdx] = psInst;
	}
	InstListIteratorFinalise(&sIter);
	ASSERT(uInstIdx == uFetchInstCount);
	
	/*
		Sort the memory load instructions so groups of instructions which could be made into
		a single fetch are together.
	*/
	qsort(apsFetches, uFetchInstCount, sizeof(apsFetches[0]), CmpFetch);

	for (uInstIdx = 0; uInstIdx < uFetchInstCount; uInstIdx++)
	{
		PINST		psBaseInst;
		IMG_UINT32	uPrimSize;
		IMG_UINT32	uLastStaticOffset;
		IMG_UINT32	uFetchCount;
		IMG_UINT32	uNextInstIdx;
		
		psBaseInst = apsFetches[uInstIdx];

		/*
			Skip instructions which have already been combined into an earlier fetch.
		*/
		if (psBaseInst == NULL)
		{
			continue;
		}

		/*
			Don't combine instructions referencing source data which can be defined more than once. 
		*/
		if (IsInstReferencingNonSSAData(psState, psBaseInst))
		{
			continue;
		}

		/*
			Check the hardware restrictions on the first memory load in a fetch.
		*/
		if (!CanStartFetch(psState, psBaseInst))
		{
			continue;
		}

		uPrimSize = psBaseInst->u.psLoadMemConst->uDataSize;

		uLastStaticOffset = GetFetchStaticOffset(psBaseInst);

		uFetchCount = 1;
		for (uNextInstIdx = uInstIdx + 1; uNextInstIdx < uFetchInstCount; uNextInstIdx++)
		{
			PINST		psNextInst = apsFetches[uNextInstIdx];
			IMG_UINT32	uNextInstStaticOffset;
			IMG_UINT32	uGap;

			if (psNextInst == NULL)
			{
				continue;
			}

			/*
				Check both instructions have the same parameters, are in the same block and are loading from addresses
				a compile-time known distance apart. 
			*/
			if (CanFetchesBeCombined(psBaseInst, psNextInst) != 0)
			{
				/* The list of fetches is sorted so all compatible instructions are together so we can stop looking now. */
				break;
			}
			
			uNextInstStaticOffset = GetFetchStaticOffset(psNextInst);

			/*
				We can't combine instructions loading from exactly the same address. But keep looking
				for an instruction loading from a larger address.
			*/
			if (uNextInstStaticOffset == uLastStaticOffset)
			{
				continue;
			}
			ASSERT(uNextInstStaticOffset > uLastStaticOffset);
			uGap = uNextInstStaticOffset - uLastStaticOffset;

			/*
				Check the difference in addresses is a multiple of the size of
				the data loaded by each fetch.
			*/
			if ((uGap % uPrimSize) != 0)
			{
				continue;
			}
			uGap /= uPrimSize;

			/*
				If the distance between the offsets is too large then stop the
				fetch here.
			*/	
			if (uGap > 2)
			{
				break;
			}

			/*
				If the distance is small then insert instructions loading unused data
				so we can make a large fetch.

				Don't create dummy fetches inside the secondary attribute program because
				we don't (at the moment) have access to temporary registers for their
				destinations.
			*/
			if (uGap == 2)
			{
				if (uFetchCount < (EURASIA_USE_MAXIMUM_REPEAT - 1) && (psBaseInst->psBlock->psOwner->psFunc != psState->psSecAttrProg))
				{
					SetDestCount(psState, psBaseInst, uFetchCount + 1);
					SetDest(psState,
							psBaseInst,
							uFetchCount,
							USEASM_REGTYPE_TEMP,
							GetNextRegister(psState),
							UF_REGFORMAT_F32);
					uFetchCount++;
				}
				else
				{
					break;
				}
			}

			/*	
				Grow the memory fetch instruction's array of destinations.
			*/
			SetDestCount(psState, psBaseInst, uFetchCount + 1);

			/*
				Move this memory load's destination to the end of the memory
				fetch instruction's array of destinations.
			*/
			MoveDest(psState, psBaseInst, uFetchCount, psNextInst, 0 /* uMoveFromDestIdx */);
			MovePartiallyWrittenDest(psState, psBaseInst, uFetchCount, psNextInst, 0 /* uMoveFromDestIdx */);
			psBaseInst->auLiveChansInDest[uFetchCount] = psNextInst->auLiveChansInDest[0];

			/*
				Remove this memory load from the block and free it.
			*/
			if (psNextInst->uBlockIndex < psBaseInst->uBlockIndex)
			{
				RemoveInst(psState, psBaseInst->psBlock, psBaseInst);
				InsertInstBefore(psState, psNextInst->psBlock, psBaseInst, psNextInst);
			}
			RemoveInst(psState, psNextInst->psBlock, psNextInst);
			FreeInst(psState, psNextInst);
			apsFetches[uNextInstIdx] = NULL;

			uFetchCount++;
	
			if (uFetchCount == EURASIA_USE_MAXIMUM_REPEAT)
			{
				break;
			}

			uLastStaticOffset = uNextInstStaticOffset;
		}

		SetBit(psBaseInst->auFlag, INST_FETCH, 1);
		psBaseInst->u.psLoadMemConst->uFetchCount = uFetchCount;
	}

	UscFree(psState, apsFetches);
	apsFetches = NULL;
}

typedef enum _REORDER_INST_TYPE_
{
	REORDER_INST_TYPE_NORMAL = 0,
	REORDER_INST_TYPE_FETCH = 1,
	REORDER_INST_TYPE_HIGH_LATNCY = 2,
	REORDER_INST_TYPE_UNKNOWN = 3
}REORDER_INST_TYPE, *PREORDER_INST_TYPE;

typedef struct _HIGH_LATNCY_INSTS_REORDER_STATE
{
	/*
		Cycles required by instructions to complete.
	*/
	IMG_PUINT32 auInstsCycles;
	PREORDER_INST_TYPE  aeReorderInstType;
	/*
		Cycle to start instructions.
	*/
	IMG_PUINT32	auStartInstAtCycle;
	IMG_PBOOL abInstScheduled;	
	PINST *apsHighLatncyInsts;
	/*
		Worst case cycles required to reach high latency instruction.
	*/
	IMG_PUINT32 auCyclesToHighLatncyInsts;
	IMG_UINT32 uHighLatncyInstsCount;
}HIGH_LATNCY_INSTS_REORDER_STATE, *PHIGH_LATNCY_INSTS_REORDER_STATE;

static IMG_BOOL IsHighLatncyNonMemLoadInst(PINTERMEDIATE_STATE psState, PINST psInst)
/*****************************************************************************
 FUNCTION	: IsHighLatncyNonMemLoadInst

 PURPOSE	: Tells if the specified instruction is High Latency non memory 
			  load instruction.

 PARAMETERS	: psState				- Compiler state.
			  psInst				- Instruction to test High Latency type.

 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	if((g_psInstDesc[psInst->eOpcode].uOptimizationGroup & USC_OPT_GROUP_HIGH_LATENCY) && (psInst->eOpcode != ILOADMEMCONST))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_VOID InitHighLatncyInstsReorderState(PINTERMEDIATE_STATE	psState, PHIGH_LATNCY_INSTS_REORDER_STATE psHighLatncyInstsReorderState, IMG_UINT32 uMaxBlockInsts)
/*****************************************************************************
 FUNCTION	: InitHighLatncyInstsReorderState

 PURPOSE	: Initialize High Latency Instruction Reordering state.

 PARAMETERS	: psState				- Compiler state.
			  uMaxBlockInsts		- Maximum possible instruction in a block.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uId;
	psHighLatncyInstsReorderState->auInstsCycles = UscAlloc(psState, (sizeof(psHighLatncyInstsReorderState->auInstsCycles[0]) * uMaxBlockInsts));
	psHighLatncyInstsReorderState->aeReorderInstType = UscAlloc(psState, (sizeof(psHighLatncyInstsReorderState->aeReorderInstType[0]) * uMaxBlockInsts));
	psHighLatncyInstsReorderState->auStartInstAtCycle = UscAlloc(psState, (sizeof(psHighLatncyInstsReorderState->auStartInstAtCycle[0]) * uMaxBlockInsts));
	psHighLatncyInstsReorderState->abInstScheduled = UscAlloc(psState, (sizeof(psHighLatncyInstsReorderState->abInstScheduled[0]) * uMaxBlockInsts));
	psHighLatncyInstsReorderState->apsHighLatncyInsts = UscAlloc(psState, (sizeof(psHighLatncyInstsReorderState->apsHighLatncyInsts[0]) * uMaxBlockInsts));
	psHighLatncyInstsReorderState->auCyclesToHighLatncyInsts = UscAlloc(psState, (sizeof(psHighLatncyInstsReorderState->auCyclesToHighLatncyInsts[0]) * uMaxBlockInsts));
	for(uId = 0; uId<uMaxBlockInsts; uId++)
	{
		psHighLatncyInstsReorderState->auInstsCycles[uId] = 0;
		psHighLatncyInstsReorderState->aeReorderInstType[uId] = REORDER_INST_TYPE_UNKNOWN;
		psHighLatncyInstsReorderState->auStartInstAtCycle[uId] = 0;
		psHighLatncyInstsReorderState->abInstScheduled[uId] = IMG_FALSE;
		psHighLatncyInstsReorderState->apsHighLatncyInsts[uId] = NULL;
		psHighLatncyInstsReorderState->auCyclesToHighLatncyInsts[uId] = 0;
	}
	psHighLatncyInstsReorderState->uHighLatncyInstsCount = 0;
	return;
}

static IMG_VOID FreeHighLatncyInstsReorderState(PINTERMEDIATE_STATE	psState, PHIGH_LATNCY_INSTS_REORDER_STATE psHighLatncyInstsReorderState)
/*****************************************************************************
 FUNCTION	: FreeHighLatncyInstsReorderState

 PURPOSE	: Free High Latency Instruction Reordering state.

 PARAMETERS	: psState				- Compiler state.
			  uMaxBlockInsts		- Maximum possible instruction in a block.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UscFree(psState, psHighLatncyInstsReorderState->auInstsCycles);
	psHighLatncyInstsReorderState->auInstsCycles = NULL;

	UscFree(psState, psHighLatncyInstsReorderState->aeReorderInstType);
	psHighLatncyInstsReorderState->aeReorderInstType = NULL;

	UscFree(psState, psHighLatncyInstsReorderState->auStartInstAtCycle);
	psHighLatncyInstsReorderState->auStartInstAtCycle = NULL;

	UscFree(psState, psHighLatncyInstsReorderState->abInstScheduled);
	psHighLatncyInstsReorderState->abInstScheduled = NULL;

	UscFree(psState, psHighLatncyInstsReorderState->apsHighLatncyInsts);
	psHighLatncyInstsReorderState->apsHighLatncyInsts = NULL;
	
	UscFree(psState, psHighLatncyInstsReorderState->auCyclesToHighLatncyInsts);
	psHighLatncyInstsReorderState->auCyclesToHighLatncyInsts = NULL;

	psHighLatncyInstsReorderState->uHighLatncyInstsCount = 0;
	return;
}

static IMG_VOID CalculateMaxCyclesToHighLatncyInsts(PINTERMEDIATE_STATE	psState, PDGRAPH_STATE psDepState, PHIGH_LATNCY_INSTS_REORDER_STATE psHighLatncyInstsReorderState)
/*****************************************************************************
 FUNCTION	: CalculateMaxCyclesToHighLatncyInsts

 PURPOSE	: Calculates maximum cycles to reach High Latency instruction.

 PARAMETERS	: psState				- Compiler state.
 			  psDepState			- Instuction dependency state.
 			  psHighLatncyInstsReorderState
			 						- High Latency Instuructions reordering 
									state.

 RETURNS	: Nothing.
*****************************************************************************/
{	
	IMG_UINT32 uInstId;
	/*	
		Find types of instructions and cycles required by instructions.
	*/
	for (
		uInstId = 0; 
		uInstId < (psDepState->uBlockInstructionCount); 
		uInstId++
	)
	{
		PINST psInst = (PINST)ArrayGet(psState, psDepState->psInstructions, uInstId);
		if(psInst->eOpcode == IDELTA)
		{
			psHighLatncyInstsReorderState->auInstsCycles[uInstId] = 0;
		}
		else if (IsHighLatncyNonMemLoadInst(psState, psInst))
		{
			psHighLatncyInstsReorderState->auInstsCycles[uInstId] = NON_MEM_LOAD_LATENCY_INST_LATENCY;
			#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
			if(psInst->eOpcode == IFNRM || psInst->eOpcode == IFNRMF16)
			{
				psHighLatncyInstsReorderState->aeReorderInstType[uInstId] = REORDER_INST_TYPE_FETCH;
			}
			else
			#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
			{
				psHighLatncyInstsReorderState->aeReorderInstType[uInstId] = REORDER_INST_TYPE_HIGH_LATNCY;
			}
		}
		else if (psInst->eOpcode == ILOADMEMCONST)
		{
			psHighLatncyInstsReorderState->aeReorderInstType[uInstId] = REORDER_INST_TYPE_FETCH;
			psHighLatncyInstsReorderState->auInstsCycles[uInstId] = NON_MEM_LOAD_LATENCY_INST_LATENCY + psInst->u.psLoadMemConst->uFetchCount;
		}
		else
		{
			psHighLatncyInstsReorderState->aeReorderInstType[uInstId] = REORDER_INST_TYPE_NORMAL;
			psHighLatncyInstsReorderState->auInstsCycles[uInstId] = 1;			
		}
		
		if (psState->uMaxInstMovement != USC_MAXINSTMOVEMENT_NOLIMIT)
		{
			psHighLatncyInstsReorderState->auStartInstAtCycle[uInstId] = uInstId - min(uInstId, psState->uMaxInstMovement);
		}
	}
	for (
		uInstId = 0; 
		uInstId < (psDepState->uBlockInstructionCount); 
		uInstId++
	)
	{		
		if((psHighLatncyInstsReorderState->aeReorderInstType[uInstId] == REORDER_INST_TYPE_HIGH_LATNCY) || (psHighLatncyInstsReorderState->aeReorderInstType[uInstId] == REORDER_INST_TYPE_FETCH))
		{			
			IMG_UINT32 uCyclesToHighLatncyInst = 0;
			USC_PVECTOR	psInstParentsVec = NULL;
			GraphColRef(psState, psDepState->psClosedDepGraph, uInstId, &psInstParentsVec);
			if(psInstParentsVec != NULL)
			{
				IMG_UINT32 uParentInstId;
				for(uParentInstId = 0; uParentInstId < psDepState->uBlockInstructionCount; uParentInstId++)
				{
					if(VectorGet(psState, psInstParentsVec, uParentInstId))
					{
						uCyclesToHighLatncyInst += psHighLatncyInstsReorderState->auInstsCycles[uParentInstId];
					}
				}
			}
			{
				PINST psInst;
				psHighLatncyInstsReorderState->auCyclesToHighLatncyInsts[psHighLatncyInstsReorderState->uHighLatncyInstsCount] = uCyclesToHighLatncyInst;
				psInst = (PINST)ArrayGet(psState, psDepState->psInstructions, uInstId);
				psHighLatncyInstsReorderState->apsHighLatncyInsts[psHighLatncyInstsReorderState->uHighLatncyInstsCount] = psInst;
				psHighLatncyInstsReorderState->uHighLatncyInstsCount++;
			}
		}
	}
	return;
}

static IMG_UINT32 MinCyclsToHighLatncyInst(PINTERMEDIATE_STATE psState, PDGRAPH_STATE psDepState, PHIGH_LATNCY_INSTS_REORDER_STATE psHighLatncyInstsReorderState, IMG_UINT32 uParentInstId)
/*****************************************************************************
 FUNCTION	: MinCyclsToHighLatncyInst

 PURPOSE	: Find minimum cycles required to reach a High Latency 
			  instruction which is also children of specified instruction.

 PARAMETERS	: psState				- Compiler state.
 			  psDepState			- Instuction dependency state.
 			  psHighLatncyInstsReorderState
			 						- High Latency Instuructions reordering 
									state.
			  uParentInstId			- Id of instruction whose High Latency child 
									at minimum cycles to find.

 RETURNS	: Minimum cycles required to reach High Latency instruction 
			  which is also child of specified instruction.
*****************************************************************************/
{
	IMG_UINT32 uHighLatncyInstIdx;
	IMG_UINT32 uMinCyclesToHighLatncyInst = UINT_MAX - 1;
	for (
		uHighLatncyInstIdx = 0; 
		uHighLatncyInstIdx < (psHighLatncyInstsReorderState->uHighLatncyInstsCount); 
		uHighLatncyInstIdx++
	)
	{
		if (!psHighLatncyInstsReorderState->abInstScheduled[psHighLatncyInstsReorderState->apsHighLatncyInsts[uHighLatncyInstIdx]->uId])
		{
			if (psHighLatncyInstsReorderState->apsHighLatncyInsts[uHighLatncyInstIdx]->uId == uParentInstId)
			{
				uMinCyclesToHighLatncyInst = psHighLatncyInstsReorderState->auCyclesToHighLatncyInsts[uHighLatncyInstIdx];
			}
			else if (GraphGet(psState, psDepState->psClosedDepGraph, psHighLatncyInstsReorderState->apsHighLatncyInsts[uHighLatncyInstIdx]->uId, uParentInstId))
			{
				if(uMinCyclesToHighLatncyInst > psHighLatncyInstsReorderState->auCyclesToHighLatncyInsts[uHighLatncyInstIdx])
				{
					uMinCyclesToHighLatncyInst = psHighLatncyInstsReorderState->auCyclesToHighLatncyInsts[uHighLatncyInstIdx];
				}
			}
		}
	}
	return uMinCyclesToHighLatncyInst;
}

static IMG_VOID RemoveCyclesFromAllHighLatncyChildren(PINTERMEDIATE_STATE psState, PDGRAPH_STATE psDepState, PHIGH_LATNCY_INSTS_REORDER_STATE psHighLatncyInstsReorderState, IMG_UINT32 uParentInstId, IMG_UINT32 uCyclesToRemove)
/*****************************************************************************
 FUNCTION	: RemoveCyclesFromAllHighLatncyChildren

 PURPOSE	: Removes specified cycles from all High Latency children of 
			  specified instruction.

 PARAMETERS	: psState				- Compiler state.
 			  psDepState			- Instuction dependency state.
 			  psHighLatncyInstsReorderState
			 						- High Latency Instuructions reordering 
									state.
			  uParentInstId			- Id of instruction whose children cycles 
									to be reduced.
			  uCyclesToRemove		- Count of cycles to reduce.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32 uHighLatncyInstIdx;
	for (
		uHighLatncyInstIdx = 0; 
		uHighLatncyInstIdx < (psHighLatncyInstsReorderState->uHighLatncyInstsCount); 
		uHighLatncyInstIdx++
	)
	{
		if(!psHighLatncyInstsReorderState->abInstScheduled[psHighLatncyInstsReorderState->apsHighLatncyInsts[uHighLatncyInstIdx]->uId])
		{
			if(GraphGet(psState, psDepState->psClosedDepGraph, psHighLatncyInstsReorderState->apsHighLatncyInsts[uHighLatncyInstIdx]->uId, uParentInstId))
			{
				psHighLatncyInstsReorderState->auCyclesToHighLatncyInsts[uHighLatncyInstIdx] -= uCyclesToRemove;
			}
		}
	}
}

static IMG_VOID ReorderHighLatncyInsts(PINTERMEDIATE_STATE psState, PDGRAPH_STATE psDepState, PHIGH_LATNCY_INSTS_REORDER_STATE psHighLatncyInstsReorderState, PCODEBLOCK psBlock)
/*****************************************************************************
 FUNCTION	: ReorderHighLatncyInsts

 PURPOSE	: Reorders High Latency instructions in a block.

 PARAMETERS	: psState				- Compiler state.
 			  psDepState			- Instuction dependency state.
 			  psHighLatncyInstsReorderState
			 						- High Latency Instuructions reordering 
									state.
			  psBlock				- Code block whose instructions will be 
			  						reordered.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST psInst;
	IMG_UINT32 uCurrentCycle = 0;
	IMG_UINT32 uCurrentHighLatncyInstsCount = 0;
	IMG_BOOL bCurrentFetchInst = IMG_FALSE;
	PINST *apsScheduledHighLatncyInsts = UscAlloc(psState, (sizeof(apsScheduledHighLatncyInsts[0]) * psDepState->uBlockInstructionCount));
	IMG_UINT32 uScheduledHighLatncyInstsCount = 0;
	/*
		Scheduled instructions cycles left to complete.
	*/
	IMG_UINT32 uScheduledInstsCyclsLeft = 0;
	IMG_UINT32 uCurrInstCycles = 0;
	IMG_UINT32 uLastFetchEndCycle = 0;
	IMG_UINT32 uFetchPipeLineCyclesLeft = 0;
	psInst = GetNextAvailable(psDepState, NULL);
	while(psInst != NULL)
	{
		IMG_UINT32 uClosestInstCycle = UINT_MAX - 1;
		PINST psHigstPriorityInstAtClosestCycle = NULL;
		IMG_UINT32 uMinCyclsToHighLatncyInst = UINT_MAX - 1;
		/*
			Instruction at cycle closest to current cycle.
		*/
		for (; 
			 psInst != NULL; 
			 psInst = GetNextAvailable(psDepState, psInst))
		{
			IMG_UINT32 uInstStartCycle = psHighLatncyInstsReorderState->auStartInstAtCycle[psInst->uId];
			if (psHighLatncyInstsReorderState->aeReorderInstType[psInst->uId] == REORDER_INST_TYPE_FETCH)
			{
				uInstStartCycle = max(uInstStartCycle, uLastFetchEndCycle);
			}
			if((uClosestInstCycle > uInstStartCycle) && (uClosestInstCycle > uCurrentCycle))
			{
				uClosestInstCycle = uInstStartCycle;
				psHigstPriorityInstAtClosestCycle = psInst;
				uMinCyclsToHighLatncyInst = MinCyclsToHighLatncyInst(psState, psDepState, psHighLatncyInstsReorderState, psInst->uId);
			}
			else if ((uClosestInstCycle == uInstStartCycle) || (uInstStartCycle <= uCurrentCycle))
			{	
				IMG_UINT32 uMinCyclsToHighLatncyInstCurr = MinCyclsToHighLatncyInst(psState, psDepState, psHighLatncyInstsReorderState, psInst->uId);
				if(uMinCyclsToHighLatncyInstCurr <  uMinCyclsToHighLatncyInst)
				{
					uMinCyclsToHighLatncyInst = uMinCyclsToHighLatncyInstCurr;
					uClosestInstCycle = uInstStartCycle;
					psHigstPriorityInstAtClosestCycle = psInst;
				}
			}
		}
		if (psHighLatncyInstsReorderState->aeReorderInstType[psHigstPriorityInstAtClosestCycle->uId] == REORDER_INST_TYPE_FETCH)
		{
			psHighLatncyInstsReorderState->auStartInstAtCycle[psHigstPriorityInstAtClosestCycle->uId] = max(psHighLatncyInstsReorderState->auStartInstAtCycle[psHigstPriorityInstAtClosestCycle->uId], uLastFetchEndCycle);
		}
		ASSERT(psHigstPriorityInstAtClosestCycle != NULL);
		psHighLatncyInstsReorderState->abInstScheduled[psHigstPriorityInstAtClosestCycle->uId] = IMG_TRUE;
		{
			/*
				Check if current instruction is child of some high latency instruction recently scheduled.
			*/
			IMG_UINT32 uScheduledHighLatncyInstIdx;
			for(uScheduledHighLatncyInstIdx = 0; uScheduledHighLatncyInstIdx < uScheduledHighLatncyInstsCount; uScheduledHighLatncyInstIdx++)
			{	
				IMG_UINT32	uHighLatencyInstId = apsScheduledHighLatncyInsts[uScheduledHighLatncyInstIdx]->uId;

				if(psHighLatncyInstsReorderState->auInstsCycles[uHighLatencyInstId] > 0)
				{
					/*
						Scheduled high latency instrutions still have cycles to be completed.
					*/
					if(GraphGet(psState, psDepState->psClosedDepGraph, psHigstPriorityInstAtClosestCycle->uId, uHighLatencyInstId))
					{
						RemoveCyclesFromAllHighLatncyChildren(psState, 
															  psDepState, 
															  psHighLatncyInstsReorderState, 
															  uHighLatencyInstId, 
															  psHighLatncyInstsReorderState->auInstsCycles[uHighLatencyInstId]);
						psHighLatncyInstsReorderState->auInstsCycles[uHighLatencyInstId] = 0;
					}
				}
			}
		}
		{
			uCurrInstCycles = psHighLatncyInstsReorderState->auInstsCycles[psHigstPriorityInstAtClosestCycle->uId];
			if(psHighLatncyInstsReorderState->aeReorderInstType[psHigstPriorityInstAtClosestCycle->uId] == REORDER_INST_TYPE_FETCH)
			{
				uCurrInstCycles += uFetchPipeLineCyclesLeft;
			}
			if (uCurrInstCycles == 1)
			{
				if(uScheduledInstsCyclsLeft > 0)
				{
					uScheduledInstsCyclsLeft--;
				}
			}
			else
			{
				if(uCurrInstCycles > uScheduledInstsCyclsLeft)
				{
					uScheduledInstsCyclsLeft = uCurrInstCycles - 1;
				}
				else
				{
					if(uCurrInstCycles > 0)
					{
						uScheduledInstsCyclsLeft--;
					}
				}
			}
		}
		{
			
			IMG_UINT32 uDepInstId;
			PADJACENCY_LIST	psList;
			ADJACENCY_LIST_ITERATOR sIterState;
			IMG_BOOL bCurrInstHighLatncy = IMG_FALSE;
			if(psHighLatncyInstsReorderState->aeReorderInstType[psHigstPriorityInstAtClosestCycle->uId] == REORDER_INST_TYPE_HIGH_LATNCY)
			{
				bCurrInstHighLatncy = IMG_TRUE;
				uCurrentHighLatncyInstsCount++;
				if(
					(uCurrentHighLatncyInstsCount == (EURASIA_USE_DRC_BANK_SIZE * EURASIA_USE_DRC_MAXCOUNT)) || 
					((uCurrentHighLatncyInstsCount == ((EURASIA_USE_DRC_BANK_SIZE * EURASIA_USE_DRC_MAXCOUNT)-1)) && bCurrentFetchInst)
				)
				{
					/*
						Increment the start cycles of all the Following High Latncy 
						instructions by current instruction's latency.
					*/
					IMG_UINT32 uInstId;
					for (uInstId = 0; uInstId < psDepState->uBlockInstructionCount; uInstId++)
					{
						if (!psHighLatncyInstsReorderState->abInstScheduled[uInstId])
						{
							if ((psHighLatncyInstsReorderState->aeReorderInstType[uInstId] == REORDER_INST_TYPE_HIGH_LATNCY) || (psHighLatncyInstsReorderState->aeReorderInstType[uInstId] == REORDER_INST_TYPE_FETCH))
							{
								psHighLatncyInstsReorderState->auStartInstAtCycle[uInstId] = max(psHighLatncyInstsReorderState->auStartInstAtCycle[uInstId], uCurrentCycle + uScheduledInstsCyclsLeft);
							}
						}
					}
					uCurrentHighLatncyInstsCount = 0;
					bCurrentFetchInst = IMG_FALSE;
				}
			}
			else if (psHighLatncyInstsReorderState->aeReorderInstType[psHigstPriorityInstAtClosestCycle->uId] == REORDER_INST_TYPE_FETCH)
			{
				bCurrInstHighLatncy = IMG_TRUE;				
				/*
					Increment the start cycles of all the Following High Latncy 
					instructions by current instruction's latency.
				*/
				if (uCurrentHighLatncyInstsCount == ((EURASIA_USE_DRC_BANK_SIZE * EURASIA_USE_DRC_MAXCOUNT)-1) || bCurrentFetchInst)
				{
					IMG_UINT32 uInstId;
					for (uInstId = 0; uInstId < psDepState->uBlockInstructionCount; uInstId++)
					{
						if (!psHighLatncyInstsReorderState->abInstScheduled[uInstId])
						{
							if (psHighLatncyInstsReorderState->aeReorderInstType[uInstId] == REORDER_INST_TYPE_HIGH_LATNCY || psHighLatncyInstsReorderState->aeReorderInstType[uInstId] == REORDER_INST_TYPE_FETCH)
							{
								psHighLatncyInstsReorderState->auStartInstAtCycle[uInstId] = max(psHighLatncyInstsReorderState->auStartInstAtCycle[uInstId], uCurrentCycle + uScheduledInstsCyclsLeft);
							}
						}
					}					
					uCurrentHighLatncyInstsCount = 0;
					bCurrentFetchInst = IMG_FALSE;
				}
				else
				{
					bCurrentFetchInst = IMG_TRUE;
				}
			}
			psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, psHigstPriorityInstAtClosestCycle->uId);
			for (
				uDepInstId = FirstAdjacent(psList, &sIterState); 
				!IsLastAdjacent(&sIterState); 
				uDepInstId = NextAdjacent(&sIterState)
			)
			{
				/* 
					Add latency of current instruction to all its children.
				*/
				psHighLatncyInstsReorderState->auStartInstAtCycle[uDepInstId] = max(psHighLatncyInstsReorderState->auStartInstAtCycle[uDepInstId], uCurrentCycle + uCurrInstCycles);
			}
			if(uCurrInstCycles > 0)
			{
				RemoveCyclesFromAllHighLatncyChildren(psState, psDepState, psHighLatncyInstsReorderState, psHigstPriorityInstAtClosestCycle->uId, 1);
			}
			psHighLatncyInstsReorderState->auInstsCycles[psHigstPriorityInstAtClosestCycle->uId]--;
			if(bCurrInstHighLatncy)
			{
				apsScheduledHighLatncyInsts[uScheduledHighLatncyInstsCount] = psHigstPriorityInstAtClosestCycle;
				uScheduledHighLatncyInstsCount++;
			}
		}
		RemoveInstruction(psDepState, psHigstPriorityInstAtClosestCycle);
		if (psHighLatncyInstsReorderState->aeReorderInstType[psHigstPriorityInstAtClosestCycle->uId] == REORDER_INST_TYPE_FETCH)
		{
			if((uCurrInstCycles - NON_MEM_LOAD_LATENCY_INST_LATENCY) > 1)
			{
				uFetchPipeLineCyclesLeft = uFetchPipeLineCyclesLeft + ((uCurrInstCycles - NON_MEM_LOAD_LATENCY_INST_LATENCY) - 1);
			}
			uLastFetchEndCycle = (uCurrentCycle + uCurrInstCycles) - 1;
		}
		else
		{
			if(uCurrInstCycles > 0)
			{
				if(uFetchPipeLineCyclesLeft > 0)
				{
					uFetchPipeLineCyclesLeft--;
				}
			}
			if(bCurrentFetchInst)
			{
				if(uCurrInstCycles > 0)
				{
					uLastFetchEndCycle = 0;
				}
			}
		}
		{
			IMG_UINT32	uOldCurrentCycle = uCurrentCycle;
			if(uCurrentCycle < psHighLatncyInstsReorderState->auStartInstAtCycle[psHigstPriorityInstAtClosestCycle->uId])
			{
				uCurrentCycle = psHighLatncyInstsReorderState->auStartInstAtCycle[psHigstPriorityInstAtClosestCycle->uId];
			}
			psHighLatncyInstsReorderState->auStartInstAtCycle[psHigstPriorityInstAtClosestCycle->uId] = uOldCurrentCycle;
		}
		if(uCurrInstCycles > 0)
		{
			uCurrentCycle++;
		}
		InsertInstBefore(psState, psBlock, psHigstPriorityInstAtClosestCycle, NULL);
		psInst = GetNextAvailable(psDepState, NULL);
	}
	UscFree(psState, apsScheduledHighLatncyInsts);
	return;
}

IMG_INTERNAL 
IMG_VOID ReorderHighLatncyInstsBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvNull)
/*****************************************************************************
 FUNCTION	: ReorderInstructionsBP

 PURPOSE	: Reorder the instructions in a block for best performance.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to reorder.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PDGRAPH_STATE psDepState;	
	HIGH_LATNCY_INSTS_REORDER_STATE sHighLatncyInstsReorderState;
	PVR_UNREFERENCED_PARAMETER(pvNull);

	if( !TestInstructionUsage(psState, ILOADMEMCONST) && !TestBlockForInstGroup(psBlock, USC_OPT_GROUP_HIGH_LATENCY))
	{
		return;
	}

	InitHighLatncyInstsReorderState(psState, &sHighLatncyInstsReorderState, psBlock->uInstCount);
	psDepState = ComputeBlockDependencyGraph(psState, psBlock, IMG_FALSE);
	ComputeClosedDependencyGraph(psDepState, IMG_FALSE);
	ClearBlockInsts(psState, psBlock);
	//PrintDependencyGraph(psDepState, IMG_TRUE);	
	CalculateMaxCyclesToHighLatncyInsts(psState, psDepState, &sHighLatncyInstsReorderState);
	ReorderHighLatncyInsts(psState, psDepState, &sHighLatncyInstsReorderState, psBlock);
	FreeHighLatncyInstsReorderState(psState, &sHighLatncyInstsReorderState);
	FreeBlockDGraphState(psState, psBlock);
	//PrintDependencyGraph(psDepState, IMG_TRUE);
}

static IMG_BOOL IsFBDependency(PINTERMEDIATE_STATE psState, PDGRAPH_STATE psDepState, IMG_PUINT32 puFBDependency, IMG_UINT32 uInst)
/*****************************************************************************
 FUNCTION	: IsFBDependency
    
 PURPOSE	: Checks if an instruction is used in the calculation of the
			  results to feedback.

 PARAMETERS	: psState			- Compiler state.
			  psDepGraph		- Dependency graph for the block containing the
								feedback calculation.
			  puFBDependency	- Bit array with a set entry for each instruction
								known to be required for feedback.
			  uInst				- Instruction to check.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uPrevInst;

	for (uPrevInst = uInst + 1; uPrevInst < psDepState->uBlockInstructionCount; uPrevInst++)
	{
		/*
			Check for an indirect dependency on the data required for
			punchthrough feedback.
		*/
		if (GraphGet(psState, psDepState->psDepGraph, uPrevInst, uInst) && 
			GetBit(puFBDependency, uPrevInst))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_VOID SetFBDependency(IMG_UINT32 auFBDependency[], IMG_PUINT32 puFBInstCount, IMG_UINT32 uInstId)
/*****************************************************************************
 FUNCTION	: SetFBDependency
    
 PURPOSE	: Flags an instruction as used in the calculation of a shader output
			  needed for feedback.

 PARAMETERS	: puFBDependency	- Bit array with a set entry for each instruction
								known to be required for feedback.
			  puFBInstCount		- Count of instructions required for feedback.
			  uInstId			- ID of the instruction to flag.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (!GetBit(auFBDependency, uInstId))
	{
		SetBit(auFBDependency, uInstId, 1);
		(*puFBInstCount)++;
	}
}

#define MAX_FEEDBACK_RESULTS				(5)

typedef struct _FEEDBACK_RESULT
{
	/* Register which is read by the driver generated feedback code. */
	PFIXED_REG_DATA		psFixedReg;
	IMG_UINT32			uFixedRegOffset;
	/* Mask of channels from the register which are used */
	IMG_UINT32			uMask;
	IMG_UINT32			uPartialResultRegNum;
	PINST				psDefInst;
	/*
		TRUE if this shader result is used in pre-feedback driver epilog only (not
		in the main epilog).
	*/
	IMG_BOOL			bFBOnly;
} FEEDBACK_RESULT, *PFEEDBACK_RESULT;

typedef struct _FEEDBACK_RESULTS
{
	IMG_UINT32			uCount;
	FEEDBACK_RESULT		asResults[MAX_FEEDBACK_RESULTS];
} FEEDBACK_RESULTS, *PFEEDBACK_RESULTS;

static IMG_VOID AddToNeededForFeedbackSet(PINTERMEDIATE_STATE	psState,
										  PFEEDBACK_RESULT		psResult)
/*****************************************************************************
 FUNCTION	: AddToNeededForFeedbackSet
    
 PURPOSE	: Record the instruction which defines a shader result needed for
			  feedback.

 PARAMETERS	: psState				- Compiler state.
			  psResult				- Details of the shader result.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN		psResultUseDef;
	IMG_UINT32			uResultRegType = psResult->psFixedReg->uVRegType;
	IMG_UINT32			uResultRegNumber = psResult->psFixedReg->auVRegNum[psResult->uFixedRegOffset];
	
	/*
		For a register where only a subset of the channels are required for feedback ignore any
		partial definitions. 
	*/
	psResult->psDefInst = NULL;
	for (;;)
	{
		PINST			psResultDefInst;
		IMG_UINT32		uResultDefIdx;
		PARG			psResultOldDest;
		PUSEDEF			psResultDef;

		psResultUseDef = UseDefGet(psState, uResultRegType, uResultRegNumber);

		ASSERT(psResultUseDef->psDef != NULL);
		psResultDef = psResultUseDef->psDef;
		ASSERT(UseDefIsDef(psResultDef));

		if (psResultDef->eType != DEF_TYPE_INST)
		{
			break;
		}
		psResultDefInst = psResultDef->u.psInst;
		uResultDefIdx = psResultDef->uLocation;
		ASSERT(uResultDefIdx < psResultDefInst->uDestCount);

		/*
			Check if the instruction defines the channels required for feedback.
		*/
		if ((psResultDefInst->auDestMask[uResultDefIdx] & psResult->uMask) != 0)
		{
			psResult->psDefInst = psResultDefInst;
			break;
		}
		psResultOldDest = psResultDefInst->apsOldDest[uResultDefIdx];
		if (psResultOldDest == NULL || psResultOldDest->uType != uResultRegType)
		{
			psResult->psDefInst = psResultDefInst;
			break;
		}

		/*
			Look at the definition of the old value of the partially updated
			destination.
		*/
		uResultRegNumber = psResultOldDest->uNumber;
	}

	/*
		If we ignoring some parital definitions then record the register number where the channels
		we are interested in was defined.
	*/
	psResult->uPartialResultRegNum = uResultRegNumber;
}	


static IMG_VOID GetResultsNeededForFeedback(PINTERMEDIATE_STATE	psState,
											PFEEDBACK_RESULTS	psResults)
/*****************************************************************************
 FUNCTION	: GetResultsNeededForFeedback
    
 PURPOSE	: Get the results needed for feedback.

 PARAMETERS	: psState				- Compiler state.
			  psResults				- List of the shader results which are inputs to the
									pre-feedback driver epilog.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PFEEDBACK_RESULT	psResult;
	IMG_UINT32			uResultIdx;

	psResults->uCount = 0;

	#if defined(SUPPORT_SGX545)
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX)
	{
		IMG_UINT32			uIdx;
		PFIXED_REG_DATA		psVSOutput;
		PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;

		ASSERT(psVS->psVertexShaderOutputsFixedReg != NULL);
		psVSOutput = psVS->psVertexShaderOutputsFixedReg;

		/*
			All component of the position are needed for feedback.
		*/
		for (uIdx = 0; uIdx < 4; uIdx++)
		{
			ASSERT(uIdx < psVSOutput->uConsecutiveRegsCount);

			psResult = &psResults->asResults[psResults->uCount++];

			psResult->psFixedReg = psVSOutput;
			psResult->uFixedRegOffset = uIdx;
			psResult->uMask = USC_ALL_CHAN_MASK;
			psResult->bFBOnly = IMG_FALSE;
		}

		/*
			Convert the offset of the point-size output register to an offset into the
			packed output buffer.
		*/
		if (psState->psSAOffsets->uVSPointSizeOutputNum != UINT_MAX)
		{
			if (GetBit(psState->psSAOffsets->puValidShaderOutputs, psState->psSAOffsets->uVSPointSizeOutputNum))
			{
				IMG_UINT32	uPointSizeOutputNum;
				IMG_UINT32	uOutputIdx;

				uPointSizeOutputNum = 0;
				for (uOutputIdx = 0; uOutputIdx < psState->psSAOffsets->uVSPointSizeOutputNum; uOutputIdx++)
				{
					if (GetBit(psState->puPackedShaderOutputs, uOutputIdx))
					{
						uPointSizeOutputNum++;
					}
				}
				ASSERT(uPointSizeOutputNum < psVSOutput->uConsecutiveRegsCount);

				/*
					The pointsize output is need for feedback.
				*/
				psResult = &psResults->asResults[psResults->uCount++];

				psResult->psFixedReg = psVSOutput;
				psResult->uFixedRegOffset = uPointSizeOutputNum;
				psResult->uMask = USC_ALL_CHAN_MASK;
				psResult->bFBOnly = IMG_FALSE;
			}
		}
	}
	else
	#else /* defined(SUPPORT_SGX545) */
	ASSERT((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL));	
	#endif /* defined(SUPPORT_SGX545) */
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

		if ((psState->uCompilerFlags & UF_NOALPHATEST) == 0)
		{
			IMG_UINT32	uOutputMask;

			if (psState->psSAOffsets->ePackDestFormat == UF_REGFORMAT_U8)
			{
				uOutputMask = 1 << USC_ALPHA_CHAN;
			}
			else
			{
				uOutputMask = USC_DESTMASK_FULL;
			}

			psResult = &psResults->asResults[psResults->uCount++];

			psResult->psFixedReg = psPS->psColFixedReg;
			psResult->uFixedRegOffset = psPS->uAlphaOutputOffset;
			psResult->uMask = uOutputMask;
			psResult->bFBOnly = IMG_FALSE;
		}

		if (psState->uFlags & USC_FLAGS_TEXKILL_PRESENT)
		{
			psResult = &psResults->asResults[psResults->uCount++];

			psResult->psFixedReg = psPS->psTexkillOutput;
			psResult->uFixedRegOffset = 0;
			psResult->uMask = USC_ALL_CHAN_MASK;
			psResult->bFBOnly = IMG_TRUE;
		}
		if (psState->uFlags & USC_FLAGS_DEPTHFEEDBACKPRESENT)
		{
			psResult = &psResults->asResults[psResults->uCount++];

			psResult->psFixedReg = psPS->psDepthOutput;
			psResult->uFixedRegOffset = 0;
			psResult->uMask = USC_ALL_CHAN_MASK;
			psResult->bFBOnly = IMG_TRUE;
		}
		if ((psState->uFlags & USC_FLAGS_OMASKFEEDBACKPRESENT) != 0)
		{
			psResult = &psResults->asResults[psResults->uCount++];

			psResult->psFixedReg = psPS->psOMaskOutput;
			psResult->uFixedRegOffset = 0;
			psResult->uMask = USC_ALL_CHAN_MASK;
			psResult->bFBOnly = IMG_TRUE;
		}
	}

	ASSERT(psResults->uCount <= (sizeof(psResults->asResults) / sizeof(psResults->asResults[0])));

	/*
		Find the instruction which defines each feedback result.
	*/
	for (uResultIdx = 0; uResultIdx < psResults->uCount; uResultIdx++)
	{
		AddToNeededForFeedbackSet(psState, &psResults->asResults[uResultIdx]);
	}
}

static PCODEBLOCK GetFeedbackSplitBlock(PINTERMEDIATE_STATE psState, PFEEDBACK_RESULTS psResults)
/*****************************************************************************
 FUNCTION	: GetFeedbackSplitBlock
    
 PURPOSE	: Get a pointer to a block which (non-strictly) post-dominates the
			  blocks which contain all of the instructions defining a shader
			  result.

 PARAMETERS	: psState				- Compiler state.
			  psResults				- List of the shader results which are inputs to the
									pre-feedback driver epilog.
			  
 RETURNS	: A pointer to the block.
*****************************************************************************/
{
	PCODEBLOCK	psSplitBlock;
	IMG_UINT32	uResultIdx;

	psSplitBlock = psState->psMainProg->sCfg.psEntry;
	for (uResultIdx = 0; uResultIdx < psResults->uCount; uResultIdx++)
	{
		PCODEBLOCK			psResultDefBlock;
		PFEEDBACK_RESULT	psResult = &psResults->asResults[uResultIdx];
		PINST				psDefInst = psResult->psDefInst;

		if (psDefInst == NULL)
		{
			continue;
		}
		psResultDefBlock = psDefInst->psBlock;
		if (psResultDefBlock->psOwner->psFunc == psState->psSecAttrProg)
		{
			/*
				Results written in the secondary update program are available at the start of
				the main program.
			*/
			continue;
		}
		
		ASSERT(psResultDefBlock->psOwner->psFunc == psState->psMainProg);
		psSplitBlock = FindLeastCommonDominator(psState, psSplitBlock, psResultDefBlock, IMG_TRUE /* bPostDom */);
	}

	/*
		If the split point is part of a loop then find a block which post-dominates the entire loop.
	*/
	while (LoopDepth(psSplitBlock) != 0)
	{
		psSplitBlock = psSplitBlock->psIPostDom;
	}

	return psSplitBlock;
}

static IMG_VOID GetInstsNeededForFeedback(PCODEBLOCK			psSplitBlock,
										  PFEEDBACK_RESULTS		psResults,
										  IMG_UINT32			auFBDependency[],
										  IMG_PUINT32			puFBInstCount)
/*****************************************************************************
 FUNCTION	: GetInstsNeededForFeedback
    
 PURPOSE	: Get the instructions whose results are directly used for feedback.

 PARAMETERS	: psSplitBlock			- Block we are trying to split for feedback.
			  psResults				- List of the shader results which are inputs to the
									pre-feedback driver epilog.
			  auFBDependency		- Bit array. On return the bits corresponding to
									the IDs of instruction required for feedback are set.
			  puFBInstCount			- Incremented for each bit set in auFBDependency.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uResultIdx;

	/*
		Mark all the instruction writing one of the shader results as needing to be in the
		pre-feedback shader program.
	*/
	for (uResultIdx = 0; uResultIdx < psResults->uCount; uResultIdx++)
	{
		PFEEDBACK_RESULT	psResult;
		PINST				psDefInst;

		psResult = &psResults->asResults[uResultIdx];
		psDefInst = psResult->psDefInst;
		if (psDefInst != NULL && psDefInst->psBlock == psSplitBlock)
		{
			SetFBDependency(auFBDependency, puFBInstCount, psDefInst->uId);
		}
	}
}

static
IMG_BOOL IsInternalRegisterResultUsedPostFeedback(PINTERMEDIATE_STATE	psState,
												  PCODEBLOCK			psSplitBlock,
												  PINST					psInst,
												  IMG_UINT32			uDest,
												  IMG_PUINT32			puFBDependency)
/*****************************************************************************
 FUNCTION	: IsInternalRegisterResultUsedPostFeedback
    
 PURPOSE	: Check for an internal register destination whether any of the
			  instructions using it are in the post-feedback part of the block.

 PARAMETERS	: psState			- Compiler state.
			  psSplitBlock		- Block which is to be split into pre- and
								post-feedback sections.
			  psInst			- Instruction defining an internal register.
			  uDest				- Instruction destination which defines an
								internal register.
			  
 RETURNS	: TRUE if the internal register is used in a post-feedback instruction.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	/*
		Look at the uses following after the define.
	*/
	for (psListEntry = psInst->asDestUseDef[uDest].sUseDef.sListEntry.psNext; 
		 psListEntry != NULL; 
		 psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		if (psUse->eType == USE_TYPE_OLDDEST || psUse->eType == USE_TYPE_SRC)
		{
			PINST	psUseInst;

			/*
				Check for a use in a post-feedback instruction.
			*/
			psUseInst = psUse->u.psInst;
			ASSERT(psUseInst->psBlock == psSplitBlock);
			if (!GetBit(puFBDependency, psUseInst->uId))
			{
				return IMG_TRUE;
			}
		}
		else
		{
			/*
				Stop once the internal register is overwritten.
			*/
			ASSERT(psUse->eType == DEF_TYPE_INST);
			break;
		}
	}
	return IMG_FALSE;
}

static
IMG_BOOL AreInternalRegsLiveAtFB(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psSplitBlock,
								 PDGRAPH_STATE			psDepState,
								 IMG_PUINT32			puFBDependency)
/*****************************************************************************
 FUNCTION	: AreInternalRegsLiveAtFB
    
 PURPOSE	: Check if internal registers are live at the point of feedback.

 PARAMETERS	: psState			- Compiler state.
			  psSplitBlock		- Block which is to be split into pre- and
								post-feedback sections.
			  psDepState		- Dependency graph state.
			  puFBDependency	- Bit array with a bit set for each instruction
								which is before feedback.
			  
 RETURNS	: TRUE if internal registers are defined in the pre-feedback
			  instructions and used in post-feedback instructions.
*****************************************************************************/
{
	IMG_UINT32	uInst;

	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
	{
		if (GetBit(puFBDependency, uInst))
		{
			IMG_UINT32	uDest;
			PINST		psInst = ArrayGet(psState, psDepState->psInstructions, uInst);

			for (uDest = 0; uDest < psInst->uDestCount; uDest++)
			{
				PARG	psDest = &psInst->asDest[uDest];

				/*
					If this instruction defines an internal register then check whether any of the instruction
					using it are in post-feedback instruction.
				*/
				if (psDest->uType == USEASM_REGTYPE_FPINTERNAL)
				{
					if (IsInternalRegisterResultUsedPostFeedback(psState, psSplitBlock, psInst, uDest, puFBDependency))
					{
						return IMG_TRUE;
					}
				}
			}
		}
	}
	return IMG_FALSE;
}

/*****************************************************************************
 FUNCTION	: IsBlockDominatedByPreSplitBlock

 PURPOSE	: Tests whether a specific instruction is dominated by Pre Split block.

 PARAMETERS	: psState			- Compiler state.			  
			  psInst			- Instruction to test the dominance for.
			  
 RETURNS	: IMG_TRUE	- If instruction is dominated by Pre Split block.
			  IMG_FALSE	- If instruction is not dominated by Pre Split block.
*****************************************************************************/
IMG_INTERNAL
IMG_BOOL IsBlockDominatedByPreSplitBlock(	PINTERMEDIATE_STATE	psState,										
											PCODEBLOCK			psCodeBlock)
{
	if(((psState->uFlags2 & USC_FLAGS2_SPLITCALC) == 0) || psCodeBlock == NULL || psState->psPreSplitBlock == NULL) return IMG_FALSE;
	ASSERT(psState->psPreSplitBlock->uNumSuccs == 1);	
	return Dominates(psState, psState->psPreSplitBlock->asSuccs[0].psDest, psCodeBlock);
}

static
IMG_VOID ProcessFeedbackResults(PINTERMEDIATE_STATE psState, PFEEDBACK_RESULTS	psResults)
/*****************************************************************************
 FUNCTION	: ProcessFeedbackResults
    
 PURPOSE	: 

 PARAMETERS	: psState			- Compiler state.
			  psResults
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uResultIdx;

	for (uResultIdx = 0; uResultIdx < psResults->uCount; uResultIdx++)
	{
		PFEEDBACK_RESULT	psResult = &psResults->asResults[uResultIdx];
		PFIXED_REG_DATA		psFixedReg = psResult->psFixedReg;

		ASSERT(psResult->uFixedRegOffset < psFixedReg->uConsecutiveRegsCount);

		if (psFixedReg->aeUsedForFeedback == NULL)
		{
			IMG_UINT32	uRegIdx;

			psFixedReg->aeUsedForFeedback = 
				UscAlloc(psState, sizeof(psFixedReg->aeUsedForFeedback[0]) * psFixedReg->uConsecutiveRegsCount);
			for (uRegIdx = 0; uRegIdx < psFixedReg->uConsecutiveRegsCount; uRegIdx++)
			{
				psFixedReg->aeUsedForFeedback[uRegIdx] = FEEDBACK_USE_TYPE_NONE;
			}
		}
		if (psResult->bFBOnly)
		{
			psFixedReg->aeUsedForFeedback[psResult->uFixedRegOffset] = FEEDBACK_USE_TYPE_ONLY;
		}
		else
		{
			psFixedReg->aeUsedForFeedback[psResult->uFixedRegOffset] = FEEDBACK_USE_TYPE_BOTH;
		}

		if (psResult->uPartialResultRegNum != psFixedReg->auVRegNum[psResult->uFixedRegOffset] || psResult->bFBOnly)
		{
			PINST	psEpilogInst;

			psEpilogInst = AllocateInst(psState, NULL);
			SetOpcodeAndDestCount(psState, psEpilogInst, IFEEDBACKDRIVEREPILOG, 1 /* uDestCount */);

			psEpilogInst->u.psFeedbackDriverEpilog->psFixedReg = psFixedReg;
			psEpilogInst->u.psFeedbackDriverEpilog->uFixedRegOffset = psResult->uFixedRegOffset;
			if (psResult->bFBOnly)
			{
				psEpilogInst->u.psFeedbackDriverEpilog->bPartial = IMG_FALSE;
				psEpilogInst->u.psFeedbackDriverEpilog->uFixedRegChan = USC_UNDEF;
			}
			else
			{
				psEpilogInst->u.psFeedbackDriverEpilog->bPartial = IMG_TRUE;
				ASSERT(g_abSingleBitSet[psResult->uMask]);
				psEpilogInst->u.psFeedbackDriverEpilog->uFixedRegChan = (IMG_UINT32)g_aiSingleComponent[psResult->uMask];
			}

			SetSrc(psState, 
				   psEpilogInst, 
				   0 /* uSrcIdx */, 
				   psFixedReg->uVRegType, 
				   psResult->uPartialResultRegNum, 
				   UseDefGet(psState, psFixedReg->uVRegType, psResult->uPartialResultRegNum)->eFmt);

			AppendInst(psState, psState->psPreFeedbackDriverEpilogBlock, psEpilogInst);
		}
	}
}

static
IMG_BOOL IsFormatConversion(PINST psInst)
/*****************************************************************************
 FUNCTION	: IsFormatConversion
    
 PURPOSE	: 

 PARAMETERS	: psInst	- Instruction to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (psInst->eOpcode)
	{
		case IPCKU8F32:
		case IPCKU8F16:
		case IPCKU8U8:
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case IVPCKU8U8:
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static
IMG_VOID ReorderColourOutputPackInstructions(PINTERMEDIATE_STATE psState, PCODEBLOCK psLastBlock)
/*****************************************************************************
 FUNCTION	: ReorderColourOutputPackInstructions
    
 PURPOSE	: Where only part of the colour result is needed before feedback
			  and the colour result is written as a series of channel-by-channel
			  updates: reorder the defining instructions so the channels required
			  for feedback are written as early as possible.

 PARAMETERS	: psState			- Compiler state.
			  psLastBlock		- Block to reorder.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PFIXED_REG_DATA	psColOutput;
	PINST			psColDefInst;
	IMG_UINT32		uColDefDestIdx;
	PARG			psOldDest;
	IMG_UINT32		uSrc;

	/*
		Where the result of the pixel shader is a U8 vec4 which is converted from a vec4 of floats we
		might have generated the conversions in any channel order. Try and reorder the conversions so
		the ALPHA channel (which is needed for feedback) is written as earlier as possible e.g.

			IPCKU8F32	TEMP.XY, A, B
			IPCKU8F32	RESULT.ZW[=TEMP], C, D
		->
			IPCKU8F32	TEMP.ZW, C, D
			IPCKU8F32	RESULT.XY[=TEMP], A, B
	*/

	/*
		We only need to modify pixel shaders writing a U8 format colour result.
	*/
	if (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_PIXEL)
	{
		return;
	}
	if (psState->psSAOffsets->ePackDestFormat != UF_REGFORMAT_U8)
	{
		return;
	}

	/*
		Get the details of the intermediate register containing the colour result.
	*/
	psColOutput = psState->sShader.psPS->psColFixedReg;
	ASSERT(psColOutput->uConsecutiveRegsCount >= 1);

	/*
		Get the instruction which defines the colour result.
	*/
	psColDefInst = UseDefGetDefInst(psState, psColOutput->uVRegType, psColOutput->auVRegNum[0], &uColDefDestIdx);

	/*
		Check the instruction is in the block we are trying to split.
	*/
	if (psColDefInst == NULL || psColDefInst->psBlock != psLastBlock)
	{
		return;
	}

	/*
		Check the instruction is a format conversion.
	*/
	if (!IsFormatConversion(psColDefInst))
	{
		return;
	}
	ASSERT(uColDefDestIdx == 0);

	if ((psColDefInst->auDestMask[0] & USC_ALPHA_CHAN_MASK) == 0)
	{
		return;
	}
	if (!NoPredicate(psState, psColDefInst))
	{
		return;
	}
	if (psColDefInst->auDestMask[0] == USC_ALL_CHAN_MASK)
	{
		return;
	}

	for (;;)
	{
		PINST			psSecondDefInst;
		IMG_UINT32		uSecondDefDestIdx;
		ARG				sPartialDest;

		/*
			Get the source for the channels in the colour result not written in the first instruction.
		*/
		psOldDest = psColDefInst->apsOldDest[0];
		if (psOldDest == NULL || psOldDest->uType != USEASM_REGTYPE_TEMP)
		{	
			return;
		}

		/*
			Get the instruction which defines the channels not written in the first instruction.
		*/
		psSecondDefInst = UseDefGetDefInst(psState, psOldDest->uType, psOldDest->uNumber, &uSecondDefDestIdx);
		if (psSecondDefInst == NULL || psSecondDefInst->psBlock != psLastBlock)
		{
			return;
		}

		/*
			Check the second instruction is also a format conversion.
		*/
		if (!IsFormatConversion(psSecondDefInst))
		{
			return;
		}

		/*
			Check the two instructions write non-overlapping sets of channels.
		*/
		if ((psSecondDefInst->auDestMask[0] & psColDefInst->auDestMask[0]) != 0)
		{
			return;
		}
		if (!NoPredicate(psState, psSecondDefInst))
		{
			return;
		}

		/*	
			Check the result of the earlier instruction is used only in the later instruction.
		*/
		if (!UseDefIsPartialDestOnly(psState, psColDefInst, 0 /* uDestIdx */, &psSecondDefInst->asDest[0]))
		{
			return;
		}
	
		/*
			Check that the sources to the earlier instruction are either constants or types in SSA form. This
			means we can safely move it forward.
		*/	
		for (uSrc = 0; uSrc < psSecondDefInst->uArgumentCount; uSrc++)
		{
			PARG	psSrc = &psSecondDefInst->asArg[uSrc];

			if (IsModifiableRegisterArray(psState, psSrc->uType, psSrc->uNumber) || psSrc->uType == USEASM_REGTYPE_FPINTERNAL)
			{
				return;
			}
		}

		/*
			Save the destination of the second instruction.
		*/
		sPartialDest = psSecondDefInst->asDest[0];

		/*
			Move the second instruction to after the first instruction.
		*/
		RemoveInst(psState, psLastBlock, psSecondDefInst);
		InsertInstAfter(psState, psLastBlock, psSecondDefInst, psColDefInst);

		/*
			Move the second instruction's partial destination to the first instruction.
		*/
		CopyPartiallyWrittenDest(psState, psColDefInst, 0 /* uMoveToDestIdx */, psSecondDefInst, 0 /* uMoveFromDestIdx */);
	
		/*
			Move the first instruction's destination to the second instruction.
		*/
		MoveDest(psState, psSecondDefInst, 0 /* uMoveToDestIdx */, psColDefInst, 0 /* uMoveFromDestIdx */);
		psSecondDefInst->auLiveChansInDest[0] = psColDefInst->auLiveChansInDest[0];

		/*
			Write to what used to be the second instruction's destination from the first instruction and use that result 
			as the source for the channels not written by the second instruction.
		*/
		SetPartiallyWrittenDest(psState, psSecondDefInst, 0 /* uDestIdx */, &sPartialDest);
		SetDestFromArg(psState, psColDefInst, 0 /* uDestIdx */, &sPartialDest);
		psColDefInst->auLiveChansInDest[0] &= ~psSecondDefInst->auDestMask[0];
	}
}

IMG_INTERNAL 
IMG_VOID SplitFeedback(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: SplitFeedback
    
 PURPOSE	: Try and split the shader into two phases. For vertex shaders
			  the first phase calculates just the position; for pixel shader
			  the first phase calculates the result to be alpha tested, the
			  texkill result and the depth feedback result.

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PCODEBLOCK			psSplitBlock;
	IMG_UINT32			uFBInstCount;
	IMG_UINT32			uInst;
	IMG_INT32			iInst;
	PDGRAPH_STATE		psDepState;
	IMG_PUINT32			puFBDependency;
	PUSC_LIST_ENTRY		psListEntry;
	FEEDBACK_RESULTS	sResults;

	psState->psPreFeedbackBlock = NULL;
	psState->psPreFeedbackDriverEpilogBlock = NULL;

	/*
		Reorder writes to a packed pixel shader output to maximise the chance of splitting
		the program.
	*/
	ReorderColourOutputPackInstructions(psState, psState->psMainProg->sCfg.psExit);

	/*
		Create a list of the shader results which must be defined before the feedback
		point.
	*/
	GetResultsNeededForFeedback(psState, &sResults);

	/*
		Find a block which post-dominates all of the blocks containing instructions
		defining shader results needed for feedback.
	*/
	ComputeLoopNestingTree(psState, psState->psMainProg->sCfg.psEntry);
	psSplitBlock = GetFeedbackSplitBlock(psState, &sResults);
	if (IsSampleRateStillSplit(psState))
	{
		if(!PostDominated(psState, psSplitBlock, psState->psPreSplitBlock))
		{
			USC_ERROR(UF_ERR_INVALID_PROG_STRUCT, "Feedback Split point going in Sample Rate phase");
		}
	}

	/*
		Compute dependencies for the block.
	*/
	psDepState = ComputeBlockDependencyGraph(psState, psSplitBlock, IMG_FALSE);

	/*
		Find the instructions whose results are directly required for
		feedback.
	*/
	usc_alloc_bitnum(psState, puFBDependency, psDepState->uBlockInstructionCount);
	memset(puFBDependency, 0, UINTS_TO_SPAN_BITS(psDepState->uBlockInstructionCount) * sizeof(IMG_UINT32));
	uFBInstCount = 0;

	/*
		Add the instructions which are directly required for feedback.
	*/
	GetInstsNeededForFeedback(psSplitBlock, &sResults, puFBDependency, &uFBInstCount);

	/*
		Any delta instructions must go in the pre-feedback block since any predecessors of the unsplit block will
		point to the pre-feedback block and the post-feedback block will be the unconditional successor of the
		pre-feedback block.
	*/
	for (psListEntry = psSplitBlock->sDeltaInstList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PDELTA_PARAMS	psDeltaParams;
		PINST			psDeltaInst;

		psDeltaParams = IMG_CONTAINING_RECORD(psListEntry, PDELTA_PARAMS, sListEntry);
		psDeltaInst = psDeltaParams->psInst;
		SetFBDependency(puFBDependency, &uFBInstCount, psDeltaInst->uId);
	}

	/*
		Find all the instructions that the feedback calculation uses indirectly.
	*/
	for (iInst = (IMG_INT32)(psDepState->uBlockInstructionCount - 1U); iInst >= 0; iInst--)
	{
		/*
			Check if any instructions already needed before feedback are dependent
			on this instruction.
		*/
		if ((GetBit(puFBDependency, iInst) == 0U) && IsFBDependency(psState, psDepState, puFBDependency, iInst))
		{
			uFBInstCount++;
			SetBit(puFBDependency, iInst, 1U);
		}
	}

	/*
		Check if the internal registers are live at the point where the
		punchthrough calculation is completed.
	*/
	if (AreInternalRegsLiveAtFB(psState, psSplitBlock, psDepState, puFBDependency))
	{
		if (!IsSampleRateStillSplit(psState))
		{
			goto exitfunc;
		}
	}

	/*
		Check if it is worthwhile to split the alpha calculation.
	*/
	if (psSplitBlock->eType == CBTYPE_EXIT)
	{
		IMG_UINT32	uInstsSaved;

		uInstsSaved = psDepState->uBlockInstructionCount - uFBInstCount;
		if ((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX && uInstsSaved <= 2) || uInstsSaved == 0)
		{
			if (!IsSampleRateStillSplit(psState))
			{
				goto exitfunc;
			}
		}	
	}

	/*
		Remove all the instructions from the block we are going to split.
	*/
	ClearBlockInsts(psState, psSplitBlock);

	/*
		Allocate a new basic block to hold the instructions needed by the alpha test.
	*/
	psState->psPreFeedbackBlock = AllocateBlock(psState, &(psState->psMainProg->sCfg));
	RedirectEdgesFromPredecessors(psState, psSplitBlock, psState->psPreFeedbackBlock, IMG_FALSE /* bSyncEnd */);
	SetBlockUnconditional(psState, psState->psPreFeedbackBlock, psSplitBlock);

	/*
		Allocate a new basic block which represents the uses of shader results by the driver
		epilog code inserted between the pre- and post-feedback phases.
	*/
	psState->psPreFeedbackDriverEpilogBlock = AllocateBlock(psState, &(psState->psMainProg->sCfg));
	RedirectEdgesFromPredecessors(psState, psSplitBlock, psState->psPreFeedbackDriverEpilogBlock, IMG_FALSE /* bSyncEnd */);
	SetBlockUnconditional(psState, psState->psPreFeedbackDriverEpilogBlock, psSplitBlock);
	
	/*
		Split the last block into all instructions that are required for the alpha calculation and all the rest.
	*/
	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
	{
		if (GetBit(puFBDependency, uInst))
		{
			PINST psInst = ArrayGet(psState, psDepState->psInstructions, uInst);
			InsertInstBefore(psState, psState->psPreFeedbackBlock, psInst, NULL);
		}
	}
	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
	{
		if (!GetBit(puFBDependency, uInst))
		{
			PINST psInst = ArrayGet(psState, psDepState->psInstructions, uInst);
			InsertInstBefore(psState, psSplitBlock, psInst, NULL);
		}
	}

	ProcessFeedbackResults(psState, &sResults);

	/*
		Flag to later stages that we have done the split.
	*/
	psState->uFlags |= USC_FLAGS_SPLITFEEDBACKCALC;

exitfunc:
	/* Release code */
	psSplitBlock->uFlags |= USC_CODEBLOCK_NEED_DEP_RECALC;
	UscFree(psState, puFBDependency);
	FreeBlockDGraphState(psState, psSplitBlock);
	MergeBasicBlocks(psState, psState->psMainProg);
}

static
IMG_BOOL PeepholeOptimizer(PINTERMEDIATE_STATE psState,
						   PCODEBLOCK psCodeBlock,
						   PINST psFirstInst)
/*****************************************************************************
 FUNCTION	: PeepholeOptimizer
    
 PURPOSE	: Apply peephole optimsations at a particluar instruction

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Codeblock to optimise
			  psFirstInst		- Instruction to look at.
			  
 RETURNS	: IMG_TRUE iff a change was made
*****************************************************************************/
{
//	const IMG_UINT32 uMaxInsts = 3;		// Maximum number of instruction needed for a match
	PINST apsInst[3];				    // Matched instructions
	IMG_BOOL bChanged = IMG_FALSE;		// Whether anything changed
	IMG_BOOL bMatched;
	IMG_UINT32 uCtr;

	/* Check for something to do */
	if (psFirstInst == NULL)
	{
		return IMG_FALSE;
	}

	/* Start matching */
	/* 
	   pckf16f16 p, dst1, src1, #0
	   pckf16f16 !p, dst1, src1, #0
	   pckf16f16 dst2, dst1, #00
	   -->
	   pckf16f16 p, dst2, src1, #0
	   pckf16f16 !p, dst2, src1, #0
	*/
	{
		IMG_UINT32 uDestMask = 0;
		IMG_UINT32 uChan;
		ARG sDst;
		/* char* name[3] = {"", "", ""}; */

		apsInst[0] = psFirstInst;
		apsInst[1] = psFirstInst->psNext;
		apsInst[2] = (apsInst[1] != NULL) ? apsInst[1]->psNext : NULL;

		/*
		name[0] = g_psInstDesc[apsInst[0]->eOpcode].pszName;
		name[1] = (apsInst[1] != NULL) ? g_psInstDesc[apsInst[1]->eOpcode].pszName : "";
		name[2] = (apsInst[2] != NULL) ? g_psInstDesc[apsInst[2]->eOpcode].pszName : "";
		*/

		bMatched = IMG_TRUE;
		/* General tests */
		for (uCtr = 0; uCtr < 3; uCtr ++)
		{
			if (apsInst[uCtr] == NULL ||
				apsInst[uCtr]->eOpcode != IPCKF16F16 ||
				apsInst[uCtr]->asArg[1].uType != USEASM_REGTYPE_IMMEDIATE ||
				apsInst[uCtr]->asArg[1].uNumber != 0)
			{
				bMatched = IMG_FALSE;
				break;
			}
		}
		/* Test last instruction */
		if (bMatched)
		{
			/* Get the destination of the initial packs */
			sDst = apsInst[2]->asArg[0];
			uChan = GetPCKComponent(psState, apsInst[2], 0 /* uArgIdx */);

			if (uChan == 0)
				uDestMask = ((1 << 1) | (1 << 0));
			else if (uChan == 2)
				uDestMask = ((1 << 3) | (1 << 2));
			else
				bMatched = IMG_FALSE;
		}
		/* Match first instructions */
		if (bMatched)
		{
			if (!(apsInst[0]->auDestMask[0] == uDestMask &&
				  apsInst[1]->auDestMask[0] == uDestMask &&
				  EqualArgs(&apsInst[0]->asDest[0], &sDst) &&
				  EqualArgs(&apsInst[1]->asDest[0], &sDst) &&
				  EqualArgs(&apsInst[1]->asArg[0], &apsInst[1]->asArg[0]) &&
				  EqualPredicates(apsInst[0], apsInst[1])))
			{
				bMatched = IMG_FALSE;
			}
		}
		/* Act on match */
		if (bMatched)
		{
			/* Replace destinations */
			apsInst[0]->asDest[0] = apsInst[2]->asDest[0];
			apsInst[1]->asDest[0] = apsInst[2]->asDest[0];

			/* Replace destination channels liveness information */
			apsInst[0]->auLiveChansInDest[0] = apsInst[2]->auLiveChansInDest[0];
			apsInst[1]->auLiveChansInDest[0] = apsInst[2]->auLiveChansInDest[0];

			/* Remove last instruction */
			RemoveInst(psState, psCodeBlock, apsInst[2]);
			FreeInst(psState, apsInst[2]);

			bChanged = IMG_TRUE;
		}
	}

	return bChanged;
}


IMG_INTERNAL 
IMG_BOOL PeepholeOptimizeBlock(PINTERMEDIATE_STATE psState,
							   PCODEBLOCK psCodeBlock)
/*****************************************************************************
 FUNCTION	: PeepholeOptimizeBlock
    
 PURPOSE	: Apply peephole optimsations to a code block.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Codeblock to optimise
			  
 RETURNS	: IMG_TRUE iff a change was made
*****************************************************************************/
{
	IMG_BOOL bChanged = IMG_FALSE;
	PINST psInst;

	for (psInst = psCodeBlock->psBody; psInst; psInst = psInst->psNext)
	{
		if (PeepholeOptimizer(psState, psCodeBlock, psInst))
		{
			bChanged = IMG_TRUE;
		}
	}

	return bChanged;
}

/******************************************************************************
 End of file (reorder.c)
******************************************************************************/

