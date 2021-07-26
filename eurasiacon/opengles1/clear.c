/******************************************************************************
 * Name         : clear.c
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
 * $Log: clear.c $
 *****************************************************************************/

#include "context.h"

#include "sgxpdsdefs.h"

#if defined(FIX_HW_BRN_31988)
/* PHAS + NOP + TST + LD + WDF */
#define GLES1_CLEAR_VERTEX_FIRSTPHAS_SIZE_IN_BYTES  (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 12), EURASIA_PDS_DOUTU_PHASE_START_ALIGN)) 
#else
#define GLES1_CLEAR_VERTEX_FIRSTPHAS_SIZE_IN_BYTES  (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 8), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))
#endif

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

#if defined(SGX_FEATURE_VCB)
/* 1st block, EMITVCB, PHAS */
#define GLES1_CLEAR_VERTEX_SIZE_IN_BYTES	(GLES1_CLEAR_VERTEX_FIRSTPHAS_SIZE_IN_BYTES + (EURASIA_USE_INSTRUCTION_SIZE * 2))  
#else
#define GLES1_CLEAR_VERTEX_SIZE_IN_BYTES	(GLES1_CLEAR_VERTEX_FIRSTPHAS_SIZE_IN_BYTES)  
#endif /* defined(SGX_FEATURE_VCB) */
#define GLES1_CLEAR_FRAGMENT_SECTION_SIZE_IN_BYTES	(ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 2), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))

#if defined(FIX_HW_BRN_29838)
#define GLES1_CLEAR_FRAGMENT_SIZE_IN_BYTES	((GLES1_CLEAR_FRAGMENT_SECTION_SIZE_IN_BYTES * 16) + (EURASIA_USE_INSTRUCTION_SIZE * 3))
#else
#define GLES1_CLEAR_FRAGMENT_SIZE_IN_BYTES	((GLES1_CLEAR_FRAGMENT_SECTION_SIZE_IN_BYTES * 16) + (EURASIA_USE_INSTRUCTION_SIZE * 2))
#endif /*FIX_HW_BRN_29838*/

#else 
#define GLES1_CLEAR_VERTEX_SIZE_IN_BYTES	(EURASIA_USE_INSTRUCTION_SIZE * 7)
#define GLES1_CLEAR_FRAGMENT_SECTION_SIZE_IN_BYTES	(ALIGNCOUNT(EURASIA_USE_INSTRUCTION_SIZE, EURASIA_PDS_DOUTU_PHASE_START_ALIGN))

#if defined(FIX_HW_BRN_29838)
#define GLES1_CLEAR_FRAGMENT_SIZE_IN_BYTES	((GLES1_CLEAR_FRAGMENT_SECTION_SIZE_IN_BYTES * 16) + (EURASIA_USE_INSTRUCTION_SIZE * 2))
#else
#define GLES1_CLEAR_FRAGMENT_SIZE_IN_BYTES	((GLES1_CLEAR_FRAGMENT_SECTION_SIZE_IN_BYTES * 16) + (EURASIA_USE_INSTRUCTION_SIZE * 1))
#endif /*FIX_HW_BRN_29838*/

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */




/***********************************************************************************
 Function Name      : InitClearUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : Success
 Description        : Initialises all USSE code blocks required for clears - except
					: delta buffer clear
************************************************************************************/
IMG_INTERNAL IMG_BOOL InitClearUSECodeBlocks(GLES1Context *gc)
{	
  IMG_UINT32 *pui32BufferBase, *pui32Buffer;
	IMG_UINT32 ui32ColorMask;
	IMG_UINT32 ui32Offset;

	gc->sPrim.psClearVertexCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEVertexCodeHeap,
														GLES1_CLEAR_VERTEX_SIZE_IN_BYTES, gc->psSysContext->hPerProcRef);

	if(!gc->sPrim.psClearVertexCodeBlock)
	{
		PVR_DPF((PVR_DBG_FATAL,"Couldn't allocate USE vertex code for Clear Object"));
		return IMG_FALSE;
	}
	
	GLES1_INC_COUNT(GLES1_TIMER_USECODEHEAP_VERT_COUNT, GLES1_CLEAR_VERTEX_SIZE_IN_BYTES >> 2);

	pui32BufferBase = gc->sPrim.psClearVertexCodeBlock->pui32LinAddress;

	/* PRQA S 3198 1 */ /* pui32Buffer is used in the following GLES1_ASSERT. */
	pui32Buffer =  USEGenWriteSpecialObjVtxProgram (pui32BufferBase, 
													USEGEN_CLEAR_PACKEDCOL,
													gc->sPrim.psClearVertexCodeBlock->sCodeAddress,
													gc->psSysContext->uUSEVertexHeapBase, 
													SGX_VTXSHADER_USE_CODE_BASE_INDEX);

	GLES1_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= GLES1_CLEAR_VERTEX_SIZE_IN_BYTES >> 2);


	gc->sPrim.psClearFragmentCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap,
															  GLES1_CLEAR_FRAGMENT_SIZE_IN_BYTES, gc->psSysContext->hPerProcRef);

	if(!gc->sPrim.psClearFragmentCodeBlock)
	{
		PVR_DPF((PVR_DBG_FATAL,"Couldn't allocate USE fragment code for Clear Object"));
		UCH_CodeHeapFree(gc->sPrim.psClearVertexCodeBlock);
		return IMG_FALSE;
	}
	
	GLES1_INC_COUNT(GLES1_TIMER_USECODEHEAP_FRAG_COUNT, GLES1_CLEAR_FRAGMENT_SIZE_IN_BYTES >> 2);

	pui32Buffer = pui32BufferBase = gc->sPrim.psClearFragmentCodeBlock->pui32LinAddress;

	 /* Setup code blocks for color masks upto all on */
	for(ui32ColorMask = 0; ui32ColorMask < GLES1_COLORMASK_ALL; ui32ColorMask++)
	{
		/* Bytes to Dwords conversion */
	    ui32Offset = (GLES1_CLEAR_FRAGMENT_SECTION_SIZE_IN_BYTES * ui32ColorMask) >> 2;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	    /* Insert Single Phas Instruction */
		BuildPHASLastPhase ((PPVR_USE_INST) &pui32Buffer[ui32Offset], 0);
		ui32Offset += USE_INST_LENGTH;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

		/* Some color channels are disabled. 
		Use the sum-of-products with mask instruction, which operates on
		8-bit data. Use MAX operation with duplicate sources:
		SOPWM.colormask o0, MAX(pa0.rgba, pa0.rgba) */

		BuildSOP2WM((PPVR_USE_INST)		&pui32BufferBase[ui32Offset],
										IMG_TRUE,
										EURASIA_USE1_SPRED_ALWAYS,
										USE_REGTYPE_OUTPUT,	0,
										USE_REGTYPE_IMMEDIATE, 0,
										USE_REGTYPE_PRIMATTR, 0,
										USE_REGTYPE_PRIMATTR, 0,
										EURASIA_USE1_SOP2WM_COP_MAX,
										EURASIA_USE1_SOP2WM_AOP_MAX,
										0, 0, 0, 0,
										ui32ColorMask);
	}
	
	ui32Offset = (GLES1_CLEAR_FRAGMENT_SECTION_SIZE_IN_BYTES * GLES1_COLORMASK_ALL) >> 2;

#if defined(FIX_HW_BRN_29838)
	USEGenWriteClearPixelProgramF16(&pui32Buffer[ui32Offset]);
#else
	/* All color channels are enabled, so just do a single mov instruction:	MOV o0, pa0 */
	USEGenWriteClearPixelProgram(&pui32Buffer[ui32Offset], IMG_FALSE);
#endif

	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : FreeClearUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : -
 Description        : Frees all USSE code blocks required for clears
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeClearUSECodeBlocks(GLES1Context *gc)
{
	UCH_CodeHeapFree(gc->sPrim.psClearVertexCodeBlock);
	UCH_CodeHeapFree(gc->sPrim.psClearFragmentCodeBlock);
}


/***********************************************************************************
 Function Name      : GetClearSize
 Inputs             : gc, pfVertices, bForceFulLScreen
 Outputs            : -
 Returns            : -
 Description        : Attempts to generate correct clear primitive size, based on 
					  scissor state, drawable size & window clipping.
************************************************************************************/
static IMG_BOOL GetClearSize(GLES1Context *gc, IMG_FLOAT *pfVertices, IMG_BOOL bForceFulLScreen)
{
	EGLDrawableParams *psDrawParams = gc->psDrawParams;
	IMG_INT32 i32ClearX0, i32ClearY0, i32ClearX1, i32ClearY1;
	IMG_BOOL bIsFullScreen = IMG_FALSE;

	if(((gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE) != 0) && !bForceFulLScreen)
	{
		if(gc->bFullScreenScissor)
		{
			bIsFullScreen = IMG_TRUE;
		}

		if(psDrawParams->eRotationAngle == PVRSRV_FLIP_Y)
		{
			i32ClearX0 = gc->sState.sScissor.i32ScissorX;
			i32ClearY0 = gc->sState.sScissor.i32ScissorY;
		}
		else
		{
			i32ClearX0 = gc->sState.sScissor.i32ScissorX;
			i32ClearY0 = (IMG_INT32)psDrawParams->ui32Height - (gc->sState.sScissor.i32ScissorY + gc->sState.sScissor.i32ClampedHeight);
		}

		i32ClearX1 = i32ClearX0 + gc->sState.sScissor.i32ClampedWidth;
		i32ClearY1 = i32ClearY0 + gc->sState.sScissor.i32ClampedHeight;

		if(i32ClearX1 > (IMG_INT32)psDrawParams->ui32Width)
		{
			i32ClearX1 = (IMG_INT32)psDrawParams->ui32Width;
		}

		if(i32ClearY1 > (IMG_INT32)psDrawParams->ui32Height)
		{
			i32ClearY1 = (IMG_INT32)psDrawParams->ui32Height;
		}
	}
	else
	{
		bIsFullScreen = IMG_TRUE;

		i32ClearX0 = 0;
		i32ClearY0 = 0;

		i32ClearX1 = (IMG_INT32)psDrawParams->ui32Width;
		i32ClearY1 = (IMG_INT32)psDrawParams->ui32Height;
	}

	pfVertices[0] = (IMG_FLOAT)MAX(i32ClearX0, 0);
	pfVertices[1] = (IMG_FLOAT)MAX(i32ClearY0, 0);
	pfVertices[2] = (IMG_FLOAT)i32ClearX1;
	pfVertices[3] = (IMG_FLOAT)i32ClearY1;

	return bIsFullScreen;
}

typedef struct GLES1ClearVertex_TAG
{
	IMG_FLOAT fX;
	IMG_FLOAT fY;
	IMG_FLOAT fZ;
	IMG_UINT32 ui32Color;

}GLES1ClearVertex;

/***********************************************************************************
 Function Name      : SetupVerticesForClear
 Inputs             : gc, bForceFullScreen, fDepth
 Outputs            : puVertexAddr, puIndexAddr, pui32NumIndices
 Returns            : Mem Error
 Description        : Generates vertices and indices for clear
************************************************************************************/
static GLES1_MEMERROR SetupVerticesForClear(GLES1Context *gc, IMG_DEV_VIRTADDR *puVertexAddr, 
											IMG_DEV_VIRTADDR *puIndexAddr, IMG_UINT32 *pui32NumIndices,
											IMG_BOOL bForceFullScreen, IMG_FLOAT fDepth)
{
	GLES1ClearVertex *psVertices;
	IMG_FLOAT afClearSize[4];
	IMG_BOOL bIsFullScreen;
	IMG_UINT16 *pui16Indices;
	IMG_UINT32 *pui32Indices;
	IMG_UINT32 ui32NumIndices = 3;
	IMG_UINT32 ui32VertexDWords, ui32IndexDWords = 2;
	
	bIsFullScreen = GetClearSize(gc, afClearSize, bForceFullScreen);

	if(bIsFullScreen && (afClearSize[2] < (IMG_FLOAT)((EURASIA_PARAM_VF_X_MAXIMUM / 2) - 1)) &&
						(afClearSize[3] < (IMG_FLOAT)((EURASIA_PARAM_VF_Y_MAXIMUM / 2) - 1)))
	{
		ui32VertexDWords = 12;
	}
	else
	{
		ui32VertexDWords = 16;

		ui32NumIndices++;
	}

	psVertices = (GLES1ClearVertex *) CBUF_GetBufferSpace(gc->apsBuffers, ui32VertexDWords,
												CBUF_TYPE_VERTEX_DATA_BUFFER, IMG_FALSE);

	if(!psVertices)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	pui32Indices = CBUF_GetBufferSpace(gc->apsBuffers, ui32IndexDWords, CBUF_TYPE_INDEX_DATA_BUFFER, IMG_FALSE);

	if(!pui32Indices)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	/* Get the device address of the buffer */
	*puVertexAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, (IMG_UINT32 *)psVertices, CBUF_TYPE_VERTEX_DATA_BUFFER);
	*puIndexAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32Indices, CBUF_TYPE_INDEX_DATA_BUFFER);

	pui16Indices = (IMG_UINT16 *)pui32Indices;

	pui16Indices[0]	= 0;
	pui16Indices[1]	= 1;
	pui16Indices[2]	= 2;

	if(ui32VertexDWords == 12)
	{
		/* Use double sized triangle for performance */
		afClearSize[2] *= 2;
		afClearSize[3] *= 2;

		psVertices[0].fX = 0;
		psVertices[0].fY = 0;
		psVertices[0].fZ = fDepth;
		psVertices[0].ui32Color = gc->sState.sRaster.ui32ClearColor;

		psVertices[1].fX = afClearSize[2];
		psVertices[1].fY = 0;
		psVertices[1].fZ = fDepth;
		psVertices[1].ui32Color = gc->sState.sRaster.ui32ClearColor;

		psVertices[2].fX = 0;
		psVertices[2].fY = afClearSize[3];
		psVertices[2].fZ = fDepth;
		psVertices[2].ui32Color = gc->sState.sRaster.ui32ClearColor;
	}
	else
	{
		psVertices[0].fX = afClearSize[0];
		psVertices[0].fY = afClearSize[1];
		psVertices[0].fZ = fDepth;
		psVertices[0].ui32Color = gc->sState.sRaster.ui32ClearColor;

		psVertices[1].fX = afClearSize[2];
		psVertices[1].fY = afClearSize[1];
		psVertices[1].fZ = fDepth;
		psVertices[1].ui32Color = gc->sState.sRaster.ui32ClearColor;

		psVertices[2].fX = afClearSize[0];
		psVertices[2].fY = afClearSize[3];
		psVertices[2].fZ = fDepth;
		psVertices[2].ui32Color = gc->sState.sRaster.ui32ClearColor;

		psVertices[3].fX = afClearSize[2];
		psVertices[3].fY = afClearSize[3];
		psVertices[3].fZ = fDepth;
		psVertices[3].ui32Color = gc->sState.sRaster.ui32ClearColor;

		pui16Indices[3]	= 3;
	}

	CBUF_UpdateBufferPos(gc->apsBuffers, ui32VertexDWords, CBUF_TYPE_VERTEX_DATA_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_VERTEX_DATA_COUNT, ui32VertexDWords);

	CBUF_UpdateBufferPos(gc->apsBuffers, ui32IndexDWords, CBUF_TYPE_INDEX_DATA_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_INDEX_DATA_COUNT, ui32IndexDWords);
	
	*pui32NumIndices = ui32NumIndices;

	return GLES1_NO_ERROR;
}

/***********************************************************************************
 Function Name      : SetupVerticesAndShaderForClear
 Inputs             : gc, bForceFullScreen, fDepth
 Outputs            : -
 Returns            : -
 Description        : Generates vertices and shader for clear
************************************************************************************/
static GLES1_MEMERROR SetupVerticesAndShaderForClear(GLES1Context *gc, IMG_BOOL bForceFullScreen, IMG_FLOAT fDepth)
{
	IMG_UINT32 *pui32Buffer, *pui32BufferBase;
	PDS_VERTEX_SHADER_PROGRAM sPDSVertexShaderProgram;
	IMG_DEV_VIRTADDR uPDSCodeAddr, uVerticesAddr, uIndexAddr;
	IMG_UINT32 ui32NumIndices, ui32NumUSEAttribsInBytes;
	GLES1_MEMERROR eError;

	/* 4 PA's used for vertices */
	ui32NumUSEAttribsInBytes = 16;

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	ui32NumUSEAttribsInBytes += (SGX_USE_MINTEMPREGS << 2);
#endif

	eError = SetupVerticesForClear(gc, &uVerticesAddr, &uIndexAddr, &ui32NumIndices, bForceFullScreen, fDepth);
	
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
	sPDSVertexShaderProgram.asStreams[0].ui32Stride = 16;

	sPDSVertexShaderProgram.asStreams[0].asElements[0].ui32Offset = 0;
	sPDSVertexShaderProgram.asStreams[0].asElements[0].ui32Size = 16;
	sPDSVertexShaderProgram.asStreams[0].asElements[0].ui32Register = 0;

	/* Set the USE Task control words */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sPDSVertexShaderProgram.aui32USETaskControl[0] = (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU0_TRC_SHIFT);
	sPDSVertexShaderProgram.aui32USETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	sPDSVertexShaderProgram.aui32USETaskControl[2] = 0;
#else
	sPDSVertexShaderProgram.aui32USETaskControl[0] = 0;
	sPDSVertexShaderProgram.aui32USETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL |
												     (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU1_TRC_SHIFT);
	sPDSVertexShaderProgram.aui32USETaskControl[2] = 0;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	/* Set the execution address of the vertex copy program */
	SetUSEExecutionAddress(&sPDSVertexShaderProgram.aui32USETaskControl[0], 
							0, 
							gc->sPrim.psClearVertexCodeBlock->sCodeAddress, 
							gc->psSysContext->uUSEVertexHeapBase, 
							SGX_VTXSHADER_USE_CODE_BASE_INDEX);

	/*
		Get buffer space for the PDS vertex program
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, EURASIA_NUM_PDS_VERTEX_PROGRAM_DWORDS, CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);
	
	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	/* Generate the PDS Program for this shader */
	pui32Buffer = PDSGenerateVertexShaderProgram(&sPDSVertexShaderProgram, pui32BufferBase, IMG_NULL);

	/* Update buffer position */
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_VERT_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, ((IMG_UINT32)(pui32Buffer - pui32BufferBase)));
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
#endif
						ui32NumIndices				|
						((ui32NumIndices == 3) ? EURASIA_VDM_TRIS : EURASIA_VDM_STRIP);

	/* Index List 1 */	
	pui32Buffer[1]	= (uIndexAddr.uiAddr >> EURASIA_VDM_IDXBASE16_ALIGNSHIFT) << 
						EURASIA_VDM_IDXBASE16_SHIFT;

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
	pui32Buffer[5] =	(2 << EURASIA_VDMPDS_VERTEXSIZE_SHIFT)				|
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
	CBUF_UpdateBufferCommittedPrimOffsets(gc->apsBuffers,&gc->psRenderSurface->bPrimitivesSinceLastTA, (IMG_VOID *)gc, KickLimit_ScheduleTA);
	CBUF_UpdateVIBufferCommittedPrimOffsets(gc->apsBuffers,&gc->psRenderSurface->bPrimitivesSinceLastTA, (IMG_VOID *)gc, KickLimit_ScheduleTA);

	return GLES1_NO_ERROR;
}

/***********************************************************************************
 Function Name      : SetupFragmentShaderForClear
 Inputs             : gc
 Outputs            : pui32PDSState
 Returns            : -
 Description        : Generates fragment shader for clear
************************************************************************************/
static GLES1_MEMERROR SetupFragmentShaderForClear(GLES1Context *gc, IMG_UINT32 *pui32PDSState)
{
    PDS_PIXEL_SHADER_PROGRAM sProgram;
	IMG_UINT32 ui32PDSProgramSize, ui32PDSDataSize, ui32PDSDataSizeDS0, ui32PDSDataSizeDS1;
	IMG_UINT32 *pui32BufferBase, *pui32Buffer;
	IMG_DEV_VIRTADDR uPDSPixelShaderBaseAddr, uPDSPixelShaderSecondaryBaseAddr, uUSSECodeAddr;

	uUSSECodeAddr.uiAddr = gc->sPrim.psClearFragmentCodeBlock->sCodeAddress.uiAddr + 
						(gc->sState.sRaster.ui32ColorMask * GLES1_CLEAR_FRAGMENT_SECTION_SIZE_IN_BYTES);

	/*
		Setup the USE task control words
	*/
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sProgram.aui32USETaskControl[0]	= (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU0_TRC_SHIFT);
	sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_ITERATORSDEPENDENCY |
	                                  EURASIA_PDS_DOUTU1_MODE_PARALLEL	     |
									  EURASIA_PDS_DOUTU1_SDSOFT              ;
	sProgram.aui32USETaskControl[2]	= 0;
#else
	sProgram.aui32USETaskControl[0]	= EURASIA_PDS_DOUTU0_ITERATORSDEPENDENCY;
	sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL	     |
									  (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU1_TRC_SHIFT);
	sProgram.aui32USETaskControl[2]	= EURASIA_PDS_DOUTU2_SDSOFT;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	/* Set the execution address of the clear fragment shader */
	SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
							0, 
							uUSSECodeAddr, 
							gc->psSysContext->uUSEFragmentHeapBase, 
							SGX_PIXSHADER_USE_CODE_BASE_INDEX);
	/*
		Setup the FPU iterator control words and calculate the size of the PDS pixel program.
	*/
#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
	ui32PDSProgramSize = 4;
#else
	ui32PDSProgramSize = 3;
#endif
	ui32PDSDataSize = 4;

	sProgram.ui32NumFPUIterators = 1;

#if defined(SGX545)
	sProgram.aui32FPUIterators1[0] = 0;
	sProgram.aui32FPUIterators0[0] = 
#else 
	sProgram.aui32FPUIterators[0] = 
#endif /* defined(SGX545) */

#if defined(FIX_HW_BRN_29838)
									EURASIA_PDS_DOUTI_USEFORMAT_F16		|
									EURASIA_PDS_DOUTI_USECOLFLOAT		|
									(0 << EURASIA_PDS_DOUTI_USEWRAP_SHIFT)	|
									EURASIA_PDS_DOUTI_USEISSUE_TC0		| 
#else
									EURASIA_PDS_DOUTI_USEISSUE_V0		|
#endif
	                                
									EURASIA_PDS_DOUTI_TEXISSUE_NONE		|
									EURASIA_PDS_DOUTI_USEPERSPECTIVE	|
									EURASIA_PDS_DOUTI_USELASTISSUE		|
									EURASIA_PDS_DOUTI_USEDIM_4D;


	sProgram.aui32TAGLayers[0]	= 0xFFFFFFFF;

	ui32PDSDataSizeDS0 = (ui32PDSDataSize / 4) * 2 + MIN((ui32PDSDataSize % 4), 2);	/* PRQA S 3355 */ /* Use this expression to keep code flexible as ui32PDSDataSize may have different value in the future. */
	ui32PDSDataSizeDS1 = ui32PDSDataSize - ui32PDSDataSizeDS0;

	/* PRQA S 3358 1 */ /* Use this expression to keep code flexible as ui32PDSDataSizeDS1 may have different value if ui32PDSDataSize changes. */
	if (ui32PDSDataSizeDS1 & 7)
	{
		ui32PDSDataSize = ((ui32PDSDataSizeDS0 + 7) & ~7UL) + ui32PDSDataSizeDS1;
	}

	ui32PDSProgramSize += (ui32PDSDataSize + ((1 << (EURASIA_PDS_DATASIZE_ALIGNSHIFT - 2)) - 1)) & ~((1 << (EURASIA_PDS_DATASIZE_ALIGNSHIFT - 2UL)) - 1UL);

	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, ui32PDSProgramSize, CBUF_TYPE_PDS_FRAG_BUFFER, IMG_FALSE);
	
	if(!pui32BufferBase)
	{
		return GLES1_3D_BUFFER_ERROR;
	}

	/* Save the PDS pixel shader data segment address and size (always offset by PDS exec base) */
	uPDSPixelShaderBaseAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_PDS_FRAG_BUFFER);

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
	uPDSPixelShaderBaseAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

	/*
		Generate the PDS pixel state program.
		Should be ok sending NULL for psTexImageUnits, as we are not doing any texture lookups
	*/
	pui32Buffer = PDSGeneratePixelShaderProgram(IMG_NULL,
												&sProgram,
												pui32BufferBase);

	/*
	   Update buffer position 
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_FRAG_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_PDS_FRAG_DATA_COUNT, ((IMG_UINT32)(pui32Buffer - pui32BufferBase)));
	GLES1_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= ui32PDSProgramSize);

	/* Save the PDS pixel shader secondary data segment address and size (always offset by PDS exec base) */
	uPDSPixelShaderSecondaryBaseAddr = gc->sPrim.psDummyPixelSecondaryPDSCode->sCodeAddress;
	
#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
	uPDSPixelShaderSecondaryBaseAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

	/*
		Write the PDS pixel shader secondary attribute base address and data size
	*/	
	pui32PDSState[0] =  (((uPDSPixelShaderSecondaryBaseAddr.uiAddr >> EURASIA_PDS_BASEADD_ALIGNSHIFT) 
						<< EURASIA_PDS_BASEADD_SHIFT) & ~EURASIA_PDS_BASEADD_CLRMSK)					|
						(((gc->sPrim.ui32DummyPDSPixelSecondaryDataSize >> EURASIA_PDS_DATASIZE_ALIGNSHIFT) 
							<< EURASIA_PDS_DATASIZE_SHIFT) & ~EURASIA_PDS_DATASIZE_CLRMSK);
	/*
		Write the DMS pixel control information
	*/
	pui32PDSState[1] =	(0 << EURASIA_PDS_SECATTRSIZE_SHIFT)	|
						(3 << EURASIA_PDS_USETASKSIZE_SHIFT)	|
#if defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
						((uPDSPixelShaderSecondaryBaseAddr.uiAddr & EURASIA_PDS_BASEADD_MSB) ? EURASIA_PDS_BASEADD_SEC_MSB : 0) |
						((uPDSPixelShaderBaseAddr.uiAddr & EURASIA_PDS_BASEADD_MSB) ? EURASIA_PDS_BASEADD_PRIM_MSB : 0) |
#endif
						((EURASIA_PDS_PDSTASKSIZE_MAX << EURASIA_PDS_PDSTASKSIZE_SHIFT)	& 
						~EURASIA_PDS_PDSTASKSIZE_CLRMSK)		|
#if defined(FIX_HW_BRN_29838)
						(2 << EURASIA_PDS_PIXELSIZE_SHIFT);
#else
						(1 << EURASIA_PDS_PIXELSIZE_SHIFT);
#endif

	/*
		Write the PDS pixel shader base address and data size
	*/
	pui32PDSState[2]	= (((uPDSPixelShaderBaseAddr.uiAddr >> EURASIA_PDS_BASEADD_ALIGNSHIFT)
					<< EURASIA_PDS_BASEADD_SHIFT) & ~EURASIA_PDS_BASEADD_CLRMSK)				|
					(((sProgram.ui32DataSize >> EURASIA_PDS_DATASIZE_ALIGNSHIFT) 
					<< EURASIA_PDS_DATASIZE_SHIFT) & ~EURASIA_PDS_DATASIZE_CLRMSK);

	return GLES1_NO_ERROR;
}


#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)

/***********************************************************************************
 Function Name      : SendDepthBiasPrims
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Sends a depth bias object
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR SendDepthBiasPrims(GLES1Context *gc)
{
	IMG_UINT32 ui32StateHeader;
	IMG_DEV_VIRTADDR uStateAddr;
	IMG_UINT32 *pui32Buffer, *pui32BufferBase, ui32NumStateDWords;
	GLES1_MEMERROR eError;

	/* Always send ispA, output selects (including texsize word)
	   and MTE control - rest can take default/previous state 
	*/
	ui32StateHeader =	EURASIA_TACTLPRES_ISPCTLFF0		|
						EURASIA_TACTLPRES_ISPCTLFF1		|
						EURASIA_TACTLPRES_ISPCTLFF2		|
						EURASIA_TACTLPRES_OUTSELECTS	|
						EURASIA_TACTLPRES_MTECTRL		|
						EURASIA_TACTLPRES_TEXSIZE		|
						EURASIA_TACTLPRES_TEXFLOAT		|
						EURASIA_TACTLPRES_RGNCLIP;

	ui32NumStateDWords = 10;

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

	*pui32Buffer++ = ui32StateHeader;

	*pui32Buffer++ = EURASIA_ISPA_PASSTYPE_DEPTHBIAS | ((gc->sState.sPolygon.i32Units << EURASIA_ISPA_DEPTHBIAS_SHIFT) & ~EURASIA_ISPA_DEPTHBIAS_CLRMSK);

	*pui32Buffer++ = gc->sState.sPolygon.factor.ui32Val;
	
	*pui32Buffer++ = 0;

	*pui32Buffer++	= EURASIA_TARGNCLIP_MODE_NONE;
	*pui32Buffer++	= 0;


	/* ========= */
	/* Output Selects */
	/* ========= */
	*pui32Buffer++ =	EURASIA_MTE_WPRESENT	|
#if !defined(FIX_HW_BRN_29838)
						EURASIA_MTE_BASE		|
#endif
						(8 << EURASIA_MTE_VTXSIZE_SHIFT);


	/* ========= */
	/*	MTE control */
	/* ========= */
	*pui32Buffer++	=	EURASIA_MTE_PRETRANSFORM	|
#if defined(EURASIA_MTE_SHADE_GOURAUD)
	                    EURASIA_MTE_SHADE_GOURAUD	|
#endif
						EURASIA_MTE_CULLMODE_NONE;
	
	/* ========= */
	/*	Texture coordinate size */
	/* ========= */
#if defined(SGX545)
	*pui32Buffer++ = (EURASIA_MTE_ATTRDIM_UVST << EURASIA_MTE_ATTRDIM0_SHIFT);
#else
	*pui32Buffer++ = 0;
#endif

	/* ========= */
	/*	Texture float info */
	/* ========= */
	*pui32Buffer++ = 0;	/* PRQA S 3199 */ /* pui32Buffer is used in the following GLES1_ASSERT. */
	
	GLES1_ASSERT(ui32NumStateDWords == (IMG_UINT32)(pui32Buffer - pui32BufferBase));

	CBUF_UpdateBufferPos(gc->apsBuffers, ui32NumStateDWords, CBUF_TYPE_PDS_VERT_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, ui32NumStateDWords);

	GLES1_INC_COUNT(GLES1_TIMER_MTE_STATE_COUNT, ui32NumStateDWords);

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

	eError = SetupVerticesAndShaderForClear(gc, IMG_TRUE, 0.0f);
	
	if(eError != GLES1_NO_ERROR)
	{
		return eError;
	}

	gc->ui32EmitMask |=	GLES1_EMITSTATE_MTE_STATE_ISP |
						GLES1_EMITSTATE_MTE_STATE_CONTROL |
						GLES1_EMITSTATE_MTE_STATE_OUTPUTSELECTS |
						GLES1_EMITSTATE_MTE_STATE_REGION_CLIP;

	gc->psRenderSurface->i32LastUnitsInObject = gc->sState.sPolygon.i32Units;
	gc->psRenderSurface->fLastFactorInObject = gc->sState.sPolygon.factor.fVal;

	gc->psRenderSurface->ui32DepthBiasState = EGL_RENDER_DEPTHBIAS_OBJECT_USED;

	return GLES1_NO_ERROR;
}
#endif /* SGX_FEATURE_DEPTH_BIAS_OBJECTS */

/***********************************************************************************
 Function Name      : SendClearPrims
 Inputs             : gc, ui32ClearFlags, bForceFullScreen, fDepth
 Outputs            : -
 Returns            : -
 Description        : Sends a clear object
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR SendClearPrims(GLES1Context *gc, IMG_UINT32 ui32ClearFlags, 
										   IMG_BOOL bForceFullScreen, IMG_FLOAT fDepth)
{
	IMG_UINT32 ui32StateHeader;
	IMG_UINT32 ui32ISPControlA, ui32ISPControlB = 0, ui32ISPControlC = 0;
	IMG_DEV_VIRTADDR uStateAddr;
	IMG_UINT32 *pui32Buffer, *pui32BufferBase, ui32NumStateDWords;
	IMG_UINT32 aui32PDSState[3] = {0, 0, 0};
	GLES1_MEMERROR eError;

	/* Always send ispA, output selects (including texsize word)
	   and MTE control - rest can take default/previous state 
	*/
	ui32StateHeader =	EURASIA_TACTLPRES_ISPCTLFF0		|
						EURASIA_TACTLPRES_OUTSELECTS	|
						EURASIA_TACTLPRES_MTECTRL		|
						EURASIA_TACTLPRES_TEXSIZE		|
						EURASIA_TACTLPRES_TEXFLOAT;

	ui32ISPControlA =	EURASIA_ISPA_PASSTYPE_OPAQUE	|
						EURASIA_ISPA_DCMPMODE_ALWAYS	|
	                    EURASIA_ISPA_OBJTYPE_TRI		;

	ui32NumStateDWords = 6;

	if(ui32ClearFlags & GLES1_CLEARFLAG_COLOR)
	{
		ui32StateHeader |= EURASIA_TACTLPRES_ISPCTLFF1;
		ui32ISPControlB |= (gc->sState.sRaster.ui32ColorMask << EURASIA_ISPB_UPASSCTL_SHIFT);

		ui32StateHeader |= EURASIA_TACTLPRES_PDSSTATEPTR;
		ui32ISPControlA |= EURASIA_ISPA_BPRES;

		eError = SetupFragmentShaderForClear(gc, aui32PDSState);

		if(eError != GLES1_NO_ERROR)
		{
			return eError;
		}

		/* 1 for ISPB, 3 for PDS state */
		ui32NumStateDWords += 4;

		gc->ui32EmitMask |=	GLES1_EMITSTATE_PDS_FRAGMENT_STATE | 
							GLES1_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE;
	}
	else
	{
		/* No PDS state needed for TWD objects */
		ui32ISPControlA |= EURASIA_ISPA_TAGWRITEDIS;
	}

	/* May need to send region clip word dirtied from last scene */
	if(gc->ui32EmitMask & GLES1_EMITSTATE_MTE_STATE_REGION_CLIP)
	{
		ui32StateHeader |= EURASIA_TACTLPRES_RGNCLIP;
		ui32NumStateDWords += 2;
	}

	if((ui32ClearFlags & GLES1_CLEARFLAG_DEPTH) == 0)
	{
		ui32ISPControlA |= EURASIA_ISPA_DWRITEDIS;
	}

	if(ui32ClearFlags & GLES1_CLEARFLAG_STENCIL)
	{
		ui32ISPControlA |= (gc->sState.sStencil.ui32Clear << EURASIA_ISPA_SREF_SHIFT);
		ui32ISPControlA |= EURASIA_ISPA_CPRES;
		ui32StateHeader |= EURASIA_TACTLPRES_ISPCTLFF2;

		ui32ISPControlC |= EURASIA_ISPC_SCMP_ALWAYS	 |
						   EURASIA_ISPC_SOP3_REPLACE |
						   (gc->sState.sStencil.ui32Stencil & ~EURASIA_ISPC_SWMASK_CLRMSK);

		/* 1 for ISPC */
		ui32NumStateDWords++;
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

	*pui32Buffer++ = ui32StateHeader;

	*pui32Buffer++ = ui32ISPControlA;

	if(ui32StateHeader & EURASIA_TACTLPRES_ISPCTLFF1)
	{
		*pui32Buffer++ = ui32ISPControlB;
	}

	if(ui32StateHeader & EURASIA_TACTLPRES_ISPCTLFF2)
	{
		*pui32Buffer++ = ui32ISPControlC;
	}

	if(ui32StateHeader & EURASIA_TACTLPRES_PDSSTATEPTR)
	{
		*pui32Buffer++ = aui32PDSState[0];
		*pui32Buffer++ = aui32PDSState[1];
		*pui32Buffer++ = aui32PDSState[2];
	}

	if(ui32StateHeader & EURASIA_TACTLPRES_RGNCLIP)
	{
		*pui32Buffer++	= gc->psRenderSurface->aui32RegionClipWord[0];
		*pui32Buffer++	= gc->psRenderSurface->aui32RegionClipWord[1];
	}


	/* ========= */
	/* Output Selects */
	/* ========= */
	*pui32Buffer++ =	EURASIA_MTE_WPRESENT	|
#if !defined(FIX_HW_BRN_29838)
						EURASIA_MTE_BASE		|
#endif
						(8 << EURASIA_MTE_VTXSIZE_SHIFT);


	/* ========= */
	/*	MTE control */
	/* ========= */
	*pui32Buffer++	=
#if !defined(SGX545)
	                    EURASIA_MTE_SHADE_GOURAUD	|
#endif
						EURASIA_MTE_PRETRANSFORM	|
						EURASIA_MTE_CULLMODE_NONE;
	
	/* ========= */
	/*	Texture coordinate size */
	/* ========= */
#if defined(SGX545)
	*pui32Buffer++ = (EURASIA_MTE_ATTRDIM_UVST << EURASIA_MTE_ATTRDIM0_SHIFT);
#else
	*pui32Buffer++ = 0;
#endif

	/* ========= */
	/*	Texture float info */
	/* ========= */
	*pui32Buffer++ = 0;	/* PRQA S 3199 */ /* pui32Buffer is used in the following GLES1_ASSERT. */
	
	GLES1_ASSERT(ui32NumStateDWords == (IMG_UINT32)(pui32Buffer - pui32BufferBase));

	CBUF_UpdateBufferPos(gc->apsBuffers, ui32NumStateDWords, CBUF_TYPE_PDS_VERT_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, ui32NumStateDWords);

	GLES1_INC_COUNT(GLES1_TIMER_MTE_STATE_COUNT, ui32NumStateDWords);

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

	eError = SetupVerticesAndShaderForClear(gc, bForceFullScreen, fDepth);
	
	if(eError != GLES1_NO_ERROR)
	{
		return eError;
	}

	if(gc->ui32EmitMask & GLES1_EMITSTATE_MTE_STATE_REGION_CLIP)
	{
		gc->ui32EmitMask &= ~GLES1_EMITSTATE_MTE_STATE_REGION_CLIP;
	}

	gc->ui32EmitMask |=	GLES1_EMITSTATE_MTE_STATE_ISP |
						GLES1_EMITSTATE_MTE_STATE_CONTROL |
						GLES1_EMITSTATE_MTE_STATE_OUTPUTSELECTS;

	return GLES1_NO_ERROR;
}


/***********************************************************************************
 Function Name      : glClear
 Inputs             : mask
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Clears buffers specified by mask. 
					  If a frame has not been started clears can go through start of 
					  frame, otherwise they are sent as primitives - they ARE affected 
					  by current scissor. Special programs/state must be used
					  for these primitives. 
					  All clears will be sent with Depth func always.
					  A color clear will be sent with depth write disable.
					  A depth clear will be sent with depth write enable and TAG write disable.
					  A clear of both color and depth will be sent depth write enable and 
					  TAG write enable. 
************************************************************************************/
GL_API void GL_APIENTRY glClear(GLbitfield mask)
{
	IMG_UINT32 ui32ClearFlags = 0;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glClear"));

	GLES1_TIME_START(GLES1_TIMES_glClear);

	if (mask & ~(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
		))
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glClear);

		return;
	}

	if(GetFrameBufferCompleteness(gc) != GL_FRAMEBUFFER_COMPLETE_OES)
	{
		SetError(gc, GL_INVALID_FRAMEBUFFER_OPERATION_OES);

		GLES1_TIME_STOP(GLES1_TIMES_glClear);

		return;
	}

	if (((mask & GL_COLOR_BUFFER_BIT) != 0) && (gc->sState.sRaster.ui32ColorMask != 0))
	{
		/* Do color clear */
		ui32ClearFlags |= GLES1_CLEARFLAG_COLOR;
	}

	if (((mask & GL_DEPTH_BUFFER_BIT) != 0) && gc->psMode->ui32DepthBits && 
		((gc->sState.sDepth.ui32TestFunc & EURASIA_ISPA_DWRITEDIS) == 0))
	{
		/* Do depth clear */
		ui32ClearFlags |= GLES1_CLEARFLAG_DEPTH;
	}

	if (((mask & GL_STENCIL_BUFFER_BIT) != 0) && gc->psMode->ui32StencilBits)
	{
		/* Do stencil clear */
		ui32ClearFlags |= GLES1_CLEARFLAG_STENCIL;
	}

	if(ui32ClearFlags)
	{
		if(!PrepareToDraw(gc, &ui32ClearFlags, IMG_TRUE))
		{
			PVR_DPF((PVR_DBG_ERROR,"glClear: Can't prepare to draw"));

			GLES1_TIME_STOP(GLES1_TIMES_glClear);

			return;
		}

		/*
		 * An accumulate object can do the depth clear, so check to see whether
		 * a clear is still needed
		 */
		if(ui32ClearFlags)
		{


			if(SendDrawMaskForClear(gc) != GLES1_NO_ERROR)
			{
				PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);

				PVR_DPF((PVR_DBG_ERROR,"glClear: Can't send drawmask for clear"));
				GLES1_TIME_STOP(GLES1_TIMES_glClear);
				return;
			}
			
			if(SendClearPrims(gc, ui32ClearFlags, IMG_FALSE, gc->sState.sDepth.fClear) != GLES1_NO_ERROR)
			{
				PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);

				PVR_DPF((PVR_DBG_ERROR,"glClear: Can't send clear prims"));
				GLES1_TIME_STOP(GLES1_TIMES_glClear);
				return;
			}

			/* Make sure thet we emit any drawmasks needed for primitives on the next draw call */
			gc->bDrawMaskInvalid = IMG_TRUE;
		}

		PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);
	}


	GLES1_TIME_STOP(GLES1_TIMES_glClear);
}


/***********************************************************************************
 Function Name      : glClearColor(x)
 Inputs             : red, green, blue, alpha
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Stores color of clear internally.
					  Used when clear primitives/start of frame clears are sent.
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glClearColor"));

	GLES1_TIME_START(GLES1_TIMES_glClearColor);

	gc->sState.sRaster.sClearColor.fRed   = Clampf(red, GLES1_Zero, GLES1_One);
	gc->sState.sRaster.sClearColor.fGreen = Clampf(green, GLES1_Zero, GLES1_One);
	gc->sState.sRaster.sClearColor.fBlue  = Clampf(blue, GLES1_Zero, GLES1_One);
	gc->sState.sRaster.sClearColor.fAlpha = Clampf(alpha, GLES1_Zero, GLES1_One);

	gc->sState.sRaster.ui32ClearColor = ColorConvertToHWFormat(&gc->sState.sRaster.sClearColor);

	GLES1_TIME_STOP(GLES1_TIMES_glClearColor);
}
#endif

GL_API void GL_APIENTRY glClearColorx(GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glClearColorx"));

	GLES1_TIME_START(GLES1_TIMES_glClearColorx);

	gc->sState.sRaster.sClearColor.fRed   = Clampf(FIXED_TO_FLOAT(red), GLES1_Zero, GLES1_One);
	gc->sState.sRaster.sClearColor.fGreen = Clampf(FIXED_TO_FLOAT(green), GLES1_Zero, GLES1_One);
	gc->sState.sRaster.sClearColor.fBlue  = Clampf(FIXED_TO_FLOAT(blue), GLES1_Zero, GLES1_One);
	gc->sState.sRaster.sClearColor.fAlpha = Clampf(FIXED_TO_FLOAT(alpha), GLES1_Zero, GLES1_One);

	gc->sState.sRaster.ui32ClearColor = ColorConvertToHWFormat(&gc->sState.sRaster.sClearColor);

	GLES1_TIME_STOP(GLES1_TIMES_glClearColorx);
}


/***********************************************************************************
 Function Name      : glClearDepth(f/x)
 Inputs             : depth
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Stores depth of clear internally.
					  Used when clear primitives/start of frame clears are sent.
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glClearDepthf(GLclampf depth)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glClearDepthf"));

	GLES1_TIME_START(GLES1_TIMES_glClearDepthf);

	gc->sState.sDepth.fClear = Clampf(depth, GLES1_Zero, GLES1_One);

	GLES1_TIME_STOP(GLES1_TIMES_glClearDepthf);
}

#endif

GL_API void GL_APIENTRY glClearDepthx(GLclampx depth)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glClearDepthx"));

	GLES1_TIME_START(GLES1_TIMES_glClearDepthx);

	gc->sState.sDepth.fClear = Clampf(FIXED_TO_FLOAT(depth), GLES1_Zero, GLES1_One);

	GLES1_TIME_STOP(GLES1_TIMES_glClearDepthx);
}


/***********************************************************************************
 Function Name      : glClearStencil
 Inputs             : s
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Stores stencil clear value internally.
					  Used when clear primitives/start of frame clears are sent.
************************************************************************************/
GL_API void GL_APIENTRY glClearStencil(GLint s)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glClearStencil"));

	GLES1_TIME_START(GLES1_TIMES_glClearStencil);

	gc->sState.sStencil.ui32Clear = ((IMG_UINT32)s & GLES1_MAX_STENCIL_VALUE);

	GLES1_TIME_STOP(GLES1_TIMES_glClearStencil);
}


/******************************************************************************
 End of file (clear.c)
******************************************************************************/

