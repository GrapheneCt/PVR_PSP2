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
 *****************************************************************************/

#include <kernel.h>

#include "context.h"

#if (defined(DEBUG) || defined(TIMING))
IMG_INTERNAL IMG_UINT32 ui32TextureMemCurrent = 0;
IMG_INTERNAL IMG_UINT32  ui32TextureMemHWM = 0;
#endif

#define GLES2_IS_MIPMAP(ui32Filter) \
(((ui32Filter) &~EURASIA_PDS_DOUTT0_MIPMAPCLAMP_CLRMSK) != EURASIA_PDS_DOUTT0_NOTMIPMAP)

/***********************************************************************************
 Function Name      : FlushUnflushedTextureRenders
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : Succes//Failure
 Description        : Flushes outstanding renders to a texture
************************************************************************************/
static IMG_BOOL FlushUnflushedTextureRenders(GLES2Context *gc, GLES2Texture *psTex)
{
	GLES2SurfaceFlushList **ppsFlushList, *psFlushItem;
	EGLRenderSurface *psRenderSurface;
	GLES2Texture *psFlushTex;
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

		GLES_ASSERT(psRenderSurface);

		if(psTex == psFlushTex)
		{
#if defined(PDUMP)
			PDumpTexture(gc, psTex);
#endif
			if(!FlushRenderSurface(gc, psRenderSurface, (GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_IGNORE_FLUSHLIST)))
			{
				bResult = IMG_FALSE;

				ppsFlushList = &psFlushItem->psNext;
			}
			else
			{
				*ppsFlushList = psFlushItem->psNext;

				GLES2Free(IMG_NULL, psFlushItem);

				/*
				 * Remove remaining references to the same render surface
				 */
				while(*ppsFlushList)
				{
					psFlushItem = *ppsFlushList;

					if(psFlushItem->psRenderSurface == psRenderSurface)
					{
						*ppsFlushList = psFlushItem->psNext;

						GLES2Free(IMG_NULL, psFlushItem);
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
static IMG_BOOL IsTextureConsistentWithMipMaps(const GLES2Texture *psTex)
{
	const GLES2MipMapLevel *psLevel;
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


#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
/***********************************************************************************
 Function Name      : TexStreamUnbindBufferDevice
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : -
 Description        : Unbind buffer device from the texture stream
************************************************************************************/
static IMG_VOID TexStreamUnbindBufferDevice(GLES2Context *gc, IMG_VOID *hBufferDevice)
{
    GLES2StreamDevice *psBufferDevice = (GLES2StreamDevice *)hBufferDevice;

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
		    GLES2StreamDevice *psGCBufferDevice = gc->psBufferDevice;
		    GLES2StreamDevice *psHeadGCBufferDevice = gc->psBufferDevice;
		    GLES2StreamDevice *psPrevGCBufferDevice = gc->psBufferDevice;

			/* Then the buffer class device can be unmapped and closed. */
			for(i = 0; i < psBufferDevice->sBufferInfo.ui32BufferCount; i++)
			{
				PVRSRVUnmapDeviceClassMemory (&gc->psSysContext->s3D, psBufferDevice->psBuffer[i].psBufferSurface);
			}

			if (psBufferDevice->psBuffer)
			{
				GLES2Free(IMG_NULL, psBufferDevice->psBuffer);
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

			/* Free and reinitialise this buffer device node */
			GLES2Free(IMG_NULL, psBufferDevice->psExtTexState);
			GLES2Free(IMG_NULL, psBufferDevice);		
				
			psBufferDevice = IMG_NULL;
		}
	}
}
#endif /* defined(GLES2_EXTENSION_TEXTURE_STREAM) */


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
	GLES2Texture     *psTex = (GLES2Texture*)((IMG_UINTPTR_T)psResource -offsetof(GLES2Texture, sResource));
	GLES2Context	 *gc = (GLES2Context *)pvContext;
	IMG_UINT32       ui32Lod, ui32Face;
	GLES2MipMapLevel *psLevel;

	GLES_ASSERT(psResource);

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
			for(ui32Lod = 0; ui32Lod < GLES2_MAX_TEXTURE_MIPMAP_LEVELS; ui32Lod++)
			{
				psLevel = &psTex->psMipLevel[ui32Lod];

				/* If the mipmap has already been loaded it must be read back */
				if(psLevel->pui8Buffer == GLES2_LOADED_LEVEL)
				{
					/* This code was copied from glTexSubImage */
					IMG_UINT32 ui32Stride     = psLevel->ui32Width * psLevel->psTexFormat->ui32TotalBytesPerTexel;
					IMG_UINT32 ui32BufferSize = ui32Stride * psLevel->ui32Height;
					IMG_UINT8  *pui8Src       = GLES2MallocHeapUNC(gc, ui32BufferSize);

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
					FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psLevel, 
											GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_WAIT_FOR_3D);
					ReadBackTextureData(gc, psTex, ui32Face, ui32Lod, pui8Src);
					psLevel->pui8Buffer = pui8Src;
				}
			}
		}
		while( (psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM) && (++ui32Face < GLES2_TEXTURE_CEM_FACE_MAX) );

		/* After all mipmaps have been read back, free the texture's device mem and mark it as non-resident */
		GLES2FREEDEVICEMEM_HEAP(gc, psTex->psMemInfo);
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
	GLES2Ghost *psGhost = (GLES2Ghost*)((IMG_UINTPTR_T)psResource -offsetof(GLES2Ghost, sResource));
	GLES2Context *gc = (GLES2Context *)pvContext;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psGhost->hImage)
	{
		KEGLUnbindImage(psGhost->hImage);
	}
	else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
	if(psGhost->hBufferDevice)
	{
		TexStreamUnbindBufferDevice(gc, psGhost->hBufferDevice);
	}
	else
#endif /* defined(GLES2_EXTENSION_TEXTURE_STREAM) */
	{
		GLES2FREEDEVICEMEM_HEAP(gc, psGhost->psMemInfo);
	}
	
	gc->psSharedState->psTextureManager->ui32GhostMem -= psGhost->ui32Size;

	GLES2Free(IMG_NULL, psGhost);
}

/***********************************************************************************
 Function Name      : CreateTextureMemory
 Inputs             : gc, psTex
 Outputs            : psTex
 Returns            : Success of memory create
 Description        : Allocates physical memory for a texture and fills in the 
					  Meminfo structure.
************************************************************************************/
IMG_INTERNAL IMG_BOOL  CreateTextureMemory(GLES2Context *gc, GLES2Texture *psTex)
{
	GLES2TextureManager *psTexMgr         = gc->psSharedState->psTextureManager;
	const GLES2TextureFormat *psTexFormat = psTex->psFormat;
	IMG_UINT32 ui32TexSize = 0;
	IMG_UINT32 ui32TexAlign = EURASIA_CACHE_LINE_SIZE;
	IMG_UINT32 ui32BytesPerTexel = psTexFormat->ui32TotalBytesPerTexel;
	IMG_UINT32 ui32BytesPerChunk = ui32BytesPerTexel / psTexFormat->ui32NumChunks;
	IMG_UINT32 i;
	PVRSRV_ERROR eError;

	GLES2_TIME_START(GLES2_TIMER_TEXTURE_ALLOCATE_TIME);

	/* Special case of chunks with different sizes */
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
	if(psTexFormat==&TexFormatFloatDepthU8Stencil)
	{
		ui32BytesPerChunk = 4;
	}
	else
#endif
	if(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_B16G16R16F)
	{
		ui32BytesPerChunk = 4;
	}

	if(psTex->ui32HWFlags & GLES2_NONPOW2)
	{
		IMG_UINT32 ui32MipChainSize;

		ui32MipChainSize = GetNPOTMipMapOffset(psTex->ui32NumLevels, psTex);
		
		ui32TexSize = ui32BytesPerTexel * ui32MipChainSize;

		psTex->ui32ChunkSize = ui32BytesPerChunk * ui32MipChainSize;
	}
	else
	{
		IMG_UINT32 ui32MipChainSize;
		IMG_UINT32 ui32TexWord1 = psTex->sState.aui32StateWord1[0];
		IMG_UINT32 ui32TopUSize;
		IMG_UINT32 ui32TopVSize;

		if(((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK) == EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE) ||
			((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK) == EURASIA_PDS_DOUTT1_TEXTYPE_TILED))
		{
			ui32TopUSize = ((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK) >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT) + 1;
			ui32TopVSize = ((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) + 1;
		}
		else
		{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
			ui32TopUSize = 1U << ((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
			ui32TopVSize = 1U << ((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
			ui32TopUSize = 1 + ((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
			ui32TopVSize = 1 + ((ui32TexWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif
		}

		/* If these fail something is wrong with IsLegalRangeTex or the constants */
		GLES_ASSERT(ui32TopUSize <= GLES2_MAX_TEXTURE_SIZE);
		GLES_ASSERT(ui32TopVSize <= GLES2_MAX_TEXTURE_SIZE);

		if(psTex->ui32HWFlags & GLES2_COMPRESSED)
		{
			/* PVRTC compressed texture */
			IMG_BOOL bIs2bpp;

			/* Is it 2-bits-per-pixel? */
			bIs2bpp = (IMG_BOOL)( (psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTC2) || 
								(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCII2));

			ui32TexSize = ui32BytesPerTexel *
				GetCompressedMipMapOffset(psTex->ui32NumLevels, ui32TopUSize, ui32TopVSize, bIs2bpp);

			if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
			{
				if(psTex->ui32HWFlags & GLES2_MIPMAP)
				{
					if(ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)
					{
						ui32TexSize = ALIGNCOUNT(ui32TexSize, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
					}
				}
				ui32TexSize *= 6;
			}
		}
		else
		{
			/* Mipmapped non-compressed texture */
			ui32MipChainSize = GetMipMapOffset(psTex->ui32NumLevels, ui32TopUSize, ui32TopVSize);

			if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
			{
				if((psTex->ui32HWFlags & GLES2_MIPMAP) && 
					(((ui32BytesPerTexel == 1) && (ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
					(ui32TopUSize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP)))
				{
					IMG_UINT32 ui32FaceSize;

					/* Chunk size includes alignment to next chunk */
					psTex->ui32ChunkSize = ui32BytesPerChunk * ui32MipChainSize;
					psTex->ui32ChunkSize = ALIGNCOUNT(psTex->ui32ChunkSize, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
					psTex->ui32ChunkSize *= 6;

					ui32TexSize = psTex->ui32ChunkSize;

					/* Already worked out 1st chunk size */
					for(i=1; i < psTexFormat->ui32NumChunks; i++)
					{
						/* Each face is bytesperchunk * mipchain size */
						ui32FaceSize = ui32BytesPerChunk * ui32MipChainSize;

						/* Align each face to facealignment requirements */
						ui32FaceSize = ALIGNCOUNT(ui32FaceSize, EURASIA_TAG_CUBEMAP_FACE_ALIGN);

						ui32TexSize += (ui32FaceSize * 6);
					}
				}
				else
				{
					psTex->ui32ChunkSize = ui32BytesPerChunk * ui32MipChainSize * 6;
					ui32TexSize = ui32BytesPerTexel * ui32MipChainSize * 6;
				}
			}
			else
			{
				ui32TexSize = ui32BytesPerTexel * ui32MipChainSize;
				psTex->ui32ChunkSize = ui32BytesPerChunk * ui32MipChainSize;
			}
		}
	}

	eError = GLES2ALLOCDEVICEMEM_HEAP(gc,
		PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MAP_GC_MMU,
		ui32TexSize,
		ui32TexAlign,
		&psTex->psMemInfo);

	if (eError != PVRSRV_OK)
	{
		eError = GLES2ALLOCDEVICEMEM_HEAP(gc,
			PVRSRV_MEM_READ | PVRSRV_MEM_WRITE,
			ui32TexSize,
			ui32TexAlign,
			&psTex->psMemInfo);
	}

	if(eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_WARNING,"CreateTextureMemory: Reaping active textures"));

		KRM_DestroyUnneededGhosts(gc, &psTexMgr->sKRM);
		KRM_ReclaimUnneededResources(gc, &psTexMgr->sKRM);

		eError = GLES2ALLOCDEVICEMEM_HEAP(gc,
			PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MAP_GC_MMU,
			ui32TexSize,
			ui32TexAlign,
			&psTex->psMemInfo);

		if (eError != PVRSRV_OK)
		{
			eError = GLES2ALLOCDEVICEMEM_HEAP(gc,
				PVRSRV_MEM_READ | PVRSRV_MEM_WRITE,
				ui32TexSize,
				ui32TexAlign,
				&psTex->psMemInfo);
		}

		if(eError != PVRSRV_OK)
		{
/* PRQA S 3332 1 */ /* Override QAC suggestion and use this macro. */
			{
				/* Could do Load store render to clear active list */
				GLES2_TIME_STOP(GLES2_TIMER_TEXTURE_ALLOCATE_TIME);
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

	GLES2_TIME_STOP(GLES2_TIMER_TEXTURE_ALLOCATE_TIME);

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
IMG_INTERNAL IMG_BOOL SetupTextureRenderTargetControlWords(GLES2Context *gc, GLES2Texture *psTex)
{
	GLES2TextureParamState   *psParams = &psTex->sState;
	const GLES2TextureFormat *psTexFormat = psTex->psMipLevel[0].psTexFormat;
	IMG_UINT32               i, ui32Width, ui32Height;
	IMG_UINT32               ui32NumLevels, ui32FormatFlags = 0, ui32TexTypeSize = 0;
	IMG_UINT32               aui32StateWord1[GLES2_MAX_CHUNKS];
	IMG_UINT32               ui32WidthLog2, ui32HeightLog2;
	IMG_BOOL                 bStateWord1Changed = IMG_FALSE;
	IMG_UINT32				 ui32OverloadTexLayout = gc->sAppHints.ui32OverloadTexLayout;

	/*                                                                */
	/* This part of the function is copied from IsTextureConsistent() */
	/*                                                                */

	GLES_ASSERT(psTex);
	GLES_ASSERT(psTexFormat);


#if defined(GLES2_EXTENSION_EGL_IMAGE)
	/* Don't need to setup control words or make imagetarget resident  */
	if(psTex->psEGLImageTarget)
	{
#if defined PDUMP
		/* PDump it immediately if needed */
		PDumpTexture(gc, psTex);
#endif
		return IMG_TRUE;
	}
#endif

	/* Compressed textures cannot be render targets. Neither can multichunk */
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
	if(psTexFormat!=&TexFormatFloatDepthU8Stencil)
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
	{
		GLES_ASSERT(psTexFormat->ui32TotalBytesPerTexel <= 4);
		GLES_ASSERT(psTexFormat->ui32NumChunks == 1);
	}

	/* NOTE: we are intentionally not messing with psTex->bLevelsConsitent */

	if(GLES2_IS_MIPMAP(psParams->ui32MinFilter))
	{
		ui32FormatFlags |= GLES2_MIPMAP;
	}

	for(i=0; i < psTexFormat->ui32NumChunks; i++)
	{
		aui32StateWord1[i] = asSGXPixelFormat[psTexFormat->ePixelFormat].aui32TAGControlWords[i][1];
	}

	ui32Width  = psTex->psMipLevel[0].ui32Width;
	ui32Height = psTex->psMipLevel[0].ui32Height;

	if(((ui32Width & (ui32Width - 1)) != 0) || ((ui32Height & (ui32Height - 1)) != 0))
	{
		ui32FormatFlags |= GLES2_NONPOW2;
		ui32WidthLog2  = psTex->psMipLevel[0].ui32WidthLog2 + 1;
		ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2 + 1;
	}
	else
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
	if(psTexFormat==&TexFormatFloatDepth)
	{
		ui32FormatFlags |= GLES2_NONPOW2;

		ui32WidthLog2  = psTex->psMipLevel[0].ui32WidthLog2 + 1;
		ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2 + 1;
	}
	else
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
	if(psTexFormat==&TexFormatFloatDepthU8Stencil)
	{
		ui32FormatFlags |= GLES2_NONPOW2;

		ui32WidthLog2  = psTex->psMipLevel[0].ui32WidthLog2 + 1;
		ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2 + 1;
	}
	else
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
	{
		ui32WidthLog2  = psTex->psMipLevel[0].ui32WidthLog2;
		ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2;
	}

	if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
	{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_CEM;
#else
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_CEM;
#endif

		GLES_ASSERT(ui32Width != 0 && ui32Height != 0);
	}
	else
	{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_2D;
#else
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_2D;
#endif
	}

	if(ui32FormatFlags & GLES2_MIPMAP)
	{
		ui32NumLevels = MAX(ui32WidthLog2, ui32HeightLog2) + 1;
	}
	else
	{
		ui32NumLevels = 1;
	}

	if(ui32FormatFlags & GLES2_NONPOW2)
	{
#if defined(GLES2_EXTENSION_EGL_IMAGE)
		if(psTex->psEGLImageSource)
		{
			ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE                     |
			                   ((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  |
			                   ((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) ;
		}
		else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
#if defined(SGX545)
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
		if(psTexFormat==&TexFormatFloatDepthU8Stencil)
		{
			ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				|
								((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
								((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
		}
		else
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
		if(psTexFormat==&TexFormatFloatDepth)
		{
			ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				|
								((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
								((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
		}
		else
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */
#endif /* defined(SGX545) */
		{
			/* Always use stride textures on NPOT mipmaps */
			if (ui32FormatFlags & GLES2_MIPMAP)
			{
				ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				|
									((psTex->psMipLevel[0].ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
									((psTex->psMipLevel[0].ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
			}
			else
			{
				switch((ui32OverloadTexLayout & GLES2_HINT_OVERLOAD_TEX_LAYOUT_NONPOW2_MASK) >> GLES2_HINT_OVERLOAD_TEX_LAYOUT_NONPOW2_SHIFT)
				{
					case GLES2_HINT_OVERLOAD_TEX_LAYOUT_STRIDE:
					{
						ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				|
										((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
										((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
						break;
					}

					case GLES2_HINT_OVERLOAD_TEX_LAYOUT_TWIDDLED:
#if !defined(SGX_FEATURE_HYBRID_TWIDDLING)
					{
						PVR_DPF((PVR_DBG_ERROR,"Invalid apphint for overloading texture layout - npot textures cannot be twiddled"));
					}
#endif /* !defined(SGX_FEATURE_HYBRID_TWIDDLING) */

					case GLES2_HINT_OVERLOAD_TEX_LAYOUT_DEFAULT:
					case GLES2_HINT_OVERLOAD_TEX_LAYOUT_TILED:
					default:
					{
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
						ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_2D				    |
#else
						ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_TILED				|
#endif /* defined(SGX_FEATURE_HYBRID_TWIDDLING) */ 
										((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
										((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
						break;
					}

				}
			}
		}
	}
	else
	{
		ui32OverloadTexLayout = (ui32OverloadTexLayout & GLES2_HINT_OVERLOAD_TEX_LAYOUT_POW2_MASK) >> GLES2_HINT_OVERLOAD_TEX_LAYOUT_POW2_SHIFT;
		
		
		if(((ui32FormatFlags & GLES2_COMPRESSED) != 0) || /* PRQA S 3356 */ /* Keep this test condition to keep the code flexsible. */
			(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_TWIDDLED) ||
			(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_DEFAULT))
		{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
			ui32TexTypeSize |=	(ui32WidthLog2 << EURASIA_PDS_DOUTT1_USIZE_SHIFT)	|
								(ui32HeightLog2 << EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else 
		    ui32TexTypeSize |= (((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  |
			                   (((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;
#endif 
		}
		else if(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_STRIDE)
		{
			ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				|
								((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
								((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
		}
		else if(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_TILED)
		{
			ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_TILED				|
								((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
								((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);

		}
	}

	/* Has the texture size or format changed? we need to ghost it */
	for(i=0; i < psTexFormat->ui32NumChunks; i++)
	{
		aui32StateWord1[i] |= ui32TexTypeSize;

		if(psParams->aui32StateWord1[i] != aui32StateWord1[i])
		{
			bStateWord1Changed = IMG_TRUE;
		}
	}

	if(psTex->psMemInfo)
	{
		GLES2TextureManager *psTexMgr = gc->psSharedState->psTextureManager;

		KRM_FlushUnKickedResource(&psTexMgr->sKRM, &psTex->sResource, (IMG_VOID *)gc, KickUnFlushed_ScheduleTA);

		/* If the state word 1 has changed or
		 * if the texture has changed from non-mipmap to mipmap
		 */
		if(bStateWord1Changed || 
		 (((ui32FormatFlags & GLES2_MIPMAP) != 0) && ((psTex->ui32HWFlags & GLES2_MIPMAP) == 0)))
		{
			/* ...then we ghost it and allocate new device mem for it. */
			if(!TexMgrGhostTexture(gc, psTex))
			{
				return IMG_FALSE;
			}
		}
	}

	/* Replace the old state words */
	for(i=0; i < psTexFormat->ui32NumChunks; i++)
	{
		psParams->aui32StateWord1[i] = aui32StateWord1[i];
	}

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
IMG_INTERNAL IMG_BOOL TexMgrGhostTexture(GLES2Context *gc, GLES2Texture *psTex)
{
	GLES2TextureManager *psTexMgr = gc->psSharedState->psTextureManager;
	GLES2Ghost *psGhost;

	psGhost = GLES2Calloc(gc, sizeof(GLES2Ghost));

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

#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
	if (psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_STREAM)
	{
		psGhost->ui32Size	   = ((psTex->psBufferDevice->psBuffer[psTex->ui32BufferOffset].ui32ByteStride) * 
								  (psTex->psBufferDevice->psBuffer[psTex->ui32BufferOffset].ui32PixelHeight));
		psGhost->hBufferDevice = (IMG_VOID *)psTex->psBufferDevice;
	}
	else
#endif
	{
#if defined(GLES2_EXTENSION_EGL_IMAGE)
	    if(psTex->psEGLImageSource)
		{
			GLES_ASSERT(psTex->psEGLImageSource->psMemInfo==psTex->psMemInfo)

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
		{
			psGhost->psMemInfo	= psTex->psMemInfo;
			psGhost->ui32Size	= psTex->psMemInfo->uAllocSize;
		}
#else
		/* Move the meminfo pointer from the texture to the ghost */
		psGhost->psMemInfo = psTex->psMemInfo;

		psGhost->ui32Size = psTex->psMemInfo->uAllocSize;
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
	}

#if defined PDUMP
	/* We should dump on a mipmap level basis but hopefully this will be good enough */
	psGhost->bDumped = psTex->ui32LevelsToDump? IMG_FALSE : IMG_TRUE;
#endif

	psTex->psMemInfo           = IMG_NULL;
	psTex->bResidence          = IMG_FALSE;
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
IMG_INTERNAL IMG_VOID  TextureRemoveResident(GLES2Context *gc, GLES2Texture *psTex)
{
	psTex->bResidence = IMG_FALSE;

	/*
	 * by setting validate here, we do a delay load of the texture at validation time
	 */
/* PRQA S 3332 1 */ /* Override QAC suggestion and use this macro. */
	PVR_UNREFERENCED_PARAMETER(gc);
}


#if defined PDUMP
/***********************************************************************************
 Function Name      : PDumpTexture
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Dumps all marked levels of the texture.
************************************************************************************/
IMG_INTERNAL IMG_VOID PDumpTexture(GLES2Context *gc, GLES2Texture *psTex)
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

				if(GLES2_TEXTURE_TARGET_CEM == psTex->ui32TextureTarget)
				{
					ui32MaxFace = 6;
				}
				else
				{
					ui32MaxFace = 1;
				}

				for(ui32Face=0; ui32Face<ui32MaxFace; ui32Face++)
				{
					GLES2MipMapLevel *psMipLevel;
					IMG_UINT32 ui32OffsetInBytes;
					IMG_UINT32 ui32TopUsize, ui32TopVsize;
					const GLES2TextureFormat *psTexFmt = psTex->psFormat;
					GLES2TextureParamState *psParams = &psTex->sState;
					IMG_UINT32 ui32BytesPerTexel, ui32SizeInBytes;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
					ui32TopUsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
					ui32TopVsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
					ui32TopUsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
					ui32TopVsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

					ui32BytesPerTexel = psTexFmt->ui32TotalBytesPerTexel;

					psMipLevel = &psTex->psMipLevel[ui32LOD + (ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS)];

					ui32SizeInBytes = psMipLevel->ui32ImageSize;

					if(psTex->ui32HWFlags & GLES2_NONPOW2)
					{
						ui32OffsetInBytes = GetNPOTMipMapOffset(ui32LOD, psTex) * ui32BytesPerTexel;

						ui32SizeInBytes =  GetNPOTMipMapOffset(ui32LOD+1, psTex) * ui32BytesPerTexel - ui32OffsetInBytes;
					}
					else if(psTex->ui32HWFlags & GLES2_COMPRESSED)
					{
						IMG_BOOL bIs2Bpp;

						GLES_ASSERT(psTexFmt->ui32NumChunks == 1);

						bIs2Bpp = (IMG_BOOL)( (psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTC2) || 
											(psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCII2));

						ui32OffsetInBytes =  ui32BytesPerTexel * 
							GetCompressedMipMapOffset(ui32LOD, ui32TopUsize, ui32TopVsize, bIs2Bpp);

						if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
						{
							IMG_UINT32 ui32FaceOffset = ui32BytesPerTexel *
								GetCompressedMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize, bIs2Bpp);

							if(psTex->ui32HWFlags & GLES2_MIPMAP)
							{
								if(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)
								{
									ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
								}
							}

							ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
						}
					}
					else
					{
						ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32LOD, ui32TopUsize, ui32TopVsize);

						if(psTex->ui32HWFlags & GLES2_MULTICHUNK)
						{
							/* FIXME: support multichunk textures */
						}

						if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
						{
							IMG_UINT32 ui32FaceOffset =
								ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);

							if(psTex->ui32HWFlags & GLES2_MIPMAP)
							{
								if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
									(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
								{
									ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
								}
							}

							ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
						}
					}

					PDUMP_STRING((gc, "Dumping level %d of texture %d\r\n", ui32LOD, ((GLES2NamedItem*)psTex)->ui32Name));
					PDUMP_MEM(gc, psTex->psMemInfo, ui32OffsetInBytes, ui32SizeInBytes);
				}
			}

			ui32Offset += psTex->psMipLevel[ui32LOD].ui32ImageSize;

			ui32LOD++;

			psTex->ui32LevelsToDump >>= 1;
		}
	}

#if defined(GLES2_EXTENSION_EGL_IMAGE)
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

	if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_STREAM && gc->psBufferDevice)
	{
		IMG_UINT32 i;

		for(i=0; i <  gc->psBufferDevice->sBufferInfo.ui32BufferCount; i++)
		{
			PDUMP_STRING((gc, "Dumping buffer %d of stream device\r\n", i));
			PDUMP_MEM(gc,  gc->psBufferDevice->psBuffer[i].psBufferSurface, 0,  gc->psBufferDevice->psBuffer[i].psBufferSurface->uAllocSize);
		}
	}

}
#endif /* defined PDUMP */


/***********************************************************************************
 Function Name      : TextureMakeResident
 Inputs             : gc, psTex
 Outputs            : -
 Returns            : Whether the texture was loaded to device memory.
 Description        : Allocates texture device memory, initialises texture control words and
					  loads texture data to device memory. This is called when the 
					  texture state is validated. Unlike an immediate mode renderer, 
					  our textures need to stay resident after they have been removed 
					  (until they have actually been rendered). So, if a texture is live and
					  marked as dirty, we may need to ghost the current state 
					  before loading the new texture data.
************************************************************************************/
IMG_INTERNAL IMG_BOOL TextureMakeResident(GLES2Context *gc, GLES2Texture *psTex)
{
	GLES2TextureParamState *psParams = &psTex->sState;
	PVRSRV_CLIENT_MEM_INFO sMemInfo  = {0};
	IMG_BOOL               bDirty    = psTex->bResidence?IMG_FALSE:IMG_TRUE;
	IMG_UINT32 ui32TexAddr;
	IMG_UINT32 i, j, ui32MaxLevel, ui32MaxFace;
	IMG_UINT32 ui32Level;
	GLES2MipMapLevel *psMipLevel;

	if(psTex->ui32HWFlags & GLES2_MIPMAP)
	{
		ui32MaxLevel = GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
	}
	else
	{
		ui32MaxLevel = 1;
	}

	if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
	{
		ui32MaxFace = 6;
	}
	else
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

	/* Ghost texture if necessary */
	if (psTex->psMemInfo &&
	    bDirty)
	{
		/* Whether to use SW to upload any level of this texture */
		if ( (!gc->sAppHints.bDisableHWTQTextureUpload) && 

		     ( KRM_IsResourceNeeded(&(gc->psSharedState->psTextureManager->sKRM), &(psTex->sResource)) ) )
		{
			sMemInfo = *psTex->psMemInfo;
			
			TexMgrGhostTexture(gc, psTex);
		}
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
		if(sMemInfo.uAllocSize && ((psTex->ui32HWFlags & GLES2_MIPMAP) != 0))
		{
			IMG_BOOL bCopyTexture = IMG_FALSE;
			
			for(j=0; j < ui32MaxFace; j++)
			{
				for(i=0; i < ui32MaxLevel; i++)
				{
					ui32Level = (j * GLES2_MAX_TEXTURE_MIPMAP_LEVELS) + i;
					psMipLevel = &psTex->psMipLevel[ui32Level];

					if(psMipLevel->pui8Buffer == GLES2_LOADED_LEVEL)
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
				GLES2_TIME_START(GLES2_TIMER_TEXTURE_GHOST_LOAD_TIME);

				CopyTextureData(gc, psTex, 0, &sMemInfo, 0, sMemInfo.uAllocSize);
		
				GLES2_TIME_STOP(GLES2_TIMER_TEXTURE_GHOST_LOAD_TIME);
			}
		}
	
		SetupTwiddleFns(psTex);

		ui32TexAddr = psTex->psMemInfo->sDevVAddr.uiAddr;

		psParams->aui32StateWord2[0] =
				(ui32TexAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) <<
				EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

		for(i=1; i < psTex->psFormat->ui32NumChunks; i++)
		{
			psParams->aui32StateWord2[i] = 
				((ui32TexAddr + (psTex->ui32ChunkSize * i)) >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) <<
				EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

		}

		bDirty = IMG_TRUE;
	}

	if (bDirty && psTex->psMemInfo)
	{
		GLES2_TIME_START(GLES2_TIMER_TEXTURE_TRANSLATE_LOAD_TIME);

		for(j=0; j < ui32MaxFace; j++)
		{
			for(i=0; i < ui32MaxLevel; i++)
			{
				ui32Level = (j * GLES2_MAX_TEXTURE_MIPMAP_LEVELS) + i;
				psMipLevel = &psTex->psMipLevel[ui32Level];

				if(psMipLevel->pui8Buffer != GLES2_LOADED_LEVEL && psMipLevel->pui8Buffer != 0)
				{
					TranslateLevel(gc, psTex, j, i);

#if (defined(DEBUG) || defined(TIMING))
					ui32TextureMemCurrent -= psMipLevel->ui32ImageSize;
#endif
					GLES2FreeAsync(gc, psMipLevel->pui8Buffer);

					psMipLevel->pui8Buffer = GLES2_LOADED_LEVEL;
				}

				if (psMipLevel->ui32Width == 1 && psMipLevel->ui32Height == 1)
				{
					break;
				}
			}
		}

		GLES2_TIME_STOP(GLES2_TIMER_TEXTURE_TRANSLATE_LOAD_TIME);
	}

	psTex->bResidence = IMG_TRUE;

	GLES_ASSERT(IsTextureConsistentWithMipMaps(psTex));

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
IMG_INTERNAL IMG_BOOL UnloadInconsistentTexture(GLES2Context *gc, GLES2Texture *psTex)
{
	IMG_UINT32 i, j, ui32MaxFace;
	GLES2MipMapLevel *psMipLevel;

	GLES2_TIME_START(GLES2_TIMER_TEXTURE_READBACK_TIME);

	if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
	{
		ui32MaxFace = 6;
	}
	else
	{
		ui32MaxFace = 1;
	}

	for(j=0; j < ui32MaxFace; j++)
	{
		for(i=0; i < GLES2_MAX_TEXTURE_MIPMAP_LEVELS; i++)
		{
			psMipLevel = &psTex->psMipLevel[i + (j * GLES2_MAX_TEXTURE_MIPMAP_LEVELS)];

			if(psMipLevel->pui8Buffer == GLES2_LOADED_LEVEL)
			{
				psMipLevel->pui8Buffer = GLES2MallocHeapUNC(gc, psMipLevel->ui32ImageSize);

				if (psMipLevel->pui8Buffer == IMG_NULL) 
				{
					GLES2_TIME_STOP(GLES2_TIMER_TEXTURE_READBACK_TIME);
					return IMG_FALSE;
				}

	#if (defined(DEBUG) || defined(TIMING))
				ui32TextureMemCurrent += psMipLevel->ui32ImageSize;

				if (ui32TextureMemCurrent>ui32TextureMemHWM)
				{
					ui32TextureMemHWM = ui32TextureMemCurrent;
				}
	#endif
				/* Flush + wait for outstanding renders as necessary before reading it back */
				FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psMipLevel, GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_WAIT_FOR_3D);

				ReadBackTextureData(gc, psTex, j, i, psMipLevel->pui8Buffer);

				FBOAttachableHasBeenModified(gc, (GLES2FrameBufferAttachable *)psMipLevel);
			}
		}
	}
	GLES2_TIME_STOP(GLES2_TIMER_TEXTURE_READBACK_TIME);	


	/* If the texture is live we must ghost it */
	if(KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &(psTex->sResource)))
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
			GLES2FREEDEVICEMEM_HEAP(gc, psTex->psMemInfo);
			psTex->psMemInfo = IMG_NULL;
			
			KRM_RemoveResourceFromAllLists(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource);
		}

		psTex->bResidence = IMG_FALSE;
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : IsTextureConsistent
 Inputs             : gc, psTex, ui32OverloadTexLayout, bCheckForRenderingLoop
 Outputs            : -
 Returns            : Whether a texture is bConsistent.
 Description        : Checks consistency for cubemap/mipmapped textures. Unloads data for
					  inconsistent textures. Also unloads textures which are 
					  consistent but don't match their device memory versions.
************************************************************************************/
IMG_INTERNAL IMG_UINT32 IsTextureConsistent(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32OverloadTexLayout, IMG_BOOL bCheckForRenderingLoop)
{
	/* NOTE: If we change how this function computes the control words we must update
	 * SetupTextureRenderTargetControlWords() to reflect those changes.
	 */
	const GLES2TextureFormat *psTexFormat = psTex->psMipLevel[0].psTexFormat;
	GLES2TextureParamState    *psParams = &psTex->sState;
	IMG_UINT32                i, ui32Width, ui32Height, ui32Face, ui32MaxFace, ui32WidthLog2, ui32HeightLog2,
							  ui32NumLevels, ui32Consistent, ui32FormatFlags = 0, ui32TexTypeSize = 0,
							  aui32StateWord1[GLES2_MAX_CHUNKS];
	GLenum                    eRequestedFormat;
	IMG_BOOL                  bStateWord1Changed = IMG_FALSE, bValidFilter;
#if defined(SGX545)
	IMG_UINT32                ui32StateWord0 = 0;
	IMG_BOOL                  bStateWord0Changed = IMG_FALSE;
#endif
	GLES2FrameBufferAttachable *psColorAttachment = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
	GLES2FrameBufferAttachable *psDepthAttachment = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES2_DEPTH_ATTACHMENT];
#endif

	if(psTex->ui32LevelsConsistent != GLES2_TEX_UNKNOWN)
	{
		return psTex->ui32LevelsConsistent;
	}

	if(psTexFormat == IMG_NULL)
	{
		psTex->ui32LevelsConsistent = GLES2_TEX_INCONSISTENT;

		return IMG_FALSE;
	}


	/* If there is a rendering loop, return false, without setting the levelsconsistent state. The texture isn't necessarily 
	 * inconsistent, but we don't want to reference the texture, until it is no longer bound as a destination for rendering.
	 */
	if(bCheckForRenderingLoop && psColorAttachment && psColorAttachment->eAttachmentType == GL_TEXTURE)
	{
		if(((GLES2MipMapLevel*)psColorAttachment)->psTex->sNamedItem.ui32Name == psTex->sNamedItem.ui32Name)
		{
			return IMG_FALSE;
		}
	}

#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
	if(psDepthAttachment && psDepthAttachment->eAttachmentType == GL_TEXTURE)
	{
		if(((GLES2MipMapLevel*)psDepthAttachment)->psTex->sNamedItem.ui32Name == psTex->sNamedItem.ui32Name)
		{
			return IMG_FALSE;
		}
	}
#endif

	if(GLES2_IS_MIPMAP(psParams->ui32MinFilter))
	{
		ui32FormatFlags |= GLES2_MIPMAP;
	}

#if defined(SGX545)
	if((psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_A8) ||
		(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_L8) ||
		(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_A8L8) ||
		(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_A_F16) ||
		(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_L_F16) ||
		(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_L_F16_A_F16) ||
		(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_NV12) ||
		(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12))
	{
		ui32StateWord0   = EURASIA_PDS_DOUTT0_TEXFEXT;
	}
#endif /* defined(SGX545) */	

	for(i=0; i < psTexFormat->ui32NumChunks; i++)
	{
		aui32StateWord1[i] = asSGXPixelFormat[psTexFormat->ePixelFormat].aui32TAGControlWords[i][1];
	}

	/* Multichunk? PVRTC? Single chunk? */
	if(psTexFormat->ui32NumChunks > 1)
	{
		ui32FormatFlags |= GLES2_MULTICHUNK;
	}

	switch(psTexFormat->ePixelFormat)
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
			ui32FormatFlags |= GLES2_FLOAT;
			break;
		case PVRSRV_PIXEL_FORMAT_PVRTC2:
		case PVRSRV_PIXEL_FORMAT_PVRTC4:
		case PVRSRV_PIXEL_FORMAT_PVRTCII2:
		case PVRSRV_PIXEL_FORMAT_PVRTCII4:
		case PVRSRV_PIXEL_FORMAT_PVRTCIII:
			ui32FormatFlags |= GLES2_COMPRESSED;
			break;
		default:
			break;	
	}

	ui32Consistent = GLES2_TEX_CONSISTENT;
	
	ui32Width = psTex->psMipLevel[0].ui32Width;
	ui32Height = psTex->psMipLevel[0].ui32Height;

	/* POT or NPOT */
	if(((ui32Width & (ui32Width - 1)) != 0) || ((ui32Height & (ui32Height - 1)) != 0))
	{
		ui32FormatFlags |= GLES2_NONPOW2;
		/* Round up NPOT texture sizes to the next POT */
		if ((ui32Width & (ui32Width - 1)) != 0)
		{
			ui32WidthLog2 = psTex->psMipLevel[0].ui32WidthLog2 + 1;
		}
		else
		{
			ui32WidthLog2 = psTex->psMipLevel[0].ui32WidthLog2;
		}
		if ((ui32Height & (ui32Height - 1)) != 0)
		{
			ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2 + 1;
		}
		else
		{
			ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2;
		}
	}
	else
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
	if(psTexFormat==&TexFormatFloatDepth)
	{
		ui32FormatFlags |= GLES2_NONPOW2;

		ui32WidthLog2  = psTex->psMipLevel[0].ui32WidthLog2 + 1;
		ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2 + 1;
	}
	else
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
	if(psTexFormat==&TexFormatFloatDepthU8Stencil)
	{
		ui32FormatFlags |= GLES2_NONPOW2;

		ui32WidthLog2  = psTex->psMipLevel[0].ui32WidthLog2 + 1;
		ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2 + 1;
	}
	else
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
	{
		ui32WidthLog2 = psTex->psMipLevel[0].ui32WidthLog2;
		ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2;
	}
	
	/* Consistency check for CEMs */
	if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
	{
		eRequestedFormat = psTex->psMipLevel[0].eRequestedFormat;
		ui32MaxFace = 6;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_CEM;
#else
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_CEM;
#endif

		GLES_ASSERT(ui32Width != 0 && ui32Height != 0);

		/* First check base level of each face is consistent */
		for(ui32Face=1; ui32Face < ui32MaxFace; ui32Face++)
		{
			IMG_UINT32 ui32FaceBaseLevel = ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS;

			if(psTex->psMipLevel[ui32FaceBaseLevel].eRequestedFormat != eRequestedFormat)
			{
				ui32Consistent = GLES2_TEX_INCONSISTENT;
				break;
			}
			else if(psTex->psMipLevel[ui32FaceBaseLevel].psTexFormat != psTexFormat)
			{
				ui32Consistent = GLES2_TEX_INCONSISTENT;
				break;
			}
			else if(psTex->psMipLevel[ui32FaceBaseLevel].ui32Width != ui32Width)
			{
				ui32Consistent = GLES2_TEX_INCONSISTENT;
				break;
			}
			else if(psTex->psMipLevel[ui32FaceBaseLevel].ui32Height != ui32Height)
			{
				ui32Consistent = GLES2_TEX_INCONSISTENT;
				break;
			}
		}
	}
	else
	{
		ui32MaxFace = 1;
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_2D;
#else
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_2D;
#endif
	}

	/* A non power of 2 texture is inconsistent if using a non-clamp-to-edge wrapmode */
	if(((ui32FormatFlags & GLES2_NONPOW2) != 0) && 
		((psParams->ui32StateWord0 & ~(EURASIA_PDS_DOUTT0_UADDRMODE_CLRMSK & EURASIA_PDS_DOUTT0_VADDRMODE_CLRMSK)) != 
									(EURASIA_PDS_DOUTT0_UADDRMODE_CLAMP | EURASIA_PDS_DOUTT0_VADDRMODE_CLAMP)))
	{
		ui32Consistent = GLES2_TEX_INCONSISTENT;
	}


#if defined(GLES2_EXTENSION_EGL_IMAGE)
	/* Don't support mipmapped eglimage textures */
	if(psTex->psEGLImageTarget)
	{
		if(ui32FormatFlags & GLES2_MIPMAP)
		{
			ui32Consistent = GLES2_TEX_INCONSISTENT;
		}

		/* Just return - the eglimage is self-consistent and we have already set up control words */
		psTex->ui32LevelsConsistent = ui32Consistent;
	
		return ui32Consistent;
	}
#endif


	/* Mipmap consistency */
	if(((ui32FormatFlags & GLES2_MIPMAP) != 0) && (ui32Consistent == GLES2_TEX_CONSISTENT))
	{
		eRequestedFormat = psTex->psMipLevel[0].eRequestedFormat;

		bValidFilter = IMG_FALSE;

		/* If the texture is float, mipmap filtering is very restricted 
		   since we don't implement GL_OES_texture_float_linear
		*/
		if(ui32FormatFlags & GLES2_FLOAT)
		{
			/* The min/mag filters must be NEAREST/NEAREST_MIPMAP_NEAREST */
			if(psTex->sState.ui32MinFilter & EURASIA_PDS_DOUTT0_MIPFILTER)
			{
				/* The float texture is invalid because it has mipmaps and it's filtering between them.
				   (Redundant assignment but easier to understand)
				*/
				bValidFilter = IMG_FALSE;
			}
			else
			{
				if(((psTex->sState.ui32MagFilter & ~EURASIA_PDS_DOUTT0_MAGFILTER_CLRMSK) == EURASIA_PDS_DOUTT0_MAGFILTER_POINT) &&
				   ((psTex->sState.ui32MinFilter & ~EURASIA_PDS_DOUTT0_MINFILTER_CLRMSK) == EURASIA_PDS_DOUTT0_MINFILTER_POINT))
				{
					/* There is not filtering between the mipmaps.
					   Besides, minification and magnification are NEAREST, so the texture is OK
				    */
					bValidFilter = IMG_TRUE;
				}
			}
		}
		else
		{
#if defined(GLES2_EXTENSION_NPOT)
			/* Can't have GL_NEAREST_MIPMAP_LINEAR or GL_LINEAR_MIPMAP_LINEAR minification filter on non-power-of-two textures */
			if(gc->sAppHints.bAllowTrilinearNPOT ||
 	 			((ui32FormatFlags & GLES2_NONPOW2) == 0) || ((psParams->ui32MinFilter & EURASIA_PDS_DOUTT0_MIPFILTER) == 0))
	 		{
				bValidFilter = IMG_TRUE;
	 		}
#else
			/* Can't have mipmapping at all on a non-power-of-two texture. */
			if(!(ui32FormatFlags & GLES2_NONPOW2))
			{
				bValidFilter = IMG_TRUE;
			}
#endif
		}

		if(!bValidFilter)
		{
			ui32Consistent = GLES2_TEX_INCONSISTENT;
			
			/* Skip level check */
			ui32MaxFace = 0;
		}

		for(ui32Face = 0; ui32Face < ui32MaxFace; ui32Face++)
		{
			IMG_UINT32 ui32FaceBaseLevel = ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS;

			i = 0;

			ui32Width = psTex->psMipLevel[0].ui32Width;
			ui32Height = psTex->psMipLevel[0].ui32Height;


			while (++i < GLES2_MAX_TEXTURE_MIPMAP_LEVELS) 
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

#if defined(GLES2_EXTENSION_NPOT)
				if(((ui32FormatFlags & GLES2_NONPOW2) != 0) && ((ui32Width<MIN_POW2_MIPLEVEL_SIZE) || (ui32Height<MIN_POW2_MIPLEVEL_SIZE)))
				{
					break;
				}	
#endif /* defined(GLES2_EXTENSION_NPOT) */

				if(psTex->psMipLevel[ui32FaceBaseLevel + i].eRequestedFormat != eRequestedFormat)
				{
					ui32Consistent = GLES2_TEX_INCONSISTENT;

					break;
				}
				else if(psTex->psMipLevel[ui32FaceBaseLevel + i].psTexFormat != psTexFormat)
				{
					ui32Consistent = GLES2_TEX_INCONSISTENT;
					
					break;
				}
				else if(psTex->psMipLevel[ui32FaceBaseLevel + i].ui32Width != ui32Width)
			    {
					ui32Consistent = GLES2_TEX_INCONSISTENT;
					
					break;
				}
				else if(psTex->psMipLevel[ui32FaceBaseLevel + i].ui32Height != ui32Height)
				{
					ui32Consistent = GLES2_TEX_INCONSISTENT;
					
					break;
				}
			}
		}
	}
	
	if(ui32FormatFlags & GLES2_MIPMAP)
	{
		ui32NumLevels = MAX(ui32WidthLog2, ui32HeightLog2) + 1;
	}
	else
	{
		ui32NumLevels = 1;
	}

#if defined(SGX545)
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
	if(psTexFormat==&TexFormatFloatDepthU8Stencil)
	{
		ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				|
							((psTex->psMipLevel[0].ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
							((psTex->psMipLevel[0].ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
	}
	else
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
	if(psTexFormat==&TexFormatFloatDepth)
	{
		ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				|
							((psTex->psMipLevel[0].ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
							((psTex->psMipLevel[0].ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
	}
	else
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */
#endif /* defined(SGX545) */
	if(ui32FormatFlags & GLES2_NONPOW2)
	{
		/* Always use stride textures on NPOT mipmaps */
		if (ui32FormatFlags & GLES2_MIPMAP)
		{
			ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				|
									((psTex->psMipLevel[0].ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
									((psTex->psMipLevel[0].ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
		}
		else
		{
			switch((ui32OverloadTexLayout & GLES2_HINT_OVERLOAD_TEX_LAYOUT_NONPOW2_MASK) >> GLES2_HINT_OVERLOAD_TEX_LAYOUT_NONPOW2_SHIFT)
			{
				case GLES2_HINT_OVERLOAD_TEX_LAYOUT_STRIDE:
				{
					ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				|
									((psTex->psMipLevel[0].ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
									((psTex->psMipLevel[0].ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
					break;
				}

				case GLES2_HINT_OVERLOAD_TEX_LAYOUT_TWIDDLED:
#if !defined(SGX_FEATURE_HYBRID_TWIDDLING)
				{
					PVR_DPF((PVR_DBG_ERROR,"Invalid apphint for overloading texture layout - npot textures cannot be twiddled"));
				}
#endif /* !defined(SGX_FEATURE_HYBRID_TWIDDLING) */

				case GLES2_HINT_OVERLOAD_TEX_LAYOUT_DEFAULT:
				case GLES2_HINT_OVERLOAD_TEX_LAYOUT_TILED:
				default:
				{
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
					ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_2D			  	    |
#else
					ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_TILED				|
#endif /* !defined(SGX_FEATURE_HYBRID_TWIDDLING) */
									((psTex->psMipLevel[0].ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
									((psTex->psMipLevel[0].ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
					break;
				}
			}
		}
	}
	else
	{
		ui32OverloadTexLayout = (ui32OverloadTexLayout & GLES2_HINT_OVERLOAD_TEX_LAYOUT_POW2_MASK) >> GLES2_HINT_OVERLOAD_TEX_LAYOUT_POW2_SHIFT;
		
		if(((ui32FormatFlags & (GLES2_COMPRESSED | GLES2_MIPMAP)) != 0) || 
			(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_TWIDDLED) ||
			(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_DEFAULT))
		{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
			ui32TexTypeSize   |= (ui32WidthLog2 << EURASIA_PDS_DOUTT1_USIZE_SHIFT)  |
								 (ui32HeightLog2 << EURASIA_PDS_DOUTT1_VSIZE_SHIFT) ;
#else 
		    ui32TexTypeSize   |= (((psTex->psMipLevel[0].ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)	|
			                     (((psTex->psMipLevel[0].ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK); 
#endif 
		}
		else if(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_STRIDE)
		{
			ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				|
								((psTex->psMipLevel[0].ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
								((psTex->psMipLevel[0].ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
		}
		else if(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_TILED)
		{
			ui32TexTypeSize |=	EURASIA_PDS_DOUTT1_TEXTYPE_TILED				|
								((psTex->psMipLevel[0].ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
								((psTex->psMipLevel[0].ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);

		}
	}

	/* Has the texture size or format changed? we need to unload it */

	/* Texture Control Word 0: WHETHER USING EXT TEXTURE FORMAT */
#if defined(SGX545)
	if (ui32StateWord0 != (psParams->ui32StateWord0 & EURASIA_PDS_DOUTT0_TEXFEXT))
	{
		bStateWord0Changed = IMG_TRUE;
	}
#endif /* defined(SGX545) */

	/* Texture Control Word 1: TEXURE TYPE, FORMAT, UV / WH SIZE */ 
	for(i=0; i < psTexFormat->ui32NumChunks; i++)
	{
		aui32StateWord1[i] |= ui32TexTypeSize;

		if(psParams->aui32StateWord1[i] != aui32StateWord1[i])
		{
			bStateWord1Changed = IMG_TRUE;
		}
	}

	if(ui32FormatFlags & GLES2_MIPMAP)
	{
		if(psTex->psMemInfo && 
			(ui32Consistent== GLES2_TEX_INCONSISTENT || 
			 bStateWord1Changed                      || 
#if defined(SGX545)
			 bStateWord0Changed                      || 
#endif
			 ((psTex->ui32HWFlags & GLES2_MIPMAP) == 0)))
		{
			if(UnloadInconsistentTexture(gc, psTex) == IMG_FALSE)
			{
				return IMG_FALSE;
			}
		}
		
		if(ui32Consistent == GLES2_TEX_CONSISTENT)
		{
			for(i=0; i < psTexFormat->ui32NumChunks; i++)
			{
				psParams->aui32StateWord1[i] = aui32StateWord1[i];
			}
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
			else if(psTex->ui32HWFlags & GLES2_MIPMAP)
			{
				/* Texture is mimapped, but filter is not, keep texture as mipmap */
				ui32NumLevels = psTex->ui32NumLevels;
				ui32FormatFlags |= GLES2_MIPMAP;
			}
		}

		for(i=0; i < psTexFormat->ui32NumChunks; i++)
		{
			psParams->aui32StateWord1[i] = aui32StateWord1[i];
		}

		psTex->ui32NumLevels = ui32NumLevels;
		psTex->ui32HWFlags = ui32FormatFlags;
		psTex->psFormat = psTexFormat;
	}

	psTex->ui32LevelsConsistent = ui32Consistent;

	return ui32Consistent;
}



#if defined(GLES2_EXTENSION_EGL_IMAGE)
/***********************************************************************************
 Function Name      : TextureCreateImageLevel
 Inputs             : gc, psEGLImage
 Outputs            : -
 Returns            : -
 Description        : Creates a texture level from an EGLImage
************************************************************************************/
IMG_INTERNAL IMG_BOOL TextureCreateImageLevel(GLES2Context *gc, GLES2Texture *psTex)
{
	const GLES2TextureFormat *psTexFormat;
	GLES2MipMapLevel  *psMipLevel;
	IMG_UINT32 i, ui32BufferSize; 
	GLES2TextureParamState *psParams = &psTex->sState;
	EGLImage *psEGLImage;

	/* Frees any data currently associated with the texture - all levels */
	for(i = 0; i < GLES2_MAX_TEXTURE_MIPMAP_LEVELS; i++)
	{
		psMipLevel = &psTex->psMipLevel[i];

		/* The texture level is being freed */
		if (psMipLevel->pui8Buffer != NULL && psMipLevel->pui8Buffer != GLES2_LOADED_LEVEL)
		{
			PVR_UNREFERENCED_PARAMETER(gc);
			GLES2FreeAsync(gc, psMipLevel->pui8Buffer);
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
#if defined(EGL_IMAGE_COMPRESSED_GLES2)
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
			psTexFormat = &TexFormatETC1RGB;
			break;
		}
#endif
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
		case PVRSRV_PIXEL_FORMAT_NV12:
		{
			psTexFormat = &TexFormatYUV420;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_YV12:
		{
			psTexFormat = &TexFormat3PlaneYUV420;
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"TextureCreateImageLevel: Unknown pixel format: %d", psEGLImage->ePixelFormat));			

			return IMG_FALSE;
		}
	}
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
	if (psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_STREAM)
	{
		CreateExternalTextureState(psTex->psExtTexState, psEGLImage->ui32Width,
									psEGLImage->ui32Height, psEGLImage->ePixelFormat,
									psEGLImage->ui32Stride, psEGLImage->ui32Flags);
	}
#endif

	ui32BufferSize = psEGLImage->ui32Width * psEGLImage->ui32Height * psTexFormat->ui32TotalBytesPerTexel;

	psMipLevel->pui8Buffer		 = GLES2_LOADED_LEVEL;
	psMipLevel->ui32Width		 = psEGLImage->ui32Width;
	psMipLevel->ui32Height		 = psEGLImage->ui32Height;
	psMipLevel->ui32ImageSize	 = ui32BufferSize;
	psMipLevel->ui32WidthLog2	 = FloorLog2(psMipLevel->ui32Width);
	psMipLevel->ui32HeightLog2	 = FloorLog2(psMipLevel->ui32Height);
	psMipLevel->psTexFormat		 = psTexFormat;
	psMipLevel->eRequestedFormat = (GLenum)((psTexFormat->ui32BaseFormatIndex == GLES2_RGB_TEX_INDEX) ? GL_RGB : GL_RGBA);

	psTex->psFormat = psTexFormat;

	SetupTwiddleFns(psTex);

	psParams->aui32StateWord1[0] = asSGXPixelFormat[psTexFormat->ePixelFormat].aui32TAGControlWords[0][1];

	if(psEGLImage->bTwiddled)
	{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
		psParams->aui32StateWord1[0] |= (FloorLog2(psMipLevel->ui32Width)  << EURASIA_PDS_DOUTT1_USIZE_SHIFT) |
								  		(FloorLog2(psMipLevel->ui32Height) << EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
		psParams->aui32StateWord1[0] |= (((psMipLevel->ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  |
										(((psMipLevel->ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;
#endif 
	}
	else
	{
		psParams->aui32StateWord1[0] |= EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE |
									  ((psMipLevel->ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
									  ((psMipLevel->ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
	}

	psParams->aui32StateWord2[0] = (psEGLImage->ui32HWSurfaceAddress >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;
	
	psTex->ui32LevelsConsistent = GLES2_TEX_UNKNOWN;

	return IMG_TRUE;
}


#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)

#if defined(SGX545)
/***********************************************************************************
 Function Name      : Setup545DepthStencilStride
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID Setup545DepthStencilStride(GLES2Texture *psTex, IMG_UINT32 *pui32StateWord0, IMG_UINT32 *pui32StateWord1)
{
	IMG_UINT32 ui32Stride, ui32MatchingStride;

	ui32Stride = ALIGNCOUNT(psTex->psMipLevel[0].ui32Width, EURASIA_TAG_TILE_SIZEX);

#if(EURASIA_TAG_STRIDE_THRESHOLD > 0)
	if(ui32Width < EURASIA_TAG_STRIDE_THRESHOLD)
	{
		ui32MatchingStride = (psTex->psMipLevel[0].ui32Width + (EURASIA_TAG_STRIDE_ALIGN0 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN0 - 1);
	}
	else
#endif
	{			
		ui32MatchingStride = (psTex->psMipLevel[0].ui32Width + (EURASIA_TAG_STRIDE_ALIGN1 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN1 - 1);
	}

	/* Only switch to NP2 stride mode if stride and width don't match according to HW */
	if(ui32Stride != ui32MatchingStride)
	{
		/* Stride is specified as 0 == 1 */
		ui32Stride -= 1;

		*pui32StateWord1 &= EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK;
		*pui32StateWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT;
			
		*pui32StateWord0 &= (EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK);

		*pui32StateWord0 |= 
			(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDELO_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK) |
			(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK);
	}
}
#endif /* defined(SGX545) */


/***********************************************************************************
 Function Name      : SetupEGLImageArbitraryStride
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID SetupEGLImageArbitraryStride(GLES2Texture *psTex, IMG_UINT32 *pui32StateWord0, IMG_UINT32 *pui32StateWord1)
{
	IMG_UINT32 ui32StateWord0 = psTex->sState.ui32StateWord0;
	IMG_UINT32 ui32StateWord1 = psTex->sState.aui32StateWord1[0];
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
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
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
			PVR_DPF((PVR_DBG_ERROR,"SetupEGLImageArbitraryStride: Unsupported pixel format %d", psTex->psEGLImageTarget->ePixelFormat));
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

#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */



/***********************************************************************************
 Function Name      : TextureCreateLevel
 Inputs             : gc, psTex, ui32Level, eInternalFormat, psTexFormat, ui32Width, 
					  ui32Height
 Outputs            : -
 Returns            : Pointer to new texture level buffer
 Description        : (Re)Allocates host memory for a new texture level.
************************************************************************************/
IMG_INTERNAL IMG_UINT8*  TextureCreateLevel(GLES2Context *gc, GLES2Texture *psTex,
											IMG_UINT32 ui32Level, GLenum eInternalFormat, 
											const GLES2TextureFormat *psTexFormat, 
											IMG_UINT32 ui32Width, IMG_UINT32 ui32Height)
{
	IMG_UINT8 *pui8Buffer;
	IMG_UINT32 ui32BufferSize, ui32BufferWidth, ui32BufferHeight;
	GLES2MipMapLevel *psMipLevel = &psTex->psMipLevel[ui32Level];
	IMG_BOOL bUseCachedMemory = IMG_FALSE;

#if defined (DEBUG)
	IMG_UINT32 ui32BaseWidth, ui32BaseHeight, ui32Lod;

	ui32Lod = (ui32Level % GLES2_MAX_TEXTURE_MIPMAP_LEVELS);
	
	ui32BaseWidth = ui32Width * (1 << ui32Lod);
	ui32BaseHeight = ui32Height * (1 << ui32Lod);

	/* If these fail something is wrong in IsLegalRangeTex */
	GLES_ASSERT(ui32BaseWidth  <= GLES2_MAX_TEXTURE_SIZE);
	GLES_ASSERT(ui32BaseHeight <= GLES2_MAX_TEXTURE_SIZE);
#endif /* DEBUG */

	GLES_ASSERT(psTexFormat != IMG_NULL);

	ui32BufferWidth = ui32Width;
	ui32BufferHeight = ui32Height;

	/* Adjust width, height for compressed formats */
	switch(psTexFormat->ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_PVRTC2:
		case PVRSRV_PIXEL_FORMAT_PVRTCII2:
		{
			ui32BufferWidth = MAX(ui32BufferWidth >> 3, 1);
			ui32BufferHeight = MAX(ui32BufferHeight >> 2, 1);
			bUseCachedMemory = IMG_TRUE;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_PVRTC4:
		case PVRSRV_PIXEL_FORMAT_PVRTCII4:
		{
			ui32BufferWidth = MAX(ui32BufferWidth >> 2, 1);
			ui32BufferHeight = MAX(ui32BufferHeight >> 2, 1);
			bUseCachedMemory = IMG_TRUE;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_PVRTCIII:
		{
			ui32BufferWidth = ALIGNCOUNTINBLOCKS(ui32BufferWidth, 2);
			ui32BufferHeight = ALIGNCOUNTINBLOCKS(ui32BufferHeight, 2);
			bUseCachedMemory = IMG_TRUE;

			break;
		}
		default:
		{
			break;
		}
	}
	  
	ui32BufferSize = ui32BufferWidth * ui32BufferHeight * psTexFormat->ui32TotalBytesPerTexel;

	if (ui32BufferSize)
	{
		if (psMipLevel->pui8Buffer != GLES2_LOADED_LEVEL)
		{
			if (!bUseCachedMemory)
			{
				pui8Buffer = (IMG_UINT8 *)GLES2ReallocHeapUNC(gc, psMipLevel->pui8Buffer, ui32BufferSize);
			}
			else
			{
				pui8Buffer = (IMG_UINT8 *)GLES2Realloc(gc, psMipLevel->pui8Buffer, ui32BufferSize);
			}

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
			if (!bUseCachedMemory)
			{
				pui8Buffer = (IMG_UINT8 *)GLES2MallocHeapUNC(gc, ui32BufferSize);
			}
			else
			{
				pui8Buffer = (IMG_UINT8 *)GLES2Malloc(gc, ui32BufferSize);
			}

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

#if defined DEBUG
			/* Overwrite the texels with a known pattern to aid debugging */
			/* It looks dark red if the format is RGBA8 */
			if(ui32BufferSize >= 4)
			{
				IMG_UINT32 i;

				for(i=0; i < ui32BufferSize -3; i+= 4)
				{
					pui8Buffer[i  ] = 0x81;
					pui8Buffer[i+1] = 0x12;
					pui8Buffer[i+2] = 0x13;
					pui8Buffer[i+3] = 0xF4;
				}
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
		if (psMipLevel->pui8Buffer != IMG_NULL && psMipLevel->pui8Buffer != GLES2_LOADED_LEVEL)
		{
#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent -= psMipLevel->ui32ImageSize;
#endif
			GLES2FreeAsync(gc, psMipLevel->pui8Buffer);
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

	psTex->ui32LevelsConsistent = GLES2_TEX_UNKNOWN;

	return psMipLevel->pui8Buffer;
}

/***********************************************************************************
 Function Name      : CreateTexture
 Inputs             : gc, ui32Name, ui32Target
 Outputs            : -
 Returns            : Pointer to new texture structure
 Description        : Creates and initialises a new texture structure.
************************************************************************************/
IMG_INTERNAL GLES2Texture *CreateTexture(GLES2Context *gc, IMG_UINT32 ui32Name,
										 IMG_UINT32 ui32Target)
{
	GLES2Texture           *psTex;
	GLES2TextureParamState *psParams;
	IMG_UINT32             i, ui32NumLevels /* = 1 */;
	GLES2MipMapLevel       *psLevel;

	/* To avoid warning in non-Symbian builds */
	PVR_UNREFERENCED_PARAMETER(gc);

	psTex = (GLES2Texture *) GLES2Calloc(gc, sizeof(GLES2Texture));

	if (psTex == IMG_NULL)
	{
		return IMG_NULL;
	}

#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
	psTex->psExtTexState = (GLES2ExternalTexState *) GLES2Calloc(gc, sizeof(GLES2ExternalTexState));
#endif

	psParams = &psTex->sState;

	/* initialize the texture */
	psParams->ui32MagFilter            = EURASIA_PDS_DOUTT0_MAGFILTER_LINEAR;
	psParams->ui32MinFilter            = EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX | 
								         EURASIA_PDS_DOUTT0_MINFILTER_POINT |
								         EURASIA_PDS_DOUTT0_MIPFILTER;
	psParams->bAutoMipGen              = IMG_FALSE;

	psParams->ui32StateWord0           = EURASIA_PDS_DOUTT0_UADDRMODE_REPEAT | 
								         EURASIA_PDS_DOUTT0_VADDRMODE_REPEAT;

	psTex->sNamedItem.ui32Name	       = ui32Name;
	psTex->bResidence			       = IMG_FALSE;

	psTex->bHasEverBeenGhosted         = IMG_FALSE;

	psTex->ui32LevelsConsistent        = GLES2_TEX_INCONSISTENT;
	psTex->ui32TextureTarget	       = ui32Target;

#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
	psTex->psBufferDevice              = IMG_NULL;
	psTex->ui32BufferOffset            = 0;
#endif /* defined(GLES2_EXTENSION_TEXTURE_STREAM) */


	if(ui32Target == GLES2_TEXTURE_TARGET_2D)
	{
		ui32NumLevels = GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
	}
	else if(ui32Target == GLES2_TEXTURE_TARGET_STREAM)
	{
		psTex->sNamedItem.ui32Name	= ui32Name;
		psTex->bResidence			= IMG_TRUE;
		psTex->ui32LevelsConsistent = GLES2_TEX_CONSISTENT;
		psParams->ui32MagFilter =	EURASIA_PDS_DOUTT0_MAGFILTER_LINEAR;

#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
		psParams->ui32MinFilter =	EURASIA_PDS_DOUTT0_MINFILTER_POINT | EURASIA_PDS_DOUTT0_NOTMIPMAP;
#endif
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
		psParams->ui32MinFilter =	EURASIA_PDS_DOUTT0_MINFILTER_LINEAR | EURASIA_PDS_DOUTT0_NOTMIPMAP;
#endif

		psParams->ui32StateWord0 =	EURASIA_PDS_DOUTT0_UADDRMODE_CLAMP | 
									EURASIA_PDS_DOUTT0_VADDRMODE_CLAMP;
		ui32NumLevels = GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
	}
	else
	{
		/* CUBE MAP */
		ui32NumLevels = GLES2_MAX_TEXTURE_MIPMAP_LEVELS * GLES2_TEXTURE_CEM_FACE_MAX;
	}

	psTex->psMipLevel = GLES2Calloc(gc, sizeof(GLES2MipMapLevel) * ui32NumLevels);

	if(psTex->psMipLevel == IMG_NULL)
	{
		GLES2Free(IMG_NULL, psTex);
		return IMG_NULL;
	}

	for(i=0; i < ui32NumLevels; ++i)
	{
		psLevel                 = &psTex->psMipLevel[i];
		psLevel->psTex          = psTex;
		psLevel->ui32Level      = i;

		((GLES2FrameBufferAttachable*)psLevel)->psRenderSurface = IMG_NULL;
		((GLES2FrameBufferAttachable*)psLevel)->eAttachmentType = GL_TEXTURE;
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
static IMG_VOID FreeTexture(GLES2Context *gc, GLES2Texture *psTex)
{
	IMG_UINT32       i, ui32MaxLevel;
	GLES2MipMapLevel *psMipLevel;

	ui32MaxLevel = GLES2_MAX_TEXTURE_MIPMAP_LEVELS;

	if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
	{
		ui32MaxLevel *= 6;
	}

	FlushUnflushedTextureRenders(gc, psTex);

	for (i = 0; i < ui32MaxLevel; i++)
	{
		psMipLevel = &psTex->psMipLevel[i];

		DestroyFBOAttachableRenderSurface(gc, (GLES2FrameBufferAttachable*)psMipLevel);

		if (psMipLevel->pui8Buffer && psMipLevel->pui8Buffer != GLES2_LOADED_LEVEL) 
		{
#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent -= psMipLevel->ui32ImageSize;
#endif
			GLES2FreeAsync(gc, psMipLevel->pui8Buffer);
			psMipLevel->pui8Buffer = IMG_NULL;
		}
	}

	GLES2Free(IMG_NULL, psTex->psMipLevel);
	psTex->psMipLevel = IMG_NULL;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
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
#endif /*defined(GLES2_EXTENSION_EGL_IMAGE) */
#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
	if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_STREAM)
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
#endif /* defined(GLES2_EXTENSION_TEXTURE_STREAM) */
	if (psTex->psMemInfo)
	{
		/* If the texture was live we must ghost it */
		if (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &(psTex->sResource)))
		{
			/* FIXME: This doesn't make much sense in the case we are shutting down */
			TexMgrGhostTexture(gc, psTex);
            //[LGE_UPDATE_S] jeonghoon.cho@lge.com Fix clock widget memory leak issue as blocking drawing texture in invisible area.
			FlushAllUnflushedFBO(gc, IMG_FALSE);
            //[LGE_UPDATE_E] jeonghoon.cho@lge.com
		}
		else
		{
#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent -= psTex->psMemInfo->uAllocSize;
#endif
			GLES2FREEDEVICEMEM_HEAP(gc, psTex->psMemInfo);

			psTex->psMemInfo = IMG_NULL;
		}
	}


	KRM_RemoveResourceFromAllLists(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource);

	TextureRemoveResident(gc, psTex);
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
	GLES2Free(IMG_NULL, psTex->psExtTexState);
#endif

	GLES2Free(IMG_NULL, psTex);
}

/***********************************************************************************
 Function Name      : DisposeTexObj
 Inputs             : gc, psItem, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Generic texture object free function; called from names.c
************************************************************************************/
static IMG_VOID DisposeTexObj(GLES2Context *gc, GLES2NamedItem *psItem, IMG_BOOL bIsShutdown)
{
	GLES2Texture *psTex = (GLES2Texture *)psItem;

	/* Avoid warning in release mode */
	PVR_UNREFERENCED_PARAMETER(bIsShutdown);

	GLES_ASSERT(bIsShutdown || (psTex->sNamedItem.ui32RefCount == 0));

	FreeTexture(gc, psTex);
}


/***********************************************************************************
 Function Name      : CreateDummyTextures
 Inputs             : gc, psTexMgr
 Outputs            : -
 Returns            : -
 Description        : Creates dummy textures for use when we run out of texture memory
					 (1x1 INTENSITY_8 texture) or when a program samples an
					  inconsistent texture (1x1 LUMINANCE_ALPHA_8 texture)
************************************************************************************/
static IMG_BOOL CreateDummyTextures(GLES2Context *gc, GLES2TextureManager *psTexMgr)
{
	IMG_UINT32 *pui32TextureData;

	if(GLES2ALLOCDEVICEMEM(gc->ps3DDevData,
							gc->psSysContext->hGeneralHeap,
							PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
							4,
							(1 << EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT),
							&psTexMgr->psWhiteDummyTexture) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"CreateDummyTexture: Can't create our white dummy texture"));

		return IMG_FALSE;
	}

	pui32TextureData = (IMG_UINT32 *)psTexMgr->psWhiteDummyTexture->pvLinAddr;
	pui32TextureData[0] = 0xFFFFFFFF;


	if(GLES2ALLOCDEVICEMEM(gc->ps3DDevData, 
							gc->psSysContext->hGeneralHeap, 
							PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
							4,
							(1 << EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT),
							&psTexMgr->psBlackDummyTexture) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"CreateDummyTexture: Can't create our black dummy texture"));

		if(GLES2FREEDEVICEMEM(gc->ps3DDevData, psTexMgr->psWhiteDummyTexture) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"CreateDummyTexture: Can't free our white dummy texture"));
		}

		return IMG_FALSE;
	}

	pui32TextureData = (IMG_UINT32 *)psTexMgr->psBlackDummyTexture->pvLinAddr;
	pui32TextureData[0] = 0xFF000000;


#if (defined(DEBUG) || defined(TIMING))
	ui32TextureMemCurrent += psTexMgr->psWhiteDummyTexture->uAllocSize;
	ui32TextureMemCurrent += psTexMgr->psBlackDummyTexture->uAllocSize;

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
static IMG_BOOL DestroyDummyTextures(GLES2Context *gc, GLES2TextureManager *psTexMgr)
{
	IMG_BOOL bRetVal;

#if (defined(DEBUG) || defined(TIMING))
	ui32TextureMemCurrent -= psTexMgr->psWhiteDummyTexture->uAllocSize;
	ui32TextureMemCurrent -= psTexMgr->psBlackDummyTexture->uAllocSize;
#endif

	bRetVal = IMG_TRUE;

	if(GLES2FREEDEVICEMEM(gc->ps3DDevData, psTexMgr->psWhiteDummyTexture) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"DestroyDummyTexture: Can't free our white dummy texture"));

		bRetVal = IMG_FALSE;
	}

	if(GLES2FREEDEVICEMEM(gc->ps3DDevData, psTexMgr->psBlackDummyTexture) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"DestroyDummyTexture: Can't free our black dummy texture"));

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
                      The pointer to the context is only passed because GLES2Malloc/GLES2Free need it :-(
************************************************************************************/
IMG_INTERNAL GLES2TextureManager * CreateTextureManager(GLES2Context *gc)
{
	GLES2TextureManager *psTexMgr;

	psTexMgr = GLES2Calloc(gc, sizeof(GLES2TextureManager));

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
static IMG_BOOL InitTextureState(GLES2Context *gc)
{
	IMG_UINT32 i, j;

	/* Init texture unit state */
	for (i=0; i < GLES2_MAX_TEXTURE_UNITS; i++) 
	{
		for(j=0; j < GLES2_TEXTURE_TARGET_MAX; j++)
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

#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL) || defined(GLES2_EXTENSION_TEXTURE_STREAM)
/***********************************************************************************
 Function Name      : SetupExternalTextureState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Sets necessary state for 'external' YUV textures
************************************************************************************/
static IMG_BOOL SetupExternalTextureState(GLES2Context *gc, GLES2ExternalTexState *psExtTexState,
										  GLES2Texture *psTex, GLES2CompiledTextureState *psTextureState,
										  IMG_UINT32 *ui32ChunkNumber, IMG_DEV_VIRTADDR uBufferAddr,
										  IMG_UINT32 ui32PixelHeight, IMG_UINT32 ui32ByteStride
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
										  ,IMG_UINT32 ui32StateWord3
#endif
#if defined(SUPPORT_NV12_FROM_2_HWADDRS)
										  ,IMG_UINT32 ui32HWSurfaceAddress2
#endif
										  )
{
	GLES2TextureParamState *psParams = &psTex->sState;
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
		psTextureState->aui32TAGControlWord[*ui32ChunkNumber][0] = ui32InitStrideStateWord0 | psExtTexState->aui32TexWord0[0];
	}
	else
	{
		psTextureState->aui32TAGControlWord[*ui32ChunkNumber][0] = psParams->ui32StateWord0;
	}
	psTextureState->aui32TAGControlWord[*ui32ChunkNumber][1] = psExtTexState->aui32TexWord1[0];
	psTextureState->aui32TAGControlWord[*ui32ChunkNumber][2] =
		(uBufferAddr.uiAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
	psTextureState->aui32TAGControlWord[*ui32ChunkNumber][3] = ui32StateWord3;
#endif /* (EURASIA_TAG_TEXTURE_STATE_SIZE == 4) */

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
		(*ui32ChunkNumber)++;

		if ((psExtTexState->aui32TexStrideFlag & 0x2U) != 0)
		{
			/* if use the stride */
			psTextureState->aui32TAGControlWord[*ui32ChunkNumber][0] = ui32InitStrideStateWord0 | psExtTexState->aui32TexWord0[1];
		}
		else
		{
			psTextureState->aui32TAGControlWord[*ui32ChunkNumber][0] = psParams->ui32StateWord0;
		}
		psTextureState->aui32TAGControlWord[*ui32ChunkNumber][1] = psExtTexState->aui32TexWord1[1];
		psTextureState->aui32TAGControlWord[*ui32ChunkNumber][2] = (ui32BufferAddress >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
																	EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
		psTextureState->aui32TAGControlWord[*ui32ChunkNumber][3] = ui32StateWord3;
#endif /* (EURASIA_TAG_TEXTURE_STATE_SIZE == 4) */
		
		if(psTex->psFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12)
		{
			/* Extra unit inserted for third plane - V */
			(*ui32ChunkNumber)++;

			ui32BufferAddress += (ui32ByteStride * ui32PixelHeight / 4);

			if ((psExtTexState->aui32TexStrideFlag & 0x2U) != 0)
			{
				/* if use the stride */
				psTextureState->aui32TAGControlWord[*ui32ChunkNumber][0] = ui32InitStrideStateWord0 | psExtTexState->aui32TexWord0[2];
			}
			else
			{
				psTextureState->aui32TAGControlWord[*ui32ChunkNumber][0] = psParams->ui32StateWord0;
			}
			psTextureState->aui32TAGControlWord[*ui32ChunkNumber][1] = psExtTexState->aui32TexWord1[2];
			psTextureState->aui32TAGControlWord[*ui32ChunkNumber][2] = (ui32BufferAddress >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
																		EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
			psTextureState->aui32TAGControlWord[*ui32ChunkNumber][3] = ui32StateWord3;
#endif /* (EURASIA_TAG_TEXTURE_STATE_SIZE == 4) */
		}
	}


	return IMG_TRUE;

}

#endif /* defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL) || defined(GLES2_EXTENSION_TEXTURE_STREAM) */

/***********************************************************************************
 Function Name      : CreateTextureState
 Inputs             : gc
 Outputs            : -
 Returns            : Success
 Description        : Initialises the texture manager and creates/shares the name 
					  array. Creates the default object (name 0).
************************************************************************************/

IMG_INTERNAL IMG_BOOL CreateTextureState(GLES2Context *gc)
{
	GLES2Texture *psTex;
	IMG_UINT32 i;

	gc->psSharedState->psTextureManager = gc->psSharedState->psTextureManager;

	gc->sState.sTexture.psActive = &gc->sState.sTexture.asUnit[0];

	/*
	** Set up the texture object for the default texture.
	** Because the default texture is not shared, it is
	** not hung off of the psNamesArray structure.
	** For that reason we have to set up manually the refcount.
	*/
	for(i=0; i < GLES2_TEXTURE_TARGET_MAX; i++)
	{
		psTex = CreateTexture(gc, 0, i);

		if(!psTex)
		{
			PVR_DPF((PVR_DBG_ERROR,"Couldn't create default texture"));
			return IMG_FALSE;
		}
		psTex->sNamedItem.ui32RefCount = 1;

		GLES_ASSERT(psTex->sNamedItem.ui32Name == 0);
		GLES_ASSERT(psTex->sNamedItem.ui32RefCount == 1);
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

IMG_INTERNAL IMG_VOID  SetupTexNameArray(GLES2NamesArray *psNamesArray)
{
	psNamesArray->pfnFree = DisposeTexObj;
}


/***********************************************************************************
 Function Name      : ReleaseTextureManager
 Inputs             : gc, psTexMgr
 Outputs            : -
 Returns            : -
 Description        : Frees the texture manager and associated memory
************************************************************************************/
IMG_INTERNAL IMG_VOID ReleaseTextureManager(GLES2Context *gc, GLES2TextureManager *psTexMgr)
{
	DestroyDummyTextures(gc, psTexMgr);

	/* Destroy all ghosts. Textures will be destroyed in FreeTextureState() */
	KRM_WaitForAllResources(&psTexMgr->sKRM, GLES2_DEFAULT_WAIT_RETRIES);
	KRM_DestroyUnneededGhosts(gc, &psTexMgr->sKRM);

	/* Destroy the manager itself */
	KRM_Destroy(gc, &psTexMgr->sKRM);
	GLES2Free(IMG_NULL, psTexMgr);
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
IMG_INTERNAL IMG_BOOL FreeTextureState(GLES2Context *gc)
{
	IMG_UINT32 i, j;
	GLES2Texture *psTex;
	IMG_BOOL bResult = IMG_TRUE;

	/* Unbind all non-default texture objects */
	for (i=0; i < GLES2_MAX_TEXTURE_UNITS; i++) 
	{
		for(j=0; j < GLES2_TEXTURE_TARGET_MAX; j++)
		{
			if(BindTexture(gc, i, j, 0) != IMG_TRUE)
			{
				PVR_DPF((PVR_DBG_ERROR,"FreeTextureState: BindTexture %u,%u failed",i,j));
				bResult=IMG_FALSE;
			}
		}
	}

	for(i=0; i < GLES2_TEXTURE_TARGET_MAX; i++)
	{
		/* Free default texture object */
		psTex = gc->sTexture.psDefaultTexture[i];
		psTex->sNamedItem.ui32RefCount--;

		GLES_ASSERT(psTex->sNamedItem.ui32RefCount == 0);

		FreeTexture(gc, psTex);
		gc->sTexture.psDefaultTexture[i] = IMG_NULL;
	}

	if(gc->sPrim.sVertexTextureState.psTextureImageChunks)
	{
		GLES2Free(IMG_NULL, gc->sPrim.sVertexTextureState.psTextureImageChunks);
	}

	if(gc->sPrim.sFragmentTextureState.psTextureImageChunks)
	{
		GLES2Free(IMG_NULL, gc->sPrim.sFragmentTextureState.psTextureImageChunks);
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
IMG_INTERNAL IMG_VOID SetupTextureState(GLES2Context *gc)
{
	IMG_UINT32 j;

	for(j=0; j < 2; j++)
	{
		IMG_UINT32 i, ui32ChunkNumber;

		GLES2ProgramShader *psShader;
		GLES2TextureSampler *psTextureSampler;
		GLES2TextureManager *psTexMgr = gc->psSharedState->psTextureManager;
		GLES2CompiledTextureState *psTextureState;
		IMG_UINT32 *pui32NumChunks;

		if(j==0)
		{
			/* First vertex shader texturing */
			psShader = &gc->sProgram.psCurrentProgram->sVertex;
			psTextureState = &gc->sPrim.sVertexTextureState;
		}
		else
		{
			/* Second fragment shader texturing */
			psShader = &gc->sProgram.psCurrentProgram->sFragment;
			psTextureState = &gc->sPrim.sFragmentTextureState;
		}

		psTextureState->bSomeTexturesWereGhosted = IMG_FALSE;
		psTextureState->ui32ImageUnitEnables     = 0;

		pui32NumChunks = psTextureState->aui32ChunkCount;

		if(!psShader->ui32SamplersActive)
		{
			continue;
		}

		for(i=0; i < GLES2_MAX_TEXTURE_UNITS; i++)
		{
			ui32ChunkNumber = i * PDS_NUM_TEXTURE_IMAGE_CHUNKS;

			if(psShader->ui32SamplersActive & (1U << i))
			{
				IMG_UINT8 ui8ImageUnit, ui8SamplerType;

				psTextureSampler = &psShader->asTextureSamplers[i];
				ui8ImageUnit = psTextureSampler->ui8ImageUnit;
				ui8SamplerType = psTextureSampler->ui8SamplerTypeIndex;

				if(GLES2_IS_PERM_TEXTURE_UNIT(ui8ImageUnit))
				{
					/* FIXME - do something */
					continue;
				}
				else if(GLES2_IS_GRAD_TEXTURE_UNIT(ui8ImageUnit))
				{
					/* FIXME - do something */
					continue;
				}
				else
				{
					GLES2Texture *psTex;
					IMG_BOOL bUseWhiteDummyTexture, bUseBlackDummyTexture;
					GLES2TextureParamState *psParams;
					IMG_UINT32 ui32DAdjust = EURASIA_PDS_DOUTT_DADJUST_ZERO_UINT, k;
					const IMG_UINT32 ui32StateWord0ClearMask =  EURASIA_PDS_DOUTT0_MIPMAPCLAMP_CLRMSK	&
																EURASIA_PDS_DOUTT0_MAGFILTER_CLRMSK		&
																EURASIA_PDS_DOUTT0_MINFILTER_CLRMSK		&
#if defined(EURASIA_PDS_DOUTT0_TEXFEXT)
													   	       ~EURASIA_PDS_DOUTT0_TEXFEXT      		&
#else
															   ~EURASIA_PDS_DOUTT0_CHANREPLICATE		&
#endif /* defined(EURASIA_PDS_DOUTT0_TEXFEXT) */
															   ~EURASIA_PDS_DOUTT0_MIPFILTER;
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
					IMG_UINT32 ui32StateWord3 = 0;

#if defined(SGX545)
					ui32StateWord3 = (1 << EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT);
#endif
#if defined(SGX_FEATURE_8BIT_DADJUST)
					/* 2 most significant bits of lod bias are in doutt3 */
					ui32StateWord3 |= (((ui32DAdjust >> EURASIA_PDS_DOUTT3_DADJUST_ALIGNSHIFT) << EURASIA_PDS_DOUTT3_DADJUST_SHIFT) & 
										~EURASIA_PDS_DOUTT3_DADJUST_CLRMSK);
#endif
#endif

					/* PRQA S ??? 1 */ /* ui8ImageUnit has been prevalidated in glUniform1i */
					/* PRQA S ??? 1 */ /* ui8SamplerType has been prevalidated in glLinkProgram */
					psTex = gc->sTexture.apsBoundTexture[ui8ImageUnit][ui8SamplerType];
					GLES_ASSERT(psTex);
					psParams = &psTex->sState;

					bUseWhiteDummyTexture = IMG_FALSE;
					bUseBlackDummyTexture = IMG_FALSE;

					if(psTex->ui32NumRenderTargets > 0)
					{
						/*
						 * At least one of the mipmap levels is a render target.
						 * Kick the 3D core before using the texture as a source.
						 */
						FlushUnflushedTextureRenders(gc, psTex);
					}

					if(ui8SamplerType == GLES2_TEXTURE_TARGET_STREAM)
					{
						if(psTex->psFormat)
						{
							psTex->bResidence = IMG_TRUE;
						}
						else
						{
							bUseBlackDummyTexture = IMG_TRUE;
						}
					}
					else if(IsTextureConsistent(gc, psTex, gc->sAppHints.ui32OverloadTexLayout, IMG_TRUE) == GLES2_TEX_CONSISTENT)
					{
						if(!TextureMakeResident(gc, psTex))
						{
							bUseWhiteDummyTexture = IMG_TRUE;
							SetError(gc, GL_OUT_OF_MEMORY);
						}
					}
					else
					{
						bUseBlackDummyTexture = IMG_TRUE;
					}

					if(bUseBlackDummyTexture || bUseWhiteDummyTexture)
					{
						IMG_UINT32 ui32DummyTexAddr, ui32StateWord0;

						ui32StateWord0 = psParams->ui32StateWord0 & ui32StateWord0ClearMask;

						ui32StateWord0 |= (EURASIA_PDS_DOUTT0_NOTMIPMAP |
										   EURASIA_PDS_DOUTT0_MINFILTER_POINT |
										   EURASIA_PDS_DOUTT0_MAGFILTER_POINT |
										  ((ui32DAdjust << EURASIA_PDS_DOUTT0_DADJUST_SHIFT) & ~EURASIA_PDS_DOUTT0_DADJUST_CLRMSK));

						if(bUseBlackDummyTexture)
						{
							ui32DummyTexAddr = (psTexMgr->psBlackDummyTexture->sDevVAddr.uiAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT);
						}
						else
						{
							ui32DummyTexAddr = (psTexMgr->psWhiteDummyTexture->sDevVAddr.uiAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT);
						}
		
						pui32NumChunks[i] = 1;

						psTextureState->aui32TAGControlWord[ui32ChunkNumber][0] = ui32StateWord0;
#if defined(SGX_FEATURE_TAG_SWIZZLE)
						psTextureState->aui32TAGControlWord[ui32ChunkNumber][1] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8888;
#else
						psTextureState->aui32TAGControlWord[ui32ChunkNumber][1] = EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8;
#endif
						psTextureState->aui32TAGControlWord[ui32ChunkNumber][2] = ui32DummyTexAddr << EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;


#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
						psTextureState->aui32TAGControlWord[ui32ChunkNumber][3] = ui32StateWord3;
#endif /* (EURASIA_TAG_TEXTURE_STATE_SIZE == 4) */

						psTextureState->apsTexFormat[i] = &TexFormatABGR8888;
					}
					else
					{
						psParams->ui32StateWord0 &= ui32StateWord0ClearMask;

						/*
						 * Set the mip level clamp to 0 if:
						 * - The texture is CEM
						 * - The texture in memory is mipmapped
						 * - The min filter is not mipmap
						*/
						if((psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM) &&
						   (psTex->ui32HWFlags & GLES2_MIPMAP) &&
						  !(GLES2_IS_MIPMAP(psParams->ui32MinFilter)))
						{
						   psParams->ui32StateWord0 |= (psParams->ui32MinFilter & ~EURASIA_PDS_DOUTT0_MINFILTER_CLRMSK);
						   psParams->ui32StateWord0 |= (EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MIN | EURASIA_PDS_DOUTT0_MIPFILTER);
						}
						else
						{
							psParams->ui32StateWord0 |= psParams->ui32MinFilter;
						}

						psParams->ui32StateWord0 |= psParams->ui32MagFilter;

#if defined(GLES2_EXTENSION_NPOT)
						if(((psTex->ui32HWFlags & GLES2_NONPOW2) != 0) && GLES2_IS_MIPMAP(psParams->ui32MinFilter))
						{
							/* min dadjust (note min is -ve) plus offset.
							 * Then * 8 because the offset is in .125 increments */
							ui32DAdjust = (IMG_UINT32)((-0.5f - EURASIA_PDS_DOUTT_DADJUST_MIN) * 8.0f);
						}
#endif /* defined(GLES2_EXTENSION_NPOT) */

						psParams->ui32StateWord0 |= ((ui32DAdjust << EURASIA_PDS_DOUTT0_DADJUST_SHIFT) & ~EURASIA_PDS_DOUTT0_DADJUST_CLRMSK);

#if defined(GLES2_EXTENSION_NPOT)
			 	 		if (((psTex->ui32HWFlags & GLES2_NONPOW2) != 0) && 
			 	 		  ((psParams->ui32MinFilter & ~EURASIA_PDS_DOUTT0_MIPMAPCLAMP_CLRMSK)!=EURASIA_PDS_DOUTT0_NOTMIPMAP))
				 		{
							/* Disable mipmapping if the top map is too small */
							if((psTex->psMipLevel[0].ui32Width<MIN_POW2_MIPLEVEL_SIZE) && 
							   (psTex->psMipLevel[0].ui32Height<MIN_POW2_MIPLEVEL_SIZE))
							{
								psParams->ui32StateWord0 &= EURASIA_PDS_DOUTT0_MIPMAPCLAMP_CLRMSK;
								psParams->ui32StateWord0 |= EURASIA_PDS_DOUTT0_NOTMIPMAP;
							}
							else
							{
								/* Clamp to the minimum supported size */
								IMG_UINT32 ui32Width, ui32Height, ui32Lod;

								ui32Width  = psTex->psMipLevel[0].ui32Width>>1;
								ui32Height = psTex->psMipLevel[0].ui32Height>>1;

								ui32Lod = 0;

								while((ui32Width>=MIN_POW2_MIPLEVEL_SIZE) && (ui32Height>=MIN_POW2_MIPLEVEL_SIZE))
								{
									ui32Lod++;

									ui32Width  >>= 1;
									ui32Height >>= 1;
								}

								psParams->ui32StateWord0 &= EURASIA_PDS_DOUTT0_MIPMAPCLAMP_CLRMSK;
								psParams->ui32StateWord0 |= (ui32Lod<<EURASIA_PDS_DOUTT0_MIPMAPCLAMP_SHIFT);
							}
				 		}
#endif /* defined(GLES2_EXTENSION_NPOT) */

#if !defined(SGX_FEATURE_TAG_LUMINANCE_ALPHA)
						/*
						* Using channel replicate on alpha, luminance and luminance alpha textures,
						* to reduce swizzle requirements 
						*/
						switch(psTex->psFormat->ePixelFormat)
						{
							case PVRSRV_PIXEL_FORMAT_A8:
							case PVRSRV_PIXEL_FORMAT_L8:
							case PVRSRV_PIXEL_FORMAT_A8L8:
							case PVRSRV_PIXEL_FORMAT_D32F:
							case PVRSRV_PIXEL_FORMAT_NV12:
							case PVRSRV_PIXEL_FORMAT_YV12:
							{
								psParams->ui32StateWord0 |= EURASIA_PDS_DOUTT0_CHANREPLICATE;
								break;
							}
							default:
								break;
						}
#endif /* !defined(SGX_FEATURE_TAG_LUMINANCE_ALPHA) */
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
						if (ui8SamplerType == GLES2_TEXTURE_TARGET_STREAM)
						{
							IMG_DEV_VIRTADDR uBufferAddr;
							EGLImage *psEGLImage;

							psEGLImage = psTex->psEGLImageTarget;
							uBufferAddr.uiAddr = psEGLImage->ui32HWSurfaceAddress;
;

							SetupExternalTextureState(gc, psTex->psExtTexState, psTex, psTextureState,
												&ui32ChunkNumber, uBufferAddr, psTex->psEGLImageTarget->ui32Height,
												psTex->psEGLImageTarget->ui32Stride
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
												, ui32StateWord3
#endif
#if defined(SUPPORT_NV12_FROM_2_HWADDRS)
												,psEGLImage->ui32HWSurfaceAddress2
#endif
												);
						}
						else


#elif defined(GLES2_EXTENSION_TEXTURE_STREAM)
						if(ui8SamplerType == GLES2_TEXTURE_TARGET_STREAM)
						{
							GLES2StreamDevice *psBufferDevice = psTex->psBufferDevice;
							GLES2DeviceBuffer *psStreamBuffer;
							IMG_DEV_VIRTADDR uBufferAddr;
							GLES_ASSERT(psBufferDevice);
							psStreamBuffer = &psBufferDevice->psBuffer[psTex->ui32BufferOffset];
							uBufferAddr = psBufferDevice->psBuffer[psTex->ui32BufferOffset].psBufferSurface->sDevVAddr;
							SetupExternalTextureState(gc, psBufferDevice->psExtTexState, psTex, psTextureState,
							                          &ui32ChunkNumber, uBufferAddr, psStreamBuffer->ui32PixelHeight,
							                          psStreamBuffer->ui32ByteStride
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
													  , ui32StateWord3
#endif
													  );
						}
						else
#endif /* defined(GLES2_EXTENSION_TEXTURE_STREAM) */
						{

#if defined(GLES2_EXTENSION_EGL_IMAGE)
							if(psTex->psEGLImageTarget && !psTex->psEGLImageTarget->bTwiddled)
							{
								IMG_UINT32 ui32StateWord0 = psParams->ui32StateWord0;
								IMG_UINT32 ui32StateWord1 = psParams->aui32StateWord1[0];

#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
								SetupEGLImageArbitraryStride(psTex, &ui32StateWord0, &ui32StateWord1);
#endif

								psTextureState->aui32TAGControlWord[ui32ChunkNumber][0] = ui32StateWord0;
								psTextureState->aui32TAGControlWord[ui32ChunkNumber][1] = ui32StateWord1;
								psTextureState->aui32TAGControlWord[ui32ChunkNumber][2] = psParams->aui32StateWord2[0];
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
								psTextureState->aui32TAGControlWord[ui32ChunkNumber][3] = ui32StateWord3;
#endif /* (EURASIA_TAG_TEXTURE_STATE_SIZE == 4) */
							}
							else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
							{
#if defined(SGX545)
								if(psTex->psFormat->ePixelFormat==PVRSRV_PIXEL_FORMAT_D32F)
								{
									IMG_UINT32 ui32StateWord0 = psParams->ui32StateWord0;
									IMG_UINT32 ui32StateWord1 = psParams->aui32StateWord1[0];

									Setup545DepthStencilStride(psTex, &ui32StateWord0, &ui32StateWord1);

									psTextureState->aui32TAGControlWord[ui32ChunkNumber][0] = ui32StateWord0;
									psTextureState->aui32TAGControlWord[ui32ChunkNumber][1] = ui32StateWord1;
									psTextureState->aui32TAGControlWord[ui32ChunkNumber][2] = psParams->aui32StateWord2[0];
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
									psTextureState->aui32TAGControlWord[ui32ChunkNumber][3] = ui32StateWord3;
#endif /* (EURASIA_TAG_TEXTURE_STATE_SIZE == 4) */
								}
								else

#endif /* defined(SGX545) */
								{
									for(k=0; k < psTex->psFormat->ui32NumChunks; k++)
									{
										psTextureState->aui32TAGControlWord[ui32ChunkNumber + k][0] = psParams->ui32StateWord0;
										psTextureState->aui32TAGControlWord[ui32ChunkNumber + k][1] = psParams->aui32StateWord1[k];
										psTextureState->aui32TAGControlWord[ui32ChunkNumber + k][2] = psParams->aui32StateWord2[k];
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
										psTextureState->aui32TAGControlWord[ui32ChunkNumber + k][3] = ui32StateWord3;
#endif /* (EURASIA_TAG_TEXTURE_STATE_SIZE == 4) */
									}
								}
							}
						}
							
						pui32NumChunks[i] = psTex->psFormat->ui32NumChunks;
						psTextureState->apsTexFormat[i] = psTex->psFormat;

						if(psTex->bHasEverBeenGhosted)
						{
							psTextureState->bSomeTexturesWereGhosted = IMG_TRUE;
						}
					}

					psTextureState->ui32ImageUnitEnables |= (1U << i);
				}
			}
		}
	}
}

/******************************************************************************
 End of file (texmgmt.c)
******************************************************************************/
