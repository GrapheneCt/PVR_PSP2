/**************************************************************************
 * Name         : fog.c
 *
 * Copyright    : 2004-2006 by Imagination Technologies Limited. All rights reserved.
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
 *
 * $Log: fog.c $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "context.h"

/***********************************************************************************
 Function Name      : Fogfv
 Inputs             : gc, ePname, pfParams
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets fog state specified by pname.
					  Fog start, end and density are used to calculate a per vertex 
					  fog factor value (sent through offset alpha). This offset alpha 
					  *incorrectly* takes flat shading control from ISPTSP control.
					  Fog colour can be set by a register (per frame), or through 
					  "special" ISPTSP objects which can update the register between 
					  primitives.
************************************************************************************/
static IMG_VOID Fogfv(GLES1Context *gc, GLenum ePname, const GLfloat *pfParams)
{
	switch (ePname)
	{
		case GL_FOG_COLOR:
		{			
			IMG_UINT32 ui32Color;
			
			gc->sState.sFog.sColor.fRed   = Clampf(pfParams[0], GLES1_Zero, GLES1_One);
			gc->sState.sFog.sColor.fGreen = Clampf(pfParams[1], GLES1_Zero, GLES1_One);
			gc->sState.sFog.sColor.fBlue  = Clampf(pfParams[2], GLES1_Zero, GLES1_One);
			gc->sState.sFog.sColor.fAlpha = Clampf(pfParams[3], GLES1_Zero, GLES1_One);

			ui32Color = ColorConvertToHWFormat(&gc->sState.sFog.sColor);
			
			if(gc->sState.sFog.ui32Color != ui32Color)
			{
				gc->sState.sFog.ui32Color = ui32Color;
				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;
			}
			
			break;			
		}
		case GL_FOG_DENSITY:
		{
			if (pfParams[0] < 0)
			{
				SetError(gc, GL_INVALID_VALUE);
				return;
			}

			gc->sState.sFog.fDensity = pfParams[0];			

			gc->ui32DirtyMask |=  GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
		case GL_FOG_END:
		{
			gc->sState.sFog.fEnd = pfParams[0];	
			gc->sState.sFog.fOneOverEMinusS = (gc->sState.sFog.fEnd > gc->sState.sFog.fStart || gc->sState.sFog.fEnd < gc->sState.sFog.fStart) ? 
				GLES1_One / (gc->sState.sFog.fEnd - gc->sState.sFog.fStart) : GLES1_Zero;

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
		case GL_FOG_START:
		{
			gc->sState.sFog.fStart = pfParams[0];			
			gc->sState.sFog.fOneOverEMinusS = (gc->sState.sFog.fEnd > gc->sState.sFog.fStart || gc->sState.sFog.fEnd < gc->sState.sFog.fStart) ? 
				GLES1_One / (gc->sState.sFog.fEnd - gc->sState.sFog.fStart) : GLES1_Zero;

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
		case GL_FOG_MODE:
		{
			switch ((GLenum) pfParams[0]) 
			{
				case GL_EXP:
				case GL_EXP2:
				case GL_LINEAR:
				{
					if(gc->sState.sFog.eMode != (GLenum)pfParams[0])
					{
						gc->sState.sFog.eMode = (GLenum)pfParams[0];
						gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTEX_PROGRAM;
					}

					break;
				}
				default:
				{
					SetError(gc, GL_INVALID_ENUM);

					return;
				}
			}

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
 Function Name      : glFog[f/fv/x/xv]
 Inputs             : gc, pname, param[s]
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets fog state specified by pname.
************************************************************************************/
GL_API void GL_APIENTRY glFogfv(GLenum pname, const GLfloat *params)
{
    __GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFogfv"));

	GLES1_TIME_START(GLES1_TIMES_glFogfv);
	
	Fogfv(gc, pname, params);

	GLES1_TIME_STOP(GLES1_TIMES_glFogfv);
}


GL_API void GL_APIENTRY glFogf(GLenum pname, GLfloat param)
{
    __GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFogf"));

	GLES1_TIME_START(GLES1_TIMES_glFogf);

	/* Accept only enumerants that correspond to single values */
    switch (pname)
    {
		case GL_FOG_DENSITY:
		case GL_FOG_END:
		case GL_FOG_START:
		case GL_FOG_MODE:
		{
			Fogfv(gc, pname, &param);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			return;
		}	
    }

	GLES1_TIME_STOP(GLES1_TIMES_glFogf);
}
#endif /* PROFILE_COMMON */


GL_API void GL_APIENTRY glFogxv(GLenum pname, const GLfixed *params)
{
	IMG_FLOAT afParam[4];
    __GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFogxv"));

	GLES1_TIME_START(GLES1_TIMES_glFogxv);

	switch (pname)
	{
		case GL_FOG_COLOR:
		{
			afParam[0] = FIXED_TO_FLOAT(params[0]);
			afParam[1] = FIXED_TO_FLOAT(params[1]);
			afParam[2] = FIXED_TO_FLOAT(params[2]);
			afParam[3] = FIXED_TO_FLOAT(params[3]);

			break;	
		}
		case GL_FOG_MODE:
		{
			afParam[0] = (IMG_FLOAT)params[0];

			break;
		}
		default:
		{
			afParam[0] = FIXED_TO_FLOAT(params[0]);

			break;
		}
	}	

	Fogfv(gc, pname, afParam);

	GLES1_TIME_STOP(GLES1_TIMES_glFogxv);
}


GL_API void GL_APIENTRY glFogx(GLenum pname, GLfixed param)
{
	IMG_FLOAT fParam;
    __GLES1_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glFogx"));

	GLES1_TIME_START(GLES1_TIMES_glFogx);

	/* Accept only enumerants that correspond to single values */
    switch (pname)
    {
		case GL_FOG_DENSITY:
		case GL_FOG_END:
		case GL_FOG_START:
		{
			fParam = FIXED_TO_FLOAT(param);

			Fogfv(gc, pname, &fParam);

			break;
		}
		case GL_FOG_MODE:
		{
			/* Mode takes enums which are not shifted */
			fParam = (IMG_FLOAT)param;

			Fogfv(gc, pname, &fParam);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			return;
		}	
    }

	GLES1_TIME_STOP(GLES1_TIMES_glFogx);
}



