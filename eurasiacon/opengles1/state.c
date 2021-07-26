/******************************************************************************
 * Name         : state.c
 *
 * Copyright    : 2003-2006 by Imagination Technologies Limited.
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
 * $Log: state.c $
 *****************************************************************************/

#include "context.h"

/***********************************************************************************
 Function Name      : glEnable
 Inputs             : cap
 Outputs            : -
 Returns            : -
 Description        : Enables state specified by cap.
************************************************************************************/
GL_API void GL_APIENTRY glEnable(GLenum cap)
{
	IMG_UINT32 ui32RasterEnables, ui32TnLEnables, ui32IgnoredEnables, ui32FrameEnables;
	IMG_UINT32 ui32DirtyBits = 0;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glEnable: 0x%X",cap));

	GLES1_TIME_START(GLES1_TIMES_glEnable);

	ui32RasterEnables = gc->ui32RasterEnables;
	ui32TnLEnables = gc->ui32TnLEnables;
	ui32IgnoredEnables = gc->ui32IgnoredEnables;
	ui32FrameEnables = gc->ui32FrameEnables;

	switch (cap)
	{
		/*
		 * Raterisation Enables, that HW supports
		 */
		case GL_ALPHA_TEST:
		{
			ui32RasterEnables |= GLES1_RS_ALPHATEST_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_ALPHATEST, Raster);

			goto RASTER_CHECK;
		}
		case GL_BLEND:
		{
			ui32RasterEnables |= GLES1_RS_ALPHABLEND_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM|GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_ALPHABLEND, Raster);

			goto RASTER_CHECK;
		}
		case GL_COLOR_LOGIC_OP:
		{
			ui32RasterEnables |= GLES1_RS_LOGICOP_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM|GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_LOGICOP, Raster);

			goto RASTER_CHECK;
		}
		case GL_STENCIL_TEST:
		{
			ui32RasterEnables |= GLES1_RS_STENCILTEST_ENABLE;

			if(gc->psMode->ui32StencilBits)
			{
				ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE;
			}

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_STENCILTEST, Raster);

			goto RASTER_CHECK;
		}
		case GL_TEXTURE_2D:
		{
			IMG_UINT32 ui32TextureShift = GLES1_RS_2DTEXTURE0 + gc->sState.sTexture.ui32ActiveTexture;

			ui32RasterEnables |= (1UL << ui32TextureShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS
								| GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(ui32TextureShift, Raster);

			goto RASTER_CHECK;
		}
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_OES:
		{
			IMG_UINT32 ui32TextureShift = GLES1_RS_CEMTEXTURE0 + gc->sState.sTexture.ui32ActiveTexture;

			ui32RasterEnables |= (1UL << ui32TextureShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS
								| GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_DEPTHTEST, Raster);

			goto RASTER_CHECK;
		}
		case GL_TEXTURE_GEN_STR_OES:
		{
			IMG_UINT32 ui32TextureShift = GLES1_RS_GENTEXTURE0 + gc->sState.sTexture.ui32ActiveTexture;

			ui32RasterEnables |= (1UL << ui32TextureShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(ui32TextureShift, Raster);

			goto RASTER_CHECK;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
		case GL_DEPTH_TEST:
		{
			ui32RasterEnables |= GLES1_RS_DEPTHTEST_ENABLE;

			if( gc->psMode->ui32DepthBits )
			{
				ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE;
			}

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_DEPTHTEST, Raster);

			goto RASTER_CHECK;
		}
		case GL_POLYGON_OFFSET_FILL:
		{
			ui32RasterEnables |= GLES1_RS_POLYOFFSET_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_POLYOFFSET, Raster);

			goto RASTER_CHECK;
		}
		case GL_CULL_FACE:
		{
			ui32TnLEnables |= GLES1_TL_CULLFACE_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_CULLFACE, TnL);

			goto TNL_CHECK;
		}
		case GL_FOG:
		{
			/* Although fog can be turned on/off per prim, the fog table
			 * can't be reloaded mid-frame
			 */
			ui32RasterEnables |= GLES1_RS_FOG_ENABLE;
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS
								| GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;			

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_FOG, Raster);

			goto RASTER_CHECK;
		}
		/*
		 * State which affects vertex processing
		 */
		case GL_RESCALE_NORMAL:
		{
			ui32TnLEnables |= GLES1_TL_RESCALE_ENABLE;
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_RESCALE, TnL);

			goto TNL_CHECK;
		}
		case GL_COLOR_MATERIAL:
		{
			ui32TnLEnables |= GLES1_TL_COLORMAT_ENABLE;

			/* Need to update material properties with current color when colormaterial is enabled */
			Materialfv(gc, GL_FRONT_AND_BACK, gc->sState.sLight.eColorMaterialParam, &gc->sState.sCurrent.asAttrib[AP_COLOR].fX);
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_COLORMAT, TnL);

			goto TNL_CHECK;
		}
		case GL_LIGHTING:
		{
			ui32TnLEnables |= GLES1_TL_LIGHTING_ENABLE;
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_LIGHTING, TnL);

			goto TNL_CHECK;
		}
		case GL_NORMALIZE:
		{
			ui32TnLEnables |= GLES1_TL_NORMALIZE_ENABLE;
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_NORMALIZE, TnL);

			goto TNL_CHECK;
		}
		case GL_LIGHT0: case GL_LIGHT1:
		case GL_LIGHT2: case GL_LIGHT3:
		case GL_LIGHT4: case GL_LIGHT5:
		case GL_LIGHT6: case GL_LIGHT7:
		{
			int tmpLightShift = GLES1_TL_LIGHT0 + (int)cap - GL_LIGHT0;
			ui32TnLEnables |= (1UL << tmpLightShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(tmpLightShift, TnL);

			goto TNL_CHECK;
		}
		case GL_CLIP_PLANE0:
		case GL_CLIP_PLANE1:
		case GL_CLIP_PLANE2:
		case GL_CLIP_PLANE3:
		case GL_CLIP_PLANE4:
		case GL_CLIP_PLANE5:
		{
			int nClipShift = GLES1_TL_CLIP_PLANE0 + (int)cap - GL_CLIP_PLANE0;
			ui32TnLEnables |= (1UL << nClipShift);
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(nClipShift, TnL);

			goto TNL_CHECK;
		}

		/*
		 * Whole frame/framebuffer state
		 * Although this state can be turned on/off per prim, 
		 * either we need to special handling or we can only do on a
		 * per frame basis
		 */
		case GL_SCISSOR_TEST:
		{
			if ((gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE) == 0)
			{
				gc->bDrawMaskInvalid = IMG_TRUE;

				ui32FrameEnables |= GLES1_FS_SCISSOR_ENABLE;
			}

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_FS_SCISSOR, Frame);

			goto FS_CHECK;
		}
		case GL_DITHER:
		{
			ui32FrameEnables |= GLES1_FS_DITHER_ENABLE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_FS_DITHER, Frame);

			goto FS_CHECK;
		}
		case GL_MULTISAMPLE:
		{
			ui32FrameEnables |= GLES1_FS_MULTISAMPLE_ENABLE;


			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_FS_MULTISAMPLE, Frame);

			goto FS_CHECK;
		}
		/*
		 * Unsupported multisample state - not do-able in HW
		 */
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
		{
			ui32IgnoredEnables |= GLES1_IG_MSALPHACOV_ENABLE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_IG_MSALPHACOV, Ignored);

			goto IGNORE_CHECK;
		}
		case GL_SAMPLE_ALPHA_TO_ONE:
		{
			ui32IgnoredEnables |= GLES1_IG_MSSAMPALPHA_ENABLE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_IG_MSSAMPALPHA, Ignored);

			goto IGNORE_CHECK;
		}
		case GL_SAMPLE_COVERAGE:
		{
			ui32IgnoredEnables |= GLES1_IG_MSSAMPCOV_ENABLE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_IG_MSSAMPCOV, Ignored);

			goto IGNORE_CHECK;
		}
		/*
		 * Unsupported per-primitive state, and not do-able in HW
		 */ 
		case GL_LINE_SMOOTH:
		{
			ui32RasterEnables |= GLES1_RS_LINESMOOTH_ENABLE;
			
			/* Can cause new line width due to clamping */
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE;
			gc->sState.sLine.pfLineWidth = &gc->sState.sLine.fSmoothWidth;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_LINESMOOTH, Raster);

			goto RASTER_CHECK;
		}
		case GL_POINT_SMOOTH:
		{
			ui32RasterEnables |= GLES1_RS_POINTSMOOTH_ENABLE;
		
			/* Can cause new point size due to clamping */
			ui32DirtyBits = GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			/* Ignore smooth point state when pointsprites are enabled */
			if(gc->ui32TnLEnables & GLES1_TL_POINTSPRITE_ENABLE)
			{
				gc->sState.sPoint.pfPointSize = &gc->sState.sPoint.fAliasedSize;
				gc->sState.sPoint.pfMaxPointSize = &gc->sState.sPoint.fMaxAliasedSize;
				gc->sState.sPoint.pfMinPointSize = &gc->sState.sPoint.fMinAliasedSize;
			}
			else
			{
				gc->sState.sPoint.pfPointSize = &gc->sState.sPoint.fSmoothSize;
				gc->sState.sPoint.pfMaxPointSize = &gc->sState.sPoint.fMaxSmoothSize;
				gc->sState.sPoint.pfMinPointSize = &gc->sState.sPoint.fMinSmoothSize;
			}

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_POINTSMOOTH, Raster);

			goto RASTER_CHECK;
		}
		case GL_POINT_SPRITE_OES:
		{
			ui32TnLEnables |= GLES1_TL_POINTSPRITE_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			/* Ignore smooth point state when pointsprites are enabled */
			gc->sState.sPoint.pfPointSize = &gc->sState.sPoint.fAliasedSize;
			gc->sState.sPoint.pfMaxPointSize = &gc->sState.sPoint.fMaxAliasedSize;
			gc->sState.sPoint.pfMinPointSize = &gc->sState.sPoint.fMinAliasedSize;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_POINTSPRITE, TnL);

			goto TNL_CHECK;
		}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_PALETTE_OES:
		{
			ui32TnLEnables |= GLES1_TL_MATRIXPALETTE_ENABLE;

			ui32DirtyBits = GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_MATRIXPALETTE, TnL);

			goto TNL_CHECK; 
		}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_STREAM_IMG:
		{
			IMG_UINT32 ui32TextureShift = GLES1_RS_TEXTURE0_STREAM + gc->sState.sTexture.ui32ActiveTexture;
			ui32RasterEnables |= (1UL << ui32TextureShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS
								| GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(ui32TextureShift, Raster);

			goto RASTER_CHECK;
		}
#elif defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_EXTERNAL_OES:
		{
			IMG_UINT32 ui32TextureShift = GLES1_RS_TEXTURE0_STREAM + gc->sState.sTexture.ui32ActiveTexture;
			ui32RasterEnables |= (1UL << ui32TextureShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS
								| GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(ui32TextureShift, Raster);

			goto RASTER_CHECK;
		}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL) */

		/*
		 * Set error value, not an OGL-ES state
		 */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glEnable);

			return;
		}
	}

RASTER_CHECK:
	if(gc->ui32RasterEnables != ui32RasterEnables)
	{
		gc->ui32RasterEnables = ui32RasterEnables;
		gc->ui32DirtyMask |= ui32DirtyBits;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glEnable);
	
	return;

TNL_CHECK:
	if(gc->ui32TnLEnables != ui32TnLEnables)
	{
		gc->ui32TnLEnables = ui32TnLEnables;
		gc->ui32DirtyMask |= ui32DirtyBits;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glEnable);
	
	return;

FS_CHECK:
	gc->ui32FrameEnables = ui32FrameEnables;

	GLES1_TIME_STOP(GLES1_TIMES_glEnable);
	
	return;

IGNORE_CHECK:
	gc->ui32IgnoredEnables = ui32IgnoredEnables;

	GLES1_TIME_STOP(GLES1_TIMES_glEnable);
	
	return;
}


/***********************************************************************************
 Function Name      : glDisable
 Inputs             : cap
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Disables state specified by cap.
					  All GL enables are stored in a set of 4 enable words grouped by 
					  usage. Some enables are for per-primitive raster state, some for
					  TNL,some for per-frame state, and some cannot be handled by MBX 
					  and are ignored.
************************************************************************************/
GL_API void GL_APIENTRY glDisable(GLenum cap)
{
	IMG_UINT32 ui32RasterEnables, ui32TnLEnables, ui32IgnoredEnables, ui32FrameEnables;
	IMG_UINT32 ui32DirtyBits=0;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDisable: 0x%X",cap));

	GLES1_TIME_START(GLES1_TIMES_glDisable);

	ui32RasterEnables = gc->ui32RasterEnables;
	ui32TnLEnables = gc->ui32TnLEnables;
	ui32IgnoredEnables = gc->ui32IgnoredEnables;
	ui32FrameEnables = gc->ui32FrameEnables;

	switch (cap) 
	{
		/*
		 * Raterisation Enables, that HW supports
		 */
		case GL_ALPHA_TEST:
		{
			ui32RasterEnables &= ~GLES1_RS_ALPHATEST_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_ALPHATEST, Raster);

			goto RASTER_CHECK;
		}
		case GL_BLEND:
		{
			ui32RasterEnables &= ~GLES1_RS_ALPHABLEND_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM|GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_ALPHABLEND, Raster);

			goto RASTER_CHECK;
		}
		case GL_COLOR_LOGIC_OP:
		{
			ui32RasterEnables &= ~GLES1_RS_LOGICOP_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM|GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_LOGICOP, Raster);

			goto RASTER_CHECK;
		}
		case GL_STENCIL_TEST:
		{
			ui32RasterEnables &= ~GLES1_RS_STENCILTEST_ENABLE;

			if(gc->psMode->ui32StencilBits)
			{
				ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE;
			}

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_STENCILTEST, Raster);

			goto RASTER_CHECK;
		}
		case GL_TEXTURE_2D:
		{
			IMG_UINT32 ui32TextureShift = GLES1_RS_2DTEXTURE0 + gc->sState.sTexture.ui32ActiveTexture;

			ui32RasterEnables &= ~(1UL << ui32TextureShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS
								| GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(ui32TextureShift, Raster);

			goto RASTER_CHECK;
		}
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_OES:
		{
			IMG_UINT32 ui32TextureShift = GLES1_RS_CEMTEXTURE0 + gc->sState.sTexture.ui32ActiveTexture;

			ui32RasterEnables &= ~(1UL << ui32TextureShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS
								| GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(ui32TextureShift, Raster);

			goto RASTER_CHECK;
		}
		case GL_TEXTURE_GEN_STR_OES:
		{
			IMG_UINT32 ui32TextureShift = GLES1_RS_GENTEXTURE0 + gc->sState.sTexture.ui32ActiveTexture;

			ui32RasterEnables &= ~(1UL << ui32TextureShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(ui32TextureShift, Raster);

			goto RASTER_CHECK;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
		case GL_DEPTH_TEST:
		{
			ui32RasterEnables &= ~GLES1_RS_DEPTHTEST_ENABLE;

			if( gc->psMode->ui32DepthBits )
			{
				ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE;
			}

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_DEPTHTEST, Raster);

			goto RASTER_CHECK;
		}
		case GL_POLYGON_OFFSET_FILL:
		{
			ui32RasterEnables &= ~GLES1_RS_POLYOFFSET_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_POLYOFFSET, Raster);

			goto RASTER_CHECK;
		}
		case GL_CULL_FACE:
		{
			ui32TnLEnables &= ~GLES1_TL_CULLFACE_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_CULLFACE, TnL);

			goto TNL_CHECK;
		}
		case GL_FOG:
		{
			/* Although fog can be turned on/off per prim, the fog table
			 * can't be reloaded mid-frame
			 */
			ui32RasterEnables &= ~GLES1_RS_FOG_ENABLE;
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS
								| GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;			

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_FOG, Raster);

			goto RASTER_CHECK;
		}
		/*
		 * State which affects vertex processing
		 */
		case GL_RESCALE_NORMAL:
		{
			ui32TnLEnables &= ~GLES1_TL_RESCALE_ENABLE;
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_RESCALE, TnL);

			goto TNL_CHECK;
		}
		case GL_COLOR_MATERIAL:
		{
			/*
			 * Need to update material properties with current color when colormaterial is disabled
			 * (ensures corollary 11 of the spec is enforced - material changes have no effect while colormaterial
			 * is enabled.
			 */
			if (ui32TnLEnables & GLES1_TL_COLORMAT_ENABLE)
			{
				Materialfv(gc, GL_FRONT_AND_BACK, gc->sState.sLight.eColorMaterialParam, &gc->sState.sCurrent.asAttrib[AP_COLOR].fX);
			}
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
			ui32TnLEnables &= ~GLES1_TL_COLORMAT_ENABLE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_COLORMAT, TnL);

			goto TNL_CHECK;
		}
		case GL_LIGHTING:
		{
			ui32TnLEnables &= ~GLES1_TL_LIGHTING_ENABLE;
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_LIGHTING, TnL);

			goto TNL_CHECK;
		}
		case GL_NORMALIZE:
		{
			ui32TnLEnables &= ~GLES1_TL_NORMALIZE_ENABLE;
			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_NORMALIZE, TnL);

			goto TNL_CHECK;
		}
		case GL_LIGHT0: case GL_LIGHT1:
		case GL_LIGHT2: case GL_LIGHT3:
		case GL_LIGHT4: case GL_LIGHT5:
		case GL_LIGHT6: case GL_LIGHT7:
		{
			int tmpLightShift = GLES1_TL_LIGHT0 + (int)cap - GL_LIGHT0;
			ui32TnLEnables &= ~(1UL << tmpLightShift);

			ui32DirtyBits =  GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(tmpLightShift, TnL);

			goto TNL_CHECK;
		}
		case GL_CLIP_PLANE0:
		case GL_CLIP_PLANE1:
		case GL_CLIP_PLANE2:
		case GL_CLIP_PLANE3:
		case GL_CLIP_PLANE4:
		case GL_CLIP_PLANE5:
		{
			int nClipShift = GLES1_TL_CLIP_PLANE0 + (int)cap - GL_CLIP_PLANE0;
			ui32TnLEnables &= ~(1UL << nClipShift);
		
			ui32DirtyBits = GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(nClipShift, TnL);

			goto TNL_CHECK;
		}
		/*
		 * Whole frame/framebuffer state
		 * Although this state can be turned on/off per prim,
		 * either we need to special handling or we can only do on a
		 * per frame basis
		 */
		case GL_SCISSOR_TEST:
		{
			if (gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE)
			{
				gc->bDrawMaskInvalid = IMG_TRUE;

				ui32FrameEnables &= ~GLES1_FS_SCISSOR_ENABLE;
			}

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_FS_SCISSOR, Frame);

			goto FS_CHECK;
		}
		case GL_DITHER:
		{
			ui32FrameEnables &= ~GLES1_FS_DITHER_ENABLE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_FS_DITHER, Frame);

			goto FS_CHECK;
		}
		case GL_MULTISAMPLE:
		{
			ui32FrameEnables &= ~GLES1_FS_MULTISAMPLE_ENABLE;


			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_FS_MULTISAMPLE, Frame);

			goto FS_CHECK;
		}
		/*
		 * Unsupported multisample state - not do-able in HW
		 */
		case GL_SAMPLE_ALPHA_TO_COVERAGE:
		{
			ui32IgnoredEnables &= ~GLES1_IG_MSALPHACOV_ENABLE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_IG_MSALPHACOV, Ignored);

			goto IGNORE_CHECK;
		}
		case GL_SAMPLE_ALPHA_TO_ONE:
		{
			ui32IgnoredEnables &= ~GLES1_IG_MSSAMPALPHA_ENABLE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_IG_MSSAMPALPHA, Ignored);

			goto IGNORE_CHECK;
		}
		case GL_SAMPLE_COVERAGE:
		{
			ui32IgnoredEnables &= ~GLES1_IG_MSSAMPCOV_ENABLE;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_IG_MSSAMPCOV, Ignored);

			goto IGNORE_CHECK;
		}
		/*
		 * Unsupported per-primitive state, and not do-able in HW
		 */ 
		case GL_LINE_SMOOTH:
		{
			ui32RasterEnables &= ~GLES1_RS_LINESMOOTH_ENABLE;
			
			/* Can cause new line width due to clamping */
			ui32DirtyBits = GLES1_DIRTYFLAG_RENDERSTATE;
			gc->sState.sLine.pfLineWidth = &gc->sState.sLine.fAliasedWidth;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_LINESMOOTH, Raster);

			goto RASTER_CHECK;
		}
		case GL_POINT_SMOOTH:
		{
			ui32RasterEnables &= ~GLES1_RS_POINTSMOOTH_ENABLE;
			
			/* Can cause new point size due to clamping */
			ui32DirtyBits = GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			gc->sState.sPoint.pfPointSize = &gc->sState.sPoint.fAliasedSize;
			gc->sState.sPoint.pfMaxPointSize = &gc->sState.sPoint.fMaxAliasedSize;
			gc->sState.sPoint.pfMinPointSize = &gc->sState.sPoint.fMinAliasedSize;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_RS_POINTSMOOTH, Raster);

			goto RASTER_CHECK;
		}
		case GL_POINT_SPRITE_OES:
		{
			ui32TnLEnables &= ~GLES1_TL_POINTSPRITE_ENABLE;
			ui32DirtyBits = GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			/* Use smooth point state when pointsprites are disabled */
			if(gc->ui32RasterEnables & GLES1_RS_POINTSMOOTH_ENABLE)
			{
				gc->sState.sPoint.pfPointSize = &gc->sState.sPoint.fSmoothSize;
				gc->sState.sPoint.pfMaxPointSize = &gc->sState.sPoint.fMaxSmoothSize;
				gc->sState.sPoint.pfMinPointSize = &gc->sState.sPoint.fMinSmoothSize;
			}
			else
			{
				gc->sState.sPoint.pfPointSize = &gc->sState.sPoint.fAliasedSize;
				gc->sState.sPoint.pfMaxPointSize = &gc->sState.sPoint.fMaxAliasedSize;
				gc->sState.sPoint.pfMinPointSize = &gc->sState.sPoint.fMinAliasedSize;
			}

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_POINTSPRITE, TnL);

			goto TNL_CHECK;
		}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_PALETTE_OES:
		{
			ui32TnLEnables &= ~GLES1_TL_MATRIXPALETTE_ENABLE;

			ui32DirtyBits = GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(GLES1_TL_MATRIXPALETTE, TnL);

			goto TNL_CHECK; 
		}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_STREAM_IMG:
		{
			IMG_UINT32 ui32TextureShift = GLES1_RS_TEXTURE0_STREAM + gc->sState.sTexture.ui32ActiveTexture;

			ui32RasterEnables &= ~(1UL << ui32TextureShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS
								| GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(ui32TextureShift, Raster);

			goto RASTER_CHECK;
		}
#elif defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_EXTERNAL_OES:
		{
			IMG_UINT32 ui32TextureShift = GLES1_RS_TEXTURE0_STREAM + gc->sState.sTexture.ui32ActiveTexture;

			ui32RasterEnables &= ~(1UL << ui32TextureShift);
			ui32DirtyBits = GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS
								| GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

			GLES1_PROFILE_ADD_REDUNDANT_STATE(ui32TextureShift, Raster);

			goto RASTER_CHECK;
		}
#endif	/* defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL) */
		/*
		 * Set error value, not an OGL-ES state
		 */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glDisable);

			return;
		}
	}

RASTER_CHECK:
	if(gc->ui32RasterEnables != ui32RasterEnables)
	{
		gc->ui32RasterEnables = ui32RasterEnables;

		gc->ui32DirtyMask |= ui32DirtyBits;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glDisable);

	return;

TNL_CHECK:
	if(gc->ui32TnLEnables != ui32TnLEnables)
	{
		gc->ui32TnLEnables = ui32TnLEnables;

		gc->ui32DirtyMask |= ui32DirtyBits;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glDisable);

	return;

FS_CHECK:
	gc->ui32FrameEnables = ui32FrameEnables;

	GLES1_TIME_STOP(GLES1_TIMES_glDisable);

	return;

IGNORE_CHECK:
	gc->ui32IgnoredEnables = ui32IgnoredEnables;

	GLES1_TIME_STOP(GLES1_TIMES_glDisable);

	return;
}


/***********************************************************************************
 Function Name      : glAlphaFunc[x]
 Inputs             : func, ref
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets alpha test reference value and compare function.
					  Stored directly in TSP object word.
************************************************************************************/
static IMG_VOID SetUpAlphaTestParams(GLES1Context *gc, IMG_UINT32 ui32Func, IMG_FLOAT fRef)
{
	if ((ui32Func < GL_NEVER) || (ui32Func > GL_ALWAYS)) 
	{
		SetError(gc, GL_INVALID_ENUM);

		return;
	}

	fRef = Clampf(fRef, GLES1_Zero, GLES1_One);

	if((gc->sState.sRaster.ui32AlphaTestFunc != ui32Func) ||
	   ((gc->sState.sRaster.fAlphaTestRef < fRef) || (gc->sState.sRaster.fAlphaTestRef > fRef)))
	{
		gc->sState.sRaster.ui32AlphaTestFunc = ui32Func;
		gc->sState.sRaster.fAlphaTestRef	 = fRef;
		gc->sState.sRaster.ui32AlphaTestRef  = FloatToHWByte(fRef);

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
	}
}


#if defined(PROFILE_COMMON)
GL_API void GL_APIENTRY glAlphaFunc(GLenum func, GLclampf ref)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glAlphaFunc"));

	GLES1_TIME_START(GLES1_TIMES_glAlphaFunc);

	SetUpAlphaTestParams(gc, func, ref);

	GLES1_TIME_STOP(GLES1_TIMES_glAlphaFunc);
}
#endif

GL_API void GL_APIENTRY glAlphaFuncx(GLenum func, GLclampx ref)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glAlphaFuncx"));

	GLES1_TIME_START(GLES1_TIMES_glAlphaFuncx);

	SetUpAlphaTestParams(gc, func, FIXED_TO_FLOAT(ref));

	GLES1_TIME_STOP(GLES1_TIMES_glAlphaFuncx);
}


#if defined(GLES1_EXTENSION_BLEND_SUBTRACT)
/***********************************************************************************
 Function Name      : glBlendEquationOES
 Inputs             : mode
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets back end blend equation for both RGB and alpha
					  blending.
************************************************************************************/
GL_API_EXT void GL_APIENTRY glBlendEquationOES(GLenum mode)
{
	IMG_UINT32 ui32BlendEquation;

	__GLES1_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glBlendEquationOES"));

	GLES1_TIME_START(GLES1_TIMES_glBlendEquation);

	switch (mode)
	{
		case GL_FUNC_ADD_OES:
		{
			ui32BlendEquation =	(GLES1_BLENDMODE_ADD << GLES1_BLENDMODE_RGB_SHIFT) |
								(GLES1_BLENDMODE_ADD << GLES1_BLENDMODE_ALPHA_SHIFT);
			break;
		}
		case GL_FUNC_SUBTRACT_OES:
		{
			ui32BlendEquation =	(GLES1_BLENDMODE_SUBTRACT << GLES1_BLENDMODE_RGB_SHIFT) |
								(GLES1_BLENDMODE_SUBTRACT << GLES1_BLENDMODE_ALPHA_SHIFT);
			break;
		}
		case GL_FUNC_REVERSE_SUBTRACT_OES:
		{
			ui32BlendEquation =	(GLES1_BLENDMODE_REVSUBTRACT << GLES1_BLENDMODE_RGB_SHIFT) |
								(GLES1_BLENDMODE_REVSUBTRACT << GLES1_BLENDMODE_ALPHA_SHIFT);
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glBlendEquation);

			return;
		}
	}

	if(	gc->sState.sRaster.ui32BlendEquation != ui32BlendEquation)
	{
		gc->sState.sRaster.ui32BlendEquation = ui32BlendEquation;
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM|GLES1_DIRTYFLAG_TEXTURE_STATE;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glBlendEquation);
}
#endif /* defined(GLES1_EXTENSION_BLEND_SUBTRACT) */


#if defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE)
/***********************************************************************************
 Function Name      : glBlendEquationSeparateOES
 Inputs             : modeRGB, modeAlpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets separate back end blend equations for both
					  RGB and alpha blending.
************************************************************************************/
GL_API_EXT void GL_APIENTRY glBlendEquationSeparateOES(GLenum modeRGB, GLenum modeAlpha)
{
	IMG_UINT32 ui32RGBBlendEquation, ui32AlphaBlendEquation;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBlendEquationSeparateOES"));

	GLES1_TIME_START(GLES1_TIMES_glBlendEquationSeparate);

	switch (modeRGB) 
	{
		case GL_FUNC_ADD_OES:
		{
			ui32RGBBlendEquation = GLES1_BLENDMODE_ADD << GLES1_BLENDMODE_RGB_SHIFT;

			break;
		}
		case GL_FUNC_SUBTRACT_OES:
		{
			ui32RGBBlendEquation = GLES1_BLENDMODE_SUBTRACT << GLES1_BLENDMODE_RGB_SHIFT;

			break;
		}
		case GL_FUNC_REVERSE_SUBTRACT_OES:
		{
			ui32RGBBlendEquation = GLES1_BLENDMODE_REVSUBTRACT << GLES1_BLENDMODE_RGB_SHIFT;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glBlendEquationSeparate);

			return;
		}
	}

	switch (modeAlpha)
	{
		case GL_FUNC_ADD_OES:
		{
			ui32AlphaBlendEquation = GLES1_BLENDMODE_ADD << GLES1_BLENDMODE_ALPHA_SHIFT;

			break;
		}
		case GL_FUNC_SUBTRACT_OES:
		{
			ui32AlphaBlendEquation = GLES1_BLENDMODE_SUBTRACT << GLES1_BLENDMODE_ALPHA_SHIFT;

			break;
		}
		case GL_FUNC_REVERSE_SUBTRACT_OES:
		{
			ui32AlphaBlendEquation = GLES1_BLENDMODE_REVSUBTRACT << GLES1_BLENDMODE_ALPHA_SHIFT;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glBlendEquationSeparate);

			return;
		}
	}

	if(	gc->sState.sRaster.ui32BlendEquation != (ui32RGBBlendEquation | ui32AlphaBlendEquation))
	{
		gc->sState.sRaster.ui32BlendEquation = ui32RGBBlendEquation | ui32AlphaBlendEquation;
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glBlendEquationSeparate);
}
#endif /* defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE) */


/***********************************************************************************
 Function Name      : BlendFuncSeparate
 Inputs             : gc, srcRGB, dstRGB, srcAlpha, dstAlpha
 Outputs            : -
 Returns            : -
 Description        : Utility: Sets separate back end blend factors for both
					  RGB and alpha blending.
************************************************************************************/
static IMG_VOID BlendFuncSeparate(GLES1Context *gc, GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
	IMG_UINT32 ui32Factor, ui32BlendFunction;
	IMG_UINT32 i, aui32Factor[4];

	static const IMG_UINT32 aui32FactorShift[4] =
	{
		GLES1_BLENDFACTOR_RGBSRC_SHIFT,
		GLES1_BLENDFACTOR_ALPHASRC_SHIFT,
		GLES1_BLENDFACTOR_RGBDST_SHIFT,
		GLES1_BLENDFACTOR_ALPHADST_SHIFT
	};

	aui32Factor[0] = srcRGB;
	aui32Factor[1] = srcAlpha;
	aui32Factor[2] = dstRGB;
	aui32Factor[3] = dstAlpha;

	ui32BlendFunction = 0;

	for(i=0; i < 4; i++)
	{
		switch (aui32Factor[i]) 
		{
			case GL_ZERO:
			{
				ui32Factor = GLES1_BLENDFACTOR_ZERO;
				break;
			}
			case GL_ONE:
			{
				ui32Factor = GLES1_BLENDFACTOR_ONE;
				break;
			}
			case GL_DST_COLOR:
			{
				/* Only allowed on source factors */
				if(i < 2)
				{
					ui32Factor = GLES1_BLENDFACTOR_DSTCOLOR;
					break;
				}
				else
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}
			case GL_ONE_MINUS_DST_COLOR:
			{
				/* Only allowed on source factors */
				if(i < 2)
				{
					ui32Factor = GLES1_BLENDFACTOR_ONEMINUS_DSTCOLOR;
					break;
				}
				else
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}
			case GL_SRC_ALPHA:
			{
				ui32Factor = GLES1_BLENDFACTOR_SRCALPHA;

				break;
			}
			case GL_ONE_MINUS_SRC_ALPHA:
			{
				ui32Factor = GLES1_BLENDFACTOR_ONEMINUS_SRCALPHA;

				break;
			}
			case GL_DST_ALPHA:
			{
				ui32Factor = GLES1_BLENDFACTOR_DSTALPHA;

				break;
			}
			case GL_ONE_MINUS_DST_ALPHA:
			{
				ui32Factor = GLES1_BLENDFACTOR_ONEMINUS_DSTALPHA;
				break;
			}
			case GL_SRC_ALPHA_SATURATE:
			{
				/* Only allowed on source factors */
				if(i < 2)
				{
					ui32Factor = GLES1_BLENDFACTOR_SRCALPHA_SATURATE;
					break;
				}
				else
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}
			case GL_SRC_COLOR:
			{
				/* Only allowed on destination factors */
				if(i > 1)
				{
					ui32Factor = GLES1_BLENDFACTOR_SRCCOLOR;
					break;
				}
				else
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}
			case GL_ONE_MINUS_SRC_COLOR:
			{
				/* Only allowed on destination factors */
				if(i > 1)
				{
					ui32Factor = GLES1_BLENDFACTOR_ONEMINUS_SRCCOLOR;
					break;
				}
				else
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}
			default:
			{
				SetError(gc, GL_INVALID_ENUM);
				return;
			}
		}

		ui32BlendFunction |= (ui32Factor << aui32FactorShift[i]);
	}

	if(gc->sState.sRaster.ui32BlendFunction != ui32BlendFunction)
	{
		gc->sState.sRaster.ui32BlendFunction = ui32BlendFunction;

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE|GLES1_DIRTYFLAG_TEXTURE_STATE;
	}
}


#if defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE)
/***********************************************************************************
 Function Name      : glBlendFuncSeparateOES
 Inputs             : srcRGB, dstRGB, srcAlpha, dstAlpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets separate back end blend factors for both
					  RGB and alpha blending.
************************************************************************************/
GL_API_EXT void GL_APIENTRY glBlendFuncSeparateOES(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBlendFuncSeparateOES"));

	GLES1_TIME_START(GLES1_TIMES_glBlendFuncSeparate);

	BlendFuncSeparate(gc, srcRGB, dstRGB, srcAlpha, dstAlpha);

	GLES1_TIME_STOP(GLES1_TIMES_glBlendFuncSeparate);
}
#endif /* defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE) */


/***********************************************************************************
 Function Name      : glBlendFunc
 Inputs             : sfactor, dfactor
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets back end blend factors for both
					  RGB and alpha blending.
************************************************************************************/
GL_API void GL_APIENTRY glBlendFunc(GLenum sfactor, GLenum dfactor)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBlendFunc"));

	GLES1_TIME_START(GLES1_TIMES_glBlendFunc);

	BlendFuncSeparate(gc, sfactor, dfactor, sfactor, dfactor);

	GLES1_TIME_STOP(GLES1_TIMES_glBlendFunc);
}


/***********************************************************************************
 Function Name      : glCullface
 Inputs             : mode
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets culling face state.
					  Stores cull mode in internal state. Needs to be combined with
					  frontface direction to give actual cull mode.
************************************************************************************/
GL_API void GL_APIENTRY glCullFace(GLenum mode)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glCullFace"));

	GLES1_TIME_START(GLES1_TIMES_glCullFace);

	switch (mode)
	{
		case GL_FRONT:
		case GL_BACK:
		case GL_FRONT_AND_BACK:
		{
			if(gc->sState.sPolygon.eCullMode != mode)
			{
				gc->sState.sPolygon.eCullMode = mode;

				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
			}

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glCullFace);
}


/***********************************************************************************
 Function Name      : glDepthFunc
 Inputs             : cap
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets depth test compare function.
					  Stored in ISPTSP blend word format. Will be overridden by
					  Depth Func ALWAYS if depth test is disabled.
************************************************************************************/
GL_API void GL_APIENTRY glDepthFunc(GLenum cap)
{
	IMG_UINT32 ui32TestFunc;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDepthFunc"));

	GLES1_TIME_START(GLES1_TIMES_glDepthFunc);

	if ((cap < GL_NEVER) || (cap > GL_ALWAYS)) 
	{
		SetError(gc, GL_INVALID_ENUM);

		GLES1_TIME_STOP(GLES1_TIMES_glDepthFunc);

		return;
	}

	ui32TestFunc = ((cap - GL_NEVER) << EURASIA_ISPA_DCMPMODE_SHIFT) |
					(gc->sState.sDepth.ui32TestFunc & EURASIA_ISPA_DWRITEDIS);

	if(gc->sState.sDepth.ui32TestFunc != ui32TestFunc)
	{
		gc->sState.sDepth.ui32TestFunc = ui32TestFunc;

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glDepthFunc);
}


 /***********************************************************************************
 Function Name      : glDepthMask
 Inputs             : flag
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets depthbuffer writemask state. Sets the depth
					  write disable bit directly in ISPTSP control word.
************************************************************************************/
GL_API void GL_APIENTRY glDepthMask(GLboolean flag)
{
	IMG_BOOL bEnabled;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDepthMask"));

	GLES1_TIME_START(GLES1_TIMES_glDepthMask);

	bEnabled = (gc->sState.sDepth.ui32TestFunc & EURASIA_ISPA_DWRITEDIS) ? IMG_FALSE : IMG_TRUE;

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

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
	}
		
	GLES1_TIME_STOP(GLES1_TIMES_glDepthMask);
}


/***********************************************************************************
 Function Name      : glLineWidth(x)
 Inputs             : width
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current line width state
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glLineWidth(GLfloat width)
{
	IMG_FLOAT fSmoothWidth;
    IMG_FLOAT fAliasedWidth;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLineWidth"));

	GLES1_TIME_START(GLES1_TIMES_glLineWidth);

	if (width <= 0) 
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glLineWidth);

		return;
	}

	/* HW line width */
	fSmoothWidth = Clampf(width, (IMG_FLOAT)GLES1_SMOOTH_LINE_WIDTH_MIN, (IMG_FLOAT)GLES1_SMOOTH_LINE_WIDTH_MAX);
	fAliasedWidth = Clampf(width, (IMG_FLOAT)GLES1_ALIASED_LINE_WIDTH_MIN, (IMG_FLOAT)GLES1_ALIASED_LINE_WIDTH_MAX);

	if ( ((gc->sState.sLine.fSmoothWidth < fSmoothWidth) || (gc->sState.sLine.fSmoothWidth > fSmoothWidth)) ||
		 ((gc->sState.sLine.fAliasedWidth < fAliasedWidth) || (gc->sState.sLine.fAliasedWidth > fAliasedWidth)) )
	{
		gc->sState.sLine.fSmoothWidth  = fSmoothWidth;
		gc->sState.sLine.fAliasedWidth = fAliasedWidth;

        gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
	}
	
	GLES1_TIME_STOP(GLES1_TIMES_glLineWidth);
}
#endif


GL_API void GL_APIENTRY glLineWidthx(GLfixed width)
{
	IMG_FLOAT fSmoothWidth;
    IMG_FLOAT fAliasedWidth;
	IMG_INT32 i32Width = FIXED_TO_LONG(width);

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLineWidthx"));

	GLES1_TIME_START(GLES1_TIMES_glLineWidthx);

	if (i32Width <= 0) 
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glLineWidthx);

		return;
	}
	
	/* HW uses 1/2 line width */
	fSmoothWidth = (IMG_FLOAT)Clampi(i32Width, (IMG_INT32)GLES1_SMOOTH_LINE_WIDTH_MIN,	(IMG_INT32)GLES1_SMOOTH_LINE_WIDTH_MAX) ;
	
	fAliasedWidth = (IMG_FLOAT)Clampi(i32Width, (IMG_INT32)GLES1_ALIASED_LINE_WIDTH_MIN, (IMG_INT32)GLES1_ALIASED_LINE_WIDTH_MAX) ;

	if ( ((gc->sState.sLine.fSmoothWidth < fSmoothWidth) || (gc->sState.sLine.fSmoothWidth > fSmoothWidth)) || 
		 ((gc->sState.sLine.fAliasedWidth < fAliasedWidth) || (gc->sState.sLine.fAliasedWidth > fAliasedWidth)) )
	{
		gc->sState.sLine.fSmoothWidth  = fSmoothWidth;
		gc->sState.sLine.fAliasedWidth = fAliasedWidth;

        gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
    }


	GLES1_TIME_STOP(GLES1_TIMES_glLineWidthx);
}


/***********************************************************************************
 Function Name      : PointSize
 Inputs             : gc, fPointSize
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets current point size state
************************************************************************************/
static IMG_VOID PointSize(GLES1Context *gc, IMG_FLOAT fPointSize)
{
	IMG_FLOAT fSize;
	IMG_UINT32 ui32Size;

	if (fPointSize <= 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		return;
	}

	gc->sState.sPoint.fRequestedSize = fPointSize;

	gc->sState.sPoint.fMinSmoothSize =  Clampf(gc->sState.sPoint.fClampMin,
											  (IMG_FLOAT)GLES1_SMOOTH_POINT_SIZE_MIN,
											  (IMG_FLOAT)GLES1_SMOOTH_POINT_SIZE_MAX);

	gc->sState.sPoint.fMaxSmoothSize =  Clampf(gc->sState.sPoint.fClampMax,
											  (IMG_FLOAT)GLES1_SMOOTH_POINT_SIZE_MIN,
											  (IMG_FLOAT)GLES1_SMOOTH_POINT_SIZE_MAX);

	fSize = Clampf(fPointSize, gc->sState.sPoint.fMinSmoothSize, gc->sState.sPoint.fMaxSmoothSize);


	/* HW uses 1/2 point size */
	gc->sState.sPoint.fSmoothSize = fSize;

	gc->sState.sPoint.fMinAliasedSize =  Clampf(gc->sState.sPoint.fClampMin,
												GLES1_ALIASED_POINT_SIZE_MIN,
												GLES1_ALIASED_POINT_SIZE_MAX);

	gc->sState.sPoint.fMaxAliasedSize =  Clampf(gc->sState.sPoint.fClampMax,
										 	   GLES1_ALIASED_POINT_SIZE_MIN,
										 	   GLES1_ALIASED_POINT_SIZE_MAX);

	fSize = Clampf(fPointSize, gc->sState.sPoint.fMinAliasedSize, gc->sState.sPoint.fMaxAliasedSize);

	/* Aliased point size is rounded to nearest integer */
	ui32Size = (IMG_UINT32) (GLES1_Half + fSize);

	/* HW uses 1/2 point size */
	if ( (gc->sState.sPoint.fAliasedSize < (IMG_FLOAT)(ui32Size)) || (gc->sState.sPoint.fAliasedSize > (IMG_FLOAT)(ui32Size)) )
	{
	/* To ensure point size update */
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
		gc->sState.sPoint.fAliasedSize = (IMG_FLOAT)(ui32Size);
	}
}


/***********************************************************************************
 Function Name      : glPointSize(x)
 Inputs             : size
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current point size state
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glPointSize(GLfloat size)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPointSize"));

	GLES1_TIME_START(GLES1_TIMES_glPointSize);

	PointSize(gc, size);

	GLES1_TIME_STOP(GLES1_TIMES_glPointSize);
}
#endif

GL_API void GL_APIENTRY glPointSizex(GLfixed size)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPointSizex"));

	GLES1_TIME_START(GLES1_TIMES_glPointSizex);

	PointSize(gc, FIXED_TO_FLOAT(size));

	GLES1_TIME_STOP(GLES1_TIMES_glPointSizex);
}


/***********************************************************************************
 Function Name      : PointParameterfv
 Inputs             : gc, ePname, pfParams
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets current point parameter state
************************************************************************************/
static IMG_VOID PointParameterfv(GLES1Context *gc, GLenum ePname, const IMG_FLOAT *pfParams)
{	
	if ((ePname != GL_POINT_DISTANCE_ATTENUATION) && (pfParams[0] < 0)) 
	{
		SetError(gc, GL_INVALID_VALUE);
		return;
	}
		
	switch (ePname)
	{
		case GL_POINT_SIZE_MIN:
		{
			gc->sState.sPoint.fClampMin = pfParams[0];

			PointSize(gc, gc->sState.sPoint.fRequestedSize);

			break;
		}
		case GL_POINT_SIZE_MAX:
		{
			gc->sState.sPoint.fClampMax = pfParams[0];

			PointSize(gc, gc->sState.sPoint.fRequestedSize);

			break;
		}
		case GL_POINT_FADE_THRESHOLD_SIZE:
		{
			if ( (gc->sState.sPoint.fFade < pfParams[0]) || (gc->sState.sPoint.fFade > pfParams[0]) )
			{
			gc->sState.sPoint.fFade = pfParams[0];
				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
			}

			break;
		}
		case GL_POINT_DISTANCE_ATTENUATION:
		{
			/* PRQA S 3341 1 */ /* 1.0 and 0.0 can be exactly expressed in binary using IEEE float format */
			if ((pfParams[0] == 1.0) && (pfParams[1] == 0.0) && (pfParams[2] == 0.0))
			{
				gc->sState.sPoint.bAttenuationEnabled = IMG_FALSE;
			}
			else
			{
				gc->sState.sPoint.bAttenuationEnabled = IMG_TRUE;
				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
			}

			gc->sState.sPoint.afAttenuation[0] = pfParams[0];
			gc->sState.sPoint.afAttenuation[1] = pfParams[1];
			gc->sState.sPoint.afAttenuation[2] = pfParams[2];

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			return;
		}
	}

	
}


#if defined(PROFILE_COMMON)

/***********************************************************************************
 Function Name      : glPointParameter[f/x]v
 Inputs             : pname, params
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current point parameter state
************************************************************************************/
GL_API void GL_APIENTRY glPointParameterfv(GLenum pname, const GLfloat *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPointParameterfv"));

	GLES1_TIME_START(GLES1_TIMES_glPointParameterfv);

	PointParameterfv(gc, pname, params);

	GLES1_TIME_STOP(GLES1_TIMES_glPointParameterfv);
}


GL_API void GL_APIENTRY glPointParameterf(GLenum pname, GLfloat param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPointParameterf"));

	GLES1_TIME_START(GLES1_TIMES_glPointParameterf);

	switch (pname)
	{
		case GL_POINT_SIZE_MIN:
		case GL_POINT_SIZE_MAX:
		case GL_POINT_FADE_THRESHOLD_SIZE:
		{
			PointParameterfv(gc, pname, &param);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glPointParameterf);
}

#endif /* defined(PROFILE_COMMON) */

GL_API void GL_APIENTRY glPointParameterxv(GLenum pname, const GLfixed *params)
{
	IMG_FLOAT afParams[3];

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPointParameterxv"));

	GLES1_TIME_START(GLES1_TIMES_glPointParameterxv);

	afParams[0] = FIXED_TO_FLOAT(params[0]);

	if(pname == GL_POINT_DISTANCE_ATTENUATION)
	{
		afParams[1] = FIXED_TO_FLOAT(params[1]);
		afParams[2] = FIXED_TO_FLOAT(params[2]);
	}

	PointParameterfv(gc, pname, afParams);

	GLES1_TIME_STOP(GLES1_TIMES_glPointParameterxv);
}


GL_API void GL_APIENTRY glPointParameterx(GLenum pname, GLfixed param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPointParameterx"));

	GLES1_TIME_START(GLES1_TIMES_glPointParameterx);

	switch (pname)
	{
		case GL_POINT_SIZE_MIN:
		case GL_POINT_SIZE_MAX:
		case GL_POINT_FADE_THRESHOLD_SIZE:
		{
			IMG_FLOAT fParam;

			fParam = FIXED_TO_FLOAT(param);

			PointParameterfv(gc, pname, &fParam);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glPointParameterx);
}	


/***********************************************************************************
 Function Name      : glFrontFace
 Inputs             : mode
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets winding order for front-face determination.
					  Needs to be combined with cullface state to generate HW cull
					  mode info.
************************************************************************************/
GL_API void GL_APIENTRY glFrontFace(GLenum mode)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFrontFace"));

	GLES1_TIME_START(GLES1_TIMES_glFrontFace);

	switch (mode) 
	{
		case GL_CW:
		case GL_CCW:
		{
			if(gc->sState.sPolygon.eFrontFaceDirection != mode)
			{
				gc->sState.sPolygon.eFrontFaceDirection = mode;
				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
			}

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glFrontFace);
}


/***********************************************************************************
 Function Name      : glLogicOp
 Inputs             : opcode
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets frame buffer logical operation mode
					  Stores logical op state in ISPTSP word format. This is then
					  used based on Blend Enable and Color Mask state. (FB Blending
					  has precedence).
			    		Some MBX variants do not support Logical ops combined with:
							texture blends other than replace.
							fog
************************************************************************************/
GL_API void GL_APIENTRY glLogicOp(GLenum opcode)
{
	IMG_UINT32 ui32LogicOp;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLogicOp"));

	GLES1_TIME_START(GLES1_TIMES_glLogicOp);

	switch(opcode)
	{
		case GL_CLEAR:
		case GL_AND:
		case GL_AND_REVERSE:
		case GL_COPY:
		case GL_AND_INVERTED:
		case GL_NOOP:
		case GL_XOR:
		case GL_OR:
		case GL_NOR:
		case GL_EQUIV:
		case GL_INVERT:
		case GL_OR_REVERSE:
		case GL_COPY_INVERTED:
		case GL_OR_INVERTED:
		case GL_NAND:
		case GL_SET:
		{
			ui32LogicOp = opcode;
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glLogicOp);

			return;
		}
	}

	if(gc->sState.sRaster.ui32LogicOp != ui32LogicOp)
	{
		gc->sState.sRaster.ui32LogicOp = ui32LogicOp;

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glLogicOp);
}


/***********************************************************************************
 Function Name      : glPolygonOffset[x]
 Inputs             : factor, units
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets state for calculating z offset for polygons.
					  Sets z bias in primitive block header. Not fully supported in HW,
					  so use values that "look right". Halflife uses glPolygonOffset
					  of (1.0,4.0) and (-1.0,-4.0), while Quake 3 uses (-1.0,-2.0).
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glPolygonOffset(GLfloat factor, GLfloat units)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPolygonOffset"));

	GLES1_TIME_START(GLES1_TIMES_glPolygonOffset);

	if ( ((gc->sState.sPolygon.factor.fVal < factor) || (gc->sState.sPolygon.factor.fVal > factor)) || 
		((gc->sState.sPolygon.fUnits < units) || (gc->sState.sPolygon.fUnits > units)) )
	{
		gc->sState.sPolygon.factor.fVal = factor;
		gc->sState.sPolygon.fUnits  = units;
		
#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
		gc->sState.sPolygon.i32Units = (IMG_INT32)units;
#endif
	
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glPolygonOffset);
}
#endif

GL_API void GL_APIENTRY glPolygonOffsetx(GLfixed factor, GLfixed units)
{
	IMG_FLOAT fFactor, fUnits;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPolygonOffsetx"));

	GLES1_TIME_START(GLES1_TIMES_glPolygonOffsetx);

	fFactor = FIXED_TO_FLOAT(factor);
	fUnits  = FIXED_TO_FLOAT(units);

	if ( ((gc->sState.sPolygon.factor.fVal < fFactor) || (gc->sState.sPolygon.factor.fVal > fFactor)) || 
		((gc->sState.sPolygon.fUnits < fUnits) || (gc->sState.sPolygon.fUnits > fUnits)) )
	{
		gc->sState.sPolygon.factor.fVal = fFactor;
		gc->sState.sPolygon.fUnits  = fUnits;

#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
		gc->sState.sPolygon.i32Units = (IMG_INT32)fUnits;
#endif
	
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glPolygonOffsetx);
}


/***********************************************************************************
 Function Name      : glShadeModel
 Inputs             : mode
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets flat shading state for primitives.
					  Controls gouraud shading - sets up ISPTSP word directly.
************************************************************************************/
GL_API void GL_APIENTRY glShadeModel(GLenum mode)
{
	IMG_UINT32 ui32ShadeModel;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glShadeModel"));

	GLES1_TIME_START(GLES1_TIMES_glShadeModel);

	if ((mode < GL_FLAT) || (mode > GL_SMOOTH)) 
	{
		SetError(gc, GL_INVALID_ENUM);

		GLES1_TIME_STOP(GLES1_TIMES_glShadeModel);

		return;
	}

	if (mode == GL_SMOOTH)
	{
#if defined(SGX545)
		ui32ShadeModel = EURASIA_MTE_SHADE_RESERVED;
#else /* defined(SGX545) */
		ui32ShadeModel = EURASIA_MTE_SHADE_GOURAUD;
#endif /* defined(SGX545) */
	}
	else
	{
		ui32ShadeModel = EURASIA_MTE_SHADE_VERTEX2;
	}

	if (gc->sState.sShade.ui32ShadeModel != ui32ShadeModel)
	{
		gc->sState.sShade.ui32ShadeModel = ui32ShadeModel;

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glShadeModel);
}

/***********************************************************************************
 Function Name      : glStencilFunc
 Inputs             : func, ref, mask
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets stencil test state.
					  Ignored.
************************************************************************************/
GL_API void GL_APIENTRY glStencilFunc(GLenum func, GLint ref, GLuint mask)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glStencilFunc"));

	GLES1_TIME_START(GLES1_TIMES_glStencilFunc);

	if ((func < GL_NEVER) || (func > GL_ALWAYS)) 
	{
		SetError(gc, GL_INVALID_ENUM);

		GLES1_TIME_STOP(GLES1_TIMES_glStencilFunc);

		return;
	}

	gc->sState.sStencil.ui32StencilCompareMaskIn = mask;
	gc->sState.sStencil.i32StencilRefIn         = ref;

	ref = Clampi(ref, 0, (IMG_INT32)GLES1_MAX_STENCIL_VALUE);

	mask &= GLES1_MAX_STENCIL_VALUE;

	gc->sState.sStencil.ui32StencilRef = (IMG_UINT32)ref << EURASIA_ISPA_SREF_SHIFT;

	gc->sState.sStencil.ui32Stencil &= (EURASIA_ISPC_SCMP_CLRMSK & EURASIA_ISPC_SCMPMASK_CLRMSK);

	gc->sState.sStencil.ui32Stencil |= ((func - GL_NEVER) << EURASIA_ISPC_SCMP_SHIFT) | (mask << EURASIA_ISPC_SCMPMASK_SHIFT);

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;

	GLES1_TIME_STOP(GLES1_TIMES_glStencilFunc);
}


/***********************************************************************************
 Function Name      : glStencilop
 Inputs             : fail, zfail, zpass
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets stencil op state.
					  Ignored.
************************************************************************************/
GL_API void GL_APIENTRY glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
	IMG_UINT32 i;
	IMG_UINT32 aui32OpShift[3] = {EURASIA_ISPC_SOP1_SHIFT, EURASIA_ISPC_SOP2_SHIFT, EURASIA_ISPC_SOP3_SHIFT};
	IMG_UINT32 ui32StencilOp = 0;
	IMG_UINT32 aui32StencilOp[3];
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glStencilOp"));

	GLES1_TIME_START(GLES1_TIMES_glStencilOp);

	aui32StencilOp[0] = fail;
	aui32StencilOp[1] = zfail;
	aui32StencilOp[2] = zpass;

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
#if defined(GLES1_EXTENSION_STENCIL_WRAP)
			case GL_INCR_WRAP_OES:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_INC << aui32OpShift[i]);

				break;
			}
			case GL_DECR_WRAP_OES:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_DEC << aui32OpShift[i]);

				break;
			}
#endif /* defined(GLES1_EXTENSION_STENCIL_WRAP) */
			case GL_INVERT:
			{
				ui32StencilOp |= (EURASIA_ISPC_SOP_INVERT << aui32OpShift[i]);

				break;
			}
			default:
			{
				SetError(gc, GL_INVALID_ENUM);

				GLES1_TIME_STOP(GLES1_TIMES_glStencilOp);

				return;
			}
		}
	}

	ui32StencilOp |= (gc->sState.sStencil.ui32Stencil & EURASIA_ISPC_SOPALL_CLRMSK);

	if(gc->sState.sStencil.ui32Stencil != ui32StencilOp)
	{
		gc->sState.sStencil.ui32Stencil = ui32StencilOp;
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glStencilOp);
}


/***********************************************************************************
 Function Name      : glStencilMask
 Inputs             : mask
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets stencilbuffer writemask state.
************************************************************************************/
GL_API void GL_APIENTRY glStencilMask(GLuint mask)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glStencilMask"));

	GLES1_TIME_START(GLES1_TIMES_glStencilMask);

	gc->sState.sStencil.ui32StencilWriteMaskIn = mask;

	mask &= GLES1_MAX_STENCIL_VALUE;

	gc->sState.sStencil.ui32Stencil = (gc->sState.sStencil.ui32Stencil & EURASIA_ISPC_SWMASK_CLRMSK) | (mask << EURASIA_ISPC_SWMASK_SHIFT);

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;

	GLES1_TIME_STOP(GLES1_TIMES_glStencilMask);
}


/***********************************************************************************
 Function Name      : glColorMask
 Inputs             : red, green, blue, alpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets colorbuffer writemask state. Cannot be handled
					  fully. Some cases can be achieved using TAG write disable/ 
					  frame buffer blending.
************************************************************************************/
GL_API void GL_APIENTRY glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	IMG_UINT32 ui32ColorMask;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glColorMask"));

	GLES1_TIME_START(GLES1_TIMES_glColorMask);

	/* Store the state in the sense that mask means mask IN a plane */
	ui32ColorMask  = (alpha ? GLES1_COLORMASK_ALPHA : 0U);
	ui32ColorMask |= (blue  ? GLES1_COLORMASK_BLUE : 0U);
	ui32ColorMask |= (green ? GLES1_COLORMASK_GREEN : 0U);
	ui32ColorMask |= (red   ? GLES1_COLORMASK_RED : 0U);

	if(ui32ColorMask != gc->sState.sRaster.ui32ColorMask)
	{
		gc->sState.sRaster.ui32ColorMask = ui32ColorMask;

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glColorMask);
}


/***********************************************************************************
 Function Name      : glSampleCoverage(x)
 Inputs             : value, invert
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Stores value of multisample coverage state internally
					  Not used.
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glSampleCoverage(GLclampf value, GLboolean invert)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glSampleCoverage"));

	GLES1_TIME_START(GLES1_TIMES_glSampleCoverage);
	
	gc->sState.sMultisample.fSampleCoverageValue =	Clampf(value, GLES1_Zero, GLES1_One);
	gc->sState.sMultisample.bSampleCoverageInvert = invert ? IMG_TRUE : IMG_FALSE;

	GLES1_TIME_STOP(GLES1_TIMES_glSampleCoverage);
}

#endif

GL_API void GL_APIENTRY glSampleCoveragex(GLclampx value, GLboolean invert)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glSampleCoveragex"));

	GLES1_TIME_START(GLES1_TIMES_glSampleCoveragex);

	gc->sState.sMultisample.fSampleCoverageValue =	Clampf(FIXED_TO_FLOAT(value), GLES1_Zero, GLES1_One);
	gc->sState.sMultisample.bSampleCoverageInvert = invert ? IMG_TRUE : IMG_FALSE;

	GLES1_TIME_STOP(GLES1_TIMES_glSampleCoveragex);
}

/******************************************************************************
 End of file (state.c)
******************************************************************************/

