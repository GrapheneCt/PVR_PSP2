/******************************************************************************
 * Name         : pixelop.c
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
 * $Log: pixelop.c $
 *****************************************************************************/

#include "context.h"
#include "spanpack.h"
#include "twiddle.h"

/***********************************************************************************
 Function Name      : GetStridedSurfaceData
 Inputs             : gc, psReadParams, psSpanInfo
 Outputs            : -
 Returns            : Pointer to strided data, or null
 Description        : Returns a pointer that can be used to read the surface as
					  strided data.  Detwiddling/detiling is performed if necessary
					  with the calling function responsible for freeing any 
					  temporary surface data (if the returned pointer doesn't equal
					  psReadParams->pvLinSurfaceAddress)
************************************************************************************/
IMG_INTERNAL IMG_VOID *GetStridedSurfaceData(GLES2Context *gc, EGLDrawableParams *psReadParams, GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_MEMLAYOUT eMemFormat;

	eMemFormat = GetColorAttachmentMemFormat(gc, gc->sFrameBuffer.psActiveFrameBuffer);

	switch(eMemFormat)
	{
		case IMG_MEMLAYOUT_STRIDED:
		{
			return psReadParams->pvLinSurfaceAddress;
		}
		case IMG_MEMLAYOUT_TILED:
		{
			GLES2FrameBufferAttachable *psColorAttachment;
			GLES2MipMapLevel *psSrcMipLevel;
			IMG_VOID *pvDeTiledSurface;
			GLES2Texture *psSrcTex;
			IMG_VOID *pvSrcData;

			psColorAttachment = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];

			GLES_ASSERT(GL_TEXTURE == psColorAttachment->eAttachmentType);

			psSrcMipLevel = (GLES2MipMapLevel*)psColorAttachment;

			psSrcTex = psSrcMipLevel->psTex;

			GLES_ASSERT(psSrcTex);

			pvSrcData = (IMG_UINT8 *)psReadParams->pvLinSurfaceAddress;
			
			pvDeTiledSurface = GLES2Malloc(gc, psSrcTex->psFormat->ui32TotalBytesPerTexel * psReadParams->ui32Width * psReadParams->ui32Height);

			if(!pvDeTiledSurface)
			{
			    SetError(gc, GL_OUT_OF_MEMORY);

				return IMG_NULL;
			}

			ReadBackTiledData(pvDeTiledSurface, pvSrcData, 
							  psReadParams->ui32Width, psReadParams->ui32Height,
							  psSrcTex);
		   
			/* Set source increment to match temporary strided-data buffer */
			psSpanInfo->i32SrcRowIncrement = (IMG_INT32)(psReadParams->ui32Width * psSrcTex->psFormat->ui32TotalBytesPerTexel);

			return pvDeTiledSurface;
		}
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
		case IMG_MEMLAYOUT_HYBRIDTWIDDLED:
#else
		case IMG_MEMLAYOUT_TWIDDLED:
#endif
		{
			GLES2FrameBufferAttachable *psColorAttachment;
			IMG_VOID *pvDeTwiddledSurface;
			IMG_VOID *pvSrcData;

			psColorAttachment = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];

			pvSrcData = (IMG_UINT8 *)psReadParams->pvLinSurfaceAddress;

			if(psColorAttachment->eAttachmentType==GL_TEXTURE)
			{
				IMG_UINT32 ui32WidthLog2, ui32HeightLog2;
				GLES2TextureParamState *psParams;
				GLES2MipMapLevel *psSrcMipLevel;
				GLES2Texture *psSrcTex;

				psSrcMipLevel = (GLES2MipMapLevel*)psColorAttachment;
				psSrcTex = psSrcMipLevel->psTex;

				GLES_ASSERT(psSrcTex);

				psParams = &psSrcTex->sState;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
				ui32WidthLog2 = (psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT;
				ui32HeightLog2 = (psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT;
#else
				ui32WidthLog2 = (psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT;
				ui32HeightLog2 = (psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT;

				ui32WidthLog2 = FloorLog2(ui32WidthLog2 + 1);
				ui32HeightLog2 = FloorLog2(ui32HeightLog2 + 1);
#endif

				/* We need to read back the surface before reading it linearly */
				pvDeTwiddledSurface = GLES2Malloc(gc, psSrcTex->psFormat->ui32TotalBytesPerTexel*psSrcMipLevel->ui32Width*psSrcMipLevel->ui32Height);

				if(!pvDeTwiddledSurface)
				{
				    SetError(gc, GL_OUT_OF_MEMORY);

					return IMG_NULL;
				}

				GLES_ASSERT((psSrcTex->psFormat->ui32TotalBytesPerTexel*psSrcMipLevel->ui32Width) == (IMG_UINT32)psSpanInfo->i32SrcRowIncrement);

				psSrcTex->pfnReadBackData(pvDeTwiddledSurface, pvSrcData, 
									   ui32WidthLog2, ui32HeightLog2, 
									   psSrcMipLevel->ui32Width, psSrcMipLevel->ui32Height,
									   psSrcMipLevel->ui32Width);
			}
			else
			{
				GLES2RenderBuffer *psRenderBuffer;
				IMG_UINT32 ui32BPP, ui32Width, ui32Height;
				IMG_VOID (*pfnReadBackData)(IMG_VOID *pvDest, const IMG_VOID *pvSrc,
											IMG_UINT32 ui32Log2Width, IMG_UINT32 ui32Log2Height,
											IMG_UINT32 ui32Width, IMG_UINT32 ui32Height, 
											IMG_UINT32 ui32DstStride);

				psRenderBuffer = (GLES2RenderBuffer *)psColorAttachment;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
				if(psRenderBuffer->psEGLImageTarget)
				{
					EGLImage *psEGLImage;

					psEGLImage = psRenderBuffer->psEGLImageTarget;

					switch(psEGLImage->ePixelFormat)
					{
						case PVRSRV_PIXEL_FORMAT_RGB565:
						case PVRSRV_PIXEL_FORMAT_ARGB4444:
						case PVRSRV_PIXEL_FORMAT_ARGB1555:
						{
							ui32BPP = 2;

							break;
						}
						case PVRSRV_PIXEL_FORMAT_ARGB8888:
						case PVRSRV_PIXEL_FORMAT_ABGR8888:
						case PVRSRV_PIXEL_FORMAT_XRGB8888:
						case PVRSRV_PIXEL_FORMAT_XBGR8888:
						{
							ui32BPP = 4;

							break;
						}
						default:
						{
							PVR_DPF((PVR_DBG_ERROR,"GetStridedSurfaceData: Unsupported pixel format"));

							return IMG_NULL;
						}
					}

					ui32Width = psEGLImage->ui32Width;
					ui32Height = psEGLImage->ui32Height;
				}
				else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
				{
					ui32BPP = (psRenderBuffer->ui8RedSize + 
							   psRenderBuffer->ui8GreenSize + 
							   psRenderBuffer->ui8BlueSize + 
							   psRenderBuffer->ui8AlphaSize)/8;

					ui32Width = psRenderBuffer->ui32Width;
					ui32Height = psRenderBuffer->ui32Height;
				}

				/* We need to read back the surface before reading it linearly */
				pvDeTwiddledSurface = GLES2Malloc(gc, ui32Width*ui32Height*ui32BPP);

				if(!pvDeTwiddledSurface)
				{
				    SetError(gc, GL_OUT_OF_MEMORY);

					return IMG_NULL;
				}

				switch(ui32BPP)
				{
					case 1:
					{
						pfnReadBackData = ReadBackTwiddle8bpp;
						break;
					}
					case 2:
					{
						pfnReadBackData = ReadBackTwiddle16bpp;
						break;
					}
					case 4:
					default:
					{
						pfnReadBackData = ReadBackTwiddle32bpp;
						break;
					}
				}

				pfnReadBackData(pvDeTwiddledSurface, pvSrcData, 
							    FloorLog2(ui32Width),FloorLog2(ui32Height),
							    ui32Width, ui32Height,
							    ui32Width);
			}

			return pvDeTwiddledSurface;
		}
		default:
		{
			GLES_ASSERT(eMemFormat != eMemFormat);
		}
	}

	return IMG_NULL;
}	


/***********************************************************************************
 Function Name      : glPixelStorei
 Inputs             : pname, param
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Stores pack and unpack alignment internally.
					  Used for converting texture data to internal format and for 
					  converting readpixel data to external format.
************************************************************************************/
GL_APICALL void GL_APIENTRY glPixelStorei(GLenum pname, GLint param)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPixelStorei"));

	GLES2_TIME_START(GLES2_TIMES_glPixelStorei);

	switch(pname)
	{
		case GL_PACK_ALIGNMENT:
		{
			switch (param) 
			{
				case 1:
				case 2:
				case 4:
				case 8:
				{
					gc->sState.sClientPixel.ui32PackAlignment = (IMG_UINT32)param;

					break;
				}
				default:
				{
bad_value:
					SetError(gc, GL_INVALID_VALUE);

					GLES2_TIME_STOP(GLES2_TIMES_glPixelStorei);

					return;
				}
			}

			break;
		}
		case GL_UNPACK_ALIGNMENT:
		{
			switch (param) 
			{
				case 1:
				case 2:
				case 4:
				case 8:
				{
					gc->sState.sClientPixel.ui32UnpackAlignment = (IMG_UINT32)param;

					break;
				}
				default:
				{
					goto bad_value;
				}
			}

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glPixelStorei);

			return;
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glPixelStorei);
}


/***********************************************************************************
 Function Name      : CheckReadPixelArgs
 Inputs             : gc, i32Width, i32Height, format, type
 Outputs            : pfnSpanPack
 Returns            : Success
 Description        : Checks readpixels arguments. Returns relevant readspan 
					  function pointer
************************************************************************************/
static IMG_BOOL CheckReadPixelArgs(GLES2Context *gc, IMG_INT32 i32Width, IMG_INT32 i32Height, 
								   GLenum format, GLenum type, PFNSpanPack *ppfnSpanPack)
{
	PVRSRV_PIXEL_FORMAT ePixelFormat;
	
	if ((i32Width < 0) || (i32Height < 0)) 
	{
		SetError(gc, GL_INVALID_VALUE);

		return IMG_FALSE;
	}

	ePixelFormat = gc->psReadParams->ePixelFormat;

	switch (format) 
	{
		case GL_RGB:
		{
			switch(type)
			{
				case GL_UNSIGNED_SHORT_5_6_5:
				{
					if(ePixelFormat != PVRSRV_PIXEL_FORMAT_RGB565)
					{
bad_op:
						SetError(gc, GL_INVALID_OPERATION);

						return IMG_FALSE;
					}

					*ppfnSpanPack = SpanPack16;

					break;
				}
				case GL_UNSIGNED_BYTE:
				case GL_UNSIGNED_SHORT_4_4_4_4:
				case GL_UNSIGNED_SHORT_5_5_5_1:
#if defined(GLES2_EXTENSION_READ_FORMAT)
				case GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG:
#endif /* GLES2_EXTENSION_READ_FORMAT */
				{
					goto bad_op;
				}
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
		case GL_RGBA:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
					switch(ePixelFormat)
					{
						case PVRSRV_PIXEL_FORMAT_ARGB8888:
						{
							*ppfnSpanPack = SpanPackARGB8888toABGR8888;

							break;
						}
						case PVRSRV_PIXEL_FORMAT_ABGR8888:
						{
							*ppfnSpanPack = SpanPack32;

							break;
						}
						case PVRSRV_PIXEL_FORMAT_XRGB8888:
						{
							*ppfnSpanPack = SpanPackXRGB8888to1BGR8888;

							break;
						}
						case PVRSRV_PIXEL_FORMAT_XBGR8888:
						{
							*ppfnSpanPack = SpanPackXBGR8888to1BGR8888;

							break;
						}
						case PVRSRV_PIXEL_FORMAT_ARGB4444:
						{
							*ppfnSpanPack = SpanPackARGB4444toABGR8888;

							break;
						}
						case PVRSRV_PIXEL_FORMAT_ARGB1555:
						{
							*ppfnSpanPack = SpanPackARGB1555toABGR8888;

							break;
						}
						case PVRSRV_PIXEL_FORMAT_RGB565:
						default:
						{
							*ppfnSpanPack = SpanPackRGB565toXBGR8888;

							break;
						}
					}

					break;
				}
				case GL_UNSIGNED_SHORT_4_4_4_4:
				{
					if(ePixelFormat != PVRSRV_PIXEL_FORMAT_ARGB4444)
					{
						goto bad_op;
					}

					*ppfnSpanPack = SpanPackARGB4444toRGBA4444;

					break;
				}
				case GL_UNSIGNED_SHORT_5_5_5_1:
				{
					if(ePixelFormat != PVRSRV_PIXEL_FORMAT_ARGB1555)
					{
						goto bad_op;
					}

					*ppfnSpanPack = SpanPackARGB1555toRGBA5551;

					break;
				}
				case GL_UNSIGNED_SHORT_5_6_5:
#if defined(GLES2_EXTENSION_READ_FORMAT)
				case GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG:
#endif /* GLES2_EXTENSION_READ_FORMAT */
				{
					goto bad_op;
				}
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
#if defined(GLES2_EXTENSION_READ_FORMAT)
		case GL_BGRA_IMG:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
					if(ePixelFormat != PVRSRV_PIXEL_FORMAT_ARGB8888)
					{
						goto bad_op;
					}

					*ppfnSpanPack = SpanPack32;

					break;
				}
				case GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG:
				{
					if(ePixelFormat != PVRSRV_PIXEL_FORMAT_ARGB4444)
					{
						goto bad_op;
					}

					*ppfnSpanPack = SpanPack16;

					break;
				}
				case GL_UNSIGNED_SHORT_4_4_4_4:
				case GL_UNSIGNED_SHORT_5_5_5_1:
				case GL_UNSIGNED_SHORT_5_6_5:
				{
					goto bad_op;
				}
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
#endif /* GLES2_EXTENSION_READ_FORMAT */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}



/***********************************************************************************
 Function Name      : ClipReadPixels
 Inputs             : psSpanInfo, psReadParams
 Outputs            : spanInfo
 Returns            : Success
 Description        : This routine clips ReadPixels calls so that only fragments 
					  which are owned by this window will be read and copied into 
					  the user's data. Parts of the ReadPixels rectangle lying 
					  outside of the window will be ignored.
************************************************************************************/
IMG_INTERNAL IMG_BOOL ClipReadPixels(GLES2PixelSpanInfo *psSpanInfo, EGLDrawableParams *psReadParams)
{
	IMG_UINT32 ui32SkipPixels, ui32SkipRows, ui32Width, ui32Height;
	IMG_INT32 i32ClipLeft, i32ClipRight, i32ClipTop, i32ClipBottom;
	IMG_INT32 i32X1, i32Y1, i32X2, i32Y2, i32Temp;

	i32ClipLeft = 0;
	i32ClipRight = (IMG_INT32)psReadParams->ui32Width;
	i32ClipTop = 0;
	i32ClipBottom = (IMG_INT32)psReadParams->ui32Height;	
	
	ui32Width	= psSpanInfo->ui32Width;
	ui32Height	= psSpanInfo->ui32Height;
	i32X1		= psSpanInfo->i32ReadX;
	i32Y1		= psSpanInfo->i32ReadY;
	i32X2		= i32X1 + (IMG_INT32)psSpanInfo->ui32Width;
	i32Y2		= i32Y1 + (IMG_INT32)psSpanInfo->ui32Height;

	if (i32X1 < i32ClipLeft) 
	{
		ui32SkipPixels = (IMG_UINT32)(i32ClipLeft - i32X1);

		if (ui32SkipPixels >= ui32Width) 
		{
			return IMG_FALSE;
		}

		ui32Width -= ui32SkipPixels;

		i32X1 = i32ClipLeft;

		psSpanInfo->ui32DstSkipPixels += ui32SkipPixels;

		psSpanInfo->i32ReadX = i32X1;
	}

	if (i32X2 > i32ClipRight) 
	{
		i32Temp = i32X2 - i32ClipRight;

		if (i32Temp >= (IMG_INT32)ui32Width) 
		{
			return IMG_FALSE;
		}

		ui32Width -= (IMG_UINT32)i32Temp;
	}

	if (i32Y1 < i32ClipTop) 
	{
		ui32SkipRows = (IMG_UINT32)(i32ClipTop - i32Y1);

		if (ui32SkipRows >= ui32Height) 
		{
			return IMG_FALSE;
		}

		ui32Height -= ui32SkipRows;

		i32Y1 = i32ClipTop;

		psSpanInfo->ui32DstSkipLines += ui32SkipRows;

		psSpanInfo->i32ReadY = i32Y1;
	}

	if (i32Y2 > i32ClipBottom) 
	{
		i32Temp = i32Y2 - i32ClipBottom;
		
		if (i32Temp >= (IMG_INT32)ui32Height) 
		{
			return IMG_FALSE;
		}
		
		ui32Height -= (IMG_UINT32)i32Temp;
	}

	psSpanInfo->ui32Width = ui32Width;
	psSpanInfo->ui32Height = ui32Height;

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : ElementsPerGroup
 Inputs             : format, type
 Outputs            : -
 Returns            : Number of elements per group
 Description        : UTILITY: Return the number of elements per group of a 
					  specified format. If the type is packed pixel, the number of 
					  elements per group is understood to be one.
************************************************************************************/
static IMG_UINT32 ElementsPerGroup(GLenum format, GLenum type)
{
	switch(type) 
	{
		case GL_UNSIGNED_SHORT_5_6_5:
		case GL_UNSIGNED_SHORT_4_4_4_4:
		case GL_UNSIGNED_SHORT_5_5_5_1:
#if defined(GLES2_EXTENSION_READ_FORMAT)
		case GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG:
#endif /* GLES2_EXTENSION_READ_FORMAT */
		{
			return 1;
		}
		default:
		{
			break;
		}
	}

	switch(format) 
	{
		case GL_RGB:
		{
			return 3;
		}
#if defined(GLES2_EXTENSION_READ_FORMAT)
		case GL_BGRA_IMG:
#endif /* GLES2_EXTENSION_READ_FORMAT */
		case GL_RGBA:
		{
			return 4;
		}
		case GL_LUMINANCE_ALPHA:
		{
			return 2;
		}
		default:
		{
			return 1;
		}
	}
}


/***********************************************************************************
 Function Name      : BytesPerElement
 Inputs             : type
 Outputs            : -
 Returns            : Number of bytes per element
 Description        : UTILITY: Return the number of bytes per element, based on the 
					  element type
************************************************************************************/
static IMG_UINT32 BytesPerElement(GLenum type)
{
	switch(type) 
	{
		case GL_UNSIGNED_SHORT_5_6_5:
		case GL_UNSIGNED_SHORT_4_4_4_4:
		case GL_UNSIGNED_SHORT_5_5_5_1:
#if defined(GLES2_EXTENSION_READ_FORMAT)
		case GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG:
#endif /* GLES2_EXTENSION_READ_FORMAT */
		{
			return 2;
		}
		case GL_UNSIGNED_BYTE:
		{
			return 1;
		}
		default:
		{
			return 0;
		}
	}
}


/***********************************************************************************
 Function Name      : BytesPerPixel
 Inputs             : ePixelFormat
 Outputs            : -
 Returns            : Number of bytes pixels
 Description        : UTILITY: Return the number of bytes per element, based on the 
					  element type
************************************************************************************/
static IMG_UINT32 BytesPerPixel(PVRSRV_PIXEL_FORMAT ePixelFormat)
{
	switch(ePixelFormat) 
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			return 2;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		case PVRSRV_PIXEL_FORMAT_XRGB8888:
		case PVRSRV_PIXEL_FORMAT_XBGR8888:
		{
			return 4;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"Unknown pixel format"));
			return 0;
		}
	}
}


/***********************************************************************************
 Function Name      : SetupReadPixelsSpanInfo
 Inputs             : gc, psSpanInfo, i32X, u32Y, ui32Width, ui32Height, format, type, 
					  bUsePackAlignment, psReadParams
 Outputs            : psSpanInfo
 Returns            : Anything to read.
 Description        : UTILITY: Sets up the spanInfo structure for a 
					  readpixels/copyteximage. 
************************************************************************************/
IMG_INTERNAL IMG_BOOL SetupReadPixelsSpanInfo(GLES2Context *gc, GLES2PixelSpanInfo *psSpanInfo,
											  IMG_INT32 i32X, IMG_INT32 i32Y, IMG_UINT32 ui32Width,
											  IMG_UINT32 ui32Height, GLenum format, GLenum type,
											  IMG_BOOL bUsePackAlignment, EGLDrawableParams *psReadParams)
{
	IMG_UINT32 ui32Alignment, ui32Padding;

	psSpanInfo->i32ReadX 	= i32X;
	psSpanInfo->i32ReadY 	= i32Y;
	psSpanInfo->ui32Width 	= ui32Width;
	psSpanInfo->ui32Height 	= ui32Height;

	if (!ClipReadPixels(psSpanInfo, psReadParams))
	{
		return IMG_FALSE;
	}

	/* bUsePackAlignment: This is set for readpixels, but not for copytexture */
	if (bUsePackAlignment)
	{
		ui32Alignment = gc->sState.sClientPixel.ui32PackAlignment;
	}
	else
	{
		ui32Alignment = 1;
	}

	psSpanInfo->ui32DstGroupIncrement = BytesPerElement(type) * ElementsPerGroup(format, type);
	psSpanInfo->ui32DstRowIncrement = ui32Width * psSpanInfo->ui32DstGroupIncrement;

	ui32Padding = (psSpanInfo->ui32DstRowIncrement % ui32Alignment);

	if (ui32Padding) 
	{
		psSpanInfo->ui32DstRowIncrement += ui32Alignment - ui32Padding;
	}

	switch(psReadParams->eRotationAngle)
	{
		case PVRSRV_ROTATE_0:
		default:
		{
			psSpanInfo->i32SrcGroupIncrement	= (IMG_INT32)BytesPerPixel(psReadParams->ePixelFormat);
			psSpanInfo->i32SrcRowIncrement 		= -(IMG_INT32)psReadParams->ui32Stride;
			psSpanInfo->i32ReadY 				= 1 + psSpanInfo->i32ReadY - (IMG_INT32)psReadParams->ui32Height;

			break;
		}
		case PVRSRV_ROTATE_90:
		{
			psSpanInfo->i32SrcGroupIncrement 	= (IMG_INT32)(psReadParams->ui32Stride);
			psSpanInfo->i32SrcRowIncrement 		= (IMG_INT32)BytesPerPixel(psReadParams->ePixelFormat);

			break;
		}
		case PVRSRV_ROTATE_180:
		{
			psSpanInfo->i32SrcGroupIncrement 	= -(IMG_INT32)(BytesPerPixel(psReadParams->ePixelFormat));
			psSpanInfo->i32SrcRowIncrement 		= (IMG_INT32)psReadParams->ui32Stride;
			psSpanInfo->i32ReadX 				= 1 + psSpanInfo->i32ReadX - (IMG_INT32)psReadParams->ui32Width;

			break;
		}
		case PVRSRV_ROTATE_270:
		{
			psSpanInfo->i32SrcGroupIncrement 	= -(IMG_INT32)(psReadParams->ui32Stride);
			psSpanInfo->i32SrcRowIncrement 		= -(IMG_INT32)BytesPerPixel(psReadParams->ePixelFormat);
			psSpanInfo->i32ReadX 				= 1 + psSpanInfo->i32ReadX - (IMG_INT32)psReadParams->ui32Width;
			psSpanInfo->i32ReadY 				= 1 + psSpanInfo->i32ReadY - (IMG_INT32)psReadParams->ui32Height;

			break;
		}		
		case PVRSRV_FLIP_Y:
		{
			psSpanInfo->i32SrcGroupIncrement 	= (IMG_INT32)BytesPerPixel(psReadParams->ePixelFormat);
			psSpanInfo->i32SrcRowIncrement 		= (IMG_INT32)psReadParams->ui32Stride;

			break;
		}
	}

	return IMG_TRUE;
}

//#define READPIXELS_OPTIMISATION 1

/***********************************************************************************
 Function Name      : glReadPixels
 Inputs             : x, y, width, height, format, type
 Outputs            : pixels
 Returns            : -
 Description        : ENTRYPOINT: Reads color pixels from the frame buffer.
					  Will require a render and wait for completion. 
					  Format conversion follows from this.
************************************************************************************/
GL_APICALL void GL_APIENTRY glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, 
									 GLenum format, GLenum type, void *pixels)
{
	GLES2PixelSpanInfo sSpanInfo = {0};
	PFNSpanPack pfnSpanPack = IMG_NULL;
	EGLDrawableParams *psReadParams;
	IMG_VOID *pvSurfacePointer;
	IMG_UINT32 i;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glReadPixels"));

	GLES2_TIME_START(GLES2_TIMES_glReadPixels);

	if(GetFrameBufferCompleteness(gc) != GL_FRAMEBUFFER_COMPLETE)
	{
		SetError(gc, GL_INVALID_FRAMEBUFFER_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

		return;
	}

	psReadParams = gc->psReadParams;

	/* No read surface specified */
	if(!psReadParams->psRenderSurface)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

		return;
	}

	if(!gc->psRenderSurface)
	{
		PVR_DPF((PVR_DBG_ERROR,"glReadPixels: Surface is invalid"));

		GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

		return;
	}

	if(!width || !height)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

		return;
	}

	if(!CheckReadPixelArgs(gc, width, height, format, type, &pfnSpanPack))
	{
		GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

		return;
	}

	if(!SetupReadPixelsSpanInfo(gc, &sSpanInfo, x, y, (IMG_UINT32)width, (IMG_UINT32)height,
								format, type, IMG_TRUE, psReadParams))
	{
		GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

		return;
	}

#if defined(READPIXELS_OPTIMISATION)

	/* Wait for any possible outstanding renders to complete if we're not in frame */
	if(!psReadParams->psRenderSurface->bInFrame)
	{
		if(ScheduleTA(gc, psReadParams->psRenderSurface, GLES2_SCHEDULE_HW_WAIT_FOR_3D) != IMG_EGL_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"glReadPixels: Failed to wait for HW flush"));

			GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

			return;
		}
	}
	else
	{
		IMG_BOOL bOptimisedReadPixels;

		/* Do a bounded render if the read area is less than a quarter of the read surface size */
		if((sSpanInfo.ui32Width*sSpanInfo.ui32Height) < ((psReadParams->ui32Width * psReadParams->ui32Height)>>2))
		{
			bOptimisedReadPixels = IMG_TRUE;
		}
		else
		{
			bOptimisedReadPixels = IMG_FALSE;
		}

		/* This optimisation doesn't seem to work on MSAA surfaces */
		if(psReadParams->psRenderSurface->bFirstKick)
		{
			if(gc->psMode->ui32AntiAliasMode)
			{
				bOptimisedReadPixels = IMG_FALSE;
			}
		}
		else
		{
			if(psReadParams->psRenderSurface->bMultiSample)
			{
				bOptimisedReadPixels = IMG_FALSE;
			}
		}

		if(bOptimisedReadPixels)
		{
			IMG_UINT32 ui32Left, ui32Right, ui32Top, ui32Bottom, ui32PreviousTerminateRegion;

			/* Save original terminate region */
			ui32PreviousTerminateRegion = psReadParams->psRenderSurface->ui32TerminateRegion;

			/* Calculate region for bounding box */
			ui32Left = sSpanInfo.i32ReadX >> EURASIA_TARNDCLIP_LR_ALIGNSHIFT;
			ui32Right = (sSpanInfo.i32ReadX+sSpanInfo.ui32Width-1) >> EURASIA_TARNDCLIP_LR_ALIGNSHIFT;

			if(psReadParams->eRotationAngle==PVRSRV_FLIP_Y)
			{
				ui32Top = (sSpanInfo.i32ReadY) >> EURASIA_TARNDCLIP_TB_ALIGNSHIFT;

				ui32Bottom = (sSpanInfo.i32ReadY+sSpanInfo.ui32Height-1) >> EURASIA_TARNDCLIP_TB_ALIGNSHIFT;
			}
			else
			{				
				ui32Top = (-sSpanInfo.i32ReadY-sSpanInfo.ui32Height+1) >> EURASIA_TARNDCLIP_TB_ALIGNSHIFT;

				ui32Bottom = (-sSpanInfo.i32ReadY) >> EURASIA_TARNDCLIP_TB_ALIGNSHIFT;
			}

			/* Set terminate region for bounding box render */
			psReadParams->psRenderSurface->ui32TerminateRegion = (ui32Left << EURASIA_TARNDCLIP_LEFT_SHIFT)		|
																 (ui32Right << EURASIA_TARNDCLIP_RIGHT_SHIFT)	|
																 (ui32Top << EURASIA_TARNDCLIP_TOP_SHIFT)		|
																 (ui32Bottom << EURASIA_TARNDCLIP_BOTTOM_SHIFT);

			/* Kick bounding box render and wait for render to complete */
			if(ScheduleTA(gc, psReadParams->psRenderSurface, GLES2_SCHEDULE_HW_LAST_IN_SCENE |
															 GLES2_SCHEDULE_HW_BBOX_RENDER |
															 GLES2_SCHEDULE_HW_WAIT_FOR_3D) != IMG_EGL_NO_ERROR)
			{
				PVR_DPF((PVR_DBG_ERROR,"glReadPixels: Couldn't flush HW"));

				GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

				return;
			}

			/* Restore terminate region */
			psReadParams->psRenderSurface->ui32TerminateRegion = ui32PreviousTerminateRegion;

			/* Kick full render and don't wait for the render to complete */
			if(ScheduleTA(gc, psReadParams->psRenderSurface, GLES2_SCHEDULE_HW_LAST_IN_SCENE | 
															 GLES2_SCHEDULE_HW_POST_BBOX_RENDER) != IMG_EGL_NO_ERROR)
			{
				PVR_DPF((PVR_DBG_ERROR,"glReadPixels: Couldn't flush HW"));

				GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

				return;
			}
		}
		else
		{
			/* Kick and wait for render to complete */
			if(ScheduleTA(gc, psReadParams->psRenderSurface, GLES2_SCHEDULE_HW_LAST_IN_SCENE |
															 GLES2_SCHEDULE_HW_WAIT_FOR_3D) != IMG_EGL_NO_ERROR)
			{
				PVR_DPF((PVR_DBG_ERROR,"glReadPixels: Couldn't flush HW"));

				GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

				return;
			}
		}
	}

#else /* defined(READPIXELS_OPTIMISATION) */

	/* Kick render if outstanding, and/or wait for render to complete */
	if(ScheduleTA(gc, psReadParams->psRenderSurface, GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_MIDSCENE_RENDER |
													 GLES2_SCHEDULE_HW_WAIT_FOR_3D) != IMG_EGL_NO_ERROR)
	{
		PVR_DPF((PVR_DBG_ERROR,"glReadPixels: Couldn't flush HW"));

		GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

		return;
	}

#endif /* defined(READPIXELS_OPTIMISATION) */

	pvSurfacePointer = GetStridedSurfaceData(gc, psReadParams, &sSpanInfo);

	if(!pvSurfacePointer)
	{
		PVR_DPF((PVR_DBG_ERROR,"glReadPixels: Failed to get strided data"));

		GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);

		return;
	}

	sSpanInfo.pvOutData = (IMG_VOID *) (((IMG_UINT8*) pixels) +
										sSpanInfo.ui32DstSkipLines * sSpanInfo.ui32DstRowIncrement +
										sSpanInfo.ui32DstSkipPixels * sSpanInfo.ui32DstGroupIncrement);

	sSpanInfo.pvInData = (IMG_VOID *) (((IMG_UINT8*) pvSurfacePointer) + 
										sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement + 
										sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);

	for (i=0; i<sSpanInfo.ui32Height; i++) 
	{
		(*pfnSpanPack)(&sSpanInfo);

		sSpanInfo.pvOutData = (IMG_VOID *) ((IMG_UINT8 *)sSpanInfo.pvOutData + sSpanInfo.ui32DstRowIncrement);

		sSpanInfo.pvInData  = (IMG_VOID *) ((IMG_UINT8 *)sSpanInfo.pvInData  + sSpanInfo.i32SrcRowIncrement);
	}

	if(pvSurfacePointer!=psReadParams->pvLinSurfaceAddress)
	{
		GLES2FreeAsync(gc, pvSurfacePointer);
	}

	GLES2_INC_PIXEL_COUNT(GLES2_TIMES_glReadPixels, sSpanInfo.ui32Width * sSpanInfo.ui32Height);

	GLES2_TIME_STOP(GLES2_TIMES_glReadPixels);
}

/******************************************************************************
 End of file pixelop.c
******************************************************************************/
