/******************************************************************************
 * Name         : drawtex.c
 *
 * Copyright    : 2006-2008 by Imagination Technologies Limited.
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
 * $Log: drawtex.c $
 **************************************************************************/

#include "context.h"


#if defined(GLES1_EXTENSION_DRAW_TEXTURE)


/***********************************************************************************
 Function Name      : DrawTexture
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 

************************************************************************************/
static IMG_VOID DrawTexture(GLES1Context *gc, GLfloat fXs, GLfloat fYs, GLfloat fZs, GLfloat fWidth, GLfloat fHeight)
{

	GLESViewport *psViewport;
	IMG_FLOAT *pfVertexData, fScreenHeight;
	IMG_UINT32 ui32Unit, ui32VertexDWords, i;
	IMG_UINT16 *pui16Indices;
	IMG_UINT32 *pui32Indices;
	IMG_UINT32 ui32NoClears = 0;
	IMG_FLOAT fNewWidth, fNewHeight, fNewXs, fNewYs;

	if ((fWidth<=0.0f) || (fHeight<=0.0f))
	{
		SetError(gc, GL_INVALID_VALUE);
		return;
	}

	/* Nothing to do if the X.Y offset is outside the drawable */
	if((fXs >= (IMG_FLOAT)gc->psDrawParams->ui32Width) || (fYs >= (IMG_FLOAT)gc->psDrawParams->ui32Height))
	{
		return;
	}

	if(!PrepareToDraw(gc, &ui32NoClears, IMG_TRUE))
	{
		PVR_DPF((PVR_DBG_ERROR,"DrawTexture: Can't prepare to draw"));

		return;
	}

	if(gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
	{
		gc->sPrim.eCurrentPrimitiveType = GLES1_PRIMTYPE_DRAWTEXTURE;

   		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM | GLES1_DIRTYFLAG_VP_STATE | GLES1_DIRTYFLAG_VERTEX_PROGRAM;

		if (gc->ui32RasterEnables & GLES1_RS_POLYOFFSET_ENABLE)
		{
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
		}

		gc->ui32EmitMask |=	GLES1_EMITSTATE_MTE_STATE_CONTROL;
	}
	else
	{
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER | GLES1_DIRTYFLAG_VP_STATE | GLES1_DIRTYFLAG_VERTEX_PROGRAM;
	}

	if(gc->ui32DirtyMask)
	{
		ValidateState(gc);
	}

	if (!gc->ui32NumImageUnitsActive)
	{
		PVR_DPF((PVR_DBG_WARNING,"DrawTexture: No textures enabled"));
		PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);
		return;
	}


	/* Colour(4) + 4 * (position(4) + each texture layer (4)) */
	ui32VertexDWords = 4 + (4 * (4 + (4 * gc->ui32NumImageUnitsActive)));

	pfVertexData = (IMG_FLOAT *) CBUF_GetBufferSpace(gc->apsBuffers, ui32VertexDWords, CBUF_TYPE_VERTEX_DATA_BUFFER, IMG_FALSE);

	if(!pfVertexData)
	{
		PVR_DPF((PVR_DBG_ERROR,"DrawTexture: Can't get vertex buffer space"));
		SetError(gc, GL_OUT_OF_MEMORY);
		PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);
		return;
	}	

	pui32Indices = CBUF_GetBufferSpace(gc->apsBuffers, 2, CBUF_TYPE_INDEX_DATA_BUFFER, IMG_FALSE);

	if(!pui32Indices)
	{
		PVR_DPF((PVR_DBG_ERROR,"DrawTexture: Can't get index buffer space"));
		SetError(gc, GL_OUT_OF_MEMORY);
		PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);
		return;
	}	

	gc->sPrim.pvDrawTextureAddr = pfVertexData;
	pui16Indices = (IMG_UINT16 *)pui32Indices;
 
	/*
	** Clamp Z
	*/
	psViewport = &gc->sState.sViewport;

	if (fZs<=0.0f)
	{
		fZs = psViewport->fZNear;
	}
	else if (fZs>1.0f)
	{
		fZs = psViewport->fZFar;
	}
	else
	{
		fZs = psViewport->fZNear + fZs*(psViewport->fZFar - psViewport->fZNear);
	}

	/*
	** Current color for all vertices
	*/
	pfVertexData[0] = gc->sState.sCurrent.asAttrib[AP_COLOR].fX;
	pfVertexData[1]	= gc->sState.sCurrent.asAttrib[AP_COLOR].fY;
	pfVertexData[2] = gc->sState.sCurrent.asAttrib[AP_COLOR].fZ;
	pfVertexData[3]	= gc->sState.sCurrent.asAttrib[AP_COLOR].fW;

	/*
	** Calculate the strip's window coordinates
	**
	** Clip primitive to screen boundary to prevent guard band clamping
	** from moving the vertices (but not moving the texcoords)
	*/
	fNewXs = fXs;
	fNewYs = fYs;
	fNewWidth = fWidth;
	fNewHeight = fHeight;

	if(fNewXs < 0.0f)
	{
		fNewWidth += fNewXs;

		fNewXs = 0.0f;
	}

	if(fNewYs < 0.0f)
	{
		fNewHeight += fNewYs;

		fNewYs = 0.0f;
	}

	fScreenHeight = (IMG_FLOAT)gc->psDrawParams->ui32Height;

	if((fNewXs + fNewWidth) > (IMG_FLOAT)gc->psDrawParams->ui32Width)
	{
		fNewWidth = (IMG_FLOAT)gc->psDrawParams->ui32Width - fNewXs;
	}

	if((fNewYs + fNewHeight) > fScreenHeight)
	{
		fNewHeight = fScreenHeight - fNewYs;
	}


	if(gc->psDrawParams->eRotationAngle!=PVRSRV_FLIP_Y)
	{
		pfVertexData[4]  = fNewXs;
		pfVertexData[5]  = fScreenHeight - fNewYs;
		pfVertexData[6]  = fZs;
		pfVertexData[7]  = 1.0f;

		pfVertexData[8]  = fNewXs;
		pfVertexData[9]  = fScreenHeight - (fNewYs + fNewHeight);
		pfVertexData[10] = fZs;
		pfVertexData[11] = 1.0f;

		pfVertexData[12] = fNewXs + fNewWidth;
		pfVertexData[13] = fScreenHeight - fNewYs;
		pfVertexData[14] = fZs;
		pfVertexData[15] = 1.0f;

		pfVertexData[16] = fNewXs + fNewWidth;
		pfVertexData[17] = fScreenHeight - (fNewYs + fNewHeight);
		pfVertexData[18] = fZs;
		pfVertexData[19] = 1.0f;
	}
	else
	{
		pfVertexData[4]  = fNewXs;
		pfVertexData[5]  = fNewYs;
		pfVertexData[6]  = fZs;
		pfVertexData[7]  = 1.0f;

		pfVertexData[8]  = fNewXs;
		pfVertexData[9]  = fNewYs + fNewHeight;
		pfVertexData[10] = fZs;
		pfVertexData[11] = 1.0f;

		pfVertexData[12] = fNewXs + fNewWidth;
		pfVertexData[13] = fNewYs;
		pfVertexData[14] = fZs;
		pfVertexData[15] = 1.0f;

		pfVertexData[16] = fNewXs + fNewWidth;
		pfVertexData[17] = fNewYs + fNewHeight;
		pfVertexData[18] = fZs;
		pfVertexData[19] = 1.0f;
	}

	pfVertexData += 20;

	/*
	** Calculate the strip's texture coordinates
	*/
	for (i=0; i<gc->ui32NumImageUnitsActive; i++)
	{
		GLESTexture *psTex;
		GLESTextureParamState *psParamState;
		IMG_FLOAT fX0, fX1, fY0, fY1, fWt, fHt;

		ui32Unit = gc->ui32TexImageUnitsEnabled[i];
		psTex = gc->sTexture.apsBoundTexture[ui32Unit][gc->sTexture.aui32CurrentTarget[ui32Unit]];
		psParamState = &psTex->sState;

		fWt = (IMG_FLOAT)psTex->psMipLevel[0].ui32Width;
		fHt = (IMG_FLOAT)psTex->psMipLevel[0].ui32Height;

		/*
		** s = (Ucr + (X - Xs)*(Wcr/Ws)) / Wt
		** t = (Vcr + (Y - Ys)*(Hcr/Hs)) / Ht
		** r = 0
		** q = 1
		*/
		fX0 = ((IMG_FLOAT)psParamState->i32CropRectU + (fNewXs - fXs) * ((IMG_FLOAT)psParamState->i32CropRectW / fWidth)) / fWt;
		fX1 = ((IMG_FLOAT)psParamState->i32CropRectU + (fNewXs + fNewWidth - fXs) * ((IMG_FLOAT)psParamState->i32CropRectW / fWidth)) / fWt;

		fY0 = ((IMG_FLOAT)psParamState->i32CropRectV + (fNewYs - fYs) * ((IMG_FLOAT)psParamState->i32CropRectH / fHeight)) / fHt;
		fY1 = ((IMG_FLOAT)psParamState->i32CropRectV + (fNewYs + fNewHeight - fYs) * ((IMG_FLOAT)psParamState->i32CropRectH / fHeight)) / fHt;

		pfVertexData[0]  = fX0;
		pfVertexData[1]  = fY0;
		pfVertexData[2]  = 0.0f;
		pfVertexData[3]  = 1.0f;

		pfVertexData[4]  = fX0;
		pfVertexData[5]  = fY1;
		pfVertexData[6]  = 0.0f;
		pfVertexData[7]  = 1.0f;

		pfVertexData[8]  = fX1;
		pfVertexData[9]  = fY0;
		pfVertexData[10] = 0.0f;
		pfVertexData[11] = 1.0f;

		pfVertexData[12] = fX1;
		pfVertexData[13] = fY1;
		pfVertexData[14] = 0.0f;
		pfVertexData[15] = 1.0f;

		pfVertexData += 16;
	}

	/*
	** Indices for the strip
	*/
	pui16Indices[0] = 0;
	pui16Indices[1] = 1;
	pui16Indices[2] = 2;
	pui16Indices[3] = 3;

	/* Attach all used resources to the current surface */
	AttachAllUsedResourcesToCurrentSurface(gc);

	/* Emit state */
	GLES1EmitState(gc, 4, CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32Indices, CBUF_TYPE_INDEX_DATA_BUFFER), 0);

	CBUF_UpdateBufferPos(gc->apsBuffers, ui32VertexDWords, CBUF_TYPE_VERTEX_DATA_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_VERTEX_DATA_COUNT, ui32VertexDWords);

	CBUF_UpdateBufferPos(gc->apsBuffers, 2, CBUF_TYPE_INDEX_DATA_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_INDEX_DATA_COUNT, 2);

	/*
		Update vertex and index buffers committed primitive offset
	*/
	CBUF_UpdateVIBufferCommittedPrimOffsets(gc->apsBuffers, &gc->psRenderSurface->bPrimitivesSinceLastTA, (IMG_VOID *)gc, KickLimit_ScheduleTA);

	PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);
}						 


/***********************************************************************************
 Function Name      : glDrawTexsOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 

************************************************************************************/
GL_API_EXT void GL_APIENTRY glDrawTexsOES(GLshort x, GLshort y, GLshort z, GLshort width, GLshort height)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDrawTexsOES"));

	GLES1_TIME_START(GLES1_TIMES_glDrawTexsOES);

	DrawTexture(gc, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)width, (GLfloat)height);

	GLES1_TIME_STOP(GLES1_TIMES_glDrawTexsOES);
}	


/***********************************************************************************
 Function Name      : glDrawTexiOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 

************************************************************************************/
GL_API_EXT void GL_APIENTRY glDrawTexiOES(GLint x, GLint y, GLint z, GLint width, GLint height)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDrawTexiOES"));

	GLES1_TIME_START(GLES1_TIMES_glDrawTexiOES);

	DrawTexture(gc, (GLfloat)x, (GLfloat)y, (GLfloat)z, (GLfloat)width, (GLfloat)height);

	GLES1_TIME_STOP(GLES1_TIMES_glDrawTexiOES);
}	


#if defined(PROFILE_COMMON)
/***********************************************************************************
 Function Name      : glDrawTexfOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 

************************************************************************************/
GL_API_EXT void GL_APIENTRY glDrawTexfOES(GLfloat x, GLfloat y, GLfloat z, GLfloat width, GLfloat height)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDrawTexfOES"));

	GLES1_TIME_START(GLES1_TIMES_glDrawTexfOES);

	DrawTexture(gc, x, y, z, width, height);

	GLES1_TIME_STOP(GLES1_TIMES_glDrawTexfOES);
}
#endif /* PROFILE_COMMON */


/***********************************************************************************
 Function Name      : glDrawTexxOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 

************************************************************************************/
GL_API_EXT void GL_APIENTRY glDrawTexxOES(GLfixed x, GLfixed y, GLfixed z, GLfixed width, GLfixed height)
{
	GLfloat fX, fY, fZ, fWidth, fHeight;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDrawTexxOES"));

	GLES1_TIME_START(GLES1_TIMES_glDrawTexxOES);

	fX		= FIXED_TO_FLOAT(x);
	fY		= FIXED_TO_FLOAT(y);
	fZ		= FIXED_TO_FLOAT(z);
	fWidth	= FIXED_TO_FLOAT(width);
	fHeight	= FIXED_TO_FLOAT(height);

	DrawTexture(gc, fX, fY, fZ, fWidth, fHeight);

	GLES1_TIME_STOP(GLES1_TIMES_glDrawTexxOES);
}	


/***********************************************************************************
 Function Name      : glDrawTexsvOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 

************************************************************************************/
GL_API_EXT void GL_APIENTRY glDrawTexsvOES(const GLshort *coords)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDrawTexsvOES"));

	GLES1_TIME_START(GLES1_TIMES_glDrawTexsvOES);

	DrawTexture(gc, (GLfloat)coords[0], (GLfloat)coords[1], (GLfloat)coords[2], (GLfloat)coords[3], (GLfloat)coords[4]);

	GLES1_TIME_STOP(GLES1_TIMES_glDrawTexsvOES);
}


/***********************************************************************************
 Function Name      : glDrawTexivOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 

************************************************************************************/
GL_API_EXT void GL_APIENTRY glDrawTexivOES(const GLint *coords)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDrawTexivOES"));

	GLES1_TIME_START(GLES1_TIMES_glDrawTexivOES);

	DrawTexture(gc, (GLfloat)coords[0], (GLfloat)coords[1], (GLfloat)coords[2], (GLfloat)coords[3], (GLfloat)coords[4]);

	GLES1_TIME_STOP(GLES1_TIMES_glDrawTexivOES);
}


#if defined(PROFILE_COMMON)
/***********************************************************************************
 Function Name      : glDrawTexfvOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 

************************************************************************************/
GL_API_EXT void GL_APIENTRY glDrawTexfvOES(const GLfloat *coords)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDrawTexfvOES"));

	GLES1_TIME_START(GLES1_TIMES_glDrawTexfvOES);

	DrawTexture(gc, coords[0], coords[1], coords[2], coords[3], coords[4]);

	GLES1_TIME_STOP(GLES1_TIMES_glDrawTexfvOES);
}
#endif /* PROFILE_COMMON */


/***********************************************************************************
 Function Name      : glDrawTexxvOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 

************************************************************************************/
GL_API_EXT void GL_APIENTRY glDrawTexxvOES(const GLfixed *coords)
{
	GLfloat fX, fY, fZ, fWidth, fHeight;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDrawTexxvOES"));

	GLES1_TIME_START(GLES1_TIMES_glDrawTexxvOES);

	fX		= FIXED_TO_FLOAT(coords[0]);
	fY		= FIXED_TO_FLOAT(coords[1]);
	fZ		= FIXED_TO_FLOAT(coords[2]);
	fWidth	= FIXED_TO_FLOAT(coords[3]);
	fHeight	= FIXED_TO_FLOAT(coords[4]);

	DrawTexture(gc, fX, fY, fZ, fWidth, fHeight);

	GLES1_TIME_STOP(GLES1_TIMES_glDrawTexxvOES);
}

#endif /* defined(GLES1_EXTENSION_DRAW_TEXTURE) */

/******************************************************************************
 End of file (drawtex.c)
******************************************************************************/
