/**************************************************************************
 * Name         : texdata.c
 *
 * Copyright    : 2003-2009 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means,electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, 
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: texdata.c $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include <kernel/dmacmgr.h>
#include <ult.h>

#include "context.h"
#include "twiddle.h"
#include "texformat.h"
#include "psp2/swtexop.h"

#define ABSVALUE(X)  ( X >= 0 ? X : 0 - X)

#define MAXVALUE(X, Y) (X >= Y ? X : Y)

#define MINVALUE(X, Y) (X >= Y ? Y : X)

#define ISALIGNED(V, A) (((V) & ((1UL << (A)) - 1)) == 0) 



/*******************************************************************************************************
 Function Name      : PrepareHWTQTextureUpload
 Inputs             : gc, 
                    : psTex, ui32OffsetInBytes, psMipLevel
                    : psSubTexInfo, 
                    : pfnCopyTextureData, ui32AppRowSize, pui8AppPixels
 Outputs            : psQueueTransfer
 Returns            : bSuccessful

 Description        : Checks whether to try to use hardware transfer queue to upload 
                    : a whole level or sub level (square/rectangle/nonpow2) texture
                      with the TILED, LINEAR or TWIDDLED memlayout
                    :
                    : This function would be used in glTexImage2D & glTexSubImage2D only, 
                    : in which case, application supplies the texture source data.
*******************************************************************************************************/

IMG_INTERNAL IMG_BOOL PrepareHWTQTextureUpload(GLES1Context       *gc, 
											   GLESTexture        *psTex,
											   IMG_UINT32          ui32OffsetInBytes,
											   GLESMipMapLevel    *psMipLevel, 
											   GLESSubTextureInfo *psSubTexInfo,
											   PFNCopyTextureData  pfnCopyTextureData,
											   IMG_UINT32          ui32AppRowSize,
											   IMG_UINT8          *pui8AppPixels,
											   SGX_QUEUETRANSFER  *psQueueTransfer)
{

	PVRSRV_PIXEL_FORMAT ePixelFormat;
	IMG_UINT32 ui32BytesPerTexel = psMipLevel->psTexFormat->ui32TotalBytesPerTexel; 

	IMG_BOOL   bUseAppData = IMG_FALSE;
	IMG_UINT32 ui32SubTexStrideInBytes = ui32AppRowSize;
	IMG_UINT32 ui32SubTexRowByteIncrement = 0;

	IMG_PBYTE  pbySrcLinAddr = IMG_NULL;
	IMG_UINT32 ui32SrcWidth;
	IMG_UINT32 ui32SrcHeight;
	IMG_UINT32 ui32SrcChunkStride;
	IMG_INT32  i32SrcStrideInBytes;
	SGXTQ_MEMLAYOUT eSrcMemLayout = SGXTQ_MEMLAYOUT_STRIDE;

	IMG_UINT32 ui32DstUiAddr;
	IMG_UINT32 ui32DstWidth;
	IMG_UINT32 ui32DstHeight;
	IMG_UINT32 ui32DstRoundedWidth;
	IMG_UINT32 ui32DstRoundedHeight;
	IMG_UINT32 ui32DstChunkStride;
	IMG_INT32  i32DstStrideInBytes;
	SGXTQ_MEMLAYOUT eDstMemLayout;
	
	IMG_INT32  i32SrcRectX0;
	IMG_INT32  i32SrcRectX1;
	IMG_INT32  i32SrcRectY0;
	IMG_INT32  i32SrcRectY1;
	
	IMG_INT32  i32DstRectX0;
	IMG_INT32  i32DstRectX1;
	IMG_INT32  i32DstRectY0;
	IMG_INT32  i32DstRectY1;

	IMG_INT32 i32ClampX0, i32ClampX1, i32ClampY0, i32ClampY1;


	/* Assert HWTQTextureUpload is enabled */
	GLES1_ASSERT(!gc->sAppHints.bDisableHWTQTextureUpload);


	/***** Firstly check the basic restriction on texture size *****/

	if ( !(psMipLevel->ui32Width) ||    /* non zero width */
		 !(psMipLevel->ui32Height) ||   /* non zero height */
		 !(psMipLevel->ui32ImageSize) ) /* non zero size */
	{
		PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: unsupported zero-sized texture surface"));
		return IMG_FALSE;
	}

	if ( psSubTexInfo &&
		 (!(psSubTexInfo->ui32SubTexWidth) || 
		  !(psSubTexInfo->ui32SubTexHeight)) )
	{
		PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: unsupported zero-sized texture surface rectangular"));
		return IMG_FALSE;
	}

	if ( ((psMipLevel->ui32Width == 1) && (psMipLevel->ui32Height > 1))
#if defined(FIX_HW_BRN_23054)
		 ||
		 ((psMipLevel->ui32Width  < EURASIA_ISPREGION_SIZEX) ||
		  (psMipLevel->ui32Height < EURASIA_ISPREGION_SIZEY))
#endif
		)
	{
		PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: unsupported texture surface size"));
		return IMG_FALSE;
	}


	/***** Prepare HWTQ pixel format *****/

	switch(psMipLevel->psTexFormat->ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_ARGB8888: /* RGB8888, RGBA8888 */
		case PVRSRV_PIXEL_FORMAT_ABGR8888: /* BGRA8888 */
		case PVRSRV_PIXEL_FORMAT_ARGB1555: /* ARGB1555 */
		case PVRSRV_PIXEL_FORMAT_ARGB4444: /* ARGB4444 */
		case PVRSRV_PIXEL_FORMAT_RGB565: /* RGB565 */
		case PVRSRV_PIXEL_FORMAT_A8: /* ALPHA */
		case PVRSRV_PIXEL_FORMAT_L8: /* LUMINANCE */
		case PVRSRV_PIXEL_FORMAT_A8L8: /* LUMINANCE ALPHA */
		{
			ePixelFormat = psMipLevel->psTexFormat->ePixelFormat;
			break;
		}
	    case PVRSRV_PIXEL_FORMAT_PVRTC2:
	    case PVRSRV_PIXEL_FORMAT_PVRTC4:
		case PVRSRV_PIXEL_FORMAT_PVRTCII2:
		case PVRSRV_PIXEL_FORMAT_PVRTCII4:
		case PVRSRV_PIXEL_FORMAT_PVRTCIII:
		{
		    PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: unsupported compressed texture format"));
			return IMG_FALSE;
		}
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
		case PVRSRV_PIXEL_FORMAT_D32F:
		{
		    PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: unsupported multichunk texture format"));
			return IMG_FALSE;
		}
		default:
		{
		    PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: unsupported texture format"));
			return IMG_FALSE;
		}
	}

	/***** Prepare HWTQ Src, Dst, SrcRect, DstRect *****/

	/* Prepare for the whole texture mipmap level */
	if (!psSubTexInfo)
	{
		pbySrcLinAddr       = psMipLevel->pui8Buffer;
		ui32SrcWidth        = psMipLevel->ui32Width;
		ui32SrcHeight       = psMipLevel->ui32Height; 
		ui32SrcChunkStride  = 0;  /* Would need to be set for >32bpp formats */
		i32SrcStrideInBytes = (IMG_INT32)((psMipLevel->ui32Width) * ui32BytesPerTexel);
	
#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psTex->psEGLImageTarget)
		{
			ui32DstUiAddr   = psTex->psEGLImageTarget->psMemInfo->sDevVAddr.uiAddr + ui32OffsetInBytes;
		}
		else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
		{
			ui32DstUiAddr   = psTex->psMemInfo->sDevVAddr.uiAddr + ui32OffsetInBytes;
		}
		ui32DstWidth        = psMipLevel->ui32Width;
		ui32DstHeight       = psMipLevel->ui32Height;

		ui32DstRoundedWidth  = ui32DstWidth;
		ui32DstRoundedHeight = ui32DstHeight;

		ui32DstChunkStride  = 0;  /* Would need to be set for >32bpp formats */

		i32SrcRectX0 = 0;
		i32SrcRectY0 = 0;
		i32SrcRectX1 = (IMG_INT32)psMipLevel->ui32Width;
		i32SrcRectY1 = (IMG_INT32)psMipLevel->ui32Height;
	
		i32DstRectX0 = 0;
		i32DstRectY0 = 0;
		i32DstRectX1 = (IMG_INT32)psMipLevel->ui32Width;
		i32DstRectY1 = (IMG_INT32)psMipLevel->ui32Height;
	}
	else /* Prepare for the sub texture */
	{
		IMG_UINT32 ui32Align = gc->sState.sClientPixel.ui32UnpackAlignment;
		IMG_UINT32 ui32Padding = ui32AppRowSize % ui32Align;
		
		if (!psSubTexInfo->pui8SubTexBuffer)
		{		  
		    PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: unable to allocate sub texture buffer"));
			SetError(gc, GL_OUT_OF_MEMORY);	  
			return IMG_FALSE;
		}

		if(ui32Padding)
		{
			ui32SubTexStrideInBytes = ui32AppRowSize + (ui32Align - ui32Padding);
		}

		/* If the copy function is NULL we assume no copy is needed */
		if (pfnCopyTextureData == IMG_NULL)
		{
			/* If there is no copy function, ensure we aren't supplied a row size */
			GLES1_ASSERT(ui32AppRowSize == 0);
			bUseAppData = IMG_TRUE;
		}
		else if ( (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture32Bits) ||
			 (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture16Bits) ||
			 (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture8Bits) )
		{
			if (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture32Bits)
			{
				ui32SubTexRowByteIncrement = ui32SubTexStrideInBytes - (psSubTexInfo->ui32SubTexWidth * sizeof(IMG_UINT32));
			}
			else if (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture16Bits)
			{
				ui32SubTexRowByteIncrement = ui32SubTexStrideInBytes - (psSubTexInfo->ui32SubTexWidth * sizeof(IMG_UINT16));
			}
			else if (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture8Bits)
			{
				ui32SubTexRowByteIncrement = ui32SubTexStrideInBytes - (psSubTexInfo->ui32SubTexWidth * sizeof(IMG_UINT8));
			}

			if (!ui32SubTexRowByteIncrement)
			{
				bUseAppData = IMG_TRUE;
			}
			else
			{
				bUseAppData = IMG_FALSE;
			}
		}
		else if ( (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture5551) ||
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture4444) ||
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture888X) ||
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture888to565) ||
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture565toRGBX8888) ||
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture5551to4444) ||
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture5551toRGBA8888) ||
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture4444to5551) ||
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTextureRGBA8888to5551) ||
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTextureRGBA8888to4444) ||
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture5551toBGRA8888) || 
				  (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture4444toRGBA8888)
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
				  || (pfnCopyTextureData == (PFNCopyTextureData) CopyTexture8888BGRAto8888RGBA)
				  || (pfnCopyTextureData == (PFNCopyTextureData) CopyTextureBGRA8888to5551)
				  || (pfnCopyTextureData == (PFNCopyTextureData) CopyTextureBGRA8888to4444)
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
				)
		{
			bUseAppData = IMG_FALSE;
		}
		else
		{
			PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: Unsupported pfnCopyTextureData function"));
			return IMG_FALSE;
		}

		ui32SrcWidth        = psSubTexInfo->ui32SubTexWidth;
		ui32SrcHeight       = psSubTexInfo->ui32SubTexHeight;
		ui32SrcChunkStride  = 0; /* Would need to be set for >32bpp formats */
		i32SrcStrideInBytes = (IMG_INT32)(psSubTexInfo->ui32SubTexWidth * ui32BytesPerTexel);

#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psTex->psEGLImageTarget)
		{
			ui32DstUiAddr   = psTex->psEGLImageTarget->psMemInfo->sDevVAddr.uiAddr + ui32OffsetInBytes;
		}
		else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
		{
			ui32DstUiAddr   = psTex->psMemInfo->sDevVAddr.uiAddr + ui32OffsetInBytes;
		}
		ui32DstWidth        = psMipLevel->ui32Width;
		ui32DstHeight       = psMipLevel->ui32Height;

		ui32DstRoundedWidth  = ui32DstWidth;
		ui32DstRoundedHeight = ui32DstHeight;

		ui32DstChunkStride  = 0; /* Would need to be set for >32bpp formats */
	
		i32SrcRectX0 = 0;
		i32SrcRectY0 = 0;
		i32SrcRectX1 = (IMG_INT32)(psSubTexInfo->ui32SubTexWidth);
		i32SrcRectY1 = (IMG_INT32)(psSubTexInfo->ui32SubTexHeight);
	
		i32DstRectX0 = (IMG_INT32)(psSubTexInfo->ui32SubTexXoffset);
		i32DstRectY0 = (IMG_INT32)(psSubTexInfo->ui32SubTexYoffset);
		i32DstRectX1 = (IMG_INT32)(psSubTexInfo->ui32SubTexXoffset + psSubTexInfo->ui32SubTexWidth);
		i32DstRectY1 = (IMG_INT32)(psSubTexInfo->ui32SubTexYoffset + psSubTexInfo->ui32SubTexHeight);

	}

	/* Calculate i32ClampX0, i32ClampX1, i32ClampY0, i32ClampY1 */
	i32ClampX0 = MAXVALUE(0L, i32DstRectX0);
	i32ClampX1 = MINVALUE((IMG_INT32)ui32DstWidth, i32DstRectX1);
	i32ClampY0 = MAXVALUE(0L, i32DstRectY0);
	i32ClampY1 = MINVALUE((IMG_INT32)ui32DstHeight, i32DstRectY1);
	
	if (i32ClampX1 > 16)
	{
		/* i32ClampX0 is used in the following conditionally: #if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) && defined(FIX_HW_BRN_23054) */
		i32ClampX0 = MAXVALUE((IMG_INT32)((IMG_UINT32)(i32ClampX1 - 1) & ~15UL), i32ClampX0);	/* PRQA S 3199 */
	}
	if (i32ClampY1 > 16)
	{
		/* i32ClampY0 is used in the following conditionally: #if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) && defined(FIX_HW_BRN_23054) */
		i32ClampY0 = MAXVALUE((IMG_INT32)((IMG_UINT32)(i32ClampY1 - 1) & ~15UL), i32ClampY0);	/* PRQA S 3199 */
	}

	/***** Prepare HWTQ Dst's eDstMemLayout, i32DstStrideInBytes *****/

	switch(psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK)
	{
		case EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE:
		{
			/* texture upload as LINEAR */ 
			eDstMemLayout = SGXTQ_MEMLAYOUT_OUT_LINEAR; 

#if EURASIA_TAG_STRIDE_THRESHOLD
			if (ui32DstWidth < EURASIA_TAG_STRIDE_THRESHOLD)
			{
			    ui32DstRoundedWidth = (ui32DstWidth + (EURASIA_TAG_STRIDE_ALIGN0-1)) & ~(EURASIA_TAG_STRIDE_ALIGN0-1); 
			}
			else
#endif					
			{
			    ui32DstRoundedWidth = (ui32DstWidth + (EURASIA_TAG_STRIDE_ALIGN1-1)) & ~(EURASIA_TAG_STRIDE_ALIGN1-1); 
			}			  

			i32DstStrideInBytes = (IMG_INT32)(ui32DstRoundedWidth * ui32BytesPerTexel);

			/* Check HWTQ texture upload restriction when Dst's layout is stride */
			if ( !ISALIGNED((IMG_UINT32)i32DstStrideInBytes, EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT)
#if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) && defined(FIX_HW_BRN_23054)
				 || (i32ClampX1 - i32ClampX0 < 3)
				 || (i32ClampY1 - i32ClampY0 < 3)
#endif				 
			   )
			{
				PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: HWTQ failure can be predicted!"));
				return IMG_FALSE;
			}

			break;

		}
		case EURASIA_PDS_DOUTT1_TEXTYPE_TILED:
		{
			/* texture upload as TILED */ 
			eDstMemLayout = SGXTQ_MEMLAYOUT_OUT_TILED; 

			ui32DstRoundedWidth = (ui32DstWidth + (EURASIA_TAG_TILE_SIZEX-1)) & ~(EURASIA_TAG_TILE_SIZEX-1);

			ui32DstRoundedHeight = (ui32DstHeight + (EURASIA_TAG_TILE_SIZEY-1)) & ~(EURASIA_TAG_TILE_SIZEY-1);

			i32DstStrideInBytes = (IMG_INT32)(ui32DstRoundedWidth * ui32BytesPerTexel);

			/* Check HWTQ texture upload restriction when Dst's layout is tiled */
#if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) && defined(FIX_HW_BRN_23054)
			if ( (i32ClampX1 - i32ClampX0 < 3) || (i32ClampY1 - i32ClampY0 < 3) )
			{
				PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: HWTQ failure can be predicted!"));
				return IMG_FALSE;
			}
#endif	
			break;

		}
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		case EURASIA_PDS_DOUTT1_TEXTYPE_2D:
#else
		case EURASIA_PDS_DOUTT1_TEXTYPE_ARB_2D:
#endif
		default:
		{
			IMG_UINT32	ui32POTWidth = 1;
			IMG_UINT32	ui32POTHeight = 1;
		  
			/* texture upload as TWIDDLED */ 
			eDstMemLayout = SGXTQ_MEMLAYOUT_OUT_TWIDDLED; 
				
			while (ui32POTWidth < ui32DstWidth)
			{
					ui32POTWidth <<= 1;	
			}
			while (ui32POTHeight < ui32DstHeight)
			{
					ui32POTHeight <<= 1;	
			}

			ui32DstRoundedWidth  = ui32POTWidth;
			ui32DstRoundedHeight = ui32POTHeight;

			i32DstStrideInBytes = 0;

			/* Check HWTQ texture upload restriction when Dst's layout is twiddled */
			if ( ( ( (ui32DstRoundedWidth < EURASIA_ISPREGION_SIZEX) || (ui32DstRoundedHeight < EURASIA_ISPREGION_SIZEY) ) &&
				   (ui32DstRoundedWidth != ui32DstRoundedHeight) &&
				   (ui32DstRoundedWidth<<1 != ui32DstRoundedHeight) &&
				   ( (ui32DstRoundedWidth  != (IMG_UINT32)ABSVALUE(i32DstRectX1 - i32DstRectX0)) ||
					 (ui32DstRoundedHeight != (IMG_UINT32)ABSVALUE(i32DstRectY1 - i32DstRectY0)) ) )
#if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) && defined(FIX_HW_BRN_23054)
				 || ( (i32ClampX1 - i32ClampX0 < 7) || 
					  (i32ClampY1 - i32ClampY0 < 7) )
#endif	
			   )
			{
				PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: HWTQ failure can be predicted!"));
				return IMG_FALSE;
			}

			// PSP2: we probably don't need this check since we use PTLA instead of 3D core
			/* Check HWTQ texture upload subtile twiddling restriction */
#if !defined(__psp2__)
#if (EURASIA_ISPREGION_SIZEX != 16) || (EURASIA_ISPREGION_SIZEY != 16) || (EURASIA_TAPDSSTATE_PIPECOUNT != 2) || \
	(defined(FIX_HW_BRN_23615) && defined(FIX_HW_BRN_23070)) || defined(FIX_HW_BRN_26361) || defined(FIX_HW_BRN_28825)
			if ((ui32DstRoundedWidth < EURASIA_ISPREGION_SIZEX) || (ui32DstRoundedHeight < EURASIA_ISPREGION_SIZEY))
			{
				PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureUpload: HWTQ subtile twiddling failure can be predicted!"));
				return IMG_FALSE;
			}
#endif
#endif

			break;
		}   
	}


	/* Prepare for Src's pbySrcLinAddr when uploading subtex */
	if (psSubTexInfo)
	{
		if (bUseAppData)
		{
			/* directly upload the application texture data */
			pbySrcLinAddr = pui8AppPixels;
		}
		else
		{
		    /* pSubTexBuffer is pui8Dest here */
		    (*pfnCopyTextureData)(psSubTexInfo->pui8SubTexBuffer, 
								  pui8AppPixels, 
								  psSubTexInfo->ui32SubTexWidth, 
								  psSubTexInfo->ui32SubTexHeight,
								  ui32SubTexStrideInBytes, 
								  psMipLevel, 
								  IMG_FALSE);
			
			/* upload the data pointed by pui8SubTexBuffer */
		   pbySrcLinAddr = psSubTexInfo->pui8SubTexBuffer;
		}
	}	  


	/***** Setup HWTQ with all prepared values *****/

	PVR_UNREFERENCED_PARAMETER(gc);
	GLES1MemSet(psQueueTransfer, 0, sizeof(SGX_QUEUETRANSFER));

	psQueueTransfer->eType = SGXTQ_BLIT;

	psQueueTransfer->Details.sTextureUpload.pbySrcLinAddr = pbySrcLinAddr;
	psQueueTransfer->Details.sTextureUpload.ui32BytesPP = ui32BytesPerTexel;

	psQueueTransfer->ui32NumSources = 1;
	psQueueTransfer->asSources[0].eFormat          = ePixelFormat;
	psQueueTransfer->asSources[0].ui32Width        = ui32SrcWidth;
	psQueueTransfer->asSources[0].ui32Height       = ui32SrcHeight;
	psQueueTransfer->asSources[0].eMemLayout       = eSrcMemLayout;
	psQueueTransfer->asSources[0].ui32ChunkStride  = ui32SrcChunkStride; 
	psQueueTransfer->asSources[0].i32StrideInBytes = i32SrcStrideInBytes;

	psQueueTransfer->ui32NumDest = 1;
	psQueueTransfer->asDests[0].eFormat          = ePixelFormat;
	psQueueTransfer->asDests[0].sDevVAddr.uiAddr = ui32DstUiAddr;
	psQueueTransfer->asDests[0].ui32Width        = ui32DstRoundedWidth;
	psQueueTransfer->asDests[0].ui32Height       = ui32DstRoundedHeight;
	psQueueTransfer->asDests[0].eMemLayout       = eDstMemLayout;
	psQueueTransfer->asDests[0].ui32ChunkStride  = ui32DstChunkStride;
	if (eDstMemLayout == SGXTQ_MEMLAYOUT_OUT_TWIDDLED && i32DstStrideInBytes == 0)
		psQueueTransfer->asDests[0].i32StrideInBytes = ui32DstRoundedWidth * ui32BytesPerTexel;
	else
		psQueueTransfer->asDests[0].i32StrideInBytes = i32DstStrideInBytes;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageTarget)
	{
		psQueueTransfer->asDests[0].psSyncInfo   = psTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
	}
	else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
	{
		psQueueTransfer->asDests[0].psSyncInfo   = psTex->psMemInfo->psClientSyncInfo;
	}	

  	psQueueTransfer->ui32NumSrcRects = 1; 
	psQueueTransfer->asSrcRects[0].x0 = i32SrcRectX0; 
	psQueueTransfer->asSrcRects[0].y0 = i32SrcRectY0; 
	psQueueTransfer->asSrcRects[0].x1 = i32SrcRectX1;
	psQueueTransfer->asSrcRects[0].y1 = i32SrcRectY1;

	psQueueTransfer->ui32NumDestRects = 1;
	psQueueTransfer->asDestRects[0].x0 = i32DstRectX0;
	psQueueTransfer->asDestRects[0].y0 = i32DstRectY0;
	psQueueTransfer->asDestRects[0].x1 = i32DstRectX1;
	psQueueTransfer->asDestRects[0].y1 = i32DstRectY1;

	psQueueTransfer->ui32NumStatusValues = 0; 
	psQueueTransfer->ui32Flags = SGX_KICKTRANSFER_FLAGS_3DTQ_SYNC;
	psQueueTransfer->bPDumpContinuous = IMG_TRUE;

	psQueueTransfer->Details.sBlit.bEnableGamma = 0;
	psQueueTransfer->Details.sBlit.bEnablePattern = 0;
	psQueueTransfer->Details.sBlit.bSingleSource = 0;
	psQueueTransfer->Details.sBlit.byCustomRop3 = 0;
	psQueueTransfer->Details.sBlit.byGlobalAlpha = 0;
	psQueueTransfer->Details.sBlit.eAlpha = 0;
	psQueueTransfer->Details.sBlit.eColourKey = 0;
	psQueueTransfer->Details.sBlit.eCopyOrder = 0;
	psQueueTransfer->Details.sBlit.eFilter = 0;
	psQueueTransfer->Details.sBlit.eRotation = 0;
	psQueueTransfer->Details.sBlit.sUSEExecAddr.uiAddr = 0;
	psQueueTransfer->Details.sBlit.ui32ColourKey = 0;
	psQueueTransfer->Details.sBlit.ui32ColourKeyMask = 0;
	psQueueTransfer->Details.sBlit.uiNumTemporaryRegisters = 0;
	psQueueTransfer->Details.sBlit.UseParams[0] = 0;
	psQueueTransfer->Details.sBlit.UseParams[1] = 0;

	psQueueTransfer->asSources[0].sDevVAddr.uiAddr = pbySrcLinAddr;

	return IMG_TRUE;

}


/***********************************************************************************
 Function Name      : HWTQTextureUpload
 Inputs             : gc, psTex, psQueueTransfer
 Outputs            : -
 Returns            : Success/failure
 Description        : Upload a sub square/rectangle/nonpow2 texture
                      using the device with the TILED, LINEAR or TWIDDLED memlayout
************************************************************************************/

IMG_INTERNAL IMG_BOOL HWTQTextureUpload(GLES1Context *gc,
										GLESTexture *psTex, 
										SGX_QUEUETRANSFER *psQueueTransfer)
{
  
	PVRSRV_ERROR eResult;

#if defined(DEBUG)
	SceKernelMemBlockInfo sMemInfo;
	sMemInfo.size = sizeof(SceKernelMemBlockInfo);
	sMemInfo.memoryType = 0;
	sceKernelGetMemBlockInfoByAddr(psQueueTransfer->asSources[0].sDevVAddr.uiAddr, &sMemInfo);
	if (sMemInfo.memoryType == SCE_KERNEL_MEMBLOCK_TYPE_USER_RW)
	{
		PVR_DPF((PVR_DBG_WARNING, "HWTQTextureUpload: Texture upload source is cached memory. Performance will be negatively affected"));
	}
#endif

	eResult = SGXQueueTransfer(&gc->psSysContext->s3D, gc->psSysContext->hTransferContext, psQueueTransfer);

	if(eResult != PVRSRV_OK)
	{
		PVRSRV_CLIENT_SYNC_INFO	*psDstSyncInfo;

		PVR_DPF((PVR_DBG_WARNING, "HWTQTextureUpload: Failed to load texture image (error=%d). Falling back to SW", eResult));

#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psTex->psEGLImageTarget)
		{
			psDstSyncInfo = psTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
		}
		else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
		{
			psDstSyncInfo = psTex->psMemInfo->psClientSyncInfo;
		}	

		/* check the texture has been uploaded before sw fallback */
		if(psDstSyncInfo)
		{		
#if defined(PDUMP)

			PVRSRVPDumpSyncPol( gc->ps3DDevData->psConnection,
								psDstSyncInfo,
								IMG_FALSE,
								psDstSyncInfo->psSyncData->ui32WriteOpsPending,
								0xFFFFFFFF);
		
#endif	/*defined (PDUMP)*/

			/* HWTQTextureUpload: waiting for previous texture transfer */
			while (SGX2DQueryBlitsComplete(&gc->psSysContext->s3D, psDstSyncInfo, IMG_TRUE) != PVRSRV_OK)
			{
			}
		}

		return IMG_FALSE;
	}

  
	return IMG_TRUE;

}


/*******************************************************************************************************
 Function Name      : PrepareHWTQTextureNormalBlit
 Inputs             : gc, 
                    : Dst: psDstTex, ui32DstOffsetInBytes, psDstMipLevel, psDstSubTexInfo,
                    : Src: psSrcReadParams, psSrcSubTexInfo
 Outputs            : psQueueTransfer
 Returns            : bSuccessful

 Description        : Checks whether to try to use hardware transfer queue to normal blit 
                    : a whole level or sub level (square/rectangle/nonpow2) texture
                    : with the TILED, LINEAR or TWIDDLED memlayout
                    : from one device memory to another device memory
                    :
                    : This function would be used in glCopyTexImage2D & glCopyTexSubImage2D only, 
                    : (when the texture's device memory has already been allocated and uploaded before,
                    :  and the copy function won't change texture's size, format, level number, ...)
                    : in which case, framebuffer surface supplies the texture source data.
*******************************************************************************************************/

IMG_INTERNAL IMG_BOOL PrepareHWTQTextureNormalBlit(GLES1Context       *gc, 
												   GLESTexture        *psDstTex,
												   IMG_UINT32          ui32DstOffsetInBytes,
												   GLESMipMapLevel    *psDstMipLevel,
												   GLESSubTextureInfo *psDstSubTexInfo,
												   EGLDrawableParams  *psSrcReadParams,
												   GLESSubTextureInfo *psSrcReadInfo,
												   SGX_QUEUETRANSFER  *psQueueTransfer)
{
    PVRSRV_PIXEL_FORMAT eSrcPixelFormat;
	IMG_UINT32          ui32SrcBytesPerTexel;
	IMG_MEMLAYOUT       eSrcMemFormat = GetColorAttachmentMemFormat(gc, gc->sFrameBuffer.psActiveFrameBuffer);
	SGXTQ_MEMLAYOUT	    eSrcMemLayout;

	PVRSRV_PIXEL_FORMAT eDstPixelFormat;
	IMG_UINT32          ui32DstBytesPerTexel;
	IMG_UINT32          ui32DstRoundedWidth;
	IMG_UINT32          ui32DstRoundedHeight;
	SGXTQ_MEMLAYOUT	    eDstMemLayout;

	IMG_UINT32 ui32SrcUiAddr;
	IMG_UINT32 ui32SrcWidth;
	IMG_UINT32 ui32SrcHeight;
	IMG_UINT32 ui32SrcChunkStride;
	IMG_INT32  i32SrcStrideInBytes;

	IMG_UINT32 ui32DstUiAddr;
	IMG_UINT32 ui32DstWidth;
	IMG_UINT32 ui32DstHeight;
	IMG_UINT32 ui32DstChunkStride;
	IMG_INT32  i32DstStrideInBytes;

	IMG_INT32  i32SrcRectX0;
	IMG_INT32  i32SrcRectX1;
	IMG_INT32  i32SrcRectY0;
	IMG_INT32  i32SrcRectY1;

	IMG_INT32  i32DstRectX0;
	IMG_INT32  i32DstRectX1;
	IMG_INT32  i32DstRectY0;
	IMG_INT32  i32DstRectY1;

	IMG_INT32 i32ClampX0, i32ClampX1, i32ClampY0, i32ClampY1;


	/* Assert psSrcReadInfo is not NULL */
	GLES1_ASSERT(psSrcReadInfo);

	/* Assert HWTQNormalBlit is enabled */
	GLES1_ASSERT(!gc->sAppHints.bDisableHWTQNormalBlit);


  /* Check Normal blit limitations */
#if defined(FIX_HW_BRN_27298)
    return IMG_FALSE;
#endif


	/***** prepare and check HWTQ source pixel format
		   also prepare ui32SrcBytesPerTexel     *****/

	eSrcPixelFormat = psSrcReadParams->ePixelFormat;

	switch(eSrcPixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			ui32SrcBytesPerTexel = 2;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		{
			ui32SrcBytesPerTexel = 4;
			break;
		}
	    default:
		{
			PVR_DPF((PVR_DBG_MESSAGE,"PrepareHWTQTextureNormalBlit: Unsupported source pixel format"));
			return IMG_FALSE;
		}
	}
	

	/***** Prepare and check HWTQ destination pixel format 
	       also prepare ui32DstBytesPerTexel          *****/

	eDstPixelFormat = psDstMipLevel->psTexFormat->ePixelFormat;
	ui32DstBytesPerTexel = psDstMipLevel->psTexFormat->ui32TotalBytesPerTexel;

	if (psDstMipLevel->psTexFormat == &TexFormatXBGR8888)
	{
		PVR_DPF((PVR_DBG_MESSAGE,"PrepareHWTQTextureNormalBlit: Unsupported destination texture format"));
		return IMG_FALSE;
	}

	switch(eDstPixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_A8: 
		case PVRSRV_PIXEL_FORMAT_RGB565:
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		{
			break;
		}
		case PVRSRV_PIXEL_FORMAT_L8: 
		case PVRSRV_PIXEL_FORMAT_A8L8:
	    default:
		{
			PVR_DPF((PVR_DBG_MESSAGE,"PrepareHWTQTextureNormalBlit: Unsupported destination pixel format"));
			return IMG_FALSE;
		}
	}


	/***** check whether HWTQ source and destination pixel formats fall into
		   the matching category (which is different between old cores and new cores):
		   current HWTQ do not support swizzling properly on new cores      *****/

#if (EURASIA_TAG_TEXTURE_STATE_SIZE	== 4)
	if ( ( (eSrcPixelFormat == PVRSRV_PIXEL_FORMAT_ARGB8888) &&
		   (eDstPixelFormat == PVRSRV_PIXEL_FORMAT_ABGR8888) ) 
		 ||
		 ( (eSrcPixelFormat == PVRSRV_PIXEL_FORMAT_ABGR8888) &&
		   (eDstPixelFormat == PVRSRV_PIXEL_FORMAT_ARGB8888) ) )
	{
		PVR_DPF((PVR_DBG_MESSAGE,"PrepareHWTQTextureNormalBlit: Unsupported conversion from source pixel format to destination pixel format"));
		return IMG_FALSE;
	}
#endif

	if ( (eSrcPixelFormat == PVRSRV_PIXEL_FORMAT_RGB565) &&
		 (eDstPixelFormat == PVRSRV_PIXEL_FORMAT_A8) )
	{
		PVR_DPF((PVR_DBG_MESSAGE,"PrepareHWTQTextureNormalBlit: illogical conversion from source pixel format to destination pixel format"));
		return IMG_FALSE;
	}


	/***** Check HWTQ SrcReadInfo *****/
	if ( !(psSrcReadInfo->ui32SubTexWidth) ||
		 !(psSrcReadInfo->ui32SubTexHeight) )
	{
		PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureNormalBlit: unsupported zero-sized source"));
		return IMG_FALSE;
	}
	

	/***** Prepare HWTQ source Src, SrcRect *****/
	ui32SrcUiAddr = psSrcReadParams->ui32HWSurfaceAddress;
	ui32SrcWidth  = psSrcReadParams->ui32Width;
	ui32SrcHeight = psSrcReadParams->ui32Height;
	ui32SrcChunkStride = 0;

	switch(psSrcReadParams->eRotationAngle)
	{
		case PVRSRV_ROTATE_0:
		{
			i32SrcRectX0 = (IMG_INT32)psSrcReadInfo->ui32SubTexXoffset;
			i32SrcRectY0 = (IMG_INT32)(psSrcReadParams->ui32Height - psSrcReadInfo->ui32SubTexYoffset - psSrcReadInfo->ui32SubTexHeight);
			i32SrcRectX1 = (IMG_INT32)(psSrcReadInfo->ui32SubTexXoffset + psSrcReadInfo->ui32SubTexWidth);
			i32SrcRectY1 = (IMG_INT32)(psSrcReadParams->ui32Height - psSrcReadInfo->ui32SubTexYoffset);

			break;
		}
		case PVRSRV_ROTATE_90:
		{
			ui32SrcWidth  = psSrcReadParams->ui32Height;
			ui32SrcHeight = psSrcReadParams->ui32Width;
	
			i32SrcRectX0 = (IMG_INT32)psSrcReadInfo->ui32SubTexYoffset;
			i32SrcRectY0 = (IMG_INT32)psSrcReadInfo->ui32SubTexXoffset;
			i32SrcRectX1 = i32SrcRectX0 + psSrcReadInfo->ui32SubTexHeight;
			i32SrcRectY1 = i32SrcRectY0 + psSrcReadInfo->ui32SubTexWidth;

			break;
		}
		case PVRSRV_ROTATE_180:
		{
			i32SrcRectX0 = (IMG_INT32)(ui32SrcWidth - psSrcReadInfo->ui32SubTexXoffset - psSrcReadInfo->ui32SubTexWidth);
			i32SrcRectY0 = (IMG_INT32)psSrcReadInfo->ui32SubTexYoffset;
			i32SrcRectX1 = i32SrcRectX0 + psSrcReadInfo->ui32SubTexWidth;
			i32SrcRectY1 = i32SrcRectX0 + psSrcReadInfo->ui32SubTexHeight;

			break;
		}
		case PVRSRV_ROTATE_270:
		{
			ui32SrcWidth  = psSrcReadParams->ui32Height;
			ui32SrcHeight = psSrcReadParams->ui32Width;
	
			i32SrcRectX0 = (IMG_INT32)(ui32SrcWidth - psSrcReadInfo->ui32SubTexYoffset - psSrcReadInfo->ui32SubTexHeight);
			i32SrcRectY0 = (IMG_INT32)(ui32SrcHeight - psSrcReadInfo->ui32SubTexXoffset - psSrcReadInfo->ui32SubTexWidth);
			i32SrcRectX1 = i32SrcRectX0 + psSrcReadInfo->ui32SubTexHeight;
			i32SrcRectY1 = i32SrcRectY0 + psSrcReadInfo->ui32SubTexWidth;

			break;
		}
		default:
	    case PVRSRV_FLIP_Y:
		{
		    i32SrcRectX0 = (IMG_INT32)psSrcReadInfo->ui32SubTexXoffset;
			i32SrcRectY0 = (IMG_INT32)psSrcReadInfo->ui32SubTexYoffset;
			i32SrcRectX1 = (IMG_INT32)(psSrcReadInfo->ui32SubTexXoffset + psSrcReadInfo->ui32SubTexWidth);
			i32SrcRectY1 = (IMG_INT32)(psSrcReadInfo->ui32SubTexYoffset + psSrcReadInfo->ui32SubTexHeight);

			break;
		}
	}

	/***** Prepare and check HWTQ source layout, i32SrcStrideInBytes *****/
	switch(eSrcMemFormat)
	{
		case IMG_MEMLAYOUT_STRIDED:
		{
		    /* Setup memlayout */
			eSrcMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
			
			/* Setup i32SrcStrideInBytes */
			i32SrcStrideInBytes = (IMG_INT32)(psSrcReadParams->ui32Stride);

			/* Check the limitations on strided source surface */
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION) && !defined(FIX_HW_BRN_26518) && !defined(FIX_HW_BRN_27408)
			if ( ((IMG_UINT32)ABSVALUE(i32SrcStrideInBytes) & 3) )
			{
				PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureNormalBlit: HWTQ failure can be predicted!"));
				return IMG_FALSE;
			}
#else
			/* DO NOT get very complicated at this stage */
#endif			
			break;

		}
		case IMG_MEMLAYOUT_TILED:
		{
		    /* Setup memlayout */
			eSrcMemLayout = SGXTQ_MEMLAYOUT_TILED;

			/* Setup i32SrcStrideInBytes */
			i32SrcStrideInBytes = (IMG_INT32)(psSrcReadParams->ui32Stride);

			break;

		}
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
		case IMG_MEMLAYOUT_HYBRIDTWIDDLED:
		{
			PVR_DPF((PVR_DBG_MESSAGE,"PrepareHWTQTextureNormalBlit: Unsupported source memory layout"));
			return IMG_FALSE;
		}
#else /* defined(SGX_FEATURE_HYBRID_TWIDDLING) */
		case IMG_MEMLAYOUT_TWIDDLED:
#endif /* defined(SGX_FEATURE_HYBRID_TWIDDLING) */
	    default:
		{
		    /* Setup memlayout */
			eSrcMemLayout = SGXTQ_MEMLAYOUT_2D;

			/* Setup i32SrcStrideInBytes */
			i32SrcStrideInBytes = 0;

			/* Check the limitations on twiddled source surface */
			/* No need to check both width and height are power of 2 */
			
			break;
		}
	}


	/***** Prepare and check HWTQ Dst, DstRect *****/
#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psDstTex->psEGLImageTarget)
	{
		ui32DstUiAddr = psDstTex->psEGLImageTarget->psMemInfo->sDevVAddr.uiAddr + ui32DstOffsetInBytes;
	}
	else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
	{
		ui32DstUiAddr = psDstTex->psMemInfo->sDevVAddr.uiAddr + ui32DstOffsetInBytes;
	}
	ui32DstWidth  = psDstMipLevel->ui32Width;
	ui32DstHeight = psDstMipLevel->ui32Height;
	ui32DstChunkStride = 0;
	ui32DstRoundedWidth  = ui32DstWidth;
	ui32DstRoundedHeight = ui32DstHeight;

	if (psDstSubTexInfo)
	{
		i32DstRectX0 = (IMG_INT32)(psDstSubTexInfo->ui32SubTexXoffset);
		i32DstRectY0 = (IMG_INT32)(psDstSubTexInfo->ui32SubTexYoffset);
		i32DstRectX1 = (IMG_INT32)(psDstSubTexInfo->ui32SubTexXoffset + psDstSubTexInfo->ui32SubTexWidth);
		i32DstRectY1 = (IMG_INT32)(psDstSubTexInfo->ui32SubTexYoffset + psDstSubTexInfo->ui32SubTexHeight);
	}
	else
	{
	    i32DstRectX0 = 0;
		i32DstRectY0 = 0;
		i32DstRectX1 = (IMG_INT32)(psDstMipLevel->ui32Width);
		i32DstRectY1 = (IMG_INT32)(psDstMipLevel->ui32Height);
	}	
	
	/* Calculate i32ClampX0, i32ClampX1, i32ClampY0, i32ClampY1 */
	i32ClampX0 = MAXVALUE(0L, i32DstRectX0);
	i32ClampX1 = MINVALUE((IMG_INT32)ui32DstWidth, i32DstRectX1);
	i32ClampY0 = MAXVALUE(0L, i32DstRectY0);
	i32ClampY1 = MINVALUE((IMG_INT32)ui32DstHeight, i32DstRectY1);
	
	if (i32ClampX1 > 16)
	{
		/* i32ClampX0 is used in the following conditionally: #if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) && defined(FIX_HW_BRN_23054) */
		i32ClampX0 = MAXVALUE((IMG_INT32)((IMG_UINT32)(i32ClampX1 - 1) & ~15UL), i32ClampX0);	/* PRQA S 3199 */
	}
	if (i32ClampY1 > 16)
	{
		/* i32ClampY0 is used in the following conditionally: #if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) && defined(FIX_HW_BRN_23054) */
		i32ClampY0 = MAXVALUE((IMG_INT32)((IMG_UINT32)(i32ClampY1 - 1) & ~15UL), i32ClampY0);	/* PRQA S 3199 */
	}


	/***** Prepare and check HWTQ destination layout, i32DstStrideInBytes *****/
	switch(psDstTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK)
	{

		case EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE:
		{
		    /* Setup memlayout */
			eDstMemLayout = SGXTQ_MEMLAYOUT_OUT_LINEAR; 

			/* Setup ui32DstRoundedWidth & i32DstStrideInBytes */
#if EURASIA_TAG_STRIDE_THRESHOLD
			if (ui32DstWidth < EURASIA_TAG_STRIDE_THRESHOLD)
			{
			    ui32DstRoundedWidth = (ui32DstWidth + (EURASIA_TAG_STRIDE_ALIGN0-1)) & ~(EURASIA_TAG_STRIDE_ALIGN0-1);
			}
			else
#endif
			{
				ui32DstRoundedWidth = (ui32DstWidth + (EURASIA_TAG_STRIDE_ALIGN1-1)) & ~(EURASIA_TAG_STRIDE_ALIGN1-1);
			}

			i32DstStrideInBytes = (IMG_INT32)(ui32DstRoundedWidth * ui32DstBytesPerTexel);
			  
			/* Check the limitations on stride destination surface */
			if ( !ISALIGNED((IMG_UINT32)i32DstStrideInBytes, EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT)
#if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) && defined(FIX_HW_BRN_23054)
				 || (i32ClampX1 - i32ClampX0 < 3) 
				 || (i32ClampY1 - i32ClampY0 < 3)
#endif
			   )
			{
				PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureNormalBlit: HWTQ failure can be predicted!"));
				return IMG_FALSE;
			}
			
			break;

		}
		case EURASIA_PDS_DOUTT1_TEXTYPE_TILED:
		{
		    /* Setup memlayout */
			eDstMemLayout = SGXTQ_MEMLAYOUT_OUT_TILED; 

			/* Setup ui32DstRoundedWidth, ui32DstRoundedHeight & i32DstStrideInBytes */
			ui32DstRoundedWidth = (ui32DstWidth + (EURASIA_TAG_TILE_SIZEX-1)) & ~(EURASIA_TAG_TILE_SIZEX-1);

			ui32DstRoundedHeight = (ui32DstHeight + (EURASIA_TAG_TILE_SIZEY-1)) & ~(EURASIA_TAG_TILE_SIZEY-1);

			i32DstStrideInBytes = (IMG_INT32)(ui32DstRoundedWidth * ui32DstBytesPerTexel);

			/* Check the limitations on tiled destination surface */
#if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) && defined(FIX_HW_BRN_23054)
			if ((i32ClampX1 - i32ClampX0 < 3) || (i32ClampY1 - i32ClampY0 < 3))
			{
				PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureNormalBlit: HWTQ failure can be predicted!"));
				return IMG_FALSE;
			}
#endif
			break;

		}		
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		case EURASIA_PDS_DOUTT1_TEXTYPE_2D:
#else
		case EURASIA_PDS_DOUTT1_TEXTYPE_ARB_2D:
#endif
		default:
		{
			IMG_UINT32	ui32POTWidth = 1;
			IMG_UINT32	ui32POTHeight = 1;

		    /* Setup memlayout */
			eDstMemLayout = SGXTQ_MEMLAYOUT_OUT_TWIDDLED; 

			/* Setup ui32DstRoundedWidth & i32DstStrideInBytes */
			while (ui32POTWidth < ui32DstWidth)
			{
					ui32POTWidth <<= 1;	
			}
			while (ui32POTHeight < ui32DstHeight)
			{
					ui32POTHeight <<= 1;	
			}

			ui32DstRoundedWidth  = ui32POTWidth;
			ui32DstRoundedHeight = ui32POTHeight;

			i32DstStrideInBytes = 0;

			/* Check the limitations on twiddled destination surface */
			/* No need to check both width and height are power of 2 */

#if !defined(SGX_FEATURE_UNIFIED_STORE_64BITS) && defined(FIX_HW_BRN_23054)
			if ((i32ClampX1 - i32ClampX0 < 7) || (i32ClampY1 - i32ClampY0 < 7))
			{
				PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureNormalBlit: HWTQ failure can be predicted!"));
				return IMG_FALSE;
			}
#endif			
			break;

		}		
	}


	/***** Setup HWTQ with all prepared values *****/

	PVR_UNREFERENCED_PARAMETER(gc);
	GLES1MemSet(psQueueTransfer, 0, sizeof(SGX_QUEUETRANSFER));

	psQueueTransfer->eType = SGXTQ_BLIT;
	
	psQueueTransfer->Details.sBlit.eFilter             = SGXTQ_FILTERTYPE_POINT;
	psQueueTransfer->Details.sBlit.eColourKey          = SGXTQ_COLOURKEY_NONE;
	psQueueTransfer->Details.sBlit.ui32ColourKey       = 0;
	psQueueTransfer->Details.sBlit.ui32ColourKeyMask   = 0;
	psQueueTransfer->Details.sBlit.bEnableGamma        = IMG_FALSE;
	psQueueTransfer->Details.sBlit.eAlpha              = SGXTQ_ALPHA_NONE;
	psQueueTransfer->Details.sBlit.byGlobalAlpha       = 0;
	psQueueTransfer->Details.sBlit.byCustomRop3        = 0;
	psQueueTransfer->Details.sBlit.sUSEExecAddr.uiAddr = 0;
	psQueueTransfer->Details.sBlit.bEnablePattern      = IMG_FALSE;
	psQueueTransfer->Details.sBlit.bSingleSource       = IMG_TRUE;
	psQueueTransfer->Details.sBlit.eRotation		   = SGXTQ_ROTATION_NONE;

	switch(psSrcReadParams->eRotationAngle)
	{
		case PVRSRV_ROTATE_0:
		default:
		{
		    /* First flip in TSP, then rotate in PBE */
		    psQueueTransfer->ui32Flags = SGX_TRANSFER_FLAGS_INVERTY; /* Flip Y from pbuffer to texture */
			break;
		}
		case PVRSRV_ROTATE_90:
		{
		    psQueueTransfer->ui32Flags = SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE; 
			break;
		}
		case PVRSRV_ROTATE_180:
		{
		    psQueueTransfer->ui32Flags = SGX_TRANSFER_FLAGS_INVERTX; 
			break;
		}
		case PVRSRV_ROTATE_270:
		{
		    psQueueTransfer->ui32Flags = SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE | SGX_TRANSFER_FLAGS_INVERTX | SGX_TRANSFER_FLAGS_INVERTY; 
			break;
	}		
	    case PVRSRV_FLIP_Y:
		{
		    psQueueTransfer->ui32Flags = 0;
			break;
		}
	}

	psQueueTransfer->ui32NumSources = 1;
	psQueueTransfer->asSources[0].sDevVAddr.uiAddr = ui32SrcUiAddr;
	psQueueTransfer->asSources[0].ui32Width  = ui32SrcWidth;
	psQueueTransfer->asSources[0].ui32Height = ui32SrcHeight;
	psQueueTransfer->asSources[0].eFormat = eSrcPixelFormat;
	psQueueTransfer->asSources[0].i32StrideInBytes = i32SrcStrideInBytes;
	psQueueTransfer->asSources[0].ui32ChunkStride = ui32SrcChunkStride;
	psQueueTransfer->asSources[0].eMemLayout = eSrcMemLayout;
	psQueueTransfer->asSources[0].psSyncInfo = psSrcReadParams->psSyncInfo;

	psQueueTransfer->ui32NumDest = 1;
	psQueueTransfer->asDests[0].sDevVAddr.uiAddr = ui32DstUiAddr;
	psQueueTransfer->asDests[0].ui32Width  = ui32DstRoundedWidth;
	psQueueTransfer->asDests[0].ui32Height = ui32DstRoundedHeight;
	psQueueTransfer->asDests[0].eFormat = eDstPixelFormat;
	psQueueTransfer->asDests[0].i32StrideInBytes = i32DstStrideInBytes;
	psQueueTransfer->asDests[0].ui32ChunkStride = ui32DstChunkStride;
	psQueueTransfer->asDests[0].eMemLayout = eDstMemLayout;
#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psDstTex->psEGLImageTarget)
	{
		psQueueTransfer->asDests[0].psSyncInfo = psDstTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
	}
	else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
	{
		psQueueTransfer->asDests[0].psSyncInfo = psDstTex->psMemInfo->psClientSyncInfo;
	}	
	
	psQueueTransfer->ui32NumSrcRects = 1;
	psQueueTransfer->asSrcRects[0].x0 = i32SrcRectX0; 
	psQueueTransfer->asSrcRects[0].y0 = i32SrcRectY0; 
	psQueueTransfer->asSrcRects[0].x1 = i32SrcRectX1;
	psQueueTransfer->asSrcRects[0].y1 = i32SrcRectY1;

	psQueueTransfer->ui32NumDestRects = 1;
	psQueueTransfer->asDestRects[0].x0 = i32DstRectX0;
	psQueueTransfer->asDestRects[0].y0 = i32DstRectY0;
	psQueueTransfer->asDestRects[0].x1 = i32DstRectX1;
	psQueueTransfer->asDestRects[0].y1 = i32DstRectY1;

	psQueueTransfer->ui32NumStatusValues = 0;
	psQueueTransfer->bPDumpContinuous = IMG_TRUE;

	psQueueTransfer->ui32Flags |= SGX_KICKTRANSFER_FLAGS_3DTQ_SYNC;

	return IMG_TRUE;

}



/***********************************************************************************
 Function Name      : HWTQTextureNormalBlit
 Inputs             : gc, psTex, psQueueTransfer
 Outputs            : -
 Returns            : Success/failure
 Description        : Upload a sub square/rectangle/nonpow2 texture
                      using the device with the TILED, LINEAR or TWIDDLED memlayout
************************************************************************************/

IMG_INTERNAL IMG_BOOL HWTQTextureNormalBlit(GLES1Context      *gc,
											GLESTexture       *psDstTex, 
											EGLDrawableParams *psSrcReadParams,
											SGX_QUEUETRANSFER *psQueueTransfer)
{
 
	PVRSRV_ERROR eResult;

	eResult = SGXQueueTransfer(&gc->psSysContext->s3D, gc->psSysContext->hTransferContext, psQueueTransfer);

	if(eResult != PVRSRV_OK)
	{
		PVRSRV_CLIENT_SYNC_INFO	  *psDstSyncInfo;
		PVRSRV_CLIENT_SYNC_INFO	  *psSrcSyncInfo;

		PVR_DPF((PVR_DBG_WARNING, "HWTQTextureNormalBlit: Failed to load texture image (error=%d). Falling back to SW", eResult));


		/* Setup psDstSyncInfo */
#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psDstTex->psEGLImageTarget)
		{
			psDstSyncInfo = psDstTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
		}
		else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
		{
			psDstSyncInfo = psDstTex->psMemInfo->psClientSyncInfo;
		}	

		/* check the texture has been uploaded before sw fallback */
		if(psDstSyncInfo)
		{		
#if defined(PDUMP)

			PVRSRVPDumpSyncPol( gc->ps3DDevData->psConnection,
								psDstSyncInfo,
								IMG_FALSE,
								psDstSyncInfo->psSyncData->ui32WriteOpsPending,
								0xFFFFFFFF);
		
#endif	/*defined (PDUMP)*/

			/* HWTQTextureNormalBlit: waiting for previous texture transfer */
			while (SGX2DQueryBlitsComplete(&gc->psSysContext->s3D, psDstSyncInfo, IMG_TRUE) != PVRSRV_OK)
			{
			}
		}


		/* Setup psSrcSyncInfo */
		psSrcSyncInfo = psSrcReadParams->psSyncInfo;

		/* check the texture has been uploaded before sw fallback */
		if(psSrcSyncInfo)
		{		
#if defined(PDUMP)

			PVRSRVPDumpSyncPol( gc->ps3DDevData->psConnection,
								psSrcSyncInfo,
								IMG_FALSE,
								psSrcSyncInfo->psSyncData->ui32WriteOpsPending,
								0xFFFFFFFF);
		
#endif	/*defined (PDUMP)*/

			/* HWTQTextureNormalBlit: waiting for previous texture transfer */
			while (SGX2DQueryBlitsComplete(&gc->psSysContext->s3D, psSrcSyncInfo, IMG_TRUE) != PVRSRV_OK)
			{
			}
		}

		return IMG_FALSE;
	}

	return IMG_TRUE;

}

#if !defined(__psp2__)
/***********************************************************************************
 Function Name      : PrepareHWTQTextureBufferBlit
 Inputs             : gc, 
                    : psDstTex, ui32DstOffsetInBytes,
                    : psSrcInfo, ui32SrcOffsetInBytes,
                    : ui32SizeInBytes
 Outputs            : psQueueTransfer
 Returns            : Success/failure
 Description        : Transfer a certain part of texture using the device.
************************************************************************************/

static IMG_BOOL PrepareHWTQTextureBufferBlit(GLES1Context           *gc,
											 GLESTexture            *psDstTex, 
											 IMG_UINT32              ui32DstOffsetInBytes,
											 PVRSRV_CLIENT_MEM_INFO *psSrcInfo,
											 IMG_UINT32              ui32SrcOffsetInBytes,
											 IMG_UINT32              ui32SizeInBytes,
											 SGX_QUEUETRANSFER       *psQueueTransfer) 
{

	/* Assert HWTQNormalBlit is enabled */
	GLES1_ASSERT(!gc->sAppHints.bDisableHWTQBufferBlit);


	/***** Firstly check the basic restriction on texture size *****/
	
	/* Restriction 1: 
	   if texture size is too small, then there's no gain on using HWTQ buffer blit */
	if (psSrcInfo->uAllocSize < 256)
	{
		PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureBufferBlit: texture size is too small to be copied by using HWTQ buffer blit"));
		return IMG_FALSE;
	}

	/* Restriction 2: for UNIFIED_STORE_64BITS & HWBRN23054 */
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS) & defined(FIX_HW_BRN_23054)
	if ( (psDstTex->psMipLevel[0]->ui32Width  < 8) ||
		 (psDstTex->psMipLevel[0]->ui32Height < 8) )
	{
		PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureBufferBlit: texture size is too small to be copied by using HWTQ buffer blit"));
		return IMG_FALSE;
	}
#endif	

	/* Restriction 3: for HWBRN23054 */
#if defined(FIX_HW_BRN_23054)
	if ( (psSrcInfo->uAllocSize % 4 != 0) ||
		 (psSrcInfo->uAllocSize <= EURASIA_RENDERSIZE_MAXX * 4) )
	  /*
		 (ui32TexAreaSize >= EURASIA_ISPREGION_SIZEX * EURASIA_ISPREGION_SIZEY)
	  */
	{
		PVR_DPF((PVR_DBG_MESSAGE, "PrepareHWTQTextureBufferBlit: texture size is too small to be copied by using HWTQ buffer blit"));
		return IMG_FALSE;
	}	
#endif

	/* Setup psQueueTransfer */
	PVR_UNREFERENCED_PARAMETER(gc);
	GLES1MemSet(psQueueTransfer, 0, sizeof(SGX_QUEUETRANSFER));

	/* Use buffer blit: which sets 
	   source with SGXTQ_MEMLAYOUT_STRIDE, destination with SGXTQ_MEMLAYOUT_OUT_LINEAR */
	psQueueTransfer->eType = SGXTQ_BUFFERBLT; 

	/* Setup the bytes size needed to be blitted */
	psQueueTransfer->Details.sBufBlt.ui32Bytes = ui32SizeInBytes;

	psQueueTransfer->ui32NumSources = 1;
	psQueueTransfer->asSources[0].sDevVAddr.uiAddr = psSrcInfo->sDevVAddr.uiAddr + ui32SrcOffsetInBytes;
	psQueueTransfer->asSources[0].psSyncInfo = psSrcInfo->psClientSyncInfo;

	psQueueTransfer->ui32NumDest = 1;
#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psDstTex->psEGLImageTarget)
	{
		psQueueTransfer->asDests[0].sDevVAddr.uiAddr = psDstTex->psEGLImageTarget->psMemInfo->sDevVAddr.uiAddr + ui32DstOffsetInBytes;	
		psQueueTransfer->asDests[0].psSyncInfo = psDstTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
	}
	else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
	{
		psQueueTransfer->asDests[0].sDevVAddr.uiAddr = psDstTex->psMemInfo->sDevVAddr.uiAddr + ui32DstOffsetInBytes;	
		psQueueTransfer->asDests[0].psSyncInfo = psDstTex->psMemInfo->psClientSyncInfo;
	}	

	psQueueTransfer->ui32NumStatusValues = 0;
	psQueueTransfer->ui32Flags = 0;
	psQueueTransfer->bPDumpContinuous = IMG_TRUE;

	return IMG_TRUE;

}



/***********************************************************************************
 Function Name      : HWTQTextureBufferBlit
 Inputs             : gc, psDstTex, psSrcInfo  
 Outputs            : psQueueTransfer
 Returns            : Success/failure
 Description        : Transfer a certain part of texture using the device.
************************************************************************************/

static IMG_BOOL HWTQTextureBufferBlit(GLES1Context           *gc,
									  GLESTexture            *psDstTex, 
									  PVRSRV_CLIENT_MEM_INFO *psSrcInfo,
									  SGX_QUEUETRANSFER      *psQueueTransfer) 
{
	PVRSRV_ERROR eResult;

	eResult = SGXQueueTransfer(&gc->psSysContext->s3D, gc->psSysContext->hTransferContext, psQueueTransfer);

	if(eResult != PVRSRV_OK)
	{
		PVRSRV_CLIENT_SYNC_INFO	  *psDstSyncInfo;
		PVRSRV_CLIENT_SYNC_INFO	  *psSrcSyncInfo;

		PVR_DPF((PVR_DBG_WARNING, "HWTQTextureBufferBlit: Failed to load texture image (error=%d). Falling back to SW", eResult));


#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psDstTex->psEGLImageTarget)
		{
			psDstSyncInfo = psDstTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
		}
		else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
		{
			psDstSyncInfo = psDstTex->psMemInfo->psClientSyncInfo;
		}	

		/* check the texture has been uploaded before sw fallback */
		if(psDstSyncInfo)
		{		
#if defined(PDUMP)

			PVRSRVPDumpSyncPol( gc->ps3DDevData->psConnection,
								psDstSyncInfo,
								IMG_FALSE,
								psDstSyncInfo->psSyncData->ui32WriteOpsPending,
								0xFFFFFFFF);
#endif	/*defined (PDUMP)*/
			
			/* HWTQTextureBufferBlit: waiting for previous texture transfer */
			while (SGX2DQueryBlitsComplete(&gc->psSysContext->s3D, psDstSyncInfo, IMG_TRUE) != PVRSRV_OK)
			{
			}
		}


		psSrcSyncInfo = psSrcInfo->psClientSyncInfo;

		/* check the texture has been uploaded before sw fallback */
		if(psSrcSyncInfo)
		{		
#if defined(PDUMP)

			PVRSRVPDumpSyncPol( gc->ps3DDevData->psConnection,
								psSrcSyncInfo,
								IMG_FALSE,
								psSrcSyncInfo->psSyncData->ui32WriteOpsPending,
								0xFFFFFFFF);
		
#endif	/*defined (PDUMP)*/

			/* HWTQTextureBufferBlit: waiting for previous texture transfer */
			while (SGX2DQueryBlitsComplete(&gc->psSysContext->s3D, psSrcSyncInfo, IMG_TRUE) != PVRSRV_OK)
			{
			}
		}

		return IMG_FALSE;

	}

	return IMG_TRUE;

}
#endif


/***********************************************************************************
 Function Name      : GetMipMapOffset
 Inputs             : ui32MapLevel, ui32TopUSize, ui32TopVSize
 Outputs            : -
 Returns            : Mipmap offset in pixels
 Description        : Calculates the pixel offset to a given map in a mipmap chain.
************************************************************************************/
IMG_INTERNAL IMG_UINT32 GetMipMapOffset(IMG_UINT32 ui32MapLevel, IMG_UINT32 ui32TopUSize, IMG_UINT32 ui32TopVSize)
{
    IMG_UINT32 i, ui32USize, ui32VSize, ui32MapOffset = 0;

	ui32USize = ui32TopUSize;
	ui32VSize = ui32TopVSize;
	
	/* Calculate the space taken up by all the map levels previous to this one */
	for (i = 0; i < ui32MapLevel; i++)
	{
		ui32MapOffset += (ui32USize * ui32VSize);
		
		ui32USize >>= 1;
		
		if(ui32USize==0)
		{
			ui32USize=1;
		}
		
		ui32VSize >>= 1;
		
		if(ui32VSize==0)
		{
			ui32VSize=1;
		}
	}

	return ui32MapOffset;
}


/***********************************************************************************
 Function Name      : GetCompressedMipMapOffset
 Inputs             : ui32MapLevel, ui32TopUSize, ui32TopVSize
 Outputs            : -
 Returns            : Mipmap offset in pixels
 Description        : Calculates the pixel offset to a given map in a mipmap chain.
************************************************************************************/
IMG_INTERNAL IMG_UINT32 GetCompressedMipMapOffset(IMG_UINT32 ui32MapLevel, IMG_UINT32 ui32TopUSize, 
									 IMG_UINT32 ui32TopVSize, IMG_BOOL bIs2Bpp)
{
    IMG_UINT32 i, ui32USize, ui32VSize, ui32MapOffset = 0;

	ui32USize = ui32TopUSize;
	ui32VSize = ui32TopVSize;
	
	if(bIs2Bpp)
	{
		/* Calculate the space taken up by all the map levels previous to this one */
		for (i = 0; i < ui32MapLevel; i++)
		{
			ui32MapOffset += (ui32USize * ui32VSize);
			
			ui32USize >>= 1;
			
			if(ui32USize==4)
			{
				ui32USize=8;
			}
			
			ui32VSize >>= 1;
			
			if(ui32VSize==2)
			{
				ui32VSize=4;
			}
		}

		/* PVRTC2 stores 8x4 texels in a block,
		 * so divide by 32 for number of compressed blocks
		 */
		ui32MapOffset >>= 5;
	}
	else
	{
		/* Calculate the space taken up by all the map levels previous to this one */
		for (i = 0; i < ui32MapLevel; i++)
		{
			ui32MapOffset += (ui32USize * ui32VSize);
			
			ui32USize >>= 1;
			
			if(ui32USize==2)
			{
				ui32USize=4;
			}
			
			ui32VSize >>= 1;
			
			if(ui32VSize==2)
			{
				ui32VSize=4;
			}
		}

		/* PVRTC4 stores 4x4 texels in a block,
		 * so divide by 16 for number of compressed blocks
		 */
		ui32MapOffset >>= 4;
	}

	return ui32MapOffset;
}

/***********************************************************************************
 Function Name      : TranslateLevel
 Inputs             : gc, psTex, ui32Face, ui32Lod
 Outputs            : psTex
 Returns            : -
 Description        : This is called to translate a given level of the texture object 
					  into the final texture.  
************************************************************************************/
IMG_INTERNAL IMG_VOID TranslateLevel(GLES1Context *gc, GLESTexture *psTex, IMG_UINT32 ui32Face, IMG_UINT32 ui32Lod)
{
    GLESMipMapLevel          *psMipLevel;
	IMG_UINT32                ui32OffsetInBytes, ui32TopUsize, ui32TopVsize, ui32BytesPerTexel;
    const GLESTextureFormat *psTexFmt = psTex->psFormat;
    GLESTextureParamState    *psParams = &psTex->sState;
	IMG_VOID *pvDest;
	SGX_QUEUETRANSFER sQueueTransfer;
	IMG_BOOL bHWUploaded = IMG_FALSE;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
	ui32TopUsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
	ui32TopVsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
	ui32TopUsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
	ui32TopVsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

	ui32BytesPerTexel = psTexFmt->ui32TotalBytesPerTexel;

	psMipLevel = &psTex->psMipLevel[ui32Lod + (ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS)];
	

	/********** Deal with compressed texture **********/
	if(psTex->ui32HWFlags & GLES1_COMPRESSED)
	{
		IMG_BOOL bIs2Bpp = IMG_FALSE;

		if( (psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTC2) || 
			(psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCII2))
		{
			bIs2Bpp = IMG_TRUE;
		}

		ui32OffsetInBytes =  ui32BytesPerTexel * 
			GetCompressedMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize, bIs2Bpp);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		{
			IMG_UINT32 ui32FaceOffset = ui32BytesPerTexel * 
				GetCompressedMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize, bIs2Bpp);
			
			if(psTex->ui32HWFlags & GLES1_MIPMAP)
			{
				if(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}
			ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
	

		GLES1_ASSERT(psTex->psMemInfo);

#if defined(DEBUG)
		if(psMipLevel->ui32ImageSize + ui32OffsetInBytes > psTex->psMemInfo->uAllocSize)
		{
			PVR_DPF((PVR_DBG_ERROR,"TranslateLevel: Memory calculation error somewhere"));

			return;
		}
#endif
		pvDest = (IMG_UINT8 *)psTex->psMemInfo->pvLinAddr + ui32OffsetInBytes;


#if defined(GLES1_EXTENSION_ETC1)
		if(psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCIII)
		{
			DeTwiddleAddressETC1(pvDest, psMipLevel->pui8Buffer, MAX(psMipLevel->ui32Width/4, 1), 
								 MAX(psMipLevel->ui32Height/4, 1), MAX(psMipLevel->ui32Width/4, 1));
		}
		else
#endif /* defined(GLES1_EXTENSION_ETC1) */
		{
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
			if (bIs2Bpp)
			{
				DeTwiddleAddressPVRTC2(pvDest, psMipLevel->pui8Buffer, psMipLevel->ui32Width, psMipLevel->ui32Height, psMipLevel->ui32Width);
			}
			else
			{
				DeTwiddleAddressPVRTC4(pvDest, psMipLevel->pui8Buffer, psMipLevel->ui32Width, psMipLevel->ui32Height, psMipLevel->ui32Width);
			}
#else
			GLES1MemCopy(pvDest, (IMG_VOID *)psMipLevel->pui8Buffer, psMipLevel->ui32ImageSize);
#endif
		}
	}
	else /********** Deal with non-compressed texture **********/
	{
		ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
		
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
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}

			ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
		}	
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psTex->psEGLImageTarget)
		{
#if defined(DEBUG)
			if(psMipLevel->ui32Width * psMipLevel->ui32Height * ui32BytesPerTexel + ui32OffsetInBytes > psTex->psEGLImageTarget->psMemInfo->uAllocSize)
			{
				PVR_DPF((PVR_DBG_ERROR,"TranslateLevel: Memory calculation error somewhere"));
				
				return;
			}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

			if(psTex->psEGLImageTarget->bTwiddled)
			{
				pvDest = (IMG_UINT8 *)psTex->psEGLImageTarget->pvLinSurfaceAddress;

				psTex->pfnTextureTwiddle(pvDest, psMipLevel->pui8Buffer, psMipLevel->ui32Width, 
										 psMipLevel->ui32Height, psMipLevel->ui32Width);
			}
			else
			{
				IMG_UINT32 ui32Stride, ui32Width, i;
				IMG_VOID *pvSrc;

				pvSrc = (IMG_VOID *)psMipLevel->pui8Buffer;
				pvDest = (IMG_UINT8 *)psTex->psEGLImageTarget->pvLinSurfaceAddress;

				ui32Width = ui32BytesPerTexel * psMipLevel->ui32Width;
				ui32Stride = psTex->psEGLImageTarget->ui32Stride;

				for(i=0; i<psMipLevel->ui32Height; i++)
				{
					GLES1MemCopy(pvDest, pvSrc, ui32Stride);

					pvSrc = (IMG_VOID *)((IMG_UINT8 *)pvSrc + ui32Width);
					pvDest = (IMG_VOID *)((IMG_UINT8 *)pvDest + ui32Stride);
				}
			}
		}
		else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
		{
#if defined(DEBUG)
			if(psMipLevel->ui32Width * psMipLevel->ui32Height * ui32BytesPerTexel + ui32OffsetInBytes > psTex->psMemInfo->uAllocSize)
			{
				PVR_DPF((PVR_DBG_ERROR,"TranslateLevel: Memory calculation error somewhere"));
				
				return;
			}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

			if ( (!gc->sAppHints.bDisableHWTQTextureUpload) &&
				 (PrepareHWTQTextureUpload(gc, psTex, ui32OffsetInBytes, psMipLevel,
										   IMG_NULL, IMG_NULL, 0, IMG_NULL, &sQueueTransfer)) )
			{
			    bHWUploaded = HWTQTextureUpload(gc, psTex, &sQueueTransfer);
			}

			if (!bHWUploaded)
			{
				pvDest = (IMG_UINT8 *)psTex->psMemInfo->pvLinAddr + ui32OffsetInBytes;

				if (psMipLevel->ui32Width >= 128 && psMipLevel->ui32Height >= 128)
				{
					SWTextureUpload(gc, psTex, pvDest, psMipLevel->pui8Buffer, psMipLevel->ui32Width,
						psMipLevel->ui32Height, psMipLevel->ui32Width);
				}
				else
				{
					psTex->pfnTextureTwiddle(pvDest, psMipLevel->pui8Buffer, psMipLevel->ui32Width,
						psMipLevel->ui32Height, psMipLevel->ui32Width);
				}
			}
		}
	}

#if defined(PDUMP)
	if(!bHWUploaded)
	{
		psTex->ui32LevelsToDump |= (1UL << ui32Lod);
	}
#endif
}


/***********************************************************************************
 Function Name      : ReadBackTextureData
 Inputs             : gc, ui32Face, ui32Level
 Outputs            : pvBuffer
 Returns            : -
 Description        : Reads back texture level data from a HW texture. 
************************************************************************************/
IMG_INTERNAL IMG_VOID ReadBackTextureData(GLES1Context *gc, GLESTexture *psTex, IMG_UINT32 ui32Face, 
											IMG_UINT32 ui32Lod, IMG_VOID *pvBuffer)
{
	IMG_UINT32                ui32Width, ui32Height, ui32TopUsize, ui32TopVsize, 
	                          ui32BytesPerTexel, ui32OffsetInBytes, ui32SizeInBytes;
    GLESMipMapLevel          *psMipLevel;
	IMG_VOID                  *pvSrcData;
	const GLESTextureFormat *psTexFormat;
    GLESTextureParamState    *psParams = &psTex->sState;
	PVRSRV_CLIENT_SYNC_INFO	*psDstSyncInfo;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
	ui32TopUsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
	ui32TopVsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
	ui32TopUsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
	ui32TopVsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

	psMipLevel = &psTex->psMipLevel[ui32Lod + (ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS)];
	psTexFormat = psMipLevel->psTexFormat;

	ui32BytesPerTexel = psTexFormat->ui32TotalBytesPerTexel;
	

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageTarget)
	{
	    psDstSyncInfo = psTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
	}
	else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
	{
	    psDstSyncInfo = psTex->psMemInfo->psClientSyncInfo;
	}	

	/* check the texture has been uploaded before reading it back */
	if(psDstSyncInfo)
	{		
#if defined(PDUMP)
		PVRSRVPDumpSyncPol(gc->ps3DDevData->psConnection, 
							psDstSyncInfo,
							IMG_FALSE,
							psDstSyncInfo->psSyncData->ui32WriteOpsPending,
							0xFFFFFFFF);
 
#endif	/*defined (PDUMP)*/

		/* ReadBackTextureData: waiting for texture upload */
		while (SGX2DQueryBlitsComplete(&gc->psSysContext->s3D, psDstSyncInfo, IMG_TRUE) != PVRSRV_OK)
		{
		}
	}
	
	if(psTex->ui32HWFlags & GLES1_COMPRESSED)
	{
		IMG_BOOL bIs2Bpp = IMG_FALSE;

		if( (psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTC2) || 
			(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCII2))
		{
			bIs2Bpp = IMG_TRUE;
		}

		ui32OffsetInBytes = GetCompressedMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize, bIs2Bpp) << 3;
		
		ui32SizeInBytes =  
			(GetCompressedMipMapOffset(ui32Lod + 1, ui32TopUsize, ui32TopVsize, bIs2Bpp) << 3) - ui32OffsetInBytes;
		
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		{
			IMG_UINT32 ui32FaceOffset = 
				GetCompressedMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize, bIs2Bpp) << 3;
			
			if(psTex->ui32HWFlags & GLES1_MIPMAP)
			{
				if(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}

			ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
	
		GLES1_ASSERT(psTex->psMemInfo);

		pvSrcData = (IMG_UINT8 *)psTex->psMemInfo->pvLinAddr + ui32OffsetInBytes;

#if defined(GLES1_EXTENSION_ETC1)
		if(psTex->psFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCIII)
		{
			ui32Width = psMipLevel->ui32Width;
			ui32Height = psMipLevel->ui32Height;

			ReadBackTwiddleETC1(pvBuffer, pvSrcData, psMipLevel->ui32WidthLog2, psMipLevel->ui32HeightLog2, 
								MAX(ui32Width/4, 1), MAX(ui32Height/4, 1), MAX(ui32Width/4, 1));
		}
		else
#endif /* defined(GLES1_EXTENSION_ETC1) */
		{
			GLES1MemCopy(pvBuffer, pvSrcData, ui32SizeInBytes);
		}
	}
	else
	{
		ui32Width = psMipLevel->ui32Width;
		ui32Height = psMipLevel->ui32Height;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psTex->psEGLImageTarget)
		{
			pvSrcData = psTex->psEGLImageTarget->pvLinSurfaceAddress;

			if(psTex->psEGLImageTarget->bTwiddled)
			{
				psTex->pfnReadBackData(pvBuffer, pvSrcData, psMipLevel->ui32WidthLog2, psMipLevel->ui32HeightLog2, 
									   ui32Width, ui32Height, ui32Width);
			}
			else
			{
				IMG_UINT32 ui32Stride, i;

				ui32Width = ui32BytesPerTexel * psMipLevel->ui32Width;
				ui32Stride = psTex->psEGLImageTarget->ui32Stride;

				for(i=0; i<psMipLevel->ui32Height; i++)
				{
					GLES1MemCopy(pvBuffer, pvSrcData, ui32Stride);

					pvBuffer = (IMG_VOID *)((IMG_UINT8 *)pvBuffer + ui32Width);
					pvSrcData = (IMG_VOID *)((IMG_UINT8 *)pvSrcData + ui32Stride);
				}
			}
		}
		else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
		{
			ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
			if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
			{
				IMG_UINT32 ui32FaceOffset = 
					ui32BytesPerTexel *	GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);
				
				if(psTex->ui32HWFlags & GLES1_MIPMAP)
				{
					if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
						(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
					{
						ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
					}
				}

				ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
			}	
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
		
			pvSrcData = (IMG_UINT8 *)psTex->psMemInfo->pvLinAddr + ui32OffsetInBytes;

			psTex->pfnReadBackData(pvBuffer, pvSrcData, psMipLevel->ui32WidthLog2, psMipLevel->ui32HeightLog2, 
								   ui32Width, ui32Height, ui32Width);
		}
	}
}


/***********************************************************************************
 Function Name      : SetupTwiddleFns
 Inputs             : psTex
 Outputs            : psTex
 Returns            : -
 Description        : Sets up the appropriate twiddle,subtwiddle and readback 
					  functions based on a texture's bit depth.
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupTwiddleFns(GLESTexture *psTex)
{
	switch(psTex->psFormat->ui32TotalBytesPerTexel)
	{
		case 1:
		{
			psTex->pfnTextureTwiddle = DeTwiddleAddress8bpp;
			psTex->pfnReadBackData   = ReadBackTwiddle8bpp;

			break;
		}
		case 2:
		{
			psTex->pfnTextureTwiddle = DeTwiddleAddress16bpp;
			psTex->pfnReadBackData   = ReadBackTwiddle16bpp;
			break;
		}
		case 4:
		{
			psTex->pfnTextureTwiddle = DeTwiddleAddress32bpp;
			psTex->pfnReadBackData   = ReadBackTwiddle32bpp;
			break;
		}
		default:
		{
			/* May get here for PVRTC - shouldn't use these fn pointers anyway */
			break;
		}
	}
}


/***********************************************************************************
 Function Name      : CopyTextureData
 Inputs             : gc, 
                      psDstTex, ui32DstOffsetInBytes,
                      psSrcInfo, ui32SrcOffsetInBytes,
                      ui32SizeInBytes
 Outputs            : psDstTex
 Returns            : -
 Description        : Copies texture data from one texture to another. (Could be
					  accelerated by HW).
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureData(GLES1Context           *gc, 
									  GLESTexture            *psDstTex, 
									  IMG_UINT32              ui32DstOffsetInBytes,
									  PVRSRV_CLIENT_MEM_INFO *psSrcInfo,
									  IMG_UINT32              ui32SrcOffsetInBytes,
									  IMG_UINT32              ui32SizeInBytes)
{
	/***************** Two paths ********************/

	{
	    GLES1_ASSERT(psDstTex->psMemInfo);

		/* PATH 1: using DMAC */
		if (psSrcInfo->uAllocSize > 6400)
		{
			sceDmacMemcpy((IMG_PVOID)((IMG_UINTPTR_T)(psDstTex->psMemInfo->pvLinAddr) + ui32DstOffsetInBytes),
				(IMG_PVOID)((IMG_UINTPTR_T)psSrcInfo->pvLinAddr + ui32SrcOffsetInBytes),
				psSrcInfo->uAllocSize);
		}
		/* PATH 2: using SW */
		else
		{
			GLES1MemCopy((IMG_PVOID)((IMG_UINTPTR_T)(psDstTex->psMemInfo->pvLinAddr) + ui32DstOffsetInBytes),
				(IMG_PVOID)((IMG_UINTPTR_T)psSrcInfo->pvLinAddr + ui32SrcOffsetInBytes),
				psSrcInfo->uAllocSize);
		}
	}

}



