/******************************************************************************
 * Name         : validate.c
 *
 * Copyright    : 2006-2010 by Imagination Technologies Limited.
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
 * of SA)
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "context.h"
#include "sgxpdsdefs.h"
#include "pixevent.h"
#include "pixeventpbesetup.h"
#include "dmscalc.h"

#include "pds_mte_state_copy.h"
#include "pds_mte_state_copy_sizeof.h"
#include "pds_aux_vtx.h"
#include "pds_aux_vtx_sizeof.h"

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


/* Maps GLES primitive types to ISP primitive types */
static const IMG_UINT32 aui32GLESPrimToISPPrim[GLES1_PRIMTYPE_MAX] =
{
	EURASIA_ISPA_OBJTYPE_SPRITEUV,
	EURASIA_ISPA_OBJTYPE_LINE,
	EURASIA_ISPA_OBJTYPE_LINE,
	EURASIA_ISPA_OBJTYPE_LINE,
	EURASIA_ISPA_OBJTYPE_TRI,
	EURASIA_ISPA_OBJTYPE_TRI,
	EURASIA_ISPA_OBJTYPE_TRI,
	EURASIA_ISPA_OBJTYPE_SPRITE01UV,
	EURASIA_ISPA_OBJTYPE_TRI
};

/* Maps GLES primitive types to VDM primitive types */
static const IMG_UINT32 aui32GLESPrimToVDMPrim[GLES1_PRIMTYPE_MAX] =
{
	EURASIA_VDM_POINTS,
	EURASIA_VDM_LINES,
	EURASIA_VDM_LINES,
	EURASIA_VDM_LINES,
	EURASIA_VDM_TRIS,
	EURASIA_VDM_STRIP,
	EURASIA_VDM_FAN,
	EURASIA_VDM_POINTS,
	EURASIA_VDM_STRIP
};

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
					ui32DMASize			- Size of the DMA in ui32ords.
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

	GLES1_ASSERT(ui32DMASize > 0);

#if (EURASIA_PDS_DOUTD1_MAXBURST == EURASIA_PDS_DOUTD1_BSIZE_MAX)

	GLES1_ASSERT(ui32DMASize <= (EURASIA_PDS_DOUTD1_MAXBURST * PDS_NUM_DMA_KICKS));

#else
	GLES1_ASSERT(ui32DMASize <= (EURASIA_PDS_DOUTD1_MAXBURST * (PDS_NUM_DMA_KICKS - 1)));

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
				GLES1_ASSERT(g_abyFactorTable[i] == abyFactorTable[i]);
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
IMG_INTERNAL GLES1_MEMERROR OutputTerminateState(GLES1Context *gc, 
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
	if(ui32KickFlags & GLES1_SCHEDULE_HW_DISCARD_SCENE)
	{
		return GLES1_NO_ERROR;
	}



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

		GLES1_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= PDS_TERMINATE_SAUPDATE_DWORDS);

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
	if(((ui32KickFlags & GLES1_SCHEDULE_HW_LAST_IN_SCENE) != 0) && 
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
		return GLES1_TA_BUFFER_ERROR;
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
	*pui32Buffer++	= (EURASIA_TAOBJTYPE_STATE                      |
					   EURASIA_TAPDSSTATE_LASTTASK					|
					   (((uPDSCodeAddress.uiAddr >> EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) 
						 << EURASIA_TAPDSSTATE_BASEADDR_SHIFT) & ~EURASIA_TAPDSSTATE_BASEADDR_CLRMSK));

	*pui32Buffer++	= ((((ui32PDSDataSize >> EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) 
						<< EURASIA_TAPDSSTATE_DATASIZE_SHIFT) & ~EURASIA_TAPDSSTATE_DATASIZE_CLRMSK) |
					   EURASIA_TAPDSSTATE_USEPIPE_1                 |
#if defined(SGX545)
					   (2UL << EURASIA_TAPDSSTATE_PARTITIONS_SHIFT) |
#else
					   (1UL << EURASIA_TAPDSSTATE_PARTITIONS_SHIFT) |
#endif
					   EURASIA_TAPDSSTATE_SD                        |
					   EURASIA_TAPDSSTATE_MTE_EMIT					|
					   (ALIGNCOUNTINBLOCKS(ui32USEAttribSize, EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS) << EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_SHIFT));

	*pui32Buffer++ = EURASIA_TAOBJTYPE_TERMINATE;

	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_VDM_CTRL_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_VDM_CTRL_STATE_COUNT, (pui32Buffer - pui32BufferBase));

	gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->ui32CommittedPrimOffsetInBytes = gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->ui32CurrentWriteOffsetInBytes;

	gc->ui32EmitMask |= GLES1_EMITSTATE_STATEUPDATE;

	return GLES1_NO_ERROR;
}


/*****************************************************************************
 Function Name	: SetupPBEStateEmit
 Inputs			: gc, psPixelBEState, ui32EmitIdx
 Outputs		: none
 Returns		: success
 Description	: Get the state to emit to PBE on an End Of Tile event
*****************************************************************************/
static GLES1_MEMERROR SetupPBEStateEmit(GLES1Context *gc, EGLPixelBEState *psPixelBEState)
{
	PBE_SURF_PARAMS			 sSurfParams;
	PBE_RENDER_PARAMS		 sRenderParams;
	IMG_UINT32               aui32EmitWords[STATE_PBE_DWORDS + 1];

#if defined(FIX_HW_BRN_24181)
	if(((gc->ui32FrameEnables & GLES1_FS_DITHER_ENABLE) != 0) && (gc->psDrawParams->ePixelFormat != PVRSRV_PIXEL_FORMAT_ARGB4444))
#else
	if(gc->ui32FrameEnables & GLES1_FS_DITHER_ENABLE)
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

	if(IsColorAttachmentTwiddled(gc, gc->sFrameBuffer.psActiveFrameBuffer))
	{
#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
		sSurfParams.eMemLayout = IMG_MEMLAYOUT_HYBRIDTWIDDLED;
#else
		sSurfParams.eMemLayout = IMG_MEMLAYOUT_TWIDDLED;
#endif
	}
	else
	{
		sSurfParams.eMemLayout = IMG_MEMLAYOUT_STRIDED;
	}

	if(gc->psRenderSurface->bMultiSample)
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
	sSurfParams.bZOnlyRender = IMG_FALSE;

	WritePBEEmitState(&sSurfParams, &sRenderParams, aui32EmitWords);

	GLES1MemCopy(psPixelBEState->aui32EmitWords, aui32EmitWords, sizeof(psPixelBEState->aui32EmitWords));

	psPixelBEState->ui32SidebandWord = aui32EmitWords[STATE_PBE_DWORDS];

	return GLES1_NO_ERROR;
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

IMG_INTERNAL GLES1_MEMERROR SetupPixelEventProgram(GLES1Context *gc, EGLPixelBEState *psPixelBEState, IMG_BOOL bPatch)
{
	PDS_PIXEL_EVENT_PROGRAM	 sProgram;
	IMG_UINT32               ui32StateSize;
	IMG_UINT32               *pui32Buffer, *pui32BufferBase;
	IMG_DEV_VIRTADDR         uPixelEventPDSProgramAddr;
	IMG_DEV_VIRTADDR         uEOTCodeAddress, uPTOFFCodeAddress, uEORCodeAddress;
	GLES1_MEMERROR			 eError;
	

	/************************
      A: Set PBE Emit words
	************************/

	eError = SetupPBEStateEmit(gc, psPixelBEState);

	if(eError != GLES1_NO_ERROR)
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

	if(eError != GLES1_NO_ERROR)
	{
		return eError;
	}

	/*
	  B2: Setup Address of PTOFF / End Of Render USSE Code: uPTOFFCodeAddress/uEORCodeAddress 
	      (PTOFF / End of Render USSE Programs have already been set up when initialising context) 
	*/
	uPTOFFCodeAddress = gc->sPrim.psPixelEventPTOFFCodeBlock->sCodeAddress;

	GLES1_ASSERT((uPTOFFCodeAddress.uiAddr & (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) == 0);

	uEORCodeAddress = gc->sPrim.psPixelEventEORCodeBlock->sCodeAddress;

	GLES1_ASSERT((uEORCodeAddress.uiAddr & (EURASIA_PDS_DOUTU_PHASE_START_ALIGN - 1)) == 0);


	/*
	  B3: Initialise Pixel Event Position
	*/
	if(bPatch)
	{
		pui32BufferBase = psPixelBEState->pui32PixelEventPDS;

		GLES1_ASSERT(pui32BufferBase);
	}
	else
	{
		pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, PDSGetPixeventProgSize() >> 2UL, CBUF_TYPE_PDS_FRAG_BUFFER, IMG_FALSE);
		
		if(!pui32BufferBase)
		{
			return GLES1_3D_BUFFER_ERROR;
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


	/* Setup the USE execution address */
	SetUSEExecutionAddress(&sProgram.aui32EOTUSETaskControl[0], 
							0, 
							uEOTCodeAddress, 
							gc->psSysContext->uUSEFragmentHeapBase, 
							SGX_PIXSHADER_USE_CODE_BASE_INDEX);


	/*
	  B5: Setup PTOFF / End Of Render Task Control for PDS Program
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

		GLES1_INC_COUNT(GLES1_TIMER_PDS_FRAG_DATA_COUNT, (pui32Buffer - pui32BufferBase));
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

	return GLES1_NO_ERROR;
}

/*****************************************************************************
 Function Name	: WritePDSPixelShaderProgram
 Inputs			: gc		- pointer to the context
 Outputs		: pbChanged
 Returns		: success
 Description	: Writes the PDS pixel shader program
*****************************************************************************/
static GLES1_MEMERROR WritePDSPixelShaderProgram(GLES1Context *gc, IMG_BOOL *pbChanged)
{
	GLES1Shader *psShader = gc->sProgram.psCurrentFragmentShader;
	GLES1ShaderVariant *psFragmentVariant = gc->sProgram.psCurrentFragmentVariant;
	GLES1PDSCodeVariant *psPDSVariant;
	GLES1PDSInfo *psPDSInfo;
	GLESShaderTextureState *psShaderTextureState = &gc->sPrim.sTextureState;
	PDS_TEXTURE_IMAGE_UNIT *psTextureImageUnit = &psShaderTextureState->asTextureImageUnits[0];
	PDS_PIXEL_SHADER_PROGRAM sProgram;
	IMG_UINT32 i;
	IMG_UINT32 ui32PDSProgramSize, ui32PDSDataSize;
	IMG_UINT32 ui32NumTemps;
	IMG_UINT32 ui32NumTempsInBlocks;
	IMG_UINT32 *pui32Buffer, *pui32BufferBase = IMG_NULL;
	IMG_BOOL bSetupDMSInfo;
	IMG_DEV_VIRTADDR uPDSPixelShaderBaseAddr;
	HashValue tPDSVariantHash;
	IMG_UINT32 aui32HashInput[GLES1_MAX_TEXTURE_IMAGE_UNITS + 1], ui32HashInputSizeInDWords, ui32HashCompareSizeInDWords, *pui32HashCompare;
#if defined(FIX_HW_BRN_25339) || defined(FIX_HW_BRN_27330)
	IMG_UINT32 ui32Row, ui32Column;
#else
	IMG_UINT32 ui32PDSDataSizeDS0;
	IMG_UINT32 ui32PDSDataSizeDS1;
#endif

	/* Return if there's no variant */
	if(!psFragmentVariant)
	{
		PVR_DPF((PVR_DBG_ERROR,"WritePDSPixelShaderProgram: No pixel shader!"));

		return GLES1_GENERAL_MEM_ERROR;
	}

	psPDSVariant = psFragmentVariant->psPDSVariant;
	
	psPDSInfo = &psFragmentVariant->u.sFragment.sPDSInfo;


	/* If we haven't generated any PDS variants before, we need to do the DMS Info calculations. They are, however,
	 * the same for all variants. Once done, they don't need to be repeated (phew!).
	 */
	if(psPDSVariant)
	{
		bSetupDMSInfo = IMG_FALSE;
	}
	else
	{
		bSetupDMSInfo = IMG_TRUE;
	}
	ui32NumTemps = psFragmentVariant->ui32MaxTempRegs;

	/* Align to temp allocation granularity */
	ui32NumTempsInBlocks = ALIGNCOUNTINBLOCKS(ui32NumTemps, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);

	/* USSE base address + unit enables */
	ui32HashCompareSizeInDWords = 2;

	for(i=0; i < GLES1_MAX_TEXTURE_IMAGE_UNITS; i++)
	{
		if(psPDSInfo->ui32NonDependentImageUnits & (1UL << i))
		{
			ui32HashCompareSizeInDWords += EURASIA_TAG_TEXTURE_STATE_SIZE;
		}
	}

	pui32HashCompare = gc->sProgram.uTempBuffer.aui32HashCompare;

	ui32HashCompareSizeInDWords = 0;
	ui32HashInputSizeInDWords = 0;

	pui32HashCompare[ui32HashCompareSizeInDWords++] = psFragmentVariant->sStartAddress[0].uiAddr;
	aui32HashInput[ui32HashInputSizeInDWords++] =  psFragmentVariant->sStartAddress[0].uiAddr;

	for(i=0; i < GLES1_MAX_TEXTURE_IMAGE_UNITS; i++)
	{
		if(psPDSInfo->ui32NonDependentImageUnits & (1UL << i))
		{
			pui32HashCompare[ui32HashCompareSizeInDWords++] = psTextureImageUnit[i].ui32TAGControlWord0;
			pui32HashCompare[ui32HashCompareSizeInDWords++] = psTextureImageUnit[i].ui32TAGControlWord1;
			pui32HashCompare[ui32HashCompareSizeInDWords++] = psTextureImageUnit[i].ui32TAGControlWord2;
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
			pui32HashCompare[ui32HashCompareSizeInDWords++] = psTextureImageUnit[i].ui32TAGControlWord3;
#endif		

			/* Use Texture address */
			aui32HashInput[ui32HashInputSizeInDWords++] = psTextureImageUnit[i].ui32TAGControlWord2;
		}
	}

	pui32HashCompare[ui32HashCompareSizeInDWords++] = psPDSInfo->ui32NonDependentImageUnits;

	tPDSVariantHash = HashFunc(aui32HashInput, ui32HashInputSizeInDWords, STATEHASH_INIT_VALUE);
	psPDSVariant = IMG_NULL;

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
		/* Adjust PDS execution address for restricted address range */
		uBaseAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

		if((gc->sPrim.ui32FragmentPDSDataSize!=psPDSVariant->ui32DataSize) ||
		   (gc->sPrim.uFragmentPDSBaseAddress.uiAddr!=uBaseAddr.uiAddr))
		{
			/*
				Save the PDS pixel shader data segment address and size
			*/
			gc->sPrim.ui32FragmentPDSDataSize = psPDSVariant->ui32DataSize;
			gc->sPrim.uFragmentPDSBaseAddress = uBaseAddr;

			*pbChanged = IMG_TRUE;
		}
		else
		{
			*pbChanged = IMG_FALSE;
		}

		/* Attach PDS fragment variants */
		KRM_Attach(&gc->psSharedState->sPDSVariantKRM, gc->psRenderSurface, 
			       &gc->psRenderSurface->sRenderStatusUpdate, &psPDSVariant->sResource);

		GLES1_INC_COUNT(GLES1_TIMER_PDSVARIANT_HIT_COUNT, 1);
	}
	else
	{
		GLESShaderTextureState *psTextureState = &gc->sPrim.sTextureState;

		GLES1_INC_COUNT(GLES1_TIMER_PDSVARIANT_MISS_COUNT, 1);

		*pbChanged = IMG_TRUE;

		/* Do not cache this PDS variant if any of the active textures has been ghosted. The rationale 
		   is that when an application is ghosting textures there is no point in trying to cache the 
		   PDS variants as the texture addresses are going to change the next time the texture is 
		   ghosted, rendering the cached PDS code useless.

		   Do not insert a new entry in the hash table if there is no available place in the hash table to insert.
		*/
		if(!psTextureState->bSomeTexturesWereGhosted && ValidateHashTableInsert(gc, &gc->sProgram.sPDSFragmentVariantHashTable, tPDSVariantHash))
        {
			psPDSVariant = GLES1Calloc(gc, sizeof(GLES1PDSCodeVariant));

			if(!psPDSVariant)
			{
				PVR_DPF((PVR_DBG_ERROR,"WritePDSPixelShaderProgram: Failed to allocate PDS variant structure"));
				
				return GLES1_HOST_MEM_ERROR;
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

		/* First phase is only selective if there isn't a second phase */
		if(psFragmentVariant->u.sFragment.bMSAATrans && (psFragmentVariant->ui32PhaseCount == 1))
		{
#if (SGX_FEATURE_USE_NUMBER_PC_BITS == 20)
		sProgram.aui32USETaskControl[1] |= EURASIA_PDS_DOUTU1_SAMPLE_RATE_SELECTIVE;
#else
		sProgram.aui32USETaskControl[0] |= EURASIA_PDS_DOUTU0_SAMPLE_RATE_SELECTIVE;
#endif
		}





		/* Set the execution address of the fragment shader phase */
		SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
								0, 
								psFragmentVariant->sStartAddress[0], 
								gc->psSysContext->uUSEFragmentHeapBase, 
								SGX_PIXSHADER_USE_CODE_BASE_INDEX);

#else 
		sProgram.aui32USETaskControl[0]	= psPDSInfo->ui32DependencyControl;
		sProgram.aui32USETaskControl[1]	= psShader->ui32USEMode     |
		                                  ((ui32NumTempsInBlocks << EURASIA_PDS_DOUTU1_TRC_SHIFT) & ~EURASIA_PDS_DOUTU1_TRC_CLRMSK);
		sProgram.aui32USETaskControl[2]	= EURASIA_PDS_DOUTU2_SDSOFT |
		                                   ((ui32NumTempsInBlocks >> EURASIA_PDS_DOUTU2_TRC_INTERNALSHIFT) << EURASIA_PDS_DOUTU2_TRC_SHIFT);

		/* If discard is enabled then we need to set this bit */
		if (gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK)
		{
			sProgram.aui32USETaskControl[1] |= EURASIA_PDS_DOUTU1_PUNCHTHROUGH_PHASE1;
		}



		/*
			Setup the phase enables.
		*/
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
		uPDSPixelShaderBaseAddr.uiAddr = 0;

		for (i = 0; i < sProgram.ui32NumFPUIterators; i++)
		{
			IMG_UINT32 ui32TagIssue = psPDSInfo->aui32LayerControl[i];

#if defined(SGX545)
			sProgram.aui32FPUIterators0[i] = psPDSInfo->aui32TSPParamFetchInterface0[i];
			sProgram.aui32FPUIterators1[i] = psPDSInfo->aui32TSPParamFetchInterface1[i];
#else 
			sProgram.aui32FPUIterators[i] = psPDSInfo->aui32TSPParamFetchInterface[i];
#endif /* defined(SGX545) */

			sProgram.aui32TAGLayers[i]	= ui32TagIssue;

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

#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
			sProgram.aui8LayerSize[i] = 0;
			sProgram.abMinPack[i] = IMG_FALSE;
			sProgram.aui8FormatConv[i] = EURASIA_PDS_MOVS_DOUTT_FCONV_UNCHANGED;
#endif
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
			ui32PDSDataSize = ((ui32PDSDataSizeDS0 + 7) & ~7UL) + ui32PDSDataSizeDS1;
		}
#endif

		ui32PDSProgramSize += (ui32PDSDataSize + ((1UL << (EURASIA_PDS_DATASIZE_ALIGNSHIFT - 2)) - 1)) & ~((1UL << (EURASIA_PDS_DATASIZE_ALIGNSHIFT - 2)) - 1);

		if(psPDSVariant)
		{
			GLES1_TIME_START(GLES1_TIMER_PDSCODEHEAP_FRAG_TIME);
	
			psPDSVariant->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psPDSFragmentCodeHeap,
															ui32PDSProgramSize << 2, gc->psSysContext->hPerProcRef);

			GLES1_TIME_STOP(GLES1_TIMER_PDSCODEHEAP_FRAG_TIME);
			GLES1_INC_COUNT(GLES1_TIMER_PDSCODEHEAP_FRAG_COUNT, ui32PDSProgramSize << 2);
		
			if(psPDSVariant->psCodeBlock)
			{
				pui32BufferBase = psPDSVariant->psCodeBlock->pui32LinAddress;
				uPDSPixelShaderBaseAddr = psPDSVariant->psCodeBlock->sCodeAddress;

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
				GLES1_ASSERT(uPDSPixelShaderBaseAddr.uiAddr >= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr);
#endif
			}
			else
			{
				PVR_DPF((PVR_DBG_WARNING,"WritePDSPixelShaderProgram: Failed to allocate PDS variant"));

				GLES1Free(gc, psPDSVariant);

				psPDSVariant = IMG_NULL;
			}
		}

		if(!psPDSVariant)
		{
			pui32BufferBase = (IMG_UINT32 *) CBUF_GetBufferSpace(gc->apsBuffers, ui32PDSProgramSize,
															CBUF_TYPE_PDS_FRAG_BUFFER, IMG_FALSE);

			if(!pui32BufferBase)
			{
				return GLES1_3D_BUFFER_ERROR;
			}

			uPDSPixelShaderBaseAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_PDS_FRAG_BUFFER);
		}
	
		/*
			Generate the PDS pixel state program
		*/
		pui32Buffer = PDSGeneratePixelShaderProgram(psTextureImageUnit,
													&sProgram,
													pui32BufferBase);

		if(psPDSVariant)
		{
			/* Copy hashcompare data from temp buffer */
			pui32HashCompare = GLES1Malloc(gc, ui32HashCompareSizeInDWords*sizeof(IMG_UINT32));

			if(!pui32HashCompare)
			{
				PVR_DPF((PVR_DBG_ERROR,"WritePDSPixelShaderProgram: Failed to allocate hash compare structure"));
				
				GLES1Free(gc, psPDSVariant);
				return GLES1_HOST_MEM_ERROR;
			}

			GLES1MemCopy(pui32HashCompare, gc->sProgram.uTempBuffer.aui32HashCompare, ui32HashCompareSizeInDWords*sizeof(IMG_UINT32));
			
			psPDSVariant->ui32DataSize = sProgram.ui32DataSize;

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

			/* Attach PDS fragment variants */
			KRM_Attach(&gc->psSharedState->sPDSVariantKRM, gc->psRenderSurface, 
						&gc->psRenderSurface->sRenderStatusUpdate, &psPDSVariant->sResource);

#if defined(PDUMP)
			/* Force redump of shader as new PDS variant has been added */
			psFragmentVariant->psCodeBlock->bDumped = IMG_FALSE;
#endif
		}
		else
		{
			/* Update buffer position */
			CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_PDS_FRAG_BUFFER);

			GLES1_INC_COUNT(GLES1_TIMER_PDS_FRAG_DATA_COUNT, (pui32Buffer - pui32BufferBase));
		}

		/*
			Save the PDS pixel shader data segment address and size
		*/
		gc->sPrim.uFragmentPDSBaseAddress = uPDSPixelShaderBaseAddr;

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
		/* Adjust PDS execution address for restricted address range */
		gc->sPrim.uFragmentPDSBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif
		
		gc->sPrim.ui32FragmentPDSDataSize = sProgram.ui32DataSize;
	}

	if(bSetupDMSInfo || ((gc->ui32DirtyMask & GLES1_DIRTYFLAG_DMS_INFO) != 0))
	{
		IMG_UINT32 ui32InstanceCount;

		CalculatePixelDMSInfo(&gc->psSysContext->sHWInfo, psPDSInfo->ui32DMSInfoIterationSize + psPDSInfo->ui32DMSInfoTextureSize, 
								ui32NumTemps, psShader->ui32USESecAttribDataSizeInDwords, 1, 0, &psPDSInfo->ui32DMSInfo, &ui32InstanceCount);

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
		psPDSInfo->ui32DMSInfo &= ~EURASIA_PDS_PT_SECONDARY;

#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
		if((gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF) == 0)
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
		{
			if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK)
			{
				psPDSInfo->ui32DMSInfo |= EURASIA_PDS_PT_SECONDARY;
			}
		}	
#endif
	}

	return GLES1_NO_ERROR;
}

/***********************************************************************************
 Function Name      : ReclaimPDSVariantMemKRM
 Inputs             : gc, psResource
 Outputs            : -
 Returns            : -
 Description        : Reclaims the device memory of a use shader variant. This function
					  also destroys the variant itself, as we are not managing a top 
					  level GL resource and we can easily create a new version of the 
					  variant.
************************************************************************************/
IMG_INTERNAL IMG_VOID ReclaimPDSVariantMemKRM(IMG_VOID *pvContext, KRMResource *psResource)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(psResource);

	PVR_DPF((PVR_DBG_ERROR,"ReclaimPDSVariantMemKRM: Shouldn't be called ever"));
}


/***********************************************************************************
 Function Name      : DestroyPDSVariantGhostKRM
 Inputs             : gc, psResource
 Outputs            : -
 Returns            : -
 Description        : Destroys a ghosted PDS code variant.
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyPDSVariantGhostKRM(IMG_VOID *pvContext, KRMResource *psResource)
{
	PVR_UNREFERENCED_PARAMETER(pvContext);
	PVR_UNREFERENCED_PARAMETER(psResource);

	PVR_DPF((PVR_DBG_ERROR,"DestroyPDSVariantGhostKRM: Shouldn't be called ever"));
}


/*****************************************************************************
 Function Name	: PatchAuxiliaryPDSVertexShaderProgram
 Inputs			: 
 Outputs		: 
 Returns		:
 Description	: patch pregenerated auxiliary program already allocated in 
				  the circular buffer with current streams values	 
*****************************************************************************/
static GLES1_MEMERROR PatchAuxiliaryPDSVertexShaderProgram(GLES1Context *gc)
{
	IMG_UINT32 *pui32Buffer, *pui32BufferBase;
	IMG_UINT32 ui32PDSProgramSizeInDWords, ui32ProgramDataSize;
	IMG_DEV_VIRTADDR uAuxiliaryPDSVertexShaderBaseAddr;
	IMG_UINT32 i;
	IMG_UINT32 aui32Streams[GLES1_STATIC_PDS_NUMBER_OF_STREAMS];
	GLES1PDSVertexCodeVariant *psPDSVertexCode = gc->sProgram.psCurrentVertexVariant->psPDSVertexCodeVariant;

	GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);

	/* Return if there's no PDS vertex variant */
	if(!psPDSVertexCode)
	{
		PVR_DPF((PVR_DBG_ERROR,"PatchAuxiliaryPDSVertexShaderProgram: No PDS vertex program!"));

		return GLES1_GENERAL_MEM_ERROR;
	}
		
	/* Assert VAO */
	GLES1_ASSERT(VAO(gc));

	/*
		Get buffer space and write new auxiliary program
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, ALIGNED_PDS_AUXILIARY_PROG_SIZE, CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER, IMG_FALSE);
	
	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	if (gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
	{
		/* Setup copies for all of the streams */
		for (i = 0; i < psVAOMachine->ui32NumItemsPerVertex; i++)
		{
			GLES1AttribArrayPointerMachine *psPackedAttrib = psVAOMachine->apsPackedAttrib[i];
			IMG_DEV_VIRTADDR uHWStreamAddress;
			
			/* A non-zero buffer name meams the stream address is actually an offset */
			if(psPackedAttrib->psState->psBufObj && !psPackedAttrib->bIsCurrentState)
			{
				IMG_DEV_VIRTADDR uDevAddr;
 				IMG_UINTPTR_T uOffset;
 
 				uDevAddr.uiAddr = psPackedAttrib->psState->psBufObj->psMemInfo->sDevVAddr.uiAddr;			
				uOffset = (IMG_UINTPTR_T)psPackedAttrib->pvPDSSrcAddress;

				/* If the pointer is beyond the buffer object, it is probably an application bug - just set it
					to 0, to try to prevent an SGX page fault
				*/
				if(uOffset > psPackedAttrib->psState->psBufObj->psMemInfo->uAllocSize)
				{
					PVR_DPF((PVR_DBG_WARNING,"PatchAuxiliaryPDSVertexShaderProgram: Offset to VBO out of bounds. Likely an app bug! Setting offset to zero"));
					uOffset = 0;
				}

 				/* add in offset */
				uHWStreamAddress.uiAddr = uDevAddr.uiAddr + uOffset;
			}
			else
			{
				/* Get the device address of the buffer */
				uHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers,
															   psPackedAttrib->pvPDSSrcAddress,
															   CBUF_TYPE_VERTEX_DATA_BUFFER);
			}

			/* store stream  */

			aui32Streams[i] = uHWStreamAddress.uiAddr;
		}
	}
	else
	{
		IMG_DEV_VIRTADDR uHWStreamAddress;

		/* Get the device address of the buffer */
		uHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers, (IMG_UINT32 *)gc->sPrim.pvDrawTextureAddr, CBUF_TYPE_VERTEX_DATA_BUFFER);

		/* Store address of colour data */
		aui32Streams[0] = uHWStreamAddress.uiAddr;

		/* Move past colour data to position data */
		uHWStreamAddress.uiAddr += 4*4;

		for (i=1; i<2+gc->ui32NumImageUnitsActive; i++)
		{
			/* store stream address */
			aui32Streams[i] = uHWStreamAddress.uiAddr;

			/* Position/texture data is 16 floats */
			uHWStreamAddress.uiAddr += 16*4;
		}
	}
	
	/* patch the aux program with current streams */
	PDSAuxiliaryVertexSetSTREAM0(pui32BufferBase, aui32Streams[0]);
	PDSAuxiliaryVertexSetSTREAM1(pui32BufferBase, aui32Streams[1]);
	PDSAuxiliaryVertexSetSTREAM2(pui32BufferBase, aui32Streams[2]);
	PDSAuxiliaryVertexSetSTREAM3(pui32BufferBase, aui32Streams[3]);
	PDSAuxiliaryVertexSetSTREAM4(pui32BufferBase, aui32Streams[4]);
	PDSAuxiliaryVertexSetSTREAM5(pui32BufferBase, aui32Streams[5]);
	PDSAuxiliaryVertexSetSTREAM6(pui32BufferBase, aui32Streams[6]);
	PDSAuxiliaryVertexSetSTREAM7(pui32BufferBase, aui32Streams[7]);

	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_AUXILIARY_COUNT, 
		(gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_DRAWTEXTURE ? 2 + gc->ui32NumImageUnitsActive : psVAOMachine->ui32NumItemsPerVertex));
	
	uAuxiliaryPDSVertexShaderBaseAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER);

	/* 
	   Update buffer position 
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, ALIGNED_PDS_AUXILIARY_PROG_SIZE, CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER);

	ui32ProgramDataSize = PDS_AUXILIARYVERTEX_DATA_SEGMENT_SIZE;
	
#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
	/* Adjust PDS execution address for restricted address range */
	uAuxiliaryPDSVertexShaderBaseAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

	/***************************************************************
		Get VDM control stream space for the PDS vertex state block
	****************************************************************/

	/* FIXME: do we need NUM_SECATTR_STATE_BLOCKS? Possibly no */
	ui32PDSProgramSizeInDWords = MAX_DWORDS_PER_PDS_STATE_BLOCK;

	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, ui32PDSProgramSizeInDWords, CBUF_TYPE_VDM_CTRL_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	pui32Buffer = pui32BufferBase;

	/*
		Write the vertex auxiliary state update PDS data block
	*/
	*pui32Buffer++	= EURASIA_TAOBJTYPE_STATE |
						(((uAuxiliaryPDSVertexShaderBaseAddr.uiAddr >> EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) 
						<< EURASIA_TAPDSSTATE_BASEADDR_SHIFT) & ~EURASIA_TAPDSSTATE_BASEADDR_CLRMSK);
		
	*pui32Buffer++	= (((ui32ProgramDataSize >> EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) 
							<< EURASIA_TAPDSSTATE_DATASIZE_SHIFT) & ~EURASIA_TAPDSSTATE_DATASIZE_CLRMSK) | 
							EURASIA_TAPDSSTATE_USEPIPE_ALL;


	/*
		Lock TA control stream space for the PDS vertex state block
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_VDM_CTRL_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_VDM_CTRL_STATE_COUNT, (pui32Buffer - pui32BufferBase));

	return GLES1_NO_ERROR;

}


/*****************************************************************************
 Function Name	: WritePDSStaticVertexShaderProgram
 Inputs			: 
 Outputs		: 
 Returns		:
 Description	: generate static PDS vertex program - do it only once per vertex
				variant (unless the stride has changed), then just update streams
				info and retrive the proper PDS vertex program
*****************************************************************************/
static GLES1_MEMERROR WritePDSStaticVertexShaderProgram(GLES1Context *gc, IMG_BOOL b32BitPDSIndices)
{
	IMG_UINT32          i, *pui32Buffer, *pui32BufferBase, ui32MaxReg = 0; 
	GLES1Shader        *psVertexShader = gc->sProgram.psCurrentVertexShader;
	GLES1ShaderVariant *psVertexVariant = gc->sProgram.psCurrentVertexVariant;
	IMG_UINT32          ui32NumTemps; 
	IMG_UINT32          aui32CurrentStride[GLES1_MAX_ATTRIBS_ARRAY] = {0};

	GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);
	GLES1VertexArrayObject    *psVAO = psVAOMachine->psActiveVAO;
	GLES1PDSVertexCodeVariant *psPDSVertexCode;
	GLES1PDSVertexState       *psPDSVertexState;
	PDS_VERTEX_SHADER_PROGRAM *psPDSVertexShaderProgram;
	IMG_DEV_VIRTADDR           uPDSVertexShaderBaseAddr;


	IMG_BOOL bSameStride;


	/* Assert VAO and setup its PDS states and PDS program */
	GLES1_ASSERT(psVAO);


	/* Return if there's no variant */
	if(!psVertexVariant)
	{
		PVR_DPF((PVR_DBG_ERROR,"WritePDSStaticVertexShaderProgram: No vertex shader!"));

		return GLES1_GENERAL_MEM_ERROR;
	}

	psPDSVertexCode = psVertexVariant->psPDSVertexCodeVariant;
	

	/* Assert VAO and setup its PDS states and PDS program */
	GLES1_ASSERT(VAO(gc));
	psPDSVertexState         = psVAO->psPDSVertexState;
	psPDSVertexShaderProgram = psVAO->psPDSVertexShaderProgram;


	/* Allocate space for psPDSVertexState */
	if (!psPDSVertexState)
	{
		psPDSVertexState = (GLES1PDSVertexState *) GLES1Calloc(gc, sizeof(GLES1PDSVertexState));

		if (psPDSVertexState == IMG_NULL)
		{
			GLES1Free(gc, psVAO);
				
			return GLES1_GENERAL_MEM_ERROR;
		}
		psVAO->psPDSVertexState = psPDSVertexState;
	}
	  
	/* Allocate space for psPDSVertexShaderProgram */
	if (!psPDSVertexShaderProgram)
	{
		psPDSVertexShaderProgram = (PDS_VERTEX_SHADER_PROGRAM *) GLES1Calloc(gc, sizeof(PDS_VERTEX_SHADER_PROGRAM));

		if (psPDSVertexShaderProgram == IMG_NULL)
		{
			GLES1Free(gc, psVAO);

			return GLES1_GENERAL_MEM_ERROR;
		}
		psVAO->psPDSVertexShaderProgram = psPDSVertexShaderProgram;
	}


	/*
		How this works:
		1- generate a quick description of requirements for the PDS program
		2- if the vertex variant has already a program allocated _and_ if
			the strides have not changed, retrieve PDS program and exit
		3- else do a complete description of requirements and generate PDS program 
	*/

	while(psPDSVertexCode)
	{
	
		/************************************************************************	
		1 - quick description == have strides changed?
		*************************************************************************/

		bSameStride = IMG_TRUE;

		if (gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
		{
			
			/* Setup copies for all of the streams */
			for (i = 0; i < psVAOMachine->ui32NumItemsPerVertex; i++)
			{
				GLES1AttribArrayPointerMachine *psPackedAttrib = psVAOMachine->apsPackedAttrib[i];
				
				if(psPackedAttrib->bIsCurrentState)
				{				
					aui32CurrentStride[i]  = 0;				
				}
				else if (psPackedAttrib->psState->psBufObj)
				{
					aui32CurrentStride[i] = psPackedAttrib->ui32Stride;				
				}
				else 
				{
					aui32CurrentStride[i] = psPackedAttrib->ui32Size;				
				}				

				if(aui32CurrentStride[i] != psPDSVertexCode->aui32Stride[i])
				{
					bSameStride = IMG_FALSE;
				}
			}
		}
		else
		{

			for (i=0; i<2+gc->ui32NumImageUnitsActive; i++)
			{
				if (i==0)
				{
					aui32CurrentStride[0] = 0;
				}
				else
				{
					aui32CurrentStride[i] = 16;
				}

				if(aui32CurrentStride[i] != psPDSVertexCode->aui32Stride[i])
				{
					bSameStride = IMG_FALSE;
				}
			}
		}

		/****************************************************	
		2 - check if we can retrieve an existing PDS program
		*****************************************************/

		/*
			if same stride reuse PDS program, else we have to generate a new one
		*/
		if(bSameStride && (psPDSVertexCode->b32BitPDSIndices==b32BitPDSIndices))
		{
			uPDSVertexShaderBaseAddr = psPDSVertexCode->psCodeBlock->sCodeAddress;

			gc->sPrim.uVertexPDSBaseAddress = uPDSVertexShaderBaseAddr; 

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
			/* Adjust PDS execution address for restricted address range */
			gc->sPrim.uVertexPDSBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

			gc->sPrim.ui32VertexPDSDataSize = psPDSVertexCode->ui32DataSize;
	
			return GLES1_NO_ERROR;
		}
		else
		{
			/*
				else loop on the next PDS vertex program variant
			*/
			psPDSVertexCode = psPDSVertexCode->psNext;
		}
	
		
	}/* while loop */

	/***********************
	3 - generate program 	
	************************/

	/* allocate PDS vertex code struct */
	psPDSVertexCode = GLES1Calloc(gc, sizeof(GLES1PDSVertexCodeVariant));

	if(!psPDSVertexCode)
	{
		PVR_DPF((PVR_DBG_ERROR,"WritePDSStaticVertexShaderProgram: Failed to allocate PDS vertex variant structure"));

		return GLES1_HOST_MEM_ERROR;
	}	

	psPDSVertexCode->psNext = psVertexVariant->psPDSVertexCodeVariant;

	psVertexVariant->psPDSVertexCodeVariant = psPDSVertexCode;
	

	ui32NumTemps = psVertexVariant->ui32MaxTempRegs;

	/* Align to temp allocation granularity */
	ui32NumTemps = ALIGNCOUNTINBLOCKS(ui32NumTemps, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);

	/* Setup the input program structure */
	psPDSVertexShaderProgram->pui32DataSegment = IMG_NULL;
	psPDSVertexShaderProgram->ui32DataSize	 = 0;
	psPDSVertexShaderProgram->b32BitIndices	 = b32BitPDSIndices;
	psPDSVertexShaderProgram->ui32NumInstances = 0;
	psPDSVertexShaderProgram->bIterateVtxID = IMG_FALSE;
	psPDSVertexShaderProgram->bIterateInstanceID = IMG_FALSE;

	psPDSVertexCode->b32BitPDSIndices = b32BitPDSIndices;

	if (gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
	{
		psPDSVertexShaderProgram->ui32NumStreams = psVAOMachine->ui32NumItemsPerVertex;
	}
	else
	{
		psPDSVertexShaderProgram->ui32NumStreams = 2 + gc->ui32NumImageUnitsActive;
	}
	
	/* Set the USE Task control words */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	psPDSVertexShaderProgram->aui32USETaskControl[0]	= (ui32NumTemps << EURASIA_PDS_DOUTU0_TRC_SHIFT) & ~EURASIA_PDS_DOUTU0_TRC_CLRMSK;
	psPDSVertexShaderProgram->aui32USETaskControl[1]	= psVertexShader->ui32USEMode;
	psPDSVertexShaderProgram->aui32USETaskControl[2]	= 0;

	/* Set the execution address of the vertex shader phase */
	SetUSEExecutionAddress(&psPDSVertexShaderProgram->aui32USETaskControl[0], 
							0, 
							psVertexVariant->sStartAddress[0], 
							gc->psSysContext->uUSEVertexHeapBase, 
							SGX_VTXSHADER_USE_CODE_BASE_INDEX);

#else
	psPDSVertexShaderProgram->aui32USETaskControl[0]	= 0;
	psPDSVertexShaderProgram->aui32USETaskControl[1]	= (psVertexShader->ui32USEMode |
													   ((ui32NumTemps << EURASIA_PDS_DOUTU1_TRC_SHIFT) & ~EURASIA_PDS_DOUTU1_TRC_CLRMSK));
	psPDSVertexShaderProgram->aui32USETaskControl[2]	= (((ui32NumTemps >> EURASIA_PDS_DOUTU2_TRC_INTERNALSHIFT) 
														<< EURASIA_PDS_DOUTU2_TRC_SHIFT));
	/* Load all of the phases */
	for (i = 0; i < psVertexVariant->ui32PhaseCount; i++)
	{
		IMG_UINT32 aui32ValidExecutionEnables[2] = {0, EURASIA_PDS_DOUTU1_EXE1VALID};

		/* Mark execution address as active */
		psPDSVertexShaderProgram->aui32USETaskControl[1] |= aui32ValidExecutionEnables[i];

		/* Set the execution address of the vertex shader phase */
		SetUSEExecutionAddress(&psPDSVertexShaderProgram->aui32USETaskControl[0], 
								i, 
								psVertexVariant->sStartAddress[i], 
								gc->psSysContext->uUSEVertexHeapBase, 
								SGX_VTXSHADER_USE_CODE_BASE_INDEX);
	}
#endif


	if (gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
	{
		/* Setup copies for all of the streams */
		for (i = 0; i < psVAOMachine->ui32NumItemsPerVertex; i++)
		{
			GLES1AttribArrayPointerMachine *psPackedAttrib = psVAOMachine->apsPackedAttrib[i];
			IMG_DEV_VIRTADDR uHWStreamAddress;
			PDS_VERTEX_STREAM *psPDSVertexStream = &(psPDSVertexShaderProgram->asStreams[i]);
			PDS_VERTEX_ELEMENT *psPDSVertexElement = &(psPDSVertexStream->asElements[0]);
			IMG_UINT32 ui32CurrentAttrib = (IMG_UINT32)(psPackedAttrib - &(psVAOMachine->asAttribPointer[0]));
			VSInputReg *pasVSInputRegisters = &gc->sProgram.psCurrentVertexShader->asVSInputRegisters[0];
			IMG_UINT32 ui32PrimaryAttrReg, ui32PASize;

			/* A non-zero buffer name meams the stream address is actually an offset */
			if(psPackedAttrib->psState->psBufObj && !psPackedAttrib->bIsCurrentState)
			{
				IMG_DEV_VIRTADDR uDevAddr;
				IMG_UINTPTR_T uOffset;

				uDevAddr.uiAddr = psPackedAttrib->psState->psBufObj->psMemInfo->sDevVAddr.uiAddr;
				uOffset = (IMG_UINTPTR_T)psPackedAttrib->pvPDSSrcAddress;

				/* If the pointer is beyond the buffer object, it is probably an application bug - just set it
					to 0, to try to prevent an SGX page fault
				*/
				if(uOffset > psPackedAttrib->psState->psBufObj->psMemInfo->uAllocSize)
				{
					PVR_DPF((PVR_DBG_WARNING,"WritePDSStaticVertexShaderProgram: Offset to VBO out of bounds. Likely an app bug! Setting offset to zero"));
					uOffset = 0;
				}

				/* add in offset */
				uHWStreamAddress.uiAddr = uDevAddr.uiAddr + uOffset;
			}
			else
			{
				/* Get the device address of the buffer */
				uHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers,
															   psPackedAttrib->pvPDSSrcAddress,
															   CBUF_TYPE_VERTEX_DATA_BUFFER);
			}

			/* store stream */			
			psPDSVertexState->sProgramInfo.aui32StreamAddresses[i] = uHWStreamAddress.uiAddr;

			ui32PrimaryAttrReg  = pasVSInputRegisters[ui32CurrentAttrib].ui32PrimaryAttribute;
			ui32PASize			= pasVSInputRegisters[ui32CurrentAttrib].ui32Size;

			psPDSVertexElement->ui32Offset   = 0;

			psPDSVertexElement->ui32Register = ui32PrimaryAttrReg;

			/* Store highest register written to (0 == 4 registers) */
			if (ui32PrimaryAttrReg + ui32PASize > ui32MaxReg)
			{
				ui32MaxReg = ui32PrimaryAttrReg + ui32PASize;
			}

			/* Fill in the PDS descriptor */
			psPDSVertexStream->ui32NumElements = 1;
			psPDSVertexStream->bInstanceData   = IMG_FALSE;
			psPDSVertexStream->ui32Multiplier  = 0;
			psPDSVertexStream->ui32Address     = uHWStreamAddress.uiAddr;

			/* If the data comes from current state it is (4*float) with infinite repeat. */
			if(psPackedAttrib->bIsCurrentState)
			{
				psPDSVertexElement->ui32Size	= 16;

				psPDSVertexStream->ui32Stride	= 0;
				aui32CurrentStride[i]           = 0;
				psPDSVertexStream->ui32Shift    = 31;
			}
			else if (psPackedAttrib->psState->psBufObj)
			{
				psPDSVertexElement->ui32Size    = psPackedAttrib->ui32Size;

				psPDSVertexStream->ui32Stride   = psPackedAttrib->ui32Stride;
				aui32CurrentStride[i]           = psPackedAttrib->ui32Stride;
				psPDSVertexStream->ui32Shift    = 0;
			}
			else
			{
				psPDSVertexElement->ui32Size    = psPackedAttrib->ui32Size;

				psPDSVertexStream->ui32Stride   = psPackedAttrib->ui32Size;
				aui32CurrentStride[i]        	= psPackedAttrib->ui32Size;
				psPDSVertexStream->ui32Shift    = 0;
			}
		}
	}
	else
	{
		VSInputReg *pasVSInputRegisters = &gc->sProgram.psCurrentVertexShader->asVSInputRegisters[0];
		PDS_VERTEX_ELEMENT *psPDSVertexElement;
		PDS_VERTEX_STREAM *psPDSVertexStream;
		IMG_DEV_VIRTADDR uHWStreamAddress;
		IMG_UINT32 ui32PrimaryAttrReg, ui32PASize;

		/* Get the device address of the buffer */
		uHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers, (IMG_UINT32 *)gc->sPrim.pvDrawTextureAddr, CBUF_TYPE_VERTEX_DATA_BUFFER);

		for (i=0; i<2+gc->ui32NumImageUnitsActive; i++)
		{
			IMG_UINT32 ui32InputRegIndex;

			psPDSVertexStream = &(psPDSVertexShaderProgram->asStreams[i]);
			psPDSVertexElement = &(psPDSVertexStream->asElements[0]);

			/* Colour is the first stream, then comes position and then any texture coordinates */ 
			if(i == 0)
			{
				ui32InputRegIndex = AP_COLOR;
			}
			else if(i == 1)
			{
				ui32InputRegIndex = AP_VERTEX;
			}
			else
			{
				ui32InputRegIndex = AP_TEXCOORD0 + gc->ui32TexImageUnitsEnabled[i-2];
			}

			ui32PrimaryAttrReg  = pasVSInputRegisters[ui32InputRegIndex].ui32PrimaryAttribute;
			ui32PASize			= pasVSInputRegisters[ui32InputRegIndex].ui32Size;

			/* Store highest register written to (0 == 4 registers) */
			if (ui32PrimaryAttrReg + ui32PASize > ui32MaxReg)
			{
				ui32MaxReg = ui32PrimaryAttrReg + ui32PASize;
			}

			psPDSVertexElement->ui32Offset   	= 0;
			psPDSVertexElement->ui32Size     	= 16;
			psPDSVertexElement->ui32Register 	= ui32PrimaryAttrReg;

			psPDSVertexStream->ui32NumElements 	= 1;
			psPDSVertexStream->bInstanceData   	= IMG_FALSE;
			psPDSVertexStream->ui32Multiplier  	= 0;
			psPDSVertexStream->ui32Address     	= uHWStreamAddress.uiAddr;

			/* store stream */
			psPDSVertexState->sProgramInfo.aui32StreamAddresses[i] = uHWStreamAddress.uiAddr;

			if (i==0)
			{
				/* Set infinite repeat on colour */
				psPDSVertexStream->ui32Shift  = 31;
				psPDSVertexStream->ui32Stride = 0;

				uHWStreamAddress.uiAddr += 4*4;

			}
			else
			{
				psPDSVertexStream->ui32Shift  = 0;
				psPDSVertexStream->ui32Stride = 16;

				uHWStreamAddress.uiAddr += 16*4;
			}

			aui32CurrentStride[i] = psPDSVertexStream->ui32Stride;	
		}
	}


	/* Generate the PDS Program for this shader */
	pui32Buffer = (IMG_UINT32 *)PDSGenerateStaticVertexShaderProgram(psPDSVertexShaderProgram, psPDSVertexState->aui32LastPDSProgram, &psPDSVertexState->sProgramInfo);

	psPDSVertexState->ui32ProgramSize = (IMG_UINT32)(pui32Buffer - psPDSVertexState->aui32LastPDSProgram);

	//FIXME: we might want to insert some metrics here
	psPDSVertexCode->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psPDSVertexCodeHeap,
														psPDSVertexState->ui32ProgramSize << 2, gc->psSysContext->hPerProcRef);
	
	/* 
		use the PDS vertex heap if possible, otherwise use the circular buffer 
	*/
	if(psPDSVertexCode->psCodeBlock)
	{		
		pui32BufferBase = psPDSVertexCode->psCodeBlock->pui32LinAddress;
		uPDSVertexShaderBaseAddr = psPDSVertexCode->psCodeBlock->sCodeAddress;

		GLES1_ASSERT(uPDSVertexShaderBaseAddr.uiAddr >= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr);

		/* copy PDS program into the heap */	
		GLES1MemCopy(pui32BufferBase, psPDSVertexState->aui32LastPDSProgram, psPDSVertexState->ui32ProgramSize << 2);

		/* store PDS data size */
		psPDSVertexCode->ui32DataSize = psPDSVertexShaderProgram->ui32DataSize;

		/* store current strides - this can change within the same vertex variant */ 
		for(i = 0; i < GLES1_MAX_ATTRIBS_ARRAY; i++)
		{
			psPDSVertexCode->aui32Stride[i]   = aui32CurrentStride[i];
		}
#if defined(PDUMP)
		/* Force redump of shader as new PDS variant has been added */
		psPDSVertexCode->psCodeBlock->bDumped = IMG_FALSE;
#endif
	}
	else
	{
		/*
			Get buffer space for the PDS vertex program
		*/
		pui32BufferBase = (IMG_UINT32 *) CBUF_GetBufferSpace(gc->apsBuffers, psPDSVertexState->ui32ProgramSize, 
														CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);

		if(!pui32BufferBase)
		{
			return GLES1_TA_BUFFER_ERROR;
		}

		GLES1MemCopy(pui32BufferBase, psPDSVertexState->aui32LastPDSProgram, psPDSVertexState->ui32ProgramSize << 2);

		/* Update buffer position */
		CBUF_UpdateBufferPos(gc->apsBuffers, psPDSVertexState->ui32ProgramSize, CBUF_TYPE_PDS_VERT_BUFFER);



		/*
		  Save the PDS vertex shader data segment physical address and size
		*/
		uPDSVertexShaderBaseAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, 
															   pui32BufferBase, 
															   CBUF_TYPE_PDS_VERT_BUFFER);
	}
	
	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, psPDSVertexState->ui32ProgramSize);

	gc->sPrim.uVertexPDSBaseAddress = uPDSVertexShaderBaseAddr; 

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
	/* Adjust PDS execution address for restricted address range */
	gc->sPrim.uVertexPDSBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

	gc->sPrim.ui32VertexPDSDataSize = psPDSVertexShaderProgram->ui32DataSize;
	
	/* Reset sPrim.psPDSVertexState */
	gc->sPrim.psPDSVertexState = psVAO->psPDSVertexState;


	psVertexVariant->ui32USEPrimAttribCount  = ui32MaxReg;

	/* Reset VAO's dirty flag */
    psVAO->ui32DirtyMask &= ~GLES1_DIRTYFLAG_VAO_ALL;


	return GLES1_NO_ERROR;
}


/*****************************************************************************
 Function Name	: WritePDSVertexShaderProgramWithVAO
 Inputs			: 
 Outputs		: 
 Returns		:
 Description	: 
*****************************************************************************/
static GLES1_MEMERROR WritePDSVertexShaderProgramWithVAO(GLES1Context *gc, IMG_BOOL b32BitPDSIndices)
{
    IMG_UINT32          i, *pui32Buffer;
	GLES1Shader        *psVertexShader  = gc->sProgram.psCurrentVertexShader;
	GLES1ShaderVariant *psVertexVariant = gc->sProgram.psCurrentVertexVariant;
	IMG_UINT32          ui32NumTemps;
	IMG_UINT32          ui32MaxReg;
	IMG_DEV_VIRTADDR	uVertexPDSBaseAddress;

	GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);
	GLES1VertexArrayObject    *psVAO = psVAOMachine->psActiveVAO;
	GLES1PDSVertexState       *psPDSVertexState;
	PDS_VERTEX_SHADER_PROGRAM *psPDSVertexShaderProgram;

    IMG_BOOL bDirty = (gc->ui32DirtyMask & ( GLES1_DIRTYFLAG_VP_STATE           |
											 GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM  |
											 GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER |
											 GLES1_DIRTYFLAG_VAO_BINDING        |
											 GLES1_DIRTYFLAG_PRIM_INDEXBITS     )) ? IMG_TRUE : IMG_FALSE;

	IMG_BOOL bDirtyOnlyBinding = (gc->ui32DirtyMask & ( GLES1_DIRTYFLAG_VP_STATE           |
														GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM  |
														GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER |
														GLES1_DIRTYFLAG_PRIM_INDEXBITS     )) == 0 ? IMG_TRUE : IMG_FALSE;

	IMG_BOOL bDirtyOnlyPointerBinding = (gc->ui32DirtyMask & ( GLES1_DIRTYFLAG_VP_STATE          |
															   GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM |
															   GLES1_DIRTYFLAG_PRIM_INDEXBITS    )) == 0 ? IMG_TRUE : IMG_FALSE;
	
	IMG_BOOL bDirtyProgram = bDirtyOnlyBinding ? IMG_FALSE : IMG_TRUE;


	/* Assert VAO and setup its PDS states and PDS program */
	GLES1_ASSERT(VAO(gc));
	psPDSVertexState         = psVAO->psPDSVertexState;
	psPDSVertexShaderProgram = psVAO->psPDSVertexShaderProgram;


	/* Return if there's no variant */
	if(!psVertexVariant)
	{
		PVR_DPF((PVR_DBG_ERROR,"WritePDSVertexShaderProgramWithVAO: No vertex shader!"));

		return GLES1_GENERAL_MEM_ERROR;
	}
	ui32NumTemps = psVertexVariant->ui32MaxTempRegs;
	ui32MaxReg   = psVertexVariant->ui32USEPrimAttribCount;


	GLES1_TIME_START(GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);


	GLES1_TIME_START(GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_GENERATE_OR_PATCH_TIME);
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

	    IMG_BOOL bOkToPatch = IMG_TRUE;


	    /* Allocate space for psPDSVertexState */
		if (!psPDSVertexState)
		{
			psPDSVertexState = (GLES1PDSVertexState *) GLES1Calloc(gc, sizeof(GLES1PDSVertexState));

			if (psPDSVertexState == IMG_NULL)
			{
				GLES1Free(gc, psVAO);
				
				GLES1_TIME_STOP(GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);
				
				return GLES1_GENERAL_MEM_ERROR;
			}
			psVAO->psPDSVertexState = psPDSVertexState;
		}
	  

	    /* Allocate space for psPDSVertexShaderProgram */
		if (!psPDSVertexShaderProgram)
		{
			psPDSVertexShaderProgram = (PDS_VERTEX_SHADER_PROGRAM *) GLES1Calloc(gc, sizeof(PDS_VERTEX_SHADER_PROGRAM));

			if (psPDSVertexShaderProgram == IMG_NULL)
			{
				GLES1Free(gc, psVAO);

				GLES1_TIME_STOP(GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);

				return GLES1_GENERAL_MEM_ERROR;
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

			psPDSVertexState->sProgramInfo.bPatchTaskControl = IMG_FALSE;
			

			if (gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
			{
				/* Setup addresses for all of the streams */
				for (i = 0; i < psVAOMachine->ui32NumItemsPerVertex; i++)
				{
				    GLES1AttribArrayPointerMachine *psVAOAttribPointer = psVAOMachine->apsPackedAttrib[i];
					GLES1AttribArrayPointerState   *psVAOAPState = psVAOAttribPointer->psState;
					IMG_DEV_VIRTADDR    uHWStreamAddress;

					/* A non-zero buffer name meams the stream address is actually an offset */
					if(psVAOAPState->psBufObj && !psVAOAttribPointer->bIsCurrentState)
					{
						IMG_DEV_VIRTADDR uDevAddr;
						IMG_UINTPTR_T uOffset = (IMG_UINTPTR_T)psVAOAttribPointer->pvPDSSrcAddress;

						uDevAddr.uiAddr = psVAOAPState->psBufObj->psMemInfo->sDevVAddr.uiAddr;
						
						/* If the pointer is beyond the buffer object, it is probably an application bug - just set it
						   to 0, to try to prevent an SGX page fault */
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
																	   (const IMG_UINT32 *)psVAOAttribPointer->pvPDSSrcAddress,
																	   CBUF_TYPE_VERTEX_DATA_BUFFER);
					}

					psPDSVertexState->sProgramInfo.aui32StreamAddresses[ui32NumStreams++] = uHWStreamAddress.uiAddr;

					/* If the next stream was merged with this one, make sure that it's still able to be merged */
					if(((i+1)<psVAOMachine->ui32NumItemsPerVertex) && psPDSVertexState->bMerged[i])
					{
						GLES1AttribArrayPointerMachine *psNextVAOAttribPointer = psVAOMachine->apsPackedAttrib[i+1];
						IMG_DEV_VIRTADDR uNextHWStreamAddress;

						GLES1_ASSERT(!psNextVAOAttribPointer->bIsCurrentState);

						/* A non-zero buffer name meams the stream address is actually an offset */
						if(psNextVAOAttribPointer->psState->psBufObj)
						{
							IMG_DEV_VIRTADDR uDevAddr;
 							IMG_UINTPTR_T uOffset;

							uDevAddr.uiAddr = psNextVAOAttribPointer->psState->psBufObj->psMemInfo->sDevVAddr.uiAddr;
							uOffset = (IMG_UINTPTR_T)psNextVAOAttribPointer->pvPDSSrcAddress;

							/* If the pointer is beyond the buffer object, it is probably an application bug - just set it
								to 0, to try to prevent an SGX page fault
							*/
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
																			   (const IMG_UINT32 *)psNextVAOAttribPointer->pvPDSSrcAddress,
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
			}
			else
			{
				IMG_DEV_VIRTADDR uHWStreamAddress;

				uHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers, 
															   (IMG_UINT32 *)gc->sPrim.pvDrawTextureAddr, 
															   CBUF_TYPE_VERTEX_DATA_BUFFER);
				
				/* Store address of colour data */
				psPDSVertexState->sProgramInfo.aui32StreamAddresses[0] = uHWStreamAddress.uiAddr;
				
				/* Move past colour data to position data */
				uHWStreamAddress.uiAddr += 4*4;
				
				for (i=1; i<2+gc->ui32NumImageUnitsActive; i++)
				{
					/* store stream address */
					psPDSVertexState->sProgramInfo.aui32StreamAddresses[i] = uHWStreamAddress.uiAddr;

					/* Position/texture data is 16 floats */
					uHWStreamAddress.uiAddr += 16*4;
				}
			}

			if(bOkToPatch)
			{
				PDSPatchVertexShaderProgram( &(psPDSVertexState->sProgramInfo), 
											 psPDSVertexState->aui32LastPDSProgram );
			}
		}


		if (!bOkToPatch ||
			!bDirtyOnlyPointerBinding)
		{
		    /*
			  For all the other dirty cases and the case when patching PDS program is unachievable, 

			  PDS vertex shader program has to be generated.
			*/

		    /* Setup the input program structure */
		    psPDSVertexShaderProgram->pui32DataSegment   = IMG_NULL;
			psPDSVertexShaderProgram->ui32DataSize	   = 0;
			psPDSVertexShaderProgram->b32BitIndices	   = b32BitPDSIndices;
			psPDSVertexShaderProgram->ui32NumInstances   = 0;
			psPDSVertexShaderProgram->bIterateVtxID      = IMG_FALSE;
			psPDSVertexShaderProgram->bIterateInstanceID = IMG_FALSE;

			/* Align to temp allocation granularity */
			ui32NumTemps = ALIGNCOUNTINBLOCKS(ui32NumTemps, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);


			if (gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
			{
				/* This will be incremented as the streams are defined */
				psPDSVertexShaderProgram->ui32NumStreams = 0;
			}
			else
			{
				psPDSVertexShaderProgram->ui32NumStreams = 2 + gc->ui32NumImageUnitsActive;
			}

			/* Align to temp allocation granularity */
			ui32NumTemps = ALIGNCOUNTINBLOCKS(ui32NumTemps, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);	


			/* Set the USE Task control words */
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
 
			psPDSVertexShaderProgram->aui32USETaskControl[0]	= ((ui32NumTemps << EURASIA_PDS_DOUTU0_TRC_SHIFT)
																                 & ~EURASIA_PDS_DOUTU0_TRC_CLRMSK);
			psPDSVertexShaderProgram->aui32USETaskControl[1]	= psVertexShader->ui32USEMode;
			psPDSVertexShaderProgram->aui32USETaskControl[2]	= 0;
			
			/* Set the execution address of the vertex shader phase */
			SetUSEExecutionAddress(&(psPDSVertexShaderProgram->aui32USETaskControl[0]), 
								   0, 
								   psVertexVariant->sStartAddress[0], 
								   gc->psSysContext->uUSEVertexHeapBase, 
								   SGX_VTXSHADER_USE_CODE_BASE_INDEX);

#else /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

			psPDSVertexShaderProgram->aui32USETaskControl[0]	= 0;
			psPDSVertexShaderProgram->aui32USETaskControl[1]	= psVertexShader->ui32USEMode |
			                                                      ((ui32NumTemps << EURASIA_PDS_DOUTU1_TRC_SHIFT) 
																                 & ~EURASIA_PDS_DOUTU1_TRC_CLRMSK);

			psPDSVertexShaderProgram->aui32USETaskControl[2]	= ((ui32NumTemps >> EURASIA_PDS_DOUTU2_TRC_INTERNALSHIFT) 
																                 << EURASIA_PDS_DOUTU2_TRC_SHIFT);

			/* Load all of the phases */
			for (i = 0; i < psVertexVariant->ui32PhaseCount; i++)
			{
				IMG_UINT32 aui32ValidExecutionEnables[2] = {0, EURASIA_PDS_DOUTU1_EXE1VALID};

				/* Set the execution address of the fragment shader phase */
				SetUSEExecutionAddress(&(psPDSVertexShaderProgram->aui32USETaskControl[0]), 
									   i, 
									   psVertexVariant->sStartAddress[i], 
									   gc->psSysContext->uUSEVertexHeapBase, 
									   SGX_VTXSHADER_USE_CODE_BASE_INDEX);

				/* Mark execution address as active */
				psPDSVertexShaderProgram->aui32USETaskControl[1] |= aui32ValidExecutionEnables[i];
			}
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */


			if (gc->sPrim.eCurrentPrimitiveType != GLES1_PRIMTYPE_DRAWTEXTURE)
			{
				/* Setup copies for all of the streams */
				for (i = 0; i < psVAOMachine->ui32NumItemsPerVertex; i++)
				{
				    GLES1AttribArrayPointerMachine *psVAOAttribPointer = psVAOMachine->apsPackedAttrib[i];
					IMG_DEV_VIRTADDR    uHWStreamAddress;
					PDS_VERTEX_STREAM  *psPDSVertexStream    = &(psPDSVertexShaderProgram->asStreams[psPDSVertexShaderProgram->ui32NumStreams++]);
					PDS_VERTEX_ELEMENT *psPDSVertexElement   = &(psPDSVertexStream->asElements[0]);
					IMG_UINT32          ui32CurrentAttrib = (IMG_UINT32)(psVAOAttribPointer - &(psVAOMachine->asAttribPointer[0]));
					VSInputReg         *pasVSInputRegisters  = &gc->sProgram.psCurrentVertexShader->asVSInputRegisters[0];
					IMG_UINT32          ui32PrimaryAttrReg, ui32PASize;

					/* A non-zero buffer name meams the stream address is actually an offset */
					if(psVAOAttribPointer->psState->psBufObj && !psVAOAttribPointer->bIsCurrentState)
					{
						IMG_DEV_VIRTADDR uDevAddr;
 						IMG_UINTPTR_T uOffset;

						uDevAddr.uiAddr = psVAOAttribPointer->psState->psBufObj->psMemInfo->sDevVAddr.uiAddr;			
						uOffset = (IMG_UINTPTR_T)psVAOAttribPointer->pvPDSSrcAddress;

						/* If the pointer is beyond the buffer object, it is probably an application bug - just set it
							to 0, to try to prevent an SGX page fault
						*/
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
						/* Get the device address of the buffer */
						uHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers,
																	   (const IMG_UINT32 *)psVAOAttribPointer->pvPDSSrcAddress,
																	   CBUF_TYPE_VERTEX_DATA_BUFFER);
					}

					ui32PrimaryAttrReg  = pasVSInputRegisters[ui32CurrentAttrib].ui32PrimaryAttribute;
					ui32PASize			= pasVSInputRegisters[ui32CurrentAttrib].ui32Size;
					
					psPDSVertexElement->ui32Offset   = 0;

					psPDSVertexElement->ui32Register = ui32PrimaryAttrReg;
					
					/* Store highest register written to (0 == 4 registers) */
					if (ui32PrimaryAttrReg + ui32PASize > ui32MaxReg)
					{
						ui32MaxReg = ui32PrimaryAttrReg + ui32PASize;
					}

					/* Fill in the PDS descriptor */
					psPDSVertexStream->ui32NumElements = 1;
					psPDSVertexStream->bInstanceData   = IMG_FALSE;
					psPDSVertexStream->ui32Multiplier  = 0;
					psPDSVertexStream->ui32Address     = uHWStreamAddress.uiAddr;

					/* If the data comes from current state it is (4*float) with infinite repeat. */
					if(psVAOAttribPointer->bIsCurrentState)
					{
						psPDSVertexElement->ui32Size	= 16;

						psPDSVertexStream->ui32Stride	= 16;
						psPDSVertexStream->ui32Shift    = 31;
						
						psPDSVertexState->bMerged[i] = IMG_FALSE;
					}
					else 
					{
						if(psVAOAttribPointer->psState->psBufObj)
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
						if((i+1)<psVAOMachine->ui32NumItemsPerVertex)
						{
						    GLES1AttribArrayPointerMachine *psNextVAOAttribPointer = psVAOMachine->apsPackedAttrib[i+1];
							IMG_UINT32 ui32NextAttrib = (IMG_UINT32)(psNextVAOAttribPointer - &(psVAOMachine->asAttribPointer[0]));
							IMG_DEV_VIRTADDR uNextHWStreamAddress;
							IMG_UINT32       ui32NextPrimaryAttribute = pasVSInputRegisters[ui32NextAttrib].ui32PrimaryAttribute;

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
							if(ui32NextPrimaryAttribute != (ui32PrimaryAttrReg + ui32PASize))
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
 								IMG_UINTPTR_T uOffset;

								uOffset = (IMG_UINTPTR_T)psNextVAOAttribPointer->pvPDSSrcAddress;

								/* If the pointer is beyond the buffer object, it is probably an application bug - just set it
									to 0, to try to prevent an SGX page fault
								*/
								if(uOffset > psNextVAOAttribPointer->psState->psBufObj->psMemInfo->uAllocSize)
								{
									PVR_DPF((PVR_DBG_WARNING,"WritePDSVertexShaderProgramWithVAO: Offset to VBO out of bounds. Likely an app bug! Setting offset to zero"));
									uOffset = 0;
								}

								/* Setup next source address */
								uNextDevAddr.uiAddr = psNextVAOAttribPointer->psState->psBufObj->psMemInfo->sDevVAddr.uiAddr; 

								/* add in offset */
								uNextHWStreamAddress.uiAddr = uNextDevAddr.uiAddr + uOffset;
							}
							else
							{
								/* Get the device address of the buffer */
							    uNextHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers,
																				   (const IMG_UINT32 *)psNextVAOAttribPointer->pvPDSSrcAddress,
																				   CBUF_TYPE_VERTEX_DATA_BUFFER);
							}

							/* Source addresses must be sequential */
							if(uNextHWStreamAddress.uiAddr != (uHWStreamAddress.uiAddr + psVAOAttribPointer->ui32Size))
							{
								continue;
							}
					
							/* Store highest register written to (0 == 4 registers) */
							if (ui32NextPrimaryAttribute + pasVSInputRegisters[ui32NextAttrib].ui32Size > ui32MaxReg)
							{
								ui32MaxReg = ui32NextPrimaryAttribute + pasVSInputRegisters[ui32NextAttrib].ui32Size;
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
			}
			else
			{
				VSInputReg *pasVSInputRegisters = &gc->sProgram.psCurrentVertexShader->asVSInputRegisters[0];
				PDS_VERTEX_ELEMENT *psPDSVertexElement;
				PDS_VERTEX_STREAM *psPDSVertexStream;
				IMG_DEV_VIRTADDR uHWStreamAddress;
				IMG_UINT32 ui32PrimaryAttrReg, ui32PASize;
				
				/* Get the device address of the buffer */
				uHWStreamAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers, (IMG_UINT32 *)gc->sPrim.pvDrawTextureAddr, CBUF_TYPE_VERTEX_DATA_BUFFER);

				for (i=0; i<2+gc->ui32NumImageUnitsActive; i++)
				{
					IMG_UINT32 ui32InputRegIndex;

					psPDSVertexStream = &(psPDSVertexShaderProgram->asStreams[i]);
					psPDSVertexElement = &(psPDSVertexStream->asElements[0]);
					
					/* Colour is the first stream, then comes position and then any texture coordinates */ 
					if(i == 0)
					{
						ui32InputRegIndex = AP_COLOR;
					}
					else if(i == 1)
					{
						ui32InputRegIndex = AP_VERTEX;
					}
					else
					{
						ui32InputRegIndex = AP_TEXCOORD0 + gc->ui32TexImageUnitsEnabled[i-2];
					}

					ui32PrimaryAttrReg  = pasVSInputRegisters[ui32InputRegIndex].ui32PrimaryAttribute;
					ui32PASize			= pasVSInputRegisters[ui32InputRegIndex].ui32Size;
					
					/* Store highest register written to (0 == 4 registers) */
					if (ui32PrimaryAttrReg + ui32PASize > ui32MaxReg)
					{
						ui32MaxReg = ui32PrimaryAttrReg + ui32PASize;
					}

					psPDSVertexElement->ui32Offset   	= 0;
					psPDSVertexElement->ui32Size     	= 16;
					psPDSVertexElement->ui32Register 	= ui32PrimaryAttrReg;

					psPDSVertexStream->ui32NumElements 	= 1;
					psPDSVertexStream->bInstanceData   	= IMG_FALSE;
					psPDSVertexStream->ui32Multiplier  	= 0;
					psPDSVertexStream->ui32Address     	= uHWStreamAddress.uiAddr;
					psPDSVertexStream->ui32Stride   	= 16;

					if (i==0)
					{
						/* Set infinite repeat on colour */
						psPDSVertexStream->ui32Shift	= 31;

						uHWStreamAddress.uiAddr += 4*4;
					}
					else
					{
						psPDSVertexStream->ui32Shift	= 0;

						uHWStreamAddress.uiAddr += 16*4;
					}
				}
			}

			/* Generate the PDS Program for this shader */
			pui32Buffer = (IMG_UINT32 *)PDSGenerateVertexShaderProgram(psPDSVertexShaderProgram, 
																	   psPDSVertexState->aui32LastPDSProgram, 
																	   &(psPDSVertexState->sProgramInfo));

			psPDSVertexState->ui32ProgramSize = (IMG_UINT32)(pui32Buffer - (psPDSVertexState->aui32LastPDSProgram));

		}

	} /* Finished setting up VAO's psPDSVertexShaderProgram, psPDSVertexState */

	GLES1_TIME_STOP(GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_GENERATE_OR_PATCH_TIME);




	/* Set up uVertexPDSBaseAddress */

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

		  
			if (GLES1ALLOCDEVICEMEM(gc->ps3DDevData,
									gc->psSysContext->hPDSVertexHeap,
									ui32AllocFlags,
									ui32VAOSize,
									ui32VAOAlign,
									&(psVAO->psMemInfo)) != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"WritePDSVertexShaderProgramWithVAO: Can't allocate meminfo for VAO"));
				psVAO->psMemInfo = IMG_NULL;

				GLES1_TIME_STOP(GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);

				return GLES1_TA_BUFFER_ERROR;
			}

			pvSrc  = (IMG_VOID *)((IMG_UINT8 *)psPDSVertexState->aui32LastPDSProgram);
			pvDest = psVAO->psMemInfo->pvLinAddr;
		
			GLES1MemCopy(pvDest, pvSrc, (psPDSVertexState->ui32ProgramSize << 2));
		}

		/* Retrieve the PDS vertex shader data segment physical address */
		uVertexPDSBaseAddress = psVAO->psMemInfo->sDevVAddr;


		/* Add in the VAO if it possesses MemInfo for PDS program */
		/*
		KRM_Attach(&gc->sVAOKRM, gc, gc->psTAKRMMemInfo->psClientSyncInfo->psSyncData, &psVAO->sResource, IMG_TRUE);
		*/
	}
	else
	{
		/* Get buffer space for the PDS vertex program */
	    IMG_UINT32 *pui32BufferBase;

		pui32BufferBase = (IMG_UINT32 *) CBUF_GetBufferSpace(gc->apsBuffers, 
															 psPDSVertexState->ui32ProgramSize, 
															 CBUF_TYPE_PDS_VERT_BUFFER, 
															 IMG_FALSE);
		if(!pui32BufferBase)
		{
		    GLES1_TIME_STOP(GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);
			return GLES1_TA_BUFFER_ERROR;
		}

		GLES1MemCopy(pui32BufferBase, 
					 psPDSVertexState->aui32LastPDSProgram, 
					 psPDSVertexState->ui32ProgramSize << 2);

		/* Update buffer position */
		CBUF_UpdateBufferPos(gc->apsBuffers, 
							 psPDSVertexState->ui32ProgramSize, 
							 CBUF_TYPE_PDS_VERT_BUFFER);

		GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, psPDSVertexState->ui32ProgramSize);

		/* Retrieve the PDS vertex shader data segment physical address */
		uVertexPDSBaseAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers, 
															pui32BufferBase,
															CBUF_TYPE_PDS_VERT_BUFFER);
	}

	/* Reset VAO's dirty flag */
    psVAO->ui32DirtyMask &= ~GLES1_DIRTYFLAG_VAO_ALL;


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

	GLES1_TIME_STOP(GLES1_TIMER_WRITE_PDS_VERTEX_SHADER_PROGRAM_VAO_TIME);

	return GLES1_NO_ERROR;

}


/*****************************************************************************
 Function Name	: LoadSecondaryAttributes
 Inputs			: gc, ui32ProgramType
 Outputs		: Device address and size of secondaries
 Returns		: Mem Error
 Description	: Loads secondary attribute constants into a buffer for upload
*****************************************************************************/
static GLES1_MEMERROR LoadSecondaryAttributes(GLES1Context *gc,
											IMG_UINT32 ui32ProgramType,
											IMG_DEV_VIRTADDR *puDeviceAddress,
											IMG_UINT32 *pui32DataSizeInDWords)
{
	IMG_UINT32 ui32SABufferType;
	IMG_UINT32 ui32SizeOfConstantsInDWords;
	GLES1Shader *psShader;
	IMG_UINT32 *pui32SecAttribs;
	FFGEN_PROGRAM_DETAILS *psHWCode;
	GLES1_MEMERROR eError;
#if defined(DEBUG)
	IMG_UINT32 i;
#endif

	if (ui32ProgramType == GLES1_SHADERTYPE_VERTEX)
	{
		psShader = gc->sProgram.psCurrentVertexShader;

		ui32SABufferType = CBUF_TYPE_PDS_VERT_BUFFER;

		eError = GLES1_TA_BUFFER_ERROR;
	
		ui32SizeOfConstantsInDWords = GLES1_VERTEX_SECATTR_NUM_RESERVED;
	}
	else
	{
		psShader = gc->sProgram.psCurrentFragmentShader;

		ui32SABufferType = CBUF_TYPE_PDS_FRAG_BUFFER;

		eError = GLES1_3D_BUFFER_ERROR;

		ui32SizeOfConstantsInDWords = GLES1_FRAGMENT_SECATTR_NUM_RESERVED;
	}

	psHWCode = psShader->psFFGENProgramDetails;

	ui32SizeOfConstantsInDWords += psHWCode->ui32SecondaryAttributeCount;

	/* Get buffer space for correct SA-constant amount */
	pui32SecAttribs = CBUF_GetBufferSpace(gc->apsBuffers,
											ui32SizeOfConstantsInDWords,
											ui32SABufferType, IMG_FALSE); 

	if(!pui32SecAttribs)
	{
		return eError;
	}

#if defined(DEBUG)
	for (i = 0; i < ui32SizeOfConstantsInDWords; i++)
	{
		pui32SecAttribs[i] = 0xF0000000 | i;
	}
#endif
	if (ui32ProgramType == GLES1_SHADERTYPE_VERTEX)
	{
		union Float2uint32
		{
			IMG_UINT32 u32;
			IMG_FLOAT  f;
		} u;

		/* Set constants base (-4 to take the prefetch into account) */
		pui32SecAttribs[GLES1_VERTEX_SECATTR_CONSTANTBASE] = psShader->uUSEConstsDataBaseAddress.uiAddr + (IMG_UINT32)(psShader->psFFGENProgramDetails->iSAAddressAdjust);

		/* Set indexable temp base */
		pui32SecAttribs[GLES1_VERTEX_SECATTR_INDEXABLETEMPBASE] = psShader->uIndexableTempBaseAddress.uiAddr;

		/* Set up constants for unpacking of normalized signed bytes */
		u.f = 2.0f/255.0f;
		pui32SecAttribs[GLES1_VERTEX_SECATTR_UNPCKS8_CONST1] = u.u32;

		u.f = 1.0f/255.0f;
		pui32SecAttribs[GLES1_VERTEX_SECATTR_UNPCKS8_CONST2] = u.u32;

		/* Set up constants for unpacking of normalized signed shorts */
		u.f = 2.0f/65535.0f;
		pui32SecAttribs[GLES1_VERTEX_SECATTR_UNPCKS16_CONST1] = u.u32;

		u.f = 1.0f/65535.0f;
		pui32SecAttribs[GLES1_VERTEX_SECATTR_UNPCKS16_CONST2] = u.u32;

		/* For those float constants moved to secondary attribute area */
		if(psHWCode->ui32SecondaryAttributeCount)
		{
			GLES1MemCopy(&pui32SecAttribs[GLES1_VERTEX_SECATTR_NUM_RESERVED], 
						 &psShader->pfConstantData[GLES1_VERTEX_SECATTR_NUM_RESERVED], 
						 psHWCode->ui32SecondaryAttributeCount*sizeof(IMG_FLOAT)); 
		}
	}
	else
	{
		IMG_UINT32 ui32SecAttribOffset;

		/* Return if there's nothing to do */
		if(!psHWCode->ui32SecondaryAttributeCount && !gc->sPrim.sRenderState.ui32AlphaTestFlags &&
		   ((gc->ui32RasterEnables & GLES1_RS_FOG_ENABLE) == 0)
		   )
		{
			CBUF_UpdateBufferPos(gc->apsBuffers, 0, ui32SABufferType);

			*pui32DataSizeInDWords = 0;

			return GLES1_NO_ERROR;
		}

		ui32SecAttribOffset = 0;

		/* Alpha test*/
		if(gc->sPrim.sRenderState.ui32AlphaTestFlags)
		{
			pui32SecAttribs[ui32SecAttribOffset++] = gc->sPrim.sRenderState.ui32ISPFeedback0;
			pui32SecAttribs[ui32SecAttribOffset++] = gc->sPrim.sRenderState.ui32ISPFeedback1;
		}
	
		/* Fog colour */
		if(gc->ui32RasterEnables & GLES1_RS_FOG_ENABLE)
		{
			pui32SecAttribs[ui32SecAttribOffset++] = gc->sState.sFog.ui32Color;
		}


		/* Set min value */
		ui32SizeOfConstantsInDWords = ui32SecAttribOffset;

		/* For those float constants moved to secondary attribute area */
		if(psHWCode->ui32SecondaryAttributeCount)
		{
			GLES1MemCopy(&pui32SecAttribs[ui32SecAttribOffset], 
						 psShader->pfConstantData, 
						 psHWCode->ui32SecondaryAttributeCount*sizeof(IMG_FLOAT)); 

			ui32SizeOfConstantsInDWords += psHWCode->ui32SecondaryAttributeCount;
		}
	}

	/* Update buffer position */
	CBUF_UpdateBufferPos(gc->apsBuffers, ui32SizeOfConstantsInDWords, ui32SABufferType);

	if(ui32ProgramType == GLES1_SHADERTYPE_VERTEX)
	{
		GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, ui32SizeOfConstantsInDWords);		
	}
	else
	{
		GLES1_INC_COUNT(GLES1_TIMER_PDS_FRAG_DATA_COUNT, ui32SizeOfConstantsInDWords);
	}

	/*
	  Record the physcial address of the SA-register constants and their size,
	  for use later by the PDS program that copies them to the USE SA registers.
	*/
	*puDeviceAddress      = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32SecAttribs, ui32SABufferType); 
	*pui32DataSizeInDWords = ui32SizeOfConstantsInDWords;

	return GLES1_NO_ERROR;
}


/*****************************************************************************
 Function Name	: WritePDSUSEShaderSAProgram
 Inputs			: psContext, ui32ProgramType
 Outputs		: pbChanged
 Returns		: Mem Error
 Description	: Writes the PDS program for secondary attributes
*****************************************************************************/
static GLES1_MEMERROR WritePDSUSEShaderSAProgram(GLES1Context *gc, IMG_UINT32 ui32ProgramType, IMG_BOOL *pbChanged)
{
	GLES1Shader *psShader;
	PDS_PIXEL_SHADER_SA_PROGRAM	sProgram = {0};
	IMG_UINT32	ui32StateSizeInDWords;
	IMG_DEV_VIRTADDR uPDSUSEShaderSAStateAddr;
	IMG_DEV_VIRTADDR uPDSUSEShaderSABaseAddr;
	IMG_UINT32 ui32PDSProgramSizeInDWords;
	IMG_UINT32 *pui32Buffer;
	IMG_UINT32 *pui32BufferBase;
	IMG_UINT32 ui32PDSBufferType;
	UCH_UseCodeBlock *psPixelSecondaryCode = gc->sPrim.psDummyPixelSecondaryPDSCode;
	GLES1_MEMERROR eError;


	if (ui32ProgramType == GLES1_SHADERTYPE_VERTEX)
	{
		psShader = gc->sProgram.psCurrentVertexShader;

		ui32PDSBufferType = CBUF_TYPE_PDS_VERT_BUFFER;

		/*
			Write the USE shaders constants to a buffer where they can be accessed 
		*/
		eError = WriteUSEVertexShaderMemConsts(gc);

		if(eError != GLES1_NO_ERROR)
		{
			return eError;
		}
	}
	else
	{
		psShader = gc->sProgram.psCurrentFragmentShader;

		ui32PDSBufferType = CBUF_TYPE_PDS_FRAG_BUFFER;
	}

	/*
		Loads the secondary attributes and records the virtual address of the SA-register constants and their size,
		for use later by the PDS program that copies them to the USE SA registers.
	*/
	eError = LoadSecondaryAttributes(gc, ui32ProgramType, &uPDSUSEShaderSAStateAddr, &ui32StateSizeInDWords);
	
	if(eError != GLES1_NO_ERROR)
	{
		return eError;
	}

	/*
		Fill out the DMA control for the PDS.
	*/
	if (ui32StateSizeInDWords)
	{
		IMG_UINT32 ui32DMAOffset = 0;

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
		if(((gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK) != 0) && (ui32ProgramType == GLES1_SHADERTYPE_FRAGMENT))
		{
			ui32DMAOffset = GLES1_FRAGMENT_SECATTR_ALPHATEST_OFFSET;

			if((gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF) == 0)
			{
				sProgram.bIterateZAbs = IMG_TRUE;
			}
		}
#else /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
		if(((gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK) != 0) && (ui32ProgramType == GLES1_SHADERTYPE_FRAGMENT))
		{
			ui32DMAOffset = GLES1_FRAGMENT_SECATTR_ALPHATEST_OFFSET;
			sProgram.bIterateZAbs = IMG_TRUE;
		}
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
#endif /* defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE) */

		sProgram.ui32NumDMAKicks = EncodeDmaBurst(sProgram.aui32DMAControl, ui32DMAOffset, ui32StateSizeInDWords, uPDSUSEShaderSAStateAddr);
	}
	else if(ui32ProgramType == GLES1_SHADERTYPE_FRAGMENT)
	{
		GLES1PDSInfo *psPDSInfo = &gc->sProgram.psCurrentFragmentVariant->u.sFragment.sPDSInfo;

		IMG_DEV_VIRTADDR uBaseAddr = psPixelSecondaryCode->sCodeAddress;

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
		/* Adjust PDS execution address for restricted address range */
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
				Save the PDS pixel shader data segment address and size
			*/
			gc->sPrim.ui32FragmentPDSSecAttribDataSize = gc->sPrim.ui32DummyPDSPixelSecondaryDataSize;
			gc->sPrim.uFragmentPDSSecAttribBaseAddress = uBaseAddr;

			*pbChanged = IMG_TRUE;
		}
		else
		{
			*pbChanged = IMG_FALSE;
		}

		return GLES1_NO_ERROR;
	}

#if !defined(SGX_FEATURE_SECONDARY_REQUIRES_USE_KICK)
	if(ui32StateSizeInDWords)
#endif
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

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
		if(sProgram.bIterateZAbs)
		{
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			sProgram.aui32USETaskControl[1]	|= EURASIA_PDS_DOUTU1_ITERATORSDEPENDENCY;
#else 
			sProgram.aui32USETaskControl[0]	|= EURASIA_PDS_DOUTU0_ITERATORSDEPENDENCY;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */
		}
#endif

		if(ui32ProgramType == GLES1_SHADERTYPE_VERTEX)
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

	/* Save the secondary attribute size */
	psShader->ui32USESecAttribDataSizeInDwords = ui32StateSizeInDWords;

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
	if(sProgram.bIterateZAbs)
	{
		psShader->ui32USESecAttribDataSizeInDwords += GLES1_FRAGMENT_SECATTR_ALPHATEST_OFFSET;
	}
#endif

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
		return ((ui32PDSBufferType == CBUF_TYPE_PDS_VERT_BUFFER) ? GLES1_TA_BUFFER_ERROR : GLES1_3D_BUFFER_ERROR);
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

	if(ui32ProgramType == GLES1_SHADERTYPE_VERTEX)
	{
		GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, (pui32Buffer - pui32BufferBase));		
	}
	else
	{
		GLES1_INC_COUNT(GLES1_TIMER_PDS_FRAG_DATA_COUNT, (pui32Buffer - pui32BufferBase));
	}

	if (ui32ProgramType == GLES1_SHADERTYPE_FRAGMENT)
	{
		GLES1PDSInfo *psPDSInfo = &gc->sProgram.psCurrentFragmentVariant->u.sFragment.sPDSInfo;
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

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
		/* Adjust PDS execution address for restricted address range */
		gc->sPrim.uFragmentPDSSecAttribBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif

		gc->sPrim.ui32FragmentPDSSecAttribDataSize   = sProgram.ui32DataSize;
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

	return GLES1_NO_ERROR;
}


/***********************************************************************************
 Function Name      : DestroyHashedPDSFragSA
 Inputs             : gc, ui32Item
 Outputs            : -
 Returns            : -
 Description        : 
************************************************************************************/
IMG_INTERNAL IMG_VOID DestroyHashedPDSFragSA(GLES1Context *gc, IMG_UINT32 ui32Item)
{
	PDSFragmentSAInfo *psPDSFragmentSAInfo;

	PVR_UNREFERENCED_PARAMETER(gc);

	psPDSFragmentSAInfo = (PDSFragmentSAInfo *)ui32Item;

	if(psPDSFragmentSAInfo->psCodeBlock)
	{
		UCH_CodeHeapFree(psPDSFragmentSAInfo->psCodeBlock);
	}

	GLES1Free(gc, psPDSFragmentSAInfo);
}


/***********************************************************************************
 Function Name      : WritePDSPixelSAProgram
 Inputs             : gc
 Outputs            : pbChanged
 Returns            : Error
 Description        : 
************************************************************************************/
static GLES1_MEMERROR WriteStaticPDSPixelSAProgram(GLES1Context *gc, IMG_BOOL *pbChanged)
{
	FFGEN_PROGRAM_DETAILS *psHWCode;
	GLES1Shader *psShader;

	psShader = gc->sProgram.psCurrentFragmentShader;

	psHWCode = psShader->psFFGENProgramDetails;

	/*
		Fill out the DMA control for the PDS.
	*/
	if ( gc->sPrim.sRenderState.ui32AlphaTestFlags ||
		((gc->ui32RasterEnables & GLES1_RS_FOG_ENABLE) != 0) ||
		 psHWCode->ui32SecondaryAttributeCount
		 )
	{
		PDSFragmentSAInfo *psCachedPDSFragmentSAInfo;
		HashValue tPDSFragmentSAInfoHashValue;
		PDSFragmentSAInfoHashData sHashData;

		sHashData.ui32AttribCount = 0;
		sHashData.ui32SAOffset    = 0;

		/* Alpha test*/
		if(gc->sPrim.sRenderState.ui32AlphaTestFlags)
		{
			sHashData.aui32AttribData[sHashData.ui32AttribCount++] = gc->sPrim.sRenderState.ui32ISPFeedback0;
			sHashData.aui32AttribData[sHashData.ui32AttribCount++] = gc->sPrim.sRenderState.ui32ISPFeedback1;

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
			if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK)
			{
				sHashData.ui32SAOffset = GLES1_FRAGMENT_SECATTR_ALPHATEST_OFFSET;
			}
#endif
		}
	
		/* Fog colour */
		if(gc->ui32RasterEnables & GLES1_RS_FOG_ENABLE)
		{
			sHashData.aui32AttribData[sHashData.ui32AttribCount++] = gc->sState.sFog.ui32Color;
		}


		/* For those float constants moved to secondary attribute area */
		if(psHWCode->ui32SecondaryAttributeCount)
		{
			GLES1MemCopy(&sHashData.aui32AttribData[sHashData.ui32AttribCount], 
						 psShader->pfConstantData, 
						 psHWCode->ui32SecondaryAttributeCount*sizeof(IMG_FLOAT)); 

			sHashData.ui32AttribCount += psHWCode->ui32SecondaryAttributeCount;
		}

		GLES1_ASSERT(sHashData.ui32AttribCount <= MAX_PIXEL_SA_COUNT);


		/* Save the secondary attribute size */
		psShader->ui32USESecAttribDataSizeInDwords = sHashData.ui32AttribCount;

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
		if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK)
		{
			psShader->ui32USESecAttribDataSizeInDwords += GLES1_FRAGMENT_SECATTR_ALPHATEST_OFFSET;
		}
#endif

		/* Initialise the hash value */
		tPDSFragmentSAInfoHashValue = STATEHASH_INIT_VALUE;

		/* Hash the program data  */
		tPDSFragmentSAInfoHashValue = HashFunc((IMG_UINT32*)&sHashData, sHashData.ui32AttribCount+2, tPDSFragmentSAInfoHashValue);

		/* Search hash table to see if we have already stored this program */
		if(HashTableSearch(  gc, 
							&gc->sProgram.sPDSFragmentSAHashTable, tPDSFragmentSAInfoHashValue, 
							(IMG_UINT32*)&sHashData, sHashData.ui32AttribCount+2, 
							(IMG_UINT32 *)&psCachedPDSFragmentSAInfo))
		{
			/*
			**
			**  There's already the right program in the hash table, so use it
			**
			*/
			IMG_UINT32 ui32NewDMSInfo, ui32SecAttribAllocation;
			GLES1PDSInfo *psPDSInfo;

			psPDSInfo = &gc->sProgram.psCurrentFragmentVariant->u.sFragment.sPDSInfo;

			ui32NewDMSInfo = psPDSInfo->ui32DMSInfo & EURASIA_PDS_SECATTRSIZE_CLRMSK & (~EURASIA_PDS_USE_SEC_EXEC);

			ui32SecAttribAllocation = ALIGNCOUNTINBLOCKS(psShader->ui32USESecAttribDataSizeInDwords, EURASIA_PDS_SECATTRSIZE_ALIGNSHIFTINREGISTERS);

			ui32NewDMSInfo |= (ui32SecAttribAllocation << EURASIA_PDS_SECATTRSIZE_SHIFT) | EURASIA_PDS_USE_SEC_EXEC;

			if((gc->sPrim.uFragmentPDSSecAttribBaseAddress.uiAddr != psCachedPDSFragmentSAInfo->uPDSUSEShaderSABaseAddr.uiAddr) ||
			   (gc->sPrim.ui32FragmentPDSSecAttribDataSize != psCachedPDSFragmentSAInfo->ui32FragmentPDSSecAttribDataSize) ||
			   (psPDSInfo->ui32DMSInfo != ui32NewDMSInfo))
			{
				/*
				**
				**  Set this program to be the current one
				**
				*/
				gc->sPrim.uFragmentPDSSecAttribBaseAddress.uiAddr = psCachedPDSFragmentSAInfo->uPDSUSEShaderSABaseAddr.uiAddr;
				gc->sPrim.ui32FragmentPDSSecAttribDataSize = psCachedPDSFragmentSAInfo->ui32FragmentPDSSecAttribDataSize;

				/*
					Set up the secondary PDS program fields in the pixel shader
					data master information word.
				*/
				gc->sProgram.psCurrentFragmentVariant->u.sFragment.sPDSInfo.ui32DMSInfo = ui32NewDMSInfo;

				*pbChanged = IMG_TRUE;
			}
			else
			{
				/*
				**
				**  We're already using the right program
				**
				*/
				*pbChanged = IMG_FALSE;
			}

			return GLES1_NO_ERROR;
		}
		else
		{
			/*
			**
			**  This program isn't cached so create a new one
			**
			*/
		    PDS_PIXEL_SHADER_STATIC_SA_PROGRAM sProgram = {0};
			IMG_UINT32 *pui32Buffer, *pui32BufferBase;
			PDSFragmentSAInfo *psNewPDSFragmentSAInfo;
			IMG_DEV_VIRTADDR uPDSUSEShaderSABaseAddr;
			IMG_UINT32 ui32PDSProgramSizeInBytes;
			IMG_UINT32 ui32SecAttribAllocation;
			GLES1PDSInfo *psPDSInfo;

			IMG_UINT32 ui32NumTemps = gc->sProgram.psCurrentFragmentVariant->ui32MaxTempRegs;


			/* Align to temp allocation granularity */
			ui32NumTemps = ALIGNCOUNTINBLOCKS(ui32NumTemps, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);

			
			/* psPDSInfo = &gc->sProgram.psCurrentFragmentVariant->u.sFragment.sPDSInfo; */

			psNewPDSFragmentSAInfo = GLES1Malloc(gc, sizeof(PDSFragmentSAInfo));

			if(!psNewPDSFragmentSAInfo)
			{
				PVR_DPF((PVR_DBG_ERROR,"WritePDSPixelSAProgram: malloc() failed"));

				*pbChanged = IMG_FALSE;

				return GLES1_HOST_MEM_ERROR;
			}


#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
#if defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728)
			if((gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_BRNFIX_DEPTHF) == 0)
#endif /* defined(FIX_HW_BRN_29546) || defined(FIX_HW_BRN_31728) */
			if(gc->sPrim.sRenderState.ui32AlphaTestFlags & GLES1_ALPHA_TEST_FEEDBACK)
			{
				sProgram.bIterateZAbs = IMG_TRUE;
			}
#endif
			sProgram.ui32DAWCount  = sHashData.ui32AttribCount;
			sProgram.ui32DAWOffset = sHashData.ui32SAOffset;
			sProgram.pui32DAWData  = sHashData.aui32AttribData;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			sProgram.aui32USETaskControl[0]	= ui32NumTemps << EURASIA_PDS_DOUTU0_TRC_SHIFT;
			sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
			sProgram.aui32USETaskControl[2]	= 0;
#else 
			sProgram.aui32USETaskControl[0]	= 0;
			sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL | (ui32NumTemps << EURASIA_PDS_DOUTU1_TRC_SHIFT);
			sProgram.aui32USETaskControl[2]	= 0;
#endif /* defined(SGX_FEATURE_USE_UNLIMTED_PHASES) */

#if defined(SGX_FEATURE_ALPHATEST_SECONDARY_PERPRIMITIVE)
			if(sProgram.bIterateZAbs)
			{
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
				sProgram.aui32USETaskControl[1]	|= EURASIA_PDS_DOUTU1_ITERATORSDEPENDENCY;
#else 
				sProgram.aui32USETaskControl[0]	|= EURASIA_PDS_DOUTU0_ITERATORSDEPENDENCY;
#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */
			}
#endif

			SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
									0, 
									gc->sProgram.psDummyFragUSECode->sDevVAddr, 
									gc->psSysContext->uUSEFragmentHeapBase, 
									SGX_PIXSHADER_USE_CODE_BASE_INDEX);

			/* Kick a USE dummy program (all secondary tasks with DMA must have a USSE program executed on them) */
			sProgram.bKickUSEDummyProgram = IMG_TRUE;



			pui32BufferBase = gc->sProgram.uTempBuffer.aui32StaticPixelShaderSAProg;

			/*
				Generate the PDS pixel state program
			*/
			pui32Buffer = (IMG_UINT32 *)PDSGenerateStaticPixelShaderSAProgram(&sProgram, pui32BufferBase);

			ui32PDSProgramSizeInBytes = ((IMG_UINT32)(pui32Buffer - pui32BufferBase)) << 2;

			GLES1_ASSERT(ui32PDSProgramSizeInBytes <= sizeof(gc->sProgram.uTempBuffer.aui32StaticPixelShaderSAProg));

			if (ValidateHashTableInsert(gc, &gc->sProgram.sPDSFragmentSAHashTable, tPDSFragmentSAInfoHashValue))
			{
				GLES1_TIME_START(GLES1_TIMER_PDSCODEHEAP_FRAG_TIME);
	
				psNewPDSFragmentSAInfo->psCodeBlock = UCH_CodeHeapAllocate(gc->psSharedState->psPDSFragmentCodeHeap, ui32PDSProgramSizeInBytes, gc->psSysContext->hPerProcRef);

				GLES1_TIME_STOP(GLES1_TIMER_PDSCODEHEAP_FRAG_TIME);
				GLES1_INC_COUNT(GLES1_TIMER_PDSCODEHEAP_FRAG_COUNT, ui32PDSProgramSizeInBytes);
			}
			else
			{
				psNewPDSFragmentSAInfo->psCodeBlock = IMG_NULL;
			}
		
			if(psNewPDSFragmentSAInfo->psCodeBlock)
			{
				pui32BufferBase = psNewPDSFragmentSAInfo->psCodeBlock->pui32LinAddress;

				uPDSUSEShaderSABaseAddr = psNewPDSFragmentSAInfo->psCodeBlock->sCodeAddress;

				GLES1_ASSERT(uPDSUSEShaderSABaseAddr.uiAddr >= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr);
#if defined(PDUMP)
				psNewPDSFragmentSAInfo->psCodeBlock->bDumped = IMG_FALSE;
#endif
			}
			else
			{
				pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, ui32PDSProgramSizeInBytes>>2, CBUF_TYPE_PDS_FRAG_BUFFER, IMG_FALSE);
				
				if(!pui32BufferBase)
				{
					GLES1Free(gc, psNewPDSFragmentSAInfo);

					return GLES1_3D_BUFFER_ERROR;
				}
				
				uPDSUSEShaderSABaseAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_PDS_FRAG_BUFFER);
			}

			GLES1_ASSERT((((IMG_UINT32)pui32BufferBase) & ((1UL << EURASIA_PDS_BASEADD_ALIGNSHIFT) - 1)) == 0);

			GLES1MemCopy(pui32BufferBase, gc->sProgram.uTempBuffer.aui32StaticPixelShaderSAProg, ui32PDSProgramSizeInBytes);


			psPDSInfo = &gc->sProgram.psCurrentFragmentVariant->u.sFragment.sPDSInfo;

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

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
			/* Adjust PDS execution address for restricted address range */
			gc->sPrim.uFragmentPDSSecAttribBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif
			gc->sPrim.ui32FragmentPDSSecAttribDataSize = sProgram.ui32DataSize;

			if(psNewPDSFragmentSAInfo->psCodeBlock)
			{
				PDSFragmentSAInfoHashData *psHashData;

				psNewPDSFragmentSAInfo->uPDSUSEShaderSABaseAddr.uiAddr   = gc->sPrim.uFragmentPDSSecAttribBaseAddress.uiAddr;
				psNewPDSFragmentSAInfo->ui32FragmentPDSSecAttribDataSize = gc->sPrim.ui32FragmentPDSSecAttribDataSize;

				psHashData = GLES1Malloc(gc, (sHashData.ui32AttribCount+2)*sizeof(IMG_UINT32));

				if(!psHashData)
				{
					PVR_DPF((PVR_DBG_ERROR,"WritePDSPixelSAProgram: malloc() failed"));

					UCH_CodeHeapFree(psNewPDSFragmentSAInfo->psCodeBlock);

					GLES1Free(gc, psNewPDSFragmentSAInfo);

					*pbChanged = IMG_FALSE;

					return GLES1_HOST_MEM_ERROR;
				}

				GLES1MemCopy(psHashData, &sHashData, (sHashData.ui32AttribCount+2)*sizeof(IMG_UINT32));

				HashTableInsert(gc, 
								&gc->sProgram.sPDSFragmentSAHashTable, tPDSFragmentSAInfoHashValue, 
								(IMG_UINT32*)psHashData, psHashData->ui32AttribCount+2, 
								(IMG_UINT32)psNewPDSFragmentSAInfo);
			}
			else
			{
				/* 
				   Update buffer position 
				*/
				CBUF_UpdateBufferPos(gc->apsBuffers, ui32PDSProgramSizeInBytes>>2, CBUF_TYPE_PDS_FRAG_BUFFER);

				GLES1_INC_COUNT(GLES1_TIMER_PDS_FRAG_DATA_COUNT, ui32PDSProgramSizeInBytes>>2);

				GLES1Free(gc, psNewPDSFragmentSAInfo);
			}

			*pbChanged = IMG_TRUE;
		}
	}
	else
	{
		/*
		**
		**  Use a dummy program as there are no SAs
		**
		*/
		IMG_DEV_VIRTADDR uBaseAddr;
		GLES1PDSInfo *psPDSInfo;
		
		psPDSInfo = &gc->sProgram.psCurrentFragmentVariant->u.sFragment.sPDSInfo;

		/* Save the secondary attribute size */
		psShader->ui32USESecAttribDataSizeInDwords = 0;

		uBaseAddr = gc->sPrim.psDummyPixelSecondaryPDSCode->sCodeAddress;

#if !defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
		/* Adjust PDS execution address for restricted address range */
		uBaseAddr.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif


		/*
			Set up the secondary PDS program fields in the pixel shader
			data master information word.
		*/
		psPDSInfo->ui32DMSInfo &= EURASIA_PDS_SECATTRSIZE_CLRMSK & (~EURASIA_PDS_USE_SEC_EXEC);

		if( (gc->sPrim.uFragmentPDSSecAttribBaseAddress.uiAddr!=uBaseAddr.uiAddr) ||
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
	}

	return GLES1_NO_ERROR;
}


/*****************************************************************************
 Function Name	: WriteMTEState
 Inputs			: 
 Outputs		: none
 Returns		: Mem Error
 Description	: 
*****************************************************************************/
static GLES1_MEMERROR WriteMTEState(GLES1Context *gc, IMG_UINT32 *pui32DWordsWritten,
							    IMG_DEV_VIRTADDR *psStateAddr)
{
	IMG_UINT32	ui32USEFormatHeader = 0, *pui32Buffer, *pui32BufferBase;
	IMG_UINT32  ui32StateDWords = GLES1_MAX_MTE_STATE_DWORDS;
	IMG_FLOAT fWClamp = 0.00001f;
	GLESCompiledRenderState *psRenderState = &gc->sPrim.sRenderState;
	GLES1ShaderVariant *psFragmentVariant = gc->sProgram.psCurrentFragmentVariant;
	GLES1PDSInfo *psPDSInfo = &psFragmentVariant->u.sFragment.sPDSInfo;

	/*
		Get buffer space for the actual output state
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers,
									ui32StateDWords,
									CBUF_TYPE_PDS_VERT_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}
	
	/*
		Record the buffer base (for the header) and advance pointer past the header
	*/
	pui32Buffer = pui32BufferBase;

	pui32Buffer++;

	/* ========= */
	/* ISP state words */
	/* ========= */
	if(gc->ui32EmitMask & GLES1_EMITSTATE_MTE_STATE_ISP)
	{
		IMG_UINT32 ui32ISPAPrimitiveType;
		IMG_UINT32 ui32ISPWordA = psRenderState->ui32ISPControlWordA;

		if((gc->psDrawParams->eRotationAngle==PVRSRV_FLIP_Y) && (gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_SPRITE))
		{
			/* Point sprites need to be flipped for pbuffers */
			ui32ISPAPrimitiveType = EURASIA_ISPA_OBJTYPE_SPRITE10UV;
		}
		else
		{
			{
			    ui32ISPAPrimitiveType = aui32GLESPrimToISPPrim[gc->sPrim.eCurrentPrimitiveType];
			}
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
	}

	/* ========= */
	/* PDS state */
	/* ========= */
	if(gc->ui32EmitMask & (GLES1_EMITSTATE_PDS_FRAGMENT_STATE | GLES1_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE))
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
	if(gc->ui32EmitMask & GLES1_EMITSTATE_MTE_STATE_REGION_CLIP)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_RGNCLIP;

		*pui32Buffer++	= gc->psRenderSurface->aui32RegionClipWord[0];
		*pui32Buffer++	= gc->psRenderSurface->aui32RegionClipWord[1];
	}

	/* ========= */
	/* View Port */
	/* ========= */
	if(gc->ui32EmitMask & GLES1_EMITSTATE_MTE_STATE_VIEWPORT)
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
	if(gc->ui32EmitMask & GLES1_EMITSTATE_MTE_STATE_TAMISC)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_WRAP;

		*pui32Buffer++	= 0;
	}


	/* ========= */
	/* Output Selects */
	/* ========= */
	if (gc->ui32EmitMask & GLES1_EMITSTATE_MTE_STATE_OUTPUTSELECTS)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_OUTSELECTS;
	
		*pui32Buffer++	= gc->sProgram.psCurrentVertexShader->ui32OutputSelects;
	}

	/* ========= */
	/* W Clamping */
	/* ========= */
	if(gc->ui32EmitMask & GLES1_EMITSTATE_MTE_STATE_TAMISC)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_WCLAMP;

		*pui32Buffer++ = FLOAT_TO_UINT32(fWClamp);
	}

#if defined(SGX545)
	//	    ui32USEFormatHeader |= (gc->sProgram.psCurrentVertexShader->ui32NumTexWords - 1) << EURASIA_TACTLPRES_NUMTEXWORDS_SHIFT;
#endif /* defined(SGX545) */


	/* ========= */
	/* MTE control */
	/* ========= */
	if(gc->ui32EmitMask & GLES1_EMITSTATE_MTE_STATE_CONTROL)
	{
		IMG_UINT32 ui32MTEControl = psRenderState->ui32MTEControl;

		ui32USEFormatHeader |= EURASIA_TACTLPRES_MTECTRL;

		/* No transform on DrawTexture objects */
		if(gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_DRAWTEXTURE)
		{
			ui32MTEControl |= EURASIA_MTE_PRETRANSFORM;
		}

		*pui32Buffer++	= ui32MTEControl;
	}


	/* ========= */
	/* Texture coordinate size */
	/* ========= */
	if (gc->ui32EmitMask & GLES1_EMITSTATE_MTE_STATE_OUTPUTSELECTS)
	{
		ui32USEFormatHeader |= EURASIA_TACTLPRES_TEXSIZE | EURASIA_TACTLPRES_TEXFLOAT;

		*pui32Buffer++	= gc->sProgram.psCurrentVertexShader->ui32TexCoordSelects;

		/* Controls 16/32bit texture coordinate format */ 
		*pui32Buffer++  = gc->sProgram.psCurrentVertexShader->ui32TexCoordPrecision;
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

	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, ui32StateDWords);

	GLES1_INC_COUNT(GLES1_TIMER_MTE_STATE_COUNT, ui32StateDWords);

	/* Write the format header */
	*pui32BufferBase = ui32USEFormatHeader;

	/*
		Get the address of the state data in video memory.
	*/
	*psStateAddr = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, CBUF_TYPE_PDS_VERT_BUFFER);

	*pui32DWordsWritten = ui32StateDWords;

	return GLES1_NO_ERROR; 
}


/*****************************************************************************
 Function Name	: PatchPregenMTEStateCopyPrograms
 Inputs			: gc		- pointer to the context
 Outputs		: none
 Returns		: Mem Error
 Description	: patch the pregenerated PDS state program
*****************************************************************************/
IMG_INTERNAL GLES1_MEMERROR PatchPregenMTEStateCopyPrograms(GLES1Context	*gc,
															IMG_UINT32 ui32StateSizeInDWords,
															IMG_DEV_VIRTADDR	uStateDataHWAddress)
{
    IMG_UINT32 aui32DMAControl[PDS_NUM_DMA_KICKS * PDS_NUM_DMA_CONTROL_WORDS] = {0};
    IMG_UINT32 aui32USETaskControl[PDS_NUM_USE_TASK_CONTROL_WORDS];
	IMG_DEV_VIRTADDR uUSEExecutionHWAddress;
	IMG_UINT32 *pui32BufferBase;

	uUSEExecutionHWAddress = GetStateCopyUSEAddress(gc, ui32StateSizeInDWords);

	/*
		Setup the parameters for the PDS program.
	*/
	EncodeDmaBurst(aui32DMAControl, 0, ui32StateSizeInDWords, uStateDataHWAddress);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	aui32USETaskControl[0] = (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU0_TRC_SHIFT);
	aui32USETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	aui32USETaskControl[2] = 0;
#else /* defined(SGX545) */
	aui32USETaskControl[0] = EURASIA_PDS_DOUTU0_PDSDMADEPENDENCY;
	aui32USETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL | (SGX_USE_MINTEMPREGS << EURASIA_PDS_DOUTU1_TRC_SHIFT);
	aui32USETaskControl[2] = 0;
#endif /* defined(SGX545) */

	SetUSEExecutionAddress(&aui32USETaskControl[0], 
							0, 
							uUSEExecutionHWAddress, 
							gc->psSysContext->uUSEVertexHeapBase, 
							SGX_VTXSHADER_USE_CODE_BASE_INDEX);

	/*
		Get buffer space for the PDS state copy program
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, GLES1_ALIGNED_MTE_COPY_PROG_SIZE, CBUF_TYPE_MTE_COPY_PREGEN_BUFFER, IMG_FALSE);
	
	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	/*
		patch the PDS state copy program
	*/
	PDSMTEStateCopySetDMA0(pui32BufferBase, aui32DMAControl[0]);
	PDSMTEStateCopySetDMA1(pui32BufferBase, aui32DMAControl[1]);
	PDSMTEStateCopySetDMA2(pui32BufferBase, aui32DMAControl[2]);
	PDSMTEStateCopySetDMA3(pui32BufferBase, aui32DMAControl[3]);
	
	PDSMTEStateCopySetUSE0(pui32BufferBase, aui32USETaskControl[0]);
	PDSMTEStateCopySetUSE1(pui32BufferBase, aui32USETaskControl[1]);

	/*
		As long as USE2 is 0 we don't need to patch it,
		the template program has already it set to 0

		PDSMTEStateCopySetUSE2(pui32BufferBase, sProgram.aui32USETaskControl[2]);
	*/
	
	/*
	   Update buffer position for the PDS pixel program.
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, GLES1_ALIGNED_MTE_COPY_PROG_SIZE, CBUF_TYPE_MTE_COPY_PREGEN_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_MTE_COPY_COUNT, 6);

	/*
		Save the PDS pixel shader program physical base address and data size
	*/
	gc->sPrim.uOutputStatePDSBaseAddress = CBUF_GetBufferDeviceAddress(gc->apsBuffers, pui32BufferBase, 
																  CBUF_TYPE_MTE_COPY_PREGEN_BUFFER);

	gc->sPrim.ui32OutputStatePDSDataSize    = PDS_MTESTATECOPY_DATA_SEGMENT_SIZE;
	gc->sPrim.ui32OutputStateUSEAttribSize  = ui32StateSizeInDWords;

#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
	/* Adjust PDS execution address for restricted address range */
	gc->sPrim.uOutputStatePDSBaseAddress.uiAddr -= gc->psSysContext->sHWInfo.uPDSExecBase.uiAddr;
#endif
#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	gc->sPrim.ui32OutputStateUSEAttribSize += SGX_USE_MINTEMPREGS;
#endif

	return GLES1_NO_ERROR;
}


/*****************************************************************************
 Function Name	: WriteMTEStateCopyPrograms
 Inputs			: gc		- pointer to the context
 Outputs		: none
 Returns		: Mem Error
 Description	: Writes the PDS state program
*****************************************************************************/
IMG_INTERNAL GLES1_MEMERROR WriteMTEStateCopyPrograms(GLES1Context	*gc,
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
#endif

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
		return GLES1_TA_BUFFER_ERROR;
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

	GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_DATA_COUNT, (pui32Buffer - pui32BufferBase));


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

	return GLES1_NO_ERROR;
}

#if defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690)
#define NUM_SECATTR_STATE_BLOCKS			SGX_FEATURE_NUM_USE_PIPES
#else
#define NUM_SECATTR_STATE_BLOCKS			1
#endif /* defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690) */

/*****************************************************************************
 Function Name	: SetupStateUpdate
 Inputs			: bMTEStateUpdate
 Outputs		: none
 Returns		: none
 Description	: Writes the state update block (either MTE update or vertex
				  secondary update) to the VDM control stream/
*****************************************************************************/
IMG_INTERNAL GLES1_MEMERROR SetupStateUpdate(GLES1Context *gc, IMG_BOOL bMTEStateUpdate)
{
	IMG_UINT32 *pui32BufferBase, *pui32Buffer;
	IMG_UINT32 ui32PDSDwordStateUpdate = MAX_DWORDS_PER_PDS_STATE_BLOCK * NUM_SECATTR_STATE_BLOCKS;

	/*
		Get VDM control stream space for the PDS vertex state block
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, ui32PDSDwordStateUpdate, CBUF_TYPE_VDM_CTRL_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	pui32Buffer = pui32BufferBase;

	if(bMTEStateUpdate)
	{
		/*
				Write the MTE state update PDS data block
		*/
		*pui32Buffer++	=   (EURASIA_TAOBJTYPE_STATE                      |
				    (((gc->sPrim.uOutputStatePDSBaseAddress.uiAddr >> EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) 
				    << EURASIA_TAPDSSTATE_BASEADDR_SHIFT) & ~EURASIA_TAPDSSTATE_BASEADDR_CLRMSK));
	
		*pui32Buffer++	=   ((((gc->sPrim.ui32OutputStatePDSDataSize >> EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) 
							   << EURASIA_TAPDSSTATE_DATASIZE_SHIFT) & ~EURASIA_TAPDSSTATE_DATASIZE_CLRMSK) |
#if defined(PDUMP)
							 EURASIA_TAPDSSTATE_USEPIPE_1 |
#else
							 (EURASIA_TAPDSSTATE_USEPIPE_1 + gc->sPrim.ui32CurrentOutputStateBlockUSEPipe) |
#endif
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
		GLES1Shader *psVertexShader = gc->sProgram.psCurrentVertexShader;
		IMG_UINT32 ui32StateWord0, ui32StateWord1;
		IMG_UINT32 uSecAttrDataSize = psVertexShader->ui32USESecAttribDataSizeInDwords;
		

		/*
			Write the vertex secondary state update PDS data block
		*/
		ui32StateWord0	=   EURASIA_TAOBJTYPE_STATE |
				    (((gc->sPrim.uVertexPDSSecAttribBaseAddress.uiAddr >> EURASIA_TAPDSSTATE_BASEADDR_ALIGNSHIFT) 
				    << EURASIA_TAPDSSTATE_BASEADDR_SHIFT) & ~EURASIA_TAPDSSTATE_BASEADDR_CLRMSK);
		
		ui32StateWord1	=   (((gc->sPrim.ui32VertexPDSSecAttribDataSize >> EURASIA_TAPDSSTATE_DATASIZE_ALIGNSHIFT) 
				    << EURASIA_TAPDSSTATE_DATASIZE_SHIFT) & ~EURASIA_TAPDSSTATE_DATASIZE_CLRMSK) |
#if !defined(SGX545)
				    EURASIA_TAPDSSTATE_SEC_EXEC | 
#endif
				    EURASIA_TAPDSSTATE_SECONDARY |
				    (ALIGNCOUNTINBLOCKS(uSecAttrDataSize, EURASIA_TAPDSSTATE_USEATTRIBUTESIZE_ALIGNSHIFTINREGISTERS) 
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

	GLES1_INC_COUNT(GLES1_TIMER_VDM_CTRL_STATE_COUNT, (pui32Buffer - pui32BufferBase));

	return GLES1_NO_ERROR;
}


/***********************************************************************************
 Function Name      : GenerateVertexCompileMask
 Inputs             : gc
 Outputs            : -
 Returns            : Have needs changed
 Description        : Generates a flag of vertex attributes required by current state.
************************************************************************************/
static IMG_BOOL GenerateVertexCompileMask(GLES1Context *gc)
{
	IMG_UINT32 ui32Loop, ui32CompileMask;

	GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);




	ui32CompileMask = VARRAY_VERT_ENABLE;

	if (gc->ui32TnLEnables & GLES1_TL_LIGHTING_ENABLE)
	{
		if(gc->ui32TnLEnables & GLES1_TL_LIGHTS_ENABLE)
		{
			ui32CompileMask |= VARRAY_NORMAL_ENABLE;
		}
		
		if(gc->ui32TnLEnables & GLES1_TL_COLORMAT_ENABLE)
		{
			ui32CompileMask |= VARRAY_COLOR_ENABLE;
		}
	}
	else
	{
		ui32CompileMask |= VARRAY_COLOR_ENABLE;
	}
	
	for(ui32Loop = 0; ui32Loop < gc->ui32NumImageUnitsActive; ui32Loop++)
	{
		/* Need normals for texgen */
		if (gc->ui32RasterEnables & (1UL << (GLES1_RS_GENTEXTURE0 + gc->ui32TexImageUnitsEnabled[ui32Loop])))
		{
			ui32CompileMask |= VARRAY_NORMAL_ENABLE;
		}
		else if(!((gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_SPRITE) && 
				 	gc->sState.sTexture.asUnit[ui32Loop].sEnv.bPointSpriteReplace))
		{
			ui32CompileMask |= (VARRAY_TEXCOORD0_ENABLE << gc->ui32TexImageUnitsEnabled[ui32Loop]);
		}
	}

	if(gc->ui32TnLEnables & GLES1_TL_MATRIXPALETTE_ENABLE)
	{
		ui32CompileMask |= (VARRAY_MATRIXINDEX_ENABLE | VARRAY_WEIGHTARRAY_ENABLE);
	}

	if(((psVAOMachine->ui32ArrayEnables & VARRAY_POINTSIZE_ENABLE) != 0) &&
		((gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_SPRITE)||(gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_POINT)))
	{
		ui32CompileMask |= VARRAY_POINTSIZE_ENABLE;
	}

	if(psVAOMachine->ui32CompileMask != ui32CompileMask)
	{
		psVAOMachine->ui32CompileMask = ui32CompileMask;

		return IMG_TRUE;
	}

	return IMG_FALSE;

}


/***********************************************************************************
 Function Name      : SetupMTEPregenBuffer
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : fill MTE pregen circular buffer with PDS program template 
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR SetupMTEPregenBuffer(GLES1Context *gc)
{
	IMG_UINT32 ui32Count, ui32AlignedMTECopyProgramSizeInBytes;
	IMG_UINT32 *pui32BufferBase;
	IMG_UINT32 ui32BufferSize;	
	
	
	ui32BufferSize = gc->apsBuffers[CBUF_TYPE_MTE_COPY_PREGEN_BUFFER]->ui32BufferLimitInBytes;

	ui32AlignedMTECopyProgramSizeInBytes = (GLES1_ALIGNED_MTE_COPY_PROG_SIZE << 2);

	/* 
		fill buffer with template MTE program
	*/
	pui32BufferBase = gc->apsBuffers[CBUF_TYPE_MTE_COPY_PREGEN_BUFFER]->pui32BufferBase;	

	for(ui32Count = 0; ui32Count < ui32BufferSize; ui32Count += ui32AlignedMTECopyProgramSizeInBytes)
	{
		GLES1MemCopy(pui32BufferBase + (ui32Count >> 2), g_pui32PDSMTEStateCopy, PDS_MTESTATECOPY_SIZE);		
	}

	return GLES1_NO_ERROR;
}

/***********************************************************************************
 Function Name      : SetupPDSVertexPregenBuffer
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : fill pregen circular buffer with PDS auxiliary program template 
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR SetupPDSVertexPregenBuffer(GLES1Context *gc)
{
	IMG_UINT32 ui32Count, ui32AlignedPDSAuxialiarySizeInBytes;
	IMG_UINT32 *pui32BufferBase;
	IMG_UINT32 ui32BufferSize;
	IMG_UINT32 aui32PatchableAuxProgram[ALIGNED_PDS_AUXILIARY_PROG_SIZE] = {0};
	PDS_PIXEL_SHADER_SA_PROGRAM	sProgram = {0};	/* PRQA S 3197 */ /* This initialization is necessary. */

	/* 
		if this assert fails you have to update
		PatchAuxiliaryPDSVertexShaderProgram(), using the proper 
		amount of SetSTREAM functionsq
	*/
	GLES1_ASSERT(GLES1_STATIC_PDS_NUMBER_OF_STREAMS == 8);
 
	/* 
		we need the aux programs to run with a dummy USE programs 
	*/
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sProgram.aui32USETaskControl[0]	= 0 << EURASIA_PDS_DOUTU0_TRC_SHIFT;
	sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL;
	sProgram.aui32USETaskControl[2]	= 0;	
#else
	sProgram.aui32USETaskControl[0]	= 0;
	sProgram.aui32USETaskControl[1]	= EURASIA_PDS_DOUTU1_MODE_PARALLEL | (0 << EURASIA_PDS_DOUTU1_TRC_SHIFT);
	sProgram.aui32USETaskControl[2]	= 0;
#endif

	SetUSEExecutionAddress(&sProgram.aui32USETaskControl[0], 
							0, 
							gc->sProgram.psDummyVertUSECode->sDevVAddr, 
							gc->psSysContext->uUSEVertexHeapBase, 
							SGX_VTXSHADER_USE_CODE_BASE_INDEX);
	
	GLES1MemCopy(aui32PatchableAuxProgram, g_pui32PDSAuxiliaryVertex, PDS_AUXILIARYVERTEX_SIZE);

	PDSAuxiliaryVertexSetUSE0(aui32PatchableAuxProgram, sProgram.aui32USETaskControl[0]);
	PDSAuxiliaryVertexSetUSE1(aui32PatchableAuxProgram, sProgram.aui32USETaskControl[1]);
	PDSAuxiliaryVertexSetUSE2(aui32PatchableAuxProgram, sProgram.aui32USETaskControl[2]);
	

	ui32BufferSize = gc->apsBuffers[CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER]->ui32BufferLimitInBytes;

	ui32AlignedPDSAuxialiarySizeInBytes = (ALIGNED_PDS_AUXILIARY_PROG_SIZE << 2);

	/* 
		fill buffer with template auxiliary program
	*/
	pui32BufferBase = gc->apsBuffers[CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER]->pui32BufferBase;	

	for(ui32Count = 0; ui32Count < ui32BufferSize; ui32Count += ui32AlignedPDSAuxialiarySizeInBytes)
	{
		GLES1MemCopy(pui32BufferBase + (ui32Count >> 2), aui32PatchableAuxProgram, ui32AlignedPDSAuxialiarySizeInBytes);

		GLES1_INC_COUNT(GLES1_TIMER_PDS_VERT_AUXILIARY_COUNT, ALIGNED_PDS_AUXILIARY_PROG_SIZE);
	}

	return GLES1_NO_ERROR;
}



/***********************************************************************************
 Function Name      : SetupVAOAttribPointers
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Sets up VAO's attribute copy pointers based on current state
************************************************************************************/
static IMG_VOID SetupVAOAttribPointers(GLES1Context *gc)
{
 	IMG_UINT32 i;
	GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);

	for(i=0; i<psVAOMachine->ui32NumItemsPerVertex; i++)
	{
	    GLES1AttribArrayPointerMachine *psVAOAPMachine = psVAOMachine->apsPackedAttrib[i];

		if (psVAOAPMachine->bIsCurrentState)
		{
			IMG_UINT32 ui32CurrentVAOAttrib = (IMG_UINT32)(psVAOAPMachine - &(psVAOMachine->asAttribPointer[0]));
			
			psVAOAPMachine->pui8CopyPointer = (IMG_VOID *)&gc->sState.sCurrent.asAttrib[ui32CurrentVAOAttrib];
		}
		else
		{
			psVAOAPMachine->pui8CopyPointer = psVAOAPMachine->psState->pui8Pointer;
		}
	}
}  
		
/* Size of attribs in bytes, by type */
static const IMG_UINT32 aui32AttribSize[GLES1_STREAMTYPE_MAX] =
{
	1,		/* GLES1_STREAMTYPE_BYTE */
	1,		/* GLES1_STREAMTYPE_UBYTE */
	2,		/* GLES1_STREAMTYPE_SHORT */
	2,		/* GLES1_STREAMTYPE_USHORT */
	4,		/* GLES1_STREAMTYPE_FLOAT */
	2,		/* GLES1_STREAMTYPE_HALFFLOAT */
	4		/* GLES1_STREAMTYPE_FIXED */
};


/*******************************************************************************************
 Function Name      : SetupVAOAttribStreams
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Picks appropriate vertex copy functions based on current VAO state
********************************************************************************************/
static IMG_VOID SetupVAOAttribStreams(GLES1Context *gc)
{
	IMG_UINT32 ui32NumItemsPerVertex, ui32CompileMask, i;
	GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);
	GLES1VertexArrayObject *psVAO = psVAOMachine->psActiveVAO;

	GLES1_ASSERT(psVAO);

	ui32CompileMask = psVAOMachine->ui32CompileMask;

	gc->ui32VertexSize	 = 0;
	gc->ui32VertexRCSize = 0;
	gc->ui32VertexAlignSize = 0;

	ui32NumItemsPerVertex = 0;

	psVAOMachine->ui32ControlWord = 0;

	/* Setup VAO's attribute states */
	for(i=0; i<GLES1_MAX_ATTRIBS_ARRAY; i++)
	{
		if(ui32CompileMask & (1UL << i))
		{
			GLES1AttribArrayPointerMachine *psVAOAPMachine = &(psVAOMachine->asAttribPointer[i]);

		    GLES1AttribArrayPointerState *psVAOAPState = &(psVAO->asVAOState[i]);

			/* Use VAO Machine's array enables item, 
			   which has already been set in ValidateState() */
			if(psVAOMachine->ui32ArrayEnables & (1UL << i))
			{
				IMG_UINT32 ui32StreamTypeSize = psVAOAPState->ui32StreamTypeSize;
				IMG_UINT32 ui32UserStride = psVAOAPState->ui32UserStride;

				IMG_UINT32 ui32Size = (ui32StreamTypeSize >> GLES1_STREAMSIZE_SHIFT) *
				                      aui32AttribSize[ui32StreamTypeSize & GLES1_STREAMTYPE_MASK];

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

#if defined(GLES1_EXTENSION_MAP_BUFFER)
					if (psVAOAPState->psBufObj->bMapped)
					{
						/* This error condition will be picked up the draw call */
						psVAOMachine->ui32ControlWord |= ATTRIBARRAY_MAP_BUFOBJ;
					}
#endif /* defined(GLES1_EXTENSION_MAP_BUFFER) */
				}
				else
				{
				    if (VAO_IS_ZERO(gc)) /* Setup the default VAO's client arrays */
					{
						IMG_UINT32 ui32UserSize = (psVAOAPState->ui32StreamTypeSize >> GLES1_STREAMSIZE_SHIFT);
						IMG_UINT32 ui32Type = (psVAOAPState->ui32StreamTypeSize & GLES1_STREAMTYPE_MASK);

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

				psVAOAPMachine->pui8CopyPointer		   = psVAOAPState->pui8Pointer;
				psVAOAPMachine->ui32CopyStreamTypeSize = psVAOAPState->ui32StreamTypeSize;
				psVAOAPMachine->ui32CopyStride		   = psVAOAPMachine->ui32Stride;
				psVAOAPMachine->ui32DstSize			   = psVAOAPMachine->ui32Size;

			}
			else /* Setup VAO's current attribute arrays */
			{
				psVAOMachine->ui32ControlWord |= ATTRIBARRAY_SOURCE_CURRENT;

				psVAOAPMachine->bIsCurrentState = IMG_TRUE;

#if defined(NO_UNALIGNED_ACCESS)
				psVAOAPMachine->pfnCopyData[0]	 = CopyData[3][GLES1_STREAMTYPE_FLOAT];
				psVAOAPMachine->pfnCopyData[1]	 = CopyDataByteAligned[3][GLES1_STREAMTYPE_FLOAT];
				psVAOAPMachine->pfnCopyData[2]	 = CopyDataShortAligned[3][GLES1_STREAMTYPE_FLOAT];
				psVAOAPMachine->pfnCopyData[3]	 = CopyDataByteAligned[3][GLES1_STREAMTYPE_FLOAT];
#else
				psVAOAPMachine->pfnCopyData	 	= CopyData[3][GLES1_STREAMTYPE_FLOAT];
#endif

				psVAOAPMachine->pui8CopyPointer		   = (IMG_VOID *)&gc->sState.sCurrent.asAttrib[i];
				psVAOAPMachine->ui32CopyStreamTypeSize = GLES1_STREAMTYPE_FLOAT | (4 << GLES1_STREAMSIZE_SHIFT);
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
static IMG_VOID SetupRenderState(GLES1Context *gc)
{
	GLESCompiledRenderState *psCompiledRenderState = &gc->sPrim.sRenderState;
	IMG_BOOL bTranslucentBlend = IMG_FALSE;
	IMG_BOOL bTrivialBlend = IMG_TRUE;
	IMG_UINT32 ui32RasterEnables, ui32TnLEnables;
	IMG_UINT32 ui32LineWidth;
	IMG_UINT32 ui32FrontFace = gc->sState.sPolygon.eFrontFaceDirection;	
	IMG_UINT32 ui32AlphaTestFunc = gc->sState.sRaster.ui32AlphaTestFunc;
	IMG_UINT32 ui32AlphaRef		 = gc->sState.sRaster.ui32AlphaTestRef & 0xFF;
	
	/* ISP Control Words */
	IMG_UINT32	ui32ISPControlWordA	= 0;
	IMG_UINT32	ui32ISPControlWordB	= 0;
	IMG_UINT32	ui32ISPControlWordC;

	/* Visibility test state sent back to ISP from USE for pixel discard */ 
	IMG_UINT32 ui32AlphaTestFlags = 0;
	IMG_UINT32	ui32ISPFeedback0 = 0;
	IMG_UINT32	ui32ISPFeedback1 = 0;

	/* MTE control words */
	IMG_UINT32  ui32MTEControl = (EURASIA_MTE_DRAWCLIPPEDEDGES    | 
								  EURASIA_MTE_WCLAMPEN)           ;
#if defined(SGX545)
	if (gc->sState.sShade.ui32ShadeModel == EURASIA_MTE_SHADE_RESERVED)
	{
		ui32MTEControl |= ( (EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE0_SHIFT) |
							(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE1_SHIFT) |
							(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE2_SHIFT) |
							(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE3_SHIFT) |
							(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE4_SHIFT) |
							(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE5_SHIFT) |
							(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE6_SHIFT) |
							(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE7_SHIFT) |
							(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE8_SHIFT) |
							(EURASIA_MTE_INTERPOLATIONMODE_GOURAUD << EURASIA_MTE_INTERPOLATIONMODE9_SHIFT) );
	}
	else
	{
		ui32MTEControl |= gc->sState.sShade.ui32ShadeModel;
	}
#else

#if defined(FIX_HW_BRN_25211)
	if(gc->sAppHints.bUseC10Colours)
	{
		ui32MTEControl |= EURASIA_MTE_SHADE_VERTEX2;
	}
	else
#endif /* defined(FIX_HW_BRN_25211) */
	{
		ui32MTEControl |= gc->sState.sShade.ui32ShadeModel;
	}

#endif /* defined(SGX545) */	


	ui32RasterEnables	= gc->ui32RasterEnables;
	ui32TnLEnables		= gc->ui32TnLEnables;

	/* Line width */
#if defined(SGX545)
	/* convert the value into unsigned 4.4 fixed point format */
	ui32LineWidth = (IMG_UINT32)(*gc->sState.sLine.pfLineWidth * EURASIA_ISPB_PLWIDTH_ONE);
	ui32LineWidth = MIN(ui32LineWidth, EURASIA_ISPB_PLWIDTH_MAX);
	ui32LineWidth = (ui32LineWidth << EURASIA_ISPB_PLWIDTH_SHIFT) & ~EURASIA_ISPB_PLWIDTH_CLRMSK;

	ui32ISPControlWordB |= ui32LineWidth;
#else 
	ui32LineWidth = ( (((((IMG_UINT32)(*gc->sState.sLine.pfLineWidth+0.5f)) - 1) & 0xF) 
					   << EURASIA_ISPA_PLWIDTH_SHIFT) 
					  & ~EURASIA_ISPA_PLWIDTH_CLRMSK );

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
	if(((ui32TnLEnables & GLES1_TL_CULLFACE_ENABLE) != 0) && (gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE))
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
	if(((ui32RasterEnables & GLES1_RS_DEPTHTEST_ENABLE) != 0) && gc->psMode->ui32DepthBits)
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
	if(((ui32RasterEnables & GLES1_RS_POLYOFFSET_ENABLE) != 0) && 
	   (gc->sPrim.eCurrentPrimitiveType>=GLES1_PRIMTYPE_TRIANGLE) &&
	   (gc->sPrim.eCurrentPrimitiveType<=GLES1_PRIMTYPE_TRIANGLE_FAN))
	{
#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
		ui32ISPControlWordB |= EURASIA_ISPB_DBIAS_ENABLE;
#else
		IMG_INT32  i32DepthBiasUnits, i32DepthBiasFactor;

		i32DepthBiasUnits  = (IMG_INT32)gc->sState.sPolygon.fUnits;
		i32DepthBiasFactor = (IMG_INT32)(gc->sState.sPolygon.factor.fVal);

		/* Check that the units and factor are within range */
		i32DepthBiasUnits = Clampi(i32DepthBiasUnits, -16, 15);
		i32DepthBiasFactor = Clampi(i32DepthBiasFactor, -16, 15);
			
		ui32ISPControlWordB |= (((IMG_UINT32)(i32DepthBiasUnits) << EURASIA_ISPB_DBIASUNITS_SHIFT) & ~EURASIA_ISPB_DBIASUNITS_CLRMSK);
		ui32ISPControlWordB |= (((IMG_UINT32)(i32DepthBiasFactor) << EURASIA_ISPB_DBIASFACTOR_SHIFT) & ~EURASIA_ISPB_DBIASFACTOR_CLRMSK);
#endif
	}
	
	psCompiledRenderState->bNeedLogicOpsCode  = IMG_FALSE;
	psCompiledRenderState->bNeedFBBlendCode   = IMG_FALSE;
	psCompiledRenderState->bNeedColorMaskCode = IMG_FALSE;

	/* Logical ops have a higher priority than blending (OpenGL spec) */
	if(ui32RasterEnables & GLES1_RS_LOGICOP_ENABLE)
	{
		if (gc->sState.sRaster.ui32LogicOp==GL_NOOP)
		{
			/* No-op, so don't send to TSP */
			ui32ISPControlWordA |= EURASIA_ISPA_TAGWRITEDIS;
		}
		else
		{
			/* Mark for creation of USE code to perform the logical op */
			psCompiledRenderState->bNeedLogicOpsCode = IMG_TRUE;

			/* Set ISP pass-type to translucent */
			ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
			ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_TRANS;
		}
	}
	else
	{
		/* Framebuffer blending */
		if(ui32RasterEnables & GLES1_RS_ALPHABLEND_ENABLE)
		{
			GetFBBlendType(gc, &bTrivialBlend, &bTranslucentBlend);

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
	}

	/* 
	* Colour masks
	*/
	if(gc->sState.sRaster.ui32ColorMask)
	{
		/* Check to see if all colour channels are enabled  */
		if(gc->sState.sRaster.ui32ColorMask != GLES1_COLORMASK_ALL)
		{
			/* Mark for creation of USE code to perform the color mask.
			   NOTE: this code also copies from Temp0 to Output0 */
			psCompiledRenderState->bNeedColorMaskCode = IMG_TRUE;
		}

		/* Set user pass-spawn control to match the colour mask, this should generate a new ISP pass if 
		   different from previous user pass-spawn control/colour mask. Object does not need to be made
		   translucent, unless it's a partial mask and MSAA is enabled on cores without phase rate control */
		ui32ISPControlWordB |= (gc->sState.sRaster.ui32ColorMask << EURASIA_ISPB_UPASSCTL_SHIFT);

#if !defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		if(gc->psMode->ui32AntiAliasMode && (gc->sState.sRaster.ui32ColorMask != GLES1_COLORMASK_ALL))
		{
			/* Mark object as translucent to ensure the USSE runs at full rate on edges */
			ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
			ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_TRANS;
			
			bTranslucentBlend = IMG_TRUE;
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
	 * Stencil test
	*/
	if (((ui32RasterEnables & GLES1_RS_STENCILTEST_ENABLE) != 0) && gc->psMode->ui32StencilBits)
	{
		ui32ISPControlWordC = gc->sState.sStencil.ui32Stencil;
		ui32ISPControlWordA |= gc->sState.sStencil.ui32StencilRef;
	}
	else
	{
		/* Else stencil always passes - and no action is done on stencil buffer */
		ui32ISPControlWordC = (EURASIA_ISPC_SCMP_ALWAYS |
							 EURASIA_ISPC_SOP1_KEEP |
							 EURASIA_ISPC_SOP2_KEEP |
							 EURASIA_ISPC_SOP3_KEEP);
	}

	/* Alpha test*/
	if (ui32RasterEnables & GLES1_RS_ALPHATEST_ENABLE)
	{
		if((ui32AlphaTestFunc == GL_ALWAYS) ||
		   (ui32AlphaTestFunc == GL_GEQUAL && ui32AlphaRef == 0) ||
		   (ui32AlphaTestFunc == GL_LEQUAL && ui32AlphaRef == 0xFF))
		{
			PVR_DPF((PVR_DBG_MESSAGE,"SetupRenderState: Optimizing out Alpha Test as it always passes"));
		}
		else if((ui32AlphaTestFunc == GL_NEVER) ||
				(ui32AlphaTestFunc == GL_GREATER && ui32AlphaRef == 0xFF) ||
				(ui32AlphaTestFunc == GL_LESS && ui32AlphaRef == 0))
		{
			PVR_DPF((PVR_DBG_MESSAGE,"SetupRenderState: Optimizing out Alpha Test as it never passes"));
			
			ui32ISPControlWordA |= EURASIA_ISPA_TAGWRITEDIS | EURASIA_ISPA_DWRITEDIS;
		}
		else
		{
			/* If depth write is disabled and stencil test is disabled we can optimise alpha test/ISP feedback */
			if(((ui32ISPControlWordA & EURASIA_ISPA_DWRITEDIS) != 0) && 
				(ui32ISPControlWordC == (EURASIA_ISPC_SCMP_ALWAYS |
							 EURASIA_ISPC_SOP1_KEEP |
							 EURASIA_ISPC_SOP2_KEEP |
							 EURASIA_ISPC_SOP3_KEEP)))
			{
				ui32AlphaTestFlags |= GLES1_ALPHA_TEST_ONLY;
			
				/* We need to make this translucent in case alpha test fails and nothing is drawn */
				ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
				ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_TRANS;
			}
			else
			{
				ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
				
				if(bTranslucentBlend)
				{
					/* Set object type to transparent PT */
					ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_TRANSPT;
				}
				else
				{
					/* Alpha test and no translucent blend, so set object type to fast punch through */
					ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_FASTPT;
				}

				/* Make sure tag write disable is not set */
				if(ui32ISPControlWordA & EURASIA_ISPA_TAGWRITEDIS)
				{
					ui32AlphaTestFlags |= GLES1_ALPHA_TEST_WAS_TWD;

					/* Get rid of any other second phase code */
					bTrivialBlend = IMG_TRUE;
					psCompiledRenderState->bNeedLogicOpsCode  = IMG_FALSE;
					psCompiledRenderState->bNeedColorMaskCode = IMG_FALSE;
					
					ui32ISPControlWordA &= ~EURASIA_ISPA_TAGWRITEDIS;
				}

				ui32AlphaTestFlags |= GLES1_ALPHA_TEST_FEEDBACK;

#if defined(FIX_HW_BRN_29546)
				if((gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_SPRITE) ||
				   (gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_POINT))
				{
					ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;

					/* Set object type to depth feedback */
					ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_DEPTHFEEDBACK;

					ui32AlphaTestFlags |= GLES1_ALPHA_TEST_BRNFIX_DEPTHF;
				}
#endif /* defined(FIX_HW_BRN_29546) */
#if defined(FIX_HW_BRN_31728)
				if((gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_LINE)	   ||
				   (gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_LINE_LOOP) ||
				   (gc->sPrim.eCurrentPrimitiveType==GLES1_PRIMTYPE_LINE_STRIP))
				{
					ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;

					/* Set object type to depth feedback */
					ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_DEPTHFEEDBACK;

					ui32AlphaTestFlags |= GLES1_ALPHA_TEST_BRNFIX_DEPTHF;
				}
#endif /* defined(FIX_HW_BRN_31728) */
 			}
		}
	}


	/* Depth write disable AND Tag write disable - make object opaque */
	if ((ui32ISPControlWordA & (EURASIA_ISPA_TAGWRITEDIS | EURASIA_ISPA_DWRITEDIS)) == (EURASIA_ISPA_TAGWRITEDIS | EURASIA_ISPA_DWRITEDIS))
	{
		ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;
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
									((gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE && !gc->bFullScreenScissor) || 
									 !gc->bFullScreenViewport))
		{

			ui32ISPControlWordA &= EURASIA_ISPA_PASSTYPE_CLRMSK;

			ui32AlphaTestFlags = GLES1_ALPHA_TEST_FEEDBACK;
		
			/* Make sure tag write disable is not set */
			if(ui32ISPControlWordA & EURASIA_ISPA_TAGWRITEDIS)
			{
				ui32AlphaTestFlags |= GLES1_ALPHA_TEST_WAS_TWD;

				/* Get rid of any other second phase code */
				bTrivialBlend = IMG_TRUE;
				psCompiledRenderState->bNeedLogicOpsCode  = IMG_FALSE;
				psCompiledRenderState->bNeedColorMaskCode = IMG_FALSE;
				
				ui32ISPControlWordA &= ~EURASIA_ISPA_TAGWRITEDIS;
			}

			if(bTranslucentBlend || ((ui32AlphaTestFlags & GLES1_ALPHA_TEST_WAS_TWD) != 0))
			{
				/* Set object type to transparent PT */
				ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_TRANSPT;
			}
			else
			{
				/* Alpha test and no translucent blend, so set object type to fast punch through */
				ui32ISPControlWordA |= EURASIA_ISPA_PASSTYPE_FASTPT;
			}

			ui32AlphaTestFunc = GL_ALWAYS;
		}
	}
#endif

	/* Only support fill mode. 
	 */
	ui32ISPControlWordA |= EURASIA_ISPA_OBJTYPE_TRI;

	/* Alpha test requires ISP Feedback */
	if(ui32AlphaTestFlags)
	{
		/* Setup ISP visibility test state that gets sent from USE ATST8/DEPTHF instructions */
		
		/* State word 0 */
#if defined(SGX_FEATURE_ALPHATEST_AUTO_COEFF)
		ui32ISPFeedback0 |= (ui32ISPControlWordA & EURASIA_ISPA_DWRITEDIS) ? EURASIA_USE_VISTEST_STATE0_DWDISABLE : 0;

		ui32ISPFeedback0 |= (ui32AlphaRef & 0xFF) << EURASIA_USE_VISTEST_STATE0_AREF_SHIFT;
		ui32ISPFeedback0 |= ((ui32AlphaTestFunc << EURASIA_USE_VISTEST_STATE0_ACMPMODE_SHIFT) & ~EURASIA_USE_VISTEST_STATE0_ACMPMODE_CLRMSK);	

#else
		ui32ISPFeedback0 |= GLES1_EXTRACT(ui32ISPControlWordA, ISPA_SREF) << EURASIA_USE_VISTEST_STATE0_SREF_SHIFT;
		ui32ISPFeedback0 |= GLES1_EXTRACT(ui32ISPControlWordA, ISPA_DCMPMODE) << EURASIA_USE_VISTEST_STATE0_DCMPMODE_SHIFT;

		ui32ISPFeedback0 |= GLES1_EXTRACT(ui32ISPControlWordC, ISPC_SCMP) << EURASIA_USE_VISTEST_STATE0_SCMPMODE_SHIFT;
		ui32ISPFeedback0 |= GLES1_EXTRACT(ui32ISPControlWordC, ISPC_SOP1) << EURASIA_USE_VISTEST_STATE0_SOP1_SHIFT;
		ui32ISPFeedback0 |= GLES1_EXTRACT(ui32ISPControlWordC, ISPC_SOP2) << EURASIA_USE_VISTEST_STATE0_SOP2_SHIFT;
		ui32ISPFeedback0 |= GLES1_EXTRACT(ui32ISPControlWordC, ISPC_SOP3) << EURASIA_USE_VISTEST_STATE0_SOP3_SHIFT;
		ui32ISPFeedback0 |= GLES1_EXTRACT(ui32ISPControlWordC, ISPC_SCMPMASK) << EURASIA_USE_VISTEST_STATE0_SMASK_SHIFT;		

		ui32ISPFeedback0 |= (ui32ISPControlWordA & EURASIA_ISPA_DWRITEDIS) ? EURASIA_USE_VISTEST_STATE0_DWDISABLE : 0;

		/* State word 1 */
		ui32ISPFeedback1 |= (ui32AlphaRef & 0xFF) << EURASIA_USE_VISTEST_STATE1_AREF_SHIFT;
		ui32ISPFeedback1 |= ((ui32AlphaTestFunc << EURASIA_USE_VISTEST_STATE1_ACMPMODE_SHIFT) & ~EURASIA_USE_VISTEST_STATE1_ACMPMODE_CLRMSK);	
		ui32ISPFeedback1 |= GLES1_EXTRACT(ui32ISPControlWordC, ISPC_SWMASK) << EURASIA_USE_VISTEST_STATE1_SWMASK_SHIFT;
		
		ui32ISPFeedback1 |= GLES1_EXTRACT(ui32ISPControlWordC, ISPC_SCMP) << EURASIA_USE_VISTEST_STATE1_CCWSCMP_SHIFT;
		ui32ISPFeedback1 |= GLES1_EXTRACT(ui32ISPControlWordC, ISPC_SOP1) << EURASIA_USE_VISTEST_STATE1_CCWSOP1_SHIFT;
		ui32ISPFeedback1 |= GLES1_EXTRACT(ui32ISPControlWordC, ISPC_SOP2) << EURASIA_USE_VISTEST_STATE1_CCWSOP2_SHIFT;
		ui32ISPFeedback1 |= GLES1_EXTRACT(ui32ISPControlWordC, ISPC_SOP3) << EURASIA_USE_VISTEST_STATE1_CCWSOP3_SHIFT;
#endif
	}

	/* HW doesn't have default for colormask, so always send B */
	ui32ISPControlWordA |= EURASIA_ISPA_BPRES;

	/* HW defaults to stencil off */
	if(ui32ISPControlWordC !=(EURASIA_ISPC_SCMP_ALWAYS | EURASIA_ISPC_SOP1_KEEP | EURASIA_ISPC_SOP2_KEEP | EURASIA_ISPC_SOP3_KEEP))

	{
		ui32ISPControlWordA |= EURASIA_ISPA_CPRES;
	}

	/* When Temps and PAs are unified, a pass type change in MSAA mode may result in the DMS info being recalculated */
#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	if((gc->psMode->ui32AntiAliasMode) && 
		((psCompiledRenderState->ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK) != 
		(ui32ISPControlWordA & ~EURASIA_ISPA_PASSTYPE_CLRMSK)))
	{
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_DMS_INFO;
	}
#endif


	/* ------------------------------- */
	/* Setup the compiled render state */
	/* ------------------------------- */
	if(psCompiledRenderState->ui32MTEControl != ui32MTEControl)
	{
		/* Emit the MTE control if its changed */
		gc->ui32EmitMask |= GLES1_EMITSTATE_MTE_STATE_CONTROL;
	}

	psCompiledRenderState->ui32MTEControl = ui32MTEControl;

	if((psCompiledRenderState->ui32ISPControlWordA != ui32ISPControlWordA) ||
	   (psCompiledRenderState->ui32ISPControlWordB != ui32ISPControlWordB) ||
	   (psCompiledRenderState->ui32ISPControlWordC != ui32ISPControlWordC))
	{
		/* Emit the ISP state if its changed */
		gc->ui32EmitMask |= GLES1_EMITSTATE_MTE_STATE_ISP;
	}

	psCompiledRenderState->ui32ISPControlWordA	= ui32ISPControlWordA;
	psCompiledRenderState->ui32ISPControlWordB	= ui32ISPControlWordB;
	psCompiledRenderState->ui32ISPControlWordC	= ui32ISPControlWordC;

	psCompiledRenderState->ui32AlphaTestFlags	= ui32AlphaTestFlags;

	if(psCompiledRenderState->ui32ISPFeedback0 != ui32ISPFeedback0 ||
		psCompiledRenderState->ui32ISPFeedback1 != ui32ISPFeedback1)
	{
		psCompiledRenderState->ui32ISPFeedback0		= ui32ISPFeedback0;
		psCompiledRenderState->ui32ISPFeedback1		= ui32ISPFeedback1;
	
		/* Emit the fragprog constants if they changed */
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;
	}

	if(!bTrivialBlend)
	{
		psCompiledRenderState->bNeedFBBlendCode = IMG_TRUE;
	}
} /* SetupRenderState */


/***********************************************************************************
 Function Name      : WriteVDMControlStream
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static GLES1_MEMERROR WriteVDMControlStream(GLES1Context *gc, IMG_UINT32 ui32NumIndices, IMG_DEV_VIRTADDR uIndexAddress, IMG_UINT32 ui32IndexOffset)
{
	IMG_UINT32 ui32VDMIndexListHeader;
	IMG_UINT32 ui32VDMPrimitiveType = aui32GLESPrimToVDMPrim[gc->sPrim.eCurrentPrimitiveType];
	IMG_UINT32 *pui32BufferBase, *pui32Buffer;
	IMG_DEV_VIRTADDR uPDSBaseAddr;
	GLES1Shader *psVertexShader = gc->sProgram.psCurrentVertexShader;
	GLES1ShaderVariant *psShaderVariant = gc->sProgram.psCurrentVertexVariant;
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
								ui32VDMPrimitiveType		 |
								(ui32NumIndices << EURASIA_VDM_IDXCOUNT_SHIFT);

	CalculateVertexDMSInfo(&gc->psSysContext->sHWInfo, psShaderVariant->ui32USEPrimAttribCount, 
						  psShaderVariant->ui32MaxTempRegs, psVertexShader->ui32USESecAttribDataSizeInDwords, 
						  (psVertexShader->ui32OutputSelects & ~EURASIA_MTE_VTXSIZE_CLRMSK) >> EURASIA_MTE_VTXSIZE_SHIFT,
						  &ui32DMSIndexList2, &ui32DMSIndexList4, &ui32DMSIndexList5);
	/*
		Get TA control stream space for index list block
	*/
	pui32BufferBase = CBUF_GetBufferSpace(gc->apsBuffers, MAX_DWORDS_PER_INDEX_LIST_BLOCK, CBUF_TYPE_VDM_CTRL_BUFFER, IMG_FALSE);

	if(!pui32BufferBase)
	{
		return GLES1_TA_BUFFER_ERROR;
	}

	pui32Buffer = pui32BufferBase;

	*pui32Buffer++ = ui32VDMIndexListHeader;

	/****************************************************************************
	  Index List 1: Base address of the index data
	  Index List 2 (Optional): Indicates size of indices supplied in primitive
	 ****************************************************************************/

	/* Index List 1 */
	*pui32Buffer++	= (uIndexAddress.uiAddr >> EURASIA_VDM_IDXBASE16_ALIGNSHIFT) << EURASIA_VDM_IDXBASE16_SHIFT;

	/* Index List 2 */
#if defined(SGX545)
	if(gc->sState.sShade.ui32ShadeModel==EURASIA_MTE_SHADE_RESERVED)
#else

#if defined(FIX_HW_BRN_25211)
	if(!gc->sAppHints.bUseC10Colours && (gc->sState.sShade.ui32ShadeModel==EURASIA_MTE_SHADE_GOURAUD))
#else
	if(gc->sState.sShade.ui32ShadeModel == EURASIA_MTE_SHADE_GOURAUD)
#endif /* defined(FIX_HW_BRN_25211) */

#endif 
	{
		*pui32Buffer++	= ui32DMSIndexList2 | EURASIA_VDM_IDXSIZE_16BIT | (ui32IndexOffset << EURASIA_VDM_IDXOFF_SHIFT);
	}
	else
	{
		*pui32Buffer++	= EURASIA_VDM_FLATSHADE_VERTEX2 | ui32DMSIndexList2 | EURASIA_VDM_IDXSIZE_16BIT |(ui32IndexOffset << EURASIA_VDM_IDXOFF_SHIFT);
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
	*pui32Buffer++	= ui32DMSIndexList5 | (ui32PDSDataSize	<< EURASIA_VDMPDS_DATASIZE_SHIFT);

	/*
		Lock TA control stream space for the index list block
	*/
	CBUF_UpdateBufferPos(gc->apsBuffers, (IMG_UINT32)(pui32Buffer - pui32BufferBase), CBUF_TYPE_VDM_CTRL_BUFFER);

	GLES1_INC_COUNT(GLES1_TIMER_VDM_CTRL_STATE_COUNT, (pui32Buffer - pui32BufferBase));
	
	/* 
		Update TA buffers committed primitive offset
	*/
	CBUF_UpdateBufferCommittedPrimOffsets(gc->apsBuffers, &gc->psRenderSurface->bPrimitivesSinceLastTA, (IMG_VOID *)gc, KickLimit_ScheduleTA);

	return GLES1_NO_ERROR;
}

/***********************************************************************************
 Function Name      : UpdateMVP
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Recalulates the modelviewprojection matrix.
************************************************************************************/
static IMG_VOID UpdateMVP(GLES1Context *gc)
{
	GLES1Transform *psMVTransform, *psPTransform;
	psPTransform  = gc->sTransform.psProjection;
	psMVTransform = gc->sTransform.psModelView;

	(*gc->sProcs.sMatrixProcs.pfnMult)(&psMVTransform->sMvp, &psMVTransform->sMatrix, &psPTransform->sMatrix);

}

/***********************************************************************************
  Function Name      : MoveFFTNLShaderToTop
  Inputs             : gc
  Outputs            : -
  Returns            : -
  Description        : Move the current FFTNL shader to the top of the list
***********************************************************************************/
static IMG_VOID MoveFFTNLShaderToTop(GLES1Context *gc)
{
	GLES1Shader *psVertexShader = gc->sProgram.psCurrentVertexShader;

	if(gc->sProgram.psVertex != psVertexShader)
	{
		/* Remove from current position in list */
		psVertexShader->psPrevious->psNext = psVertexShader->psNext;

		if(psVertexShader->psNext)
		{
			psVertexShader->psNext->psPrevious = psVertexShader->psPrevious;
		}

		/* Add to front of shader list */
		psVertexShader->psPrevious = IMG_NULL;
		psVertexShader->psNext = gc->sProgram.psVertex;

		if(gc->sProgram.psVertex)
		{
			gc->sProgram.psVertex->psPrevious = psVertexShader;
		}

		gc->sProgram.psVertex = psVertexShader;
	}
}

/**********************************************************************************
 Function Name      : PropagateDirtyConstants
 Inputs             : gc
 Outputs            :
 Returns            :
 Description        : Propagate dirty flags to five recently used FFTNL programs.
                      Set dirty all constants to the sixth program (so that programs
                      after first five always have dirty all constant setting)
***********************************************************************************/
static IMG_VOID PropagateDirtyConstants(GLES1Context *gc)
{
	IMG_UINT32 i;
	GLES1Shader *psVertexShader = gc->sProgram.psVertex;
	IMG_UINT32 ui32NumProgramsPropagated = 5;

	i = 0;
	while(psVertexShader)
	{
		/* Propagating dirty constant flags to the top five programs, and set dirty all to the sixth program */
		if(i < ui32NumProgramsPropagated)
		{
			psVertexShader->ui32ConstantDirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
		}
		else
		{
			psVertexShader->ui32ConstantDirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			/* No need to set all dirty for the following objects since it has been set */
			break;
		}

		i++;
		psVertexShader = psVertexShader->psNext;
	}
}


/***********************************************************************************
 Function Name      : ValidateState
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR ValidateState(GLES1Context *gc)
{
	GLES1_MEMERROR eError;
	IMG_BOOL bChanged;
	GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine);
	GLES1VertexArrayObject *psVAO = psVAOMachine->psActiveVAO;

	/* Assert VAO is not NULL */
	GLES1_ASSERT(VAO(gc));


	GLES1_TIME_START(GLES1_TIMER_VALIDATE_TIME);

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(gc->ui32NumEGLImageTextureTargetsBound)
	{
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE;
	}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */


	/* Only setup context's VAO dirty flag once starting ValidateState() */
	gc->ui32DirtyMask |= psVAO->ui32DirtyMask;


	/* Setup the context's dirty state regarding element buffer,
	   and accordingly set VAO Machine's element buffer */
	if (gc->ui32DirtyMask & GLES1_DIRTYFLAG_VAO_BINDING)
	{
		/* Set the context's dirty state with DIRTYFLAG_VAO_ELEMENT_BUFFER) */
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ELEMENT_BUFFER;
	}	
	if (gc->ui32DirtyMask & GLES1_DIRTYFLAG_VAO_ELEMENT_BUFFER)
	{
		psVAOMachine->psBoundElementBuffer = psVAO->psBoundElementBuffer;
	}


	/* Setup VAO related dirty flags regarding the change of array enables */
	if(gc->ui32DirtyMask & GLES1_DIRTYFLAG_VAO_CLIENT_STATE)
	{	
	    /* If VAO's array enables are different from VAO's previous array enables,
		   then VAO Machine's attribute stream needs to be reset */
	    if (psVAO->ui32PreviousArrayEnables != psVAO->ui32CurrentArrayEnables)
		{
			psVAO->ui32PreviousArrayEnables = psVAO->ui32CurrentArrayEnables;

			psVAO->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
		}

		/* If VAO's array enables are different from VAO Machine's array enables,
		   then VAO Machine's attribute stream needs to be reset */
	    if (psVAOMachine->ui32ArrayEnables != psVAO->ui32CurrentArrayEnables)
		{
			psVAOMachine->ui32ArrayEnables = psVAO->ui32CurrentArrayEnables;

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM;
		}

		gc->ui32DirtyMask |= psVAO->ui32DirtyMask;
	}

	/* Setup texture state */
	if(gc->ui32DirtyMask & GLES1_DIRTYFLAG_TEXTURE_STATE)
	{
		GLES1_TIME_START(GLES1_TIMER_SETUP_TEXTURESTATE_TIME);

		if(SetupTextureState(gc))
		{
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
		}

		GLES1_TIME_STOP(GLES1_TIMER_SETUP_TEXTURESTATE_TIME);
	}


	/* Setup VAO Machine's streams & pointers: have to be after SetupTextureState(), 
	   since SetupTextureState will set ui32NumImageUnitsActive, which is used in GenerateVertexCompileMask() */
	if(gc->sPrim.eCurrentPrimitiveType!=GLES1_PRIMTYPE_DRAWTEXTURE)
	{
		if(gc->ui32DirtyMask & (GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM | GLES1_DIRTYFLAG_VERTEX_PROGRAM))
		{
			GLES1_TIME_START(GLES1_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME);

			GenerateVertexCompileMask(gc);
			SetupVAOAttribStreams(gc);
			
			GLES1_TIME_STOP(GLES1_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME);

		}
		else if( ((gc->ui32DirtyMask & GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER) != 0) &&
				 ((gc->ui32DirtyMask & GLES1_DIRTYFLAG_VAO_BINDING) == 0) )
		{
			GLES1_TIME_START(GLES1_TIMER_SETUP_VAO_ATTRIB_POINTERS_TIME);

			SetupVAOAttribPointers(gc);
	
			GLES1_TIME_STOP(GLES1_TIMER_SETUP_VAO_ATTRIB_POINTERS_TIME);

		}
		else if (gc->ui32DirtyMask & GLES1_DIRTYFLAG_VAO_BINDING)
		{
			GLES1_TIME_START(GLES1_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME);

			GenerateVertexCompileMask(gc);
			SetupVAOAttribStreams(gc);
			
			GLES1_TIME_STOP(GLES1_TIMER_SETUP_VAO_ATTRIB_STREAMS_TIME);
			
		}
	}

	if( (gc->ui32DirtyMask & (GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM    | 
							  GLES1_DIRTYFLAG_VAO_BINDING      |
							  GLES1_DIRTYFLAG_VERTEX_PROGRAM)) != 0)
	{
		GLES1_TIME_START(GLES1_TIMER_SETUP_VERTEXSHADER_TIME);

		eError = SetupFFTNLShader(gc);

		if(eError != GLES1_NO_ERROR)
		{
			GLES1_TIME_STOP(GLES1_TIMER_SETUP_VERTEXSHADER_TIME);
			GLES1_TIME_STOP(GLES1_TIMER_VALIDATE_TIME);

			return eError;
		}

		eError = SetupUSEVertexShader(gc, &bChanged);

		if(eError != GLES1_NO_ERROR)
		{
			GLES1_TIME_STOP(GLES1_TIMER_SETUP_VERTEXSHADER_TIME);
			GLES1_TIME_STOP(GLES1_TIMER_VALIDATE_TIME);

			return eError;
		}

		if(bChanged)
		{
			/* Tex coords selects have changed, so we need to revalidate fragment program */
			if(SetupFFTNLShaderOutputs(gc))
			{
				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
			}

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VP_STATE;

			gc->ui32EmitMask |= GLES1_EMITSTATE_MTE_STATE_OUTPUTSELECTS;

			MoveFFTNLShaderToTop(gc);
		}
		else
		{
			gc->ui32DirtyMask &= ~GLES1_DIRTYFLAG_VERTEX_PROGRAM;
		}

		GLES1_TIME_STOP(GLES1_TIMER_SETUP_VERTEXSHADER_TIME);
	}

	if(gc->ui32DirtyMask & (GLES1_DIRTYFLAG_VP_STATE | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS))
	{
		PropagateDirtyConstants(gc);
	}

	if(gc->sProgram.psCurrentVertexShader->ui32ConstantDirtyMask & GLES1_DIRTYFLAG_VERTPROG_CONSTANTS)	
	{
		GLES1_TIME_START(GLES1_TIMER_SETUP_VERTEXCONSTANTS_TIME);

		UpdateMVP(gc);

		SetupBuildFFTNLShaderConstants(gc);
	
		gc->sProgram.psCurrentVertexShader->ui32ConstantDirtyMask = 0;

		GLES1_TIME_STOP(GLES1_TIMER_SETUP_VERTEXCONSTANTS_TIME);
	}
	
	if(gc->ui32DirtyMask & GLES1_DIRTYFLAG_RENDERSTATE)
	{
		GLES1_TIME_START(GLES1_TIMER_SETUP_RENDERSTATE_TIME);

		SetupRenderState(gc);

		GLES1_TIME_STOP(GLES1_TIMER_SETUP_RENDERSTATE_TIME);
	}
	
	if(gc->ui32DirtyMask & (GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_FRAGMENT_PROGRAM))
	{
		GLES1_TIME_START(GLES1_TIMER_SETUP_FRAGMENTSHADER_TIME);

		eError = ValidateFFTextureBlending(gc);
		
        if(eError != GLES1_NO_ERROR)
		{
			GLES1_TIME_STOP(GLES1_TIMER_VALIDATE_TIME);

			return eError;
		}

		eError = SetupUSEFragmentShader(gc, &bChanged);

		GLES1_TIME_STOP(GLES1_TIMER_SETUP_FRAGMENTSHADER_TIME);

		if(eError != GLES1_NO_ERROR)
		{
			GLES1_TIME_STOP(GLES1_TIMER_VALIDATE_TIME);

			return eError;
		}

		if(bChanged)
		{
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_FP_STATE;
		}
	}

	if(gc->ui32DirtyMask & (GLES1_DIRTYFLAG_RENDERSTATE | GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS))
	{
		ValidateFFTextureConstants(gc);
	}

	GLES1_TIME_STOP(GLES1_TIMER_VALIDATE_TIME);
	
	return GLES1_NO_ERROR;
}

/***********************************************************************************
 Function Name      : GLES1EmitState
 Inputs             : gc, ui32NumIndices, uIndexAddress, ui32IndexOffset
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_INTERNAL GLES1_MEMERROR GLES1EmitState(GLES1Context *gc, IMG_UINT32 ui32NumIndices, IMG_DEV_VIRTADDR uIndexAddress, IMG_UINT32 ui32IndexOffset)
{
	IMG_BOOL b32BitPDSIndices;
	GLES1_MEMERROR eError;
#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)
	IMG_BOOL bForceTAKick;
#endif /* defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH) */

	GLES1_TIME_START(GLES1_TIMER_STATE_EMIT_TIME);

	if(ui32NumIndices == 0)
	{
		eError = GLES1_NO_ERROR;
        goto StopTimerAndReturn;
	}

	/* Attach all used BOs and the VAO to the current kick */
	AttachAllUsedBOsAndVAOToCurrentKick(gc);

	/* If last primitive type does not match that about to be used re-emit the render state */
	if(aui32GLESPrimToISPPrim[gc->sPrim.eCurrentPrimitiveType] != 
	   aui32GLESPrimToISPPrim[gc->sPrim.ePreviousPrimitiveType])
	{
		gc->ui32EmitMask |= (GLES1_EMITSTATE_MTE_STATE_ISP | GLES1_EMITSTATE_MTE_STATE_CONTROL);
	}

	gc->sPrim.ePreviousPrimitiveType = gc->sPrim.eCurrentPrimitiveType;


	/* Are 32-bit indices needed in the PDS program? */
	if(ui32IndexOffset && (ui32IndexOffset+ui32NumIndices > 65536))
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

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_PRIM_INDEXBITS;
	}



	if(gc->ui32DirtyMask || gc->ui32EmitMask)
	{
		IMG_BOOL bChanged;

		/*
			Write the PDS pixel program for pixel shader secondary attributes
		*/
		if(gc->ui32DirtyMask & (GLES1_DIRTYFLAG_FP_STATE | GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS))
		{
			GLES1_TIME_START(GLES1_TIMER_WRITEPDSUSESHADERSAPROGRAM_TIME);

			if(gc->sAppHints.bDisableStaticPDSPixelSAProgram)
			{
				eError = WritePDSUSEShaderSAProgram(gc, GLES1_SHADERTYPE_FRAGMENT, &bChanged);
			}
			else
			{
				eError = WriteStaticPDSPixelSAProgram(gc, &bChanged);
			}
			
			GLES1_TIME_STOP(GLES1_TIMER_WRITEPDSUSESHADERSAPROGRAM_TIME);

			if(eError != GLES1_NO_ERROR)
			{
				goto StopTimerAndReturn;
			}

			if(bChanged)
			{
				gc->ui32EmitMask |= GLES1_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE;
			}
		}


		/*
			Write the PDS pixel program for iteration, texture fetch and the pixel shader
		*/
		if(gc->ui32DirtyMask & (GLES1_DIRTYFLAG_FP_STATE | GLES1_DIRTYFLAG_TEXTURE_STATE | GLES1_DIRTYFLAG_DMS_INFO))
		{
			GLES1_TIME_START(GLES1_TIMER_WRITEPDSPIXELSHADERPROGRAM_TIME);
		
			eError = WritePDSPixelShaderProgram(gc, &bChanged);
		
			GLES1_TIME_STOP(GLES1_TIMER_WRITEPDSPIXELSHADERPROGRAM_TIME);
	
			if(eError != GLES1_NO_ERROR)
			{
				goto StopTimerAndReturn;
			}

			if(bChanged)
			{
				gc->ui32EmitMask |= GLES1_EMITSTATE_PDS_FRAGMENT_STATE;
			}
		}


		/*
			Write the PDS vertex program for vertex fetch and the vertex shader
		*/
		    if( (gc->ui32DirtyMask & (GLES1_DIRTYFLAG_VP_STATE            | 
									  GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER  |
									  GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM   |
									  GLES1_DIRTYFLAG_VAO_BINDING         |
									  GLES1_DIRTYFLAG_PRIM_INDEXBITS  )) )
			{
#if !defined(SGX545)
			    if(gc->sAppHints.bEnableStaticPDSVertex &&
				   (gc->sVAOMachine.psActiveVAO == &(gc->sVAOMachine.sDefaultVAO)) &&
				   (gc->sVAOMachine.ui32NumItemsPerVertex <= GLES1_STATIC_PDS_NUMBER_OF_STREAMS)
				   )
				{
					if(gc->ui32DirtyMask & (GLES1_DIRTYFLAG_VP_STATE          | 
											GLES1_DIRTYFLAG_VAO_ATTRIB_STREAM | 
											GLES1_DIRTYFLAG_PRIM_INDEXBITS))
					{
						GLES1_TIME_START(GLES1_TIMER_WRITEPDSVERTEXSHADERPROGRAM_TIME);	

						eError = WritePDSStaticVertexShaderProgram(gc, b32BitPDSIndices);
						
						GLES1_TIME_STOP(GLES1_TIMER_WRITEPDSVERTEXSHADERPROGRAM_TIME);
						
						if(eError != GLES1_NO_ERROR)
						{
							goto StopTimerAndReturn;
						}
					}
					
					GLES1_TIME_START(GLES1_TIMER_WRITEPDSVERTEXAUXILIARY_TIME);

					eError = PatchAuxiliaryPDSVertexShaderProgram(gc);
					
					GLES1_TIME_STOP(GLES1_TIMER_WRITEPDSVERTEXAUXILIARY_TIME);

					if(eError != GLES1_NO_ERROR)
					{
						goto StopTimerAndReturn;
					}
				}
				else
#endif /* !defined(SGX545) */
				{
					eError = WritePDSVertexShaderProgramWithVAO(gc, b32BitPDSIndices);

					if(eError != GLES1_NO_ERROR)
					{
						goto StopTimerAndReturn;
					}
				}
			}
		/*
			Write the MTE state for tiling and pixel processing
		*/
		if(gc->ui32EmitMask &  (GLES1_EMITSTATE_MTE_STATE_REGION_CLIP |
								GLES1_EMITSTATE_MTE_STATE_VIEWPORT |
								GLES1_EMITSTATE_MTE_STATE_TAMISC |
								GLES1_EMITSTATE_MTE_STATE_OUTPUTSELECTS |
								GLES1_EMITSTATE_MTE_STATE_CONTROL |
								GLES1_EMITSTATE_MTE_STATE_ISP |
								GLES1_EMITSTATE_PDS_FRAGMENT_STATE |
								GLES1_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE))
		{
			IMG_DEV_VIRTADDR uStateHWAddress;
			IMG_UINT32 ui32NumStateDWords;

			GLES1_TIME_START(GLES1_TIMER_WRITEMTESTATE_TIME);

			eError = WriteMTEState(gc, &ui32NumStateDWords, &uStateHWAddress);

			GLES1_TIME_STOP(GLES1_TIMER_WRITEMTESTATE_TIME);

			if(eError != GLES1_NO_ERROR)
			{
				goto StopTimerAndReturn;
			}
		
			if(ui32NumStateDWords)
			{
				/*
					Write the PDS/USE programs for copying MTE state through the primary attributes to the output buffer
				*/
				GLES1_TIME_START(GLES1_TIMER_WRITEMTESTATECOPYPROGRAMS_TIME);

				if(gc->sAppHints.bEnableStaticMTECopy)
				{
					eError = PatchPregenMTEStateCopyPrograms(gc, ui32NumStateDWords, uStateHWAddress);
				}
				else
				{
					eError = WriteMTEStateCopyPrograms(gc, ui32NumStateDWords, uStateHWAddress);
				}

				GLES1_TIME_STOP(GLES1_TIMER_WRITEMTESTATECOPYPROGRAMS_TIME);
		
				if(eError != GLES1_NO_ERROR)
				{
					goto StopTimerAndReturn;
				}

				gc->ui32EmitMask |= GLES1_EMITSTATE_STATEUPDATE;
			}
		}


		/*
			Write the PDS vertex program for vertex shader secondary attributes
		*/
		if(gc->ui32DirtyMask & (GLES1_DIRTYFLAG_VP_STATE | 
								 GLES1_DIRTYFLAG_VERTPROG_CONSTANTS))
		{
			GLES1_TIME_START(GLES1_TIMER_WRITEPDSUSESHADERSAPROGRAM_TIME);

			eError = WritePDSUSEShaderSAProgram(gc, GLES1_SHADERTYPE_VERTEX, &bChanged);

			GLES1_TIME_STOP(GLES1_TIMER_WRITEPDSUSESHADERSAPROGRAM_TIME);
	
			if(eError != GLES1_NO_ERROR)
			{
				goto StopTimerAndReturn;
			}

			if(bChanged)
			{
				gc->ui32EmitMask |= GLES1_EMITSTATE_PDS_VERTEX_SECONDARY_STATE;
			}
		}

		/*
			Write the state block for tiling and pixel processing to the control stream
		*/
		if(gc->ui32EmitMask & GLES1_EMITSTATE_STATEUPDATE)
		{
			/* send the state */
			GLES1_TIME_START(GLES1_TIMER_SETUPSTATEUPDATE_TIME);

			eError = SetupStateUpdate(gc, IMG_TRUE);

			GLES1_TIME_STOP(GLES1_TIMER_SETUPSTATEUPDATE_TIME);

			if(eError != GLES1_NO_ERROR)
			{
				goto StopTimerAndReturn;
			}			
		}
			
		/*
			Write the vertex SA state block to the control stream
		*/
		if(gc->ui32EmitMask & GLES1_EMITSTATE_PDS_VERTEX_SECONDARY_STATE)
		{
			GLES1_TIME_START(GLES1_TIMER_SETUPSTATEUPDATE_TIME);

			eError = SetupStateUpdate(gc, IMG_FALSE);
			
			GLES1_TIME_STOP(GLES1_TIMER_SETUPSTATEUPDATE_TIME);

			if(eError != GLES1_NO_ERROR)
			{
				goto StopTimerAndReturn;
			}
		}
	}




	if(gc->sAppHints.bOptimisedValidation)
	{
		gc->ui32EmitMask = 0;
		gc->ui32DirtyMask = 0;
	}

	GLES1_TIME_START(GLES1_TIMER_WRITEVDMCONTROLSTREAM_TIME);

	/* Write VDM Control Stream after dirty flags have been reset - this way, if a TA is forcibly kicked,
	 * the new dirty flags will be retained.
	 */
	{
	    eError = WriteVDMControlStream(gc, ui32NumIndices, uIndexAddress, ui32IndexOffset);
	}

	GLES1_TIME_STOP(GLES1_TIMER_WRITEVDMCONTROLSTREAM_TIME);
	
	if(eError != GLES1_NO_ERROR)
	{
		goto StopTimerAndReturn;
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

	eError = GLES1_NO_ERROR;

StopTimerAndReturn:

	GLES1_TIME_STOP(GLES1_TIMER_STATE_EMIT_TIME);

	return eError;
}

/******************************************************************************
 End of file (validate.c)
******************************************************************************/
