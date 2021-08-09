/******************************************************************************
 * Name         : vertexarrobj.c
 *
 * Copyright    : 2009 by Imagination Technologies Limited.
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
 * $Log: vertexarrobj.c $
 * ,
 *  --- Revision Logs Removed --- 

 *****************************************************************************/

#include "context.h"


/***********************************************************************************
 Function Name      : CreateVertexArrayObjectState
 Inputs             : gc
 Outputs            : -
 Returns            : Success
 Description        : Initialises the vertex array object state and creates name array. 
************************************************************************************/
IMG_INTERNAL IMG_BOOL CreateVertexArrayObjectState(GLES1Context *gc)
{
    gc->sVAOMachine.psActiveVAO = IMG_NULL;

	gc->sVAOMachine.sDefaultVAO.sNamedItem.ui32Name          = 0;

	gc->sVAOMachine.sDefaultVAO.psBoundElementBuffer         = IMG_NULL;

	gc->sVAOMachine.sDefaultVAO.psPDSVertexState             = IMG_NULL;
	gc->sVAOMachine.sDefaultVAO.psPDSVertexShaderProgram     = IMG_NULL;
	gc->sVAOMachine.sDefaultVAO.psMemInfo                    = IMG_NULL;

	gc->sVAOMachine.sDefaultVAO.bUseMemInfo                  = IMG_TRUE;

	gc->sVAOMachine.sDefaultVAO.ui32DirtyMask                = GLES1_DIRTYFLAG_VAO_ALL;

    /* Set the default VAO as the active VAO when creating VAO state */
    gc->sVAOMachine.psActiveVAO = &(gc->sVAOMachine.sDefaultVAO);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : ReclaimVAOMemKRM
 Inputs             : pvContext, psResource
 Outputs            : -
 Returns            : -
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID ReclaimVAOMemKRM(IMG_VOID *pvContext, KRMResource *psResource)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(psResource);

	PVR_DPF((PVR_DBG_WARNING, "ReclaimVAOMemKRM: Called"));
}

/***********************************************************************************
 Function Name      : DestroyVAOGhostKRM
 Inputs             : pvContext, psResource
 Outputs            : -
 Returns            : -
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyVAOGhostKRM(IMG_VOID *pvContext, KRMResource *psResource)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(psResource);

	PVR_DPF((PVR_DBG_WARNING, "DestroyVAOGhostKRM: Called"));
}	


/***********************************************************************************
 Function Name      : WaitUntilVAONotUsed
 Inputs             : gc, psVAO
 Outputs            : -
 Returns            : Success/Failure
 Description        : Waits until a buffer object is no longer needed by the TA
************************************************************************************/
static IMG_BOOL WaitUntilVAONotUsed(GLES1Context *gc, GLES1VertexArrayObject *psVAO)
{
	/*
	** 1 - never used - RETURN TRUE
	**
	** 2 - used in previous TA which has completed - RETURN TRUE
	**
	** 3 - used in TA which hasn't beed kicked - KICK TA, THEN WAIT UNTIL COMPLETE
	**
	** 4 - used in TA which has been kicked, but hasn't completed - WAIT UNTIL COMPLETE
	*/

	/*
	** Case 1 or Case 2
	*/
	if(!KRM_IsResourceNeeded(&gc->sVAOKRM, &psVAO->sResource))
	{
#if defined(PDUMP)

		if(PDUMP_ISCAPTURING(gc))
		{
			if(!gc->sKRMTAStatusUpdate.bPDumpInitialised)
			{
				PDUMP_MEM(gc, gc->sKRMTAStatusUpdate.psMemInfo, 0, sizeof(IMG_UINT32));
				gc->sKRMTAStatusUpdate.bPDumpInitialised=IMG_TRUE;
			}
		}

		PVRSRVPDumpMemPol( gc->ps3DDevData->psConnection,
						   gc->sKRMTAStatusUpdate.psMemInfo,
						   0, 											/* Offset */
						   gc->sKRMTAStatusUpdate.ui32StatusValue - 1, 	/* Value */
						   0xFFFFFFFF, 									/* Mask */
						   PDUMP_POLL_OPERATOR_EQUAL,					/* Operator */
						   0); 											/* Flags */

#endif /* defined(PDUMP) */

		return IMG_TRUE;
	}

	/*
	** Case 3
	*/
	if(gc->psRenderSurface->bPrimitivesSinceLastTA)
	{
		/* Is this buffer object attached to the current kick? */
		if(KRM_IsResourceInUse(&gc->sVAOKRM,
							   gc,
							   &gc->sKRMTAStatusUpdate, 
							   &psVAO->sResource))
		{
			/* Kick the TA and wait for it. */
			if(ScheduleTA(gc, gc->psRenderSurface, GLES1_SCHEDULE_HW_WAIT_FOR_TA) != IMG_EGL_NO_ERROR)
			{
				return IMG_FALSE;
			}
		}
	}

#if defined(PDUMP)

	if(PDUMP_ISCAPTURING(gc))
	{
		if(!gc->sKRMTAStatusUpdate.bPDumpInitialised)
		{
			PDUMP_MEM(gc, gc->sKRMTAStatusUpdate.psMemInfo, 0, sizeof(IMG_UINT32));
			gc->sKRMTAStatusUpdate.bPDumpInitialised=IMG_TRUE;
		}
	}

	PVRSRVPDumpMemPol( gc->ps3DDevData->psConnection,
					   gc->sKRMTAStatusUpdate.psMemInfo,
					   0, 											/* Offset */
					   gc->sKRMTAStatusUpdate.ui32StatusValue - 1, 	/* Value */
					   0xFFFFFFFF, 									/* Mask */
					   PDUMP_POLL_OPERATOR_EQUAL,					/* Operator */
					   0); 											/* Flags */

#endif /* defined(PDUMP) */

	/*
	** Case 4
	*/
	return KRM_WaitUntilResourceIsNotNeeded(&gc->sVAOKRM, &psVAO->sResource, KRM_DEFAULT_WAIT_RETRIES);

}


/***********************************************************************************
 Function Name      : FreeVertexArrayObjectInternalPointers
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Unbinds all vertex array objects, and deletes this also.
************************************************************************************/
static IMG_VOID FreeVertexArrayObjectInternalPointers(GLES1Context *gc, GLES1VertexArrayObject *psVAO)
{
	IMG_UINT32 i;
	GLESBufferObject *psBufObj;
	GLES1NamesArray *psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ];	
  
	GLES1_ASSERT(psVAO);

	/* Unbind any buffer object from VAO's attribute pointer */
	for (i=0; i<GLES1_MAX_ATTRIBS_ARRAY; i++)
	{
		psBufObj = psVAO->asVAOState[i].psBufObj;
			
		if (psBufObj && (psBufObj->sNamedItem.ui32Name != 0))
		{
		    GLES1_ASSERT(psNamesArray);
			
			/* Decrease bufobj's RefCount by 1 */
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psBufObj);
		}
		psVAO->asVAOState[i].psBufObj = IMG_NULL;
	}

	/* Unbind any buffer object from VAO's element buffer object pointer */
	psBufObj = psVAO->psBoundElementBuffer;
		
	if (psBufObj && (psBufObj->sNamedItem.ui32Name != 0))
	{
		GLES1_ASSERT(psNamesArray);

		/* Decrease bufobj's RefCount by 1 */
		NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psBufObj);
	}
	psVAO->psBoundElementBuffer = IMG_NULL;


	/* Free VAO's psPDSVertexState */
	if (psVAO->psPDSVertexState != IMG_NULL)
	{
	    GLES1Free(IMG_NULL, psVAO->psPDSVertexState);

		psVAO->psPDSVertexState = IMG_NULL;
	}

	/* Free VAO's psPDSVertexShaderProgram */
	if (psVAO->psPDSVertexShaderProgram != IMG_NULL)
	{
	    GLES1Free(IMG_NULL, psVAO->psPDSVertexShaderProgram);

		psVAO->psPDSVertexShaderProgram = IMG_NULL;
	}

	/* Free VAO's device memory */
    if (psVAO->psMemInfo)
	{
	    if (!WaitUntilVAONotUsed(gc, psVAO))
		{
			PVR_DPF((PVR_DBG_ERROR,"FreeVertexArrayObjectInternalPointers: Problem freeing VAO's MemInfo"));
		}
		GLES1FREEDEVICEMEM(gc->ps3DDevData, psVAO->psMemInfo);

		psVAO->psMemInfo = IMG_NULL;
	}

}


/***********************************************************************************
 Function Name      : FreeVertexArrayObjectState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Unbinds all vertex array objects, and deletes this also.
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeVertexArrayObjectState(GLES1Context *gc)
{
    GLES1VertexArrayObject *psVAO = gc->sVAOMachine.psActiveVAO;
	GLES1VertexArrayObject *psDefaultVAO = &(gc->sVAOMachine.sDefaultVAO); 

	/* Assert VAO is not NULL */
	GLES1_ASSERT(VAO(gc));

	/* Free VAO's all the internal pointer data and
	   decrease the attached buffer objects' RefCount since they're unbound from the VAO. */
	FreeVertexArrayObjectInternalPointers(gc, psVAO);

	/* Remove VAO's KRM resource */
	KRM_RemoveResourceFromAllLists(&gc->sVAOKRM, &psVAO->sResource);

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)

	if (!VAO_IS_ZERO(gc))
	{
		GLES1NamesArray  *psNamesArray = gc->apsNamesArray[GLES1_NAMETYPE_VERARROBJ - GLES1_MAX_SHAREABLE_NAMETYPE];

		GLES1_ASSERT(psNamesArray);

		/* Unbind the current named VAO from the context by decreasing its RefCount by 1 */
		NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psVAO);

		/* Free the default VAO's all the internal pointer data and 
		   decrease the attached buffer objects' RefCount since they're unbound from the default VAO. */
		FreeVertexArrayObjectInternalPointers(gc, psDefaultVAO);
		
		/* Remove the default VAO's KRM resource */
		KRM_RemoveResourceFromAllLists(&gc->sVAOKRM, &psDefaultVAO->sResource);		
	}
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */


	/* Reset the current VAO as the default VAO */
	gc->sVAOMachine.psActiveVAO = psDefaultVAO;

}


#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)

/***********************************************************************************
 Function Name      : FreeVertexArrayObject
 Inputs             : gc, psVerArrObj
 Outputs            : -
 Returns            : -
 Description        : Frees all structures/data of a vertex array object
************************************************************************************/
static IMG_VOID FreeVertexArrayObject(GLES1Context *gc, GLES1VertexArrayObject *psVAO, IMG_BOOL bIsShutdown)
{

	/* Avoid build warning in release mode */
	PVR_UNREFERENCED_PARAMETER(bIsShutdown);
	GLES1_ASSERT(bIsShutdown || (psVAO->sNamedItem.ui32RefCount == 0));

	/* Free VAO's all the internal pointer data */
    FreeVertexArrayObjectInternalPointers(gc, psVAO);

	/* Remove VAO's KRM resource */
	KRM_RemoveResourceFromAllLists(&gc->sVAOKRM, &psVAO->sResource);
	
	GLES1Free(IMG_NULL, psVAO);
}


/***********************************************************************************
 Function Name      : DisposeVertexArrayObject
 Inputs             : gc, psItem, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Generic vertex array object free function; called from names.c
************************************************************************************/
static IMG_VOID DisposeVertexArrayObject(GLES1Context *gc, GLES1NamedItem *psItem, IMG_BOOL bIsShutdown)
{
    GLES1VertexArrayObject *psVAO = (GLES1VertexArrayObject *)psItem;

	FreeVertexArrayObject(gc, psVAO, bIsShutdown);
}


/***********************************************************************************
 Function Name      : SetupVertexArrayObjectNameArray
 Inputs             : psNamesArray
 Outputs            : -
 Returns            : -
 Description        : Sets up names array for vertex array objects.
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupVertexArrayObjectNameArray(GLES1NamesArray *psNamesArray)
{
    psNamesArray->pfnFree = DisposeVertexArrayObject;
}


/***********************************************************************************
 Function Name      : CreateVertexArrayObject
 Inputs             : gc, ui32Name
 Outputs            : -
 Returns            : Pointer to new framebuffer object structure
 Description        : Creates and initialises a new framebuffer object structure.
************************************************************************************/
static GLES1VertexArrayObject* CreateVertexArrayObject(GLES1Context *gc, IMG_UINT32 ui32Name)
{
    GLES1VertexArrayObject *psVAO;
	
	PVR_UNREFERENCED_PARAMETER(gc);

	psVAO = (GLES1VertexArrayObject *) GLES1Calloc(gc, sizeof(GLES1VertexArrayObject));

	if (psVAO)
	{

	    psVAO->sNamedItem.ui32Name       = ui32Name;

		psVAO->ui32PreviousArrayEnables  = 0;
		psVAO->ui32CurrentArrayEnables   = 0;

		psVAO->psBoundElementBuffer      = IMG_NULL;

		psVAO->psPDSVertexState          = IMG_NULL;
		psVAO->psPDSVertexShaderProgram  = IMG_NULL;
		psVAO->psMemInfo                 = IMG_NULL;
		psVAO->bUseMemInfo               = IMG_TRUE;

		psVAO->ui32DirtyMask             = GLES1_DIRTYFLAG_VAO_ALL;
	}

	return psVAO; /* if GLES1Calloc failed, then return NULL */

}


/***********************************************************************************
 Function Name      : glBindVertexArrayOES
 Inputs             : vertexarray
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glBindVertexArrayOES(GLuint vertexarray)
{
	GLES1NamesArray  *psNamesArray;
	GLES1VertexArrayObject *psCurrentVAO;
	GLES1VertexArrayObject *psVAO;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBindVertexArrayOES"));

	GLES1_TIME_START(GLES1_TIMES_glBindVertexArrayOES);


	GLES1_ASSERT(IMG_NULL != gc->apsNamesArray[GLES1_NAMETYPE_VERARROBJ - GLES1_MAX_SHAREABLE_NAMETYPE]);

	psNamesArray = gc->apsNamesArray[GLES1_NAMETYPE_VERARROBJ - GLES1_MAX_SHAREABLE_NAMETYPE];


	/* Retrieve the vertex array object from the namesArray structure. */
	if (vertexarray) 
	{
	    /* named vertex array object */
		psVAO = (GLES1VertexArrayObject *) NamedItemAddRef(psNamesArray, vertexarray);

		/* Is this the first time that this name has been bound? */
		if (!psVAO)
		{
			/* If so, create a new vertex array object and initialize it */
			psVAO = CreateVertexArrayObject(gc, vertexarray);

			if (!psVAO)
			{
				PVR_DPF((PVR_DBG_ERROR,"glBindVertexArrayOES: CreateVertexArrayObject failed"));
				SetError(gc, GL_OUT_OF_MEMORY);
				GLES1_TIME_STOP(GLES1_TIMES_glBindVertexArrayOES);
				return;
			}	

			GLES1_ASSERT(psVAO->sNamedItem.ui32Name == vertexarray);

			if(!InsertNamedItem(psNamesArray, (GLES1NamedItem*)psVAO))
			{
			    /* ((GLES1NamedItem*)psVerArrObj != IMG_NULL) and
				  (((GLES1NamedItem*)psVerArrObj->ui32Name) != IMG_NULL) 
				  so no GL_OUT_OF_MEMOR error can be generated   		*/

				SetError(gc, GL_INVALID_OPERATION);

				FreeVertexArrayObject(gc, psVAO, IMG_FALSE);
				GLES1_TIME_STOP(GLES1_TIMES_glBindVertexArrayOES);
				return;
			}			

			NamedItemAddRef(psNamesArray, vertexarray);
		}
		else
		{
			/* If not, retrieve an existing vertex array object & Do some sanity checks */
			GLES1_ASSERT(psVAO->sNamedItem.ui32Name == vertexarray);
		}
	}	
	else /* vertexarray == 0 means to bind the default VAO */
	{
		psVAO = &(gc->sVAOMachine.sDefaultVAO);
	}
	
	/* Prepare to unbind the object that is currently bound */
	psCurrentVAO = gc->sVAOMachine.psActiveVAO;

	/* Assert the current and prospective VAOs */
	GLES1_ASSERT(psCurrentVAO);
	GLES1_ASSERT(psVAO);


	/* If the currently bound vao is not this to-be-bound vao */
	if (psCurrentVAO != psVAO)
	{
		if (psCurrentVAO && (((GLES1NamedItem*)psCurrentVAO)->ui32Name != 0))
		{
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psCurrentVAO);
		}

		/* Bind the new VAO to this context */
		gc->sVAOMachine.psActiveVAO = psVAO;

		/* Set vao's dirty flag */
		psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_BINDING; 	   
	}
	
	GLES1_TIME_STOP(GLES1_TIMES_glBindVertexArrayOES);

}
			

/***********************************************************************************
 Function Name      : glDeleteVertexArraysOES
 Inputs             : n, arrays
 Outputs            : 
 Returns            : 
 Description        : ENTRYPOINT: Deletes a list of n arrays.
                      Deletes internal array structures for named vertex array objects,
                      and if they were currently bound, binds0 (rebinds default).
************************************************************************************/
GL_API_EXT void GL_APIENTRY glDeleteVertexArraysOES(GLsizei n, const GLuint *vertexarrays)
{
	GLES1NamesArray  *psNamesArray;
	GLES1VertexArrayObject *psVAO;
	IMG_INT32        i; /* used to compare with n (which is signed integer) */

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDeleteVertexArraysOES"));

	GLES1_TIME_START(GLES1_TIMES_glDeleteVertexArraysOES);

	if (n < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteVertexArraysOES);
		return;
	}
	else if (n == 0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteVertexArraysOES);
		return;
	}

	if (!vertexarrays)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteVertexArraysOES);
		return;
	}

	psNamesArray = gc->apsNamesArray[GLES1_NAMETYPE_VERARROBJ - GLES1_MAX_SHAREABLE_NAMETYPE];

	GLES1_ASSERT(psNamesArray);

	/*
	  If a vertex array object that is being deleted is currently bound,
	  bind the 0 name vertex array object to its target.
	*/
	for (i=0; i < n; i++)
	{
		if(vertexarrays[i] != 0)
		{
			psVAO = gc->sVAOMachine.psActiveVAO;

			/* Free VAO's all the internal pointer data and
			decrease the attached buffer objects' RefCount since they're unbound from the VAO. */
			FreeVertexArrayObjectInternalPointers(gc, psVAO);

			/* Unbind the currently bound VAO from the context. */
			if (psVAO && (psVAO->sNamedItem.ui32Name == vertexarrays[i]))
			{
				/* Decrease VAO's RefCount by 1 */
			    NamedItemDelRef(gc,	psNamesArray, (GLES1NamedItem*)psVAO);

				/* Reset the current VAO to the default VAO */
				gc->sVAOMachine.psActiveVAO = &(gc->sVAOMachine.sDefaultVAO);
			}
		}
	}

	/* Finally, decrement the refcount of all of them */
	NamedItemDelRefByName(gc, psNamesArray, (IMG_UINT32)n, (const IMG_UINT32 *)vertexarrays);

	GLES1_TIME_STOP(GLES1_TIMES_glDeleteVertexArraysOES);

}


/***********************************************************************************
 Function Name      : glGenVertexArraysOES
 Inputs             : n
 Outputs            : arrays
 Returns            : 
 Description        : ENTRYPOINT: Generates a set of n names for vertex array objects
************************************************************************************/
GL_API_EXT void GL_APIENTRY glGenVertexArraysOES(GLsizei n, GLuint *vertexarrays)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGenVertexArraysOES"));

	GLES1_TIME_START(GLES1_TIMES_glGenVertexArraysOES);

	if (n < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES1_TIME_STOP(GLES1_TIMES_glGenVertexArraysOES);
		return;
	}
	else if (n == 0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glGenVertexArraysOES);
		return;
	}

	if (!vertexarrays)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glGenVertexArraysOES);
		return;
	}	

	GLES1_ASSERT(gc->apsNamesArray[GLES1_NAMETYPE_VERARROBJ - GLES1_MAX_SHAREABLE_NAMETYPE] != IMG_NULL);

	NamesArrayGenNames(gc->apsNamesArray[GLES1_NAMETYPE_VERARROBJ - GLES1_MAX_SHAREABLE_NAMETYPE], 
					   (IMG_UINT32)n, 
					   (IMG_UINT32 *)vertexarrays);

	GLES1_TIME_STOP(GLES1_TIMES_glGenVertexArraysOES);

}


/***********************************************************************************
 Function Name      : glIsVertexArrayOES
 Inputs             : vertexarray
 Outputs            : 
 Returns            : true/false
 Description        : Returns whether a named object is a vertex array object
************************************************************************************/
GL_API_EXT GLboolean GL_APIENTRY glIsVertexArrayOES(GLuint vertexarray)
{
	GLES1NamesArray  *psNamesArray;
	GLES1VertexArrayObject *psVAO;

	__GLES1_GET_CONTEXT_RETURN(IMG_FALSE);


	PVR_DPF((PVR_DBG_CALLTRACE,"glIsVertexArrayOES"));

	GLES1_TIME_START(GLES1_TIMES_glIsVertexArrayOES);

	if (vertexarray == 0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glIsVertexArrayOES);
		return GL_FALSE;
	}

	GLES1_ASSERT(gc->apsNamesArray[GLES1_NAMETYPE_VERARROBJ - GLES1_MAX_SHAREABLE_NAMETYPE] != IMG_NULL);
	
	psNamesArray = gc->apsNamesArray[GLES1_NAMETYPE_VERARROBJ - GLES1_MAX_SHAREABLE_NAMETYPE];

	psVAO = (GLES1VertexArrayObject *) NamedItemAddRef(psNamesArray, vertexarray);

	if (psVAO == IMG_NULL)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glIsVertexArrayOES);
		return GL_FALSE;
	}

	NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psVAO);


	GLES1_TIME_STOP(GLES1_TIMES_glIsVertexArrayOES);

	return GL_TRUE;
}


#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */
