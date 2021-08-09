/******************************************************************************
 * Name         : tnlapi.c
 *
 * Copyright    : 2004-2006 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of
 *              : Imagination Technologies Limited, Home Park Estate,
 *              : Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: tnlapi.c $
 *********************************************************************************/

#include "context.h"


/***********************************************************************************
 Function Name      : InitLightingState
 Inputs             : gc
 Outputs            : -
 Returns            : Success
 Description        : Initialises internal Lighting state to default values
************************************************************************************/
IMG_INTERNAL IMG_BOOL InitLightingState(GLES1Context *gc)
{
	GLESLightSourceState *psLightState;
	GLfloat DefaultsAmbient[4] = { 0.2F, 0.2F, 0.2F, 1.0F };
	GLfloat DefaultsDiffuse[4] = { 0.8F, 0.8F, 0.8F, 1.0F };
	GLfloat DefaultBlack[4] = { 0.0F, 0.0F, 0.0F, 1.0F };
	GLfloat DefaultWhite[4] = { 1.0F, 1.0F, 1.0F, 1.0F };
	GLuint i;

	if(!gc->sState.sLight.psSource)
	{
		gc->sState.sLight.psSource = (GLESLightSourceState*) GLES1Calloc(gc, GLES1_MAX_LIGHTS * sizeof(GLESLightSourceState));
		
		if(!gc->sState.sLight.psSource)
		{
			return IMG_FALSE;
		}
	}

	if(!gc->sLight.psSource)
	{
		gc->sLight.psSource = (GLESLightSourceMachine*) GLES1Calloc(gc, GLES1_MAX_LIGHTS * sizeof(GLESLightSourceMachine));

		if(!gc->sLight.psSource)
		{
			GLES1Free(IMG_NULL, gc->sState.sLight.psSource);

			return IMG_FALSE;
		}
	}

	gc->sState.sLight.eColorMaterialParam = GL_AMBIENT_AND_DIFFUSE;

	/* GL_LIGHTING_BIT state */
	gc->sState.sLight.sModel.sAmbient.fRed		= DefaultsAmbient[0];
	gc->sState.sLight.sModel.sAmbient.fGreen	= DefaultsAmbient[1];
	gc->sState.sLight.sModel.sAmbient.fBlue		= DefaultsAmbient[2];
	gc->sState.sLight.sModel.sAmbient.fAlpha	= DefaultsAmbient[3];
	gc->sState.sLight.sModel.bTwoSided			= IMG_FALSE;
	gc->sState.sLight.sMaterial.sAmbient.fRed	= DefaultsAmbient[0];
	gc->sState.sLight.sMaterial.sAmbient.fGreen	= DefaultsAmbient[1];
	gc->sState.sLight.sMaterial.sAmbient.fBlue	= DefaultsAmbient[2];
	gc->sState.sLight.sMaterial.sAmbient.fAlpha	= DefaultsAmbient[3];
	gc->sState.sLight.sMaterial.sDiffuse.fRed	= DefaultsDiffuse[0];
	gc->sState.sLight.sMaterial.sDiffuse.fGreen	= DefaultsDiffuse[1];
	gc->sState.sLight.sMaterial.sDiffuse.fBlue	= DefaultsDiffuse[2];
	gc->sState.sLight.sMaterial.sDiffuse.fAlpha	= DefaultsDiffuse[3];
	gc->sState.sLight.sMaterial.sSpecular.fRed	= DefaultBlack[0];
	gc->sState.sLight.sMaterial.sSpecular.fGreen= DefaultBlack[1];
	gc->sState.sLight.sMaterial.sSpecular.fBlue	= DefaultBlack[2];
	gc->sState.sLight.sMaterial.sSpecular.fAlpha= DefaultBlack[3];
	gc->sState.sLight.sMaterial.sEmissive.fRed	= DefaultBlack[0];
	gc->sState.sLight.sMaterial.sEmissive.fGreen= DefaultBlack[1];
	gc->sState.sLight.sMaterial.sEmissive.fBlue	= DefaultBlack[2];
	gc->sState.sLight.sMaterial.sEmissive.fAlpha= DefaultBlack[3];

	/* Initialize the individual lights */
	psLightState = &gc->sState.sLight.psSource[0];

	for (i = 0; i < GLES1_MAX_LIGHTS; i++, psLightState++)
	{
		psLightState->sAmbient.fRed   = DefaultBlack[0];
		psLightState->sAmbient.fGreen = DefaultBlack[1];
		psLightState->sAmbient.fBlue  = DefaultBlack[2];
		psLightState->sAmbient.fAlpha = DefaultBlack[3];

		if (i == 0)
		{
			psLightState->sDiffuse.fRed   = DefaultWhite[0];
			psLightState->sDiffuse.fGreen = DefaultWhite[1];
			psLightState->sDiffuse.fBlue  = DefaultWhite[2];
			psLightState->sDiffuse.fAlpha = DefaultWhite[3];
		}
		else
		{
			psLightState->sDiffuse.fRed   = DefaultBlack[0];
			psLightState->sDiffuse.fGreen = DefaultBlack[1];
			psLightState->sDiffuse.fBlue  = DefaultBlack[2];
			psLightState->sDiffuse.fAlpha = DefaultBlack[3];
		}

		psLightState->sSpecular 			= psLightState->sDiffuse;
		psLightState->sPosition.fZ 			= GLES1_One;
		psLightState->sPositionEye.fZ 		= GLES1_One;
		psLightState->sDirection.fZ 		= GLES1_MinusOne;
		psLightState->sSpotDirectionEye.fZ 	= GLES1_MinusOne;
		psLightState->fSpotLightCutOffAngle = 180;

		psLightState->afAttenuation[GLES1_LIGHT_ATTENUATION_CONSTANT] = GLES1_One;
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : FreeLightingState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Free internal Lighting state
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeLightingState(GLES1Context *gc)
{
	if(gc->sState.sLight.psSource)
	{
		GLES1Free(IMG_NULL, gc->sState.sLight.psSource);

		gc->sState.sLight.psSource = NULL;
	}

	if(gc->sLight.psSource)
	{
		GLES1Free(IMG_NULL, gc->sLight.psSource);

		gc->sLight.psSource = NULL;
	}
}


/***********************************************************************************
 Function Name      : SetupTransformLightingProcs
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Sets up function pointers. Some are fixed with room for adding
					  optimised variants, some are dynamic based on state.
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupTransformLightingProcs(GLES1Context *gc)
{
	gc->sProcs.sMatrixProcs.pfnCopy            = CopyMatrix;
	gc->sProcs.sMatrixProcs.pfnInvertTranspose = InvertTransposeMatrix;
	gc->sProcs.sMatrixProcs.pfnMakeIdentity    = MakeIdentity;
	gc->sProcs.sMatrixProcs.pfnMult			   = MultMatrix;

	gc->sProcs.pfnPushMatrix 				= PushModelViewMatrix;
	gc->sProcs.pfnPopMatrix 				= PopModelViewMatrix;
	gc->sProcs.pfnLoadIdentity 				= LoadIdentityModelViewMatrix;
	gc->sProcs.pfnPickMatrixProcs 			= PickMatrixProcs;
	gc->sProcs.pfnPickInvTransposeProcs 	= PickInvTransposeProcs;
	gc->sProcs.pfnComputeInverseTranspose 	= ComputeInverseTranspose;
	gc->sProcs.pfnNormalize					= Normalize;
}


/***********************************************************************************
 Function Name      : TransformLightDirection
 Inputs             : gc, psLss
 Outputs            : -
 Returns            : -
 Description        : Transforms a light direction by inverse transpose of modelview
					  matrix (like normals).
************************************************************************************/
static IMG_VOID TransformLightDirection(GLES1Context *gc, GLESLightSourceState *psLss)
{
	GLESMatrix *psMatrix = &gc->sTransform.psModelView->sMatrix;
	GLEScoord sDir;

	sDir.fX = psLss->sDirection.fX;
	sDir.fY = psLss->sDirection.fY;
	sDir.fZ = psLss->sDirection.fZ;

	/* This used to be a 4x4 multiply with the inverse transpose MV. This is actually incorrect according to the
	 * spec, which uses the top 3x3 of the MV matrix instead. 
	 */
	psLss->sSpotDirectionEye.fX = sDir.fX*psMatrix->afMatrix[0][0] + sDir.fY*psMatrix->afMatrix[1][0] + sDir.fZ*psMatrix->afMatrix[2][0];
	psLss->sSpotDirectionEye.fY = sDir.fX*psMatrix->afMatrix[0][1] + sDir.fY*psMatrix->afMatrix[1][1] + sDir.fZ*psMatrix->afMatrix[2][1];
	psLss->sSpotDirectionEye.fZ = sDir.fX*psMatrix->afMatrix[0][2] + sDir.fY*psMatrix->afMatrix[1][2] + sDir.fZ*psMatrix->afMatrix[2][2];

	(*gc->sProcs.pfnNormalize)(&psLss->sDirection.fX, &psLss->sSpotDirectionEye.fX);
}


/***********************************************************************************
 Function Name      : glLightModel[f/fv/x/xv]
 Inputs             : pname, param
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets light model parameter specified by pname
					  Stored internally, used to control VGP code+constants/SW TNL.
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glLightModelfv(GLenum pname, const GLfloat *params)
{
	GLESLightModelState *psModel;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLightModelfv"));
	
	GLES1_TIME_START(GLES1_TIMES_glLightModelfv);

	psModel = &gc->sState.sLight.sModel;

	switch (pname) 
	{
		case GL_LIGHT_MODEL_AMBIENT:
		{
			psModel->sAmbient.fRed	 = params[0];
			psModel->sAmbient.fGreen = params[1];
			psModel->sAmbient.fBlue  = params[2];
			psModel->sAmbient.fAlpha = params[3];

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS ;

			break;
		}
		case GL_LIGHT_MODEL_TWO_SIDE:
		{
			IMG_BOOL bTwoSided;

			/* PRQA S 3341 1 */ /* 0.0f can be exactly expressed in binary using IEEE float format */
			if(params[0] != 0)
			{
				bTwoSided = IMG_TRUE;
			}
			else
			{
				bTwoSided = IMG_FALSE;
			}

			if(psModel->bTwoSided!=bTwoSided)
			{
				psModel->bTwoSided = bTwoSided;	

				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_RENDERSTATE;
			}

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glLightModelfv);

			return;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glLightModelfv);
}


GL_API void GL_APIENTRY glLightModelf(GLenum pname, GLfloat param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLightModelf"));

	GLES1_TIME_START(GLES1_TIMES_glLightModelf);

	/* Accept only enumerants that correspond to single values */
	switch (pname)
	{
		case GL_LIGHT_MODEL_TWO_SIDE:
		{
			glLightModelfv(pname, &param);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glLightModelf);

			return;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glLightModelf);
}

#endif


GL_API void GL_APIENTRY glLightModelxv(GLenum pname, const GLfixed *params)
{
	GLESLightModelState *psModel;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLightModelxv"));

	GLES1_TIME_START(GLES1_TIMES_glLightModelxv);

	psModel = &gc->sState.sLight.sModel;

	switch (pname) 
	{
		case GL_LIGHT_MODEL_AMBIENT:
		{
			psModel->sAmbient.fRed	 = FIXED_TO_FLOAT(params[0]);
			psModel->sAmbient.fGreen = FIXED_TO_FLOAT(params[1]);
			psModel->sAmbient.fBlue	 = FIXED_TO_FLOAT(params[2]);
			psModel->sAmbient.fAlpha = FIXED_TO_FLOAT(params[3]);

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
		case GL_LIGHT_MODEL_TWO_SIDE:
		{
			IMG_BOOL bTwoSided;

			if(params[0] != 0)
			{
				bTwoSided = IMG_TRUE;
			}
			else
			{
				bTwoSided = IMG_FALSE;
			}

			if(psModel->bTwoSided!=bTwoSided)
			{
				psModel->bTwoSided = bTwoSided;	

				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_RENDERSTATE;
			}

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glLightModelxv);

			return;
		}
	}


	GLES1_TIME_STOP(GLES1_TIMES_glLightModelxv);
}

GL_API void GL_APIENTRY glLightModelx(GLenum pname, GLfixed param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLightModelx"));

	GLES1_TIME_START(GLES1_TIMES_glLightModelx);

	/* Accept only enumerants that correspond to single values */
	switch (pname) 
	{
		case GL_LIGHT_MODEL_TWO_SIDE:
		{
			glLightModelxv(pname, &param);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glLightModelx);

			return;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glLightModelx);
}

/***********************************************************************************
 Function Name      : Lightfv
 Inputs             : gc, eLight, ePname, pfParams
 Outputs            : -
 Returns            : -
 Description        : UTILTIY: Sets light parameter specified by pname. 
					  Stored internally, used to control VGP code+constants/SW TNL.
************************************************************************************/

static IMG_VOID Lightfv(GLES1Context *gc, GLenum eLight, GLenum ePname, const IMG_FLOAT *pfParams)
{
	GLESLightSourceState *psLss;
	GLESMatrix *psMatrix;

	eLight -= GL_LIGHT0;

	if (eLight >= GLES1_MAX_LIGHTS)
	{
bad_enum:
		SetError(gc, GL_INVALID_ENUM);

		return;
	}

	psLss = &gc->sState.sLight.psSource[eLight];

	switch (ePname) 
	{
		case GL_AMBIENT:
		{
			psLss->sAmbient.fRed = pfParams[0];
			psLss->sAmbient.fGreen = pfParams[1];
			psLss->sAmbient.fBlue = pfParams[2];
			psLss->sAmbient.fAlpha = pfParams[3];
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS ;

			break;
		}
		case GL_DIFFUSE:
		{
			psLss->sDiffuse.fRed = pfParams[0];
			psLss->sDiffuse.fGreen = pfParams[1];
			psLss->sDiffuse.fBlue = pfParams[2];
			psLss->sDiffuse.fAlpha = pfParams[3];
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
		case GL_SPECULAR:
		{
			psLss->sSpecular.fRed = pfParams[0];
			psLss->sSpecular.fGreen = pfParams[1];
			psLss->sSpecular.fBlue = pfParams[2];
			psLss->sSpecular.fAlpha = pfParams[3];
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS | GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			break;
		}
		case GL_POSITION:
		{
			psLss->sPosition.fX = pfParams[0];
			psLss->sPosition.fY = pfParams[1];
			psLss->sPosition.fZ = pfParams[2];
			psLss->sPosition.fW = pfParams[3];
			
			/*
			** Transform light position into eye space
			*/
			psMatrix = &gc->sTransform.psModelView->sMatrix;

			(*psMatrix->pfnXf4)(&psLss->sPositionEye, &psLss->sPosition.fX, psMatrix);
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS | GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			break;
		}
		case GL_SPOT_DIRECTION:
		{
			psLss->sDirection.fX = pfParams[0];
			psLss->sDirection.fY = pfParams[1];
			psLss->sDirection.fZ = pfParams[2];
			psLss->sDirection.fW = 1;
			TransformLightDirection(gc, psLss);
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS ;

			break;
		}
		case GL_SPOT_EXPONENT:
		{
			if ((pfParams[0] < 0) || (pfParams[0] > 128))
			{
bad_value:
				SetError(gc, GL_INVALID_VALUE);

				return;
			}

			psLss->fSpotLightExponent = pfParams[0];
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS ;

			break;
		}
		case GL_SPOT_CUTOFF:
		{
			/* PRQA S 3341 1 */ /* 180.0f, 0.0f and 90.0f can be exactly expressed in binary using IEEE float format */
			if ((pfParams[0] != 180) && ((pfParams[0] < 0) || (pfParams[0] > 90)))
			{
				goto bad_value;
			}
			
			psLss->fSpotLightCutOffAngle = pfParams[0];
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS | GLES1_DIRTYFLAG_VERTEX_PROGRAM;

			break;
		}
		case GL_CONSTANT_ATTENUATION:
		{
			if (pfParams[0] < GLES1_Zero)
			{
				goto bad_value;
			}

			psLss->afAttenuation[GLES1_LIGHT_ATTENUATION_CONSTANT] = pfParams[0];
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS ;

			break;
		}
		case GL_LINEAR_ATTENUATION:
		{
			if (pfParams[0] < GLES1_Zero)
			{
				goto bad_value;
			}

			psLss->afAttenuation[GLES1_LIGHT_ATTENUATION_LINEAR] = pfParams[0];
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS ;

			break;
		}
		case GL_QUADRATIC_ATTENUATION:
		{
			if (pfParams[0] < GLES1_Zero)
			{
				goto bad_value;
			}

			psLss->afAttenuation[GLES1_LIGHT_ATTENUATION_QUADRATIC] = pfParams[0];
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS ;

			break;
		}
		default:
		{
			goto bad_enum;
		}
	}
}


/***********************************************************************************
 Function Name      : glLight[f/fv/x/xv]
 Inputs             : light, pname, param
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets light parameter specified by pname. 
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glLightf(GLenum light, GLenum pname, GLfloat param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLightf"));

	GLES1_TIME_START(GLES1_TIMES_glLightf);

	/* Accept only enumerants that correspond to single values */
	switch (pname)
	{
		case GL_SPOT_EXPONENT:
		case GL_SPOT_CUTOFF:
		case GL_CONSTANT_ATTENUATION:
		case GL_LINEAR_ATTENUATION:
		case GL_QUADRATIC_ATTENUATION:
		{
			Lightfv(gc, light, pname, &param);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glLightf);
}


GL_API void GL_APIENTRY glLightfv(GLenum light, GLenum pname, const GLfloat *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLightfv"));

	GLES1_TIME_START(GLES1_TIMES_glLightfv);

	Lightfv(gc, light, pname, params);

	GLES1_TIME_STOP(GLES1_TIMES_glLightfv);
}

#endif

GL_API void GL_APIENTRY glLightx(GLenum light, GLenum pname, GLfixed param)
{
	GLfloat p = FIXED_TO_FLOAT(param);

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLightx"));

	GLES1_TIME_START(GLES1_TIMES_glLightx);

	/* Accept only enumerants that correspond to single values */
	switch (pname)
	{
		case GL_SPOT_EXPONENT:
		case GL_SPOT_CUTOFF:
		case GL_CONSTANT_ATTENUATION:
		case GL_LINEAR_ATTENUATION:
		case GL_QUADRATIC_ATTENUATION:
		{
			Lightfv(gc, light, pname, &p);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glLightx);
}

GL_API void GL_APIENTRY glLightxv(GLenum light, GLenum pname, const GLfixed *params)
{
	GLfloat p[4];
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLightxv"));
	
	GLES1_TIME_START(GLES1_TIMES_glLightxv);

	p[0] = FIXED_TO_FLOAT(params[0]);

	switch (pname) 
	{
		case GL_AMBIENT:
		case GL_DIFFUSE:
		case GL_SPECULAR:
		case GL_POSITION:
		case GL_SPOT_DIRECTION:
		{
			p[1] = FIXED_TO_FLOAT(params[1]);
			p[2] = FIXED_TO_FLOAT(params[2]);
			p[3] = FIXED_TO_FLOAT(params[3]);

			break;
		}
	}

	Lightfv(gc, light, pname, p);

	GLES1_TIME_STOP(GLES1_TIMES_glLightxv);
}


/***********************************************************************************
 Function Name      : Materialfv
 Inputs             : gc, ui32Face, ePname, pfParams
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets material parameter specified by pname. 
					  Stored internally, used to control VGP code+constants/SW TNL.
************************************************************************************/
IMG_INTERNAL IMG_VOID Materialfv(GLES1Context *gc, GLenum ui32Face, GLenum ePname, const IMG_FLOAT *pfParams)
{
	if(ui32Face != GL_FRONT_AND_BACK)
	{
bad_enum:
		SetError(gc, GL_INVALID_ENUM);

		return;
	}

	switch (ePname) 
	{
		case GL_EMISSION:
		{
			gc->sState.sLight.sMaterial.sEmissive.fRed = pfParams[0];
			gc->sState.sLight.sMaterial.sEmissive.fGreen = pfParams[1];
			gc->sState.sLight.sMaterial.sEmissive.fBlue = pfParams[2];
			gc->sState.sLight.sMaterial.sEmissive.fAlpha = pfParams[3];

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS ;

			break;
		}
		case GL_SPECULAR:
		{
			gc->sState.sLight.sMaterial.sSpecular.fRed = pfParams[0];
			gc->sState.sLight.sMaterial.sSpecular.fGreen = pfParams[1];
			gc->sState.sLight.sMaterial.sSpecular.fBlue = pfParams[2];
			gc->sState.sLight.sMaterial.sSpecular.fAlpha = pfParams[3];

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS | GLES1_DIRTYFLAG_VERTEX_PROGRAM;
			
			break;
		}
		case GL_AMBIENT:
		{
			/* See Corollory 5 of Appendix B of GLES1.1 full spec */
			if((gc->ui32TnLEnables & GLES1_TL_COLORMAT_ENABLE) == 0)
			{
				gc->sState.sLight.sMaterial.sAmbient.fRed = pfParams[0];
				gc->sState.sLight.sMaterial.sAmbient.fGreen = pfParams[1];
				gc->sState.sLight.sMaterial.sAmbient.fBlue = pfParams[2];
				gc->sState.sLight.sMaterial.sAmbient.fAlpha = pfParams[3];

				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
			}

			break;
		}
		case GL_DIFFUSE:
		{
			/* See Corollory 5 of Appendix B of GLES1.1 full spec */
			if((gc->ui32TnLEnables & GLES1_TL_COLORMAT_ENABLE) == 0)
			{
				gc->sState.sLight.sMaterial.sDiffuse.fRed = pfParams[0];
				gc->sState.sLight.sMaterial.sDiffuse.fGreen = pfParams[1];
				gc->sState.sLight.sMaterial.sDiffuse.fBlue = pfParams[2];
				gc->sState.sLight.sMaterial.sDiffuse.fAlpha = pfParams[3];

				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
			}
			break;
		}
		case GL_AMBIENT_AND_DIFFUSE:
		{
			/* See Corollory 5 of Appendix B of GLES1.1 full spec */
			if((gc->ui32TnLEnables & GLES1_TL_COLORMAT_ENABLE) == 0)
			{
				gc->sState.sLight.sMaterial.sAmbient.fRed = pfParams[0];
				gc->sState.sLight.sMaterial.sAmbient.fGreen = pfParams[1];
				gc->sState.sLight.sMaterial.sAmbient.fBlue = pfParams[2];
				gc->sState.sLight.sMaterial.sAmbient.fAlpha = pfParams[3];
				
				gc->sState.sLight.sMaterial.sDiffuse = gc->sState.sLight.sMaterial.sAmbient;

				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
			}

			break;
		}
		case GL_SHININESS:
		{
			if (pfParams[0] < 0 || pfParams[0] > 128)
			{
				SetError(gc, GL_INVALID_VALUE);

				return;
			}

			gc->sState.sLight.sMaterial.sSpecularExponent.fX = pfParams[0];

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS ;
			
			break;
		}
		default:
		{
			goto bad_enum;
		}
	}

}


/***********************************************************************************
 Function Name      : glMaterial[f/fv/x/xv]
 Inputs             : ui32Face, pname, param
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets material parameter specified by pname. 
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glMaterialf(GLenum ui32Face, GLenum pname, GLfloat param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glMaterialf"));

	GLES1_TIME_START(GLES1_TIMES_glMaterialf);

	/* Accept only enumerants that correspond to single values */
	switch (pname) 
	{
		case GL_SHININESS:
		{
			Materialfv(gc, ui32Face, pname, &param);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glMaterialf);
}

GL_API void GL_APIENTRY glMaterialfv(GLenum ui32Face, GLenum pname, const GLfloat *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glMaterialfv"));

	GLES1_TIME_START(GLES1_TIMES_glMaterialfv);

	Materialfv(gc, ui32Face, pname, params);

	GLES1_TIME_STOP(GLES1_TIMES_glMaterialfv);
}

#endif

GL_API void GL_APIENTRY glMaterialx(GLenum ui32Face, GLenum pname, GLfixed param)
{
	IMG_FLOAT fParam = FIXED_TO_FLOAT(param);

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glMaterialx"));

	GLES1_TIME_START(GLES1_TIMES_glMaterialx);

	/* Accept only enumerants that correspond to single values */
	switch (pname) 
	{
		case GL_SHININESS:
		{
			Materialfv(gc, ui32Face, pname, &fParam);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glMaterialx);
}

GL_API void GL_APIENTRY glMaterialxv(GLenum ui32Face, GLenum pname, const GLfixed *params)
{
	IMG_FLOAT afParams[4];
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glMaterialxv"));

	GLES1_TIME_START(GLES1_TIMES_glMaterialxv);

	afParams[0] = FIXED_TO_FLOAT(params[0]);

	switch (pname) 
	{
		case GL_EMISSION:
		case GL_SPECULAR:
		case GL_AMBIENT:
		case GL_DIFFUSE:
		case GL_AMBIENT_AND_DIFFUSE:
		{
			afParams[1] = FIXED_TO_FLOAT(params[1]);
			afParams[2] = FIXED_TO_FLOAT(params[2]);
			afParams[3] = FIXED_TO_FLOAT(params[3]);

			break;
		}
	}

	Materialfv(gc, ui32Face, pname, afParams);

	GLES1_TIME_STOP(GLES1_TIMES_glMaterialxv);
}


/***********************************************************************************
 Function Name      : ApplyViewport
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets current viewport transform
************************************************************************************/
IMG_INTERNAL IMG_VOID ApplyViewport(GLES1Context *gc)
{
	IMG_FLOAT fWW, fHH;
	GLESViewport *psViewport = &gc->sState.sViewport;

	/* Compute operational viewport values */
	fWW = (IMG_FLOAT)psViewport->ui32Width * GLES1_Half;
	fHH = (IMG_FLOAT)psViewport->ui32Height * GLES1_Half;

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

	gc->ui32EmitMask |= GLES1_EMITSTATE_MTE_STATE_VIEWPORT;
}


/***********************************************************************************
 Function Name      : glViewport
 Inputs             : x, y, width, height
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current viewport state.
************************************************************************************/
GL_API void GL_APIENTRY glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
	GLESViewport *psViewport;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glViewport"));

	GLES1_TIME_START(GLES1_TIMES_glViewport);

	psViewport = &gc->sState.sViewport;

	if ((width < 0) || (height < 0))
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glViewport);

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
		GLES1_TIME_STOP(GLES1_TIMES_glViewport);

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

	GLES1_TIME_STOP(GLES1_TIMES_glViewport);
}


/***********************************************************************************
 Function Name      : ApplyDepthRange
 Inputs             : gc, fZNear, fZFar
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets current depth range state.
					  Sets VGP viewport/SW TNL state.
************************************************************************************/
IMG_INTERNAL IMG_VOID ApplyDepthRange(GLES1Context *gc, IMG_FLOAT fZNear, IMG_FLOAT fZFar)
{
	GLESViewport *psViewport = &gc->sState.sViewport;

	fZNear = Clampf(fZNear, GLES1_Zero, GLES1_One);
	fZFar  = Clampf(fZFar, GLES1_Zero, GLES1_One);

	if ( ((psViewport->fZNear < fZNear) || (psViewport->fZNear > fZNear)) ||
		 ((psViewport->fZFar < fZFar) || (psViewport->fZFar > fZFar)) )
	{
		psViewport->fZNear = fZNear;
		psViewport->fZFar  = fZFar;

		/* Compute viewport values for the new depth range */
		gc->sState.sViewport.fZScale  = (psViewport->fZFar - psViewport->fZNear) * GLES1_Half;
		gc->sState.sViewport.fZCenter = (psViewport->fZFar + psViewport->fZNear) * GLES1_Half;

		gc->ui32EmitMask |= GLES1_EMITSTATE_MTE_STATE_VIEWPORT;
	}
}


/***********************************************************************************
 Function Name      : glDepthRange[f/x]
 Inputs             : fZNear, fZFar
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current depth range state.
					  Sets VGP viewport/SW TNL state.
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glDepthRangef(GLclampf fZNear, GLclampf fZFar)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDepthRangef"));

	GLES1_TIME_START(GLES1_TIMES_glDepthRangef);

	ApplyDepthRange(gc, fZNear, fZFar);

	GLES1_TIME_STOP(GLES1_TIMES_glDepthRangef);
}
#endif

GL_API void GL_APIENTRY glDepthRangex(GLclampx fZNear, GLclampx fZFar)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDepthRangex"));

	GLES1_TIME_START(GLES1_TIMES_glDepthRangex);

	ApplyDepthRange(gc, FIXED_TO_FLOAT(fZNear), FIXED_TO_FLOAT(fZFar));

	GLES1_TIME_STOP(GLES1_TIMES_glDepthRangex);
}


/***********************************************************************************
 Function Name      : glMatrixMode
 Inputs             : mode
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current matrix mode. 
					  Picks appropriate function pointers for push/pop/loadidentity.
************************************************************************************/

GL_API void GL_APIENTRY glMatrixMode(GLenum mode)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glMatrixMode"));

	GLES1_TIME_START(GLES1_TIMES_glMatrixMode);

	if(mode != gc->sState.eMatrixMode)
	{
		switch (mode) 
		{
			case GL_MODELVIEW:
			{
				gc->sProcs.pfnPushMatrix = PushModelViewMatrix;
				gc->sProcs.pfnPopMatrix = PopModelViewMatrix;
				gc->sProcs.pfnLoadIdentity = LoadIdentityModelViewMatrix;

				break;
			}
			case GL_PROJECTION:
			{
				gc->sProcs.pfnPushMatrix = PushProjectionMatrix;
				gc->sProcs.pfnPopMatrix = PopProjectionMatrix;
				gc->sProcs.pfnLoadIdentity = LoadIdentityProjectionMatrix;

				break;
			}
			case GL_TEXTURE:
			{
				gc->sProcs.pfnPushMatrix = PushTextureMatrix;
				gc->sProcs.pfnPopMatrix = PopTextureMatrix;
				gc->sProcs.pfnLoadIdentity = LoadIdentityTextureMatrix;

				break;
			}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
			case GL_MATRIX_PALETTE_OES:
			{
				gc->sProcs.pfnPushMatrix	= PushMatrixPaletteMatrix;
				gc->sProcs.pfnPopMatrix		= PopMatrixPaletteMatrix;
				gc->sProcs.pfnLoadIdentity	= LoadIdentityMatrixPaletteMatrix;

				break;
			}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
			default:
			{
				SetError(gc, GL_INVALID_ENUM);

				return;
			}
		}

		gc->sState.eMatrixMode = mode;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glMatrixMode);
}


/***********************************************************************************
 Function Name      : Frustumf
 Inputs             : gc, fLeft, fRight, fBottom, fTop, fZNear, fZFar
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Multiplies the current matrix by a perspective matrix.
************************************************************************************/
static IMG_VOID Frustumf(GLES1Context *gc, IMG_FLOAT fLeft, IMG_FLOAT fRight, IMG_FLOAT fBottom, 
						IMG_FLOAT fTop, IMG_FLOAT fZNear, IMG_FLOAT fZFar)
{
	IMG_FLOAT fDeltax, fDeltay, fDeltaz;
	GLESMatrix sMatrix;

	fDeltax = fRight - fLeft;
	fDeltay = fTop - fBottom;
	fDeltaz = fZFar - fZNear;

	/* GLES1_Zero can be exactly expressed in binary using IEEE float format */
	/* PRQA S 3341 ++ */
	if ((fZNear <= GLES1_Zero) ||
		(fZFar  <= GLES1_Zero) ||
		(fDeltax == GLES1_Zero) || 
		(fDeltay == GLES1_Zero) ||
		(fDeltaz == GLES1_Zero)) 
	{
		SetError(gc, GL_INVALID_VALUE);

		return;
	}
	/* PRQA S 3341 -- */

	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&sMatrix);

	sMatrix.afMatrix[0][0] = fZNear * 2.0f / fDeltax;
	sMatrix.afMatrix[1][1] = fZNear * 2.0f / fDeltay;
	sMatrix.afMatrix[2][0] = (fRight + fLeft) / fDeltax;
	sMatrix.afMatrix[2][1] = (fTop + fBottom) / fDeltay;
	sMatrix.afMatrix[2][2] = -(fZFar + fZNear) / fDeltaz;
	sMatrix.afMatrix[2][3] = -1.0f;
	sMatrix.afMatrix[3][2] =  -2.0f * fZNear * fZFar / fDeltaz;
	sMatrix.afMatrix[3][3] = GLES1_Zero;

	sMatrix.eMatrixType = GLES1_MT_GENERAL;

	DoMultMatrix(gc, &sMatrix, MultiplyMatrix);
}


/***********************************************************************************
 Function Name      : glFrustum[f/x]
 Inputs             : left, right, bottom, top, fZNear, fZFar
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Multiplies the current matrix by a perspective
					  matrix.
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat fZNear, GLfloat fZFar)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFrustumf"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS); 
	GLES1_TIME_START(GLES1_TIMES_glFrustumf);
	
	Frustumf(gc, left, right, bottom, top, fZNear, fZFar);

	GLES1_TIME_STOP(GLES1_TIMES_glFrustumf);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}
#endif

GL_API void GL_APIENTRY glFrustumx(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, 
						 GLfixed fZNear, GLfixed fZFar)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glFrustumx"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glFrustumx);

	Frustumf(gc, FIXED_TO_FLOAT(left), FIXED_TO_FLOAT(right), FIXED_TO_FLOAT(bottom),
				  FIXED_TO_FLOAT(top), FIXED_TO_FLOAT(fZNear), FIXED_TO_FLOAT(fZFar));

	GLES1_TIME_STOP(GLES1_TIMES_glFrustumx);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}

/***********************************************************************************
 Function Name      : Orthof
 Inputs             : gc, fLeft, fRight, fBottom, fTop, fZNear, fZFar
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Multiplies the current matrix by an orthographic 
					  matrix.
************************************************************************************/
static IMG_VOID Orthof(GLES1Context *gc, IMG_FLOAT fLeft, IMG_FLOAT fRight, IMG_FLOAT fBottom, 
					   IMG_FLOAT fTop, IMG_FLOAT fZNear, IMG_FLOAT fZFar)
{
	IMG_FLOAT fDeltax, fDeltay, fDeltaz;
	GLESMatrix sMatrix;

	fDeltax = fRight - fLeft;
	fDeltay = fTop - fBottom;
	fDeltaz = fZFar - fZNear;

	/* PRQA S 3341 1 */ /* GLES1_Zero can be exactly expressed in binary using IEEE float format */
	if ((fDeltax == GLES1_Zero) || (fDeltay == GLES1_Zero) || (fDeltaz == GLES1_Zero)) 
	{
		SetError(gc, GL_INVALID_VALUE);

		return;
	}

	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&sMatrix);

	sMatrix.afMatrix[0][0] = 2.0f / fDeltax;
	sMatrix.afMatrix[3][0] = -(fRight + fLeft) / fDeltax;
	sMatrix.afMatrix[1][1] = 2.0f / fDeltay;
	sMatrix.afMatrix[3][1] = -(fTop + fBottom) / fDeltay;
	sMatrix.afMatrix[2][2] = -2.0f / fDeltaz;
	sMatrix.afMatrix[3][2] = -(fZFar + fZNear) / fDeltaz;

	/* PRQA S 3341 ++ */ /* GLES1_Zero can be exactly expressed in binary using IEEE float format */
	if (fLeft == GLES1_Zero && fBottom == GLES1_Zero && 
		(fRight - (IMG_FLOAT)gc->sState.sViewport.ui32Width == GLES1_Zero) &&
	    (fTop - (IMG_FLOAT)gc->sState.sViewport.ui32Height == GLES1_Zero) &&
	    fZNear <= GLES1_Zero && 
	    fZFar >= GLES1_Zero) 
	{
		sMatrix.eMatrixType = GLES1_MT_IS2DNRSC;
		sMatrix.ui32Width = gc->sState.sViewport.ui32Width;
		sMatrix.ui32Height = gc->sState.sViewport.ui32Height;
	}
	else
	{
		sMatrix.eMatrixType = GLES1_MT_IS2DNR;
	}
	/* PRQA S 3341 -- */

	DoMultMatrix(gc, &sMatrix, MultiplyMatrix);
}


/***********************************************************************************
 Function Name      : glOrtho[f/x]
 Inputs             : left, right, bottom, top, fZNear, fZFar
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Multiplies the current matrix by an orthographic
					  matrix.
************************************************************************************/

#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, 
					   GLfloat fZNear, GLfloat fZFar)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glOrthof"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glOrthof);

	Orthof(gc, left, right, bottom, top, fZNear, fZFar);

	GLES1_TIME_STOP(GLES1_TIMES_glOrthof);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}
#endif

GL_API void GL_APIENTRY glOrthox(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, 
					   GLfixed fZNear, GLfixed fZFar)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glOrthox"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glOrthox);

	Orthof(gc, FIXED_TO_FLOAT(left), FIXED_TO_FLOAT(right), FIXED_TO_FLOAT(bottom),
			   FIXED_TO_FLOAT(top), FIXED_TO_FLOAT(fZNear), FIXED_TO_FLOAT(fZFar));

	GLES1_TIME_STOP(GLES1_TIMES_glOrthox);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}



/***********************************************************************************
 Function Name      : glLoadIdentity
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets the current matrix to the identity
************************************************************************************/
GL_API void GL_APIENTRY glLoadIdentity(IMG_VOID)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLoadIdentity"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glLoadIdentity);

	(*gc->sProcs.pfnLoadIdentity)(gc);

	GLES1_TIME_STOP(GLES1_TIMES_glLoadIdentity);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}


/***********************************************************************************
 Function Name      : glLoadMatrix[f/x]
 Inputs             : m
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets the current matrix to the incoming data.
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glLoadMatrixf(const GLfloat *m)
{
	GLES1Transform *psTransform;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLoadMatrixf"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glLoadMatrixf);

	switch (gc->sState.eMatrixMode) 
	{
		case GL_MODELVIEW:
		default:
		{
			psTransform = gc->sTransform.psModelView;

			break;
		}
		case GL_PROJECTION:
		{
			psTransform = gc->sTransform.psProjection;

			break;
		}
		case GL_TEXTURE:
		{
			psTransform = gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture];

			break;
		}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_PALETTE_OES:
		{
			psTransform = &gc->sTransform.psMatrixPalette[gc->sState.sCurrent.ui32MatrixPaletteIndex];

			break;
		}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
	}

	psTransform->sMatrix.afMatrix[0][0] = m[0];
	psTransform->sMatrix.afMatrix[0][1] = m[1];
	psTransform->sMatrix.afMatrix[0][2] = m[2];
	psTransform->sMatrix.afMatrix[0][3] = m[3];
	psTransform->sMatrix.afMatrix[1][0] = m[4];
	psTransform->sMatrix.afMatrix[1][1] = m[5];
	psTransform->sMatrix.afMatrix[1][2] = m[6];
	psTransform->sMatrix.afMatrix[1][3] = m[7];
	psTransform->sMatrix.afMatrix[2][0] = m[8];
	psTransform->sMatrix.afMatrix[2][1] = m[9];
	psTransform->sMatrix.afMatrix[2][2] = m[10];
	psTransform->sMatrix.afMatrix[2][3] = m[11];
	psTransform->sMatrix.afMatrix[3][0] = m[12];
	psTransform->sMatrix.afMatrix[3][1] = m[13];
	psTransform->sMatrix.afMatrix[3][2] = m[14];
	psTransform->sMatrix.afMatrix[3][3] = m[15];
	psTransform->sMatrix.eMatrixType = GLES1_MT_GENERAL;

	DoLoadMatrix(gc, &psTransform->sMatrix);

	GLES1_TIME_STOP(GLES1_TIMES_glLoadMatrixf);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}
#endif

GL_API void GL_APIENTRY glLoadMatrixx(const GLfixed *m)
{
	GLES1Transform *psTransform;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLoadMatrixx"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glLoadMatrixx);

	switch (gc->sState.eMatrixMode) 
	{
		case GL_MODELVIEW:
		default:
		{
			psTransform = gc->sTransform.psModelView;

			break;
		}
		case GL_PROJECTION:
		{
			psTransform = gc->sTransform.psProjection;

			break;
		}
		case GL_TEXTURE:
		{
			psTransform = gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture];

			break;
		}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_PALETTE_OES:
		{
			psTransform = &gc->sTransform.psMatrixPalette[gc->sState.sCurrent.ui32MatrixPaletteIndex];

			break;
		}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
	}

	psTransform->sMatrix.afMatrix[0][0] = FIXED_TO_FLOAT(m[0]);
	psTransform->sMatrix.afMatrix[0][1] = FIXED_TO_FLOAT(m[1]);
	psTransform->sMatrix.afMatrix[0][2] = FIXED_TO_FLOAT(m[2]);
	psTransform->sMatrix.afMatrix[0][3] = FIXED_TO_FLOAT(m[3]);
	psTransform->sMatrix.afMatrix[1][0] = FIXED_TO_FLOAT(m[4]);
	psTransform->sMatrix.afMatrix[1][1] = FIXED_TO_FLOAT(m[5]);
	psTransform->sMatrix.afMatrix[1][2] = FIXED_TO_FLOAT(m[6]);
	psTransform->sMatrix.afMatrix[1][3] = FIXED_TO_FLOAT(m[7]);
	psTransform->sMatrix.afMatrix[2][0] = FIXED_TO_FLOAT(m[8]);
	psTransform->sMatrix.afMatrix[2][1] = FIXED_TO_FLOAT(m[9]);
	psTransform->sMatrix.afMatrix[2][2] = FIXED_TO_FLOAT(m[10]);
	psTransform->sMatrix.afMatrix[2][3] = FIXED_TO_FLOAT(m[11]);
	psTransform->sMatrix.afMatrix[3][0] = FIXED_TO_FLOAT(m[12]);
	psTransform->sMatrix.afMatrix[3][1] = FIXED_TO_FLOAT(m[13]);
	psTransform->sMatrix.afMatrix[3][2] = FIXED_TO_FLOAT(m[14]);
	psTransform->sMatrix.afMatrix[3][3] = FIXED_TO_FLOAT(m[15]);
	psTransform->sMatrix.eMatrixType = GLES1_MT_GENERAL;

	DoLoadMatrix(gc, &psTransform->sMatrix);

	GLES1_TIME_STOP(GLES1_TIMES_glLoadMatrixx);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}


/***********************************************************************************
 Function Name      : glMultMatrix[f/x]
 Inputs             : m
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Multiplies the current matrix by the incoming data.
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glMultMatrixf(const GLfloat *m)
{
	GLESMatrix sMatrix;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glMultMatrixf"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glMultMatrixf);

	sMatrix.afMatrix[0][0] = m[0];
	sMatrix.afMatrix[0][1] = m[1];
	sMatrix.afMatrix[0][2] = m[2];
	sMatrix.afMatrix[0][3] = m[3];
	sMatrix.afMatrix[1][0] = m[4];
	sMatrix.afMatrix[1][1] = m[5];
	sMatrix.afMatrix[1][2] = m[6];
	sMatrix.afMatrix[1][3] = m[7];
	sMatrix.afMatrix[2][0] = m[8];
	sMatrix.afMatrix[2][1] = m[9];
	sMatrix.afMatrix[2][2] = m[10];
	sMatrix.afMatrix[2][3] = m[11];
	sMatrix.afMatrix[3][0] = m[12];
	sMatrix.afMatrix[3][1] = m[13];
	sMatrix.afMatrix[3][2] = m[14];
	sMatrix.afMatrix[3][3] = m[15];
	sMatrix.eMatrixType = GLES1_MT_GENERAL;

	DoMultMatrix(gc, &sMatrix, MultiplyMatrix);

	GLES1_TIME_STOP(GLES1_TIMES_glMultMatrixf);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}
#endif

GL_API void GL_APIENTRY glMultMatrixx(const GLfixed *m)
{
	GLESMatrix sMatrix;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glMultMatrixx"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glMultMatrixx);

	sMatrix.afMatrix[0][0] = FIXED_TO_FLOAT(m[0]);
	sMatrix.afMatrix[0][1] = FIXED_TO_FLOAT(m[1]);
	sMatrix.afMatrix[0][2] = FIXED_TO_FLOAT(m[2]);
	sMatrix.afMatrix[0][3] = FIXED_TO_FLOAT(m[3]);
	sMatrix.afMatrix[1][0] = FIXED_TO_FLOAT(m[4]);
	sMatrix.afMatrix[1][1] = FIXED_TO_FLOAT(m[5]);
	sMatrix.afMatrix[1][2] = FIXED_TO_FLOAT(m[6]);
	sMatrix.afMatrix[1][3] = FIXED_TO_FLOAT(m[7]);
	sMatrix.afMatrix[2][0] = FIXED_TO_FLOAT(m[8]);
	sMatrix.afMatrix[2][1] = FIXED_TO_FLOAT(m[9]);
	sMatrix.afMatrix[2][2] = FIXED_TO_FLOAT(m[10]);
	sMatrix.afMatrix[2][3] = FIXED_TO_FLOAT(m[11]);
	sMatrix.afMatrix[3][0] = FIXED_TO_FLOAT(m[12]);
	sMatrix.afMatrix[3][1] = FIXED_TO_FLOAT(m[13]);
	sMatrix.afMatrix[3][2] = FIXED_TO_FLOAT(m[14]);
	sMatrix.afMatrix[3][3] = FIXED_TO_FLOAT(m[15]);
	sMatrix.eMatrixType = GLES1_MT_GENERAL;

	DoMultMatrix(gc, &sMatrix, MultiplyMatrix);

	GLES1_TIME_STOP(GLES1_TIMES_glMultMatrixx);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}


/***********************************************************************************
 Function Name      : Rotatef
 Inputs             : gc, fAngle, fX, fY, fZ
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Multiplies the current matrix by a rotation matrix
************************************************************************************/
static IMG_VOID Rotatef(GLES1Context *gc, IMG_FLOAT fAngle, IMG_FLOAT fX, IMG_FLOAT fY, IMG_FLOAT fZ)
{
	GLESMatrix sMatrix;
	IMG_FLOAT fRadians, fSine, fCosine, fAB, fBC, fCA, fT;
	IMG_FLOAT afAv[4], afAxis[4];

	afAv[0] = fX;
	afAv[1] = fY;
	afAv[2] = fZ;
	afAv[3] = 0;

	(*gc->sProcs.pfnNormalize)(afAxis, afAv);

	fRadians = fAngle * GLES1_DegreesToRadians;
	fSine = GLES1_SINF(fRadians);
	fCosine = GLES1_COSF(fRadians);

	fAB = afAxis[0] * afAxis[1] * (1 - fCosine);
	fBC = afAxis[1] * afAxis[2] * (1 - fCosine);
	fCA = afAxis[2] * afAxis[0] * (1 - fCosine);

	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&sMatrix);

	fT = afAxis[0] * afAxis[0];
	sMatrix.afMatrix[0][0] = fT + fCosine * (1 - fT);
	sMatrix.afMatrix[2][1] = fBC - afAxis[0] * fSine;
	sMatrix.afMatrix[1][2] = fBC + afAxis[0] * fSine;

	fT = afAxis[1] * afAxis[1];
	sMatrix.afMatrix[1][1] = fT + fCosine * (1 - fT);
	sMatrix.afMatrix[2][0] = fCA + afAxis[1] * fSine;
	sMatrix.afMatrix[0][2] = fCA - afAxis[1] * fSine;

	fT = afAxis[2] * afAxis[2];
	sMatrix.afMatrix[2][2] = fT + fCosine * (1 - fT);
	sMatrix.afMatrix[1][0] = fAB - afAxis[2] * fSine;
	sMatrix.afMatrix[0][1] = fAB + afAxis[2] * fSine;

	/* PRQA S 3341 1 */ /* GLES1_Zero can be exactly expressed in binary using IEEE float format */
	if (fX == GLES1_Zero && fY == GLES1_Zero) 
	{
		sMatrix.eMatrixType = GLES1_MT_IS2D;
	}
	else
	{
		sMatrix.eMatrixType = GLES1_MT_W0001;
	}

	DoMultMatrix(gc, &sMatrix, MultiplyMatrix);
}


/***********************************************************************************
 Function Name      : glRotate[f/x]
 Inputs             : fAngle, x, y, z
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Multiplies the current matrix by a rotation matrix
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glRotatef(GLfloat fAngle, GLfloat x, GLfloat y, GLfloat z)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glRotatef"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glRotatef);

	Rotatef(gc, fAngle, x, y, z);

	GLES1_TIME_STOP(GLES1_TIMES_glRotatef);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}

#endif

GL_API void GL_APIENTRY glRotatex(GLfixed fAngle, GLfixed x, GLfixed y, GLfixed z)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glRotatex"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glRotatex);

	Rotatef(gc, FIXED_TO_FLOAT(fAngle), FIXED_TO_FLOAT(x), FIXED_TO_FLOAT(y), FIXED_TO_FLOAT(z));

	GLES1_TIME_STOP(GLES1_TIMES_glRotatex);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}


/***********************************************************************************
 Function Name      : glScale[f/x]
 Inputs             : x, y, z
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Multiplies the current matrix by a scaling matrix
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glScalef(GLfloat x, GLfloat y, GLfloat z)
{
	IMG_FLOAT afScale[3];
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glScalef"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glScalef);

	afScale[0] = x;
	afScale[1] = y;
	afScale[2] = z;

	DoMultMatrix(gc, afScale, ScaleMatrix);

	GLES1_TIME_STOP(GLES1_TIMES_glScalef);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}

#endif

GL_API void GL_APIENTRY glScalex(GLfixed x, GLfixed y, GLfixed z)
{
	IMG_FLOAT afScale[3];
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glScalex"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glScalex);

	afScale[0] = FIXED_TO_FLOAT(x);
	afScale[1] = FIXED_TO_FLOAT(y);
	afScale[2] = FIXED_TO_FLOAT(z);

	DoMultMatrix(gc, afScale, ScaleMatrix);

	GLES1_TIME_STOP(GLES1_TIMES_glScalex);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}


/***********************************************************************************
 Function Name      : glTranslate[f/x]
 Inputs             : x, y, z
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Multiplies the current matrix by a translation matrix
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glTranslatef(GLfloat x, GLfloat y, GLfloat z)
{
	IMG_FLOAT afTranslate[3];
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTranslatef"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glTranslatef);

	afTranslate[0] = x;
	afTranslate[1] = y;
	afTranslate[2] = z;

	DoMultMatrix(gc, afTranslate, TranslateMatrix);

	GLES1_TIME_STOP(GLES1_TIMES_glTranslatef);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}
#endif

GL_API void GL_APIENTRY glTranslatex(GLfixed x, GLfixed y, GLfixed z)
{
	IMG_FLOAT afTranslate[3];
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTranslatex"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glTranslatex);

	afTranslate[0] = FIXED_TO_FLOAT(x);
	afTranslate[1] = FIXED_TO_FLOAT(y);
	afTranslate[2] = FIXED_TO_FLOAT(z);

	DoMultMatrix(gc, afTranslate, TranslateMatrix);

	GLES1_TIME_STOP(GLES1_TIMES_glTranslatex);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}


/***********************************************************************************
 Function Name      : glPushMatrix
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Pushes the current matrix onto its stack.
************************************************************************************/
GL_API void GL_APIENTRY glPushMatrix(IMG_VOID)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPushMatrix"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glPushMatrix);

	(*gc->sProcs.pfnPushMatrix)(gc);

	GLES1_TIME_STOP(GLES1_TIMES_glPushMatrix);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}


/***********************************************************************************
 Function Name      : glPopMatrix
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Pops the current matrix stack
************************************************************************************/
GL_API void GL_APIENTRY glPopMatrix(IMG_VOID)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glPopMatrix"));

	GLES1_TIME_START(GLES1_TIMER_MATRIXMATHS);
	GLES1_TIME_START(GLES1_TIMES_glPopMatrix);

	(*gc->sProcs.pfnPopMatrix)(gc);

	GLES1_TIME_STOP(GLES1_TIMES_glPopMatrix);
	GLES1_TIME_STOP(GLES1_TIMER_MATRIXMATHS);
}

/***********************************************************************************
 Function Name      : CalcClipPlane
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Project user clip plane into eye space
************************************************************************************/
static IMG_VOID CalcClipPlane(GLES1Context *gc, IMG_UINT32 ui32ClipPlane, const GLEScoord *psPlaneEqn)
{
	GLES1Transform *psTransform;

	psTransform = gc->sTransform.psModelView;

	if (psTransform->bUpdateInverse) 
	{
		(*gc->sProcs.pfnComputeInverseTranspose)(gc, psTransform);
	}

	/*
	** Project user clip plane into eye space.
	*/
	(*psTransform->sInverseTranspose.pfnXf4)(&gc->sTransform.asEyeClipPlane[ui32ClipPlane], 
											 &psPlaneEqn->fX, &psTransform->sInverseTranspose);

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
}

#if defined(PROFILE_COMMON)

/***********************************************************************************
 Function Name      : glClipPlanef
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets float user clip plane
************************************************************************************/

GL_API void GL_APIENTRY glClipPlanef(GLenum p, const GLfloat eqn[4])
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glClipPlanef"));

	GLES1_TIME_START(GLES1_TIMES_glClipPlanef);	

	if((p < GL_CLIP_PLANE0) || (p >= (GL_CLIP_PLANE0 + GLES1_MAX_CLIP_PLANES)))
	{
		SetError(gc, GL_INVALID_ENUM);

		GLES1_TIME_STOP(GLES1_TIMES_glClipPlanef);

		return;
	}

	CalcClipPlane(gc, p - GL_CLIP_PLANE0, (const GLEScoord *)eqn);

	GLES1_TIME_STOP(GLES1_TIMES_glClipPlanef);
}
#endif

/***********************************************************************************
 Function Name      : glClipPlanex
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets fixed point user clip plane
************************************************************************************/
GL_API void GL_APIENTRY glClipPlanex(GLenum p, const GLfixed eqn[4])
{
	GLEScoord sClip;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glClipPlanex"));

	GLES1_TIME_START(GLES1_TIMES_glClipPlanex);

	if((p < GL_CLIP_PLANE0) || (p >= (GL_CLIP_PLANE0 + GLES1_MAX_CLIP_PLANES)))
	{
		SetError(gc, GL_INVALID_ENUM);

		GLES1_TIME_STOP(GLES1_TIMES_glClipPlanex);

		return;
	}

	sClip.fX = FIXED_TO_FLOAT(eqn[0]);
	sClip.fY = FIXED_TO_FLOAT(eqn[1]);
	sClip.fZ = FIXED_TO_FLOAT(eqn[2]);
	sClip.fW = FIXED_TO_FLOAT(eqn[3]);

	CalcClipPlane(gc, p - GL_CLIP_PLANE0, &sClip);

	GLES1_TIME_STOP(GLES1_TIMES_glClipPlanex);
}

/******************************************************************************
 End of file (tnlapi.c)
******************************************************************************/

