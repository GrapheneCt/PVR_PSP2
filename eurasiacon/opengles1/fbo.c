/******************************************************************************
 * Name         : fbo.c
 *
 * Copyright    : 2006-2007 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park  Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: fbo.c $
 * 
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "context.h"

#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
#include "twiddle.h"
#endif

/* OES_framebuffer_object */

#define GLES1_FRAMEBUFFER_STATUS_UNKNOWN          0xDEAD


/***********************************************************************************
 Function Name      : IsColorAttachmentTwiddled
 Inputs             : psFrameBuffer
 Outputs            : -
 Returns            : IMG_TRUE if the the color attachment of the given framebuffer object
                      is twiddled. IMG_FALSE otherwise.
 Description        : -
************************************************************************************/
IMG_INTERNAL IMG_BOOL IsColorAttachmentTwiddled(GLES1Context *gc, GLESFrameBuffer *psFrameBuffer)
{
	GLES1FrameBufferAttachable *psColorAttachment;

	if(psFrameBuffer)
	{
		if(psFrameBuffer == &gc->sFrameBuffer.sDefaultFrameBuffer)
		{
			if(psFrameBuffer->sDrawParams.psRenderSurface->bIsTwiddledSurface)
			{
				return IMG_TRUE;
			}
			else
			{
				return IMG_FALSE;
			}
		}
		else
		{
			psColorAttachment = psFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

			if(psColorAttachment && (GL_TEXTURE == psColorAttachment->eAttachmentType))
			{
#if defined(GLES1_EXTENSION_EGL_IMAGE)
				GLESMipMapLevel *psMipMapLevel = (GLESMipMapLevel *)psFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

				if(psMipMapLevel->psTex->psEGLImageTarget && !psMipMapLevel->psTex->psEGLImageTarget->bTwiddled)
				{
					return IMG_FALSE;
				}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

				/* The color attachment of the current framebuffer is a texture */
				/* Is its size a power-of-two? */
				GLES1_ASSERT(((GLESMipMapLevel*)psColorAttachment)->psTex);

				/* Indeed it is a power-of-two, so it is twiddled */
				return IMG_TRUE;
			}
#if defined(GLES1_EXTENSION_EGL_IMAGE)
			if(psColorAttachment && (GL_RENDERBUFFER_OES == psColorAttachment->eAttachmentType))
			{
				GLESRenderBuffer *psRenderBuffer = (GLESRenderBuffer *)psFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

				if(psRenderBuffer->psEGLImageTarget && psRenderBuffer->psEGLImageTarget->bTwiddled)
				{
					return IMG_TRUE;
				}
			}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
		}
	}

	return IMG_FALSE;
}




/***********************************************************************************
 Function Name      : GetColorAttachmentMemFormat
 Inputs             : psFrameBuffer
 Outputs            : -
 Returns            : Returns the memory format of the color attachment of the given framebuffer object
 Description        : -
************************************************************************************/
IMG_INTERNAL IMG_MEMLAYOUT GetColorAttachmentMemFormat(GLES1Context *gc, GLESFrameBuffer *psFrameBuffer)
{

	GLES1FrameBufferAttachable *psColorAttachment;

	if(psFrameBuffer)
	{
		if(psFrameBuffer == &gc->sFrameBuffer.sDefaultFrameBuffer)
		{
			if(psFrameBuffer->sDrawParams.psRenderSurface->bIsTwiddledSurface)
			{
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
			    return IMG_MEMLAYOUT_HYBRIDTWIDDLED;
#else
				return IMG_MEMLAYOUT_TWIDDLED;
#endif
			}
			else
			{
			    /* Default framebuffer is stride */
			    return IMG_MEMLAYOUT_STRIDED;
			}
		}
		else
		{
			psColorAttachment = psFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

			if(psColorAttachment && (GL_TEXTURE == psColorAttachment->eAttachmentType))
			{
				GLESTexture *psTex = ((GLESMipMapLevel *)psFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT])->psTex;

				GLES1_ASSERT(psTex);

#if defined(GLES1_EXTENSION_EGL_IMAGE)
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
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

				switch(psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK)
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
#if defined(GLES1_EXTENSION_EGL_IMAGE)
			if(psColorAttachment && (GL_RENDERBUFFER_OES == psColorAttachment->eAttachmentType))
			{
			    GLESRenderBuffer *psRenderBuffer = (GLESRenderBuffer *)psFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

				if(psRenderBuffer->psEGLImageTarget && psRenderBuffer->psEGLImageTarget->bTwiddled)
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
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
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
IMG_INTERNAL IMG_BOOL FlushRenderSurface(GLES1Context *gc, 
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
			((ui32KickFlags & GLES1_SCHEDULE_HW_LAST_IN_SCENE) != 0) ||
			((ui32KickFlags & GLES1_SCHEDULE_HW_DISCARD_SCENE) != 0))
		{
			/* If the current surface is in frame we need to kick its TA before rendering the
			surface. If the current surface is the same as the argument,
				the TA is going to be kicked anyway so we don't do it here.
			*/
			if(gc->psRenderSurface && gc->psRenderSurface->bInFrame && gc->psRenderSurface != psRenderSurface)
			{
				if(ScheduleTA(gc, gc->psRenderSurface, 0) != IMG_EGL_NO_ERROR)
				{
					PVR_DPF((PVR_DBG_ERROR,"FlushRenderSurface: ScheduleTA did not work properly on the current surface"));
					bResult = IMG_FALSE;
				}
			}
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

IMG_INTERNAL IMG_BOOL FlushAllUnflushedFBO(GLES1Context *gc, 
										 IMG_BOOL bWaitForHW)
{
	IMG_BOOL bResult = IMG_TRUE;
	EGLRenderSurface *psRenderSurface;
	GLES1Context *psFlushGC;
	GLES1SurfaceFlushList **ppsFlushList, *psFlushItem; 
	IMG_UINT32 ui32Flags = GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_IGNORE_FLUSHLIST;

	if(bWaitForHW)
	{
		ui32Flags |= GLES1_SCHEDULE_HW_WAIT_FOR_3D;
	}
	
	PVRSRVLockMutex(gc->psSharedState->hFlushListLock);

	ppsFlushList = &gc->psSharedState->psFlushList;

	while(*ppsFlushList)
	{	
		psFlushItem = *ppsFlushList;
		psRenderSurface = psFlushItem->psRenderSurface;
		psFlushGC = psFlushItem->gc;
				
		GLES1_ASSERT(psRenderSurface);

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
				
				GLES1Free(gc, psFlushItem);
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
IMG_INTERNAL IMG_BOOL FlushAttachableIfNeeded(GLES1Context *gc,
												 GLES1FrameBufferAttachable *psAttachment,
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
			PDumpTexture(gc, ((GLESMipMapLevel*)psAttachment)->psTex);
		}
#endif
		bResult = FlushRenderSurface(gc, psRenderSurface, ui32KickFlags);
	}
	return bResult;
}


/***********************************************************************************
 Function Name      : FreeFrameBuffer
 Inputs             : gc, psFrameBuffer
 Outputs            : -
 Returns            : -
 Description        : Frees all structures/data associated with a frame buffer
************************************************************************************/
static IMG_VOID FreeFrameBuffer(GLES1Context *gc, GLESFrameBuffer *psFrameBuffer)
{
	GLES1FrameBufferAttachable *psAttachment;
	IMG_UINT32                 i;

	for(i=0; i < GLES1_MAX_ATTACHMENTS; i++) 
	{
		psAttachment = psFrameBuffer->apsAttachment[i];

		/* Is a renderbuffer currently attached? */
		if (psAttachment && psAttachment->eAttachmentType == GL_RENDERBUFFER_OES) 
		{
			/* Detach currently attached renderbuffer */
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER],
				(GLES1NamedItem*)psAttachment);
		}
		else if (psAttachment && psAttachment->eAttachmentType == GL_TEXTURE) 
		{
			/* Detach currently attached texture */
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ],
				(GLES1NamedItem*)(((GLESMipMapLevel*)psAttachment)->psTex));
		}
	}

	GLES1Free(gc, psFrameBuffer);
}


/***********************************************************************************
 Function Name      : DestroyFBOAttachableRenderSurface
 Inputs             : gc, psAttachment
 Outputs            : -
 Returns            : -
 Description        : If the attachment had a render surface associated, it is destroyed,
                      the memory is freed and the pointer to the surface is reset to null.
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyFBOAttachableRenderSurface(GLES1Context *gc, GLES1FrameBufferAttachable *psAttachment)
{
	if(psAttachment->psRenderSurface)
	{
		FlushAttachableIfNeeded(gc, psAttachment, GLES1_SCHEDULE_HW_LAST_IN_SCENE|GLES1_SCHEDULE_HW_WAIT_FOR_3D);

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
			KRM_RemoveAttachmentPointReferences(&gc->psSharedState->sPDSVariantKRM, psAttachment->psRenderSurface);

			if(gc->psRenderSurface==psAttachment->psRenderSurface)
			{
				gc->psRenderSurface = IMG_NULL;
			}

			GLES1Free(gc, psAttachment->psRenderSurface);
		}

		psAttachment->psRenderSurface = IMG_NULL;

		if(GL_TEXTURE == psAttachment->eAttachmentType)
		{
			((GLESMipMapLevel*)psAttachment)->psTex->ui32NumRenderTargets--;
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
static IMG_VOID FreeRenderBuffer(GLES1Context *gc, GLESRenderBuffer *psRenderBuffer)
{
	DestroyFBOAttachableRenderSurface(gc, &psRenderBuffer->sFBAttachable);

#if defined(GLES1_EXTENSION_EGL_IMAGE)
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
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

	if(psRenderBuffer->psMemInfo)
	{
		GLES1FREEDEVICEMEM_HEAP(gc, psRenderBuffer->psMemInfo);
	}

	GLES1Free(gc, psRenderBuffer);
}

/***********************************************************************************
 Function Name      : DisposeFrameBufferObject
 Inputs             : gc, psNamedItem, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Generic frame buffer object free function; called from names.c
************************************************************************************/
static IMG_VOID DisposeFrameBufferObject(GLES1Context *gc, GLES1NamedItem* psNamedItem, IMG_BOOL bIsShutdown)
{
	GLESFrameBuffer *psFrameBuffer = (GLESFrameBuffer *)psNamedItem;

	GLES1_ASSERT(bIsShutdown || (psFrameBuffer->sNamedItem.ui32RefCount == 0));

	PVR_UNREFERENCED_PARAMETER(bIsShutdown);

	FreeFrameBuffer(gc, psFrameBuffer);
}

/***********************************************************************************
 Function Name      : DisposeRenderBuffer
 Inputs             : gc, psNamedItem, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Generic render buffer object free function; called from names.c
************************************************************************************/
static IMG_VOID DisposeRenderBuffer(GLES1Context *gc, GLES1NamedItem* psNamedItem, IMG_BOOL bIsShutdown)
{
	GLESRenderBuffer *psRenderBuffer = (GLESRenderBuffer *)psNamedItem;
	
	GLES1_ASSERT(bIsShutdown || (psRenderBuffer->sFBAttachable.sNamedItem.ui32RefCount == 0));

	PVR_UNREFERENCED_PARAMETER(bIsShutdown);

	FreeRenderBuffer(gc, psRenderBuffer);
}

/***********************************************************************************
 Function Name      : SetupRenderBufferNameArray
 Inputs             : psNamesArray
 Outputs            : -
 Returns            : -
 Description        : Sets up names array for render buffer objects.
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupRenderBufferNameArray(GLES1NamesArray *psNamesArray)
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
IMG_INTERNAL IMG_VOID SetupFrameBufferObjectNameArray(GLES1NamesArray *psNamesArray)
{
	psNamesArray->pfnFree = DisposeFrameBufferObject;
}


#if defined(GLES1_EXTENSION_FRAME_BUFFER_OBJECTS)
/***********************************************************************************
 Function Name      : CreateFrameBufferObject
 Inputs             : gc, ui32Name
 Outputs            : -
 Returns            : Pointer to new framebuffer object structure
 Description        : Creates and initialises a new framebuffer object structure.
************************************************************************************/
static GLESFrameBuffer* CreateFrameBufferObject(GLES1Context *gc, IMG_UINT32 ui32Name)
{
	GLESFrameBuffer *psFrameBuffer;

	PVR_UNREFERENCED_PARAMETER(gc);

	psFrameBuffer = (GLESFrameBuffer *) GLES1Calloc(gc, sizeof(GLESFrameBuffer));

	if (psFrameBuffer == IMG_NULL)
	{
		return IMG_NULL;
	}

	psFrameBuffer->sNamedItem.ui32Name = ui32Name;

	psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES;

	psFrameBuffer->sMode.ui32MaxViewportX = GLES1_MAX_TEXTURE_SIZE;
	psFrameBuffer->sMode.ui32MaxViewportY = GLES1_MAX_TEXTURE_SIZE;

	return psFrameBuffer;
}

/***********************************************************************************
 Function Name      : CreateRenderBufferObject
 Inputs             : gc, ui32Name
 Outputs            : -
 Returns            : Pointer to new renderbuffer object structure
 Description        : Creates and initialises a new renderbuffer object structure.
************************************************************************************/
static GLESRenderBuffer* CreateRenderBufferObject(GLES1Context *gc, IMG_UINT32 ui32Name)
{
	GLESRenderBuffer *psRenderBuffer;

	PVR_UNREFERENCED_PARAMETER(gc);

	psRenderBuffer = (GLESRenderBuffer *) GLES1Calloc(gc, sizeof(GLESRenderBuffer));

	if (psRenderBuffer == IMG_NULL)
	{
		return IMG_NULL;
	}

	((GLES1NamedItem*)psRenderBuffer)->ui32Name  = ui32Name;
	((GLES1FrameBufferAttachable*)psRenderBuffer)->psRenderSurface = IMG_NULL;
	((GLES1FrameBufferAttachable*)psRenderBuffer)->eAttachmentType = GL_RENDERBUFFER_OES;

	return psRenderBuffer;
}

#endif /* defined(GLES1_EXTENSION_FRAME_BUFFER_OBJECTS) */


/***********************************************************************************
 Function Name      : CreateFrameBufferState
 Inputs             : gc, psMode
 Outputs            : -
 Returns            : Success
 Description        : Initialises the framebuffer object state
************************************************************************************/
IMG_INTERNAL IMG_BOOL CreateFrameBufferState(GLES1Context *gc, EGLcontextMode *psMode)
{
	gc->sFrameBuffer.sDefaultFrameBuffer.eStatus =             GL_FRAMEBUFFER_COMPLETE_OES;
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
IMG_INTERNAL IMG_VOID FreeFrameBufferState(GLES1Context *gc)
{
	/* unbind any frame/render buffer objects */
	if(gc->sFrameBuffer.psActiveRenderBuffer)
	{
		NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER],
			(GLES1NamedItem*)gc->sFrameBuffer.psActiveRenderBuffer);
		gc->sFrameBuffer.psActiveRenderBuffer = IMG_NULL;
	}

	if(gc->sFrameBuffer.psActiveFrameBuffer &&
	  (gc->sFrameBuffer.psActiveFrameBuffer != &gc->sFrameBuffer.sDefaultFrameBuffer))
	{
		GLESFrameBuffer *psFrameBuffer = &gc->sFrameBuffer.sDefaultFrameBuffer;

		NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_FRAMEBUFFER],
						(GLES1NamedItem*)gc->sFrameBuffer.psActiveFrameBuffer);

		gc->sFrameBuffer.psActiveFrameBuffer = psFrameBuffer;
	}
}


/***********************************************************************************
 Function Name      : FrameBufferHasBeenModified
 Inputs             : gc, psFrameBuffer
 Outputs            : psFrameBuffer
 Returns            : -
 Description        : Set the framebuffer status as unknown.
************************************************************************************/
static IMG_VOID FrameBufferHasBeenModified(GLESFrameBuffer *psFrameBuffer)
{
	if (psFrameBuffer && (psFrameBuffer->sNamedItem.ui32Name != 0))
	{
		psFrameBuffer->eStatus = GLES1_FRAMEBUFFER_STATUS_UNKNOWN;
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
IMG_INTERNAL IMG_VOID RemoveFrameBufferAttachment(GLES1Context *gc, IMG_BOOL bIsRenderBuffer, IMG_UINT32 ui32Name)
{
	GLESFrameBuffer           *psFrameBuffer;
	GLES1FrameBufferAttachable *psAttachment;
	IMG_BOOL                   bIsFrameBufferComplete, bAttachmentRemoved;
	IMG_UINT32                 i;

	psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	/* Nothing to do if we have no currently bound framebuffer */
	if(!psFrameBuffer)
	{
		return;
	}

	bIsFrameBufferComplete = (GL_FRAMEBUFFER_COMPLETE_OES == psFrameBuffer->eStatus)? IMG_TRUE : IMG_FALSE;

	bAttachmentRemoved = IMG_FALSE;

	for(i=0; i < GLES1_MAX_ATTACHMENTS; i++) 
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
			if ((psAttachment->eAttachmentType == GL_RENDERBUFFER_OES) && 
				(((GLES1NamedItem*)psAttachment)->ui32Name == ui32Name))
			{
				/* Kick the TA if the active framebuffer was complete and the surface was pending a kick. */
				if(bIsFrameBufferComplete)
				{
					FlushAttachableIfNeeded(gc, psAttachment, 0);
				}

				/* Detach currently attached renderbuffer */
				NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER], (GLES1NamedItem*)psAttachment);

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
				GLES1NamedItem *psTex = (GLES1NamedItem*) ((GLESMipMapLevel*)psAttachment)->psTex;

				if(psTex->ui32Name == ui32Name)
				{
					/* Kick the TA if the active framebuffer was complete and the surface was pending a kick. */
					if(bIsFrameBufferComplete)
					{
						FlushAttachableIfNeeded(gc, psAttachment, 0);
					}

					/* Detach currently attached texture */
					NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ], psTex);

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
IMG_INTERNAL IMG_VOID ChangeDrawableParams(GLES1Context *gc,
										   GLESFrameBuffer *psFrameBuffer,
										   EGLDrawableParams *psReadParams,
										   EGLDrawableParams *psDrawParams)
{
	IMG_BOOL bPreviousYFlip = (gc->psDrawParams->eRotationAngle == PVRSRV_FLIP_Y) ? IMG_TRUE : IMG_FALSE;
	IMG_BOOL bYFlip;
	IMG_UINT32 ui32MaxFBOStencilVal;

	if (GLES1_FRAMEBUFFER_STATUS_UNKNOWN == psFrameBuffer->eStatus)
	{
		gc->psRenderSurface = IMG_NULL;

		return;
	}

	ui32MaxFBOStencilVal = (1 << gc->sFrameBuffer.psActiveFrameBuffer->sMode.ui32StencilBits) - 1;
		
	if(gc->sState.sStencil.ui32MaxFBOStencilVal != ui32MaxFBOStencilVal)
	{
		gc->sState.sStencil.ui32MaxFBOStencilVal = ui32MaxFBOStencilVal;

		/* Update stencil masks */
		gc->sState.sStencil.ui32Stencil =  (gc->sState.sStencil.ui32Stencil & (EURASIA_ISPC_SWMASK_CLRMSK & EURASIA_ISPC_SCMPMASK_CLRMSK));
		gc->sState.sStencil.ui32Stencil |= (gc->sState.sStencil.ui32StencilWriteMaskIn & ui32MaxFBOStencilVal)<<EURASIA_ISPC_SWMASK_SHIFT;
		gc->sState.sStencil.ui32Stencil |= (gc->sState.sStencil.ui32StencilCompareMaskIn & ui32MaxFBOStencilVal)<<EURASIA_ISPC_SCMPMASK_SHIFT;

		/* Updates stencil reference values */
		gc->sState.sStencil.ui32StencilRef = ((IMG_UINT32)Clampi(gc->sState.sStencil.i32StencilRefIn, 0, (IMG_INT32)ui32MaxFBOStencilVal)) << EURASIA_ISPA_SREF_SHIFT;

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
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
		/* Setup pds/usse buffer to current render surface */
		gc->apsBuffers[CBUF_TYPE_PDS_FRAG_BUFFER] = &gc->psRenderSurface->sPDSBuffer;
		gc->apsBuffers[CBUF_TYPE_USSE_FRAG_BUFFER] = &gc->psRenderSurface->sUSSEBuffer;
	}

	bYFlip = (gc->psDrawParams->eRotationAngle == PVRSRV_FLIP_Y) ? IMG_TRUE : IMG_FALSE;

	if(bYFlip != bPreviousYFlip)
	{
		/* Ensure render state is validated in case y-invert has changed */
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
	}


	/* Change of drawable params can cause MTE / ISP state to need re-emission */
	gc->ui32EmitMask |= (GLES1_EMITSTATE_MTE_STATE_ISP | GLES1_EMITSTATE_MTE_STATE_REGION_CLIP);

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
 Outputs            : 
 Returns            : IMG_TRUE on success, IMG_FALSE on failure
 Description        : Sets up the color buffer meminfo for a framebuffer
************************************************************************************/

static IMG_BOOL SetupFrameBufferColorAttachment(GLES1Context *gc, GLESFrameBuffer *psFrameBuffer, IMG_UINT32 ui32MipmapOffset)
{
	#if defined(GLES2_EXTENSION_EGL_IMAGE)
	EGLImage *psEGLImageTarget = IMG_NULL;
	#endif
	EGLRenderSurface *psFBORenderSurface;

	if(psFrameBuffer->sMode.ui32ColorBits)
	{
		PVRSRV_CLIENT_SYNC_INFO	*psOverrideSyncInfo = IMG_NULL;

		GLES1FrameBufferAttachable *psAttachment = psFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

		GLES1_ASSERT(psAttachment);

		if(psAttachment->eAttachmentType == GL_RENDERBUFFER_OES)
		{
			GLESRenderBuffer *psRenderBuffer = (GLESRenderBuffer*)psAttachment;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
			if(psRenderBuffer->psEGLImageTarget)
			{
				psEGLImageTarget = psRenderBuffer->psEGLImageTarget;

				/* Use EGLImage sync info */
				psOverrideSyncInfo = psEGLImageTarget->psMemInfo->psClientSyncInfo;
			}
			else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
			{
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psRenderBuffer->psMemInfo->pvLinAddr;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psRenderBuffer->psMemInfo->sDevVAddr.uiAddr;
			}
		}
		else
		{
			GLESTexture *psTex = ((GLESMipMapLevel*)psAttachment)->psTex;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
			if(psTex->psEGLImageTarget)
			{
				psEGLImageTarget = psTex->psEGLImageTarget;

				/* Use EGLImage sync info */
				psOverrideSyncInfo = psEGLImageTarget->psMemInfo->psClientSyncInfo;
			}
			else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
			{
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psTex->psMemInfo->pvLinAddr;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psTex->psMemInfo->sDevVAddr.uiAddr;
			
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = (IMG_UINT8*)psFrameBuffer->sDrawParams.pvLinSurfaceAddress + ui32MipmapOffset;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress += ui32MipmapOffset;
				/* Use texture sync info */
				psOverrideSyncInfo = psTex->psMemInfo->psClientSyncInfo;
			}
		}

#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psEGLImageTarget)
		{
			psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psEGLImageTarget->pvLinSurfaceAddress;
			psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psEGLImageTarget->ui32HWSurfaceAddress;
			psFrameBuffer->sDrawParams.ui32Stride = psEGLImageTarget->ui32Stride;
		}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

		psFrameBuffer->sDrawParams.psRenderSurface      = psAttachment->psRenderSurface;
		psFrameBuffer->sDrawParams.ui32AccumHWAddress = psFrameBuffer->sDrawParams.ui32HWSurfaceAddress;

		/* If there is a render surface at this point, reuse it */
		if(!psAttachment->psRenderSurface)
		{
			/* Else, allocate a new render surface */
			psAttachment->psRenderSurface = GLES1Calloc(gc, sizeof(EGLRenderSurface));
			
			if(!psAttachment->psRenderSurface)
			{
CouldNotCreateRenderSurface:

				PVR_DPF((PVR_DBG_ERROR,"ComputeFrameBufferCompleteness: Can't create render surface."));

				psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED_OES;

				return IMG_FALSE;
			}

			psFrameBuffer->sDrawParams.psRenderSurface = psAttachment->psRenderSurface;

			if(!KEGLCreateRenderSurface(gc->psSysContext, 
										&psFrameBuffer->sDrawParams, 
										IMG_FALSE, 
										IMG_FALSE,
										IMG_FALSE,
										psFrameBuffer->sDrawParams.psRenderSurface))
			{
				GLES1Free(gc, psAttachment->psRenderSurface);

				psAttachment->psRenderSurface = 0;

				goto CouldNotCreateRenderSurface;
			}

			psAttachment->psRenderSurface->ui32FBOAttachmentCount = 1;

			if(GL_TEXTURE == psAttachment->eAttachmentType)
			{
				((GLESMipMapLevel*)psAttachment)->psTex->ui32NumRenderTargets++;
			}
		}

		psFBORenderSurface = psAttachment->psRenderSurface;

		/*
		** Use the sync info of the texture attachement or EGL Image
		** so that we can wait for HW texture uploads correctly
		*/
		if(psOverrideSyncInfo)
		{
			psFBORenderSurface->psSyncInfo = psOverrideSyncInfo;
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
static IMG_VOID SetupFrameBufferZLS(GLES1Context *gc, GLESFrameBuffer *psFrameBuffer)
{
	if(psFrameBuffer->sMode.ui32DepthBits || psFrameBuffer->sMode.ui32StencilBits)
	{
		GLES1FrameBufferAttachable *psDepthAttachment, *psStencilAttachment;
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


		psDepthAttachment   = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES1_DEPTH_ATTACHMENT];
		psStencilAttachment = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES1_STENCIL_ATTACHMENT];

		if(psDepthAttachment)
		{
			GLESRenderBuffer *psDepthRenderBuffer = (GLESRenderBuffer*)psDepthAttachment;
			PVRSRV_CLIENT_MEM_INFO *psDepthMemInfo;

			GLES1_ASSERT(psDepthAttachment->eAttachmentType == GL_RENDERBUFFER_OES);

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

		if(psStencilAttachment)
		{
			GLESRenderBuffer *psStencilRenderBuffer = (GLESRenderBuffer *)psStencilAttachment;
			PVRSRV_CLIENT_MEM_INFO *psStencilMemInfo;

			GLES1_ASSERT(psStencilAttachment->eAttachmentType == GL_RENDERBUFFER_OES);

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

			ui32ISPStencilLoadBase  =  psStencilMemInfo->sDevVAddr.uiAddr;
			ui32ISPStencilStoreBase =  psStencilMemInfo->sDevVAddr.uiAddr;

			if(!gc->sAppHints.bFBODepthDiscard)
			{
				ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_FORCEZSTORE_MASK |
									 EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK;
			}
		}

		if(ui32ISPZLSControl)
		{
			ui32ISPZLSControl |= ((ui32Extent - 1) << EUR_CR_ISP_ZLSCTL_ZLSEXTENT_SHIFT) | 
								EUR_CR_ISP_ZLSCTL_LOADTILED_MASK |
								EUR_CR_ISP_ZLSCTL_STORETILED_MASK |
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
static IMG_VOID ComputeFrameBufferCompleteness(GLES1Context *gc)
{
	GLES1FrameBufferAttachable  *psAttachment;
	GLESFrameBuffer *psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;
	PVRSRV_PIXEL_FORMAT ePixelFormat       = PVRSRV_PIXEL_FORMAT_UNKNOWN;
	EGLRenderSurface *psFBORenderSurface;
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
#if defined(GLES1_EXTENSION_EGL_IMAGE)
	EGLImage *psEGLImageTarget = IMG_NULL;
#endif

	GLES1_ASSERT(GLES1_FRAMEBUFFER_STATUS_UNKNOWN == psFrameBuffer->eStatus);

	/* Check color attachment completeness */
	psAttachment = psFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

	if (psAttachment)
	{
		psAttachment->bGhosted = IMG_FALSE;
	}

	if(psAttachment && (GL_TEXTURE == psAttachment->eAttachmentType))
	{
		GLESMipMapLevel *psLevel  = (GLESMipMapLevel*)psAttachment;
		GLESTexture     *psTex    = psLevel->psTex;

		if(!psLevel->psTexFormat)
		{
			/* The texture has no format. It means that glTexImage returned an error (the app probably ignored it).
			   That makes the texture invalid and unacceptable as an attachment.
			*/
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
			return;
		}

		ui32Width  = psLevel->ui32Width;
		ui32Height = psLevel->ui32Height;

		if((ui32Width == 0) || (ui32Height == 0))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
			return;
		}

		switch(psLevel->psTexFormat->ePixelFormat)
		{
			case PVRSRV_PIXEL_FORMAT_ABGR8888:
			case PVRSRV_PIXEL_FORMAT_ARGB8888:
			{
				ui32RedBits   = 8;
				ui32GreenBits = 8;
				ui32BlueBits  = 8;

				if(psLevel->psTexFormat->ui32BaseFormatIndex == GLES1_RGBA_TEX_INDEX)
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

				psFrameBuffer->eStatus =  GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;

				return;
			}
		}
				
		ePixelFormat = psLevel->psTexFormat->ePixelFormat;	

		/* Make the texture resident immediately */
		if(!SetupTextureRenderTargetControlWords(gc, psTex))
		{
			psFrameBuffer->eStatus =  GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
			return;
		}
		ui32NumAttachments++;

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		{
			IMG_UINT32 ui32Face = psLevel->ui32Level/GLES1_MAX_TEXTURE_MIPMAP_LEVELS;
			IMG_UINT32 ui32TopUSize;
			IMG_UINT32 ui32TopVSize;
			IMG_UINT32 ui32FaceOffset; 

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
			ui32TopUSize = 1U << ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
			ui32TopVSize = 1U << ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
			ui32TopUSize = 1 + ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
			ui32TopVSize = 1 + ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif
	
			ui32FaceOffset = psLevel->psTexFormat->ui32TotalBytesPerTexel *	GetMipMapOffset(psTex->ui32NumLevels, ui32TopUSize, ui32TopVSize);

			if(psTex->ui32HWFlags & GLES1_MIPMAP)
			{
				if(((psLevel->psTexFormat->ui32TotalBytesPerTexel == 1) && (ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
					(ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}

			ui32MipmapOffset = (ui32FaceOffset * ui32Face);
		}
#endif
	}
	else if(psAttachment && (GL_RENDERBUFFER_OES == psAttachment->eAttachmentType))
	{
		GLESRenderBuffer *psRenderBuffer = (GLESRenderBuffer*)psAttachment;
		
		ui32Width  = psRenderBuffer->ui32Width;
		ui32Height = psRenderBuffer->ui32Height;

		if((ui32Width == 0) || (ui32Height == 0))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
			return;
		}

		ui32RedBits   = psRenderBuffer->ui8RedSize;
		ui32GreenBits = psRenderBuffer->ui8GreenSize;
		ui32BlueBits  = psRenderBuffer->ui8BlueSize;
		ui32AlphaBits = psRenderBuffer->ui8AlphaSize;

		/* Can't have a depth/stencil renderbuffer attached to the color attachment */
		switch(psRenderBuffer->eRequestedFormat)
		{
			case GL_RGB565_OES:
			{
				ui32BufferBits = 16;
				ePixelFormat = PVRSRV_PIXEL_FORMAT_RGB565;
				break;
			}
			case GL_RGBA4_OES:
			{
				ui32BufferBits = 16;
				ePixelFormat = PVRSRV_PIXEL_FORMAT_ARGB4444;
				break;
			}
			case GL_RGB5_A1_OES:
			{
				ui32BufferBits = 16;
				ePixelFormat = PVRSRV_PIXEL_FORMAT_ARGB1555;
				break;
			}
#if defined(GLES1_EXTENSION_RGB8_RGBA8)
			case GL_RGBA8_OES:
			{
				ui32BufferBits = 32;
				ePixelFormat = PVRSRV_PIXEL_FORMAT_ABGR8888;
				break;
			}
			case GL_RGB8_OES:
			{
				ui32BufferBits = 32;
				ePixelFormat = PVRSRV_PIXEL_FORMAT_ABGR8888;
				break;
			}
#endif /* defined(GLES1_EXTENSION_RGB8_RGBA8) */
			case GL_DEPTH_COMPONENT16_OES:
#if defined(GLES1_EXTENSION_DEPTH24)
			case GL_DEPTH_COMPONENT24_OES:
#endif /* defined(GLES1_EXTENSION_DEPTH24) */
#if defined(GLES1_EXTENSION_STENCIL8)
			case GL_STENCIL_INDEX8_OES:
#endif /* defined(GLES1_EXTENSION_STENCIL8) */
			{
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
				return;
			}
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"GetFrameBufferCompleteness: format mismatch with RenderbufferStorage"));
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED_OES;
				return;
			}
		}

		ui32NumAttachments++;
	}
	else
	{
		/* No colour attachment - unsupported */
		psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED_OES;

		return;
	}

	/* Check depth attachment completeness */
	psAttachment = psFrameBuffer->apsAttachment[GLES1_DEPTH_ATTACHMENT];

	if (psAttachment)
	{
		psAttachment->bGhosted = IMG_FALSE;
	}

	if(psAttachment && (GL_TEXTURE == psAttachment->eAttachmentType))
	{
		/* Don't support depth format textures - so must be incomplete */
		psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
		return;
	}
	else if(psAttachment && (psAttachment->eAttachmentType == GL_RENDERBUFFER_OES))
	{
		GLESRenderBuffer *psRenderBuffer = (GLESRenderBuffer*)psAttachment;
		
		/* Depth attachment dimensions must match color attachment,  if present */
		if(ui32NumAttachments && ((ui32Width != psRenderBuffer->ui32Width) ||
								(ui32Height != psRenderBuffer->ui32Height)))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES;
			return;
		}

		ui32Width     = psRenderBuffer->ui32Width;
		ui32Height    = psRenderBuffer->ui32Height;
		ui32DepthBits = psRenderBuffer->ui8DepthSize;

		/* Can't have a color/stencil renderbuffer attached to the depth attachment */
		switch(psRenderBuffer->eRequestedFormat)
		{
			case GL_DEPTH_COMPONENT16_OES:	
#if defined(GLES1_EXTENSION_DEPTH24)
			case GL_DEPTH_COMPONENT24_OES:	
#endif /* defined(GLES1_EXTENSION_DEPTH24) */
			{
				break;
			}	
			case GL_RGB565_OES:
			case GL_RGBA4_OES:
			case GL_RGB5_A1_OES:
#if defined(GLES1_EXTENSION_RGB8_RGBA8)
			case GL_RGBA8_OES:
			case GL_RGB8_OES:
#endif /* defined(GLES1_EXTENSION_RGB8_RGBA8) */
#if defined(GLES1_EXTENSION_STENCIL8)
			case GL_STENCIL_INDEX8_OES:
#endif /* defined(GLES1_EXTENSION_STENCIL8) */
			{
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
				return;
			}
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"GetFrameBufferCompleteness: format mismatch with RenderbufferStorage"));
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED_OES;
				return;
			}
		}

		if((ui32Width == 0) || (ui32Height == 0))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
			return;
		}

		ui32NumAttachments++;
	}

	/* Check stencil attachment completeness */
	psAttachment = psFrameBuffer->apsAttachment[GLES1_STENCIL_ATTACHMENT];

	if (psAttachment)
	{
		psAttachment->bGhosted = IMG_FALSE;
	}

	if(psAttachment && (GL_TEXTURE == psAttachment->eAttachmentType))
	{
		/* Don't support stencil format textures - so must be incomplete */
		psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
		return;
	}
	else if(psAttachment && (GL_RENDERBUFFER_OES == psAttachment->eAttachmentType))
	{
		GLESRenderBuffer *psRenderBuffer = (GLESRenderBuffer*)psAttachment;
		
		/* Stencil attachment dimensions must match color/depth attachments, 
		   if present 
		 */

		if(ui32NumAttachments && ((ui32Width != psRenderBuffer->ui32Width) ||
								(ui32Height != psRenderBuffer->ui32Height)))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES;
			return;
		}

		ui32Width       = psRenderBuffer->ui32Width;
		ui32Height      = psRenderBuffer->ui32Height;
		ui32StencilBits = psRenderBuffer->ui8StencilSize;

		/* Can't have a color/depth renderbuffer attached to the stencil attachment */
		switch(psRenderBuffer->eRequestedFormat)
		{
#if defined(GLES1_EXTENSION_STENCIL8)
			case GL_STENCIL_INDEX8_OES:
#endif /* defined(GLES1_EXTENSION_STENCIL8) */
			{
				break;
			}	
			case GL_DEPTH_COMPONENT16_OES:	
#if defined(GLES1_EXTENSION_DEPTH24)
			case GL_DEPTH_COMPONENT24_OES:	
#endif /* defined(GLES1_EXTENSION_DEPTH24) */
			case GL_RGB565_OES:
			case GL_RGBA4_OES:
			case GL_RGB5_A1_OES:
#if defined(GLES1_EXTENSION_RGB8_RGBA8)
			case GL_RGBA8_OES:
			case GL_RGB8_OES:
#endif /* defined(GLES1_EXTENSION_RGB8_RGBA8) */
			{
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
				return;
			}	
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"GetFrameBufferCompleteness: format mismatch with RenderbufferStorage"));
				psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED_OES;
				return;
			}
		}

		if((ui32Width == 0) || (ui32Height == 0))
		{
			psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES;
			return;
		}

		ui32NumAttachments++;
	}

	if(ui32NumAttachments == 0)
	{
		psFrameBuffer->eStatus = GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES;
		return;
	}

	psFrameBuffer->sMode.ui32AlphaBits        = ui32AlphaBits;
	psFrameBuffer->sMode.ui32RedBits          = ui32RedBits;
	psFrameBuffer->sMode.ui32GreenBits        = ui32GreenBits;
	psFrameBuffer->sMode.ui32BlueBits         = ui32BlueBits;
	psFrameBuffer->sMode.ui32ColorBits        = ui32AlphaBits + ui32RedBits + ui32GreenBits + ui32BlueBits;
	psFrameBuffer->sMode.ui32DepthBits        = ui32DepthBits;
	psFrameBuffer->sMode.ui32StencilBits      = ui32StencilBits;

	psFrameBuffer->sDrawParams.eRotationAngle = PVRSRV_FLIP_Y;
	psFrameBuffer->sDrawParams.ePixelFormat   = ePixelFormat;
	psFrameBuffer->sDrawParams.ui32Width      = ui32Width;
	psFrameBuffer->sDrawParams.ui32Height     = ui32Height;

	psFBORenderSurface = IMG_NULL;

	if(psFrameBuffer->sMode.ui32ColorBits)
	{
		PVRSRV_CLIENT_SYNC_INFO	*psOverrideSyncInfo = IMG_NULL;

		psAttachment = psFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

		GLES1_ASSERT(psAttachment);

		if(psAttachment->eAttachmentType == GL_RENDERBUFFER_OES)
		{
			GLESRenderBuffer *psRenderBuffer = (GLESRenderBuffer*)psAttachment;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
			if(psRenderBuffer->psEGLImageTarget)
			{
				psEGLImageTarget = psRenderBuffer->psEGLImageTarget;

				/* Use EGLImage sync info */
				psOverrideSyncInfo = psEGLImageTarget->psMemInfo->psClientSyncInfo;
			}
			else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
			{
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psRenderBuffer->psMemInfo->pvLinAddr;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psRenderBuffer->psMemInfo->sDevVAddr.uiAddr;
			}
		}
		else
		{
			GLESTexture *psTex = ((GLESMipMapLevel*)psAttachment)->psTex;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
			if(psTex->psEGLImageTarget)
			{
				psEGLImageTarget = psTex->psEGLImageTarget;

				/* Use EGLImage sync info */
				psOverrideSyncInfo = psEGLImageTarget->psMemInfo->psClientSyncInfo;
			}
			else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
			{
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psTex->psMemInfo->pvLinAddr;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psTex->psMemInfo->sDevVAddr.uiAddr;
			
				psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = (IMG_UINT8*)psFrameBuffer->sDrawParams.pvLinSurfaceAddress + ui32MipmapOffset;
				psFrameBuffer->sDrawParams.ui32HWSurfaceAddress += ui32MipmapOffset;

				/* Use texture sync info */
				psOverrideSyncInfo = psTex->psMemInfo->psClientSyncInfo;
			}
		}

#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psEGLImageTarget)
		{
			psFrameBuffer->sDrawParams.pvLinSurfaceAddress  = psEGLImageTarget->pvLinSurfaceAddress;
			psFrameBuffer->sDrawParams.ui32HWSurfaceAddress = psEGLImageTarget->ui32HWSurfaceAddress;
			psFrameBuffer->sDrawParams.ui32Stride = psEGLImageTarget->ui32Stride;
		}
		else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
		{
			if(IsColorAttachmentTwiddled(gc, psFrameBuffer))
			{
					psFrameBuffer->sDrawParams.ui32Stride = ui32Width;
			}
			else
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
			}

			psFrameBuffer->sDrawParams.ui32Stride          *= (ui32BufferBits >> 3);
		}

		psFrameBuffer->sDrawParams.psRenderSurface      = psAttachment->psRenderSurface;
		psFrameBuffer->sDrawParams.ui32AccumHWAddress	= psFrameBuffer->sDrawParams.ui32HWSurfaceAddress;

		/* If there is a render surface at this point, reuse it */
		if(!psAttachment->psRenderSurface)
		{
			/* Else, allocate a new render surface */
			psAttachment->psRenderSurface = GLES1Calloc(gc, sizeof(EGLRenderSurface));
			
			if(!psAttachment->psRenderSurface)
			{
CouldNotCreateRenderSurface:

				PVR_DPF((PVR_DBG_ERROR,"ComputeFrameBufferCompleteness: Can't create render surface."));

				psFrameBuffer->eStatus = GL_FRAMEBUFFER_UNSUPPORTED_OES;

				return;
			}

			psFrameBuffer->sDrawParams.psRenderSurface = psAttachment->psRenderSurface;

			if(!KEGLCreateRenderSurface(gc->psSysContext, 
										&psFrameBuffer->sDrawParams, 
										IMG_FALSE, 
										IMG_FALSE,
										IMG_FALSE,
										psFrameBuffer->sDrawParams.psRenderSurface))
			{
				GLES1Free(gc, psAttachment->psRenderSurface);

				psAttachment->psRenderSurface = 0;

				goto CouldNotCreateRenderSurface;
			}

			psAttachment->psRenderSurface->ui32FBOAttachmentCount = 1;

			if(GL_TEXTURE == psAttachment->eAttachmentType)
			{
				((GLESMipMapLevel*)psAttachment)->psTex->ui32NumRenderTargets++;
			}
		}

		psFBORenderSurface = psAttachment->psRenderSurface;

		/*
		** Use the sync info of the texture attachement or EGL Image
		** so that we can wait for HW texture uploads correctly
		*/
		if(psOverrideSyncInfo)
		{
			psFBORenderSurface->psSyncInfo = psOverrideSyncInfo;
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


	psAttachment = psFrameBuffer->apsAttachment[GLES1_DEPTH_ATTACHMENT];

	if (psAttachment)
	{
		/* We don't support FBO with no color attachment, so we should already have an psFBORenderSurface */
		GLES1_ASSERT(psFBORenderSurface);

		if(psAttachment->psRenderSurface && psFBORenderSurface && (psAttachment->psRenderSurface!=psFBORenderSurface))
		{
			/* Destroy the attachment RS if we've got an FBO RS and a different attachment RS */
			DestroyFBOAttachableRenderSurface(gc, psAttachment);
		}

		if(!psAttachment->psRenderSurface)
		{
			/* Use the FBO RS on the attachment */
			psAttachment->psRenderSurface = psFBORenderSurface;

			/* Increment ref count */
			psAttachment->psRenderSurface->ui32FBOAttachmentCount++;
		}
		else
		{
			/* Attachment RS is the same as the FBO RS, so no need to do anything */
			GLES1_ASSERT(psAttachment->psRenderSurface==psFBORenderSurface);
		}
	}

	psAttachment = psFrameBuffer->apsAttachment[GLES1_STENCIL_ATTACHMENT];

	if (psAttachment)
	{
		/* We don't support FBO with no color attachment, so we should already have an psFBORenderSurface */
		GLES1_ASSERT(psFBORenderSurface);

		if(psAttachment->psRenderSurface && psFBORenderSurface && (psAttachment->psRenderSurface!=psFBORenderSurface))
		{
			/* Destroy the attachment RS if we've got an FBO RS and a different attachment RS */
			DestroyFBOAttachableRenderSurface(gc, psAttachment);
		}

		if(!psAttachment->psRenderSurface)
		{
			/* Use the FBO RS on the attachment */
			psAttachment->psRenderSurface = psFBORenderSurface;

			/* Increment ref count */
			psAttachment->psRenderSurface->ui32FBOAttachmentCount++;
		}
		else
		{
			/* Attachment RS is the same as the FBO RS, so no need to do anything */
			GLES1_ASSERT(psAttachment->psRenderSurface==psFBORenderSurface);
		}
	}

	psFrameBuffer->sReadParams = psFrameBuffer->sDrawParams;

	/* There is no surface for reading. We are not supporting framebuffer objects with no color attachment */
	if(!psFrameBuffer->sMode.ui32ColorBits)
	{
		psFrameBuffer->sReadParams.psRenderSurface = IMG_NULL;
	}

	SetupFrameBufferZLS(gc, psFrameBuffer);

	psFrameBuffer->eStatus = GL_FRAMEBUFFER_COMPLETE_OES;

	ChangeDrawableParams(gc, psFrameBuffer, &psFrameBuffer->sReadParams, &psFrameBuffer->sDrawParams);
}

/***********************************************************************************
 Function Name      : UpdateGhostedFrameBuffer
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Checks if any attachments to the framebuffer have been ghosted,
                      if they have update as necessary and clear the ghosted flag
************************************************************************************/
static IMG_VOID UpdateGhostedFrameBufferIfRequired(GLES1Context *gc)
{
	GLESFrameBuffer *psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;
	GLES1FrameBufferAttachable *psAttachment;
	IMG_BOOL bNeedToUpdateZLS = IMG_FALSE;

	/* Color attachment */
	psAttachment = psFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];


	if (psAttachment && psAttachment->bGhosted)
	{
		IMG_UINT32 ui32MipmapOffset = 0;
		GLESMipMapLevel *psLevel = (GLESMipMapLevel*)psAttachment;
		GLESTexture *psTex = psLevel->psTex;

		psAttachment->bGhosted = IMG_FALSE;
		//Only texture attachments can be ghosted
		GLES1_ASSERT(psAttachment->eAttachmentType == GL_TEXTURE);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		{
			IMG_UINT32 ui32Face = psLevel->ui32Level/GLES1_MAX_TEXTURE_MIPMAP_LEVELS;
			IMG_UINT32 ui32TopUSize;
			IMG_UINT32 ui32TopVSize;
			IMG_UINT32 ui32FaceOffset; 

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
			ui32TopUSize = 1U << ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
			ui32TopVSize = 1U << ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
			ui32TopUSize = 1 + ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
			ui32TopVSize = 1 + ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif
	
			ui32FaceOffset = psLevel->psTexFormat->ui32TotalBytesPerTexel *	GetMipMapOffset(psTex->ui32NumLevels, ui32TopUSize, ui32TopVSize);

			if(psTex->ui32HWFlags & GLES1_MIPMAP)
			{
				if(((psLevel->psTexFormat->ui32TotalBytesPerTexel == 1) && (ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
					(ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}

			ui32MipmapOffset = (ui32FaceOffset * ui32Face);
		}
#endif
		if (!SetupFrameBufferColorAttachment(gc, psFrameBuffer, ui32MipmapOffset))
		{
			return;
		}
	}

	/* Depth attachment */
  psAttachment = psFrameBuffer->apsAttachment[GLES1_DEPTH_ATTACHMENT];
  if (psAttachment && psAttachment->bGhosted)
  {
    psAttachment->bGhosted = IMG_FALSE;
    //Only texture attachments can be ghosted
    GLES1_ASSERT(psAttachment->eAttachmentType == GL_TEXTURE);
    bNeedToUpdateZLS = IMG_TRUE;
  }

  /* Stencil attachment */
  psAttachment = psFrameBuffer->apsAttachment[GLES1_STENCIL_ATTACHMENT];
  if (psAttachment && psAttachment->bGhosted)
  {
    psAttachment->bGhosted = IMG_FALSE;
    //Only texture attachments can be ghosted
    GLES1_ASSERT(psAttachment->eAttachmentType == GL_TEXTURE);
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
IMG_INTERNAL GLenum GetFrameBufferCompleteness(GLES1Context *gc)
{
	GLESFrameBuffer *psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	GLES1_ASSERT(psFrameBuffer);
	
	if(GLES1_FRAMEBUFFER_STATUS_UNKNOWN == psFrameBuffer->eStatus)
	{
		ComputeFrameBufferCompleteness(gc);
	}
	else
	{
		/* Checks if any attachments have been ghosted and updates accordingly */
    UpdateGhostedFrameBufferIfRequired(gc);
	}

	GLES1_ASSERT( (GL_FRAMEBUFFER_COMPLETE_OES                      == psFrameBuffer->eStatus) ||
	             (GL_FRAMEBUFFER_UNSUPPORTED_OES                   == psFrameBuffer->eStatus) ||
	             (GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_OES         == psFrameBuffer->eStatus) ||
	             (GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_OES == psFrameBuffer->eStatus) ||
	             (GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_OES         == psFrameBuffer->eStatus) );

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
static IMG_VOID NotifyFrameBuffer(GLES1Context *gc, const IMG_VOID* pvAttachment, GLES1NamedItem *psNamedItem)
{
	IMG_UINT32                 i;
	GLESFrameBuffer           *psFrameBuffer = (GLESFrameBuffer*)psNamedItem;
	const GLES1FrameBufferAttachable *psAttachment  = (const GLES1FrameBufferAttachable*)pvAttachment, *psFBAttachment;
	GLESTexture               *psTex;

	PVR_UNREFERENCED_PARAMETER(gc);

	GLES1_ASSERT(psFrameBuffer);
	GLES1_ASSERT(psAttachment);

	if(GL_RENDERBUFFER_OES == psAttachment->eAttachmentType)
	{
		for(i = 0; i < GLES1_MAX_ATTACHMENTS; ++i)
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
		GLES1_ASSERT(GL_TEXTURE == psAttachment->eAttachmentType);

		psTex = ((const GLESMipMapLevel*)psAttachment)->psTex;

		for(i = 0; i < GLES1_MAX_ATTACHMENTS; ++i)
		{
			psFBAttachment = psFrameBuffer->apsAttachment[i];

			if(psFBAttachment &&
			   (GL_TEXTURE == psFBAttachment->eAttachmentType) &&
			   (((const GLESMipMapLevel*)psFBAttachment)->psTex == psTex))
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
IMG_INTERNAL IMG_VOID FBOAttachableHasBeenModified(GLES1Context *gc, GLES1FrameBufferAttachable *psAttachment)
{
	GLES1_ASSERT(psAttachment);

	/* 1- Delete the render surface */
	DestroyFBOAttachableRenderSurface(gc, psAttachment);

	/* 2- Notify all attached framebuffers about the change */
	NamesArrayMapFunction(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_FRAMEBUFFER], NotifyFrameBuffer, (IMG_VOID*)psAttachment);
}



#if defined(GLES1_EXTENSION_FRAME_BUFFER_OBJECTS)

/***********************************************************************************
 Function Name      : glIsRenderbufferOES
 Inputs             : renderbuffer
 Outputs            : -
 Returns            : true/false
 Description        : Returns whether a named object is a renderbuffer
************************************************************************************/
GL_API_EXT GLboolean GL_APIENTRY glIsRenderbufferOES(GLuint renderbuffer)
{
	/* Semantics specified in section 6.1.15 of GL_EXT_framebuffer_object */
	GLESRenderBuffer *psRenderBuffer;
	GLES1NamesArray *psNamesArray;

	__GLES1_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsRenderbufferOES"));

	GLES1_TIME_START(GLES1_TIMES_glIsRenderbufferOES);

	if (renderbuffer==0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glIsRenderbufferOES);
		return GL_FALSE;
	}

	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER];

	psRenderBuffer = (GLESRenderBuffer *) NamedItemAddRef(psNamesArray, renderbuffer);

	if (psRenderBuffer==IMG_NULL)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glIsRenderbufferOES);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psRenderBuffer);

	GLES1_TIME_STOP(GLES1_TIMES_glIsRenderbufferOES);

	return GL_TRUE;
}

/***********************************************************************************
 Function Name      : glBindRenderbufferOES
 Inputs             : target, renderbuffer
 Outputs            : -
 Returns            : -
 Description        : Sets current render buffer object for subsequent calls.
					  Will create an internal render buffer structure. Uses name table.
************************************************************************************/
GL_API_EXT void GL_APIENTRY glBindRenderbufferOES(GLenum target, GLuint renderbuffer)
{
	/* Semantics specified in section 4.4.2.1 of GL_EXT_framebuffer_object */
	GLESRenderBuffer *psRenderBuffer = IMG_NULL;
	GLESRenderBuffer *psBoundRenderBuffer;
	GLES1NamesArray   *psNamesArray;	

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBindRenderbufferOES"));

	GLES1_TIME_START(GLES1_TIMES_glBindRenderbufferOES);

	/* Check that the target is valid */
	if(target != GL_RENDERBUFFER_OES)
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES1_TIME_STOP(GLES1_TIMES_glBindRenderbufferOES);
		return;
	}

	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER];

	/*
	** Retrieve the framebuffer object from the namesArray structure.
	*/
	if (renderbuffer) 
	{
		psRenderBuffer = (GLESRenderBuffer *) NamedItemAddRef(psNamesArray, renderbuffer);

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

				GLES1_TIME_STOP(GLES1_TIMES_glBindRenderbufferOES);

				return;
			}

			GLES1_ASSERT(IMG_NULL != psRenderBuffer);
			GLES1_ASSERT(renderbuffer == psRenderBuffer->sFBAttachable.sNamedItem.ui32Name);

			if(!InsertNamedItem(psNamesArray, (GLES1NamedItem*)psRenderBuffer))
			{
				FreeRenderBuffer(gc, psRenderBuffer);

				SetError(gc, GL_OUT_OF_MEMORY);

				GLES1_TIME_STOP(GLES1_TIMES_glBindRenderbufferOES);

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
			GLES1_ASSERT(renderbuffer == psRenderBuffer->sFBAttachable.sNamedItem.ui32Name);
		}
	}

	/*
	** Release that is being unbound.
	*/
	psBoundRenderBuffer = gc->sFrameBuffer.psActiveRenderBuffer;

	if (psBoundRenderBuffer && (((GLES1NamedItem*)psBoundRenderBuffer)->ui32Name != 0)) 
	{
		NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psBoundRenderBuffer);
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

	GLES1_TIME_STOP(GLES1_TIMES_glBindRenderbufferOES);
}

/***********************************************************************************
 Function Name      : glDeleteRenderbuffersOES
 Inputs             : n, renderbuffers
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Deletes a list of n framebuffers.
					  Deletes internal framebuffer structures for named framebuffer objects
					  and if they were currently bound, binds 0 (default setting)
************************************************************************************/
GL_API_EXT void GL_APIENTRY glDeleteRenderbuffersOES(GLsizei n, const GLuint *renderbuffers)
{
	/* Semantics specified in section 4.4.2.1 of GL_EXT_framebuffer_object */
	GLES1NamesArray *psNamesArray;
	GLESRenderBuffer *psRenderBuffer;

	IMG_INT32 i;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDeleteRenderbuffersOES"));

	GLES1_TIME_START(GLES1_TIMES_glDeleteRenderbuffersOES);

	if(!renderbuffers)
	{
		/* The application developer is having a bad day */
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteRenderbuffersOES);
		return;
	}

	if (n < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteRenderbuffersOES);
		return;
	}
	else if (n == 0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteRenderbuffersOES);
		return;
	}

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER];

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

			if (psRenderBuffer && ((GLES1NamedItem*)psRenderBuffer)->ui32Name == renderbuffers[i]) 
			{
				NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psRenderBuffer);
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

	GLES1_TIME_STOP(GLES1_TIMES_glDeleteRenderbuffersOES);
}


/***********************************************************************************
 Function Name      : glGenRenderbuffersOES
 Inputs             : n
 Outputs            : renderbuffers
 Returns            : -
 Description        : ENTRYPOINT: Generates a set of n names for renderbuffer objects
************************************************************************************/
GL_API_EXT void GL_APIENTRY glGenRenderbuffersOES(GLsizei n, GLuint *renderbuffers)
{
	/* Semantics specified in section 4.4.2.1 of GL_EXT_framebuffer_object */
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGenRenderbuffersOES"));

	GLES1_TIME_START(GLES1_TIMES_glGenRenderbuffersOES);

	if (n < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
	}
	else if (n > 0 && renderbuffers)
	{
		GLES1_ASSERT(gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER] != NULL);

		NamesArrayGenNames(gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER], (IMG_UINT32)n, (IMG_UINT32 *)renderbuffers);
	}

	GLES1_TIME_STOP(GLES1_TIMES_glGenRenderbuffersOES);
}

/***********************************************************************************
 Function Name      : glRenderbufferStorageOES
 Inputs             : target, internalformat, width, height
 Outputs            : 0
 Returns            : -
 Description        : ENTRYPOINT: Defines a renderbuffer and allocates memory for it
************************************************************************************/
GL_API_EXT void GL_APIENTRY glRenderbufferStorageOES(GLenum target, GLenum internalformat,
												 GLsizei width, GLsizei height)
{
	/* Semantics specified in section 4.4.2.1 of GL_EXT_framebuffer_object */
	GLESRenderBuffer *psRenderBuffer;
	IMG_UINT32 ui32BufferSize;
	IMG_UINT8  ui8RedSize, ui8GreenSize, ui8BlueSize, ui8AlphaSize, ui8DepthSize, ui8StencilSize;
	PVRSRV_ERROR eError;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glRenderbufferStorageOES"));

	GLES1_TIME_START(GLES1_TIMES_glRenderbufferStorageOES);

	/* Check that the target is valid */
	if(target != GL_RENDERBUFFER_OES)
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES1_TIME_STOP(GLES1_TIMES_glRenderbufferStorageOES);
		return;
	}

	/* According to OES_framebuffer_object, the following formats are required:
	 *     RGB565_OES, RGBA4, RGB5_A1, DEPTH_COMPONENT_16.
	 *
	 *  Besides, the following formats are optional:
	 *     RGBA8, RGB8, DEPTH_COMPONENT_24, DEPTH_COMPONENT_32, STENCIL_INDEX1_OES, 
	 *	   STENCIL_INDEX4_OES, STENCIL_INDEX8_OES
	 */
	switch(internalformat)
	{
		/* These are the mandatory formats */
		case GL_RGB565_OES:
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
		case GL_RGBA4_OES:
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
		case GL_RGB5_A1_OES:
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
		case GL_DEPTH_COMPONENT16_OES:
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
#if defined(GLES1_EXTENSION_RGB8_RGBA8)
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
#endif /* defined(GLES1_EXTENSION_RGB8_RGBA8) */
#if defined(GLES1_EXTENSION_DEPTH24)
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
#endif /* defined(GLES1_EXTENSION_DEPTH24) */
#if defined(GLES1_EXTENSION_STENCIL8)
		case GL_STENCIL_INDEX8_OES:
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
#endif /* defined(GLES1_EXTENSION_STENCIL8) */

		/* Else, the format is not supported */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			GLES1_TIME_STOP(GLES1_TIMES_glRenderbufferStorageOES);
			return;
		}
	}

	if(width < 0 || height < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES1_TIME_STOP(GLES1_TIMES_glRenderbufferStorageOES);
		return;
	}

	/* The value of GL_MAX_RENDERBUFFER_SIZE_OES is equal to GLES1_MAX_TEXTURE_SIZE */
	if(((IMG_UINT32)width > GLES1_MAX_TEXTURE_SIZE) || ((IMG_UINT32)height > GLES1_MAX_TEXTURE_SIZE))
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES1_TIME_STOP(GLES1_TIMES_glRenderbufferStorageOES);
		return;
	}

	psRenderBuffer = gc->sFrameBuffer.psActiveRenderBuffer;

	if(IMG_NULL == psRenderBuffer)
	{
		SetError(gc, GL_INVALID_OPERATION);
		GLES1_TIME_STOP(GLES1_TIMES_glRenderbufferStorageOES);
		return;
	}

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psRenderBuffer->psEGLImageSource)
	{
		/* Inform EGL that the source is being orphaned */
		KEGLUnbindImage(psRenderBuffer->psEGLImageSource->hImage);

		/* Source has been ghosted */
		psRenderBuffer->psMemInfo = IMG_NULL;
	}
	else if(psRenderBuffer->psEGLImageTarget)
	{
		ReleaseImageFromRenderbuffer(psRenderBuffer);
	}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

	/* Notify all the framebuffers attached to this renderbuffers about the change */
	FBOAttachableHasBeenModified(gc, (GLES1FrameBufferAttachable*)psRenderBuffer);

	ui32BufferSize *= ((IMG_UINT32)width  + EURASIA_TAG_TILE_SIZEX-1) & ~(EURASIA_TAG_TILE_SIZEX-1);
	ui32BufferSize *= ((IMG_UINT32)height + EURASIA_TAG_TILE_SIZEY-1) & ~(EURASIA_TAG_TILE_SIZEY-1);


	/* If the buffer already has memory allocated it is worth checking whether the
	 * new buffer size matches the old buffer size and avoid the burden of freeing the
	 * old memory and allocating a new chunk.
	 */
	if(ui32BufferSize != psRenderBuffer->ui32AllocatedBytes)
	{
		/* The size has changed. Free any memory that we had allocated earlier for this renderbuffer */
		if(psRenderBuffer->psMemInfo)
		{
			GLES1FREEDEVICEMEM_HEAP(gc, psRenderBuffer->psMemInfo);

			psRenderBuffer->psMemInfo = IMG_NULL;
		}

		/* Allocate new memory immediately */
		/* TODO: use heuristics to decide whether it is necessary to alloc external z/stencil buffers */
		if(width!=0 && height!=0)
		{
			eError = GLES1ALLOCDEVICEMEM_HEAP(gc,
				PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_MAP_GC_MMU,
				ui32BufferSize,
				EURASIA_CACHE_LINE_SIZE,
				&psRenderBuffer->psMemInfo);

			if (eError != PVRSRV_OK)
			{
				eError = GLES1ALLOCDEVICEMEM_HEAP(gc,
					PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_NO_SYNCOBJ,
					ui32BufferSize,
					EURASIA_CACHE_LINE_SIZE,
					&psRenderBuffer->psMemInfo);
			}

			if(eError != PVRSRV_OK)
			{
				SetError(gc, GL_OUT_OF_MEMORY);

				GLES1_TIME_STOP(GLES1_TIMES_glRenderbufferStorageOES);

				return;
			}
		}
	}

	psRenderBuffer->eRequestedFormat	= internalformat;
	psRenderBuffer->ui32Width			= (IMG_UINT32)width;
	psRenderBuffer->ui32Height			= (IMG_UINT32)height;
	psRenderBuffer->ui8RedSize			= ui8RedSize;
	psRenderBuffer->ui8GreenSize		= ui8GreenSize;
	psRenderBuffer->ui8BlueSize			= ui8BlueSize;
	psRenderBuffer->ui8AlphaSize		= ui8AlphaSize;
	psRenderBuffer->ui8DepthSize		= ui8DepthSize;
	psRenderBuffer->ui8StencilSize		= ui8StencilSize;
	psRenderBuffer->ui32AllocatedBytes	= ui32BufferSize;
	psRenderBuffer->bInitialised		= IMG_FALSE;

	GLES1_TIME_STOP(GLES1_TIMES_glRenderbufferStorageOES);
}


#if defined(GLES1_EXTENSION_EGL_IMAGE)
/***********************************************************************************
 Function Name      : SetupRenderbufferFromEGLImage
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_BOOL SetupRenderbufferFromEGLImage(GLES1Context *gc, GLESRenderBuffer *psRenderBuffer)
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
			internalformat = GL_RGB565_OES;
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
			internalformat = GL_RGBA4_OES;
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
			internalformat = GL_RGB5_A1_OES;
			break;
		}
#if defined(GLES1_EXTENSION_RGB8_RGBA8)
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
#endif /* defined(GLES1_EXTENSION_RGB8_RGBA8) */
		default:
		{
			PVR_DPF((PVR_DBG_WARNING,"SetupRenderbufferFromEGLImage: Unsupported pixel format"));

			return IMG_FALSE;
		}
	}

	/* Notify all the framebuffers attached to this renderbuffers about the change */
	FBOAttachableHasBeenModified(gc, (GLES1FrameBufferAttachable*)psRenderBuffer);

	/* Free any memory that we had allocated earlier for this renderbuffer */
	if(psRenderBuffer->psMemInfo)
	{
		GLES1FREEDEVICEMEM_HEAP(gc, psRenderBuffer->psMemInfo);

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
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */


/***********************************************************************************
 Function Name      : glGetRenderbufferParameterivOES
 Inputs             : target, pname
 Outputs            : params
 Returns            : -
 Description        : Returns information about the currently bound renderbuffer
************************************************************************************/
GL_API_EXT void GL_APIENTRY glGetRenderbufferParameterivOES(GLenum target, GLenum pname, GLint* params)
{
	/* Semantics specified in "Additions to Chapter 6" of GL_EXT_framebuffer_object */
	GLESRenderBuffer *psRenderBuffer;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetRenderbufferParameterivOES"));

	GLES1_TIME_START(GLES1_TIMES_glGetRenderbufferParameterivOES);

	if(!params)
	{
		/* The app developer is having a bad day */
		GLES1_TIME_STOP(GLES1_TIMES_glGetRenderbufferParameterivOES);

		return;
	}

	/* Check that the target is valid and that there's an active renderbuffer */
	if(GL_RENDERBUFFER_OES != target)
	{
		SetError(gc, GL_INVALID_ENUM);

		GLES1_TIME_STOP(GLES1_TIMES_glGetRenderbufferParameterivOES);

		return;
	}

	psRenderBuffer = gc->sFrameBuffer.psActiveRenderBuffer;

	if(IMG_NULL == psRenderBuffer)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES1_TIME_STOP(GLES1_TIMES_glGetRenderbufferParameterivOES);

		return;
	}

	switch(pname)
	{
		case GL_RENDERBUFFER_WIDTH_OES:
		{
			*params = (GLint)psRenderBuffer->ui32Width;

			break;
		}
		case GL_RENDERBUFFER_HEIGHT_OES:
		{
			*params = (GLint)psRenderBuffer->ui32Height;

			break;
		}
		case GL_RENDERBUFFER_INTERNAL_FORMAT_OES:
		{
			*params = (GLint)psRenderBuffer->eRequestedFormat;

			break;
		}
		case GL_RENDERBUFFER_RED_SIZE_OES:
		{
			*params = (GLint)psRenderBuffer->ui8RedSize;

			break;
		}
		case GL_RENDERBUFFER_GREEN_SIZE_OES:
		{
			*params = (GLint)psRenderBuffer->ui8GreenSize;

			break;
		}
		case GL_RENDERBUFFER_BLUE_SIZE_OES:
		{
			*params = (GLint)psRenderBuffer->ui8BlueSize;

			break;
		}
		case GL_RENDERBUFFER_ALPHA_SIZE_OES:
		{
			*params = (GLint)psRenderBuffer->ui8AlphaSize;

			break;
		}
		case GL_RENDERBUFFER_DEPTH_SIZE_OES:
		{
			*params = (GLint)psRenderBuffer->ui8DepthSize;

			break;
		}
		case GL_RENDERBUFFER_STENCIL_SIZE_OES:
		{
			*params = (GLint)psRenderBuffer->ui8StencilSize;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glGetRenderbufferParameterivOES);

			return;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glGetRenderbufferParameterivOES);
}


/***********************************************************************************
 Function Name      : glIsFramebufferOES
 Inputs             : framebuffer
 Outputs            : -
 Returns            : true/false
 Description        : Returns whether a named object is a framebuffer
************************************************************************************/
GL_API_EXT GLboolean GL_APIENTRY glIsFramebufferOES(GLuint framebuffer)
{
	/* Semantics specified in section 6.1.14 of GL_EXT_framebuffer_object */
	GLESFrameBuffer *psFrameBuffer;
	GLES1NamesArray  *psNamesArray;

	__GLES1_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsFramebufferOES"));

	GLES1_TIME_START(GLES1_TIMES_glIsFramebufferOES);

	if (0 == framebuffer)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glIsFramebufferOES);
		return GL_FALSE;
	}

	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_FRAMEBUFFER]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_FRAMEBUFFER];

	psFrameBuffer = (GLESFrameBuffer *) NamedItemAddRef(psNamesArray, framebuffer);

	if (IMG_NULL == psFrameBuffer)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glIsFramebufferOES);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psFrameBuffer);

	GLES1_TIME_STOP(GLES1_TIMES_glIsFramebufferOES);

	return GL_TRUE;
}


/***********************************************************************************
 Function Name      : glBindFramebufferOES
 Inputs             : target, framebuffer
 Outputs            : -
 Returns            : -
 Description        : Sets current frame buffer object for subsequent calls.
					  Will create an internal frame buffer structure. Uses name table.
************************************************************************************/
GL_API_EXT void GL_APIENTRY glBindFramebufferOES(GLenum target, GLuint framebuffer)
{
	/* Semantics specified in section 4.4.1 of GL_EXT_framebuffer_object */
	GLESFrameBuffer *psFrameBuffer;
	GLESFrameBuffer *psBoundFrameBuffer;
	GLES1NamesArray  *psNamesArray;	
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBindFramebufferOES"));

	GLES1_TIME_START(GLES1_TIMES_glBindFramebufferOES);

	/* Check that the target is valid */
	if(target != GL_FRAMEBUFFER_OES)
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES1_TIME_STOP(GLES1_TIMES_glBindFramebufferOES);
		return;
	}

	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_FRAMEBUFFER]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_FRAMEBUFFER];

	/*
	** Retrieve the framebuffer object from the namesArray structure.
	*/
	if (framebuffer)
	{
		psFrameBuffer = (GLESFrameBuffer *) NamedItemAddRef(psNamesArray, framebuffer);

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

				GLES1_TIME_STOP(GLES1_TIMES_glBindFramebufferOES);

				return;
			}

			GLES1_ASSERT(IMG_NULL != psFrameBuffer);
			GLES1_ASSERT(framebuffer == psFrameBuffer->sNamedItem.ui32Name);

			/* FIXME: race condition could cause returning an error when we should not */
			if(!InsertNamedItem(psNamesArray, (GLES1NamedItem*)psFrameBuffer))
			{
				FreeFrameBuffer(gc, psFrameBuffer);

				SetError(gc, GL_OUT_OF_MEMORY);

				GLES1_TIME_STOP(GLES1_TIMES_glBindFramebufferOES);

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
			GLES1_ASSERT(framebuffer == psFrameBuffer->sNamedItem.ui32Name);
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
		   (psBoundFrameBuffer->eStatus == GL_FRAMEBUFFER_COMPLETE_OES))
		{
			IMG_UINT32 ui32KickFlags;

			ui32KickFlags = 0;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
			if(psBoundFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT])
			{
				if (psBoundFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT]->eAttachmentType==GL_RENDERBUFFER_OES)
				{
					GLESRenderBuffer *psRenderBuffer = (GLESRenderBuffer *)psBoundFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

					if(psRenderBuffer->psEGLImageSource || psRenderBuffer->psEGLImageTarget)
					{
						ui32KickFlags |= GLES1_SCHEDULE_HW_LAST_IN_SCENE;
					}
				}
				else if(psBoundFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT]->eAttachmentType==GL_TEXTURE)
				{
					GLESMipMapLevel *psMipMapLevel = (GLESMipMapLevel *)psBoundFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

					if(psMipMapLevel->psTex->psEGLImageSource || psMipMapLevel->psTex->psEGLImageTarget)
					{
						ui32KickFlags |= GLES1_SCHEDULE_HW_LAST_IN_SCENE;
					}
				}
			}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

			if(ScheduleTA(gc, gc->psRenderSurface, ui32KickFlags) != IMG_EGL_NO_ERROR)
			{
				PVR_DPF((PVR_DBG_ERROR,"glBindFramebufferOES: ScheduleTA did not work properly"));
			}
		}

		if(gc->psRenderSurface)
		{
			PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);
		}

		if(psBoundFrameBuffer->sNamedItem.ui32Name != 0)
		{
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psBoundFrameBuffer);
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
		for(i=0; i<GLES1_MAX_ATTACHMENTS; i++)
		{
			GLES1FrameBufferAttachable *psAttachment;

			psAttachment = psFrameBuffer->apsAttachment[i];

			if(psAttachment && (GL_TEXTURE == psAttachment->eAttachmentType))
			{
				GLESTexture *psTex;
				
				psTex = ((GLESMipMapLevel*)psAttachment)->psTex;

				if(KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
				{
					psFrameBuffer->eStatus = GLES1_FRAMEBUFFER_STATUS_UNKNOWN;
				}
			}
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glBindFramebufferOES);
}

/***********************************************************************************
 Function Name      : glDeleteFramebuffersOES
 Inputs             : n, framebuffers
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Deletes a list of n framebuffers.
					  Deletes internal framebuffer structures for named framebuffer objects
					  and if they were currently bound, binds 0 
					 (rebinds default, ie WSEGL framebuffer).
************************************************************************************/
GL_API_EXT void GL_APIENTRY glDeleteFramebuffersOES(GLsizei n, const GLuint *framebuffers)
{
	GLES1NamesArray  *psNamesArray;
	GLESFrameBuffer *psFrameBuffer;
	IMG_INT32        i;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDeleteFramebuffersOES"));

	GLES1_TIME_START(GLES1_TIMES_glDeleteFramebuffersOES);

	if(!framebuffers)
	{
		/* The application developer is having a bad day */
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteFramebuffersOES);
		return;
	}

	if (n < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteFramebuffersOES);
		return;
	}
	else if (n == 0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteFramebuffersOES);
		return;
	}

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_FRAMEBUFFER];

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

		/* Is the buffer object currently bound? */
		if (psFrameBuffer && (psFrameBuffer->sNamedItem.ui32Name == framebuffers[i]) && (framebuffers[i] != 0))
		{
			GLESFrameBuffer *psDefaultFrameBuffer = &gc->sFrameBuffer.sDefaultFrameBuffer;

			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psFrameBuffer);

			/* Setup various draw/read params state now the framebuffer has changed */
			gc->sFrameBuffer.psActiveFrameBuffer = psDefaultFrameBuffer;
			ChangeDrawableParams(gc, psDefaultFrameBuffer, &psDefaultFrameBuffer->sReadParams, &psDefaultFrameBuffer->sDrawParams);
		}
	}

	NamedItemDelRefByName(gc, psNamesArray, (IMG_UINT32)n, (const IMG_UINT32*)framebuffers);


	GLES1_TIME_STOP(GLES1_TIMES_glDeleteFramebuffersOES);
}

/***********************************************************************************
 Function Name      : glGenFramebuffersOES
 Inputs             : n
 Outputs            : framebuffers
 Returns            : -
 Description        : ENTRYPOINT: Generates a set of n names for framebuffer objects
************************************************************************************/
GL_API_EXT void GL_APIENTRY glGenFramebuffersOES(GLsizei n, GLuint *framebuffers)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGenFramebuffersOES"));

	GLES1_TIME_START(GLES1_TIMES_glGenFramebuffersOES);

	if (n < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES1_TIME_STOP(GLES1_TIMES_glGenFramebuffersOES);
		return;
	}
	else if (n == 0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glGenFramebuffersOES);
		return;
	}
	
	if (!framebuffers) 
	{
		/* The application developer is having a bad day */
		GLES1_TIME_STOP(GLES1_TIMES_glGenFramebuffersOES);
		return;
	}

	GLES1_ASSERT(gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_FRAMEBUFFER] != NULL);

	NamesArrayGenNames(gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_FRAMEBUFFER], (IMG_UINT32)n, (IMG_UINT32 *)framebuffers);

	GLES1_TIME_STOP(GLES1_TIMES_glGenFramebuffersOES);

}

/***********************************************************************************
 Function Name      : glCheckFramebufferStatusOES
 Inputs             : target
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Checks the completeness of a framebuffer
************************************************************************************/
GL_API_EXT GLenum GL_APIENTRY glCheckFramebufferStatusOES(GLenum target)
{
	GLenum eStatus;

	__GLES1_GET_CONTEXT_RETURN(0);

	PVR_DPF((PVR_DBG_CALLTRACE,"glCheckFramebufferStatusOES"));

	GLES1_TIME_START(GLES1_TIMES_glCheckFramebufferStatusOES);

	/* Check that the target is valid */
	if(target != GL_FRAMEBUFFER_OES)
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES1_TIME_STOP(GLES1_TIMES_glCheckFramebufferStatusOES);
		return 0;
	}

	eStatus = GetFrameBufferCompleteness(gc);

	GLES1_TIME_STOP(GLES1_TIMES_glCheckFramebufferStatusOES);

	return eStatus;
}

/***********************************************************************************
 Function Name      : glFramebufferTexture2DOES
 Inputs             : target, attachment, textarget, texture, level
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Attaches a texture level to a framebuffer
************************************************************************************/
GL_API_EXT void GL_APIENTRY glFramebufferTexture2DOES(GLenum target, GLenum attachment,
												  GLenum textarget, GLuint texture, GLint level)
{
	IMG_UINT32                 ui32Attachment;
	GLESFrameBuffer           *psFrameBuffer;
	GLES1FrameBufferAttachable *psOldAttachment;
	GLES1NamesArray            *psNamesArray;
	GLESTexture               *psTex;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFramebufferTexture2DOES"));

	GLES1_TIME_START(GLES1_TIMES_glFramebufferTexture2DOES);

	/* Check that the target is valid */
	if(target != GL_FRAMEBUFFER_OES)
	{
bad_enum:
		SetError(gc, GL_INVALID_ENUM);
		GLES1_TIME_STOP(GLES1_TIMES_glFramebufferTexture2DOES);
		return;
	}

	psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	if((psFrameBuffer == IMG_NULL) || (psFrameBuffer->sNamedItem.ui32Name == 0))
	{
		/* No framebuffer is bound to the target */
bad_op:
		SetError(gc, GL_INVALID_OPERATION);
		GLES1_TIME_STOP(GLES1_TIMES_glFramebufferTexture2DOES);
		return;
	}

	/* Check level param */
	if(level!=0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES1_TIME_STOP(GLES1_TIMES_glFramebufferTexture2DOES);
		return;
	}

	switch(attachment)
	{
		case GL_COLOR_ATTACHMENT0_OES:
		{
			ui32Attachment = GLES1_COLOR_ATTACHMENT;
			break;
		}
		case GL_DEPTH_ATTACHMENT_OES:
		{
			ui32Attachment = GLES1_DEPTH_ATTACHMENT;
			break;
		}
		case GL_STENCIL_ATTACHMENT_OES:
		{
			ui32Attachment = GLES1_STENCIL_ATTACHMENT;
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
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X_OES:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y_OES:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_OES:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z_OES:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_OES:
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
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

	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ];

	psOldAttachment = psFrameBuffer->apsAttachment[ui32Attachment];

	/* Kick the TA on the old surface */
	if(psOldAttachment)
	{
		/* Kick the TA on the old surface */
		FlushAttachableIfNeeded(gc, psOldAttachment, 0);

		/* Decrement the refcount */
		if(GL_TEXTURE == psOldAttachment->eAttachmentType)
		{
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ], (GLES1NamedItem*)((GLESMipMapLevel*)psOldAttachment)->psTex);
		}
		else
		{
			GLES1_ASSERT(GL_RENDERBUFFER_OES == psOldAttachment->eAttachmentType);
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER], (GLES1NamedItem*)psOldAttachment);
		}
	}

	if(texture)
	{
		IMG_UINT32 ui32Face = 0;
		/*
		** Retrieve the texture from the psNamesArray structure.
		** This will increment the refcount as we require, so do not delete the reference at the end.
		*/
		psTex = (GLESTexture *) NamedItemAddRef(psNamesArray, texture);

		if(!psTex)
		{
			goto bad_op;
		}

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		{
			ui32Face = textarget - GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + GLES1_TEXTURE_CEM_FACE_POSX;	/*PRQA S 3382*/ /* this operation can always result in correct result. */

			if(ui32Face >= GLES1_TEXTURE_CEM_FACE_MAX)
			{
				NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psTex);
				goto bad_op;
			}
		}
		else 
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
		if(textarget != GL_TEXTURE_2D)
		{
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psTex);
			goto bad_op;
		}

		psFrameBuffer->apsAttachment[ui32Attachment] = (GLES1FrameBufferAttachable*)
			&psTex->psMipLevel[ui32Face*GLES1_MAX_TEXTURE_MIPMAP_LEVELS + (IMG_UINT32)level];
	}
	else
	{
		/* Set to defaults */
		psFrameBuffer->apsAttachment[ui32Attachment] = IMG_NULL;
	}

	/* Set the status of this framebuffer as unknown */
	FrameBufferHasBeenModified(psFrameBuffer);

	GLES1_TIME_STOP(GLES1_TIMES_glFramebufferTexture2DOES);
}

/***********************************************************************************
 Function Name      : glFramebufferRenderbufferOES
 Inputs             : target, attachment, renderbuffertarget, renderbuffer
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Attaches a renderbuffer to a framebuffer
************************************************************************************/
GL_API_EXT void GL_APIENTRY glFramebufferRenderbufferOES(GLenum target, GLenum attachment,
													 GLenum renderbuffertarget, GLuint renderbuffer)
{
	IMG_UINT32                 ui32Attachment;
	GLESFrameBuffer           *psFrameBuffer;
	GLESRenderBuffer          *psRenderBuffer;
	GLES1NamesArray            *psNamesArray;
	GLES1FrameBufferAttachable *psOldAttachment;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFramebufferRenderbufferOES"));

	GLES1_TIME_START(GLES1_TIMES_glFramebufferRenderbufferOES);

	/* Check that the target is valid */
	if(target != GL_FRAMEBUFFER_OES)
	{
bad_enum:
		SetError(gc, GL_INVALID_ENUM);
		GLES1_TIME_STOP(GLES1_TIMES_glFramebufferRenderbufferOES);
		return;
	}

	psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	if((psFrameBuffer == IMG_NULL) || (psFrameBuffer->sNamedItem.ui32Name == 0))
	{
bad_op:
		SetError(gc, GL_INVALID_OPERATION);
		GLES1_TIME_STOP(GLES1_TIMES_glFramebufferRenderbufferOES);
		return;
	}

	switch(attachment)
	{
		case GL_COLOR_ATTACHMENT0_OES:
			ui32Attachment = GLES1_COLOR_ATTACHMENT;
			break;
		case GL_DEPTH_ATTACHMENT_OES:
			ui32Attachment = GLES1_DEPTH_ATTACHMENT;
			break;
		case GL_STENCIL_ATTACHMENT_OES:
			ui32Attachment = GLES1_STENCIL_ATTACHMENT;
			break;
		default:
			goto bad_enum;
	}

	if(renderbuffer != 0 && renderbuffertarget != GL_RENDERBUFFER_OES)
	{
		goto bad_op;
	}

	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER];

	psOldAttachment = psFrameBuffer->apsAttachment[ui32Attachment];

	if(psOldAttachment)
	{
		/* Kick the TA on the old surface */
		FlushAttachableIfNeeded(gc, psOldAttachment, 0);

		/* Decrement the refcount */
		if(GL_TEXTURE == psOldAttachment->eAttachmentType)
		{
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ], (GLES1NamedItem*)((GLESMipMapLevel*)psOldAttachment)->psTex);
		}
		else
		{
			GLES1_ASSERT(GL_RENDERBUFFER_OES == psOldAttachment->eAttachmentType);
			NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_RENDERBUFFER], (GLES1NamedItem*)psOldAttachment);
		}
	}

	if(renderbuffer)
	{
		/*
		** Retrieve the render buffer object from the psNamesArray structure.
		** This will increment the refcount as we require, so do not delete the reference at the end.
		*/
		psRenderBuffer = (GLESRenderBuffer *) NamedItemAddRef(psNamesArray, renderbuffer);

		if(psRenderBuffer == IMG_NULL)
		{
			goto bad_op;
		}

		psFrameBuffer->apsAttachment[ui32Attachment] = (GLES1FrameBufferAttachable*)psRenderBuffer;
	}
	else
	{
		psFrameBuffer->apsAttachment[ui32Attachment] = IMG_NULL;
	}

	/* Set the status of this framebuffer as unknown */
	FrameBufferHasBeenModified(psFrameBuffer);

	GLES1_TIME_STOP(GLES1_TIMES_glFramebufferRenderbufferOES);
}

/***********************************************************************************
 Function Name      : glGetFramebufferAttachmentParameterivOES
 Inputs             : target, attachment, pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Gets information about the attachments of a 
					  framebuffer object
************************************************************************************/
GL_API_EXT void GL_APIENTRY glGetFramebufferAttachmentParameterivOES(GLenum target, GLenum attachment,
																 GLenum pname, GLint *params)
{
	IMG_UINT32                 ui32Attachment;
	GLESFrameBuffer           *psFrameBuffer;
	GLES1FrameBufferAttachable *psAttachment;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetFramebufferAttachmentParameterivOES"));

	GLES1_TIME_START(GLES1_TIMES_glGetFramebufferAttachmentParameterivOES);

	/* Check that the target is valid */
	if(target != GL_FRAMEBUFFER_OES)
	{
bad_enum:
		SetError(gc, GL_INVALID_ENUM);
		GLES1_TIME_STOP(GLES1_TIMES_glGetFramebufferAttachmentParameterivOES);
		return;
	}

	psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;

	if((psFrameBuffer == IMG_NULL) || (psFrameBuffer->sNamedItem.ui32Name == 0))
	{
		SetError(gc, GL_INVALID_OPERATION);
		GLES1_TIME_STOP(GLES1_TIMES_glGetFramebufferAttachmentParameterivOES);
		return;
	}

	switch(attachment)
	{
		case GL_COLOR_ATTACHMENT0_OES:
			ui32Attachment = GLES1_COLOR_ATTACHMENT;
			break;
		case GL_DEPTH_ATTACHMENT_OES:
			ui32Attachment = GLES1_DEPTH_ATTACHMENT;
			break;
		case GL_STENCIL_ATTACHMENT_OES:
			ui32Attachment = GLES1_STENCIL_ATTACHMENT;
			break;
		default:
			goto bad_enum;
	}

	psAttachment = psFrameBuffer->apsAttachment[ui32Attachment];

	switch(pname)
	{
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_OES:
		{
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_OES:
		{
			if(!psAttachment)
			{
				goto bad_enum;
			}
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_OES:
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_OES:
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
		GLES1_TIME_STOP(GLES1_TIMES_glGetFramebufferAttachmentParameterivOES);
		return;
	}

	switch(pname)
	{
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_OES:
		{
			if(psAttachment)
			{
				*params = (GLint)psAttachment->eAttachmentType;
			}
			else
			{
				*params = GL_NONE_OES;
			}
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_OES:
		{
			if(psAttachment->eAttachmentType == GL_TEXTURE)
			{
				GLESMipMapLevel *psMipLevel = (GLESMipMapLevel *)psAttachment;

				*params = (GLint)psMipLevel->psTex->sNamedItem.ui32Name;
			}
			else
			{
				*params = (GLint)(((GLES1NamedItem*)psAttachment)->ui32Name);
			}
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_OES:
		{
			/* XXX: correct but ugly */
			IMG_UINT32 ui32Face = ((GLESMipMapLevel*)psAttachment)->ui32Level/GLES1_MAX_TEXTURE_MIPMAP_LEVELS;
			*params = (GLint)(((GLESMipMapLevel*)psAttachment)->ui32Level - ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS);
			break;
		}
		case GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_OES:
		{
		     GLESMipMapLevel * psMipLevel = (GLESMipMapLevel *)psAttachment;
		     
		     if (psMipLevel->psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		     {
			  IMG_UINT32 ui32Face = psMipLevel->ui32Level / GLES1_MAX_TEXTURE_MIPMAP_LEVELS;
			  *params = (GLint)(ui32Face + (IMG_UINT32)GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES);
		     }
		     else
		     {
			  *params = 0;
		     }
		     break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glGetFramebufferAttachmentParameterivOES);
}

/***********************************************************************************
 Function Name      : glGenerateMipmapOES
 Inputs             : target
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Generates a mipmap chain for a texture
************************************************************************************/

/* TODO: why not using the fancy hardware-accelerated mipmap generation? */

GL_API_EXT void GL_APIENTRY glGenerateMipmapOES(GLenum eTarget)
{
	GLenum                   eErrorCode = GL_NO_ERROR;	/* PRQA S 3197 */ /* This value is going to be used before being modified. */
	GLESTexture             *psTex;
	IMG_UINT32               ui32TexTarget, ui32Face;
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	GLESMipMapLevel         *psLevel;
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
	GLESTextureParamState   *psParams;
	/* IMG_BOOL                 bHWMipGen = IMG_FALSE; */

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE, "glGenerateMipmapOES"));

	GLES1_TIME_START(GLES1_TIMES_glGenerateMipmapOES);

	/* Get the target texture */
	switch(eTarget)
	{
		case GL_TEXTURE_2D:
		{
			ui32TexTarget = GLES1_TEXTURE_TARGET_2D;
			break;
		}
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_OES:
		{
			ui32TexTarget = GLES1_TEXTURE_TARGET_CEM;
			break;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
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


#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	psLevel = &psTex->psMipLevel[0];

	/* For cube maps, all the sides must be identical and square. See section 3.8.10 of the spec */
	if( ui32TexTarget == GLES1_TEXTURE_TARGET_CEM )
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
		for(ui32Face=1; ui32Face < GLES1_TEXTURE_CEM_FACE_MAX; ++ui32Face)
		{
			psLevel = &psTex->psMipLevel[ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS];

			if( (psLevel->ui32Width  != ui32RefWidth) &&
				(psLevel->ui32Height != ui32RefHeight) )
			{
				eErrorCode = GL_INVALID_OPERATION;
				goto ExitWithError;
			}
		}
	}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

	/* Nothing to do for 1x1 base level */
	if(psTex->psMipLevel[0].ui32Width == 1 && psTex->psMipLevel[0].ui32Height == 1)
		goto ExitNoError;

	/* First try Hardware MipGen */
	if (!gc->sAppHints.bDisableHWTQMipGen &&
		((psTex->ui32HWFlags & GLES1_COMPRESSED) == 0) &&
		HardwareMakeTextureMipmapLevels(gc, psTex, ui32TexTarget))
	{
		/* bHWMipGen = IMG_TRUE; */
		goto ExitNoError;
	}	

	/* Software MipGen */
	/* if (!bHWMipGen) */
	{
		/* Finally, do generate the mipmaps. See makemips.c */
		ui32Face= 0;

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		do
		{
			if(!MakeTextureMipmapLevels(gc, psTex, ui32Face))
			{
				/* Something is wrong in the driver, but let's blame the app. */
				eErrorCode = GL_OUT_OF_MEMORY;
				goto ExitWithError;
			}
		}
		while( (ui32TexTarget == GLES1_TEXTURE_TARGET_CEM) && (++ui32Face < GLES1_TEXTURE_CEM_FACE_MAX) );

#else /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

		if(!MakeTextureMipmapLevels(gc, psTex, ui32Face))
		{
			/* Something is wrong in the driver, but let's blame the app. */
			eErrorCode = GL_OUT_OF_MEMORY;
			goto ExitWithError;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

		goto ExitNoError;
	}

ExitWithError:
	GLES1_ASSERT (eErrorCode != GL_NO_ERROR);
	SetError(gc, eErrorCode);

	/* Exit gracefully */
ExitNoError:
	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE;

	GLES1_TIME_STOP(GLES1_TIMES_glGenerateMipmapOES);
}
#endif /* defined(GLES1_EXTENSION_FRAME_BUFFER_OBJECTS) */

/******************************************************************************
 End of file (fbo.c)
******************************************************************************/

