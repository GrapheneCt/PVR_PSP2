/**************************************************************************
 * Name         : texformat.c
 * Author       : BCB
 * Created      : 02/03/2006
 *
 * Copyright    : 2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Date: 2011/06/22 10:50:06 $ $Revision: 1.33 $
 * $Log: texformat.c $
 **************************************************************************/
#include "context.h"

/* These are structures which define HW texture formats */
IMG_INTERNAL const GLES2TextureFormat TexFormatABGR8888 = 
{
    1,											/* ui32NumChunks */
	4,											/* ui32TotalBytesPerTexel */
	GLES2_RGBA_TEX_INDEX,						/* ui32BaseFormatIndex */
    {
        USP_TEXTURE_FORMAT_R8G8B8A8,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_ABGR8888				/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatARGB8888 = 
{
    1,											/* ui32NumChunks */
	4,											/* ui32TotalBytesPerTexel */
	GLES2_RGBA_TEX_INDEX,						/* ui32BaseFormatIndex */
    {
        USP_TEXTURE_FORMAT_B8G8R8A8,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_ARGB8888				/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatARGB1555 = 
{
    1,											/* ui32NumChunks */
	2,											/* ui32TotalBytesPerTexel */
	GLES2_RGBA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_A1R5G5B5,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_ARGB1555				/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatARGB4444 = 
{
    1,											/* ui32NumChunks */
	2,											/* ui32TotalBytesPerTexel */
	GLES2_RGBA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{  
        USP_TEXTURE_FORMAT_A4R4G4B4,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_ARGB4444				/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatXBGR8888 = 
{
    1,											/* ui32NumChunks */
	4,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_R8G8B8A8,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_ABGR8888				/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatXRGB8888 = 
{
    1,											/* ui32NumChunks */
	4,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_B8G8R8A8,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_ARGB8888				/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatRGB565 = 
{
    1,											/* ui32NumChunks */
	2,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_R5G6B5,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_RGB565					/* ePixelFormat */
};

/* Use Channel replicate */
IMG_INTERNAL const GLES2TextureFormat TexFormatAlpha = 
{
    1,											/* ui32NumChunks */
	1,											/* ui32TotalBytesPerTexel */
	GLES2_ALPHA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_A8,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_A8						/* ePixelFormat */

};

/* Use Channel replicate */
IMG_INTERNAL const GLES2TextureFormat TexFormatLuminance = 
{
    1,											/* ui32NumChunks */
	1,											/* ui32TotalBytesPerTexel */
	GLES2_LUMINANCE_TEX_INDEX,					/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_L8,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_L8						/* ePixelFormat */
};

/* Use Channel replicate */
IMG_INTERNAL const GLES2TextureFormat TexFormatLuminanceAlpha = 
{
    1,											/* ui32NumChunks */
	2,											/* ui32TotalBytesPerTexel */
	GLES2_LUMINANCE_ALPHA_TEX_INDEX,			/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_L8A8,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_A8L8					/* ePixelFormat */

};


IMG_INTERNAL const GLES2TextureFormat TexFormatPVRTC2RGB = 
{
    1,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_PVRT2BPP_RGB,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_PVRTC2					/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatPVRTC4RGB = 
{
    1,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_PVRT4BPP_RGB,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */ 
	PVRSRV_PIXEL_FORMAT_PVRTC4					/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatPVRTC2RGBA = 
{
    1,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_RGBA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_PVRT2BPP_RGBA,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_PVRTC2					/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatPVRTC4RGBA = 
{
    1,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_RGBA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_PVRT4BPP_RGBA,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_PVRTC4					/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatPVRTCII2RGB = 
{
    1,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_PVRTII2BPP_RGB,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_PVRTCII2				/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatPVRTCII4RGB = 
{
    1,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_PVRTII4BPP_RGB,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_PVRTCII4				/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatPVRTCII2RGBA = 
{
    1,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_RGBA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_PVRTII2BPP_RGBA,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_PVRTCII2				/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatPVRTCII4RGBA = 
{
    1,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_RGBA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_PVRTII4BPP_RGBA,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_PVRTCII4				/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatETC1RGB = 
{
    1,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_PVRTIII,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_PVRTCIII				/* ePixelFormat */

};


#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)

IMG_INTERNAL const GLES2TextureFormat TexFormatRGBAFloat = 
{
    4,											/* ui32NumChunks */
	16,											/* ui32TotalBytesPerTexel */
	GLES2_RGBA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_RGBA_F32,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_A32B32G32R32F			/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatRGBFloat = 
{
    3,											/* ui32NumChunks */
	12,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_RGB_F32,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_B32G32R32F					/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatFloatAlpha = 
{
    1,											/* ui32NumChunks */
	4,											/* ui32TotalBytesPerTexel */
	GLES2_ALPHA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_AF32,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_A_F32					/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatFloatLuminance = 
{
    1,											/* ui32NumChunks */
	4,											/* ui32TotalBytesPerTexel */
	GLES2_LUMINANCE_TEX_INDEX,					/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_LF32,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_L_F32					/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatFloatLuminanceAlpha = 
{
    2,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_LUMINANCE_ALPHA_TEX_INDEX,			/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_LF32AF32,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_L_F32_A_F32				/* ePixelFormat */

};

#endif /* GLES2_EXTENSION_FLOAT_TEXTURE */

#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)

IMG_INTERNAL const GLES2TextureFormat TexFormatRGBAHalfFloat = 
{
    2,											/* ui32NumChunks */
	8,											/* ui32TotalBytesPerTexel */
	GLES2_RGBA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_RGBA_F16,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_A16B16G16R16F			/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatRGBHalfFloat = 
{
    2,											/* ui32NumChunks */
	6,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_RGB_F16,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_B16G16R16F				/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatHalfFloatAlpha = 
{
    1,											/* ui32NumChunks */
	2,											/* ui32TotalBytesPerTexel */
	GLES2_ALPHA_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_AF16,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_A_F16					/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatHalfFloatLuminance = 
{
    1,											/* ui32NumChunks */
	2,											/* ui32TotalBytesPerTexel */
	GLES2_LUMINANCE_TEX_INDEX,					/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_LF16,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_L_F16					/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatHalfFloatLuminanceAlpha = 
{
    1,											/* ui32NumChunks */
	4,											/* ui32TotalBytesPerTexel */
	GLES2_LUMINANCE_ALPHA_TEX_INDEX,			/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_LF16AF16,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_L_F16_A_F16				/* ePixelFormat */

};

#endif /* GLES2_EXTENSION_HALF_FLOAT_TEXTURE */


IMG_INTERNAL const GLES2TextureFormat TexFormatUYVY = 
{
    1,											/* ui32NumChunks */
	2,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_UYVY,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY			/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatVYUY = 
{
    1,											/* ui32NumChunks */
	2,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_VYUY,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY			/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormatYUYV = 
{
    1,											/* ui32NumChunks */
	2,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_YUYV,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV			/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatYVYU = 
{
    1,											/* ui32NumChunks */
	2,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_YVYU,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU			/* ePixelFormat */

};

IMG_INTERNAL const GLES2TextureFormat TexFormatYUV420 = 
{
#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
    1,											/* ui32NumChunks */
#else 
    2,											/* ui32NumChunks */
#endif /* defined (SGX_FEATURE_TAG_YUV_TO_RGB) */
	3,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
	    USP_TEXTURE_FORMAT_C0_YUV420_2P_UV, 
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_NV12					/* ePixelFormat */
};

IMG_INTERNAL const GLES2TextureFormat TexFormat3PlaneYUV420 = 
{
#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
    1,											/* ui32NumChunks */
#else 
    3,											/* ui32NumChunks */
#endif /* defined (SGX_FEATURE_TAG_YUV_TO_RGB) */
	1,											/* ui32TotalBytesPerTexel */
	GLES2_RGB_TEX_INDEX,						/* ui32BaseFormatIndex */
	{
	    USP_TEXTURE_FORMAT_C0_YUV420_3P_UV, 
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_YV12,					/* ePixelFormat */
};


#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)

/* Use Channel replicate */
IMG_INTERNAL const GLES2TextureFormat TexFormatFloatDepth = 
{
    1,											/* ui32NumChunks */
	4,											/* ui32TotalBytesPerTexel */
	GLES2_DEPTH_TEX_INDEX,				    	/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_DF32,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_D32F					/* ePixelFormat */
};

#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */


#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)

/* Use Channel replicate */
IMG_INTERNAL const GLES2TextureFormat TexFormatFloatDepthU8Stencil = 
{
    2,											/* ui32NumChunks */
	5,											/* ui32TotalBytesPerTexel */
	GLES2_DEPTH_STENCIL_TEX_INDEX,				/* ui32BaseFormatIndex */
	{
        USP_TEXTURE_FORMAT_DF32,
        {USP_CHAN_SWIZZLE_CHAN_0, USP_CHAN_SWIZZLE_CHAN_1, USP_CHAN_SWIZZLE_CHAN_2, USP_CHAN_SWIZZLE_CHAN_3}
    },                                          /* sTexFormat */
	PVRSRV_PIXEL_FORMAT_D32F					/* ePixelFormat */

};

#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */
