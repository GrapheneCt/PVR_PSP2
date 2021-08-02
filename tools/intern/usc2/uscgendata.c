/*****************************************************************************
 Name			: uscgendata.c

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

 Description 	: Generic data types implementation

 Program Type	: 32-bit DLL

 Version	 	: $Revision: 1.13 $

 Modifications	:

 $Log: uscgendata.c $
*****************************************************************************/

#include "uscgendata.h"

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
 * Bitvector functions 
 ***/

/***
 * Simple lists
 ***/

/**
 * Pairs
 **/

/***
 * Stacks
 ***/

/* Number of elements in a stack chunk */
#define GEN_STACK_CHUNK_SIZE (8)

/*
  STACK: Structure containing an list of chunks and the size of
  each element in the stack.
  STACK.apsChunk is made up of GEN_STACK_CHUNK_SIZE elements
  and a pointer to the next chunk.

  [<-- pointer:(sizeof(IMG_VOID*) * 1 -->|<-- chunk:(GEN_STACK_CHUNK_SIZE * uElemSize) -->]

*/
struct GEN_TYNAME(_0STACK_BASE_)
{
	IMG_UINT32 uElemSize;   // Size in bytes of a stack element
	IMG_UINT32 uIdx;        // Current index of top of stack (0 iff stack is empty)
	IMG_VOID* *apvChunk;    // Pointer to first array of elements
};

static
IMG_PVOID GEN_NAME(StackElementPtr)(GEN_TYNAME(STACK_PTR) psStack,
									const IMG_UINT32 uElem)
/*****************************************************************************
 FUNCTION	: StackElementPtr

 PURPOSE	: Get a pointer to an element on the stack

 INPUT     	: psStack    - Stack
              uElem      - Element to get

 RETURNS	: Pointer to the element
*****************************************************************************/
{
	IMG_PVOID* ppvElemArray = &(psStack->apvChunk[1]);
	IMG_UINT32 uElemSize = psStack->uElemSize;
	IMG_CHAR* psTmp = (IMG_CHAR*)ppvElemArray;

	psTmp += (uElem * uElemSize);
	return (IMG_PVOID)psTmp;
}

IMG_INTERNAL
IMG_BOOL GEN_NAME(StackEmpty)(GEN_TYNAME(STACK_PTR) psStack)
/*****************************************************************************
 FUNCTION	: StackEmpty

 PURPOSE	: Test whether a gen-stack is empty

 INPUT     	: psStack    - Stack to test

 RETURNS	: IMG_TRUE iff the stack is empty
*****************************************************************************/
{
	const IMG_UINT32 uTop = psStack->uIdx;

	if (uTop == 0)
		return IMG_TRUE;

	return IMG_FALSE;
}

IMG_INTERNAL
GEN_TYNAME(STACK_PTR) GEN_NAME(StackMake)(PGDATA_SYS_STATE psState,
										  IMG_UINT32 uElemSize)
/*****************************************************************************
 FUNCTION	: StackMake

 PURPOSE	: Make an empty stack

 INPUT     	: psState     - Compiler state
              uElemSize   - Size in bytes of a stack element

 RETURNS	: The new stack
*****************************************************************************/
{
	GEN_TYNAME(STACK_PTR) psStack;

	psStack = GDATA_ALLOC(psState, sizeof(*psStack));
	psStack->apvChunk = GDATA_ALLOC(psState,
									(sizeof(IMG_VOID*) +
									 (uElemSize * GEN_STACK_CHUNK_SIZE)));

	psStack->apvChunk[0] = NULL;
	psStack->uIdx = 0;
	psStack->uElemSize = uElemSize;

	return psStack;
}

IMG_INTERNAL
IMG_VOID GEN_NAME(StackDelete)(PGDATA_SYS_STATE psState,
							   GEN_TYNAME(STACK_PTR) psStack)
/*****************************************************************************
 FUNCTION	: StackDelete

 PURPOSE	: Delete a stack

 INPUT     	: psState     - Compiler state
              psStack     - Stack to delete

 NOTES      : Does not delete the contents of the stack

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_PVOID* psCurr;
	IMG_PVOID* psNext;

	/* Delete all chunks */
	for (psCurr = psStack->apvChunk;
		 psCurr != NULL;
		 psCurr = psNext)
	{
		psNext = psCurr[0];
		GDATA_FREE(psState, psCurr);
	}

	/* Free the stack itself.*/
	GDATA_FREE(psState, psStack);
}

IMG_INTERNAL
IMG_PVOID GEN_NAME(StackPush)(PGDATA_SYS_STATE psState,
							  GEN_TYNAME(STACK_PTR) psStack,
							  IMG_PVOID pvData)
/*****************************************************************************
 FUNCTION	: StackPush

 PURPOSE	: Push a gen onto the front of a gen stack

 INPUT     	: psState     - Compiler state
              psStack     - Stack
              pvData      - Pointer to data to push on to stack

 RETURNS	: Pointer to new top element of stack
*****************************************************************************/
{
	IMG_UINT32 uTop;
	IMG_PVOID* psChunk;
	IMG_VOID* pvRet;
	const IMG_UINT32 uElemSize = psStack->uElemSize;

	ASSERT(psStack != NULL);
	uTop = psStack->uIdx;
	psChunk = psStack->apvChunk;

	uTop += 1;
	if (!(uTop <= GEN_STACK_CHUNK_SIZE))
	{
		/* Need to create a new chunk */
		IMG_PVOID* pvNewChunk;
		pvNewChunk = GDATA_ALLOC(psState,
								 sizeof(IMG_PVOID*) +
								 (uElemSize * GEN_STACK_CHUNK_SIZE));

		/* Make the new chunk the head of the list */
		pvNewChunk[0] = psChunk;
		psStack->apvChunk = pvNewChunk;
		psStack->uIdx = 0;

		uTop = 1;
	}

	/* Calculate the pointer to the element */
	pvRet = GEN_NAME(StackElementPtr)(psStack, psStack->uIdx);
	ASSERT(pvRet != NULL);

	/* Store the data */
	if (pvData != NULL)
	{
		memcpy(pvRet, pvData, uElemSize);
	}

	psStack->uIdx = uTop;

	/* Return the element */
	return pvRet;
}

IMG_INTERNAL
IMG_PVOID GEN_NAME(StackTop)(GEN_TYNAME(STACK_PTR) psStack)
/*****************************************************************************
 FUNCTION	: StackTop

 PURPOSE	: Gets the element at the top of a stack

 INPUT     	: psState     - Compiler state
              psStack     - Stack

 RETURNS	: A pointer to the element at the top of the stack or NULL if
              stack is empty.
*****************************************************************************/
{
	if (GEN_NAME(StackEmpty)(psStack))
	{
		return NULL;
	}
	else
	{
		IMG_PVOID pvTop;
		IMG_UINT32 uTop = psStack->uIdx - 1;

		/* Calculate the pointer to the element */
		pvTop = GEN_NAME(StackElementPtr)(psStack, uTop);

		return pvTop;
	}
}

IMG_INTERNAL
IMG_BOOL GEN_NAME(StackPop)(PGDATA_SYS_STATE psState,
							GEN_TYNAME(STACK_PTR) psStack)
/*****************************************************************************
 FUNCTION	: StackPop

 PURPOSE	: Cut and delete the top of the stack

 INPUT     	: psState     - Compiler state
              psPoint     - Point to cut (the new head of the right-hand stack)

 NOTES      : Doesn't delete the data item stored in the stack element

 RETURNS	: IMG_TRUE if the stack had an element, IMG_FALSE if the stack was empty
*****************************************************************************/
{
	IMG_UINT32 uTop;
	IMG_PVOID pvNextChunk;

	ASSERT(psStack != NULL);

	if (GEN_NAME(StackEmpty)(psStack))
	{
		return IMG_FALSE;
	}

	uTop = psStack->uIdx;
	ASSERT(uTop > 0);
	pvNextChunk = psStack->apvChunk[0];

	/* Pop the data */
	uTop -= 1;
	if (uTop == 0 && pvNextChunk != NULL)
	{
		/* Drop the chunk */
		GDATA_FREE(psState, psStack->apvChunk);
		psStack->apvChunk = pvNextChunk;
		uTop = GEN_STACK_CHUNK_SIZE;
	}
	/* Update the index */
	psStack->uIdx = uTop;

	return IMG_TRUE;
}

/***
 * Trees
 ***/

/* Type Definitions */
typedef struct GEN_TYNAME(_0BASETREE_)
{
	struct GEN_TYNAME(_0BASETREE_)* psLeft;
	struct GEN_TYNAME(_0BASETREE_)* psRight;
	struct GEN_TYNAME(_0BASETREE_)* psParent;
	IMG_BOOL bRed;

	IMG_BYTE *pvElem;
} GEN_TYNAME(BASETREE), *GEN_TYNAME(BASETREE_PTR);

					  struct GEN_TYNAME(_0TREE_)
{
	/* The actual tree */
	GEN_TYNAME(BASETREE_PTR) psBase;

	/* The element size */
	IMG_UINT32 uElemSize;

	/* The ordering function */
	GEN_TYNAME(COMPARE_FN) pfnCompare;
};

/* Function Definitions */

FORCE_INLINE
IMG_VOID BaseTreeSet(GEN_TYNAME(BASETREE_PTR) psBaseTree,
					 IMG_UINT32 uElemSize,
					 IMG_PVOID *ppvData)
/*****************************************************************************
 FUNCTION	: BaseTreeSet

 PURPOSE	: Set the data in a list item

 INPUT     	: psBaseTree     - Tree element
              uElemSize      - Size in bytes of the data
              ppvData        - Pointer to data to set

 RETURNS    : Nothing

 NOTES      : if psItem is NULL, the head of the list is set.
*****************************************************************************/
{
	memcpy(&(psBaseTree->pvElem), ppvData, uElemSize);
}

FORCE_INLINE
IMG_PVOID BaseTreeGetPtr(GEN_TYNAME(BASETREE_PTR) psBaseTree)
/*****************************************************************************
 FUNCTION	: BaseTreeGetPtr

 PURPOSE	: Get a pointer to the data in a list item

 INPUT     	: psBaseTree    - Tree element

 RETURNS    : A pointer to the data in the list item
*****************************************************************************/
{
	IMG_BYTE** ppvPtr;
	if (psBaseTree == NULL)
	{
		return NULL;
	}

	ppvPtr = (IMG_BYTE**)(&(psBaseTree->pvElem));
	return (IMG_PVOID)ppvPtr;
}

FORCE_INLINE
IMG_VOID BaseTreeGet(GEN_TYNAME(BASETREE_PTR) psBaseTree,
					 IMG_UINT32 uElemSize,
					 IMG_VOID* ppvData)
/*****************************************************************************
 FUNCTION	: BaseTreeGet

 PURPOSE	: Get the data in a list item

 INPUT     	: psBaseTree     - List
              uElemSize      - Element size

 OUTPUT     : ppvData    - Pointer to location to copy data into

 RETURNS    : Nothing
*****************************************************************************/
{
	IMG_PVOID ppvPtr;

	ppvPtr = BaseTreeGetPtr(psBaseTree);
	memcpy(ppvData, ppvPtr, uElemSize);
}

static
GEN_TYNAME(BASETREE_PTR) BaseTreeMake(PGDATA_SYS_STATE psState,
									  IMG_UINT32 uElemSize,
									  GEN_TYNAME(BASETREE_PTR) psLeft,
									  GEN_TYNAME(BASETREE_PTR) psRight)
/*****************************************************************************
 FUNCTION	: BaseTreeMake

 PURPOSE	: Make a basetree element from a data item

 INPUT     	: psState       - Compiler state
              uElemSize     - Size of data to be stored
              psLeft        - Left branch
              psRight       - Right branch

 RETURNS	: The new basetree element
*****************************************************************************/
{
	const IMG_UINT32 uBaseSize = (sizeof(GEN_TYNAME(BASETREE)) + uElemSize);
	GEN_TYNAME(BASETREE_PTR) psNewBaseTree;

	psNewBaseTree = GDATA_ALLOC(psState, uBaseSize);
	memset(psNewBaseTree, 0, uBaseSize);

	psNewBaseTree->psLeft = psLeft;
	psNewBaseTree->psRight = psRight;
	psNewBaseTree->bRed = IMG_TRUE;

	return psNewBaseTree;
}

static
IMG_VOID BaseTreeDetach(PGDATA_SYS_STATE psState,
						GEN_TYNAME(BASETREE_PTR) psBaseTree)
/*****************************************************************************
 FUNCTION	: BaseTreeDetach

 PURPOSE	: Detach a basetree element from the basetree.

 INPUT     	: psState       - Compiler state
              psBaseTree    - BaseTree to detach

 RETURNS	: Nothing
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	if (psBaseTree == NULL)
		return;

	psBaseTree->psLeft = NULL;
	psBaseTree->psRight = NULL;
	psBaseTree->psParent = NULL;
}

static
IMG_VOID BaseTreeDelete(PGDATA_SYS_STATE psState,
						GEN_TYNAME(BASETREE_PTR) psBaseTree,
						GEN_TYNAME(ITERATOR_FN) pfnIter,
						IMG_PVOID pvIterData)
/*****************************************************************************
 FUNCTION	: BaseTreeDelete

 PURPOSE	: Delete a basetree

 INPUT     	: psState       - Compiler state
              psBaseTree    - BaseTree to delete
              pfnIter       - Iterator to call on each element
              pvIterData    - Data to pass to the iterator function

 RETURNS	: Nothing
*****************************************************************************/
{
	GEN_TYNAME(STACK_PTR) psStack;

	if (psBaseTree == NULL)
	{
		return;
	}

	psStack = GEN_NAME(StackMake)(psState, sizeof(GEN_TYNAME(BASETREE_PTR)));
	GEN_NAME(StackPush)(psState, psStack, &psBaseTree);

	while (!GEN_NAME(StackEmpty)(psStack))
	{
		GEN_TYNAME(BASETREE_PTR)* ppsCurrPtr;
		GEN_TYNAME(BASETREE_PTR) psCurr;

		ppsCurrPtr = (GEN_TYNAME(BASETREE_PTR)*)GEN_NAME(StackTop)(psStack);
		psCurr = *ppsCurrPtr;

		GEN_NAME(StackPop)(psState, psStack);

		if (psCurr->psLeft != NULL)
		{
			GEN_NAME(StackPush)(psState, psStack, &(psCurr->psLeft));
		}
		if (psCurr->psRight != NULL)
		{
			GEN_NAME(StackPush)(psState, psStack, &(psCurr->psRight));
		}

		/* Apply the iterator to the data */
		if (pfnIter != NULL)
		{
			IMG_PVOID ppvData = (IMG_PVOID)BaseTreeGetPtr(psCurr);
			pfnIter(pvIterData, ppvData);
		}

		/* Delete the element */
		BaseTreeDetach(psState, psCurr);
		GDATA_FREE(psState, psCurr);
	}
	GEN_NAME(StackDelete)(psState, psStack);
}

IMG_INTERNAL
GEN_TYNAME(TREE_PTR) GEN_NAME(TreeMake)(PGDATA_SYS_STATE psState,
										IMG_UINT32 uElemSize,
										GEN_TYNAME(COMPARE_FN) pfnCmp)
/*****************************************************************************
 FUNCTION	: TreeMake

 PURPOSE	: Allocate and initialise a tree

 INPUT     	: psState       - Compiler state
              uElemSize     - Element size in bytes
              pfnCmp        - Comparison function to use as an ordering

 RETURNS	: The newly allocated, empty tree
*****************************************************************************/
{
	GEN_TYNAME(TREE_PTR) psTree;

	ASSERT(pfnCmp != NULL);

	psTree = GDATA_ALLOC(psState, sizeof(GEN_TYNAME(TREE)));

	psTree->psBase = NULL;
	psTree->uElemSize = uElemSize;
	psTree->pfnCompare = pfnCmp;

	return psTree;
}

IMG_INTERNAL
IMG_VOID GEN_NAME(TreeDelete)(PGDATA_SYS_STATE psState,
							  GEN_TYNAME(TREE_PTR) psTree)
/*****************************************************************************
 FUNCTION	: TreeDelete

 PURPOSE	: Delete a tree

 INPUT     	: psState       - Compiler state
              psTree        - Tree to delete

 RETURNS	: Nothing
*****************************************************************************/
{
	if (psTree == NULL)
		return;

	BaseTreeDelete(psState, psTree->psBase, NULL, NULL);
	GDATA_FREE(psState, psTree);
}

IMG_INTERNAL
IMG_VOID GEN_NAME(TreeDeleteAll)(PGDATA_SYS_STATE psState,
								 GEN_TYNAME(TREE_PTR) psTree,
								 GEN_TYNAME(ITERATOR_FN) pfnIter,
								 IMG_PVOID pvIterData)
/*****************************************************************************
 FUNCTION	: TreeDeleteAll

 PURPOSE	: Delete a tree

 INPUT     	: psState     - Compiler state
              psTree      - Tree to delete
              pfnIter     - Iterator to call on each element
              pvIterData  - Data to pass to the iterator function

 NOTES      : pfnIter is intended to allow data in the tree to be deleted
              so that deleting the tree and its contents can be done in one
              pass.

 RETURNS	: Nothing
*****************************************************************************/
{
	if (psTree == NULL)
		return;

	BaseTreeDelete(psState, psTree->psBase, pfnIter, pvIterData);
	GDATA_FREE(psState, psTree);
}

static
GEN_TYNAME(BASETREE_PTR) TreeBaseGetPtr(GEN_TYNAME(TREE_PTR) psTree, IMG_PVOID pvKey)
/*****************************************************************************
 FUNCTION	: TreeBaseGetPtr

 PURPOSE	: Return a pointer to the node associated with a key

 INPUT     	: psTree     - Tree to search
              pvKey      - Key to find

 RETURNS	: A pointer to the node associated with key pvKey or NULL if not found
*****************************************************************************/
{
	GEN_TYNAME(BASETREE_PTR) psCurr;
	GEN_TYNAME(BASETREE_PTR) psNext;
	GEN_TYNAME(COMPARE_FN) pfnCmp;

	/* Check for early exit */
	if (psTree == NULL)
		return NULL;
	if (psTree->psBase == NULL)
		return NULL;

	pfnCmp = psTree->pfnCompare;
	psNext = NULL;
	for (psCurr = psTree->psBase;
		 psCurr != NULL;
		 psCurr = psNext)
	{
		IMG_PVOID *ppvData = BaseTreeGetPtr(psCurr);
		const IMG_INT iCmp = pfnCmp(pvKey, ppvData);

		if (iCmp == 0)
		{
			/* Found the key */
			return psCurr;
		}
		if (iCmp < 0)
		{
			/* Go left */
			psNext = psCurr->psLeft;
		}
		if (iCmp > 0)
		{
			/* Go right */
			psNext = psCurr->psRight;
		}
	}

	/* Not found */
	return NULL;
}

IMG_INTERNAL
IMG_PVOID GEN_NAME(TreeGetPtr)(GEN_TYNAME(TREE_PTR) psTree, IMG_PVOID pvKey)
/*****************************************************************************
 FUNCTION	: TreeGetPtr

 PURPOSE	: Return a pointer to the data associated with a key

 INPUT     	: psTree     - Tree to search
              pvKey      - Key to find

 RETURNS	: A pointer to the data associated with key pvKey or NULL if not found
*****************************************************************************/
{
	GEN_TYNAME(BASETREE_PTR) psCurr;

	psCurr = TreeBaseGetPtr(psTree, pvKey);
	if (psCurr == NULL)
	{
		/* The key wasn't present in the tree. */
		return NULL;
	}
	return BaseTreeGetPtr(psCurr);
}

static
IMG_VOID TreeReplaceParent(PGDATA_SYS_STATE			psState,
						   GEN_TYNAME(TREE_PTR)		psTree,
						   GEN_TYNAME(BASETREE_PTR)	psNodeToBeReplaced,
						   GEN_TYNAME(BASETREE_PTR)	psReplacement)
/*****************************************************************************
 FUNCTION	: TreeReplaceParent

 PURPOSE	: Update child pointers in the parent node when replacing one
			  node by another.

 INPUT     	: psState    - Compiler state
              psTree     - Tree to which the nodes belong.
			  psNodeToBeReplaced
			  psReplacement

 RETURNS	: Nothing
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);

	if (psNodeToBeReplaced->psParent == NULL)
	{
		psTree->psBase = psReplacement;
	}
	else if (psNodeToBeReplaced == psNodeToBeReplaced->psParent->psLeft)
	{
		psNodeToBeReplaced->psParent->psLeft = psReplacement;
	}
	else
	{
		ASSERT(psNodeToBeReplaced->psParent->psRight == psNodeToBeReplaced);
		psNodeToBeReplaced->psParent->psRight = psReplacement;
	}

	if (psReplacement != NULL)
	{
		psReplacement->psParent = psNodeToBeReplaced->psParent;
	}	
}

static
IMG_VOID TreeRotateLeft(PGDATA_SYS_STATE			psState,
						GEN_TYNAME(TREE_PTR)		psTree,
						GEN_TYNAME(BASETREE_PTR)	psA)
/*****************************************************************************
 FUNCTION	: TreeRotateLeft

 PURPOSE	: Rotate a tree by one place to the left

 INPUT     	: psState    - Compiler state
              psNode     - Tree to rotate
			  psA		 - Node to rotate.

 NOTES      : RotateLeft(Top(Left, Right(A, B))) = Right(Top(Left, A), B)

 RETURNS	: Nothing.
*****************************************************************************/
{
	GEN_TYNAME(BASETREE_PTR) psB;

	/*
			 A								B
		    / \                            / \
		   /   \                          /   \
		  /     \                        /     \
		 X      B		----->          A       Z
			   / \                     / \
		      /   \                   /   \
			 Y     Z                 X     Y
	*/

	psB = psA->psRight;
	if (psB != IMG_NULL)
	{
		/*
			Copy the left child of B to the right child of A.
		*/
		psA->psRight = psB->psLeft;
		if (psA->psRight != NULL)
		{
			psB->psLeft->psParent = psA;
		}
	}
	else
	{
		psA->psRight = IMG_NULL;
	}
	
	/*
		Replace A by B in it's parent pointer.
	*/
	TreeReplaceParent(psState, psTree, psA, psB);

	/*
		Set A as the left child of B.
	*/
	if (psB != IMG_NULL)
	{
		psB->psLeft = psA;
	}
	psA->psParent = psB;
}

static
IMG_VOID TreeRotateRight(PGDATA_SYS_STATE			psState,
						 GEN_TYNAME(TREE_PTR)		psTree,
						 GEN_TYNAME(BASETREE_PTR)	psA)
/*****************************************************************************
 FUNCTION	: TreeRotateRight

 PURPOSE	: Rotate a tree by one place to the right

 INPUT     	: psState    - Compiler state
              psTree	 - Tree to which the nodes belong.
			  psA		 - Point to rotate at.

 NOTES      : RotateRight(Top(Left(A, B), Right))) = Left(A, Top(B, Right))

 RETURNS	: Nothing.
*****************************************************************************/
{
	GEN_TYNAME(BASETREE_PTR) psB;

	/*
				 A								B
			    / \                            / \
			   /   \                          /   \
			  /     \                        /     \
			 B		 Z		----->          X       A
			/ \									   / \
		   /   \								  /   \
		  X     Y								 Y     Z
	*/

	psB = psA->psLeft;

	/*
		Copy the right child of B to the left child of A.
	*/
	if (psB != IMG_NULL)
	{
		psA->psLeft = psB->psRight;
		if (psA->psLeft != NULL)
		{
			psA->psLeft->psParent = psA;
		}
	}
	else
	{
		psA->psLeft = IMG_NULL;
	}


	/*
		Replace A by B in it's parent pointer.
	*/
	TreeReplaceParent(psState, psTree, psA, psB);

	/*
		Set A as the right child of B.
	*/
	if (psB != IMG_NULL)
	{
		psB->psRight = psA;
	}
	psA->psParent = psB;
}

static
GEN_TYNAME(BASETREE_PTR) TreeMinimum(GEN_TYNAME(BASETREE_PTR) psSubTree)
/*****************************************************************************
 FUNCTION	: TreeMinimum

 PURPOSE	: Returns the node with the minimum key value in a subtree

 INPUT     	: psSubTree		- Subtree to get the minimum for.
              
 RETURNS	: A pointer to the minimum node.
*****************************************************************************/
{
	while (psSubTree->psLeft != NULL)
	{
		psSubTree = psSubTree->psLeft;
	}
	return psSubTree;
}

static
GEN_TYNAME(BASETREE_PTR) TreeSuccessor(GEN_TYNAME(BASETREE_PTR) psX)
/*****************************************************************************
 FUNCTION	: TreeSuccessor

 PURPOSE	: Returns the node with the next key value after a specified node.

 INPUT     	: psX		- Node to return the successor of.
              
 RETURNS	: A pointer to the successor.
*****************************************************************************/
{
	GEN_TYNAME(BASETREE_PTR)	psY;

	if (psX->psRight != NULL)
	{
		return TreeMinimum(psX->psRight);
	}
	psY = psX->psParent;
	while (psY != NULL && psX == psY->psRight)
	{
		psX = psY;
		psY = psY->psParent;
	}
	return psY;
}

static
IMG_VOID BaseTreeValid(PGDATA_SYS_STATE psState,
					   GEN_TYNAME(BASETREE_PTR) psTree,
					   GEN_TYNAME(COMPARE_FN) pfCmp,
					   GEN_TYNAME(BASETREE_PTR)* ppsMin,
					   GEN_TYNAME(BASETREE_PTR)* ppsMax,
					   IMG_PUINT32 puNumBlackNodes)
/*****************************************************************************
 FUNCTION	: BaseTreeValid

 PURPOSE	: Checks if a subtree is valid - raising an assert if it's invalid.

 INPUT     	: psState	- Compiler state.
			  psTree	- Subtree to check.
			  pfCmp		- Comparison function for the tree.
			  ppsMin	- Returns the node with the smallest key in the subtree.
			  ppsMax	- Returns the node with the largest key in the subtree.
			  puNumBlackNodes
						- Returns the number of black nodes on any path from the
						root of the subtree to a leaf.
              
 RETURNS	: Nothing
*****************************************************************************/
{
	GEN_TYNAME(BASETREE_PTR)	psMin, psMax;
	IMG_UINT32					uLeftNumBlackNodes;
	IMG_UINT32					uRightNumBlackNodes;
	IMG_UINT32					uNumBlackNodes;

	if (psTree->psLeft != NULL)
	{
		GEN_TYNAME(BASETREE_PTR)	psLeftMin, psLeftMax;

		/*
			Check the parent pointer in the left child.
		*/
		ASSERT(psTree->psLeft->psParent == psTree);

		/*
			Check the left subtree.
		*/
		BaseTreeValid(psState, psTree->psLeft, pfCmp, &psLeftMin, &psLeftMax, &uLeftNumBlackNodes);

		/*
			Check every key in the left subtree is less than the key of the root.
		*/
		ASSERT(pfCmp(BaseTreeGetPtr(psLeftMax), BaseTreeGetPtr(psTree)) < 0);
		psMin = psLeftMin;
	}
	else
	{
		psMin = psTree;
		uLeftNumBlackNodes = 0;
	}

	if (psTree->psRight != NULL)
	{
		GEN_TYNAME(BASETREE_PTR)	psRightMin, psRightMax;

		/*
			Check the parent pointer in the right child.
		*/
		ASSERT(psTree->psRight->psParent == psTree);

		/*
			Check the right subtree.
		*/
		BaseTreeValid(psState, psTree->psRight, pfCmp, &psRightMin, &psRightMax, &uRightNumBlackNodes);

		/*
			Check every key in the right subtree is greater than the key of the root.
		*/
		ASSERT(pfCmp(BaseTreeGetPtr(psRightMin), BaseTreeGetPtr(psTree)) > 0);
		psMax = psRightMax;
	}
	else
	{
		psMax = psTree;
		uRightNumBlackNodes = 0;
	}

	/*
		Check a red node has two black children.
	*/
	if (psTree->bRed)
	{
		ASSERT(psTree->psLeft == IMG_NULL || !psTree->psLeft->bRed);
		ASSERT(psTree->psRight == IMG_NULL || !psTree->psRight->bRed);
	}

	/*
		Check the black height of both subtrees is equal.
	*/
	ASSERT(uLeftNumBlackNodes == uRightNumBlackNodes);

	/*
		Calculate the black height of this node.
	*/
	uNumBlackNodes = uLeftNumBlackNodes;
	if (!psTree->bRed)
	{
		uNumBlackNodes++;
	}

	/*
		Return information about the subtree to the caller.
	*/
	if (ppsMin != NULL)
	{
		*ppsMin = psMin;
	}
	if (ppsMax != NULL)
	{
		*ppsMax = psMax;
	}
	if (puNumBlackNodes != NULL)
	{
		*puNumBlackNodes = uNumBlackNodes;
	}
}

IMG_INTERNAL
IMG_VOID GEN_NAME(TreeValid)(PGDATA_SYS_STATE psState,
							 GEN_TYNAME(TREE_PTR) psTree)
/*****************************************************************************
 FUNCTION	: TreeValid

 PURPOSE	: Checks if a tree is valid - raising an assert if it's invalid.

 INPUT     	: psState	- Compiler state.
			  psTree	- Tree to check.
              
 RETURNS	: Nothing
*****************************************************************************/
{
	if (psTree->psBase != NULL)
	{
		ASSERT(psTree->psBase->psParent == NULL);
		ASSERT(!psTree->psBase->bRed);
		BaseTreeValid(psState, psTree->psBase, psTree->pfnCompare, NULL, NULL, NULL);
	}
}

static 
IMG_BOOL TreeIsRed(GEN_TYNAME(BASETREE_PTR) psNode)
/*****************************************************************************
 FUNCTION	: TreeIsRed

 PURPOSE	: Returns whether a node is red or black (empty nodes are
			  implicitly black).

 INPUT     	: psNode	- Node to check.
              
 RETURNS	: TRUE if the node is red.
*****************************************************************************/
{
	return psNode != NULL && psNode->bRed;
}

static
IMG_VOID TreeDelete_Fix(PGDATA_SYS_STATE			psState,
						GEN_TYNAME(TREE_PTR)		psTree, 
						GEN_TYNAME(BASETREE_PTR)	psX, 
						GEN_TYNAME(BASETREE_PTR)	psXParent)
/*****************************************************************************
 FUNCTION	: TreeDelete_Fix

 PURPOSE	: Rebalance a tree after deleting a node.

 INPUT     	: psState	- Compiler state.
			  psTree	- Tree to fix.
			  psX		- Invalid node (could be NULL).
			  psXParent	- Parent of the invalid node.
              
 RETURNS	: Nothing
*****************************************************************************/
{
	while (psX != psTree->psBase && !TreeIsRed(psX))
	{
		if (psX == psXParent->psLeft)
		{
			GEN_TYNAME(BASETREE_PTR)	psW;

			psW = psXParent->psRight;
			if (TreeIsRed(psW))
			{
				psW->bRed = IMG_FALSE;
				psXParent->bRed = IMG_TRUE;
				TreeRotateLeft(psState, psTree, psXParent);
				psW = psXParent->psRight;
			}
			if (!TreeIsRed(psW->psLeft) && !TreeIsRed(psW->psRight))
			{
				psW->bRed = IMG_TRUE;

				psX = psXParent;
				psXParent = psX->psParent;
			}
			else 
			{
				if (!TreeIsRed(psW->psRight))
				{
					psW->psLeft->bRed = IMG_FALSE;
					psW->bRed = IMG_TRUE;
					TreeRotateRight(psState, psTree, psW);
					psW = psXParent->psRight;
				}
				psW->bRed = psXParent->bRed;
				psXParent->bRed = IMG_FALSE;
				psW->psRight->bRed = IMG_FALSE;
				TreeRotateLeft(psState, psTree, psXParent);

				psX = psTree->psBase;
				psXParent = NULL;
			}
		}
		else
		{
			GEN_TYNAME(BASETREE_PTR)	psW;

			ASSERT(psX == psXParent->psRight);

			psW = psXParent->psLeft;
			if (psW->bRed)
			{
				psW->bRed = IMG_FALSE;
				psXParent->bRed = IMG_TRUE;
				TreeRotateRight(psState, psTree, psXParent);
				psW = psXParent->psLeft;
			}
			if (!TreeIsRed(psW->psRight) && !TreeIsRed(psW->psLeft))
			{
				psW->bRed = IMG_TRUE;

				psX = psXParent;
				psXParent = psX->psParent;
			}
			else 
			{
				if (!TreeIsRed(psW->psLeft))
				{
					psW->psRight->bRed = IMG_FALSE;
					psW->bRed = IMG_TRUE;
					TreeRotateLeft(psState, psTree, psW);
					psW = psXParent->psLeft;
				}
				psW->bRed = psXParent->bRed;
				psXParent->bRed = IMG_FALSE;
				if (psW->psLeft != NULL)
				{
					psW->psLeft->bRed = IMG_FALSE;
				}
				TreeRotateRight(psState, psTree, psXParent);

				psX = psTree->psBase;
				psXParent = NULL;
			}
		}
	}
	if (psX != NULL)
	{
		psX->bRed = IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_VOID GEN_NAME(TreeRemove)(PGDATA_SYS_STATE psState,
							  GEN_TYNAME(TREE_PTR) psTree,
							  IMG_PVOID pvKey,
							  GEN_TYNAME(ITERATOR_FN) pfnIter,
							  IMG_PVOID pvIterData)
/*****************************************************************************
 FUNCTION	: TreeRemove

 PURPOSE	: Remove a data item from a tree

 INPUT     	: psState		- Compiler state
              psTree		- Tree to remove from
              pvKey			- Key of the item to remove.
			  pfnIter		- Callback to call on the item to remove.
              pvIterData	- Data to pass to the callback.

 RETURNS	: Nothing
*****************************************************************************/
{
	GEN_TYNAME(BASETREE_PTR)	psNodeToRemove;
	GEN_TYNAME(BASETREE_PTR)	psNodeToSplice;
	GEN_TYNAME(BASETREE_PTR)	psSpliceChild;
	GEN_TYNAME(BASETREE_PTR)	psSpliceParent;
	IMG_BOOL					bSpliceIsRed;

	/*
		Search the tree for a node with the same key.
	*/
	psNodeToRemove = TreeBaseGetPtr(psTree, pvKey);
	if (psNodeToRemove == NULL)
	{
		/*
			Key isn't present in the tree.
		*/
		return;
	}

	/*
		Get the node to splice into the same position as the removed node.
	*/
	if (psNodeToRemove->psLeft == NULL || psNodeToRemove->psRight == NULL)
	{
		/*
			If the removed node doesn't have two children then removing it doesn't affect
			the binary search tree property.
		*/
		psNodeToSplice = psNodeToRemove;
	}
	else
	{
		/*
			Otherwise splice in the node with the next largest key.
		*/
		psNodeToSplice = TreeSuccessor(psNodeToRemove);
	}

	/*
		Replace the spliced node by one of its children.
	*/
	if (psNodeToSplice->psLeft != NULL)
	{
		psSpliceChild = psNodeToSplice->psLeft;
		ASSERT(psNodeToSplice->psRight == NULL);
	}
	else
	{
		psSpliceChild = psNodeToSplice->psRight;
	}

	/*
		Replace the spliced node by its child.
	*/
	TreeReplaceParent(psState, psTree, psNodeToSplice, psSpliceChild);

	psSpliceParent = psNodeToSplice->psParent;
	bSpliceIsRed = psNodeToSplice->bRed;

	if (psNodeToSplice != psNodeToRemove)
	{
		/*
			Copy the data from the removed node.
		*/
		psNodeToSplice->psLeft = psNodeToRemove->psLeft;
		psNodeToSplice->psRight = psNodeToRemove->psRight;
		psNodeToSplice->psParent = psNodeToRemove->psParent; 
		psNodeToSplice->bRed = psNodeToRemove->bRed;

		if (psSpliceParent == psNodeToRemove)
		{
			psSpliceParent = psNodeToSplice;
		}

		/*
			Replace the parent pointers in the removed node's children.
		*/
		if (psNodeToRemove->psLeft != NULL)
		{
			ASSERT(psNodeToRemove->psLeft->psParent == psNodeToRemove);
			psNodeToRemove->psLeft->psParent = psNodeToSplice;
		}
		if (psNodeToRemove->psRight != NULL)
		{
			ASSERT(psNodeToRemove->psRight->psParent == psNodeToRemove);
			psNodeToRemove->psRight->psParent = psNodeToSplice;
		}

		/*
			Replace the child pointer in the removed node's parent.
		*/
		TreeReplaceParent(psState, psTree, psNodeToRemove, psNodeToSplice);
	}

	/*
		Rebalance the tree.
	*/
	if (!bSpliceIsRed)
	{
		TreeDelete_Fix(psState, psTree, psSpliceChild, psSpliceParent);
	}

	/*
		Free the removed node.
	*/
	if (pfnIter != NULL)
	{
		pfnIter(pvIterData, (IMG_PVOID)BaseTreeGetPtr(psNodeToRemove));
	}
	GDATA_FREE(psState, psNodeToRemove);
}

static
IMG_VOID TreeAdd_Fix(PGDATA_SYS_STATE			psState,
					 GEN_TYNAME(TREE_PTR)		psTree,
					 GEN_TYNAME(BASETREE_PTR)	psZ)
/*****************************************************************************
 FUNCTION	: TreeAdd_Fix

 PURPOSE	: Rebalance a tree after adding a node.

 INPUT     	: psTree     - Tree to fix.
              psZ	 - Unbalanced subtree.

 RETURNS	: Nothing
*****************************************************************************/
{
	while (psZ->psParent != IMG_NULL && psZ->psParent->bRed)
	{
		if (psZ->psParent == psZ->psParent->psParent->psLeft)
		{
			GEN_TYNAME(BASETREE_PTR) y = psZ->psParent->psParent->psRight;

			if (y != IMG_NULL && y->bRed)
			{
				psZ->psParent->bRed = IMG_FALSE;
				y->bRed = IMG_FALSE;
				psZ->psParent->psParent->bRed = IMG_TRUE;
				psZ = psZ->psParent->psParent;
			}
			else
			{
				if (psZ == psZ->psParent->psRight)
				{
					psZ = psZ->psParent;
					TreeRotateLeft(psState, psTree, psZ);
				}
				psZ->psParent->bRed = IMG_FALSE;
				psZ->psParent->psParent->bRed = IMG_TRUE;
				TreeRotateRight(psState, psTree, psZ->psParent->psParent);
			}
		}
		else
		{
			GEN_TYNAME(BASETREE_PTR) y = psZ->psParent->psParent->psLeft;

			if (y != IMG_NULL && y->bRed)
			{
				psZ->psParent->bRed = IMG_FALSE;
				y->bRed = IMG_FALSE;
				psZ->psParent->psParent->bRed = IMG_TRUE;
				psZ = psZ->psParent->psParent;
			}
			else
			{
				if (psZ == psZ->psParent->psLeft)
				{
					psZ = psZ->psParent;
					TreeRotateRight(psState, psTree, psZ);
				}
				psZ->psParent->bRed = IMG_FALSE;
				psZ->psParent->psParent->bRed = IMG_TRUE;
				TreeRotateLeft(psState, psTree, psZ->psParent->psParent);
			}
		}
	}
	psTree->psBase->bRed = IMG_FALSE;
}

IMG_INTERNAL
IMG_PVOID GEN_NAME(TreeAdd)(PGDATA_SYS_STATE psState,
						    GEN_TYNAME(TREE_PTR) psTree,
						    IMG_PVOID pvNewData)
/*****************************************************************************
 FUNCTION	: TreeAdd

 PURPOSE	: Add a data item to the tree

 INPUT     	: psState    - Compiler state
              psTree     - Tree to search
              pvNewData  - Pointer to data to add

 RETURNS	: A pointer to the new element.
*****************************************************************************/
{
	GEN_TYNAME(BASETREE_PTR)	psParent;
	GEN_TYNAME(BASETREE_PTR)*	ppsParent;
	GEN_TYNAME(BASETREE_PTR)	psWalker;
	GEN_TYNAME(BASETREE_PTR)	psNew;
	IMG_INT32	iCmp;

	ASSERT(psTree != NULL);

	ppsParent	= &psTree->psBase;
	psParent	= NULL;
	psWalker	= psTree->psBase;

	/*
		Walk down the tree looking for the point to insert the new node.
	*/
	while (psWalker != IMG_NULL)
	{
		iCmp = psTree->pfnCompare(pvNewData, BaseTreeGetPtr(psWalker));
		if (iCmp == 0)
		{
			/*
			  Found a match, replace it and bail out
			  (no need for re-balancing)
			*/
			BaseTreeSet(psWalker, psTree->uElemSize, pvNewData);
			return BaseTreeGetPtr(psWalker);
		}
		
		psParent = psWalker;
		if (iCmp < 0)
		{
			ppsParent = &psWalker->psLeft;
			psWalker = psWalker->psLeft;
		}
		else
		{
			ppsParent = &psWalker->psRight;
			psWalker = psWalker->psRight;
		}
	}

	/*
		Create a new node.
	*/
	psNew = BaseTreeMake(psState, psTree->uElemSize, NULL, NULL);
	BaseTreeSet(psNew, psTree->uElemSize, pvNewData);

	/*
		Link the new node in at the last point we reached in our walk of the tree.
	*/
	psNew->psParent = psParent;
	*ppsParent = psNew;

	/*
		Rebalance the tree.
	*/
	TreeAdd_Fix(psState, psTree, psNew);

	return BaseTreeGetPtr(psNew);
}

typedef enum _USCTREE_ITERATIONORDER
{
	USCTREE_ITERATION_PREORDER,
	USCTREE_ITERATION_POSTORDER,
	USCTREE_ITERATION_INORDER,
} USCTREE_ITERATIONORDER, *PUSCTREE_ITERATIONORDER;

static
IMG_VOID BaseTreeIter(PGDATA_SYS_STATE			psState,
					  GEN_TYNAME(TREE_PTR)		psTree,
					  USCTREE_ITERATIONORDER	eOrderType,
					  GEN_TYNAME(ITERATOR_FN)	pfnIter,
					  IMG_PVOID					pvIterData)
/*****************************************************************************
 FUNCTION	: BaseTreeIter

 PURPOSE	: Apply an iterator through a tree

 INPUT     	: psState       - Compiler state
              psTree        - Tree to iterate through
			  eOrderType	- Order of iteration.
              pfnIter       - Iterator to call on each element
              pvIterData    - Data to pass to the iterator function

 RETURNS	: Nothing
*****************************************************************************/
{
	GEN_TYNAME(BASETREE_PTR) psNode;
	GEN_TYNAME(BASETREE_PTR) psPrevNode;

	PVR_UNREFERENCED_PARAMETER(psState);

	if (psTree == NULL)
		return;

	psPrevNode = NULL;
	psNode = psTree->psBase;
	while (psNode != NULL)
	{
		enum
		{
			NEXT_NODE_LEFT,
			NEXT_NODE_RIGHT,
			NEXT_NODE_PARENT,
		} eNextNode;

		/*
			Choose the next node to process after this one.
		*/
		if (psPrevNode == psNode->psParent)
		{
			eNextNode = NEXT_NODE_LEFT;
		}
		else if (psPrevNode == psNode->psLeft)
		{
			eNextNode = NEXT_NODE_RIGHT;
		}
		else
		{
			ASSERT(psPrevNode == psNode->psRight);
			eNextNode = NEXT_NODE_PARENT;
		}

		/*
			Update the current node skipping the left- or right-hand
			subtrees if either is empty.
		*/
		psPrevNode = psNode;
		switch (eNextNode)
		{
			case NEXT_NODE_LEFT:
			{
				if (eOrderType == USCTREE_ITERATION_PREORDER)
				{
					pfnIter(pvIterData, BaseTreeGetPtr(psNode));
				}
				if (psNode->psLeft != NULL)
				{
					psNode = psNode->psLeft;
					break;
				}
				/* Intentional fallthrough. */
			}
			case NEXT_NODE_RIGHT:
			{
				if (eOrderType == USCTREE_ITERATION_INORDER)
				{
					pfnIter(pvIterData, BaseTreeGetPtr(psNode));
				}
				if (psNode->psRight != NULL)
				{
					psNode = psNode->psRight;
					break;
				}
				/* Intentional fallthrough. */
			}
			case NEXT_NODE_PARENT:
			{
				if (eOrderType == USCTREE_ITERATION_POSTORDER)
				{
					pfnIter(pvIterData, BaseTreeGetPtr(psNode));
				}
				psNode = psNode->psParent;
				break;
			}
			default: imgabort();
		}
	}
}

IMG_INTERNAL
IMG_VOID GEN_NAME(TreeIterInOrder)(PGDATA_SYS_STATE psState,
								   GEN_TYNAME(TREE_PTR) psTree,
								   GEN_TYNAME(ITERATOR_FN) pfnIter,
								   IMG_PVOID pvIterData)
/*****************************************************************************
 FUNCTION	: TreeIterInOrder

 PURPOSE	: Apply an iterator through a tree

 INPUT     	: psTree        - Tree to iterate through
              pfnIter       - Iterator to call on each element
              pvIterData    - Data to pass to the iterator function

 RETURNS	: Nothing
*****************************************************************************/
{
	BaseTreeIter(psState,
				 psTree,
				 USCTREE_ITERATION_INORDER,
				 pfnIter,
				 pvIterData);
}

IMG_INTERNAL
IMG_VOID GEN_NAME(TreeIterPreOrder)(PGDATA_SYS_STATE psState,
									GEN_TYNAME(TREE_PTR) psTree,
									GEN_TYNAME(ITERATOR_FN) pfnIter,
									IMG_PVOID pvIterData)
/*****************************************************************************
 FUNCTION	: TreeIterPreOrder

 PURPOSE	: Apply an iterator through a tree

 INPUT     	: psState       - Compiler state
              psTree        - Tree to iterate through
              pfnIter       - Iterator to call on each element
              pvIterData    - Data to pass to the iterator function

 RETURNS	: Nothing
*****************************************************************************/
{
	BaseTreeIter(psState,
				 psTree,
				 USCTREE_ITERATION_PREORDER,
				 pfnIter,
				 pvIterData);
}

#ifndef USC_DATA_DEF /* Don't use generic vectors for USC */

/***
 * Vectors
 ***/

/* uElemsPerChunk: MUST BE < 32 */
static const IMG_UINT32 uElemsPerChunk = 15;

/* Chunks: Section of a vector */
typedef struct GEN_TYNAME(_0CHUNK_)
{
	/* Vector index of first element in this chunk */
	IMG_UINT32 uIndex;

	/* Bitset of used elements */
	IMG_UINT32 uUseSet;

	/* Links for chunk list */
	struct GEN_TYNAME(_0CHUNK_) *psPrev;
	struct GEN_TYNAME(_0CHUNK_) *psNext; 
	
	/* Elements contained in this chunk */
	IMG_BYTE pbData[];
} GEN_TYNAME(CHUNK), *GEN_TYNAME(CHUNK_PTR);

#if 0
struct _GDATA_DUMMY_CHUNK_
{
};
#endif

/**
 * Data in a vector is arranged in chunks of (uElemsPerChunk + 1) elements.
 **/
struct GEN_TYNAME(_0VECTOR_)
{
	/* uNumChunks: Number of chunks in the vector */
	IMG_UINT32 uNumChunks;
	/* uChunk: Number of elements in a chunk */
	IMG_UINT32 uChunk;
	/* uEnd: Index after last element (uIdx < uEnd checks for uIdx in bounds) */
	IMG_UINT32 uEnd;

	/* uElemSize: Size in bytes of a vector element */
	IMG_UINT32 uElemSize;

	/* psFirst: Pointer to first chunk  */
	GEN_TYNAME(CHUNK_PTR) psFirst;
	/* psMemo: Last chunk to be accessed */
	GEN_TYNAME(CHUNK_PTR) psMemo;

	/* pbDefault: Default element value */
	IMG_BYTE pbDefault[];
};
//GEN_TYNAME(VECTOR), *GEN_TYNAME(VECTOR_PTR);

#if 0
struct _GDATA_DUMMY_VEC_
{
};
#endif

/**
 * Functions
 **/ 

/*****************************************************************************
 FUNCTION	: ChunkMake

 PURPOSE	: Allocate and initialise a chunk

 INPUT     	: psState       - Compiler state
              uElemSize     - Element size in bytes

 RETURNS	: The newly allocated chunk
*****************************************************************************/
static
GEN_TYNAME(CHUNK_PTR) GEN_NAME(ChunkMake)(PGDATA_SYS_STATE psState,
										  IMG_UINT32 uElemSize)
{
	GEN_TYNAME(CHUNK_PTR) psChunk = NULL;
	IMG_UINT32 uDataSize = 0;

	/* Calculate the size of a chunk */
	uDataSize = sizeof(GEN_TYNAME(CHUNK));      // Housekeeping information
	uDataSize += (uElemSize * uElemsPerChunk);  // Data array (field pbData)

	/* Allocate */
	psChunk = GDATA_ALLOC(psState, uDataSize);

	/* Initialise  */
	memset(psChunk, 0, uDataSize);

	/* Done */
	return psChunk;
}

#if 0
/*****************************************************************************
 FUNCTION	: ChunkDrop

 PURPOSE	: Drop a chunk from its list

 INPUT     	: psState       - Compiler state
              psChunk       - Chunk to drop

 RETURNS	: Nothing
*****************************************************************************/
static
IMG_VOID GEN_NAME(ChunkDrop)(PGDATA_SYS_STATE psState,
							 GEN_TYNAME(CHUNK_PTR) psChunk)
{
	GEN_TYNAME(CHUNK_PTR) psPrev = NULL;
	GEN_TYNAME(CHUNK_PTR) psNext = NULL;

	PVR_UNREFERENCED_PARAMETER(psState);

	if (psChunk == NULL)
		return;

	/* Re-attach the previous and next chunks in the list */
	psPrev = psChunk->psPrev;
	psNext = psChunk->psNext;

	psPrev->psNext = psNext;
	psNext->psPrev = psPrev;

	psChunk->psPrev = NULL;
	psChunk->psNext = NULL;
}
#endif

/*****************************************************************************
 FUNCTION	: ChunkDelete

 PURPOSE	: Delete a chunk

 INPUT     	: psState       - Compiler state
              psChunk       - Chunk to delete

 RETURNS	: Nothing
*****************************************************************************/
static
IMG_VOID GEN_NAME(ChunkDelete)(PGDATA_SYS_STATE psState,
							   GEN_TYNAME(CHUNK_PTR) psChunk)
{
	if (psChunk == NULL)
		return;

	/* Delete the structure */
	GDATA_FREE(psState, psChunk);
}


/*****************************************************************************
 FUNCTION	: ChunkGet

 PURPOSE	: Get the chunk containing an element

 INPUT     	: psStart       - Chunk to start looking from
              uIdx          - Index of element to find

 OUTPUT     : ppsChunk      - The required chunk or the closest to it.

 RETURNS	: IMG_TRUE iff chunk exists
*****************************************************************************/
static
IMG_BOOL GEN_NAME(ChunkGet)(GEN_TYNAME(CHUNK_PTR) psStart,
							IMG_UINT32 uIdx,
							GEN_TYNAME(CHUNK_PTR)* ppsChunk)
{
	GEN_TYNAME(CHUNK_PTR) psChunk = NULL;
	GEN_TYNAME(CHUNK_PTR) psCurr = NULL;
	GEN_TYNAME(CHUNK_PTR) psLast = NULL;
	IMG_BOOL bExists = IMG_FALSE;
	IMG_BOOL bForward = IMG_TRUE;
	
	/* Test for no existing chunks */
	if (psStart == NULL)
	{
		psChunk = NULL;
		bExists = IMG_FALSE;
		goto Exit;
	}

	/* Establish whether the required chunk is ahead or behind the start */
	if (uIdx < psStart->uIndex)
	{
		/* Go backward */
		bForward = IMG_TRUE;
	}
	else
	{
		/* Go forward */
		bForward = IMG_TRUE;
	}

	/* Find the chunk */
	psLast = psCurr;
	if (bForward)
	{
		/* Search forward */
		GEN_TYNAME(CHUNK_PTR) psNext = NULL;

		for (psCurr = psStart; psCurr != NULL; psCurr = psNext)
		{
			const IMG_UINT32 uCurrIndex = psCurr->uIndex;

			psNext = psCurr->psNext;
			if (uCurrIndex < uIdx &&
				uIdx <= (uCurrIndex + uElemsPerChunk))
			{
				/* Found the required chunk */
				bExists = IMG_TRUE;
				psChunk = psCurr;
				goto Exit;
			}
			psLast = psCurr;
		}
	}
	else
	{
		/* Search backward */
		GEN_TYNAME(CHUNK_PTR) psPrev = NULL;

		for (psCurr = psStart; psCurr != NULL; psCurr = psPrev)
		{
			const IMG_UINT32 uCurrIndex = psCurr->uIndex;

			psPrev = psCurr->psPrev;
			if (uCurrIndex < uIdx &&
				uIdx <= (uCurrIndex + uElemsPerChunk))
			{
				/* Found the required chunk */
				bExists = IMG_TRUE;
				psChunk = psCurr;
				goto Exit;
			}
			/* Didn't find the chunk */
			psLast = psCurr;
		}
	}
	/* Didn't find the chunk */
	bExists = IMG_FALSE;
	psChunk = psLast;

  Exit:
	/* Set return values */
	if (ppsChunk != NULL)
		(*ppsChunk) = psChunk;

	return bExists;
}

/*****************************************************************************
 FUNCTION	: ChunkAdd

 PURPOSE	: Add a chunk to contain an element

 INPUT     	: psState    - Compiler state
              psPos      - Chunk to start looking from
              uIdx       - Index of element to contain

 RETURNS	: The chunk to contain the element

 NOTES      : Chunk will be inserted to the left or the right of psPos,
              depending on uIdx.
              psPos can be NULL.
*****************************************************************************/
static
GEN_TYNAME(CHUNK_PTR) GEN_NAME(ChunkAdd)(PGDATA_SYS_STATE psState,
										 GEN_TYNAME(CHUNK_PTR) psPos,
										 IMG_UINT32 uElemSize,
										 IMG_UINT32 uIdx)
{
	GEN_TYNAME(CHUNK_PTR) psChunk = NULL;
	IMG_UINT32 uChunkIdx = 0;
	IMG_UINT32 uChunkEnd = 0;
	IMG_UINT32 uPosIndex = 0;
	IMG_UINT32 uPosEnd = 0;

	/* Work out the start and end of the elements in the new chunk */
	uChunkIdx = uIdx - (uIdx % uElemsPerChunk);
	uChunkEnd = uChunkIdx + uElemsPerChunk;

	/* Test for an empty chunk list */
	if (psPos == NULL)
	{
		psChunk = GEN_NAME(ChunkMake)(psState, uElemSize);
		psChunk->uIndex = uChunkIdx;
		goto Exit;
	}

	/* Create the new chunk */
	psChunk = GEN_NAME(ChunkMake)(psState, uElemSize);
	psChunk->uIndex = uChunkIdx;

	/* Insert on left or right */
	uPosIndex = psPos->uIndex;
	uPosEnd = uPosIndex + uElemsPerChunk;

	assert((uIdx < uPosIndex) || (uIdx >= uPosEnd));
	if (uIdx < uPosIndex)
	{
		/* Chunk goes on the left */
		GEN_TYNAME(CHUNK_PTR) psPrev = psPos->psPrev;

#ifdef DEBUG
		/* sanity check */
		assert((psPrev == NULL) ||
			   (uIdx >= (psPrev->uIndex + uElemsPerChunk)));

#endif /* DEBUG */

		if (psPrev != NULL)
			psPrev->psNext = psChunk;
		psPos->psPrev = psChunk;

		psChunk->psPrev = psPrev;
		psChunk->psNext = psPos;
	}
	else
	{
		/* Chunk goes on the right */
		GEN_TYNAME(CHUNK_PTR) psNext = psPos->psNext;

#ifdef DEBUG
		/* sanity check */
		assert((psNext == NULL) ||
			   (uIdx < psNext->uIndex));
#endif /* DEBUG */

		if (psNext != NULL)
			psNext->psPrev = psChunk;
		psPos->psNext = psChunk;

		psChunk->psNext = psNext;
		psChunk->psPrev = psPos;
	}

  Exit:
	/* Done */
	return psChunk;
}

/*****************************************************************************
 FUNCTION	: VectorInit

 PURPOSE	: Initialise a vector

 INPUT     	: psState       - Compiler state
              psVector      - Vector to initialise
              uElemSize     - Element size in bytes
              pvDefault     - Pointer to a default value (or NULL)

 RETURNS	: The initialised vector
*****************************************************************************/
IMG_INTERNAL
GEN_TYNAME(VECTOR_PTR) GEN_NAME(VectorInit)(PGDATA_SYS_STATE psState, 
											GEN_TYNAME(VECTOR_PTR) psVector,
											IMG_UINT32 uElemSize,
											IMG_VOID* pvDefault)
{
	PVR_UNREFERENCED_PARAMETER(psState);

	/* Initialise the vector fields */
	memset(psVector, 0, sizeof(*psVector));
	psVector->uElemSize = uElemSize;

	/* Set up the default value */
	if (pvDefault == NULL)
		memset(psVector->pbDefault, 0, uElemSize);
	else
		memcpy(psVector->pbDefault, pvDefault, uElemSize);

	return psVector;
}

/*****************************************************************************
 FUNCTION	: VectorClear

 PURPOSE	: Reset a vector, deleting its internal structures.

 INPUT     	: psState       - Compiler state
              psVector      - Vector to delete

 RETURNS	: Nothing
*****************************************************************************/
IMG_INTERNAL
IMG_VOID GEN_NAME(VectorClear)(PGDATA_SYS_STATE psState,
							   GEN_TYNAME(VECTOR_PTR) psVector)
{
	GEN_TYNAME(CHUNK_PTR) psCurr = NULL;
	GEN_TYNAME(CHUNK_PTR) psNext = NULL;

	if (psVector == NULL)
		return;

	/* Delete the list of chunks */
	psNext = NULL;
	for (psCurr = psVector->psFirst;
		 psCurr != NULL;
		 psCurr = psNext)
	{
		psNext = psCurr->psNext;
		GEN_NAME(ChunkDelete)(psState, psCurr);
	}

	/* Reset the fields */
	psVector->uNumChunks = 0;
	psVector->uChunk = 0;
	psVector->uEnd = 0;

	psVector->psMemo = NULL;
	psVector->psFirst = NULL;
}

/*****************************************************************************
 FUNCTION	: VectorMake

 PURPOSE	: Allocate and initialise a vector

 INPUT     	: psState       - Compiler state
              uElemSize     - Element size in bytes
              pvDefault     - Pointer to a default value (or NULL)

 RETURNS	: The newly allocated vector
*****************************************************************************/
IMG_INTERNAL
GEN_TYNAME(VECTOR_PTR) GEN_NAME(VectorMake)(PGDATA_SYS_STATE psState,
											IMG_UINT32 uElemSize,
											IMG_VOID* pvDefault)
{
	GEN_TYNAME(VECTOR_PTR) psVector = NULL;
	IMG_UINT32 uVecSize = 0;

	/* Allocate the vector */
	uVecSize = sizeof(GEN_TYNAME(VECTOR));  // Base structure
	uVecSize += uElemSize;                  // pbDefault value
	psVector = GDATA_ALLOC(psState, sizeof(GEN_TYNAME(VECTOR)));

	return GEN_NAME(VectorInit(psState, psVector, uElemSize, pvDefault));
}

/*****************************************************************************
 FUNCTION	 : VectorDelete

 PURPOSE	 : Delete a vector

 INPUT     	 : psState       - Compiler state
               psVector      - Vector to delete

 RETURNS	 : Nothing
*****************************************************************************/
IMG_INTERNAL
IMG_VOID GEN_NAME(VectorDelete)(PGDATA_SYS_STATE psState,
								GEN_TYNAME(VECTOR_PTR) psVector)
{
	if (psVector == NULL)
		return;

	/* Delete the fields */
	GEN_NAME(VectorClear)(psState, psVector);

	/* Delete the structure */
	GDATA_FREE(psState, psVector);
}

/*****************************************************************************
 FUNCTION	: VectorGet

 PURPOSE	: Get a vector element

 INPUT     	: psVector   - Vector
              uIdx       - Index of element

 OUTPUT     : pvData     - Element data

 RETURNS	: Nothing
*****************************************************************************/
IMG_INTERNAL
IMG_VOID GEN_NAME(VectorGet)(GEN_TYNAME(VECTOR_PTR) psVector,
							 IMG_UINT32 uIdx,
							 IMG_VOID* pvData)
{
	GEN_TYNAME(CHUNK_PTR) psChunk = NULL;
	IMG_PVOID *ppvElem = NULL;
	IMG_BOOL bFound = IMG_FALSE;
	IMG_UINT32 uElemSize =  0;

	assert(psVector != NULL);
	assert(pvData != NULL);

#ifdef DEBUG
	/* If there is a list of chunks, the memo cannot be NULL */
	assert(psVector->psFirst == NULL || psVector->psMemo != NULL);
#endif /* DEBUG */

	bFound = GEN_NAME(ChunkGet)(psVector->psMemo, uIdx, &psChunk);
	ppvElem = (IMG_PVOID)(&(psVector->pbDefault[0]));
	uElemSize = psVector->uElemSize;

	if (bFound)
	{
		/* Found the chunk */
		IMG_UINT32 uElemIdx = 0;
		IMG_UINT32 uElemOffset = 0;

		assert(psChunk != NULL);
		assert(psChunk->uIndex >= uIdx);
		assert(uIdx < (psChunk->uIndex + uElemsPerChunk));
		psVector->psMemo = psChunk;

		uElemIdx = uIdx - psChunk->uIndex;
		uElemOffset = uElemSize * uElemIdx;

		/* Found the element */
		if (psChunk->uUseSet & (1U << uElemOffset))
		{
			/* Element is set */
			ppvElem = (IMG_PVOID)(&(psChunk->pbData[uElemOffset]));
		}
	}
	else if (psChunk != NULL)
	{
		/* No element found, but came close. Sanity checks and update the memo */
		assert((uIdx < psChunk->uIndex) ||
			   (uIdx >= (psChunk->uIndex + uElemsPerChunk)));
		psVector->psMemo = psChunk;
	}

	/* Copy the element */
	memcpy(pvData, ppvElem, uElemSize);
}

/*****************************************************************************
 FUNCTION	: VectorSet

 PURPOSE	: Set a vector element

 INPUT     	: psVector   - Vector
              uIdx       - Index of element
              pvData     - Element data

 RETURNS	: Nothing
*****************************************************************************/
IMG_INTERNAL
IMG_VOID GEN_NAME(VectorSet)(PGDATA_SYS_STATE psState,
							 GEN_TYNAME(VECTOR_PTR) psVector,
							 IMG_UINT32 uIdx,
							 IMG_VOID* pvData)
{
	GEN_TYNAME(CHUNK_PTR) psChunk = NULL;
	IMG_PVOID *ppvElem = NULL;
	IMG_BOOL bFound = IMG_FALSE;
	IMG_UINT32 uElemSize =  0;

	assert(psVector != NULL);
	assert(pvData != NULL);

#ifdef DEBUG
	/* If there is a list of chunks, the memo cannot be NULL */
	assert(psVector->psFirst == NULL || psVector->psMemo != NULL);
#endif /* DEBUG */

	ppvElem = (IMG_PVOID)(&(psVector->pbDefault[0]));
	uElemSize = psVector->uElemSize;

	bFound = IMG_FALSE;
	psChunk = NULL;
	if (psVector->psMemo != NULL)
	{
		bFound = GEN_NAME(ChunkGet)(psVector->psMemo, uIdx, &psChunk);
	}

	if (!bFound)
	{
		GEN_TYNAME(CHUNK_PTR) psNewChunk = NULL;

		/* Didn't find the chunk so add it */
		psNewChunk = GEN_NAME(ChunkAdd)(psState, psChunk, uElemSize, uIdx);
		assert(psNewChunk != NULL);
		psChunk = psNewChunk;

		if (psVector->psFirst == NULL)
		{
			psVector->psFirst = psChunk;
		}
	}
	/* Update the memo */
	psVector->psMemo = psChunk;

	/* Get a pointer to the element */
	{
		IMG_UINT32 uElemIdx = 0;
		IMG_UINT32 uElemOffset = 0;

		assert(psChunk != NULL);
		assert(psChunk->uIndex >= uIdx);
		assert(uIdx < (psChunk->uIndex + uElemsPerChunk));

		uElemIdx = uIdx - psChunk->uIndex;
		uElemOffset = uElemSize * uElemIdx;

		ppvElem = (IMG_PVOID)(&(psChunk->pbData[uElemOffset]));

		/* Update the use set */
		psChunk->uUseSet |= (1U << uElemOffset);
	}

	/* Copy the element */
	if (pvData != NULL)
	{
		memcpy(ppvElem, pvData, uElemSize);
	}
}
#endif /* USC_DATA_DEF */
#if defined(SUPPORT_ICODE_SERIALIZATION)
static
IMG_VOID BaseTreePostOrderTraverseRecursive(PGDATA_SYS_STATE psState,
									GEN_TYNAME(BASETREE_PTR) *ppsBaseTree,
									GEN_TYNAME(ITERATOR_EX_FN) pfnSerializeUnserialize,
									GEN_TYNAME(ITERATOR_FN) pfnTranslate,
									IMG_PVOID pvData,
									IMG_UINT32 uSize)
/*****************************************************************************
 FUNCTION	: BaseTreePostOrderTraverse

 PURPOSE	: Delete a basetree

 INPUT     	: psState       - Compiler state
              psBaseTree    - BaseTree to delete
              pfnIter       - Iterator to call on each element
              pvIterData    - Data to pass to the iterator function

 RETURNS	: Nothing
*****************************************************************************/
{
	GEN_TYNAME(BASETREE_PTR) psCurr;

	if ((*ppsBaseTree) == NULL)
	{
		return;
	}
	
	/* Apply the iterator to the data */
	pfnSerializeUnserialize(pvData, ppsBaseTree, sizeof(GEN_TYNAME(BASETREE)) + uSize - sizeof(IMG_BYTE*));
	psCurr = *ppsBaseTree;

	BaseTreePostOrderTraverseRecursive(psState, &psCurr->psLeft, pfnSerializeUnserialize, 
									   pfnTranslate, pvData, uSize);
	BaseTreePostOrderTraverseRecursive(psState, &psCurr->psRight, pfnSerializeUnserialize, 
									   pfnTranslate, pvData, uSize);
	
	TranslateSimpleElement(pvData, (IMG_PVOID*)&psCurr->psParent);
	pfnTranslate(pvData, &psCurr->pvElem);
}

IMG_INTERNAL
IMG_VOID GEN_NAME(TreePostOrderTraverseRecursive)(PGDATA_SYS_STATE psState,
				  								  GEN_TYNAME(TREE_PTR) *ppsTree, 
												  GEN_TYNAME(ITERATOR_EX_FN) pfnSerializeUnserialize,
												  GEN_TYNAME(ITERATOR_FN) pfnTranslate,
												  IMG_PVOID pvData)
/*****************************************************************************
 FUNCTION	: TreePostOrderTraverseRecursive

 PURPOSE	: Delete a tree

 INPUT     	: psState       - Compiler state
              psTree        - Tree to delete

 RETURNS	: Nothing
*****************************************************************************/
{
	pfnSerializeUnserialize(pvData, ppsTree, sizeof(GEN_TYNAME(TREE)));
	BaseTreePostOrderTraverseRecursive(psState, &((*ppsTree)->psBase), pfnSerializeUnserialize, 
									   pfnTranslate, pvData, (*ppsTree)->uElemSize);
}
#endif /*defined(SUPPORT_ICODE_SERIALIZATION)*/
/*****************************************************************************
 End of file
*****************************************************************************/
