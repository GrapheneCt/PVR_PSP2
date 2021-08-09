/******************************************************************************
 * Name         : fftnlgles.c
 *
 * Copyright    : 2006-2007 by Imagination Technologies Limited.
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
 * $Log: fftnlgles.c $
 *****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "context.h"
#include "dmscalc.h"

#if defined(FFGEN_UNIFLEX)
#include "usc.h"

#if defined(DEBUG)
static FILE *fOutputFile = 0;
#endif

#endif /* defined(FFGEN_UNIFLEX) */

#define NUM_PALETTE_ENTRIES_MIN MIN(gc->sPrim.ui32MaxMatrixPaletteIndex + 2, GLES1_MAX_PALETTE_MATRICES)

static IMG_VOID IMG_CALLCONV FFGENPrintf(const IMG_CHAR* pszFormat, ...) IMG_FORMAT_PRINTF(1, 2);

/***********************************************************************************
 Function Name      : FFGENMalloc
 Inputs             : hHandle, ui32Size
 Outputs            : None
 Returns            : pvAddress
 Description        : Allocs FFGEN mem
************************************************************************************/
static IMG_VOID * IMG_CALLCONV FFGENMalloc(IMG_HANDLE hHandle, IMG_UINT32 ui32Size)
{
	PVR_UNREFERENCED_PARAMETER(hHandle);

	return GLES1Malloc((GLES1Context *) hHandle, ui32Size);
}


/***********************************************************************************
 Function Name      : FFGENCalloc
 Inputs             : hHandle, ui32Size
 Outputs            : None
 Returns            : pvAddress
 Description        : Callocs FFGEN mem
************************************************************************************/
static IMG_VOID * IMG_CALLCONV FFGENCalloc(IMG_HANDLE hHandle, IMG_UINT32 ui32Size)
{
	PVR_UNREFERENCED_PARAMETER(hHandle);

	return GLES1Calloc((GLES1Context *) hHandle, ui32Size);
}


/***********************************************************************************
 Function Name      : FFTNLRealloc
 Inputs             : hHandle, pvData, ui32Size
 Outputs            : None
 Returns            : pvData
 Description        : Reallocs FFGEN mem
************************************************************************************/
static IMG_VOID * IMG_CALLCONV FFGENRealloc(IMG_HANDLE hHandle, IMG_VOID *pvData, IMG_UINT32 ui32Size)
{
	PVR_UNREFERENCED_PARAMETER(hHandle);

	return GLES1Realloc((GLES1Context *) hHandle, pvData, ui32Size);
}


/***********************************************************************************
 Function Name      : FFGENFree
 Inputs             : hHandle, pvData
 Outputs            : none
 Returns            : None
 Description        : Frees FFGEN mem
************************************************************************************/
static IMG_VOID IMG_CALLCONV FFGENFree(IMG_HANDLE hHandle, IMG_VOID *pvData)
{
	PVR_UNREFERENCED_PARAMETER(hHandle);

	GLES1Free(IMG_NULL, pvData);
}


/***********************************************************************************
 Function Name      : FFGENPrintf
 Inputs             : pszFormat
 Outputs            : none
 Returns            : None
 Description        : Prints FFGEN messages
************************************************************************************/
static IMG_VOID IMG_CALLCONV FFGENPrintf(const IMG_CHAR* pszFormat, ...)
{
#if defined(DEBUG)
	va_list vaArgs;
	char szBuffer[256];

	va_start (vaArgs, pszFormat);
	vsprintf (szBuffer, pszFormat, vaArgs);

	PVR_TRACE(("FFGen: %s", szBuffer));

	va_end (vaArgs);
#else
	PVR_UNREFERENCED_PARAMETER(pszFormat);
#endif
}


/***********************************************************************************
 Function Name      : DestroyFFTNLCode
 Inputs             : gc
 Outputs            : none
 Returns            : None
 Description        : Destroys FFTNL Code
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyFFTNLCode(GLES1Context *gc, IMG_UINT32 ui32Item)
{
	GLES1Shader *psVertexShader;
	FFGenProgram *psFFTNLProgram;

	psVertexShader = (GLES1Shader *)ui32Item;

	psFFTNLProgram = psVertexShader->u.sFFTNL.psFFTNLProgram;

#if defined(FFGEN_UNIFLEX)
	if(psFFTNLProgram->pui32UFConstantData)
	{
		GLES1Free(IMG_NULL, psFFTNLProgram->pui32UFConstantData);
	}

	if(psFFTNLProgram->pui32UFConstantDest)
	{
		GLES1Free(IMG_NULL, psFFTNLProgram->pui32UFConstantDest);
	}
#endif /* defined(FFGEN_UNIFLEX) */

	/* Free program code */
	if(psFFTNLProgram)
	{
		/* Free the actual program */
		FFGenFreeProgram((IMG_VOID *)gc->sProgram.hFFTNLGenContext, psFFTNLProgram);
	}

	/* Unlink shader */
	if(psVertexShader->psPrevious)
	{
		psVertexShader->psPrevious->psNext = psVertexShader->psNext;
	}
	else
	{
		/* Shader is at head of list */
		gc->sProgram.psVertex = psVertexShader->psNext;
	}

	if(psVertexShader->psNext)
	{
		psVertexShader->psNext->psPrevious = psVertexShader->psPrevious;
	}

	FreeShader(gc, psVertexShader);
}


#if defined(FFGEN_UNIFLEX)
/***********************************************************************************
 Function Name      : UniFlexAlloc
 Inputs             : ui32Size
 Outputs            : -
 Returns            : 
 Description        : Alocates UniFlex mem
************************************************************************************/
static IMG_VOID *UniFlexAlloc(IMG_UINT32 ui32Size)
{
    return GLES1Malloc(IMG_NULL, ui32Size);
}


/***********************************************************************************
 Function Name      : UniFlexFree
 Inputs             : pvData
 Outputs            : -
 Returns            : -
 Description        : Frees UniFlex mem
************************************************************************************/
static IMG_VOID UniFlexFree(IMG_VOID *pvData)
{
    GLES1Free(IMG_NULL, pvData);
}


static IMG_VOID UniFlexPrint(const IMG_CHAR *pszFmt, ...) IMG_FORMAT_PRINTF(1, 2);

/***********************************************************************************
 Function Name      : UniFlexPrint
 Inputs             : pszFmt, ...
 Outputs            : -
 Returns            : -
 Description        : Prints UniFlex messages
************************************************************************************/
static IMG_VOID UniFlexPrint(const IMG_CHAR *pszFmt, ...)
{
#if defined(DEBUG)
	va_list ap;
	IMG_CHAR pszTemp[256];

	va_start(ap, pszFmt);
	vsprintf(pszTemp, pszFmt, ap);

	if(fOutputFile)
	{
		fprintf(fOutputFile,"%s\n",pszTemp);
	}

	va_end(ap);
#else
	PVR_UNREFERENCED_PARAMETER(pszFmt);
#endif

}	
#endif /* defined(FFGEN_UNIFLEX) */


/***********************************************************************************
 Function Name      : InitFFTNLState
 Inputs             : gc
 Outputs            : gc->hFFTNLGenContext
 Returns            : None
 Description        : Initialises FF TNL 
************************************************************************************/
IMG_INTERNAL IMG_BOOL InitFFTNLState(GLES1Context *gc)
{
#if !defined(FFGEN_UNIFLEX)
	if (gc->sAppHints.bDumpShaders)
	{
		gc->sProgram.hFFTNLGenContext = FFGenCreateContext((IMG_HANDLE)(gc), GLES1_FFTNL_SHADERS_FILENAME,
												FFGENMalloc, FFGENCalloc, FFGENRealloc, FFGENFree, FFGENPrintf);
	}
	else
#endif /* !defined(FFGEN_UNIFLEX) */
	{
		gc->sProgram.hFFTNLGenContext = FFGenCreateContext((IMG_HANDLE)(gc), IMG_NULL,
												FFGENMalloc, FFGENCalloc, FFGENRealloc, FFGENFree, FFGENPrintf);
	}

	if(gc->sProgram.hFFTNLGenContext == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_WARNING, "InitFFTNLState: Failed create FFTNL context"));

		return IMG_FALSE;
	}

#if defined(FFGEN_UNIFLEX)
	gc->sProgram.hUniFlexContext = PVRUniFlexCreateContext(UniFlexAlloc, UniFlexFree, UniFlexPrint,
											   IMG_NULL, IMG_NULL, IMG_NULL, IMG_NULL, IMG_NULL);

	if(gc->sProgram.hUniFlexContext == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_WARNING, "InitFFTNLState: Failed create USC context"));

		FFGenDestroyContext((IMG_VOID*)gc->sProgram.hFFTNLGenContext);

		return IMG_FALSE;
	}
#endif /* defined(FFGEN_UNIFLEX) */

	if(!HashTableCreate(gc, &gc->sProgram.sFFTNLHashTable, STATEHASH_LOG2TABLESIZE, STATEHASH_MAXNUMENTRIES, DestroyFFTNLCode
#ifdef HASHTABLE_DEBUG
		, "FFTNL"
#endif
		))
	{
		PVR_DPF((PVR_DBG_WARNING, "InitFFTNLState: Failed create FFTNL hash table"));

#if defined(FFGEN_UNIFLEX)
	PVRUniFlexDestroyContext((IMG_VOID*)gc->sProgram.hUniFlexContext);
#endif /* defined(FFGEN_UNIFLEX) */

		FFGenDestroyContext((IMG_VOID*)gc->sProgram.hFFTNLGenContext);

		return IMG_FALSE;
	}

#if defined(FFGEN_UNIFLEX) && defined(DEBUG)
	if(gc->sAppHints.bDumpShaders)
	{
		fOutputFile = fopen("USCShaders.txt","w+t");
	}
#endif /* defined(FFGEN_UNIFLEX) && defined(DEBUG)*/

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : FreeFFTNLState
 Inputs             : gc
 Outputs            : None
 Returns            : None
 Description        : Deinitialises FF TNL 
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeFFTNLState(GLES1Context *gc)
{
	IMG_HANDLE *hFFTNLGenContext = gc->sProgram.hFFTNLGenContext;

	/* Hash table call back function will destroy FFTNL code entries  */
	HashTableDestroy(gc, &gc->sProgram.sFFTNLHashTable);

	if(hFFTNLGenContext)
	{
		FFGenDestroyContext((IMG_VOID*)gc->sProgram.hFFTNLGenContext);
	}

#if defined(FFGEN_UNIFLEX)
	PVRUniFlexDestroyContext((IMG_VOID*)gc->sProgram.hUniFlexContext);

#if defined(DEBUG)
	if(gc->sAppHints.bDumpShaders)
	{
		fclose(fOutputFile);
	}
#endif /* defined(DEBUG) */

#endif /* defined(FFGEN_UNIFLEX) */
}

/***********************************************************************************
 Function Name      : GetFFTNLGenDesc
 Inputs             : gc
 Outputs            : psFFTNLGenDesc
 Returns            : None
 Description        : Gets a FFTNL desc
************************************************************************************/
static IMG_VOID GetFFTNLGenDesc(GLES1Context *gc, FFTNLGenDesc *psFFTNLGenDesc)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32FFTNLEnables1 = 0;
	IMG_UINT32 ui32FFTNLEnables2 = 0;

	IMG_UINT32 ui32TnLEnables = gc->ui32TnLEnables;
	IMG_UINT32 ui32RasterEnables = gc->ui32RasterEnables;

	IMG_UINT32 ui32ClipPlanesMask = (ui32TnLEnables & GLES1_TL_CLIP_PLANES_ENABLE) >> GLES1_TL_CLIP_PLANE0_SHIFT;

	GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);

	/* Set everything to 0 */
	PVRSRVMemSet(psFFTNLGenDesc, 0, sizeof(FFTNLGenDesc));

	/* Some default values */
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)

	psFFTNLGenDesc->uNumBlendUnits = psVAOMachine->asAttribPointer[AP_MATRIXINDEX].psState->ui32StreamTypeSize >> GLES1_STREAMSIZE_SHIFT;;
//	psFFTNLGenDesc->uNumBlendUnits = psVAOMachine->asAttribPointer[AP_MATRIXINDEX].ui32CopyStreamTypeSize >> GLES1_STREAMSIZE_SHIFT;;

	psFFTNLGenDesc->uNumMatrixPaletteEntries = NUM_PALETTE_ENTRIES_MIN;
#else
	psFFTNLGenDesc->uNumBlendUnits           = 0;
	psFFTNLGenDesc->uNumMatrixPaletteEntries = 0;
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
	psFFTNLGenDesc->eCodeGenMethod 			 = FFCGM_TWO_PASS;  


#if defined(FFGEN_UNIFLEX)
	psFFTNLGenDesc->eCodeGenFlags |= FFGENCGF_INPUT_REG_SIZE_4;
#endif /* defined(FFGEN_UNIFLEX) */

	/* DrawTexture has no transform or clipping etc */
	if (gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
	{
		/* Point size is enabled based on the current primitive mode */
		if((gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_POINT) ||
		   (gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_SPRITE))
		{
			if (gc->ui32TnLEnables & GLES1_TL_POINTSPRITE_ENABLE)
			{
				ui32FFTNLEnables2 |= FFTNL_ENABLES2_POINTSPRITES;
			}
			else
			{
				ui32FFTNLEnables2 |= FFTNL_ENABLES2_POINTS;
			}

			if(psVAOMachine->ui32ArrayEnables & VARRAY_POINTSIZE_ENABLE)
			{
				ui32FFTNLEnables1 |= FFTNL_ENABLES1_POINTSIZEARRAY;
			}
		}

		/* Determine transformation code to be executed - default is no transformation - normal and position passed straight through */
		if (ui32TnLEnables & GLES1_TL_MATRIXPALETTE_ENABLE)
		{
			ui32FFTNLEnables1 |= FFTNL_ENABLES1_MATRIXPALETTEBLENDING;
		}
		else
		{
			ui32FFTNLEnables1 |= FFTNL_ENABLES1_STANDARDTRANSFORMATION;
		}

		/* Should the clipping code be executed? */
		if (ui32ClipPlanesMask)
		{
			ui32FFTNLEnables1 |= FFTNL_ENABLES1_CLIPPING;
		
			psFFTNLGenDesc->uEnabledClipPlanes = ui32ClipPlanesMask;
		}

		/* Point attenuation is required if the attenuation constants are not {1, 0, 0} */
		/* 1.0f can be exactly expressed in binary using IEEE float format */
		/* PRQA S 3341 ++ */
		if((gc->sState.sPoint.afAttenuation[0] != 1.0f) ||
		   (gc->sState.sPoint.afAttenuation[1] != 0.0f) ||
		   (gc->sState.sPoint.afAttenuation[2] != 0.0f))
		{
			/* Only enable for point/pointsprite primitives */
			if(((ui32FFTNLEnables1 & FFTNL_ENABLES1_POINTSIZEARRAY) != 0) ||
			   ((ui32FFTNLEnables2 & (FFTNL_ENABLES2_POINTS|FFTNL_ENABLES2_POINTSPRITES)) != 0))
			{
				ui32FFTNLEnables1 |= FFTNL_ENABLES1_POINTATTEN | FFTNL_ENABLES1_VERTEXTOEYEVECTOR;
			}
		}
		/* PRQA S 3341 -- */

		/* Sort out colour material */
		if (ui32TnLEnables & GLES1_TL_COLORMAT_ENABLE)
		{
			ui32FFTNLEnables1 |= FFTNL_ENABLES1_COLOURMATERIAL;

			switch (gc->sState.sLight.eColorMaterialParam) 
			{
				case GL_EMISSION:
				{
					ui32FFTNLEnables1 |= FFTNL_ENABLES1_VERTCOLOUREMISSIVE;

					break;
				}
				case GL_AMBIENT:
				{
					ui32FFTNLEnables1 |= FFTNL_ENABLES1_VERTCOLOURAMBIENT;

					break;
				}
				case GL_DIFFUSE:
				{
					ui32FFTNLEnables1 |= FFTNL_ENABLES1_VERTCOLOURDIFFUSE;

					break;
				}
				case GL_AMBIENT_AND_DIFFUSE:
				{
					ui32FFTNLEnables1 |= FFTNL_ENABLES1_VERTCOLOURAMBIENT | FFTNL_ENABLES1_VERTCOLOURDIFFUSE;

					break;
				}
				case GL_SPECULAR:
				{
					ui32FFTNLEnables1 |= FFTNL_ENABLES1_VERTCOLOURSPECULAR;

					break;
				}
			}
		}

		/* Lighting */
		if (ui32TnLEnables & GLES1_TL_LIGHTING_ENABLE)
		{
			IMG_UINT32 ui32NumberOfLights = 0;
			GLESLightSourceState *psLightState = &gc->sState.sLight.psSource[0];
			IMG_UINT32 ui32EnablesLight = (ui32TnLEnables & GLES1_TL_LIGHTS_ENABLE) >> GLES1_TL_LIGHT0;
			/* Does the material have a specular component */
			IMG_BOOL bMaterialHasSpecular;

			/* 0.0f can be exactly expressed in binary using IEEE float format */
			/* PRQA S 3341 ++ */
			if ( gc->sState.sLight.sMaterial.sSpecular.fRed != 0.0f ||
				 gc->sState.sLight.sMaterial.sSpecular.fGreen != 0.0f ||
				 gc->sState.sLight.sMaterial.sSpecular.fBlue != 0.0f ||
				 ((ui32FFTNLEnables1 & FFTNL_ENABLES1_VERTCOLOURSPECULAR) != 0) )
			{
				bMaterialHasSpecular = IMG_TRUE;
			}
			else
			{
				bMaterialHasSpecular = IMG_FALSE;
			}
			/* PRQA S 3341 -- */

			ui32FFTNLEnables1 |= FFTNL_ENABLES1_LIGHTINGENABLED;  


			if (gc->sState.sLight.sModel.bTwoSided)
			{
				/* Need to enable two sided lighting */
				if ((gc->sPrim.eCurrentPrimitiveType>=GLES1_PRIMTYPE_TRIANGLE) &&
				    (gc->sPrim.eCurrentPrimitiveType<=GLES1_PRIMTYPE_TRIANGLE_FAN))
				{
					ui32FFTNLEnables1 |= FFTNL_ENABLES1_LIGHTINGTWOSIDED;
				}
			
			}

			for (i = 0; (GLint)i < GLES1_MAX_LIGHTS; i++, psLightState++, ui32EnablesLight >>= 1)
			{
				if (ui32EnablesLight & 1)
				{
					IMG_BOOL bLightHasSpecular;

					/* 0.0f can be exactly expressed in binary using IEEE float format */
					/* PRQA S 3341 ++ */
					bLightHasSpecular = (IMG_BOOL)(psLightState->sSpecular.fRed != 0.0f ||
												   psLightState->sSpecular.fGreen != 0.0f ||
												   psLightState->sSpecular.fBlue != 0.0f);
					/* PRQA S 3341 -- */
					
					/* PRQA S 3341 1 */ /* 0.0f can be exactly expressed in binary using IEEE float format */
					if (psLightState->sPositionEye.fW == 0.0f) 
					{
						if (psLightState->fSpotLightCutOffAngle >= 180.0f)
						{
							psFFTNLGenDesc->uEnabledPointInfiniteLights |= (1UL << i);
						}
						else
						{
							psFFTNLGenDesc->uEnabledSpotInfiniteLights |= (1UL << i);
						}
					}
					else if (psLightState->fSpotLightCutOffAngle >= 180.0f)
					{
						psFFTNLGenDesc->uEnabledPointLocalLights |= (1UL << i);
					}
					else
					{
						psFFTNLGenDesc->uEnabledSpotLocalLights |= (1UL << i);
					}

					/* Check if light has specular */
					if (bLightHasSpecular && bMaterialHasSpecular)
					{
						psFFTNLGenDesc->uLightHasSpecular |= (1UL << i);
					}

					ui32NumberOfLights++;
				}

				/* Only care about this stuff if we're actually doing lighting */
				if (ui32NumberOfLights > 0)
				{
					ui32FFTNLEnables1 |= FFTNL_ENABLES1_TRANSFORMNORMALS;
				}
			}
		}

		/* Should the normals be normalised? */
		if (ui32TnLEnables & (GLES1_TL_NORMALIZE_ENABLE | GLES1_TL_RESCALE_ENABLE))
		{
			/* Only normalise if we've got some  */
			if (ui32FFTNLEnables1 & FFTNL_ENABLES1_TRANSFORMNORMALS)
			{
				ui32FFTNLEnables1 |= FFTNL_ENABLES1_NORMALISENORMALS;
			}
		}

		/* Texturing */
		if (gc->ui32NumImageUnitsActive)
		{
			for (i = 0; i < gc->ui32NumImageUnitsActive; i++)
			{
				IMG_UINT32 j;
				IMG_UINT32 ui32Unit = gc->ui32TexImageUnitsEnabled[i];

				if ((gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_SPRITE) && 
					 gc->sState.sTexture.asUnit[ui32Unit].sEnv.bPointSpriteReplace)
				{
					/* Do nothing */
				}
				else
				{
				    GLES1AttribArrayPointerMachine *psAPMachine;
					IMG_UINT8 ui8CoordMask;

					/* Only enable texure coord processing if needed */
					ui32FFTNLEnables1 |= FFTNL_ENABLES1_TEXTURINGENABLED;

					psAPMachine = &(psVAOMachine->asAttribPointer[AP_TEXCOORD0 + ui32Unit]);


					/* Work out texture matrix stuff */
					if (gc->sTransform.apsTexture[ui32Unit]->sMatrix.eMatrixType != GLES1_MT_IDENTITY)
					{
						psFFTNLGenDesc->uEnabledTextureMatrices |= (1UL << ui32Unit);

						/* Need all 4 */
						ui8CoordMask = 0xF;
					}
					else
					{
						if (psAPMachine->bIsCurrentState)
						{
							/* Need all 4 */
							ui8CoordMask = 0xF;
						}
						else
						{
							IMG_UINT32 ui32UserInputSize;

							/* Need at least 2 */
							ui8CoordMask = 0x3;

							ui32UserInputSize = (psAPMachine->psState->ui32StreamTypeSize & GLES1_STREAMSIZE_MASK) >> GLES1_STREAMSIZE_SHIFT;

							for (j=2; j<ui32UserInputSize; j++)
							{
								ui8CoordMask |= ((IMG_UINT8)(1 << j));
							}
						}
					}

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
					if (gc->ui32RasterEnables & (1UL << (GLES1_RS_GENTEXTURE0 + ui32Unit)))
					{
						/* Need at least 3 */
						ui8CoordMask |= 0x7;

						switch (gc->sState.sTexture.asUnit[ui32Unit].eTexGenMode) 
						{
							case GL_NORMAL_MAP_OES:
							{
								ui32FFTNLEnables1 |= FFTNL_ENABLES1_TRANSFORMNORMALS;

								psFFTNLGenDesc->uEnabledNormalMapCoords	|= (1UL << ui32Unit);

								psFFTNLGenDesc->aubNormalMapCoordMask[ui32Unit] = ui8CoordMask;

								break;
							}
							case GL_REFLECTION_MAP_OES:
							{
								ui32FFTNLEnables1 |= (FFTNL_ENABLES1_REFLECTIONMAP | FFTNL_ENABLES1_TRANSFORMNORMALS);

								psFFTNLGenDesc->uEnabledReflectionMapCoords	|= (1UL << ui32Unit);

								psFFTNLGenDesc->aubReflectionMapCoordMask[ui32Unit] = ui8CoordMask;

								break;
							}
						}
					}
					else
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
					{
						psFFTNLGenDesc->uEnabledPassthroughCoords |= (1UL << ui32Unit);

						/* No tex gen so pass this coord straight through */
						psFFTNLGenDesc->aubPassthroughCoordMask[ui32Unit] = ui8CoordMask;
					}

				}
			}

			/* De we need a vertex to eye vector? */ 
			if (psFFTNLGenDesc->uEnabledReflectionMapCoords)
			{
				ui32FFTNLEnables1 |= FFTNL_ENABLES1_VERTEXTOEYEVECTOR;
			}
		}
	}
	else
	{
		ui32FFTNLEnables1 |= FFTNL_ENABLES1_TEXTURINGENABLED;

		for (i = 0; i < gc->ui32NumImageUnitsActive; i++)
		{
			IMG_UINT32 ui32Unit = gc->ui32TexImageUnitsEnabled[i];

			psFFTNLGenDesc->uEnabledPassthroughCoords |= (1UL << ui32Unit);

			psFFTNLGenDesc->aubPassthroughCoordMask[ui32Unit]  = 3;
		}
	}

	/* Is a fog value required? */
	if(ui32RasterEnables & GLES1_RS_FOG_ENABLE)
	{
		if(gc->sPrim.eCurrentPrimitiveType == GLES1_PRIMTYPE_DRAWTEXTURE)
		{
			ui32FFTNLEnables2 |= FFTNL_ENABLES2_FOGCOORDZERO;
		}
		else
		{
			ui32FFTNLEnables1 |= FFTNL_ENABLES1_FOGCOORDEYEPOS;
		}

		if (gc->sState.sFog.eMode == GL_LINEAR)
		{
			ui32FFTNLEnables1 |= FFTNL_ENABLES1_FOGLINEAR;
		}
		else if (gc->sState.sFog.eMode == GL_EXP)
		{
			ui32FFTNLEnables1 |= FFTNL_ENABLES1_FOGEXP;
		}
		else if (gc->sState.sFog.eMode == GL_EXP2)
		{
			ui32FFTNLEnables1 |= FFTNL_ENABLES1_FOGEXP2;
		}
	}
	
	/* Do we need eye position? */
	if ((psFFTNLGenDesc->uEnabledPointLocalLights)       ||
		(psFFTNLGenDesc->uEnabledSpotLocalLights)        ||
		((ui32FFTNLEnables1 & (FFTNL_ENABLES1_CLIPPING | FFTNL_ENABLES1_FOGCOORDEYEPOS | FFTNL_ENABLES1_VERTEXTOEYEVECTOR))) != 0)
	{
		ui32FFTNLEnables1 |= FFTNL_ENABLES1_EYEPOSITION;
	}

#if defined(FIX_HW_BRN_25211)
	if(gc->sAppHints.bUseC10Colours)
	{
		ui32FFTNLEnables2 |= FFTNL_ENABLES2_CLAMP_OUTPUT_COLOURS;
	}
#endif /* defined(FIX_HW_BRN_25211) */

	psFFTNLGenDesc->ui32FFTNLEnables1              = ui32FFTNLEnables1;
	psFFTNLGenDesc->ui32FFTNLEnables2              = ui32FFTNLEnables2;
	psFFTNLGenDesc->uSecAttrConstBaseAddressReg = 0;
	psFFTNLGenDesc->uSecAttrAllOther            = 1;
	psFFTNLGenDesc->uSecAttrStart               = GLES1_VERTEX_SECATTR_NUM_RESERVED;
	psFFTNLGenDesc->uSecAttrEnd                 = PVR_MAX_VS_SECONDARIES;
}


/***********************************************************************************
 Function Name      : SetupFFTNLShaderInputs
 Inputs             : psFFTNLProgram
 Outputs            : psShader->aui32InputRegMappings, 
 Returns            : None
 Description        : Gets a FFTNL input registers
************************************************************************************/
static IMG_VOID SetupFFTNLShaderInputs(GLES1Shader *psShader)
{
	FFGenRegList *psRegList = psShader->u.sFFTNL.psFFTNLProgram->psInputsList;
	VSInputReg *pasVSInputRegisters = &psShader->asVSInputRegisters[0];
	IMG_UINT32 ui32Attribute;

	/* Loop through list converting the registers */
	while (psRegList)
	{
		FFGenReg *psReg = psRegList->psReg;

		switch(psReg->eBindingRegDesc)
		{
			case FFGEN_INPUT_NORMAL:
			{
				ui32Attribute = AP_NORMAL;

				break;
			}
			case FFGEN_INPUT_VERTEX:
			{
				ui32Attribute = AP_VERTEX;

				break;
			}
			case FFGEN_INPUT_MULTITEXCOORD0:
			{
				ui32Attribute = AP_TEXCOORD0;

				break;
			}
			case FFGEN_INPUT_MULTITEXCOORD1:
			{
				ui32Attribute = AP_TEXCOORD1;

				break;
			}
			case FFGEN_INPUT_MULTITEXCOORD2:
			{
				ui32Attribute = AP_TEXCOORD2;

				break;
			}
			case FFGEN_INPUT_MULTITEXCOORD3:
			{
				ui32Attribute = AP_TEXCOORD3;

				break;
			}
			case FFGEN_INPUT_COLOR:
			{
				ui32Attribute = AP_COLOR;

				break;
			}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
			case FFGEN_INPUT_VERTEXBLENDWEIGHT:
			{
				ui32Attribute = AP_WEIGHTARRAY;

				break;
			}
			case FFGEN_INPUT_VERTEXMATRIXINDEX:
			{
				ui32Attribute = AP_MATRIXINDEX;

				break;
			}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
			case FFGEN_INPUT_POINTSIZE:
			{
				ui32Attribute = AP_POINTSIZE;

				break;
			}
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"SetupFFTNLShaderInputs: psReg->eBindingRegDesc = %x (Bad input)", psReg->eBindingRegDesc));

				continue;
			}
		}

		pasVSInputRegisters[ui32Attribute].ui32PrimaryAttribute = psReg->uOffset;
		pasVSInputRegisters[ui32Attribute].ui32Size 			= psReg->uSizeInDWords;

		psRegList = psRegList->psNext;
	}
}


#if defined(SGX545)
#define ATTRIB0_SHIFT		EURASIA_MTE_ATTRDIM0_SHIFT
#define ATTRIB_U			EURASIA_MTE_ATTRDIM_U
#define ATTRIB_UV			EURASIA_MTE_ATTRDIM_UV
#define ATTRIB_UVS			EURASIA_MTE_ATTRDIM_UVS
#define ATTRIB_UVT			EURASIA_MTE_ATTRDIM_UVT
#define ATTRIB_UVST			EURASIA_MTE_ATTRDIM_UVST

#else
#define ATTRIB0_SHIFT		EURASIA_MTE_TEXDIM0_SHIFT
#define ATTRIB_UV			EURASIA_MTE_TEXDIM_UV
#define ATTRIB_UVS			EURASIA_MTE_TEXDIM_UVS
#define ATTRIB_UVT			EURASIA_MTE_TEXDIM_UVT
#define ATTRIB_UVST			EURASIA_MTE_TEXDIM_UVST
#endif


/***********************************************************************************
 Function Name      : SetupFFTNLShaderOutputs
 Inputs             : psShader
 Outputs            : psShader->eOutputSelect, 
 Returns            : Texcoords have changed
 Description        : Gets a FFTNL output selects
************************************************************************************/
IMG_INTERNAL IMG_BOOL SetupFFTNLShaderOutputs(GLES1Context *gc)
{
	IMG_UINT32 i;
	GLES1Shader *psShader = gc->sProgram.psCurrentVertexShader;
	FFGenProgram	*psFFTNLProgram = psShader->u.sFFTNL.psFFTNLProgram;
	FFGenRegList	*psOutputsList  = psFFTNLProgram->psOutputsList;
	IMG_BOOL bTexCoordChanged = IMG_FALSE;
	IMG_UINT32 ui32TexCoordOffset = 0;
	IMG_UINT32 ui32OutputSelects, ui32VertexSize, ui32TexCoordSelects, ui32TexCoordPrecision;
#if defined(SGX545)
	IMG_UINT32 ui32FogFragOffset = 0;
#endif


	/* Vertex coordinates always present*/
	ui32VertexSize = 4;
	ui32OutputSelects = EURASIA_MTE_WPRESENT;

	ui32TexCoordSelects = 0;
	ui32TexCoordPrecision = 0;


	/* Setup output selection mask */
	while (psOutputsList)
	{
		switch (psOutputsList->psReg->eBindingRegDesc)
		{
			case FFGEN_OUTPUT_POSITION:
			{
				break;
			}
			case FFGEN_OUTPUT_FRONTCOLOR:
			{
#if defined(FIX_HW_BRN_25211)
				if(gc->sAppHints.bUseC10Colours)
				{
					if (gc->sPrim.eCurrentPrimitiveType == GLES1_PRIMTYPE_SPRITE)
					{
						/* Step past the point sprite coord set */
						ui32TexCoordOffset = 1;

						for (i=0; i<gc->ui32NumImageUnitsActive; i++)
						{
							IMG_UINT32 ui32Unit = gc->ui32TexImageUnitsEnabled[i];

							if (!gc->sState.sTexture.asUnit[ui32Unit].sEnv.bPointSpriteReplace)
							{
								ui32TexCoordOffset++;
							}
						}
					}
					else
					{
						ui32TexCoordOffset = gc->ui32NumImageUnitsActive;
					}

					ui32TexCoordSelects   |= (EURASIA_MTE_TEXDIM_UVST) << (ui32TexCoordOffset*3);
					ui32TexCoordPrecision |= (EURASIA_TATEXFLOAT_TC0_16B << ui32TexCoordOffset);

					ui32VertexSize += 4;
				}
				else
#endif /* defined(FIX_HW_BRN_25211) */
				{
					ui32OutputSelects |= EURASIA_MTE_BASE;
#if defined(SGX545)
					ui32TexCoordSelects |= (EURASIA_MTE_ATTRDIM_UVST << EURASIA_MTE_ATTRDIM0_SHIFT);

					ui32TexCoordOffset++;
					ui32FogFragOffset++;
#endif
					ui32VertexSize += 4;
				}

				break;
			}
			case FFGEN_OUTPUT_BACKCOLOR:
			{
#if defined(FIX_HW_BRN_25211)
				if(gc->sAppHints.bUseC10Colours)
				{
					if (gc->sPrim.eCurrentPrimitiveType == GLES1_PRIMTYPE_SPRITE)
					{
						/* Step past the point sprite coord set and the front colour */
						ui32TexCoordOffset = 2;

						for (i=0; i<gc->ui32NumImageUnitsActive; i++)
						{
							IMG_UINT32 ui32Unit = gc->ui32TexImageUnitsEnabled[i];

							if (!gc->sState.sTexture.asUnit[ui32Unit].sEnv.bPointSpriteReplace)
							{
								ui32TexCoordOffset++;
							}
						}
					}
					else
					{
						ui32TexCoordOffset = gc->ui32NumImageUnitsActive + 1;
					}

					ui32TexCoordSelects   |= (EURASIA_MTE_TEXDIM_UVST) << (ui32TexCoordOffset*3);
					ui32TexCoordPrecision |= (EURASIA_TATEXFLOAT_TC0_16B << ui32TexCoordOffset);

					ui32VertexSize += 4;
				}
				else
#endif /* defined(FIX_HW_BRN_25211) */
				{
					ui32OutputSelects |= EURASIA_MTE_OFFSET;
#if defined(SGX545)
					ui32TexCoordSelects |= (EURASIA_MTE_ATTRDIM_UVST << EURASIA_MTE_ATTRDIM1_SHIFT);

					ui32TexCoordOffset++;
					ui32FogFragOffset++;
#endif

					ui32VertexSize += 4;
				}
				break;
			}
			case FFGEN_OUTPUT_POINTSPRITE:
			{
			    ui32TexCoordSelects |=  (ATTRIB_UV << ((ATTRIB0_SHIFT + ui32TexCoordOffset) * 3));

#if defined(SGX545)
				ui32TexCoordOffset++;
				ui32FogFragOffset++;
#endif
				ui32VertexSize += 2;

				break;
			}
			case FFGEN_OUTPUT_TEXCOORD:
			{
				if (gc->sPrim.eCurrentPrimitiveType == GLES1_PRIMTYPE_SPRITE)
				{
					IMG_UINT32 j;

					j = 1;

					for (i = 0; i < gc->ui32NumImageUnitsActive; i++)
					{
						IMG_UINT32 ui32Unit = gc->ui32TexImageUnitsEnabled[i];

						if (!gc->sState.sTexture.asUnit[ui32Unit].sEnv.bPointSpriteReplace)
						{
							switch(psFFTNLProgram->auOutputTexDimensions[ui32Unit])
							{
#if defined(SGX545)
								case 1:
								{
								    ui32TexCoordSelects |= ATTRIB_U << (ATTRIB0_SHIFT + ((j + ui32TexCoordOffset) * 3));

									ui32VertexSize += 1;
								}
#endif
								case 2:
								{
								    ui32TexCoordSelects |= ATTRIB_UV << (ATTRIB0_SHIFT + ((j + ui32TexCoordOffset) * 3));

									ui32VertexSize += 2;

									break;
								}
								case 3:
								{
								    ui32TexCoordSelects |= ATTRIB_UVS << (ATTRIB0_SHIFT + ((j + ui32TexCoordOffset) * 3));

									ui32VertexSize += 3;

									break;
								}
								case 4:
								default:
								{
								    ui32TexCoordSelects |= ATTRIB_UVT << (ATTRIB0_SHIFT + ((j+ ui32TexCoordOffset) * 3));

									ui32VertexSize += 3;

									break;
								}
							}
#if defined(SGX545)
							ui32FogFragOffset++;
#endif

							j++;
						}
					}
				}
				else
				{
					for (i = 0; i < gc->ui32NumImageUnitsActive; i++)
					{
						switch(psFFTNLProgram->auOutputTexDimensions[gc->ui32TexImageUnitsEnabled[i]])
						{
#if defined(SGX545)
						    case 1:
							{
							    ui32TexCoordSelects |= ATTRIB_U << (ATTRIB0_SHIFT + ((i + ui32TexCoordOffset) * 3));

								ui32VertexSize += 1;
							}
#endif
							case 2:
							{
							    ui32TexCoordSelects |= ATTRIB_UV << (ATTRIB0_SHIFT + ((i + ui32TexCoordOffset) * 3));

								ui32VertexSize += 2;

								break;
							}
							case 3:
							{
							    ui32TexCoordSelects |= ATTRIB_UVS << (ATTRIB0_SHIFT + ((i + ui32TexCoordOffset) * 3));

								ui32VertexSize += 3;

								break;
							}
							case 4:
							default:
							{
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
								if(gc->sTexture.aui32CurrentTarget[gc->ui32TexImageUnitsEnabled[i]] == GLES1_TEXTURE_TARGET_CEM)
								{
									ui32TexCoordSelects |= ATTRIB_UVS << (ATTRIB0_SHIFT + ((i + ui32TexCoordOffset) * 3));
								}
								else
#endif
								{
									ui32TexCoordSelects |= ATTRIB_UVT << (ATTRIB0_SHIFT + ((i + ui32TexCoordOffset) * 3));
								}

								ui32VertexSize += 3;

								break;
							}
						}
#if defined(SGX545)
						ui32FogFragOffset++;
#endif
					}
				}

				break;
			}
			case FFGEN_OUTPUT_POINTSIZE:
			{
				ui32OutputSelects |= EURASIA_MTE_SIZE;

				ui32VertexSize += 1;

				break;
			}
			case FFGEN_OUTPUT_FOGFRAGCOORD:
			{
#if defined(SGX545)
				ui32TexCoordSelects |= ATTRIB_U << (ATTRIB0_SHIFT + (ui32FogFragOffset * 3));
#else 
				ui32OutputSelects |= EURASIA_MTE_FOG;
#endif /* defined(SGX545) */

#if defined(SGX_FEATURE_USE_VEC34)
				ui32VertexSize += 2;
#else
				ui32VertexSize += 1;
#endif /* defined(SGX_FEATURE_USE_VEC34) */

				break;
			}
			case FFGEN_OUTPUT_CLIPVERTEX:
			{
				IMG_UINT32 ui32NumClipPlanes;
				IMG_UINT32 ui32ClipPlanesMask = (gc->ui32TnLEnables & GLES1_TL_CLIP_PLANES_ENABLE) >> GLES1_TL_CLIP_PLANE0_SHIFT;
	
				ui32NumClipPlanes = 0;

				while(ui32ClipPlanesMask)
				{
					if(ui32ClipPlanesMask & 1)
					{
						ui32OutputSelects |= (EURASIA_MTE_PLANE0 << ui32NumClipPlanes);

						ui32VertexSize++;

						ui32NumClipPlanes++;
					}

					ui32ClipPlanesMask >>= 1;
				}

				break;
			}
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"SetupFFTNLShaderOutputs: psOutputsList->psReg->eBindingRegDesc = %x (Bad output)", psOutputsList->psReg->eBindingRegDesc));

				break;
			}
		}

		psOutputsList = psOutputsList->psNext;
	}

	ui32OutputSelects |= ui32VertexSize << EURASIA_MTE_VTXSIZE_SHIFT;

#if defined(SGX545)
	ui32OutputSelects |= EURASIA_MTE_CLIPMODE_FRONT_AND_REAR;
#endif

	psShader->ui32OutputSelects = ui32OutputSelects;
	psShader->ui32TexCoordPrecision = ui32TexCoordPrecision;
	psShader->ui32TexCoordSelects = ui32TexCoordSelects;
	
	if(gc->sPrim.ui32CurrentTexCoordSelects != ui32TexCoordSelects)
	{
		gc->sPrim.ui32CurrentTexCoordSelects = ui32TexCoordSelects;

		bTexCoordChanged = IMG_TRUE;
	}

	return bTexCoordChanged;
}	


/***********************************************************************************
 Function Name      : UpdateFFTNLShaderConstantsSize
 Inputs             : psFFTNLProgram
 Outputs            : psShader->sConstantData.pfConstantData
 Returns            : None
 Description        : Updates buffer for new set of constants
************************************************************************************/
static PVRSRV_ERROR UpdateFFTNLShaderConstantsSize(GLES1Context *gc, GLES1Shader *psShader)
{
	FFGenProgram *psFFTNLProgram = psShader->u.sFFTNL.psFFTNLProgram;

#if defined(FFGEN_UNIFLEX)
	IMG_UINT32 ui32ConstantsSize = psFFTNLProgram->uMemoryConstantsSize + psFFTNLProgram->uSecAttribSize + psFFTNLProgram->ui32NumUFConstants;
#else
	IMG_UINT32 ui32ConstantsSize = psFFTNLProgram->uMemoryConstantsSize + psFFTNLProgram->uSecAttribSize;
#endif

	PVR_UNREFERENCED_PARAMETER(gc);

	/* Set up constant data */
	if(ui32ConstantsSize)
	{
		/* constants buffer extists, reallloc and update size */
		if(psShader->ui32SizeOfConstants != ui32ConstantsSize)  
		{
			IMG_FLOAT *pfNewConstantData = GLES1Realloc(gc, psShader->pfConstantData,
														ui32ConstantsSize * sizeof(IMG_FLOAT));

			if(!pfNewConstantData)
			{
				PVR_DPF((PVR_DBG_FATAL,"UpdateFFTNLShaderConstantsSize: Failed to allocate constant data"));

				return PVRSRV_ERROR_OUT_OF_MEMORY;
			}

			psShader->pfConstantData = pfNewConstantData;
			psShader->ui32SizeOfConstants = ui32ConstantsSize;	
		}
	}

	return PVRSRV_OK;
}


#if defined(FFGEN_UNIFLEX)
/***********************************************************************************
 Function Name      : SetupBuildFFTNLShaderConstants
 Inputs             : psFFTNLProgram
 Outputs            : psShader->sConstantData.pfConstantData
 Returns            : None
 Description        : Setups and builds a FFTNL shader constants
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupBuildFFTNLShaderConstants(GLES1Context *gc)
{
	IMG_FLOAT *pfConstantBase, *pfTempMatrix;	
	FFGenProgram *psFFTNLProgram;
	FFGenRegList *psRegList;
	GLESMatrix sTempMatrix;
	GLES1Shader *psShader;
	IMG_UINT32 i;
	
	psShader = gc->sProgram.psCurrentVertexShader;

	psFFTNLProgram = psShader->u.sFFTNL.psFFTNLProgram;

	pfTempMatrix =  &sTempMatrix.afMatrix[0][0];

	/* update constants buffer size */
	if(UpdateFFTNLShaderConstantsSize(gc, psShader) != PVRSRV_OK)
	{
		return;
	}

#if 0
{
	IMG_UINT32 *pui32Data = (IMG_UINT32 *)psShader->pfConstantData;

	for (i=0;i<psShader->ui32SizeOfConstants;i++)
	{
		pui32Data[i] = 0xDEADBEEF;
	}
}	
#endif

	pfConstantBase = psShader->pfConstantData;

	psRegList = psFFTNLProgram->psConstantsList;

	/* Loop through list converting the registers */
	while (psRegList)
	{
		FFGenReg *psReg = psRegList->psReg;

		/* Determine properties */
		switch (psReg->eBindingRegDesc)
		{
			case FFGEN_STATE_MODELVIEWMATRIX:
			{
				IMG_FLOAT *pfConstant;

				GLES1_ASSERT(psReg->uSizeInDWords <= FFTNL_SIZE_MATRIX_4X4);

				TransposeMatrix(&sTempMatrix, &gc->sTransform.psModelView->sMatrix);

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, pfTempMatrix[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_TEXTUREMATRIX:
			{
				IMG_FLOAT *pfConstant;
				IMG_FLOAT afTemp[GLES1_MAX_TEXTURE_UNITS * FFTNL_SIZE_MATRIX_4X4];

				GLES1_ASSERT((psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4) <= GLES1_MAX_TEXTURE_UNITS);

				pfConstant = &afTemp[0];

				for(i = 0; i < (psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4 ); i++)
				{
					TransposeMatrix(&sTempMatrix, &gc->sTransform.apsTexture[i]->sMatrix);

					COPY_MATRIX(pfConstant, pfTempMatrix, FFTNL_SIZE_MATRIX_4X4);

					pfConstant += FFTNL_SIZE_MATRIX_4X4;
				}

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];


					COPY_FLOAT(pfConstant, afTemp[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSE:
			{
				IMG_FLOAT *pfConstant;

				GLES1_ASSERT(psReg->uSizeInDWords <= FFTNL_SIZE_MATRIX_4X4);

				if(gc->sTransform.psModelView->bUpdateInverse)
				{
					(*gc->sProcs.pfnComputeInverseTranspose)(gc, &gc->sTransform.psModelView[0]);
				}

				TransposeMatrix(&sTempMatrix, &gc->sTransform.psModelView->sInverseTranspose);

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];


					COPY_FLOAT(pfConstant, pfTempMatrix[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_PROJECTMATRIX:
			{
				IMG_FLOAT *pfConstant;

				GLES1_ASSERT(psReg->uSizeInDWords <= FFTNL_SIZE_MATRIX_4X4);

				TransposeMatrix(&sTempMatrix, &gc->sTransform.psProjection->sMatrix);

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, pfTempMatrix[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_CLIPPLANE:
			{
				IMG_FLOAT *pfConstant;

				IMG_FLOAT afTemp[FFTNL_SIZE_CLIPPLANE*GLES1_MAX_CLIP_PLANES];

				GLES1_ASSERT((psReg->uSizeInDWords / FFTNL_SIZE_CLIPPLANE) <= GLES1_MAX_CLIP_PLANES);

				pfConstant = &afTemp[0];

				for(i = 0; i < (psReg->uSizeInDWords / FFTNL_SIZE_CLIPPLANE); i++)
				{
					COPY_COORD4(pfConstant, gc->sTransform.asEyeClipPlane[i]);

					pfConstant += FFTNL_SIZE_CLIPPLANE;
				}

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, afTemp[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_MODELVIEWPROJECTIONMATRIX:
			{
				IMG_FLOAT *pfConstant;

				GLES1_ASSERT(psReg->uSizeInDWords <= FFTNL_SIZE_MATRIX_4X4);

				TransposeMatrix(&sTempMatrix, &gc->sTransform.psModelView->sMvp);

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, pfTempMatrix[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_POINT:
			{
				IMG_FLOAT *pfConstant;

				GLES1_ASSERT(psReg->uSizeInDWords == FFTNL_SIZE_POINTPARAMS);

				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_SIZE] = gc->sState.sPoint.fRequestedSize;
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_MIN] = *gc->sState.sPoint.pfMinPointSize;
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_MAX] = *gc->sState.sPoint.pfMaxPointSize;
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_FADETHRESH] = gc->sState.sPoint.fFade;
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_CONATTEN] = gc->sState.sPoint.afAttenuation[0];
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_LINATTEN] = gc->sState.sPoint.afAttenuation[1];
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_QUADATTEN] = gc->sState.sPoint.afAttenuation[2];

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, pfTempMatrix[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_LIGHTSOURCE0:
			case FFGEN_STATE_LIGHTSOURCE1:
			case FFGEN_STATE_LIGHTSOURCE2:
			case FFGEN_STATE_LIGHTSOURCE3:
			case FFGEN_STATE_LIGHTSOURCE4:
			case FFGEN_STATE_LIGHTSOURCE5:
			case FFGEN_STATE_LIGHTSOURCE6:
			case FFGEN_STATE_LIGHTSOURCE7:
			{
				IMG_FLOAT *pfConstant;
				GLEScoord sCoordTemp;
				GLESLightSourceState *psLss;
				IMG_UINT32 ui32LightNum;
				IMG_FLOAT afTemp[FFTNL_SIZE_LIGHTSOURCE];

				GLES1_ASSERT(psReg->uSizeInDWords==FFTNL_SIZE_LIGHTSOURCE);

				pfConstant = &afTemp[0];
			
				ui32LightNum = (IMG_UINT32)(psReg->eBindingRegDesc - FFGEN_STATE_LIGHTSOURCE0);

				psLss = &gc->sState.sLight.psSource[ui32LightNum];	

				/* ambient */
				COPY_COLOR4(pfConstant,  psLss->sAmbient);
				pfConstant+= auLightSourceMemberSizes[0];
				/* vec4 diffuse; */
				COPY_COLOR4(pfConstant, psLss->sDiffuse);
				pfConstant+= auLightSourceMemberSizes[1];

				/* vec4 specular;  */
				COPY_COLOR4(pfConstant, psLss->sSpecular);
				pfConstant+= auLightSourceMemberSizes[2];

				/* vec4 position */
				sCoordTemp = psLss->sPositionEye;

				if(sCoordTemp.fW)
				{
					sCoordTemp.fW = 1.0f / sCoordTemp.fW;
					sCoordTemp.fX *= sCoordTemp.fW;
					sCoordTemp.fY *= sCoordTemp.fW;
					sCoordTemp.fZ *= sCoordTemp.fW;
				}
				sCoordTemp.fW = 1.0f;

				COPY_COORD4(pfConstant, sCoordTemp);
				pfConstant+= auLightSourceMemberSizes[3];

				/* vec4 positionNormalised */
				sCoordTemp = psLss->sPositionEye;
				sCoordTemp.fW = (IMG_FLOAT) sqrt(sCoordTemp.fX * sCoordTemp.fX + sCoordTemp.fY * sCoordTemp.fY + sCoordTemp.fZ * sCoordTemp.fZ);
				if(sCoordTemp.fW)
				{
					sCoordTemp.fW = 1.0f / sCoordTemp.fW;
					sCoordTemp.fX *= sCoordTemp.fW;
					sCoordTemp.fY *= sCoordTemp.fW;
					sCoordTemp.fZ *= sCoordTemp.fW;
				}
				sCoordTemp.fW = 1.0f;

				COPY_COORD4(pfConstant, sCoordTemp);
				pfConstant+= auLightSourceMemberSizes[4];

				 /* vec4 halfVector;  */
				sCoordTemp = psLss->sPositionEye;
				sCoordTemp.fZ += 1.0f;
				sCoordTemp.fW =	(IMG_FLOAT) (1.0f / (IMG_FLOAT) sqrt(sCoordTemp.fX * sCoordTemp.fX +
											sCoordTemp.fY * sCoordTemp.fY +
											sCoordTemp.fZ * sCoordTemp.fZ));
				sCoordTemp.fX *= sCoordTemp.fW;
				sCoordTemp.fY *= sCoordTemp.fW;
				sCoordTemp.fZ *= sCoordTemp.fW;
				sCoordTemp.fW = 1.0f;

				COPY_COORD4(pfConstant, sCoordTemp);
				pfConstant+= auLightSourceMemberSizes[5];

				/* vec3 spotDirection; */
				COPY_COORD3(pfConstant, psLss->sDirection);
				pfConstant+= auLightSourceMemberSizes[6];

				/* spotExponent;*/
				COPY_FLOAT(pfConstant, psLss->fSpotLightExponent);
				pfConstant+= auLightSourceMemberSizes[7];

				/* float constantAttenuation */
				COPY_FLOAT(pfConstant, psLss->afAttenuation[0]);
				pfConstant+= auLightSourceMemberSizes[10];

				/* float linearAttenuation */
				COPY_FLOAT(pfConstant, psLss->afAttenuation[1]);
				pfConstant+= auLightSourceMemberSizes[11];

				/* float quadraticAttenuation; */
				COPY_FLOAT(pfConstant, psLss->afAttenuation[2]);
				pfConstant+= auLightSourceMemberSizes[12];
			
				/* spotCutoff */
				COPY_FLOAT(pfConstant, psLss->fSpotLightCutOffAngle);
				pfConstant+= auLightSourceMemberSizes[8];

				/* float spotCosCutoff */
				pfTempMatrix[0] = (IMG_FLOAT) GLES1_COSF(psLss->fSpotLightCutOffAngle * GLES1_DegreesToRadians);
				COPY_FLOAT(pfConstant, pfTempMatrix[0]);

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, afTemp[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_FRONTLIGHTPRODUCT:
			case FFGEN_STATE_BACKLIGHTPRODUCT:
			{
				IMG_FLOAT *pfConstant;
				GLESLightSourceState *psLss;
				GLESMaterialState *psMaterial = &gc->sState.sLight.sMaterial;
				IMG_FLOAT afTemp[FFTNL_SIZE_LIGHTPRODUCT*GLES1_MAX_LIGHTS];

				GLES1_ASSERT((psReg->uSizeInDWords / FFTNL_SIZE_LIGHTPRODUCT) <= GLES1_MAX_LIGHTS);

				pfConstant = &afTemp[0];

				for(i = 0; i < (psReg->uSizeInDWords / FFTNL_SIZE_LIGHTPRODUCT); i++)
				{
					psLss = &gc->sState.sLight.psSource[i];

					/* ambient */
					pfConstant[0] = psMaterial->sAmbient.fRed * psLss->sAmbient.fRed;
					pfConstant[1] = psMaterial->sAmbient.fGreen * psLss->sAmbient.fGreen;
					pfConstant[2] = psMaterial->sAmbient.fBlue * psLss->sAmbient.fBlue;
					pfConstant[3] = psMaterial->sAmbient.fAlpha;
					pfConstant += auLightProductMemberSizes[0];

					/* diffuse */
					pfConstant[0] = psMaterial->sDiffuse.fRed * psLss->sDiffuse.fRed;
					pfConstant[1] = psMaterial->sDiffuse.fGreen * psLss->sDiffuse.fGreen;
					pfConstant[2] = psMaterial->sDiffuse.fBlue * psLss->sDiffuse.fBlue;
					pfConstant[3] = psMaterial->sDiffuse.fAlpha;
					pfConstant += auLightProductMemberSizes[1];

					/* specular */
					pfConstant[0] = psMaterial->sSpecular.fRed * psLss->sSpecular.fRed;
					pfConstant[1] = psMaterial->sSpecular.fGreen * psLss->sSpecular.fGreen;
					pfConstant[2] = psMaterial->sSpecular.fBlue * psLss->sSpecular.fBlue;
					pfConstant[3] = psMaterial->sSpecular.fAlpha;
					pfConstant += auLightProductMemberSizes[2];
				}

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, afTemp[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_FRONTMATERIAL:
			case FFGEN_STATE_BACKMATERIAL:
			{
				IMG_FLOAT *pfConstant;
				GLESMaterialState *psMaterial = &gc->sState.sLight.sMaterial;
				IMG_FLOAT afTemp[FFTNL_SIZE_MATERIAL];

				GLES1_ASSERT(psReg->uSizeInDWords == FFTNL_SIZE_MATERIAL);

				pfConstant = &afTemp[0];

				COPY_COLOR4(pfConstant, psMaterial->sEmissive);
				pfConstant+= auMaterialMemberSizes[0];

				COPY_COLOR4(pfConstant, psMaterial->sAmbient);
				pfConstant+= auMaterialMemberSizes[1];

				COPY_COLOR4(pfConstant, psMaterial->sDiffuse);
				pfConstant+= auMaterialMemberSizes[2];

				COPY_COLOR4(pfConstant, psMaterial->sSpecular);
				pfConstant+= auMaterialMemberSizes[3];

				COPY_FLOAT(pfConstant,  psMaterial->sSpecularExponent.fX);

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, afTemp[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_LIGHTMODEL:
			{
				IMG_FLOAT *pfConstant;
				IMG_FLOAT afTemp[FFTNL_SIZE_LIGHTTMODEL];

				GLES1_ASSERT(psReg->uSizeInDWords == FFTNL_SIZE_LIGHTTMODEL);

				pfConstant = &afTemp[0];

				COPY_COLOR4(pfConstant, gc->sState.sLight.sModel.sAmbient);

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, afTemp[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_FRONTLIGHTMODELPRODUCT:
			case FFGEN_STATE_BACKLIGHTMODELPRODUCT:
			{
				IMG_FLOAT *pfConstant;
				GLESMaterialState *psMaterial = &gc->sState.sLight.sMaterial;
				GLESLightModelState *psModel = &gc->sState.sLight.sModel;
				IMG_FLOAT afTemp[FFTNL_SIZE_LIGHTTMODELPRODUCT];

				GLES1_ASSERT(psReg->uSizeInDWords == FFTNL_SIZE_LIGHTTMODELPRODUCT);

				pfConstant = &afTemp[0];

				pfConstant[0] = (psMaterial->sAmbient.fRed * psModel->sAmbient.fRed) + psMaterial->sEmissive.fRed;
				pfConstant[1] = (psMaterial->sAmbient.fGreen * psModel->sAmbient.fGreen) + psMaterial->sEmissive.fGreen;
				pfConstant[2] = (psMaterial->sAmbient.fBlue * psModel->sAmbient.fBlue) + psMaterial->sEmissive.fBlue;
				pfConstant[3] =  psMaterial->sAmbient.fAlpha;

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, afTemp[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_PMXFOGPARAM:
			{
				IMG_FLOAT *pfConstant;
				IMG_FLOAT afTemp[4];

				GLES1_ASSERT(psReg->uSizeInDWords == 4);

				pfConstant = &afTemp[0];

				pfConstant[0] = gc->sState.sFog.fDensity * GLES1_ONE_OVER_LN_TWO;
				pfConstant[1] = gc->sState.sFog.fDensity * GLES1_ONE_OVER_SQRT_LN_TWO;
				pfConstant[2] = -gc->sState.sFog.fOneOverEMinusS;
				pfConstant[3] = gc->sState.sFog.fEnd * gc->sState.sFog.fOneOverEMinusS;

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, afTemp[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_PMXPOINTSIZE:
			{
				IMG_FLOAT *pfConstant;

				GLES1_ASSERT(psReg->uSizeInDWords == 1);

				pfConstant = pfConstantBase + psReg->pui32DstOffset[0];

				COPY_FLOAT(pfConstant, *gc->sState.sPoint.pfPointSize);

				break;
			}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
			case FFGEN_STATE_MATRIXPALETTEINDEXCLAMP:
			{
				IMG_FLOAT *pfConstant;

				GLES1_ASSERT(psReg->uSizeInDWords == 1);

				pfConstant = pfConstantBase + psReg->pui32DstOffset[0];

				COPY_FLOAT(pfConstant, (IMG_FLOAT)(NUM_PALETTE_ENTRIES_MIN - 1));

				break;
			}
			case FFGEN_STATE_MODELVIEWMATRIXPALETTE:
			{
				IMG_FLOAT *pfConstant;
				IMG_FLOAT afTemp[GLES1_MAX_PALETTE_MATRICES * FFTNL_SIZE_MATRIX_4X4];

				GLES1_ASSERT((psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4) <= GLES1_MAX_PALETTE_MATRICES);

				pfConstant = &afTemp[0];

				for(i=0; i<(psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4); i++)
				{
					TransposeMatrix(&sTempMatrix, &gc->sTransform.psMatrixPalette[i].sMatrix);

					COPY_MATRIX(pfConstant, pfTempMatrix, FFTNL_SIZE_MATRIX_4X4);

					pfConstant += FFTNL_SIZE_MATRIX_4X4;
				}

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, afTemp[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
			case FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSEPALETTE:
			{
				IMG_FLOAT *pfConstant;
				IMG_FLOAT afTemp[GLES1_MAX_PALETTE_MATRICES * FFTNL_SIZE_MATRIX_4X4];

				GLES1_ASSERT((psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4) <= GLES1_MAX_PALETTE_MATRICES);

				pfConstant = &afTemp[0];

				for(i=0; i<(psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4); i++)
				{
					if(gc->sTransform.psMatrixPalette[i].bUpdateInverse)
					{
						(*gc->sProcs.pfnComputeInverseTranspose)(gc, &gc->sTransform.psMatrixPalette[i]);
					}

					TransposeMatrix(&sTempMatrix, &gc->sTransform.psMatrixPalette[i].sInverseTranspose);

					COPY_MATRIX(pfConstant, pfTempMatrix, FFTNL_SIZE_MATRIX_4X4);

					pfConstant += FFTNL_SIZE_MATRIX_4X4;
				}

				for(i = 0; i < psReg->ui32ConstantCount; i++)
				{
					pfConstant = pfConstantBase + psReg->pui32DstOffset[i];

					COPY_FLOAT(pfConstant, afTemp[psReg->pui32SrcOffset[i]]);
				}

				break;
			}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"SetupBuildFFTNLShaderConstants: psReg->eBindingRegDesc = %x (Bad state)",psReg->eBindingRegDesc));

				break;
			}
		}

		psRegList = psRegList->psNext;
	}


	/* Load the constants generated by UniFlex */
	for(i=0;i<psFFTNLProgram->ui32NumUFConstants;i++)
	{
		IMG_UINT32 *pui32Constant = (IMG_UINT32 *)psShader->pfConstantData + psFFTNLProgram->pui32UFConstantDest[i];

		*pui32Constant = psFFTNLProgram->pui32UFConstantData[i];
	}
}




/***********************************************************************************
 Function Name      : CompileFFGenUniFlexCode
 Inputs             : gc, psFFTNLProgram
 Outputs            : 
 Returns            : 
 Description        : Compiles USC FFGen code
************************************************************************************/
static IMG_BOOL CompileFFGenUniFlexCode(GLES1Context *gc, FFGenProgram *psFFTNLProgram)
{
	FFGEN_PROGRAM_DETAILS *psFFGENProgramDetails;
	UNIFLEX_PROGRAM_PARAMETERS sProgramParameters;
	IMG_UINT32 ui32Error;
	SGX_CORE_INFO sTarget;
	UNIFLEX_HW sUniFlexHW={0};
	UNIFLEX_CONSTDEF sConstants;
	UNIFLEX_INST *psUniFlexInstruction;
	UNIFLEX_RANGE sConstsRanges[20];
	IMG_UINT32 ui32NumSecAttribs, ui32NumMemConsts, i;
	FFGenRegList *psRegList;

	GLES1MemSet(&sProgramParameters, 0, sizeof(UNIFLEX_PROGRAM_PARAMETERS));

	sTarget.eID = SGX_CORE_ID;

#if defined(SGX_CORE_REV)
	sTarget.uiRev = SGX_CORE_REV;
#else
	sTarget.uiRev = 0;		/* use head revision */
#endif

	psFFGENProgramDetails = psFFTNLProgram->psFFGENProgramDetails;

	sProgramParameters.eShaderType					= USC_SHADERTYPE_VERTEX;
	sProgramParameters.uInRegisterConstantOffset	= GLES1_VERTEX_SECATTR_NUM_RESERVED;
	sProgramParameters.uInRegisterConstantLimit		= PVR_MAX_VS_SECONDARIES - GLES1_VERTEX_SECATTR_NUM_RESERVED;

	sProgramParameters.uIndexableTempBase			= 1;
	sProgramParameters.psIndexableTempArraySizes	= IMG_NULL;
	sProgramParameters.uPackDestType				= USEASM_REGTYPE_OUTPUT;
	sProgramParameters.uNumAvailableTemporaries		= 96;
	sProgramParameters.sTarget						= sTarget;
	sProgramParameters.puValidShaderOutputs[0]		= 0xFFFFFFFF;
	sProgramParameters.puValidShaderOutputs[1]		= 0xFFFFFFFF;
	sProgramParameters.puValidShaderOutputs[2]		= 0xFFFFFFFF;
	sProgramParameters.puValidShaderOutputs[3]		= 0xFFFFFFFF;


	psRegList = psFFTNLProgram->psOutputsList;

	/* Setup output selection mask. Find (u,v,s,t) and collapse to (u,v,s) or (u,v,t).
	   Improves parameter buffer usage and works around HW BRN 33583.
	 */
	while (psRegList)
	{
		if(psRegList->psReg->eBindingRegDesc == FFGEN_OUTPUT_TEXCOORD)
		{
			IMG_UINT32 ui32TexCoordOffset = psRegList->psReg->uOffset;

			for (i = 0; i < gc->ui32NumImageUnitsActive; i++)
			{
				if(psFFTNLProgram->auOutputTexDimensions[gc->ui32TexImageUnitsEnabled[i]] == 4)
				{
					IMG_UINT32 ui32OutputIndex = ui32TexCoordOffset / 32;
					IMG_UINT32 ui32OutputBit = 2 + (ui32TexCoordOffset % 32);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
					/* For cubemaps, turn uvst into uvs */
					if(gc->sTexture.aui32CurrentTarget[gc->ui32TexImageUnitsEnabled[i]] == GLES1_TEXTURE_TARGET_CEM)
					{
						ui32OutputBit++;
					}
#endif

					sProgramParameters.puValidShaderOutputs[ui32OutputIndex] &= ~(1 << ui32OutputBit);
				}
				ui32TexCoordOffset += psFFTNLProgram->auOutputTexDimensions[gc->ui32TexImageUnitsEnabled[i]]; 
			}
		}
		psRegList = psRegList->psNext;
	}

	sProgramParameters.uFlags = UF_GLSL |
								UF_DONTRESETMOEAFTERPROGRAM |
								UF_VSOUTPUTSALWAYSVALID;
	
	sProgramParameters.uFlags2 = 0;


#if defined(DEBUG)
	if(!gc->sAppHints.bDumpShaders)
#endif /* defined(DEBUG) */
	{
		sProgramParameters.uFlags |= UF_QUIET;
	}

	sConstants.uCount = 0;
	sConstants.puConstStaticFlags = IMG_NULL;
	sConstants.pfConst = IMG_NULL;

	psRegList = psFFTNLProgram->psConstantsList;

	sProgramParameters.asConstBuffDesc[UF_CONSTBUFFERID_LEGACY].uStartingSAReg = 0;
	sProgramParameters.asConstBuffDesc[UF_CONSTBUFFERID_LEGACY].uBaseAddressSAReg = 0;
	sProgramParameters.asConstBuffDesc[UF_CONSTBUFFERID_LEGACY].uRelativeConstantMax = 0;		
	sProgramParameters.asConstBuffDesc[UF_CONSTBUFFERID_LEGACY].eConstBuffLocation = UF_CONSTBUFFERLOCATION_DONTCARE;

	sProgramParameters.asConstBuffDesc[UF_CONSTBUFFERID_LEGACY].sConstsBuffRanges.uRangesCount = 0;
	sProgramParameters.asConstBuffDesc[UF_CONSTBUFFERID_LEGACY].sConstsBuffRanges.psRanges = &sConstsRanges[0];

	while (psRegList)
	{
		FFGenReg *psReg = psRegList->psReg;
		
		if((psReg->eBindingRegDesc == FFGEN_STATE_MODELVIEWMATRIXPALETTE) ||
		   (psReg->eBindingRegDesc == FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSEPALETTE))
		{
			sConstsRanges[sProgramParameters.asConstBuffDesc[UF_CONSTBUFFERID_LEGACY].sConstsBuffRanges.uRangesCount].uRangeStart = psReg->uOffset/4;
			sConstsRanges[sProgramParameters.asConstBuffDesc[UF_CONSTBUFFERID_LEGACY].sConstsBuffRanges.uRangesCount].uRangeEnd = psReg->uOffset/4 + (psReg->uSizeInDWords+3)/4;

			sProgramParameters.asConstBuffDesc[UF_CONSTBUFFERID_LEGACY].sConstsBuffRanges.uRangesCount++;

			sProgramParameters.uFlags |= UF_CONSTRANGES;
		}
		
		psRegList = psRegList->psNext;
	}

	/* Reduce instruction movement for matrix palette, to ensure large numbers of 
	 * temps aren't used to hold matrices for too long
	 */
	if(sProgramParameters.uFlags & UF_CONSTRANGES)
	{
		sProgramParameters.uMaxInstMovement = 7;
	}
	else
	{
		sProgramParameters.uMaxInstMovement = 15;
	}


	/* No shader input ranges. */
	sProgramParameters.sShaderInputRanges.uRangesCount = 0;

	/* Reserved the default number of PDS constants. */
	sProgramParameters.uNumPDSPrimaryConstantsReserved = EURASIA_PDS_DOUTU_NONLOOPBACK_STATE_SIZE;

	ui32Error = PVRUniFlexCompileToHw((IMG_VOID*)gc->sProgram.hUniFlexContext,
									  psFFTNLProgram->psUniFlexInst,
									  &sConstants,
									  &sProgramParameters,
									  &sUniFlexHW);

#if defined(DEBUG)
	if(gc->sAppHints.bDumpShaders)
	{
#if 0
		UseDisassembler(UseAsmGetCoreDesc(&sTarget), sUniFlexHW.uInstructionCount, sUniFlexHW.puInstructions);
#else
		if(fOutputFile)
		{
			for(i=0; i<sUniFlexHW.uInstructionCount<<1; i+=2)
			{
				IMG_CHAR pszInst[256];

				UseDisassembleInstruction(UseAsmGetCoreDesc(&sTarget), sUniFlexHW.puInstructions[i], sUniFlexHW.puInstructions[i+1], pszInst);

				fprintf(fOutputFile,"%s\n", pszInst);
			}

			fprintf(fOutputFile,"\n-------------------------------------\n\n");
		}

#endif
	}
#endif /* defined(DEBUG) */

	psUniFlexInstruction = psFFTNLProgram->psUniFlexInst;

	while(psUniFlexInstruction)
	{
		UNIFLEX_INST *psInstructionToFree;

		psInstructionToFree  = psUniFlexInstruction;
		psUniFlexInstruction = psUniFlexInstruction->psILink;

		GLES1Free(IMG_NULL, psInstructionToFree);
	}

	psFFTNLProgram->psUniFlexInst = IMG_NULL;

	if(ui32Error != UF_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "CompileFFGenUniFlexCode: PVRUniFlexCompileToHw failed with error (%d)\n", ui32Error));

		PVRCleanupUniflexHw((IMG_VOID*)gc->sProgram.hUniFlexContext, &sUniFlexHW);

		return IMG_FALSE;
	}

	if(sUniFlexHW.uSpillAreaSize!=0)
	{
		PVR_DPF((PVR_DBG_ERROR, "CompileFFGenUniFlexCode: PVRUniFlexCompileToHw uSpillAreaSize != 0\n"));

		PVRCleanupUniflexHw((IMG_VOID*)gc->sProgram.hUniFlexContext, &sUniFlexHW);

		return IMG_FALSE;
	}

	if (sUniFlexHW.uFlags & UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE)
	{
		psFFGENProgramDetails->bUSEPerInstanceMode = IMG_TRUE;
	}

	/* Setup compiled code */
	psFFGENProgramDetails->ui32InstructionCount	= sUniFlexHW.uInstructionCount;
	psFFGENProgramDetails->pui32Instructions	= GLES1Malloc(gc, sUniFlexHW.uInstructionCount * EURASIA_USE_INSTRUCTION_SIZE);
	
	if(!psFFGENProgramDetails->pui32Instructions)
	{
		PVR_DPF((PVR_DBG_ERROR, "CompileFFGenUniFlexCode: Failed to allocate host memory for USSE code"));

		PVRCleanupUniflexHw((IMG_VOID*)gc->sProgram.hUniFlexContext, &sUniFlexHW);

		return IMG_FALSE;
	}

	GLES1MemCopy(psFFGENProgramDetails->pui32Instructions, sUniFlexHW.puInstructions, sUniFlexHW.uInstructionCount * EURASIA_USE_INSTRUCTION_SIZE);

	PVR_ASSERT(sUniFlexHW.uPhaseCount == 1);
	PVR_ASSERT(sUniFlexHW.asPhaseInfo[0].uPhaseStart==0);
	PVR_ASSERT(sUniFlexHW.uSAProgInstructionCount==0);

	psFFGENProgramDetails->ui32PrimaryAttributeCount    = sUniFlexHW.uPrimaryAttributeCount;
	psFFGENProgramDetails->ui32TemporaryRegisterCount   = sUniFlexHW.asPhaseInfo[0].uTemporaryRegisterCount;
	psFFGENProgramDetails->iSAAddressAdjust				= (IMG_INT16)(sUniFlexHW.iSAAddressAdjust);



	psFFTNLProgram->pui32UFConstantData = IMG_NULL;
	psFFTNLProgram->pui32UFConstantDest = IMG_NULL;
	psFFTNLProgram->ui32NumUFConstants = 0;
	psFFTNLProgram->ui32MaxNumUFConstants = 0;

	ui32NumSecAttribs = 0;

	psRegList = psFFTNLProgram->psConstantsList;

	while (psRegList)
	{
		FFGenReg *psReg = psRegList->psReg;
		
		IMG_UINT32 ui32ConstantCount;

		ui32ConstantCount = 0;

		for(i=0;i<sUniFlexHW.uInRegisterConstCount;i++)
		{
			switch(sUniFlexHW.psInRegisterConstMap[i].eFormat)
			{
				case UNIFLEX_CONST_FORMAT_F32:
				{
					if( (sUniFlexHW.psInRegisterConstMap[i].u.s.uSrcIdx >= psReg->uOffset) &&
						(sUniFlexHW.psInRegisterConstMap[i].u.s.uSrcIdx < (psReg->uOffset+psReg->uSizeInDWords)))
					{
						psReg->pui32SrcOffset[ui32ConstantCount] = sUniFlexHW.psInRegisterConstMap[i].u.s.uSrcIdx - psReg->uOffset;
						psReg->pui32DstOffset[ui32ConstantCount] = sUniFlexHW.psInRegisterConstMap[i].uDestIdx + GLES1_VERTEX_SECATTR_NUM_RESERVED;

						ui32ConstantCount++;
						ui32NumSecAttribs++;
					}

					break;
				}
				case UNIFLEX_CONST_FORMAT_STATIC:
				{
					if(sUniFlexHW.psInRegisterConstMap[i].uDestIdx==0xDEAD)
					{
						break;
					}

					if(psFFTNLProgram->ui32NumUFConstants==psFFTNLProgram->ui32MaxNumUFConstants)
					{
						IMG_UINT32 ui32NewSize;

						ui32NewSize = psFFTNLProgram->ui32NumUFConstants + 20;

						psFFTNLProgram->pui32UFConstantData = GLES1Realloc(gc, psFFTNLProgram->pui32UFConstantData,
																	ui32NewSize * sizeof(IMG_UINT32));

						if(!psFFTNLProgram->pui32UFConstantData)
						{
							PVR_DPF((PVR_DBG_ERROR,"CompileFFGenUniFlexCode: Realloc failed"));

							return IMG_FALSE;
						}

						psFFTNLProgram->pui32UFConstantDest = GLES1Realloc(gc, psFFTNLProgram->pui32UFConstantDest,
																	ui32NewSize * sizeof(IMG_UINT32));

						if(!psFFTNLProgram->pui32UFConstantDest)
						{
							PVR_DPF((PVR_DBG_ERROR,"CompileFFGenUniFlexCode: Realloc failed"));

							return IMG_FALSE;
						}
						
						psFFTNLProgram->ui32MaxNumUFConstants = ui32NewSize;
					}

					psFFTNLProgram->pui32UFConstantData[psFFTNLProgram->ui32NumUFConstants] = sUniFlexHW.psInRegisterConstMap[i].u.uValue;
					psFFTNLProgram->pui32UFConstantDest[psFFTNLProgram->ui32NumUFConstants++] = sUniFlexHW.psInRegisterConstMap[i].uDestIdx + 
																								GLES1_VERTEX_SECATTR_NUM_RESERVED;

					ui32NumSecAttribs++;

					/* Prevent the constant being loaded again */
					sUniFlexHW.psInRegisterConstMap[i].uDestIdx=0xDEAD;

					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_WARNING,"CompileFFGenUniFlexCode: Unsupported constant type"));
				}
			}
		}

		psReg->ui32ConstantCount = ui32ConstantCount;

		psRegList = psRegList->psNext;
	}

	ui32NumMemConsts = 0;

	psRegList = psFFTNLProgram->psConstantsList;

	while (psRegList)
	{
		FFGenReg *psReg = psRegList->psReg;
		
		IMG_UINT32 ui32ConstantCount;

		ui32ConstantCount = psReg->ui32ConstantCount;

		for(i=0;i<sUniFlexHW.psMemRemappingForConstsBuff[UF_CONSTBUFFERID_LEGACY].uConstCount;i++)
		{
			UNIFLEX_CONST_LOAD *psConstantLoad = &sUniFlexHW.psMemRemappingForConstsBuff[UF_CONSTBUFFERID_LEGACY].psConstMap[i];

			switch(psConstantLoad->eFormat)
			{
				case UNIFLEX_CONST_FORMAT_F32:
				{
					if( (psConstantLoad->u.s.uSrcIdx >= psReg->uOffset) &&
						(psConstantLoad->u.s.uSrcIdx < (psReg->uOffset+psReg->uSizeInDWords)))
					{
						psReg->pui32SrcOffset[ui32ConstantCount] = psConstantLoad->u.s.uSrcIdx - psReg->uOffset;
						psReg->pui32DstOffset[ui32ConstantCount] = psConstantLoad->uDestIdx + GLES1_VERTEX_SECATTR_NUM_RESERVED + ui32NumSecAttribs;

						ui32ConstantCount++;
						ui32NumMemConsts++;
					}

					break;
				}
				case UNIFLEX_CONST_FORMAT_STATIC:
				{
					if(psConstantLoad->uDestIdx==0xDEAD)
					{
						break;
					}

					if(psFFTNLProgram->ui32NumUFConstants==psFFTNLProgram->ui32MaxNumUFConstants)
					{
						IMG_UINT32 ui32NewSize;

						ui32NewSize = psFFTNLProgram->ui32NumUFConstants + 20;

						psFFTNLProgram->pui32UFConstantData = GLES1Realloc(gc, psFFTNLProgram->pui32UFConstantData,
																	ui32NewSize * sizeof(IMG_UINT32));

						if(!psFFTNLProgram->pui32UFConstantData)
						{
							PVR_DPF((PVR_DBG_ERROR,"CompileFFGenUniFlexCode: Realloc failed"));

							return IMG_FALSE;
						}

						psFFTNLProgram->pui32UFConstantDest = GLES1Realloc(gc, psFFTNLProgram->pui32UFConstantDest,
																	ui32NewSize * sizeof(IMG_UINT32));

						if(!psFFTNLProgram->pui32UFConstantDest)
						{
							PVR_DPF((PVR_DBG_ERROR,"CompileFFGenUniFlexCode: Realloc failed"));

							return IMG_FALSE;
						}
						
						psFFTNLProgram->ui32MaxNumUFConstants = ui32NewSize;
					}

					psFFTNLProgram->pui32UFConstantData[psFFTNLProgram->ui32NumUFConstants] = psConstantLoad->u.uValue;
					psFFTNLProgram->pui32UFConstantDest[psFFTNLProgram->ui32NumUFConstants++] = psConstantLoad->uDestIdx + GLES1_VERTEX_SECATTR_NUM_RESERVED;

					ui32NumMemConsts++;

					/* Prevent the constant being loaded again */
					psConstantLoad->uDestIdx=0xDEAD;

					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_WARNING,"CompileFFGenUniFlexCode: Unsupported constant type"));
				}
			}
		}

		psReg->ui32ConstantCount = ui32ConstantCount;

		psRegList = psRegList->psNext;
	}

	PVRCleanupUniflexHw((IMG_VOID*)gc->sProgram.hUniFlexContext, &sUniFlexHW);

	psFFGENProgramDetails->ui32SecondaryAttributeCount = ui32NumSecAttribs;
	psFFGENProgramDetails->ui32MemoryConstantCount     = ui32NumMemConsts;

	psFFTNLProgram->uMemoryConstantsSize	= ui32NumMemConsts;
	psFFTNLProgram->uMemConstBaseAddrSAReg	= 0;
	psFFTNLProgram->uSecAttribStart			= GLES1_VERTEX_SECATTR_NUM_RESERVED;
	psFFTNLProgram->uSecAttribSize			= GLES1_VERTEX_SECATTR_NUM_RESERVED + ui32NumSecAttribs;

	return IMG_TRUE;
}	
											  

#else


/***********************************************************************************
 Function Name      : SetupBuildFFTNLShaderConstants
 Inputs             : psFFTNLProgram
 Outputs            : psShader->sConstantData.pfConstantData
 Returns            : None
 Description        : Setups and builds a FFTNL shader constants
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupBuildFFTNLShaderConstants(GLES1Context *gc)
{
	IMG_UINT32 i;

	IMG_FLOAT *pfConstantBase;	

	GLES1Shader *psShader = gc->sProgram.psCurrentVertexShader;

	FFGenProgram 	*psFFTNLProgram =	psShader->u.sFFTNL.psFFTNLProgram;
	FFGenRegList    *psRegList = 		psFFTNLProgram->psConstantsList;
	
	/* Temp matrix used for transposing from opengl format */
	GLESMatrix sTempMatrix;
	
	/* update constants buffer size */
	if(UpdateFFTNLShaderConstantsSize(gc, psShader) != PVRSRV_OK)
	{
		return;
	}

	/* get constant base address */
	pfConstantBase = psShader->pfConstantData;

	/* Loop through list converting the registers */
	while (psRegList)
	{
		FFGenReg *psReg = psRegList->psReg;
		IMG_FLOAT *pfTempMatrix =  &sTempMatrix.afMatrix[0][0];
		IMG_FLOAT *pfConstant = pfConstantBase + psReg->uOffset;

		/* Check if it's a constant in memory */
		if (psReg->eType == USEASM_REGTYPE_TEMP)
		{
			/*
			   Adjust the offset so that it will appear after the secondary attributes
			   as the memory constants will share the same offsets. Need to adjust the
			   remap table to reflect this adjustment
			*/
			pfConstant += psFFTNLProgram->uSecAttribSize; 
		}

		/* Determine properties */
		switch (psReg->eBindingRegDesc)
		{
			case FFGEN_STATE_MODELVIEWMATRIX:
			{
				GLES1_ASSERT(psReg->uSizeInDWords <= FFTNL_SIZE_MATRIX_4X4);

				TransposeMatrix(&sTempMatrix, &gc->sTransform.psModelView->sMatrix);

				COPY_MATRIX(pfConstant, pfTempMatrix, psReg->uSizeInDWords);

				break;
			}
			case FFGEN_STATE_TEXTUREMATRIX:
			{
				GLES1_ASSERT((psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4) <= GLES1_MAX_TEXTURE_UNITS);

				for(i = 0; i < (psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4 ); i++)
				{
					TransposeMatrix(&sTempMatrix, &gc->sTransform.apsTexture[i]->sMatrix);

					COPY_MATRIX(pfConstant, pfTempMatrix, FFTNL_SIZE_MATRIX_4X4);

					pfConstant += FFTNL_SIZE_MATRIX_4X4;
				}

				break;
			}
			case FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSE:
			{
				GLES1_ASSERT(psReg->uSizeInDWords <= FFTNL_SIZE_MATRIX_4X4);

				if(gc->sTransform.psModelView->bUpdateInverse)
				{
					(*gc->sProcs.pfnComputeInverseTranspose)(gc, &gc->sTransform.psModelView[0]);
				}

				TransposeMatrix(&sTempMatrix, &gc->sTransform.psModelView->sInverseTranspose);

				COPY_MATRIX(pfConstant, pfTempMatrix, psReg->uSizeInDWords);

				break;
			}
			case FFGEN_STATE_PROJECTMATRIX:
			{
				GLES1_ASSERT(psReg->uSizeInDWords <= FFTNL_SIZE_MATRIX_4X4);

				TransposeMatrix(&sTempMatrix, &gc->sTransform.psProjection->sMatrix);

				COPY_MATRIX(pfConstant, pfTempMatrix, psReg->uSizeInDWords);

				break;
			}
			case FFGEN_STATE_CLIPPLANE:
			{
				GLES1_ASSERT((psReg->uSizeInDWords / FFTNL_SIZE_CLIPPLANE) <= GLES1_MAX_CLIP_PLANES);

				for(i = 0; i < (psReg->uSizeInDWords / FFTNL_SIZE_CLIPPLANE); i++)
				{
					COPY_COORD4(pfConstant, gc->sTransform.asEyeClipPlane[i]);

					pfConstant += FFTNL_SIZE_CLIPPLANE;
				}

				break;
			}
			case FFGEN_STATE_MODELVIEWPROJECTIONMATRIX:
			{
				GLES1_ASSERT(psReg->uSizeInDWords <= FFTNL_SIZE_MATRIX_4X4);

				TransposeMatrix(&sTempMatrix, &gc->sTransform.psModelView->sMvp);

				COPY_MATRIX(pfConstant, pfTempMatrix, psReg->uSizeInDWords);

				break;
			}
			case FFGEN_STATE_POINT:
			{
				GLES1_ASSERT(psReg->uSizeInDWords == FFTNL_NUM_MEMBERS_POINTPARAMS);

				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_SIZE] = gc->sState.sPoint.fRequestedSize;
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_MIN] = *gc->sState.sPoint.pfMinPointSize;
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_MAX] = *gc->sState.sPoint.pfMaxPointSize;
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_FADETHRESH] = gc->sState.sPoint.fFade;
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_CONATTEN] = gc->sState.sPoint.afAttenuation[0];
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_LINATTEN] = gc->sState.sPoint.afAttenuation[1];
				pfTempMatrix[FFTNL_OFFSETS_POINTPARAMS_QUADATTEN] = gc->sState.sPoint.afAttenuation[2];

				for(i = 0; i < psReg->uSizeInDWords; i++)
				{
					COPY_FLOAT(pfConstant, pfTempMatrix[i]);

					pfConstant += auPointParamsMemberSizes[i];
				}
			
				break;
			}
			case FFGEN_STATE_LIGHTSOURCE0:
			case FFGEN_STATE_LIGHTSOURCE1:
			case FFGEN_STATE_LIGHTSOURCE2:
			case FFGEN_STATE_LIGHTSOURCE3:
			case FFGEN_STATE_LIGHTSOURCE4:
			case FFGEN_STATE_LIGHTSOURCE5:
			case FFGEN_STATE_LIGHTSOURCE6:
			case FFGEN_STATE_LIGHTSOURCE7:
			{
				GLEScoord sCoordTemp;
				GLESLightSourceState *psLss;
				IMG_UINT32 ui32LightNum;

				GLES1_ASSERT(psReg->uSizeInDWords==FFTNL_SIZE_LIGHTSOURCE);
			
				ui32LightNum = (IMG_UINT32)(psReg->eBindingRegDesc - FFGEN_STATE_LIGHTSOURCE0);

				psLss = &gc->sState.sLight.psSource[ui32LightNum];	

				/* ambient */
				COPY_COLOR4(pfConstant,  psLss->sAmbient);
				pfConstant+= auLightSourceMemberSizes[0];
				/* vec4 diffuse; */
				COPY_COLOR4(pfConstant, psLss->sDiffuse);
				pfConstant+= auLightSourceMemberSizes[1];

				/* vec4 specular;  */
				COPY_COLOR4(pfConstant, psLss->sSpecular);
				pfConstant+= auLightSourceMemberSizes[2];

				/* vec4 position */
				sCoordTemp = psLss->sPositionEye;

				if(sCoordTemp.fW)
				{
					sCoordTemp.fW = 1.0f / sCoordTemp.fW;
					sCoordTemp.fX *= sCoordTemp.fW;
					sCoordTemp.fY *= sCoordTemp.fW;
					sCoordTemp.fZ *= sCoordTemp.fW;
				}
				sCoordTemp.fW = 1.0f;

				COPY_COORD4(pfConstant, sCoordTemp);
				pfConstant+= auLightSourceMemberSizes[3];

				/* vec4 positionNormalised */
				sCoordTemp = psLss->sPositionEye;
				sCoordTemp.fW = (IMG_FLOAT) sqrt(sCoordTemp.fX * sCoordTemp.fX + sCoordTemp.fY * sCoordTemp.fY + sCoordTemp.fZ * sCoordTemp.fZ);
				if(sCoordTemp.fW)
				{
					sCoordTemp.fW = 1.0f / sCoordTemp.fW;
					sCoordTemp.fX *= sCoordTemp.fW;
					sCoordTemp.fY *= sCoordTemp.fW;
					sCoordTemp.fZ *= sCoordTemp.fW;
				}
				sCoordTemp.fW = 1.0f;

				COPY_COORD4(pfConstant, sCoordTemp);
				pfConstant+= auLightSourceMemberSizes[4];

				 /* vec4 halfVector;  */
				sCoordTemp = psLss->sPositionEye;
				sCoordTemp.fZ += 1.0f;
				sCoordTemp.fW =	(IMG_FLOAT) (1.0f / (IMG_FLOAT) sqrt(sCoordTemp.fX * sCoordTemp.fX +
											sCoordTemp.fY * sCoordTemp.fY +
											sCoordTemp.fZ * sCoordTemp.fZ));
				sCoordTemp.fX *= sCoordTemp.fW;
				sCoordTemp.fY *= sCoordTemp.fW;
				sCoordTemp.fZ *= sCoordTemp.fW;
				sCoordTemp.fW = 1.0f;

				COPY_COORD4(pfConstant, sCoordTemp);
				pfConstant+= auLightSourceMemberSizes[5];

				/* vec3 spotDirection; */
				COPY_COORD3(pfConstant, psLss->sDirection);
				pfConstant+= auLightSourceMemberSizes[6];

				/* spotExponent;*/
				COPY_FLOAT(pfConstant, psLss->fSpotLightExponent);
				pfConstant+= auLightSourceMemberSizes[7];

				/* float constantAttenuation */
				COPY_FLOAT(pfConstant, psLss->afAttenuation[0]);
				pfConstant+= auLightSourceMemberSizes[10];

				/* float linearAttenuation */
				COPY_FLOAT(pfConstant, psLss->afAttenuation[1]);
				pfConstant+= auLightSourceMemberSizes[11];

				/* float quadraticAttenuation; */
				COPY_FLOAT(pfConstant, psLss->afAttenuation[2]);
				pfConstant+= auLightSourceMemberSizes[12];
			
				/* spotCutoff */
				COPY_FLOAT(pfConstant, psLss->fSpotLightCutOffAngle);
				pfConstant+= auLightSourceMemberSizes[8];

				/* float spotCosCutoff */
				pfTempMatrix[0] = (IMG_FLOAT) GLES1_COSF(psLss->fSpotLightCutOffAngle * GLES1_DegreesToRadians);
				COPY_FLOAT(pfConstant, pfTempMatrix[0]);

				break;
			}
			case FFGEN_STATE_FRONTLIGHTPRODUCT:
			case FFGEN_STATE_BACKLIGHTPRODUCT:
			{
				GLESLightSourceState *psLss;
				GLESMaterialState *psMaterial = &gc->sState.sLight.sMaterial;

				GLES1_ASSERT((psReg->uSizeInDWords / FFTNL_SIZE_LIGHTPRODUCT) <= GLES1_MAX_LIGHTS);

				for(i = 0; i < (psReg->uSizeInDWords / FFTNL_SIZE_LIGHTPRODUCT); i++)
				{
					psLss = &gc->sState.sLight.psSource[i];

					/* ambient */
					pfConstant[0] = psMaterial->sAmbient.fRed * psLss->sAmbient.fRed;
					pfConstant[1] = psMaterial->sAmbient.fGreen * psLss->sAmbient.fGreen;
					pfConstant[2] = psMaterial->sAmbient.fBlue * psLss->sAmbient.fBlue;
					pfConstant[3] = psMaterial->sAmbient.fAlpha;
					pfConstant += auLightProductMemberSizes[0];

					/* diffuse */
					pfConstant[0] = psMaterial->sDiffuse.fRed * psLss->sDiffuse.fRed;
					pfConstant[1] = psMaterial->sDiffuse.fGreen * psLss->sDiffuse.fGreen;
					pfConstant[2] = psMaterial->sDiffuse.fBlue * psLss->sDiffuse.fBlue;
					pfConstant[3] = psMaterial->sDiffuse.fAlpha;
					pfConstant += auLightProductMemberSizes[1];

					/* specular */
					pfConstant[0] = psMaterial->sSpecular.fRed * psLss->sSpecular.fRed;
					pfConstant[1] = psMaterial->sSpecular.fGreen * psLss->sSpecular.fGreen;
					pfConstant[2] = psMaterial->sSpecular.fBlue * psLss->sSpecular.fBlue;
					pfConstant[3] = psMaterial->sSpecular.fAlpha;
					pfConstant += auLightProductMemberSizes[2];
				}

				break;
			}
			case FFGEN_STATE_FRONTMATERIAL:
			case FFGEN_STATE_BACKMATERIAL:
			{
				GLESMaterialState *psMaterial = &gc->sState.sLight.sMaterial;
				GLES1_ASSERT(psReg->uSizeInDWords == FFTNL_SIZE_MATERIAL);

				COPY_COLOR4(pfConstant, psMaterial->sEmissive);
				pfConstant+= auMaterialMemberSizes[0];

				COPY_COLOR4(pfConstant, psMaterial->sAmbient);
				pfConstant+= auMaterialMemberSizes[1];

				COPY_COLOR4(pfConstant, psMaterial->sDiffuse);
				pfConstant+= auMaterialMemberSizes[2];

				COPY_COLOR4(pfConstant, psMaterial->sSpecular);
				pfConstant+= auMaterialMemberSizes[3];

				COPY_FLOAT(pfConstant,  psMaterial->sSpecularExponent.fX);

				break;
			}
			case FFGEN_STATE_LIGHTMODEL:
			{
				GLES1_ASSERT(psReg->uSizeInDWords == FFTNL_SIZE_LIGHTTMODEL);

				COPY_COLOR4(pfConstant, gc->sState.sLight.sModel.sAmbient);

				break;
			}
			case FFGEN_STATE_FRONTLIGHTMODELPRODUCT:
			case FFGEN_STATE_BACKLIGHTMODELPRODUCT:
			{
				GLESMaterialState *psMaterial = &gc->sState.sLight.sMaterial;
				GLESLightModelState *psModel = &gc->sState.sLight.sModel;

				GLES1_ASSERT(psReg->uSizeInDWords == FFTNL_SIZE_LIGHTTMODELPRODUCT);

				pfConstant[0] = (psMaterial->sAmbient.fRed * psModel->sAmbient.fRed) + psMaterial->sEmissive.fRed;
				pfConstant[1] = (psMaterial->sAmbient.fGreen * psModel->sAmbient.fGreen) + psMaterial->sEmissive.fGreen;
				pfConstant[2] = (psMaterial->sAmbient.fBlue * psModel->sAmbient.fBlue) + psMaterial->sEmissive.fBlue;
				pfConstant[3] =  psMaterial->sAmbient.fAlpha;

				break;
			}
			case FFGEN_STATE_PMXFOGPARAM:
			{
				GLES1_ASSERT(psReg->uSizeInDWords == 4);

				pfConstant[0] = gc->sState.sFog.fDensity * GLES1_ONE_OVER_LN_TWO;
				pfConstant[1] = gc->sState.sFog.fDensity * GLES1_ONE_OVER_SQRT_LN_TWO;
				pfConstant[2] = -gc->sState.sFog.fOneOverEMinusS;
				pfConstant[3] = gc->sState.sFog.fEnd * gc->sState.sFog.fOneOverEMinusS;

				break;
			}
			case FFGEN_STATE_PMXPOINTSIZE:
			{
				GLES1_ASSERT(psReg->uSizeInDWords == 1);

				COPY_FLOAT(pfConstant, *gc->sState.sPoint.pfPointSize);

				break;
			}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
			case FFGEN_STATE_MATRIXPALETTEINDEXCLAMP:
			{
				IMG_FLOAT fIndexClampValue;

				GLES1_ASSERT(psReg->uSizeInDWords == 1);

				fIndexClampValue = (IMG_FLOAT) (NUM_PALETTE_ENTRIES_MIN - 1);

				COPY_FLOAT(pfConstant, fIndexClampValue);

				break;
			}
			case FFGEN_STATE_MODELVIEWMATRIXPALETTE:
			{
				GLES1_ASSERT((psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4) <= GLES1_MAX_PALETTE_MATRICES);

				for(i=0; i<(psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4); i++)
				{
					TransposeMatrix(&sTempMatrix, &gc->sTransform.psMatrixPalette[i].sMatrix);

					COPY_MATRIX(pfConstant, pfTempMatrix, FFTNL_SIZE_MATRIX_4X4);

					pfConstant += FFTNL_SIZE_MATRIX_4X4;
				}

				break;
			}
			case FFGEN_STATE_MODELVIEWMATRIXINVERSETRANSPOSEPALETTE:
			{
				GLES1_ASSERT((psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4) <= GLES1_MAX_PALETTE_MATRICES);

				for(i=0; i<(psReg->uSizeInDWords / FFTNL_SIZE_MATRIX_4X4); i++)
				{
					if(gc->sTransform.psMatrixPalette[i].bUpdateInverse)
					{
						(*gc->sProcs.pfnComputeInverseTranspose)(gc, &gc->sTransform.psMatrixPalette[i]);
					}

					TransposeMatrix(&sTempMatrix, &gc->sTransform.psMatrixPalette[i].sInverseTranspose);

					COPY_MATRIX(pfConstant, pfTempMatrix, FFTNL_SIZE_MATRIX_4X4);

					pfConstant += FFTNL_SIZE_MATRIX_4X4;
				}

				break;
			}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"SetupBuildFFTNLShaderConstants: psReg->eBindingRegDesc = %x (Bad state)",psReg->eBindingRegDesc));
				break;
			}
		}

		psRegList = psRegList->psNext;
	}

}
#endif


/***********************************************************************************
 Function Name      : SetupFFTNLShaderCode
 Inputs             : psFFTNLProgram
 Outputs            : psShader
 Returns            : error 
 Description        : Gets a FFTNL shader code
************************************************************************************/
static GLES1_MEMERROR SetupFFTNLShaderCode(GLES1Context *gc, GLES1Shader *psShader, HashValue tFFTNLHashValue, FFTNLGenDesc *psFFTNLGenDesc)
{
	FFGenProgram *psFFTNLProgram;


	/* Create a new entry */
	GLES1_TIME_START(GLES1_TIMER_FFGEN_GENERATION_TIME);

	psFFTNLProgram = FFGenGenerateTNLProgram((IMG_VOID*)gc->sProgram.hFFTNLGenContext, psFFTNLGenDesc);

	GLES1_TIME_STOP(GLES1_TIMER_FFGEN_GENERATION_TIME);

	if(!psFFTNLProgram)
	{
		PVR_DPF((PVR_DBG_FATAL,"SetupFFTNLShaderCode: Failed to allocate fftnl code memory"));

		return GLES1_HOST_MEM_ERROR;
	}

#if defined(FFGEN_UNIFLEX)


	GLES1_TIME_START(GLES1_TIMER_USC_TIME);

	if(!CompileFFGenUniFlexCode(gc, psFFTNLProgram))
	{
		GLES1_TIME_STOP(GLES1_TIMER_USC_TIME);

		FFGenFreeProgram((IMG_VOID *)gc->sProgram.hFFTNLGenContext, psFFTNLProgram);

		return GLES1_HOST_MEM_ERROR;
	}

	GLES1_TIME_STOP(GLES1_TIMER_USC_TIME);

#endif /* defined(FFGEN_UNIFLEX) */

	psFFTNLProgram->uHashValue = (IMG_UINT32) tFFTNLHashValue;

	/* get program code */
	psShader->psFFGENProgramDetails = psFFTNLProgram->psFFGENProgramDetails;

	/* get new FFTNL data */
	psShader->u.sFFTNL.psFFTNLProgram = psFFTNLProgram;

	/* reset constant settings */
	psShader->ui32SizeOfConstants = 0;
	psShader->pfConstantData = IMG_NULL;

	return GLES1_NO_ERROR;
}


/***********************************************************************************
 Function Name      : SetupFFTNLShader
 Inputs             : gc
 Outputs            : pbChanged
 Returns            : error
 Description        : Gets a FFTNL program
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR SetupFFTNLShader(GLES1Context *gc)
{
	FFTNLGenDesc *psFFTNLGenDesc;

	/* Arbitrary initial value */
	HashValue tFFTNLHashValue = STATEHASH_INIT_VALUE;

	/* Get shader head */
	GLES1Shader *psVertexShader;

	GetFFTNLGenDesc(gc, &gc->sProgram.uTempBuffer.sTNLDescription);

	/* Hash the fftnl description */
	tFFTNLHashValue = HashFunc((IMG_UINT32 *)&gc->sProgram.uTempBuffer.sTNLDescription, (sizeof(FFTNLGenDesc)/sizeof(IMG_UINT32)), tFFTNLHashValue);

	/* Search hash table to see if we have already stored this program */
	if(!HashTableSearch(gc, 
						&gc->sProgram.sFFTNLHashTable, tFFTNLHashValue, 
						(IMG_UINT32 *)&gc->sProgram.uTempBuffer.sTNLDescription, (sizeof(FFTNLGenDesc)/sizeof(IMG_UINT32)), 
						(IMG_UINT32 *)&psVertexShader))
	{
		if (!ValidateHashTableInsert(gc, &gc->sProgram.sFFTNLHashTable, tFFTNLHashValue))
		{
			PVR_DPF((PVR_DBG_FATAL,"SetupFFTNLShader: Hash table is full and cannot become free"));
			return GLES1_GENERAL_MEM_ERROR;
		}

		psFFTNLGenDesc = GLES1Malloc(gc, sizeof(FFTNLGenDesc));

		if(!psFFTNLGenDesc)
		{
			PVR_DPF((PVR_DBG_FATAL,"SetupUSEVertexShader: Failed to allocate has data memory"));

			return GLES1_HOST_MEM_ERROR;
		}

		GLES1MemCopy(psFFTNLGenDesc,&gc->sProgram.uTempBuffer.sTNLDescription, sizeof(FFTNLGenDesc));

		/* allocate space for new shader */
		psVertexShader = GLES1Calloc(gc, sizeof(GLES1Shader));
	
		if(!psVertexShader)
		{
			GLES1Free(IMG_NULL, psFFTNLGenDesc);

			PVR_DPF((PVR_DBG_FATAL,"SetupUSEVertexShader: Failed to allocate shader memory"));

			return GLES1_HOST_MEM_ERROR;
		}

		/* gets program code, constants and outputselects  */
		if(SetupFFTNLShaderCode(gc, psVertexShader, (IMG_UINT32) tFFTNLHashValue, psFFTNLGenDesc) != GLES1_NO_ERROR)
		{
			GLES1Free(IMG_NULL, psVertexShader);

			GLES1Free(IMG_NULL, psFFTNLGenDesc);

			PVR_DPF((PVR_DBG_FATAL,"SetupFFTNLShader: Failed to create FFTNL shader code"));

			return GLES1_HOST_MEM_ERROR;
		}

		SetupFFTNLShaderInputs(psVertexShader);	

		HashTableInsert(gc, 
						&gc->sProgram.sFFTNLHashTable, tFFTNLHashValue, 
						(IMG_UINT32 *)psFFTNLGenDesc, (sizeof(FFTNLGenDesc)/sizeof(IMG_UINT32)), 
						(IMG_UINT32)psVertexShader);

		/* Add to shader list */
		psVertexShader->psPrevious = IMG_NULL;
		psVertexShader->psNext = gc->sProgram.psVertex;

		if(gc->sProgram.psVertex)
		{
			gc->sProgram.psVertex->psPrevious = psVertexShader;
		}

		gc->sProgram.psVertex = psVertexShader;

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
	}
	else
	{
		/* update FFTNL code ref counter */
		if(psVertexShader->u.sFFTNL.psFFTNLProgram)
		{
			psVertexShader->u.sFFTNL.psFFTNLProgram->uRefCount++;
		}
		else
		{
			PVR_DPF((PVR_DBG_FATAL,"SetupUSEVertexShader: Existing shader code not found"));

			return GLES1_GENERAL_MEM_ERROR;
		}
	}

	gc->sProgram.psCurrentVertexShader = psVertexShader;

	return GLES1_NO_ERROR;
}

/******************************************************************************
 End of file (fftnlgles.c)
******************************************************************************/

