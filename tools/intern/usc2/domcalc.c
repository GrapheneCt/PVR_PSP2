/******************************************************************************
 * Name         : domcalc.c
 * Title        : Calculation of Dominator trees & other CFG stucture analyses
 * Created      : July 2008
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
 * $Log: domcalc.c $
 *****************************************************************************/
#include "uscshrd.h"
#include "bitops.h"

typedef struct _DOM_INFO_
{
	PCODEBLOCK psBlock; //block with this number

	/* all the following are DFS indices of blocks in question (0 for none) */
	IMG_UINT32 uParent; //in DFS tree
	IMG_UINT32 uAncestor; //for EVAL tree
	IMG_UINT32 uBest; //(ex-)ancestor with smallest uSemiDom - for EVAL/COMPRESS only
	IMG_UINT32 uSemiDom;

	IMG_UINT32 uBucket;
} DOM_INFO, *PDOM_INFO;

/* call first with pointer to node you want to eval - 'twill fill with root */
static IMG_UINT32 GetBestOfAnyAncestor(IMG_PUINT32 puAncestor, PDOM_INFO asInfo)
/******************************************************************************
 FUNCTION		: GetBestOfAnyAncestor

 DESCRIPTION	: Finds the ancestor node with the lowest semi-dominator,
				  also updating tree with path compression.

 PARAMETERS		: puAncestor	- points to node whose ancestors to inspect
				  asInfo		- dominator calculation state

 RETURNS		: Index of node with lowest semi-dominator
 OUTPUT			: puAncestor set to highest ancestor of node (path compression)
******************************************************************************/
{
	IMG_UINT32 uNode = *puAncestor, uRes = asInfo[uNode].uBest;
	//printf("GetBest %i...", uNode);
	if (asInfo[uNode].uAncestor)
	{
		IMG_UINT32 uBest = GetBestOfAnyAncestor(&asInfo[uNode].uAncestor, asInfo);
		if (asInfo[uBest].uSemiDom < asInfo[uRes].uSemiDom)
		{
			asInfo[uNode].uBest = uBest;
			uRes = uBest;
		}
		*puAncestor = asInfo[uNode].uAncestor;
	}
	return uRes;
}

static IMG_UINT32 DoDfs(PCODEBLOCK		psBlock,
						IMG_UINT32		uParent,
						IMG_PUINT32		auDfsNum,
						IMG_UINT32		uNextDfsNum,
						PDOM_INFO		asInfo,
						IMG_BOOL		bDoms)
/******************************************************************************
 FUNCTION		: DoDfs

 DESCRIPTION	: Performs a depth-first traversal of a CFG numbering the nodes

 PARAMETERS		: psBlock		- Block to traverse
				  uParent		- DFS number of parent (whence psBlock reached)
				  auDfsNum		- DFS number for each CFG block, indexed by uIdx
				  uNextDfsNum	- Next DFS number to allocate
				  asInfo		- Dominance calculation state
				  bDoms			- Whether to traverse to successors (calculates
								  dominance) or predecessors (--> postdominance)

 RETURNS		: The next DFS number to allocate after this call.
 OUTPUT			: Sets auDfsNum for this block, if not already
******************************************************************************/
{
	if (auDfsNum[psBlock->uIdx] == 0)
	{
		IMG_UINT32 i = (bDoms) ? psBlock -> uNumSuccs : psBlock->uNumPreds;
		IMG_UINT32 uDfsNum = uNextDfsNum++;
		const PCODEBLOCK_EDGE asAdj = (bDoms) ? psBlock->asSuccs : psBlock->asPreds;
		auDfsNum[psBlock->uIdx] = uDfsNum;
		asInfo[uDfsNum].psBlock = psBlock;

		asInfo[uDfsNum].uBest = uDfsNum; //i.e. the node itself
		asInfo[uDfsNum].uSemiDom = uDfsNum;

		asInfo[uDfsNum].uAncestor = 0;
		asInfo[uDfsNum].uBucket = 0;
		
		asInfo[uDfsNum].uParent = uParent;
	
		while (i > 0) //note decrement
		{
			i--;
			uNextDfsNum = DoDfs(asAdj[i].psDest, uDfsNum, auDfsNum, uNextDfsNum, asInfo, bDoms);
		}
	}
	return uNextDfsNum;
}

static IMG_VOID AddToParent(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 auVisited[])
/******************************************************************************
 FUNCTION		: AddToParent
 
 DESCRIPTION	: Adds each node to the apsDomChildren array of its immediate
				  dominator, by a depth-first traversal.

 PARAMETERS		: psState	- Compiler intermediate state
				  psBlock	- Block to traverse
				  auVisited	- bit array of which blocks have been reached in
							  traversal
 
 NOTE			: (Forwards) dominators ONLY, not called for postdominators.
******************************************************************************/
{
	IMG_UINT32 uSucc, uCh;
	//check - have we visited already (or are we visiting atm) ?
	if (GetBit(auVisited, psBlock->uIdx)) return;
	SetBit(auVisited, psBlock->uIdx, IMG_TRUE);

	/*
		At this point in traversal, we cannot have visited any of this node's
		dominated children (as those children can *only* be reached through
		successor edges from this node!).

		Hence, now is safe time to set up array for children to add themselves.
	*/
	if (psBlock->apsDomChildren) UscFree(psState, psBlock->apsDomChildren);
	psBlock->apsDomChildren = UscAlloc(psState,
				psBlock->uNumDomChildren * sizeof(psBlock->apsDomChildren[0]));
	
	//backup uNumDomChildren, as used as 'next index to fill' by children
	uCh = psBlock->uNumDomChildren;
		
	//5b. add each node as child of dominator (by recursive traversal).
	for (uSucc = 0; uSucc < psBlock->uNumSuccs; uSucc++)
	{
		AddToParent(psState, psBlock->asSuccs[uSucc].psDest, auVisited);
	}

	//all children should now have added themselves...
	ASSERT (psBlock->uNumDomChildren == 0);
	//so restore count
	psBlock->uNumDomChildren = uCh;

	//last - but most important:
	if (psBlock->psIDom)
	{
		//add to parent
		IMG_UINT32 uIndexToFill = --psBlock->psIDom->uNumDomChildren;
		psBlock->psIDom->apsDomChildren[uIndexToFill] = psBlock;
		/*
			Note this fills in apsDomChildren in reverse order (highest index
			first); this ensures the children are in topological sort order.
			(If child A reaches child B, B will add itself first because of
			post-order traversal, so will be at a later array index).
		*/
	}
}

IMG_INTERNAL IMG_VOID CalcDoms(PINTERMEDIATE_STATE psState, PCFG psCfg)
/******************************************************************************
 FUNCTION		: CalcDoms

 DESCRIPTION	: Calculates dominator tree and postdominators across a CFG

 PARAMETERS		: psState	- Compiler intermediate state
				  psCfg		- CFG to analyse

 RETURNS		: Nothing, but sets apsDomChildren, psIDom & psIPostDom
				  for all blocks in the function
******************************************************************************/
{
	//mapping from (block uIdx) to DFS# - NOTE DFS#'s start at 1
	IMG_PUINT32 auDfsNum = (IMG_PUINT32)UscAlloc(psState,
									psCfg->uNumBlocks * sizeof(IMG_UINT32));
	//array of dom_info structures, indexed by DFS# - NOTE INDEX 0 IS UNUSED
	PDOM_INFO asInfo = (PDOM_INFO)UscAlloc(psState,
								(psCfg->uNumBlocks + 1) * sizeof(DOM_INFO));

	IMG_UINT32 i;
	IMG_BOOL bDoms = IMG_TRUE; //FALSE --> calculating POST-dominators...
	
	for (i = 0; i < psCfg->uNumBlocks; i++)
	{
		psCfg->apsAllBlocks[i]->psIDom = NULL;
		psCfg->apsAllBlocks[i]->psIPostDom = NULL;
	}

	for (;;)
	{
		IMG_UINT uNumReachableBlocks;

		memset(auDfsNum, 0, psCfg->uNumBlocks * sizeof(IMG_UINT32));
		uNumReachableBlocks = DoDfs(bDoms ? psCfg->psEntry : psCfg->psExit,
				  0, auDfsNum, 1, asInfo, bDoms) - 1;

		if (uNumReachableBlocks != psCfg->uNumBlocks)
		{
			//DFS didn't reach all blocks...
			if (bDoms)
			{
				/*
					and was traversing on forward edges! So all unreachable
					blocks should have been deleted. Must be that we just can't
					reach the exit...(i.e. program never terminates)
				*/
				ASSERT (psCfg->psExit->uNumPreds == 0); //exit never deleted
				ASSERT (uNumReachableBlocks == psCfg->uNumBlocks - 1); //but rest is!
				//continue, setting dominators in the reachable part
			}
			else
			{
				// some/all nodes have no paths to exit.
				if (psCfg->psExit->uNumPreds == 0)
				{
					/*
						exit is unreachable (under any circumstances)
						(all other unreachable nodes are deleted!)
						so postdominance is meaningless.
					*/
					break;
				}
				/*
					else - some nodes don't reach the exit (e.g. inf loops)
					but other nodes do; continue to calculate postdominators
					for the latter, only.
				*/
			}
		}

		for (i = uNumReachableBlocks; i > 0; i--)
		{
			IMG_UINT32 u;
			/*
				1. clear bucket (i.e. work list for this node, filled in by DFS children).
				For leaf nodes (visited before parents!) this'll be empty.
			*/
			for (u = asInfo[i].uBucket; u; u = asInfo[u].uBucket)
			{
				IMG_UINT32 uTmp		= u,
						   uBest	= GetBestOfAnyAncestor(&uTmp, asInfo),
						   uNewBest = (asInfo[uBest].uSemiDom < i) ? uBest : i;
				
				ASSERT (asInfo[u].uSemiDom == i);

				if (bDoms)
				{
					asInfo[u].psBlock->psIDom = asInfo[uNewBest].psBlock;
				}
				else
				{
					asInfo[u].psBlock->psIPostDom = asInfo[uNewBest].psBlock;
				}
			}
			//2. set semi-dominator. (this'll actually *do something* before previous step)
			for (u = bDoms ? asInfo[i].psBlock->uNumPreds : asInfo[i].psBlock->uNumSuccs; u-- > 0;)
			{
				PCODEBLOCK_EDGE asAdj = (bDoms) ? asInfo[i].psBlock->asPreds
												: asInfo[i].psBlock->asSuccs;
				IMG_UINT32 uPred = auDfsNum[asAdj[u].psDest->uIdx], uBest;
				if (uPred == 0)
				{
					/*
						node was not reached during DFS
						(but we're now going along edges in opposite direction)
						so skip
					*/
					continue;
				}
				uBest = GetBestOfAnyAncestor(&uPred, asInfo);
				//if (asInfo[i].uSemiDom > asInfo[uBest].uSemiDom)
				//	asInfo[i].uSemiDom = asInfo[uBest].uSemiDom;
				asInfo[i].uSemiDom = min(asInfo[i].uSemiDom, asInfo[uBest].uSemiDom);
			}

			//3. add node i to bucket of semi[i] (for processing at loop start)
			asInfo[i].uBucket = asInfo[asInfo[i].uSemiDom].uBucket;
			asInfo[asInfo[i].uSemiDom].uBucket = i;
			asInfo[i].uAncestor = asInfo[i].uParent;
		}

		/*
			4. DFS-order pass, setting (post-)dominators,
			and (for forwards dominators only) counting children
		*/
		if (bDoms) psCfg->psEntry->uNumDomChildren = 0;
		for (i = 2; i <= uNumReachableBlocks; i++)
		{
			PCODEBLOCK *ppsImmed = (bDoms) ? &asInfo[i].psBlock->psIDom
										   : &asInfo[i].psBlock->psIPostDom;
			if (*ppsImmed != asInfo[asInfo[i].uSemiDom].psBlock)
			{
				*ppsImmed = (bDoms) ? (*ppsImmed)->psIDom
									: (*ppsImmed)->psIPostDom;
			}
			if (bDoms)
			{
				//initialise count - ok here as our DFS# is < that of any child
				asInfo[i].psBlock->uNumDomChildren = 0;
				(*ppsImmed)->uNumDomChildren++;
			}
		}
		
		if (bDoms == IMG_FALSE) break; //done both
		bDoms = IMG_FALSE; //no, done dominators - so postdominators next.
	}
	UscFree(psState, auDfsNum);
	UscFree(psState, asInfo);

	//5. set up child arrays (dominators only)...
	{
		IMG_UINT32 *auVisited = UscAlloc(psState,
				UINTS_TO_SPAN_BITS(psCfg->uNumBlocks) * sizeof(IMG_UINT32));
		memset(auVisited, 0, UINTS_TO_SPAN_BITS(psCfg->uNumBlocks) * sizeof(IMG_UINT32));
		AddToParent(psState, psCfg->psEntry, auVisited);
		UscFree(psState, auVisited);
	}
}

IMG_INTERNAL
IMG_BOOL Dominates(PINTERMEDIATE_STATE psState, PCODEBLOCK psDom, PCODEBLOCK psCh)
/******************************************************************************
 FUNCTION		: Dominates

 DESCRIPTION	: Determines whether one codeblock dominates another

 PARAMETERS		: psState	- Compiler intermediate state
				  psDom		- (Node to check whether is) dominator
				  psCh		- (Node to check whether is) child (in dom. tree)

 RETURNS		: IMG_TRUE iff psDom dominates psCh (strictly or otherwise)
******************************************************************************/
{
	PCFG psCfg = psDom->psOwner;
	PVR_UNREFERENCED_PARAMETER(psState);
	if (psCfg != psCh->psOwner)
#if 0
	{
		PFUNC psCfg; PINST psInst;
		IMG_BOOL bSeenDom = IMG_FALSE, bSeenCh = IMG_FALSE;
		ASSERT (psDom->psOwner != psState->psSecAttrProg && psCh->psOwner != psState->psSecAttrProg);
		/*
			Dominance across functions holds if psDom dominates every call to the function
			containing psCh (!). Hence, firstly we require that fn to be *strictly* more
			nested than the one containing psDom...
			(However, hoping this is unnecessary overkill for now!)
		*/
		for (psCfg.psFunc = psState->psFnOutermost; !bSeenCh; psCfg.psFunc = psFunc->psFnNestInner)
		{
			if (psDom->psOwner == psCfg.psFunc) bSeenDom = IMG_TRUE;
			if (psCh->psOwner == psCfg.psFunc) bSeenCh = IMG_TRUE;
		}
		//seen ch's containing function
		if (!bSeenDom) return IMG_FALSE; //more nested!!
		ASSERT (psCh->psOwner == psCfg);
		//Terminates as every call to psCh's containing function must be less nested than said fn itself
		for (psInst = psCfg.psFunc->psCallSiteHead; psInst; psInst = psInst->psCall->psCallSiteNext)
		{
			if (!Dominates(psState, psDom, psInst->psCall->psBlock)) return IMG_FALSE;
		}
		//yup...(!)
		return IMG_TRUE;
	}
#else
		return IMG_FALSE;
#endif
	ASSERT (psCfg->bBlockStructureChanged == IMG_FALSE);

	for (; psCh; psCh = psCh->psIDom)
		if (psCh == psDom) return IMG_TRUE;
	
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_BOOL PostDominated(PINTERMEDIATE_STATE psState, PCODEBLOCK psCh, PCODEBLOCK psPDom)
/******************************************************************************
 FUNCTION		: PostDominated

 DESCRIPTION	: Determines whether one codeblock is postdominated by another

 PARAMETERS		: psState	- Compiler intermediate state
				  psCh		- First node (thought to come before postdominator)
				  psPDom	- Potential postdominator (must be executed later)

 RETURNS		: IMG_TRUE if psPDom postdominates psCh (strictly or otherwise)
******************************************************************************/
{
	PCFG psCfg = psPDom->psOwner;
	PVR_UNREFERENCED_PARAMETER(psState);

	if (psCfg != psCh->psOwner) return IMG_FALSE;
	//could do same crazy cross-function thing as in Dominates, but not now!!
	
	ASSERT (psCfg->bBlockStructureChanged == IMG_FALSE);
	
	for (; psCh; psCh = psCh->psIPostDom)
		if (psCh == psPDom) return IMG_TRUE;
	
	return IMG_FALSE;
}

static IMG_VOID MarkInLoop(PINTERMEDIATE_STATE psState, PCODEBLOCK psHeader, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION	: MarkInLoop

 PURPOSE	: Marks nodes as being in a loop (part of ComputeLoopNestingTree)

 PARAMETERS	: psState	- Compiler intermediate statu
			  psHeader	- Header of the loop whose body to mark
			  psBlock	- Node in the loop to mark, along with predecessors

 RETURNS	: Nothing
******************************************************************************/
{
	IMG_UINT32 uPred;
	//take account of any existing marking...
	while (psBlock->psLoopHeader)
	{
		if (psBlock->psLoopHeader == psHeader)
		{
			/*
				Node already marked as being part of the same loop. Either
				- we've got back to the loop header, and can stop marking
					(ComputeLoopNestingTree temporarily sets every block as its
					own header before calling MarkInLoop, below); or
				- we've reached a node already marked as being in our loop, by
					some other call to MarkInLoop (i.e. it must have multiple
					in-loop successors, and an earlier call to MarkInLoop on
					a different one of those marked this node - presumably
					this node is an IF in the loop) - in which case said
					earlier call has already marked psBlock's predecessors
			*/
			return;
		}
		/*
			node must belong to a loop nested within this one, i.e. is already
			marked with the inner header. So skip the nodes in the inner loop.
			Resume backwards traversal by marking the header of the inner loop,
			which must be in the outer loop (as loops are well-nested!).
		*/
		psBlock = psBlock->psLoopHeader;
	}
	
	//mark node as being in our loop
	psBlock->psLoopHeader = psHeader;
	
	/*
		recurse on predecessors, as *these must also be in the loop*
		(the loop is reducible, so the only way to get "out" of the loop along
		a backwards control-flow edge is through the loop header)
	*/
	for (uPred = 0; uPred < psBlock->uNumPreds; uPred++)
		MarkInLoop(psState, psHeader, psBlock->asPreds[uPred].psDest);
}

IMG_INTERNAL
IMG_VOID ComputeLoopNestingTree(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: ComputeLoopNestingTree

 DESCRIPTION	: Marks nodes in a CFG with the header of the smallest strictly
				  enclosing loop (i.e., loop header nodes are marked with the
				  header of the next loop outside their own). Call on the entry
				  node of the function to mark all nodes in it.
 
 PARAMETERS		: psState	- Compiler intermediate state
				  psBlock	- Block to traverse (in dominator tree)
 
 RETURNS		: Nothing, but sets psLoopHeader of blocks dominated by psBlock
 *****************************************************************************/
{
	IMG_UINT32 uIdx;
	/*
		Ok to reset psLoopHeader now, as psBlock is definitely not in
		an inner loop (i.e. whose header it dominates!)
	*/
	psBlock->psLoopHeader = NULL; //ok to clear now, not in any _inner_ loop
	
	//recurse on children, so nodes in inner loops are marked first
	for (uIdx = 0; uIdx < psBlock->uNumDomChildren; uIdx++)
	{
		ComputeLoopNestingTree(psState, psBlock->apsDomChildren[uIdx]);
	}

	//set psLoopHeader as a marked to prevent infinite recursion of MarkInLoop
	psBlock->psLoopHeader = psBlock;
	/*
		mark nodes in any loop for which psBlock is the header.
		We do this by looking for loop backedges whose destination is
		psBlock; these are listed among psBlock's predecessors, and
		distinguished by psBlock dominating the edge *source*...
	*/
	for (uIdx = 0; uIdx < psBlock->uNumPreds; uIdx++)
	{
		if (Dominates(psState, psBlock, psBlock->asPreds[uIdx].psDest))
		{
			/*
				predecessor is the source of a loop backedge,
				so psBlock is it's header
			*/
			MarkInLoop(psState, psBlock, psBlock->asPreds[uIdx].psDest);
		}
	}
	/*
		finally clear psLoopHeader: psBlock isn't *stored* or *marked* as being
		in its own loop, as we can easily see whether a block is a loop header
		without this information (see IsLoopHeader below).
		
		If psBlock is in any *outer* loop, the outer loop's header must dominate
		psBlock, so the call to ComputeLoopNestingTree on that node will enclose
		this call, and will mark psBlock later (postorder traversal)
	*/
	psBlock->psLoopHeader = NULL;
}

IMG_INTERNAL IMG_BOOL IsLoopHeader(PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: IsLoopHeader
 
 DESCRIPTION	: Determines whether a block is header of a loop (includes the
				  case where the block itself is the only node in its loop)
 
 PARAMETERS		: psBlock	- block to examine
 
 RETURNS		: IMG_TRUE iff the specified block is header of a loop
******************************************************************************/
{
	IMG_UINT32 uSucc;
	//any loop body must include at least one successor!
	for (uSucc = 0; uSucc < psBlock->uNumSuccs; uSucc++)
	{
		PCODEBLOCK psSucc = psBlock->asSuccs[uSucc].psDest;
		if (psSucc == psBlock || psSucc->psLoopHeader == psBlock) return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_UINT32 LoopDepth(PCODEBLOCK psBlock)
/******************************************************************************
 FUNCTION		: LoopDepth

 DESCRIPTION	: Computes the loop nesting depth of a block.

 PARAMETERS		: psBlock	- block to inspect

 RETURNS		: 0 if block is not in a loop, 1 = in an outermost loop, etc.
				  
 NOTE 1			: For purposes of this function, loop header nodes *are*
				  considered to be in their own loops.

 NOTE 2			: Correct iff ComputeLoopNestingTree has been called on the CFG
******************************************************************************/
{
	IMG_UINT32 uLoopDepth = IsLoopHeader(psBlock) ? 1 : 0;
	//outer / strictly containing loops?
	for (;psBlock->psLoopHeader; psBlock = psBlock->psLoopHeader) uLoopDepth++;
	return uLoopDepth;
}

#define INPATH_FLAG	0x80000000

static
PCODEBLOCK BlockTreeParent(PCODEBLOCK psBlock, IMG_BOOL bPostDom)
/******************************************************************************
 FUNCTION		: BlockTreeParent

 DESCRIPTION	: Return the parent of a block in either the dominator or
				  post-dominator trees.

 PARAMETERS		: psBlock		- Block to return the parent of.
				  bPostDom		- If TRUE return the parent in the post-dominator
								tree.
								  If FALSE return the parent in the dominator
							    tree.

 RETURNS		: A pointer to the parent.
******************************************************************************/
{
	return bPostDom ? psBlock->psIPostDom : psBlock->psIDom;
}

IMG_INTERNAL
PCODEBLOCK FindLeastCommonDominator(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock1, PCODEBLOCK psBlock2, IMG_BOOL bPostDom)
/******************************************************************************
 FUNCTION		: FindLeastCommonDominator

 DESCRIPTION	: Find a block which dominates both of two specified blocks
				  and is dominates by any other block with the same
				  property.

 PARAMETERS		: psBlock1, psBlock2	- Blocks to find a common dominator for.
				  bPostDom				- If TRUE find the least common post-dominator
										instead.

 RETURNS		: Nothing
******************************************************************************/
{
	PCODEBLOCK	psLCA;
	PCODEBLOCK	psWalker;

	ASSERT(psBlock1->psOwner == psBlock2->psOwner);
	
	/*
		Mark all the blocks on the path between psBlock1 and the function entry (or
		exit for post-dominators).
	*/
	psLCA = NULL;
	for (psWalker = psBlock1; psWalker != NULL; psWalker = BlockTreeParent(psWalker, bPostDom))
	{
		psWalker->uIdx |= INPATH_FLAG;

		/*
			Check if psBlock2 dominates (post-dominates) psBlock1.
		*/
		if (psWalker == psBlock2)
		{
			psLCA = psBlock2;
			break;
		}
	}
	if (psLCA == NULL)
	{
		/*
			Find the first block on the path between psBlock2 and the function entry (exit) which
			also dominates (post-dominates) psBlock1.
		*/
		for (psWalker = psBlock2; psWalker != NULL; psWalker = BlockTreeParent(psWalker, bPostDom))
		{
			if ((psWalker->uIdx & INPATH_FLAG) != 0)
			{
				psLCA = psWalker;
				break;
			}
		}
	}
	/*
		Reset the flag marking ancestors of psBlock1.
	*/
	for (psWalker = psBlock1; psWalker != NULL; psWalker = BlockTreeParent(psWalker, bPostDom))
	{
		if ((psWalker->uIdx & INPATH_FLAG) != 0)
		{
			psWalker->uIdx &= ~INPATH_FLAG;
		}
		else
		{
			break;
		}
	}
	return psLCA;
}

/* EOF */
