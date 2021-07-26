/*!****************************************************************************
@File           egl_sync.c

@Title          Implements EGL_KHR_fence_sync draft extension

@Author         Imagination Technologies

@Date           2009/02/17

@Copyright      Copyright 2009 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or otherwise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform      	generic egl

@Description    Implements EGL_KHR_fence_sync draft extension

******************************************************************************/
/******************************************************************************
Modifications :-
$Log: egl_sync.c $
******************************************************************************/

#include "imgextensions.h"
#include "egl_internal.h"

#if defined(EGL_EXTENSION_KHR_FENCE_SYNC) || defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)

#include "img_types.h"
#include "tls.h"

#include "generic_ws.h"
#include "drvegl.h"
#include "drveglext.h"

#include "egl_sync.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

//TODOPSP2: find less strict implementation (needs to only check for writes, currently checks for reads too)
#define EGL_FENCE_SYNC_COMPLETE(psDevData, sync) !SGX2DQueryBlitsComplete(psDevData, (sync)->psSyncMemInfo->psClientSyncInfo, IMG_FALSE)

/* external symbols */
KEGL_DISPLAY *GetKEGLDisplay(TLS psTls, EGLDisplay eglDpy);


static void _waitFence(KEGL_SYNC *psSync, SrvSysContext *psSysContext, EGLTimeKHR timeout);
static EGLint IMGeglClientWaitSyncKHR_Fence(TLS psTls, KEGL_SYNC *psSync, EGLint flags, EGLTimeKHR timeout);

static EGLint IMGeglClientWaitSyncKHR_Reusable(TLS psTls, KEGL_SYNC *psSync, EGLint flags, EGLTimeKHR timeout);


/***********************************************************************************
 Function Name      : IsSync
 Inputs             : psDpy, psSync
 Outputs            : -
 Returns            : TRUE/FALSE
 Description        : Search the sync objects associated with a specified display
                      for a given sync object
************************************************************************************/
static EGLBoolean IsSync(KEGL_DISPLAY *psDpy, KEGL_SYNC *psSync)
{
	KEGL_SYNC *psSyncI;

	if(!psSync)
	{
		return EGL_FALSE;
	}

	for(psSyncI=psDpy->psHeadSync; psSyncI!=IMG_NULL; psSyncI=psSyncI->psNextSync)
	{
		if(psSync==psSyncI)
		{
			if ( psSync->bIsDeleting )
			{
				return EGL_FALSE;
			}
			return EGL_TRUE;
		}
	}

	return EGL_FALSE;
}

/***********************************************************************************
 Function Name      : _insertEglFence
 Inputs             : eglDpy, sync
 Outputs            : -
 Returns            : TRUE/FALSE
 Description        : internal function to insert a fence command into the GL command
                      stream.  Called when a fence object is created only.
************************************************************************************/
static EGLBoolean _insertEglFence(KEGL_CONTEXT *ctx, TLS psTls, KEGL_SYNC *psSync)
{
	PVRSRV_ERROR eResult;
	SrvSysContext *psSysContext;

	PVR_ASSERT(ctx);
	PVR_ASSERT(psTls);
	PVR_ASSERT(psSync);

	psTls->lastError = EGL_SUCCESS;

	psSysContext = &psTls->psGlobalData->sSysContext;

	/* FIXME - Although the fence is now outside of the API command queue, a check of context is retained.
	 * along with a check that the context is supported */

	if(ctx == EGL_NO_CONTEXT)
	{
		psTls->lastError = EGL_BAD_CONTEXT;
		return EGL_FALSE;
	}

#if !defined(SUPPORT_OPENGLES1) && !defined(SUPPORT_OPENGLES2) && !defined(API_MODULES_RUNTIME_CHECKED)
	PVR_UNREFERENCED_PARAMETER(psSync);
#else

	psSync->sQueueTransfer.eType = SGXTQ_FILL; 
	psSync->sQueueTransfer.Details.sFill.ui32Colour = 0xFFFFFFFF;

	psSync->sQueueTransfer.ui32NumSources = 0;
	SGX_QUEUETRANSFER_NUM_SRC_RECTS(psSync->sQueueTransfer) = 0;

	psSync->sQueueTransfer.ui32NumDest = 1;
	SGX_QUEUETRANSFER_NUM_DST_RECTS(psSync->sQueueTransfer) = 1;
	SGX_QUEUETRANSFER_DST_RECT(psSync->sQueueTransfer, 0).x0 = 0;
	SGX_QUEUETRANSFER_DST_RECT(psSync->sQueueTransfer, 0).y0 = 0;
	SGX_QUEUETRANSFER_DST_RECT(psSync->sQueueTransfer, 0).x1 = 1;
	SGX_QUEUETRANSFER_DST_RECT(psSync->sQueueTransfer, 0).y1 = 1;
	psSync->sQueueTransfer.asDests[0].ui32Width = 1;
	psSync->sQueueTransfer.asDests[0].ui32Height = 1;
	/* Minimum TQ API stride - not really used as this is a 1x1 blit */
	psSync->sQueueTransfer.asDests[0].i32StrideInBytes = EURASIA_ISPREGION_SIZEX * 4;
	psSync->sQueueTransfer.asDests[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
	psSync->sQueueTransfer.asDests[0].sDevVAddr.uiAddr = psSync->psSyncMemInfo->sDevVAddr.uiAddr;
	psSync->sQueueTransfer.asDests[0].eFormat = PVRSRV_PIXEL_FORMAT_ARGB8888;
	psSync->sQueueTransfer.asDests[0].psSyncInfo = psSync->psSyncMemInfo->psClientSyncInfo;
	psSync->sQueueTransfer.ui32NumStatusValues = 0;
	psSync->sQueueTransfer.ui32Flags = SGX_KICKTRANSFER_FLAGS_3DTQ_SYNC;
	psSync->sQueueTransfer.bPDumpContinuous = IMG_TRUE;

	eResult = SGXQueueTransfer(&psSysContext->s3D, psSysContext->hTransferContext, &psSync->sQueueTransfer);

	if(eResult != PVRSRV_OK)
	{
		psTls->lastError = EGL_BAD_ALLOC;
		return EGL_FALSE;
	}
	return EGL_TRUE;
#endif

	PVR_DPF((PVR_DBG_CALLTRACE, "_insertEglFence: unsupported context"));
	psTls->lastError = EGL_BAD_CONTEXT;
	return EGL_FALSE;
}

/***********************************************************************************
 Function Name      : _flushBuffers
 Inputs             : ctx, psTLS
 Outputs            : -
 Returns            : TRUE/FALSE
 Description        : internal function to call the API specific FlushBuffers call
************************************************************************************/
static EGLBoolean _flushBuffers(KEGL_CONTEXT *ctx, TLS psTls)
{
	IMG_EGLERROR eError = IMG_EGL_NO_ERROR; /* FlushBuffers returns an IMG_EGLERROR */
	EGLBoolean apiFound = EGL_FALSE;

	PVR_ASSERT(ctx);
	PVR_ASSERT(psTls);

#if defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)
	if(
		(ctx->eContextType==IMGEGL_CONTEXT_OPENGLES2) &&
		psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES2]
	)
	{
		PVR_ASSERT(psTls->psGlobalData->bHaveOGLES2Functions)
		PVR_ASSERT(psTls->psGlobalData->spfnOGLES2.pfnGLESFlushBuffersGC)

		eError = psTls->psGlobalData->spfnOGLES2.pfnGLESFlushBuffersGC(ctx->hClientContext, IMG_NULL, IMG_TRUE, IMG_FALSE, IMG_FALSE);

		apiFound = EGL_TRUE;
	}
#endif /* defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED) */

#if defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED)
	if(
		(ctx->eContextType==IMGEGL_CONTEXT_OPENGLES1) &&
		psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1]
	)
	{
		PVR_ASSERT(psTls->psGlobalData->bHaveOGLES1Functions)
		PVR_ASSERT(psTls->psGlobalData->spfnOGLES1.pfnGLESFlushBuffersGC)

		eError = psTls->psGlobalData->spfnOGLES1.pfnGLESFlushBuffersGC(ctx->hClientContext, IMG_NULL, IMG_TRUE, IMG_FALSE, IMG_FALSE);

		apiFound = EGL_TRUE;
	}
#endif /* defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED) */

#if defined(SUPPORT_OPENVG) || defined(SUPPORT_VGX) || defined(API_MODULES_RUNTIME_CHECKED)
	if(
		(ctx->eContextType==IMGEGL_CONTEXT_OPENVG) &&
		psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENVG]
	)
	{
		PVR_ASSERT(psTls->psGlobalData->bHaveOVGFunctions)
		PVR_ASSERT(psTls->psGlobalData->spfnOVG.pfnOVGFlushBuffersGC)

		eError = psTls->psGlobalData->spfnOVG.pfnOVGFlushBuffersGC(ctx->hClientContext, IMG_FALSE, IMG_FALSE);

		apiFound = EGL_TRUE;
	}
#endif /* defined(SUPPORT_OPENVG) || defined(SUPPORT_VGX) || defined(API_MODULES_RUNTIME_CHECKED) */

#if defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED)
	if(
		(ctx->eContextType==IMGEGL_CONTEXT_OPENGL) &&
		psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGL]
	)
	{
		PVR_ASSERT(psTls->psGlobalData->bHaveOGLFunctions)
		PVR_ASSERT(psTls->psGlobalData->spfnOGL.pfnGLFlushBuffersGC)

		eError = psTls->psGlobalData->spfnOGL.pfnGLFlushBuffersGC(ctx->hClientContext, IMG_FALSE, IMG_FALSE);

		apiFound = EGL_TRUE;
	}
#endif /* defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED) */

	if(eError == IMG_EGL_MEMORY_INVALID_ERROR)
	{
		psTls->lastError = EGL_CONTEXT_LOST;

		return EGL_FALSE;
	}
	else if(eError != IMG_EGL_NO_ERROR)
	{
		psTls->lastError = EGL_BAD_CONTEXT;

		return EGL_FALSE;
	}

	return apiFound;
}

/***********************************************************************************
 Function Name      : _deleteSync
 Inputs             : psSync
 Outputs            : -
 Returns            : TRUE/FALSE
 Description        : internal function to free the sync object and its device memory
                      must always be called synchrously (in a locked Tls)
************************************************************************************/
static EGLBoolean _deleteSync(KEGL_SYNC *psSync, SrvSysContext *psSysContext)
{
	PVR_ASSERT(psSync);
	PVR_ASSERT(psSysContext);

	if(!EGL_FENCE_SYNC_COMPLETE(&psSysContext->s3D, psSync))
	{
		return EGL_FALSE;
	}

	/* unbind the sync from the display */
	if(psSync->psDpy->psHeadSync != psSync)
	{
		KEGL_SYNC *psSyncI = psSync->psDpy->psHeadSync;

		/* Find Sync previous to this one */
		while(psSyncI->psNextSync != psSync)
		{
			psSyncI = psSyncI->psNextSync;
		}

		/* Unlink our Sync from the list */
		psSyncI->psNextSync = psSync->psNextSync;
	}
	else
	{
		/* Just need to relink head (may be relinked to NULL) */
		psSync->psDpy->psHeadSync = psSync->psDpy->psHeadSync->psNextSync;
	}

	if ( psSync->eglSyncType == EGL_SYNC_FENCE_KHR )
	{
		/* Free the sync object and dummy memory */
		if(IMGEGLFREEDEVICEMEM( &psSysContext->s3D,
								psSync->psSyncMemInfo) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"DeleteSync: Could not free Fence Sync dummy memory"));
			return EGL_FALSE;
		}
	}
#if defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)
	else
	{
		PVRSRVDestroySemaphore( psSync->hReusableSemaphore );
	}
#endif

	EGLFree(psSync);

	return EGL_TRUE;
}

/***********************************************************************************
 Function Name      : _cleanupSyncs
 Inputs             : psDpy 
 Outputs            : -
 Returns            : void
 Description        : Delete completed syncs that are flagged for delete
************************************************************************************/
static void _cleanupSyncs(KEGL_DISPLAY * psDpy, SrvSysContext *psSysContext)
{
	KEGL_SYNC *psSync = psDpy->psHeadSync;

	/* search the syncs associated with a specified display and delete those marked for delete */
	while(psSync)
	{
		KEGL_SYNC *psNextSync = psSync->psNextSync;

		if(psSync->psDpy == psDpy)
		{
			if(psSync->bIsDeleting && psSync->refcount==0 && EGL_FENCE_SYNC_COMPLETE(&psSysContext->s3D, psSync))
			{
				_deleteSync(psSync, psSysContext);
			}
		}

		psSync = psNextSync;
	}	
}


/***********************************************************************************
 Function Name      : TerminateSyncs
 Inputs             : psDpy 
 Outputs            : -
 Returns            : void
 Description        : call destroy on remaining eglSyncs.
************************************************************************************/
IMG_INTERNAL IMG_VOID TerminateSyncs(KEGL_DISPLAY * psDpy, SrvSysContext *psSysContext)
{
	KEGL_SYNC *psSync = psDpy->psHeadSync;

	/* search the syncs associated with a specified display and mark or delete */
	while(psSync)
	{
		KEGL_SYNC *psNextSync = psSync->psNextSync;

		if(psSync->psDpy == psDpy)
		{
			/* We wait if there are any ClientWaitSyncs outstanding or
			 * if there were no waits issued, we still have to wait for TQ to complete */
			if(psSync->refcount>0 || !EGL_FENCE_SYNC_COMPLETE(&psSysContext->s3D, psSync))
			{
				/* wait for TQ to complete */
				_waitFence(psSync, psSysContext, EGL_FOREVER_KHR);
			}
			if(psSync)
			{
				if(EGL_FENCE_SYNC_COMPLETE(&psSysContext->s3D, psSync))
				{
					if(!_deleteSync(psSync, psSysContext))
					{
						PVR_DPF((PVR_DBG_ERROR, "Delete Sync Failed %p", psSync));
					}
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR, "Sync did not complete after wait - cannot delete %p", psSync));
				}
			}

		}

		psSync = psNextSync;
	}	
}

/***********************************************************************************
 Function Name      : IMGeglCreateSyncKHR
 Inputs             : eglDpy, condition, pAttribList
 Outputs            : -
 Returns            : EGLSyncKHR (void*)
 Description        : 
************************************************************************************/
IMG_INTERNAL EGLSyncKHR	IMGeglCreateSyncKHR(EGLDisplay eglDpy, EGLenum type, const EGLint *piAttribList)
{
	KEGL_DISPLAY    *psDpy;
	KEGL_SYNC       *psSync = IMG_NULL;
	TLS             psTls;
	KEGL_CONTEXT    *psCurrentContext = EGL_NO_CONTEXT;
	SrvSysContext   *psSysContext;

	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglCreateSyncKHR"));

	psTls = TLS_Open(_TlsInit);

	if (!psTls)
	{
		goto err_out;
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglCreateSyncKHR);

	psTls->lastError = EGL_SUCCESS;

	/* --------------------------------------------------------------*/

	switch (type)
	{
#if defined(EGL_EXTENSION_KHR_FENCE_SYNC)
		case EGL_SYNC_FENCE_KHR:
			break;
#endif /* defined(EGL_EXTENSION_KHR_FENCE_SYNC) */
#if defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)
		case EGL_SYNC_REUSABLE_KHR:
			break;
#endif /* defined(EGL_EXTENSION_KHR_REUSABLE_SYNC) */
		default:
			psTls->lastError = EGL_BAD_ATTRIBUTE;

			goto err_stop_timer;
	}

	/* We expect a null or empty attrib list for fence sync objects */
	if(piAttribList)
	{
		if (*piAttribList != EGL_NONE)
		{
			psTls->lastError = EGL_BAD_ATTRIBUTE;
			goto err_free_sync;
		}
	}

	/* Get an EGL_DISPLAY pointer for the passed display */
	psDpy = GetKEGLDisplay(psTls, eglDpy);

	if(!psDpy)
	{
		psTls->lastError = EGL_BAD_DISPLAY;

		goto err_stop_timer;
	}

	/* In case of fence sync check the client API. */
	if(type == EGL_SYNC_FENCE_KHR)
	{

#       if defined(API_MODULES_RUNTIME_CHECKED)
		{
			/*  Valid case: API is OpenGL ES and at least one of ES1 / ES2 is present  */
			if(
				(psTls->ui32API!=IMGEGL_API_OPENGLES) ||
				(
					!psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1] &&
					!psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES2]
				)
			)
			{
				psTls->lastError = EGL_BAD_MATCH;
			}
		}
#       else /* defined(API_MODULES_RUNTIME_CHECKED) */
		{

			/* The client API must support placing fence commands */
			/* slightly odd code is due to compiler warning that code is unreachable */
#           if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2)
			if(psTls->ui32API!=IMGEGL_API_OPENGLES)
#           endif
			{
				psTls->lastError = EGL_BAD_MATCH;
			}
		}
#       endif /* defined(API_MODULES_RUNTIME_CHECKED) */

		if(psTls->lastError==EGL_BAD_MATCH)
		{
			goto err_stop_timer;
		}

		/* retrieve the current context for the current api */
		psCurrentContext = psTls->apsCurrentContext[psTls->ui32API];
	
		if (psCurrentContext == EGL_NO_CONTEXT)
		{
			psTls->lastError = EGL_BAD_MATCH;
	
			goto err_stop_timer;
		}
	
		/* The display must match the that of the currently bound context */
		/* for the currently bound API */
		if (psDpy != psCurrentContext->psDpy)
		{
			psTls->lastError = EGL_BAD_MATCH;
	
			goto err_stop_timer;
		}
	}

	/* --------------------------------------------------------------*/


	/* Create the sync object */
	psSync = EGLCalloc(sizeof(KEGL_SYNC));

	if(!psSync)
	{
		psTls->lastError = EGL_BAD_ALLOC;

		goto err_stop_timer;
	}

	/* default attribute values */
	psSync->eglSyncType = type;
	psSync->eglSyncStatus = EGL_UNSIGNALED_KHR;
	psSync->eglSyncCondition = EGL_SYNC_PRIOR_COMMANDS_COMPLETE_KHR;

	psSync->psDpy = psDpy;
	psSync->psContext = psCurrentContext;

	psSync->refcount = 0; /* 0 means psSync can be deleted. Each waiter will inc refcount */
	psSync->bIsDeleting = EGL_FALSE;

	if(type == EGL_SYNC_FENCE_KHR)
	{
		/* Allocate dummy device memory to get a services sync object */
		psSysContext = &psTls->psGlobalData->sSysContext;

		/* Allocate device memory for receiving TQ update */
		psSysContext = &psTls->psGlobalData->sSysContext;
		if (IMGEGLALLOCDEVICEMEM(   &psSysContext->s3D,
									psSysContext->hGeneralHeap,
									PVRSRV_MEM_WRITE | PVRSRV_MEM_READ | PVRSRV_MEM_CACHE_CONSISTENT,
									4,                      /* size */
									4,                      /* alignment */
									&psSync->psSyncMemInfo  /* for receiving the mem info */
								) != PVRSRV_OK )
		{
			psTls->lastError = EGL_BAD_ALLOC;
			goto err_free_sync;
		}

		PVRSRVMemSet(psSync->psSyncMemInfo->pvLinAddr, 0, 4);

#if defined(PDUMP)
		PVRSRVPDumpMem(	psSysContext->psConnection,
						IMG_NULL, psSync->psSyncMemInfo,
						0, 4,
						PVRSRV_PDUMP_FLAGS_CONTINUOUS);
#endif /* defined(PDUMP) */

		/* Flush the buffers to get the TA/3D to complete */
		if(!_flushBuffers(psSync->psContext, psTls))
		{
			/* If flush fails and last error is set, bail out */
			/* Otherwise it is just a case of no context or context that can't be flushed */
			if(psTls->lastError != EGL_SUCCESS)
			{
				PVR_DPF((PVR_DBG_ERROR,"IMGeglCreateSyncKHR: Flushbuffers failed"));
			}
		}

		/* Place a fence command into the GL command stream */
		if(!_insertEglFence(psSync->psContext, psTls, psSync))
		{
			goto err_dealloc_device_mem;
		}

	}
#if defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)
	else
	{
		/* Must be reusable sync. */
		psSync->ui32WaitValue = 0;
		if ( PVRSRVCreateSemaphore( &psSync->hReusableSemaphore, 0 ) != PVRSRV_OK )
		{
			psTls->lastError = EGL_BAD_ALLOC;
			goto err_free_sync;
		}
		psSync->ui64WaitLevel = 0;
	}
#endif

	EGLThreadLock(psTls);
	
	/* Chain the new image onto the displays sync list */
	psSync->psNextSync = psDpy->psHeadSync;
	psDpy->psHeadSync = psSync;

	EGLThreadUnlock(psTls);
	
	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateSyncKHR);

	return (EGLSyncKHR)psSync; /* returns void* */

err_dealloc_device_mem:
	if(IMGEGLFREEDEVICEMEM( &psSysContext->s3D,
							psSync->psSyncMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"IMGeglCreateSyncKHR: Could not free Fence Sync dummy memory"));
	}

err_free_sync:

	EGLFree(psSync);

err_stop_timer:

	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateSyncKHR);

err_out:

	return EGL_NO_SYNC_KHR;
}



/***********************************************************************************
 Function Name      : IMGeglDestroySyncKHR
 Inputs             : eglDpy, sync
 Outputs            : -
 Returns            : TRUE/FALSE
 Description        : 
************************************************************************************/
IMG_INTERNAL EGLBoolean IMGeglDestroySyncKHR(EGLDisplay eglDpy, EGLSyncKHR sync)
{
	KEGL_SYNC *psSync = (KEGL_SYNC *)sync;
	EGLBoolean ret = EGL_FALSE;
	KEGL_DISPLAY *psDpy;
	TLS	psTls;
	SrvSysContext *psSysContext;

	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglDestroySyncKHR"));

	psTls = TLS_Open(_TlsInit);

	if (!psTls)
	{
		goto err_out;
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglDestroySyncKHR);

	psTls->lastError = EGL_SUCCESS;

	/* TODO: Behaviour undefined by spec
			 (NULL display)
	 */
	psDpy = GetKEGLDisplay(psTls, eglDpy);

	if(!psDpy)
	{
		psTls->lastError = EGL_BAD_DISPLAY;

		goto err_stop_timer;
	}

	EGLThreadLock(psTls);

	if(!IsSync(psDpy, psSync))
	{
		psTls->lastError = EGL_BAD_PARAMETER;

		goto err_unlock;
	}

	/* TODO: Behaviour undefined by spec
	         (valid current Dpy does not match CreateSync Dpy)
	 */
	if(psSync->psDpy != psDpy)
	{
		psTls->lastError = EGL_BAD_MATCH;

		goto err_unlock;
	}

#if defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)
	if ( psSync->eglSyncType != EGL_SYNC_FENCE_KHR )
	{
		/* Must be reusable sync. */
		if ( psSync->ui32WaitValue > 0 )
		{
			psSync->ui64WaitLevel++;
			PVRSRVPostSemaphore( psSync->hReusableSemaphore, psSync->ui32WaitValue );
			psSync->ui32WaitValue = 0;
		}
	}
#endif

	psSysContext = &psTls->psGlobalData->sSysContext;

	/* If there are any ClientWaits blocking on this sync, just mark for delete */
	if(psSync->refcount>0 || !EGL_FENCE_SYNC_COMPLETE(&psSysContext->s3D, psSync))
	{
		psSync->bIsDeleting=EGL_TRUE;
	}
	else
	{
		ret = _deleteSync(psSync, psSysContext);
	}

	_cleanupSyncs(psDpy, psSysContext);

err_unlock:

	EGLThreadUnlock(psTls);

err_stop_timer:

	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroySyncKHR);

err_out:

	return ret;
}



/***********************************************************************************
 Function Name      : IMGeglClientWaitSyncKHR
 Inputs             : eglDpy, sync, flags, timeout
 Outputs            : -
 Returns            : status
 Description        : Extension entrypoint: Calls the appropriate wait function for
                      Fence or Reusable sync
					  Note: you can't wait on a deleted sync
************************************************************************************/
IMG_INTERNAL EGLint IMGeglClientWaitSyncKHR(EGLDisplay eglDpy, EGLSyncKHR *sync, EGLint flags, EGLTimeKHR timeout)
{
	EGLint ret = EGL_FALSE;
	KEGL_SYNC *psSync = (KEGL_SYNC*)sync;
	KEGL_DISPLAY *psDpy;
	TLS	psTls;
	SrvSysContext *psSysContext;

	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglClientWaitSyncKHR"));

	if(!psSync)
	{
		goto err_out;
	}

	psTls = TLS_Open(_TlsInit);

	if (!psTls)
	{
		goto err_out;
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglClientWaitSyncKHR);

	psTls->lastError = EGL_SUCCESS;
	psSysContext = &psTls->psGlobalData->sSysContext;

	/* TODO: Behaviour undefined by spec
	         (NULL display)
	 */
	psDpy = GetKEGLDisplay(psTls, eglDpy);

	if(!psDpy)
	{
		psTls->lastError = EGL_BAD_DISPLAY;

		goto err_stop_timer;
	}

	EGLThreadLock(psTls);

	if(!IsSync(psDpy, psSync))
	{
		psTls->lastError = EGL_BAD_PARAMETER;

		goto err_unlock;
	}

	/* TODO: Behaviour undefined by spec
	         (valid current Dpy does not match CreateSync Dpy)
	 */
	if(psSync->psDpy != psDpy)
	{
		psTls->lastError = EGL_BAD_MATCH;

		goto err_unlock;
	}

	if ( psSync->eglSyncType == EGL_SYNC_FENCE_KHR )
	{

#		if defined(API_MODULES_RUNTIME_CHECKED)
		{
			/*  Valid case: API is OpenGL ES and at least one of ES1 / ES2 is present  */
			if(
				(psTls->ui32API != IMGEGL_API_OPENGLES) ||
				(
					!psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1] &&
					!psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES2]
				)
			)
			{
				psTls->lastError = EGL_BAD_MATCH;
			}

		}
#		else /* defined(API_MODULES_RUNTIME_CHECKED) */
		{

			/* The client API must support placing fence commands */
			/* Currently this is ES1/2 only. New support should add further checks here */
			/* slightly odd code is due to compiler warning that code is unreachable */
#			if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2)
			if(psTls->ui32API!=IMGEGL_API_OPENGLES)
#			endif
			{
				psTls->lastError = EGL_BAD_MATCH;
			}
		}
#		endif /* defined(API_MODULES_RUNTIME_CHECKED) */

		if(psTls->lastError==EGL_BAD_MATCH)
		{
			goto err_stop_timer;
		}
	}

	psSync->refcount++;

	EGLThreadUnlock(psTls);

	if ( psSync->eglSyncType == EGL_SYNC_FENCE_KHR )
	{
		ret = IMGeglClientWaitSyncKHR_Fence( psTls, psSync, flags, timeout );
	}
	else
	{
		ret = IMGeglClientWaitSyncKHR_Reusable( psTls, psSync, flags, timeout );
	}
	
	EGLThreadLock(psTls);

	psSync->refcount--;

	_cleanupSyncs(psDpy, psSysContext);

err_unlock:

	EGLThreadUnlock(psTls);

err_stop_timer:

	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglClientWaitSyncKHR);

err_out:

	return ret;
}

/***********************************************************************************
 Function Name      : _waitFence
 Inputs             : eglDpy, sync, timeout
 Outputs            : -
 Returns            : 
 Description        : Wait for Fence to complete
************************************************************************************/
static void _waitFence(KEGL_SYNC *psSync, SrvSysContext *psSysContext, EGLTimeKHR timeout)
{
	IMG_UINT32 ui32LastTime_us=0, ui32Timeout_us;
	
	/* note current time */
	if(timeout != EGL_FOREVER_KHR)
	{
		ui32LastTime_us = PVRSRVClockus();
	}

	/* timeout loop */
	ui32Timeout_us = (IMG_UINT32) (timeout / 1000);

	while(timeout == EGL_FOREVER_KHR || ui32Timeout_us > 0)
	{
		//This will result in inaccurate timeouts, but it is still better than what was here before
		if (!SGX2DQueryBlitsComplete(&psSysContext->s3D, psSync->psSyncMemInfo->psClientSyncInfo, IMG_TRUE))
		{
			psSync->eglSyncStatus = EGL_SIGNALED_KHR;
			break;
		}

		/* Check the object was not signalled externally (by destroy) */
		if(psSync->eglSyncStatus==EGL_SIGNALED_KHR)
		{
			break;
		}
		
		if(timeout != EGL_FOREVER_KHR)
		{
			IMG_UINT32 elapsed;
			elapsed = PVRSRVClockus() - ui32LastTime_us;
			ui32Timeout_us = (elapsed > ui32Timeout_us) ? 0 : ui32Timeout_us - elapsed;
			ui32LastTime_us = PVRSRVClockus();
		}
	}
}

/***********************************************************************************
 Function Name      : IMGeglClientWaitSyncKHR_Fence
 Inputs             : eglDpy, sync, flags, timeout
 Outputs            : -
 Returns            : status
 Description        : Flush buffers on current API if needed then check the signal
                      state. Then if the timeout is 0 just return the result otherwise
                      wait for up to timeout ns for a result to become available.
************************************************************************************/
static EGLint IMGeglClientWaitSyncKHR_Fence(TLS psTls, KEGL_SYNC *psSync, EGLint flags, EGLTimeKHR timeout)
{	
	EGLint ret = EGL_FALSE;

#if !defined(EGL_EXTENSION_KHR_FENCE_SYNC)

	PVR_UNREFERENCED_PARAMETER(psTls);
	PVR_UNREFERENCED_PARAMETER(psSync);
	PVR_UNREFERENCED_PARAMETER(flags);
	PVR_UNREFERENCED_PARAMETER(timeout);

#else /* !defined(EGL_EXTENSION_KHR_FENCE_SYNC) */

	KEGL_CONTEXT *psCurrentContext = EGL_NO_CONTEXT;

	PVR_ASSERT(psTls);
	PVR_ASSERT(psSync);
	
	if(psSync->psContext == EGL_NO_CONTEXT)
	{
		return EGL_FALSE;
	}

	/* Flush the buffers using an API specific call to prevent lock-ups */
	/* Spec states we should flush CURRENT context for current API */
	if( (psSync->eglSyncStatus == EGL_UNSIGNALED_KHR) && (flags & EGL_SYNC_FLUSH_COMMANDS_BIT_KHR))
	{
		/* retrieve the current context for the current api */
		psCurrentContext = psTls->apsCurrentContext[psTls->ui32API];

		/* Only flush if there is a context */
		if(psCurrentContext != EGL_NO_CONTEXT)
		{
			if(!_flushBuffers(psCurrentContext, psTls))
			{
				/* If flush fails and last error is set, bail out */
				/* Otherwise it is just a case of no context or context that can't be flushed */
				if(psTls->lastError != EGL_SUCCESS)
				{
					return EGL_FALSE;
				}
			}
		}
	}


	/* Check the sync object and set the signalling state */
	if(EGL_FENCE_SYNC_COMPLETE(&psTls->psGlobalData->sSysContext.s3D, psSync))
	{
		psSync->eglSyncStatus = EGL_SIGNALED_KHR;
		ret = EGL_CONDITION_SATISFIED_KHR;
		goto err_got_result;
	}

	/* If timeout is 0 then return the signaling state */
	if(timeout == 0)
	{
		if(psSync->eglSyncStatus == EGL_SIGNALED_KHR)
		{
			ret = EGL_CONDITION_SATISFIED_KHR;
		}
		else
		{
			ret = EGL_TIMEOUT_EXPIRED_KHR;
		}

		goto err_got_result;
	}

	_waitFence(psSync, &psTls->psGlobalData->sSysContext, timeout);

	if(psSync->eglSyncStatus == EGL_SIGNALED_KHR)
	{
		ret = EGL_CONDITION_SATISFIED_KHR;
	}
	else
	{
		ret = EGL_TIMEOUT_EXPIRED_KHR;
	}

err_got_result:
#endif /* !defined(EGL_EXTENSION_KHR_FENCE_SYNC) */

	return ret;
}

/***********************************************************************************
 Function Name      : IMGeglClientWaitSyncKHR_Reusable
 Inputs             : eglDpy, sync, flags, timeout
 Outputs            : -
 Returns            : status
 Description        : 
************************************************************************************/
static EGLint IMGeglClientWaitSyncKHR_Reusable(TLS psTls, KEGL_SYNC *psSync, EGLint flags, EGLTimeKHR timeout)
{	
	EGLint ret = EGL_FALSE;

#if !defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)

	PVR_UNREFERENCED_PARAMETER(psTls);
	PVR_UNREFERENCED_PARAMETER(psSync);
	PVR_UNREFERENCED_PARAMETER(flags);
	PVR_UNREFERENCED_PARAMETER(timeout);

#else /* !defined(EGL_EXTENSION_KHR_REUSABLE_SYNC) */

	IMG_UINT64 ui64Timeout_us;
	IMG_UINT64 ui64WaitLevel;
	PVRSRV_ERROR eWaitResult;
	EGLenum eglSyncStatus;
	KEGL_CONTEXT *psCurrentContext = EGL_NO_CONTEXT;
	
	PVR_ASSERT(psTls);
	PVR_ASSERT(psSync);

	EGLThreadLock(psTls);

	
	eglSyncStatus = psSync->eglSyncStatus;
	if ( eglSyncStatus == EGL_UNSIGNALED_KHR && timeout > 0 )
	{
		psSync->ui32WaitValue++;
		ui64WaitLevel = psSync->ui64WaitLevel;
	}

	EGLThreadUnlock(psTls);
	
	/* Flush the buffers using an API specific call to prevent lock-ups */
	if( (eglSyncStatus == EGL_UNSIGNALED_KHR) && (flags & EGL_SYNC_FLUSH_COMMANDS_BIT_KHR))
	{
		/* retrieve the current context for the current api */
		psCurrentContext = psTls->apsCurrentContext[psTls->ui32API];

		/* Only flush if there is a context */
		if(psCurrentContext != EGL_NO_CONTEXT)
		{
			if(!_flushBuffers(psCurrentContext, psTls))
			{
				/* If flush fails and last error is set, bail out */
				if(psTls->lastError != EGL_SUCCESS)
				{
					return EGL_FALSE;
				}
			}
		}
	}

	/* If timeout is 0 then return the signaling state */
	if(timeout == 0)
	{
		if(eglSyncStatus == EGL_SIGNALED_KHR)
		{
			ret = EGL_CONDITION_SATISFIED_KHR;
		}
		else
		{
			ret = EGL_TIMEOUT_EXPIRED_KHR;
		}

		goto err_got_result;
	}
	
	/* If the sync is already signalled then return. */
	if ( eglSyncStatus == EGL_SIGNALED_KHR )
	{
		ret = EGL_CONDITION_SATISFIED_KHR;
		goto err_got_result;
	}
	
	if ( timeout != EGL_FOREVER_KHR )
	{
		/* timeout loop */
		ui64Timeout_us = (timeout + 999) / 1000;

		while ( !ret )
		{
			/*
			 * Here is a slight possibility that if there was a signalling before we issue the wait on semaphore a wrong wait will be signalled.
			 * Consider the following execution sequence:
			 * Thread1:
			 * 	EGLClientWaitSyncKHR() -> (sync is unsignaled) -> switch to Thread2 -> (continue after switch back from Thread2) -> PVRSRVWaitSemaphore()
			 * Thread2:
			 * 	EGLSignalSyncKHR(signal); EGLSignalSyncKHR(unsignal); EGLClientWaitSyncKHR(); -> switch to Thread1
			 * With this sequence the Thread2's EGLClientWaitSyncKHR call will be satisfied first, which is wrong.
			 */
			eWaitResult = PVRSRVWaitSemaphore( psSync->hReusableSemaphore, ui64Timeout_us );
			if ( eWaitResult == PVRSRV_OK )
			{
				if ( ui64WaitLevel == psSync->ui64WaitLevel )
				{
					/* This is not the right level of signalling. The signal was "stolen" from a lower level wait, re-issue it. */
					PVRSRVPostSemaphore( psSync->hReusableSemaphore, 1 );
					PVRSRVReleaseThreadQuanta();
				}
				else
				{
					ret = EGL_CONDITION_SATISFIED_KHR;
				}
			}
			else
			if ( eWaitResult == PVRSRV_ERROR_TIMEOUT )
			{
				ret = EGL_TIMEOUT_EXPIRED_KHR;
			}
		}
	}
	else
	{
		PVRSRVWaitSemaphore( psSync->hReusableSemaphore, IMG_SEMAPHORE_WAIT_INFINITE );
		ret = EGL_CONDITION_SATISFIED_KHR;
	}

err_got_result:

#endif /* !defined(EGL_EXTENSION_KHR_REUSABLE_SYNC) */

	return ret;
}

/***********************************************************************************
 Function Name      : IMGeglGetSyncAttribKHR
 Inputs             : eglDpy, sync, attribute
 Outputs            : pValue
 Returns            : TRUE/FALSE
 Description        : 
************************************************************************************/
IMG_INTERNAL EGLBoolean	IMGeglGetSyncAttribKHR(EGLDisplay eglDpy, EGLSyncKHR sync, EGLint attribute, EGLint *pValue)
{
	KEGL_SYNC *psSync = (KEGL_SYNC *)sync;
	EGLBoolean ret = EGL_FALSE;
	KEGL_DISPLAY *psDpy;
	TLS	psTls;

	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglGetSyncAttribKHR"));

	psTls = TLS_Open(_TlsInit);

	if (!psTls)
	{
		goto err_out;
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglGetSyncAttribKHR);

	psTls->lastError = EGL_SUCCESS;

	/* TODO: Behaviour undefined by spec
			 (NULL display)
	 */
	psDpy = GetKEGLDisplay(psTls, eglDpy);

	if(!psDpy)
	{
		psTls->lastError = EGL_BAD_DISPLAY;

		goto err_stop_timer;
	}

	EGLThreadLock(psTls);

	if(!IsSync(psDpy, psSync))
	{
		psTls->lastError = EGL_BAD_PARAMETER;

		EGLThreadUnlock(psTls);

		goto err_stop_timer;
	}

	/* TODO: Behaviour undefined by spec
			 (valid current Dpy does not match CreateSync Dpy)
	 */
	if(psSync->psDpy != psDpy)
	{
		psTls->lastError = EGL_BAD_MATCH;

		goto err_down;
	}

	/* TODO: Behaviour undefined by spec
			 (invalid "value" parameter)
	 */
	if(!pValue)
	{
		psTls->lastError = EGL_BAD_PARAMETER;

		goto err_down;
	}

	switch(attribute)
	{
		case EGL_SYNC_TYPE_KHR:
		{
			*pValue = psSync->eglSyncType;

			break;
		}
		case EGL_SYNC_STATUS_KHR:
		{
			*pValue = psSync->eglSyncStatus;

			break;
		}
		case EGL_SYNC_CONDITION_KHR:
		{
			if(psSync->eglSyncType != EGL_SYNC_FENCE_KHR)
			{
				psTls->lastError = EGL_BAD_MATCH;

				goto err_down;
			}

			*pValue = psSync->eglSyncCondition;

			break;
		}
		default:
		{
			psTls->lastError = EGL_BAD_ATTRIBUTE;

			goto err_down;
		}
	}

	ret = EGL_TRUE;

err_down:

	EGLThreadUnlock(psTls);

err_stop_timer:

	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglGetSyncAttribKHR);

err_out:

	return ret;
}

#if defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)
/***********************************************************************************
 Function Name      : IMGeglSignalSyncKHR
 Inputs             : eglDpy, sync, mode
 Outputs            : -
 Returns            : TRUE/FALSE
 Description        : 
************************************************************************************/
IMG_INTERNAL EGLBoolean IMGeglSignalSyncKHR(EGLDisplay eglDpy, EGLSyncKHR sync, EGLenum mode)
{
	KEGL_SYNC *psSync = (KEGL_SYNC *)sync;
	EGLBoolean ret = EGL_FALSE;
	KEGL_DISPLAY *psDpy;
	TLS	psTls;

	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglSignalSyncKHR"));

	psTls = TLS_Open(_TlsInit);

	if (!psTls)
	{
		goto err_out;
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglSignalSyncKHR);

	psTls->lastError = EGL_SUCCESS;

	/* TODO: Behaviour undefined by spec
			 (NULL display)
	 */
	psDpy = GetKEGLDisplay(psTls, eglDpy);

	if(!psDpy)
	{
		psTls->lastError = EGL_BAD_DISPLAY;

		goto err_stop_timer;
	}

	EGLThreadLock(psTls);

	if(!IsSync(psDpy, psSync))
	{
		psTls->lastError = EGL_BAD_PARAMETER;

		goto err_unlock;
	}

	/* TODO: Behaviour undefined by spec
			 (valid current Dpy does not match CreateSync Dpy)
	 */
	if(psSync->psDpy != psDpy)
	{
		psTls->lastError = EGL_BAD_MATCH;

		goto err_unlock;
	}

	if(psSync->eglSyncType != EGL_SYNC_REUSABLE_KHR)
	{
		psTls->lastError = EGL_BAD_MATCH;

		goto err_unlock;
	}

	switch ( mode )
	{
		case EGL_SIGNALED_KHR:
		{
			if ( psSync->eglSyncStatus == EGL_UNSIGNALED_KHR )
			{
				psSync->eglSyncStatus = EGL_SIGNALED_KHR;
				if ( psSync->ui32WaitValue > 0 )
				{
					psSync->ui64WaitLevel++;
					PVRSRVPostSemaphore( psSync->hReusableSemaphore, psSync->ui32WaitValue );
					psSync->ui32WaitValue = 0;
				}
			}
			break;
		}
		case EGL_UNSIGNALED_KHR:
		{
			psSync->eglSyncStatus = EGL_UNSIGNALED_KHR;
			break;
		}
		default:
		{
			/* TODO: Behaviour undefined by spec
					 (invalid mode attribute)
			 */
			psTls->lastError = EGL_BAD_PARAMETER;
			
			goto err_unlock;
		}
	}

	ret = EGL_TRUE;

err_unlock:
	
	EGLThreadUnlock(psTls);
	
err_stop_timer:

	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglSignalSyncKHR);

err_out:

	return ret;
}
#endif /* defined(EGL_EXTENSION_KHR_REUSABLE_SYNC) */

#endif /* defined(EGL_EXTENSION_KHR_FENCE_SYNC) || defined(EGL_EXTENSION_KHR_REUSABLE_SYNC) */

/*****************************************************************************
 End of file (egl_sync.c)
******************************************************************************/
