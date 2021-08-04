/******************************************************************************
* Name         : misc.h
*
* Copyright    : 2003-2006 by Imagination Technologies Limited.
*              : All rights reserved. No part of this software, either
*              : material or conceptual may be copied or distributed,
*              : transmitted, transcribed, stored in a retrieval system or
*              : translated into any human or computer language in any form
*              : by any means, electronic, mechanical, manual or otherwise,
*              : or disclosed to third parties without the express written
*              : permission of Imagination Technologies Limited, Home Park
*              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
*
* Platform     : ANSI
*
* $Log: misc.h $
******************************************************************************/
#ifndef _MISC_
#define _MISC_

#if defined (__cplusplus)
extern "C" {
#endif

/*****************************************************************************/

#define GLES1_HINT_KICK_DEFAULT        0
#define GLES1_HINT_KICK_TA             1
#define GLES1_HINT_KICK_3D             2

#define	GLES1_FLOAT		0	/* __GLfloat */
#define GLES1_FIXED		1	/* api 32 bit fixed */
#define GLES1_INT32		2	/* api 32 bit int */
#define GLES1_BOOLEAN	3	/* api 8 bit boolean */
#define GLES1_COLOR		4	/* unscaled color in __GLfloat */
#define GLES1_ENUM		5	/* enums - not shifted for fixed */

typedef struct GLESAppHintsRec
{
	IMG_BOOL	bDumpShaders;

	IMG_UINT32	ui32ExternalZBufferMode;

	IMG_BOOL	bFBODepthDiscard;

	IMG_BOOL	bOptimisedValidation;

	IMG_BOOL	bDisableHWTQTextureUpload;
	IMG_BOOL    bDisableHWTQNormalBlit;
    IMG_BOOL    bDisableHWTQMipGen;

    IMG_UINT32  ui32FlushBehaviour;

	IMG_BOOL    bEnableStaticPDSVertex;
	IMG_BOOL    bEnableStaticMTECopy;

	IMG_BOOL    bDisableStaticPDSPixelSAProgram;

	IMG_BOOL	bDisableUSEASMOPT;

#if defined(FIX_HW_BRN_25211)
	IMG_BOOL	bUseC10Colours;
#endif /* defined(FIX_HW_BRN_25211) */

	IMG_UINT32  ui32DefaultVertexBufferSize;
	IMG_UINT32  ui32MaxVertexBufferSize;
	IMG_UINT32  ui32DefaultIndexBufferSize;
	IMG_UINT32  ui32DefaultPDSVertBufferSize;
	IMG_UINT32  ui32DefaultPregenPDSVertBufferSize;
	IMG_UINT32  ui32DefaultPregenMTECopyBufferSize;
	IMG_UINT32  ui32DefaultVDMBufferSize;

#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)
	IMG_UINT32  ui32KickTAMode;
	IMG_UINT32	ui32KickTAThreshold;
#endif

#if defined (TIMING) || defined (DEBUG)
	IMG_BOOL 	bDumpProfileData;
	IMG_UINT32 	ui32ProfileStartFrame;
	IMG_UINT32 	ui32ProfileEndFrame;
	IMG_BOOL    bEnableMemorySpeedTest;
	IMG_BOOL    bDisableMetricsOutput;
#endif /* (TIMING) || (DEBUG) */

	IMG_BOOL    bEnableAppTextureDependency;

	/* PSP2-specific */

	IMG_UINT32 ui32OGLES1CDRAMTexHeapSize;
	IMG_UINT32 ui32OGLES1UNCTexHeapSize;
	IMG_BOOL bOGLES1EnableCDRAMAutoExtend;
	IMG_BOOL bOGLES1EnableUNCAutoExtend;
	IMG_UINT32 ui32OGLES1SwTexOpThreadNum;
	IMG_UINT32 ui32OGLES1SwTexOpThreadPriority;
	IMG_UINT32 ui32OGLES1SwTexOpThreadAffinity;
} GLESAppHints;

IMG_BOOL GetApplicationHints(GLESAppHints *psAppHints, EGLcontextMode *psMode);


IMG_VOID SetErrorFileLine(GLES1Context *gc, GLenum code, const IMG_CHAR *szFile, int iLine);
#if defined(DEBUG)
#define SetError(gc, code) SetErrorFileLine(gc, code, __FILE__, __LINE__)
#else
#define SetError(gc, code) SetErrorFileLine(gc, code, "", 0)
#endif

IMG_FLOAT Clampf(IMG_FLOAT fVal, IMG_FLOAT fMin, IMG_FLOAT fMax);
IMG_INT32 Clampi(IMG_INT32 i32Val, IMG_INT32 i32Min, IMG_INT32 i32Max);
IMG_FLOAT Floor(IMG_FLOAT fVal);
IMG_UINT32 FloorLog2(IMG_UINT32 ui32Val);

IMG_UINT32 FloatToHWByte(IMG_FLOAT fValue);
IMG_UINT32 ColorConvertToHWFormat(GLEScolor *psColor);
IMG_VOID ColorConvertFromHWFormat(GLEScolor *psColor, IMG_UINT32 ui32Color);

IMG_BOOL BuildExtensionString(GLES1Context *gc);
IMG_VOID DestroyExtensionString(GLES1Context *gc);

IMG_VOID ConvertData(IMG_UINT32 ui32FromType, const IMG_VOID *pvRawdata, IMG_UINT32 ui32ToType, IMG_VOID *pvResult, IMG_UINT32 ui32Size);

/*****************************************************************************/

#if defined (__cplusplus)
}
#endif
#endif /* _MISC_ */

/******************************************************************************
 End of file (misc.h)
******************************************************************************/



