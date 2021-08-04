/******************************************************************************
 * Name         : texrender.c
 *
 * Copyright    : 2005-2009 by Imagination Technologies Limited.
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
 * Description	: Texture render extension
 *
 * $Log: texrender.c $
 *****************************************************************************/

#include "context.h"

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)


/***********************************************************************************
 Function Name      : ReleasePbufferFromTexture
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : Release a pbuffer from a texture
************************************************************************************/
IMG_INTERNAL IMG_VOID ReleasePbufferFromTexture(GLES1Context *gc, GLESTexture *psTex)
{
	TextureRemoveResident(gc, psTex);

	/* If the texture was live we must ghost it */
	if (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
	{
		TexMgrGhostTexture(gc, psTex);
	}
	else
	{
		KEGLSurfaceUnbind(gc->psSysContext, psTex->hPBuffer);
	}

	/* deassociate the pbuffer and the texture */
	psTex->hPBuffer = IMG_NULL;

	/* hmm... */
	psTex->ui32LevelsConsistent = GLES1_TEX_UNKNOWN;
}


/***********************************************************************************
 Function Name      : GLESBindTexImage
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_BOOL GLESBindTexImage(EGLContextHandle hContext, EGLDrawableHandle hSurface, EGLDrawableHandle *hTexture)
{
	GLES1Context *gc = (GLES1Context *)hContext;
	GLESTexture *psTex;

	psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture][GLES1_TEXTURE_TARGET_2D];

	if(psTex->hPBuffer != IMG_NULL)
	{
		/*  Spec says "If buffer is already bound to a texture then an EGL BAD ACCESS error is returned. "
		*  I believe this should say "if buffer is already bound to THE texture then ...." as per the ARB spec
		*/
		if(psTex->hPBuffer == hSurface)
		{
			return IMG_FALSE;
		}

		/* release old pbuffer first */
		ReleasePbufferFromTexture(gc, psTex);
	}
	else if (psTex->psMemInfo)
	{
		if(KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
		{
			if(TexMgrGhostTexture(gc, psTex) != IMG_TRUE)
			{
				PVR_DPF((PVR_DBG_ERROR,"GLESBindTexImage: Can't ghost the texture"));
				return IMG_FALSE; 
			}
		}
		else
		{
#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent -= psTex->psMemInfo->uAllocSize;
#endif
			GLES1FREEDEVICEMEM_HEAP(gc, psTex->psMemInfo);

			psTex->psMemInfo = IMG_NULL;
		}
	}

	/* associate the pbuffer and the texture */
	psTex->hPBuffer = hSurface;
	*hTexture = (IMG_HANDLE*) psTex;

	KEGLSurfaceBind(hSurface);

	if(TextureCreatePBufferLevel(gc, psTex) != IMG_TRUE)
	{
		return IMG_FALSE;
	}

	psTex->bResidence = IMG_TRUE;

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : GLESReleaseTexImage
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID GLESReleaseTexImage(EGLContextHandle hContext, EGLDrawableHandle hSurface, EGLDrawableHandle *hTexture)
{
	GLESTexture *psTex = (GLESTexture *) *hTexture;

	if(psTex->hPBuffer == hSurface)
	{
		GLES1Context *gc = (GLES1Context *)hContext;

		ReleasePbufferFromTexture(gc, psTex);

		*hTexture = IMG_NULL;
	}
}

#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

/******************************************************************************
 End of file (texrender.c)
******************************************************************************/

