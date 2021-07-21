/*!****************************************************************************
@File           sgxtransfer_2d.c

@Title          Device specific transfer queue routines

@Author         Imagination Technologies

@date           08/02/06

@Copyright      Copyright 2007-2009 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or other-wise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform       Generic

@Description    Device specific functions

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: sgxtransfer_2d.c $
******************************************************************************/

#include <stddef.h>

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "img_types.h"
#include "services.h"
#include "servicesext.h"
#include "sgxapi.h"
#include "servicesint.h"
#include "pvr_debug.h"
#include "pixevent.h"
#include "pvr_bridge.h"
#include "sgxinfo.h"
#include "sgxinfo_client.h"
#include "sgxtransfer_client.h"
#include "sgxutils_client.h"
#include "sgx_bridge_um.h"
#include "sgxptladefs.h"

#define PTLA_FORMAT_GROUP_RGB		1
#define PTLA_FORMAT_GROUP_MONO		2
#define PTLA_FORMAT_GROUP_YUV		3
#define PTLA_FORMAT_GROUP_RAW		4


#if defined(SGX_FEATURE_PTLA) /*FIXME: CHANGE THE MAKEFILES SO THIS IS ONLY COMPILED WHEN NEEDED*/

#define PTLA_MAX_CMD_SIZE				30

static IMG_INT32 GetUpscaleAlpha(const IMG_INT32 DestSize, const IMG_INT32 SrcSize);

static PVRSRV_ERROR GeneratePTLAControlStream_Fill (const SGX_QUEUETRANSFER *psQueueTransfer,
									IMG_UINT32 *pui32ControlStream, IMG_UINT32	*pui32Bytes);

static PVRSRV_ERROR GeneratePTLAControlStream (const SGX_QUEUETRANSFER *psQueueTransfer,
									IMG_UINT32 *pui32ControlStream,
									IMG_UINT32	*pui32Bytes);

static PVRSRV_ERROR GetPTLAFormat(const PVRSRV_PIXEL_FORMAT eFormat,
									const SGXTQ_MEMLAYOUT eMemLayout,
									IMG_PUINT32 pPTLAFormat,
									IMG_PUINT32 pPTLASurfaceLayout,
									IMG_PUINT32 pNoOfPlanes,
									IMG_PUINT32 pGroup);

static IMG_UINT32 GetColKeyMask(const PVRSRV_PIXEL_FORMAT Format);

static IMG_BOOL IsPo2 (const IMG_UINT32 v);

/*****************************************************************************
 * Function Name		:	SGXQueue2DTransfer
 * Inputs				:	hTransferContext - The context created by CreateTransferContext()
 * 							psQueueTransfer - The transfer operation supplied by the client
 * Outputs				:	Error status
 * Description			:	The client calls this to send a transfer operation to the hardware.
 *							Services may split the transfer operation into several hardware kicks.
 *
 *							If SGXTQ_PREP_SUBMIT_SEPERATE is defined then this *doesn't* do any kick,
 *							the client has to call the SGXKickTransfer() to really submit the queued
 *							commands.
 ******************************************************************************/
#if defined(__psp2__)
IMG_INTERNAL
PVRSRV_ERROR IMG_CALLCONV SGXQueue2DTransfer(PVRSRV_DEV_DATA *psDevData,
											IMG_HANDLE hTransferContext,
											SGX_QUEUETRANSFER *psQueueTransfer)
#else
IMG_INTERNAL
PVRSRV_ERROR IMG_CALLCONV SGXQueue2DTransfer(IMG_HANDLE hTransferContext,
										   SGX_QUEUETRANSFER *psQueueTransfer)
#endif
{
	PVRSRV_ERROR eError;
	IMG_UINT32 aui32ControlStream[PTLA_MAX_CMD_SIZE];
	IMG_UINT32 ui32Bytes = 0; /* Initialise variable to avoid incorrect compiler warning */

	/*
	 * The blit dispatcher
	 */
	switch (psQueueTransfer->eType)
	{
		case SGXTQ_BLIT:
		case SGXTQ_FILL:
		{
			switch (psQueueTransfer->eType)
			{
				default:
				case SGXTQ_BLIT:
				{
					eError = GeneratePTLAControlStream(psQueueTransfer,
														aui32ControlStream,
														&ui32Bytes);
					break;
				}
				case SGXTQ_FILL:
				{
					eError = GeneratePTLAControlStream_Fill(psQueueTransfer,
														aui32ControlStream,
														&ui32Bytes);
					break;
				}
			}//switch
			
			if (eError != PVRSRV_OK)
			{
				return eError;
			}

			eError = SGXTransferControlStream(
				aui32ControlStream,
				ui32Bytes,
				psDevData,
				hTransferContext,
				psQueueTransfer->asDests->psSyncInfo,
				psQueueTransfer->ui32Flags & SGX_KICKTRANSFER_FLAGS_TATQ_SYNC,
				psQueueTransfer->ui32Flags & SGX_KICKTRANSFER_FLAGS_3DTQ_SYNC,
				IMG_NULL);
		}

		default:
		{
			/* Operation type not recognised */
			eError = PVRSRV_ERROR_NOT_SUPPORTED;
			return eError;
		}
	}

	return eError;
}



#if defined (FIX_HW_BRN_34939)
/*
	Detect and work around the PTLA scaling bug BRN_34939
  	The bug occurs when the dst_x_size > 8, and dst_y_size < 8, and dst_x_size mod 8 = 5, 6 or 7.

	The workaround is to split the blit into 2 parts:
	The first blit has a dst_x_size where dst_x_size mod 8 = 0,
	the 2nd blit has a dst_x_size where dst_x_size mod 8 = 5,6 or 7.
*/
static PVRSRV_ERROR GenerateFixedPTLAControlStream(const SGX_QUEUETRANSFER *psQueueTransfer,
									  IMG_UINT32 *pui32ControlStream,
									  IMG_UINT32 *pui32Bytes);
#define SCALE_TYPE_UNSUPPORTED	0
#define SCALE_TYPE_NONE			1
#define SCALE_TYPE_DOWNSCALE	2
#define SCALE_TYPE_UPSCALE		3
#define SCALE_TYPE_UPDOWNSCALE	4

static IMG_UINT32 GetScaleType(const IMG_INT32 iSrcSize, const IMG_INT32 iDestSize)
{
	IMG_UINT32 ui32ScaleType;
	const IMG_INT32 iDestSize_x2 = iDestSize<<1; // dest size x 2

	if (iDestSize == iSrcSize)
	{
		ui32ScaleType = SCALE_TYPE_NONE; // no scaling needed. scale factor = 1.0
	}
	else if (iDestSize_x2 < iSrcSize)
	{
		ui32ScaleType = SCALE_TYPE_UNSUPPORTED; // scale factor < 0.5 is not supported
	}
	else if (iDestSize_x2 == iSrcSize)
	{
		ui32ScaleType = SCALE_TYPE_DOWNSCALE; // scale factor = 0.5
	}
	else if (iDestSize > iSrcSize)
	{
		ui32ScaleType = SCALE_TYPE_UPSCALE; // scale factor > 1.0
	}
	else
	{
		// if none of the above, then scale factor must be > 0.5 and < 1.0
		// in that case we upscale first then use the box filter to downscale.
		ui32ScaleType = SCALE_TYPE_UPDOWNSCALE;
	}

	return ui32ScaleType;
}


static IMG_BOOL FixNeeded(const SGX_QUEUETRANSFER *psQueueTransfer, IMG_UINT32 *pMod, IMG_BOOL bValidate)
{
	IMG_INT32 iBltDestWidth;
	IMG_INT32 iBltDestHeight;
	IMG_INT32 iBltSourceWidth;
	IMG_INT32 iBltSourceHeight;
	IMG_UINT32 ui32ScaleTypeX;
	IMG_UINT32 ui32ScaleTypeY;
	IMG_UINT32 ui32Mod=0;

	// Detect a valid box filter scaling range
	iBltDestWidth = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x1 - SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x0;
	iBltDestHeight = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).y1 - SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).y0;
	iBltSourceWidth = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x1 - SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x0;
	iBltSourceHeight = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).y1 - SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).y0;
	
	ui32ScaleTypeX = GetScaleType(iBltSourceWidth, iBltDestWidth);
	ui32ScaleTypeY = GetScaleType(iBltSourceHeight, iBltDestHeight);
	
	if (ui32ScaleTypeX != ui32ScaleTypeY)
	{
		// The scale type must be the same in X and Y.
		if (bValidate)
		{
			// Indicate that the input params are not compatible with the hardware
			return IMG_TRUE;
		}
		else
		{
			// Indicate no fix is needed (because the blt will not be attempted)
			return IMG_FALSE;
		}
	}
	
	// Detect HW BRN 34939
	switch (ui32ScaleTypeX)
	{
		// Test the two cases where the box filter is used :
		case SCALE_TYPE_DOWNSCALE:
		case SCALE_TYPE_UPDOWNSCALE:
		{
			// The bug occurs when the dst_x_size > 8, and dst_y_size < 8, and dst_x_size mod 8 = 5, 6 or 7.
			if ((iBltDestWidth > 8) && (iBltDestHeight < 8))
			{
				ui32Mod = iBltDestWidth % 8;

				if (ui32Mod>=5 && ui32Mod<=7)
				{
					if (pMod)
					{
						*pMod = ui32Mod; // this is needed for the workaround
					}
					// The workaround is needed
					return IMG_TRUE;
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}

	// No fix needed
	return IMG_FALSE;
}


static PVRSRV_ERROR GeneratePTLAControlStream(const SGX_QUEUETRANSFER *psQueueTransfer,
									  IMG_UINT32 *pui32ControlStream,
									  IMG_UINT32 *pui32Bytes)
{
	PVRSRV_ERROR Ret=PVRSRV_OK;
	IMG_BOOL bFixDownscale=IMG_FALSE;
	IMG_UINT32 ui32Mod=0;

	/* Validate params */
	if(!psQueueTransfer || !psQueueTransfer->ui32NumDest)
	{
		PVR_DPF((PVR_DBG_ERROR, "GeneratePTLAControlStream: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (psQueueTransfer->ui32NumSources == 1)
	{
		bFixDownscale = FixNeeded (psQueueTransfer, &ui32Mod, IMG_FALSE);
	}

#if defined (FIX_HW_BRN_33920)
	if (bFixDownscale && ui32Mod)
	{
		// FIX_HW_BRN_33920 prevents us from issuing a fence
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
#endif//FIX_HW_BRN_33920

	if (bFixDownscale)
	{
		// The fix is : split it into two smaller blts.
		// subtract ui32Mod from the dst width and do that first
		// then do the remainder in a second blt
		SGX_QUEUETRANSFER sQueueTransfer0 = *psQueueTransfer;
		SGX_QUEUETRANSFER sQueueTransfer1 = *psQueueTransfer;
		IMG_UINT32* pAddr = pui32ControlStream;
		IMG_UINT32 ui32Size1;
		IMG_UINT32 ui32Size2;
		IMG_UINT32 ui32DestDelta;
		IMG_UINT32 ui32SrcDelta;
		IMG_INT32 iBltDestWidth;
		IMG_INT32 iBltSourceWidth;

		iBltDestWidth = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x1
							- SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x0;

		iBltSourceWidth = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x1
							- SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x0;

		// subtract ui32Mod from the dst width
		// If there is only one pixel difference between the source and dest widths we won't be able to
		// split it such that there is scaling enabled on both parts, in which case we will return an error.
		// (because the PTLA box filter has one control bit for both x and y)
		// Also, for the case where there is 2 pixels difference we must get both parts roughly the same size.
		// (((width/8)/2)*8) gives a width with the same mod 8 but half the size of the dest.
		ui32DestDelta = ui32Mod + ((iBltDestWidth>>4)<<3);

		// Map the scaled dest size to the source rectangle
		ui32SrcDelta = ((IMG_UINT32)iBltSourceWidth * ui32DestDelta) / (IMG_UINT32)iBltDestWidth;

		// Prepare the first part
		sQueueTransfer0.asDestRects[0].x1 -= ui32DestDelta; // Changing this width will fix the downscale bug
		sQueueTransfer0.asSrcRects[0].x1 -= ui32SrcDelta;

		// Prepare the second part
		sQueueTransfer1.asSrcRects[0].x0 = sQueueTransfer1.asSrcRects[0].x1 - ui32SrcDelta;
		sQueueTransfer1.asDestRects[0].x0 = sQueueTransfer1.asDestRects[0].x1 - ui32DestDelta;

		// Check both parts and use a different fix for the edge cases
		if (FixNeeded(&sQueueTransfer0, NULL, IMG_TRUE) || FixNeeded(&sQueueTransfer1, NULL, IMG_TRUE))
		{
			// subtracting 4 or 5 is also a valid fix because the bug only
			// occurs when dst_x_size mod 8 = 5, 6 or 7
			// (ui32Mod is always >=5 hence ui32DestDelta is always >=5)
			ui32DestDelta = 4 + ((iBltDestWidth>>4)<<3);

			// Map the scaled dest size to the source rectangle
			ui32SrcDelta = ((IMG_UINT32)iBltSourceWidth * ui32DestDelta) / (IMG_UINT32)iBltDestWidth;

			sQueueTransfer0 = *psQueueTransfer;
			sQueueTransfer1 = *psQueueTransfer;

			// Prepare the first part
			sQueueTransfer0.asDestRects[0].x1 -= ui32DestDelta; // Changing this width will fix the downscale bug
			sQueueTransfer0.asSrcRects[0].x1 -= ui32SrcDelta;

			// Prepare the second part
			sQueueTransfer1.asSrcRects[0].x0 = sQueueTransfer1.asSrcRects[0].x1 - ui32SrcDelta;
			sQueueTransfer1.asDestRects[0].x0 = sQueueTransfer1.asDestRects[0].x1 - ui32DestDelta;

			if (FixNeeded(&sQueueTransfer0, NULL, IMG_TRUE) || FixNeeded(&sQueueTransfer1, NULL, IMG_TRUE))
			{
				ui32DestDelta = 5 + ((iBltDestWidth>>4)<<3); // subtract 5

				// Map the scaled dest size to the source rectangle
				ui32SrcDelta = ((IMG_UINT32)iBltSourceWidth * ui32DestDelta) / (IMG_UINT32)iBltDestWidth;

				sQueueTransfer0 = *psQueueTransfer;
				sQueueTransfer1 = *psQueueTransfer;

				// Prepare the first part
				sQueueTransfer0.asDestRects[0].x1 -= ui32DestDelta; // Changing this width will fix the downscale bug
				sQueueTransfer0.asSrcRects[0].x1 -= ui32SrcDelta;

				// Prepare the second part
				sQueueTransfer1.asSrcRects[0].x0 = sQueueTransfer1.asSrcRects[0].x1 - ui32SrcDelta;
				sQueueTransfer1.asDestRects[0].x0 = sQueueTransfer1.asDestRects[0].x1 - ui32DestDelta;

				if (FixNeeded(&sQueueTransfer0, NULL, IMG_TRUE) || FixNeeded(&sQueueTransfer1, NULL, IMG_TRUE))
				{
					// No other workarounds are available, so fail the blt
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
			}
		}

		Ret = GenerateFixedPTLAControlStream(&sQueueTransfer0, pui32ControlStream, &ui32Size1);

		if (Ret == PVRSRV_OK)
		{
			pAddr += ui32Size1;

			// Add a PTLA fence beteewn the two blts
			*pAddr++ = 0x70000000;
			ui32Size1++;

			// Do the second part
			Ret = GenerateFixedPTLAControlStream(&sQueueTransfer1, pAddr , &ui32Size2);

			if (Ret == PVRSRV_OK)
			{
				*pui32Bytes = ui32Size1 + ui32Size2;
			}
		}
	}
	else
	{
		Ret = GenerateFixedPTLAControlStream(psQueueTransfer, pui32ControlStream, pui32Bytes);
	}

	return Ret;
}

static PVRSRV_ERROR GenerateFixedPTLAControlStream(const SGX_QUEUETRANSFER *psQueueTransfer,
									  IMG_UINT32 *pui32ControlStream,
									  IMG_UINT32 *pui32Bytes)
#else//FIX_HW_BRN_34939

static PVRSRV_ERROR GeneratePTLAControlStream(const SGX_QUEUETRANSFER *psQueueTransfer,
									  IMG_UINT32 *pui32ControlStream,
									  IMG_UINT32 *pui32Bytes)

#endif//FIX_HW_BRN_34939

{
	IMG_UINT32 ui32CmdStreamSize=0;
	IMG_UINT32 ui32BltControl=0;
	IMG_UINT32 ui32SourceFormat;
	IMG_UINT32 ui32SourceSurfaceLayout=0;
	IMG_UINT32 ui32DestFormat;
	IMG_UINT32 ui32DestSurfaceLayout=0;
	PVRSRV_ERROR Ret;
	IMG_BOOL bRotated;
	IMG_BOOL bBackwards;
	IMG_BOOL bDownScaleX=IMG_FALSE;
	IMG_BOOL bDownScaleY=IMG_FALSE;
	IMG_BOOL bUpScaleX=IMG_FALSE;
	IMG_BOOL bUpScaleY=IMG_FALSE;
	IMG_BOOL bUpAndDownScaleX=IMG_FALSE;
	IMG_BOOL bUpAndDownScaleY=IMG_FALSE;
	IMG_INT32 i32XStart;
	IMG_INT32 i32YStart;
	IMG_INT32 iBltDestWidth;
	IMG_INT32 iBltDestHeight;
	IMG_INT32 iTmp;
	IMG_UINT32 ui32NoOfSrcPlanes;
	IMG_UINT32 ui32NoOfDstPlanes;
	IMG_UINT32 uSrcRectWidth, uSrcRectHeight;
	const SGXTQ_BLITOP *pBlt = &psQueueTransfer->Details.sBlit;
	IMG_UINT32 ui32SrcGroup;
	IMG_UINT32 ui32DstGroup;
	
	PVRSRVMemSet(pui32ControlStream,0,sizeof(*pui32ControlStream));
	
	/* Validate params */
	if(!psQueueTransfer)
	{
		PVR_DPF((PVR_DBG_ERROR, "GeneratePTLAControlStream: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	// PTLA does not support TQ flags
	if ((psQueueTransfer->ui32Flags & (SGX_TRANSFER_FLAGS_DITHER
				| SGX_TRANSFER_FLAGS_INVERTX
				| SGX_TRANSFER_FLAGS_INVERTY
				| SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE
				| SGX_TRANSFER_LAYER_0_ROT_90
				| SGX_TRANSFER_LAYER_0_ROT_180
				| SGX_TRANSFER_LAYER_0_ROT_270))!=0)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	// PTLA can't do alpha blend
	if (psQueueTransfer->Details.sBlit.eAlpha != SGXTQ_ALPHA_NONE)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	Ret = GetPTLAFormat(psQueueTransfer->asDests[0].eFormat,//pBltInfo->DstFormat,
						psQueueTransfer->asDests[0].eMemLayout,
						&ui32DestFormat, 
						&ui32DestSurfaceLayout,
						&ui32NoOfDstPlanes, &ui32DstGroup);
	
	if (Ret != PVRSRV_OK)
	{
		return Ret;
	}
	
	bRotated = (pBlt->eRotation == SGXTQ_ROTATION_NONE) ? IMG_FALSE : IMG_TRUE;
	iBltDestWidth = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x1 - SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x0;
	iBltDestHeight = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).y1 - SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).y0;
	
	// Caller may specify the copy order		
	bBackwards = (pBlt->eCopyOrder >= SGXTQ_COPYORDER_TR2BL) ? IMG_TRUE : IMG_FALSE;
	
	if (ui32DestSurfaceLayout == EURASIA_PTLA_SURF_LAYOUT_TWIDDLED)
	{
		if (!IsPo2(iBltDestWidth) || !IsPo2(iBltDestHeight)
			|| SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x0 > 0
			|| SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).y0 > 0	)
		{
			// PTLA can only twiddle when width and height are both a power of 2
			// Subrectangle twiddling is also not supported
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}
	else if (ui32DestSurfaceLayout == EURASIA_PTLA_SURF_LAYOUT_TILED)
	{
		// PTLA tiling only works for rectangles that are a multiple of the texture tile size (32)
		if ((iBltDestWidth&0x1F) || (iBltDestHeight&0x1F))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
		
		// PTLA tiling offsets must be zero
		if (		(SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x0 > 0)
				||	(SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).y0 > 0) )
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

	/* Source copy or colour fill */
	if (pBlt->byCustomRop3 == 0xCC || pBlt->byCustomRop3 == 0)
	{
		Ret = GetPTLAFormat(psQueueTransfer->asSources[0].eFormat,//pBltInfo->DstFormat,
							psQueueTransfer->asSources[0].eMemLayout,
							&ui32SourceFormat, 
							&ui32SourceSurfaceLayout, 
							&ui32NoOfSrcPlanes, &ui32SrcGroup);
		
		if (Ret != PVRSRV_OK)
		{
			return Ret;
		}
		
		// Check format conversion requests
		if (ui32SourceFormat != ui32DestFormat)
		{
			if (ui32SrcGroup != ui32DstGroup)
			{
				// Format conversion must be within the same group
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			
			if (ui32SrcGroup == PTLA_FORMAT_GROUP_MONO || ui32SrcGroup == PTLA_FORMAT_GROUP_RAW)
			{
				// No format conversion support within mono or raw groups
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			
			// Check the yuv group
			if (ui32SrcGroup == PTLA_FORMAT_GROUP_YUV)
			{
				if (ui32NoOfDstPlanes != ui32NoOfSrcPlanes)
				{
					// No of planes must be the same for source and dest
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
			}
		}

		// source rectangle size
		uSrcRectWidth = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x1 - SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x0;
		uSrcRectHeight = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).y1 - SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).y0;

		if (ui32SourceSurfaceLayout == EURASIA_PTLA_SURF_LAYOUT_TWIDDLED)
		{
			if (	!IsPo2(uSrcRectWidth) || !IsPo2(uSrcRectHeight)
					|| SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x0 > 0
					|| SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).y0 > 0	)
			{
				// PTLA can only untwiddle when width and height are both a power of 2
				// Subrectangle untwiddling is also not supported
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			// Twiddled->Tiled copy is not supported
			if (ui32DestSurfaceLayout == EURASIA_PTLA_SURF_LAYOUT_TILED)
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
		}
		else if (ui32SourceSurfaceLayout == EURASIA_PTLA_SURF_LAYOUT_TILED)
		{
			// PTLA untiling only works for rectangles that are a multiple of the texture tiling size (32)
			if ((uSrcRectWidth&0x1F) || (uSrcRectHeight&0x1F))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			// PTLA tiling offsets must be zero
			if (		(SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x0 > 0)
					||	(SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).y0 > 0) )
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			
			// Tiled->Twiddled copy is not supported
			if (ui32DestSurfaceLayout == EURASIA_PTLA_SURF_LAYOUT_TWIDDLED)
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
		}

		// Source offset
		i32XStart = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x0;
		i32YStart = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).y0;

		// Check for rotation and copy order and adjust source offset accordingly
		if (bRotated)
		{
			if (psQueueTransfer->Details.sBlit.eRotation == SGXTQ_ROTATION_90)
			{
				ui32BltControl |= EURASIA_PTLA_ROT_90DEGS;
				i32YStart = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).y1 - 1;

				// swap aspect
				iTmp = iBltDestWidth;
				iBltDestWidth = iBltDestHeight;
				iBltDestHeight = iTmp;
			}
			else if (psQueueTransfer->Details.sBlit.eRotation == SGXTQ_ROTATION_180)
			{
				ui32BltControl |= EURASIA_PTLA_ROT_180DEGS;
				i32YStart = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).y1 -1;
				i32XStart = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x1 -1;
			}
			else
			{
				ui32BltControl |= EURASIA_PTLA_ROT_270DEGS;
				i32XStart = SGX_QUEUETRANSFER_SRC_RECT((*psQueueTransfer), 0).x1 -1;

				// swap aspect
				iTmp = iBltDestWidth;
				iBltDestWidth = iBltDestHeight;
				iBltDestHeight = iTmp;
			}
		}
		else if (bBackwards)
		{
			if (pBlt->eCopyOrder == SGXTQ_COPYORDER_BR2TL)
			{
				ui32BltControl |= EURASIA_PTLA_COPYORDER_BR2TL;
				i32YStart += uSrcRectHeight -1;
				i32XStart += uSrcRectWidth -1;
			}
			else if (pBlt->eCopyOrder == SGXTQ_COPYORDER_TR2BL)
			{
				ui32BltControl |= EURASIA_PTLA_COPYORDER_TR2BL;
				i32XStart += uSrcRectWidth -1;
			}
			else
			{
				ui32BltControl |= EURASIA_PTLA_COPYORDER_BL2TR;
				i32YStart += uSrcRectHeight -1;
			}
		}
		
		// Detect scale down in X
		if (iBltDestWidth < (IMG_INT32)uSrcRectWidth)
		{
			if (((IMG_INT32)uSrcRectWidth+1)>>1 > iBltDestWidth)
			{
				// Can't scale down by more than 0.5
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			bDownScaleX = IMG_TRUE;
		}
		// Detect scale up in X
		else if (iBltDestWidth > (IMG_INT32)uSrcRectWidth)
		{
			bUpScaleX = IMG_TRUE;
		}
		
		// Detect scale down in Y
		if (iBltDestHeight < (IMG_INT32)uSrcRectHeight)
		{
			if (((IMG_INT32)uSrcRectHeight+1)>>1 > iBltDestHeight)
			{
				// Can't scale down by more than 0.5
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			
			bDownScaleY = IMG_TRUE;
		}
		// Detect scale up in Y
		else if (iBltDestHeight > (IMG_INT32)uSrcRectHeight)
		{
			bUpScaleY = IMG_TRUE;
		}
		
		// Enable PTLA downscale
		if (bDownScaleX || bDownScaleY)
		{
			// Box filter downscale is enabled for both X and Y
			if (bDownScaleX && bDownScaleY)
			{
				// Set control bit for downscale
				ui32BltControl |= EURASIA_PTLA_DOWNSCALE_ENABLE;

				// Handle the case where scale factor is between 0.5 and 1.0
				// Box filter can only downscale by 0.5 but PTLA can also upscale at the same time to
				// accommodate any downscale scale factor between 0.5 and 1.0
				if (iBltDestWidth > (IMG_INT32)uSrcRectWidth>>1)
				{
					bUpAndDownScaleX = IMG_TRUE;
				}
				if (iBltDestHeight > (IMG_INT32)uSrcRectHeight>>1)
				{
					bUpAndDownScaleY = IMG_TRUE;
				}

				if (bUpAndDownScaleX || bUpAndDownScaleY)
				{
					if (bUpAndDownScaleX && bUpAndDownScaleY)
					{
						// Enable upscale as well as downscale
						ui32BltControl |= EURASIA_PTLA_UPSCALE_ENABLE;
					}
					else
					{
						// Upscale has to be enabled for both X and Y
						return PVRSRV_ERROR_INVALID_PARAMS;
					}
				}
			}
			else
			{
				// Can't scale just x or just y
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
		}

		// Enable PTLA upscale
		if (bUpScaleX || bUpScaleY)
		{
			// Upscale is for both X and Y
			if (bUpScaleX && bUpScaleY)
			{
				// Set control bit for upscale
				ui32BltControl |= EURASIA_PTLA_UPSCALE_ENABLE;
			}
			else
			{
				// Can't scale just x or just y
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
		}

		// Trap illegal combinations of features.
		if (bUpScaleX || bDownScaleX || bUpAndDownScaleX)
		{
			// PTLA scaling only works if certain other features are disabled
			if ( bRotated || bBackwards
					|| (ui32SourceSurfaceLayout != EURASIA_PTLA_SURF_LAYOUT_STRIDED)
					|| (ui32DestSurfaceLayout != EURASIA_PTLA_SURF_LAYOUT_STRIDED)
					|| (psQueueTransfer->Details.sBlit.eColourKey != SGXTQ_COLOURKEY_NONE)	)
			{
					return PVRSRV_ERROR_INVALID_PARAMS;
			}
		}

		// For twiddled to twiddled PTLA copy we must do a strided copy
		// it gives the same result and also is a hardware requirement
		if ((ui32DestSurfaceLayout==EURASIA_PTLA_SURF_LAYOUT_TWIDDLED)
			 && (ui32SourceSurfaceLayout == EURASIA_PTLA_SURF_LAYOUT_TWIDDLED))
		{
			ui32DestSurfaceLayout = EURASIA_PTLA_SURF_LAYOUT_STRIDED;
			ui32SourceSurfaceLayout = EURASIA_PTLA_SURF_LAYOUT_STRIDED;
		}

		// PTLA Source surface command : header with stride and format
		pui32ControlStream[ui32CmdStreamSize++] = EURASIA_PTLA_SRC_SURF_BH
			 | ((ui32SourceSurfaceLayout << EURASIA_PTLA_SURF_LAYOUT_SHIFT) & EURASIA_PTLA_SURF_LAYOUT_MASK)
			 | ((ui32SourceFormat << EURASIA_PTLA_SURF_FORMAT_SHIFT) & EURASIA_PTLA_SURF_FORMAT_MASK)
			 | (psQueueTransfer->asSources[0].i32StrideInBytes & EURASIA_PTLA_SURF_STRIDE_MASK);
		
		// base address
		pui32ControlStream[ui32CmdStreamSize++] = SGX_TQSURFACE_GET_DEV_VADDR(psQueueTransfer->asSources[0]);

		if (ui32NoOfSrcPlanes >= 2)
		{
			// Source Surface Word 2 (YUV TWO PLANER)
			pui32ControlStream[ui32CmdStreamSize++] = SGX_TQSURFACE_GET_DEV_VADDR(psQueueTransfer->asSources[0])
					+ psQueueTransfer->asSources[0].ui32ChromaPlaneOffset[0];
		}

		if (ui32NoOfSrcPlanes == 3)
		{
			// Source Surface Word 3 (YUV THREE PLANER)
			pui32ControlStream[ui32CmdStreamSize++] = SGX_TQSURFACE_GET_DEV_VADDR(psQueueTransfer->asSources[0])
					+ psQueueTransfer->asSources[0].ui32ChromaPlaneOffset[1];
		}

		// PTLA Source offset command : header with x and y offset
		pui32ControlStream[ui32CmdStreamSize++] = EURASIA_PTLA_SRCOFF_BH | (i32YStart & EURASIA_PTLA_SRCOFF_YSTART_MASK)
			| ((i32XStart << EURASIA_PTLA_SRCOFF_XSTART_SHIFT) & EURASIA_PTLA_SRCOFF_XSTART_MASK);
		
		pui32ControlStream[ui32CmdStreamSize++] = (uSrcRectHeight & EURASIA_PTLA_SRCOFF_YSIZE_MASK)
			| ((uSrcRectWidth << EURASIA_PTLA_SRCOFF_XSIZE_SHIFT) & EURASIA_PTLA_SRCOFF_XSIZE_MASK);
		
		// EURASIA_PTLA_SRCOFF_YUV2_BH - Two Planer YUV Source Offset
		if (ui32NoOfSrcPlanes >= 2)
		{
			// PTLA Plane2 Source offset command : header with x and y offset
			pui32ControlStream[ui32CmdStreamSize++] = EURASIA_PTLA_SRCOFF_YUV2_BH | (i32YStart & EURASIA_PTLA_SRCOFF_YSTART_MASK)
				| ((i32XStart << EURASIA_PTLA_SRCOFF_XSTART_SHIFT) & EURASIA_PTLA_SRCOFF_XSTART_MASK);
			
			// source rectangle size
			pui32ControlStream[ui32CmdStreamSize++] = (uSrcRectHeight & EURASIA_PTLA_SRCOFF_YSIZE_MASK)
				| ((uSrcRectWidth << EURASIA_PTLA_SRCOFF_XSIZE_SHIFT) & EURASIA_PTLA_SRCOFF_XSIZE_MASK);

			// EURASIA_PTLA_SRCOFF_YUV3_BH - Three Planer YUV Source Offset
			if (ui32NoOfSrcPlanes == 3)
			{
				// PTLA Plane3 Source offset command : header with x and y offset
				pui32ControlStream[ui32CmdStreamSize++] = EURASIA_PTLA_SRCOFF_YUV3_BH | (i32YStart & EURASIA_PTLA_SRCOFF_YSTART_MASK)
					| ((i32XStart << EURASIA_PTLA_SRCOFF_XSTART_SHIFT) & EURASIA_PTLA_SRCOFF_XSTART_MASK);
				
				// source rectangle size
				pui32ControlStream[ui32CmdStreamSize++] = (uSrcRectHeight & EURASIA_PTLA_SRCOFF_YSIZE_MASK)
					| ((uSrcRectWidth << EURASIA_PTLA_SRCOFF_XSIZE_SHIFT) & EURASIA_PTLA_SRCOFF_XSIZE_MASK);
			}
		}

		// Colour Key
		if (psQueueTransfer->Details.sBlit.eColourKey == SGXTQ_COLOURKEY_SOURCE)
		{
			// PTLA Colour key control command
			// pBltInfo->ColourKey
			pui32ControlStream[ui32CmdStreamSize++] = EURASIA_PTLA_CTRL_BH | EURASIA_PTLA_CTRL_SRC_CKEY;
			pui32ControlStream[ui32CmdStreamSize++] = psQueueTransfer->Details.sBlit.ui32ColourKey; // Colour Key colour (source)

			// Colour key mask
			if (psQueueTransfer->Details.sBlit.ui32ColourKeyMask)
			{
				// Colour key mask specified by caller
				pui32ControlStream[ui32CmdStreamSize++] = psQueueTransfer->Details.sBlit.ui32ColourKeyMask;
			}
			else
			{
				// Unspecified mask, so use default for source format
				pui32ControlStream[ui32CmdStreamSize++] = GetColKeyMask(psQueueTransfer->asSources[0].eFormat); // Colour key mask
			}
		}
		
		 // The PTLA downscale box filter can be used with upscale to improve range of scale factors
		if (bUpAndDownScaleX)
		{
			IMG_UINT32 ui32XUpscale;
			IMG_UINT32 ui32YUpscale;
			
			// PTLA Scaling control command
			pui32ControlStream[ui32CmdStreamSize++] = EURASIA_PTLA_CTRL_BH | EURASIA_PTLA_CTRL_UPSCALE;

			// Calculate bilinear scaling alpha
			ui32XUpscale = (IMG_UINT32) GetUpscaleAlpha(iBltDestWidth<<1, uSrcRectWidth);
			ui32YUpscale = (IMG_UINT32) GetUpscaleAlpha(iBltDestHeight<<1, uSrcRectHeight);
			
			pui32ControlStream[ui32CmdStreamSize++] = (ui32YUpscale & EURASIA_PTLA_CTRL_UPSCALE_ALPHA_Y_MASK) |
					((ui32XUpscale << EURASIA_PTLA_CTRL_UPSCALE_ALPHA_X_SHIFT) & EURASIA_PTLA_CTRL_UPSCALE_ALPHA_X_MASK);
		}
		else if (bUpScaleX) // Just upscale
		{
			IMG_UINT32 ui32XUpscale;
			IMG_UINT32 ui32YUpscale;
			
			// PTLA Scaling control command
			pui32ControlStream[ui32CmdStreamSize++] = EURASIA_PTLA_CTRL_BH | EURASIA_PTLA_CTRL_UPSCALE;
			
			// Bilinear upscale is calculated with the equation P = A + a(B-A).
			// The alpha fields are calculated by the equation scale_factor = 1/(1+scale_factor)
			// (in x and y) where scale_factor is a 4.6 number
			ui32XUpscale = (IMG_UINT32) GetUpscaleAlpha(iBltDestWidth, uSrcRectWidth);
			ui32YUpscale = (IMG_UINT32) GetUpscaleAlpha(iBltDestHeight, uSrcRectHeight);
			
			pui32ControlStream[ui32CmdStreamSize++] = (ui32YUpscale & EURASIA_PTLA_CTRL_UPSCALE_ALPHA_Y_MASK) |
					((ui32XUpscale << EURASIA_PTLA_CTRL_UPSCALE_ALPHA_X_SHIFT) & EURASIA_PTLA_CTRL_UPSCALE_ALPHA_X_MASK);
		}
	}
	else if (pBlt->byCustomRop3 == 0xF0) // colour fill
	{
		ui32BltControl |= EURASIA_PTLA_FILL_ENABLE;
	}
	else
	{
		return PVRSRV_ERROR_INVALID_PARAMS; // no other rop codes are supported by the PTLA, just source copy and colour fill.
	}
	
	// Dest surface : base address, format, and stride
	
	// PTLA Dest surface command : header with stride and format
	pui32ControlStream[ui32CmdStreamSize++] = EURASIA_PTLA_DST_SURF_BH
		 | ((ui32DestSurfaceLayout << EURASIA_PTLA_SURF_LAYOUT_SHIFT) & EURASIA_PTLA_SURF_LAYOUT_MASK)
		 | ((ui32DestFormat << EURASIA_PTLA_SURF_FORMAT_SHIFT) & EURASIA_PTLA_SURF_FORMAT_MASK)
		 | (psQueueTransfer->asDests[0].i32StrideInBytes & EURASIA_PTLA_SURF_STRIDE_MASK);
	
	// Dest surface base address
	pui32ControlStream[ui32CmdStreamSize++] = SGX_TQSURFACE_GET_DEV_VADDR(psQueueTransfer->asDests[0]);

	// Two and three planer YUV destination base addresses
	if (ui32NoOfDstPlanes >= 2)
	{
		// Destination Surface Word 2 (YUV TWO PLANER second plane base address)
		pui32ControlStream[ui32CmdStreamSize++] = SGX_TQSURFACE_GET_DEV_VADDR(psQueueTransfer->asDests[0])
				+ psQueueTransfer->asDests[0].ui32ChromaPlaneOffset[0];
		
		if (ui32NoOfDstPlanes == 3)
		{
				// Destination Surface Word 3 (YUV THREE PLANER third plane base address)
			pui32ControlStream[ui32CmdStreamSize++] = SGX_TQSURFACE_GET_DEV_VADDR(psQueueTransfer->asDests[0])
					+ psQueueTransfer->asDests[0].ui32ChromaPlaneOffset[1];
		}
	}

	// Colour key command
	// PTLA supports two kinds of source key op : pass and reject
	// pass copies the source when the source colour matches the key colour.
	// reject does the opposite, it copies source when colour does not match.
	if (psQueueTransfer->Details.sBlit.eColourKey == SGXTQ_COLOURKEY_SOURCE)
	{
		ui32BltControl |= EURASIA_PTLA_SRCCK_ENABLE; // source reject (eg non rectangular blts)
	}
	else if (psQueueTransfer->Details.sBlit.eColourKey == SGXTQ_COLOURKEY_SOURCE_PASS)
	{
		ui32BltControl |= EURASIA_PTLA_SRCCK_ENABLE | EURASIA_PTLA_SRCCK_PASS_ENABLE; // source pass (eg text)
	}
	else if (psQueueTransfer->Details.sBlit.eColourKey == SGXTQ_COLOURKEY_DEST)
	{
		// PTLA does not support destination colour key, only source.
		PVR_DPF((PVR_DBG_ERROR, "GeneratePTLAControlStream: SGXTQ_COLOURKEY_DEST unsupported"));
		return PVRSRV_ERROR_INVALID_PARAMS;		
	}
	
	// EURASIA_PTLA_BLIT_BH - Blt Start command includes downscale, upscale, colourkey, rotation and copy order
	pui32ControlStream[ui32CmdStreamSize++] = ui32BltControl | EURASIA_PTLA_BLIT_BH;
	
	// BLT Control Word 1 - Fill Colour
	if (ui32BltControl & EURASIA_PTLA_FILL_ENABLE)
	{
		// BLT Control Word 1 (Fill Colour)
		pui32ControlStream[ui32CmdStreamSize++] = psQueueTransfer->Details.sBlit.ui32ColourKey; // until TQAPI is reworked
	}

	// Dest rect offset
	i32XStart = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x0;
	i32YStart = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).y0;

	// The PTLA dest offset must take into account the copy order (if not TL2BR)
	if (bBackwards)
	{
		if (pBlt->eCopyOrder == SGXTQ_COPYORDER_BR2TL)
		{
			i32YStart += iBltDestHeight -1;
			i32XStart += iBltDestWidth -1;
		}
		else if (pBlt->eCopyOrder == SGXTQ_COPYORDER_TR2BL)
		{
			i32XStart += iBltDestWidth -1;
		}
		else
		{
			i32YStart += iBltDestHeight -1; // BL2TR
		}
	}
	
	// BLT Control Word 2 - Dest X and Y start
	pui32ControlStream[ui32CmdStreamSize++] = (i32YStart & EURASIA_PTLA_DST_YSTART_MASK) 
		| ((i32XStart << EURASIA_PTLA_DST_XSTART_SHIFT) & EURASIA_PTLA_DST_XSTART_MASK);
	
	// BLT Control Word 3 (YUV TWO PLANER X and Y start)
	if (ui32NoOfDstPlanes >= 2)
	{
		pui32ControlStream[ui32CmdStreamSize++] = (i32YStart & EURASIA_PTLA_DST_YSTART_MASK) 
			| ((i32XStart << EURASIA_PTLA_DST_XSTART_SHIFT) & EURASIA_PTLA_DST_XSTART_MASK);
	}
	
	// BLT Control Word 4 (YUV THREE PLANER X and Y start)
	if (ui32NoOfDstPlanes == 3)
	{
		pui32ControlStream[ui32CmdStreamSize++] = (i32YStart & EURASIA_PTLA_DST_YSTART_MASK) 
			| ((i32XStart << EURASIA_PTLA_DST_XSTART_SHIFT) & EURASIA_PTLA_DST_XSTART_MASK);
	}
	
	// BLT Control Word 5 - X and Y blt size
	pui32ControlStream[ui32CmdStreamSize++] = (iBltDestHeight & EURASIA_PTLA_DST_YSIZE_MASK) 
		| ((iBltDestWidth << EURASIA_PTLA_DST_XSIZE_SHIFT) & EURASIA_PTLA_DST_XSIZE_MASK);
	
	*pui32Bytes = ui32CmdStreamSize;
	
	return PVRSRV_OK;
}


static PVRSRV_ERROR GeneratePTLAControlStream_Fill (const SGX_QUEUETRANSFER *psQueueTransfer,
									IMG_UINT32 *pui32ControlStream, IMG_UINT32	*pui32Bytes)
{
	const SGXTQ_FILLOP *pFill = &psQueueTransfer->Details.sFill;
	IMG_UINT32 ui32CmdStreamSize=0;
	IMG_UINT32 ui32BltControl=EURASIA_PTLA_FILL_ENABLE;
	IMG_UINT32 ui32DestFormat;
	IMG_UINT32 ui32DestSurfaceLayout;
	IMG_INT32 i32XStart;
	IMG_INT32 i32YStart;
	IMG_INT32 iBltDestWidth;
	IMG_INT32 iBltDestHeight;
	IMG_UINT32 ui32NoOfDstPlanes;
	IMG_UINT32 ui32DstGroup;
	PVRSRV_ERROR Ret;

	PVRSRVMemSet(pui32ControlStream,0,sizeof(*pui32ControlStream));
	
	/* Validate params */
	if(!psQueueTransfer)
	{
		PVR_DPF((PVR_DBG_ERROR, "GeneratePTLAControlStream: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	Ret = GetPTLAFormat(psQueueTransfer->asDests[0].eFormat,//pBltInfo->DstFormat,
						psQueueTransfer->asDests[0].eMemLayout,
						&ui32DestFormat, 
						&ui32DestSurfaceLayout,
						&ui32NoOfDstPlanes, &ui32DstGroup);
	
	if (Ret != PVRSRV_OK)
	{
		return Ret;
	}

	// Two and three planer YUV is not supported for PTLA colour fill, only single planar formats
	if (ui32NoOfDstPlanes >= 2)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (ui32DestSurfaceLayout != EURASIA_PTLA_SURF_LAYOUT_STRIDED)
	{
		// Colour fill is only supported in the hardware for strided surfaces.
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	
	// PTLA Dest surface command : header with stride and format
	pui32ControlStream[ui32CmdStreamSize++] = EURASIA_PTLA_DST_SURF_BH
		 | ((ui32DestFormat << EURASIA_PTLA_SURF_FORMAT_SHIFT) & EURASIA_PTLA_SURF_FORMAT_MASK)
		 | (psQueueTransfer->asDests[0].i32StrideInBytes & EURASIA_PTLA_SURF_STRIDE_MASK);
	
	// Dest surface base address
	pui32ControlStream[ui32CmdStreamSize++] = SGX_TQSURFACE_GET_DEV_VADDR(psQueueTransfer->asDests[0]);
	
	// h/w nif : we need to send a dummy source surface command (strided) even though there is no source surface.
	pui32ControlStream[ui32CmdStreamSize++] = EURASIA_PTLA_SRC_SURF_BH
		 | ((ui32DestFormat << EURASIA_PTLA_SURF_FORMAT_SHIFT) & EURASIA_PTLA_SURF_FORMAT_MASK)
		 | (psQueueTransfer->asDests[0].i32StrideInBytes & EURASIA_PTLA_SURF_STRIDE_MASK);
	pui32ControlStream[ui32CmdStreamSize++] = SGX_TQSURFACE_GET_DEV_VADDR(psQueueTransfer->asDests[0]);

	// EURASIA_PTLA_BLIT_BH - Blt Start command includes downscale, upscale, colourkey, rotation and copy order
	pui32ControlStream[ui32CmdStreamSize++] = ui32BltControl | EURASIA_PTLA_BLIT_BH;
	
	// BLT Control Word 1 - Fill Colour
	if (ui32BltControl & EURASIA_PTLA_FILL_ENABLE)
	{
		// BLT Control Word 1 (Fill Colour)
		pui32ControlStream[ui32CmdStreamSize++] = pFill->ui32Colour;
	}

	// Dest rect
	i32XStart = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x0;
	i32YStart = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).y0;

	iBltDestWidth = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).x1 - i32XStart;
	iBltDestHeight = SGX_QUEUETRANSFER_DST_RECT((*psQueueTransfer), 0).y1 - i32YStart;

	// BLT Control Word 2 - Dest X and Y start
	pui32ControlStream[ui32CmdStreamSize++] = (i32YStart & EURASIA_PTLA_DST_YSTART_MASK) 
		| ((i32XStart << EURASIA_PTLA_DST_XSTART_SHIFT) & EURASIA_PTLA_DST_XSTART_MASK);
	
	// BLT Control Word 3 - X and Y blt size
	pui32ControlStream[ui32CmdStreamSize++] = (iBltDestHeight & EURASIA_PTLA_DST_YSIZE_MASK) 
		| ((iBltDestWidth << EURASIA_PTLA_DST_XSIZE_SHIFT) & EURASIA_PTLA_DST_XSIZE_MASK);
	
	*pui32Bytes = ui32CmdStreamSize;
	
	return PVRSRV_OK;
}




// Default colour key masks
static IMG_UINT32 GetColKeyMask(const PVRSRV_PIXEL_FORMAT Format)
{
	switch(Format)
	{
		default : return 0x00FFFFFF;

		case PVRSRV_PIXEL_FORMAT_RGB565 : return 0x00F8FCF8;
		case PVRSRV_PIXEL_FORMAT_ARGB1555 : return 0x00F8F8F8;
		case PVRSRV_PIXEL_FORMAT_ARGB4444 : return 0x00F0F0F0;
		case PVRSRV_PIXEL_FORMAT_A2B10G10R10F : return 0x3FFFFFFF;

		case PVRSRV_PIXEL_FORMAT_YUYV :
		case PVRSRV_PIXEL_FORMAT_YVYU :
		case PVRSRV_PIXEL_FORMAT_UYVY :
		case PVRSRV_PIXEL_FORMAT_VYUY :
			return 0xFFFFFFFF;
	}
}

static PVRSRV_ERROR GetPTLAFormat(const PVRSRV_PIXEL_FORMAT eFormat,
									const SGXTQ_MEMLAYOUT eMemLayout,
									IMG_PUINT32 pPTLAFormat,
									IMG_PUINT32 pPTLASurfaceLayout,
									IMG_PUINT32 pNoOfPlanes,
									IMG_PUINT32 pGroup)
{
	/* set default number of planes */
	*pNoOfPlanes = 1;
	*pGroup = PTLA_FORMAT_GROUP_RGB;

	switch(eMemLayout)
	{
		case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
		case SGXTQ_MEMLAYOUT_2D:
		{
			*pPTLASurfaceLayout = EURASIA_PTLA_SURF_LAYOUT_TWIDDLED;
			break;	
		}
		case SGXTQ_MEMLAYOUT_OUT_LINEAR:
		case SGXTQ_MEMLAYOUT_STRIDE:
		{
			*pPTLASurfaceLayout = EURASIA_PTLA_SURF_LAYOUT_STRIDED;
			break;	
		}
		case SGXTQ_MEMLAYOUT_OUT_TILED:
		case SGXTQ_MEMLAYOUT_TILED:
		{
			*pPTLASurfaceLayout = EURASIA_PTLA_SURF_LAYOUT_TILED;
			break;	
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "GetPTLAFormat: Invalid layout"));
			return PVRSRV_ERROR_INVALID_PARAMS;			
		}
	}

	switch (eFormat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_565RGB;
			break;
		}
		
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_8888ARGB;
			break;
		}
		
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_1555ARGB;
			break;
		}
		
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_4444ARGB;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_RGB888:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_888RGB;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_A2RGB10:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_2101010ARGB;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_YUYV:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_YUV422_YUYV;
			*pGroup = PTLA_FORMAT_GROUP_YUV;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_UYVY:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_YUV422_UYVY;
			*pGroup = PTLA_FORMAT_GROUP_YUV;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_YVYU:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_YUV422_YVYU;
			*pGroup = PTLA_FORMAT_GROUP_YUV;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_VYUY:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_YUV422_VYUY;
			*pGroup = PTLA_FORMAT_GROUP_YUV;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_NV12:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_YUV420_2PLANE;
			*pNoOfPlanes = 2;
			*pGroup = PTLA_FORMAT_GROUP_YUV;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_YV12:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_YUV420_3PLANE;
			*pNoOfPlanes = 3;
			*pGroup = PTLA_FORMAT_GROUP_YUV;
			break;
		}
		
		case PVRSRV_PIXEL_FORMAT_MONO8:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_U8;
			*pGroup = PTLA_FORMAT_GROUP_MONO;
			break;
		}
		
		case PVRSRV_PIXEL_FORMAT_G8R8_UINT:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_U88;
			*pGroup = PTLA_FORMAT_GROUP_MONO;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_R8_SINT:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_S8;
			*pGroup = PTLA_FORMAT_GROUP_MONO;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_B8G8R8_SINT:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_888RSGSBS;
			break;
		}
		
		case PVRSRV_PIXEL_FORMAT_R16:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_16BPP_RAW;
			*pGroup = PTLA_FORMAT_GROUP_RAW;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_X8R8G8B8:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_32BPP_RAW;
			*pGroup = PTLA_FORMAT_GROUP_RAW;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_A16B16G16R16:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_64BPP_RAW;
			*pGroup = PTLA_FORMAT_GROUP_RAW;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_A32B32G32R32:
		{
			*pPTLAFormat = EURASIA_PTLA_FORMAT_128BPP_RAW;
			*pGroup = PTLA_FORMAT_GROUP_RAW;
			break;
		}

		default :
		{
			PVR_DPF((PVR_DBG_ERROR, "GetPTLAFormat: Invalid format"));
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}
	
	return PVRSRV_OK;
}



/*
	Upscaling alpha calculation, using integer arithmatic :

	Inputs :
	SrcSize = source width in pixels
	DestSize = dest width in pixels

*/
static IMG_INT32 GetUpscaleAlpha(const IMG_INT32 DestSize, const IMG_INT32 SrcSize)
{
	IMG_UINT32 ret;

	if (!DestSize) return 0; // prevent divide by zero

	// Alpha is 1/scale factor with fixed point offset
	// Scale factor is output/input = DestSize/SrcSize
	ret = (SrcSize * 1024) / DestSize;
	return ret;
}

/*
	IsPo2 returns IMG_TRUE if the input is a power of 2
*/
static IMG_BOOL IsPo2 (const IMG_UINT32 v)
{
	IMG_UINT32 t;

	t = v && !(v & (v - 1));

	return (t) ? IMG_TRUE : IMG_FALSE;
}

#endif /*PTLA*/

/******************************************************************************
 End of file (sgxtransfer_2d.c)
 ******************************************************************************/
