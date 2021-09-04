/******************************************************************************
 * Name         : shader.c
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
 * $Log: shader.c $
 * ./
 *  --- Revision Logs Removed --- 
 *****************************************************************************/
#include <string.h>
#include <stdarg.h>

#include "context.h"
#include "binshader.h"
#include "dmscalc.h"
/* From useasm directory */
#include "use.h"

#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE)
#include "digest.h"
#endif

#define GET_REG_OFFSET(comp)		((comp) / REG_COMPONENTS)
#define GET_REG_OFFCOMP(comp)		((comp) % REG_COMPONENTS)
#define GET_REG_COUNT(compcount)	(((compcount) + REG_COMPONENTS - 1)/REG_COMPONENTS)

/***********************************************************************************
 Function Name      : SharedShaderStateAddRef
 Inputs             : gc, psSharedState
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Adds a reference to shared shader state
************************************************************************************/
static IMG_VOID SharedShaderStateAddRef(GLES2Context *gc, GLES2SharedShaderState *psSharedState)
{
	GLES_ASSERT(psSharedState);

	/* ENTER CRITICAL SECTION */
	PVRSRVLockMutex(gc->psSharedState->hPrimaryLock);

	psSharedState->ui32RefCount++;

	/* EXIT CRITICAL SECTION */
	PVRSRVUnlockMutex(gc->psSharedState->hPrimaryLock);
}


/***********************************************************************************
 Function Name      : SharedShaderStateDelRef
 Inputs             : gc, psSharedState
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Removes a reference to shared shader state.
                      If the reference count reaches zero, the shared state is freed.
************************************************************************************/
static IMG_VOID SharedShaderStateDelRef(GLES2Context *gc, GLES2SharedShaderState *psSharedState)
{
	IMG_UINT32 i, j;
	GLSLBindingSymbol *psSymbol;

	/* Ignore NULL argument silently */
	if(psSharedState)
	{
		/* ENTER CRITICAL SECTION */
		PVRSRVLockMutex(gc->psSharedState->hPrimaryLock);

		psSharedState->ui32RefCount--;

		if(psSharedState->ui32RefCount == 0)
		{
			if(psSharedState->sBindingSymbolList.uNumBindings)
			{
				for(i=0; i < psSharedState->sBindingSymbolList.uNumBindings; i++)
				{
					psSymbol = &psSharedState->sBindingSymbolList.psBindingSymbolEntries[i];

					GLES2Free(IMG_NULL, psSymbol->pszName);

					if(psSymbol->uNumBaseTypeMembers)
					{
						for(j=0; j < psSymbol->uNumBaseTypeMembers; j++)
						{
							GLES2Free(IMG_NULL, psSymbol->psBaseTypeMembers[j].pszName);
						}

						GLES2Free(IMG_NULL, psSymbol->psBaseTypeMembers);
					}
				}

				GLES2Free(IMG_NULL, psSharedState->sBindingSymbolList.psBindingSymbolEntries);
			}
		

			if(psSharedState->sBindingSymbolList.uNumCompsUsed)
			{
				GLES2Free(IMG_NULL, psSharedState->sBindingSymbolList.pfConstantData);
			}

			if(psSharedState->pvUniPatchShader)
			{
				PVRUniPatchDestroyShader(gc->sProgram.pvUniPatchContext, psSharedState->pvUniPatchShader);
			}

			if(psSharedState->pvUniPatchShaderMSAATrans)
			{
				PVRUniPatchDestroyShader(gc->sProgram.pvUniPatchContext, psSharedState->pvUniPatchShaderMSAATrans);
			}

			USESecondaryUploadTaskDelRef(gc, psSharedState->psSecondaryUploadTask);

			GLES2Free(IMG_NULL, psSharedState);
		}

		/* EXIT CRITICAL SECTION */
		PVRSRVUnlockMutex(gc->psSharedState->hPrimaryLock);
	}
}


/***********************************************************************************
 Function Name        : USESecondaryUploadTaskAddRef
 Inputs             : gc, psUSESecondaryUploadTask
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Adds a reference to a USE secondary attribute upload task.
************************************************************************************/
IMG_INTERNAL IMG_VOID USESecondaryUploadTaskAddRef(GLES2Context *gc, GLES2USESecondaryUploadTask *psUSESecondaryUploadTask)
{
	/* ENTER CRITICAL SECTION */
	PVRSRVLockMutex(gc->psSharedState->hSecondaryLock);

	/* Ignore NULL argument silently */
	if(psUSESecondaryUploadTask)
	{
		psUSESecondaryUploadTask->ui32RefCount++;
	}

	/* EXIT CRITICAL SECTION */
	PVRSRVUnlockMutex(gc->psSharedState->hSecondaryLock);
}


/***********************************************************************************
 Function Name      : USESecondaryUploadTaskDelRef
 Inputs             : gc, psUSESecondaryUploadTask
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Removes a reference to a USE secondary attribute upload task.
                      If the reference count reaches zero, the task is freed.
************************************************************************************/
IMG_INTERNAL IMG_VOID USESecondaryUploadTaskDelRef(GLES2Context *gc, GLES2USESecondaryUploadTask *psUSESecondaryUploadTask)
{
	IMG_BOOL bDoFree = IMG_FALSE;

	/* Ignore NULL argument silently */
	if(!psUSESecondaryUploadTask)
	{
		return;
	}

	/* ENTER CRITICAL SECTION */
	PVRSRVLockMutex(gc->psSharedState->hSecondaryLock);

	psUSESecondaryUploadTask->ui32RefCount--;

	if(psUSESecondaryUploadTask->ui32RefCount == 0)
	{
		bDoFree = IMG_TRUE;
	}

	/* EXIT CRITICAL SECTION */
	PVRSRVUnlockMutex(gc->psSharedState->hSecondaryLock);

	if(bDoFree)
	{
		UCH_CodeHeapFree(psUSESecondaryUploadTask->psSecondaryCodeBlock);
		GLES2Free(IMG_NULL, psUSESecondaryUploadTask);
	}
}


/***********************************************************************************
 Function Name      : ShaderScratchMemAddRef
 Inputs             : gc, psScratchMem
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Adds a reference to a shader scratch memory allocation
************************************************************************************/
IMG_INTERNAL IMG_VOID ShaderScratchMemAddRef(GLES2Context *gc, GLES2ShaderScratchMem *psScratchMem)
{
	/* ENTER CRITICAL SECTION */
	PVRSRVLockMutex(gc->psSharedState->hSecondaryLock);

	/* Ignore NULL argument silently */
	if(psScratchMem)
	{
		psScratchMem->ui32RefCount++;
	}

	/* EXIT CRITICAL SECTION */
	PVRSRVUnlockMutex(gc->psSharedState->hSecondaryLock);
}


/***********************************************************************************
 Function Name      : ShaderScratchMemDelRef
 Inputs             : gc, psScratchMem
 Outputs            : -
 Returns            : -
 Description        : Removes a reference to a shader scratch memory allocation.
                      If the reference count reaches zero, the memory is freed.
************************************************************************************/
static IMG_VOID ShaderScratchMemDelRef(GLES2Context *gc, GLES2ShaderScratchMem *psScratchMem)
{
	IMG_BOOL bDoFree = IMG_FALSE;

	/* Ignore NULL argument silently */
	if(!psScratchMem)
	{
		return;
	}

	/* ENTER CRITICAL SECTION */
	PVRSRVLockMutex(gc->psSharedState->hSecondaryLock);

	psScratchMem->ui32RefCount--;

	if(psScratchMem->ui32RefCount == 0)
	{
		bDoFree = IMG_TRUE;
	}

	/* EXIT CRITICAL SECTION */
	PVRSRVUnlockMutex(gc->psSharedState->hSecondaryLock);

	if(bDoFree)
	{
		GLES2FREEDEVICEMEM(gc->ps3DDevData, psScratchMem->psMemInfo);

		GLES2Free(IMG_NULL, psScratchMem);
	}
}


/***********************************************************************************
 Function Name      : ShaderIndexableTempsMemAddRef
 Inputs             : gc, psIndexableTempsMem
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Adds a reference to a shader scratch memory allocation
************************************************************************************/
IMG_INTERNAL IMG_VOID ShaderIndexableTempsMemAddRef(GLES2Context *gc, GLES2ShaderIndexableTempsMem *psIndexableTempsMem)
{
	/* ENTER CRITICAL SECTION */
	PVRSRVLockMutex(gc->psSharedState->hSecondaryLock);

	/* Ignore NULL argument silently */
	if(psIndexableTempsMem)
	{
		psIndexableTempsMem->ui32RefCount++;
	}

	/* EXIT CRITICAL SECTION */
	PVRSRVUnlockMutex(gc->psSharedState->hSecondaryLock);
}


/***********************************************************************************
 Function Name      : ShaderIndexableTempsMemDelRef
 Inputs             : gc, psIndexableTempsMem
 Outputs            : -
 Returns            : -
 Description        : Removes a reference to a shader scratch memory allocation.
                      If the reference count reaches zero, the memory is freed.
************************************************************************************/
static IMG_VOID ShaderIndexableTempsMemDelRef(GLES2Context *gc, GLES2ShaderIndexableTempsMem *psIndexableTempsMem)
{
	IMG_BOOL bDoFree = IMG_FALSE;

	/* Ignore NULL argument silently */
	if(!psIndexableTempsMem)
	{
		return;
	}

	/* ENTER CRITICAL SECTION */
	PVRSRVLockMutex(gc->psSharedState->hSecondaryLock);

	psIndexableTempsMem->ui32RefCount--;

	if(psIndexableTempsMem->ui32RefCount == 0)
	{
		bDoFree = IMG_TRUE;
	}

	/* EXIT CRITICAL SECTION */
	PVRSRVUnlockMutex(gc->psSharedState->hSecondaryLock);

	if(bDoFree)
	{
		GLES2FREEDEVICEMEM(gc->ps3DDevData, psIndexableTempsMem->psMemInfo);

		GLES2Free(IMG_NULL, psIndexableTempsMem);
	}
}


#if defined(SUPPORT_SOURCE_SHADER)

/***********************************************************************************
 Function Name      : CopySymbolBindings
 Inputs             : gc, psDstList, psSrcList
 Outputs            : -
 Returns            : Success
 Description        : Copies a binding symbol list (including sub-structure members)
************************************************************************************/
static IMG_BOOL CopySymbolBindings(GLES2Context *gc, GLSLBindingSymbolList *psDstList,
							GLSLBindingSymbolList *psSrcList)
{
	IMG_UINT32 i, j, ui32DataSize;
	GLSLBindingSymbol *psSrcSymbol, *psDstSymbol;

	/* Remove warning in non-Symbian builds */
	PVR_UNREFERENCED_PARAMETER(gc);

	if(!psSrcList->uNumBindings)
	{
		return IMG_TRUE;
	}

	ui32DataSize = psSrcList->uNumBindings * sizeof(GLSLBindingSymbol);

	psDstList->psBindingSymbolEntries = GLES2Malloc(gc, ui32DataSize);

	if(!psDstList->psBindingSymbolEntries)
	{
		return IMG_FALSE;
	}

	/* Copy list of symbols */
	GLES2MemCopy(psDstList->psBindingSymbolEntries, 
					psSrcList->psBindingSymbolEntries, 
					ui32DataSize);


	/* For each symbol, copy the name, and any list of substructure symbols */
	for(i = 0; i < psSrcList->uNumBindings; i++)
	{
		psSrcSymbol = &psSrcList->psBindingSymbolEntries[i];
		psDstSymbol = &psDstList->psBindingSymbolEntries[i];

		ui32DataSize = strlen(psSrcSymbol->pszName) + 1;

		psDstSymbol->pszName = GLES2Malloc(gc, ui32DataSize);

		if(!psDstSymbol->pszName)
		{
			goto CleanupSymbols;
		}
		
		GLES2MemCopy(psDstSymbol->pszName, psSrcSymbol->pszName, ui32DataSize);

		/* Copy the list of substructure symbols */
		if(psSrcSymbol->uNumBaseTypeMembers)
		{
			GLSLBindingSymbol *psSrcBaseSymbol, *psDstBaseSymbol;

			ui32DataSize = psSrcSymbol->uNumBaseTypeMembers * sizeof(GLSLBindingSymbol);

			psDstSymbol->psBaseTypeMembers = GLES2Malloc(gc, ui32DataSize);

			if(!psDstSymbol->psBaseTypeMembers)
			{
				GLES2Free(IMG_NULL, psDstSymbol->pszName);
				goto CleanupSymbols;
			}

			GLES2MemCopy(psDstSymbol->psBaseTypeMembers, psSrcSymbol->psBaseTypeMembers, ui32DataSize);

			for(j=0; j < psSrcSymbol->uNumBaseTypeMembers; j++)
			{
				psSrcBaseSymbol = &psSrcSymbol->psBaseTypeMembers[j];
				psDstBaseSymbol = &psDstSymbol->psBaseTypeMembers[j];

				ui32DataSize = strlen(psSrcBaseSymbol->pszName) + 1;

				psDstBaseSymbol->pszName = GLES2Malloc(gc, ui32DataSize);

				if(!psDstBaseSymbol->pszName)
				{
					while(j)
					{
						j--;
						GLES2Free(IMG_NULL, psDstSymbol->psBaseTypeMembers[j].pszName);
					}
					GLES2Free(IMG_NULL, psDstSymbol->psBaseTypeMembers);
					GLES2Free(IMG_NULL, psDstSymbol->pszName);
					goto CleanupSymbols;
				}

				GLES2MemCopy(psDstBaseSymbol->pszName, psSrcBaseSymbol->pszName, ui32DataSize);
			}
		}
	}

	return IMG_TRUE;

CleanupSymbols:
	while(i)
	{
		i--;
		psDstSymbol = &psDstList->psBindingSymbolEntries[i];

		for(j=0; j < psDstSymbol->uNumBaseTypeMembers; j++)
		{
			GLES2Free(IMG_NULL, psDstSymbol->psBaseTypeMembers[j].pszName);
		}

		if(psDstSymbol->uNumBaseTypeMembers)
		{
			GLES2Free(IMG_NULL, psDstSymbol->psBaseTypeMembers);
		}

		GLES2Free(IMG_NULL, psDstSymbol->pszName);
	}

	GLES2Free(IMG_NULL, psDstList->psBindingSymbolEntries);

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : CreateSharedShaderState
 Inputs             : gc, psCompiledProgram
 Outputs            : -
 Returns            : Shared Shader State
 Description        : Creates the state which is shared between a compiled shader and
					  the programs it may be attached to.
************************************************************************************/
static GLES2SharedShaderState * CreateSharedShaderState(GLES2Context *gc,
														GLSLCompiledUniflexProgram *psCompiledProgram)
{
	GLES2SharedShaderState *psSharedState;
	GLSLUniFlexCode *psUFCode = psCompiledProgram->psUniFlexCode;
	IMG_UINT32 ui32DataSize;

	psSharedState = (GLES2SharedShaderState *) GLES2Calloc(gc, sizeof(GLES2SharedShaderState));

	if(!psSharedState) 
	{
		return IMG_NULL;
	}

	psSharedState->ui32RefCount = 1;

	psSharedState->pvUniPatchShader = PVRUniPatchCreateShader(gc->sProgram.pvUniPatchContext, psCompiledProgram->psUniFlexCode->psUniPatchInput);

	if(!psSharedState->pvUniPatchShader)
	{
		/* UniPatch didn't like the input */
		PVR_DPF((PVR_DBG_ERROR,"CreateSharedShaderState: UniPatch failed to create a shader"));
		GLES2Free(IMG_NULL, psSharedState);
		return IMG_NULL;
	}

#if !defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* Unconditionally create the MSAA trans version of the shader, in case it is used with a MSAA surface 
	 * after being compiled while a non-MSAA surface is bound.
	 */
	if(psCompiledProgram->eProgramType == GLSLPT_FRAGMENT)
	{
		psSharedState->pvUniPatchShaderMSAATrans = PVRUniPatchCreateShader(gc->sProgram.pvUniPatchContext, psCompiledProgram->psUniFlexCode->psUniPatchInputMSAATrans);

		if(!psSharedState->pvUniPatchShaderMSAATrans)
		{
			/* UniPatch didn't like the input */
			PVR_DPF((PVR_DBG_ERROR,"CreateSharedShaderState: UniPatch failed to create an MSAA shader"));
			PVRUniPatchDestroyShader(gc->sProgram.pvUniPatchContext, psSharedState->pvUniPatchShader);
			GLES2Free(IMG_NULL, psSharedState);
			return IMG_NULL;
		}
	}
#endif

	psSharedState->sBindingSymbolList = *psCompiledProgram->psBindingSymbolList;

	/* Depth textures unsupported */
	GLES_ASSERT(psSharedState->sBindingSymbolList.uNumDepthTextures == 0);

	ui32DataSize = psCompiledProgram->psBindingSymbolList->uNumCompsUsed * sizeof(IMG_FLOAT);

	if(ui32DataSize)
	{
		psSharedState->sBindingSymbolList.pfConstantData = GLES2Calloc(gc, ui32DataSize);

		if(psSharedState->sBindingSymbolList.pfConstantData)
		{
			GLES2MemCopy(psSharedState->sBindingSymbolList.pfConstantData, 
						psCompiledProgram->psBindingSymbolList->pfConstantData, 
						ui32DataSize);
		}
		else
		{
			PVRUniPatchDestroyShader(gc->sProgram.pvUniPatchContext, psSharedState->pvUniPatchShader);

			if(psSharedState->pvUniPatchShaderMSAATrans)
			{
				PVRUniPatchDestroyShader(gc->sProgram.pvUniPatchContext, psSharedState->pvUniPatchShaderMSAATrans);
			}

			GLES2Free(IMG_NULL, psSharedState);
			return IMG_NULL;
		}
	}

	if(CopySymbolBindings(gc, &psSharedState->sBindingSymbolList,
							psCompiledProgram->psBindingSymbolList) != IMG_TRUE)
	{
		if(psSharedState->sBindingSymbolList.uNumCompsUsed)
		{
			GLES2Free(IMG_NULL, psSharedState->sBindingSymbolList.pfConstantData);
		}

		PVRUniPatchDestroyShader(gc->sProgram.pvUniPatchContext, psSharedState->pvUniPatchShader);

		if(psSharedState->pvUniPatchShaderMSAATrans)
		{
			PVRUniPatchDestroyShader(gc->sProgram.pvUniPatchContext, psSharedState->pvUniPatchShaderMSAATrans);
		}

		GLES2Free(IMG_NULL, psSharedState);
		return IMG_NULL;
	}

	GLES2MemCopy(psSharedState->aeTexCoordPrecisions, psUFCode->aeTexCoordPrecisions, sizeof(psUFCode->aeTexCoordPrecisions));
	GLES2MemCopy(psSharedState->aui32TexCoordDims, psUFCode->auTexCoordDims, sizeof(psUFCode->auTexCoordDims));
	psSharedState->eActiveVaryingMask = psUFCode->eActiveVaryingMask;
	psSharedState->eProgramFlags = psCompiledProgram->eProgramFlags;

	return psSharedState;
}

#endif

/***********************************************************************************
 Function Name      : GetNamedProgram
 Inputs             : gc, program
 Outputs            : -
 Returns            : psProgram
 Description        : UTILTIY: Finds and checks a program name
************************************************************************************/
IMG_INTERNAL GLES2Program *GetNamedProgram(GLES2Context *gc, GLuint program)
{
	GLES2NamesArray *psNamesArray;
	GLES2Program *psProgram;

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM];

	/* Retrieve the program object from the psNamesArray structure. */
	/* Improvement: instead of increasing the refcount just to decrease it immediately, we should have
	          one more function for name arrays that returns whether the object exists without increasing the
	          refcount. That function would also be used in glIsShader, glIsTexture, etc.
	*/
	psProgram = (GLES2Program *) NamedItemAddRef(psNamesArray, program);

	if(psProgram == IMG_NULL)
	{
		SetError(gc, GL_INVALID_VALUE);
		return IMG_NULL;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psProgram);

	if(psProgram->ui32Type != GLES2_SHADERTYPE_PROGRAM)
	{
		SetError(gc, GL_INVALID_OPERATION);
		return IMG_NULL;
	}

	return psProgram;
}

/***********************************************************************************
 Function Name      : GetNamedShader
 Inputs             : gc, shader
 Outputs            : -
 Returns            : psShader
 Description        : UTILTIY: Finds and checks a shader name
************************************************************************************/
IMG_INTERNAL GLES2Shader *GetNamedShader(GLES2Context *gc, GLuint shader)
{
	GLES2NamesArray *psNamesArray;
	GLES2Shader *psShader;

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM];

	/* Retrieve the shader object from the psNamesArray structure. */
	psShader = (GLES2Shader *) NamedItemAddRef(psNamesArray, shader);

	if(psShader == IMG_NULL)
	{
		SetError(gc, GL_INVALID_VALUE);
		return IMG_NULL;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psShader);

	if(psShader->ui32Type == GLES2_SHADERTYPE_PROGRAM)
	{
		SetError(gc, GL_INVALID_OPERATION);
		return IMG_NULL;
	}

	return psShader;
}

/***********************************************************************************
 Function Name      : glAttachShader
 Inputs             : program, shader
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Attaches a shader to a program 
************************************************************************************/
GL_APICALL void GL_APIENTRY glAttachShader(GLuint program, GLuint shader)
{
	GLES2Program *psProgram;
	GLES2Shader *psShader;
	GLES2NamesArray *psNamesArray;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glAttachShader"));

	GLES2_TIME_START(GLES2_TIMES_glAttachShader);

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glAttachShader);
		return;
	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM];
	
	/*
	** Retrieve the shader object from the psNamesArray structure.
	** This will increment the refcount as we require, so do not unlock at the end.
	*/
	psShader = (GLES2Shader *) NamedItemAddRef(psNamesArray, shader);

	if(psShader == IMG_NULL)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glAttachShader);
		return;
	}

	/* Check whether this shader has been attached to this program */ 
	switch(psShader->ui32Type)
	{
		case GLES2_SHADERTYPE_VERTEX:
		{
			if(psProgram->psVertexShader)
			{
				NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psShader);

				SetError(gc, GL_INVALID_OPERATION);
				GLES2_TIME_STOP(GLES2_TIMES_glAttachShader);
				return;
			}
			
			psProgram->psVertexShader = psShader;

#if defined(DEBUG)
			if(gc->pShaderAnalysisHandle)
			{
				fprintf(gc->pShaderAnalysisHandle, "Attach Vertex Shader %u to Program %u\n\n",
						psShader->sNamedItem.ui32Name, psProgram->sNamedItem.ui32Name);
			}
#endif

			break;
		}
		case GLES2_SHADERTYPE_FRAGMENT:
		{
			if(psProgram->psFragmentShader)
			{
				NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psShader);

				SetError(gc, GL_INVALID_OPERATION);
				GLES2_TIME_STOP(GLES2_TIMES_glAttachShader);
				return;
			}
			
			psProgram->psFragmentShader = psShader;

#if defined(DEBUG)
			if(gc->pShaderAnalysisHandle)
			{
				fprintf(gc->pShaderAnalysisHandle, "Attach Fragment Shader %u to Program %u\n\n",
						psShader->sNamedItem.ui32Name, psProgram->sNamedItem.ui32Name);
			}
#endif

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_OPERATION);
			GLES2_TIME_STOP(GLES2_TIMES_glAttachShader);
			return;
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glAttachShader);
}

/***********************************************************************************
 Function Name      : glDetachShader
 Inputs             : program, shader
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Detaches a shader from a program (and deletes it if
					  marked for deletion and not bound to any other program).
************************************************************************************/
GL_APICALL void GL_APIENTRY glDetachShader(GLuint program, GLuint shader)
{
	GLES2Program *psProgram;
	GLES2Shader *psShader;
	GLES2NamesArray *psNamesArray;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDetachShader"));

	GLES2_TIME_START(GLES2_TIMES_glDetachShader);

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM];

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		goto StopTimeAndReturn;
	}

	psShader = GetNamedShader(gc, shader);

	if(psShader == IMG_NULL)
	{
		goto StopTimeAndReturn;
	}

	/* Check whether this shader has been attached to this program */ 
	switch(psShader->ui32Type)
	{
		case GLES2_SHADERTYPE_VERTEX:
		{
			if(psProgram->psVertexShader && psProgram->psVertexShader->sNamedItem.ui32Name == shader)
			{
				psProgram->psVertexShader = IMG_NULL;
			}
			else
			{
				SetError(gc, GL_INVALID_OPERATION);
				goto StopTimeAndReturn;
			}
			break;
		}
		case GLES2_SHADERTYPE_FRAGMENT:
		{
			if(psProgram->psFragmentShader && psProgram->psFragmentShader->sNamedItem.ui32Name == shader)
			{
				psProgram->psFragmentShader = IMG_NULL;
			}
			else
			{
				SetError(gc, GL_INVALID_OPERATION);
				goto StopTimeAndReturn;
			}
			break;	
		}
		default:
		{
			SetError(gc, GL_INVALID_VALUE);
			goto StopTimeAndReturn;
		}
	}

	/* Unbind the shader name and delete the shader if its not being referenced by anyone else */
	NamedItemDelRef(gc, psNamesArray, &psShader->sNamedItem);

StopTimeAndReturn:
	GLES2_TIME_STOP(GLES2_TIMES_glDetachShader);
}

/***********************************************************************************
 Function Name      : glBindAttribLocation
 Inputs             : program, index, name
 Outputs            : None
 Returns            : None
 Description        : ENTRYPOINT: Binds a named attribute to an index
                      
************************************************************************************/
GL_APICALL void GL_APIENTRY glBindAttribLocation(GLuint program, GLuint index, const char *name)
{
	GLES2Program           *psProgram;
	GLSLAttribUserBinding *psBinding;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBindAttribLocation"));

	GLES2_TIME_START(GLES2_TIMES_glBindAttribLocation);

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glBindAttribLocation);
		return;
	}

	if (index >= GLES2_MAX_VERTEX_ATTRIBS)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndReturn;
	}

	/* gl_ prefix is reserved */
	if(!strncmp(name, "gl_", 3))
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto StopTimeAndReturn;
	}

	/* If 'name' has been bound before, overwrite it */
	psBinding = psProgram->psUserBinding;
	while(psBinding)
	{
		if(!strcmp(psBinding->pszName, name))
		{
			psBinding->i32Index = (IMG_INT32)index;
			goto StopTimeAndReturn;
		}
		psBinding = psBinding->psNext;
	}

	/* Add an entry to the usder binding list for use in LinkProgram */
	psBinding = GLES2Calloc(gc, sizeof(GLSLAttribUserBinding));
	
	if(!psBinding)
	{
		SetError(gc, GL_OUT_OF_MEMORY);
		goto StopTimeAndReturn;
	}

	psBinding->pszName = GLES2Calloc(gc, strlen(name) + 1);

	if(!psBinding->pszName)
	{
		SetError(gc, GL_OUT_OF_MEMORY);
		goto StopTimeAndReturn;
	}

	strcpy(psBinding->pszName, name);
	psBinding->i32Index = (IMG_INT32)index;
	psBinding->psNext = IMG_NULL;

	if(psProgram->psUserBinding)
	{
		psProgram->psLastUserBinding->psNext = psBinding;
	}
	else
	{
		psProgram->psUserBinding = psBinding;
	}
	psProgram->psLastUserBinding = psBinding;

StopTimeAndReturn:
	GLES2_TIME_STOP(GLES2_TIMES_glBindAttribLocation);
}


/***********************************************************************************
 Function Name      : CreateProgram
 Inputs             : gc, ui32Name
 Outputs            : -
 Returns            : Pointer to new program structure
 Description        : Creates and initialises a new program structure.
************************************************************************************/
static GLES2Program *CreateProgram(GLES2Context *gc, IMG_UINT32 ui32Name)
{
	GLES2Program *psProgram;

	PVR_UNREFERENCED_PARAMETER(gc);

	psProgram = (GLES2Program *) GLES2Calloc(gc, sizeof(GLES2Program));

	if(psProgram) 
	{
		psProgram->sNamedItem.ui32Name = ui32Name;
		psProgram->ui32Type = GLES2_SHADERTYPE_PROGRAM;
		
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
		psProgram->bLoadFromBinary = IMG_FALSE;
#endif

		psProgram->sVertex.eProgramType   = GLSLPT_VERTEX;
		psProgram->sVertex.pfConstantData	= IMG_NULL;

		psProgram->sFragment.eProgramType = GLSLPT_FRAGMENT;
		psProgram->sFragment.pfConstantData	= IMG_NULL;
	}

	return psProgram;
}

/***********************************************************************************
 Function Name      : glCreateProgram
 Inputs             : 
 Outputs            : -
 Returns            : Program Name
 Description        : ENTRYPOINT: Creates and initialises a new program structure.
************************************************************************************/
GL_APICALL GLuint GL_APIENTRY glCreateProgram(void)
{
	IMG_UINT32   ui32ProgramName = 0;
	GLES2Program *psProgram;

	__GLES2_GET_CONTEXT_RETURN(0);
	PVR_DPF((PVR_DBG_CALLTRACE,"glCreateProgram"));

	GLES2_TIME_START(GLES2_TIMES_glCreateProgram);

	/* Get a name for the object */
	NamesArrayGenNames(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM], 1, &ui32ProgramName);

	GLES_ASSERT(ui32ProgramName);

	psProgram = CreateProgram(gc, ui32ProgramName);
	
	if(psProgram == IMG_NULL)
	{
		SetError(gc, GL_OUT_OF_MEMORY);
		GLES2_TIME_STOP(GLES2_TIMES_glCreateProgram);
		return 0;
	}

	if(!InsertNamedItem(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM], (GLES2NamedItem*)psProgram))
	{
		gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM]->pfnFree(gc, (GLES2NamedItem*)psProgram, IMG_TRUE);
	
		SetError(gc, GL_OUT_OF_MEMORY);
		GLES2_TIME_STOP(GLES2_TIMES_glCreateProgram);
		return 0;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glCreateProgram);
	return ui32ProgramName;
}


/***********************************************************************************
 Function Name      : glDeleteProgram
 Inputs             : program
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Deletes a program and unbinds any attached shaders
************************************************************************************/
GL_APICALL void GL_APIENTRY glDeleteProgram(GLuint program)
{
	GLES2Program *psProgram;
	GLES2NamesArray *psNamesArray;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDeleteProgram"));

	GLES2_TIME_START(GLES2_TIMES_glDeleteProgram);

	/* Spec says to silently ignore 0 */
	if(!program)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteProgram);
		return;
	}

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteProgram);
		return;
	}

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM];

	GLES_ASSERT(psProgram->sNamedItem.ui32RefCount >= 1);

	if(!psProgram->bDeleting)
	{
		/* FIXME: Race condition */
		psProgram->bDeleting = IMG_TRUE;
		NamedItemDelRef(gc, psNamesArray, &psProgram->sNamedItem);
	}

	GLES2_TIME_STOP(GLES2_TIMES_glDeleteProgram);
}


/***********************************************************************************
 Function Name      : CreateShader
 Inputs             : gc, ui32Name
 Outputs            : -
 Returns            : Pointer to new program structure
 Description        : Creates and initialises a new program structure.
************************************************************************************/
static GLES2Shader *CreateShader(GLES2Context *gc, IMG_UINT32 ui32Name, IMG_UINT32 ui32Type)
{
	GLES2Shader *psShader;

	PVR_UNREFERENCED_PARAMETER(gc);

	psShader = (GLES2Shader *) GLES2Calloc(gc, sizeof(GLES2Shader));

	if (psShader == IMG_NULL) 
	{
		return IMG_NULL;
	}

	psShader->sNamedItem.ui32Name = ui32Name;
	psShader->ui32Type = ui32Type;

	return psShader;
}

/***********************************************************************************
 Function Name      : glCreateShader
 Inputs             : type
 Outputs            : -
 Returns            : Shader Name
 Description        : ENTRYPOINT: Creates and initialises a new shader structure.
************************************************************************************/
GL_APICALL GLuint GL_APIENTRY glCreateShader(GLenum type)
{
	IMG_UINT32 ui32ShaderName;
	IMG_UINT32 ui32Type;
	GLES2Shader *psShader;

	__GLES2_GET_CONTEXT_RETURN(0);
	PVR_DPF((PVR_DBG_CALLTRACE,"glCreateShader"));

	GLES2_TIME_START(GLES2_TIMES_glCreateShader);

	switch(type)
	{
		case GL_VERTEX_SHADER:
			ui32Type = GLES2_SHADERTYPE_VERTEX;
			break;
		case GL_FRAGMENT_SHADER:
			ui32Type = GLES2_SHADERTYPE_FRAGMENT;
			break;
		default:
			SetError(gc, GL_INVALID_ENUM);
			GLES2_TIME_STOP(GLES2_TIMES_glCreateShader);
			return 0;
	}

	/* Get a name for the object */
	NamesArrayGenNames(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM], 1, (IMG_UINT32*)&ui32ShaderName);

	GLES_ASSERT(ui32ShaderName);

	psShader = CreateShader(gc, ui32ShaderName, ui32Type);

	if(psShader == IMG_NULL)
	{
		SetError(gc, GL_OUT_OF_MEMORY);
		GLES2_TIME_STOP(GLES2_TIMES_glCreateShader);
		return 0;
	}

	if(!InsertNamedItem(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM], (GLES2NamedItem*)psShader))
	{
		gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM]->pfnFree(gc, (GLES2NamedItem*)psShader, IMG_TRUE);
	
		SetError(gc, GL_OUT_OF_MEMORY);
		GLES2_TIME_STOP(GLES2_TIMES_glCreateShader);
		return 0;
	}

	GLES2_TIME_STOP(GLES2_TIMES_glCreateShader);
	return ui32ShaderName;
}

/***********************************************************************************
 Function Name      : glDeleteShader
 Inputs             : shader
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Deletes a shader if it is not bound to any programs
					  otherwise, marks for deletion
************************************************************************************/
GL_APICALL void GL_APIENTRY glDeleteShader(GLuint shader)
{
	GLES2Shader *psShader;
	GLES2NamesArray *psNamesArray;

	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glDeleteShader"));

	GLES2_TIME_START(GLES2_TIMES_glDeleteShader);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM];

	/* Spec says to silently ignore 0 */
	if(shader == 0)
	{
		return;
	}

	psShader = GetNamedShader(gc, shader);

	if(psShader)
	{
		GLES_ASSERT(psShader->sNamedItem.ui32RefCount >= 1);

		if(!psShader->bDeleting)
		{
			/* FIXME: Race condition */
			psShader->bDeleting = IMG_TRUE;
			NamedItemDelRef(gc, psNamesArray, &psShader->sNamedItem);
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glDeleteShader);
}

/***********************************************************************************
 Function Name      : glGetActiveAttrib
 Inputs             : program, index, bufsize
 Outputs            : length, size, type, name
 Returns            : None
 Description        : ENTRYPOINT: Gets information about the active attributes in a 
					  program
************************************************************************************/
GL_APICALL void GL_APIENTRY glGetActiveAttrib(GLuint program, GLuint index, GLsizei bufsize, 
										  GLsizei *length, GLint *size, GLenum *type, char *name)
{
	GLES2Program *psProgram;
	GLES2Attribute *psAttrib;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetActiveAttrib"));

	GLES2_TIME_START(GLES2_TIMES_glGetActiveAttrib);

	if (bufsize < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGetActiveAttrib);
		return;
	}

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGetActiveAttrib);
		return;
	}

	if(index >= (GLuint)psProgram->ui32NumActiveAttribs)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGetActiveAttrib);
		return;
	}

	psAttrib = &psProgram->psActiveAttributes[index];

	if(strlen(psAttrib->psSymbolVP->pszName) < (GLuint)bufsize)
	{
		strcpy(name, psAttrib->psSymbolVP->pszName);
	}
	else
	{
		if (bufsize > 0)
		{
			GLES2MemCopy(name, psAttrib->psSymbolVP->pszName, (IMG_UINT32)(bufsize - 1));
			name[bufsize - 1] = 0;
		}
	}

	if(length)
	{
		*length = (GLsizei)strlen(name);
	}

	*size = psAttrib->psSymbolVP->iActiveArraySize;

	*type = ConvertGLSLtoGLType(psAttrib->psSymbolVP->eTypeSpecifier);
	
	GLES2_TIME_STOP(GLES2_TIMES_glGetActiveAttrib);
}

#if defined(SUPPORT_SOURCE_SHADER)

/***********************************************************************************
 Function Name      : LoadCompilerModule
 Inputs             : gc
 Outputs            : gc->sProgram.hGLSLCompiler, gc->sProgram.sGLSLFuncTable
 Returns            : IMG_TRUE if successfull. IMG_FALSE otherwise.
 Description        : Loads the GLSL compiler library module.
                      Sets up the function pointers in gc->psSharedState->sGLSLFuncTable.
                      The function may be called multiple times, even if the compiler module
                      is already initialized.
************************************************************************************/
static IMG_BOOL LoadCompilerModule(GLES2Context *gc)
{
	IMG_HANDLE              hGLSLCompiler;
	GLES2CompilerFuncTable  sFuncTable;

	if(gc->sProgram.hGLSLCompiler)
	{
		/* It looks like the shader compiler has already been initialized */
		return IMG_TRUE;
	}

	/* Load the module */
	hGLSLCompiler = 1;

	/* Get the function pointers from the module */
	GLES2MemSet(&sFuncTable, 0, sizeof(GLES2CompilerFuncTable));

	sFuncTable.pfnInitCompiler = GLSLInitCompiler;
	sFuncTable.pfnCompileToUniflex = GLSLCompileToUniflex;
	sFuncTable.pfnFreeCompiledUniflexProgram = GLSLFreeCompiledUniflexProgram;
	sFuncTable.pfnDisplayMetrics = GLSLDisplayMetrics;
	sFuncTable.pfnShutDownCompiler = GLSLShutDownCompiler;
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
	sFuncTable.pfnCreateBinaryProgram = SGXBS_CreateBinaryProgram;
#endif 
#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE)
	sFuncTable.pfnCreateBinaryShader = SGXBS_CreateBinaryShader;
#endif
	if(!(sFuncTable.pfnInitCompiler && sFuncTable.pfnCompileToUniflex &&
	     sFuncTable.pfnFreeCompiledUniflexProgram && sFuncTable.pfnDisplayMetrics && sFuncTable.pfnShutDownCompiler
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
	     && sFuncTable.pfnCreateBinaryProgram
#endif 
#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE)
	     && sFuncTable.pfnCreateBinaryShader
#endif 
		 ))
	{
		PVR_DPF((PVR_DBG_ERROR, "LoadCompilerModule: Some of the function pointers could not be retrieved."));
		return IMG_FALSE;
	}

	/* Everything worked alright. Update the handle in the shared state */
	gc->sProgram.hGLSLCompiler  = hGLSLCompiler;
	gc->sProgram.sGLSLFuncTable = sFuncTable;

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : UnloadCompilerModule
 Inputs             : gc
 Outputs            : gc->sProramState.hGLSLCompiler, gc->sProgram.sGLSLFuncTable
 Returns            : -
 Description        : Releases the dynamically loaded GLSL compiler library.
************************************************************************************/
static IMG_VOID UnloadCompilerModule(GLES2Context *gc)
{
	if(!gc->sProgram.hGLSLCompiler)
	{
		/* The compiler has already been released */
		return;
	}

	{
		/* Reset the handler and the function pointers */
		gc->sProgram.hGLSLCompiler = 0;
		GLES2MemSet(&gc->sProgram.sGLSLFuncTable, 0, sizeof(GLES2CompilerFuncTable));
	}
}

/***********************************************************************************
 Function Name      : SetPrecision
 Inputs             : ui32PrecisionBitMask
 Outputs            : psRP
 Returns            : -
 Description        : Sets the default precisions for different data types and
                      builtin variables in GLSL.
************************************************************************************/
static IMG_VOID SetPrecision(GLSLRequestedPrecisions *psRP, IMG_UINT32 ui32PrecisionBitMask)
{
	/* Defaults for user defined data */
	psRP->eDefaultUserVertFloat   = GLSLPRECQ_HIGH;
	psRP->eDefaultUserVertInt     = GLSLPRECQ_HIGH;
	psRP->eDefaultUserVertSampler = GLSLPRECQ_LOW;
	psRP->eDefaultUserFragFloat   = GLSLPRECQ_UNKNOWN;
	psRP->eDefaultUserFragInt     = GLSLPRECQ_MEDIUM;
	psRP->eDefaultUserFragSampler = GLSLPRECQ_LOW;

	psRP->eVertBooleanPrecision	  = GLSLPRECQ_HIGH;
	psRP->eFragBooleanPrecision	  = GLSLPRECQ_HIGH;

	/* Built in data */
	psRP->eBIStateInt             = GLSLPRECQ_HIGH;
	psRP->eBIFragFloat            = GLSLPRECQ_MEDIUM;

	/* Specials */
	psRP->eGLPosition             = GLSLPRECQ_HIGH;
	psRP->eGLPointSize            = GLSLPRECQ_MEDIUM;
	psRP->eGLPointCoord           = GLSLPRECQ_MEDIUM;
	psRP->eDepthRange             = GLSLPRECQ_HIGH;

	/* Disable forcing of precision */
	psRP->eForceUserVertFloat     = GLSLPRECQ_UNKNOWN;
	psRP->eForceUserVertInt       = GLSLPRECQ_UNKNOWN;
	psRP->eForceUserVertSampler   = GLSLPRECQ_UNKNOWN;
	psRP->eForceUserFragFloat     = GLSLPRECQ_UNKNOWN;
	psRP->eForceUserFragInt       = GLSLPRECQ_UNKNOWN;
	psRP->eForceUserFragSampler   = GLSLPRECQ_UNKNOWN;

	/* Not used for ES */
	psRP->eBIStateFloat           = GLSLPRECQ_UNKNOWN;
	psRP->eBIVertAttribFloat      = GLSLPRECQ_UNKNOWN;
	psRP->eBIVaryingFloat         = GLSLPRECQ_UNKNOWN;

	if (ui32PrecisionBitMask)
	{
		SET_BITFIELD_REQUESTED_PRECISION(psRP, ui32PrecisionBitMask);	/* PRQA S 1482 */ /* Any possible value of expression here can be casted to a valid enum. */
		if(ui32PrecisionBitMask == 0xFFFFFFFF)
		{
			psRP->eDefaultUserVertSampler = GLSLPRECQ_HIGH;
			psRP->eDefaultUserFragSampler = GLSLPRECQ_HIGH;
			psRP->eForceUserVertSampler = GLSLPRECQ_HIGH;
			psRP->eForceUserFragSampler = GLSLPRECQ_HIGH;
		}
	}
}

#if defined(DEBUG)

static IMG_VOID PrintShaders(const IMG_CHAR* pszFmt, ...) IMG_FORMAT_PRINTF(1, 2);
static IMG_VOID PrintShaders(const IMG_CHAR* pszFmt, ...)
{
	IMG_CHAR pszTemp[256];
	va_list ap;

	__GLES2_GET_CONTEXT();

	va_start(ap, pszFmt);
	vsprintf(pszTemp, pszFmt, ap);
	strcat(pszTemp, "\n");
	va_end(ap);

	fprintf(gc->pShaderAnalysisHandle, "%s", pszTemp);
}

#endif /* defined(DEBUG) */

/***********************************************************************************
 Function Name      : InitializeGLSLCompiler
 Inputs             : gc, psInitCompilerContext
 Outputs            : -
 Returns            : Success
 Description        : Initialises the GLSL compiler.
************************************************************************************/
IMG_INTERNAL IMG_BOOL InitializeGLSLCompiler(GLES2Context *gc)
{
	GLSLInitCompilerContext *psInitCompilerContext = &gc->sProgram.sInitCompilerContext;
	GLSLCompilerResources *psResources;

	if(gc->sProgram.hGLSLCompiler)
	{
		/* The compiler was already initialized */
		PVR_DPF((PVR_DBG_WARNING, "InitializeGLSLCompiler: The compiler was already initialized\n"));
		return IMG_TRUE;
	}

	/* Load the dynamic library */
	if(!LoadCompilerModule(gc))
	{
		return IMG_FALSE;
	}

	/* Reset to zeroes as recommended by the compiler documentation */
	GLES2MemSet(psInitCompilerContext, (IMG_UINT8)0, sizeof(GLSLInitCompilerContext));

	if(gc->sAppHints.bDumpCompilerLogFiles)
	{
		psInitCompilerContext->eLogFiles = GLSLLF_LOG_ALL;
	}
	else
	{
		psInitCompilerContext->eLogFiles = GLSLLF_NOT_LOG;
	}

#if defined(DEBUG)
	if(gc->pShaderAnalysisHandle)
	{
		psInitCompilerContext->pfnPrintShaders = PrintShaders;
	}

	psInitCompilerContext->bEnableUSCMemoryTracking = gc->sAppHints.bTrackUSCMemory;
#endif

	psResources = &psInitCompilerContext->sCompilerResources;
	psResources->iGLMaxVertexAttribs = GLES2_MAX_VERTEX_ATTRIBS;
	psResources->iGLMaxVertexUniformVectors = GLES2_MAX_VERTEX_UNIFORM_VECTORS;
	psResources->iGLMaxVaryingVectors = GLES2_MAX_VARYING_VECTORS;
	psResources->iGLMaxVertexTextureImageUnits = GLES2_MAX_VERTEX_TEXTURE_UNITS;
	psResources->iGLMaxCombinedTextureImageUnits = GLES2_MAX_TEXTURE_UNITS;
	psResources->iGLMaxTextureImageUnits = GLES2_MAX_TEXTURE_UNITS;
	psResources->iGLMaxFragmentUniformVectors = GLES2_MAX_FRAGMENT_UNIFORM_VECTORS;
	psResources->iGLMaxDrawBuffers = GLES2_MAX_DRAW_BUFFERS;

	/* If no apphint is set, default GLSL ES precisions are specified */
	SetPrecision(&psInitCompilerContext->sRequestedPrecisions, gc->sAppHints.ui32AdjustShaderPrecision);

	/* Set inline rules */
	psInitCompilerContext->sInlineFuncRules.bInlineCalledOnceFunc			= IMG_TRUE;	/* No penalty for setting this */
	psInitCompilerContext->sInlineFuncRules.bInlineSamplerParamFunc			= IMG_TRUE;	/* Should always be true; */
	psInitCompilerContext->sInlineFuncRules.uNumICInstrsBodyLessThan		= 10;		/* 10 intermediate instructions */
	psInitCompilerContext->sInlineFuncRules.uNumParamComponentsGreaterThan	= 32;		/* Up to 8 vec4s */

	/* Set unroll rules */
	psInitCompilerContext->sUnrollLoopRules.bEnableUnroll					= IMG_TRUE;	/* Enable unrolling */
	psInitCompilerContext->sUnrollLoopRules.bUnrollRelativeAddressingOnly	= IMG_TRUE; /* Only enable unroll if contain relative addressing */
	psInitCompilerContext->sUnrollLoopRules.uMaxNumIterations				= 50;		/* The number of iterations has to be less than */

	if(!gc->sProgram.sGLSLFuncTable.pfnInitCompiler(psInitCompilerContext))
	{
		PVR_DPF((PVR_DBG_ERROR, "InitializeGLSLCompiler: Failed to initialise the GLSL compiler !\n"));
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : DestroyGLSLCompiler
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Destroys the GLSL compiler. This is called when an app calls
                      glReleaseShaderCompiler and after the names arrays are destroyed.
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyGLSLCompiler(GLES2Context *gc)
{
	/* The compiler may have been explicitly destroyed by the app or maybe the app only used binary shaders. */
	if(gc->sProgram.hGLSLCompiler)
	{
#if defined(TIMING)
		gc->sProgram.sGLSLFuncTable.pfnDisplayMetrics(&gc->sProgram.sInitCompilerContext);
#endif
		gc->sProgram.sGLSLFuncTable.pfnShutDownCompiler(&gc->sProgram.sInitCompilerContext);
		UnloadCompilerModule(gc);
	}
}

#endif /* defined(SUPPORT_SOURCE_SHADER) */

static IMG_VOID * IMG_CALLCONV UniPatchMalloc(IMG_UINT32 ui32Size )
{
	/* IMPORTANT: if this call is not kept GL-context independent, then allocating a shader in one context
	              and freeing it in a different context will not work as we will allocate it in one heap
	              and deallocate from a different heap (think Symbian).
	*/
	return GLES2Malloc(0, ui32Size);
}

static IMG_VOID IMG_CALLCONV UniPatchFree(IMG_VOID *pvData)
{
	GLES2Free(0, pvData);
}


static IMG_VOID IMG_CALLCONV UniPatchDebugPrint(const IMG_CHAR* pszFormat, ...) IMG_FORMAT_PRINTF(1,2);

static IMG_VOID IMG_CALLCONV UniPatchDebugPrint(const IMG_CHAR* pszFormat, ...)
{
#if defined(DEBUG)
	__GLES2_GET_CONTEXT();

	if(gc->sAppHints.bDumpUSPOutput)
	{
		SceUID fd = sceIoDopen("ux0:data/gles/usp");

		if (fd <= 0)
		{
			sceIoMkdir("ux0:data/gles", 0777);
			sceIoMkdir("ux0:data/gles/usp", 0777);
		}
		else
		{
			sceIoDclose(fd);
		}


		FILE    *fstream = fopen("ux0:data/gles/usp/usp_out.txt","a+t");
		va_list vaList;

		if(fstream)
		{
			/* Write out the disassembly of the machine code generated by unipatch */
			va_start(vaList, pszFormat);
			vfprintf(fstream, pszFormat, vaList);
			va_end(vaList);
		}
		else
		{
			PVR_DPF((PVR_DBG_WARNING, "UniPatchDebugPrint: Could not open file. Cannot dump USP output"));
		}
	}
#else
	PVR_UNREFERENCED_PARAMETER(pszFormat);
#endif
}


/***********************************************************************************
 Function Name      : DestroyHashedPDSVariant
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyHashedPDSVariant(GLES2Context *gc, IMG_UINT32 ui32Item)
{
	GLES2PDSCodeVariant **ppsPDSVariantList, *psPDSVariant;
	GLES2USEShaderVariant *psUSEVariant;

	PVR_UNREFERENCED_PARAMETER(gc);

	psUSEVariant = ((GLES2PDSCodeVariant *)ui32Item)->psUSEVariant;
	ppsPDSVariantList = &psUSEVariant->psPDSVariant;

	while(*ppsPDSVariantList)
	{
		if(*ppsPDSVariantList == (GLES2PDSCodeVariant *)ui32Item)
		{
			psPDSVariant = *ppsPDSVariantList;
			*ppsPDSVariantList = psPDSVariant->psNext;
			
			UCH_CodeHeapFree(psPDSVariant->psCodeBlock);
			GLES2Free(IMG_NULL, psPDSVariant);
			return;
		}
			
		ppsPDSVariantList = &((*ppsPDSVariantList)->psNext);
	}
}


/***********************************************************************************
 Function Name      : ReclaimUSEShaderVariantMemKRM
 Inputs             : gc, psResource
 Outputs            : -
 Returns            : -
 Description        : Reclaims the device memory of a use shader variant. This function
					  also destroys the variant itself, as we are not managing a top 
					  level GL resource and we can easily create a new version of the 
					  variant.
************************************************************************************/
IMG_INTERNAL IMG_VOID ReclaimUSEShaderVariantMemKRM(IMG_VOID *pvContext, KRMResource *psResource)
{
	/* Note the tricky pointer arithmetic. It is necessary */
	GLES2USEShaderVariant *psUSEVariant = (GLES2USEShaderVariant*)((IMG_UINTPTR_T)psResource -offsetof(GLES2USEShaderVariant, sResource));
	GLES2Context	 *gc = (GLES2Context *)pvContext;

	DestroyUSEShaderVariant(gc, psUSEVariant);
}


/***********************************************************************************
 Function Name      : DestroyUSECodeVariantGhostKRM
 Inputs             : gc, psResource
 Outputs            : -
 Returns            : -
 Description        : Destroys a ghosted USSE code variant.
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyUSECodeVariantGhostKRM(IMG_VOID *pvContext, KRMResource *psResource)
{
	/* Note the tricky pointer arithmetic. It is necessary */
	GLES2USEShaderVariantGhost *psUSEVariantGhost =
		(GLES2USEShaderVariantGhost*)((IMG_UINTPTR_T)psResource -offsetof(GLES2USEShaderVariantGhost, sResource));
	GLES2Context	 *gc = (GLES2Context *)pvContext;

	DestroyUSEShaderVariantGhost(gc, psUSEVariantGhost);
}


/***********************************************************************************
 Function Name      : GhostUSEShaderVariant
 Inputs             : gc, psUSEVariant
 Outputs            : -
 Returns            : -
 Description        : Ghosts a USSE shader variant and destroys the original.
************************************************************************************/
static IMG_VOID GhostUSEShaderVariant(GLES2Context *gc, GLES2USEShaderVariant *psUSEVariant)
{
	GLES2USEShaderVariantGhost *psUSEVariantGhost;
	GLES2PDSCodeVariant        *psPDSVariant;
	GLES2PDSCodeVariantGhost   *psPDSVariantGhost;

	/* We only ghost fragment shaders. For vertex shaders we kick the TA and wait */
	GLES_ASSERT(psUSEVariant->psProgramShader && psUSEVariant->psProgramShader->eProgramType == GLSLPT_FRAGMENT);

	psUSEVariantGhost = GLES2Calloc(gc, sizeof(GLES2USEShaderVariantGhost));

	if(!psUSEVariantGhost)
	{
		PVR_DPF((PVR_DBG_ERROR, "GhostUSEShaderVariant: Out of memory. Could not ghost USE variant at %p\n", psUSEVariant));

		SetError(gc, GL_OUT_OF_MEMORY);

		return;
	}

	/* Transfer ownership of the USE code block from the variant to the ghost */
	psUSEVariantGhost->psUSECodeBlock  = psUSEVariant->psCodeBlock;
	psUSEVariant->psCodeBlock          = IMG_NULL;

	USESecondaryUploadTaskAddRef(gc, psUSEVariant->psSecondaryUploadTask);
	psUSEVariantGhost->psSecondaryUploadTask = psUSEVariant->psSecondaryUploadTask;


	psUSEVariantGhost->psScratchMem = psUSEVariant->psScratchMem;
	ShaderScratchMemAddRef(gc, psUSEVariantGhost->psScratchMem);

	psUSEVariantGhost->psIndexableTempsMem = psUSEVariant->psIndexableTempsMem;
	ShaderIndexableTempsMemAddRef(gc, psUSEVariantGhost->psIndexableTempsMem);


	psPDSVariant = psUSEVariant->psPDSVariant;

	/* Transfer ownership of the PDS code blocks from the PDS variants to the PDS ghosts */
	while(psPDSVariant)
	{
		psPDSVariantGhost = GLES2Calloc(gc, sizeof(GLES2PDSCodeVariantGhost));
		
		if(!psPDSVariantGhost)
		{
			DestroyUSEShaderVariantGhost(gc, psUSEVariantGhost);

			PVR_DPF((PVR_DBG_ERROR, "GhostUSEShaderVariant: Out of memory. Could not ghost USE variant at %p\n", psUSEVariant));

			SetError(gc, GL_OUT_OF_MEMORY);

			return;
		}

		psPDSVariantGhost->psCodeBlock = psPDSVariant->psCodeBlock;
		psPDSVariant->psCodeBlock = IMG_NULL;

		/* Insert the PDS variant ghost at the front of the list */
		psPDSVariantGhost->psNext = psUSEVariantGhost->psPDSVariantGhost;
		psUSEVariantGhost->psPDSVariantGhost = psPDSVariantGhost;

		psPDSVariant = psPDSVariant->psNext;
	}

	/* The creation of the ghost was successfull. Notify the KRM */
	KRM_GhostResource(&gc->psSharedState->sUSEShaderVariantKRM, 
							&psUSEVariant->sResource, &psUSEVariantGhost->sResource);

	/* Finally, destroy the original */
	DestroyUSEShaderVariant(gc, psUSEVariant);

	return;
}


/***********************************************************************************
 Function Name      : FreeListOfVertexUSEVariants
 Inputs             : gc
                      ppsUSEVariantListHead - The head of the list of variants
 Outputs            : 
 Returns            : 
 Description        : Frees a linked list of vertex USE variants and their attached PDS variants.
************************************************************************************/
static IMG_VOID FreeListOfVertexUSEVariants(GLES2Context *gc, GLES2USEShaderVariant **ppsUSEVariantListHead)
{
	GLES2USEShaderVariant     *psUSEVariant, *psUSEVariantNext;

	psUSEVariant = *ppsUSEVariantListHead;

	if(psUSEVariant)
	{
		if(gc->psRenderSurface)
		{
			/* Before relinking kick the TA and wait. Then free the vertex variants. */
			if(ScheduleTA(gc, gc->psRenderSurface, GLES2_SCHEDULE_HW_WAIT_FOR_TA) != IMG_EGL_NO_ERROR)
			{
				PVR_DPF((PVR_DBG_ERROR, "FreeListOfVertexUSEVariants: Kicking the TA failed\n"));
				return;
			}
		}

		do
		{
			psUSEVariantNext = psUSEVariant->psNext;

			DestroyUSEShaderVariant(gc, psUSEVariant);

			if(gc->sProgram.psCurrentVertexVariant==psUSEVariant)
			{
				gc->sProgram.psCurrentVertexVariant=IMG_NULL;
			}	

			/* Keep the list consistent as DestroyUSEShaderVariant requires it */
			psUSEVariant = psUSEVariantNext;

			*ppsUSEVariantListHead = psUSEVariant;
		}
		while(psUSEVariant);
	}
}


/***********************************************************************************
 Function Name      : DestroyVertexVariants
 Inputs             : gc, psNamedItem
 Outputs            : -
 Returns            : -
 Description        : Deletes all vertex variants for a given program.
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyVertexVariants(GLES2Context *gc, const IMG_VOID* pvAttachment, GLES2NamedItem *psNamedItem)
{
	GLES2Program *psProgram = (GLES2Program*)psNamedItem;

	PVR_UNREFERENCED_PARAMETER(pvAttachment);

	GLES_ASSERT(psProgram);

	if(psProgram->ui32Type == GLES2_SHADERTYPE_PROGRAM)
	{
		FreeListOfVertexUSEVariants(gc, &psProgram->sVertex.psVariant);
	}
}


/***********************************************************************************
 Function Name      : FreeListOfFragmentUSEVariants
 Inputs             : gc
                      ppsUSEVariantListHead - The head of the list of variants
 Outputs            : 
 Returns            : -
 Description        : Frees a linked list of fragment USE variants and their attached PDS variants.
                      If the variants are still in use they are ghosted appropriately.
************************************************************************************/
static IMG_VOID FreeListOfFragmentUSEVariants(GLES2Context *gc, GLES2USEShaderVariant **ppsUSEVariantListHead)
{
	KRMKickResourceManager *psKRM = &gc->psSharedState->sUSEShaderVariantKRM;
	GLES2USEShaderVariant       *psUSEVariant, *psUSEVariantNext;

	/* Fragment variants. The KRM keeps track of their status. */
	psUSEVariant = *ppsUSEVariantListHead;

	while(psUSEVariant)
	{
		psUSEVariantNext = psUSEVariant->psNext;

		if(KRM_IsResourceNeeded(psKRM, &psUSEVariant->sResource))
		{
			GhostUSEShaderVariant(gc, psUSEVariant);
		}
		else
		{
			DestroyUSEShaderVariant(gc, psUSEVariant);
		}

		if(gc->sProgram.psCurrentFragmentVariant==psUSEVariant)
		{
			gc->sProgram.psCurrentFragmentVariant=IMG_NULL;
		}	

		/* Keep the list consistent as DestroyUSEShaderVariant requires it */
		psUSEVariant = psUSEVariantNext;

		*ppsUSEVariantListHead = psUSEVariant;
	}
}


/***********************************************************************************
 Function Name      : ResetProgramLinkedState
 Inputs             : gc, program
 Outputs            : 
 Returns            : 
 Description        : Reset and free old state of the program before re-linking
************************************************************************************/
static IMG_VOID ResetProgramLinkedState(GLES2Context *gc, GLES2Program *psProgram)
{
	/* Linked state */
	psProgram->bSuccessfulLink                  = IMG_FALSE;

	psProgram->ui32NumActiveAttribs				= 0;
	psProgram->ui32AttribCompileMask			= 0;
	psProgram->ui32LengthOfLongestAttribName	= 0;

	psProgram->ui32NumActiveUniforms			= 0;
	psProgram->ui32NumBuiltInUniforms			= 0;

	psProgram->ui32LengthOfLongestUniformName	= 0;
	psProgram->ui32NumActiveUserUniforms		= 0;

	psProgram->ui32NumActiveVaryings			= 0;

	psProgram->bSuccessfulValidate = IMG_FALSE;

	/* In the vertex side we kick and wait */
	FreeListOfVertexUSEVariants(gc, &psProgram->sVertex.psVariant);
	GLES_ASSERT(!psProgram->sVertex.psVariant);

	/* Destroy the vertex shared state _after_ destroying the variants to make sure that the secondary upload task is finished */
	SharedShaderStateDelRef(gc, psProgram->sVertex.psSharedState);
	psProgram->sVertex.psSharedState = IMG_NULL;

	/* In the fragment side we ghost */
	FreeListOfFragmentUSEVariants(gc, &psProgram->sFragment.psVariant);
	GLES_ASSERT(!psProgram->sFragment.psVariant);

	SharedShaderStateDelRef(gc, psProgram->sFragment.psSharedState);
	psProgram->sFragment.psSharedState = IMG_NULL;

	/* Reset every variable for safety */
	if (psProgram->sVertex.pfConstantData != IMG_NULL)
	{
		GLES2Free(IMG_NULL, psProgram->sVertex.pfConstantData);
		psProgram->sVertex.pfConstantData = IMG_NULL;
	}
	
	GLES2MemSet(&psProgram->sVertex, 0, sizeof(GLES2ProgramShader));

	if (psProgram->sFragment.pfConstantData != IMG_NULL)
	{
		GLES2Free(IMG_NULL, psProgram->sFragment.pfConstantData);
		psProgram->sFragment.pfConstantData = IMG_NULL;
	}

	GLES2MemSet(&psProgram->sFragment, 0, sizeof(GLES2ProgramShader));

	psProgram->sVertex.eProgramType   = GLSLPT_VERTEX;
	psProgram->sFragment.eProgramType = GLSLPT_FRAGMENT;
}


/***********************************************************************************
 Function Name      : SetupOutputRemapping
 Inputs             : gc, program
 Outputs            : 
 Returns            : 
 Description        : Setup ouput remapping.
************************************************************************************/
static IMG_BOOL SetupOutputRemapping(GLES2Context *gc, GLES2Program *psProgram, IMG_CHAR szLogMessage[GLES2_MAX_LINK_MESSAGE_LENGTH])
{
	IMG_UINT32 i, t;
	GLES2Varying *psVarying;
	IMG_UINT32 ui32StartRegVP, ui32StartRegFP, ui32NumRegFP, ui32NumRegVP;
	GLSLBindingSymbol *psSymbolVP, *psSymbolFP;

	PVR_UNREFERENCED_PARAMETER(gc);

	for(i = 0; i < psProgram->ui32NumActiveVaryings; i++)
	{
		psVarying = &psProgram->psActiveVaryings[i];

		psSymbolVP = psVarying->psSymbolVP;
		psSymbolFP = psVarying->psSymbolFP;

		if(psSymbolFP)
		{
			ui32StartRegVP = GET_REG_OFFSET(psSymbolVP->sRegisterInfo.u.uBaseComp);

			ui32StartRegFP = GET_REG_OFFSET(psSymbolFP->sRegisterInfo.u.uBaseComp);
			ui32NumRegFP = GET_REG_COUNT(psSymbolFP->sRegisterInfo.uCompAllocCount * (IMG_UINT32)psSymbolFP->iActiveArraySize);

			GLES_ASSERT(ui32StartRegFP + ui32NumRegFP <= NUM_TC_REGISTERS);

			if(psSymbolFP->iActiveArraySize > psSymbolVP->iActiveArraySize)
			{
				snprintf(szLogMessage, GLES2_MAX_LINK_MESSAGE_LENGTH, 
					"The active size of %s in fragment shaders is greater than that in vertex shaders \n", psSymbolVP->pszName);
				return IMG_FALSE;
			}

			/* Setup Output reg remapping */
			for(t = 0; t < ui32NumRegFP; t++)
			{
				psProgram->aui32IteratorPosition[ui32StartRegFP + t] = ui32StartRegVP + t;
			}
		}

		if(psSymbolVP)
		{
			ui32StartRegVP = GET_REG_OFFSET(psSymbolVP->sRegisterInfo.u.uBaseComp);
			ui32NumRegVP = GET_REG_COUNT(psSymbolVP->sRegisterInfo.uCompAllocCount * (IMG_UINT32)psSymbolVP->iActiveArraySize);

			GLES_ASSERT(ui32StartRegVP + ui32NumRegVP <= NUM_TC_REGISTERS);

			/* Setup Output reg reverse mapping */
			for(t = 0; t < ui32NumRegVP; t++)
			{
				if(psSymbolFP)
				{
					psProgram->aui32FragVaryingPosition[ui32StartRegVP + t] = GET_REG_OFFSET(psSymbolFP->sRegisterInfo.u.uBaseComp) + t;
				}
				else
				{
					psProgram->aui32FragVaryingPosition[ui32StartRegVP + t] = 0xFFFFFFFF;
				}
			}
		}
	}

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : GetNumIndicesForAttribute
 Inputs             : type
 Outputs            : 
 Returns            : 
 Description        : return num of index needed for an attrib
************************************************************************************/
static IMG_UINT32 GetNumIndicesForAttribute(GLSLTypeSpecifier type)
{
	if(type == GLSLTS_MAT2X2)
	{
		return 2;
	}
	else if(type == GLSLTS_MAT3X3)
	{
		return 3;
	}
	else if(type == GLSLTS_MAT4X4)
	{
		return 4;
	}

	return 1;
}


/***********************************************************************************
 Function Name      : AssignAttributeLocations
 Inputs             : gc, program
 Outputs            : 
 Returns            : 
 Description        : Assign all attrib locations
************************************************************************************/
static IMG_BOOL AssignAttributeLocations(GLES2Context *gc, GLES2Program *psProgram, IMG_CHAR szLogMessage[GLES2_MAX_LINK_MESSAGE_LENGTH])
{
	IMG_BOOL abSlotStatus[GLES2_MAX_VERTEX_ATTRIBS];
	IMG_UINT32 i, ui32Index, j, ui32MinEmptySlot = 0;
	GLSLAttribUserBinding *psBinding;
	GLES2Attribute *psAttrib;

	PVR_UNREFERENCED_PARAMETER(gc);

	/* Initialise binding status */
	for(i = 0; i < GLES2_MAX_VERTEX_ATTRIBS; i++)
	{
		abSlotStatus[i] = IMG_FALSE;
	}

	/* Match up attributes which the user has defined locations for */
	if(psProgram->psUserBinding)
	{
		for(i = 0; i < psProgram->ui32NumActiveAttribs; i++)
		{
			psAttrib = &psProgram->psActiveAttributes[i];

			if(psAttrib->psSymbolVP->eBIVariableID != GLSLBV_NOT_BTIN) 
			{
				snprintf(szLogMessage, GLES2_MAX_LINK_MESSAGE_LENGTH, 
					"No built-in attributes in GLSL ES language: %s.",  psAttrib->psSymbolVP->pszName);
				return IMG_FALSE;
			}

			psBinding = psProgram->psUserBinding;
			
			while(psBinding)
			{
				if(!strcmp(psBinding->pszName, psAttrib->psSymbolVP->pszName))
				{
					ui32Index = (IMG_UINT32)(psBinding->i32Index);

					psAttrib->i32Index = (IMG_INT32)ui32Index;
					psAttrib->ui32NumIndices = GetNumIndicesForAttribute(psAttrib->psSymbolVP->eTypeSpecifier);

					for(j = ui32Index; j < ui32Index + psAttrib->ui32NumIndices; j++)
					{
						if(abSlotStatus[j]) 
						{
							snprintf(szLogMessage, GLES2_MAX_LINK_MESSAGE_LENGTH,
								"Not enough contiguous indices for attribute %s.",  psBinding->pszName);
							return IMG_FALSE;
						}
						else
						{
							abSlotStatus[j] = IMG_TRUE;

							if(ui32Index == ui32MinEmptySlot)
							{
								ui32MinEmptySlot = ui32Index + psAttrib->ui32NumIndices;
							}
						}
					}

					break;
				}

				psBinding = psBinding->psNext;
			}
		}
	}

	/* Assign attributes which the user has not defined locations for */
	for(i = 0; i < psProgram->ui32NumActiveAttribs; i++)
	{
		psAttrib = &psProgram->psActiveAttributes[i];

		if(psAttrib->psSymbolVP->eBIVariableID == GLSLBV_NOT_BTIN && !psAttrib->ui32NumIndices )
		{
			IMG_BOOL bFind;

			psAttrib->ui32NumIndices = GetNumIndicesForAttribute(psAttrib->psSymbolVP->eTypeSpecifier);

			/* Find contiguous slots */
			ui32Index = ui32MinEmptySlot;

			for(;;)
			{
				bFind = IMG_TRUE;
				if(ui32Index + psAttrib->ui32NumIndices > GLES2_MAX_VERTEX_ATTRIBS)
				{
					psAttrib->ui32NumIndices = 0;

					/* Outside of MAX_VERTEX_ATTRIBS, return FALSE */
					snprintf(szLogMessage, GLES2_MAX_LINK_MESSAGE_LENGTH, 
						"Not enough contiguous indices for attribute %s.",  psAttrib->psSymbolVP->pszName);
					return IMG_FALSE;
				}

				for(j = ui32Index; j < ui32Index + psAttrib->ui32NumIndices; j++)
				{
					if(abSlotStatus[j]) 
					{
						bFind = IMG_FALSE;
						break;
					}
				}

				if(bFind) 
				{
					psAttrib->i32Index = (IMG_INT32)ui32Index;

					for(j = ui32Index; j < ui32Index + psAttrib->ui32NumIndices; j++)
					{
						abSlotStatus[j] = IMG_TRUE;
					}

					if(ui32Index == ui32MinEmptySlot) 
						ui32MinEmptySlot = ui32Index + psAttrib->ui32NumIndices;

					break;
				}
				else
				{
					ui32Index = j;
					while(abSlotStatus[ui32Index])
					{
						ui32Index++;
					}
				}
			}
		}
	}

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : AssignUniformLocations
 Inputs             : gc, program
 Outputs            : 
 Returns            : 
 Description        : Assign all uniform locations
************************************************************************************/
static IMG_BOOL AssignUniformLocations(GLES2Program *psProgram)
{
	IMG_INT32 i32Location = 1;
	IMG_UINT32 i;
	GLES2Uniform *psUniform;

	for(i = 0; i < psProgram->ui32NumActiveUniforms; i++)
	{
		psUniform = &psProgram->psActiveUniforms[i];

		if(psUniform->i32Location != -1)
		{
			psUniform->i32Location = i32Location;
			i32Location += (psUniform->ui32DeclaredArraySize) ? (IMG_INT32)psUniform->ui32DeclaredArraySize : 1;
		}
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : AttributeCompileMask
 Inputs             : gc, program
 Outputs            : 
 Returns            : 
 Description        : Generates mask of required attributes
************************************************************************************/
static IMG_BOOL AttributeCompileMask(GLES2Context *gc, GLES2Program *psProgram, IMG_CHAR szLogMessage[GLES2_MAX_LINK_MESSAGE_LENGTH])
{
	IMG_UINT32 i, j;
	GLES2Attribute *psAttrib;
	GLSLBindingSymbol *psSymbol;
	IMG_UINT32 ui32DestReg;

	PVR_UNREFERENCED_PARAMETER(gc);

	for(i = 0; i < psProgram->ui32NumActiveAttribs; i++)
	{
		psAttrib = &psProgram->psActiveAttributes[i];
		psSymbol = psAttrib->psSymbolVP;

		ui32DestReg = GET_REG_OFFSET(psSymbol->sRegisterInfo.u.uBaseComp);

		/* Get the attribute compile mask */
		switch(psSymbol->eBIVariableID)
		{
			case GLSLBV_NOT_BTIN:
			{
				for(j = 0; j < psAttrib->ui32NumIndices; j++)
				{
					psProgram->ui32AttribCompileMask |= (1U << (AP_VERTEX_ATTRIB0 + psAttrib->i32Index + (IMG_INT32)j));
					psProgram->aui32InputRegMappings[AP_VERTEX_ATTRIB0 + psAttrib->i32Index + (IMG_INT32)j] = ui32DestReg + j;
				}

				break;
			}
			default:
			{
				snprintf(szLogMessage, GLES2_MAX_LINK_MESSAGE_LENGTH, "In-built attributes unsupported in GLSL ES");
				return IMG_FALSE;
			}
		}
	}
	
	return IMG_TRUE;
}

#if defined(SGX545)
#define ATTRIB0_SHIFT		EURASIA_MTE_ATTRDIM0_SHIFT
#define ATTRIB_U			EURASIA_MTE_ATTRDIM_U
#define ATTRIB_UV			EURASIA_MTE_ATTRDIM_UV
#define ATTRIB_UVS			EURASIA_MTE_ATTRDIM_UVS
#define ATTRIB_UVST			EURASIA_MTE_ATTRDIM_UVST

#else
#define ATTRIB0_SHIFT		EURASIA_MTE_TEXDIM0_SHIFT
#define ATTRIB_UV			EURASIA_MTE_TEXDIM_UV
#define ATTRIB_UVS			EURASIA_MTE_TEXDIM_UVS
#define ATTRIB_UVST			EURASIA_MTE_TEXDIM_UVST
#endif


/***********************************************************************************
 Function Name      : SetupProgramOutputSelects
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : Setup program output selects
************************************************************************************/
static IMG_VOID SetupProgramOutputSelects(GLES2Context *gc, GLES2Program *psProgram)
{
	GLES2SharedShaderState *psSharedShaderState = psProgram->sVertex.psSharedState;
	IMG_UINT32 ui32VaryingMask = psSharedShaderState->eActiveVaryingMask;
	IMG_UINT32 *pui32TexCoordDims = psSharedShaderState->aui32TexCoordDims;
	GLSLPrecisionQualifier *peTexCoordPrec = psSharedShaderState->aeTexCoordPrecisions;
	IMG_UINT32 ui32OutputSelects, ui32VertexSize, ui32TexCoordPrecision;
	IMG_UINT32 ui32TexCoordSelects = 0;
	IMG_UINT32 i;

	PVR_UNREFERENCED_PARAMETER(gc);

	/* Vertex coordinates always present*/
	ui32VertexSize = 4;
	ui32OutputSelects = EURASIA_MTE_WPRESENT;
	ui32TexCoordPrecision = 0;

	for(i = 0; i < NUM_TC_REGISTERS; i++)
	{
		/* PRQA S 4130 */
		if(ui32VaryingMask & (IMG_UINT32)(GLSLVM_TEXCOORD0 << i))
		{
			switch(pui32TexCoordDims[i])
			{
#if defined(SGX545)
				case 1:
				{
					ui32TexCoordSelects |= (ATTRIB_U << (ATTRIB0_SHIFT + (i * 3)));
					ui32VertexSize += 1;

					break;
				}
#endif /* defined(SGX545) */ 

				case 2:
				{
					ui32TexCoordSelects |= (ATTRIB_UV << (ATTRIB0_SHIFT + (i * 3)));
					ui32VertexSize += 2;

					break;
				}

				case 3:
				{
					ui32TexCoordSelects |= (ATTRIB_UVS << (ATTRIB0_SHIFT + (i * 3)));
					ui32VertexSize += 3;

					break;
				}

				case 4:
				{
					ui32TexCoordSelects |= (ATTRIB_UVST << (ATTRIB0_SHIFT + (i * 3)));
					ui32VertexSize += 4;

					break;
				}
			}

			/* Use 16 bit tex coords if vertex shader varying is medium or low */
			switch(peTexCoordPrec[i])
			{
				case GLSLPRECQ_HIGH:
					break;
				case GLSLPRECQ_MEDIUM:
				case GLSLPRECQ_LOW:
				case GLSLPRECQ_UNKNOWN:
					ui32TexCoordPrecision |= (EURASIA_TATEXFLOAT_TC0_16B << i);
					break;
				default:
					PVR_DPF((PVR_DBG_ERROR, "Invalid precision qualifier"));
					break;
			}
		}
	}

	if(ui32VaryingMask & GLSLVM_PTSIZE)
	{
		ui32OutputSelects |= EURASIA_MTE_SIZE;

		ui32VertexSize += 1;
	}
	
	ui32OutputSelects |= ui32VertexSize << EURASIA_MTE_VTXSIZE_SHIFT;
	
#if defined(SGX545)
	ui32OutputSelects |= EURASIA_MTE_CLIPMODE_FRONT_AND_REAR;
#endif

	psProgram->ui32TexCoordSelects = ui32TexCoordSelects;
	psProgram->ui32OutputSelects   = ui32OutputSelects;
	psProgram->ui32TexCoordPrecision = ui32TexCoordPrecision;

	gc->ui32EmitMask |= GLES2_EMITSTATE_MTE_STATE_OUTPUTSELECTS;
}



/***********************************************************************************
 Function Name      : CountAttribUniformVaryings
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID CountAttribUniformVaryings(const GLSLBindingSymbolList *psBindingSymbolList,
											IMG_UINT32 *pui32NumAttribs,
											IMG_UINT32 *pui32NumUniforms,
											IMG_UINT32 *pui32NumBuiltInUniforms,
											IMG_UINT32 *pui32NumVaryings)
{
	GLSLBindingSymbol *psBindingSymbol;
	IMG_UINT32 ui32NumAttribs = 0;
	IMG_UINT32 ui32NumUniforms = 0;
	IMG_UINT32 ui32NumBuiltInUniforms = 0;
	IMG_UINT32 ui32NumVaryings = 0;
	IMG_UINT i;

	for(i = 0; i < psBindingSymbolList->uNumBindings; i++)
	{
		psBindingSymbol = &psBindingSymbolList->psBindingSymbolEntries[i];

		switch(psBindingSymbol->eTypeQualifier)
		{
			case GLSLTQ_VERTEX_IN:
			{
				ui32NumAttribs ++;

				break;
			}
			case GLSLTQ_UNIFORM:
			{
				if(psBindingSymbol->eTypeSpecifier == GLSLTS_STRUCT)
				{
					ui32NumUniforms += psBindingSymbol->uNumBaseTypeMembers; 
				}
				else
				{
					ui32NumUniforms++;
				}
				
				if(psBindingSymbol->eBIVariableID != GLSLBV_NOT_BTIN)
				{
					ui32NumBuiltInUniforms++;
				}

				break;
			}
			case GLSLTQ_VERTEX_OUT:
			case GLSLTQ_FRAGMENT_IN:
			{
				ui32NumVaryings ++;

				break;
			}
			default:
			{
				break;
			}
		}
	}

	*pui32NumAttribs += ui32NumAttribs;
	*pui32NumUniforms += ui32NumUniforms;
	*pui32NumVaryings += ui32NumVaryings;
	*pui32NumBuiltInUniforms += ui32NumBuiltInUniforms;
}

/***********************************************************************************
 Function Name      : AddNewUniforms
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : Add a new uniform
************************************************************************************/
static IMG_VOID AddNewUniforms(GLES2Context *gc,
								GLES2Program *psProgram,
								IMG_BOOL bIsVertexShader,
								GLSLBindingSymbol	*psSymbol)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32NumUniforms = 1;
	GLES2Uniform *psUniform;

	PVR_UNREFERENCED_PARAMETER(gc);

	/* Add built-in uniform list if it is a builtin uniform */
	if(psSymbol->eBIVariableID != GLSLBV_NOT_BTIN)
	{
		GLES2BuiltInUniform *psBuiltInUniform = &psProgram->psBuiltInUniforms[psProgram->ui32NumBuiltInUniforms];

		psBuiltInUniform->eBuiltinVariableID = psSymbol->eBIVariableID;

		if(bIsVertexShader)
		{
			psBuiltInUniform->psSymbolVP = psSymbol;
			psBuiltInUniform->psSymbolFP = IMG_NULL;
//			program->vertex.uniformBindingMask |= BuiltInSymbolMappingTable[symbol->eBIVariableID];
		}
		else
		{
			psBuiltInUniform->psSymbolVP = IMG_NULL;
			psBuiltInUniform->psSymbolFP = psSymbol;
//			program->fragment.uniformBindingMask |= BuiltInSymbolMappingTable[symbol->eBIVariableID];
		}

		psProgram->ui32NumBuiltInUniforms++;
	}

	if(psSymbol->uNumBaseTypeMembers)
	{
		ui32NumUniforms = psSymbol->uNumBaseTypeMembers;
	
		psSymbol = psSymbol->psBaseTypeMembers;
	}

	for(i = 0; i < ui32NumUniforms; i++)
	{
		psUniform = &psProgram->psActiveUniforms[psProgram->ui32NumActiveUniforms];
		psProgram->ui32NumActiveUniforms++;

		psUniform->eTypeSpecifier = psSymbol[i].eTypeSpecifier;
		psUniform->ePrecisionQualifier = psSymbol[i].ePrecisionQualifier;
		psUniform->ui32ActiveArraySize = (IMG_UINT32)psSymbol[i].iActiveArraySize;
		psUniform->ui32DeclaredArraySize = (IMG_UINT32)psSymbol[i].iDeclaredArraySize;
		psUniform->pszName = psSymbol[i].pszName;

		if(psSymbol->eBIVariableID == GLSLBV_NOT_BTIN)
		{
			psUniform->i32Location = 0; /* Be assigned in later funciton */ 
		}
		else
		{
			psUniform->i32Location = -1;
		}

		if(bIsVertexShader) 
		{
			psUniform->psSymbolVP = &psSymbol[i];
			psUniform->psSymbolFP = IMG_NULL;
		}
		else
		{
			psUniform->psSymbolVP = IMG_NULL;
			psUniform->psSymbolFP = &psSymbol[i];
		}

		if(GLES2_IS_SAMPLER(psUniform->eTypeSpecifier))
		{
			IMG_UINT32 ui32Count;
			IMG_UINT32 ui32Unit = psSymbol[i].sRegisterInfo.u.uTextureUnit;
			IMG_UINT32 ui32UnitCount = (IMG_UINT32)psSymbol[i].iActiveArraySize;
			GLES2ProgramShader *psShader;
			GLES2TextureSampler *psTextureSampler;

			/* Make the uniform point to the starting entry of the texture sampler info array */
			if(bIsVertexShader)
			{
				psShader = &psProgram->sVertex;
				psUniform->ui32VPSamplerIndex = ui32Unit;
			}
			else
			{
				psShader = &psProgram->sFragment;
				psUniform->ui32FPSamplerIndex = ui32Unit;
			}

			psTextureSampler = &psShader->asTextureSamplers[0];

			for(ui32Count = ui32Unit; ui32Count < ui32UnitCount + ui32Unit; ui32Count++)
			{
				switch(psUniform->eTypeSpecifier)
				{
					case GLSLTS_SAMPLER2D:
						psTextureSampler[ui32Count].ui8SamplerTypeIndex = GLES2_TEXTURE_TARGET_2D;
						break;
					case GLSLTS_SAMPLERCUBE:
						psTextureSampler[ui32Count].ui8SamplerTypeIndex = GLES2_TEXTURE_TARGET_CEM;
						break;
					case GLSLTS_SAMPLERSTREAM:
					case GLSLTS_SAMPLEREXTERNAL:
						psTextureSampler[ui32Count].ui8SamplerTypeIndex = GLES2_TEXTURE_TARGET_STREAM;
						break;
					default:
						PVR_DPF((PVR_DBG_ERROR,"Unknown sampler type"));
						break;
				}

				psTextureSampler[ui32Count].ui8ImageUnit		= 0;	/* Set-up by application later:
																		 * by default, it is 0 
																		 */
				psTextureSampler[ui32Count].psUniformEntry		= psUniform;
				psShader->ui32SamplersActive |= (1U << ui32Count);
			}

			if(GLES2_IS_PERM_TEXTURE_SAMPLER(psSymbol[i].eBIVariableID))
			{
				psShader->asTextureSamplers[ui32Unit].ui8ImageUnit = GLES2_GLSL_PERM_TEXTURE_UNIT;
			}
			else if(GLES2_IS_GRAD_TEXTURE_SAMPLER(psSymbol[i].eBIVariableID))
			{
				psShader->asTextureSamplers[ui32Unit].ui8ImageUnit = GLES2_GLSL_GRAD_TEXTURE_UNIT;
			}
		}

		if(psSymbol[i].eBIVariableID <= GLSLBV_FOGFRAGCOORD)
		{
			IMG_UINT32 ui32UniformNameLength = strlen(psUniform->pszName) + 1;

			/* User uniform (not HW special) */
			psProgram->ppsActiveUserUniforms[psProgram->ui32NumActiveUserUniforms] = psUniform;
			psProgram->ui32NumActiveUserUniforms++;

			if(psUniform->ui32DeclaredArraySize)
			{
				ui32UniformNameLength += 3;
			}

			if(ui32UniformNameLength > psProgram->ui32LengthOfLongestUniformName)
			{
				psProgram->ui32LengthOfLongestUniformName = ui32UniformNameLength;
			}
		}
	}
}

/***********************************************************************************
 Function Name      : AddFragmentProgramUniforms
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_BOOL AddFragmentProgramUniforms(GLES2Context *gc,
										  GLES2Program *psProgram,
										  GLSLBindingSymbol	*psSymbol,
										  IMG_CHAR szLogMessage[GLES2_MAX_LINK_MESSAGE_LENGTH])
{
	GLSLBindingSymbol *psLoopSymbol;
	IMG_BOOL bFindMatch = IMG_FALSE;
	IMG_BOOL bSuccessful = IMG_TRUE;
	IMG_UINT32 ui32NumUniforms = 1;
	IMG_UINT32 i, j;

	psLoopSymbol = psSymbol;

	if(psSymbol->uNumBaseTypeMembers)
	{
		ui32NumUniforms = psSymbol->uNumBaseTypeMembers;
	
		psLoopSymbol = psSymbol->psBaseTypeMembers;
	}

	for(i=0; i < ui32NumUniforms; i++)
	{
		GLES2Uniform *psUniform;

		/* Check if this name has been added to the list */
		for(j = 0; j < psProgram->ui32NumActiveUniforms; j++)
		{
			if(!strcmp(psProgram->psActiveUniforms[j].pszName, psLoopSymbol[i].pszName))
			{
				psUniform = &psProgram->psActiveUniforms[j];

				if(psUniform->eTypeSpecifier == psLoopSymbol[i].eTypeSpecifier &&
					psUniform->ePrecisionQualifier ==  psLoopSymbol[i].ePrecisionQualifier)
				{
					psUniform->ui32ActiveArraySize = MAX(psUniform->ui32ActiveArraySize, (IMG_UINT32)psLoopSymbol[i].iActiveArraySize);
					psUniform->ui32DeclaredArraySize = MAX(psUniform->ui32DeclaredArraySize, (IMG_UINT32)psLoopSymbol[i].iDeclaredArraySize);

					psUniform->psSymbolFP = &psLoopSymbol[i];

					if(GLES2_IS_SAMPLER(psUniform->eTypeSpecifier))
					{
						IMG_UINT32 ui32Count;
						IMG_UINT32 ui32Unit = psLoopSymbol[i].sRegisterInfo.u.uTextureUnit;
						IMG_UINT32 ui32UnitCount = (IMG_UINT32)psLoopSymbol[i].iActiveArraySize;
						GLES2ProgramShader *psShader = &psProgram->sFragment;
						GLES2TextureSampler *psTextureSampler; 

						psUniform->ui32FPSamplerIndex = ui32Unit;
						psTextureSampler = &psShader->asTextureSamplers[0];

						for(ui32Count = ui32Unit; ui32Count < ui32UnitCount + ui32Unit; ui32Count++)
						{
							psTextureSampler[ui32Count].ui8SamplerTypeIndex	=  (IMG_UINT8)(
								(psUniform->eTypeSpecifier == GLSLTS_SAMPLER2D) ? 
								GLES2_TEXTURE_TARGET_2D : GLES2_TEXTURE_TARGET_CEM);
						
							psTextureSampler[ui32Count].ui8ImageUnit		= 0;	/* Set-up by application later:
																					* by default, it is 0 
																					*/
							psTextureSampler[ui32Count].psUniformEntry		= psUniform;
							psShader->ui32SamplersActive |= (1U << ui32Count);
						}
					}
				}
				else
				{
					snprintf(szLogMessage, GLES2_MAX_LINK_MESSAGE_LENGTH,
						"Uniform variable %s type/precision does not match in vertex and fragment shader\n", psLoopSymbol[i].pszName);

					bSuccessful = IMG_FALSE;
				}

				bFindMatch = IMG_TRUE;
			}
		}
	}

	/* Check if this name has been added to the list */
	for(j = 0; j < psProgram->ui32NumBuiltInUniforms; j++)
	{
		if(psProgram->psBuiltInUniforms[j].eBuiltinVariableID == psSymbol->eBIVariableID)
		{
			psProgram->psBuiltInUniforms[j].psSymbolFP = psSymbol;
//			psProgram->sFragment.uniformBindingMask |= BuiltInSymbolMappingTable[symbol->eBIVariableID];

			bFindMatch = IMG_TRUE;

			break;
		}
	}
	if(!bFindMatch)
		AddNewUniforms(gc, psProgram, IMG_FALSE, psSymbol); 

	return bSuccessful;
}


/***********************************************************************************
 Function Name      : AppendMessageToProgramInfoLog
 Inputs             : gc, psProgram, pszMessage
 Outputs            : -
 Returns            : -
 Description        : Appends the given message to the infolog of the given program.
************************************************************************************/
static IMG_VOID AppendMessageToProgramInfoLog(GLES2Context *gc, GLES2Program *psProgram, const IMG_CHAR *pszMessage)
{
	IMG_CHAR *pszNewInfoLog;
	IMG_UINT32 ui32NewInfoLogLength = strlen(pszMessage) + 1;

	if(psProgram->pszInfoLog)
	{
		ui32NewInfoLogLength += strlen(psProgram->pszInfoLog);
		pszNewInfoLog = GLES2Realloc(gc, psProgram->pszInfoLog, ui32NewInfoLogLength);
	}
	else
	{
		pszNewInfoLog = GLES2Malloc(gc, ui32NewInfoLogLength);
		pszNewInfoLog[0] = '\0';
	}


	if(!pszNewInfoLog)
	{
		SetError(gc, GL_OUT_OF_MEMORY);
		return;
	}

	psProgram->pszInfoLog = pszNewInfoLog;
	strcat(psProgram->pszInfoLog, pszMessage);
}


/***********************************************************************************
 Function Name      : LinkVertexFragmentPrograms
 Inputs             : gc, psProgram
 Outputs            : 
 Returns            : 
 Description        : Post process following GLSL compilation
					  1) Generate active uniform list, attribute list and varying list from 
						 the general binding symbol list.
					  2) Assign generic uniforms, attributes location
					  3) Generate attribute compile mask
					  4) Generate uniform binding mask
					  5) Setup Vertex Input reg mapping
					  6) Setup Vertex Output reg remapping 
					  7) Link log message if any
************************************************************************************/
static IMG_BOOL LinkVertexFragmentPrograms(GLES2Context *gc, GLES2Program *psProgram)
{
	GLES2Attribute			*psAttrib;
	GLES2Varying			*psVarying = IMG_NULL;
	IMG_UINT32				ui32NumUniforms = 0;
	IMG_UINT32				ui32NumAttribs = 0;
	IMG_UINT32				ui32NumVaryings = 0;
	IMG_UINT32				ui32NumBuiltInUniforms = 0;
	IMG_BOOL				bSpecial;
	IMG_UINT32				i, j;
	GLSLBindingSymbolList	*psVertexSymbolList = IMG_NULL;
	GLSLBindingSymbolList	*psFragmentSymbolList = IMG_NULL;
	IMG_CHAR				szLogMessage[GLES2_MAX_LINK_MESSAGE_LENGTH];
	IMG_BOOL				bLinkSuccess = IMG_TRUE;

	GLES2MemSet(szLogMessage, 0, GLES2_MAX_LINK_MESSAGE_LENGTH);

	/* Take reference to shared shader state */
	psProgram->sVertex.psSharedState = psProgram->psVertexShader->psSharedState;
	SharedShaderStateAddRef(gc, psProgram->sVertex.psSharedState);
	
	psProgram->sFragment.psSharedState = psProgram->psFragmentShader->psSharedState;
	SharedShaderStateAddRef(gc, psProgram->sFragment.psSharedState);

	/* 
	** Allocate all the necessary memory
	*/
	if (psProgram->sVertex.psSharedState != IMG_NULL)
	{
		if (psProgram->sVertex.psSharedState->sBindingSymbolList.pfConstantData != IMG_NULL)
		{
			IMG_UINT32 uAllocSize = psProgram->sVertex.psSharedState->sBindingSymbolList.uNumCompsUsed * sizeof(IMG_FLOAT);
			IMG_FLOAT *pfNewConstantData = (IMG_FLOAT*)GLES2Realloc(gc, psProgram->sVertex.pfConstantData, uAllocSize);
			if (pfNewConstantData == IMG_NULL)
			{
				PVR_DPF((PVR_DBG_ERROR, "LinkVertexFragmentPrograms: Cannot get local memory for vertex shader constant storage\n"));
				goto bad_alloc;
			}

			psProgram->sVertex.pfConstantData = pfNewConstantData;
			GLES2MemCopy(psProgram->sVertex.pfConstantData, psProgram->sVertex.psSharedState->sBindingSymbolList.pfConstantData, uAllocSize);
		}
		else
		{
			GLES2Free(IMG_NULL, psProgram->sVertex.pfConstantData);
			psProgram->sVertex.pfConstantData = IMG_NULL;
		}
	}
	else
	{
		GLES2Free(IMG_NULL, psProgram->sVertex.pfConstantData);
		psProgram->sVertex.pfConstantData = IMG_NULL;
	}
	
	if (psProgram->sFragment.psSharedState != IMG_NULL)
	{
		if (psProgram->sFragment.psSharedState->sBindingSymbolList.pfConstantData != IMG_NULL)
		{
			IMG_UINT32 uAllocSize = psProgram->sFragment.psSharedState->sBindingSymbolList.uNumCompsUsed * sizeof(IMG_FLOAT);
			IMG_FLOAT *pfNewConstantData = (IMG_FLOAT*)GLES2Realloc(gc, psProgram->sFragment.pfConstantData, uAllocSize);
			if (pfNewConstantData == IMG_NULL)
			{
				PVR_DPF((PVR_DBG_ERROR, "LinkVertexFragmentPrograms: Cannot get local memory for fragment shader constant storage\n"));
				goto bad_alloc;
			}

			psProgram->sFragment.pfConstantData = pfNewConstantData;
			GLES2MemCopy(psProgram->sFragment.pfConstantData, psProgram->sFragment.psSharedState->sBindingSymbolList.pfConstantData, uAllocSize);
		}
		else
		{
			GLES2Free(IMG_NULL, psProgram->sFragment.pfConstantData);
			psProgram->sFragment.pfConstantData = IMG_NULL;
		}
	}
	else
	{
		GLES2Free(IMG_NULL, psProgram->sFragment.pfConstantData);
		psProgram->sFragment.pfConstantData = IMG_NULL;
	}
	
	if(psProgram->psVertexShader)
	{
		psVertexSymbolList = &psProgram->sVertex.psSharedState->sBindingSymbolList;
		CountAttribUniformVaryings(psVertexSymbolList, &ui32NumAttribs, &ui32NumUniforms,
														&ui32NumBuiltInUniforms, &ui32NumVaryings);
	}

	if(psProgram->psFragmentShader)
	{
		psFragmentSymbolList = &psProgram->sFragment.psSharedState->sBindingSymbolList;
		CountAttribUniformVaryings(psFragmentSymbolList, &ui32NumAttribs, &ui32NumUniforms,
														&ui32NumBuiltInUniforms, &ui32NumVaryings);
	}

	if(ui32NumUniforms)
	{
		/* Resize the array of active uniforms */
		GLES2Uniform *psNewActiveUniforms = (GLES2Uniform*)GLES2Realloc(gc, psProgram->psActiveUniforms,
															ui32NumUniforms * sizeof(GLES2Uniform));
		GLES2Uniform **ppsNewActiveUserUniforms;

		if(!psNewActiveUniforms)
		{
			PVR_DPF((PVR_DBG_ERROR, "LinkVertexFragmentPrograms: Cannot get local memory for active uniforms\n"));
			goto bad_alloc;
		}

		psProgram->psActiveUniforms = psNewActiveUniforms;

		/* Resize the array of active user uniforms */
		ppsNewActiveUserUniforms = (GLES2Uniform**)GLES2Realloc(gc, psProgram->ppsActiveUserUniforms,
															ui32NumUniforms * sizeof(GLES2Uniform *));

		if(!ppsNewActiveUserUniforms)
		{
			PVR_DPF((PVR_DBG_ERROR, "LinkVertexFragmentPrograms: Cannot get local memory for active user uniform list \n"));
			goto bad_alloc;
		}

		psProgram->ppsActiveUserUniforms = ppsNewActiveUserUniforms;
	}
	else
	{
		GLES2Free(IMG_NULL, psProgram->psActiveUniforms);
		psProgram->psActiveUniforms = IMG_NULL;

		GLES2Free(IMG_NULL, psProgram->ppsActiveUserUniforms);
		psProgram->ppsActiveUserUniforms = IMG_NULL;
	}

	if(ui32NumBuiltInUniforms)
	{
		/* Resize the array of built-in uniforms */
		GLES2BuiltInUniform *psNewBuiltinUniforms = (GLES2BuiltInUniform*)GLES2Realloc(gc, psProgram->psBuiltInUniforms, 
													ui32NumBuiltInUniforms * sizeof(GLES2BuiltInUniform));

		if(!psNewBuiltinUniforms)
		{
			PVR_DPF((PVR_DBG_ERROR, "LinkVertexFragmentPrograms: Cannot get local memory for built in uniform list \n"));
			goto bad_alloc;
		}

		psProgram->psBuiltInUniforms = psNewBuiltinUniforms;
	}
	else
	{
		GLES2Free(IMG_NULL, psProgram->psBuiltInUniforms);
		psProgram->psBuiltInUniforms = IMG_NULL;
	}

	if(ui32NumAttribs)
	{
		/* Resize the array of active attributes */
		GLES2Attribute *psNewActiveAttributes = GLES2Realloc(gc, psProgram->psActiveAttributes,
														ui32NumAttribs * sizeof(GLES2Attribute));
		if(!psNewActiveAttributes)
		{
			PVR_DPF((PVR_DBG_ERROR, "LinkVertexFragmentPrograms: Cannot get local memory for active attrib list \n"));
			goto bad_alloc;
		}

		psProgram->psActiveAttributes = psNewActiveAttributes;
	}
	else
	{
		GLES2Free(IMG_NULL, psProgram->psActiveAttributes);
		psProgram->psActiveAttributes = IMG_NULL;
	}

	if(ui32NumVaryings)
	{
		/* Resize the array of active varyings */
		GLES2Varying *psNewActiveVaryings = (GLES2Varying*)GLES2Realloc(gc, psProgram->psActiveVaryings,
														ui32NumVaryings * sizeof(GLES2Varying));

		if(!psNewActiveVaryings)
		{
			PVR_DPF((PVR_DBG_ERROR, "LinkVertexFragmentPrograms: Cannot get local memory for active varying list \n"));
			goto bad_alloc;
		}

		psProgram->psActiveVaryings = psNewActiveVaryings;
	}
	else
	{
		GLES2Free(IMG_NULL, psProgram->psActiveVaryings);
		psProgram->psActiveVaryings = IMG_NULL;
	}

	if(psProgram->psVertexShader)
	{
		GLSLBindingSymbolList *psSymbolList = psVertexSymbolList;
		GLSLBindingSymbol *psSymbol;

		if (psSymbolList != NULL)
		{
			for(i = 0; i < psSymbolList->uNumBindings; i++)
			{
				psSymbol = &psSymbolList->psBindingSymbolEntries[i];

				/* First check if it is a special variable */
				bSpecial = IMG_TRUE;

				switch(psSymbol->eBIVariableID)
				{
					case GLSLBV_POSITION:
						psProgram->sVertex.psActiveSpecials[GLES2_SPECIAL_POSITION] = psSymbol;
						break;
					default:
						bSpecial = IMG_FALSE;
						break;
				}

				if(bSpecial) 
					continue;

				switch(psSymbol->eTypeQualifier)
				{
					case GLSLTQ_UNIFORM:
					{
						AddNewUniforms(gc, psProgram, IMG_TRUE, psSymbol);
						break;
					}
					case GLSLTQ_VERTEX_IN:
					{
						psAttrib = &psProgram->psActiveAttributes[psProgram->ui32NumActiveAttribs];

						psAttrib->i32Index = -1;
						psAttrib->ui32NumIndices = 0;

						psAttrib->psSymbolVP = psSymbol;

						psProgram->ui32NumActiveAttribs ++;

						if((IMG_UINT32)(strlen(psSymbol->pszName) + 1) > psProgram->ui32LengthOfLongestAttribName)
						{
							psProgram->ui32LengthOfLongestAttribName = (IMG_UINT32)(strlen(psSymbol->pszName) + 1);
						}

						break;
					}
					case GLSLTQ_VERTEX_OUT:
					{
						psVarying = &psProgram->psActiveVaryings[psProgram->ui32NumActiveVaryings];

						psVarying->pszName = psSymbol->pszName;
						psVarying->eTypeSpecifier = psSymbol->eTypeSpecifier;
						psVarying->ui32ActiveArraySize = (IMG_UINT32)psSymbol->iActiveArraySize;
						psVarying->ui32DeclaredArraySize = (IMG_UINT32)psSymbol->iDeclaredArraySize;
						psVarying->psSymbolVP = psSymbol;
						psVarying->psSymbolFP = IMG_NULL;

						psProgram->ui32NumActiveVaryings ++;
						break;
					}
					default:
						break;
				}
			}
		}
	}

	if(psProgram->psFragmentShader)
	{
		GLSLBindingSymbolList *psSymbolList = psFragmentSymbolList;
		GLSLBindingSymbol *psSymbol;
		IMG_BOOL bFindMatch; 

		if (psSymbolList != NULL)
		{
			for(i = 0; i < psSymbolList->uNumBindings; i++)
			{
				psSymbol = &psSymbolList->psBindingSymbolEntries[i];

				/* First check if it is a special variable */
				bSpecial = IMG_TRUE;

				switch(psSymbol->eBIVariableID)
				{
					case GLSLBV_FRAGCOORD:
						psProgram->sFragment.psActiveSpecials[GLES2_SPECIAL_FRAGCOORD] = psSymbol;
						break;
					case GLSLESBV_POINTCOORD:
						psProgram->sFragment.psActiveSpecials[GLES2_SPECIAL_POINTCOORD] = psSymbol;
						break;
					case GLSLBV_FRONTFACING:
						psProgram->sFragment.psActiveSpecials[GLES2_SPECIAL_FRONTFACING] = psSymbol;
						break;
					case GLSLBV_FRAGCOLOR:
						psProgram->sFragment.psActiveSpecials[GLES2_SPECIAL_FRAGCOLOR] = psSymbol;
						break;
					case GLSLBV_FRAGDATA:
						psProgram->sFragment.psActiveSpecials[GLES2_SPECIAL_FRAGDATA] = psSymbol;
						break;
					default:
						bSpecial = IMG_FALSE;
						break;
				}

				if(bSpecial)
					continue;

				switch(psSymbol->eTypeQualifier)
				{
					case GLSLTQ_UNIFORM:
					{
						if(!AddFragmentProgramUniforms(gc, psProgram, psSymbol, szLogMessage))
						{
							bLinkSuccess = IMG_FALSE;
							goto FailedLink;
						}	
						break;
					}
					case GLSLTQ_FRAGMENT_IN:
					{
						/* Check if this name has been added to the list */
						bFindMatch = IMG_FALSE;

						for(j = 0; j < psProgram->ui32NumActiveVaryings; j++)
						{
							psVarying = &psProgram->psActiveVaryings[j];
							
							if(!strcmp(psVarying->pszName, psSymbol->pszName))
							{
								bFindMatch = IMG_TRUE;
								break;
							}
						}

						if(bFindMatch)
						{
							if(	psVarying->eTypeSpecifier != psSymbol->eTypeSpecifier)
							{
								snprintf(szLogMessage, GLES2_MAX_LINK_MESSAGE_LENGTH,
									"The type for varying variable %s does not match in vertex and fragment shaders\n", psSymbol->pszName);
								bLinkSuccess = IMG_FALSE;
								goto FailedLink;
							}
							else if(psVarying->psSymbolVP->eVaryingModifierFlags != psSymbol->eVaryingModifierFlags)
							{
								snprintf(szLogMessage, GLES2_MAX_LINK_MESSAGE_LENGTH,
									"The invariance for varying variable %s does not match in vertex and fragment shaders\n", psSymbol->pszName);
								bLinkSuccess = IMG_FALSE;
								goto FailedLink;
							}
							else
							{
								psVarying->psSymbolFP = psSymbol;

								psVarying->ui32ActiveArraySize = 
									MAX(psVarying->ui32ActiveArraySize, (IMG_UINT32)psSymbol->iActiveArraySize);
								psVarying->ui32DeclaredArraySize = 
									MAX(psVarying->ui32DeclaredArraySize,(IMG_UINT32) psSymbol->iDeclaredArraySize);

								if(psVarying->psSymbolFP->iActiveArraySize > psVarying->psSymbolVP->iActiveArraySize)
								{
									snprintf(szLogMessage, GLES2_MAX_LINK_MESSAGE_LENGTH,
										"Active size for varying %s in fragment shaders is greater than that in vertex shaders\n", psSymbol->pszName);
									bLinkSuccess = IMG_FALSE;
									goto FailedLink;
								}
							}
						}
						else
						{
							/* Cannot find the name */
							snprintf(szLogMessage, GLES2_MAX_LINK_MESSAGE_LENGTH,
								"Varying variable %s is used in fragment shader, but not found in vertex shader.\n", psSymbol->pszName);
							bLinkSuccess = IMG_FALSE;
							goto FailedLink;
						}

						break;
					}
					default:
					{
						break;
					}
				}
			}
		}
	}

	if(!SetupOutputRemapping(gc, psProgram, szLogMessage))
	{
		bLinkSuccess = IMG_FALSE;
		goto FailedLink;
	}

	/* Assign attribute locations */
	if(!AssignAttributeLocations(gc, psProgram, szLogMessage))
	{
		bLinkSuccess = IMG_FALSE;
		goto FailedLink;
	}

	/* Compute attribute compile mask */
	if(!AttributeCompileMask(gc, psProgram, szLogMessage))
	{
		bLinkSuccess = IMG_FALSE;
		goto FailedLink;
	}

	/* Assign uniform locations */
	if(!AssignUniformLocations(psProgram))
	{
		bLinkSuccess = IMG_FALSE;
		goto FailedLink;
	}

	/* Setup output selects */
	SetupProgramOutputSelects(gc, psProgram);

FailedLink:
	if(!bLinkSuccess)
	{
		AppendMessageToProgramInfoLog(gc, psProgram, szLogMessage);
	}

#if defined(DEBUG)
	if(gc->pShaderAnalysisHandle)
	{
		static const IMG_CHAR pszTypeSpecifierDescTable[GLSLTS_STRUCT + 1][5] =
		{
			"uk",   /* GLSLTS_INVALID */
			"vd",   /* GLSLTS_VOID    */
			"f1",   /* GLSLTS_FLOAT   */
			"f2",   /* GLSLTS_VEC2    */
			"f3",   /* GLSLTS_VEC3    */
			"f4",   /* GLSLTS_VEC4    */
			"i1",   /* GLSLTS_INT     */
			"i2",   /* GLSLTS_IVEC2   */
			"i3",   /* GLSLTS_IVEC3   */
			"i4",   /* GLSLTS_IVEC4   */
			"b1",   /* GLSLTS_BOOL    */
			"b2",   /* GLSLTS_BVEC2   */
			"b3",   /* GLSLTS_BVEC3   */
			"b4",   /* GLSLTS_BVEC4   */
			"m2x2", /* GLSLTS_MAT2X2  */
			"m2x3", /* GLSLTS_MAT2X3  */
			"m2x4", /* GLSLTS_MAT2X4  */
			"m3x2", /* GLSLTS_MAT3X2  */
			"m3x3", /* GLSLTS_MAT3X3  */
			"m3x4", /* GLSLTS_MAT3X4  */
			"m4x2", /* GLSLTS_MAT4X2  */
			"m4x3", /* GLSLTS_MAT4X3  */
			"m4x4", /* GLSLTS_MAT4X4  */
			"s1",   /* GLSLTS_SAMPLER1D       */
			"s2",   /* GLSLTS_SAMPLER2D       */
			"s3",   /* GLSLTS_SAMPLER3D       */
			"sc",   /* GLSLTS_SAMPLERCUBE     */
			"d1",   /* GLSLTS_SAMPLER1DSHADOW */
			"d2",   /* GLSLTS_SAMPLER2DSHADOW */
			"st"    /* GLSLTS_STRUCT          */
		};
		char str[512];
		GLES2Uniform *psUniform = IMG_NULL;

		fprintf(gc->pShaderAnalysisHandle, "\n--------------------------------\n");
		fprintf(gc->pShaderAnalysisHandle, "Active Attributes:\n");
		fprintf(gc->pShaderAnalysisHandle, "--------------------------------\n");
		fprintf(gc->pShaderAnalysisHandle, "Name                                               Type  index   Comp(  Reg)  DataMask\n");

		for(i = 0; i < psProgram->ui32NumActiveAttribs; i++)
		{
			psAttrib = &psProgram->psActiveAttributes[i];

			fprintf(gc->pShaderAnalysisHandle, "%-50s %-5s %-5d %6u(%3u.%1u)  ",
					psAttrib->psSymbolVP->pszName,
					pszTypeSpecifierDescTable[psAttrib->psSymbolVP->eTypeSpecifier],
					psAttrib->i32Index,
					psAttrib->psSymbolVP->sRegisterInfo.u.uBaseComp,
					GET_REG_OFFSET(psAttrib->psSymbolVP->sRegisterInfo.u.uBaseComp),
					GET_REG_OFFCOMP(psAttrib->psSymbolVP->sRegisterInfo.u.uBaseComp));

			fprintf(gc->pShaderAnalysisHandle, "0x%04x   \n", psAttrib->psSymbolVP->sRegisterInfo.ui32CompUseMask);
		}

		fprintf(gc->pShaderAnalysisHandle, "\n--------------------------------\n");
		fprintf(gc->pShaderAnalysisHandle, "Active Uniforms:\n");
		fprintf(gc->pShaderAnalysisHandle, "--------------------------------\n");
		fprintf(gc->pShaderAnalysisHandle, "Name                                              Type  loc   Active Array  VComp(  Reg)  DataMask  FComp(  Reg)  DataMask\n");

		for(i = 0; i < psProgram->ui32NumActiveUniforms; i++)
		{
			psUniform = &psProgram->psActiveUniforms[i]; 
			j = sprintf(str, "%-50s %-5s %-5d %-6u %-5u",
						psUniform->pszName,
						pszTypeSpecifierDescTable[psUniform->eTypeSpecifier],
						psUniform->i32Location,
						psUniform->ui32ActiveArraySize,
						psUniform->ui32DeclaredArraySize);

			if(psUniform->psSymbolVP)
			{
				j += sprintf(str+j, "%6u(%3u.%1u)  ",
							 psUniform->psSymbolVP->sRegisterInfo.u.uBaseComp,
							 GET_REG_OFFSET(psUniform->psSymbolVP->sRegisterInfo.u.uBaseComp),
							 GET_REG_OFFCOMP(psUniform->psSymbolVP->sRegisterInfo.u.uBaseComp));

				j += sprintf(str+j, "0x%04x   ", psUniform->psSymbolVP->sRegisterInfo.ui32CompUseMask);
			}
			else
			{
				j += sprintf(str+j, "    -            -      ");
			}

			if(psUniform->psSymbolFP)
			{
				j += sprintf(str+j, "%6u(%3u.%1u)  ", 
							 psUniform->psSymbolFP->sRegisterInfo.u.uBaseComp,
							 GET_REG_OFFSET(psUniform->psSymbolFP->sRegisterInfo.u.uBaseComp),
							 GET_REG_OFFCOMP(psUniform->psSymbolFP->sRegisterInfo.u.uBaseComp));

				j += sprintf(str+j, "0x%04x   ", psUniform->psSymbolFP->sRegisterInfo.ui32CompUseMask);
			}
			else
			{
				j += sprintf(str+j, "    -            -      ");
			}

			fprintf(gc->pShaderAnalysisHandle, "%s\n", str);
		}

		fprintf(gc->pShaderAnalysisHandle, "\n--------------------------------\n");
		fprintf(gc->pShaderAnalysisHandle, "Active Varyings:\n");
		fprintf(gc->pShaderAnalysisHandle, "--------------------------------\n");
		fprintf(gc->pShaderAnalysisHandle, "Name                                               Type  Active Array VComp(  Reg)  DataMask  FComp(  Reg)  DataMask\n");

		for(i = 0; i < psProgram->ui32NumActiveVaryings; i++)
		{
			psVarying = &psProgram->psActiveVaryings[i]; 

			j = sprintf(str, "%-50s %-5s %-6u %-5u",
						psVarying->pszName,
						pszTypeSpecifierDescTable[psVarying->eTypeSpecifier],
						psVarying->ui32ActiveArraySize,
						psVarying->ui32DeclaredArraySize);

			if(psVarying->psSymbolVP)
			{
				j += sprintf(str+j, "%6u(%3u.%1u)  ",
							 psVarying->psSymbolVP->sRegisterInfo.u.uBaseComp,
							 GET_REG_OFFSET(psVarying->psSymbolVP->sRegisterInfo.u.uBaseComp),
							 GET_REG_OFFCOMP(psVarying->psSymbolVP->sRegisterInfo.u.uBaseComp));

				j += sprintf(str+j, "0x%04x   ", psVarying->psSymbolVP->sRegisterInfo.ui32CompUseMask);
			}
			else
			{
				j += sprintf(str+j, "    -            -      ");
			}

			if(psVarying->psSymbolFP)
			{
				j += sprintf(str+j, "%6u(%3u.%1u)  ",
							 psVarying->psSymbolFP->sRegisterInfo.u.uBaseComp,
							 GET_REG_OFFSET(psVarying->psSymbolFP->sRegisterInfo.u.uBaseComp),
							 GET_REG_OFFCOMP(psVarying->psSymbolFP->sRegisterInfo.u.uBaseComp));

				j += sprintf(str+j, "0x%04x   ", psVarying->psSymbolFP->sRegisterInfo.ui32CompUseMask);
			}
			else
			{
				j += sprintf(str+j, "    -            -      ");
			}

			fprintf(gc->pShaderAnalysisHandle, "%s\n", str);
		}

		fprintf(gc->pShaderAnalysisHandle, "\n--------------------------------\n");
		fprintf(gc->pShaderAnalysisHandle, "Special Vertex variables active:\n");
		fprintf(gc->pShaderAnalysisHandle, "--------------------------------\n");
		for(i = 0; i < GLES2_NUM_SPECIALS; i++)
		{
			if(psProgram->sVertex.psActiveSpecials[i])
			{
				fprintf(gc->pShaderAnalysisHandle, "%-15s register %u\n", 
						psProgram->sVertex.psActiveSpecials[i]->pszName,
						GET_REG_OFFSET(psProgram->sVertex.psActiveSpecials[i]->sRegisterInfo.u.uBaseComp));
			}
		}

		fprintf(gc->pShaderAnalysisHandle, "\n--------------------------------\n");
		fprintf(gc->pShaderAnalysisHandle, "Special Fragment variables active:\n");
		fprintf(gc->pShaderAnalysisHandle, "--------------------------------\n");
		for(i = 0; i < GLES2_NUM_SPECIALS; i++)
		{
			if(psProgram->sFragment.psActiveSpecials[i])
			{
				fprintf(gc->pShaderAnalysisHandle, "%-15s register %u\n", 
						psProgram->sFragment.psActiveSpecials[i]->pszName,
						GET_REG_OFFSET(psProgram->sFragment.psActiveSpecials[i]->sRegisterInfo.u.uBaseComp));
			}
		}

		if(psProgram->psVertexShader)
		{
			static IMG_CHAR * const abyVaryingDesc[] =
			{
				"position", "color0", "color1", "tc0", "tc1", "tc2", "tc3", "tc4", "tc5", "tc6", "tc7", 
				"tc8", "tc9", "ptsize", "fog", "clipvertex"
			};

			fprintf(gc->pShaderAnalysisHandle, "\n-------------------------\n");
			fprintf(gc->pShaderAnalysisHandle, "Vertex Shader Output Selects\n");
			fprintf(gc->pShaderAnalysisHandle, "-------------------------\n");
			for(i = 0; i < GLSL_NUM_VARYING_TYPES; i++)
			{
				if(psProgram->sVertex.psSharedState->eActiveVaryingMask & (GLSLVM_POSITION << i))
				{
					fprintf(gc->pShaderAnalysisHandle, "%-10s", abyVaryingDesc[i]);
					if(((1 << i) <= GLSLVM_TEXCOORD9) && ((1 << i) >= GLSLVM_TEXCOORD0))
					{
						fprintf(gc->pShaderAnalysisHandle, "%u", psProgram->sVertex.psSharedState->aui32TexCoordDims[i - 3]);
					}
					fprintf(gc->pShaderAnalysisHandle, "\n");
				}
			}
		}

		fprintf(gc->pShaderAnalysisHandle, "\n--------------------------------\n");
		fprintf(gc->pShaderAnalysisHandle, "VGP Output Remapping:\n");
		fprintf(gc->pShaderAnalysisHandle, "--------------------------------\n");

		for(i = 0; i < NUM_TC_REGISTERS; i++)
		{
			fprintf(gc->pShaderAnalysisHandle, "  Remap Output %-2u from register %-2u \n", 
					psProgram->aui32IteratorPosition[i], i);
		}
	}
#endif /* defined DEBUG */

	return bLinkSuccess;

bad_alloc:
	/* Clean up all memory, set error and return false */
	GLES2Free(IMG_NULL, psProgram->pszInfoLog);
	psProgram->pszInfoLog = IMG_NULL;

	GLES2Free(IMG_NULL, psProgram->sVertex.pfConstantData);
	psProgram->sVertex.pfConstantData = IMG_NULL;

	GLES2Free(IMG_NULL, psProgram->sFragment.pfConstantData);
	psProgram->sFragment.pfConstantData = IMG_NULL;

	GLES2Free(IMG_NULL, psProgram->psActiveVaryings);
	psProgram->psActiveVaryings = IMG_NULL;

	GLES2Free(IMG_NULL, psProgram->psActiveAttributes);
	psProgram->psActiveAttributes = IMG_NULL;

	GLES2Free(IMG_NULL, psProgram->psBuiltInUniforms);
	psProgram->psBuiltInUniforms = IMG_NULL;

	GLES2Free(IMG_NULL, psProgram->ppsActiveUserUniforms);
	psProgram->ppsActiveUserUniforms = IMG_NULL;

	GLES2Free(IMG_NULL, psProgram->psActiveUniforms);
	psProgram->psActiveUniforms = IMG_NULL;

	SetError(gc, GL_OUT_OF_MEMORY);

	return IMG_FALSE;
}

/***********************************************************************************
 Function Name      : SetupDirtyProgramValidationFlags
 Inputs             : 
 Outputs            : None
 Returns            : None
 Description        : Setup validation flags when progam is enabled, disabled 
					  or switched. 
************************************************************************************/
static void SetupDirtyProgramValidationFlags(GLES2Context *gc,
											 IMG_BOOL bOldVP, IMG_BOOL bOldFP,
											 IMG_BOOL bCurrentVP, IMG_BOOL bCurrentFP)
{
	if(bOldVP || bCurrentVP)
	{
		gc->ui32DirtyState |= GLES2_DIRTYFLAG_VERTEX_PROGRAM;
	}

	if(bOldFP || bCurrentFP)
	{
		gc->ui32DirtyState |= GLES2_DIRTYFLAG_FRAGMENT_PROGRAM;
	}
}

/***********************************************************************************
 Function Name      : UseProgram
 Inputs             : gc, program
 Outputs            : -
 Returns            : -
 Description        : Bussiness logic of glUseProgram, safe to be called from within the driver.
************************************************************************************/
static IMG_VOID UseProgram(GLES2Context *gc, GLuint program)
{
	GLES2NamesArray *psNamesArray;
	GLES2Program *psProgram;
	GLES2Program *psCurrentProgram;
	IMG_BOOL bOldVP = IMG_FALSE, bOldFP = IMG_FALSE, bCurrentVP = IMG_FALSE, bCurrentFP = IMG_FALSE;

	/* If a program is bound it cannot be named zero since that's a reserved name */
	GLES_ASSERT(!gc->sProgram.psCurrentProgram || gc->sProgram.psCurrentProgram->sNamedItem.ui32Name != 0);

	if(gc->sProgram.psCurrentProgram && (gc->sProgram.psCurrentProgram->sNamedItem.ui32Name == program))
	{
		/* If it is the one currently in use, just ignore the command */
		return;
	}
	else if(gc->sProgram.psCurrentProgram == IMG_NULL && program == 0)
	{
		/* Unbinding while no current program is bound */
		return;
	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM];

	/* Treat program 0 as an "unbind" command */
	if (program == 0)
	{
		psProgram = IMG_NULL;
	}
	else 
	{
		/*
		** Retrieve the program object from the psNamesArray structure.
		*/
		psProgram = (GLES2Program *) NamedItemAddRef(psNamesArray, program);

		if(!psProgram)
		{
			SetError(gc, GL_INVALID_VALUE);
			return;
		}

		if(psProgram->ui32Type != GLES2_SHADERTYPE_PROGRAM || !psProgram->bSuccessfulLink)
		{
			SetError(gc, GL_INVALID_OPERATION);
			return;
		}

		/*
		** Retrieved an existing program object.  Do some
		** sanity checks.
		*/
		GLES_ASSERT(program == psProgram->sNamedItem.ui32Name);
		GLES_ASSERT(program != 0);
	}

	/*
	** Release program that is being unbound.
	*/
	psCurrentProgram = gc->sProgram.psCurrentProgram;

	if (psCurrentProgram) 
	{
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
		if (psCurrentProgram->bLoadFromBinary)
		{
			bOldVP = bOldFP = IMG_TRUE;
		}
		else
#endif
		{
			if(gc->sProgram.psCurrentProgram->psVertexShader)
			{
				bOldVP = IMG_TRUE;
			}

			if(gc->sProgram.psCurrentProgram->psFragmentShader)
			{
				bOldFP = IMG_TRUE;
			}
		}

		GLES_ASSERT((psCurrentProgram->sNamedItem.ui32RefCount > 1) || psCurrentProgram->bDeleting);
		NamedItemDelRef(gc, psNamesArray, &psCurrentProgram->sNamedItem);
	}

	gc->sProgram.psCurrentProgram = psProgram;
	if (gc->sProgram.psCurrentProgram) 
	{
#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
		if (gc->sProgram.psCurrentProgram->bLoadFromBinary)
		{
			bCurrentVP = bCurrentFP = IMG_TRUE;
		}
		else
#endif
		{
			if(gc->sProgram.psCurrentProgram->psVertexShader)
			{
				bCurrentVP = IMG_TRUE;
			}

			if(gc->sProgram.psCurrentProgram->psFragmentShader)
			{
				bCurrentFP = IMG_TRUE;
			}
		}
	}

	SetupDirtyProgramValidationFlags(gc, bOldVP, bOldFP, bCurrentVP, bCurrentFP);
}


/***********************************************************************************
 Function Name      : glLinkProgram
 Inputs             : program
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Links a program
************************************************************************************/
GL_APICALL void GL_APIENTRY glLinkProgram(GLuint program)
{
	GLES2Program *psProgram;
	IMG_BOOL bOldVP = IMG_FALSE;
	IMG_BOOL bOldFP = IMG_FALSE;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLinkProgram"));

	GLES2_TIME_START(GLES2_TIMES_glLinkProgram);

	/* Are there old vp or fp ? */
	if(gc->sProgram.psCurrentProgram)
	{
		if(gc->sProgram.psCurrentProgram->psVertexShader)
		{
			bOldVP = IMG_TRUE;
		}

		if(gc->sProgram.psCurrentProgram->psFragmentShader)
		{
			bOldFP = IMG_TRUE;
		}
	}

	psProgram = GetNamedProgram(gc, program);

	if(psProgram == IMG_NULL)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto TimeStopAndExit;
	}

	/* First reset the info log to an empty string */
	GLES2Free(IMG_NULL, psProgram->pszInfoLog);
	psProgram->pszInfoLog = IMG_NULL;

	/* Indicate that there's been an attempt to link the program */
	psProgram->bAttemptedLink = IMG_TRUE;

	/*
	** Free and reset all the old state
	*/
	ResetProgramLinkedState(gc, psProgram);

	if(psProgram->psVertexShader   && psProgram->psVertexShader->bSuccessfulCompile &&
	   psProgram->psFragmentShader && psProgram->psFragmentShader->bSuccessfulCompile)
	{
		/*
		** Generate uniform list, attribute list and varying list, assign 
		** each uniform and attribute a location, and more ...
		*/
		if(LinkVertexFragmentPrograms(gc, psProgram))
		{
			psProgram->bSuccessfulLink  = IMG_TRUE;
			psProgram->sVertex.bValid   = IMG_TRUE;
			psProgram->sFragment.bValid = IMG_TRUE;
		}
	}
	else
	{
		static const IMG_CHAR pszVertMissing[]     = "Link Error: Vertex shader is missing.\n";
		static const IMG_CHAR pszVertNotCompiled[] = "Link Error: Vertex shader was not successfully compiled.\n"; 
		static const IMG_CHAR pszFragMissing[]     = "Link Error: Fragment shader is missing.\n";
		static const IMG_CHAR pszFragNotCompiled[] = "Link Error: Fragment shader was not successfully compiled.\n"; 
	
		if(!psProgram->psVertexShader)
		{
			AppendMessageToProgramInfoLog(gc, psProgram, pszVertMissing);
		}
		else if(!psProgram->psVertexShader->bSuccessfulCompile)
		{
			AppendMessageToProgramInfoLog(gc, psProgram, pszVertNotCompiled);
		}

		if(!psProgram->psFragmentShader)
		{
			AppendMessageToProgramInfoLog(gc, psProgram, pszFragMissing);
		}
		else if(!psProgram->psFragmentShader->bSuccessfulCompile)
		{
			AppendMessageToProgramInfoLog(gc, psProgram, pszFragNotCompiled);
		}
	}

	if(psProgram->bSuccessfulLink && psProgram == gc->sProgram.psCurrentProgram)
	{
		SetupDirtyProgramValidationFlags(gc, bOldVP, bOldFP,
										 (psProgram->psVertexShader ? IMG_TRUE : IMG_FALSE),
										 (psProgram->psFragmentShader ? IMG_TRUE : IMG_FALSE));
	}

#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
	/* clear bLoadFromBinary flag to indicate the program is compiled and linked from shader source */
	psProgram->bLoadFromBinary = IMG_FALSE;
#endif

TimeStopAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glLinkProgram);
}


/***********************************************************************************
 Function Name      : glUseProgram
 Inputs             : program
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current program (vertex+fragment shader) to 
					  use in drawing primitives
************************************************************************************/
GL_APICALL void GL_APIENTRY glUseProgram(GLuint program)
{
	__GLES2_GET_CONTEXT();
	PVR_DPF((PVR_DBG_CALLTRACE,"glUseProgram"));

	GLES2_TIME_START(GLES2_TIMES_glUseProgram);

	UseProgram(gc, program);

	GLES2_TIME_STOP(GLES2_TIMES_glUseProgram);
}

/***********************************************************************************
 Function Name      : glValidateProgram
 Inputs             : program
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Validates a program's executable status
                      Semantics specified in section 2.15.4, subsection "Validation"
                      of the Desktop GL 2.0 spec.
************************************************************************************/
GL_APICALL void GL_APIENTRY glValidateProgram(GLuint program)
{
	GLES2Program *psProgram;
	GLES2Uniform *psUniform;
	IMG_UINT32   i, ui32SamplerIndex, ui32NumVertexSamplers, ui32NumFragmentSamplers;

	/* The unit is not referenced in the program */
	const IMG_UINT32 TEXUNIT_NOT_REFERENCED = 0x0U;
	/* The unit is referenced in the vertex shader */
	const IMG_UINT32 TEXUNIT_VERTEX_FLAG    = 0x1U;
	/* The unit is referenced in the fragment shader */
	const IMG_UINT32 TEXUNIT_FRAGMENT_FLAG  = 0x2U;
	/* The unit is read as a 2D texture */
	const IMG_UINT32 TEXUNIT_2D_FLAG        = 0x4U;
	/* The unit is read as a CEM texture */
	const IMG_UINT32 TEXUNIT_CEM_FLAG       = 0x8U;

#if GLES2_MAX_TEXTURE_UNITS != GLES2_MAX_VERTEX_TEXTURE_UNITS
#error "GLES2_MAX_TEXTURE_UNITS != GLES2_MAX_VERTEX_TEXTURE_UNITS but this code assumes they are equal."
#endif
	/* For each texture unit we store the corresponding flags declared above */
	IMG_UINT32 aui32TexUnit[GLES2_MAX_TEXTURE_UNITS];

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glValidateProgram"));

	GLES2_TIME_START(GLES2_TIMES_glValidateProgram);

	psProgram = GetNamedProgram(gc, program);

	if(psProgram)
	{
		/*
		 * According to the spec (section 2.15.4, subsection "Validation" of GL 2.0),
		 *
		 *    VALIDATE_STATUS is set to false if any of these is true:
		 *
		 *    1- Any two active samplers in the current program object are of different types,
		 *       but refer to the same texture image unit
		 *    2- The number of active samplers in the program exceeds
		 *       the total number of texture image units allowed.
		 *
		 *    ValidateProgram will check for all the conditions that could lead to an
		 *    INVALID OPERATION error when rendering commands are issued, and may check
		 *    for other conditions as well. [...] If validation succeeded the program 
		 *    object is guaranteed to execute, given the current GL state. If validation 
		 *    failed, the program object is guaranteed to not execute, given the current
		 *    GL state.
		 */
		psProgram->bSuccessfulValidate = IMG_FALSE;
		GLES2Free(IMG_NULL, psProgram->pszInfoLog);
		psProgram->pszInfoLog = IMG_NULL;

		if(!psProgram->bSuccessfulLink)
		{
			/* The program didn't even link. The calling app is teh suxx0r */
			goto StopTimeAndReturn;
		}

		/* Initialize the texture units' flags */
		GLES2MemSet(aui32TexUnit, (IMG_INT32)TEXUNIT_NOT_REFERENCED, GLES2_MAX_TEXTURE_UNITS*sizeof(IMG_UINT32));

		/* Iterate all uniforms marking their usage of texture units */
		for(i = 0; i < psProgram->ui32NumActiveUniforms; i++)
		{
			psUniform = &psProgram->psActiveUniforms[i];

			/* Is this uniform a sampler? */
			if(GLES2_IS_SAMPLER(psUniform->eTypeSpecifier))
			{
				ui32SamplerIndex = 0xDEADBEEF;

				/* Is this sampler active in the vertex shader? */
				if(psUniform->psSymbolVP)
				{
					ui32SamplerIndex = psUniform->ui32VPSamplerIndex;
					aui32TexUnit[ui32SamplerIndex] |= TEXUNIT_VERTEX_FLAG;
				}

				/* Is this sampler active in the fragment shader? */
				if(psUniform->psSymbolFP)
				{
					ui32SamplerIndex = psUniform->ui32FPSamplerIndex;
					aui32TexUnit[ui32SamplerIndex] |= TEXUNIT_FRAGMENT_FLAG;
				}

				/* Is this sampler 2D or CEM? */
				GLES_ASSERT(ui32SamplerIndex != 0xDEADBEEF);
				if (ui32SamplerIndex < GLES2_MAX_TEXTURE_UNITS)
					aui32TexUnit[ui32SamplerIndex] |= ((psUniform->eTypeSpecifier == GLSLTS_SAMPLER2D)? TEXUNIT_2D_FLAG : TEXUNIT_CEM_FLAG);
			}
		}

		/* Iterate all texture units and check the conditions above */
		ui32NumVertexSamplers   = 0;
		ui32NumFragmentSamplers = 0;

		for(i = 0; i < GLES2_MAX_TEXTURE_UNITS; i++)
		{
			/* Is this unit accessed from the vertex shader? */
			if(aui32TexUnit[i] & TEXUNIT_VERTEX_FLAG)
			{
				if(++ui32NumVertexSamplers > GLES2_MAX_VERTEX_TEXTURE_UNITS)
				{
					/* Using too many samplers in the vertex shader */
					goto StopTimeAndReturn;
				}
			}

			/* Is this unit accessed from the fragment shader? */
			if(aui32TexUnit[i] & TEXUNIT_FRAGMENT_FLAG)
			{
				if(++ui32NumFragmentSamplers > GLES2_MAX_TEXTURE_UNITS)
				{
					/* Using too many samplers in the fragment shader */
					goto StopTimeAndReturn;
				}
			}

			/* Is this unit accessed both as 2D and as CEM in this program? */
			if( ((aui32TexUnit[i] & TEXUNIT_2D_FLAG) != 0) && ((aui32TexUnit[i] & TEXUNIT_CEM_FLAG) != 0) )
			{
				/* Samplers of different types pointing to same unit */
				goto StopTimeAndReturn;
			}
		}

		/* The program didn't fail any of the tests */
		psProgram->bSuccessfulValidate = IMG_TRUE;
	}

StopTimeAndReturn:
	GLES2_TIME_STOP(GLES2_TIMES_glValidateProgram);
}

/***********************************************************************************
 Function Name      : glCompileShader
 Inputs             : shader
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Compiles a source shader
************************************************************************************/
#if defined(SUPPORT_SOURCE_SHADER)
GL_APICALL void GL_APIENTRY glCompileShader (GLuint shader)
{
	IMG_UINT32 ui32InfoLogLength;
	GLES2Shader *psShader;
	GLSLProgramType eProgramType;
	GLSLUniFlexHWCodeInfo sUniFlexInfo;
	UNIFLEX_PROGRAM_PARAMETERS sUniFlexParams;
	GLSLCompileProgramContext sCompileContext = {0};
	GLSLCompileUniflexProgramContext sCompileUniflexContext;
	GLSLCompiledUniflexProgram *psCompiledProgram;
#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE)
	IMG_CHAR szHashStr[DIGEST_STRING_LENGTH];
#endif

	__GLES2_GET_CONTEXT();

	GLES2MemSet(&sUniFlexInfo, 0, sizeof(GLSLUniFlexHWCodeInfo));
	GLES2MemSet(&sUniFlexParams, 0, sizeof(UNIFLEX_PROGRAM_PARAMETERS));

	PVR_DPF((PVR_DBG_CALLTRACE,"glCompileShader"));

	GLES2_TIME_START(GLES2_TIMES_glCompileShader);

	psShader = GetNamedShader(gc, shader);
	
	if(psShader == IMG_NULL)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glCompileShader);
		return;
	}

	psShader->bSuccessfulCompile = IMG_FALSE;

	eProgramType = (psShader->ui32Type == GLES2_SHADERTYPE_VERTEX) ? GLSLPT_VERTEX : GLSLPT_FRAGMENT;

#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE)
	if(psShader->pszSource)
	{
		IMG_VOID *pvBinary = IMG_NULL;
		IMG_UINT32 ui32BinarySize = 0;

		DigestTextToHashString(psShader->pszSource, szHashStr);

		ui32BinarySize = KEGLGetBlob(szHashStr, DIGEST_STRING_LENGTH, pvBinary, ui32BinarySize);

		if(ui32BinarySize)
		{
			SGXBS_Error eError;
			GLES2SharedShaderState *psSharedShaderState = IMG_NULL;
			IMG_UINT32 ui32ReturnSize;

			pvBinary = GLES2Malloc(gc, ui32BinarySize);

			if(pvBinary == IMG_NULL)
			{
				goto NoBinary;
			}

			ui32ReturnSize = KEGLGetBlob(szHashStr, DIGEST_STRING_LENGTH, pvBinary, ui32BinarySize);

			if (ui32ReturnSize != ui32BinarySize) 
			{
				GLES2Free(IMG_NULL, pvBinary);
				goto NoBinary;
			}

			/*
			* Try to unpack the binary. Return errors if necessary.
			*/
			eError = SGXBS_CreateSharedShaderState(gc, pvBinary, ui32BinarySize, (psShader->ui32Type == GLES2_SHADERTYPE_VERTEX)? IMG_TRUE : IMG_FALSE,
													IMG_TRUE, gc->sProgram.pvUniPatchContext, &psSharedShaderState);


			GLES2Free(IMG_NULL, pvBinary);

			if(eError != SGXBS_NO_ERROR || !psSharedShaderState)
			{
				psShader->psSharedState = IMG_NULL;
				goto NoBinary;
			}

			/* Remove previous shared state (or drop refcount) */
			SharedShaderStateDelRef(gc, psShader->psSharedState);
			psShader->psSharedState = psSharedShaderState;

			psShader->bSuccessfulCompile = IMG_TRUE;

			GLES2_TIME_STOP(GLES2_TIMES_glCompileShader);
			return;
		}

	}
NoBinary:
#endif

	/* Must be able to fit a 2x2 block in */
	sUniFlexParams.uNumAvailableTemporaries = gc->psSysContext->sHWInfo.ui32NumUSETemporaryRegisters >> 2;

	if (eProgramType == GLSLPT_FRAGMENT)
	{
		sUniFlexParams.uConstantBase		= GLES2_FRAGMENT_SECATTR_CONSTANTBASE;
		sUniFlexParams.uIndexableTempBase	= GLES2_FRAGMENT_SECATTR_INDEXABLETEMPBASE;
		sUniFlexParams.uScratchBase			= GLES2_FRAGMENT_SECATTR_SCRATCHBASE;

		sUniFlexParams.uInRegisterConstantOffset = GLES2_FRAGMENT_SECATTR_NUM_RESERVED;
		sUniFlexParams.uInRegisterConstantLimit = PVR_MAX_PS_SECONDARIES - sUniFlexParams.uInRegisterConstantOffset;

		sUniFlexParams.uPackDestType = USEASM_REGTYPE_PRIMATTR;
		sUniFlexParams.uPackPrecision = 5; /* Arbitrary choice */
		sUniFlexParams.uExtraPARegisters = 0;
	}
	else
	{
		sUniFlexParams.uConstantBase		= GLES2_VERTEX_SECATTR_CONSTANTBASE;
		sUniFlexParams.uIndexableTempBase	= GLES2_VERTEX_SECATTR_INDEXABLETEMPBASE;
		sUniFlexParams.uScratchBase			= GLES2_VERTEX_SECATTR_SCRATCHBASE;
	
		sUniFlexParams.uInRegisterConstantOffset = GLES2_VERTEX_SECATTR_NUM_RESERVED;
		sUniFlexParams.uInRegisterConstantLimit = PVR_MAX_VS_SECONDARIES - sUniFlexParams.uInRegisterConstantOffset;

		sUniFlexParams.uExtraPARegisters = 0;
	}

	sUniFlexParams.ePredicationLevel = UF_PREDLVL_AUTO;
	sUniFlexParams.uMaxALUInstsToFlatten = 0;

	sUniFlexInfo.psUFParams = &sUniFlexParams;

	sCompileUniflexContext.eOutputCodeType = GLSLPF_UNIFLEX_OUTPUT;
	sCompileUniflexContext.psUniflexHWCodeInfo = &sUniFlexInfo;
	sCompileUniflexContext.psCompileProgramContext = &sCompileContext;

#if !defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	/* Unconditionally create the MSAA trans version of the shader, in case it is used with a MSAA surface 
	 * after being compiled while a non-MSAA surface is bound.
	 */
	if(eProgramType == GLSLPT_FRAGMENT)
	{
		sCompileUniflexContext.bCompileMSAATrans = IMG_TRUE;
	}
	else
#endif
	{
		sCompileUniflexContext.bCompileMSAATrans = IMG_FALSE;
	}

	if(!gc->sProgram.hGLSLCompiler && !InitializeGLSLCompiler(gc))
	{
		GLES2_TIME_STOP(GLES2_TIMES_glCompileShader);
		return;
	}

	sCompileContext.psInitCompilerContext = &gc->sProgram.sInitCompilerContext;
	sCompileContext.eProgramType = eProgramType;
	sCompileContext.ppszSourceCodeStrings = &psShader->pszSource;
	sCompileContext.uNumSourceCodeStrings = 1;

	sCompileContext.bCompleteProgram = IMG_TRUE;
	sCompileContext.bDisplayMetrics = IMG_FALSE;
	sCompileContext.bValidateOnly = IMG_FALSE;
	sCompileContext.eEnabledWarnings = gc->sAppHints.ui32GLSLEnabledWarnings;

	psCompiledProgram = gc->sProgram.sGLSLFuncTable.pfnCompileToUniflex(&sCompileUniflexContext);

	if (!psCompiledProgram)
	{
		PVR_DPF((PVR_DBG_ERROR, "glCompileShader: Failed to compile program\n"));
		GLES2_TIME_STOP(GLES2_TIMES_glCompileShader);
		return;
	}

	/* Remove previous shared state (or drop refcount) */
	SharedShaderStateDelRef(gc, psShader->psSharedState);
	psShader->psSharedState = IMG_NULL;

	/* Allocate memory for the info log then copy it */
	ui32InfoLogLength = strlen(psCompiledProgram->sInfoLog.pszInfoLogString);
	psShader->pszInfoLog = GLES2Calloc(gc, ui32InfoLogLength+1);

	if(psShader->pszInfoLog)
	{
		GLES2MemCopy(psShader->pszInfoLog, psCompiledProgram->sInfoLog.pszInfoLogString, ui32InfoLogLength);
	}
	else
	{
		SetError(gc, GL_OUT_OF_MEMORY);
	}

	if(psCompiledProgram->bSuccessfullyCompiled)
	{
#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE)
		if(psShader->pszSource)
		{
			SGXBS_Error eError;
			IMG_VOID *pvBinary = IMG_NULL;
			IMG_UINT32 ui32BinarySize = 0;

			eError = gc->sProgram.sGLSLFuncTable.pfnCreateBinaryShader(psCompiledProgram, UniPatchMalloc, UniPatchFree, &pvBinary, &ui32BinarySize);

			if(eError == SGXBS_NO_ERROR)
			{
				KEGLSetBlob(szHashStr, DIGEST_STRING_LENGTH, pvBinary, ui32BinarySize);
				UniPatchFree(pvBinary);
			}
		}
#endif

		psShader->psSharedState = CreateSharedShaderState(gc, psCompiledProgram);

		if (psShader->psSharedState == IMG_NULL) 
		{
			GLES2Free(IMG_NULL, psShader->pszInfoLog);
			psShader->pszInfoLog = IMG_NULL;
			SetError(gc, GL_OUT_OF_MEMORY);
		}
		else
		{
			psShader->bSuccessfulCompile = IMG_TRUE;
		}
	}
	
	/* We have copied all the information we want out of the compiledprogram - now free it */
	gc->sProgram.sGLSLFuncTable.pfnFreeCompiledUniflexProgram(&gc->sProgram.sInitCompilerContext,
																 psCompiledProgram);
	
	GLES2_TIME_STOP(GLES2_TIMES_glCompileShader);
}

#else

GL_APICALL void GL_APIENTRY glCompileShader (GLuint shader)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glCompileShader"));
}

#endif /* defined(SUPPORT_SOURCE_SHADER) */


/***********************************************************************************
 Function Name      : glReleaseShaderCompiler
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Releases resources associated with the shader compiler
************************************************************************************/
GL_APICALL void GL_APIENTRY glReleaseShaderCompiler(void)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glReleaseShaderCompiler"));

#if defined(SUPPORT_SOURCE_SHADER)

	GLES2_TIME_START(GLES2_TIMES_glReleaseShaderCompiler);

	DestroyGLSLCompiler(gc);

	GLES2_TIME_STOP(GLES2_TIMES_glReleaseShaderCompiler);
#endif
}

/***********************************************************************************
 Function Name      : glShaderSource
 Inputs             : shader, count, string, length
 Outputs            : 
 Returns            : -
 Description        : ENTRYPOINT: Provides a shader as a set if GLSL ES source strings
************************************************************************************/
GL_APICALL void GL_APIENTRY glShaderSource(GLuint shader, GLsizei count, const char **string, const GLint *length)
{
	GLES2Shader *psShader;
	IMG_INT32 i;
	IMG_UINT32  ui32SourceLength = 1;
	IMG_CHAR    *pszString, *pszNewSource;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glShaderSource"));

	GLES2_TIME_START(GLES2_TIMES_glShaderSource);

	if(count < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndReturn;
	}

	psShader = GetNamedShader(gc, shader);

	if(psShader == IMG_NULL)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndReturn;
	}

	/* Work out the total string length */
	for(i = 0; i < count; i++)
	{
		if((length) && (length[i] > 0))
		{
			ui32SourceLength += (IMG_UINT32)length[i];
		}
		else
		{
			ui32SourceLength += strlen(string[i]);
		}
	}

	pszNewSource = GLES2Realloc(gc, psShader->pszSource, ui32SourceLength);

	if(!pszNewSource) 
	{
		SetError(gc, GL_OUT_OF_MEMORY);
		goto StopTimeAndReturn;
	}
	psShader->pszSource = pszNewSource;

	/* Copy the string over */
	pszString = psShader->pszSource;

	for(i = 0; i < count; i++)
	{
		if((length) && (length[i] > 0))
		{
			GLES2MemCopy(pszString, (const IMG_VOID *)string[i], (IMG_UINT32)length[i]);
			pszString += length[i];
		}
		else
		{
			strcpy(pszString, string[i]);
			pszString += strlen(string[i]);
		}
	}

	/* NULL termination */
	pszString[0] = '\0';

	/* Initialise other variables */
	psShader->bSuccessfulCompile = IMG_FALSE;
	psShader->bDeleting = IMG_FALSE;
	GLES2Free(IMG_NULL, psShader->pszInfoLog);
	psShader->pszInfoLog = IMG_NULL;

	/* Dump shader */
#if defined DEBUG
	if(gc->pShaderAnalysisHandle)
	{
		GLsizei j;

		if (!gc->ui32ShaderCount)
		{
			fprintf(gc->pShaderAnalysisHandle, "Shader Analysis\n");
			fprintf(gc->pShaderAnalysisHandle, "---------------\n\n");
		}

		for (j = 0; j < count; j++)
		{
			char *cp = (char *)string[j];

			fprintf(gc->pShaderAnalysisHandle, "------------ \n");
			fprintf(gc->pShaderAnalysisHandle, "SHADER: Name(%u) ", psShader->sNamedItem.ui32Name);
					
			if(psShader->ui32Type == GLES2_SHADERTYPE_VERTEX)
			{
				fprintf(gc->pShaderAnalysisHandle, "(Vertex Shader) \n");
			}
			else
			{
				fprintf(gc->pShaderAnalysisHandle, "(Fragment Shader) \n");
			}
			fprintf(gc->pShaderAnalysisHandle, "------------ \n");
			fprintf(gc->pShaderAnalysisHandle,"%s", cp);
			fprintf(gc->pShaderAnalysisHandle,"\n\n");
			gc->ui32ShaderCount++;
		}
	}
#endif /* defined DEBUG */

StopTimeAndReturn:
	GLES2_TIME_STOP(GLES2_TIMES_glShaderSource);
}


/***********************************************************************************
 Function Name      : glShaderBinary
 Inputs             : n, shaders, binaryformat, binary, length
 Outputs            : 
 Returns            : -
 Description        : ENTRYPOINT: Provides a shader as a predefined binary format
************************************************************************************/
#if defined(SUPPORT_BINARY_SHADER)
GL_APICALL void GL_APIENTRY glShaderBinary(GLint n, const GLuint *shaders, GLenum binaryformat, 
										  const void *binary, GLint length)
{
	GLES2Shader                   *psShader;
	SGXBS_Error                   eError;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glShaderBinary"));

	GLES2_TIME_START(GLES2_TIMES_glShaderBinary);

	/*
	 * Argument validation.
	 */
	if(binaryformat != GL_SGX_BINARY_IMG)
	{
		SetError(gc, GL_INVALID_ENUM);
		goto StopTimeAndReturn;
	}

	if(!shaders || !binary || n != 1)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndReturn;
	}

	/*
	 * Fetch the shader and fail if the name was unbound.
	 */
	psShader = GetNamedShader(gc, shaders[0]);

	if(!psShader)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto StopTimeAndReturn;
	}

	psShader->pszInfoLog = IMG_NULL;
	psShader->pszSource = IMG_NULL;

	/* Decrease refcount of old shared state */
	SharedShaderStateDelRef(gc, psShader->psSharedState);

	/* Initially assume the shader won't compile */
	psShader->bSuccessfulCompile = IMG_FALSE;

	/*
	 * Try to unpack the binary. Return errors if necessary.
	 */
	eError = SGXBS_CreateSharedShaderState(gc, binary, length, (psShader->ui32Type == GLES2_SHADERTYPE_VERTEX)? IMG_TRUE : IMG_FALSE,
											IMG_FALSE, gc->sProgram.pvUniPatchContext, &psShader->psSharedState);

	if(eError != SGXBS_NO_ERROR)
	{
		psShader->psSharedState = IMG_NULL;

		if((SGXBS_CORRUPT_BINARY_ERROR == eError) || (SGXBS_INVALID_ARGUMENTS_ERROR == eError))
		{
			// Do something like: SetShaderInfoLog(gc, psShader, "Error: could not load corrupt binary data.\n");
			SetError(gc, GL_INVALID_VALUE);
			goto StopTimeAndReturn;
		}

		/* Internal errors and out-of-memory errors are treated equally as "out of memory" */
		SetError(gc, GL_OUT_OF_MEMORY);
		goto StopTimeAndReturn;
	}
	else if(!psShader->psSharedState)
	{
		/* If this happens, there is a bug in the unpacker. */
		PVR_DPF((PVR_DBG_ERROR, "glShaderBinary: SGXBS_CreateSharedShaderState did not return an error but the returned pointer is NULL"));
		SetError(gc, GL_OUT_OF_MEMORY);
		goto StopTimeAndReturn;
	}

	psShader->bSuccessfulCompile = IMG_TRUE;

StopTimeAndReturn:

	GLES2_TIME_STOP(GLES2_TIMES_glShaderBinary);
}
#else

GL_APICALL void GL_APIENTRY glShaderBinary(GLint n, const GLuint *shaders, GLenum binaryformat, 
										  const void *binary, GLint length)
{
	__GLES2_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glShaderBinary"));

	SetError(gc, GL_INVALID_ENUM);


}

#endif /* defined(SUPPORT_BINARY_SHADER) */

#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)

/***********************************************************************************
 Function Name      : glGetProgramBinaryOES
 Inputs             : program, bufSize, binaryFormat
 Outputs            : length, binary
 Returns            : -
 Description        : ENTRYPOINT: Provides a binary with current program stored
************************************************************************************/

/* 
	Note: GetProgramBinaryOES is usefull only if there is an online compiler;
			on the other hand ProgramBinaryOES does not depend on the compiler
			(that is, the binary unpacking code is part of the GLES2 driver
*/
#if defined(SUPPORT_SOURCE_SHADER)

GL_API_EXT void GL_APIENTRY glGetProgramBinaryOES (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary)
{
	IMG_UINT32 i;
	GLES2Program *psProgram;
	SGXBS_Error eError;
	GLES2SharedShaderState * psVertex;
	GLES2SharedShaderState * psFragment;
	GLSLCompiledUniflexProgram sUniflexProgramVertex;
	GLSLCompiledUniflexProgram sUniflexProgramFragment;
	GLSLUniFlexCode        sCodeVert = {0};
	GLSLUniFlexCode        sCodeFrag = {0};

	__GLES2_GET_CONTEXT();

	GLES2MemSet(&sUniflexProgramVertex, 0, sizeof(GLSLCompiledUniflexProgram));
	GLES2MemSet(&sUniflexProgramFragment, 0, sizeof(GLSLCompiledUniflexProgram));

	PVR_DPF((PVR_DBG_CALLTRACE,"glGetProgramBinaryOES"));
	GLES2_TIME_START(GLES2_TIMES_glGetProgramBinaryOES);

	psProgram = GetNamedProgram(gc, program);

	if((psProgram == IMG_NULL) || !binary || !binaryFormat || (bufSize <= 0))
	{
		SetError(gc, GL_INVALID_VALUE);
		goto TimeStopAndExit;
	}

	if(!psProgram->bSuccessfulLink)
	{
		SetError(gc, GL_INVALID_OPERATION);
		goto TimeStopAndExit;
	}
	
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

	/*****************************************************
		 pack program 
	******************************************************/	
	eError = gc->sProgram.sGLSLFuncTable.pfnCreateBinaryProgram(&sUniflexProgramVertex, &sUniflexProgramFragment, psProgram->psUserBinding, (IMG_UINT32)bufSize, (IMG_UINT32 *) length, binary, IMG_TRUE);

	if(eError != SGXBS_NO_ERROR)
	{
		if(eError == SGXBS_OUT_OF_MEMORY_ERROR)
		{
			if(length)
			{
				*length = 0;
			}
			/* this error is required by the spec */
			SetError(gc, GL_INVALID_OPERATION);
			goto TimeStopAndExit;
		}
		else if(eError == SGXBS_INVALID_ARGUMENTS_ERROR)
		{
			SetError(gc, GL_INVALID_VALUE);
			goto TimeStopAndExit;
		}
		else 
		{
			/* this is our way to communicate the developer
			something went wrong internally */
			SetError(gc, GL_OUT_OF_MEMORY);
			goto TimeStopAndExit;
		}
	}

	/* 
		The format returned should match that
		retrieved by quering GL_PROGRAM_BINARY_FORMATS_OES
	*/	
	*binaryFormat = GL_SGX_PROGRAM_BINARY_IMG;

TimeStopAndExit:
	/* clean up precompiled shaders */
	if(sUniflexProgramVertex.psUniFlexCode != NULL && sUniflexProgramVertex.psUniFlexCode->psUniPatchInput)
	{
		PVRUniPatchDestroyPCShader(gc->sProgram.pvUniPatchContext, sUniflexProgramVertex.psUniFlexCode->psUniPatchInput);
	}
	if(sUniflexProgramFragment.psUniFlexCode != NULL)
	{
		if (sUniflexProgramFragment.psUniFlexCode->psUniPatchInput)
		{
			PVRUniPatchDestroyPCShader(gc->sProgram.pvUniPatchContext, sUniflexProgramFragment.psUniFlexCode->psUniPatchInput);
		}
		if(sUniflexProgramFragment.psUniFlexCode->psUniPatchInputMSAATrans)
		{
			PVRUniPatchDestroyPCShader(gc->sProgram.pvUniPatchContext, sUniflexProgramFragment.psUniFlexCode->psUniPatchInputMSAATrans);
		}	
	}

	GLES2_TIME_STOP(GLES2_TIMES_glGetProgramBinaryOES);
}

#else /* defined(SUPPORT_BINARY_SHADER) */

GL_API_EXT void GL_APIENTRY glGetProgramBinaryOES (GLuint program, GLsizei bufSize, GLsizei *length, GLenum *binaryFormat, void *binary)
{
	__GLES2_GET_CONTEXT();
	
	PVR_DPF((PVR_DBG_CALLTRACE,"glGetProgramBinaryOES"));

	PVR_DPF((PVR_DBG_ERROR, "glGetProgramBinaryOES: online compiler is not supported\n"));

	SetError(gc, GL_INVALID_ENUM);
}

#endif /* defined(SUPPORT_BINARY_SHADER) */

/***********************************************************************************
 Function Name      : glProgramBinaryOES
 Inputs             : program, binaryFormat, binary, length
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Loads a program from a program binary
************************************************************************************/
GL_API_EXT void GL_APIENTRY glProgramBinaryOES (GLuint program, GLenum binaryFormat, const void *binary, GLint length)
{
	GLES2Program *psProgram;
	GLES2Shader * psAttachedVertexShader;
	GLES2Shader * psAttachedFragmentShader;
	GLES2SharedShaderState * psVertexState;
	GLES2SharedShaderState * psFragmentState;
	GLES2SharedShaderState sVertexState;
	GLES2SharedShaderState sFragmentState;
	GLES2Shader sVertexShader;
	GLES2Shader sFragmentShader;
	GLSLAttribUserBinding * psBinding, * psTmp;
	SGXBS_Error eError;
	IMG_BOOL  bOldVP = IMG_FALSE;
	IMG_BOOL  bOldFP = IMG_FALSE;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glProgramBinaryOES"));
	GLES2_TIME_START(GLES2_TIMES_glProgramBinaryOES);

	psProgram = GetNamedProgram(gc, program);

	if((psProgram == IMG_NULL) || !binary || !length)
	{
		SetError(gc, GL_INVALID_VALUE);
		goto TimeStopAndExit;
	}

	if(binaryFormat != GL_SGX_PROGRAM_BINARY_IMG)
	{
		SetError(gc, GL_INVALID_ENUM);
		goto TimeStopAndExit;
	}

	/* First reset the info log to an empty string */
	GLES2Free(IMG_NULL, psProgram->pszInfoLog);
	psProgram->pszInfoLog = IMG_NULL;

	/* Are there old vp or fp ? */
	if(gc->sProgram.psCurrentProgram)
	{
		if(gc->sProgram.psCurrentProgram->psVertexShader)
		{
			bOldVP = IMG_TRUE;
		}

		if(gc->sProgram.psCurrentProgram->psFragmentShader)
		{
			bOldFP = IMG_TRUE;
		}
	}

	/*******************************************
			unpack binary 
	********************************************/
	memset(&sVertexState, 0, sizeof(GLES2SharedShaderState));
	memset(&sFragmentState, 0, sizeof(GLES2SharedShaderState));

	psVertexState = &sVertexState;
	psFragmentState = &sFragmentState;

	/* 
		Free user defined attribute bindings already present in the program object,
		as we are about to override this info with the one loaded from the binary
	*/
	psBinding = psProgram->psUserBinding;
	while(psBinding)
	{
		psTmp = psBinding;
		GLES2Free(IMG_NULL, psBinding->pszName);

		psBinding = psBinding->psNext;
		GLES2Free(IMG_NULL, psTmp);
	}

	eError = SGXBS_CreateProgramState(gc, binary, length, gc->sProgram.pvUniPatchContext, &psVertexState, &psFragmentState, &(psProgram->psUserBinding));
	
	if(eError != SGXBS_NO_ERROR)
	{
		if((eError == SGXBS_CORRUPT_BINARY_ERROR) || (eError == SGXBS_INVALID_ARGUMENTS_ERROR))
		{
			static const IMG_CHAR pszCorruptBinary[]     = "Unpack error: could not load corrupt binary data.\n";
			AppendMessageToProgramInfoLog(gc, psProgram, pszCorruptBinary);
			SetError(gc, GL_INVALID_VALUE);
			goto TimeStopAndExit;
		}

		/* Internal errors and out-of-memory errors are treated equally as "out of memory" */
		SetError(gc, GL_OUT_OF_MEMORY);
		goto TimeStopAndExit;
	}
	else
	{
		if(!psVertexState || !psFragmentState)
		{
			/* If this happens, there is a bug in the unpacker. */
			PVR_DPF((PVR_DBG_ERROR, "glProgramBinaryOES: SGXBS_CreateProgramState did not return an error but the returned pointer is NULL"));
			SetError(gc, GL_OUT_OF_MEMORY);
			goto TimeStopAndExit;
		}
	}

	/******************************************* 
				link program 
	********************************************/
	memset(&sVertexShader, 0, sizeof(GLES2Shader));
	memset(&sFragmentShader, 0, sizeof(GLES2Shader));

	sVertexShader.psSharedState        = psVertexState;
	sVertexShader.bSuccessfulCompile   = IMG_TRUE;
	sFragmentShader.psSharedState      = psFragmentState;
	sFragmentShader.bSuccessfulCompile = IMG_TRUE;

	psAttachedVertexShader   = psProgram->psVertexShader;
	psAttachedFragmentShader = psProgram->psFragmentShader;
	psProgram->psVertexShader   = &sVertexShader;
	psProgram->psFragmentShader = &sFragmentShader;

	/* 
		loaded SharedShaderStates are referenced only by the program,
		they don't belong to any shader. That's why we need to decrement
		the refcount
	*/
	/* ENTER CRITICAL SECTION */
	PVRSRVLockMutex(gc->psSharedState->hPrimaryLock);

	if(psProgram->psVertexShader->psSharedState->ui32RefCount > 0)
		psProgram->psVertexShader->psSharedState->ui32RefCount--;

	if(psProgram->psFragmentShader->psSharedState->ui32RefCount > 0)
		psProgram->psFragmentShader->psSharedState->ui32RefCount--;
	
	/* EXIT CRITICAL SECTION */
	PVRSRVUnlockMutex(gc->psSharedState->hPrimaryLock);

	/* Indicate that there's been an attempt to link the program */
	psProgram->bAttemptedLink = IMG_TRUE;

	/*
	** Free and reset all the old state
	*/
	ResetProgramLinkedState(gc, psProgram);

	/*
	** Generate uniform list, attribute list and varying list, assign 
	** each uniform and attribute a location, and more ...
	*/
	if(LinkVertexFragmentPrograms(gc, psProgram))
	{
		psProgram->bSuccessfulLink  = IMG_TRUE;
		psProgram->sVertex.bValid   = IMG_TRUE;
		psProgram->sFragment.bValid = IMG_TRUE;
	}
	
	if(psProgram->bSuccessfulLink && psProgram == gc->sProgram.psCurrentProgram)
	{
		SetupDirtyProgramValidationFlags(gc, bOldVP, bOldFP,
										 (psProgram->psVertexShader ? IMG_TRUE : IMG_FALSE),
										 (psProgram->psFragmentShader ? IMG_TRUE : IMG_FALSE));
	}
	
	/* set bLoadFromBinary flag to indicate the program is loaded from binary */
	psProgram->bLoadFromBinary = IMG_TRUE;

	/* copy back previously attached shaders, in case the program gets re-linked through glLinkProgram */
	psProgram->psVertexShader   = psAttachedVertexShader;
	psProgram->psFragmentShader = psAttachedFragmentShader;

TimeStopAndExit:
	GLES2_TIME_STOP(GLES2_TIMES_glProgramBinaryOES);
}

#endif /* GLES2_EXTENSION_GET_PROGRAM_BINARY */

/***********************************************************************************
 Function Name      : FreeShader
 Inputs             : gc, psShader
 Outputs            : -
 Returns            : -
 Description        : Frees all structures/data associated with a shader
************************************************************************************/
static IMG_VOID FreeShader(GLES2Context *gc, GLES2Shader *psShader)
{
	GLES2Free(IMG_NULL, psShader->pszInfoLog);
	GLES2Free(IMG_NULL, psShader->pszSource);

	SharedShaderStateDelRef(gc, psShader->psSharedState);

	GLES2Free(IMG_NULL, psShader);
}


/***********************************************************************************
 Function Name      : DetachAllShaders
 Inputs             : gc
 Outputs            : 
 Returns            : -
 Description        : Detach all shaders from a program
************************************************************************************/
static IMG_VOID DetachAllShaders(GLES2Context *gc, GLES2Program *psProgram, IMG_BOOL bIsShutdown)
{
	if(!bIsShutdown)
	{
		GLES2NamesArray  *psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_PROGRAM];
		
		if(psProgram->psVertexShader)
		{
			GLES_ASSERT(psProgram->psVertexShader->sNamedItem.ui32RefCount >= 1);

			NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psProgram->psVertexShader);
		}
	
		if(psProgram->psFragmentShader)
		{
			GLES_ASSERT(psProgram->psFragmentShader->sNamedItem.ui32RefCount >= 1);

			NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psProgram->psFragmentShader);
		}
	}

	psProgram->psVertexShader = IMG_NULL;
	psProgram->psFragmentShader = IMG_NULL;
}


/***********************************************************************************
 Function Name      : FreeProgram
 Inputs             : gc, psProgram, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Frees all structures/data associated with a program
************************************************************************************/
static IMG_VOID FreeProgram(GLES2Context *gc, GLES2Program *psProgram, IMG_BOOL bIsShutdown)
{
	GLSLAttribUserBinding    *psBinding, *psTmp;

	/* Free uniforms */
	GLES2Free(IMG_NULL, psProgram->psActiveUniforms);
	GLES2Free(IMG_NULL, psProgram->ppsActiveUserUniforms);
	GLES2Free(IMG_NULL, psProgram->psBuiltInUniforms);

	/* Free user defined attribute binding */
	psBinding = psProgram->psUserBinding;

	while(psBinding)
	{
		psTmp = psBinding;

		GLES2Free(IMG_NULL, psBinding->pszName);

		psBinding = psBinding->psNext;

		GLES2Free(IMG_NULL, psTmp);
	}

	/* Free attributes */
	GLES2Free(IMG_NULL, psProgram->psActiveAttributes);

	/* Free varyings */
	GLES2Free(IMG_NULL, psProgram->psActiveVaryings);

	/* Free info log */
	GLES2Free(IMG_NULL, psProgram->pszInfoLog);

	/* Free constant storage for vertex and fragment shader */
	GLES2Free(IMG_NULL, psProgram->sVertex.pfConstantData);
	GLES2Free(IMG_NULL, psProgram->sFragment.pfConstantData);

	SharedShaderStateDelRef(gc, psProgram->sVertex.psSharedState);
	SharedShaderStateDelRef(gc, psProgram->sFragment.psSharedState);

	DetachAllShaders(gc, psProgram, bIsShutdown);

	FreeListOfVertexUSEVariants(gc, &psProgram->sVertex.psVariant);
	FreeListOfFragmentUSEVariants(gc, &psProgram->sFragment.psVariant);

	ShaderScratchMemDelRef(gc, psProgram->sVertex.psScratchMem);
	ShaderIndexableTempsMemDelRef(gc, psProgram->sVertex.psIndexableTempsMem);

	ShaderScratchMemDelRef(gc, psProgram->sFragment.psScratchMem);
	ShaderIndexableTempsMemDelRef(gc, psProgram->sFragment.psIndexableTempsMem);

	GLES2Free(IMG_NULL, psProgram);
}


/***********************************************************************************
 Function Name      : DisposeProgram
 Inputs             : gc, psItem, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Generic program free function; called from names.c
************************************************************************************/
static IMG_VOID DisposeProgram(GLES2Context *gc, GLES2NamedItem *psItem, IMG_BOOL bIsShutdown)
{
	GLES2Program *psProgram = (GLES2Program *)psItem;

	if(psProgram->ui32Type == GLES2_SHADERTYPE_PROGRAM)
	{
		GLES_ASSERT(bIsShutdown || (psProgram->sNamedItem.ui32RefCount == 0));

		FreeProgram(gc, psProgram, bIsShutdown);
	}
	else
	{
		GLES2Shader *psShader = (GLES2Shader *)psProgram;

		GLES_ASSERT(bIsShutdown || (psShader->sNamedItem.ui32RefCount == 0));

		FreeShader(gc, psShader);
	}
}


/***********************************************************************************
 Function Name      : SetupProgramNameArray
 Inputs             : psNamesArray
 Outputs            : -
 Returns            : -
 Description        : Sets up names array for program/shader objects.
************************************************************************************/

IMG_INTERNAL IMG_VOID SetupProgramNameArray(GLES2NamesArray *psNamesArray)
{
	psNamesArray->pfnFree = DisposeProgram;
}


/***********************************************************************************
 Function Name      : CreateProgramState
 Inputs             : gc
 Outputs            : -
 Returns            : Success
 Description        : Initialises the program state.
************************************************************************************/
IMG_INTERNAL IMG_BOOL CreateProgramState(GLES2Context *gc)
{
    IMG_UINT32 *pui32BufferBase;

#if defined(SUPPORT_SOURCE_SHADER)
	/* The compiler module is loaded on demand, so we don't call InitializeGLSLCompiler() here */
	gc->sProgram.hGLSLCompiler    = 0;
#endif
	gc->sProgram.psCurrentProgram = IMG_NULL;

	gc->sProgram.pvUniPatchContext = PVRUniPatchCreateContext(UniPatchMalloc, UniPatchFree, UniPatchDebugPrint);

	if(!gc->sProgram.pvUniPatchContext)
	{
		PVR_DPF((PVR_DBG_ERROR, "CreateProgramState: Failed to initialise Unipatch !\n"));
		return IMG_FALSE;
	}

	if(GLES2ALLOCDEVICEMEM(gc->ps3DDevData,
						   gc->psSysContext->hUSEFragmentHeap,
						   PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
						   EURASIA_USE_INSTRUCTION_SIZE,	   /* size of NOP/PHAS */
						   EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE,
						   &gc->sProgram.psDummyFragUSECode) == PVRSRV_OK)
	{
		pui32BufferBase = (IMG_UINT32 *)gc->sProgram.psDummyFragUSECode->pvLinAddr;
		
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES) 
		BuildPHASLastPhase ((PPVR_USE_INST) pui32BufferBase, EURASIA_USE1_OTHER2_PHAS_END);
#else
		/* Write a NOP to the buffer */
		BuildNOP((PPVR_USE_INST)pui32BufferBase, EURASIA_USE1_END, IMG_FALSE);
#endif 
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "CreateProgramState: Failed to create Dummy Frag USSE code block\n"));

		gc->psSharedState->psUSEVertexCodeHeap = IMG_NULL;
		gc->psSharedState->psUSEFragmentCodeHeap = IMG_NULL;

		FreeProgramState(gc);

		return IMG_FALSE;
	}

	if(GLES2ALLOCDEVICEMEM(gc->ps3DDevData,
							gc->psSysContext->hUSEVertexHeap,
							PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
							EURASIA_USE_INSTRUCTION_SIZE,  /* size of NOP/PHAS */
							EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE,
							&gc->sProgram.psDummyVertUSECode) == PVRSRV_OK)
	{
		pui32BufferBase = (IMG_UINT32 *)gc->sProgram.psDummyVertUSECode->pvLinAddr;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES) 
		BuildPHASLastPhase ((PPVR_USE_INST) pui32BufferBase, EURASIA_USE1_OTHER2_PHAS_END);
#else
		/* Write a NOP to the buffer */
		BuildNOP((PPVR_USE_INST)pui32BufferBase, EURASIA_USE1_END, IMG_FALSE);
#endif 
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "CreateProgramState: Failed to create Dummy Vert USSE code block\n"));

		gc->psSharedState->psUSEVertexCodeHeap = IMG_NULL;
		gc->psSharedState->psUSEFragmentCodeHeap = IMG_NULL;
	
		GLES2FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psDummyFragUSECode);

		FreeProgramState(gc);

		return IMG_FALSE;
	}

#if defined(FIX_HW_BRN_31988)
	if(GLES2ALLOCDEVICEMEM(gc->ps3DDevData,
						   gc->psSysContext->hUSEFragmentHeap,
						   PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
						   EURASIA_USE_INSTRUCTION_SIZE * 6,	   /* size of PHAS/NOP/TST/LDAD/WDF/NOP */
						   EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE,
						   &gc->sProgram.psBRN31988FragUSECode) == PVRSRV_OK)
	{
	    IMG_UINT32 *pui32Buffer;

		pui32Buffer = pui32BufferBase = (IMG_UINT32 *)gc->sProgram.psBRN31988FragUSECode->pvLinAddr;
		
		BuildPHASLastPhase ((PPVR_USE_INST) pui32BufferBase, 0);

		pui32Buffer += USE_INST_LENGTH;

		pui32Buffer = USEGenWriteBRN31988Fragment(pui32Buffer);

		BuildNOP((PPVR_USE_INST)pui32Buffer, EURASIA_USE1_END, IMG_FALSE);

		GLES_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= EURASIA_USE_INSTRUCTION_SIZE * 5);
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "CreateProgramState: Failed to BRN31988 Frag USSE code block\n"));

		GLES2FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psDummyFragUSECode);
		GLES2FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psDummyVertUSECode);

		gc->psSharedState->psUSEVertexCodeHeap = IMG_NULL;
		gc->psSharedState->psUSEFragmentCodeHeap = IMG_NULL;

		FreeProgramState(gc);

		return IMG_FALSE;
	}
#endif

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : FreeProgramState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Frees the program state on the current context.
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeProgramState(GLES2Context *gc)
{
	/* Unbind the current program */
	UseProgram(gc, 0);

	GLES2FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psDummyFragUSECode);
	GLES2FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psDummyVertUSECode);

#if defined(FIX_HW_BRN_31988)
	GLES2FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psBRN31988FragUSECode);
#endif

#if defined(SUPPORT_SOURCE_SHADER)
	DestroyGLSLCompiler(gc);
#endif

	FreeSpecialUSECodeBlocks(gc);

	/* The current program should already be null thanks to the call to UseProgram(gc, 0) above */
	GLES_ASSERT(!gc->sProgram.psCurrentProgram);
}


/***********************************************************************************
 Function Name      : DestroyUSEShaderVariant
 Inputs             : gc, psUSEVariant
 Outputs            : -
 Returns            : -
 Description        : Destroys the given variant, removing it from the list and freeing
                      all of its memory.
 Limitations        : The variant MUST belong to a program shader.
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyUSEShaderVariant(GLES2Context *gc, GLES2USEShaderVariant *psUSEVariant)
{
	/* *** FRAGMENT *** */
	GLES2PDSCodeVariant   *psPDSVariant, *psPDSVariantNext;
	GLES2USEShaderVariant *psList;
	IMG_UINT32 ui32DummyItem;

	/* Remove this variant from the program list */
	psList = psUSEVariant->psProgramShader->psVariant;

	if(psList == psUSEVariant)
	{
		/* The element was in the head of the list */
		psUSEVariant->psProgramShader->psVariant = psList->psNext;
	}
	else
	{
		/* The element was in the body of the list */
		while(psList)
		{
			if(psList->psNext == psUSEVariant)
			{
				psList->psNext = psUSEVariant->psNext;

				break;
			}

			psList = psList->psNext;
		}

		/* Check that the psUSEVariant was found */
		GLES_ASSERT(psList);
	}

	/* Remove the variant from the KRM list */
	KRM_RemoveResourceFromAllLists(&gc->psSharedState->sUSEShaderVariantKRM, &psUSEVariant->sResource);

	/* Once the lists are OK, destroy the variant */
	PVRUniPatchDestroyHWShader(gc->sProgram.pvUniPatchContext, psUSEVariant->psPatchedShader);

	UCH_CodeHeapFree(psUSEVariant->psCodeBlock);

	USESecondaryUploadTaskDelRef(gc, psUSEVariant->psSecondaryUploadTask);

	ShaderScratchMemDelRef(gc, psUSEVariant->psScratchMem);
	ShaderIndexableTempsMemDelRef(gc, psUSEVariant->psIndexableTempsMem);

	/* Note that vertex shaders do not have PDS variants */
	psPDSVariant = psUSEVariant->psPDSVariant;

	while(psPDSVariant)
	{
		psPDSVariantNext = psPDSVariant->psNext;

		if(!HashTableDelete(gc, &gc->sProgram.sPDSFragmentVariantHashTable, psPDSVariant->tHashValue,  
									  psPDSVariant->pui32HashCompare, psPDSVariant->ui32HashCompareSizeInDWords,
									  &ui32DummyItem))
		{
			PVR_DPF((PVR_DBG_ERROR,"PDS Variant not found in hash table"));
		}

		psPDSVariant = psPDSVariantNext;
	}

	GLES2Free(IMG_NULL, psUSEVariant);
}


/***********************************************************************************
 Function Name      : DestroyUSEShaderVariantGhost
 Inputs             : gc, psUSEVariant
 Outputs            : -
 Returns            : -
 Description        : Destroys the given variant ghost, freeing all of its memory.
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyUSEShaderVariantGhost(GLES2Context *gc, GLES2USEShaderVariantGhost *psUSEVariantGhost)
{
	GLES2PDSCodeVariantGhost *psPDSVariantGhost, *psPDSVariantGhostNext;

	/* Free the code blocks */
	UCH_CodeHeapFree(psUSEVariantGhost->psUSECodeBlock);
	USESecondaryUploadTaskDelRef(gc, psUSEVariantGhost->psSecondaryUploadTask);

	ShaderScratchMemDelRef(gc, psUSEVariantGhost->psScratchMem);
	ShaderIndexableTempsMemDelRef(gc, psUSEVariantGhost->psIndexableTempsMem);

	/* Free the PDS variants' ghosts */
	psPDSVariantGhost = psUSEVariantGhost->psPDSVariantGhost;

	while(psPDSVariantGhost)
	{
		psPDSVariantGhostNext = psPDSVariantGhost->psNext;

		UCH_CodeHeapFree(psPDSVariantGhost->psCodeBlock);

		GLES2Free(IMG_NULL, psPDSVariantGhost);

		psPDSVariantGhost = psPDSVariantGhostNext;
	}

	GLES2Free(IMG_NULL, psUSEVariantGhost);
}



/******************************************************************************
 End of file (shader.c)
******************************************************************************/

