/**************************************************************************
 * Name         : state.c
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
 * $Log: state.c $
 *
 **************************************************************************/

#include "context.h"


/***********************************************************************************
 Function Name      : glBlendColor
 Inputs             : red, green, blue, alpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets constant blend color state
************************************************************************************/
GL_APICALL void GL_APIENTRY glBlendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	IMG_UINT32 ui32BlendColor;

    __GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBlendColor"));

	GLES2_TIME_START(GLES2_TIMES_glBlendColor);

	gc->sState.sRaster.sBlendColor.fRed = Clampf(red, GLES2_Zero, GLES2_One);
	gc->sState.sRaster.sBlendColor.fGreen = Clampf(green, GLES2_Zero, GLES2_One);
	gc->sState.sRaster.sBlendColor.fBlue = Clampf(blue, GLES2_Zero, GLES2_One);
	gc->sState.sRaster.sBlendColor.fAlpha = Clampf(alpha, GLES2_Zero, GLES2_One);

	ui32BlendColor = ColorConvertToHWFormat(&gc->sState.sRaster.sBlendColor);

	if(	gc->sState.sRaster.ui32BlendColor != ui32BlendColor)
	{
		gc->sState.sRaster.ui32BlendColor = ColorConvertToHWFormat(&gc->sState.sRaster.sBlendColor);

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_FRAGPROG_CONSTANTS;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glBlendColor);
}


/***********************************************************************************
 Function Name      : glBlendEquation
 Inputs             : mode
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets back end blend equation for both RGB and alpha
					  blending.
************************************************************************************/
GL_APICALL void GL_APIENTRY glBlendEquation( GLenum mode )
{
	IMG_UINT32 ui32BlendEquation;
    
	__GLES2_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glBlendEquation"));

	GLES2_TIME_START(GLES2_TIMES_glBlendEquation);

    switch (mode) 
	{
		case GL_FUNC_ADD:
		{
			ui32BlendEquation =	(GLES2_BLENDFUNC_ADD << GLES2_BLENDFUNC_RGB_SHIFT) | 
								(GLES2_BLENDFUNC_ADD << GLES2_BLENDFUNC_ALPHA_SHIFT);
			break;
		}
		case GL_FUNC_SUBTRACT:
		{
			ui32BlendEquation =	(GLES2_BLENDFUNC_SUBTRACT << GLES2_BLENDFUNC_RGB_SHIFT) | 
								(GLES2_BLENDFUNC_SUBTRACT << GLES2_BLENDFUNC_ALPHA_SHIFT);
			break;
		}
		case GL_FUNC_REVERSE_SUBTRACT:
		{
			ui32BlendEquation =	(GLES2_BLENDFUNC_REVSUBTRACT << GLES2_BLENDFUNC_RGB_SHIFT) | 
								(GLES2_BLENDFUNC_REVSUBTRACT << GLES2_BLENDFUNC_ALPHA_SHIFT);
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glBlendEquation);

			return;
		}
    }

	if(	gc->sState.sRaster.ui32BlendEquation != ui32BlendEquation)
	{
		gc->sState.sRaster.ui32BlendEquation = ui32BlendEquation;

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glBlendEquation);
}


/***********************************************************************************
 Function Name      : glBlendEquationSeparate
 Inputs             : modeRGB, modeAlpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets seperate back end blend equations for both 
					  RGB and alpha blending.
************************************************************************************/
GL_APICALL void	GL_APIENTRY glBlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
	IMG_UINT32 ui32RGBBlendEquation, ui32AlphaBlendEquation;
    
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBlendEquationSeparate"));

	GLES2_TIME_START(GLES2_TIMES_glBlendEquationSeparate);

    switch (modeRGB) 
	{
		case GL_FUNC_ADD:
		{
			ui32RGBBlendEquation = GLES2_BLENDFUNC_ADD << GLES2_BLENDFUNC_RGB_SHIFT;

			break;
		}
		case GL_FUNC_SUBTRACT:
		{
			ui32RGBBlendEquation = GLES2_BLENDFUNC_SUBTRACT << GLES2_BLENDFUNC_RGB_SHIFT;

			break;
		}
		case GL_FUNC_REVERSE_SUBTRACT:
		{
			ui32RGBBlendEquation = GLES2_BLENDFUNC_REVSUBTRACT << GLES2_BLENDFUNC_RGB_SHIFT;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glBlendEquationSeparate);

			return;
		}
    }

	switch (modeAlpha) 
	{
		case GL_FUNC_ADD:
		{
			ui32AlphaBlendEquation = GLES2_BLENDFUNC_ADD << GLES2_BLENDFUNC_ALPHA_SHIFT;

			break;
		}
		case GL_FUNC_SUBTRACT:
		{
			ui32AlphaBlendEquation = GLES2_BLENDFUNC_SUBTRACT << GLES2_BLENDFUNC_ALPHA_SHIFT;

			break;
		}
		case GL_FUNC_REVERSE_SUBTRACT:
		{
			ui32AlphaBlendEquation = GLES2_BLENDFUNC_REVSUBTRACT << GLES2_BLENDFUNC_ALPHA_SHIFT;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glBlendEquationSeparate);

			return;
		}
    }
	
	if(	gc->sState.sRaster.ui32BlendEquation != (ui32RGBBlendEquation | ui32AlphaBlendEquation))
	{
		gc->sState.sRaster.ui32BlendEquation = ui32RGBBlendEquation | ui32AlphaBlendEquation;

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glBlendEquationSeparate);
}


/***********************************************************************************
 Function Name      : BlendFuncSeparate
 Inputs             : gc, srcRGB, dstRGB, srcAlpha, dstAlpha
 Outputs            : -
 Returns            : -
 Description        : Utility: Sets seperate back end blend factors for both 
					  RGB and alpha blending.
************************************************************************************/
static IMG_VOID BlendFuncSeparate(GLES2Context *gc, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
	IMG_UINT32 ui32Factor, ui32BlendFactor;
	IMG_UINT32 i, aui32Factor[4], aui32FactorShift[4];
    
	aui32Factor[0] = srcRGB;
	aui32Factor[1] = srcAlpha;
	aui32Factor[2] = dstRGB;
	aui32Factor[3] = dstAlpha;

	aui32FactorShift[0] = GLES2_BLENDFACTOR_RGBSRC_SHIFT;
	aui32FactorShift[1] = GLES2_BLENDFACTOR_ALPHASRC_SHIFT;
	aui32FactorShift[2] = GLES2_BLENDFACTOR_RGBDST_SHIFT;
	aui32FactorShift[3] = GLES2_BLENDFACTOR_ALPHADST_SHIFT;

	ui32BlendFactor = 0;

	for(i=0; i < 4; i++)
	{
		switch (aui32Factor[i]) 
		{
			case GL_ZERO:
			{
				ui32Factor = GLES2_BLENDFACTOR_ZERO;
				break;
			}
			case GL_ONE:
			{
				ui32Factor = GLES2_BLENDFACTOR_ONE;
				break;
			}
			case GL_DST_COLOR:
			{
				ui32Factor = GLES2_BLENDFACTOR_DSTCOLOR;
				break;
			}
			case GL_ONE_MINUS_DST_COLOR:
			{
				ui32Factor = GLES2_BLENDFACTOR_ONEMINUS_DSTCOLOR;
				break;
			}
			case GL_SRC_ALPHA:
			{
				ui32Factor = GLES2_BLENDFACTOR_SRCALPHA;

				break;
			}
			case GL_ONE_MINUS_SRC_ALPHA:
			{
				ui32Factor = GLES2_BLENDFACTOR_ONEMINUS_SRCALPHA;

				break;
			}
			case GL_DST_ALPHA:
			{
				ui32Factor = GLES2_BLENDFACTOR_DSTALPHA;

				break;
			}
			case GL_ONE_MINUS_DST_ALPHA:
			{
				ui32Factor = GLES2_BLENDFACTOR_ONEMINUS_DSTALPHA;
				break;
			}
			case GL_SRC_ALPHA_SATURATE:
			{
				/* Only allowed src alpha sat on src factors */
				if(i < 2)
				{
					ui32Factor = GLES2_BLENDFACTOR_SRCALPHA_SATURATE;
					break;
				}
				else
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}
			case GL_CONSTANT_COLOR:
			{
				ui32Factor = GLES2_BLENDFACTOR_CONSTCOLOR;
				break;
			}
			case GL_ONE_MINUS_CONSTANT_COLOR:
			{
				ui32Factor = GLES2_BLENDFACTOR_ONEMINUS_CONSTCOLOR;
				break;
			}
			case GL_CONSTANT_ALPHA:
			{
				ui32Factor = GLES2_BLENDFACTOR_CONSTALPHA;
				break;
			}
			case GL_ONE_MINUS_CONSTANT_ALPHA:
			{
				ui32Factor = GLES2_BLENDFACTOR_ONEMINUS_CONSTALPHA;
				break;
			}
			case GL_SRC_COLOR:
			{
				ui32Factor = GLES2_BLENDFACTOR_SRCCOLOR;
				break;
			}
			case GL_ONE_MINUS_SRC_COLOR:
			{
				ui32Factor = GLES2_BLENDFACTOR_ONEMINUS_SRCCOLOR;
				break;
			}
			default:
			{
				SetError(gc, GL_INVALID_ENUM);
				return;
			}
		}

		ui32BlendFactor |= (ui32Factor << aui32FactorShift[i]);
	}

	if(	gc->sState.sRaster.ui32BlendFactor != ui32BlendFactor)
	{
		gc->sState.sRaster.ui32BlendFactor = ui32BlendFactor;

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
	}
}


/***********************************************************************************
 Function Name      : glBlendFuncSeparate
 Inputs             : srcRGB, dstRGB, srcAlpha, dstAlpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets seperate back end blend factors for both 
					  RGB and alpha blending.
************************************************************************************/
GL_APICALL void GL_APIENTRY glBlendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBlendFuncSeparate"));

	GLES2_TIME_START(GLES2_TIMES_glBlendFuncSeparate);

	BlendFuncSeparate(gc, srcRGB, dstRGB, srcAlpha, dstAlpha);

	GLES2_TIME_STOP(GLES2_TIMES_glBlendFuncSeparate);
}


/***********************************************************************************
 Function Name      : glBlendFunc
 Inputs             : sfactor, dfactor
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets back end blend factors for both 
					  RGB and alpha blending.
************************************************************************************/
GL_APICALL void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBlendFunc"));

	GLES2_TIME_START(GLES2_TIMES_glBlendFunc);

	BlendFuncSeparate(gc, sfactor, dfactor, sfactor, dfactor);

	GLES2_TIME_STOP(GLES2_TIMES_glBlendFunc);
}


/***********************************************************************************
 Function Name      : glColorMask
 Inputs             : red, green, blue, alpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets colorbuffer writemask state. 
************************************************************************************/
GL_APICALL void GL_APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	IMG_UINT32 ui32ColorMask;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glColorMask"));

	GLES2_TIME_START(GLES2_TIMES_glColorMask);

	/* Store the state in the sense that mask means mask IN a plane */
	ui32ColorMask  = (alpha ? GLES2_COLORMASK_ALPHA : 0);
	ui32ColorMask |= (blue  ? GLES2_COLORMASK_BLUE : 0);
	ui32ColorMask |= (green ? GLES2_COLORMASK_GREEN : 0);
	ui32ColorMask |= (red   ? GLES2_COLORMASK_RED : 0);

	if(ui32ColorMask != gc->sState.sRaster.ui32ColorMask)
	{
		gc->sState.sRaster.ui32ColorMask = ui32ColorMask;

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glColorMask);
}


/***********************************************************************************
 Function Name      : glCullFace
 Inputs             : mode
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets primitive face(s) to cull
************************************************************************************/
GL_APICALL void GL_APIENTRY glCullFace(GLenum mode)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glCullFace"));

	GLES2_TIME_START(GLES2_TIMES_glCullFace);

	switch (mode)
	{
		case GL_FRONT:
		case GL_BACK:
		case GL_FRONT_AND_BACK:
		{
			if(gc->sState.sPolygon.eCullMode != mode)
			{
				gc->sState.sPolygon.eCullMode = mode;

				gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
			}

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
		}
    }

	GLES2_TIME_STOP(GLES2_TIMES_glCullFace);
}


/***********************************************************************************
 Function Name      : glDepthFunc
 Inputs             : func
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets depth test compare function.
************************************************************************************/
GL_APICALL void GL_APIENTRY glDepthFunc(GLenum func)
{
	IMG_UINT32 ui32TestFunc;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDepthFunc"));

	GLES2_TIME_START(GLES2_TIMES_glDepthFunc);

	if ((func < GL_NEVER) || (func > GL_ALWAYS)) 
	{
		SetError(gc, GL_INVALID_ENUM);

		GLES2_TIME_STOP(GLES2_TIMES_glDepthFunc);

		return;
    }

	ui32TestFunc = ((func - GL_NEVER) << EURASIA_ISPA_DCMPMODE_SHIFT) |
					(gc->sState.sDepth.ui32TestFunc & EURASIA_ISPA_DWRITEDIS);

	if(gc->sState.sDepth.ui32TestFunc != ui32TestFunc)
	{
		gc->sState.sDepth.ui32TestFunc = ui32TestFunc;

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glDepthFunc);
}


/***********************************************************************************
 Function Name      : glDepthMask
 Inputs             : flag
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets depthbuffer writemask state
************************************************************************************/
GL_APICALL void GL_APIENTRY glDepthMask(GLboolean flag)
{
	GLboolean bEnabled;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDepthMask"));

	GLES2_TIME_START(GLES2_TIMES_glDepthMask);

	/* This assignment is reversed because EURASIA_ISPA_DWRITEDIS represents "Writing to Z is disabled" */
	bEnabled = (GLboolean)((gc->sState.sDepth.ui32TestFunc & EURASIA_ISPA_DWRITEDIS) ? GL_FALSE : GL_TRUE);

	if(bEnabled != flag)
	{
		if(flag)
		{
			gc->sState.sDepth.ui32TestFunc &= ~EURASIA_ISPA_DWRITEDIS;
		}
		else
		{
			gc->sState.sDepth.ui32TestFunc |= EURASIA_ISPA_DWRITEDIS;
		}

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glDepthMask);
}


/***********************************************************************************
 Function Name      : ApplyDepthRange
 Inputs             : gc, fZNear, fZFar
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets current depth range state.
					  Sets VGP viewport/SW TNL state.
************************************************************************************/
IMG_INTERNAL IMG_VOID ApplyDepthRange(GLES2Context *gc, IMG_FLOAT fZNear, IMG_FLOAT fZFar)
{
	GLES2viewport *psViewport = &gc->sState.sViewport;

	fZNear = Clampf(fZNear, GLES2_Zero, GLES2_One);
	fZFar  = Clampf(fZFar, GLES2_Zero, GLES2_One);
	
	if (psViewport->fZNear < fZNear || psViewport->fZNear > fZNear || psViewport->fZFar < fZFar || psViewport->fZFar > fZFar)
	{
		psViewport->fZNear = fZNear;
		psViewport->fZFar  = fZFar;

		/* Compute viewport values for the new depth range */
		gc->sState.sViewport.fZScale  = (psViewport->fZFar - psViewport->fZNear) * GLES2_Half;
		gc->sState.sViewport.fZCenter = (psViewport->fZFar + psViewport->fZNear) * GLES2_Half;

		gc->ui32EmitMask |= GLES2_EMITSTATE_MTE_STATE_VIEWPORT;
	}
}


/***********************************************************************************
 Function Name      : glDepthRangef
 Inputs             : zNear, zFar
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current depth range state.
************************************************************************************/
GL_APICALL void GL_APIENTRY glDepthRangef(GLclampf zNear, GLclampf zFar)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDepthRangef"));

	GLES2_TIME_START(GLES2_TIMES_glDepthRangef);
	
	ApplyDepthRange(gc, zNear, zFar);

	GLES2_TIME_STOP(GLES2_TIMES_glDepthRangef);
}


/***********************************************************************************

 Function Name      : glDisable
 Inputs             : cap
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Disables state specified by cap.
************************************************************************************/
GL_APICALL void GL_APIENTRY glDisable(GLenum cap)
{
	IMG_UINT32 ui32Enables, ui32DirtyBits = 0;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDisable: 0x%X",cap));

	GLES2_TIME_START(GLES2_TIMES_glDisable);
   
	ui32Enables = gc->ui32Enables;

    switch (cap) 
	{
		case GL_CULL_FACE:
		{
			ui32Enables &= ~GLES2_CULLFACE_ENABLE;
			ui32DirtyBits |= GLES2_DIRTYFLAG_RENDERSTATE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_CULLFACE);
			break;
		}
		case GL_POLYGON_OFFSET_FILL:
		{
			ui32Enables &= ~GLES2_POLYOFFSET_ENABLE;
			ui32DirtyBits |= GLES2_DIRTYFLAG_RENDERSTATE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_POLYOFFSET);
			break;
		}
		case GL_SCISSOR_TEST:
		{
			if(ui32Enables & GLES2_SCISSOR_ENABLE)
			{
				ui32Enables &= ~GLES2_SCISSOR_ENABLE;
				gc->bDrawMaskInvalid = IMG_TRUE;				
			}
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_SCISSOR);
			break;
		}
		case GL_BLEND:
		{
			ui32Enables &= ~GLES2_ALPHABLEND_ENABLE;
			ui32DirtyBits |= GLES2_DIRTYFLAG_RENDERSTATE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_ALPHABLEND);
			break;
		}
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
		{
			ui32Enables &= ~GLES2_MSALPHACOV_ENABLE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_MSALPHACOV);
			break;
		}
		case GL_SAMPLE_COVERAGE:
		{
			ui32Enables &= ~GLES2_MSSAMPCOV_ENABLE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_MSSAMPCOV);
			break;
		}
		case GL_STENCIL_TEST:
		{
			ui32Enables &= ~GLES2_STENCILTEST_ENABLE;
			ui32DirtyBits |= GLES2_DIRTYFLAG_RENDERSTATE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_STENCILTEST);
			break;
		}
		case GL_DEPTH_TEST:
		{
			ui32Enables &= ~GLES2_DEPTHTEST_ENABLE;
			ui32DirtyBits |= GLES2_DIRTYFLAG_RENDERSTATE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_DEPTHTEST);
			break;
		}
		case GL_DITHER:
		{
			ui32Enables &= ~GLES2_DITHER_ENABLE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_DITHER);
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glDisable);

			return;
		}
    }

	if(gc->ui32Enables != ui32Enables)
	{
		gc->ui32Enables = ui32Enables;
		gc->ui32DirtyState |= ui32DirtyBits;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glDisable);
}


/***********************************************************************************
 Function Name      : glEnable
 Inputs             : cap
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Enables state specified by cap.
************************************************************************************/
GL_APICALL void GL_APIENTRY glEnable(GLenum cap)
{
	IMG_UINT32 ui32Enables, ui32DirtyBits = 0;
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glEnable: 0x%X",cap));

	GLES2_TIME_START(GLES2_TIMES_glEnable);
   
	ui32Enables = gc->ui32Enables;

    switch (cap) 
	{
		case GL_CULL_FACE:
		{
			ui32Enables |= GLES2_CULLFACE_ENABLE;
			ui32DirtyBits |= GLES2_DIRTYFLAG_RENDERSTATE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_CULLFACE);
			break;
		}
		case GL_POLYGON_OFFSET_FILL:
		{
			ui32Enables |= GLES2_POLYOFFSET_ENABLE;
			ui32DirtyBits |= GLES2_DIRTYFLAG_RENDERSTATE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_POLYOFFSET);
			break;
		}
		case GL_SCISSOR_TEST:
		{
			if((ui32Enables & GLES2_SCISSOR_ENABLE) == 0)
			{
				ui32Enables |= GLES2_SCISSOR_ENABLE;
				gc->bDrawMaskInvalid = IMG_TRUE;				
			}
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_SCISSOR);
			break;
		}
		case GL_BLEND:
		{
			ui32Enables |= GLES2_ALPHABLEND_ENABLE;
			ui32DirtyBits |= GLES2_DIRTYFLAG_RENDERSTATE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_ALPHABLEND);
			break;
		}
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
		{
			ui32Enables |= GLES2_MSALPHACOV_ENABLE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_MSALPHACOV);
			break;
		}
		case GL_SAMPLE_COVERAGE:
		{
			ui32Enables |= GLES2_MSSAMPCOV_ENABLE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_MSSAMPCOV);
			break;
		}
		case GL_STENCIL_TEST:
		{
			ui32Enables |= GLES2_STENCILTEST_ENABLE;
			ui32DirtyBits |= GLES2_DIRTYFLAG_RENDERSTATE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_STENCILTEST);
			break;
		}
		case GL_DEPTH_TEST:
		{
			ui32Enables |= GLES2_DEPTHTEST_ENABLE;
			ui32DirtyBits |= GLES2_DIRTYFLAG_RENDERSTATE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_DEPTHTEST);
			break;
		}
		case GL_DITHER:
		{
			ui32Enables |= GLES2_DITHER_ENABLE;
			GLES2_PROFILE_ADD_REDUNDANT_ENABLE(GLES2_DITHER);
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			GLES2_TIME_STOP(GLES2_TIMES_glEnable);
			return;
		}
    }

	if(gc->ui32Enables != ui32Enables)
	{
		gc->ui32Enables = ui32Enables;
		gc->ui32DirtyState |= ui32DirtyBits;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glEnable);
}


/***********************************************************************************
 Function Name      : glFrontFace
 Inputs             : mode
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets which primitive winding is considered to be 
					  the front face
************************************************************************************/
GL_APICALL void GL_APIENTRY glFrontFace(GLenum mode)
{
	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glFrontFace"));

	GLES2_TIME_START(GLES2_TIMES_glFrontFace);
   
	switch (mode) 
	{
		case GL_CW:
		case GL_CCW:
		{
			if(gc->sState.sPolygon.eFrontFaceDirection != mode)
			{
				gc->sState.sPolygon.eFrontFaceDirection = mode;

				gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
			}

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
		}
    }

	GLES2_TIME_STOP(GLES2_TIMES_glFrontFace);
}


/***********************************************************************************
 Function Name      : glLineWidth
 Inputs             : width
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets line width state
************************************************************************************/
GL_APICALL void GL_APIENTRY glLineWidth(GLfloat width)
{
	IMG_FLOAT fWidth;
	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glLineWidth"));

	GLES2_TIME_START(GLES2_TIMES_glLineWidth);

	if (width <= 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glLineWidth);
		return;
	}
		
	fWidth = Clampf(width, (IMG_FLOAT)GLES2_ALIASED_LINE_WIDTH_MIN, (IMG_FLOAT)GLES2_ALIASED_LINE_WIDTH_MAX);

	if (gc->sState.sLine.fWidth >  fWidth || gc->sState.sLine.fWidth < fWidth)
	{
		gc->sState.sLine.fWidth = fWidth;

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glLineWidth);
}


/***********************************************************************************
 Function Name      : glPolygonOffset
 Inputs             : factor, units
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets state for calculating z offset for polygons.
************************************************************************************/
GL_APICALL void GL_APIENTRY glPolygonOffset(GLfloat factor, GLfloat units)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPolygonOffset"));

	GLES2_TIME_START(GLES2_TIMES_glPolygonOffset);
	
	if (gc->sState.sPolygon.factor.fVal <  factor || gc->sState.sPolygon.factor.fVal > factor || 
		gc->sState.sPolygon.fUnits < units || gc->sState.sPolygon.fUnits > units)
	{
		gc->sState.sPolygon.factor.fVal = factor;
		gc->sState.sPolygon.fUnits  = units;

#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
		gc->sState.sPolygon.i32Units = (IMG_INT32)units;
#endif

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glPolygonOffset);
}


/***********************************************************************************
 Function Name      : glSampleCoverage
 Inputs             : value, invert
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets value of multisample coverage state.
					  Ignored.
************************************************************************************/
GL_APICALL void GL_APIENTRY glSampleCoverage(GLclampf value, GLboolean invert)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glSampleCoverage"));

	GLES2_TIME_START(GLES2_TIMES_glSampleCoverage);
	
	gc->sState.sMultisample.fSampleCoverageValue =	Clampf(value, GLES2_Zero, GLES2_One);
	gc->sState.sMultisample.bSampleCoverageInvert = invert ? IMG_TRUE : IMG_FALSE;

	GLES2_TIME_STOP(GLES2_TIMES_glSampleCoverage);
}


/***********************************************************************************
 Function Name      : StencilFunc
 Inputs             : gc, face, func, ref, mask
 Outputs            : -
 Returns            : -
 Description        : Utility: Sets stencil test state for front/back faces.
************************************************************************************/
static IMG_VOID StencilFunc(GLES2Context *gc, GLenum face, GLenum func, GLint ref, GLuint mask)
{
	if ((func < GL_NEVER) || (func > GL_ALWAYS)) 
	{
		SetError(gc, GL_INVALID_ENUM);

		return;
    }
	
	switch(face)
	{
		case GL_FRONT:
		{
			gc->sState.sStencil.ui32FFStencilRef = (IMG_UINT32)Clampi(ref, 0, (IMG_INT32)GLES2_MAX_STENCIL_VALUE) << EURASIA_ISPA_SREF_SHIFT;

			gc->sState.sStencil.ui32FFStencil &= (EURASIA_ISPC_SCMP_CLRMSK & EURASIA_ISPC_SCMPMASK_CLRMSK);

			gc->sState.sStencil.ui32FFStencil |= ((func - GL_NEVER) << EURASIA_ISPC_SCMP_SHIFT) | 
												 ((mask & GLES2_MAX_STENCIL_VALUE) << EURASIA_ISPC_SCMPMASK_SHIFT);

			gc->sState.sStencil.ui32FFStencilCompareMaskIn	= mask;

			gc->sState.sStencil.i32FFStencilRefIn			= ref;

			break;
		}
		case GL_FRONT_AND_BACK:
		{
			gc->sState.sStencil.ui32FFStencilRef = (IMG_UINT32)Clampi(ref, 0, (IMG_INT32)GLES2_MAX_STENCIL_VALUE) << EURASIA_ISPA_SREF_SHIFT;

			gc->sState.sStencil.ui32FFStencil &= (EURASIA_ISPC_SCMP_CLRMSK & EURASIA_ISPC_SCMPMASK_CLRMSK);

			gc->sState.sStencil.ui32FFStencil |= ((func - GL_NEVER) << EURASIA_ISPC_SCMP_SHIFT)	|
												((mask & GLES2_MAX_STENCIL_VALUE) << EURASIA_ISPC_SCMPMASK_SHIFT);

			gc->sState.sStencil.ui32FFStencilCompareMaskIn	= mask;

			gc->sState.sStencil.i32FFStencilRefIn			= ref;

			/* Intentional fall-through */
		}
		case GL_BACK:
		{
			gc->sState.sStencil.ui32BFStencilRef = (IMG_UINT32)Clampi(ref, 0, (IMG_INT32)GLES2_MAX_STENCIL_VALUE) << EURASIA_ISPA_SREF_SHIFT;

			gc->sState.sStencil.ui32BFStencil &= (EURASIA_ISPC_SCMP_CLRMSK & EURASIA_ISPC_SCMPMASK_CLRMSK);

			gc->sState.sStencil.ui32BFStencil |= ((func - GL_NEVER) << EURASIA_ISPC_SCMP_SHIFT)	|
												((mask & GLES2_MAX_STENCIL_VALUE) << EURASIA_ISPC_SCMPMASK_SHIFT);

			gc->sState.sStencil.ui32BFStencilCompareMaskIn	= mask;

			gc->sState.sStencil.i32BFStencilRefIn			= ref;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			return;
		}
	}

	gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
}


/***********************************************************************************
 Function Name      : glStencilFunc
 Inputs             : func, ref, mask
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets stencil test state (for front+back faces).
************************************************************************************/
GL_APICALL void GL_APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glStencilFunc"));

	GLES2_TIME_START(GLES2_TIMES_glStencilFunc);

	StencilFunc(gc, GL_FRONT_AND_BACK, func, ref, mask);

	GLES2_TIME_STOP(GLES2_TIMES_glStencilFunc);

}


/***********************************************************************************
 Function Name      : glStencilFuncSeparate
 Inputs             : face, func, ref, mask
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets stencil test state seperately for front/back
					  faces.
************************************************************************************/
GL_APICALL void GL_APIENTRY glStencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glStencilFuncSeparate"));

	GLES2_TIME_START(GLES2_TIMES_glStencilFuncSeparate);

	StencilFunc(gc, face, func, ref, mask);

	GLES2_TIME_STOP(GLES2_TIMES_glStencilFuncSeparate);
}


/***********************************************************************************
 Function Name      : StencilMask
 Inputs             : gc, face, mask
 Outputs            : -
 Returns            : -
 Description        : Utility: Sets stencil test state for front/back faces.
************************************************************************************/
static IMG_VOID StencilMask(GLES2Context *gc, GLenum face, GLuint mask)
{
	switch(face)
	{
		case GL_FRONT:
		{
			gc->sState.sStencil.ui32FFStencil = (gc->sState.sStencil.ui32FFStencil & EURASIA_ISPC_SWMASK_CLRMSK) |
														((mask & GLES2_MAX_STENCIL_VALUE) << EURASIA_ISPC_SWMASK_SHIFT);

			gc->sState.sStencil.ui32FFStencilWriteMaskIn = mask;

			break;
		}
		case GL_FRONT_AND_BACK:
		{
			gc->sState.sStencil.ui32FFStencil = (gc->sState.sStencil.ui32FFStencil & EURASIA_ISPC_SWMASK_CLRMSK) |
														((mask & GLES2_MAX_STENCIL_VALUE) << EURASIA_ISPC_SWMASK_SHIFT);

			gc->sState.sStencil.ui32FFStencilWriteMaskIn = mask;

			/* Intentional fall-through */
		}
		case GL_BACK:
		{
			gc->sState.sStencil.ui32BFStencil = (gc->sState.sStencil.ui32BFStencil & EURASIA_ISPC_SWMASK_CLRMSK) |
														((mask & GLES2_MAX_STENCIL_VALUE) << EURASIA_ISPC_SWMASK_SHIFT);

			gc->sState.sStencil.ui32BFStencilWriteMaskIn = mask;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			return;
		}
	}

	gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
}


/***********************************************************************************
 Function Name      : glStencilMask
 Inputs             : mask
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets stencilbuffer writemask state
************************************************************************************/
GL_APICALL void GL_APIENTRY glStencilMask(GLuint mask)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glStencilMask"));

	GLES2_TIME_START(GLES2_TIMES_glStencilMask);

	StencilMask(gc, GL_FRONT_AND_BACK, mask);

	GLES2_TIME_STOP(GLES2_TIMES_glStencilMask);
}


/***********************************************************************************
 Function Name      : glStencilMaskSeparate
 Inputs             : mask
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets stencilbuffer writemask state for front/back
************************************************************************************/
GL_APICALL void GL_APIENTRY glStencilMaskSeparate(GLenum face, GLuint mask)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glStencilMaskSeparate"));

	GLES2_TIME_START(GLES2_TIMES_glStencilMaskSeparate);

	StencilMask(gc, face, mask);

	GLES2_TIME_STOP(GLES2_TIMES_glStencilMaskSeparate);
}


/***********************************************************************************
 Function Name      : StencilMask
 Inputs             : gc, face, mask
 Outputs            : -
 Returns            : -
 Description        : Utility: Sets stencil test state for front/back faces.
************************************************************************************/
static IMG_VOID StencilOp(GLES2Context *gc, GLenum face, IMG_UINT32 aui32StencilOp[3])
{
	IMG_UINT32 i;
	IMG_UINT32 aui32OpShift[3] = {EURASIA_ISPC_SOP1_SHIFT, EURASIA_ISPC_SOP2_SHIFT, EURASIA_ISPC_SOP3_SHIFT};
	IMG_UINT32 ui32StencilOp = 0;

	for(i=0; i < 3; i++)
	{
		switch (aui32StencilOp[i]) 
		{
			case GL_KEEP:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_KEEP << aui32OpShift[i]);

				break;
			}
			case GL_ZERO:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_ZERO << aui32OpShift[i]);

				break;
			}
			case GL_REPLACE:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_REPLACE << aui32OpShift[i]);

				break;
			}
			case GL_INCR:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_INCSAT << aui32OpShift[i]);

				break;
			}
			case GL_DECR:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_DECSAT << aui32OpShift[i]);

				break;
			}
			case GL_INCR_WRAP:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_INC << aui32OpShift[i]);

				break;
			}
			case GL_DECR_WRAP:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_DEC << aui32OpShift[i]);

				break;
			}
			case GL_INVERT:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_INVERT << aui32OpShift[i]);

				break;
			}
			default:
			{
				SetError(gc, GL_INVALID_ENUM);

				return;
			}
		}
	}

	switch(face)
	{
		case GL_FRONT:
		{
			gc->sState.sStencil.ui32FFStencil = 
					(gc->sState.sStencil.ui32FFStencil & EURASIA_ISPC_SOPALL_CLRMSK) | ui32StencilOp;

			break;
		}
		case GL_FRONT_AND_BACK:
		{
			gc->sState.sStencil.ui32FFStencil = 
					(gc->sState.sStencil.ui32FFStencil & EURASIA_ISPC_SOPALL_CLRMSK) | ui32StencilOp;
			/* Intentional fall-through */
		}
		case GL_BACK:
		{
			gc->sState.sStencil.ui32BFStencil = 
					(gc->sState.sStencil.ui32BFStencil & EURASIA_ISPC_SOPALL_CLRMSK) | ui32StencilOp;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			return;
		}
	}

	gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
}


/***********************************************************************************
 Function Name      : glStencilOp
 Inputs             : fail, zfail, zpass
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT:Sets stencil op state (for front+back faces)
************************************************************************************/
GL_APICALL void GL_APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	IMG_UINT32 aui32StencilOp[3];

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glStencilOp"));

	GLES2_TIME_START(GLES2_TIMES_glStencilOp);
   
	aui32StencilOp[0] = fail;
	aui32StencilOp[1] = zfail;
	aui32StencilOp[2] = zpass;

	StencilOp(gc, GL_FRONT_AND_BACK, aui32StencilOp);

	GLES2_TIME_STOP(GLES2_TIMES_glStencilOp);
}


/***********************************************************************************
 Function Name      : glStencilOpSeparate
 Inputs             : face, fail, zfail, zpass
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets stencil op state seperately for front/back
					  faces.
************************************************************************************/
GL_APICALL void GL_APIENTRY glStencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
	IMG_UINT32 aui32StencilOp[3];

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glStencilOpSeparate"));

	GLES2_TIME_START(GLES2_TIMES_glStencilOpSeparate);

	aui32StencilOp[0] = fail;
	aui32StencilOp[1] = zfail;
	aui32StencilOp[2] = zpass;

	StencilOp(gc, face, aui32StencilOp);

	GLES2_TIME_STOP(GLES2_TIMES_glStencilOpSeparate);
}


/***********************************************************************************
 Function Name      : ApplyViewport
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets current viewport transform
************************************************************************************/
IMG_INTERNAL IMG_VOID ApplyViewport(GLES2Context *gc)
{
	IMG_FLOAT fWW, fHH;
	GLES2viewport *psViewport = &gc->sState.sViewport;

	/* Compute operational viewport values */
	fWW = (IMG_FLOAT)psViewport->ui32Width * GLES2_Half;
	fHH = (IMG_FLOAT)psViewport->ui32Height * GLES2_Half;
	
	if(gc->psDrawParams->eRotationAngle == PVRSRV_FLIP_Y)
	{
		psViewport->fXScale = fWW;
		psViewport->fXCenter = (IMG_FLOAT)psViewport->i32X + fWW;
		psViewport->fYScale = fHH;
		psViewport->fYCenter = (IMG_FLOAT)psViewport->i32Y + fHH;
	}
	else
	{
		psViewport->fXScale = fWW;
		psViewport->fXCenter = (IMG_FLOAT)psViewport->i32X + fWW;
		psViewport->fYScale = -fHH;
		psViewport->fYCenter = (IMG_FLOAT)gc->psDrawParams->ui32Height - ((IMG_FLOAT)psViewport->i32Y + fHH);
	}

	gc->ui32EmitMask |= GLES2_EMITSTATE_MTE_STATE_VIEWPORT;
}


/***********************************************************************************
 Function Name      : glViewport
 Inputs             : x, y, width, height
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current viewport state.
************************************************************************************/
GL_APICALL void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	GLES2viewport *psViewport;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glViewport"));

	GLES2_TIME_START(GLES2_TIMES_glViewport);

	psViewport = &gc->sState.sViewport;

	if ((width < 0) || (height < 0))
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glViewport);

		return;
	}
    
	if (width > (GLsizei)gc->psMode->ui32MaxViewportX)
	{
		width = (GLsizei)gc->psMode->ui32MaxViewportX;
	}

	if (height > (GLsizei)gc->psMode->ui32MaxViewportY)
	{
		height = (GLsizei)gc->psMode->ui32MaxViewportY;
	}
    
	if(psViewport->i32X != x || psViewport->i32Y != y || 
		psViewport->ui32Width != (IMG_UINT32)width || psViewport->ui32Height != (IMG_UINT32)height)
	{
		psViewport->i32X = x;
		psViewport->i32Y = y;
		psViewport->ui32Width = (IMG_UINT32)width;
		psViewport->ui32Height = (IMG_UINT32)height;
	}
	else
	{
		GLES2_TIME_STOP(GLES2_TIMES_glViewport);

		return;
	}

	ApplyViewport(gc);

	if ((x <= 0) && (y <= 0) &&
		(((IMG_INT32)psViewport->ui32Width + x) >= (IMG_INT32)gc->psDrawParams->ui32Width) && 
		(((IMG_INT32)psViewport->ui32Height + y) >= (IMG_INT32)gc->psDrawParams->ui32Height))
	{
		gc->bFullScreenViewport = IMG_TRUE;
	}
	else
	{
		gc->bFullScreenViewport = IMG_FALSE;
	}

	gc->bDrawMaskInvalid = IMG_TRUE;

	GLES2_TIME_STOP(GLES2_TIMES_glViewport);
}
