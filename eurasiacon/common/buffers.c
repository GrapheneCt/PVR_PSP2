/******************************************************************************
 * Name         : buffers.c
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
 * $Log: buffers.c $
 * ./
 *  --- Revision Logs Removed --- 
 *****************************************************************************/
#include "psp2_pvr_defs.h"
#include "imgextensions.h"
#include "services.h"
#include "sgxmmu.h"
#include "sgxdefs.h"
#include "pvr_debug.h"
#include "buffers.h"

#if defined(PVRSRV_NEED_PVR_DPF) || defined(PVRSRV_NEED_PVR_TRACE)
#if (CBUF_NUM_BUFFERS==9)
static const char * const asBufferDesc[CBUF_NUM_BUFFERS] =
{
	"VDM_CTRL",
	"VERTEX_DATA",
	"INDEX_DATA",
	"PDS_VERT",
	"PDS_VERT_SEC_PREGEN",
	"PDS_MTE_COPY_PREGEN",
	"PDS_AUX_PREGEN",
	"PDS_FRAG",
	"USSE_FRAG",
};
#else
#error "CBUF_NUM_BUFFERS!=9"
#endif
#endif


#if defined(PDUMP) || defined(PVRSRV_NEED_PVR_DPF) || defined(PVRSRV_NEED_PVR_TRACE)

IMG_INTERNAL const IMG_CHAR * const pszBufferNames[CBUF_NUM_BUFFERS] = {
	"VDM Control Stream  ",
	"Dynamic Vertex data ",
	"Dynamic Index data  ",
	"PDS Vertex shader   ",
	"PreGen Vtx PDS Sec  ",
	"PreGen MTE PDS Copy ",
	"PreGen Aux PDS Copy ",
	"PDS Fragment shader ",
	"USSE Fragment buffer"
};
#endif /* defined(PDUMP) || defined(PVRSRV_NEED_PVR_DPF) || defined(PVRSRV_NEED_PVR_TRACE) */


/*****************************************************************************
 Function Name	: ExtraSizeForAlignment
 Inputs			: ui32Position, ui32AlignShifts
 Outputs		: -
 Returns		: Extra size needed
 Description	: Calculate extra size need for alignment.
*****************************************************************************/
#ifdef INLINE_IS_PRAGMA
#pragma inline(ExtraSizeForAlignment)
#endif
static INLINE IMG_UINT32 ExtraSizeForAlignment(IMG_UINT32 ui32Position, IMG_UINT32 ui32AlignShifts)
{
	if(ui32Position & ((1UL << ui32AlignShifts) - 1))
	{
		return ((1UL << ui32AlignShifts) - (ui32Position & ((1UL << ui32AlignShifts) -1 )));
	}
	else
	{
		return 0;
	}
}


#if defined(PDUMP)
/*****************************************************************************
 Function Name	: GetBufferSpaceUsed
 Inputs			: psBuffer
 Outputs		: -
 Returns		: Used buffer space
 Description	: Return how much space in the buffer is being
				: used in the current frame
*****************************************************************************/
IMG_INTERNAL IMG_UINT32 CBUF_GetBufferSpaceUsedInBytes(CircularBuffer *psBuffer)
{
	IMG_UINT32 ui32Space;

	if (psBuffer->ui32CommittedHWOffsetInBytes <= psBuffer->ui32CurrentWriteOffsetInBytes)
	{
		ui32Space = psBuffer->ui32CurrentWriteOffsetInBytes - psBuffer->ui32CommittedHWOffsetInBytes;
	}
	else
	{
		ui32Space = psBuffer->ui32BufferLimitInBytes - (psBuffer->ui32CommittedHWOffsetInBytes - psBuffer->ui32CurrentWriteOffsetInBytes);
	}

	return ui32Space;
}
#endif /* defined(PDUMP) */


/*****************************************************************************
 Function Name	: GetBufferSpaceLeftInBytes
 Inputs			: psBuffer, ui32ReadOffset, bSpaceCanWrap
 Outputs		: pbSpaceHasWrapped
 Returns		: Free buffer space
 Description	: Calculate contiguous free buffer space according to current read pos, 
				: write pos and limit
*****************************************************************************/
#ifdef INLINE_IS_PRAGMA
#pragma inline(GetBufferSpaceLeftInBytes)
#endif
static INLINE IMG_UINT32 GetBufferSpaceLeftInBytes( CircularBuffer *psBuffer,
													IMG_UINT32 ui32ReadOffset,
													IMG_BOOL bSpaceCanWrap, 
													IMG_BOOL *pbSpaceHasWrapped)
{
	IMG_UINT32 ui32SpaceLeftInBytes;

	*pbSpaceHasWrapped = IMG_FALSE;

	if(ui32ReadOffset > psBuffer->ui32CurrentWriteOffsetInBytes) 
	{
		ui32SpaceLeftInBytes =  ui32ReadOffset - psBuffer->ui32CurrentWriteOffsetInBytes;
	}
	else
	{
		ui32SpaceLeftInBytes = psBuffer->ui32BufferLimitInBytes - psBuffer->ui32CurrentWriteOffsetInBytes;

		if (bSpaceCanWrap)
		{
			/* If space at start of buffer is larger return that */
			if (ui32ReadOffset > ui32SpaceLeftInBytes)
			{
				ui32SpaceLeftInBytes = ui32ReadOffset;

				*pbSpaceHasWrapped = IMG_TRUE;
			}
			/* if ui32ReadOffset is in the middle of empty buffer */
			else if( (ui32ReadOffset == ui32SpaceLeftInBytes) && (ui32ReadOffset == psBuffer->ui32CurrentWriteOffsetInBytes) )
			{
				ui32SpaceLeftInBytes = psBuffer->ui32BufferLimitInBytes;

				*pbSpaceHasWrapped = IMG_TRUE;
			}
		}
	}

	/* Ensure buffer can never be completely filled as readoffset == writeoffset -> buffer empty */
	return ui32SpaceLeftInBytes - 4;
}


/*****************************************************************************
 Function Name	: GetTotalBufferSpaceLeftInBytes
 Inputs			: psBuffer, ui32ReadOffset
 Outputs		: -
 Returns		: Free buffer space
 Description	: Calculate contiguous free buffer space according to current read pos, 
				: write pos and limit
*****************************************************************************/
#ifdef INLINE_IS_PRAGMA
#pragma inline(GetTotalBufferSpaceLeftInBytes)
#endif
static INLINE IMG_UINT32 GetTotalBufferSpaceLeftInBytes(CircularBuffer *psBuffer, IMG_UINT32 ui32ReadOffset)
{
	IMG_UINT32 ui32SpaceLeftInBytes;

	if(ui32ReadOffset > psBuffer->ui32CurrentWriteOffsetInBytes) 
	{
		ui32SpaceLeftInBytes =  ui32ReadOffset - psBuffer->ui32CurrentWriteOffsetInBytes;
	}
	else
	{
		ui32SpaceLeftInBytes = (psBuffer->ui32BufferLimitInBytes - psBuffer->ui32CurrentWriteOffsetInBytes) + ui32ReadOffset;
	}

	/* Ensure buffer can never be completely filled as readoffset == writeoffset -> buffer empty */
	return ui32SpaceLeftInBytes - 4;
}


/*****************************************************************************
 Function Name	: CheckPDSBufferSpace
 Inputs			: psBuffer, ui32BufferType, ui32ReadOffset, ui32BytesRequired
 Outputs		: -
 Returns		: Success/failure
 Description	: Check if there is enough for the required size 
*****************************************************************************/
static IMG_BOOL CheckPDSBufferSpace(CircularBuffer *psBuffer,
									IMG_UINT32 ui32BufferType,
									IMG_UINT32 ui32ReadOffset,
									IMG_UINT32 ui32BytesRequired)
{
	IMG_UINT32 ui32AlignSpaceInBytes = 0;
	IMG_BOOL bSpaceHasWrapped;

	/* Avoid build warning in release mode */
	PVR_UNREFERENCED_PARAMETER(ui32BufferType);

	/*
		HW requirement for PDS base address: EURASIA_VDMPDS_BASEADDR_ALIGNSHIFT = 4, i.e 128 bits
		For best performance, it is recommended to align to 256 bit (EURASIA_VDMPDS_BASEADDR_CACHEALIGN = 32) 
		according to Paul Burgess's instruction.
	*/
	if(psBuffer->ui32CurrentWriteOffsetInBytes % EURASIA_VDMPDS_BASEADDR_CACHEALIGN)
	{
		ui32AlignSpaceInBytes = EURASIA_VDMPDS_BASEADDR_CACHEALIGN - (psBuffer->ui32CurrentWriteOffsetInBytes % EURASIA_VDMPDS_BASEADDR_CACHEALIGN);
	}
	
	/*
		Check free space 
	*/
	if(GetBufferSpaceLeftInBytes(psBuffer, ui32ReadOffset, IMG_FALSE, &bSpaceHasWrapped) > (ui32AlignSpaceInBytes + ui32BytesRequired))
	{
		/*
		   We have enough free space for the required size. 
		   Update the current write offset by taking align space into consideration.
		*/
		psBuffer->ui32CurrentWriteOffsetInBytes += ui32AlignSpaceInBytes;

		return IMG_TRUE;
	}
	/*  Can we get enough space if we wrap? */
	else if (GetBufferSpaceLeftInBytes(psBuffer, ui32ReadOffset, IMG_TRUE, &bSpaceHasWrapped) > (ui32AlignSpaceInBytes + ui32BytesRequired))	
	{
		if(bSpaceHasWrapped)
		{
			/* Reset write offset */
			PVR_DPF((PVR_DBG_VERBOSE, "CheckPDSBufferSpace: %s buffer wraps", asBufferDesc[ui32BufferType]));

			psBuffer->ui32CurrentWriteOffsetInBytes = 0;
		}
		else
		{
			psBuffer->ui32CurrentWriteOffsetInBytes += ui32AlignSpaceInBytes;
		}

		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}


/*****************************************************************************
 Function Name	: CheckUSSEBufferSpace
 Inputs			: psBuffer, ui32BufferType, ui32ReadOffset, ui32BytesRequired
 Outputs		: -
 Returns		: Success/failure
 Description	: Check if there is enough for the required size 
*****************************************************************************/
static IMG_BOOL CheckUSSEBufferSpace(CircularBuffer *psBuffer,
									IMG_UINT32 ui32BufferType,
									IMG_UINT32 ui32ReadOffset,
									IMG_UINT32 ui32BytesRequired)
{
	IMG_UINT32 ui32AlignSpaceInBytes;
	IMG_BOOL bSpaceHasWrapped;

	/* Avoid build warning in release mode */
	PVR_UNREFERENCED_PARAMETER(ui32BufferType);

	/*
		Check if the free space would straddle two instruction pages.
	*/
	if ((psBuffer->ui32CurrentWriteOffsetInBytes >> EURASIA_USE_CODE_PAGE_ALIGN_SHIFT) != ((psBuffer->ui32CurrentWriteOffsetInBytes + ui32BytesRequired - 1) >> EURASIA_USE_CODE_PAGE_ALIGN_SHIFT))
	{
		ui32AlignSpaceInBytes = ExtraSizeForAlignment(psBuffer->ui32CurrentWriteOffsetInBytes, EURASIA_USE_CODE_PAGE_ALIGN_SHIFT);
	}
	else
	{
		/* 
			HW requirement for USE execution address: EURASIA_PDS_DOUTU0_EXE0_ALIGNSHIFT = 4, i.e 128 bits
		*/
		ui32AlignSpaceInBytes = ExtraSizeForAlignment(psBuffer->ui32CurrentWriteOffsetInBytes, EURASIA_PDS_DOUTU0_EXE_ALIGNSHIFT);
	}
	
	/*
		Check free space 
	*/
	if(GetBufferSpaceLeftInBytes(psBuffer, ui32ReadOffset, IMG_FALSE, &bSpaceHasWrapped) > (ui32AlignSpaceInBytes + ui32BytesRequired))
	{
		/*
		   We have enough free space for the required size. 
		   Update the current write offset by taking align space into consideration.
		*/
		psBuffer->ui32CurrentWriteOffsetInBytes += ui32AlignSpaceInBytes;

		return IMG_TRUE;
	}
	/*  Can we get enough space if we wrap? */
	else if (GetBufferSpaceLeftInBytes(psBuffer, ui32ReadOffset, IMG_TRUE, &bSpaceHasWrapped) > (ui32AlignSpaceInBytes + ui32BytesRequired))	
	{
		if(bSpaceHasWrapped)
		{
			/* Reset write offset */
			PVR_DPF((PVR_DBG_VERBOSE, "CheckPDSBufferSpace: %s buffer wraps", asBufferDesc[ui32BufferType]));

			psBuffer->ui32CurrentWriteOffsetInBytes = 0;
		}
		else
		{
			psBuffer->ui32CurrentWriteOffsetInBytes += ui32AlignSpaceInBytes;
		}

		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}


/*****************************************************************************
 Function Name	: CheckVIBufferSpace
 Inputs			: psBuffer, ui32BufferType, ui32ReadOffset, ui32BytesRequired
 Outputs		: -
 Returns		: Success/failure
 Description	: Check if there is enough for the required size 
*****************************************************************************/
static IMG_BOOL CheckVIBufferSpace( CircularBuffer *psBuffer,
									IMG_UINT32 ui32BufferType,
									IMG_UINT32 ui32ReadOffset,
									IMG_UINT32 ui32BytesRequired)
{
	IMG_BOOL bSpaceHasWrapped;

	/* Avoid build warning in release mode */
	PVR_UNREFERENCED_PARAMETER(ui32BufferType);

	/*
		Check total free space
	*/
	if(GetTotalBufferSpaceLeftInBytes(psBuffer, ui32ReadOffset) < ui32BytesRequired)
	{
		return IMG_FALSE;
	}

	/*
		Check free space
	*/
	if(GetBufferSpaceLeftInBytes(psBuffer, ui32ReadOffset, IMG_FALSE, &bSpaceHasWrapped) > ui32BytesRequired)
	{
		return IMG_TRUE;
	}
	/*  Can we get enough space if we wrap? */
	else if (GetBufferSpaceLeftInBytes(psBuffer, ui32ReadOffset, IMG_TRUE, &bSpaceHasWrapped) > ui32BytesRequired)
	{
		if(bSpaceHasWrapped)
		{
			/* Reset write offset */
			PVR_DPF((PVR_DBG_VERBOSE, "CheckVIBufferSpace: %s buffer wraps", asBufferDesc[ui32BufferType]));

			psBuffer->ui32CurrentWriteOffsetInBytes = 0;
		}

		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}


/*****************************************************************************
 Function Name	: CheckTACtrlBufferSpace
 Inputs			: psTACtrlBuffer, ui32ReadOffset, ui32BytesRequired
 Outputs		: -
 Returns		: Success/failure
 Description	: Check if there is enough TA ctrl space for the required size.
				: Note: always needs to reserve space for terminator of TA.
*****************************************************************************/
static IMG_BOOL CheckTACtrlBufferSpace(CircularBuffer *psTACtrlBuffer, 
									   IMG_UINT32 ui32ReadOffset,
									   IMG_UINT32 ui32BytesRequired)
{
	IMG_BOOL bSpaceHasWrapped;
	/*
		Always leave space for terminate or stream link.
		Ensure we don't run into the "guard band" equivalent to the maximum burst size of the VDM: 
		Note, the burst size is variable, with variable alignment.
	*/
	ui32BytesRequired += ((VDM_CTRL_TERMINATE_DWORDS << 2) + EURASIA_VDM_CTRL_STREAM_BURST_SIZE);

	/*
		Get current buffer space left 
	*/
	if(GetBufferSpaceLeftInBytes(psTACtrlBuffer, ui32ReadOffset, IMG_FALSE, &bSpaceHasWrapped) > ui32BytesRequired)
	{
		return IMG_TRUE;
	}
	else if (GetBufferSpaceLeftInBytes(psTACtrlBuffer, ui32ReadOffset, IMG_TRUE, &bSpaceHasWrapped) > ui32BytesRequired)
	{
		if(bSpaceHasWrapped)
		{
			IMG_UINT32 *pui32Buffer =  psTACtrlBuffer->pui32BufferBase + (psTACtrlBuffer->ui32CurrentWriteOffsetInBytes >> 2);

			PVR_DPF((PVR_DBG_MESSAGE, "CheckTACtrlBufferSpace: TA ctrl buffer wraps, adding stream link "));

			/* Insert stream link to start of buffer*/
			pui32Buffer[0] = EURASIA_TAOBJTYPE_LINK | ((psTACtrlBuffer->uDevVirtBase.uiAddr >> EURASIA_TASTRMLINK_ADDR_ALIGNSHIFT) << EURASIA_TASTRMLINK_ADDR_SHIFT);
			
			/* Reset write offset */
			psTACtrlBuffer->ui32CurrentWriteOffsetInBytes = 0;
		}
		
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}


/***********************************************************************************
 Function Name      : GetBufferSpace
 Inputs             : apsBuffers, ui32DWordsRequired, ui32BufferType,
 Outputs            : -
 Returns            : New write address
 Description        : Request space in a buffer
************************************************************************************/
IMG_INTERNAL IMG_UINT32 *CBUF_GetBufferSpace(CircularBuffer **apsBuffers,
											 IMG_UINT32 ui32DWordsRequired,
											 IMG_UINT32 ui32BufferType,
											 IMG_BOOL bTerminate)
{
	CircularBuffer *psBuffer;
	IMG_BOOL bEnoughSpace, bFirstTry;
	IMG_UINT32 ui32ReadOffset;
	IMG_UINT32 ui32BytesRequired = ui32DWordsRequired << 2;

	/* Get the actual buffer */
	psBuffer = apsBuffers[ui32BufferType];

	if(psBuffer->bLocked)
	{
		PVR_DPF((PVR_DBG_ERROR, "CBUF_GetBufferSpace: Buffer is already locked !"));

		return IMG_NULL;
	}
	
	/* This code assumes that no single request for space is larger than the ui32SingleKickLimitInBytes */

	bFirstTry = IMG_TRUE;

	do
	{
		/*	Don't access the real HW read offset everytime, unless we run out of space */
		if(bFirstTry)
		{
			ui32ReadOffset = psBuffer->ui32ReadOffsetCopy;
		}
		else
		{
			/* Read HW offset */
			ui32ReadOffset = *psBuffer->pui32ReadOffset;

			psBuffer->ui32ReadOffsetCopy = ui32ReadOffset;
		}

		switch(ui32BufferType)
		{
			case CBUF_TYPE_VDM_CTRL_BUFFER:
			{
				if(bTerminate)
				{
					/* We have already reserved space for the terminate */
					bEnoughSpace = IMG_TRUE;
				}
				else
				{
					bEnoughSpace = CheckTACtrlBufferSpace(psBuffer, ui32ReadOffset, ui32BytesRequired);
				}

				break;
			}
			case CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER:
			case CBUF_TYPE_MTE_COPY_PREGEN_BUFFER:
			case CBUF_TYPE_PDS_AUXILIARY_PREGEN_BUFFER:
			case CBUF_TYPE_PDS_VERT_BUFFER:
			case CBUF_TYPE_PDS_FRAG_BUFFER:
			{
				bEnoughSpace = CheckPDSBufferSpace(psBuffer, ui32BufferType, ui32ReadOffset, ui32BytesRequired);

				break;
			}
			case CBUF_TYPE_USSE_FRAG_BUFFER:
			{
				bEnoughSpace = CheckUSSEBufferSpace(psBuffer, ui32BufferType, ui32ReadOffset, ui32BytesRequired);

				break;
			}
			default:
			{
				bEnoughSpace = CheckVIBufferSpace(psBuffer, ui32BufferType, ui32ReadOffset, ui32BytesRequired);

				break;		
			}
		}

		if(!bEnoughSpace)
		{
			if(ui32ReadOffset == apsBuffers[ui32BufferType]->ui32CommittedHWOffsetInBytes)
			{
				if(ui32BufferType == CBUF_TYPE_VERTEX_DATA_BUFFER || ui32BufferType == CBUF_TYPE_INDEX_DATA_BUFFER)
				{
					PVR_DPF((PVR_DBG_MESSAGE, "CBUF_GetBufferSpace: Run out of space in the %s buffer, with no outstanding HW ops", asBufferDesc[ui32BufferType])); 
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR, "CBUF_GetBufferSpace: Run out of space in the %s buffer, with no outstanding HW ops", asBufferDesc[ui32BufferType])); 
				}

				return IMG_NULL;		
			}

			if(!bFirstTry)
			{
				if(sceGpuSignalWait(sceKernelGetTLSAddr(0x44), 100000) != SCE_OK)
				{
					PVR_DPF((PVR_DBG_MESSAGE, "CBUF_GetBufferSpace: sceGpuSignalWait failed"));
				}
			}
		}
		bFirstTry = IMG_FALSE;
	}
	while(!bEnoughSpace);

	/* Mark buffer is locked and record the lock count */
	psBuffer->bLocked = IMG_TRUE;
	psBuffer->ui32LockCount = ui32DWordsRequired;

#if defined(DEBUG)
	if(psBuffer->ui32CurrentWriteOffsetInBytes + ui32BytesRequired > psBuffer->ui32BufferLimitInBytes)
	{
		/* This should not be happening, the current write offset have already been wrapped */
		PVR_DPF((PVR_DBG_ERROR, "CBUF_GetBufferSpace: Space required cannot fit in the end of USE buffer"));
	}
#endif /* defined(DEBUG) */

	return psBuffer->pui32BufferBase + (psBuffer->ui32CurrentWriteOffsetInBytes >> 2); 
}


/*****************************************************************************
 Function Name	: UpdateBufferPos
 Inputs			: apsBuffers, ui32DWordsWritten, ui32BufferType
 Outputs		: 
 Returns		: 
 Description	: 
*****************************************************************************/
IMG_INTERNAL IMG_VOID CBUF_UpdateBufferPos(CircularBuffer **apsBuffers, 
										   IMG_UINT32 ui32DWordsWritten,
										   IMG_UINT32 ui32BufferType)
{
	IMG_UINT32 ui32Limit;
	IMG_UINT32 *pui32WriteOffset;
#ifdef DEBUG 
	IMG_UINT32 ui32FreeSpaceInBytes;
	IMG_BOOL bSpaceHasWrapped;
#endif
	CircularBuffer *psBuffer = apsBuffers[ui32BufferType];

	if(!psBuffer->bLocked)
	{
		PVR_DPF((PVR_DBG_ERROR, "CBUF_UpdateBufferPos: Buffer is not locked, cannot update pos"));

		return;
	}

	if(ui32DWordsWritten > psBuffer->ui32LockCount)
	{
		PVR_DPF((PVR_DBG_WARNING, "CBUF_UpdateBufferPos: More dwords written than lock count in USE buffer!"));
	}

	ui32Limit = psBuffer->ui32BufferLimitInBytes;
	pui32WriteOffset = &psBuffer->ui32CurrentWriteOffsetInBytes;

#ifdef DEBUG
	if(*pui32WriteOffset + (ui32DWordsWritten << 2) > ui32Limit)
	{
		PVR_DPF((PVR_DBG_WARNING, "CBUF_UpdateBufferPos: Number of words written is crossing the end of the buffer!"));
	}

	ui32FreeSpaceInBytes = GetBufferSpaceLeftInBytes(psBuffer, *psBuffer->pui32ReadOffset, IMG_TRUE, &bSpaceHasWrapped);

	if(ui32FreeSpaceInBytes < (ui32DWordsWritten << 2))
	{
		PVR_DPF((PVR_DBG_WARNING, "CBUF_UpdateBufferPos: Free space is less than words written in buffer"));
	}
	
#endif
	*pui32WriteOffset = *pui32WriteOffset + (ui32DWordsWritten << 2);

	if(*pui32WriteOffset == ui32Limit)
	{
		PVR_DPF((PVR_DBG_VERBOSE, "CBUF_UpdateBufferPos: %s buffer wraps ", asBufferDesc[ui32BufferType])); 

		*pui32WriteOffset = 0;
	}

	/* Unlock the buffer */
	psBuffer->bLocked = IMG_FALSE;
}


/*****************************************************************************
 Function Name	: GetBufferDeviceAddress
 Inputs			: apsBuffers, *pui32LinAddr, ui32BufferType
 Outputs		: 
 Returns		: 
 Description	: Get TA buffer device virtual address from its linear address
*****************************************************************************/
IMG_INTERNAL IMG_DEV_VIRTADDR CBUF_GetBufferDeviceAddress(CircularBuffer **apsBuffers, 
														  const IMG_VOID *pvLinAddr,
														  IMG_UINT32 ui32BufferType)
{
	CircularBuffer *psBuffer = apsBuffers[ui32BufferType];
	IMG_UINT32 ui32Offset;
	IMG_DEV_VIRTADDR uDevAddr;

	PVR_ASSERT((IMG_UINT32)pvLinAddr < (IMG_UINT32)((IMG_UINT8 *)psBuffer->pui32BufferBase + psBuffer->ui32BufferLimitInBytes));

	ui32Offset = (IMG_UINT32)((const IMG_UINT8 *)pvLinAddr - (IMG_UINT8 *)psBuffer->pui32BufferBase);

	uDevAddr.uiAddr = psBuffer->uDevVirtBase.uiAddr + ui32Offset;

	return uDevAddr;
}


/*****************************************************************************
 Function Name	: UpdateBufferCommittedPrimOffsets
 Inputs			: apsBuffers, psRenderSurface
 Outputs		: 
 Returns		: 
 Description	: Update TA buffers commited primitive offset. It is called after 
				: we emit a primitive
*****************************************************************************/
IMG_INTERNAL IMG_VOID CBUF_UpdateBufferCommittedPrimOffsets(CircularBuffer **apsBuffers, 
															IMG_BOOL *pbPrimitivesSinceLastTA,
															IMG_VOID *pvContext,
															IMG_VOID (*pfnScheduleTA)(IMG_VOID *, IMG_BOOL))
{
	IMG_UINT32 i, ui32KickSizeInBytes;
	IMG_BOOL bKickTA, bLastInScene;

	CircularBuffer *psBuffer;

	bKickTA = IMG_FALSE;

	bLastInScene = IMG_FALSE;

	for(i = 0; i < CBUF_NUM_BUFFERS; i++)
	{
#if defined(OGLES1_MODULE) || defined(OGLES2_MODULE)
		/* Skip vertex and index buffers - they will be updated by CBUF_UpdateVIBufferCommittedPrimOffsets */
		if((i==CBUF_TYPE_VERTEX_DATA_BUFFER) || (i==CBUF_TYPE_INDEX_DATA_BUFFER))
		{
			continue;
		}

#endif /* defined(OGLES1_MODULE) || defined(OGLES2_MODULE) */

		psBuffer = apsBuffers[i];

		if(psBuffer != IMG_NULL)
		{
			psBuffer->ui32CommittedPrimOffsetInBytes = psBuffer->ui32CurrentWriteOffsetInBytes;

			if(psBuffer->ui32CommittedPrimOffsetInBytes >= psBuffer->ui32CommittedHWOffsetInBytes)
			{
				ui32KickSizeInBytes = psBuffer->ui32CommittedPrimOffsetInBytes - psBuffer->ui32CommittedHWOffsetInBytes;
			}
			else
			{
				ui32KickSizeInBytes = psBuffer->ui32BufferLimitInBytes - psBuffer->ui32CommittedHWOffsetInBytes + psBuffer->ui32CommittedPrimOffsetInBytes;
			}

			if(ui32KickSizeInBytes >= psBuffer->ui32SingleKickLimitInBytes)
			{
				bKickTA = IMG_TRUE;

				if(i >= CBUF_TYPE_PDS_FRAG_BUFFER)
				{
					PVR_DPF ((PVR_DBG_WARNING, "Kicking render due to frag buffer space"));
					bLastInScene = IMG_TRUE;
				}

#if defined(DEBUG) || defined(TIMING)
				psBuffer->ui32KickCount++;
#endif /* defined(DEBUG) || defined(TIMING) */
			}
		}
	}

	*pbPrimitivesSinceLastTA = IMG_TRUE;

	/* This should be a perfect position to kick the HW - after we have completed a primitive */
	if(bKickTA)
	{
		pfnScheduleTA(pvContext, bLastInScene);
	}
}


#if defined(OGLES1_MODULE) || defined(OGLES2_MODULE)
/*****************************************************************************
 Function Name	: CBUF_UpdateVIBufferCommittedPrimOffsets
 Inputs			: apsBuffers, pvContext, pfnScheduleTA
 Outputs		: pbPrimitivesSinceLastTA
 Returns		: 
 Description	: Update vertex and index buffer commited primitive offsets. 
 				  It is called  at the very end of a primitive
*****************************************************************************/
IMG_INTERNAL IMG_VOID CBUF_UpdateVIBufferCommittedPrimOffsets(CircularBuffer **apsBuffers, 
															IMG_BOOL *pbPrimitivesSinceLastTA,
															IMG_VOID *pvContext,
															IMG_VOID (*pfnScheduleTA)(IMG_VOID *, IMG_BOOL))
{
	IMG_UINT32 i, ui32KickSizeInBytes;
	IMG_BOOL bKickTA, bLastInScene;
	CircularBuffer *psBuffer;

	bKickTA = IMG_FALSE;

	bLastInScene = IMG_FALSE;

	for(i=CBUF_TYPE_VERTEX_DATA_BUFFER; i<=CBUF_TYPE_INDEX_DATA_BUFFER; i++)
	{
		psBuffer = apsBuffers[i];

		if(psBuffer != IMG_NULL)
		{
			psBuffer->ui32CommittedPrimOffsetInBytes = psBuffer->ui32CurrentWriteOffsetInBytes;

			if(psBuffer->ui32CommittedPrimOffsetInBytes >= psBuffer->ui32CommittedHWOffsetInBytes)
			{
				ui32KickSizeInBytes = psBuffer->ui32CommittedPrimOffsetInBytes - psBuffer->ui32CommittedHWOffsetInBytes;
			}
			else
			{
				ui32KickSizeInBytes = psBuffer->ui32BufferLimitInBytes - psBuffer->ui32CommittedHWOffsetInBytes + psBuffer->ui32CommittedPrimOffsetInBytes;
			}

			if(ui32KickSizeInBytes >= psBuffer->ui32SingleKickLimitInBytes)
			{
				bKickTA = IMG_TRUE;

#if defined(DEBUG) || defined(TIMING)
				psBuffer->ui32KickCount++;
#endif /* defined(DEBUG) || defined(TIMING) */
			}
		}
	}

	*pbPrimitivesSinceLastTA = IMG_TRUE;

	/* This should be a perfect position to kick the HW - after we have completed a primitive */
	if(bKickTA)
	{
		pfnScheduleTA(pvContext, bLastInScene);
	}
}
#endif /* defined(OGLES1_MODULE) || defined(OGLES2_MODULE) */


/*****************************************************************************
 Function Name	: UpdateBufferCommittedHWOffsets
 Inputs			: apsBuffers
 Outputs		: 
 Returns		: 
 Description	: Update buffers committed HW offset. It is called  
				: after kicking the TA
*****************************************************************************/
IMG_INTERNAL IMG_VOID CBUF_UpdateBufferCommittedHWOffsets(CircularBuffer **apsBuffers, IMG_BOOL bFinalFlush)
{
	IMG_UINT32 i;
	CircularBuffer *psBuffer;
	IMG_UINT32 ui32NumBufferToUpdate;

	if(bFinalFlush) 
	{
		ui32NumBufferToUpdate = CBUF_NUM_BUFFERS;
	}
	else
	{
		/* The last buffers are for render */
		ui32NumBufferToUpdate = CBUF_NUM_TA_BUFFERS;
	}

	for(i=0; i<ui32NumBufferToUpdate; i++)
	{
		psBuffer = apsBuffers[i];

		if(psBuffer != IMG_NULL)
		{
			/* Update the committed hw offset */
			psBuffer->ui32CommittedHWOffsetInBytes = psBuffer->ui32CommittedPrimOffsetInBytes;

			/* Update HW read offset copy */
			psBuffer->ui32ReadOffsetCopy = *psBuffer->pui32ReadOffset;
		}
	}
}


/*****************************************************************************
 Function Name	: UpdateTACtrlKickBase
 Inputs			: apsBuffers
 Outputs		: 
 Returns		: 
 Description	: Update TA ctrl stream base for the next TA kick.
*****************************************************************************/
IMG_INTERNAL IMG_VOID CBUF_UpdateTACtrlKickBase(CircularBuffer **apsBuffers)
{
	CircularBuffer *psTACtrlBuffer = apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER];

	/* Extra bytes needed for alignment */
	IMG_UINT32 ui32AlignSizeInBytes = ExtraSizeForAlignment(psTACtrlBuffer->ui32CurrentWriteOffsetInBytes, EUR_CR_VDM_CTRL_STREAM_BASE_ALIGNSHIFT);

	/* Make sure we got enough space for the alignment */
	if (!CheckTACtrlBufferSpace(psTACtrlBuffer, *psTACtrlBuffer->pui32ReadOffset, ui32AlignSizeInBytes))
	{
		/* Should always be space after we've assigned some new buffers */
		PVR_DPF ((PVR_DBG_ERROR, "CBUF_UpdateTACtrlKickBase: Should always be space after we've assigned some new buffers"));
	}

	/* Only align if not at start of buffer */
	if (psTACtrlBuffer->ui32CurrentWriteOffsetInBytes)
	{
		psTACtrlBuffer->ui32CurrentWriteOffsetInBytes += ui32AlignSizeInBytes;
	}

	psTACtrlBuffer->uTACtrlKickDevAddr.uiAddr = 
		psTACtrlBuffer->uDevVirtBase.uiAddr + psTACtrlBuffer->ui32CurrentWriteOffsetInBytes;
}


/***********************************************************************************
 Function Name      : CreateBuffer
 Inputs             : ps3DDevData, ui32BufferType, hHeapAllocator, ui32Size, hPerProcRef
 Outputs            : 
 Returns            : Buffer pointer
 Description        : Creates a buffer in device memory
************************************************************************************/
IMG_INTERNAL CircularBuffer *CBUF_CreateBuffer(PVRSRV_DEV_DATA *ps3DDevData, 
												  IMG_UINT32   ui32BufferType,
												  IMG_HANDLE   hHeapAllocator,
												  IMG_HANDLE   hSyncInfoHeapAllocator,
												  IMG_HANDLE   hOSEvent,
												  IMG_UINT32   ui32Size,
												  IMG_SID	   hPerProcRef)
{
	CircularBuffer *psBuffer;
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;
	IMG_UINT32 ui32Align;
	IMG_UINT32 ui32AllocFlags = PVRSRV_MEM_READ;

	PVR_ASSERT(ui32BufferType < CBUF_NUM_TA_BUFFERS);

	psBuffer = (CircularBuffer *)PVRSRVAllocUserModeMem(sizeof(CircularBuffer));
	if (!psBuffer)
	{
		PVR_DPF((PVR_DBG_ERROR,"CBUF_CreateBuffer: psBuffer allocation failed"));
		return IMG_NULL;
	}

	switch(ui32BufferType)
	{
		case CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER:
		case CBUF_TYPE_MTE_COPY_PREGEN_BUFFER:
		case CBUF_TYPE_PDS_VERT_BUFFER:
		case CBUF_TYPE_PDS_AUXILIARY_PREGEN_BUFFER:
		{
			ui32Align = EURASIA_PDS_DATA_CACHE_LINE_SIZE;


			break;
		}
		case CBUF_TYPE_PDS_FRAG_BUFFER:
		{
			PVR_DPF((PVR_DBG_ERROR,"CBUF_CreateBuffer: Requested buffer type should not be CBUF_TYPE_PDS_FRAG_BUFFER"));

			return psBuffer;
		}
		case CBUF_TYPE_USSE_FRAG_BUFFER:
		{
			PVR_DPF((PVR_DBG_ERROR,"CBUF_CreateBuffer: Requested buffer type should not be CBUF_TYPE_USE_FRAG_BUFFER"));

			return psBuffer;
		}
		case CBUF_TYPE_VERTEX_DATA_BUFFER:
		{
			ui32Size = (ui32Size + (EURASIA_CACHE_LINE_SIZE - 1)) & ~(EURASIA_CACHE_LINE_SIZE - 1);
			ui32Align = EURASIA_CACHE_LINE_SIZE;
			break;
		}
		case CBUF_TYPE_INDEX_DATA_BUFFER:
		{
			ui32Size = (ui32Size + (EURASIA_VDM_INDEX_FETCH_BURST_SIZE - 1)) & ~(EURASIA_VDM_INDEX_FETCH_BURST_SIZE - 1);
			ui32Align = EURASIA_VDM_INDEX_FETCH_BURST_SIZE;
			break;
		}
		case CBUF_TYPE_VDM_CTRL_BUFFER:
		{
			ui32Size = (ui32Size + (EURASIA_VDM_CTRL_STREAM_BURST_SIZE - 1)) & ~(EURASIA_VDM_CTRL_STREAM_BURST_SIZE - 1);
			ui32Align = EURASIA_VDM_CTRL_STREAM_BURST_SIZE;
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"CBUF_CreateBuffer: Invalid buffer type %d", ui32BufferType));
			PVRSRVFreeUserModeMem(psBuffer);
			return IMG_NULL;
		}
	}

	// PSP2: Add compulsory flags
	ui32AllocFlags |= PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC;

	if(PVRSRVAllocDeviceMem(ps3DDevData,
							hHeapAllocator,
							ui32AllocFlags,
							ui32Size,
							ui32Align,
							hPerProcRef,
							&psMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"CBUF_CreateBuffer: DeviceMem alloc of 0x%x failed for buffer %d", ui32Size, ui32BufferType));
		PVRSRVFreeUserModeMem(psBuffer);
		return IMG_NULL;
	}

	if (PVRSRVAllocSyncInfo(ps3DDevData, &psMemInfo->psClientSyncInfo))
	{
		PVR_DPF((PVR_DBG_ERROR, "CBUF_CreateBuffer: PVRSRVAllocSyncInfo failed for buffer %d", ui32BufferType));
		PVRSRVFreeDeviceMem(ps3DDevData, psMemInfo);
		PVRSRVFreeUserModeMem(psBuffer);
		return IMG_NULL;
	}
	
	psBuffer->psMemInfo							= psMemInfo;
	psBuffer->pui32BufferBase					= psMemInfo->pvLinAddr;
	psBuffer->uDevVirtBase						= psMemInfo->sDevVAddr;
	psBuffer->ui32BufferLimitInBytes			= ui32Size;
	psBuffer->ui32SingleKickLimitInBytes		= ui32Size >> 1;
	psBuffer->ui32ReadOffsetCopy				= 0;

	psBuffer->ui32CurrentWriteOffsetInBytes		= 0;
	psBuffer->ui32CommittedPrimOffsetInBytes	= 0;
	psBuffer->ui32CommittedHWOffsetInBytes		= 0;
	psBuffer->uTACtrlKickDevAddr				= psBuffer->uDevVirtBase;

	psBuffer->bLocked							= IMG_FALSE;
	psBuffer->ui32LockCount						= 0;

	psBuffer->psDevData							= ps3DDevData;

#if defined(PDUMP)
	psBuffer->bSyncDumped						= IMG_FALSE;
#endif /* defined(PDUMP) */

#if defined(DEBUG) || defined(TIMING)
	psBuffer->ui32KickCount						= 0;
#endif /* defined(DEBUG) || defined(TIMING) */

	/* Allocate device memory for status update */
	if(PVRSRVAllocDeviceMem(ps3DDevData,
							hSyncInfoHeapAllocator, 
							PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_MEM_CACHE_CONSISTENT | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC, 
							4, 
							4,
							hPerProcRef,
							&psBuffer->psStatusUpdateMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"CBUF_CreateBuffer: Failed to alloc sync update dev mem for buffer %u",ui32BufferType));
		PVRSRVFreeUserModeMem(psBuffer);
		return IMG_NULL;
	}
	PVRSRVMemSet(psBuffer->psStatusUpdateMemInfo->pvLinAddr, 0, 4);

	return psBuffer;
}


/***********************************************************************************
 Function Name      : DestroyBuffer
 Inputs             : ps3DDevData, psBuffer
 Outputs            : 
 Returns            : Success
 Description        : Destroys a device memory buffer
************************************************************************************/
IMG_INTERNAL IMG_VOID CBUF_DestroyBuffer(PVRSRV_DEV_DATA *ps3DDevData, CircularBuffer *psBuffer)
{
	if (psBuffer->psMemInfo->psClientSyncInfo != IMG_NULL)
	{
		PVRSRVFreeSyncInfo(ps3DDevData, psBuffer->psMemInfo->psClientSyncInfo);
	}

	PVRSRVFreeDeviceMem(ps3DDevData, psBuffer->psMemInfo);

	PVRSRVFreeDeviceMem(ps3DDevData, psBuffer->psStatusUpdateMemInfo);

	PVRSRVFreeUserModeMem(psBuffer);
}


/******************************************************************************
 End of file (buffers.c)
******************************************************************************/
