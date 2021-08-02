/*****************************************************************************
 Name           : ARRAY.H

 Title          : Interface between psc.c and pscopt.c

 C Author       : John Russell

 Created        : 02/01/2002

 Copyright      : 2002-2007 by Imagination Technologies Ltd.
                  All rights reserved. No part of this software, either
                  material or conceptual may be copied or distributed,
                  transmitted, transcribed, stored in a retrieval system or
                  translated into any human or computer language in any form
                  by any means, electronic, mechanical, manual or otherwise,
                  or disclosed to third parties without the express written
                  permission of Imagination Technologies Ltd,
                  Home Park Estate, Kings Langley, Hertfordshire,
                  WD4 8LZ, U.K.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.38 $

 Modifications  :

 $Log: data.h $
*****************************************************************************/

#ifndef __USC_DATA_H
#define __USC_DATA_H

#include <limits.h>

/********************
 * Generic data types
 ********************/

/***
 * Set up the definitions for the generic data types
 ***/

#ifdef PGDATA_SYS_STATE
#undef PGDATA_SYS_STATE
#endif
#ifdef GDATA_ALLOC
#undef GDATA_ALLOC
#endif
#ifdef GDATA_FREE
#undef GDATA_FREE
#endif

#ifdef GEN_TYNAME
#undef GEN_TYNAME
#endif

#ifdef GEN_NAME
#undef GEN_NAME
#endif

/* Memory allocation functions */

#define PGDATA_SYS_STATE USC_DATA_STATE_PTR
#define GDATA_ALLOC UscAlloc
#define GDATA_FREE UscFree

/* Naming macros */
#define GEN_TYNAME(NAME) USC_##NAME
#define GEN_NAME(NAME) Usc##NAME

/***
 * Data Type Declarations
 ***/
#include "uscgendata.h"

/***
 * Aliases
 ***/

typedef GEN_TYNAME(PAIR_PTR) PUSC_PAIR;
typedef GEN_TYNAME(STACK_PTR) PUSC_STACK;
typedef GEN_TYNAME(TREE_PTR) PUSC_TREE;

/***
 * Finsh
 ***/

/********************
 * END Generic data types
 ********************/

#include "bitops.h"

/*
  Memoisation data
 */
typedef struct _USC_MEMO_
{
	IMG_PVOID pvData;
} USC_MEMO, *USC_PMEMO;
#define USC_MEMO_DEFAULT { NULL }

/* USC_MIN_ARRAY_CHUNK: Minimum size in unsigned integers of a chunk used for an array */
#define USC_MIN_ARRAY_CHUNK 48

/**
 * USC_CHUNK: Dynamically allocated arrays used as storage for
 * extendable data structures.
 *
 * A chunk is a dynamically allocated array, to hold data, with a link to a
 * list of chunks.
 **/
struct _USC_VECTOR_;
struct _INST;
typedef struct _USC_CHUNK_
{
	IMG_UINT32 uIndex;					/* Index of the first element in the chunk */
	IMG_PVOID *pvArray;					/* Pointer to the array */
#if 0
	/* This version used for debugging */
	struct _USC_VECTOR_* pvArray[USC_MIN_ARRAY_CHUNK + 1];	 /* Pointer to the array */
#endif
	struct _USC_CHUNK_ *psPrev;	/* Pointer to previous in the list */
	struct _USC_CHUNK_ *psNext;	/* Pointer to next in the list */
} USC_CHUNK, *USC_PCHUNK;

/*
   USC_CHUNK_MEM_ARRAY: Index into State->apsChunkMem for chunks of size USC_MIC_ARRAY_CHUNK.
   USC_CHUNK_MEM_VECTOR: Index into State->apsChunkMem for chunks of size USC_MIC_VECTOR_CHUNK.
 */
#define USC_CHUNK_MEM_ARRAY (0)
#define USC_CHUNK_MEM_VECTOR (1)

/*
  USC_STATE_FREEFN: Free function for internal compiler use.
 */
typedef IMG_VOID (*USC_STATE_FREEFN)(USC_DATA_STATE_PTR, IMG_PVOID*);

/**
 * Extendable arrays and bitvectors
 *
 * Data in an array is arranged in chunks of USC_ARRAY_CHUNK_SIZE + 1 elements.
 * The first element in a chunk is always the index of the first array element
 * stored in the chunk.
 **/
typedef struct _USC_ARRAY_
{
	/* uNumChunks: Number of chunks in the array */
	IMG_UINT32 uNumChunks;
	/* uChunk: Number of (IMG_UINT32) elements */
	IMG_UINT32 uChunk;
	/* uEnd: Index after last element (uIdx < uEnd checks for uIdx in bounds) */
	IMG_UINT32 uEnd;
	/* uSize: Size of each element in bytes */
	IMG_UINT32 uSize;
	/* pvDefault: Default value to report */
	IMG_PVOID pvDefault;
	/* psFirst: Pointer to the first chunk of array */
	USC_PCHUNK psFirst;
	/* sMemo: Memoise element access */
	USC_MEMO sMemo;
} USC_ARRAY, *USC_PARRAY;

/* ARRAY_CHUNK_SIZE: Number of elements in an array chunk */
#define USC_ARRAY_CHUNK_SIZE USC_MIN_ARRAY_CHUNK

typedef struct _USC_VECTOR_
{
	/* Number of chunks in the vector. */
	IMG_UINT32 uSize;
	/* Default value for a dword in the vector. */
	IMG_UINT32 uDefault;
	/* Default value for a bit in the vector. */
	IMG_BOOL bDefault;
	/* uChunk: Number of unsigned integers in the chunk. */
	IMG_UINT32 uChunk;
	/* psFirst: Pointer to the first in the list of chunks */
	USC_PCHUNK psFirst;
	/* sMemo: Memoise element access */
	USC_MEMO sMemo;
} USC_VECTOR, *USC_PVECTOR;

/*
   USC_BITS_PER_VCHUNK: Number of bits in each vector chunk
*/
#define USC_BITS_PER_VCHUNK 256

/*
   USC_MIN_VECTOR_CHUNK: Minimum size in sizeof(IMG_UINT32) of a chunk used for bit vectors
   (enough space for 256 bits).
*/
#define USC_MIN_VECTOR_CHUNK (USC_BITS_PER_VCHUNK / BITS_PER_UINT)

/* USC_VECTOR_OP: Operations on extendable bitvectors and array elements */
typedef enum _USC_VECTOR_OP_
{
	USC_VEC_NOT,		/* Bitwise negation */
	USC_VEC_AND,		/* Bitwise and */
	USC_VEC_OR,			/* Bitwise or */
	USC_VEC_ADD,		/* Add to an array element */
	USC_VEC_SUB,		/* Subtract from an array element */

	USC_VEC_EQ,			/* Equal bit vectors */

	USC_VEC_DISJ		/* Bisjoint bit vectors */

} USC_VECTOR_OP;


IMG_VOID ClearArray(USC_DATA_STATE_PTR psState,
					USC_PARRAY psArray,
					USC_STATE_FREEFN fnFree);
IMG_VOID FreeArray(USC_DATA_STATE_PTR psState,
				   USC_PARRAY *ppsArray);
IMG_VOID InitArray(USC_PARRAY psArray,
				   IMG_UINT32 uChunk,
				   IMG_PVOID pvDefault,
				   IMG_UINT32 uSize);
USC_PARRAY NewArray(USC_DATA_STATE_PTR psState,
					IMG_UINT32 uChunk,
					IMG_PVOID pvDefault,
					IMG_UINT32 uSize);
IMG_PVOID ArrayGet(USC_DATA_STATE_PTR psState,
				   USC_PARRAY psArray,
				   IMG_UINT32 uIdx);
USC_PARRAY ArraySet(USC_DATA_STATE_PTR psState,
					USC_PARRAY psArray,
					IMG_UINT32 uIdx,
					IMG_PVOID pvData);
USC_PARRAY ArrayElemOp(USC_DATA_STATE_PTR psState,
					   USC_PARRAY psArray,
					   USC_VECTOR_OP eOp,
					   IMG_UINT32 uIdx,
					   IMG_UINT32 uArg);

IMG_VOID ClearVector(USC_DATA_STATE_PTR psState, USC_PVECTOR psVector);
IMG_VOID FreeVector(USC_DATA_STATE_PTR psState, USC_PVECTOR *ppsVector);
IMG_VOID InitVector(USC_PVECTOR psVector,
					IMG_UINT32 uChunk,
					IMG_BOOL bDefault);
USC_PVECTOR NewVector(USC_DATA_STATE_PTR psState,
					  IMG_UINT32 uChunk,
					  IMG_BOOL bDefault);
IMG_UINT32 VectorGet(USC_DATA_STATE_PTR psState,
					 USC_PVECTOR psVector,
					 IMG_UINT32 uBitIdx);
USC_PVECTOR VectorSet(USC_DATA_STATE_PTR psState,
					  USC_PVECTOR psVector,
					  IMG_UINT32 uBitIdx,
					  IMG_UINT32 uData);
IMG_UINT32 VectorGetRange(USC_DATA_STATE_PTR psState,
						  USC_PVECTOR psVector,
						  IMG_UINT32 uEndIdx,
						  IMG_UINT32 uStartIdx);
USC_PVECTOR VectorSetRange(USC_DATA_STATE_PTR psState,
						   USC_PVECTOR psVector,
						   IMG_UINT32 uEndIdx,
						   IMG_UINT32 uStartIdx,
						   IMG_UINT32 uData);
USC_PVECTOR VectorOrRange(USC_DATA_STATE_PTR psState,
						  USC_PVECTOR psVector,
						  IMG_UINT32 uEndIdx,
						  IMG_UINT32 uStartIdx,
						  IMG_UINT32 uData);
USC_PVECTOR VectorAndRange(USC_DATA_STATE_PTR psState,
						   USC_PVECTOR psVector,
						   IMG_UINT32 uEndIdx,
						   IMG_UINT32 uStartIdx,
						   IMG_UINT32 uData);
USC_PVECTOR VectorOp(USC_DATA_STATE_PTR psState,
					 USC_VECTOR_OP eOp,
					 USC_PVECTOR psDest,
					 USC_PVECTOR psSrc1,
					 USC_PVECTOR psSrc2);
USC_PVECTOR VectorCopy(USC_DATA_STATE_PTR psState,
					   USC_PVECTOR psVec,
					   USC_PVECTOR psDest);
					   
#if defined(DEBUG)
IMG_BOOL VectorMatch(USC_DATA_STATE_PTR psState,
						const USC_VECTOR *psSrc1,
						const USC_VECTOR *psSrc2);
#endif

/*
	Iterator for the set bits in a vector.
*/

typedef struct _VECTOR_ITERATOR
{
	/*
		Vector over which we are iterating.
	*/
	USC_PVECTOR	psVector;
	/*
		Dword within the current chunk.
	*/
	IMG_UINT32	uCurrentDwordPos;
	/*
		Bit within the current dword.
	*/
	IMG_UINT32	uCurrentBitPos;
	/*
		Current chunk.
	*/
	USC_PCHUNK	psCurrentChunk;
	/*
		Size of a range.
	*/
	IMG_UINT32	uStep;
} VECTOR_ITERATOR, *PVECTOR_ITERATOR;

IMG_VOID VectorIteratorInitialize(PINTERMEDIATE_STATE	psState, 
								  USC_PVECTOR			psVector, 
								  IMG_UINT32			uStep, 
								  PVECTOR_ITERATOR		psIterator);
IMG_UINT32 VectorIteratorCurrentPosition(PVECTOR_ITERATOR psIterator);
IMG_UINT32 VectorIteratorCurrentMask(PVECTOR_ITERATOR psIterator);
IMG_VOID VectorIteratorNext(PVECTOR_ITERATOR psIterator);
IMG_BOOL VectorIteratorContinue(PVECTOR_ITERATOR psIterator);

/**
 * Graphs
 **/
typedef enum _USC_GRAPH_TYPE_
{
	/* NULL: Empty/non-existent */
	GRAPH_NULL = 0,

	/* Plain: Simple, extendable graph. */
	GRAPH_PLAIN = (1 << 1),

	/* Symmetric: G(x, y) = G(y, x) */
	GRAPH_SYM = (1 << 2),

	/* Reflexive: G(x, x) = TRUE 	*/
	GRAPH_REFL = (1 << 3)
} USC_GRAPH_TYPE;

typedef IMG_UINT32 USC_GRAPH_FLAGS;

typedef struct _USC_GRAPH_
{
	IMG_UINT32 uChunk;			/* Chunk size of arrays in the graph */
	USC_GRAPH_FLAGS eType;		/* Properties of the graph */
	IMG_PVOID pvDefault;
	USC_PARRAY psArray;
} USC_GRAPH, *USC_PGRAPH;
USC_PGRAPH NewGraph(USC_DATA_STATE_PTR psState,
					IMG_UINT32 uChunk,
					IMG_PVOID pvDefault,
					USC_GRAPH_FLAGS eType);
IMG_VOID ClearGraph(USC_DATA_STATE_PTR psState, USC_PGRAPH psGraph);
IMG_VOID FreeGraph(USC_DATA_STATE_PTR psState, USC_PGRAPH *ppsGraph);
IMG_BOOL GraphGet(USC_DATA_STATE_PTR psState,
				  USC_PGRAPH psGraph,
				  IMG_UINT32 uReg1,
				  IMG_UINT32 uReg2);
IMG_VOID GraphSet(USC_DATA_STATE_PTR psState,
				  USC_PGRAPH psGraph,
				  IMG_UINT32 uReg1,
				  IMG_UINT32 uReg2,
				  IMG_BOOL bSet);
IMG_VOID GraphColRef(USC_DATA_STATE_PTR psState,
					 USC_PGRAPH psGraph,
					 IMG_UINT32 uCol,
					 USC_PVECTOR *ppsVector);
IMG_VOID GraphDupCol(USC_DATA_STATE_PTR psState,
					 USC_PGRAPH psGraph,
					 IMG_UINT32 uSrc,
					 IMG_UINT32 uDst);
#if defined(DEBUG)
IMG_BOOL GraphMatch(USC_DATA_STATE_PTR psState,
						const USC_GRAPH *psSrc1,
						const USC_GRAPH *psSrc2);
#endif
IMG_VOID GraphCopy(USC_DATA_STATE_PTR psState,
				   USC_PGRAPH psSrc,
				   USC_PGRAPH psDst);
IMG_VOID GraphClearRow(USC_DATA_STATE_PTR psState,
					   USC_PGRAPH psGraph,
					   IMG_UINT32 uRow);
IMG_VOID GraphClearCol(USC_DATA_STATE_PTR psState,
					   USC_PGRAPH psGraph,
					   IMG_UINT32 uCol);
IMG_VOID GraphOrCol(USC_DATA_STATE_PTR psState,
					USC_PGRAPH psGraph,
					IMG_UINT32 uCol,
					USC_PVECTOR psVector);

/**
 * Matrices
 **/
typedef enum _USC_MATRIX_TYPE_
{
	MATRIX_PLAIN = 0,

	/* Symmetric: G(x, y) = G(y, x) */
	MATRIX_SYM = (1 << 1)

} USC_MATRIX_TYPE;
typedef IMG_UINT32 USC_MATRIX_FLAGS;
typedef USC_GRAPH USC_MATRIX, *USC_PMATRIX;


USC_PMATRIX NewMatrix(USC_DATA_STATE_PTR psState,
					  IMG_UINT32 uChunk,
					  IMG_PVOID pvDefault,
					  USC_MATRIX_FLAGS eType);
IMG_VOID ClearMatrix(USC_DATA_STATE_PTR psState, USC_PMATRIX psMatrix);
IMG_VOID ClearMatrixCol(USC_DATA_STATE_PTR psState,
						USC_PMATRIX psMatrix,
						IMG_UINT32 uCol);
IMG_VOID FreeMatrix(USC_DATA_STATE_PTR psState, USC_PMATRIX *ppsMatrix);
IMG_PVOID MatrixGet(USC_DATA_STATE_PTR psState,
					USC_PMATRIX psMatrix,
					IMG_UINT32 uReg1,
					IMG_UINT32 uReg2);
IMG_VOID MatrixSet(USC_DATA_STATE_PTR psState,
				   USC_PMATRIX psMatrix,
				   IMG_UINT32 uReg1,
				   IMG_UINT32 uReg2,
				   IMG_PVOID psData);


/**
 * Lists
 **/
void InsertSort(USC_DATA_STATE_PTR psState,
				USC_COMPARE_FN pfnCompare,
				IMG_UINT32 uArraySize,
				IMG_PVOID* apsArray);

#define IMG_OFFSETOF(TYPE, MEMBER)				((IMG_UINTPTR_T)(&(((TYPE)0)->MEMBER)))
#define IMG_CONTAINING_RECORD(PTR, TYPE, ENTRY)	((TYPE)(((IMG_UINTPTR_T)PTR) - IMG_OFFSETOF(TYPE, ENTRY)))

typedef struct _USC_LIST_
{
	struct _USC_LIST_ENTRY_*	psHead;
	struct _USC_LIST_ENTRY_*	psTail;
} USC_LIST, *PUSC_LIST;

typedef struct _USC_LIST_ENTRY_
{
	struct _USC_LIST_ENTRY_ *psPrev;
	struct _USC_LIST_ENTRY_ *psNext;
} USC_LIST_ENTRY, *PUSC_LIST_ENTRY;

typedef IMG_INT32 (*USC_LIST_ENTRY_COMPARE_FN)(PUSC_LIST_ENTRY psListEntry1, PUSC_LIST_ENTRY psListEntry2);

void InsertSortList(PUSC_LIST					psList,
					USC_LIST_ENTRY_COMPARE_FN	pfnCompare);
IMG_VOID InsertInListSorted(PUSC_LIST					psList,
							USC_LIST_ENTRY_COMPARE_FN	pfnCompare,
							PUSC_LIST_ENTRY				psListEntryToInsert);

FORCE_INLINE
IMG_BOOL IsEntryInList(PUSC_LIST psList, PUSC_LIST_ENTRY psListEntry)
/*****************************************************************************
 FUNCTION	: IsEntryInList

 PURPOSE	: Check if a list entry is in a list (on the assumption that
			  list entries are always cleared when removed from a list).

 PARAMETERS	: psList				- List to check.
			  psListEntry			- List entry to check.
*****************************************************************************/
{
	if (psListEntry->psNext != NULL || 
		psListEntry->psPrev != NULL || 
		psListEntry == psList->psHead ||
		psListEntry == psList->psTail)
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

FORCE_INLINE
IMG_VOID ClearListEntry(PUSC_LIST_ENTRY	psListEntry)
/*****************************************************************************
 FUNCTION	: ClearListEntry

 PURPOSE	: Initializes a list entry.

 PARAMETERS	: psListEntry			- List entry to initialize.
*****************************************************************************/
{
	psListEntry->psNext = psListEntry->psPrev = NULL;
}

FORCE_INLINE
IMG_VOID InitializeList(PUSC_LIST	psList)
/*****************************************************************************
 FUNCTION	: InitializeList

 PURPOSE	: Initializes a list.

 PARAMETERS	: psList				- List to initialize.
*****************************************************************************/
{
	psList->psHead = psList->psTail = NULL;
}

FORCE_INLINE
IMG_BOOL IsListEmpty(PUSC_LIST	psList)
/*****************************************************************************
 FUNCTION	: IsListEmpty

 PURPOSE	: Check if a list contains zero entries..

 PARAMETERS	: psList				- List to check.
*****************************************************************************/
{
	return psList->psHead == NULL ? IMG_TRUE : IMG_FALSE;
}

FORCE_INLINE
IMG_BOOL IsSingleElementList(PUSC_LIST	psList)
/*****************************************************************************
 FUNCTION	: IsSingleElementList

 PURPOSE	: Check if a list contain exactly one entry.

 PARAMETERS	: psList				- List to check.
*****************************************************************************/
{
	if (psList->psHead != NULL &&
		psList->psHead->psPrev == NULL &&
		psList->psHead->psNext == NULL)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}


FORCE_INLINE
IMG_BOOL IsTwoElementList(PUSC_LIST	psList)
/*****************************************************************************
 FUNCTION	: IsTwoElementList

 PURPOSE	: Check if a list contain exactly two entries.

 PARAMETERS	: psList				- List to check.
*****************************************************************************/
{
	if (psList->psHead != NULL &&
		psList->psHead->psNext != NULL &&
		psList->psHead->psNext->psNext == NULL)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

FORCE_INLINE
IMG_VOID PrependToList(PUSC_LIST		psList,
					   PUSC_LIST_ENTRY	psItem)
/*****************************************************************************
 FUNCTION	: PrependToList

 PURPOSE	: Adds an item to the head of a list.

 PARAMETERS	: psList				- List to prepend to.
			  psItem				- Item to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psItem->psPrev = NULL;
	psItem->psNext = psList->psHead;

	if (psList->psHead == NULL)
	{
		psList->psTail = psItem;
	}
	else
	{
		psList->psHead->psPrev = psItem;
	}
	psList->psHead = psItem;
}

FORCE_INLINE
PUSC_LIST_ENTRY RemoveListHead(PUSC_LIST	psList)
/*****************************************************************************
 FUNCTION	: RemoveListHead

 PURPOSE	: Removes the list head.

 PARAMETERS	: psList				- To remove the head from.

 RETURNS	: A pointer to the old head.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psOldHead;

	if (IsListEmpty(psList))
	{
		return NULL;
	}

	psOldHead = psList->psHead;

	psList->psHead = psList->psHead->psNext;
	if (psList->psHead != NULL)
	{
		psList->psHead->psPrev = NULL;
	}
	if (psList->psTail == psOldHead)
	{
		psList->psTail = psList->psHead;
	}

	return psOldHead;
}

FORCE_INLINE
PUSC_LIST_ENTRY RemoveListTail(PUSC_LIST	psList)
/*****************************************************************************
 FUNCTION	: RemoveListTail

 PURPOSE	: Removes the list tail.

 PARAMETERS	: psList				- To remove the tail from.

 RETURNS	: A pointer to the old tail.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psOldTail;

	if (IsListEmpty(psList))
	{
		return NULL;
	}

	psOldTail = psList->psTail;

	psList->psTail = psList->psTail->psPrev;
	if (psList->psTail != NULL)
	{
		psList->psTail->psNext = NULL;
	}
	if (psList->psHead == psOldTail)
	{
		psList->psHead = psList->psTail;
	}

	return psOldTail;
}

FORCE_INLINE
IMG_VOID AppendListToList(PUSC_LIST	psList1,
						  PUSC_LIST	psList2)
/*****************************************************************************
 FUNCTION	: AppendListToList

 PURPOSE	: Concatenate two lists.

 PARAMETERS	: psList1		- List to place first.
			  psList2		- List to place second.
*****************************************************************************/
{
	if (psList2->psHead == NULL)
	{
		/* Second list is empty: nothing to do. */
	}
	else if (psList1->psHead == NULL)
	{
		psList1->psHead = psList2->psHead;
		psList1->psTail = psList2->psTail;
	}
	else
	{
		psList1->psTail->psNext = psList2->psHead;
		psList2->psHead->psPrev = psList1->psTail;

		psList1->psTail = psList2->psTail;
	}
}

FORCE_INLINE
IMG_VOID AppendToList(PUSC_LIST psList, PUSC_LIST_ENTRY psEntry)
/*****************************************************************************
 FUNCTION	: AppendToList

 PURPOSE	: Add an entry to the end of a list.

 PARAMETERS	: psList		- List to add to.
			  psEntry		- Entry to add.
*****************************************************************************/
{
	psEntry->psPrev = psList->psTail;
	psEntry->psNext = NULL;
	if (psList->psTail == NULL)
	{
		psList->psHead = psEntry;
	}
	else
	{
		psList->psTail->psNext = psEntry;
	}
	psList->psTail = psEntry;
}

FORCE_INLINE
IMG_VOID InsertInList(PUSC_LIST psList, PUSC_LIST_ENTRY psEntryToInsert, PUSC_LIST_ENTRY psEntryToInsertBefore)
/*****************************************************************************
 FUNCTION	: InsertInList

 PURPOSE	: Inserts an entry in the middle of a list.

 PARAMETERS	: psList				- List to insert into.
			  psEntryToInsert		- Entry to insert.
			  psEntryToInsertBefore	- Location to insert the entry.
*****************************************************************************/
{
	if (psEntryToInsertBefore == NULL)
	{
		AppendToList(psList, psEntryToInsert);
		return;
	}

	psEntryToInsert->psPrev = psEntryToInsertBefore->psPrev;
	psEntryToInsert->psNext = psEntryToInsertBefore;

	if (psEntryToInsertBefore->psPrev != NULL)
	{
		psEntryToInsertBefore->psPrev->psNext = psEntryToInsert;
	}
	else
	{
		psList->psHead = psEntryToInsert;
	}
	psEntryToInsertBefore->psPrev = psEntryToInsert;
}

FORCE_INLINE
IMG_VOID RemoveFromList(PUSC_LIST psList, PUSC_LIST_ENTRY psEntry)
/*****************************************************************************
 FUNCTION	: RemoveFromList

 PURPOSE	: Remove an entry from a list.

 PARAMETERS	: psList		- List to remove from.
			  psEntry		- Entry to remove.
*****************************************************************************/
{
	if (psEntry->psPrev == NULL)
	{
		psList->psHead = psEntry->psNext;
	}
	else
	{
		psEntry->psPrev->psNext = psEntry->psNext;
	}
	if (psEntry->psNext == NULL)
	{
		psList->psTail = psEntry->psPrev;
	}
	else
	{
		psEntry->psNext->psPrev = psEntry->psPrev;
	}
}

FORCE_INLINE
IMG_VOID CutList(PUSC_LIST psOldList, PUSC_LIST_ENTRY psCutPoint, PUSC_LIST psNewList)
/*****************************************************************************
 FUNCTION	: CutList

 PURPOSE	: Create a separate list from all the entries in a list after a
			  particular point.

 PARAMETERS	: psOldList		- List to remove from.
			  psCutPoint	- Start of the entries to remove.
			  psNewList		- Returns the new list.
*****************************************************************************/
{
	psNewList->psHead = psCutPoint;
	psNewList->psTail = psOldList->psTail;

	if (psCutPoint->psPrev == NULL)
	{
		psOldList->psHead = NULL;
	}
	else
	{
		psCutPoint->psPrev->psNext = NULL;
	}
	psOldList->psTail = psCutPoint->psPrev;

	psCutPoint->psPrev = NULL;
}

FORCE_INLINE
IMG_UINT32 GetListNodesCount(PUSC_LIST psList)
{
	PUSC_LIST_ENTRY	psListEntry;
	IMG_UINT32 uListNodesCount = 0;
	for (psListEntry = psList->psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		uListNodesCount++;
	}
	return uListNodesCount;
}

/**
 * Adjacency lists
 **/

#define ADJACENCY_LIST_NODES_PER_CHUNK		(32)

typedef struct _ADJACENCY_LIST_CHUNK
{
	struct _ADJACENCY_LIST_CHUNK*	psNext;
	IMG_UINT32						auNodes[ADJACENCY_LIST_NODES_PER_CHUNK];
} ADJACENCY_LIST_CHUNK, *PADJACENCY_LIST_CHUNK;

typedef struct _ADJACENCY_LIST
{
	PADJACENCY_LIST_CHUNK	psFirstChunk;
	PADJACENCY_LIST_CHUNK	psLastChunk;
	IMG_UINT32				uCountInLastChunk;
} ADJACENCY_LIST, *PADJACENCY_LIST;

typedef struct _ADJACENCY_LIST_ITERATOR
{
	PADJACENCY_LIST_CHUNK	psCurrentChunk;
	IMG_UINT32				uCurrentIndex;
	IMG_UINT32				uCurrentChunkLimit;
	IMG_UINT32				uLastChunkLimit;
} ADJACENCY_LIST_ITERATOR, *PADJACENCY_LIST_ITERATOR;

FORCE_INLINE
IMG_VOID MoveToNextChunk(PADJACENCY_LIST_ITERATOR	psIterState,
						 PADJACENCY_LIST_CHUNK		psNextChunk)
/*****************************************************************************
 FUNCTION	: MoveToNextChunk

 PURPOSE	: Sets up the iterator state when moving onto the next chunk.

 PARAMETERS	: psIterState			- Iterator state.
			  psNextChunk			- Next chunk to iterate from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psIterState->psCurrentChunk = psNextChunk;

	/*
		Check for reaching the end of the list of chunks.
	*/
	if (psIterState->psCurrentChunk == NULL)
	{
		return;
	}

	/*
		Reset the index into the array per-chunk.
	*/
	psIterState->uCurrentIndex = 0;

	/*
		Get the number of nodes remaining in this chunk.
	*/
	if (psIterState->psCurrentChunk->psNext == NULL)
	{
		psIterState->uCurrentChunkLimit = psIterState->uLastChunkLimit;
	}
	else
	{
		psIterState->uCurrentChunkLimit = ADJACENCY_LIST_NODES_PER_CHUNK;
	}
}

FORCE_INLINE
IMG_UINT32 NextAdjacent(PADJACENCY_LIST_ITERATOR	psIterState)
/*****************************************************************************
 FUNCTION	: NextAdjacent

 PURPOSE	: Get the next adjacent node in the list.

 PARAMETERS	: psIterState			- Iterator state.

 RETURNS	: The next adjacent node.
*****************************************************************************/
{
	IMG_UINT32	uRet;

	/*
		Check for reaching the end of the array of nodes in this chunk.
	*/
	if (psIterState->uCurrentIndex == psIterState->uCurrentChunkLimit)
	{
		MoveToNextChunk(psIterState, psIterState->psCurrentChunk->psNext);
	}

	/*
		Check for reaching the end of the list of chunks.
	*/
	if (psIterState->psCurrentChunk == NULL)
	{
		return UINT_MAX;
	}

	/*
		Return the next node in this chunk.
	*/
	uRet = psIterState->psCurrentChunk->auNodes[psIterState->uCurrentIndex++];

	return uRet;
}

FORCE_INLINE
IMG_UINT32 FirstAdjacent(PADJACENCY_LIST			psList,
						 PADJACENCY_LIST_ITERATOR	psIterState)
/*****************************************************************************
 FUNCTION	: FirstAdjacent

 PURPOSE	: Get the first adjacent node and set up the iterator state to
			  iterate over the remaining node.

 PARAMETERS	: psList				- List of adjacent nodes.
			  psIterState			- Iterator state.

 RETURNS	: The first adjacent node.
*****************************************************************************/
{
	if (psList == NULL || psList->psFirstChunk == NULL)
	{
		/*
			No nodes in the list so initialize the iterator state
			to default values.
		*/
		psIterState->psCurrentChunk = NULL;
		psIterState->uCurrentIndex = 0;
		psIterState->uLastChunkLimit = 0;
		psIterState->uCurrentChunkLimit = 0;
		return UINT_MAX;
	}

	/*
		Save the number of nodes in the last chunk in the
		list.
	*/
	psIterState->uLastChunkLimit = psList->uCountInLastChunk;

	/*
		Set up the iterator state for the first chunk.
	*/
	MoveToNextChunk(psIterState, psList->psFirstChunk);

	/*
		Return the first node.
	*/
	return NextAdjacent(psIterState);
}

FORCE_INLINE
IMG_BOOL IsLastAdjacent(PADJACENCY_LIST_ITERATOR	psIterState)
/*****************************************************************************
 FUNCTION	: IsLastAdjacent

 PURPOSE	: Has the end of the list of adjacent nodes been reached?

 PARAMETERS	: psIterState			- Iterator state.
*****************************************************************************/
{
	return psIterState->psCurrentChunk == NULL ? IMG_TRUE : IMG_FALSE;
}

IMG_VOID InitialiseAdjacencyList(PADJACENCY_LIST	psList);
IMG_VOID DeleteAdjacencyList(USC_DATA_STATE_PTR psState, PADJACENCY_LIST	psList);
IMG_VOID DeleteAdjacencyListIndirect(USC_DATA_STATE_PTR psState, IMG_PVOID* ppvList);
IMG_VOID AddToAdjacencyList(USC_DATA_STATE_PTR	psState,
							PADJACENCY_LIST					psList,
							IMG_UINT32						uItemToAdd);
IMG_VOID ReplaceInAdjacencyList(USC_DATA_STATE_PTR	psState,
								PADJACENCY_LIST					psList,
								IMG_UINT32						uReplaceBy,
								IMG_UINT32						uReplaceWith);
IMG_VOID RemoveFromAdjacencyList(USC_DATA_STATE_PTR	psState,
								 PADJACENCY_LIST				psList,
								 IMG_UINT32						uItemToRemove);
PADJACENCY_LIST CloneAdjacencyList(USC_DATA_STATE_PTR	psState,
								   PADJACENCY_LIST					psList);
IMG_UINT32 GetAdjacencyListLength(PADJACENCY_LIST psList);

typedef USC_TREE REGISTER_VECTOR_MAP, *PREGISTER_VECTOR_MAP;

PREGISTER_VECTOR_MAP IntermediateRegisterVectorMapCreate(PINTERMEDIATE_STATE psState);
IMG_VOID IntermediateRegisterVectorMapDestroy(PINTERMEDIATE_STATE psState, PREGISTER_VECTOR_MAP psMap);
IMG_VOID IntermediateRegisterVectorMapSet(PINTERMEDIATE_STATE	psState, 
										  PREGISTER_VECTOR_MAP	psMap, 
										  IMG_UINT32			uVectorLength, 
										  PCARG					asVector, 
										  IMG_PVOID				pvData);
IMG_PVOID IntermediateRegisterVectorMapGet(PREGISTER_VECTOR_MAP psMap, IMG_UINT32 uVectorLength, PCARG asVector);

typedef USC_TREE REGISTER_MAP, *PREGISTER_MAP;

PREGISTER_MAP IntermediateRegisterMapCreate(PINTERMEDIATE_STATE psState);
IMG_VOID IntermediateRegisterMapDestroy(PINTERMEDIATE_STATE psState, PREGISTER_MAP psMap);
IMG_VOID IntermediateRegisterMapSet(PINTERMEDIATE_STATE psState, PREGISTER_MAP psMap, IMG_UINT32 uRegNum, IMG_PVOID pvData);
IMG_VOID IntermediateRegisterMapRemove(PINTERMEDIATE_STATE psState, PREGISTER_MAP psMap, IMG_UINT32 uRegNum);
IMG_PVOID IntermediateRegisterMapGet(PREGISTER_MAP psMap, IMG_UINT32 uRegNum);

/**
 * Sparse sets.
 **/

typedef struct _SPARSE_SET
{
	/*
		If k is a member of the set then
			0 <= puSparse[k] < uMemberCount
			puDense[puSparse[k]] == k
	*/
	IMG_PUINT32	puSparse;
	IMG_PUINT32	puDense;
	IMG_UINT32	uMemberCount;
} SPARSE_SET, *PSPARSE_SET;

typedef struct _SPARSE_SET_ITERATOR
{
	PSPARSE_SET	psSet;
	IMG_UINT32	uIdx;
} SPARSE_SET_ITERATOR, *PSPARSE_SET_ITERATOR;

PSPARSE_SET SparseSetAlloc(PINTERMEDIATE_STATE psState, IMG_UINT32 uMaxMemberCount);
IMG_VOID SparseSetFree(PINTERMEDIATE_STATE psState, PSPARSE_SET psSet);
IMG_VOID SparseSetEmpty(PSPARSE_SET psSet);
IMG_VOID SparseSetCopy(PSPARSE_SET psDest, PSPARSE_SET psSrc);
IMG_BOOL SparseSetIsMember(PSPARSE_SET psSet, IMG_UINT32 uMember);
IMG_VOID SparseSetAddMember(PSPARSE_SET psSet, IMG_UINT32 uMemberToAdd);
IMG_VOID SparseSetDeleteMember(PSPARSE_SET psSet, IMG_UINT32 uMemberToDelete);

FORCE_INLINE
IMG_VOID SparseSetIteratorInitialize(PSPARSE_SET psSet, PSPARSE_SET_ITERATOR psIter)
/*****************************************************************************
 FUNCTION	: SparseSetIteratorInitialize

 PURPOSE	: Initialize an iterator over a sparse set.

 PARAMETERS	: psSet				- Set to iterate over.
			  psIter			- Iterator to initialize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psIter->psSet = psSet;
	psIter->uIdx = 0;
}

FORCE_INLINE
IMG_BOOL SparseSetIteratorContinue(PSPARSE_SET_ITERATOR psIter)
/*****************************************************************************
 FUNCTION	: SparseSetIteratorContinue

 PURPOSE	: Checks for reaching the end of a sparse set when iterating over
			  the members.

 PARAMETERS	: psIter			- Iterator to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return (psIter->uIdx < psIter->psSet->uMemberCount) ? IMG_TRUE : IMG_FALSE;
}

FORCE_INLINE
IMG_VOID SparseSetIteratorNext(PSPARSE_SET_ITERATOR psIter)
/*****************************************************************************
 FUNCTION	: SparseSetIteratorNext

 PURPOSE	: Move onto the next member of a sparse set when iterating.

 PARAMETERS	: psIter			- Iterator to modify.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psIter->uIdx++;
}

FORCE_INLINE
IMG_UINT32 SparseSetIteratorCurrent(PSPARSE_SET_ITERATOR psIter)
/*****************************************************************************
 FUNCTION	: SparseSetIteratorCurrent

 PURPOSE	: Get the currently iterated member of a sparse set.

 PARAMETERS	: psIter			- Iterator to check.

 RETURNS	: The current member.
*****************************************************************************/
{
	return psIter->psSet->puDense[psIter->uIdx];
}

/**
 * Register substitutions
 **/

typedef struct _ARG *USC_DATA_REG_PTR;

/* USC_REGSUBST: A register substitution */
typedef USC_TREE USC_REGSUBST, *PUSC_REGSUBST;

PUSC_REGSUBST RegSubstAlloc(USC_DATA_STATE_PTR psState);
IMG_VOID RegSubstDelete(USC_DATA_STATE_PTR psState,
						PUSC_REGSUBST psSubst);
PUSC_REGSUBST RegSubstAdd(USC_DATA_STATE_PTR psState,
						  PUSC_REGSUBST psSubst,
						  USC_DATA_REG_PTR psReg,
						  USC_DATA_REG_PTR psRepl);
IMG_BOOL RegSubstFind(PUSC_REGSUBST psSubst,
					  USC_DATA_REG_PTR psReg,
					  USC_DATA_REG_PTR* ppsRepl);
IMG_BOOL RegSubstBaseFind(PUSC_REGSUBST psSubst,
						  IMG_UINT32 uRegType,
						  IMG_UINT32 uRegNum,
						  IMG_PUINT32 puReplRegType,
						  IMG_PUINT32 puReplRegNum);
PUSC_REGSUBST BaseRegSubstAdd(USC_DATA_STATE_PTR psState,
							  PUSC_REGSUBST psSubst,
							  IMG_UINT32 uRegType,
							  IMG_UINT32 uRegNumber,
							  IMG_UINT32 uReplType,
							  IMG_UINT32 uReplNumber);


#if defined(UF_TESTBENCH)
IMG_VOID RegSubstPrint(USC_DATA_STATE_PTR psState, PUSC_REGSUBST psSubst);
#endif /* DEBUG */

typedef struct _SAFE_LIST
{
	/*
		List of items in the list.
	*/
	USC_LIST	sBaseList;
	/*
		List of active iterators.
	*/
	USC_LIST	sIteratorList;
} SAFE_LIST, *PSAFE_LIST;

typedef struct _SAFE_LIST_ITERATOR
{
	/*
		List over which we are iterating.
	*/
	PSAFE_LIST		psList;
	/*
		Entry in the list's list of active iterators.
	*/
	USC_LIST_ENTRY	sIteratorListEntry;
	/*
		Next position for the iterator when moving forwards
	*/
	PUSC_LIST_ENTRY	psNext;
	/*
		Next position for the iterator when moving backwards.
	*/
	PUSC_LIST_ENTRY psPrev;
	/*
		Current position of the iterator. This might be NULL if the current item was deleted or if
		we have reached the end or the beginning of the list.
	*/
	PUSC_LIST_ENTRY psCurrent;
	/*
		IMG_FALSE if the iterator is positioned either after the end of the list or before the start.
	*/
	IMG_BOOL		bContinue;
} SAFE_LIST_ITERATOR, *PSAFE_LIST_ITERATOR;

IMG_VOID SafeListIteratorInitializeAtPoint(PSAFE_LIST psList, PSAFE_LIST_ITERATOR psIterator, PUSC_LIST_ENTRY psPoint);
IMG_VOID SafeListIteratorInitialize(PSAFE_LIST psList, PSAFE_LIST_ITERATOR psIterator);
IMG_VOID SafeListIteratorInitializeAtEnd(PSAFE_LIST psList, PSAFE_LIST_ITERATOR psIterator);
PUSC_LIST_ENTRY SafeListIteratorCurrent(PSAFE_LIST_ITERATOR psIterator);
IMG_VOID SafeListIteratorNext(PSAFE_LIST_ITERATOR psIterator);
IMG_VOID SafeListIteratorPrev(PSAFE_LIST_ITERATOR psIterator);
IMG_BOOL SafeListIteratorContinue(PSAFE_LIST_ITERATOR psIterator);
IMG_VOID SafeListIteratorFinalise(PSAFE_LIST_ITERATOR psIterator);
IMG_VOID SafeListInitialize(PSAFE_LIST psList);
IMG_VOID SafeListPrependItem(PSAFE_LIST psList, PUSC_LIST_ENTRY psItemToInsert);
IMG_VOID SafeListInsertItemBeforePoint(PSAFE_LIST			psList,
									   PUSC_LIST_ENTRY		psItemToInsert,
									   PUSC_LIST_ENTRY		psPoint);
IMG_VOID SafeListAppendItem(PSAFE_LIST psList, PUSC_LIST_ENTRY psItemToInsert);
IMG_VOID SafeListRemoveItem(PSAFE_LIST psList, PUSC_LIST_ENTRY psItemToRemove);

FORCE_INLINE
IMG_BOOL SafeListIsEmpty(PSAFE_LIST psList)
{
	return IsListEmpty(&psList->sBaseList);
}

typedef struct _INTFGRAPH_VERTEX
{
	/*
		List of vertices connected to this vertex.
	*/
	ADJACENCY_LIST		sIntfList;
	/*
		Row in the interference bit matrix for this vertex. The matrix is symmetrical so only the
		entries for vertices with lower numbers than this one are stored here. The other entries are
		found in the row for the higher numbered vertex.
	*/
	IMG_PUINT32			puIntfGraphRow;
	/*
		Numbre of vertices connected to this vertex.
	*/
	IMG_UINT32			uDegree;
} INTFGRAPH_VERTEX, *PINTFGRAPH_VERTEX;

typedef struct _INTFGRAPH
{
	/*
		Count of vertices in the graph.
	*/
	IMG_UINT32			uVertexCount;
	/*
		Information about each vertex.
	*/
	PINTFGRAPH_VERTEX	asVertices;
	/*
		Bit array for an entry for each vertex. An entry is set if the vertex has been
		removed from the graph.
	*/
	IMG_PUINT32			auRemoved;
} INTFGRAPH, *PINTFGRAPH;

FORCE_INLINE
IMG_UINT32 IntfGraphGetVertexDegree(PINTFGRAPH psGraph, IMG_UINT32 uVertex)
/*****************************************************************************
 FUNCTION   : IntfGraphGetVertexDegree

 PURPOSE    : Get the count of vertices adjacent to a specified vertex.

 PARAMETERS : psGraph			- Graph to query.
			  uVertex			- Vertex to query.

 RETURNS    : The count.
*****************************************************************************/
{
	return psGraph->asVertices[uVertex].uDegree;
}

FORCE_INLINE
IMG_BOOL IntfGraphIsVertexRemoved(PINTFGRAPH psGraph, IMG_UINT32 uVertex)
/*****************************************************************************
 FUNCTION   : IntfGraphIsVertexRemoved

 PURPOSE    : Check if a vertex has been removed from the graph.

 PARAMETERS : psGraph			- Graph to query.
			  uVertex			- Vertex to query.

 RETURNS    : TRUE if the graph has been removed.
*****************************************************************************/
{
	return GetBit(psGraph->auRemoved, uVertex) ? IMG_TRUE : IMG_FALSE;
}

FORCE_INLINE
PADJACENCY_LIST IntfGraphGetVertexAdjacent(PINTFGRAPH psGraph, IMG_UINT32 uVertex)
/*****************************************************************************
 FUNCTION   : IntfGraphGetVertexAdjacent

 PURPOSE    : Get a list of the vertices adjacent to a specified vertex.

 PARAMETERS : psGraph			- Graph to query.
			  uVertex			- Vertex to query.

 RETURNS    : A pointer to list of other vertices adjacent to this one.
*****************************************************************************/
{
	return &psGraph->asVertices[uVertex].sIntfList;
}

PINTFGRAPH IntfGraphCreate(PINTERMEDIATE_STATE psState, IMG_UINT32 uVertexCount);
IMG_VOID IntfGraphDelete(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph);
IMG_BOOL IntfGraphGet(PINTFGRAPH psGraph, IMG_UINT32 uVertex1, IMG_UINT32 uVertex2);
IMG_VOID IntfGraphAddEdge(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph, IMG_UINT32 uVertex1, IMG_UINT32 uVertex2);
IMG_VOID IntfGraphMergeVertices(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph, IMG_UINT32 uDest, IMG_UINT32 uSrc);
IMG_VOID IntfGraphInsert(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph, IMG_UINT32 uVertex);
IMG_VOID IntfGraphRemove(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph, IMG_UINT32 uVertex);

IMG_UINT32 FindFirstSetBitInRange(IMG_UINT32 const * puArr, IMG_UINT32 uStart, IMG_UINT32 uCount);

#endif /* __USC_DATA_H */
