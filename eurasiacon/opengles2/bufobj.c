/******************************************************************************
 * Name         : bufobj.c
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited.
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
 * $Log: bufobj.c $
 *****************************************************************************/

#include "context.h"


/***********************************************************************************
 Function Name      : CreateBufObjState
 Inputs             : gc, psSharedContext
 Outputs            : -
 Returns            : Success
 Description        : Initialises the buffer object state and creates/shares the name 
					  array. 
************************************************************************************/
IMG_INTERNAL IMG_BOOL CreateBufObjState(GLES2Context *gc)
{
	IMG_UINT32 i;

	/*
	** Set up the buffer objects for the default buffers.
	*/
	for (i=0; i < GLES2_NUM_BUFOBJ_BINDINGS; i++) 
	{
		gc->sBufferObject.psActiveBuffer[i] = IMG_NULL;
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : FreeBufObjState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Unbinds all buffer objects.
					  If no other contexts are sharing the names array, will delete 
					  this also.
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeBufObjState(GLES2Context *gc)
{
	IMG_UINT32 i;

	/* unbind all the buffer objects */
	for (i=0; i < GLES2_NUM_BUFOBJ_BINDINGS; i++) 
	{
		if(gc->sBufferObject.psActiveBuffer[i])
		{
		    /* Only decrease VBO's refCount by 1, 
			   since only VBO can be bound to the context, while IBO can only be bound to VAO */
		    if (i == ARRAY_BUFFER_INDEX)
			{
				NamedItemDelRef(gc, gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_BUFOBJ], (GLES2NamedItem*)gc->sBufferObject.psActiveBuffer[i]);
			}
			gc->sBufferObject.psActiveBuffer[i] = IMG_NULL;
		}
	}
}


/***********************************************************************************
 Function Name      : CreateBufferObject
 Inputs             : gc, ui32Name
 Outputs            : -
 Returns            : Pointer to new buffer object structure
 Description        : Creates and initialises a new buffer object structure.
************************************************************************************/
static GLES2BufferObject * CreateBufferObject(GLES2Context *gc, IMG_UINT32 ui32Name)
{
	GLES2BufferObject *psBufObj;

	PVR_UNREFERENCED_PARAMETER(gc);

	psBufObj = (GLES2BufferObject *) GLES2Calloc(gc, sizeof(GLES2BufferObject));

	if (psBufObj)
	{
		psBufObj->sNamedItem.ui32Name = ui32Name;

		/* init the buffer object state */
		psBufObj->eIndex              = ARRAY_BUFFER_INDEX;
		psBufObj->eUsage			  = GL_STATIC_DRAW;
		psBufObj->ui32BufferSize	  = 0;
		psBufObj->ui32AllocAlign	  = 0;
		psBufObj->eAccess			  = GL_WRITE_ONLY_OES;

		psBufObj->psMemInfo	= IMG_NULL;
	}

	return psBufObj;
}


/***********************************************************************************
 Function Name      : ReclaimBufferObjectMemKRM
 Inputs             : pvContext, psResource
 Outputs            : -
 Returns            : -
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID ReclaimBufferObjectMemKRM(IMG_VOID *pvContext, KRMResource *psResource)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(psResource);

	PVR_DPF((PVR_DBG_WARNING, "ReclaimBufferObjectMemKRM: Called"));
}	


/***********************************************************************************
 Function Name      : DestroyBufferObjectGhostKRM
 Inputs             : pvContext, psResource
 Outputs            : -
 Returns            : -
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyBufferObjectGhostKRM(IMG_VOID *pvContext, KRMResource *psResource)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(psResource);

	PVR_DPF((PVR_DBG_WARNING, "DestroyBufferObjectGhostKRM: Called"));
}	


/***********************************************************************************
 Function Name      : WaitUntilBufObjNotUsed
 Inputs             : gc, psBufObj
 Outputs            : -
 Returns            : Success/Failure
 Description        : Waits until a buffer object is no longer needed by the TA
************************************************************************************/
static IMG_BOOL WaitUntilBufObjNotUsed(GLES2Context *gc, GLES2BufferObject *psBufObj)
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
	if(!KRM_IsResourceNeeded(&gc->psSharedState->sBufferObjectKRM, &psBufObj->sResource))
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
						   0, 										/* Offset */
						   gc->sKRMTAStatusUpdate.ui32StatusValue - 1, 	/* Value */
						   0xFFFFFFFF, 								/* Mask */
						   PDUMP_POLL_OPERATOR_EQUAL,				/* Operator */
						   0); 										/* Flags */

#endif /* defined(PDUMP) */

		return IMG_TRUE;
	}

	
	/*
	** Case 3
	*/
	if(gc->psRenderSurface->bPrimitivesSinceLastTA)
	{
		/* Is this buffer object attached to the current kick? */
		if(KRM_IsResourceInUse(&gc->psSharedState->sBufferObjectKRM,
								gc,
								&gc->sKRMTAStatusUpdate, 
								&psBufObj->sResource))
		{

			GLES2_INC_COUNT(GLES2_TIMER_SGXKICKTA_BUFDATA_COUNT, 1);

			/* Kick the TA and wait for it. */
			if(ScheduleTA(gc, gc->psRenderSurface, GLES2_SCHEDULE_HW_WAIT_FOR_TA) != IMG_EGL_NO_ERROR)
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
					   0, 										/* Offset */
					   gc->sKRMTAStatusUpdate.ui32StatusValue - 1, 	/* Value */
					   0xFFFFFFFF, 								/* Mask */
					   PDUMP_POLL_OPERATOR_EQUAL,				/* Operator */
					   0); 										/* Flags */

#endif /* defined(PDUMP) */

	/*
	** Case 4
	*/
	return KRM_WaitUntilResourceIsNotNeeded(&gc->psSharedState->sBufferObjectKRM, &psBufObj->sResource, KRM_DEFAULT_WAIT_RETRIES);
}


/***********************************************************************************
 Function Name      : FreeBufferObject
 Inputs             : gc, psBufObj
 Outputs            : -
 Returns            : -
 Description        : Frees all structures/data associated with a buffer object
************************************************************************************/
static IMG_VOID FreeBufferObject(GLES2Context *gc, GLES2BufferObject *psBufObj, IMG_BOOL bIsShutdown)
{
	PVR_UNREFERENCED_PARAMETER(bIsShutdown);
	GLES_ASSERT(bIsShutdown || (psBufObj->sNamedItem.ui32RefCount == 0));

	/* No need to detach bufobj from any referenced pointer:
	   if a draw call refers to any deleted bufobj, 
	   then the result is undefined, mostly crashed. And this is expected */

	/* Free its device memory */
	if (psBufObj->psMemInfo)
	{
		if(!WaitUntilBufObjNotUsed(gc, psBufObj))
		{
			PVR_DPF((PVR_DBG_ERROR,"FreeBufferObject: Problem freeing buffer object"));
		}

		GLES2FREEDEVICEMEM_HEAP(gc, psBufObj->psMemInfo);

#if defined(DEBUG) || defined(TIMING)
		gc->ui32VBOMemCurrent -= psBufObj->psMemInfo->uAllocSize;
#endif /* defined(DEBUG) || defined(TIMING) */
	}

	KRM_RemoveResourceFromAllLists(&gc->psSharedState->sBufferObjectKRM, &psBufObj->sResource);

	GLES2Free(IMG_NULL, psBufObj);
}


/***********************************************************************************
 Function Name      : DisposeBufObject
 Inputs             : gc, psItem, bIsShutdown
 Outputs            : -
 Returns            : -
 Description        : Generic buffer object free function; called from names.c
***********************************************************************************/
static IMG_VOID DisposeBufObject(GLES2Context *gc, GLES2NamedItem *psItem, IMG_BOOL bIsShutdown)
{
	GLES2BufferObject *psBufObj = (GLES2BufferObject *)psItem;

	FreeBufferObject(gc, psBufObj, bIsShutdown);
}


/***********************************************************************************
 Function Name      : SetupBufObjNameArray
 Inputs             : psNamesArray
 Outputs            : -
 Returns            : -
 Description        : Sets up names array for buffer objects.
************************************************************************************/
IMG_INTERNAL IMG_VOID SetupBufObjNameArray(GLES2NamesArray *psNamesArray)
{
	psNamesArray->pfnFree  = DisposeBufObject;
}



/***********************************************************************************
 Function Name      : glBindBuffer
 Inputs             : target, buffer
 Outputs            : -
 Returns            : -
 Description        : Sets current buffer object for subsequent calls.
					  Will create an internal psBufObj structure, but no buffer data 
					  memory is allocated yet. Uses name table.
************************************************************************************/
GL_APICALL void GL_APIENTRY glBindBuffer(GLenum target, GLuint buffer)
{
	GLES2BufferObject *psBufObj = IMG_NULL;
	GLES2BufferObject *psBoundBuffer;
	IMG_UINT32 ui32TargetIndex;
	GLES2NamesArray *psNamesArray;	
	GLES2VertexArrayObject *psVAO;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBindBuffer"));

	GLES2_TIME_START(GLES2_TIMES_glBindBuffer);

	/* Setup VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES_ASSERT(VAO(gc));

	/* check */
	switch(target)
	{
		case GL_ARRAY_BUFFER:
		case GL_ELEMENT_ARRAY_BUFFER:
		{
			ui32TargetIndex = target - GL_ARRAY_BUFFER;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glBindBuffer);

			return;
		}
	}

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_BUFOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_BUFOBJ];

	/*
	** Retrieve or create a buffer object from the namesArray structure.
	*/
	if (buffer) 
	{
	    /* Add RefCount by 1 when binding the bufobj to the context */
		psBufObj = (GLES2BufferObject *) NamedItemAddRef(psNamesArray, buffer);

		/*
		** Is this the first time this name has been bound?
		** If so, create a new object and initialize it.
		*/
		if (!psBufObj)
		{
			psBufObj = CreateBufferObject(gc, buffer);

			if(psBufObj == IMG_NULL)
			{
				PVR_DPF((PVR_DBG_ERROR,"glBindBuffer: CreateBufferObject failed"));

				SetError(gc, GL_OUT_OF_MEMORY);

				GLES2_TIME_STOP(GLES2_TIMES_glBindBuffer);

				return;
			}

			GLES_ASSERT(IMG_NULL != psBufObj);
			GLES_ASSERT(buffer == psBufObj->sNamedItem.ui32Name);

			if(!InsertNamedItem(psNamesArray, (GLES2NamedItem*)psBufObj))
			{
				FreeBufferObject(gc, psBufObj, IMG_FALSE);

				SetError(gc, GL_OUT_OF_MEMORY);

				GLES2_TIME_STOP(GLES2_TIMES_glBindBuffer);

				return;
			}
			
			/* Add RefCount by 1 when creating the bufobj */
			NamedItemAddRef(psNamesArray, buffer);
		}
		else
		{
			/*
			** Retrieved an existing object.  Do some
			** sanity checks.
			*/
			GLES_ASSERT(buffer == psBufObj->sNamedItem.ui32Name);
		}
	}

	
	/* The following code deals with RefCount differently for ARRAY_BUFFER & ELEMENT_ARRAY_BUFFER */
	switch(ui32TargetIndex)
	{
	    case ARRAY_BUFFER_INDEX:
		{
			/* Decrease RefCount by 1 when unbinding the previously bufobj from the context. */
			psBoundBuffer = gc->sBufferObject.psActiveBuffer[ui32TargetIndex];

			if (psBoundBuffer && (psBoundBuffer->sNamedItem.ui32Name != 0))
			{
				NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psBoundBuffer);
			}
			break;
		}
	    case ELEMENT_ARRAY_BUFFER_INDEX:
		{
		    /* Decrease RefCount by 1 when unbinding the previously bufobj from the VAO */
			psBoundBuffer = psVAO->psBoundElementBuffer;
			
			if (psBoundBuffer && (psBoundBuffer->sNamedItem.ui32Name != 0))
			{
				NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psBoundBuffer);
			}
		}
	}

	/*
	** Install the new buffer into the correct target in this context.
	*/
	if(buffer)
	{
		gc->sBufferObject.psActiveBuffer[ui32TargetIndex] = psBufObj;

		/* Assign eIndex value */
		switch(target)
		{
		    case GL_ARRAY_BUFFER:
			{
				psBufObj->eIndex = ARRAY_BUFFER_INDEX;	/* PRQA S 0505 */ /* psBufObj here is always valid. */

				break;
			}
		    case GL_ELEMENT_ARRAY_BUFFER:
			{
				psBufObj->eIndex = ELEMENT_ARRAY_BUFFER_INDEX;

				break;
			}
		}
	}
	else
	{
		gc->sBufferObject.psActiveBuffer[ui32TargetIndex] = IMG_NULL;
	}


	/* Setup VAO's element buffer */
	if (ui32TargetIndex == ELEMENT_ARRAY_BUFFER_INDEX)
	{
		if (psVAO->psBoundElementBuffer != gc->sBufferObject.psActiveBuffer[ui32TargetIndex])
		{
			psVAO->psBoundElementBuffer = gc->sBufferObject.psActiveBuffer[ui32TargetIndex];

			psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ELEMENT_BUFFER;
		}
	}

	GLES2_TIME_STOP(GLES2_TIMES_glBindBuffer);
}


/***********************************************************************************
 Function Name      : glDeleteBuffers
 Inputs             : n, buffers
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Deletes a list of n buffers.
					  Deletes internal psBufObj structures for named buffer objects
					  and if they were currently bound, binds 0 (disabled b/o).
************************************************************************************/
GL_APICALL void GL_APIENTRY glDeleteBuffers(GLsizei n, const GLuint *buffers)
{
	GLES2BufferObject *psBufObj;
	GLES2NamesArray *psNamesArray;
	GLES2VertexArrayObject *psVAO;
	IMG_INT32       i, j;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDeleteBuffers"));

	GLES2_TIME_START(GLES2_TIMES_glDeleteBuffers);

	if (n < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glDeleteBuffers);

		return;
	}

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_BUFOBJ];

	for (i=0; i < n; i++)
	{
	    psVAO = gc->sVAOMachine.psActiveVAO;
		GLES_ASSERT(VAO(gc));

		if(buffers[i] != 0)
		{
			/*
			* If the buffer object is currently bound to the VAO's attribute pointer, bind 0 to its target. 
			*/
			for (j=0; j < GLES2_MAX_VERTEX_ATTRIBS; j++)
			{
				psBufObj = psVAO->asVAOState[j].psBufObj;

				/* Is the buffer object currently bound to the VAO? Unbind it */
				if (psBufObj && psBufObj->sNamedItem.ui32Name == buffers[i])
				{
					/* Decrease RefCount by 1 when unbinding it from the VAO */
					NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psBufObj);		
				
					psVAO->asVAOState[j].psBufObj = IMG_NULL;
					
					/* Mark as dirty so we will spot the unbind if someone tries to draw with it */
					psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM;
				}
			}

			/*
			* If the buffer object is currently bound to the VAO's element buffer pointer, bind 0 to its target. 
			*/
			psBufObj = psVAO->psBoundElementBuffer;
			
			if (psBufObj && psBufObj->sNamedItem.ui32Name == buffers[i]) 
			{
				/* Decrease RefCount by 1 when unbinding it from the VAO */
				NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psBufObj);		
				
				psVAO->psBoundElementBuffer = IMG_NULL;
			}
				
			/*
			* If the buffer object is currently bound to the context, bind 0 to its target. 
			* Only when an array buffer object is bound to the context, the RefCount is increased,
			* An element array buffer object can only be bound to the VAO, and the RefCount is increased then.
			*/
			for (j=0; j < GLES2_NUM_BUFOBJ_BINDINGS; j++) 
			{
				psBufObj = gc->sBufferObject.psActiveBuffer[j];

				/* Is the buffer object currently bound to the context? Unbind it */
				if (psBufObj && psBufObj->sNamedItem.ui32Name == buffers[i]) 
				{
					/* Only decrease VBO's refCount by 1, 
					since only VBO can be bound to the context, while IBO can only be bound to VAO */
					if (j == ARRAY_BUFFER_INDEX)
					{
						NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)gc->sBufferObject.psActiveBuffer[j]);
					}
					gc->sBufferObject.psActiveBuffer[j] = IMG_NULL;
				}
			}
		}
	}

	/* Finally, decrement the refcount of all of them */
	NamedItemDelRefByName(gc, psNamesArray, (IMG_UINT32)n, (const IMG_UINT32 *)buffers);

	GLES2_TIME_STOP(GLES2_TIMES_glDeleteBuffers);
}


/***********************************************************************************
 Function Name      : glGenBuffers
 Inputs             : n
 Outputs            : buffers
 Returns            : -
 Description        : ENTRYPOINT: Generates a set of n names for buffer objects
************************************************************************************/
GL_APICALL void GL_APIENTRY glGenBuffers(GLsizei n, GLuint *buffers)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGenBuffers"));

	GLES2_TIME_START(GLES2_TIMES_glGenBuffers);

	if (n < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glGenBuffers);

		return;
	}
	
	NamesArrayGenNames(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_BUFOBJ], (IMG_UINT32)n, (IMG_UINT32 *)buffers);

#if defined(DEBUG) || defined(TIMING)
	gc->ui32VBONamesGenerated += n;
#endif /* defined(DEBUG) || defined(TIMING) */

	GLES2_TIME_STOP(GLES2_TIMES_glGenBuffers);
}


/***********************************************************************************
 Function Name      : glBufferData
 Inputs             : target, size, data, usage
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Saves buffer data and hints to bound buffer object
************************************************************************************/
GL_APICALL void GL_APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
	IMG_UINT32 ui32TargetIndex, uAllocSize, ui32AllocAlign;
	GLES2BufferObject *psBufObj;
	GLES2VertexArrayObject *psVAO;
	PVRSRV_ERROR eError;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBufferData"));

	GLES2_TIME_START(GLES2_TIMES_glBufferData);

	/* Setup VAO */
	psVAO = gc->sVAOMachine.psActiveVAO;
	GLES_ASSERT(VAO(gc));

	/* check */
	switch(target)
	{
		case GL_ARRAY_BUFFER:
		case GL_ELEMENT_ARRAY_BUFFER:
		{
			ui32TargetIndex  = target - GL_ARRAY_BUFFER;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glBufferData);

			return;
		}
	}

	if(size < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glBufferData);

		return;
	}

	switch(usage)
	{
		case GL_STATIC_DRAW:
		case GL_DYNAMIC_DRAW:
		case GL_STREAM_DRAW:
		{
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glBufferData);

			return;
		}
	}

	psBufObj = gc->sBufferObject.psActiveBuffer[ui32TargetIndex];

	if(psBufObj == IMG_NULL)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glBufferData);

		return;
	}

	if(target == GL_ARRAY_BUFFER)
	{
		/* Align vertices to the cache line size + overallocate by a Dword, in case PDS fetches vertex
		 * data at the end of the allocation and pulls in an extra cache line.
		 */
		uAllocSize = ALIGNCOUNT((IMG_UINT32)size + 4, EURASIA_CACHE_LINE_SIZE);
		ui32AllocAlign = EURASIA_CACHE_LINE_SIZE;
	}
	else
	{
		/* Align indices to index fetch burst size */
		uAllocSize = ALIGNCOUNT((IMG_UINT32)size, EURASIA_VDM_INDEX_FETCH_BURST_SIZE);
		ui32AllocAlign = EURASIA_VDM_INDEX_FETCH_BURST_SIZE;
	}

	/* if it already holds some data, free it first (unless it is the same size as the new request) */
	if (psBufObj->psMemInfo)
	{
		if(WaitUntilBufObjNotUsed(gc, psBufObj))
		{
			if((psBufObj->psMemInfo->uAllocSize != uAllocSize) ||
			   (psBufObj->ui32AllocAlign != ui32AllocAlign))
			{
#if defined(DEBUG) || defined(TIMING)
				gc->ui32VBOMemCurrent -= psBufObj->psMemInfo->uAllocSize;
#endif /* defined(DEBUG) || defined(TIMING) */
				
				GLES2FREEDEVICEMEM_HEAP(gc, psBufObj->psMemInfo);

				psBufObj->psMemInfo = IMG_NULL;
			}
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,"glBufferData: Can't update/free buffer as buffer didn't become free"));

			SetError(gc, GL_OUT_OF_MEMORY);

			GLES2_TIME_STOP(GLES2_TIMES_glBufferData);

			return;
		}
	}
	
	/* Don't allocate if we are reusing memory previously allocated */
	if(!psBufObj->psMemInfo)
	{
		if(size)
		{

			eError = GLES2ALLOCDEVICEMEM_HEAP(gc,
				PVRSRV_MEM_READ | PVRSRV_MAP_GC_MMU,		/* Read only (by device) */
				uAllocSize,
				ui32AllocAlign,
				&psBufObj->psMemInfo);

			if (eError != PVRSRV_OK)
			{
				eError = GLES2ALLOCDEVICEMEM_HEAP(gc,
					PVRSRV_MEM_READ,							/* Read only (by device) */
					uAllocSize,
					ui32AllocAlign,
					&psBufObj->psMemInfo);
			}

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"glBufferData: Can't allocate memory for object"));

				psBufObj->psMemInfo = IMG_NULL;

				/* Mark as dirty so we will spot the invalid memory if someone tries to draw with it */
				psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM;
			
				SetError(gc, GL_OUT_OF_MEMORY);

				GLES2_TIME_STOP(GLES2_TIMES_glBufferData);

				return;
			}

			psBufObj->ui32AllocAlign = ui32AllocAlign;
		}

		/* Setup dirty states for the attribute */
 		psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM;

		/* Setup dirty state for VAO's element buffer */
		if (VAO_INDEX_BUFFER_OBJECT(gc))
		{
			if (psVAO->psBoundElementBuffer == psBufObj)
			{
				psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ELEMENT_BUFFER;
			}
		}

#if defined(DEBUG) || defined(TIMING)
		gc->ui32VBOMemCurrent += size;

		if(gc->ui32VBOMemCurrent > gc->ui32VBOHighWaterMark)
		{
			gc->ui32VBOHighWaterMark = gc->ui32VBOMemCurrent;
		}
#endif /* defined(DEBUG) || defined(TIMING) */
	}

	if(data && size)
	{
		GLES2MemCopy(psBufObj->psMemInfo->pvLinAddr, (const IMG_VOID *)data, (IMG_UINT32)size);
	}

	/* store the state */
	psBufObj->ui32BufferSize = (IMG_UINT32)size;
	psBufObj->eUsage = usage;
	psBufObj->bMapped = IMG_FALSE;

#if defined(PDUMP)
	psBufObj->bDumped = IMG_FALSE;
#endif

	GLES2_TIME_STOP(GLES2_TIMES_glBufferData);
}


/***********************************************************************************
 Function Name      : glBufferSubData
 Inputs             : target, offset, size, data
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Updates portion of buffer data to bound buffer object
************************************************************************************/
GL_APICALL void GL_APIENTRY glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void *data)
{
	GLES2BufferObject *psBufObj;
	IMG_UINT32 ui32TargetIndex;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBufferSubData"));

	GLES2_TIME_START(GLES2_TIMES_glBufferSubData);

	/* check */
	switch(target)
	{
		case GL_ARRAY_BUFFER:
		case GL_ELEMENT_ARRAY_BUFFER:
		{
			ui32TargetIndex = target - GL_ARRAY_BUFFER;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glBufferSubData);

			return;
		}
	}

	if(size < 0 || offset < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glBufferSubData);

		return;
	}

	psBufObj = gc->sBufferObject.psActiveBuffer[ui32TargetIndex];
	
	if((psBufObj == IMG_NULL) || psBufObj->bMapped)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glBufferSubData);

		return;
	}

	if((IMG_UINT32)(offset + size) > psBufObj->ui32BufferSize)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glBufferSubData);

		return;
	}

	if(!psBufObj->psMemInfo)
	{
		PVR_DPF((PVR_DBG_ERROR,"glBufferSubData: No memory for object data"));

		SetError(gc, GL_OUT_OF_MEMORY);

		GLES2_TIME_STOP(GLES2_TIMES_glBufferSubData);

		return;
	}

	if (data)
	{
		if(WaitUntilBufObjNotUsed(gc, psBufObj))
		{
			IMG_VOID *pvDst;

			pvDst = (IMG_VOID *)((IMG_UINT8 *)psBufObj->psMemInfo->pvLinAddr + offset);

			GLES2MemCopy(pvDst, (const IMG_VOID *)data, (IMG_UINT32)size);
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR,"glBufferSubData: Can't update data as buffer didn't become free"));

			SetError(gc, GL_OUT_OF_MEMORY);

			GLES2_TIME_STOP(GLES2_TIMES_glBufferSubData);

			return;
		}
	}

#if defined(PDUMP)
	psBufObj->bDumped = IMG_FALSE;
#endif

	GLES2_TIME_STOP(GLES2_TIMES_glBufferSubData);
}


/***********************************************************************************
 Function Name      : glMapBuffer
 Inputs             : target, access
 Outputs            : -
 Returns            : 
 Description        : 
************************************************************************************/
/* PRQA S 3334 1 */ /* access is the required name for this function. */
GL_API_EXT void * GL_APIENTRY glMapBufferOES(GLenum target, GLenum access)
{
	GLES2BufferObject *psBufObj;
	IMG_UINT32 ui32TargetIndex;

	__GLES2_GET_CONTEXT_RETURN(IMG_NULL);

	PVR_DPF((PVR_DBG_CALLTRACE,"glMapBuffer"));

	GLES2_TIME_START(GLES2_TIMES_glMapBuffer);

	/* check */
	switch(target)
	{
		case GL_ARRAY_BUFFER:
		case GL_ELEMENT_ARRAY_BUFFER:
		{
			ui32TargetIndex  = target - GL_ARRAY_BUFFER;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glMapBuffer);

			return IMG_NULL;
		}
	}

	switch(access)
	{
		case GL_WRITE_ONLY_OES:
		{
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glMapBuffer);

			return IMG_NULL;
		}
	}

	psBufObj = gc->sBufferObject.psActiveBuffer[ui32TargetIndex];

	if(!psBufObj || psBufObj->bMapped)
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glMapBuffer);

		return IMG_NULL;
	}

	if(psBufObj->psMemInfo->pvLinAddr)
	{
		if(!WaitUntilBufObjNotUsed(gc, psBufObj))
		{
			PVR_DPF((PVR_DBG_ERROR,"glMapBuffer: Buffer didn't become free"));

			/* See section 2.9 in the desktop GL 2.0 spec */
SetOutOfMemErrorAndReturnNULL:

			SetError(gc, GL_OUT_OF_MEMORY);

			GLES2_TIME_STOP(GLES2_TIMES_glMapBuffer);

			return IMG_NULL;
		}

		psBufObj->eAccess = access;
		psBufObj->bMapped = IMG_TRUE;

		GLES2_TIME_STOP(GLES2_TIMES_glMapBuffer);

		return psBufObj->psMemInfo->pvLinAddr;
	}
	else
	{
		goto SetOutOfMemErrorAndReturnNULL;
	}
}


/***********************************************************************************
 Function Name      : glUnmapBufferOES
 Inputs             : target
 Outputs            : -
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT GLboolean GL_APIENTRY glUnmapBufferOES(GLenum target)
{
	GLES2BufferObject *psBufObj;
	IMG_UINT32 ui32TargetIndex;

	__GLES2_GET_CONTEXT_RETURN(GL_FALSE);

	PVR_DPF((PVR_DBG_CALLTRACE,"glUnmapBuffer"));

	GLES2_TIME_START(GLES2_TIMES_glUnmapBuffer);

	/* check */
	switch(target)
	{
		case GL_ARRAY_BUFFER:
		case GL_ELEMENT_ARRAY_BUFFER:
		{
			ui32TargetIndex  = target - GL_ARRAY_BUFFER;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			return GL_FALSE;
		}
	}

	psBufObj = gc->sBufferObject.psActiveBuffer[ui32TargetIndex];
	
	if(!psBufObj || !psBufObj->bMapped || !psBufObj->psMemInfo->pvLinAddr) 
	{
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glUnmapBuffer);

		return GL_FALSE;
	}

	psBufObj->bMapped = IMG_FALSE;

#if defined(PDUMP)
	psBufObj->bDumped = IMG_FALSE;
#endif

	GLES2_TIME_STOP(GLES2_TIMES_glUnmapBuffer);

	return GL_TRUE;
}

/******************************************************************************
 End of file (bufobj.c)
******************************************************************************/

