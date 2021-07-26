/******************************************************************************
 Name           : codeheap.h

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

 Version	 	: $Revision: 1.9 $

 Modifications  :

 $Log: codeheap.h $
******************************************************************************/
#ifndef _CODEHEAP_
#define _CODEHEAP_

/* Forward declaration */
struct UCH_UseCodeHeapTAG;
typedef struct UCH_UseCodeHeapTAG UCH_UseCodeHeap;

typedef struct UCH_UseCodeBlockTAG
{
	/* Pointer to the heap from which this block was allocated */
	UCH_UseCodeHeap				*psHeap;

	/* Pointer to the code memory segment this block belongs to. */
	PVRSRV_CLIENT_MEM_INFO       *psCodeMemory;

	/* Address of the code in device memory. */
	IMG_DEV_VIRTADDR             sCodeAddress;

	/* User accessible address of the code memory. */
	IMG_UINT32                   *pui32LinAddress;

	/* Size of this block's memory in bytes. */
	IMG_UINT32                   ui32Size;

	/* Next block in the list. */
	struct UCH_UseCodeBlockTAG  *psNext;

	/* Pointer to private client specific private data */
	IMG_VOID					*pvClientData;

#ifdef PDUMP
	/* Has this block been pdumped since it was last changed. */
	IMG_BOOL                     bDumped;
#endif /* PDUMP */

#ifdef DEBUG
	/* In debug mode we keep track of where were blocks allocated */
	const IMG_CHAR              *pszFileName;
	IMG_UINT32                   ui32Line;
#endif /* defined DEBUG */

} UCH_UseCodeBlock;


typedef enum _UCH_CodeHeapType
{
	UCH_USE_CODE_HEAP_TYPE = 0x1,
	UCH_PDS_CODE_HEAP_TYPE = 0x2

} UCH_CodeHeapType;

struct UCH_UseCodeHeapTAG
{
	/* Type of code heap: PDS or USSE */
	UCH_CodeHeapType eType;

	/* Dev data to use in dev mem allocs */
	PVRSRV_DEV_DATA*	ps3DDevData;

	/* Linked list of the segments that have been allocated for use as code memory. */
	PVRSRV_CLIENT_MEM_INFO *psCodeMemory;

	/* Linked list of free blocks. */
	UCH_UseCodeBlock *psFreeBlockList;

#if defined(DEBUG) || defined(PDUMP)
	/* Linked list of blocks that are allocated */
	UCH_UseCodeBlock *psAllocatedBlockList;
#endif /* defined DEBUG */

	/* Number of memory blocks that have been allocated but not freed yet */
	IMG_INT32 i32AllocationsNotDeallocated;

	/* Handle to device memory heap to allocate from */
	IMG_HANDLE hHeapAllocator;

	/* Optional mutex handle for locking during alloc/free */
	PVRSRV_MUTEX_HANDLE hSharedLock;

	/* This bit is set to true every time we deallocate a block.
	   ScheduleTA can look at it every time we kick the TA to
	   decide if we have to flush the cache.
	*/
	IMG_BOOL bDirtySinceLastTAKick;
};


/* Prototypes. */
UCH_UseCodeHeap * UCH_CodeHeapCreate(PVRSRV_DEV_DATA* ps3DDevData, UCH_CodeHeapType eType, IMG_HANDLE hHeapAllocator, 
									 PVRSRV_MUTEX_HANDLE hSharedLock, IMG_SID hPerProcRef);

IMG_VOID UCH_CodeHeapDestroy(UCH_UseCodeHeap *psHeap);
IMG_VOID UCH_CodeHeapFreeFunc(UCH_UseCodeBlock *psBlockToFree);
UCH_UseCodeBlock *UCH_CodeHeapAllocateFunc(UCH_UseCodeHeap *psHeap, IMG_UINT32 ui32Size, IMG_SID hPerProcRef);


/* IMPORTANT: To allocate and free code blocks, use always the macros CodeHeapAllocate and CodeHeapFree */

#if defined(DEBUG) || defined(PDUMP)

     /* Track where memory allocations took place */
	UCH_UseCodeBlock *UCH_TrackCodeHeapAllocate(UCH_UseCodeHeap *psHeap, IMG_UINT32 ui32Size,
												const IMG_CHAR *pszFileName, IMG_UINT32 ui32Line, IMG_SID hPerProcRef);

	IMG_VOID UCH_TrackCodeHeapFree(UCH_UseCodeBlock *psBlockToFree);

	#define UCH_CodeHeapAllocate(psHeap, ui32Size, hPerProcRef) UCH_TrackCodeHeapAllocate(psHeap, ui32Size, __FILE__, __LINE__, hPerProcRef)

	#define UCH_CodeHeapFree(psBlockToFree) UCH_TrackCodeHeapFree(psBlockToFree)

#else /* defined(DEBUG) || defined(PDUMP) */

	#define UCH_CodeHeapAllocate(psHeap, ui32Size) UCH_CodeHeapAllocateFunc(psHeap, ui32Size)

	#define UCH_CodeHeapFree(psBlockToFree) UCH_CodeHeapFreeFunc(psBlockToFree)

#endif /* defined(DEBUG) || defined(PDUMP) */


#endif /* _CODEHEAP_ */

/******************************************************************************
 End of file (codeheap.h)
******************************************************************************/


