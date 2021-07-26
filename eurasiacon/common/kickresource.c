/******************************************************************************
 * Name         : kickresource.c
 *
 * Copyright    : 2006-2010 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed,stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: kickresource.c $
 *
 *
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "psp2_pvr_defs.h"
#include "imgextensions.h"
#include "img_defs.h"
#include "pvr_debug.h"
#include "services.h"
#include "kickresource.h"
#include "buffers.h"


/* This must be >= 2 */
#define KRM_INITIAL_ATTACHMENTS 2
#define KRM_MAX_QUEUED_RENDERS	3


/***********************************************************************************
 Function Name      : KRM_ENTER_CRITICAL_SECTION
 Inputs             : ui32Name
 Outputs            : -
 Returns            : NULL
 Description        : Lock function that either locks Mutex or does nothing.
************************************************************************************/
static IMG_VOID KRM_ENTER_CRITICAL_SECTION(const KRMKickResourceManager *psMgr)
{
    if (psMgr->bUseLock)
	{
		PVRSRVLockMutex(psMgr->hSharedLock);
	}
}


/***********************************************************************************
 Function Name      : KRM_EXIT_CRITICAL_SECTION
 Inputs             : ui32Name
 Outputs            : -
 Returns            : NULL
 Description        : Lock function that either locks Mutex or does nothing.
************************************************************************************/
static IMG_VOID KRM_EXIT_CRITICAL_SECTION(const KRMKickResourceManager *psMgr)
{
    if (psMgr->bUseLock)
	{
		PVRSRVUnlockMutex(psMgr->hSharedLock);
	}
}


static IMG_VOID RemoveResourceFromAllLists(KRMKickResourceManager *psMgr, KRMResource *psResource);


/***********************************************************************************
 Function Name      : IsKickFinished
 Inputs             : psAttachment, eType
 Outputs            : -
 Returns            : IMG_TRUE if the frame/kick is finished. IMG_FALSE otherwise.
 Description        : Returns whether the hardware has finished the render/TA
                      number psAttachment->ui32Value on the surface/context
                      psAttachment->pvAttachmentPoint,
************************************************************************************/
static IMG_BOOL IsKickFinished(const KRMAttachment *psAttachment, const KRMType eType)
{
	PVR_ASSERT(psAttachment->pvAttachmentPoint);

	switch(eType)
	{
		case KRM_TYPE_3D:
		{
#if defined (NO_HARDWARE) && defined (PDUMP) && defined(OPENVG_MODULE)
			return ((psAttachment->ui32Value + KRM_MAX_QUEUED_RENDERS) < *((IMG_UINT32*)psAttachment->psStatusUpdate->psMemInfo->pvLinAddr) ) ? IMG_TRUE : IMG_FALSE;
#else
			return (psAttachment->ui32Value <= *((IMG_UINT32*)psAttachment->psStatusUpdate->psMemInfo->pvLinAddr) ) ? IMG_TRUE : IMG_FALSE;
#endif
		}
		case KRM_TYPE_TA:
		{
			return (psAttachment->ui32Value <= *((IMG_UINT32*)psAttachment->psStatusUpdate->psMemInfo->pvLinAddr) ) ? IMG_TRUE : IMG_FALSE;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "IsKickFinished: Invalid manager type"));

			return IMG_FALSE;
		}
	}
}


/***********************************************************************************
 Function Name      : KRM_Initialize
 Inputs             : eType, hSharedLock
 Outputs            : psMgr
 Returns            : Success
 Description        : Initializes the given kick resource manager for the given
 					  type of resources.
************************************************************************************/
IMG_INTERNAL IMG_BOOL KRM_Initialize(KRMKickResourceManager			*psMgr,
									 KRMType						 eType,
									 IMG_BOOL                        bUseLock,
									 PVRSRV_MUTEX_HANDLE			 hSharedLock,
									 PVRSRV_DEV_DATA				*psDevData,
									 IMG_HANDLE 					 hOSEvent,
									 IMG_VOID  (*pfnReclaimResourceMem) (IMG_VOID *, KRMResource *),
									 IMG_BOOL                        bRemoveResourceAfterRecoveringMem,
									 IMG_VOID  (*pfnDestroyGhost)    (IMG_VOID *, KRMResource *))
{
	IMG_UINT32 i;

	PVR_ASSERT(psMgr);
	PVR_ASSERT(!psMgr->bInitialized);

	/* If Lock should be used */
	if (bUseLock)
	{
		/* Set up the shared lock */
		if(!hSharedLock)
		{
			PVR_DPF((PVR_DBG_ERROR, "KRM_Initialize: Invalid mutex parameter"));

			goto ReturnFalse;
		}
	}

	if(!pfnReclaimResourceMem || !pfnDestroyGhost)
	{
		PVR_DPF((PVR_DBG_ERROR, "KRM_Initialize: Invalid callback parameter"));

		goto ReturnFalse;
	}

	switch(eType)
	{
		case KRM_TYPE_3D:
		case KRM_TYPE_TA:
		{
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "KRM_Initialize: Invalid type parameter"));

			goto ReturnFalse;
		}
	}

	psMgr->pfnReclaimResourceMem = pfnReclaimResourceMem;
	psMgr->bRemoveResourceAfterRecoveringMem = bRemoveResourceAfterRecoveringMem;
	psMgr->pfnDestroyGhost		 = pfnDestroyGhost;
	psMgr->bUseLock              = bUseLock;
	psMgr->hSharedLock			 = hSharedLock;
	psMgr->psDevData			 = psDevData;
	psMgr->hOSEvent				 = hOSEvent;
	psMgr->eType				 = eType;

	/* Allocate the pool of attachments */
	psMgr->ui32MaxAttachments = KRM_INITIAL_ATTACHMENTS;

	psMgr->asAttachment = PVRSRVCallocUserModeMem(psMgr->ui32MaxAttachments * sizeof(KRMAttachment));

	if(!psMgr->asAttachment)
	{
		PVR_DPF((PVR_DBG_ERROR, "KRM_Initialize: Could not allocate attachment pool"));

		goto ReturnFalse;
	}

	/* Initialize the free list of attachments */
	psMgr->asAttachment[0].ui32Next = 0;

	for(i=1; i < psMgr->ui32MaxAttachments; ++i)
	{
		psMgr->asAttachment[i].pvAttachmentPoint = IMG_NULL;;
		psMgr->asAttachment[i].ui32Value = 0;
		psMgr->asAttachment[i].ui32Next = i + 1;
	}

	psMgr->asAttachment[psMgr->ui32MaxAttachments-1].ui32Next = 0;

	/* Zero is reserved. Start the list with element 1 */
	psMgr->ui32AttachmentFreeList = 1;

	/* Initialize all the lists as empty */
	psMgr->psResourceList = IMG_NULL;
	psMgr->psGhostList    = IMG_NULL;
	psMgr->bInitialized   = IMG_TRUE;

	return IMG_TRUE;

ReturnFalse:

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : AllocAttachment
 Inputs             : psMgr
 Outputs            : psMgr
 Returns            : The index of the next free element.
 Description        : Finds a free element in the pool psMgr->asAttachment and returns its index.
************************************************************************************/
static IMG_UINT32 AllocAttachment(KRMKickResourceManager *psMgr)
{
	IMG_UINT32 ui32OldFreeList;

	if(!psMgr->ui32AttachmentFreeList)
	{
		KRMAttachment *asNewAttachment;
	
		IMG_UINT32 ui32NewMaxAttachments, i;

		/* We ran out of space in the pool. First allocate some more memory */
		ui32NewMaxAttachments = psMgr->ui32MaxAttachments << 1;

		asNewAttachment = PVRSRVReallocUserModeMem(psMgr->asAttachment, ui32NewMaxAttachments * sizeof(KRMAttachment));

		if(!asNewAttachment)
		{
			PVR_DPF((PVR_DBG_WARNING, "AllocAttachment: Out of memory. Returning 0\n"));

			return 0;
		}

		psMgr->asAttachment = asNewAttachment;

		/* Memory was allocated. Regenerate the free list */
		for(i = psMgr->ui32MaxAttachments; i < ui32NewMaxAttachments; ++i)
		{
			asNewAttachment[i].pvAttachmentPoint = IMG_NULL;;
			asNewAttachment[i].ui32Value = 0;
			asNewAttachment[i].ui32Next = i + 1;
		}

		asNewAttachment[ui32NewMaxAttachments - 1].ui32Next = 0;

		psMgr->ui32AttachmentFreeList = psMgr->ui32MaxAttachments;

		/* Update the size */
		psMgr->ui32MaxAttachments = ui32NewMaxAttachments;
	}

	ui32OldFreeList = psMgr->ui32AttachmentFreeList;

	psMgr->ui32AttachmentFreeList = psMgr->asAttachment[ui32OldFreeList].ui32Next;

	psMgr->asAttachment[ui32OldFreeList].ui32Next = 0;

	return ui32OldFreeList;
}


/***********************************************************************************
 Function Name      : FreeAttachment
 Inputs             : psMgr, ui32Attachment
 Outputs            : psMgr
 Returns            : -
 Description        : Adds an element to the free list. The element must have been
                      allocated with AllocAttachment.
************************************************************************************/
static IMG_VOID FreeAttachment(KRMKickResourceManager *psMgr, IMG_UINT32 ui32Attachment)
{
	PVR_ASSERT(ui32Attachment < psMgr->ui32MaxAttachments);

	/* Attach the element to the head of the free list. Silently ignore NULL */
	if(ui32Attachment)
	{
		PVR_ASSERT(psMgr->asAttachment[ui32Attachment].ui32Next < psMgr->ui32MaxAttachments);

		psMgr->asAttachment[ui32Attachment].pvAttachmentPoint = IMG_NULL;
		psMgr->asAttachment[ui32Attachment].ui32Value = 0;
		psMgr->asAttachment[ui32Attachment].ui32Next  = psMgr->ui32AttachmentFreeList;

		psMgr->ui32AttachmentFreeList = ui32Attachment;
	}
}


/***********************************************************************************
 Function Name      : KRM_Attach
 Inputs             : psMgr, pvAttachment, pvSyncData
 Outputs            : psResource
 Returns            : Success
 Description        : Attaches a resource to a given surface/context.
************************************************************************************/
IMG_INTERNAL IMG_BOOL KRM_Attach(KRMKickResourceManager *psMgr, const IMG_VOID *pvAttachmentPoint,
								 const KRMStatusUpdate *psStatusUpdate, KRMResource *psResource)
{
	IMG_UINT32 ui32Value, ui32NextAttachment;
	KRMAttachment *psAttachment;
	IMG_BOOL bFound = IMG_FALSE;

	KRM_ENTER_CRITICAL_SECTION(psMgr);

	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(pvAttachmentPoint);
	PVR_ASSERT(psResource);

	switch(psMgr->eType)
	{
		case KRM_TYPE_3D:
		case KRM_TYPE_TA:
		{
			ui32Value = psStatusUpdate->ui32StatusValue;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "FRM_Attach: Unable to get a new attachment."));

			KRM_EXIT_CRITICAL_SECTION(psMgr);

			return IMG_FALSE;
		}
	}

	/* If the resource is not in the resource list, add it. */
	if(!(psResource->psPrev || psResource->psNext || psResource==psMgr->psResourceList))
	{
		psResource->psPrev = IMG_NULL;
		psResource->psNext = psMgr->psResourceList;

		if(psMgr->psResourceList)
		{
			psMgr->psResourceList->psPrev = psResource;
		}

		psMgr->psResourceList = psResource;
	}

	/* Iterate through all surfaces this resource is attached to */
	ui32NextAttachment = psResource->ui32FirstAttachment;

	while(ui32NextAttachment && !bFound)
	{
		psAttachment = &psMgr->asAttachment[ui32NextAttachment];

		if(psAttachment->pvAttachmentPoint == pvAttachmentPoint)
		{
			/* The resource was already attached to the same surface */
			psAttachment->psStatusUpdate = psStatusUpdate;
			psAttachment->ui32Value = ui32Value;

			bFound = IMG_TRUE;
		}
		else
		{
			ui32NextAttachment = psAttachment->ui32Next;
		}
	}

	if(!bFound)
	{
		IMG_UINT32 ui32NewAttachment;

		ui32NewAttachment = AllocAttachment(psMgr);

		if(!ui32NewAttachment)
		{
			PVR_DPF((PVR_DBG_ERROR, "FRM_Attach: Unable to get a new attachment."));

			KRM_EXIT_CRITICAL_SECTION(psMgr);

			return IMG_FALSE;
		}

		/* Set up the attributes of the new attachment */
		psAttachment					= &psMgr->asAttachment[ui32NewAttachment];
		psAttachment->pvAttachmentPoint	= pvAttachmentPoint;
		psAttachment->ui32Value			= ui32Value;
		psAttachment->psStatusUpdate	= psStatusUpdate;
		psAttachment->ui32Next			= psResource->ui32FirstAttachment;

		/* Insert the new attachment in the head of the attachment list */
		psResource->ui32FirstAttachment = ui32NewAttachment;
	}
	
	KRM_EXIT_CRITICAL_SECTION(psMgr);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : IsResourceInUse
 Inputs             : psMgr, pvAttachmentPoint, psSyncData, psResource, bNotKickedYet
 Outputs            : 
                      1) Is not needed at all: return FALSE & NULL surface
                      2) Is needed only by one surface/context: return FALSE & THE ONE surface
                      3) Is needed by more than one surface/context: return TRUE & NULL surface

 Returns            : Whether a resource is being used to render any frame, is part
 					  of a TA kick, or not.
 Description        : Generalization of TextureIsLive() for arbitrary resources
                      (including ghosts).
************************************************************************************/
static IMG_BOOL IsResourceInUse(const KRMKickResourceManager *psMgr,
							    const IMG_VOID *pvAttachmentPoint,
							    const KRMStatusUpdate *psStatusUpdate,
							    const KRMResource *psResource)
{
	IMG_UINT32     ui32Value, ui32NextAttachment;
	KRMAttachment  *psAttachment;
	IMG_BOOL       bFirstAttachment = IMG_TRUE;
	IMG_BOOL       bInUse = IMG_FALSE;

	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);

	switch(psMgr->eType)
	{
		case KRM_TYPE_3D:
		case KRM_TYPE_TA:
		{
			ui32Value = psStatusUpdate->ui32StatusValue;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "IsResourceInUse: Invalid manager type."));

			return IMG_FALSE;
		}
	}
	
	/* Iterate all attachments this resource is attached to */
	ui32NextAttachment = psResource->ui32FirstAttachment;

	while(ui32NextAttachment)
	{
		PVR_ASSERT(ui32NextAttachment < psMgr->ui32MaxAttachments);
		
		psAttachment = &(psMgr->asAttachment[ui32NextAttachment]);

		/* 
		   If only the current surface's/context's frame/kick is attached to the resource, 
		   check the following two cases. 
		*/
		if ( bFirstAttachment && 
			 (psAttachment->ui32Next  == 0) &&
			 (psAttachment->pvAttachmentPoint == pvAttachmentPoint) )
		{
		    /* Acquire the frame/kick (in the current surface/context) which 
			   was attached to the resource last time. */
		    IMG_UINT32 ui32PreviousAttachedValue = psAttachment->ui32Value;

			/* Assert the relation between frames/kicks */
			PVR_ASSERT(ui32Value >= ui32PreviousAttachedValue);

			/* 
			   Two cases:

			   1. ui32Value > ui32PreviousAttachedValue:
			      the resource has only attached to the previous frame/kick,  
				  so the resource is NOT in use for the current frame/kick;
			   
			   2. ui32Value == ui32PreviousAttachedValue
			      the resource is attached to the current frame/kick, which means
				  the resource has already been used to draw the current frame/kick, 
				  so if the current frame/kick is not finished, then the resource MUST be in use right now!
			*/
			if (ui32Value == ui32PreviousAttachedValue)
			{
				if (!IsKickFinished(psAttachment, psMgr->eType))
				{
					bInUse = IMG_TRUE;
				}
			}

			ui32NextAttachment = 0;

			bFirstAttachment = IMG_FALSE;
		}
		else 
		{
		    /* 
			   If more than one surfaces'/contexts' frames/kicks are attached to the resource, 
			   use the usual routine.
			*/
			if (IsKickFinished(psAttachment, psMgr->eType))
			{
				ui32NextAttachment = psAttachment->ui32Next;
			}
			else
			{
				bInUse = IMG_TRUE;

				ui32NextAttachment = 0;
			}

			bFirstAttachment = IMG_FALSE;
		}
	}

	return bInUse;
}


/*********************************************************************************************
 Function Name      : KRM_IsResourceInUse
 Inputs             : See IsResourceInUse.
 Outputs            : See IsResourceInUse.
 Returns            : See IsResourceInUse.
 Description        : Thread-safe wrapper around IsResourceInUse. This wrapper is necessary
                      since we need internally a non-locking version of this function.
**********************************************************************************************/
IMG_INTERNAL IMG_BOOL KRM_IsResourceInUse(const KRMKickResourceManager *psMgr,
										  const IMG_VOID *pvAttachmentPoint,
										  const KRMStatusUpdate *psStatusUpdate,
										  const KRMResource *psResource)
{
	IMG_BOOL bSuccess;

	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(pvAttachmentPoint);
	PVR_ASSERT(psResource);

	KRM_ENTER_CRITICAL_SECTION(psMgr);

	bSuccess = IsResourceInUse(psMgr, pvAttachmentPoint, psStatusUpdate, psResource);

	KRM_EXIT_CRITICAL_SECTION(psMgr);

	return bSuccess;
}


/***********************************************************************************
 Function Name      : IsResourceKicked
 Inputs             : psMgr, psResource
 Outputs            : -
 Returns            : Whether a frame or TA kick which references a resoure has been
					  kicked or not. 
 Description        : If not, no amount of waiting will cause the 
					  resource to be unneeded.
************************************************************************************/
static IMG_BOOL IsResourceKicked(const KRMKickResourceManager *psMgr, const KRMResource *psResource)
{
	IMG_UINT32 ui32NextAttachment;
	KRMAttachment *psAttachment;

	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);

	/* Iterate all frames/contexts this resource is attached to */
	ui32NextAttachment = psResource->ui32FirstAttachment;

	while(ui32NextAttachment)
	{
		PVR_ASSERT(ui32NextAttachment < psMgr->ui32MaxAttachments);

		psAttachment = &psMgr->asAttachment[ui32NextAttachment];

		/* The frame/TA has not been kicked yet */
		if(psAttachment->ui32Value == psAttachment->psStatusUpdate->ui32StatusValue)
		{
			return IMG_FALSE;
		}

		ui32NextAttachment = psAttachment->ui32Next;
	}

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : IsResourceNeeded
 Inputs             : psMgr, psResource
 Outputs            : -
 Returns            : Whether a resource is required to render any frame, or complete
 					  a TA kick, or not,
 Description        : Generalization of TextureIsLive() for arbitrary resources
                      (including ghosts).
************************************************************************************/
static IMG_BOOL IsResourceNeeded(const KRMKickResourceManager *psMgr, const KRMResource *psResource)
{
	IMG_UINT32 ui32NextAttachment;
	KRMAttachment *psAttachment;
	IMG_BOOL bNeeded;

	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);

	bNeeded = IMG_FALSE;

	/* Iterate all frames/contexts this resource is attached to */
	ui32NextAttachment = psResource->ui32FirstAttachment;

	while(ui32NextAttachment)
	{
		PVR_ASSERT(ui32NextAttachment < psMgr->ui32MaxAttachments);

		psAttachment = &psMgr->asAttachment[ui32NextAttachment];

		/* Is this frame/kick finished? */
		if(IsKickFinished(psAttachment, psMgr->eType))
		{
			/* Yes, continue */
			ui32NextAttachment = psAttachment->ui32Next;
		}
		else
		{
			/* No, the resource is needed and we can stop the search. */
			bNeeded = IMG_TRUE;

			ui32NextAttachment = 0;
		}
	}

	return bNeeded;
}


/***********************************************************************************
 Function Name      : KRM_IsResourceNeeded
 Inputs             : See IsResourceNeeded.
 Outputs            : See IsResourceNeeded.
 Returns            : See IsResourceNeeded.
 Description        : Thread-safe wrapper around IsResourceNeeded. This wrapper is necessary
                      since we need internally a non-locking version of this function.
************************************************************************************/
IMG_INTERNAL IMG_BOOL KRM_IsResourceNeeded(const KRMKickResourceManager *psMgr,	const KRMResource *psResource)
{
	IMG_BOOL bSuccess;

	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(psResource);

	KRM_ENTER_CRITICAL_SECTION(psMgr);

	bSuccess = IsResourceNeeded(psMgr, psResource);

	KRM_EXIT_CRITICAL_SECTION(psMgr);

	return bSuccess;
}


/***********************************************************************************
 Function Name      : WaitUntilResourceIsNotNeeded
 Inputs             : psMgr, psResource, ui32MaxRetries
 Outputs            : -
 Returns            : Success
 Description        : Returns to the caller when all frames the resource is attached to
                      have finished rendering, or all the contexts the resource is attached to
                      have finished TA'ing, or when the maximum number of retries is reached.
************************************************************************************/
static IMG_BOOL WaitUntilResourceIsNotNeeded(const KRMKickResourceManager *psMgr,
											 const KRMResource *psResource,
											 const IMG_UINT32 ui32MaxRetries)
{
	IMG_UINT32 ui32TriesLeft;

	ui32TriesLeft = ui32MaxRetries;

	while(IsResourceNeeded(psMgr, psResource) && IsResourceKicked(psMgr, psResource))
	{
		if(!ui32TriesLeft)
		{
			return IMG_FALSE;
		}

		if(sceGpuSignalWait(sceKernelGetTLSAddr(0x44), 100000) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_MESSAGE, "WaitUntilResourceIsNotNeeded: PVRSRVEventObjectWait failed"));
			ui32TriesLeft--;
		}
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : KRM_WaitUntilResourceIsNotNeeded
 Inputs             : psMgr, psResource, ui32MaxRetries
 Outputs            : -
 Returns            : Success
 Description        : Returns to the caller when all frames the resource is attached to
                      have finished rendering, or all the contexts the resource is attached to
                      have finished TA'ing, or when the maximum number of retries is reached.
************************************************************************************/
IMG_INTERNAL IMG_BOOL KRM_WaitUntilResourceIsNotNeeded(const KRMKickResourceManager *psMgr,	
													   const KRMResource *psResource,
													   const IMG_UINT32 ui32MaxRetries)
{
	IMG_BOOL bSuccess;

	KRM_ENTER_CRITICAL_SECTION(psMgr);

	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(psResource);

	bSuccess = WaitUntilResourceIsNotNeeded(psMgr, psResource, ui32MaxRetries);

	KRM_EXIT_CRITICAL_SECTION(psMgr);

	return bSuccess;
}


/***********************************************************************************
 Function Name      : KRM_WaitForAllResources
 Inputs             : psMgr, ui32MaxRetries
 Outputs            : -
 Returns            : Success
 Description        : Waits until all ghosted and non-ghosted resources are not needed.
                      It is equivalent to calling KRM_WaitUntilResourceIsNotNeeded
                      for each element in the resource and in the ghost list.
************************************************************************************/
IMG_INTERNAL IMG_BOOL KRM_WaitForAllResources(const KRMKickResourceManager *psMgr, const IMG_UINT32 ui32MaxRetries)
{
	KRMResource *psResource;
	IMG_BOOL bSuccess;

	KRM_ENTER_CRITICAL_SECTION(psMgr);

	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);

	bSuccess = IMG_TRUE;

	/* Wait for all elements in the resource list */
	psResource = psMgr->psResourceList;

	while(psResource && bSuccess)
	{
		bSuccess = WaitUntilResourceIsNotNeeded(psMgr, psResource, ui32MaxRetries);

		psResource = psResource->psNext;
	}

	/* Wait for all elements in the ghost list */
	psResource = psMgr->psGhostList;

	while(psResource && bSuccess)
	{
		bSuccess = WaitUntilResourceIsNotNeeded(psMgr, psResource, ui32MaxRetries);

		psResource = psResource->psNext;
	}

	KRM_EXIT_CRITICAL_SECTION(psMgr);

	return bSuccess;
}


/***********************************************************************************
 Function Name      : KRM_GhostResource
 Inputs             : psMgr
 Outputs            : psOriginalResurce, psGhostOfOriginalResource
 Returns            : Success.
 Description        : Ghosts a resource. The ghost keeps all the reverse dependencies of the original,
                      and the original is updated to have no reverse dependencies.
************************************************************************************/
IMG_INTERNAL IMG_BOOL KRM_GhostResource(KRMKickResourceManager *psMgr, KRMResource *psOriginalResource, KRMResource *psGhostOfOriginalResource)
{
	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(psOriginalResource);
	PVR_ASSERT(psGhostOfOriginalResource);

	KRM_ENTER_CRITICAL_SECTION(psMgr);

	/* First, transfer the reverse dependencies from the the original resource to the ghost */
	psGhostOfOriginalResource->ui32FirstAttachment = psOriginalResource->ui32FirstAttachment;

	/* Second, make the original resource have no reverse dependencies */
	psOriginalResource->ui32FirstAttachment = 0;

	/* Third, add the ghost to the ghost list.
	 * There is no need to remove the original from the resource list even though it has no dependencies.
	 * Resources are ghosted when they are required in a frame, so after this call returns
	 * KRM_Attach will be called soon on the original resource.
	 */
	psGhostOfOriginalResource->psNext = psMgr->psGhostList;
	psGhostOfOriginalResource->psPrev = IMG_NULL;

	if(psMgr->psGhostList)
	{
		psMgr->psGhostList->psPrev = psGhostOfOriginalResource;
	}

	psMgr->psGhostList = psGhostOfOriginalResource;

	KRM_EXIT_CRITICAL_SECTION(psMgr);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : ReclaimUnneededResourcesInList
 Inputs             : psMgr, ppsResourceList, pfnFreeResource, bRemoveFromListIfUnneeded
 Outputs            : psMgr
 Returns            : -
 Description        : Frees the resources in the list that are no longer needed to render any frame,
					  or process any TA kick.  The list may be updated and even turned to 
					  NULL if all resources are freed.

 Limitations:         IMPORTANT: If the flag bRemoveFromListIfUnneeded is TRUE then the function
                      pfnFreeResource may call any of the FRM functions, but if the flag is FALSE
                      then pfnFreeResource MUST NOT call them as that would produce a deadlock.
                      This limitation is due to the reuse of the psNext pointer in the Resource
                      struct to build a list of resources that will be freed outside of the critical section.
************************************************************************************/
static IMG_VOID ReclaimUnneededResourcesInList(KRMKickResourceManager *psMgr,
											   KRMResource **ppsResourceList,
											   IMG_VOID (*pfnFreeResource)(IMG_VOID*, KRMResource *),
											   IMG_VOID *pvContext,
											   IMG_BOOL bRemoveFromListIfUnneeded)
{
	KRMResource *psNextResource, *psHold, *psDeadList;

	PVR_ASSERT(psMgr);
	PVR_ASSERT(ppsResourceList);

	KRM_ENTER_CRITICAL_SECTION(psMgr);

	psDeadList = IMG_NULL;

	psNextResource = *ppsResourceList;

	while(psNextResource)
	{
		if(IsResourceNeeded(psMgr, psNextResource))
		{
			/* It is needed. Skip it */
			psNextResource = psNextResource->psNext;
		}
		else
		{
			/* It is not needed. Free it and possibly remove it from the list */
			psHold = psNextResource->psNext;

			if(bRemoveFromListIfUnneeded)
			{
				RemoveResourceFromAllLists(psMgr, psNextResource);

				/* Build the list of dead resources to be deleted outside of the critical section */
				psNextResource->psNext = psDeadList;

				psDeadList = psNextResource;
			}
			else
			{
				/* Free the resource immediately.
				 * If it deadlocks it means someone didn't read the function limitations.
				 */
				pfnFreeResource(pvContext, psNextResource);
			}

			psNextResource = psHold;
			/* Do not update the 'prev' pointer */
		}
	}

	KRM_EXIT_CRITICAL_SECTION(psMgr);

	/* Deferred freeing of the resources (outside of the critical section,
	 * so the freeing function may call FRM routines).
	 */
	while(psDeadList)
	{
		psHold = psDeadList->psNext;

		/* This is important to avoid confusing IsResourceInList and similar functions */
		PVRSRVMemSet(psDeadList, 0, sizeof(KRMResource));

		pfnFreeResource(pvContext, psDeadList);

		psDeadList = psHold;
	}
}


/***********************************************************************************
 Function Name      : KRM_ReclaimUnneededResources
 Inputs             : psMgr
 Outputs            : psMgr
 Returns            : -
 Description        : Destroys all non-ghosted resources that are no longer needed.
                      Device and host memory may be freed as appropriate
************************************************************************************/
IMG_INTERNAL IMG_VOID KRM_ReclaimUnneededResources(IMG_VOID *pvContext, KRMKickResourceManager *psMgr)
{
	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(pvContext);

	ReclaimUnneededResourcesInList(psMgr, &psMgr->psResourceList,
                                   psMgr->pfnReclaimResourceMem, pvContext, psMgr->bRemoveResourceAfterRecoveringMem);
}


/***********************************************************************************
 Function Name      : KRM_DestroyUnneededGhosts
 Inputs             : psMgr
 Outputs            : psMgr
 Returns            : -
 Description        : Destroys all ghosts that are no longer needed.
                      Both their device and their host memory is freed.
************************************************************************************/
IMG_INTERNAL IMG_VOID KRM_DestroyUnneededGhosts(IMG_VOID *pvContext, KRMKickResourceManager *psMgr)
{
	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(pvContext);

	ReclaimUnneededResourcesInList(psMgr, &psMgr->psGhostList, psMgr->pfnDestroyGhost, pvContext, IMG_TRUE);
}


/***********************************************************************************
 Function Name      : RemoveResourceFromAllLists
 Inputs             : psMgr, psResource
 Outputs            : -
 Returns            : -
 Description        : Removes any resource from any list it was in.
************************************************************************************/
static IMG_VOID RemoveResourceFromAllLists(KRMKickResourceManager *psMgr, KRMResource *psResource)
{
	IMG_UINT32 ui32NextAttachment, ui32AttachmentToDelete;

	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(psResource);

	/* Remove the resource from any list it is in */
	if(psResource->psPrev)
	{
		psResource->psPrev->psNext = psResource->psNext;
	}

	if(psResource->psNext)
	{
		psResource->psNext->psPrev = psResource->psPrev;
	}

	/* ...and update the pointers if the element was the head of any list. */
	if(psMgr->psResourceList == psResource)
	{
		psMgr->psResourceList = psResource->psNext;
	}
	else if(psMgr->psGhostList == psResource)
	{
		psMgr->psGhostList = psResource->psNext;
	}
	
	/* Free all of its attachments */
	ui32NextAttachment = psResource->ui32FirstAttachment;

	while(ui32NextAttachment)
	{
		PVR_ASSERT(ui32NextAttachment < psMgr->ui32MaxAttachments);

		ui32AttachmentToDelete = ui32NextAttachment;

		ui32NextAttachment     = psMgr->asAttachment[ui32NextAttachment].ui32Next;

		FreeAttachment(psMgr, ui32AttachmentToDelete);
	}

	/* Reset the resource */
	PVRSRVMemSet(psResource, (IMG_UINT8)0, sizeof(KRMResource));
}


/***********************************************************************************
 Function Name      : KRM_RemoveResourceFromAllLists
 Inputs             : psMgr, psResource
 Outputs            : -
 Returns            : -
 Description        : Removes any resource from any list it was in. Thread safe.
************************************************************************************/
IMG_INTERNAL IMG_VOID KRM_RemoveResourceFromAllLists(KRMKickResourceManager *psMgr, KRMResource *psResource)
{
	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(psResource);

	KRM_ENTER_CRITICAL_SECTION(psMgr);

	RemoveResourceFromAllLists(psMgr, psResource);

	KRM_EXIT_CRITICAL_SECTION(psMgr);
}


/***********************************************************************************
 Function Name      : RemoveSurfaceReferencesInList
 Inputs             : psMgr, pvAttachmentPoint, psResourceList
 Outputs            : psMgr
 Returns            : Success.
 Description        : Frees the attachments associated to this attachment point.
************************************************************************************/
static IMG_VOID RemoveAttachmentPointReferencesInList(KRMKickResourceManager *psMgr, IMG_VOID *pvAttachmentPoint, KRMResource *psResourceList)
{
	KRMResource *psNextResource;

	PVR_ASSERT(psMgr);
	PVR_ASSERT(pvAttachmentPoint);

	psNextResource = psResourceList;

	while(psNextResource)
	{
		IMG_UINT32 ui32PrevAttachment, ui32NextAttachment;

		ui32NextAttachment = psNextResource->ui32FirstAttachment;
		ui32PrevAttachment = 0;

		/* Iterate all attachments of the current resource */
		while(ui32NextAttachment)
		{
			PVR_ASSERT(ui32NextAttachment < psMgr->ui32MaxAttachments);

			if(psMgr->asAttachment[ui32NextAttachment].pvAttachmentPoint == pvAttachmentPoint)
			{
				IMG_UINT32 ui32Hold;

				/* This is an attachment to the attachment point that is going to be deleted. Destroy it */
				ui32Hold = psMgr->asAttachment[ui32NextAttachment].ui32Next;

				if(ui32PrevAttachment)
				{
					/* Update the 'next' pointer of the previous attachment */
					psMgr->asAttachment[ui32PrevAttachment].ui32Next = ui32Hold;
				}
				else
				{
					/* This attachment was the first in the list. */
					psNextResource->ui32FirstAttachment = ui32Hold;
				}

				FreeAttachment(psMgr, ui32NextAttachment);

				/* The pointer to the previous element remains unchanged */
				ui32NextAttachment = ui32Hold;
			}
			else
			{
				/* The attachment is to a different attachment point. Update the pointer to the previous element. */
				ui32PrevAttachment = ui32NextAttachment;
				ui32NextAttachment = psMgr->asAttachment[ui32PrevAttachment].ui32Next;
			}
		}

		psNextResource = psNextResource->psNext;
	}
}


/***********************************************************************************
 Function Name      : KRM_RemoveAttachmentPointReferences
 Inputs             : psMgr, pvAttachmentPoint
 Outputs            : psMgr
 Returns            : Success.
 Description        : Makes the manager remove any references to the given surface
					  or context. All attachments to that surface/context are 
					  therefore eliminated.
************************************************************************************/
IMG_INTERNAL IMG_VOID KRM_RemoveAttachmentPointReferences(KRMKickResourceManager *psMgr, IMG_VOID *pvAttachmentPoint)
{
	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(pvAttachmentPoint);

	KRM_ENTER_CRITICAL_SECTION(psMgr);

	/* Iterate all resources */
	RemoveAttachmentPointReferencesInList(psMgr, pvAttachmentPoint, psMgr->psResourceList);

	/* Iterate all ghosts */
	RemoveAttachmentPointReferencesInList(psMgr, pvAttachmentPoint, psMgr->psGhostList);

	KRM_EXIT_CRITICAL_SECTION(psMgr);
}


/***********************************************************************************
 Function Name      : KRM_Destroy
 Inputs             : psMgr
 Outputs            : psMgr
 Returns            : -
 Description        : Destroys the given kick resource manager.
************************************************************************************/
IMG_INTERNAL IMG_VOID KRM_Destroy(IMG_VOID *pvContext, KRMKickResourceManager *psMgr)
{
	KRMResource *psGhostToDestroy;
#if defined(DEBUG)
	IMG_BOOL bAlreadyPrintedError = IMG_FALSE;
#endif /* defined(DEBUG) */

	/* Silently ignore uninitialized managers */
	if(!psMgr || !psMgr->bInitialized)
	{
		return;
	}

	/* The resource list should be empty at this point */
	PVR_ASSERT(!psMgr->psResourceList);

	/* Destroy all the ghosts. In debug mode, issue an error if any of them was needed */
	while(psMgr->psGhostList)
	{
		psGhostToDestroy = psMgr->psGhostList;

		/* Be polite and wait a reasonable time until the ghost is no longer needed */
		WaitUntilResourceIsNotNeeded(psMgr, psGhostToDestroy, KRM_DEFAULT_WAIT_RETRIES);

#ifdef DEBUG
		/* Safety check in case the ghost is still in use. */
		if(IsResourceNeeded(psMgr, psGhostToDestroy) && !bAlreadyPrintedError)
		{
			bAlreadyPrintedError = IMG_TRUE;

			PVR_DPF((PVR_DBG_ERROR, "KRM_Destroy: Destroying a ghost that is still needed for a frame or TA\n"));
		}
#endif /* DEBUG */

		RemoveResourceFromAllLists(psMgr, psGhostToDestroy);

		psMgr->pfnDestroyGhost(pvContext, psGhostToDestroy);
	}

	/* Free the attachment pool */
	PVRSRVFreeUserModeMem(psMgr->asAttachment);

	/* Reset all variables with zeroes as mandated by the header file */
	PVRSRVMemSet(psMgr, (IMG_UINT8)0, sizeof(KRMKickResourceManager));
}


/***********************************************************************************
 Function Name      : KRM_FlushUnKickedResource
 Inputs             : psMgr, psResource, pvContext, pfnScheduleTA
 Outputs            : -
 Returns            : Success
 Description        : Schedules a render/TA for unkicked attachments
************************************************************************************/
IMG_INTERNAL IMG_BOOL KRM_FlushUnKickedResource(const KRMKickResourceManager *psMgr,	
												const KRMResource *psResource,
												IMG_VOID *pvContext,
												IMG_VOID (*pfnScheduleTA)(IMG_VOID *, IMG_VOID *))
{
	IMG_UINT32 ui32NextAttachment;

	KRM_ENTER_CRITICAL_SECTION(psMgr);

	PVR_ASSERT(psMgr);
	PVR_ASSERT(psMgr->bInitialized);
	PVR_ASSERT(psResource);
	PVR_ASSERT(pfnScheduleTA);

	/*
		psAttachment->ui32Value > psAttachment->psStatusUpdate->ui32StatusValue) -> not possible!

		psAttachment->ui32Value == psAttachment->psStatusUpdate->ui32StatusValue) -> used in this frame, not kicked

		psAttachment->ui32Value < psAttachment->psStatusUpdate->ui32StatusValue) -> used in previously kicked frame
	*/

	/* Iterate all frames/contexts this resource is attached to */
	ui32NextAttachment = psResource->ui32FirstAttachment;

	switch(psMgr->eType)
	{
		case KRM_TYPE_3D:
		{
			while(ui32NextAttachment)
			{
				KRMAttachment *psAttachment;

				psAttachment = &psMgr->asAttachment[ui32NextAttachment];

				if(psAttachment->ui32Value == psAttachment->psStatusUpdate->ui32StatusValue)
				{
					pfnScheduleTA(pvContext, (IMG_VOID *)psAttachment->pvAttachmentPoint);
				}

				ui32NextAttachment = psAttachment->ui32Next;
			}

			break;
		}
		case KRM_TYPE_TA:
		{
			while(ui32NextAttachment)
			{
				KRMAttachment *psAttachment;

				psAttachment = &psMgr->asAttachment[ui32NextAttachment];

				if(psAttachment->ui32Value == psAttachment->psStatusUpdate->ui32StatusValue)
				{
					pfnScheduleTA((IMG_VOID *)psAttachment->pvAttachmentPoint, IMG_NULL);
				}

				ui32NextAttachment = psAttachment->ui32Next;
			}

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "KRM_FlushUnKickedResource: Invalid resource manager type"));

			KRM_EXIT_CRITICAL_SECTION(psMgr);

			return IMG_FALSE;
		}
	}

	KRM_EXIT_CRITICAL_SECTION(psMgr);

	return IMG_TRUE;
}

/******************************************************************************
 End of file (kickresource.c)
******************************************************************************/
