/******************************************************************************
 * Name         : fbo.c
 *
 * Copyright    : 2005-2008 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: fbo.c $
 * 
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "context.h"

#include "psp2/swtexop.h"

#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
#include "twiddle.h"
#endif

#define GLES2_FRAMEBUFFER_STATUS_UNKNOWN		0xDEAD


/***********************************************************************************
 Function Name      : GetColorAttachmentMemFormat
 Inputs             : psFrameBuffer
 Outputs            : -
 Returns            : Returns the memory format of the color attachment of the given framebuffer object
 Description        : -
************************************************************************************/
IMG_INTERNAL IMG_MEMLAYOUT GetColorAttachmentMemFormat(GLES2Context *gc, GLES2FrameBuffer *psFrameBuffer)
{

	if(psFrameBuffer != &gc->sFrameBuffer.sDefaultFrameBuffer)
	{
		GLES2FrameBufferAttachable *psColorAttachment;

		psColorAttachment = psFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];

		if(psColorAttachment)
		{
			if(GL_TEXTURE == psColorAttachment->eAttachmentType)
			{
				GLES2Texture *psTex = ((GLES2MipMapLevel *)psFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT])->psTex;

				GLES_ASSERT(psTex);

#if defined(GLES2_EXTENSION_EGL_IMAGE)
				if(psTex->psEGLImageTarget)
				{
					if(psTex->psEGLImageTarget->bTwiddled)
					{
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
						return IMG_MEMLAYOUT_HYBRIDTWIDDLED;
#else
						return IMG_MEMLAYOUT_TWIDDLED;
#endif
					}
					else
					{
						return IMG_MEMLAYOUT_STRIDED;
					}
				}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

				switch(psTex->sState.aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK)
				{
					case EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE:
						return IMG_MEMLAYOUT_STRIDED;
					case EURASIA_PDS_DOUTT1_TEXTYPE_TILED:
						return IMG_MEMLAYOUT_TILED;
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
					case EURASIA_PDS_DOUTT1_TEXTYPE_2D:
#else
					case EURASIA_PDS_DOUTT1_TEXTYPE_ARB_2D:
#endif
					default:
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
						return IMG_MEMLAYOUT_HYBRIDTWIDDLED;
#else
						return IMG_MEMLAYOUT_TWIDDLED;
#endif
				}
			}
			else
			{
#if defined(GLES2_EXTENSION_EGL_IMAGE)
				GLES2RenderBuffer *psRenderBuffer = (GLES2RenderBuffer *)psFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

				/* The color attachment of the current framebuffer is a renderbuffer */
				GLES_ASSERT(GL_RENDERBUFFER == psColorAttachment->eAttachmentType);

#if defined(GLES2_EXTENSION_EGL_IMAGE)
				if(psRenderBuffer->psEGLImageTarget && psRenderBuffer->psEGLImageTarget->bTwiddled)
				{
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
					return IMG_MEMLAYOUT_HYBRIDTWIDDLED;
#else
					return IMG_MEMLAYOUT_TWIDDLED;
#endif
				}
				else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
				{
					return IMG_MEMLAYOUT_STRIDED;
				}
			}
		}
	}

	/* Default framebuffer is stride */
	return IMG_MEMLAYOUT_STRIDED;
}


/***********************************************************************************
 Function Name      : FlushRenderSurface
 Inputs             : gc, psRenderSurface, ui32KickFlags
 Outputs            : -
 Returns            : IMG_TRUE if the TA/3D was successfully kicked. IMG_FALSE otherwise.
 Description        : UTILITY: Forces a hardware flush if the attachable has been recently
                      used as an output.
************************************************************************************/
IMG_INTERNAL IMG_BOOL FlushRenderSurface(GLES2Context *gc, 
										 EGLRenderSurface *psRenderSurface, 
										 IMG_UINT32 ui32KickFlags)
{
	IMG_BOOL bResult = IMG_TRUE;

	if(psRenderSurface != gc->psRenderSurface)
	{
		PVRSRVLockMutex(psRenderSurface->hMutex);
	}

    /* We preempt the check in scheduleTA for the surface so that we don't attempt to kick the current scene
		unnecessarily.
	*/
	if(psRenderSurface->bInFrame)
	{
		if(psRenderSurface->bPrimitivesSinceLastTA           || 
			((ui32KickFlags & GLES2_SCHEDULE_HW_LAST_IN_SCENE) != 0) ||
			((ui32KickFlags & GLES2_SCHEDULE_HW_DISCARD_SCENE) != 0))
		{
			/* If the current surface is in frame we need to kick its TA before rendering the
			surface. If the current surface is the same as the argument,
				the TA is going to be kicked anyway so we don't do it here.
			*/
			if(gc->psRenderSurface && gc->psRenderSurface->bInFrame && gc->psRenderSurface != psRenderSurface)
			{
				if(gc->psRenderSurface->bPrimitivesSinceLastTA)
				{
					GLES2_INC_COUNT(GLES2_TIMER_SGXKICKTA_FLUSHFRAMEBUFFER_COUNT, 1);
				}

				if(ScheduleTA(gc, gc->psRenderSurface, 0) != IMG_EGL_NO_ERROR)
				{
					PVR_DPF((PVR_DBG_ERROR,"FlushRenderSurface: ScheduleTA did not work properly on the current surface"));
					bResult = IMG_FALSE;
				}
			}
			GLES2_INC_COUNT(GLES2_TIMER_SGXKICKTA_FLUSHFRAMEBUFFER_COUNT, 1);
		}
	}

	/* We assume that the framebuffer this is attached to is complete */
	if(ScheduleTA(gc, psRenderSurface, ui32KickFlags) != IMG_EGL_NO_ERROR)
	{
		PVR_DPF((PVR_DBG_ERROR,"FlushRenderSurface: ScheduleTA did not work properly on the attachment"));
		bResult = IMG_FALSE;
	}

	if(psRenderSurface != gc->psRenderSurface)
	{
		PVRSRVUnlockMutex(psRenderSurface->hMutex);
	}

	return bResult;
}

IMG_INTERNAL IMG_BOOL FlushAllUnflushedFBO(GLES2Context *gc, 
										 IMG_BOOL bWaitForHW)
{
	IMG_BOOL bResult = IMG_TRUE;
	EGLRenderSurface *psRenderSurface;
	GLES2Context *psFlushGC;
	GLES2SurfaceFlushList **ppsFlushList, *psFlushItem; 
	IMG_UINT32 ui32Flags = GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_IGNORE_FLUSHLIST;

	if(bWaitForHW)
	{
		ui32Flags |= GLES2_SCHEDULE_HW_WAIT_FOR_3D;
	}
	
	PVRSRVLockMutex(gc->psSharedState->hFlushListLock);

	ppsFlushList = &gc->psSharedState->psFlushList;

	while(*ppsFlushList)
	{	
		psFlushItem = *ppsFlushList;
		psRenderSurface = psFlushItem->psRenderSurface;
		psFlushGC = psFlushItem->gc;
				
		GLES_ASSERT(psRenderSurface);

		/* Only flush surface which were drawn to from this context */
		if(gc == psFlushGC)
		{
			if(!FlushRenderSurface(gc, psRenderSurface, ui32Flags))
			{
				bResult = IMG_FALSE;
				ppsFlushList = &psFlushItem->psNext;	
			}
			else
			{
				*ppsFlushList = psFlushItem->psNext;
				
				GLES2Free(IMG_NULL, psFlushItem);
			}
		}
		else
		{
			ppsFlushList = &psFlushItem->psNext;	
		}
	}
	
	PVRSRVUnlockMutex(gc->psSharedState->hFlushListLock);

	return bResult;
}

/***********************************************************************************
 Function Name      : FlushAttachableIfNeeded
 Inputs             : gc, psAttachment, ui32KickFlags
 Outputs            : -
 Returns            : IMG_TRUE if the TA was successfully kicked. IMG_FALSE otherwise.
 Description        : UTILITY: Forces a hardware flush if the attachable has been recently
                      used as an output.
************************************************************************************/
IMG_INTERNAL IMG_BOOL FlushAttachableIfNeeded(GLES2Context *gc, 
												 GLES2FrameBufferAttachable *psAttachment, 
												 IMG_UINT32 ui32KickFlags)
{
	EGLRenderSurface *psRenderSurface = psAttachment->psRenderSurface;
	IMG_BOOL bResult = IMG_TRUE;

	if(psRenderSurface)
	{
		if (psAttachment->bGhosted)
		{
			//This attachable has been ghosted, so the meminfo may be invalidd
			return IMG_FALSE;
		}
#if defined(PDUMP)
		if(GL_TEXTURE == psAttachment->eAttachmentType)
		{
			PDumpTexture(gc, ((GLES2MipMapLevel*)psAttachment)->psTex);
		}
#endif
		bResult = FlushRenderSurface(gc, psRenderSurface, ui32KickFlags);
	}
	return bResult;
}


/***********************************************************************************
 Function Name      : FreeFrameBuffer
 Inputs             : gc, psFrameBuffer, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Frees all structures/data associated with a frame buffer
************************************************************************************/
static IMG_VOID FreeFrameBuffer(GLES2Context *gc, GLES2FrameBuffer *psFrameBuffer)
{
	GLES2FrameBufferAttachable *psAttachment;
	IMG_UINT32 i;

	for(i=0; i < GLES2_MAX_ATTACHMENTS; i++) 
	{
		psAttachment = psFrameBuffer->apsAttachment[i];

		/* Is a renderbuffer currently attached? */
		if (psAttachment && psAttachment->eAttachmentType == GL_RENDERBUFFER) 
		{
			/* Detach currently attached renderbuffer */
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER],
				(GLES2NamedItem*)psAttachment);
		}
		else if (psAttachment && psAttachment->eAttachmentType == GL_TEXTURE) 
		{
			/* Detach currently attached texture */
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ],
				(GLES2NamedItem*)(((GLES2MipMapLevel*)psAttachment)->psTex));
		}
	}

	GLES2Free(IMG_NULL, psFrameBuffer);
}


/***********************************************************************************
 Function Name      : FrameBufferHasBeenModified
 Inputs             : gc, psFrameBuffer
 Outputs            : psFrameBuffer
 Returns            : -
 Description        : Set the framebuffer status as unknown.
************************************************************************************/
static IMG_VOID FrameBufferHasBeenModified(GLES2FrameBuffer *psFrameBuffer)
{
	if (psFrameBuffer && (psFrameBuffer->sNamedItem.ui32Name != 0))
	{
		psFrameBuffer->eStatus = GLES2_FRAMEBUFFER_STATUS_UNKNOWN;
	}
}


/***********************************************************************************
 Function Name      : DestroyFBOAttachableRenderSurface
 Inputs             : gc, psAttachment
 Outputs            : -
 Returns            : -
 Description        : If the attachment had a render surface associated, it is destroyed,
                      the memory is freed and the pointer to the surface is reset to null.
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyFBOAttachableRenderSurface(GLES2Context *gc, GLES2FrameBufferAttachable *psAttachment)
{
	if(psAttachment->psRenderSurface)
	{
		FlushAttachableIfNeeded(gc, psAttachment, GLES2_SCHEDULE_HW_LAST_IN_SCENE|GLES2_SCHEDULE_HW_WAIT_FOR_3D);

		psAttachment->psRenderSurface->ui32FBOAttachmentCount--;

		/* Only destroy render surface if this is the last reference */
		if(psAttachment->psRenderSurface->ui32FBOAttachmentCount)
		{
			/* Restore the sync info */
			if(psAttachment->psRenderSurface->psRenderSurfaceSyncInfo!=psAttachment->psRenderSurface->psSyncInfo)
			{
				psAttachment->psRenderSurface->psSyncInfo = psAttachment->psRenderSurface->psRenderSurfaceSyncInfo;
			}
		}
		else
		{
			if(!KEGLDestroyRenderSurface(gc->psSysContext, psAttachment->psRenderSurface))
			{
				PVR_DPF((PVR_DBG_ERROR,"FBOAttachableHasBeenModified: Couldn't destroy render surface"));
			}

			/* Remove references to this surface in all resource managers */
			KRM_RemoveAttachmentPointReferences(&gc->psSharedState->psTextureManager->sKRM, psAttachment->psRenderSurface);
			KRM_RemoveAttachmentPointReferences(&gc->psSharedState->sUSEShaderVariantKRM, psAttachment->psRenderSurface);

			if(gc->psRenderSurface==psAttachment->psRenderSurface)
			{
				gc->psRenderSurface = IMG_NULL;
			}

			GLES2Free(IMG_NULL, psAttachment->psRenderSurface);
		}

		psAttachment->psRenderSurface = IMG_NULL;

		if(GL_TEXTURE == psAttachment->eAttachmentType)
		{
			((GLES2MipMapLevel*)psAttachment)->psTex->ui32NumRenderTargets--;
		}
	}
}


/***********************************************************************************
 Function Name      : FreeRenderBuffer
 Inputs             : gc, psRenderBuffer
 Outputs            : -
 Returns            : -
 Description        : Frees all structures/data associated with a render buffer
************************************************************************************/
static IMG_VOID FreeRenderBuffer(GLES2Context *gc, GLES2RenderBuffer *psRenderBuffer)
{
	DestroyFBOAttachableRenderSurface(gc, &psRenderBuffer->sFBAttachable);

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psRenderBuffer->psEGLImageSource)
	{
		/* Inform EGL that the source is being deleted */
		KEGLUnbindImage(psRenderBuffer->psEGLImageSource->hImage);

		/* Source has been ghosted */
		psRenderBuffer->psMemInfo = IMG_NULL;
	}
	else if(psRenderBuffer->psEGLImageTarget)
	{
		KEGLUnbindImage(psRenderBuffer->psEGLImageTarget->hImage);
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

	if(psRenderBuffer->psMemInfo)
	{
		GLES2FREEDEVICEMEM_HEAP(gc, psRenderBuffer->psMemInfo);
	}

	GLES2Free(IMG_NULL, psRenderBuffer);
}


/***********************************************************************************
 Function Name      : DisposeFrameBufferObject
 Inputs             : gc, pvData, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Generic frame buffer object free function; called from names.c
************************************************************************************/
static IMG_VOID DisposeFrameBufferObject(GLES2Context *gc, GLES2NamedItem* psNamedItem, IMG_BOOL bIsShutdown)
{
	GLES2FrameBuffer *psFrameBuffer = (GLES2FrameBuffer *)psNamedItem;

	/* Avoid build warning in release mode */
	PVR_UNREFERENCED_PARAMETER(bIsShutdown);

	GLES_ASSERT(bIsShutdown || (psFrameBuffer->sNamedItem.ui32RefCount == 0));

	FreeFrameBuffer(gc, psFrameBuffer);
}


/***********************************************************************************
 Function Name      : DisposeRenderBuffer
 Inputs             : gc, pvData, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Generic render buffer object free function; called from names.c
************************************************************************************/
static IMG_VOID DisposeRenderBuffer(GLES2Context *gc, GLES2NamedItem* psNamedItem, IMG_BOOL bIsShutdown)
{
	GLES2RenderBuffer *psRenderBuffer = (GLES2RenderBuffer *)psNamedItem;

	/* Avoid build warning in release mode */
	PVR_UNREFERENCED_PARAMETER(bIsShutdown);

	GLES_ASSERT(bIsShutdown || (psRenderBuffer->sFBAttachable.sNamedItem.ui32RefCount == 0));

	FreeRenderBuffer(gc, psRenderBuffer);
}


/***********************************************************************************
 Function Name      : SetupRenderBufferNameArray
 Inputs             : psNamesArray
 Outputs            : -
 Returns            : -
 Description        : Sets up names array for render buffer objects.
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupRenderBufferNameArray(GLES2NamesArray *psNamesArray)
{
	psNamesArray->pfnFree = DisposeRenderBuffer;
}


/***********************************************************************************
 Function Name      : SetupFrameBufferObjectNameArray
 Inputs             : psNamesArray
 Outputs            : -
 Returns            : -
 Description        : Sets up names array for frame buffer objects.
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupFrameBufferObjectNameArray(GLES2NamesArray *psNamesArray)
{
	psNamesArray->pfnFree = DisposeFrameBufferObject;
}


/***********************************************************************************
 Function Name      : CreateFrameBufferObject
 Inputs             : gc, ui32Name
 Outputs            : -
 Returns            : Pointer to new framebuffer object structure
 Description        : Creates and initialises a new framebuffer object structure.
************************************************************************************/
static GLES2FrameBuffer* CreateFrameBufferObject(GLES2Context *gc, IMG_UINT32 ui32Name)
{
	GLES2FrameBuffer *psFrameBuffer;

	PVR_UNREFERENCED_PARAMETER(gc);

	psFrameBuffer = (GLES2FrameBuffer *) GLES2Calloc(gc, sizeof(GLES2FrameBuffer));

	if (psFrameBuffer == IMG_NULL)
	{
		return IMG_NULL;
	}

	psFrameBuffer->sNamedItem.ui32Name = ui32Name;

	psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;

	psFrameBuffer->sMode.ui32MaxViewportX = GLES2_MAX_TEXTURE_SIZE;
	psFrameBuffer->sMode.ui32MaxViewportY = GLES2_MAX_TEXTURE_SIZE;

	return psFrameBuffer;
}


/***********************************************************************************
 Function Name      : CreateRenderBufferObject
 Inputs             : gc, ui32Name
 Outputs            : -
 Returns            : Pointer to new renderbuffer object structure
 Description        : Creates and initialises a new renderbuffer object structure.
************************************************************************************/
static GLES2RenderBuffer* CreateRenderBufferObject(GLES2Context *gc, IMG_UINT32 ui32Name)
{
	GLES2RenderBuffer *psRenderBuffer;

	PVR_UNREFERENCED_PARAMETER(gc);
	psRenderBuffer = (GLES2RenderBuffer *) GLES2Calloc(gc, sizeof(GLES2RenderBuffer));

	if (psRenderBuffer == IMG_NULL)
	{
		return IMG_NULL;
	}

	((GLES2NamedItem*)psRenderBuffer)->ui32Name  = ui32Name;
	((GLES2FrameBufferAttachable*)psRenderBuffer)->psRenderSurface = IMG_NULL;
	((GLES2FrameBufferAttachable*)psRenderBuffer)->eAttachmentType = GL_RENDERBUFFER;
	
	return psRenderBuffer;
}


/***********************************************************************************
 Function Name      : CreateFrameBufferState
 Inputs             : gc, psMode
 Outputs            : -
 Returns            : Success
 Description        : Initialises the framebuffer object state and creates/shares the name 
					  array. 
************************************************************************************/
IMG_INTERNAL IMG_BOOL CreateFrameBufferState(GLES2Context *gc,
											 EGLcontextMode *psMode)
{
	gc->sFrameBuffer.sDefaultFrameBuffer.eStatus =             GL_FRAMEBUFFER_COMPLETE;
	gc->sFrameBuffer.sDefaultFrameBuffer.sNamedItem.ui32Name = 0;
	gc->sFrameBuffer.sDefaultFrameBuffer.sMode =               *psMode;


	gc->psMode = &gc->sFrameBuffer.sDefaultFrameBuffer.sMode;
	gc->psDrawParams = &gc->sFrameBuffer.sDefaultFrameBuffer.sDrawParams;
	gc->psReadParams = &gc->sFrameBuffer.sDefaultFrameBuffer.sReadParams;

	gc->sFrameBuffer.psActiveFrameBuffer = &gc->sFrameBuffer.sDefaultFrameBuffer;

	gc->sFrameBuffer.psActiveRenderBuffer = IMG_NULL;

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : FreeFrameBufferState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Unbinds all frame/render buffer objects.
					  If no other contexts are sharing the names arrays, will delete 
					  these also.
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeFrameBufferState(GLES2Context *gc)
{
	/* unbind any frame/render buffer objects */
	if(gc->sFrameBuffer.psActiveRenderBuffer)
	{
		NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER],
			(GLES2NamedItem*)gc->sFrameBuffer.psActiveRenderBuffer);
		gc->sFrameBuffer.psActiveRenderBuffer = IMG_NULL;
	}

	if(gc->sFrameBuffer.psActiveFrameBuffer &&
	  (gc->sFrameBuffer.psActiveFrameBuffer != &gc->sFrameBuffer.sDefaultFrameBuffer))
	{
		GLES2FrameBuffer *psFrameBuffer = &gc->sFrameBuffer.sDefaultFrameBuffer;

		NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_FRAMEBUFFER],
						(GLES2NamedItem*)gc->sFrameBuffer.psActiveFrameBuffer);
		
		gc->sFrameBuffer.psActiveFrameBuffer = psFrameBuffer;
	}
}


/***********************************************************************************
 Function Name      : RemoveFrameBufferAttachment
 Inputs             : gc, bIsRenderBuffer, ui32Name
 Outputs            : -
 Returns            : -
 Description        : Detaches a texture/render buffer from all attachment points in
					  the active framebuffer
************************************************************************************/
IMG_INTERNAL IMG_VOID RemoveFrameBufferAttachment(GLES2Context *gc, IMG_BOOL bIsRenderBuffer, IMG_UINT32 ui32Name)
{
	GLES2FrameBuffer           *psFrameBuffer;
	GLES2FrameBufferAttachable *psAttachment /* = 0 */;
	IMG_BOOL                   bIsFrameBufferComplete, bAttachmentRemoved;
	IMG_UINT32                 i;
	
	psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	/* Nothing to do if we have no currently bound framebuffer */
	if(!psFrameBuffer)
	{
		return;
	}

	bIsFrameBufferComplete = (GL_FRAMEBUFFER_COMPLETE == psFrameBuffer->eStatus)? IMG_TRUE : IMG_FALSE;

	bAttachmentRemoved = IMG_FALSE;

	for(i=0; i < GLES2_MAX_ATTACHMENTS; i++) 
	{
		psAttachment = psFrameBuffer->apsAttachment[i];

		if(!psAttachment)
		{
			/* Nothing was attached to this attachment point */
			continue;
		}

		if(bIsRenderBuffer)
		{
			/* Is the renderbuffer currently attached? */
			if ((psAttachment->eAttachmentType == GL_RENDERBUFFER) && 
				(((GLES2NamedItem*)psAttachment)->ui32Name == ui32Name))
			{
				/* Kick the TA if the active framebuffer was complete and the surface was pending a kick. */
				if(bIsFrameBufferComplete)
				{
					FlushAttachableIfNeeded(gc, psAttachment, 0);
				}

				/* Detach currently attached renderbuffer */
				NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER], (GLES2NamedItem*)psAttachment);

				psFrameBuffer->apsAttachment[i] = IMG_NULL;

				bAttachmentRemoved = IMG_TRUE;

				/* A framebuffer-attachable cannot be attached twice to the same framebuffer */
				break;
			}
		}
		else
		{
			/* Is the texture currently attached? */
			if ((psAttachment->eAttachmentType == GL_TEXTURE))
			{
				GLES2NamedItem *psTex = (GLES2NamedItem*) ((GLES2MipMapLevel*)psAttachment)->psTex;

				if(psTex->ui32Name == ui32Name)
				{
					/* Kick the TA if the active framebuffer was complete and the surface was pending a kick. */
					if(bIsFrameBufferComplete)
					{
						FlushAttachableIfNeeded(gc, psAttachment, 0);
					}

					/* Detach currently attached texture */
					NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ], psTex);

					psFrameBuffer->apsAttachment[i] = IMG_NULL;

					bAttachmentRemoved = IMG_TRUE;

					/* A framebuffer-attachable cannot be attached twice to the same framebuffer */
					break;
				}
			}
		}
	}

	if(bAttachmentRemoved)
	{
		/* Update the framebuffer state */
		FrameBufferHasBeenModified(psFrameBuffer);
	}
}


/***********************************************************************************
 Function Name      : ChangeDrawableParams
 Inputs             : gc, psFrameBuffer, psReadParams, psDrawParams
 Outputs            : psFrameBuffer->sReadParams, psFrameBuffer->sDrawParams
 Returns            : -
 Description        : Resets the draw/read params if a surface/framebuffer has changed
************************************************************************************/
IMG_INTERNAL IMG_VOID ChangeDrawableParams(GLES2Context *gc,
										   GLES2FrameBuffer *psFrameBuffer,
										   EGLDrawableParams *psReadParams,
										   EGLDrawableParams *psDrawParams)
{
	IMG_BOOL bPreviousYFlip = (gc->psDrawParams->eRotationAngle == PVRSRV_FLIP_Y) ? IMG_TRUE : IMG_FALSE;
	IMG_BOOL bYFlip;
	IMG_UINT32 ui32MaxFBOStencilVal;

	if (GLES2_FRAMEBUFFER_STATUS_UNKNOWN == psFrameBuffer->eStatus)
	{
		gc->psRenderSurface = IMG_NULL;

		return;
	}

	ui32MaxFBOStencilVal = (1 << gc->sFrameBuffer.psActiveFrameBuffer->sMode.ui32StencilBits) - 1;
		
	if(gc->sState.sStencil.ui32MaxFBOStencilVal != ui32MaxFBOStencilVal)
	{
		gc->sState.sStencil.ui32MaxFBOStencilVal = ui32MaxFBOStencilVal;

		/* Update stencil masks */
		gc->sState.sStencil.ui32FFStencil =  (gc->sState.sStencil.ui32FFStencil & (EURASIA_ISPC_SWMASK_CLRMSK & EURASIA_ISPC_SCMPMASK_CLRMSK));
		gc->sState.sStencil.ui32FFStencil |= (gc->sState.sStencil.ui32FFStencilWriteMaskIn & ui32MaxFBOStencilVal)<<EURASIA_ISPC_SWMASK_SHIFT;
		gc->sState.sStencil.ui32FFStencil |= (gc->sState.sStencil.ui32FFStencilCompareMaskIn & ui32MaxFBOStencilVal)<<EURASIA_ISPC_SCMPMASK_SHIFT;

		gc->sState.sStencil.ui32BFStencil =  (gc->sState.sStencil.ui32BFStencil & (EURASIA_ISPC_SWMASK_CLRMSK & EURASIA_ISPC_SCMPMASK_CLRMSK));
		gc->sState.sStencil.ui32BFStencil |= (gc->sState.sStencil.ui32BFStencilWriteMaskIn & ui32MaxFBOStencilVal)<<EURASIA_ISPC_SWMASK_SHIFT;
		gc->sState.sStencil.ui32BFStencil |= (gc->sState.sStencil.ui32BFStencilCompareMaskIn & ui32MaxFBOStencilVal)<<EURASIA_ISPC_SCMPMASK_SHIFT;

		/* Updates stencil reference values */
		gc->sState.sStencil.ui32FFStencilRef = (IMG_UINT32)Clampi(gc->sState.sStencil.i32FFStencilRefIn, 0, (IMG_INT32)ui32MaxFBOStencilVal) << EURASIA_ISPA_SREF_SHIFT;
		gc->sState.sStencil.ui32BFStencilRef = (IMG_UINT32)Clampi(gc->sState.sStencil.i32BFStencilRefIn, 0, (IMG_INT32)ui32MaxFBOStencilVal) << EURASIA_ISPA_SREF_SHIFT;

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
	}

	/* Save off the read/write params */
	psFrameBuffer->sReadParams = *psReadParams;
	psFrameBuffer->sDrawParams = *psDrawParams;

	gc->psDrawParams = &gc->sFrameBuffer.psActiveFrameBuffer->sDrawParams;
	gc->psReadParams = &gc->sFrameBuffer.psActiveFrameBuffer->sReadParams;
	gc->psRenderSurface = gc->psDrawParams->psRenderSurface;
	gc->psMode = &gc->sFrameBuffer.psActiveFrameBuffer->sMode;

	if(gc->sFrameBuffer.psActiveFrameBuffer == &gc->sFrameBuffer.sDefaultFrameBuffer)
	{
		gc->hEGLSurface = gc->psRenderSurface->hEGLSurface;
	}

	if(gc->psRenderSurface)
	{
		/* Setup fragment buffer to current render surface */
		gc->apsBuffers[CBUF_TYPE_PDS_FRAG_BUFFER] = &gc->psRenderSurface->sPDSBuffer;
		gc->apsBuffers[CBUF_TYPE_USSE_FRAG_BUFFER] = &gc->psRenderSurface->sUSSEBuffer;
	}

	bYFlip = (gc->psDrawParams->eRotationAngle == PVRSRV_FLIP_Y) ? IMG_TRUE : IMG_FALSE;

	if(bYFlip != bPreviousYFlip)
	{
		/* Ensure render state is validated in case y-invert has changed */
		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
	}


	/* Change of drawable params can cause MTE / ISP state to need re-emission */
	gc->ui32EmitMask |= (GLES2_EMITSTATE_MTE_STATE_ISP | GLES2_EMITSTATE_MTE_STATE_REGION_CLIP);

	/* 
	 * Ensure that a change of render target size will apply the correct viewport, scissor and
	 * drawmask objects.
	 */
	ApplyViewport(gc);

	if ((gc->sState.sViewport.i32X == 0) && (gc->sState.sViewport.i32Y == 0) &&
		(gc->sState.sViewport.ui32Width == gc->psDrawParams->ui32Width) &&
		(gc->sState.sViewport.ui32Height == gc->psDrawParams->ui32Height))
	{
		gc->bFullScreenViewport = IMG_TRUE;
	}
	else
	{
		gc->bFullScreenViewport = IMG_FALSE;
	}

	if((gc->sState.sScissor.i32ScissorX == 0) && (gc->sState.sScissor.i32ScissorY == 0) &&
		(gc->sState.sScissor.ui32ScissorWidth == gc->psDrawParams->ui32Width) &&
		(gc->sState.sScissor.ui32ScissorHeight == gc->psDrawParams->ui32Height))
	{
		gc->bFullScreenScissor = IMG_TRUE;
	}
	else
	{
		gc->bFullScreenScissor = IMG_FALSE;
	}

	gc->bDrawMaskInvalid = IMG_TRUE;
}

/***********************************************************************************
 Function Name      : SetupFrameBufferColorAttachment
 Inputs             : gc, psFrameBuffer, ui32BufferBits
 Outputs            : ppsFBORenderSurface
 Returns            : -
 Description        : Sets up the color buffer meminfo for a framebuffer
************************************************************************************/
static IMG_BOOL SetupFrameBufferColorAttachment(GLES2Context *gc, GLES2FrameBuffer *psFrameBuffer, IMG_UINT32 ui32MipmapOffset)
{
	#if defined(GLES2_EXTENSION_EGL_IMAGE)
	EGLImage *psEGLImageTarget = IMG_NULL;
	#endif
	GLES2FrameBufferAttachable *psAttachment = psFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];
	EGLRenderSurface *psFBORenderSurface;

	if(psFrameBuffer->sMode.ui32ColorBits)
	{
		PVRSRV_CLIENT_SYNC_INFO	*psOverrideSyncInfo = IMG_NULL;

		psAttachment = psFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];

		GLES_ASSERT(psAttachment);

		if(psAttachment->eAttachmentType == GL_RENDERBUFFER)
		{
			GLES2RenderBuffer *psRenderBuffer = (GLES2RenderBuffer*)psAttachment;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
			if(psRenderBuffer->psEGLImageTarget)
			{
				psEGLImageTarget = psRenderBuffer->psEGLImageTarget;

				/* Use EGLImage sync info */
				psOverrideSyncInfo = psEGLImageTarget->psMemInfo->psClientSyncInfo;
			}
			else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
			{
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psRenderBuffer->psMemInfo->pvLinAddr;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psRenderBuffer->psMemInfo->sDevVAddr.uiAddr;
			}
		}
		else
		{
			GLES2Texture *psTex = ((GLES2MipMapLevel*)psAttachment)->psTex;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
			if(psTex->psEGLImageTarget)
			{
				psEGLImageTarget = psTex->psEGLImageTarget;

				/* Use EGLImage sync info */
				psOverrideSyncInfo = psEGLImageTarget->psMemInfo->psClientSyncInfo;
			}
			else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
			{
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psTex->psMemInfo->pvLinAddr;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psTex->psMemInfo->sDevVAddr.uiAddr;
			
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = (IMG_UINT8*)psFrameBuffer->sDrawParams.pvLinSurfaceAddress + ui32MipmapOffset;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress += ui32MipmapOffset;

				/* Use texture sync info */
				psOverrideSyncInfo = psTex->psMemInfo->psClientSyncInfo;
			}
		}

#if defined(GLES2_EXTENSION_EGL_IMAGE)
		if(psEGLImageTarget)
		{
			psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psEGLImageTarget->pvLinSurfaceAddress;
			psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psEGLImageTarget->ui32HWSurfaceAddress;
		}
#endif
		psFrameBuffer->sDrawParams.psRenderSurface      = psAttachment->psRenderSurface;
		psFrameBuffer->sDrawParams.ui32AccumHWAddress	= psFrameBuffer->sDrawParams.ui32HWSurfaceAddress;

		/* If there is a render surface at this point, reuse it */
		if(!psAttachment->psRenderSurface)
		{
			/* Else, allocate a new render surface */
			psAttachment->psRenderSurface = GLES2Calloc(gc, sizeof(EGLRenderSurface));
			
			if(!psAttachment->psRenderSurface)
			{
				return IMG_FALSE;
			}

			psFrameBuffer->sDrawParams.psRenderSurface = psAttachment->psRenderSurface;

			if(!KEGLCreateRenderSurface(gc->psSysContext, 
										&psFrameBuffer->sDrawParams, 
#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
										psFrameBuffer->sMode.ui32AntiAliasMode ? IMG_TRUE : IMG_FALSE, 
#else
										IMG_FALSE,
#endif
										IMG_FALSE,
										IMG_FALSE,
										psFrameBuffer->sDrawParams.psRenderSurface))
			{
				GLES2Free(IMG_NULL, psAttachment->psRenderSurface);
				psAttachment->psRenderSurface = 0;
				return IMG_FALSE;
			}

			psAttachment->psRenderSurface->ui32FBOAttachmentCount = 1;

			if(GL_TEXTURE == psAttachment->eAttachmentType)
			{
				((GLES2MipMapLevel*)psAttachment)->psTex->ui32NumRenderTargets++;
			}
		}

		psFBORenderSurface = psAttachment->psRenderSurface;

		psAttachment->psRenderSurface->eAlphaFormat = PVRSRV_ALPHA_FORMAT_NONPRE;
		psAttachment->psRenderSurface->eColourSpace = PVRSRV_COLOURSPACE_FORMAT_NONLINEAR;

		/*
		** Use the sync info of the texture attachement or EGL Image
		** so that we can wait for HW texture uploads correctly
		*/
		if(psOverrideSyncInfo)
		{
			psFBORenderSurface->psSyncInfo		  = psOverrideSyncInfo;
			psFrameBuffer->sDrawParams.psSyncInfo = psOverrideSyncInfo;
		}
		else
		{
			/* Use the sync info of the render surface */
			psFBORenderSurface->psSyncInfo = psAttachment->psRenderSurface->psRenderSurfaceSyncInfo;

			/* Set draw params syncinfo to be rendersurface sync (this is not necessarily true for EGL surfaces) */
			psFrameBuffer->sDrawParams.psSyncInfo = psAttachment->psRenderSurface->psRenderSurfaceSyncInfo;
		}
	}
	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : SetupFrameBufferZLS
 Inputs             : gc, psFrameBuffer
 Outputs            : 
 Returns            : -
 Description        : Sets up ZLS control words for a framebuffer
************************************************************************************/
static IMG_VOID SetupFrameBufferZLS(GLES2Context *gc, GLES2FrameBuffer *psFrameBuffer)
{
	/* ZLS setup */
	if(psFrameBuffer->sMode.ui32DepthBits || psFrameBuffer->sMode.ui32StencilBits)
	{
		GLES2FrameBufferAttachable *psDepthAttachment, *psStencilAttachment;
		IMG_UINT32 ui32MultiSampleMultiplier = 1;
		GLES3DRegisters *ps3DRegs;
		IMG_UINT32 ui32Extent, ui32ISPZLSControl, ui32ISPZLoadBase, ui32ISPZStoreBase, ui32ISPStencilLoadBase;
		IMG_UINT32 ui32ISPStencilStoreBase, ui32RoundedWidth, ui32RoundedHeight;
		
		ui32ISPZLSControl 		= 0;
		ui32ISPZLoadBase 		= 0;
		ui32ISPZStoreBase 		= 0;
		ui32ISPStencilLoadBase 	= 0;
		ui32ISPStencilStoreBase = 0;

		ui32RoundedWidth = ALIGNCOUNT(psFrameBuffer->sDrawParams.ui32Width, EURASIA_TAG_TILE_SIZEX);
		ui32RoundedHeight = ALIGNCOUNT(psFrameBuffer->sDrawParams.ui32Height, EURASIA_TAG_TILE_SIZEX);

		ui32Extent = ui32RoundedWidth >> EURASIA_ISPREGION_SHIFTX;


		psDepthAttachment   = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES2_DEPTH_ATTACHMENT];
		psStencilAttachment = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES2_STENCIL_ATTACHMENT];

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
		/* Get the number of samples from one of the attachments - both are the same */
		/* Use this to adjust the width in tiles in the ZLSControl word */
		if(psDepthAttachment)
		{
			if(psDepthAttachment->ui32Samples>0)
			{
				ui32Extent *= 2;

				/* The stencil offset address depends on whether the depth attachment is multisampled */
				ui32MultiSampleMultiplier = 4;
			}
		}
		else if(psStencilAttachment) /* Stencil without depth */
		{
			if(psStencilAttachment->ui32Samples>0)
			{
				ui32Extent *= 2;

				ui32MultiSampleMultiplier = 4;
			}
		}
#endif

		if(psDepthAttachment)
		{
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE) || defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
			if (psDepthAttachment->eAttachmentType == GL_TEXTURE)
			{
				GLES2MipMapLevel *psMipLevel = (GLES2MipMapLevel*)psDepthAttachment;
				PVRSRV_CLIENT_MEM_INFO *psDepthMemInfo;

				/* Enable depth load/store */
				ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_ZLOADEN_MASK |
									 EUR_CR_ISP_ZLSCTL_ZSTOREEN_MASK |
									 EUR_CR_ISP_ZLSCTL_FORCEZSTORE_MASK |
									 EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK;

				psDepthMemInfo = psMipLevel->psTex->psMemInfo;

				ui32ISPZLoadBase  = psDepthMemInfo->sDevVAddr.uiAddr;
				ui32ISPZStoreBase = psDepthMemInfo->sDevVAddr.uiAddr;
			}
			else
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) || defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
			{
				GLES2RenderBuffer *psDepthRenderBuffer = (GLES2RenderBuffer*)psDepthAttachment;
				PVRSRV_CLIENT_MEM_INFO *psDepthMemInfo;

				GLES_ASSERT(psDepthAttachment->eAttachmentType == GL_RENDERBUFFER);

				/* Don't load on the first use of the buffer */
				if(psDepthRenderBuffer->bInitialised)
				{
					/* Enable depth load/store */
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_ZLOADEN_MASK |
										 EUR_CR_ISP_ZLSCTL_ZSTOREEN_MASK;
				}
				else
				{
					/* Enable depth store only */
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_ZSTOREEN_MASK;

					psDepthRenderBuffer->bInitialised = IMG_TRUE;
				}

				psDepthMemInfo = psDepthRenderBuffer->psMemInfo;

				ui32ISPZLoadBase  = psDepthMemInfo->sDevVAddr.uiAddr;
				ui32ISPZStoreBase = psDepthMemInfo->sDevVAddr.uiAddr;

				if(!gc->sAppHints.bFBODepthDiscard)
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_FORCEZSTORE_MASK |
										 EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK;
				}

			}
		}

		if(psStencilAttachment)
		{
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
			if(psStencilAttachment->eAttachmentType == GL_TEXTURE)
			{
				GLES2MipMapLevel *psMipLevel = (GLES2MipMapLevel*)psStencilAttachment;
				IMG_UINT32 ui32Address = psMipLevel->psTex->psMemInfo->sDevVAddr.uiAddr;

				GLES_ASSERT(psMipLevel->eRequestedFormat == GL_DEPTH_STENCIL_OES);

				/* Enable stencil load/store */
				ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_SLOADEN_MASK |
									 EUR_CR_ISP_ZLSCTL_SSTOREEN_MASK |
	 								 EUR_CR_ISP_ZLSCTL_FORCEZSTORE_MASK |
									 EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK;

				/* Stencil data is after depth data in 24_8 surface */
				ui32Address += ui32RoundedWidth * ui32RoundedHeight * 4 * ui32MultiSampleMultiplier;
				
				ui32ISPStencilLoadBase  =  ui32Address;
				ui32ISPStencilStoreBase =  ui32Address;
			}
			else
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) && defined(GLES2_EXTENSION_DEPTH_TEXTURE) */
			{
				GLES2RenderBuffer *psStencilRenderBuffer = (GLES2RenderBuffer *)psStencilAttachment;
				PVRSRV_CLIENT_MEM_INFO *psStencilMemInfo;

				GLES_ASSERT(psStencilAttachment->eAttachmentType == GL_RENDERBUFFER);

				/* Don't load on the first use of the buffer */
				if(psStencilRenderBuffer->bInitialised)
				{
					/* Enable stencil load/store */
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_SLOADEN_MASK |
										 EUR_CR_ISP_ZLSCTL_SSTOREEN_MASK;
				}
				else
				{
					/* Enable stencil store only */
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_SSTOREEN_MASK;

					psStencilRenderBuffer->bInitialised = IMG_TRUE;
				}

				psStencilMemInfo = psStencilRenderBuffer->psMemInfo;


#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
				if(psStencilRenderBuffer->eRequestedFormat == GL_DEPTH24_STENCIL8_OES)
				{
					IMG_UINT32 ui32Address = psStencilMemInfo->sDevVAddr.uiAddr;

					/* Stencil data is after depth data in 24_8 surface */
					ui32Address += ui32RoundedWidth * ui32RoundedHeight * 4 * ui32MultiSampleMultiplier;
					
					ui32ISPStencilLoadBase  =  ui32Address;
					ui32ISPStencilStoreBase =  ui32Address;
				}
				else
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
				{
					ui32ISPStencilLoadBase  =  psStencilMemInfo->sDevVAddr.uiAddr;
					ui32ISPStencilStoreBase =  psStencilMemInfo->sDevVAddr.uiAddr;
				}

				if(!gc->sAppHints.bFBODepthDiscard)
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_FORCEZSTORE_MASK |
										 EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK;
				}
			}
		}

		if(ui32ISPZLSControl)
		{
			ui32ISPZLSControl |= ((ui32Extent - 1) << EUR_CR_ISP_ZLSCTL_ZLSEXTENT_SHIFT) | 
#if !defined(SGX545)
								EUR_CR_ISP_ZLSCTL_LOADTILED_MASK |
								EUR_CR_ISP_ZLSCTL_STORETILED_MASK |
#endif /* !defined(SGX545) */
#if defined(SGX_FEATURE_ZLS_EXTERNALZ)
								EUR_CR_ISP_ZLSCTL_EXTERNALZBUFFER_MASK |
#endif
								EUR_CR_ISP_ZLSCTL_ZLOADFORMAT_F32Z |
								EUR_CR_ISP_ZLSCTL_ZSTOREFORMAT_F32Z;
		}

		ps3DRegs = &psFrameBuffer->sDrawParams.psRenderSurface->s3DRegs;

		GLES_SET_REGISTER(ps3DRegs->sISPZLSControl, EUR_CR_ISP_ZLSCTL, ui32ISPZLSControl);
		GLES_SET_REGISTER(ps3DRegs->sISPZLoadBase, EUR_CR_ISP_ZLOAD_BASE, ui32ISPZLoadBase);
		GLES_SET_REGISTER(ps3DRegs->sISPZStoreBase, EUR_CR_ISP_ZSTORE_BASE, ui32ISPZStoreBase);
		GLES_SET_REGISTER(ps3DRegs->sISPStencilLoadBase, EUR_CR_ISP_STENCIL_LOAD_BASE, ui32ISPStencilLoadBase);
		GLES_SET_REGISTER(ps3DRegs->sISPStencilStoreBase, EUR_CR_ISP_STENCIL_STORE_BASE, ui32ISPStencilStoreBase);
	}
	else
	{
		GLES3DRegisters *ps3DRegs;

		ps3DRegs = &psFrameBuffer->sDrawParams.psRenderSurface->s3DRegs;

		GLES_SET_REGISTER(ps3DRegs->sISPZLSControl, EUR_CR_ISP_ZLSCTL, 0);
		GLES_SET_REGISTER(ps3DRegs->sISPZLoadBase, EUR_CR_ISP_ZLOAD_BASE, 0);
		GLES_SET_REGISTER(ps3DRegs->sISPZStoreBase, EUR_CR_ISP_ZSTORE_BASE, 0);
		GLES_SET_REGISTER(ps3DRegs->sISPStencilLoadBase, EUR_CR_ISP_STENCIL_LOAD_BASE, 0);
		GLES_SET_REGISTER(ps3DRegs->sISPStencilStoreBase, EUR_CR_ISP_STENCIL_STORE_BASE, 0);
	}

}

/***********************************************************************************
 Function Name      : ComputeFrameBufferCompleteness
 Inputs             : gc
 Outputs            : 
 Returns            : -
 Description        : Computes the active framebuffer's status and updates the framebuffer accordingly.
************************************************************************************/
static IMG_VOID ComputeFrameBufferCompleteness(GLES2Context *gc)
{
	GLES2FrameBufferAttachable  *psAttachment;
	GLES2FrameBuffer *psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;
	PVRSRV_PIXEL_FORMAT ePixelFormat       = PVRSRV_PIXEL_FORMAT_UNKNOWN;
	EGLRenderSurface *psFBORenderSurface;
	IMG_MEMLAYOUT eMemFormat;
	IMG_UINT32          ui32NumAttachments = 0,
	                    ui32Width          = 0,
	                    ui32Height         = 0,
	                    ui32StencilBits    = 0,
	                    ui32DepthBits      = 0,
	                    ui32RedBits        = 0,
	                    ui32GreenBits      = 0,
	                    ui32BlueBits       = 0,
	                    ui32AlphaBits      = 0,
	                    ui32MipmapOffset   = 0,
	                    ui32BufferBits     = 0;
#if defined(GLES2_EXTENSION_EGL_IMAGE)
	EGLImage *psEGLImageTarget = IMG_NULL;
#endif
#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	/* use to check that attachments have a consistent number of samples */
	IMG_UINT32 ui32SamplesCheck = 0;
	IMG_BOOL   bFirstAttachment = IMG_TRUE;
#endif

	GLES_ASSERT(GLES2_FRAMEBUFFER_STATUS_UNKNOWN == psFrameBuffer->eStatus);

	/* Check color attachment completeness */
	psAttachment = psFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];

	if (psAttachment)
	{
		psAttachment->bGhosted = IMG_FALSE;
	}

	if(psAttachment && (GL_TEXTURE == psAttachment->eAttachmentType))
	{
		GLES2MipMapLevel *psLevel  = (GLES2MipMapLevel*)psAttachment;
		GLES2Texture     *psTex    = psLevel->psTex;
		IMG_UINT32 ui32TopUSize;
		IMG_UINT32 ui32TopVSize;
		IMG_UINT32 ui32Lod = psLevel->ui32Level % GLES2_MAX_TEXTURE_MIPMAP_LEVELS;

		if(!psLevel->psTexFormat)
		{
			/* The texture has no format. It means that glTexImage returned an error (the app probably ignored it).
			   That makes the texture invalid and unacceptable as an attachment.
			*/
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
			return;
		}

		ui32Width  = psLevel->ui32Width;
		ui32Height = psLevel->ui32Height;

		if((ui32Width == 0) || (ui32Height == 0))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
			return;
		}

		/* Cannot support rendering to non-power of 2 mipmap levels */
		if( ui32Lod && (((ui32Width & (ui32Width-1U)) != 0) || ((ui32Height & (ui32Height-1U)) != 0)) )
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED;
			return;
		}

		/* The vanilla fixed point textures are identified by their first (and only) chunk */
		switch(psLevel->psTexFormat->ePixelFormat)
		{
			case PVRSRV_PIXEL_FORMAT_ABGR8888:
			case PVRSRV_PIXEL_FORMAT_ARGB8888:
			{
				ui32RedBits   = 8;
				ui32GreenBits = 8;
				ui32BlueBits  = 8;

				if(psLevel->psTexFormat->ui32BaseFormatIndex == GLES2_RGBA_TEX_INDEX)
				{
					ui32AlphaBits = 8;
				}

				ui32BufferBits = 32;

				break;
			}
			case PVRSRV_PIXEL_FORMAT_ARGB1555:
			{
				ui32RedBits    = 5;
				ui32GreenBits  = 5;
				ui32BlueBits   = 5;
				ui32AlphaBits  = 1;
				ui32BufferBits = 16;

				break;
			}
			case PVRSRV_PIXEL_FORMAT_ARGB4444:
			{
				ui32RedBits    = 4;
				ui32GreenBits  = 4;
				ui32BlueBits   = 4;
				ui32AlphaBits  = 4;
				ui32BufferBits = 16;

				break;
			}
			case PVRSRV_PIXEL_FORMAT_RGB565:
			{
				ui32RedBits    = 5;
				ui32GreenBits  = 6;
				ui32BlueBits   = 5;
				ui32BufferBits = 16;

				break;
			}
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"ComputeFrameBufferCompleteness: Unknown texture format"));

				psFrameBuffer->eStatus =  GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

				return;
			}
		}
				
		ePixelFormat = psLevel->psTexFormat->ePixelFormat;				

		/* Make the texture resident immediately */
		if(!SetupTextureRenderTargetControlWords(gc, psTex))
		{
			psFrameBuffer->eStatus =  GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
			return;
		}

		ui32NumAttachments++;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
		ui32TopUSize = 1U << ((psTex->sState.aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
		ui32TopVSize = 1U << ((psTex->sState.aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
		ui32TopUSize = 1 + ((psTex->sState.aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
		ui32TopVSize = 1 + ((psTex->sState.aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

		ui32MipmapOffset = GetMipMapOffset(ui32Lod, ui32TopUSize, ui32TopVSize) * psLevel->psTexFormat->ui32TotalBytesPerTexel;

		if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
		{
			IMG_UINT32 ui32Face = psLevel->ui32Level/GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
			IMG_UINT32 ui32FaceOffset = 
				psLevel->psTexFormat->ui32TotalBytesPerTexel *	GetMipMapOffset(psTex->ui32NumLevels, ui32TopUSize, ui32TopVSize);

			if(psTex->ui32HWFlags & GLES2_MIPMAP)
			{
				if(((psLevel->psTexFormat->ui32TotalBytesPerTexel == 1) && (ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
					(ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}

			ui32MipmapOffset += (ui32FaceOffset * ui32Face);
		}

	}
	else if(psAttachment && (GL_RENDERBUFFER == psAttachment->eAttachmentType))
	{
		GLES2RenderBuffer *psRenderBuffer = (GLES2RenderBuffer*)psAttachment;
		
		ui32Width  = psRenderBuffer->ui32Width;
		ui32Height = psRenderBuffer->ui32Height;

		if((ui32Width == 0) || (ui32Height == 0))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

			return;
		}

		ui32RedBits   = psRenderBuffer->ui8RedSize;
		ui32GreenBits = psRenderBuffer->ui8GreenSize;
		ui32BlueBits  = psRenderBuffer->ui8BlueSize;
		ui32AlphaBits = psRenderBuffer->ui8AlphaSize;

		/* Can't have a depth/stencil renderbuffer attached to the color attachment */
		switch(psRenderBuffer->eRequestedFormat)
		{
			case GL_RGB565:
			{
				ePixelFormat = PVRSRV_PIXEL_FORMAT_RGB565;

				ui32BufferBits = 16;

				break;
			}
			case GL_RGBA4:
			{
				ePixelFormat = PVRSRV_PIXEL_FORMAT_ARGB4444;

				ui32BufferBits = 16;

				break;
			}
			case GL_RGB5_A1:
			{
				ePixelFormat = PVRSRV_PIXEL_FORMAT_ARGB1555;

				ui32BufferBits = 16;

				break;
			}
			case GL_RGBA8_OES:
			{
				ePixelFormat = PVRSRV_PIXEL_FORMAT_ABGR8888;

				ui32BufferBits = 32;

				break;
			}
			case GL_RGB8_OES:
			{
				ePixelFormat = PVRSRV_PIXEL_FORMAT_ABGR8888;

				ui32BufferBits = 32;

				break;
			}
			case GL_DEPTH_COMPONENT16:
			case GL_DEPTH_COMPONENT24_OES:
			case GL_STENCIL_INDEX8:
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
			case GL_DEPTH24_STENCIL8_OES:
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
				return;
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"GetFrameBufferCompleteness: format mismatch with RenderbufferStorage"));
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED;
				return;
			}
		}

		ui32NumAttachments++;
	}

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	/* Note the number of samples so we can check that other attachments match */
	if(psAttachment)
	{
		ui32SamplesCheck = psAttachment->ui32Samples;
		bFirstAttachment=IMG_FALSE;
	}
#endif


	/* Check depth attachment completeness */
	psAttachment = psFrameBuffer->apsAttachment[GLES2_DEPTH_ATTACHMENT];

	if (psAttachment)
	{
		psAttachment->bGhosted = IMG_FALSE;
	}

	if(psAttachment && (GL_TEXTURE == psAttachment->eAttachmentType))
	{
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE) || defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
		GLES2MipMapLevel *psMipLevel = (GLES2MipMapLevel *)psAttachment;
		IMG_UINT32 ui32Lod = psMipLevel->ui32Level % GLES2_MAX_TEXTURE_MIPMAP_LEVELS;

		/* Depth attachment dimensions must match color attachment,  if present */
		if(ui32NumAttachments && ((ui32Width  != psMipLevel->ui32Width) ||
								  (ui32Height != psMipLevel->ui32Height)))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;

			return;
		}

		ui32Width     = psMipLevel->ui32Width;
		ui32Height    = psMipLevel->ui32Height;
		ui32DepthBits = 16;

		if(!psMipLevel->psTexFormat)
		{
			/* The texture has no format. It means that glTexImage returned an error (the app probably ignored it).
			   That makes the texture invalid and unacceptable as an attachment.
			*/
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

			return;
		}

		if((ui32Width == 0) || (ui32Height == 0))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

			return;
		}

		/* Cannot support rendering to non-power of 2 mipmap levels */
		if( ui32Lod && (((ui32Width & (ui32Width-1U)) != 0) || ((ui32Height & (ui32Height-1U)) != 0)) )
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED;

			return;
		}

		/* Can't have a color/stencil texture attached to the depth attachment */
		switch(psMipLevel->psTexFormat->ui32BaseFormatIndex)
		{
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
			case GLES2_DEPTH_TEX_INDEX:
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
			case GLES2_DEPTH_STENCIL_TEX_INDEX:
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
				break;
			default:
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
				return;
		}

		/* Make the texture resident immediately */
		if(!SetupTextureRenderTargetControlWords(gc, psMipLevel->psTex))
		{
			psFrameBuffer->eStatus =  GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
			return;
		}

		ui32NumAttachments++;

#else /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) || defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

		/* Don't support depth format textures - so must be incomplete */
		psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

		return;

#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */
	}
	else if(psAttachment && (psAttachment->eAttachmentType == GL_RENDERBUFFER))
	{
		GLES2RenderBuffer *psRenderBuffer = (GLES2RenderBuffer*)psAttachment;
		
		/* Depth attachment dimensions must match color attachment,  if present */
		if(ui32NumAttachments && ((ui32Width != psRenderBuffer->ui32Width) ||
								(ui32Height != psRenderBuffer->ui32Height)))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
			return;
		}

		ui32Width     = psRenderBuffer->ui32Width;
		ui32Height    = psRenderBuffer->ui32Height;
		ui32DepthBits = psRenderBuffer->ui8DepthSize;

		/* Can't have a color/stencil renderbuffer attached to the depth attachment */
		switch(psRenderBuffer->eRequestedFormat)
		{
			case GL_DEPTH_COMPONENT16:	
			case GL_DEPTH_COMPONENT24_OES:	
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
			case GL_DEPTH24_STENCIL8_OES:
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
				break;
			case GL_RGB565:
			case GL_RGBA4:
			case GL_RGB5_A1:
			case GL_RGBA8_OES:
			case GL_RGB8_OES:
			case GL_STENCIL_INDEX8:
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
				return;
			default:
				PVR_DPF((PVR_DBG_ERROR,"GetFrameBufferCompleteness: format mismatch with RenderbufferStorage"));
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED;
				return;
		}

		if((ui32Width == 0) || (ui32Height == 0))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
			return;
		}

		ui32NumAttachments++;
	}

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	/* Check attachment samples consistency */
	if(psAttachment)
	{
		if(bFirstAttachment)
		{
			/* Note the number of samples so we can check that other attachments match */
			ui32SamplesCheck = psAttachment->ui32Samples;
			bFirstAttachment=IMG_FALSE;
		}
		else if(ui32SamplesCheck != psAttachment->ui32Samples)
		{
			PVR_DPF((PVR_DBG_ERROR,"ComputeFrameBufferCompleteness: Inconsistent samples across attachments."));
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_IMG;
			return;
		}
	}
#endif


	/* Check stencil attachment completeness */
	psAttachment = psFrameBuffer->apsAttachment[GLES2_STENCIL_ATTACHMENT];

	if (psAttachment)
	{
		psAttachment->bGhosted = IMG_FALSE;
	}

	if(psAttachment && (GL_TEXTURE == psAttachment->eAttachmentType))
	{
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
		GLES2MipMapLevel *psMipLevel = (GLES2MipMapLevel *)psAttachment;
		IMG_UINT32 ui32Lod = psMipLevel->ui32Level % GLES2_MAX_TEXTURE_MIPMAP_LEVELS;

		/* Depth attachment dimensions must match color attachment,  if present */
		if(ui32NumAttachments && ((ui32Width  != psMipLevel->ui32Width) ||
								  (ui32Height != psMipLevel->ui32Height)))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;

			return;
		}

		ui32Width     = psMipLevel->ui32Width;
		ui32Height    = psMipLevel->ui32Height;
		ui32StencilBits = 8;

		if(!psMipLevel->psTexFormat)
		{
			/* The texture has no format. It means that glTexImage returned an error (the app probably ignored it).
			   That makes the texture invalid and unacceptable as an attachment.
			*/
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

			return;
		}

		if((ui32Width == 0) || (ui32Height == 0))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

			return;
		}

		/* Cannot support rendering to non-power of 2 mipmap levels */
		if( ui32Lod && (((ui32Width & (ui32Width-1U)) != 0) || ((ui32Height & (ui32Height-1U)) != 0)) )
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED;

			return;
		}

		/* Can't have a color/depth texture attached to the stencil attachment */
		switch(psMipLevel->psTexFormat->ui32BaseFormatIndex)
		{
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
			case GLES2_DEPTH_STENCIL_TEX_INDEX:
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
				break;
			default:
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
				return;
		}

		/* Make the texture resident immediately */
		if(!SetupTextureRenderTargetControlWords(gc, psMipLevel->psTex))
		{
			psFrameBuffer->eStatus =  GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
			return;
		}

		ui32NumAttachments++;

#else /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

		/* Don't support stencil format textures - so must be incomplete */
		psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;

		return;

#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
	}
	else if(psAttachment && (GL_RENDERBUFFER == psAttachment->eAttachmentType))
	{
		GLES2RenderBuffer *psRenderBuffer = (GLES2RenderBuffer*)psAttachment;

		/* Stencil attachment dimensions must match color/depth attachments, 
		   if present 
		 */

		if(ui32NumAttachments && ((ui32Width != psRenderBuffer->ui32Width) ||
								(ui32Height != psRenderBuffer->ui32Height)))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
			return;
		}

		ui32Width       = psRenderBuffer->ui32Width;
		ui32Height      = psRenderBuffer->ui32Height;
		ui32StencilBits = psRenderBuffer->ui8StencilSize;

		/* Double-check against RenderbufferStorage */
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
		GLES_ASSERT((psRenderBuffer->eRequestedFormat == GL_STENCIL_INDEX8) ||
					(psRenderBuffer->eRequestedFormat == GL_DEPTH24_STENCIL8_OES));
#else
		GLES_ASSERT(psRenderBuffer->eRequestedFormat == GL_STENCIL_INDEX8);
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

		if((ui32Width == 0) || (ui32Height == 0))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
			return;
		}

		/* Can't have a color/depth renderbuffer attached to the stencil attachment */
		switch(psRenderBuffer->eRequestedFormat)
		{
			case GL_STENCIL_INDEX8:
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
			case GL_DEPTH24_STENCIL8_OES:
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
				break;
			case GL_DEPTH_COMPONENT16:	
			case GL_DEPTH_COMPONENT24_OES:	
			case GL_RGB565:
			case GL_RGBA4:
			case GL_RGB5_A1:
			case GL_RGBA8_OES:
			case GL_RGB8_OES:
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
				return;
			default:
				PVR_DPF((PVR_DBG_ERROR,"GetFrameBufferCompleteness: format mismatch with RenderbufferStorage"));
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED;
				return;
		}

		ui32NumAttachments++;
	}

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	/* Check attachment samples consistency */
	if(psAttachment)
	{
		if(bFirstAttachment)
		{
			/* Note the number of samples so we can check that other attachments match */
			ui32SamplesCheck = psAttachment->ui32Samples;
			bFirstAttachment=IMG_FALSE;
		}
		else if(ui32SamplesCheck != psAttachment->ui32Samples)
		{
			PVR_DPF((PVR_DBG_ERROR,"ComputeFrameBufferCompleteness: Inconsistent samples across attachments."));
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_IMG;
			return;
		}
	}
#endif


	if(ui32NumAttachments == 0)
	{
		psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
		return;
	}

	psFrameBuffer->sMode.ui32AlphaBits        = ui32AlphaBits;
	psFrameBuffer->sMode.ui32RedBits          = ui32RedBits;
	psFrameBuffer->sMode.ui32GreenBits        = ui32GreenBits;
	psFrameBuffer->sMode.ui32BlueBits         = ui32BlueBits;
	psFrameBuffer->sMode.ui32ColorBits        = ui32AlphaBits + ui32RedBits + ui32GreenBits + ui32BlueBits;
	psFrameBuffer->sMode.ui32DepthBits        = ui32DepthBits;
	psFrameBuffer->sMode.ui32StencilBits      = ui32StencilBits;

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	psFrameBuffer->sMode.ui32AntiAliasMode    = ui32SamplesCheck;
#endif

	psFrameBuffer->sDrawParams.eRotationAngle = PVRSRV_FLIP_Y;
	psFrameBuffer->sDrawParams.ePixelFormat   = ePixelFormat;
	psFrameBuffer->sDrawParams.ui32Width      = ui32Width;
	psFrameBuffer->sDrawParams.ui32Height     = ui32Height;

	psFBORenderSurface = IMG_NULL;

	if(psFrameBuffer->sMode.ui32ColorBits)
	{
		PVRSRV_CLIENT_SYNC_INFO	*psOverrideSyncInfo = IMG_NULL;

		psAttachment = psFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];

		GLES_ASSERT(psAttachment);

		if(psAttachment->eAttachmentType == GL_RENDERBUFFER)
		{
			GLES2RenderBuffer *psRenderBuffer = (GLES2RenderBuffer*)psAttachment;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
			if(psRenderBuffer->psEGLImageTarget)
			{
				psEGLImageTarget = psRenderBuffer->psEGLImageTarget;

				/* Use EGLImage sync info */
				psOverrideSyncInfo = psEGLImageTarget->psMemInfo->psClientSyncInfo;
			}
			else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
			{
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psRenderBuffer->psMemInfo->pvLinAddr;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psRenderBuffer->psMemInfo->sDevVAddr.uiAddr;
			}
		}
		else
		{
			GLES2Texture *psTex = ((GLES2MipMapLevel*)psAttachment)->psTex;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
			if(psTex->psEGLImageTarget)
			{
				psEGLImageTarget = psTex->psEGLImageTarget;

				/* Use EGLImage sync info */
				psOverrideSyncInfo = psEGLImageTarget->psMemInfo->psClientSyncInfo;
			}
			else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
			{
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psTex->psMemInfo->pvLinAddr;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psTex->psMemInfo->sDevVAddr.uiAddr;
			
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = (IMG_UINT8*)psFrameBuffer->sDrawParams.pvLinSurfaceAddress + ui32MipmapOffset;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress += ui32MipmapOffset;

				/* Use texture sync info */
				psOverrideSyncInfo = psTex->psMemInfo->psClientSyncInfo;
			}
		}

#if defined(GLES2_EXTENSION_EGL_IMAGE)
		if(psEGLImageTarget)
		{
			psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psEGLImageTarget->pvLinSurfaceAddress;
			psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psEGLImageTarget->ui32HWSurfaceAddress;
			psFrameBuffer->sDrawParams.ui32Stride = psEGLImageTarget->ui32Stride;
		}
		else
#endif
		{
			eMemFormat = GetColorAttachmentMemFormat(gc, psFrameBuffer);

			switch(eMemFormat)
			{
				case IMG_MEMLAYOUT_STRIDED:
				{
#if (EURASIA_TAG_STRIDE_THRESHOLD > 0)
					if(ui32Width < EURASIA_TAG_STRIDE_THRESHOLD)
					{
						psFrameBuffer->sDrawParams.ui32Stride           = (ui32Width + (EURASIA_TAG_STRIDE_ALIGN0 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN0 - 1);
					}
					else
#endif
					{
						psFrameBuffer->sDrawParams.ui32Stride           = (ui32Width + (EURASIA_TAG_STRIDE_ALIGN1 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN1 - 1);			
					}

					break;
				}
				case IMG_MEMLAYOUT_TILED:
				{
					psFrameBuffer->sDrawParams.ui32Stride = (ui32Width + (EURASIA_TAG_TILE_SIZEX - 1)) & ~(EURASIA_TAG_TILE_SIZEX - 1);			
					break;
				}
				case IMG_MEMLAYOUT_TWIDDLED:
				case IMG_MEMLAYOUT_HYBRIDTWIDDLED:
				{
					psFrameBuffer->sDrawParams.ui32Stride = ui32Width;
					break;
				}
			}

			psFrameBuffer->sDrawParams.ui32Stride          *= (ui32BufferBits >> 3);
		}

		psFrameBuffer->sDrawParams.psRenderSurface      = psAttachment->psRenderSurface;
		psFrameBuffer->sDrawParams.ui32AccumHWAddress	= psFrameBuffer->sDrawParams.ui32HWSurfaceAddress;

		/* If there is a render surface at this point, reuse it */
		if(!psAttachment->psRenderSurface)
		{
			/* Else, allocate a new render surface */
			psAttachment->psRenderSurface = GLES2Calloc(gc, sizeof(EGLRenderSurface));
			
			if(!psAttachment->psRenderSurface)
			{
				goto CouldNotCreateRenderSurface;
			}

			psFrameBuffer->sDrawParams.psRenderSurface = psAttachment->psRenderSurface;

			if(!KEGLCreateRenderSurface(gc->psSysContext, 
										&psFrameBuffer->sDrawParams, 
#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
										psFrameBuffer->sMode.ui32AntiAliasMode ? IMG_TRUE : IMG_FALSE, 
#else
										IMG_FALSE,
#endif
										IMG_FALSE,
										IMG_FALSE,
										psFrameBuffer->sDrawParams.psRenderSurface))
			{
				GLES2Free(IMG_NULL, psAttachment->psRenderSurface);
				psAttachment->psRenderSurface = 0;
				goto CouldNotCreateRenderSurface;
			}

			psAttachment->psRenderSurface->ui32FBOAttachmentCount = 1;

			if(GL_TEXTURE == psAttachment->eAttachmentType)
			{
				((GLES2MipMapLevel*)psAttachment)->psTex->ui32NumRenderTargets++;
			}
		}

		psFBORenderSurface = psAttachment->psRenderSurface;

		psAttachment->psRenderSurface->eAlphaFormat = PVRSRV_ALPHA_FORMAT_NONPRE;
		psAttachment->psRenderSurface->eColourSpace = PVRSRV_COLOURSPACE_FORMAT_NONLINEAR;

		/*
		** Use the sync info of the texture attachement or EGL Image
		** so that we can wait for HW texture uploads correctly
		*/
		if(psOverrideSyncInfo)
		{
			psFBORenderSurface->psSyncInfo		  = psOverrideSyncInfo;
			psFrameBuffer->sDrawParams.psSyncInfo = psOverrideSyncInfo;
		}
		else
		{
			/* Use the sync info of the render surface */
			psFBORenderSurface->psSyncInfo = psAttachment->psRenderSurface->psRenderSurfaceSyncInfo;

			/* Set draw params syncinfo to be rendersurface sync (this is not necessarily true for EGL surfaces) */
			psFrameBuffer->sDrawParams.psSyncInfo = psAttachment->psRenderSurface->psRenderSurfaceSyncInfo;
		}
	}


	psAttachment = psFrameBuffer->apsAttachment[GLES2_DEPTH_ATTACHMENT];

	if (psAttachment)
	{
		if(psAttachment->psRenderSurface && psFBORenderSurface && (psAttachment->psRenderSurface!=psFBORenderSurface))
		{
			/* Destroy the attachment RS if we've got an FBO RS and a different attachment RS */
			DestroyFBOAttachableRenderSurface(gc, psAttachment);
		}

		if(psFBORenderSurface)
		{
			if(!psAttachment->psRenderSurface)
			{
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE) || defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
				if(psAttachment->eAttachmentType == GL_TEXTURE)
				{
					GLES2MipMapLevel *psMipLevel = (GLES2MipMapLevel *)psAttachment;

					psMipLevel->psTex->ui32NumRenderTargets++;
				}
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) || defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

				/* Use the FBO RS on the attachment */
				psAttachment->psRenderSurface = psFBORenderSurface;

				/* Increment ref count */
				psAttachment->psRenderSurface->ui32FBOAttachmentCount++;

			}
			else
			{
				/* Attachment RS is the same as the FBO RS, so no need to do anything */
				GLES_ASSERT(psAttachment->psRenderSurface==psFBORenderSurface);
			}
		}
		else if(psAttachment->psRenderSurface)
		{
			/* Already have an attachment RS, but no FBO RS, so set the FBO RS to be the attachment RS */
			psFBORenderSurface = psAttachment->psRenderSurface;
		}
		else
		{
			/* No FBO RS or attachment RS, so create a new one */

			psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = 0;
			psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = 0;
			psFrameBuffer->sDrawParams.ui32Stride           = 0;
			psFrameBuffer->sDrawParams.psRenderSurface      = psAttachment->psRenderSurface;
			psFrameBuffer->sDrawParams.ui32AccumHWAddress	= psFrameBuffer->sDrawParams.ui32HWSurfaceAddress;

			/* Else, allocate a new render surface */
			psAttachment->psRenderSurface = GLES2Calloc(gc, sizeof(EGLRenderSurface));

			if(!psAttachment->psRenderSurface)
			{
				goto CouldNotCreateRenderSurface;
			}

			psFrameBuffer->sDrawParams.psRenderSurface = psAttachment->psRenderSurface;

			if(!KEGLCreateRenderSurface(gc->psSysContext, 
									&psFrameBuffer->sDrawParams, 
#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
									psFrameBuffer->sMode.ui32AntiAliasMode ? IMG_TRUE : IMG_FALSE, 
#else
									IMG_FALSE,
#endif
									IMG_FALSE,
									IMG_FALSE,
									psFrameBuffer->sDrawParams.psRenderSurface))
			{
				GLES2Free(IMG_NULL, psAttachment->psRenderSurface);

				psAttachment->psRenderSurface = 0;

				goto CouldNotCreateRenderSurface;
			}

#if defined(GLES2_EXTENSION_DEPTH_TEXTURE) || defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
			if(psAttachment->eAttachmentType == GL_TEXTURE)
			{
				GLES2MipMapLevel *psMipLevel = (GLES2MipMapLevel *)psAttachment;

				psMipLevel->psTex->ui32NumRenderTargets++;
			}
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) || defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

			psAttachment->psRenderSurface->ui32FBOAttachmentCount = 1;

			psFBORenderSurface = psAttachment->psRenderSurface;

			psAttachment->psRenderSurface->eAlphaFormat = PVRSRV_ALPHA_FORMAT_UNKNOWN;
			psAttachment->psRenderSurface->eColourSpace = PVRSRV_COLOURSPACE_FORMAT_UNKNOWN;

			/* Use the sync info of the render surface */
			psFBORenderSurface->psSyncInfo = psAttachment->psRenderSurface->psRenderSurfaceSyncInfo;

			/* Set draw params syncinfo to be rendersurface sync (this is not necessarily true for EGL surfaces) */
			psFrameBuffer->sDrawParams.psSyncInfo = psAttachment->psRenderSurface->psRenderSurfaceSyncInfo;
		}
	}

	psAttachment = psFrameBuffer->apsAttachment[GLES2_STENCIL_ATTACHMENT];

	if (psAttachment)
	{
		if(psAttachment->psRenderSurface && psFBORenderSurface && (psAttachment->psRenderSurface!=psFBORenderSurface))
		{
			/* Destroy the attachment RS if we've got an FBO RS and a different attachment RS */
			DestroyFBOAttachableRenderSurface(gc, psAttachment);
		}

		if(psFBORenderSurface)
		{
			if(!psAttachment->psRenderSurface)
			{
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
				if(psAttachment->eAttachmentType == GL_TEXTURE)
				{
					GLES2MipMapLevel *psMipLevel = (GLES2MipMapLevel *)psAttachment;

					psMipLevel->psTex->ui32NumRenderTargets++;
				}
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

				/* Use the FBO RS on the attachment */
				psAttachment->psRenderSurface = psFBORenderSurface;

				/* Increment ref count */
				psAttachment->psRenderSurface->ui32FBOAttachmentCount++;
			}
			else
			{
				/* Attachment RS is the same as the FBO RS, so no need to do anything */
				GLES_ASSERT(psAttachment->psRenderSurface==psFBORenderSurface);
			}
		}
		else if(psAttachment->psRenderSurface)
		{
			/* Already have an attachment RS, but no FBO RS, so set the FBO RS to be the attachment RS */
			/* psFBORenderSurface = psAttachment->psRenderSurface; */
		}
		else
		{
			psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = 0;
			psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = 0;
			psFrameBuffer->sDrawParams.ui32Stride           = 0;
			psFrameBuffer->sDrawParams.psRenderSurface      = psAttachment->psRenderSurface;
			psFrameBuffer->sDrawParams.ui32AccumHWAddress	= psFrameBuffer->sDrawParams.ui32HWSurfaceAddress;

			/* Else, allocate a new render surface */
			psAttachment->psRenderSurface = GLES2Calloc(gc, sizeof(EGLRenderSurface));

			if(!psAttachment->psRenderSurface)
			{
				goto CouldNotCreateRenderSurface;
			}

			psFrameBuffer->sDrawParams.psRenderSurface = psAttachment->psRenderSurface;

			if(!KEGLCreateRenderSurface(gc->psSysContext, 
									&psFrameBuffer->sDrawParams, 
#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
									psFrameBuffer->sMode.ui32AntiAliasMode ? IMG_TRUE : IMG_FALSE, 
#else
									IMG_FALSE,
#endif
									IMG_FALSE,
									IMG_FALSE,
									psFrameBuffer->sDrawParams.psRenderSurface))
			{
				GLES2Free(IMG_NULL, psAttachment->psRenderSurface);

				psAttachment->psRenderSurface = 0;

				goto CouldNotCreateRenderSurface;
			}

#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
			if(psAttachment->eAttachmentType == GL_TEXTURE)
			{
				GLES2MipMapLevel *psMipLevel = (GLES2MipMapLevel *)psAttachment;

				psMipLevel->psTex->ui32NumRenderTargets++;
			}	
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

			psAttachment->psRenderSurface->ui32FBOAttachmentCount = 1;

			psFBORenderSurface = psAttachment->psRenderSurface;

			psAttachment->psRenderSurface->eAlphaFormat = PVRSRV_ALPHA_FORMAT_UNKNOWN;
			psAttachment->psRenderSurface->eColourSpace = PVRSRV_COLOURSPACE_FORMAT_UNKNOWN;

			/* Use the sync info of the render surface */
			psFBORenderSurface->psSyncInfo = psAttachment->psRenderSurface->psRenderSurfaceSyncInfo;

			/* Set draw params syncinfo to be rendersurface sync (this is not necessarily true for EGL surfaces) */
			psFrameBuffer->sDrawParams.psSyncInfo = psAttachment->psRenderSurface->psRenderSurfaceSyncInfo;
		}
	}

	psFrameBuffer->sReadParams = psFrameBuffer->sDrawParams;

	/* There is no surface for reading. FIXME we are not supporting framebuffer objects with no color attachment */
	if(!psFrameBuffer->sMode.ui32ColorBits)
	{
		psFrameBuffer->sReadParams.psRenderSurface = IMG_NULL;
	}

	SetupFrameBufferZLS(gc, psFrameBuffer);

	psFrameBuffer->eStatus = GL_FRAMEBUFFER_COMPLETE;
	ChangeDrawableParams(gc, psFrameBuffer, &psFrameBuffer->sReadParams, &psFrameBuffer->sDrawParams);

	return;

CouldNotCreateRenderSurface:

	PVR_DPF((PVR_DBG_ERROR,"ComputeFrameBufferCompleteness: Can't create render surface."));

	SetError(gc, GL_OUT_OF_MEMORY);

	psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED;

	return;
}

/***********************************************************************************
 Function Name      : UpdateGhostedFrameBuffer
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Checks if any attachments to the framebuffer have been ghosted,
                      if they have update as necessary and clear the ghosted flag
************************************************************************************/
static IMG_VOID UpdateGhostedFrameBufferIfRequired(GLES2Context *gc)
{
	GLES2FrameBuffer *psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;
	GLES2FrameBufferAttachable *psAttachment;
	IMG_BOOL bNeedToUpdateZLS = IMG_FALSE;
	
	/* Color attachment */
	psAttachment = psFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];
	if (psAttachment && psAttachment->bGhosted)
	{
		GLES2MipMapLevel *psLevel  = (GLES2MipMapLevel*)psAttachment;
		GLES2Texture     *psTex    = psLevel->psTex;
		IMG_UINT32 ui32TopUSize;
		IMG_UINT32 ui32TopVSize;
		IMG_UINT32 ui32Lod = psLevel->ui32Level % GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
		IMG_UINT32 ui32MipmapOffset;

		psAttachment->bGhosted = IMG_FALSE;
		//Only texture attachments can be ghosted
		GLES_ASSERT(psAttachment->eAttachmentType == GL_TEXTURE);

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
		ui32TopUSize = 1U << ((psTex->sState.aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
		ui32TopVSize = 1U << ((psTex->sState.aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
		ui32TopUSize = 1 + ((psTex->sState.aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
		ui32TopVSize = 1 + ((psTex->sState.aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

		ui32MipmapOffset = GetMipMapOffset(ui32Lod, ui32TopUSize, ui32TopVSize) * psLevel->psTexFormat->ui32TotalBytesPerTexel;

		if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
		{
			IMG_UINT32 ui32Face = psLevel->ui32Level/GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
			IMG_UINT32 ui32FaceOffset = 
				psLevel->psTexFormat->ui32TotalBytesPerTexel *	GetMipMapOffset(psTex->ui32NumLevels, ui32TopUSize, ui32TopVSize);

			if(psTex->ui32HWFlags & GLES2_MIPMAP)
			{
				if(((psLevel->psTexFormat->ui32TotalBytesPerTexel == 1) && (ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
					(ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}

			ui32MipmapOffset += (ui32FaceOffset * ui32Face);
		}


		if (!SetupFrameBufferColorAttachment(gc, psFrameBuffer, ui32MipmapOffset))
		{
			return;
		}
	}

	/* Depth attachment */
	psAttachment = psFrameBuffer->apsAttachment[GLES2_DEPTH_ATTACHMENT];
	if (psAttachment && psAttachment->bGhosted)
	{
		psAttachment->bGhosted = IMG_FALSE;
		//Only texture attachments can be ghosted
		GLES_ASSERT(psAttachment->eAttachmentType == GL_TEXTURE);
		bNeedToUpdateZLS = IMG_TRUE;
	}

	/* Stencil attachment */
	psAttachment = psFrameBuffer->apsAttachment[GLES2_STENCIL_ATTACHMENT];
	if (psAttachment && psAttachment->bGhosted)
	{
		psAttachment->bGhosted = IMG_FALSE;
		//Only texture attachments can be ghosted
		GLES_ASSERT(psAttachment->eAttachmentType == GL_TEXTURE);
		bNeedToUpdateZLS = IMG_TRUE;
	}

	if (bNeedToUpdateZLS)
	{
		SetupFrameBufferZLS(gc, psFrameBuffer);
	}
}
/***********************************************************************************
 Function Name      : GetFrameBufferCompleteness
 Inputs             : gc
 Outputs            : gc->sFrameBuffer.psActiveFrameBuffer.eStatus
 Returns            : Frame buffer completeness error
 Description        : If the currently bound framebuffer completeness status is UNKNOWN, it computes it
                      and returns the result. Else, it simply returns the precomputed value
************************************************************************************/
IMG_INTERNAL GLenum GetFrameBufferCompleteness(GLES2Context *gc)
{
	IMG_BOOL bAssert;

	GLES2FrameBuffer *psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	GLES_ASSERT(psFrameBuffer);
	
	if(GLES2_FRAMEBUFFER_STATUS_UNKNOWN == psFrameBuffer->eStatus)
	{
		ComputeFrameBufferCompleteness(gc);
	}
	else
	{
		/* Checks if any attachments have been ghosted and updates accordingly */
		UpdateGhostedFrameBufferIfRequired(gc);
	}

	bAssert = ( (GL_FRAMEBUFFER_COMPLETE                      == psFrameBuffer->eStatus) ||
				(GL_FRAMEBUFFER_UNSUPPORTED                   == psFrameBuffer->eStatus) ||
				(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT         == psFrameBuffer->eStatus) ||
				(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT == psFrameBuffer->eStatus) ||
				(GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS         == psFrameBuffer->eStatus) 
#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
			 || (GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_IMG == psFrameBuffer->eStatus)
#endif
			  ) ? IMG_TRUE : IMG_FALSE;

	GLES_ASSERT(bAssert);

	PVR_UNREFERENCED_PARAMETER(bAssert);

	return psFrameBuffer->eStatus;
}


/***********************************************************************************
 Function Name      : NotifyFrameBuffer
 Inputs             : gc, psNamedItem
 Outputs            : -
 Returns            : -
 Description        : If the attachment is a renderbuffer and the framebuffer is
                      attached to it, its status is marked as unknown to force a future check.
                      If the attachment is a mipmap level and the framebuffer is
                      attached to any level of the same texture, its status is marked as unknown as well.
                      Called by FBOAttachableHasBeenModified through NamesArrayMapFunction.
************************************************************************************/
static IMG_VOID NotifyFrameBuffer(GLES2Context *gc, const IMG_VOID* pvAttachment, GLES2NamedItem *psNamedItem)
{
	IMG_UINT32                 i;
	GLES2FrameBuffer		   *psFrameBuffer = (GLES2FrameBuffer*)psNamedItem;
	GLES2FrameBufferAttachable *psAttachment  = (GLES2FrameBufferAttachable*)((IMG_UINTPTR_T)pvAttachment), *psFBAttachment;
	GLES2Texture               *psTex;

	PVR_UNREFERENCED_PARAMETER(gc);

	GLES_ASSERT(psFrameBuffer);
	GLES_ASSERT(psAttachment);

	if(GL_RENDERBUFFER == psAttachment->eAttachmentType)
	{
		for(i = 0; i < GLES2_MAX_ATTACHMENTS; ++i)
		{
			if(psFrameBuffer->apsAttachment[i] == psAttachment)
			{
				/* This framebuffer object was attached to this renderbuffer */
				FrameBufferHasBeenModified(psFrameBuffer);

				/* A renderbuffer can only be attached once to the same framebuffer */
				break;
			}
		}
	}
	else
	{
		GLES_ASSERT(GL_TEXTURE == psAttachment->eAttachmentType);

		psTex = ((GLES2MipMapLevel*)psAttachment)->psTex;

		for(i = 0; i < GLES2_MAX_ATTACHMENTS; ++i)
		{
			psFBAttachment = psFrameBuffer->apsAttachment[i];

			if(psFBAttachment &&
			   (GL_TEXTURE == psFBAttachment->eAttachmentType) &&
			   (((GLES2MipMapLevel*)psFBAttachment)->psTex == psTex))
			{
				/* This framebuffer object was attached to this mipmap level or another level of the same texture */
				FrameBufferHasBeenModified(psFrameBuffer);

				/* A texture can only be attached once to the same framebuffer */
				break;
			}
		}

	}
}


/***********************************************************************************
 Function Name      : FBOAttachableHasBeenModified
 Inputs             : gc, psAttachment
 Outputs            : psFrameBuffer->eStatus
 Returns            : -
 Description        : This is called by either glTexImage2D or glRenderBufferStorage.
                      It deletes the render surface if it existed and then notifies 
					  all attached framebuffers to force a future recheck of completeness.
************************************************************************************/
IMG_INTERNAL IMG_VOID FBOAttachableHasBeenModified(GLES2Context *gc, GLES2FrameBufferAttachable *psAttachment)
{
	GLES_ASSERT(psAttachment);

	/* 1- Delete the render surface */
	DestroyFBOAttachableRenderSurface(gc, psAttachment);

	/* 2- Notify all attached framebuffers about the change */
	NamesArrayMapFunction(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_FRAMEBUFFER], NotifyFrameBuffer, (IMG_VOID*)psAttachment);
}


/***********************************************************************************
 Function Name      : glIsRenderbuffer
 Inputs             : renderbuffer
 Outputs            : -
 Returns            : true/false
 Description        : Returns whether a named object is a renderbuffer
************************************************************************************/
GL_APICALL GLboolean GL_APIENTRY glIsRenderbuffer(GLuint renderbuffer)
{
	GLES2RenderBuffer *psRenderBuffer;
	GLES2NamesArray *psNamesArray;

	__GLES2_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsRenderbuffer"));
	
	GLES2_TIME_START(GLES2_TIMES_glIsRenderbuffer);

	if (renderbuffer==0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsRenderbuffer);
		return GL_FALSE;
	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER];

	psRenderBuffer = (GLES2RenderBuffer *) NamedItemAddRef(psNamesArray, renderbuffer);

	if (psRenderBuffer==IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsRenderbuffer);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psRenderBuffer);

	GLES2_TIME_STOP(GLES2_TIMES_glIsRenderbuffer);

	return GL_TRUE;
}

/***********************************************************************************
 Function Name      : glBindRenderbuffer
 Inputs             : target, renderbuffer
 Outputs            : -
 Returns            : -
 Description        : Sets current render buffer object for subsequent calls.
					  Will create an internal render buffer structure. Uses name table.
************************************************************************************/
GL_APICALL void GL_APIENTRY glBindRenderbuffer(GLenum target, GLuint renderbuffer)
{
	GLES2RenderBuffer *psRenderBuffer = IMG_NULL;
	GLES2RenderBuffer *psBoundRenderBuffer;
	GLES2NamesArray   *psNamesArray;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBindRenderbuffer"));

	GLES2_TIME_START(GLES2_TIMES_glBindRenderbuffer);

	/* Check that the target is valid */
	if(target != GL_RENDERBUFFER)
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES2_TIME_STOP(GLES2_TIMES_glBindRenderbuffer);
		return;
	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER];

	/*
	** Retrieve the framebuffer object from the namesArray structure.
	*/
	if (renderbuffer)
	{
		psRenderBuffer = (GLES2RenderBuffer *) NamedItemAddRef(psNamesArray, renderbuffer);

		/*
		** Is this the first time this name has been bound?
		** If so, create a new object and initialize it.
		*/
		if (IMG_NULL == psRenderBuffer)
		{
			// FIXME: Race condition. What happens if two threads try to bind at the same time
			// a renderbuffer with the same name that had not been created previously?
			// NOTE: The same problem is replicated in tex.c's BindTexture()
			psRenderBuffer = CreateRenderBufferObject(gc, renderbuffer);

			if (IMG_NULL == psRenderBuffer)
			{
				SetError(gc, GL_OUT_OF_MEMORY);

				GLES2_TIME_STOP(GLES2_TIMES_glBindRenderbuffer);

				return;
			}

			GLES_ASSERT(IMG_NULL != psRenderBuffer);
			GLES_ASSERT(renderbuffer == psRenderBuffer->sFBAttachable.sNamedItem.ui32Name);

			if(!InsertNamedItem(psNamesArray, (GLES2NamedItem*)psRenderBuffer))
			{
				FreeRenderBuffer(gc, psRenderBuffer);

				SetError(gc, GL_OUT_OF_MEMORY);

				GLES2_TIME_STOP(GLES2_TIMES_glBindRenderbuffer);

				return;
			}

			NamedItemAddRef(psNamesArray, renderbuffer);
		}
		else
		{
			/*
			** Retrieved an existing object.  Do some
			** sanity checks.
			*/
			GLES_ASSERT(renderbuffer == psRenderBuffer->sFBAttachable.sNamedItem.ui32Name);
		}
	}

	/*
	** Release that is being unbound.
	*/
	psBoundRenderBuffer = gc->sFrameBuffer.psActiveRenderBuffer;

	if (psBoundRenderBuffer && (((GLES2NamedItem*)psBoundRenderBuffer)->ui32Name != 0)) 
	{
		NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psBoundRenderBuffer);
	}

	/*
	** Install the new buffer into the correct target in this context.
	*/
	if(renderbuffer)
	{
		gc->sFrameBuffer.psActiveRenderBuffer = psRenderBuffer;
	}
	else
	{
		gc->sFrameBuffer.psActiveRenderBuffer = IMG_NULL;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glBindRenderbuffer);
}

/***********************************************************************************
 Function Name      : glDeleteRenderbuffers
 Inputs             : n, renderbuffers
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Deletes a list of n framebuffers.
					  Deletes internal framebuffer structures for named framebuffer objects
					  and if they were currently bound, binds 0 (default setting)
************************************************************************************/
GL_APICALL void GL_APIENTRY glDeleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
	GLES2NamesArray *psNamesArray;
	GLES2RenderBuffer *psRenderBuffer;

	IMG_INT32 i;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDeleteRenderbuffers"));

	GLES2_TIME_START(GLES2_TIMES_glDeleteRenderbuffers);

	if(!renderbuffers)
	{
		/* The application developer is having a bad day */
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteRenderbuffers);
		return;
	}

	if (n < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteRenderbuffers);
		return;
	}
	else if (n == 0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteRenderbuffers);
		return;
	}

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER];

	/*
	** If a renderbuffer object that is being deleted is currently bound,
	** bind the 0 name buffer to its target.
	*/
	for (i=0; i < n; i++)
	{
		if(renderbuffers[i] != 0)
		{
			/* Is the render buffer currently bound? Unbind it */
			psRenderBuffer = gc->sFrameBuffer.psActiveRenderBuffer;

			if (psRenderBuffer && ((GLES2NamedItem*)psRenderBuffer)->ui32Name == renderbuffers[i])
			{
				NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psRenderBuffer);

				gc->sFrameBuffer.psActiveRenderBuffer = IMG_NULL;
			}

			/*
			* If the renderbuffer is currently attached to the currently bound framebuffer, 
			* detach it. See section 4.4.2.2 of GL_EXT_framebuffer_object.
			*/
			RemoveFrameBufferAttachment(gc, IMG_TRUE, renderbuffers[i]);
		}
	}

	NamedItemDelRefByName(gc, psNamesArray, (IMG_UINT32)n, (const IMG_UINT32*)renderbuffers);


	GLES2_TIME_STOP(GLES2_TIMES_glDeleteRenderbuffers);
}

/***********************************************************************************
 Function Name      : glGenRenderbuffers
 Inputs             : n
 Outputs            : renderbuffers
 Returns            : -
 Description        : ENTRYPOINT: Generates a set of n names for renderbuffer objects
************************************************************************************/
GL_APICALL void GL_APIENTRY glGenRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGenRenderbuffers"));

	GLES2_TIME_START(GLES2_TIMES_glGenRenderbuffers);

	if (n < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
	}
	else if (n > 0 && renderbuffers)
	{
		GLES_ASSERT(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER] != NULL);

		NamesArrayGenNames(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER], (IMG_UINT32)n, (IMG_UINT32 *)renderbuffers);
	}

	GLES2_TIME_STOP(GLES2_TIMES_glGenRenderbuffers);
}


/***********************************************************************************
 Function Name      : RenderbufferStorage
 Inputs             : target, samples internalformat, width, height
                    : call with samples=0 for non-multisample
 Outputs            : 0
 Returns            : -
 Description        : Defines a renderbuffer and allocates memory for it
                    : Multisample internal version
************************************************************************************/
static IMG_VOID RenderbufferStorage(GLenum target, GLsizei samples, GLenum internalformat, 
										  GLsizei width, GLsizei height)
{
	GLES2RenderBuffer *psRenderBuffer;
	IMG_UINT32 ui32BufferSize;
	IMG_UINT8  ui8RedSize, ui8GreenSize, ui8BlueSize, ui8AlphaSize, ui8DepthSize, ui8StencilSize;
	PVRSRV_ERROR eError;

	__GLES2_GET_CONTEXT();

#if !defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	PVR_UNREFERENCED_PARAMETER(samples);
#endif
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glRenderbufferStorage"));

	GLES2_TIME_START(GLES2_TIMES_glRenderbufferStorage);

	/* Check that the target is valid */
	if(target != GL_RENDERBUFFER)
	{
		SetError(gc, GL_INVALID_ENUM);

		GLES2_TIME_STOP(GLES2_TIMES_glRenderbufferStorage);

		return;
	}

	/* According to the core spec, the following formats are required:
	 *     RGB565, RGBA4, RGB5_A1, DEPTH_COMPONENT16, STENCIL_INDEX8.
	 *
	 *  Besides, the following formats are optional:
	 *     RGBA8_OES, RGB8_OES, DEPTH_COMPONENT24_OES, DEPTH_COMPONENT32_OES, STENCIL_INDEX1_OES, 
	 *	   STENCIL_INDEX4_OES
	 */
	switch(internalformat)
	{
		/* These are the mandatory formats */
		case GL_RGB565:
		{
			ui8RedSize =     5;
			ui8GreenSize =   6;
			ui8BlueSize =    5;
			ui8AlphaSize =   0;
			ui8DepthSize =   0;
			ui8StencilSize = 0;
			ui32BufferSize = 2;
			break;
		}
		case GL_RGBA4:
		{
			ui8RedSize =     4;
			ui8GreenSize =   4;
			ui8BlueSize =    4;
			ui8AlphaSize =   4;
			ui8DepthSize =   0;
			ui8StencilSize = 0;
			ui32BufferSize = 2;
			break;
		}
		case GL_RGB5_A1:
		{
			ui8RedSize =     5;
			ui8GreenSize =   5;
			ui8BlueSize =    5;
			ui8AlphaSize =   1;
			ui8DepthSize =   0;
			ui8StencilSize = 0;
			ui32BufferSize = 2;
			break;
		}
		case GL_DEPTH_COMPONENT16:
		{
			ui8RedSize =     0;
			ui8GreenSize =   0;
			ui8BlueSize =    0;
			ui8AlphaSize =   0;
			ui8DepthSize =   16;
			ui8StencilSize = 0;
			ui32BufferSize = 4; /* Uses F32Z format */
			break;
		}

		/* These are the optional formats we support */
		case GL_RGBA8_OES:
		{
			ui8RedSize =     8;
			ui8GreenSize =   8;
			ui8BlueSize =    8;
			ui8AlphaSize =   8;
			ui8DepthSize =   0;
			ui8StencilSize = 0;
			ui32BufferSize = 4;
			break;
		}
		case GL_RGB8_OES:
		{
			ui8RedSize =     8;
			ui8GreenSize =   8;
			ui8BlueSize =    8;
			ui8AlphaSize =   0;
			ui8DepthSize =   0;
			ui8StencilSize = 0;
			ui32BufferSize = 4; /* Use RGBA8888 format */
			break;
		}
		case GL_DEPTH_COMPONENT24_OES:
		{
			ui8RedSize =     0;
			ui8GreenSize =   0;
			ui8BlueSize =    0;
			ui8AlphaSize =   0;
			ui8DepthSize =   24;
			ui8StencilSize = 0;
			ui32BufferSize = 4; /* Uses F32Z format */
			break;
		}
		case GL_STENCIL_INDEX8:
		{
			ui8RedSize =     0;
			ui8GreenSize =   0;
			ui8BlueSize =    0;
			ui8AlphaSize =   0;
			ui8DepthSize =   0;
			ui8StencilSize = 8;
			ui32BufferSize = 1;
			break;
		}
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
		case GL_DEPTH24_STENCIL8_OES:
		{
			ui8RedSize =     0;
			ui8GreenSize =   0;
			ui8BlueSize =    0;
			ui8AlphaSize =   0;
			ui8DepthSize =   24;
			ui8StencilSize = 8;
			ui32BufferSize = 5;
			break;
		}
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

		/* Else, the format is not supported */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glRenderbufferStorage);

			return;
		}
	}
	
	if(width < 0 || height < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glRenderbufferStorage);
		return;
	}

	/* The value of GL_MAX_RENDERBUFFER_SIZE is equal to GLES2_MAX_TEXTURE_SIZE */
	if(((IMG_UINT32)width > GLES2_MAX_TEXTURE_SIZE) || ((IMG_UINT32)height > GLES2_MAX_TEXTURE_SIZE))
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glRenderbufferStorage);

		return;
	}

	psRenderBuffer = gc->sFrameBuffer.psActiveRenderBuffer;

	if(IMG_NULL == psRenderBuffer)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glRenderbufferStorage);

		return;
	}


#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psRenderBuffer->psEGLImageSource)
	{
		/* Inform EGL that the source is being orphaned */
		KEGLUnbindImage(psRenderBuffer->psEGLImageSource->hImage);

		/* Source has been ghosted */
		psRenderBuffer->psMemInfo = IMG_NULL;
	}
	else if(psRenderBuffer->psEGLImageTarget)
	{
		ReleaseImageFromRenderbuffer(gc, psRenderBuffer);
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

	/* Notify all the framebuffers attached to this renderbuffers about the change */
	FBOAttachableHasBeenModified(gc, (GLES2FrameBufferAttachable*)psRenderBuffer);

	ui32BufferSize *= ((IMG_UINT32)width  + EURASIA_TAG_TILE_SIZEX-1) & ~(EURASIA_TAG_TILE_SIZEX-1);
	ui32BufferSize *= ((IMG_UINT32)height + EURASIA_TAG_TILE_SIZEY-1) & ~(EURASIA_TAG_TILE_SIZEY-1);

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	/* Number of samples is stores with the render buffer - copied to attachment later */
	if(samples==0)
	{
		psRenderBuffer->ui32Samples = 0;
	}
	else if(samples <= 4)
	{
		/* only support 2x2 anti alias in EGL */
		psRenderBuffer->ui32Samples = 4; 

		/* Allocate extra renderbuffer storage for multisample*/
		ui32BufferSize *= 4;
	}
#endif

	/* If the buffer already has memory allocated it is worth checking whether the
	 * new buffer size matches the old buffer size and avoid the burden of freeing the
	 * old memory and allocating a new chunk.
	 */
	if(ui32BufferSize != psRenderBuffer->ui32AllocatedBytes)
	{
		/* The size has changed. Free any memory that we had allocated earlier for this renderbuffer */
		if(psRenderBuffer->psMemInfo)
		{
			GLES2FREEDEVICEMEM_HEAP(gc, psRenderBuffer->psMemInfo);

			psRenderBuffer->psMemInfo = IMG_NULL;
		}

		/* Allocate new memory immediately */
		/* TODO: use heuristics to decide whether it is necessary to alloc external z/stencil buffers */
		if(width!=0 && height!=0)
		{
			eError = GLES2ALLOCDEVICEMEM_HEAP(gc,
				PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_MAP_GC_MMU,
				ui32BufferSize,
				EURASIA_CACHE_LINE_SIZE,
				&psRenderBuffer->psMemInfo);

			if (eError != PVRSRV_OK)
			{
				eError = GLES2ALLOCDEVICEMEM_HEAP(gc,
					PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_NO_SYNCOBJ,
					ui32BufferSize,
					EURASIA_CACHE_LINE_SIZE,
					&psRenderBuffer->psMemInfo);
			}

			if(eError != PVRSRV_OK)
			{
				SetError(gc, GL_OUT_OF_MEMORY);

				GLES2_TIME_STOP(GLES2_TIMES_glRenderbufferStorage);

				return;
			}
		}
	}

	psRenderBuffer->eRequestedFormat 	= internalformat;
	psRenderBuffer->ui32Width 			= (IMG_UINT32)width;
	psRenderBuffer->ui32Height 			= (IMG_UINT32)height;
	psRenderBuffer->ui8RedSize 			= ui8RedSize;
	psRenderBuffer->ui8GreenSize 		= ui8GreenSize;
	psRenderBuffer->ui8BlueSize 		= ui8BlueSize;
	psRenderBuffer->ui8AlphaSize 		= ui8AlphaSize;
	psRenderBuffer->ui8DepthSize 		= ui8DepthSize;
	psRenderBuffer->ui8StencilSize 		= ui8StencilSize;
	psRenderBuffer->ui32AllocatedBytes	= ui32BufferSize;
	psRenderBuffer->bInitialised 		= IMG_FALSE;

	GLES2_TIME_STOP(GLES2_TIMES_glRenderbufferStorage);
}

/***********************************************************************************
 Function Name      : glRenderbufferStorage
 Inputs             : target, internalformat, width, height
 Outputs            : 0
 Returns            : -
 Description        : ENTRYPOINT: Defines a renderbuffer and allocates memory for it
************************************************************************************/
GL_APICALL void GL_APIENTRY glRenderbufferStorage(GLenum target, GLenum internalformat, 
												 GLsizei width, GLsizei height)
{
	RenderbufferStorage(target, 0, internalformat, width, height);
}

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
/***********************************************************************************
 Function Name      : glRenderbufferStorageMultisampleIMG
 Inputs             : target, samples internalformat, width, height
 Outputs            : 0
 Returns            : -
 Description        : Defines a renderbuffer and allocates memory for it. 
                    : MultiSample extension version
************************************************************************************/
GL_API_EXT void GL_APIENTRY glRenderbufferStorageMultisampleIMG(
												 GLenum target, GLsizei samples, GLenum internalformat, 
												 GLsizei width, GLsizei height)
{
	__GLES2_GET_CONTEXT();

	if(samples<0 || samples>4)
	{
		SetError(gc, GL_INVALID_VALUE);

		return;
	}

	RenderbufferStorage(target, samples, internalformat, width, height);
}
#endif


#if defined(GLES2_EXTENSION_EGL_IMAGE)
/***********************************************************************************
 Function Name      : SetupRenderbufferFromEGLImage
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_BOOL SetupRenderbufferFromEGLImage(GLES2Context *gc, GLES2RenderBuffer *psRenderBuffer)
{
	IMG_UINT8 ui8RedSize, ui8GreenSize, ui8BlueSize, ui8AlphaSize, ui8DepthSize, ui8StencilSize;
	GLenum internalformat;
	EGLImage *psEGLImage;

	psEGLImage = psRenderBuffer->psEGLImageTarget;

	switch(psEGLImage->ePixelFormat)
	{
		/* These are the mandatory formats */
		case PVRSRV_PIXEL_FORMAT_RGB565:
		{
			ui8RedSize =     5;
			ui8GreenSize =   6;
			ui8BlueSize =    5;
			ui8AlphaSize =   0;
			ui8DepthSize =   0;
			ui8StencilSize = 0;
			internalformat = GL_RGB565;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			ui8RedSize =     4;
			ui8GreenSize =   4;
			ui8BlueSize =    4;
			ui8AlphaSize =   4;
			ui8DepthSize =   0;
			ui8StencilSize = 0;
			internalformat = GL_RGBA4;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			ui8RedSize =     5;
			ui8GreenSize =   5;
			ui8BlueSize =    5;
			ui8AlphaSize =   1;
			ui8DepthSize =   0;
			ui8StencilSize = 0;
			internalformat = GL_RGB5_A1;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		{
			ui8RedSize =     8;
			ui8GreenSize =   8;
			ui8BlueSize =    8;
			ui8AlphaSize =   8;
			ui8DepthSize =   0;
			ui8StencilSize = 0;
			internalformat = GL_RGBA8_OES;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_XBGR8888:
		{
			ui8RedSize =     8;
			ui8GreenSize =   8;
			ui8BlueSize =    8;
			ui8AlphaSize =   0;
			ui8DepthSize =   0;
			ui8StencilSize = 0;
			internalformat = GL_RGB8_OES;
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_WARNING,"SetupRenderbufferFromEGLImage: Unsupported pixel format"));

			return IMG_FALSE;
		}
	}

	/* Notify all the framebuffers attached to this renderbuffers about the change */
	FBOAttachableHasBeenModified(gc, (GLES2FrameBufferAttachable*)psRenderBuffer);

	/* Free any memory that we had allocated earlier for this renderbuffer */
	if(psRenderBuffer->psMemInfo)
	{
		GLES2FREEDEVICEMEM_HEAP(gc, psRenderBuffer->psMemInfo);

		psRenderBuffer->psMemInfo = IMG_NULL;
	}

	psRenderBuffer->eRequestedFormat	= internalformat;
	psRenderBuffer->ui32Width			= psEGLImage->ui32Width;
	psRenderBuffer->ui32Height			= psEGLImage->ui32Height;
	psRenderBuffer->ui8RedSize			= ui8RedSize;
	psRenderBuffer->ui8GreenSize		= ui8GreenSize;
	psRenderBuffer->ui8BlueSize			= ui8BlueSize;
	psRenderBuffer->ui8AlphaSize		= ui8AlphaSize;
	psRenderBuffer->ui8DepthSize		= ui8DepthSize;
	psRenderBuffer->ui8StencilSize		= ui8StencilSize;
	psRenderBuffer->ui32AllocatedBytes	= 0;
	psRenderBuffer->bInitialised		= IMG_FALSE;

	return IMG_TRUE;
}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */


/***********************************************************************************
= Function Name      : glGetRenderbufferParameteriv
 Inputs             : target, pname
 Outputs            : params
 Returns            : -
 Description        : Returns information about the currently bound renderbuffer
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetRenderbufferParameteriv(GLenum target, GLenum pname, GLint* params)
{
	GLES2RenderBuffer *psRenderBuffer;
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetRenderbufferParameteriv"));

	GLES2_TIME_START(GLES2_TIMES_glGetRenderbufferParameteriv);

	if(!params)
	{
		/* The app developer is having a bad day */
		GLES2_TIME_STOP(GLES2_TIMES_glGetRenderbufferParameteriv);
		return;
	}

	/* Check that the target is valid and that there's an active renderbuffer */
	if(GL_RENDERBUFFER != target)
	{
		SetError(gc, GL_INVALID_ENUM);

		GLES2_TIME_STOP(GLES2_TIMES_glGetRenderbufferParameteriv);

		return;
	}

	psRenderBuffer = gc->sFrameBuffer.psActiveRenderBuffer;

	if(IMG_NULL == psRenderBuffer)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glGetRenderbufferParameteriv);

		return;
	}

	switch(pname)
	{
		case GL_RENDERBUFFER_WIDTH:
		{
			*params = (GLint)(psRenderBuffer->ui32Width);

			break;
		}
		case GL_RENDERBUFFER_HEIGHT:
		{
			*params = (GLint)(psRenderBuffer->ui32Height);

			break;
		}
		case GL_RENDERBUFFER_INTERNAL_FORMAT:
		{
			*params = (GLint)(psRenderBuffer->eRequestedFormat);

			break;
		}
		case GL_RENDERBUFFER_RED_SIZE:
		{
			*params = (GLint)(psRenderBuffer->ui8RedSize);

			break;
		}
		case GL_RENDERBUFFER_GREEN_SIZE:
		{
			*params = (GLint)(psRenderBuffer->ui8GreenSize);

			break;
		}
		case GL_RENDERBUFFER_BLUE_SIZE:
		{
			*params = (GLint)(psRenderBuffer->ui8BlueSize);

			break;
		}
		case GL_RENDERBUFFER_ALPHA_SIZE:
		{
			*params = (GLint)(psRenderBuffer->ui8AlphaSize);

			break;
		}
		case GL_RENDERBUFFER_DEPTH_SIZE:
		{
			*params = (GLint)(psRenderBuffer->ui8DepthSize);

			break;
		}
		case GL_RENDERBUFFER_STENCIL_SIZE:
		{
			*params = (GLint)(psRenderBuffer->ui8StencilSize);

			break;
		}
#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
		case GL_RENDERBUFFER_SAMPLES_IMG:
		{
			*params = (GLint)(psRenderBuffer->ui32Samples);

			break;
		}
#endif
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glGetRenderbufferParameteriv);

			return;
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glGetRenderbufferParameteriv);
}


/***********************************************************************************
 Function Name      : glIsFramebuffer
 Inputs             : framebuffer
 Outputs            : -
 Returns            : true/false
 Description        : Returns whether a named object is a framebuffer
************************************************************************************/
GL_APICALL GLboolean GL_APIENTRY glIsFramebuffer(GLuint framebuffer)
{
	GLES2FrameBuffer *psFrameBuffer;
	GLES2NamesArray  *psNamesArray;

	__GLES2_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsFramebuffer"));

	GLES2_TIME_START(GLES2_TIMES_glIsFramebuffer);

	if (0 == framebuffer)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsFramebuffer);
		return GL_FALSE;
	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_FRAMEBUFFER]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_FRAMEBUFFER];

	psFrameBuffer = (GLES2FrameBuffer *) NamedItemAddRef(psNamesArray, framebuffer);

	if (IMG_NULL == psFrameBuffer)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsFramebuffer);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psFrameBuffer);

	GLES2_TIME_STOP(GLES2_TIMES_glIsFramebuffer);

	return GL_TRUE;
}

/***********************************************************************************
 Function Name      : glBindFramebuffer
 Inputs             : target, framebuffer
 Outputs            : -
 Returns            : -
 Description        : Sets current frame buffer object for subsequent calls.
					  Will create an internal frame buffer structure. Uses name table.
************************************************************************************/

GL_APICALL void GL_APIENTRY glBindFramebuffer(GLenum target, GLuint framebuffer)
{
	GLES2FrameBuffer *psFrameBuffer;
	GLES2FrameBuffer *psBoundFrameBuffer;
	GLES2NamesArray  *psNamesArray;
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBindFramebuffer"));

	GLES2_TIME_START(GLES2_TIMES_glBindFramebuffer);

	/* Check that the target is valid */
	if(target != GL_FRAMEBUFFER)
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES2_TIME_STOP(GLES2_TIMES_glBindFramebuffer);
		return;
	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_FRAMEBUFFER]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_FRAMEBUFFER];

	/*
	** Retrieve the framebuffer object from the namesArray structure.
	*/
	if (framebuffer) 
	{
		psFrameBuffer = (GLES2FrameBuffer *) NamedItemAddRef(psNamesArray, framebuffer);

		/*
		** Is this the first time this name has been bound?
		** If so, create a new object and initialize it.
		*/
		if (IMG_NULL == psFrameBuffer)
		{
			psFrameBuffer = CreateFrameBufferObject(gc, framebuffer);

			if(psFrameBuffer == IMG_NULL)
			{
				SetError(gc, GL_OUT_OF_MEMORY);

				GLES2_TIME_STOP(GLES2_TIMES_glBindFramebuffer);

				return;
			}

			GLES_ASSERT(IMG_NULL != psFrameBuffer);
			GLES_ASSERT(framebuffer == psFrameBuffer->sNamedItem.ui32Name);

			/* FIXME: race condition could cause returning an error when we should not */
			if(!InsertNamedItem(psNamesArray, (GLES2NamedItem*)psFrameBuffer))
			{
				FreeFrameBuffer(gc, psFrameBuffer);

				SetError(gc, GL_OUT_OF_MEMORY);

				GLES2_TIME_STOP(GLES2_TIMES_glBindFramebuffer);

				return;
			}

			NamedItemAddRef(psNamesArray, framebuffer);
		}
		else
		{
			/*
			** Retrieved an existing object.  Do some
			** sanity checks.
			*/
			GLES_ASSERT(framebuffer == psFrameBuffer->sNamedItem.ui32Name);
		}
	}
	else
	{
		psFrameBuffer = &gc->sFrameBuffer.sDefaultFrameBuffer;
	}

	psBoundFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	if (psBoundFrameBuffer)
	{
		/* Kick the TA to improve performance and to terminate the list of geometry */
		/* XXX: TODO: Support framebuffers with no color attachment */
		if(gc->psRenderSurface)
		{
			PVRSRVLockMutex(gc->psRenderSurface->hMutex);
		}

		if(gc->psRenderSurface &&
		   gc->psRenderSurface->bInFrame &&
		   (psBoundFrameBuffer->eStatus == GL_FRAMEBUFFER_COMPLETE))
		{
			IMG_UINT32 ui32KickFlags;

			ui32KickFlags = 0;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
			if(psBoundFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT])
			{
				if (psBoundFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT]->eAttachmentType==GL_RENDERBUFFER)
				{
					GLES2RenderBuffer *psRenderBuffer = (GLES2RenderBuffer *)psBoundFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];

					if(psRenderBuffer->psEGLImageSource || psRenderBuffer->psEGLImageTarget)
					{
						ui32KickFlags |= GLES2_SCHEDULE_HW_LAST_IN_SCENE;
					}
				}
				else if(psBoundFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT]->eAttachmentType==GL_TEXTURE)
				{
					GLES2MipMapLevel *psMipMapLevel = (GLES2MipMapLevel *)psBoundFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];

					if(psMipMapLevel->psTex->psEGLImageSource || psMipMapLevel->psTex->psEGLImageTarget)
					{
						ui32KickFlags |= GLES2_SCHEDULE_HW_LAST_IN_SCENE;
					}
				}
			}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

			if(gc->psRenderSurface->bInFrame)
			{
				if(gc->psRenderSurface->bPrimitivesSinceLastTA || ((ui32KickFlags & GLES2_SCHEDULE_HW_LAST_IN_SCENE) != 0))
				{
					GLES2_INC_COUNT(GLES2_TIMER_SGXKICKTA_BINDFRAMEBUFFER_COUNT, 1);
				}
			}

			if(ScheduleTA(gc, gc->psRenderSurface, ui32KickFlags) != IMG_EGL_NO_ERROR)
			{
				PVR_DPF((PVR_DBG_ERROR,"glBindFramebuffer: ScheduleTA did not work properly"));
			}
		}

		if(gc->psRenderSurface)
		{
			PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);
		}


		if(psBoundFrameBuffer->sNamedItem.ui32Name != 0)
		{
			NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psBoundFrameBuffer);
		}
	}

	/* Install the new buffer into the correct target in this context. */
	if(gc->sFrameBuffer.psActiveFrameBuffer != psFrameBuffer)
	{
		IMG_UINT32 i;

		gc->sFrameBuffer.psActiveFrameBuffer = psFrameBuffer;

		/* Setup various draw/read params state now the framebuffer has changed */
		ChangeDrawableParams(gc, psFrameBuffer, &psFrameBuffer->sReadParams, &psFrameBuffer->sDrawParams);

		/* We may need to ghost a texture attachment if it's still be sourced from */
		for(i=0; i<GLES2_MAX_ATTACHMENTS; i++)
		{
			GLES2FrameBufferAttachable *psAttachment;

			psAttachment = psFrameBuffer->apsAttachment[i];

			if(psAttachment && (GL_TEXTURE == psAttachment->eAttachmentType))
			{
				GLES2Texture *psTex;
				
				psTex = ((GLES2MipMapLevel*)psAttachment)->psTex;

				if(KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
				{
					psFrameBuffer->eStatus = GLES2_FRAMEBUFFER_STATUS_UNKNOWN;
				}
			}
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glBindFramebuffer);
}


/***********************************************************************************
 Function Name      : glDeleteFramebuffers
 Inputs             : n, framebuffers
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Deletes a list of n framebuffers.
					  Deletes internal framebuffer structures for named framebuffer objects
					  and if they were currently bound, binds 0 
					 (rebinds default, ie WSEGL framebuffer).
************************************************************************************/
GL_APICALL void GL_APIENTRY glDeleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
	GLES2NamesArray  *psNamesArray;
	GLES2FrameBuffer *psFrameBuffer;
	IMG_INT32        i;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDeleteFramebuffers"));

	GLES2_TIME_START(GLES2_TIMES_glDeleteFramebuffers);

	if(!framebuffers)
	{
		/* The application developer is having a bad day */
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteFramebuffers);
		return;
	}

	if (n < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteFramebuffers);
		return;
	}
	else if (n == 0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteFramebuffers);
		return;
	}

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_FRAMEBUFFER];

	/*
	** If a framebuffer object that is being deleted is currently bound,
	** bind the 0 name buffer (default WSEGL framebuffer) to its target.
	*/
	for (i=0; i < n; i++)
	{
		/*
		 * If the framebuffer object is currently bound, bind 0 to its target. 
		 */
		psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

		/* Is the framebuffer currently bound? */
		if (psFrameBuffer && (psFrameBuffer->sNamedItem.ui32Name == framebuffers[i]) && (psFrameBuffer->sNamedItem.ui32Name != 0)) 
		{
			GLES2FrameBuffer *psDefaultFrameBuffer = &gc->sFrameBuffer.sDefaultFrameBuffer;

			NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psFrameBuffer);

			/* Setup various draw/read params state now the framebuffer has changed */
			gc->sFrameBuffer.psActiveFrameBuffer = psDefaultFrameBuffer;
			ChangeDrawableParams(gc, psDefaultFrameBuffer, &psDefaultFrameBuffer->sReadParams, &psDefaultFrameBuffer->sDrawParams);
		}
	}

	NamedItemDelRefByName(gc, psNamesArray, (IMG_UINT32)n, (const IMG_UINT32*)framebuffers);


	GLES2_TIME_STOP(GLES2_TIMES_glDeleteFramebuffers);
}

/***********************************************************************************
 Function Name      : glGenFramebuffers
 Inputs             : n
 Outputs            : framebuffers
 Returns            : -
 Description        : ENTRYPOINT: Generates a set of n names for framebuffer objects
************************************************************************************/
GL_APICALL void GL_APIENTRY glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGenFramebuffers"));

	GLES2_TIME_START(GLES2_TIMES_glGenFramebuffers);

	if (n < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGenFramebuffers);
		return;
	}
	else if (n == 0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGenFramebuffers);
		return;
	}

	if (!framebuffers) 
	{
		/* The application developer is having a bad day */
		GLES2_TIME_STOP(GLES2_TIMES_glGenFramebuffers);
		return;
	}

	GLES_ASSERT(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_FRAMEBUFFER] != NULL);

	NamesArrayGenNames(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_FRAMEBUFFER], (IMG_UINT32)n, (IMG_UINT32 *)framebuffers);

	GLES2_TIME_STOP(GLES2_TIMES_glGenFramebuffers);

}

/***********************************************************************************
 Function Name      : glCheckFramebufferStatus
 Inputs             : target
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Checks the completeness of a framebuffer
************************************************************************************/
GL_APICALL GLenum GL_APIENTRY glCheckFramebufferStatus(GLenum target)
{
	GLenum eStatus;

	__GLES2_GET_CONTEXT_RETURN(0);

	PVR_DPF((PVR_DBG_CALLTRACE,"glCheckFramebufferStatus"));

	GLES2_TIME_START(GLES2_TIMES_glCheckFramebufferStatus);

	/* Check that the target is valid */
	if(target != GL_FRAMEBUFFER)
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES2_TIME_STOP(GLES2_TIMES_glCheckFramebufferStatus);
		return 0;
	}

	eStatus = GetFrameBufferCompleteness(gc);

	GLES2_TIME_STOP(GLES2_TIMES_glCheckFramebufferStatus);

	return eStatus;
}

/***********************************************************************************
 Function Name      : FramebufferTexture2D
 Inputs             : target, attachment, textarget, texture, level, samples
                    : call with samples=0 for non-multisample
 Outputs            : -
 Returns            : -
 Description        : Attaches a texture level to a framebuffer
                    : Multisample internal version
************************************************************************************/
static IMG_VOID GL_APIENTRY FramebufferTexture2D( GLenum target, GLenum attachment,
												  GLenum textarget, GLuint texture, GLint level, GLsizei samples)
{
	IMG_UINT32                 ui32Attachment;
	GLES2FrameBuffer           *psFrameBuffer;
	GLES2FrameBufferAttachable *psOldAttachment;
	GLES2NamesArray            *psNamesArray;
	GLES2Texture               *psTex;

	__GLES2_GET_CONTEXT();

#if !defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	PVR_UNREFERENCED_PARAMETER(samples);
#endif

	PVR_DPF((PVR_DBG_CALLTRACE,"glFramebufferTexture2D"));

	GLES2_TIME_START(GLES2_TIMES_glFramebufferTexture2D);

	/* Check that the target is valid */
	if(target != GL_FRAMEBUFFER)
	{
bad_enum:
		SetError(gc, GL_INVALID_ENUM);

		GLES2_TIME_STOP(GLES2_TIMES_glFramebufferTexture2D);

		return;
	}

	psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	if((psFrameBuffer == IMG_NULL) || (psFrameBuffer->sNamedItem.ui32Name == 0))
	{
		/* No framebuffer is bound to the target */
bad_op:
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glFramebufferTexture2D);

		return;
	}

	switch(attachment)
	{
		case GL_COLOR_ATTACHMENT0:
		{
			ui32Attachment = GLES2_COLOR_ATTACHMENT;

			break;
		}
		case GL_DEPTH_ATTACHMENT:
		{
			ui32Attachment = GLES2_DEPTH_ATTACHMENT;

			break;
		}
		case GL_STENCIL_ATTACHMENT:
		{
			ui32Attachment = GLES2_STENCIL_ATTACHMENT;

			break;
		}
		default:
		{
			goto bad_enum;
		}
	}

	switch(textarget)
	{
		case GL_TEXTURE_2D:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		{
			break;
		}
		default:
		{
			if(texture != 0)
			{
				goto bad_op;
			}

			break;
		}

	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ];

	psOldAttachment = psFrameBuffer->apsAttachment[ui32Attachment];

	if(psOldAttachment)
	{
		/* Kick the TA on the old surface */
		FlushAttachableIfNeeded(gc, psOldAttachment, 0);

		/* Decrement the refcount */
		if(GL_TEXTURE == psOldAttachment->eAttachmentType)
		{
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ], (GLES2NamedItem*)((GLES2MipMapLevel*)psOldAttachment)->psTex);
		}
		else
		{
			GLES_ASSERT(GL_RENDERBUFFER == psOldAttachment->eAttachmentType);

			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER], (GLES2NamedItem*)psOldAttachment);
		}
	}

	if(texture)
	{
		IMG_UINT32 ui32Face = 0;
		/*
		** Retrieve the texture from the psNamesArray structure.
		** This will increment the refcount as we require, so do not delete the reference at the end.
		*/
		psTex = (GLES2Texture *) NamedItemAddRef(psNamesArray, texture);

		if(!psTex)
		{
			goto bad_op;
		}

		if((IMG_UINT32)level >= GLES2_MAX_TEXTURE_MIPMAP_LEVELS)
		{
			SetError(gc, GL_INVALID_VALUE);

			GLES2_TIME_STOP(GLES2_TIMES_glFramebufferTexture2D);

			return;
		}

		if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
		{
			if ((textarget >= GL_TEXTURE_CUBE_MAP_POSITIVE_X) && (textarget <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z))
			{
				ui32Face = textarget - GL_TEXTURE_CUBE_MAP_POSITIVE_X + GLES2_TEXTURE_CEM_FACE_POSX;
			}
			else
			{
				NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psTex);

				goto bad_op;
			}
		}
		else if(textarget != GL_TEXTURE_2D)
		{
			NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psTex);

			goto bad_op;
		}

		psFrameBuffer->apsAttachment[ui32Attachment] = (GLES2FrameBufferAttachable*)
			&psTex->psMipLevel[ui32Face*GLES2_MAX_TEXTURE_MIPMAP_LEVELS + (IMG_UINT32)level];

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
		/* Mark this attachment as multisampled */
		if(samples==0)
		{
			psFrameBuffer->apsAttachment[ui32Attachment]->ui32Samples = 0;
		}
		/* only support 2x2 antialias in EGL */
		else if(samples<=4)
		{
			psFrameBuffer->apsAttachment[ui32Attachment]->ui32Samples = 4;
		}
#endif

	}
	else
	{
		/* Set to defaults */
		psFrameBuffer->apsAttachment[ui32Attachment] = IMG_NULL;
	}

	/* Set the status of this framebuffer as unknown */
	FrameBufferHasBeenModified(psFrameBuffer);

	GLES2_TIME_STOP(GLES2_TIMES_glFramebufferTexture2D);
}
/***********************************************************************************
 Function Name      : glFramebufferTexture2D
 Inputs             : target, attachment, textarget, texture, level
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Attaches a texture level to a framebuffer
************************************************************************************/
GL_APICALL void GL_APIENTRY glFramebufferTexture2D(GLenum target, GLenum attachment,
												  GLenum textarget, GLuint texture, GLint level)
{
	FramebufferTexture2D(target, attachment, textarget, texture, level, 0);
}

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
/***********************************************************************************
 Function Name      : glFramebufferTexture2DMultisampleIMG
 Inputs             : target, attachment, textarget, texture, level
 Outputs            : -
 Returns            : -
 Description        : EXTENSION ENTRYPOINT: Attaches a texture level to a framebuffer
************************************************************************************/
GL_API_EXT void GL_APIENTRY glFramebufferTexture2DMultisampleIMG(GLenum target, GLenum attachment,
												  GLenum textarget, GLuint texture, GLint level, GLsizei samples)
{
	__GLES2_GET_CONTEXT();

	/* The texture must be attached as the color attachment */
	if(attachment != GL_COLOR_ATTACHMENT0)
	{
		SetError(gc, GL_INVALID_VALUE);

		return;
	}

	/* check value of samples is within limits */
	if(samples<0 || samples>4)
	{
		SetError(gc, GL_INVALID_VALUE);

		return;
	}


	FramebufferTexture2D(target, attachment, textarget, texture, level, samples);
}
#endif

/***********************************************************************************
 Function Name      : glFramebufferRenderbuffer
 Inputs             : target, attachment, renderbuffertarget, renderbuffer
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Attaches a renderbuffer to a framebuffer
************************************************************************************/
GL_APICALL void GL_APIENTRY glFramebufferRenderbuffer(GLenum target, GLenum attachment,
													 GLenum renderbuffertarget, GLuint renderbuffer)
{
	IMG_UINT32                 ui32Attachment;
	GLES2FrameBuffer           *psFrameBuffer;
	GLES2RenderBuffer          *psRenderBuffer;
	GLES2NamesArray            *psNamesArray;
	GLES2FrameBufferAttachable *psOldAttachment;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFramebufferRenderbuffer"));

	GLES2_TIME_START(GLES2_TIMES_glFramebufferRenderbuffer);

	/* Check that the target is valid */
	if(target != GL_FRAMEBUFFER)
	{
bad_enum:
		SetError(gc, GL_INVALID_ENUM);
		GLES2_TIME_STOP(GLES2_TIMES_glFramebufferRenderbuffer);
		return;
	}

	psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	if((psFrameBuffer == IMG_NULL) || (psFrameBuffer->sNamedItem.ui32Name == 0))
	{
bad_op:
		SetError(gc, GL_INVALID_OPERATION);
		GLES2_TIME_STOP(GLES2_TIMES_glFramebufferRenderbuffer);
		return;
	}
	
	switch(attachment)
	{
		case GL_COLOR_ATTACHMENT0:
			ui32Attachment = GLES2_COLOR_ATTACHMENT;
			break;
		case GL_DEPTH_ATTACHMENT:
			ui32Attachment = GLES2_DEPTH_ATTACHMENT;
			break;
		case GL_STENCIL_ATTACHMENT:
			ui32Attachment = GLES2_STENCIL_ATTACHMENT;
			break;
		default:
			goto bad_enum;
	}

	if(renderbuffer != 0 && renderbuffertarget != GL_RENDERBUFFER)
	{
		goto bad_op;
	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER];

	psOldAttachment = psFrameBuffer->apsAttachment[ui32Attachment];

	if(psOldAttachment)
	{
		/* Kick the TA on the old surface */
		FlushAttachableIfNeeded(gc, psOldAttachment, 0);

		/* Decrement the refcount */
		if(GL_TEXTURE == psOldAttachment->eAttachmentType)
		{
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ], (GLES2NamedItem*)((GLES2MipMapLevel*)psOldAttachment)->psTex);
		}
		else
		{
			GLES_ASSERT(GL_RENDERBUFFER == psOldAttachment->eAttachmentType);
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_RENDERBUFFER], (GLES2NamedItem*)psOldAttachment);
		}
	}

	if(renderbuffer)
	{
		/*
		** Retrieve the render buffer object from the psNamesArray structure.
		** This will increment the refcount as we require, so do not delete the reference at the end.
		*/
		psRenderBuffer = (GLES2RenderBuffer *) NamedItemAddRef(psNamesArray, renderbuffer);

		if(psRenderBuffer == IMG_NULL)
		{
			goto bad_op;
		}

		psFrameBuffer->apsAttachment[ui32Attachment] = (GLES2FrameBufferAttachable*)psRenderBuffer;

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
		/* Copy the samples number into the attachable object too */
		psFrameBuffer->apsAttachment[ui32Attachment]->ui32Samples = psRenderBuffer->ui32Samples;
#endif

	}
	else
	{
		psFrameBuffer->apsAttachment[ui32Attachment] = IMG_NULL;
	}

	/* Set the status of this framebuffer as unknown */
	FrameBufferHasBeenModified(psFrameBuffer);

	GLES2_TIME_STOP(GLES2_TIMES_glFramebufferRenderbuffer);
}

/***********************************************************************************
 Function Name      : glGetFramebufferAttachmentParameteriv
 Inputs             : target, attachment, pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Gets information about the attachments of a 
					  framebuffer object
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetFramebufferAttachmentParameteriv(GLenum target, GLenum attachment,
																 GLenum pname, GLint *params)
{
	IMG_UINT32                 ui32Attachment;
	GLES2FrameBuffer           *psFrameBuffer;
	GLES2FrameBufferAttachable *psAttachment;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetFramebufferAttachmentParameteriv"));

	GLES2_TIME_START(GLES2_TIMES_glGetFramebufferAttachmentParameteriv);

	/* Check that the target is valid */
	if(target != GL_FRAMEBUFFER)
	{
bad_enum:
		SetError(gc, GL_INVALID_ENUM);
		GLES2_TIME_STOP(GLES2_TIMES_glGetFramebufferAttachmentParameteriv);
		return;
	}

	psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	if((psFrameBuffer == IMG_NULL) || (psFrameBuffer->sNamedItem.ui32Name == 0))
	{
		SetError(gc, GL_INVALID_OPERATION);
		GLES2_TIME_STOP(GLES2_TIMES_glGetFramebufferAttachmentParameteriv);
		return;
	}

	switch(attachment)
	{
		case GL_COLOR_ATTACHMENT0:
			ui32Attachment = GLES2_COLOR_ATTACHMENT;
			break;
		case GL_DEPTH_ATTACHMENT:
			ui32Attachment = GLES2_DEPTH_ATTACHMENT;
			break;
		case GL_STENCIL_ATTACHMENT:
			ui32Attachment = GLES2_STENCIL_ATTACHMENT;
			break;
		default:
			goto bad_enum;
	}

	psAttachment = psFrameBuffer->apsAttachment[ui32Attachment];

	switch(pname)
	{
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
		{
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
		case GL_TEXTURE_SAMPLES_IMG:
		{
			if(!psAttachment)
			{
				goto bad_enum;
			}
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
		{
			if(!psAttachment || psAttachment->eAttachmentType != GL_TEXTURE)
			{
				goto bad_enum;
			}
			break;
		}
		default:
			goto bad_enum;
	}

	if(!params)
	{
		/* The app developer is having a bad day */
		GLES2_TIME_STOP(GLES2_TIMES_glGetFramebufferAttachmentParameteriv);
		return;
	}

	switch(pname)
	{
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE:
		{
			if(psAttachment)
			{
				*params = (GLint)psAttachment->eAttachmentType;
			}
			else
			{
				*params = GL_NONE;
			}
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME:
		{
			if(psAttachment->eAttachmentType == GL_TEXTURE)
			{
				GLES2MipMapLevel *psMipLevel = (GLES2MipMapLevel *)psAttachment;

				*params = (GLint)(psMipLevel->psTex->sNamedItem.ui32Name);
			}
			else
			{
				*params = (GLint)(((GLES2NamedItem*)psAttachment)->ui32Name);
			}
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL:
		{
			/* XXX: correct but ugly */
			IMG_UINT32 ui32Face = ((GLES2MipMapLevel*)psAttachment)->ui32Level/GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
			*params = (GLint)(((GLES2MipMapLevel*)psAttachment)->ui32Level - ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS);
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE:
		{
		     GLES2MipMapLevel * psMipLevel = (GLES2MipMapLevel *)psAttachment;
		     
		     if (psMipLevel->psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
		     {
			  IMG_UINT32 ui32Face = psMipLevel->ui32Level / GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
			  *params = (GLint)(ui32Face + (IMG_UINT32)GL_TEXTURE_CUBE_MAP_POSITIVE_X);
		     }
		     else
		     {
			  *params = 0;
		     }
		     break;
		}
#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
		case GL_TEXTURE_SAMPLES_IMG:
		{
			if(attachment == GL_COLOR_ATTACHMENT0)
			{
				*params = (GLint)(psAttachment->ui32Samples);
			}
			else
			{
				goto bad_enum;
			}
			break;
		}
#endif
	}

	GLES2_TIME_STOP(GLES2_TIMES_glGetFramebufferAttachmentParameteriv);
}

/***********************************************************************************
 Function Name      : glGenerateMipmap
 Inputs             : target
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Generates a mipmap chain for a texture
************************************************************************************/

GL_APICALL void GL_APIENTRY glGenerateMipmap(GLenum eTarget)
{
	GLenum                   eErrorCode = GL_NO_ERROR;
	GLES2Texture             *psTex;
	IMG_UINT32               ui32TexTarget, ui32Face;
	GLES2MipMapLevel         *psLevel;
	GLES2TextureParamState   *psParams;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE, "glGenerateMipmap"));

	GLES2_TIME_START(GLES2_TIMES_glGenerateMipmap);

	/* Get the target texture */
	switch(eTarget)
	{
		case GL_TEXTURE_2D:
		{
			ui32TexTarget = GLES2_TEXTURE_TARGET_2D;

			break;
		}
		case GL_TEXTURE_CUBE_MAP:
		{
			ui32TexTarget = GLES2_TEXTURE_TARGET_CEM;

			break;
		}
		default:
		{
			eErrorCode = GL_INVALID_ENUM;

			goto ExitWithError;
		}
	}

	psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture][ui32TexTarget];

	if (psTex == IMG_NULL)
	{
		eErrorCode = GL_INVALID_OPERATION;

		goto ExitWithError;
	}

	psParams = &psTex->sState;
	psParams->bAutoMipGen = IMG_TRUE;

	psLevel = &psTex->psMipLevel[0];

	/* Check that the texture base level is non-float */
	/* Mipmapping float textures is part of an unsupported extension */
	switch(psLevel->psTexFormat->ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_A32B32G32R32F:
		case PVRSRV_PIXEL_FORMAT_B32G32R32F:
		case PVRSRV_PIXEL_FORMAT_A_F32:
		case PVRSRV_PIXEL_FORMAT_L_F32:
		case PVRSRV_PIXEL_FORMAT_L_F32_A_F32:
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16F:
		case PVRSRV_PIXEL_FORMAT_B16G16R16F:
		case PVRSRV_PIXEL_FORMAT_A_F16:
		case PVRSRV_PIXEL_FORMAT_L_F16:
		case PVRSRV_PIXEL_FORMAT_L_F16_A_F16:
			eErrorCode = GL_INVALID_OPERATION;
			goto ExitWithError;
		default:
			break;
	}

#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
	if(psTex->psFormat && psTex->psFormat->ui32BaseFormatIndex==GLES2_DEPTH_TEX_INDEX)
	{
		eErrorCode = GL_INVALID_OPERATION;

		goto ExitWithError;
	}
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */

	/* For cube maps, all the sides must be identical and square. See section 3.8.10 of the spec */
	if(ui32TexTarget == GLES2_TEXTURE_TARGET_CEM)
	{
		/* Get the size of the first face of the cube */
		IMG_UINT32 ui32RefWidth = psLevel->ui32Width, ui32RefHeight = psLevel->ui32Height;

		/* Check that it is square */
		if( ui32RefWidth != ui32RefHeight )
		{
			eErrorCode = GL_INVALID_OPERATION;

			goto ExitWithError;
		}

		/* And then check that every face has the same size as the first one */
		for(ui32Face=1; ui32Face < GLES2_TEXTURE_CEM_FACE_MAX; ++ui32Face)
		{
			psLevel = &psTex->psMipLevel[ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS];

			if( (psLevel->ui32Width  != ui32RefWidth) &&
				(psLevel->ui32Height != ui32RefHeight))
			{
				eErrorCode = GL_INVALID_OPERATION;

				goto ExitWithError;
			}
		}
	}

	/* Finally, do generate the mipmaps. See makemips.c */
	if(!MakeTextureMipmapLevels(gc, psTex, ui32TexTarget))
	{
		/* Something is wrong in the driver, but let's blame the app. */
		eErrorCode = GL_OUT_OF_MEMORY;

		goto ExitWithError;
	}

ExitWithError:
	/* Exit gracefully */
	if (eErrorCode != GL_NO_ERROR)
	{
		GLES_ASSERT(eErrorCode != GL_NO_ERROR);

		SetError(gc, eErrorCode);
	}

	gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;

	GLES2_TIME_STOP(GLES2_TIMES_glGenerateMipmap);
}

#if defined(GLES2_EXTENSION_DISCARD_FRAMEBUFFER)
/***********************************************************************************
 Function Name      : glDiscardFramebufferEXT
 Inputs             : target, numAttachments, attachments
 Outputs            : None
 Returns            : -
                      Can set GL_INVALID_VALUE and GL_INVALID_ENUM errors
 Description        : ENTRYPOINT: Causes the contents of the named framebuffer attachable
                      images to become undefined. The effect here is to not enable a depth
					  or stencil store in the ISP ZLSControl word at the next TA Kick.
                      Note: This ENTRYPOINT call directly affects the registers being
					        prepared in the current render surface. It was not deferred
							to processing in sgxif.c due to the requirement of placing
							control flags in the 3DRegs anyway.
                      Note: Must always have a FORCE if either a Z or S ENABLE is set.
************************************************************************************/
GL_API_EXT void GL_APIENTRY glDiscardFramebufferEXT(GLenum target, GLsizei numAttachments, const GLenum *attachments)
{
	IMG_INT32 i;
	IMG_PUINT32 pui32ISPZLSControl; /* The ISP Control word to be written */
	IMG_BOOL bDiscardColor = IMG_FALSE;
	IMG_BOOL bDiscardDepth = IMG_FALSE;
	IMG_BOOL bDiscardStencil = IMG_FALSE;
	
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE, "glDiscardFramebufferEXT"));

	GLES2_TIME_START(GLES2_TIMES_glDiscardFramebufferEXT);
	
	/* target must be GL_FRAMEBUFFER */
	if (GL_FRAMEBUFFER != target)
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES2_TIME_STOP(GLES2_TIMES_glDiscardFramebufferEXT);
		return;
	}

	/* check numAttachments is not invalid */
	if (numAttachments <= 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glDiscardFramebufferEXT);
		return;
	}


	/* Check what was passed as attachments */
	for (i=0;i<numAttachments;i++)
	{
		/* Set whether this operation is done on the default FB or on a FB Object*/
		if (gc->sFrameBuffer.psActiveFrameBuffer == &gc->sFrameBuffer.sDefaultFrameBuffer)
		{
			switch (attachments[i])
			{
				case GL_COLOR_EXT:
					bDiscardColor = IMG_TRUE;
					break;
				case GL_DEPTH_EXT:
					bDiscardDepth = IMG_TRUE;
					break;
				case GL_STENCIL_EXT:
					bDiscardStencil = IMG_TRUE;
					break;
				default:
					SetError(gc, GL_INVALID_ENUM);
					GLES2_TIME_STOP(GLES2_TIMES_glDiscardFramebufferEXT);
					return;
			}
		}
		else
		{
			switch (attachments[i])
			{
				case GL_COLOR_ATTACHMENT0:
					bDiscardColor = IMG_TRUE;
					break;
				case GL_DEPTH_ATTACHMENT:
					bDiscardDepth = IMG_TRUE;
					break;
				case GL_STENCIL_ATTACHMENT:
					bDiscardStencil = IMG_TRUE;
					break;
				default:
					SetError(gc, GL_INVALID_ENUM);
					GLES2_TIME_STOP(GLES2_TIMES_glDiscardFramebufferEXT);
					return;
			}
		}
	}

	if(gc->psRenderSurface)
	{
		/* Now make the changes to the ISPZLSControl word in the current render surface */
		pui32ISPZLSControl = &gc->psRenderSurface->s3DRegs.sISPZLSControl.ui32RegVal;
			
		if (bDiscardDepth)
		{
			/* Disable Z Store */
			*pui32ISPZLSControl &= ~EUR_CR_ISP_ZLSCTL_ZSTOREEN_MASK;
		}
		if (bDiscardStencil)
		{
			/* Disable stencil store */
			*pui32ISPZLSControl &= ~EUR_CR_ISP_ZLSCTL_SSTOREEN_MASK;
		}

		/* Check if we have to turn off the FORCE state too */
		if ( ((*pui32ISPZLSControl & EUR_CR_ISP_ZLSCTL_ZSTOREEN_MASK) == 0) && ((*pui32ISPZLSControl & EUR_CR_ISP_ZLSCTL_SSTOREEN_MASK) == 0) )
		{
			*pui32ISPZLSControl &= ~EUR_CR_ISP_ZLSCTL_FORCEZSTORE_MASK;
		}

		GLES_ASSERT(gc->psMode);

		/* Have all attachments been discarded? */
		if((!gc->psMode->ui32ColorBits || bDiscardColor) && 
			(!gc->psMode->ui32DepthBits || bDiscardDepth) && 
			(!gc->psMode->ui32StencilBits || bDiscardStencil))
		{
			FlushRenderSurface(gc, gc->psRenderSurface, GLES2_SCHEDULE_HW_DISCARD_SCENE);
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glDiscardFramebufferEXT);
}
#endif /* GLES2_EXTENSION_DISCARD_FRAMEBUFFER */


/******************************************************************************
 End of file(fbo.c)
******************************************************************************/

