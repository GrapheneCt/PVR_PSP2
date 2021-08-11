/**************************************************************************
 * Name         : get.c
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
 * $Log: get.c $
 * ./
 *  --- Revision Logs Removed --- 
 *
 **************************************************************************/

#include "context.h"
#include "pvrversion.h"

#include <string.h>


extern const IMG_OGLES2EGL_Interface sGLES2FunctionTable;

static const IMG_CHAR * const pszVendor = "Imagination Technologies";

#if defined(SGX535) || defined(SGX535_V1_1)

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

                        	IMG_INTERNAL const IMG_CHAR * const pszRenderer = "PowerVR SGX 541MP";

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

static const IMG_CHAR * const pszVersion = "OpenGL ES 2.0 build "PVRVERSION_STRING_SHORT;

static const IMG_CHAR * const pszLanguageVersion = "OpenGL ES GLSL ES 1.00 build "PVRVERSION_STRING_SHORT;

static GLboolean IsEnabled(GLES2Context *gc, GLenum eCap);

/***********************************************************************************
 Function Name      : IsEnabled
 Inputs             : gc, eCap
 Outputs            : -
 Returns            : Enable bit
 Description        : UTILITY: Returns an enable bit for state indicated by eCap
************************************************************************************/
static GLboolean IsEnabled(GLES2Context *gc, GLenum eCap)
{
    GLuint bit;

    switch (eCap) 
	{
		case GL_BLEND:
		{
			bit	= gc->ui32Enables & GLES2_ALPHABLEND_ENABLE;
			break;
		}
		case GL_CULL_FACE:
		{
			bit = gc->ui32Enables & GLES2_CULLFACE_ENABLE;
			break;
		}
		case GL_DEPTH_TEST:
		{
			bit	= gc->ui32Enables & GLES2_DEPTHTEST_ENABLE;
			break;
		}
		case GL_DITHER:
		{
			bit = gc->ui32Enables & GLES2_DITHER_ENABLE;
			break;
		}
		case GL_SCISSOR_TEST:
		{
			bit = gc->ui32Enables & GLES2_SCISSOR_ENABLE;
			break;
		}
		case GL_STENCIL_TEST:
		{
			bit = gc->ui32Enables & GLES2_STENCILTEST_ENABLE;
			break;
		}
		case GL_POLYGON_OFFSET_FILL:
		{
			bit	= gc->ui32Enables & GLES2_POLYOFFSET_ENABLE;
			break;
		}
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
		{
			bit = gc->ui32Enables & GLES2_MSALPHACOV_ENABLE;
			break;
		}
		case GL_SAMPLE_COVERAGE:
		{
			bit = gc->ui32Enables & GLES2_MSSAMPCOV_ENABLE;
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			return GL_FALSE;
		}
    }

    return (GLboolean)(bit!=0 ? GL_TRUE : GL_FALSE);
}



/***********************************************************************************
 Function Name      : GetBlendEquation
 Inputs             : gc, ui32Shift, ui32Mask
 Outputs            : Blend equation
 Returns            : -
 Description        : UTILITY: Gets a blend factor from current state
************************************************************************************/
static IMG_UINT32 GetBlendEquation(GLES2Context *gc, IMG_UINT32 ui32Shift, IMG_UINT32 ui32Mask)
{
	IMG_UINT32 ui32BlendEquation = (gc->sState.sRaster.ui32BlendEquation & ui32Mask) >> ui32Shift;

	switch(ui32BlendEquation)
	{
		default:
		case GLES2_BLENDFUNC_ADD:
			return GL_FUNC_ADD;
			
		case GLES2_BLENDFUNC_SUBTRACT:
			return GL_FUNC_SUBTRACT;
			
		case GLES2_BLENDFUNC_REVSUBTRACT:
			return GL_FUNC_REVERSE_SUBTRACT;
	}
}

/***********************************************************************************
 Function Name      : GetBlendFactor
 Inputs             : gc, ui32Shift, ui32Mask
 Outputs            : Blend factor
 Returns            : -
 Description        : UTILITY: Gets a blend factor from current state
************************************************************************************/
static IMG_UINT32 GetBlendFactor(GLES2Context *gc, IMG_UINT32 ui32Shift, IMG_UINT32 ui32Mask)
{
	IMG_UINT32 ui32BlendFactor = (gc->sState.sRaster.ui32BlendFactor & ui32Mask) >> ui32Shift;

	switch(ui32BlendFactor)
	{
		default:
		case GLES2_BLENDFACTOR_ZERO:
			return GL_ZERO;
			
		case GLES2_BLENDFACTOR_ONE:
			return GL_ONE;
			
		case GLES2_BLENDFACTOR_DSTCOLOR:
			return GL_DST_COLOR;
			
		case GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR:
			return GL_ONE_MINUS_DST_COLOR;
			
		case GLES2_BLENDFACTOR_SRCCOLOR:
			return GL_SRC_COLOR;
			
		case GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR:
			return GL_ONE_MINUS_SRC_COLOR;
			
		case GLES2_BLENDFACTOR_SRCALPHA:
			return GL_SRC_ALPHA;
			
		case GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA:
			return GL_ONE_MINUS_SRC_ALPHA;
			
		case GLES2_BLENDFACTOR_DSTALPHA:
			return GL_DST_ALPHA;
			
		case GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA:
			return GL_ONE_MINUS_DST_ALPHA;
			
		case GLES2_BLENDFACTOR_CONSTCOLOR:
			return GL_CONSTANT_COLOR;
			
		case GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR:
			return GL_ONE_MINUS_CONSTANT_COLOR;
			
		case GLES2_BLENDFACTOR_CONSTALPHA:
			return GL_CONSTANT_ALPHA;
			
		case GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA:
			return GL_ONE_MINUS_CONSTANT_ALPHA;
			
		case GLES2_BLENDFACTOR_SRCALPHA_SATURATE:
			return GL_SRC_ALPHA_SATURATE;
			
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
			return GL_KEEP;
		case EURASIA_ISPC_SOP_ZERO:
			return GL_ZERO;
		case EURASIA_ISPC_SOP_REPLACE:
			return GL_REPLACE;
		case EURASIA_ISPC_SOP_INCSAT:
			return GL_INCR;
		case EURASIA_ISPC_SOP_DECSAT:
			return GL_DECR;
		case EURASIA_ISPC_SOP_INC:
			return GL_INCR_WRAP;
		case EURASIA_ISPC_SOP_DEC:
			return GL_DECR_WRAP;
		case EURASIA_ISPC_SOP_INVERT:
			return GL_INVERT;
	}
}

/***********************************************************************************
 Function Name      : DoGet
 Inputs             : gc, eQuery, ui32Type, pszProcName
 Outputs            : pvResult
 Returns            : -
 Description        : Returns some GL state and coverts to the type specified.
************************************************************************************/
static IMG_VOID DoGet(GLES2Context *gc, GLenum eQuery, IMG_VOID *pvResult, IMG_UINT32 ui32Type)
{
    GLint itemp[16], *ip = itemp;			  /* NOTE: for ints */
	IMG_UINT32 etemp[16], *ep = etemp;		  /* NOTE: for enums */
    IMG_FLOAT ftemp[16], *fp = ftemp;		  /* NOTE: for floats */
	IMG_FLOAT ctemp[16], *cp = ctemp;		  /* NOTE: for colors */
    GLboolean ltemp[16], *lp = ltemp;		  /* NOTE: for logicals */

    switch (eQuery) 
	{
		case GL_BLEND:
		case GL_CULL_FACE:
		case GL_DEPTH_TEST:
		case GL_DITHER:
		case GL_SCISSOR_TEST:
		case GL_STENCIL_TEST:
		case GL_POLYGON_OFFSET_FILL:
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
		case GL_SAMPLE_COVERAGE:
		{
			*lp++ = IsEnabled(gc, eQuery);
			break;
		}	
		case GL_ACTIVE_TEXTURE:
		{
			*ep++ = gc->sState.sTexture.ui32ActiveTexture + GL_TEXTURE0;
			break;
		}	
		case GL_UNPACK_ALIGNMENT:
		{
			*ip++ = (IMG_INT32)gc->sState.sClientPixel.ui32UnpackAlignment;
			break;
		}	
		case GL_PACK_ALIGNMENT:
		{
			*ip++ = (IMG_INT32)gc->sState.sClientPixel.ui32PackAlignment;
			break;
		}
		case GL_LINE_WIDTH:
		{
			*fp++ = gc->sState.sLine.fWidth;
			break;
		}	
		case GL_SAMPLE_COVERAGE_VALUE:
		{
			*fp++ = gc->sState.sMultisample.fSampleCoverageValue;
			break;
		}	
		case GL_SAMPLE_COVERAGE_INVERT:
		{
			*lp++ = (GLboolean)gc->sState.sMultisample.bSampleCoverageInvert;
			break;
		}	
		case GL_CULL_FACE_MODE:
		{
			*ep++ = gc->sState.sPolygon.eCullMode;
			break;
		}	
		case GL_FRONT_FACE:
		{
			*ep++ = gc->sState.sPolygon.eFrontFaceDirection;
			break;
		}	
		case GL_DEPTH_RANGE:
		{
			/* These get scaled like colors, to [0, 2^31-1] */
			*cp++ = gc->sState.sViewport.fZNear;
			*cp++ = gc->sState.sViewport.fZFar;
			break;
		}	
		case GL_DEPTH_WRITEMASK:
		{
			if(gc->sState.sDepth.ui32TestFunc & EURASIA_ISPA_DWRITEDIS)
				*lp++ = GL_FALSE;
			else
				*lp++ = GL_TRUE;
			break;
		}	
		case GL_DEPTH_CLEAR_VALUE:
		{
			/* This gets scaled like colors, to [0, 2^31-1] */
			*cp++ = gc->sState.sDepth.fClear;
			break;
		}	
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
			IMG_UINT32 ui32StencilFunc = (gc->sState.sStencil.ui32FFStencil & ~EURASIA_ISPC_SCMP_CLRMSK) >> 
																				EURASIA_ISPC_SCMP_SHIFT;
			*ep++ = GL_NEVER + ui32StencilFunc;
			break;
		}
		case GL_STENCIL_VALUE_MASK:
		{
			*ip++ = (IMG_INT32)(gc->sState.sStencil.ui32FFStencilCompareMaskIn & GLES2_MAX_STENCIL_VALUE);
			break;
		}
		case GL_STENCIL_FAIL:
		{
			*ep++ = GetStencilOp(gc->sState.sStencil.ui32FFStencil, EURASIA_ISPC_SOP1_SHIFT, ~EURASIA_ISPC_SOP1_CLRMSK);
			break;
		}
		case GL_STENCIL_PASS_DEPTH_FAIL:
		{
			*ep++ = GetStencilOp(gc->sState.sStencil.ui32FFStencil, EURASIA_ISPC_SOP2_SHIFT, ~EURASIA_ISPC_SOP2_CLRMSK);
			break;
		}
		case GL_STENCIL_PASS_DEPTH_PASS:
		{
			*ep++ = GetStencilOp(gc->sState.sStencil.ui32FFStencil, EURASIA_ISPC_SOP3_SHIFT, ~EURASIA_ISPC_SOP3_CLRMSK);
			break;
		}
		case GL_STENCIL_REF:
		{
			*ip++ = Clampi(gc->sState.sStencil.i32FFStencilRefIn, 0, (IMG_INT32)GLES2_MAX_STENCIL_VALUE);
			break;
		}
		case GL_STENCIL_WRITEMASK:
		{
			*ip++ = (IMG_INT32)(gc->sState.sStencil.ui32FFStencilWriteMaskIn & GLES2_MAX_STENCIL_VALUE);
			break;
		}
		case GL_STENCIL_BACK_FUNC:
		{
			IMG_UINT32 ui32StencilFunc = (gc->sState.sStencil.ui32BFStencil & ~EURASIA_ISPC_SCMP_CLRMSK) >> 
																				EURASIA_ISPC_SCMP_SHIFT;
			*ep++ = GL_NEVER + ui32StencilFunc;
			break;
		}
		case GL_STENCIL_BACK_VALUE_MASK:
		{
			*ip++ = (IMG_INT32)(gc->sState.sStencil.ui32BFStencilCompareMaskIn & GLES2_MAX_STENCIL_VALUE);
			break;
		}
		case GL_STENCIL_BACK_FAIL:
		{
			*ep++ = GetStencilOp(gc->sState.sStencil.ui32BFStencil, EURASIA_ISPC_SOP1_SHIFT, ~EURASIA_ISPC_SOP1_CLRMSK);
			break;
		}
		case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
		{
			*ep++ = GetStencilOp(gc->sState.sStencil.ui32BFStencil, EURASIA_ISPC_SOP2_SHIFT, ~EURASIA_ISPC_SOP2_CLRMSK);
			break;
		}
		case GL_STENCIL_BACK_PASS_DEPTH_PASS:
		{
			*ep++ = GetStencilOp(gc->sState.sStencil.ui32BFStencil, EURASIA_ISPC_SOP3_SHIFT, ~EURASIA_ISPC_SOP3_CLRMSK);
			break;
		}
		case GL_STENCIL_BACK_REF:
		{
			*ip++ = Clampi(gc->sState.sStencil.i32BFStencilRefIn, 0, (IMG_INT32)GLES2_MAX_STENCIL_VALUE);
			break;
		}
		case GL_STENCIL_BACK_WRITEMASK:
		{
			*ip++ = (IMG_INT32)(gc->sState.sStencil.ui32BFStencilWriteMaskIn & GLES2_MAX_STENCIL_VALUE);
			break;	
		}
		case GL_VIEWPORT:
			*ip++ = gc->sState.sViewport.i32X;
			*ip++ = gc->sState.sViewport.i32Y;
			*ip++ = (IMG_INT32)gc->sState.sViewport.ui32Width;
			*ip++ = (IMG_INT32)gc->sState.sViewport.ui32Height;
			break;
		case GL_BLEND_DST_RGB:
			*ep++ = GetBlendFactor(gc, GLES2_BLENDFACTOR_RGBDST_SHIFT, GLES2_BLENDFACTOR_RGBDST_MASK);
			break;
		case GL_BLEND_SRC_RGB:
			*ep++ = GetBlendFactor(gc, GLES2_BLENDFACTOR_RGBSRC_SHIFT, GLES2_BLENDFACTOR_RGBSRC_MASK);
			break;
		case GL_BLEND_DST_ALPHA:
			*ep++ = GetBlendFactor(gc, GLES2_BLENDFACTOR_ALPHADST_SHIFT, GLES2_BLENDFACTOR_ALPHADST_MASK);
			break;
		case GL_BLEND_SRC_ALPHA:
			*ep++ = GetBlendFactor(gc, GLES2_BLENDFACTOR_ALPHASRC_SHIFT, GLES2_BLENDFACTOR_ALPHASRC_MASK);
			break;
		case GL_BLEND_EQUATION_RGB:
			*ep++ = GetBlendEquation(gc, GLES2_BLENDFUNC_RGB_SHIFT, GLES2_BLENDFUNC_RGB_MASK);
			break;
		case GL_BLEND_EQUATION_ALPHA:
			*ep++ = GetBlendEquation(gc, GLES2_BLENDFUNC_ALPHA_SHIFT, GLES2_BLENDFUNC_ALPHA_MASK);
			break;
		case GL_BLEND_COLOR:
			*cp++ = gc->sState.sRaster.sBlendColor.fRed;
			*cp++ = gc->sState.sRaster.sBlendColor.fGreen;
			*cp++ = gc->sState.sRaster.sBlendColor.fBlue;
			*cp++ = gc->sState.sRaster.sBlendColor.fAlpha;
			break;
		case GL_SCISSOR_BOX:
			*ip++ = gc->sState.sScissor.i32ScissorX;
			*ip++ = gc->sState.sScissor.i32ScissorY;
			*ip++ = (IMG_INT32)gc->sState.sScissor.ui32ScissorWidth;
			*ip++ = (IMG_INT32)gc->sState.sScissor.ui32ScissorHeight;
			break;
		case GL_COLOR_CLEAR_VALUE:
			*cp++ = gc->sState.sRaster.sClearColor.fRed;
			*cp++ = gc->sState.sRaster.sClearColor.fGreen;
			*cp++ = gc->sState.sRaster.sClearColor.fBlue;
			*cp++ = gc->sState.sRaster.sClearColor.fAlpha;
			break;
		case GL_COLOR_WRITEMASK:
		{
			IMG_UINT32 ui32ColorMask;

			ui32ColorMask = gc->sState.sRaster.ui32ColorMask;
			
			*lp++ = (GLboolean)((ui32ColorMask & GLES2_COLORMASK_RED)   ? GL_TRUE : GL_FALSE);
			*lp++ = (GLboolean)((ui32ColorMask & GLES2_COLORMASK_GREEN) ? GL_TRUE : GL_FALSE);
			*lp++ = (GLboolean)((ui32ColorMask & GLES2_COLORMASK_BLUE)  ? GL_TRUE : GL_FALSE);
			*lp++ = (GLboolean)((ui32ColorMask & GLES2_COLORMASK_ALPHA) ? GL_TRUE : GL_FALSE);
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
			GLES2Texture *psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture]
																[GLES2_TEXTURE_TARGET_2D];		
			*ip++ = (IMG_INT32)psTex->sNamedItem.ui32Name;
			break;
		}
		case GL_TEXTURE_BINDING_CUBE_MAP:
		{
			GLES2Texture *psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture]
																[GLES2_TEXTURE_TARGET_CEM];		
			*ip++ = (IMG_INT32)psTex->sNamedItem.ui32Name;
			break;
		}
#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_BINDING_STREAM_IMG:
		{
			GLES2Texture *psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture]
																[GLES2_TEXTURE_TARGET_STREAM];		
			*ip++ = (IMG_INT32)psTex->sNamedItem.ui32Name;
			break;
		}
#endif
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_BINDING_EXTERNAL_OES:
		{
			GLES2Texture *psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture]
																[GLES2_TEXTURE_TARGET_STREAM];		
			*ip++ = (IMG_INT32)psTex->sNamedItem.ui32Name;
			break;
		}
#endif   
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
		    GLES2VertexArrayObject *psVAO = gc->sVAOMachine.psActiveVAO;
			GLES_ASSERT(psVAO);

			if(psVAO->psBoundElementBuffer)
				*ip++ = (IMG_INT32)psVAO->psBoundElementBuffer->sNamedItem.ui32Name;
			else 
				*ip++ = 0;
			break;
		}
		case GL_CURRENT_PROGRAM:
		{
			GLES2Program *psProgram = gc->sProgram.psCurrentProgram;

			if(psProgram)
				*ip++ = (IMG_INT32)psProgram->sNamedItem.ui32Name;
			else 
				*ip++ = 0;
			break;
		}
		case GL_GENERATE_MIPMAP_HINT:
			*ep++ = gc->sState.sHints.eHint[GLES2_HINT_GENMIPMAP];
			break;
		case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
			*ep++ = gc->sState.sHints.eHint[GLES2_HINT_DERIVATIVE];
			break;
			
		case GL_ALIASED_POINT_SIZE_RANGE:
			*fp++ = GLES2_POINT_SPRITE_SIZE_MIN;
			*fp++ = GLES2_POINT_SPRITE_SIZE_MAX;
			break;
		case GL_ALIASED_LINE_WIDTH_RANGE:
			*fp++ = GLES2_ALIASED_LINE_WIDTH_MIN;
			*fp++ = GLES2_ALIASED_LINE_WIDTH_MAX;
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
		case GL_MAX_VERTEX_ATTRIBS:
			*ip++ = GLES2_MAX_VERTEX_ATTRIBS;
			break;
		case GL_MAX_VERTEX_UNIFORM_VECTORS:
			*ip++ = GLES2_MAX_VERTEX_UNIFORM_VECTORS;
			break;
		case GL_MAX_VARYING_VECTORS:
			*ip++ = GLES2_MAX_VARYING_VECTORS;
			break;
		case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
			*ip++ = GLES2_MAX_TEXTURE_UNITS;
			break;
		case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
			*ip++ = GLES2_MAX_VERTEX_TEXTURE_UNITS;
			break;
		case GL_MAX_TEXTURE_IMAGE_UNITS:
			*ip++ = GLES2_MAX_TEXTURE_UNITS;
			break;
		case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
			*ip++ = GLES2_MAX_FRAGMENT_UNIFORM_VECTORS;
			break;
		case GL_MAX_TEXTURE_SIZE:
			*ip++ = GLES2_MAX_TEXTURE_SIZE;
			break;
		case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
			*ip++ = GLES2_MAX_CEM_TEXTURE_SIZE;
			break;
		case GL_MAX_VIEWPORT_DIMS:
			*ip++ = (IMG_INT32)gc->psMode->ui32MaxViewportX;
			*ip++ = (IMG_INT32)gc->psMode->ui32MaxViewportY;
			break;
		case GL_SUBPIXEL_BITS:
			*ip++ = GLES2_MAX_SUBPIXEL_BITS;
			break;
		case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
			*ip++ = 5;
			break;
		case GL_COMPRESSED_TEXTURE_FORMATS:
			*ep++ = GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
			*ep++ = GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
			*ep++ = GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
			*ep++ = GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
			*ep++ = GL_ETC1_RGB8_OES;
			break;
		case GL_SAMPLE_BUFFERS:
		{
			switch(gc->psMode->ui32AntiAliasMode)
			{
				case 0:
				default:
					*ip++ = 0;
					break;
				case IMG_ANTIALIAS_2x1:
				case IMG_ANTIALIAS_2x2:
					*ip++ = 1;
					break;
			}
			break;
		}
		case GL_SAMPLES:
		{
			switch(gc->psMode->ui32AntiAliasMode)
			{
				case 0:
				default:
					*ip++ = 0;
					break;
				case IMG_ANTIALIAS_2x1:
					*ip++ = 2;
					break;
				case IMG_ANTIALIAS_2x2:
					*ip++ = 4;
					break;
			}
			break;
		}	
		case GL_FRAMEBUFFER_BINDING:
		{
			GLES2FrameBuffer *psFrameBuffer = gc->sFrameBuffer.psActiveFrameBuffer;		

			*ip++ = (IMG_INT32)psFrameBuffer->sNamedItem.ui32Name;
			break;
		}
		case GL_RENDERBUFFER_BINDING:
		{
			GLES2RenderBuffer *psRenderBuffer = gc->sFrameBuffer.psActiveRenderBuffer;		

			if(psRenderBuffer)
				*ip++ = (IMG_INT32)(((GLES2NamedItem*)psRenderBuffer)->ui32Name);
			else
				*ip++ = 0;
			break;
		}
		case GL_MAX_RENDERBUFFER_SIZE:
		{
			*ip++ = GLES2_MAX_TEXTURE_SIZE;
			break;
		}
		case GL_IMPLEMENTATION_COLOR_READ_TYPE:
		{
			switch(gc->psReadParams->ePixelFormat)
			{	
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				case PVRSRV_PIXEL_FORMAT_ARGB8888:
				case PVRSRV_PIXEL_FORMAT_XBGR8888:
				case PVRSRV_PIXEL_FORMAT_XRGB8888:
				{
					*ep++ = GL_UNSIGNED_BYTE;
					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB4444:
				{
#if defined(GLES2_EXTENSION_READ_FORMAT)
					*ep++ = GL_UNSIGNED_SHORT_4_4_4_4_REV_IMG;
#else /* GLES2_EXTENSION_READ_FORMAT */
					*ep++ = GL_UNSIGNED_SHORT_4_4_4_4;
#endif /* GLES2_EXTENSION_READ_FORMAT */
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
		case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
		{
			switch(gc->psReadParams->ePixelFormat)
			{	
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				case PVRSRV_PIXEL_FORMAT_ARGB1555:
				case PVRSRV_PIXEL_FORMAT_XBGR8888:
				case PVRSRV_PIXEL_FORMAT_XRGB8888:
				{
					*ep++ = GL_RGBA;
					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB8888:
				case PVRSRV_PIXEL_FORMAT_ARGB4444:
				{
#if defined(GLES2_EXTENSION_READ_FORMAT)
					*ep++ = GL_BGRA_IMG;
#else
					*ep++ = GL_RGBA;
#endif/* GLES2_EXTENSION_READ_FORMAT */
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
		case GL_NUM_SHADER_BINARY_FORMATS:
		{
#if defined(SUPPORT_BINARY_SHADER)
			*ip++ = 1;
#else
			*ip++ = 0;
#endif
			break;
		}
		case GL_SHADER_BINARY_FORMATS:
		{
#if defined(SUPPORT_BINARY_SHADER)
			*ep++ = GL_SGX_BINARY_IMG;
#endif
			break;
		}
		case GL_SHADER_COMPILER:
		{
#if defined(SUPPORT_SOURCE_SHADER)
			*lp++ = GL_TRUE;
#else
			*lp++ = GL_FALSE;
#endif
			break;
		}
		case GL_NUM_PROGRAM_BINARY_FORMATS_OES:
		{
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
			*ip++ = 1;	
#else
			*ip++ = 0;
#endif
			break;
		}
		case GL_PROGRAM_BINARY_FORMATS_OES:
		{
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
			*ep++ = GL_SGX_PROGRAM_BINARY_IMG;
#endif
			break;
		}		
#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_NUM_STREAM_DEVICES_IMG:
		{
			*ip++ = (IMG_INT32)GetNumStreamDevices(gc);
			break;
		}
#endif
#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)

	    case GL_VERTEX_ARRAY_BINDING_OES:
		{
		    GLES_ASSERT(VAO(gc));
			*ip++ = (IMG_INT32)gc->sVAOMachine.psActiveVAO->sNamedItem.ui32Name;
			break;
		}	
#endif
#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
		case GL_MAX_SAMPLES_IMG:
		{
			*ip++ = 4;
			break;
		}
#endif
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			return;
		}
    }

    /* Use the motion of the pointers to type convert the result */
    if (ip != itemp) 
	{
		ConvertData(GLES_INT32, itemp, ui32Type, pvResult, (IMG_UINT32)(ip - itemp));
    }
	else if (ep != etemp) 
	{
		ConvertData(GLES_ENUM, etemp, ui32Type, pvResult, (IMG_UINT32)(ep - etemp));
	} 
	else if (fp != ftemp) 
	{
		ConvertData(GLES_FLOAT, ftemp, ui32Type, pvResult, (IMG_UINT32)(fp - ftemp));
	} 
	else if (lp != ltemp) 
	{
		ConvertData(GLES_BOOLEAN, ltemp, ui32Type, pvResult, (IMG_UINT32)(lp - ltemp));
    } 
	else if (cp != ctemp) 
	{
		ConvertData(GLES_COLOR, ctemp, ui32Type, pvResult, (IMG_UINT32)(cp - ctemp));
    } 
}

/***********************************************************************************
 Function Name      : glGetAttachedShaders
 Inputs             : program, maxcount
 Outputs            : count, shaders
 Returns            : -
 Description        : Returns the list of attached shaders for a program
************************************************************************************/

GL_APICALL void GL_APIENTRY glGetAttachedShaders(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders)
{
	GLES2Program *psProgram;
	IMG_INT32 i = 0;

	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetAttachedShaders"));

	GLES2_TIME_START(GLES2_TIMES_glGetAttachedShaders);
   
	if (maxcount < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGetAttachedShaders);
		return;
	}

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGetAttachedShaders);
		return;
	}

	if(psProgram->psVertexShader && maxcount > 0)
	{
		shaders[i++] = psProgram->psVertexShader->sNamedItem.ui32Name;
	}

	if(psProgram->psFragmentShader && maxcount > 1)
	{
		shaders[i++] = psProgram->psFragmentShader->sNamedItem.ui32Name;
	}

	/* The number of shaders attached */
	if(count) 
		*count = i;
	
	GLES2_TIME_STOP(GLES2_TIMES_glGetAttachedShaders);

}

/***********************************************************************************
 Function Name      : glGetBooleanv
 Inputs             : pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of state indicated by pname
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetBooleanv(GLenum pname, GLboolean *params)
{
 	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetBooleanv"));

	GLES2_TIME_START(GLES2_TIMES_glGetBooleanv);

	DoGet(gc, pname, params, GLES_BOOLEAN);

	GLES2_TIME_STOP(GLES2_TIMES_glGetBooleanv);
}

/***********************************************************************************
 Function Name      : glGetFloatv
 Inputs             : pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of state indicated by pname
************************************************************************************/

GL_APICALL void GL_APIENTRY glGetFloatv(GLenum pname, GLfloat *params)
{
 	__GLES2_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetFloatv"));

	GLES2_TIME_START(GLES2_TIMES_glGetFloatv);
	
	DoGet(gc, pname, params, GLES_FLOAT);
	
	GLES2_TIME_STOP(GLES2_TIMES_glGetFloatv);
}

/***********************************************************************************
 Function Name      : glGetIntegerv
 Inputs             : pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of state indicated by pname
************************************************************************************/

GL_APICALL void GL_APIENTRY glGetIntegerv(GLenum pname, GLint *params)
{
 	__GLES2_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetIntegerv"));

	GLES2_TIME_START(GLES2_TIMES_glGetIntegerv);
	
	DoGet(gc, pname, params, GLES_INT32);
	
	GLES2_TIME_STOP(GLES2_TIMES_glGetIntegerv);

}

/***********************************************************************************
 Function Name      : glGetBufferParameteriv
 Inputs             : target, pname
 Outputs            : params
 Returns            : -
 Description        : Returns information about the currently bound buffer object
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
	IMG_UINT32 ui32TargetIndex;
	GLES2BufferObject *psBufObj;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetBufferParameteriv"));

	GLES2_TIME_START(GLES2_TIMES_glGetBufferParameteriv);
	
	switch(target)
	{
		case GL_ARRAY_BUFFER:
		case GL_ELEMENT_ARRAY_BUFFER:
			ui32TargetIndex = target - GL_ARRAY_BUFFER;
			break;
		default:
			SetError(gc, GL_INVALID_ENUM);
			GLES2_TIME_STOP(GLES2_TIMES_glGetBufferParameteriv);
			return;
	}

	psBufObj = gc->sBufferObject.psActiveBuffer[ui32TargetIndex];

	if(psBufObj == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		GLES2_TIME_STOP(GLES2_TIMES_glGetBufferParameteriv);
		return;
	}

	switch(pname)
	{
		case GL_BUFFER_SIZE:
		{
			*params = (IMG_INT32)psBufObj->ui32BufferSize;

			break;
		}
		case GL_BUFFER_USAGE:
		{
			*params = (IMG_INT32)psBufObj->eUsage;

			break;
		}
		case GL_BUFFER_ACCESS_OES:
		{
			*params = (IMG_INT32)psBufObj->eAccess;

			break;
		}
		case GL_BUFFER_MAPPED_OES:
		{
			*params = psBufObj->bMapped;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			*params = 0;

			break;
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glGetBufferParameteriv);
}

/***********************************************************************************
 Function Name      : glGetBufferPointervOES
 Inputs             : target, pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Returns pointer to the currently bound buffer object
************************************************************************************/
GL_API_EXT void APIENTRY glGetBufferPointervOES(GLenum target, GLenum pname, void **params)
{
	IMG_UINT32 ui32TargetIndex;
	GLES2BufferObject *psBufObj;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetBufferParameterivOES"));

	GLES2_TIME_START(GLES2_TIMES_glGetBufferPointervOES);

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

			GLES2_TIME_STOP(GLES2_TIMES_glGetBufferPointervOES);

			return;
		}
	}

	psBufObj = gc->sBufferObject.psActiveBuffer[ui32TargetIndex];

	if(psBufObj == NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glGetBufferPointervOES);

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

	GLES2_TIME_START(GLES2_TIMES_glGetBufferPointervOES);
	
}	


/***********************************************************************************
 Function Name      : glGetError
 Inputs             : -
 Outputs            : -
 Returns            : Current error
 Description        : ENTRYPOINT: Returns current error. Resets error state.
************************************************************************************/
GL_APICALL GLenum GL_APIENTRY glGetError(void)
{
    IMG_INT32 i32Error;
    __GLES2_GET_CONTEXT_RETURN(GL_NO_ERROR);

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetError"));

	GLES2_TIME_START(GLES2_TIMES_glGetError);

    i32Error = gc->i32Error;
    gc->i32Error = 0;

	GLES2_TIME_STOP(GLES2_TIMES_glGetError);

    return (GLenum)i32Error;
}


/***********************************************************************************
 Function Name      : glGetShaderiv
 Inputs             : shader, pname
 Outputs            : params
 Returns            : -
 Description        : Returns information about a shader
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetShaderiv (GLuint shader, GLenum pname, GLint *params)
{
	GLES2Shader *psShader;

	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetShaderiv"));

	GLES2_TIME_START(GLES2_TIMES_glGetShaderiv);

	psShader = GetNamedShader(gc, shader);

	if(psShader == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGetShaderiv);
		return;
	}
   
	switch(pname)
	{
		case GL_SHADER_TYPE: 
			*params = ((psShader->ui32Type == GLES2_SHADERTYPE_VERTEX) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
			break;
			
		case GL_DELETE_STATUS: 
			*params = (GLint)(psShader->bDeleting ? GL_TRUE : GL_FALSE);
			break;
			
		case GL_COMPILE_STATUS: 
			*params = (GLint)(psShader->bSuccessfulCompile ? GL_TRUE : GL_FALSE);
			break;
			
		case GL_INFO_LOG_LENGTH:
			/* If pname is INFO_LOG_LENGTH, the length of the info log,
			   _including_a_null_terminator_, is returned.
			   If there is no info log, zero is returned.
			*/
			if(psShader->pszInfoLog)
			{
				*params = (GLint)strlen(psShader->pszInfoLog) + 1;
			}
			else
			{
				*params = 0;
			}
			break;
			
		case GL_SHADER_SOURCE_LENGTH:
			/* If pname is SHADER_SOURCE_LENGTH, the length of the concatenation
			   of the source strings making up the shader source, _including_a_null_terminator_, is
			   returned. If no source has been defined, zero is returned.
			*/
			if(psShader->pszSource)
			{
				*params = (GLint)strlen(psShader->pszSource) + 1;
			}
			else
			{
				*params = 0;
			}
			break;

		default:
			SetError(gc, GL_INVALID_ENUM);
			GLES2_TIME_STOP(GLES2_TIMES_glGetShaderiv);
			return;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glGetShaderiv);
}

/***********************************************************************************
 Function Name      : glGetProgramiv
 Inputs             : program, pname
 Outputs            : params
 Returns            : -
 Description        : Returns information about a program
************************************************************************************/

GL_APICALL void GL_APIENTRY glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
	GLES2Program *psProgram;

	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetProgramiv"));

	GLES2_TIME_START(GLES2_TIMES_glGetProgramiv);

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGetProgramiv);
		return;
	}

	switch(pname)
	{
		case GL_ATTACHED_SHADERS:
		{
			IMG_UINT32 i = 0;

			if(psProgram->psVertexShader)
			{
				i++;
			}

			if(psProgram->psFragmentShader)
			{
				i++;
			}
			*params = (GLint)i;
			break;			
		}
		case GL_ACTIVE_ATTRIBUTES:
			*params = (GLint)psProgram->ui32NumActiveAttribs;
			break;	

		case GL_ACTIVE_ATTRIBUTE_MAX_LENGTH:
			*params = (GLint)psProgram->ui32LengthOfLongestAttribName;
			break;	

		case GL_ACTIVE_UNIFORMS: 
			*params = (GLint)psProgram->ui32NumActiveUserUniforms;
			break;

		case GL_ACTIVE_UNIFORM_MAX_LENGTH: 
			*params = (GLint)psProgram->ui32LengthOfLongestUniformName;
			break;	

		case GL_DELETE_STATUS: 
			*params = (GLint)(psProgram->bDeleting ? GL_TRUE : GL_FALSE);
			break;	

		case GL_LINK_STATUS: 
			*params = (GLint)(psProgram->bSuccessfulLink ? GL_TRUE : GL_FALSE);
			break;

		case GL_VALIDATE_STATUS: 
			*params = (GLint)(psProgram->bSuccessfulValidate ? GL_TRUE : GL_FALSE);
			break;

		case GL_INFO_LOG_LENGTH:
			/* If pname is INFO_LOG_LENGTH, the length of the info log, _including_a_null_terminator_,
			   is returned. If there is no info log, 0 is returned.
			*/
			if(psProgram->pszInfoLog)
			{
				*params = (GLint)strlen(psProgram->pszInfoLog) + 1;
			}
			else
			{
				*params = 0;
			}
			break;

#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
		case GL_PROGRAM_BINARY_LENGTH_OES:	
			{
				if(!psProgram->bSuccessfulLink)
				{
					*params = 0;	
				}
				else
				{
					IMG_UINT32 length, i;
					GLSLCompiledUniflexProgram sUniflexProgramVertex, sUniflexProgramFragment;					
					GLES2SharedShaderState *psVertex, *psFragment;
					GLSLUniFlexCode        sCodeVert = {0};
					GLSLUniFlexCode        sCodeFrag = {0};	
	
					psVertex   = psProgram->sVertex.psSharedState;
					psFragment = psProgram->sFragment.psSharedState;

					/*****************************************************
						convert vertex shared state into uniflex program 
					******************************************************/
					sUniflexProgramVertex.eProgramType						= GLSLPT_VERTEX;
					sUniflexProgramVertex.eProgramFlags						= psVertex->eProgramFlags;
					sUniflexProgramVertex.bSuccessfullyCompiled				= IMG_TRUE;
					sUniflexProgramVertex.psBindingSymbolList			    = &(psVertex->sBindingSymbolList);
					
					for(i = 0; i < NUM_TC_REGISTERS; i++)
					{
						sCodeVert.auTexCoordDims[i]		  = psVertex->aui32TexCoordDims[i];
						sCodeVert.aeTexCoordPrecisions[i] = psVertex->aeTexCoordPrecisions[i];
					}
					sCodeVert.eActiveVaryingMask	= psVertex->eActiveVaryingMask;
					sCodeVert.psUniPatchInput		= PVRUniPatchCreatePCShader(gc->sProgram.pvUniPatchContext, (PUSP_SHADER)   psVertex->pvUniPatchShader);
					sCodeVert.psUniPatchInputMSAATrans = IMG_NULL;
					sUniflexProgramVertex.psUniFlexCode = &sCodeVert; 

					/*****************************************************
						convert fragment shared state into uniflex program 
					******************************************************/	
					sUniflexProgramFragment.eProgramType					= GLSLPT_FRAGMENT;
					sUniflexProgramFragment.eProgramFlags					= psFragment->eProgramFlags;
					sUniflexProgramFragment.bSuccessfullyCompiled			= IMG_TRUE;
					sUniflexProgramFragment.psBindingSymbolList				= &(psFragment->sBindingSymbolList);
					
					for(i = 0; i < NUM_TC_REGISTERS; i++)
					{
						sCodeFrag.auTexCoordDims[i]		  = psVertex->aui32TexCoordDims[i];
						sCodeFrag.aeTexCoordPrecisions[i] = psVertex->aeTexCoordPrecisions[i];
					}
					sCodeFrag.eActiveVaryingMask	   = psFragment->eActiveVaryingMask;
					sCodeFrag.psUniPatchInput		   = PVRUniPatchCreatePCShader(gc->sProgram.pvUniPatchContext, (PUSP_SHADER)   psFragment->pvUniPatchShader);
					if(psFragment->pvUniPatchShaderMSAATrans)
					{
						sCodeFrag.psUniPatchInputMSAATrans = PVRUniPatchCreatePCShader(gc->sProgram.pvUniPatchContext, (PUSP_SHADER)   psFragment->pvUniPatchShaderMSAATrans);
					}
					else
					{
						sCodeFrag.psUniPatchInputMSAATrans = IMG_NULL;
					}
					sUniflexProgramFragment.psUniFlexCode = &sCodeFrag;  
				

					/********************************************************************************************************** 
						there's a danger here that the user may have called glReleaseShaderCompiler, which will have deleted
						our function table, so the following call can segfault. So (re)initialise the compiler if we need to
 					***********************************************************************************************************/
					if(!gc->sProgram.hGLSLCompiler && !InitializeGLSLCompiler(gc))
					{
						GLES2_TIME_STOP(GLES2_TIMES_glGetProgramiv);
						return;
					}

					/********************************************************************************************************** 
						then call CreateBinaryProgram() passing in the false flag, the function will calculate the program size
						without actually creating it 
					***********************************************************************************************************/
					gc->sProgram.sGLSLFuncTable.pfnCreateBinaryProgram(&sUniflexProgramVertex, &sUniflexProgramFragment, psProgram->psUserBinding, 1, &length, (IMG_VOID *) &length, IMG_FALSE);

					/* clean up precompiled shaders */
					PVRUniPatchDestroyPCShader(gc->sProgram.pvUniPatchContext, sUniflexProgramVertex.psUniFlexCode->psUniPatchInput);
					PVRUniPatchDestroyPCShader(gc->sProgram.pvUniPatchContext, sUniflexProgramFragment.psUniFlexCode->psUniPatchInput);
					if(sUniflexProgramFragment.psUniFlexCode->psUniPatchInputMSAATrans)
					{
						PVRUniPatchDestroyPCShader(gc->sProgram.pvUniPatchContext, sUniflexProgramFragment.psUniFlexCode->psUniPatchInputMSAATrans);
					}

					*params = (GLint)length;
				}
			}		
	
			break;
#endif /* GLES2_EXTENSION_GET_PROGRAM_BINARY */

		default:
			SetError(gc, GL_INVALID_ENUM);
			GLES2_TIME_STOP(GLES2_TIMES_glGetProgramiv);
			return;

	}

	GLES2_TIME_STOP(GLES2_TIMES_glGetProgramiv);
}

/***********************************************************************************
 Function Name      : glGetProgramInfoLog
 Inputs             : program, bufsize
 Outputs            : length, infolog
 Returns            : -
 Description        : Returns log information from a program
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, char *infolog)
{
	GLES2Program *psProgram;

	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetProgramInfoLog"));

	GLES2_TIME_START(GLES2_TIMES_glGetProgramInfoLog);

	if (bufsize < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGetProgramInfoLog);
		return;
	}

	/* Even if the program is invalid, set the infolog to the empty string */
	if(length)
	{
		*length = 0;
	}

	if(infolog && bufsize > 0)
	{
		infolog[0] = '\0';
	}

	/* Now try to store the actual infolog */
	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGetShaderInfoLog);
		return;
	}

	if(infolog && bufsize > 1)
	{

		if(psProgram->pszInfoLog)
		{
			strncpy(infolog, psProgram->pszInfoLog, (IMG_UINT32)bufsize);
			/* In case the buffer is too small to store the full info log, we put a NULL at the end */
			infolog[bufsize-1] = '\0';
		}
		else
		{
			infolog[0] = '\0';
		}

		/* The actual number of characters written into infoLog,
		   _excluding_the_null_terminator_, is returned in length.
		   If length is NULL, then no length is returned.
		*/
		if(length)
		{
			*length = (GLsizei)strlen(infolog);
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glGetProgramInfoLog);
}

/***********************************************************************************
 Function Name      : glGetShaderInfoLog
 Inputs             : shader, bufsize
 Outputs            : length, infolog
 Returns            : -
 Description        : ENTRYPOINT: Gets the information log for a shader
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetShaderInfoLog (GLuint shader, GLsizei bufsize, GLsizei *length, char *infolog)
{
	GLES2Shader *psShader;
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetShaderInfoLog"));

	GLES2_TIME_START(GLES2_TIMES_glGetShaderInfoLog);
	
	if (bufsize < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGetShaderInfoLog);
		return;
	}

	/* Even if the shader is invalid, be nice and set the infolog to the empty string */
	if(length)
	{
		*length = 0;
	}

	if(infolog && bufsize > 0)
	{
		infolog[0] = '\0';
	}

	/* Now try to store the actual infolog */
	psShader = GetNamedShader(gc, shader);

	if(psShader == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGetShaderInfoLog);
		return;
	}

	if(infolog && bufsize > 1)
	{
		if(psShader->pszInfoLog)
		{
			strncpy(infolog, psShader->pszInfoLog, (IMG_UINT32)bufsize);
			/* In case the buffer is too small to store the full info log, we put a NULL at the end */
			infolog[bufsize-1] = '\0';
		}
		else
		{
			infolog[0] = '\0';
		}

		/* The actual number of characters written into infoLog,
		   _excluding_the_null_terminator_, is returned in length.
		   If length is NULL, then no length is returned.
		*/
		if(length)
		{
			*length = (GLsizei)strlen(infolog);
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glGetShaderInfoLog);
}

/***********************************************************************************
 Function Name      : glGetShaderSource
 Inputs             : shader, bufsize
 Outputs            : length, source
 Returns            : -
 Description        : ENTRYPOINT: Gets the source code for a shader
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetShaderSource (GLuint shader, GLsizei bufsize, GLsizei *length, char *source)
{
	GLES2Shader *psShader;
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetShaderSource"));

	GLES2_TIME_START(GLES2_TIMES_glGetShaderSource);
	
	if (bufsize < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGetShaderSource);
		return;
	}

	psShader = GetNamedShader(gc, shader);

	if(psShader == IMG_NULL)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndReturn;
	}

	if(source && bufsize > 1)
	{
		if(psShader->pszSource)
		{
			strncpy(source, psShader->pszSource, (IMG_UINT32)bufsize);
			/* In case the buffer is too small to store the full info log, we put a NULL at the end */
			source[bufsize-1] = '\0';
		}
		else
		{
			source[0] = '\0';
		}

		if(length)
		{
			/* The actual number of characters written into source,
			   _excluding_the_null_terminator_, is returned in length.
			*/
			*length = (GLsizei)strlen(source);
		}
	}

StopTimeAndReturn:
	GLES2_TIME_STOP(GLES2_TIMES_glGetShaderSource);
}

/***********************************************************************************
 Function Name      : glGetString
 Inputs             : name
 Outputs            : -
 Returns            : string
 Description        : Returns the static string indicated by name
************************************************************************************/
GL_APICALL const GLubyte * GL_APIENTRY glGetString(GLenum name)
{
	/* This test has to be here because when this function is called
	 * we still don't have a GL context
	 */
	if (name == IMG_OGLES2_FUNCTION_TABLE)
	{
		return (const GLubyte *)&sGLES2FunctionTable;
	}
	else
	{
		const GLubyte *pszReturnString;
		
		__GLES2_GET_CONTEXT_RETURN(IMG_NULL);

		PVR_DPF((PVR_DBG_CALLTRACE,"glGetString"));

		GLES2_TIME_START(GLES2_TIMES_glGetString);

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
			case GL_SHADING_LANGUAGE_VERSION:
			{
				pszReturnString = (const GLubyte *)pszLanguageVersion;
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

				PVR_DPF((PVR_DBG_ERROR, "glGetString unknown name"));

				SetError(gc, GL_INVALID_ENUM);

				break;
			}
	    }

		GLES2_TIME_STOP(GLES2_TIMES_glGetString);

		return pszReturnString;
	}
}


/***********************************************************************************
 Function Name      : GetTexParameter
 Inputs             : gc, eTarget, ePname
 Outputs            : 
 Returns            : Texture parameter
 Description        : UTILITY: Returns texture parameter state as indicated by ePname
************************************************************************************/
static IMG_BOOL GetTexParameter(GLES2Context *gc, GLenum eTarget, GLenum ePname, IMG_VOID *pvResult, IMG_UINT32 ui32Type)
{
	GLES2Texture *psTex;
	GLES2TextureParamState *psParamState;
	IMG_UINT32 ui32TexTarget;
	
	IMG_UINT32 etemp[16], *ep = etemp;		  /* NOTE: for enum */
    IMG_FLOAT ftemp[16], *fp = ftemp;		  /* NOTE: for floats */

	switch(eTarget)
	{
		case GL_TEXTURE_2D:
			ui32TexTarget = GLES2_TEXTURE_TARGET_2D;
			break;
		case GL_TEXTURE_CUBE_MAP:
			ui32TexTarget = GLES2_TEXTURE_TARGET_CEM;
			break;
#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_STREAM_IMG:
			ui32TexTarget = GLES2_TEXTURE_TARGET_STREAM;
			break;
#endif
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_EXTERNAL_OES:
			ui32TexTarget = GLES2_TEXTURE_TARGET_STREAM;
			break;
#endif
		default:
			SetError(gc, GL_INVALID_ENUM);
			return IMG_FALSE;
	}

	psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture][ui32TexTarget];
	psParamState = &psTex->sState;

	switch(ePname)
	{
		case GL_TEXTURE_WRAP_S:
		case GL_TEXTURE_WRAP_T:
		{
			IMG_UINT32 ui32WrapMode;
					
			if(ePname == GL_TEXTURE_WRAP_S)
			{
				ui32WrapMode = (psParamState->ui32StateWord0 & ~EURASIA_PDS_DOUTT0_UADDRMODE_CLRMSK) >> 
																EURASIA_PDS_DOUTT0_UADDRMODE_SHIFT;
			}
			else
			{
				ui32WrapMode = (psParamState->ui32StateWord0 & ~EURASIA_PDS_DOUTT0_VADDRMODE_CLRMSK) >> 
																EURASIA_PDS_DOUTT0_VADDRMODE_SHIFT;
			}

			switch(ui32WrapMode)
			{
				case EURASIA_PDS_DOUTT0_ADDRMODE_REPEAT:
				{
					*ep++ = GL_REPEAT;
					break;
				}
				case EURASIA_PDS_DOUTT0_ADDRMODE_CLAMP:
				{
					*ep++ = GL_CLAMP_TO_EDGE;
					break;
				}
				case EURASIA_PDS_DOUTT0_ADDRMODE_FLIP:
				{
					*ep++ = GL_MIRRORED_REPEAT;
					break;
				}
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
					*ep ++ = GL_LINEAR;
					break;
				}
				case (EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX | EURASIA_PDS_DOUTT0_MINFILTER_POINT):
				{
					*ep ++ = GL_NEAREST_MIPMAP_NEAREST;
					break;
				}
				case (EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX | EURASIA_PDS_DOUTT0_MINFILTER_LINEAR):
				{
					*ep ++ = GL_LINEAR_MIPMAP_NEAREST;
					break;
				}
				case (EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX | EURASIA_PDS_DOUTT0_MINFILTER_POINT |	EURASIA_PDS_DOUTT0_MIPFILTER):
				{
					*ep ++ = GL_NEAREST_MIPMAP_LINEAR;
					break;
				}
				case (EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX | EURASIA_PDS_DOUTT0_MINFILTER_LINEAR | EURASIA_PDS_DOUTT0_MIPFILTER):
				{
					*ep ++ = GL_LINEAR_MIPMAP_LINEAR;
					break;
				}
			}
			break;
		}
		case GL_TEXTURE_MAG_FILTER:
		{
			switch(psParamState->ui32MagFilter)
			{	
				case EURASIA_PDS_DOUTT0_MAGFILTER_POINT:
				{
					*ep ++ = GL_NEAREST;
					break;
				}	
				case EURASIA_PDS_DOUTT0_MAGFILTER_LINEAR:
				{
					*ep ++ = GL_LINEAR;
					break;
				}
			}
			break;
		}
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_REQUIRED_TEXTURE_IMAGE_UNITS_OES:
		{
			*fp ++ = 1.0;
			break;
		}
#endif
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			return IMG_FALSE;
		}
	}

	/* Use the motion of the pointers to type convert the result */
	if (ep != etemp) 
	{
		ConvertData(GLES_ENUM, etemp, ui32Type, pvResult, (IMG_UINT32)(ep - etemp));
	} 
	else if (fp != ftemp) 
	{
		ConvertData(GLES_FLOAT, ftemp, ui32Type, pvResult, (IMG_UINT32)(fp - ftemp));
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : glGetTexParameteriv
 Inputs             : target, pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of texture parameter state 
					  indicated by pname
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
{
	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexParameteriv"));

	GLES2_TIME_START(GLES2_TIMES_glGetTexParameteriv);

	GetTexParameter(gc, target, pname, params, GLES_INT32);
	
	GLES2_TIME_STOP(GLES2_TIMES_glGetTexParameteriv);
}


/***********************************************************************************
 Function Name      : glGetTexParameterfv
 Inputs             : target, pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Returns a piece of texture parameter state 
					  indicated by pname
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
{
	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetTexParameterfv"));
	
	GLES2_TIME_START(GLES2_TIMES_glGetTexParameterfv);

	GetTexParameter(gc, target, pname, params, GLES_FLOAT);
	
	GLES2_TIME_STOP(GLES2_TIMES_glGetTexParameterfv);
}


/***********************************************************************************
 Function Name      : glGetUniformfv
 Inputs             : program, location
 Outputs            : params
 Returns            : None
 Description        : Gets a uniform value given a location
                      
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetUniformfv(GLuint program, GLint location, GLfloat *params)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	IMG_UINT32 ui32NumFloats;

	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetUniformfv"));

	GLES2_TIME_START(GLES2_TIMES_glGetUniformfv);

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGetUniformfv);
		return;
	}

	if(!psProgram->bSuccessfulLink)
	{
		SetError(gc, GL_INVALID_OPERATION);
		GLES2_TIME_STOP(GLES2_TIMES_glGetUniformfv);
		return;
	}

	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == IMG_NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		GLES2_TIME_STOP(GLES2_TIMES_glGetUniformfv);
		return;
	}
	
	GetUniformData(gc, psProgram, psUniform, location, &ui32NumFloats, params);

	GLES2_TIME_STOP(GLES2_TIMES_glGetUniformfv);
}


GL_APICALL void GL_APIENTRY glGetUniformiv(GLuint program, GLint location, GLint *params)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	IMG_UINT32 ui32NumFloats, i;
	IMG_FLOAT afData[16];

	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetUniformiv"));

	GLES2_TIME_START(GLES2_TIMES_glGetUniformiv);

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGetUniformiv);
		return;
	}

	if(!psProgram->bSuccessfulLink)
	{
		SetError(gc, GL_INVALID_OPERATION);
		GLES2_TIME_STOP(GLES2_TIMES_glGetUniformfv);
		return;
	}

	psUniform = FindUniformFromLocation(gc, psProgram, location);

	if(psUniform == IMG_NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);
		GLES2_TIME_STOP(GLES2_TIMES_glGetUniformfv);
		return;
	}
	
	GetUniformData(gc, psProgram, psUniform, location, &ui32NumFloats, afData);
	
	/* convert all data to interger */
	for(i = 0; i < ui32NumFloats; i++)
	{
		params[i] = (GLint) afData[i];
	}

	GLES2_TIME_STOP(GLES2_TIMES_glGetUniformiv);
}


GL_APICALL int GL_APIENTRY glGetAttribLocation(GLuint program, const char *name)
{
	GLES2Program *psProgram;
	IMG_UINT32 i;

	__GLES2_GET_CONTEXT_RETURN(-1);
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetAttribLocation"));

	GLES2_TIME_START(GLES2_TIMES_glGetAttribLocation);

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGetAttribLocation);

		return -1;
	}

	if(!psProgram->bSuccessfulLink)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glGetAttribLocation);

		return -1;
	}

	/*
	// gl_ prefix is reserved
	*/
	if(strlen(name)>=3)
	{
		if ((name[0] == 'g') &&
			(name[1] == 'l') &&
			(name[2] == '_'))
		{
			GLES2_TIME_STOP(GLES2_TIMES_glGetAttribLocation);

			return -1;
		}
	}

	for(i = 0; i < psProgram->ui32NumActiveAttribs; i++)
	{
		if(!strcmp(name, psProgram->psActiveAttributes[i].psSymbolVP->pszName))
		{
			GLES2_TIME_STOP(GLES2_TIMES_glGetAttribLocation);

			return psProgram->psActiveAttributes[i].i32Index;
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glGetAttribLocation);

	return -1;
}


/***********************************************************************************
 Function Name      : glGetVertexAttribfv
 Inputs             : index, pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Gets attrib state specified by pname, for
					  attribute number index
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)
{
    /* BE CERTAIN TO:
	   get vertex attrib info from the current VAO instead of VAOMachine,
	   since the info was passed from VAO to VAOMachine only in draw call */

	GLES2VertexArrayObject        *psVAO;
	GLES2AttribArrayPointerState  *psAttribState;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetVertexAttribfv"));

	GLES2_TIME_START(GLES2_TIMES_glGetVertexAttribfv);
	
	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGetVertexAttribfv);
		return;
	}

	/* Setup VAO and attribute state */
	psVAO = gc->sVAOMachine.psActiveVAO;

	GLES_ASSERT(psVAO);

	psAttribState = &(psVAO->asVAOState[AP_VERTEX_ATTRIB0 + index]);

	switch(pname)
	{
		case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
		{
		    *params = (IMG_FLOAT)((psVAO->ui32CurrentArrayEnables & (VARRAY_ATTRIB0_ENABLE << index)) ? GL_TRUE : GL_FALSE);
			break;
		}
		case GL_VERTEX_ATTRIB_ARRAY_SIZE:
		{
			*params = (IMG_FLOAT)(psAttribState->ui32StreamTypeSize >> GLES2_STREAMSIZE_SHIFT);
			break;
		}
		case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
		{
			*params = (IMG_FLOAT)psAttribState->ui32UserStride;
			break;
		}
		case GL_VERTEX_ATTRIB_ARRAY_TYPE:
		{
			switch(psAttribState->ui32StreamTypeSize & GLES2_STREAMTYPE_MASK)
			{
				case GLES2_STREAMTYPE_BYTE:
					*params = GL_BYTE;
					break;
				case GLES2_STREAMTYPE_SHORT:
					*params = GL_SHORT;
					break;
				case GLES2_STREAMTYPE_UBYTE:
					*params = GL_UNSIGNED_BYTE;
					break;
				case GLES2_STREAMTYPE_USHORT:
					*params = GL_UNSIGNED_SHORT;
					break;
				case GLES2_STREAMTYPE_FLOAT:
					*params = GL_FLOAT;
					break;
				case GLES2_STREAMTYPE_HALFFLOAT:
					*params = GL_HALF_FLOAT_OES;
					break;
				case GLES2_STREAMTYPE_FIXED:		
					*params = GL_FIXED;
					break;
			}
			break;
		}
		case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
		{
			if(psAttribState->ui32StreamTypeSize & GLES2_STREAMNORM_BIT)
				*params = 1.0f;
			else
				*params = 0.0f;
			break;
		}
		case GL_CURRENT_VERTEX_ATTRIB:
		{
			params[0] = gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fX;
			params[1] = gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fY;
			params[2] = gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fZ;
			params[3] = gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index].fW;
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			GLES2_TIME_STOP(GLES2_TIMES_glGetVertexAttribfv);
			return;
		}
	}
	
	GLES2_TIME_STOP(GLES2_TIMES_glGetVertexAttribfv);
}


/***********************************************************************************
 Function Name      : glGetVertexAttribiv
 Inputs             : index, pname
 Outputs            : params
 Returns            : -
 Description        : ENTRYPOINT: Gets attrib state specified by pname, for	
                      attribute number index
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
    /* BE CERTAIN TO:
	   get vertex attrib info from the current VAO instead of VAOMachine,
	   since the info was passed from VAO to VAOMachine only in draw call */

	GLES2VertexArrayObject        *psVAO;
	GLES2AttribArrayPointerState  *psAttribState;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetVertexAttribiv"));

	GLES2_TIME_START(GLES2_TIMES_glGetVertexAttribiv);
	
	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGetVertexAttribiv);
		return;
	}

	/* Setup VAO and attribute state */
	psVAO = gc->sVAOMachine.psActiveVAO;

	GLES_ASSERT(psVAO);

	psAttribState = &(psVAO->asVAOState[AP_VERTEX_ATTRIB0 + index]);

	switch(pname)
	{
		case GL_VERTEX_ATTRIB_ARRAY_ENABLED:
	{
		    *params = (GLint)((psVAO->ui32CurrentArrayEnables & (VARRAY_ATTRIB0_ENABLE << index)) ? GL_TRUE : GL_FALSE);
			break;
		}
		case GL_VERTEX_ATTRIB_ARRAY_SIZE:
		{
			*params = (GLint)(psAttribState->ui32StreamTypeSize >> GLES2_STREAMSIZE_SHIFT);
			break;
		}
		case GL_VERTEX_ATTRIB_ARRAY_STRIDE:
		{
			*params = (GLint)psAttribState->ui32UserStride;
			break;
		}
		case GL_VERTEX_ATTRIB_ARRAY_TYPE:
		{
			switch(psAttribState->ui32StreamTypeSize & GLES2_STREAMTYPE_MASK)
			{
				case GLES2_STREAMTYPE_BYTE:
				{
					*params = GL_BYTE;
					break;
				}
				case GLES2_STREAMTYPE_SHORT:
				{
					*params = GL_SHORT;
					break;
				}
				case GLES2_STREAMTYPE_UBYTE:
				{
					*params = GL_UNSIGNED_BYTE;
					break;
				}
				case GLES2_STREAMTYPE_USHORT:
				{
					*params = GL_UNSIGNED_SHORT;
					break;
				}
				case GLES2_STREAMTYPE_FLOAT:
				{
					*params = GL_FLOAT;
					break;
				}
				case GLES2_STREAMTYPE_HALFFLOAT:
				{
					*params = GL_HALF_FLOAT_OES;
					break;
				}
				case GLES2_STREAMTYPE_FIXED:		
				{
					*params = GL_FIXED;
					break;
				}
			}
			break;
		}
		case GL_VERTEX_ATTRIB_ARRAY_NORMALIZED:
		{
			if(psAttribState->ui32StreamTypeSize & GLES2_STREAMNORM_BIT)
				*params = GL_TRUE;
			else
				*params = GL_FALSE;
			break;
		}
		case GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING:
		{
			if(psAttribState->psBufObj)
				*params = (GLint)psAttribState->psBufObj->sNamedItem.ui32Name;
			else 
				*params = 0;
			break;
		}
		
		case GL_CURRENT_VERTEX_ATTRIB:
		{
		    ConvertData(GLES_FLOAT, &(gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + index]), GLES_INT32, params, 4);
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			GLES2_TIME_STOP(GLES2_TIMES_glGetVertexAttribiv);
			return;
		}
	}
	
	GLES2_TIME_STOP(GLES2_TIMES_glGetVertexAttribiv);
}


/***********************************************************************************
 Function Name      : glGetVertexAttribPointerv
 Inputs             : index, pname
 Outputs            : pointer
 Returns            : -
 Description        : ENTRYPOINT: Gets attrib pointer specified by index
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
    /* BE CERTAIN TO:
	   get vertex attrib info from the current VAO instead of VAOMachine,
	   since the info was passed from VAO to VAOMachine only in draw call */

    GLES2VertexArrayObject        *psVAO;
	GLES2AttribArrayPointerState  *psAttribState;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetVertexAttribPointerv"));

	GLES2_TIME_START(GLES2_TIMES_glGetVertexAttribPointerv);
	
	if(index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGetVertexAttribPointerv);
		return;
	}

	if(pname != GL_VERTEX_ATTRIB_ARRAY_POINTER)
	{
		SetError(gc, GL_INVALID_ENUM);
		GLES2_TIME_STOP(GLES2_TIMES_glGetVertexAttribPointerv);
		return;
	}

	/* Setup VAO and attribute state */
	psVAO = gc->sVAOMachine.psActiveVAO;

	GLES_ASSERT(psVAO);

	psAttribState = &(psVAO->asVAOState[AP_VERTEX_ATTRIB0 + index]);

	*pointer = *((IMG_VOID **)&psAttribState->pui8Pointer);
	
	GLES2_TIME_STOP(GLES2_TIMES_glGetVertexAttribPointerv);

}


/***********************************************************************************
 Function Name      : glIsEnabled
 Inputs             : cap
 Outputs            : -
 Returns            : Is state enabled
 Description        : ENTRYPOINT: Returns the enable bit for a piece of state 
					  indicated by pname
************************************************************************************/
GL_APICALL GLboolean GL_APIENTRY glIsEnabled(GLenum cap)
{
	GLboolean bIsEnabled;

	__GLES2_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsEnabled"));

	GLES2_TIME_START(GLES2_TIMES_glIsEnabled);
	
	bIsEnabled = IsEnabled(gc, cap);

	GLES2_TIME_STOP(GLES2_TIMES_glIsEnabled);

	return bIsEnabled;
}


/***********************************************************************************
 Function Name      : glIsBuffer
 Inputs             : buffer
 Outputs            : -
 Returns            : true/false
 Description        : Returns whether a named object is a buffer
************************************************************************************/
GL_APICALL GLboolean GL_APIENTRY glIsBuffer(GLuint buffer)
{
    GLES2BufferObject *psBuf;
	GLES2NamesArray   *psNamesArray;

	__GLES2_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsBuffer"));

	GLES2_TIME_START(GLES2_TIMES_glIsBuffer);

	if (buffer==0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsBuffer);
		return GL_FALSE;
	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_BUFOBJ];

	psBuf = (GLES2BufferObject *) NamedItemAddRef(psNamesArray, buffer);

	if (psBuf==IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsBuffer);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psBuf);

	GLES2_TIME_STOP(GLES2_TIMES_glIsBuffer);

	return GL_TRUE;
}


/***********************************************************************************
 Function Name      : glIsProgram
 Inputs             : program
 Outputs            : -
 Returns            : true/false
 Description        : Returns whether a named object is a program
************************************************************************************/
GL_APICALL GLboolean GL_APIENTRY glIsProgram(GLuint program)
{
    GLES2Program *psProgram;
	GLES2NamesArray *psNamesArray;

	__GLES2_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsProgram"));

	GLES2_TIME_START(GLES2_TIMES_glIsProgram);

	if (program==0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsProgram);
		return GL_FALSE;
	}
	
	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM];

	/*
	** Retrieve the program object from the psNamesArray structure.
	*/
	psProgram = (GLES2Program *) NamedItemAddRef(psNamesArray, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsProgram);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psProgram);

	if(psProgram->ui32Type != GLES2_SHADERTYPE_PROGRAM)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsProgram);
		return GL_FALSE;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glIsProgram);

	return GL_TRUE;
}


/***********************************************************************************
 Function Name      : glIsShader
 Inputs             : shader
 Outputs            : -
 Returns            : true/false
 Description        : Returns whether a named object is a shader
************************************************************************************/
GL_APICALL GLboolean GL_APIENTRY glIsShader(GLuint shader)
{
    GLES2Shader *psShader;
	GLES2NamesArray *psNamesArray;

	__GLES2_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsShader"));

	GLES2_TIME_START(GLES2_TIMES_glIsShader);

	if (shader==0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsShader);
		return GL_FALSE;
	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM];

	/*
	** Retrieve the shader object from the psNamesArray structure.
	*/
	psShader = (GLES2Shader *) NamedItemAddRef(psNamesArray, shader);

	if(psShader == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsShader);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psShader);

	if(psShader->ui32Type == GLES2_SHADERTYPE_PROGRAM)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsShader);
		return GL_FALSE;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glIsShader);
	
	return GL_TRUE;
}


/***********************************************************************************
 Function Name      : glIsTexture
 Inputs             : texture
 Outputs            : -
 Returns            : true/false
 Description        : Returns whether a named object is a texture
************************************************************************************/
GL_APICALL GLboolean GL_APIENTRY glIsTexture(GLuint texture)
{
    GLES2Texture    *psTex;
	GLES2NamesArray *psNamesArray;

	__GLES2_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glIsTexture"));

	GLES2_TIME_START(GLES2_TIMES_glIsTexture);

	if (texture==0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsTexture);
		return GL_FALSE;
	}
	
	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ];

	psTex = (GLES2Texture *) NamedItemAddRef(psNamesArray, texture);

	if (psTex==IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glIsTexture);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psTex);

	GLES2_TIME_STOP(GLES2_TIMES_glIsTexture);

	return GL_TRUE;
}


/***********************************************************************************
 Function Name      : glGetShaderPrecisionFormat
 Inputs             : shadertype, precisiontype
 Outputs            : range, precision
 Returns            : -
 Description        : Returns range+ precision of a type within a shader
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetShaderPrecisionFormat(GLenum shadertype, GLenum precisiontype, 
													   GLint *range, GLint *precision)
{
	__GLES2_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetShaderPrecisionFormat"));

	GLES2_TIME_START(GLES2_TIMES_glGetShaderPrecisionFormat);

	switch(shadertype)
	{
		case GL_FRAGMENT_SHADER:
		case GL_VERTEX_SHADER:
		{
			switch(precisiontype)
			{
				case GL_LOW_FLOAT:
				{
					range[0]   = 1;
					range[1]   = 1;

					*precision = 8;

					break;
				}
				case GL_MEDIUM_FLOAT:
				{
					range[0]   = 14;
					range[1]   = 14;

					*precision = 10;

					break;
				}
				case GL_HIGH_FLOAT:
				{
					range[0]   = 126;
					range[1]   = 126;

					*precision = 23;

					break;
				}
				case GL_LOW_INT:
				{
					range[0]   = 8;
					range[1]   = 8;

					*precision = 0;

					break;
				}
				case GL_MEDIUM_INT:
				{
					range[0]   = 11;
					range[1]   = 11;

					*precision = 0;

					break;
				}
				case GL_HIGH_INT:
				{
					range[0]   = 24;
					range[1]   = 24;

					*precision = 0;

					break;
				}
				default:
				{
					GLES2_TIME_STOP(GLES2_TIMES_glGetShaderPrecisionFormat);

					SetError(gc, GL_INVALID_ENUM);

					return;
				}
			}

			break;
		}
		default:
		{
			GLES2_TIME_STOP(GLES2_TIMES_glGetShaderPrecisionFormat);

			SetError(gc, GL_INVALID_ENUM);

			return;
		}
	}
}



