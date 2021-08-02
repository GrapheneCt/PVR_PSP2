/*************************************************************************
 * Name         : dgraph.c
 * Title        : Dependency Graphs
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
 * $Log: dgraph.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include <limits.h>

/*
	The type of ELEMENTS of the USC_PARRAYs "psRUsers" in a DGRAPH_STATE
	(for R = Temp, PA, Output, Pred, IReg). That is, for reg #x of that type,
	psRUsers[x] is a PINT_LIST, where
		the first element is the index of the last instr to write the register
			(-1, or the entire list null, for no write seen so far);
		any subsequent elements are indices of instructions reading the register
			(usually _since_ the last write, except for alpha-splitting on PAs).
*/
typedef struct _INT_LIST_
{
	IMG_UINT32 uInst;
	struct _INT_LIST_ *psNext;
} INT_LIST, *PINT_LIST;

static IMG_VOID FreeIntList(PINTERMEDIATE_STATE psState, PINT_LIST *ppsIntList)
/******************************************************************************
 FUNCTION	: FreeIntList
 
 PURPOSE	: Frees up an INT_LIST structure; castable to USC_STATE_FREEFN
			  so can be passed to ClearArray

 PARAMETERS	: psState		- Compiler intermediate state
			  ppsIntList	- identifies pointer to structure to free

 RETURNS	: Nothing, but sets (*ppsIntList) to NULL.
******************************************************************************/
{
	PINT_LIST psElem, psNext;
	for (psElem = *ppsIntList; psElem; psElem = psNext)
	{
		psNext = psElem->psNext;
		UscFree(psState, psElem);
	}
	*ppsIntList = NULL;
}

IMG_INTERNAL
PDGRAPH_STATE NewDGraphState(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: NewDGraphState
    
 PURPOSE	: Allocate a dependency graph state

 PARAMETERS	: psState	- Compiler State
			  
 RETURNS	: New DGraph state or NULL on failure.
*****************************************************************************/
{
	PDGRAPH_STATE psDGraph;
	IMG_UINT32	uIdx;

	/* Allocate state */
	psDGraph = UscAlloc(psState, sizeof(DGRAPH_STATE));
	if (psDGraph == NULL)
	{
		return NULL;
	}

	/* Instruction count */
	psDGraph->uBlockInstructionCount = 0;

	/* Instruction array */
	psDGraph->psInstructions = NewArray(psState, USC_MIN_ARRAY_CHUNK, 
										NULL, sizeof(PINST));
	
	/* Main dependency */
	psDGraph->psMainDep = NewArray(psState, USC_MIN_ARRAY_CHUNK, 
								   (IMG_PVOID)(IMG_UINTPTR_T)USC_MAINDEPENDENCY_NONE,
								   sizeof(IMG_UINT32));

	/* Dependency graphs */
	psDGraph->psDepGraph = NewGraph(psState, USC_MIN_ARRAY_CHUNK, 
									(IMG_PVOID)0, GRAPH_PLAIN);
	psDGraph->psClosedDepGraph = NewGraph(psState, USC_MIN_ARRAY_CHUNK, 
										  (IMG_PVOID)0, GRAPH_PLAIN);

	/* Register writers */
	psDGraph->psTempUsers = NewArray(psState, USC_MIN_ARRAY_CHUNK,
									 NULL, sizeof(USC_PARRAY));

	psDGraph->psPAUsers = NewArray(psState, USC_MIN_ARRAY_CHUNK, 
								   NULL, sizeof(USC_PARRAY));
	
	psDGraph->psOutputUsers = NewArray(psState, USC_MIN_ARRAY_CHUNK, 
									   NULL, sizeof(USC_PARRAY));
	
	psDGraph->psIRegUsers = NewArray(psState, USC_MIN_ARRAY_CHUNK,
									 NULL, sizeof(USC_PARRAY));

	psDGraph->psIndexUsers = NewArray(psState, USC_MIN_ARRAY_CHUNK,
									  NULL, sizeof(USC_PARRAY));

	psDGraph->psPredUsers = NewArray(psState, USC_MIN_ARRAY_CHUNK, 
									 NULL, sizeof(USC_PARRAY));

	if (psState->uNumVecArrayRegs > 0)
	{
		psDGraph->apsRegArrayLastWriter = UscAlloc(psState,
													(sizeof(USC_PARRAY) *
													 psState->uNumVecArrayRegs));
		for (uIdx = 0; uIdx < psState->uNumVecArrayRegs; uIdx++)
		{
			psDGraph->apsRegArrayLastWriter[uIdx] = NewArray(psState, USC_MIN_ARRAY_CHUNK, 
															  (IMG_PVOID)(IMG_UINTPTR_T)USC_UNDEF, sizeof(IMG_UINT32));
		}
	}
	else
	{
		psDGraph->apsRegArrayLastWriter = NULL;
	}
	

	/* Compiler state */
	psDGraph->psState = psState;

	/* Dependency counters */
	psDGraph->psDepCount = NewArray(psState, 
									USC_MIN_ARRAY_CHUNK, 
									(IMG_PVOID)0,
									sizeof(IMG_UINT32));
	psDGraph->psSatDepCount = NewArray(psState, 
									   USC_MIN_ARRAY_CHUNK, 
									   (IMG_PVOID)0,
									   sizeof(IMG_UINT32));

	/* Dependency list. */
	psDGraph->psDepList = NewArray(psState, USC_MIN_ARRAY_CHUNK, NULL, sizeof(IMG_PVOID));

	return psDGraph;
}

IMG_INTERNAL
IMG_VOID FreeDGraphState(PINTERMEDIATE_STATE psState,
						 PDGRAPH_STATE *ppsDGraph)
/*****************************************************************************
 FUNCTION	: FreeDGraphState
    
 PURPOSE	: Free a dependency graph state

 PARAMETERS	: psDepState	- Compiler State
			  ppDGraph		- dependence graph to free.
			  
 RETURNS	: Nothing
*****************************************************************************/
{
	PDGRAPH_STATE psDGraph = *ppsDGraph;
	IMG_UINT32 uIdx;

	/* Dependency list. */
	ClearArray(psState, psDGraph->psDepList, DeleteAdjacencyListIndirect);
	FreeArray(psState, &psDGraph->psDepList);

	/* Dependency counters */
	FreeArray(psState, &psDGraph->psDepCount);
	FreeArray(psState, &psDGraph->psSatDepCount);

	/* Register writers */
	ClearArray(psState, psDGraph->psTempUsers, (USC_STATE_FREEFN)FreeIntList);
	FreeArray(psState, &psDGraph->psTempUsers);
	ClearArray(psState, psDGraph->psPAUsers, (USC_STATE_FREEFN)FreeIntList);
	FreeArray(psState, &psDGraph->psPAUsers);
	ClearArray(psState, psDGraph->psOutputUsers, (USC_STATE_FREEFN)FreeIntList);
	FreeArray(psState, &psDGraph->psOutputUsers);
	ClearArray(psState, psDGraph->psIRegUsers, (USC_STATE_FREEFN)FreeIntList);
	FreeArray(psState, &psDGraph->psIRegUsers);
	ClearArray(psState, psDGraph->psIndexUsers, (USC_STATE_FREEFN)FreeIntList);
	FreeArray(psState, &psDGraph->psIndexUsers);
	ClearArray(psState, psDGraph->psPredUsers, (USC_STATE_FREEFN)FreeIntList);
	FreeArray(psState, &psDGraph->psPredUsers);
	if (psDGraph->apsRegArrayLastWriter != NULL)
	{
		for (uIdx = 0; uIdx < psState->uNumVecArrayRegs; uIdx++)
		{
			FreeArray(psState, &psDGraph->apsRegArrayLastWriter[uIdx]);
		}
		UscFree(psState, psDGraph->apsRegArrayLastWriter);
	}

	/* Dependency graphs */
	FreeGraph(psState, &psDGraph->psDepGraph);
	FreeGraph(psState, &psDGraph->psClosedDepGraph);

	/* Main dependency */
	FreeArray(psState, &psDGraph->psMainDep);
	/* Instructions */
	FreeArray(psState, &psDGraph->psInstructions);

	/* Free dependence state */
	UscFree(psState, psDGraph);
	*ppsDGraph = NULL;
}


IMG_INTERNAL
IMG_UINT32 AddNewInstToDGraph(PDGRAPH_STATE psDepState, PINST psNewInst)
/*********************************************************************************
 Function			: AddNewInstToDGraph

 Description		: Add a new instruction to the dependency graph structure.
 
 Parameters			: psDepState	- Module state.
					  psNewInst		- Pointer to the new instruction.

 Globals Effected	: None

 Return				: ID of the new instruction.
*********************************************************************************/
{
	PINTERMEDIATE_STATE psState = psDepState->psState;
	IMG_UINT32 uNewInst;

	uNewInst = psDepState->uBlockInstructionCount++;
	ArraySet(psState, psDepState->psInstructions, uNewInst, psNewInst);

	psNewInst->uId = uNewInst;

	GraphClearCol(psState, psDepState->psDepGraph, uNewInst);
	GraphClearCol(psState, psDepState->psClosedDepGraph, uNewInst);

	ArraySet(psState, psDepState->psDepCount, uNewInst, (IMG_PVOID)0);
	ArraySet(psState, psDepState->psSatDepCount, uNewInst, (IMG_PVOID)0);

	ArraySet(psState, psDepState->psDepList, uNewInst, NULL);

	psDepState->uAvailInstCount++;

	AppendToList(&psDepState->sAvailableList, &psNewInst->sAvailableListEntry);

	return uNewInst;
}

IMG_INTERNAL 
PINST GetNextAvailable(PDGRAPH_STATE psDepState, PINST psStartInst)
/*********************************************************************************
 Function			: GetNextAvailable

 Description		: Gets the next instruction with no unsatisfied dependencies.
 
 Parameters			: psDepState	- Module state.
					  psStartInst	- Point to start looking for instructions.

 Globals Effected	: None

 Return				: The instruction found or NULL if none are available.
*********************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	if (psStartInst == NULL)
	{
		psListEntry = psDepState->sAvailableList.psHead;
	}
	else
	{
		psListEntry = psStartInst->sAvailableListEntry.psNext;
	}
	if (psListEntry == NULL)
	{
		return NULL;
	}
	return IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
}

static
IMG_VOID AddToDepList(PDGRAPH_STATE	psDepState,
					  IMG_UINT32 uFrom,
					  IMG_UINT32 uTo)
/*********************************************************************************
 Function			: AddToDepList

 Description		: Adds an dependency to the adjacency list data structure.
 
 Parameters			: psDepState	- Module state.
					  uFrom, uTo	- Dependency to add.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PADJACENCY_LIST	psList;
	PINTERMEDIATE_STATE	psState = psDepState->psState;

	psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uFrom);
	if (psList == NULL)
	{
		psList = UscAlloc(psState, sizeof(*psList));
		InitialiseAdjacencyList(psList);
		ArraySet(psState, psDepState->psDepList, uFrom, (IMG_PVOID)psList);
	}
	AddToAdjacencyList(psState, psList, uTo);
}

static
IMG_VOID AddToDepGraph(PDGRAPH_STATE psDepState,
					   IMG_UINT32 uFrom,
					   IMG_UINT32 uTo)
/*********************************************************************************
 Function			: AddToDepGraph

 Description		: Adds an dependency to the graph data structure.
 
 Parameters			: psDepState	- Module state.
					  uFrom, uTo	- Dependency to add.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32			uDepCount, uSatDepCount;
	IMG_BOOL			bWasAvailable;
	PINTERMEDIATE_STATE	psState = psDepState->psState;

	ASSERT(!GraphGet(psState, psDepState->psDepGraph, uFrom, uTo));

	GraphSet(psState, psDepState->psDepGraph, uTo, uFrom, IMG_TRUE);
	
	uDepCount = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psDepState->psDepCount, uTo);
	uSatDepCount = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psDepState->psSatDepCount, uTo);

	bWasAvailable = (uDepCount == uSatDepCount);

	ArrayElemOp(psState, psDepState->psDepCount, USC_VEC_ADD, uTo, 1);
	
	if (ArrayGet(psState, psDepState->psInstructions, uFrom) == NULL)
	{
		ArrayElemOp(psState, psDepState->psSatDepCount, USC_VEC_ADD, uTo, 1);
	}
	
	uDepCount = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psDepState->psDepCount, uTo);
	uSatDepCount = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psDepState->psSatDepCount, uTo);

	if (uDepCount - uSatDepCount == 1 && bWasAvailable)
	{
		PINST	psTo = (PINST)ArrayGet(psState, psDepState->psInstructions, uTo);

		ASSERT(psDepState->uAvailInstCount > 0);
		psDepState->uAvailInstCount--;

		RemoveFromList(&psDepState->sAvailableList, &psTo->sAvailableListEntry);
		
		psTo->sAvailableListEntry.psNext = psTo->sAvailableListEntry.psPrev = NULL;
	}
}

IMG_INTERNAL
IMG_VOID AddDependency(PDGRAPH_STATE psDepState, 
					   IMG_UINT32 uFrom, 
					   IMG_UINT32 uTo)
/*********************************************************************************
 Function			: AddDependency

 Description		: Inserts a dependency between two instructions.
 
 Parameters			: psDepState	- Module state.
					  uFrom, uTo	- Instructions to insert the dependency between.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PINTERMEDIATE_STATE	psState = psDepState->psState;
	const IMG_UINT32			uFromMainDep = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, 
																	psDepState->psMainDep, 
																	uFrom);

	//test if anything to do
	if (GraphGet(psState, psDepState->psDepGraph, uTo, uFrom)) return;

	if (uFromMainDep == USC_MAINDEPENDENCY_NONE || 
		uFromMainDep == uTo)
	{
		ArraySet(psState, psDepState->psMainDep, uFrom, (IMG_PVOID)(IMG_UINTPTR_T)uTo);
	}
	else
	{
		ArraySet(psState, psDepState->psMainDep, 
				 uFrom, (IMG_PVOID)(IMG_UINTPTR_T)USC_MAINDEPENDENCY_MULTIPLE);
	}

	AddToDepGraph(psDepState, uFrom, uTo);

	AddToDepList(psDepState, uFrom, uTo);
}

IMG_INTERNAL
IMG_VOID AddClosedDependency(PINTERMEDIATE_STATE psState, PDGRAPH_STATE	psDepState, IMG_UINT32 uFrom, IMG_UINT32 uTo)
/*****************************************************************************
 FUNCTION	: AddClosedDependency
    
 PURPOSE	: Add a dependency between two instructions and update the closed
			  version of the dependency graph.

 PARAMETERS	: psState			- Compiler state.
			  uTo, uFrom		- uTo is made dependent on uFrom.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uFrom != UINT_MAX);
	ASSERT(uTo != UINT_MAX);
	ASSERT(uFrom != uTo);
	ASSERT(!GraphGet(psState, psDepState->psClosedDepGraph, uFrom, uTo));

	if (!GraphGet(psState, psDepState->psClosedDepGraph, uTo, uFrom))
	{
		AddDependency(psDepState, uFrom, uTo);

		GraphSet(psDepState->psState, psDepState->psClosedDepGraph,
				 uTo, uFrom, IMG_TRUE); 
		UpdateClosedDependencyGraph(psDepState, uTo, uFrom);
	}
}

static IMG_VOID FindReaders(PDGRAPH_STATE	psDepState, 
							PINT_LIST		psUsers,
							IMG_UINT32		uInst,
							IMG_BOOL		bClearReaders)
/******************************************************************************
 FUNCTION		: FindReaders

 DESCRIPTION	: Adds dependencies from all readers in an INT_LIST to an inst

 PARAMETERS		: psDepState	- Dependency graph state
				  psUsers		- Instructions reading/writing some register
				  uInst			- Instruction to make target of dependencies
				  bClearReaders	- Whether to remove the readers from the list
					(if so, also adds a dependency from any previous writer)

 RETURNS		: Nothing
******************************************************************************/
{
	PINTERMEDIATE_STATE psState = psDepState->psState;
	PINT_LIST psReaders;

	ASSERT(psUsers);

	//first element of psUsers is prev writer, so skip. Later elems are readers
	for (psReaders = psUsers->psNext; psReaders; psReaders = psReaders->psNext)
	{
		if (psReaders->uInst != uInst) //instruction might have read reg itself
		{
			AddDependency(psDepState, psReaders->uInst, uInst);
		}
	}
	
	if (bClearReaders)
	{
		/*
			Instruction writes (entire) reg;
			so ensure we won't make deps from earlier readers to any later writers
		*/
		if (psUsers->uInst != UINT_MAX)
		{
			/* add dependency on previous writer */
			AddDependency(psDepState, psUsers->uInst, uInst);
		}
		FreeIntList(psState, &psUsers->psNext);
	}
}

static IMG_VOID FindRegArrayReaders(PDGRAPH_STATE	psDepState, 
									IMG_UINT32		uStartInst, 
									IMG_UINT32		uIndex,
									IMG_UINT32		uArrayNum, 
									IMG_UINT32		uArrayOffset)
/*********************************************************************************
 Function			: FindRegArrayReaders

 Description		: Find the readers of the previous value of a register in an
					  indexable array.
 
 Parameters			: psDepState - Current module state.
					  uStartInst - Instruction after the end of the region to find 
								   previous readers in.
					  uIndex	 - Details of the register written.
					  uArrayNum
					  uArrayOffset

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PINTERMEDIATE_STATE psState = psDepState->psState;
	IMG_INT32	iInst;

	for (iInst = (uStartInst - 1); iInst >= 0; iInst--)
	{
		PINST psInst = ArrayGet(psState, psDepState->psInstructions, iInst);
		IMG_UINT32 uArg;
		IMG_UINT32	uDestIdx;

		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			/*
				Check for a read from the same array.
			*/
			if (psInst->asArg[uArg].uType == USC_REGTYPE_REGARRAY &&
				psInst->asArg[uArg].uNumber == uArrayNum)
			{
				/*
					Add dependencies on previous readers:-
					For a non-indexed write:
						any indexed read or a non-indexed read of the same register
					For an indexed write
						any read
				*/
				if (
						psInst->asArg[uArg].uArrayOffset == uArrayOffset ||
						uIndex != USC_REGTYPE_NOINDEX ||
						psInst->asArg[uArg].uIndexType != USC_REGTYPE_NOINDEX
				   )
				{
					AddDependency(psDepState, iInst, uStartInst);
				}
			}
		}

		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			/*
				Check for a write to the same array.
			*/
			if (psInst->asDest[uDestIdx].uType == USC_REGTYPE_REGARRAY &&
				psInst->asDest[uDestIdx].uNumber == uArrayNum)
			{
				/*
					Add dependencies on a previous write to exactly the
					same register or if either the current write or
					the previous one are indexed.
				*/
				if (psInst->asDest[uDestIdx].uArrayOffset == uArrayOffset ||
					psInst->asDest[uDestIdx].uIndexType != USC_REGTYPE_NOINDEX ||
					uIndex != USC_REGTYPE_NOINDEX)
				{
					AddDependency(psDepState, iInst, uStartInst);

					/*
						For an indexed write add a dependency on all previous
						non-indexed writes. Once we see another indexed write
						we can stop because that write will itself be
						dependent on all previous writes.
					*/
					if (uIndex == USC_REGTYPE_NOINDEX ||
						psInst->asDest[uDestIdx].uIndexType != USC_REGTYPE_NOINDEX)
					{
						return;
					}
				}
			}
		}
	}
}

#if 0
/*********************************************************************************
 Function			: CheckClosedGraph

 Description		: Check that the dependency graph is consistent with its
					  transistive closure.
 
 Parameters			: psDepState - Current module state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
static IMG_VOID CheckClosedGraph(PDGRAPH_STATE psDepState)
{
	PINTERMEDIATE_STATE psState = psDepState->psState;
	IMG_UINT32	uInst1, uInst2, uInst3;

	for (uInst1 = 0; uInst1 < psDepState->uBlockInstructionCount; uInst1++)
	{
		for (uInst2 = 0; uInst2 < psDepState->uBlockInstructionCount; uInst2++)
		{
			IMG_BOOL	bDepend = IMG_FALSE;
			for (uInst3 = 0; uInst3 < psDepState->uBlockInstructionCount; uInst3++)
			{
				if (ArrayGet(psDepState->psInstructions, uInst1) != NULL &&
					ArrayGet(psDepState->psInstructions, uInst2) != NULL &&
					ArrayGet(psDepState->apsInstructions, uInst3) != NULL &&
					GetBit(psDepState->aauDependencyGraph[uInst1], uInst2) &&
					GetBit(psDepState->aauDependencyGraph[uInst2], uInst3))
				{
					ASSERT(GetBit(psDepState->aauClosedDependencyGraph[uInst1], uInst3));
					bDepend = IMG_TRUE;
				}
			}
			if (!bDepend)
			{
				ASSERT(!GetBit(psDepState->aauClosedDependencyGraph[uInst1], uInst3));
			}
		}
	}
}
#endif /* UF_TESTBENCH */

static
IMG_VOID InsertInAvailableList(PDGRAPH_STATE	psDepState,
							   PINST			psInstToInsert)
/*********************************************************************************
 Function			: InsertInAvailableList

 Description		: Inserts an instruction in the list of instructions with no
					  dependencies.
 
 Parameters			: psDepState		- Current module state.
					  psInstToInsert	- Instruction to isnert.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUSC_LIST_ENTRY		psEntry;
	PINTERMEDIATE_STATE psState = psDepState->psState;

	/*
		Sort the list by instruction index.
	*/
	for (psEntry = psDepState->sAvailableList.psHead; psEntry != NULL; psEntry = psEntry->psNext)
	{
		PINST psInst = IMG_CONTAINING_RECORD(psEntry, PINST, sAvailableListEntry);

		ASSERT(psInst->uId != psInstToInsert->uId);

		if (psInst->uId > psInstToInsert->uId)
		{
			break;
		}
	}

	/*
		Insert the entry.
	*/
	if (psEntry == NULL)
	{
		AppendToList(&psDepState->sAvailableList, &psInstToInsert->sAvailableListEntry);
	}
	else
	{
		InsertInList(&psDepState->sAvailableList, &psInstToInsert->sAvailableListEntry, psEntry);
	}
}

IMG_INTERNAL
IMG_VOID RemoveDependency(PDGRAPH_STATE		psDepState,
						  IMG_UINT32		uFrom,
						  PINST				psTo)
/*********************************************************************************
 Function			: RemoveDependency

 Description		: Removes a dependency between two instructions.
 
 Parameters			: psDepState	- Current module state.
					  uFrom, psTo	- Dependency to remove.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PADJACENCY_LIST		psList;
	PINTERMEDIATE_STATE	psState = psDepState->psState;
	IMG_UINT32			uTo = psTo->uId;

	/*
		Check if the two instructions are dependent.
	*/
	if (GraphGet(psState, psDepState->psDepGraph, uTo, uFrom))
	{
		IMG_UINT32 uSatDepCount;

		/*
			Update the graph.
		*/
		GraphSet(psState, psDepState->psDepGraph, uTo, uFrom, IMG_FALSE);

		/*
			Remove TO from FROM's list.
		*/
		psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uFrom);
		RemoveFromAdjacencyList(psState, psList, uTo);

		/*
			Decrease the count of instructions needed by TO.
		*/
		ArrayElemOp(psState, psDepState->psDepCount, USC_VEC_SUB, uTo, 1);

		if ((PINST)ArrayGet(psState, psDepState->psInstructions, uFrom) == NULL)
		{
			ArrayElemOp(psState, psDepState->psSatDepCount, USC_VEC_SUB, uTo, 1);
		}
		
		uSatDepCount = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psDepState->psSatDepCount, uTo);

		/*
			If we decreased TO's count of zero then TO can be scheduled immediately.	
		*/
		if ((IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psDepState->psDepCount, uTo) == uSatDepCount)
		{
			psDepState->uAvailInstCount++;

			InsertInAvailableList(psDepState, psTo);
		}
	}
}
 
IMG_INTERNAL
IMG_VOID SplitInstructions(PDGRAPH_STATE	psDepState,
						   IMG_UINT32		auNewInst[],
						   IMG_UINT32		uOldInst)
/*********************************************************************************
 Function			: SplitInstructions

 Description		: Removes a dependency between two instructions.
 
 Parameters			: psDepState	- Current module state.
					  auNewInst		- Array of two new instructions.
					  uOldInst		- Instruction to be removed.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32			uDepInst;
	IMG_UINT32			uInstIdx;
	PINTERMEDIATE_STATE	psState = psDepState->psState;
	IMG_UINT32			uDepCount;
	PADJACENCY_LIST		psList;
	PINST				psOldInst = (PINST)ArrayGet(psState, psDepState->psInstructions, uOldInst);
	
	uDepCount = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, 
									 psDepState->psDepCount, 
									 uOldInst);

	for (uInstIdx = 0; uInstIdx < 2; uInstIdx++)
	{
		IMG_UINT32		uNewInst = auNewInst[uInstIdx];

		/*
			Copy the dependency count from the old instruction.
		*/
		ArraySet(psDepState->psState, 
				 psDepState->psDepCount, 
				 uNewInst, 
				 (IMG_PVOID)(IMG_UINTPTR_T)uDepCount);
		ArraySet(psDepState->psState, 
				 psDepState->psSatDepCount, 
				 uNewInst, 
				 0);

		if (uDepCount == 0)
		{
			PINST	psNewInst = (PINST)ArrayGet(psState, psDepState->psInstructions, uNewInst);

			psDepState->uAvailInstCount++;

			AppendToList(&psDepState->sAvailableList, &psNewInst->sAvailableListEntry);
		}

		/*
			Copy the dependencies from the old instruction.
		*/
		GraphDupCol(psDepState->psState, 
					psDepState->psDepGraph,
					uOldInst, 
					uNewInst);
		GraphDupCol(psDepState->psState, 
					psDepState->psClosedDepGraph,
					uOldInst, 
					uNewInst);
	}

	/*
		Copy the dependency list from the old instruction. Reuse the list for one
		new instruction; make a copy of the list for the other.
	*/
	psList = ArrayGet(psState, psDepState->psDepList, uOldInst);
	ArraySet(psState, psDepState->psDepList, auNewInst[0], psList);
	ArraySet(psState, psDepState->psDepList, auNewInst[1], CloneAdjacencyList(psState, psList));
	ArraySet(psState, psDepState->psDepList, uOldInst, NULL);

	/*
		Replace dependencies on the old instruction by dependencies on the new instructions.
	*/
	for (uDepInst = 0; uDepInst < psDepState->uBlockInstructionCount; uDepInst++)
	{
		/*
			If OLDINST was dependent on DEPINST then replace the entries in
			DEPINST's dependency list.
		*/
		if (GraphGet(psState, psDepState->psDepGraph, uOldInst, uDepInst))
		{
			PADJACENCY_LIST	psListGraph;

			psListGraph = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uDepInst);
			ReplaceInAdjacencyList(psState, psListGraph, uOldInst, auNewInst[0]);
			AddToAdjacencyList(psState, psListGraph, auNewInst[1]);
		}

		/*
			If DEPINST was dependent on OLDINST then make DEPINST
			dependent on both of the new instructions.
		*/
		if (GraphGet(psState, psDepState->psDepGraph, uDepInst, uOldInst))
		{
			GraphSet(psDepState->psState, psDepState->psDepGraph,
					 uDepInst, uOldInst, IMG_FALSE);

			GraphSet(psDepState->psState, psDepState->psDepGraph,
					 uDepInst, auNewInst[0], IMG_TRUE);
			GraphSet(psDepState->psState, psDepState->psDepGraph,
					 uDepInst, auNewInst[1], IMG_TRUE);

			ArrayElemOp(psDepState->psState, psDepState->psDepCount, 
						USC_VEC_ADD, uDepInst, 1);
		}
		else
		{
			GraphSet(psDepState->psState, psDepState->psDepGraph,
					 uDepInst, auNewInst[0], IMG_FALSE);
			GraphSet(psDepState->psState, psDepState->psDepGraph,
					 uDepInst, auNewInst[1], IMG_FALSE);
		}
		if (GraphGet(psState, psDepState->psClosedDepGraph, uDepInst, uOldInst))
		{
			GraphSet(psDepState->psState, psDepState->psClosedDepGraph,
					 uDepInst, uOldInst, IMG_FALSE);
			GraphSet(psDepState->psState, psDepState->psClosedDepGraph,
					 uDepInst, auNewInst[0], IMG_TRUE);
			GraphSet(psDepState->psState, psDepState->psClosedDepGraph,
					 uDepInst, auNewInst[1], IMG_TRUE);
		}
		else
		{
			GraphSet(psDepState->psState, psDepState->psClosedDepGraph,
					 uDepInst, auNewInst[0], IMG_FALSE);
			GraphSet(psDepState->psState, psDepState->psClosedDepGraph,
					 uDepInst, auNewInst[1], IMG_FALSE);
		}
	}

	/*
		Clear the state associated with the old instruction in the dependency graph.
	*/
	ArraySet(psDepState->psState, psDepState->psInstructions, uOldInst, NULL);

	GraphClearCol(psDepState->psState, psDepState->psDepGraph, uOldInst);
	GraphClearCol(psDepState->psState, psDepState->psClosedDepGraph, uOldInst);

	if ((IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psDepState->psState, psDepState->psDepCount, uOldInst) == 0U)
	{
		psDepState->uAvailInstCount--;

		RemoveFromList(&psDepState->sAvailableList, &psOldInst->sAvailableListEntry);
		
		psOldInst->sAvailableListEntry.psNext = psOldInst->sAvailableListEntry.psPrev = NULL;
	}
	ArraySet(psDepState->psState, psDepState->psDepCount, uOldInst, (IMG_PVOID)0);

	psDepState->uRemovedInstCount++;
}

/*********************************************************************************
 Function			: MergeInstructions

 Description		: Merge the dependency information for two instructions.
 
 Parameters			: psDepState - Current module state.
					  uInst - Instruction which has been output.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
IMG_INTERNAL 
IMG_VOID MergeInstructions(PDGRAPH_STATE psDepState, 
						   IMG_UINT32 uDestInst, 
						   IMG_UINT32 uSrcInst, 
						   IMG_BOOL bRemoveSrc)
{
	PINTERMEDIATE_STATE		psState = psDepState->psState;
	IMG_UINT32				uInst;
	USC_PVECTOR				psSrcVec = NULL;
	PADJACENCY_LIST			psSrcAdjacentList;
	ADJACENCY_LIST_ITERATOR	sIter;

	/*
		Combine the transistive dependencies for the two instructions.
	*/
	GraphColRef(psState, psDepState->psClosedDepGraph, uSrcInst, &psSrcVec);
	VectorSet(psState, psSrcVec, uDestInst, 0); /* Don't include any dependency SRC -> DEST */
	GraphOrCol(psState, psDepState->psClosedDepGraph, uDestInst, psSrcVec);

	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
	{
		if (GraphGet(psState, psDepState->psClosedDepGraph, uInst, uDestInst))
		{
			if (!GraphGet(psState, psDepState->psClosedDepGraph, uInst, uSrcInst))
			{
				/*
					If uInst is dependent on uDestInst but not uSrcInst then add the
					indirect dependencies of uSrcInst to those of uInst.
				*/
				psSrcVec = NULL;
				GraphColRef(psState, psDepState->psClosedDepGraph, uSrcInst, &psSrcVec);
				GraphOrCol(psState, psDepState->psClosedDepGraph, uInst, psSrcVec);
			}
		}
		else
		{
			if (GraphGet(psState, psDepState->psClosedDepGraph, uInst, uSrcInst))
			{
				/*
					If uInst is dependent on uSrcInst but not uDestInst then add the
					indirect dependencies of uDestInst to those of uInst.
				*/
				psSrcVec = NULL;
				GraphColRef(psState, psDepState->psClosedDepGraph, uDestInst, &psSrcVec);
				GraphOrCol(psState, psDepState->psClosedDepGraph, uInst, psSrcVec);

				/*
					Add an indirect dependencies on uDestInst.
				*/
				GraphSet(psState, psDepState->psClosedDepGraph, uInst, uDestInst, IMG_TRUE);
				GraphSet(psState, psDepState->psClosedDepGraph, uInst, uSrcInst, IMG_FALSE);	
			}
		}

		/*
			If the source instruction is dependent on this instruction then
			make the destination instruction dependent on this instruction.
		*/
		if (GraphGet(psState, psDepState->psDepGraph, uSrcInst, uInst))
		{
			PADJACENCY_LIST	psList;

			psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uInst);
	
			if (uInst != uDestInst && !GraphGet(psState, psDepState->psDepGraph, uDestInst, uInst))
			{
				/*
					Add a dependency from uDestInst to uInst.
				*/
				AddToDepGraph(psDepState, uInst, uDestInst);
	
				/*
					Replace uSrcInst in uInst's dependency by uDestInst.
				*/
				ReplaceInAdjacencyList(psState, psList, uSrcInst, uDestInst);
			}
			else
			{
				/*
					Remove uSrcInst from uInst's dependency list.
				*/
				RemoveFromAdjacencyList(psState, psList, uSrcInst);
			}
		}
	}

	/*
		For all the instructions dependent on the source instruction make
		them dependent on the destination instruction.
	*/
	psSrcAdjacentList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uSrcInst);
	for (uInst = FirstAdjacent(psSrcAdjacentList, &sIter); !IsLastAdjacent(&sIter); uInst = NextAdjacent(&sIter))
	{
		if (!GraphGet(psState, psDepState->psDepGraph, uInst, uDestInst))
		{
			GraphSet(psState, psDepState->psDepGraph, uInst, uDestInst, IMG_TRUE);
			AddToDepList(psDepState, uDestInst, uInst);
		}
		else
		{
			ArrayElemOp(psState, psDepState->psDepCount, USC_VEC_SUB, uInst, 1);
		}
		GraphSet(psState, psDepState->psDepGraph, uInst, uSrcInst, IMG_FALSE);
	}

	if (bRemoveSrc)
	{
		IMG_UINT32 uDepCount, uSatDepCount;

		/*
			Clear the columns in the dependency graph.
		*/
		GraphClearCol(psState, psDepState->psDepGraph, uSrcInst);
		GraphClearCol(psState, psDepState->psClosedDepGraph, uSrcInst);

		psDepState->uRemovedInstCount++;
		
		/*
			If uSrcInst had zero dependencies then remove it from the list
			of available instructions.
		*/
		uDepCount = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psDepState->psDepCount, uSrcInst);
		uSatDepCount = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psDepState->psSatDepCount, uSrcInst);

		if (uSatDepCount == uDepCount)
		{
			PINST	psSrcInst = (PINST)ArrayGet(psState, psDepState->psInstructions, uSrcInst);

			psDepState->uAvailInstCount--;
			RemoveFromList(&psDepState->sAvailableList, &psSrcInst->sAvailableListEntry);
		
			psSrcInst->sAvailableListEntry.psNext = psSrcInst->sAvailableListEntry.psPrev = NULL;
		}
		/*
			Clear the corresponding entry in the instruction array.
		*/
		ArraySet(psState, psDepState->psInstructions, uSrcInst, NULL);
	}
}

IMG_INTERNAL 
IMG_VOID RemoveInstruction(PDGRAPH_STATE psDepState, PINST psInst)
/*********************************************************************************
 Function			: RemoveInstruction

 Description		: Flags that an instruction has been output.
 
 Parameters			: psDepState - Current module state.
					  uInst - Instruction which has been output.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PINTERMEDIATE_STATE		psState = psDepState->psState;
	PADJACENCY_LIST			psList;
	ADJACENCY_LIST_ITERATOR	sIter;
	IMG_UINT32				uOtherInst;
	IMG_UINT32				uInst = psInst->uId;

	psDepState->uAvailInstCount--;
	RemoveFromList(&psDepState->sAvailableList, &psInst->sAvailableListEntry);
	psInst->sAvailableListEntry.psNext = psInst->sAvailableListEntry.psPrev = NULL;

	psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uInst);
	for (uOtherInst = FirstAdjacent(psList, &sIter); !IsLastAdjacent(&sIter); uOtherInst = NextAdjacent(&sIter))
	{
		IMG_UINT32 uDepCount, uSatDepCount;

		uDepCount = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psDepState->psDepCount, uOtherInst);
		uSatDepCount = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psDepState->psSatDepCount, uOtherInst);

		ASSERT(uSatDepCount < uDepCount);
		ArrayElemOp(psState, psDepState->psSatDepCount, USC_VEC_ADD, uOtherInst, 1);

		if ((uSatDepCount + 1) == uDepCount)
		{
			PINST psOtherInst;

			psOtherInst = (PINST)ArrayGet(psState, psDepState->psInstructions, uOtherInst);

			psDepState->uAvailInstCount++;

			InsertInAvailableList(psDepState, psOtherInst);
		}
	}

	ArraySet(psState, psDepState->psInstructions, uInst, NULL);
	psDepState->uRemovedInstCount++;

	ASSERT((psDepState->uAvailInstCount > 0) || 
		   (psDepState->uRemovedInstCount == psDepState->uBlockInstructionCount));
}

#ifdef UF_TESTBENCH
IMG_INTERNAL
IMG_VOID PrintDependencyGraph(PDGRAPH_STATE psDepState, IMG_BOOL bClosed)
/******************************************************************************
 FUNCTION	: PrintDepGraph

 PURPOSE	: Debugging routine, prints the current dependency graph

 PARAMETERS	: psDepState	- Dependency graph state (contains both graphs)
			  bClosed		- IMG_TRUE to print psDepState->psClosedDepGraph
							  (the transitive closure - if it's been computed);
							  IMG_FALSE for the unclosed graph (->psDepGraph)

 NOTES		: Prints both the adjacency matrix, and also a DOT representation
			  (www.graphviz.org) thereof, to psState->pfnPrint
******************************************************************************/
{
	PINTERMEDIATE_STATE psState = psDepState->psState;
	USC_PGRAPH psGraph = (bClosed) ? psDepState->psClosedDepGraph : psDepState->psDepGraph;
	IMG_UINT32 i,j;
	IMG_PCHAR apcTempString = UscAlloc(psState, max(
			/*one row of matrix = a digit + a space, per column (i.e. inst)*/
			psDepState->uBlockInstructionCount,
			128/*sufficient to represent an instruction (for DOT output)*/));

	for (i = 0; i < psDepState->uBlockInstructionCount; i++)
	{
		apcTempString[0] = '\0';
		for (j = 0; j < psDepState->uBlockInstructionCount; j++)
		{
			sprintf(apcTempString + strlen(apcTempString),
					"%c ", (GraphGet(psState, psGraph, i, j)) ? '1' : '0');
		}
		DBG_PRINTF((DBG_MESSAGE, "%s", apcTempString));
	}
	DBG_PRINTF((DBG_MESSAGE, "digraph BLOCK {"));
	for (i = 0; i < psDepState->uBlockInstructionCount; i++)
	{
		PrintIntermediateInst(psState,
							  ArrayGet(psState, psDepState->psInstructions, i),
							  NULL, apcTempString);
		DBG_PRINTF((DBG_MESSAGE, "n%u [label=\"(%u) %s\"];", i, i, apcTempString));
		for (j = 0; j < psDepState->uBlockInstructionCount; j++)
		{
			if (GraphGet(psState, psGraph, i, j))
			{
				DBG_PRINTF((DBG_MESSAGE, "n%u -> n%u;", j, i));
			}
		}
	}
	DBG_PRINTF((DBG_MESSAGE, "}"));
}

IMG_INTERNAL
IMG_VOID DotPrintDepGraph(PDGRAPH_STATE psDepState, IMG_PCHAR pchFileName, IMG_BOOL bClosed)
{
	static IMG_UINT32 uFunctionCallCount = 0;
	IMG_CHAR acFileName[256];
	const PINTERMEDIATE_STATE psState = psDepState->psState;
	USC_PGRAPH psGraph = (bClosed) ? psDepState->psClosedDepGraph : psDepState->psDepGraph;
	IMG_UINT32 uInst;
	IMG_CHAR apcTempString[512];
	FILE *fOut;
	
	/* Making sure files won't be overwritten */
	sprintf(acFileName, "%s-%04u", pchFileName, uFunctionCallCount++);
	fOut = fopen(acFileName, "w");
	
	fprintf(fOut, "digraph DEP {\n");
	/* instructions */
	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
	{
		PINST psInst = ArrayGet(psState, psDepState->psInstructions, uInst);
		if(psInst)
		{
			sprintf(apcTempString, "n%u [label=\"%u", uInst, uInst);
			PrintIntermediateInst(psState, psInst, NULL, apcTempString+strlen(apcTempString));
			sprintf(apcTempString+strlen(apcTempString), "\"];");
			fprintf(fOut, "%s\n", apcTempString);
		}
	}
	/* nodes! */
	for(uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
	{
		IMG_UINT32 uOtherInst;
		for (uOtherInst = 0; uOtherInst < psDepState->uBlockInstructionCount; uOtherInst++)
		{
			if (!GraphGet(psState, psGraph, uInst, uOtherInst)) continue;
			sprintf(apcTempString, "n%u -> n%u;", uInst, uOtherInst);
			fprintf(fOut, "%s\n", apcTempString);
		}
	}
	fprintf(fOut, "};\n");
	fclose(fOut);
}
#endif

IMG_INTERNAL 
IMG_VOID ComputeClosedDependencyGraph(PDGRAPH_STATE psDepState, IMG_BOOL bUnorderedDeps)
/*********************************************************************************
 Function			: ComputeClosedDependencyGraph

 Description		: Calculate the transitive closure of the dependency graph.
 
 Parameters			: psState - Current module state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PINTERMEDIATE_STATE psState = psDepState->psState;

	IMG_UINT32	uInst, uDepInst;
	USC_PVECTOR	psSrcVec;

	ClearGraph(psState, psDepState->psClosedDepGraph);

	/* Start with the non-transitive dependencies. */
	GraphCopy(psState, psDepState->psDepGraph, psDepState->psClosedDepGraph);

	if (bUnorderedDeps)
	{
		IMG_UINT32	uInst1, uInst2, uInst3;

		for (uInst1 = 0; uInst1 < psDepState->uBlockInstructionCount; uInst1++)
		{
			for (uInst2 = 0; uInst2 < psDepState->uBlockInstructionCount; uInst2++)
			{
				for (uInst3 = 0; uInst3 < psDepState->uBlockInstructionCount; uInst3++)
				{
					if (!GraphGet(psState, psDepState->psClosedDepGraph, uInst2, uInst3))
					{
						if (GraphGet(psState, psDepState->psClosedDepGraph, uInst2, uInst1) &&
							GraphGet(psState, psDepState->psClosedDepGraph, uInst1, uInst3))
						{
							GraphSet(psState, psDepState->psClosedDepGraph, uInst2, uInst3, IMG_TRUE);
						}
					}
				}
			}
		}
	}
	else
	{
		for (uInst = 0; uInst < psDepState->uBlockInstructionCount; uInst++)
		{
			PADJACENCY_LIST			psList;
			ADJACENCY_LIST_ITERATOR	sIterState;

			psSrcVec = NULL;
			GraphColRef(psState, psDepState->psClosedDepGraph, uInst, &psSrcVec);

			psList = (PADJACENCY_LIST)ArrayGet(psState, psDepState->psDepList, uInst);
			for (uDepInst = FirstAdjacent(psList, &sIterState); 
				!IsLastAdjacent(&sIterState); 
				uDepInst = NextAdjacent(&sIterState))
			{
				GraphOrCol(psState, psDepState->psClosedDepGraph, uDepInst, psSrcVec);
			}
		}
	}
}

IMG_INTERNAL 
IMG_VOID UpdateClosedDependencyGraph(PDGRAPH_STATE psDepState, IMG_UINT32 uTo, IMG_UINT32 uFrom)
/*****************************************************************************
 FUNCTION	: UpdateClosedDependencyGraph
    
 PURPOSE	: Update the closed dependency graph when a new dependency is added.

 PARAMETERS	: psDepState		- Module state.
			  uTo				- Dependency destination.
			  uFrom				- Dependency source.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINTERMEDIATE_STATE psState = psDepState->psState;

	IMG_UINT32	uDepInst;
	USC_PVECTOR psSrcVec = NULL;

	/*
		Any dependencies of FROM are now dependencies of TO together.
	*/
	GraphColRef(psState, psDepState->psClosedDepGraph, uFrom, &psSrcVec);
	GraphOrCol(psState, psDepState->psClosedDepGraph, uTo, psSrcVec);

	/*
		Any instruction DEP which depends on FROM now depends on TO as well.
	*/
	for (uDepInst = 0; uDepInst < psDepState->uBlockInstructionCount; uDepInst++)
	{
		if (GraphGet(psState, psDepState->psClosedDepGraph, uDepInst, uTo))
		{
			psSrcVec = NULL;
			GraphSet(psState, psDepState->psClosedDepGraph, 
					 uDepInst, uFrom, IMG_TRUE);
			GraphColRef(psState, psDepState->psClosedDepGraph, uFrom, &psSrcVec);
			GraphOrCol(psState, psDepState->psClosedDepGraph, uDepInst, psSrcVec);
		}
	}
}

static IMG_VOID RecordWrite(PDGRAPH_STATE psDepState,
							IMG_UINT32 uInst,
							USC_PARRAY psUserArray,
							IMG_UINT32 uReg)
/******************************************************************************
 FUNCTION	: RecordWrite

 PURPOSE	: Records a write to a register whose usage info is stored (as an
			  INT_LIST) in an element of a USC_PARRAY (i.e. a <psRUsers> in an
			  PCOMPUTE_DGRAPH_STATE). 

 PARAMETERS	: psDepState	- Dependency Graph state
			  uInst			- Instruction performing the write
			  psUserArray	- Array storing usage info for all regs of type
			  uReg			- Number of register (of that type) written

 NOTE		: Includes adding dependencies to ensure the write stays after any
			  previous reads of the same register.
******************************************************************************/
{
	const PINTERMEDIATE_STATE psState = psDepState->psState;
	PINT_LIST psUsers = (PINT_LIST)ArrayGet(psState, psUserArray, uReg);
	if (psUsers)
	{
		FindReaders(psDepState, psUsers, uInst, IMG_TRUE /* bClearReaders */);
	}
	else
	{
		//no previous reads or writes in block
		psUsers = UscAlloc(psState, sizeof(INT_LIST));
		psUsers->psNext = NULL;
		ArraySet(psState, psUserArray, uReg, psUsers);
	}
	psUsers->uInst = uInst;
}

static IMG_VOID AddDestDependencies(PDGRAPH_STATE psDepState, 
									IMG_UINT32 uInst, 
									PARG psDest, 
									IMG_BOOL bIgnoreDesched,
									PINST psInst)
/*********************************************************************************
 FUNCTION		: AddDestDependencies

 DESCRIPTION	: Add dependencies from previous readers/writers of a
				  destination register.
 
 PARAMETERS		: psDepState	- Current module state.
				  uInst			- Current instruction index
				  psDest		- Destination register.
				  bIgnoreDesched
								- If FALSE, IReg writes gain dependencies from
								  descheduling points

 RETURNS		: Nothing.
*********************************************************************************/
{
	PINTERMEDIATE_STATE psState = psDepState->psState;
	USC_PARRAY psUserArray;
	
	//most cases fall through to code beneath, except for last two
	if (psDest->uType == USEASM_REGTYPE_TEMP)
	{
		psUserArray = psDepState->psTempUsers;
	}
	else if (psDest->uType == USEASM_REGTYPE_OUTPUT)
	{
		psUserArray = psDepState->psOutputUsers;
	}
	else if (psDest->uType == USEASM_REGTYPE_PRIMATTR)
	{
		psUserArray = psDepState->psPAUsers;
	}
	else if (psDest->uType == USEASM_REGTYPE_FPINTERNAL)
	{
		psUserArray = psDepState->psIRegUsers;
	}
	else if (psDest->uType == USEASM_REGTYPE_INDEX)
	{
		psUserArray = psDepState->psIndexUsers;
	}
	else if (psDest->uType == USEASM_REGTYPE_PREDICATE)
	{
		psUserArray = psDepState->psPredUsers;
	}
	else if (psDest->uType == USC_REGTYPE_REGARRAY)
	{
		IMG_UINT32	uArrayNum = psDest->uNumber;

		ASSERT(uArrayNum < psState->uNumVecArrayRegs);
		ASSERT(psDepState->apsRegArrayLastWriter != NULL);

		FindRegArrayReaders(psDepState, 
							uInst, 
							psDest->uIndexType, 
							uArrayNum,
							psDest->uArrayOffset);

		if (psDest->uIndexType == USC_REGTYPE_NOINDEX)
		{
			ArraySet(psState,
					 psDepState->apsRegArrayLastWriter[uArrayNum],
					 psDest->uArrayOffset,
					 (IMG_PVOID)(IMG_UINTPTR_T)uInst);
		}
		else
		{
			IMG_UINT32	uIdx;

			for (uIdx = 0; uIdx < psState->apsVecArrayReg[uArrayNum]->uRegs; uIdx++)
			{
				ArraySet(psState,
						 psDepState->apsRegArrayLastWriter[uArrayNum],
						 uIdx,
						 (IMG_PVOID)(IMG_UINTPTR_T)uInst);
			}
		}
		return;
	}
	else 
	{
		return;
	}
	
	RecordWrite(psDepState, uInst, psUserArray, psDest->uNumber);

	if (psDest->uType == USEASM_REGTYPE_FPINTERNAL && !bIgnoreDesched)
	{
		/*
			Add dependencies on previous descheduling points.
		*/

		PINST		psPrevInst;
		IMG_UINT32	uPrevInst;

		for (psPrevInst = psInst->psPrev, uPrevInst = uInst - 1; 
			 psPrevInst != NULL; 
			 psPrevInst = psPrevInst->psPrev, uPrevInst--)
		{
			if (IsDeschedulingPoint(psState, psPrevInst))
			{
				AddDependency(psDepState, uPrevInst, uInst);
			}
		}
	}
}

static IMG_VOID RecordRead(PDGRAPH_STATE	psDepState,
						   IMG_UINT32		uInst,
						   USC_PARRAY		psUserArray,
						   IMG_UINT32		uReg)
/******************************************************************************
 FUNCTION		: RecordRead

 DESCRIPTION	: Records a read of a register whose usage info is stored (as
				  an INT_LIST) in an element of a USC_PARRAY.

 PARAMETERS		: psDepState	- Dependency Graph state
				  uInst			- Index of reading instruction
				  psUserArray	- array of usage info for all regs of type
				  uReg			- number of reg (of that type)

 RETURNS		: Nothing

 NOTE			: Includes adding a dependency from any previous writer
******************************************************************************/
{
	const PINTERMEDIATE_STATE psState = psDepState->psState;
	PINT_LIST	psUsers		= (PINT_LIST)ArrayGet(psState, psUserArray, uReg),
				psThisRead	= UscAlloc(psState, sizeof(INT_LIST));
	psThisRead->uInst = uInst;

	if (psUsers)
	{
		//first element is the write, if any (might have been in earlier block)
		if (psUsers->uInst != UINT_MAX)
		{
			AddDependency(psDepState, psUsers->uInst, uInst);
		}
	}
	else
	{
		psUsers = UscAlloc(psState, sizeof(INT_LIST));
		psUsers->uInst = UINT_MAX;
		psUsers->psNext = NULL;
		ArraySet(psState, psUserArray, uReg, psUsers);
	}
	//insert after write and before other readers
	psThisRead->psNext = psUsers->psNext;
	psUsers->psNext = psThisRead;
}

static IMG_VOID RecordReadsByArg(PDGRAPH_STATE	psDepState,
								 IMG_UINT32		uInst,
								 IMG_UINT32		uSourceType,
								 IMG_UINT32		uSourceNum,
								 IMG_UINT32		uSourceArrayOffset,
								 IMG_UINT32		uSourceIndexType)
/******************************************************************************
 FUNCTION		: RecordReadsByArg

 DESCRIPTION	: Adds dependencies from the previous writer of an argument reg
 
 PARAMETERS		: psDepState	- Depnedency Graph State
				  uInst			- Index of reading instruction
				  psArg			- Argument being read
******************************************************************************/
{
	PINTERMEDIATE_STATE psState = psDepState->psState;
	USC_PARRAY psUserArray;
	
	//note all cases except last (arrays) fall through to ArrayGet at end
	if (uSourceType == USEASM_REGTYPE_TEMP)
	{
		psUserArray = psDepState->psTempUsers;
	}
	else if (uSourceType == USEASM_REGTYPE_OUTPUT)
	{
		psUserArray = psDepState->psOutputUsers;
	}
	else if (uSourceType == USEASM_REGTYPE_PRIMATTR)
	{
		psUserArray = psDepState->psPAUsers;
	}
	else if (uSourceType == USEASM_REGTYPE_FPINTERNAL)
	{
		psUserArray = psDepState->psIRegUsers;
	}
	else if (uSourceType == USEASM_REGTYPE_INDEX)
	{
		psUserArray = psDepState->psIndexUsers;
	}
	else if (uSourceType == USEASM_REGTYPE_PREDICATE)
	{
		psUserArray = psDepState->psPredUsers;
	}
	else if (uSourceType == USC_REGTYPE_REGARRAY)
	{
		IMG_UINT32	uArrayNum = uSourceNum;
		USC_PARRAY	psWriterArray =  psDepState->apsRegArrayLastWriter[uArrayNum];

		ASSERT(uArrayNum < psState->uNumVecArrayRegs);

		if (uSourceIndexType == USC_REGTYPE_NOINDEX)
		{
			IMG_UINT32 uLastWriter = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, 
														  psWriterArray, 
														  uSourceArrayOffset);
			if (uLastWriter != (USC_UNDEF))
			{
				AddDependency(psDepState, uLastWriter, uInst);
			}
		}
		else
		{
			IMG_UINT32 uIdx;
			for (uIdx = 0; uIdx < psState->apsVecArrayReg[uArrayNum]->uRegs; uIdx++)
			{
				IMG_UINT32 uLastWriter = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, 
															  psWriterArray, 
															  uIdx);
				if (uLastWriter != UINT_MAX)
				{
					AddDependency(psDepState, uLastWriter, uInst);
				}
			}
		}
		return;
	}
	else return; //some other regtype?? - imgabort()?
	
	RecordRead(psDepState, uInst, psUserArray, uSourceNum);
}

static
IMG_VOID ComputeLoadStoreDependence (PDGRAPH_STATE psDepState)
/*********************************************************************************
 Function			: ComputeLoadStoreDependence

 Description		: Calculate the dependency graph between loads and stores in a block.
 
 Parameters			: psDepState	- Dependency graph state

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	typedef struct __STORE_DATA__ 
	{
		IMG_UINT32 uInst;
		PINST	 psStoreInst;
		IMG_BOOL bAddressRegChanged;
		USC_LIST_ENTRY sListEntry;
	} STORE_DATA, *PSTORE_DATA;

	IMG_UINT32 uInst;
	USC_LIST asStores;
	PUSC_LIST_ENTRY psEntry;
	PINTERMEDIATE_STATE psState = psDepState->psState;

	InitializeList (&asStores);

	/**
	 * Consider all stores using absolute addressing as a dependency of
	 * all absolute addressed loads/stores in this block unless it is known that:
	 * 1) The base addresses are different and the static offsets the same.
	 * 2) The base addresses are the same and the static offsets different.
	 * 3) The memory address spaces are different.
	 */
	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; ++uInst)
	{
		PINST psInst = ArrayGet(psState, psDepState->psInstructions, uInst);

		if (TestInstructionGroup (psInst->eOpcode, USC_OPT_GROUP_ABS_MEMLDST))
		{
			IMG_UINT32 uFetch;
			IMG_UINT8 uFlags;
			IMG_UINT32 uDataSize;
			IMG_UINT32 uImplicitOffset = 0;

			// Gather common data.
			if (psInst->eOpcode == ILOADMEMCONST)
			{
				uFetch = psInst->u.psLoadMemConst->uFetchCount;
				uFlags = psInst->u.psLoadMemConst->uFlags;
				uDataSize = psInst->u.psLoadMemConst->uDataSize;
			}
			else
			{
				uFetch = psInst->uRepeat;
				uFlags = psInst->u.psLdSt->uFlags;

				switch (psInst->eOpcode)
				{
				case ILDAD:
				case ISTAD:
					{
						uDataSize = 4;
						break;
					}
				case ILDAW:
				case ISTAW:
					{
						uDataSize = 2;
						break;
					}
				case ILDAB:
				case ISTAB:
					{
						uDataSize = 1;
						break;
					}
				default:
					{
						imgabort ();
						break;
					}
				}

				if (uFetch)
				{
					uImplicitOffset = uDataSize;
				}
			}

			psEntry = asStores.psHead;
			while (psEntry != NULL)
			{
				PSTORE_DATA psStoreData = IMG_CONTAINING_RECORD (psEntry, PSTORE_DATA, sListEntry);

				/* 1) The base addresses point to different objects. */
				if (((uFlags & UF_MEM_NOALIAS) ||
					(psStoreData->psStoreInst->u.psLdSt->uFlags & UF_MEM_NOALIAS)) &&
					(psStoreData->psStoreInst->asArg[0].uType != psInst->asArg[0].uType ||
					psStoreData->psStoreInst->asArg[0].uNumber != psInst->asArg[0].uNumber))
				{
					psEntry = psEntry->psNext;
					continue;
				}

				/* 2) The base addresses are the same and the static offsets different. */
				if (!psStoreData->bAddressRegChanged &&
					psInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE &&
					psStoreData->psStoreInst->asArg[1].uType == USEASM_REGTYPE_IMMEDIATE && 
					psStoreData->psStoreInst->asArg[0].uType == psInst->asArg[0].uType &&
					psStoreData->psStoreInst->asArg[0].uNumber == psInst->asArg[0].uNumber)
				{
					/* Calculate the static offset range */
					IMG_UINT32 uOffsetLowest;
					IMG_UINT32 uOffsetHighest;
					IMG_UINT32 uSOffsetLowest;
					IMG_UINT32 uSOffsetHighest;

					/* Get the instruction offset low-high range */
					uOffsetLowest = (0xFFFF & psInst->asArg[1].uNumber) + uImplicitOffset;
					uOffsetHighest = uOffsetLowest + (uFetch - 1) * uDataSize;

					/* Get the store instruction offset low-high range */
					uSOffsetLowest = (0xFFFF & psStoreData->psStoreInst->asArg[1].uNumber);
					uSOffsetLowest *= psStoreData->psStoreInst->asArg[1].uNumber >> 16;
					uSOffsetHighest = uSOffsetLowest;

					if (1 < psStoreData->psStoreInst->uRepeat)
					{
						uSOffsetHighest += psStoreData->psStoreInst->uRepeat * uDataSize;
					}

					if (uSOffsetLowest < uOffsetLowest || uOffsetHighest < uSOffsetHighest)
					{
						psEntry = psEntry->psNext;
						continue;
					}
				}

				/* 3) The memory address spaces are different. */
				if ((uFlags & ~UF_MEM_NOALIAS) !=
					(psStoreData->psStoreInst->u.psLdSt->uFlags & ~UF_MEM_NOALIAS))
				{
					psEntry = psEntry->psNext;
					continue;
				}

				/* Assume dependent as we cannot prove otherwise */
				AddDependency(psDepState, psStoreData->uInst, uInst);
				psEntry = psEntry->psNext;
			}

			if (g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_MEMSTORE)
			{
				/* Track all mem stores. */
				PSTORE_DATA psStoreData = UscAlloc (psState, sizeof (STORE_DATA));
				psStoreData->uInst = uInst;
				psStoreData->psStoreInst = psInst;
				psStoreData->bAddressRegChanged = IMG_FALSE;
				AppendToList (&asStores, &psStoreData->sListEntry);
			}
		}
		else
		{
			// Track any changes to address registers.
			psEntry = asStores.psHead;
			while (psEntry != NULL)
			{
				PSTORE_DATA psStoreData = IMG_CONTAINING_RECORD (psEntry, PSTORE_DATA, sListEntry);

				IMG_UINT32 d;
				for (d = 0; d < psInst->uDestCount; ++d)
				{
					if (psStoreData->psStoreInst->asArg[0].uType == psInst->asDest[d].uType &&
						psStoreData->psStoreInst->asArg[0].uNumber == psInst->asDest[d].uNumber)
					{
						psStoreData->bAddressRegChanged = IMG_TRUE;
						break;
					}
				}

				psEntry = psEntry->psNext;
			}
		}
	}

	psEntry = asStores.psHead;
	while (psEntry != NULL)
	{
		PUSC_LIST_ENTRY psNext = psEntry->psNext;
		PSTORE_DATA psStoreData = IMG_CONTAINING_RECORD (psEntry, PSTORE_DATA, sListEntry);
		UscFree (psState, psStoreData);
		psEntry = psNext;
	}
}

static
IMG_VOID ComputeMutexLockDependence(PDGRAPH_STATE psDepState)
/*********************************************************************************
 Function			: ComputeMutexLockDependence

 Description		: Inserts a dependence of every instruction inside a mutex lock/release.

 Parameters			: psDepState	- Dependency graph state

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32 uLockInst = 0;
	IMG_BOOL   bLocked = IMG_FALSE;
	IMG_UINT32 uInst = 0;

	for (uInst = 0; uInst < psDepState->uBlockInstructionCount; ++uInst)
	{
		PINST psInst = ArrayGet(psDepState->psState, psDepState->psInstructions, uInst);

		/* Check that there are no nested locks*/
		if (bLocked && (psInst->eOpcode == ILOCK))
		{
			UscAbort(psDepState->psState, UF_ERR_INVALID_OPCODE,
			         "Nested mutex locks not allowed", IMG_NULL, 0);
		}

		/* Check that we are not releasing a lock when there is none */
		if (!bLocked && (psInst->eOpcode == IRELEASE))
		{
			UscAbort(psDepState->psState, UF_ERR_INVALID_OPCODE,
			         "Cannot release an unacquired lock", IMG_NULL, 0);
		}

		/* If we are not in a lock state and this isn't a lock instruction, continue */
		if (!bLocked && (psInst->eOpcode != ILOCK))
			continue;

		/* If we are not in a lock state and this is a lock instruction, set locked */
		if (!bLocked && (psInst->eOpcode == ILOCK))
		{
			bLocked = IMG_TRUE;
			uLockInst = uInst;
			continue;
		}

		/* Set the current instruction as dependent on the LOCK instruction
		 to stop them from being moved before the lock*/
		AddDependency(psDepState, uLockInst, uInst);

		/* If we are in a lock state and this is a release, end the lock state */
		if (bLocked && (psInst->eOpcode == IRELEASE))
		{
			bLocked = IMG_FALSE;
			/* To prevent instruction from being moved outside of the lock, set the release
			 instruction as dependent on every instruction in the lock section */
			for (++uLockInst; uLockInst < (uInst-1); ++uLockInst)
				AddDependency(psDepState, uLockInst, uInst);

			/* To prevent instructions from being moved into the lock section,
			 set the next instruction as dependent on release */
			if (uInst < (psDepState->uBlockInstructionCount-1))
			{
				AddDependency(psDepState, uInst, uInst+1);
			}
		}
	}

	if (bLocked)
	{
		UscAbort(psDepState->psState, UF_ERR_INVALID_OPCODE,
		         "Block ended with unreleased lock", IMG_NULL, 0);
	}
}



IMG_INTERNAL 
PDGRAPH_STATE ComputeDependencyGraph(PINTERMEDIATE_STATE psState,
									 const CODEBLOCK *psBlock,
									 IMG_BOOL bIgnoreDesched)
/*********************************************************************************
 Function			: ComputeDependencyGraph

 Description		: Calculate a dependency graph for a block.
 
 Parameters			: psDepState	- Dependency graph state
					  psBlock		- Block to calculate the graph for.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PDGRAPH_STATE	psDepState;
	PINST			psInst;
	IMG_UINT32		uInst, uInstCount;
	IMG_UINT32		uLastLocalLoadStore;
	IMG_UINT32		uIdx;
	
	psDepState = NewDGraphState(psState);
	InitializeList(&psDepState->sAvailableList);

	/*
		Make an array with the instructions from the block.
	*/
	for (psInst = psBlock->psBody, uInstCount = 0;
		 psInst != NULL; 
		 psInst = psInst->psNext, uInstCount++)
	{
		ArraySet(psState, psDepState->psInstructions, uInstCount, psInst);
		psInst->uId = uInstCount;

		AppendToList(&psDepState->sAvailableList, &psInst->sAvailableListEntry);
	}
	psDepState->uBlockInstructionCount = uInstCount;

	psDepState->uAvailInstCount = psDepState->uBlockInstructionCount;
	psDepState->uRemovedInstCount = 0;
	
	ClearArray(psState, psDepState->psTempUsers, (USC_STATE_FREEFN)FreeIntList);
	ClearArray(psState, psDepState->psOutputUsers, (USC_STATE_FREEFN)FreeIntList);
	ClearArray(psState, psDepState->psPAUsers, (USC_STATE_FREEFN)FreeIntList);
	ClearArray(psState, psDepState->psPredUsers, (USC_STATE_FREEFN)FreeIntList);
	ClearArray(psState, psDepState->psIRegUsers, (USC_STATE_FREEFN)FreeIntList);
	ClearArray(psState, psDepState->psIndexUsers, (USC_STATE_FREEFN)FreeIntList);
	if (psDepState->apsRegArrayLastWriter != NULL)
	{
		for (uIdx = 0; uIdx < psState->uNumVecArrayRegs; uIdx++)
		{
			ClearArray(psState, psDepState->apsRegArrayLastWriter[uIdx], NULL);
		}
	}

	ClearGraph(psState, psDepState->psDepGraph);

	ClearArray(psState, psDepState->psDepCount, NULL);
	ClearArray(psState, psDepState->psSatDepCount, NULL);

	ClearArray(psState, psDepState->psDepList, DeleteAdjacencyListIndirect);

	uLastLocalLoadStore = UINT_MAX;

	for (uInst = 0; uInst < uInstCount; uInst++)
	{
		IMG_UINT32 uArg;
		IMG_UINT32 uDestIdx;
		IMG_UINT32 uPred;

		psInst = ArrayGet(psState, psDepState->psInstructions, uInst);
	
		ArraySet(psState, psDepState->psMainDep, uInst, (IMG_PVOID)(IMG_UINTPTR_T)USC_MAINDEPENDENCY_NONE);

		/*
			Add dependencies on the last writer of each instruction source.
		*/
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			PARG psArg = &psInst->asArg[uArg];

			RecordReadsByArg(psDepState, uInst, psArg->uType, psArg->uNumber, psArg->uArrayOffset, psArg->uIndexType);
			
			/*
				Add dependencies on the last writer of the index register.
			*/
			if (psArg->uIndexType != USC_REGTYPE_NOINDEX)
			{
				RecordReadsByArg(psDepState, 
								 uInst, 
								 psArg->uIndexType, 
								 psArg->uIndexNumber, 
								 USC_UNDEF /* uSourceArrayOffset */, 
								 USC_REGTYPE_NOINDEX);
			}
		}

		/*
			For a descheduling point add dependencies on instructions reading the
			internal registers.
		*/
		if (!bIgnoreDesched && IsDeschedulingPoint(psState, psInst))
		{
			IMG_UINT32	uIRegNum;

			for (uIRegNum = 0; uIRegNum < psState->uGPISizeInScalarRegs; uIRegNum++)
			{
				PINT_LIST psUsers = (PINT_LIST)ArrayGet(psState,
														psDepState->psIRegUsers,
														uIRegNum);
				if (psUsers)
				{
					//make dependent on previous writer also
					if (psUsers->uInst != UINT_MAX)
					{
						AddDependency(psDepState, psUsers->uInst, uInst);
					}
					FindReaders(psDepState,
								psUsers,
								uInst,
								IMG_FALSE/*do not clear previous readers*/);
				}
			}
		}

		/*
			Add a dependency on the writer of the predicate register used.
		*/
		for (uPred = 0; uPred < psInst->uPredCount; ++uPred)
		{
			if (psInst->apsPredSrc[uPred] != NULL)
			{
				ASSERT(psInst->apsPredSrc[uPred]->uType == USEASM_REGTYPE_PREDICATE);
				RecordRead(psDepState, uInst, psDepState->psPredUsers, psInst->apsPredSrc[uPred]->uNumber);
			}
		}

		/*
			Add dependencies on the previous writer of any partially/conditionally
			written destinations.
		*/
		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG	psOldDest = psInst->apsOldDest[uDestIdx];

			if (psOldDest != NULL)
			{
				RecordReadsByArg(psDepState, 
								 uInst, 
								 psOldDest->uType, 
								 psOldDest->uNumber, 
								 psOldDest->uArrayOffset, 
								 psOldDest->uIndexType);
			}
		}

		/*
			Add dependencies on the readers of the previous value of each destination
			register.
		*/
		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			PARG	psDest = &psInst->asDest[uDestIdx];

			AddDestDependencies(psDepState, 
								uInst, 
								psDest, 
								bIgnoreDesched, 
								psInst);
			
			/*
				Add a dependency on the last writer of any index register used in the
				destination.
			*/
			if (psDest->uIndexType != USC_REGTYPE_NOINDEX)
			{
				RecordReadsByArg(psDepState, 
								 uInst, 
								 psDest->uIndexType, 
								 psDest->uIndexNumber, 
								 USC_UNDEF /* uSourceArrayOffset */, 
								 USC_REGTYPE_NOINDEX);
			}
		}

		/*
			Stores to indexable temporary registers must always be ordered.
		*/
		if (TestInstructionGroup(psInst->eOpcode, USC_OPT_GROUP_LDST_IND_TMP_INST))
		{
			/*
				Filter following instructions
				ILDLD, ILDLW, ILDLB, ISTLD, ISTLW, ISTLB
			*/
			
			if (uLastLocalLoadStore != UINT_MAX)
			{
				AddDependency(psDepState, uLastLocalLoadStore, uInst);
			}
			uLastLocalLoadStore = uInst;
		}
	}

	/* Prevent moving of instructions outside of mutex locks */
	ComputeMutexLockDependence(psDepState);

	if (psState->uCompilerFlags & UF_OPENCL)
	{
		ComputeLoadStoreDependence (psDepState);
	}
	
	return psDepState;
}

static
PINST FindLastWriterInstOfArgument(PINTERMEDIATE_STATE psState, PINST psPoint, PARG psArgToFind)
/*********************************************************************************
 Function			: FindLastWriterInstOfArgument

 Description		: Finds last writer instruction for given argument
 
 Parameters			: psDepState	- Dependency graph state
					  psPoint		- Search start from.

  Return			: Instruction
*********************************************************************************/
{
	PINST			psFindDefInst;
	USEDEF_RECORD	sLocalUseDefRec;
	PUSEDEF_RECORD	psLocalUseDefRec = &sLocalUseDefRec;
	
	InitUseDef(psLocalUseDefRec);
	ClearUseDef(psState, psLocalUseDefRec);
	for(psFindDefInst = psPoint->psPrev; psFindDefInst; psFindDefInst = psFindDefInst->psPrev)
	{
		/* Calculate register def */
		InstDef(psState, psFindDefInst, &psLocalUseDefRec->sDef);
		
		if(GetRegUseDef(psState, &psLocalUseDefRec->sDef, psArgToFind->uType, psArgToFind->uNumber))
		{
			ClearUseDef(psState, psLocalUseDefRec);
			return psFindDefInst;
		}
	}
	ClearUseDef(psState, psLocalUseDefRec);
	return IMG_NULL;
}

//#define ICODE_DEP_DUMP
static
IMG_VOID AddRelationDAG(PINTERMEDIATE_STATE psState, PINST psFindDefInst, PINST psCurrent, PARG psArgToFind, IMG_UINT32 uFlags)
/*********************************************************************************
 Function			: AddRelationDAG

 Description		: Internal function used by GenerateInstructionDAG, to add
					  Relation between instrucitons.
 
 Parameters			: psDepState	- Dependency graph state
					  psFindDefInst	- Argment defining instruction.

  Return			: Nothing
*********************************************************************************/
{
	PORDINATE psNewSup, psNewSub, psFinder = IMG_NULL;
				
	if(!(uFlags & DEP_GRAPH_COMPUTE_DEP_PER_ARG))
	{
		/*
			Check if relation already exists
		*/
		psFinder = psCurrent->psImmediateSuperordinates;
		while(psFinder)
		{
			if(psFinder->psInstruction == psFindDefInst)
			{
				/*
					psFindDefInst is already marked as supernode of psCurrent
				*/
				break;
			}
			psFinder = psFinder->psNext;
		}
	}
	if(!psFinder)
	{
		psNewSup = UscAlloc(psState, sizeof(ORDINATE));
		psNewSup->psInstruction = psFindDefInst;
		psNewSup->psInstruction->uGraphFlags |= DEP_GRAPH_HAS_SUBERORDINATES;
		psNewSup->psNext = psCurrent->psImmediateSuperordinates;
		psCurrent->psImmediateSuperordinates = psNewSup;
		
		psNewSub = UscAlloc(psState, sizeof(ORDINATE));
		psNewSub->psInstruction = psCurrent;
		psNewSub->psNext = psNewSup->psInstruction->psImmediateSubordinates;
		psNewSup->psInstruction->psImmediateSubordinates = psNewSub;
		
		if(uFlags & DEP_GRAPH_COMPUTE_COLLECT_ARG_INFO)
		{
			psNewSup->pvData = (IMG_PVOID)psArgToFind;
			psNewSub->pvData = (IMG_PVOID)psArgToFind;
		}
		else
		{
			psNewSup->pvData = IMG_NULL;
			psNewSub->pvData = IMG_NULL;
		}
#if defined(UF_TESTBENCH)
		if(psArgToFind->uType < USC_REGTYPE_MAXIMUM)
		{
			sprintf(psNewSup->aszLable, "%s%d", g_pszRegType[psArgToFind->uType], psArgToFind->uNumber);
			sprintf(psNewSub->aszLable, "%s%d", g_pszRegType[psArgToFind->uType], psArgToFind->uNumber);
		}
		else
		{
			psNewSup->aszLable[0] = '\0';
			psNewSub->aszLable[0] = '\0';
		}
#endif /*defined(UF_TESTBENCH)*/
#if defined(ICODE_DEP_DUMP)
		if(psArgToFind->uType < USC_REGTYPE_MAXIMUM)
		{
			fprintf(stderr, "%d(%s%d),", psFindDefInst->uBlockIndex, 
						g_pszRegType[psArgToFind->uType], psArgToFind->uNumber);
		}
#endif
	}
}

IMG_INTERNAL
IMG_VOID GenerateInstructionDAG(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock, IMG_UINT32 uFlags)
/*********************************************************************************
 Function			: GenerateInstructionDAG

 Description		: Creates DAG information per instruction for a block
 
 Parameters			: psDepState	- Dependency graph state
					  psBlock		- Block to be freed DAG data

  Return			: Nothing
*********************************************************************************/
{
	IMG_UINT32			uArgInd;
	PINST				psCurrent, psEndInst, psFindDefInst;
	PARG				psArgToFind;
	
	USEDEF_RECORD	sLocalUseDefRec;
	PUSEDEF_RECORD	psLocalUseDefRec = &sLocalUseDefRec;
	
	InitUseDef(psLocalUseDefRec);
	
	psEndInst = psBlock->psBodyTail;
	
#if defined(ICODE_DEP_DUMP)
	fprintf(stderr, "\n");
#endif
	
	/* Scan for usages by past instructions */
	for(psCurrent = psEndInst; psCurrent != NULL; psCurrent = psCurrent->psPrev)
	{
#if defined(ICODE_DEP_DUMP)
		fprintf(stderr, "DEP(%02d) %02d %s -> ", psBlock->uGlobalIdx, 
							psCurrent->uBlockIndex, g_psInstDesc[psCurrent->eOpcode].pszName);
#endif
		if(psCurrent->apsOldDest && (uFlags & DEP_GRAPH_COMPUTE_COLLECT_OLD_DEST) != 0)
		{
			IMG_UINT32 uDstIdx;
			
			/* instructions writing to partially writen destination */
			for(uDstIdx = 0; uDstIdx < psCurrent->uDestCount; uDstIdx++)
			{
				if(psCurrent->apsOldDest[uDstIdx])
				{
					psArgToFind = psCurrent->apsOldDest[uDstIdx];
					psFindDefInst = FindLastWriterInstOfArgument(psState, psCurrent, psArgToFind);
					if(psFindDefInst)
					{
						AddRelationDAG(psState, psFindDefInst, psCurrent, psArgToFind, uFlags);
					}
				}
			}
		}
		
		/* See if any argument of psCurrent is defined/written by psFindDefInst */
		for(uArgInd = 0; uArgInd < psCurrent->uArgumentCount; uArgInd++)
		{
			psArgToFind = &psCurrent->asArg[uArgInd];
			psFindDefInst = FindLastWriterInstOfArgument(psState, psCurrent, psArgToFind);
			if(psFindDefInst)
			{
				AddRelationDAG(psState, psFindDefInst, psCurrent, psArgToFind, uFlags);
			}
		}
#if defined(ICODE_DEP_DUMP)
		fprintf(stderr, "\n");
#endif
	}
	ClearUseDef(psState, psLocalUseDefRec);
	
	TESTONLY(VerifyDAG(psState, psBlock));
}

#if defined(DEBUG)
IMG_INTERNAL
IMG_BOOL VerifyDAG(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/*********************************************************************************
 Function			: VerifyDAG

 Description		: Debug check to verify consistancy.
 
 Parameters			: psDepState	- Dependency graph state
					  psBlock		- Block to check

  Return			: Nothing
*********************************************************************************/
{
	PINST psInst;
	PORDINATE psNext;
	IMG_BOOL bVerifyFailed = IMG_FALSE, bQuit = IMG_FALSE;
	
	for(psInst = psBlock->psBody; psInst != IMG_NULL && !bQuit; psInst = psInst->psNext)
	{
		psNext = psInst->psImmediateSuperordinates;
		while(psNext)
		{
			if(!(psNext->psInstruction->uBlockIndex < psInst->uBlockIndex))
			{
				DBG_PRINTF((DBG_ERROR, "DAG Verification failed for instruction %d Superordinate found %d", 
							psInst->uBlockIndex, psNext->psInstruction->uBlockIndex));
				
				bVerifyFailed = IMG_TRUE;
				bQuit = IMG_TRUE;
				break;
			}
			psNext = psNext->psNext;
		}
		psNext = psInst->psImmediateSubordinates;
		while(psNext)
		{
			if(!(psNext->psInstruction->uBlockIndex > psInst->uBlockIndex))
			{
				DBG_PRINTF((DBG_ERROR, "DAG Verification failed for instruction %d Subordinate found %d", 
							psInst->uBlockIndex, psNext->psInstruction->uBlockIndex));
				
				bVerifyFailed = IMG_TRUE;
				bQuit = IMG_TRUE;
				break;
			}
			psNext = psNext->psNext;
		}
	}
	
	if(bVerifyFailed)
	{
		TESTONLY_PRINT_INTERMEDIATE("DAG Verification failed", "verify_dag_failed", IMG_TRUE);
		imgabort();
	}
	
	return IMG_TRUE;
}
#endif

IMG_INTERNAL
IMG_VOID FreeInstructionDAG(PINTERMEDIATE_STATE psState, PCODEBLOCK psBlock)
/*********************************************************************************
 Function			: FreeInstructionDAG

 Description		: Frees DAG data for a instruction block
 
 Parameters			: psDepState	- Dependency graph state
					  psBlock		- Block to be freed DAG data

  Return			: Nothing
*********************************************************************************/
{
	PINST psInst;
	PORDINATE psTemp, psNext;
	
	for(psInst = psBlock->psBody; psInst != IMG_NULL; psInst = psInst->psNext)
	{
		psNext = psInst->psImmediateSuperordinates;
		while(psNext)
		{
			psTemp = psNext;
			psNext = psNext->psNext;
			UscFree(psState, psTemp);
		}
		psNext = psInst->psImmediateSubordinates;
		while(psNext)
		{
			psTemp = psNext;
			psNext = psNext->psNext;
			UscFree(psState, psTemp);
		}
		psInst->uGraphFlags					= 0;
		psInst->psImmediateSuperordinates	= IMG_NULL;
		psInst->psImmediateSubordinates		= IMG_NULL;
		
		/* Cached results for pattern search */
		psInst->psLastFailedRule			= IMG_NULL;
		psInst->psLastPassedRule			= IMG_NULL;
	}
}

IMG_INTERNAL
IMG_BOOL SearchInstPatternInDAG(const PINTERMEDIATE_STATE psState, const PINST psInst, const PATTERN_RULE * psRule, const IMG_UINT32 uMaxDepth, PINST *ppsMatchInst)
/*********************************************************************************
 Function			: SearchInstPatternInDAG

 Description		: Looks for instruction pattern in Instruction DAG. Pattern may
					  contain expression like AND | OR | * etc.
 
 Parameters			: psDepState	- Dependency graph state
					  psInst		- Seach starting instruction. Mostly terminal 
									  node.
					  uMaxDepth		- To control recursion level
					  ppsMatchInst	- Return last instruction match if op was CONCAT

  Return			: True if pattern found
*********************************************************************************/
{
	PORDINATE psSup;
	
	if(!uMaxDepth)
	{
		return IMG_FALSE;
	}
	
	/*
		If we already have applied same rule before for same instruction, we can do quick exit.
		This should considerably reduce recursion. As instructions or *rule never* changed, this is ok.
	*/
	if((PPATTERN_RULE)psInst->psLastFailedRule == psRule)
	{
		return IMG_FALSE;
	}
	if((PPATTERN_RULE)psInst->psLastPassedRule == psRule)
	{
		/* 
			Also save last matched instruction in case this call was generated from CONCAT op
		*/
		*ppsMatchInst = psInst->psLastMatchInst;
		return IMG_TRUE;
	}

	switch(psRule->eType)
	{
		case NODE_INSTRUCTION:
		{
			if(psRule->eOpcode == psInst->eOpcode)
			{
				*ppsMatchInst = psInst;
				goto rule_passed;
			}
			else
				goto rule_failed;
		}
		case NODE_OR:
		{
			if(SearchInstPatternInDAG(psState, psInst, psRule->psRightNode, uMaxDepth, ppsMatchInst) == IMG_TRUE)
			{
				goto rule_passed;
			}
			if(SearchInstPatternInDAG(psState, psInst, psRule->psLeftNode, uMaxDepth, ppsMatchInst) == IMG_TRUE)
			{
				goto rule_passed;
			}
			goto rule_failed;
		}
		case NODE_AND:
		{
			if(SearchInstPatternInDAG(psState, psInst, psRule->psRightNode, uMaxDepth, ppsMatchInst) == IMG_FALSE)
			{
				goto rule_failed;
			}
			if(SearchInstPatternInDAG(psState, psInst, psRule->psLeftNode, uMaxDepth, ppsMatchInst) == IMG_FALSE)
			{
				goto rule_failed;
			}
			goto rule_passed;
		}
		case NODE_CONCAT:
		{
			PINST psLeftPath;
			if(SearchInstPatternInDAG(psState, psInst, psRule->psLeftNode, uMaxDepth, &psLeftPath) == IMG_FALSE)
			{
				goto rule_failed;
			}
			
			/* Unlikely case SearchInstPatternInDAG should be returning false */
			ASSERT(psLeftPath);
			
			psSup = psLeftPath->psImmediateSuperordinates;
			while(psSup)
			{
				if(SearchInstPatternInDAG(psState, psSup->psInstruction, psRule->psRightNode, (uMaxDepth-1), ppsMatchInst) == IMG_TRUE)
				{
					goto rule_passed;
				}
				psSup = psSup->psNext;
			}
			goto rule_failed;
		}
		case NODE_ANYTHING:
		{
			if(SearchInstPatternInDAG(psState, psInst, psRule->psRightNode, uMaxDepth, ppsMatchInst) == IMG_TRUE)
				goto rule_passed;
			
			psSup = psInst->psImmediateSuperordinates;
			while(psSup)
			{
				if(SearchInstPatternInDAG(psState, psSup->psInstruction, psRule, (uMaxDepth - 1), ppsMatchInst) == IMG_TRUE)
				{
					goto rule_passed;
				}
				psSup = psSup->psNext;
			}
			goto rule_failed;
		}
		case NODE_START:
		{
			if(!psInst->psImmediateSuperordinates)
			{
				if(SearchInstPatternInDAG(psState, psInst, psRule->psRightNode, uMaxDepth, ppsMatchInst) == IMG_TRUE)
					goto rule_passed;
			}
			else
			{
				psSup = psInst->psImmediateSuperordinates;
				while(psSup)
				{
					if(SearchInstPatternInDAG(psState, psSup->psInstruction, psRule, (uMaxDepth - 1), ppsMatchInst) == IMG_TRUE)
					{
						goto rule_passed;
					}
					psSup = psSup->psNext;
				}
			}
		}
		default:
			goto rule_failed;
	}
	
rule_failed:
	*ppsMatchInst = IMG_NULL;
	psInst->psLastFailedRule = psRule;
	return IMG_FALSE;
	
rule_passed:
	psInst->psLastMatchInst = *ppsMatchInst;
	psInst->psLastPassedRule = psRule;
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL IsImmediateSubordinate(PINST psInst, PINST psPossibleSubordinate)
/*********************************************************************************
 Function			: IsImmediateSubordinate

 Description		: Check whether one instruction directly depends on another.
 
 Parameters			: psInst				- The main instruction to check.
					  psPossibleSubordinate	- The instruction to check for dependency on the first.

  Return			: Returns TRUE or FALSE;
*********************************************************************************/
{
	PORDINATE psSubordinate;
	for (psSubordinate = psInst->psImmediateSubordinates; psSubordinate != NULL; psSubordinate = psSubordinate->psNext)
	{
		if (psSubordinate->psInstruction == psPossibleSubordinate)
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID GetLivenessSpanForArgumentDAG(PINTERMEDIATE_STATE	psState,
									   PINST				psPoint, 
									   IMG_UINT32			uArgIdx,
									   PINST				*ppsStartInst,
									   PINST				*ppsOldDestStart,
									   PINST				*ppsEndInst)
/*********************************************************************************
 Function			: GetLivenessSpanForArgumentDAG

 Description		: Computes first (defining) and last instruction (using) for an argument
 
 Parameters			: psState			- Compiler state
					  psInst			- Center point instruction.
					  uArgIdx			- Index of argument to search for
					  ppsStartInst		- Return pointer of defining same argument
					  ppsEndInst		- Return pointer of last use of same argument

  Return			: Returns nothing - No changes are made to instruction
*********************************************************************************/
{
	PINST		psStartInst;
	IMG_UINT32	uIndex;
	PARG		psArgToFind;
	PORDINATE	psRunner;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	psStartInst = *ppsStartInst = IMG_NULL;
	*ppsOldDestStart = IMG_NULL;
	
	psArgToFind = &psPoint->asArg[uArgIdx];
	
	/* Find the defining instruction for argument */
	psRunner = psPoint->psImmediateSuperordinates;
	while(psRunner)
	{
		if(CompareArgs((PARG)psRunner->pvData, psArgToFind) == 0)
		{
			psStartInst = *ppsStartInst = psRunner->psInstruction;
			break;
		}
		psRunner = psRunner->psNext;
	}
	
	if(!(*ppsStartInst))
	{
		*ppsEndInst = IMG_NULL;
		return;
	}
	*ppsEndInst = psPoint;
	
	/* Find the instruction for last use of argument */
	psRunner = psStartInst->psImmediateSubordinates;
	while(psRunner)
	{
		if((*ppsEndInst)->uBlockIndex < psRunner->psInstruction->uBlockIndex)
		{
			*ppsEndInst = psRunner->psInstruction;
		}
		psRunner = psRunner->psNext;
	}
	
	if(psStartInst->apsOldDest)
	{
		for(uIndex = 0; uIndex < psStartInst->uDestCount; uIndex++)
		{
			if(CompareArgs(&psStartInst->asDest[uIndex], psArgToFind) == 0 && psStartInst->apsOldDest[uIndex])
			{
				if(psStartInst->apsOldDest[uIndex]->uType == USEASM_REGTYPE_TEMP)
				{
					ASSERT(psStartInst->apsOldDest[uIndex]->psRegister);
					ASSERT(psStartInst->apsOldDest[uIndex]->psRegister->psUseDefChain);
					*ppsOldDestStart = UseDefGetDefInstFromChain(psStartInst->apsOldDest[uIndex]->psRegister->psUseDefChain, IMG_NULL);
				}
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID GetLivenessSpanForDestDAG(	PINTERMEDIATE_STATE	psState,
									PINST				psPoint, 
									IMG_UINT32			uDestIdx,
									PINST				*ppsOldDestStart,
									PINST				*ppsEndInst)
/*********************************************************************************
 Function			: GetLivenessSpanForDestDAG

 Description		: Computes first (defining) and last instruction (using) for an argument
 
 Parameters			: psState			- Compiler state
					  psInst			- Center point instruction.
					  uDestIdx			- Index of destination to search for
					  ppsOldDestStart	- Return pointer of partially written 
										  defining same destination if exists any.
					  ppsEndInst		- Return pointer of last use of same argument

  Return			: Returns nothing
*********************************************************************************/
{
	ARG			sArgToReplace;
	PORDINATE	psDestUseOrd;
	
	PVR_UNREFERENCED_PARAMETER(psState);
	
	psDestUseOrd = psPoint->psImmediateSubordinates;
	*ppsEndInst = IMG_NULL;
	if (ppsOldDestStart != NULL)
	{
		*ppsOldDestStart = IMG_NULL;
	}
	sArgToReplace = psPoint->asDest[uDestIdx];
	
	while(psDestUseOrd)
	{
		PINST psUseInst;
		IMG_UINT32 uArgIdx;
		
		psUseInst = psDestUseOrd->psInstruction;
			
		for(uArgIdx = 0; uArgIdx < psUseInst->uArgumentCount; uArgIdx++)
		{
			if(CompareArgs(&psUseInst->asArg[uArgIdx], &sArgToReplace) == 0)
			{
				*ppsEndInst = psUseInst;
			}
		}
		for (uArgIdx = 0; uArgIdx < psUseInst->uDestCount; uArgIdx++)
		{
			PARG	psOldDest = psUseInst->apsOldDest[uArgIdx];

			if (psOldDest != NULL && CompareArgs(psOldDest, &sArgToReplace) == 0)
			{
				GetLivenessSpanForDestDAG(psState, psUseInst, uArgIdx, NULL /* ppsOldDestStart */, ppsEndInst);
			}
		}
		psDestUseOrd = psDestUseOrd->psNext;
	}
	
	if(psPoint->apsOldDest && ppsOldDestStart != NULL)
	{
		if(psPoint->apsOldDest[uDestIdx] && psPoint->apsOldDest[uDestIdx]->uType == USEASM_REGTYPE_TEMP)
		{
			ASSERT(psPoint->apsOldDest[uDestIdx]->psRegister);
			ASSERT(psPoint->apsOldDest[uDestIdx]->psRegister->psUseDefChain);
			*ppsOldDestStart = UseDefGetDefInstFromChain(psPoint->apsOldDest[uDestIdx]->psRegister->psUseDefChain, 
															IMG_NULL);
		}
	}
}

static
IMG_BOOL ApplyConstraintDestinationReplacementTopDAG(	PINTERMEDIATE_STATE psState,
														PINST				psPoint,
														IMG_UINT32			uDestIdx,
														PFN_ARG_CONSTRAINT	pfnSourceConstraint,
														PFN_ARG_CONSTRAINT	pfnDestinationConstraint,
														PFN_ARG_REPLACE_SRC pfnUpdateSrcReplacement,
														PFN_ARG_REPLACE_DEST pfnUpdateDstReplacement,
														PFN_INST_UPDATE		pfnInstUpdate,
														IMG_BOOL			bContraintCheckOnly,
														const ARG			*pcsSubstitue,
														IMG_PVOID			*pvData)
/*********************************************************************************
 Function			: ApplyConstraintArgumentReplacementTopDAG

 Description		: Performes source argument replacement for an instruction, along
 					  with reflecting substitutions over defining and using instructions.
 					  
 					  *one or more instructions can be affected by substitution*
 					  But data flow links are always maintainted.
 					  
 					  Recursively performes replacement for partial written destinations.
 
 Parameters			: psState				- Compiler state
					  psPoint				- Instruction point to start substitution for
					  uDestIdx				- Index of argument to be replaced
					  pfnSourceConstraint	- Contraint checking function for arguments
					  pfnDestinationConstraint - Contraint checking function for destination
					  pfnUpdateSrcReplacement - Update function for src replacement
					  pfnUpdateDstReplacement - Update function for dst replacement
					  pfnInstUpdate			- Update funtion for changing instructions
					  bContraintCheckOnly	- Flag to indicate should replacement actually
											  Perform or not
					  pcsSubstitue			- Argument to substitute with

  Return			: True if any of substitution is done
*********************************************************************************/
{
	IMG_BOOL	bResult;
	IMG_UINT32	uArgIdx, uDstIdx;
	PORDINATE	psArgUseOrd;
	ARG			sArgToReplace;
	PINST		psEndInst = IMG_NULL, psInterInst;
	PINST		psStartInst = psPoint, psDestDefInst = psPoint;
	
	sArgToReplace = psPoint->asDest[uDestIdx];
	
	/* Constraint check */
	if(bContraintCheckOnly && pfnDestinationConstraint && 
		  !pfnDestinationConstraint(psState, psDestDefInst, uDestIdx, IMG_TRUE, IMG_NULL, IMG_NULL, pvData))
	{
		return IMG_FALSE;
	}
	
	if(pfnSourceConstraint)
	{
		PINST	psOldDestUseInst = NULL;

		psArgUseOrd = psDestDefInst->psImmediateSubordinates;
		while(psArgUseOrd)
		{
			PINST psUseInst;
			
			psUseInst = psArgUseOrd->psInstruction;
				
			for(uArgIdx = 0; uArgIdx < psUseInst->uArgumentCount; uArgIdx++)
			{
				if(CompareArgs(&psUseInst->asArg[uArgIdx], &sArgToReplace) == 0)
				{
					if(!pfnSourceConstraint(psState, psArgUseOrd->psInstruction, uArgIdx, IMG_TRUE, psPoint, psEndInst, pvData))
					{
						return IMG_FALSE;
					}
				}
			}
			for(uDstIdx = 0; uDstIdx < psUseInst->uDestCount && psUseInst->apsOldDest; uDstIdx++)
			{
				if(psUseInst->apsOldDest[uDstIdx]
					&& (CompareArgs(psUseInst->apsOldDest[uDstIdx], &sArgToReplace) == 0))
				{
					/*
						Check there is only one use of the argument in a partially overwritten
						destination.
					*/
					if (psOldDestUseInst != NULL)
					{
						return IMG_FALSE;
					}
					psOldDestUseInst = psUseInst;

					if(!ApplyConstraintDestinationReplacementTopDAG(	psState, psUseInst, uDstIdx, 
																		pfnSourceConstraint, pfnDestinationConstraint, 
																		IMG_NULL, IMG_NULL, IMG_NULL,
																		IMG_TRUE, pcsSubstitue, pvData))
					{
						return IMG_FALSE;
					}
				}
			}
			if(!psEndInst || psEndInst->uBlockIndex < psUseInst->uBlockIndex) 
			{
				psEndInst = psUseInst;
			}
			psArgUseOrd = psArgUseOrd->psNext;
		}

		/*
			Check any use of the argument in a partially overwritten destination is after all
			the uses as a source.
		*/
		if (psOldDestUseInst != NULL && psEndInst != psOldDestUseInst)
		{
			return IMG_FALSE;
		}
		
		for(psInterInst = psEndInst; psInterInst != psStartInst && psInterInst; psInterInst = psInterInst->psPrev)
		{
			if(!pfnSourceConstraint(psState, psInterInst, UINT_MAX, IMG_FALSE, psDestDefInst, psEndInst, pvData))
			{
				return IMG_FALSE;
			}
		}
	}
	
	if(bContraintCheckOnly)
	{
		return IMG_TRUE;
	}
	
	ASSERT(pfnUpdateSrcReplacement);
	ASSERT(pfnUpdateDstReplacement);
	ASSERT(pfnInstUpdate);
	
	/* Commit the changes */
	psArgUseOrd = psDestDefInst->psImmediateSubordinates;
	while(psArgUseOrd)
	{
		PINST		psUseInst;
		IMG_BOOL	bInstUpdated = IMG_FALSE;
		
		psUseInst = psArgUseOrd->psInstruction;
			
		for(uArgIdx = 0; uArgIdx < psUseInst->uArgumentCount; uArgIdx++)
		{
			if(psUseInst->asArg[uArgIdx].uType == USEASM_REGTYPE_UNDEF || psUseInst->asArg[uArgIdx].uType == USC_REGTYPE_UNUSEDSOURCE)
			{
				continue;
			}
			if(CompareArgs(&psUseInst->asArg[uArgIdx], &sArgToReplace) == 0)
			{
				pfnUpdateSrcReplacement(psState, psUseInst, uArgIdx, pcsSubstitue);
				bInstUpdated = IMG_TRUE;
			}
		}
		for(uDstIdx = 0; uDstIdx < psUseInst->uDestCount && psUseInst->apsOldDest; uDstIdx++)
		{
			if(psUseInst->apsOldDest[uDstIdx]
				&& (CompareArgs(psUseInst->apsOldDest[uDstIdx], &sArgToReplace) == 0))
			{
				bResult = ApplyConstraintDestinationReplacementTopDAG(	psState, psUseInst, uDstIdx, 
																	pfnSourceConstraint, pfnDestinationConstraint, 
																	pfnUpdateSrcReplacement, pfnUpdateDstReplacement, pfnInstUpdate,
																	IMG_FALSE, pcsSubstitue, pvData);
				SetPartiallyWrittenDest(psState, psUseInst, uDstIdx, &psUseInst->asDest[uDstIdx]);
			}
		}
		if(bInstUpdated)
		{
			pfnInstUpdate(psState, psUseInst);
		}
		psArgUseOrd = psArgUseOrd->psNext;
	}
	
	pfnUpdateDstReplacement(psState, psDestDefInst, uDestIdx, pcsSubstitue);
	if(psDestDefInst->apsOldDest && psDestDefInst->apsOldDest[uDestIdx])
	{
		SetPartiallyWrittenDest(psState, psDestDefInst, uDestIdx, &psDestDefInst->asDest[uDestIdx]);
	}
	pfnInstUpdate(psState, psDestDefInst);
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL ApplyConstraintDestinationReplacementDAG(	PINTERMEDIATE_STATE psState,
													PINST				psPoint,
													IMG_UINT32			uDestIdx,
													PFN_ARG_CONSTRAINT	pfnSourceConstraint,
													PFN_ARG_CONSTRAINT	pfnDestinationConstraint,
													PFN_ARG_REPLACE_SRC pfnUpdateSrcReplacement,
													PFN_ARG_REPLACE_DEST pfnUpdateDstReplacement,
													PFN_INST_UPDATE		pfnInstUpdate,
													IMG_BOOL			bContraintCheckOnly,
													const ARG			*pcsSubstitue,
													IMG_PVOID			*pvData)
/*********************************************************************************
 Function			: ApplyConstraintArgumentReplacementDAG

 Description		: Performes source argument replacement for an instruction, along
 					  with reflecting substitutions over defining and using instructions.
 					  
 					  *one or more instructions can be affected by substitution*
 					  But data flow links are always maintainted.
 
 Parameters			: psState				- Compiler state
					  psPoint				- Instruction point to start substitution for
					  uDestIdx				- Index of argument to be replaced
					  pfnSourceConstraint	- Contraint checking function for arguments
					  pfnDestinationConstraint - Contraint checking function for destination
					  pfnUpdateSrcReplacement - Update function for src replacement
					  pfnUpdateDstReplacement - Update function for dst replacement
					  pfnInstUpdate			- Update funtion for changing instructions
					  bContraintCheckOnly	- Flag to indicate should replacement actually
											  Perform or not
					  pcsSubstitue			- Argument to substitute with

  Return			: True if any of substitution is done
*********************************************************************************/
{
	PINST		psDestDefInst = psPoint;
	PINST		psOldDestDefInst = IMG_NULL;
	IMG_UINT32	uDestLocation;
	
	/* Constraint check */
	if(bContraintCheckOnly && pfnDestinationConstraint && 
		  !pfnDestinationConstraint(psState, psDestDefInst, uDestIdx, IMG_TRUE, IMG_NULL, IMG_NULL, pvData))
	{
		return IMG_FALSE;
	}
	
	if(psDestDefInst->apsOldDest && psDestDefInst->apsOldDest[uDestIdx])
	{
		/* Navigate to top definiation first */
		psOldDestDefInst = UseDefGetDefInstFromChain(psDestDefInst->apsOldDest[uDestIdx]->psRegister->psUseDefChain, &uDestLocation);
		ASSERT(psOldDestDefInst);
		if(bContraintCheckOnly)
		{
			if(!ApplyConstraintDestinationReplacementDAG(psState, psOldDestDefInst, uDestLocation, 
														 pfnSourceConstraint, pfnDestinationConstraint, 
														 IMG_NULL, IMG_NULL, IMG_NULL,
														 IMG_TRUE, IMG_NULL, pvData))
			{
				return IMG_FALSE;
			}
		}
		else
		{
			IMG_BOOL bResult;
			
			bResult = ApplyConstraintDestinationReplacementDAG(	psState, psOldDestDefInst, uDestLocation, 
																pfnSourceConstraint, pfnDestinationConstraint, 
																pfnUpdateSrcReplacement, pfnUpdateDstReplacement, pfnInstUpdate,
																IMG_FALSE, pcsSubstitue, pvData);
			ASSERT(bResult);
		}
		return IMG_TRUE;
	}
	return ApplyConstraintDestinationReplacementTopDAG(	psState,
														psPoint,
														uDestIdx,
														pfnSourceConstraint,
														pfnDestinationConstraint,
														pfnUpdateSrcReplacement,
														pfnUpdateDstReplacement,
														pfnInstUpdate,
														bContraintCheckOnly,
														pcsSubstitue,
														pvData);
}
	

IMG_INTERNAL
IMG_BOOL ApplyConstraintArgumentReplacementDAG(	PINTERMEDIATE_STATE psState,
												PINST				psPoint,
												IMG_UINT32			uArgIdx,
												PFN_ARG_CONSTRAINT	pfnSourceConstraint,
												PFN_ARG_CONSTRAINT	pfnDestinationConstraint,
												PFN_ARG_REPLACE_SRC pfnUpdateSrcReplacement,
												PFN_ARG_REPLACE_DEST pfnUpdateDstReplacement,
												PFN_INST_UPDATE		pfnInstUpdate,
												IMG_BOOL			bContraintCheckOnly,
												const ARG			*pcsSubstitue,
												IMG_PVOID			*pvData)
/*********************************************************************************
 Function			: ApplyConstraintArgumentReplacementDAG

 Description		: Performes source argument replacement for an instruction, along
 					  with reflecting substitutions over defining and using instructions.
 					  
 					  *one or more instructions can be affected by substitution*
 					  But data flow links are always maintainted.
 
 Parameters			: psState			- Compiler state
					  psPoint			- Instruction point to start substitution for
					  uArgIdx			- Index of argument to be replaced
					  pfnSourceConstraint
										- Constraint for arguments check, (*all the 
					  					  substitutions must pass constaint check*)
					  pfnDestinationConstraint
					  					- Constraint for destinations check, (*all the 
					  					  substitutions must pass constaint check*)
					  pfnUpdateSrcReplacement - Update function for src replacement
					  pfnUpdateDstReplacement - Update function for dst replacement
					  pfnInstUpdate			- Update funtion for changing instructions
					  bContraintCheckOnly
					  					- No changes are made to instructions.
					  pcsSubstitue		- Argument to substitute with

  Return			: True if any of substitution is done
*********************************************************************************/
{
	ARG			sArgToReplace;
	PINST		psStartInst, psEndInst, psOldDestStartInst;

	sArgToReplace = psPoint->asArg[uArgIdx];

	GetLivenessSpanForArgumentDAG(psState, psPoint, uArgIdx, &psStartInst, &psOldDestStartInst, &psEndInst);
	if(!psStartInst || !psEndInst)
	{
		/* Argument is not defined in same block */
		return IMG_FALSE;
	}
	
	for(uArgIdx = 0; uArgIdx < psStartInst->uDestCount; uArgIdx++)
	{
		if(CompareArgs(&psStartInst->asDest[uArgIdx], &sArgToReplace) == 0)
		{
			if(!ApplyConstraintDestinationReplacementDAG(psState, psStartInst, uArgIdx, 
													 pfnSourceConstraint, pfnDestinationConstraint, 
													 pfnUpdateSrcReplacement, pfnUpdateDstReplacement,
													 pfnInstUpdate,
													 bContraintCheckOnly,
													 pcsSubstitue, pvData))
			{
				return IMG_FALSE;
			}
		}
	}
	return IMG_TRUE;
}

/******************************************************************************
 End of file (dgraph.c)
******************************************************************************/
