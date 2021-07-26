/******************************************************************************
 * Name         : get.c
 *
 * Copyright    : 2004-2006 by Imagination Technologies Limited.
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
 * Description	: Dynamic gets
 *
 * $Log: get.c $
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/
#include "context.h"

extern const IMG_OGLES1EGL_Interface sGLES1FunctionTable;

static const IMG_CHAR * const pszVendor = "Imagination Technologies";

#if defined(SGX535_V1_1) || defined(SGX535)
	IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 535";
#else
	#if defined(SGX530)
		IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 530";
	#else
		#if defined(SGX520)
			IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 520";
		#else
			#if defined(SGX540)
				IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 540";
			#else
				#if defined(SGX531)
					IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 531";
				#else
                    #if defined(SGX545)
                        IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 545";
					#else
						#if defined(SGX541)
                        	IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 541";
                    	#else						
							#if defined(SGX543)
                        		IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 543MP";
                    		#else						
 								#if defined(SGX544)
									#if defined(SGX_FEATURE_MP)
	                        			IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 544MP";
									#else
										IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 544";
									#endif
                    			#else
 									#if defined(SGX554)
										#if defined(SGX_FEATURE_MP)
	                        				IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 554MP";
										#else
											IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 554";
										#endif
                    				#else
										#error ("HW not defined")
									#endif
								#endif
							#endif
						#endif
				    #endif
				#endif
			#endif			
		#endif
	#endif
#endif


#if defined(PROFILE_COMMON)

	static const IMG_CHAR * const pszVersion = "OpenGL ES-CM 1.1";

#else /* PROFILE_COMMON */

	static const IMG_CHAR * const pszVersion = "OpenGL ES-CL 1.1";

#endif


static GLboolean IsEnabled(GLES1Context *gc, GLenum eCap);

/***********************************************************************************
 Function Name      : GetBlendEquation
 Inputs             : gc, ui32Shift, ui32Mask
 Outputs            : Blend equation
 Returns            : -
 Description        : UTILITY: Gets a blend factor from current state
************************************************************************************/
static IMG_UINT32 GetBlendEquation(GLES1Context *gc, IMG_UINT32 ui32Shift, IMG_UINT32 ui32Mask)
{
	IMG_UINT32 ui32BlendEquation = (gc->sState.sRaster.ui32BlendEquation & ui32Mask) >> ui32Shift;

	switch(ui32BlendEquation)
	{
		default:
		case GLES1_BLENDMODE_ADD:
			return GL_FUNC_ADD_OES;
			
		case GLES1_BLENDMODE_SUBTRACT:
			return GL_FUNC_SUBTRACT_OES;
			
		case GLES1_BLENDMODE_REVSUBTRACT:
			return GL_FUNC_REVERSE_SUBTRACT_OES;
	}
}


/***********************************************************************************
 Function Name      : GetBlendFactor
 Inputs             : gc, ui32Shift, ui32Mask
 Outputs            : Blend factor
 Returns            : -
 Description        : UTILITY: Gets a blend factor from current state
************************************************************************************/
static IMG_UINT32 GetBlendFactor(GLES1Context *gc, IMG_UINT32 ui32Shift, IMG_UINT32 ui32Mask)
{
	IMG_UINT32 ui32BlendFactor = (gc->sState.sRaster.ui32BlendFunction & ui32Mask) >> ui32Shift;

	switch(ui32BlendFactor)
	{
		default:
		case GLES1_BLENDFACTOR_ZERO:
		{
			return GL_ZERO;
		}
		case GLES1_BLENDFACTOR_ONE:
		{
			return GL_ONE;
		}
		case GLES1_BLENDFACTOR_DSTCOLOR:
		{
			return GL_DST_COLOR;
		}
		case GLES1_BLENDFACTOR_ONEMINUS_DSTCOLOR:
		{
			return GL_ONE_MINUS_DST_COLOR;
		}
		case GLES1_BLENDFACTOR_SRCCOLOR:
		{
			return GL_SRC_COLOR;
		}
		case GLES1_BLENDFACTOR_ONEMINUS_SRCCOLOR:
		{
			return GL_ONE_MINUS_SRC_COLOR;
		}
		case GLES1_BLENDFACTOR_SRCALPHA:
		{
			return GL_SRC_ALPHA;
		}
		case GLES1_BLENDFACTOR_ONEMINUS_SRCALPHA:
		{
			return GL_ONE_MINUS_SRC_ALPHA;
		}
		case GLES1_BLENDFACTOR_DSTALPHA:
		{
			return GL_DST_ALPHA;
		}
		case GLES1_BLENDFACTOR_ONEMINUS_DSTALPHA:
		{
			return GL_ONE_MINUS_DST_ALPHA;
		}
		case GLES1_BLENDFACTOR_SRCALPHA_SATURATE:
		{
			return GL_SRC_ALPHA_SATURATE;
		}
	}
}


/***********************************************************************************
 Function Name      : GetStencilOp
 Inputs             : ui32Stencilword, ui32Shift, ui32Mask
 Outputs            : Stencil op
 Returns            : -
 Description        : UTILITY: Gets a stencil op from current state
************************************************************************************/
static IMG_UINT32 GetStencilOp(IMG_UINT32 ui32StencilWord, IMG_UINT32 ui32Shift, IMG_UINT32 ui32Mask)
{
	IMG_UINT32 ui32StencilOp = (ui32StencilWord & ui32Mask) >> ui32Shift;

	switch (ui32StencilOp) 
	{
		case EURASIA_ISPC_SOP_KEEP:
		default:
		{
			return GL_KEEP;
		}
		case EURASIA_ISPC_SOP_ZERO:
		{
			return GL_ZERO;
		}
		case EURASIA_ISPC_SOP_REPLACE:
		{
			return GL_REPLACE;
		}
		case EURASIA_ISPC_SOP_INCSAT:
		{
			return GL_INCR;
		}
		case EURASIA_ISPC_SOP_DECSAT:
		{
			return GL_DECR;
		}
#if defined(GLES1_EXTENSION_STENCIL_WRAP)
		case EURASIA_ISPC_SOP_INC:
		{
			return GL_INCR_WRAP_OES;
		}
		case EURASIA_ISPC_SOP_DEC:
		{
			return GL_DECR_WRAP_OES;
		}
#endif /* defined(GLES1_EXTENSION_STENCIL_WRAP) */
		case EURASIA_ISPC_SOP_INVERT:
		{
			return GL_INVERT;
		}
	}
}


/***********************************************************************************
 Function Name      : DoGet
 Inputs             : gc, eQuery, ui32Type, pszProcName
 Outputs            : pvResult
 Returns            : -
 Description        : Returns some GL state and coverts to the type specified.
************************************************************************************/
static IMG_VOID DoGet(GLES1Context *gc, GLenum eQuery, IMG_VOID *pvResult, IMG_UINT32 ui32Type)
{
    GLint itemp[16], *ip = itemp;			  /* NOTE: for ints */
	IMG_UINT32 etemp[16], *ep = etemp;		  /* NOTE: for enums */
    IMG_FLOAT ftemp[16], *fp = ftemp;		  /* NOTE: for floats */
	IMG_FLOAT ctemp[16], *cp = ctemp;		  /* NOTE: for colors */
    GLboolean ltemp[16], *lp = ltemp;		  /* NOTE: for logicals */
    IMG_FLOAT *mp;


    /* BE CERTAIN TO:
	   get vertex attrib info from the current VAO instead of VAOMachine,
	   since the info was passed from VAO to VAOMachine only in draw call */

	GLES1VertexArrayObject        *psVAO;
	GLES1AttribArrayPointerState  *psAttribState;

	/* Setup VAO and attribute state */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(psVAO);


    switch (eQuery) 
	{
		case GL_ALPHA_TEST:
		case GL_BLEND:
		case GL_COLOR_MATERIAL:
		case GL_CULL_FACE:
		case GL_DEPTH_TEST:
		case GL_DITHER:
		case GL_FOG:
		case GL_LIGHTING:
		case GL_LINE_SMOOTH:
		case GL_COLOR_LOGIC_OP:
		case GL_NORMALIZE:
		case GL_POINT_SMOOTH:
		case GL_RESCALE_NORMAL:
		case GL_SCISSOR_TEST:
		case GL_STENCIL_TEST:
		case GL_TEXTURE_2D:
 		case GL_CLIP_PLANE0: 
		case GL_CLIP_PLANE1: case GL_CLIP_PLANE2: 
		case GL_CLIP_PLANE3: case GL_CLIP_PLANE4: case GL_CLIP_PLANE5:
		case GL_LIGHT0: case GL_LIGHT1:
		case GL_LIGHT2: case GL_LIGHT3:
		case GL_LIGHT4: case GL_LIGHT5:
		case GL_LIGHT6: case GL_LIGHT7:
		case GL_POLYGON_OFFSET_FILL:
		case GL_VERTEX_ARRAY:
		case GL_NORMAL_ARRAY:
		case GL_COLOR_ARRAY:
		case GL_TEXTURE_COORD_ARRAY:
		case GL_POINT_SIZE_ARRAY_OES:
		case GL_MULTISAMPLE:
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
		case GL_SAMPLE_ALPHA_TO_ONE:
		case GL_SAMPLE_COVERAGE:
		case GL_POINT_SPRITE_OES:
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_PALETTE_OES:
		case GL_MATRIX_INDEX_ARRAY_OES:
		case GL_WEIGHT_ARRAY_OES:
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_OES:
		case GL_TEXTURE_GEN_STR_OES:
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_STREAM_IMG:
#endif /* GLES1_EXTENSION_TEXTURE_STREAM */ 
			*lp++ = IsEnabled(gc, eQuery);
			break;
		case GL_ACTIVE_TEXTURE:
			*ep++ = gc->sState.sTexture.ui32ActiveTexture + GL_TEXTURE0;
			break;
		case GL_CLIENT_ACTIVE_TEXTURE:
			*ep++ = gc->sState.ui32ClientActiveTexture + GL_TEXTURE0;
			break;
		case GL_UNPACK_ALIGNMENT:
			*ip++ = (IMG_INT32)gc->sState.sClientPixel.ui32UnpackAlignment;
			break;
		case GL_PACK_ALIGNMENT:
			*ip++ = (IMG_INT32)gc->sState.sClientPixel.ui32PackAlignment;
			break;
		case GL_POINT_SIZE:
			*fp++ = gc->sState.sPoint.fRequestedSize;
			break;
		case GL_LINE_WIDTH:
			*fp++ = *gc->sState.sLine.pfLineWidth;
			break;
		case GL_POINT_SIZE_MIN:
			*fp++ = gc->sState.sPoint.fClampMin;
			break;
		case GL_POINT_SIZE_MAX:
			*fp++ = gc->sState.sPoint.fClampMax;
			break;
		case GL_POINT_FADE_THRESHOLD_SIZE:
			*fp++ = gc->sState.sPoint.fFade;
			break;
		case GL_POINT_DISTANCE_ATTENUATION:
			*fp++ = gc->sState.sPoint.afAttenuation[0];
			*fp++ = gc->sState.sPoint.afAttenuation[1];
			*fp++ = gc->sState.sPoint.afAttenuation[2];
			break;
		case GL_SAMPLE_COVERAGE_VALUE:
			*fp++ = gc->sState.sMultisample.fSampleCoverageValue;
			break;
		case GL_SAMPLE_COVERAGE_INVERT:
			*lp++ = (GLboolean)gc->sState.sMultisample.bSampleCoverageInvert;
			break;
		case GL_CULL_FACE_MODE:
			*ep++ = gc->sState.sPolygon.eCullMode;
			break;
		case GL_FRONT_FACE:
			*ep++ = gc->sState.sPolygon.eFrontFaceDirection;
			break;
		case GL_LIGHT_MODEL_TWO_SIDE:
			*lp++ = (GLboolean)gc->sState.sLight.sModel.bTwoSided;
			break;
		case GL_LIGHT_MODEL_AMBIENT:
			*cp++ = gc->sState.sLight.sModel.sAmbient.fRed;
			*cp++ = gc->sState.sLight.sModel.sAmbient.fGreen;
			*cp++ = gc->sState.sLight.sModel.sAmbient.fBlue;
			*cp++ = gc->sState.sLight.sModel.sAmbient.fAlpha;
			break;
		case GL_SHADE_MODEL:
		{
#if defined(SGX545)
			if (gc->sState.sShade.ui32ShadeModel == (EURASIA_MTE_SHADE_RESERVED))
#else
			if (gc->sState.sShade.ui32ShadeModel == (EURASIA_MTE_SHADE_GOURAUD))
#endif
			{
				*ep++ = GL_SMOOTH;
			}
			else
			{
				*ep++ = GL_FLAT;
			}
			break;
		}
		case GL_FOG_DENSITY:
			*fp++ = gc->sState.sFog.fDensity;
			break;
		case GL_FOG_START:
			*fp++ = gc->sState.sFog.fStart;
			break;
		case GL_FOG_END:
			*fp++ = gc->sState.sFog.fEnd;
			break;
		case GL_FOG_MODE:
			*ep++ = gc->sState.sFog.eMode;
			break;
		case GL_FOG_COLOR:
		{
			*cp++ = gc->sState.sFog.sColor.fRed;
			*cp++ = gc->sState.sFog.sColor.fGreen;
			*cp++ = gc->sState.sFog.sColor.fBlue;
			*cp++ = gc->sState.sFog.sColor.fAlpha;
			break;
		}
		case GL_DEPTH_RANGE:
			/* These get scaled like colors, to [0, 2^31-1] */
			*cp++ = gc->sState.sViewport.fZNear;
			*cp++ = gc->sState.sViewport.fZFar;
			break;
		case GL_DEPTH_WRITEMASK:
		{
			if(gc->sState.sDepth.ui32TestFunc & EURASIA_ISPA_DWRITEDIS)
			{
				*lp++ = GL_FALSE;
			}
			else
			{
				*lp++ = GL_TRUE;
			}
			break;
		}
		case GL_DEPTH_CLEAR_VALUE:
			/* This gets scaled like colors, to [0, 2^31-1] */
			*cp++ = gc->sState.sDepth.fClear;
			break;
		case GL_DEPTH_FUNC:
		{
			IMG_UINT32 ui32DepthFunc = (gc->sState.sDepth.ui32TestFunc & ~EURASIA_ISPA_DCMPMODE_CLRMSK) >> 
																		  EURASIA_ISPA_DCMPMODE_SHIFT;
			*ep++ = GL_NEVER + ui32DepthFunc;
			break;
		}
		case GL_STENCIL_CLEAR_VALUE:
		{
			*ip++ = (IMG_INT32)gc->sState.sStencil.ui32Clear;
			break;
		}
		case GL_STENCIL_FUNC:
		{
			IMG_UINT32 ui32StencilFunc = (gc->sState.sStencil.ui32Stencil & ~EURASIA_ISPC_SCMP_CLRMSK) >> 
																				EURASIA_ISPC_SCMP_SHIFT;
			*ep++ = GL_NEVER + ui32StencilFunc;
			break;
		}
		case GL_STENCIL_VALUE_MASK:
		{
			*ip++ = (IMG_INT32)(gc->sState.sStencil.ui32StencilCompareMaskIn & GLES1_MAX_STENCIL_VALUE);
			break;
		}
		case GL_STENCIL_FAIL:
		{
			*ep++ = GetStencilOp(gc->sState.sStencil.ui32Stencil, EURASIA_ISPC_SOP1_SHIFT, ~EURASIA_ISPC_SOP1_CLRMSK);
			break;
		}
		case GL_STENCIL_PASS_DEPTH_FAIL:
		{
			*ep++ = GetStencilOp(gc->sState.sStencil.ui32Stencil, EURASIA_ISPC_SOP2_SHIFT, ~EURASIA_ISPC_SOP2_CLRMSK);
			break;
		}
		case GL_STENCIL_PASS_DEPTH_PASS:
		{
			*ep++ = GetStencilOp(gc->sState.sStencil.ui32Stencil, EURASIA_ISPC_SOP3_SHIFT, ~EURASIA_ISPC_SOP3_CLRMSK);
			break;
		}
		case GL_STENCIL_REF:
		{
			*ip++ = Clampi(gc->sState.sStencil.i32StencilRefIn, 0, (IMG_INT32)GLES1_MAX_STENCIL_VALUE);
			break;
		}
		case GL_STENCIL_WRITEMASK:
		{
			*ip++ = (IMG_INT32)(gc->sState.sStencil.ui32StencilWriteMaskIn & GLES1_MAX_STENCIL_VALUE);
			break;
		}
		case GL_MATRIX_MODE:
			*ep++ = gc->sState.eMatrixMode;
			break;
		case GL_VIEWPORT:
			*ip++ = gc->sState.sViewport.i32X;
			*ip++ = gc->sState.sViewport.i32Y;
			*ip++ = (IMG_INT32)gc->sState.sViewport.ui32Width;
			*ip++ = (IMG_INT32)gc->sState.sViewport.ui32Height;
			break;
		case GL_MODELVIEW_MATRIX:
		{
			mp = &gc->sTransform.psModelView->sMatrix.afMatrix[0][0];
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++;
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++;
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++;
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp;
			break;
		}
		case GL_PROJECTION_MATRIX:
		{
			mp = &gc->sTransform.psProjection->sMatrix.afMatrix[0][0];
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++;
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++;
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++;
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp;
			break;
		}
		case GL_TEXTURE_MATRIX:
		{
			mp = &gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture]->sMatrix.afMatrix[0][0];
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++;
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++;
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++;
			*fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp++; *fp++ = *mp;
			break;
		}
		case GL_MODELVIEW_MATRIX_FLOAT_AS_INT_BITS_OES:
		{
			if (ui32Type != GLES1_INT32)
			{
				SetError(gc, GL_INVALID_ENUM);
				return;
			}
			mp = &gc->sTransform.psModelView->sMatrix.afMatrix[0][0];
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); 
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); 
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); 
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp); 
			break;
		}
		case GL_PROJECTION_MATRIX_FLOAT_AS_INT_BITS_OES:
		{
			if (ui32Type != GLES1_INT32)
			{
				SetError(gc, GL_INVALID_ENUM);
				return;
			}
			mp = &gc->sTransform.psProjection->sMatrix.afMatrix[0][0];
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); 
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); 
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); 
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp); 
			break;
		}
		case GL_TEXTURE_MATRIX_FLOAT_AS_INT_BITS_OES:
		{
			if (ui32Type != GLES1_INT32)
			{
				SetError(gc, GL_INVALID_ENUM);
				return;
			}
			mp = &gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture]->sMatrix.afMatrix[0][0];
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); 
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); 
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); 
			*ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp++); *ip++ = FLOAT_TO_LONG(*mp); 
			break;
		}
		case GL_ALPHA_TEST_FUNC:
		{
			*ep++ = gc->sState.sRaster.ui32AlphaTestFunc;
			break;
		}
		case GL_ALPHA_TEST_REF:
		{
			/* This gets scaled like colors, to [0, 2^31-1] */
			*cp++ = gc->sState.sRaster.fAlphaTestRef;
			break;	
		}
		case GL_BLEND_SRC:
#if defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE)
		case GL_BLEND_SRC_RGB_OES:
#endif /* defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE) */
		{
			*ep++ = GetBlendFactor(gc, GLES1_BLENDFACTOR_RGBSRC_SHIFT, GLES1_BLENDFACTOR_RGBSRC_MASK);
			break;
		}
		case GL_BLEND_DST:
#if defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE)
		case GL_BLEND_DST_RGB_OES:
#endif /* defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE) */
		{
			*ep++ = GetBlendFactor(gc, GLES1_BLENDFACTOR_RGBDST_SHIFT, GLES1_BLENDFACTOR_RGBDST_MASK);
			break;
		}
#if defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE)
		case GL_BLEND_DST_ALPHA_OES:
		{
			*ep++ = GetBlendFactor(gc, GLES1_BLENDFACTOR_ALPHADST_SHIFT, GLES1_BLENDFACTOR_ALPHADST_MASK);
			break;
		}
		case GL_BLEND_SRC_ALPHA_OES:
		{
			*ep++ = GetBlendFactor(gc, GLES1_BLENDFACTOR_ALPHASRC_SHIFT, GLES1_BLENDFACTOR_ALPHASRC_MASK);
			break;
		}
#endif /* defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE) */
#if defined(GLES1_EXTENSION_BLEND_SUBTRACT) || defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE) 
		case GL_BLEND_EQUATION_OES:
#if defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE)
/*		case GL_BLEND_EQUATION_RGB: (GL_BLEND_EQUATION_RGB is the same value as GL_BLEND_EQUATION) */
#endif /* defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE) */
		{
			*ep++ = GetBlendEquation(gc, GLES1_BLENDMODE_RGB_SHIFT, GLES1_BLENDMODE_RGB_MASK);
			break;
		}
#endif /* defined(GLES1_EXTENSION_BLEND_SUBTRACT) || defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE) */
#if defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE)
		case GL_BLEND_EQUATION_ALPHA_OES:
		{
			*ep++ = GetBlendEquation(gc, GLES1_BLENDMODE_ALPHA_SHIFT, GLES1_BLENDMODE_ALPHA_MASK);
			break;
		}
#endif /* defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE) */
		case GL_LOGIC_OP_MODE:
			*ep++ = gc->sState.sRaster.ui32LogicOp;
			break;
		case GL_SCISSOR_BOX:
			*ip++ = gc->sState.sScissor.i32ScissorX;
			*ip++ = gc->sState.sScissor.i32ScissorY;
			*ip++ = (IMG_INT32)gc->sState.sScissor.ui32ScissorWidth;
			*ip++ = (IMG_INT32)gc->sState.sScissor.ui32ScissorHeight;
			break;
		case GL_COLOR_CLEAR_VALUE:
		{
			*cp++ = gc->sState.sRaster.sClearColor.fRed;
			*cp++ = gc->sState.sRaster.sClearColor.fGreen;
			*cp++ = gc->sState.sRaster.sClearColor.fBlue;
			*cp++ = gc->sState.sRaster.sClearColor.fAlpha;
			break;
		}
		case GL_COLOR_WRITEMASK:
		{
			IMG_UINT32 ui32ColorMask = gc->sState.sRaster.ui32ColorMask;
			
			*lp++ = (GLboolean)((ui32ColorMask & GLES1_COLORMASK_RED)   ? GL_TRUE : GL_FALSE);
			*lp++ = (GLboolean)((ui32ColorMask & GLES1_COLORMASK_GREEN) ? GL_TRUE : GL_FALSE);
			*lp++ = (GLboolean)((ui32ColorMask & GLES1_COLORMASK_BLUE)  ? GL_TRUE : GL_FALSE);
			*lp++ = (GLboolean)((ui32ColorMask & GLES1_COLORMASK_ALPHA) ? GL_TRUE : GL_FALSE);

			break;
		}
		case GL_POLYGON_OFFSET_FACTOR:
			*fp++ = gc->sState.sPolygon.factor.fVal;
			break;
		case GL_POLYGON_OFFSET_UNITS:
			*fp++ = gc->sState.sPolygon.fUnits;
			break;
		case GL_TEXTURE_BINDING_2D:
		{
			GLESTexture *psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture]
																[GLES1_TEXTURE_TARGET_2D];		
			*ip++ = (IMG_INT32)psTex->sNamedItem.ui32Name;
			break;
		}
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_BINDING_CUBE_MAP_OES:
		{
			GLESTexture *psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture]
																[GLES1_TEXTURE_TARGET_CEM];		
			*ip++ = (IMG_INT32)psTex->sNamedItem.ui32Name;
			break;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_BINDING_STREAM_IMG:
		{
			GLESTexture *psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture]
																[GLES1_TEXTURE_TARGET_STREAM];		
			*ip++ = (IMG_INT32)psTex->sNamedItem.ui32Name;
			break;
		}
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_BINDING_EXTERNAL_OES:
		{
			GLESTexture *psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture]
																[GLES1_TEXTURE_TARGET_STREAM];		
			*ip++ = (IMG_INT32)psTex->sNamedItem.ui32Name;
			break;
		}
#endif   
		case GL_VERTEX_ARRAY_SIZE:
		{
			psAttribState = &(psVAO->asVAOState[AP_VERTEX]);

			*ip++ = (IMG_INT32)(psAttribState->ui32StreamTypeSize >> GLES1_STREAMSIZE_SHIFT);
			break;

		}
		case GL_VERTEX_ARRAY_TYPE:
		{
			psAttribState = &(psVAO->asVAOState[AP_VERTEX]);

			switch(psAttribState->ui32StreamTypeSize & GLES1_STREAMTYPE_MASK)
			{
				case GLES1_STREAMTYPE_BYTE:
				{
					*ep++ = GL_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_SHORT:
				{
					*ep++ = GL_SHORT;
					break;
				}
				case GLES1_STREAMTYPE_UBYTE:
				{
					*ep++ = GL_UNSIGNED_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_FLOAT:
				{
					*ep++ = GL_FLOAT;
					break;
				}
				case GLES1_STREAMTYPE_FIXED:		
				{
					*ep++ = GL_FIXED;
					break;
				}
			}

			break;
		}
		case GL_VERTEX_ARRAY_STRIDE:
		{
			psAttribState = &(psVAO->asVAOState[AP_VERTEX]);

			*ip++ = (IMG_INT32)psAttribState->ui32UserStride;
			break;
		}
		case GL_NORMAL_ARRAY_TYPE:
		{
			psAttribState = &(psVAO->asVAOState[AP_NORMAL]);

			switch(psAttribState->ui32StreamTypeSize & GLES1_STREAMTYPE_MASK)
			{
				case GLES1_STREAMTYPE_BYTE:
				{
					*ep++ = GL_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_SHORT:
				{
					*ep++ = GL_SHORT;
					break;
				}
				case GLES1_STREAMTYPE_UBYTE:
				{
					*ep++ = GL_UNSIGNED_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_FLOAT:
				{
					*ep++ = GL_FLOAT;
					break;
				}
				case GLES1_STREAMTYPE_FIXED:		
				{
					*ep++ = GL_FIXED;
					break;
				}
			}

			break;
		}
		case GL_NORMAL_ARRAY_STRIDE:
		{
			psAttribState = &(psVAO->asVAOState[AP_NORMAL]);

			*ip++ = (IMG_INT32)psAttribState->ui32UserStride;
			break;
		}
		case GL_POINT_SIZE_ARRAY_TYPE_OES:
		{
			psAttribState = &(psVAO->asVAOState[AP_POINTSIZE]);

			switch(psAttribState->ui32StreamTypeSize & GLES1_STREAMTYPE_MASK)
			{
				case GLES1_STREAMTYPE_BYTE:
				{
					*ep++ = GL_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_SHORT:
				{
					*ep++ = GL_SHORT;
					break;
				}
				case GLES1_STREAMTYPE_UBYTE:
				{
					*ep++ = GL_UNSIGNED_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_FLOAT:
				{
					*ep++ = GL_FLOAT;
					break;
				}
				case GLES1_STREAMTYPE_FIXED:		
				{
					*ep++ = GL_FIXED;
					break;
				}
			}

			break;
		}
		case GL_POINT_SIZE_ARRAY_STRIDE_OES:
		{
			psAttribState = &(psVAO->asVAOState[AP_POINTSIZE]);

			*ip++ = (IMG_INT32)psAttribState->ui32UserStride;
			break;
		}
		case GL_COLOR_ARRAY_SIZE:
		{
			psAttribState = &(psVAO->asVAOState[AP_COLOR]);

			*ip++ = (IMG_INT32)(psAttribState->ui32StreamTypeSize >> GLES1_STREAMSIZE_SHIFT);
			break;
		}
		case GL_COLOR_ARRAY_TYPE:
		{
			psAttribState = &(psVAO->asVAOState[AP_COLOR]);

			switch(psAttribState->ui32StreamTypeSize & GLES1_STREAMTYPE_MASK)
			{
				case GLES1_STREAMTYPE_BYTE:
				{
					*ep++ = GL_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_SHORT:
				{
					*ep++ = GL_SHORT;
					break;
				}
				case GLES1_STREAMTYPE_UBYTE:
				{
					*ep++ = GL_UNSIGNED_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_FLOAT:
				{
					*ep++ = GL_FLOAT;
					break;
				}
				case GLES1_STREAMTYPE_FIXED:		
				{
					*ep++ = GL_FIXED;
					break;
				}
			}

			break;
		}
		case GL_COLOR_ARRAY_STRIDE:
		{
			psAttribState = &(psVAO->asVAOState[AP_COLOR]);

			*ip++ = (IMG_INT32)psAttribState->ui32UserStride;
			break;
		}
		case GL_TEXTURE_COORD_ARRAY_SIZE:
		{
			psAttribState = &(psVAO->asVAOState[AP_TEXCOORD0 + gc->sState.ui32ClientActiveTexture]);

			*ip++ = (IMG_INT32)(psAttribState->ui32StreamTypeSize >> GLES1_STREAMSIZE_SHIFT);
			break;
		}
		case GL_TEXTURE_COORD_ARRAY_TYPE:
		{
			psAttribState = &(psVAO->asVAOState[AP_TEXCOORD0 + gc->sState.ui32ClientActiveTexture]);

			switch(psAttribState->ui32StreamTypeSize & GLES1_STREAMTYPE_MASK)
			{
				case GLES1_STREAMTYPE_BYTE:
				{
					*ep++ = GL_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_SHORT:
				{
					*ep++ = GL_SHORT;
					break;
				}
				case GLES1_STREAMTYPE_UBYTE:
				{
					*ep++ = GL_UNSIGNED_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_FLOAT:
				{
					*ep++ = GL_FLOAT;
					break;
				}
				case GLES1_STREAMTYPE_FIXED:		
				{
					*ep++ = GL_FIXED;
					break;
				}
			}

			break;
		}
		case GL_TEXTURE_COORD_ARRAY_STRIDE:
		{
			psAttribState = &(psVAO->asVAOState[AP_TEXCOORD0 + gc->sState.ui32ClientActiveTexture]);

			*ip++ = (IMG_INT32)psAttribState->ui32UserStride;
			break;
		}
		case GL_ARRAY_BUFFER_BINDING:
		{
			if(gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX])
				*ip++ = (IMG_INT32)gc->sBufferObject.psActiveBuffer[ARRAY_BUFFER_INDEX]->sNamedItem.ui32Name;
			else 
				*ip++ = 0;
			break;
		}
		case GL_ELEMENT_ARRAY_BUFFER_BINDING:
		{

			if(psVAO->psBoundElementBuffer)
				*ip++ = (GLint)(psVAO->psBoundElementBuffer->sNamedItem.ui32Name);
			else 
				*ip++ = 0;
			break;
		}
        case GL_VERTEX_ARRAY_BUFFER_BINDING:
		{
			psAttribState = &(psVAO->asVAOState[AP_VERTEX]);

			if(psAttribState->psBufObj)
				*ip++ = (IMG_INT32)psAttribState->psBufObj->sNamedItem.ui32Name;
			else 
				*ip++ = 0;
			break;

		}
        case GL_NORMAL_ARRAY_BUFFER_BINDING:
		{
			psAttribState = &(psVAO->asVAOState[AP_NORMAL]);

			if(psAttribState->psBufObj)
				*ip++ = (IMG_INT32)psAttribState->psBufObj->sNamedItem.ui32Name;
			else 
				*ip++ = 0;
			break;

		}
        case GL_COLOR_ARRAY_BUFFER_BINDING:
		{
			psAttribState = &(psVAO->asVAOState[AP_COLOR]);

			if(psAttribState->psBufObj)
				*ip++ = (IMG_INT32)psAttribState->psBufObj->sNamedItem.ui32Name;
			else 
				*ip++ = 0;
			break;

		}
        case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
		{
			psAttribState = &(psVAO->asVAOState[AP_TEXCOORD0 + gc->sState.ui32ClientActiveTexture]);

			if(psAttribState->psBufObj)
				*ip++ = (IMG_INT32)psAttribState->psBufObj->sNamedItem.ui32Name;
			else 
				*ip++ = 0;
			break;

		}
        case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES:
		{
		    psAttribState = &(psVAO->asVAOState[AP_POINTSIZE]);

			if(psAttribState->psBufObj)
				*ip++ = (IMG_INT32)psAttribState->psBufObj->sNamedItem.ui32Name;
			else 
				*ip++ = 0;
			break;

		}
		case GL_PERSPECTIVE_CORRECTION_HINT:
		case GL_POINT_SMOOTH_HINT:
		case GL_LINE_SMOOTH_HINT:
		case GL_FOG_HINT:
			*ep++ = gc->sState.sHints.eHint[eQuery - GL_PERSPECTIVE_CORRECTION_HINT];
			break;
		case GL_GENERATE_MIPMAP_HINT:
			*ep++ = gc->sState.sHints.eHint[GLES1_HINT_GENMIPMAP];
			break;
		case GL_ALIASED_POINT_SIZE_RANGE:
			*ip++ = (IMG_INT32)GLES1_ALIASED_POINT_SIZE_MIN;
			*ip++ = (IMG_INT32)GLES1_ALIASED_POINT_SIZE_MAX;
			break;
		case GL_ALIASED_LINE_WIDTH_RANGE:
			*ip++ = (IMG_INT32)GLES1_ALIASED_LINE_WIDTH_MIN;
			*ip++ = (IMG_INT32)GLES1_ALIASED_LINE_WIDTH_MAX;
			break;
		case GL_SMOOTH_POINT_SIZE_RANGE: 
			*ip++ = (IMG_INT32)GLES1_SMOOTH_POINT_SIZE_MIN;
			*ip++ = (IMG_INT32)GLES1_SMOOTH_POINT_SIZE_MAX;
			break;
		case GL_SMOOTH_LINE_WIDTH_RANGE:
			*ip++ = (IMG_INT32)GLES1_SMOOTH_LINE_WIDTH_MIN;
			*ip++ = (IMG_INT32)GLES1_SMOOTH_LINE_WIDTH_MAX;
			break;
		case GL_RED_BITS:
		{
			 /* Don't check return - value is undefined if framebuffer is incomplete */
			GetFrameBufferCompleteness(gc);
			*ip++ = (IMG_INT32)gc->psMode->ui32RedBits;
			break;
		}
		case GL_GREEN_BITS:
		{
			 /* Don't check return - value is undefined if framebuffer is incomplete */
			GetFrameBufferCompleteness(gc);
			*ip++ = (IMG_INT32)gc->psMode->ui32GreenBits;
			break;
		}
		case GL_BLUE_BITS:
		{
			 /* Don't check return - value is undefined if framebuffer is incomplete */
			GetFrameBufferCompleteness(gc);
			*ip++ = (IMG_INT32)gc->psMode->ui32BlueBits;
			break;
		}
		case GL_ALPHA_BITS:
		{
			 /* Don't check return - value is undefined if framebuffer is incomplete */
			GetFrameBufferCompleteness(gc);
			*ip++ = (IMG_INT32)gc->psMode->ui32AlphaBits;
			break;
		}
		case GL_DEPTH_BITS:
		{
			 /* Don't check return - value is undefined if framebuffer is incomplete */
			GetFrameBufferCompleteness(gc);
			*ip++ = (IMG_INT32)gc->psMode->ui32DepthBits;
			break;
		}
		case GL_STENCIL_BITS:
		{
			 /* Don't check return - value is undefined if framebuffer is incomplete */
			GetFrameBufferCompleteness(gc);
			*ip++ = (IMG_INT32)gc->psMode->ui32StencilBits;
			break;
		}
		case GL_MAX_LIGHTS:
		{
			*ip++ = GLES1_MAX_LIGHTS;
			break;
		}
		case GL_MAX_MODELVIEW_STACK_DEPTH:
		{
			*ip++ = GLES1_MAX_MODELVIEW_STACK_DEPTH;
			break;
		}
		case GL_MAX_PROJECTION_STACK_DEPTH:
		{
			*ip++ = GLES1_MAX_PROJECTION_STACK_DEPTH;
			break;
		}
		case GL_MAX_TEXTURE_STACK_DEPTH:
		{
			*ip++ = GLES1_MAX_TEXTURE_STACK_DEPTH;
			break;
		}
		case GL_MAX_TEXTURE_SIZE:
		{
			*ip++ = GLES1_MAX_TEXTURE_SIZE;
			break;
		}
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_MAX_CUBE_MAP_TEXTURE_SIZE_OES:
		{
			*ip++ = GLES1_MAX_TEXTURE_SIZE;
			break;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
		case GL_MAX_TEXTURE_UNITS:
		{
			*ip++ = GLES1_MAX_TEXTURE_UNITS;
			break;
		}
		case GL_MAX_VIEWPORT_DIMS:
		{
			*ip++ = (IMG_INT32)gc->psMode->ui32MaxViewportX;
			*ip++ = (IMG_INT32)gc->psMode->ui32MaxViewportY;
			break;
		}
		case GL_SUBPIXEL_BITS:
		{
			*ip++ = GLES1_MAX_SUBPIXEL_BITS;
			break;
		}
		case GL_IMPLEMENTATION_COLOR_READ_TYPE_OES:
		{
			switch(gc->psReadParams->ePixelFormat)
			{	
				case PVRSRV_PIXEL_FORMAT_ARGB8888:
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				case PVRSRV_PIXEL_FORMAT_XRGB8888:
				case PVRSRV_PIXEL_FORMAT_XBGR8888:
				{
					*ep++ = GL_UNSIGNED_BYTE;
					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB4444:
				{
#if defined(GLES1_EXTENSION_READ_FORMAT)
					*ep++ = GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG;
#else 
					*ep++ = GL_UNSIGNED_SHORT_4_4_4_4;
#endif
					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB1555:
				{
					*ep++ = GL_UNSIGNED_SHORT_5_5_5_1;
					break;
				}
				case PVRSRV_PIXEL_FORMAT_RGB565:
				default:
				{
					*ep++ = GL_UNSIGNED_SHORT_5_6_5;
					break;
				}
			}

			break;
		}
		case GL_IMPLEMENTATION_COLOR_READ_FORMAT_OES:
		{
			switch(gc->psReadParams->ePixelFormat)
			{	
				case PVRSRV_PIXEL_FORMAT_ARGB1555:
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				case PVRSRV_PIXEL_FORMAT_XRGB8888:
				case PVRSRV_PIXEL_FORMAT_XBGR8888:
				{
					*ep++ = GL_RGBA;
					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB8888:
				case PVRSRV_PIXEL_FORMAT_ARGB4444:
				{
#if defined(GLES1_EXTENSION_READ_FORMAT)
					*ep++ = GL_BGRA_IMG;
#else
					*ep++ = GL_RGBA;
#endif/* GLES1_EXTENSION_READ_FORMAT */
					break;
				}
				case PVRSRV_PIXEL_FORMAT_RGB565:
				default:
				{
					*ep++ = GL_RGB;
					break;
				}
			}

			break;
		}
		case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
		{
			GLint numformats;

			 numformats = 10;

#if defined(GLES1_EXTENSION_PVRTC)
			numformats += 4;
#endif /* defined(GLES1_EXTENSION_PVRTC) */
#if defined(GLES1_EXTENSION_ETC1)
			numformats += 1;
#endif /* defined(GLES1_EXTENSION_ETC1) */

			*ip++ = numformats;

			break;
		}
		case GL_COMPRESSED_TEXTURE_FORMATS:
		{
			*ep++ = GL_PALETTE4_RGB8_OES;
			*ep++ = GL_PALETTE4_RGBA8_OES;
			*ep++ = GL_PALETTE4_R5_G6_B5_OES;
			*ep++ = GL_PALETTE4_RGBA4_OES;
			*ep++ = GL_PALETTE4_RGB5_A1_OES;
			*ep++ = GL_PALETTE8_RGB8_OES;
			*ep++ = GL_PALETTE8_RGBA8_OES;
			*ep++ = GL_PALETTE8_R5_G6_B5_OES;
			*ep++ = GL_PALETTE8_RGBA4_OES;
			*ep++ = GL_PALETTE8_RGB5_A1_OES;
#if defined(GLES1_EXTENSION_PVRTC)
			*ep++ = GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
			*ep++ = GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
			*ep++ = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
			*ep++ = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
#endif /* defined(GLES1_EXTENSION_PVRTC) */
#if defined(GLES1_EXTENSION_ETC1)
			*ep++ = GL_ETC1_RGB8_OES;
#endif /* defined(GLES1_EXTENSION_ETC1) */
			break;
		}
		case GL_MAX_CLIP_PLANES:
		{
			*ip++ = GLES1_MAX_CLIP_PLANES;
			break;
		}
		case GL_CURRENT_COLOR:
		{
			*cp++ = gc->sState.sCurrent.asAttrib[AP_COLOR].fX;
			*cp++ = gc->sState.sCurrent.asAttrib[AP_COLOR].fY;
			*cp++ = gc->sState.sCurrent.asAttrib[AP_COLOR].fZ;
			*cp++ = gc->sState.sCurrent.asAttrib[AP_COLOR].fW;
			break;
		}
		case GL_CURRENT_NORMAL:
		{
			/* These get scaled like colors, to [0, 2^31-1] */
			*cp++ = gc->sState.sCurrent.asAttrib[AP_NORMAL].fX;
			*cp++ = gc->sState.sCurrent.asAttrib[AP_NORMAL].fY;
			*cp++ = gc->sState.sCurrent.asAttrib[AP_NORMAL].fZ;
			*cp++ = gc->sState.sCurrent.asAttrib[AP_NORMAL].fW;
			break;
		}
		case GL_CURRENT_TEXTURE_COORDS:
		{
			*fp++ = gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + gc->sState.sTexture.ui32ActiveTexture].fX;
			*fp++ = gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + gc->sState.sTexture.ui32ActiveTexture].fY;
			*fp++ = gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + gc->sState.sTexture.ui32ActiveTexture].fZ;
			*fp++ = gc->sState.sCurrent.asAttrib[AP_TEXCOORD0 + gc->sState.sTexture.ui32ActiveTexture].fW;
			break;
		}
		case GL_MODELVIEW_STACK_DEPTH:
		{
			*ip++ = (gc->sTransform.psModelView - gc->sTransform.psModelViewStack) + 1;
			break;			
		}
		case GL_PROJECTION_STACK_DEPTH:
		{
			*ip++ = (gc->sTransform.psProjection - gc->sTransform.psProjectionStack) + 1;
			break;			
		}
		case GL_TEXTURE_STACK_DEPTH:
		{
			*ip++ = (gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture] - 
					 gc->sTransform.apsTextureStack[gc->sState.sTexture.ui32ActiveTexture]) + 1;
			break;			
		}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_INDEX_ARRAY_SIZE_OES:
		{
			psAttribState = &(psVAO->asVAOState[AP_MATRIXINDEX]);

			*ip++ = (IMG_INT32)(psAttribState->ui32StreamTypeSize >> GLES1_STREAMSIZE_SHIFT);
			break;
		}
		case GL_MATRIX_INDEX_ARRAY_TYPE_OES:
		{
			psAttribState = &(psVAO->asVAOState[AP_MATRIXINDEX]);

			switch(psAttribState->ui32StreamTypeSize & GLES1_STREAMTYPE_MASK)
			{
				case GLES1_STREAMTYPE_BYTE:
				{
					*ep++ = GL_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_SHORT:
				{
					*ep++ = GL_SHORT;
					break;
				}
				case GLES1_STREAMTYPE_UBYTE:
				{
					*ep++ = GL_UNSIGNED_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_FLOAT:
				{
					*ep++ = GL_FLOAT;
					break;
				}
				case GLES1_STREAMTYPE_FIXED:		
				{
					*ep++ = GL_FIXED;
					break;
				}
			}

			break;
		}
		case GL_MATRIX_INDEX_ARRAY_STRIDE_OES:
		{
			psAttribState = &(psVAO->asVAOState[AP_MATRIXINDEX]);

			*ip++ = (IMG_INT32)psAttribState->ui32UserStride;
			break;
		}
		case GL_MATRIX_INDEX_ARRAY_BUFFER_BINDING_OES:
		{
			psAttribState = &(psVAO->asVAOState[AP_MATRIXINDEX]);

			if(psAttribState->psBufObj)
			{
				*ip++ = (IMG_INT32)psAttribState->psBufObj->sNamedItem.ui32Name;
			}
			else 
			{
				*ip++ = 0;
			}
			break;
		}
		case GL_WEIGHT_ARRAY_SIZE_OES:
		{
			psAttribState = &(psVAO->asVAOState[AP_WEIGHTARRAY]);

			*ip++ = (IMG_INT32)(psAttribState->ui32StreamTypeSize >> GLES1_STREAMSIZE_SHIFT);
			break;
		}
		case GL_WEIGHT_ARRAY_TYPE_OES:
		{
			psAttribState = &(psVAO->asVAOState[AP_WEIGHTARRAY]);

			switch(psAttribState->ui32StreamTypeSize & GLES1_STREAMTYPE_MASK)
			{
				case GLES1_STREAMTYPE_BYTE:
				{
					*ep++ = GL_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_SHORT:
				{
					*ep++ = GL_SHORT;
					break;
				}
				case GLES1_STREAMTYPE_UBYTE:
				{
					*ep++ = GL_UNSIGNED_BYTE;
					break;
				}
				case GLES1_STREAMTYPE_FLOAT:
				{
					*ep++ = GL_FLOAT;
					break;
				}
				case GLES1_STREAMTYPE_FIXED:		
				{
					*ep++ = GL_FIXED;
					break;
				}
			}

			break;
		}
		case GL_WEIGHT_ARRAY_STRIDE_OES:
		{
			psAttribState = &(psVAO->asVAOState[AP_WEIGHTARRAY]);

			*ip++ = (IMG_INT32)psAttribState->ui32UserStride;
			break;
		}
		case GL_WEIGHT_ARRAY_BUFFER_BINDING_OES:
		{
			psAttribState = &(psVAO->asVAOState[AP_WEIGHTARRAY]);

			if(psAttribState->psBufObj)
			{
				*ip++ = (IMG_INT32)psAttribState->psBufObj->sNamedItem.ui32Name;
			}
			else 
			{
				*ip++ = 0;
			}
			break;
		}
		case GL_MAX_PALETTE_MATRICES_OES:
		{
			*ip++ = GLES1_MAX_PALETTE_MATRICES;
			break;
		}	
		case GL_MAX_VERTEX_UNITS_OES:
		{
			*ip++ = GLES1_MAX_VERTEX_UNITS;
			break;
		}	
		case GL_CURRENT_PALETTE_MATRIX_OES:
		{
			*ip++ = (IMG_INT32)gc->sState.sCurrent.ui32MatrixPaletteIndex;
			break;
		}	
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
		case GL_SAMPLE_BUFFERS:
		{
			switch(gc->psMode->ui32AntiAliasMode)
			{
				case 0:
				default:
				{
					*ip++ = 0;
					break;
				}
				case IMG_ANTIALIAS_2x1:
				case IMG_ANTIALIAS_2x2:
				{
					*ip++ = 1;
					break;
				}
			}

			break;
		}
		case GL_SAMPLES:
		{
			switch(gc->psMode->ui32AntiAliasMode)
			{
				case 0:
				default:
				{
					*ip++ = 0;
					break;
				}
				case IMG_ANTIALIAS_2x1:
				{
					*ip++ = 2;
					break;
				}
				case IMG_ANTIALIAS_2x2:
				{
					*ip++ = 4;
					break;
				}
			}

			break;
		}	
#if defined(GLES1_EXTENSION_FRAME_BUFFER_OBJECTS)
		case GL_FRAMEBUFFER_BINDING_OES:
		{
			GLESFrameBuffer *psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;		

			*ip++ = (IMG_INT32)psFrameBuffer->sNamedItem.ui32Name;
			break;
		}
		case GL_RENDERBUFFER_BINDING_OES:
		{
			GLESRenderBuffer *psRenderBuffer = gc->sFrameBuffer.psActiveRenderBuffer;		

			if(psRenderBuffer)
				*ip++ = (IMG_INT32)((GLES1NamedItem*)psRenderBuffer)->ui32Name;
			else
				*ip++ = 0;
			break;
		}
		case GL_MAX_RENDERBUFFER_SIZE_OES:
		{
			*ip++ = GLES1_MAX_TEXTURE_SIZE;
			break;
		}
#endif /* GLES1_EXTENSION_FRAME_BUFFER_OBJECTS */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_NUM_STREAM_DEVICES_IMG:
		{
			*ip++ = (IMG_INT32)GetNumStreamDevices(gc);
			break;
		}
#endif /* GLES1_EXTENSION_TEXTURE_STREAM */
#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)
	    case GL_VERTEX_ARRAY_BINDING_OES:
		{
		    GLES1_ASSERT(VAO(gc));
			*ip++ = (IMG_INT32)(gc->sVAOMachine.psActiveVAO->sNamedItem.ui32Name);
			break;
		}	
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			return;
		}
    }

    /* Use the motion of the pointers to type convert the result */
    if (ip != itemp) 
	{
		ConvertData(GLES1_INT32, itemp, ui32Type, pvResult, (IMG_UINT32)(ip - itemp));
    }
	else if (ep != etemp) 
	{
		ConvertData(GLES1_ENUM, etemp, ui32Type, pvResult, (IMG_UINT32)(ep - etemp));
	} 
	else if (fp != ftemp) 
	{
		ConvertData(GLES1_FLOAT, ftemp, ui32Type, pvResult, (IMG_UINT32)(fp - ftemp));
	} 
	else if (lp != ltemp) 
	{
		ConvertData(GLES1_BOOLEAN, ltemp, ui32Type, pvResult, (IMG_UINT32)(lp - ltemp));
    } 
	else if (cp != ctemp) 
	{
		ConvertData(GLES1_COLOR, ctemp, ui32Type, pvResult, (IMG_UINT32)(cp - ctemp));
    } 
}



/***********************************************************************************
 Function Name      : GetTexParameter
 Inputs             : gc, eTarget, ePname
 Outputs            : pvReturn
 Returns            : Texture parameter
 Description        : UTILITY: Returns texture parameter state as indicated by ePname
************************************************************************************/
static IMG_BOOL GetTexParameter(GLES1Context *gc, GLenum eTarget, GLenum ePname, IMG_VOID *pvResult, IMG_UINT32 ui32Type)
{
	GLESTexture *psTex;
	GLESTextureParamState *psParamState;
	IMG_UINT32 ui32TexTarget;
	
	IMG_INT32 itemp[16], *ip = itemp;		  /* NOTE: for ints */
	IMG_UINT32 etemp[16], *ep = etemp;		  /* NOTE: for enum */
    IMG_FLOAT ftemp[16], *fp = ftemp;		  /* NOTE: for floats */
	GLboolean ltemp[16], *lp = ltemp;		  /* NOTE: for logicals */

	switch(eTarget)
	{
		case GL_TEXTURE_2D:
			ui32TexTarget = GLES1_TEXTURE_TARGET_2D;
			break;
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_OES:
			ui32TexTarget = GLES1_TEXTURE_TARGET_CEM;
			break;
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_STREAM_IMG:
			ui32TexTarget = GLES1_TEXTURE_TARGET_STREAM;
			break;
#endif /* GLES1_EXTENSION_TEXTURE_STREAM */ 
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_EXTERNAL_OES:
			ui32TexTarget = GLES1_TEXTURE_TARGET_STREAM;
			break;
#endif
		default:
bad_enum:
			SetError(gc, GL_INVALID_ENUM);
			return IMG_FALSE;
	}

	psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture][ui32TexTarget];
	psParamState = &psTex->sState;

	switch (ePname) 
	{
#if defined(GLES1_EXTENSION_DRAW_TEXTURE)
		case GL_TEXTURE_CROP_RECT_OES:
		{
			*ip++ = psParamState->i32CropRectU;
			*ip++ = psParamState->i32CropRectV;
			*ip++ = psParamState->i32CropRectW;
			*ip++ = psParamState->i32CropRectH;
			break;
		}
#endif /* defined(GLES1_EXTENSION_DRAW_TEXTURE) */
		case GL_TEXTURE_WRAP_S:
		{			    
			switch(psParamState->ui32StateWord0 & ~EURASIA_PDS_DOUTT0_UADDRMODE_CLRMSK)
			{
				case EURASIA_PDS_DOUTT0_UADDRMODE_REPEAT:
				{
					*ep++ = GL_REPEAT;
					break;
				}
				case EURASIA_PDS_DOUTT0_UADDRMODE_CLAMP:
				{
					*ep++ = GL_CLAMP_TO_EDGE;
					break;
				}
#if defined(GLES1_EXTENSION_TEX_MIRRORED_REPEAT)
				case EURASIA_PDS_DOUTT0_UADDRMODE_FLIP:
				{
					*ep++ = GL_MIRRORED_REPEAT_OES;
					break;
				}
#endif /* defined(GLES1_EXTENSION_TEX_MIRRORED_REPEAT) */
			}
			break;
		}
		case GL_TEXTURE_WRAP_T:
		{
			switch(psParamState->ui32StateWord0 & ~EURASIA_PDS_DOUTT0_VADDRMODE_CLRMSK)
			{
				case EURASIA_PDS_DOUTT0_VADDRMODE_REPEAT:
				{
					*ep++ = GL_REPEAT;
					break;
				}
				case EURASIA_PDS_DOUTT0_VADDRMODE_CLAMP:
				{
					*ep++ = GL_CLAMP_TO_EDGE;
					break;
				}
#if defined(GLES1_EXTENSION_TEX_MIRRORED_REPEAT)
				case EURASIA_PDS_DOUTT0_VADDRMODE_FLIP:
				{
					*ep++ = GL_MIRRORED_REPEAT_OES;
					break;
				}
#endif /* defined(GLES1_EXTENSION_TEX_MIRRORED_REPEAT) */
			}
			break;
		}
		case GL_TEXTURE_MIN_FILTER:
		{
			switch(psParamState->ui32MinFilter)
			{
				case (EURASIA_PDS_DOUTT0_NOTMIPMAP | EURASIA_PDS_DOUTT0_MINFILTER_POINT):
				{
					*ep++ = GL_NEAREST;
					break;
				}
				case (EURASIA_PDS_DOUTT0_NOTMIPMAP | EURASIA_PDS_DOUTT0_MINFILTER_LINEAR):
				{
					*ep++ = GL_LINEAR;
					break;
				}
				case (EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX | EURASIA_PDS_DOUTT0_MINFILTER_POINT):
				{
					*ep++ = GL_NEAREST_MIPMAP_NEAREST;
					break;
				}
				case (EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX | EURASIA_PDS_DOUTT0_MINFILTER_LINEAR):
				{
					*ep++ = GL_LINEAR_MIPMAP_NEAREST;
					break;
				}
				case (EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX | EURASIA_PDS_DOUTT0_MINFILTER_POINT |	EURASIA_PDS_DOUTT0_MIPFILTER):
				{
					*ep++ = GL_NEAREST_MIPMAP_LINEAR;
					break;
				}
				case (EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX | EURASIA_PDS_DOUTT0_MIPFILTER | EURASIA_PDS_DOUTT0_MINFILTER_LINEAR):
				{
					*ep++ = GL_LINEAR_MIPMAP_LINEAR;
					break;
				}
			}
			break;
		}
		case GL_TEXTURE_MAG_FILTER:
		{
			switch(psParamState->ui32MagFilter)
			{
				case (EURASIA_PDS_DOUTT0_MAGFILTER_POINT):
				{
					*ep++ = GL_NEAREST;
					break;
				}
				case (EURASIA_PDS_DOUTT0_MAGFILTER_LINEAR):
				{
					*ep++ = GL_LINEAR;
					break;
				}
			}
			break;
		}
		case GL_GENERATE_MIPMAP:
		{
			*lp++ = (GLboolean)((psParamState->bGenerateMipmap ? GL_TRUE : GL_FALSE));
			break;
		}
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES:
		{
			*ip ++ = 1;
			break;
		}
#endif
		default:
		{
			goto bad_enum;
		}
	}
	
	/* Use the motion of the pointers to type convert the result */
    if (ip != itemp) 
	{
		ConvertData(GLES1_INT32, itemp, ui32Type, pvResult, (IMG_UINT32)(ip - itemp));
    }
	else if (ep != etemp) 
	{
		ConvertData(GLES1_ENUM, etemp, ui32Type, pvResult, (IMG_UINT32)(ep - etemp));
	} 
	else if (fp != ftemp) 
	{
		ConvertData(GLES1_FLOAT, ftemp, ui32Type, pvResult, (IMG_UINT32)(fp - ftemp));
	} 
	else if (lp != ltemp) 
	{
		ConvertData(GLES1_BOOLEAN, ltemp, ui32Type, pvResult, (IMG_UINT32)(lp - ltemp));
    }

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : GetTexEnvCombine
 Inputs             : ePname, psEnvState
 Outputs            : pui32Result
 Returns            : -
 Description        : UTILITY: Returns texture combiner state as indicated by ePname
************************************************************************************/
static IMG_BOOL GetTexEnvCombine(GLES1Context *gc, GLenum ePname, IMG_UINT32 *pui32Result)
{
	IMG_UINT32 i;

	switch(ePname)
	{
		case GL_COMBINE_RGB:
		{
			switch(GLES1_COMBINE_GET_COLOROP(gc->sState.sTexture.psActive->sEnv.ui32Op))
			{
				case GLES1_COMBINEOP_REPLACE:
				{
					*pui32Result = GL_REPLACE;
					break;
				}
				case GLES1_COMBINEOP_MODULATE:
				{
					*pui32Result = GL_MODULATE;
					break;
				}
				case GLES1_COMBINEOP_ADD:
				{
					*pui32Result = GL_ADD;
					break;
				}
				case GLES1_COMBINEOP_ADDSIGNED:
				{
					*pui32Result = GL_ADD_SIGNED;
					break;
				}
				case GLES1_COMBINEOP_INTERPOLATE:
				{
					*pui32Result = GL_INTERPOLATE;
					break;
				}
				case GLES1_COMBINEOP_SUBTRACT:
				{
					*pui32Result = GL_SUBTRACT;
					break;
				}
				case GLES1_COMBINEOP_DOT3_RGB:
				{
					*pui32Result = GL_DOT3_RGB;
					break;
				}
				case GLES1_COMBINEOP_DOT3_RGBA:
				{
					*pui32Result = GL_DOT3_RGBA;
					break;
				}
				default:
				{
					*pui32Result = 0;

					PVR_DPF((PVR_DBG_ERROR,"GetTexEnvCombine: Invalid operation"));

					return IMG_FALSE;
				}
			}
			break;
		}
		case GL_COMBINE_ALPHA:
		{
			switch(GLES1_COMBINE_GET_ALPHAOP(gc->sState.sTexture.psActive->sEnv.ui32Op))
			{
				case GLES1_COMBINEOP_REPLACE:
				{
					*pui32Result = GL_REPLACE;
					break;
				}
				case GLES1_COMBINEOP_MODULATE:
				{
					*pui32Result = GL_MODULATE;
					break;
				}
				case GLES1_COMBINEOP_ADD:
				{
					*pui32Result = GL_ADD;
					break;
				}
				case GLES1_COMBINEOP_ADDSIGNED:
				{
					*pui32Result = GL_ADD_SIGNED;
					break;
				}
				case GLES1_COMBINEOP_INTERPOLATE:
				{
					*pui32Result = GL_INTERPOLATE;
					break;
				}
				case GLES1_COMBINEOP_SUBTRACT:
				{
					*pui32Result = GL_SUBTRACT;
					break;
				}
				default:
				{
					*pui32Result = 0;

					PVR_DPF((PVR_DBG_ERROR,"GetTexEnvCombine: Invalid operation"));

					return IMG_FALSE;
				}
			}
			break;
		}
		case GL_SRC0_RGB:
		case GL_SRC1_RGB:
		case GL_SRC2_RGB:
		{
			i = ePname - GL_SRC0_RGB;

			switch(GLES1_COMBINE_GET_COLSRC(gc->sState.sTexture.psActive->sEnv.ui32ColorSrcs, i))
			{
				case GLES1_COMBINECSRC_PRIMARY:
				{
					*pui32Result = GL_PRIMARY_COLOR;
					break;
				}
				case GLES1_COMBINECSRC_PREVIOUS:
				{
					*pui32Result = GL_PREVIOUS;
					break;
				}
				case GLES1_COMBINECSRC_TEXTURE:
				{
					i = GLES1_COMBINE_GET_COLCROSSBAR(gc->sState.sTexture.psActive->sEnv.ui32ColorSrcs, i);

					if(i & GLES1_COMBINECSRC_CROSSBAR)
					{
						/* Crossbar unit */
						*pui32Result = GL_TEXTURE0 + (i >> GLES1_COMBINECSRC_CROSSBAR_UNIT_SHIFT);
					}
					else
					{
						*pui32Result = GL_TEXTURE;				
					}
					break;
				}
				case GLES1_COMBINECSRC_CONSTANT:
				{
					*pui32Result = GL_CONSTANT;
					break;
				}
				default:
				{
					*pui32Result = 0;

					PVR_DPF((PVR_DBG_ERROR,"GetTexEnvCombine: Invalid source"));

					return IMG_FALSE;
				}
			}
			break;
		}
		case GL_SRC0_ALPHA:
		case GL_SRC1_ALPHA:
		case GL_SRC2_ALPHA:
		{
			i = ePname - GL_SRC0_ALPHA;

			switch(GLES1_COMBINE_GET_ALPHASRC(gc->sState.sTexture.psActive->sEnv.ui32AlphaSrcs, i))
			{
				case GLES1_COMBINEASRC_PRIMARY:
				{
					*pui32Result = GL_PRIMARY_COLOR;
					break;
				}
				case GLES1_COMBINEASRC_PREVIOUS:
				{
					*pui32Result = GL_PREVIOUS;
					break;
				}
				case GLES1_COMBINEASRC_TEXTURE:
				{
					i = GLES1_COMBINE_GET_ALPHACROSSBAR(gc->sState.sTexture.psActive->sEnv.ui32AlphaSrcs, i);

					if(i & GLES1_COMBINEASRC_CROSSBAR)
					{
						/* Crossbar unit */
						*pui32Result = GL_TEXTURE0 + (i >> GLES1_COMBINEASRC_CROSSBAR_UNIT_SHIFT);
					}
					else
					{
						*pui32Result = GL_TEXTURE;
					}
					break;
				}
				case GLES1_COMBINEASRC_CONSTANT:
				{
					*pui32Result = GL_CONSTANT;
					break;
				}
				default:
				{
					*pui32Result = 0;

					PVR_DPF((PVR_DBG_ERROR,"GetTexEnvCombine: Invalid source"));

					return IMG_FALSE;
				}
			}
			break;
		}
		case GL_OPERAND0_RGB:
		case GL_OPERAND1_RGB:
		case GL_OPERAND2_RGB:
		{
			i = ePname - GL_OPERAND0_RGB;

			switch(GLES1_COMBINE_GET_COLOPERAND(gc->sState.sTexture.psActive->sEnv.ui32ColorSrcs, i))
			{
				case GLES1_COMBINECSRC_OPERANDCOLOR:
				{
					*pui32Result = GL_SRC_COLOR;
					break;
				}
				case GLES1_COMBINECSRC_OPERANDCOLOR | GLES1_COMBINECSRC_OPERANDONEMINUS:
				{
					*pui32Result = GL_ONE_MINUS_SRC_COLOR;
					break;
				}
				case GLES1_COMBINECSRC_OPERANDALPHA:
				{
					*pui32Result = GL_SRC_ALPHA;
					break;
				}
				case GLES1_COMBINECSRC_OPERANDALPHA | GLES1_COMBINECSRC_OPERANDONEMINUS:
				{
					*pui32Result = GL_ONE_MINUS_SRC_ALPHA;
					break;
				}
				default:
				{
					*pui32Result = 0;

					PVR_DPF((PVR_DBG_ERROR,"GetTexEnvCombine: Invalid operand"));

					return IMG_FALSE;
				}
			}
			break;
		}
		case GL_OPERAND0_ALPHA:
		case GL_OPERAND1_ALPHA:
		case GL_OPERAND2_ALPHA:
		{
			i = ePname - GL_OPERAND0_ALPHA;

			if(GLES1_COMBINE_GET_ALPHAOPERAND(gc->sState.sTexture.psActive->sEnv.ui32AlphaSrcs, i) == 
			   GLES1_COMBINEASRC_OPERANDONEMINUS)
			{
				*pui32Result = GL_ONE_MINUS_SRC_ALPHA;
			}
			else
			{
				*pui32Result = GL_SRC_ALPHA;
			}
			break;
		}
		case GL_RGB_SCALE:
		{
			switch(GLES1_COMBINE_GET_COLORSCALE(gc->sState.sTexture.psActive->sEnv.ui32Op))
			{
				case GLES1_COMBINEOP_SCALE_ONE:
				{
					*pui32Result = 1;
					break;
				}
				case GLES1_COMBINEOP_SCALE_TWO:
				{
					*pui32Result = 2;
					break;
				}
				case GLES1_COMBINEOP_SCALE_FOUR:
				{
					*pui32Result = 4;
					break;
				}
				default:
				{
					*pui32Result = 0;

					PVR_DPF((PVR_DBG_ERROR,"GetTexEnvCombine: Invalid scale"));

					return IMG_FALSE;
				}
			}
			break;
		}
		case GL_ALPHA_SCALE:
		{
			switch(GLES1_COMBINE_GET_ALPHASCALE(gc->sState.sTexture.psActive->sEnv.ui32Op))
			{
				case GLES1_COMBINEOP_SCALE_ONE:
				{
					*pui32Result = 1;
					break;
				}
				case GLES1_COMBINEOP_SCALE_TWO:
				{
					*pui32Result = 2;
					break;
				}
				case GLES1_COMBINEOP_SCALE_FOUR:
				{
					*pui32Result = 4;
					break;
				}
				default:
				{
					*pui32Result = 0;

					PVR_DPF((PVR_DBG_ERROR,"GetTexEnvCombine: Invalid scale"));

					return IMG_FALSE;
				}
			}
			break;
		}
		default:
		{
			*pui32Result = 0;

			PVR_DPF((PVR_DBG_ERROR,"GetTexEnvCombine: Invalid pname"));

			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : GetTexEnvfv
 Inputs             : gc, eTarget, ePname
 Outputs            : pfResult
 Returns            : -
 Description        : UTILITY: Returns texture environment state as indicated by ePname
************************************************************************************/
static IMG_BOOL GetTexEnvfv(GLES1Context *gc, GLenum eTarget, GLenum ePname, IMG_FLOAT pfResult[])
{		
	IMG_UINT32 ui32Result;

	switch (eTarget)
	{
		case GL_TEXTURE_ENV:
		{
		    switch (ePname) 
			{
				case GL_TEXTURE_ENV_MODE:
				{
					switch(gc->sState.sTexture.psActive->sEnv.ui32Mode)
					{
						case GLES1_MODULATE_INDEX:
						{
							pfResult[0] = GL_MODULATE;

							break;
						}
						case GLES1_DECAL_INDEX:
						{
							pfResult[0] = GL_DECAL;

							break;
						}
						case GLES1_BLEND_INDEX:
						{
							pfResult[0] = GL_BLEND;

							break;
						}
						case GLES1_REPLACE_INDEX:
						{
							pfResult[0] = GL_REPLACE;

							break;
						}
						case GLES1_ADD_INDEX:
						{
							pfResult[0] = GL_ADD;

							break;
						}
						case GLES1_COMBINE_INDEX:
						{
							pfResult[0] = GL_COMBINE;

							break;
						}
					}

					break;
				}
				case GL_TEXTURE_ENV_COLOR:
				{
					pfResult[0] = gc->sState.sTexture.psActive->sEnv.sColor.fRed;
					pfResult[1] = gc->sState.sTexture.psActive->sEnv.sColor.fGreen;
					pfResult[2] = gc->sState.sTexture.psActive->sEnv.sColor.fBlue;
					pfResult[3] = gc->sState.sTexture.psActive->sEnv.sColor.fAlpha;

					break;
				}
				case GL_COMBINE_RGB:
				case GL_COMBINE_ALPHA:
				case GL_SRC0_RGB:
				case GL_SRC1_RGB:
				case GL_SRC2_RGB:
				case GL_SRC0_ALPHA:
				case GL_SRC1_ALPHA:
				case GL_SRC2_ALPHA:
				case GL_OPERAND0_RGB:
				case GL_OPERAND1_RGB:
				case GL_OPERAND2_RGB:
				case GL_OPERAND0_ALPHA:
				case GL_OPERAND1_ALPHA:
				case GL_OPERAND2_ALPHA:
				case GL_RGB_SCALE:
				case GL_ALPHA_SCALE:
				{
					if (GetTexEnvCombine(gc, ePname, &ui32Result) == IMG_FALSE)
					{
						goto bad_enum;
					}

					pfResult[0] = (IMG_FLOAT) ui32Result;

					break;
				}
				default:
				{
bad_enum:
					SetError(gc, GL_INVALID_ENUM);

					return IMG_FALSE;
				}
		    }
			break;
		}
		case GL_POINT_SPRITE_OES:
		{
			switch (ePname)
			{
				case GL_COORD_REPLACE_OES:
				{
					pfResult[0] = gc->sState.sTexture.psActive->sEnv.bPointSpriteReplace;

					break;
				}
				default:
				{
					goto bad_enum;
				}
			}
			break;
		}
		default:
		{
			goto bad_enum;
		}
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : GetLightfv
 Inputs             : gc, eLight, ePname
 Outputs            : pfResult
 Returns            : -
 Description        : UTILITY: Returns light state as indicated by eLight, ePname
************************************************************************************/
static IMG_VOID GetLightfv(GLES1Context *gc, GLenum eLight, GLenum ePname, IMG_FLOAT pfResult[])
{
    GLESLightSourceState *psLss;

	eLight -= GL_LIGHT0;

    if ((IMG_UINT32)eLight >= GLES1_MAX_LIGHTS)
	{ 
bad_enum:
		SetError(gc, GL_INVALID_ENUM);
		return;
    }

    psLss = &gc->sState.sLight.psSource[eLight];

    switch (ePname) 
	{
		case GL_AMBIENT:
			pfResult[0] = psLss->sAmbient.fRed;
			pfResult[1] = psLss->sAmbient.fGreen;
			pfResult[2] = psLss->sAmbient.fBlue;
			pfResult[3] = psLss->sAmbient.fAlpha;
			break;
		case GL_DIFFUSE:
			pfResult[0] = psLss->sDiffuse.fRed;
			pfResult[1] = psLss->sDiffuse.fGreen;
			pfResult[2] = psLss->sDiffuse.fBlue;
			pfResult[3] = psLss->sDiffuse.fAlpha;
			break;
		case GL_SPECULAR:
			pfResult[0] = psLss->sSpecular.fRed;
			pfResult[1] = psLss->sSpecular.fGreen;
			pfResult[2] = psLss->sSpecular.fBlue;
			pfResult[3] = psLss->sSpecular.fAlpha;
			break;
		case GL_POSITION:
			pfResult[0] = psLss->sPositionEye.fX;
			pfResult[1] = psLss->sPositionEye.fY;
			pfResult[2] = psLss->sPositionEye.fZ;
			pfResult[3] = psLss->sPositionEye.fW;
			break;
		case GL_SPOT_DIRECTION:
			pfResult[0] = psLss->sSpotDirectionEye.fX;
			pfResult[1] = psLss->sSpotDirectionEye.fY;
			pfResult[2] = psLss->sSpotDirectionEye.fZ;
			break;
		case GL_SPOT_EXPONENT:
			pfResult[0] = psLss->fSpotLightExponent;
			break;
		case GL_SPOT_CUTOFF:
			pfResult[0] = psLss->fSpotLightCutOffAngle;
			break;
		case GL_CONSTANT_ATTENUATION:
			pfResult[0] = psLss->afAttenuation[GLES1_LIGHT_ATTENUATION_CONSTANT];
			break;
		case GL_LINEAR_ATTENUATION:
			pfResult[0] = psLss->afAttenuation[GLES1_LIGHT_ATTENUATION_LINEAR];
			break;
		case GL_QUADRATIC_ATTENUATION:
			pfResult[0] = psLss->afAttenuation[GLES1_LIGHT_ATTENUATION_QUADRATIC];
			break;
		default:
			goto bad_enum;
    }
}


/***********************************************************************************
 Function Name      : GetMaterialfv
 Inputs             : gc, eFace, ePname
 Outputs            : pfResult
 Returns            : -
 Description        : UTILITY: Returns material state as indicated by ePname
************************************************************************************/
static IMG_VOID GetMaterialfv(GLES1Context *gc, GLenum eFace, GLenum ePname, IMG_FLOAT pfResult[])
{
    GLESMaterialState *psMaterial = &gc->sState.sLight.sMaterial;

    switch (eFace) 
	{
		case GL_FRONT:
		case GL_BACK:
			break;
		default:
bad_enum:
			SetError(gc, GL_INVALID_ENUM);
			return;
    }

    switch (ePname) 
	{
		case GL_SHININESS:
			pfResult[0] = psMaterial->sSpecularExponent.fX;
			break;
		case GL_EMISSION:
			pfResult[0] = psMaterial->sEmissive.fRed;
			pfResult[1] = psMaterial->sEmissive.fGreen;
			pfResult[2] = psMaterial->sEmissive.fBlue;
			pfResult[3] = psMaterial->sEmissive.fAlpha;
			break;
		case GL_AMBIENT:
			pfResult[0] = psMaterial->sAmbient.fRed;
			pfResult[1] = psMaterial->sAmbient.fGreen;
			pfResult[2] = psMaterial->sAmbient.fBlue;
			pfResult[3] = psMaterial->sAmbient.fAlpha;
			break;
		case GL_DIFFUSE:
			pfResult[0] = psMaterial->sDiffuse.fRed;
			pfResult[1] = psMaterial->sDiffuse.fGreen;
			pfResult[2] = psMaterial->sDiffuse.fBlue;
			pfResult[3] = psMaterial->sDiffuse.fAlpha;
			break;
		case GL_SPECULAR:
			pfResult[0] = psMaterial->sSpecular.fRed;
			pfResult[1] = psMaterial->sSpecular.fGreen;
			pfResult[2] = psMaterial->sSpecular.fBlue;
			pfResult[3] = psMaterial->sSpecular.fAlpha;
			break;
		default:
			goto bad_enum;
    }
}



/***********************************************************************************
 Function Name      : IsEnabled
 Inputs             : gc, eCap
 Outputs            : -
 Returns            : Enable bit
 Description        : UTILITY: Returns an enable bit for state indicated by eCap
************************************************************************************/
static GLboolean IsEnabled(GLES1Context *gc, GLenum eCap)
{
    GLuint bit;
	IMG_UINT32 ui32ArrayEnables;

   /* BE CERTAIN TO:
	   get vertex attrib info from the current VAO instead of VAOMachine,
	   since the info was passed from VAO to VAOMachine only in draw call */

	ui32ArrayEnables = gc->sVAOMachine.psActiveVAO->ui32CurrentArrayEnables;


    switch (eCap) 
	{
		case GL_ALPHA_TEST:
		{
			bit	= gc->ui32RasterEnables & GLES1_RS_ALPHATEST_ENABLE;
			break;
		}
		case GL_BLEND:
		{
			bit	= gc->ui32RasterEnables & GLES1_RS_ALPHABLEND_ENABLE;
			break;
		}
		case GL_COLOR_MATERIAL:
		{
			bit = gc->ui32TnLEnables & GLES1_TL_COLORMAT_ENABLE;
			break;
		}
		case GL_CULL_FACE:
		{
			bit = gc->ui32TnLEnables & GLES1_TL_CULLFACE_ENABLE;
			break;
		}
		case GL_DEPTH_TEST:
		{
			bit	= gc->ui32RasterEnables & GLES1_RS_DEPTHTEST_ENABLE;
			break;
		}
		case GL_DITHER:
		{
			bit = gc->ui32FrameEnables & GLES1_FS_DITHER_ENABLE;
			break;
		}
		case GL_FOG:
		{
			bit	= gc->ui32RasterEnables & GLES1_RS_FOG_ENABLE;
			break;
		}
		case GL_LIGHTING:
		{
			bit = gc->ui32TnLEnables & GLES1_TL_LIGHTING_ENABLE;
			break;
		}
		case GL_LINE_SMOOTH:
		{
			bit	= gc->ui32RasterEnables & GLES1_RS_LINESMOOTH_ENABLE;
			break;
		}
		case GL_COLOR_LOGIC_OP:
		{
			bit	= gc->ui32RasterEnables & GLES1_RS_LOGICOP_ENABLE;
			break;
		}
		case GL_NORMALIZE:
		{
			bit = gc->ui32TnLEnables & GLES1_TL_NORMALIZE_ENABLE;
			break;
		}
		case GL_POINT_SMOOTH:
		{
			bit	= gc->ui32RasterEnables & GLES1_RS_POINTSMOOTH_ENABLE;
			break;
		}
		case GL_RESCALE_NORMAL:
		{
			bit = gc->ui32TnLEnables & GLES1_TL_RESCALE_ENABLE;
			break;
		}
		case GL_SCISSOR_TEST:
		{
			bit = gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE;
			break;
		}
		case GL_STENCIL_TEST:
		{
			bit = gc->ui32RasterEnables & GLES1_RS_STENCILTEST_ENABLE;
			break;
		}
		case GL_TEXTURE_2D:
		{
			bit	= gc->ui32RasterEnables & (GLES1_RS_2DTEXTURE0_ENABLE << gc->sState.sTexture.ui32ActiveTexture);
			break;			
		}
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_OES:
		{
			bit	= gc->ui32RasterEnables & (GLES1_RS_CEMTEXTURE0_ENABLE << gc->sState.sTexture.ui32ActiveTexture);
			break;			
		}
		case GL_TEXTURE_GEN_STR_OES:
		{
			bit	= gc->ui32RasterEnables & (GLES1_RS_GENTEXTURE0_ENABLE << gc->sState.sTexture.ui32ActiveTexture);
			break;			
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_STREAM_IMG:
		{
			bit	= gc->ui32RasterEnables & (GLES1_RS_TEXTURE0_STREAM_ENABLE << gc->sState.sTexture.ui32ActiveTexture);
			break;			
		}
#elif defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_EXTERNAL_OES:
		{
			bit	= gc->ui32RasterEnables & (GLES1_RS_TEXTURE0_STREAM_ENABLE << gc->sState.sTexture.ui32ActiveTexture);
		}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)*/

 		case GL_CLIP_PLANE0: case GL_CLIP_PLANE1:
 		case GL_CLIP_PLANE2: case GL_CLIP_PLANE3: 
 		case GL_CLIP_PLANE4: case GL_CLIP_PLANE5:
		{
			bit = gc->ui32TnLEnables & (GLES1_TL_CLIP_PLANE0_ENABLE << (eCap - GL_CLIP_PLANE0));
			break;
		}
		case GL_LIGHT0: case GL_LIGHT1:
		case GL_LIGHT2: case GL_LIGHT3:
		case GL_LIGHT4: case GL_LIGHT5:
		case GL_LIGHT6: case GL_LIGHT7:
		{
			bit = gc->ui32TnLEnables & (GLES1_TL_LIGHT0_ENABLE << (eCap - GL_LIGHT0));
			break;
		}
		case GL_POLYGON_OFFSET_FILL:
		{
			bit	= gc->ui32RasterEnables & GLES1_RS_POLYOFFSET_ENABLE;
			break;
		}
		case GL_VERTEX_ARRAY:
		{
			bit = ui32ArrayEnables & VARRAY_VERT_ENABLE;
			break;
		}
		case GL_NORMAL_ARRAY:
		{
			bit = ui32ArrayEnables & VARRAY_NORMAL_ENABLE;
			break;
		}
		case GL_COLOR_ARRAY:
		{
			bit = ui32ArrayEnables & VARRAY_COLOR_ENABLE;
			break;
		}
		case GL_TEXTURE_COORD_ARRAY:
		{
			bit = ui32ArrayEnables & (1UL << (AP_TEXCOORD0 + gc->sState.ui32ClientActiveTexture));
			break;
		}
		case GL_POINT_SIZE_ARRAY_OES:
		{
			bit = ui32ArrayEnables & VARRAY_POINTSIZE_ENABLE;
			break;
		}
		case GL_MULTISAMPLE:
		{
			bit = gc->ui32FrameEnables & GLES1_FS_MULTISAMPLE_ENABLE;
			break;
		}
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
		{
			bit = gc->ui32IgnoredEnables & GLES1_IG_MSALPHACOV_ENABLE;
			break;
		}
		case GL_SAMPLE_ALPHA_TO_ONE:
		{
			bit = gc->ui32IgnoredEnables & GLES1_IG_MSSAMPALPHA_ENABLE;
			break;
		}
		case GL_SAMPLE_COVERAGE:
		{
			bit = gc->ui32IgnoredEnables & GLES1_IG_MSSAMPCOV_ENABLE;
			break;
		}
		case GL_POINT_SPRITE_OES:
		{
			bit = gc->ui32TnLEnables & GLES1_TL_POINTSPRITE_ENABLE;
			break;
		}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_PALETTE_OES:
		{
			bit = gc->ui32TnLEnables & GLES1_TL_MATRIXPALETTE_ENABLE;
			break;
		}
		case GL_MATRIX_INDEX_ARRAY_OES:
		{
			bit = ui32ArrayEnables & VARRAY_MATRIXINDEX_ENABLE;
			break;
		}
		case GL_WEIGHT_ARRAY_OES:
		{
			bit = ui32ArrayEnables & VARRAY_WEIGHTARRAY_ENABLE;
			break;
		}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			return GL_FALSE;
		}
    }

    return (GLboolean)(bit != 0);
}


GL_API GLboolean GL_APIENTRY glIsTexture(GLuint texture)
{
    GLESTexture    *psTex;
	GLES1NamesArray *psNamesArray;

	__GLES1_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsTexture"));

	GLES1_TIME_START(GLES1_TIMES_glIsTexture);

	if (texture==0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glIsTexture);
		return GL_FALSE;
	}
	
	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ];

	psTex = (GLESTexture *) NamedItemAddRef(psNamesArray, texture);

	if (psTex==IMG_NULL)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glIsTexture);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psTex);

	GLES1_TIME_STOP(GLES1_TIMES_glIsTexture);

	return GL_TRUE;
}


GL_API GLboolean GL_APIENTRY glIsBuffer(GLuint buffer)
{
    GLESBufferObject *psBuf;
	GLES1NamesArray   *psNamesArray;

	__GLES1_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsBuffer"));

	GLES1_TIME_START(GLES1_TIMES_glIsBuffer);

	if (buffer==0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glIsBuffer);
		return GL_FALSE;
	}

	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ];

	psBuf = (GLESBufferObject *) NamedItemAddRef(psNamesArray, buffer);

	if (psBuf==IMG_NULL)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glIsBuffer);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psBuf);

	GLES1_TIME_STOP(GLES1_TIMES_glIsBuffer);

	return GL_TRUE;
}

/***********************************************************************************
 Function Name      : glGetBufferParameteriv
 Inputs             : target, pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of material state indicated by pname
************************************************************************************/
GL_API void GL_APIENTRY glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
	IMG_UINT32 ui32TargetIndex;
	GLESBufferObject *psBufObj;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetBufferParameteriv"));

	GLES1_TIME_START(GLES1_TIMES_glGetBufferParameteriv);
	
	switch(target)
	{
		case GL_ARRAY_BUFFER:
		case GL_ELEMENT_ARRAY_BUFFER:
		{
			ui32TargetIndex = target - GL_ARRAY_BUFFER;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glGetBufferParameteriv);

			return;
		}
	}

	psBufObj = gc->sBufferObject.psActiveBuffer[ui32TargetIndex];

	if(psBufObj == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES1_TIME_STOP(GLES1_TIMES_glGetBufferParameteriv);

		return;
	}

	switch(pname)
	{
		case GL_BUFFER_SIZE:
		{
			*params = (GLint)psBufObj->ui32BufferSize;

			break;
		}
		case GL_BUFFER_USAGE:
		{
			*params = (GLint)psBufObj->eUsage;

			break;
		}
#if defined(GLES1_EXTENSION_MAP_BUFFER)
		case GL_BUFFER_MAPPED_OES:
		{
			*params = (GLint)psBufObj->bMapped;

			break;
		}
		case GL_BUFFER_ACCESS_OES:
		{
			*params = (GLint)psBufObj->eAccess;

			break;
		}
#endif
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			*params = 0;

			break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glGetBufferParameteriv);
}


#if defined(GLES1_EXTENSION_MAP_BUFFER)
/***********************************************************************************
 Function Name      : glGetBufferPointervOES
 Inputs             : target, pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of material state indicated by pname
************************************************************************************/
GL_API_EXT void GL_APIENTRY glGetBufferPointervOES(GLenum target, GLenum pname, void **params)
{
	IMG_UINT32 ui32TargetIndex;
	GLESBufferObject *psBufObj;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetBufferParameterivOES"));

	GLES1_TIME_START(GLES1_TIMES_glGetBufferPointervOES);

	switch(target)
	{
		case GL_ARRAY_BUFFER:
		case GL_ELEMENT_ARRAY_BUFFER:
		{
			ui32TargetIndex = target - GL_ARRAY_BUFFER;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glGetBufferPointervOES);

			return;
		}
	}

	psBufObj = gc->sBufferObject.psActiveBuffer[ui32TargetIndex];

	if(psBufObj == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES1_TIME_STOP(GLES1_TIMES_glGetBufferPointervOES);

		return;
	}

	switch(pname)
	{
		case GL_BUFFER_MAP_POINTER_OES:
		{
			if (psBufObj->bMapped)
			{
				*params = (void *)psBufObj->psMemInfo->pvLinAddr;
			}
			else
			{
				*params = IMG_NULL;
			}
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			*params = 0;

			break;
		}
	}

	GLES1_TIME_START(GLES1_TIMES_glGetBufferPointervOES);
	
}	
#endif /* defined(GLES1_EXTENSION_MAP_BUFFER) */


/***********************************************************************************
 Function Name      : glGetMaterial[f/x]v
 Inputs             : face, pname
 Outputs            : result
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of material state indicated by pname
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glGetMaterialfv(GLenum face, GLenum pname, GLfloat result[])
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetMaterialfv"));

	GLES1_TIME_START(GLES1_TIMES_glGetMaterialfv);

	GetMaterialfv(gc, face, pname, result);

	GLES1_TIME_STOP(GLES1_TIMES_glGetMaterialfv);
}
#endif

GL_API void GL_APIENTRY glGetMaterialxv(GLenum face, GLenum pname, GLfixed result[])
{
	IMG_FLOAT afResult[4];

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetMaterialxv"));

	GLES1_TIME_START(GLES1_TIMES_glGetMaterialxv);

	GetMaterialfv(gc, face, pname, afResult);

	switch (pname)
	{
	        case GL_FRONT:
	        case GL_BACK:
	        {
                break;
	        }
	        case GL_SHININESS:
	        {
                ConvertData(GLES1_FLOAT, afResult, GLES1_FIXED, result, 1);

                break;
	        }
	        case GL_EMISSION:
	        case GL_AMBIENT:
	        case GL_DIFFUSE:
	        case GL_SPECULAR:
	        {
                ConvertData(GLES1_COLOR, afResult, GLES1_FIXED, result, 4);

                break;
	        }
	        default:
	        {
	                break;
	        }
	}

	GLES1_TIME_STOP(GLES1_TIMES_glGetMaterialxv);
}


/***********************************************************************************
 Function Name      : glGetLight[f/x]v
 Inputs             : light, pname
 Outputs            : result
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of light state indicated by pname
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glGetLightfv(GLenum light, GLenum pname, GLfloat result[])
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetLightfv"));

	GLES1_TIME_START(GLES1_TIMES_glGetLightfv);

	GetLightfv(gc, light, pname, result);

	GLES1_TIME_STOP(GLES1_TIMES_glGetLightfv);
}
#endif

GL_API void GL_APIENTRY glGetLightxv(GLenum light, GLenum pname, GLfixed result[])
{
    IMG_FLOAT afResult[4];

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetLightxv"));

	GLES1_TIME_START(GLES1_TIMES_glGetLightxv);

	GetLightfv(gc, light, pname, afResult);

    switch (pname) 
	{
		case GL_AMBIENT:
		case GL_DIFFUSE:
		case GL_SPECULAR:
			ConvertData(GLES1_COLOR, afResult, GLES1_FIXED, result, 4);
			break;
		case GL_POSITION:
			ConvertData(GLES1_FLOAT, afResult, GLES1_FIXED, result, 4);
			break;
		case GL_SPOT_DIRECTION:
			ConvertData(GLES1_FLOAT, afResult, GLES1_FIXED, result, 3);
			break;
		case GL_SPOT_EXPONENT:
		case GL_SPOT_CUTOFF:
		case GL_CONSTANT_ATTENUATION:
		case GL_LINEAR_ATTENUATION:
		case GL_QUADRATIC_ATTENUATION:
			ConvertData(GLES1_FLOAT, afResult, GLES1_FIXED, result, 1);
			break;
    }

	GLES1_TIME_STOP(GLES1_TIMES_glGetLightxv);
}

#if defined(PROFILE_COMMON)

/***********************************************************************************
 Function Name      : glGetTexEnv[f/x]v
 Inputs             : target, pname
 Outputs            : v
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of texture environment state 
					  indicated by pname
************************************************************************************/
GL_API void GL_APIENTRY glGetTexEnvfv(GLenum target, GLenum pname, GLfloat v[])
{
	__GLES1_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexEnvfv"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexEnvfv);

	GetTexEnvfv(gc, target, pname, v);

	GLES1_TIME_STOP(GLES1_TIMES_glGetTexEnvfv);
}
#endif

GL_API void GL_APIENTRY glGetTexEnvxv(GLenum target, GLenum pname, GLfixed v[])
{
	IMG_FLOAT afResult[4];

	__GLES1_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexEnvxv"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexEnvxv);

	if (GetTexEnvfv(gc, target, pname, afResult) != IMG_FALSE)
	{
		if((target == GL_TEXTURE_ENV) && (pname == GL_TEXTURE_ENV_COLOR))
		{
			ConvertData(GLES1_COLOR, afResult, GLES1_FIXED, v, 4);
		}
		else if((target == GL_TEXTURE_ENV) && ((pname == GL_RGB_SCALE) || (pname == GL_ALPHA_SCALE)))
		{
			ConvertData(GLES1_FLOAT, afResult, GLES1_FIXED, v, 1);
		}
		else
		{
			/* Don't shift results for enums */
			v[0] = (IMG_INT32)afResult[0];
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glGetTexEnvxv);
}


GL_API void GL_APIENTRY glGetTexEnviv(GLenum target, GLenum pname, GLint v[])
{
	IMG_FLOAT afResult[4];

	__GLES1_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexEnviv"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexEnviv);

	if(GetTexEnvfv(gc, target, pname, afResult) != IMG_FALSE)
	{
		if((target==GL_TEXTURE_ENV) && (pname==GL_TEXTURE_ENV_COLOR))
		{
			ConvertData(GLES1_COLOR, afResult, GLES1_INT32, v, 4);
		}
		else
		{
			v[0] = (IMG_INT32)afResult[0];
		}
	}
	GLES1_TIME_STOP(GLES1_TIMES_glGetTexEnviv);
}


#if defined(PROFILE_COMMON)
 /***********************************************************************************
 Function Name      : glGetClipPlane[f/x]
 Inputs             : plane
 Outputs            : eqn
 Returns            : -
 Description        : ENTRYPOINT: Returns a clip plane equation indicated by plane
************************************************************************************/
GL_API void GL_APIENTRY glGetClipPlanef(GLenum plane, GLfloat eqn[4])
{
    IMG_UINT32 ui32ClipPlane;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetClipPlanef"));

	GLES1_TIME_START(GLES1_TIMES_glGetClipPlanef);

    ui32ClipPlane = plane - GL_CLIP_PLANE0;
    
	if (ui32ClipPlane >= GLES1_MAX_CLIP_PLANES) 
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES1_TIME_STOP(GLES1_TIMES_glGetClipPlanef);
		return;
    }

    eqn[0] = gc->sTransform.asEyeClipPlane[ui32ClipPlane].fX;
    eqn[1] = gc->sTransform.asEyeClipPlane[ui32ClipPlane].fY;
    eqn[2] = gc->sTransform.asEyeClipPlane[ui32ClipPlane].fZ;
    eqn[3] = gc->sTransform.asEyeClipPlane[ui32ClipPlane].fW;

	GLES1_TIME_STOP(GLES1_TIMES_glGetClipPlanef);
}
#endif


GL_API void GL_APIENTRY glGetClipPlanex(GLenum plane, GLfixed eqn[4])
{
    IMG_UINT32 ui32ClipPlane;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetClipPlanex"));

	GLES1_TIME_START(GLES1_TIMES_glGetClipPlanex);

    ui32ClipPlane = plane - GL_CLIP_PLANE0;
    
	if (ui32ClipPlane >= GLES1_MAX_CLIP_PLANES) 
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES1_TIME_STOP(GLES1_TIMES_glGetClipPlanex);
		return;
    }

    eqn[0] = FLOAT_TO_FIXED(gc->sTransform.asEyeClipPlane[ui32ClipPlane].fX);
    eqn[1] = FLOAT_TO_FIXED(gc->sTransform.asEyeClipPlane[ui32ClipPlane].fY);
    eqn[2] = FLOAT_TO_FIXED(gc->sTransform.asEyeClipPlane[ui32ClipPlane].fZ);
    eqn[3] = FLOAT_TO_FIXED(gc->sTransform.asEyeClipPlane[ui32ClipPlane].fW);

	GLES1_TIME_STOP(GLES1_TIMES_glGetClipPlanex);
}


/***********************************************************************************
 Function Name      : glGetTexParameteriv
 Inputs             : target, pname
 Outputs            : v
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of texture parameter state 
					  indicated by pname
************************************************************************************/
GL_API void GL_APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint v[])
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexParameteriv"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexParameteriv);

	GetTexParameter(gc, target, pname, v, GLES1_INT32);

	GLES1_TIME_STOP(GLES1_TIMES_glGetTexParameteriv);
}


#if defined(PROFILE_COMMON)
GL_API void GL_APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat v[])
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexParameterfv"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexParameterfv);

	GetTexParameter(gc, target, pname, v, GLES1_FLOAT);

	GLES1_TIME_STOP(GLES1_TIMES_glGetTexParameterfv);
}
#endif /* PROFILE_COMMON */


GL_API void GL_APIENTRY glGetTexParameterxv(GLenum target, GLenum pname, GLfixed v[])
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexParameterxv"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexParameterxv);

	GetTexParameter(gc, target, pname, v, GLES1_FIXED);

	GLES1_TIME_STOP(GLES1_TIMES_glGetTexParameterxv);
}


#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
/***********************************************************************************
 Function Name      : glGetTexGenivOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glGetTexGenivOES(GLenum coord, GLenum pname, GLint *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexGenivOES"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexGenivOES);

	switch( coord )
	{
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_GEN_STR_OES:
		{
			if(pname!=GL_TEXTURE_GEN_MODE_OES)
			{
				SetError(gc, GL_INVALID_ENUM);
				return;
			}
			params[0] = (GLint)gc->sState.sTexture.psActive->eTexGenMode;
			break;
		}
#endif
		default:
			SetError(gc, GL_INVALID_ENUM);
			return;
	}
	GLES1_TIME_STOP(GLES1_TIMES_glGetTexGenivOES);
}


#if defined(PROFILE_COMMON)
/***********************************************************************************
 Function Name      : glGetTexGenfvOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glGetTexGenfvOES(GLenum coord, GLenum pname, GLfloat *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexGenfvOES"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexGenfvOES);

	switch( coord )
	{
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_GEN_STR_OES:
		{
			if(pname!=GL_TEXTURE_GEN_MODE_OES)
			{
				SetError(gc, GL_INVALID_ENUM);
				return;
			}
			params[0] = (GLfloat)gc->sState.sTexture.psActive->eTexGenMode;
			break;
		}
#endif
		default:
			SetError(gc, GL_INVALID_ENUM);
			return;
	}
	GLES1_TIME_STOP(GLES1_TIMES_glGetTexGenfvOES);
}
#endif

#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) || defined(GLES1_EXTENSION_CARNAV) */

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
/***********************************************************************************
 Function Name      : glGetTexGenxvOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glGetTexGenxvOES(GLenum coord, GLenum pname, GLfixed *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexGenxvOES"));

	GLES1_TIME_START(GLES1_TIMES_glGetTexGenxvOES);

	if(coord!=GL_TEXTURE_GEN_STR_OES || pname!=GL_TEXTURE_GEN_MODE_OES)
	{
		SetError(gc, GL_INVALID_ENUM);
		return;
	}

	params[0] = (GLfixed)gc->sState.sTexture.psActive->eTexGenMode;

	GLES1_TIME_STOP(GLES1_TIMES_glGetTexGenxvOES);
}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */


/***********************************************************************************
 Function Name      : glGetFixedv
 Inputs             : sq
 Outputs            : result
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of state indicated by sq
************************************************************************************/
GL_API void GL_APIENTRY glGetFixedv(GLenum sq, GLfixed result[])
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetFixedv"));

	GLES1_TIME_START(GLES1_TIMES_glGetFixedv);

    DoGet(gc, sq, result, GLES1_FIXED);

	GLES1_TIME_STOP(GLES1_TIMES_glGetFixedv);
}

#if defined(PROFILE_COMMON)

/***********************************************************************************
 Function Name      : glGetFloatv
 Inputs             : sq
 Outputs            : result
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of state indicated by sq
************************************************************************************/
GL_API void GL_APIENTRY glGetFloatv(GLenum sq, GLfloat result[])
{
 	__GLES1_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetFloatv"));

	GLES1_TIME_START(GLES1_TIMES_glGetFloatv);
	
	DoGet(gc, sq, result, GLES1_FLOAT);
	
	GLES1_TIME_STOP(GLES1_TIMES_glGetFloatv);
}
#endif

/***********************************************************************************
 Function Name      : glGetBooleanv
 Inputs             : sq
 Outputs            : result
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of state indicated by sq
************************************************************************************/
GL_API void GL_APIENTRY glGetBooleanv(GLenum sq, GLboolean result[])
{
 	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetBooleanv"));

	GLES1_TIME_START(GLES1_TIMES_glGetBooleanv);

	DoGet(gc, sq, result, GLES1_BOOLEAN);

	GLES1_TIME_STOP(GLES1_TIMES_glGetBooleanv);
}



/***********************************************************************************
 Function Name      : glGetPointerv
 Inputs             : pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Returns a client pointer indicated by pname
************************************************************************************/
GL_API void GL_APIENTRY glGetPointerv(GLenum pname, void **params)
{

	GLES1VertexArrayObject *psVAO;

	__GLES1_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetPointerv"));

	GLES1_TIME_START(GLES1_TIMES_glGetPointerv);


	/* Setup VAO and attribute state */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1_ASSERT(psVAO);

	/* Safe const cast here. */
	/* PRQA S 0311 ++ */
	switch(pname) 
	{
	    case GL_VERTEX_ARRAY_POINTER:
		    *params = *(IMG_VOID **)&psVAO->asVAOState[AP_VERTEX].pui8Pointer;
			break;
	    case GL_NORMAL_ARRAY_POINTER:
		    *params = *(IMG_VOID **)&psVAO->asVAOState[AP_NORMAL].pui8Pointer;
			break;
	    case GL_COLOR_ARRAY_POINTER:
		    *params = *(IMG_VOID **)&psVAO->asVAOState[AP_COLOR].pui8Pointer;
			break;
	    case GL_TEXTURE_COORD_ARRAY_POINTER:
		    *params = *(IMG_VOID **)&psVAO->asVAOState[AP_TEXCOORD0 + gc->sState.ui32ClientActiveTexture].pui8Pointer;
			break;
	    case GL_POINT_SIZE_ARRAY_POINTER_OES:
		    *params = *(IMG_VOID **)&psVAO->asVAOState[AP_POINTSIZE].pui8Pointer;
			break;
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
	    case GL_MATRIX_INDEX_ARRAY_POINTER_OES:
		    *params = *(IMG_VOID **)&psVAO->asVAOState[AP_MATRIXINDEX].pui8Pointer;
			break;
	    case GL_WEIGHT_ARRAY_POINTER_OES:
		    *params = *(IMG_VOID **)&psVAO->asVAOState[AP_WEIGHTARRAY].pui8Pointer;
			break;
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
	    default:
		    SetError(gc, GL_INVALID_ENUM);
			break;
	}

	/* PRQA S 0311 -- */

	GLES1_TIME_STOP(GLES1_TIMES_glGetPointerv);
}

/***********************************************************************************
 Function Name      : glIsEnabled
 Inputs             : cap
 Outputs            : -
 Returns            : Is state enabled
 Description        : ENTRYPOINT: Returns the enable bit for a piece of state 
					  indicated by pname
************************************************************************************/
GL_API GLboolean GL_APIENTRY glIsEnabled(GLenum cap)
{
	GLboolean bIsEnabled;

	__GLES1_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsEnabled"));

	GLES1_TIME_START(GLES1_TIMES_glIsEnabled);
	
	bIsEnabled = IsEnabled(gc, cap);

	GLES1_TIME_STOP(GLES1_TIMES_glIsEnabled);

	return bIsEnabled;
}




/***********************************************************************************
 Function Name      : glGetIntegerv
 Inputs             : pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of [static] state indicated by pname
************************************************************************************/
GL_API void GL_APIENTRY glGetIntegerv(GLenum pname, GLint *params)
{
    __GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetIntegerv"));

	GLES1_TIME_START(GLES1_TIMES_glGetIntegerv);

	DoGet(gc, pname, params, GLES1_INT32);

	GLES1_TIME_STOP(GLES1_TIMES_glGetIntegerv);
}


/***********************************************************************************
 Function Name      : glGetError
 Inputs             : -
 Outputs            : -
 Returns            : Current error
 Description        : ENTRYPOINT: Returns current error. Resets error state.
************************************************************************************/
GL_API GLenum GL_APIENTRY glGetError(void)
{
    GLenum eError;
    __GLES1_GET_CONTEXT_RETURN(GL_NO_ERROR);

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetError"));

	GLES1_TIME_START(GLES1_TIMES_glGetError);

    eError = (GLenum)gc->i32Error;

    gc->i32Error = GL_NO_ERROR;

	GLES1_TIME_STOP(GLES1_TIMES_glGetError);

    return eError;
}


/***********************************************************************************
 Function Name      : glGetString
 Inputs             : pname
 Outputs            : -
 Returns            : Appropriate static string
 Description        : ENTRYPOINT: Returns a static string indicated by pname.
************************************************************************************/
GL_API const GLubyte * GL_APIENTRY glGetString(GLenum name)
{
	if (name==IMG_OGLES1_FUNCTION_TABLE)
	{
		return (const GLubyte *)&sGLES1FunctionTable;
	}
	else
	{
		const GLubyte *pszReturnString;
		__GLES1_GET_CONTEXT_RETURN(IMG_NULL);

		PVR_DPF((PVR_DBG_CALLTRACE,"glGetString"));

		GLES1_TIME_START(GLES1_TIMES_glGetString);

		switch (name) 
		{
			case GL_VENDOR:
			{
				pszReturnString = (const GLubyte *)pszVendor;

				break;
			}
			case GL_RENDERER:
			{
				pszReturnString = (const GLubyte *)pszRenderer;		

				break;
			}
			case GL_VERSION:
			{
				pszReturnString = (const GLubyte *)pszVersion;

				break;
			}
			case GL_EXTENSIONS:
			{
				pszReturnString = (const GLubyte *)gc->pszExtensions;

				break;
			}
			default:
			{
				pszReturnString = (const GLubyte *)0;

				PVR_DPF((PVR_DBG_ERROR, "glGetString: Unknown name"));

				SetError(gc, GL_INVALID_ENUM);

				break;
			}
	    }

		GLES1_TIME_STOP(GLES1_TIMES_glGetString);

		return pszReturnString;
	}
}

/******************************************************************************
 End of file (get.c)
******************************************************************************/


