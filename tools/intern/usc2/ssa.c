/*************************************************************************
 * Name         : ssa.c
 * Title        : 
 * Created      : Nov 2005
 *
 * Copyright    : 2002-2006 by Imagination Technologies Limited. All rights reserved.
 *              : No part of this software, either material or conceptual 
 *              : may be copied or distributed, transmitted, transcribed,
 *              : stored in a retrieval system or translated into any 
 *              : human or computer language in any form by any means,
 *              : electronic, mechanical, manual or other-wise, or 
 *              : disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Unit 8, HomePark
 *              : Industrial Estate, King's Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Modifications:-
 * $Log: ssa.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include "usedef.h"
#include "alloc.h"

typedef struct _SSA_BLOCK
{
	IMG_UINT32	uNumFrontierBlocks;
	PCODEBLOCK*	apsFrontierBlocks;
} SSA_BLOCK, *PSSA_BLOCK;

typedef struct _DEF_SITE
{
	PCODEBLOCK			psBlock;
	struct _DEF_SITE*	psNext;
} DEF_SITE, *PDEF_SITE;

typedef struct _VAR_DATA
{
	/* Linked list of blocks containing one or more definitions of this variable. */
	PDEF_SITE		psDefSites;
	UF_REGFORMAT	eFmt;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/* If TRUE then this is a vector register. */
	IMG_BOOL		bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
} VAR_DATA, *PVAR_DATA;

typedef struct _VARS_DATA
{
	/*
		Information for each pre-SSA temporary register.
	*/
	IMG_UINT32	uOrigNumTemps;
	PVAR_DATA	asTemp;
	/*
		Information for each pre-SSA prediate register.
	*/
	IMG_UINT32	uOrigNumPreds;
	PVAR_DATA	asPred;
} VARS_DATA, *PVARS_DATA;

typedef struct _SSA_VAR
{
	PUSC_STACK		psRenameStack;	
} SSA_VAR, *PSSA_VAR;

typedef struct _SSA_FUNC
{
	/* Information for each block in the function. */
	PSSA_BLOCK	asBlocks;
	/*
		Information for each pre-SSA temporary register.
	*/
	IMG_UINT32	uOrigNumTemps;
	PSSA_VAR	asTemp;
	/*
		Information for each pre-SSA prediate register.
	*/
	IMG_UINT32	uOrigNumPreds;
	PSSA_VAR	asPred;
} SSA_FUNC, *PSSA_FUNC;

static
IMG_VOID AddToFrontier(PSSA_BLOCK psBlockData, PCODEBLOCK psBlockToAdd)
{
	IMG_UINT32	uIdx;

	for (uIdx = 0; uIdx < psBlockData->uNumFrontierBlocks; uIdx++)
	{
		if (psBlockData->apsFrontierBlocks[uIdx] == psBlockToAdd)
		{
			return;
		}
	}
	psBlockData->apsFrontierBlocks[uIdx] = psBlockToAdd;
	psBlockData->uNumFrontierBlocks++;
}

static
IMG_VOID ComputeNodeDominanceFrontier(PINTERMEDIATE_STATE psState, PSSA_FUNC psFuncData, PCODEBLOCK psBlock)
/*********************************************************************************
 Function			: ComputeNodeDominanceFrontier

 Description		: Calculate the dominance frontier for a flow control block and
					  it's children in the dominator tree.
 
 Parameters			: psState		- Current compiler state.
					  psFuncData	- Information about the function containing
									the block.
					  psBlock		- Flow control block.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uDomIdx;
	IMG_UINT32	uSuccIdx;
	IMG_UINT32	uMaxNumFrontierBlocks;
	PSSA_BLOCK	psSSABlock = &psFuncData->asBlocks[psBlock->uIdx];

	/*
		Calculate the frontiers for all children of this block in the dominator tree.
	*/
	uMaxNumFrontierBlocks = psBlock->uNumSuccs;
	for (uDomIdx = 0; uDomIdx < psBlock->uNumDomChildren; uDomIdx++)
	{
		PCODEBLOCK	psDomChild = psBlock->apsDomChildren[uDomIdx];
		PSSA_BLOCK	psDomChildData = &psFuncData->asBlocks[psDomChild->uIdx];
		ComputeNodeDominanceFrontier(psState, psFuncData, psDomChild);
		uMaxNumFrontierBlocks += psDomChildData->uNumFrontierBlocks;
	}

	/*
		Allocate storage for the maximum possible size of the dominance frontier for this block.
	*/
	psSSABlock->uNumFrontierBlocks = 0;
	psSSABlock->apsFrontierBlocks = UscAlloc(psState, sizeof(psSSABlock->apsFrontierBlocks[0]) * uMaxNumFrontierBlocks);

	for (uSuccIdx = 0; uSuccIdx < psBlock->uNumSuccs; uSuccIdx++)
	{
		PCODEBLOCK	psSucc = psBlock->asSuccs[uSuccIdx].psDest;
		if (psSucc->psIDom != psBlock)
		{
			AddToFrontier(psSSABlock, psSucc);
		}
	}
	for (uDomIdx = 0; uDomIdx < psBlock->uNumDomChildren; uDomIdx++)
	{
		PCODEBLOCK	psDomChild = psBlock->apsDomChildren[uDomIdx];
		PSSA_BLOCK	psDomChildData = &psFuncData->asBlocks[psDomChild->uIdx];
		IMG_UINT32	uFrontierIdx;
		for (uFrontierIdx = 0; uFrontierIdx < psDomChildData->uNumFrontierBlocks; uFrontierIdx++)
		{
			PCODEBLOCK	psDomChildFrontier = psDomChildData->apsFrontierBlocks[uFrontierIdx];
			if (psDomChildFrontier->psIDom != psBlock)
			{
				AddToFrontier(psSSABlock, psDomChildFrontier);
			}
		}
	}

	/*
		Shrink the storage for the dominance frontier to just the part which was used.
	*/
	ResizeArray(psState,
				psSSABlock->apsFrontierBlocks,
				uMaxNumFrontierBlocks * sizeof(psSSABlock->apsFrontierBlocks[0]),
				psSSABlock->uNumFrontierBlocks * sizeof(psSSABlock->apsFrontierBlocks[0]),
				(IMG_PVOID*)&psSSABlock->apsFrontierBlocks);

	#if defined(UF_TESTBENCH) && !defined(USER_PERFORMANCE_STATS_ONLY)
	{
		IMG_UINT32	uFrontierIdx;

		printf("\tBlock %d dominance frontier:", psBlock->uIdx);
		for (uFrontierIdx = 0; uFrontierIdx < psSSABlock->uNumFrontierBlocks; uFrontierIdx++)
		{
			printf(" %d", psSSABlock->apsFrontierBlocks[uFrontierIdx]->uIdx);
		}
		printf("\n");
	}
	#endif /* defined(UF_TESTBENCH) */
}

static
IMG_VOID CalculateDefSitesBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvVarsData)
/*********************************************************************************
 Function			: CalculateDefSitesBP

 Description		: For each variable create a list of the blocks in a function
					  containing a definition of the variable.
 
 Parameters			: psState		- Compiler state.
					  psBlock		- Current block in the function to process.
					  pvVarsData	- Information about each pre-SSA variable.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PVARS_DATA	psVarsData = (PVARS_DATA)pvVarsData;
	PINST		psInst;

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		{
			IMG_UINT32	uDestIdx;

			for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
			{
				PARG		psDest = &psInst->asDest[uDestIdx];
				PVAR_DATA	psVar;
				PDEF_SITE	psNewDefSite;

				if (psDest->uType == USEASM_REGTYPE_TEMP)
				{
					ASSERT(psDest->uNumber < psVarsData->uOrigNumTemps);
					psVar = &psVarsData->asTemp[psDest->uNumber];
				}
				else if (psDest->uType == USEASM_REGTYPE_PREDICATE)
				{
					ASSERT(psDest->uNumber < psVarsData->uOrigNumPreds);
					psVar = &psVarsData->asPred[psDest->uNumber];
				}
				else
				{
					continue;
				}

				/*
					Record the register format used with the block.
				*/
				if (psVar->eFmt == UF_REGFORMAT_UNTYPED)
				{
					psVar->eFmt = psDest->eFmt;
				}
				else
				{
					ASSERT(psVar->eFmt == psDest->eFmt || psDest->eFmt == UF_REGFORMAT_UNTYPED);
				}

				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				/*
					Record if this register is a vector.
				*/
				psVar->bVector = IsVectorDest(psState, psInst, uDestIdx);
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						
				/*
					Skip if we already found a definition of the variable in this block.
				*/
				if (psVar->psDefSites != NULL && psVar->psDefSites->psBlock == psBlock)
				{
					continue;
				}

				/*
					Create a new definition record for the variable in this block.
				*/
				psNewDefSite = UscAlloc(psState, sizeof(*psNewDefSite));
				psNewDefSite->psBlock = psBlock;
				psNewDefSite->psNext = psVar->psDefSites;
				psVar->psDefSites = psNewDefSite;
			}
		}
		{
			IMG_UINT32	uSrcIdx;

			for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
			{
				PARG		psSrc = &psInst->asArg[uSrcIdx];
				PVAR_DATA	psVar;				

				if (psSrc->uType == USEASM_REGTYPE_TEMP)
				{
					ASSERT(psSrc->uNumber < psVarsData->uOrigNumTemps);
					psVar = &psVarsData->asTemp[psSrc->uNumber];
				}
				else if (psSrc->uType == USEASM_REGTYPE_PREDICATE)
				{
					ASSERT(psSrc->uNumber < psVarsData->uOrigNumPreds);
					psVar = &psVarsData->asPred[psSrc->uNumber];
				}
				else
				{
					continue;
				}

				/*
					Record the register format used with the block.
				*/
				if (psVar->eFmt == UF_REGFORMAT_UNTYPED)
				{
					psVar->eFmt = psSrc->eFmt;
				}
				else
				{
					ASSERT(psVar->eFmt == psSrc->eFmt || psSrc->eFmt == UF_REGFORMAT_UNTYPED);
				}

				#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
				/*
					Record if this register is a vector.
				*/
				psVar->bVector = IsVectorSource(psState, psInst, uSrcIdx);
				#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */				
			}
		}
	}
}

static
IMG_VOID PushRename(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, PSSA_VAR psVar)
/*********************************************************************************
 Function			: PushRename

 Description		: Add a new entry to the rename stack for a register.
 
 Parameters			: psState			- Current compiler state.	
					  uType				- Register type.
					  psVar				- Register.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uNewVar;

	if (uType == USEASM_REGTYPE_TEMP)
	{
		uNewVar = GetNextRegister(psState);
	}
	else
	{
		ASSERT(uType == USEASM_REGTYPE_PREDICATE);
		uNewVar = GetNextPredicateRegister(psState);
	}
	UscStackPush(psState, psVar->psRenameStack, &uNewVar);
}

static
IMG_UINT32 GetNewName(PINTERMEDIATE_STATE psState, 
					  PSSA_FUNC psFuncData, 
					  IMG_UINT32 uType, 
					  IMG_UINT32 uOldNumber,
					  IMG_BOOL bAllowUninitialized)
/*********************************************************************************
 Function			: GetNewName

 Description		: Get the new name for a register.
 
 Parameters			: psState			- Current compiler state.	
					  psFuncData		- Information about the current function.
					  uType				- Register.
					  uOldNumber
					  bAllowUninitialized
										- If TRUE return USC_UNDEF if the register
										is uninitialized.

 Globals Effected	: None

 Return				: The new register number.
*********************************************************************************/
{
	PSSA_VAR	psVar;
	
	if (uType == USEASM_REGTYPE_TEMP)
	{
		ASSERT(uOldNumber < psFuncData->uOrigNumTemps);
		psVar = &psFuncData->asTemp[uOldNumber];
	}
	else
	{
		ASSERT(uType == USEASM_REGTYPE_PREDICATE);
		ASSERT(uOldNumber < psFuncData->uOrigNumPreds);
		psVar = &psFuncData->asPred[uOldNumber];
	}
	
	if (bAllowUninitialized && (psVar->psRenameStack == NULL || UscStackEmpty(psVar->psRenameStack)))
	{
		return USC_UNDEF;
	}

	if (psVar->psRenameStack == NULL)
	{
		psVar->psRenameStack = UscStackMake(psState, sizeof(IMG_UINT32));
	}

	if (UscStackEmpty(psVar->psRenameStack))
	{
		PushRename(psState, uType, psVar);
	}

	return *(IMG_PUINT32)UscStackTop(psVar->psRenameStack);
}

IMG_INTERNAL
PVREGISTER GetVRegister(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber)
/*********************************************************************************
 Function			: GetVRegister

 Description		: 
 
 Parameters			: psState			- Current compiler state.	
					  uType				- Register.
					  uNumber

 Globals Effected	: None

 Return				: A pointer to information about the virtual register.
*********************************************************************************/
{
	switch (uType)
	{
		case USEASM_REGTYPE_TEMP:		
		{
			if (psState->psTempVReg != NULL)
			{
				return (PVREGISTER)ArrayGet(psState, psState->psTempVReg, uNumber);
			}
			else
			{
				return NULL;
			}
		}
		case USEASM_REGTYPE_PREDICATE:	
		{
			if (psState->psPredVReg != NULL)
			{
				return (PVREGISTER)ArrayGet(psState, psState->psPredVReg, uNumber);
			}
			else
			{
				return NULL;
			}
		}
		default:
		{
			return NULL;
		}
	}
}

static
IMG_BOOL RenameSrc(PINTERMEDIATE_STATE	psState, 
				   PSSA_FUNC			psFuncData,
				   IMG_UINT32			uType,
				   IMG_PUINT32			puNumber,
				   PVREGISTER*			ppsRegister,
				   PUSEDEF				psUse,
				   IMG_BOOL				bAllowUninitialized)
/*********************************************************************************
 Function			: RenameInstSrc

 Description		: Rename an instruction source argument and add the source
					  argument to the USE/DEF information for the renamed register.
 
 Parameters			: psState			- Current compiler state.	
					  psFuncData		- Information about the current function.
					  puType			- Source argument to rename.
					  puNumber
					  ppsRegister
					  psUse				- Information about the type of USE.
					  bAllowUninitialized
										- If TRUE then don't rename a uninitialized
										register.

 Globals Effected	: None

 Return				: FALSE if the source argument is uninitialized and "bAllowUninitialized"
					  was TRUE.
*********************************************************************************/
{
	if (uType == USEASM_REGTYPE_TEMP || uType == USEASM_REGTYPE_PREDICATE)
	{
		IMG_UINT32	uNewNumber;

		uNewNumber = GetNewName(psState, psFuncData, uType, *puNumber, bAllowUninitialized);
		if (uNewNumber == USC_UNDEF)
		{
			return IMG_FALSE;
		}

		if ((ppsRegister != NULL) && (uNewNumber != USC_UNDEF))
		{
			*ppsRegister = GetVRegister(psState, uType, uNewNumber);
		}
		*puNumber = uNewNumber;
	}
	if (uType == USEASM_REGTYPE_TEMP ||
		uType == USEASM_REGTYPE_PREDICATE ||
		uType == USC_REGTYPE_REGARRAY ||
		uType == USC_REGTYPE_ARRAYBASE)
	{
		ASSERT(psUse->eType != USEDEF_TYPE_UNDEF);
		UseDefAddUse(psState, uType, *puNumber, psUse);
	}
	return IMG_TRUE;
}

static
IMG_BOOL RenameInstSrc(PINTERMEDIATE_STATE	psState, 
					   PSSA_FUNC			psFuncData,
					   PARG					psArg,
					   PARGUMENT_USEDEF		psArgUse,
					   IMG_BOOL				bAllowUninitializedSrc)
/*********************************************************************************
 Function			: RenameInstSrc

 Description		: Rename an instruction source argument and add the source
					  argument to the USE/DEF information for the renamed register.
 
 Parameters			: psState			- Current compiler state.	
					  psFuncData		- Information about the current function.
					  psArg				- Source argument to rename.
					  psArgUse			- Information about the type of USE.
					  bAllowUninitializedSrc
										- If TRUE don't rename an uninitialized
										source argument.

 Globals Effected	: None

 Return				: FALSE if "bAllowUninitializedSrc" was TRUE and the source
					  argument was uninitialized.
*********************************************************************************/
{
	IMG_BOOL	bWasUninitialized;

	bWasUninitialized = 
		RenameSrc(psState, 
				  psFuncData, 
				  psArg->uType, 
				  &psArg->uNumber, 
				  &psArg->psRegister, 
				  &psArgUse->sUseDef,
				  bAllowUninitializedSrc);
	RenameSrc(psState, 
			  psFuncData, 
			  psArg->uIndexType, 
			  &psArg->uIndexNumber, 
			  &psArg->psIndexRegister,
			  &psArgUse->sIndexUseDef,
			  IMG_FALSE /* bAllowUninitialized */);
	return bWasUninitialized;
}

static
IMG_VOID RenameDest(PINTERMEDIATE_STATE psState, 
					PSSA_FUNC			psFuncData, 
					IMG_UINT32			uType, 
					IMG_PUINT32			puNumber,
					PVREGISTER*			ppsRegister)
/*********************************************************************************
 Function			: RenameDest

 Description		: Rename an instruction destination.
 
 Parameters			: psState			- Current compiler state.	
					  psFuncData		- Information about the current function.
					  uType				- Register type of the instruction destination.
					  puNumber			- On input: the old register number of the destination.
										  On output: the renamed register number.
				      ppsRegister		- If non-NULL then returns a pointer to
										the virtual register information for the
										renamed register.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uOldNumber = *puNumber;
	PSSA_VAR	psVar;
	
	if (uType == USEASM_REGTYPE_TEMP)
	{
		ASSERT(uOldNumber < psFuncData->uOrigNumTemps);
		psVar = &psFuncData->asTemp[uOldNumber];
	}
	else
	{
		ASSERT(uType == USEASM_REGTYPE_PREDICATE);
		ASSERT(uOldNumber < psFuncData->uOrigNumPreds);
		psVar = &psFuncData->asPred[uOldNumber];
	}

	if (psVar->psRenameStack == NULL)
	{
		psVar->psRenameStack = UscStackMake(psState, sizeof(IMG_UINT32));
	}
	PushRename(psState, uType, psVar);
				
	*puNumber = *(IMG_PUINT32)UscStackTop(psVar->psRenameStack);
	if (ppsRegister != NULL)
	{
		*ppsRegister = GetVRegister(psState, uType, *puNumber);
	}
}

static
IMG_VOID RenameFunctionOutputs(PINTERMEDIATE_STATE	psState, 
							   PFUNC				psFunc, 
						       PSSA_FUNC			psFuncData)
/*********************************************************************************
 Function			: RenameFunctionOutputs

 Description		: Rename function outputs.
 
 Parameters			: psState			- Current compiler state.	
					  psFunc			- Function to rename for.
					  psFuncData		- Information about the current function.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	i;

	for (i = 0; i < psFunc->sOut.uCount; i++)
	{
		PFUNC_INOUT	psOut = &psFunc->sOut.asArray[i];

		RenameSrc(psState,
				  psFuncData,
				  psOut->uType,
				  &psOut->uNumber,
				  NULL /* ppsRegister */,
				  &psFunc->sOut.asArrayUseDef[i],
				  IMG_FALSE /* bAllowUninitialized */);
	}
}

static
IMG_VOID RenameFunctionInputs(PINTERMEDIATE_STATE	psState, 
							  PFUNC					psFunc, 
							  PSSA_FUNC				psFuncData)
/*********************************************************************************
 Function			: RenameFunctionInputs

 Description		: Rename function inputs.
 
 Parameters			: psState			- Current compiler state.	
					  psFunc			- Function to rename for.
					  psFuncData		- Information about the current function.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	i;

	for (i = 0; i < psFunc->sIn.uCount; i++)
	{
		PFUNC_INOUT	psIn = &psFunc->sIn.asArray[i];

		RenameDest(psState, 
				   psFuncData, 
				   psIn->uType,
				   &psIn->uNumber,
				   NULL /* ppsRegister */);
		UseDefAddDef(psState, psIn->uType, psIn->uNumber, &psFunc->sIn.asArrayUseDef[i]);
	}
}

static
IMG_VOID RenameFixedRegs(PINTERMEDIATE_STATE psState, PSSA_FUNC psFuncData, IMG_BOOL bLiveAtShaderEnd)
/*********************************************************************************
 Function			: RenameFixedRegs

 Description		: Rename the set of registers written in the driver prolog or
					  used in the driver epilog.
 
 Parameters			: psState			- Current compiler state.	
					  psFuncData		- Information about the current function.
					  bLiveAtShaderEnd	- If TRUE rename in the driver epilog.
										  If FALSE rename in the driver prolog.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psState->sFixedRegList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PFIXED_REG_DATA	psFixedReg = IMG_CONTAINING_RECORD(psListEntry, PFIXED_REG_DATA, sListEntry);

		if (psFixedReg->bLiveAtShaderEnd == bLiveAtShaderEnd)
		{
			IMG_UINT32	uRegIdx;

			/*
				If this fixed register is associated with a register array then copy the
				new register numbers from the array.
			*/
			if (psFixedReg->uRegArrayIdx != USC_UNDEF)
			{
				PUSC_VEC_ARRAY_REG	psRegArray = psState->apsVecArrayReg[psFixedReg->uRegArrayIdx];

				ASSERT((psFixedReg->uRegArrayOffset + psFixedReg->uConsecutiveRegsCount) <= psRegArray->uRegs);
				for (uRegIdx = 0; uRegIdx < psFixedReg->uConsecutiveRegsCount; uRegIdx++)
				{
					psFixedReg->auVRegNum[uRegIdx] = psRegArray->uBaseReg + psFixedReg->uRegArrayOffset + uRegIdx;
				}
			}
			else
			{
				for (uRegIdx = 0; uRegIdx < psFixedReg->uConsecutiveRegsCount; uRegIdx++)
				{
					if (bLiveAtShaderEnd)
					{
						RenameSrc(psState, 
								  psFuncData, 
								  psFixedReg->uVRegType, 
								  &psFixedReg->auVRegNum[uRegIdx], 
								  NULL /* ppsRegister */,
								  &psFixedReg->asVRegUseDef[uRegIdx],
								  IMG_FALSE /* bAllowUninitialized */);
					}
					else
					{
						RenameDest(psState, 
								   psFuncData, 
								   psFixedReg->uVRegType, 
								   &psFixedReg->auVRegNum[uRegIdx],
								   NULL /* ppsRegister */);
						UseDefAddFixedRegDef(psState, psFixedReg, uRegIdx);
					}
				}
			}
		}
	}
}

static
IMG_VOID RenameBlock(PINTERMEDIATE_STATE psState, PSSA_FUNC psFuncData, PCODEBLOCK psBlock)
/*********************************************************************************
 Function			: RenameBlock

 Description		: Rename variables in a block and it's dominated children.
 
 Parameters			: psState			- Current compiler state.	
					  psFuncData		- Information about the current function.
					  psBlock			- Block to rename in.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PINST				psInst;
	IMG_UINT32			uSuccIdx;
	IMG_UINT32			uDomIdx;
	PFUNC				psFunc = psBlock->psOwner->psFunc;

	if (psBlock == psFunc->sCfg.psEntry)
	{
		if (psFunc == psState->psMainProg)
		{
			/*
				Rename registers in the driver prolog.
			*/
			RenameFixedRegs(psState, psFuncData, IMG_FALSE /* bLiveAtShaderEnd */);
		}
		else
		{
			RenameFunctionInputs(psState, psFunc, psFuncData);
		}
	}

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uSrcIdx;
		IMG_UINT32	uDestIdx;

		if (psInst->eOpcode != IDELTA)
		{
			IMG_UINT32 uPred;

			/*
				Rename instruction source arguments.
			*/
			for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
			{
				PARG	psSrc = &psInst->asArg[uSrcIdx];

				RenameInstSrc(psState,
							  psFuncData,
							  psSrc,
							  &psInst->asArgUseDef[uSrcIdx],
							  IMG_FALSE /* bAllowUninitializedSrc */);
			}	
			
			for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
			{
				if (psInst->apsPredSrc[uPred] != NULL)
				{
					RenameSrc(psState,
						psFuncData,
						psInst->apsPredSrc[uPred]->uType,
						&psInst->apsPredSrc[uPred]->uNumber,
						&psInst->apsPredSrc[uPred]->psRegister,
						psInst->apsPredSrcUseDef[uPred],
						IMG_FALSE /* bAllowUninitialized */);
				}
			}

			for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
			{
				PARG	psOldDest = psInst->apsOldDest[uDestIdx];
				PARG	psDest = &psInst->asDest[uDestIdx];

				/*
					Rename the old value of a partially/conditionally written
					destination.
				*/
				if (psOldDest != NULL)
				{
					if (!RenameInstSrc(psState, 
									   psFuncData, 
									   psOldDest, 
									   psInst->apsOldDestUseDef[uDestIdx],
									   IMG_TRUE /* bAllowUninitializedSrc */))
					{
						/*
							If the old value is uninitialized then clear the reference to it.
						*/
						SetPartiallyWrittenDest(psState, psInst, uDestIdx, NULL /* psPartialDest */);
					}
				}
				/*
					Rename any index used in a destination.
				*/
				if (psDest->uIndexType == USEASM_REGTYPE_TEMP)
				{
					RenameSrc(psState, 
							  psFuncData, 
							  USEASM_REGTYPE_TEMP, 
							  &psDest->uIndexNumber, 
							  &psDest->psIndexRegister,
							  &psInst->asDestUseDef[uDestIdx].sIndexUseDef,
							  IMG_FALSE /* bAllowUninitializedSrc */);
				}
			}
		}
		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG	psDest = &psInst->asDest[uDestIdx];
			PUSEDEF	psDestDef = &psInst->asDestUseDef[uDestIdx].sUseDef;

			if (psDest->uType == USEASM_REGTYPE_TEMP || psDest->uType == USEASM_REGTYPE_PREDICATE)
			{
				/*
					Abuse the uNumberPreMoe to store the register number before renaming. We'll use this
					below to pop the stack for instruction destinations.
				*/
				psDest->uNumberPreMoe = psDest->uNumber;

				/*
					Update the destination register number.
				*/
				RenameDest(psState, psFuncData, psDest->uType, &psDest->uNumber, &psDest->psRegister);

				/*
					Record the definition point of the new register number.
				*/
				UseDefAddDef(psState, psDest->uType, psDest->uNumber, psDestDef);
			}
			else if (psDest->uType == USC_REGTYPE_REGARRAY || 
					 psDest->uType == USC_REGTYPE_ARRAYBASE)
			{
				/*
					Just record the definition point for indexable objects.
				*/
				UseDefAddDef(psState, psDest->uType, psDest->uNumber, psDestDef);
			}
		}
	}

	/*
		Rename in a flow control block.
	*/
	if (psBlock->eType == CBTYPE_COND)
	{
		RenameSrc(psState, 
				  psFuncData,
				  psBlock->u.sCond.sPredSrc.uType,
				  &psBlock->u.sCond.sPredSrc.uNumber,
				  &psBlock->u.sCond.sPredSrc.psRegister,
				  &psBlock->u.sCond.sPredSrcUse,
				  IMG_FALSE /* bAllowUninitializedSrc */);
	}
	else if (psBlock->eType == CBTYPE_SWITCH)
	{
		RenameInstSrc(psState, 
					  psFuncData,
					  psBlock->u.sSwitch.psArg,
					  &psBlock->u.sSwitch.sArgUse,
					  IMG_FALSE /* bAllowUninitializedSrc */);
	}

	if (psBlock == psFunc->sCfg.psExit)
	{
		if (psFunc == psState->psMainProg)
		{
			/*
				Rename in the driver epilog.
			*/
			RenameFixedRegs(psState, psFuncData, IMG_TRUE /* bLiveAtShaderEnd */);
		}
		else
		{
			/*
				Rename results of the function.
			*/
			RenameFunctionOutputs(psState, psFunc, psFuncData);
		}
	}

	/*
		Rename in delta functions in this block's successors.
	*/
	for (uSuccIdx = 0; uSuccIdx < psBlock->uNumSuccs; uSuccIdx++)
	{
		PCODEBLOCK		psSucc = psBlock->asSuccs[uSuccIdx].psDest;
		IMG_UINT32		uPredIdx = psBlock->asSuccs[uSuccIdx].uDestIdx;
		PUSC_LIST_ENTRY	psListEntry;

		for (psListEntry = psSucc->sDeltaInstList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PDELTA_PARAMS	psDeltaParams;
			PINST			psDeltaInst;
			PARG			psDeltaSrc;

			psDeltaParams = IMG_CONTAINING_RECORD(psListEntry, PDELTA_PARAMS, sListEntry);
			psDeltaInst = psDeltaParams->psInst;
			ASSERT(psDeltaInst->eOpcode == IDELTA);
			ASSERT(psDeltaInst->uArgumentCount == psSucc->uNumPreds);

			/*
				Get the argument corresponding to the edge from the current block to this successor block in
				the control flow graph.
			*/
			ASSERT(uPredIdx < psDeltaInst->uArgumentCount);
			psDeltaSrc = &psDeltaInst->asArg[uPredIdx];

			ASSERT(psDeltaSrc->uType == USEASM_REGTYPE_TEMP || psDeltaSrc->uType == USEASM_REGTYPE_PREDICATE);

			/*
				Rename the argument.
			*/
			RenameInstSrc(psState, 
						  psFuncData, 
						  psDeltaSrc, 
						  &psDeltaInst->asArgUseDef[uPredIdx],
						  IMG_FALSE /* bAllowUninitialized */);
		}
	}

	/*
		Recursively rename in the children of this block in the dominator tree.
	*/
	for (uDomIdx = 0; uDomIdx < psBlock->uNumDomChildren; uDomIdx++)
	{
		RenameBlock(psState, psFuncData, psBlock->apsDomChildren[uDomIdx]);
	}

	/*
		For each define in this block pop the rename stack for the defined register.
	*/
	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uDestIdx;

		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG		psDest = &psInst->asDest[uDestIdx];
			PSSA_VAR	psVar;

			if (psDest->uType == USEASM_REGTYPE_TEMP)
			{
				psVar = &psFuncData->asTemp[psDest->uNumberPreMoe];
			}
			else if (psDest->uType == USEASM_REGTYPE_PREDICATE)
			{
				psVar = &psFuncData->asPred[psDest->uNumberPreMoe];
			}
			else
			{
				continue;
			}

			ASSERT(psVar->psRenameStack != NULL);
			UscStackPop(psState, psVar->psRenameStack);
		}
	}
}

static
IMG_VOID InsertDelta(PINTERMEDIATE_STATE	psState,
					 PFUNC					psFunc,
					 PSSA_FUNC				psFuncData,
					 IMG_PUINT32			auInsertedDelta,
					 IMG_UINT32				uVarType,
					 PVAR_DATA				asVars,
					 IMG_UINT32				uCount)
/*********************************************************************************
 Function			: InsertDelta

 Description		: Insert delta functions where two possible flow control
				      paths both containing definitions of a variable merge.
 
 Parameters			: psState		- Current compiler state.
					  psVars		- For each variable: a list of blocks where it
									is defined.
					  psFunc		- Function to insert delta functions in.
					  psFuncData	- Dominance frontier for each block in the
									function.
					  auInsertedDelta
									- Bit array which is the same size as the
									number of blocks in the function. For
									temporary storage.
					  uVarType		- Type of variables to insert delta functions
									for.
					  asVars		- List of definition sites for each variable.
					  uCount		- Count of variables.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uVarIdx;

	for (uVarIdx = 0; uVarIdx < uCount; uVarIdx++)
	{
		PDEF_SITE		psDefSite;
		BLOCK_WORK_LIST	sWorkList;
		PCODEBLOCK		psWork;

		memset(auInsertedDelta, 0, UINTS_TO_SPAN_BITS(psFunc->sCfg.uNumBlocks) * sizeof(IMG_UINT32));

		/*
			Create the initial work list from the list of blocks containing definitions of the
			variable.
		*/
		InitializeBlockWorkList(&sWorkList);
		for (psDefSite = asVars[uVarIdx].psDefSites; psDefSite != NULL; psDefSite = psDefSite->psNext)
		{
			if (psDefSite->psBlock->psOwner->psFunc == psFunc)
			{
				AppendToBlockWorkList(&sWorkList, psDefSite->psBlock);
			}
		}

		/*
			Remove an entry from the work list.
		*/
		while ((psWork = RemoveBlockWorkListHead(psState, &sWorkList)) != NULL)
		{
			PSSA_BLOCK	psBlockData;
			IMG_UINT32	uBlockIdx;

			/*
				For each block in the dominance frontier of the block from the work
				list.
			*/
			psBlockData = &psFuncData->asBlocks[psWork->uIdx];
			for (uBlockIdx = 0; uBlockIdx < psBlockData->uNumFrontierBlocks; uBlockIdx++)
			{
				PCODEBLOCK	psFrontier = psBlockData->apsFrontierBlocks[uBlockIdx];

				/*
					Have we already inserted a delta function in this block?
				*/
				if (GetBit(auInsertedDelta, psFrontier->uIdx) == 0)
				{
					PINST		psDeltaInst;
					IMG_UINT32	uPredIdx;

					psDeltaInst = AllocateInst(psState, NULL);
					SetOpcode(psState, psDeltaInst, IDELTA);
					SetArgumentCount(psState, psDeltaInst, psFrontier->uNumPreds);
					psDeltaInst->asDest[0].uType = uVarType;
					psDeltaInst->asDest[0].uNumber = uVarIdx;
					psDeltaInst->asDest[0].eFmt = asVars[uVarIdx].eFmt;
					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					psDeltaInst->u.psDelta->bVector = asVars[uVarIdx].bVector;
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					for (uPredIdx = 0; uPredIdx < psFrontier->uNumPreds; uPredIdx++)
					{
						psDeltaInst->asArg[uPredIdx].uType = uVarType;
						psDeltaInst->asArg[uPredIdx].uNumber = uVarIdx;
						psDeltaInst->asArg[uPredIdx].eFmt = asVars[uVarIdx].eFmt;
					}
					InsertInstBefore(psState, psFrontier, psDeltaInst, psFrontier->psBody);

					/*
						The block we inserted the delta function in now contains a definition of the
						variable so add it to the work list.
					*/
					SetBit(auInsertedDelta, psFrontier->uIdx, 1);
					AppendToBlockWorkList(&sWorkList, psFrontier);
				}
			}
		}
	}
}

static
IMG_VOID FreeVars(PINTERMEDIATE_STATE psState, PSSA_VAR asVars, IMG_UINT32 uVarCount)
/*********************************************************************************
 Function			: FreeVars

 Description		: Free information about registers.
 
 Parameters			: psState			- Current compiler state.	
					  asVars			- Array of register information.
					  uVarCount			- Size of the array.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uVarIdx;

	for (uVarIdx = 0; uVarIdx < uVarCount; uVarIdx++)
	{
		PSSA_VAR	psVar = &asVars[uVarIdx];

		if (psVar->psRenameStack != NULL)
		{
			UscStackDelete(psState, psVar->psRenameStack);
		}
	}
}

static
IMG_VOID FreeVarData(PINTERMEDIATE_STATE psState, IMG_UINT32 uVarCount, PVAR_DATA asVars)
/*********************************************************************************
 Function			: FreeVarData

 Description		: Free information about each variable in the pre-SSA program.
 
 Parameters			: psState			- Current compiler state.	
					  uVarCount			- Count of elements to free.
					  asVars			- Array to free.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uVarIdx;

	for (uVarIdx = 0; uVarIdx < uVarCount; uVarIdx++)
	{
		PVAR_DATA	psVar = &asVars[uVarIdx];
		PDEF_SITE	psDefSite;
		PDEF_SITE	psNextDefSite;

		for (psDefSite = psVar->psDefSites; psDefSite != NULL; psDefSite = psNextDefSite)
		{
			psNextDefSite = psDefSite->psNext;
			UscFree(psState, psDefSite);
		}
	}
}

static
IMG_VOID SetupDefSites(PINTERMEDIATE_STATE psState, PVARS_DATA psVars, IMG_UINT32 uOrigNumTemps, IMG_UINT32 uOrigNumPreds)
/*********************************************************************************
 Function			: SetupDefSites

 Description		: Create a list of the definition sites of each variables.
 
 Parameters			: psState			- Current compiler state.	
					  psVars			- Information about each pre-SSA variable.
					  uOrigNumTemps		- Maximum register numbers used in the pre-SSA
					  uOrigNumPreds		program.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uRegIdx;

	/*
		Allocate per-temporary register data.
	*/
	psVars->uOrigNumTemps = uOrigNumTemps;
	psVars->asTemp = UscAlloc(psState, sizeof(psVars->asTemp[0]) * uOrigNumTemps);
	for (uRegIdx = 0; uRegIdx < uOrigNumTemps; uRegIdx++)
	{
		psVars->asTemp[uRegIdx].psDefSites = NULL;
		psVars->asTemp[uRegIdx].eFmt = UF_REGFORMAT_UNTYPED;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		psVars->asTemp[uRegIdx].bVector = IMG_FALSE;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}

	/*
		Allocate per-predicate register data.
	*/
	psVars->uOrigNumPreds = uOrigNumPreds;
	psVars->asPred = UscAlloc(psState, sizeof(psVars->asPred[0]) * uOrigNumPreds);
	for (uRegIdx = 0; uRegIdx < uOrigNumPreds; uRegIdx++)
	{
		psVars->asPred[uRegIdx].psDefSites = NULL;
		psVars->asPred[uRegIdx].eFmt = UF_REGFORMAT_UNTYPED;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		psVars->asTemp[uRegIdx].bVector = IMG_FALSE;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}

	/*
		Generate a list for each variable of the blocks containing a definition of that
		variable.
	*/
	DoOnAllBasicBlocks(psState, ANY_ORDER, CalculateDefSitesBP, IMG_TRUE /* bHandlesCalls */, psVars);
}

static
IMG_VOID InsertBlocksBeforeNonMergableBlocks(	PINTERMEDIATE_STATE psState, 
												PCFG				psCfg)
/*********************************************************************************
 Function			: InsertBlocksBeforeNonMergableBlocks

 Description		: Inserts block before every non mergable block in a function.
 
 Parameters			: psState			- Current compiler state.	
					  psCfg				- CFG to insert blocks in.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32 uNonMergableBlksCount = 0;
	PCODEBLOCK *apsNonMergableBlks = UscAlloc(psState, (sizeof(*apsNonMergableBlks) * psCfg->uNumBlocks));
	{
		IMG_UINT32 uI;
		for (uI = 0; uI < psCfg->uNumBlocks; uI++)
		{
			if(IsNonMergable(psState, psCfg->apsAllBlocks[uI]))
			{
				apsNonMergableBlks[uNonMergableBlksCount] = psCfg->apsAllBlocks[uI];
				uNonMergableBlksCount++;
			}
		}
	}
	{
		IMG_UINT32 uI;
		for (uI = 0; uI < uNonMergableBlksCount; uI++)
		{
			AddUnconditionalPredecessor(psState, apsNonMergableBlks[uI]);			
		}
	}
	UscFree(psState, apsNonMergableBlks);
}

static
IMG_VOID ValueNumberingFunc(PINTERMEDIATE_STATE psState, 
							PVARS_DATA			psVars, 
							PFUNC				psFunc, 
							IMG_UINT32			uOrigNumTemps, 
							IMG_UINT32			uOrigNumPreds)
/*********************************************************************************
 Function			: ValueNumberingFunc

 Description		: Convert a single function to SSA form.
 
 Parameters			: psState			- Current compiler state.	
					  psVars			- For each variable: a list of the blocks
										where it is defined.
					  psFunc			- Function to convert.
					  uOrigNumTemps		- Maximum register numbers used in the pre-SSA
					  uOrigNumPreds		program.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_PUINT32		auInsertedDelta;
	SSA_FUNC		sFuncData;
	IMG_UINT32		uBlockIdx;
	IMG_UINT32		uRegIdx;

	auInsertedDelta = UscAlloc(psState, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(psFunc->sCfg.uNumBlocks));

	sFuncData.uOrigNumTemps = uOrigNumTemps;
	sFuncData.uOrigNumPreds = uOrigNumPreds;

	/*
		Allocate per-flow control block data.
	*/
	sFuncData.asBlocks = UscAlloc(psState, sizeof(sFuncData.asBlocks[0]) * (psFunc->sCfg.uNumBlocks));
	for (uBlockIdx = 0; uBlockIdx < (psFunc->sCfg.uNumBlocks); uBlockIdx++)
	{
		sFuncData.asBlocks[uBlockIdx].apsFrontierBlocks = NULL;
	}

	/*
		Allocate per-temporary register data.
	*/
	sFuncData.asTemp = UscAlloc(psState, sizeof(sFuncData.asTemp[0]) * uOrigNumTemps);
	for (uRegIdx = 0; uRegIdx < uOrigNumTemps; uRegIdx++)
	{
		sFuncData.asTemp[uRegIdx].psRenameStack = NULL;
	}

	/*
		Allocate per-predicate register data.
	*/
	sFuncData.asPred = UscAlloc(psState, sizeof(sFuncData.asPred[0]) * uOrigNumPreds);
	for (uRegIdx = 0; uRegIdx < uOrigNumPreds; uRegIdx++)
	{
		sFuncData.asPred[uRegIdx].psRenameStack = NULL;
	}

	/*
		Calculate the dominance frontier for each flow control block in the function.
	*/
	#if defined(UF_TESTBENCH) && !defined(USER_PERFORMANCE_STATS_ONLY)
	if (psFunc->pchEntryPointDesc != NULL)
	{
		printf("Node dominance frontiers for: %s\n", psFunc->pchEntryPointDesc);
	}
	else
	{
		printf("Node dominance frontier for function %d\n", psFunc->uLabel);
	}
	#endif /* defined(UF_TESTBENCH) */
	ComputeNodeDominanceFrontier(psState, &sFuncData, psFunc->sCfg.psEntry);

	/*
		Insert delta functions where two flow control paths merge.
	*/
	for (uBlockIdx = 0; uBlockIdx < psFunc->sCfg.uNumBlocks; uBlockIdx++)
	{
		psFunc->sCfg.apsAllBlocks[uBlockIdx]->psWorkListNext = NULL;
	}
	InsertDelta(psState, psFunc, &sFuncData, auInsertedDelta, USEASM_REGTYPE_TEMP, psVars->asTemp, uOrigNumTemps);
	InsertDelta(psState, psFunc, &sFuncData, auInsertedDelta, USEASM_REGTYPE_PREDICATE, psVars->asPred, uOrigNumPreds);

	/*
		Apply renaming to all the blocks in the function starting at the entry and working down the dominator
		tree.
	*/
	RenameBlock(psState, &sFuncData, psFunc->sCfg.psEntry);

	/*
		For the case of a function containing an infinite loop the exit won't be reached via the dominator
		tree. Apply renaming to the exit anyway to rename stored information about function outputs.
	*/
	if (psFunc->sCfg.psExit->uNumPreds == 0 && psFunc->sCfg.psExit != psFunc->sCfg.psEntry)
	{
		RenameBlock(psState, &sFuncData, psFunc->sCfg.psExit);
	}

	/*	
		Free information about flow control blocks.
	*/
	for (uBlockIdx = 0; uBlockIdx < psFunc->sCfg.uNumBlocks; uBlockIdx++)
	{
		if (sFuncData.asBlocks[uBlockIdx].apsFrontierBlocks != NULL)
		{
			UscFree(psState, sFuncData.asBlocks[uBlockIdx].apsFrontierBlocks);
		}
	}
	UscFree(psState, sFuncData.asBlocks);

	/*
		Free information about temporary registers.
	*/
	FreeVars(psState, sFuncData.asTemp, uOrigNumTemps);
	UscFree(psState, sFuncData.asTemp);

	/*
		Free information about predicate registers.
	*/
	FreeVars(psState, sFuncData.asPred, uOrigNumPreds);
	UscFree(psState, sFuncData.asPred);

	UscFree(psState, auInsertedDelta);
}

IMG_INTERNAL
PCODEBLOCK AddUnconditionalPredecessor(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
{
	PCODEBLOCK	psNewBlock;

	psNewBlock = AllocateBlock(psState, psBlock->psOwner);
	RedirectEdgesFromPredecessors(psState, psBlock, psNewBlock, IMG_FALSE /* bSyncEnd */);
	SetBlockUnconditional(psState, psNewBlock, psBlock);
	return psNewBlock;
}

static
IMG_VOID CopyFunctionInputs(PINTERMEDIATE_STATE	psState,
							PFUNC				psFunc,
							PINST				psCallInst)
{
	IMG_UINT32	i;
	PCODEBLOCK	psDestBlock;
	PCODEBLOCK	psCallBlock;

	if (psFunc->sIn.uCount == 0)
	{
		return;
	}

	psCallBlock = psCallInst->psBlock;

	/*
		Create a new block with the block containing the call as its unconditional predecessor.
	*/
	psDestBlock = AddUnconditionalPredecessor(psState, psCallBlock);

	/*
		The set of registers live before the call is the set of registers live after the call
		less the registers written by the called function.
	*/
	CopyRegLiveSet(psState, &psCallBlock->sRegistersLiveOut, &psDestBlock->sRegistersLiveOut);
	for (i = 0; i < psFunc->sOut.uCount; i++)
	{
		PFUNC_INOUT		psOut = &psFunc->sOut.asArray[i];

		SetRegisterLiveMask(psState,
							&psDestBlock->sRegistersLiveOut,
							psOut->uType,
							psOut->uNumber,
							0 /* uArrayOffset */,
							0 /* uMask */);
	}

	for (i = 0; i < psFunc->sIn.uCount; i++)
	{
		PINST		psMOVInst;
		PFUNC_INOUT	psIn = &psFunc->sIn.asArray[i];
		PARG		psCallSrc = &psCallInst->asArg[i];

		psMOVInst = AllocateInst(psState, NULL /* psSrcLineInst */);
		if (psCallSrc->uType == USEASM_REGTYPE_IMMEDIATE && psCallSrc->uNumber > EURASIA_USE_MAXIMUM_IMMEDIATE)
		{
			SetOpcode(psState, psMOVInst, ILIMM);
		}
		else
		{
			SetOpcode(psState, psMOVInst, IMOV);
		}
		SetDest(psState, psMOVInst, 0 /* uDestIdx */, psIn->uType, psIn->uNumber, psIn->eFmt);
		MoveSrc(psState, psMOVInst, 0 /* uMoveToSrcIdx */, psCallInst, i /* uMoveFromSrcIdx */);
		AppendInst(psState, psDestBlock, psMOVInst);

		SetRegisterLiveMask(psState,
							&psDestBlock->sRegistersLiveOut,
							psIn->uType,
							psIn->uNumber,
							0 /* uArrayOffset */,
							psIn->uChanMask);
	}
}

static
IMG_VOID CopyFunctionOutputs(PINTERMEDIATE_STATE	psState,
							 PFUNC					psFunc,
							 PINST					psCallInst)
{
	IMG_UINT32	i;
	PCODEBLOCK	psOldCallBlock;
	PCODEBLOCK	psNewCallBlock;

	if (psFunc->sOut.uCount == 0)
	{
		return;
	}

	psOldCallBlock = psCallInst->psBlock;
	psNewCallBlock = AddUnconditionalPredecessor(psState, psOldCallBlock);
	
	CopyRegLiveSet(psState, &psOldCallBlock->sRegistersLiveOut, &psNewCallBlock->sRegistersLiveOut);

	RemoveInst(psState, psOldCallBlock, psCallInst);
	AppendInst(psState, psNewCallBlock, psCallInst);

	for (i = 0; i < psFunc->sOut.uCount; i++)
	{
		PINST		psMOVInst;
		PFUNC_INOUT	psOut = &psFunc->sOut.asArray[i];

		psMOVInst = AllocateInst(psState, NULL /* psSrcLineInst */);
		SetOpcode(psState, psMOVInst, IMOV);
		MoveDest(psState, psMOVInst, 0 /* uMoveToSrcIdx */, psCallInst, i /* uMoveFromSrcIdx */);
		SetSrc(psState, psMOVInst, 0 /* uSrcIdx */, psOut->uType, psOut->uNumber, psOut->eFmt);
		AppendInst(psState, psOldCallBlock, psMOVInst);

		SetRegisterLiveMask(psState,
							&psNewCallBlock->sRegistersLiveOut,
							psOut->uType,
							psOut->uNumber,
							0 /* uArrayOffset */,
							psOut->uChanMask);
		SetRegisterLiveMask(psState,
							&psNewCallBlock->sRegistersLiveOut,
							psMOVInst->asDest[0].uType,
							psMOVInst->asDest[0].uNumber,
							0 /* uArrayOffset */,
							0 /* uMask */);
	}
}

static
IMG_VOID SubstituteConstantCallParametersFunc(PINTERMEDIATE_STATE psState, PFUNC psFunc)
/*********************************************************************************
 Function			: SubstituteConstantCallParametersFunc

 Description		: Check for parameters which are the same at every call
				      site and substitute them into the call body.
 
 Parameters			: psState	- Current compiler state.
					  psFunc	- Function to check.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PINST		psCallSite;
	IMG_UINT32	uConstantInputCount;
	IMG_UINT32	uInIdx;
	IMG_PBOOL	abConstantInput;

	/*
		Skip uncalled functions and those with no parameters.
	*/
	if (psFunc->psCallSiteHead == NULL || psFunc->sIn.uCount == 0)
	{
		return;
	}

	/*
		Allocate an array to record whether each function input is constant.
	*/
	abConstantInput = UscAlloc(psState, sizeof(abConstantInput[0]) * psFunc->sIn.uCount);

	uConstantInputCount = 0;
	for (uInIdx = 0; uInIdx < psFunc->sIn.uCount; uInIdx++)
	{
		abConstantInput[uInIdx] = IMG_FALSE;
		if (psFunc->psCallSiteHead->asArg[uInIdx].uType == USEASM_REGTYPE_TEMP)
		{
			abConstantInput[uInIdx] = IMG_TRUE;
			uConstantInputCount++;
		}
	}

	for (psCallSite = psFunc->psCallSiteHead->u.psCall->psCallSiteNext; 
		 psCallSite != NULL; 
		 psCallSite = psCallSite->u.psCall->psCallSiteNext)
	{	
		for (uInIdx = 0; uInIdx < psFunc->sIn.uCount; uInIdx++)
		{
			if (abConstantInput[uInIdx])
			{
				if (!EqualArgs(&psFunc->psCallSiteHead->asArg[uInIdx], &psCallSite->asArg[uInIdx]))
				{
					abConstantInput[uInIdx] = IMG_FALSE;

					ASSERT(uConstantInputCount > 0);
					uConstantInputCount--;

					if (uConstantInputCount == 0)
					{
						break;
					}
				}
			}
		}
	}

	if (uConstantInputCount > 0)
	{
		IMG_UINT32	uOldInputCount;
		IMG_UINT32	uOutIdx;

		uOldInputCount = psFunc->sIn.uCount;

		uOutIdx = 0;
		for (uInIdx = 0; uInIdx < uOldInputCount; uInIdx++)
		{
			if (abConstantInput[uInIdx])
			{
				PUSEDEF_CHAIN	psInputUseDef;
				PUSC_LIST_ENTRY	psListEntry;
				PUSC_LIST_ENTRY	psNextListEntry;

				/*
					Substitute the constant input directly into the function body.
				*/
				psInputUseDef = 
					UseDefGet(psState, psFunc->sIn.asArray[uInIdx].uType, psFunc->sIn.asArray[uInIdx].uNumber);
				for (psListEntry = psInputUseDef->sList.psHead; 
					 psListEntry != NULL; 
					 psListEntry = psNextListEntry)
				{
					PUSEDEF	psUse;

					psNextListEntry = psListEntry->psNext;
					psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
					if (psUse == psInputUseDef->psDef)
					{
						continue;
					}
					UseDefSubstUse(psState, psInputUseDef, psUse, &psFunc->psCallSiteHead->asArg[uInIdx]);
				}

				UseDefDropFuncInput(psState, psFunc, uInIdx);
			}
			else
			{
				UseDefMoveFuncInput(psState, psFunc, uOutIdx, uInIdx);
				uOutIdx++;
			}
		}

		psFunc->sIn.uCount -= uConstantInputCount;
		ResizeTypedArray(psState, psFunc->sIn.asArray, uOldInputCount, psFunc->sIn.uCount);
		psFunc->sIn.asArrayUseDef = 
			UseDefResizeArray(psState, psFunc->sIn.asArrayUseDef, uOldInputCount, psFunc->sIn.uCount);

		/*
			Remove the constant arguments from all calls to the function.
		*/
		for (psCallSite = psFunc->psCallSiteHead; 
			 psCallSite != NULL; 
			 psCallSite = psCallSite->u.psCall->psCallSiteNext)
		{	
			/*
				Shuffle the call arguments down.
			*/
			uOutIdx = 0;
			for (uInIdx = 0; uInIdx < uOldInputCount; uInIdx++)
			{
				if (!abConstantInput[uInIdx])
				{
					MoveSrc(psState, psCallSite, uOutIdx, psCallSite, uInIdx);
					uOutIdx++;
				}
			}

			SetArgumentCount(psState, psCallSite, psFunc->sIn.uCount);
		}
	}

	UscFree(psState, abConstantInput);
}

static
IMG_VOID SubstituteConstantCallParameters(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: SubstituteConstantCallParameters

 Description		: Check for parameters which are the same at every call
				      site and substitute them into the call body.
 
 Parameters			: psState - Current compiler state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PFUNC	psFunc;

	/*
		Check for calls whether some of the inputs are the same at every call site.
	*/
	for (psFunc = psState->psFnInnermost; psFunc != NULL; psFunc = psFunc->psFnNestOuter)
	{
		SubstituteConstantCallParametersFunc(psState, psFunc);
	}
}

static
IMG_VOID ExpandCallInstruction(PINTERMEDIATE_STATE psState, PINST psCallInst)
/*********************************************************************************
 Function			: ExpandCallInstruction

 Description		: Expand a call instruction so each input to the function is
					  defined at each call site and each output is used at each
					  call site.
 
 Parameters			: psState		- Current compiler state.
					  psCallInst	- Call instruction to expand.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PFUNC	psTarget;

	ASSERT(psCallInst->eOpcode == ICALL);

	psTarget = psCallInst->u.psCall->psTarget;

	ASSERT(psCallInst->uArgumentCount == psTarget->sIn.uCount);
	ASSERT(psCallInst->uDestCount == psTarget->sOut.uCount);

	/*
		Insert MOV instructions after the call site to copy to each destination to the CALL from the temporary
		register where the function body writes that output.
	*/
	CopyFunctionOutputs(psState, psTarget, psCallInst);

	/*
		Insert MOV instructions before the call site to copy from each source argument to the CALL to the
		temporary register where the function body reads that input.
	*/
	CopyFunctionInputs(psState, psTarget, psCallInst);

	/*
		Clear the function source and destination arrays.
	*/
	SetDestCount(psState, psCallInst, 0 /* uDestCount */);
	SetArgumentCount(psState, psCallInst, 0 /* uArgumentCount */);
}

IMG_INTERNAL
IMG_VOID ExpandCallInstructions(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: ExpandCallInstructions

 Description		: Expand call instructions so each input to the function is
					  defined at each call site and each output is used at each
					  call site.
 
 Parameters			: psState - Current compiler state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PFUNC			psFunc;

	/*
		For all CALL instructions in the program.
	*/
	ForAllInstructionsOfType(psState, ICALL, ExpandCallInstruction);

	/*
		If we've add to insert extra basic blocks (to maintain the invariant that each call instruction
		has its own basic block) then update the block structure.
	*/
	MergeAllBasicBlocks(psState);

	/*
		Free input/output information for all functions.
	*/
	for (psFunc = psState->psFnInnermost; psFunc != NULL; psFunc = psFunc->psFnNestOuter)
	{
		IMG_UINT32	i;

		for (i = 0; i < psFunc->sOut.uCount; i++)
		{
			UseDefDropFuncOutput(psState, psFunc, i);
		}
		UscFree(psState, psFunc->sOut.asArray);
		UscFree(psState, psFunc->sOut.asArrayUseDef);
		psFunc->sOut.asArray = NULL;
		psFunc->sOut.uCount = 0;

		for (i = 0; i < psFunc->sIn.uCount; i++)
		{
			UseDefDropFuncInput(psState, psFunc, i);
		}
		UscFree(psState, psFunc->sIn.asArray);
		UscFree(psState, psFunc->sIn.asArrayUseDef);
		psFunc->sIn.asArray = NULL;
		psFunc->sIn.uCount = 0;
	}
}

static
IMG_VOID SetArgumentArrayForFunction(PFUNC_INOUT_ARRAY		psParams,
									 PARG					asArgs)
/*********************************************************************************
 Function			: SetArgumentArrayForFunction

 Description		: Set the source or destination array for a CALL instruction
					  from the set of registers live into/live out of the function.
 
 Parameters			: psParams		- Function parameters.
					  asArg			- Argument array to set.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	i;

	for (i = 0; i < psParams->uCount; i++)
	{
		PARG		psArg = &asArgs[i];
		PFUNC_INOUT	psParam = &psParams->asArray[i];

		InitInstArg(psArg);
		psArg->uType = psParam->uType;
		psArg->uNumber = psParam->uNumber;
		psArg->eFmt = psParam->eFmt;
	}
}


typedef struct _FUNC_USE_DEF_SET
{
	IMG_PUINT32	auTempUse;
	IMG_PUINT32 auPredUse;
	IMG_PUINT32 auTempDef;
	IMG_PUINT32 auPredDef;
} FUNC_USE_DEF_SET, *PFUNC_USE_DEF_SET;

static
IMG_VOID UpdateFuncUseSet(PFUNC_USE_DEF_SET psSet, IMG_UINT32 uType, IMG_UINT32 uNumber)
{
	if (uType == USEASM_REGTYPE_TEMP)
	{
		SetBit(psSet->auTempUse, uNumber, 1);
	}
	else if (uType == USEASM_REGTYPE_PREDICATE)
	{
		SetBit(psSet->auPredUse, uNumber, 1);
	}
}

static
IMG_VOID UpdateFuncUseSetSrc(PFUNC_USE_DEF_SET psSet, PARG psSrc)
{
	UpdateFuncUseSet(psSet, psSrc->uType, psSrc->uNumber);
	UpdateFuncUseSet(psSet, psSrc->uIndexType, psSrc->uIndexNumber);
}

static
IMG_VOID UpdateFuncUseDefSetBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvSet)
{
	PFUNC_USE_DEF_SET	psSet = (PFUNC_USE_DEF_SET)pvSet;
	PINST				psInst;

	PVR_UNREFERENCED_PARAMETER(psState);

	for (psInst = psBlock->psBody; psInst != NULL; psInst = psInst->psNext)
	{
		IMG_UINT32	uDestIdx;
		IMG_UINT32	uSrcIdx;
		IMG_UINT32	uPred;

		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG	psDest = &psInst->asDest[uDestIdx];
			PARG	psOldDest = psInst->apsOldDest[uDestIdx];

			if (psDest->uType == USEASM_REGTYPE_TEMP)
			{
				SetBit(psSet->auTempDef, psDest->uNumber, 1);
			}
			else if (psDest->uType == USEASM_REGTYPE_PREDICATE)
			{
				SetBit(psSet->auPredDef, psDest->uNumber, 1);
			}
			if (psOldDest != NULL)
			{
				UpdateFuncUseSetSrc(psSet, psOldDest);
			}
		}
		for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
		{
			UpdateFuncUseSetSrc(psSet, &psInst->asArg[uSrcIdx]);
		}
		for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
		{
			if (psInst->apsPredSrc[uPred] != NULL)
			{
				UpdateFuncUseSetSrc(psSet, psInst->apsPredSrc[uPred]);
			}
		}
	}

	if (psBlock->eType == CBTYPE_COND)
	{
		UpdateFuncUseSetSrc(psSet, &psBlock->u.sCond.sPredSrc);
	}
	else if (psBlock->eType == CBTYPE_SWITCH)
	{
		UpdateFuncUseSetSrc(psSet, psBlock->u.sSwitch.psArg);
	}
}

static
IMG_UINT32 CopyLiveRegisters(PINTERMEDIATE_STATE	psState,
							 PREGISTER_LIVESET		psLiveset,
							 IMG_UINT32				auUsedSet[],
							 IMG_UINT32				uRegType,
							 IMG_UINT32				uMaxRegNum,
							 PFUNC_INOUT			asParams,
							 PVAR_DATA				asVarsData)
{
	IMG_UINT32	uRegCount;
	IMG_UINT32	uRegNum;

	uRegCount = 0;
	for (uRegNum = 0; uRegNum < uMaxRegNum; uRegNum++)
	{
		IMG_UINT32	uLiveChanMask;
		if (GetBit(auUsedSet, uRegNum))
		{
			uLiveChanMask = GetRegisterLiveMask(psState, psLiveset, uRegType, uRegNum, 0 /* uArrayOffset */);
			if (uLiveChanMask != 0)
			{
				if (asParams != NULL)
				{
					asParams[uRegCount].uType = uRegType;
					asParams[uRegCount].uNumber = uRegNum;
					asParams[uRegCount].eFmt = asVarsData[uRegNum].eFmt;
					#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
					asParams[uRegCount].bVector = asVarsData[uRegNum].bVector;
					#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
					asParams[uRegCount].uChanMask = uLiveChanMask;
				}
				uRegCount++;
			}
		}
	}
	return uRegCount;
}	


static
IMG_VOID SetupFunctionInOuts(PINTERMEDIATE_STATE	psState, 
							 PFUNC					psFunc,
							 PVARS_DATA				psVars,
							 PREGISTER_LIVESET		psLiveset,
							 IMG_PUINT32			auTempUsedSet,
							 IMG_PUINT32			auPredUsedSet,
							 PFUNC_INOUT_ARRAY		psParams,
							 USEDEF_TYPE			eUseDefType)
/*********************************************************************************
 Function			: SetupFunctionInOuts

 Description		: Initialize the array describing the function inputs or outputs.
 
 Parameters			: psState	- Current compiler state.	
					  psFunc	- Function to initialize for.
					  psVars	- Information about each intermediate registers in
								the program.
					  psLiveset	- Set of registers live into/out of the function.
					  auTempUsedSet
								- Bit array. The bit corresponding to a temporary
								register number is set if that temporary is
								used or defined inside the function.
					  auPredUsedSet
								- Bit array. The bit corresponding to a predicate
								register number is set if that predicate is used
								or defined inside the function.
					  psParams	- Information about inputs/outputs to be initialized.
					  eUseDefType
								- DEF_TYPE_FUNCINPUT to initialize function inputs.
								  USE_TYPE_FUNCOUTPUT to initialize function outputs.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uTempRegCount;
	IMG_UINT32	uPredRegCount;
	IMG_UINT32	uRegCount;
	IMG_UINT32	uRegIdx;

	uTempRegCount = CopyLiveRegisters(psState, 
									  psLiveset, 
									  auTempUsedSet, 
									  USEASM_REGTYPE_TEMP, 
									  psState->uNumRegisters, 
									  NULL /* asParams */,
									  NULL /* asVarData */);
	uPredRegCount = CopyLiveRegisters(psState, 
									  psLiveset, 
									  auPredUsedSet, 
									  USEASM_REGTYPE_PREDICATE, 
									  psState->uNumPredicates, 
									  NULL /* asParams */,
									  NULL /* asVarData */);
	uRegCount = uTempRegCount + uPredRegCount;

	psParams->asArray = UscAlloc(psState, sizeof(psParams->asArray[0]) * uRegCount);
	psParams->asArrayUseDef = UscAlloc(psState, sizeof(psParams->asArrayUseDef[0]) * uRegCount);
	for (uRegIdx = 0; uRegIdx < uRegCount; uRegIdx++)
	{
		UseDefReset(&psParams->asArrayUseDef[uRegIdx], eUseDefType, uRegIdx, psFunc);
	}

	CopyLiveRegisters(psState, 
					  psLiveset, 
					  auTempUsedSet, 
					  USEASM_REGTYPE_TEMP, 
					  psState->uNumRegisters, 
					  psParams->asArray,
					  psVars->asTemp);
	CopyLiveRegisters(psState, 
					  psLiveset, 
					  auPredUsedSet, 
					  USEASM_REGTYPE_PREDICATE, 
					  psState->uNumPredicates, 
					  psParams->asArray + uTempRegCount,
					  psVars->asPred);

	psParams->uCount = uRegCount;
}

IMG_INTERNAL
IMG_VOID ReleaseVRegisterInfo(PINTERMEDIATE_STATE	psState)
/*********************************************************************************
 Function			: ReleaseVRegisterInfo

 Description		: .
 
 Parameters			: psState - Current compiler state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32	uIdx;

	for (uIdx = 0; uIdx < 2; uIdx++)
	{
		IMG_UINT32	uReg;
		IMG_UINT32	uCount;
		USC_PARRAY	psArray;

		if (uIdx == 0)
		{
			psArray = psState->psTempVReg;
			uCount = psState->uNumRegisters;
		}
		else
		{
			psArray = psState->psPredVReg;
			uCount = psState->uNumPredicates;
		}

		for (uReg = 0; uReg < uCount; uReg++)
		{
			PVREGISTER	psVReg;

			psVReg = ArrayGet(psState, psArray, uReg);
			if (psVReg != NULL)
			{
				UscFree(psState, psVReg);
				ArraySet(psState, psArray, uReg, NULL);
			}
		}
	}

	FreeArray(psState, &psState->psPredVReg);
	psState->psPredVReg = NULL;
	FreeArray(psState, &psState->psTempVReg);
	psState->psTempVReg = NULL;

}

static
IMG_VOID SetLiveChansInDeltaInstDest(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: SetLiveChansInDeltaInstDest

 Description		: 
 
 Parameters			: psState - Current compiler state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	SAFE_LIST_ITERATOR	sIter;
	USC_LIST			sProcessList;
	PUSC_LIST_ENTRY		psListEntry;

	InitializeList(&sProcessList);

	/*
		Set the mask of live channels in the result of each SSA choice function
		to zero.
	*/
	InstListIteratorInitialize(psState, IDELTA, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psInst;

		psInst = InstListIteratorCurrent(&sIter);
		
		ASSERT(psInst->uDestCount == 1);
		psInst->auLiveChansInDest[0] = 0;

		AppendToList(&sProcessList, &psInst->sAvailableListEntry);
		SetBit(psInst->auFlag, INST_LOCAL0, 1);
	}
	InstListIteratorFinalise(&sIter);

	/*
		Calculate the mask of live channels for each SSA choice function.
	*/
	while ((psListEntry = RemoveListHead(&sProcessList)) != NULL)
	{
		PINST		psInst;
		IMG_UINT32	uSrc;

		psInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
		SetBit(psInst->auFlag, INST_LOCAL0, 0);

		ASSERT(psInst->uDestCount == 1);
		psInst->auLiveChansInDest[0] = UseDefGetUsedChanMask(psState, psInst->asDest[0].psRegister->psUseDefChain);

		/*
			If any sources to the instruction are themselves defined by an SSA choice function then
			increase the mask of channels used from the result of that instruction.
		*/
		for (uSrc = 0; uSrc < psInst->uArgumentCount; uSrc++)
		{
			PARG	psSrc;

			psSrc = &psInst->asArg[uSrc];
			if (psSrc->uType == USEASM_REGTYPE_TEMP || psSrc->uType == USEASM_REGTYPE_PREDICATE)
			{
				PINST		psDefInst;
				IMG_UINT32	uDefDestIdx;

				psDefInst = UseDefGetDefInstFromChain(psSrc->psRegister->psUseDefChain, &uDefDestIdx);
				if (psDefInst != NULL && psDefInst->eOpcode == IDELTA)
				{
					ASSERT(uDefDestIdx == 0);

					ASSERT(psDefInst->eOpcode == IDELTA);
					if ((psInst->auLiveChansInDest[0] & ~psDefInst->auLiveChansInDest[0]) != 0)
					{
						psDefInst->auLiveChansInDest[0] |= psInst->auLiveChansInDest[0];

						/*
							Reprocess the modified instruction.
						*/
						if (GetBit(psDefInst->auFlag, INST_LOCAL0) == 0)
						{
							AppendToList(&sProcessList, &psDefInst->sAvailableListEntry);
							SetBit(psDefInst->auFlag, INST_LOCAL0, 1);
						}
					}
				}
			}
		}
	}

	/*
		Drop any SSA choice functions whose results are completely unused.
	*/
	InstListIteratorInitialize(psState, IDELTA, &sIter);
	for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
	{
		PINST	psInst;

		psInst = InstListIteratorCurrent(&sIter);
		
		ASSERT(psInst->uDestCount == 1);
		if (psInst->auLiveChansInDest[0] == 0)
		{
			RemoveInst(psState, psInst->psBlock, psInst);
			FreeInst(psState, psInst);
		}
	}
	InstListIteratorFinalise(&sIter);
}

IMG_INTERNAL 
IMG_VOID ValueNumbering(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: ValueNumbering

 Description		: Transform the program to SSA form.
 
 Parameters			: psState - Current compiler state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PFUNC				psFunc;
	IMG_UINT32			uOrigNumTemps;
	IMG_UINT32			uOrigNumPreds;
	FUNC_USE_DEF_SET	sFuncUseDefSet;
	VARS_DATA			sVars;

	for (psFunc = psState->psFnInnermost; psFunc != NULL; psFunc = psFunc->psFnNestOuter)
	{
		InsertBlocksBeforeNonMergableBlocks(psState, &(psFunc->sCfg));
		CalcDoms(psState, &(psFunc->sCfg));
	}

	uOrigNumTemps = psState->uNumRegisters;
	uOrigNumPreds = psState->uNumPredicates;

	/*
		For each variable: create a list of blocks where it is defined.
	*/
	SetupDefSites(psState, &sVars, uOrigNumTemps, uOrigNumPreds);

	/*
		Create sets of registers live into and live out of each subfunction.
	*/
	sFuncUseDefSet.auTempDef = UscAlloc(psState, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uOrigNumTemps));
	sFuncUseDefSet.auTempUse = UscAlloc(psState, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uOrigNumTemps));
	sFuncUseDefSet.auPredDef = UscAlloc(psState, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uOrigNumPreds));
	sFuncUseDefSet.auPredUse = UscAlloc(psState, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uOrigNumPreds));
	for (psFunc = psState->psFnInnermost; psFunc != NULL; psFunc = psFunc->psFnNestOuter)
	{
		PINST	psCallInst;

		if (psFunc == psState->psMainProg || psFunc == psState->psSecAttrProg)
		{
			continue;
		}

		memset(sFuncUseDefSet.auTempDef, 0, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uOrigNumTemps));
		memset(sFuncUseDefSet.auTempUse, 0, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uOrigNumTemps));
		memset(sFuncUseDefSet.auPredDef, 0, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uOrigNumPreds));
		memset(sFuncUseDefSet.auPredUse, 0, sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uOrigNumPreds));

		DoOnCfgBasicBlocks(psState,
						   &(psFunc->sCfg),
						   ANY_ORDER,
						   UpdateFuncUseDefSetBP,
						   IMG_TRUE /* bHandlesCalls */,
						   &sFuncUseDefSet);

		/* Create the set of registers live in. */
		SetupFunctionInOuts(psState, 
							psFunc,
							&sVars,
							&psFunc->sCallStartRegistersLive,
							sFuncUseDefSet.auTempUse,
							sFuncUseDefSet.auPredUse,
							&psFunc->sIn,
							DEF_TYPE_FUNCINPUT);

		/* Create the set of registers live out. */
		SetupFunctionInOuts(psState,
							psFunc,
							&sVars,
							&psFunc->sCfg.psExit->sRegistersLiveOut,
							sFuncUseDefSet.auTempDef,
							sFuncUseDefSet.auPredDef,
							&psFunc->sOut,
							USE_TYPE_FUNCOUTPUT);

		/*
			Change CALL instruction so their destination array is the set of registers live out of the
			called function and their source array is set of registers live into the called function.
		*/
		for (psCallInst = psFunc->psCallSiteHead; psCallInst != NULL; psCallInst = psCallInst->u.psCall->psCallSiteNext)
		{
			PCALL_PARAMS	psCallParams;
			IMG_UINT32		uDestIdx;

			ASSERT(psCallInst->eOpcode == ICALL);
			ASSERT(psCallInst->uArgumentCount == 0);
			ASSERT(psCallInst->uDestCount == 0);

			psCallParams = psCallInst->u.psCall;

			ASSERT(psCallParams->psTarget == psFunc);

			/*
				Set the function inputs as the arguments to the CALL instruction.
			*/
			SetArgumentCount(psState, psCallInst, psFunc->sIn.uCount);
			SetArgumentArrayForFunction(&psFunc->sIn, psCallInst->asArg);

			/*
				Set the function outputs as the destinations to the CALL instruction.
			*/
			SetDestCount(psState, psCallInst, psFunc->sOut.uCount);
			SetArgumentArrayForFunction(&psFunc->sOut, psCallInst->asDest);

			/*
				Add the CALL instruction as a definition site for the function outputs.
			*/
			for (uDestIdx = 0; uDestIdx < psCallInst->uDestCount; uDestIdx++)
			{
				PDEF_SITE	psDestSite;
				PARG		psDest = &psCallInst->asDest[uDestIdx];
				PVAR_DATA	psDestVar;
				IMG_BOOL	bFound;

				if (psDest->uType == USEASM_REGTYPE_TEMP)
				{
					psDestVar = &sVars.asTemp[psDest->uNumber];
				}
				else
				{
					ASSERT(psDest->uType == USEASM_REGTYPE_PREDICATE);
					psDestVar = &sVars.asPred[psDest->uNumber];
				}

				bFound = IMG_FALSE;
				for (psDestSite = psDestVar->psDefSites; psDestSite != NULL; psDestSite = psDestSite->psNext)
				{
					if (psDestSite->psBlock == psCallInst->psBlock)
					{
						bFound = IMG_TRUE;
					}
				}

				if (!bFound)
				{
					PDEF_SITE	psNewDefSite;

					psNewDefSite = UscAlloc(psState, sizeof(*psNewDefSite));
					psNewDefSite->psBlock = psCallInst->psBlock;
					psNewDefSite->psNext = psDestVar->psDefSites;
					psDestVar->psDefSites = psNewDefSite;
				}
			}
		}
	}
	UscFree(psState, sFuncUseDefSet.auTempDef);
	sFuncUseDefSet.auTempDef = NULL;
	UscFree(psState, sFuncUseDefSet.auTempUse);
	sFuncUseDefSet.auTempUse = NULL;
	UscFree(psState, sFuncUseDefSet.auPredDef);
	sFuncUseDefSet.auPredDef = NULL;
	UscFree(psState, sFuncUseDefSet.auPredUse);
	sFuncUseDefSet.auPredUse = NULL;

	TESTONLY_PRINT_INTERMEDIATE("Setup Function Inputs/Outputs", "func_inout", IMG_FALSE);

	psState->uNumRegisters = 0;
	psState->uNumPredicates = 0;
	psState->psTempVReg = NewArray(psState, USC_MIN_ARRAY_CHUNK, NULL /* pvDefault */, sizeof(PVREGISTER));
	psState->psPredVReg = NewArray(psState, USC_MIN_ARRAY_CHUNK, NULL /* pvDefault */, sizeof(PVREGISTER));

	/*
		Initialize support for recording USE-DEF chains.
	*/
	UseDefInitialize(psState);

	/* Register arrays */
	if (psState->apsVecArrayReg != NULL)
	{
		IMG_UINT32 uVecIdx;
		for (uVecIdx = 0; uVecIdx < psState->uNumVecArrayRegs; uVecIdx++)
		{
			PUSC_VEC_ARRAY_REG	psArray = psState->apsVecArrayReg[uVecIdx];

			if (psArray != NULL)
			{	
				psArray->uBaseReg = GetNextRegisterCount(psState, psArray->uRegs);
			}
		}
	}

	/*
		Apply the SSA transformation to all functions.
	*/
	psState->uFlags2 |= USC_FLAGS2_SSA_FORM | USC_FLAGS2_IN_SSA_RENAME_PASS;
	for (psFunc = psState->psFnInnermost; psFunc != NULL; psFunc = psFunc->psFnNestOuter)
	{
		ValueNumberingFunc(psState, &sVars, psFunc, uOrigNumTemps, uOrigNumPreds);
	}
	psState->uFlags2 &= ~USC_FLAGS2_IN_SSA_RENAME_PASS;

	/*
		Free information about pre-SSA variables.
	*/
	FreeVarData(psState, sVars.uOrigNumTemps, sVars.asTemp);
	UscFree(psState, sVars.asTemp);
	sVars.asTemp = NULL;
	FreeVarData(psState, sVars.uOrigNumPreds, sVars.asPred);
	UscFree(psState, sVars.asPred);
	sVars.asPred = NULL;

	TESTONLY_PRINT_INTERMEDIATE("Value numbering (Before dead code elimination)", "value_num_predce", IMG_FALSE);

	/*
		Remove any SSA choice functions whose result is unused.
	*/
	SetLiveChansInDeltaInstDest(psState);

	SubstituteConstantCallParameters(psState);

	TESTONLY_PRINT_INTERMEDIATE("Value numbering", "value_num", IMG_FALSE);
}

/* EOF */
