/******************************************************************************
 * Name         : validate.c
 *
 * Copyright    : 2005-2010 by Imagination Technologies Limited.
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
 * $Log: validate.c $
 * 
 *  --- Revision Logs Removed --- 
 * SA)
 *  --- Revision Logs Removed --- 
 *
 *****************************************************************************/

#include "context.h"
#include "sgxpdsdefs.h"
#include "gles2errata.h"
#include "pixevent.h"
#include "pixeventpbesetup.h"
#include "dmscalc.h"

/* generated PDS programs */
#include "pds_mte_state_copy.h"
#include "pds_mte_state_copy_sizeof.h"

#if defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690)

#if SGX_FEATURE_NUM_USE_PIPES == 1
	static const IMG_UINT32 aui32USEPipe[] = {	EURASIA_TAPDSSTATE_USEPIPE_1};

#elif SGX_FEATURE_NUM_USE_PIPES == 2
	static const IMG_UINT32 aui32USEPipe[] = {	EURASIA_TAPDSSTATE_USEPIPE_1,
												EURASIA_TAPDSSTATE_USEPIPE_2};

#elif SGX_FEATURE_NUM_USE_PIPES == 4
	static const IMG_UINT32 aui32USEPipe[] = {	EURASIA_TAPDSSTATE_USEPIPE_1,
												EURASIA_TAPDSSTATE_USEPIPE_2,
												EURASIA_TAPDSSTATE_USEPIPE_3,
												EURASIA_TAPDSSTATE_USEPIPE_4};
#else
#error "Unknown number of pipes"
#endif

#endif /* FIX_HW_BRN_23687 || FIX_HW_BRN_23687 */


/* Maps GLES2 primitive types to ISP primitive types */
static const IMG_UINT32 aui32GLES2PrimToISPPrim[GLES2_PRIMTYPE_MAX] = {
																		EURASIA_ISPA_OBJTYPE_SPRITEUV,
																		EURASIA_ISPA_OBJTYPE_LINE,
																		EURASIA_ISPA_OBJTYPE_LINE,
																		EURASIA_ISPA_OBJTYPE_LINE,
																		EURASIA_ISPA_OBJTYPE_TRI,
																		EURASIA_ISPA_OBJTYPE_TRI,
																		EURASIA_ISPA_OBJTYPE_TRI,
																		EURASIA_ISPA_OBJTYPE_SPRITE01UV};
/* Maps GLES2 primitive types to VDM primitive types */
static const IMG_UINT32 aui32GLES2PrimToVDMPrim[GLES2_PRIMTYPE_MAX] = {
																		EURASIA_VDM_POINTS,
																		EURASIA_VDM_LINES,
																		EURASIA_VDM_LINES,
																		EURASIA_VDM_LINES,
																		EURASIA_VDM_TRIS,
																		EURASIA_VDM_STRIP,
																		EURASIA_VDM_FAN,
																		EURASIA_VDM_POINTS};

/* Mapping from pixel types (PVRSRV_PIXEL_FORMAT) to line stride's granularity shift */
static const IMG_UINT32 aui32PixelTypeToLineStrideGranShift[] =
{
	0,								/* 0  PVRSRV_PIXEL_FORMAT_UNKNOWN */
	1,								/* 1  PVRSRV_PIXEL_FORMAT_RGB565 */
	0,								/* 2  PVRSRV_PIXEL_FORMAT_RGB555 */
	0,								/* 3  PVRSRV_PIXEL_FORMAT_RGB888 */
	0,								/* 4  PVRSRV_PIXEL_FORMAT_BGR888 */
	0,								/* 5  Unused */
	0,								/* 6  Unused */
	0,								/* 7  Unused */
	0,								/* 8  PVRSRV_PIXEL_FORMAT_GREY_SCALE */
	0,								/* 9  Unused */
	0,								/* 10 Unused */
	0,								/* 11 Unused */
	0,								/* 12 Unused */
	0,								/* 13 PVRSRV_PIXEL_FORMAT_PAL12 */
	0,								/* 14 PVRSRV_PIXEL_FORMAT_PAL8 */
	0,								/* 15 PVRSRV_PIXEL_FORMAT_PAL4 */
	0,								/* 16 PVRSRV_PIXEL_FORMAT_PAL2 */
	0,								/* 17 PVRSRV_PIXEL_FORMAT_PAL1 */
	1,								/* 18 PVRSRV_PIXEL_FORMAT_ARGB1555 */
	1,								/* 19 PVRSRV_PIXEL_FORMAT_ARGB4444 */
	2,								/* 20 PVRSRV_PIXEL_FORMAT_ARGB8888 */
	2,								/* 21 PVRSRV_PIXEL_FORMAT_ABGR8888 */
	0,								/* 22 PVRSRV_PIXEL_FORMAT_YV12 */
	0,								/* 23 PVRSRV_PIXEL_FORMAT_I420 */
	0,								/* 24 Unused */
	0,								/* 25 PVRSRV_PIXEL_FORMAT_IMC2 */
	2,								/* 26 PVRSRV_PIXEL_FORMAT_XRGB8888 */
	2								/* 27 PVRSRV_PIXEL_FORMAT_XBGR8888 */
};


#if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX)


/*
	Initialize a table for an entry for each burst size giving the largest 
	factor less than or equal to the maximum line size (if one exists).
	The look-up table is generated from the following code:

	void main(void)
	{
		unsigned char abyFactorTable[EURASIA_PDS_DOUTD1_MAXBURST + 1];
		unsigned int i, j;

		memset(abyFactorTable, 0xff, sizeof (abyFactorTable));

		for (i = 1; i <= EURASIA_PDS_DOUTD1_MAXBURST; i++)
		{
			abyFactorTable[i] = EURASIA_PDS_DOUTD1_BSIZE_MAX;
			for (j = EURASIA_PDS_DOUTD1_BSIZE_MAX; j > 0; j--)
			{
				if ((i % j) == 0 && (i / j) <= EURASIA_PDS_DOUTD1_BLINES_MAX)
				{
					abyFactorTable[i - 1] = (unsigned char)j;

					break;
				}
			}
		}

		for (i = 0; i < EURASIA_PDS_DOUTD1_MAXBURST  ; i+=16)
		{
			for (j = 0; j < EURASIA_PDS_DOUTD1_MAXBURST /16 ; j++)
			{
				fprintf(stdout, "%2d, ", abyFactorTable[i + j]);
			}
			fprintf(stdout, "\n");
		}
	}

}*/

static const IMG_BYTE g_abyFactorTable[] = 
{
	 1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16,
	16,  9, 16, 10,  7, 11, 16, 12,  5, 13,  9, 14, 16, 15, 16, 16,
	11, 16,  7, 12, 16, 16, 13, 10, 16, 14, 16, 11, 15, 16, 16, 16,
	 7, 10, 16, 13, 16,  9, 11, 14, 16, 16, 16, 15, 16, 16,  9, 16,
	13, 11, 16, 16, 16, 14, 16, 12, 16, 16, 15, 16, 11, 13, 16, 16,
	 9, 16, 16, 14, 16, 16, 16, 11, 16, 15, 13, 16, 16, 16, 16, 16,
	16, 14, 11, 10, 16, 16, 16, 13, 15, 16, 16, 12, 16, 11, 16, 16,
	16, 16, 16, 16, 13, 16, 16, 15, 11, 16, 16, 16, 16, 14, 16, 16,
	16, 13, 16, 12, 16, 16, 15, 16, 16, 16, 16, 14, 16, 16, 13, 16,
	16, 16, 16, 16, 16, 15, 16, 16, 16, 14, 16, 13, 16, 16, 16, 16,
	16, 16, 16, 16, 15, 16, 16, 14, 13, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 15, 16, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 15, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
	16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16
};

/* Table will be incorrect if these values change */
#if (EURASIA_PDS_DOUTD1_MAXBURST != 256)
#error ("EURASIA_PDS_DOUTD1_MAXBURST changed!")
#endif

#if (EURASIA_PDS_DOUTD1_BSIZE_MAX != 16)
#error ("EURASIA_PDS_DOUTD1_BSIZE_MAX changed!")
#endif

#if (EURASIA_PDS_DOUTD1_BLINES_MAX != 16)
#error ("EURASIA_PDS_DOUTD1_BLINES_MAX changed!")
#endif

#endif /* #if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX) */

/*********************************************************************************
 Function		:	EncodeDmaBurst

 Description	:	Encode a DMA burst.

 Parameters		:	pui32DMAControl		- DMA control words
					ui32DestOffset		- Destination offset in the attribute buffer.
					ui32DMASize			- Size of the dma in dwords.
					ui32Address			- Source address for the burst.

 Return			:	The number of DMA transfers required.
*********************************************************************************/
IMG_INTERNAL IMG_UINT32 EncodeDmaBurst(IMG_UINT32 *pui32DMAControl,
									   IMG_UINT32 ui32DestOffset,
									   IMG_UINT32 ui32DMASize,
									   IMG_DEV_VIRTADDR uSrcAddress)
{
	IMG_UINT32 ui32BurstSize, ui32NumKicks = 0;
#if (EURASIA_PDS_DOUTD1_MAXBURST != EURASIA_PDS_DOUTD1_BSIZE_MAX)
	IMG_UINT32 ui32Factor, ui32Lines;
#endif

	GLES_ASSERT(ui32DMASize > 0);

#if (EURASIA_PDS_DOUTD1_MAXBURST == EURASIA_PDS_DOUTD1_BSIZE_MAX)

	GLES_ASSERT(ui32DMASize <= (EURASIA_PDS_DOUTD1_MAXBURST * PDS_NUM_DMA_KICKS));

#else
	GLES_ASSERT(ui32DMASize <= (EURASIA_PDS_DOUTD1_MAXBURST * (PDS_NUM_DMA_KICKS - 1)));

	#if defined(DEBUG)
	{
		static IMG_BOOL bFactorTableChecked = IMG_FALSE;

		/*
			Validate the table.
		*/
		if (!bFactorTableChecked)
		{
			IMG_BYTE abyFactorTable[EURASIA_PDS_DOUTD1_MAXBURST + 1];

			IMG_UINT32 i, j;

			for (i = 1; i <= EURASIA_PDS_DOUTD1_MAXBURST; i++)
			{
				abyFactorTable[i] = EURASIA_PDS_DOUTD1_BSIZE_MAX;
				for (j = EURASIA_PDS_DOUTD1_BSIZE_MAX; j > 0; j--)
				{
					if ((i % j) == 0 && (i / j) <= EURASIA_PDS_DOUTD1_BLINES_MAX)
					{						
						abyFactorTable[i - 1] = (IMG_BYTE)j;

						break;
					}
				}
			}

			for (i = 0; i < EURASIA_PDS_DOUTD1_MAXBURST; i++)
			{
				GLES_ASSERT(g_abyFactorTable[i] == abyFactorTable[i]);
			}
			
			bFactorTableChecked = IMG_TRUE;
		}
	}
	#endif /* DEBUG */
#endif

	do
	{
#if (EURASIA_PDS_DOUTD1_MAXBURST == EURASIA_PDS_DOUTD1_BSIZE_MAX)
		if(ui32DMASize > EURASIA_PDS_DOUTD1_MAXBURST)
		{
			ui32BurstSize = EURASIA_PDS_DOUTD1_MAXBURST;
		}
		else
		{
			ui32BurstSize = ui32DMASize;
		}

		pui32DMAControl[ui32NumKicks * 2] = (uSrcAddress.uiAddr << EURASIA_PDS_DOUTD0_SBASE_SHIFT);
		pui32DMAControl[ui32NumKicks * 2 + 1] = ((ui32BurstSize - 1)	<< EURASIA_PDS_DOUTD1_BSIZE_SHIFT)  |
												(ui32DestOffset	<< EURASIA_PDS_DOUTD1_AO_SHIFT);

		ui32DestOffset += ui32BurstSize;
		uSrcAddress.uiAddr += ui32BurstSize * sizeof(IMG_UINT32);

#else
		if(ui32DMASize > EURASIA_PDS_DOUTD1_MAXBURST)
		{
			ui32Lines = EURASIA_PDS_DOUTD1_BLINES_MAX;
			ui32Factor = EURASIA_PDS_DOUTD1_BSIZE_MAX;
			ui32BurstSize = EURASIA_PDS_DOUTD1_MAXBURST;
		}
		else
		{
			ui32Factor = g_abyFactorTable[ui32DMASize - 1];
			ui32Lines = ui32DMASize / ui32Factor;
			ui32BurstSize = ui32Lines * ui32Factor;
		}

		pui32DMAControl[ui32NumKicks * 2] = (uSrcAddress.uiAddr << EURASIA_PDS_DOUTD0_SBASE_SHIFT);
		pui32DMAControl[ui32NumKicks * 2 + 1] = ((ui32Factor - 1)	<< EURASIA_PDS_DOUTD1_BSIZE_SHIFT)  |
						 ((ui32Lines - 1)	<< EURASIA_PDS_DOUTD1_BLINES_SHIFT) |
						 ((ui32Factor - 1)	<< EURASIA_PDS_DOUTD1_STRIDE_SHIFT) |
						 (ui32DestOffset	<< EURASIA_PDS_DOUTD1_AO_SHIFT);

		ui32DestOffset += ui32Lines * ui32Factor;
		uSrcAddress.uiAddr += ui32Lines * ui32Factor * sizeof(IMG_UINT32);
#endif

		ui32DMASize -= ui32BurstSize;
		ui32NumKicks++;

	}while(ui32DMASize && (ui32NumKicks <= PDS_NUM_DMA_KICKS));

	return ui32NumKicks;
}


/***********************************************************************************
 Function Name      : OutputTerminateState
 Inputs             : gc, psRenderSurface, ui32KickFlags
 Outputs            : 
 Returns            : Success
 Description        : 
************************************************************************************/
IMG_INTERNAL GLES2_MEMERROR OutputTerminateState(GLES2Context *gc,
										   EGLRenderSurface *psRenderSurface,
										   IMG_UINT32 ui32KickFlags)
{
	EGLTerminateState *psTerminateState = &psRenderSurface->sTerm;
	PDS_TERMINATE_STATE_PROGRAM sProgram;
	IMG_UINT32 *pui32Buffer, *pui32BufferBase;
	IMG_DEV_VIRTADDR uPDSCodeAddress, uPDSSAUpdateCodeAddress;
	IMG_UINT32 ui32PDSDataSize, ui32SAUpdatePDSDataSize, ui32USEAttribSize = 2;
	IMG_UINT32 ui32StateWord0, ui32StateWord1;

	/* No terminate required for a scene discard */
	if(ui32KickFlags & GLES2_SCHEDULE_HW_DISCARD_SCENE)
		return GLES2_NO_ERROR;

	sProgram.ui32TerminateRegion = psRenderSurface->ui32TerminateRegion;

	if(!psTerminateState->ui32PDSDataSize)
	{
		IMG_UINT32 ui32Offset;
		PDS_PIXEL_SHADER_SA_PROGRAM sSAProgram = {0};
	
		/* Ensure previous TA has finished */
		WaitForTA(gc);

		pui32Buffer = pui32BufferBase = (IMG_UINT32 *)psTerminateState->psSAUpdatePDSMemInfo->pvLinAddr;

		/*
			Setup the parameters for the PDS program.
		*/
		sSAProgram.aui32USETaskControl[0]	= 0;
		sSAProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
		sSAProgram.aui32USETaskControl[2]	= 0;
		sSAProgram.bKickUSEDummyProgram		= IMG_TRUE;

		SetUSEExecutionAddress(&sSAProgram.aui32USETaskControl[0], 
								0, 
								gc->sProgram.psDummyVertUSECode->sDevVAddr, 
								gc->psSysContext->uUSEVertexHeapBase, 
								SGX_VTXSHADER_USE_CODE_BASE_INDEX);


		/* Dummy vertex secondary program */
		pui32Buffer = PDSGeneratePixelShaderSAProgram(&sSAProgram, pui32BufferBase);

		GLES_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= PDS_TERMINATE_SAUPDATE_DWORDS);

		pui32Buffer = pui32BufferBase = (IMG_UINT32 *)psTerminateState->psTerminatePDSMemInfo->pvLinAddr;
	
		/*
			Setup the parameters for the PDS program.
		*/
		sProgram.aui32USETaskControl[0]	= 0;
		sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
		sProgram.aui32USETaskControl[2]	= 0;

		SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
								0, 
								psTerminateState->psTerminateUSEMemInfo->sDevVAddr, 
								gc->psSysContext->uUSEVertexHeapBase, 
								SGX_VTXSHADER_USE_CODE_BASE_INDEX);

		PDSGenerateTerminateStateProgram(&sProgram, pui32Buffer);
	
		ui32Offset = (IMG_UINT32)((IMG_UINT8 *)sProgram.pui32DataSegment - (IMG_UINT8 *)pui32BufferBase);

		uPDSCodeAddress.uiAddr = psTerminateState->psTerminatePDSMemInfo->sDevVAddr.uiAddr + ui32Offset;
		uPDSSAUpdateCodeAddress = psTerminateState->psSAUpdatePDSMemInfo->sDevVAddr;

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
		/* Adjust PDS execution address for restricted address range */
		uPDSCodeAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
		uPDSSAUpdateCodeAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

		ui32PDSDataSize = sProgram.ui32DataSize;
		ui32SAUpdatePDSDataSize = sSAProgram.ui32DataSize;

		psTerminateState->ui32TerminateRegion = psRenderSurface->ui32TerminateRegion;
		psTerminateState->uPDSCodeAddress = uPDSCodeAddress;
		psTerminateState->uSAUpdateCodeAddress = uPDSSAUpdateCodeAddress;
		psTerminateState->ui32PDSDataSize = ui32PDSDataSize;
		psTerminateState->ui32SAUpdatePDSDataSize = ui32SAUpdatePDSDataSize;
	}
	else
	{
		ui32PDSDataSize = psTerminateState->ui32PDSDataSize;
		ui32SAUpdatePDSDataSize = psTerminateState->ui32SAUpdatePDSDataSize;
		uPDSCodeAddress = psTerminateState->uPDSCodeAddress;
		uPDSSAUpdateCodeAddress = psTerminateState->uSAUpdateCodeAddress;
	}

	/* Only need to patch if we are kicking the last TA and the region has changed */
	if(((ui32KickFlags & GLES2_SCHEDULE_HW_LAST_IN_SCENE) != 0) && 
		(psRenderSurface->ui32TerminateRegion != psRenderSurface->sTerm.ui32TerminateRegion))
	{
		pui32BufferBase = (IMG_UINT32 *)psTerminateState->psTerminatePDSMemInfo->pvLinAddr;
		
		/* Ensure previous TA has finished */
		WaitForTA(gc);

		PDSPatchTerminateStateProgram(&sProgram, pui32BufferBase);

		psTerminateState->ui32TerminateRegion = psRenderSurface->ui32TerminateRegion;
	}

	/*
		Get VDM control stream space for the PDS vertex state block
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers,
										VDM_CTRL_TERMINATE_DWORDS,
										CBUF_TYPE_VDM_CTRL_BUFFER, IMG_TRUE);

	pui32Buffer = pui32BufferBase;

	if(!pui32Buffer)
	{
		return GLES2_TA_BUFFER_ERROR;
	}

	/*
		Write the vertex secondary state update PDS data block
	*/
	ui32StateWord0	= EURASIA_TAOBJTYPE_STATE |
					(((uPDSSAUpdateCodeAddress.uiAddr >> EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) 
					<< EURASIA_TAPDSSTATE_BASEADDR_SHIFT) & ~EURASIA_TAPDSSTATE_BASEADDR_CLRMSK);

	ui32StateWord1	= (((ui32SAUpdatePDSDataSize >> EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) 
						<< EURASIA_TAPDSSTATE_DATASIZE_SHIFT) & ~EURASIA_TAPDSSTATE_DATASIZE_CLRMSK) |
					EURASIA_TAPDSSTATE_SEC_EXEC | 
					EURASIA_TAPDSSTATE_SECONDARY |
					(ALIGNCOUNTINBLOCKS(0, EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS) 
					<< EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_SHIFT);

#if defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690)
	{
		IMG_UINT32 i;
		
		for (i = 0; i < SGX_FEATURE_NUM_USE_PIPES; i++)
		{
			*pui32Buffer++	= ui32StateWord0;
			*pui32Buffer++	= ui32StateWord1 | aui32USEPipe[i];
		}
	}
#else /* defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690) */
	*pui32Buffer++	= ui32StateWord0;
	*pui32Buffer++	= ui32StateWord1 | EURASIA_TAPDSSTATE_USEPIPE_ALL;
#endif /* defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690) */

	/*
	  Write terminate to VDM control stream, Write the PDS state block
	*/
	*pui32Buffer++	=	EURASIA_TAOBJTYPE_STATE                     |
						EURASIA_TAPDSSTATE_LASTTASK					|
	                    (((uPDSCodeAddress.uiAddr >> EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) 
						  << EURASIA_TAPDSSTATE_BASEADDR_SHIFT) & ~EURASIA_TAPDSSTATE_BASEADDR_CLRMSK);

	*pui32Buffer++	= ( (((ui32PDSDataSize >> EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) 
						  << EURASIA_TAPDSSTATE_DATASIZE_SHIFT) & ~EURASIA_TAPDSSTATE_DATASIZE_CLRMSK) |
						EURASIA_TAPDSSTATE_USEPIPE_1                 |
#if defined(SGX545)
						(2UL << EURASIA_TAPDSSTATE_PARTITIONS_SHIFT) |
#else
						(1UL << EURASIA_TAPDSSTATE_PARTITIONS_SHIFT) |
#endif
						EURASIA_TAPDSSTATE_SD                        |
						EURASIA_TAPDSSTATE_MTE_EMIT					 |
						(ALIGNCOUNTINBLOCKS(ui32USEAttribSize, EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS) 
						 << EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_SHIFT) );

	*pui32Buffer++ = EURASIA_TAOBJTYPE_TERMINATE;

	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_VDM_CTRL_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_VDM_CTRL_STATE_COUNT, (pui32Buffer - pui32BufferBase));

	gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->ui32CommittedPrimOffsetInBytes = gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->ui32CurrentWriteOffsetInBytes;

	gc->ui32EmitMask |= GLES2_EMITSTATE_STATEUPDATE;

	return GLES2_NO_ERROR;
}


/*****************************************************************************
 Function Name	: SetupPBEStateEmit
 Inputs			: gc, psPixelBEState, ui32EmitIdx
 Outputs		: none
 Returns		: success
 Description	: Get the state to emit to PBE on an End Of Tile event
*****************************************************************************/
static GLES2_MEMERROR SetupPBEStateEmit(GLES2Context *gc, EGLPixelBEState *psPixelBEState)
{
	PBE_SURF_PARAMS			 sSurfParams;
	PBE_RENDER_PARAMS		 sRenderParams;
	IMG_UINT32               aui32EmitWords[STATE_PBE_DWORDS + 1];

#if defined(FIX_HW_BRN_24181)
	if((gc->ui32Enables & GLES2_DITHER_ENABLE) && (gc->psDrawParams->ePixelFormat != PVRSRV_PIXEL_FORMAT_ARGB4444))
#else
	if(gc->ui32Enables & GLES2_DITHER_ENABLE)
#endif
	{
		sSurfParams.bEnableDithering = IMG_TRUE;
		psPixelBEState->bDither = IMG_TRUE;
	}
	else
	{
		sSurfParams.bEnableDithering = IMG_FALSE;
		psPixelBEState->bDither = IMG_FALSE;
	}

#if defined(SGX_FEATURE_PBE_GAMMACORRECTION)
	sSurfParams.bEnableGamma = IMG_FALSE;
#endif
	sSurfParams.eFormat = gc->psDrawParams->ePixelFormat;
	sSurfParams.eMemLayout = GetColorAttachmentMemFormat(gc, gc->sFrameBuffer.psActiveFrameBuffer);
	
	if(gc->psMode->ui32AntiAliasMode)
	{
		sSurfParams.eScaling = IMG_SCALING_AA;
	}
	else
	{
		sSurfParams.eScaling = IMG_SCALING_NONE;
	}

	sSurfParams.sAddress.uiAddr = gc->psDrawParams->ui32HWSurfaceAddress;
	sSurfParams.ui32LineStrideInPixels = (gc->psDrawParams->ui32Stride) >> aui32PixelTypeToLineStrideGranShift[gc->psDrawParams->ePixelFormat];

	sRenderParams.eRotation = gc->psDrawParams->eRotationAngle;
	sRenderParams.ui32MaxXClip = gc->psDrawParams->ui32Width - 1;
	sRenderParams.ui32MaxYClip = gc->psDrawParams->ui32Height - 1;
	sRenderParams.ui32MinXClip = 0;
	sRenderParams.ui32MinYClip = 0;
	sRenderParams.uSel.ui32SrcSelect = 0;

	if(gc->psRenderSurface->eColourSpace==PVRSRV_COLOURSPACE_FORMAT_UNKNOWN)
	{
		sSurfParams.bZOnlyRender = IMG_TRUE;

#if defined(FIX_HW_BRN_26922)

		sRenderParams.ui32MaxXClip = 0;
		sRenderParams.ui32MaxYClip = 0;

		sSurfParams.eFormat = PVRSRV_PIXEL_FORMAT_RGB565;
		sSurfParams.eMemLayout = IMG_MEMLAYOUT_STRIDED;
		sSurfParams.ui32LineStrideInPixels = (2 + ((1 << EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT) - 1)) >> 
							EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT;

		sSurfParams.sAddress = gc->sPrim.sBRN26922State.psBRN26922MemInfo->sDevVAddr;
#endif /* defined(FIX_HW_BRN_26922) */
	}
	else
	{
		sSurfParams.bZOnlyRender = IMG_FALSE;
	}

	WritePBEEmitState(&sSurfParams, &sRenderParams, aui32EmitWords);

	GLES2MemCopy(psPixelBEState->aui32EmitWords, aui32EmitWords, sizeof(psPixelBEState->aui32EmitWords));

	psPixelBEState->ui32SidebandWord = aui32EmitWords[STATE_PBE_DWORDS];

	return GLES2_NO_ERROR;
}



/*****************************************************************************
 Function Name	: SetupPixelEventProgram
 Inputs			: gc, psPixelBEState, bPatch
 Outputs		: none
 Returns		: success
 Description	: Sets up the program which will handle pixel events. This may
				  patch preexisting state if there has been a change between
				  StartFrame and the first ScheduleTA
*****************************************************************************/

IMG_INTERNAL GLES2_MEMERROR SetupPixelEventProgram(GLES2Context *gc, EGLPixelBEState *psPixelBEState, IMG_BOOL bPatch)
{
	PDS_PIXEL_EVENT_PROGRAM	 sProgram;
	IMG_UINT32               ui32StateSize;
	IMG_UINT32               *pui32Buffer, *pui32BufferBase;
	IMG_DEV_VIRTADDR         uPixelEventPDSProgramAddr;
	IMG_DEV_VIRTADDR         uEOTCodeAddress, uPTOFFCodeAddress, uEORCodeAddress;
	GLES2_MEMERROR			 eError;


	/************************
      A: Set PBE Emit words
	************************/

	eError = SetupPBEStateEmit(gc, psPixelBEState);

	if(eError != GLES2_NO_ERROR)
	{
		return eError;
	}


	/********************************************
      B: Setup Pixel Event USE and PDS Programs
	********************************************/
	/*
	  B1: Setup End Of Tile USSE Program &
	            Address of End of Tile USSE Code: uEOTCodeAddress
	*/
	eError = WriteEOTUSSECode(gc, psPixelBEState, &uEOTCodeAddress, bPatch);

	if(eError != GLES2_NO_ERROR)
	{
		return eError;
	}

	/*
	  B2: Setup Address of PTOFF/End of Render USSE Code: uPTOFFCodeAddress/uEORCodeAddress
	      (PTOFF/End of Render USSE Programs have already been set up when initialising context) 
	*/
	uPTOFFCodeAddress = gc->sPrim.psPixelEventPTOFFCodeBlock->sCodeAddress;

	GLES_ASSERT((uPTOFFCodeAddress.uiAddr & (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) == 0);

	uEORCodeAddress = gc->sPrim.psPixelEventEORCodeBlock->sCodeAddress;

	GLES_ASSERT((uEORCodeAddress.uiAddr & (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) == 0);


	/*
	  B3: Initialise Pixel Event Position
	*/
	if(bPatch)
	{
		pui32BufferBase = psPixelBEState->pui32PixelEventPDS;

		GLES_ASSERT(pui32BufferBase);
	}
	else
	{
		pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, PDSGetPixeventProgSize() >> 2UL, CBUF_TYPE_PDS_FRAG_BUFFER, IMG_FALSE);
		
		if(!pui32BufferBase)
		{
			return GLES2_3D_BUFFER_ERROR;
		}
		
		/* Record pixel event position so we can patch at first kick if necessary */
		psPixelBEState->pui32PixelEventPDS = pui32BufferBase;
	}

	pui32Buffer = pui32BufferBase;

	/*
	  B4: Setup End Of Tile Task Control for PDS Program
	*/
	sProgram.aui32EOTUSETaskControl[0]	= 0;
	sProgram.aui32EOTUSETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sProgram.aui32EOTUSETaskControl[0] |= 
		(ALIGNCOUNTINBLOCKS(USE_PIXELEVENT_NUM_TEMPS, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT) << EURASIA_PDS_DOUTU0_TRC_SHIFT); 

#if defined(SGX545)
	sProgram.aui32EOTUSETaskControl[1] |= EURASIA_PDS_DOUTU1_PBENABLE; 
#else
	sProgram.aui32EOTUSETaskControl[2] = EURASIA_PDS_DOUTU2_PBENABLE;
#endif


#else 

	sProgram.aui32EOTUSETaskControl[1] |= 
		(ALIGNCOUNTINBLOCKS(USE_PIXELEVENT_NUM_TEMPS, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT) << EURASIA_PDS_DOUTU1_TRC_SHIFT);

	sProgram.aui32EOTUSETaskControl[2] = EURASIA_PDS_DOUTU2_PBENABLE;

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	/* Get a USE execution address*/
	SetUSEExecutionAddress(&sProgram.aui32EOTUSETaskControl[0], 
							0, 
							uEOTCodeAddress, 
							gc->psSysContext->uUSEFragmentHeapBase, 
							SGX_PIXSHADER_USE_CODE_BASE_INDEX);

	/*
	  B5: Setup End Of PTOFF/End of Render Task Control for PDS Program
	*/
	sProgram.aui32PTOFFUSETaskControl[0] = 0;
	sProgram.aui32PTOFFUSETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	sProgram.aui32PTOFFUSETaskControl[2] = 0;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sProgram.aui32PTOFFUSETaskControl[0] |= (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU0_TRC_SHIFT); 
#else 
	sProgram.aui32PTOFFUSETaskControl[1] |= (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU1_TRC_SHIFT);
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	SetUSEExecutionAddress(&sProgram.aui32PTOFFUSETaskControl[0], 
							0, 
							uPTOFFCodeAddress, 
							gc->psSysContext->uUSEFragmentHeapBase, 
							SGX_PIXSHADER_USE_CODE_BASE_INDEX);


	sProgram.aui32EORUSETaskControl[0] = 0;
	sProgram.aui32EORUSETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	sProgram.aui32EORUSETaskControl[2] = 0;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sProgram.aui32EORUSETaskControl[0] |= 
		(ALIGNCOUNTINBLOCKS(USE_PIXELEVENT_EOR_NUM_TEMPS, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT) << EURASIA_PDS_DOUTU0_TRC_SHIFT); 

#if defined(SGX545)
	sProgram.aui32EORUSETaskControl[1] = EURASIA_PDS_DOUTU1_PBENABLE |
										(1 << EURASIA_PDS_DOUTU1_ENDOFRENDER_SHIFT);
#else
	sProgram.aui32EORUSETaskControl[2] = EURASIA_PDS_DOUTU2_PBENABLE |
										(1 << EURASIA_PDS_DOUTU2_ENDOFRENDER_SHIFT);
#endif 
#else 
	sProgram.aui32EORUSETaskControl[1] |= 
				(ALIGNCOUNTINBLOCKS(USE_PIXELEVENT_EOR_NUM_TEMPS, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT) << EURASIA_PDS_DOUTU1_TRC_SHIFT);

	sProgram.aui32EORUSETaskControl[2] |= (1 << EURASIA_PDS_DOUTU2_ENDOFRENDER_SHIFT) | EURASIA_PDS_DOUTU2_PBENABLE;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	SetUSEExecutionAddress(&sProgram.aui32EORUSETaskControl[0], 
							0, 
							uEORCodeAddress, 
							gc->psSysContext->uUSEFragmentHeapBase, 
							SGX_PIXSHADER_USE_CODE_BASE_INDEX);


	/*
	  B6: Generate Pixel Event PDS Program
	*/
	pui32Buffer = PDSGeneratePixelEventProgram(&sProgram, pui32Buffer, IMG_FALSE, IMG_FALSE, 0);

	if(bPatch)
	{
		/* Reset pixel event position until next StartFrame */
		psPixelBEState->pui32PixelEventPDS = IMG_NULL;
	}
	else
	{
		CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_FRAG_BUFFER);

		GLES2_INC_COUNT(GLES2_TIMER_PDS_FRAG_DATA_COUNT, (pui32Buffer - pui32BufferBase));
	}

	/* 
	  B7: Setup Address of Pixel Event PDS Program 
	*/
	uPixelEventPDSProgramAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, sProgram.pui32DataSegment, CBUF_TYPE_PDS_FRAG_BUFFER);

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
	/* Adjust PDS execution address for restricted address range */
	uPixelEventPDSProgramAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif


	/*
	  C: Setup Hardware Register Values
	*/
	ui32StateSize = PDS_PIXEVENT_NUM_PAS;

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	ui32StateSize += USE_PIXELEVENT_NUM_TEMPS * sizeof(IMG_UINT32);
#endif
	
	ui32StateSize += ((1 << EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_ALIGNSHIFT) - 1);

	psPixelBEState->sEventPixelExec.ui32RegAddr = EUR_CR_EVENT_PIXEL_PDS_EXEC;
	psPixelBEState->sEventPixelExec.ui32RegVal = uPixelEventPDSProgramAddr.uiAddr;

	psPixelBEState->sEventPixelData.ui32RegAddr = EUR_CR_EVENT_PIXEL_PDS_DATA;
	psPixelBEState->sEventPixelData.ui32RegVal = (sProgram.ui32DataSize >> EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_ALIGNSHIFT);

	psPixelBEState->sEventPixelInfo.ui32RegAddr = EUR_CR_EVENT_PIXEL_PDS_INFO;

	psPixelBEState->sEventPixelInfo.ui32RegVal = 
		((ui32StateSize >> EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_ALIGNSHIFT) << EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_SHIFT);

#if defined(EUR_CR_EVENT_PIXEL_PDS_INFO_DM_PIXEL)

	psPixelBEState->sEventPixelInfo.ui32RegVal |= 	EUR_CR_EVENT_PIXEL_PDS_INFO_DM_PIXEL |
													EUR_CR_EVENT_PIXEL_PDS_INFO_PNS_MASK |
#if defined(EUR_CR_EVENT_PIXEL_PDS_INFO_USE_PIPELINE_BOTH)
													EUR_CR_EVENT_PIXEL_PDS_INFO_USE_PIPELINE_BOTH |
#endif
													(127 << EUR_CR_EVENT_PIXEL_PDS_INFO_ODS_SHIFT);
#endif

	return GLES2_NO_ERROR;
}


/***********************************************************************************
 Function Name      : ReserveScratchMemory
 Inputs             : gc, ui32ProgramType - either GLES2_SHADERTYPE_VERTEX or GLES2_SHADERTYPE_FRAGMENT
 Outputs            : 
 Returns            : Memory error if any. GLES2_NO_ERROR if successfull.
 Description        : Allocates device memory for the scratch area. Needed by shaders that run out
                      of temporary registers.
************************************************************************************/
static GLES2_MEMERROR ReserveScratchMemory(GLES2Context *gc, IMG_UINT32 ui32ProgramType)
{
	GLES2ProgramShader *psShader;
	USP_HW_SHADER *psPatchedShader;

	if (ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
	{
		psShader = &gc->sProgram.psCurrentProgram->sVertex;
		psPatchedShader = gc->sProgram.psCurrentVertexVariant->psPatchedShader;
	}
	else
	{
		psShader = &gc->sProgram.psCurrentProgram->sFragment;
		psPatchedShader = gc->sProgram.psCurrentFragmentVariant->psPatchedShader;
	}

	GLES_ASSERT(psPatchedShader->sSARegUse.uFlags & USP_HW_SAREG_USE_FLAG_SCRATCH_USED);
	GLES_ASSERT(psShader->psScratchMem == IMG_NULL);
	GLES_ASSERT(psPatchedShader->uScratchAreaSize > 0);


	psShader->psScratchMem = GLES2Calloc(gc, sizeof(GLES2ShaderScratchMem));

	if(!psShader->psScratchMem)
	{
		return GLES2_GENERAL_MEM_ERROR;
	}

	if(GLES2ALLOCDEVICEMEM(gc->ps3DDevData,
						   gc->psSysContext->hGeneralHeap,
						   PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_NO_SYNCOBJ,
						   psPatchedShader->uScratchAreaSize,
						   4,
						   &psShader->psScratchMem->psMemInfo) != PVRSRV_OK)
	{
		GLES2Free(IMG_NULL, psShader->psScratchMem);

		psShader->psScratchMem = IMG_NULL;

		return GLES2_GENERAL_MEM_ERROR;
	}

	ShaderScratchMemAddRef(gc, psShader->psScratchMem);

	return GLES2_NO_ERROR;
}


/***********************************************************************************
 Function Name      : ReserveMemoryForIndexableTemps
 Inputs             : gc, ui32ProgramType - either GLES2_SHADERTYPE_VERTEX or GLES2_SHADERTYPE_FRAGMENT
 Outputs            : 
 Returns            : Memory error if any. GLES2_NO_ERROR if successfull.
 Description        : Allocates device memory for the indexable temporaries. Needed by shaders that
                      index into temporary (non-uniform) arrays.
************************************************************************************/
static GLES2_MEMERROR ReserveMemoryForIndexableTemps(GLES2Context *gc, IMG_UINT32 ui32ProgramType)
{
	GLES2ProgramShader *psShader;
	USP_HW_SHADER *psPatchedShader;

	if (ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
	{
		psShader = &gc->sProgram.psCurrentProgram->sVertex;
		psPatchedShader = gc->sProgram.psCurrentVertexVariant->psPatchedShader;
	}
	else
	{
		psShader = &gc->sProgram.psCurrentProgram->sFragment;
		psPatchedShader = gc->sProgram.psCurrentFragmentVariant->psPatchedShader;
	}

	GLES_ASSERT(psPatchedShader->sSARegUse.uFlags & USP_HW_SAREG_USE_FLAG_INDEXEDTEMPS_USED);
	GLES_ASSERT(psShader->psIndexableTempsMem == IMG_NULL);
	GLES_ASSERT(psPatchedShader->uIndexedTempTotalSize > 0);

	psShader->psIndexableTempsMem = GLES2Calloc(gc, sizeof(GLES2ShaderIndexableTempsMem));

	if(!psShader->psIndexableTempsMem)
	{
		return GLES2_GENERAL_MEM_ERROR;
	}

	if(GLES2ALLOCDEVICEMEM(gc->ps3DDevData,
						   gc->psSysContext->hGeneralHeap,
						   PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_NO_SYNCOBJ,
						   psPatchedShader->uIndexedTempTotalSize,
						   4,
						   &psShader->psIndexableTempsMem->psMemInfo) != PVRSRV_OK)
	{
		GLES2Free(IMG_NULL, psShader->psIndexableTempsMem);

		psShader->psIndexableTempsMem = IMG_NULL;

		return GLES2_GENERAL_MEM_ERROR;
	}

	ShaderIndexableTempsMemAddRef(gc, psShader->psIndexableTempsMem);

	return GLES2_NO_ERROR;
}


/*****************************************************************************
 Function Name	: WritePDSPixelShaderProgram
 Inputs			: gc		- pointer to the context
 Outputs		: pbChanged
 Returns		: success
 Description	: Writes the PDS pixel shader program
*****************************************************************************/
static GLES2_MEMERROR WritePDSPixelShaderProgram(GLES2Context *gc, IMG_BOOL *pbChanged)
{
	GLES2ProgramShader *psShader = &gc->sProgram.psCurrentProgram->sFragment;
	GLES2USEShaderVariant *psFragmentVariant = gc->sProgram.psCurrentFragmentVariant;
	GLES2PDSCodeVariant *psPDSVariant;
	GLES2PDSInfo *psPDSInfo = &psFragmentVariant->u.sFragment.sPDSInfo;
	USP_HW_SHADER *psPatchedShader = psFragmentVariant->psPatchedShader;
	IMG_UINT32 *pui32ChunkCount = gc->sPrim.sFragmentTextureState.aui32ChunkCount;
	PDS_PIXEL_SHADER_PROGRAM sProgram = {0};
	GLES2CompiledTextureState *psTextureState = &gc->sPrim.sFragmentTextureState;
	IMG_UINT32 i;
	IMG_UINT32 ui32PDSProgramSize, ui32PDSDataSize;
#if !defined(FIX_HW_BRN_25339) && !defined(FIX_HW_BRN_27330)
	IMG_UINT32 ui32PDSDataSizeDS0;
	IMG_UINT32 ui32PDSDataSizeDS1;
#endif
	IMG_UINT32 ui32NumTemps;
	IMG_UINT32 ui32NumTempsInBlocks;
	IMG_UINT32 ui32ChunkNumber;
	IMG_UINT32 *pui32Buffer, *pui32BufferBase = 0;
	IMG_DEV_VIRTADDR uPDSPixelShaderBaseAddr;
	HashValue tPDSVariantHash;
	IMG_UINT32 aui32HashInput[GLES2_MAX_TEXTURE_UNITS + 1], ui32HashInputSizeInDWords, ui32HashCompareSizeInDWords, *pui32HashCompare;
#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
	IMG_UINT32 ui32Row, ui32Column;
#endif

	ui32NumTemps = psFragmentVariant->ui32MaxTempRegs;

	/* Align to temp allocation granularity */
	ui32NumTempsInBlocks = ALIGNCOUNTINBLOCKS(ui32NumTemps, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);

	/* USSE base address + unit enables */
	ui32HashCompareSizeInDWords = 2;

	for(i=0; i < GLES2_MAX_TEXTURE_UNITS; i++)
	{
		if(psPDSInfo->ui32NonDependentImageUnits & (1U << i))
		{
			/* 3/4 control words + chunk count */
			ui32HashCompareSizeInDWords += (EURASIA_TAG_TEXTURE_STATE_SIZE + 1);

#if defined(SUPPORT_NV12_FROM_2_HWADDRS)
            if(psTextureState->apsTexFormat[i]->ePixelFormat == PVRSRV_PIXEL_FORMAT_NV12)
            {
			   ui32HashCompareSizeInDWords++;
            }
#endif
		}
	}

	pui32HashCompare = gc->sProgram.aui32HashCompare;

	ui32HashCompareSizeInDWords = 0;
	ui32HashInputSizeInDWords = 0;

	pui32HashCompare[ui32HashCompareSizeInDWords++] = psFragmentVariant->sStartAddress[0].uiAddr;
	aui32HashInput[ui32HashInputSizeInDWords++] =  psFragmentVariant->sStartAddress[0].uiAddr;
	ui32ChunkNumber = 0;

	for(i=0; i < GLES2_MAX_TEXTURE_UNITS; i++)
	{
		if(psPDSInfo->ui32NonDependentImageUnits & (1U << i))
		{
			pui32HashCompare[ui32HashCompareSizeInDWords++] = psTextureState->aui32TAGControlWord[ui32ChunkNumber][0];
			pui32HashCompare[ui32HashCompareSizeInDWords++] = psTextureState->aui32TAGControlWord[ui32ChunkNumber][1];
			pui32HashCompare[ui32HashCompareSizeInDWords++] = psTextureState->aui32TAGControlWord[ui32ChunkNumber][2];

#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
			pui32HashCompare[ui32HashCompareSizeInDWords++] = psTextureState->aui32TAGControlWord[ui32ChunkNumber][3];
#endif		
#if defined(SUPPORT_NV12_FROM_2_HWADDRS)
            if(psTextureState->apsTexFormat[i]->ePixelFormat == PVRSRV_PIXEL_FORMAT_NV12)
            {
			    pui32HashCompare[ui32HashCompareSizeInDWords++] = psTextureState->aui32TAGControlWord[ui32ChunkNumber+1][2];
            }
#endif
            
            pui32HashCompare[ui32HashCompareSizeInDWords++] = pui32ChunkCount[i];

			/* Use Texture address */
			aui32HashInput[ui32HashInputSizeInDWords++] = psTextureState->aui32TAGControlWord[ui32ChunkNumber][2];
		}
		
		ui32ChunkNumber += PDS_NUM_TEXTURE_IMAGE_CHUNKS;
	}

	pui32HashCompare[ui32HashCompareSizeInDWords++] = psPDSInfo->ui32NonDependentImageUnits;

	tPDSVariantHash = HashFunc(aui32HashInput, ui32HashInputSizeInDWords, STATEHASH_INIT_VALUE);
	psPDSVariant = IMG_NULL;
	uPDSPixelShaderBaseAddr.uiAddr = 0;

	/* Search hash table to see if we have already stored this program */
	if(HashTableSearch(gc, 
						&gc->sProgram.sPDSFragmentVariantHashTable, tPDSVariantHash, 
						pui32HashCompare, ui32HashCompareSizeInDWords, 
						(IMG_UINT32 *)&psPDSVariant))
	{
		UCH_UseCodeBlock *psCodeBlock = psPDSVariant->psCodeBlock;
		IMG_UINT32 ui32BlockOffset = (IMG_UINT32)((IMG_UINT8 *)psCodeBlock->pui32LinAddress - (IMG_UINT8 *) psCodeBlock->psCodeMemory->pvLinAddr);
		IMG_DEV_VIRTADDR uBaseAddr;
		
		uBaseAddr.uiAddr = psCodeBlock->psCodeMemory->sDevVAddr.uiAddr + ui32BlockOffset;

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
		uBaseAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

		if((gc->sPrim.ui32FragmentPDSDataSize!=psPDSVariant->ui32DataSize) ||
		   (gc->sPrim.uFragmentPDSBaseAddress.uiAddr!=uBaseAddr.uiAddr))
		{
			/*
				Save the PDS pixel shader data segment address and size (always offset by PDS exec base)
			*/
			gc->sPrim.ui32FragmentPDSDataSize = psPDSVariant->ui32DataSize;
			gc->sPrim.uFragmentPDSBaseAddress = uBaseAddr;

			*pbChanged = IMG_TRUE;
		}
		else
		{
			*pbChanged = IMG_FALSE;
		}
		
		GLES2_INC_COUNT(GLES2_TIMER_PDSVARIANT_HIT_COUNT, 1);
	}
	else
	{
		GLES2_INC_COUNT(GLES2_TIMER_PDSVARIANT_MISS_COUNT, 1);

		*pbChanged = IMG_TRUE;
	
		/* Do not cache this PDS variant if any of the active textures has been ghosted. The rationale 
		   is that when an application is ghosting textures there is no point in trying to cache the 
		   PDS variants as the texture addresses are going to change the next time the texture is 
		   ghosted, rendering the cached PDS code useless.
		*/
		if(!psTextureState->bSomeTexturesWereGhosted)
		{
			psPDSVariant = GLES2Calloc(gc, sizeof(GLES2PDSCodeVariant));

			if(!psPDSVariant)
			{
				PVR_DPF((PVR_DBG_ERROR,"WritePDSPixelShaderProgram: Failed to allocate PDS variant structure"));
				return GLES2_HOST_MEM_ERROR;
			}
		}

		/*
			Setup the USE task control words
		*/
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

		sProgram.aui32USETaskControl[0]	= (ui32NumTempsInBlocks << EURASIA_PDS_DOUTU0_TRC_SHIFT) & ~EURASIA_PDS_DOUTU0_TRC_CLRMSK;
		sProgram.aui32USETaskControl[1]	= psPDSInfo->ui32DependencyControl |
		                                  psShader->ui32USEMode            |
		                                  EURASIA_PDS_DOUTU1_SDSOFT        ;
		sProgram.aui32USETaskControl[2]	= 0;		

		/* Set the execution address of the fragment shader phase */
		SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
								0, 
								psFragmentVariant->sStartAddress[0], 
								gc->psSysContext->uUSEFragmentHeapBase, 
								SGX_PIXSHADER_USE_CODE_BASE_INDEX);

#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

		sProgram.aui32USETaskControl[0]	= psPDSInfo->ui32DependencyControl;
		sProgram.aui32USETaskControl[1]	= psShader->ui32USEMode     |
		                                  ((ui32NumTempsInBlocks << EURASIA_PDS_DOUTU1_TRC_SHIFT) & ~EURASIA_PDS_DOUTU1_TRC_CLRMSK);
		sProgram.aui32USETaskControl[2]	= EURASIA_PDS_DOUTU2_SDSOFT |
		                                  ((ui32NumTempsInBlocks >> EURASIA_PDS_DOUTU2_TRC_INTERNALSHIFT) << EURASIA_PDS_DOUTU2_TRC_SHIFT);

		/* Setup the phase enables.	*/
		for (i = 0; i < psFragmentVariant->ui32PhaseCount; i++)
		{
			IMG_UINT32 aui32ValidExecutionEnables[2] = {0, EURASIA_PDS_DOUTU1_EXE1VALID};

			/* Set the execution address of the fragment shader phase */
			SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
									i, 
									psFragmentVariant->sStartAddress[i], 
									gc->psSysContext->uUSEFragmentHeapBase, 
									SGX_PIXSHADER_USE_CODE_BASE_INDEX);

			/* Mark execution address as active */
			sProgram.aui32USETaskControl[1] |= aui32ValidExecutionEnables[i];
		} 
		
		/* If discard is enabled then we need to set this bit */
		if (gc->sPrim.sRenderState.ui32AlphaTestFlags)
		{
			sProgram.aui32USETaskControl[1] |= EURASIA_PDS_DOUTU1_PUNCHTHROUGH_PHASE1;
		}

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */
			
		/*
			Setup the FPU iterator control words and calculate the size of the PDS pixel program.
		*/
		sProgram.ui32NumFPUIterators = psPDSInfo->ui32IterationCount;

#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
		ui32PDSProgramSize = 3;
#else
		ui32PDSProgramSize = 2;
#endif
		ui32PDSDataSize = 3;

		for (i = 0; i < sProgram.ui32NumFPUIterators; i++)
		{
			IMG_UINT32 ui32TagIssue = psPDSInfo->aui32LayerControl[i];

#if defined(SGX545)
			sProgram.aui32FPUIterators0[i] = psPDSInfo->aui32TSPParamFetchInterface0[i];
			sProgram.aui32FPUIterators1[i] = psPDSInfo->aui32TSPParamFetchInterface1[i];
#else /* defined(SGX545) */
			sProgram.aui32FPUIterators[i] = psPDSInfo->aui32TSPParamFetchInterface[i];
#endif /* defined(SGX545) */

			sProgram.aui32TAGLayers[i]	= ui32TagIssue;

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			sProgram.aui8LayerSize[i] = psPatchedShader->psPSInputLoads[i].uDataSize - 1;
			sProgram.abMinPack[i] = psPatchedShader->psPSInputLoads[i].bPackSampleResults;

			if (ui32TagIssue != 0xFFFFFFFF)
			{
				switch(psPatchedShader->psPSInputLoads[i].eFormat)
				{
					case USP_HW_PSINPUT_FMT_F32:
						sProgram.aui8FormatConv[i] = EURASIA_PDS_MOVS_DOUTT_FCONV_F32;
						break;
					case USP_HW_PSINPUT_FMT_F16:
						sProgram.aui8FormatConv[i] = EURASIA_PDS_MOVS_DOUTT_FCONV_F16;
						break;
					case USP_HW_PSINPUT_FMT_NONE:
						sProgram.aui8FormatConv[i] = EURASIA_PDS_MOVS_DOUTT_FCONV_UNCHANGED;
						break;
					default:
						GLES_ASSERT(psPatchedShader->psPSInputLoads[i].eFormat == USP_HW_PSINPUT_FMT_NONE);
						break;
				}
			}
#endif

			if (ui32TagIssue != 0xFFFFFFFF)
			{
				ui32PDSProgramSize += 2;

#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
				ui32PDSProgramSize += (EURASIA_TAG_TEXTURE_STATE_SIZE - 2);
#endif

#if(EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
				/* 4 DWORD TAG words need to be 2 DWORD aligned, so assume worst case of extra DWORD on every DOUTT */
				ui32PDSDataSize += (EURASIA_TAG_TEXTURE_STATE_SIZE + 1 + EURASIA_PDS_DOUTI_STATE_SIZE);
#else
				ui32PDSDataSize += (EURASIA_TAG_TEXTURE_STATE_SIZE + EURASIA_PDS_DOUTI_STATE_SIZE);
#endif
			}
			else
			{
				ui32PDSProgramSize++;
				ui32PDSDataSize += EURASIA_PDS_DOUTI_STATE_SIZE;
			}
		}

#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
		ui32Row			= (ui32PDSDataSize - 1) / PDS_NUM_DWORDS_PER_ROW;
		ui32Column		= (ui32PDSDataSize - 1) % PDS_NUM_DWORDS_PER_ROW;
		ui32PDSDataSize	= (2 * ui32Row) * PDS_NUM_DWORDS_PER_ROW + ui32Column + 1;
#else
		ui32PDSDataSizeDS0 = (ui32PDSDataSize / 4) * 2 + MIN((ui32PDSDataSize % 4), 2);
		ui32PDSDataSizeDS1 = ui32PDSDataSize - ui32PDSDataSizeDS0;

		if (ui32PDSDataSizeDS1 & 7)
		{
			ui32PDSDataSize = ((ui32PDSDataSizeDS0 + 7) & ~7U) + ui32PDSDataSizeDS1;
		}
#endif

		ui32PDSProgramSize += (ui32PDSDataSize + ((1U << (EURASIA_PDS_DATASIZE_ALIGNSHIFT - 2)) - 1)) & ~((1U << (EURASIA_PDS_DATASIZE_ALIGNSHIFT - 2)) - 1);

		if(psPDSVariant)
		{
			GLES2_TIME_START(GLES2_TIMER_PDSCODEHEAP_FRAG_TIME);
	
			psPDSVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psPDSFragmentCodeHeap,
															ui32PDSProgramSize << 2,
															gc->psSysContext->hPerProcRef);

			GLES2_TIME_STOP(GLES2_TIMER_PDSCODEHEAP_FRAG_TIME);
			GLES2_INC_COUNT(GLES2_TIMER_PDSCODEHEAP_FRAG_COUNT, ui32PDSProgramSize);
		
			if(psPDSVariant->psCodeBlock)
			{
				pui32BufferBase = psPDSVariant->psCodeBlock->pui32LinAddress;
				uPDSPixelShaderBaseAddr = psPDSVariant->psCodeBlock->sCodeAddress;

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
				GLES_ASSERT(uPDSPixelShaderBaseAddr.uiAddr >= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr);
#endif
			}
			else
			{
				PVR_DPF((PVR_DBG_WARNING,"WritePDSPixelShaderProgram: Failed to allocate PDS variant"));

				GLES2Free(IMG_NULL, psPDSVariant);

				psPDSVariant = IMG_NULL;
			}
		}

		if(!psPDSVariant)
		{
			pui32BufferBase = (IMG_UINT32 *) CBUF_GetBufferSpace(gc->apsBuffers, ui32PDSProgramSize, 
															CBUF_TYPE_PDS_FRAG_BUFFER, IMG_FALSE);

			if(!pui32BufferBase)
			{
				return GLES2_3D_BUFFER_ERROR;
			}

			uPDSPixelShaderBaseAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_PDS_FRAG_BUFFER);
		}
	
		if(psTextureState->ui32NumCompiledImageUnits < psPatchedShader->uTexCtrWrdLstCount)
		{
			if(psTextureState->psTextureImageChunks)
			{
				GLES2Free(IMG_NULL, psTextureState->psTextureImageChunks);
			}

			psTextureState->psTextureImageChunks = GLES2Malloc(gc, psPatchedShader->uTexCtrWrdLstCount * sizeof(PDS_TEXTURE_IMAGE_UNIT));
		}

		for(i=0; i < psPatchedShader->uTexCtrWrdLstCount; i++)
		{
			USP_TEX_CTR_WORDS *psTexControlWords = &psPatchedShader->psTextCtrWords[i];

			ui32ChunkNumber = psTexControlWords->uTextureIdx * PDS_NUM_TEXTURE_IMAGE_CHUNKS + psTexControlWords->uChunkIdx;
			
			psTextureState->psTextureImageChunks[i].ui32TAGControlWord0 = 
				(psTextureState->aui32TAGControlWord[ui32ChunkNumber][0] & psTexControlWords->auMask[0]) | psTexControlWords->auWord[0];

			psTextureState->psTextureImageChunks[i].ui32TAGControlWord1 = 
				(psTextureState->aui32TAGControlWord[ui32ChunkNumber][1] & psTexControlWords->auMask[1]) | psTexControlWords->auWord[1];

			psTextureState->psTextureImageChunks[i].ui32TAGControlWord2 = 
				(psTextureState->aui32TAGControlWord[ui32ChunkNumber][2] & psTexControlWords->auMask[2]) | psTexControlWords->auWord[2];

#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
			psTextureState->psTextureImageChunks[i].ui32TAGControlWord3 = 
				(psTextureState->aui32TAGControlWord[ui32ChunkNumber][3] & psTexControlWords->auMask[3]) | psTexControlWords->auWord[3];
#endif
		}

		/*
			Generate the PDS pixel state program
		*/
		pui32Buffer = PDSGeneratePixelShaderProgram(psTextureState->psTextureImageChunks, 
													&sProgram, 
													pui32BufferBase);

		GLES_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= ui32PDSProgramSize);

		if(psPDSVariant)
		{
			psPDSVariant->ui32DataSize = sProgram.ui32DataSize;
	
			/* Copy hashcompare data from temp buffer */
			pui32HashCompare = GLES2Malloc(gc, ui32HashCompareSizeInDWords*sizeof(IMG_UINT32));

			if(!pui32HashCompare)
			{
				PVR_DPF((PVR_DBG_ERROR,"WritePDSPixelShaderProgram: Failed to allocate hash compare structure"));
				
				GLES2Free(IMG_NULL, psPDSVariant);
				return GLES2_HOST_MEM_ERROR;
			}

			GLES2MemCopy(pui32HashCompare, gc->sProgram.aui32HashCompare, ui32HashCompareSizeInDWords*sizeof(IMG_UINT32));

			/* Insert PDS variant in hash table */
			HashTableInsert(gc, 
							&gc->sProgram.sPDSFragmentVariantHashTable, tPDSVariantHash, 
							pui32HashCompare, ui32HashCompareSizeInDWords, 
							(IMG_UINT32)psPDSVariant);

			/* Add to USE variant list */
			psPDSVariant->psNext = psFragmentVariant->psPDSVariant;
			psFragmentVariant->psPDSVariant = psPDSVariant;
		
			/* Record hashing info in item so we can retrieve it through the USE variant */
			psPDSVariant->pui32HashCompare = pui32HashCompare;
			psPDSVariant->ui32HashCompareSizeInDWords = ui32HashCompareSizeInDWords;
			psPDSVariant->tHashValue = tPDSVariantHash;
			
			/* Link to USE variant */
			psPDSVariant->psUSEVariant = psFragmentVariant;

#if defined(PDUMP)
			/* Force redump of shader as new PDS variant has been added */
			psFragmentVariant->psCodeBlock->bDumped = IMG_FALSE;
#endif
		}
		else
		{
			/* Update buffer position */
			CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_FRAG_BUFFER);

			GLES2_INC_COUNT(GLES2_TIMER_PDS_FRAG_DATA_COUNT, (pui32Buffer - pui32BufferBase));
		}

		/*
			Save the PDS pixel shader data segment address and size
		*/
		gc->sPrim.uFragmentPDSBaseAddress.uiAddr = uPDSPixelShaderBaseAddr.uiAddr;

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
		/* Adjust PDS execution address for restricted address range */
		gc->sPrim.uFragmentPDSBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif
		
		gc->sPrim.ui32FragmentPDSDataSize = sProgram.ui32DataSize;
	}

	/* If we haven't generated any PDS variants before, we need to do the DMS Info calculations. They are, however,
	 * the same for all variants. Once done, they don't need to be repeated unless dirty DMS info is flagged.
	 */
	if(!psFragmentVariant->u.sFragment.bHasSetupDMSInfo || ((gc->ui32DirtyState & GLES2_DIRTYFLAG_DMS_INFO) != 0))
	{
		IMG_UINT32 ui32InstanceCount;

		CalculatePixelDMSInfo(&gc->psSysContext->sHWInfo, psFragmentVariant->ui32USEPrimAttribCount, 
							  ui32NumTemps, psShader->ui32USESecAttribDataSizeInDwords, 1, 0, &psPDSInfo->ui32DMSInfo, &ui32InstanceCount);

#if defined(EUR_CR_ISP_TAGCTRL_TRIANGLE_FRAGMENT_PIXELS_MASK)
		/* Round to nearest 16 instances */
		ui32InstanceCount &= 0xFFFFFFF0;
		
		psPDSInfo->ui32TriangleSplitPixelThreshold = MAX(ui32InstanceCount, 16);
#endif

		psFragmentVariant->u.sFragment.bHasSetupDMSInfo = IMG_TRUE;
	}

#if defined(EUR_CR_ISP_TAGCTRL_TRIANGLE_FRAGMENT_PIXELS_MASK)
	gc->psRenderSurface->ui32TriangleSplitPixelThreshold = 
		MIN(gc->psRenderSurface->ui32TriangleSplitPixelThreshold, psPDSInfo->ui32TriangleSplitPixelThreshold);
#endif

	return GLES2_NO_ERROR;
}


/****************************************************************************************************
 Function Name	: WritePDSVertexShaderProgramWithVAO 
 Inputs			: gc, b32BitIndices
 Outputs		: 
 Returns		: Success
 Description	: Writes the PDS program for the vertex shader
*****************************************************************************************************/
static GLES2_MEMERROR WritePDSVertexShaderProgramWithVAO(GLES2Context *gc, IMG_BOOL b32BitIndices)
{
    IMG_UINT32 i, *pui32Buffer;
	GLES2ProgramShader    *psVertexShader  = &(gc->sProgram.psCurrentProgram->sVertex);
	GLES2USEShaderVariant *psVertexVariant = gc->sProgram.psCurrentVertexVariant;
	IMG_UINT32             ui32NumTemps;
	IMG_UINT32             ui32MaxReg;
	IMG_DEV_VIRTADDR	   uVertexPDSBaseAddress;

	GLES2VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);
	GLES2VertexArrayObject    *psVAO = gc->sVAOMachine.psActiveVAO;
	GLES2PDSVertexState       *psPDSVertexState;
	PDS_VERTEX_SHADER_PROGRAM *psPDSVertexShaderProgram;

    IMG_BOOL bDirty = (gc->ui32DirtyState & ( GLES2_DIRTYFLAG_VP_STATE           |
											  GLES2_DIRTYFLAG_VAO_ATTRIB_POINTER |
											  GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM  |
											  GLES2_DIRTYFLAG_VAO_BINDING        |
											  GLES2_DIRTYFLAG_PRIM_INDEXBITS     )) ? IMG_TRUE : IMG_FALSE;

	IMG_BOOL bDirtyOnlyBinding = ( (gc->ui32DirtyState & ( GLES2_DIRTYFLAG_VP_STATE           |
														   GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM  |
														   GLES2_DIRTYFLAG_VAO_ATTRIB_POINTER |
														   GLES2_DIRTYFLAG_PRIM_INDEXBITS     )) == 0 ) ? IMG_TRUE : IMG_FALSE;

	IMG_BOOL bDirtyOnlyPointerBinding = ( (gc->ui32DirtyState & ( GLES2_DIRTYFLAG_VP_STATE          |
																  GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM |
																  GLES2_DIRTYFLAG_PRIM_INDEXBITS    )) == 0 ) ? IMG_TRUE : IMG_FALSE;
	
	IMG_BOOL bDirtyProgram = bDirtyOnlyBinding ? IMG_FALSE : IMG_TRUE;


	/* Assert VAO and setup its PDS states and PDS program */
	GLES_ASSERT(VAO(gc));
	psPDSVertexState         = psVAO->psPDSVertexState;
	psPDSVertexShaderProgram = psVAO->psPDSVertexShaderProgram;


	/* Return if there's no variant */
	if(!psVertexVariant)
	{
		PVR_DPF((PVR_DBG_ERROR,"WritePDSVertexShaderProgramWithVAO: No vertex shader!"));

		return GLES2_GENERAL_MEM_ERROR;
	}
	ui32NumTemps = psVertexVariant->ui32MaxTempRegs;
	ui32MaxReg   = psVertexVariant->ui32USEPrimAttribCount;


	GLES2_TIME_START(GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);

	/*
	   Setup VAO's psPDSVertexState & psPDSVertexShaderProgram & 
	   if the state or the program hasn't been initialised, 
	   or uVertexPDSBaseAddress hasn't been assigned,
	   or if stream data and pointers of the vertex array object have changed,
	*/
	if ( bDirty                      ||
		 (!psPDSVertexState)         ||
		 (!psPDSVertexShaderProgram)  )
	{

	    IMG_BOOL   bOkToPatch = IMG_TRUE;


	    /* Allocate space for psPDSVertexState */
		if (!psPDSVertexState)
		{
			psPDSVertexState = (GLES2PDSVertexState *) GLES2Calloc(gc, sizeof(GLES2PDSVertexState));

			if (psPDSVertexState == IMG_NULL)
			{
				GLES2Free(IMG_NULL, psVAO);
				
				GLES2_TIME_STOP(GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);
				
				return GLES2_GENERAL_MEM_ERROR;
			}
			psVAO->psPDSVertexState = psPDSVertexState;
		}
	  

	    /* Allocate space for psPDSVertexShaderProgram */
		if (!psPDSVertexShaderProgram)
		{
			psPDSVertexShaderProgram = (PDS_VERTEX_SHADER_PROGRAM *) GLES2Calloc(gc, sizeof(PDS_VERTEX_SHADER_PROGRAM));

			if (psPDSVertexShaderProgram == IMG_NULL)
			{
				GLES2Free(IMG_NULL, psVAO);

				GLES2_TIME_STOP(GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);

				return GLES2_GENERAL_MEM_ERROR;
			}
			psVAO->psPDSVertexShaderProgram = psPDSVertexShaderProgram;

		}


		if (bDirtyOnlyBinding) 
		{
		    /*
			   If just bind another VAO (its program has already been prepared before)

			   DO NOTHING : DO NOT GENERATE OR PATCH PDS VERTEX SHADER PROGRAM. 
			*/
		}	
		else if (bDirtyOnlyPointerBinding)
		{
		    /* 
			   When Only attrib pointer and | or VAO binding is changed,
			   
			   Patch PDS vertex shader program.
			*/

		    IMG_UINT32 ui32NumStreams = 0;
		
			GLES2_TIME_START(GLES2_TIMER_PATCH_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);

			psPDSVertexState->sProgramInfo.bPatchTaskControl = IMG_FALSE;

			/* Setup addresses for all of the streams */
			for (i = 0; i < psVAOMachine->ui32NumItemsPerVertex; i++)
			{
			    GLES2AttribArrayPointerMachine *psVAOAttribPointer = psVAOMachine->apsPackedAttrib[i];
				GLES2AttribArrayPointerState   *psVAOAPState = psVAOAttribPointer->psState;
				IMG_DEV_VIRTADDR    uHWStreamAddress;
			
				/* A non-zero buffer name meams the stream address is actually an offset */
				if(psVAOAPState->psBufObj && !psVAOAttribPointer->bIsCurrentState)
				{
					IMG_DEV_VIRTADDR uDevAddr;
					IMG_UINTPTR_T uOffset = (IMG_UINTPTR_T)psVAOAttribPointer->pvPDSSrcAddress;
					
					uDevAddr.uiAddr = psVAOAPState->psBufObj->psMemInfo->sDevVAddr.uiAddr;
					
					/* If the pointer is beyond the buffer object, it is probably an application bug - 
					   just set it to 0, to try to prevent an SGX page fault */
					if(uOffset > psVAOAPState->psBufObj->psMemInfo->uAllocSize)
					{
						PVR_DPF((PVR_DBG_WARNING,"WritePDSVertexShaderProgramWithVAO: Offset to VBO out of bounds. Likely an app bug! Setting offset to zero"));
						uOffset = 0;
					}

					/* add in offset */
					uHWStreamAddress.uiAddr = uDevAddr.uiAddr + uOffset;
				}
				else
				{
					/* Get the device address of the buffer */
					uHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers,
																   (IMG_UINT32 *)psVAOAttribPointer->pvPDSSrcAddress,
																   CBUF_TYPE_VERTEX_DATA_BUFFER);
				}

				psPDSVertexState->sProgramInfo.aui32StreamAddresses[ui32NumStreams++] = uHWStreamAddress.uiAddr;

				/* If the next stream was merged with this one, make sure that it's still able to be merged */
				if(((i+1)<psVAOMachine->ui32NumItemsPerVertex) && psPDSVertexState->bMerged[i])
				{
					GLES2AttribArrayPointerMachine *psNextVAOAttribPointer = psVAOMachine->apsPackedAttrib[i+1];
					IMG_DEV_VIRTADDR uNextHWStreamAddress;
				
					GLES_ASSERT(!psNextVAOAttribPointer->bIsCurrentState);
			
					/* A non-zero buffer name meams the stream address is actually an offset */
					if(psNextVAOAttribPointer->psState->psBufObj)
					{
						IMG_DEV_VIRTADDR uDevAddr;
						IMG_UINTPTR_T uOffset = (IMG_UINTPTR_T)psNextVAOAttribPointer->pvPDSSrcAddress;
						
						uDevAddr.uiAddr = psNextVAOAttribPointer->psState->psBufObj->psMemInfo->sDevVAddr.uiAddr;
						
						/* If the pointer is beyond the buffer object, it is probably an application bug - 
							just set it to 0, to try to prevent an SGX page fault */
						if(uOffset > psNextVAOAttribPointer->psState->psBufObj->psMemInfo->uAllocSize)
						{
							PVR_DPF((PVR_DBG_WARNING,"WritePDSVertexShaderProgramWithVAO: Offset to VBO out of bounds. Likely an app bug! Setting offset to zero"));
							uOffset = 0;
						}

						/* add in offset */
						uNextHWStreamAddress.uiAddr = uDevAddr.uiAddr + uOffset;
					}
					else
					{
						/* Get the device address of the buffer */
						uNextHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers,
																		   (IMG_UINT32 *)psNextVAOAttribPointer->pvPDSSrcAddress,
																		   CBUF_TYPE_VERTEX_DATA_BUFFER);
					}

					/* Source addresses must be sequential */
					if((uHWStreamAddress.uiAddr + psNextVAOAttribPointer->ui32Size) != uNextHWStreamAddress.uiAddr)
					{
						bOkToPatch = IMG_FALSE;
					}

					/* Ok to remain merged, so skip the next stream */
					i++;
				}
			}

			if(bOkToPatch)
			{
				PDSPatchVertexShaderProgram( &(psPDSVertexState->sProgramInfo), 
											 psPDSVertexState->aui32LastPDSProgram );
			}

			GLES2_TIME_STOP(GLES2_TIMER_PATCH_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);

		}

		if (!bOkToPatch ||
			!bDirtyOnlyPointerBinding)
		{
		    /*
			  For all the other dirty cases and the case when patching PDS program is unfeasible, 

			  PDS vertex shader program has to be generated.
			*/

		    GLES2_TIME_START(GLES2_TIMER_GENERATE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);

			/* Setup the input program structure */
			psPDSVertexShaderProgram->pui32DataSegment   = IMG_NULL;
			psPDSVertexShaderProgram->ui32DataSize       = 0;
			psPDSVertexShaderProgram->b32BitIndices  	 = b32BitIndices;
			psPDSVertexShaderProgram->ui32NumInstances   = 0;
			psPDSVertexShaderProgram->ui32NumStreams     = 0;
			psPDSVertexShaderProgram->bIterateVtxID      = IMG_FALSE;
			psPDSVertexShaderProgram->bIterateInstanceID = IMG_FALSE;

			/* Align to temp allocation granularity */
			ui32NumTemps = ALIGNCOUNTINBLOCKS(ui32NumTemps, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);

			/* Set the USE Task control words */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

			psPDSVertexShaderProgram->aui32USETaskControl[0]	= ((ui32NumTemps << EURASIA_PDS_DOUTU0_TRC_SHIFT) 
																                 & ~EURASIA_PDS_DOUTU0_TRC_CLRMSK);
			psPDSVertexShaderProgram->aui32USETaskControl[1]	= psVertexShader->ui32USEMode;
			psPDSVertexShaderProgram->aui32USETaskControl[2]	= 0;
		
			/* Set the execution address of the fragment shader phase */
			SetUSEExecutionAddress(&(psPDSVertexShaderProgram->aui32USETaskControl[0]), 
								   0, 
								   psVertexVariant->sStartAddress[0], 
								   gc->psSysContext->uUSEVertexHeapBase, 
								   SGX_VTXSHADER_USE_CODE_BASE_INDEX);
		
#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

			psPDSVertexShaderProgram->aui32USETaskControl[0]	= 0;
			psPDSVertexShaderProgram->aui32USETaskControl[1]	= (psVertexShader->ui32USEMode |
																   ((ui32NumTemps << EURASIA_PDS_DOUTU1_TRC_SHIFT) 
																	              & ~EURASIA_PDS_DOUTU1_TRC_CLRMSK));

			psPDSVertexShaderProgram->aui32USETaskControl[2]	= (((ui32NumTemps >> EURASIA_PDS_DOUTU2_TRC_INTERNALSHIFT) 
																                  << EURASIA_PDS_DOUTU2_TRC_SHIFT));

			/* Load all of the phases */
			for (i = 0; i < psVertexVariant->ui32PhaseCount; i++)
			{
				IMG_UINT32 aui32ValidExecutionEnables[2] = {0, EURASIA_PDS_DOUTU1_EXE1VALID};

				/* Set the execution address of the vertex shader phase */
				SetUSEExecutionAddress(&(psPDSVertexShaderProgram->aui32USETaskControl[0]), 
									   i,
									   psVertexVariant->sStartAddress[i], 
									   gc->psSysContext->uUSEVertexHeapBase, 
									   SGX_VTXSHADER_USE_CODE_BASE_INDEX);
				
				/* Mark execution address as active */
				psPDSVertexShaderProgram->aui32USETaskControl[1] |= aui32ValidExecutionEnables[i];
			}
		
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */


			/* Setup copies for all of the streams */
			for (i = 0; i < psVAOMachine->ui32NumItemsPerVertex; i++)
			{
			    GLES2AttribArrayPointerMachine *psVAOAttribPointer = psVAOMachine->apsPackedAttrib[i];
				IMG_DEV_VIRTADDR    uHWStreamAddress;
				PDS_VERTEX_STREAM  *psPDSVertexStream    = &(psPDSVertexShaderProgram->asStreams[psPDSVertexShaderProgram->ui32NumStreams++]);
				PDS_VERTEX_ELEMENT *psPDSVertexElement   = &(psPDSVertexStream->asElements[0]);
				IMG_UINT32          ui32CurrentVAOAttrib = (IMG_UINT32)(psVAOAttribPointer - &(psVAOMachine->asAttribPointer[0]));
				IMG_UINT32          ui32PrimaryAttrReg;

				/* A non-zero buffer name meams the stream address is actually an offset */
				if ((psVAOAttribPointer->psState->psBufObj) && !(psVAOAttribPointer->bIsCurrentState)) 
				{
					IMG_DEV_VIRTADDR uDevAddr;
					IMG_UINTPTR_T uOffset = (IMG_UINTPTR_T)psVAOAttribPointer->pvPDSSrcAddress;
					
					uDevAddr.uiAddr = psVAOAttribPointer->psState->psBufObj->psMemInfo->sDevVAddr.uiAddr;
					
					/* If the pointer is beyond the buffer object, it is probably an application bug - 
						just set it to 0, to try to prevent an SGX page fault */
					if(uOffset > psVAOAttribPointer->psState->psBufObj->psMemInfo->uAllocSize)
					{
						PVR_DPF((PVR_DBG_WARNING,"WritePDSVertexShaderProgramWithVAO: Offset to VBO out of bounds. Likely an app bug! Setting offset to zero"));
						uOffset = 0;
					}
			
					/* add in offset */
					uHWStreamAddress.uiAddr = uDevAddr.uiAddr + uOffset;
				}
				else
				{
					/* Get the device address of the buffer for current state values */
				    uHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers,
																   (IMG_UINT32 *)psVAOAttribPointer->pvPDSSrcAddress,
																   CBUF_TYPE_VERTEX_DATA_BUFFER);
				}

				/* An input register in uniflex is considered to be 4 IMG_FLOATs therefore we need to multiply
				   the destReg by 4 to insure that the data for each register is in the correct place */
				ui32PrimaryAttrReg = gc->sProgram.psCurrentProgram->aui32InputRegMappings[ui32CurrentVAOAttrib] << 2;
				
				psPDSVertexElement->ui32Offset   = 0;
				psPDSVertexElement->ui32Register = ui32PrimaryAttrReg;
			
				/* Store highest register written to (0 == 4 registers) */
				if (ui32PrimaryAttrReg + 4 > ui32MaxReg)
				{
					ui32MaxReg = ui32PrimaryAttrReg + 4;
				}

				/* Fill in the PDS descriptor */
				psPDSVertexStream->ui32NumElements = 1;
				psPDSVertexStream->bInstanceData   = IMG_FALSE;
				psPDSVertexStream->ui32Multiplier  = 0;
				psPDSVertexStream->ui32Address     = uHWStreamAddress.uiAddr;
				
				/* If the data comes from current state it is (4*float) with infinite repeat. */
				if(psVAOAttribPointer->bIsCurrentState)
				{
					psPDSVertexElement->ui32Size    = 16;
					psPDSVertexStream->ui32Stride   = 16;
					psPDSVertexStream->ui32Shift    = 31;
				
					psPDSVertexState->bMerged[i] = IMG_FALSE;
				}
				else 
				{
				    if (psVAOAttribPointer->psState->psBufObj)
					{
						psPDSVertexElement->ui32Size    = psVAOAttribPointer->ui32Size;
						psPDSVertexStream->ui32Stride   = psVAOAttribPointer->ui32Stride;
						psPDSVertexStream->ui32Shift    = 0;
					}
					else
					{
					    if (VAO_IS_ZERO(gc)) /* Setup the default VAO's client array attributes */
						{
							psPDSVertexElement->ui32Size    = psVAOAttribPointer->ui32Size;
							psPDSVertexStream->ui32Stride   = psVAOAttribPointer->ui32Size;
							psPDSVertexStream->ui32Shift    = 0;
						}	
					}

					psPDSVertexState->bMerged[i] = IMG_FALSE;

					/* Try to combine the next attribute stream with this one */
					if((i+1) < psVAOMachine->ui32NumItemsPerVertex)
					{
						GLES2AttribArrayPointerMachine *psNextVAOAttribPointer = psVAOMachine->apsPackedAttrib[i+1];
						IMG_UINT32 ui32NextVAOAttrib = (IMG_UINT32)(psNextVAOAttribPointer - &(psVAOMachine->asAttribPointer[0]));
						IMG_DEV_VIRTADDR uNextHWStreamAddress;
						IMG_UINT32 ui32NextPrimaryAttribute = gc->sProgram.psCurrentProgram->aui32InputRegMappings[ui32NextVAOAttrib] << 2;

						/* Can't combine current state with normal attributes (it's too big anyway) */
						if(psNextVAOAttribPointer->bIsCurrentState)
						{
							continue;
						}

						/* Combined attrib data must be <= max DMA size */
						if(psVAOAttribPointer->ui32Size + psNextVAOAttribPointer->ui32Size > (EURASIA_PDS_DOUTD1_BSIZE_MAX<<2))
						{
							continue;
						}

						/* Must be writing to sequential PAs */
						if(ui32NextPrimaryAttribute != (ui32PrimaryAttrReg + 4))
						{
						    continue;
						}
						
						/* Can't have any gaps in the PAs */
						if(ui32NextPrimaryAttribute != (ui32PrimaryAttrReg + psVAOAttribPointer->ui32Size/4))
						{
							continue;
						}

						/* Strides must match */
						if(psPDSVertexStream->ui32Stride != psNextVAOAttribPointer->ui32Stride)
						{
							continue;
						}

						if(psNextVAOAttribPointer->psState->psBufObj)
						{
							IMG_DEV_VIRTADDR uNextDevAddr;
							IMG_UINTPTR_T uOffset = (IMG_UINTPTR_T)psNextVAOAttribPointer->pvPDSSrcAddress;
							
							/* Setup next source address */
							uNextDevAddr.uiAddr = psNextVAOAttribPointer->psState->psBufObj->psMemInfo->sDevVAddr.uiAddr; 
							
							/* If the pointer is beyond the buffer object, it is probably an application bug - 
								just set it to 0, to try to prevent an SGX page fault */
							if(uOffset > psNextVAOAttribPointer->psState->psBufObj->psMemInfo->uAllocSize)
							{
								PVR_DPF((PVR_DBG_WARNING,"WritePDSVertexShaderProgramWithVAO: Offset to VBO out of bounds. Likely an app bug! Setting offset to zero"));
								uOffset = 0;
							}

							/* add in offset */
							uNextHWStreamAddress.uiAddr = uNextDevAddr.uiAddr + uOffset;
						}
						else
						{
							/* Get the device address of the buffer */
							uNextHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers,
																			   (IMG_UINT32 *)psNextVAOAttribPointer->pvPDSSrcAddress,
																			   CBUF_TYPE_VERTEX_DATA_BUFFER);
						}
					
						/* Source addresses must be sequential */
						if(uNextHWStreamAddress.uiAddr != (uHWStreamAddress.uiAddr + psVAOAttribPointer->ui32Size))
						{
							continue;
						}
				
						/* Store highest register written to (0 == 4 registers) */
						if (ui32NextPrimaryAttribute + 4 > ui32MaxReg)
						{
							ui32MaxReg = ui32NextPrimaryAttribute + 4;
						}

						/* Added together the size of the 2 attribute streams */
						psPDSVertexElement->ui32Size += psNextVAOAttribPointer->ui32Size;
						
						psPDSVertexState->bMerged[i]   = IMG_TRUE;
						psPDSVertexState->bMerged[i+1] = IMG_FALSE;
						
						/* Skip the next attribute as it's now combined with the current one */
						i++;
					}
				}
			}

			/* Generate the PDS Program for this shader */
			pui32Buffer = (IMG_UINT32 *)PDSGenerateVertexShaderProgram(psPDSVertexShaderProgram, 
																	   psPDSVertexState->aui32LastPDSProgram,
																	   &(psPDSVertexState->sProgramInfo));

			psPDSVertexState->ui32ProgramSize = (IMG_UINT32)(pui32Buffer - (psPDSVertexState->aui32LastPDSProgram));
			

			GLES2_TIME_STOP(GLES2_TIMER_GENERATE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);
			
		}

	} /* Finished setting up VAO's psPDSVertexShaderProgram, psPDSState */


	/* Set up uVertexPDSBaseAddress */

	GLES2_TIME_START(GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_REST_TIME);


	/* 
	   Copy generated or patched PDS vertex shader program into device memory:

	   1st time (program is generated): allocate psMemInfo and copy program into it;
                                        refer to the psMemInfo address when using program.

	   2nd time (program is generated): keep psMemInfo, 
	                                    copy program into circular buffer every time when using program,
                                        and refer to the new address in circular buffer.

	   later on (program is generated): same as the 2nd time.
	*/

	if ( bDirtyProgram && (psVAO->psMemInfo != IMG_NULL) )
	{
	    psVAO->bUseMemInfo = IMG_FALSE;
	}

	if (psVAO->bUseMemInfo == IMG_TRUE)
	{
	    if (psVAO->psMemInfo == IMG_NULL)
		{
		    IMG_UINT32 ui32VAOAlign   = EURASIA_PDS_DATA_CACHE_LINE_SIZE;
			IMG_UINT32 ui32VAOSize    = psVAO->psPDSVertexState->ui32ProgramSize << 2;
			IMG_UINT32 ui32AllocFlags = PVRSRV_MEM_READ;
			IMG_VOID *pvSrc, *pvDest;

		  
			if (GLES2ALLOCDEVICEMEM(gc->ps3DDevData,
									gc->psSysContext->hPDSVertexHeap,
									ui32AllocFlags,
									ui32VAOSize,
									ui32VAOAlign,
									&(psVAO->psMemInfo)) != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"WritePDSVertexShaderProgramWithVAO: Can't allocate meminfo for VAO"));
				psVAO->psMemInfo = IMG_NULL;

				GLES2_TIME_STOP(GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_REST_TIME);
				GLES2_TIME_STOP(GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);

				return GLES2_TA_BUFFER_ERROR;
			}

			pvSrc  = (IMG_VOID *)((IMG_UINT8 *)psPDSVertexState->aui32LastPDSProgram);
			pvDest = psVAO->psMemInfo->pvLinAddr;
		
			GLES2MemCopy(pvDest, pvSrc, (psPDSVertexState->ui32ProgramSize << 2));
		}

		/* Retrieve the PDS vertex shader data segment physical address */
		uVertexPDSBaseAddress = psVAO->psMemInfo->sDevVAddr;
		
	}
	else
	{
		/* Get buffer space for the PDS vertex program */
		IMG_UINT32 *pui32BufferBase = (IMG_UINT32 *) CBUF_GetBufferSpace(gc->apsBuffers, 
																		 psPDSVertexState->ui32ProgramSize, 
																		 CBUF_TYPE_PDS_VERT_BUFFER, 
																		 IMG_FALSE);
		if(!pui32BufferBase)
		{
		    GLES2_TIME_STOP(GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_REST_TIME);
			GLES2_TIME_STOP(GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);
			
			return GLES2_TA_BUFFER_ERROR;
		}
		
		GLES2MemCopy(pui32BufferBase, 
					 psPDSVertexState->aui32LastPDSProgram, 
					 psPDSVertexState->ui32ProgramSize << 2);
		
		/* Update buffer position */
		CBUF_UpdateBufferPos(gc->apsBuffers, 
							 psPDSVertexState->ui32ProgramSize, 
							 CBUF_TYPE_PDS_VERT_BUFFER);
		
		GLES2_INC_COUNT(GLES2_TIMER_PDS_VERT_DATA_COUNT, psPDSVertexState->ui32ProgramSize);
		
		/* Retrieve the PDS vertex shader data segment physical address */
		uVertexPDSBaseAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers, 
															pui32BufferBase,
															CBUF_TYPE_PDS_VERT_BUFFER);
	}


	/* Reset VAO's dirty flag */
    psVAO->ui32DirtyState &= ~GLES2_DIRTYFLAG_VAO_ALL;


	/* Reset sPrim.psPDSVertexState */
	gc->sPrim.psPDSVertexState = psVAO->psPDSVertexState;

	/* Setup sPrim's PDS vertex shader data segment physical address and size */
	gc->sPrim.uVertexPDSBaseAddress = uVertexPDSBaseAddress;

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
	/* Adjust PDS execution address for restricted address range */
	gc->sPrim.uVertexPDSBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

	gc->sPrim.ui32VertexPDSDataSize = psPDSVertexShaderProgram->ui32DataSize;

	psVertexVariant->ui32USEPrimAttribCount = ui32MaxReg;


	GLES2_TIME_STOP(GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_REST_TIME);

	GLES2_TIME_STOP(GLES2_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);
	
	return GLES2_NO_ERROR;
}


/*****************************************************************************
 Function Name	: LoadSecondaryAttributes
 Inputs			: gc, ui32ProgramType
 Outputs		: Device address and size of secondaries
 Returns		: Mem Error
 Description	: Loads secondary attribute constants into a buffer for upload
*****************************************************************************/
static GLES2_MEMERROR LoadSecondaryAttributes(GLES2Context *gc,
											IMG_UINT32 ui32ProgramType,
											IMG_DEV_VIRTADDR *puDeviceAddress,
											IMG_UINT32 *pui32DataSizeInDWords)
{
	IMG_UINT32 ui32SABufferType;
	IMG_UINT32 ui32SizeOfConstantsInDWords;
	GLES2ProgramShader *psShader;
	IMG_UINT32 i;
	IMG_UINT32 *pui32SecAttribs;
	GLSLBindingSymbolList *psSymbolList;
	USP_HW_SHADER *psPatchedShader;
	GLES2_MEMERROR eError;
	GLES2CompiledRenderState *psRenderState = &gc->sPrim.sRenderState;
	IMG_UINT32 (* pui32TexControlWords)[EURASIA_TAG_TEXTURE_STATE_SIZE];

	if (ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
	{
		psShader = &gc->sProgram.psCurrentProgram->sVertex;
		psPatchedShader = gc->sProgram.psCurrentVertexVariant->psPatchedShader;
		pui32TexControlWords = &gc->sPrim.sVertexTextureState.aui32TAGControlWord[0];

		ui32SABufferType = CBUF_TYPE_PDS_VERT_BUFFER;
		eError = GLES2_TA_BUFFER_ERROR;
		/* Set min value */
		ui32SizeOfConstantsInDWords = GLES2_VERTEX_SECATTR_NUM_RESERVED;
	}
	else
	{
		psShader = &gc->sProgram.psCurrentProgram->sFragment;
		psPatchedShader = gc->sProgram.psCurrentFragmentVariant->psPatchedShader;
		pui32TexControlWords = &gc->sPrim.sFragmentTextureState.aui32TAGControlWord[0];

		ui32SABufferType = CBUF_TYPE_PDS_FRAG_BUFFER;
		eError = GLES2_3D_BUFFER_ERROR;
		/* Set min value */
		ui32SizeOfConstantsInDWords = GLES2_FRAGMENT_SECATTR_NUM_RESERVED;
	}

	psSymbolList = &psShader->psSharedState->sBindingSymbolList;

	/* Get buffer space for all the SA-register constants */
	pui32SecAttribs = CBUF_GetBufferSpace(gc->apsBuffers,
									 MAX(ui32SizeOfConstantsInDWords, psPatchedShader->uSARegCount),
									 ui32SABufferType, IMG_FALSE); 

	if(!pui32SecAttribs)
	{
		return eError;
	}

#ifdef DEBUG
	for (i = 0; i < MAX(ui32SizeOfConstantsInDWords, psPatchedShader->uSARegCount); i++)
	{
		pui32SecAttribs[i] = 0xF0000000 | i;
	}
#endif

	if (ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
	{
		GLES2_FUINT32 sTemp;

		/* Set constants base */
		pui32SecAttribs[GLES2_VERTEX_SECATTR_CONSTANTBASE] = 
			psShader->uUSEConstsDataBaseAddress.uiAddr + (IMG_UINT32)psPatchedShader->iSAAddressAdjust;

		/* Set indexable temp base */
		if(psShader->psIndexableTempsMem)
		{
			pui32SecAttribs[GLES2_VERTEX_SECATTR_INDEXABLETEMPBASE] = psShader->psIndexableTempsMem->psMemInfo->sDevVAddr.uiAddr;
		}

		/* Set up constants for unpacking of normalized signed bytes */
		sTemp.fVal = 2.0f/255.0f;
		pui32SecAttribs[GLES2_VERTEX_SECATTR_UNPCKS8_CONST1] = sTemp.ui32Val;

		sTemp.fVal = 1.0f/255.0f;
		pui32SecAttribs[GLES2_VERTEX_SECATTR_UNPCKS8_CONST2] = sTemp.ui32Val;

		/* Set up constants for unpacking of normalized signed shorts */
		sTemp.fVal = 2.0f/65535.0f;
		pui32SecAttribs[GLES2_VERTEX_SECATTR_UNPCKS16_CONST1] = sTemp.ui32Val;

		sTemp.fVal = 1.0f/65535.0f;
		pui32SecAttribs[GLES2_VERTEX_SECATTR_UNPCKS16_CONST2] = sTemp.ui32Val;

		/* Set scratch area base */
		if(psShader->psScratchMem)
		{
			pui32SecAttribs[GLES2_VERTEX_SECATTR_SCRATCHBASE] = psShader->psScratchMem->psMemInfo->sDevVAddr.uiAddr;
		}
	}
	else
	{
		/* Set min value */
		ui32SizeOfConstantsInDWords = 0;

		if(psPatchedShader->sSARegUse.uFlags & USP_HW_SAREG_USE_FLAG_MEMCONSTS_USED)
		{
			/* Set constants base */
			pui32SecAttribs[GLES2_FRAGMENT_SECATTR_CONSTANTBASE] = 
				psShader->uUSEConstsDataBaseAddress.uiAddr + (IMG_UINT32)psPatchedShader->iSAAddressAdjust;

			ui32SizeOfConstantsInDWords = MAX(ui32SizeOfConstantsInDWords, GLES2_FRAGMENT_SECATTR_CONSTANTBASE + 1);	/* PRQA S 3356 */ /* Override QAC suggestion to keep code flexible. */
		}

		if(psShader->psIndexableTempsMem)
		{
			/* Set indexable temp base */
			GLES_ASSERT(psPatchedShader->sSARegUse.uFlags & USP_HW_SAREG_USE_FLAG_INDEXEDTEMPS_USED);

			pui32SecAttribs[GLES2_FRAGMENT_SECATTR_INDEXABLETEMPBASE] = psShader->psIndexableTempsMem->psMemInfo->sDevVAddr.uiAddr;

			ui32SizeOfConstantsInDWords = MAX(ui32SizeOfConstantsInDWords, GLES2_FRAGMENT_SECATTR_INDEXABLETEMPBASE + 1);	/* PRQA S 3356 */ /* Override QAC suggestion to keep code flexible. */
		}

		if(psRenderState->bHasConstantColorBlend)
		{
			/* Blend constant colour */
			pui32SecAttribs[GLES2_FRAGMENT_SECATTR_FBBLENDCONST] = gc->sState.sRaster.ui32BlendColor;
		
			ui32SizeOfConstantsInDWords = MAX(ui32SizeOfConstantsInDWords, GLES2_FRAGMENT_SECATTR_FBBLENDCONST + 1);	/* PRQA S 3356 */ /* Override QAC suggestion to keep code flexible. */
		}

		if(psRenderState->ui32AlphaTestFlags)
		{
			/* Alpha-test and visibility test*/
			pui32SecAttribs[GLES2_FRAGMENT_SECATTR_ISPFEEDBACK0]   = psRenderState->ui32ISPFeedback0;
			pui32SecAttribs[GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1]   = psRenderState->ui32ISPFeedback1;
		
			ui32SizeOfConstantsInDWords = MAX(ui32SizeOfConstantsInDWords, GLES2_FRAGMENT_SECATTR_ISPFEEDBACK1 + 1);	/* PRQA S 3356 */ /* Override QAC suggestion to keep code flexible. */
		}

		if(psShader->psScratchMem)
		{
			/* Set scratch area base */
			GLES_ASSERT(psPatchedShader->sSARegUse.uFlags & USP_HW_SAREG_USE_FLAG_SCRATCH_USED);

			pui32SecAttribs[GLES2_FRAGMENT_SECATTR_SCRATCHBASE] = psShader->psScratchMem->psMemInfo->sDevVAddr.uiAddr;

			ui32SizeOfConstantsInDWords = MAX(ui32SizeOfConstantsInDWords, GLES2_FRAGMENT_SECATTR_SCRATCHBASE + 1);	/* PRQA S 3356 */ /* Override QAC suggestion to keep code flexible. */
		}
	}

	/* Load texture control words */
	if(psPatchedShader->uRegTexStateCount)
	{
		USP_HW_TEXSTATE_LOAD *psInRegisterTexMap;
		USP_TEX_CTR_WORDS *psInRegisterTex;

		for(i = 0; i < psPatchedShader->uRegTexStateCount; i++)
		{
			IMG_UINT32 ui32Count, ui32Offset, ui32Chunk;

			psInRegisterTexMap = &psPatchedShader->psRegTexStateLoads[i];
			psInRegisterTex = &psPatchedShader->psTextCtrWords[psInRegisterTexMap->uTexCtrWrdIdx];

			ui32Offset = psInRegisterTexMap->uDestIdx;
			ui32Chunk = psInRegisterTex->uChunkIdx + (psInRegisterTex->uTextureIdx * PDS_NUM_TEXTURE_IMAGE_CHUNKS);
			
			for(ui32Count = 0; ui32Count < EURASIA_TAG_TEXTURE_STATE_SIZE; ui32Count++)
			{
				pui32SecAttribs[ui32Offset++] = 
					(pui32TexControlWords[ui32Chunk][ui32Count] & psInRegisterTex->auMask[ui32Count]) | psInRegisterTex->auWord[ui32Count];
			}

			ui32SizeOfConstantsInDWords = MAX(ui32SizeOfConstantsInDWords, ui32Offset);
		}
	}

	/* For those float constants moved to secondary attribute area */
	if(psPatchedShader->uRegConstCount)
	{
		IMG_FLOAT *pfSecAttribs = (IMG_FLOAT *)pui32SecAttribs;
	
		for(i = 0; i < psPatchedShader->uRegConstCount; i++)
		{
			USP_HW_CONST_LOAD *psConstLoad = &psPatchedShader->psRegConstLoads[i];

			switch(psConstLoad->eFormat)
			{
				case USP_HW_CONST_FMT_F32:
				{
					GLES_ASSERT(psConstLoad->uSrcIdx < psSymbolList->uNumCompsUsed);

					/* F32 */
					pfSecAttribs[psConstLoad->uDestIdx] = psShader->pfConstantData[psConstLoad->uSrcIdx]; 
					break;
				}
				case USP_HW_CONST_FMT_F16:
				{
					IMG_UINT16 uF16Value;

					GLES_ASSERT(psConstLoad->uSrcIdx < psSymbolList->uNumCompsUsed);
				
					uF16Value = GLES2ConvertFloatToF16(psShader->pfConstantData[psConstLoad->uSrcIdx]);
			
					uF16Value >>= psConstLoad->uSrcShift;

					pui32SecAttribs[psConstLoad->uDestIdx] &= CLEARMASK(psConstLoad->uDestShift, (16 - psConstLoad->uSrcShift));
					
					pui32SecAttribs[psConstLoad->uDestIdx] |= ((IMG_UINT32)uF16Value << psConstLoad->uDestShift);
					break;
				}
				case USP_HW_CONST_FMT_C10:
				{
					IMG_UINT16 uC10Value;

					GLES_ASSERT(psConstLoad->uSrcIdx < psSymbolList->uNumCompsUsed);

					uC10Value = GLES2ConvertFloatToC10(psShader->pfConstantData[psConstLoad->uSrcIdx]);

					uC10Value = (uC10Value & 0x3FF) >> psConstLoad->uSrcShift;

					pui32SecAttribs[psConstLoad->uDestIdx] &= CLEARMASK(psConstLoad->uDestShift, (10 - psConstLoad->uSrcShift));

					pui32SecAttribs[psConstLoad->uDestIdx] |= ((IMG_UINT32)uC10Value << psConstLoad->uDestShift);
					break;
				}
				case USP_HW_CONST_FMT_STATIC:
				{
					pui32SecAttribs[psConstLoad->uDestIdx] = psConstLoad->uSrcIdx;
					break;
				}
				case USP_HW_CONST_FMT_U8:
				default:
				{
					PVR_DPF((PVR_DBG_ERROR,"Unsupported USP HW Constant format"));
					break;
				}
			}

			ui32SizeOfConstantsInDWords = MAX(ui32SizeOfConstantsInDWords, (IMG_UINT32)(psConstLoad->uDestIdx + 1));
		}

	}

	/* Update buffer position */
	CBUF_UpdateBufferPos(gc->apsBuffers, ui32SizeOfConstantsInDWords, ui32SABufferType);

	if(ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
	{
		GLES2_INC_COUNT(GLES2_TIMER_PDS_VERT_DATA_COUNT, ui32SizeOfConstantsInDWords);		
	}
	else
	{
		GLES2_INC_COUNT(GLES2_TIMER_PDS_FRAG_DATA_COUNT, ui32SizeOfConstantsInDWords);
	}

	/*
	  Record the physical address of the SA-register constants and their size,
	  for use later by the PDS program that copies them to the USE SA registers.
	*/
	*puDeviceAddress      = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32SecAttribs, ui32SABufferType); 
	*pui32DataSizeInDWords = ui32SizeOfConstantsInDWords;

	return GLES2_NO_ERROR;
}

/*****************************************************************************
 Function Name	: WritePDSUSEShaderSAProgram
 Inputs			: psContext, ui32ProgramType
 Outputs		: pbChanged
 Returns		: Mem Error
 Description	: Writes the PDS program for secondary attributes
*****************************************************************************/
static GLES2_MEMERROR WritePDSUSEShaderSAProgram(GLES2Context *gc, IMG_UINT32 ui32ProgramType, IMG_BOOL *pbChanged)
{
	GLES2ProgramShader *psShader;
	USP_HW_SHADER *psPatchedShader;
	GLES2USEShaderVariant *psUSEShaderVariant;
	PDS_PIXEL_SHADER_SA_PROGRAM	sProgram = {0};
	IMG_UINT32	ui32StateSizeInDWords;
	IMG_DEV_VIRTADDR uPDSUSEShaderSAStateAddr;
	IMG_DEV_VIRTADDR uPDSUSEShaderSABaseAddr;
	IMG_UINT32 ui32PDSProgramSizeInDWords;
	IMG_UINT32 *pui32Buffer;
	IMG_UINT32 *pui32BufferBase;
	IMG_UINT32 ui32PDSBufferType;
	UCH_UseCodeBlock *psPixelSecondaryCode = gc->sPrim.psDummyPixelSecondaryPDSCode;
	GLES2_MEMERROR eError;

	if (ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
	{
		psShader = &gc->sProgram.psCurrentProgram->sVertex;
		psUSEShaderVariant = gc->sProgram.psCurrentVertexVariant;
		psPatchedShader = gc->sProgram.psCurrentVertexVariant->psPatchedShader;
		ui32PDSBufferType = CBUF_TYPE_PDS_VERT_BUFFER;
	}
	else
	{
		psShader = &gc->sProgram.psCurrentProgram->sFragment;
		psUSEShaderVariant = gc->sProgram.psCurrentFragmentVariant;
		psPatchedShader = gc->sProgram.psCurrentFragmentVariant->psPatchedShader;
		ui32PDSBufferType = CBUF_TYPE_PDS_FRAG_BUFFER;
	}

	/*
	  Copy any built in uniform data from state to program constants 
	*/
	SetupBuiltInUniforms(gc, ui32ProgramType);

	/*
	  Write the USE shaders constants to a buffer where they can be accessed 
	*/
	eError = WriteUSEShaderMemConsts(gc, ui32ProgramType);

	if(eError != GLES2_NO_ERROR)
	{
		return eError;
	}

	/*
	  Allocate scratch memory if necessary.
	*/
	if(((psPatchedShader->sSARegUse.uFlags & USP_HW_SAREG_USE_FLAG_SCRATCH_USED) != 0) && !psShader->psScratchMem)
	{
		eError = ReserveScratchMemory(gc, ui32ProgramType);

		if(eError != GLES2_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"LoadSecondaryAttributes: Could not alloc mem for scratch area"));
			return eError;
		}
	}

	if(psShader->psScratchMem && !psUSEShaderVariant->psScratchMem)
	{
		psUSEShaderVariant->psScratchMem = psShader->psScratchMem;

		ShaderScratchMemAddRef(gc, psUSEShaderVariant->psScratchMem);
	}

	/*
	  Allocate memory for indexable temps if they are used.
	*/
	if(((psPatchedShader->sSARegUse.uFlags & USP_HW_SAREG_USE_FLAG_INDEXEDTEMPS_USED) != 0) && !psShader->psIndexableTempsMem)
	{
		eError = ReserveMemoryForIndexableTemps(gc, ui32ProgramType);

		if(eError != GLES2_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"LoadSecondaryAttributes: Could not alloc mem for indexable temps"));
			return eError;
		}
	}

	if(psShader->psIndexableTempsMem && !psUSEShaderVariant->psIndexableTempsMem)
	{
		psUSEShaderVariant->psIndexableTempsMem = psShader->psIndexableTempsMem;

		ShaderIndexableTempsMemAddRef(gc, psUSEShaderVariant->psIndexableTempsMem);
	}

	/*
	  Loads the secondary attributes and records the virtual address of the SA-register constants and their size,
	  for use later by the PDS program that copies them to the USE SA registers.
	*/
	eError = LoadSecondaryAttributes(gc, ui32ProgramType, &uPDSUSEShaderSAStateAddr, &ui32StateSizeInDWords);
	
	if(eError != GLES2_NO_ERROR)
	{
		return eError;
	}

	/* Save the secondary attribute size (ensure the total includes any constants generated by the 
	 * secondary USSE code).
	 */
	psShader->ui32USESecAttribDataSizeInDwords = MAX(ui32StateSizeInDWords, psPatchedShader->uSARegCount);

	if (psShader->ui32USESecAttribDataSizeInDwords)
	{
		/*
			Fill out the DMA control for the PDS.
		*/
		if(ui32StateSizeInDWords)
		{
			sProgram.ui32NumDMAKicks = EncodeDmaBurst(sProgram.aui32DMAControl, 0, ui32StateSizeInDWords, uPDSUSEShaderSAStateAddr);
		}
	
		/* There is a secondary USSE program to run */
		if(psShader->psSharedState->psSecondaryUploadTask)
		{
			IMG_UINT32 uUSETempCount = psPatchedShader->uSecTempRegCount;
			IMG_UINT32 ui32SDSoft = 
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			                        EURASIA_PDS_DOUTU1_SDSOFT;
#else
			                        EURASIA_PDS_DOUTU2_SDSOFT;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

			/* Align to temp allocation granularity */
			uUSETempCount = ALIGNCOUNTINBLOCKS(uUSETempCount, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);

			/* Setup USE task control */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			sProgram.aui32USETaskControl[0]	= (uUSETempCount << EURASIA_PDS_DOUTU0_TRC_SHIFT) & ~EURASIA_PDS_DOUTU0_TRC_CLRMSK;
			sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL |
			                                  ui32SDSoft                       ;
			sProgram.aui32USETaskControl[2]	= 0;
#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */
			sProgram.aui32USETaskControl[0]	= EURASIA_PDS_DOUTU0_PDSDMADEPENDENCY;
			sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL |
											((uUSETempCount << EURASIA_PDS_DOUTU1_TRC_SHIFT) & ~EURASIA_PDS_DOUTU1_TRC_CLRMSK);
			sProgram.aui32USETaskControl[2]	= ((uUSETempCount >> EURASIA_PDS_DOUTU2_TRC_INTERNALSHIFT) << EURASIA_PDS_DOUTU2_TRC_SHIFT) | 
											  ui32SDSoft;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

			if(ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
			{
				SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
										0,
										psShader->psSharedState->psSecondaryUploadTask->psSecondaryCodeBlock->sCodeAddress, 
										gc->psSysContext->uUSEVertexHeapBase, 
										SGX_VTXSHADER_USE_CODE_BASE_INDEX);
			}
			else
			{
				SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
										0, 
										psShader->psSharedState->psSecondaryUploadTask->psSecondaryCodeBlock->sCodeAddress, 
										gc->psSysContext->uUSEFragmentHeapBase, 
										SGX_PIXSHADER_USE_CODE_BASE_INDEX);
			}

			sProgram.bKickUSE = IMG_TRUE;
		}
	}
	else if(ui32ProgramType == GLES2_SHADERTYPE_FRAGMENT)
	{
		GLES2PDSInfo *psPDSInfo	= &gc->sProgram.psCurrentFragmentVariant->u.sFragment.sPDSInfo;

		IMG_DEV_VIRTADDR uBaseAddr = psPixelSecondaryCode->sCodeAddress;
		
#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
		uBaseAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

		/*
			Set up the secondary PDS program fields in the pixel shader
			data master information word.
		*/
		psPDSInfo->ui32DMSInfo &= EURASIA_PDS_SECATTRSIZE_CLRMSK & (~EURASIA_PDS_USE_SEC_EXEC);

		if((gc->sPrim.uFragmentPDSSecAttribBaseAddress.uiAddr!=uBaseAddr.uiAddr) ||
		   (gc->sPrim.ui32FragmentPDSSecAttribDataSize!=gc->sPrim.ui32DummyPDSPixelSecondaryDataSize))
		{
			/*
				Save the PDS pixel shader data segment address and size (always offset by PDS exec base)
			*/
			gc->sPrim.ui32FragmentPDSSecAttribDataSize = gc->sPrim.ui32DummyPDSPixelSecondaryDataSize;
			gc->sPrim.uFragmentPDSSecAttribBaseAddress = uBaseAddr;

			*pbChanged = IMG_TRUE;
		}
		else
		{
			*pbChanged = IMG_FALSE;
		}

		return GLES2_NO_ERROR;
	}

	if(!sProgram.bKickUSE
#if !defined(SGX_FEATURE_SECONDARY_REQUIRES_USE_KICK)
		&& (ui32StateSizeInDWords > 0)
#endif
		)
	{
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		sProgram.aui32USETaskControl[0]	= 0 << EURASIA_PDS_DOUTU0_TRC_SHIFT;
		sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
		sProgram.aui32USETaskControl[2]	= 0;
#else 
		sProgram.aui32USETaskControl[0]	= 0;
		sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL | (0 << EURASIA_PDS_DOUTU1_TRC_SHIFT);
		sProgram.aui32USETaskControl[2]	= 0;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

		if(ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
		{
			SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
									0,
									gc->sProgram.psDummyVertUSECode->sDevVAddr, 
									gc->psSysContext->uUSEVertexHeapBase, 
									SGX_VTXSHADER_USE_CODE_BASE_INDEX);

		}
		else
		{
			SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
									0, 
#if defined(FIX_HW_BRN_31988)		
									gc->sProgram.psBRN31988FragUSECode->sDevVAddr, 
#else
									gc->sProgram.psDummyFragUSECode->sDevVAddr, 
#endif
									gc->psSysContext->uUSEFragmentHeapBase, 
									SGX_PIXSHADER_USE_CODE_BASE_INDEX);
		}


		/* Kick a USE dummy program (all secondary tasks with DMA must have a USSE program executed on them) */
		sProgram.bKickUSEDummyProgram = IMG_TRUE;
	}

	/*
		Get memory for the PDS program
	*/
#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
	ui32PDSProgramSizeInDWords = 20 + sProgram.ui32NumDMAKicks + 3;
#else
	ui32PDSProgramSizeInDWords = 12 + sProgram.ui32NumDMAKicks + 3;
#endif

	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, ui32PDSProgramSizeInDWords, ui32PDSBufferType, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return ((ui32PDSBufferType == CBUF_TYPE_PDS_VERT_BUFFER) ? GLES2_TA_BUFFER_ERROR : GLES2_3D_BUFFER_ERROR);
	}

	uPDSUSEShaderSABaseAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, ui32PDSBufferType);


	/*
		Generate the PDS pixel state program
	*/
	pui32Buffer = (IMG_UINT32 *)PDSGeneratePixelShaderSAProgram(&sProgram, pui32BufferBase);

	/*
		Update buffer position 
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), ui32PDSBufferType);

	if(ui32ProgramType == GLES2_SHADERTYPE_VERTEX)
	{
		GLES2_INC_COUNT(GLES2_TIMER_PDS_VERT_DATA_COUNT, (pui32Buffer - pui32BufferBase));		
	}
	else
	{
		GLES2_INC_COUNT(GLES2_TIMER_PDS_FRAG_DATA_COUNT, (pui32Buffer - pui32BufferBase));
	}

	if (ui32ProgramType == GLES2_SHADERTYPE_FRAGMENT)
	{
		GLES2PDSInfo *psPDSInfo	= &gc->sProgram.psCurrentFragmentVariant->u.sFragment.sPDSInfo;
		IMG_UINT32 ui32SecAttribAllocation;

		/*
			Set up the secondary PDS program fields in the pixel shader
			data master information word.
		*/
		psPDSInfo->ui32DMSInfo &= EURASIA_PDS_SECATTRSIZE_CLRMSK & (~EURASIA_PDS_USE_SEC_EXEC);

		ui32SecAttribAllocation = ALIGNCOUNTINBLOCKS(psShader->ui32USESecAttribDataSizeInDwords, EURASIA_PDS_SECATTRSIZE_ALIGNSHIFTINREGISTERS);

		psPDSInfo->ui32DMSInfo |= (ui32SecAttribAllocation << EURASIA_PDS_SECATTRSIZE_SHIFT) | EURASIA_PDS_USE_SEC_EXEC;
		
		/*
			Save the PDS pixel shader data segment address and size
		*/
		gc->sPrim.uFragmentPDSSecAttribBaseAddress = uPDSUSEShaderSABaseAddr;
		gc->sPrim.ui32FragmentPDSSecAttribDataSize = sProgram.ui32DataSize;

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
		/* Adjust PDS execution address for restricted address range */
		gc->sPrim.uFragmentPDSSecAttribBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif
	}
	else
	{
		/*
			Save the PDS vertex shader data segment physical address and size
		*/
		gc->sPrim.uVertexPDSSecAttribBaseAddress = uPDSUSEShaderSABaseAddr;
		gc->sPrim.ui32VertexPDSSecAttribDataSize = sProgram.ui32DataSize;
	
#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
		/* Adjust PDS execution address for restricted address range */
		gc->sPrim.uVertexPDSSecAttribBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

	}

	*pbChanged = IMG_TRUE;

	return GLES2_NO_ERROR;
}


/*****************************************************************************
 Function Name	: WriteMTEState
 Inputs			: 
 Outputs		: none
 Returns		: Mem Error
 Description	: 
*****************************************************************************/
static GLES2_MEMERROR WriteMTEState(GLES2Context *gc,
								IMG_UINT32 ePrimitiveType,
								IMG_UINT32 *pui32DWordsWritten,
								IMG_DEV_VIRTADDR *psStateAddr)
{
	IMG_UINT32	ui32USEFormatHeader = 0, *pui32Buffer, *pui32BufferBase;
	IMG_UINT32	ui32ISPAPrimitiveType;
	IMG_UINT32  ui32StateDWords = GLES2_MAX_MTE_STATE_DWORDS;
	IMG_FLOAT fWClamp = 0.00001f;
	GLES2CompiledRenderState *psRenderState = &gc->sPrim.sRenderState;
	GLES2USEShaderVariant *psFragmentVariant = gc->sProgram.psCurrentFragmentVariant;
	GLES2PDSInfo *psPDSInfo = &psFragmentVariant->u.sFragment.sPDSInfo;
	GLES2Program *psProgram = gc->sProgram.psCurrentProgram;

	/*
		Get buffer space for the actual output state
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, 
									ui32StateDWords, 
									CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES2_TA_BUFFER_ERROR;
	}
	
	/*
		Record the buffer base (for the header) and advance pointer past the header
	*/
	pui32Buffer = pui32BufferBase;

	pui32Buffer++;

	
	/* ========= */
	/* ISP state words */
	/* ========= */
	if(gc->ui32EmitMask & GLES2_EMITSTATE_MTE_STATE_ISP)
	{
		IMG_UINT32 ui32ISPWordA = psRenderState->ui32ISPControlWordA;
		
		if((gc->psDrawParams->eRotationAngle==PVRSRV_FLIP_Y) && (ePrimitiveType==GLES2_PRIMTYPE_SPRITE))
		{
			/* Point sprites need to be flipped in this case */
			ui32ISPAPrimitiveType = EURASIA_ISPA_OBJTYPE_SPRITE10UV;
		}
		else
		{
		    ui32ISPAPrimitiveType = aui32GLES2PrimToISPPrim[ePrimitiveType];
		}

		ui32USEFormatHeader |= EURASIA_TACTLPRES_ISPCTLFF0;

		ui32ISPWordA &= EURASIA_ISPA_OBJTYPE_CLRMSK;
		ui32ISPWordA |= ui32ISPAPrimitiveType;

		*pui32Buffer++ = ui32ISPWordA;

		if(ui32ISPWordA & EURASIA_ISPA_BPRES) 
		{
			ui32USEFormatHeader |= EURASIA_TACTLPRES_ISPCTLFF1;
			*pui32Buffer++ = psRenderState->ui32ISPControlWordB;
		}

		if(ui32ISPWordA & EURASIA_ISPA_CPRES) 
		{
			ui32USEFormatHeader |= EURASIA_TACTLPRES_ISPCTLFF2;
			*pui32Buffer++ = psRenderState->ui32ISPControlWordC;
		}

		if(psRenderState->ui32BFISPControlWordA)
		{
			ui32USEFormatHeader |= EURASIA_TACTLPRES_ISPCTLBF2 | 
									EURASIA_TACTLPRES_ISPCTLBF1 |
									EURASIA_TACTLPRES_ISPCTLBF0;

			ui32ISPWordA = psRenderState->ui32BFISPControlWordA;
	
			ui32ISPWordA &= EURASIA_ISPA_OBJTYPE_CLRMSK;
			ui32ISPWordA |= ui32ISPAPrimitiveType;

			*pui32Buffer++ = ui32ISPWordA;
			*pui32Buffer++ = psRenderState->ui32BFISPControlWordB;
			*pui32Buffer++ = psRenderState->ui32BFISPControlWordC;
		}
	}


	/* ========= */
	/* PDS state */
	/* ========= */
	if(gc->ui32EmitMask & (GLES2_EMITSTATE_PDS_FRAGMENT_STATE | GLES2_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE))
	{
		IMG_UINT32 ui32DMSInfo = psPDSInfo->ui32DMSInfo;

#if defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
		ui32DMSInfo |=	((gc->sPrim.uFragmentPDSSecAttribBaseAddress.uiAddr & EURASIA_PDS_BASEADD_MSB) ? EURASIA_PDS_BASEADD_SEC_MSB : 0) |
						((gc->sPrim.uFragmentPDSBaseAddress.uiAddr & EURASIA_PDS_BASEADD_MSB) ? EURASIA_PDS_BASEADD_PRIM_MSB : 0);
#endif

		ui32USEFormatHeader |= EURASIA_TACTLPRES_PDSSTATEPTR;

		/*
			Write the PDS pixel shader secondary attribute base address and data size
		*/
		*pui32Buffer++	= (((gc->sPrim.uFragmentPDSSecAttribBaseAddress.uiAddr >> EURASIA_PDS_BASEADD_ALIGNSHIFT) 
							<< EURASIA_PDS_BASEADD_SHIFT) & ~EURASIA_PDS_BASEADD_CLRMSK)							|
							(((gc->sPrim.ui32FragmentPDSSecAttribDataSize >> EURASIA_PDS_DATASIZE_ALIGNSHIFT) 
							<< EURASIA_PDS_DATASIZE_SHIFT) & ~EURASIA_PDS_DATASIZE_CLRMSK);

		/*
			Write the DMS pixel control information
		*/
		*pui32Buffer++ = ui32DMSInfo;

		/*
			Write the PDS pixel shader physical base address and data size
		*/
		*pui32Buffer++	= (((gc->sPrim.uFragmentPDSBaseAddress.uiAddr >> EURASIA_PDS_BASEADD_ALIGNSHIFT) 
						<< EURASIA_PDS_BASEADD_SHIFT) & ~EURASIA_PDS_BASEADD_CLRMSK)						|
						(((gc->sPrim.ui32FragmentPDSDataSize >> EURASIA_PDS_DATASIZE_ALIGNSHIFT) 
						<< EURASIA_PDS_DATASIZE_SHIFT) & ~EURASIA_PDS_DATASIZE_CLRMSK);
	}


	/* ========= */
	/* Region Clip */
	/* ========= */
	if(gc->ui32EmitMask & GLES2_EMITSTATE_MTE_STATE_REGION_CLIP)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_RGNCLIP;

		*pui32Buffer++	= gc->psRenderSurface->aui32RegionClipWord[0];
		*pui32Buffer++	= gc->psRenderSurface->aui32RegionClipWord[1];
	}


	/* ========= */
	/* View Port */
	/* ========= */
	if(gc->ui32EmitMask & GLES2_EMITSTATE_MTE_STATE_VIEWPORT)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_VIEWPORT;

		*pui32Buffer++ = FLOAT_TO_UINT32(gc->sState.sViewport.fXCenter); 
		*pui32Buffer++ = FLOAT_TO_UINT32(gc->sState.sViewport.fXScale);
		*pui32Buffer++ = FLOAT_TO_UINT32(gc->sState.sViewport.fYCenter);
		*pui32Buffer++ = FLOAT_TO_UINT32(gc->sState.sViewport.fYScale);
		*pui32Buffer++ = FLOAT_TO_UINT32(gc->sState.sViewport.fZCenter);
		*pui32Buffer++ = FLOAT_TO_UINT32(gc->sState.sViewport.fZScale);
	}


	/* ========= */
	/* Texture Wrapping Control */
	/* ========= */
	if(gc->ui32EmitMask & GLES2_EMITSTATE_MTE_STATE_TAMISC)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_WRAP;

		*pui32Buffer++	= 0;
	}


	/* ========= */
	/* Output Selects */
	/* ========= */
	if (gc->ui32EmitMask & GLES2_EMITSTATE_MTE_STATE_OUTPUTSELECTS)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_OUTSELECTS;
	
		*pui32Buffer++	= psProgram->ui32OutputSelects;
	}


	/* ========= */
	/*	W Clamping */
	/* ========= */
	if(gc->ui32EmitMask & GLES2_EMITSTATE_MTE_STATE_TAMISC)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_WCLAMP;

		*pui32Buffer++ = FLOAT_TO_UINT32(fWClamp);
	}


	/* ========= */
	/*	MTE control */
	/* ========= */
	if(gc->ui32EmitMask & GLES2_EMITSTATE_MTE_STATE_CONTROL)
	{
		IMG_UINT32 ui32MTEControl = psRenderState->ui32MTEControl;

		ui32USEFormatHeader |= EURASIA_TACTLPRES_MTECTRL;

/* PRQA S 3332 1 */ /* Override QAC suggestion and use this macro. */

		*pui32Buffer++	= ui32MTEControl;
	}


	/* ========= */
	/*	Texture coordinate float and size */
	/* ========= */
	if (gc->ui32EmitMask & GLES2_EMITSTATE_MTE_STATE_OUTPUTSELECTS)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_TEXSIZE | EURASIA_TACTLPRES_TEXFLOAT;

		*pui32Buffer++	= psProgram->ui32TexCoordSelects;

		/* Controls 16/32bit texture coordinate format */ 
		*pui32Buffer++ = psProgram->ui32TexCoordPrecision;
	}


	/*
		If any state to be issued  
	*/
	if (ui32USEFormatHeader)
	{
		ui32StateDWords = (IMG_UINT32)(pui32Buffer - pui32BufferBase);
	}
	else
	{
		ui32StateDWords = 0;
	}

	/* Update PDS buffer position */
	CBUF_UpdateBufferPos(gc->apsBuffers, ui32StateDWords, CBUF_TYPE_PDS_VERT_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_PDS_VERT_DATA_COUNT, ui32StateDWords);

	GLES2_INC_COUNT(GLES2_TIMER_MTE_STATE_COUNT, ui32StateDWords);

	/* Write the format header */
	*pui32BufferBase = ui32USEFormatHeader;

	/*
		Get the address of the state data in video memory.
	*/
	*psStateAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_PDS_VERT_BUFFER);

	*pui32DWordsWritten = ui32StateDWords;

	return GLES2_NO_ERROR; 
}

/*****************************************************************************
 Function Name	: PatchPregenMTEStateCopyPrograms
 Inputs			: gc		- pointer to the context
 Outputs		: none
 Returns		: Mem Error
 Description	: patch the pregenerated PDS state program
*****************************************************************************/
IMG_INTERNAL GLES2_MEMERROR PatchPregenMTEStateCopyPrograms(GLES2Context	*gc,
															IMG_UINT32 ui32StateSizeInDWords,
															IMG_DEV_VIRTADDR	uStateDataHWAddress)
{
	IMG_UINT32 *pui32BufferBase;
	IMG_DEV_VIRTADDR uUSEExecutionHWAddress;
	PDS_STATE_COPY_PROGRAM	sProgram;
	
	GLES2MemSet(&sProgram, 0, sizeof(PDS_STATE_COPY_PROGRAM));

	uUSEExecutionHWAddress = GetStateCopyUSEAddress(gc, ui32StateSizeInDWords);

	/*
		Setup the parameters for the PDS program.
	*/
	sProgram.ui32NumDMAKicks = EncodeDmaBurst(sProgram.aui32DMAControl, 0, ui32StateSizeInDWords, uStateDataHWAddress);	/* PRQA S 3199 */

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sProgram.aui32USETaskControl[0] = (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU0_TRC_SHIFT);
	sProgram.aui32USETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	sProgram.aui32USETaskControl[2]	= 0;
#else
	sProgram.aui32USETaskControl[0]	= EURASIA_PDS_DOUTU0_PDSDMADEPENDENCY;
	sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL | 
									  (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU1_TRC_SHIFT);
	sProgram.aui32USETaskControl[2]	= 0;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
							0,
							uUSEExecutionHWAddress, 
							gc->psSysContext->uUSEVertexHeapBase, 
							SGX_VTXSHADER_USE_CODE_BASE_INDEX);

	sProgram.ui32DataSize =  PDS_MTESTATECOPY_DATA_SEGMENT_SIZE;

	/*
		Get buffer space for the PDS state copy program
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, GLES2_ALIGNED_MTE_COPY_PROG_SIZE, CBUF_TYPE_MTE_COPY_PREGEN_BUFFER, IMG_FALSE);
	
	if(!pui32BufferBase)
	{
		return GLES2_TA_BUFFER_ERROR;
	}

	/*
		patch the PDS state copy program
	*/
	PDSMTEStateCopySetDMA0(pui32BufferBase, sProgram.aui32DMAControl[0]);
	PDSMTEStateCopySetDMA1(pui32BufferBase, sProgram.aui32DMAControl[1]);
	PDSMTEStateCopySetDMA2(pui32BufferBase, sProgram.aui32DMAControl[2]);
	PDSMTEStateCopySetDMA3(pui32BufferBase, sProgram.aui32DMAControl[3]);
	
	PDSMTEStateCopySetUSE0(pui32BufferBase, sProgram.aui32USETaskControl[0]);
	PDSMTEStateCopySetUSE1(pui32BufferBase, sProgram.aui32USETaskControl[1]);

	/*
		As long as USE2 is 0 we don't need to patch it,
		the template program has already it set to 0

		pui32Buffer[10] = sProgram.aui32USETaskControl[2];
	*/

	/*
	   Update buffer position for the PDS pixel program.
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, GLES2_ALIGNED_MTE_COPY_PROG_SIZE, CBUF_TYPE_MTE_COPY_PREGEN_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_MTE_COPY_COUNT, 6);

	/*
		Save the PDS pixel shader program physical base address and data size
	*/
	gc->sPrim.uOutputStatePDSBaseAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, 
																  CBUF_TYPE_MTE_COPY_PREGEN_BUFFER);

	gc->sPrim.ui32OutputStatePDSDataSize    = sProgram.ui32DataSize;
	gc->sPrim.ui32OutputStateUSEAttribSize  = ui32StateSizeInDWords;

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
	/* Adjust PDS execution address for restricted address range */
	gc->sPrim.uOutputStatePDSBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif
#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	gc->sPrim.ui32OutputStateUSEAttribSize += SGX_USE_MINTEMPREGS;
#endif

	return GLES2_NO_ERROR;
}


/*****************************************************************************
 Function Name	: WriteMTEStateCopyPrograms
 Inputs			: gc		- pointer to the context
 Outputs		: none
 Returns		: Mem Error
 Description	: Writes the PDS state program
*****************************************************************************/
IMG_INTERNAL GLES2_MEMERROR WriteMTEStateCopyPrograms(GLES2Context	*gc,
														IMG_UINT32 ui32StateSizeInDWords,
														IMG_DEV_VIRTADDR	uStateDataHWAddress)
{
	IMG_UINT32 *pui32BufferBase, *pui32Buffer;
	IMG_DEV_VIRTADDR uUSEExecutionHWAddress;
	PDS_STATE_COPY_PROGRAM	sProgram = {0};

	uUSEExecutionHWAddress = GetStateCopyUSEAddress(gc, ui32StateSizeInDWords);

	/*
		Setup the parameters for the PDS program.
	*/
	sProgram.ui32NumDMAKicks = EncodeDmaBurst(sProgram.aui32DMAControl, 0, ui32StateSizeInDWords, uStateDataHWAddress);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sProgram.aui32USETaskControl[0]	= (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU0_TRC_SHIFT);
	sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	sProgram.aui32USETaskControl[2]	= 0;
#else
	sProgram.aui32USETaskControl[0]	= EURASIA_PDS_DOUTU0_PDSDMADEPENDENCY;
	sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL | 
									  (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU1_TRC_SHIFT);
	sProgram.aui32USETaskControl[2]	= 0;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
							0,
							uUSEExecutionHWAddress, 
							gc->psSysContext->uUSEVertexHeapBase, 
							SGX_VTXSHADER_USE_CODE_BASE_INDEX);

	/*
		Get buffer space for the PDS state copy program
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, 
									   EURASIA_NUM_PDS_STATE_PROGRAM_DWORDS, 
									   CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);
	
	if(!pui32BufferBase)
	{
		return GLES2_TA_BUFFER_ERROR;
	}

	pui32Buffer = pui32BufferBase;

	/*
		Generate the PDS state copy program
	*/
	pui32Buffer = PDSGenerateStateCopyProgram(&sProgram, pui32Buffer);

	/* 
	   Update buffer position for the PDS pixel program.
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_VERT_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_PDS_VERT_DATA_COUNT, (pui32Buffer - pui32BufferBase));

	/*
		Save the PDS pixel shader program physical base address and data size
	*/
	gc->sPrim.uOutputStatePDSBaseAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers, (IMG_UINT32 *)sProgram.pui32DataSegment, 
																  CBUF_TYPE_PDS_VERT_BUFFER);

	gc->sPrim.ui32OutputStatePDSDataSize    = sProgram.ui32DataSize;
	gc->sPrim.ui32OutputStateUSEAttribSize    = ui32StateSizeInDWords;

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
	/* Adjust PDS execution address for restricted address range */
	gc->sPrim.uOutputStatePDSBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif
#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	gc->sPrim.ui32OutputStateUSEAttribSize += SGX_USE_MINTEMPREGS;
#endif

	return GLES2_NO_ERROR;
}


/*****************************************************************************
 Function Name	: SetupStateUpdate
 Inputs			: bMTEStateUpdate
 Outputs		: none
 Returns		: none
 Description	: Writes the state update block (either MTE update or vertex
				  secondary update) to the VDM control stream/
*****************************************************************************/
IMG_INTERNAL GLES2_MEMERROR SetupStateUpdate(GLES2Context *gc, IMG_BOOL bMTEStateUpdate)
{
	IMG_UINT32 *pui32BufferBase, *pui32Buffer;
	GLES2ProgramShader *psVertexShader = &gc->sProgram.psCurrentProgram->sVertex;
	
	/* If there are no vertex secondaries, just return */
	if(!bMTEStateUpdate && !psVertexShader->ui32USESecAttribDataSizeInDwords)
	{
		return GLES2_NO_ERROR;
	}

	/*
		Get VDM control stream space for the PDS vertex state block
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, CBUF_MAX_DWORDS_PER_SECONDARY_STATE_BLOCK, CBUF_TYPE_VDM_CTRL_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES2_TA_BUFFER_ERROR;
	}

	pui32Buffer = pui32BufferBase;

	if(bMTEStateUpdate)
	{
		/*
			Write the MTE state update PDS data block
		*/
		*pui32Buffer++	= (EURASIA_TAOBJTYPE_STATE                      |
						(((gc->sPrim.uOutputStatePDSBaseAddress.uiAddr >> EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) 
					<< EURASIA_TAPDSSTATE_BASEADDR_SHIFT) & ~EURASIA_TAPDSSTATE_BASEADDR_CLRMSK));
		
		*pui32Buffer++	= ((((gc->sPrim.ui32OutputStatePDSDataSize >> EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) 
							<< EURASIA_TAPDSSTATE_DATASIZE_SHIFT) & ~EURASIA_TAPDSSTATE_DATASIZE_CLRMSK) |
						(EURASIA_TAPDSSTATE_USEPIPE_1 + gc->sPrim.ui32CurrentOutputStateBlockUSEPipe) |
#if defined(SGX545)
						(2UL << EURASIA_TAPDSSTATE_PARTITIONS_SHIFT) |
#else
						(1UL << EURASIA_TAPDSSTATE_PARTITIONS_SHIFT) |
#endif
						EURASIA_TAPDSSTATE_SD                        |
						EURASIA_TAPDSSTATE_MTE_EMIT					|
						(ALIGNCOUNTINBLOCKS(gc->sPrim.ui32OutputStateUSEAttribSize, EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS) << EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_SHIFT));

		/* Switch USE pipes for the next block */
		gc->sPrim.ui32CurrentOutputStateBlockUSEPipe++;
		gc->sPrim.ui32CurrentOutputStateBlockUSEPipe %= EURASIA_TAPDSSTATE_PIPECOUNT;

	}
	else
	{
		IMG_UINT32 ui32StateWord0, ui32StateWord1;

		/*
			Write the vertex secondary state update PDS data block
		*/
		ui32StateWord0	= EURASIA_TAOBJTYPE_STATE |
						(((gc->sPrim.uVertexPDSSecAttribBaseAddress.uiAddr >> EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) 
						<< EURASIA_TAPDSSTATE_BASEADDR_SHIFT) & ~EURASIA_TAPDSSTATE_BASEADDR_CLRMSK);

		ui32StateWord1	= (((gc->sPrim.ui32VertexPDSSecAttribDataSize >> EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) 
							<< EURASIA_TAPDSSTATE_DATASIZE_SHIFT) & ~EURASIA_TAPDSSTATE_DATASIZE_CLRMSK) |
#if !defined(SGX545)
						EURASIA_TAPDSSTATE_SEC_EXEC | 
#endif
						EURASIA_TAPDSSTATE_SECONDARY |
						(ALIGNCOUNTINBLOCKS(psVertexShader->ui32USESecAttribDataSizeInDwords, EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS) 
						<< EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_SHIFT);

#if defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690)
		{
			IMG_UINT32 i;
			
			for (i = 0; i < SGX_FEATURE_NUM_USE_PIPES; i++)
			{
				*pui32Buffer++	= ui32StateWord0;
				*pui32Buffer++	= ui32StateWord1 | aui32USEPipe[i];
			}
		}
#else /* defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690) */
		*pui32Buffer++	= ui32StateWord0;
		*pui32Buffer++	= ui32StateWord1 | EURASIA_TAPDSSTATE_USEPIPE_ALL;
#endif /* defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690) */

	}

	/*
		Lock TA control stream space for the PDS vertex state block
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_VDM_CTRL_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_VDM_CTRL_STATE_COUNT, (pui32Buffer - pui32BufferBase));

	return GLES2_NO_ERROR;
}

/***********************************************************************************
 Function Name      : SetupMTEPregenBuffer
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : fill MTE pregen circular buffer with PDS program template 
************************************************************************************/
IMG_INTERNAL GLES2_MEMERROR SetupMTEPregenBuffer(GLES2Context *gc)
{
	IMG_UINT32 ui32Count, ui32AlignedMTECopyProgramSizeInBytes;
	IMG_UINT32 *pui32BufferBase;
	IMG_UINT32 ui32BufferSize;	
	
	ui32BufferSize = gc->apsBuffers[CBUF_TYPE_MTE_COPY_PREGEN_BUFFER]->ui32BufferLimitInBytes;

	ui32AlignedMTECopyProgramSizeInBytes = (GLES2_ALIGNED_MTE_COPY_PROG_SIZE << 2);

	/* 
		fill buffer with template auxiliary program
	*/
	pui32BufferBase = gc->apsBuffers[CBUF_TYPE_MTE_COPY_PREGEN_BUFFER]->pui32BufferBase;	

	for(ui32Count = 0; ui32Count < ui32BufferSize; ui32Count += ui32AlignedMTECopyProgramSizeInBytes)
	{
		GLES2MemCopy(pui32BufferBase + (ui32Count >> 2), g_pui32PDSMTEStateCopy, PDS_MTESTATECOPY_SIZE);		
	}

	return GLES2_NO_ERROR;
}


/***********************************************************************************
 Function Name      : SetupVAOAttribPointers
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Sets up VAO's attribute copy pointers based on current state
************************************************************************************/
static IMG_VOID SetupVAOAttribPointers(GLES2Context *gc)
{
 	IMG_UINT32 i;
	GLES2VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);

	GLES_ASSERT(gc->sVAOMachine.psActiveVAO);

	for(i=0; i<psVAOMachine->ui32NumItemsPerVertex; i++)
	{
	    GLES2AttribArrayPointerMachine *psVAOAPMachine = psVAOMachine->apsPackedAttrib[i];

		if (psVAOAPMachine->bIsCurrentState)
		{
			IMG_UINT32 ui32CurrentVAOAttrib = (IMG_UINT32)(psVAOAPMachine - &(psVAOMachine->asAttribPointer[0]));
			
			psVAOAPMachine->pui8CopyPointer = (IMG_VOID *)&(psVAOMachine->asCurrentAttrib[ui32CurrentVAOAttrib]);
		}
		else
		{
			psVAOAPMachine->pui8CopyPointer = psVAOAPMachine->psState->pui8Pointer;
		}
	}
}  
		

/* Size of attribs in bytes, by type */
static const IMG_UINT32 aui32AttribSize[GLES2_STREAMTYPE_MAX] =
{
	1,		/* GLES2_STREAMTYPE_BYTE */
	1,		/* GLES2_STREAMTYPE_UBYTE */
	2,		/* GLES2_STREAMTYPE_SHORT */
	2,		/* GLES2_STREAMTYPE_USHORT */
	4,		/* GLES2_STREAMTYPE_FLOAT */
	2,		/* GLES2_STREAMTYPE_HALFFLOAT */
	4		/* GLES2_STREAMTYPE_FIXED */
};


/***********************************************************************************
 Function Name      : SetupVAOAttribStreams
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Sets up VAO's attribute streams based on current state
************************************************************************************/
static IMG_VOID SetupVAOAttribStreams(GLES2Context *gc)
{
	IMG_UINT32 ui32NumItemsPerVertex, ui32CompileMask, i;
	GLES2VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);
	GLES2VertexArrayObject *psVAO = psVAOMachine->psActiveVAO;

	GLES_ASSERT(psVAO);

	/* Initialise other parameters */
	gc->ui32VertexSize	 = 0;
	gc->ui32VertexRCSize = 0;
	gc->ui32VertexAlignSize = 0;

	ui32NumItemsPerVertex = 0;
	ui32CompileMask = gc->sProgram.psCurrentProgram->ui32AttribCompileMask;

	/* Setup VAO's control word */
	psVAOMachine->ui32ControlWord = 0;
 
	/* Setup VAO's attribute states */
	for (i=0; i<AP_VERTEX_ATTRIB_MAX; i++)
	{
		if (ui32CompileMask & (1U << i))
		{
			GLES2AttribArrayPointerMachine *psVAOAPMachine = &(psVAOMachine->asAttribPointer[i]);

		    GLES2AttribArrayPointerState *psVAOAPState = &(psVAO->asVAOState[i]);

			/* Use VAO Machine's array enables item, 
			   which has already been set in ValidateState() */
			if (psVAOMachine->ui32ArrayEnables & (1U << i))
			{
				IMG_UINT32 ui32StreamTypeSize = psVAOAPState->ui32StreamTypeSize;
				IMG_UINT32 ui32UserStride = psVAOAPState->ui32UserStride;

				IMG_UINT32 ui32Size = (ui32StreamTypeSize >> GLES2_STREAMSIZE_SHIFT) *
				                      aui32AttribSize[ui32StreamTypeSize & GLES2_STREAMTYPE_MASK];

				IMG_UINT32 ui32Stride = ui32UserStride ? ui32UserStride : ui32Size;
				
				psVAOAPMachine->ui32Size = ui32Size;
				psVAOAPMachine->ui32Stride = ui32Stride;

				psVAOAPMachine->bIsCurrentState = IMG_FALSE;

				/* Setup VAO's buffer objects */
				if (psVAOAPState->psBufObj)
				{
					psVAOMachine->ui32ControlWord |= ATTRIBARRAY_SOURCE_BUFOBJ;
 
					/* There is a buffer object with no memory bound. */
					if(psVAOAPState->psBufObj->psMemInfo == IMG_NULL)
					{
						psVAOMachine->ui32ControlWord |= ATTRIBARRAY_BAD_BUFOBJ;
					}

					if (psVAOAPState->psBufObj->bMapped)
					{
						psVAOMachine->ui32ControlWord |= ATTRIBARRAY_MAP_BUFOBJ;
					}
				}
				else
				{
				    if (VAO_IS_ZERO(gc)) /* Setup the default VAO's client arrays */
					{
						IMG_UINT32 ui32UserSize = (psVAOAPState->ui32StreamTypeSize >> GLES2_STREAMSIZE_SHIFT);
						IMG_UINT32 ui32Type = (psVAOAPState->ui32StreamTypeSize & GLES2_STREAMTYPE_MASK);

						psVAOMachine->ui32ControlWord |= ATTRIBARRAY_SOURCE_VARRAY;

						if (psVAOAPMachine->ui32Stride == psVAOAPMachine->ui32Size)
						{
#if defined(NO_UNALIGNED_ACCESS)
							psVAOAPMachine->pfnCopyData[0] = MemCopyData[ui32UserSize - 1][ui32Type];
							psVAOAPMachine->pfnCopyData[1] = MemCopyData[ui32UserSize - 1][ui32Type];
							psVAOAPMachine->pfnCopyData[2] = MemCopyData[ui32UserSize - 1][ui32Type];
							psVAOAPMachine->pfnCopyData[3] = MemCopyData[ui32UserSize - 1][ui32Type];
#else
							psVAOAPMachine->pfnCopyData = MemCopyData[ui32UserSize - 1][ui32Type];
#endif
						}
						else
						{
#if defined(NO_UNALIGNED_ACCESS)
							psVAOAPMachine->pfnCopyData[0] = CopyData[ui32UserSize - 1][ui32Type];
							psVAOAPMachine->pfnCopyData[1] = CopyDataByteAligned[ui32UserSize - 1][ui32Type];
							psVAOAPMachine->pfnCopyData[2] = CopyDataShortAligned[ui32UserSize - 1][ui32Type];
							psVAOAPMachine->pfnCopyData[3] = CopyDataByteAligned[ui32UserSize - 1][ui32Type];
#else
							psVAOAPMachine->pfnCopyData = CopyData[ui32UserSize - 1][ui32Type];
#endif
						}

#if defined(NO_UNALIGNED_ACCESS)
						if(psVAOAPMachine->ui32Size % 4)
						{
							/* Ensure next dest stream is aligned */
							gc->ui32VertexAlignSize += 4;
						}
#endif
						gc->ui32VertexSize += psVAOAPMachine->ui32Size;
					}
				}

				psVAOAPMachine->pui8CopyPointer        = psVAOAPState->pui8Pointer;
				psVAOAPMachine->ui32CopyStreamTypeSize = psVAOAPState->ui32StreamTypeSize;
				psVAOAPMachine->ui32CopyStride         = psVAOAPMachine->ui32Stride;
				psVAOAPMachine->ui32DstSize            = psVAOAPMachine->ui32Size;					

			}
			else /* Setup VAO's current attribute arrays */
			{
				psVAOMachine->ui32ControlWord |= ATTRIBARRAY_SOURCE_CURRENT;

				psVAOAPMachine->bIsCurrentState = IMG_TRUE;

#if defined(NO_UNALIGNED_ACCESS)
				psVAOAPMachine->pfnCopyData[0]	       = CopyData[3][GLES2_STREAMTYPE_FLOAT];
				psVAOAPMachine->pfnCopyData[1]	       = CopyDataByteAligned[3][GLES2_STREAMTYPE_FLOAT];
				psVAOAPMachine->pfnCopyData[2]	       = CopyDataShortAligned[3][GLES2_STREAMTYPE_FLOAT];
				psVAOAPMachine->pfnCopyData[3]	       = CopyDataByteAligned[3][GLES2_STREAMTYPE_FLOAT];
#else
				psVAOAPMachine->pfnCopyData	           = CopyData[3][GLES2_STREAMTYPE_FLOAT];
#endif

				psVAOAPMachine->pui8CopyPointer		   = (IMG_VOID *)&gc->sVAOMachine.asCurrentAttrib[i];
				psVAOAPMachine->ui32CopyStreamTypeSize = GLES2_STREAMTYPE_FLOAT | (4 << GLES2_STREAMSIZE_SHIFT);
				psVAOAPMachine->ui32CopyStride		   = 0;
				psVAOAPMachine->ui32DstSize			   = 16;

				gc->ui32VertexRCSize += 16;
			}

			psVAOMachine->apsPackedAttrib[ui32NumItemsPerVertex++] = psVAOAPMachine;

			psVAOAPMachine->psState = psVAOAPState;							
		}
	}

	psVAOMachine->ui32NumItemsPerVertex = ui32NumItemsPerVertex;

}


/***********************************************************************************
 Function Name      : SetupRenderState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Sets up render state
************************************************************************************/
static IMG_VOID OptimiseVaryingPrecision(GLES2Context *gc, IMG_BOOL *pbPrecisionChanged)
{
	GLES2Program *psProgram = gc->sProgram.psCurrentProgram;
	GLES2SharedShaderState *psSharedShaderState = psProgram->sVertex.psSharedState;
	IMG_UINT32 ui32VaryingMask = psSharedShaderState->eActiveVaryingMask;
	GLSLPrecisionQualifier *peVertTexCoordPrec = psSharedShaderState->aeTexCoordPrecisions;
	GLSLPrecisionQualifier *peFragTexCoordPrec = psProgram->sFragment.psSharedState->aeTexCoordPrecisions;
	IMG_UINT32 ui32NewPrecision = 0;
	GLES2USEShaderVariant *psFragmentVariant = gc->sProgram.psCurrentFragmentVariant;
    USP_HW_SHADER *psPatchedShader = psFragmentVariant->psPatchedShader;
	IMG_UINT32 i, j;
	PVR_UNREFERENCED_PARAMETER(gc);

	for(i = 0; i < NUM_TC_REGISTERS; i++)
	{
		/* PRQA S 4130 */
		if(ui32VaryingMask & (IMG_UINT32)(GLSLVM_TEXCOORD0 << i))
		{
			/* Use 16 bit tex coords if vertex or fragment varying is mediump or lowp.
			 * If vertex shader varying is highp, and non-dependent texture lookup consumes the varying, leave at highp
			 */
			switch(peVertTexCoordPrec[i])
			{
				case GLSLPRECQ_HIGH:
				{
					IMG_UINT32 ui32InputCoordUnit;
					IMG_BOOL bUsedInNonDepRead = IMG_FALSE;

					ui32InputCoordUnit = psProgram->aui32FragVaryingPosition[i];

					/* Mismatch between VS and FS. Set precision to F16 */
					if(ui32InputCoordUnit == 0xFFFFFFFF)
					{
						ui32NewPrecision |= (EURASIA_TATEXFLOAT_TC0_16B << i);
					}
					else
					{
						for(j=0; j < psPatchedShader->uPSInputCount; j++)
						{
							IMG_UINT32 ui32IteratorNumber  = psPatchedShader->psPSInputLoads[j].eCoord;

							if(ui32InputCoordUnit == ui32IteratorNumber)
							{
								if (psPatchedShader->psPSInputLoads[j].eType == USP_HW_PSINPUT_TYPE_SAMPLE)
								{
									bUsedInNonDepRead = IMG_TRUE;
								}
							}
						}

						if(!bUsedInNonDepRead)
						{
							/* Fragment shader precision is not highp and varying is not used for direct texture
							 * lookup.
							 */
							if(peFragTexCoordPrec[ui32InputCoordUnit] != GLSLPRECQ_HIGH)
							{
								ui32NewPrecision |= (EURASIA_TATEXFLOAT_TC0_16B << i);
							}
						}
					}

					break;
				}
				case GLSLPRECQ_MEDIUM:
				case GLSLPRECQ_LOW:
				case GLSLPRECQ_UNKNOWN:
				{
					ui32NewPrecision |= (EURASIA_TATEXFLOAT_TC0_16B << i);
					break;
				}
				default:
					PVR_DPF((PVR_DBG_ERROR, "Invalid precision qualifier"));
					break;
			}
		}
	}

	if(ui32NewPrecision != psProgram->ui32TexCoordPrecision)
	{
		psProgram->ui32TexCoordPrecision = ui32NewPrecision;
		*pbPrecisionChanged = IMG_TRUE;
	}
	else
	{
		*pbPrecisionChanged = IMG_FALSE;
	}
}

/***********************************************************************************
 Function Name      : SetupRenderState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Sets up render state
************************************************************************************/
static IMG_VOID SetupRenderState(GLES2Context *gc)
{
	GLES2CompiledRenderState *psCompiledRenderState = &gc->sPrim.sRenderState;
	IMG_BOOL bTrivialBlend = IMG_FALSE;
	IMG_BOOL bTranslucentBlend = IMG_FALSE;
	IMG_UINT32 ui32StateEnables = gc->ui32Enables;
	IMG_UINT32 ui32LineWidth;
	GLSLProgramFlags eProgramFlags = gc->sProgram.psCurrentProgram->sFragment.psSharedState->eProgramFlags;
	IMG_UINT32 ui32FrontFace = gc->sState.sPolygon.eFrontFaceDirection;	

	/* MTE control words - all layers are gouraud shaded */
	IMG_UINT32  ui32MTEControl =	EURASIA_MTE_DRAWCLIPPEDEDGES							|
#if defined(SGX545)
	                                (EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE0_SHIFT) |
									(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE1_SHIFT) |
									(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE2_SHIFT) |
									(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE3_SHIFT) |
									(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE4_SHIFT) |
									(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE5_SHIFT) |
									(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE6_SHIFT) |
									(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE7_SHIFT) |
#else	 
									(EURASIA_MTE_SHADE_GOURAUD << EURASIA_MTE_SHADE_SHIFT)	|
#endif /* defined(SGX545) */
									EURASIA_MTE_WCLAMPEN;

	/* ISP Control Words */
	IMG_UINT32	ui32ISPControlWordA 		= 0;
	IMG_UINT32	ui32ISPControlWordB 		= 0;
	IMG_UINT32	ui32ISPControlWordC;

	/* Back-facing triangle ISP Control Words */
	IMG_UINT32	ui32BFISPControlWordA		= 0;
	IMG_UINT32	ui32BFISPControlWordC		= 0;

	/* Visibility test state sent back to ISP from USE for pixel discard */ 
	IMG_UINT32	ui32ISPFeedback0	= 0;
	IMG_UINT32	ui32ISPFeedback1	= 0;
	IMG_UINT32	ui32AlphaTestFlags;

	ui32AlphaTestFlags = (eProgramFlags & GLSLPF_DISCARD_EXECUTED) ? GLES2_ALPHA_TEST_DISCARD : 0;

	/* Line width */
#if defined(SGX545)
	/* convert the value into unsigned 4.4 fixed point format */
	ui32LineWidth = (IMG_UINT32)(gc->sState.sLine.fWidth * EURASIA_ISPB_PLWIDTH_ONE);
	ui32LineWidth = MIN(ui32LineWidth, EURASIA_ISPB_PLWIDTH_MAX);
	ui32LineWidth = (ui32LineWidth << EURASIA_ISPB_PLWIDTH_SHIFT) & ~EURASIA_ISPB_PLWIDTH_CLRMSK;

	ui32ISPControlWordB |= ui32LineWidth;
#else 
	ui32LineWidth = (((((IMG_UINT32)(gc->sState.sLine.fWidth+0.5f)) - 1) & 0xF) 
					 << EURASIA_ISPA_PLWIDTH_SHIFT) & ~EURASIA_ISPA_PLWIDTH_CLRMSK;

	ui32ISPControlWordA |= ui32LineWidth;
#endif /* defined(SGX545) */

	/* The Y-inverted state of a render surface affects the winding order of
	   triangles, which in turn affects culling and two-sided stencil. 
	   Adjust the winding order for non-inverted Y (normal surfaces have inverted
	   drawables and that is what has been coded for) */
	if(gc->psDrawParams->eRotationAngle == PVRSRV_FLIP_Y)
	{
		if(ui32FrontFace == GL_CCW)
		{
			ui32FrontFace = GL_CW;
		}
		else
		{
			ui32FrontFace = GL_CCW;
		}
	}

	/* 
	 * Culling 
	 */
	if(ui32StateEnables & GLES2_CULLFACE_ENABLE)
	{
		if(ui32FrontFace == GL_CCW)
		{
			if (gc->sState.sPolygon.eCullMode == GL_FRONT)
			{
				ui32MTEControl |= EURASIA_MTE_CULLMODE_CCW;
			}
			else if(gc->sState.sPolygon.eCullMode ==  GL_BACK)
			{
				ui32MTEControl |= EURASIA_MTE_CULLMODE_CW;
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR,"SetupRenderState: Unsupported cull mode"));
			}
		}
		else
		{
			if (gc->sState.sPolygon.eCullMode ==  GL_FRONT)
			{
				ui32MTEControl |= EURASIA_MTE_CULLMODE_CW;
			}
			else if(gc->sState.sPolygon.eCullMode ==  GL_BACK)
			{
				ui32MTEControl |= EURASIA_MTE_CULLMODE_CCW;
			}
			else
			{
				PVR_DPF((PVR_DBG_ERROR,"SetupRenderState: Unsupported cull mode"));
			}
		}
	}

	/*
	 * Depth test
	 */
	if(((ui32StateEnables & GLES2_DEPTHTEST_ENABLE) != 0) && gc->psMode->ui32DepthBits)
	{
		ui32ISPControlWordA |= gc->sState.sDepth.ui32TestFunc;
	}
	else
	{
		/* Depth test is disabled so always pass the test, but never update the z-buffer */
		ui32ISPControlWordA |= EURASIA_ISPA_DCMPMODE_ALWAYS;
		ui32ISPControlWordA |= EURASIA_ISPA_DWRITEDIS;
	}

	/*
	 * Depth bias
	 */
	if(ui32StateEnables & GLES2_POLYOFFSET_ENABLE)
	{
#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
		ui32ISPControlWordB |= EURASIA_ISPB_DBIAS_ENABLE;
#else
		IMG_INT32  i32DepthBiasUnits, i32DepthBiasFactor;

		i32DepthBiasUnits  = (IMG_INT32)(gc->sAppHints.fPolygonUnitsMultiplier*gc->sState.sPolygon.fUnits);
		i32DepthBiasFactor = (IMG_INT32)(gc->sAppHints.fPolygonFactorMultiplier*gc->sState.sPolygon.factor.fVal);

		/* Check that the units and factor are within range */
		i32DepthBiasUnits = Clampi(i32DepthBiasUnits, -16, 15);
		i32DepthBiasFactor = Clampi(i32DepthBiasFactor, -16, 15);

		ui32ISPControlWordB |= (((IMG_UINT32)i32DepthBiasUnits << EURASIA_ISPB_DBIASUNITS_SHIFT) & ~EURASIA_ISPB_DBIASUNITS_CLRMSK);
		ui32ISPControlWordB |= (((IMG_UINT32)i32DepthBiasFactor << EURASIA_ISPB_DBIASFACTOR_SHIFT) & ~EURASIA_ISPB_DBIASFACTOR_CLRMSK);
#endif
	}

	/* 
	* Colour masks
	*/
	if(gc->sState.sRaster.ui32ColorMask)
	{
		/* Set user pass-spawn control to match the colour mask, this should generate a new ISP pass if 
		   different from previous user pass-spawn control/colour mask. Object does not need to be made
		   translucent, unless it's a partial mask and MSAA is enabled on cores without phase rate control */
		ui32ISPControlWordB |= (gc->sState.sRaster.ui32ColorMask << EURASIA_ISPB_UPASSCTL_SHIFT);

#if !defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		if(gc->psMode->ui32AntiAliasMode && (gc->sState.sRaster.ui32ColorMask != GLES2_COLORMASK_ALL))
		{
			/* Mark object as translucent to ensure the USSE runs at full rate on edges */
			ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
			ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_TRANS;
		}
#endif
	}
	else
	{
		/*
		 * Set tag write disable so it doesn't chew TSP bandwidth 
		 */
		ui32ISPControlWordA |= EURASIA_ISPA_TAGWRITEDIS;
		ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
		ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_OPAQUE;
	}

	/*
	* Framebuffer blending
	*/

	if(ui32StateEnables & GLES2_ALPHABLEND_ENABLE)
	{
		GetFBBlendType(gc, &bTrivialBlend, &bTranslucentBlend, &psCompiledRenderState->bHasConstantColorBlend);

		if(bTrivialBlend)
		{
			/* No src blend, so no point sending to the TSP - set tag write disable so 
				it doesn't chew TSP bandwidth */
			ui32ISPControlWordA |= EURASIA_ISPA_TAGWRITEDIS;
		}
		else if(bTranslucentBlend)
		{
			/* Mark object as translucent for the ISP */
			ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
			ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_TRANS;
		}
	}
	
	/* Depth write disable AND Tag write disable - make object opaque */
	if (((ui32ISPControlWordA & EURASIA_ISPA_TAGWRITEDIS) != 0) &&
		((ui32ISPControlWordA & EURASIA_ISPA_DWRITEDIS) != 0))
	{
		ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
	}

	/* 
	* Discard - a fragment shader that can kill pixels is being used.
	* Need to set the ISP pass-type to punchthrough and make sure TAGWRITEDIS is NOT set
	*/
	if (ui32AlphaTestFlags)
	{
		ui32ISPControlWordA &= ~EURASIA_ISPA_TAGWRITEDIS;

		if((ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK) == EURASIA_ISPA_PASSTYPE_TRANS)
		{
			/* Set object type to transparent PT */
			ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
			ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_TRANSPT;
		}
		else
		{
			ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
			ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_FASTPT;
		}

#if defined(FIX_HW_BRN_29546)
		if((gc->sPrim.eCurrentPrimitiveType==GLES2_PRIMTYPE_SPRITE) ||
		   (gc->sPrim.eCurrentPrimitiveType==GLES2_PRIMTYPE_POINT))
		{
			ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;

			/* Set object type to depth feedback */
			ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_DEPTHFEEDBACK;

			ui32AlphaTestFlags |= GLES1_ALPHA_TEST_BRNFIX_DEPTHF;
		}
#endif /* defined(FIX_HW_BRN_29546) */
#if defined(FIX_HW_BRN_31728)
		if((gc->sPrim.eCurrentPrimitiveType==GLES2_PRIMTYPE_LINE)	   ||
		   (gc->sPrim.eCurrentPrimitiveType==GLES2_PRIMTYPE_LINE_LOOP) ||
		   (gc->sPrim.eCurrentPrimitiveType==GLES2_PRIMTYPE_LINE_STRIP))
		{
			ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;

			/* Set object type to depth feedback */
			ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_DEPTHFEEDBACK;

			ui32AlphaTestFlags |= GLES1_ALPHA_TEST_BRNFIX_DEPTHF;
		}
#endif /* defined(FIX_HW_BRN_31728) */
	}

	/*
	 * Stencil test
	*/
	if (((ui32StateEnables & GLES2_STENCILTEST_ENABLE) != 0) && gc->psMode->ui32StencilBits)
	{
		/* Stencil state is the same for front and back faces */
		if(gc->sState.sStencil.ui32BFStencil == gc->sState.sStencil.ui32FFStencil &&
		   gc->sState.sStencil.ui32BFStencilRef == gc->sState.sStencil.ui32FFStencilRef)
		{
			ui32ISPControlWordC = gc->sState.sStencil.ui32FFStencil;
			ui32ISPControlWordA |= gc->sState.sStencil.ui32FFStencilRef;
		}
		else
		{
			/* Culling is enabled, so pick the non-culled face's stencil state */
			if(ui32StateEnables & GLES2_CULLFACE_ENABLE)
			{
				if(gc->sState.sPolygon.eCullMode == GL_BACK)
				{
					ui32ISPControlWordC = gc->sState.sStencil.ui32FFStencil;
					ui32ISPControlWordA |= gc->sState.sStencil.ui32FFStencilRef;
				}
				else
				{
					ui32ISPControlWordC = gc->sState.sStencil.ui32BFStencil;
					ui32ISPControlWordA |= gc->sState.sStencil.ui32BFStencilRef;
				}
			}
			else
			{
				ui32ISPControlWordA |= EURASIA_ISPA_2SIDED;
				ui32BFISPControlWordA = ui32ISPControlWordA;


				/* Pick correct Stencil face if 2-sided and no culling */
				if(ui32FrontFace == GL_CW)
				{
					ui32ISPControlWordC = gc->sState.sStencil.ui32FFStencil;
					ui32ISPControlWordA |= gc->sState.sStencil.ui32FFStencilRef;
					
					ui32BFISPControlWordC = gc->sState.sStencil.ui32BFStencil;
					ui32BFISPControlWordA |= gc->sState.sStencil.ui32BFStencilRef;
				}
				else
				{
					ui32ISPControlWordC = gc->sState.sStencil.ui32BFStencil;
					ui32ISPControlWordA |= gc->sState.sStencil.ui32BFStencilRef;
					
					ui32BFISPControlWordC = gc->sState.sStencil.ui32FFStencil;
					ui32BFISPControlWordA |= gc->sState.sStencil.ui32FFStencilRef;
				}
			}
		}
	}
	else
	{
		/* Else stencil always passes - and no action is done on stencil buffer */

		ui32ISPControlWordC = (EURASIA_ISPC_SCMP_ALWAYS |
							 EURASIA_ISPC_SOP1_KEEP |
							 EURASIA_ISPC_SOP2_KEEP |
							 EURASIA_ISPC_SOP3_KEEP);
	}

#if defined(FIX_HW_BRN_25077)

	/* Object was not already (fast) punchthrough, stencil was enabled, and either scissor or viewport were not fullscreen */
	if(((ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK) == EURASIA_ISPA_PASSTYPE_OPAQUE) ||
		((ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK) == EURASIA_ISPA_PASSTYPE_TRANS))
	{
		if((ui32ISPControlWordC != (EURASIA_ISPC_SCMP_ALWAYS |
									EURASIA_ISPC_SOP1_KEEP |
									EURASIA_ISPC_SOP2_KEEP |
									EURASIA_ISPC_SOP3_KEEP)) &&
									((gc->ui32Enables & GLES2_SCISSOR_ENABLE && !gc->bFullScreenScissor) || 
									 !gc->bFullScreenViewport))
		{
			ui32AlphaTestFlags = GLES2_ALPHA_TEST_BRN25077;
		
			/* Make sure tag write disable is not set */
			if(ui32ISPControlWordA & EURASIA_ISPA_TAGWRITEDIS)
			{
				ui32AlphaTestFlags |= GLES2_ALPHA_TEST_WAS_TWD;

				ui32ISPControlWordA &= ~EURASIA_ISPA_TAGWRITEDIS;
			}

			if(	((ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK) == EURASIA_ISPA_PASSTYPE_TRANS)
				|| (ui32AlphaTestFlags & GLES2_ALPHA_TEST_WAS_TWD))
			{
				/* Set object type to transparent PT */
				ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
				ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_TRANSPT;
			}
			else
			{
				/* Alpha test and no translucent blend, so set object type to fast punch through */
				ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
				ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_FASTPT;
			}
		}
	}
#endif

	/* Only support fill mode. 
	 */
	ui32ISPControlWordA |= EURASIA_ISPA_OBJTYPE_TRI;

	if(ui32ISPControlWordA & EURASIA_ISPA_2SIDED)
	{
		ui32BFISPControlWordA |= EURASIA_ISPA_OBJTYPE_TRI;
	}

	/* See if a visibility test of some kind (alpha-test, texkill, depth feedback)
	   is enabled */
	if(ui32AlphaTestFlags)
	{
		/* Setup ISP visibility test state that gets sent from USE ATST8/DEPTHF instructions */
		
		/* State word 0 */
#if defined(SGX_FEATURE_ALPHATEST_AUTO_COEFF)
		ui32ISPFeedback0 |= (ui32ISPControlWordA & EURASIA_ISPA_DWRITEDIS) ? EURASIA_USE_VISTEST_STATE0_DWDISABLE : 0;

		/* There is no Alpha-test, so set alpha compare mode to ALWAYS */
		ui32ISPFeedback0 |= EURASIA_USE_VISTEST_STATE0_ACMPMODE_ALWAYS << EURASIA_USE_VISTEST_STATE0_ACMPMODE_SHIFT;

#else
		ui32ISPFeedback0 |= GLES2_EXTRACT(ui32ISPControlWordA, ISPA_SREF) << EURASIA_USE_VISTEST_STATE0_SREF_SHIFT;
		ui32ISPFeedback0 |= GLES2_EXTRACT(ui32ISPControlWordA, ISPA_DCMPMODE) << EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT;

		ui32ISPFeedback0 |= GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SCMP) << EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
		ui32ISPFeedback0 |= GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SOP1) << EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
		ui32ISPFeedback0 |= GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SOP2) << EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
		ui32ISPFeedback0 |= GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SOP3) << EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
		ui32ISPFeedback0 |= GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SCMPMASK) << EURASIA_USE_VISTEST_STATE0_SMASK_SHIFT;		

		ui32ISPFeedback0 |= (ui32ISPControlWordA & EURASIA_ISPA_DWRITEDIS) ? EURASIA_USE_VISTEST_STATE0_DWDISABLE : 0;

		/* State word 1 */
		/* There is no Alpha-test, so set alpha compare mode to ALWAYS */
		ui32ISPFeedback1 |= EURASIA_USE_VISTEST_STATE1_ACMPMODE_ALWAYS << EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT;
		ui32ISPFeedback1 |= GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SWMASK) << EURASIA_USE_VISTEST_STATE1_SWMASK_SHIFT;

		/* If two-sided stencil is enabled, copy state from back-facing words,
		   otherwise duplicate the front-facing state. */
		if(ui32ISPControlWordA & EURASIA_ISPA_2SIDED)
		{
			ui32ISPFeedback1 |= GLES2_EXTRACT(ui32BFISPControlWordC, ISPC_SCMP) << EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
			ui32ISPFeedback1 |= GLES2_EXTRACT(ui32BFISPControlWordC, ISPC_SOP1) << EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
			ui32ISPFeedback1 |= GLES2_EXTRACT(ui32BFISPControlWordC, ISPC_SOP2) << EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
			ui32ISPFeedback1 |= GLES2_EXTRACT(ui32BFISPControlWordC, ISPC_SOP3) << EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
		}
		else
		{
			ui32ISPFeedback1 |= GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SCMP) << EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
			ui32ISPFeedback1 |= GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SOP1) << EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
			ui32ISPFeedback1 |= GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SOP2) << EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
			ui32ISPFeedback1 |= GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SOP3) << EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
		}

		/* This condition is expected to be very rare.
		   If it occurs, it could be fixed by issuing two ATST8 instructions in 
		   the fragment program, predicated by the back-face register.
			- JDP 26/09/2005 */
		if(((ui32ISPControlWordA & EURASIA_ISPA_2SIDED) != 0) &&
		   ((GLES2_EXTRACT(ui32ISPControlWordA, ISPA_SREF)	 != GLES2_EXTRACT(ui32BFISPControlWordA, ISPA_SREF))		||
		   (GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SCMPMASK) != GLES2_EXTRACT(ui32BFISPControlWordC, ISPC_SCMPMASK))	||
		   (GLES2_EXTRACT(ui32ISPControlWordC, ISPC_SWMASK)	 != GLES2_EXTRACT(ui32BFISPControlWordC, ISPC_SWMASK))))
		{
			PVR_DPF((PVR_DBG_ERROR, "SetupRenderState:Discard with stencil and different front/back masks not supported"));
		}
#endif
	}

	/* HW doesn't have default for colormask, so always send B */
	ui32ISPControlWordA |= EURASIA_ISPA_BPRES;

	/* HW defaults to stencil off */
	if(ui32ISPControlWordC !=(EURASIA_ISPC_SCMP_ALWAYS | EURASIA_ISPC_SOP1_KEEP | EURASIA_ISPC_SOP2_KEEP | EURASIA_ISPC_SOP3_KEEP))
	{
		ui32ISPControlWordA |= EURASIA_ISPA_CPRES;
	}

	if(ui32BFISPControlWordA)
	{
		/* HW doesn't have default for colormask, so always send B */
		ui32BFISPControlWordA |= EURASIA_ISPA_BPRES;

		/* HW defaults to stencil off */
		if(ui32BFISPControlWordC !=(EURASIA_ISPC_SCMP_ALWAYS | EURASIA_ISPC_SOP1_KEEP | EURASIA_ISPC_SOP2_KEEP | EURASIA_ISPC_SOP3_KEEP))
		{
			ui32BFISPControlWordA |= EURASIA_ISPA_CPRES;
		}
	}

	/* This check is here to ensure the ISP state is re-emitted when the discard status of the
	   shader has changed - all other cases will be covered by setting of GLES2_DIRTYFLAG_RENDERSTATE elsewhere. 
	   When Temps and PAs are unified, a pass type change may also result in the DMS info being recalculated.
	 */
	if((psCompiledRenderState->ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK) != 
		(ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK))
	{
		gc->ui32DirtyState |= GLES2_DIRTYFLAG_RENDERSTATE;
		gc->ui32EmitMask |= GLES2_EMITSTATE_MTE_STATE_ISP;

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
		if(gc->psMode->ui32AntiAliasMode)
		{
			gc->ui32DirtyState |= GLES2_DIRTYFLAG_DMS_INFO;
		}
#endif
	}

	/* ------------------------------- */
	/* Setup the compiled render state */
	/* ------------------------------- */
	if(psCompiledRenderState->ui32MTEControl != ui32MTEControl)
	{
		/* Emit the MTE control if its changed */
		gc->ui32EmitMask |= GLES2_EMITSTATE_MTE_STATE_CONTROL;
	}

	psCompiledRenderState->ui32MTEControl				= ui32MTEControl;

	if((psCompiledRenderState->ui32ISPControlWordA != ui32ISPControlWordA) ||
	   (psCompiledRenderState->ui32ISPControlWordB != ui32ISPControlWordB) ||
	   (psCompiledRenderState->ui32ISPControlWordC != ui32ISPControlWordC))
	{
		/* Emit the ISP state if its changed */
		gc->ui32EmitMask |= GLES2_EMITSTATE_MTE_STATE_ISP;
	}

	psCompiledRenderState->ui32ISPControlWordA			= ui32ISPControlWordA;
	psCompiledRenderState->ui32ISPControlWordB			= ui32ISPControlWordB;
	psCompiledRenderState->ui32ISPControlWordC			= ui32ISPControlWordC;

	psCompiledRenderState->ui32BFISPControlWordA		= ui32BFISPControlWordA;
	psCompiledRenderState->ui32BFISPControlWordB		= psCompiledRenderState->ui32ISPControlWordB;
	psCompiledRenderState->ui32BFISPControlWordC		= ui32BFISPControlWordC;

	psCompiledRenderState->ui32AlphaTestFlags			= ui32AlphaTestFlags;

	if(psCompiledRenderState->ui32ISPFeedback0 != ui32ISPFeedback0 ||
		psCompiledRenderState->ui32ISPFeedback1 != ui32ISPFeedback1)
	{
		psCompiledRenderState->ui32ISPFeedback0				= ui32ISPFeedback0;
		psCompiledRenderState->ui32ISPFeedback1				= ui32ISPFeedback1;
	
		/* Emit the fragprog constants if they changed */
		gc->ui32DirtyState |= GLES2_DIRTYFLAG_FRAGPROG_CONSTANTS;
	}

} /* SetupRenderState */


/***********************************************************************************
 Function Name      : WriteVDMControlStream
 Inputs             : gc, ePrimitiveType, b32BitIndices, ui32NumIndices, uIndexAddress
 Outputs            : 
 Returns            : Mem Error
 Description        : 
************************************************************************************/
static GLES2_MEMERROR WriteVDMControlStream(GLES2Context *gc, IMG_UINT32 ePrimitiveType, IMG_BOOL b32BitIndices,
										IMG_UINT32 ui32NumIndices, IMG_DEV_VIRTADDR uIndexAddress, IMG_UINT32 ui32IndexOffset)
{
	IMG_UINT32 ui32VDMIndexListHeader;
	IMG_UINT32 ui32VDMPrimitiveType = aui32GLES2PrimToVDMPrim[ePrimitiveType];
	IMG_UINT32 *pui32BufferBase, *pui32Buffer;
	IMG_DEV_VIRTADDR uPDSBaseAddr;
	GLES2Program *psProgram = gc->sProgram.psCurrentProgram;
	GLES2ProgramShader *psVertexShader = &psProgram->sVertex;
	GLES2USEShaderVariant *psShaderVariant = gc->sProgram.psCurrentVertexVariant;
	IMG_UINT32 ui32PDSDataSize;
	IMG_UINT32 ui32DMSIndexList2, ui32DMSIndexList4, ui32DMSIndexList5;

	/*********************
	* Send the primitive *
	**********************/
	ui32VDMIndexListHeader =	EURASIA_TAOBJTYPE_INDEXLIST |
								EURASIA_VDM_IDXPRES2		|
								EURASIA_VDM_IDXPRES3		|
#if !defined(SGX545)
								EURASIA_VDM_IDXPRES45		|
#endif /* !defined(SGX545) */
								ui32VDMPrimitiveType		|
								(ui32NumIndices << EURASIA_VDM_IDXCOUNT_SHIFT);

	CalculateVertexDMSInfo(&gc->psSysContext->sHWInfo, psShaderVariant->ui32USEPrimAttribCount, 
						  psShaderVariant->ui32MaxTempRegs, psVertexShader->ui32USESecAttribDataSizeInDwords, 
						  (psProgram->ui32OutputSelects & ~EURASIA_MTE_VTXSIZE_CLRMSK) >> EURASIA_MTE_VTXSIZE_SHIFT,
						  &ui32DMSIndexList2, &ui32DMSIndexList4, &ui32DMSIndexList5);


	/*
		Get TA control stream space for index list block
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, MAX_DWORDS_PER_INDEX_LIST_BLOCK, CBUF_TYPE_VDM_CTRL_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES2_TA_BUFFER_ERROR;
	}

	pui32Buffer = pui32BufferBase;

	*pui32Buffer++ = ui32VDMIndexListHeader;

	if (b32BitIndices)
	{
		/* Index List 1 */
		*pui32Buffer++	= (uIndexAddress.uiAddr >> EURASIA_VDM_IDXBASE32_ALIGNSHIFT) << EURASIA_VDM_IDXBASE32_SHIFT;

		/* Index List 2 */
		*pui32Buffer++	= ui32DMSIndexList2 | EURASIA_VDM_IDXSIZE_32BIT | (ui32IndexOffset << EURASIA_VDM_IDXOFF_SHIFT);

	}
	else
	{
		/* Index List 1 */
		*pui32Buffer++	= (uIndexAddress.uiAddr >> EURASIA_VDM_IDXBASE16_ALIGNSHIFT) << EURASIA_VDM_IDXBASE16_SHIFT;

		/* Index List 2 */
		*pui32Buffer++	= ui32DMSIndexList2 | EURASIA_VDM_IDXSIZE_16BIT | (ui32IndexOffset << EURASIA_VDM_IDXOFF_SHIFT);
	}
	
	/**************************************************************************
	  Index List 3 (Optional): Number of indices that TA should read before 
							   jumping back to start of buffer
	**************************************************************************/
	*pui32Buffer++ = ~EURASIA_VDM_WRAPCOUNT_CLRMSK;

	/*************************************************************************
	  Index List 4, 5: PDS program to be executed in order to setup 
						vertices for the USE
	*************************************************************************/

	/*
		Determine the PDS program base address and data size
	*/
	uPDSBaseAddr.uiAddr = gc->sPrim.uVertexPDSBaseAddress.uiAddr >> EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT;
	ui32PDSDataSize = gc->sPrim.ui32VertexPDSDataSize >> EURASIA_VDMPDS_DATASIZE_ALIGNSHIFT;

	/* Index List 4 */
	*pui32Buffer++	= ui32DMSIndexList4 | (uPDSBaseAddr.uiAddr << EURASIA_VDMPDS_BASEADDR_SHIFT);

	/* Index List 5 */
	*pui32Buffer++	= ui32DMSIndexList5 | (ui32PDSDataSize << EURASIA_VDMPDS_DATASIZE_SHIFT);


	/*
		Lock TA control stream space for the index list block
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_VDM_CTRL_BUFFER);

	GLES2_INC_COUNT(GLES2_TIMER_VDM_CTRL_STATE_COUNT, (pui32Buffer - pui32BufferBase));
	
	/* 
		Update TA buffers commited primitive offset
	*/
	CBUF_UpdateBufferCommittedPrimOffsets(gc->apsBuffers, &gc->psRenderSurface->bPrimitivesSinceLastTA, (IMG_VOID *)gc, KickLimit_ScheduleTA);

	return GLES2_NO_ERROR;
}


/***********************************************************************************
 Function Name      : ValidateState
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL GLES2_MEMERROR ValidateState(GLES2Context *gc)
{
	GLES2_MEMERROR eError;
	IMG_BOOL bProgramChanged;
	GLES2VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);
	GLES2VertexArrayObject *psVAO = psVAOMachine->psActiveVAO;
	
	/* Assert VAO is not NULL */
	GLES_ASSERT(VAO(gc));

	GLES2_TIME_START(GLES2_TIMER_VALIDATE_TIME);

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(gc->ui32NumEGLImageTextureTargetsBound)
	{
		gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */


	/* Only setup context's VAO dirty flag once starting ValidateState() */
	gc->ui32DirtyState |= psVAO->ui32DirtyState;


	/* Setup the context's dirty state regarding element buffer,
	   and accordingly set VAO Machine's element buffer */
	if (gc->ui32DirtyState & GLES2_DIRTYFLAG_VAO_BINDING)
	{
		/* Set the context's dirty state with DIRTYFLAG_VAO_ELEMENT_BUFFER) */
		gc->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ELEMENT_BUFFER;
	}	
	if (gc->ui32DirtyState & GLES2_DIRTYFLAG_VAO_ELEMENT_BUFFER)
	{
		psVAOMachine->psBoundElementBuffer = psVAO->psBoundElementBuffer;
	}


	/* Setup VAO related dirty flags regarding the change of array enables */
	if(gc->ui32DirtyState & GLES2_DIRTYFLAG_VAO_CLIENT_STATE)
	{	
	    /* If VAO's array enables are different from VAO's previous array enables,
		   then VAO Machine's attribute stream needs to be reset */
	    if (psVAO->ui32PreviousArrayEnables != psVAO->ui32CurrentArrayEnables)
		{
			psVAO->ui32PreviousArrayEnables = psVAO->ui32CurrentArrayEnables;

			psVAO->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM;
		}

		/* If VAO's array enables are different from VAO Machine's array enables,
		   then VAO Machine's attribute stream needs to be reset */
	    if (psVAOMachine->ui32ArrayEnables != psVAO->ui32CurrentArrayEnables)
		{
			psVAOMachine->ui32ArrayEnables = psVAO->ui32CurrentArrayEnables;

			gc->ui32DirtyState |= GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM;
		}

		gc->ui32DirtyState |= psVAO->ui32DirtyState;
	}


	/* Setup VAO Machine's streams & pointers */
	if(gc->ui32DirtyState & (GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM | GLES2_DIRTYFLAG_VERTEX_PROGRAM))
	{
	    GLES2_TIME_START(GLES2_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME);

		SetupVAOAttribStreams(gc);
			
		GLES2_TIME_STOP(GLES2_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME);
	}
	else if( ((gc->ui32DirtyState & GLES2_DIRTYFLAG_VAO_ATTRIB_POINTER) != 0) &&
			 ((gc->ui32DirtyState & GLES2_DIRTYFLAG_VAO_BINDING) == 0) )
	{
		GLES2_TIME_START(GLES2_TIMER_SETUP_VAO_ATTRIB_POINTERS_TIME);

		SetupVAOAttribPointers(gc);
	
		GLES2_TIME_STOP(GLES2_TIMER_SETUP_VAO_ATTRIB_POINTERS_TIME);
	}
	else if (gc->ui32DirtyState & GLES2_DIRTYFLAG_VAO_BINDING)
	{
		GLES2_TIME_START(GLES2_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME);

		SetupVAOAttribStreams(gc);
		
		GLES2_TIME_STOP(GLES2_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME);

	}

	/* Setup texture state */
	if(gc->ui32DirtyState & (GLES2_DIRTYFLAG_TEXTURE_STATE	| 
							 GLES2_DIRTYFLAG_FRAGMENT_PROGRAM | 
							 GLES2_DIRTYFLAG_VERTEX_PROGRAM))
	{
		GLES2_TIME_START(GLES2_TIMER_SETUP_TEXTURESTATE_TIME);

		SetupTextureState(gc);
	
		GLES2_TIME_STOP(GLES2_TIMER_SETUP_TEXTURESTATE_TIME);

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_FP_TEXCTRLWORDS | GLES2_DIRTYFLAG_VP_TEXCTRLWORDS;
	}

	/* Setup USE vertex shader and corresponding states */
	if( (gc->ui32DirtyState & (GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM | 
							   GLES2_DIRTYFLAG_VAO_BINDING       |
							   GLES2_DIRTYFLAG_VERTEX_PROGRAM))  )
	{
		GLES2_TIME_START(GLES2_TIMER_SETUP_VERTEXSHADER_TIME);
	
		eError = SetupUSEVertexShader(gc, &bProgramChanged);
		
		GLES2_TIME_STOP(GLES2_TIMER_SETUP_VERTEXSHADER_TIME);

		if(eError != GLES2_NO_ERROR)
		{
			GLES2_TIME_STOP(GLES2_TIMER_VALIDATE_TIME);

			return eError;
		}

		if(bProgramChanged)
		{
			gc->ui32DirtyState |= GLES2_DIRTYFLAG_VP_STATE;

			gc->ui32EmitMask |= GLES2_EMITSTATE_MTE_STATE_OUTPUTSELECTS;
		}
	}

	/* Setup render states */
	if(gc->ui32DirtyState & (GLES2_DIRTYFLAG_RENDERSTATE | GLES2_DIRTYFLAG_FRAGMENT_PROGRAM))
	{
		GLES2_TIME_START(GLES2_TIMER_SETUP_RENDERSTATE_TIME);
		
		SetupRenderState(gc);
	
		GLES2_TIME_STOP(GLES2_TIMER_SETUP_RENDERSTATE_TIME);
	}
	
	/* Setup USE fragment shader and corresponding states */
	if(gc->ui32DirtyState & (GLES2_DIRTYFLAG_RENDERSTATE | GLES2_DIRTYFLAG_FRAGMENT_PROGRAM | GLES2_DIRTYFLAG_TEXTURE_STATE))
	{
		GLES2_TIME_START(GLES2_TIMER_SETUP_FRAGMENTSHADER_TIME);

		eError = SetupUSEFragmentShader(gc, &bProgramChanged);
	
		GLES2_TIME_STOP(GLES2_TIMER_SETUP_FRAGMENTSHADER_TIME);

		if(eError != GLES2_NO_ERROR)
		{
			GLES2_TIME_STOP(GLES2_TIMER_VALIDATE_TIME);

			return eError;
		}

		if(bProgramChanged)
		{
			gc->ui32DirtyState |= GLES2_DIRTYFLAG_FP_STATE;
		}
	}

	if(gc->sAppHints.bEnableVaryingPrecisionOpt && (gc->ui32DirtyState & (GLES2_DIRTYFLAG_VP_STATE | GLES2_DIRTYFLAG_FP_STATE)))
	{
		IMG_BOOL bPrecisionChanged;

		OptimiseVaryingPrecision(gc, &bPrecisionChanged);

		if(bPrecisionChanged)
		{
			gc->ui32EmitMask |= GLES2_EMITSTATE_MTE_STATE_OUTPUTSELECTS;
		}
	}

	GLES2_TIME_STOP(GLES2_TIMER_VALIDATE_TIME);
	
	return GLES2_NO_ERROR;
}


/***********************************************************************************
 Function Name      : GLES2EmitState
 Inputs             : gc, ePrimitiveType, ui32NumIndices,
 					  b32BitIndices, uIndexAddress, ui32IndexOffset
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL GLES2_MEMERROR GLES2EmitState(GLES2Context *gc, IMG_UINT32 ePrimitiveType, IMG_BOOL b32BitIndices,
										IMG_UINT32 ui32NumIndices, IMG_DEV_VIRTADDR uIndexAddress, IMG_UINT32 ui32IndexOffset)
{
	IMG_DEV_VIRTADDR uStateHWAddress;
	IMG_UINT32 ui32NumStateDWords;
	IMG_BOOL b32BitPDSIndices;
	GLES2_MEMERROR eError;
#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)
	IMG_BOOL bForceTAKick;
#endif /* defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH) */

	GLES2_TIME_START(GLES2_TIMER_STATE_EMIT_TIME);

	if(ui32NumIndices == 0)
	{
		GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);
		return GLES2_NO_ERROR;
	}

	/* Attach all used VAOs and VBOs to the current kick */
	AttachAllUsedBOsAndVAOToCurrentKick(gc);

	/* Change a point to a sprite */
	if((ePrimitiveType == GLES2_PRIMTYPE_POINT) && 
		gc->sProgram.psCurrentProgram->sFragment.psActiveSpecials[GLES2_SPECIAL_POINTCOORD])
	{
		ePrimitiveType = GLES2_PRIMTYPE_SPRITE;
	}

	/* If last primitive type does not match that about to be used re-emit the render state */
	if(aui32GLES2PrimToISPPrim[ePrimitiveType] != 
	   aui32GLES2PrimToISPPrim[gc->sPrim.ePreviousPrimitiveType])
	{
		gc->sPrim.ePreviousPrimitiveType = ePrimitiveType;

		gc->ui32EmitMask |= (GLES2_EMITSTATE_MTE_STATE_ISP | GLES2_EMITSTATE_MTE_STATE_CONTROL);
	}

	/* Are 32-bit indices needed in the PDS program? */
	if(b32BitIndices ||
	  (ui32IndexOffset && (ui32IndexOffset+ui32NumIndices > 65536)))
	{
		b32BitPDSIndices = IMG_TRUE;
	}
	else
	{
		b32BitPDSIndices = IMG_FALSE;
	}

	if(b32BitPDSIndices != gc->sPrim.bPrevious32BitPDSIndices)
	{
		gc->sPrim.bPrevious32BitPDSIndices = b32BitPDSIndices;

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_PRIM_INDEXBITS;
	}


	if(gc->ui32DirtyState || gc->ui32EmitMask)
	{
		IMG_BOOL bChanged;

		/*
			Write the PDS pixel program for pixel shader secondary attributes
		*/
		if(gc->ui32DirtyState & (GLES2_DIRTYFLAG_FP_STATE |
								 GLES2_DIRTYFLAG_FRAGPROG_CONSTANTS |
								 GLES2_DIRTYFLAG_FP_TEXCTRLWORDS))
		{
			GLES2_TIME_START(GLES2_TIMER_WRITESAFRAGPROGRAM_TIME);

			eError = WritePDSUSEShaderSAProgram(gc, GLES2_SHADERTYPE_FRAGMENT, &bChanged);
		
			GLES2_TIME_STOP(GLES2_TIMER_WRITESAFRAGPROGRAM_TIME);

			if(eError != GLES2_NO_ERROR)
			{
				GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);

				return eError;
			}

			if(bChanged)
			{
				gc->ui32EmitMask |= GLES2_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE;
			}
		}


		/*
			Write the PDS pixel program for iteration, texture fetch and the pixel shader
		*/
		if(gc->ui32DirtyState & (GLES2_DIRTYFLAG_FP_STATE | GLES2_DIRTYFLAG_FP_TEXCTRLWORDS | GLES2_DIRTYFLAG_DMS_INFO))
		{
			GLES2_TIME_START(GLES2_TIMER_WRITEPDSPIXELSHADERPROGRAM_TIME);
			
			eError = WritePDSPixelShaderProgram(gc, &bChanged);
			
			GLES2_TIME_STOP(GLES2_TIMER_WRITEPDSPIXELSHADERPROGRAM_TIME);
		
			if(eError != GLES2_NO_ERROR)
			{
				GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);

				return eError;
			}

			if(bChanged)
			{
				gc->ui32EmitMask |= GLES2_EMITSTATE_PDS_FRAGMENT_STATE;
			}
		}


		/*
			Write the PDS vertex program for vertex fetch and the vertex shader
		*/
		if( (gc->ui32DirtyState & (GLES2_DIRTYFLAG_VP_STATE            | 
								   GLES2_DIRTYFLAG_VAO_ATTRIB_POINTER  |
								   GLES2_DIRTYFLAG_VAO_ATTRIB_STREAM   |
								   GLES2_DIRTYFLAG_VAO_BINDING         |
								   GLES2_DIRTYFLAG_PRIM_INDEXBITS  )) )
		{
			eError = WritePDSVertexShaderProgramWithVAO(gc, b32BitPDSIndices);

			if(eError != GLES2_NO_ERROR)
			{
				GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);

				return eError;
			}
		}


		/*
			Write the MTE state for tiling and pixel processing
		*/
		if(gc->ui32EmitMask &  (GLES2_EMITSTATE_MTE_STATE_REGION_CLIP |
								GLES2_EMITSTATE_MTE_STATE_VIEWPORT |
								GLES2_EMITSTATE_MTE_STATE_TAMISC |
								GLES2_EMITSTATE_MTE_STATE_OUTPUTSELECTS |
								GLES2_EMITSTATE_MTE_STATE_CONTROL |
								GLES2_EMITSTATE_MTE_STATE_ISP |
								GLES2_EMITSTATE_PDS_FRAGMENT_STATE |
								GLES2_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE))
		{

			GLES2_TIME_START(GLES2_TIMER_WRITEMTESTATE_TIME);
			
			eError = WriteMTEState(gc, ePrimitiveType, &ui32NumStateDWords, &uStateHWAddress);
		
			GLES2_TIME_STOP(GLES2_TIMER_WRITEMTESTATE_TIME);
		
			if(eError != GLES2_NO_ERROR)
			{
				GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);

				return eError;
			}
		
			if(ui32NumStateDWords)
			{
				/*
					Write the PDS/USE programs for copying MTE state through the primary attributes to the output buffer
				*/
				GLES2_TIME_START(GLES2_TIMER_WRITEMTESTATECOPYPROGRAMS_TIME);
			
				if(gc->sAppHints.bEnableStaticMTECopy)
				{
					eError = PatchPregenMTEStateCopyPrograms(gc, ui32NumStateDWords, uStateHWAddress);
				}
				else
				{
					eError = WriteMTEStateCopyPrograms(gc, ui32NumStateDWords, uStateHWAddress);
				}	

				GLES2_TIME_STOP(GLES2_TIMER_WRITEMTESTATECOPYPROGRAMS_TIME);
		
				if(eError != GLES2_NO_ERROR)
				{
					GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);

					return eError;
				}

				gc->ui32EmitMask |= GLES2_EMITSTATE_STATEUPDATE;
			}
		}

		/*
			Write the PDS vertex program for vertex shader secondary attributes
		*/
		if(gc->ui32DirtyState & (GLES2_DIRTYFLAG_VP_STATE |
								 GLES2_DIRTYFLAG_VERTPROG_CONSTANTS |
								 GLES2_DIRTYFLAG_VP_TEXCTRLWORDS))
		{
			GLES2_TIME_START(GLES2_TIMER_WRITESAVERTPROGRAM_TIME);

			eError = WritePDSUSEShaderSAProgram(gc, GLES2_SHADERTYPE_VERTEX, &bChanged);
		
			GLES2_TIME_STOP(GLES2_TIMER_WRITESAVERTPROGRAM_TIME);
	
			if(eError != GLES2_NO_ERROR)
			{
				GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);

				return eError;
			}

			if(bChanged)
			{
				gc->ui32EmitMask |= GLES2_EMITSTATE_PDS_VERTEX_SECONDARY_STATE;
			}
		}

		/*
			Write the state block for tiling and pixel processing to the control stream
		*/
		if(gc->ui32EmitMask & GLES2_EMITSTATE_STATEUPDATE)
		{
			/* send the state */
			GLES2_TIME_START(GLES2_TIMER_WRITEVDMCONTROLSTREAM_TIME);
			
			eError = SetupStateUpdate(gc, IMG_TRUE);
		
			GLES2_TIME_STOP(GLES2_TIMER_WRITEVDMCONTROLSTREAM_TIME);

			if(eError != GLES2_NO_ERROR)
			{
				GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);

				return eError;
			}
		}
			
		/*
			Write the vertex SA state block to the control stream
		*/
		if(gc->ui32EmitMask & GLES2_EMITSTATE_PDS_VERTEX_SECONDARY_STATE)
		{
			GLES2_TIME_START(GLES2_TIMER_WRITEVDMCONTROLSTREAM_TIME);

			eError = SetupStateUpdate(gc, IMG_FALSE);

			GLES2_TIME_STOP(GLES2_TIMER_WRITEVDMCONTROLSTREAM_TIME);

			if(eError != GLES2_NO_ERROR)
			{
				GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);

				return eError;
			}
		}
	}
	
	if(gc->sAppHints.bOptimisedValidation)
	{
		gc->ui32EmitMask = 0;
		gc->ui32DirtyState = 0;
	}
	
	GLES2_TIME_START(GLES2_TIMER_WRITEVDMCONTROLSTREAM_TIME);


	/* Write VDM Control Stream after dirty flags have been reset - this way, if a TA is forcibly kicked,
	 * the new dirty flags will be retained.
	 */
	eError = WriteVDMControlStream(gc, ePrimitiveType, b32BitIndices, ui32NumIndices, uIndexAddress, ui32IndexOffset);
	
	GLES2_TIME_STOP(GLES2_TIMER_WRITEVDMCONTROLSTREAM_TIME);
	
	if(eError != GLES2_NO_ERROR)
	{
		GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);

		return eError;
	}

#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)

	gc->sSmallKickTA.ui32NumIndicesThisTA += ui32NumIndices;
	gc->sSmallKickTA.ui32NumPrimitivesThisTA++;

	bForceTAKick = IMG_FALSE;

	switch(gc->sAppHints.ui32KickTAMode)
	{
		case 1:
		{
			/* Kick on primitive count */
			if(gc->sSmallKickTA.ui32NumPrimitivesThisTA >= gc->sAppHints.ui32KickTAThreshold)
			{
				bForceTAKick = IMG_TRUE;
			}

			break;
		}
		case 2:
		{
			/* Kick on index count */
			if(gc->sSmallKickTA.ui32NumIndicesThisTA >= gc->sAppHints.ui32KickTAThreshold)
			{
				bForceTAKick = IMG_TRUE;
			}

			break;
		}
		case 3:
		{
			/* Kick on primitive count calculated each frame */
			if(gc->sSmallKickTA.ui32NumPrimitivesThisTA >= gc->sSmallKickTA.ui32KickThreshold)
			{
				bForceTAKick = IMG_TRUE;
			}

			break;
		}
		case 4:
		{
			/* Kick on index count calculated each frame */
			if(gc->sSmallKickTA.ui32NumIndicesThisTA >= gc->sSmallKickTA.ui32KickThreshold)
			{
				bForceTAKick = IMG_TRUE;
			}

			break;
		}
		default:
		{
			/* Do nothing */
			break;
		}
	}

	if(bForceTAKick)
	{
		if(ScheduleTA(gc, gc->psRenderSurface, 0) != IMG_EGL_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"EmitState: Couldn't flush HW"));					
		}
	}

#endif /* defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH) */

	GLES2_TIME_STOP(GLES2_TIMER_STATE_EMIT_TIME);

	return GLES2_NO_ERROR;
}

/******************************************************************************
 End of file (validate.c)
******************************************************************************/

