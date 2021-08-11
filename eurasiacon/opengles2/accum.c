/******************************************************************************
 * Name         : accum.c
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
 * $Log: accum.c $
 * 
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "context.h"
#include "sgxpdsdefs.h"
#include "usegles2.h"

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

#if defined(FIX_HW_BRN_31988)
#define GLES2_ACCUM_VERTEX_FIRSTPHAS_SIZE_IN_BYTES (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 9), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))
#else
#define GLES2_ACCUM_VERTEX_FIRSTPHAS_SIZE_IN_BYTES (ALIGNCOUNT((EURASIA_USE_INSTRUCTION_SIZE * 5), EURASIA_PDS_DOUTU_PHASE_START_ALIGN))
#endif

#if defined(SGX_FEATURE_VCB)
/* 1st phase, EMITVCB, PHAS */
#define GLES2_ACCUM_VERTEX_SIZE_IN_BYTES	(GLES2_ACCUM_VERTEX_FIRSTPHAS_SIZE_IN_BYTES + (EURASIA_USE_INSTRUCTION_SIZE * 2)) 
#else /* defined(SGX_FEATURE_VCB) */
#define GLES2_ACCUM_VERTEX_SIZE_IN_BYTES	(GLES2_ACCUM_VERTEX_FIRSTPHAS_SIZE_IN_BYTES)
#endif /* defined(SGX_FEATURE_VCB) */

#define GLES2_ACCUM_FRAGMENT_SIZE_IN_BYTES	(EURASIA_USE_INSTRUCTION_SIZE * 2)

#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

#define GLES2_ACCUM_VERTEX_SIZE_IN_BYTES	(EURASIA_USE_INSTRUCTION_SIZE * 4)
#define GLES2_ACCUM_FRAGMENT_SIZE_IN_BYTES	EURASIA_USE_INSTRUCTION_SIZE

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */


/***********************************************************************************
 Function Name      : InitAccumUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : Success
 Description        : Initialises all USSE code blocks required for accum/hwbg obj
************************************************************************************/
IMG_INTERNAL IMG_BOOL InitAccumUSECodeBlocks(GLES2Context *gc)
{
	IMG_UINT32 *pui32BufferBase, *pui32Buffer;
	IMG_UINT32 ui32PDSProgramSizeInDWords;
	PDS_PIXEL_SHADER_SA_PROGRAM	sProgram = {0};

	gc->sPrim.psAccumVertexCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEVertexCodeHeap,
															GLES2_ACCUM_VERTEX_SIZE_IN_BYTES,
															gc->psSysContext->hPerProcRef);
	
	GLES2_INC_COUNT(GLES2_TIMER_USECODEHEAP_VERT_COUNT, GLES2_ACCUM_VERTEX_SIZE_IN_BYTES >> 2);

	if(!gc->sPrim.psAccumVertexCodeBlock)
	{
		PVR_DPF((PVR_DBG_FATAL,"InitAccumUSECodeBlocks: Couldn't allocate USE vertex code for Accumulate Object"));
		return IMG_FALSE;
	}

	pui32BufferBase = gc->sPrim.psAccumVertexCodeBlock->pui32LinAddress;

	/* PRQA S 3198 1 */ /* pui32Buffer is used in the following GLES_ASSERT. */
	pui32Buffer =  USEGenWriteSpecialObjVtxProgram (pui32BufferBase, 
													USEGEN_ACCUM,
													gc->sPrim.psAccumVertexCodeBlock->sCodeAddress,
													gc->psSysContext->uUSEVertexHeapBase, 
													SGX_VTXSHADER_USE_CODE_BASE_INDEX);

	GLES_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= GLES2_ACCUM_VERTEX_SIZE_IN_BYTES >> 2);


	gc->sPrim.psHWBGCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psUSEFragmentCodeHeap,
												GLES2_ACCUM_FRAGMENT_SIZE_IN_BYTES,
												gc->psSysContext->hPerProcRef);

	GLES2_INC_COUNT(GLES2_TIMER_USECODEHEAP_FRAG_COUNT, GLES2_ACCUM_FRAGMENT_SIZE_IN_BYTES >> 2);

	if(!gc->sPrim.psHWBGCodeBlock)
	{
		PVR_DPF((PVR_DBG_FATAL,"InitAccumUSECodeBlocks: Couldn't allocate USE code for HW BG Object"));
		
		UCH_CodeHeapFree(gc->sPrim.psAccumVertexCodeBlock);
		return IMG_FALSE;
	}

	pui32Buffer = pui32BufferBase = gc->sPrim.psHWBGCodeBlock->pui32LinAddress;

	/* just do a single mov instruction:	MOV o0, pa0 */
	USEGenWriteClearPixelProgram(pui32Buffer, IMG_FALSE);

	/*
		Get memory for the PDS program
	*/
	ui32PDSProgramSizeInDWords = 1;

#if defined(SGX_FEATURE_SECONDARY_REQUIRES_USE_KICK)
#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
	ui32PDSProgramSizeInDWords += (8 + 2);
#else
	ui32PDSProgramSizeInDWords += (4 + 1);
#endif
		
	sProgram.bKickUSE = IMG_TRUE;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sProgram.aui32USETaskControl[0]	= 0 << EURASIA_PDS_DOUTU0_TRC_SHIFT;
	sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	sProgram.aui32USETaskControl[2]	= 0;
#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */
	sProgram.aui32USETaskControl[0]	= 0;
	sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL | (0 << EURASIA_PDS_DOUTU1_TRC_SHIFT);
	sProgram.aui32USETaskControl[2]	= 0;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
							0, 
							gc->sProgram.psDummyFragUSECode->sDevVAddr, 
							gc->psSysContext->uUSEFragmentHeapBase, 
							SGX_PIXSHADER_USE_CODE_BASE_INDEX);

#endif /* SGX_FEATURE_SECONDARY_REQUIRES_USE_KICK */

	gc->sPrim.psDummyPixelSecondaryPDSCode = UCH_CodeHeapAllocate(gc->psSharedState->psPDSFragmentCodeHeap, ui32PDSProgramSizeInDWords << 2, gc->psSysContext->hPerProcRef);

	GLES2_INC_COUNT(GLES2_TIMER_PDSCODEHEAP_FRAG_COUNT, ui32PDSProgramSizeInDWords);

	if(!gc->sPrim.psDummyPixelSecondaryPDSCode)
	{
		PVR_DPF((PVR_DBG_FATAL,"InitAccumUSECodeBlocks: Failed to allocate Dummy Pixel Secondary PDS program"));

		UCH_CodeHeapFree(gc->sPrim.psAccumVertexCodeBlock);
		UCH_CodeHeapFree(gc->sPrim.psHWBGCodeBlock);

		return IMG_FALSE;
	}
	
	pui32BufferBase = gc->sPrim.psDummyPixelSecondaryPDSCode->pui32LinAddress;
	
	pui32Buffer = PDSGeneratePixelShaderSAProgram(&sProgram, pui32BufferBase); /* PRQA S 3199 */ /* pui32Buffer is used in the following GLES_ASSERT. */

	GLES_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) == ui32PDSProgramSizeInDWords);

	gc->sPrim.ui32DummyPDSPixelSecondaryDataSize = sProgram.ui32DataSize;
	
	return IMG_TRUE;
}

/***********************************************************************************
 Function Name      : FreeAccumUSECodeBlocks
 Inputs             : gc
 Outputs            : 
 Returns            : -
 Description        : Frees all USSE code blocks required for accum/hwbg obj
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeAccumUSECodeBlocks(GLES2Context *gc)
{
	UCH_CodeHeapFree(gc->sPrim.psAccumVertexCodeBlock);
	UCH_CodeHeapFree(gc->sPrim.psHWBGCodeBlock);
	UCH_CodeHeapFree(gc->sPrim.psDummyPixelSecondaryPDSCode);
}


/***********************************************************************************
 Function Name      : SetupVerticesForAccum
 Inputs             : gc
 Outputs            : puVertexAddr, puIndexAddr, pui32NumIndices, fDepth
 Returns            : Mem Error
 Description        : Generates vertices and indices for accumulate
************************************************************************************/
static GLES2_MEMERROR SetupVerticesForAccum(GLES2Context *gc,
											IMG_DEV_VIRTADDR *puVertexAddr, 
											IMG_DEV_VIRTADDR *puIndexAddr,
											IMG_UINT32 *pui32NumIndices,
											IMG_FLOAT fDepth)
{
	IMG_FLOAT *pfVertices;
	IMG_UINT16 *pui16Indices;
	IMG_UINT32 ui32NumIndices = 3;
	IMG_UINT32 ui32VertexDWords, ui32IndexDWords = 2;
	
	if((gc->psDrawParams->ui32AccumWidth < ((EURASIA_PARAM_VF_X_MAXIMUM / 2) - 1)) &&
		(gc->psDrawParams->ui32AccumHeight < ((EURASIA_PARAM_VF_Y_MAXIMUM / 2) - 1)))
	{
		ui32VertexDWords = 15;
	}
	else
	{
		ui32VertexDWords = 20;

		ui32NumIndices++;
	}

	pfVertices = (IMG_FLOAT *) CBUF_GetBufferSpace(gc->apsBuffers, ui32VertexDWords,
												CBUF_TYPE_VERTEX_DATA_BUFFER, IMG_FALSE);

	if(!pfVertices)
	{
		return GLES2_TA_BUFFER_ERROR;
	}

	pui16Indices = (IMG_UINT16 *) CBUF_GetBufferSpace(gc->apsBuffers, ui32IndexDWords,
												 CBUF_TYPE_INDEX_DATA_BUFFER, IMG_FALSE);

	if(!pui16Indices)
	{
		return GLES2_TA_BUFFER_ERROR;
	}

	/* Get the device address of the buffer */
	*puVertexAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, (IMG_UINT32 *)pfVertices, CBUF_TYPE_VERTEX_DATA_BUFFER);
	*puIndexAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, (IMG_VOID *)pui16Indices, CBUF_TYPE_INDEX_DATA_BUFFER);

	*pui16Indices++	= 0;
	*pui16Indices++	= 1;
	*pui16Indices++	= 2;

	if(ui32VertexDWords == 15)
	{
		/* Use double sized triangle for performance */
		/* X, Y, Z */
		pfVertices[0]	= 0.0f;
		pfVertices[1]	= 0.0f;
		pfVertices[2]	= fDepth;

		/* X, Y, Z */
		pfVertices[5]	= (IMG_FLOAT)(gc->psDrawParams->ui32AccumWidth) * 2.0f;
		pfVertices[6]	= 0.0f;
		pfVertices[7]	= fDepth;

		/* X, Y, Z */
		pfVertices[10]	= 0.0f;
		pfVertices[11]	= (IMG_FLOAT)(gc->psDrawParams->ui32AccumHeight) * 2.0f;
		pfVertices[12]	= fDepth;
		
		switch(gc->psDrawParams->eAccumRotationAngle)
		{
			default:
			case PVRSRV_ROTATE_0:
			case PVRSRV_FLIP_Y:
			{
				/* U, V */
				pfVertices[3]	= 0.0f;
				pfVertices[4]	= 0.0f;

				/* U, V */
				pfVertices[8]	= 2.0f;
				pfVertices[9]	= 0.0f;

				/* U, V */
				pfVertices[13]	= 0.0f;
				pfVertices[14]	= 2.0f;

				break;
			}
			case PVRSRV_ROTATE_90:
			{
				/* U, V */
				pfVertices[3]	= 1.0f;
				pfVertices[4]	= 0.0f;
			
				/* U, V */
				pfVertices[8]	= 1.0f;
				pfVertices[9]	= 2.0f;
				
				/* U, V */
				pfVertices[13]	= -1.0f;
				pfVertices[14]	= 0.0f;

				break;
			}
			case PVRSRV_ROTATE_180:
			{
				/* U, V */
				pfVertices[3]	= 1.0f;
				pfVertices[4]	= 1.0f;
			
				/* U, V */
				pfVertices[8]	= -1.0f;
				pfVertices[9]	= 1.0f;
				
				/* U, V */
				pfVertices[13]	= 1.0f;
				pfVertices[14]	= -1.0f;

				break;
			}
			case PVRSRV_ROTATE_270:
			{
				/* U, V */
				pfVertices[3]	= 0.0f;
				pfVertices[4]	= 1.0f;
			
				/* U, V */
				pfVertices[8]	= 0.0f;
				pfVertices[9]	= -1.0f;
				
				/* U, V */
				pfVertices[13]	= 2.0f;
				pfVertices[14]	= 1.0f;

				break;
			}
		}
	}
	else
	{
		/* X, Y, Z */
		pfVertices[0]	= 0.0f;
		pfVertices[1]	= 0.0f;
		pfVertices[2]	= fDepth;

		/* X, Y, Z */
		pfVertices[5]	= (IMG_FLOAT)gc->psDrawParams->ui32AccumWidth;
		pfVertices[6]	= 0.0f;
		pfVertices[7]	= fDepth;

		/* X, Y, Z */
		pfVertices[10]	= 0.0f;
		pfVertices[11]	= (IMG_FLOAT)gc->psDrawParams->ui32AccumHeight;
		pfVertices[12]	= fDepth;

		/* X, Y, Z */
		pfVertices[15]	= (IMG_FLOAT)gc->psDrawParams->ui32AccumWidth;
		pfVertices[16]	= (IMG_FLOAT)gc->psDrawParams->ui32AccumHeight;
		pfVertices[17]	= fDepth;

		switch(gc->psDrawParams->eAccumRotationAngle)
		{
			default:
			case PVRSRV_ROTATE_0:
			case PVRSRV_FLIP_Y:
			{
				/* U, V */
				pfVertices[3]	= 0.0f;
				pfVertices[4]	= 0.0f;

				/* U, V */
				pfVertices[8]	= 1.0f;
				pfVertices[9]	= 0.0f;

				/* U, V */
				pfVertices[13]	= 0.0f;
				pfVertices[14]	= 1.0f;
		
				/* U, V */
				pfVertices[18]	= 1.0f;
				pfVertices[19]	= 1.0f;
	
				break;
			}
			case PVRSRV_ROTATE_90:
			{
				/* U, V */
				pfVertices[3]	= 1.0f;
				pfVertices[4]	= 0.0f;
			
				/* U, V */
				pfVertices[8]	= 1.0f;
				pfVertices[9]	= 1.0f;
				
				/* U, V */
				pfVertices[13]	= 0.0f;
				pfVertices[14]	= 0.0f;

				/* U, V */
				pfVertices[18]	= 0.0f;
				pfVertices[19]	= 1.0f;
			
				break;
			}
			case PVRSRV_ROTATE_180:
			{
				/* U, V */
				pfVertices[3]	= 1.0f;
				pfVertices[4]	= 1.0f;
			
				/* U, V */
				pfVertices[8]	= 0.0f;
				pfVertices[9]	= 1.0f;
				
				/* U, V */
				pfVertices[13]	= 1.0f;
				pfVertices[14]	= 0.0f;
			
				/* U, V */
				pfVertices[18]	= 0.0f;
				pfVertices[19]	= 0.0f;

				break;
			}
			case PVRSRV_ROTATE_270:
			{
				/* U, V */
				pfVertices[3]	= 0.0f;
				pfVertices[4]	= 1.0f;
			
				/* U, V */
				pfVertices[8]	= 0.0f;
				pfVertices[9]	= 0.0f;
				
				/* U, V */
				pfVertices[13]	= 1.0f;
				pfVertices[14]	= 1.0f;

				/* U, V */
				pfVertices[18]	= 1.0f;
				pfVertices[19]	= 0.0f;

				break;
			}
		}

		*pui16Indices		=3;
	}

	CBUF_UpdateBufferPos(gc->apsBuffers, ui32VertexDWords, CBUF_TYPE_VERTEX_DATA_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_VERTEX_DATA_COUNT, ui32VertexDWords);

	CBUF_UpdateBufferPos(gc->apsBuffers, ui32IndexDWords, CBUF_TYPE_INDEX_DATA_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_INDEX_DATA_COUNT, ui32IndexDWords);

	*pui32NumIndices = ui32NumIndices;

	return GLES2_NO_ERROR;
}

/***********************************************************************************
 Function Name      : SetupVerticesAndShaderForAccum
 Inputs             : gc, fDepth
 Outputs            : -
 Returns            : Mem Error
 Description        : Generates vertices and shader for accumulate
************************************************************************************/
static GLES2_MEMERROR SetupVerticesAndShaderForAccum(GLES2Context *gc, IMG_FLOAT fDepth)
{
	IMG_UINT32 *pui32Buffer, *pui32BufferBase;
	PDS_VERTEX_SHADER_PROGRAM sPDSVertexShaderProgram;
	IMG_DEV_VIRTADDR uPDSCodeAddr, uVerticesAddr, uIndexAddr;
	IMG_UINT32 ui32NumIndices, ui32NumUSEAttribsInBytes;
	GLES2_MEMERROR eError;

	/* 5 PA's used for vertices */
	ui32NumUSEAttribsInBytes = 20;

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	ui32NumUSEAttribsInBytes += (SGX_USE_MINTEMPREGS << 2);
#endif

	eError = SetupVerticesForAccum(gc, &uVerticesAddr, &uIndexAddr, &ui32NumIndices, fDepth);

	if(eError != GLES2_NO_ERROR)
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
	sPDSVertexShaderProgram.asStreams[0].ui32Stride = 20;

	sPDSVertexShaderProgram.asStreams[0].asElements[0].ui32Offset = 0;
	sPDSVertexShaderProgram.asStreams[0].asElements[0].ui32Size = 20;
	sPDSVertexShaderProgram.asStreams[0].asElements[0].ui32Register = 0;

	/* Set the USE Task control words */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sPDSVertexShaderProgram.aui32USETaskControl[0]	= SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU0_TRC_SHIFT;
	sPDSVertexShaderProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	sPDSVertexShaderProgram.aui32USETaskControl[2]	= 0;
#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */
	sPDSVertexShaderProgram.aui32USETaskControl[0]	= 0;
	sPDSVertexShaderProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL |
													  (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU1_TRC_SHIFT);
	sPDSVertexShaderProgram.aui32USETaskControl[2]	= 0;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	/* Set the execution address of the vertex copy program */
	SetUSEExecutionAddress(&sPDSVertexShaderProgram.aui32USETaskControl[0], 
							0, 
							gc->sPrim.psAccumVertexCodeBlock->sCodeAddress, 
							gc->psSysContext->uUSEVertexHeapBase, 
							SGX_VTXSHADER_USE_CODE_BASE_INDEX);

	/*
		Get buffer space for the PDS vertex program
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, EURASIA_NUM_PDS_VERTEX_PROGRAM_DWORDS, CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);
	
	if(!pui32BufferBase)
	{
		return GLES2_TA_BUFFER_ERROR;
	}

	/* Generate the PDS Program for this shader */
	pui32Buffer = PDSGenerateVertexShaderProgram(&sPDSVertexShaderProgram, pui32BufferBase, IMG_NULL);

	/* Update buffer position */
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_VERT_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_PDS_VERT_DATA_COUNT, (pui32Buffer - pui32BufferBase));

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
		return GLES2_TA_BUFFER_ERROR;
	}

	pui32Buffer = pui32BufferBase;

	/* Index List Block header */
	*pui32Buffer++ =	EURASIA_TAOBJTYPE_INDEXLIST	|
						EURASIA_VDM_IDXPRES2		|
						EURASIA_VDM_IDXPRES3		|
#if !defined(SGX545)
						EURASIA_VDM_IDXPRES45		|
#endif
						ui32NumIndices				|
						((ui32NumIndices == 3) ? EURASIA_VDM_TRIS : EURASIA_VDM_STRIP);

	/* Index List 1 */	
	*pui32Buffer++	= (uIndexAddr.uiAddr >> EURASIA_VDM_IDXBASE16_ALIGNSHIFT) << 
						EURASIA_VDM_IDXBASE16_SHIFT;

	/* Index List 2 */
	*pui32Buffer++	= ((IMG_UINT32)((EURASIA_VDMPDS_MAXINPUTINSTANCES_MAX - 1) << EURASIA_VDMPDS_MAXINPUTINSTANCES_SHIFT)) |
					  EURASIA_VDM_IDXSIZE_16BIT;

	/* Index List 3 */
	*pui32Buffer++ = ~EURASIA_VDM_WRAPCOUNT_CLRMSK;

	/* Index List 4 */
	*pui32Buffer++ =	(7 << EURASIA_VDMPDS_PARTSIZE_SHIFT) |
						(uPDSCodeAddr.uiAddr >> EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT) << 
						EURASIA_VDMPDS_BASEADDR_SHIFT;

	/* Index List 5 */
	*pui32Buffer   =	(2 << EURASIA_VDMPDS_VERTEXSIZE_SHIFT)				|
						(3 << EURASIA_VDMPDS_VTXADVANCE_SHIFT)				|
						(ALIGNCOUNTINBLOCKS(ui32NumUSEAttribsInBytes, EURASIA_VDMPDS_USEATTRIBUTESIZE_ALIGNSHIFT) <<
						EURASIA_VDMPDS_USEATTRIBUTESIZE_SHIFT)				|
						(sPDSVertexShaderProgram.ui32DataSize >> EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT)	
						<< EURASIA_VDMPDS_DATASIZE_SHIFT;

	CBUF_UpdateBufferPos(gc->apsBuffers, MAX_DWORDS_PER_INDEX_LIST_BLOCK, CBUF_TYPE_VDM_CTRL_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_VDM_CTRL_STATE_COUNT, MAX_DWORDS_PER_INDEX_LIST_BLOCK);

	/*
		Update TA buffers commited primitive offset
	*/
	CBUF_UpdateBufferCommittedPrimOffsets(gc->apsBuffers, &gc->psRenderSurface->bPrimitivesSinceLastTA, (IMG_VOID *)gc, KickLimit_ScheduleTA);
	CBUF_UpdateVIBufferCommittedPrimOffsets(gc->apsBuffers, &gc->psRenderSurface->bPrimitivesSinceLastTA, (IMG_VOID *)gc, KickLimit_ScheduleTA);

	return GLES2_NO_ERROR;
}



/***********************************************************************************
 Function Name      : SetupBGObject
 Inputs             : gc, bIsAccumulate
 Outputs            : pui32PDSState
 Returns            : Mem Error
 Description        : Generates PDS code for background object. SW Object is for
					  accumulation, HW object is for SPM.
************************************************************************************/
IMG_INTERNAL GLES2_MEMERROR SetupBGObject(GLES2Context *gc, IMG_BOOL bIsAccumulate, IMG_UINT32 *pui32PDSState)
{
	EGLDrawableParams			*psDrawParams = gc->psDrawParams;
	UCH_UseCodeBlock *psPixelSecondaryCode = gc->sPrim.psDummyPixelSecondaryPDSCode;
	PDS_PIXEL_SHADER_PROGRAM    sProgram = {0};
	IMG_UINT32                  ui32PDSProgramSize, ui32PDSDataSize, ui32PDSDataSizeDS0;
	IMG_UINT32                  ui32PDSDataSizeDS1;
	IMG_UINT32					*pui32BufferBase, *pui32Buffer;
	IMG_DEV_VIRTADDR            uPDSPixelShaderBaseAddr, uPDSPixelShaderSecondaryBaseAddr;
	PDS_TEXTURE_IMAGE_UNIT		sTextureImageUnit;
	IMG_MEMLAYOUT				eMemFormat;
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
	IMG_UINT32					ui32Stride;
#endif

	IMG_UINT32					ui32AccumWidth, ui32AccumHeight, ui32AccumStride;
	PVRSRV_PIXEL_FORMAT			eAccumPixelFormat;
	PVRSRV_ROTATION				eAccumRotationAngle;

	if(bIsAccumulate)
	{
		ui32AccumWidth = psDrawParams->ui32AccumWidth;
		ui32AccumHeight = psDrawParams->ui32AccumHeight;
		ui32AccumStride = psDrawParams->ui32AccumStride;
		eAccumPixelFormat = psDrawParams->eAccumPixelFormat;
		eAccumRotationAngle = psDrawParams->eAccumRotationAngle;
	}
	else
	{
		ui32AccumWidth = psDrawParams->ui32Width;
		ui32AccumHeight = psDrawParams->ui32Height;
		ui32AccumStride = psDrawParams->ui32Stride;
		eAccumPixelFormat = psDrawParams->ePixelFormat;
		eAccumRotationAngle = psDrawParams->eRotationAngle;
	}

	/***************************************
      A: Setup Texture State for BGObject
	***************************************/
	/* 
	   Setup texture control block word --- filtering and address mode 
	*/
	sTextureImageUnit.ui32TAGControlWord0 = EURASIA_PDS_DOUTT0_NOTMIPMAP		|
											EURASIA_PDS_DOUTT0_ANISOCTL_NONE	|
											EURASIA_PDS_DOUTT0_MINFILTER_POINT	|
											EURASIA_PDS_DOUTT0_MAGFILTER_POINT	|
											EURASIA_PDS_DOUTT0_UADDRMODE_CLAMP	|
											EURASIA_PDS_DOUTT0_VADDRMODE_CLAMP  ;
	
	/* 
	   Setup texture control block word --- size and format 
	*/	
	sTextureImageUnit.ui32TAGControlWord1 = asSGXPixelFormat[eAccumPixelFormat].aui32TAGControlWords[0][1];
		
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
	sTextureImageUnit.ui32TAGControlWord3 = asSGXPixelFormat[eAccumPixelFormat].aui32TAGControlWords[0][3];
#endif /* (EURASIA_TAG_TEXTURE_STATE_SIZE == 4) */

	eMemFormat = GetColorAttachmentMemFormat(gc, gc->sFrameBuffer.psActiveFrameBuffer);

	switch(eMemFormat)
	{
		case IMG_MEMLAYOUT_STRIDED:
		{
			switch(eAccumRotationAngle)
			{
				default:
				case PVRSRV_ROTATE_0:
				case PVRSRV_FLIP_Y:
				case PVRSRV_ROTATE_180:
				{
#if defined(SGX545)
					sTextureImageUnit.ui32TAGControlWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE                                  |
															(((ui32AccumWidth  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT ) &	~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  |
															(((ui32AccumHeight - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;
#else
					sTextureImageUnit.ui32TAGControlWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE                                  |
															((ui32AccumWidth  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
															((ui32AccumHeight - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) ;

#endif
					break;
				}
				case PVRSRV_ROTATE_90:
				case PVRSRV_ROTATE_270:
				{
#if defined(SGX545)
					sTextureImageUnit.ui32TAGControlWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE                                  |
															(((ui32AccumHeight  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT ) &	~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  |
															(((ui32AccumWidth - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;
#else
					sTextureImageUnit.ui32TAGControlWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE                                  |
															((ui32AccumHeight  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
															((ui32AccumWidth - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) ;
#endif
					break;
				}
			}

#if defined(SGX545)
			sTextureImageUnit.ui32TAGControlWord3 |= ((1) << EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT) & ~EURASIA_PDS_DOUTT3_TAGDATASIZE_CLRMSK;
#endif

#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
			ui32Stride = ui32AccumStride;

			/* Must be a multiple of 4 bytes */
			GLES_ASSERT(!(ui32Stride & 3));

			/* Stride in 32 bit units */
			ui32Stride >>= 2;

			/* Stride is specified as 0 == 1 */
			ui32Stride -= 1;

#if defined(EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT)
			sTextureImageUnit.ui32TAGControlWord1 &= EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK;
			sTextureImageUnit.ui32TAGControlWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT;

			sTextureImageUnit.ui32TAGControlWord0 &= (EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK);

			sTextureImageUnit.ui32TAGControlWord0 |= 
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDELO_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK) |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK);

#if defined(EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK)
			sTextureImageUnit.ui32TAGControlWord0 &= (EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK);
	
			sTextureImageUnit.ui32TAGControlWord0 |= 
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEEX_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEEX_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK);
#endif



#else /* EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT */

#if defined(SGX_FEATURE_VOLUME_TEXTURES)
			sTextureImageUnit.ui32TAGControlWord0 &= (EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK);

			sTextureImageUnit.ui32TAGControlWord0 |= EURASIA_PDS_DOUTT0_STRIDE |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDELO_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK) |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK);
		
#else /* SGX_FEATURE_VOLUME_TEXTURES */
			sTextureImageUnit.ui32TAGControlWord0 &= (EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & EURASIA_PDS_DOUTT0_STRIDELO2_CLRMSK);

#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
			sTextureImageUnit.ui32TAGControlWord0 &= EURASIA_PDS_DOUTT0_STRIDEEX0_CLRMSK;
#endif

			sTextureImageUnit.ui32TAGControlWord0 |= EURASIA_PDS_DOUTT0_STRIDE |
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEEX0_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEEX0_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEEX0_CLRMSK) |
#endif
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO2_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDELO2_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO2_CLRMSK) |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK);
		
			sTextureImageUnit.ui32TAGControlWord1 &= EURASIA_PDS_DOUTT1_STRIDELO1_CLRMSK;

#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
			sTextureImageUnit.ui32TAGControlWord1 &= EURASIA_PDS_DOUTT1_STRIDEEX1_CLRMSK;
#endif

			sTextureImageUnit.ui32TAGControlWord1 |= 
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
				(((ui32Stride >> EURASIA_PDS_DOUTT1_STRIDEEX1_STRIDE_SHIFT) << EURASIA_PDS_DOUTT1_STRIDEEX1_SHIFT) & ~EURASIA_PDS_DOUTT1_STRIDEEX1_CLRMSK) |
#endif
				(((ui32Stride >> EURASIA_PDS_DOUTT1_STRIDELO1_STRIDE_SHIFT) << EURASIA_PDS_DOUTT1_STRIDELO1_SHIFT) & ~EURASIA_PDS_DOUTT1_STRIDELO1_CLRMSK);
#endif /* SGX_FEATURE_VOLUME_TEXTURES */
#endif /* defined(EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT) */ 
#endif /* SGX_FEATURE_TEXTURESTRIDE_EXTENSION */

			break;
		}
		case IMG_MEMLAYOUT_TILED:
		{
			sTextureImageUnit.ui32TAGControlWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_TILED									|
													((ui32AccumWidth  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
													((ui32AccumHeight - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) ;
			break;
		}
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
		case IMG_MEMLAYOUT_HYBRIDTWIDDLED:
		{
			sTextureImageUnit.ui32TAGControlWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_2D                                      |
			                                         (((ui32AccumWidth  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  |
			                                         (((ui32AccumHeight - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;

			sTextureImageUnit.ui32TAGControlWord3 = (1 << EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT);
			break;
		}
#else
		case IMG_MEMLAYOUT_TWIDDLED:
		{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
			sTextureImageUnit.ui32TAGControlWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_2D                                           |
													(FloorLog2(ui32AccumWidth)  << EURASIA_PDS_DOUTT1_USIZE_SHIFT) |
													(FloorLog2(ui32AccumHeight) << EURASIA_PDS_DOUTT1_VSIZE_SHIFT) ;
#else
			sTextureImageUnit.ui32TAGControlWord1 |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_2D                                      |
			                                         (((ui32AccumWidth  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  |
			                                         (((ui32AccumHeight - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;
#endif
			break;
		}
#endif /* defined(SGX545) */
		default:
			GLES_ASSERT(eMemFormat != eMemFormat);
			break;
	}

	/* 
	   Setup texture control block word --- texture address 
	*/
	if(bIsAccumulate)
	{
		sTextureImageUnit.ui32TAGControlWord2 = 
			(psDrawParams->ui32AccumHWAddress >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
			EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;
	}
	else
	{
		sTextureImageUnit.ui32TAGControlWord2 = 
			(psDrawParams->ui32HWSurfaceAddress >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
			EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;
	}

	/*******************************************
      B: Generate PDS Program for HW BGObject
	*******************************************/
	/*
		Setup USE Task Control Words
	*/
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sProgram.aui32USETaskControl[0] = 0;
	sProgram.aui32USETaskControl[1] = EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY   |
	                                  EURASIA_PDS_DOUTU1_MODE_PARALLEL       |
	                                  EURASIA_PDS_DOUTU1_SDSOFT              ;
	sProgram.aui32USETaskControl[2] = 0;
#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */
	sProgram.aui32USETaskControl[0]	= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
	sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	sProgram.aui32USETaskControl[2]	= EURASIA_PDS_DOUTU2_SDSOFT;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	/* Set the execution address of the clear fragment shader */
	SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
							0, 
							gc->sPrim.psHWBGCodeBlock->sCodeAddress, 
							gc->psSysContext->uUSEFragmentHeapBase, 
							SGX_PIXSHADER_USE_CODE_BASE_INDEX);

	/*
		Setup FPU Iterator Control Words & 
		Calculate Size of PDS Pixel Program.
	*/
#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
	ui32PDSProgramSize = 6;
	ui32PDSDataSize = 16;

#if(EURASIA_PDS_DOUTI_STATE_SIZE == 2)
	ui32PDSProgramSize += 1;
	ui32PDSDataSize += 1;
#endif


#else
	ui32PDSProgramSize = 4;
	ui32PDSDataSize = 7;
#endif

	sProgram.ui32NumFPUIterators = 1;

#if defined(SGX545)
	sProgram.aui32FPUIterators0[0] = 
#else /* defined(SGX545) */
	sProgram.aui32FPUIterators[0]  =
#endif /* defined(SGX545) */
	                                 EURASIA_PDS_DOUTI_USEISSUE_NONE	|
									 EURASIA_PDS_DOUTI_TEXISSUE_TC0		|
									 EURASIA_PDS_DOUTI_TEXLASTISSUE;
	
	sProgram.aui32TAGLayers[0]	= 0;
	
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	sProgram.aui8FormatConv[0] = 0;
	sProgram.aui8LayerSize[0] = 0;
#endif

	ui32PDSDataSizeDS0 = (ui32PDSDataSize / 4) * 2 + MIN((ui32PDSDataSize % 4), 2); /* PRQA S 3356 */ /* Use this expression to keep code flexible as ui32PDSDataSize may have different value in the future. */
	ui32PDSDataSizeDS1 = ui32PDSDataSize - ui32PDSDataSizeDS0;

	/* PRQA S 3358 1 */ /* Use this expression to keep code flexible as ui32PDSDataSizeDS1 may have different value if ui32PDSDataSize changes. */
	if (ui32PDSDataSizeDS1 & 7)
	{
		ui32PDSDataSize = ((ui32PDSDataSizeDS0 + 7) & ~7U) + ui32PDSDataSizeDS1;
	}

	ui32PDSProgramSize += (ui32PDSDataSize + ((1U << (EURASIA_PDS_DATASIZE_ALIGNSHIFT - 2)) - 1)) 
	                                      & ~((1U << (EURASIA_PDS_DATASIZE_ALIGNSHIFT - 2)) - 1);

	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, ui32PDSProgramSize, CBUF_TYPE_PDS_FRAG_BUFFER, IMG_FALSE);
	
	if(!pui32BufferBase)
	{
		return GLES2_3D_BUFFER_ERROR;
	}

	/* Save the PDS pixel shader data segment address and size (always offset by PDS exec base) */
	uPDSPixelShaderBaseAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_PDS_FRAG_BUFFER);
	
#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
	uPDSPixelShaderBaseAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

	/*
		Generate PDS Pixel State Program.
	*/
	pui32Buffer = PDSGeneratePixelShaderProgram(&sTextureImageUnit,
												&sProgram,
												pui32BufferBase);

	/* 
	   Update Buffer Position 
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_FRAG_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_PDS_FRAG_DATA_COUNT, (pui32Buffer - pui32BufferBase));
	
	GLES_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= ui32PDSProgramSize);

	/* Get the PDS pixel shader secondary data segment address and size (always offset by PDS exec base) */
	uPDSPixelShaderSecondaryBaseAddr = psPixelSecondaryCode->sCodeAddress;

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
	uPDSPixelShaderSecondaryBaseAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

	GLES_ASSERT(!(uPDSPixelShaderSecondaryBaseAddr.uiAddr & ((1 << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)));

	/*************************************************
      C: Store HW BGObject state for SGX Kick Later
	*************************************************/
	/*
	  Write PDS Pixel Shader Secondary Attribute Base Address and Data Size
	*/
	pui32PDSState[0] = (((uPDSPixelShaderSecondaryBaseAddr.uiAddr >> EURASIA_PDS_BASEADD_ALIGNSHIFT)
						<< EURASIA_PDS_BASEADD_SHIFT) & ~EURASIA_PDS_BASEADD_CLRMSK)					|
						(((gc->sPrim.ui32DummyPDSPixelSecondaryDataSize >> EURASIA_PDS_DATASIZE_ALIGNSHIFT) 
						<< EURASIA_PDS_DATASIZE_SHIFT) & ~EURASIA_PDS_DATASIZE_CLRMSK);
	/*
	  Write DMS Pixel Control Information
	*/
	pui32PDSState[1] =	(0 << EURASIA_PDS_SECATTRSIZE_SHIFT)	|
						(3 << EURASIA_PDS_USETASKSIZE_SHIFT)	|
#if defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
						((uPDSPixelShaderSecondaryBaseAddr.uiAddr & EURASIA_PDS_BASEADD_MSB) ? EURASIA_PDS_BASEADD_SEC_MSB : 0) |
						((uPDSPixelShaderBaseAddr.uiAddr & EURASIA_PDS_BASEADD_MSB) ? EURASIA_PDS_BASEADD_PRIM_MSB : 0) |
#endif
						((EURASIA_PDS_PDSTASKSIZE_MAX << EURASIA_PDS_PDSTASKSIZE_SHIFT)	& 
						~EURASIA_PDS_PDSTASKSIZE_CLRMSK)		|
						(1 << EURASIA_PDS_PIXELSIZE_SHIFT);

	/*
	  Write PDS Pixel Shader Base Address and Data Size
	*/
	pui32PDSState[2] = 	(((uPDSPixelShaderBaseAddr.uiAddr >> EURASIA_PDS_BASEADD_ALIGNSHIFT) 
						<< EURASIA_PDS_BASEADD_SHIFT) & ~EURASIA_PDS_BASEADD_CLRMSK)					|
						(((sProgram.ui32DataSize >> EURASIA_PDS_DATASIZE_ALIGNSHIFT) 
						<< EURASIA_PDS_DATASIZE_SHIFT) & ~EURASIA_PDS_DATASIZE_CLRMSK);

	return GLES2_NO_ERROR;
}


/***********************************************************************************
 Function Name      : SendAccumulateObject
 Inputs             : gc, bClearDepth, fDepth
 Outputs            : -
 Returns            : Mem Error
 Description        : Sends an accumulate object
************************************************************************************/
IMG_INTERNAL GLES2_MEMERROR SendAccumulateObject(GLES2Context *gc, IMG_BOOL bClearDepth, IMG_FLOAT fDepth)
{
	IMG_UINT32 ui32StateHeader;
	IMG_UINT32 ui32ISPControlA;
	IMG_DEV_VIRTADDR uStateAddr;
	IMG_UINT32 *pui32Buffer, *pui32BufferBase, ui32NumStateDWords;
	IMG_UINT32 aui32PDSState[3];
	GLES2_MEMERROR eError;

	/* Always send ISPA, output selects (including texsize word)
	   and MTE control - rest can take default/previous state 
	*/
	ui32StateHeader =	EURASIA_TACTLPRES_ISPCTLFF0		|
						EURASIA_TACTLPRES_OUTSELECTS	|
						EURASIA_TACTLPRES_MTECTRL		|
						EURASIA_TACTLPRES_TEXSIZE		|
						EURASIA_TACTLPRES_TEXFLOAT		|
						EURASIA_TACTLPRES_PDSSTATEPTR;

	ui32ISPControlA =	EURASIA_ISPA_PASSTYPE_OPAQUE	|
						EURASIA_ISPA_DCMPMODE_ALWAYS	|
	                    EURASIA_ISPA_OBJTYPE_TRI		;

	if(!bClearDepth)
	{
		ui32ISPControlA |= EURASIA_ISPA_DWRITEDIS;
	}

	ui32NumStateDWords = 9;

	/* Setup Accumulation shader */
	eError = SetupBGObject(gc, IMG_TRUE, aui32PDSState);

	if(eError != GLES2_NO_ERROR)
	{
		return eError;
	}

	/* May need to send region clip word dirtied from last scene */
	if(gc->ui32EmitMask & GLES2_EMITSTATE_MTE_STATE_REGION_CLIP)
	{
		ui32StateHeader |= EURASIA_TACTLPRES_RGNCLIP;
		ui32NumStateDWords += 2;

		gc->ui32EmitMask &= ~GLES2_EMITSTATE_MTE_STATE_REGION_CLIP;
	}


	/*
		Get buffer space for the actual output state
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers,
									ui32NumStateDWords,
									CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES2_TA_BUFFER_ERROR;
	}

	pui32Buffer = pui32BufferBase;

	*pui32Buffer++ = ui32StateHeader;

	*pui32Buffer++ = ui32ISPControlA;

	*pui32Buffer++ = aui32PDSState[0];
	*pui32Buffer++ = aui32PDSState[1];
	*pui32Buffer++ = aui32PDSState[2];

	if(ui32StateHeader & EURASIA_TACTLPRES_RGNCLIP)
	{
		*pui32Buffer++	= gc->psRenderSurface->aui32RegionClipWord[0];
		*pui32Buffer++	= gc->psRenderSurface->aui32RegionClipWord[1];
	}


	/* ========= */
	/* Output Selects */
	/* ========= */
	*pui32Buffer++ =	EURASIA_MTE_WPRESENT	|
						(6 << EURASIA_MTE_VTXSIZE_SHIFT);


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
	*pui32Buffer++	= 
#if defined(SGX545)
	                  EURASIA_MTE_ATTRDIM_UV << (EURASIA_MTE_ATTRDIM0_SHIFT + ((1-0) * 3)); /* Layer 1 */
#else /* defined(SGX545) */
	                  EURASIA_MTE_TEXDIM_UV << EURASIA_MTE_TEXDIM0_SHIFT;
#endif /* defined(SGX545) */ 

	
	/* ========= */
	/*	Texture coordinate float (Use 16 bit float) */
	/* ========= */
	*pui32Buffer++ = EURASIA_TATEXFLOAT_TC0_16B;	/* PRQA S 3199 */ /* pui32Buffer is used in the following GLES_ASSERT. */

	GLES_ASSERT(ui32NumStateDWords == (IMG_UINT32)(pui32Buffer - pui32BufferBase));

	CBUF_UpdateBufferPos(gc->apsBuffers, ui32NumStateDWords, CBUF_TYPE_PDS_VERT_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_PDS_VERT_DATA_COUNT, ui32NumStateDWords);

	GLES2_INC_COUNT(GLES2_TIMER_MTE_STATE_COUNT, ui32NumStateDWords);

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
	
	if(eError != GLES2_NO_ERROR)
	{
		return eError;
	}

	eError = SetupStateUpdate(gc, IMG_TRUE);

	if(eError != GLES2_NO_ERROR)
	{
		return eError;
	}

	eError = SetupVerticesAndShaderForAccum(gc, fDepth);

	if(eError != GLES2_NO_ERROR)
	{
		return eError;
	}	

	gc->ui32EmitMask |=	GLES2_EMITSTATE_MTE_STATE_ISP |
						GLES2_EMITSTATE_MTE_STATE_CONTROL |
						GLES2_EMITSTATE_MTE_STATE_OUTPUTSELECTS |
						GLES2_EMITSTATE_PDS_FRAGMENT_STATE | 
						GLES2_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE |
						GLES2_EMITSTATE_STATEUPDATE;

	return GLES2_NO_ERROR;
}

/******************************************************************************
 End of file (accum.c)
******************************************************************************/

