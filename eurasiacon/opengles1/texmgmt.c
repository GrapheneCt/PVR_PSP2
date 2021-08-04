/******************************************************************************
 * Name         : texmgmt.c
 *
 * Copyright    : 2005-2010 by Imagination Technologies Limited.
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
 * $Log: texmgmt.c $
 * .#
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include <kernel.h>

#include "context.h"

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
#include "texformat.h"
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

#if (defined(DEBUG) || defined(TIMING))
IMG_INTERNAL IMG_UINT32 ui32TextureMemCurrent = 0;
IMG_INTERNAL IMG_UINT32  ui32TextureMemHWM = 0;
#endif

#define GLES1_IS_MIPMAP(ui32Filter) \
(((ui32Filter) &~EURASIA_PDS_DOUTT0_MIPMAPCLAMP_CLRMSK) != EURASIA_PDS_DOUTT0_NOTMIPMAP)

/***********************************************************************************
 Function Name      : FlushUnflushedTextureRenders
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : Succes//Failure
 Description        : Flushes outstanding renders to a texture
************************************************************************************/
static IMG_BOOL FlushUnflushedTextureRenders(GLES1Context *gc, GLESTexture *psTex)
{
	GLES1SurfaceFlushList **ppsFlushList, *psFlushItem;
	EGLRenderSurface *psRenderSurface;
	GLESTexture *psFlushTex;
	IMG_BOOL bResult;

	PVRSRVLockMutex(gc->psSharedState->hFlushListLock);

	bResult = IMG_TRUE;

	ppsFlushList = &gc->psSharedState->psFlushList;

	/*
	** An FBO using a depth stencil texture can add two items to the list, both with the same psTex and the same psRenderSurface
	** An FBO using a colour and depth (or depth/stencil) texture can add to the list two (or three) items with different psTex and the same psRenderSurface
	**
	** 1. Find the texture in the list
	** 2. Flush the render surface associated with the texture
	** 3. Remove any further references to that render surface from the list.  This should remove any further references to this texture, or any other texure
	**    that uses the same rendersurface
	*/
	while(*ppsFlushList)
	{
		psFlushItem = *ppsFlushList;
		psRenderSurface = psFlushItem->psRenderSurface;
		psFlushTex = psFlushItem->psTex;

		GLES1_ASSERT(psRenderSurface);

		if(psTex == psFlushTex)
		{
#if defined(PDUMP)
			PDumpTexture(gc, psTex);
#endif
			if(!FlushRenderSurface(gc, psRenderSurface, (GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_IGNORE_FLUSHLIST)))
			{
				bResult = IMG_FALSE;

				ppsFlushList = &psFlushItem->psNext;
			}
			else
			{
				*ppsFlushList = psFlushItem->psNext;

				GLES1Free(gc, psFlushItem);

				/*
				 * Remove remaining references to the same render surface
				 */
				while(*ppsFlushList)
				{
					psFlushItem = *ppsFlushList;

					if(psFlushItem->psRenderSurface == psRenderSurface)
					{
						*ppsFlushList = psFlushItem->psNext;

						GLES1Free(gc, psFlushItem);
					}
					else
					{
						ppsFlushList = &psFlushItem->psNext;
					}
				}

				/* Go back to the start of the list */
				ppsFlushList = &gc->psSharedState->psFlushList;
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


#ifdef DEBUG

/***********************************************************************************
 Function Name      : IsTextureConsistentWithMipMaps
 Inputs             : psTex
 Outputs            : -
 Returns            : Whether the relationship between a texture and its mipmaps is consistent
 Description        : Another texture sanity check.
************************************************************************************/
static IMG_BOOL IsTextureConsistentWithMipMaps(const GLESTexture *psTex)
{
	const GLESMipMapLevel *psLevel;
	IMG_UINT32              i;

	if(psTex)
	{
		for(i=0; i < psTex->ui32NumLevels; ++i)
		{
			psLevel = &psTex->psMipLevel[i];

			if((psLevel->psTex != psTex) || (psLevel->ui32Level != i))
			{
				/*
				 * The pointer back to the texture is wrong or
				 * the level of this mipmap does not match.
				 */
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

#else /* !defined DEBUG */

#define IsTextureConsistentWithMipMaps(tex) (IMG_TRUE)

#endif


#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
/***********************************************************************************
 Function Name      : TexStreamUnbindBufferDevice
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : -
 Description        : Unbind buffer device from the texture stream
************************************************************************************/
static IMG_VOID TexStreamUnbindBufferDevice(GLES1Context *gc, IMG_VOID *hBufferDevice)
{
    GLES1StreamDevice *psBufferDevice = (GLES1StreamDevice *)hBufferDevice;

    /* when the stream target has been bound with the texture, 
	   and actual buffer device stream has been bound with it */
    if (psBufferDevice)
	{
	    PVRSRV_ERROR eError;
							
		/* Decrement psBufferDevice's ui32BufferRefCount */
		psBufferDevice->ui32BufferRefCount--;

		/* If no texture is bound to any buffer of this device, 
		   then unmap device memory and try to close this buffer device */
		if (psBufferDevice->ui32BufferRefCount == 0)
		{
		    IMG_UINT32 i;
		    GLES1StreamDevice *psGCBufferDevice = gc->psBufferDevice;
		    GLES1StreamDevice *psHeadGCBufferDevice = gc->psBufferDevice;
		    GLES1StreamDevice *psPrevGCBufferDevice = gc->psBufferDevice;

			/* Then the buffer class device can be unmapped and closed. */
			for(i = 0; i < psBufferDevice->sBufferInfo.ui32BufferCount; i++)
			{
				PVRSRVUnmapDeviceClassMemory (&gc->psSysContext->s3D, psBufferDevice->psBuffer[i].psBufferSurface);
			}

			if (psBufferDevice->psBuffer)
			{
				GLES1Free(gc, psBufferDevice->psBuffer);
			}

			eError = PVRSRVCloseBCDevice(gc->psSysContext->psConnection, psBufferDevice->hBufferDevice);
					
			if(eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"TexStreamUnbindBufferDevice: CloseBufferClassDevice failed"));
			}

			/* Remove this buffer device from the context's device list */
			while (psGCBufferDevice)
			{
			    if (psGCBufferDevice == psBufferDevice)
			    {
				if (psGCBufferDevice == psHeadGCBufferDevice)
				{
				    psPrevGCBufferDevice = psGCBufferDevice = psHeadGCBufferDevice = psGCBufferDevice->psNext;
				}
				else
				{
				    psPrevGCBufferDevice->psNext = psGCBufferDevice->psNext;
				    psGCBufferDevice = psGCBufferDevice->psNext;
				}
			    }
			    else
			    {
			        psPrevGCBufferDevice = psGCBufferDevice;
				psGCBufferDevice = psGCBufferDevice->psNext;

			    }
			}
			gc->psBufferDevice = psHeadGCBufferDevice;

			GLES1Free(gc, psBufferDevice->psExtTexState);


			/* Free and reinitialise this buffer device node */
			GLES1Free(gc, psBufferDevice);		
				
			psBufferDevice = IMG_NULL;
		}
	}
}
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */



/***********************************************************************************
 Function Name      : ReclaimTextureMemKRM
 Inputs             : gc, psResource
 Outputs            : -
 Returns            : -
 Description        : Reclaims the device memory of a texture. This function does not destroy
					  the resource itself, because we are managing a top level GL resource.
					  This function reads back the texture data because we have no other copy.
************************************************************************************/
static IMG_VOID ReclaimTextureMemKRM(IMG_VOID *pvContext, KRMResource *psResource)
{
	/* Note the tricky pointer arithmetic. It is necessary */
	GLESTexture     *psTex = (GLESTexture*)((IMG_UINTPTR_T)psResource -offsetof(GLESTexture, sResource));
	GLES1Context	 *gc = (GLES1Context *)pvContext;
	IMG_UINT32       ui32Lod, ui32Face;
	GLESMipMapLevel *psLevel;

	GLES1_ASSERT(psResource);

	/* If the texture has not been made resident or it has just been ghosted then there's nothing we can free */
	if(psTex->psMemInfo)
	{
		/* Here we don't really free any physical memory. We move mipmaps from device memory into host memory.
			The rationale is that in some platforms we could be running out of virtual address space, or out of NUMA
			physical space, and this should help. See makemips.c and glTexSubImage2D for similar readback code.
		*/

		/* Reclaiming of FBOs is not supported.
		 */
		if(psTex->ui32NumRenderTargets > 0)
		{
			return;
		}

		ui32Face= 0;

		do
		{
			for(ui32Lod = 0; ui32Lod < GLES1_MAX_TEXTURE_MIPMAP_LEVELS; ui32Lod++)
			{
				psLevel = &psTex->psMipLevel[ui32Lod];

				/* If the mipmap has already been loaded it must be read back */
				if(psLevel->pui8Buffer == GLES1_LOADED_LEVEL)
				{
					/* This code was copied from glTexSubImage */
					IMG_UINT32 ui32Stride     = psLevel->ui32Width * psLevel->psTexFormat->ui32TotalBytesPerTexel;
					IMG_UINT32 ui32BufferSize = ui32Stride * psLevel->ui32Height;
					IMG_UINT8  *pui8Src       = GLES1Malloc(gc, ui32BufferSize);

					if(!pui8Src)
					{
						PVR_DPF((PVR_DBG_WARNING, "FreeTextureMemKRM: Could not allocate mipmap level. Non-fatal error."));
						/* Skip the rest of the mipmaps since we can't free the texture from device mem anyway */
						return;
					}

#if (defined(DEBUG) || defined(TIMING))
					ui32TextureMemCurrent += ui32BufferSize;

					if (ui32TextureMemCurrent>ui32TextureMemHWM)
					{
						ui32TextureMemHWM = ui32TextureMemCurrent;
					}
#endif	
					/* Render to mipmap before reading it back */
					FlushAttachableIfNeeded(gc, (GLES1FrameBufferAttachable*)psLevel, 
											GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_WAIT_FOR_3D);

					ReadBackTextureData(gc, psTex, ui32Face, ui32Lod, pui8Src);

					psLevel->pui8Buffer = pui8Src;
				}
			}
		}
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		while( (psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM) && (++ui32Face < GLES1_TEXTURE_CEM_FACE_MAX) );
#else
		while(psTex->ui32TextureTarget != GLES1_TEXTURE_TARGET_2D);
#endif
		/* After all mipmaps have been read back, free the texture's device mem and mark it as non-resident */
		GLES1FREEDEVICEMEM_HEAP(gc, psTex->psMemInfo);

		psTex->psMemInfo  = IMG_NULL;
		psTex->bResidence = IMG_FALSE;
	}
}


/***********************************************************************************
 Function Name      : DestroyTextureGhostKRM
 Inputs             : gc, psResource
 Outputs            : -
 Returns            : -
 Description        : Destroys a ghosted texture.
************************************************************************************/
static IMG_VOID DestroyTextureGhostKRM(IMG_VOID *pvContext, KRMResource *psResource)
{
	/* Note the tricky pointer arithmetic. It is necessary */
	GLESGhost *psGhost = (GLESGhost*)((IMG_UINTPTR_T)psResource -offsetof(GLESGhost, sResource));
	GLES1Context *gc = (GLES1Context *)pvContext;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psGhost->hImage)
	{
		KEGLUnbindImage(psGhost->hImage);
	}
	else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	if(psGhost->hPBuffer)
	{
		KEGLSurfaceUnbind(gc->psSysContext, psGhost->hPBuffer);
	}
	else
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
	if(psGhost->hBufferDevice)
	{
		TexStreamUnbindBufferDevice(gc, psGhost->hBufferDevice);
	}
	else
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */
	{
		GLES1FREEDEVICEMEM_HEAP(gc, psGhost->psMemInfo);
	}
	
	gc->psSharedState->psTextureManager->ui32GhostMem -= psGhost->ui32Size;

	GLES1Free(gc, psGhost);
}


/***********************************************************************************
 Function Name      : CreateTextureMemory
 Inputs             : gc, psTex
 Outputs            : psTex
 Returns            : Success of memory create
 Description        : Allocates physical memory for a texture and fills in the
					  Meminfo structure.
************************************************************************************/
IMG_INTERNAL IMG_BOOL CreateTextureMemory(GLES1Context *gc, GLESTexture *psTex)
{
	GLES1TextureManager *psTexMgr         = gc->psSharedState->psTextureManager;
	IMG_UINT32 ui32TexSize;
	IMG_UINT32 ui32TexSizeSceAlign;
	IMG_UINT32 ui32TexAlign = EURASIA_CACHE_LINE_SIZE;
	IMG_UINT32 ui32BytesPerTexel = psTex->psFormat->ui32TotalBytesPerTexel;
	IMG_UINT32 ui32TexWord1      = psTex->sState.ui32StateWord1;
	IMG_UINT32 ui32TopUsize, ui32TopVsize;
	PVRSRV_ERROR eError;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
	ui32TopUsize = 1U << ((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
	ui32TopVsize = 1U << ((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
	ui32TopUsize = 1 + ((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
	ui32TopVsize = 1 + ((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

	GLES1_TIME_START(GLES1_TIMER_TEXTURE_ALLOCATE_TIME);

	if(psTex->ui32HWFlags & GLES1_COMPRESSED)
	{
		if( (psTex->psFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTC2) ||
			(psTex->psFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCII2))
		{
			ui32TexSize = ui32BytesPerTexel * 
				GetCompressedMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize, IMG_TRUE);
		}
		else
		{
			ui32TexSize = ui32BytesPerTexel * 
				GetCompressedMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize, IMG_FALSE);
		}

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		{
			if(psTex->ui32HWFlags & GLES1_MIPMAP)
			{
				if(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)
				{
					ui32TexSize = ALIGNCOUNT(ui32TexSize, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}

			ui32TexSize *= 6;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
	}
	else
	{
		ui32TexSize = ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		{
			if(psTex->ui32HWFlags & GLES1_MIPMAP)
			{
				if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
					(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
				{
					ui32TexSize = ALIGNCOUNT(ui32TexSize, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}

			ui32TexSize *= 6;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
	}

#if defined(DEBUG)
	{
		static IMG_UINT32 ui32LastFrame;
		
		if (gc->ui32FrameNum - ui32LastFrame > 100)
		{
			PVR_DPF((PVR_DBG_WARNING, "%uk ghosted", psTexMgr->ui32GhostMem/1024));

			ui32LastFrame = gc->ui32FrameNum;
		}
	}
#endif

	eError = GLES1ALLOCDEVICEMEM_HEAP(gc,
		PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MAP_GC_MMU,
		ui32TexSize,
		ui32TexAlign,
		&psTex->psMemInfo);

	if (eError != PVRSRV_OK)
	{
		eError = GLES1ALLOCDEVICEMEM_HEAP(gc,
			PVRSRV_MEM_READ | PVRSRV_MEM_WRITE,
			ui32TexSize,
			ui32TexAlign,
			&psTex->psMemInfo);
	}

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_WARNING, "CreateTextureMemory: Reaping active textures"));

		KRM_DestroyUnneededGhosts(gc, &psTexMgr->sKRM);
		KRM_ReclaimUnneededResources(gc, &psTexMgr->sKRM);

		eError = GLES1ALLOCDEVICEMEM_HEAP(gc,
			PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MAP_GC_MMU,
			ui32TexSize,
			ui32TexAlign,
			&psTex->psMemInfo);

		if (eError != PVRSRV_OK)
		{
			eError = GLES1ALLOCDEVICEMEM_HEAP(gc,
				PVRSRV_MEM_READ | PVRSRV_MEM_WRITE,
				ui32TexSize,
				ui32TexAlign,
				&psTex->psMemInfo);
		}

		if (eError != PVRSRV_OK)
		{
			/* PRQA S 3332 1 */ /* FIXME is reserved here for reference. */
			{
				/* Could do Load store render to clear active list */
				GLES1_TIME_STOP(GLES1_TIMER_TEXTURE_ALLOCATE_TIME);
				return IMG_FALSE;
			}
		}

	}

#if (defined(DEBUG) || defined(TIMING))
	ui32TextureMemCurrent += psTex->psMemInfo->uAllocSize;

	if (ui32TextureMemCurrent>ui32TextureMemHWM)
	{
		ui32TextureMemHWM = ui32TextureMemCurrent;
	}
#endif

	GLES1_TIME_STOP(GLES1_TIMER_TEXTURE_ALLOCATE_TIME);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : SetupTextureRenderTargetControlWords
 Inputs             : gc, psTex
 Outputs            : psTex->sState.sParams
 Returns            : IMG_TRUE if successfull. IMG_FALSE otherwise.
 Description        : Sets up the control words for a texture is going to be used
                      as a render target. Makes the texture resident.
					  NOTE: Do not use with other type of textures.
************************************************************************************/
IMG_INTERNAL IMG_BOOL SetupTextureRenderTargetControlWords(GLES1Context *gc, GLESTexture *psTex)
{
	GLESTextureParamState   *psParams = &psTex->sState;
	const GLESTextureFormat *psTexFormat = psTex->psMipLevel[0].psTexFormat;
	IMG_UINT32               ui32NumLevels, ui32FormatFlags = 0, ui32TexTypeSize = 0;
	IMG_UINT32               ui32StateWord1;
	IMG_UINT32               ui32WidthLog2, ui32HeightLog2;
	IMG_BOOL                 bStateWord1Changed = IMG_FALSE;

	/*                                                                */
	/* This part of the function is copied from IsTextureConsistent() */
	/*                                                                */
	GLES1_ASSERT(psTex);
	GLES1_ASSERT(psTexFormat);

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	/* Don't need to setup control words or make imagetarget resident  */
	if(psTex->psEGLImageTarget)
	{
#if defined PDUMP
		/* PDump it immediately if needed */
		PDumpTexture(gc, psTex);
#endif
		return IMG_TRUE;
	}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

	/* Compressed textures cannot be render targets */
	GLES1_ASSERT(psTexFormat->ui32TotalBytesPerTexel <= 4);

	/* NOTE: we are intentionally not messing with psTex->bLevelsConsitent */

	if(GLES1_IS_MIPMAP(psParams->ui32MinFilter))
	{
		ui32FormatFlags |= GLES1_MIPMAP;
	}

	ui32StateWord1 = asSGXPixelFormat[psTexFormat->ePixelFormat].aui32TAGControlWords[0][1];

	ui32WidthLog2  = psTex->psMipLevel[0].ui32WidthLog2;
	ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2;

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
	{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_CEM;
#else
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_CEM;
#endif
		GLES1_ASSERT(psTex->psMipLevel[0].ui32Width != 0 && psTex->psMipLevel[0].ui32Height != 0);
	}
	else
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
	{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_2D;
#else
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_2D;
#endif
	}
	
	if(ui32FormatFlags & GLES1_MIPMAP)
	{
		ui32NumLevels = MAX(ui32WidthLog2, ui32HeightLog2) + 1;
	}
	else
	{
		ui32NumLevels = 1;
	}

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
	ui32TexTypeSize |= (ui32WidthLog2  << EURASIA_PDS_DOUTT1_USIZE_SHIFT) |
	                   (ui32HeightLog2 << EURASIA_PDS_DOUTT1_VSIZE_SHIFT) ;
#else
    ui32TexTypeSize |= (((psTex->psMipLevel[0].ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  |
	                   (((psTex->psMipLevel[0].ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;
#endif

	ui32StateWord1 |= ui32TexTypeSize;

	if(psParams->ui32StateWord1 != ui32StateWord1)
	{
		bStateWord1Changed = IMG_TRUE;
	}


	if(psTex->psMemInfo)
	{
		GLES1TextureManager *psTexMgr = gc->psSharedState->psTextureManager;

		KRM_FlushUnKickedResource(&psTexMgr->sKRM, &psTex->sResource, (IMG_VOID *)gc, KickUnFlushed_ScheduleTA);

		/* If the state word 1 has changed or
		 * if the texture has changed from non-mipmap to mipmap or
		 */
		if(bStateWord1Changed || 
		   (((ui32FormatFlags & GLES1_MIPMAP) != 0) && ((psTex->ui32HWFlags & GLES1_MIPMAP) == 0)))
		{
			/* ...then we ghost it and allocate new device mem for it. */
			if(!TexMgrGhostTexture(gc, psTex))
			{
				return IMG_FALSE;
			}
		}
	}

	psParams->ui32StateWord1 = ui32StateWord1;

	psTex->ui32NumLevels = ui32NumLevels;
	psTex->ui32HWFlags   = ui32FormatFlags;
	psTex->psFormat = psTexFormat;

	/* Make the texture resident immediately */
	if(!TextureMakeResident(gc, psTex))
	{
		PVR_DPF((PVR_DBG_ERROR,"SetupTextureRenderTargetControlWords: Can't make texture resident"));

		SetError(gc, GL_OUT_OF_MEMORY);

		return IMG_FALSE;
	}

#if defined PDUMP
	/* PDump it immediately if needed */
	PDumpTexture(gc, psTex);
#endif

	return IMG_TRUE;
}



/***********************************************************************************
 Function Name      : TexMgrGhostTexture
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : Success
 Description        : UTILITY: Creates a ghost of a texture and appends it to the ghosts list.
************************************************************************************/
IMG_INTERNAL IMG_BOOL TexMgrGhostTexture(GLES1Context *gc, GLESTexture *psTex)
{
	GLES1TextureManager *psTexMgr = gc->psSharedState->psTextureManager;
	GLESGhost *psGhost;

	psGhost = GLES1Calloc(gc, sizeof(GLESGhost));

	if(!psGhost)
	{
		return IMG_FALSE;
	}

	if (psTex->ui32NumRenderTargets)
	{
		IMG_UINT ui32MipLevel;
		for (ui32MipLevel = 0; ui32MipLevel < psTex->ui32NumLevels; ui32MipLevel++)
		{
			if (psTex->psMipLevel[ui32MipLevel].sFBAttachable.psRenderSurface)
			{
				psTex->psMipLevel[ui32MipLevel].sFBAttachable.bGhosted = IMG_TRUE;
			}
		}
	}

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
	if (psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_STREAM)
	{
		psGhost->ui32Size	   = ((psTex->psBufferDevice->psBuffer[psTex->ui32BufferOffset].ui32ByteStride) * 
								  (psTex->psBufferDevice->psBuffer[psTex->ui32BufferOffset].ui32PixelHeight));
		psGhost->hBufferDevice = (IMG_VOID *)psTex->psBufferDevice;
	}
	else
#endif
	{
#if defined(GLES1_EXTENSION_EGL_IMAGE)
	    if(psTex->psEGLImageSource)
	    {
			GLES1_ASSERT(psTex->psEGLImageSource->psMemInfo==psTex->psMemInfo)

			psGhost->ui32Size	= psTex->psEGLImageSource->psMemInfo->uAllocSize;
			psGhost->hImage		= psTex->psEGLImageSource->hImage;

			psTex->psEGLImageSource = IMG_NULL;
	    }
	    else if(psTex->psEGLImageTarget)
	    {
			psGhost->ui32Size	= psTex->psEGLImageTarget->ui32Stride * psTex->psEGLImageTarget->ui32Height;
			psGhost->hImage		= psTex->psEGLImageTarget->hImage;

			psTex->psEGLImageTarget = IMG_NULL;
	    }
	    else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	    if(psTex->hPBuffer)
	    {
			psGhost->hPBuffer = psTex->hPBuffer;

			psTex->hPBuffer = IMG_NULL;
	    }
	    else
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */
	    {
			psGhost->psMemInfo	= psTex->psMemInfo;
			psGhost->ui32Size	= psTex->psMemInfo->uAllocSize;
	    }
	}

#if defined PDUMP
	/* We should dump on a mipmap level basis but hopefully this will be good enough */
	psGhost->bDumped = psTex->ui32LevelsToDump? IMG_FALSE : IMG_TRUE;
#endif

	psTex->psMemInfo = IMG_NULL;
	psTex->bResidence = IMG_FALSE;
	psTex->bHasEverBeenGhosted = IMG_TRUE;

	psTexMgr->ui32GhostMem += psGhost->ui32Size;

	KRM_GhostResource(&psTexMgr->sKRM, &psTex->sResource, &psGhost->sResource);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : TextureRemoveResident
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : -
 Description        : UTILITY: This is called by to denote the video memory version
					  of a texture is out of date, usually because new data has been
					  supplied.
************************************************************************************/
IMG_INTERNAL IMG_VOID  TextureRemoveResident(GLES1Context *gc, GLESTexture *psTex)
{
	psTex->bResidence = IMG_FALSE;

	/*
	 * by setting validate here, we do a delay load of the texture at validation time
	 */
	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE;

}


#if defined PDUMP
/***********************************************************************************
 Function Name      : PDumpTexture
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Dumps all marked levels of the texture.
                               Called by TextureMakeResident.
************************************************************************************/
IMG_INTERNAL IMG_VOID PDumpTexture(GLES1Context *gc, GLESTexture *psTex)
{
	IMG_UINT32 ui32Offset, ui32LOD;

	if(PDUMP_ISCAPTURING(gc) && psTex->psMemInfo && psTex->ui32LevelsToDump)
	{
		ui32LOD		= 0;
		ui32Offset	= 0;

		while(psTex->ui32LevelsToDump)
		{
			if(psTex->ui32LevelsToDump & 1)
			{
				IMG_UINT32 ui32MaxFace, ui32Face;

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
				if(GLES1_TEXTURE_TARGET_CEM == psTex->ui32TextureTarget)
				{
					ui32MaxFace = 6;
				}
				else
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
				{
					ui32MaxFace = 1;
				}

				for(ui32Face=0; ui32Face<ui32MaxFace; ui32Face++)
				{
					GLESMipMapLevel *psMipLevel;
					IMG_UINT32 ui32OffsetInBytes;
					IMG_UINT32 ui32TopUsize, ui32TopVsize;
					const GLESTextureFormat *psTexFmt = psTex->psFormat;
					GLESTextureParamState *psParams = &psTex->sState;
					IMG_UINT32 ui32BytesPerTexel, ui32SizeInBytes;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
					ui32TopUsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
					ui32TopVsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
					ui32TopUsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
					ui32TopVsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif
					ui32BytesPerTexel = psTexFmt->ui32TotalBytesPerTexel;

					psMipLevel = &psTex->psMipLevel[ui32LOD + (ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS)];

					ui32SizeInBytes = psMipLevel->ui32ImageSize;

					if(psTex->ui32HWFlags & GLES1_COMPRESSED)
					{
						IMG_BOOL bIs2Bpp = IMG_FALSE;

						if( (psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTC2) || 
							(psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCII2))
						{
							bIs2Bpp = IMG_TRUE;
						}

						ui32OffsetInBytes =  ui32BytesPerTexel * 
							GetCompressedMipMapOffset(ui32LOD, ui32TopUsize, ui32TopVsize, bIs2Bpp);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
						if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
						{
							IMG_UINT32 ui32FaceOffset = ui32BytesPerTexel * 
								GetCompressedMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize, bIs2Bpp);

							if(psTex->ui32HWFlags & GLES1_MIPMAP)
							{
								if(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)
								{
									ui32FaceOffset = (ui32FaceOffset + (EURASIA_TAG_CUBEMAP_FACE_ALIGN-1)) & (~(EURASIA_TAG_CUBEMAP_FACE_ALIGN-1));
								}
							}

							ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
						}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
					}
					else
					{
						ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32LOD, ui32TopUsize, ui32TopVsize);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
						if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
						{
							IMG_UINT32 ui32FaceOffset = 
								ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);

							if(psTex->ui32HWFlags & GLES1_MIPMAP)
							{
								if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
									(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
								{
									ui32FaceOffset = (ui32FaceOffset + (EURASIA_TAG_CUBEMAP_FACE_ALIGN-1)) & (~(EURASIA_TAG_CUBEMAP_FACE_ALIGN-1));
								}
							}

							ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
						}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
					}

					PDUMP_STRING((gc, "Dumping level %d of texture %d\r\n", ui32LOD, ((GLES1NamedItem*)psTex)->ui32Name));
					PDUMP_MEM(gc, psTex->psMemInfo, ui32OffsetInBytes, ui32SizeInBytes);
				}
			}

			ui32Offset += psTex->psMipLevel[ui32LOD].ui32ImageSize;

			ui32LOD++;

			psTex->ui32LevelsToDump >>= 1;
		}
	}

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	else if(PDUMP_ISCAPTURING(gc) && psTex->psEGLImageTarget)
	{
		EGLImage *psEGLImage = psTex->psEGLImageTarget;

		/* We only dump if this EGLImage has a MemInfo.
		 * GLES and VG sourced EGLImages will, but external pixmaps may not.
		 */
		if(psEGLImage->psMemInfo)
		{
			PDUMP_STRING((gc, "Dumping EGLImage\r\n"));
			PDUMP_MEM(gc, psEGLImage->psMemInfo, 0, psEGLImage->psMemInfo->uAllocSize);
		}
	}
#endif

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
	if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_STREAM)
	{
		IMG_UINT32 i;

		for(i=0; i <  gc->psBufferDevice->sBufferInfo.ui32BufferCount; i++)
		{
			PDUMP_STRING((gc, "Dumping buffer %d of stream device\r\n", i));
			PDUMP_MEM(gc,  gc->psBufferDevice->psBuffer[i].psBufferSurface, 0,  gc->psBufferDevice->psBuffer[i].psBufferSurface->uAllocSize);
		}
	}
#endif
}

#endif /* defined PDUMP */



/***********************************************************************************
 Function Name      : TextureMakeResident
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : Whether the texture was loaded to framebuffer.
 Description        : Allocates texture memory, initialises texture control words and
					  loads texture data to framebuffer. This is called when the
					  texture state is validated. Unlike an immediate mode renderer,
					  our textures need to stay resident after they have been removed
					  (until they have actually been rendered). So, if a texture is
					  marked as dirty, we may need to ghost the current state
					  (if it's still active) before loading the new texture data.
************************************************************************************/
IMG_INTERNAL IMG_BOOL TextureMakeResident(GLES1Context *gc, GLESTexture *psTex)
{
	GLESTextureParamState *psParams = &psTex->sState;
	PVRSRV_CLIENT_MEM_INFO sMemInfo  = {0};
	IMG_BOOL               bDirty    = psTex->bResidence?IMG_FALSE:IMG_TRUE;
	IMG_UINT32			   i, j, ui32MaxLevel, ui32MaxFace;
	IMG_UINT32			   ui32Level;
	GLESMipMapLevel		   *psMipLevel;

	if(psTex->ui32HWFlags & GLES1_MIPMAP)
	{
		ui32MaxLevel = GLES1_MAX_TEXTURE_MIPMAP_LEVELS;
	}
	else
	{
		ui32MaxLevel = 1;
	}

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
	{
		ui32MaxFace = 6;
	}
	else
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
	{
		ui32MaxFace = 1;
	}

	/* Nothing to do if the texture is already resident */
	if(psTex->bResidence)
	{
		return IMG_TRUE;
	}

	PVRSRVLockMutex(gc->psSharedState->hTertiaryLock);

	/* Check again, inside mutex */
	if(psTex->bResidence)
	{
		PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

		return IMG_TRUE;
	}

	if (psTex->psMemInfo &&
	    bDirty &&
	    KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
	{
		/* The texture is dirty and live. We must ghost it */
		sMemInfo = *psTex->psMemInfo;
	
		TexMgrGhostTexture(gc, psTex);
	}

	if (!psTex->psMemInfo)
	{
		if(!CreateTextureMemory(gc, psTex))
		{
			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

			return IMG_FALSE;
		}
	
		/* The texture is dirty and live we need to copy it to the new memory before uploading
		 * new data. Only copy the texture if some levels are not dirty.
		*/
		if(sMemInfo.uAllocSize && ((psTex->ui32HWFlags & GLES1_MIPMAP) != 0))
		{
			IMG_BOOL bCopyTexture = IMG_FALSE;

			for(j=0; j < ui32MaxFace; j++)
			{
				for(i=0; i < ui32MaxLevel; i++)
				{
					ui32Level = (j * GLES1_MAX_TEXTURE_MIPMAP_LEVELS) + i;
					psMipLevel = &psTex->psMipLevel[ui32Level];

					if(psMipLevel->pui8Buffer == GLES1_LOADED_LEVEL)
					{
						bCopyTexture = IMG_TRUE;
					}

					if (psMipLevel->ui32Width == 1 && psMipLevel->ui32Height == 1)
					{
						break;
					}
				}
			}

			if(bCopyTexture)
			{
				GLES1_TIME_START(GLES1_TIMER_TEXTURE_GHOST_LOAD_TIME);

				CopyTextureData(gc, psTex, 0, &sMemInfo, 0, sMemInfo.uAllocSize);

				GLES1_TIME_STOP(GLES1_TIMER_TEXTURE_GHOST_LOAD_TIME);
			}
		}

		SetupTwiddleFns(psTex);

		psParams->ui32StateWord2 = 
			(psTex->psMemInfo->sDevVAddr.uiAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
			EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

		bDirty = IMG_TRUE;
	}

	if (bDirty && psTex->psMemInfo)
	{
		GLES1_TIME_START(GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME);
	
		for(j=0; j < ui32MaxFace; j++)
		{
			for(i=0; i < ui32MaxLevel; i++)
			{
				ui32Level = (j * GLES1_MAX_TEXTURE_MIPMAP_LEVELS) + i;
				psMipLevel = &psTex->psMipLevel[ui32Level];

				if(psMipLevel->pui8Buffer != GLES1_LOADED_LEVEL && psMipLevel->pui8Buffer != 0)
				{
					TranslateLevel(gc, psTex, j, i);

#if (defined(DEBUG) || defined(TIMING))
					ui32TextureMemCurrent -= psMipLevel->ui32ImageSize;
#endif					
					GLES1Free(gc, psMipLevel->pui8Buffer);

					psMipLevel->pui8Buffer = GLES1_LOADED_LEVEL;
				}

				if (psMipLevel->ui32Width == 1 && psMipLevel->ui32Height == 1)
				{
					break;
				}
			}
		}

		GLES1_TIME_STOP(GLES1_TIMER_TEXTURE_TRANSLATE_LOAD_TIME);
	}

	psTex->bResidence = IMG_TRUE;

	GLES1_ASSERT(IsTextureConsistentWithMipMaps(psTex));

	PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : UnloadInconsistentTexture
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : Whether the texture was unloaded from framebuffer.
 Description        : UTILITY: Unloads texture data from an inconsistent data and 
					  ghosts/frees the texture.
************************************************************************************/
IMG_INTERNAL IMG_BOOL UnloadInconsistentTexture(GLES1Context *gc, GLESTexture *psTex)
{
	IMG_UINT32 i, j, ui32MaxFace;
	GLESMipMapLevel *psMipLevel;

	GLES1_TIME_START(GLES1_TIMER_TEXTURE_READBACK_TIME);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
	{
		ui32MaxFace = 6;
	}
	else
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
	{
		ui32MaxFace = 1;
	}

	for(j=0; j < ui32MaxFace; j++)
	{
		for(i=0; i < GLES1_MAX_TEXTURE_MIPMAP_LEVELS; i++)
		{
			psMipLevel = &psTex->psMipLevel[i + (j * GLES1_MAX_TEXTURE_MIPMAP_LEVELS)];

			if(psMipLevel->pui8Buffer == GLES1_LOADED_LEVEL)
			{
				psMipLevel->pui8Buffer = GLES1Malloc(gc, psMipLevel->ui32ImageSize);

				if (psMipLevel->pui8Buffer == IMG_NULL) 
				{
					GLES1_TIME_STOP(GLES1_TIMER_TEXTURE_READBACK_TIME);

					return IMG_FALSE;
				}

#if (defined(DEBUG) || defined(TIMING))
				ui32TextureMemCurrent += psMipLevel->ui32ImageSize;

				if (ui32TextureMemCurrent>ui32TextureMemHWM)
				{
					ui32TextureMemHWM = ui32TextureMemCurrent;
				}
#endif
				/* Wait for outstanding renders as necessary before reading it back */
				FlushAttachableIfNeeded(gc, (GLES1FrameBufferAttachable*)psMipLevel, GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_WAIT_FOR_3D);

				ReadBackTextureData(gc, psTex, j, i, psMipLevel->pui8Buffer);

				FBOAttachableHasBeenModified(gc, (GLES1FrameBufferAttachable *)psMipLevel);
			}
		}
	}
	GLES1_TIME_STOP(GLES1_TIMER_TEXTURE_READBACK_TIME);	

	if(KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
	{
		TexMgrGhostTexture(gc, psTex);
	}
	else
	{
		if(psTex->psMemInfo)
		{
#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent -= psTex->psMemInfo->uAllocSize;
#endif
			GLES1FREEDEVICEMEM_HEAP(gc, psTex->psMemInfo);

			psTex->psMemInfo = IMG_NULL;
		
			KRM_RemoveResourceFromAllLists(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource);
		}

		psTex->bResidence = IMG_FALSE;
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : IsTextureConsistent
 Inputs             : gc, psTex, bCheckForRenderingLoop
 Outputs            : -
 Returns            : Whether a texture is bConsistent.
 Description        : Checks consistency for cubemap/mipmapped textures. Unloads data for
					  inconsistent textures. Also unloads textures which are 
					  consistent but don't match their device memory versions.
************************************************************************************/
IMG_INTERNAL IMG_UINT32 IsTextureConsistent(GLES1Context *gc, GLESTexture *psTex, IMG_BOOL bCheckForRenderingLoop)
{
	/* NOTE: If we change how this function computes the control words we must update
	 * SetupTextureRenderTargetControlWords() to reflect those changes.
	 */
	const GLESTextureFormat *psTexFormat = psTex->psMipLevel[0].psTexFormat;
	GLESTextureParamState    *psParams = &psTex->sState;
	IMG_UINT32                i, ui32Width, ui32Height, ui32Face, ui32MaxFace, ui32WidthLog2, ui32HeightLog2,
							  ui32NumLevels, ui32Consistent, ui32FormatFlags = 0, ui32TexTypeSize = 0,
							  ui32StateWord1;
	IMG_BOOL                  bStateWord1Changed = IMG_FALSE;
	GLenum                    eRequestedFormat;
	GLES1FrameBufferAttachable *psColorAttachment = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];


	if(psTex->ui32LevelsConsistent != GLES1_TEX_UNKNOWN)
	{
		return psTex->ui32LevelsConsistent;
	}

	if(psTexFormat == IMG_NULL)
	{
		psTex->ui32LevelsConsistent = GLES1_TEX_INCONSISTENT;
		return IMG_FALSE;
	}

	/* If there is a rendering loop, return false, without setting the levelsconsistent state. The texture isn't necessarily 
	 * inconsistent, but we don't want to reference the texture, until it is no longer bound as a destination for rendering.
	 */
	if(bCheckForRenderingLoop && psColorAttachment && psColorAttachment->eAttachmentType == GL_TEXTURE)
	{
		if(((GLESMipMapLevel*)psColorAttachment)->psTex->sNamedItem.ui32Name == psTex->sNamedItem.ui32Name)
		{
			return IMG_FALSE;
		}
	}

	if(GLES1_IS_MIPMAP(psParams->ui32MinFilter))
	{
		ui32FormatFlags |= GLES1_MIPMAP;
	}


	if(psTexFormat->ui32TotalBytesPerTexel > 4)
	{
		ui32FormatFlags |= GLES1_COMPRESSED;
	}
	
	ui32StateWord1 = asSGXPixelFormat[psTexFormat->ePixelFormat].aui32TAGControlWords[0][1];

	ui32Consistent = GLES1_TEX_CONSISTENT;

	ui32Width = psTex->psMipLevel[0].ui32Width;
	ui32Height = psTex->psMipLevel[0].ui32Height;

	ui32WidthLog2 = psTex->psMipLevel[0].ui32WidthLog2;
	ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2;
	
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
	{
		eRequestedFormat = psTex->psMipLevel[0].eRequestedFormat;
		ui32MaxFace = 6;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_CEM;
#else
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_CEM;
#endif

		GLES1_ASSERT(ui32Width != 0 && ui32Height != 0);

		/* First check base level of each face is consistent */
		for(ui32Face=1; ui32Face < ui32MaxFace; ui32Face++)
		{
			IMG_UINT32 ui32FaceBaseLevel = ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS;

			if(psTex->psMipLevel[ui32FaceBaseLevel].eRequestedFormat != eRequestedFormat)
			{
				ui32Consistent = GLES1_TEX_INCONSISTENT;
				break;
			}
			else if(psTex->psMipLevel[ui32FaceBaseLevel].psTexFormat != psTexFormat)
			{
				ui32Consistent = GLES1_TEX_INCONSISTENT;
				break;
			}
			else if(psTex->psMipLevel[ui32FaceBaseLevel].ui32Width != ui32Width)
			{
				ui32Consistent = GLES1_TEX_INCONSISTENT;
				break;
			}
			else if(psTex->psMipLevel[ui32FaceBaseLevel].ui32Height != ui32Height)
			{
				ui32Consistent = GLES1_TEX_INCONSISTENT;
				break;
			}
		}
	}
	else
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
	{
		ui32MaxFace = 1;
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_2D;
#else
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_2D;
#endif
	}

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	/* Don't support mipmapped pbuffer textures */
	if(psTex->hPBuffer)
	{
		if(ui32FormatFlags & GLES1_MIPMAP)
		{
			ui32Consistent = GLES1_TEX_INCONSISTENT;
		}

		/* Just return - the pbuffer is self-consistent and we have already set up control words */
		psTex->ui32LevelsConsistent = ui32Consistent;
	
		return ui32Consistent;
	}
#endif

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	/* Don't support mipmapped eglimage textures */
	if(psTex->psEGLImageTarget)
	{
		if(ui32FormatFlags & GLES1_MIPMAP)
		{
			ui32Consistent = GLES1_TEX_INCONSISTENT;
		}

		/* Just return - the eglimage is self-consistent and we have already set up control words */
		psTex->ui32LevelsConsistent = ui32Consistent;
	
		return ui32Consistent;
	}
#endif


	/* Now check mipmap consistency */
	if(((ui32FormatFlags & GLES1_MIPMAP) != 0) && (ui32Consistent == GLES1_TEX_CONSISTENT))
	{
		eRequestedFormat = psTex->psMipLevel[0].eRequestedFormat;

		for(ui32Face = 0; ui32Face < ui32MaxFace; ui32Face++)
		{
			IMG_UINT32 ui32FaceBaseLevel = ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS;
			
			i = 0;

			ui32Width = psTex->psMipLevel[0].ui32Width;
			ui32Height = psTex->psMipLevel[0].ui32Height;
	
			while (++i < GLES1_MAX_TEXTURE_MIPMAP_LEVELS) 
			{
				if (ui32Width == 1 && ui32Height == 1)
				{
					break;
				}

				ui32Width >>= 1;

				if (ui32Width == 0)
				{
					ui32Width = 1;
				}

				ui32Height >>= 1;

				if (ui32Height == 0)
				{
					ui32Height = 1;
				}

				if(psTex->psMipLevel[ui32FaceBaseLevel + i].eRequestedFormat != eRequestedFormat)
				{
					ui32Consistent = GLES1_TEX_INCONSISTENT;

					break;
				}
				else if(psTex->psMipLevel[ui32FaceBaseLevel + i].psTexFormat != psTexFormat)
				{
					ui32Consistent = GLES1_TEX_INCONSISTENT;

					break;
				}
				else if(psTex->psMipLevel[ui32FaceBaseLevel + i].ui32Width != ui32Width)
				{
					ui32Consistent = GLES1_TEX_INCONSISTENT;

					break;
				}
				else if(psTex->psMipLevel[ui32FaceBaseLevel + i].ui32Height != ui32Height)
				{
					ui32Consistent = GLES1_TEX_INCONSISTENT;

					break;
				}
			}
		}
	}

	if(ui32FormatFlags & GLES1_MIPMAP)
	{
		ui32NumLevels = MAX(ui32WidthLog2, ui32HeightLog2) + 1;
	}
	else
	{
		ui32NumLevels = 1;
	}

	/* Reset ui32Width & ui32Height for the following use */
	/* PRQA S 3199 2 */ /* ui32Width and ui32Height are going to be used if SGX_FEATURE_TAG_POT_TWIDDLE is undefined. */
	ui32Width = psTex->psMipLevel[0].ui32Width;
	ui32Height = psTex->psMipLevel[0].ui32Height;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
	ui32TexTypeSize |=	(ui32WidthLog2  << EURASIA_PDS_DOUTT1_USIZE_SHIFT)	|
						(ui32HeightLog2 << EURASIA_PDS_DOUTT1_VSIZE_SHIFT)  ;
#else
	ui32TexTypeSize |=	(((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)	|
		                (((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;
#endif

	ui32StateWord1 |= ui32TexTypeSize;

	if(psParams->ui32StateWord1 != ui32StateWord1)
	{
		bStateWord1Changed = IMG_TRUE;
	}
	
	if(ui32FormatFlags & GLES1_MIPMAP)
	{
		if(psTex->psMemInfo && 
		   (ui32Consistent == GLES1_TEX_INCONSISTENT || 
			bStateWord1Changed                       ||
			((psTex->ui32HWFlags & GLES1_MIPMAP) == 0)))
		{
			if(UnloadInconsistentTexture(gc, psTex) == IMG_FALSE)
			{
				return IMG_FALSE;
			}
		}

		if(ui32Consistent == GLES1_TEX_CONSISTENT)
		{
			psParams->ui32StateWord1 = ui32StateWord1;

			psTex->ui32NumLevels = ui32NumLevels;
			psTex->ui32HWFlags = ui32FormatFlags;
			psTex->psFormat = psTexFormat;
		}
	}
	else
	{
		if(psTex->psMemInfo)
		{
			/* Unload due to format/size change */
		    if(bStateWord1Changed)
			{
				if(UnloadInconsistentTexture(gc, psTex) == IMG_FALSE)
				{
					return IMG_FALSE;
				}
			}
			else if(psTex->ui32HWFlags & GLES1_MIPMAP)
			{
				/* Texture is mimapped, but filter is not, keep texture as mipmap */
				ui32NumLevels = psTex->ui32NumLevels;
				ui32FormatFlags |= GLES1_MIPMAP;
			}
		}

		psParams->ui32StateWord1 = ui32StateWord1;
	
		psTex->ui32NumLevels = ui32NumLevels;
		psTex->ui32HWFlags = ui32FormatFlags;
		psTex->psFormat = psTexFormat;
	}

	psTex->ui32LevelsConsistent = ui32Consistent;

	return ui32Consistent;
}


#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)

/***********************************************************************************
 Function Name      : TextureCreatePBufferLevel
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : -
 Description        : Frees host memory for all texture levels + sets of level 0 from
					  pbuffer params.
************************************************************************************/
IMG_INTERNAL IMG_BOOL TextureCreatePBufferLevel(GLES1Context *gc, GLESTexture *psTex)
{
	const GLESTextureFormat *psTexFormat;
	EGLDrawableParams sParams;
	GLESMipMapLevel   *psMipLevel;
	IMG_UINT32 i, ui32BufferSize; 
	GLESTextureParamState *psParams = &psTex->sState;

	/* Frees any data currently associated with the texture - all levels */
	for(i = 0; i < GLES1_MAX_TEXTURE_MIPMAP_LEVELS; i++)
	{
		psMipLevel = &psTex->psMipLevel[i];

		/* The texture level is being freed */
		if (psMipLevel->pui8Buffer != NULL && psMipLevel->pui8Buffer != GLES1_LOADED_LEVEL)
		{
			PVR_UNREFERENCED_PARAMETER(gc);
			GLES1Free(gc, psMipLevel->pui8Buffer);
		}

		psMipLevel->pui8Buffer		 = IMG_NULL;
		psMipLevel->ui32Width		 = 0;
		psMipLevel->ui32Height		 = 0;
		psMipLevel->ui32ImageSize	 = 0;
		psMipLevel->ui32WidthLog2	 = 0;
		psMipLevel->ui32HeightLog2	 = 0;
		psMipLevel->psTexFormat		 = IMG_NULL;
		psMipLevel->eRequestedFormat = 1;
		psMipLevel->ui32Level        = 0;
		psMipLevel->psTex            = psTex;
	}

	psMipLevel = &psTex->psMipLevel[0];

	if (!KEGLGetDrawableParameters(psTex->hPBuffer, &sParams, IMG_TRUE))
	{
		PVR_DPF((PVR_DBG_ERROR,"TextureCreatePBufferLevel: Can't get drawable info"));

		return IMG_FALSE;
	}

	GLES1_ASSERT(sParams.eDrawableType==EGL_DRAWABLETYPE_PBUFFER);

	switch(sParams.ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		{
			psTexFormat = &TexFormatRGB565;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			psTexFormat = &TexFormatARGB4444;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		{
			psTexFormat = &TexFormatARGB8888;
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"TextureCreatePBufferLevel: Unknown pixel format: %d", sParams.ePixelFormat));			

			return IMG_FALSE;
		}
	}

	ui32BufferSize = sParams.ui32Width * sParams.ui32Height * psTexFormat->ui32TotalBytesPerTexel;

	psMipLevel->ui32Width		 = sParams.ui32Width;
	psMipLevel->ui32Height		 = sParams.ui32Height;
	psMipLevel->ui32ImageSize	 = ui32BufferSize;
	psMipLevel->ui32WidthLog2	 = FloorLog2(psMipLevel->ui32Width);
	psMipLevel->ui32HeightLog2	 = FloorLog2(psMipLevel->ui32Height);
	psMipLevel->psTexFormat		 = psTexFormat;
	psMipLevel->eRequestedFormat = (GLenum)((psTexFormat->ui32BaseFormatIndex == GLES1_RGB_TEX_INDEX) ? GL_RGB : GL_RGBA);

	psTex->psFormat = psTexFormat;

	psParams->ui32StateWord1 = asSGXPixelFormat[psTexFormat->ePixelFormat].aui32TAGControlWords[0][1];;

	if(sParams.psRenderSurface->bIsTwiddledSurface)
	{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
		psParams->ui32StateWord1 |= (FloorLog2(psMipLevel->ui32Width)  << EURASIA_PDS_DOUTT1_USIZE_SHIFT) |
								  	(FloorLog2(psMipLevel->ui32Height) << EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
		psParams->ui32StateWord1 |= (((psMipLevel->ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  |
									(((psMipLevel->ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;
#endif 
	}
	else
	{
		psParams->ui32StateWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE |
								  ((psMipLevel->ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
								  ((psMipLevel->ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
	}

	psParams->ui32StateWord2 = (sParams.ui32HWSurfaceAddress >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;


	psTex->ui32LevelsConsistent = GLES1_TEX_UNKNOWN;

	return IMG_TRUE;
}

#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */


#if defined(GLES1_EXTENSION_EGL_IMAGE)
/***********************************************************************************
 Function Name      : TextureCreateImageLevel
 Inputs             : gc, psEGLImage
 Outputs            : -
 Returns            : -
 Description        : Creates a texture level from an EGLImage
************************************************************************************/
IMG_INTERNAL IMG_BOOL TextureCreateImageLevel(GLES1Context *gc, GLESTexture *psTex)
{
	const GLESTextureFormat *psTexFormat;
	GLESMipMapLevel   *psMipLevel;
	IMG_UINT32 i, ui32BufferSize;
	GLESTextureParamState *psParams = &psTex->sState;
	EGLImage *psEGLImage;

	/* Frees any data currently associated with the texture - all levels */
	for(i = 0; i < GLES1_MAX_TEXTURE_MIPMAP_LEVELS; i++)
	{
		psMipLevel = &psTex->psMipLevel[i];

		/* The texture level is being freed */
		if (psMipLevel->pui8Buffer != NULL && psMipLevel->pui8Buffer != GLES1_LOADED_LEVEL)
		{
			PVR_UNREFERENCED_PARAMETER(gc);
			GLES1Free(gc, psMipLevel->pui8Buffer);
		}

		psMipLevel->pui8Buffer		 = IMG_NULL;
		psMipLevel->ui32Width		 = 0;
		psMipLevel->ui32Height		 = 0;
		psMipLevel->ui32ImageSize	 = 0;
		psMipLevel->ui32WidthLog2	 = 0;
		psMipLevel->ui32HeightLog2	 = 0;
		psMipLevel->psTexFormat		 = IMG_NULL;
		psMipLevel->eRequestedFormat = 1;
		psMipLevel->ui32Level        = 0;
		psMipLevel->psTex            = psTex;
	}

	psMipLevel = &psTex->psMipLevel[0];

	psEGLImage = psTex->psEGLImageTarget;

	switch(psEGLImage->ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		{
			psTexFormat = &TexFormatRGB565;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			psTexFormat = &TexFormatARGB4444;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			psTexFormat = &TexFormatARGB1555;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		{
			psTexFormat = &TexFormatARGB8888;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		{
			psTexFormat = &TexFormatABGR8888;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_XRGB8888:
		{
			psTexFormat = &TexFormatXRGB8888;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_XBGR8888:
		{
			psTexFormat = &TexFormatXBGR8888;

			break;
		}
#if defined(EGL_IMAGE_COMPRESSED_GLES1)
		case PVRSRV_PIXEL_FORMAT_PVRTC2:
		{
			if( psEGLImage->bCompressedRGBOnly )
				psTexFormat = &TexFormatPVRTC2RGB;
			else
				psTexFormat = &TexFormatPVRTC2RGBA;

			break;
		}

		case PVRSRV_PIXEL_FORMAT_PVRTC4:
		{
			if( psEGLImage->bCompressedRGBOnly )
				psTexFormat = &TexFormatPVRTC4RGB;
			else
				psTexFormat = &TexFormatPVRTC4RGBA;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_PVRTCII2:
		{
			if( psEGLImage->bCompressedRGBOnly )
				psTexFormat = &TexFormatPVRTCII2RGB;
			else
				psTexFormat = &TexFormatPVRTCII2RGBA;

			break;
		}

		case PVRSRV_PIXEL_FORMAT_PVRTCII4:
		{
			if( psEGLImage->bCompressedRGBOnly )
				psTexFormat = &TexFormatPVRTCII4RGB;
			else
				psTexFormat = &TexFormatPVRTCII4RGBA;

			break;
		}

		case PVRSRV_PIXEL_FORMAT_PVRTCIII:
		{
			psTexFormat = &TexFormatETC1RGB8;
			break;
		}
#endif
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case PVRSRV_PIXEL_FORMAT_NV12:
		{
			psTexFormat = &TexFormat2PlaneYUV420;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV:
		{
			psTexFormat = &TexFormatYUYV;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU:
		{
			psTexFormat = &TexFormatYVYU;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY:
		{
			psTexFormat = &TexFormatUYVY;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY:
		{
			psTexFormat = &TexFormatVYUY;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_YV12:
		{
			psTexFormat = &TexFormat3PlaneYUV420;
			break;
		}
#endif /* GLES1_EXTENSION_EGL_IMAGE_EXTERNAL */
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"TextureCreateImageLevel: Unknown pixel format: %d", psEGLImage->ePixelFormat));			

			return IMG_FALSE;
		}
	}

#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
	if (psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_STREAM)
	{
		/* FIXME: Get flags somehow for EGL image (Color conversion, range etc.) */
		CreateExternalTextureState(psTex->psExtTexState, psEGLImage->ui32Width,
									   psEGLImage->ui32Height, psEGLImage->ePixelFormat,
									   psEGLImage->ui32Stride, psEGLImage->ui32Flags);

	}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL) */
									   

	ui32BufferSize = psEGLImage->ui32Width * psEGLImage->ui32Height * psTexFormat->ui32TotalBytesPerTexel;

	psMipLevel->pui8Buffer		 = GLES1_LOADED_LEVEL;
	psMipLevel->ui32Width		 = psEGLImage->ui32Width;
	psMipLevel->ui32Height		 = psEGLImage->ui32Height;
	psMipLevel->ui32ImageSize	 = ui32BufferSize;
	psMipLevel->ui32WidthLog2	 = FloorLog2(psMipLevel->ui32Width);
	psMipLevel->ui32HeightLog2	 = FloorLog2(psMipLevel->ui32Height);
	psMipLevel->psTexFormat		 = psTexFormat;
	psMipLevel->eRequestedFormat = (GLenum)((psTexFormat->ui32BaseFormatIndex == GLES1_RGB_TEX_INDEX) ? GL_RGB : GL_RGBA);

	psTex->psFormat = psTexFormat;

	SetupTwiddleFns(psTex);

	psParams->ui32StateWord1 = asSGXPixelFormat[psTexFormat->ePixelFormat].aui32TAGControlWords[0][1];

	if(psEGLImage->bTwiddled)
	{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
		psParams->ui32StateWord1 |= (FloorLog2(psMipLevel->ui32Width)  << EURASIA_PDS_DOUTT1_USIZE_SHIFT) |
								  	(FloorLog2(psMipLevel->ui32Height) << EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
		psParams->ui32StateWord1 |= (((psMipLevel->ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  |
									(((psMipLevel->ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;
#endif 
	}
	else
	{
		psParams->ui32StateWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE |
								  ((psMipLevel->ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
								  ((psMipLevel->ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
	}

	psParams->ui32StateWord2 = (psEGLImage->ui32HWSurfaceAddress >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

	psTex->ui32LevelsConsistent = GLES1_TEX_UNKNOWN;

	return IMG_TRUE;
}

#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)

static IMG_VOID SetupEGLImageArbitraryStride(GLESTexture *psTex, IMG_UINT32 *pui32StateWord0, IMG_UINT32 *pui32StateWord1)
{
	IMG_UINT32 ui32StateWord0 = psTex->sState.ui32StateWord0;
	IMG_UINT32 ui32StateWord1 = psTex->sState.ui32StateWord1;
	IMG_UINT32 ui32StrideInPixels, ui32MatchingStride;
	IMG_UINT32 ui32Stride = psTex->psEGLImageTarget->ui32Stride;

	switch(psTex->psEGLImageTarget->ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			ui32StrideInPixels = ui32Stride >> 1;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		case PVRSRV_PIXEL_FORMAT_XRGB8888:
		case PVRSRV_PIXEL_FORMAT_XBGR8888:
		{
			ui32StrideInPixels = ui32Stride >> 2;
			break;
		}
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case PVRSRV_PIXEL_FORMAT_NV12:
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV:
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY:
		{
			ui32StrideInPixels = ui32Stride >> 1;
			break;
		}
#endif /* GLES1_EXTENSION_EGL_IMAGE_EXTERNAL */
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"SetupEGLImageArbitraryStride: Unsupported pixel format"));
			return;
		}
	}

#if(EURASIA_TAG_STRIDE_THRESHOLD > 0)
	if(psTex->psEGLImageTarget->ui32Width < EURASIA_TAG_STRIDE_THRESHOLD)
	{
		ui32MatchingStride = (psTex->psEGLImageTarget->ui32Width + (EURASIA_TAG_STRIDE_ALIGN0 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN0 - 1);
	}
	else
#endif
	{			
		ui32MatchingStride = (psTex->psEGLImageTarget->ui32Width + (EURASIA_TAG_STRIDE_ALIGN1 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN1 - 1);
	}

	/* Only switch to NP2 stride mode if stride and width don't match according to HW */
	if(ui32StrideInPixels != ui32MatchingStride)
	{
		/* Stride in 32 bit units */
		ui32Stride >>= 2;

		/* Stride is specified as 0 == 1 */
		ui32Stride -= 1;

#if defined(EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT)

		ui32StateWord1 &= EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK;
		ui32StateWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT;
			
		ui32StateWord0 &= (EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK);

		ui32StateWord0 |= 
			(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDELO_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK) |
			(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK);

#if defined(EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK)
		ui32StateWord0 &= (EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK);

		ui32StateWord0 |= 
			(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEEX_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEEX_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK);
#endif

#else /* EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT */

#if defined(SGX_FEATURE_VOLUME_TEXTURES)
		ui32StateWord0 &= (EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK);

		ui32StateWord0 |= EURASIA_PDS_DOUTT0_STRIDE |
									(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDELO_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK) |
									(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK) ;

#else /* defined(SGX_FEATURE_VOLUME_TEXTURES) */

		ui32StateWord0 &= (EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & EURASIA_PDS_DOUTT0_STRIDELO2_CLRMSK);

#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
		ui32StateWord0 &= EURASIA_PDS_DOUTT0_STRIDEEX0_CLRMSK;
#endif

		ui32StateWord0 |= EURASIA_PDS_DOUTT0_STRIDE |
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
									(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEEX0_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEEX0_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEEX0_CLRMSK) |
#endif
									(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO2_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDELO2_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO2_CLRMSK) |
									(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK)    ;

		ui32StateWord1 &= EURASIA_PDS_DOUTT1_STRIDELO1_CLRMSK;

#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
		ui32StateWord1 &= EURASIA_PDS_DOUTT1_STRIDEEX1_CLRMSK;
#endif

		ui32StateWord1 |= 
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
						(((ui32Stride >> EURASIA_PDS_DOUTT1_STRIDEEX1_STRIDE_SHIFT) << EURASIA_PDS_DOUTT1_STRIDEEX1_SHIFT) & ~EURASIA_PDS_DOUTT1_STRIDEEX1_CLRMSK) |
#endif
						(((ui32Stride >> EURASIA_PDS_DOUTT1_STRIDELO1_STRIDE_SHIFT) << EURASIA_PDS_DOUTT1_STRIDELO1_SHIFT) & ~EURASIA_PDS_DOUTT1_STRIDELO1_CLRMSK);

#endif /* defined(SGX_FEATURE_VOLUME_TEXTURES) */

#endif /* EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT */
	}

	*pui32StateWord0 = ui32StateWord0;
	*pui32StateWord1 = ui32StateWord1;

}

#endif /* defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION) */

#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */



/***********************************************************************************
 Function Name      : TextureCreateLevel
 Inputs             : gc, psTex, ui32Level, eInternalFormat, psTexFormat, ui32Width, 
					  ui32Height
 Outputs            : -
 Returns            : Pointer to new texture level buffer
 Description        : (Re)Allocates host memory for a new texture level.
************************************************************************************/
IMG_INTERNAL IMG_UINT8*  TextureCreateLevel(GLES1Context *gc, GLESTexture *psTex, 
											IMG_UINT32 ui32Level, GLenum eInternalFormat, 
											const GLESTextureFormat *psTexFormat, 
											 IMG_UINT32 ui32Width, IMG_UINT32 ui32Height)
{
	IMG_UINT8 *pui8Buffer;
	IMG_UINT32 ui32BaseWidth;  
	IMG_UINT32 ui32BaseHeight;
	IMG_UINT32 ui32BufferSize;
	IMG_UINT32 ui32BufferWidth, ui32BufferHeight;
	GLESMipMapLevel *psMipLevel = &psTex->psMipLevel[ui32Level];
	IMG_UINT32 ui32Lod = (ui32Level % GLES1_MAX_TEXTURE_MIPMAP_LEVELS);

	ui32BaseWidth = ui32Width * (1UL << ui32Lod);
	ui32BaseHeight = ui32Height * (1UL << ui32Lod);

	GLES1_ASSERT(psTexFormat != IMG_NULL);

	ui32BufferWidth = ui32Width;
	ui32BufferHeight = ui32Height;

	/* Adjust width, height for compressed formats */
	switch(psTexFormat->ePixelFormat)
	{
#if defined(GLES1_EXTENSION_PVRTC)
		case PVRSRV_PIXEL_FORMAT_PVRTC2:
		case PVRSRV_PIXEL_FORMAT_PVRTCII2:
		{
			ui32BufferWidth = MAX(ui32BufferWidth >> 3, 1);
			ui32BufferHeight = MAX(ui32BufferHeight >> 2, 1);

			break;
		}
		case PVRSRV_PIXEL_FORMAT_PVRTC4:
		case PVRSRV_PIXEL_FORMAT_PVRTCII4:
		{
			ui32BufferWidth = MAX(ui32BufferWidth >> 2, 1);
			ui32BufferHeight = MAX(ui32BufferHeight >> 2, 1);

			break;
		}
#endif
#if defined(GLES1_EXTENSION_ETC1)
		case PVRSRV_PIXEL_FORMAT_PVRTCIII:
		{
			ui32BufferWidth = MAX(ui32BufferWidth >> 2, 1);
			ui32BufferHeight = MAX(ui32BufferHeight >> 2, 1);

			break;
		}
#endif /* defined(GLES1_EXTENSION_ETC1) */
		default:
		{
			break;
		}
	}

	ui32BufferSize = ui32BufferWidth * ui32BufferHeight * psTexFormat->ui32TotalBytesPerTexel;

	if (ui32BaseWidth > GLES1_MAX_TEXTURE_SIZE || ui32BaseHeight > GLES1_MAX_TEXTURE_SIZE)
	{
		/* Texture allocation failed */
		SetError(gc, GL_INVALID_VALUE);
		return 0;
	}

	if (ui32BufferSize)
	{
		if (psMipLevel->pui8Buffer != GLES1_LOADED_LEVEL)
		{
			pui8Buffer = (IMG_UINT8 *) GLES1Realloc(gc, psMipLevel->pui8Buffer, ui32BufferSize);
		
			if (pui8Buffer == IMG_NULL) 
			{
no_memory:
				SetError(gc, GL_OUT_OF_MEMORY);
				return 0;
			}

			psMipLevel->pui8Buffer = pui8Buffer;

#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent += (IMG_INT32) (ui32BufferSize - psMipLevel->ui32ImageSize);

			if (ui32TextureMemCurrent>ui32TextureMemHWM)
			{
				ui32TextureMemHWM = ui32TextureMemCurrent;
			}
#endif
		}
		else
		{
			pui8Buffer = (IMG_UINT8 *) GLES1Malloc(gc, ui32BufferSize);

			if (pui8Buffer == IMG_NULL) 
			{
				goto no_memory;
			}

			psMipLevel->pui8Buffer = pui8Buffer;

#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent += ui32BufferSize;

			if (ui32TextureMemCurrent>ui32TextureMemHWM)
			{
				ui32TextureMemHWM = ui32TextureMemCurrent;
			}
#endif
		}

		psMipLevel->ui32Width        = ui32Width;
		psMipLevel->ui32Height       = ui32Height;
		psMipLevel->ui32ImageSize    = ui32BufferSize;
		psMipLevel->ui32WidthLog2     = FloorLog2(psMipLevel->ui32Width);
		psMipLevel->ui32HeightLog2    = FloorLog2(psMipLevel->ui32Height);
		psMipLevel->psTexFormat      = psTexFormat;
		psMipLevel->eRequestedFormat = eInternalFormat;
		psMipLevel->ui32Level        = ui32Level;
		psMipLevel->psTex            = psTex;

		psTex->psFormat = psTexFormat;
	}
	else
	{
		/* The texture level is being freed */
		if (psMipLevel->pui8Buffer != IMG_NULL && psMipLevel->pui8Buffer != GLES1_LOADED_LEVEL)
		{
#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent -= psMipLevel->ui32ImageSize;
#endif
			GLES1Free(gc, psMipLevel->pui8Buffer);
		}

		psMipLevel->pui8Buffer = IMG_NULL;

		psMipLevel->ui32Width        = 0;
		psMipLevel->ui32Height       = 0;
		psMipLevel->ui32ImageSize    = 0;
		psMipLevel->ui32WidthLog2     = 0;
		psMipLevel->ui32HeightLog2    = 0;
		psMipLevel->psTexFormat      = IMG_NULL;
		psMipLevel->eRequestedFormat = 1;
		psMipLevel->ui32Level        = ui32Level;
		psMipLevel->psTex            = psTex;
	}

	psTex->ui32LevelsConsistent = GLES1_TEX_UNKNOWN;

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE;

	return psMipLevel->pui8Buffer;
}


/***********************************************************************************
 Function Name      : CreateTexture
 Inputs             : gc, ui32Name, ui32Target
 Outputs            : -
 Returns            : Pointer to new texture structure
 Description        : Creates and initialises a new texture structure.
************************************************************************************/
IMG_INTERNAL GLESTexture *CreateTexture(GLES1Context *gc, IMG_UINT32 ui32Name,
										IMG_UINT32 ui32Target)
{
	GLESTexture           *psTex;
	GLESTextureParamState *psParams;
	IMG_UINT32             i, ui32NumLevels;
	GLESMipMapLevel       *psLevel;

	PVR_UNREFERENCED_PARAMETER(gc);

	psTex = (GLESTexture *) GLES1Calloc(gc, sizeof(GLESTexture));

	if (psTex == IMG_NULL)
	{
		return IMG_NULL;
	}

	psParams = &psTex->sState;

	/* initialize the texture */
	psParams->ui32MagFilter =	EURASIA_PDS_DOUTT0_MAGFILTER_LINEAR;
	psParams->ui32MinFilter =	EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX | 
								EURASIA_PDS_DOUTT0_MINFILTER_POINT |
								EURASIA_PDS_DOUTT0_MIPFILTER;
	psParams->bAutoMipGen   =   IMG_FALSE;

	psParams->ui32StateWord0 =	EURASIA_PDS_DOUTT0_UADDRMODE_REPEAT | 
								EURASIA_PDS_DOUTT0_VADDRMODE_REPEAT;

	psTex->sNamedItem.ui32Name	= ui32Name;
	psTex->bResidence			= IMG_FALSE;
	psTex->bHasEverBeenGhosted  = IMG_FALSE;
	psTex->ui32LevelsConsistent = GLES1_TEX_INCONSISTENT;
	psTex->ui32TextureTarget	= ui32Target;

#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
	psTex->psExtTexState = GLES1Calloc(gc, sizeof(GLES1ExternalTexState));
#endif /* GLES1_EXTENSION_EGL_IMAGE_EXTERNAL */

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
	psTex->psBufferDevice           = IMG_NULL;
	psTex->ui32BufferOffset         = 0;
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */

	if(ui32Target == GLES1_TEXTURE_TARGET_2D)
	{
		ui32NumLevels = GLES1_MAX_TEXTURE_MIPMAP_LEVELS;
	}

#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
	else if(ui32Target == GLES1_TEXTURE_TARGET_STREAM)
	{
		psTex->sNamedItem.ui32Name	= ui32Name;
		psTex->bResidence			= IMG_TRUE;
		psTex->ui32LevelsConsistent = GLES1_TEX_CONSISTENT;
		psParams->ui32MagFilter =	EURASIA_PDS_DOUTT0_MAGFILTER_LINEAR;
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		psParams->ui32MinFilter =	EURASIA_PDS_DOUTT0_MINFILTER_POINT | EURASIA_PDS_DOUTT0_NOTMIPMAP;
#endif
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		psParams->ui32MinFilter =	EURASIA_PDS_DOUTT0_MINFILTER_LINEAR | EURASIA_PDS_DOUTT0_NOTMIPMAP;
#endif
		psParams->ui32StateWord0 =	EURASIA_PDS_DOUTT0_UADDRMODE_CLAMP | 
									EURASIA_PDS_DOUTT0_VADDRMODE_CLAMP;
		ui32NumLevels = GLES1_MAX_TEXTURE_MIPMAP_LEVELS;
	}
#endif	/* defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)*/

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	else
	{
		/* CUBE MAP */
		ui32NumLevels = GLES1_MAX_TEXTURE_MIPMAP_LEVELS * GLES1_TEXTURE_CEM_FACE_MAX;
	}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

	psTex->psMipLevel = GLES1Calloc(gc, sizeof(GLESMipMapLevel) * ui32NumLevels);

	if(psTex->psMipLevel == IMG_NULL)
	{
		GLES1Free(gc, psTex);
		return IMG_NULL;
	}

	for(i=0; i < ui32NumLevels; ++i)
	{
		psLevel            = &psTex->psMipLevel[i];
		psLevel->psTex     = psTex;
		psLevel->ui32Level = i;

		((GLES1FrameBufferAttachable*)psLevel)->psRenderSurface = IMG_NULL;
		((GLES1FrameBufferAttachable*)psLevel)->eAttachmentType = GL_TEXTURE;
	}


#if defined(PDUMP)
	psTex->ui32LevelsToDump = 0;
#endif

	return psTex;
}

/***********************************************************************************
 Function Name      : FreeTexture
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : -
 Description        : Frees/ghosts all structures/data associated with a texture
************************************************************************************/
static IMG_VOID FreeTexture(GLES1Context *gc, GLESTexture *psTex)
{
	IMG_UINT32       i, ui32MaxLevel;
	GLESMipMapLevel *psMipLevel;

	ui32MaxLevel = GLES1_MAX_TEXTURE_MIPMAP_LEVELS;

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
	{
		ui32MaxLevel *= 6;
	}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

	FlushUnflushedTextureRenders(gc, psTex);

	for (i = 0; i < ui32MaxLevel; i++)
	{
		psMipLevel = &psTex->psMipLevel[i];

		DestroyFBOAttachableRenderSurface(gc, (GLES1FrameBufferAttachable*)psMipLevel);

		if (psMipLevel->pui8Buffer && psMipLevel->pui8Buffer != GLES1_LOADED_LEVEL) 
		{
#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent -= psMipLevel->ui32ImageSize;
#endif
			GLES1Free(gc, psMipLevel->pui8Buffer);

			psMipLevel->pui8Buffer = IMG_NULL;
		}
	}

	GLES1Free(gc, psTex->psMipLevel);

	psTex->psMipLevel = IMG_NULL;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageSource || psTex->psEGLImageTarget)
	{
		/* If the texture was live we must ghost it */
		if (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
		{
			TexMgrGhostTexture(gc, psTex);
		}
		else
		{
			if(psTex->psEGLImageSource)
			{
				KEGLUnbindImage(psTex->psEGLImageSource->hImage);
			}
			else
			{
				KEGLUnbindImage(psTex->psEGLImageTarget->hImage);
			}
		}
	}
	else
#endif /*defined(GLES1_EXTENSION_EGL_IMAGE) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
	if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_STREAM)
	{
	    if (psTex->psBufferDevice)
		{
			/* If the texture was live, it must be ghosted */
			if (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
			{
				psTex->psBufferDevice->bGhosted = IMG_TRUE;

				TexMgrGhostTexture(gc, psTex);
			}
			else
			{
				TexStreamUnbindBufferDevice(gc, (IMG_VOID *)psTex->psBufferDevice);
			}
		}
	}	
    else
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */
#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	if(psTex->hPBuffer)
	{
		ReleasePbufferFromTexture(gc, psTex);
	}
	else
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */
	if (psTex->psMemInfo)
	{
		/* If the texture was live we must ghost it */
		if (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
		{
			/* FIXME: This doesn't make much sense in the case we are shutting down */
			TexMgrGhostTexture(gc, psTex);
		}
		else
		{
#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent -= psTex->psMemInfo->uAllocSize;
#endif
			GLES1FREEDEVICEMEM_HEAP(gc, psTex->psMemInfo);

			psTex->psMemInfo = IMG_NULL;
		}		
	}

	KRM_RemoveResourceFromAllLists(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource);

	TextureRemoveResident(gc, psTex);

#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
	/*FIXME: Ghosting? */
	if (psTex->psExtTexState)
	{
		GLES1Free(gc, psTex->psExtTexState);
		psTex->psExtTexState = IMG_NULL;
	}
#endif


	GLES1Free(gc, psTex);
}

/***********************************************************************************
 Function Name      : DisposeTexObj
 Inputs             : gc, psItem, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Generic texture object free function; called from names.c
************************************************************************************/
static IMG_VOID DisposeTexObj(GLES1Context *gc, GLES1NamedItem *psItem, IMG_BOOL bIsShutdown)
{
	GLESTexture *psTex = (GLESTexture *)psItem;

	PVR_UNREFERENCED_PARAMETER(bIsShutdown);
	GLES1_ASSERT(bIsShutdown || (psTex->sNamedItem.ui32RefCount == 0));

	FreeTexture(gc, psTex);
}


/***********************************************************************************
 Function Name      : CreateDummyTexture
 Inputs             : gc, psTexMgr
 Outputs            : -
 Returns            : -
 Description        : Creates dummy texture for use when we run out of texture memory
					 (1x1 LUMINANCE_8 texture) or when a program samples an
					  inconsistent texture (1x1 LUMINANCE_ALPHA_8 texture)
************************************************************************************/
static IMG_BOOL CreateDummyTextures(GLES1Context *gc, GLES1TextureManager *psTexMgr)
{
	IMG_UINT8 *pui8TextureData;

	if(GLES1ALLOCDEVICEMEM(gc->ps3DDevData,
							gc->psSysContext->hGeneralHeap,
							PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
							4,
							EURASIA_CACHE_LINE_SIZE,
							&psTexMgr->psWhiteDummyTexture) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"CreateDummyTexture: Can't create our white dummy texture"));

		return IMG_FALSE;
	}

	pui8TextureData = (IMG_UINT8 *)psTexMgr->psWhiteDummyTexture->pvLinAddr;
	pui8TextureData[0] = 0xff;

#if (defined(DEBUG) || defined(TIMING))
	ui32TextureMemCurrent += psTexMgr->psWhiteDummyTexture->uAllocSize;
	
	if (ui32TextureMemCurrent>ui32TextureMemHWM)
	{
		ui32TextureMemHWM = ui32TextureMemCurrent;
	}
#endif

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : DestroyDummyTexture
 Inputs             : gc, psTexMgr
 Outputs            : -
 Returns            : -
 Description        : Destroy the dummy textures of the manager
************************************************************************************/
static IMG_BOOL DestroyDummyTextures(GLES1Context *gc, GLES1TextureManager *psTexMgr)
{
	IMG_BOOL bRetVal;

#if (defined(DEBUG) || defined(TIMING))
	ui32TextureMemCurrent -= psTexMgr->psWhiteDummyTexture->uAllocSize;	
#endif

	bRetVal = IMG_TRUE;

	if(GLES1FREEDEVICEMEM(gc->ps3DDevData, psTexMgr->psWhiteDummyTexture) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"DestroyDummyTexture: Can't free our white dummy texture"));

		bRetVal = IMG_FALSE;
	}

	return bRetVal;
}


/***********************************************************************************
 Function Name      : CreateTextureManager
 Inputs             : gc
 Outputs            : -
 Returns            : Success
 Description        : Returns a new texture manager. Does not update the pointer in the context.
************************************************************************************/
IMG_INTERNAL GLES1TextureManager *CreateTextureManager(GLES1Context *gc)
{
	GLES1TextureManager *psTexMgr;

	psTexMgr = GLES1Calloc(gc, sizeof(GLES1TextureManager));

	if(!psTexMgr)
	{
		return IMG_NULL;
	}

	psTexMgr->ui32GhostMem = 0;

	/* Initialize the frame texture manager */
	if(!KRM_Initialize(	&psTexMgr->sKRM,
						KRM_TYPE_3D, 
						IMG_TRUE,
						gc->psSharedState->hSecondaryLock, 
						gc->ps3DDevData,
						gc->psSysContext->sHWInfo.sMiscInfo.hOSGlobalEvent,
						ReclaimTextureMemKRM, 
						IMG_FALSE, 
						DestroyTextureGhostKRM))
	{
		ReleaseTextureManager(gc, psTexMgr);
		return IMG_NULL;
	}

	if(!CreateDummyTextures(gc, psTexMgr))
	{
		ReleaseTextureManager(gc, psTexMgr);
		return IMG_NULL;
	}

	return psTexMgr;
}

/***********************************************************************************
 Function Name      : InitTextureState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Binds the default object (ui32Name 0) and sets up default
					  texture formats.
************************************************************************************/
static IMG_BOOL InitTextureState(GLES1Context *gc)
{
	IMG_UINT32 i, j;

	/* Init texture unit state */
	for (i=0; i < GLES1_MAX_TEXTURE_UNITS; i++) 
	{
		for(j=0; j < GLES1_TEXTURE_TARGET_MAX; j++)
		{
			/* Set up default binding */
			if(BindTexture(gc, i, j, 0) != IMG_TRUE)
			{
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL) || defined (GLES1_EXTENSION_TEXTURE_STREAM)

static IMG_BOOL SetupExternalTextureState(GLES1Context *gc, GLES1ExternalTexState *psExtTexState,
                                          GLESTexture *psTex, PDS_TEXTURE_IMAGE_UNIT *psTextureUnitState,
                                          IMG_UINT32 *ui32ImageUnit, IMG_DEV_VIRTADDR uBufferAddr,
                                          IMG_UINT32 ui32PixelHeight, IMG_UINT32 ui32ByteStride
#if defined(SUPPORT_NV12_FROM_2_HWADDRS)
										  ,IMG_UINT32 ui32HWSurfaceAddress2
#endif
											)


{
	GLESTextureParamState *psParams = &psTex->sState;
	IMG_UINT32 ui32InitStrideStateWord0 = psParams->ui32StateWord0;

	switch(psTex->psFormat->ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY:
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV:
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY:
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU:
		case PVRSRV_PIXEL_FORMAT_NV12:
		case PVRSRV_PIXEL_FORMAT_YV12:
		{
			CopyYUVRegisters(gc, &(psExtTexState->sYUVRegisters));
			break;
		}
		default:
			break;
	}

	if ((psExtTexState->aui32TexStrideFlag & 0x3U) != 0)
	{
		/* the psParams->ui32StateWord0 needs to be adjusted for the stride as some shared bits set before need to be reset */

		ui32InitStrideStateWord0 = 
#if defined(EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT)
								EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & 
								EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK &
#if defined(EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK)
								EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK &
#endif

#else /* EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT */

#if defined(SGX_FEATURE_VOLUME_TEXTURES)
								EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & 
								EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK &
	
#else /* SGX_FEATURE_VOLUME_TEXTURES */
								EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & 
								EURASIA_PDS_DOUTT0_STRIDELO2_CLRMSK &

#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
								EURASIA_PDS_DOUTT0_STRIDEEX0_CLRMSK &
#endif

#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
								EURASIA_PDS_DOUTT0_STRIDEEX0_CLRMSK & 
#endif
								EURASIA_PDS_DOUTT0_STRIDELO2_CLRMSK &
								EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK &
#endif /* SGX_FEATURE_VOLUME_TEXTURES */
#endif /* defined(EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT) */
								ui32InitStrideStateWord0;
	}

	if ((psExtTexState->aui32TexStrideFlag & 0x1U) != 0)
	{
		/* if use the stride */
		psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord0 = ui32InitStrideStateWord0 | psExtTexState->aui32TexWord0[0];
	}
	else
	{
		psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord0 = psParams->ui32StateWord0;
	}
	psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord1 = psExtTexState->aui32TexWord1[0];
	psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord2 = 
		(uBufferAddr.uiAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
	psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord3 = psParams->ui32StateWord3;
#endif
				
	/* YUV 420  */
	if(psTex->psFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_NV12 || psTex->psFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12)
	{
		/* Y plane is 8bit per pixel */
		IMG_UINT32 ui32BufferAddress = uBufferAddr.uiAddr + (ui32ByteStride * ui32PixelHeight * 1);

#if defined(SUPPORT_NV12_FROM_2_HWADDRS)
		if(ui32BufferAddress != ui32HWSurfaceAddress2)
		{
			PVR_DPF((PVR_DBG_MESSAGE,"Y and UV offsets do not match for buffer %p: %d != %d \n "
						"Expect issues with OMAP4470 (SGX544) and later chipsets\n",
						psExtTexState, ui32BufferAddress, ui32HWSurfaceAddress2));
		}

		if(psTex->psFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_NV12)
		{
			ui32BufferAddress = ui32HWSurfaceAddress2;
		}
#endif						
		/* Extra unit inserted for second plane - VU */
		(*ui32ImageUnit)++;

		if ((psExtTexState->aui32TexStrideFlag & 0x2U) != 0)
		{
			/* if use the stride */
			psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord0 = ui32InitStrideStateWord0 | psExtTexState->aui32TexWord0[1];
		}
		else
		{
			psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord0 = psParams->ui32StateWord0;
		}
		psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord1 = psExtTexState->aui32TexWord1[1];
		psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord2 = (ui32BufferAddress >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
																	EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
#if defined(SGX545)
		psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord3 = (1 << EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT);
#else
		psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord3 = 0;
#endif
#endif

		if(psTex->psFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12)
		{
			/* Extra unit inserted for third plane - V */
			(*ui32ImageUnit)++;

			ui32BufferAddress += (ui32ByteStride * ui32PixelHeight / 4);

			if ((psExtTexState->aui32TexStrideFlag & 0x2U) != 0)
			{
				/* if use the stride */
				psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord0 = ui32InitStrideStateWord0 | psExtTexState->aui32TexWord0[2];
			}
			else
			{
				psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord0 = psParams->ui32StateWord0;
			}
			psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord1 = psExtTexState->aui32TexWord1[2];
			psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord2 = (ui32BufferAddress >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
																		EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
#if defined(SGX545)
			psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord3 = (1 << EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT);
#else
			psTextureUnitState[*ui32ImageUnit].ui32TAGControlWord3 = 0;
#endif
#endif
		}

	}

	return IMG_TRUE;
}
#endif /* GLES1_EXTENSION_EGL_IMAGE_EXTERNAL || GLES1_EXTENSION_TEXTURE_STREAM */

/***********************************************************************************
 Function Name      : CreateTextureState
 Inputs             : gc, 
 Outputs            : -
 Returns            : Success
 Description        : Initialises the texture manager and creates the default
 					  object (name 0).
************************************************************************************/
IMG_INTERNAL IMG_BOOL CreateTextureState(GLES1Context *gc)
{
	GLESTexture *psTex;
	IMG_UINT32 i;

	gc->psSharedState->psTextureManager = gc->psSharedState->psTextureManager;

	gc->sState.sTexture.psActive = &gc->sState.sTexture.asUnit[0];

	/*
	** Set up the texture object for the default texture.
	** Because the default texture is not shared, it is
	** not hung off of the psNamesArray structure.
	** For that reason we have to set up manually the refcount.
	*/
	for(i=0; i < GLES1_TEXTURE_TARGET_MAX; i++)
	{
		psTex = CreateTexture(gc, 0, i);

		if(!psTex)
		{
			PVR_DPF((PVR_DBG_ERROR,"Couldn't create default texture"));
			return IMG_FALSE;
		}

		psTex->sNamedItem.ui32RefCount = 1;

		GLES1_ASSERT(psTex->sNamedItem.ui32Name == 0);
		GLES1_ASSERT(psTex->sNamedItem.ui32RefCount == 1);
		gc->sTexture.psDefaultTexture[i] = psTex;
	}

	return InitTextureState(gc);
}


/***********************************************************************************
 Function Name      : SetupTexNameArray
 Inputs             : psNamesArray
 Outputs            : -
 Returns            : -
 Description        : Sets up names array for texture objects.
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupTexNameArray(GLES1NamesArray *psNamesArray)
{
	psNamesArray->pfnFree = DisposeTexObj;
}

/***********************************************************************************
 Function Name      : ReleaseTextureManager
 Inputs             : gc, psTexMgr
 Outputs            : -
 Returns            : -
 Description        : Frees the texture manager and associated memory - reaps the
					  active list for each frame of latency.
************************************************************************************/
IMG_INTERNAL IMG_VOID ReleaseTextureManager(GLES1Context *gc, GLES1TextureManager *psTexMgr)
{
	DestroyDummyTextures(gc, psTexMgr);

	/* Destroy all ghosts. Textures will be destroyed in FreeTextureState() */
	KRM_WaitForAllResources(&psTexMgr->sKRM, GLES1_DEFAULT_WAIT_RETRIES);
	KRM_DestroyUnneededGhosts(gc, &psTexMgr->sKRM);

	/* Destroy the manager itself */
	KRM_Destroy(gc, &psTexMgr->sKRM);
	GLES1Free(gc, psTexMgr);
}


/***********************************************************************************
 Function Name      : FreeTextureState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Unbinds all non-default textures and frees the default texture.
					  If no other contexts are sharing the ui32Name array, will delete
					  this also.
************************************************************************************/
IMG_INTERNAL IMG_BOOL FreeTextureState(GLES1Context *gc)
{
	IMG_UINT32 i, j;
	GLESTexture *psTex;
	IMG_BOOL bResult = IMG_TRUE;

	/* Unbind all non-default texture objects */
	for (i=0; i < GLES1_MAX_TEXTURE_UNITS; i++)
	{
		for(j=0; j < GLES1_TEXTURE_TARGET_MAX; j++)
		{
			if(BindTexture(gc, i, j, 0) != IMG_TRUE)
			{
				PVR_DPF((PVR_DBG_ERROR,"FreeTextureState: BindTexture %u,%u failed",i,j));

				bResult=IMG_FALSE;
			}
		}
	}

	for(i=0; i < GLES1_TEXTURE_TARGET_MAX; i++)
	{
		/* Free default texture object */
		psTex = gc->sTexture.psDefaultTexture[i];
		psTex->sNamedItem.ui32RefCount--;

		GLES1_ASSERT(psTex->sNamedItem.ui32RefCount == 0);

		FreeTexture(gc, psTex);
		gc->sTexture.psDefaultTexture[i] = IMG_NULL;
	}

	return bResult;
}


/***********************************************************************************
 Function Name      : SetupTextureState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Makes textures resident if valid and not already resident.
************************************************************************************/
IMG_INTERNAL IMG_BOOL SetupTextureState(GLES1Context *gc)
{
	GLES1TextureManager *psTexMgr = gc->psSharedState->psTextureManager;
	IMG_UINT32 ui32ImageUnitEnables, ui32Unit, ui32ImageUnit;
	PDS_TEXTURE_IMAGE_UNIT *psTextureUnitState;
	GLESShaderTextureState *psTextureState;
	IMG_BOOL bChanged;

	gc->ui32NumImageUnitsActive = 0;

	psTextureState = &gc->sPrim.sTextureState;
	
	psTextureUnitState = psTextureState->asTextureImageUnits;

	psTextureState->bSomeTexturesWereGhosted = IMG_FALSE;
	
	ui32ImageUnit = 0;

	ui32ImageUnitEnables = 0;

	bChanged = IMG_FALSE;

	for(ui32Unit=0; ui32Unit < GLES1_MAX_TEXTURE_UNITS; ui32Unit++)
	{
		IMG_BOOL bEnabled;
#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		if (gc->ui32RasterEnables & (GLES1_RS_TEXTURE0_STREAM_ENABLE << ui32Unit))
		{
			if(gc->sTexture.aui32CurrentTarget[ui32Unit] != GLES1_TEXTURE_TARGET_STREAM)
			{
			    gc->sTexture.aui32CurrentTarget[ui32Unit] = GLES1_TEXTURE_TARGET_STREAM;

				bChanged = IMG_TRUE;
			}

			bEnabled = IMG_TRUE;
		}
		else
#endif
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(gc->ui32RasterEnables & (GLES1_RS_CEMTEXTURE0_ENABLE << ui32Unit))
		{
			if(gc->sTexture.aui32CurrentTarget[ui32Unit] != GLES1_TEXTURE_TARGET_CEM)
			{
				gc->sTexture.aui32CurrentTarget[ui32Unit] = GLES1_TEXTURE_TARGET_CEM;

				bChanged = IMG_TRUE;
			}

			bEnabled = IMG_TRUE;
		}
		else 
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
		if(gc->ui32RasterEnables & (GLES1_RS_2DTEXTURE0_ENABLE << ui32Unit))
		{
			if(gc->sTexture.aui32CurrentTarget[ui32Unit] != GLES1_TEXTURE_TARGET_2D)
			{
				gc->sTexture.aui32CurrentTarget[ui32Unit] = GLES1_TEXTURE_TARGET_2D;

				bChanged = IMG_TRUE;
			}

			bEnabled = IMG_TRUE;
		}
		else
		{
			bEnabled = IMG_FALSE;
		}

		if(bEnabled)
		{
			GLESTexture *psTex;
			IMG_BOOL bUseWhiteDummyTexture;
			GLESTextureParamState *psParams;
			const IMG_UINT32 ui32DAdjust = EURASIA_PDS_DOUTT_DADJUST_ZERO_UINT;
			const IMG_UINT32 ui32StateWord0ClearMask =  EURASIA_PDS_DOUTT0_MIPMAPCLAMP_CLRMSK	&
														EURASIA_PDS_DOUTT0_MAGFILTER_CLRMSK		&
														EURASIA_PDS_DOUTT0_MINFILTER_CLRMSK		&
														EURASIA_PDS_DOUTT0_ANISOCTL_CLRMSK		&
#if defined(EURASIA_PDS_DOUTT0_TEXFEXT)
														~EURASIA_PDS_DOUTT0_TEXFEXT      		&
#else
														~EURASIA_PDS_DOUTT0_CHANREPLICATE		&
#endif /* defined(EURASIA_PDS_DOUTT0_TEXFEXT) */
														~EURASIA_PDS_DOUTT0_MIPFILTER;

			psTex = gc->sTexture.apsBoundTexture[ui32Unit][gc->sTexture.aui32CurrentTarget[ui32Unit]];

			psParams = &psTex->sState;
	
			bUseWhiteDummyTexture = IMG_FALSE;
			
			if(psTex->ui32NumRenderTargets > 0)
			{
				/*
				 * At least one of the mipmap levels is a render target.
				 * Kick the 3D core before using the texture as a source.
				 */
				FlushUnflushedTextureRenders(gc, psTex);
			}

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)|| defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
			if(gc->sTexture.aui32CurrentTarget[ui32Unit] == GLES1_TEXTURE_TARGET_STREAM)
			{
				if(psTex->psFormat)
				{
					psTex->bResidence = IMG_TRUE;
				}
				else
				{
					/*texture inconsistent, should look like it is disabled*/
					continue;
				}
			}
			else
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)*/
			if(IsTextureConsistent(gc, psTex, IMG_TRUE) == GLES1_TEX_CONSISTENT)
			{
				if(!TextureMakeResident(gc, psTex))
				{
					bUseWhiteDummyTexture = IMG_TRUE;
					bChanged = IMG_TRUE;
					SetError(gc, GL_OUT_OF_MEMORY);
				}

				if(gc->sTexture.apsCurrentFormat[ui32Unit] != psTex->psFormat)
				{
					gc->sTexture.apsCurrentFormat[ui32Unit] = psTex->psFormat;

					bChanged = IMG_TRUE;
				}
			}
			else
			{
				/*texture inconsistent, should look like it is disabled*/
				continue;
			}

			if(bUseWhiteDummyTexture)
			{
				IMG_UINT32 ui32DummyTexAddr, ui32StateWord0;

				ui32StateWord0 = psParams->ui32StateWord0 & ui32StateWord0ClearMask;

				ui32StateWord0 |= (EURASIA_PDS_DOUTT0_NOTMIPMAP | 
								   EURASIA_PDS_DOUTT0_MINFILTER_POINT |
								   EURASIA_PDS_DOUTT0_MAGFILTER_POINT |
#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
								   EURASIA_PDS_DOUTT0_CHANREPLICATE	  |
#endif
								  ((ui32DAdjust << EURASIA_PDS_DOUTT0_DADJUST_SHIFT) & ~EURASIA_PDS_DOUTT0_DADJUST_CLRMSK));

				ui32DummyTexAddr = (psTexMgr->psWhiteDummyTexture->sDevVAddr.uiAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT);
	
				psTextureUnitState[ui32ImageUnit].ui32TAGControlWord0 = ui32StateWord0;
				psTextureUnitState[ui32ImageUnit].ui32TAGControlWord1 = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
				psTextureUnitState[ui32ImageUnit].ui32TAGControlWord2 = ui32DummyTexAddr << EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)

#if defined(SGX545)
				psTextureUnitState[ui32ImageUnit].ui32TAGControlWord3 = (1 << EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT);
#else
				psTextureUnitState[ui32ImageUnit].ui32TAGControlWord3 = 0;
#endif
#if defined(SGX_FEATURE_8BIT_DADJUST)
				/* 2 most significant bits of lod bias are in doutt3 */
				psTextureUnitState[ui32ImageUnit].ui32TAGControlWord3 |= ((ui32DAdjust >> EURASIA_PDS_DOUTT3_DADJUST_ALIGNSHIFT) << EURASIA_PDS_DOUTT3_DADJUST_SHIFT) & ~EURASIA_PDS_DOUTT3_DADJUST_CLRMSK;
#endif

#endif
			}
			else
			{
				psParams->ui32StateWord0 &= ui32StateWord0ClearMask;

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
				/*
				 * Set the mip level clamp to 0 if:
				 * - The texture is CEM
				 * - The texture in memory is mipmapped
				 * - The min filter is not mipmap
				*/
				if((psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM) &&
				   (psTex->ui32HWFlags & GLES1_MIPMAP) &&
				  !(GLES1_IS_MIPMAP(psParams->ui32MinFilter)))
				{
				   psParams->ui32StateWord0 |= (psParams->ui32MinFilter & ~EURASIA_PDS_DOUTT0_MINFILTER_CLRMSK);
				   psParams->ui32StateWord0 |= (EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MIN | EURASIA_PDS_DOUTT0_MIPFILTER);
				}
				else
#endif//defined(GLES1_EXTENSION_TEX_CUBE_MAP)
				{
					psParams->ui32StateWord0 |= psParams->ui32MinFilter;
				}	

				psParams->ui32StateWord0 |= psParams->ui32MagFilter;

				psParams->ui32StateWord0 |= ((ui32DAdjust << EURASIA_PDS_DOUTT0_DADJUST_SHIFT) & ~EURASIA_PDS_DOUTT0_DADJUST_CLRMSK);
	
				psParams->ui32StateWord0 |= asSGXPixelFormat[psTex->psFormat->ePixelFormat].aui32TAGControlWords[0][0];
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
#if defined(SGX545)
				psParams->ui32StateWord3 |= (1 << EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT);
#else
				psParams->ui32StateWord3 = asSGXPixelFormat[psTex->psFormat->ePixelFormat].aui32TAGControlWords[0][3];
#endif

#if defined(SGX_FEATURE_8BIT_DADJUST)
				/* 2 most significant bits of lod bias are in doutt3 */
				psParams->ui32StateWord3 |= (((ui32DAdjust >> EURASIA_PDS_DOUTT3_DADJUST_ALIGNSHIFT) << 
											EURASIA_PDS_DOUTT3_DADJUST_SHIFT) & ~EURASIA_PDS_DOUTT3_DADJUST_CLRMSK);
#endif

#endif
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
				if (psTex->psEGLImageTarget &&
					gc->sTexture.aui32CurrentTarget[ui32Unit] == GLES1_TEXTURE_TARGET_STREAM)
				{
					EGLImage *psEGLImage;
					IMG_DEV_VIRTADDR uBufferAddr;


					psEGLImage = psTex->psEGLImageTarget;
					uBufferAddr.uiAddr = psEGLImage->ui32HWSurfaceAddress;


					SetupExternalTextureState(gc, psTex->psExtTexState, psTex, psTextureUnitState,
					                          &ui32ImageUnit, uBufferAddr, psEGLImage->ui32Height,
					                          psEGLImage->ui32Stride
#if defined(SUPPORT_NV12_FROM_2_HWADDRS)
												,psEGLImage->ui32HWSurfaceAddress2
#endif
												);
					bChanged = IMG_TRUE;


				}
				else
#endif
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
				if(gc->sTexture.aui32CurrentTarget[ui32Unit] == GLES1_TEXTURE_TARGET_STREAM)
				{
					GLES1StreamDevice *psBufferDevice = psTex->psBufferDevice;
					GLES1DeviceBuffer *psStreamBuffer;
					IMG_UINT32 ui32PixelHeight, ui32ByteStride;
					IMG_DEV_VIRTADDR uBufferAddr;

					GLES1_ASSERT(psBufferDevice);

					psStreamBuffer = &psBufferDevice->psBuffer[psTex->ui32BufferOffset];

					ui32PixelHeight = psStreamBuffer->ui32PixelHeight;
					ui32ByteStride = psStreamBuffer->ui32ByteStride;

					

					uBufferAddr = psBufferDevice->psBuffer[psTex->ui32BufferOffset].psBufferSurface->sDevVAddr;

					SetupExternalTextureState(gc, psBufferDevice->psExtTexState, psTex,
					                          psTextureUnitState, &ui32ImageUnit, uBufferAddr,
					                          ui32PixelHeight, ui32ByteStride);

				}
				else
#endif

				{

					IMG_UINT32 ui32StateWord0 = psParams->ui32StateWord0;
					IMG_UINT32 ui32StateWord1 = psParams->ui32StateWord1;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
					if(psTex->psEGLImageTarget && !psTex->psEGLImageTarget->bTwiddled)
					{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
						SetupEGLImageArbitraryStride(psTex, &ui32StateWord0, &ui32StateWord1);
#endif
					}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

					psTextureUnitState[ui32ImageUnit].ui32TAGControlWord0 = ui32StateWord0;
					psTextureUnitState[ui32ImageUnit].ui32TAGControlWord1 = ui32StateWord1;
					psTextureUnitState[ui32ImageUnit].ui32TAGControlWord2 = psParams->ui32StateWord2;
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
					psTextureUnitState[ui32ImageUnit].ui32TAGControlWord3 = psParams->ui32StateWord3;
#endif
				}

				if(psTex->bHasEverBeenGhosted)
				{
					psTextureState->bSomeTexturesWereGhosted = IMG_TRUE;
				}
			}

			ui32ImageUnit++;

			ui32ImageUnitEnables |= (1UL << ui32Unit);

			gc->ui32TexImageUnitsEnabled[gc->ui32NumImageUnitsActive++] = ui32Unit;
		}
	}

	if(gc->ui32ImageUnitEnables != ui32ImageUnitEnables)
	{
		gc->ui32ImageUnitEnables = ui32ImageUnitEnables;

		bChanged = IMG_TRUE;		
	}

	return bChanged;
}

/******************************************************************************
 End of file (texmgmt.c)
******************************************************************************/
