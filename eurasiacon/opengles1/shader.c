/******************************************************************************
 * Name         : shader.c
 *
 * Copyright    : 2005-2007 by Imagination Technologies Limited.
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
 *****************************************************************************/

#include "context.h"


/***********************************************************************************
 Function Name      : DestroyHashedPDSVariant
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyHashedPDSVariant(GLES1Context *gc, IMG_UINT32 ui32Item)
{
	GLES1PDSCodeVariant **ppsPDSVariantList, *psPDSVariant;
	GLES1ShaderVariant *psUSEVariant;

	PVR_UNREFERENCED_PARAMETER(gc);

	psUSEVariant = ((GLES1PDSCodeVariant *)ui32Item)->psUSEVariant;
	ppsPDSVariantList = &psUSEVariant->psPDSVariant;

	while(*ppsPDSVariantList)
	{
		if(*ppsPDSVariantList == (GLES1PDSCodeVariant *)ui32Item)
		{
			psPDSVariant = *ppsPDSVariantList;
			*ppsPDSVariantList = psPDSVariant->psNext;
			
			UCH_CodeHeapFree(psPDSVariant->psCodeBlock);
			GLES1Free(gc, psPDSVariant);
			return;
		}
			
		ppsPDSVariantList = &((*ppsPDSVariantList)->psNext);
	}
}


/***********************************************************************************
 Function Name      : DestroyUSEShaderVariant
 Inputs             : gc, psUSEVariant
 Outputs            : -
 Returns            : -
 Description        : Destroys the given variant, removing it from the list and freeing
                      all of its memory.
 Limitations        : The variant MUST belong to a shader.
************************************************************************************/
static IMG_VOID DestroyUSEShaderVariant(GLES1Context *gc, GLES1ShaderVariant *psUSEVariant)
{
	GLES1PDSCodeVariant *psPDSVariant, *psPDSVariantNext;
	GLES1PDSVertexCodeVariant *psPDSVertexVariant, *psPDSVertexVariantNext;
	GLES1ShaderVariant *psList;
	IMG_UINT32 ui32DummyItem;

	/* Remove this variant from the list attached to its shader */
	psList = psUSEVariant->psShader->psShaderVariant;

	if(psList == psUSEVariant)
	{
		/* The element was in the head of the list */
		psUSEVariant->psShader->psShaderVariant = psList->psNext;
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
		GLES1_ASSERT(psList);
	}

	/* Remove the variant from the KRM list */
	KRM_RemoveResourceFromAllLists(&gc->psSharedState->sUSEShaderVariantKRM, &psUSEVariant->sResource);
	
	/* destroy PDS vertex variants, if present */
	psPDSVertexVariant = psUSEVariant->psPDSVertexCodeVariant;
	
	while(psPDSVertexVariant)
	{
		psPDSVertexVariantNext = psPDSVertexVariant->psNext;

		if(psPDSVertexVariant->psCodeBlock)
			UCH_CodeHeapFree(psPDSVertexVariant->psCodeBlock);

		GLES1Free(gc, psPDSVertexVariant);

		psPDSVertexVariant = psPDSVertexVariantNext;
	}

	UCH_CodeHeapFree(psUSEVariant->psCodeBlock);

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

	GLES1Free(gc, psUSEVariant);
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
	GLES1ShaderVariant *psUSEVariant = (GLES1ShaderVariant*)((IMG_UINTPTR_T)psResource -offsetof(GLES1ShaderVariant, sResource));
	GLES1Context	   *gc = (GLES1Context *)pvContext;

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
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(psResource);

	PVR_DPF((PVR_DBG_ERROR,"DestroyUSECodeVariantGhostKRM: Shouldn't be called ever"));
}


/***********************************************************************************
 Function Name      : DestroyVertexVariants
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Deletes all vertex variants
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyVertexVariants(GLES1Context *gc)
{
	GLES1Shader        *psVertexShader = gc->sProgram.psVertex;
	
	if(gc->psRenderSurface)
	{
		/* Kick the TA and wait. Then free the vertex variants. */
		if(ScheduleTA(gc, gc->psRenderSurface, GLES1_SCHEDULE_HW_WAIT_FOR_TA) != IMG_EGL_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR, "DestroyVertexVariants: Kicking the TA failed\n"));

			return;
		}
	}

	while(psVertexShader)
	{
		while(psVertexShader->psShaderVariant)
		{
			/* Note that calling DestroyUSEShaderVariant will update the value of 
			   psVertexShader->psShaderVariant automatically to the next element of the list.
			*/
			DestroyUSEShaderVariant(gc, psVertexShader->psShaderVariant);
		}

		psVertexShader = psVertexShader->psNext;
	}
}


/***********************************************************************************
 Function Name      : FreeShader
 Inputs             : gc, psShader
 Outputs            : -
 Returns            : -
 Description        : Frees all structures/data associated with a shader
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeShader(GLES1Context *gc, GLES1Shader *psShader)
{
	while(psShader->psShaderVariant)
	{
		DestroyUSEShaderVariant(gc, psShader->psShaderVariant);
	}

	if(psShader->pfConstantData)
	{
		GLES1Free(gc, (IMG_VOID*) psShader->pfConstantData);
	}

	GLES1Free(gc, psShader);
}


/***********************************************************************************
 Function Name      : DisposePrograms
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Generic program free function; called from eglglue.c
************************************************************************************/
static IMG_VOID DisposePrograms(GLES1Context *gc)
{
	GLES1Shader *psShader, *psNextShader; 

	psShader = gc->sProgram.psVertex;

	while(psShader)
	{
		psNextShader = psShader->psNext;

		FreeShader(gc,psShader);

		psShader = psNextShader;
	}

	psShader = gc->sProgram.psFragment;

	while(psShader)
	{
		psNextShader = psShader->psNext;

		FreeShader(gc,psShader);

		psShader = psNextShader;
	}

	/* clear the current shaders */
	gc->sProgram.psCurrentVertexShader = IMG_NULL;		
	gc->sProgram.psCurrentFragmentShader = IMG_NULL;
}


/***********************************************************************************
 Function Name      : FreeProgramState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Frees the program state the name array.
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeProgramState(GLES1Context *gc)
{
	DisposePrograms(gc);

	FreeSpecialUSECodeBlocks(gc);
}

/******************************************************************************
 End of file (shader.c)
******************************************************************************/

