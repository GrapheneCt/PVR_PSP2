/******************************************************************************
 * Name         : cdg.c
 * Title        : Control Dependence Graph
 * Created      : April 2011
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
 * $Log: cdg.c $
 *****************************************************************************/
#include "cdg.h"
#if defined(EXECPRED)
static
IMG_VOID AppendCtrlDepNodeToList(PINTERMEDIATE_STATE psState, PUSC_LIST psCtrlDepNodeLst, PCTRL_DEP_NODE psCtrlDepNode)
{
	PCTRL_DEP_NODE_LISTENTRY psCtrlDepNodeLstEntry;
	psCtrlDepNodeLstEntry = UscAlloc(psState, sizeof(*psCtrlDepNodeLstEntry));
	psCtrlDepNodeLstEntry->psCtrlDepNode = psCtrlDepNode;
	AppendToList(psCtrlDepNodeLst, &(psCtrlDepNodeLstEntry->sListEntry));	
}

static
IMG_BOOL IsCtrlDepNodePresentInList(PUSC_LIST psCtrlDepNodeLst, PCTRL_DEP_NODE psCtrlDepNode, PCTRL_DEP_NODE* ppsCtrlDepNode)
{	
	PUSC_LIST_ENTRY	psListEntry;
	PCTRL_DEP_NODE_LISTENTRY psCtrlDepNodeLstEntry;
	for (psListEntry = psCtrlDepNodeLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psCtrlDepNodeLstEntry = IMG_CONTAINING_RECORD(psListEntry, PCTRL_DEP_NODE_LISTENTRY, sListEntry);
		if (psCtrlDepNodeLstEntry->psCtrlDepNode == psCtrlDepNode)
		{
			*ppsCtrlDepNode = psCtrlDepNodeLstEntry->psCtrlDepNode;
			return IMG_TRUE;
		}
	}
	*ppsCtrlDepNode = NULL;
	return IMG_FALSE;
}

static
IMG_VOID FreeCtrlDepNodeList(PINTERMEDIATE_STATE psState, PUSC_LIST psCtrlDepNodeLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	while ((psListEntry = RemoveListHead(psCtrlDepNodeLst)) != NULL)
	{
		PCTRL_DEP_NODE_LISTENTRY psCtrlDepNodeLstEntry;
		psCtrlDepNodeLstEntry = IMG_CONTAINING_RECORD(psListEntry, PCTRL_DEP_NODE_LISTENTRY, sListEntry);		
		UscFree(psState, psCtrlDepNodeLstEntry);
	}
}

static
IMG_VOID FreeCtrlDepBlock(PINTERMEDIATE_STATE psState, PCTRL_DEP_NODE psCtrlDepBlock)
{
	FreeCtrlDepNodeList(psState, &(psCtrlDepBlock->u.sBlock.sPredLst));
}

static
IMG_VOID FreeCtrlDepRegon(PINTERMEDIATE_STATE psState, PCTRL_DEP_NODE psCtrlDepRegon)
{
	FreeCtrlDepNodeList(psState, &(psCtrlDepRegon->u.sRegion.sSuccLst));
}

typedef struct _CTRL_DEP_BLOCK_LISTENTRY
{
	PCTRL_DEP_NODE	psCtrlDepBlock;	
	USC_LIST_ENTRY	sListEntry;
} CTRL_DEP_BLOCK_LISTENTRY, *PCTRL_DEP_BLOCK_LISTENTRY;

static
PCTRL_DEP_NODE AppendCtrlDepBlockToList(PINTERMEDIATE_STATE psState, PUSC_LIST psCtrlDepBlockLst, PCODEBLOCK psBlock)
{
	PCTRL_DEP_BLOCK_LISTENTRY psCtrlDepBlockLstEntry;
	psCtrlDepBlockLstEntry = UscAlloc(psState, sizeof(*psCtrlDepBlockLstEntry));
	psCtrlDepBlockLstEntry->psCtrlDepBlock = (PCTRL_DEP_NODE)UscAlloc(psState, sizeof(*(psCtrlDepBlockLstEntry->psCtrlDepBlock)));
	AppendToList(psCtrlDepBlockLst, &(psCtrlDepBlockLstEntry->sListEntry));
	psCtrlDepBlockLstEntry->psCtrlDepBlock->eCtrlDepType = CTRL_DEP_TYPE_BLOCK;
	psCtrlDepBlockLstEntry->psCtrlDepBlock->u.sBlock.psBlock = psBlock;
	psCtrlDepBlockLstEntry->psCtrlDepBlock->u.sBlock.uNumSucc = 0;
	InitializeList(&(psCtrlDepBlockLstEntry->psCtrlDepBlock->u.sBlock.sPredLst));
	return psCtrlDepBlockLstEntry->psCtrlDepBlock;
}

static
IMG_BOOL IsCtrlDepBlockPresentInList(PUSC_LIST psCtrlDepBlockLst, PCODEBLOCK psBlock, PCTRL_DEP_NODE* ppsCtrlDepBlock)
{	
	PUSC_LIST_ENTRY	psListEntry;
	PCTRL_DEP_BLOCK_LISTENTRY psCtrlDepBlockLstEntry;
	for (psListEntry = psCtrlDepBlockLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psCtrlDepBlockLstEntry = IMG_CONTAINING_RECORD(psListEntry, PCTRL_DEP_BLOCK_LISTENTRY, sListEntry);
		if (psCtrlDepBlockLstEntry->psCtrlDepBlock->u.sBlock.psBlock == psBlock)
		{
			*ppsCtrlDepBlock = psCtrlDepBlockLstEntry->psCtrlDepBlock;
			return IMG_TRUE;
		}
	}
	*ppsCtrlDepBlock = NULL;
	return IMG_FALSE;
}

static
IMG_VOID FreeCtrlDepBlockList(PINTERMEDIATE_STATE psState, PUSC_LIST psCtrlDepBlockLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	while ((psListEntry = RemoveListHead(psCtrlDepBlockLst)) != NULL)
	{
		PCTRL_DEP_BLOCK_LISTENTRY psCtrlDepBlockLstEntry;
		psCtrlDepBlockLstEntry = IMG_CONTAINING_RECORD(psListEntry, PCTRL_DEP_BLOCK_LISTENTRY, sListEntry);
		FreeCtrlDepBlock(psState, psCtrlDepBlockLstEntry->psCtrlDepBlock);
		UscFree(psState, psCtrlDepBlockLstEntry->psCtrlDepBlock);
		UscFree(psState, psCtrlDepBlockLstEntry);
	}
}

typedef struct _CTRL_DEP_REGON_LISTENTRY
{
	PCTRL_DEP_NODE	psCtrlDepRegon;
	PCODEBLOCK psBlock;
	IMG_UINT32 uSuccIdx;
	USC_LIST_ENTRY	sListEntry;
} CTRL_DEP_REGON_LISTENTRY, *PCTRL_DEP_REGON_LISTENTRY;

static
PCTRL_DEP_NODE AddCtrlDepRegonToCtrlDepBlock(PINTERMEDIATE_STATE psState, PCTRL_DEP_NODE psCtrlDepBlock, IMG_UINT32 uSuccIdx)
{
	PCTRL_DEP_NODE psNewCtrlDepRegon = (PCTRL_DEP_NODE)(UscAlloc(psState, sizeof(CTRL_DEP_NODE)));
	psNewCtrlDepRegon->eCtrlDepType = CTRL_DEP_TYPE_REGION;
	InitializeList(&(psNewCtrlDepRegon->u.sRegion.sSuccLst));
	psCtrlDepBlock->u.sBlock.apsSucc[psCtrlDepBlock->u.sBlock.uNumSucc] = psNewCtrlDepRegon;
	psCtrlDepBlock->u.sBlock.auSuccIdx[psCtrlDepBlock->u.sBlock.uNumSucc] = uSuccIdx;
	psCtrlDepBlock->u.sBlock.uNumSucc++;
	return psNewCtrlDepRegon;
}


static
PCTRL_DEP_NODE AppendCtrlDepRegonToList(PINTERMEDIATE_STATE psState, PUSC_LIST psCtrlDepBlockLst, PUSC_LIST psCtrlDepRegonLst, PCODEBLOCK psBlock, IMG_UINT32 uSuccIdx)
{
	PCTRL_DEP_NODE psCtrlDepBlock;
	PCTRL_DEP_REGON_LISTENTRY psCtrlDepRegonLstEntry;
	if (!IsCtrlDepBlockPresentInList(psCtrlDepBlockLst, psBlock, &psCtrlDepBlock))
	{
		imgabort();
	}	
	psCtrlDepRegonLstEntry = UscAlloc(psState, sizeof(*psCtrlDepRegonLstEntry));	
	AppendToList(psCtrlDepRegonLst, &(psCtrlDepRegonLstEntry->sListEntry));
	psCtrlDepRegonLstEntry->psBlock = psBlock;
	psCtrlDepRegonLstEntry->uSuccIdx = uSuccIdx;
	psCtrlDepRegonLstEntry->psCtrlDepRegon = AddCtrlDepRegonToCtrlDepBlock(psState, psCtrlDepBlock, uSuccIdx);	
	return psCtrlDepRegonLstEntry->psCtrlDepRegon;
}

static
IMG_BOOL IsCtrlDepRegonPresentInList(PUSC_LIST psCtrlDepRegonLst, PCODEBLOCK psBlock, IMG_UINT32 uSuccIdx, PCTRL_DEP_NODE* ppsCtrlDepRegon)
{	
	PUSC_LIST_ENTRY	psListEntry;
	PCTRL_DEP_REGON_LISTENTRY psCtrlDepRegonLstEntry;
	for (psListEntry = psCtrlDepRegonLst->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		psCtrlDepRegonLstEntry = IMG_CONTAINING_RECORD(psListEntry, PCTRL_DEP_REGON_LISTENTRY, sListEntry);
		if ((psCtrlDepRegonLstEntry->psBlock == psBlock) && (psCtrlDepRegonLstEntry->uSuccIdx == uSuccIdx))
		{
			*ppsCtrlDepRegon = psCtrlDepRegonLstEntry->psCtrlDepRegon;
			return IMG_TRUE;
		}
	}
	*ppsCtrlDepRegon = NULL;
	return IMG_FALSE;
}

static
IMG_VOID FreeCtrlDepRegonList(PINTERMEDIATE_STATE psState, PUSC_LIST psCtrlDepRegonLst)
{
	PUSC_LIST_ENTRY	psListEntry;
	while ((psListEntry = RemoveListHead(psCtrlDepRegonLst)) != NULL)
	{
		PCTRL_DEP_REGON_LISTENTRY psCtrlDepRegonLstEntry;
		psCtrlDepRegonLstEntry = IMG_CONTAINING_RECORD(psListEntry, PCTRL_DEP_REGON_LISTENTRY, sListEntry);
		FreeCtrlDepRegon(psState, psCtrlDepRegonLstEntry->psCtrlDepRegon);
		UscFree(psState, psCtrlDepRegonLstEntry->psCtrlDepRegon);
		UscFree(psState, psCtrlDepRegonLstEntry);
	}
}

static
IMG_VOID CalcCtrlDepBlock(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, PUSC_LIST psVisitedEdgeLst, PUSC_LIST psCtrlDepBlockLst, PUSC_LIST psCtrlDepRegonLst)
{
	IMG_UINT32 uSuccIdx;
	for (uSuccIdx = 0; uSuccIdx<psBlock->uNumSuccs; uSuccIdx++)
	{
		if 
		(
			(!PostDominated(psState, psBlock, psBlock->asSuccs[uSuccIdx].psDest))
			&&
			(!IsEdgeInfoPresentInList(psVisitedEdgeLst, psBlock, uSuccIdx))
		)
		{
			{
				PCTRL_DEP_NODE psCtrlDepRegon;				
				if (!IsCtrlDepRegonPresentInList(psCtrlDepRegonLst, psBlock, uSuccIdx, &psCtrlDepRegon))
				{					
					psCtrlDepRegon = AppendCtrlDepRegonToList(psState, psCtrlDepBlockLst, psCtrlDepRegonLst, psBlock, uSuccIdx);					 
				}
				{
					PCODEBLOCK psCtrlDepCodeBlock = psBlock->asSuccs[uSuccIdx].psDest;
					while ((psCtrlDepCodeBlock != NULL) && (!PostDominated(psState, psBlock, psCtrlDepCodeBlock)))
					{
						PCTRL_DEP_NODE psCtrlDepBlock;
						PCTRL_DEP_NODE psCtrlDepNode;
						if (psCtrlDepCodeBlock != psCtrlDepCodeBlock->psOwner->psExit)
						{
							if (!IsCtrlDepBlockPresentInList(psCtrlDepBlockLst, psCtrlDepCodeBlock, &psCtrlDepBlock))
							{
								psCtrlDepBlock = AppendCtrlDepBlockToList(psState, psCtrlDepBlockLst, psCtrlDepCodeBlock);							
							}
							if (!IsCtrlDepNodePresentInList(&(psCtrlDepRegon->u.sRegion.sSuccLst), psCtrlDepBlock, &psCtrlDepNode))
							{
								AppendCtrlDepNodeToList(psState, &(psCtrlDepRegon->u.sRegion.sSuccLst), psCtrlDepBlock);
								AppendCtrlDepNodeToList(psState, &(psCtrlDepBlock->u.sBlock.sPredLst), psCtrlDepRegon);
							}
							psCtrlDepCodeBlock = psCtrlDepCodeBlock->psIPostDom;
						}
					}
				}
			}
		}
		if (!IsEdgeInfoPresentInList(psVisitedEdgeLst, psBlock, uSuccIdx))
		{
			CalcCtrlDepBlock(psState, psBlock->asSuccs[uSuccIdx].psDest, psVisitedEdgeLst, psCtrlDepBlockLst, psCtrlDepRegonLst);
			{
				PEDGE_INFO psEdgeInfo = AppendEdgeInfoToList(psState, psVisitedEdgeLst);
				psEdgeInfo->psEdgeParentBlock = psBlock;
				psEdgeInfo->uEdgeSuccIdx = uSuccIdx;
			}
		}
	}		
}

IMG_INTERNAL
PCTRL_DEP_GRAPH CalcCtrlDep(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
{
	USC_LIST sVisitedEdgeLst;	
	PCTRL_DEP_GRAPH psCtrlDepGraph;
	psCtrlDepGraph = UscAlloc(psState, sizeof(*psCtrlDepGraph));
	InitializeList(&sVisitedEdgeLst);
	InitializeList(&(psCtrlDepGraph->sCtrlDepBlockLst));
	InitializeList(&(psCtrlDepGraph->sCtrlDepRegonLst));
	psCtrlDepGraph->psRootCtrlDepNode = AppendCtrlDepBlockToList(psState, &(psCtrlDepGraph->sCtrlDepBlockLst), psBlock);
	CalcCtrlDepBlock(psState, psBlock, &sVisitedEdgeLst, &(psCtrlDepGraph->sCtrlDepBlockLst), &(psCtrlDepGraph->sCtrlDepRegonLst));
	FreeEdgeInfoList(psState, &sVisitedEdgeLst);
	return psCtrlDepGraph;
}

IMG_INTERNAL
IMG_VOID FreeCtrlDepGraph(PINTERMEDIATE_STATE psState, CTRL_DEP_GRAPH **psCtrlDepGraph)
{
	FreeCtrlDepBlockList(psState, &((*psCtrlDepGraph)->sCtrlDepBlockLst));
	FreeCtrlDepRegonList(psState, &((*psCtrlDepGraph)->sCtrlDepRegonLst));
	UscFree(psState, *psCtrlDepGraph);
	*psCtrlDepGraph = NULL;
}
#endif /* #if defined(EXECPRED) */
