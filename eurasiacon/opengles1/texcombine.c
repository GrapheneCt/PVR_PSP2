/******************************************************************************
 * Name         : texcombine.c
 *
 * Copyright    : 2004-2006 by Imagination Technologies Limited.
 *                All rights reserved. No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system or
 *                translated into any human or computer language in any form
 *                by any means, electronic, mechanical, manual or otherwise,
 *                or disclosed to third parties without the express written
 *                permission of Imagination Technologies Limited, Home Park
 *                Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: texcombine.c $
 *****************************************************************************/

#include "context.h"


/***********************************************************************************
 Function Name      : InitTexCombineState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Initialising texture blend state
************************************************************************************/
IMG_INTERNAL IMG_VOID InitTexCombineState(GLES1Context *gc)
{
	IMG_UINT32 i;

	/* Init texture combine unit state */
	for (i=0; i<GLES1_MAX_TEXTURE_UNITS; i++) 
	{
		GLESTextureEnvState *psEnv = &gc->sState.sTexture.asUnit[i].sEnv;

		psEnv->ui32Mode = GLES1_MODULATE_INDEX;

		psEnv->ui32Op = GLES1_COMBINE_SET_COLOROP(psEnv->ui32Op, GLES1_COMBINEOP_MODULATE);
		psEnv->ui32Op = GLES1_COMBINE_SET_ALPHAOP(psEnv->ui32Op, GLES1_COMBINEOP_MODULATE);

		psEnv->ui32ColorSrcs = GLES1_COMBINE_SET_COLSRC(psEnv->ui32ColorSrcs, 0, GLES1_COMBINECSRC_TEXTURE);
		psEnv->ui32ColorSrcs = GLES1_COMBINE_SET_COLSRC(psEnv->ui32ColorSrcs, 1, GLES1_COMBINECSRC_PREVIOUS);
		psEnv->ui32ColorSrcs = GLES1_COMBINE_SET_COLSRC(psEnv->ui32ColorSrcs, 2, GLES1_COMBINECSRC_CONSTANT);

		psEnv->ui32ColorSrcs = GLES1_COMBINE_SET_COLOPERAND(psEnv->ui32ColorSrcs, 0, GLES1_COMBINECSRC_OPERANDCOLOR);
		psEnv->ui32ColorSrcs = GLES1_COMBINE_SET_COLOPERAND(psEnv->ui32ColorSrcs, 1, GLES1_COMBINECSRC_OPERANDCOLOR);
		psEnv->ui32ColorSrcs = GLES1_COMBINE_SET_COLOPERAND(psEnv->ui32ColorSrcs, 2, GLES1_COMBINECSRC_OPERANDALPHA);

		psEnv->ui32AlphaSrcs = GLES1_COMBINE_SET_ALPHASRC(psEnv->ui32AlphaSrcs, 0, GLES1_COMBINEASRC_TEXTURE);
		psEnv->ui32AlphaSrcs = GLES1_COMBINE_SET_ALPHASRC(psEnv->ui32AlphaSrcs, 1, GLES1_COMBINEASRC_PREVIOUS);
		psEnv->ui32AlphaSrcs = GLES1_COMBINE_SET_ALPHASRC(psEnv->ui32AlphaSrcs, 2, GLES1_COMBINEASRC_CONSTANT);
	}
}


/***********************************************************************************
 Function Name      : TexEnvCombine
 Inputs             : gc, pname, e
 Outputs            : -
 Returns            : -
 Description        : Packs texture combiner state into state structure
************************************************************************************/
IMG_INTERNAL IMG_VOID TexEnvCombine(GLES1Context *gc, GLenum pname, GLenum e)
{
	IMG_UINT32 ui32Op, i, ui32Src, ui32Unit;
	GLESTextureEnvState *psEnvState	= &gc->sState.sTexture.psActive->sEnv;

	switch (pname) 
	{
		case GL_COMBINE_RGB:
		{
			switch(e)
			{
				case GL_REPLACE:
				{
					ui32Op = GLES1_COMBINEOP_REPLACE;
					break;
				}
				case GL_MODULATE:
				{
					ui32Op = GLES1_COMBINEOP_MODULATE;
					break;
				}
				case GL_ADD:
				{
					ui32Op = GLES1_COMBINEOP_ADD;
					break;
				}
				case GL_ADD_SIGNED:
				{
					ui32Op = GLES1_COMBINEOP_ADDSIGNED;
					break;
				}
				case GL_INTERPOLATE:
				{
					ui32Op = GLES1_COMBINEOP_INTERPOLATE;
					break;
				}
				case GL_SUBTRACT:
				{
					ui32Op = GLES1_COMBINEOP_SUBTRACT;
					break;
				}
				case GL_DOT3_RGB:
				{
					ui32Op = GLES1_COMBINEOP_DOT3_RGB;
					break;
				}
				case GL_DOT3_RGBA:
				{
					ui32Op = GLES1_COMBINEOP_DOT3_RGBA;
					break;
				}
				default:
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}

			psEnvState->ui32Op = GLES1_COMBINE_SET_COLOROP(psEnvState->ui32Op, ui32Op);

			break;
		}
		case GL_COMBINE_ALPHA:
		{
			switch(e)
			{
				case GL_REPLACE:
				{
					ui32Op = GLES1_COMBINEOP_REPLACE;
					break;
				}
				case GL_MODULATE:
				{
					ui32Op = GLES1_COMBINEOP_MODULATE;
					break;
				}
				case GL_ADD:
				{
					ui32Op = GLES1_COMBINEOP_ADD;
					break;
				}
				case GL_ADD_SIGNED:
				{
					ui32Op = GLES1_COMBINEOP_ADDSIGNED;
					break;
				}
				case GL_INTERPOLATE:
				{
					ui32Op = GLES1_COMBINEOP_INTERPOLATE;
					break;
				}
				case GL_SUBTRACT:
				{
					ui32Op = GLES1_COMBINEOP_SUBTRACT;
					break;
				}
				default:
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}

			psEnvState->ui32Op = GLES1_COMBINE_SET_ALPHAOP(psEnvState->ui32Op, ui32Op);

			break;
		}
		case GL_SRC0_RGB:
		case GL_SRC1_RGB:
		case GL_SRC2_RGB:
		{
			i = pname - GL_SRC0_RGB;
			ui32Unit = e - GL_TEXTURE0;
			
			if (((IMG_INT32)ui32Unit < 0) || (GLES1_MAX_TEXTURE_UNITS <= ui32Unit))
			{
				switch(e)
				{
					case GL_TEXTURE:
					{
						ui32Src = GLES1_COMBINECSRC_TEXTURE;
						break;
					}
					case GL_CONSTANT:
					{
						ui32Src = GLES1_COMBINECSRC_CONSTANT;
						break;
					}
					case GL_PRIMARY_COLOR:
					{
						ui32Src = GLES1_COMBINECSRC_PRIMARY;
						break;
					}
					case GL_PREVIOUS:
					{
						ui32Src = GLES1_COMBINECSRC_PREVIOUS;
						break;
					}
					default:
					{
						SetError(gc, GL_INVALID_ENUM);
						return;
					}
				}

				/* Not crossbar, so clear crossbar state */
				psEnvState->ui32ColorSrcs = GLES1_COMBINE_CLEAR_COLCROSSBAR(psEnvState->ui32ColorSrcs, i);	
			}
			else
			{
#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
				/* Crossbar */
				if(e >= GL_TEXTURE0 && e <= GL_TEXTURE7)
				{
					ui32Src = GLES1_COMBINECSRC_TEXTURE;
					psEnvState->ui32ColorSrcs = GLES1_COMBINE_SET_COLCROSSBAR(psEnvState->ui32ColorSrcs, i, ui32Unit);
				}
				else
#endif /* defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR) */
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}

			psEnvState->ui32ColorSrcs = GLES1_COMBINE_SET_COLSRC(psEnvState->ui32ColorSrcs, i, ui32Src);
	
			break;
		}
		case GL_SRC0_ALPHA:
		case GL_SRC1_ALPHA:
		case GL_SRC2_ALPHA:
		{
			i = pname - GL_SRC0_ALPHA;
			ui32Unit = e - GL_TEXTURE0;
			
			if (((IMG_INT32)ui32Unit < 0) || (GLES1_MAX_TEXTURE_UNITS <= ui32Unit))
			{	
				switch(e)
				{
					case GL_TEXTURE:
					{
						ui32Src = GLES1_COMBINEASRC_TEXTURE;
						break;
					}
					case GL_CONSTANT:
					{
						ui32Src = GLES1_COMBINEASRC_CONSTANT;
						break;
					}
					case GL_PRIMARY_COLOR:
					{
						ui32Src = GLES1_COMBINEASRC_PRIMARY;
						break;
					}
					case GL_PREVIOUS:
					{
						ui32Src = GLES1_COMBINEASRC_PREVIOUS;
						break;
					}
					default:
					{
						SetError(gc, GL_INVALID_ENUM);
						return;
					}
				}

				/* Not crossbar, so clear crossbar state */
				psEnvState->ui32AlphaSrcs = GLES1_COMBINE_CLEAR_ALPHACROSSBAR(psEnvState->ui32AlphaSrcs, i);	
			}
			else
			{
#if defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR)
				/* Crossbar */
				if(e >= GL_TEXTURE0 && e <= GL_TEXTURE7)
				{
					ui32Src = GLES1_COMBINEASRC_TEXTURE;
					psEnvState->ui32AlphaSrcs = GLES1_COMBINE_SET_ALPHACROSSBAR(psEnvState->ui32AlphaSrcs, i, ui32Unit);
				}
				else
#endif /* defined(GLES1_EXTENSION_TEX_ENV_CROSSBAR) */
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}

			psEnvState->ui32AlphaSrcs = GLES1_COMBINE_SET_ALPHASRC(psEnvState->ui32AlphaSrcs, i, ui32Src);

			break;
		}
		case GL_OPERAND0_RGB:
		case GL_OPERAND1_RGB:
		case GL_OPERAND2_RGB:
		{
			i = pname - GL_OPERAND0_RGB;

			switch(e)
			{
				case GL_SRC_COLOR:
				{
					ui32Src = GLES1_COMBINECSRC_OPERANDCOLOR;
					break;
				}
				case GL_ONE_MINUS_SRC_COLOR:
				{
					ui32Src = GLES1_COMBINECSRC_OPERANDCOLOR | GLES1_COMBINECSRC_OPERANDONEMINUS;
					break;
				}
				case GL_SRC_ALPHA:
				{
					ui32Src = GLES1_COMBINECSRC_OPERANDALPHA;
					break;
				}
				case GL_ONE_MINUS_SRC_ALPHA:
				{
					ui32Src = GLES1_COMBINECSRC_OPERANDALPHA | GLES1_COMBINECSRC_OPERANDONEMINUS;
					break;
				}
				default:
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}

			psEnvState->ui32ColorSrcs = GLES1_COMBINE_SET_COLOPERAND(psEnvState->ui32ColorSrcs, i, ui32Src);

			break;
		}
		case GL_OPERAND0_ALPHA:
		case GL_OPERAND1_ALPHA:
		case GL_OPERAND2_ALPHA:
		{
			i = pname - GL_OPERAND0_ALPHA;

			switch(e)
			{
				case GL_SRC_ALPHA:
				{
					ui32Src = 0; /* No operand necessary for SRC_ALPHA */
					break;
				}
				case GL_ONE_MINUS_SRC_ALPHA:
				{
					ui32Src = GLES1_COMBINEASRC_OPERANDONEMINUS;
					break;
				}
				default:
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}

			psEnvState->ui32AlphaSrcs = GLES1_COMBINE_SET_ALPHAOPERAND(psEnvState->ui32AlphaSrcs, i, ui32Src);

			break;
		}
		case GL_RGB_SCALE:
		{
			switch(e)
			{
				case 1:
				{
					psEnvState->ui32Op = GLES1_COMBINE_SET_COLORSCALE(psEnvState->ui32Op, GLES1_COMBINEOP_SCALE_ONE);
					break;
				}
				case 2:
				{
					psEnvState->ui32Op = GLES1_COMBINE_SET_COLORSCALE(psEnvState->ui32Op, GLES1_COMBINEOP_SCALE_TWO);
					break;
				}
				case 4:
				{
					psEnvState->ui32Op = GLES1_COMBINE_SET_COLORSCALE(psEnvState->ui32Op, GLES1_COMBINEOP_SCALE_FOUR);
					break;
				}
				default:
				{
					SetError(gc, GL_INVALID_VALUE);
					return;
				}
			}

			break;
		}
		case GL_ALPHA_SCALE:
		{
			switch(e)
			{
				case 1:
				{
					psEnvState->ui32Op = GLES1_COMBINE_SET_ALPHASCALE(psEnvState->ui32Op, GLES1_COMBINEOP_SCALE_ONE);
					break;
				}
				case 2:
				{
					psEnvState->ui32Op = GLES1_COMBINE_SET_ALPHASCALE(psEnvState->ui32Op, GLES1_COMBINEOP_SCALE_TWO);
					break;
				}
				case 4:
				{
					psEnvState->ui32Op = GLES1_COMBINE_SET_ALPHASCALE(psEnvState->ui32Op, GLES1_COMBINEOP_SCALE_FOUR);
					break;
				}
				default:
				{
					SetError(gc, GL_INVALID_VALUE);
					return;
				}
			}

			break;
		}
	}
}

/******************************************************************************
 End of file (texcombine.c)
******************************************************************************/

