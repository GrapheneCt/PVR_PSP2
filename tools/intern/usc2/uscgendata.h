/*****************************************************************************
 Name			: uscgendata.h

 Title			: Generic data types

 C Author 		: Matthew Wahab

 Created  		: 5/05/2009

 Copyright      : 2002-2009 by Imagination Technologies Limited. All rights reserved.
                : No part of this software, either material or conceptual
                : may be copied or distributed, transmitted, transcribed,
                : stored in a retrieval system or translated into any
                : human or computer language in any form by any means,
                : electronic, mechanical, manual or other-wise, or
                : disclosed to third parties without the express written
                : permission of Imagination Technologies Limited, Unit 8, HomePark
                : Industrial Estate, King's Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description 	: Generic data types

 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.9 $

 Modifications	:

 $Log: uscgendata.h $
*****************************************************************************/

#ifndef USCGENDATA_H
#define USCGENDATA_H

/* USC_DATA_DEF: Choose only the data definitions for USC */
#define USC_DATA_DEF

/***
 * Required definitions
 ***/

/*
  GEN_TYNAME(N): Form a type name
  Typical definition is, for some name MOD,
  #define GEN_TYNAME(N) MOD_##N
*/
#ifndef GEN_TYNAME
#warning "Macro GEN_TYNAME must be defined"
#endif /* GEN_TYNAME */
  
/*
  GEN_NAME(N): Form a function or variable name
  Typical definition is, for some name Mod,
  #define GEN_NAME(N) Mod##N
*/
#ifndef GEN_NAME
#warning "Macro GEN_NAME must be defined"
#endif /* GEN_NAME */
  
/* GDATA_ALLOC: malloc function */
#ifndef GDATA_ALLOC
#warning "Macro GDATA_ALLOC must be defined"
#endif /* GDATA_ALLOC */

/* GDATA_FREE: free function */
#ifndef GDATA_FREE
#warning "Macro GDATA_FREE must be defined"
#endif /* GDATA_FREE */

/* GDATA_SYS_STATE: System state */
#ifndef PGDATA_SYS_STATE
#warning "Macro GDATA_SYS_STATE must be defined"
#endif /* PGDATA_SYS_STATE */

/***
 * Functions
 ***/

/*
  COMPARE_FN: Comparison function.
  fn(l, r) < 0  if l is less-than r
  fn(l, r) == 0 if l is equal-to r
  fn(l, r) > 0  if l is greater-than r
 */
typedef IMG_INT32 (*GEN_TYNAME(COMPARE_FN))(IMG_VOID* pvLeft, IMG_VOID* pvRight);

/*
  ITERATOR_FN: Iterator function.
  Apply fn(d, e) to all elements of a structure, where d is data to be passed
  to the function and e the elements of the structure.
 */
typedef IMG_VOID (*GEN_TYNAME(ITERATOR_FN))(IMG_VOID* pvData, IMG_VOID* pvElement);

/***
 * Bitvector functions 
 ***/

/*****************************************************************************
 FUNCTION   : BitVecSize

 PURPOSE    : Calculate the number of bytes needed for a bitvector

 PARAMETERS : uNumBits    - Number of bits in bitvector to get

 RETURNS    : Number of bytes needed for the bitvector
*****************************************************************************/
FORCE_INLINE
IMG_UINT32 GEN_NAME(BitVecSize)(const IMG_UINT32 uNumBits)
{
	return (sizeof(IMG_UINT32) * UINTS_TO_SPAN_BITS(uNumBits));
}

/*****************************************************************************
 FUNCTION   : GetBit

 PURPOSE    : Get a bit in a bitvector represented as an array of integers

 PARAMETERS : auArr    - Bitvector
              uBit     - Bit to get

 RETURNS    : IMG_TRUE if the bit is set, IMG_FALSE otherwise
*****************************************************************************/
FORCE_INLINE
IMG_BOOL GEN_NAME(GetBit)(const IMG_UINT32 auBitVec[],
						  const IMG_UINT32 uBit)
{
	return auBitVec[uBit >> 5] & (1U << (uBit % 32)) ? IMG_TRUE : IMG_FALSE;
}

/*****************************************************************************
 FUNCTION   : SetBit

 PURPOSE    : Set a bit in a bitvector represented as an array of integers

 PARAMETERS : auArr    - Bitvector
              uBit     - Bit to set
              bBitData - IMG_TRUE: set the bit, IMG_FALSE: clear the bit

 RETURNS    : Nothing
*****************************************************************************/
FORCE_INLINE
IMG_VOID GEN_NAME(SetBit)(IMG_UINT32 auBitVec[],
						  IMG_UINT32 uBit,
						  IMG_BOOL bBitValue)
{
	if (bBitValue)
	{
		auBitVec[uBit >> 5] |= (1U << (uBit % 32));
	}
	else
	{
		auBitVec[uBit >> 5] &= ~(1U << (uBit % 32));
	}
}

#ifndef USC_DATA_DEF
/**
 * Simple Lists
 **/

typedef struct GEN_TYNAME(SLIST0_ITEM_)
{
	struct GEN_TYNAME(SLIST0_ITEM_)* psPrev;
	struct GEN_TYNAME(SLIST0_ITEM_)* psNext;
	IMG_CHAR pvData[];
} GEN_TYNAME(SLIST_ITEM), * GEN_TYNAME(SLIST_ITEM_PTR);

typedef struct GEN_TYNAME(SLIST0_)
{
	IMG_UINT32 uElemSize;       // Element size
	GEN_TYNAME(SLIST_ITEM_PTR) psFirst;
	GEN_TYNAME(SLIST_ITEM_PTR) psLast;
} GEN_TYNAME(SLIST), *GEN_TYNAME(SLIST_PTR);
	

/* Make an empty struct so that emacs can handle the indentation */
#if 0
struct _empty_
{
};
#endif

FORCE_INLINE
GEN_TYNAME(SLIST_PTR) GEN_NAME(SListInit)(GEN_TYNAME(SLIST_PTR) psList,
										  IMG_UINT32 uElemSize)
/*****************************************************************************
 FUNCTION   : SListInit

 PURPOSE    : Initialise a simple list.

 PARAMETERS : psList     - List to initialise
              uElemSize  - Size of a list data item

 RETURNS    : A new list
*****************************************************************************/
{
	psList->uElemSize = uElemSize;
	psList->psFirst = NULL;
	psList->psLast = NULL;

	return psList;
}

FORCE_INLINE
GEN_TYNAME(SLIST_PTR) GEN_NAME(SListMake)(PGDATA_SYS_STATE psState, IMG_UINT32 uElemSize)
/*****************************************************************************
 FUNCTION   : SListMake

 PURPOSE    : Create a simple list.

 PARAMETERS : psState    - State
              uElemSize  - Size of a list data item

 RETURNS    : A new list
*****************************************************************************/
{
	GEN_TYNAME(SLIST_PTR) psList;

	psList = GDATA_ALLOC(psState, sizeof(GEN_TYNAME(SLIST)));
	psList->uElemSize = uElemSize;
	psList->psFirst = NULL;
	psList->psLast = NULL;

	return psList;
}

FORCE_INLINE
IMG_BOOL GEN_NAME(SListEmpty)(GEN_TYNAME(SLIST_PTR) psList)
/*****************************************************************************
 FUNCTION	: SListEmpty

 PURPOSE	: Test whether a list is empty

 PARAMETERS	: psList     - List

 RETURNS    : IMG_TRUE iff the list is empty
*****************************************************************************/
{
	if (psList->psFirst == NULL)
		return IMG_TRUE;
	else
		return IMG_FALSE;
}

FORCE_INLINE
GEN_TYNAME(SLIST_PTR) GEN_NAME(SListDrop)(PGDATA_SYS_STATE psState,
										  GEN_TYNAME(SLIST_PTR) psList)
/*****************************************************************************
 FUNCTION	: SListDrop

 PURPOSE	: Drop and delete the front of the list

 PARAMETERS	: psState    - Compiler state
              psList     - List

 RETURNS    : The head of the new list
*****************************************************************************/
{
	GEN_TYNAME(SLIST_ITEM_PTR) psFirst;
	GEN_TYNAME(SLIST_ITEM_PTR) psPrev = NULL;
	GEN_TYNAME(SLIST_ITEM_PTR) psNext = NULL;

	if (GEN_NAME(SListEmpty)(psList))
	{
		return psList;
	}

	psFirst = psList->psFirst;
	psPrev = psFirst->psPrev;
	psNext = psFirst->psNext;

	psList->psFirst = psNext;
	if (psList->psLast == psFirst)
	{
		psList->psLast = psNext;
	}

	/* Detach the front of the list */
	psFirst->psNext = NULL;
	psFirst->psPrev = NULL;
	if (psPrev != NULL)
	{
		psPrev->psNext = NULL;
	}
	if (psNext != NULL)
	{
		psNext->psPrev = NULL;
	}
	GDATA_FREE(psState, psFirst);

	return psList;
}


FORCE_INLINE
IMG_VOID GEN_NAME(SListClear)(PGDATA_SYS_STATE psState, GEN_TYNAME(SLIST_PTR) psList)
/*****************************************************************************
 FUNCTION	: SListClear

 PURPOSE	: Clear a simple list.

 PARAMETERS	: psState    - Compiler state
              psList     - List to clear

 NOTES      : Does not delete the contents of the list.
*****************************************************************************/
{
	GEN_TYNAME(SLIST_ITEM_PTR) psCurr = NULL;
	GEN_TYNAME(SLIST_ITEM_PTR) psNext = NULL;

	if (psList == NULL)
	{
		return;
	}

	psNext = NULL;
	for(psCurr = psList->psFirst; psCurr != NULL; psCurr = psNext)
	{
		psNext = psCurr->psNext;
		GDATA_FREE(psState, psCurr);
	}

	GEN_NAME(SListInit)(psList, psList->uElemSize);
}

FORCE_INLINE
IMG_VOID GEN_NAME(SListDelete)(PGDATA_SYS_STATE psState, GEN_TYNAME(SLIST_PTR) psList)
/*****************************************************************************
 FUNCTION	: SListDelete

 PURPOSE	: Delete a simple list.

 PARAMETERS	: psState    - Compiler state
              psList     - List to delete

 NOTES      : Does not delete the contents of the list
*****************************************************************************/
{
	GEN_TYNAME(SLIST_ITEM_PTR) psCurr = NULL;
	GEN_TYNAME(SLIST_ITEM_PTR) psNext = NULL;

	if (psList == NULL)
	{
		return;
	}

	psNext = NULL;
	for(psCurr = psList->psFirst; psCurr != NULL; psCurr = psNext)
	{
		psNext = psCurr->psNext;
		GDATA_FREE(psState, psCurr);
	}

	GDATA_FREE(psState, psList);
}

FORCE_INLINE
GEN_TYNAME(SLIST_PTR) GEN_NAME(SListAppend)(PGDATA_SYS_STATE psState,
											GEN_TYNAME(SLIST_PTR) psLeft,
											GEN_TYNAME(SLIST_PTR) psRight)
/*****************************************************************************
 FUNCTION	: SListAppend

 PURPOSE	: Append right list to left list.

 PARAMETERS	: psState    - Compiler state
              psLeft     - First list
              psRight    - Second list

 RETURNS    : Left list.

 NOTES      : Deletes right list.
*****************************************************************************/
{
	GEN_TYNAME(SLIST_ITEM_PTR) psLeftLast = NULL;
	GEN_TYNAME(SLIST_ITEM_PTR) psRightFirst = NULL;

	if (psLeft == NULL)
		return psRight;
	if (psRight == NULL)
		return psLeft;
	if (psLeft->uElemSize != psRight->uElemSize)
	{
		return NULL;
	}

	if (GEN_NAME(SListEmpty)(psLeft))
	{
		(*psLeft) = (*psRight);
		GEN_NAME(SListDelete)(psState, psRight);
	}
	if (GEN_NAME(SListEmpty)(psRight))
	{
		GEN_NAME(SListDelete)(psState, psRight);
	}
	psLeftLast = psLeft->psLast;
	psRightFirst = psRight->psFirst;

	psLeftLast->psNext = psRightFirst;
	psRightFirst->psPrev = psLeftLast;

	psLeft->psLast = psRight->psLast;

	psRight->psFirst = NULL;
	psRight->psLast = NULL;
	GEN_NAME(SListDelete)(psState, psRight);

	return psLeft;
}

FORCE_INLINE
GEN_TYNAME(SLIST_PTR) GEN_NAME(SListRev)(GEN_TYNAME(SLIST_PTR) psList)
/*****************************************************************************
 FUNCTION	: SListRev

 PURPOSE	: Reverse a list

 PARAMETERS	: psList     - List to reverse

 RETURNS    : The reversed list
*****************************************************************************/
{
	GEN_TYNAME(SLIST_ITEM_PTR) psCurr = NULL;
	GEN_TYNAME(SLIST_ITEM_PTR) psNext = NULL;

	if (psList == NULL)
		return NULL;

	psNext = NULL;
	for (psCurr = psList->psFirst;
		 psCurr->psNext != NULL;
		 psCurr = psNext)
	{
		psNext = psCurr->psNext;

		psCurr->psNext = psCurr->psPrev;
		psCurr->psPrev = psNext;
	}

	psCurr = psList->psFirst;
	psList->psFirst = psList->psLast;
	psList->psLast = psCurr;

	return psList;
}

FORCE_INLINE
IMG_VOID GEN_NAME(SListSet)(GEN_TYNAME(SLIST_PTR) psList,
							GEN_TYNAME(SLIST_ITEM_PTR) psItem,
							IMG_PVOID ppvData)
/*****************************************************************************
 FUNCTION	: SListSet

 PURPOSE	: Set the data in a list item

 PARAMETERS	: psList     - List containing the item
              psItem     - Item to set
              ppvData    - Pointer to Data to set

 RETURNS    : Nothing

 NOTES      : if psItem is NULL, the head of the list is set.
*****************************************************************************/
{
	memcpy(psItem->pvData, ppvData, psList->uElemSize);
}

FORCE_INLINE
IMG_PVOID GEN_NAME(SListGetPtr)(GEN_TYNAME(SLIST_ITEM_PTR) psItem)
/*****************************************************************************
 FUNCTION	: SListGetPtr

 PURPOSE	: Get a pointer to the data in a list item

 PARAMETERS	: psItem     - Item to get

 RETURNS    : A pointer to the data in the list item
*****************************************************************************/
{
	IMG_PVOID* ppvPtr = NULL;
	if (psItem == NULL)
	{
		return NULL;
	}

	ppvPtr = (IMG_PVOID*)(&(psItem->pvData));
	return (IMG_PVOID)ppvPtr;
}

FORCE_INLINE
IMG_VOID GEN_NAME(SListGet)(GEN_TYNAME(SLIST_PTR) psList,
							GEN_TYNAME(SLIST_ITEM_PTR) psItem,
							IMG_PVOID* ppvData)
/*****************************************************************************
 FUNCTION	: SListGet

 PURPOSE	: Get the data in a list item

 PARAMETERS	: psList     - List
              psItem     - List item to get

 OUTPUT     : ppvData    - Pointer to location to copy data into

 RETURNS    : Nothing
*****************************************************************************/
{
	IMG_PVOID* ppvPtr = NULL;

	ppvPtr = GEN_NAME(SListGetPtr)(psItem);
	memcpy(ppvData, ppvPtr, psList->uElemSize);
}

FORCE_INLINE
GEN_TYNAME(SLIST_ITEM_PTR) GEN_NAME(SListInsertBefore)(PGDATA_SYS_STATE psState,
													   GEN_TYNAME(SLIST_PTR) psList,
													   GEN_TYNAME(SLIST_ITEM_PTR) psPos,
													   IMG_PVOID ppvData)
/*****************************************************************************
 FUNCTION	: SListInsertBefore

 PURPOSE	: Insert an element in the list

 PARAMETERS	: psState    - Compiler state
              psList     - List
              psPos      - Item to insert before or NULL
              ppvData    - Pointer to data to add or NULL

 RETURNS    : The new item
*****************************************************************************/
{
	GEN_TYNAME(SLIST_ITEM_PTR) psItem = NULL;
	GEN_TYNAME(SLIST_ITEM_PTR) psPrev = NULL;

	/* Create new item */
	psItem = (GEN_TYNAME(SLIST_ITEM_PTR))GDATA_ALLOC(psState,
													 (sizeof(GEN_TYNAME(SLIST_ITEM)) +
													  psList->uElemSize));

	/* Attach new item */
	if (psPos == NULL)
	{
		psPos = psList->psFirst;
	}

	psPrev = NULL;
	if (psPos != NULL)
	{
		psPrev = psPos->psPrev;
	}

	psItem->psPrev = psPrev;
	if (psPrev != NULL)
	{
		psPrev->psNext = psItem;
	}
	psItem->psNext = psPos;
	if (psPos != NULL)
	{
		psPos->psPrev = psItem;
	}

	if (psList->psFirst == NULL)
	{
		psList->psFirst = psItem;
		psList->psLast = psItem;
	}
	else if (psList->psFirst == psPos)
	{
		psList->psFirst = psItem;
	}

	/* Copy data */
	if (ppvData != NULL)
	{
		GEN_NAME(SListSet)(psList, psItem, ppvData);
	}

	/* Done */
	return psItem;
}

FORCE_INLINE
GEN_TYNAME(SLIST_ITEM_PTR) GEN_NAME(SListInsertAfter)(PGDATA_SYS_STATE psState,
													  GEN_TYNAME(SLIST_PTR) psList,
													  GEN_TYNAME(SLIST_ITEM_PTR) psPos,
													  IMG_PVOID ppvData)
/*****************************************************************************
 FUNCTION	: SListInsertAfter

 PURPOSE	: Insert an element in the list

 PARAMETERS	: psState    - Compiler state
              psList     - List
              psPos      - Item to insert after or NULL
              ppvData    - Pointer to data to add or NULL

 RETURNS    : The new item
*****************************************************************************/
{
	GEN_TYNAME(SLIST_ITEM_PTR) psItem = NULL;
	GEN_TYNAME(SLIST_ITEM_PTR) psNext = NULL;

	/* Create new item */
	psItem = (GEN_TYNAME(SLIST_ITEM_PTR))GDATA_ALLOC(psState,
													 (sizeof(GEN_TYNAME(SLIST_ITEM)) +
													  psList->uElemSize));

	/* Attach new item */
	if (psPos == NULL)
	{
		psPos = psList->psLast;
	}

	psNext = NULL;
	if (psPos != NULL)
	{
		psNext = psPos->psNext;
	}

	psItem->psNext = psNext;
	if (psNext != NULL)
	{
		psNext->psPrev = psItem;
	}
	psItem->psPrev = psPos;
	if (psPos != NULL)
	{
		psPos->psNext = psItem;
	}

	if (psList->psFirst == NULL)
	{
		psList->psFirst = psItem;
		psList->psLast = psItem;
	}
	else if (psList->psLast == psPos)
	{
		psList->psLast = psItem;
	}

	/* Copy data */
	if (ppvData != NULL)
	{
		GEN_NAME(SListSet)(psList, psItem, ppvData);
	}

	/* Done */
	return psItem;
}

FORCE_INLINE
GEN_TYNAME(SLIST_ITEM_PTR) GEN_NAME(SListCons)(PGDATA_SYS_STATE psState,
											   GEN_TYNAME(SLIST_PTR) psList,
											   IMG_PVOID ppvData)
/*****************************************************************************
 FUNCTION	: SListCons

 PURPOSE	: Insert an element at the head of alist

 PARAMETERS	: psState    - Compiler state
              psList     - List
              ppvData    - Pointer to data to copy into the new elemtn to front of list (or NULL)

 RETURNS    : A pointer to the new list item
*****************************************************************************/
{
	return GEN_NAME(SListInsertBefore)(psState, psList, psList->psFirst, ppvData);
}

FORCE_INLINE
GEN_TYNAME(SLIST_ITEM_PTR) GEN_NAME(SListFirst)(GEN_TYNAME(SLIST_PTR) psList)
/*****************************************************************************
 FUNCTION	: SListFirst

 PURPOSE	: Get the item at the head of the list

 PARAMETERS	: psList     - List

 RETURNS    : The item at the head of the list
*****************************************************************************/
{
	if (psList == NULL)
		return NULL;
	return psList->psFirst;
}

FORCE_INLINE
GEN_TYNAME(SLIST_ITEM_PTR) GEN_NAME(SListLast)(GEN_TYNAME(SLIST_PTR) psList)
/*****************************************************************************
 FUNCTION	: SListLast

 PURPOSE	: Get the item at the end of the list

 PARAMETERS	: psList     - List

 RETURNS    : The item at the end of the list
*****************************************************************************/
{
	if (psList == NULL)
		return NULL;
	return psList->psLast;
}

FORCE_INLINE
IMG_PVOID GEN_NAME(SListHead)(GEN_TYNAME(SLIST_PTR) psList)
/*****************************************************************************
 FUNCTION	: SListHead

 PURPOSE	: Get the data at the head of the list

 PARAMETERS	: psList     - List

 RETURNS    : A pointer to the data at the head of the list
*****************************************************************************/
{
	IMG_PVOID pvData = NULL;
	GEN_TYNAME(SLIST_ITEM_PTR) psHead;

	if (psList == NULL)
		return NULL;

	psHead = GEN_NAME(SListFirst)(psList);
	pvData = GEN_NAME(SListGetPtr)(psHead);
	return pvData;
}

FORCE_INLINE
GEN_TYNAME(SLIST_ITEM_PTR) GEN_NAME(SListTail)(GEN_TYNAME(SLIST_PTR) psList)
/*****************************************************************************
 FUNCTION	: SListTail

 PURPOSE	: Get the tail of the list

 PARAMETERS	: psList     - List

 RETURNS    : The tail of the list
*****************************************************************************/
{
	if (psList == NULL)
		return NULL;
	return psList->psFirst->psNext;
}

FORCE_INLINE
IMG_UINT32 GEN_NAME(SListLength)(GEN_TYNAME(SLIST_PTR) psList)
/*****************************************************************************
 FUNCTION   : SListLength

 PURPOSE    : Length of a list

 PARAMETERS : psList    - List

 RETURNS    : Number of elements in psList
*****************************************************************************/
{
	GEN_TYNAME(SLIST_ITEM_PTR) psCurr = NULL;
	IMG_UINT32 uLen;

	uLen = 0;
	for (psCurr = GEN_NAME(SListFirst)(psList);
		 psCurr != NULL;
		 psCurr = psCurr->psNext)
	{
		uLen += 1;
	}

	return uLen;
}

FORCE_INLINE
GEN_TYNAME(SLIST_ITEM_PTR) GEN_NAME(SListItem)(GEN_TYNAME(SLIST_PTR) psList,
											   const IMG_UINT32 uIdx)
/*****************************************************************************
 FUNCTION   : SListItem

 PURPOSE    : Get an item in the list

 PARAMETERS : psList    - List
              uIdx      - Index of item to get

 RETURNS    : Indexed item or NULL
*****************************************************************************/
{
	GEN_TYNAME(SLIST_ITEM_PTR) psCurr = NULL;
	IMG_UINT32 uCtr = 0;

	uCtr = uIdx + 1;
	for (psCurr = GEN_NAME(SListFirst)(psList);
		 psCurr != NULL;
		 psCurr = psCurr->psNext)
	{
		uCtr -= 1;
		if (uCtr == 0)
		{
			return psCurr;
		}
	}

	return NULL;
}
#endif /* USC_DATA_DEF */

/**
 * Pairs
 **/
typedef struct GEN_TYNAME(_0USC_PAIR_)
{
	IMG_VOID* pvLeft;
	IMG_VOID* pvRight;
} GEN_TYNAME(PAIR), *GEN_TYNAME(PAIR_PTR);

FORCE_INLINE
GEN_TYNAME(PAIR_PTR) GEN_NAME(PairMake)(PGDATA_SYS_STATE psState,
										IMG_PVOID pvLeft,
										IMG_PVOID pvRight)
/*****************************************************************************
 FUNCTION	: PairMake

 PURPOSE	: Make a pair

 PARAMETERS	: psState    - Compiler state
              psLeft     - Data
              psRight    - Data

 RETURNS	: The new pair
*****************************************************************************/
{
	GEN_TYNAME(PAIR_PTR) psNewPair;
	psNewPair = (GEN_TYNAME(PAIR_PTR))GDATA_ALLOC(psState, sizeof(GEN_TYNAME(PAIR)));
	psNewPair->pvLeft = pvLeft;
	psNewPair->pvRight = pvRight;

	return psNewPair;
}

FORCE_INLINE
IMG_VOID GEN_NAME(PairDelete)(PGDATA_SYS_STATE psState,
							  GEN_TYNAME(PAIR_PTR) psPair)
/*****************************************************************************
 FUNCTION	: PairDelete

 PURPOSE	: Delete a pair

 PARAMETERS	: psState    - Compiler state
              psPair     - Pair to delete

 RETURNS	: Nothing
*****************************************************************************/
{
	GDATA_FREE(psState, psPair);
}

/**
 * Stacks
 **/

struct GEN_TYNAME(_0STACK_BASE_);
typedef struct GEN_TYNAME(_0STACK_BASE_) GEN_TYNAME(STACK);
typedef struct GEN_TYNAME(_0STACK_BASE_) *GEN_TYNAME(STACK_PTR);
					 
GEN_TYNAME(STACK_PTR) GEN_NAME(StackMake)(PGDATA_SYS_STATE psState,
										  IMG_UINT32 uElemSize);
IMG_VOID GEN_NAME(StackDelete)(PGDATA_SYS_STATE psState,
							   GEN_TYNAME(STACK_PTR) psStack);
IMG_PVOID GEN_NAME(StackPush)(PGDATA_SYS_STATE psState,
							  GEN_TYNAME(STACK_PTR) psStack,
							  IMG_PVOID pvData);
IMG_PVOID GEN_NAME(StackTop)(GEN_TYNAME(STACK_PTR) psStack);
IMG_BOOL GEN_NAME(StackPop)(PGDATA_SYS_STATE psState,
							GEN_TYNAME(STACK_PTR) psStack);
IMG_BOOL GEN_NAME(StackEmpty)(GEN_TYNAME(STACK_PTR) psStack);


/**
 * Trees
 **/

typedef struct GEN_TYNAME(_0TREE_) GEN_TYNAME(TREE), *GEN_TYNAME(TREE_PTR);

GEN_TYNAME(TREE_PTR) GEN_NAME(TreeMake)(PGDATA_SYS_STATE psState,
										 IMG_UINT32 uElemSize,
										 GEN_TYNAME(COMPARE_FN) pfnCmp);
IMG_VOID GEN_NAME(TreeDelete)(PGDATA_SYS_STATE psState,
							  GEN_TYNAME(TREE_PTR) psTree);
IMG_VOID GEN_NAME(TreeDeleteAll)(PGDATA_SYS_STATE psState,
								 GEN_TYNAME(TREE_PTR) psTree,
								 GEN_TYNAME(ITERATOR_FN) pfnIter,
								 IMG_PVOID pvIterData);
IMG_PVOID GEN_NAME(TreeGetPtr)(GEN_TYNAME(TREE_PTR) psTree,
							   IMG_PVOID pvKey);
IMG_PVOID GEN_NAME(TreeAdd)(PGDATA_SYS_STATE psState,
						    GEN_TYNAME(TREE_PTR) psTree,
						    IMG_PVOID pvData);
IMG_VOID GEN_NAME(TreeRemove)(PGDATA_SYS_STATE psState,
							  GEN_TYNAME(TREE_PTR) psTree,
							  IMG_PVOID pvKey,
							  GEN_TYNAME(ITERATOR_FN) pfnIter,
							  IMG_PVOID pvIterData);
IMG_VOID GEN_NAME(TreeIterInOrder)(PGDATA_SYS_STATE psState,
								   GEN_TYNAME(TREE_PTR) psTree,
								   GEN_TYNAME(ITERATOR_FN) pfnIter,
								   IMG_PVOID pvIterData);
IMG_VOID GEN_NAME(TreeIterPreOrder)(PGDATA_SYS_STATE psState,
									GEN_TYNAME(TREE_PTR) psTree,
									GEN_TYNAME(ITERATOR_FN) pfnIter,
									IMG_PVOID pvIterData);

#ifndef USC_DATA_DEF
/***
 * Vectors
 ***/

typedef struct GEN_TYNAME(_0VECTOR_) GEN_TYNAME(VECTOR), *GEN_TYNAME(VECTOR_PTR);

#if 0
/* Dummy structure to help emacs indentation mode */
struct _dummy_
{
} _x;
#endif


GEN_TYNAME(VECTOR_PTR) GEN_NAME(VectorInit)(PGDATA_SYS_STATE psState, 
											GEN_TYNAME(VECTOR_PTR) psVector,
											IMG_UINT32 uElemSize,
											IMG_VOID* pvDefault);
IMG_VOID GEN_NAME(VectorClear)(PGDATA_SYS_STATE psState,
							   GEN_TYNAME(VECTOR_PTR) psVector);

GEN_TYNAME(VECTOR_PTR) GEN_NAME(VectorMake)(PGDATA_SYS_STATE psState,
											IMG_UINT32 uElemSize,
											IMG_VOID* pvDefault);
IMG_VOID GEN_NAME(VectorDelete)(PGDATA_SYS_STATE psState,
								GEN_TYNAME(VECTOR_PTR) psVector);
IMG_UINT32 GEN_NAME(VectorLength)(PGDATA_SYS_STATE psState,
								  GEN_TYNAME(VECTOR_PTR) psVector);
IMG_VOID GEN_NAME(VectorGet)(GEN_TYNAME(VECTOR_PTR) psVector,
							 IMG_UINT32 uIdx,
							 IMG_VOID* pvData);
IMG_VOID GEN_NAME(VectorSet)(PGDATA_SYS_STATE psState,
							 GEN_TYNAME(VECTOR_PTR) psVector,
							 IMG_UINT32 uIdx,
							 IMG_PVOID pvData);

IMG_VOID GEN_NAME(VectorIter)(PGDATA_SYS_STATE psState,
							  GEN_TYNAME(VECTOR_PTR) psVector,
							  GEN_TYNAME(ITERATOR_FN) pfnIter,
							  IMG_PVOID pvIterData);

#endif /* USC_DATA_DEF */

#if defined(SUPPORT_ICODE_SERIALIZATION)
/*
  ITERATOR_EX_FN: Iterator function.
  Apply fn(d, e, u) to all elements of a structure, where d is data to be passed
  to the function and e the elements of the structure. u describes data size
 */
typedef IMG_VOID (*GEN_TYNAME(ITERATOR_EX_FN))(IMG_VOID* pvData, IMG_VOID* pvElement, IMG_UINT32 uSize);

IMG_INTERNAL
IMG_VOID GEN_NAME(TreePostOrderTraverseRecursive)(PGDATA_SYS_STATE psState,
				  								  GEN_TYNAME(TREE_PTR) *ppsTree, 
												  GEN_TYNAME(ITERATOR_EX_FN) pfnSerializeUnserialize,
												  GEN_TYNAME(ITERATOR_FN) pfnTranslate,
												  IMG_PVOID pvData);
#endif
#endif /* USCGENDATA_H */
/*****************************************************************************
 End of file
*****************************************************************************/
