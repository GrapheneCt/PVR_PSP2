/******************************************************************************
 * Name         : execpred.c
 * Title        : Execution Predicate Instructions generation
 * Created      : March 2011
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
 * $Log: execpred.c $
 *****************************************************************************/
#include "execpred.h"
#include "cfa.h"
#include "cdg.h"
#if defined(EXECPRED)

/******************************************************************************
 FUNCTION		: AppendExecPredInst

 DESCRIPTION	: Append a Execution Predication related instruciton to a CFG 
				  block

 PARAMETERS		: psState		- Compiler intermediate state
				  psBlock		- CFG block in which to append the instruction				 
				  eOpcode		- Opcode of the new instruction				 
			      uCondPred		- Index of the intermediate predicate controlling
								  conditional execution (result of previous test)
								  Not used for ICNDEND;
				  bCondPredInv	- Whether the state of the predicate should be
								  inverted. Not used for ICNDEND.				  
				  uLoopPred		- Index of the intermediate predicate to
								  indicate whether a loop is complete. Used
								  for ICNDLT only.

 RETURNS		: IMG_VOID
 
 OUTPUT			: Nothing
******************************************************************************/
static
IMG_VOID AppendExecPredInst(
							PINTERMEDIATE_STATE psState,
							PCODEBLOCK psBlock,
							IOPCODE eOpcode,
							IMG_UINT32 uCondPred,
							IMG_BOOL bCondPredInv,
							IMG_UINT32 uLoopPred, 
							IMG_UINT32 uAdjust)
{
	PINST	psExecPredInst;
	IMG_UINT32 uCurrSrc = 0;

	if (psState->uExecPredTemp == USC_UNDEF)
	{
		psState->uExecPredTemp = GetNextRegister(psState);		
	}	

	/*
		Create and append the CNDx instruction to the block
	*/
	psExecPredInst = AllocateInst(psState, NULL);
	if(eOpcode == ICNDLT || eOpcode == ICNDEND)
	{
		SetOpcodeAndDestCount(psState, psExecPredInst, eOpcode, 2);
	}
	else
	{
		SetOpcodeAndDestCount(psState, psExecPredInst, eOpcode, 1);
	}
	
	SetDest(psState, psExecPredInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, psState->uExecPredTemp, UF_REGFORMAT_F32);
	
	if	(eOpcode == ICNDLT)
	{
		MakePredicateDest(psState, psExecPredInst, 1, uLoopPred);
	}
	else if	(eOpcode == ICNDEND)
	{
		psExecPredInst->asDest[1].uType = USC_REGTYPE_DUMMY;
	}

	SetSrc(psState, psExecPredInst, uCurrSrc, USEASM_REGTYPE_TEMP, psState->uExecPredTemp, UF_REGFORMAT_F32);
	uCurrSrc++;

	if (eOpcode != ICNDEND)
	{
		if(uCondPred != USC_PREDREG_NONE)
		{
			MakePredicateSource(psState, psExecPredInst, uCurrSrc, uCondPred);			
		}
		else
		{
			psExecPredInst->asArg[uCurrSrc].uType	= USEASM_REGTYPE_IMMEDIATE;
			psExecPredInst->asArg[uCurrSrc].uNumber = 0;			
		}
		uCurrSrc++;

		psExecPredInst->asArg[uCurrSrc].uType	= USEASM_REGTYPE_IMMEDIATE;			
		if(bCondPredInv)
		{			
			psExecPredInst->asArg[uCurrSrc].uNumber = 1;
		}
		else
		{
			psExecPredInst->asArg[uCurrSrc].uNumber = 0;
		}
		uCurrSrc++;		
	}
	{
		psExecPredInst->asArg[uCurrSrc].uType = USEASM_REGTYPE_IMMEDIATE;
		if ((eOpcode == ICNDLT) || (eOpcode == ICNDSTLOOP) || (eOpcode == ICNDEFLOOP) || (eOpcode == ICNDSM))
		{
			
			psExecPredInst->asArg[uCurrSrc].uNumber = uAdjust;
		}
		else
		{
			psExecPredInst->asArg[uCurrSrc].uNumber = 1;
		}
	}
	AppendInst(psState, psBlock, psExecPredInst);
}

static
PCFG DuplicateCfg(PINTERMEDIATE_STATE psState, PCFG psCfg);

static 
PCODEBLOCK DuplicateCodeBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PCFG psCfg)
{	
	PINST psCurrInst;
	PCODEBLOCK psDuplicateCodeBlock = AllocateBlock(psState, psCfg);
	if (psCodeBlock->uFlags & USC_CODEBLOCK_CFG)
	{
		psDuplicateCodeBlock->u.sUncond.psCfg = DuplicateCfg(psState, psCfg);
		psDuplicateCodeBlock->uFlags |= USC_CODEBLOCK_CFG;
	}
	for (psCurrInst = psCodeBlock->psBody; psCurrInst != NULL; psCurrInst = psCurrInst->psNext)
	{
		PINST psDuplicateInst = CopyInst(psState, psCurrInst);
		AppendInst(psState, psDuplicateCodeBlock, psDuplicateInst);
	}
	CopyRegLiveSet(psState, &psCodeBlock->sRegistersLiveOut, &psDuplicateCodeBlock->sRegistersLiveOut);
	return psDuplicateCodeBlock;
}

static
PCODEBLOCK DuplicateCfgBlocks(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PCFG psDuplicateCfg, CODEBLOCK ** ppsDuplicateBlockRefs)
{
	if (ppsDuplicateBlockRefs[psCodeBlock->uIdx] == NULL)
	{
		PCODEBLOCK psDuplicateBlock = NULL;
		switch (psCodeBlock->eType)
		{
			case CBTYPE_UNCOND:
			{
				PCODEBLOCK psDuplicateSuccBlock;
				psDuplicateSuccBlock = DuplicateCfgBlocks(psState, psCodeBlock->asSuccs[0].psDest, psDuplicateCfg, ppsDuplicateBlockRefs);
				psDuplicateBlock = DuplicateCodeBlock(psState, psCodeBlock, psDuplicateCfg);
				SetBlockUnconditional(psState, psDuplicateBlock, psDuplicateSuccBlock);
				break;
			}
			case CBTYPE_COND:
			{
				PCODEBLOCK psDuplicateTrueSuccBlock;
				PCODEBLOCK psDuplicateFalseSuccBlock;
				psDuplicateTrueSuccBlock = DuplicateCfgBlocks(psState, psCodeBlock->asSuccs[0].psDest, psDuplicateCfg, ppsDuplicateBlockRefs);
				psDuplicateFalseSuccBlock = DuplicateCfgBlocks(psState, psCodeBlock->asSuccs[1].psDest, psDuplicateCfg, ppsDuplicateBlockRefs);
				psDuplicateBlock = DuplicateCodeBlock(psState, psCodeBlock, psDuplicateCfg);
				if (psCodeBlock->u.sCond.sPredSrc.uType != USC_REGTYPE_EXECPRED)
				{
					SetBlockConditional(psState, psDuplicateBlock, psCodeBlock->u.sCond.sPredSrc.uNumber, psDuplicateTrueSuccBlock, psDuplicateFalseSuccBlock, psCodeBlock->bStatic);
				}
				else
				{
					SetBlockConditionalExecPred(psState, psDuplicateBlock, psDuplicateTrueSuccBlock, psDuplicateFalseSuccBlock, psCodeBlock->bStatic);
				}
				break;
			}
			case CBTYPE_EXIT:
			{
				psDuplicateBlock = psDuplicateCfg->psExit;
				break;
			}
			default:
			{
				imgabort();
			}
		}
		ppsDuplicateBlockRefs[psCodeBlock->uIdx] = psDuplicateBlock;
		return psDuplicateBlock;
	}
	else
	{
		return ppsDuplicateBlockRefs[psCodeBlock->uIdx];
	}
}

static
PCFG DuplicateCfg(PINTERMEDIATE_STATE psState, PCFG psCfg)
{
	ASSERT(psCfg->uNumBlocks > 0);
	{
		IMG_UINT32 uBlockIdx;
		PCFG psDuplicateCfg = AllocateCfg(psState, psCfg->psFunc);
		CODEBLOCK ** ppsDuplicateBlockRefs = UscAlloc(psState, (psCfg->uNumBlocks) * sizeof(PCODEBLOCK));
		//not using memset
		for (uBlockIdx = 0; uBlockIdx < psCfg->uNumBlocks; uBlockIdx++)
		{
			ppsDuplicateBlockRefs[uBlockIdx] = NULL;
		}
		psDuplicateCfg->psEntry = DuplicateCfgBlocks(psState, psCfg->psEntry, psDuplicateCfg, ppsDuplicateBlockRefs);
		UscFree(psState, ppsDuplicateBlockRefs);
		return psDuplicateCfg;
	}
}

static
PCODEBLOCK DuplicateCodeBlockIfNeeded(PINTERMEDIATE_STATE psState, IMG_BOOL bDuplicateCodeBlock, PCODEBLOCK psCodeBlock)
{
	if (bDuplicateCodeBlock == IMG_FALSE)
	{
		return psCodeBlock;
	}
	else
	{
		return DuplicateCodeBlock(psState, psCodeBlock, psCodeBlock->psOwner);
	}
}

static
IMG_VOID LayoutCfgFromCtrlDepNode(PINTERMEDIATE_STATE psState, PCTRL_DEP_NODE psCtrlDepNode, PCFG psCfg, PCODEBLOCK* ppsEntryBlock, PCODEBLOCK* ppsExitBlock, PCODEBLOCK psCfgActualExitBlock, IMG_BOOL bDuplicateCodeBlocks)
{
	if (psCtrlDepNode->eCtrlDepType == CTRL_DEP_TYPE_BLOCK)
	{
		if (bDuplicateCodeBlocks == IMG_FALSE)
		{
			IMG_UINT32 uLstNodesCount = GetListNodesCount(&(psCtrlDepNode->u.sBlock.sPredLst));
			if (uLstNodesCount > 1)
			{
				PUSC_LIST_ENTRY	psListEntry = RemoveListHead(&(psCtrlDepNode->u.sBlock.sPredLst));
				PCTRL_DEP_NODE_LISTENTRY psCtrlDepNodeLstEntry;
				psCtrlDepNodeLstEntry = IMG_CONTAINING_RECORD(psListEntry, PCTRL_DEP_NODE_LISTENTRY, sListEntry);		
				UscFree(psState, psCtrlDepNodeLstEntry);			
				//printf("duplication needed\n");				
				bDuplicateCodeBlocks = IMG_TRUE;
			}
		}
		if (psCtrlDepNode->u.sBlock.uNumSucc == 0)
		{
			if (psCtrlDepNode->u.sBlock.psBlock != psCfgActualExitBlock)
			{
				(*ppsExitBlock) = (*ppsEntryBlock) = DuplicateCodeBlockIfNeeded(psState, bDuplicateCodeBlocks, psCtrlDepNode->u.sBlock.psBlock);
			}
			else
			{
				PCODEBLOCK psReturnBlock;
				PINST psReturnInst;
				psReturnBlock = AllocateBlock(psState, psCfg);
				psReturnInst = AllocateInst(psState, NULL);
				SetOpcodeAndDestCount(psState, psReturnInst, IRETURN, 0);				
				AppendInst(psState, psReturnBlock, psReturnInst);
				(*ppsExitBlock) = (*ppsEntryBlock) = psReturnBlock;
			}
		}
		else if (psCtrlDepNode->u.sBlock.uNumSucc == 1)
		{
			PCODEBLOCK psSubCfgEntryBlock;
			PCODEBLOCK psSubCfgExitBlock;
			LayoutCfgFromCtrlDepNode(psState, psCtrlDepNode->u.sBlock.apsSucc[0], psCfg, &psSubCfgEntryBlock, &psSubCfgExitBlock, psCfgActualExitBlock, bDuplicateCodeBlocks);
			{
				PCODEBLOCK psCndExeStBlock = AllocateBlock(psState, psCfg);
				PCODEBLOCK psCndExeEndBlock = AllocateBlock(psState, psCfg);
				PCODEBLOCK psCurrCodeBlock;
				AppendExecPredInst(psState, psCndExeStBlock, ICNDST, psCtrlDepNode->u.sBlock.psBlock->u.sCond.sPredSrc.uNumber, psCtrlDepNode->u.sBlock.auSuccIdx[0] == 0 ? IMG_FALSE : IMG_TRUE /* uCondPredInv */, USC_PREDREG_NONE, 1);
				psCurrCodeBlock = DuplicateCodeBlockIfNeeded(psState, bDuplicateCodeBlocks, psCtrlDepNode->u.sBlock.psBlock);
				SetBlockUnconditional(psState, psCurrCodeBlock, psCndExeStBlock);
				AppendExecPredInst(psState, psCndExeEndBlock, ICNDEND, USC_PREDREG_NONE, IMG_FALSE /* uCondPredInv */, USC_PREDREG_NONE, 1);
				SetBlockConditionalExecPred(psState, psCndExeStBlock, psSubCfgEntryBlock, psCndExeEndBlock, IMG_FALSE);
				SetBlockUnconditional(psState, psSubCfgExitBlock, psCndExeEndBlock);
				(*ppsEntryBlock) = psCurrCodeBlock;
				(*ppsExitBlock) = psCndExeEndBlock;
			}
		}
		else
		{
			PCODEBLOCK psSubCfgEntryBlock;
			PCODEBLOCK psSubCfgExitBlock;
			PCODEBLOCK psCndExeEfBlock;
			LayoutCfgFromCtrlDepNode(psState, psCtrlDepNode->u.sBlock.apsSucc[0], psCfg, &psSubCfgEntryBlock, &psSubCfgExitBlock, psCfgActualExitBlock, bDuplicateCodeBlocks);
			psCndExeEfBlock = AllocateBlock(psState, psCfg);
			{
				PCODEBLOCK psCndExeStBlock = AllocateBlock(psState, psCfg);				
				PCODEBLOCK psCurrCodeBlock;
				AppendExecPredInst(psState, psCndExeStBlock, ICNDST, psCtrlDepNode->u.sBlock.psBlock->u.sCond.sPredSrc.uNumber, psCtrlDepNode->u.sBlock.auSuccIdx[0] == 0 ? IMG_FALSE : IMG_TRUE /* uCondPredInv */, USC_PREDREG_NONE, 1);
				psCurrCodeBlock = DuplicateCodeBlockIfNeeded(psState, bDuplicateCodeBlocks, psCtrlDepNode->u.sBlock.psBlock);
				SetBlockUnconditional(psState, psCurrCodeBlock, psCndExeStBlock);
				AppendExecPredInst(psState, psCndExeEfBlock, ICNDEF, USC_PREDREG_NONE, IMG_TRUE /* uCondPredInv */, USC_PREDREG_NONE, 1);
				SetBlockConditionalExecPred(psState, psCndExeStBlock, psSubCfgEntryBlock, psCndExeEfBlock, IMG_FALSE);
				SetBlockUnconditional(psState, psSubCfgExitBlock, psCndExeEfBlock);
				(*ppsEntryBlock) = psCurrCodeBlock;
			}
			LayoutCfgFromCtrlDepNode(psState, psCtrlDepNode->u.sBlock.apsSucc[1], psCfg, &psSubCfgEntryBlock, &psSubCfgExitBlock, psCfgActualExitBlock, bDuplicateCodeBlocks);
			{
				PCODEBLOCK psCndExeEndBlock = AllocateBlock(psState, psCfg);
				AppendExecPredInst(psState, psCndExeEndBlock, ICNDEND, USC_PREDREG_NONE, IMG_FALSE /* uCondPredInv */, USC_PREDREG_NONE, 1);
				SetBlockConditionalExecPred(psState, psCndExeEfBlock, psSubCfgEntryBlock, psCndExeEndBlock, IMG_FALSE);
				SetBlockUnconditional(psState, psSubCfgExitBlock, psCndExeEndBlock);
				(*ppsExitBlock) = psCndExeEndBlock;								
			}
		}
	}
	else
	{
		PCODEBLOCK psPrevSubCfgExitBlock = NULL;
		PCODEBLOCK psSubCfgEntryBlock;
		PCODEBLOCK psSubCfgExitBlock;
		PUSC_LIST_ENTRY	psListEntry;
		PCTRL_DEP_NODE_LISTENTRY psCtrlDepNodeLstEntry;
		for (psListEntry = psCtrlDepNode->u.sRegion.sSuccLst.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			psCtrlDepNodeLstEntry = IMG_CONTAINING_RECORD(psListEntry, PCTRL_DEP_NODE_LISTENTRY, sListEntry);
			LayoutCfgFromCtrlDepNode(psState, psCtrlDepNodeLstEntry->psCtrlDepNode, psCfg, &psSubCfgEntryBlock, &psSubCfgExitBlock, psCfgActualExitBlock, bDuplicateCodeBlocks);
			if (psPrevSubCfgExitBlock == NULL)
			{
				(*ppsEntryBlock) = psSubCfgEntryBlock;
			}
			else
			{
				SetBlockUnconditional(psState, psPrevSubCfgExitBlock, psSubCfgEntryBlock);
			}
			psPrevSubCfgExitBlock = psSubCfgExitBlock;
		}
		(*ppsExitBlock) = psPrevSubCfgExitBlock;
	}
}

static
IMG_VOID LayoutExecPredCfgFromCtrlDepGraph(PINTERMEDIATE_STATE psState, PCTRL_DEP_GRAPH psCtrlDepGraph, PCFG psCfg, PCODEBLOCK psCfgActualExitBlock)
{
	PCODEBLOCK psPrevCfgEntryBlock = psCfg->psEntry;
	PCODEBLOCK psPrevCfgExitBlock = psCfg->psExit;
	PCODEBLOCK psPrevSubCfgExitBlock = NULL;
	PCODEBLOCK psSubCfgEntryBlock;
	PCODEBLOCK psSubCfgExitBlock;
	PUSC_LIST_ENTRY	psListEntry;
	PCTRL_DEP_NODE_LISTENTRY psCtrlDepNodeLstEntry;
	for (psListEntry = psCtrlDepGraph->psRootCtrlDepNode->u.sBlock.apsSucc[0]->u.sRegion.sSuccLst.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psCtrlDepNodeLstEntry = IMG_CONTAINING_RECORD(psListEntry, PCTRL_DEP_NODE_LISTENTRY, sListEntry);
		LayoutCfgFromCtrlDepNode(psState, psCtrlDepNodeLstEntry->psCtrlDepNode, psCfg, &psSubCfgEntryBlock, &psSubCfgExitBlock, psCfgActualExitBlock, IMG_FALSE);
		if (psPrevSubCfgExitBlock == NULL)
		{
			psCfg->psEntry = psSubCfgEntryBlock;
		}
		else
		{
			SetBlockUnconditional(psState, psPrevSubCfgExitBlock, psSubCfgEntryBlock);
		}
		psPrevSubCfgExitBlock = psSubCfgExitBlock;
	}
	if(psCfgActualExitBlock == NULL)
	{
		psCfg->psExit = psPrevSubCfgExitBlock;
	}
	else
	{
		SetBlockUnconditional(psState, psPrevSubCfgExitBlock, psCfgActualExitBlock);
		psCfg->psExit = psCfgActualExitBlock;
	}	
	FreeBlock(psState, psPrevCfgEntryBlock);
	ClearSuccessors(psState, psCfg->psExit);
	FreeBlock(psState, psPrevCfgExitBlock);
	psCfg->psExit->eType = CBTYPE_EXIT;
	psCfg->psEntry->psIDom = NULL;
	psCfg->pfnCurrentSortOrder = NULL;
}

static
IMG_VOID OptimizeFunctionEnd(PINTERMEDIATE_STATE psState, PCODEBLOCK psReturnMergeTail)
{
	IMG_UINT32 	 uPredIdx;
	for (uPredIdx = 0; uPredIdx < psReturnMergeTail->uNumPreds; uPredIdx++)
	{
		PCODEBLOCK psPredBlock = psReturnMergeTail->asPreds[uPredIdx].psDest;
		if (psPredBlock->psBody != NULL && psPredBlock->psBody->eOpcode == IRETURN)
		{
			OptimizeFunctionEnd(psState, psPredBlock);
			RedirectEdgesFromPredecessors(psState, psPredBlock, psReturnMergeTail, IMG_FALSE);
			ClearSuccessors(psState, psPredBlock);
			FreeBlock(psState, psPredBlock);
		}
	}
}

static
IMG_VOID RestructureNoneLoopCfgToExecPred(PINTERMEDIATE_STATE psState, PCFG psCfg, IMG_BOOL bMostOuterLevel)
{
	PCTRL_DEP_GRAPH psCtrlDepGraph;
	PCODEBLOCK psNewEntryBlock;
	PCODEBLOCK psNewExitBlock;
	PCODEBLOCK psCfgActualExitBlock;
	if(psCfg->uNumBlocks > 1)
	{
		psNewEntryBlock = AllocateBlock(psState, psCfg);
		psNewExitBlock = AllocateBlock(psState, psCfg);
		psCfg->psExit->eType = CBTYPE_UNCOND;
		if (bMostOuterLevel == IMG_FALSE)
		{
			psCfgActualExitBlock = NULL;		
		}
		else
		{
			psCfgActualExitBlock = psCfg->psExit;
		}
		SetBlockUnconditional(psState, psCfg->psExit, psNewExitBlock);
		SetBlockConditional(psState, psNewEntryBlock, GetNextPredicateRegister(psState), psCfg->psEntry, psNewExitBlock, IMG_FALSE);
		psCfg->psEntry = psNewEntryBlock;
		psNewExitBlock->eType = CBTYPE_EXIT;	
		psCfg->psExit = psNewExitBlock;
		CalcDoms(psState, psCfg);
		psCfg->bBlockStructureChanged = IMG_FALSE;
		//TESTONLY_PRINT_INTERMEDIATE("After changing program structure", "beforeCtrlDep", IMG_FALSE);
		psCtrlDepGraph = CalcCtrlDep(psState, psCfg->psEntry);
		LayoutExecPredCfgFromCtrlDepGraph(psState, psCtrlDepGraph, psCfg, psCfgActualExitBlock);	
		FreeCtrlDepGraph(psState, &psCtrlDepGraph);
		if (bMostOuterLevel == IMG_TRUE)
		{
			//TESTONLY_PRINT_INTERMEDIATE("After changing program structure", "returns", IMG_FALSE);
			OptimizeFunctionEnd(psState, psCfg->psExit);
		}
	}
}

static
IMG_VOID SetLoopFlowBP(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_PVOID pvUserData)
{
	PVR_UNREFERENCED_PARAMETER(pvUserData);
	if (psBlock->psBody)
	{
		switch (psBlock->psBody->eOpcode)
		{
			case IBREAK:
			{
				if (psBlock->uNumSuccs < 2)
				{
					SetBlockConditionalExecPred(psState, psBlock, psBlock->psOwner->psExit, psBlock->asSuccs[0].psDest, IMG_FALSE);
				}
				break;
			}
			case ICONTINUE:
			{
				if (psBlock->uNumSuccs < 2)
				{
					SetBlockConditionalExecPred(psState, psBlock, psBlock->psOwner->psEntry, psBlock->asSuccs[0].psDest, IMG_FALSE);
				}
				break;
			}
			case ICNDLT:
			{
				SetBlockConditional(psState, psBlock, psBlock->psBody->asDest[1].uNumber, psBlock->psOwner->psEntry, psBlock->asSuccs[0].psDest, IMG_TRUE);
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

static
IMG_VOID SetLoopFlow(PINTERMEDIATE_STATE psState, PCFG psCfg)
{
	DoOnCfgBasicBlocks(psState, psCfg, ANY_ORDER, SetLoopFlowBP, IMG_FALSE, NULL);
}

static
IMG_VOID RestructureLoopToExecPred(PINTERMEDIATE_STATE psState, PLOOP_INFO psLoopInfo, PUSC_LIST psLoopInfoLst)
{
	PCFG psLoopCfg;
	IMG_BOOL bGenerateInfiniteLoop;
	IMG_UINT32 uLoopPredReg;
	IMG_BOOL bInvertLoopPredReg;	
	RestructureLoop(psState, psLoopInfo, &psLoopCfg, &bGenerateInfiniteLoop, &uLoopPredReg, &bInvertLoopPredReg, psLoopInfoLst);
	RestructureNoneLoopCfgToExecPred(psState, psLoopCfg, IMG_FALSE);
	{
		PCODEBLOCK psCndEfLoopBlock = NULL;
		{
			psCndEfLoopBlock = AllocateBlock(psState, psLoopCfg);
			RedirectEdgesFromPredecessors(psState, psLoopCfg->psExit, psCndEfLoopBlock, IMG_FALSE);
			AppendExecPredInst(psState, psCndEfLoopBlock, ICNDEFLOOP, USC_PREDREG_NONE, IMG_TRUE, USC_PREDREG_NONE, 0);				
		}
		{
			PCODEBLOCK psCndExeLtBlock = AllocateBlock(psState, psLoopCfg);
			IMG_UINT32 uPDstLoop = GetNextPredicateRegister(psState);
			AppendExecPredInst(psState, psCndExeLtBlock, ICNDLT, uLoopPredReg, bGenerateInfiniteLoop ? bGenerateInfiniteLoop : bInvertLoopPredReg, uPDstLoop, 2);
			SetBlockUnconditional(psState, psCndEfLoopBlock, psCndExeLtBlock);
			SetBlockUnconditional(psState, psCndExeLtBlock, psLoopCfg->psExit);
		}			
	}	
	SetLoopFlow(psState, psLoopCfg);
	{
		PCODEBLOCK psCndStLoopBlock;
		PCODEBLOCK psOldEntry = psLoopCfg->psEntry;
		psCndStLoopBlock = AllocateBlock(psState, psLoopCfg);
		AppendExecPredInst(psState, psCndStLoopBlock, ICNDSTLOOP, USC_PREDREG_NONE, IMG_TRUE /* uCondPredInv */, USC_PREDREG_NONE, 2);
		psLoopCfg->psEntry = psCndStLoopBlock;
		SetBlockUnconditional(psState, psCndStLoopBlock, psOldEntry);
	}
}

static
IMG_VOID RestructureCfgLoopsToExecPred(PINTERMEDIATE_STATE psState, PFUNC_LOOPS_INFO psFuncLoopsInfo)
{
	PUSC_LIST_ENTRY	psListEntry;
	PLOOP_INFO_LISTENTRY psLoopInfoLstEntry;
	for (psListEntry = psFuncLoopsInfo->sFuncLoopInfoLst.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psLoopInfoLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_INFO_LISTENTRY, sListEntry);
		RestructureLoopToExecPred(psState, &(psLoopInfoLstEntry->sLoopInfo), &(psFuncLoopsInfo->sFuncLoopInfoLst));
	}
}

static
PCODEBLOCK SetBlockBreakAndRetrunLevels(PINTERMEDIATE_STATE psState, PCODEBLOCK psStartBlock, IMG_UINT32 uBrkNestingLevel, IMG_UINT32 uRetNestingLevel, CODEBLOCK** ppsEnabledEndAfterEarlyRetBlock);

static
PCODEBLOCK SetLoopBreakAndRetrunLevels(PINTERMEDIATE_STATE psState, PCODEBLOCK psStartBlock, IMG_UINT32 uRetNestingLevel, CODEBLOCK** ppsEnabledEndAfterEarlyRetBlock)
{
	return SetBlockBreakAndRetrunLevels(psState, psStartBlock->asSuccs[0].psDest, 0, uRetNestingLevel, ppsEnabledEndAfterEarlyRetBlock);
}

static
PCODEBLOCK SetBlockBreakAndRetrunLevels(PINTERMEDIATE_STATE psState, PCODEBLOCK psStartBlock, IMG_UINT32 uBrkNestingLevel, IMG_UINT32 uRetNestingLevel, CODEBLOCK** ppsEnabledEndAfterEarlyRetBlock)
{
	while(psStartBlock->uNumSuccs > 0)
	{
		if (psStartBlock->psBody != NULL)
		{
			switch (psStartBlock->psBody->eOpcode)
			{
				case ICNDST:
				{
					uBrkNestingLevel++;
					uRetNestingLevel++;
					psStartBlock = psStartBlock->asSuccs[0].psDest;
					break;
				}
				case ICNDEF:
				{
					psStartBlock = psStartBlock->asSuccs[0].psDest;
					break;
				}
				case ICNDEND:
				{
					uBrkNestingLevel--;
					uRetNestingLevel--;
					psStartBlock = psStartBlock->asSuccs[0].psDest;
					break;
				}
				case ICNDSTLOOP:
				{
					uRetNestingLevel += 2; 
					psStartBlock = SetLoopBreakAndRetrunLevels(psState, psStartBlock->asSuccs[0].psDest, uRetNestingLevel, ppsEnabledEndAfterEarlyRetBlock);
					uRetNestingLevel -= 2;
					break;
				}
				case ICNDLT:
				{
					return psStartBlock->asSuccs[1].psDest;
				}
				case IBREAK:
				{
					PINST psInstToRemove;
					psInstToRemove = psStartBlock->psBody;
					RemoveInst(psState, psStartBlock, psStartBlock->psBody);
					FreeInst(psState, psInstToRemove);
					AppendExecPredInst(psState, psStartBlock, ICNDSM, USC_PREDREG_NONE, IMG_TRUE, USC_PREDREG_NONE, uBrkNestingLevel + 2);
					psStartBlock->u.sCond.bDontGenBranch = IMG_TRUE;
					psStartBlock = psStartBlock->asSuccs[1].psDest;
					break;
				}
				case ICONTINUE:
				{
					PINST psInstToRemove;
					psInstToRemove = psStartBlock->psBody;
					RemoveInst(psState, psStartBlock, psStartBlock->psBody);
					FreeInst(psState, psInstToRemove);
					AppendExecPredInst(psState, psStartBlock, ICNDSM, USC_PREDREG_NONE, IMG_TRUE, USC_PREDREG_NONE, uBrkNestingLevel + 1);
					psStartBlock = psStartBlock->asSuccs[1].psDest;
					break;
				}
				case IRETURN:
				{
					if (((*ppsEnabledEndAfterEarlyRetBlock) == NULL) && (psStartBlock->psOwner->psExit->psBody != NULL))
					{
						PCODEBLOCK psCndExeEndBlock = AllocateBlock(psState, psStartBlock->psOwner);						
						AppendExecPredInst(psState, psCndExeEndBlock, ICNDEND, USC_PREDREG_NONE, IMG_FALSE /* uCondPredInv */, USC_PREDREG_NONE, 1);
						RedirectEdgesFromPredecessors(psState, psStartBlock->psOwner->psExit, psCndExeEndBlock, IMG_FALSE);
						SetBlockUnconditional(psState, psCndExeEndBlock, psStartBlock->psOwner->psExit);						
						*ppsEnabledEndAfterEarlyRetBlock = psCndExeEndBlock;
					}
					{
						PINST psInstToRemove;
						psInstToRemove = psStartBlock->psBody;
						RemoveInst(psState, psStartBlock, psStartBlock->psBody);
						FreeInst(psState, psInstToRemove);
						AppendExecPredInst(psState, psStartBlock, ICNDSM, USC_PREDREG_NONE, IMG_TRUE, USC_PREDREG_NONE, uRetNestingLevel);
						psStartBlock->u.sCond.bDontGenBranch = IMG_TRUE;
						if ((*ppsEnabledEndAfterEarlyRetBlock) == NULL)
						{
							SetBlockConditionalExecPred(psState, psStartBlock, psStartBlock->psOwner->psExit, psStartBlock->asSuccs[0].psDest, IMG_FALSE);
						}
						else
						{
							SetBlockConditionalExecPred(psState, psStartBlock, *ppsEnabledEndAfterEarlyRetBlock, psStartBlock->asSuccs[0].psDest, IMG_FALSE);
						}
						psStartBlock = psStartBlock->asSuccs[1].psDest;
					}
					break;
				}
				default:
				{
					ASSERT(psStartBlock->uNumSuccs == 1);
					psStartBlock = psStartBlock->asSuccs[0].psDest;
				}
			}
		}
		else
		{
			ASSERT(psStartBlock->uNumSuccs == 1);
			psStartBlock = psStartBlock->asSuccs[0].psDest;
		}
	}
	return psStartBlock;
}

static
IMG_VOID SetBreakAndReturnLevels(PINTERMEDIATE_STATE psState, PCODEBLOCK psStartBlock)
{
	PCODEBLOCK psEnabledEndAfterEarlyRetBlock = NULL;
	SetBlockBreakAndRetrunLevels(psState, psStartBlock, 0, 0, &psEnabledEndAfterEarlyRetBlock);
}

static
IMG_VOID RestructureProgramLoopsToExecPred(PINTERMEDIATE_STATE psState, PUSC_LIST psFuncLoopsInfoLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	PFUNC_LOOPS_INFO_LISTENTRY psFuncLoopsInfoLstEntry;
	for (psListEntry = psFuncLoopsInfoLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psFuncLoopsInfoLstEntry = IMG_CONTAINING_RECORD(psListEntry, PFUNC_LOOPS_INFO_LISTENTRY, sListEntry);		
		RestructureCfgLoopsToExecPred(psState, &(psFuncLoopsInfoLstEntry->sFuncLoopsInfo));
		RestructureNoneLoopCfgToExecPred(psState, &(psFuncLoopsInfoLstEntry->sFuncLoopsInfo.psFunc->sCfg), IMG_TRUE);
		AttachSubCfgs(psState, &(psFuncLoopsInfoLstEntry->sFuncLoopsInfo.psFunc->sCfg));
		SetBreakAndReturnLevels(psState, psFuncLoopsInfoLstEntry->sFuncLoopsInfo.psFunc->sCfg.psEntry);
	}
}

IMG_INTERNAL
IMG_VOID ChangeProgramStructToExecPred(PINTERMEDIATE_STATE psState)
{
	USC_LIST sFuncLoopsInfoLst;
	FindProgramLoops(psState, &sFuncLoopsInfoLst);
	RestructureProgramLoopsToExecPred(psState, &sFuncLoopsInfoLst);
	FreeFuncLoopsInfoList(psState, &sFuncLoopsInfoLst);	
	if (psState->uExecPredTemp != USC_UNDEF)
	{
		PINST psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IMOV);
		SetDest(psState, psInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, psState->uExecPredTemp, UF_REGFORMAT_F32);
		SetSrc(psState, psInst, 0, USEASM_REGTYPE_IMMEDIATE, 0, UF_REGFORMAT_F32);
		InsertInstBefore(psState, psState->psMainProg->sCfg.psEntry,
						 psInst,
						 psState->psMainProg->sCfg.psEntry->psBody);
		{
			SAFE_LIST_ITERATOR	sIter;
			
			InstListIteratorInitialize(psState, ICNDSM, &sIter);
			for (; InstListIteratorContinue(&sIter); InstListIteratorNext(&sIter))
			{
				PINST psCndsmInst;
				IMG_UINT32 uAdjust;
				psCndsmInst = InstListIteratorCurrent(&sIter);
				uAdjust = psCndsmInst->asArg[3].uNumber;
				if (AddStaticSecAttr(psState, uAdjust, NULL /* puArgType */, NULL /* puArgNum */))
				{
					IMG_UINT32	uArgType, uArgNumber;

					/*
						Use the secondary attribute as the instruction source.
					*/
					AddStaticSecAttr(psState, uAdjust, &uArgType, &uArgNumber);
					SetSrc(psState, psCndsmInst, 3, uArgType, uArgNumber, UF_REGFORMAT_F32);
				}
				else
				{
					PINST		psLimmInst;
					IMG_UINT32	uImmTemp = GetNextRegister(psState);
					PCODEBLOCK	psLimmCodeBlock = AllocateBlock(psState, psCndsmInst->psBlock->psOwner);
					
					RedirectEdgesFromPredecessors(psState, psCndsmInst->psBlock, psLimmCodeBlock, IMG_FALSE);
					SetBlockUnconditional(psState, psLimmCodeBlock, psCndsmInst->psBlock);
					/*
						Add a LIMM instruction to load the immediate value into a temporary register.
					*/
					psLimmInst = AllocateInst(psState, psCndsmInst);
					SetOpcode(psState, psLimmInst, ILIMM);
					SetDestTempArg(psState, psLimmInst, 0 /* uDestIdx */, uImmTemp, UF_REGFORMAT_F32);
					psLimmInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
					psLimmInst->asArg[0].uNumber = uAdjust;
					InsertInstBefore(psState, psLimmCodeBlock, psLimmInst, psLimmCodeBlock->psBody);

					/*
						Use the temporary register as the instruction source.
					*/
					SetSourceTempArg(psState, psCndsmInst, 3, uImmTemp, UF_REGFORMAT_F32);
				}				
			}
			InstListIteratorFinalise(&sIter);
		}			
	}
	return;
}
#endif /* #if defined(EXECPRED) */
