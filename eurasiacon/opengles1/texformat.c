/**************************************************************************
 * Name         : texformat.c
 *
 * Copyright    : 2006-2009 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: texformat.c $
 **************************************************************************/
#include "context.h"

/* These are structures which define HW texture formats */
IMG_INTERNAL const GLESTextureFormat TexFormatABGR8888 = 
{
	4,										/* ui32TotalBytesPerTexel */
	GLES1_RGBA_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_ABGR8888,			/* ePixelFormat */
};

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE) || defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
IMG_INTERNAL const GLESTextureFormat TexFormatARGB8888 = 
{
	4,										/* ui32TotalBytesPerTexel */
	GLES1_RGBA_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_ARGB8888,			/* ePixelFormat */
};
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */


IMG_INTERNAL const GLESTextureFormat TexFormatARGB1555 = 
{
	2,										/* ui32TotalBytesPerTexel */
	GLES1_RGBA_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_ARGB1555,			/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatARGB4444 = 
{
	2,										/* ui32TotalBytesPerTexel */
	GLES1_RGBA_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_ARGB4444,			/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatXBGR8888 = 
{
	4,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_ABGR8888,			/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatXRGB8888 = 
{
	4,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_ARGB8888,			/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatRGB565 = 
{
	2,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_RGB565,				/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatAlpha = {
	1,										/* ui32TotalBytesPerTexel */
	GLES1_ALPHA_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_A8,					/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatLuminance = 
{
	1,										/* ui32TotalBytesPerTexel */
	GLES1_LUMINANCE_TEX_INDEX,				/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_L8,					/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatLuminanceAlpha = 
{
	2,										/* ui32TotalBytesPerTexel */
	GLES1_LUMINANCE_ALPHA_TEX_INDEX,		/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_A8L8,				/* ePixelFormat */
};

#if defined(GLES1_EXTENSION_PVRTC)
IMG_INTERNAL const GLESTextureFormat TexFormatPVRTC2RGB = 
{
	8,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_PVRTC2,				/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatPVRTC4RGB = 
{
	8,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_PVRTC4,				/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatPVRTC2RGBA = 
{
	8,										/* ui32TotalBytesPerTexel */
	GLES1_RGBA_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_PVRTC2,				/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatPVRTC4RGBA = 
{
	8,										/* ui32TotalBytesPerTexel */
	GLES1_RGBA_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_PVRTC4,				/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatPVRTCII2RGB = 
{
	8,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_PVRTCII2,			/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatPVRTCII4RGB = 
{
	8,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_PVRTCII4,			/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatPVRTCII2RGBA = 
{
	8,										/* ui32TotalBytesPerTexel */
	GLES1_RGBA_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_PVRTCII2,			/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatPVRTCII4RGBA = 
{
	8,										/* ui32TotalBytesPerTexel */
	GLES1_RGBA_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_PVRTCII4,			/* ePixelFormat */
};

#endif /* defined(GLES1_EXTENSION_PVRTC) */


#if defined(GLES1_EXTENSION_ETC1)

IMG_INTERNAL const GLESTextureFormat TexFormatETC1RGB8 = 
{
	8,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_PVRTCIII,			/* ePixelFormat */
};

#endif /* defined(GLES1_EXTENSION_ETC1) */
		
#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)

IMG_INTERNAL const GLESTextureFormat TexFormatUYVY = 
{
	2,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY,	/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatVYUY = 
{
	2,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY,	/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatYUYV = 
{
	2,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV,	/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormatYVYU = 
{
	2,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU,	/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormat2PlaneYUV420 = 
{
	1,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_NV12,				/* ePixelFormat */
};

IMG_INTERNAL const GLESTextureFormat TexFormat3PlaneYUV420 = 
{
	1,										/* ui32TotalBytesPerTexel */
	GLES1_RGB_TEX_INDEX,					/* ui32BaseFormatIndex */
	PVRSRV_PIXEL_FORMAT_YV12,				/* ePixelFormat */
};

#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL) */




