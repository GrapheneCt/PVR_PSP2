/******************************************************************************
 Name           : codeheap.c

 Title          : USSE code heap managment

 Author         : David Welch

 Created        : 19/08/03

 Copyright      : 2003-2006 by Imagination Technologies Limited.
                  All rights reserved. No part of this software, either
                  material or conceptual may be copied or distributed,
                  transmitted, transcribed, stored in a retrieval system or
                  translated into any human or computer language in any form
                  by any means, electronic, mechanical, manual or otherwise,
                  or disclosed to third parties without the express written
                  permission of Imagination Technologies Limited,
                  Home Park Estate, Kings Langley, Hertfordshire,
                  WD4 8LZ, U.K.

 Description 	: USSE code heap managment

 Version	 	: $Revision: 1.17 $

 Modifications	:

 $Log: codeheap.c $
*****************************************************************************/

#include <string.h>
#include "imgextensions.h"
#include "services.h"
#include "codeheap.h"
#include "pvr_debug.h"
#include "sgxdefs.h"

#define LOCK_CODEHEAP(psCodeHeap)    if (psCodeHeap->hSharedLock) { PVRSRVLockMutex(psCodeHeap->hSharedLock); }
#define UNLOCK_CODEHEAP(psCodeHeap)  if (psCodeHeap->hSharedLock) { PVRSRVUnlockMutex(psCodeHeap->hSharedLock); }


/* Alloc 4K instructions worth of device memory */
#define CODEHEAP_SEGMENTSIZE	(4096 * EURASIA_USE_INSTRUCTION_SIZE)

/*****************************************************************************
 FUNCTION	: UCH_CodeHeapCreate

 PURPOSE	: Create a code heap

 PARAMETERS: ps3DDevData			- 
             eType              - Whether this is a PDS or USE heap.
             hHeapAllocator     - Services handle for the heap where the memory will be pulled from.
			 hSharedLock		- Optional mutex handle for multithread safety.
			 hPerProcRef		- GPU driver per-process reference ID. Pass 0 if first GPU driver allocation

 RETURNS	: The created heap or IMG_NULL on error.
*****************************************************************************/
IMG_INTERNAL UCH_UseCodeHeap * UCH_CodeHeapCreate(PVRSRV_DEV_DATA*	ps3DDevData, UCH_CodeHeapType eType,
												  IMG_HANDLE hHeapAllocator, PVRSRV_MUTEX_HANDLE hSharedLock, IMG_SID hPerProcRef)
{
	UCH_UseCodeHeap  *psHeap;
	UCH_UseCodeBlock *psBlock;

	/* Initialize the main structure */
	psHeap = PVRSRVCallocUserModeMem(sizeof(UCH_UseCodeHeap));

	if (psHeap == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"UCH_CodeHeapCreate: Out of host mem"));
		goto FreeHeapStopTimeAndReturn;
	}

	PVR_ASSERT((eType == UCH_USE_CODE_HEAP_TYPE) || (eType == UCH_PDS_CODE_HEAP_TYPE));

	psHeap->eType		= eType;
	psHeap->ps3DDevData = ps3DDevData;

	if(PVRSRVAllocDeviceMem(ps3DDevData, 
						    hHeapAllocator, 
							PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC,
							CODEHEAP_SEGMENTSIZE,
							CODEHEAP_SEGMENTSIZE,
							hPerProcRef,
							&psHeap->psCodeMemory) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"UCH_CodeHeapCreate: Out of device mem"));
		goto FreeHeapStopTimeAndReturn;
	}

	if (PVRSRVAllocSyncInfo(ps3DDevData, &psHeap->psCodeMemory->psClientSyncInfo))
	{
		PVR_DPF((PVR_DBG_ERROR, "UCH_CodeHeapCreate: PVRSRVAllocSyncInfo failed"));
		PVRSRVFreeDeviceMem(ps3DDevData, psHeap->psCodeMemory);
		goto FreeHeapStopTimeAndReturn;
	}

	psHeap->psCodeMemory->psNext = IMG_NULL;

	/* Initialize the free list with a single block that contains all of psCodeMemory */
	psBlock = PVRSRVCallocUserModeMem(sizeof(UCH_UseCodeBlock));

	if (!psBlock)
	{
		PVRSRVFreeDeviceMem(psHeap->ps3DDevData, psHeap->psCodeMemory); 
		PVR_DPF((PVR_DBG_ERROR,"UCH_CodeHeapCreate: Out of host mem 2"));
		goto FreeHeapStopTimeAndReturn;
	}

	psBlock->sCodeAddress   = psHeap->psCodeMemory->sDevVAddr;
	psBlock->pui32LinAddress = psHeap->psCodeMemory->pvLinAddr;
	psBlock->ui32Size       = psHeap->psCodeMemory->uAllocSize;
	psBlock->psCodeMemory   = psHeap->psCodeMemory;

	psHeap->psFreeBlockList = psBlock;
	psHeap->hHeapAllocator  = hHeapAllocator;
	psHeap->hSharedLock		= hSharedLock;

	return psHeap;

FreeHeapStopTimeAndReturn:
	PVRSRVFreeUserModeMem(psHeap);
	return IMG_NULL;
}

/*****************************************************************************
 FUNCTION	: UCH_CodeHeapDestroy

 PURPOSE	: Destroy a code heap.

 PARAMETERS	: psHeap		- The heap to destroy.

 RETURNS	: Nothing.
*****************************************************************************/
IMG_INTERNAL IMG_VOID UCH_CodeHeapDestroy(UCH_UseCodeHeap * psHeap)
{
	PVRSRV_CLIENT_MEM_INFO    *psSegment, *psSegmentNext;
	UCH_UseCodeBlock         *psBlock, *psBlockNext;

	/* Silently ignore NULL */
	if(!psHeap)
	{
		return;
	}

	/* It is up to caller to ensure threads are not still accessing heaps when destroy comes */
#ifdef DEBUG
	/* Iterate the list of blocks that are still allocated and print where did the allocation take place */
	psBlock = psHeap->psAllocatedBlockList;

	while(psBlock)
	{
		psBlockNext = psBlock->psNext;

		PVR_DPF((PVR_DBG_ERROR,"UCH_CodeHeapDestroy: In heap %p a block of size %d at address %p allocated in %s, line %d was not deallocated.",
			psHeap, psBlock->ui32Size, psBlock, psBlock->pszFileName, psBlock->ui32Line));

		UCH_CodeHeapFree(psBlock);

		psBlock = psBlockNext;
	}
#endif /* DEBUG */

	if(psHeap->i32AllocationsNotDeallocated)
	{
		PVR_DPF((PVR_DBG_ERROR,"UCH_CodeHeapDestroy: In heap %p there are still at least %d memory leaks",
			psHeap, psHeap->i32AllocationsNotDeallocated));
	}

	/* Free all the memory allocated for code. */
	for (psSegment = psHeap->psCodeMemory; psSegment != IMG_NULL; psSegment = psSegmentNext)
	{
		psSegmentNext = psSegment->psNext;
		PVRSRVFreeSyncInfo(psHeap->ps3DDevData, psSegment->psClientSyncInfo);
		PVRSRVFreeDeviceMem(psHeap->ps3DDevData, psSegment);
	}

	/* Free all the host memory used in the code heap. */
	for (psBlock = psHeap->psFreeBlockList; psBlock != IMG_NULL; psBlock = psBlockNext)
	{
		psBlockNext = psBlock->psNext;
		PVRSRVFreeUserModeMem(psBlock);
	}

	PVRSRVMemSet(psHeap, 0, sizeof(UCH_UseCodeHeap));

	PVRSRVFreeUserModeMem(psHeap);
}

#ifdef DEBUG
/*****************************************************************************
 FUNCTION	: CodeHeapIsSane

 PURPOSE	: Determine whether the internal data structures in a code heap are consistent.

 PARAMETERS	: psHeap		- The heap to test

 RETURNS	: Whether the heap is sane.
*****************************************************************************/
static IMG_BOOL CodeHeapIsSane(const UCH_UseCodeHeap * psHeap)
{
	UCH_UseCodeBlock *psBlock;
#if EXTREMELY_SLOW
	IMG_UINT32 ui32Count, ui32ReCount;
#endif

#if EXTREMELY_SLOW
	/*
		Check that there are no loops in the free list. This algorithm takes O(n^2) in time.
	*/
	ui32Count = 0;
	psBlock = psHeap->psFreeBlockList;

	while(psBlock)
	{
		ui32ReCount = 0;
		psPrevBlock = psHeap->psFreeBlockList;

		while(psPrevBlock)
		{
			if(psPrevBlock == psBlock)
			{
				break;
			}

			ui32ReCount++;
			psPrevBlock = psPrevBlock->psNext;
		}

		if(ui32ReCount != ui32Count)
		{
			PVR_DPF((PVR_DBG_ERROR,"CodeHeapIsSane: There's a loop in the free list"));
			return IMG_FALSE;
		}

		ui32Count++;
		psBlock = psBlock->psNext;
	}
#endif /* EXTREMELY_SLOW */

	/*
		Check that the free list is sorted by linear address.
	*/
	psBlock = psHeap->psFreeBlockList;

	while(psBlock && psBlock->psNext)
	{
		if(psBlock->pui32LinAddress >= psBlock->psNext->pui32LinAddress)
		{
			PVR_DPF((PVR_DBG_ERROR,"CodeHeapIsSane: The free list is not sorted"));
			return IMG_FALSE;
		}
		psBlock = psBlock->psNext;
	}

	/*
		Check that the blocks in the free list do not overlap.
	*/
	psBlock = psHeap->psFreeBlockList;

	while(psBlock && psBlock->psNext)
	{
		if((IMG_UINT8 *)psBlock->pui32LinAddress + psBlock->ui32Size > (IMG_UINT8 *)psBlock->psNext->pui32LinAddress)
		{
			PVR_DPF((PVR_DBG_ERROR,"CodeHeapIsSane: Some blocks in the free list overlap"));
			return IMG_FALSE;
		}
		psBlock = psBlock->psNext;
	}


	/* Could also check that the blocks in the allocated list do not overlap */
	/* Could also check that the free list and the allocated list do not overlap */
	/* Could also check that there are no bits of memory outside of both the free and the allocated list */

	return IMG_TRUE;
}
#else /* !DEBUG */
#    define CodeHeapIsSane(psHeap) (psHeap == psHeap)
#endif /* !DEBUG */


/*****************************************************************************
 FUNCTION	: CodeHeapInsertBlockInFreeList

 PURPOSE	: Inserts a block in the free list.

 PARAMETERS	: psHeap		- The heap to which the block belongs.
			  psBlockToFree	- The block to free.

 RETURNS	: Nothing.
*****************************************************************************/
static IMG_VOID CodeHeapInsertBlockInFreeList(UCH_UseCodeHeap * psHeap, UCH_UseCodeBlock * psBlockToFree)
{
	UCH_UseCodeBlock *psBlock, *psPrevBlock, *psNextBlock;
	IMG_UINT32 i;

	/*
		If no blocks are free then make this block the head of the free list.
	*/
	if (psHeap->psFreeBlockList == IMG_NULL)
	{
		psHeap->psFreeBlockList = psBlockToFree;
		psBlockToFree->psNext   = IMG_NULL;
		PVR_ASSERT(CodeHeapIsSane(psHeap));
		return;
	}

	/*
		Sort the free list by linear address.
	*/
	psBlock = psHeap->psFreeBlockList;
	psPrevBlock = IMG_NULL;

	while(psBlock != IMG_NULL)
	{
		if (psBlock->pui32LinAddress > psBlockToFree->pui32LinAddress)
		{
			break;
		}

		psPrevBlock = psBlock;
		psBlock = psBlock->psNext;
	}

#ifdef DEBUG
	/*
		Some sanity checks
	*/
	if(psHeap->psFreeBlockList && (psHeap->psFreeBlockList->pui32LinAddress == psBlockToFree->pui32LinAddress))
	{
		PVR_DPF((PVR_DBG_ERROR,"CodeHeapInsertBlockInFreeList: Refusing to free the same block multiple times"));
		return;
	}

	if(psPrevBlock == IMG_NULL)
	{
		if((IMG_UINT8 *)psBlockToFree->pui32LinAddress + psBlockToFree->ui32Size > (IMG_UINT8 *)psHeap->psFreeBlockList->pui32LinAddress)
		{
			PVR_DPF((PVR_DBG_ERROR,"CodeHeapInsertBlockInFreeList: Block to be freed overlaps free list"));
		}
	}
	else
	{
		if((IMG_UINT8 *)psPrevBlock->pui32LinAddress + psPrevBlock->ui32Size > (IMG_UINT8 *)psBlockToFree->pui32LinAddress)
		{
			PVR_DPF((PVR_DBG_ERROR,"CodeHeapInsertBlockInFreeList: Free list overlaps block to be freed"));
		}
	}

	PVR_ASSERT(psBlock != psBlockToFree);
#endif /* DEBUG */

	/*
		Insert the new block in the free list at the appropriate point.
	*/
	psBlockToFree->psNext = psBlock;
	if (psPrevBlock == IMG_NULL)
	{
		psHeap->psFreeBlockList = psBlockToFree;
	}
	else
	{
		psPrevBlock->psNext = psBlockToFree;
	}

	/*
		Check whether we can coalesce blocks together starting with the block before the newly inserted
		block.
	*/
	if (psPrevBlock == IMG_NULL)
	{
		psBlock = psHeap->psFreeBlockList;
	}
	else
	{
		psBlock = psPrevBlock;
	}

	/* Iteration 0: See if we can coalesce the previous block with thew newly inserted block */
	/* Iteration 1: See if we can coalesce the newly inserted block with the next block */
	for(i = 0; i < 2; ++i)
	{
		if(psBlock && psBlock->psNext)
		{
			if(((IMG_UINT8 *)psBlock->pui32LinAddress + psBlock->ui32Size == (IMG_UINT8 *)psBlock->psNext->pui32LinAddress) &&
			   (psBlock->psCodeMemory == psBlock->psNext->psCodeMemory))
			{
				psNextBlock = psBlock->psNext;
				psBlock->psNext = psNextBlock->psNext;
				psBlock->ui32Size += psNextBlock->ui32Size;
				PVRSRVFreeUserModeMem(psNextBlock);
			}
			else
			{
				psBlock = psBlock->psNext;
			}
		}
	}
}

/*****************************************************************************
 FUNCTION	: UCH_CodeHeapFreeFunc

 PURPOSE	: Free a code block.

 PARAMETERS	: psBlockToFree	- The block to free.

 RETURNS	: Nothing.
*****************************************************************************/
IMG_INTERNAL IMG_VOID UCH_CodeHeapFreeFunc(UCH_UseCodeBlock * psBlockToFree)
{
	UCH_UseCodeHeap *psHeap;

	/* Silently ignore NULL */
	if(!psBlockToFree)
	{
		return;
	}

	psHeap = psBlockToFree->psHeap;

	if(psHeap->eType == UCH_USE_CODE_HEAP_TYPE)
	{
		PVR_ASSERT((psBlockToFree->sCodeAddress.uiAddr & (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) == 0);
	}

	LOCK_CODEHEAP(psHeap);
	
	PVR_ASSERT(CodeHeapIsSane(psHeap));

	/* Update the leak count */
	psHeap->i32AllocationsNotDeallocated--;
	
	CodeHeapInsertBlockInFreeList(psHeap, psBlockToFree);

	psHeap->bDirtySinceLastTAKick = IMG_TRUE;

	PVR_ASSERT(CodeHeapIsSane(psHeap));

	UNLOCK_CODEHEAP(psHeap);
}

/***********************************************************************************
 Function Name      : FindFreeBlock
 Inputs             : psHeap, ui32Size
 Outputs            : ppsBlockPrev
 Returns            : A free block that is at least ui32Size bytes long or IMG_NULL.
 Description        : Find a block in the free list that is big enough.
************************************************************************************/
static UCH_UseCodeBlock * FindFreeBlock(UCH_UseCodeHeap *psHeap, IMG_UINT32 ui32Size,
										UCH_UseCodeBlock ***pppsBestBlockPrev, 
										IMG_UINT32 *pui32BestBlockAlignSize)
{
	UCH_UseCodeBlock **ppsBlockPrev, *psBlock, *psBestBlock = IMG_NULL;
	IMG_UINT32        ui32AlignSize;

	/* Initialize loop variables */
	*pppsBestBlockPrev = IMG_NULL;

	psBlock = psHeap->psFreeBlockList;
	ppsBlockPrev = &psHeap->psFreeBlockList;

	while(psBlock)
	{
		/* Check against a block which covers two instruction pages. */
		if ((psBlock->sCodeAddress.uiAddr                 >> EURASIA_USE_CODE_PAGE_ALIGN_SHIFT) !=
		   ((psBlock->sCodeAddress.uiAddr + ui32Size - 1) >> EURASIA_USE_CODE_PAGE_ALIGN_SHIFT))
		{
			ui32AlignSize = EURASIA_USE_CODE_PAGE_SIZE - (psBlock->sCodeAddress.uiAddr & (EURASIA_USE_CODE_PAGE_SIZE - 1));
		}
		else
		{
			ui32AlignSize = 0;
		}

		/* Check for an exact fit block. */
		if (psBlock->ui32Size == ui32Size + ui32AlignSize)
		{
			*pppsBestBlockPrev     = ppsBlockPrev;
			psBestBlock            = psBlock;
			*pui32BestBlockAlignSize = ui32AlignSize;

			/* Look no further */
			break;
		}

		/* Check for a best-fit so far block. */
		if ((psBlock->ui32Size >= ui32Size + ui32AlignSize) && (!psBestBlock || (psBlock->ui32Size < psBestBlock->ui32Size)))
		{
			*pppsBestBlockPrev     = ppsBlockPrev;
			psBestBlock            = psBlock;
			*pui32BestBlockAlignSize = ui32AlignSize;
		}

		/* Next iteration */
		ppsBlockPrev = &psBlock->psNext;
		psBlock = psBlock->psNext;
	}

	PVR_ASSERT(!psBestBlock || ( ((psBestBlock->sCodeAddress.uiAddr + *pui32BestBlockAlignSize) >> EURASIA_USE_CODE_PAGE_ALIGN_SHIFT) == 
	                              ((psBestBlock->sCodeAddress.uiAddr + *pui32BestBlockAlignSize + ui32Size - 1) >> EURASIA_USE_CODE_PAGE_ALIGN_SHIFT) ));

	if(psHeap->eType == UCH_USE_CODE_HEAP_TYPE)
	{
#if defined(FIX_HW_BRN_31988)
		PVR_ASSERT(!psBestBlock || ((psBestBlock->sCodeAddress.uiAddr & (EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE - 1)) == 0));
#else
		PVR_ASSERT(!psBestBlock || ((psBestBlock->sCodeAddress.uiAddr & (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) == 0));
#endif
	}

	if(psBestBlock)
	{
		psBestBlock->psHeap = psHeap;
	}

	return psBestBlock;
}

/*****************************************************************************
 FUNCTION	: UCH_CodeHeapAllocateFunc

 PURPOSE	: Allocate a code block.

 PARAMETERS	: psHeap		- The heap to allocate the block from.
			  ui32Size		- The size of block to allocate (in bytes).
			  hPerProcRef	- GPU driver per-process reference ID. Pass 0 if first GPU driver allocation

 RETURNS	: The allocated block or IMG_NULL on error.
*****************************************************************************/
IMG_INTERNAL UCH_UseCodeBlock * UCH_CodeHeapAllocateFunc(UCH_UseCodeHeap	*psHeap,
														 IMG_UINT32			ui32Size,
														 IMG_SID			hPerProcRef)
{
	UCH_UseCodeBlock *psBestBlock, *psNewBlock;
	UCH_UseCodeBlock **ppsBestBlockPrev = IMG_NULL;
	IMG_UINT32        ui32BestBlockAlignSize = 0;

	/* Check against allocating a zero-sized block. */
	PVR_ASSERT(ui32Size != 0);

	/* Check against an oversized block. */
	PVR_ASSERT(ui32Size <= CODEHEAP_SEGMENTSIZE);

	/* Check that the code block size is a multiple of the instruction size. */
	if(psHeap->eType == UCH_USE_CODE_HEAP_TYPE)
	{
		PVR_ASSERT((ui32Size & (EURASIA_USE_INSTRUCTION_SIZE - 1)) == 0);

#if defined(FIX_HW_BRN_31988)
		/* USSE blocks returned by this function must be aligned to a cacheline, so make all of the
		   blocks have a cacheline aligned size.
		*/
		ui32Size = (ui32Size + EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE - 1) & ~(EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE - 1);
#else
		/* USSE blocks returned by this function must be aligned to a double instruction boundary so make all of the
		   blocks have a double instruction aligned size.
		*/
		ui32Size = (ui32Size + EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1) & ~(EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1);
#endif
	}
	else
	{
		IMG_UINT32 u32AllocAlignment;

		PVR_ASSERT((ui32Size & (EURASIA_PDS_INSTRUCTION_SIZE - 1)) == 0);

		/* PDS blocks returned by this function must be aligned to u32AllocAlignment so make all of the
		   blocks a size multiple of u32AllocAlignment.
		*/
		u32AllocAlignment = EURASIA_PDS_DATA_CACHE_LINE_SIZE;

		ui32Size = (ui32Size + u32AllocAlignment - 1) & ~(u32AllocAlignment - 1);
	}

	LOCK_CODEHEAP(psHeap);

	PVR_ASSERT(CodeHeapIsSane(psHeap));

	/* Look for a free block of sufficient size. */
	psBestBlock = FindFreeBlock(psHeap, ui32Size, &ppsBestBlockPrev, &ui32BestBlockAlignSize);

	PVR_ASSERT(CodeHeapIsSane(psHeap));

#ifdef DEBUG
	/*
		Paranoia check: make sure if a block has been returned, it and its predecesor are in the free list
	*/
	if(psBestBlock)
	{
		UCH_UseCodeBlock *psBlock;
		IMG_BOOL bBlockFound = IMG_FALSE, bPrevFound = IMG_FALSE;

		PVR_ASSERT(ppsBestBlockPrev && *ppsBestBlockPrev);
		
		if(ppsBestBlockPrev == &psHeap->psFreeBlockList)
		{
			bPrevFound = IMG_TRUE;
		}

		psBlock = psHeap->psFreeBlockList;

		while(psBlock)
		{
			if(psBlock == psBestBlock)
			{
				bBlockFound = IMG_TRUE;
			}

			if(psBlock == *ppsBestBlockPrev)
			{
				bPrevFound = IMG_TRUE;
			}

			if(bBlockFound && bPrevFound)
			{
				break;
			}

			psBlock = psBlock->psNext;
		}

		PVR_ASSERT(*ppsBestBlockPrev == psBestBlock);
		PVR_ASSERT(bBlockFound && bPrevFound);
	}

#endif /* DEBUG */

	if (psBestBlock)
	{
		/* Sanity */
		PVR_ASSERT(ppsBestBlockPrev && *ppsBestBlockPrev == psBestBlock);

		/* Remove the block from the free list */
		*ppsBestBlockPrev = psBestBlock->psNext;

		PVR_ASSERT(CodeHeapIsSane(psHeap));

		/* 
			Create a new block for the alignment gap at the start.
		*/
		if (ui32BestBlockAlignSize > 0)
		{
			/* Create a new block */
			psNewBlock = PVRSRVCallocUserModeMem(sizeof(UCH_UseCodeBlock));
			
			if(!psNewBlock)
			{
				PVR_ASSERT(CodeHeapIsSane(psHeap));
				UNLOCK_CODEHEAP(psHeap);
				return IMG_NULL;
			}

			psNewBlock->psCodeMemory   = psBestBlock->psCodeMemory;
			psNewBlock->pui32LinAddress = psBestBlock->pui32LinAddress;
			psNewBlock->sCodeAddress   = psBestBlock->sCodeAddress;
			psNewBlock->ui32Size       = ui32BestBlockAlignSize;
			psNewBlock->psNext         = IMG_NULL;
	
			PVR_ASSERT((psNewBlock->sCodeAddress.uiAddr & (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) == 0);

			psBestBlock->pui32LinAddress = psBestBlock->pui32LinAddress + (ui32BestBlockAlignSize >> 2);
			psBestBlock->sCodeAddress.uiAddr += ui32BestBlockAlignSize;
			psBestBlock->ui32Size -= ui32BestBlockAlignSize;
			PVR_ASSERT((psBestBlock->sCodeAddress.uiAddr >> EURASIA_USE_CODE_PAGE_ALIGN_SHIFT) == ((psBestBlock->sCodeAddress.uiAddr + ui32Size - 1) >> EURASIA_USE_CODE_PAGE_ALIGN_SHIFT));

			PVR_ASSERT((psBestBlock->sCodeAddress.uiAddr & (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) == 0);

			/* There's no possibility of coalescing blocks,
			 * so we simply add the new block to the free list by hand
			 */
			psNewBlock->psNext = *ppsBestBlockPrev;
			*ppsBestBlockPrev = psNewBlock;

			/* Update the pointer to the prev element to point to the newly inserted block */
			ppsBestBlockPrev  = &psNewBlock->psNext;
		}

		PVR_ASSERT(CodeHeapIsSane(psHeap));

		/*
			If the block is larger than the size we want then create a new free block for the remaining space.
		*/
		PVR_ASSERT(psBestBlock->ui32Size >= ui32Size);

		if (psBestBlock->ui32Size > ui32Size)
		{
			/* Create a new block */
			psNewBlock = PVRSRVCallocUserModeMem(sizeof(UCH_UseCodeBlock));
			
			if(!psNewBlock)
			{
				PVR_ASSERT(CodeHeapIsSane(psHeap));
				UNLOCK_CODEHEAP(psHeap);
				return IMG_NULL;
			}

			psNewBlock->psCodeMemory = psBestBlock->psCodeMemory;
			psNewBlock->pui32LinAddress = psBestBlock->pui32LinAddress + (ui32Size >> 2);
			psNewBlock->sCodeAddress.uiAddr = psBestBlock->sCodeAddress.uiAddr + ui32Size;
			psNewBlock->ui32Size = psBestBlock->ui32Size - ui32Size;
			psNewBlock->psNext = IMG_NULL;

			PVR_ASSERT((psNewBlock->sCodeAddress.uiAddr & (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) == 0);

			/* There's no possibility of coalescing blocks,
			 * so we simply add the new block to the free list by hand
			 */
			psNewBlock->psNext = *ppsBestBlockPrev;
			*ppsBestBlockPrev = psNewBlock;
		}

		/* Increase the memory leak counter */
		psHeap->i32AllocationsNotDeallocated++;

		PVR_ASSERT(CodeHeapIsSane(psHeap));
	}
	else
	{
		PVRSRV_CLIENT_MEM_INFO	*psNewSegment;

		/*
			Try and get a new chunk of code memory.
		*/
		if(PVRSRVAllocDeviceMem(psHeap->ps3DDevData,
								psHeap->hHeapAllocator,
								PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC,
								CODEHEAP_SEGMENTSIZE,
								CODEHEAP_SEGMENTSIZE,
								hPerProcRef,
								&psNewSegment) != PVRSRV_OK)
		{
			PVR_ASSERT(CodeHeapIsSane(psHeap));
			UNLOCK_CODEHEAP(psHeap);
			return IMG_NULL;
		}

		if (PVRSRVAllocSyncInfo(psHeap->ps3DDevData, &psNewSegment->psClientSyncInfo))
		{
			PVR_ASSERT(CodeHeapIsSane(psHeap));
			UNLOCK_CODEHEAP(psHeap);
			PVRSRVFreeDeviceMem(psHeap->ps3DDevData, psNewSegment);
			return IMG_NULL;
		}

		/* Create a new block to represent the free memory. */
		psNewBlock = PVRSRVCallocUserModeMem(sizeof(UCH_UseCodeBlock));

		if (psNewBlock == IMG_NULL)
		{
			PVR_ASSERT(CodeHeapIsSane(psHeap));
			UNLOCK_CODEHEAP(psHeap);
			PVRSRVFreeDeviceMem(psHeap->ps3DDevData, psNewSegment);
			return IMG_NULL;
		}

		psNewBlock->ui32Size       = psNewSegment->uAllocSize;
		psNewBlock->pui32LinAddress = psNewSegment->pvLinAddr;
		psNewBlock->sCodeAddress   = psNewSegment->sDevVAddr;
		psNewBlock->psCodeMemory   = psNewSegment;

		/* Add the new segment to the linked list associated with the heap. */
		psNewSegment->psNext = psHeap->psCodeMemory;
		psHeap->psCodeMemory = psNewSegment;

		/* Insert it into the heap */
		CodeHeapInsertBlockInFreeList(psHeap, psNewBlock);
	
		UNLOCK_CODEHEAP(psHeap);

		/* Now try the allocation again.
		   Instead of a recursive call we could take ui32Size bytes from the block
		   that has been just allocated (psNewBlock) and return them.
		*/
		psBestBlock = UCH_CodeHeapAllocateFunc(psHeap, ui32Size, hPerProcRef);	/* PRQA S 3670 */ /* Override QAC suggestion and use this recursive call. */

		LOCK_CODEHEAP(psHeap);

	}

	PVR_ASSERT(CodeHeapIsSane(psHeap));

	if(psBestBlock)
	{
		PVR_ASSERT((psBestBlock->sCodeAddress.uiAddr >> EURASIA_USE_CODE_PAGE_ALIGN_SHIFT) == ((psBestBlock->sCodeAddress.uiAddr + ui32Size - 1) >> EURASIA_USE_CODE_PAGE_ALIGN_SHIFT));

		PVR_ASSERT((psBestBlock->sCodeAddress.uiAddr & (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) == 0);

		psBestBlock->psNext = IMG_NULL;
		psBestBlock->ui32Size = ui32Size;
#ifdef PDUMP
		psBestBlock->bDumped = IMG_FALSE;
#endif /* PDUMP */
		psBestBlock->psHeap = psHeap;
	}

	psHeap->bDirtySinceLastTAKick = IMG_TRUE;
	
	UNLOCK_CODEHEAP(psHeap);

	return psBestBlock;
}


#if defined(DEBUG) || defined(PDUMP)
/***********************************************************************************
 Function Name      : TrackCodeHeapAllocate
 Inputs             : psHeap, ui32Size, pszFileName, ui32Line, hPerProcRef
 Outputs            : -
 Returns            : See CodeHeapAllocateFunc
 Description        : Thin wrapper around CodeHeapAllocateFunc to make it easier to find memory leaks.
************************************************************************************/
IMG_INTERNAL UCH_UseCodeBlock * UCH_TrackCodeHeapAllocate(UCH_UseCodeHeap *psHeap, IMG_UINT32 ui32Size, 
													   const IMG_CHAR *pszFileName, IMG_UINT32 ui32Line, IMG_SID hPerProcRef)
{
	UCH_UseCodeBlock *psBlock = UCH_CodeHeapAllocateFunc(psHeap, ui32Size, hPerProcRef);

	LOCK_CODEHEAP(psHeap);

	if(psBlock)
	{
#ifdef DEBUG
		UCH_UseCodeBlock *psList;

		/* Set up the file name and line number */
		psBlock->ui32Line    = ui32Line;

		/* Note how we are not copying the file name, merely pointing to it. We are assuming
		 * that the file name is a string literal, so its memory will be reclaimed along with globals.
		 */
		psBlock->pszFileName = pszFileName;

		/* Sanity check: make sure that the returned block does not overlap one of the allocated blocks. */
		psList = psHeap->psAllocatedBlockList;

		while(psList)
		{
			if( ((IMG_UINT8 *)psList->pui32LinAddress + psList->ui32Size > (IMG_UINT8 *)psBlock->pui32LinAddress) &&
			    ((IMG_UINT8 *)psList->pui32LinAddress < (IMG_UINT8 *)psBlock->pui32LinAddress + psBlock->ui32Size) )
			{
				PVR_DPF((PVR_DBG_ERROR,"UCH_TrackCodeHeapAllocate: The returned block overlaps one of the previous blocks"));
			}
			psList = psList->psNext;
		}

		/* Second sanity check: make sure that the block does not overlap one of the blocks in the free list */
		psList = psHeap->psFreeBlockList;

		while(psList)
		{
			if( ((IMG_UINT8 *)psList->pui32LinAddress + psList->ui32Size > (IMG_UINT8 *)psBlock->pui32LinAddress ) &&
			    ((IMG_UINT8 *)psList->pui32LinAddress < (IMG_UINT8 *)psBlock->pui32LinAddress + psBlock->ui32Size) )
			{
				PVR_DPF((PVR_DBG_ERROR,"UCH_TrackCodeHeapAllocate: The returned block overlaps the free list"));
			}
			psList = psList->psNext;
		}
#else  /* !DEBUG */
		PVR_UNREFERENCED_PARAMETER(pszFileName);
		PVR_UNREFERENCED_PARAMETER(ui32Line);
#endif /* !DEBUG */

		/* Insert the block at the beginning of the list of allocated blocks */
		psBlock->psNext = psHeap->psAllocatedBlockList;
		psHeap->psAllocatedBlockList = psBlock;
	}
	
	UNLOCK_CODEHEAP(psHeap);

	return psBlock;
}

/***********************************************************************************
 Function Name      : TrackCodeHeapFree
 Inputs             : psHeap, psBlockToFree
 Outputs            : -
 Returns            : See CodeHeapFreeFunc
 Description        : Thin wrapper around CodeHeapFreeFunc to make it easier to find memory leaks.
************************************************************************************/
IMG_INTERNAL IMG_VOID UCH_TrackCodeHeapFree(UCH_UseCodeBlock *psBlockToFree)
{
	if(psBlockToFree)
	{
		UCH_UseCodeHeap *psHeap = psBlockToFree->psHeap;
		UCH_UseCodeBlock *psBlock, **ppsBlockPrev;

		LOCK_CODEHEAP(psHeap);

		psBlock = psHeap->psAllocatedBlockList;
		ppsBlockPrev = &psHeap->psAllocatedBlockList;

		/* Look for it in the list of allocated blocks */
		while(psBlock && psBlock != psBlockToFree)
		{
			ppsBlockPrev = &psBlock->psNext;
			psBlock = psBlock->psNext;
		}

		if(psBlock)
		{
			/* The block was found. Remove it from the list */
			*ppsBlockPrev = psBlock->psNext;
			psBlock->psNext = IMG_NULL;

			UNLOCK_CODEHEAP(psHeap);

			UCH_CodeHeapFreeFunc(psBlock);
		}
		else
		{
			/* The block was not found in the list. Issue an error */
			PVR_DPF((PVR_DBG_ERROR,"UCH_TrackCodeHeapFree: Block at address %p was not allocated in heap %p.",
				psBlockToFree, psHeap));
			PVR_DPF((PVR_DBG_ERROR,"UCH_TrackCodeHeapFree: Maybe it was allocated in a different heap?"));
			UNLOCK_CODEHEAP(psHeap);
		}
	}
}
#endif /* DEBUG */

/******************************************************************************
 End of file (codeheap.c)
******************************************************************************/
