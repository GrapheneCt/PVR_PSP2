/******************************************************************************
 * Name         : scissor.c
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
 * $Log: scissor.c $
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "context.h"

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

#if defined(FIX_HW_BRN_31988)
#define GLES1_SCISSOR_VERTEX_FIRSTPHAS_SIZE_IN_BYTES (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 9), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))
#else
#define GLES1_SCISSOR_VERTEX_FIRSTPHAS_SIZE_IN_BYTES (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 5), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))
#endif

#if defined(SGX_FEATURE_VCB)
#define GLES1_SCISSOR_VERTEX_SIZE_IN_BYTES	(GLES1_SCISSOR_VERTEX_FIRSTPHAS_SIZE_IN_BYTES + (EURASIA_USE_INSTRUCTION_SIZE * 2))
#else
#define GLES1_SCISSOR_VERTEX_SIZE_IN_BYTES	(GLES1_SCISSOR_VERTEX_FIRSTPHAS_SIZE_IN_BYTES)
#endif /* defined(SGX_FEATURE_VCB) */

#else
#define GLES1_SCISSOR_VERTEX_SIZE_IN_BYTES	(EURASIA_USE_INSTRUCTION_SIZE * 4)
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */


/***********************************************************************************
 Function Name      : InitScissorUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : Success
 Description        : Initialises all USSE code blocks required for scissors
************************************************************************************/
IMG_INTERNAL IMG_BOOL InitScissorUSECodeBlocks(GLES1Context *gc)
{	
	IMG_UINT32 *pui32BufferBase, *pui32Buffer;	/* PRQA S 3203 */ /* pui32Buffer is used in the following GLES1_ASSERT. */

	GLES1_TIME_START(GLES1_TIMER_USECODEHEAP_VERT_TIME);

	gc->sPrim.psScissorVertexCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEVertexCodeHeap,
															  GLES1_SCISSOR_VERTEX_SIZE_IN_BYTES, gc->psSysContext->hPerProcRef);

	GLES1_TIME_STOP(GLES1_TIMER_USECODEHEAP_VERT_TIME);

	if(!gc->sPrim.psScissorVertexCodeBlock)
	{
		PVR_DPF((PVR_DBG_FATAL,"Couldn't allocate USE vertex code for Scissor Object"));
		return IMG_FALSE;
	}

	GLES1_INC_COUNT(GLES1_TIMER_USECODEHEAP_VERT_COUNT, GLES1_SCISSOR_VERTEX_SIZE_IN_BYTES >> 2);

	pui32BufferBase = gc->sPrim.psScissorVertexCodeBlock->pui32LinAddress;

	/* PRQA S 3199 1 */ /* pui32Buffer is used in the following GLES1_ASSERT. */
	pui32Buffer =  USEGenWriteSpecialObjVtxProgram (pui32BufferBase, 
													USEGEN_SCISSOR,
													gc->sPrim.psScissorVertexCodeBlock->sCodeAddress,
													gc->psSysContext->uUSEVertexHeapBase, 
													SGX_VTXSHADER_USE_CODE_BASE_INDEX);

	GLES1_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= GLES1_SCISSOR_VERTEX_SIZE_IN_BYTES >> 2);

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : FreeScissorUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : -
 Description        : Frees all USSE code blocks required for scissors
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeScissorUSECodeBlocks(GLES1Context *gc)
{
	UCH_CodeHeapFree(gc->sPrim.psScissorVertexCodeBlock);
}

/***********************************************************************************
 Function Name      : CalcRegionClip
 Inputs             : gc, psRect
 Outputs            : -
 Returns            : Region Clip Word
 Description        : Calculates a region clip word for a given rect. 
************************************************************************************/
IMG_INTERNAL IMG_VOID CalcRegionClip(GLES1Context *gc, EGLRect *psRegion, IMG_UINT32 *pui32RegionClip)
{
	IMG_UINT32 ui32Right, ui32Bottom, ui32Left, ui32Top;
	EGLRect sRect;
	EGLRect *psRect;
	EGLRenderSurface *psRenderSurface = gc->psRenderSurface;

	if(psRegion)
	{
		psRect = psRegion;
	}
	else
	{
		sRect.i32X = 0;
		sRect.i32Y = 0;
		sRect.ui32Width = gc->psDrawParams->ui32Width;
		sRect.ui32Height = gc->psDrawParams->ui32Height;

		psRect = &sRect;
	}

	/*
		Snap TA clip rect to nearest 16 pixel units outside viewport rectangle.
	*/
	ui32Right = ((IMG_UINT32)psRect->i32X + psRect->ui32Width + EURASIA_TARGNCLIP_RIGHT_ALIGNSIZE - 1) & ~(EURASIA_TARGNCLIP_RIGHT_ALIGNSIZE - 1);
	ui32Bottom = ((IMG_UINT32)psRect->i32Y + psRect->ui32Height + EURASIA_TARGNCLIP_BOTTOM_ALIGNSIZE - 1) & ~(EURASIA_TARGNCLIP_BOTTOM_ALIGNSIZE - 1);


	/*
		Work out positions in 16 pixel units
	*/
	ui32Left   = ((IMG_UINT32)psRect->i32X >> EURASIA_TARGNCLIP_LEFT_ALIGNSHIFT);
	ui32Right  = (ui32Right >> EURASIA_TARGNCLIP_RIGHT_ALIGNSHIFT) - 1;
	ui32Top    = ((IMG_UINT32)psRect->i32Y >> EURASIA_TARGNCLIP_TOP_ALIGNSHIFT);
	ui32Bottom = (ui32Bottom >> EURASIA_TARGNCLIP_BOTTOM_ALIGNSHIFT) - 1;


	/*
		Setup region-clip word to clip everything outside the calculated 
		tile-region
	*/
	psRenderSurface->aui32RegionClipWord[0] = EURASIA_TARGNCLIP_MODE_OUTSIDE;

	psRenderSurface->aui32RegionClipWord[0] |= (ui32Left << EURASIA_TARGNCLIP_LEFT_SHIFT) & (~EURASIA_TARGNCLIP_LEFT_CLRMSK);

	psRenderSurface->aui32RegionClipWord[0] |= (ui32Right << EURASIA_TARGNCLIP_RIGHT_SHIFT) & (~EURASIA_TARGNCLIP_RIGHT_CLRMSK);

	psRenderSurface->aui32RegionClipWord[1] = (ui32Top << EURASIA_TARGNCLIP_TOP_SHIFT) & (~EURASIA_TARGNCLIP_TOP_CLRMSK);

	psRenderSurface->aui32RegionClipWord[1] |= (ui32Bottom << EURASIA_TARGNCLIP_BOTTOM_SHIFT) & (~EURASIA_TARGNCLIP_BOTTOM_CLRMSK);

	pui32RegionClip[0] = psRenderSurface->aui32RegionClipWord[0];
	pui32RegionClip[1] = psRenderSurface->aui32RegionClipWord[1];
}


/***********************************************************************************
 Function Name      : SetupVerticesForDrawmask
 Inputs             : gc 
 Outputs            : puVertexAddr, puIndexAddr, pui32NumIndices
 Returns            : Mem Error
 Description        : Setups vertex and index buffers for drawmask
************************************************************************************/
static GLES1_MEMERROR SetupVerticesForDrawmask(GLES1Context *gc,
										   EGLRect *psRect,
										   IMG_DEV_VIRTADDR *puVertexAddr,
										   IMG_DEV_VIRTADDR *puIndexAddr,
										   IMG_UINT32 *pui32NumIndices)
{
	IMG_FLOAT *pfVertices;
	IMG_FLOAT afClearSize[4];
	IMG_UINT16 *pui16Indices;
	IMG_UINT32 *pui32Indices;
	IMG_UINT32 ui32NumIndices = 3, ui32VertexDWords = 6, ui32IndexDWords = 2;

	if(psRect)
	{
		afClearSize[0] = (IMG_FLOAT)psRect->i32X;
		afClearSize[1] = (IMG_FLOAT)psRect->i32Y;
		afClearSize[2] = (IMG_FLOAT)((IMG_UINT32)psRect->i32X + psRect->ui32Width);
		afClearSize[3] = (IMG_FLOAT)((IMG_UINT32)psRect->i32Y + psRect->ui32Height);
	}
	else
	{
		afClearSize[0] = 0;
		afClearSize[1] = 0;
		afClearSize[2] = (IMG_FLOAT)gc->psDrawParams->ui32Width;
		afClearSize[3] = (IMG_FLOAT)gc->psDrawParams->ui32Height;
	}


	if(psRect || (afClearSize[2] >= (IMG_FLOAT)((EURASIA_PARAM_VF_X_MAXIMUM / 2) - 1)) ||
				 (afClearSize[3] >= (IMG_FLOAT)((EURASIA_PARAM_VF_Y_MAXIMUM / 2) - 1)))
	{
		ui32VertexDWords +=2;
		ui32NumIndices++;
	}

	pfVertices = (IMG_FLOAT *) CBUF_GetBufferSpace(gc->apsBuffers, ui32VertexDWords,
												CBUF_TYPE_VERTEX_DATA_BUFFER, IMG_FALSE);

	if(!pfVertices)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	pui32Indices = CBUF_GetBufferSpace(gc->apsBuffers, ui32IndexDWords, CBUF_TYPE_INDEX_DATA_BUFFER, IMG_FALSE);

	if(!pui32Indices)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	/* Get the device address of the buffer */
	*puVertexAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, (IMG_UINT32 *)pfVertices, CBUF_TYPE_VERTEX_DATA_BUFFER);
	*puIndexAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32Indices, CBUF_TYPE_INDEX_DATA_BUFFER);

	pui16Indices = (IMG_UINT16 *)pui32Indices;

	pui16Indices[0]	= 0;
	pui16Indices[1]	= 1;
	pui16Indices[2]	= 2;

	if(ui32NumIndices == 3)
	{
		/* Use double sized triangle for performance */
		afClearSize[2] *= 2;
		afClearSize[3] *= 2;

		pfVertices[0]	= 0;
		pfVertices[1]	= 0;
		
		pfVertices[2]	= afClearSize[2];
		pfVertices[3]	= 0;
		
		pfVertices[4]	= 0;
		pfVertices[5]	= afClearSize[3];
	}
	else
	{
		pfVertices[0]	= afClearSize[0];
		pfVertices[1]	= afClearSize[1];

		pfVertices[2]	= afClearSize[2];
		pfVertices[3]	= afClearSize[1];

		pfVertices[4]	= afClearSize[0];
		pfVertices[5]	= afClearSize[3];

		pfVertices[6]	= afClearSize[2];
		pfVertices[7]	= afClearSize[3];

		pui16Indices[3]	= 3;
	}
		
	CBUF_UpdateBufferPos(gc->apsBuffers, ui32VertexDWords, CBUF_TYPE_VERTEX_DATA_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_VERTEX_DATA_COUNT, ui32VertexDWords);

	CBUF_UpdateBufferPos(gc->apsBuffers, ui32IndexDWords, CBUF_TYPE_INDEX_DATA_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_INDEX_DATA_COUNT, ui32VertexDWords);

	*pui32NumIndices = ui32NumIndices;

	return GLES1_NO_ERROR;
}


static GLES1_MEMERROR SetupVerticesAndShaderForDrawmask(GLES1Context *gc, EGLRect *psRect)
{
	IMG_UINT32 *pui32Buffer, *pui32BufferBase;
	PDS_VERTEX_SHADER_PROGRAM sPDSVertexShaderProgram;
	IMG_DEV_VIRTADDR uPDSCodeAddr, uVerticesAddr, uIndexAddr;
	IMG_UINT32 ui32NumIndices, ui32NumUSEAttribsInBytes;
	GLES1_MEMERROR eError;

	/* 2 PA's used for vertices */
	ui32NumUSEAttribsInBytes = 8;

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	ui32NumUSEAttribsInBytes += (SGX_USE_MINTEMPREGS << 2);
#endif

	eError = SetupVerticesForDrawmask(gc, psRect, &uVerticesAddr, &uIndexAddr, &ui32NumIndices);

	if(eError != GLES1_NO_ERROR)
	{
		return eError;
	}

	/* Setup the input program structure */
	sPDSVertexShaderProgram.pui32DataSegment = IMG_NULL;
	sPDSVertexShaderProgram.ui32DataSize = 0;
	sPDSVertexShaderProgram.b32BitIndices = IMG_FALSE;
	sPDSVertexShaderProgram.ui32NumInstances = 0;
	sPDSVertexShaderProgram.ui32NumStreams = 1;
	sPDSVertexShaderProgram.bIterateVtxID = IMG_FALSE;
	sPDSVertexShaderProgram.bIterateInstanceID = IMG_FALSE;

	sPDSVertexShaderProgram.asStreams[0].ui32NumElements = 1;
	sPDSVertexShaderProgram.asStreams[0].bInstanceData = IMG_FALSE;
	sPDSVertexShaderProgram.asStreams[0].ui32Multiplier = 0;
	sPDSVertexShaderProgram.asStreams[0].ui32Address = uVerticesAddr.uiAddr;
	sPDSVertexShaderProgram.asStreams[0].ui32Shift = 0;
	sPDSVertexShaderProgram.asStreams[0].ui32Stride = 8;

	sPDSVertexShaderProgram.asStreams[0].asElements[0].ui32Offset = 0;
	sPDSVertexShaderProgram.asStreams[0].asElements[0].ui32Size = 8;
	sPDSVertexShaderProgram.asStreams[0].asElements[0].ui32Register = 0;

	/* Set the USE Task control words */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sPDSVertexShaderProgram.aui32USETaskControl[0]	= (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU0_TRC_SHIFT);
	sPDSVertexShaderProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	sPDSVertexShaderProgram.aui32USETaskControl[2]	= 0;
#else 
	sPDSVertexShaderProgram.aui32USETaskControl[0]	= 0;
	sPDSVertexShaderProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL	| 	
													  (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU1_TRC_SHIFT);
	sPDSVertexShaderProgram.aui32USETaskControl[2]	= 0;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	/* Set the execution address of the vertex copy program */
	SetUSEExecutionAddress(&sPDSVertexShaderProgram.aui32USETaskControl[0], 
							0, 
							gc->sPrim.psScissorVertexCodeBlock->sCodeAddress, 
							gc->psSysContext->uUSEVertexHeapBase, 
							SGX_VTXSHADER_USE_CODE_BASE_INDEX);

	/*
		Get buffer space for the PDS vertex program
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, EURASIA_NUM_PDS_VERTEX_PROGRAM_DWORDS,
										CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);
	
	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	/* Generate the PDS Program for this shader */
	pui32Buffer = PDSGenerateVertexShaderProgram(&sPDSVertexShaderProgram, pui32BufferBase, IMG_NULL);

	/* Update buffer position */
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_VERT_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, (pui32Buffer - pui32BufferBase));

	/*
	  Save the PDS vertex shader data segment physical address and size
	*/
	uPDSCodeAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers,
											sPDSVertexShaderProgram.pui32DataSegment,
											CBUF_TYPE_PDS_VERT_BUFFER);

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
	/* Adjust PDS execution address for restricted address range */
	uPDSCodeAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

	/*
		Get buffer space for the VDM control stream
	*/

	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, MAX_DWORDS_PER_INDEX_LIST_BLOCK, CBUF_TYPE_VDM_CTRL_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}
	
	pui32Buffer = pui32BufferBase;

	/* Index List Block header */
	pui32Buffer[0] =	EURASIA_TAOBJTYPE_INDEXLIST	|
						EURASIA_VDM_IDXPRES2		|
						EURASIA_VDM_IDXPRES3		|
#if !defined(SGX545)
						EURASIA_VDM_IDXPRES45		|
#endif /* !defined(SGX545) */
						ui32NumIndices				|
						((ui32NumIndices == 3) ? EURASIA_VDM_TRIS : EURASIA_VDM_STRIP);

	/* Index List 1 */
	pui32Buffer[1]	= (uIndexAddr.uiAddr >> EURASIA_VDM_IDXBASE16_ALIGNSHIFT) << EURASIA_VDM_IDXBASE16_SHIFT;

	/* Index List 2 */
	pui32Buffer[2]	= ((IMG_UINT32)((EURASIA_VDMPDS_MAXINPUTINSTANCES_MAX - 1) << EURASIA_VDMPDS_MAXINPUTINSTANCES_SHIFT)) |
					  EURASIA_VDM_IDXSIZE_16BIT;

	/* Index List 3 */
	pui32Buffer[3] = ~EURASIA_VDM_WRAPCOUNT_CLRMSK;


	/* Index List 4 */	
	pui32Buffer[4] =	(7 << EURASIA_VDMPDS_PARTSIZE_SHIFT) |
						(uPDSCodeAddr.uiAddr >> EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) << 
						EURASIA_VDMPDS_BASEADDR_SHIFT;

	/* Index List 5 */
	pui32Buffer[5] =	(1 << EURASIA_VDMPDS_VERTEXSIZE_SHIFT)				|
						(3 << EURASIA_VDMPDS_VTXADVANCE_SHIFT)				|
						(ALIGNCOUNTINBLOCKS(ui32NumUSEAttribsInBytes, EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFT) <<
						EURASIA_VDMPDS_USEATTRIBUTESIZE_SHIFT)				|
						(sPDSVertexShaderProgram.ui32DataSize >> EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT)
						<< EURASIA_VDMPDS_DATASIZE_SHIFT;

	CBUF_UpdateBufferPos(gc->apsBuffers, MAX_DWORDS_PER_INDEX_LIST_BLOCK, CBUF_TYPE_VDM_CTRL_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_VDM_CTRL_STATE_COUNT, MAX_DWORDS_PER_INDEX_LIST_BLOCK);

	/*
		Update TA buffers committed primitive offset
	*/
	CBUF_UpdateBufferCommittedPrimOffsets(gc->apsBuffers, &gc->psRenderSurface->bPrimitivesSinceLastTA, (IMG_VOID *)gc, KickLimit_ScheduleTA);
	CBUF_UpdateVIBufferCommittedPrimOffsets(gc->apsBuffers, &gc->psRenderSurface->bPrimitivesSinceLastTA, (IMG_VOID *)gc, KickLimit_ScheduleTA);

	return GLES1_NO_ERROR;
}


/***********************************************************************************
 Function Name      : SendDrawMaskRect
 Inputs             : gc, psRect, bIsEnable
 Outputs            : 
 Returns            : -
 Description        : Sends a special object to enable/disable portions of the 
					  screen for drawing
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR SendDrawMaskRect(GLES1Context *gc, EGLRect *psRect, IMG_BOOL bIsEnable)
{
	/*
	 * Possible optimisation: It makes sense to use the TA region clip word if a non-fullscreen drawmask is set. This
	 * ensures geometry stretching outside the viewport/scissor region doesn't generate extra pointers  It is also 
	 * important to detect a drawmask equivalent to the previous one (especially full-screen drawmasks). The previous
	 * drawmask state will have to be stored for this.
	 */
	EGLRect *psLastDrawMask = &gc->psRenderSurface->sLastDrawMask;
	EGLDrawableParams *psDrawParams = gc->psDrawParams;
	IMG_UINT32 ui32ISPControlWordA = 0; 
	IMG_DEV_VIRTADDR uStateAddr;
	IMG_UINT32 *pui32Buffer, *pui32BufferBase;
	IMG_UINT32 ui32NumStateDWords, ui32BlockHeader = 0;
	IMG_UINT32 aui32RegionClip[2];
	GLES1_MEMERROR eError;

	/* If we are doing a disable either because we are asked to, or for a non-fullscreen enable,
		we need to send ISPC to set the viewport disable bit - it defaults on otherwise
	*/
	if(psRect)
	{
		/* Non-fullscreen drawmasks can only be requested as enables */
		GLES1_ASSERT(bIsEnable);

		/* Reset fullscreen enable flag */		
		gc->psRenderSurface->bLastDrawMaskFullScreenEnable = IMG_FALSE;
		
		if(psRect->i32X == psLastDrawMask->i32X &&
			psRect->i32Y == psLastDrawMask->i32Y &&
			psRect->ui32Width == psLastDrawMask->ui32Width &&
			psRect->ui32Height == psLastDrawMask->ui32Height)
		{
			return GLES1_NO_ERROR;
		}

		ui32ISPControlWordA = EURASIA_ISPA_CPRES;
		ui32NumStateDWords = 9;
		ui32BlockHeader = EURASIA_TACTLPRES_ISPCTLFF2;
	}
	else
	{
		if(bIsEnable)
		{
			/* Already got a fullscreen enable - no need for another */
			if(gc->psRenderSurface->bLastDrawMaskFullScreenEnable)
				return GLES1_NO_ERROR;

			ui32NumStateDWords = 8;
		}
		else
		{
			ui32ISPControlWordA = EURASIA_ISPA_CPRES;
			ui32NumStateDWords = 9;
			ui32BlockHeader = EURASIA_TACTLPRES_ISPCTLFF2;
		}

		gc->psRenderSurface->bLastDrawMaskFullScreenEnable = bIsEnable;

		psLastDrawMask->i32X = 0;
		psLastDrawMask->i32Y = 0;
		psLastDrawMask->ui32Width = psDrawParams->ui32Width;
		psLastDrawMask->ui32Height = psDrawParams->ui32Height;

	}

	 /* Use previous enable drawmask for first disable, if doing a non-fullscreen enable. 
	    Otherwises first object is a fullscreen one 
	  */
	if(psRect)
	{
		CalcRegionClip(gc, psLastDrawMask, aui32RegionClip);

		*psLastDrawMask = *psRect;
	}
	else
	{
		CalcRegionClip(gc, IMG_NULL, aui32RegionClip);
	}

	/*
		Get buffer space for the actual output state
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers,
									ui32NumStateDWords,
									CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	pui32Buffer = pui32BufferBase;

	/* Always send ispA, output selects (including texsize word), region clip
	   and MTE control - rest can take default/previous state 
	*/
	*pui32Buffer++ =	EURASIA_TACTLPRES_ISPCTLFF0		|
						EURASIA_TACTLPRES_RGNCLIP		|
						EURASIA_TACTLPRES_OUTSELECTS	|
						EURASIA_TACTLPRES_MTECTRL		|
						EURASIA_TACTLPRES_TEXSIZE		|
						EURASIA_TACTLPRES_TEXFLOAT		|
						ui32BlockHeader;

	*pui32Buffer++ = 	EURASIA_ISPA_PASSTYPE_VIEWPORT	|
						EURASIA_ISPA_DCMPMODE_ALWAYS	|
						EURASIA_ISPA_TAGWRITEDIS		|
						EURASIA_ISPA_OBJTYPE_TRI		|
						ui32ISPControlWordA;

	/* This ensures the viewport object maintains the stencil value */
	if(ui32BlockHeader & EURASIA_TACTLPRES_ISPCTLFF2)
	{
		*pui32Buffer++ = EURASIA_ISPC_SOP3_KEEP | EURASIA_ISPC_SCMP_ISPMASKCLEAR;
	}

	/* ========= */
	/* Region Clip */
	/* ========= */
	*pui32Buffer++ =	aui32RegionClip[0];
	*pui32Buffer++ =	aui32RegionClip[1];

	/* ========= */
	/* Output Selects */
	/* ========= */
	*pui32Buffer++ =	(4 << EURASIA_MTE_VTXSIZE_SHIFT);

	/* ========= */
	/*	MTE control */
	/* ========= */
	*pui32Buffer++	=
#if !defined(SGX545)
	                    EURASIA_MTE_SHADE_GOURAUD	|
#endif /* !defined(SGX545) */
						EURASIA_MTE_PRETRANSFORM	|
						EURASIA_MTE_CULLMODE_NONE;

	/* ========= */
	/*	Texture coordinate size */
	/* ========= */
	*pui32Buffer++ = 0;

	/* ========= */
	/*	Texture float info */
	/* ========= */
	*pui32Buffer++ = 0;
	
	GLES1_ASSERT(ui32NumStateDWords == (IMG_UINT32)(pui32Buffer - pui32BufferBase));

	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_VERT_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, (pui32Buffer - pui32BufferBase));

	/*
		Get the device address of the state data
	*/
	uStateAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_PDS_VERT_BUFFER);

	if(gc->sAppHints.bEnableStaticMTECopy)
	{
		eError = PatchPregenMTEStateCopyPrograms(gc, ui32NumStateDWords, uStateAddr);
	}
	else
	{
		eError = WriteMTEStateCopyPrograms(gc, ui32NumStateDWords, uStateAddr);
	}

	if(eError != GLES1_NO_ERROR)
	{
		return eError;
	}

	gc->ui32EmitMask |=	GLES1_EMITSTATE_MTE_STATE_ISP |
						GLES1_EMITSTATE_MTE_STATE_CONTROL |
						GLES1_EMITSTATE_MTE_STATE_REGION_CLIP |
						GLES1_EMITSTATE_MTE_STATE_OUTPUTSELECTS;
	
	eError = SetupStateUpdate(gc, IMG_TRUE);

	if(eError != GLES1_NO_ERROR)
	{
		return eError;
	}

	/* Full screen object (either enable/disable as requested, 
	   or disable before later non-fullscreen enable 
	 */
	eError = SetupVerticesAndShaderForDrawmask(gc, IMG_NULL);
	
	if(eError != GLES1_NO_ERROR)
	{
		return eError;
	}

	if(!(psRect && bIsEnable))
	{
		return GLES1_NO_ERROR;
	}

	/* Now send non-fullscreen enable */
	CalcRegionClip(gc, psRect, aui32RegionClip);

	/*
		Get buffer space for the actual output state
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers,
										  ui32NumStateDWords,
										  CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	pui32Buffer = pui32BufferBase;

	/* Always send ispA, ISPC output selects (including texsize word + texfloat word), region clip
	   and MTE control - rest can take default/previous state 
	*/
	*pui32Buffer++ =	EURASIA_TACTLPRES_ISPCTLFF0		|
						EURASIA_TACTLPRES_ISPCTLFF2		|
						EURASIA_TACTLPRES_RGNCLIP		|
						EURASIA_TACTLPRES_OUTSELECTS	|
						EURASIA_TACTLPRES_MTECTRL		|
						EURASIA_TACTLPRES_TEXSIZE		|
						EURASIA_TACTLPRES_TEXFLOAT;

	/* ISP A */
	*pui32Buffer++ = 	EURASIA_ISPA_PASSTYPE_VIEWPORT	|
						EURASIA_ISPA_DCMPMODE_ALWAYS	|
						EURASIA_ISPA_TAGWRITEDIS		|
						EURASIA_ISPA_OBJTYPE_TRI		|
						EURASIA_ISPA_CPRES;

	/* ISP C */
	*pui32Buffer++ = EURASIA_ISPC_SOP3_KEEP | EURASIA_ISPC_SCMP_ISPMASKSET;

	/* ========= */
	/* Region Clip */
	/* ========= */
	*pui32Buffer++ =	aui32RegionClip[0];
	*pui32Buffer++ =	aui32RegionClip[1];

	/* ========= */
	/* Output Selects */
	/* ========= */
	*pui32Buffer++ =	(4 << EURASIA_MTE_VTXSIZE_SHIFT);

	/* ========= */
	/*	MTE control */
	/* ========= */
	*pui32Buffer++	=	
#if !defined(SGX545)
	                    EURASIA_MTE_SHADE_GOURAUD	|
#endif /* !defined(SGX545) */
						EURASIA_MTE_PRETRANSFORM	|
						EURASIA_MTE_CULLMODE_NONE   ;

	/* ========= */
	/*	Texture coordinate size */
	/* ========= */
	*pui32Buffer++ = 0;

	/* ========= */
	/*	Texture float info */
	/* ========= */
	*pui32Buffer++ = 0;
	
	GLES1_ASSERT(ui32NumStateDWords == (IMG_UINT32)(pui32Buffer - pui32BufferBase));

	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_VERT_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, (pui32Buffer - pui32BufferBase));

	GLES1_INC_COUNT(GLES1_TIMER_MTE_STATE_COUNT, (pui32Buffer - pui32BufferBase));

	/*
		Get the device address of the state data
	*/
	uStateAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_PDS_VERT_BUFFER);

	if(gc->sAppHints.bEnableStaticMTECopy)
	{
		eError = PatchPregenMTEStateCopyPrograms(gc, ui32NumStateDWords, uStateAddr);
	}
	else
	{
		eError = WriteMTEStateCopyPrograms(gc, ui32NumStateDWords, uStateAddr);
	}
	
	if(eError != GLES1_NO_ERROR)
	{
		return eError;
	}

	eError = SetupStateUpdate(gc, IMG_TRUE);

	if(eError != GLES1_NO_ERROR)
	{
		return eError;
	}

	/* Now send non-fullscreen enable */
	return SetupVerticesAndShaderForDrawmask(gc, psRect);
}


/***********************************************************************************
 Function Name      : SendDrawMaskForPrimitive
 Inputs             : gc
 Outputs            : -
 Returns            : Memory Error
 Description        : Calculates the currently required drawmasks, combining scissor,
					  viewport and drawablePrivate information. 
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR SendDrawMaskForPrimitive(GLES1Context *gc)
{
	GLES1_MEMERROR eError = GLES1_NO_ERROR;

	/*
	 * Only do something if something has changed
	 */
	if (gc->bDrawMaskInvalid && gc->psRenderSurface->bInFrame)
	{
		EGLRect sScissor;
		EGLDrawableParams *psDrawParams = gc->psDrawParams;
		IMG_INT32 i32X1, i32Y1, i32X0, i32Y0;

		gc->bDrawMaskInvalid = IMG_FALSE;

		if (((gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE) != 0) && !gc->bFullScreenScissor)
		{
			if(gc->sState.sScissor.ui32ScissorWidth == 0 || gc->sState.sScissor.ui32ScissorHeight == 0)
			{
				/*
				 * Full screen should be scissored here 
				 * Do full screen disable. 
				 */
				eError = SendDrawMaskRect(gc, IMG_NULL, IMG_FALSE);

				return eError;
			}

			if(psDrawParams->eRotationAngle == PVRSRV_FLIP_Y)
			{
				i32X0 = gc->sState.sScissor.i32ScissorX;
				i32Y0 = gc->sState.sScissor.i32ScissorY;
			}
			else
			{
				i32X0 = gc->sState.sScissor.i32ScissorX;
				i32Y0 = (IMG_INT32)psDrawParams->ui32Height - (gc->sState.sScissor.i32ScissorY + gc->sState.sScissor.i32ClampedHeight);
			}

			i32X1 = i32X0 + gc->sState.sScissor.i32ClampedWidth;
			i32Y1 = i32Y0 + gc->sState.sScissor.i32ClampedHeight;

			if (!gc->bFullScreenViewport)
			{
				GLint nXV0, nYV0, nXV1, nYV1;

				/* Work out a combination of the viewport and scissor */
				if(psDrawParams->eRotationAngle == PVRSRV_FLIP_Y)
				{
					nXV0 = gc->sState.sViewport.i32X;
					nYV0 = gc->sState.sViewport.i32Y;
				}
				else
				{
					nXV0 = gc->sState.sViewport.i32X;
					nYV0 = (IMG_INT32)psDrawParams->ui32Height - (gc->sState.sViewport.i32Y + (IMG_INT32)gc->sState.sViewport.ui32Height);
				}

				nXV1 = nXV0 + (IMG_INT32)gc->sState.sViewport.ui32Width;
				nYV1 = nYV0 + (IMG_INT32)gc->sState.sViewport.ui32Height;

				if ((i32X1 < nXV0) || (i32Y1 < nYV0) || (i32X0 > nXV1) || (i32Y0 > nYV1))
				{
					/*
					 * Viewport and scissor don't overlap so scissor fullscreen
					 * Do full screen disable. 
					 */
					eError = SendDrawMaskRect(gc, IMG_NULL, IMG_FALSE);

					return eError;
				}
				else
				{
					/*
					 * Determine overlapping rectangle
					 */
					i32X0 = MAX(i32X0, nXV0);
					i32X1 = MIN(i32X1, nXV1);
					i32Y0 = MAX(i32Y0, nYV0);
					i32Y1 = MIN(i32Y1, nYV1);
				}
			}
		}
		else if (!gc->bFullScreenViewport)
		{
			/*
			 * If there's no scissor and the viewport isn't full screen,
			 * set clip to the current viewport
			 */
			if(psDrawParams->eRotationAngle == PVRSRV_FLIP_Y)
			{
				i32X0 = gc->sState.sViewport.i32X;
				i32Y0 = gc->sState.sViewport.i32Y;
			}
			else
			{
				i32X0 = gc->sState.sViewport.i32X;
				i32Y0 = (IMG_INT32)psDrawParams->ui32Height - (gc->sState.sViewport.i32Y + (IMG_INT32)gc->sState.sViewport.ui32Height);
			}

			i32X1 = i32X0 + (IMG_INT32)gc->sState.sViewport.ui32Width;
			i32Y1 = i32Y0 + (IMG_INT32)gc->sState.sViewport.ui32Height;
		}
		else
		{
			/*
			 * No scissor, fullscreen viewport and not single buffered window
			 *  = Fullscreen enable
			 */
			eError = SendDrawMaskRect(gc, IMG_NULL, IMG_TRUE);

			return eError;
		}

		/*
		 *	Make sure the scissor/viewport hasn't any negative coordinates
		 */
		if (i32X0 < 0)
		{
			if (i32X1 < 0)
			{
				/*
				 * Viewport/scissor is off screen
				 * Do full screen disable. 
				 */
				eError = SendDrawMaskRect(gc, IMG_NULL, IMG_FALSE);

				return eError;
			}
			else
			{
				i32X0 = 0;
			}
		}

		if (i32Y0 < 0)
		{
			if (i32Y1 < 0)
			{
				/*
				 * Viewport/scissor is off screen
				 * Do full screen disable. 
				 */
				eError = SendDrawMaskRect(gc, IMG_NULL, IMG_FALSE);

				return eError;
			}
			else
			{
				i32Y0 = 0;
			}
		}

		/* Don't send offscreen region clip / viewport */
		if(i32X1 > (IMG_INT32)gc->psDrawParams->ui32Width)
		{
			i32X1 = (IMG_INT32)gc->psDrawParams->ui32Width;
		}

		if(i32Y1 > (IMG_INT32)gc->psDrawParams->ui32Height)
		{
			i32Y1 = (IMG_INT32)gc->psDrawParams->ui32Height;
		}

		/*
		 * Just send the scissor/viewport
		 */
		sScissor.i32X = i32X0;
		sScissor.i32Y = i32Y0;
		sScissor.ui32Width = (IMG_UINT32)(i32X1 - i32X0);
		sScissor.ui32Height = (IMG_UINT32)(i32Y1 - i32Y0);

		eError = SendDrawMaskRect(gc, &sScissor, IMG_TRUE);
	}
	return eError;
}

/***********************************************************************************
 Function Name      : SendDrawMaskForClear
 Inputs             : gc
 Outputs            : -
 Returns            : Memory error
 Description        : Calculates the currently required drawmasks for a clear, 
					  combining scissor, and drawable information. 
************************************************************************************/

IMG_INTERNAL GLES1_MEMERROR SendDrawMaskForClear(GLES1Context *gc)
{
	EGLDrawableParams *psDrawParams = gc->psDrawParams;
	EGLRect sScissor;
	GLES1_MEMERROR eError;

	if(gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE)
	{
		if(gc->bFullScreenScissor)
		{
			/* Full screen enable */
			eError = SendDrawMaskRect(gc, IMG_NULL, IMG_TRUE);
		}
		else if(gc->sState.sScissor.ui32ScissorWidth == 0 || gc->sState.sScissor.ui32ScissorHeight == 0)
		{
			/* Full screen should be drawmasked off here */
			eError = SendDrawMaskRect(gc, IMG_NULL, IMG_FALSE);
		}
		else
		{
			IMG_INT32 i32X1, i32Y1, i32X0, i32Y0;

			if(psDrawParams->eRotationAngle == PVRSRV_FLIP_Y)
			{
				i32X0 = gc->sState.sScissor.i32ScissorX;
				i32Y0 = gc->sState.sScissor.i32ScissorY;
			}
			else
			{
				i32X0 = gc->sState.sScissor.i32ScissorX;
				i32Y0 = (IMG_INT32)psDrawParams->ui32Height - (gc->sState.sScissor.i32ScissorY + gc->sState.sScissor.i32ClampedHeight);
			}

			i32X1 = i32X0 + gc->sState.sScissor.i32ClampedWidth;
			i32Y1 = i32Y0 + gc->sState.sScissor.i32ClampedHeight;

			if ((i32X1 < 0) || (i32Y1 < 0))
			{
				/* Full screen should be drawmasked off here */
				eError = SendDrawMaskRect(gc, NULL, IMG_FALSE);
			}
			else
			{
				if (i32X0 < 0)
				{
					i32X0 = 0;
				}
				
				if (i32Y0 < 0)
				{
					i32Y0 = 0;
				}

				/* Don't send offscreen region clip / drawmask */
				if(i32X1 > (IMG_INT32)gc->psDrawParams->ui32Width)
				{
					i32X1 = (IMG_INT32)gc->psDrawParams->ui32Width;
				}

				if(i32Y1 > (IMG_INT32)gc->psDrawParams->ui32Height)
				{
					i32Y1 = (IMG_INT32)gc->psDrawParams->ui32Height;
				}

				sScissor.i32X = i32X0;
				sScissor.i32Y = i32Y0;
				sScissor.ui32Width = (IMG_UINT32)(i32X1 - i32X0);
				sScissor.ui32Height = (IMG_UINT32)(i32Y1 - i32Y0);

				eError = SendDrawMaskRect(gc, &sScissor, IMG_TRUE);
			}
		}
	}
	else
	{
		/* Full screen enable */
		eError = SendDrawMaskRect(gc, NULL, IMG_TRUE);
	}

	return eError;
}


/***********************************************************************************
 Function Name      : glScissor
 Inputs             : x, y, width, height
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current scissor state. Implemented with ISP
					  viewport objects. We will send viewport object immediately to
					  simplify the code, although we will detect setting of the same
					  scissor multiple times.
					  Note: clears are affected by scissor as well
************************************************************************************/
GL_API void GL_APIENTRY glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
	IMG_UINT32 ui32Width, ui32Height;

	__GLES1_GET_CONTEXT();
	 
	PVR_DPF((PVR_DBG_CALLTRACE,"glScissor"));

	GLES1_TIME_START(GLES1_TIMES_glScissor);

	if ((width < 0) || (height < 0))
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES1_TIME_STOP(GLES1_TIMES_glScissor);
		return;
	}

	ui32Width =  (IMG_UINT32) width;
	ui32Height = (IMG_UINT32) height;

	/* return if nothing changed */
	if ((x == gc->sState.sScissor.i32ScissorX) &&
		(y == gc->sState.sScissor.i32ScissorY) &&
		(ui32Width == gc->sState.sScissor.ui32ScissorWidth) &&
		(ui32Height == gc->sState.sScissor.ui32ScissorHeight))
	{
		GLES1_TIME_STOP(GLES1_TIMES_glScissor);
		return;
	}

	gc->sState.sScissor.i32ScissorX = x;
	gc->sState.sScissor.i32ScissorY = y;
	gc->sState.sScissor.ui32ScissorWidth = ui32Width;
	gc->sState.sScissor.ui32ScissorHeight = ui32Height;

	gc->bDrawMaskInvalid = IMG_TRUE;

	
	if(((x <= 0) && (y <= 0) &&
		(((IMG_INT32)ui32Width + x) >= (IMG_INT32)gc->psDrawParams->ui32Width ) && 
		(((IMG_INT32)ui32Height + y) >= (IMG_INT32)gc->psDrawParams->ui32Height )))
	{
		gc->bFullScreenScissor = IMG_TRUE;
	}
	else
	{
		gc->bFullScreenScissor = IMG_FALSE;
	}

	/* Use clamped width/height to avoid maths overflow with large signed numbers */
	if(x > 0)
	{
		gc->sState.sScissor.i32ClampedWidth = (IMG_INT32) MIN(gc->sState.sScissor.ui32ScissorWidth, EURASIA_PARAM_VF_X_MAXIMUM);
	}
	else
	{
		gc->sState.sScissor.i32ClampedWidth = (IMG_INT32)gc->sState.sScissor.ui32ScissorWidth;
	}

	if(y > 0)
	{
		gc->sState.sScissor.i32ClampedHeight = (IMG_INT32) MIN(gc->sState.sScissor.ui32ScissorHeight, EURASIA_PARAM_VF_Y_MAXIMUM);
	}
	else
	{
		gc->sState.sScissor.i32ClampedHeight = (IMG_INT32)gc->sState.sScissor.ui32ScissorHeight;
	}

	GLES1_TIME_STOP(GLES1_TIMES_glScissor);
}

/******************************************************************************
 End of file (scissor.c)
******************************************************************************/

