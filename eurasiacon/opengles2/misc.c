/******************************************************************************
 * Name         : misc.c
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited.
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
 *****************************************************************************/
#include <string.h>

#include "context.h"

/*****************************************************************************/


static const IMG_UINT32 ui32ExtensionFlag = /*
											GLES2_EXTENSION_FBO_RENDER_MIPMAP |
*/
											GLES2_EXTENSION_BIT_RGB8_RGBA8 |
											GLES2_EXTENSION_BIT_DEPTH24 |
											GLES2_EXTENSION_BIT_VERTEX_HALF_FLOAT |
											GLES2_EXTENSION_BIT_STANDARD_DERIVATIVES |
#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
											GLES2_EXTENSION_BIT_TEXTURE_STREAM |
#endif
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
											GLES2_EXTENSION_BIT_TEXTURE_FLOAT |
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
											GLES2_EXTENSION_BIT_TEXTURE_HALF_FLOAT |
#endif
#if defined(GLES2_EXTENSION_EGL_IMAGE)
											GLES2_EXTENSION_BIT_EGL_IMAGE |
#endif
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
											GLES2_EXTENSION_BIT_EGL_IMAGE_EXTERNAL |
#endif
#if defined(GLES2_EXTENSION_MULTI_DRAW_ARRAYS)
											GLES2_EXTENSION_BIT_DRAW_ARRAYS |
#endif
#if defined(GLES2_EXTENSION_NPOT)
											GLES2_EXTENSION_BIT_NPOT |
#endif
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
											GLES2_EXTENSION_BIT_REQUIRED_INTERNAL_FORMAT |
#endif
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
											GLES2_EXTENSION_BIT_DEPTH_TEXTURE |
#endif
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
											GLES2_EXTENSION_BIT_GET_PROGRAM_BINARY |
#endif
#if defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888)
											GLES2_EXTENSION_BIT_TEXTURE_FORMAT_BGRA8888 |
#endif
#if defined(GLES2_EXTENSION_READ_FORMAT)
											GLES2_EXTENSION_BIT_READ_FORMAT |
#endif
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
											GLES2_EXTENSION_BIT_PACKED_DEPTH_STENCIL |
#endif
#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)
                                            GLES2_EXTENSION_BIT_VERTEX_ARRAY_OBJECT |
#endif
#if defined(GLES2_EXTENSION_DISCARD_FRAMEBUFFER)
                                            GLES2_EXTENSION_BIT_DISCARD_FRAMEBUFFER |
#endif
#if defined(EGL_EXTENSION_KHR_FENCE_SYNC)
											GLES2_EXTENSION_BIT_EGL_SYNC |
#endif
#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
											GLES2_EXTENSION_BIT_MULTISAMPLED_RENDER_TO_TEX |
#endif
											GLES2_EXTENSION_BIT_ELEMENT_INDEX_UINT |
											GLES2_EXTENSION_BIT_MAPBUFFER |
											GLES2_EXTENSION_BIT_PVRTC	|
											GLES2_EXTENSION_BIT_ETC1	|
											GLES2_EXTENSION_BIT_FRAGMENT_PRECISION_HIGH |
											GLES2_EXTENSION_BIT_SHADER_TEXTURE_LOD |
											GLES2_EXTENSION_BIT_IMG_SHADER_BINARY;


/*****************************************************************************/

static const struct GLES2_Extension_TAG
{
	const IMG_CHAR *pszExtensionName;

	IMG_UINT32 ui32ExtensionFlag;

} GLES2Extension[] = 
{
	/*{	"GL_OES_fbo_render_mipmap ",			GLES2_EXTENSION_BIT_FBO_RENDER_MIPMAP		},*/
	{	"GL_OES_rgb8_rgba8 ",					GLES2_EXTENSION_BIT_RGB8_RGBA8				},
	{	"GL_OES_depth24 ",						GLES2_EXTENSION_BIT_DEPTH24					},
	{	"GL_OES_vertex_half_float ",			GLES2_EXTENSION_BIT_VERTEX_HALF_FLOAT		},
	{	"GL_OES_texture_float ",				GLES2_EXTENSION_BIT_TEXTURE_FLOAT			},
	{	"GL_OES_texture_half_float ",			GLES2_EXTENSION_BIT_TEXTURE_HALF_FLOAT		},
	{	"GL_OES_element_index_uint ",			GLES2_EXTENSION_BIT_ELEMENT_INDEX_UINT		},
	{	"GL_OES_mapbuffer ",					GLES2_EXTENSION_BIT_MAPBUFFER				},
	{	"GL_OES_fragment_precision_high ",		GLES2_EXTENSION_BIT_FRAGMENT_PRECISION_HIGH	},
	{	"GL_OES_compressed_ETC1_RGB8_texture ",	GLES2_EXTENSION_BIT_ETC1					},
	{	"GL_OES_EGL_image ",					GLES2_EXTENSION_BIT_EGL_IMAGE				},
	{	"GL_OES_EGL_image_external ",			GLES2_EXTENSION_BIT_EGL_IMAGE_EXTERNAL		},
	{	"GL_OES_required_internalformat ",		GLES2_EXTENSION_BIT_REQUIRED_INTERNAL_FORMAT},
	{	"GL_OES_depth_texture ",				GLES2_EXTENSION_BIT_DEPTH_TEXTURE			},
	{	"GL_OES_get_program_binary ",			GLES2_EXTENSION_BIT_GET_PROGRAM_BINARY		},
	{	"GL_OES_packed_depth_stencil ",			GLES2_EXTENSION_BIT_PACKED_DEPTH_STENCIL	},
	{	"GL_OES_standard_derivatives ",			GLES2_EXTENSION_BIT_STANDARD_DERIVATIVES	},
	{   "GL_OES_vertex_array_object ",          GLES2_EXTENSION_BIT_VERTEX_ARRAY_OBJECT     }, 
	{	"GL_OES_egl_sync ",						GLES2_EXTENSION_BIT_EGL_SYNC				},

	{	"GL_EXT_multi_draw_arrays ",			GLES2_EXTENSION_BIT_DRAW_ARRAYS				},
	{	"GL_EXT_texture_format_BGRA8888 ",		GLES2_EXTENSION_BIT_TEXTURE_FORMAT_BGRA8888	},
	{	"GL_EXT_discard_framebuffer ",			GLES2_EXTENSION_BIT_DISCARD_FRAMEBUFFER		},
	{	"GL_EXT_shader_texture_lod ",			GLES2_EXTENSION_BIT_SHADER_TEXTURE_LOD		},

	{	"GL_IMG_shader_binary ",				GLES2_EXTENSION_BIT_IMG_SHADER_BINARY		},
	{	"GL_IMG_texture_compression_pvrtc ",	GLES2_EXTENSION_BIT_PVRTC					},
	{	"GL_IMG_texture_stream2 ",				GLES2_EXTENSION_BIT_TEXTURE_STREAM			},
	{	"GL_IMG_texture_npot ",					GLES2_EXTENSION_BIT_NPOT					},
	{	"GL_IMG_texture_format_BGRA8888 ",		GLES2_EXTENSION_BIT_TEXTURE_FORMAT_BGRA8888	},
	{	"GL_IMG_read_format ",					GLES2_EXTENSION_BIT_READ_FORMAT				},
	{	"GL_IMG_program_binary ",				GLES2_EXTENSION_BIT_GET_PROGRAM_BINARY		},
	{	"GL_IMG_multisampled_render_to_texture",GLES2_EXTENSION_BIT_MULTISAMPLED_RENDER_TO_TEX	},
};

#define GLES2_MAX_IMG_EXTENSIONS (sizeof(GLES2Extension) / sizeof(struct GLES2_Extension_TAG))


typedef union
{
	IMG_FLOAT f;
	IMG_UINT32 l;

} flong;


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
static IMG_UINT32 FloatToHWByte(IMG_FLOAT fValue)
{
	IMG_UINT32 b, ui32Exp, ui32Mantissa, ui32Shift;
	flong x;

	x.f = fValue;

	if ((x.l & 0x7fffffff) == 0x7fffffff)
	{
		return 255;
	}
	else if (x.f < 0.0f)
	{
		return 0;
	}
	else if (x.f >= 1.0f)
	{
		return 255;
	}

	ui32Exp = ((x.l & 0x7F800000) >> 23);
	ui32Shift = (127 - ui32Exp);
	ui32Mantissa = (x.l & 0x007FFFFF);

	/* hw has 14 bits mantissa after shift. */
	ui32Mantissa = ((ui32Mantissa >> ui32Shift) & 0xFFFFFE00);
	ui32Mantissa <<= ui32Shift;

	x.l = (x.l & 0xFF800000) | ui32Mantissa;
	x.f = x.f * 255.0f;

	/* PRQA S 3341 1 */ /* 0.5f can be exactly expressed in binary using IEEE float format */
	if ((x.f - GLES_FLOORF( x.f )) == 0.5f)
	{
		/* round to even */
		x.f = (IMG_FLOAT)GLES_FLOORF( x.f );

		if ((IMG_UINT32)x.f & 1)
		{
			x.f += 1.0f;
		}

		b = (IMG_UINT32)x.f;
	}
	else
	{
		b = (IMG_UINT32)GLES_FLOORF( x.f + 0.5f);
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
 Description        : UTILITY: Converts a GLES2color to a HW format color
************************************************************************************/
IMG_INTERNAL IMG_UINT32 ColorConvertToHWFormat(GLES2color *psColor)
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
 Function Name      : GLES2ConvertFloatToF16
 Inputs             : fValue
 Outputs            : -
 Returns            : F16 value
 Description        : UTILITY: Convert a value in IEEE single precision format to 
					  16-bit floating point format.
************************************************************************************/
IMG_INTERNAL IMG_UINT16 GLES2ConvertFloatToF16(IMG_FLOAT fValue)
{
	IMG_UINT16 ui16Value;
	IMG_UINT32 ui32Exponent;
	IMG_UINT32 ui32Mantissa;

	/* PRQA S 3341 1 */ /* 0.0f can be exactly expressed in binary using IEEE float format */
	if (fValue == 0.0f)
	{
		return(0);
	}

	if (fValue < 0)
	{
		ui16Value = 0x8000;
		fValue = -fValue;
	}
	else
	{
		ui16Value = 0;
	}
	fValue = MIN(fValue, 131008); /* 2^15 * (2 - 1/1024) = maximum f16 value. */

	/* Extract the exponent and mantissa. */
	ui32Exponent = ((*((IMG_UINT32 *)&fValue)) >> 23) - 127 + 15;
	ui32Mantissa = (*((IMG_UINT32 *)&fValue)) & ((1 << 23) - 1);

	/* If the exponent is outside the supported range then denormalise the mantissa. */
	if ((IMG_INT32)ui32Exponent <= 0)
	{
		IMG_UINT32 ui32Shift;

		ui32Mantissa |= (1 << 23);

		ui32Exponent = ((*((IMG_UINT32 *)&fValue)) >> 23);

		ui32Shift = -14 + 127 - ui32Exponent;

		if (ui32Shift < 24)
		{
			ui32Mantissa >>= ui32Shift;
		}
		else
		{
			ui32Mantissa = 0;
		}
	}
	else
	{
		ui16Value = (IMG_UINT16)(ui16Value | ((ui32Exponent << 10) & 0x7C00));
		
	}

	ui16Value = (IMG_UINT16)(ui16Value | (((ui32Mantissa >> 13) << 0) & 0x03FF));
	

	/* Round to nearest. */
	if (ui32Mantissa & (1 << 12))
	{
		ui16Value++;
	}

	return(ui16Value);
}

/***********************************************************************************
 Function Name      : GLES2ConvertFloatToC10
 Inputs             : fValue
 Outputs            : -
 Returns            : 2.8 format number
 Description        : UTILITY: Converts iEEE float to 2.8 representation
************************************************************************************/
IMG_INTERNAL IMG_UINT16 GLES2ConvertFloatToC10(IMG_FLOAT fValue)
{
	GLES2_FUINT32 fUint32;
	IMG_UINT32 ui32Exponent;
	IMG_UINT32 ui32Mantissa;
	IMG_BOOL bSign;
	IMG_UINT32 ui32Shift, a, b;
	IMG_INT32 i32abs;

	fUint32.fVal = fValue;
	ui32Exponent = (fUint32.ui32Val & 0x7F800000) >> 23;
	ui32Mantissa = (fUint32.ui32Val & 0x007FFFFF) | 0x00800000;
	bSign = (fUint32.ui32Val & 0x80000000) ? IMG_TRUE : IMG_FALSE;

	/* NaN */
	if((ui32Exponent == 255) && (ui32Mantissa > 0))
		return 0;

	if(ui32Exponent & 0x00000080)
	{
		i32abs = 510;

		if(bSign)
			i32abs = -i32abs;

		return (IMG_UINT16)((*((IMG_UINT32 *)(&i32abs))) & 0x000003FF);
	}
	else if((fUint32.ui32Val & 0x3FFFFFFF) < 0x3B008088)
	{
		return 0;
	}
	else if(ui32Exponent < 0x77)
	{
		i32abs = 1;

		if(bSign)
			i32abs = -i32abs;

		return (IMG_UINT16)((*((IMG_UINT32 *)(&i32abs))) & 0x000003FF);
	}

	ui32Shift = (ui32Exponent & 0xF) - 7;
	a = ui32Mantissa << ui32Shift;
	b = a - (a>>8) + (1 << 22);

	if(b & 0x007FFFFF)
		i32abs = (IMG_INT32)((b & 0xFF800000) >> 23);
	else 
		i32abs = (IMG_INT32)((b & 0xFF000000) >> 23);

	if(bSign)
		i32abs = -i32abs;

	return (IMG_UINT16)((*((IMG_UINT32 *)(&i32abs))) & 0x000003FF);
}

/***********************************************************************************
 Function Name      : GetExternalExtensionString
 Inputs             : -
 Outputs            : -
 Returns            : string
 Description        : UTILITY: It returns a string representing extension names
************************************************************************************/
static const IMG_CHAR *  GetExternalExtensionString(IMG_VOID)
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
IMG_INTERNAL IMG_BOOL BuildExtensionString(GLES2Context *gc)
{
	const IMG_CHAR *pszExternalExtensionString;
	IMG_UINT32 ui32Length, i;

	PVR_DPF((PVR_DBG_VERBOSE, "BuildExtensionString"));

	ui32Length = 1;

	/* First add the the IMG extensions */
	for(i = 0 ; i < GLES2_MAX_IMG_EXTENSIONS; i++)
	{
		if(GLES2Extension[i].ui32ExtensionFlag & ui32ExtensionFlag)
		{
			ui32Length += strlen(GLES2Extension[i].pszExtensionName);
		}
	}

	gc->pszExtensions = (IMG_CHAR*) GLES2Calloc(gc, ui32Length * sizeof(IMG_CHAR));
	
	if(!gc->pszExtensions)
	{
		return IMG_FALSE;
	}

	/* First add the the IMG extensions */
	for(i = 0 ; i < GLES2_MAX_IMG_EXTENSIONS; i++)
	{
		if(GLES2Extension[i].ui32ExtensionFlag & ui32ExtensionFlag)
		{
			strcat(gc->pszExtensions, GLES2Extension[i].pszExtensionName);
		}
	}

	/* Then add customer extensions */
	pszExternalExtensionString = GetExternalExtensionString();
	
	if(pszExternalExtensionString)
	{
		IMG_CHAR *pszNewExtensions;
		ui32Length += strlen(pszExternalExtensionString) + 2;

		pszNewExtensions = (IMG_CHAR*)GLES2Realloc(gc, gc->pszExtensions, ui32Length * sizeof(IMG_CHAR));

		if(!pszNewExtensions)
		{
			/* Out of memory */
			return IMG_FALSE;	
		}
		gc->pszExtensions = pszNewExtensions;

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
IMG_INTERNAL IMG_VOID DestroyExtensionString(GLES2Context *gc)
{
	if(gc->pszExtensions)
	{
		GLES2Free(IMG_NULL, gc->pszExtensions);
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
	IMG_FLOAT  fDefault;
	IMG_VOID* pvHintState;
	IMG_BOOL bDisableHWTQ = IMG_FALSE;

	PVRSRVCreateAppHintState(IMG_OPENGLES2, 0, &pvHintState);

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
	PVRSRVGetAppHint(pvHintState, "EnableStaticMTECopy", IMG_UINT_TYPE, &ui32Default, &psAppHints->bEnableStaticMTECopy);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "AdjustShaderPrecision", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32AdjustShaderPrecision);
	
	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DumpCompilerLogFiles", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDumpCompilerLogFiles);

	ui32Default = 200*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultVertexBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultVertexBufferSize);

	ui32Default = 800*1024;
	PVRSRVGetAppHint(pvHintState, "MaxVertexBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32MaxVertexBufferSize);

	ui32Default = 200*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultIndexBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultIndexBufferSize);
	
	ui32Default = 50*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultPDSVertBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultPDSVertBufferSize);
	
	ui32Default = 20*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultVDMBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultVDMBufferSize);

	ui32Default = 50*1024;
	PVRSRVGetAppHint(pvHintState, "DefaultPregenMTECopyBufferSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32DefaultPregenMTECopyBufferSize);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "StrictBinaryVersionComparison", IMG_UINT_TYPE, &ui32Default, &psAppHints->bStrictBinaryVersionComparison);

	fDefault = 1.0f;
	PVRSRVGetAppHint(pvHintState, "PolygonUnitsMultiplier", IMG_FLOAT_TYPE, &fDefault, &psAppHints->fPolygonUnitsMultiplier);

	fDefault = 1.0f;
	PVRSRVGetAppHint(pvHintState, "PolygonFactorMultiplier", IMG_FLOAT_TYPE, &fDefault, &psAppHints->fPolygonFactorMultiplier);


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


#if defined(TIMING) || defined(DEBUG)
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

#if defined(DEBUG)
	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DumpUSPOutput", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDumpUSPOutput);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "DumpShaderAnalysis", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDumpShaderAnalysis);
	
	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "TrackUSCMemory", IMG_UINT_TYPE, &ui32Default, &psAppHints->bTrackUSCMemory);
#endif

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "OverloadTexLayout", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32OverloadTexLayout);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "InitialiseVSOutputs", IMG_UINT_TYPE, &ui32Default, &psAppHints->bInitialiseVSOutputs);

#if defined(EUR_CR_ISP_TAGCTRL_TRIANGLE_FRAGMENT_PIXELS_MASK)
	ui32Default = EURASIA_DEFAULT_TAG_TRIANGLE_SPLIT;
	PVRSRVGetAppHint(pvHintState, "TriangleSplitThreshold", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32TriangleSplitPixelThreshold);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "DynamicSplitCalc", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDynamicSplitCalc);
#endif
	
	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "AllowTrilinearNPOT", IMG_UINT_TYPE, &ui32Default, &psAppHints->bAllowTrilinearNPOT);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "EnableVaryingPrecisionOpt", IMG_UINT_TYPE, &ui32Default, &psAppHints->bEnableVaryingPrecisionOpt);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "EnableAppTextureDependency", IMG_UINT_TYPE, &ui32Default, &psAppHints->bEnableAppTextureDependency);

	ui32Default = 4 * 1024;
	PVRSRVGetAppHint(pvHintState, "UNCTexHeapSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32UNCTexHeapSize);

	ui32Default = 256 * 1024;
	PVRSRVGetAppHint(pvHintState, "CDRAMTexHeapSize", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32CDRAMTexHeapSize);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "EnableUNCAutoExtend", IMG_UINT_TYPE, &ui32Default, &psAppHints->bEnableUNCAutoExtend);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "EnableCDRAMAutoExtend", IMG_UINT_TYPE, &ui32Default, &psAppHints->bEnableCDRAMAutoExtend);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "SwTexOpThreadNum", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32SwTexOpThreadNum);

	ui32Default = 70;
	PVRSRVGetAppHint(pvHintState, "SwTexOpThreadPriority", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32SwTexOpThreadPriority);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "SwTexOpThreadAffinity", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32SwTexOpThreadAffinity);

	ui32Default = 256;
	PVRSRVGetAppHint(pvHintState, "SwTexOpMaxUltNum", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32SwTexOpMaxUltNum);

	ui32Default = 10000000;
	PVRSRVGetAppHint(pvHintState, "SwTexOpCleanupDelay", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32SwTexOpCleanupDelay);

	ui32Default = 1;
	PVRSRVGetAppHint(pvHintState, "DisableAsyncTextureOp", IMG_UINT_TYPE, &ui32Default, &psAppHints->bDisableAsyncTextureOp);

	ui32Default = 1000;
	PVRSRVGetAppHint(pvHintState, "PrimitiveSplitThreshold", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32PrimitiveSplitThreshold);

	ui32Default = 0;
	PVRSRVGetAppHint(pvHintState, "MaxDrawCallsPerCore", IMG_UINT_TYPE, &ui32Default, &psAppHints->ui32MaxDrawCallsPerCore);

	PVRSRVFreeAppHintState(IMG_OPENGLES2, pvHintState);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : SetError
 Inputs             : gc, code
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets current error with code.
************************************************************************************/
IMG_INTERNAL IMG_VOID SetErrorFileLine(GLES2Context *gc, GLenum code, const IMG_CHAR *szFile, int iLine)
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
 Function Name      : glFinish
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : 
************************************************************************************/
GL_APICALL void GL_APIENTRY glFinish(void)
{
	IMG_BOOL bNeedFinish;
	IMG_UINT32 ui32Flags = 0;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFinish"));

	GLES2_TIME_START(GLES2_TIMES_glFinish);

	bNeedFinish = IMG_FALSE;

	/*********** Default behaviour ***********/
	if(gc->psDrawParams->eDrawableType==EGL_DRAWABLETYPE_PIXMAP)
	{
		bNeedFinish = IMG_TRUE;
	}
#if defined(GLES2_EXTENSION_EGL_IMAGE)
	else if(gc->sFrameBuffer.psActiveFrameBuffer)
	{
		GLES2FrameBufferAttachable *psColorAttachment;
			
		psColorAttachment = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES2_COLOR_ATTACHMENT];
		
		if(psColorAttachment)
		{
			if(psColorAttachment->eAttachmentType==GL_TEXTURE)
			{
				GLES2Texture *psTex = ((GLES2MipMapLevel *)psColorAttachment)->psTex;
					
				if(psTex->psEGLImageSource || psTex->psEGLImageTarget)
				{
					bNeedFinish = IMG_TRUE;
				}
			}
			else
			{
				GLES2RenderBuffer *psRenderBuffer = (GLES2RenderBuffer *)psColorAttachment;

				if(psRenderBuffer->psEGLImageSource || psRenderBuffer->psEGLImageTarget)
				{
					bNeedFinish = IMG_TRUE;
				}
			}
		}
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

	/* Kick TA/3D, Wait 3D, as long as it is needed */
	if(bNeedFinish)
	{
	    ui32Flags |= GLES2_SCHEDULE_HW_WAIT_FOR_3D;
		
		FlushAllUnflushedFBO(gc, IMG_TRUE);

		if(gc->psRenderSurface)
		{
			if (gc->psRenderSurface->bInFrame)
			{
				ui32Flags |= GLES2_SCHEDULE_HW_LAST_IN_SCENE;
			}

			ScheduleTA(gc, gc->psRenderSurface, ui32Flags);
		}
	}
	/*********** Kick + Wait TA / 3D according to apphint  ***********/
	else
	{
		/*********** Set Kick + Wait TA Flag ***********/
		if (gc->sAppHints.ui32FlushBehaviour == GLES2_HINT_KICK_TA)
		{
			ui32Flags |= GLES2_SCHEDULE_HW_WAIT_FOR_TA;
		}
		/*********** Set Kick + Wait 3D Flag ***********/
		else if (gc->sAppHints.ui32FlushBehaviour == GLES2_HINT_KICK_3D)
		{
			ui32Flags |= GLES2_SCHEDULE_HW_WAIT_FOR_3D;
			
			FlushAllUnflushedFBO(gc, IMG_TRUE);

			if (gc->psRenderSurface && gc->psRenderSurface->bInFrame)
			{
				ui32Flags |= GLES2_SCHEDULE_HW_LAST_IN_SCENE;
			}
		}

		if(gc->psRenderSurface)
		{
			ScheduleTA(gc, gc->psRenderSurface, ui32Flags);
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glFinish);
}


/***********************************************************************************
 Function Name      : glFlush
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : 
************************************************************************************/
GL_APICALL void GL_APIENTRY glFlush(void)
{
	IMG_UINT32 ui32KickFlags = 0;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFlush"));

	GLES2_TIME_START(GLES2_TIMES_glFlush);


	/*********** Kick 3D ***********/
	if (gc->sAppHints.ui32FlushBehaviour == GLES2_HINT_KICK_3D)
	{
		FlushAllUnflushedFBO(gc, IMG_FALSE);

		ui32KickFlags = GLES2_SCHEDULE_HW_LAST_IN_SCENE;
	}

	/*********** Kick TA ***********/
	if(gc->psRenderSurface)
	{
		ScheduleTA(gc, gc->psRenderSurface, ui32KickFlags);
	}

	GLES2_TIME_STOP(GLES2_TIMES_glFlush);
}


/***********************************************************************************
 Function Name      : glHint
 Inputs             : target, mode
 Outputs            : -
 Returns            : -
 Description        : 
************************************************************************************/
GL_APICALL void GL_APIENTRY glHint(GLenum target, GLenum mode)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glHint"));

	GLES2_TIME_START(GLES2_TIMES_glHint);

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

			GLES2_TIME_STOP(GLES2_TIMES_glHint);

			return;
		}
	}

	switch (target) 
	{
		case GL_GENERATE_MIPMAP_HINT:
		{
			gc->sState.sHints.eHint[GLES2_HINT_GENMIPMAP] = mode;

			break;
		}
		case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
		{
			gc->sState.sHints.eHint[GLES2_HINT_DERIVATIVE] = mode;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glHint);

			return;
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glHint);
}

#define GLES_FLOAT_TO_INT_SCALE(val) ((IMG_INT32) GLES_FLOORF( (val) * 2147482500.0f ))

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
		case GLES_FLOAT:
		{
			switch (ui32ToType) 
			{
				case GLES_FLOAT:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_FLOAT *)pvResult)[i] = ((const IMG_FLOAT *)pvRawdata)[i];
					}
					break;
				case GLES_FIXED:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = FLOAT_TO_FIXED(((const IMG_FLOAT *)pvRawdata)[i]);
					}
					break;
				case GLES_ENUM:
					for (i=0; i < ui32Size; i++) 
					{
						((GLenum *)pvResult)[i] = (GLenum)(((const IMG_FLOAT *)pvRawdata)[i]);
					}
					break;
				case GLES_INT32:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = (IMG_INT32)(((const IMG_FLOAT *)pvRawdata)[i] >= 0.0 ?
							((const IMG_FLOAT *)pvRawdata)[i] + GLES2_Half:
							((const IMG_FLOAT *)pvRawdata)[i] - GLES2_Half);
					}
					break;
				case GLES_BOOLEAN:
					for (i=0; i < ui32Size; i++) 
					{
						((GLboolean *)pvResult)[i] = (GLboolean)(((const IMG_FLOAT *)pvRawdata)[i] ? GL_TRUE : GL_FALSE);
					}
					break;
			}
			break;
		}
		case GLES_COLOR:
		{
			switch (ui32ToType) 
			{
				case GLES_FLOAT:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_FLOAT *)pvResult)[i] = ((const IMG_FLOAT *)pvRawdata)[i];
					}
					break;
				case GLES_FIXED:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = FLOAT_TO_FIXED(((const IMG_FLOAT *)pvRawdata)[i]);
					}
					break;
				case GLES_INT32:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = GLES_FLOAT_TO_INT_SCALE( ((const IMG_FLOAT *)pvRawdata)[i] );
					}
					break;
				case GLES_BOOLEAN:
					for (i=0; i < ui32Size; i++) 
					{
						((GLboolean *)pvResult)[i] = (GLboolean)(((const IMG_FLOAT *)pvRawdata)[i] ? GL_TRUE : GL_FALSE);
					}
					break;
			}
			break;
		}
		case GLES_INT32:
		case GLES_ENUM:
		{
			switch (ui32ToType) 
			{
				case GLES_FLOAT:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_FLOAT *)pvResult)[i] = (IMG_FLOAT) (((const IMG_INT32 *)pvRawdata)[i]);
					}
					break;
				case GLES_FIXED:
					if(ui32FromType == GLES_INT32)
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
				case GLES_ENUM:
					for (i=0; i < ui32Size; i++) 
					{
						((GLenum *)pvResult)[i] = (GLenum)(((const IMG_INT32 *)pvRawdata)[i]);
					}
					break;
				case GLES_INT32:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = ((const IMG_INT32 *)pvRawdata)[i];
					}
					break;
				case GLES_BOOLEAN:
					for (i=0; i < ui32Size; i++) 
					{
						((GLboolean *)pvResult)[i] = (GLboolean)(((const IMG_INT32 *)pvRawdata)[i] ? GL_TRUE : GL_FALSE);
					}
					break;
			}
			break;
		}
		case GLES_BOOLEAN:
		{
			switch (ui32ToType) 
			{
				case GLES_FLOAT:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_FLOAT *)pvResult)[i] = (IMG_FLOAT)(((const GLboolean *)pvRawdata)[i]);
					}
					break;
				case GLES_FIXED:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = LONG_TO_FIXED(((const GLboolean *)pvRawdata)[i]);
					}
					break;
				case GLES_INT32:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = (IMG_INT32)(((const GLboolean *)pvRawdata)[i]);
					}
					break;
				case GLES_BOOLEAN:
					for (i=0; i < ui32Size; i++) 
					{
						((GLboolean *)pvResult)[i] = (GLboolean)(((const GLboolean *)pvRawdata)[i] ? GL_TRUE : GL_FALSE);
					}
					break;
			}
			break;
		}
		case GLES_FIXED:
		{
			switch (ui32ToType) 
			{
				case GLES_FLOAT:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_FLOAT *)pvResult)[i] = FIXED_TO_FLOAT(((const GLfixed *)pvRawdata)[i]);
					}
					break;
				case GLES_ENUM:
					for (i=0; i < ui32Size; i++) 
					{
						((GLenum *)pvResult)[i] = (GLenum)(((const GLfixed *)pvRawdata)[i]);
					}
					break;
				case GLES_INT32:
					for (i=0; i < ui32Size; i++) 
					{
						((IMG_INT32 *)pvResult)[i] = FIXED_TO_LONG(((const GLfixed *)pvRawdata)[i]);
					}
					break;
				case GLES_BOOLEAN:
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

