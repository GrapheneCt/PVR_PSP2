/******************************************************************************
 * Name         : misc.c
 *
 * Copyright    : 2003-2008 by Imagination Technologies Limited.
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
 * $Log: misc.c $
 * 
 * Corrected sense of disable HW texture upload..
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "context.h"
#include <string.h>

static const IMG_CHAR gszExtensions[] =

	/* Core extensions */
	"GL_OES_byte_coordinates "
	"GL_OES_fixed_point "
	"GL_OES_single_precision "
	"GL_OES_matrix_get "

	/* Required extensions */
	"GL_OES_read_format "
	"GL_OES_compressed_paletted_texture "
	"GL_OES_point_sprite "
	"GL_OES_point_size_array "


	/* Optional extensions */
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
	"GL_OES_matrix_palette "
#endif
#if defined(GLES1_EXTENSION_DRAW_TEXTURE)
	"GL_OES_draw_texture "
#endif
#if defined(GLES1_EXTENSION_QUERY_MATRIX)
	"GL_OES_query_matrix "
#endif


	/* Extension pack */
#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
	"GL_OES_texture_env_crossbar "
#endif
#if defined(GLES1_EXTENSION_TEX_MIRRORED_REPEAT)
	"GL_OES_texture_mirrored_repeat "
#endif
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	"GL_OES_texture_cube_map "
#endif
#if defined(GLES1_EXTENSION_BLEND_SUBTRACT)
	"GL_OES_blend_subtract "
#endif
#if defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE)
	"GL_OES_blend_func_separate "
#endif
#if defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE)
	"GL_OES_blend_equation_separate "
#endif
#if defined(GLES1_EXTENSION_STENCIL_WRAP)
	"GL_OES_stencil_wrap "
#endif
#if defined(GLES1_EXTENSION_EXTENDED_MATRIX_PALETTE)
	"GL_OES_extended_matrix_palette "
#endif
#if defined(GLES1_EXTENSION_FRAME_BUFFER_OBJECTS)
	"GL_OES_framebuffer_object "

	#if defined(GLES1_EXTENSION_RGB8_RGBA8)
		"GL_OES_rgb8_rgba8 "
	#endif
	#if defined(GLES1_EXTENSION_DEPTH24)
		"GL_OES_depth24 "
	#endif
	#if defined(GLES1_EXTENSION_STENCIL8)
		"GL_OES_stencil8 "
	#endif
#endif


	/* Extras */
#if defined(GLES1_EXTENSION_ETC1)
	"GL_OES_compressed_ETC1_RGB8_texture "
#endif
#if defined(GLES1_EXTENSION_MAP_BUFFER)
	"GL_OES_mapbuffer "
#endif
#if defined(GLES1_EXTENSION_EGL_IMAGE)
	"GL_OES_EGL_image "
#endif
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
	"GL_OES_EGL_image_external "
#endif
#if defined(GLES1_EXTENSION_MULTI_DRAW_ARRAYS)
	"GL_EXT_multi_draw_arrays "
#endif
#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
	"GL_OES_required_internalformat "
#endif

	/* IMG extensions */
#if defined(GLES1_EXTENSION_READ_FORMAT)
	"GL_IMG_read_format "
#endif
#if defined(GLES1_EXTENSION_PVRTC)
	"GL_IMG_texture_compression_pvrtc "
#endif
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
	"GL_IMG_texture_format_BGRA8888 "
	"GL_EXT_texture_format_BGRA8888 "
#endif
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
	"GL_IMG_texture_stream "
#endif

#if defined(EGL_EXTENSION_KHR_FENCE_SYNC)
	"GL_OES_egl_sync "
#endif

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)
    "GL_IMG_vertex_array_object"
#endif

	;


/***********************************************************************************
 Function Name      : Clamp[f/i]
 Inputs             : [f/i32]Val, [f/i32]Min, [f/i32]Max
 Outputs            : -
 Returns            : Clamped [float/int] in range min, max
 Description        : UTILITY: Clamps a float/int the range min, max.
************************************************************************************/
IMG_INTERNAL IMG_FLOAT Clampf(IMG_FLOAT fVal, IMG_FLOAT fMin, IMG_FLOAT fMax)
{
	if (fVal < fMin)
	{
		return fMin;
	}
	else if (fVal > fMax)
	{
		return fMax;
	}
	else
	{
		return fVal;
	}
}


IMG_INTERNAL IMG_INT32 Clampi(IMG_INT32 i32Val, IMG_INT32 i32Min, IMG_INT32 i32Max)
{
	if (i32Val < i32Min)
	{
		return i32Min;
	}
	else if (i32Val > i32Max)
	{
		return i32Max;
	}
	else
	{
		return i32Val;
	}
}


/***********************************************************************************
 Function Name      : FloatToHWByte
 Inputs             : fValue
 Outputs            : -
 Returns            : Byte
 Description        : Converts float to byte in the same way that the HW does
************************************************************************************/
IMG_INTERNAL IMG_UINT32 FloatToHWByte(IMG_FLOAT fValue)
{
	IMG_UINT32 b, ui32Exp, ui32Mantissa, ui32Shift;
	GLES1_FUINT32 x;

	x.fVal = fValue;

	if ((x.ui32Val & 0x7fffffff) == 0x7fffffff)
	{
		return 255;
	}
	else if (x.fVal < 0.0f)
	{
		return 0;
	}
	else if (x.fVal >= 1.0f)
	{
		return 255;
	}

	ui32Exp = ((x.ui32Val & 0x7F800000) >> 23);
	ui32Shift = (127 - ui32Exp);
	ui32Mantissa = (x.ui32Val & 0x007FFFFF);

	/* hw has 14 bits mantissa after shift. */
	ui32Mantissa = ((ui32Mantissa >> ui32Shift) & 0xFFFFFE00);
	ui32Mantissa <<= ui32Shift;

	x.ui32Val = (x.ui32Val & 0xFF800000) | ui32Mantissa;
	x.fVal = x.fVal * 255.0f;

	/* PRQA S 3341 1 */ /* 0.5f can be exactly expressed in binary using IEEE float format */
	if ((x.fVal - GLES1_FLOORF( x.fVal )) == 0.5f)
	{
		/* round to even */
		x.fVal = (IMG_FLOAT)GLES1_FLOORF( x.fVal );

		if ((IMG_UINT32)x.fVal & 1)
		{
			x.fVal += 1.0f;
		}

		b = (IMG_UINT32)x.fVal;
	}
	else
	{
		b = (IMG_UINT32)GLES1_FLOORF( x.fVal + 0.5f);
	}

	if (b >= 256)
	{
		b = 255;
	}

	return b;
}


/***********************************************************************************
 Function Name      : ColorConvertToHWFormat
 Inputs             : psColor
 Outputs            : -
 Returns            : HW format color
 Description        : UTILITY: Converts a GLEScolor to a HW format color
************************************************************************************/
IMG_INTERNAL IMG_UINT32 ColorConvertToHWFormat(GLEScolor *psColor)
{
	IMG_UINT32 ui32Color;

#if defined(SGX_FEATURE_USE_VEC34)
	/* The hardware RGBA8 color format is A8 B8 G8 R8 */
	ui32Color =  ((IMG_UINT32)FloatToHWByte(psColor->fAlpha) << 24);

	ui32Color |= ((IMG_UINT32)FloatToHWByte(psColor->fBlue) << 16);

	ui32Color |= ((IMG_UINT32)FloatToHWByte(psColor->fGreen) << 8);

	ui32Color |=  (IMG_UINT32)FloatToHWByte(psColor->fRed);
#else
	/* The hardware RGBA8 color format is A8 R8 G8 B8 */
	ui32Color =  ((IMG_UINT32)FloatToHWByte(psColor->fAlpha) << 24);

	ui32Color |= ((IMG_UINT32)FloatToHWByte(psColor->fRed) << 16);

	ui32Color |= ((IMG_UINT32)FloatToHWByte(psColor->fGreen) << 8);

	ui32Color |=  (IMG_UINT32)FloatToHWByte(psColor->fBlue);
#endif
	return ui32Color;
}


/***********************************************************************************
 Function Name      : ColorConvertFromHWFormat
 Inputs             : psColor, ui32Color
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Converts a HW format color to GLEScolor.
************************************************************************************/
IMG_INTERNAL IMG_VOID ColorConvertFromHWFormat(GLEScolor *psColor, IMG_UINT32 ui32Color)
{
	IMG_FLOAT fTemp = 1.0f / 255.0f;

	psColor->fBlue  = (IMG_FLOAT)( ui32Color & 0xFF) * fTemp;
	psColor->fGreen = (IMG_FLOAT)((ui32Color & 0xFF00) >> 8) * fTemp;
	psColor->fRed   = (IMG_FLOAT)((ui32Color & 0xFF0000) >> 16) * fTemp;
	psColor->fAlpha = (IMG_FLOAT)((ui32Color & 0xFF000000) >> 24) * fTemp;
}


/***********************************************************************************
 Function Name      : Normalize
 Inputs             : afVin
 Outputs            : afVout
 Returns            : Square of length of input vector.
 Description        : Normalises a vector and returns dot product of input vector 
					  with itself.
***********************************************************************************/
IMG_INTERNAL IMG_FLOAT Normalize(IMG_FLOAT afVout[3], const IMG_FLOAT afVin[3])
{
	IMG_FLOAT fLen;

	fLen = afVin[0]*afVin[0] + afVin[1]*afVin[1] + afVin[2]*afVin[2];
    
	if (fLen <= GLES1_Zero)
	{
		afVout[0] = GLES1_Zero;
		afVout[1] = GLES1_Zero;
		afVout[2] = GLES1_Zero;

		return fLen;
	}

	/* PRQA S 3341 1 */ /* 1.0f can be exactly expressed in binary using IEEE float format */
	if (fLen == 1.0f)
	{
		afVout[0] = afVin[0];
		afVout[1] = afVin[1];
		afVout[2] = afVin[2];

		return fLen;
	}
	else
	{
		IMG_FLOAT fRecip = ((IMG_FLOAT) 1.0) / GLES1_SQRTF(fLen);
		/*
		 * This code could calculate a reciprocal square root accurate to well over
		 * 16 bits using Newton-Raphson approximation.
		 */
		afVout[0] = afVin[0] * fRecip;
		afVout[1] = afVin[1] * fRecip;
		afVout[2] = afVin[2] * fRecip;

		return fLen;
	}
}


/***********************************************************************************
 Function Name      : FloorLog2
 Inputs             : ui32Val
 Outputs            : 
 Returns            : Floor(Log2(ui32Val))
 Description        : Computes the floor of the log base 2 of a unsigned integer - 
					  used mostly for computing log2(2^ui32Val).
***********************************************************************************/
IMG_INTERNAL IMG_UINT32 FloorLog2(IMG_UINT32 ui32Val)
{
	IMG_UINT32 ui32Ret = 0;

	while (ui32Val >>= 1)
	{
		ui32Ret++;
	}

	return ui32Ret;
}

/***********************************************************************************
 Function Name      : Floor
 Inputs             : fVal
 Outputs            : 
 Returns            : Floor(fVal)
 Description        : Computes the floor of a float value
***********************************************************************************/
IMG_INTERNAL IMG_FLOAT Floor(IMG_FLOAT fVal)
{
	IMG_FLOAT fRet;

	fRet = (IMG_FLOAT)((IMG_INT32)fVal);

	if (fVal < 0.0f)
	{
		fRet -= 1.0f;
	}

	return fRet;
}


/***********************************************************************************
 Function Name      : glFinish
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Indicates all current commands should complete 
					  before returning. In practice, is ignored when doublebuffered.
************************************************************************************/
GL_API void GL_APIENTRY glFinish(void)
{
	IMG_BOOL bNeedFinish;
	IMG_UINT32 ui32Flags = 0;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFinish"));

	GLES1_TIME_START(GLES1_TIMES_glFinish);

	bNeedFinish = IMG_FALSE;

	/*********** Default behaviour ***********/
	if(gc->psDrawParams->eDrawableType==EGL_DRAWABLETYPE_PIXMAP)
	{
		bNeedFinish = IMG_TRUE;
	}
#if defined(GLES1_EXTENSION_EGL_IMAGE)
	else if(gc->sFrameBuffer.psActiveFrameBuffer)
	{
		GLES1FrameBufferAttachable *psColorAttachment;

		psColorAttachment = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];
		
		if(psColorAttachment)
		{
			if(psColorAttachment->eAttachmentType==GL_TEXTURE)
			{
				GLESTexture *psTex = ((GLESMipMapLevel *)psColorAttachment)->psTex;
				
				if(psTex->psEGLImageSource || psTex->psEGLImageTarget)
				{
					bNeedFinish = IMG_TRUE;
				}
			}
			else
			{
				GLESRenderBuffer *psRenderBuffer = (GLESRenderBuffer *)psColorAttachment;

				if(psRenderBuffer->psEGLImageSource || psRenderBuffer->psEGLImageTarget)
				{
					bNeedFinish = IMG_TRUE;
				}
			}
		}
	}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

	/* Kick TA/3D, Wait 3D, as long as it is needed */
	if(bNeedFinish)
	{
		ui32Flags |= GLES1_SCHEDULE_HW_WAIT_FOR_3D;
		
		FlushAllUnflushedFBO(gc, IMG_TRUE);

		if(gc->psRenderSurface)
		{
			if (gc->psRenderSurface->bInFrame)
			{
				ui32Flags |= GLES1_SCHEDULE_HW_LAST_IN_SCENE;
			}

			ScheduleTA(gc, gc->psRenderSurface, ui32Flags);
		}
	}
	/* Kick + Wait TA / 3D according to apphint */
	else
	{
		/*********** Set Kick + Wait TA Flag ***********/
		if (gc->sAppHints.ui32FlushBehaviour == GLES1_HINT_KICK_TA)
		{
			ui32Flags |= GLES1_SCHEDULE_HW_WAIT_FOR_TA;
		}
		/*********** Set Kick + Wait 3D Flag ***********/
		else if (gc->sAppHints.ui32FlushBehaviour == GLES1_HINT_KICK_3D)
		{
			ui32Flags |= GLES1_SCHEDULE_HW_WAIT_FOR_3D;

			FlushAllUnflushedFBO(gc, IMG_TRUE);

			if (gc->psRenderSurface && gc->psRenderSurface->bInFrame)
			{
				ui32Flags |= GLES1_SCHEDULE_HW_LAST_IN_SCENE;
			}
		}

		if(gc->psRenderSurface)
		{
			ScheduleTA(gc, gc->psRenderSurface, ui32Flags);
		}
	}	


	GLES1_TIME_STOP(GLES1_TIMES_glFinish);
}


/***********************************************************************************
 Function Name      : glFlush
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Indicates all current commands should be flushed.
					  In practice, is ignored when doublebuffered.
************************************************************************************/
GL_API void GL_APIENTRY glFlush(void)
{
	IMG_UINT32 ui32KickFlags = 0;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFlush"));

	GLES1_TIME_START(GLES1_TIMES_glFlush);

	/*********** Kick 3D ***********/
	if (gc->sAppHints.ui32FlushBehaviour == GLES1_HINT_KICK_3D)
	{
		FlushAllUnflushedFBO(gc, IMG_FALSE);

		ui32KickFlags = GLES1_SCHEDULE_HW_LAST_IN_SCENE;
	}

	/*********** Kick TA ***********/
	if(gc->psRenderSurface)
	{
		ScheduleTA(gc, gc->psRenderSurface, ui32KickFlags);
	}

	GLES1_TIME_STOP(GLES1_TIMES_glFlush);
}


/***********************************************************************************
 Function Name      : glHint
 Inputs             : target, mode
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Indicates a performance hint. Currently ignored.
************************************************************************************/
GL_API void GL_APIENTRY glHint(GLenum target, GLenum mode)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glHint"));

	GLES1_TIME_START(GLES1_TIMES_glHint);

	switch (mode)
	{
		case GL_DONT_CARE:
		case GL_FASTEST:
		case GL_NICEST:
		{
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glHint);

			return;
		}
	}

	switch (target)
	{
		case GL_PERSPECTIVE_CORRECTION_HINT:
		case GL_POINT_SMOOTH_HINT:
		case GL_LINE_SMOOTH_HINT:
		case GL_FOG_HINT:
		{
			gc->sState.sHints.eHint[target - GL_PERSPECTIVE_CORRECTION_HINT] = mode;

			break;
		}
		case GL_GENERATE_MIPMAP_HINT:
		{
			gc->sState.sHints.eHint[GLES1_HINT_GENMIPMAP] = mode;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glHint);

			return;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glHint);
}

#if defined(GLES1_EXTENSION_QUERY_MATRIX)
/***********************************************************************************
 Function Name      : ConvertToFixedWithMantissa
 Inputs             : fVal
 Outputs            : pi32Mantissa, pi32Exponent
 Returns            : Invalidity
 Description        : UTILITY: Converts a float into a fixed point mantissa and exponent
************************************************************************************/
static IMG_BOOL ConvertToFixedWithMantissa(IMG_FLOAT fVal, GLfixed *pi32Mantissa, GLint *pi32Exponent)
{
	IMG_INT32 i32Exp;
	IMG_FLOAT fMantissa;

	fMantissa = GLES1_FREXP(fVal, &i32Exp);
	fMantissa = fMantissa * 2147483648.0f; /* (ie shift by 31) */

	*pi32Mantissa = (GLint) fMantissa;
	*pi32Exponent = i32Exp - 15;

	/* PRQA S 3341 1 */ /* this can only fail on NaN. */
	if(fMantissa != fMantissa)
	{
		return IMG_TRUE;		/* fail on NaN, also could use isNan(mantissa) */
	}							/* should also fail on inf or failure of frexp */

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : glQueryMatrixxOES
 Inputs             : mantissa, exponent
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Queries the current matrix in fixed form
************************************************************************************/
GL_API_EXT GLbitfield GL_APIENTRY glQueryMatrixxOES(GLfixed mantissa[16], GLint exponent[16])
{
	GLESMatrix *psMatrix;
	IMG_UINT32 i;
	IMG_BOOL bInvalid;
	IMG_UINT32 ui32Invalid;
	IMG_FLOAT *pfMatrix;

	__GLES1_GET_CONTEXT_RETURN(0);

	PVR_DPF((PVR_DBG_CALLTRACE,"glQueryMatrixxOES"));

	GLES1_TIME_START(GLES1_TIMES_glQueryMatrixx);

	switch (gc->sState.eMatrixMode) 
	{
		case GL_MODELVIEW:
		default:
		{
			psMatrix = &gc->sTransform.psModelView->sMatrix;

			break;
		}
		case GL_PROJECTION:
		{
			psMatrix = &gc->sTransform.psProjection->sMatrix;

			break;
		}
		case GL_TEXTURE:
		{
			psMatrix = &gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture]->sMatrix;

			break;
		}
	}

	pfMatrix = &psMatrix->afMatrix[0][0];
	ui32Invalid = 0;

	for(i=0; i< 16; i++)
	{
		bInvalid = ConvertToFixedWithMantissa(pfMatrix[i], &mantissa[i], &exponent[i]);

		ui32Invalid |= (((IMG_UINT32)bInvalid) << i);
	}

	GLES1_TIME_STOP(GLES1_TIMES_glQueryMatrixx);

	return ui32Invalid;
}
#endif /* QUERY MATRIX */


/***********************************************************************************
 Function Name      : SetError
 Inputs             : gc, code
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets current error with code.
************************************************************************************/
IMG_INTERNAL IMG_VOID SetErrorFileLine(GLES1Context *gc, GLenum code, const IMG_CHAR *szFile, int iLine)
{
	/* Either PVR_DPF or PVR_DBG_CALLTRACE might be compiled out */
	PVR_UNREFERENCED_PARAMETER(szFile);
	PVR_UNREFERENCED_PARAMETER(iLine);

	PVR_DPF((PVR_DBG_CALLTRACE, "SetError: %s:%d set error code to 0x%x", szFile, iLine, code));

	if (!gc->i32Error) 
	{
		gc->i32Error = (IMG_INT32)code;
	}
}



/***********************************************************************************
 Function Name      : GetApplicationHints
 Inputs             : -
 Outputs            : psAppHints
 Returns            : -
 Description        : UTILITY: Gets application hints.
************************************************************************************/
IMG_INTERNAL IMG_BOOL GetApplicationHints(GLESAppHints *psAppHints, EGLcontextMode *psMode)
{
	IMG_UINT32 ui32Default;
	IMG_VOID* pvHintState;
	IMG_BOOL bDisableHWTQ = IMG_FALSE;

	PVRSRVCreateAppHintState(IMG_OPENGLES1, 0, &pvHintState);

	ui32Default = EXTERNAL_ZBUFFER_MODE_DEFAULT;
	PVRSRVGetAppHint(pvHintState, "ExternalZBufferMode", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32ExternalZBufferMode);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "FBODepthDiscard", IMG_UINT_TYPE, &ui32Default, &psAppHints->bFBODepthDiscard);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "OptimisedValidation", IMG_UINT_TYPE, &ui32Default, &psAppHints->bOptimisedValidation);
	
	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DisableHWTQTextureUpload", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDisableHWTQTextureUpload);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DisableHWTQNormalBlit", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDisableHWTQNormalBlit);		

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DisableHWTQMipGen", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDisableHWTQMipGen);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DisableHWTextureUpload", IMG_UINT_TYPE, &ui32Default, &bDisableHWTQ);

	if(bDisableHWTQ)
	{
		psAppHints->bDisableHWTQTextureUpload = IMG_TRUE;
		psAppHints->bDisableHWTQNormalBlit = IMG_TRUE;
		psAppHints->bDisableHWTQMipGen = IMG_TRUE;
	}


	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "FlushBehaviour", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32FlushBehaviour);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "EnableStaticPDSVertex", IMG_UINT_TYPE, &ui32Default, &psAppHints->bEnableStaticPDSVertex);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "EnableStaticMTECopy", IMG_UINT_TYPE, &ui32Default, &psAppHints->bEnableStaticMTECopy);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DisableStaticPDSPixelSAProgram", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDisableStaticPDSPixelSAProgram);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DisableUSEASMOPT", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDisableUSEASMOPT);

#if defined(FIX_HW_BRN_25211)
	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "UseC10Colours", IMG_UINT_TYPE, &ui32Default, &psAppHints->bUseC10Colours);
#endif /* defined(FIX_HW_BRN_25211) */

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DumpShaders", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDumpShaders);

	ui32Default = 200*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultVertexBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultVertexBufferSize);

	ui32Default = 800*1024;
	PVRSRVGetAppHint(pvHintState, "MaxVertexBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32MaxVertexBufferSize);

	ui32Default = 200*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultIndexBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultIndexBufferSize);

	ui32Default = 50*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultPDSVertBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultPDSVertBufferSize);

	ui32Default = 80*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultPregenPDSVertBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultPregenPDSVertBufferSize);

	ui32Default = 50*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultPregenMTECopyBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultPregenMTECopyBufferSize);

	ui32Default = 20*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultVDMBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultVDMBufferSize);

#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)
	/* 
	 * High priority contexts have no extra TA splitting by default. Other contexts use heuristics based on number of primitives to try
	 * to generate a certain number of TA kicks per frame.
	 */
	if(psMode->eContextPriority == SGX_CONTEXT_PRIORITY_HIGH)
	{
		ui32Default = 0;
	}
	else
	{
		ui32Default = 4;
	}

	PVRSRVGetAppHint(pvHintState, "KickTAMode", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32KickTAMode);

	ui32Default = 3;
	PVRSRVGetAppHint(pvHintState, "KickTAThreshold", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32KickTAThreshold);
#else
	PVR_UNREFERENCED_PARAMETER(psMode);
#endif

#if defined (TIMING) || defined (DEBUG)
	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DumpProfileData", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDumpProfileData);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "ProfileStartFrame", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32ProfileStartFrame);

	ui32Default = 0xFFFFFFFF;
	PVRSRVGetAppHint(pvHintState, "ProfileEndFrame", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32ProfileEndFrame);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "EnableMemorySpeedTest", IMG_UINT_TYPE, &ui32Default, &psAppHints->bEnableMemorySpeedTest);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DisableMetricsOutput", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDisableMetricsOutput);

#endif /* (TIMING) || (DEBUG) */

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "EnableAppTextureDependency", IMG_UINT_TYPE, &ui32Default, &psAppHints->bEnableAppTextureDependency);

	ui32Default = 4 * 1024;
	PVRSRVGetAppHint(pvHintState, "OGLES1UNCTexHeapSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32OGLES1UNCTexHeapSize);

	ui32Default = 256 * 1024;
	PVRSRVGetAppHint(pvHintState, "OGLES1CDRAMTexHeapSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32OGLES1CDRAMTexHeapSize);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "OGLES1EnableUNCAutoExtend", IMG_UINT_TYPE, &ui32Default, &psAppHints->bOGLES1EnableUNCAutoExtend);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "OGLES1EnableCDRAMAutoExtend", IMG_UINT_TYPE, &ui32Default, &psAppHints->bOGLES1EnableCDRAMAutoExtend);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "OGLES1SwTexOpThreadNum", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32OGLES1SwTexOpThreadNum);

	ui32Default = 70;
	PVRSRVGetAppHint(pvHintState, "OGLES1SwTexOpThreadPriority", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32OGLES1SwTexOpThreadPriority);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "OGLES1SwTexOpThreadAffinity", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32OGLES1SwTexOpThreadAffinity);

	ui32Default = 16;
	PVRSRVGetAppHint(pvHintState, "OGLES1SwTexOpMaxUltNum", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32OGLES1SwTexOpMaxUltNum);

	ui32Default = 10000000;
	PVRSRVGetAppHint(pvHintState, "OGLES1SwTexOpCleanupDelay", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32OGLES1SwTexOpCleanupDelay);

	PVRSRVFreeAppHintState(IMG_OPENGLES1, pvHintState);

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : GetExternalExtensionString
 Inputs             : -
 Outputs            : -
 Returns            : string
 Description        : UTILITY: It returns a string representing extension names
************************************************************************************/
static const IMG_CHAR * GetExternalExtensionString(IMG_VOID)
{
#if 0
	return pszExternalExtensions;
#endif
	return 0;
}


/***********************************************************************************
 Function Name      : BuildExtensionString
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : UTILITY: It builds an extension string from from different sources
************************************************************************************/
IMG_INTERNAL IMG_BOOL BuildExtensionString(GLES1Context *gc)
{
	const IMG_CHAR *pszExternalExtensionString;

	PVR_DPF((PVR_DBG_VERBOSE, "BuildExtensionString"));

	/* Any other companies extensions */
	pszExternalExtensionString = GetExternalExtensionString();

	if(!pszExternalExtensionString)
	{
		/* No extra strings - just return our inbuilt string */
		gc->pszExtensions = (IMG_CHAR *)&gszExtensions[0];	/* PRQA S 0311 */ /* Safe const cast here. */
	}
	else
	{
		IMG_UINT32 ui32Length;

		/* Add 2 for an extra space and a terminator */
		ui32Length = sizeof(gszExtensions) + (2 * sizeof(IMG_CHAR)); 
		ui32Length += strlen(pszExternalExtensionString) * sizeof(IMG_CHAR);

		gc->pszExtensions = (IMG_CHAR*)GLES1Calloc(gc, ui32Length);

		if(!gc->pszExtensions)
		{
			return IMG_FALSE;
		}

		strcat(gc->pszExtensions, &gszExtensions[0]);
		strcat(gc->pszExtensions, " ");
		strcat(gc->pszExtensions, pszExternalExtensionString);
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : DestroyExtensionString
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : UTILITY: It destroys the previously built extension string
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyExtensionString(GLES1Context *gc)
{
	if ((gc->pszExtensions) && (gc->pszExtensions != &gszExtensions[0]))
	{
		GLES1Free(IMG_NULL, gc->pszExtensions);
	}
}

#define GLES1_FLOAT_TO_INT_SCALE(val) ((IMG_INT32) GLES1_FLOORF( (val) * 2147482500.0f ))

/***********************************************************************************
 Function Name      : ConvertData
 Inputs             : ui32FromType, pvRawdata, ui32ToType, ui32Size
 Outputs            : pvResult
 Returns            : -
 Description        : UTILITY: Convert the data from one type to another.
************************************************************************************/
IMG_INTERNAL IMG_VOID ConvertData(IMG_UINT32 ui32FromType, const IMG_VOID *pvRawdata, 
								  IMG_UINT32 ui32ToType, IMG_VOID *pvResult, IMG_UINT32 ui32Size)
{
    IMG_UINT32 i;

    switch (ui32FromType) 
	{
		case GLES1_FLOAT:
		{
			switch (ui32ToType) 
			{
				case GLES1_FLOAT:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_FLOAT *)pvResult)[i] = ((const IMG_FLOAT *)pvRawdata)[i];
					}
					break;
				case GLES1_FIXED:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = FLOAT_TO_FIXED(((const IMG_FLOAT *)pvRawdata)[i]);
					}
					break;
				case GLES1_ENUM:
					for (i=0; i < ui32Size; i++) 
					{
						((GLenum *)pvResult)[i] = (GLenum)(((const IMG_FLOAT *)pvRawdata)[i]);
					}
					break;
				case GLES1_INT32:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = (IMG_INT32)(((const IMG_FLOAT *)pvRawdata)[i] >= 0.0 ?
							((const IMG_FLOAT *)pvRawdata)[i] + GLES1_Half:
							((const IMG_FLOAT *)pvRawdata)[i] - GLES1_Half);
					}
					break;
				case GLES1_BOOLEAN:
					for (i=0; i < ui32Size; i++) 
					{
						((GLboolean *)pvResult)[i] = (GLboolean)(((const IMG_FLOAT *)pvRawdata)[i] ? GL_TRUE : GL_FALSE);
					}
					break;
			}
			break;
		}
		case GLES1_COLOR:
		{
			switch (ui32ToType) 
			{
				case GLES1_FLOAT:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_FLOAT *)pvResult)[i] = ((const IMG_FLOAT *)pvRawdata)[i];
					}
					break;
				case GLES1_FIXED:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = FLOAT_TO_FIXED(((const IMG_FLOAT *)pvRawdata)[i]);
					}
					break;
				case GLES1_INT32:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = GLES1_FLOAT_TO_INT_SCALE( ((const IMG_FLOAT *)pvRawdata)[i] );
					}
					break;
				case GLES1_BOOLEAN:
					for (i=0; i < ui32Size; i++) 
					{
						((GLboolean *)pvResult)[i] = (GLboolean)(((const IMG_FLOAT *)pvRawdata)[i] ? GL_TRUE : GL_FALSE);
					}
					break;
			}
			break;
		}
		case GLES1_INT32:
		case GLES1_ENUM:
		{
			switch (ui32ToType) 
			{
				case GLES1_FLOAT:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_FLOAT *)pvResult)[i] = (IMG_FLOAT) (((const IMG_INT32 *)pvRawdata)[i]);
					}
					break;
				case GLES1_FIXED:
					if(ui32FromType == GLES1_INT32)
					{
						for (i=0; i < ui32Size; i++) 
						{
							((IMG_INT32 *)pvResult)[i] = LONG_TO_FIXED(((const IMG_INT32 *)pvRawdata)[i]);
						}
					}
					else
					{
						/* No conversion for enums */
						for (i=0; i < ui32Size; i++) 
						{
							((IMG_INT32 *)pvResult)[i] = ((const IMG_INT32 *)pvRawdata)[i];
						}
					}
					break;
				case GLES1_ENUM:
					for (i=0; i < ui32Size; i++) 
					{
						((GLenum *)pvResult)[i] = (GLenum)(((const IMG_INT32 *)pvRawdata)[i]);
					}
					break;
				case GLES1_INT32:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = ((const IMG_INT32 *)pvRawdata)[i];
					}
					break;
				case GLES1_BOOLEAN:
					for (i=0; i < ui32Size; i++) 
					{
						((GLboolean *)pvResult)[i] = (GLboolean)(((const IMG_INT32 *)pvRawdata)[i] ? GL_TRUE : GL_FALSE);
					}
					break;
			}
			break;
		}
		case GLES1_BOOLEAN:
		{
			switch (ui32ToType) 
			{
				case GLES1_FLOAT:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_FLOAT *)pvResult)[i] = (IMG_FLOAT)(((const GLboolean *)pvRawdata)[i]);
					}
					break;
				case GLES1_FIXED:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = LONG_TO_FIXED(((const GLboolean *)pvRawdata)[i]);
					}
					break;
				case GLES1_INT32:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = (IMG_INT32)(((const GLboolean *)pvRawdata)[i]);
					}
					break;
				case GLES1_BOOLEAN:
					for (i=0; i < ui32Size; i++) 
					{
						((GLboolean *)pvResult)[i] = (GLboolean)(((const GLboolean *)pvRawdata)[i] ? GL_TRUE : GL_FALSE);
					}
					break;
			}
			break;
		}
		case GLES1_FIXED:
		{
			switch (ui32ToType) 
			{
				case GLES1_FLOAT:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_FLOAT *)pvResult)[i] = FIXED_TO_FLOAT(((const GLfixed *)pvRawdata)[i]);
					}
					break;
				case GLES1_ENUM:
					for (i=0; i < ui32Size; i++) 
					{
						((GLenum *)pvResult)[i] = (GLenum)(((const GLfixed *)pvRawdata)[i]);
					}
					break;
				case GLES1_INT32:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = FIXED_TO_LONG(((const GLfixed *)pvRawdata)[i]);
					}
					break;
				case GLES1_BOOLEAN:
					for (i=0; i < ui32Size; i++) 
					{
						((GLboolean *)pvResult)[i] = (GLboolean)(((const GLfixed *)pvRawdata)[i] ? GL_TRUE : GL_FALSE);
					}
					break;
			}
			break;
		}
    }
}

/******************************************************************************
 End of file (misc.c)
******************************************************************************/
