/**************************************************************************
 * Name         : texformat.h
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
 * $Date: 2011/06/22 10:50:06 $ $Revision: 1.14 $
 * $Log: texformat.h $
 **************************************************************************/

#ifndef _TEXFORMAT_
#define _TEXFORMAT_

#include "sgxpixfmts.h"

#define GLES2_MAX_CHUNKS					4

typedef struct GLES2TextureFormatRec
{
	IMG_UINT32 ui32NumChunks;
	IMG_UINT32 ui32TotalBytesPerTexel;
	IMG_UINT32 ui32BaseFormatIndex;
	USP_TEX_FORMAT sTexFormat;
	PVRSRV_PIXEL_FORMAT ePixelFormat;

} GLES2TextureFormat;

/* These are structures which define HW texture formats */
extern const GLES2TextureFormat TexFormatABGR8888;
extern const GLES2TextureFormat TexFormatARGB1555;
extern const GLES2TextureFormat TexFormatARGB4444;
extern const GLES2TextureFormat TexFormatXBGR8888;
extern const GLES2TextureFormat TexFormatXRGB8888;
extern const GLES2TextureFormat TexFormatRGB565;
extern const GLES2TextureFormat TexFormatAlpha;
extern const GLES2TextureFormat TexFormatLuminance;
extern const GLES2TextureFormat TexFormatLuminanceAlpha;
extern const GLES2TextureFormat TexFormatPVRTC2RGB;
extern const GLES2TextureFormat TexFormatPVRTC4RGB;
extern const GLES2TextureFormat TexFormatPVRTC2RGBA;
extern const GLES2TextureFormat TexFormatPVRTC4RGBA;
extern const GLES2TextureFormat TexFormatPVRTCII2RGB;
extern const GLES2TextureFormat TexFormatPVRTCII4RGB;
extern const GLES2TextureFormat TexFormatPVRTCII2RGBA;
extern const GLES2TextureFormat TexFormatPVRTCII4RGBA;
extern const GLES2TextureFormat TexFormatETC1RGB;
extern const GLES2TextureFormat TexFormatRGBAFloat;
extern const GLES2TextureFormat TexFormatRGBAHalfFloat;
extern const GLES2TextureFormat TexFormatRGBFloat;
extern const GLES2TextureFormat TexFormatRGBHalfFloat;
extern const GLES2TextureFormat TexFormatFloatAlpha;
extern const GLES2TextureFormat TexFormatHalfFloatAlpha;
extern const GLES2TextureFormat TexFormatFloatLuminance;
extern const GLES2TextureFormat TexFormatHalfFloatLuminance;
extern const GLES2TextureFormat TexFormatFloatLuminanceAlpha;
extern const GLES2TextureFormat TexFormatHalfFloatLuminanceAlpha;
extern const GLES2TextureFormat TexFormatUYVY;
extern const GLES2TextureFormat TexFormatYUYV;
extern const GLES2TextureFormat TexFormatVYUY;
extern const GLES2TextureFormat TexFormatYVYU;
extern const GLES2TextureFormat TexFormatYUV420;
extern const GLES2TextureFormat TexFormat3PlaneYUV420;
extern const GLES2TextureFormat TexFormatARGB8888;

#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
extern const GLES2TextureFormat TexFormatFloatDepth;
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */

#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
extern const GLES2TextureFormat TexFormatFloatDepthU8Stencil;
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

#endif /* _TEXFORMAT_ */



