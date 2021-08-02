/*************************************************************************
 * Name         : data.c
 * Title        : Data structures: arrays, vectors, graphs, matrices
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
 * $Log: data.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"

IMG_INTERNAL
IMG_VOID ResizeArray(PINTERMEDIATE_STATE psState,
					 IMG_PVOID psArray,
					 IMG_UINT32 uOldSize,
					 IMG_UINT32 uNewSize,
					 IMG_PVOID* ppsNew)
/*********************************************************************************
 Function			: ResizeArray

 Description		: Resize an array, copying initial elements from the original
					  to the new array.

 Parameters			: psState	- Compiler state
					  psArray	- Array to resize
					  uOldSize	- Size in bytes of original array
					  uNewSize  - Size in bytes of new array
					  ppsNew	- The new array or NULL if new size is 0.

 Return				: IMG_FALSE on failure, IMG_TRUE otherwise

 Notes				: Releases memory for the original array.
*********************************************************************************/
{
	IMG_PVOID psNew = NULL;

	/* Test that new array has a size */
	if (uNewSize > 0)
	{
		/* Get new array */
		psNew = UscAlloc(psState, uNewSize);

		/* Copy the array */
		if (uNewSize > uOldSize)
		{
			memset(psNew, 0, uNewSize);
		}
		if (psArray != NULL)
		{
			memcpy(psNew, psArray, min(uNewSize, uOldSize));
		}
	}

	/* Delete the old array */
	UscFree(psState, psArray);

	/* Done */
	*ppsNew = psNew;
}

IMG_INTERNAL
IMG_VOID IncreaseArraySizeInPlace(PINTERMEDIATE_STATE psState,
					 IMG_UINT32 uOldSize,
					 IMG_UINT32 uNewSize,
					 IMG_PVOID* ppsArray)
/*********************************************************************************
 Function			: IncreaseArraySizeInPlace

 Description		: Increase the size of the array, the new size MUST be larger than the 
					  original size. Basically, it implements a subset of the functionality of 
					  realloc, but will NOT work if the size is to be reduced. 

 Parameters			: psState	- Compiler state
					  uOldSize	- Size in bytes of original array
					  uNewSize  - Size in bytes of new array
					  ppsArray	- The new array or NULL if new size is 0.

 Return				: IMG_FALSE on failure, IMG_TRUE otherwise

 Notes				: Releases memory for the original array.
*********************************************************************************/
{
	IMG_PVOID psNew = NULL;
	IMG_PVOID psArray = *ppsArray;

	/* Test that new array has a size */
	if (uNewSize > 0)
	{
		/* Get new array */
		psNew = UscAlloc(psState, uNewSize);

		memset(psNew, 0, uNewSize);

		if (psArray != NULL)
		{
			memcpy(psNew, psArray, uOldSize);
		}
	}

	/* Delete the old array */
	UscFree(psState, psArray);

	/* Done */
	*ppsArray = psNew;

}

static
IMG_UINT32 GetArraySize(USC_PARRAY psArray)
/*****************************************************************************
 FUNCTION	: GetArraySize

 PURPOSE	: Extract the size of an extendable array from its stored form.

 PARAMETERS	: psArray    - Array

 RETURNS	: Size encoded in uNum
*****************************************************************************/
{
	return (psArray->uNumChunks);
}

static
IMG_VOID SetArraySize(USC_PARRAY psArray, IMG_UINT32 uSize)
/*****************************************************************************
 FUNCTION	: SetArraySize

 PURPOSE	: Set the size of an extendable array in its stored form.

 PARAMETERS	: psArray    - Array
			  uSize		- Size to set

 RETURNS	: New stored form.
*****************************************************************************/
{
	psArray->uNumChunks = uSize;

	/* Update the bounding index */
	psArray->uEnd = uSize * USC_ARRAY_CHUNK_SIZE;
}

#ifdef INLINE_IS_PRAGMA
#pragma inline(GetVectorSize)
#endif
static INLINE
IMG_UINT32 GetVectorSize(USC_PVECTOR psVector)
/*****************************************************************************
 FUNCTION	: GetVectorSize

 PURPOSE	: Extract the size of an extendable vector from its stored form.

 PARAMETERS	: psVector    - Vector

 RETURNS	: Size encoded in uNum
*****************************************************************************/
{
	return psVector->uSize;
}

#ifdef INLINE_IS_PRAGMA
#pragma inline(SetVectorSize)
#endif
static INLINE
IMG_VOID SetVectorSize(USC_PVECTOR psVector, IMG_UINT32 uSize)
/*****************************************************************************
 FUNCTION	: SetVectorSize

 PURPOSE	: Set the size of an extendable vector in its stored form.

 PARAMETERS	: psVector  - vector
			  uSize		- Size in chunks

 RETURNS	: New stored form.
*****************************************************************************/
{
	psVector->uSize = uSize;
}

#ifdef INLINE_IS_PRAGMA
#pragma inline(ChunkElem)
#endif
static INLINE
IMG_PVOID* ChunkElem(IMG_PVOID *pvArray, IMG_UINT32 uSize, IMG_UINT32 uIdx)
/*****************************************************************************
 FUNCTION	: ChunkElem

 PURPOSE	: Calculate the pointer to an element in a chunk array

 PARAMETERS	: pvArray	- The chunk array
			  uSize		- Size in bytes of each element in the array
			  uIdx		- The element index

 RETURNS	: A pointer to the element indexed by uIdx, after taking into
			  account the chunk header.
*****************************************************************************/
{
	return (IMG_PVOID*)(((IMG_PBYTE)pvArray) + (uIdx * uSize));
}

static
IMG_VOID FreeChunk(PINTERMEDIATE_STATE psState,
				   USC_PCHUNK *ppsChunk)
/*****************************************************************************
 FUNCTION	: FreeChunk

 PURPOSE	: Free or recyle an allocation chunk.

 PARAMETERS	: psState	- Compiler state.
			  uSize		- Size of the chunk (excluding the header)
			  psChunk	- Chunk to free

 RETURNS	: Nothing
*****************************************************************************/
{
	USC_PCHUNK psChunk = *ppsChunk;

	if (psChunk == NULL)
		return;

	UscFree(psState, psChunk->pvArray);
	UscFree(psState, psChunk);

	*ppsChunk = NULL;
}

static
USC_PCHUNK NewChunk(PINTERMEDIATE_STATE psState,
					IMG_UINT32 uSize,
					IMG_PVOID pvDefault)
/*****************************************************************************
 FUNCTION	: NewChunk

 PURPOSE	: Allocate a chunk

 PARAMETERS	: psState	- Compiler state.
			  uSize		- Size in bytes of the chunk to allocate

 RETURNS	: Nothing
*****************************************************************************/
{
	USC_PCHUNK psChunk = UscAlloc(psState, sizeof(USC_CHUNK));

	if (uSize > 0)
	{
		psChunk->pvArray = UscAlloc(psState, uSize);
	}

	/* Set the default values */
	psChunk->uIndex = 0;
	psChunk->psNext = NULL;
	psChunk->psPrev = NULL;

	if (pvDefault == 0 ||
		((IMG_UINTPTR_T)pvDefault) == 0xffU ||
		((IMG_UINTPTR_T)pvDefault) == (USC_UNDEF))
	{
		IMG_INT32 iVal = (pvDefault == 0) ? 0 : 0xff;
		memset(psChunk->pvArray, iVal, uSize);
#if 0
		memset(psChunk->pvArray, iVal, sizeof(psChunk->pvArray));
#endif
	}
	else
	{
		IMG_UINT32 uCtr;
		const IMG_UINT32 uNum = uSize / sizeof(IMG_PVOID);
		for (uCtr = 0; uCtr < uNum; uCtr ++)
		{
			psChunk->pvArray[uCtr] = pvDefault;
		}
	}

	/* Done */
	return psChunk;
}

IMG_INTERNAL
IMG_VOID ClearArray(PINTERMEDIATE_STATE psState,
					USC_PARRAY psArray,
					USC_STATE_FREEFN fnFree)
/*****************************************************************************
 FUNCTION	: ArrayClear

 PURPOSE	: Clear an array

 PARAMETERS	: psState	- Compiler state
			  psArray	- Extendable array
			  fnFree	- Free function to apply to array elements (or NULL)

 RETURNS	: Nothing

 NOTES		: Eliminates all elements in the array.
*****************************************************************************/
{
	USC_PCHUNK psCurr, psNext;

	if (psArray == NULL)
		return;

	for (psCurr = psArray->psFirst; psCurr != NULL; psCurr = psNext)
	{
		/* Free the elements */
		if (fnFree != NULL)
		{
			IMG_UINT32 uIdx;
			for (uIdx = 0; uIdx < psArray->uChunk; uIdx ++)
			{
				IMG_PVOID* ppvElem = ChunkElem(psCurr->pvArray, psArray->uSize, uIdx);
				ASSERT(psArray->uSize == sizeof(IMG_PVOID));
				fnFree(psState, ppvElem);
				*ppvElem = NULL;
			}
		}
		/* Free the chunk */
		psNext = psCurr->psNext;
		FreeChunk(psState, &psCurr);
	}
	psArray->psFirst = NULL;
	SetArraySize(psArray, 0);

	psArray->sMemo.pvData = NULL;
}

IMG_INTERNAL
IMG_VOID FreeArray(PINTERMEDIATE_STATE psState,
				   USC_PARRAY *ppsArray)
/*****************************************************************************
 FUNCTION	: FreeGraph

 PURPOSE	: Free an extendable array

 PARAMETERS	: psState	- Compiler state.
			  psArray	- Array to free

 RETURNS	: Nothing
*****************************************************************************/
{
	USC_PARRAY psArray;

	if (ppsArray == NULL)
		return;

	psArray = *ppsArray;
	if (psArray == NULL)
		return;

	ClearArray(psState, psArray, NULL);
	UscFree(psState, psArray);
	*ppsArray = NULL;
}

IMG_INTERNAL
IMG_VOID InitArray(USC_PARRAY psArray,
				   IMG_UINT32 uChunk,
				   IMG_PVOID pvDefault,
				   IMG_UINT32 uSize)
/*****************************************************************************
 FUNCTION	: InitArray

 PURPOSE	: Initialise an extendable array

 PARAMETERS	: psArray	- Array to initialise
			  uChunk	- Chunk size (must be even)
			  pvDefaul	- Default value
			  uSize		- Size in bytes of each element

 RETURNS	: Nothing
*****************************************************************************/
{
	if (psArray == NULL)
		return;

	SetArraySize(psArray, 0);
	psArray->uChunk = uChunk;
	psArray->uSize = uSize;
	psArray->pvDefault = pvDefault;
	psArray->psFirst = NULL;

	psArray->sMemo.pvData = NULL;
}

IMG_INTERNAL
USC_PARRAY NewArray(PINTERMEDIATE_STATE psState,
					IMG_UINT32 uChunk,
					IMG_PVOID pvDefault,
					IMG_UINT32 uSize)
/*****************************************************************************
 FUNCTION	: NewArray

 PURPOSE	: Allocate and initialise an extendable array

 PARAMETERS	: psState	- Compiler state.
			  uChunk	- Chunk size (must be even)
			  pvDefaul	- Default value
			  uSize		- Size in bytes of each element

 RETURNS	: A new array

 NOTES		: Initial size of created array is max(uNum, 1)
*****************************************************************************/
{
	/* Allocate and initialise the toplevel array */
	USC_PARRAY psArray = UscAlloc(psState, sizeof(USC_ARRAY));

	InitArray(psArray, uChunk, pvDefault, uSize);

	return psArray;
}
static
USC_PCHUNK ArrayChunkGet(PINTERMEDIATE_STATE psState,
						 USC_PMEMO psMemo,
						 USC_PCHUNK *ppsFirst,
						 IMG_UINT32 uIdx,
						 IMG_UINT32 uIdxInc,
						 IMG_UINT32 uChunkSize,
						 IMG_PVOID pvDefault,
						 IMG_BOOL bExtend)
/*****************************************************************************
 FUNCTION	: ArrayChunkGet

 PURPOSE	: Get a chunk in an array to set an element, extending if necessary.

 PARAMETERS	: psState	- Compiler state.
			  psArray	- Extendable array
			  uIdx		- Index of the element
			  bExtend	- Whether to create new chunks if element isn't present.

 RETURNS	: The updated array or NULL on allocation failure.
*****************************************************************************/
{
	USC_PCHUNK psCurr, psStart, psChunk, psLast, psNextLink;
	USC_PCHUNK psNext = NULL, psPrev = NULL;
	IMG_UINT32 uChunkIdx, uNextIdx;
	IMG_BOOL bBack = IMG_FALSE;		// Whether to search backwards through the list
	USC_PCHUNK psFirst;

	ASSERT(ppsFirst != NULL);
	psFirst = *ppsFirst;

	/* Initial settings */
	psStart = psFirst;

	/* Try the memo */
	if (psMemo != NULL &&
		psMemo->pvData != NULL)
	{
		psStart = (USC_PCHUNK)psMemo->pvData;
		if (uIdx >= psStart->uIndex)
		{
			/* Memo points to a chunk that precedes required element */
			bBack = IMG_FALSE;
		}
		else
		{
			bBack = IMG_TRUE;
		}
	}

	/* Search the list of arrays */
	psChunk = NULL;
	psLast = NULL;

	for(psCurr = psStart;
		psCurr != NULL;
		psCurr = psNextLink)
	{
		/* Get the start index from the chunk */
		uChunkIdx = psCurr->uIndex;
		uNextIdx = uChunkIdx + uIdxInc;

		/* Update pointer for last element looked at */
		psLast = psCurr;

		/* Test whether current chunk is the target */
		if (uIdx >= uChunkIdx && uIdx < uNextIdx)
		{
			psChunk = psCurr;
			break;
		}

		/* Test whether to keep looking */
		if (bBack)
		{
			/* Test for end of search */
			if (uIdx >= uChunkIdx)
			{
				psNext = psCurr->psNext;
				psPrev = psCurr;
				break;
			}
			else
			{
				/* Point to next in list */
				psNextLink = psCurr->psPrev;
			}
		}
		else
		{
			/* Test for end of search */
			if (uIdx < uChunkIdx)
			{
				psNext = psCurr;
				psPrev = psCurr->psPrev;
				break;
			}
			else
			{
				/* Point to next in list */
				psNextLink = psCurr->psNext;
			}
		}
	}

	/* Create a final chunk */
	if (psChunk == NULL)
	{
		if (!bExtend)
		{
			/* Extending the array is forbidden, return NULL */
			return NULL;
		}

		/* Test whether end of list was reached */
		if (psCurr == NULL)
		{
			/* Got to end of list */
			if (bBack)
				psNext = psLast;
			else
				psPrev = psLast;
		}

		/* Create new chunk and link it into the list */
		psChunk = NewChunk(psState, uChunkSize, pvDefault);
		psChunk->psNext = psNext;
		if (psNext != NULL)
		{
			psNext->psPrev = psChunk;
		}
		psChunk->psPrev = psPrev;
		if (psPrev != NULL)
		{
			psPrev->psNext = psChunk;
		}

		/* Update the list pointer in the array if necessary */
		if (psChunk->psPrev == NULL)
		{
			*ppsFirst = psChunk;
		}

		uChunkIdx = (uIdx - (uIdx % uIdxInc));
		psChunk->uIndex = uChunkIdx;
	}
	psCurr = psChunk;

	/* Memoise the access */
	if (psMemo != NULL)
	{
		psMemo->pvData = psCurr;
	}

	return psCurr;
}

static
IMG_PVOID* BaseArrayGet(PINTERMEDIATE_STATE psState, USC_PARRAY psArray, IMG_UINT32 uIdx)
/*****************************************************************************
 FUNCTION	: BaseArrayGet

 PURPOSE	: Get an element.

 PARAMETERS	: psArray	- Extendable array
			  uIdx		- Index of the element

 RETURNS	: The element or NULL if no such element
*****************************************************************************/
{
	USC_PCHUNK psCurr;
	IMG_PVOID *psRet;
	IMG_UINT32 uChunkIdx;

	/* Check for error conditions */
	if (psArray == NULL)
	{
		return NULL;
	}

	if (uIdx >= ((GetArraySize(psArray) * psArray->uChunk) + psArray->uChunk))
	{
		/* Index out of bounds */
		return NULL;
	}

	/* Try the memo */
	if (psArray->sMemo.pvData != NULL)
	{
		psCurr = (USC_PCHUNK)psArray->sMemo.pvData;

		if (psCurr != NULL &&
			uIdx >= psCurr->uIndex &&
			uIdx < (psCurr->uIndex + psArray->uChunk))
		{
			/* Already got the required chunk */
			goto GetElement;
		}
	}

	/* Look for the required chunk */
	psCurr = ArrayChunkGet(psState,
						   &psArray->sMemo,
						   &psArray->psFirst,
						   uIdx,
						   psArray->uChunk,
						   psArray->uChunk * psArray->uSize,
						   psArray->pvDefault,
						   IMG_FALSE);
	if (psCurr == NULL)
	{
		return NULL;
	}

	/* Extract the element from the chunk */
  GetElement:
	uChunkIdx = psCurr->uIndex;
	uIdx = uIdx - uChunkIdx;
	psRet = ChunkElem(psCurr->pvArray, psArray->uSize, uIdx);

	return psRet;
}

IMG_INTERNAL
IMG_PVOID ArrayGet(PINTERMEDIATE_STATE psState, USC_PARRAY psArray, IMG_UINT32 uIdx)
/*****************************************************************************
 FUNCTION	: ArrayGet

 PURPOSE	: Get the value of an element.

 PARAMETERS	: psArray	- Extendable array
			  uIdx		- Index of the element

 RETURNS	: The element or NULL if no such element
*****************************************************************************/
{
	IMG_PVOID* ppsElem = BaseArrayGet(psState, psArray, uIdx);

	if (ppsElem != NULL)
	{
		if (psArray->uSize == sizeof(IMG_PVOID))
		{
			return *ppsElem;
		}
		else
		{
			/* x64 case */
			IMG_UINTPTR_T uiTemp = *(IMG_PUINT32)ppsElem;
			return (IMG_PVOID)uiTemp;
		}
	}

	return psArray->pvDefault;
}

static
IMG_PVOID* BaseArraySet(PINTERMEDIATE_STATE psState,
						USC_PARRAY psArray,
						IMG_UINT32 uIdx,
						IMG_BOOL bExtend)
/*****************************************************************************
 FUNCTION	: BaseArraySet

 PURPOSE	: Set the value of an element, extending the array if necessary.

 PARAMETERS	: psState	- Compiler state.
			  psArray	- Extendable array
			  uIdx		- Index of the element
			  bExtend	- Whether to create new chunks if element isn't present.

 RETURNS	: The updated array or NULL on allocation failure.
*****************************************************************************/
{
	IMG_UINT32 uNewNum;
	IMG_PVOID* psRet;
	USC_PCHUNK psCurr;
	IMG_UINT32 uChunkIdx;

	/* Check for error conditions */
	if (psArray == NULL)
	{
		return NULL;
	}

	/* Get the new array size */
	uNewNum = max(GetArraySize(psArray), (uIdx / psArray->uChunk) + 1);

	/* Try the memo */
	if (psArray->sMemo.pvData != NULL)
	{
		psCurr = (USC_PCHUNK)psArray->sMemo.pvData;

		if (psCurr != NULL &&
			uIdx >= psCurr->uIndex
			&& uIdx < (psCurr->uIndex + psArray->uChunk))
		{
			/* Already got the required chunk */
			goto GetElement;
		}
	}

	/* Look for the required chunk */
	psCurr = ArrayChunkGet(psState,
						   &psArray->sMemo,
						   &psArray->psFirst,
						   uIdx,
						   psArray->uChunk,
						   psArray->uChunk * psArray->uSize,
						   psArray->pvDefault,
						   bExtend);

	if (psCurr == NULL)
	{
		return NULL;
	}

  GetElement:
	/* Store the data */
	ASSERT(psCurr != NULL);
	ASSERT(psCurr->pvArray != NULL);

	uChunkIdx = psCurr->uIndex;
	uIdx = uIdx - uChunkIdx;
	psRet = ChunkElem(psCurr->pvArray, psArray->uSize, uIdx);

	/* Memoise the access */
	psArray->sMemo.pvData = psCurr;

	/* Record the new array size */
	SetArraySize(psArray, uNewNum);

	return psRet;
}

IMG_INTERNAL
USC_PARRAY ArraySet(PINTERMEDIATE_STATE psState,
					USC_PARRAY psArray,
					IMG_UINT32 uIdx,
					IMG_PVOID pvData)
/*****************************************************************************
 FUNCTION	: ArraySet

 PURPOSE	: Set the value of an element, extending the array if necessary.

 PARAMETERS	: psState	- Compiler state.
			  psArray	- Extendable array
			  uIdx		- Index of the element
			  uData		- Data to set

 RETURNS	: The updated array or NULL on allocation failure.
*****************************************************************************/
{
	IMG_PVOID* psElem;
	IMG_BOOL bExtend = IMG_TRUE;

	/* Test whether to extend the array */
	if (pvData == psArray->pvDefault)
		bExtend = IMG_FALSE;

	/* Get the indexed element, extending the array if necessary */
	psElem = BaseArraySet(psState, psArray, uIdx, bExtend);
	if (psElem == NULL)
	{
		if (bExtend)
		{
			/* Something went wrong */
			return NULL;
		}
		else
		{
			/* Element isn't present */
			return psArray;
		}
	}

	if (psArray->uSize == sizeof(IMG_PVOID))
	{
	/* Store the pointer value */
		*psElem = pvData;
	}
	else
	{
		/* x64 case where PVOID != UINT32 */
		*(IMG_PUINT32)psElem = (IMG_UINT32)(IMG_UINTPTR_T)pvData;
	}

	return psArray;
}

IMG_INTERNAL
IMG_VOID ClearVector(PINTERMEDIATE_STATE psState, USC_PVECTOR psVector)
/*****************************************************************************
 FUNCTION	: VectorClear

 PURPOSE	: Clear a vector

 PARAMETERS	: psState	- Compiler state
			  psVector	- Extendable array

 RETURNS	: Nothing

 NOTES		: Eliminates all elements in the array.
*****************************************************************************/
{
	USC_PCHUNK psCurr, psNext;

	if (psVector == NULL)
		return;

	for (psCurr = psVector->psFirst; psCurr != NULL; psCurr = psNext)
	{
		psNext = psCurr->psNext;
		if (psCurr->pvArray != NULL)
			psCurr->pvArray[0] = (IMG_PVOID) psVector;
		FreeChunk(psState, &psCurr);
	}
	psVector->psFirst = NULL;
	SetVectorSize(psVector, 0);

	psVector->sMemo.pvData = NULL;
}

IMG_INTERNAL
IMG_VOID FreeVector(PINTERMEDIATE_STATE psState,
					USC_PVECTOR *ppsVector)
/*****************************************************************************
 FUNCTION	: FreeVector

 PURPOSE	: Free an extendable bitvector

 PARAMETERS	: psState	- Compiler state.
			  psVector	- Bitvector to free

 RETURNS	: Nothing
*****************************************************************************/
{
	USC_PVECTOR psVector;

	if (ppsVector == NULL)
		return;

	psVector = *ppsVector;
	if (psVector == NULL)
		return;

	ClearVector(psState, psVector);
	UscFree(psState, psVector);
	*ppsVector = NULL;
}

#if 0
/*****************************************************************************
 FUNCTION	: CheckVector

 PURPOSE	: Check that a vector is well formed.

 PARAMETERS	: psState	- Internal compilation state
			  psVector	- The vector to check

 RETURNS	: True or false.

 NOTES		: This is kept only as long as the extendable bitvectors are being
			  worked on.
*****************************************************************************/
IMG_INTERNAL
IMG_BOOL CheckVector(PINTERMEDIATE_STATE psState, USC_PVECTOR psVector)
{
	static const IMG_UINT32 uBitsPerInt = sizeof(IMG_UINT32) * 8;
	IMG_UINT32 uChunkIdx = 0;
	IMG_UINT32 uNextIdx = 0, uPrevIdx;
	IMG_UINT32 uChunk = 0;
	IMG_UINT32 uNum = 0;
	USC_PCHUNK psCurr;
	IMG_UINT32 uBitsPerChunk;
	IMG_UINT32 uLastChunkIdx;
	USC_PCHUNK psPrev;

	#ifdef DEBUG
	/* psState used to handle failed ASSERT on release build only */
	PVR_UNREFERENCED_PARAMETER(psState);
	#endif

	if (psVector == NULL)
		return IMG_TRUE;

	uNum = GetVectorSize(psVector);
	uChunk = psVector->uChunk;

	uBitsPerChunk = (uBitsPerInt * psVector->uChunk);
	uLastChunkIdx = (uNum * uBitsPerChunk);

	uChunkIdx = 0;
	uPrevIdx = 0;
	uNextIdx = uChunkIdx + uBitsPerChunk;

	psPrev = NULL;
	for(psCurr = psVector->psFirst; psCurr != NULL; psCurr = psCurr->psNext)
	{
		/* Get the chunk index */
		uChunkIdx = psCurr->uIndex;
		uNextIdx = uChunkIdx + uBitsPerChunk;

		/* Check the chunk index is in bounds */
		ASSERT(!(uChunkIdx > uLastChunkIdx));

		if (psCurr != psVector->psFirst)
		{
			ASSERT(uPrevIdx < uChunkIdx);
		}

		ASSERT(uChunkIdx < uNextIdx);

		/* Check the link back */
		ASSERT(psCurr->psPrev == psPrev);
		psPrev = psCurr;

		uPrevIdx = uChunkIdx;

		uNum -= 1;
		ASSERT(uNum >= 0);
	}

	return IMG_TRUE;
}
#endif

IMG_INTERNAL
IMG_VOID InitVector(USC_PVECTOR psVector,
					IMG_UINT32 uChunk,
					IMG_BOOL bDefault)
/*****************************************************************************
 FUNCTION	: InitVector

 PURPOSE	: Initialise an extendable bitvector

 PARAMETERS	: psVector	- Vector to initialise
			  uChunk	- Chunk size (must be even)
			  bDefault	- Default value for a bit in the vector

 RETURNS	: Nothing
*****************************************************************************/
{
	if (psVector == NULL)
		return;

	SetVectorSize(psVector, 0);
	psVector->bDefault = bDefault;
	psVector->uDefault = bDefault ? (~0) : 0;

	psVector->uChunk = uChunk;
	psVector->psFirst = NULL;

	psVector->sMemo.pvData = NULL;
}

IMG_INTERNAL
USC_PVECTOR NewVector(PINTERMEDIATE_STATE psState,
					  IMG_UINT32 uChunk,
					  IMG_BOOL bDefault)
/*****************************************************************************
 FUNCTION	: NewVector

 PURPOSE	: Allocate and initialise an extendable bitvector

 PARAMETERS	: psState	- Compiler state.
			  uChunk	- Chunk size (must be even)

 RETURNS	: A new bitvector
*****************************************************************************/
{
	/* Allocate and initialise the toplevel bitvector */
	USC_PVECTOR psVector = UscAlloc(psState, sizeof(USC_VECTOR));

	InitVector(psVector, uChunk, bDefault);
	return psVector;
}

IMG_INTERNAL
IMG_UINT32 VectorGet(PINTERMEDIATE_STATE psState,
					 USC_PVECTOR psVector,
					 IMG_UINT32 uBitIdx)
/*****************************************************************************
 FUNCTION	: VectorGet

 PURPOSE	: Get the nth bit in a bitvector

 PARAMETERS	: psState	- Internal compilation state
			  psVector	- Extendable bitvector
			  uIdx		- Index of the bit

 RETURNS	: The bit at index uIdx or 0 if no such bit.
*****************************************************************************/
{
	const IMG_UINT32 uBitsPerInt = sizeof(IMG_UINT32) * 8;
	USC_PCHUNK psCurr;
	IMG_UINT32 uDefault = psVector->uDefault;

	{
		const IMG_UINT32 uBitsPerChunk = (uBitsPerInt * psVector->uChunk);
		IMG_UINT32 uBitsPerVector = GetVectorSize(psVector) * uBitsPerChunk;

		/* Check for out of bounds */
		if (uBitIdx >=(uBitsPerVector + uBitsPerChunk))
		{
			return uDefault;
		}

		/* Try the memo */
		if (psVector->sMemo.pvData != NULL)
		{
			psCurr = (USC_PCHUNK)psVector->sMemo.pvData;

			if (psCurr != NULL &&
				uBitIdx >= psCurr->uIndex
				&& uBitIdx < (psCurr->uIndex + uBitsPerChunk))
			{
				/* Already got the required chunk */
				goto GetElement;
			}
		}

		/* Get the chunk */
		psCurr = ArrayChunkGet(psState,
							   &psVector->sMemo,
							   &psVector->psFirst,
							   uBitIdx,
							   uBitsPerChunk,
							   (psVector->uChunk * sizeof(IMG_UINT32)),
							   (IMG_PVOID)(IMG_UINTPTR_T)uDefault,
							   IMG_FALSE);
		if (psCurr == NULL)
		{
			return uDefault;
		}
	}

  GetElement:
	{
		/* Found chunk in which bit is held */
		//IMG_PVOID* ppvElem = ChunkElem(psCurr->pvArray, sizeof(IMG_UINT32), 0);
		IMG_UINT32 uChunkIdx = psCurr->uIndex;

		uBitIdx = uBitIdx - uChunkIdx;
		return GetBit((IMG_PUINT32)psCurr->pvArray, uBitIdx);
	}
}

static
IMG_UINT32 CalcVecOp(USC_VECTOR_OP eOp,
					 IMG_UINT32 uSrc1,
					 IMG_UINT32 uSrc2)
/*****************************************************************************
 FUNCTION	: CalcVecOp

 PURPOSE	: Calculate the result of a vector operation applied to integers

 PARAMETERS : eOp		- Operation
			  psSrc1	- First source
			  psSrc2	- Second source (ignored for unary operations)

 RETURNS	: Result of the operation
*****************************************************************************/
{
	switch (eOp)
	{
		case USC_VEC_NOT:
			return ~(uSrc1);
		case USC_VEC_AND:
			return (uSrc1 & uSrc2);
		case USC_VEC_OR:
			return (uSrc1 | uSrc2);
		case USC_VEC_ADD:
			return (uSrc1 + uSrc2);
		case USC_VEC_SUB:
			return (uSrc1 - uSrc2);

		/* Comparisons */
		case USC_VEC_EQ:
			return ((uSrc1 == uSrc2) ? (~0U) : 0U);
		case USC_VEC_DISJ:
			return (((uSrc1 & uSrc2) == 0) ? (~0U) : 0U);
		/* Default */
		default:
		{
			/* Shouldn't get here */
			return 0;
		}
	}
}

IMG_INTERNAL
USC_PARRAY ArrayElemOp(PINTERMEDIATE_STATE psState,
					   USC_PARRAY psArray,
					   USC_VECTOR_OP eOp,
					   IMG_UINT32 uIdx,
					   IMG_UINT32 uArg)
/*****************************************************************************
 FUNCTION	: ArrayElemOp

 PURPOSE	: Apply an operation to an element in an array.

 PARAMETERS	: psState	- Compiler state.
			  psArray	- Extendable array
			  eOp		- Operation
			  uIdx		- Index of the element
			  uVal		- Additional argument (ignore for unary ops)

 RETURNS	: The updated array or NULL on allocation failure.
*****************************************************************************/
{
	IMG_UINT32* psElem;
	IMG_UINT32 uVal;

	/* Get the indexed element, extending the array if necessary */
	psElem = (IMG_PUINT32)BaseArraySet(psState, psArray, uIdx, IMG_TRUE);
	ASSERT(psElem != NULL);

	/* Store the value */
	uVal = *psElem;
	*psElem = CalcVecOp(eOp, uVal, uArg);

	return psArray;
}

static
USC_PVECTOR VectorOpWorker(PINTERMEDIATE_STATE psState,
						   USC_VECTOR_OP eOp,
						   USC_PVECTOR psDest,
						   USC_PVECTOR psVec1,
						   USC_PVECTOR psVec2)
/*****************************************************************************
 FUNCTION	: VectorOpWorker

 PURPOSE	: Helper function for VectorOr

 PARAMETERS : psState 	- Compiler State
			  eOp		- Operation
			  psDest	- Destination vector
			  psSrc1	- First source vector
			  psSrc2	- Second source vector (NULL for unary operations)

 RETURNS	: psDest or NULL on error.
*****************************************************************************/
{
	/*
	  VectorOpWorker: Calculate operations on extendable arrays.

	  Supported operations calculate results to be stored in the destination
	  vector or compare the source vectors.

	  Destination is psDest. All operations except comparisons will corrupt
	  the destination vector.

	  Sources are in psVec1 and psVec2. A vector can safely be passed as both
	  source and destination vector.

	  The function carries out operations on the elements of each chunk in the
	  source vectors. The chunks are matched so that a chunk C1 from psVec1 is
	  used with chunk C2 from psVec2 iff the index of C1 is the same as the
	  index of C2 (C1->uHeader == C2->uHeader).

	  The values of missing chunks are always taken as the default for the
	  vector. For example, if (C1->uHeader < C2->uHeader) then the
	  psVec2->uDefault is used instead of the values of C2.

	  The destination vector psDest does not have to have any chunks. If
	  psDest has chunks, they are re-used if possible otherwise new chunks are
	  dynamically allocated. A chunk C from psDest is only reused if its index
	  is less than or equal to the index of the chunk being calculated
	  (C->uHeader <= min(C1->uHeader, C2->uHeader)). This ensures that the
	  chunks in a vector used as a source and the destination do not get
	  corrupted before they have been used.
	*/
	USC_PCHUNK *ppsPrev;
	IMG_UINT32 uChunkIdx;
	USC_PCHUNK psNew, psNext, psPrev;
	IMG_UINT32 uSize;
	IMG_UINT32 uChunk;
	IMG_UINT32 uDefault;
	IMG_UINT32 uNumArgs;
	USC_PCHUNK apsCurr[2] = {NULL, NULL};
	IMG_UINT32 auChunkIdx [2] = {0, 0};
	IMG_UINT32 auDefault [2] = {0, 0};
	IMG_UINT32 auSize [2] = {0, 0};
	IMG_BOOL bCompare;

	ASSERT(psDest != NULL);
	ASSERT(psVec1 != NULL);

	/* Check number of arguments and test for comparison */
	bCompare = IMG_FALSE;
	uNumArgs = 0;
	switch (eOp)
	{
		case USC_VEC_NOT:
		{
			uNumArgs = 1;
			break;
		}
		case USC_VEC_AND:
		case USC_VEC_OR:
		{
			uNumArgs = 2;
			break;
		}
		case USC_VEC_EQ:
		case USC_VEC_DISJ:
		{
			bCompare = IMG_TRUE;
			uNumArgs = 2;
			break;
		}
		default:
		{
			/* Should only be called with one of the known operations */
			imgabort();
		}
	}

	/* Initialise basic values */
	uChunk = psVec1->uChunk;

	/* Get data about the first source vector */
	auSize[0] = GetVectorSize(psVec1);
	apsCurr[0] = psVec1->psFirst;
	auDefault[0] = psVec1->uDefault;

	/* Get data about the second source vector, if it is present */
	if (uNumArgs > 1)
	{
		if (psVec2 != NULL)
		{
			auSize[1] = GetVectorSize(psVec2);
			apsCurr[1] = psVec2->psFirst;
			auDefault[1] = psVec2->uDefault;
		}
		else /* psVec2 == NULL */
		{
			imgabort();
		}
	}

	/* Calculate and set the default value */
	uDefault = CalcVecOp(eOp, auDefault[0], auDefault[1]);
	if (!bCompare)
	{
		psDest->uDefault = (uDefault == 0) ? 0 : (~0);
	}

	/*
	   Run through the chunks in the sources, forming the chunks for the
	   destination. Because source chunk may have disjoint indices, the lists
	   of chunks must be interleaved.
	*/
	ppsPrev = &psDest->psFirst;
	psNext = psDest->psFirst;
	psPrev = NULL;
	while (apsCurr[0] != NULL || apsCurr[1] != NULL)
	{
		IMG_UINT32 uCtr;
		IMG_BOOL abValid[2] = {IMG_TRUE, IMG_TRUE};
		USC_PCHUNK psSrc1, psSrc2 = NULL;
		IMG_BOOL bDropChunk = IMG_TRUE;

		/* Test for validity */
		if (apsCurr[0] == NULL)
		{
			abValid[0] = IMG_FALSE;
		}

		if (apsCurr[1] == NULL)
		{
			abValid[1] = IMG_FALSE;
		}
		/* One of the two lists must still be active */
		ASSERT(abValid[0] || abValid[1]);

		/* Get the chunk indices */
		if (abValid[0])
			auChunkIdx[0] = apsCurr[0]->uIndex;
		if (abValid[1])
			auChunkIdx[1] = apsCurr[1]->uIndex;

		/* Set the chunk index */
		if (abValid[0] && abValid[1])
			uChunkIdx = min(auChunkIdx[0], auChunkIdx[1]);
		else if (abValid[0])
			uChunkIdx = auChunkIdx[0];
		else
			uChunkIdx = auChunkIdx[1];

		/* Choose which of the lists to use */
		if (abValid[0] && auChunkIdx[0] > uChunkIdx)
		{
			abValid[0] = IMG_FALSE;
		}
		if (abValid[1] && auChunkIdx[1] > uChunkIdx)
		{
			abValid[1] = IMG_FALSE;
		}

		/* Decide which of the lists to use */
		if (abValid[0])
		{
			psSrc1 = apsCurr[0];
			if (abValid[1])
				psSrc2 = apsCurr[1];
		}
		else
		{
			psSrc1 = apsCurr[1];
		}

		/* Create the new chunk */
		psNew = NULL;
		if (!bCompare)
		{
			if (psNext != NULL)
			{
				if (psNext->uIndex <= uChunkIdx)
				{
					psNew = psNext;
					psNext = psNext->psNext;
				}
			}
			if (psNew == NULL)
			{
				psNew = NewChunk(psState,
								 (sizeof(IMG_UINT32) * uChunk),
								 (IMG_PVOID)(IMG_UINTPTR_T)uDefault);
				psNew->psNext = psNext;
			}

			/* Store the chunk index */
			psNew->uIndex = uChunkIdx;
		}

		/* Calculate the chunk data */
		for(uCtr = 0; uCtr < uChunk; uCtr ++)
		{
			IMG_UINT32 auVal[2];
			IMG_UINT32 uVal;

			/* Get the values, using the defaults if the chunk hasn't been reached */
			if (psSrc1 != NULL)
			{
				IMG_PUINT32 const puiElem = (IMG_PUINT32)ChunkElem(psSrc1->pvArray, sizeof(IMG_UINT32), uCtr);
				auVal[0] = *puiElem;
			}
			else
				auVal[0] = auDefault[0];

			if (psSrc2 != NULL)
			{
				IMG_PUINT32 const puiElem = (IMG_PUINT32)ChunkElem(psSrc2->pvArray, sizeof(IMG_UINT32), uCtr);
				auVal[1] = *puiElem;
			}
			else
				auVal[1] = auDefault[1];

			/* Calculate and store the result of the operation */
			uVal = CalcVecOp(eOp, auVal[0], auVal[1]);

			if (bCompare)
			{
				/* If comparison fails, return immediately */
				if (uVal == 0)
				{
					return NULL;
				}
			}
			else
			{
				IMG_PUINT32 const puiElem = (IMG_PUINT32)ChunkElem(psNew->pvArray, sizeof(IMG_UINT32), uCtr);
				*puiElem = uVal;
			}

			/*
			   Test whether the current chunk can be dropped
			   (all elements must be the default value).
			*/
			if (bDropChunk && uVal != uDefault)
				bDropChunk = IMG_FALSE;
		}

		/* Increment the chunk pointers */

		/* Increment the chunk pointers */
		if (abValid[0])
		{
			apsCurr[0] = apsCurr[0]->psNext;
		}
		if (abValid[1])
		{
			apsCurr[1] = apsCurr[1]->psNext;
		}

		/* If the chunk is not going to be dropped, attach it to the end of the list */
		if (!bCompare)
		{
			ASSERT(psNew->psNext == psNext);
			if (bDropChunk)
			{
				/* Dropping the chunk, re-use it for the next iteration */
				psNext = psNew;
			}
			else
			{
				/* Keeping the chunk, link it into the list */
				*ppsPrev = psNew;
				ppsPrev = &(psNew->psNext);

				psNew->psNext = NULL;
				psNew->psPrev = psPrev;
				psPrev = psNew;
			}
		}
	}

	/* A comparison that gets this far has succeeded */
	if (bCompare)
	{
		return psDest;
	}

	/* Record the index for the next chunk in the sequence*/
	uSize = max(auSize[0], auSize[1]);
	SetVectorSize(psDest, uSize);

	/* Delete any extra chunks */
	{
		USC_PCHUNK psCurr;
		for (psCurr = psNext; psCurr != NULL; psCurr = psNext)
		{
			if (psDest->psFirst == psCurr)
			{
				psDest->psFirst = psCurr->psNext;
				if (psDest->psFirst != NULL)
				{
					psDest->psFirst->psPrev = NULL;
				}
			}
			psNext = psCurr->psNext;
			FreeChunk(psState, &psCurr);
		}
	}

	return psDest;
}

IMG_INTERNAL
USC_PVECTOR VectorOp(PINTERMEDIATE_STATE psState,
					 USC_VECTOR_OP eOp,
					 USC_PVECTOR psDest,
					 USC_PVECTOR psSrc1,
					 USC_PVECTOR psSrc2)
/*****************************************************************************
 FUNCTION	: VectorOp

 PURPOSE	: Calcluate Dest = Op(Vec1, Vec2) where Op is a vector operations

 PARAMETERS	: psState			- Compiler state
			  eOp				- Operation
			  psDest			- Destination
			  psSrc1, psSrc2	- Sources

 RETURNS	: psDest or NULL on error.

 NOTES		: All graphs must be the same type.
*****************************************************************************/
{
	USC_PVECTOR psRet;
	IMG_UINT32 uChunk;
	USC_VECTOR sVector;

	/* Check for error conditions */
	if (psDest == NULL)
	{
		return NULL;
	}

	/* Check sources are compatible */
	if (psSrc2 != NULL)
	{
		if (psSrc1->uChunk != psSrc2->uChunk)
		{
			return NULL;
		}
	}
	uChunk = psSrc1->uChunk;

	/* Setup the temporary return value */
	sVector = *psDest;

	sVector.uChunk = uChunk;
	SetVectorSize(&sVector, 0);

	sVector.sMemo.pvData = NULL;

	/* Iterate through the chunks applying the operation */
	if (VectorOpWorker(psState, eOp, &sVector, psSrc1, psSrc2) != NULL)
	{
		/* Copy the new vector into the destination */
		*psDest = sVector;
		psRet = psDest;
	}
	else
	{
		/* No new vector */
		psRet = NULL;
	}

#if 0
	/* Debugging only */
	ASSERT(CheckVector(psState, psRet));
#endif

	return psRet;
}

IMG_INTERNAL
USC_PVECTOR VectorCopy(PINTERMEDIATE_STATE psState,
					   USC_PVECTOR psSrc,
					   USC_PVECTOR psDest)
/*****************************************************************************
 FUNCTION	: VectorCopy

 PURPOSE	: Copy a vector

 PARAMETERS	: psState	- Compiler state
			  psVec		- Vector to copy
			  psDest	- Destination

 RETURNS	: psDest or NULL on error.
*****************************************************************************/
{
	USC_PCHUNK psFirst;
	if (psDest == NULL || psSrc == NULL)
		return NULL;

	/* Copy the basic data */
	psFirst = psDest->psFirst;

	*psDest = *psSrc;
	psDest->sMemo.pvData = NULL;
	psDest->psFirst = NULL;

	/* Iterate through the chunks */
	{
		USC_PCHUNK psCurr, psNew, *ppsPrev, psPrev;
		const IMG_UINT32 uSize = sizeof(IMG_UINT32);
		const IMG_UINT32 uChunk = psSrc->uChunk;
		const IMG_UINT32 uDefault = psSrc->uDefault;

		ppsPrev = &psDest->psFirst;
		psPrev = NULL;
		psNew = psFirst;
		for (psCurr = psSrc->psFirst; psCurr != NULL; psCurr = psCurr->psNext)
		{
			if (psNew == NULL)
			{
				psNew = NewChunk(psState, (uChunk * uSize), (IMG_PVOID)(IMG_UINTPTR_T)uDefault);
			}

			psNew->uIndex = psCurr->uIndex;

			*ppsPrev = psNew;
			ppsPrev = &psNew->psNext;

			psNew->psPrev = psPrev;
			memcpy(psNew->pvArray, psCurr->pvArray, (uChunk * uSize));

			psPrev = psNew;
			psNew = psNew->psNext;
		}
		*ppsPrev = NULL;
		ASSERT(psPrev == NULL || psPrev->psNext == NULL);

		for (psCurr = psNew; psCurr != NULL; psCurr = psNew)
		{
			psNew = psCurr->psNext;
			FreeChunk(psState, &psCurr);
		}
	}

#if 0
	/* Debugging only */
	ASSERT(CheckVector(psState, psDest));
#endif

	return psDest;
}

IMG_INTERNAL
USC_PVECTOR VectorSet(PINTERMEDIATE_STATE psState,
					  USC_PVECTOR psVector,
					  IMG_UINT32 uBitIdx,
					  IMG_UINT32 uData)
/*****************************************************************************
 FUNCTION	: VectorSet

 PURPOSE	: Set the value of an element, extending the array if necessary.

 PARAMETERS	: psState	- Compiler state.
			  psVector	- Extendable bitvector
			  uIdx		- Index of the element
			  uData		- Data to set

 RETURNS	: The updated vector or NULL on allocation failure.
*****************************************************************************/
{
	const IMG_UINT32 uBitsPerInt = sizeof(IMG_UINT32) * 8;
	IMG_UINT32 uBitsPerChunk;
	IMG_UINT32 uChunkIdx;
	USC_PCHUNK psCurr;
	IMG_UINT32 uNewNum;
	IMG_UINT32 uDefault;

	/* Check for error conditions */
	if (psVector == NULL)
	{
		return NULL;
	}
	uBitsPerChunk = (uBitsPerInt * psVector->uChunk);
	uDefault = psVector->uDefault;

	/* Get the new array size */
	uNewNum = max(GetVectorSize(psVector), (uBitIdx / uBitsPerChunk) + 1);

	/* Try the memo */
	if (psVector->sMemo.pvData != NULL)
	{
		psCurr = (USC_PCHUNK)psVector->sMemo.pvData;

		if (psCurr != NULL &&
			uBitIdx >= psCurr->uIndex
			&& uBitIdx < (psCurr->uIndex + uBitsPerChunk))
		{
			/* Already got the required chunk */
			goto GetElement;
		}
	}

	psCurr = ArrayChunkGet(psState,
						   &psVector->sMemo,
						   &psVector->psFirst,
						   uBitIdx,
						   uBitsPerChunk,
						   (psVector->uChunk * sizeof(IMG_UINT32)),
						   (IMG_PVOID)(IMG_UINTPTR_T)uDefault,
						   IMG_TRUE);

	/* Set the element */
  GetElement:
	ASSERT(psCurr != NULL);

	uChunkIdx = psCurr->uIndex;

	/* Set the bit */
	ASSERT(psCurr != NULL);
	ASSERT(psCurr->pvArray != NULL);

	uBitIdx = uBitIdx - uChunkIdx;
	{
		IMG_PUINT32 const puiElem = (IMG_PUINT32)ChunkElem(psCurr->pvArray, sizeof(IMG_UINT32), 0);
		SetBit(puiElem, uBitIdx, uData);
	}

	/* Record the new array size */
	SetVectorSize(psVector, uNewNum);

	psVector->sMemo.pvData = NULL;

#if 0
	/* Debugging only */
	ASSERT(CheckVector(psState, psVector));
#endif
	return psVector;
}

IMG_INTERNAL
IMG_UINT32 VectorGetRange(PINTERMEDIATE_STATE psState,
						  USC_PVECTOR psVector,
						  IMG_UINT32 uEndIdx,
						  IMG_UINT32 uStartIdx)
/*****************************************************************************
 FUNCTION	: VectorGetRange

 PURPOSE	: Get a range of bits in a bitvector

 PARAMETERS	: psState	- Compiler state
			  psVector	- Extendable bitvector
			  uEndIdx	- Index of the last bit
			  uStartIdx	- Index of the first bit

 RETURNS	: The specified slice of the bitvector

 NOTES		: The range must be no larger than 32 bits.
*****************************************************************************/
{
	static const IMG_UINT32 uNumWords = 2;
	const IMG_UINT32 uBitsPerChunk = (BITS_PER_UINT * psVector->uChunk);
	const IMG_UINT32 uSize = sizeof(IMG_UINT32);
	USC_PCHUNK apsChunk[2] = {NULL, NULL};
	IMG_UINT32 auChunkIdx[2] = {0, 0};
	IMG_UINT32 uDefault = psVector->uDefault;
	IMG_UINT32 auBitIdx[2];
	IMG_UINT32 auBitArr[2] = {0, 0};
	IMG_UINT32 auWordIdx[2] = {0, 0};
	IMG_UINT32 uCtr;

	/* Check for error conditions */
	ASSERT(!(uEndIdx < uStartIdx));

	/* Initialise values */
	auBitIdx[0] = uEndIdx;
	auBitIdx[1] = uStartIdx;


	/* Get the chunks for the start and end bits */
	for (uCtr = 0; uCtr < uNumWords; uCtr ++)
	{
		/* Get the chunk */
		apsChunk[uCtr] = ArrayChunkGet(psState,
									   &psVector->sMemo,
									   &psVector->psFirst,
									   auBitIdx[uCtr],
									   uBitsPerChunk,
									   (psVector->uChunk * uSize),
									   (IMG_PVOID)(IMG_UINTPTR_T)uDefault,
									   IMG_FALSE);
	}
	
	ASSERT(uSize == sizeof(IMG_UINT32));

	/* Get indices, pointers and values from the chunks */
	for (uCtr = 0; uCtr < uNumWords; uCtr ++)
	{
		if (apsChunk[uCtr] != NULL)
		{
			/* Get the chunk index and the bit index in the chunk */
			auChunkIdx[uCtr] = apsChunk[uCtr]->uIndex;
			auBitIdx[uCtr] -= auChunkIdx[uCtr];
			ASSERT(auBitIdx[uCtr] < uBitsPerChunk);

			/* Calculate the word in the chunk with the required bit */
			auWordIdx[uCtr] = auBitIdx[uCtr] / BITS_PER_UINT;
			auBitArr[uCtr] = *(IMG_PUINT32)ChunkElem(apsChunk[uCtr]->pvArray, uSize, auWordIdx[uCtr]);
		}
		else
		{
			auChunkIdx[uCtr] = auBitIdx[uCtr] - (auBitIdx[uCtr] % uBitsPerChunk);
			auBitArr[uCtr] = uDefault;

			auBitIdx[uCtr] -= auChunkIdx[uCtr];
			auWordIdx[uCtr] = auBitIdx[uCtr] / BITS_PER_UINT;
		}
	}

	/* Get the range from the bit array */
	{
		IMG_UINT32 uFirst, uLast;
		ASSERT(auWordIdx[0] >= auWordIdx[1]);

		uFirst = auBitIdx[0] - (auWordIdx[0] * BITS_PER_UINT);
		uFirst += (BITS_PER_UINT * (auWordIdx[0] - auWordIdx[1]));

		uLast = auBitIdx[1] - (auWordIdx[1] * BITS_PER_UINT);
		return GetRange(auBitArr, uFirst, uLast);
	}
}

static
IMG_PUINT32 VectorGetElement(PINTERMEDIATE_STATE	psState,
							 USC_PVECTOR			psVector,
							 IMG_UINT32				uElemIdx,
							 IMG_BOOL				bExtend)
{
	IMG_UINT32	uNewNum;
	USC_PCHUNK	psChunk;
	IMG_UINT32	uElemIdxInChunk;
	IMG_UINT32	uDefault = psVector->uDefault;
	const IMG_UINT32 uSize = sizeof(IMG_UINT32);

	/* Calculate new size */
	uNewNum = max(GetVectorSize(psVector), uElemIdx + 1);

	/* Record the new array size */
	SetVectorSize(psVector, uNewNum);

	/* Get the chunk containing the element to modify. */
	psChunk = ArrayChunkGet(psState,
						    &psVector->sMemo,
							&psVector->psFirst,
							uElemIdx * BITS_PER_UINT,
							psVector->uChunk * BITS_PER_UINT,
							(psVector->uChunk * uSize),
							(IMG_PVOID)(IMG_UINTPTR_T)uDefault,
							 bExtend);

	/*
		In this case we are writing a previously unwritten chunk
		with the default data which is returned when a previously
		unwritten chunk is read so there is nothing more to do.
	*/
	if (psChunk == NULL)
	{
		ASSERT(!bExtend);
		return NULL;
	}

	/*
		Get the index of the element in the chunk.
	*/
	uElemIdxInChunk = uElemIdx % psVector->uChunk;

	ASSERT(uSize == sizeof(IMG_UINT32));
	
	/*
		Return a pointer to the element.
	*/
	return (IMG_PUINT32)ChunkElem(psChunk->pvArray, uSize, uElemIdxInChunk);
}

IMG_INTERNAL
USC_PVECTOR VectorSetRange(PINTERMEDIATE_STATE psState,
						   USC_PVECTOR psVector,
						   IMG_UINT32 uEndIdx,
						   IMG_UINT32 uStartIdx,
						   IMG_UINT32 uData)
/*****************************************************************************
 FUNCTION	: VectorSetRange

 PURPOSE	: Set the value of an element, extending the array if necessary.

 PARAMETERS	: psState	- Compiler state.
			  psVector	- Extendable bitvector
			  uEndIdx	- Index of the last bit
			  uStartIdx	- Index of the first bit
			  uData		- Data to set

 RETURNS	: The updated vector or NULL on failure.

 NOTES		: The range must be no larger than 32 bits.
*****************************************************************************/
{
	IMG_UINT32	uStartIdxInElem;
	IMG_UINT32	uRangeMask;
	IMG_BOOL	bExtend;
	IMG_UINT32	uDefault = psVector->uDefault;
	IMG_UINT32	uStartElemIdx;
	IMG_UINT32	uEndElemIdx;
	IMG_UINT32	uRangeLength;
	IMG_PUINT32	puElem;

	/*
		Get the length of the range of set.
	*/
	uRangeLength = uEndIdx + 1 - uStartIdx;
	
	/*
		Create a mask with every bit in the range set.
	*/
	if (uRangeLength < BITS_PER_UINT)
	{
		uRangeMask = (1U << uRangeLength) - 1U;
	}
	else
	{
		uRangeMask = 0xFFFFFFFFUL;
	}

	/*
		Calculate the elements in the range covers.
	*/
	uStartElemIdx = uStartIdx / BITS_PER_UINT;
	uEndElemIdx = uEndIdx / BITS_PER_UINT;

	/*
		Get the start of the range relative to the element.
	*/
	uStartIdxInElem = uStartIdx % BITS_PER_UINT;

	/* Test whether the vector needs to extended */
	bExtend = IMG_TRUE;
	if (uData == uDefault)
		bExtend = IMG_FALSE;

	/*
		Get the element in the vector we want to modify.
	*/
	puElem = VectorGetElement(psState, psVector, uStartElemIdx, bExtend);

	/*
		Check for not needing to modify a never written element.
	*/
	if (puElem != NULL)
	{
		/*
			Clear the existing contents of the range.
		*/
		(*puElem) &= ~(uRangeMask << uStartIdxInElem);

		/*
			Set the new range.
		*/
		(*puElem) |= ((uData & uRangeMask) << uStartIdxInElem);
	}

	/*
		Check for a range spanning between two elements;
	*/
	if (uStartElemIdx != uEndElemIdx)
	{
		IMG_PUINT32	puElem2;

		ASSERT((uStartElemIdx + 1) == uEndElemIdx);

		/*
			Get the second element to modify.
		*/
		puElem2 = VectorGetElement(psState, psVector, uEndElemIdx, bExtend);

		if (puElem2 != NULL)
		{
			IMG_UINT32	uElem2Overhang = BITS_PER_UINT - uStartIdxInElem;

			(*puElem2) &= ~(uRangeMask >> uElem2Overhang);
			(*puElem2) |= ((uData & uRangeMask) >> uElem2Overhang);
		}
	}

#if 0
	/* Debugging only */
	ASSERT(CheckVector(psState, psVector));
#endif
	return psVector;
}

IMG_INTERNAL
USC_PVECTOR VectorOrRange(PINTERMEDIATE_STATE psState,
						  USC_PVECTOR psVector,
						  IMG_UINT32 uEndIdx,
						  IMG_UINT32 uStartIdx,
						  IMG_UINT32 uData)
/*****************************************************************************
 FUNCTION	: VectorOrRange

 PURPOSE	: Bitwise-or a range in a vector together with supplied data.

 PARAMETERS	: psState	- Compiler state.
			  psVector	- Extendable bitvector
			  uEndIdx	- Index of the last bit
			  uStartIdx	- Index of the first bit
			  uData		- Data to OR.

 RETURNS	: The updated vector or NULL on failure.

 NOTES		: The range must be no larger than 32 bits.
*****************************************************************************/
{
	IMG_UINT32	uStartIdxInElem;
	IMG_UINT32	uRangeMask;
	IMG_BOOL	bExtend;
	IMG_UINT32	uDefault = psVector->uDefault;
	IMG_UINT32	uStartElemIdx;
	IMG_UINT32	uEndElemIdx;
	IMG_UINT32	uRangeLength;
	IMG_PUINT32	puElem;

	/*
		Get the length of the range of set.
	*/
	uRangeLength = uEndIdx + 1 - uStartIdx;

	/*
		Create a mask with every bit in the range set.
	*/
	if (uRangeLength < BITS_PER_UINT)
	{
		uRangeMask = (1U << uRangeLength) - 1U;
	}
	else
	{
		uRangeMask = 0xFFFFFFFFUL;
	}

	/*
		Zero bits in the data outside the range to set.
	*/
	uData &= uRangeMask;

	/*
		Calculate the elements in the range covers.
	*/
	uStartElemIdx = uStartIdx / BITS_PER_UINT;
	uEndElemIdx = uEndIdx / BITS_PER_UINT;

	/*
		Get the start of the range relative to the element.
	*/
	uStartIdxInElem = uStartIdx % BITS_PER_UINT;

	/* Test whether the vector needs to extended */
	bExtend = IMG_TRUE;
	if (uData == uDefault)
		bExtend = IMG_FALSE;

	/*
		Get the element in the vector we want to modify.
	*/
	puElem = VectorGetElement(psState, psVector, uStartElemIdx, bExtend);

	/*
		Check for not needing to modify a never written element.
	*/
	if (puElem != NULL)
	{
		/*
			Or in the new data.
		*/
		(*puElem) |= (uData << uStartIdxInElem);
	}

	/*
		Check for a range spanning between two elements;
	*/
	if (uStartElemIdx != uEndElemIdx)
	{
		IMG_PUINT32	puElem2;

		ASSERT((uStartElemIdx + 1) == uEndElemIdx);

		/*
			Get the second element to modify.
		*/
		puElem2 = VectorGetElement(psState, psVector, uEndElemIdx, bExtend);

		if (puElem2 != NULL)
		{
			IMG_UINT32	uElem2Overhang = BITS_PER_UINT - uStartIdxInElem;

			(*puElem2) |= (uData >> uElem2Overhang);
		}
	}

#if 0
	/* Debugging only */
	ASSERT(CheckVector(psState, psVector));
#endif
	return psVector;
}

IMG_INTERNAL
USC_PVECTOR VectorAndRange(PINTERMEDIATE_STATE psState,
						   USC_PVECTOR psVector,
						   IMG_UINT32 uEndIdx,
						   IMG_UINT32 uStartIdx,
						   IMG_UINT32 uData)
/*****************************************************************************
 FUNCTION	: VectorAndRange

 PURPOSE	: Bitwise-and a range in a vector together with supplied data.

 PARAMETERS	: psState	- Compiler state.
			  psVector	- Extendable bitvector
			  uEndIdx	- Index of the last bit
			  uStartIdx	- Index of the first bit
			  uData		- Data to AND.

 RETURNS	: The updated vector or NULL on failure.

 NOTES		: The range must be no larger than 32 bits.
*****************************************************************************/
{
	IMG_UINT32	uStartIdxInElem;
	IMG_UINT32	uRangeMask;
	IMG_BOOL	bExtend;
	IMG_UINT32	uDefault = psVector->uDefault;
	IMG_UINT32	uStartElemIdx;
	IMG_UINT32	uEndElemIdx;
	IMG_UINT32	uRangeLength;
	IMG_PUINT32	puElem;

	/*
		Get the length of the range of set.
	*/
	uRangeLength = uEndIdx + 1 - uStartIdx;

	/*
		Create a mask with every bit in the range set.
	*/
	if (uRangeLength < BITS_PER_UINT)
	{
		uRangeMask = (1U << uRangeLength) - 1U;
	}
	else
	{
		uRangeMask = 0xFFFFFFFFUL;
	}

	/*
		Calculate the elements in the range covers.
	*/
	uStartElemIdx = uStartIdx / BITS_PER_UINT;
	uEndElemIdx = uEndIdx / BITS_PER_UINT;

	/*
		Get the start of the range relative to the element.
	*/
	uStartIdxInElem = uStartIdx % BITS_PER_UINT;

	/* Test whether the vector needs to extended */
	bExtend = IMG_TRUE;
	if (uData == uDefault)
		bExtend = IMG_FALSE;

	/*
		Get the element in the vector we want to modify.
	*/
	puElem = VectorGetElement(psState, psVector, uStartElemIdx, bExtend);

	/*
		Check for not needing to modify a never written element.
	*/
	if (puElem != NULL)
	{
		IMG_UINT32	uElemData;

		uElemData = uData << uStartIdxInElem;
		uElemData |= ~(uRangeMask << uStartIdxInElem);

		(*puElem) &= uElemData;
	}

	/*
		Check for a range spanning between two elements;
	*/
	if (uStartElemIdx != uEndElemIdx)
	{
		IMG_PUINT32	puElem2;

		ASSERT((uStartElemIdx + 1) == uEndElemIdx);

		/*
			Get the second element to modify.
		*/
		puElem2 = VectorGetElement(psState, psVector, uEndElemIdx, bExtend);

		if (puElem2 != NULL)
		{
			IMG_UINT32	uElem2Overhang = BITS_PER_UINT - uStartIdxInElem;
			IMG_UINT32	uElemData;

			uElemData = uData >> uElem2Overhang;
			uElemData |= ~(uRangeMask >> uElem2Overhang);

			(*puElem2) &= uElemData;
		}
	}

#if 0
	/* Debugging only */
	ASSERT(CheckVector(psState, psVector));
#endif
	return psVector;
}


#if (_MSC_VER >= 1400)
#include <intrin.h>

#pragma intrinsic(_BitScanForward)
#endif /* (_MSC_VER >= 1400)  && ! defined(UNDER_CE) */

static
IMG_UINT32 FirstSetBit(IMG_UINT32 v)
/*****************************************************************************
 FUNCTION	: FirstSetBit

 PURPOSE	: Returns the first set bit in a dword

 PARAMETERS	: v		- Find the first set bit in this dword (must be non-zero).

 RETURNS	: The (zero-based) index of the first set bit.
*****************************************************************************/
{
#if defined(LINUX)

	return ffs(v) - 1;

#else /* defined(LINUX) */

#if (_MSC_VER >= 1400)

	IMG_UINT32	uBitPos;

	_BitScanForward((unsigned long *)&uBitPos, v);
	return uBitPos;

#else /* (_MSC_VER >= 1400) && ! defined(UNDER_CE) */


	/*
		"Using de Bruijn Sequences to Index a 1 in a Computer Word"
		(http://graphics.stanford.edu/~seander/bithacks.html)
	*/

	static const int MultiplyDeBruijnBitPosition[32] = 
	{
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};
	return MultiplyDeBruijnBitPosition[(((v & -(IMG_INT32)v) * 0x077CB531U)) >> 27];


#endif /* (_MSC_VER >= 1400)  */

#endif /* defined(LINUX) */
}

IMG_INTERNAL
IMG_UINT32 FindFirstSetBitInRange(IMG_UINT32 const * puArr, IMG_UINT32 uStart, IMG_UINT32 uCount)
/*****************************************************************************
 FUNCTION	: FindFirstSetBitInRange

 PURPOSE	: Returns the first set bit in a subrange of a bit array.

 PARAMETERS	: puArr			- Bit array.
			  uStart		- Start of the range to check.
			  uCount		- Length of the range.

 RETURNS	: The index relative to the start of the range.
*****************************************************************************/
{
	IMG_UINT32	uStartElem = uStart / BITS_PER_UINT;
	IMG_UINT32	uStartOffsetInElem = uStart % BITS_PER_UINT;
	IMG_UINT32	uEnd = uStart + uCount;
	IMG_UINT32	uEndElem = uEnd / BITS_PER_UINT;
	IMG_UINT32	uEndElemCount = uEnd % BITS_PER_UINT;
	IMG_UINT32	uEndElemMask = (1 << uEndElemCount) - 1;
	IMG_UINT32	uElem;
	IMG_UINT32	uElemBitOffset = 0;

	/*
		Check any uint only partially overlapped by the start of the range.
	*/
	if (uStartOffsetInElem != 0)
	{
		IMG_UINT32	uStartValue = puArr[uStartElem];

		/*
			Check if the range also ends within the same uint.
		*/
		if (uStartElem == uEndElem)
		{
			uStartValue &= uEndElemMask;
		}

		/*
			Shift off bits not included in the range.
		*/
		uStartValue >>= uStartOffsetInElem;

		if (uStartValue != 0)
		{
			return FirstSetBit(uStartValue);
		}

		/*
			Check for a range which only covers one uint.
		*/
		if (uStartElem == uEndElem)
		{
			return USC_UNDEF;
		}
		uStartElem++;
		uElemBitOffset = BITS_PER_UINT - uStartOffsetInElem;
	}

	/*
		Check uints which are completely covered by the range.
	*/
	for (uElem = uStartElem; uElem < uEndElem; uElem++, uElemBitOffset += BITS_PER_UINT)
	{
		IMG_UINT32	uElemValue = puArr[uElem];

		if (uElemValue != 0)
		{
			return FirstSetBit(uElemValue) + uElemBitOffset;
		}
	}

	/*
		Check any uint only partially overlapped by the end of the range.
	*/
	if (uEndElemCount != 0)
	{
		IMG_UINT32	uEndValue;
		uEndValue = puArr[uEndElem];
		uEndValue &= uEndElemMask;
		if (uEndValue != 0)
		{
			return FirstSetBit(uEndValue) + uElemBitOffset;
		}
	}
	return USC_UNDEF;
}

static
IMG_VOID VectorIteratorStep(PVECTOR_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: VectorIteratorStep

 PURPOSE	: Move to the range at or after the current position containing some set bits.

 PARAMETERS	: psIterator	- Iterator to move.

 RETURNS	: Nothing.
*****************************************************************************/
{
	while (psIterator->psCurrentChunk != NULL)
	{
		IMG_PUINT32	puCurrentArray = (IMG_PUINT32)psIterator->psCurrentChunk->pvArray;

		/*
			Check if any bits are set in the current dword.
		*/
		if (psIterator->uCurrentBitPos != 0)
		{
			if (psIterator->uCurrentBitPos < BITS_PER_UINT)
			{
				IMG_UINT32	uCurrentDword;

				uCurrentDword = puCurrentArray[psIterator->uCurrentDwordPos];
				uCurrentDword >>= psIterator->uCurrentBitPos;
				if (uCurrentDword != 0)
				{
					IMG_UINT32	uNextBit;

					uNextBit = FirstSetBit(uCurrentDword);
					psIterator->uCurrentBitPos += uNextBit & ~(psIterator->uStep - 1);
					return;
				}
			}
			psIterator->uCurrentDwordPos++;
			psIterator->uCurrentBitPos = 0;
		}

		/*
			Check if there are any dwords with set bits in the rest of the current chunk.
		*/
		while (psIterator->uCurrentDwordPos < psIterator->psVector->uChunk)
		{
			IMG_UINT32	uCurrentDword = puCurrentArray[psIterator->uCurrentDwordPos];

			if (uCurrentDword != 0)
			{
				IMG_UINT32	uNextBit;

				uNextBit = FirstSetBit(uCurrentDword);
				psIterator->uCurrentBitPos = uNextBit & ~(psIterator->uStep - 1);
				return;
			}
			psIterator->uCurrentDwordPos++;
		}

		psIterator->psCurrentChunk = psIterator->psCurrentChunk->psNext;
		psIterator->uCurrentDwordPos = 0;
	}
}

IMG_INTERNAL
IMG_VOID VectorIteratorInitialize(PINTERMEDIATE_STATE	psState, 
								  USC_PVECTOR			psVector, 
								  IMG_UINT32			uStep, 
								  PVECTOR_ITERATOR		psIterator)
/*****************************************************************************
 FUNCTION	: VectorIteratorInitialize

 PURPOSE	: Initialize an iterator for the regions in a bit vector containing
			  some set bits.

 PARAMETERS	: psState		- Compiler state.
			  psVector		- Vector to iterate over.
			  uStep			- Size (in bits) of the regions to iterate over.
							This must be a power of two and less than or equal to 32.
			  psIterator	- Iterator to initialize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(uStep > 0);
	ASSERT(uStep <= BITS_PER_UINT);
	ASSERT((uStep & (uStep - 1)) == 0);

	psIterator->uCurrentDwordPos = 0;
	psIterator->uCurrentBitPos = 0;
	psIterator->psCurrentChunk = psVector->psFirst;
	psIterator->uStep = uStep;
	psIterator->psVector = psVector;
	VectorIteratorStep(psIterator);
}

IMG_INTERNAL
IMG_UINT32 VectorIteratorCurrentPosition(PVECTOR_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: VectorIteratorCurrentPosition

 PURPOSE	: Returns the current position of the iterator.

 PARAMETERS	: psIterator	- Iterator to check.

 RETURNS	: The current position (in bits).
*****************************************************************************/
{
	return psIterator->psCurrentChunk->uIndex + psIterator->uCurrentDwordPos * BITS_PER_UINT + psIterator->uCurrentBitPos;
}

IMG_INTERNAL
IMG_UINT32 VectorIteratorCurrentMask(PVECTOR_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: VectorIteratorCurrentMask

 PURPOSE	: Returns the bit range at the current position of the iterator.

 PARAMETERS	: psIterator	- Iterator to check.

 RETURNS	: The mask.
*****************************************************************************/
{
	IMG_UINT32	uCurrentDword;

	uCurrentDword = ((IMG_PUINT32)psIterator->psCurrentChunk->pvArray)[psIterator->uCurrentDwordPos];
	uCurrentDword >>= psIterator->uCurrentBitPos;
	if (psIterator->uStep < BITS_PER_UINT)
	{
		uCurrentDword &= ((1 << psIterator->uStep) - 1);
	}
	return uCurrentDword;
}

IMG_INTERNAL
IMG_BOOL VectorIteratorContinue(PVECTOR_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: VectorIteratorContinue

 PURPOSE	: Check if there are any more ranges containing set bits at or
			  after the current position of the iterator.

 PARAMETERS	: psIterator	- Iterator to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	return psIterator->psCurrentChunk != NULL;
}

IMG_INTERNAL
IMG_VOID VectorIteratorNext(PVECTOR_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: VectorIteratorNext

 PURPOSE	: Move to the next range containing some set bits.

 PARAMETERS	: psIterator	- Iterator to move.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psIterator->uCurrentBitPos += psIterator->uStep;
	VectorIteratorStep(psIterator);
}

IMG_INTERNAL
USC_PGRAPH NewGraph(PINTERMEDIATE_STATE psState,
					IMG_UINT32 uChunk,
					IMG_PVOID pvDefault,
					USC_GRAPH_FLAGS eType)
/*****************************************************************************
 FUNCTION	: NewGraph

 PURPOSE	: Allocate and initialise an interference graph

 PARAMETERS	: psState	- Compiler state.
			  uChunk	- Chunk size (must be even)
			  eType		- Properties of the graph

 RETURNS	: A new graph

 NOTES		: Initial size of created graph is max(uNum, 1)
*****************************************************************************/
{
	USC_PGRAPH psGraph;

	uChunk = min(USC_MIN_ARRAY_CHUNK, uChunk);

	psGraph = UscAlloc(psState, sizeof(USC_GRAPH));
	psGraph->eType = eType;
	psGraph->uChunk = uChunk;
	psGraph->pvDefault = pvDefault;
	psGraph->psArray = NULL;

	return psGraph;
}

IMG_INTERNAL
IMG_VOID ClearGraph(PINTERMEDIATE_STATE psState, USC_PGRAPH psGraph)
/*****************************************************************************
 FUNCTION	: ClearGraph

 PURPOSE	: Clear a graph, removing all elements.

 PARAMETERS	: psState	- Compiler state.
			  psGraph	- Graph to free

 RETURNS	: Nothing.
*****************************************************************************/
{
	USC_PARRAY psArray;

	if (psGraph == NULL)
		return ;

	psArray = psGraph->psArray;
	if (psArray != NULL)
	{
		ClearArray(psState, psGraph->psArray, (USC_STATE_FREEFN)FreeVector);
		FreeArray(psState, &psGraph->psArray);
	}
}

IMG_INTERNAL
IMG_VOID FreeGraph(PINTERMEDIATE_STATE psState,
				   USC_PGRAPH *ppsGraph)
/*****************************************************************************
 FUNCTION	: FreeGraph

 PURPOSE	: Free an interference graph

 PARAMETERS	: psState	- Compiler state.
			  ppsGraph	- Graph to free

 RETURNS	: Nothing.
*****************************************************************************/
{
	USC_PGRAPH psGraph;

	if (ppsGraph == NULL)
		return;
	psGraph = *ppsGraph;
	if (psGraph == NULL)
		return;

	ClearGraph(psState, psGraph);
	UscFree(psState, psGraph);

	*ppsGraph = NULL;
}

IMG_INTERNAL
IMG_BOOL GraphGet(PINTERMEDIATE_STATE psState,
				  USC_PGRAPH psGraph,
				  IMG_UINT32 uReg1, IMG_UINT32 uReg2)
/*****************************************************************************
 FUNCTION	: GraphGet

 PURPOSE	: Test whether two registers interfere

 PARAMETERS	: psGraph		- Inteference graph
			  uReg1, uReg2	- Registers to test

 RETURNS	: Whether uReg1 and uReg2 interfere
*****************************************************************************/
{
	IMG_UINT32 uLarge = uReg1, uSmall = uReg2;

	if ((psGraph->eType & GRAPH_REFL) != 0 && (uReg1 == uReg2))
	{
		/* Reflexive graphs: R(x, x) = true */
		return IMG_TRUE;
	}

	if ((psGraph->eType & GRAPH_SYM) != 0 && uReg1 < uReg2)
	{
		/* Symmetric graphs: R(x, y) = R(y, x) */
		uLarge = uReg2;
		uSmall = uReg1;
	}

	/* Get element, if any */
	if (psGraph->psArray != NULL)
	{
		USC_PVECTOR *ppsElem = (USC_PVECTOR*)BaseArrayGet(psState, psGraph->psArray, uLarge);
		if (ppsElem)
		{
			USC_PVECTOR psVector = *ppsElem;
			if (psVector && VectorGet(psState, psVector, uSmall)) return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID GraphSet(PINTERMEDIATE_STATE psState,
				  USC_PGRAPH psGraph,
				  IMG_UINT32 uReg1,
				  IMG_UINT32 uReg2,
				  IMG_BOOL bSet)
/*****************************************************************************
 FUNCTION	: GraphSet

 PURPOSE	: Set whether two registers interfere

 PARAMETERS	: psState		- Compiler state
			  psGraph		- Inteference graph
			  uReg1, uReg2	- Registers
			  bSet			- Whether uReg1 and uReg2 interfere

 RETURNS	: Nothing

 Notes		: Expands the graph if necessary.
*****************************************************************************/
{
	IMG_UINT32 uLarge = uReg1, uSmall = uReg2;

	if ((psGraph->eType & GRAPH_REFL) != 0 && (uReg1 == uReg2))
	{
		/* Reflexive graphs: R(x, x) = true */
		return;
	}

	if ((psGraph->eType & GRAPH_SYM) != 0 && uReg1 < uReg2)
	{
		/* Symmetric graphs: R(x, y) = R(y, x) */
		uLarge = uReg2;
		uSmall = uReg1;
	}

	{
		USC_PVECTOR *ppsElem;
		USC_PVECTOR psVector;

		/* Create the array if necessary */
		if (psGraph->psArray == NULL)
		{
			IMG_UINT32 uChunkSize = min(USC_MIN_ARRAY_CHUNK, psGraph->uChunk);

			psGraph->psArray = NewArray(psState,
										uChunkSize,
										NULL,
										sizeof(USC_PVECTOR));
		}

		/* Get the vector, extending the array if necessary */
		ppsElem = (USC_PVECTOR*)BaseArraySet(psState, psGraph->psArray, uLarge, IMG_TRUE);

		ASSERT(ppsElem != NULL);
		psVector = *ppsElem;

		/* Test whether to create the bitvector */
		if (psVector == NULL)
		{
			psVector = NewVector(psState, USC_MIN_VECTOR_CHUNK, IMG_FALSE);
			/* Store the bitvector */
			*ppsElem = (IMG_PVOID)psVector;
		}

		/* Set the value in the bitvector */
		if (bSet)
		{
			VectorSet(psState, psVector, uSmall, 1);
		}
		else
		{
			VectorSet(psState, psVector, uSmall, 0);
		}
	}
}

IMG_INTERNAL
IMG_VOID GraphColRef(PINTERMEDIATE_STATE psState,
					 USC_PGRAPH psGraph,
					 IMG_UINT32 uCol,
					 USC_PVECTOR *ppsVector)
/*****************************************************************************
 FUNCTION	: GraphColRef

 PURPOSE	: Get a reference to a column in a graph

 PARAMETERS	: psGraph	- Inteference graph
			  uCol		- Column to get
			  ppsVector	- Vector to store the reference in.

 RETURNS	: Nothing

 NOTES		: If column doesn't exists, *ppsVector is NULL.
*****************************************************************************/
{
	USC_PVECTOR psCol = NULL;

	/* Get the array */
	if (psGraph->psArray != NULL)
	{
		psCol = (USC_PVECTOR)ArrayGet(psState, psGraph->psArray, uCol);
	}

	*ppsVector = psCol;
}

IMG_INTERNAL
IMG_VOID GraphDupCol(PINTERMEDIATE_STATE psState,
					 USC_PGRAPH psGraph,
					 IMG_UINT32 uSrc,
					 IMG_UINT32 uDst)
/*****************************************************************************
 FUNCTION	: GraphDupCol

 PURPOSE	: Duplicate a column in the graph

 PARAMETERS	: psState	- Compiler state
			  psGraph	- Inteference graph
			  uSrc		- Column to copy
			  uDst		- Column to overwrite

 RETURNS	: Nothing

 NOTES		: Destination column is cleared before being overwritten
*****************************************************************************/
{
	USC_PVECTOR psSrc, psDst = NULL, *ppsDst;

	if (psGraph == NULL)
		return;
	if (psGraph->psArray == NULL)
		return;

	/* Get the source vector */
	psSrc = (USC_PVECTOR)ArrayGet(psState, psGraph->psArray, uSrc);

	/*
	   If the destination vector already exists, clear it and zero out its
	   position in the array.
	*/
	ppsDst = (USC_PVECTOR*)BaseArrayGet(psState, psGraph->psArray, uDst);
	if (ppsDst != NULL)
	{
		psDst = *ppsDst;
	}

	if (psSrc == NULL)
	{
		/* Make sure that the destination is also zero'd out. */
		if (ppsDst != NULL)
			FreeVector(psState, ppsDst);

		/* Nothing left to do */
		return;
	}

	/* Clear or create the destination vector if needed */
	if (psDst == NULL)
	{
		psDst = NewVector(psState, USC_MIN_VECTOR_CHUNK, IMG_FALSE);

		/* Should now have a destination vector */
		ASSERT(psDst != NULL);
	}
	else
	{
		/* Clear the vector if necessary */
		ClearVector(psState, psDst);
	}

	/* Make a deep copy of the source */
	VectorCopy(psState, psSrc, psDst);

	/* Store the destination vector in the graph */
	if (ppsDst != NULL)
	{
		/* Already have a pointer to the element so use it */
		*ppsDst = psDst;
	}
	else
	{
		/* Do it the long way */
		psGraph->psArray = ArraySet(psState, psGraph->psArray,
									uDst, (IMG_PVOID)psDst);
	}
}

IMG_INTERNAL
IMG_VOID GraphClearRow(PINTERMEDIATE_STATE psState,
					   USC_PGRAPH psGraph,
					   IMG_UINT32 uRow)
/*****************************************************************************
 FUNCTION	: GraphClearRow

 PURPOSE	: Reset a row of values to 0

 PARAMETERS	: psState	- Compiler State
			  psGraph	- Inteference graph
			  uRow		- Row to clear

 RETURNS	: Nothing

 NOTES		: Copy is shallow
*****************************************************************************/
{
	if(psGraph == NULL)
		return;
	if(psGraph->psArray == NULL)
		return;

	{
		const IMG_UINT32 uChunk = psGraph->psArray->uChunk;
		const IMG_UINT32 uSize = psGraph->psArray->uSize;
		USC_PCHUNK psCurr;
		
		ASSERT(uSize == sizeof(IMG_PVOID));

		for (psCurr = psGraph->psArray->psFirst; psCurr != NULL; psCurr = psCurr->psNext)
		{
			IMG_UINT32 uCtr;
			USC_PVECTOR psCol;

			for (uCtr = 0; uCtr < uChunk; uCtr ++)
			{
				psCol = (USC_PVECTOR)(*ChunkElem(psCurr->pvArray, uSize, uCtr));
				VectorSet(psState, psCol, uRow, 0);
			}
		}

	}
}

IMG_INTERNAL
IMG_VOID GraphClearCol(PINTERMEDIATE_STATE psState,
					   USC_PGRAPH psGraph,
					   IMG_UINT32 uCol)
/*****************************************************************************
 FUNCTION	: GraphClearCol

 PURPOSE	: Reset a column of values to 0

 PARAMETERS	: psState	- Compiler State
			  psGraph	- Inteference graph
			  uCol		- Column to clear

 RETURNS	: Nothing

 NOTES		: Copy is shallow
*****************************************************************************/
{
	if(psGraph == NULL)
		return;
	if(psGraph->psArray == NULL)
		return;

	{
		USC_PVECTOR* ppsElem = (USC_PVECTOR*)BaseArrayGet(psState, psGraph->psArray, uCol);

		if (ppsElem != NULL)
			FreeVector(psState, ppsElem);
	}
}

IMG_INTERNAL
IMG_VOID GraphOrCol(PINTERMEDIATE_STATE psState,
					USC_PGRAPH psGraph,
					IMG_UINT32 uCol,
					USC_PVECTOR psVector)
/*****************************************************************************
 FUNCTION	: GraphOrCol

 PURPOSE	: Bitwise or a column with a vector.

 PARAMETERS	: psState	- Compiler State
			  psGraph	- Inteference graph
			  uCol		- Column to bitwise or
			  psVector	- The other vector

 RETURNS	: Nothing
*****************************************************************************/
{
	if(psGraph == NULL || psVector == NULL)
		return;
	if(psGraph->psArray == NULL)
		return;

	{
		USC_PVECTOR psCol;
		USC_PVECTOR* ppsElem = (USC_PVECTOR*)BaseArrayGet(psState, psGraph->psArray, uCol);

		if (ppsElem != NULL)
		{
			IMG_BOOL bEmptyCol = IMG_FALSE;
			psCol = *ppsElem;

			/* Bitwise or valid vectors */
			if (psCol == NULL)
				bEmptyCol = IMG_TRUE;
			else if (psCol->uChunk == 0)
				bEmptyCol = IMG_TRUE;

			if (bEmptyCol)
			{
				/* No column, so copy and store source */
				psCol = NewVector(psState, 0, IMG_FALSE);
				VectorCopy(psState, psVector, psCol);
				*ppsElem = psCol;
			}
			else
			{
				USC_PVECTOR psNewCol = psCol;

				psNewCol = VectorOp(psState, USC_VEC_OR, psNewCol, psCol, psVector);

				*ppsElem = psNewCol;
			}
		}
		else if (psVector != NULL)	/* ppsElem == NULL but psVector != NULL */
		{
			/* No column in the graph, copy the source */
			psCol = NewVector(psState, 0, IMG_FALSE);
			VectorCopy(psState, psVector, psCol);

			psGraph->psArray = ArraySet(psState, psGraph->psArray,
										uCol, (IMG_PVOID)psCol);
		}
	}
}

IMG_INTERNAL
IMG_VOID GraphCopy(PINTERMEDIATE_STATE psState,
				   USC_PGRAPH psSrc,
				   USC_PGRAPH psDst)
/*****************************************************************************
 FUNCTION	: GraphCopy

 PURPOSE	: Copy a graph.

 PARAMETERS	: psSrc		- Graph to copy
			  psDst		- Graph to copy into.

 RETURNS	: Nothing
*****************************************************************************/
{
	if (psSrc == NULL || psDst == NULL)
	{
		ASSERT(psSrc != NULL && psDst != NULL);
		return;
	}

	/* Shallow copy but zero out pointers. */
	*psDst = *psSrc;
	psDst->psArray = NULL;

	/* Done if there are no columns */
	if (psSrc->psArray == NULL)
	{
		return;
	}

	/* Run through the array elements, copying each column */
	{
		const IMG_UINT32 uChunk = psSrc->psArray->uChunk;
		const IMG_UINT32 uSize = psSrc->psArray->uSize;
		const IMG_PVOID pvDefault = psSrc->psArray->pvDefault;
		USC_PCHUNK psCurr, psNew, *ppsPrev, psPrev;

		/* Form the new array */
		psDst->psArray = NewArray(psState,
								  uChunk,
								  pvDefault,
								  uSize);
		SetArraySize(psDst->psArray, GetArraySize(psSrc->psArray));

		/* Run through the array chunks */
		psPrev = NULL;
		ppsPrev = &psDst->psArray->psFirst;
		for (psCurr = psSrc->psArray->psFirst;
			 psCurr != NULL;
			 psCurr = psCurr->psNext)
		{
			USC_PVECTOR psCol;
			IMG_UINT32 uCtr;

			/* Create a new chunk and link it into the new list */
			psNew = NewChunk(psState, (uSize * uChunk), pvDefault);
			*ppsPrev = psNew;
			ppsPrev = &psNew->psNext;

			psNew->psPrev = psPrev;
			psPrev = psNew;

			/* Copy the chunk index */
			psNew->uIndex = psCurr->uIndex;
			
			ASSERT(uSize == sizeof(IMG_PVOID));

			/* Copy the vectors in the chunk */
			for(uCtr = 0; uCtr < uChunk; uCtr ++)
			{
				USC_PVECTOR psNewCol;
				IMG_PVOID* ppvElem = ChunkElem(psCurr->pvArray, uSize, uCtr);

				ASSERT (uSize == sizeof(IMG_PVOID *));
				psCol = (USC_PVECTOR)(*ppvElem);
				if (psCol != NULL)
				{
					IMG_PVOID *ppvNewElem = ChunkElem(psNew->pvArray, uSize, uCtr);
					psNewCol = NewVector(psState, psCol->uChunk, IMG_FALSE);
					VectorCopy(psState, psCol, psNewCol);
					*ppvNewElem = psNewCol;
				}
			}
		}
	}

	/* All done */
}

#if defined(DEBUG)
IMG_INTERNAL
IMG_BOOL VectorMatch(PINTERMEDIATE_STATE psState,
						const USC_VECTOR *psSrc1,
						const USC_VECTOR *psSrc2)
/*****************************************************************************
 FUNCTION	: VectorMatch

 PURPOSE	: Copy a vector

 PARAMETERS	: psState	- Compiler state
			  psSrc1	- Vector source 1
			  psSrc2	- Vector source 2

 RETURNS	: psSrc2 or NULL on error.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	
	if( !psSrc1 && !psSrc2 )
		return IMG_TRUE;
	
	if((!psSrc1) || (!psSrc2))
		return IMG_FALSE;
	
	/* Iterate through the chunks */
	{
		USC_PCHUNK psCurr1, psCurr2;
		const IMG_UINT32 uSize = sizeof(IMG_UINT32);
		const IMG_UINT32 uChunk = psSrc1->uChunk;

		for (psCurr1 = psSrc1->psFirst,
			 psCurr2 = psSrc2->psFirst; 
			 psCurr1 != NULL &&
			 psCurr2 != NULL; 
			 psCurr1 = psCurr1->psNext,
			 psCurr2 = psCurr2->psNext)
		{
			/* mem cmp */
			if (memcmp(psCurr2->pvArray, psCurr1->pvArray, (uChunk * uSize)) != 0)
			{
				return IMG_FALSE;
			}
		}
		
		if(psCurr2 || psCurr1)
		{
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL GraphMatch(PINTERMEDIATE_STATE psState,
						const USC_GRAPH *psSrc1,
						const USC_GRAPH *psSrc2)
/*****************************************************************************
 FUNCTION	: GraphMatch

 PURPOSE	: Copy a graph.

 PARAMETERS	: psSrc1		- Graph to copy
			  psSrc2		- Graph to copy into.

 RETURNS	: Nothing
*****************************************************************************/
{
	if( !psSrc1 && !psSrc2 )
	{
		return IMG_TRUE;
	}
	
	if((!psSrc1) || (!psSrc2))
		return IMG_FALSE;
	
	if((!psSrc1->psArray) || (!psSrc2->psArray))
	{
		if(psSrc1->psArray) return IMG_FALSE;
		if(psSrc2->psArray) return IMG_FALSE;
		
		return ((psSrc1->uChunk == psSrc2->uChunk) && (psSrc1->eType == psSrc2->eType)) ? IMG_TRUE : IMG_FALSE;
	}

	/* Run through the array elements, comparing each column */
	{
		const IMG_UINT32 uChunk = psSrc1->psArray->uChunk;
		const IMG_UINT32 uSize = psSrc1->psArray->uSize;
		USC_PCHUNK psCurr1, psCurr2;

		/* Run through the array chunks */
		for (psCurr1 = psSrc1->psArray->psFirst,
			 psCurr2 = psSrc2->psArray->psFirst;
			 psCurr1 != NULL &&
			 psCurr2 != NULL;
			 psCurr1 = psCurr1->psNext,
			 psCurr2 = psCurr2->psNext)
		{
			USC_PVECTOR psCol1, psCol2;
			IMG_UINT32 uCtr;

			ASSERT(uSize == sizeof(IMG_PVOID));

			for(uCtr = 0; uCtr < uChunk; uCtr ++)
			{
				IMG_PVOID *ppvElem1, *ppvElem2;
				
				ppvElem1 = ChunkElem(psCurr1->pvArray, uSize, uCtr);
				psCol1 = (USC_PVECTOR)(*ppvElem1);
				
				ppvElem2 = ChunkElem(psCurr2->pvArray, uSize, uCtr);
				psCol2 = (USC_PVECTOR)(*ppvElem2);
				
				/* Do vector compare */
				if(VectorMatch(psState, psCol1, psCol2) == IMG_FALSE)
				{
					return IMG_FALSE;
				}
			}
		}
		
		if(psCurr2 || psCurr1)
		{
			/* Found one short */
			return IMG_FALSE;
		}
	}
	
	return IMG_TRUE;
}
#endif

IMG_INTERNAL
USC_PMATRIX NewMatrix(PINTERMEDIATE_STATE psState,
					  IMG_UINT32 uChunk,
					  IMG_PVOID pvDefault,
					  USC_MATRIX_FLAGS eType)
/*****************************************************************************
 FUNCTION	: NewMatrix

 PURPOSE	: Allocate and initialise an interference matrix

 PARAMETERS	: psState	- Compiler state.
			  uChunk	- Chunk size (must be even)
			  eType		- Properties of the matrix

 RETURNS	: A new matrix

 NOTES		: Initial size of created matrix is max(uNum, 1)
*****************************************************************************/
{
	USC_PMATRIX psMatrix;

	uChunk = min(USC_MIN_ARRAY_CHUNK, uChunk);

	psMatrix = UscAlloc(psState, sizeof(USC_MATRIX));
	psMatrix->eType = eType;
	psMatrix->uChunk = uChunk;
	psMatrix->pvDefault = pvDefault;
	psMatrix->psArray = NULL;

	return psMatrix;
}

IMG_INTERNAL
IMG_VOID ClearMatrixCol(PINTERMEDIATE_STATE psState,
						USC_PMATRIX psMatrix,
						IMG_UINT32 uCol)
/*****************************************************************************
 FUNCTION	: ClearMatrixCol

 PURPOSE	: Clear a matrix column removing all elements.

 PARAMETERS	: psState	- Compiler state.
			  psMatrix	- Matrix to free
			  uCol		- column to clear

 RETURNS	: Nothing.
*****************************************************************************/
{
	USC_PARRAY psArray;

	if (psMatrix == NULL)
		return ;

	psArray = psMatrix->psArray;
	if (psArray != NULL)
	{
		USC_PARRAY *ppsCol, psCol;
		ppsCol = (USC_PARRAY*)BaseArrayGet(psState, psArray, uCol);
		if (ppsCol != NULL)
		{
			psCol = *ppsCol;
			FreeArray(psState, &psCol);
			*ppsCol = NULL;
		}
	}
}

IMG_INTERNAL
IMG_VOID ClearMatrix(PINTERMEDIATE_STATE psState,
					 USC_PMATRIX psMatrix)
/*****************************************************************************
 FUNCTION	: ClearMatrix

 PURPOSE	: Clear a matrix, removing all elements.

 PARAMETERS	: psState	- Compiler state.
			  psMatrix	- Matrix to free

 RETURNS	: Nothing.
*****************************************************************************/
{
	USC_PARRAY psRow;

	if (psMatrix == NULL)
		return ;

	psRow = psMatrix->psArray;
	if (psRow != NULL)
	{
		ClearArray(psState, psMatrix->psArray, (USC_STATE_FREEFN)FreeArray);
		FreeArray(psState, &psMatrix->psArray);
	}
}

IMG_INTERNAL
IMG_VOID FreeMatrix(PINTERMEDIATE_STATE psState,
					USC_PMATRIX *ppsMatrix)
/*****************************************************************************
 FUNCTION	: FreeMatrix

 PURPOSE	: Free an interference matrix

 PARAMETERS	: psState	- Compiler state.
			  psMatrix	- Matrix to free

 RETURNS	: Nothing.
*****************************************************************************/
{
	USC_PMATRIX psMatrix;

	if (ppsMatrix == NULL)
		return ;
	psMatrix = *ppsMatrix;
	if (psMatrix == NULL)
		return ;

	ClearMatrix(psState, psMatrix);
	UscFree(psState, psMatrix);
	*ppsMatrix = NULL;
}

IMG_INTERNAL
IMG_PVOID MatrixGet(PINTERMEDIATE_STATE psState,
					USC_PMATRIX psMatrix,
					IMG_UINT32 uReg1,
					IMG_UINT32 uReg2)
/*****************************************************************************
 FUNCTION	: MatrixGet

 PURPOSE	: Test whether two registers interfere

 PARAMETERS	: psMatrix		- Inteference matrix
			  uReg1, uReg2	- Registers to test

 RETURNS	: Whether uReg1 and uReg2 interfere
*****************************************************************************/
{
	IMG_UINT32 uLarge = uReg1, uSmall = uReg2;

	if ((psMatrix->eType & MATRIX_SYM) != 0 && uReg1 < uReg2)
	{
		/* Symmetric matrixs: R(x, y) = R(y, x) */
		uLarge = uReg2;
		uSmall = uReg1;
	}

	/* Get element, if any */
	if (psMatrix->psArray != NULL)
	{
		USC_PARRAY psCol = (USC_PARRAY)ArrayGet(psState, psMatrix->psArray, uLarge);

		if (psCol != NULL)
			return ArrayGet(psState, psCol, uSmall);
	}

	return psMatrix->pvDefault;
}

IMG_INTERNAL
IMG_VOID MatrixSet(PINTERMEDIATE_STATE psState,
				   USC_PMATRIX psMatrix,
				   IMG_UINT32 uReg1,
				   IMG_UINT32 uReg2,
				   IMG_PVOID psData)
/*****************************************************************************
 FUNCTION	: MatrixSet

 PURPOSE	: Set whether two registers interfere

 PARAMETERS	: psState		- Compiler state
			  psMatrix		- Inteference matrix
			  uReg1, uReg2	- Registers
			  bSet			- Whether uReg1 and uReg2 interfere

 RETURNS	: Nothing

 Notes		: Expands the matrix if necessary.
*****************************************************************************/
{
	IMG_UINT32 uLarge = uReg1, uSmall = uReg2;

	if ((psMatrix->eType & MATRIX_SYM) != 0 && uReg1 < uReg2)
	{
		/* Symmetric matrixs: R(x, y) = R(y, x) */
		uLarge = uReg2;
		uSmall = uReg1;
	}

	{
		USC_PARRAY psCol;
		IMG_UINT32 uChunkSize = min(USC_MIN_ARRAY_CHUNK, psMatrix->uChunk);

		/* Get the array, creating it if necessary */
		if (psMatrix->psArray == NULL)
		{
			psMatrix->psArray = NewArray(psState,
										 uChunkSize,
										 NULL,
										 sizeof(USC_PARRAY));
		}

		psCol = (USC_PARRAY)ArrayGet(psState, psMatrix->psArray, uLarge);
		if (psCol == NULL)
		{
			/* Need to extend the array */
			psCol = NewArray(psState, uChunkSize,
							 psMatrix->pvDefault, sizeof(IMG_PVOID));

			/* Store the new vector */
			psMatrix->psArray = ArraySet(psState, psMatrix->psArray,
										 uLarge, (IMG_PVOID)psCol);
		}
		ArraySet(psState, psCol, uSmall, psData);
	}
}


typedef struct _SORT_ITEM_
{
	USC_LIST_ENTRY	sListEntry;
	IMG_PVOID		pvData;
} SORT_ITEM, *PSORT_ITEM;

static
IMG_VOID InsertInSortedList(PUSC_LIST		psList,
							USC_COMPARE_FN	pfnCompare,
							PSORT_ITEM		psItemToAdd)
/*****************************************************************************
 FUNCTION	: InsertInSortedList

 PURPOSE	: Insert an entry into a linked list; keeping the list sorted.

 PARAMETERS	: psList		- List to insert into.
			  pfnCompare    - Comparison function
              psItemToAdd	- Item to insert.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psEntry;

	for (psEntry = psList->psHead; psEntry != NULL; psEntry = psEntry->psNext)
	{
		PSORT_ITEM	psExistingItem;

		psExistingItem = IMG_CONTAINING_RECORD(psEntry, PSORT_ITEM, sListEntry);
		if (pfnCompare(psItemToAdd->pvData, psExistingItem->pvData) < 0)
		{
			break;
		}
	}
	if (psEntry == NULL)
	{
		AppendToList(psList, &psItemToAdd->sListEntry);
	}
	else
	{
		InsertInList(psList, &psItemToAdd->sListEntry, psEntry);
	}
}

IMG_INTERNAL
IMG_VOID InsertInListSorted(PUSC_LIST					psList,
							USC_LIST_ENTRY_COMPARE_FN	pfnCompare,
							PUSC_LIST_ENTRY				psListEntryToInsert)
/*****************************************************************************
 FUNCTION	: InsertInListSorted

 PURPOSE	: Insert a new element into an already sorted list.

 PARAMETERS	: psList		- The list to insert into.
			  pfnCompare	- Comparison function for elements of the list.
			  psListEntryToInsert
							- The element to insert.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psExistingListEntry;

	for (psExistingListEntry = psList->psHead;
		 psExistingListEntry != NULL;
		 psExistingListEntry = psExistingListEntry->psNext)
	{
		if (pfnCompare(psListEntryToInsert, psExistingListEntry) < 0)
		{
			break;
		}
	}

	if (psExistingListEntry == NULL)
	{
		AppendToList(psList, psListEntryToInsert);
	}
	else
	{
		InsertInList(psList, psListEntryToInsert, psExistingListEntry);
	}
}

IMG_INTERNAL
void InsertSortList(PUSC_LIST					psList,
					USC_LIST_ENTRY_COMPARE_FN	pfnCompare)
/*****************************************************************************
 FUNCTION	: InsertSortList

 PURPOSE	: Sort a list replacing the existing list entry links.

 PARAMETERS	: psList		- On entry the list to sort.
							  On exit replaced by the sorted list.
			  pfnCompare	- Comparison function.

 RETURNS	: Nothing.
*****************************************************************************/
{
	USC_LIST		sSortedList;
	PUSC_LIST_ENTRY	psListEntryToInsert;

	InitializeList(&sSortedList);
	while ((psListEntryToInsert = RemoveListHead(psList)) != NULL)
	{
		InsertInListSorted(&sSortedList, pfnCompare, psListEntryToInsert);
	}

	*psList = sSortedList;
}

IMG_INTERNAL
void InsertSort(PINTERMEDIATE_STATE psState,
				USC_COMPARE_FN pfnCompare,
				IMG_UINT32 uArraySize,
				IMG_PVOID* apsArray)
/*****************************************************************************
 FUNCTION	: InsertSort

 PURPOSE	: Sort an array using insertion sort

 PARAMETERS	: pfnCompare    - Comparison function
              uArraySize    - Size of array
              apsArray      - Array to sort

 RETURNS	: pvArray ordered by pfnCompare, with pvArray[0] the least element.
*****************************************************************************/
{
	USC_LIST		sList;
	PUSC_LIST_ENTRY psEntry, psNextEntry;
	IMG_UINT32 uIdx;

	if (uArraySize == 0 ||
		uArraySize == 1)
	{
		/* Base case */
		return;
	}

	/* Insert elements into list */
	InitializeList(&sList);
	for (uIdx = 0; uIdx < uArraySize; uIdx ++)
	{
		PSORT_ITEM	psItem;

		psItem = UscAlloc(psState, sizeof(*psItem));
		psItem->pvData = apsArray[uIdx];

		InsertInSortedList(&sList, pfnCompare, psItem);
	}

	/* Clear the array */
	memset(apsArray, 0, sizeof(IMG_PVOID) * uArraySize);

	/* Put the sorted list into the array */
	for (psEntry = sList.psHead, uIdx = 0; psEntry != NULL; psEntry = psNextEntry, uIdx++)
	{
		PSORT_ITEM	psItem;

		psItem = IMG_CONTAINING_RECORD(psEntry, PSORT_ITEM, sListEntry);
		apsArray[uIdx] = psItem->pvData;

		psNextEntry = psEntry->psNext;
		UscFree(psState, psItem);
	}
}

IMG_INTERNAL
IMG_VOID InitialiseAdjacencyList(PADJACENCY_LIST	psList)
/*****************************************************************************
 FUNCTION	: InitialiseAdjacencyList

 PURPOSE	: Initialize an adjacency list data structure.

 PARAMETERS	: psList		- List to initialize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psList->psFirstChunk = psList->psLastChunk = NULL;
	psList->uCountInLastChunk = 0;
}

IMG_INTERNAL
IMG_VOID DeleteAdjacencyList(PINTERMEDIATE_STATE psState, PADJACENCY_LIST	psList)
/*****************************************************************************
 FUNCTION	: DeleteAdjacencyList

 PURPOSE	: Frees memory used for adjacency list data structure.

 PARAMETERS	: psState		- Compiler state.
			  psList		- List to free.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PADJACENCY_LIST_CHUNK	psChunk, psNextChunk;

	for (psChunk = psList->psFirstChunk; psChunk != NULL; psChunk = psNextChunk)
	{
		psNextChunk = psChunk->psNext;
		UscFree(psState, psChunk);
	}
}

IMG_INTERNAL
IMG_VOID DeleteAdjacencyListIndirect(struct tagINTERMEDIATE_STATE* psState,
									 IMG_PVOID*	ppvList)
/*****************************************************************************
 FUNCTION	: DeleteAdjacencyListIndirect

 PURPOSE	: Frees memory used for adjacency list data structure.

 PARAMETERS	: psState		- Compiler state.
			  ppvList		- Contains a pointer to the list to free.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PADJACENCY_LIST	psList = (PADJACENCY_LIST)*ppvList;

	if (psList != NULL)
	{
		DeleteAdjacencyList(psState, psList);
		UscFree(psState, psList);
		*ppvList = NULL;
	}
}

IMG_INTERNAL
IMG_VOID ReplaceInAdjacencyList(PINTERMEDIATE_STATE	psState,
								PADJACENCY_LIST		psList,
								IMG_UINT32			uItemToReplace,
								IMG_UINT32			uReplacement)
/*****************************************************************************
 FUNCTION	: ReplaceInAdjacencyList

 PURPOSE	: Replaces an item in an adjacency list.

 PARAMETERS	: psState			- Compiler state.
			  psList			- List to replace in.
			  uItemToReplace	- Item to be replaced.
			  uReplacement		- Item to replace with.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PADJACENCY_LIST_CHUNK	psChunk;

	for (psChunk = psList->psFirstChunk; psChunk != NULL; psChunk = psChunk->psNext)
	{
		IMG_UINT32	uCount;
		IMG_UINT32	uIdx;

		/*
			Get the count of items in this chunk.
		*/
		uCount = (psChunk != psList->psLastChunk) ? ADJACENCY_LIST_NODES_PER_CHUNK : psList->uCountInLastChunk;

		/*
			Check each item in the chunk.
		*/
		for (uIdx = 0; uIdx < uCount; uIdx++)
		{
			if (psChunk->auNodes[uIdx] == uItemToReplace)
			{
				psChunk->auNodes[uIdx] = uReplacement;
				return;
			}
		}
	}
	imgabort();
}

IMG_INTERNAL
IMG_VOID RemoveFromAdjacencyList(PINTERMEDIATE_STATE	psState,
								 PADJACENCY_LIST		psList,
								 IMG_UINT32				uItemToRemove)
/*****************************************************************************
 FUNCTION	: RemoveFromAdjacencyList

 PURPOSE	: Removes an item from an adjacency list.

 PARAMETERS	: psState			- Compiler state.
			  psList			- List to remove the item from.
			  uItemToRemove		- Item to remove.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PADJACENCY_LIST_CHUNK	psChunk;

	for (psChunk = psList->psFirstChunk; psChunk != NULL; psChunk = psChunk->psNext)
	{
		IMG_UINT32	uCount;
		IMG_UINT32	uIdx;

		/*
			Get the count of items in this chunk.
		*/
		uCount = (psChunk != psList->psLastChunk) ? ADJACENCY_LIST_NODES_PER_CHUNK : psList->uCountInLastChunk;

		/*
			Check each item.
		*/
		for (uIdx = 0; uIdx < uCount; uIdx++)
		{
			if (psChunk->auNodes[uIdx] == uItemToRemove)
			{
				ASSERT(psList->uCountInLastChunk > 0);

				/*
					Copy the item from the end of the list over the item to be removed.
				*/
				psChunk->auNodes[uIdx] = psList->psLastChunk->auNodes[psList->uCountInLastChunk - 1];
				psList->uCountInLastChunk--;

				/*
					Have we completed emptied the last chunk.
				*/
				if (psList->uCountInLastChunk == 0)
				{
					PADJACENCY_LIST_CHUNK	psOldLastChunk = psList->psLastChunk;

					/*
						Update the count to reflect the new last chunk.
					*/
					psList->uCountInLastChunk = ADJACENCY_LIST_NODES_PER_CHUNK;

					/*
						Update the linked list to remove the last chunk.
					*/
					if (psList->psLastChunk == psList->psFirstChunk)
					{
						psList->psFirstChunk = psList->psLastChunk = NULL;
					}
					else
					{
						PADJACENCY_LIST_CHUNK	psChunkBeforeLast;

						for (psChunkBeforeLast = psList->psFirstChunk;
							 psChunkBeforeLast->psNext != psList->psLastChunk;
							 psChunkBeforeLast = psChunkBeforeLast->psNext);
						psChunkBeforeLast->psNext = NULL;

						psList->psLastChunk = psChunkBeforeLast;
					}

					/*
						Free the old last chunk.
					*/
					UscFree(psState, psOldLastChunk);
				}
				return;
			}
		}
	}
	imgabort();
}

IMG_INTERNAL
IMG_VOID AddToAdjacencyList(PINTERMEDIATE_STATE		psState,
							PADJACENCY_LIST			psList,
							IMG_UINT32				uItemToAdd)
/*****************************************************************************
 FUNCTION	: AddToAdjacencyList

 PURPOSE	: Add an item to an adjacency list

 PARAMETERS	: psState		- Compiler state.
			  psList		- List to add to.
			  uItemToAdd	- Node to node.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PADJACENCY_LIST_CHUNK	psLastChunk;

	/*
		If the last chunk in the list is full then allocate an extra item.
	*/
	if (psList->psLastChunk == NULL || psList->uCountInLastChunk == ADJACENCY_LIST_NODES_PER_CHUNK)
	{
		PADJACENCY_LIST_CHUNK	psNewLastChunk;

		psNewLastChunk = UscAlloc(psState, sizeof(*psNewLastChunk));
		psNewLastChunk->psNext = NULL;

		/*
			Add the new chunk to the end of the list.
		*/
		if (psList->psLastChunk != NULL)
		{
			psList->psLastChunk->psNext = psNewLastChunk;
		}
		else
		{
			psList->psFirstChunk = psNewLastChunk;
		}
		psList->psLastChunk = psNewLastChunk;

		psList->uCountInLastChunk = 0;
	}

	/*
		Add the new item to the last chunk.
	*/
	psLastChunk = psList->psLastChunk;
	psLastChunk->auNodes[psList->uCountInLastChunk++] = uItemToAdd;
}

IMG_INTERNAL
PADJACENCY_LIST CloneAdjacencyList(struct tagINTERMEDIATE_STATE*	psState,
								   PADJACENCY_LIST					psOldList)
/*****************************************************************************
 FUNCTION	: CloneAdjacencyList

 PURPOSE	: Duplicates all the data structures for a list.

 PARAMETERS	: psState			- Compiler state.
			  psOldList			- The list to clone.

 RETURNS	: A pointer to the new list.
*****************************************************************************/
{
	PADJACENCY_LIST			psNewList;
	PADJACENCY_LIST_CHUNK*	ppsNextPtr;
	PADJACENCY_LIST_CHUNK	psOldChunk;
	PADJACENCY_LIST_CHUNK	psNewChunk;

	if (psOldList == NULL)
	{
		return NULL;
	}

	/*
		Initialize the new list structure.
	*/
	psNewList = UscAlloc(psState, sizeof(*psNewList));
	psNewList->uCountInLastChunk = psOldList->uCountInLastChunk;

	ppsNextPtr = &psNewList->psFirstChunk;

	psNewChunk = NULL;
	for (psOldChunk = psOldList->psFirstChunk; psOldChunk != NULL; psOldChunk = psOldChunk->psNext)
	{
		IMG_UINT32	uCount;

		/*
			Allocate a new chunk.
		*/
		psNewChunk = UscAlloc(psState, sizeof(*psNewChunk));

		/*
			Set the next pointer of the previous chunk to point here.
		*/
		*ppsNextPtr = psNewChunk;
		ppsNextPtr = &psNewChunk->psNext;

		/*
			Get the count of nodes to copy.
		*/
		if (psOldChunk == psOldList->psLastChunk)
		{
			uCount = psOldList->uCountInLastChunk;
		}
		else
		{
			uCount = ADJACENCY_LIST_NODES_PER_CHUNK;
		}

		/*
			Copy the nodes from the old chunk.
		*/
		memcpy(psNewChunk->auNodes, psOldChunk->auNodes, uCount * sizeof(psOldChunk->auNodes[0]));
	}

	/*
		Update the list with a pointer to the last allocated chunk.
	*/
	*ppsNextPtr = NULL;
	psNewList->psLastChunk = psNewChunk;

	return psNewList;
}

IMG_INTERNAL IMG_UINT32 GetAdjacencyListLength(PADJACENCY_LIST psList)
/*****************************************************************************
 FUNCTION	: GetAdjacencyListLength

 PURPOSE	: Get the number of items in an adjacency list.

 PARAMETERS	: psList		- The list.

 RETURNS	: The length of the list.
*****************************************************************************/
{
	IMG_UINT32	uCount;
	PADJACENCY_LIST_CHUNK	psChunk;

	if (psList == NULL || psList->psFirstChunk == NULL)
	{
		return 0;
	}

	uCount = 0;
	for (psChunk = psList->psFirstChunk; psChunk->psNext != NULL; psChunk = psChunk->psNext)
	{
		uCount += ADJACENCY_LIST_NODES_PER_CHUNK;
	}
	uCount += psList->uCountInLastChunk;

	return uCount;
}

/********************
 * Generic data types
 ********************/

/***
 * Include the implementation code for the generic data types
 ***/
#include "uscgendata.c"

/********************
 * END Generic data types
 ********************/

/*
	Element of a REGISTER_VECTOR_MAP
*/
typedef struct _REGISTER_VECTOR_MAP_ELEMENT
{
	IMG_UINT32	uVectorLength;
	ARG			asVector[VECTOR_LENGTH];
	IMG_PVOID	pvData;
} REGISTER_VECTOR_MAP_ELEMENT, *PREGISTER_VECTOR_MAP_ELEMENT;

static
IMG_INT32 RegisterVectorMapElementCmp(IMG_PVOID pvElem1, IMG_PVOID pvElem2)
{
	PREGISTER_VECTOR_MAP_ELEMENT	psElem1 = (PREGISTER_VECTOR_MAP_ELEMENT)pvElem1;
	PREGISTER_VECTOR_MAP_ELEMENT	psElem2 = (PREGISTER_VECTOR_MAP_ELEMENT)pvElem2;
	IMG_UINT32						uVecElem;

	if (psElem1->uVectorLength != psElem2->uVectorLength)
	{
		return (IMG_INT32)(psElem1->uVectorLength - psElem2->uVectorLength);
	}
	for (uVecElem = 0; uVecElem < psElem1->uVectorLength; uVecElem++)
	{
		IMG_INT32	iCmp;

		iCmp = CompareArgs(&psElem1->asVector[uVecElem], &psElem2->asVector[uVecElem]);
		if (iCmp != 0)
		{
			return iCmp;
		}
	}

	return 0;
}

IMG_INTERNAL
PREGISTER_VECTOR_MAP IntermediateRegisterVectorMapCreate(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: IntermediateRegisterMapCreate

 PURPOSE	: Create a mapping from vectors of intermediate registers.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: The created map.
*****************************************************************************/
{
	return UscTreeMake(psState, sizeof(REGISTER_VECTOR_MAP_ELEMENT), RegisterVectorMapElementCmp);
}

IMG_INTERNAL
IMG_VOID IntermediateRegisterVectorMapDestroy(PINTERMEDIATE_STATE psState, PREGISTER_VECTOR_MAP psMap)
/*****************************************************************************
 FUNCTION	: IntermediateRegisterMapDestroy

 PURPOSE	: Destroy a mapping from vectors of intermediate registers.

 PARAMETERS	: psState		- Compiler state.
			  psMap			- Map to destroy.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UscTreeDelete(psState, psMap);
}

IMG_INTERNAL
IMG_VOID IntermediateRegisterVectorMapSet(PINTERMEDIATE_STATE	psState, 
										  PREGISTER_VECTOR_MAP	psMap, 
										  IMG_UINT32			uVectorLength, 
										  PCARG					asVector, 
										  IMG_PVOID				pvData)
/*****************************************************************************
 FUNCTION	: IntermediateRegisterMapSet

 PURPOSE	: Add an element to a map from intermediate register numbers.

 PARAMETERS	: psState		- Compiler state.
			  psMap			- Map to add to.
			  uVectorLength	- Length of the source vector.
			  asVector		- Source vector.
			  pvData		- Mapping destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	REGISTER_VECTOR_MAP_ELEMENT	sKey;	

	sKey.uVectorLength = uVectorLength;
	memcpy(sKey.asVector, asVector, sizeof(asVector[0]) * uVectorLength);
	sKey.pvData = pvData;
	UscTreeAdd(psState, psMap, &sKey);
}

IMG_INTERNAL
IMG_PVOID IntermediateRegisterVectorMapGet(PREGISTER_VECTOR_MAP psMap, IMG_UINT32 uVectorLength, PCARG asVector)
/*****************************************************************************
 FUNCTION	: IntermediateRegisterMapVectorGet

 PURPOSE	: Get an element from a map from vectors of intermediate registers.

 PARAMETERS	: psMap			- Map to query.
			  uVectorLength	- Length of the vector.
			  asVector		- Vector.

 RETURNS	: The destination of the mapping.
*****************************************************************************/
{
	PREGISTER_VECTOR_MAP_ELEMENT	psElem;
	REGISTER_VECTOR_MAP_ELEMENT		sKey;

	sKey.uVectorLength = uVectorLength;
	memcpy(sKey.asVector, asVector, sizeof(asVector[0]) * uVectorLength);
	psElem = UscTreeGetPtr(psMap, &sKey);
	if (psElem != NULL)
	{
		return psElem->pvData;
	}
	else
	{
		return NULL;
	}
}


/*
	Element of a REGISTER_MAP.
*/
typedef struct _REGISTER_MAP_ELEMENT
{
	IMG_UINT32	uRegNum;
	IMG_PVOID	pvData;
} REGISTER_MAP_ELEMENT, *PREGISTER_MAP_ELEMENT;

static
IMG_INT32 RegisterMapElementCmp(IMG_PVOID pvElem1, IMG_PVOID pvElem2)
{
	PREGISTER_MAP_ELEMENT	psElem1 = (PREGISTER_MAP_ELEMENT)pvElem1;
	PREGISTER_MAP_ELEMENT	psElem2 = (PREGISTER_MAP_ELEMENT)pvElem2;

	return (IMG_INT32)(psElem1->uRegNum - psElem2->uRegNum);
}

IMG_INTERNAL
PREGISTER_MAP IntermediateRegisterMapCreate(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: IntermediateRegisterMapCreate

 PURPOSE	: Create a mapping from intermediate register numbers.

 PARAMETERS	: psState		- Compiler state.

 RETURNS	: The created map.
*****************************************************************************/
{
	return UscTreeMake(psState, sizeof(REGISTER_MAP_ELEMENT), RegisterMapElementCmp);
}

IMG_INTERNAL
IMG_VOID IntermediateRegisterMapDestroy(PINTERMEDIATE_STATE psState, PREGISTER_MAP psMap)
/*****************************************************************************
 FUNCTION	: IntermediateRegisterMapDestroy

 PURPOSE	: Destroy a mapping from intermediate register numbers.

 PARAMETERS	: psState		- Compiler state.
			  psMap			- Map to destroy.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UscTreeDelete(psState, psMap);
}

IMG_INTERNAL
IMG_VOID IntermediateRegisterMapSet(PINTERMEDIATE_STATE psState, PREGISTER_MAP psMap, IMG_UINT32 uRegNum, IMG_PVOID pvData)
/*****************************************************************************
 FUNCTION	: IntermediateRegisterMapSet

 PURPOSE	: Add an element to a map from intermediate register numbers.

 PARAMETERS	: psState		- Compiler state.
			  psMap			- Map to add to.
			  uRegNum		- Mapping source.
			  pvData		- Mapping destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	REGISTER_MAP_ELEMENT	sKey;	

	sKey.uRegNum = uRegNum;
	sKey.pvData = pvData;
	UscTreeAdd(psState, psMap, &sKey);
}

IMG_INTERNAL
IMG_PVOID IntermediateRegisterMapGet(PREGISTER_MAP psMap, IMG_UINT32 uRegNum)
/*****************************************************************************
 FUNCTION	: IntermediateRegisterMapGet

 PURPOSE	: Get an element from a map from intermediate register numbers.

 PARAMETERS	: psMap			- Map to query.
			  uRegNum		- Mapping source.

 RETURNS	: The destination of the mapping.
*****************************************************************************/
{
	PREGISTER_MAP_ELEMENT	psElem;
	REGISTER_MAP_ELEMENT	sKey;

	sKey.uRegNum = uRegNum;
	psElem = UscTreeGetPtr(psMap, &sKey);
	if (psElem != NULL)
	{
		return psElem->pvData;
	}
	else
	{
		return NULL;
	}
}

IMG_INTERNAL
IMG_VOID IntermediateRegisterMapRemove(PINTERMEDIATE_STATE psState, PREGISTER_MAP psMap, IMG_UINT32 uRegNum)
/*****************************************************************************
 FUNCTION	: IntermediateRegisterMapRemove

 PURPOSE	: Remove an element from a map from intermediate register numbers.

 PARAMETERS	: psState		- Compiler state.
			  psMap			- Map to add to.
			  uRegNum		- Mapping source.

 RETURNS	: Nothing.
*****************************************************************************/
{
	REGISTER_MAP_ELEMENT	sKey;

	sKey.uRegNum = uRegNum;
	UscTreeRemove(psState, psMap, &sKey, NULL /* pfnIter */, NULL /* pvIterData */);
}

IMG_INTERNAL
PSPARSE_SET SparseSetAlloc(PINTERMEDIATE_STATE psState, IMG_UINT32 uMaxMemberCount)
/*****************************************************************************
 FUNCTION	: SparseSetAlloc

 PURPOSE	: Allocate and initialize a sparse set.

 PARAMETERS	: psState			- Compiler state.
			  uMaxMemberCount	- The maximum number of members of the set AND
								the maximum range of values for each member.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSPARSE_SET	psSet;

	psSet = UscAlloc(psState, sizeof(*psSet));
	psSet->puSparse = UscAlloc(psState, sizeof(psSet->puSparse[0]) * uMaxMemberCount);
	psSet->puDense = UscAlloc(psState, sizeof(psSet->puDense[0]) * uMaxMemberCount);
	psSet->uMemberCount = 0;
#if defined(VALGRIND_HEADER_PRESENT)
	if (RUNNING_ON_VALGRIND)
	{
		memset(psSet->puSparse, 0, sizeof(psSet->puSparse[0]) * uMaxMemberCount);
		memset(psSet->puDense, 0, sizeof(psSet->puDense[0]) * uMaxMemberCount);
	}
#endif /* defined(VALGRIND_HEADER_PRESENT) */
	return psSet;
}

IMG_INTERNAL
IMG_VOID SparseSetFree(PINTERMEDIATE_STATE psState, PSPARSE_SET psSet)
/*****************************************************************************
 FUNCTION	: SparseSetFree

 PURPOSE	: Free a sparse set.

 PARAMETERS	: psState		- Compiler state.
			  psSet			- Set to free.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UscFree(psState, psSet->puSparse);
	UscFree(psState, psSet->puDense);
	UscFree(psState, psSet);
}

IMG_INTERNAL
IMG_VOID SparseSetEmpty(PSPARSE_SET psSet)
/*****************************************************************************
 FUNCTION	: SparseSetEmpty

 PURPOSE	: Remove all members from a sparse set.

 PARAMETERS	: psSet			- Set to empty.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psSet->uMemberCount = 0;
}

static
IMG_VOID SparseSetBaseAddMember(PSPARSE_SET psSet, IMG_UINT32 uMemberToAdd)
/*****************************************************************************
 FUNCTION	: SparseSetBaseAddMember

 PURPOSE	: Add a member to a sparse set (without checking if it is
			  already a member).

 PARAMETERS	: psSet			- Set to modify.
			  uMemberToAdd	- Member to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psSet->puSparse[uMemberToAdd] = psSet->uMemberCount;
	psSet->puDense[psSet->uMemberCount] = uMemberToAdd;
	psSet->uMemberCount++;
}

IMG_INTERNAL
IMG_VOID SparseSetCopy(PSPARSE_SET psDest, PSPARSE_SET psSrc)
/*****************************************************************************
 FUNCTION	: SparseSetCopy

 PURPOSE	: Copy a sparse set. Both sets must have been allocated with the
			  same maximum size.

 PARAMETERS	: psDest		- Destination for the copy.
			  psSrc			- Source for the copy.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIdx;

	psDest->uMemberCount = 0;
	for (uIdx = 0; uIdx < psSrc->uMemberCount; uIdx++)
	{
		SparseSetBaseAddMember(psDest, psSrc->puDense[uIdx]);
	}
}

IMG_INTERNAL
IMG_BOOL SparseSetIsMember(PSPARSE_SET psSet, IMG_UINT32 uMember)
/*****************************************************************************
 FUNCTION	: SparseSetIsMember

 PURPOSE	: Checks if a specified integer is a member of a sparse set.

 PARAMETERS	: psSet			- Set to check.
			  uMember		- Possible set member.

 RETURNS	: TRUE if the integer is a member of the set.
*****************************************************************************/
{
	IMG_UINT32	uA = psSet->puSparse[uMember];
	if (uA < psSet->uMemberCount && psSet->puDense[uA] == uMember)
	{
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

IMG_INTERNAL
IMG_VOID SparseSetAddMember(PSPARSE_SET psSet, IMG_UINT32 uMemberToAdd)
/*****************************************************************************
 FUNCTION	: SparseSetAddMember

 PURPOSE	: Add a member to a sparse set.

 PARAMETERS	: psSet			- Set to modify.
			  uMemberToAdd	- Member to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (!SparseSetIsMember(psSet, uMemberToAdd))
	{
		SparseSetBaseAddMember(psSet, uMemberToAdd);
	}
}

IMG_INTERNAL
IMG_VOID SparseSetDeleteMember(PSPARSE_SET psSet, IMG_UINT32 uMemberToDelete)
/*****************************************************************************
 FUNCTION	: SparseSetDeleteMember

 PURPOSE	: Remove a member from a sparse set.

 PARAMETERS	: psSet				- Set to modify.
			  uMemberToDelete	- Member to remove.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (SparseSetIsMember(psSet, uMemberToDelete))
	{
		IMG_UINT32	uE = psSet->puDense[psSet->uMemberCount - 1];
		IMG_UINT32	uOldPos = psSet->puSparse[uMemberToDelete];
		
		psSet->uMemberCount--;
		psSet->puDense[uOldPos] = uE;
		psSet->puSparse[uE] = uOldPos;
	}
}

/**
 * Register replacement
 **/

/* Accessor for RegSubst element parts */
#define SubstRegType(p) ((USC_REGTYPE)(IMG_UINTPTR_T)(p->pvLeft))
#define SubstRegNum(p) ((IMG_UINT32)(IMG_UINTPTR_T)(p->pvRight))

static IMG_INT32 RegSubstCmp(IMG_PVOID pvElemA, IMG_PVOID pvElemB)
/*****************************************************************************
 FUNCTION	: RegSubstCmp

 PURPOSE	: Comparison for elements of a USC_REGSUBST tree

 PARAMETERS	: psRegA, psRegB    - Elements to compare

 RETURNS	: <0, ==0, >0 if psRegA is <, == or > psRegB
*****************************************************************************/
{
	PUSC_PAIR psElemA = (PUSC_PAIR)pvElemA;
	PUSC_PAIR psElemB = (PUSC_PAIR)pvElemB;
	PUSC_PAIR psRegA;
	PUSC_PAIR psRegB;
	USC_REGTYPE uTyA, uTyB;
	IMG_UINT32 uIntA, uIntB;

	if (psElemA == psElemB)
		return 0;
	if (psElemA == NULL)
		return (-1);
	if (psElemB == NULL)
		return 1;

	/* Extract the keys */
	psRegA = (PUSC_PAIR)(psElemA->pvLeft);
	psRegB = (PUSC_PAIR)(psElemB->pvLeft);

	/* Compare by type */
	uTyA = SubstRegType(psRegA);
	uTyB = SubstRegType(psRegB);

	if (uTyA < uTyB)
		return (-1);
	if (uTyA > uTyB)
		return 1;

	/* Compare by register number */
	uIntA = SubstRegNum(psRegA);
	uIntB = SubstRegNum(psRegB);

	if (uIntA < uIntB)
		return (-1);
	if (uIntA > uIntB)
		return 1;
	return 0;
}

static
IMG_VOID SubstElemDelete(IMG_PVOID pvIterData,
						 IMG_PVOID pvElem)
/*****************************************************************************
 FUNCTION	: SubstElemDelete

 PURPOSE	: Call-back to free a substitution element

 PARAMETERS	: pvIterData   - Compiler state
              pvElem       - Element to delete

 RETURNS	: Nothing
*****************************************************************************/
{
	USC_DATA_STATE_PTR psState = (USC_DATA_STATE_PTR)pvIterData;
	PUSC_PAIR psPair = (PUSC_PAIR)pvElem;
	PUSC_PAIR psLeft;
	PUSC_PAIR psRight;

	if (psPair == NULL)
		return;

	psLeft = (PUSC_PAIR)psPair->pvLeft;
	psRight = (PUSC_PAIR)psPair->pvRight;

	UscFree(psState, psLeft);
	UscFree(psState, psRight);
#if 0
	UscFree(psState, psPair);
#endif
}

IMG_INTERNAL
PUSC_REGSUBST RegSubstAlloc(USC_DATA_STATE_PTR psState)
/*****************************************************************************
 FUNCTION	: RegSubstAlloc

 PURPOSE	: Allocate and initialise a substitution

 PARAMETERS	: psState    - Compiler state

 RETURNS	: A newly allocated substitution
*****************************************************************************/
{
	return UscTreeMake(psState, sizeof(USC_PAIR), RegSubstCmp);
}

IMG_INTERNAL
IMG_VOID RegSubstDelete(USC_DATA_STATE_PTR psState,
						PUSC_REGSUBST psSubst)
/*****************************************************************************
 FUNCTION	: RegSubstDelete

 PURPOSE	: Free a substitution

 PARAMETERS	: psState    - Compiler state
              psSubst    - Substitution to delete

 RETURNS	: Nothing
*****************************************************************************/
{
	UscTreeDeleteAll(psState, psSubst, SubstElemDelete, (IMG_PVOID)psState);
}

IMG_INTERNAL
PUSC_REGSUBST BaseRegSubstAdd(USC_DATA_STATE_PTR psState,
							  PUSC_REGSUBST psSubst,
							  IMG_UINT32 uRegType,
							  IMG_UINT32 uRegNumber,
							  IMG_UINT32 uReplType,
							  IMG_UINT32 uReplNumber)
/*****************************************************************************
 FUNCTION	: RegSubstAdd

 PURPOSE	: Add a register replacment

 PARAMETERS	: psState    - Compiler state
              psSubst    - Substitution to add to
              uRegType   - Register to be replaced.
			  uRegNumber
              uReplType  - Replacement
			  uReplNumber

 RETURNS	: The updated substitution
*****************************************************************************/
{
	USC_PAIR sElem;
	PUSC_PAIR psKey;
	PUSC_PAIR psData;
	PUSC_PAIR psOld;

	psKey = UscAlloc(psState, sizeof(USC_PAIR));
	psData = UscAlloc(psState, sizeof(USC_PAIR));

	/* Set up the key for the tree */
	psKey->pvLeft = (IMG_PVOID)(IMG_UINTPTR_T)uRegType;
	psKey->pvRight = (IMG_PVOID)(IMG_UINTPTR_T)uRegNumber;

	/* Set up the data for the tree */
	psData->pvLeft = (IMG_PVOID)(IMG_UINTPTR_T)uReplType;
	psData->pvRight = (IMG_PVOID)(IMG_UINTPTR_T)uReplNumber;

	/* Set up the element for the tree */
	sElem.pvLeft = (IMG_PVOID)psKey;
	sElem.pvRight = (IMG_PVOID)psData;

	/* Delete the old element */
	psOld = UscTreeGetPtr(psSubst, &sElem);
	SubstElemDelete((IMG_PVOID)psState, psOld);

	/* Insert the element into the key */
	UscTreeAdd(psState, psSubst, &sElem);

	return psSubst;
}

IMG_INTERNAL
PUSC_REGSUBST RegSubstAdd(USC_DATA_STATE_PTR psState,
						  PUSC_REGSUBST psSubst,
						  PARG psReg,
						  PARG psRepl)
/*****************************************************************************
 FUNCTION	: RegSubstAdd

 PURPOSE	: Add a register replacment

 PARAMETERS	: psState    - Compiler state
              psSubst    - Substitution to add to
              psReg      - Register to be replaced.
              psRepl     - Replacement

 RETURNS	: The updated substitution
*****************************************************************************/
{
	if (psSubst == NULL || psReg == NULL)
		return psSubst;

	ASSERT(psRepl != NULL);

	return BaseRegSubstAdd(psState, psSubst, psReg->uType, psReg->uNumber, psRepl->uType, psRepl->uNumber);
}

IMG_INTERNAL

IMG_BOOL RegSubstBaseFind(PUSC_REGSUBST psSubst,
						  IMG_UINT32 uRegType,
						  IMG_UINT32 uRegNum,
						  IMG_PUINT32 puReplRegType,
						  IMG_PUINT32 puReplRegNum)
/*****************************************************************************
 FUNCTION	: RegSubstFind

 PURPOSE	: Get the replacement for a register

 PARAMETERS	: psSubst			- Substitution find in.
              uRegType			- Register to be replaced.
			  uRegNum

 OUTPUT     : puReplRegType		- Replacement or unchanged if no replacement
			  puReplRegNum

 RETURNS	: IMG_TRUE iff a replacement was found.
*****************************************************************************/
{
	USC_PAIR sElem = {NULL, NULL};
	USC_PAIR sKey = {NULL, NULL};
	PUSC_PAIR psData;
	PUSC_PAIR psElem;

	/* Assemble the key */
	sElem.pvLeft = (IMG_PVOID)(&sKey.pvLeft);

	sKey.pvLeft = (IMG_PVOID)(IMG_UINTPTR_T)(uRegType);
	sKey.pvRight = (IMG_PVOID)(IMG_UINTPTR_T)(uRegNum);

	/* Look up the data */
	psElem = UscTreeGetPtr(psSubst, &sElem);
	if (psElem == NULL)
	{
		return IMG_FALSE;
	}

	/* Got a replacement */
	psData = (PUSC_PAIR)psElem->pvRight;
	*puReplRegType = (USC_REGTYPE)(IMG_UINTPTR_T)psData->pvLeft;
	*puReplRegNum = (IMG_UINT32)(IMG_UINTPTR_T)psData->pvRight;

	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL RegSubstFind(PUSC_REGSUBST psSubst,
					  PARG psReg,
					  PARG* ppsRepl)
/*****************************************************************************
 FUNCTION	: RegSubstFind

 PURPOSE	: Get the replacement for a register

 PARAMETERS	: psState    - Compiler state
              psSubst    - Substitution to add to
              psReg      - Register to be replaced.

 OUTPUT     : ppsRepl    - Replacement or unchanged if no replacement

 RETURNS	: IMG_TRUE iff a replacement was found.
*****************************************************************************/
{
	PARG psRepl;

	if (ppsRepl == NULL)
		return IMG_FALSE;
	psRepl = *ppsRepl;
	if (psRepl == NULL)
		return IMG_FALSE;

	if (psSubst == NULL || psReg == NULL)
	{
		return IMG_FALSE;
	}

	return RegSubstBaseFind(psSubst, psReg->uType, psReg->uNumber, &psRepl->uType, &psRepl->uNumber);

}

#if defined(UF_TESTBENCH)
static
IMG_VOID SubstElemPrint(IMG_PVOID pvIterData,
						IMG_PVOID pvElem)
/*****************************************************************************
 FUNCTION	: SubstElemPrint

 PURPOSE	: Print an element

 PARAMETERS	: pvIterData - Ignored
              pvElem     - Element to print

 RETURNS	: Nothing
*****************************************************************************/
{
	PUSC_PAIR psElem = (PUSC_PAIR)pvElem;
	PUSC_PAIR psReg;
	PUSC_PAIR psRepl;
	IMG_UINT32 uRegType, uRegNum;

	PVR_UNREFERENCED_PARAMETER(pvIterData);

	if (psElem == NULL)
		return;

	psReg = (PUSC_PAIR)psElem->pvLeft;
	psRepl = (PUSC_PAIR)psElem->pvRight;

	if (psReg == NULL)
		printf("(null)");
	else
	{
		uRegType = SubstRegType(psReg);
		uRegNum =  SubstRegNum(psReg);
		if (uRegType < USC_REGTYPE_MAXIMUM)
			printf("%s%u", g_pszRegType[uRegType], uRegNum);
		else
			printf("unknown(%u)%u", uRegType, uRegNum);
	}
	printf (" --> ");

	if (psRepl == NULL)
		printf("(null)");
	else
	{
		uRegType = SubstRegType(psRepl);
		uRegNum =  SubstRegNum(psRepl);
		if (uRegType < USC_REGTYPE_MAXIMUM)
			printf("%s%u", g_pszRegType[uRegType], uRegNum);
		else
			printf("unknown(%u)%u", uRegType, uRegNum);
	}
	printf("; ");
}


IMG_INTERNAL
IMG_VOID RegSubstPrint(PINTERMEDIATE_STATE psState,
					   PUSC_REGSUBST psSubst)
/*****************************************************************************
 FUNCTION	: RegSubstPrint

 PURPOSE	: Print out a register substitution

 PARAMETERS	: psState    - Compiler state
              psSubst    - Substitution to print

 RETURNS	: Nothing
*****************************************************************************/
{
	if (psSubst == NULL)
		return;

	UscTreeIterPreOrder(psState, psSubst, SubstElemPrint, NULL);
}
#endif /* DEBUG */

static
IMG_VOID SafeListIteratorStep(PSAFE_LIST_ITERATOR psIterator, PUSC_LIST_ENTRY psPoint)
/*****************************************************************************
 FUNCTION	: SafeListIteratorStep

 PURPOSE	: Update a list iterator's state when changing position.

 PARAMETERS	: psIterator	- Iterator.
			  psPoint		- Position to change to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psIterator->psCurrent = psPoint;
	if (psIterator->psCurrent == NULL)
	{
		psIterator->bContinue = IMG_FALSE;
		psIterator->psPrev = psIterator->psNext = NULL;
	}
	else
	{
		psIterator->psPrev = psPoint->psPrev;
		psIterator->psNext = psPoint->psNext;
	}
}

IMG_INTERNAL
IMG_VOID SafeListIteratorInitializeAtPoint(PSAFE_LIST psList, PSAFE_LIST_ITERATOR psIterator, PUSC_LIST_ENTRY psPoint)
/*****************************************************************************
 FUNCTION	: SafeListIteratorInitialize

 PURPOSE	: Initialize an iterator for a list to point to a particular
			  element.

 PARAMETERS	: psList		- List to iterator over.
			  psIterator	- Iterator to initialize.
			  psPoint		- Starting point for the iteration.

 RETURNS	: Nothing
*****************************************************************************/
{
	psIterator->bContinue = IMG_TRUE;

	SafeListIteratorStep(psIterator, psPoint);

	psIterator->psList = psList;
	AppendToList(&psList->sIteratorList, &psIterator->sIteratorListEntry);
}

IMG_INTERNAL
IMG_VOID SafeListIteratorInitialize(PSAFE_LIST psList, PSAFE_LIST_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: SafeListIteratorInitialize

 PURPOSE	: Initialize an iterator for a list to point to the first element.

 PARAMETERS	: psList		- List to iterator after.
			  psIterator	- Iterator to initialize.

 RETURNS	: Nothing
*****************************************************************************/
{
	SafeListIteratorInitializeAtPoint(psList, psIterator, psList->sBaseList.psHead);
}

IMG_INTERNAL
IMG_VOID SafeListIteratorInitializeAtEnd(PSAFE_LIST psList, PSAFE_LIST_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: SafeListIteratorInitializeAtEnd

 PURPOSE	: Initialize an iterator for a list to point to the last element.

 PARAMETERS	: psList		- List to iterator after.
			  psIterator	- Iterator to initialize.

 RETURNS	: Nothing
*****************************************************************************/
{
	SafeListIteratorInitializeAtPoint(psList, psIterator, psList->sBaseList.psTail);
}

IMG_INTERNAL
PUSC_LIST_ENTRY SafeListIteratorCurrent(PSAFE_LIST_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: SafeListIteratorCurrent

 PURPOSE	: Get the current position of a list iterator.

 PARAMETERS	: psIterator	- Iterator.

 RETURNS	: A pointer to the item at the current position.
*****************************************************************************/
{
	return psIterator->psCurrent;
}

IMG_INTERNAL
IMG_VOID SafeListIteratorNext(PSAFE_LIST_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: SafeListIteratorNext

 PURPOSE	: Move the position of a list iterator forwards.

 PARAMETERS	: psIterator	- Iterator.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SafeListIteratorStep(psIterator, psIterator->psNext);
}

IMG_INTERNAL
IMG_VOID SafeListIteratorPrev(PSAFE_LIST_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: SafeListIteratorPrev

 PURPOSE	: Move the position of a list iterator backwards.

 PARAMETERS	: psIterator	- Iterator.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SafeListIteratorStep(psIterator, psIterator->psPrev);
}

IMG_INTERNAL
IMG_BOOL SafeListIteratorContinue(PSAFE_LIST_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: SafeListIteratorContinue

 PURPOSE	: Check if a list iterator is positioned after the end of the list.

 PARAMETERS	: psIterator	- Iterator.

 RETURNS	: TRUE if the iterator is not after the end of the list.
*****************************************************************************/
{
	return psIterator->bContinue;
}

IMG_INTERNAL
IMG_VOID SafeListIteratorFinalise(PSAFE_LIST_ITERATOR psIterator)
/*****************************************************************************
 FUNCTION	: SafeListIteratorFinalise

 PURPOSE	: Finish using a list iterator and release any resources.

 PARAMETERS	: psIterator	- Iterator.

 RETURNS	: Nothing.
*****************************************************************************/
{
	RemoveFromList(&psIterator->psList->sIteratorList, &psIterator->sIteratorListEntry);
	psIterator->psList = NULL;
	psIterator->psCurrent = psIterator->psPrev = psIterator->psNext = NULL;
}

IMG_INTERNAL
IMG_VOID SafeListInitialize(PSAFE_LIST psList)
/*****************************************************************************
 FUNCTION	: SafeListInitialize

 PURPOSE	: Initialize a list.

 PARAMETERS	: psList		- List to initialize.

 RETURNS	: Nothing.
*****************************************************************************/
{
	InitializeList(&psList->sBaseList);
	InitializeList(&psList->sIteratorList);
}

IMG_INTERNAL
IMG_VOID SafeListPrependItem(PSAFE_LIST psList, PUSC_LIST_ENTRY psItemToInsert)
/*****************************************************************************
 FUNCTION	: SafeListPrependItem

 PURPOSE	: Add an item before the current start of a list. 

 PARAMETERS	: psList			- List to prepend to.
			  psItemToInsert	- Item to add.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psList->sBaseList.psHead == NULL)
	{
		/*
			In this case the list is currently empty so we don't need to update the
			state for any iterators.
		*/
		PrependToList(&psList->sBaseList, psItemToInsert);
	}
	else
	{
		SafeListInsertItemBeforePoint(psList, psItemToInsert, psList->sBaseList.psHead);
	}
}

IMG_INTERNAL
IMG_VOID SafeListInsertItemBeforePoint(PSAFE_LIST			psList,
									   PUSC_LIST_ENTRY		psItemToInsert,
									   PUSC_LIST_ENTRY		psPoint)
/*****************************************************************************
 FUNCTION	: SafeListInsertItemBeforePoint

 PURPOSE	: Add an item at a specified position in a list. Any existing iterators
			  positioned before the insertion point will iterate over the new item.

 PARAMETERS	: psList			- List to add to.
			  psItemToInsert	- Item to add.
			  psPoint			- Either the item (which must already be a member of the list)
								to add the new item before; or NULL to insert the item
								at the end of the list.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSC_LIST_ENTRY psBefore;

	if (psPoint == NULL)
	{
		psBefore = psList->sBaseList.psTail;
	}
	else		
	{
		psBefore = psPoint->psPrev;
	}

	for (psListEntry = psList->sIteratorList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PSAFE_LIST_ITERATOR	psIterator;

		psIterator = IMG_CONTAINING_RECORD(psListEntry, PSAFE_LIST_ITERATOR, sIteratorListEntry);

		/*
			Check for an iterator which has the existing element immediately before 
			the insertion point as the position to move to backwards to. 
		*/
		if (psIterator->psPrev == psBefore)
		{
			psIterator->psPrev = psItemToInsert;
		}

		/*
			Check for an iterator which has the insertion point as the position to move forwards to. 
		*/
		if (psIterator->psNext == psPoint)
		{
			/*
				Update the next position to be the new item.
			*/
			psIterator->psNext = psItemToInsert;
		}
	}

	/*
		Actually insert the new item in the list.
	*/
	InsertInList(&psList->sBaseList, psItemToInsert, psPoint);
}

IMG_INTERNAL
IMG_VOID SafeListAppendItem(PSAFE_LIST psList, PUSC_LIST_ENTRY psItemToInsert)
/*****************************************************************************
 FUNCTION	: SafeListAppendItem

 PURPOSE	: Add an item to the end of a list. Any existing iterators
			  which haven't reached the end of the list will iterate over the new item.

 PARAMETERS	: psList			- List to add to.
			  psItemToInsert	- Item to add.
			  psPoint			- Item (which must already be a member of the list)
								to add the new item before.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SafeListInsertItemBeforePoint(psList, psItemToInsert, NULL /* psPoint */);
}

IMG_INTERNAL
IMG_VOID SafeListRemoveItem(PSAFE_LIST psList, PUSC_LIST_ENTRY psItemToRemove)
/*****************************************************************************
 FUNCTION	: SafeListRemoveItem

 PURPOSE	: Remove an item from a list.

 PARAMETERS	: psList			- List to remove from.
			  psItemToRemove	- Item to remove.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psList->sIteratorList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PSAFE_LIST_ITERATOR	psIterator;

		psIterator = IMG_CONTAINING_RECORD(psListEntry, PSAFE_LIST_ITERATOR, sIteratorListEntry);
		if (psIterator->psCurrent == psItemToRemove)
		{
			psIterator->psCurrent = NULL;
		}
		if (psIterator->psNext == psItemToRemove)
		{
			psIterator->psNext = psIterator->psNext->psNext;
		}
		if (psIterator->psPrev == psItemToRemove)
		{
			psIterator->psPrev = psIterator->psPrev->psPrev;
		}
	}

	RemoveFromList(&psList->sBaseList, psItemToRemove);
}

IMG_INTERNAL
IMG_PVOID CallocBitArray(PINTERMEDIATE_STATE psState, IMG_UINT32 uBitCount)
/*****************************************************************************
 FUNCTION	: UscAlloc

 PURPOSE	: Allocates an internal memory block sufficent to hold an array of
			  bits and zero the array.

 PARAMETERS	: psState		- Compiler state.
			  uBitCount		- The size of the array.

 RETURNS	: The allocated array.
*****************************************************************************/
{
	IMG_UINT32	uArraySize;
	IMG_PVOID	pvArray;

	uArraySize = UINTS_TO_SPAN_BITS(uBitCount) * sizeof(IMG_UINT32);
	pvArray = UscAlloc(psState, uArraySize);
	memset(pvArray, 0, uArraySize);
	return pvArray;
}

IMG_INTERNAL
PINTFGRAPH IntfGraphCreate(PINTERMEDIATE_STATE psState, IMG_UINT32 uVertexCount)
/*****************************************************************************
 FUNCTION   : IntfGraphCreate

 PURPOSE    : Create an interference graph.

 PARAMETERS : psState			- Compiler state.
			  uVertexCount		- Count of vertices in the graph.

 RETURNS    : A pointer to the created graph.
*****************************************************************************/
{
	PINTFGRAPH	psGraph;
	IMG_UINT32	uVertex;

	psGraph = UscAlloc(psState, sizeof(*psGraph));
	psGraph->uVertexCount = uVertexCount;
	psGraph->asVertices = UscAlloc(psState, sizeof(psGraph->asVertices[0]) * uVertexCount);
	for (uVertex = 0; uVertex < uVertexCount; uVertex++)
	{
		PINTFGRAPH_VERTEX	psVertex = &psGraph->asVertices[uVertex];

		InitialiseAdjacencyList(&psVertex->sIntfList);
		psVertex->puIntfGraphRow = NULL;
		psVertex->uDegree = 1;
	}
	psGraph->auRemoved = CallocBitArray(psState, uVertexCount);

	return psGraph;
}

IMG_INTERNAL
IMG_VOID IntfGraphDelete(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph)
/*****************************************************************************
 FUNCTION   : IntfGraphDelete

 PURPOSE    : Free an interference graph.

 PARAMETERS : psState			- Compiler state.
			  psGraph			- Graph to free.

 RETURNS    : Nothing.
*****************************************************************************/
{
	IMG_UINT32	uVertex;

	for (uVertex = 0; uVertex < psGraph->uVertexCount; uVertex++)
	{
		PINTFGRAPH_VERTEX	psVertex = &psGraph->asVertices[uVertex];

		DeleteAdjacencyList(psState, &psVertex->sIntfList);
		if (psVertex->puIntfGraphRow != NULL)
		{
			UscFree(psState, psVertex->puIntfGraphRow);
		}
	}
	UscFree(psState, psGraph->asVertices);
	UscFree(psState, psGraph->auRemoved);
	UscFree(psState, psGraph);
}

IMG_INTERNAL
IMG_BOOL IntfGraphGet(PINTFGRAPH psGraph, IMG_UINT32 uVertex1, IMG_UINT32 uVertex2)
/*****************************************************************************
 FUNCTION   : IntfGraphGet

 PURPOSE    : Get an entry from the interference bit matrix.

 PARAMETERS : psGraph				- Interference graph to query.
              uVertex1, uVertex2	- The row and column of the entry to get.

 RETURNS    : The value of the entry.
*****************************************************************************/
{
	IMG_UINT32			uLow, uHigh;
	PINTFGRAPH_VERTEX	psHigh;

	/*
		The matrix is reflexive (every node interferes with itself).
	*/
	if (uVertex1 == uVertex2)
	{
		return IMG_TRUE;
	}

	/*
		The matrix is symmetrical so each row contains entries only for
		lower numbered columns.
	*/
	if (uVertex1 < uVertex2)
	{
		uLow = uVertex1;
		uHigh = uVertex2;
	}
	else
	{
		uLow = uVertex2;
		uHigh = uVertex1;
	}

	psHigh = &psGraph->asVertices[uHigh];
	if (psHigh->puIntfGraphRow == NULL)
	{
		/*
			Storage for rows is allocated lazily - an unallocated row is treated as empty.
		*/
		return IMG_FALSE;
	}
	return GetBit(psHigh->puIntfGraphRow, uLow);
}

static IMG_BOOL IntfGraphSet(PINTERMEDIATE_STATE	psState,
							 PINTFGRAPH				psGraph, 
							 IMG_UINT32				uVertex1, 
							 IMG_UINT32				uVertex2, 
							 IMG_UINT32				uValue)
/*****************************************************************************
 FUNCTION   : IntfGraphGet

 PURPOSE    : Set an entry in the interference bit matrix.

 PARAMETERS : psState				- Compiler state.
			  psGraph				- Interference graph to modify.
              uVertex1, uVertex2	- The row and column of the entry to set.
			  uValue				- The value to set the entry to.

 RETURNS    : TRUE if the state of the entry was changed.
*****************************************************************************/
{
	IMG_UINT32			uLow, uHigh;
	PINTFGRAPH_VERTEX	psHigh;

	/*
		The matrix is reflexive (every node interferences with itself).
	*/
	if (uVertex1 == uVertex2)
	{
		DBG_ASSERT(uValue);
		return IMG_FALSE;
	}

	/*
		The matrix is symmetrical so each row contains entries only for
		lower numbered columns.
	*/
	if (uVertex1 < uVertex2)
	{
		uLow = uVertex1;
		uHigh = uVertex2;
	}
	else
	{
		uLow = uVertex2;
		uHigh = uVertex1;
	}

	psHigh = &psGraph->asVertices[uHigh];
	if (psHigh->puIntfGraphRow == NULL)
	{
		IMG_UINT32	uRowSize;

		/*
			Allocate storage for the row.
		*/
		uRowSize = UINTS_TO_SPAN_BITS(uHigh) * sizeof(IMG_UINT32);
		psHigh->puIntfGraphRow = UscAlloc(psState, uRowSize);
		memset(psHigh->puIntfGraphRow, 0, uRowSize);
	}
	if (GetBit(psHigh->puIntfGraphRow, uLow) == uValue)
	{
		return IMG_FALSE;
	}
	SetBit(psHigh->puIntfGraphRow, uLow, uValue);
	return IMG_TRUE;
}

static
IMG_VOID IntfGraphRemoveEdge(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph, IMG_UINT32 uVertex1, IMG_UINT32 uVertex2)
/*****************************************************************************
 FUNCTION   : IntfGraphRemoveEdge

 PURPOSE    : Remove a graph edge between two nodes.

 PARAMETERS : psState				- Compiler state.
			  psGraph				- Graph to modify.
              uVertex1, uVertex2	- The two nodes to remove the edge between.

 RETURNS    : Nothing.
*****************************************************************************/
{
	if (IntfGraphSet(psState, psGraph, uVertex1, uVertex2, 0 /* uValue */))
	{
		PINTFGRAPH_VERTEX	psVertex1 = &psGraph->asVertices[uVertex1];
		PINTFGRAPH_VERTEX	psVertex2 = &psGraph->asVertices[uVertex2];

		ASSERT(psVertex1->uDegree > 0);
		psVertex1->uDegree--;
		ASSERT(psVertex2->uDegree > 0);
		psVertex2->uDegree--;

		RemoveFromAdjacencyList(psState, &psVertex2->sIntfList, uVertex1);
	}
}

IMG_INTERNAL
IMG_VOID IntfGraphAddEdge(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph, IMG_UINT32 uVertex1, IMG_UINT32 uVertex2)
/*****************************************************************************
 FUNCTION	: IntfGraphAddEdge

 PURPOSE	: Add an edge between two vertices in an interference graph.

 PARAMETERS	: psState			- Compiler state.
			  psGraph			- Graph to modify.
			  uVertex1, uVertex2	- The two vertices to connect.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (IntfGraphSet(psState, psGraph, uVertex1, uVertex2, 1 /* uValue */))
	{
		PINTFGRAPH_VERTEX	psVertex1 = &psGraph->asVertices[uVertex1];
		PINTFGRAPH_VERTEX	psVertex2 = &psGraph->asVertices[uVertex2];

		psVertex2->uDegree++;
		psVertex1->uDegree++;

		AddToAdjacencyList(psState, &psVertex1->sIntfList, uVertex2);
		AddToAdjacencyList(psState, &psVertex2->sIntfList, uVertex1);
	}
}

IMG_INTERNAL
IMG_VOID IntfGraphMergeVertices(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph, IMG_UINT32 uDest, IMG_UINT32 uSrc)
/*****************************************************************************
 FUNCTION   : IntfGraphMergeVertices

 PURPOSE    : Move all the edges from one vertex to another.

 PARAMETERS : psState			- Compiler state.
			  psGraph			- Graph to modify.
			  uDest				- Vertex to move edges to.
			  uSrc				- Vertex to move edges from.
			  
 RETURNS    : Nothing.
*****************************************************************************/
{
	PADJACENCY_LIST			psList;
	ADJACENCY_LIST_ITERATOR	sIter;
	IMG_UINT32				uOtherVertex;
	PINTFGRAPH_VERTEX		psSrc = &psGraph->asVertices[uSrc];

	/*
		Merge the edges for both nodes.
	*/
	psList = &psSrc->sIntfList;
	for (uOtherVertex = FirstAdjacent(psList, &sIter); !IsLastAdjacent(&sIter); uOtherVertex = NextAdjacent(&sIter))
	{
		IntfGraphAddEdge(psState, psGraph, uDest, uOtherVertex);
		IntfGraphRemoveEdge(psState, psGraph, uSrc, uOtherVertex);
	}

	/*
		Clear the adjacency list for the node to be removed.
	*/
	DeleteAdjacencyList(psState, psList);
	InitialiseAdjacencyList(psList);
	ASSERT(psSrc->uDegree == 1);
}

IMG_INTERNAL
IMG_VOID IntfGraphInsert(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph, IMG_UINT32 uVertex)
/*****************************************************************************
 FUNCTION   : IntfGraphInsert

 PURPOSE    : Undo the changes to vertex degrees applied by IntfGraphRemove.

 PARAMETERS : psState		- Compiler state.
			  psGraph		- Graph to modify.
			  uVertex		- Vertex to insert.

 RETURNS    : Nothing.
*****************************************************************************/
{
    PINTFGRAPH_VERTEX		psVertex = &psGraph->asVertices[uVertex];
	PADJACENCY_LIST			psAdjList = &psVertex->sIntfList;
	ADJACENCY_LIST_ITERATOR sIter;
	IMG_UINT32				uOtherVertex;

	/*
		Initialize the degree of the node to 1 to represent the	
		edge to itself.
	*/
	ASSERT(psVertex->uDegree == 0);
	psVertex->uDegree = 1;

	/*
		Clear the corresponding entry in the array of removed nodes.
	*/
	ASSERT(GetBit(psGraph->auRemoved, uVertex) == 1);
	SetBit(psGraph->auRemoved, uVertex, 0);

	/*
		For each other node which interferes with it increase the degree of
		both nodes.
	*/
	for (uOtherVertex = FirstAdjacent(psAdjList, &sIter); !IsLastAdjacent(&sIter); uOtherVertex = NextAdjacent(&sIter))
	{
		if (!IntfGraphIsVertexRemoved(psGraph, uOtherVertex))
		{
			PINTFGRAPH_VERTEX	psOtherVertex = &psGraph->asVertices[uOtherVertex];

			psVertex->uDegree++;
			psOtherVertex->uDegree++;
		}
	}
}

IMG_INTERNAL
IMG_VOID IntfGraphRemove(PINTERMEDIATE_STATE psState, PINTFGRAPH psGraph, IMG_UINT32 uVertex)
/*****************************************************************************
 FUNCTION   : IntfGraphRemove

 PURPOSE    : 

 PARAMETERS : psState		- Compiler state.
			  psGraph		- Graph to modify.
			  uVertex		- Vertex to remove.

 RETURNS    : Nothing.
*****************************************************************************/
{
    PINTFGRAPH_VERTEX		psVertex = &psGraph->asVertices[uVertex];
	PADJACENCY_LIST			psAdjList = &psVertex->sIntfList;
	ADJACENCY_LIST_ITERATOR sIter;
	IMG_UINT32				uOtherVertex;

	/*
		For each other vertex which interferes with the current node decrease the degree of
		both vertices.
	*/
	for (uOtherVertex = FirstAdjacent(psAdjList, &sIter); !IsLastAdjacent(&sIter); uOtherVertex = NextAdjacent(&sIter))
	{
		if (!IntfGraphIsVertexRemoved(psGraph, uOtherVertex))
		{
			PINTFGRAPH_VERTEX	psOtherVertex = &psGraph->asVertices[uOtherVertex];

			ASSERT(psVertex->uDegree > 0);
			psVertex->uDegree--;

			ASSERT(psOtherVertex->uDegree > 0);
			psOtherVertex->uDegree--;
        }
	}

	/*
		Only the edge between the vertex and itself should be left.
	*/
	ASSERT(psVertex->uDegree == 1);
	psVertex->uDegree = 0;

	/*
		Flag the vertex as removed.
	*/
	ASSERT(GetBit(psGraph->auRemoved, uVertex) == 0);
	SetBit(psGraph->auRemoved, uVertex, 1);
}

/* EOF */
