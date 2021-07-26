/******************************************************************************
 * Name         : fftex.c
 *
 * Copyright    : 2006-2009 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Description  : Hashes fixed-function texture blending state, then generates
 *                a suitable program to implement the blending using either
 *                UniFlex or native HW code.
 *
 * Platform     : ANSI
 *
 * $Log: fftex.c $
 ****************************************************************************/

#include "context.h"

/***********************************************************************************/

/*
 * Texture Format Summary:
 *
 * GL_ALPHA: 8 (Blend ignores colour channels)
 * GL_LUMINACE: 8 (Blend ignores alpha)
 * GL_LUMINANCE_ALPHA: 8:8
 * GL_INTENSITY: 8
 * GL_RGB: 565, 8888 (Alpha set to 0xff for latter)
 * GL_RGBA: 4444, 8888
 */

/*
 * Layer Blends
 */
#define GLES1_MTOP_REPMOD GLES1_COMBINE_SET_COLOROP(GLES1_COMBINE_SET_ALPHAOP(0, GLES1_COMBINEOP_MODULATE), GLES1_COMBINEOP_REPLACE)
#define GLES1_MTOP_REPREP GLES1_COMBINE_SET_COLOROP(GLES1_COMBINE_SET_ALPHAOP(0, GLES1_COMBINEOP_REPLACE),  GLES1_COMBINEOP_REPLACE)

#define GLES1_MTOP_MODREP GLES1_COMBINE_SET_COLOROP(GLES1_COMBINE_SET_ALPHAOP(0, GLES1_COMBINEOP_REPLACE),  GLES1_COMBINEOP_MODULATE)
#define GLES1_MTOP_MODMOD GLES1_COMBINE_SET_COLOROP(GLES1_COMBINE_SET_ALPHAOP(0, GLES1_COMBINEOP_MODULATE), GLES1_COMBINEOP_MODULATE)

#define GLES1_MTOP_LRPREP GLES1_COMBINE_SET_COLOROP(GLES1_COMBINE_SET_ALPHAOP(0, GLES1_COMBINEOP_REPLACE), GLES1_COMBINEOP_INTERPOLATE)
#define GLES1_MTOP_LRPMOD GLES1_COMBINE_SET_COLOROP(GLES1_COMBINE_SET_ALPHAOP(0, GLES1_COMBINEOP_MODULATE), GLES1_COMBINEOP_INTERPOLATE)
#define GLES1_MTOP_LRPLRP GLES1_COMBINE_SET_COLOROP(GLES1_COMBINE_SET_ALPHAOP(0, GLES1_COMBINEOP_INTERPOLATE), GLES1_COMBINEOP_INTERPOLATE)

#define GLES1_MTOP_ADDREP GLES1_COMBINE_SET_COLOROP(GLES1_COMBINE_SET_ALPHAOP(0, GLES1_COMBINEOP_REPLACE), GLES1_COMBINEOP_ADD)
#define GLES1_MTOP_ADDMOD GLES1_COMBINE_SET_COLOROP(GLES1_COMBINE_SET_ALPHAOP(0, GLES1_COMBINEOP_MODULATE), GLES1_COMBINEOP_ADD)
#define GLES1_MTOP_ADDADD GLES1_COMBINE_SET_COLOROP(GLES1_COMBINE_SET_ALPHAOP(0, GLES1_COMBINEOP_ADD), GLES1_COMBINEOP_ADD)

#define GLES1_MTCS_FRAG		GLES1_COMBINE_SET_COLSRC(GLES1_COMBINE_SET_COLSRC(GLES1_COMBINE_SET_COLSRC(0, 0, GLES1_COMBINECSRC_PREVIOUS), 1, GLES1_COMBINECSRC_TEXTURE), 2, GLES1_COMBINECSRC_CONSTANT)
#define GLES1_MTCS_TEX		GLES1_COMBINE_SET_COLSRC(GLES1_COMBINE_SET_COLSRC(GLES1_COMBINE_SET_COLSRC(0, 0, GLES1_COMBINECSRC_TEXTURE), 1, GLES1_COMBINECSRC_PREVIOUS), 2, GLES1_COMBINECSRC_CONSTANT)
#define GLES1_MTCS_BLEND	GLES1_COMBINE_SET_COLSRC(GLES1_COMBINE_SET_COLSRC(GLES1_COMBINE_SET_COLSRC(0, 0, GLES1_COMBINECSRC_CONSTANT), 1, GLES1_COMBINECSRC_PREVIOUS), 2, GLES1_COMBINECSRC_TEXTURE)
#define GLES1_MTCS_ABLEND	GLES1_COMBINE_SET_COLOPERAND(GLES1_COMBINE_SET_COLSRC(GLES1_COMBINE_SET_COLSRC(GLES1_COMBINE_SET_COLSRC(0, 0, GLES1_COMBINECSRC_TEXTURE), 1, GLES1_COMBINECSRC_PREVIOUS), 2, GLES1_COMBINECSRC_TEXTURE), 2, GLES1_COMBINECSRC_OPERANDALPHA)

#define GLES1_MTAS_FRAG		GLES1_COMBINE_SET_ALPHASRC(GLES1_COMBINE_SET_ALPHASRC(GLES1_COMBINE_SET_ALPHASRC(0, 0, GLES1_COMBINEASRC_PREVIOUS), 1, GLES1_COMBINEASRC_TEXTURE), 2, GLES1_COMBINEASRC_CONSTANT)
#define GLES1_MTAS_TEX		GLES1_COMBINE_SET_ALPHASRC(GLES1_COMBINE_SET_ALPHASRC(GLES1_COMBINE_SET_ALPHASRC(0, 0, GLES1_COMBINEASRC_TEXTURE), 1, GLES1_COMBINEASRC_PREVIOUS), 2, GLES1_COMBINEASRC_CONSTANT)
#define GLES1_MTAS_BLEND	GLES1_COMBINE_SET_ALPHASRC(GLES1_COMBINE_SET_ALPHASRC(GLES1_COMBINE_SET_ALPHASRC(0, 0, GLES1_COMBINEASRC_CONSTANT), 1, GLES1_COMBINEASRC_PREVIOUS), 2, GLES1_COMBINEASRC_TEXTURE)

typedef struct
{
	IMG_UINT32 ui32Op;
	IMG_UINT32 ui32ColorSrcs;
	IMG_UINT32 ui32AlphaSrcs;

} GLES1LayerOP;


static const GLES1LayerOP GLES1LayerBlends[GLES1_NUM_TEX_FORMATS][GLES1_NUM_TEX_BLENDS] =
{
	/* GL_ALPHA */
  {	{ GLES1_MTOP_REPMOD,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG },   /* Cv=Cf, Av=AfAt */
	{ GLES1_MTOP_REPREP,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG },   /* undef */
	{ GLES1_MTOP_REPMOD,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG },   /* Cv=Cf, Av=AfAt */
	{ GLES1_MTOP_REPREP,		GLES1_MTCS_FRAG,   GLES1_MTAS_TEX  },   /* Cv=Cf, Av=At */
	{ GLES1_MTOP_REPMOD,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG } }, /* Cv=Cf, Av=AfAt */
	/* GL_LUMINACE */
  { { GLES1_MTOP_MODREP,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG },   /* Cv=LtCf, Av=Af */
	{ GLES1_MTOP_REPREP,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG },   /* undef */
	{ GLES1_MTOP_LRPREP,		GLES1_MTCS_BLEND,  GLES1_MTAS_FRAG },   /* Cv=(1-Lt)Cf + LtCc, Av=Af */
	{ GLES1_MTOP_REPREP,		GLES1_MTCS_TEX,    GLES1_MTAS_FRAG },   /* Cv=Lt, Av=Af */
	{ GLES1_MTOP_ADDREP,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG } }, /* Cv=Cf+Lt, Av=Af */
	/* GL_LUMINANCE_ALPHA */
  { { GLES1_MTOP_MODMOD,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG },   /* Cv=LtCf, Av=AtAf */
	{ GLES1_MTOP_REPREP,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG },   /* undef */
	{ GLES1_MTOP_LRPMOD,		GLES1_MTCS_BLEND,  GLES1_MTAS_FRAG },   /* Cv=(1-Lt)Cf + LtCc, Av=AtAf */
	{ GLES1_MTOP_REPREP,		GLES1_MTCS_TEX,    GLES1_MTAS_TEX  },   /* Cv=Lt, Av=At */
	{ GLES1_MTOP_ADDMOD,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG } }, /* Cv=Cf+Lt, Av=AfAt */

	/* GL_RGB */
  { { GLES1_MTOP_MODREP,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG },   /* Cv=ItCf, Av=Af */
	{ GLES1_MTOP_REPREP,		GLES1_MTCS_TEX,    GLES1_MTAS_FRAG },   /* Cv=Ct, Av=Af */
	{ GLES1_MTOP_LRPREP,		GLES1_MTCS_BLEND,  GLES1_MTAS_FRAG },   /* Cv=(1-It)Cf + LtCc, Av=Af */
	{ GLES1_MTOP_REPREP,		GLES1_MTCS_TEX,    GLES1_MTAS_FRAG },   /* Cv=Ct, Av=Af */
	{ GLES1_MTOP_ADDREP,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG } }, /* Cv=Cf+Ct, Av=Af */
	/* GL_RGBA */
  { { GLES1_MTOP_MODMOD,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG },   /* Cv=ItCf, Av=AtAf */
	{ GLES1_MTOP_LRPREP,		GLES1_MTCS_ABLEND, GLES1_MTAS_FRAG },   /* Cv=(1-At)Cf + AtCt, Av=Af */
	{ GLES1_MTOP_LRPMOD,		GLES1_MTCS_BLEND,  GLES1_MTAS_FRAG },   /* Cv=(1-Ct)Cf + CtCc, Av=AtAf */
	{ GLES1_MTOP_REPREP,		GLES1_MTCS_TEX,    GLES1_MTAS_TEX  },   /* Cv=Ct, Av=At */
	{ GLES1_MTOP_ADDMOD,		GLES1_MTCS_FRAG,   GLES1_MTAS_FRAG } }  /* Cv=Cf+Ct, Av=AfAt */
};


/* Number of sources required by each operation sepecified in __GLSGXTexCombineOp */
IMG_INTERNAL const IMG_UINT32 ui32NumCombineOpSources[] = {1, 2, 2, 2, 3, 2, 2, 2};

/***********************************************************************************
 Function Name      : SetupBlendingOPs
 Description        : Extracts texture blend state from the gc into a texture layer
					  structure. This more friendly format is used as the input
					  to the texture blend state hashing and to create a fragment
					  program to implement the texturing.
************************************************************************************/
static IMG_BOOL SetupBlendingOPs(GLES1Context		*gc,
								 FFTBBlendLayerDesc	*psBlendLayer,
								 IMG_UINT32			 ui32SrcLayerNum,
								 IMG_BOOL			bFirstTextureLayer,
								 IMG_BOOL			*pbRequiresColour,
								 IMG_BOOL			*pbFetchAllLayers)
{
	IMG_BOOL bProjected, bTextureIterate;
	GLES1AttribArrayPointerMachine *psAPMachine;
	GLESTexture *psTex;
	GLESTextureEnvState *psEnvState;
	IMG_UINT32 ui32BaseFormatIndex, ui32Unit;
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	IMG_UINT32 ui32TextureTarget;
#endif

	ui32Unit = gc->ui32TexImageUnitsEnabled[ui32SrcLayerNum];

	psTex = gc->sTexture.apsBoundTexture[ui32Unit][gc->sTexture.aui32CurrentTarget[ui32Unit]];

	psEnvState = &gc->sState.sTexture.asUnit[ui32Unit].sEnv;

#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
	if(gc->sTexture.aui32CurrentTarget[ui32Unit] == GLES1_TEXTURE_TARGET_STREAM)
	{
		ui32BaseFormatIndex = psTex->psFormat->ui32BaseFormatIndex;
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		ui32TextureTarget   = GLES1_TEXTURE_TARGET_STREAM;
#endif
	}
	else
#endif
	if(psTex->ui32LevelsConsistent!=GLES1_TEX_CONSISTENT)
	{
		/* Black dummy texture */
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)		
		ui32TextureTarget   = FFTB_TEXLOAD_2D;
#endif
		ui32BaseFormatIndex = GLES1_LUMINANCE_ALPHA_TEX_INDEX;
	}
	else if(!psTex->bResidence)
	{
		/* White dummy texture */
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		ui32TextureTarget   = GLES1_TEXTURE_TARGET_2D;
#endif
		ui32BaseFormatIndex = GLES1_LUMINANCE_TEX_INDEX;
	}
	else
	{
		/* Normal texture */
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		ui32TextureTarget   = psTex->ui32TextureTarget;
#endif
		ui32BaseFormatIndex = psTex->psFormat->ui32BaseFormatIndex;
	}


	bTextureIterate = IMG_FALSE;

	if(psEnvState->ui32Mode == GLES1_COMBINE_INDEX)
	{
#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
		IMG_UINT32 ui32SrcUnit;
#endif /* defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR) */
		IMG_UINT32 i, ui32NumSrcs, ui32Src;

		psBlendLayer->ui32Op		 = psEnvState->ui32Op;
		psBlendLayer->ui32ColorSrcs = psEnvState->ui32ColorSrcs;
		psBlendLayer->ui32AlphaSrcs = psEnvState->ui32AlphaSrcs;

#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
		/* ARB_texture_env_crossbar may be used on this unit.
		   Loop through the sources, to see if any reference texture units that
		   are disabled or don't have valid textures bound to them.
		   Only inspect sources used by the current operation. */

		/* PRQA S ??? 1 */ /* ui32Op has been prevalidated in glTexEnv[i/x/f][v] and
							  cannot be larger than GLES1_COMBINEOP_DOT3_RGBA == 7 */
		ui32NumSrcs = ui32NumCombineOpSources[GLES1_COMBINE_GET_COLOROP(psBlendLayer->ui32Op)];

		for(i=0; i<ui32NumSrcs; i++)
		{
			ui32Src = GLES1_COMBINE_GET_COLCROSSBAR(psBlendLayer->ui32ColorSrcs, i);

				/* Does this source use a crossbar texture unit? */
			if(ui32Src & GLES1_COMBINECSRC_CROSSBAR)
			{
				ui32SrcUnit = ui32Src >> GLES1_COMBINECSRC_CROSSBAR_UNIT_SHIFT;
				
				*pbFetchAllLayers = IMG_TRUE;

				if((gc->ui32ImageUnitEnables & (1UL << ui32SrcUnit)) == 0)
				{
					/* Source accesses a disabled texture, so this unit must be disabled */
					return IMG_FALSE;
				}
			}
		}
	
		/* As above, for alpha sources */
		/* PRQA S ??? 1 */ /* ui32Op has been prevalidated in glTexEnv[i/x/f][v] and
							  cannot be larger than GLES1_COMBINEOP_DOT3_RGBA == 7 */
		ui32NumSrcs = ui32NumCombineOpSources[GLES1_COMBINE_GET_ALPHAOP(psBlendLayer->ui32Op)];

		for(i=0; i<ui32NumSrcs; i++)
		{
			ui32Src = GLES1_COMBINE_GET_ALPHACROSSBAR(psBlendLayer->ui32AlphaSrcs, i);

			if(ui32Src & GLES1_COMBINEASRC_CROSSBAR)
			{
				ui32SrcUnit = ui32Src >> GLES1_COMBINEASRC_CROSSBAR_UNIT_SHIFT;
			
				*pbFetchAllLayers = IMG_TRUE;
				
				if((gc->ui32ImageUnitEnables & (1UL <<ui32SrcUnit)) == 0)
				{
					return IMG_FALSE;
				}
			}
		}

#endif /* defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR) */

		/* PRQA S ??? 1 */ /* ui32Op has been prevalidated in glTexEnv[i/x/f][v] and
							  cannot be larger than GLES1_COMBINEOP_DOT3_RGBA == 7 */
		ui32NumSrcs = ui32NumCombineOpSources[GLES1_COMBINE_GET_COLOROP(psBlendLayer->ui32Op)];

		for(i=0; i<ui32NumSrcs; i++)
		{
			ui32Src = GLES1_COMBINE_GET_COLSRC(psBlendLayer->ui32ColorSrcs, i);

			if((ui32Src == GLES1_COMBINECSRC_PRIMARY) || ((ui32Src == GLES1_COMBINECSRC_PREVIOUS) && (ui32SrcLayerNum == 0)))
				*pbRequiresColour = IMG_TRUE;

			if(ui32Src == GLES1_COMBINECSRC_TEXTURE)
				bTextureIterate = IMG_TRUE;
		}
	
		/* As above, for alpha sources */
		/* PRQA S ??? 1 */ /* ui32Op has been prevalidated in glTexEnv[i/x/f][v] and
							  cannot be larger than GLES1_COMBINEOP_DOT3_RGBA == 7 */
		ui32NumSrcs = ui32NumCombineOpSources[GLES1_COMBINE_GET_ALPHAOP(psBlendLayer->ui32Op)];

		for(i=0; i<ui32NumSrcs; i++)
		{
			ui32Src = GLES1_COMBINE_GET_ALPHASRC(psBlendLayer->ui32AlphaSrcs, i);

			if((ui32Src == GLES1_COMBINEASRC_PRIMARY) || ((ui32Src == GLES1_COMBINEASRC_PREVIOUS) && (ui32SrcLayerNum == 0)))
				*pbRequiresColour = IMG_TRUE;
		
			if(ui32Src == GLES1_COMBINEASRC_TEXTURE)
				bTextureIterate = IMG_TRUE;
		}
	}
	else
	{
		const GLES1LayerOP *op;

		/*
		 * Only one Blend Mode OP table required - SetupTextureBlend() maps
		 * GLES1_COMBINECSRC_PREVIOUS to GLES1_COMBINECSRC_PRIMARY on layer 0
		 */
		op = &GLES1LayerBlends[ui32BaseFormatIndex][psEnvState->ui32Mode];

		psBlendLayer->ui32Op		 = op->ui32Op;
		psBlendLayer->ui32ColorSrcs = op->ui32ColorSrcs;
		psBlendLayer->ui32AlphaSrcs = op->ui32AlphaSrcs;

		/*
		 * Require base colour to be iterated if:
		 * This is the first texture layer AND the texture blend is NOT replace with an RGBA OR Luminance alpha texture
		 */
		if(bFirstTextureLayer &&
		   !(psEnvState->ui32Mode==GLES1_REPLACE_INDEX &&
		   (ui32BaseFormatIndex==GLES1_RGBA_TEX_INDEX || ui32BaseFormatIndex==GLES1_LUMINANCE_ALPHA_TEX_INDEX)))
		{
			*pbRequiresColour = IMG_TRUE;
		}

		bTextureIterate = IMG_TRUE;
 	}

	/* What kind of texture is being sampled? */
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	if(ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
	{
		psBlendLayer->ui32TexLoadInfo = FFTB_TEXLOAD_CEM;
	}
	else
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
	{
		psBlendLayer->ui32TexLoadInfo = FFTB_TEXLOAD_2D;
	}

	/* Is texture projection required? */
	bProjected = IMG_FALSE;


	psAPMachine = &(gc->sVAOMachine.asAttribPointer[AP_TEXCOORD0 + ui32SrcLayerNum]);

	/* Fixed function vertex processing: texture projection required if
		texture matrix is non-identity, or the application supplied
		4-dimensional texture coordinate data. */
	if((gc->sTransform.apsTexture[ui32SrcLayerNum]->sMatrix.eMatrixType != GLES1_MT_IDENTITY) ||
		psAPMachine->bIsCurrentState ||
		((psAPMachine->psState->ui32StreamTypeSize & GLES1_STREAMSIZE_MASK)>>GLES1_STREAMSIZE_SHIFT)==4)
	{
		bProjected = IMG_TRUE;
	}

	if(bProjected)
	{
		psBlendLayer->ui32TexLoadInfo |= FFTB_TEXLOAD_PROJECTED;
	}

	if(bTextureIterate)
	{
		psBlendLayer->ui32TexLoadInfo |= FFTB_TEXLOAD_NEEDTEX;
	}


	/* Is the texture alpha, luminance or luminance-alpha (8-bit)?
	   These formats require some swizzle code in the integer
	   texture blending path.
	   Floating-point textures must be identified to make sure
	   a different fragment program is generated (using UniFlex) */
	switch(ui32BaseFormatIndex)
	{
#if !defined(SGX_FEATURE_TAG_LUMINANCE_ALPHA)
		case GLES1_ALPHA_TEX_INDEX:
		{
			psBlendLayer->ui32TexLoadInfo |= FFTB_TEXLOAD_ALPHA8;

			psBlendLayer->ui32Unpack = FFTB_TEXUNPACK_COLOR;

			break;
		}
		case GLES1_LUMINANCE_TEX_INDEX:
		{
			psBlendLayer->ui32TexLoadInfo |= FFTB_TEXLOAD_LUMINANCE8;

			psBlendLayer->ui32Unpack = FFTB_TEXUNPACK_ALPHA;

			break;
		}
#endif /* !defined(SGX_FEATURE_TAG_LUMINANCE_ALPHA) */
#if !defined(SGX_FEATURE_TAG_LUMINANCE_ALPHA) || defined(FIX_HW_BRN_29373)
		case GLES1_LUMINANCE_ALPHA_TEX_INDEX:
		{
			psBlendLayer->ui32TexLoadInfo |= FFTB_TEXLOAD_LUMINANCEALPHA8;

			psBlendLayer->ui32Unpack = FFTB_TEXUNPACK_COLOR;
#if defined(FIX_HW_BRN_29373)
			psBlendLayer->ui32Unpack |= FFTB_TEXUNPACK_ALPHA;
#endif
			break;
		}
#endif
#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GLES1_RGB_TEX_INDEX:
		{
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
			if(psTex->psBufferDevice && psTex->psBufferDevice->psExtTexState->sYUVRegisters.bUseFIRH)
#else
			if(psTex->psExtTexState && psTex->psExtTexState->sYUVRegisters.bUseFIRH)
#endif
			{
				switch(psTex->psFormat->ePixelFormat)
				{
					case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY:
					case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY:
					case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU:
					case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV:
					{
						psBlendLayer->ui32TexLoadInfo |= FFTB_TEXLOAD_YUVCSC;

						psBlendLayer->ui32Unpack = FFTB_TEXUNPACK_ALPHA;
						break;
					}
					case PVRSRV_PIXEL_FORMAT_NV12:
					{
						psBlendLayer->ui32TexLoadInfo |= FFTB_TEXLOAD_YUVCSC_420_2PLANE;

						psBlendLayer->ui32Unpack = FFTB_TEXUNPACK_ALPHA;
						break;
					}
					case PVRSRV_PIXEL_FORMAT_YV12:
					{
						psBlendLayer->ui32TexLoadInfo |= FFTB_TEXLOAD_YUVCSC_420_3PLANE;

						psBlendLayer->ui32Unpack = FFTB_TEXUNPACK_ALPHA;
						break;
					}
					default:
					{
						psBlendLayer->ui32Unpack = 0;
						break;
					}
				}
			}

			break;
		}
#endif /* GLES1_EXTENSION_TEXTURE_STREAM || GLES1_EXTENSION_EGL_IMAGE_EXTERNAL */
		default:
		{
			psBlendLayer->ui32Unpack = 0;
			break;
		}
	}


	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : DestroyHashedBlendState
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyHashedBlendState(GLES1Context *gc, IMG_UINT32 ui32Item)
{
	GLES1Shader *psFragmentShader;
	FFTBProgramDesc *psFFTBProgramDesc;

	psFragmentShader = (GLES1Shader *)ui32Item;

	psFFTBProgramDesc = psFragmentShader->u.sFFTB.psFFTBProgramDesc;

	if(psFFTBProgramDesc)
	{
		if(psFFTBProgramDesc->psFFGENProgramDetails)
		{
			if(psFFTBProgramDesc->psFFGENProgramDetails->pui32Instructions)
			{
				GLES1Free(gc, psFFTBProgramDesc->psFFGENProgramDetails->pui32Instructions);
			}

			GLES1Free(gc, psFFTBProgramDesc->psFFGENProgramDetails);
		}

		FreeUSEASMInstructionList(gc, &psFFTBProgramDesc->sUSEASMInfo);

		GLES1Free(gc, psFFTBProgramDesc);
	}

	/* Unlink shader */
	if(psFragmentShader->psPrevious)
	{
		psFragmentShader->psPrevious->psNext = psFragmentShader->psNext;
	}
	else
	{
		/* Shader is at head of list */
		gc->sProgram.psFragment = psFragmentShader->psNext;
	}

	if(psFragmentShader->psNext)
	{
		psFragmentShader->psNext->psPrevious = psFragmentShader->psPrevious;
	}

	FreeShader(gc, psFragmentShader);
}



/***********************************************************************************
 Function Name      : ReuseHashedBlendState
 Description        : Hashes the texture layer state and checks the hash table for
					  a matching entry. If no matching entry is found, the blend
					  state is created from the texture layer state.
 Outputs			: hwcx->nCurrentFPName is assigned the SGL name of the 
					  re-used or newly created fragment program
************************************************************************************/
static GLES1_MEMERROR ReuseHashedBlendState(GLES1Context			*gc,
									        FFTBBlendLayerDesc	*psBlendLayers,
									        IMG_BOOL				 bAnyTexturingEnabled,
									        IMG_BOOL				*pbTexturingEnabled, 
									        IMG_UINT32 			ui32ExtraBlendState)
{
	GLES1Shader *psFragmentShader;

	/* Arbitrary initial value, taken from statehash.c:hash2() */
	HashValue tTextureBlendStateHashValue = STATEHASH_INIT_VALUE;
	IMG_UINT32 *pui32HashData, ui32HashDataSizeInDWords;

	ui32HashDataSizeInDWords = 2 + (gc->ui32NumImageUnitsActive * sizeof(FFTBBlendLayerDesc)) / sizeof(IMG_UINT32);

	/* Setup an array with extra data that needs to be hashed
	   (so we don't call the hash function many times) */

	pui32HashData = gc->sProgram.uTempBuffer.aui32HashState;

	/* Extra blend state */
	pui32HashData[0] = ui32ExtraBlendState;

	/* Hash the unit enables */
	pui32HashData[1] = gc->ui32ImageUnitEnables;

	/* Copy texture blend info into hash data */
	GLES1MemCopy(&pui32HashData[2], psBlendLayers, (gc->ui32NumImageUnitsActive * sizeof(FFTBBlendLayerDesc))); 

	/* Hash the state */
	tTextureBlendStateHashValue = HashFunc(pui32HashData, ui32HashDataSizeInDWords, tTextureBlendStateHashValue);

	/* Search hash table to see if we have already stored this state */
	if(!HashTableSearch(gc, 
						&gc->sProgram.sFFTextureBlendHashTable, tTextureBlendStateHashValue, 
						pui32HashData, ui32HashDataSizeInDWords, 
						(IMG_UINT32 *)&psFragmentShader))
	{
		FFTBProgramDesc *psFFTBProgramDesc;

		if (!ValidateHashTableInsert(gc, &gc->sProgram.sFFTextureBlendHashTable, tTextureBlendStateHashValue))
		{
			PVR_DPF((PVR_DBG_ERROR,"ReuseHashedBlendState: Cannot insert a new item into hash table"));
			return GLES1_GENERAL_MEM_ERROR;
		}

		pui32HashData = GLES1Malloc(gc, ui32HashDataSizeInDWords*sizeof(IMG_UINT32));

		if(!pui32HashData)
		{
			PVR_DPF((PVR_DBG_ERROR,"ReuseHashedBlendState: malloc() failed"));

			return GLES1_GENERAL_MEM_ERROR;
		}

		GLES1MemCopy(pui32HashData, gc->sProgram.uTempBuffer.aui32HashState, ui32HashDataSizeInDWords*sizeof(IMG_UINT32));

		psFFTBProgramDesc = GLES1Malloc(gc, sizeof(FFTBProgramDesc));

		if(!psFFTBProgramDesc)
		{
			GLES1Free(gc, pui32HashData);

			PVR_DPF((PVR_DBG_ERROR,"ReuseHashedBlendState: malloc() failed"));

			return GLES1_GENERAL_MEM_ERROR;
		}
		
		psFragmentShader = GLES1Calloc(gc, sizeof(GLES1Shader));

		if(!psFragmentShader)
		{
			PVR_DPF((PVR_DBG_ERROR,"ReuseHashedBlendState: malloc() failed"));

			GLES1Free(gc, psFFTBProgramDesc);

			return GLES1_GENERAL_MEM_ERROR;
		}

		if(!CreateIntegerFragmentProgramFromFF(gc,
										   psBlendLayers,
										   bAnyTexturingEnabled,
										   pbTexturingEnabled,
										   ui32ExtraBlendState,
										   psFFTBProgramDesc))
		{
			PVR_DPF((PVR_DBG_ERROR,"ReuseHashedBlendState: CreateIntegerFragmentProgramFromFF failed"));

			GLES1Free(gc, psFFTBProgramDesc);
			GLES1Free(gc, psFragmentShader);
			return GLES1_GENERAL_MEM_ERROR;
		}

		HashTableInsert(gc, 
						&gc->sProgram.sFFTextureBlendHashTable, tTextureBlendStateHashValue, 
						pui32HashData, ui32HashDataSizeInDWords, 
						(IMG_UINT32)psFragmentShader);

		/* Add this shader to the head of the list of all fragment shaders */
		psFragmentShader->psPrevious = IMG_NULL;
		psFragmentShader->psNext = gc->sProgram.psFragment;

		if(gc->sProgram.psFragment)
		{
			gc->sProgram.psFragment->psPrevious = psFragmentShader;
		}

		gc->sProgram.psFragment = psFragmentShader;

		psFragmentShader->u.sFFTB.psFFTBProgramDesc = psFFTBProgramDesc;

		psFragmentShader->psFFGENProgramDetails = psFFTBProgramDesc->psFFGENProgramDetails;
	}
	else
	{
		if(!psFragmentShader->u.sFFTB.psFFTBProgramDesc)
		{
			PVR_DPF((PVR_DBG_ERROR,"ReuseHashedBlendState: Existing shader code not found"));

			gc->sProgram.psCurrentFragmentShader = IMG_NULL;

			return GLES1_GENERAL_MEM_ERROR;
		}
	}

	/* Set this shader to be the current one */
	gc->sProgram.psCurrentFragmentShader = psFragmentShader;

	return GLES1_NO_ERROR;
}


/***********************************************************************************
 Function Name      : AddFFTextureBinding
 Description        : Adds a binding to the program info and returns the constant 
					  offset, in terms of SCALARS. 
					  
				      If binding already exists it returns the existing
					  constant offset
************************************************************************************/
IMG_INTERNAL IMG_UINT32 AddFFTextureBinding(FFTBProgramDesc *psFFTBProgramDesc,
										   FFTBBindingType	eBindingType,
										   IMG_UINT32		ui32BindingValue)
{
	IMG_UINT32 i;

	for(i=0; i<psFFTBProgramDesc->ui32NumBindings; i++)
	{
		if((psFFTBProgramDesc->asBindings[i].eType == eBindingType) &&
		   (psFFTBProgramDesc->asBindings[i].ui32Value == ui32BindingValue))
		{
			/* Binding found, return constant offset */
			return psFFTBProgramDesc->asBindings[i].ui32ConstantOffset;
		}
	}

	/* Binding wasn't found, so this is a new binding */
	psFFTBProgramDesc->ui32NumBindings++;

	/* Setup binding values */
	psFFTBProgramDesc->asBindings[i].eType = eBindingType;
	psFFTBProgramDesc->asBindings[i].ui32Value = ui32BindingValue;
	psFFTBProgramDesc->asBindings[i].ui32ConstantOffset = psFFTBProgramDesc->ui32CurrentConstantOffset;

	switch(eBindingType)
	{
		case FFTB_BINDING_FACTOR_SCALAR:
		case FFTB_BINDING_IMMEDIATE_SCALAR:
		{
			/* Scalars */
			psFFTBProgramDesc->ui32CurrentConstantOffset += 1;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_WARNING,"AddFFTextureBinding(): Unknown binding type"));

			break;
		}
	}

	return psFFTBProgramDesc->asBindings[i].ui32ConstantOffset;
}
 
 
/***********************************************************************************
 Function Name      : ValidateFFTextureConstants
 Description        : Validates the constants specified in the fragment shader
************************************************************************************/
IMG_INTERNAL IMG_VOID ValidateFFTextureConstants(GLES1Context *gc)
{
	IMG_FLOAT		*pfConstants;
	FFTBBindingInfo	*psBinding;
	IMG_UINT32		i, ui32NumConsts;
	FFTBProgramDesc *psFFTBProgramDesc;
	GLES1Shader *psFragmentShader;

	psFragmentShader = gc->sProgram.psCurrentFragmentShader;

	psFFTBProgramDesc = psFragmentShader->u.sFFTB.psFFTBProgramDesc;

	if(psFFTBProgramDesc->ui32NumBindings)
	{
		IMG_UINT32 ui32SizeOfConstants;

		/* This will over-allocate for scalar constants, but should not matter */
		ui32SizeOfConstants = 4 * sizeof(IMG_FLOAT) * psFFTBProgramDesc->ui32NumBindings;

		if(psFragmentShader->ui32SizeOfConstants != ui32SizeOfConstants)
		{
			IMG_FLOAT *pfNewConstantData = GLES1Realloc(gc, psFragmentShader->pfConstantData, ui32SizeOfConstants);

			if(!pfNewConstantData)
			{
				PVR_DPF((PVR_DBG_ERROR,"ValidateFFTextureConstants: Failed to allocate constant data"));

				SetError(gc, GL_OUT_OF_MEMORY);

				return;
			}

			psFragmentShader->pfConstantData = pfNewConstantData;
			psFragmentShader->ui32SizeOfConstants = ui32SizeOfConstants;
		}

		pfConstants = psFragmentShader->pfConstantData;

		ui32NumConsts = 0;

		for(i=0; i<psFFTBProgramDesc->ui32NumBindings; i++)
		{
			psBinding = &psFFTBProgramDesc->asBindings[i];

			switch(psBinding->eType)
			{
				case FFTB_BINDING_FACTOR_SCALAR:
				{
					IMG_UINT32 *puConst = (IMG_UINT32 *)&pfConstants[ui32NumConsts++];

					*puConst = gc->sState.sTexture.asUnit[psBinding->ui32Value].sEnv.ui32Color;

					break;
				}
				case FFTB_BINDING_IMMEDIATE_SCALAR:
				{
					IMG_UINT32 *puConst = (IMG_UINT32 *)&pfConstants[ui32NumConsts++];

					*puConst = psBinding->ui32Value;

					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_WARNING,"ValidateFFTextureConstants(): Unknown constant type"));

					break;
				}
			}
		}
	}
}


/***********************************************************************************
 Function Name      : ValidateFFTextureBlending
 Description        : Creates a fragment program to implement the current
					  fixed-function texturing state. Re-uses hashed state, if
					  possible.
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR ValidateFFTextureBlending(GLES1Context *gc)
{
	FFTBBlendLayerDesc asBlendLayer[GLES1_MAX_TEXTURE_UNITS];
	IMG_BOOL pabTexturingEnabled[GLES1_MAX_TEXTURE_UNITS];
	IMG_UINT32 ui32ExtraBlendState;
	IMG_BOOL bAnyTexturingEnabled, bFirstTextureLayer, bRequiresColour, bFetchAllLayers;
	IMG_UINT32 i;
    GLES1_MEMERROR eError;

	ui32ExtraBlendState = 0;

	PVRSRVMemSet(asBlendLayer, 0x0, GLES1_MAX_TEXTURE_UNITS * sizeof(FFTBBlendLayerDesc));
	PVRSRVMemSet(pabTexturingEnabled, 0x0, GLES1_MAX_TEXTURE_UNITS * sizeof(IMG_BOOL));

	if(((gc->ui32TnLEnables & GLES1_TL_LIGHTING_ENABLE) != 0) &&
	    gc->sState.sLight.sModel.bTwoSided &&
	   (gc->sPrim.eCurrentPrimitiveType>=GLES1_PRIMTYPE_TRIANGLE) &&
	   (gc->sPrim.eCurrentPrimitiveType<=GLES1_PRIMTYPE_TRIANGLE_FAN))
	{
		IMG_BOOL bFrontFaceCW = (gc->sState.sPolygon.eFrontFaceDirection == GL_CW) ? IMG_TRUE : IMG_FALSE;
		IMG_BOOL bYInverted = 	(gc->psDrawParams->eRotationAngle == PVRSRV_FLIP_Y) ? IMG_TRUE : IMG_FALSE;
		
		ui32ExtraBlendState |= FFTB_BLENDSTATE_TWOSIDED_LIGHTING;

		if((bFrontFaceCW && !bYInverted) || (!bFrontFaceCW && bYInverted))
		{
			ui32ExtraBlendState |= FFTB_BLENDSTATE_FRONTFACE_CW;
		}
	}

	bAnyTexturingEnabled = IMG_FALSE;
	bFirstTextureLayer   = IMG_TRUE;
	bRequiresColour      = IMG_FALSE;
	bFetchAllLayers		 = IMG_FALSE;

	/* Extract blend state into a useable form */
	for(i=0; i<gc->ui32NumImageUnitsActive; i++)
	{
		pabTexturingEnabled[i] = SetupBlendingOPs(gc, &asBlendLayer[i], i, bFirstTextureLayer, &bRequiresColour, &bFetchAllLayers);

		if(pabTexturingEnabled[i])
		{
			bAnyTexturingEnabled = IMG_TRUE;

			bFirstTextureLayer = IMG_FALSE;
		}
	}

	if(bFetchAllLayers)
	{
		ui32ExtraBlendState |= FFTB_BLENDSTATE_NEEDS_ALL_LAYERS;
	}

	if(!bAnyTexturingEnabled || bRequiresColour)
	{
		ui32ExtraBlendState |= FFTB_BLENDSTATE_NEEDS_BASE_COLOUR;
	}
	else
	{
		ui32ExtraBlendState &= ~FFTB_BLENDSTATE_TWOSIDED_LIGHTING;
		PVR_DPF((PVR_DBG_VERBOSE, "ValidateFFTextureBlending: Base colour iteration has been optimised out"));
	}

	if(gc->sPrim.sRenderState.ui32AlphaTestFlags)
	{
		ui32ExtraBlendState |= FFTB_BLENDSTATE_ALPHATEST;

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
		if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK)
		{
			ui32ExtraBlendState |= FFTB_BLENDSTATE_ALPHATEST_FEEDBACK;
		}
#endif
	}

	if(gc->ui32RasterEnables & GLES1_RS_FOG_ENABLE)
	{
		ui32ExtraBlendState |= FFTB_BLENDSTATE_FOG;
	}

    eError = ReuseHashedBlendState(gc, asBlendLayer, bAnyTexturingEnabled, pabTexturingEnabled, ui32ExtraBlendState);

    return eError;
}


/******************************************************************************
 End of file (fftex.c)
******************************************************************************/
