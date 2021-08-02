/******************************************************************************
 * Name         : cfa.c
 * Title        : Control flow analysis
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
 * $Log: cfa.c $
 *****************************************************************************/
#include "cfa.h"
#include "uscshrd.h"
//#include "bitops.h"
#if defined(EXECPRED)

typedef struct _BLOCK_LISTENTRY
{	
	PCODEBLOCK psBlock;
	USC_LIST_ENTRY	sListEntry;
} BLOCK_LISTENTRY, *PBLOCK_LISTENTRY;

static
IMG_VOID AppendBlockToList(PINTERMEDIATE_STATE psState, PUSC_LIST psBlockList, PCODEBLOCK psBlock)
{
	PBLOCK_LISTENTRY psBlockListEntry;
	psBlockListEntry = UscAlloc(psState, sizeof(*psBlockListEntry));
	psBlockListEntry->psBlock = psBlock;
	AppendToList(psBlockList, &psBlockListEntry->sListEntry);
}

static
IMG_BOOL IsBlockPresentInList(USC_LIST *psBlockList, PCODEBLOCK psBlock)
{
	PUSC_LIST_ENTRY	psListEntry;
	PBLOCK_LISTENTRY psBlockListEntry;
	for (psListEntry = psBlockList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psBlockListEntry = IMG_CONTAINING_RECORD(psListEntry, PBLOCK_LISTENTRY, sListEntry);
		if (psBlockListEntry->psBlock == psBlock)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID FreeBlockList(PINTERMEDIATE_STATE psState, USC_LIST *psBlockList)
{
	PUSC_LIST_ENTRY	psListEntry;	
	while ((psListEntry = RemoveListHead(psBlockList)) != NULL)
	{
		PBLOCK_LISTENTRY psBlockListEntry;
		psBlockListEntry = IMG_CONTAINING_RECORD(psListEntry, PBLOCK_LISTENTRY, sListEntry);
		UscFree(psState, psBlockListEntry);
	}
}

typedef struct _LOOP_OUT_HEAD_AND_TAIL
{
	EDGE_INFO	sLoopOutHeadEdge;
	EDGE_INFO	sLoopOutTailEdge;
} LOOP_OUT_HEAD_AND_TAIL, *PLOOP_OUT_HEAD_AND_TAIL;

typedef struct _LOOP_OUT_HEAD_AND_TAIL_LISTENTRY
{
	LOOP_OUT_HEAD_AND_TAIL	sLoopOutHeadAndTail;
	USC_LIST_ENTRY	sListEntry;
} LOOP_OUT_HEAD_AND_TAIL_LISTENTRY, *PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY;

static
PLOOP_OUT_HEAD_AND_TAIL AppendLoopOutHeadAndTailToList(PINTERMEDIATE_STATE psState, PUSC_LIST psLoopOutHeadAndTailLst)
{
	PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY psLoopOutHeadAndTailLstEntry;
	psLoopOutHeadAndTailLstEntry = UscAlloc(psState, sizeof(*psLoopOutHeadAndTailLstEntry));
	AppendToList(psLoopOutHeadAndTailLst, &psLoopOutHeadAndTailLstEntry->sListEntry);
	psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock = NULL;
	psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.psEdgeParentBlock = NULL;
	return &(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail);
}

static
IMG_BOOL IsLoopOutHeadAndTailPresentInList(PUSC_LIST psLoopOutHeadAndTailLst, PCODEBLOCK psHeadEdgeParentBlock, IMG_UINT32 uHeadEdgeSuccIdx, PCODEBLOCK psTailEdgeParentBlock, IMG_UINT32 uTailEdgeSuccIdx)
{	
	PUSC_LIST_ENTRY	psListEntry;
	PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY psLoopOutHeadAndTailLstEntry;
	for (psListEntry = psLoopOutHeadAndTailLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psLoopOutHeadAndTailLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY, sListEntry);
		if	(
				(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock == psHeadEdgeParentBlock)
				&&
				(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.uEdgeSuccIdx == uHeadEdgeSuccIdx)
				&&
				(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.psEdgeParentBlock == psTailEdgeParentBlock)
				&&
				(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.uEdgeSuccIdx == uTailEdgeSuccIdx)
			)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID FreeLoopOutHeadAndTailList(PINTERMEDIATE_STATE psState, PUSC_LIST psLoopOutHeadAndTailLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	while ((psListEntry = RemoveListHead(psLoopOutHeadAndTailLst)) != NULL)
	{
		PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY psLoopOutHeadAndTailLstEntry;
		psLoopOutHeadAndTailLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY, sListEntry);
		UscFree(psState, psLoopOutHeadAndTailLstEntry);
	}
}

typedef struct _LOOP_OUT_FLOW
{
	PCODEBLOCK	psLoopOutDestBlock;
	USC_LIST	sLoopOutHeadAndTailLst;
} LOOP_OUT_FLOW, *PLOOP_OUT_FLOW;

typedef struct _LOOP_OUT_FLOW_LISTENTRY
{
	LOOP_OUT_FLOW	sLoopOutFlow;
	USC_LIST_ENTRY	sListEntry;
} LOOP_OUT_FLOW_LISTENTRY, *PLOOP_OUT_FLOW_LISTENTRY;

static
PLOOP_OUT_FLOW AppendLoopOutFlowToList(PINTERMEDIATE_STATE psState, PUSC_LIST psLoopOutFlowLst)
{
	PLOOP_OUT_FLOW_LISTENTRY psLoopOutFlowLstEntry;
	psLoopOutFlowLstEntry = UscAlloc(psState, sizeof(*psLoopOutFlowLstEntry));
	AppendToList(psLoopOutFlowLst, &psLoopOutFlowLstEntry->sListEntry);
	psLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock = NULL;
	InitializeList(&(psLoopOutFlowLstEntry->sLoopOutFlow.sLoopOutHeadAndTailLst));
	return &(psLoopOutFlowLstEntry->sLoopOutFlow);
}

static
IMG_BOOL IsLoopOutFlowPresentInList(PUSC_LIST psLoopOutFlowLst, PCODEBLOCK psLoopOutDestBlock, LOOP_OUT_FLOW **ppsLoopOutFlow)
{	
	PUSC_LIST_ENTRY	psListEntry;
	PLOOP_OUT_FLOW_LISTENTRY psLoopOutFlowLstEntry;
	for (psListEntry = psLoopOutFlowLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psLoopOutFlowLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_FLOW_LISTENTRY, sListEntry);
		if (psLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock == psLoopOutDestBlock)
		{
			*ppsLoopOutFlow = &(psLoopOutFlowLstEntry->sLoopOutFlow);
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID FreeLoopOutFlowList(PINTERMEDIATE_STATE psState, PUSC_LIST psLoopOutFlowLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	while ((psListEntry = RemoveListHead(psLoopOutFlowLst)) != NULL)
	{
		PLOOP_OUT_FLOW_LISTENTRY psLoopOutFlowLstEntry;
		psLoopOutFlowLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_FLOW_LISTENTRY, sListEntry);
		FreeLoopOutHeadAndTailList(psState, &(psLoopOutFlowLstEntry->sLoopOutFlow.sLoopOutHeadAndTailLst));
		UscFree(psState, psLoopOutFlowLstEntry);
	}
}

static
PLOOP_INFO AppendLoopInfoToList(PINTERMEDIATE_STATE psState, PUSC_LIST psLoopInfoLst)
{
	PLOOP_INFO_LISTENTRY psLoopInfoLstEntry;
	psLoopInfoLstEntry = UscAlloc(psState, sizeof(*psLoopInfoLstEntry));
	AppendToList(psLoopInfoLst, &psLoopInfoLstEntry->sListEntry);
	psLoopInfoLstEntry->sLoopInfo.psLoopHeaderBlock = NULL;
	psLoopInfoLstEntry->sLoopInfo.psEnclosingLoopInfo = NULL;
	InitializeList(&(psLoopInfoLstEntry->sLoopInfo.sLoopOutFlowLst));
	InitializeList(&(psLoopInfoLstEntry->sLoopInfo.sLoopCntEdgeLst));
	return &(psLoopInfoLstEntry->sLoopInfo);
}

static
IMG_BOOL IsLoopInfoPresentInList(USC_LIST *psLoopInfoLst, PCODEBLOCK psLoopHeaderBlock)
{	
	PUSC_LIST_ENTRY	psListEntry;
	PLOOP_INFO_LISTENTRY psLoopInfoLstEntry;
	for (psListEntry = psLoopInfoLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psLoopInfoLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_INFO_LISTENTRY, sListEntry);
		if (psLoopInfoLstEntry->sLoopInfo.psLoopHeaderBlock == psLoopHeaderBlock)
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID FreeLoopInfoList(PINTERMEDIATE_STATE psState, PUSC_LIST psLoopInfoLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	while ((psListEntry = RemoveListHead(psLoopInfoLst)) != NULL)
	{
		PLOOP_INFO_LISTENTRY psLoopInfoLstEntry;
		psLoopInfoLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_INFO_LISTENTRY, sListEntry);
		FreeLoopOutFlowList(psState, &(psLoopInfoLstEntry->sLoopInfo.sLoopOutFlowLst));
		FreeEdgeInfoList(psState, &(psLoopInfoLstEntry->sLoopInfo.sLoopCntEdgeLst));
		UscFree(psState, psLoopInfoLstEntry);
	}
}

static
PFUNC_LOOPS_INFO AppendFuncLoopsInfoToList(PINTERMEDIATE_STATE psState, PUSC_LIST psFuncLoopsInfoLst)
{
	PFUNC_LOOPS_INFO_LISTENTRY psFuncLoopsInfoLstEntry;
	psFuncLoopsInfoLstEntry = UscAlloc(psState, sizeof(*psFuncLoopsInfoLstEntry));
	AppendToList(psFuncLoopsInfoLst, &(psFuncLoopsInfoLstEntry->sListEntry));
	psFuncLoopsInfoLstEntry->sFuncLoopsInfo.psFunc = NULL;
	InitializeList(&(psFuncLoopsInfoLstEntry->sFuncLoopsInfo.sFuncLoopInfoLst));
	return &(psFuncLoopsInfoLstEntry->sFuncLoopsInfo);
}

IMG_INTERNAL
IMG_VOID FreeFuncLoopsInfoList(PINTERMEDIATE_STATE psState, PUSC_LIST psFuncLoopsInfoLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	while ((psListEntry = RemoveListHead(psFuncLoopsInfoLst)) != NULL)
	{
		PFUNC_LOOPS_INFO_LISTENTRY psFuncLoopsInfoLstEntry;
		psFuncLoopsInfoLstEntry = IMG_CONTAINING_RECORD(psListEntry, PFUNC_LOOPS_INFO_LISTENTRY, sListEntry);
		FreeLoopInfoList(psState, &(psFuncLoopsInfoLstEntry->sFuncLoopsInfo.sFuncLoopInfoLst));
		UscFree(psState, psFuncLoopsInfoLstEntry);
	}
}

static
IMG_BOOL BlkOrSuccsOutOfEnclosingLoop(PCODEBLOCK psBlock, PCODEBLOCK psLoopHeaderBlock)
{
	PCODEBLOCK psEnclosingLoopHeader = psLoopHeaderBlock->psLoopHeader;
	if (psBlock->psLoopHeader != psEnclosingLoopHeader)
	{
		return IMG_TRUE;
	}
	else
	{
		IMG_UINT32 uSuccIdx;
		for (uSuccIdx = 0; uSuccIdx < psBlock->uNumSuccs; uSuccIdx++)
		{
			if (psBlock->asSuccs[uSuccIdx].psDest->psLoopHeader != psEnclosingLoopHeader)
			{
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static
IMG_VOID AddLoopOutFlowData(PINTERMEDIATE_STATE psState, PUSC_LIST psLoopOutFlowLst, PCODEBLOCK psBlock, IMG_UINT32 uSuccIdx)
{
	PLOOP_OUT_FLOW psLoopOutFlow;
	if (!IsLoopOutFlowPresentInList(psLoopOutFlowLst, psBlock->asSuccs[uSuccIdx].psDest, &psLoopOutFlow))
	{
		PLOOP_OUT_HEAD_AND_TAIL psLoopOutHeadAndTail;
		psLoopOutFlow = AppendLoopOutFlowToList(psState, psLoopOutFlowLst);
		psLoopOutFlow->psLoopOutDestBlock = psBlock->asSuccs[uSuccIdx].psDest;
		psLoopOutHeadAndTail = AppendLoopOutHeadAndTailToList(psState, &(psLoopOutFlow->sLoopOutHeadAndTailLst));
		psLoopOutHeadAndTail->sLoopOutHeadEdge.psEdgeParentBlock = psBlock;
		psLoopOutHeadAndTail->sLoopOutHeadEdge.uEdgeSuccIdx = uSuccIdx;
		psLoopOutHeadAndTail->sLoopOutTailEdge.psEdgeParentBlock = psBlock;
		psLoopOutHeadAndTail->sLoopOutTailEdge.uEdgeSuccIdx = uSuccIdx;
	}
	else
	{
		PLOOP_OUT_HEAD_AND_TAIL psLoopOutHeadAndTail;
		if(!IsLoopOutHeadAndTailPresentInList(&(psLoopOutFlow->sLoopOutHeadAndTailLst), psBlock, uSuccIdx, psBlock, uSuccIdx))
		{
			psLoopOutHeadAndTail = AppendLoopOutHeadAndTailToList(psState, &(psLoopOutFlow->sLoopOutHeadAndTailLst));
			psLoopOutHeadAndTail->sLoopOutHeadEdge.psEdgeParentBlock = psBlock;
			psLoopOutHeadAndTail->sLoopOutHeadEdge.uEdgeSuccIdx = uSuccIdx;
			psLoopOutHeadAndTail->sLoopOutTailEdge.psEdgeParentBlock = psBlock;
			psLoopOutHeadAndTail->sLoopOutTailEdge.uEdgeSuccIdx = uSuccIdx;
		}
	}
}

static
IMG_BOOL PostDominatesSomeLoopExit(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PUSC_LIST psLoopOutFlowLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	PLOOP_OUT_FLOW_LISTENTRY psLoopOutFlowLstEntry;
	for (psListEntry = psLoopOutFlowLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psLoopOutFlowLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_FLOW_LISTENTRY, sListEntry);
		if (PostDominated(psState, psLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock, psBlock))
		{
			return IMG_TRUE;
		}		
	}
	return IMG_FALSE;
}

static
IMG_VOID FindLoopOutFlow(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uSuccIdx, PUSC_LIST psLoopOutFlowLst, PCODEBLOCK psLoopHeaderBlock)
{
	USC_LIST sVisitedBlockList;	
	PUSC_LIST_ENTRY	psListEntry;	
	PCODEBLOCK psDomBlock = NULL;
	IMG_UINT32 auOppositeSucc[2] = {1U, 0U};
	InitializeList(&sVisitedBlockList);
	if	(
			Dominates(psState, psBlock, psBlock->asSuccs[uSuccIdx].psDest)
			&&
			!BlkOrSuccsOutOfEnclosingLoop(psBlock->asSuccs[uSuccIdx].psDest, psLoopHeaderBlock)
			&&			
			(psBlock->asSuccs[uSuccIdx].psDest->uNumSuccs > 0)
			&&
			!Dominates(psState, psBlock->asSuccs[uSuccIdx].psDest, psBlock->psOwner->psFunc->sCfg.psExit)
			&&
			((psBlock->uNumSuccs < 2) || ((psBlock->uNumSuccs > 2) && (psBlock->asSuccs[auOppositeSucc[uSuccIdx]].psDest != psLoopHeaderBlock)))
		)
	{
		AppendBlockToList(psState, &sVisitedBlockList, psBlock->asSuccs[uSuccIdx].psDest);
		psBlock->asSuccs[uSuccIdx].psDest->psLoopHeader = psLoopHeaderBlock;
		psDomBlock = psBlock->asSuccs[uSuccIdx].psDest;		
	}
	else
	{
		AddLoopOutFlowData(psState, psLoopOutFlowLst, psBlock, uSuccIdx);
	}	
	for (psListEntry = sVisitedBlockList.psHead; (psListEntry != NULL); psListEntry = psListEntry->psNext)
	{
		PBLOCK_LISTENTRY psBlockListEntry;
		IMG_UINT32 uSuccIdxIntern;
		psBlockListEntry = IMG_CONTAINING_RECORD(psListEntry, PBLOCK_LISTENTRY, sListEntry);
		for (uSuccIdxIntern = 0; uSuccIdxIntern < psBlockListEntry->psBlock->uNumSuccs; uSuccIdxIntern++)
		{
			if (!IsBlockPresentInList(&sVisitedBlockList, psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest))
			{
				if	(
						Dominates(psState, psDomBlock, psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest)
						&&
						!BlkOrSuccsOutOfEnclosingLoop(psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest, psLoopHeaderBlock)
						&&
						(psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest->uNumSuccs > 0)
						&&
						!PostDominatesSomeLoopExit(psState, psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest, psLoopOutFlowLst)
						&&
						!Dominates(psState, psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest, psBlock->psOwner->psFunc->sCfg.psExit)
						&&
						((psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest->uNumSuccs < 2) || ((psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest->uNumSuccs > 2) && (psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest->asSuccs[auOppositeSucc[uSuccIdx]].psDest != psLoopHeaderBlock)))
					)
				{
					AppendBlockToList(psState, &sVisitedBlockList, psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest);
					psBlock->asSuccs[uSuccIdx].psDest->psLoopHeader = psLoopHeaderBlock;
				}
				else
				{
					PLOOP_OUT_FLOW psLoopOutFlow;
					if (!IsLoopOutFlowPresentInList(psLoopOutFlowLst, psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest, &psLoopOutFlow))
					{
						PLOOP_OUT_HEAD_AND_TAIL psLoopOutHeadAndTail;
						psLoopOutFlow = AppendLoopOutFlowToList(psState, psLoopOutFlowLst);
						psLoopOutFlow->psLoopOutDestBlock = psBlockListEntry->psBlock->asSuccs[uSuccIdxIntern].psDest;
						psLoopOutHeadAndTail = AppendLoopOutHeadAndTailToList(psState, &(psLoopOutFlow->sLoopOutHeadAndTailLst));
						psLoopOutHeadAndTail->sLoopOutHeadEdge.psEdgeParentBlock = psBlock;
						psLoopOutHeadAndTail->sLoopOutHeadEdge.uEdgeSuccIdx = uSuccIdx;
						psLoopOutHeadAndTail->sLoopOutTailEdge.psEdgeParentBlock = psBlockListEntry->psBlock;
						psLoopOutHeadAndTail->sLoopOutTailEdge.uEdgeSuccIdx = uSuccIdxIntern;
					}
					else
					{
						PLOOP_OUT_HEAD_AND_TAIL psLoopOutHeadAndTail;
						if(!IsLoopOutHeadAndTailPresentInList(&(psLoopOutFlow->sLoopOutHeadAndTailLst), psBlock, uSuccIdx, psBlockListEntry->psBlock, uSuccIdxIntern))
						{
							psLoopOutHeadAndTail = AppendLoopOutHeadAndTailToList(psState, &(psLoopOutFlow->sLoopOutHeadAndTailLst));
							psLoopOutHeadAndTail->sLoopOutHeadEdge.psEdgeParentBlock = psBlock;
							psLoopOutHeadAndTail->sLoopOutHeadEdge.uEdgeSuccIdx = uSuccIdx;
							psLoopOutHeadAndTail->sLoopOutTailEdge.psEdgeParentBlock = psBlockListEntry->psBlock;
							psLoopOutHeadAndTail->sLoopOutTailEdge.uEdgeSuccIdx = uSuccIdxIntern;
						}
					}
				}				
			}
		}
	}
	FreeBlockList(psState, &sVisitedBlockList);
	return;
}

static
IMG_BOOL IsBlockInLoopOrEnclosedLoops(PCODEBLOCK psCodeBlock, PCODEBLOCK psLoopHeaderBlock)
{
	while (psCodeBlock != NULL)
	{
		if (psCodeBlock->psLoopHeader == psLoopHeaderBlock)
		{
			return IMG_TRUE;
		}
		psCodeBlock = psCodeBlock->psLoopHeader;
	}
	return IMG_FALSE;
}

static
IMG_VOID FindLoopBlockFlow(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PCODEBLOCK psLoopHeaderBlock, PUSC_LIST psLoopOutFlowLst, PUSC_LIST psLoopCntEdgeLst, PUSC_LIST psLoopInfoLst)
{
	IMG_UINT32 uSuccIdx;
	for (uSuccIdx = 0; uSuccIdx < psBlock->uNumSuccs; uSuccIdx++)
	{
		if	(
				(psBlock->asSuccs[uSuccIdx].psDest != psLoopHeaderBlock) &&
				(!IsBlockInLoopOrEnclosedLoops(psBlock->asSuccs[uSuccIdx].psDest, psLoopHeaderBlock)) &&
				((psBlock == psLoopHeaderBlock) || (!IsLoopInfoPresentInList(psLoopInfoLst, psBlock)))
			)
		{
			FindLoopOutFlow(psState, psBlock, uSuccIdx, psLoopOutFlowLst, psLoopHeaderBlock);
		}
		else if (psBlock->asSuccs[uSuccIdx].psDest == psLoopHeaderBlock)
		{
 			PEDGE_INFO psEdgeInfo = AppendEdgeInfoToList(psState, psLoopCntEdgeLst);
			psEdgeInfo->psEdgeParentBlock = psBlock;
			psEdgeInfo->uEdgeSuccIdx = uSuccIdx;
		}
	}
	return;
}

static
IMG_VOID FindLoop(PINTERMEDIATE_STATE psState, PCODEBLOCK psLoopHeaderBlock, IMG_UINT32 uFirstBackEdge, PLOOP_INFO psLoopInfo, PUSC_LIST psLoopInfoLst)
{
	IMG_UINT32 uIdx;
	PCODEBLOCK psBlock;	
	USC_LIST sBlockList;
	psLoopInfo->psLoopHeaderBlock = psLoopHeaderBlock;	
	InitializeList(&sBlockList);
	FindLoopBlockFlow(psState, psLoopHeaderBlock, psLoopHeaderBlock, &(psLoopInfo->sLoopOutFlowLst), &(psLoopInfo->sLoopCntEdgeLst), psLoopInfoLst);
	for (uIdx = uFirstBackEdge; (uIdx < psLoopHeaderBlock->uNumPreds); uIdx++)
	{
		psBlock = psLoopHeaderBlock->asPreds[uIdx].psDest;
		if (
			(psBlock != psLoopHeaderBlock) 
			&&
			Dominates(psState, psLoopHeaderBlock, psBlock)
			&&
			!IsBlockPresentInList(&sBlockList, psBlock)
		)
		{
			FindLoopBlockFlow(psState, psBlock, psLoopHeaderBlock, &(psLoopInfo->sLoopOutFlowLst), &(psLoopInfo->sLoopCntEdgeLst), psLoopInfoLst);
			AppendBlockToList(psState, &sBlockList, psBlock);			
		}
	}	
	{
		PUSC_LIST_ENTRY	psListEntry;
		PBLOCK_LISTENTRY psBlockListEntry;
		PCODEBLOCK psPredBlock;
		for (psListEntry = sBlockList.psHead; (psListEntry != NULL); psListEntry = psListEntry->psNext)
		{
			psBlockListEntry = IMG_CONTAINING_RECORD(psListEntry, PBLOCK_LISTENTRY, sListEntry);
			psBlock = psBlockListEntry->psBlock;
			for (uIdx = 0; (uIdx < psBlock->uNumPreds); uIdx++)
			{
				psPredBlock = psBlock->asPreds[uIdx].psDest;
				if	(
						(psPredBlock != psLoopHeaderBlock)
						&& 
						!IsBlockPresentInList(&sBlockList, psPredBlock)												
					)
				{
					FindLoopBlockFlow(psState, psPredBlock, psLoopHeaderBlock, &(psLoopInfo->sLoopOutFlowLst), &(psLoopInfo->sLoopCntEdgeLst), psLoopInfoLst);
					AppendBlockToList(psState, &sBlockList, psPredBlock);
				}				
			}
		}
	}
	FreeBlockList(psState, &sBlockList);
	return;
}

static
IMG_VOID FindCfgLoops(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PUSC_LIST psLoopInfoLst)
{
	IMG_UINT32 uIdx;
	for (uIdx = 0; (uIdx < psBlock->uNumDomChildren); uIdx++)
	{
		FindCfgLoops(psState, psBlock->apsDomChildren[uIdx], psLoopInfoLst);
	}
	for (uIdx = 0; uIdx < psBlock->uNumPreds; uIdx++)
	{
		if (
			Dominates(psState, psBlock, psBlock->asPreds[uIdx].psDest) 
			&&
			(!IsLoopInfoPresentInList(psLoopInfoLst, psBlock))
		)
		{
			/*
				predecessor is the source of a loop backedge,
				so psBlock is it's header
			*/
			PLOOP_INFO psLoopInfo = AppendLoopInfoToList(psState, psLoopInfoLst);			
			FindLoop(psState, psBlock, uIdx, psLoopInfo, psLoopInfoLst);
		}
	}
}

static
IMG_VOID SetCfgLoopNestingDetails(PUSC_LIST psLoopInfoLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	PLOOP_INFO_LISTENTRY psLoopInfoLstEntry;
	for (psListEntry = psLoopInfoLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PCODEBLOCK psEnclosingLoopHeader;
		psLoopInfoLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_INFO_LISTENTRY, sListEntry);
		psEnclosingLoopHeader = psLoopInfoLstEntry->sLoopInfo.psLoopHeaderBlock->psLoopHeader;
		if (psEnclosingLoopHeader != NULL)
		{
			PUSC_LIST_ENTRY	psListEntryAfter;
			PLOOP_INFO_LISTENTRY psLoopInfoLstEntryAfter;
			for (psListEntryAfter = psListEntry->psNext; psListEntryAfter != NULL; psListEntryAfter = psListEntryAfter->psNext)
			{
				psLoopInfoLstEntryAfter = IMG_CONTAINING_RECORD(psListEntryAfter, PLOOP_INFO_LISTENTRY, sListEntry);
				if (psLoopInfoLstEntryAfter->sLoopInfo.psLoopHeaderBlock == psEnclosingLoopHeader)				
				{
					psLoopInfoLstEntry->sLoopInfo.psEnclosingLoopInfo = &(psLoopInfoLstEntryAfter->sLoopInfo);
					break;
				}
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID FindProgramLoops(PINTERMEDIATE_STATE psState, PUSC_LIST psFuncLoopsInfoLst)
{
	PUSC_FUNCTION psFunc;	
	InitializeList(psFuncLoopsInfoLst);	
	for (psFunc = psState->psFnInnermost; psFunc; psFunc = psFunc->psFnNestOuter)
	{
		PFUNC_LOOPS_INFO psFuncLoopsInfo = AppendFuncLoopsInfoToList(psState, psFuncLoopsInfoLst);
		psFuncLoopsInfo->psFunc = psFunc;
		ComputeLoopNestingTree(psState, psFunc->sCfg.psEntry);
		FindCfgLoops(psState, psFunc->sCfg.psEntry, &(psFuncLoopsInfo->sFuncLoopInfoLst));
		SetCfgLoopNestingDetails(&(psFuncLoopsInfo->sFuncLoopInfoLst));
	}
}

static
IMG_VOID MoveBlockInCfg(PINTERMEDIATE_STATE psState, PCFG psCfg, PCODEBLOCK psExitBlock, PCODEBLOCK psBlock)
{
	if (psBlock->psOwner != psCfg)
	{
		DetachBlockFromCfg(psState, psBlock, psBlock->psOwner);
		AttachBlockToCfg(psState, psBlock, psCfg);
	}
	if (psBlock == psExitBlock)
	{
		return;
	}
	{
		IMG_UINT32 uSuccIdx;
		for (uSuccIdx = 0; uSuccIdx < psBlock->uNumSuccs; uSuccIdx++)
		{
			MoveBlockInCfg(psState, psCfg, psExitBlock, psBlock->asSuccs[uSuccIdx].psDest);		 
		}
	}
	return;
}

static
PCFG MoveBlocksToNewCfg(PINTERMEDIATE_STATE psState, PCODEBLOCK psEntryBlock, PCODEBLOCK psExitBlock)
{
	PCFG psCfg = AllocateCfg(psState, psEntryBlock->psOwner->psFunc);	
	MoveBlockInCfg(psState, psCfg, psExitBlock, psEntryBlock);
	RedirectEdgesFromPredecessors(psState, psExitBlock, psCfg->psExit, IMG_FALSE);
	FreeBlock(psState, psExitBlock);
	return psCfg;
}

static
IMG_VOID OptimizeLoopEnd(PINTERMEDIATE_STATE psState, PCODEBLOCK psLoopContinueExitBlock, IMG_PBOOL pbGenerateInfiniteLoop, IMG_PUINT32 puLoopPredReg, IMG_PBOOL pbInvertLoopPredReg, PCFG psLoopCfg)
{
	*pbGenerateInfiniteLoop = IMG_TRUE;
	*puLoopPredReg = USC_PREDREG_NONE;
	*pbInvertLoopPredReg = IMG_FALSE;
	if(psLoopContinueExitBlock->uNumPreds > 1)
	{
		IMG_UINT32 uPredIdx;
		for (uPredIdx = 0; uPredIdx < psLoopContinueExitBlock->uNumPreds; uPredIdx++)
		{
			if (psLoopContinueExitBlock->asPreds[uPredIdx].psDest->uNumSuccs == 2)
			{
				PCODEBLOCK psCandBreakBlk = NULL;
				if (psLoopContinueExitBlock->asPreds[uPredIdx].uDestIdx == 0)
				{
					*pbInvertLoopPredReg = IMG_FALSE;
					psCandBreakBlk = psLoopContinueExitBlock->asPreds[uPredIdx].psDest->asSuccs[1].psDest;
				}
				else
				{
					*pbInvertLoopPredReg = IMG_TRUE;
					psCandBreakBlk = psLoopContinueExitBlock->asPreds[uPredIdx].psDest->asSuccs[0].psDest;
				}
				if ((psCandBreakBlk->psBody != NULL) && (psCandBreakBlk->psBody->eOpcode == IBREAK))
				{
					*pbGenerateInfiniteLoop = IMG_FALSE;
					*puLoopPredReg = psLoopContinueExitBlock->asPreds[uPredIdx].psDest->u.sCond.sPredSrc.uNumber;
					SetBlockUnconditional(psState, psLoopContinueExitBlock->asPreds[uPredIdx].psDest, psLoopContinueExitBlock);
					ClearSuccessors(psState, psCandBreakBlk);
					FreeBlock(psState, psCandBreakBlk);
					break;
				}
			}
		}
	}
	RedirectEdgesFromPredecessors(psState, psLoopContinueExitBlock, psLoopCfg->psExit, IMG_FALSE);
	ClearSuccessors(psState, psLoopContinueExitBlock);
	FreeBlock(psState, psLoopContinueExitBlock);
}

static
PCODEBLOCK GenerateImmediateTest(	PINTERMEDIATE_STATE	psState,
									IMG_UINT32			uTempReg,
									IMG_UINT32			uImmediate,
									IMG_PUINT32			puPredTemp,
									PCFG				psCfg)
/*********************************************************************************
 Function		: GenerateImmediateTest

 Description	: Generate intermediate instructions for testing whether a register
				  matches a particular immediate.

 Parameters		: psState	- The current compiler context
				  uTempReg	- Intermediate temp. reg. to test.
				  uImmediate
							- Immediate value to test.
				  puPredTemp
							- Predicate register returned contaning test result.
				  psCfg		- Cfg to put test block in.
 Return			: A basic block containing the test instruction.
*********************************************************************************/
{
	PCODEBLOCK	psTestBlock;
	PINST		psXorInst;

	psTestBlock = AllocateBlock(psState, psCfg);

	/*
		Allocate a new predicate register for the result of the test.
	*/
	*puPredTemp = GetNextPredicateRegister(psState);

	psXorInst = AllocateInst(psState, NULL/* TODO */);
	SetOpcodeAndDestCount(psState, psXorInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
	psXorInst->u.psTest->eAluOpcode = IXOR;
	MakePredicateDest(psState, psXorInst, 0 /* uDestIdx */, *puPredTemp);	
	CompOpToTest(psState, UFREG_COMPOP_EQ, &psXorInst->u.psTest->sTest);
	SetSrc(psState, psXorInst, 0, USEASM_REGTYPE_TEMP, uTempReg, UF_REGFORMAT_F32);
	SetSrc(psState, psXorInst, 1, USEASM_REGTYPE_IMMEDIATE, uImmediate, UF_REGFORMAT_F32);
	AppendInst(psState, psTestBlock, psXorInst);

	/*
		Flag the new predicate register as live out of the block containing
		the test.
	*/
	SetRegisterLiveMask(psState, 
						&psTestBlock->sRegistersLiveOut, 
						USEASM_REGTYPE_PREDICATE, 
						*puPredTemp, 
						0 /* uArrayOffset */, 
						USC_DESTMASK_FULL);

	return psTestBlock;
}

static
IMG_VOID SetLoopExits(PINTERMEDIATE_STATE psState, PLOOP_INFO psLoopInfo, PCODEBLOCK psLoopCfgBlock, PUSC_LIST psLoopOutFlowLst, IMG_BOOL bLoopHasRandomExits, IMG_UINT32 uRandExitSelectReg, IMG_UINT32 uRandExitSelectCount, PLOOP_OUT_FLOW_LISTENTRY psBrkLoopOutFlowLstEntry, PLOOP_OUT_FLOW_LISTENTRY psRetLoopOutFlowLstEntry)
{
	if (bLoopHasRandomExits == IMG_FALSE)
	{
		if (psBrkLoopOutFlowLstEntry != NULL)
		{
			SetBlockUnconditional(psState, psLoopCfgBlock, psBrkLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock);
		}
		else if (psRetLoopOutFlowLstEntry != NULL)
		{
			SetBlockUnconditional(psState, psLoopCfgBlock, psRetLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock);
		}
		else
		{
			SetBlockUnconditional(psState, psLoopCfgBlock, psLoopCfgBlock->psOwner->psFunc->sCfg.psExit);
			if (psLoopInfo->psEnclosingLoopInfo != NULL)
			{
				AddLoopOutFlowData(psState, &(psLoopInfo->psEnclosingLoopInfo->sLoopOutFlowLst), psLoopCfgBlock, 0);
			}			
		}
	}
	else
	{
		IMG_UINT32 uRandExitSelectCountIntern;
		PUSC_LIST_ENTRY	psListEntry;
		PLOOP_OUT_FLOW_LISTENTRY psLoopOutFlowLstEntry;
		PCODEBLOCK psRandExitBlock = NULL;
		IMG_UINT32 uPrevPredReg = UINT_MAX;
		PCFG psCfgToPutBlocks;		
		PCODEBLOCK psFirstTestBlock = NULL;
		PCODEBLOCK psPrevTestBlock = NULL;
		PCODEBLOCK psPrevRandExit = NULL;
		if (psLoopInfo->psEnclosingLoopInfo == NULL)
		{
			psCfgToPutBlocks = &(psLoopInfo->psLoopHeaderBlock->psOwner->psFunc->sCfg);
		}
		else
		{
			psCfgToPutBlocks = psLoopInfo->psEnclosingLoopInfo->psLoopHeaderBlock->psOwner;
		}
		if (psBrkLoopOutFlowLstEntry != NULL)
		{
			psRandExitBlock = psBrkLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock;
			psFirstTestBlock = GenerateImmediateTest(psState, uRandExitSelectReg, 0, &uPrevPredReg, psCfgToPutBlocks);
			psPrevTestBlock = psFirstTestBlock;
			psPrevRandExit = psRandExitBlock;
		}		
		uRandExitSelectCountIntern = 1;		
		for (psListEntry = psLoopOutFlowLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			psLoopOutFlowLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_FLOW_LISTENTRY, sListEntry);
			if ((psLoopOutFlowLstEntry != psBrkLoopOutFlowLstEntry) && (psBrkLoopOutFlowLstEntry != psRetLoopOutFlowLstEntry))
			{
				psRandExitBlock = psLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock;
				if (psRandExitBlock->psLoopHeader != psLoopInfo->psLoopHeaderBlock->psLoopHeader)
				{					
					/*
						Random exit destination block is beyond enclosing loop.
					*/
					PCODEBLOCK psBlockInEnclosingLoop = AllocateBlock(psState, psCfgToPutBlocks);
					ASSERT(psLoopInfo->psEnclosingLoopInfo != NULL);
					SetBlockUnconditional(psState, psBlockInEnclosingLoop, psRandExitBlock);
					psBlockInEnclosingLoop->psLoopHeader = psLoopInfo->psLoopHeaderBlock->psLoopHeader;
					AddLoopOutFlowData(psState, &(psLoopInfo->psEnclosingLoopInfo->sLoopOutFlowLst), psBlockInEnclosingLoop, 0);
					psRandExitBlock = psBlockInEnclosingLoop;
				}
				if ((uRandExitSelectCountIntern + 1) != uRandExitSelectCount)
				{
					IMG_UINT32 uNewPredReg;
					PCODEBLOCK psNewTestBlock = GenerateImmediateTest(psState, uRandExitSelectReg, uRandExitSelectCountIntern, &uNewPredReg, psCfgToPutBlocks);
					psNewTestBlock->psLoopHeader = psLoopInfo->psLoopHeaderBlock->psLoopHeader;
					if (psFirstTestBlock != NULL)
					{
						SetBlockConditional(psState, psPrevTestBlock, uPrevPredReg, psPrevRandExit, psNewTestBlock, IMG_FALSE);						
					}
					else
					{
						psFirstTestBlock = psNewTestBlock;
					}
					uPrevPredReg = uNewPredReg;
					psPrevTestBlock = psNewTestBlock;
					psPrevRandExit = psRandExitBlock;
				}
				else
				{
					if (psFirstTestBlock != NULL)
					{
						SetBlockConditional(psState, psPrevTestBlock, uPrevPredReg, psPrevRandExit, psRandExitBlock, IMG_FALSE);
					}
					else
					{
						psFirstTestBlock = psRandExitBlock;
					}
				}
				uRandExitSelectCountIntern++;
			}
		}
		SetBlockUnconditional(psState, psLoopCfgBlock, psFirstTestBlock);
	}
}

static
IMG_VOID ReplaceRefsToLoopHeaderByItsCfgBlock(PINTERMEDIATE_STATE psState, PLOOP_INFO psLoopInfo, PCODEBLOCK psLoopCfgBlock, PUSC_LIST psLoopInfoLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	PLOOP_INFO_LISTENTRY psLoopInfoLstEntry;
	for (psListEntry = psLoopInfoLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psLoopInfoLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_INFO_LISTENTRY, sListEntry);
		{
			PUSC_LIST_ENTRY	psListEntry;
			PLOOP_OUT_FLOW_LISTENTRY psLoopOutFlowLstEntry;		
			for (psListEntry = psLoopInfoLstEntry->sLoopInfo.sLoopOutFlowLst.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
			{
				psLoopOutFlowLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_FLOW_LISTENTRY, sListEntry);
				if (psLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock == psLoopInfo->psLoopHeaderBlock)
				{
					PUSC_LIST_ENTRY	psListEntry;
					PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY psLoopOutHeadAndTailLstEntry;
					for	(
							psListEntry = psLoopOutFlowLstEntry->sLoopOutFlow.sLoopOutHeadAndTailLst.psHead; 
							psListEntry != NULL;
							psListEntry = psListEntry->psNext
						)
					{
						psLoopOutHeadAndTailLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY, sListEntry);
						RedirectEdge(psState, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.psEdgeParentBlock, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.uEdgeSuccIdx, psLoopCfgBlock);
					}
					psLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock = psLoopCfgBlock;
				}
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID RestructureLoop(PINTERMEDIATE_STATE psState, PLOOP_INFO psLoopInfo, PCFG *ppsLoopCfg, IMG_PBOOL pbGenerateInfiniteLoop, IMG_PUINT32 puLoopPredReg, IMG_PBOOL pbInvertLoopPredReg, PUSC_LIST psLoopInfoLst)
{
	IMG_BOOL bLoopHasRandomExits = IMG_FALSE;
	PCODEBLOCK psLoopCfgBlock = NULL;	
	IMG_UINT32 auOppositeSucc[2] = {1U, 0U};
	PCODEBLOCK psLoopExitBlock = AllocateBlock(psState, psLoopInfo->psLoopHeaderBlock->psOwner);
	PCODEBLOCK psLoopContinueExitBlock = NULL;
	IMG_UINT32 uRandExitSelectReg = UINT_MAX;
	IMG_UINT32 uRandExitSelectCount = 0U;
	*ppsLoopCfg = NULL;
	psLoopExitBlock->psLoopHeader = psLoopInfo->psLoopHeaderBlock;
	psLoopCfgBlock = AllocateBlock(psState, psLoopInfo->psLoopHeaderBlock->psOwner);
	psLoopCfgBlock->psLoopHeader = psLoopInfo->psLoopHeaderBlock->psLoopHeader;
	{
		PUSC_LIST_ENTRY	psListEntry;
		PEDGE_INFO_LISTENTRY psEdgeInfoLstEntry;
		for (psListEntry = psLoopInfo->sLoopCntEdgeLst.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
		{
			PCODEBLOCK psContinueBlock = AllocateBlock(psState, psLoopInfo->psLoopHeaderBlock->psOwner);
			PINST psContinueInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psContinueInst, ICONTINUE);
			AppendInst(psState, psContinueBlock, psContinueInst);
			psEdgeInfoLstEntry = IMG_CONTAINING_RECORD(psListEntry, PEDGE_INFO_LISTENTRY, sListEntry);
			RedirectEdge(psState, psEdgeInfoLstEntry->sEdgeInfo.psEdgeParentBlock, psEdgeInfoLstEntry->sEdgeInfo.uEdgeSuccIdx, psContinueBlock);
			if	((psEdgeInfoLstEntry->sEdgeInfo.psEdgeParentBlock->uNumSuccs > 1) && psEdgeInfoLstEntry->sEdgeInfo.psEdgeParentBlock->asSuccs[auOppositeSucc[psEdgeInfoLstEntry->sEdgeInfo.uEdgeSuccIdx]].psDest->psLoopHeader == psLoopInfo->psLoopHeaderBlock)
			{
				SetBlockUnconditional(psState, psContinueBlock, psEdgeInfoLstEntry->sEdgeInfo.psEdgeParentBlock->asSuccs[auOppositeSucc[psEdgeInfoLstEntry->sEdgeInfo.uEdgeSuccIdx]].psDest);
			}
			else
			{	
				SetBlockUnconditional(psState, psContinueBlock, psLoopExitBlock);
				psLoopContinueExitBlock = psContinueBlock;
			}
			psContinueBlock->psLoopHeader = psLoopInfo->psLoopHeaderBlock;
		}
	}
	ASSERT(psLoopContinueExitBlock != NULL);
	{
		PLOOP_OUT_FLOW_LISTENTRY psBrkLoopOutFlowLstEntry = NULL;
		PLOOP_OUT_FLOW_LISTENTRY psRetLoopOutFlowLstEntry = NULL;
		{
			PUSC_LIST_ENTRY	psListEntry;
			PLOOP_OUT_FLOW_LISTENTRY psLoopOutFlowLstEntry;
			IMG_UINT32 uBrkOutFlowsCount = 0U;
			for (psListEntry = psLoopInfo->sLoopOutFlowLst.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
			{
				psLoopOutFlowLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_FLOW_LISTENTRY, sListEntry);
				if ((psLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock->psLoopHeader == NULL) && (psLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock == psLoopInfo->psLoopHeaderBlock->psOwner->psExit))
				{
					psRetLoopOutFlowLstEntry = psLoopOutFlowLstEntry;
				}
				else if (psLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock->psLoopHeader == psLoopInfo->psLoopHeaderBlock->psLoopHeader)
				{
					IMG_UINT32 uCurrBrkOutFlowsCount = GetListNodesCount(&(psLoopOutFlowLstEntry->sLoopOutFlow.sLoopOutHeadAndTailLst));
					if (psBrkLoopOutFlowLstEntry != NULL)
					{
						bLoopHasRandomExits = IMG_TRUE;
					}
					if (uCurrBrkOutFlowsCount > uBrkOutFlowsCount)
					{
						uBrkOutFlowsCount = uCurrBrkOutFlowsCount;
						psBrkLoopOutFlowLstEntry = psLoopOutFlowLstEntry;
					}
				}
				else if (psLoopOutFlowLstEntry->sLoopOutFlow.psLoopOutDestBlock == psLoopInfo->psLoopHeaderBlock->psOwner->psExit)
				{
					psRetLoopOutFlowLstEntry = psLoopOutFlowLstEntry;
				}
				else
				{
					bLoopHasRandomExits = IMG_TRUE;
				}
			}
		}
		if (psRetLoopOutFlowLstEntry != NULL)
		{
			PUSC_LIST_ENTRY	psListEntry;
			PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY psLoopOutHeadAndTailLstEntry;			
			for	(
					psListEntry = psRetLoopOutFlowLstEntry->sLoopOutFlow.sLoopOutHeadAndTailLst.psHead;
					psListEntry != NULL;
					psListEntry = psListEntry->psNext
				)
			{
				PCODEBLOCK psRetBlock = AllocateBlock(psState, psLoopInfo->psLoopHeaderBlock->psOwner);
				PINST psRetInst = AllocateInst(psState, NULL);
				psLoopOutHeadAndTailLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY, sListEntry);
				SetOpcode(psState, psRetInst, IRETURN);
				AppendInst(psState, psRetBlock, psRetInst);
				RedirectEdge(psState, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.psEdgeParentBlock, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.uEdgeSuccIdx, psRetBlock);
				psLoopOutHeadAndTailLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY, sListEntry);
				if	(
						(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock->uNumSuccs < 2)
						||
						(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock->asSuccs[auOppositeSucc[psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.uEdgeSuccIdx]].psDest->psLoopHeader != psLoopInfo->psLoopHeaderBlock)
					)
				{
					SetBlockUnconditional(psState, psRetBlock, psLoopContinueExitBlock);
				}
				else
				{		
					SetBlockUnconditional(psState, psRetBlock, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock->asSuccs[auOppositeSucc[psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.uEdgeSuccIdx]].psDest);
				}
				psRetBlock->psLoopHeader = psLoopInfo->psLoopHeaderBlock;
			}
		}
		if (psBrkLoopOutFlowLstEntry != NULL)
		{
			PUSC_LIST_ENTRY	psListEntry;
			PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY psLoopOutHeadAndTailLstEntry;
			for	(
					psListEntry = psBrkLoopOutFlowLstEntry->sLoopOutFlow.sLoopOutHeadAndTailLst.psHead; 
					psListEntry != NULL;
					psListEntry = psListEntry->psNext
				)
			{
				PCODEBLOCK psRandExitSelectBlock = NULL;
				PCODEBLOCK psBreakBlock = NULL;
				psLoopOutHeadAndTailLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY, sListEntry);
				if (bLoopHasRandomExits == IMG_TRUE)
				{
					psRandExitSelectBlock = AllocateBlock(psState, psLoopInfo->psLoopHeaderBlock->psOwner);
					uRandExitSelectReg = GetNextRegister(psState);
					{
						PINST psMovInst = AllocateInst(psState, IMG_NULL);
						SetOpcode(psState, psMovInst, IMOV);
						SetDest(psState, psMovInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uRandExitSelectReg, UF_REGFORMAT_F32);
						SetSrc(psState, psMovInst, 0, USEASM_REGTYPE_IMMEDIATE, 0, UF_REGFORMAT_F32);
						InsertInstBefore(psState, psRandExitSelectBlock,
										 psMovInst,
										 psRandExitSelectBlock->psBody);
					}
				}
				{
					PINST psBreakInst = AllocateInst(psState, NULL);
					psBreakBlock = AllocateBlock(psState, psLoopInfo->psLoopHeaderBlock->psOwner);					
					SetOpcode(psState, psBreakInst, IBREAK);
					AppendInst(psState, psBreakBlock, psBreakInst);
					if (bLoopHasRandomExits == IMG_FALSE)
					{
						psRandExitSelectBlock = psBreakBlock;
					}
					else
					{
						SetBlockUnconditional(psState, psRandExitSelectBlock, psBreakBlock);
					}
					RedirectEdge(psState, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.psEdgeParentBlock, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.uEdgeSuccIdx, psRandExitSelectBlock);
				}				
				if	(
						(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock->uNumSuccs < 2)
						||
						(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock->asSuccs[auOppositeSucc[psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.uEdgeSuccIdx]].psDest->psLoopHeader != psLoopInfo->psLoopHeaderBlock)
					)
				{
					SetBlockUnconditional(psState, psBreakBlock, psLoopContinueExitBlock);
				}
				else
				{
					SetBlockUnconditional(psState, psBreakBlock, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock->asSuccs[auOppositeSucc[psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.uEdgeSuccIdx]].psDest);
				}
				if (bLoopHasRandomExits == IMG_TRUE)
				{
					psRandExitSelectBlock->psLoopHeader = psLoopInfo->psLoopHeaderBlock;
				}
				psBreakBlock->psLoopHeader = psLoopInfo->psLoopHeaderBlock;
			}
		}
		if (bLoopHasRandomExits == IMG_TRUE)
		{
			PUSC_LIST_ENTRY	psListEntry;
			PLOOP_OUT_FLOW_LISTENTRY psLoopOutFlowLstEntry;
			if (uRandExitSelectReg == UINT_MAX)
			{
				uRandExitSelectReg = GetNextRegister(psState);
			}
			uRandExitSelectCount = 1;
			for (psListEntry = psLoopInfo->sLoopOutFlowLst.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
			{
				psLoopOutFlowLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_FLOW_LISTENTRY, sListEntry);
				if ((psLoopOutFlowLstEntry != psBrkLoopOutFlowLstEntry) && (psBrkLoopOutFlowLstEntry != psRetLoopOutFlowLstEntry))
				{
					PUSC_LIST_ENTRY	psListEntry;
					PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY psLoopOutHeadAndTailLstEntry;
					for	(
							psListEntry = psLoopOutFlowLstEntry->sLoopOutFlow.sLoopOutHeadAndTailLst.psHead; 
							psListEntry != NULL;
							psListEntry = psListEntry->psNext
						)
					{
						PCODEBLOCK psRandExitSelectBlock = NULL;
						PCODEBLOCK psBreakBlock = NULL;
						psRandExitSelectBlock = AllocateBlock(psState, psLoopInfo->psLoopHeaderBlock->psOwner);
						psLoopOutHeadAndTailLstEntry = IMG_CONTAINING_RECORD(psListEntry, PLOOP_OUT_HEAD_AND_TAIL_LISTENTRY, sListEntry);
						{
							PINST psMovInst = AllocateInst(psState, IMG_NULL);
							SetOpcode(psState, psMovInst, IMOV);
							SetDest(psState, psMovInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uRandExitSelectReg, UF_REGFORMAT_F32);
							SetSrc(psState, psMovInst, 0, USEASM_REGTYPE_IMMEDIATE, uRandExitSelectCount, UF_REGFORMAT_F32);
							InsertInstBefore(psState, psRandExitSelectBlock,
											 psMovInst,
											 psRandExitSelectBlock->psBody);
						}
						{
							PINST psBreakInst = AllocateInst(psState, NULL);
							psBreakBlock = AllocateBlock(psState, psLoopInfo->psLoopHeaderBlock->psOwner);							
							SetOpcode(psState, psBreakInst, IBREAK);
							AppendInst(psState, psBreakBlock, psBreakInst);
							SetBlockUnconditional(psState, psRandExitSelectBlock, psBreakBlock);
							RedirectEdge(psState, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.psEdgeParentBlock, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutTailEdge.uEdgeSuccIdx, psRandExitSelectBlock);
						}						
						if	(
								(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock->uNumSuccs < 2)
								||
								(psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock->asSuccs[auOppositeSucc[psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.uEdgeSuccIdx]].psDest->psLoopHeader != psLoopInfo->psLoopHeaderBlock)
							)
						{
							SetBlockUnconditional(psState, psBreakBlock, psLoopContinueExitBlock);
						}
						else
						{
							SetBlockUnconditional(psState, psBreakBlock, psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.psEdgeParentBlock->asSuccs[auOppositeSucc[psLoopOutHeadAndTailLstEntry->sLoopOutHeadAndTail.sLoopOutHeadEdge.uEdgeSuccIdx]].psDest);
						}
						psRandExitSelectBlock->psLoopHeader = psLoopInfo->psLoopHeaderBlock;
						psBreakBlock->psLoopHeader = psLoopInfo->psLoopHeaderBlock;
					}
					uRandExitSelectCount++;
				}
			}
		}
		RedirectEdgesFromPredecessors(psState, psLoopInfo->psLoopHeaderBlock, psLoopCfgBlock, IMG_FALSE);
		ReplaceRefsToLoopHeaderByItsCfgBlock(psState, psLoopInfo, psLoopCfgBlock, psLoopInfoLst);
		if(psLoopCfgBlock != 0)
		{
			psLoopCfgBlock->u.sUncond.psCfg = MoveBlocksToNewCfg(psState, psLoopInfo->psLoopHeaderBlock, psLoopExitBlock);
			psLoopCfgBlock->uFlags |= USC_CODEBLOCK_CFG;
			psLoopCfgBlock->u.sUncond.psCfg->psEntry = psLoopInfo->psLoopHeaderBlock;
		}
		SetLoopExits(psState, psLoopInfo, psLoopCfgBlock, &(psLoopInfo->sLoopOutFlowLst), bLoopHasRandomExits, uRandExitSelectReg, uRandExitSelectCount, psBrkLoopOutFlowLstEntry, psRetLoopOutFlowLstEntry);
	}
	{
		OptimizeLoopEnd(psState, psLoopContinueExitBlock, pbGenerateInfiniteLoop, puLoopPredReg, pbInvertLoopPredReg, psLoopCfgBlock->u.sUncond.psCfg);
		*ppsLoopCfg = psLoopCfgBlock->u.sUncond.psCfg;
	}
}

#endif /* #if defined(EXECPRED) */
