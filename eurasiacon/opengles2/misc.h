/**************************************************************************
 * Name         : misc.h
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: misc.h $
 *
 **************************************************************************/

#ifndef _MISC_
#define _MISC_

/*****************************************************************************/


#define GLES2_EXTENSION_BIT_FBO_RENDER_MIPMAP			0x00000001
#define GLES2_EXTENSION_BIT_RGB8_RGBA8					0x00000002
#define GLES2_EXTENSION_BIT_DEPTH24						0x00000004
#define GLES2_EXTENSION_BIT_VERTEX_HALF_FLOAT			0x00000008
#define GLES2_EXTENSION_BIT_TEXTURE_FLOAT				0x00000010
#define GLES2_EXTENSION_BIT_TEXTURE_HALF_FLOAT			0x00000020
#define GLES2_EXTENSION_BIT_ELEMENT_INDEX_UINT			0x00000040
#define GLES2_EXTENSION_BIT_MAPBUFFER					0x00000080
#define GLES2_EXTENSION_BIT_FRAGMENT_PRECISION_HIGH		0x00000100
#define GLES2_EXTENSION_BIT_IMG_SHADER_BINARY 			0x00000200
#define GLES2_EXTENSION_BIT_PVRTC						0x00000400
#define GLES2_EXTENSION_BIT_ETC1						0x00000800
#define GLES2_EXTENSION_BIT_EGL_IMAGE					0x00001000
#define GLES2_EXTENSION_BIT_DRAW_ARRAYS					0x00002000
#define GLES2_EXTENSION_BIT_TEXTURE_STREAM				0x00004000
#define GLES2_EXTENSION_BIT_NPOT						0x00008000
#define GLES2_EXTENSION_BIT_REQUIRED_INTERNAL_FORMAT	0x00010000
#define GLES2_EXTENSION_BIT_DEPTH_TEXTURE				0x00020000
#define GLES2_EXTENSION_BIT_TEXTURE_FORMAT_BGRA8888		0x00040000
#define GLES2_EXTENSION_BIT_READ_FORMAT					0x00080000
#define GLES2_EXTENSION_BIT_GET_PROGRAM_BINARY			0x00100000
#define GLES2_EXTENSION_BIT_PACKED_DEPTH_STENCIL		0x00200000
#define	GLES2_EXTENSION_BIT_STANDARD_DERIVATIVES		0x00400000
#define GLES2_EXTENSION_BIT_VERTEX_ARRAY_OBJECT         0x00800000
#define GLES2_EXTENSION_BIT_DISCARD_FRAMEBUFFER         0x01000000
#define GLES2_EXTENSION_BIT_EGL_SYNC                    0x02000000
#define GLES2_EXTENSION_BIT_MULTISAMPLED_RENDER_TO_TEX	0x08000000
#define GLES2_EXTENSION_BIT_SHADER_TEXTURE_LOD			0x10000000
#define GLES2_EXTENSION_BIT_EGL_IMAGE_EXTERNAL			0x20000000

#define GLES2_HINT_OVERLOAD_TEX_LAYOUT_POW2_MASK		0x0000000F
#define GLES2_HINT_OVERLOAD_TEX_LAYOUT_POW2_SHIFT		0

#define GLES2_HINT_OVERLOAD_TEX_LAYOUT_NONPOW2_MASK		0x000000F0
#define GLES2_HINT_OVERLOAD_TEX_LAYOUT_NONPOW2_SHIFT	4

#define GLES2_HINT_OVERLOAD_TEX_LAYOUT_DEFAULT			0
#define GLES2_HINT_OVERLOAD_TEX_LAYOUT_STRIDE			1
#define GLES2_HINT_OVERLOAD_TEX_LAYOUT_TILED			2
#define GLES2_HINT_OVERLOAD_TEX_LAYOUT_TWIDDLED			3

#define GLES2_HINT_KICK_DEFAULT        0
#define GLES2_HINT_KICK_TA             1
#define GLES2_HINT_KICK_3D             2

#define	GLES_FLOAT		0	/* __GLfloat */
#define GLES_FIXED		1	/* api 32 bit fixed */
#define GLES_INT32		2	/* api 32 bit int */
#define GLES_BOOLEAN	3	/* api 8 bit boolean */
#define GLES_COLOR		4	/* unscaled color in __GLfloat */
#define GLES_ENUM		5	/* enums - not shifted for fixed */

typedef struct GLESAppHintsRec
{
	IMG_BOOL	bDumpCompilerLogFiles;

	IMG_UINT32	ui32ExternalZBufferMode;

	IMG_BOOL	bFBODepthDiscard;

	IMG_BOOL	bOptimisedValidation;

	IMG_BOOL	bDisableHWTQTextureUpload;
	IMG_BOOL    bDisableHWTQNormalBlit;
    IMG_BOOL    bDisableHWTQBufferBlit;
    IMG_BOOL    bDisableHWTQMipGen;

    IMG_UINT32  ui32FlushBehaviour;

	IMG_UINT32	ui32OverloadTexLayout;
	IMG_BOOL    bEnableStaticMTECopy;
	IMG_UINT32	ui32AdjustShaderPrecision;
	IMG_UINT32  ui32DefaultVertexBufferSize;
	IMG_UINT32  ui32MaxVertexBufferSize;
	IMG_UINT32  ui32DefaultIndexBufferSize;
	IMG_UINT32  ui32DefaultPDSVertBufferSize;
	IMG_UINT32  ui32DefaultPregenMTECopyBufferSize;
	IMG_UINT32  ui32DefaultVDMBufferSize;
	IMG_BOOL    bStrictBinaryVersionComparison;
	IMG_FLOAT   fPolygonUnitsMultiplier;
	IMG_FLOAT   fPolygonFactorMultiplier;

	IMG_BOOL	bEnableVaryingPrecisionOpt;
	IMG_BOOL	bAllowTrilinearNPOT;

	IMG_BOOL    bInitialiseVSOutputs;

#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)
	IMG_UINT32  ui32KickTAMode;
	IMG_UINT32	ui32KickTAThreshold;
#endif

#if defined(EUR_CR_ISP_TAGCTRL_TRIANGLE_FRAGMENT_PIXELS_MASK)
	IMG_UINT32	ui32TriangleSplitPixelThreshold;
	IMG_BOOL	bDynamicSplitCalc;
#endif

#if defined (TIMING) || defined (DEBUG)
	IMG_BOOL 	bDumpProfileData;
	IMG_UINT32 	ui32ProfileStartFrame;
	IMG_UINT32 	ui32ProfileEndFrame;
	IMG_BOOL    bEnableMemorySpeedTest;
	IMG_BOOL	bDisableMetricsOutput;
#endif /* (TIMING) || (DEBUG) */

#if defined(DEBUG)
	IMG_BOOL    bDumpUSPOutput;
	IMG_BOOL	bDumpShaderAnalysis;
	IMG_BOOL	bTrackUSCMemory;
#endif

	IMG_BOOL    bEnableAppTextureDependency;

	/* PSP2-specific */

	IMG_UINT32 ui32CDRAMTexHeapSize;
	IMG_UINT32 ui32UNCTexHeapSize;
	IMG_BOOL bEnableCDRAMAutoExtend;
	IMG_BOOL bEnableUNCAutoExtend;
	IMG_UINT32 ui32SwTexOpThreadNum;
	IMG_UINT32 ui32SwTexOpThreadPriority;
	IMG_UINT32 ui32SwTexOpThreadAffinity;
	IMG_UINT32 ui32SwTexOpMaxUltNum;
	IMG_UINT32 ui32SwTexOpCleanupDelay;
} GLESAppHints;



IMG_BOOL GetApplicationHints(GLESAppHints *psAppHints, EGLcontextMode *psMode);

IMG_VOID SetErrorFileLine(GLES2Context *gc, GLenum code, const IMG_CHAR *szFile, int iLine);
#if defined(DEBUG)
#define SetError(gc, code) SetErrorFileLine(gc, code, __FILE__, __LINE__)
#else
#define SetError(gc, code) SetErrorFileLine(gc, code, "", 0)
#endif

IMG_UINT16 GLES2ConvertFloatToC10(IMG_FLOAT fValue);
IMG_UINT16 GLES2ConvertFloatToF16(IMG_FLOAT fValue);
IMG_FLOAT Clampf(IMG_FLOAT fVal, IMG_FLOAT fMin, IMG_FLOAT fMax);
IMG_INT32 Clampi(IMG_INT32 i32Val, IMG_INT32 i32Min, IMG_INT32 i32Max);
IMG_UINT32 FloorLog2(IMG_UINT32 ui32Val);
IMG_UINT32 ColorConvertToHWFormat(GLES2color *psColor);
IMG_BOOL BuildExtensionString(GLES2Context *gc);
IMG_VOID DestroyExtensionString(GLES2Context *gc);

IMG_VOID ConvertData(IMG_UINT32 ui32FromType, const IMG_VOID *pvRawdata, IMG_UINT32 ui32ToType, IMG_VOID *pvResult, IMG_UINT32 ui32Size);

#define GLES_CEILF(f)			(ceilf(f))
#define GLES_SQRTF(f)			(sqrtf(f))
#define GLES_POWF(a,b)			(powf(a,b))
#define GLES_ABSF(f)			(fabsf(f))
#define GLES_ABS(i)				(abs(i))
#define GLES_FLOORF(f)			(floorf(f))
#define GLES_SINF(f)			(sinf(f))
#define GLES_COSF(f)			(cosf(f))
#define GLES_ATANF(f)			(atanf(f))
#define GLES_LOGF(f)			(logf(f))
#define GLES_FREXP(f,e)			(frexpf(f, (int *)e))

#endif /* _MISC_ */
