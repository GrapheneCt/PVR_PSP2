/**************************************************************************
 * Name         : kickresource.h
 *
 * Copyright    : 2006-2010 by Imagination Technologies Limited. All rights reserved.
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
 * Platform     : ANSI
 *
 * $Log: kickresource.h $
 *
 *
 *  --- Revision Logs Removed --- 
 */

#ifndef _KICKRESOURCE_H_
#define _KICKRESOURCE_H_


/*
 * EXPLANATION OF THIS FILE. PLEASE UPDATE WHEN CHANGES ARE MADE TO THE CODE.
 *
 * The structs and functions in this file have the following main purposes:
 *
 *    - Answering the question "Is this resource needed to finish any render?"
 *          If the resource is for example a texture, the answer to this question
 *          determines whether we need to ghost it if the app tries to modify it.
 *
 *    - Destroying all ghosted resources that are no longer needed to finish any render.
 *          This is done for example every time a new frame is started.
 *
 *    - Freeing the device mem of all non-needed non-ghosted resources.
 *          This is an expensive operation done when we run out of device memory.
 *
 *    - Waiting until all or a given resource is no longer needed by any render.
 *          Done during shutdown to prevent corrupted framebuffers and/or crashes.
 *
 *
 * In order to implement this functionality we define an entity called a Frame Resource Manager.
 * For any given resource, the manager keeps a list of all surfaces that require the resource to
 * finish a render. Since this means one list per resource, list nodes are pooled to reduce
 * the number of memory allocation calls. At the moment, the pool can only increase in size.
 *
 * Apart from that, the manager also keeps a list with all non-ghosted resources, and a list with
 * all ghosted resources. These lists are ocassionally traversed to find out all resources/ghosts
 * that are no longer needed by any frame --and thus can be safely freed.
 *
 */


/*
 * A resource is any piece of data that is needed in order to finish rendering a frame
 * in a given render surface (eg shaders, textures and ghosted textures), 
 * or process a TA kick in a given context (eg buffer objects ) 
 *
 * IMPORTANT: Initialize with zeroes.
 */
typedef struct KRMResourceRec
{
	/*
	 * Index to the head of the list of attachments the resource is attached to.
	 * If its value is zero, it means the resource is not attached to any frame/context.
	 * The offset is relative to KRMResourceManager->asAttachment.
	 */
	IMG_UINT32  ui32FirstAttachment;

	/* Main doubly-linked list of resources. */
	struct KRMResourceRec *psPrev, *psNext;

} KRMResource;


typedef enum KRMType_TAG
{
	KRM_TYPE_3D	= 2,
	KRM_TYPE_TA	= 4,

} KRMType;

/* 
 * Structure used to pass both mem info and status value to KRM procedures in place of
 * SyncData structures. 
 */
typedef struct KRMStatusUpdateRec
{
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;

	IMG_UINT32 ui32StatusValue;

	IMG_BOOL bPDumpInitialised;

} KRMStatusUpdate;

/*
 * Struct used internally by KRMResourceManager.
 *
 *   Describes an attachment between a frame (in a certain surface) and a resource 
 *   This struct is used to keep track of what surfaces is a resource attached to. When a resource is only
 *   attached to completed frames, the resource can be freed without any negative consequences.
 *
 * OR
 *
 *   Describes an attachment between a TA kick (in a certain context) and a resource
 *   This struct is used to keep track of what contexts is a resource attached to. When a resource is only
 *   attached to completed TA kicks, the resource can be freed without any negative consequences.
 */
typedef struct KRMAttachmentRec
{
	/* 
	 * This is a either a context or a surface, depending on eType.
	 */
	const IMG_VOID *pvAttachmentPoint;

	/* 
	 * This is a either a render value or TA kick value, depending on eType.
	 */
	IMG_UINT32 ui32Value;

	/*
	 * This is a direct pointer to the KRMStatusUpdate data for pvAttachmentPoint to avoid doing any dereferences.
	 */
	const KRMStatusUpdate *psStatusUpdate;

	/*
	 * This is used to keep a list of all attachment points depending on a given resource.
	 * The end of the list is represented with zero.
	 */
	IMG_UINT32 ui32Next;

} KRMAttachment;


/*
 * Per-resource-type object that keeps track of what frames/kicks depend on what resources in order to
 * allow the driver free memory when needed.
 *
 * IMPORTANT: Initialize with zeroes.
 */
typedef struct KRMKickResourceManagerRec
{
	/* Resource type - context or a surface*/
	KRMType				eType;

    /* Whether to use Mutex lock  */
    IMG_BOOL			bUseLock;

	/* Mutex (used internally) */
	PVRSRV_MUTEX_HANDLE	hSharedLock;

	/* Pool for all frame-resource attachments. Element zero is reserved (equivalent to NULL). */
	KRMAttachment		*asAttachment;

	/* Number of elements allocated for the array above. */
	IMG_UINT32			ui32MaxAttachments;

	/* Index of the first free element of the array above.
	 * asAttachment[ui32AttachmentFreeList].ui32Next is the second free element, etc.
	 * Zero is used to mark the end of the list.
	 */
	IMG_UINT32			ui32AttachmentFreeList;

	/* Head of the doubly-linked list of all of non-ghosted resources */
	KRMResource         *psResourceList;

	/* Function pointer to recover memory from a non-ghosted resource.
	 * The resource may itself be destroyed and removed from the KRM (including its device memory), or
	 * its device memory alone may be freed.
	 */
	IMG_VOID			(*pfnReclaimResourceMem)(IMG_VOID *pvContext, KRMResource *psResource);
	
	/* Will the resource be removed from all lists after its memory is recovered */
	IMG_BOOL			bRemoveResourceAfterRecoveringMem;

	/* Head of the doubly-linked list of all ghosted resources */
	KRMResource			*psGhostList;

	/* Function pointer to destroy a ghosted resource.
	 * The ghost's host memory is freed along with its device memory.
	 */
	IMG_VOID			(*pfnDestroyGhost)(IMG_VOID *pvContext, KRMResource *psGhost);

	/* Handle to services for event object waiting */
	PVRSRV_DEV_DATA		*psDevData;
	
	/* Handle to object to wait on for event object waiting */
	IMG_HANDLE			hOSEvent;

	/* Whether the manager has been successfully initialized */
	IMG_BOOL			bInitialized;

} KRMKickResourceManager;


#if defined(CLIENT_DRIVER_DEFAULT_WAIT_RETRIES)
#define KRM_DEFAULT_WAIT_RETRIES CLIENT_DRIVER_DEFAULT_WAIT_RETRIES
#else
#define KRM_DEFAULT_WAIT_RETRIES 100
#endif

/*
 * Initializes the given kick resource manager.
 * This function must be called before any other, and only once per manager.
 * Returns IMG_TRUE if successfull. IMG_FALSE otherwise.
 */
IMG_INTERNAL IMG_BOOL KRM_Initialize(KRMKickResourceManager			*psMgr,
									 KRMType						 eType,
									 IMG_BOOL                        bUseLock,
									 PVRSRV_MUTEX_HANDLE			 pvSharedLock,
									 PVRSRV_DEV_DATA				*psDevData,
									 IMG_HANDLE						 hOSEvent,
                                     IMG_VOID  (*pfnReclaimResourceMem)(IMG_VOID *, KRMResource*),
                                     IMG_BOOL                        bRemoveResourceAfterRecoveringMem,
                                     IMG_VOID  (*pfnDestroyGhost)    (IMG_VOID *, KRMResource*));

/*
 * Attaches a resource to a given surface/context. The resource must not be a ghost, but that error is undetected.
 * Returns IMG_TRUE is successfull, IMG_FALSE otherwise.
 */
IMG_INTERNAL IMG_BOOL KRM_Attach(KRMKickResourceManager *psMgr,
								 const IMG_VOID *pvAttachmentPoint,
                                 const KRMStatusUpdate *psStatusUpdate, 
                                 KRMResource *psResource);

/*
 * Returns whether a resource is in use for the current kick of the current context, or frame of the current surface.
 */
IMG_INTERNAL IMG_BOOL KRM_IsResourceInUse(const KRMKickResourceManager *psMgr,
										  const IMG_VOID *pvAttachmentPoint,
										  const KRMStatusUpdate *psStatusUpdate,
										  const KRMResource *psResource);
/*
 * Returns whether a resource is required to render any frame, or process a TA kick, or not.
 */
IMG_INTERNAL IMG_BOOL KRM_IsResourceNeeded(const KRMKickResourceManager *psMgr,
                                           const KRMResource *psResource);

/*
 * Waits until the resource is no longer required to render any frame, or process a TA kick, or 
 * until the number of retries reaches the maximum allowed. In the former case it returns IMG_TRUE
 * and in the later it returns IMG_FALSE. The resource may be a ghost or a non-ghost.
 */
IMG_INTERNAL IMG_BOOL KRM_WaitUntilResourceIsNotNeeded(const KRMKickResourceManager *psMgr,
                                                       const KRMResource *psResource, 
                                                       const IMG_UINT32 ui32MaxRetries);

/*
 * Waits until all ghosted and non-ghosted resources are not needed. It is equivalent to calling
 * KRM_WaitUntilResourceIsNotNeeded for each element in the resource and in the ghost list.
 */
IMG_INTERNAL IMG_BOOL KRM_WaitForAllResources(const KRMKickResourceManager *psMgr,
                                              const IMG_UINT32 ui32MaxRetries);

/*
 * Ghosts a resource. The ghost keeps all the reverse dependencies of the original, and the original is
 * updated to have no reverse dependencies.
 * Returns IMG_TRUE if successfull, or IMG_FALSE otherwise.
 */
IMG_INTERNAL IMG_BOOL KRM_GhostResource(KRMKickResourceManager *psMgr,
										KRMResource *psOriginalResource, 
										KRMResource *psGhostOfOriginalResource);


/*
 * Destroys all non-ghosted resources that are no longer needed.
 */
IMG_INTERNAL IMG_VOID KRM_ReclaimUnneededResources(IMG_VOID *pvContext, KRMKickResourceManager *psMgr);

/*
 * Destroys all ghosts that are no longer needed. Both their device and their host memory is freed.
 */
IMG_INTERNAL IMG_VOID KRM_DestroyUnneededGhosts(IMG_VOID *pvContext, KRMKickResourceManager *psMgr);

/*
 * Removes a non-ghosted resource from any list it was in.
 */
IMG_INTERNAL IMG_VOID KRM_RemoveResourceFromAllLists(KRMKickResourceManager *psMgr, 
                                                     KRMResource *psResource);

/*
 * Makes the manager remove any references to the given surface.
 */
IMG_INTERNAL IMG_VOID KRM_RemoveAttachmentPointReferences(KRMKickResourceManager *psMgr, IMG_VOID *pvAttachmentPoint);

/*
 * Destroys the given kick resource manager.
 * After calling this function, the manager is no longer valid until it is initialized again.
 *
 * IMPORTANT: Destroying the manager _does_not_ modify the resources but it _does_ destroy all ghosts. 
 */
IMG_INTERNAL IMG_VOID KRM_Destroy(IMG_VOID *pvContext, KRMKickResourceManager *psMgr);


/*
 * Kicks the TA/3D for an un-kicked resource
 */
IMG_INTERNAL IMG_BOOL KRM_FlushUnKickedResource(const KRMKickResourceManager *psMgr,	
												const KRMResource *psResource,
												IMG_VOID *pvContext,
												IMG_VOID (*pfnScheduleTA)(IMG_VOID *, IMG_VOID *));

#endif /* defined _KICKRESOURCE_H_ */
