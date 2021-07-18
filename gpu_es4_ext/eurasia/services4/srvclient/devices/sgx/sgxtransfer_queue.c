/*!****************************************************************************
@File           sgxtransfer_queue.c

@Title          Device specific transfer queue routines

@Author         Imagination Technologies

@date           08/02/06

@Copyright      Copyright 2007-2010 by Imagination Technologies Limited.
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
$Log: sgxtransfer_queue.c $
******************************************************************************/

#if defined(TRANSFER_QUEUE)
#include <stddef.h>

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
#include "pdump_um.h"

#include "transferqueue_use_labels.h"
#include "subtwiddled_eot_labels.h"

#include "../../common/blitlib.h"
#include "../../common/blitlib_src.h"
#include "../../common/blitlib_dst.h"
#include "../../common/blitlib_op.h"


#if !defined(FIX_HW_BRN_27298)

#if defined(DEBUG)
static IMG_UINT32 gui32SrcAddr[5];
static IMG_UINT32 gui32DstAddr[5];
#endif

#endif

/* Prepare functions */
#if defined(SGX_FEATURE_2D_HARDWARE)
IMG_INTERNAL PVRSRV_ERROR Prepare2DCoreBlit(SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext,
									  SGX_QUEUETRANSFER *psQueueTransfer,
									  SGXMKIF_2DCMD_SUBMIT *ps2DSubmit)
{
	SGX_CLIENT_CCB *psCCB = psTQContext->ps2DCCB;
	PPVRSRV_CLIENT_SYNC_INFO psSyncInfo;
	PSGXMKIF_2DCMD ps2DCmd;
	PVRSRV_2D_SGX_KICK *ps2DKick = &ps2DSubmit->s2DKick;
	IMG_UINT32 i, ui32RoundedCmdSize;
#if defined(PDUMP)
	const PVRSRV_CONNECTION* psConnection = psTQContext->psDevData->psConnection;

	if (PDUMPISCAPTURINGTEST(psConnection) || psQueueTransfer->bPDumpContinuous)
	{
		PDUMPCOMMENTF(psConnection, IMG_FALSE, "\r\n-- Prepare2DCoreBlit --\r\n");
	}
#else
	PVR_UNREFERENCED_PARAMETER(psTQContext);
#endif
	ui32RoundedCmdSize = SGXCalcContextCCBParamSize(sizeof (SGXMKIF_2DCMD), SGX_CCB_ALLOCGRAN);

	ps2DCmd = (PSGXMKIF_2DCMD) SGXAcquireCCB(psTQContext->psDevData, psCCB, ui32RoundedCmdSize, psTQContext->hOSEvent);
	if (!ps2DCmd)
	{
		/* we were unable to acquire space in the CCB so exit */
		return PVRSRV_ERROR_TIMEOUT;
	}
	ps2DKick->hCCBMemInfo = psCCB->psCCBClientMemInfo->hKernelMemInfo;

	ps2DKick->ui32SharedCmdCCBOffset = CURRENT_CCB_OFFSET(psCCB) + offsetof(SGXMKIF_2DCMD, sShared);

	/* Safe to update the CCB WOFF as the this can no longer fail */
	UPDATE_CCB_OFFSET(*psCCB->pui32WriteOffset, ui32RoundedCmdSize, psCCB->ui32Size);

	/* Setup the flags within the command */
	ps2DCmd->ui32Size = ui32RoundedCmdSize;
	ps2DCmd->ui32Flags = psQueueTransfer->ui32Flags;
	ps2DCmd->ui32CtrlSizeInDwords = psQueueTransfer->Details.s2DBlit.ui32CtrlSizeInDwords;
	ps2DCmd->ui32AlphaRegValue = psQueueTransfer->Details.s2DBlit.ui32AlphaRegValue;

	if(ps2DCmd->ui32CtrlSizeInDwords >= SGX_MAX_2D_BLIT_CMD_SIZE)
	{
		PVR_DPF((PVR_DBG_ERROR, "Prepare2DCoreBlit: 2D command size larger than max"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	PVRSRVMemCopy(ps2DCmd->aui32CtrlStream, psQueueTransfer->Details.s2DBlit.pui32CtrlStream,
				ps2DCmd->ui32CtrlSizeInDwords * sizeof (IMG_UINT32));

	// flush 2d blits and generate interrupt
	#if defined(SGX_FEATURE_PTLA)
		ps2DCmd->aui32CtrlStream[ps2DCmd->ui32CtrlSizeInDwords++] = 0x80000000; // PTLA flush
	#else
		ps2DCmd->aui32CtrlStream[ps2DCmd->ui32CtrlSizeInDwords++] = 0xF0000000; // Eurasia 2D flush
	#endif /* SGX_FEATURE_PTLA */
	
#if defined(PDUMP)
	SGXTQ_PDump2DCmd(psConnection, psQueueTransfer, ps2DCmd);
#endif

	ps2DKick->ui32NumSrcSync = psQueueTransfer->ui32NumSources;
	/* Setup the source sync info(s) */
	for (i = 0; i < ps2DKick->ui32NumSrcSync; i++)
	{
		psSyncInfo = psQueueTransfer->asSources[i].psSyncInfo;
		ps2DKick->ahSrcSyncInfo[i] = psSyncInfo->hKernelSyncInfo;
	}
	/* Setup the destination sync info */
	psSyncInfo = psQueueTransfer->asDests[0].psSyncInfo;
	ps2DKick->hDstSyncInfo = psSyncInfo ? psSyncInfo->hKernelSyncInfo : IMG_NULL;
	ps2DSubmit->ps2DCmd = ps2DCmd;

	return PVRSRV_OK;
}
#endif//#if defined(SGX_FEATURE_2D_HARDWARE)


#if !defined(FIX_HW_BRN_27298)


static PVRSRV_ERROR PrepareNormalBlit(SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext,
									  SGX_QUEUETRANSFER *psQueueTransfer,
									  IMG_UINT32 ui32Pass,
									  SGXTQ_PREP_INTERNAL *psPassData,
									  SGXMKIF_TRANSFERCMD * psSubmit,
									  PVRSRV_TRANSFER_SGX_KICK *psKick,
									  IMG_UINT32 *pui32PassesRequired)
{
	SGXTQ_SURFACE		* psSrcSurf;
	SGXTQ_SURFACE		* psDstSurf;
	IMG_RECT			* psSrcRect;
	IMG_RECT			* psDstRect;
	IMG_UINT32			ui32SrcBytesPerPixel;
	IMG_UINT32			ui32DstBytesPerPixel;

	IMG_UINT32			aui32Limms[6];

	IMG_UINT32			ui32SrcDevVAddr;
	IMG_UINT32			ui32DstDevVAddr;

	SGXTQ_TSP_COORDS	sTSPCoords;

	IMG_UINT32			ui32DstWidth;
	IMG_UINT32			ui32DstHeight;

	IMG_UINT32			ui32SrcWidth;
	IMG_UINT32			ui32SrcHeight;

	IMG_UINT32			ui32SrcLineStride;
	IMG_UINT32			ui32DstLineStride;
	IMG_UINT32			ui32DstTAGWidth = 0;
	IMG_UINT32			ui32DstTAGLineStride = 0;


	IMG_UINT32			* pui32PBEState = psPassData->aui32PBEState;
	SGXTQ_PDS_UPDATE	* psPDSValues = & psPassData->sPDSUpdate;
	PVRSRV_ERROR		eError;

	IMG_RECT			sSrcRect;
	IMG_RECT			sDstRect;

	SGXTQ_USEFRAGS		eUSEProg = SGXTQ_USEBLIT_NORMAL;
	SGXTQ_PDSPRIMFRAGS	ePDSPrim = SGXTQ_PDSPRIMFRAG_SINGLESOURCE;
	SGXTQ_PDSSECFRAGS	ePDSSec = SGXTQ_PDSSECFRAG_BASIC;

#if !defined(SGX_FEATURE_USE_VEC34)
	IMG_BOOL			bStridedBlit = IMG_FALSE;
#endif
	IMG_BOOL			bInversionNeeded = IMG_FALSE;
#if ! defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
	IMG_BOOL			bBackBlit = IMG_FALSE;
#endif

	SGXTQ_ALPHA			eAlpha;
	SGXTQ_COLOURKEY		eColourKey;

	IMG_DEV_VIRTADDR	sUSEExecAddr;

	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo;

	IMG_BYTE			byCustomRop3;
	IMG_UINT32			ui32NumLayers = 1;
	IMG_BOOL			bAltUseCode = IMG_FALSE;

	IMG_UINT32			ui32NumTempRegs;


	/* Normal copy blit either reads a src and blits it to the destination,
	 * or reads a source, reads the destination, and blits a combination of
	 * the two, where the combination can be blending or ROP or something else.
	 *
	 * even if the destination is read for blending etc. it's not included in the
	 * Srcs array.
	 */
	if((ui32Pass == 0)
			&& (psQueueTransfer->ui32NumSources != 1
				|| psQueueTransfer->ui32NumDest != 1
				|| SGX_QUEUETRANSFER_NUM_SRC_RECTS(*psQueueTransfer) != 1
				|| SGX_QUEUETRANSFER_NUM_DST_RECTS(*psQueueTransfer) != 1))
	{
		PVR_DPF((PVR_DBG_ERROR, "PrepareNormalBlit: Number of destinations (%u) or sources (%u) != 1",
			psQueueTransfer->ui32NumDest,
			psQueueTransfer->ui32NumSources));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	psSrcSurf = &(psQueueTransfer->asSources[0]);
	psDstSurf = &(psQueueTransfer->asDests[0]);

	/* can't fail before copying the sync (if ui32Pass != 0)*/
	if (psSrcSurf->psSyncInfo)
	{
		psKick->ui32NumSrcSync = 1;
		psSyncInfo = psSrcSurf->psSyncInfo;
		psKick->ahSrcSyncInfo[0] = psSyncInfo->hKernelSyncInfo;
	}
	else
	{
		psKick->ui32NumSrcSync = 0;
		psKick->ahSrcSyncInfo[0] = IMG_NULL;
	}
	if (psDstSurf->psSyncInfo)
	{
		psKick->ui32NumDstSync = 1;
		psSyncInfo = psDstSurf->psSyncInfo;
		psKick->ahDstSyncInfo[0] = psSyncInfo->hKernelSyncInfo;
	}
	else
	{
		psKick->ui32NumDstSync = 0;
		psKick->ahDstSyncInfo[0] = IMG_NULL;
	}

	psSrcRect = & SGX_QUEUETRANSFER_SRC_RECT(*psQueueTransfer, 0);
	psDstRect = & SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0);

	ui32SrcHeight = psSrcSurf->ui32Height;
	ui32DstHeight = psDstSurf->ui32Height;

	eAlpha = psQueueTransfer->Details.sBlit.eAlpha;
	eColourKey = psQueueTransfer->Details.sBlit.eColourKey;
	byCustomRop3 = psQueueTransfer->Details.sBlit.byCustomRop3;

	// The caller can supply alternative usse code for the standard blt types
	if (psQueueTransfer->Details.sBlit.sUSEExecAddr.uiAddr != 0)
	{
		bAltUseCode = IMG_TRUE;
	}

	if (byCustomRop3 == 0xCC)
	{
		byCustomRop3 = 0; // Source copy is not a special case
	}

	/* let's figure out the number of input layers. */
	if ((bAltUseCode && ! psQueueTransfer->Details.sBlit.bSingleSource)
		|| byCustomRop3 != 0
		|| eAlpha != SGXTQ_ALPHA_NONE
		|| eColourKey != SGXTQ_COLOURKEY_NONE)
	{
		ePDSPrim = SGXTQ_PDSPRIMFRAG_TWOSOURCE;
		ui32NumLayers = 2;
	}

	if (ui32Pass == 0)
	{
		/* Strided blit requires special shader - we can't do it if otherwise
		 * it wouldn't be a straight copy shader */
		IMG_BOOL	bCanDoStridedBlit = ((! bAltUseCode)
				&& (ui32NumLayers == 1)
				&& (eAlpha == SGXTQ_ALPHA_NONE)
				&& (eColourKey == SGXTQ_COLOURKEY_NONE)
				&& (byCustomRop3 ==0)) ? IMG_TRUE : IMG_FALSE;
		IMG_UINT32	i;


		ui32SrcDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf);
		ui32DstDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psDstSurf);

		if (! ISALIGNED(ui32SrcDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT)
				|| ! ISALIGNED(ui32DstDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT)
				|| (ui32NumLayers == 2 && ! ISALIGNED(ui32DstDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT)))
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}

		PVR_DPF((PVR_DBG_VERBOSE, "\t-> Generic blit from 0x%08x to 0x%08x", ui32SrcDevVAddr, ui32DstDevVAddr));

		/* First pass through.  Need to calculate everything
		   from basics
		 */
		*pui32PassesRequired = 1;

		sSrcRect = *psSrcRect;
		sDstRect = *psDstRect;

		for (i = 0; i < ui32NumLayers; i++)
		{
			PVRSRVMemSet(psPDSValues->asLayers[i].aui32TAGState,
					0,
					sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);
		}

		PVRSRVMemSet(pui32PBEState,
				0,
				sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);


		/* get the pixel information */
		eError = SGXTQ_GetPixelFormats(psSrcSurf->eFormat,
				(psSrcSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
				psDstSurf->eFormat,
				(psDstSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
			    psQueueTransfer->Details.sBlit.eFilter,
				psPDSValues->asLayers[0].aui32TAGState,	
				IMG_NULL,
				& ui32SrcBytesPerPixel,
				pui32PBEState,
				& ui32DstBytesPerPixel,
				ui32Pass,
				pui32PassesRequired);
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		/* if the dst is sampled in via the TAG set up the pixel stuff in the second layer*/
		if (ui32NumLayers == 2)
		{
			IMG_UINT32 ui32DstSamplingPasses = 1;

			/* get the pixel information */
			eError = SGXTQ_GetPixelFormats(psDstSurf->eFormat,
					(psDstSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
					psDstSurf->eFormat,
					(psDstSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
			    	psQueueTransfer->Details.sBlit.eFilter,
					psPDSValues->asLayers[1].aui32TAGState,	
					IMG_NULL,
					IMG_NULL,
					IMG_NULL,
					IMG_NULL,
					ui32Pass,
					& ui32DstSamplingPasses);
			if (PVRSRV_OK != eError)
			{
				return eError;
			}
			if (ui32DstSamplingPasses != *pui32PassesRequired)
			{
				/* the number of required passes to sample the destination differs from
				 * the source.
				 */
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}
		} 

		/* PRQA S 3415 1 */ /* ignore side effects warning */
		if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psSrcSurf,
												ui32SrcBytesPerPixel,
												IMG_TRUE,
												bCanDoStridedBlit,
												& ui32SrcLineStride)) ||
			(PVRSRV_OK != SGXTQ_GetSurfaceStride(psDstSurf,
												ui32DstBytesPerPixel,
												IMG_FALSE,
												bCanDoStridedBlit,
												& ui32DstLineStride)) ||
			(PVRSRV_OK != SGXTQ_GetSurfaceWidth(psSrcSurf,
											ui32SrcBytesPerPixel,
											IMG_TRUE,
											bCanDoStridedBlit,
											& ui32SrcWidth)) ||
			(PVRSRV_OK != SGXTQ_GetSurfaceWidth(psDstSurf,
											ui32DstBytesPerPixel,
											IMG_FALSE,
											bCanDoStridedBlit,
											& ui32DstWidth)))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		/* and now check the destination as a source */
		if (ui32NumLayers == 2)
		{
			if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psDstSurf,
												ui32DstBytesPerPixel,
												IMG_TRUE,
												IMG_FALSE, /* can't do strided blit*/
												& ui32DstTAGLineStride)) ||
				(PVRSRV_OK != SGXTQ_GetSurfaceWidth(psDstSurf,
											ui32DstBytesPerPixel,
											IMG_TRUE,
											IMG_FALSE,
											& ui32DstTAGWidth)))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
		}

		/* Y inversion only needed when either the src or dst has negative stride but not both*/
		// Negative source stride support
		if (psSrcSurf->i32StrideInBytes < 0)
		{
			bInversionNeeded = (bInversionNeeded) ? IMG_FALSE : IMG_TRUE;

			// Subtract neg stride offset, the no. of bytes from pixel 0,0 to the start of the surface
			ui32SrcDevVAddr -= ((psSrcSurf->ui32Height - 1) * (IMG_UINT32) (-psSrcSurf->i32StrideInBytes));

			// Invert the source offsets
			sSrcRect.y0 = (IMG_INT32) psSrcSurf->ui32Height - psSrcRect->y1;
			sSrcRect.y1 = (IMG_INT32) psSrcSurf->ui32Height - psSrcRect->y0;
		}

		// Negative destination stride support
		if (psDstSurf->i32StrideInBytes < 0)
		{
			bInversionNeeded = (bInversionNeeded) ? IMG_FALSE : IMG_TRUE;

			// Subtract neg stride offset, the no. of bytes from pixel 0,0 to the start of the surface
			ui32DstDevVAddr -= ((psDstSurf->ui32Height - 1) * (IMG_UINT32) (-psDstSurf->i32StrideInBytes));

			// Invert the dest offsets
			sDstRect.y0 = (IMG_INT32) psDstSurf->ui32Height - psDstRect->y1;
			sDstRect.y1 = (IMG_INT32) psDstSurf->ui32Height - psDstRect->y0;
		}

#if defined(DEBUG)
		gui32SrcAddr[4] = gui32SrcAddr[3];
		gui32SrcAddr[3] = gui32SrcAddr[2];
		gui32SrcAddr[2] = gui32SrcAddr[1];
		gui32SrcAddr[1] = gui32SrcAddr[0];
		gui32SrcAddr[0] = ui32SrcDevVAddr;

		gui32DstAddr[4] = gui32DstAddr[3];
		gui32DstAddr[3] = gui32DstAddr[2];
		gui32DstAddr[2] = gui32DstAddr[1];
		gui32DstAddr[1] = gui32DstAddr[0];
		gui32DstAddr[0] = ui32DstDevVAddr;
#endif

		/* the source sub rectangles always have to be inside the src surface otherwise we would introduce
		 * undefined pixels
		 */
		if (sSrcRect.x0 < 0 || sSrcRect.y0 < 0 || sSrcRect.x1 < 0 || sSrcRect.y1 < 0
			|| sSrcRect.x0 > (IMG_INT32)ui32SrcWidth || sSrcRect.y0 > (IMG_INT32)ui32SrcHeight
			|| sSrcRect.x1 > (IMG_INT32)ui32SrcWidth || sSrcRect.y1 > (IMG_INT32)ui32SrcHeight
			|| ((ui32NumLayers == 2)
				&& (sDstRect.x0 < 0 || sDstRect.y0 < 0 || sDstRect.x1 < 0 || sDstRect.y1 < 0
					|| sDstRect.x0 > (IMG_INT32)ui32DstTAGWidth
					|| sDstRect.y0 > (IMG_INT32)ui32DstHeight
					|| sDstRect.x1 > (IMG_INT32)ui32DstTAGWidth
					|| sDstRect.y1 > (IMG_INT32)ui32DstHeight)))
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}

		if (eAlpha != SGXTQ_ALPHA_NONE)
		{
			if ((byCustomRop3 != 0)
				|| (bAltUseCode != IMG_FALSE)
				|| (eColourKey != SGXTQ_COLOURKEY_NONE))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			/* Blend operation */
			SGXTQ_ShaderFromAlpha(eAlpha, &eUSEProg, &ePDSSec);
		}
		else if (byCustomRop3 != 0)
		{
			const IMG_BYTE byRopIndex = byCustomRop3 >> 4;

			if ((bAltUseCode != IMG_FALSE)
				|| (eColourKey != SGXTQ_COLOURKEY_NONE))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			//It turns out that the common rops are 0x11, 0x22, 0x33  etc.  So check for this pattern
			if (byRopIndex == (byCustomRop3 & 0x0F))
			{
				/* this can go back to 1 input layer - we might have rejected this blit
				 * earlier in the 2 layer path.
				 */
				if (PVRSRV_OK != SGXTQ_ShaderFromRop(byCustomRop3,
													&eUSEProg,
													&ePDSPrim,
													&ui32NumLayers))
				{
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
			}
		}
		else if (bAltUseCode)
		{
			if (eColourKey != SGXTQ_COLOURKEY_NONE)
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			if (! psQueueTransfer->Details.sBlit.bSingleSource)
			{
				/* TODO
				 * the client tells us that we have to deal with a given USE code but we have
				 * no knowledge about that code at all. Presuming that it needs exactly this setup
				 * is rather hackish and wrong */
				ePDSPrim = SGXTQ_PDSPRIMFRAG_TWOSOURCE;
				ui32NumLayers = 2;
				ePDSSec = SGXTQ_PDSSECFRAG_TEXSETUP;
			}
		}
		else if (psQueueTransfer->Details.sBlit.eColourKey)
		{
			// Colour Key support
			if (psQueueTransfer->Details.sBlit.eColourKey == SGXTQ_COLOURKEY_SOURCE)
			{
				eUSEProg = SGXTQ_USEBLIT_SOURCE_COLOUR_KEY;
			}
			else if (psQueueTransfer->Details.sBlit.eColourKey == SGXTQ_COLOURKEY_DEST)
			{
				eUSEProg = SGXTQ_USEBLIT_DEST_COLOUR_KEY;
			}
			else
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			ePDSPrim = SGXTQ_PDSPRIMFRAG_TWOSOURCE;
			ui32NumLayers = 2;
			ePDSSec = SGXTQ_PDSSECFRAG_TEXSETUP;
		}
		else if (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_A2RGB10 &&
				 psSrcSurf->eFormat != psDstSurf->eFormat)
		{
			eUSEProg = SGXTQ_USEBLIT_A2R10G10B10;
		}
		else if ( (psSrcSurf->eFormat != psDstSurf->eFormat) &&
				  ((psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_A2B10G10R10) ||
				   (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_A2B10G10R10_UNORM)) )
		{
			eUSEProg = SGXTQ_USEBLIT_A2B10G10R10;
		}
#if !defined(SGX_FEATURE_USE_VEC34)
		else if (psQueueTransfer->Details.sBlit.bEnableGamma)
		{
			eUSEProg = SGXTQ_USEBLIT_SRGB;
		}
#endif
#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
		/* Handle A8 for Channel Replication differently (refer BRN 31145) */
		else if ( (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_A8) &&  
				(psDstSurf->eFormat == PVRSRV_PIXEL_FORMAT_ARGB8888) )
		{
			eUSEProg = SGXTQ_USEBLIT_A8;
		}
#endif

		/* we have to save the shader info because bw blitting might override it*/
		psPassData->Details.sBlitData.ui32NumLayers = ui32NumLayers;
		psPassData->Details.sBlitData.ePDSPrim = ePDSPrim;
		psPassData->Details.sBlitData.ePDSSec = ePDSSec;
		psPassData->Details.sBlitData.eUSEProg = eUSEProg;

#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
		/* select default scan direction (+x, +y) */
		psPassData->Details.sBlitData.ui32ScanDirection = EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR;

		//TODO: should we bother to test for src.dst rect overlap?

		/* Source Move detection */
		if (SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf) ==
			SGX_TQSURFACE_GET_DEV_VADDR(*psDstSurf))
		{
			/* what direction is the move? */
			if (sDstRect.x0 > sSrcRect.x0)
			{
				if (sDstRect.y0 > sSrcRect.y0)
				{
					/* scan -x, -y */
					psPassData->Details.sBlitData.ui32ScanDirection = EUR_CR_ISP_RENDER_FAST_SCANDIR_BR2TL;
				}
				else
				{
					/* scan -x, +y */
					psPassData->Details.sBlitData.ui32ScanDirection = EUR_CR_ISP_RENDER_FAST_SCANDIR_TR2BL;
				}
			}
			else
			{
				if (sDstRect.y0 > sSrcRect.y0)
				{
					/* scan +x, -y */
					psPassData->Details.sBlitData.ui32ScanDirection = EUR_CR_ISP_RENDER_FAST_SCANDIR_BL2TR;
				}
			}
		}
#else/* SGX_FEATURE_NATIVE_BACKWARD_BLIT */
		/* Source Move detection */
		if (SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf) ==
			SGX_TQSURFACE_GET_DEV_VADDR(*psDstSurf))
		{
			IMG_UINT32 ui32PreviousHeight = 0xffffffff;

			if ((sDstRect.y1 - sDstRect.y0) != (sSrcRect.y1 - sSrcRect.y0) ||
				(sDstRect.x1 - sDstRect.x0) != (sSrcRect.x1 - sSrcRect.x0))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			if (*pui32PassesRequired != 1)
			{
				/* supporting multi-chunk here is difficult because either we would have to
				 * acquire number of chunks times more memory from the SB - or we would have
				 * to keep track of bw-subtexturing progress per channel
				 */
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			psPassData->sBackwardBlitData.sClampedSrcRect = *psSrcRect;
			psPassData->sBackwardBlitData.sClampedDstRect = *psDstRect;

			SGXTQ_ClampInputRects(&psPassData->sBackwardBlitData.sClampedSrcRect,
								ui32SrcWidth,
								ui32SrcHeight,
								&psPassData->sBackwardBlitData.sClampedDstRect,
								ui32DstWidth,
								ui32DstHeight);

			sSrcRect = psPassData->sBackwardBlitData.sClampedSrcRect;
			sDstRect = psPassData->sBackwardBlitData.sClampedDstRect;

			/* backward blit (DST/SRC rect overlap) detection */
			if (((sDstRect.y1 > sSrcRect.y0) && (sDstRect.y0 < sSrcRect.y1)) &&
				((sDstRect.x1 > sSrcRect.x0) && (sDstRect.x0 < sSrcRect.x1)))
			{
				/*
					1) work out the maximum we can fit in this space
					2) Ask for space in the staging buffer
					3) set dst to be the buffer+writeoffset
				 */
				IMG_UINT32	ui32BlockLineStride, ui32StrideGran;
				IMG_UINT32	ui32BlockHeight;
				IMG_UINT32	ui32Height;
				IMG_UINT32	ui32BlockByteSize;
				IMG_BOOL	bSuccess;

				/* this pass goes back to single src copy*/
				eUSEProg	= SGXTQ_USEBLIT_NORMAL;
				ePDSPrim	= SGXTQ_PDSPRIMFRAG_SINGLESOURCE;
				ePDSSec		= SGXTQ_PDSSECFRAG_BASIC;
				ui32NumLayers	= 1;
				bAltUseCode		= IMG_FALSE;

				PVR_DPF((PVR_DBG_VERBOSE, "PrepareNormalBlit: backward blit!"));

				if (psTQContext->psStagingBuffer == IMG_NULL)
				{
					return PVRSRV_ERROR_NO_STAGING_BUFFER_ALLOCATED;
				}
				ui32BlockLineStride = (IMG_UINT32) (sSrcRect.x1 - sSrcRect.x0);

				/* align to the TAG granularity */
				ui32StrideGran = SGXTQ_GetStrideGran(ui32BlockLineStride, ui32SrcBytesPerPixel);
				ui32BlockLineStride = (ui32BlockLineStride + ui32StrideGran - 1) & ~(ui32StrideGran - 1);

				/* make sure the PBE 2px alignment as well*/
				ui32BlockLineStride = (ui32BlockLineStride + 1) & (~1U);

				ui32Height = (IMG_UINT32) (sSrcRect.y1 - sSrcRect.y0);
				ui32BlockHeight = ui32Height;

				ui32BlockByteSize = ui32BlockHeight * ui32BlockLineStride * ui32SrcBytesPerPixel;

				do
				{
					IMG_UINT32	ui32Delay = ui32Height / ui32BlockHeight * 2 - 1;

					/* we fence this allocation such a way that it can't be freed until the entire duration of
					 * the backwards blit. doing fenceid + 1 would mean delaying it by 1 hw op, while this means
					 * free after last chunk comes back. 
					 */
					bSuccess = SGXTQ_AcquireCB(psTQContext->psFenceIDMemInfo,
							psTQContext->ui32FenceID + ui32Delay,
							psTQContext->hOSEvent,
							psTQContext->psStagingBuffer,
							ui32BlockByteSize,
							(ui32BlockHeight <= EURASIA_ISPREGION_SIZEY) ? IMG_TRUE : IMG_FALSE,
							IMG_NULL,
							& ui32DstDevVAddr,
							psQueueTransfer->bPDumpContinuous);

					if (! bSuccess)
					{
						ui32PreviousHeight = ui32BlockHeight;
						/* check half the size (rounded) */
						ui32BlockHeight = MAX(1, ui32BlockHeight >> 1);
						ui32BlockHeight = MIN(ui32Height, (ui32BlockHeight + EURASIA_ISPREGION_SIZEY - 1) & ~(EURASIA_ISPREGION_SIZEY - 1));
						ui32BlockByteSize = ui32BlockHeight * ui32BlockLineStride * ui32SrcBytesPerPixel;

					}
				} while (! bSuccess && ui32PreviousHeight != ui32BlockHeight);

				if (! bSuccess)
				{
					/* this is a cool error code, but wouldn't it be better just to report TIMEOUT*/
					PVR_DPF((PVR_DBG_ERROR, "Can't do the backwards blit, the staging buffer doesn't get consumed."));
					return PVRSRV_ERROR_UNABLE_TO_DO_BACKWARDS_BLIT;
				}

				/* we have the space, now do the blit */
				/* change the src and dest rects */
				if (psSrcRect->y0 > psDstRect->y0)
				{
					sSrcRect.y1 = sSrcRect.y0 + (IMG_INT32) ui32BlockHeight;
				}
				else
				{
					sSrcRect.y0 = sSrcRect.y1 - (IMG_INT32) ui32BlockHeight;
				}

				sDstRect.x0 = 0;
				sDstRect.x1 = (sSrcRect.x1 - sSrcRect.x0);
				sDstRect.y0 = 0;
				sDstRect.y1 = (IMG_INT32) ui32BlockHeight;

				/* Setup the dest */
				ui32DstLineStride = ui32BlockLineStride;
				ui32DstHeight = ui32BlockHeight;
				ui32DstWidth = (IMG_UINT32) (sDstRect.x1 - sDstRect.x0);

				bBackBlit = IMG_TRUE;
				psPassData->sBackwardBlitData.ui32SrcLineStride = ui32BlockLineStride;
				psPassData->sBackwardBlitData.sBufferDevAddr.uiAddr = ui32DstDevVAddr;
				psPassData->sBackwardBlitData.ui32BlockHeight = ui32BlockHeight;

				/* we can't be sure at this point that doubling the required passes is going to be enough
				 * but we have already saved the number of passes per multi chunk block
				 */
				*pui32PassesRequired *= 2;
			}
		}
#endif/* SGX_FEATURE_NATIVE_BACKWARD_BLIT */

#if ! defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION) && \
	! defined(SGX_FEATURE_USE_VEC34)
		if ((psSrcSurf->eMemLayout == SGXTQ_MEMLAYOUT_STRIDE
					|| psSrcSurf->eMemLayout == SGXTQ_MEMLAYOUT_OUT_LINEAR)
				&& (ui32SrcLineStride % SGXTQ_GetStrideGran(ui32SrcLineStride, ui32SrcBytesPerPixel)) != 0)
		{
			/* System memory surface */
			ePDSPrim = SGXTQ_PDSPRIMFRAG_ITER;
			ePDSSec = SGXTQ_PDSSECFRAG_TEXSETUP;
			eUSEProg = SGXTQ_USEBLIT_STRIDE;

			bStridedBlit = IMG_TRUE;
			if (ui32SrcBytesPerPixel > 4)
			{
				eUSEProg = SGXTQ_USEBLIT_STRIDE_HIGHBPP;
			}
		}
#endif
		if (ui32NumLayers > 1)
		{
			sTSPCoords.ui32Src1U0 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.x0, ui32DstTAGWidth);
			sTSPCoords.ui32Src1U1 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.x1, ui32DstTAGWidth);
			sTSPCoords.ui32Src1V0 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.y0, ui32DstHeight);
			sTSPCoords.ui32Src1V1 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.y1, ui32DstHeight);
		}

		/*
		 * TSP coords
		 */
		sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.x0, ui32SrcWidth);
		sTSPCoords.ui32Src0U1 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.x1, ui32SrcWidth);


		// Negative stride support
		if (bInversionNeeded)
		{
			// Invert image
			sTSPCoords.ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.y1, ui32SrcHeight);
			sTSPCoords.ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.y0, ui32SrcHeight);
		}
		else
		{
			sTSPCoords.ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.y0, ui32SrcHeight);
			sTSPCoords.ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.y1, ui32SrcHeight);
		}

		if (*pui32PassesRequired > 1)
		{
			/* Populate the pass details */
			psPassData->Details.sBlitData.ui32SrcBytesPerPixel = ui32SrcBytesPerPixel;
			psPassData->Details.sBlitData.ui32DstBytesPerPixel = ui32DstBytesPerPixel;
			psPassData->Details.sBlitData.ui32SrcDevVAddr = ui32SrcDevVAddr;
			psPassData->Details.sBlitData.ui32DstDevVAddr = ui32DstDevVAddr;
#if ! defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
			psPassData->Details.sBlitData.bBackBlit = bBackBlit;
#endif
			psPassData->Details.sBlitData.ui32SrcLineStride = ui32SrcLineStride;
			psPassData->Details.sBlitData.ui32DstLineStride = ui32DstLineStride;
			/* ui32NumLayers might have been already modified*/
			if (psPassData->Details.sBlitData.ui32NumLayers > 1)
			{
				psPassData->Details.sBlitData.ui32DstTAGWidth = ui32DstTAGWidth;
				psPassData->Details.sBlitData.ui32DstTAGLineStride = ui32DstTAGLineStride;
			}

			psPassData->Details.sBlitData.ui32SrcWidth = ui32SrcWidth;
			psPassData->Details.sBlitData.ui32DstWidth = ui32DstWidth;

			psPassData->Details.sBlitData.sSrcRect = sSrcRect;
			psPassData->Details.sBlitData.sDstRect = sDstRect;

			psPassData->Details.sBlitData.sTSPCoords = sTSPCoords;
		}
	}
	else /* uipass > 0 */
	{

		/* Common stuff */
		ui32SrcBytesPerPixel	= psPassData->Details.sBlitData.ui32SrcBytesPerPixel;
		ui32DstBytesPerPixel	= psPassData->Details.sBlitData.ui32DstBytesPerPixel;
		ui32SrcDevVAddr			= psPassData->Details.sBlitData.ui32SrcDevVAddr;
		ui32DstDevVAddr			= psPassData->Details.sBlitData.ui32DstDevVAddr;

#if ! defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
		bBackBlit = psPassData->Details.sBlitData.bBackBlit;
#endif

		ui32NumLayers = psPassData->Details.sBlitData.ui32NumLayers;
		ePDSPrim = psPassData->Details.sBlitData.ePDSPrim;
		ePDSSec = psPassData->Details.sBlitData.ePDSSec;
		eUSEProg = psPassData->Details.sBlitData.eUSEProg;

		ui32SrcLineStride = psPassData->Details.sBlitData.ui32SrcLineStride;
		ui32DstLineStride = psPassData->Details.sBlitData.ui32DstLineStride;

		ui32SrcWidth = psPassData->Details.sBlitData.ui32SrcWidth;
		ui32DstWidth = psPassData->Details.sBlitData.ui32DstWidth;
		if (ui32NumLayers > 1)
		{
			ui32DstTAGWidth	= psPassData->Details.sBlitData.ui32DstTAGWidth;
			ui32DstTAGLineStride = psPassData->Details.sBlitData.ui32DstTAGLineStride;
		}

		sSrcRect = psPassData->Details.sBlitData.sSrcRect;
		sDstRect = psPassData->Details.sBlitData.sDstRect;

		sTSPCoords = psPassData->Details.sBlitData.sTSPCoords;

#if ! defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
		if (bBackBlit)
		{
			IMG_RECT *psClampedSrcRect = &psPassData->sBackwardBlitData.sClampedSrcRect;
			IMG_RECT *psClampedDstRect = &psPassData->sBackwardBlitData.sClampedDstRect;

			if ((ui32Pass % 2) == 0)
			{
				/* copy the next block to staging buffer */
				/* More passes are required */
				IMG_UINT32 ui32BlockLineStride;
				IMG_UINT32 ui32BlockHeight;

				/* this pass goes back to single src copy*/
				eUSEProg		= SGXTQ_USEBLIT_NORMAL;
				ePDSPrim		= SGXTQ_PDSPRIMFRAG_SINGLESOURCE;
				ePDSSec			= SGXTQ_PDSSECFRAG_BASIC;
				ui32NumLayers	= 1;
				bAltUseCode		= IMG_FALSE;

				ui32BlockLineStride = psPassData->sBackwardBlitData.ui32SrcLineStride;

				if (psSrcRect->y0 > psDstRect->y0)
				{
					ui32BlockHeight = (IMG_UINT32) (psClampedSrcRect->y1 - sSrcRect.y1);
				}
				else
				{
					ui32BlockHeight = (IMG_UINT32) (sSrcRect.y0 - psClampedSrcRect->y0);
				}

				/* we have space for what's stored in passdata, our texture is ui32BlockHeight
				 * choose the smaller
				 */
				ui32BlockHeight = MIN(psPassData->sBackwardBlitData.ui32BlockHeight, ui32BlockHeight);

				/* change the src and dest rects */
				if (psSrcRect->y0 > psDstRect->y0)
				{
					sSrcRect.y0 = sSrcRect.y1;
					sSrcRect.y1 = sSrcRect.y0 + (IMG_INT32) ui32BlockHeight;
				}
				else
				{
					sSrcRect.y1 = sSrcRect.y0;
					sSrcRect.y0 = sSrcRect.y1 - (IMG_INT32) ui32BlockHeight;
				}
				psPassData->Details.sBlitData.sSrcRect = sSrcRect;

				sDstRect.x0 = 0;
				sDstRect.x1 = (sSrcRect.x1 - sSrcRect.x0);
				sDstRect.y0 = 0;
				sDstRect.y1 = (IMG_INT) ui32BlockHeight;
				psPassData->Details.sBlitData.sDstRect = sDstRect;

				/* Setup the dest */
				ui32DstLineStride = ui32BlockLineStride;
				ui32DstHeight = ui32BlockHeight;
				ui32DstWidth = (IMG_UINT32) (sDstRect.x1 - sDstRect.x0);

				ui32DstDevVAddr = psPassData->sBackwardBlitData.sBufferDevAddr.uiAddr;
			}
			else
			{
				/* blit the staging block to the destination surface */
				IMG_INT32 i32TmpSrcY1 = sSrcRect.y1;
				IMG_INT32 i32TmpSrcY0 = sSrcRect.y0;

				ui32SrcLineStride = psPassData->sBackwardBlitData.ui32SrcLineStride;
				ui32SrcDevVAddr = psPassData->sBackwardBlitData.sBufferDevAddr.uiAddr;
				ui32DstDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psDstSurf);
				ui32SrcHeight = (IMG_UINT32) (sSrcRect.y1 - sSrcRect.y0);
				/* Stride has been aligned so it can be both stride/width on all cores*/
				ui32SrcWidth = ui32SrcLineStride;

				/* need to recalc dst line stride as bw blit initially set it to the staging stride
				 * this is for the PBE only, if we have to read this surface we will use tge ui32DstTAG*
				 * variables which should be untuouched.
				 */

				/* PRQA S 3415 1 */ /* ignore side effects warning */
				if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psDstSurf,
								ui32DstBytesPerPixel,
								IMG_FALSE,
								IMG_FALSE,
								& ui32DstLineStride))
						|| (PVRSRV_OK != SGXTQ_GetSurfaceWidth(psDstSurf,
								ui32DstBytesPerPixel,
								IMG_FALSE,
								IMG_FALSE,
								& ui32DstWidth)))
				{
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
				ui32DstHeight = psDstSurf->ui32Height;

				sSrcRect = sDstRect;

				sDstRect.x0 = psClampedDstRect->x0;
				sDstRect.x1 = psClampedDstRect->x1;
				if (psSrcRect->y0 > psDstRect->y0)
				{
					sDstRect.y0 = psClampedDstRect->y0 + (i32TmpSrcY0 - psClampedSrcRect->y0);
					sDstRect.y1 = sDstRect.y0 + (IMG_INT) ui32SrcHeight;
					if (sDstRect.y1 != psClampedDstRect->y1)
						*pui32PassesRequired += 2;
				}
				else
				{
					sDstRect.y1 = psClampedDstRect->y1 - (psClampedSrcRect->y1 - i32TmpSrcY1);
					sDstRect.y0 = sDstRect.y1 - (IMG_INT) ui32SrcHeight;
					if (sDstRect.y0 != psClampedDstRect->y0)
						*pui32PassesRequired += 2;
				}

				/* blend or ROP the destination with the staging block */
				if (ui32NumLayers > 1)
				{
					sTSPCoords.ui32Src1U0 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.x0, ui32DstTAGWidth);
					sTSPCoords.ui32Src1U1 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.x1, ui32DstTAGWidth);

					sTSPCoords.ui32Src1V0 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.y0, ui32DstHeight);
					sTSPCoords.ui32Src1V1 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.y1, ui32DstHeight);
				}
			}
			/* Recalc the U coords and V coords */
			sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.x0, ui32SrcWidth);
			sTSPCoords.ui32Src0U1 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.x1, ui32SrcWidth);
			sTSPCoords.ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.y0, ui32SrcHeight);
			sTSPCoords.ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.y1, ui32SrcHeight);
		}
		else
#endif
		{
			/* get the px info for this pass */
			eError = SGXTQ_GetPixelFormats(psSrcSurf->eFormat,
					(psSrcSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
					psDstSurf->eFormat,
					(psDstSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
					psQueueTransfer->Details.sBlit.eFilter,
					psPDSValues->asLayers[0].aui32TAGState,	
					IMG_NULL,
					IMG_NULL,
					pui32PBEState,
					IMG_NULL,
					ui32Pass,
					IMG_NULL);
			if (PVRSRV_OK != eError)
			{
				return eError;
			}

			/* if the dst is sampled in via the TAG set up the pixel stuff in the second layer*/
			if (ui32NumLayers == 2)
			{
				eError = SGXTQ_GetPixelFormats(psDstSurf->eFormat,
						(psDstSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
						psDstSurf->eFormat,
						(psDstSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
						psQueueTransfer->Details.sBlit.eFilter,
						psPDSValues->asLayers[1].aui32TAGState,	
						IMG_NULL,
						IMG_NULL,
						pui32PBEState,
						IMG_NULL,
						ui32Pass,
						IMG_NULL);
				if (PVRSRV_OK != eError)
				{
					return eError;
				}
			}
			ui32SrcDevVAddr += psSrcSurf->ui32ChunkStride * ui32Pass;
			if (!ISALIGNED(ui32SrcDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			ui32DstDevVAddr += psDstSurf->ui32ChunkStride * ui32Pass;
			if (!ISALIGNED(ui32DstDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
		}
	}

	if (psSrcSurf->eFormat != psDstSurf->eFormat)
	{
		if (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_A2RGB10)
		{
			eUSEProg = SGXTQ_USEBLIT_A2R10G10B10;
		}
		else if ( 	(psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_A2B10G10R10) ||
					(psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_A2B10G10R10_UNORM) )
		{
			eUSEProg = SGXTQ_USEBLIT_A2B10G10R10;
		}
	}
	
	/*
	 * Set up the PBE
	 */
	eError = SGXTQ_SetPBEState(&sDstRect,
							psDstSurf->eMemLayout,
							ui32DstWidth,
							ui32DstHeight,
							ui32DstLineStride,
							0,
							ui32DstDevVAddr,
							0,
							psQueueTransfer->Details.sBlit.eRotation,
							((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE),
							IMG_TRUE,
							pui32PBEState);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_SetupTransferClipRenderBox(psSubmit,
											(IMG_UINT32) sDstRect.x0,
											(IMG_UINT32) sDstRect.y0,
											(IMG_UINT32) sDstRect.x1,
											(IMG_UINT32) sDstRect.y1,
											ui32DstWidth,
											ui32DstHeight);

	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	if (psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERTX)
	{
		IMG_UINT32 ui32Tmp = sTSPCoords.ui32Src0U0;
		sTSPCoords.ui32Src0U0 = sTSPCoords.ui32Src0U1;
		sTSPCoords.ui32Src0U1 = ui32Tmp;
	}
	if (psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERTY)
	{
		IMG_UINT32 ui32Tmp = sTSPCoords.ui32Src0V0;
		sTSPCoords.ui32Src0V0 = sTSPCoords.ui32Src0V1;
		sTSPCoords.ui32Src0V1 = ui32Tmp;
	}

	/*
	 * common PDS values - primaries
	 */
	SGXTQ_SetTAGState(psPDSValues,
					0,
					ui32SrcDevVAddr,
					psQueueTransfer->Details.sBlit.eFilter,
					ui32SrcWidth,
					ui32SrcHeight,
					ui32SrcLineStride,
					0, //ui32SrcTAGFormat,
					ui32SrcBytesPerPixel,
					IMG_TRUE,
					psSrcSurf->eMemLayout);

#if ! defined(SGX_FEATURE_USE_VEC34)
	if (bStridedBlit)		/* PRQA S 3359,3201 */ /* can be true for some builds */
	{
		/* atm it's only supported on the fst layer*/
		eError = SGXTQ_SetupStridedBlit(psPDSValues->asLayers[0].aui32TAGState,
										psSrcSurf,
										eUSEProg,
										ui32SrcLineStride,
										ui32SrcBytesPerPixel,
										*pui32PassesRequired,
										ui32Pass,
										aui32Limms);
		if (PVRSRV_OK != eError)
		{
			return eError;
		}
	}
#endif

	// If the caller supplied alternative usse code then use it otherwise use the standard code
	/* TODO : client also has to give us the number of PAs*/
	if (bAltUseCode)
	{
		sUSEExecAddr.uiAddr = psQueueTransfer->Details.sBlit.sUSEExecAddr.uiAddr;
		ui32NumTempRegs = psQueueTransfer->Details.sBlit.uiNumTemporaryRegisters;
	}
	else
	{
		/* get the sader */
		SGXTQ_RESOURCE* psResource;
		eError = SGXTQ_CreateUSEResource(psTQContext,
				eUSEProg,
				aui32Limms,
				psQueueTransfer->bPDumpContinuous);
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		psResource = psTQContext->apsUSEResources[eUSEProg];
		sUSEExecAddr = psResource->sDevVAddr;
		ui32NumTempRegs = psResource->uResource.sUSE.ui32NumTempRegs;
	}

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
	psPDSValues->asLayers[0].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT) |
													(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif


	SGXTQ_SetUSSEKick(psPDSValues,
					sUSEExecAddr,
					psTQContext->sUSEExecBase,
					ui32NumTempRegs);

	switch (ePDSPrim)
	{
		case SGXTQ_PDSPRIMFRAG_SINGLESOURCE:
		{
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			psPDSValues->ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
			psPDSValues->ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

			psPDSValues->asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
															EURASIA_PDS_DOUTI_TEXISSUE_TC0	|
															EURASIA_PDS_DOUTI_TEXLASTISSUE;

			break;
		}
		case SGXTQ_PDSPRIMFRAG_TWOSOURCE:
		{
			SGXTQ_SetTAGState(psPDSValues,
							  1,
							  ui32DstDevVAddr,
							  psQueueTransfer->Details.sBlit.eFilter,
							  ui32DstTAGWidth,
							  ui32DstHeight,
							  ui32DstTAGLineStride,
							  0, //ui32DstTAGFormat,
							  ui32DstBytesPerPixel,
							  IMG_TRUE,
							  psDstSurf->eMemLayout);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			psPDSValues->ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
			psPDSValues->ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
			psPDSValues->asLayers[1].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT) |
															(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif

			psPDSValues->asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
															EURASIA_PDS_DOUTI_TEXISSUE_TC0;

			psPDSValues->asLayers[1].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
															EURASIA_PDS_DOUTI_TEXISSUE_TC1	|
															EURASIA_PDS_DOUTI_TEXLASTISSUE;
			break;
		}
		case SGXTQ_PDSPRIMFRAG_ITER:
		{

			psPDSValues->asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_TEXISSUE_NONE	|
															EURASIA_PDS_DOUTI_USEISSUE_TC0	|
															EURASIA_PDS_DOUTI_USEDIM_2D		|
															EURASIA_PDS_DOUTI_USELASTISSUE;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			psPDSValues->ui32U1 |= EURASIA_PDS_DOUTU1_ITERATORSDEPENDENCY;
#else
			psPDSValues->ui32U0 |= EURASIA_PDS_DOUTU0_ITERATORSDEPENDENCY;
#endif

			break;
		}

		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "PrepareNormalBlit: Unknown PDS primary: %d", (IMG_UINT32) ePDSPrim));
			PVR_DBG_BREAK;
			return PVRSRV_ERROR_UNKNOWN_PRIMARY_FRAG;
		}
	}

	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			ePDSPrim,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}


	/*
	 *  Update the PDS secondaries
	 */
	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
			ePDSSec,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	switch (ePDSSec)
	{
		case SGXTQ_PDSSECFRAG_BASIC:
			break;
		case SGXTQ_PDSSECFRAG_TEXSETUP:
		{
			if (bAltUseCode)
			{
				/* Update the per-blt constants needed for the caller's alternative usse code */
				psPDSValues->aui32A[0] = psQueueTransfer->Details.sBlit.UseParams[0];
				psPDSValues->aui32A[1] = psQueueTransfer->Details.sBlit.UseParams[1];
				psPDSValues->aui32A[2] = 0; // Unused at present, may be needed though
			}
			else if (psQueueTransfer->Details.sBlit.eColourKey)
			{
				psPDSValues->aui32A[0] = psQueueTransfer->Details.sBlit.ui32ColourKey; /* argb8888 key colour */
				psPDSValues->aui32A[1] = psQueueTransfer->Details.sBlit.ui32ColourKeyMask; /* argb8888 mask */
				psPDSValues->aui32A[2] = 0;
			}
			else
			{
				IMG_UINT32 i;

				for (i = 0; i < EURASIA_TAG_TEXTURE_STATE_SIZE; i++)
					psPDSValues->aui32A[i] = psPDSValues->asLayers[0].aui32TAGState[i];
			}
			break;
		}
		case SGXTQ_PDSSECFRAG_1ATTR:
		{
			/* Update the secondary PDS program constants for SGXTQ_ALPHA_GLOBAL and similar */
			IMG_UINT32 ui32Value = psQueueTransfer->Details.sBlit.byGlobalAlpha;

			ui32Value |= (ui32Value << 8) | (ui32Value << 16) | (ui32Value << 24);
			psPDSValues->aui32A[0] = ui32Value;
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "PrepareNormalBlit: Unexpected PDS secondary: %d", (IMG_UINT32) ePDSSec));
			PVR_DBG_BREAK;
			return PVRSRV_ERROR_UNEXPECTED_SECONDARY_FRAG;
		}
	}

	eError = SGXTQ_CreatePDSSecResource(psTQContext,
			ePDSSec,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = SGXTQ_CreateISPResource(psTQContext,
									psTQContext->apsPDSPrimResources[ePDSPrim],
									psTQContext->apsPDSSecResources[ePDSSec],
									&sDstRect,
									&sTSPCoords,
									IMG_FALSE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									ui32NumLayers,
									psQueueTransfer->bPDumpContinuous,
									psQueueTransfer->ui32Flags);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									pui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}


	SGXTQ_SetupTransferRegs(psTQContext,
#if defined(SUPPORT_DISPLAYCONTROLLER_TILING)
			psQueueTransfer->Details.sBlit.ui32BIFTile0Config,
#else
			0,
#endif
			psSubmit,
			psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
			ui32NumLayers,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
			psPassData->Details.sBlitData.ui32ScanDirection,
#else
			0,
#endif
			EUR_CR_ISP_RENDER_TYPE_FASTSCALE);



	/* Specific setup stuff */
	psSubmit->bLoadFIRCoefficients = IMG_FALSE;

	return PVRSRV_OK;
}

static PVRSRV_ERROR PrepareTextureUpload(SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext,
										 SGX_QUEUETRANSFER				* psQueueTransfer,
										 IMG_UINT32						ui32Pass,
										 SGXTQ_PREP_INTERNAL			* psPassData,
										 SGXMKIF_TRANSFERCMD			* psSubmit,
										 PVRSRV_TRANSFER_SGX_KICK		* psKick,
										 IMG_UINT32						* pui32PassesRequired)
{
	SGXTQ_TEXTURE_UPLOADOP		* psLoadTexture = &psQueueTransfer->Details.sTextureUpload;
	SGXTQ_TEXTURE_UPLOAD_DATA	* psTextureData = & psPassData->Details.sTexUplData;
	SGXTQ_SURFACE				* psSrcSurf = & psQueueTransfer->asSources[0];
	SGXTQ_SURFACE				* psDstSurf = & psQueueTransfer->asDests[0];

	SGXTQ_PDS_UPDATE			* psPDSValues = & psPassData->sPDSUpdate;
	IMG_UINT32					* pui32PBEState = psPassData->aui32PBEState; 
	IMG_UINT32					i;
	IMG_UINT32					ui32NumChunks;
	IMG_UINT32					ui32ChanIndex;


	/* basic validation: should have only 1 src and 1 dest*/
	if ((ui32Pass == 0)
		&& (psQueueTransfer->ui32NumSources != 1
			|| psQueueTransfer->ui32NumDest != 1
			|| SGX_QUEUETRANSFER_NUM_SRC_RECTS(*psQueueTransfer) != 1
			|| SGX_QUEUETRANSFER_NUM_DST_RECTS(*psQueueTransfer) != 1))
	{
		PVR_DPF((PVR_DBG_ERROR, "PrepareTextureUpload: Number of destinations and sources != 1"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* can't fail before sync is copied over*/
	if (psDstSurf->psSyncInfo)
	{
		psKick->ui32NumDstSync = 1;
		psKick->ahDstSyncInfo[0] = psDstSurf->psSyncInfo->hKernelSyncInfo;
	}
	else
	{
		psKick->ui32NumDstSync = 0;
		psKick->ahDstSyncInfo[0] = IMG_NULL;
	}
	psKick->ui32NumSrcSync = 0;
	psKick->ahSrcSyncInfo[0] = IMG_NULL;

	if (ui32Pass == 0)
    {
		/*
		 * Source
		 */
		IMG_RECT	* psSrcRect;
		IMG_PBYTE	pbySrcLinAddr = psLoadTexture->pbySrcLinAddr;

		IMG_UINT32	ui32SrcBytesPP;
		IMG_UINT32	ui32NumChunks2 = 1;
		IMG_UINT32	ui32DstBytesPP;
		/* TODO : fix for <0 values */
		IMG_UINT32	ui32SrcStrideInBytes = (IMG_UINT32)psSrcSurf->i32StrideInBytes;
		IMG_UINT32	ui32PixelByteStride;
		
		IMG_UINT32	ui32SBLineStride;
		IMG_RECT	* psRect = IMG_NULL;

		IMG_RECT	* psDstRect = &psTextureData->sDstRect;
		IMG_UINT32	ui32DstWidth;
		IMG_UINT32	ui32DstHeight;
		IMG_UINT32	ui32DstLineStride;

		IMG_UINT32	ui32StrideGran;

		PVRSRV_ERROR	eError;

		/* Rects */
		psSrcRect = & SGX_QUEUETRANSFER_SRC_RECT(*psQueueTransfer, 0);
		*psDstRect = SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0);


		if (((psSrcRect->x1 - psSrcRect->x0) != (psDstRect->x1 - psDstRect->x0))
			|| ((psSrcRect->y1 - psSrcRect->y0) != (psDstRect->y1 - psDstRect->y0)))
		{
			/* this is not a routine to stretch, so return */
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		PVRSRVMemSet(psPDSValues->asLayers[0].aui32TAGState,
				0,
				sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);

		PVRSRVMemSet(pui32PBEState,
				0,
				sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);

		/* get the pixel format setup */
		eError = SGXTQ_GetPixelFormats(psSrcSurf->eFormat,
				(psSrcSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
				psDstSurf->eFormat,
				(psDstSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
				SGXTQ_FILTERTYPE_POINT,
				psPDSValues->asLayers[0].aui32TAGState,	
				IMG_NULL,
				& ui32SrcBytesPP,
				pui32PBEState,
				& ui32DstBytesPP,
				ui32Pass,
				& ui32NumChunks2);
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		if (psSrcSurf->ui32ChunkStride != 0)
		{
			/* src is multi-chunk planar*/
			ui32PixelByteStride = 0;
		}
		else
		{
			/* as we have cheated on the src being planar, we have to readjust the pixel stride */
			ui32PixelByteStride = ui32SrcBytesPP * ui32NumChunks2;
		}

		/* Dest*/
		ui32DstWidth = psDstSurf->ui32Width;
		ui32DstHeight = psDstSurf->ui32Height;
		
		for (i = 0; i < ui32NumChunks2; i++)
		{
			psRect = & psTextureData->asSBRect[i];

			/* rebasing the SB rectangle to (0, 0)*/
			psRect->x0 = 0;
			psRect->y0 = 0;
			psRect->x1 = psSrcRect->x1 - psSrcRect->x0;
			psRect->y1 = psSrcRect->y1 - psSrcRect->y0;

			/* TODO.  */
			if ((psRect->x1 > EURASIA_TEXTURESIZE_MAX) || (psRect->y1 > EURASIA_TEXTURESIZE_MAX))
			{
				PVR_DPF((PVR_DBG_ERROR, "PrepareTextureUpload: Source texture dimensions exceed HW limits!"));
				return PVRSRV_ERROR_EXCEEDED_HW_LIMITS;
			}
		}

		/* TODO.  */
		if ((psDstRect->x1 - psDstRect->x0 > EURASIA_RENDERSIZE_MAXX) || (psDstRect->y1 - psDstRect->y0 > EURASIA_RENDERSIZE_MAXY))
		{
			PVR_DPF((PVR_DBG_ERROR, "PrepareTextureUpload: Destination surface dimensions exceed HW limits!"));
			return PVRSRV_ERROR_EXCEEDED_HW_LIMITS;
		}


		/*
		 * Sub tile twiddling
		 * only 16 x 16 tile is supported. We set the PBE to do strided instead of twiddled.
		 * Twiddling is done in software in the EOT.
		 */
		psTextureData->bSubTwiddled = IMG_FALSE;

#if ! defined(SGX_FEATURE_HYBRID_TWIDDLING)
		if ((psDstSurf->eMemLayout == SGXTQ_MEMLAYOUT_2D
					|| psDstSurf->eMemLayout == SGXTQ_MEMLAYOUT_OUT_TWIDDLED)
				&& ui32DstWidth != ui32DstHeight
				&& (ui32DstWidth << 1) != ui32DstHeight
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
				&& (ui32DstWidth < EURASIA_PIXELBES2LO_XSIZE_ALIGN
					|| ui32DstHeight < EURASIA_PIXELBES2LO_YSIZE_ALIGN)
#else
				&& (ui32DstWidth < EURASIA_PIXELBE1S0_XSIZE_ALIGN
					|| ui32DstHeight < EURASIA_PIXELBE1S0_YSIZE_ALIGN)
#endif
		   )
		{
#if defined(SGXTQ_SUBTILE_TWIDDLING)
			if ((ui32DstWidth & (ui32DstWidth - 1)) != 0
				|| (ui32DstHeight & (ui32DstHeight - 1)) != 0)
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			if (ui32NumChunks2 != 1)
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			/* Check if we requested a sub texture upload.*/
			if (ui32DstWidth != (IMG_UINT32) psDstRect->x1
				|| ui32DstHeight != (IMG_UINT32) psDstRect->y1
				|| 0 != psDstRect->x0
				|| 0 != psDstRect->y0)
			{
				PVR_DPF((PVR_DBG_ERROR, "PrepareTextureUpload: Can't do sub texture upload combined with sub tile twiddling!"));
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			/* destination stride in bytes 
			 * we move the PBE fbaddr with stride per emit hence we can't have a stride
			 * not aligned to framebuffer granularity
			 */
			i = psDstSurf->ui32Width * ui32DstBytesPP;

			if (! ISALIGNED(i, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
			{
				PVR_DPF((PVR_DBG_ERROR, "PrepareTextureUpload: Can't do sub tile twiddling to unaligned surface/stride!"));
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			PVR_DPF((PVR_DBG_VERBOSE, "PrepareTextureUpload: Sub tile twiddling is on."));
			ui32DstLineStride = psDstSurf->ui32Width;
			psTextureData->bSubTwiddled = IMG_TRUE;
#else
			PVR_DPF((PVR_DBG_ERROR, "PrepareTextureUpload: Can't do sub tile twiddling!"));
			return PVRSRV_ERROR_NOT_SUPPORTED;
#endif
		}
		else
#endif /* ! SGX_FEATURE_HYBRID_TWIDDLING*/
		{
			/* Not sub tile twiddling - normal limitations apply on the dst. as the hw doesn't touch
			 * the src we don't have any limitation on that one
			 */
			/* PRQA S 3415 1 */ /* ignore side effects warning */
			if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psDstSurf,
													ui32DstBytesPP,
													IMG_FALSE,
													IMG_FALSE,
													& ui32DstLineStride))
				|| (PVRSRV_OK != SGXTQ_GetSurfaceWidth(psDstSurf,
												ui32DstBytesPP,
												IMG_FALSE,
												IMG_FALSE,
												& ui32DstWidth)))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
		}

		/* Now we can determine the address offset into the source texture
		 * this has to be a valid frame buffer address.
		 */
		pbySrcLinAddr = pbySrcLinAddr + ((ui32SrcStrideInBytes * (IMG_UINT32) psSrcRect->y0) + ((IMG_UINT32) psSrcRect->x0 * ui32SrcBytesPP));
		if (! ISALIGNED((IMG_UINTPTR_T)pbySrcLinAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		/* We will force exit earlier, atm we can't calculate the required passes*/
		*pui32PassesRequired = 0xffffffff;

		/*
		 * Round up the staging stride so that we never need a strided blit
		 * (psRect has the data for the last chunk from above) 
		 */
		ui32StrideGran = SGXTQ_GetStrideGran((IMG_UINT32) psRect->x1, ui32SrcBytesPP);

		ui32SBLineStride = ((IMG_UINT32) psRect->x1 + ui32StrideGran - 1) & ~(ui32StrideGran - 1);
		psTextureData->ui32U2Float = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->x1, ui32SBLineStride);

		/* fill in the pass details */ 
		psTextureData->ui32SrcBytesPP = ui32SrcBytesPP;
		psTextureData->ui32NumChunks = ui32NumChunks2;
		psTextureData->ui32PixelByteStride = ui32PixelByteStride;
		psTextureData->ui32DstWidth = ui32DstWidth;
		psTextureData->ui32DstHeight = ui32DstHeight;
#if defined(SGXTQ_SUBTILE_TWIDDLING)
		psTextureData->ui32DstBytesPP = ui32DstBytesPP;
#endif
		psTextureData->ui32SBLineStride = ui32SBLineStride;
		psTextureData->ui32DstLineStride = ui32DstLineStride;
		for (i = 0; i < ui32NumChunks2; i++)
		{
			if (psSrcSurf->ui32ChunkStride != 0)
			{
				psTextureData->apbySrcLinAddr[i] = pbySrcLinAddr + i * psSrcSurf->ui32ChunkStride;
			}
			else
			{
				psTextureData->apbySrcLinAddr[i] = pbySrcLinAddr + i * ui32SrcBytesPP; 
			}
			psTextureData->aui32HeightLeft[i] = (IMG_UINT32) psRect->y1;
		}
	}

	/* go to the next non-empty channel */
	ui32NumChunks = psTextureData->ui32NumChunks;
	for (ui32ChanIndex = ui32Pass % ui32NumChunks; 
			psTextureData->aui32HeightLeft[ui32ChanIndex] == 0;
			ui32ChanIndex = (ui32ChanIndex+1) % ui32NumChunks) /* semicolon intended */ ; 

	{
		/* load back pass details */
		IMG_UINT32	ui32SBLineStride = psTextureData->ui32SBLineStride;
		IMG_UINT32	ui32SrcBytesPP = psTextureData->ui32SrcBytesPP;
		IMG_UINT32	ui32PixelByteStride = psTextureData->ui32PixelByteStride;

		IMG_PBYTE	pbySrcLinAddr = psTextureData->apbySrcLinAddr[ui32ChanIndex];
		IMG_RECT	* psSBRect = & psTextureData->asSBRect[ui32ChanIndex];
		IMG_UINT32	ui32HeightLeft = psTextureData->aui32HeightLeft[ui32ChanIndex];

		IMG_UINT32	ui32DstWidth = psTextureData->ui32DstWidth;
		IMG_UINT32	ui32DstHeight = psTextureData->ui32DstHeight;
#if defined(SGXTQ_SUBTILE_TWIDDLING)
		IMG_UINT32	ui32DstBytesPP = psTextureData->ui32DstBytesPP;
#endif
		IMG_UINT32	ui32DstLineStride = psTextureData->ui32DstLineStride;

		IMG_UINT32	ui32SBDevVAddr;
		IMG_PVOID	pvSBLinAddr;


		/* Locals*/
		IMG_UINT32	ui32SrcStrideInBytes = (IMG_UINT32)psSrcSurf->i32StrideInBytes;
		IMG_UINT32	ui32PreviousHeight = 0xffffffff;
		IMG_RECT	sDstRect;
		IMG_UINT32	ui32DstDevVAddr;

		IMG_UINT32	ui32BatchHeight;
		IMG_UINT32	ui32RequestedSize;
		SGXTQ_RESOURCE	* psPixEvent;
		PVRSRV_ERROR	eError;
		IMG_BOOL   	bSuccess;

		SGXTQ_TSP_COORDS sTSPCoords;

		if (psTQContext->psStagingBuffer == IMG_NULL)
		{
			PVR_DPF((PVR_DBG_ERROR, "PrepareTextureUpload: No staging buffer allocated!!!"));
			return PVRSRV_ERROR_NO_STAGING_BUFFER_ALLOCATED;
		}

		ui32BatchHeight = ui32HeightLeft;
		do
		{
			ui32RequestedSize = ui32BatchHeight * ui32SBLineStride * ui32SrcBytesPP;

			bSuccess = SGXTQ_AcquireCB(psTQContext->psFenceIDMemInfo,
					psTQContext->ui32FenceID,
					psTQContext->hOSEvent,
					psTQContext->psStagingBuffer,
					ui32RequestedSize,
					(ui32BatchHeight <= EURASIA_ISPREGION_SIZEY) ? IMG_TRUE : IMG_FALSE,
					& pvSBLinAddr,
					& ui32SBDevVAddr,
					psQueueTransfer->bPDumpContinuous);

			if ( ! bSuccess )
			{
				ui32PreviousHeight = ui32BatchHeight;

				/* if we aren't sub-tile twiddling*/
				if (ui32BatchHeight > EURASIA_ISPREGION_SIZEY)
				{
					/* check half the size (rounded) */
					ui32BatchHeight >>= 1;
					ui32BatchHeight = (ui32BatchHeight + EURASIA_ISPREGION_SIZEY - 1) & ~(EURASIA_ISPREGION_SIZEY - 1);
				}
			}
		} while (! bSuccess && ui32PreviousHeight != ui32BatchHeight);

		if (! bSuccess)
		{
			PVR_DPF((PVR_DBG_ERROR, "Can't do the texture upload, either the texture is too wide or the staging buffer doesn't get consumed."));
			return PVRSRV_ERROR_UPLOAD_TOO_BIG;
		}

		SGXTQ_CopyToStagingBuffer(pvSBLinAddr,
				ui32SBLineStride * ui32SrcBytesPP,
				pbySrcLinAddr,
				ui32SrcStrideInBytes,
				ui32SrcBytesPP,
				ui32PixelByteStride,
				ui32BatchHeight,
				(IMG_UINT32)psSBRect->x1);

		sDstRect.x0 = SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0).x0;
		sDstRect.y0 = SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0).y0 + psSBRect->y0;
		sDstRect.x1 = sDstRect.x0 + psSBRect->x1 - psSBRect->x0;
		sDstRect.y1 = sDstRect.y0 + (IMG_INT) ui32BatchHeight;

		ui32DstDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psDstSurf) + psDstSurf->ui32ChunkStride * ui32ChanIndex;
		if (!ISALIGNED(ui32DstDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		/* get the px info for this pass */
		eError = SGXTQ_GetPixelFormats(psSrcSurf->eFormat,
				(psSrcSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
				psDstSurf->eFormat,
				(psDstSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
				SGXTQ_FILTERTYPE_POINT,
				psPDSValues->asLayers[0].aui32TAGState,	
				IMG_NULL,
				IMG_NULL,
				pui32PBEState,
				IMG_NULL,
				ui32ChanIndex,
				IMG_NULL);
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		/*
		 * Set up the PBE
		 */
		if (psTextureData->bSubTwiddled)
		{
			IMG_RECT sModDstRect = { 0 };

#if defined(FIX_HW_BRN_30842)
			sModDstRect = sDstRect;
#else
			if ((IMG_UINT32)sDstRect.x1 > EURASIA_ISPREGION_SIZEX / 2)
			{
				sModDstRect.x1 = EURASIA_ISPREGION_SIZEX / 2; 
				ui32DstLineStride = EURASIA_ISPREGION_SIZEX / 2;
			}
			else
			{
				sModDstRect.x1 = sDstRect.x1;
			}
#if EURASIA_TAPDSSTATE_PIPECOUNT == 4
			if ((IMG_UINT32)sDstRect.y1 > EURASIA_ISPREGION_SIZEY / 2)
			{
				sModDstRect.y1 = EURASIA_ISPREGION_SIZEY / 2;
			}
			else
#endif
			{
				sModDstRect.y1 = sDstRect.y1;
			}
#endif
			eError = SGXTQ_SetPBEState(&sModDstRect,
					SGXTQ_MEMLAYOUT_OUT_LINEAR,
				ui32DstWidth,
				ui32DstHeight,
				ui32DstLineStride,
				0,
				ui32DstDevVAddr,
				0,
				SGXTQ_ROTATION_NONE,
				((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE),
				IMG_TRUE,
				pui32PBEState);
#if defined(FIX_HW_BRN_30842)
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
			pui32PBEState[5] |= EURASIA_PIXELBES2HI_NOADVANCE;

			/* We MUST keep the PBE pix count as 0 if subtwiddled*/
			pui32PBEState[0] &= EURASIA_PIXELBES0LO_COUNT_CLRMSK;
#else
			pui32PBEState[1] |= EURASIA_PIXELBE1S1_NOADVANCE;

			/* We MUST keep the PBE pix count as 0 if subtwiddled*/
			pui32PBEState[2] &= EURASIA_PIXELBE2S0_COUNT_CLRMSK;
#endif
#endif
		}
		else
		{
			eError = SGXTQ_SetPBEState(&sDstRect,
					psDstSurf->eMemLayout,
					ui32DstWidth,
					ui32DstHeight,
					ui32DstLineStride,
					0,
					ui32DstDevVAddr,
					0,
					SGXTQ_ROTATION_NONE,
					((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE),
					IMG_TRUE,
					pui32PBEState);
		}

		if (eError != PVRSRV_OK)
		{
			return eError;
		}

		/* Create the shader */
		eError = SGXTQ_CreateUSEResource(psTQContext,
				SGXTQ_USEBLIT_NORMAL,
				IMG_NULL,
				psQueueTransfer->bPDumpContinuous);
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		/*
		 * PDS Primary
		 */
		psSBRect->y1 = psSBRect->y0 + (IMG_INT) ui32BatchHeight;

		SGXTQ_SetTAGState(psPDSValues,
				0,
				ui32SBDevVAddr,
				SGXTQ_FILTERTYPE_POINT,
				ui32SBLineStride,
				ui32BatchHeight,
				ui32SBLineStride,
				0, //ui32DstTAGFormat,
				ui32SrcBytesPP,
				IMG_TRUE,
				SGXTQ_MEMLAYOUT_STRIDE);


		/* TODO : this can be moved to pass 0 */
		psPDSValues->asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
														EURASIA_PDS_DOUTI_TEXISSUE_TC0	|
														EURASIA_PDS_DOUTI_TEXLASTISSUE;

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
		psPDSValues->asLayers[0].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT) |
														(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif

		SGXTQ_SetUSSEKick(psPDSValues,
				psTQContext->apsUSEResources[SGXTQ_USEBLIT_NORMAL]->sDevVAddr,
				psTQContext->sUSEExecBase,
				psTQContext->apsUSEResources[SGXTQ_USEBLIT_NORMAL]->uResource.sUSE.ui32NumTempRegs);


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		psPDSValues->ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
		psPDSValues->ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

		eError = SGXTQ_CreatePDSPrimResource(psTQContext,
											SGXTQ_PDSPRIMFRAG_SINGLESOURCE,
											psPDSValues,
											psQueueTransfer->bPDumpContinuous);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}

		/*
		 * PDS secondary
		 */
		eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
				SGXTQ_PDSSECFRAG_BASIC,
				psPDSValues,
				psQueueTransfer->bPDumpContinuous);
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		eError = SGXTQ_CreatePDSSecResource(psTQContext,
				SGXTQ_PDSSECFRAG_BASIC,
				psPDSValues,
				psQueueTransfer->bPDumpContinuous);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}

		eError = SGXTQ_SetupTransferClipRenderBox(psSubmit,
												(IMG_UINT32) sDstRect.x0,
												(IMG_UINT32) sDstRect.y0,
												(IMG_UINT32) sDstRect.x1,
												(IMG_UINT32) sDstRect.y1,
												ui32DstWidth,
												ui32DstHeight);

		if (eError != PVRSRV_OK)
		{
			return eError;
		}

		PVRSRVMemSet((IMG_PBYTE)&(sTSPCoords), 0, sizeof (sTSPCoords));
		sTSPCoords.ui32Src0U1 = psTextureData->ui32U2Float;
		sTSPCoords.ui32Src0V1 = FLOAT32_ONE;

		eError = SGXTQ_CreateISPResource(psTQContext,
										psTQContext->apsPDSPrimResources[SGXTQ_PDSPRIMFRAG_SINGLESOURCE],
										psTQContext->apsPDSSecResources[SGXTQ_PDSSECFRAG_BASIC],
										&sDstRect,
										&sTSPCoords,
										IMG_FALSE,
										(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
										1,
										psQueueTransfer->bPDumpContinuous,
										0);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}

#if defined(SGXTQ_SUBTILE_TWIDDLING)
		if (psTextureData->bSubTwiddled)
		{
			IMG_UINT32 ui32Tmp6;
			IMG_UINT32 ui32Tmp7;

#if defined(FIX_HW_BRN_30842)
			ui32Tmp7 = (MIN(psTextureData->ui32DstWidth, EURASIA_ISPREGION_SIZEX) << 16) |
					 (MIN(psTextureData->ui32DstHeight, EURASIA_ISPREGION_SIZEY) & 0xffff);
			ui32Tmp6 = ui32DstBytesPP;
#else
			IMG_UINT32 ui32U, ui32V;
			IMG_UINT32 ui32AddrShift;

			ui32U = MIN(psTextureData->ui32DstWidth, EURASIA_ISPREGION_SIZEX / 2);
#if (EURASIA_TAPDSSTATE_PIPECOUNT == 4) 
			ui32V = MIN(psTextureData->ui32DstHeight, EURASIA_ISPREGION_SIZEY / 2);
#else
			ui32V = MIN(psTextureData->ui32DstHeight, EURASIA_ISPREGION_SIZEY);
#endif

			ui32AddrShift = (ui32U * (ui32V - 1)) * ui32DstBytesPP;

			ui32Tmp6 = 0; /* default is "fast path"*/

			/* assume that if we have 2 pipes ISP tile is halved in X, if we have 4 pipes
			 * ISP tile is divided into equal size quadrants across pipes. Fast path is when
			 * texture doesn't cross pipe boundary.
			 */
			if (psTextureData->ui32DstWidth > EURASIA_ISPREGION_SIZEX / 2)
			{
				ui32Tmp6 = SGXTQ_SUBTWIDDLED_TMP6_2PIPE_MODE_MASK	|
					SGXTQ_SUBTWIDDLED_TMP6_DIRECTION_X_MASK			|
					(ui32AddrShift & SGXTQ_SUBTWIDDLED_TMP6_FBADDR_MOVE_MASK);
			}
#if EURASIA_TAPDSSTATE_PIPECOUNT == 4 
			else if (psTextureData->ui32DstHeight > EURASIA_ISPREGION_SIZEY / 2)
			{
				ui32Tmp6 = SGXTQ_SUBTWIDDLED_TMP6_2PIPE_MODE_MASK	|
					(ui32AddrShift & SGXTQ_SUBTWIDDLED_TMP6_FBADDR_MOVE_MASK);
			}
#endif

			ui32Tmp7 = (ui32U << SGXTQ_SUBTWIDDLED_TMP7_U_SHIFT) |
				(ui32V << SGXTQ_SUBTWIDDLED_TMP7_V_SHIFT); 
#endif
			/* create EOT */
			eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
											pui32PBEState,
											SGXTQ_USEEOTHANDLER_SUBTWIDDLED,
											ui32Tmp6,
											ui32Tmp7,
											psQueueTransfer->bPDumpContinuous);
			if (PVRSRV_OK != eError)
			{
				return eError;
			}

			/* create pixev */
			eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
													psTQContext->psUSEEORHandler,
													psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_SUBTWIDDLED],
													SGXTQ_PDSPIXEVENTHANDLER_TILEXY,
													psQueueTransfer->bPDumpContinuous);
			if (PVRSRV_OK != eError)
			{
				return eError;
			}

			/* goes into the CRs*/
			psPixEvent = psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_TILEXY];
		}
		else
#endif
		{
			/* create EOT */
			eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
											pui32PBEState,
											SGXTQ_USEEOTHANDLER_BASIC,
											0, 0,
											psQueueTransfer->bPDumpContinuous);
			if (PVRSRV_OK != eError)
			{
				return eError;
			}

			/* create pixev */
			eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
													psTQContext->psUSEEORHandler,
													psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
													SGXTQ_PDSPIXEVENTHANDLER_BASIC,
													psQueueTransfer->bPDumpContinuous);
			if (PVRSRV_OK != eError)
			{
				return eError;
			}

			/* goes into the CRs*/
			psPixEvent = psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC];
		}


		SGXTQ_SetupTransferRegs(psTQContext,
#if defined(SUPPORT_DISPLAYCONTROLLER_TILING)
				psQueueTransfer->Details.sBlit.ui32BIFTile0Config,
#else
				0,
#endif
				psSubmit,
				psPixEvent,
				1,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
				EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
				0,
#endif
				EUR_CR_ISP_RENDER_TYPE_FASTSCALE);

		/*
		 * proceed to the next batch/chunk
		 */
		pbySrcLinAddr += ui32SrcStrideInBytes * ui32BatchHeight;
		psSBRect->y0 += (IMG_INT) ui32BatchHeight;

		ui32HeightLeft -= MIN(ui32BatchHeight, ui32HeightLeft);

		/* save pass data */
		psTextureData->apbySrcLinAddr[ui32ChanIndex] = pbySrcLinAddr;
		psTextureData->aui32HeightLeft[ui32ChanIndex] = ui32HeightLeft;

		bSuccess = IMG_TRUE;
		for (i = 0; i < ui32NumChunks; i++)
		{
			if (psTextureData->aui32HeightLeft[i] != 0)
			{
				bSuccess = IMG_FALSE;
			}
		}

		if (bSuccess)
		{
			/* hack to stop, we read the entire src texture*/
			*pui32PassesRequired = ui32Pass + 1;
		}
	}

	return PVRSRV_OK;
}


static PVRSRV_ERROR PrepareMipGen(SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext,
								  SGX_QUEUETRANSFER *psQueueTransfer,
								  IMG_UINT32 ui32Pass,
								  SGXTQ_PREP_INTERNAL *psPassData,
								  SGXMKIF_TRANSFERCMD * psSubmit,
								  PVRSRV_TRANSFER_SGX_KICK *psKick,
								  IMG_UINT32 *pui32PassesRequired)
{
	SGXTQ_SURFACE	* psSrcSurf;
	IMG_UINT32		ui32BytesPerPixel;

	IMG_UINT32		ui32SrcDevVAddr;
	IMG_UINT32		ui32DstDevVAddr;

	IMG_UINT32		ui32DstWidth;
	IMG_UINT32		ui32DstHeight;

	IMG_UINT32		ui32SrcWidth;
	IMG_UINT32		ui32SrcHeight;

	SGXTQ_TSP_COORDS	sTSPCoords;

	SGXTQ_PDS_UPDATE	* psPDSValues = & psPassData->sPDSUpdate;
	IMG_UINT32			* pui32PBEState = psPassData->aui32PBEState; 

	IMG_RECT		sDstRect;

	SGXTQ_MIPGEN_DATA* psMipData = &psPassData->Details.sMipData;

	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo;

	PVRSRV_ERROR	eError;

	if (psQueueTransfer->ui32NumSources != 1)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psSrcSurf = &(psQueueTransfer->asSources[0]);

	/* can't fail before sync is copied over*/
	if (psSrcSurf->psSyncInfo)
	{
		psSyncInfo = psSrcSurf->psSyncInfo;
		psKick->ui32NumSrcSync = 1;
		psKick->ui32NumDstSync = 1;
		psKick->ahSrcSyncInfo[0] = psSyncInfo->hKernelSyncInfo;
		psKick->ahDstSyncInfo[0] = psSyncInfo->hKernelSyncInfo;
	}
	else
	{
		psKick->ui32NumSrcSync = 0;
		psKick->ui32NumDstSync = 0;
		psKick->ahSrcSyncInfo[0] = IMG_NULL;
		psKick->ahDstSyncInfo[0] = IMG_NULL;
	}

	if (ui32Pass == 0)
	{
		IMG_UINT32	ui32NumChunks = 1;

		PVR_DPF((PVR_DBG_VERBOSE,
				"\t-> Mipmap generation of 0x%08x for %d levels",
				SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf),
				psQueueTransfer->Details.sMipGen.ui32Levels));


		*pui32PassesRequired = psQueueTransfer->Details.sMipGen.ui32Levels;

		PVRSRVMemSet(pui32PBEState, 0, sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);

		PVRSRVMemSet(psPDSValues->asLayers[0].aui32TAGState,
				0,
				sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);


		eError = SGXTQ_GetPixelFormats(psSrcSurf->eFormat,
				(psSrcSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
				psSrcSurf->eFormat,
				(psSrcSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
				SGXTQ_FILTERTYPE_POINT,
				psPDSValues->asLayers[0].aui32TAGState,	
				IMG_NULL,
				& ui32BytesPerPixel,
				pui32PBEState,
				IMG_NULL,
				0,
				& ui32NumChunks);
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		/* Can't do packed multi-chunk */
		if ((ui32NumChunks > 1) && (psSrcSurf->ui32ChunkStride == 0))
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}

		* pui32PassesRequired *= ui32NumChunks;

		if (PVRSRV_OK != SGXTQ_GetSurfaceWidth(psSrcSurf,
											ui32BytesPerPixel,
											IMG_TRUE,
											IMG_FALSE,
											& ui32SrcWidth))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
		ui32SrcHeight = psSrcSurf->ui32Height;

		/* this has to be pow of 2 even for non twiddled surfaces */
		if ((((ui32SrcWidth - 1) & ui32SrcWidth) != 0)
				&& ((ui32SrcHeight - 1) & ui32SrcHeight) != 0)
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		/* Decide on the first pass whether we have the size for the number of Levels*/
		if ((psSrcSurf->eMemLayout != SGXTQ_MEMLAYOUT_2D)
			&& (psSrcSurf->eMemLayout != SGXTQ_MEMLAYOUT_OUT_TWIDDLED))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
		else
		{
			if (ui32SrcWidth >= ui32SrcHeight)
			{
				/* smallest that we can do is 1x1 px,
				 * we allow to run out of height before end of levels; but can't do the same
				 * for the width
				 */
				if (ui32SrcWidth < (1UL << psQueueTransfer->Details.sMipGen.ui32Levels))
				{
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
			}
			else
			{
				/* we can't do 1px X npx where n > 1 ; limitation comes from the PBE stride */
				if (ui32SrcWidth <= (1UL << psQueueTransfer->Details.sMipGen.ui32Levels))
				{
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
			}
		}

		if (ui32BytesPerPixel < (1 << EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
		{
			IMG_UINT32 i;
			IMG_UINT32 ui32CheckWidth = ui32SrcWidth;
			IMG_UINT32 ui32CheckHeight = ui32SrcHeight;

			for (i = psQueueTransfer->Details.sMipGen.ui32Levels; i > 0; i--)
			{
#if ! defined(SGX_FEATURE_HYBRID_TWIDDLING)
				if ((psSrcSurf->eMemLayout == SGXTQ_MEMLAYOUT_2D
							|| psSrcSurf->eMemLayout == SGXTQ_MEMLAYOUT_OUT_TWIDDLED)
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
						&& (ui32CheckWidth < EURASIA_PIXELBES2LO_XSIZE_ALIGN
							|| ui32CheckHeight < EURASIA_PIXELBES2LO_YSIZE_ALIGN)
#else
						&& (ui32CheckWidth < EURASIA_PIXELBE1S0_XSIZE_ALIGN
							|| ui32CheckHeight < EURASIA_PIXELBE1S0_YSIZE_ALIGN)
#endif /* SGX_FEATURE_UNIFIED_STORE_64BITS*/
						&& ui32CheckWidth != ui32CheckHeight
						&& ui32CheckHeight != (ui32CheckWidth << 1))
				{
					if (ui32CheckWidth > ui32CheckHeight)
					{
						if (!ISALIGNED(ui32CheckHeight * ui32CheckHeight * ui32BytesPerPixel,
									EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
						{
							return PVRSRV_ERROR_INVALID_PARAMS;
						}
					}
					else
					{
						if (!ISALIGNED((ui32CheckWidth * ui32CheckWidth * ui32BytesPerPixel) << 1,
									EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
						{
							return PVRSRV_ERROR_INVALID_PARAMS;
						}
					}
				}
#endif /* SGX_FEATURE_HYBRID_TWIDDLING*/
				if (!ISALIGNED(ui32CheckWidth * ui32CheckHeight * ui32BytesPerPixel,
							EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
				{
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
				ui32CheckWidth = MAX(1, ui32CheckWidth >> 1);
				ui32CheckHeight = MAX(1, ui32CheckHeight >> 1);
			}
		}

		ui32SrcDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf);

		ui32DstDevVAddr = ui32SrcDevVAddr + ui32SrcWidth * ui32SrcHeight * ui32BytesPerPixel;

		/* Populate the pass details */
		psMipData->ui32SourceDevVAddr = ui32SrcDevVAddr;
		psMipData->ui32DestDevVAddr = ui32DstDevVAddr;
		psMipData->ui32NextSourceDevVAddr = ui32DstDevVAddr;
		psMipData->ui32BytesPerPixel = ui32BytesPerPixel;
		psMipData->ui32LevelsLeft = psQueueTransfer->Details.sMipGen.ui32Levels;
		psMipData->ui32ChunksLeft = ui32NumChunks;
		psMipData->ui32BatchesLeft = 0;
		psMipData->ui32Width = ui32SrcWidth;
		psMipData->ui32Height = ui32SrcHeight;
		psMipData->ui32BatchWidth = ui32SrcWidth;
		psMipData->ui32BatchHeight = ui32SrcHeight;
		if (ui32NumChunks > 1)
		{
			psMipData->ui32FirstWidth = ui32SrcWidth;
			psMipData->ui32FirstHeight = ui32SrcHeight;
			psMipData->ui32FirstSrcDevVAddr = ui32SrcDevVAddr;
		}
	}
	else
	{
		/* Load back pass details*/
		ui32SrcDevVAddr = psMipData->ui32SourceDevVAddr;
		ui32DstDevVAddr = psMipData->ui32DestDevVAddr;

		ui32BytesPerPixel = psMipData->ui32BytesPerPixel;
		ui32SrcWidth = psMipData->ui32BatchWidth;
		ui32SrcHeight = psMipData->ui32BatchHeight;
	}

	PVR_ASSERT(ui32SrcWidth > 1 && ui32SrcHeight != 0);

	ui32DstWidth = MAX(ui32SrcWidth >> 1, 1);
	ui32DstHeight = MAX(ui32SrcHeight >> 1, 1);

	if (psMipData->ui32LevelsLeft == 0)
	{
		/* if there is nothing to do we shouldn't be here, otherwise
		 * we requested more passes than we needed
		 */
		PVR_ASSERT(psMipData->ui32ChunksLeft != 0);
		
		/* starting a new chunk*/
		psMipData->ui32ChunksLeft--;
	}
	else
	{
		if (psMipData->ui32BatchesLeft == 0)
		{
			/* starting a new level
			 *
			 * we have to determine how many batches this level has
			 */
#if ! defined(SGX_FEATURE_HYBRID_TWIDDLING)
			if ((psSrcSurf->eMemLayout == SGXTQ_MEMLAYOUT_2D
						|| psSrcSurf->eMemLayout == SGXTQ_MEMLAYOUT_OUT_TWIDDLED)
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
						&& (ui32DstWidth < EURASIA_PIXELBES2LO_XSIZE_ALIGN
							|| ui32DstHeight < EURASIA_PIXELBES2LO_YSIZE_ALIGN)
#else
						&& (ui32DstWidth < EURASIA_PIXELBE1S0_XSIZE_ALIGN
							|| ui32DstHeight < EURASIA_PIXELBE1S0_YSIZE_ALIGN)
#endif /* SGX_FEATURE_UNIFIED_STORE_64BITS*/
					 && ui32DstWidth != ui32DstHeight
					 && ui32DstHeight != (ui32DstWidth << 1))
			{
				if (ui32DstWidth > ui32DstHeight)
				{
					/* ui32DstWidth > ui32DstHeight => can't underflow*/
					/* PRQA S 0585,3382 1 */
					psMipData->ui32BatchesLeft = ui32DstWidth / ui32DstHeight - 1;
					ui32DstWidth = ui32DstHeight;
					ui32SrcWidth = ui32DstWidth << 1;
					psMipData->ui32BatchWidth = ui32SrcWidth;
				}
				else
				{
					/* ui32DstWidth * 2 < ui32DstHeight => can't underflow*/
					/* PRQA S 0585,3382 1 */
					psMipData->ui32BatchesLeft = ui32DstHeight / (ui32DstWidth << 1) - 1;
					ui32DstHeight = ui32DstWidth << 1;
					ui32SrcHeight = ui32DstHeight << 1;
					psMipData->ui32BatchHeight = ui32DstHeight;
				}
				*pui32PassesRequired += psMipData->ui32BatchesLeft;
				psMipData->ui32BatchSize = ui32SrcWidth * ui32SrcHeight * ui32BytesPerPixel;
				psMipData->ui32DstBatchSize = ui32DstWidth * ui32DstHeight * ui32BytesPerPixel;
			}
			else
#endif /* SGX_FEATURE_HYBRID_TWIDDLING*/
			{
				/* single batch level*/
				psMipData->ui32BatchesLeft = 0;

				/* if there is no level left we shouldn't be here */
				PVR_ASSERT(psMipData->ui32LevelsLeft != 0);
				psMipData->ui32LevelsLeft--;
			}
		}
		else
		{
			/* starting a new batch*/

			PVR_ASSERT(psMipData->ui32BatchesLeft != 0);
			psMipData->ui32BatchesLeft--;
			if (psMipData->ui32BatchesLeft == 0)
			{
				/* if there is no level left we shouldn't be here */
				PVR_ASSERT(psMipData->ui32LevelsLeft != 0);
				psMipData->ui32LevelsLeft--;
			}
		}
	}

	sDstRect.x0 = 0;
	sDstRect.y0 = 0;
	sDstRect.x1 = (IMG_INT) ui32DstWidth;
	sDstRect.y1 = (IMG_INT) ui32DstHeight;

	/* create the shader*/
	eError = SGXTQ_CreateUSEResource(psTQContext,
			SGXTQ_USEBLIT_NORMAL,
			IMG_NULL,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * PDS primary
	 */
	SGXTQ_SetTAGState(psPDSValues,
					0,
					ui32SrcDevVAddr,
					psQueueTransfer->Details.sMipGen.eFilter,
					ui32SrcWidth,
					ui32SrcHeight,
					ui32SrcWidth,
					0,
					ui32BytesPerPixel,
					IMG_TRUE,
					psSrcSurf->eMemLayout);

	psPDSValues->asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
													EURASIA_PDS_DOUTI_TEXISSUE_TC0	|
													EURASIA_PDS_DOUTI_TEXLASTISSUE;

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)

	psPDSValues->asLayers[0].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT)	|
													(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif

	SGXTQ_SetUSSEKick(psPDSValues,
					psTQContext->apsUSEResources[SGXTQ_USEBLIT_NORMAL]->sDevVAddr,
					psTQContext->sUSEExecBase,
					psTQContext->apsUSEResources[SGXTQ_USEBLIT_NORMAL]->uResource.sUSE.ui32NumTempRegs);


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	psPDSValues->ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
	psPDSValues->ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			SGXTQ_PDSPRIMFRAG_SINGLESOURCE,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * PDS secondary
	 */
	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
			SGXTQ_PDSSECFRAG_BASIC,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSSecResource(psTQContext,
			SGXTQ_PDSSECFRAG_BASIC,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	/*
	 * Create the ISP stream
	 */
	sTSPCoords.ui32Src0U0 = 0;
	sTSPCoords.ui32Src0U1 = FLOAT32_ONE;
	sTSPCoords.ui32Src0V0 = 0;
	sTSPCoords.ui32Src0V1 = FLOAT32_ONE;

	eError = SGXTQ_CreateISPResource(psTQContext,
									psTQContext->apsPDSPrimResources[SGXTQ_PDSPRIMFRAG_SINGLESOURCE],
									psTQContext->apsPDSSecResources[SGXTQ_PDSSECFRAG_BASIC],
									& sDstRect,
									&sTSPCoords,
									IMG_FALSE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									1,
									psQueueTransfer->bPDumpContinuous,
									0);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	/*
	 * Create the Pixelevent handlers
	 */
	/* Set up the PBE*/
	eError = SGXTQ_SetPBEState(&sDstRect,
							psSrcSurf->eMemLayout,
							ui32DstWidth,
							ui32DstHeight,
							ui32DstWidth,
							0,
							ui32DstDevVAddr,
							0,
							SGXTQ_ROTATION_NONE,
							(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE,
							IMG_TRUE,
							pui32PBEState);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	/* skipping the singleton EOR for now*/
	/* create EOT */
	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									pui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* create pixev */
	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	SGXTQ_SetupTransferRegs(psTQContext,
							0,
							psSubmit,
							psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
							1,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
		EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
		0,
#endif
		EUR_CR_ISP_RENDER_TYPE_FASTSCALE);

	SGXTQ_SetupTransferRenderBox(psSubmit,
								0,
								0,
								ui32DstWidth,
								ui32DstHeight);

	psSubmit->bLoadFIRCoefficients = IMG_FALSE;

	/*
	 * either finished a level, chunk or batch
	 */
	if (psMipData->ui32ChunksLeft == 0)
	{
		/* finished - this must be the last pass*/
		PVR_ASSERT(ui32Pass + 1 == *pui32PassesRequired);
	}
	else
	{
		if (psMipData->ui32LevelsLeft == 0)
		{
			/* finished the chunk*/		
			psMipData->ui32Width = psMipData->ui32FirstWidth; 
			psMipData->ui32Height = psMipData->ui32FirstHeight; 

			psMipData->ui32FirstSrcDevVAddr += psSrcSurf->ui32ChunkStride;
			psMipData->ui32SourceDevVAddr = psMipData->ui32FirstSrcDevVAddr;

			 /* reset number of levels for the next chunk */
			psMipData->ui32LevelsLeft = psQueueTransfer->Details.sMipGen.ui32Levels;
		}
		else
		{
			if (psMipData->ui32BatchesLeft == 0)
			{
				/* finished the level*/
				psMipData->ui32Width = MAX(psMipData->ui32Width >> 1, 1);
				psMipData->ui32Height = MAX(psMipData->ui32Height >> 1, 1);

				psMipData->ui32SourceDevVAddr = psMipData->ui32NextSourceDevVAddr;
			}
			else
			{
				/* finished a batch*/
				psMipData->ui32SourceDevVAddr += psMipData->ui32BatchSize;
				psMipData->ui32DestDevVAddr += psMipData->ui32DstBatchSize;
			}
		}

		if ((psMipData->ui32LevelsLeft == 0) || (psMipData->ui32BatchesLeft == 0))
		{
			IMG_UINT32 ui32LevelSize;

			psMipData->ui32BatchWidth = psMipData->ui32Width;
			psMipData->ui32BatchHeight = psMipData->ui32Height;

			ui32LevelSize = psMipData->ui32Width * psMipData->ui32Height * ui32BytesPerPixel;
			psMipData->ui32DestDevVAddr = psMipData->ui32SourceDevVAddr + ui32LevelSize;
			psMipData->ui32NextSourceDevVAddr = psMipData->ui32DestDevVAddr;
		}
	}

	return PVRSRV_OK;
}


static PVRSRV_ERROR PrepareFillOp(SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext,
								  SGX_QUEUETRANSFER *psQueueTransfer,
								  IMG_UINT32 ui32Pass,
								  SGXTQ_PREP_INTERNAL *psPassData,
								  SGXMKIF_TRANSFERCMD * psSubmit,
								  PVRSRV_TRANSFER_SGX_KICK *psKick,
								  IMG_UINT32 *pui32PassesRequired)
{
	SGXTQ_SURFACE	* psSurf;
	IMG_RECT		* psRect;
	IMG_UINT32		ui32LineStride;

	IMG_UINT32		ui32DevVAddr;

	SGXTQ_TSP_COORDS	sTSPCoords;
	SGXTQ_PDS_UPDATE	*psPDSValues = & psPassData->sPDSUpdate;

	IMG_UINT32		ui32Width;
	IMG_UINT32		ui32Height;
	IMG_UINT32		ui32NumLayers = 0;

	IMG_UINT32		* pui32PBEState = psPassData->aui32PBEState;

	PVRSRV_ERROR	eError;

	/* TODO remove these*/
	IMG_UINT32		ui32Left;
	IMG_UINT32		ui32Right;
	IMG_UINT32		ui32Top;
	IMG_UINT32		ui32Bottom;

	SGXTQ_USEFRAGS		eUSEProg = SGXTQ_USEBLIT_FILL;
	SGXTQ_PDSPRIMFRAGS	ePDSPrim = SGXTQ_PDSPRIMFRAG_KICKONLY;
	SGXTQ_PDSSECFRAGS	ePDSSec;

	SGXTQ_USE_RESOURCE	* psUSEResource;

	IMG_UINT32	ui32Colour = 0;
	IMG_UINT32	ui32BytesPerPixel = 0;

	PVRSRV_CLIENT_SYNC_INFO	* psSyncInfo;

	/*
	   Basic validation - should only have 1 destination and rectangle
	 */
	if ((ui32Pass ==0)
		&& (psQueueTransfer->ui32NumDest != 1
#ifndef BLITLIB
		|| psQueueTransfer->ui32NumDestRects != 1
#endif
		))
	{
		PVR_DPF((PVR_DBG_ERROR, "PrepareFillOp: Number of destinations != 1"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	psSurf = &(psQueueTransfer->asDests[0]);

	/* can't fail before the sync is copied*/
	psKick->ui32NumSrcSync = 0;
	psKick->ahSrcSyncInfo[0] = IMG_NULL;

	if (psSurf->psSyncInfo)
	{
		psSyncInfo = psSurf->psSyncInfo;
		psKick->ui32NumDstSync = 1;
		psKick->ahDstSyncInfo[0] = psSyncInfo->hKernelSyncInfo;
	}
	else
	{
		psKick->ui32NumDstSync = 0;
		psKick->ahDstSyncInfo[0] = IMG_NULL;
	}

	psRect = & SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0);

	ui32Height = psSurf->ui32Height;

	ui32DevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psSurf);

	PVR_DPF((PVR_DBG_VERBOSE, "\t-> Color fill on surface 0x%08x", ui32DevVAddr));

	/* First pass through.	Need to calculate everything
	   from basics
	 */

	ui32Left = (IMG_UINT32) psRect->x0;
	ui32Right = (IMG_UINT32) psRect->x1;
	ui32Top = (IMG_UINT32) psRect->y0;
	ui32Bottom = (IMG_UINT32) psRect->y1;

	if (ui32Pass == 0)
	{
		*pui32PassesRequired = 1;

		/* first pass, let's clear the PBE/TAG state.*/
		PVRSRVMemSet(pui32PBEState, 0, sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);

		if (psQueueTransfer->Details.sFill.byCustomRop3)
		{
			PVRSRVMemSet(psPDSValues->asLayers[0].aui32TAGState,
						0,
						sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);

			ePDSPrim = SGXTQ_PDSPRIMFRAG_SINGLESOURCE;
			ui32NumLayers = 1;

			// Custom ROP supported
			switch (psQueueTransfer->Details.sFill.byCustomRop3)
			{
				case 0xA0: /* pat AND dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_AND;
					break;
				case 0x50: /* pat AND NOT dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_ANDNOT;
					break;
				case 0x0A: /* NOT pat AND dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_NOTAND;
					break;
				case 0x5A: /* pat XOR dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_XOR;
					break;
				case 0xFA: /* pat OR dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_OR;
					break;
				case 0x05: /* NOT pat AND NOT dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_NOTANDNOT;
					break;
				case 0xA5: /* NOT pat XOR dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_NOTXOR;
					break;
				case 0xF5: /* pat OR NOT dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_ORNOT;
					break;
				case 0x0F: /* NOT pat */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_NOT;
					break;
				case 0xAF: /* NOT pat OR dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_NOTOR;
					break;
				case 0x5F: /* NOT src OR NOT dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_NOTORNOT;
					break;
				case 0xF0:
					eUSEProg = SGXTQ_USEBLIT_FILL;
					ePDSPrim = SGXTQ_PDSPRIMFRAG_KICKONLY;
					ui32NumLayers = 0;
					break;
				case 0x55: /* Invert dst */
					eUSEProg = SGXTQ_USEBLIT_ROPFILL_NOTD;
					break;
				
				default:
					PVR_DPF((PVR_DBG_ERROR,
								"FillOperation: Unknown and unsupported fill rop: %d",
								psQueueTransfer->Details.sFill.byCustomRop3));
					break;
			}


			/* and set up the pixel related bit in the PBE state*/
			eError = SGXTQ_GetPixelFormats(psSurf->eFormat,
					(psSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
					psSurf->eFormat,
					(psSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
					SGXTQ_FILTERTYPE_POINT,
					psPDSValues->asLayers[0].aui32TAGState,
					IMG_NULL,
					IMG_NULL,
					pui32PBEState,
					& ui32BytesPerPixel,
					ui32Pass,
					pui32PassesRequired);
			if (PVRSRV_OK != eError)
			{
				return eError;
			}

			if (*pui32PassesRequired != 1)
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}
		}
		else
		{
			/* and set up the pixel related bit in the PBE state*/
			eError = SGXTQ_GetPixelFormats(PVRSRV_PIXEL_FORMAT_UNKNOWN,
					IMG_FALSE,
					psSurf->eFormat,
					(psSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
					SGXTQ_FILTERTYPE_POINT,
					IMG_NULL, /* TAG state*/
					IMG_NULL, /* bTAG planar?*/
					IMG_NULL, /* TAG bpp*/
					pui32PBEState,
					& ui32BytesPerPixel,
					ui32Pass,
					pui32PassesRequired);
			if (PVRSRV_OK != eError)
			{
				return eError;
			}
		}

		
		/* PRQA S 3415 1 */ /* ignore side effects warning */
		if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psSurf,
						ui32BytesPerPixel,
						/* Do we have to give this to the TAG?*/
						(IMG_BOOL) (SGXTQ_PDSPRIMFRAG_SINGLESOURCE == ePDSPrim),
						IMG_FALSE,
						&ui32LineStride))
				|| (PVRSRV_OK != SGXTQ_GetSurfaceWidth(psSurf,
						ui32BytesPerPixel,
						/* Do we have to give this to the TAG?*/
						(IMG_BOOL) (SGXTQ_PDSPRIMFRAG_SINGLESOURCE == ePDSPrim),
						IMG_FALSE,
						& ui32Width)))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		psPassData->Details.sBlitData.ui32DstLineStride = ui32LineStride;
		psPassData->Details.sBlitData.ui32DstWidth = ui32Width;
	}
	else
	{

		ui32LineStride = psPassData->Details.sBlitData.ui32DstLineStride;
		ui32Width = psPassData->Details.sBlitData.ui32DstWidth;

		eError = SGXTQ_GetPixelFormats(PVRSRV_PIXEL_FORMAT_UNKNOWN,
				(psSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
				psSurf->eFormat,
				(psSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
				SGXTQ_FILTERTYPE_POINT,
				IMG_NULL, /* TAG state */
				IMG_NULL, /* bTAGPlanar? */
				IMG_NULL, /* TAG bpp */
				pui32PBEState,
				&ui32BytesPerPixel, /* PBE bpp */
				ui32Pass,
				IMG_NULL); /* passesReq */
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		ui32DevVAddr += (psSurf->ui32ChunkStride * ui32Pass);
		if (!ISALIGNED(ui32DevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

	// Negative stride support
	if (psSurf->i32StrideInBytes < 0)
	{
		// Subtract neg stride offset, the no. of bytes from pixel 0,0 to the start of the surface
		ui32DevVAddr -= ((ui32Height - 1) * (IMG_UINT32) (0 - psSurf->i32StrideInBytes));
	}

	/*
	 * TSP coords
	 */
	if (ui32NumLayers > 0)
	{
		sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv(ui32Left, ui32Width);
		sTSPCoords.ui32Src0U1 = SGXTQ_FloatIntDiv(ui32Right, ui32Width);
		sTSPCoords.ui32Src0V0 = SGXTQ_FloatIntDiv(ui32Top, ui32Height);
		sTSPCoords.ui32Src0V1 = SGXTQ_FloatIntDiv(ui32Bottom, ui32Height);
	}

	switch (psSurf->eFormat)
	{
		case PVRSRV_PIXEL_FORMAT_R32:
		case PVRSRV_PIXEL_FORMAT_R32_SINT:
		case PVRSRV_PIXEL_FORMAT_R32_UINT:

		case PVRSRV_PIXEL_FORMAT_G32R32:
		case PVRSRV_PIXEL_FORMAT_G32R32_SINT:
		case PVRSRV_PIXEL_FORMAT_G32R32_UINT:
		case PVRSRV_PIXEL_FORMAT_GR32:

		case PVRSRV_PIXEL_FORMAT_B32G32R32:
		case PVRSRV_PIXEL_FORMAT_B32G32R32_SINT:
		case PVRSRV_PIXEL_FORMAT_B32G32R32_UINT:
		case PVRSRV_PIXEL_FORMAT_BGR32:

		case PVRSRV_PIXEL_FORMAT_A32B32G32R32:
		case PVRSRV_PIXEL_FORMAT_A32B32G32R32_SINT:
		case PVRSRV_PIXEL_FORMAT_A32B32G32R32_UINT:
		case PVRSRV_PIXEL_FORMAT_ABGR32:
		{
			ui32Colour = psQueueTransfer->Details.sFill.ui32Colour;
			ui32Colour = (ui32Colour >> (((2-ui32Pass) & 3) << 3)) & 0xff;
			ui32Colour *= 0x1010101;
			break;
		}

		case PVRSRV_PIXEL_FORMAT_R32F:
		case PVRSRV_PIXEL_FORMAT_G32R32F:
		case PVRSRV_PIXEL_FORMAT_B32G32R32F:
		case PVRSRV_PIXEL_FORMAT_A32B32G32R32F:
		{
			ui32Colour = psQueueTransfer->Details.sFill.ui32Colour;
			ui32Colour = (ui32Colour >> (((2-ui32Pass) & 3) << 3)) & 0xff;
			ui32Colour = SGXTQ_FloatIntDiv(ui32Colour, 255);
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ABGR16:
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16:
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16_SINT:
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16_UINT:
		{
			IMG_UINT32 ui32InCol = psQueueTransfer->Details.sFill.ui32Colour;

			/*R16:G16 - B16:A16*/
			if (ui32Pass == 0)
			{
				/* Take the r and g channels */
				ui32Colour = ((ui32InCol & 0x0000FF00) << 8) * 257;
				ui32Colour |= ((ui32InCol & 0x00FF0000) >> 16) * 257;
			}
			else
			{
				/* Take the top a and b channels */
				ui32Colour = ((ui32InCol & 0xFF000000) >> 8) * 257;
				ui32Colour |= (ui32InCol & 0x000000FF) * 257;
			}
			break;
		}
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16F:
		{
			IMG_UINT32 ui32InCol = psQueueTransfer->Details.sFill.ui32Colour;
			IMG_UINT32 ui32ColF16 = 0;

					
			
			/*R16:G16 - B16:A16*/
			if (ui32Pass == 0)
			{
				/* Take the r and g channels */
				ui32ColF16 = SGXTQ_ByteToF16((ui32InCol & 0x0000FF00) >> 8);
				ui32Colour = ui32ColF16 << 16;
				ui32ColF16 = SGXTQ_ByteToF16((ui32InCol & 0x00FF0000) >> 16);
				ui32Colour |= ui32ColF16;
			}
			else
			{
				/* Take the top a and b channels */
				ui32ColF16 = SGXTQ_ByteToF16((ui32InCol & 0xFF000000) >> 24);
				ui32Colour = ui32ColF16 << 16;
				ui32ColF16 = SGXTQ_ByteToF16(ui32InCol & 0x000000FF);
				ui32Colour |= ui32ColF16;
			}
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ABGR16F:
		{
			IMG_UINT32 ui32InCol = psQueueTransfer->Details.sFill.ui32Colour;

			if (ui32Pass == 0)
			{
				/* Take the r and g channels */
				ui32Colour = SGXTQ_FixedToF16(SGXTQ_FixedIntDiv((IMG_UINT16) ((ui32InCol & 0x0000FF00) >> 8), 255)) << 16;
				ui32Colour |= SGXTQ_FixedToF16(SGXTQ_FixedIntDiv((IMG_UINT16) ((ui32InCol & 0x00FF0000) >> 16), 255));
			}
			else
			{
				/* Take the top a and b channels */
				ui32Colour = SGXTQ_FixedToF16(SGXTQ_FixedIntDiv((IMG_UINT16) ((ui32InCol & 0xFF000000) >> 24), 255)) << 16;
				ui32Colour |= SGXTQ_FixedToF16(SGXTQ_FixedIntDiv((IMG_UINT16) (ui32InCol & 0x000000FF), 255));
			}
			break;
		}
		case PVRSRV_PIXEL_FORMAT_A8B8G8R8_SNORM:
		{
			IMG_UINT32 ui32InCol = psQueueTransfer->Details.sFill.ui32Colour;
			IMG_UINT32 i;

			for (i = 0; i < ui32BytesPerPixel; i++)
			{
				IMG_UINT32 ui32Channel = (ui32InCol >> (i * 8)) & 0xff;

				/* for [0, 127] => [0, 127] ; for [-128, -1] => [128, 1]*/
				if (ui32Channel > 127)
				{
					ui32Colour |= ((~ui32Channel + 1) & 0xff) << (i * 8);
				}
				else
				{
					ui32Colour |= ui32Channel << (i * 8);
				}
			}
			break;

		}
		case PVRSRV_PIXEL_FORMAT_A8B8G8R8_SINT:
		{
			/* sign at bit 7 in each (7 downto 0) 8 bit channel with 2 complement representation 
			 * where unsigned 0 == signed -128 and unsigned 255 == signed 127. We flip MSB per 8
			 * bit channel.
			 */
			ui32Colour = psQueueTransfer->Details.sFill.ui32Colour ^ 0x80808080;
			break;
		}

		default :
		{
			if (*pui32PassesRequired == 1)
			{
				/* The fill colour should always be in ARGB8888 format. */
				ui32Colour = psQueueTransfer->Details.sFill.ui32Colour;

				if (psSurf->eFormat == PVRSRV_PIXEL_FORMAT_GR88)
				{
					ui32Colour = ((ui32Colour >> 16) & 0xff) | (ui32Colour & 0xff00);
				}
				/* default 1 pass route */
				break;
			}
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}
	}
			
	/*
	 * Set up the PBE
	 */
	eError = SGXTQ_SetPBEState(psRect,
							psSurf->eMemLayout,
							ui32Width,
							ui32Height,
							ui32LineStride,
							0, //ui32PBEPackMode,
							ui32DevVAddr,
							0, //ui32SrcSel,
							SGXTQ_ROTATION_NONE,
							((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE),
							IMG_TRUE,
							pui32PBEState);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	/* create EOT */
	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									pui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* create pixev */
	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* Create the shader.
	 * The normal fill blit uses a LIMM for the fill colour.
	 * The ROP fills allocate a SA for the fill colour as only 16 bits are
	 * available for immediate values in the bitwise instructions.
	 */
	switch (eUSEProg)
	{
		case SGXTQ_USEBLIT_FILL:
		{
			eError = SGXTQ_CreateUSEResource(psTQContext,
					eUSEProg,
					&ui32Colour,
					psQueueTransfer->bPDumpContinuous);
			break;
		}
		default:
		{
			eError = SGXTQ_CreateUSEResource(psTQContext,
					eUSEProg,
					IMG_NULL,
					psQueueTransfer->bPDumpContinuous);
		}
	}
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	psUSEResource = &psTQContext->apsUSEResources[eUSEProg]->uResource.sUSE;


	/*
	 * Create the PDS primary
	 */
	SGXTQ_SetUSSEKick(psPDSValues,
					psTQContext->apsUSEResources[eUSEProg]->sDevVAddr,
					psTQContext->sUSEExecBase,
					psUSEResource->ui32NumTempRegs);

	switch (ePDSPrim)
	{
		case SGXTQ_PDSPRIMFRAG_SINGLESOURCE:
		{
			SGXTQ_SetTAGState(psPDSValues,
							  0,
							  ui32DevVAddr,
							  SGXTQ_FILTERTYPE_POINT,
							  ui32Width,
							  ui32Height,
							  ui32LineStride,
							  0, //ui32TAGFormat,
							  ui32BytesPerPixel,
							  IMG_TRUE,
							  psSurf->eMemLayout);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			psPDSValues->ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
			psPDSValues->ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

			psPDSValues->asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
															EURASIA_PDS_DOUTI_TEXISSUE_TC0	|
															EURASIA_PDS_DOUTI_TEXLASTISSUE;

			break;
		}
		case SGXTQ_PDSPRIMFRAG_KICKONLY:
		{
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "PrepareFillBlit: Unknown PDS primary: %d", (IMG_UINT32) ePDSPrim));
			PVR_DBG_BREAK;
			return PVRSRV_ERROR_UNKNOWN_PRIMARY_FRAG;
		}
	}

	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			ePDSPrim,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	
	/*
	 * Setup the secondary programs.
	 */
	switch (eUSEProg)
	{
		case SGXTQ_USEBLIT_FILL:
		{
			ePDSSec = SGXTQ_PDSSECFRAG_BASIC;
			break;
		} /* 'SGXTQ_USEBLIT_FILL' */
		default:
		{
			ePDSSec = SGXTQ_PDSSECFRAG_1ATTR;

			/* set up the colour in sa0 */
			psPDSValues->aui32A[0] = ui32Colour;
		} /* 'default' */
	}

	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
			ePDSSec,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSSecResource(psTQContext,
										ePDSSec,
										psPDSValues,
										psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * Create the ISP stream
	 */
	eError = SGXTQ_CreateISPResource(psTQContext,
									psTQContext->apsPDSPrimResources[ePDSPrim],
									psTQContext->apsPDSSecResources[ePDSSec],
									psRect,
									&sTSPCoords,
									IMG_FALSE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									ui32NumLayers,
									psQueueTransfer->bPDumpContinuous,
									0);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}



	eError = SGXTQ_SetupTransferClipRenderBox(psSubmit,
											ui32Left,
											ui32Top,
											ui32Right,
											ui32Bottom,
											ui32Width,
											ui32Height);

	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	SGXTQ_SetupTransferRegs(psTQContext,
#if defined(SUPPORT_DISPLAYCONTROLLER_TILING)
			psQueueTransfer->Details.sBlit.ui32BIFTile0Config,
#else
			0,
#endif
			psSubmit,
			psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
			ui32NumLayers,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
			EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
			0,
#endif
			EUR_CR_ISP_RENDER_TYPE_FASTSCALE);


	/* Specific setup stuff */
	psSubmit->bLoadFIRCoefficients = IMG_FALSE;

	return PVRSRV_OK;
}


static PVRSRV_ERROR PrepareBufferBlt(SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext,
									 SGX_QUEUETRANSFER *psQueueTransfer,
									 IMG_UINT32 ui32Pass,
									 SGXTQ_PREP_INTERNAL *psPassData,
									 SGXMKIF_TRANSFERCMD * psSubmit,
									 PVRSRV_TRANSFER_SGX_KICK *psKick,
									 IMG_UINT32 *pui32PassesRequired)
{
	SGXTQ_PDS_UPDATE sPDSValues;
	SGXTQ_TSP_COORDS sTSPCoords;
	IMG_UINT32 aui32PBEState[SGXTQ_PBESTATE_WORDS];

	IMG_UINT32 ui32SrcStart;
	IMG_UINT32 ui32SrcAlignBytes;
	IMG_UINT32 ui32DstStart;
	IMG_UINT32 ui32DstAlignBytes;

	IMG_BOOL bInterleaved;
	IMG_UINT32 ui32BytesLeft;
	IMG_UINT32 ui32Stride;
	IMG_UINT32 ui32Height;
	IMG_RECT sDstRect;

	PVRSRV_ERROR eError;

	/* Set up the sync infos - can't fail before this */
	if (psQueueTransfer->asSources[0].psSyncInfo)
	{
		PVRSRV_CLIENT_SYNC_INFO * psSyncInfo;

		psKick->ui32NumSrcSync = 1;
		psSyncInfo = psQueueTransfer->asSources[0].psSyncInfo;
		psKick->ahSrcSyncInfo[0] = psSyncInfo->hKernelSyncInfo;
	}
	else
	{
		psKick->ui32NumSrcSync = 0;
		psKick->ahSrcSyncInfo[0] = IMG_NULL;
	}

	if (psQueueTransfer->asDests[0].psSyncInfo)
	{
		PVRSRV_CLIENT_SYNC_INFO * psSyncInfo;

		psKick->ui32NumDstSync = 1;
		psSyncInfo = psQueueTransfer->asDests[0].psSyncInfo;
		psKick->ahDstSyncInfo[0] = psSyncInfo->hKernelSyncInfo;

	}
	else
	{
		psKick->ui32NumDstSync = 0;
		psKick->ahDstSyncInfo[0] = IMG_NULL;
	}

	if (psQueueTransfer->ui32NumSources != 1 ||
		psQueueTransfer->ui32NumDest != 1)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (ui32Pass == 0)
	{
		bInterleaved = IMG_FALSE;
		ui32SrcStart = SGX_TQSURFACE_GET_DEV_VADDR(psQueueTransfer->asSources[0]);
		ui32DstStart = SGX_TQSURFACE_GET_DEV_VADDR(psQueueTransfer->asDests[0]);
		ui32BytesLeft = psQueueTransfer->Details.sBufBlt.ui32Bytes;

		psPassData->Details.sBufBltData.ui32SrcStart = ui32SrcStart;
		psPassData->Details.sBufBltData.ui32DstStart = ui32DstStart;
		psPassData->Details.sBufBltData.ui32BytesLeft = ui32BytesLeft;
		psPassData->Details.sBufBltData.bInterleaved = bInterleaved;
	}
	else
	{
		ui32SrcStart = psPassData->Details.sBufBltData.ui32SrcStart;
		ui32DstStart = psPassData->Details.sBufBltData.ui32DstStart;
		ui32BytesLeft = psPassData->Details.sBufBltData.ui32BytesLeft;
		bInterleaved = psPassData->Details.sBufBltData.bInterleaved;
	}


	ui32SrcAlignBytes = ui32SrcStart & ((1 << EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) - 1);
	ui32DstAlignBytes = ui32DstStart & ((1 << EURASIA_PIXELBE_FBADDR_ALIGNSHIFT) - 1);
	ui32Stride = MAX(ui32SrcAlignBytes, ui32DstAlignBytes) + ui32BytesLeft;
	ui32Stride = (ui32Stride + 31UL) & ~31UL;
	if (ui32Stride == 0)
	{
		/* can't copy 0 bytes */
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (!bInterleaved && ui32Stride <= EURASIA_RENDERSIZE_MAXX)
	{
		*pui32PassesRequired = ui32Pass + 1;

		sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv(ui32SrcAlignBytes, ui32Stride);
		sTSPCoords.ui32Src0V0 = 0;
		sTSPCoords.ui32Src0U1 = SGXTQ_FloatIntDiv(ui32BytesLeft + ui32SrcAlignBytes, ui32Stride);
		sTSPCoords.ui32Src0V1 = FLOAT32_ONE;

		ui32Height = 1;

		sDstRect.x0 = (IMG_INT32) ui32DstAlignBytes;
		sDstRect.y0 = 0;
		sDstRect.x1 = (IMG_INT32) (ui32DstAlignBytes + ui32BytesLeft);
		sDstRect.y1 = 1;

	}
	else
	{
		IMG_UINT32 ui32Width;

		ui32Stride = EURASIA_RENDERSIZE_MAXX;
		ui32Width = EURASIA_RENDERSIZE_MAXX >> 1;
		ui32Height = MIN(EURASIA_RENDERSIZE_MAXY, ui32BytesLeft / ui32Stride);
		if (ui32Height == 0)
		{
			ui32Height = 1;
		}

		if (!bInterleaved)
		{
			*pui32PassesRequired = ui32Pass + 2;

			sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv(ui32SrcAlignBytes, ui32Stride);
			sTSPCoords.ui32Src0V0 = 0;
			sTSPCoords.ui32Src0U1 = FLOAT32_HALF;
			sTSPCoords.ui32Src0V1 = FLOAT32_ONE;

			sDstRect.x0 = (IMG_INT32) ui32DstAlignBytes;
			sDstRect.y0 = 0;
			sDstRect.x1 = (IMG_INT32) (ui32DstAlignBytes + ui32Width - ui32SrcAlignBytes);
			sDstRect.y1 = (IMG_INT32) ui32Height;
		}
		else
		{
			IMG_UINT32 ui32GapSize;
			IMG_UINT32 ui32SizeDone = ui32Stride * ui32Height;


			ui32GapSize = ui32SrcStart + ui32Stride;
			ui32SrcStart += ui32Width - ui32SrcAlignBytes;
			ui32DstStart += ui32Width - ui32SrcAlignBytes;
			ui32GapSize -= ui32SrcStart;

			ui32SrcAlignBytes = ui32SrcStart & ((1 << EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) - 1);
			ui32DstAlignBytes = ui32DstStart & ((1 << EURASIA_PIXELBE_FBADDR_ALIGNSHIFT) - 1);

			sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv(ui32SrcAlignBytes, ui32Stride);
			sTSPCoords.ui32Src0V0 = 0;
			sTSPCoords.ui32Src0U1 = SGXTQ_FloatIntDiv(ui32SrcAlignBytes + ui32GapSize, ui32Stride);
			sTSPCoords.ui32Src0V1 = FLOAT32_ONE;

			sDstRect.x0 = (IMG_INT32) ui32DstAlignBytes;
			sDstRect.y0 = 0;
			sDstRect.x1 = (IMG_INT32) (ui32DstAlignBytes + ui32GapSize);
			sDstRect.y1 = (IMG_INT32) ui32Height;

			if (ui32BytesLeft <= ui32SizeDone)
			{
				*pui32PassesRequired = ui32Pass + 1;
			}
			else
			{
				*pui32PassesRequired = ui32Pass + 2;
				psPassData->Details.sBufBltData.ui32BytesLeft -= ui32SizeDone;
				psPassData->Details.sBufBltData.ui32SrcStart += ui32SizeDone;
				psPassData->Details.sBufBltData.ui32DstStart += ui32SizeDone;
			}
		}
		psPassData->Details.sBufBltData.bInterleaved = (psPassData->Details.sBufBltData.bInterleaved != IMG_FALSE) ? IMG_FALSE : IMG_TRUE;
	}


	PVRSRVMemSet(& sPDSValues.asLayers[0].aui32TAGState,
			0,
			sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);

	SGXTQ_SetTAGState(&sPDSValues,
					0,
					ui32SrcStart - ui32SrcAlignBytes,
					SGXTQ_FILTERTYPE_POINT,
					ui32Stride,
					ui32Height,
					ui32Stride,
					EURASIA_PDS_DOUTT1_TEXFORMAT_U8,
					1,
					IMG_FALSE,
					SGXTQ_MEMLAYOUT_STRIDE);

	PVRSRVMemSet(aui32PBEState, 0, sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);
	eError = SGXTQ_SetPBEState(& sDstRect,
							SGXTQ_MEMLAYOUT_OUT_LINEAR,
							(IMG_UINT32) sDstRect.x1,
							ui32Height,
							ui32Stride,
#if defined(EURASIA_PIXELBE2SB_PACKMODE_MONO8)
							EURASIA_PIXELBE2SB_PACKMODE_MONO8,
#else
#if defined(EURASIA_PIXELBES0LO_PACKMODE_MONO8)
							EURASIA_PIXELBES0LO_PACKMODE_MONO8,
#else
							EURASIA_PIXELBE2S0_PACKMODE_MONO8,
#endif
#endif
							ui32DstStart - ui32DstAlignBytes,
							3,
							SGXTQ_ROTATION_NONE,
							((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE),
							IMG_FALSE,
							aui32PBEState);

	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	/* create the shader*/
	eError = SGXTQ_CreateUSEResource(psTQContext,
			SGXTQ_USEBLIT_NORMAL,
			IMG_NULL,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	sPDSValues.asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
												EURASIA_PDS_DOUTI_TEXISSUE_TC0	|
												EURASIA_PDS_DOUTI_TEXLASTISSUE;

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
	sPDSValues.asLayers[0].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT)	|
												(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif

	SGXTQ_SetUSSEKick(&sPDSValues,
					psTQContext->apsUSEResources[SGXTQ_USEBLIT_NORMAL]->sDevVAddr,
					psTQContext->sUSEExecBase,
					psTQContext->apsUSEResources[SGXTQ_USEBLIT_NORMAL]->uResource.sUSE.ui32NumTempRegs);


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sPDSValues.ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
	sPDSValues.ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			SGXTQ_PDSPRIMFRAG_SINGLESOURCE,
			&sPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * Create PDS secondary program
	 */
	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
			SGXTQ_PDSSECFRAG_BASIC,
			& sPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSSecResource(psTQContext,
			SGXTQ_PDSSECFRAG_BASIC,
			&sPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * Create ISP stream
	 */
	eError = SGXTQ_CreateISPResource(psTQContext,
									psTQContext->apsPDSPrimResources[SGXTQ_PDSPRIMFRAG_SINGLESOURCE],
									psTQContext->apsPDSSecResources[SGXTQ_PDSSECFRAG_BASIC],
									& sDstRect,
									& sTSPCoords,
									IMG_FALSE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									1,
									psQueueTransfer->bPDumpContinuous,
									0);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	/* create EOT */
	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									aui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* create pixev */
	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	SGXTQ_SetupTransferRenderBox(psSubmit,
								(IMG_UINT32) sDstRect.x0, (IMG_UINT32) sDstRect.y0,
								(IMG_UINT32) sDstRect.x1, (IMG_UINT32) sDstRect.y1);

	SGXTQ_SetupTransferRegs(psTQContext,
							0,
							psSubmit,
							psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
							1,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
		EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
		0,
#endif
		EUR_CR_ISP_RENDER_TYPE_FASTSCALE);

	return PVRSRV_OK;
}


static PVRSRV_ERROR PrepareCustomOp(SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext,
									SGX_QUEUETRANSFER *psQueueTransfer,
									IMG_UINT32 ui32Pass,
									SGXTQ_PREP_INTERNAL *psPassData,
									SGXMKIF_TRANSFERCMD * psSubmit,
									PVRSRV_TRANSFER_SGX_KICK *psKick,
									IMG_UINT32 *pui32PassesRequired)
{
	IMG_UINT32 i;
	SGXTQ_TSP_COORDS sTSPCoords;
	SGXTQ_RESOURCE sPrimary;
	SGXTQ_RESOURCE sSecondary;
	PVRSRV_ERROR eError;

	SGXTQ_SURFACE * psDest;
	IMG_RECT * psDestRect;

	PVR_DPF((PVR_DBG_VERBOSE, "\t-> Custom blit (specifying PDS and USE code)"));

	PVR_UNREFERENCED_PARAMETER(ui32Pass);
	PVR_UNREFERENCED_PARAMETER(psPassData);

	/* can't fail before the sync copy */
	psKick->ui32NumSrcSync = 0;
	psKick->ui32NumDstSync = 0;
	for (i = 0; i < psQueueTransfer->ui32NumSources; i++)
	{
		if (psQueueTransfer->asSources[i].psSyncInfo)
		{
			PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asSources[i].psSyncInfo;

			psKick->ahSrcSyncInfo[psKick->ui32NumSrcSync] = psSyncInfo->hKernelSyncInfo;
			psKick->ui32NumSrcSync++;
		}
	}
	for (i = 0; i < psQueueTransfer->ui32NumDest; i++)
	{
		if (psQueueTransfer->asDests[i].psSyncInfo)
		{
			PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asDests[i].psSyncInfo;

			psKick->ahDstSyncInfo[psKick->ui32NumDstSync] = psSyncInfo->hKernelSyncInfo;
			psKick->ui32NumDstSync++;
		}
	}



	if (psQueueTransfer->ui32NumSources >= SGXTQ_NUM_HWBGOBJS ||
		psQueueTransfer->ui32NumDest != 1)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/*
	 * set up the tsp coordinates
	 */
	/* TODO wouldn't it be better just to use an array?*/
	if (psQueueTransfer->ui32NumSources > 0)
	{
		IMG_RECT * psRect = & SGX_QUEUETRANSFER_SRC_RECT(*psQueueTransfer, 0);

		IMG_UINT32 ui32SrcWidth = psQueueTransfer->asSources[0].ui32Width;
		IMG_UINT32 ui32SrcHeight = psQueueTransfer->asSources[0].ui32Height;

		sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->x0, ui32SrcWidth);
		sTSPCoords.ui32Src0U1 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->x1, ui32SrcWidth);
		sTSPCoords.ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->y0, ui32SrcHeight);
		sTSPCoords.ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->y1, ui32SrcHeight);
	}
	if (psQueueTransfer->ui32NumSources > 1)
	{
		IMG_RECT * psRect = & SGX_QUEUETRANSFER_SRC_RECT(*psQueueTransfer, 1);
		IMG_UINT32 ui32SrcWidth = psQueueTransfer->asSources[1].ui32Width;
		IMG_UINT32 ui32SrcHeight = psQueueTransfer->asSources[1].ui32Height;

		sTSPCoords.ui32Src1U0 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->x0, ui32SrcWidth);
		sTSPCoords.ui32Src1U1 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->x1, ui32SrcWidth);
		sTSPCoords.ui32Src1V0 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->y0, ui32SrcHeight);
		sTSPCoords.ui32Src1V1 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->y1, ui32SrcHeight);
	}
	if (psQueueTransfer->ui32NumSources > 2)
	{
		IMG_RECT * psRect = & SGX_QUEUETRANSFER_SRC_RECT(*psQueueTransfer, 2);
		IMG_UINT32 ui32SrcWidth = psQueueTransfer->asSources[2].ui32Width;
		IMG_UINT32 ui32SrcHeight = psQueueTransfer->asSources[2].ui32Height;

		sTSPCoords.ui32Src2U0 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->x0, ui32SrcWidth);
		sTSPCoords.ui32Src2U1 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->x1, ui32SrcWidth);
		sTSPCoords.ui32Src2V0 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->y0, ui32SrcHeight);
		sTSPCoords.ui32Src2V1 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->y1, ui32SrcHeight);
	}

	psDest = &psQueueTransfer->asDests[0];
	psDestRect = & SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0);

	eError = SGXTQ_SetupTransferClipRenderBox(psSubmit,
												(IMG_UINT32) psDestRect->x0, (IMG_UINT32) psDestRect->y0,
												(IMG_UINT32) psDestRect->x1, (IMG_UINT32) psDestRect->y1,
												psDest->ui32Width, psDest->ui32Height);
	
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * Create the ISP stream
	 */
	/* copy the primary info */
	sPrimary.eStorage = SGXTQ_STORAGE_FOREIGN;
	sPrimary.sDevVAddr = psQueueTransfer->Details.sCustom.sDevVAddrPDSPrimCode;
	sPrimary.eResource = SGXTQ_PDS;
	sPrimary.uResource.sPDS.ui32DataLen = psQueueTransfer->Details.sCustom.ui32PDSPrimDataSize;
	sPrimary.uResource.sPDS.ui32Attributes = psQueueTransfer->Details.sCustom.ui32PDSPrimNumAttr;
#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	sPrimary.uResource.sPDS.ui32TempRegs = psQueueTransfer->Details.sCustom.ui32NumTempRegs;
#endif
	/* copy the secondary info */
	sSecondary.eStorage = SGXTQ_STORAGE_FOREIGN;
	sSecondary.sDevVAddr = psQueueTransfer->Details.sCustom.sDevVAddrPDSSecCode;
	sSecondary.eResource = SGXTQ_PDS;
	sSecondary.uResource.sPDS.ui32DataLen = psQueueTransfer->Details.sCustom.ui32PDSSecDataSize;
	sSecondary.uResource.sPDS.ui32Attributes = psQueueTransfer->Details.sCustom.ui32PDSSecNumAttr;

	/* and create the stream */
	eError = SGXTQ_CreateISPResource(psTQContext,
									& sPrimary,
									& sSecondary,
									psDestRect,
									&sTSPCoords,
									IMG_TRUE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									psQueueTransfer->ui32NumSources,
									psQueueTransfer->bPDumpContinuous,
									0);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * Create Pixevent handlers
	 */
	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									psQueueTransfer->Details.sCustom.aui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	SGXTQ_SetupTransferRegs(psTQContext,
#if defined(SUPPORT_DISPLAYCONTROLLER_TILING)
			psQueueTransfer->Details.sCustom.ui32BIFTile0Config,
#else
			0,
#endif
			psSubmit,
			psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
			psQueueTransfer->ui32NumSources,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
			EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
			0,
#endif
			EUR_CR_ISP_RENDER_TYPE_FASTSCALE);


	/* FIR coefficients */
	psSubmit->bLoadFIRCoefficients = psQueueTransfer->Details.sCustom.bLoadFIRCoefficients;
	if (psSubmit->bLoadFIRCoefficients)
	{
		psSubmit->sHWRegs.ui32FIRHFilterTable	= psQueueTransfer->Details.sCustom.ui32FIRHFilterTable;
		psSubmit->sHWRegs.ui32FIRHFilterLeft0	= psQueueTransfer->Details.sCustom.ui32FIRHFilterLeft0;
		psSubmit->sHWRegs.ui32FIRHFilterRight0	= psQueueTransfer->Details.sCustom.ui32FIRHFilterRight0;
		psSubmit->sHWRegs.ui32FIRHFilterExtra0	= psQueueTransfer->Details.sCustom.ui32FIRHFilterExtra0;
		psSubmit->sHWRegs.ui32FIRHFilterLeft1	= psQueueTransfer->Details.sCustom.ui32FIRHFilterLeft1;
		psSubmit->sHWRegs.ui32FIRHFilterRight1	= psQueueTransfer->Details.sCustom.ui32FIRHFilterRight1;
		psSubmit->sHWRegs.ui32FIRHFilterExtra1	= psQueueTransfer->Details.sCustom.ui32FIRHFilterExtra1;
		psSubmit->sHWRegs.ui32FIRHFilterLeft2	= psQueueTransfer->Details.sCustom.ui32FIRHFilterLeft2;
		psSubmit->sHWRegs.ui32FIRHFilterRight2	= psQueueTransfer->Details.sCustom.ui32FIRHFilterRight2;
		psSubmit->sHWRegs.ui32FIRHFilterExtra2	= psQueueTransfer->Details.sCustom.ui32FIRHFilterExtra2;
#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
		psSubmit->sHWRegs.ui32FIRHFilterCentre0	= psQueueTransfer->Details.sCustom.ui32FIRHFilterCentre0;
		psSubmit->sHWRegs.ui32FIRHFilterCentre1	= psQueueTransfer->Details.sCustom.ui32FIRHFilterCentre1;
#endif
	}

	/* Copy client-requested updates across */
	if (psQueueTransfer->Details.sCustom.ui32NumPatches > SGXTQ_MAX_UPDATES)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	for (i = 0; i < psQueueTransfer->Details.sCustom.ui32NumPatches; i++)
	{
		psSubmit->sUpdates[psSubmit->ui32NumUpdates].sUpdateAddr.uiAddr = psQueueTransfer->Details.sCustom.asMemUpdates[i].ui32UpdateAddr;
		psSubmit->sUpdates[psSubmit->ui32NumUpdates].ui32UpdateVal = psQueueTransfer->Details.sCustom.asMemUpdates[i].ui32UpdateVal;
		psSubmit->ui32NumUpdates++;
	}
	

	/* Custom blits are always 1 pass */
	*pui32PassesRequired = 1;

	return PVRSRV_OK;
}


static PVRSRV_ERROR PrepareFullCustomOp(SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext,
										SGX_QUEUETRANSFER *psQueueTransfer,
										IMG_UINT32 ui32Pass,
										SGXTQ_PREP_INTERNAL *psPassData,
										SGXMKIF_TRANSFERCMD *psSubmit,
										PVRSRV_TRANSFER_SGX_KICK *psKick,
										IMG_UINT32 *pui32PassesRequired)
{
	IMG_UINT32 i;
	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo;
	PVRSRV_ERROR eError;

#if defined(FIX_HW_BRN_33029)	
	IMG_PUINT32	pui32RACpuVAddr = IMG_NULL; 
	IMG_UINT32	ui32RADevVAddr;
#endif

	PVR_UNREFERENCED_PARAMETER(ui32Pass);
	PVR_UNREFERENCED_PARAMETER(psPassData);

	PVR_DPF((PVR_DBG_VERBOSE, "\t-> Custom blit (specifying vertex data)"));

	/* Set up the sync infos - can't fail before this*/
	psKick->ui32NumSrcSync = 0;
	psKick->ui32NumDstSync = 0;

	for (i = 0; i < psQueueTransfer->ui32NumSources; i++)
	{
		if (psQueueTransfer->asSources[i].psSyncInfo)
		{
			psSyncInfo = psQueueTransfer->asSources[i].psSyncInfo;

			psKick->ahSrcSyncInfo[psKick->ui32NumSrcSync] = psSyncInfo->hKernelSyncInfo;

			psKick->ui32NumSrcSync++;
		}
	}
	for (i = 0; i < psQueueTransfer->ui32NumDest; i++)
	{
		if (psQueueTransfer->asDests[i].psSyncInfo)
		{
			psSyncInfo = psQueueTransfer->asDests[i].psSyncInfo;
			psKick->ahDstSyncInfo[psKick->ui32NumDstSync] = psSyncInfo->hKernelSyncInfo;

			psKick->ui32NumDstSync++;
		}
	}
	/* create EOT */
	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									psQueueTransfer->Details.sFullCustom.aui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,	
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* create pixev */
	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	SGXTQ_SetupPixeventRegs(psTQContext,
							psSubmit,
							psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC]);

	psSubmit->sHWRegs.ui32ISPBgObjTag	= psQueueTransfer->Details.sFullCustom.ui32ISPBgObjTagReg;
	psSubmit->sHWRegs.ui32ISPBgObj		= psQueueTransfer->Details.sFullCustom.ui32ISPBgObjReg;
	psSubmit->sHWRegs.ui32ISPBgObjDepth	= FLOAT32_ONE; /* expose ?*/
	psSubmit->sHWRegs.ui32ISPRender		= psQueueTransfer->Details.sFullCustom.ui32ISPRenderReg;
	psSubmit->sHWRegs.ui32ISPRgnBase	= psQueueTransfer->Details.sFullCustom.ui32ISPRgnBaseReg;
	psSubmit->sHWRegs.ui32Bif3DReqBase	= psQueueTransfer->Details.sFullCustom.ui32BIFBase;

	psSubmit->sHWRegs.ui32ISPIPFMisc    = EUR_CR_ISP_IPFMISC_PROCESSEMPTY_MASK; /* expose ? */
	psSubmit->sHWRegs.ui323DAAMode		= 0; /* expose ? */
	psSubmit->sHWRegs.ui32ISPMultiSampCtl = (0x8UL << EUR_CR_MTE_MULTISAMPLECTL_MSAA_X0_SHIFT) |
											(0x8UL << EUR_CR_MTE_MULTISAMPLECTL_MSAA_Y0_SHIFT);

	psSubmit->sHWRegs.ui32PixelBE		=	(0x80UL << EUR_CR_PIXELBE_ALPHATHRESHOLD_SHIFT);

#if defined(SUPPORT_DISPLAYCONTROLLER_TILING)
	psSubmit->sHWRegs.ui32BIFTile0Config = psQueueTransfer->Details.sFullCustom.ui32BIFTile0Config;
#endif

#if defined(FIX_HW_BRN_33029)	
	if(psQueueTransfer->Details.sFullCustom.ui32ISPRenderReg ==  EUR_CR_ISP_RENDER_TYPE_FAST2D)
	{		
		if(*pui32PassesRequired != 2)
		{
			if (! SGXTQ_AcquireCB(psTQContext->psFenceIDMemInfo,
					psTQContext->ui32FenceID,
					psTQContext->hOSEvent,
					psTQContext->psStreamCB,
					16,
					IMG_TRUE,
					(IMG_VOID **) &pui32RACpuVAddr,
					& ui32RADevVAddr,
					IMG_FALSE))
			{
				return PVRSRV_ERROR_TIMEOUT;
			}

			*pui32RACpuVAddr++ = EURASIA_REGIONHEADER0_LASTREGION;
			*pui32RACpuVAddr++ = psQueueTransfer->Details.sFullCustom.ui32ISPRgnBaseReg & 0x0FFFFFFF;
			*pui32RACpuVAddr++ = 0;
			*pui32RACpuVAddr = 0;

			psSubmit->sHWRegs.ui32ISPRender		= EUR_CR_ISP_RENDER_TYPE_NORMAL3D;
			psSubmit->sHWRegs.ui32ISPRgnBase	= (ui32RADevVAddr  >> EUR_CR_ISP_RGN_BASE_ADDR_ALIGNSHIFT) << EUR_CR_ISP_RGN_BASE_ADDR_SHIFT;	
		
			*pui32PassesRequired = 2;
		}

		else if(*pui32PassesRequired == 2)
		{
			psSubmit->sHWRegs.ui32ISPRender		= psQueueTransfer->Details.sFullCustom.ui32ISPRenderReg;
			psSubmit->sHWRegs.ui32ISPRgnBase	= psQueueTransfer->Details.sFullCustom.ui32ISPRgnBaseReg;
		}
	}
	else
	{
		psSubmit->sHWRegs.ui32ISPRender		= psQueueTransfer->Details.sFullCustom.ui32ISPRenderReg;
		psSubmit->sHWRegs.ui32ISPRgnBase	= psQueueTransfer->Details.sFullCustom.ui32ISPRgnBaseReg;
		*pui32PassesRequired = 1;
	}		
#else
	psSubmit->sHWRegs.ui32ISPRender	= psQueueTransfer->Details.sFullCustom.ui32ISPRenderReg;
	psSubmit->sHWRegs.ui32ISPRgnBase= psQueueTransfer->Details.sFullCustom.ui32ISPRgnBaseReg;
	*pui32PassesRequired = 1;
#endif

	SGXTQ_SetupTransferRenderBox(psSubmit,
								(IMG_UINT32) psQueueTransfer->Details.sFullCustom.sRenderBox.x0,
								(IMG_UINT32) psQueueTransfer->Details.sFullCustom.sRenderBox.y0,
								(IMG_UINT32) psQueueTransfer->Details.sFullCustom.sRenderBox.x1,
								(IMG_UINT32) psQueueTransfer->Details.sFullCustom.sRenderBox.y1);

#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
	/* YUV Coefficients */
	psSubmit->bLoadYUVCoefficients = psQueueTransfer->Details.sFullCustom.bLoadYUVCoefficients;
	if (psSubmit->bLoadYUVCoefficients)
	{
		psSubmit->sHWRegs.ui32CSC0Coeff01 = psQueueTransfer->Details.sFullCustom.ui32CSC0Coeff01;
		psSubmit->sHWRegs.ui32CSC0Coeff23 = psQueueTransfer->Details.sFullCustom.ui32CSC0Coeff23;
		psSubmit->sHWRegs.ui32CSC0Coeff45 = psQueueTransfer->Details.sFullCustom.ui32CSC0Coeff45;
		psSubmit->sHWRegs.ui32CSC0Coeff67 = psQueueTransfer->Details.sFullCustom.ui32CSC0Coeff67;
#if !defined(SGX_FEATURE_TAG_YUV_TO_RGB_HIPREC)
		psSubmit->sHWRegs.ui32CSC0Coeff8 = psQueueTransfer->Details.sFullCustom.ui32CSC0Coeff8;
#else
		psSubmit->sHWRegs.ui32CSC0Coeff89 = psQueueTransfer->Details.sFullCustom.ui32CSC0Coeff89;
		psSubmit->sHWRegs.ui32CSC0Coeff1011 = psQueueTransfer->Details.sFullCustom.ui32CSC0Coeff1011;
#endif

		psSubmit->sHWRegs.ui32CSC1Coeff01 = psQueueTransfer->Details.sFullCustom.ui32CSC1Coeff01;
		psSubmit->sHWRegs.ui32CSC1Coeff23 = psQueueTransfer->Details.sFullCustom.ui32CSC1Coeff23;
		psSubmit->sHWRegs.ui32CSC1Coeff45 = psQueueTransfer->Details.sFullCustom.ui32CSC1Coeff45;
		psSubmit->sHWRegs.ui32CSC1Coeff67 = psQueueTransfer->Details.sFullCustom.ui32CSC1Coeff67;
#if !defined(SGX_FEATURE_TAG_YUV_TO_RGB_HIPREC)
		psSubmit->sHWRegs.ui32CSC1Coeff8 = psQueueTransfer->Details.sFullCustom.ui32CSC1Coeff8;
#else
		psSubmit->sHWRegs.ui32CSC1Coeff89 = psQueueTransfer->Details.sFullCustom.ui32CSC1Coeff89;
		psSubmit->sHWRegs.ui32CSC1Coeff1011 = psQueueTransfer->Details.sFullCustom.ui32CSC1Coeff1011;
#endif

		psSubmit->sHWRegs.ui32CSCScale = psQueueTransfer->Details.sFullCustom.ui32CSCScale;
	}
#endif

#if defined(SGX_FEATURE_TAG_LUMAKEY)
	/*LumaKey Coefficients*/
	psSubmit->bLoadLumaKeyCoefficients = psQueueTransfer->Details.sFullCustom.bLoadLumaKeyCoefficients;
	if (psSubmit->bLoadLumaKeyCoefficients)
	{
		psSubmit->sHWRegs.ui32LumaKeyCoeff0 = psQueueTransfer->Details.sFullCustom.ui32LumaKeyCoeff0;
		psSubmit->sHWRegs.ui32LumaKeyCoeff1 = psQueueTransfer->Details.sFullCustom.ui32LumaKeyCoeff1;
		psSubmit->sHWRegs.ui32LumaKeyCoeff2 = psQueueTransfer->Details.sFullCustom.ui32LumaKeyCoeff2;
	}
#endif

	/* FIR coefficients */
	psSubmit->bLoadFIRCoefficients = psQueueTransfer->Details.sFullCustom.bLoadFIRCoefficients;
	if (psSubmit->bLoadFIRCoefficients)
	{
		psSubmit->sHWRegs.ui32FIRHFilterTable	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterTable;
		psSubmit->sHWRegs.ui32FIRHFilterLeft0	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterLeft0;
		psSubmit->sHWRegs.ui32FIRHFilterRight0	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterRight0;
		psSubmit->sHWRegs.ui32FIRHFilterExtra0	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterExtra0;
		psSubmit->sHWRegs.ui32FIRHFilterLeft1	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterLeft1;
		psSubmit->sHWRegs.ui32FIRHFilterRight1	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterRight1;
		psSubmit->sHWRegs.ui32FIRHFilterExtra1	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterExtra1;
		psSubmit->sHWRegs.ui32FIRHFilterLeft2	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterLeft2;
		psSubmit->sHWRegs.ui32FIRHFilterRight2	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterRight2;
		psSubmit->sHWRegs.ui32FIRHFilterExtra2	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterExtra2;
#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
		psSubmit->sHWRegs.ui32FIRHFilterCentre0	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterCentre0;
		psSubmit->sHWRegs.ui32FIRHFilterCentre1	= psQueueTransfer->Details.sFullCustom.ui32FIRHFilterCentre1;
#endif
	}

	/* Copy client-requested updates across */
	if (psQueueTransfer->Details.sFullCustom.ui32NumPatches > SGXTQ_MAX_UPDATES)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	for (i = 0; i < psQueueTransfer->Details.sFullCustom.ui32NumPatches; i++)
	{
		psSubmit->sUpdates[psSubmit->ui32NumUpdates].sUpdateAddr.uiAddr = psQueueTransfer->Details.sFullCustom.asMemUpdates[i].ui32UpdateAddr;
		psSubmit->sUpdates[psSubmit->ui32NumUpdates].ui32UpdateVal = psQueueTransfer->Details.sFullCustom.asMemUpdates[i].ui32UpdateVal;
		psSubmit->ui32NumUpdates++;
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR PrepareCustomShaderBlit(SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext,
											SGX_QUEUETRANSFER				* psQueueTransfer,
											IMG_UINT32						ui32Pass,
											SGXTQ_PREP_INTERNAL				* psPassData,
											SGXMKIF_TRANSFERCMD				* psSubmit,
											PVRSRV_TRANSFER_SGX_KICK		* psKick,
											IMG_UINT32						* pui32PassesRequired)
{
	SGXTQ_PDS_UPDATE	* psPDSValues = & psPassData->sPDSUpdate;
	SGXTQ_TSP_COORDS	sTSPCoords;

	SGXTQ_SURF_DESC		asSources[SGXTQ_NUM_HWBGOBJS - 1];
	SGXTQ_SURF_DESC		sDest;
	IMG_UINT32			* pui32PBEState = psPassData->aui32PBEState;

	IMG_UINT32			i;
	SGXTQ_PDSPRIMFRAGS	ePDSPrim;
	SGXTQ_PDSSECFRAGS	ePDSSec = SGXTQ_PDSSECFRAG_BASIC;

	SGXTQ_SURFACE		* psDest;

	PVRSRV_ERROR		eError;

	/* Set up the sync infos - can't fail before this*/
	psKick->ui32NumSrcSync = 0;
	psKick->ui32NumDstSync = 0;

	for (i = 0; i < psQueueTransfer->ui32NumSources; i++)
	{
		if (psQueueTransfer->asSources[i].psSyncInfo)
		{
			PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asSources[i].psSyncInfo;

			psKick->ahSrcSyncInfo[psKick->ui32NumSrcSync] = psSyncInfo->hKernelSyncInfo;
			psKick->ui32NumSrcSync++;
		}
	}
	for (i = 0; i < psQueueTransfer->ui32NumDest; i++)
	{
		if (psQueueTransfer->asDests[i].psSyncInfo)
		{
			PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asDests[i].psSyncInfo;

			psKick->ahDstSyncInfo[psKick->ui32NumDstSync] = psSyncInfo->hKernelSyncInfo;
			psKick->ui32NumDstSync++;
		}
	}

	/* PRQA S 3415 1 */ /* ignore side effects warning */
	if ((psQueueTransfer->ui32NumSources >= SGXTQ_NUM_HWBGOBJS)
		|| (psQueueTransfer->ui32NumDest != 1)
#ifndef BLITLIB
		|| (psQueueTransfer->ui32NumSrcRects != psQueueTransfer->ui32NumSources)
		|| (psQueueTransfer->ui32NumDestRects != 1)
#endif
		)
	{
		PVR_TRACE(("SGXTQ_NUM_HWBGOBJS=%d, cond=%d", SGXTQ_NUM_HWBGOBJS,
					(psQueueTransfer->ui32NumSources >= SGXTQ_NUM_HWBGOBJS) || (psQueueTransfer->ui32NumDest != 1)));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDest = & psQueueTransfer->asDests[0];

	for (i = 0; i < psQueueTransfer->ui32NumSources; i++)
	{
		SGXTQ_SURFACE* psSurf = &psQueueTransfer->asSources[i];

		asSources[i].psSurf = psSurf;
		asSources[i].psRect = & SGX_QUEUETRANSFER_SRC_RECT(*psQueueTransfer, i);
		asSources[i].ui32DevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psSurf);
		asSources[i].ui32Height = psSurf->ui32Height;

		if (! ISALIGNED(asSources[i].ui32DevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}

		PVRSRVMemSet(psPDSValues->asLayers[i].aui32TAGState,
				0,
				sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);

		eError = SGXTQ_GetPixelFormats(psSurf->eFormat,
				(psSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
				PVRSRV_PIXEL_FORMAT_UNKNOWN,
				IMG_TRUE,
				SGXTQ_FILTERTYPE_POINT,
				psPDSValues->asLayers[i].aui32TAGState,
				IMG_NULL, /* bTAGPlanar? */
				& asSources[i].ui32BytesPerPixel,
				IMG_NULL,
				IMG_NULL,
				ui32Pass,
				IMG_NULL); /* passesReq */
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		/* PRQA S 3415 1 */ /* ignore side effects warning */
		if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psSurf,
												asSources[i].ui32BytesPerPixel,
												IMG_TRUE,
												IMG_FALSE,
												& asSources[i].ui32LineStride))
			|| (PVRSRV_OK != SGXTQ_GetSurfaceWidth(psSurf,
											asSources[i].ui32BytesPerPixel,
											IMG_TRUE,
											IMG_FALSE,
											& asSources[i].ui32Width)))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}


		
		SGXTQ_SetTAGState(psPDSValues,
						i,
						asSources[i].ui32DevVAddr,
						psQueueTransfer->Details.sCustomShader.aeFilter[i],
						asSources[i].ui32Width,
						asSources[i].ui32Height,
						asSources[i].ui32LineStride,
						0,	
						asSources[i].ui32BytesPerPixel,
						IMG_TRUE,
						psSurf->eMemLayout);

#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
		if (asSources[i].ui32TAGFormat == EURASIA_PDS_DOUTT1_TEXFORMAT_U8)
			psPDSValues->asLayers[i].aui32TAGState[0] |= EURASIA_PDS_DOUTT0_CHANREPLICATE;
#endif

		psPDSValues->asLayers[i].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE			|
#if defined(SGX545)
													((i+1) << EURASIA_PDS_DOUTI_TEXISSUE_SHIFT)	|
#else
													(i << EURASIA_PDS_DOUTI_TEXISSUE_SHIFT)		|
#endif
													((i == psQueueTransfer->ui32NumSources - 1) ?
													 EURASIA_PDS_DOUTI_TEXLASTISSUE :
													 0);


#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
		psPDSValues->asLayers[i].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT)	|
														(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif
	}

	sDest.psSurf = psDest; 
	sDest.psRect = & SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer,0);
	sDest.ui32DevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*sDest.psSurf);
	sDest.ui32Height = sDest.psSurf->ui32Height;

	if (! ISALIGNED(sDest.ui32DevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	PVRSRVMemSet(pui32PBEState, 0, sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);

	eError = SGXTQ_GetPixelFormats(PVRSRV_PIXEL_FORMAT_UNKNOWN,
			IMG_TRUE,
			psDest->eFormat,
			(psDest->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
			SGXTQ_FILTERTYPE_POINT,
			IMG_NULL,
			IMG_NULL,
			IMG_NULL,
			pui32PBEState,
			& sDest.ui32BytesPerPixel,
			ui32Pass,
			IMG_NULL); /* passesReq */
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* PRQA S 3415 1 */ /* ignore side effects warning */
	if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(sDest.psSurf,
											sDest.ui32BytesPerPixel,
											IMG_FALSE, /* even if we have to give this to the TAG, we had the validation*/
											IMG_FALSE, /* already as it is among the sources*/
											& sDest.ui32LineStride)) ||
		(PVRSRV_OK != SGXTQ_GetSurfaceWidth(sDest.psSurf,
										sDest.ui32BytesPerPixel,
										IMG_FALSE,
										IMG_FALSE,
										& sDest.ui32Width)))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_UNREFERENCED_PARAMETER(psPassData);

	SGXTQ_SetUSSEKick(psPDSValues,
					psQueueTransfer->Details.sCustomShader.sUSEExecAddr,
					psTQContext->sUSEExecBase,
					psQueueTransfer->Details.sCustomShader.ui32NumTempRegs);

	if (psQueueTransfer->ui32NumSources != 0)
	{
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		psPDSValues->ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
		psPDSValues->ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif
	}

	switch (psQueueTransfer->ui32NumSources)
	{
		case 0:
		{
			ePDSPrim = SGXTQ_PDSPRIMFRAG_KICKONLY;
			break;
		}
		case 1:
		{
			ePDSPrim = SGXTQ_PDSPRIMFRAG_SINGLESOURCE;
			break;
		}
		case 2:
		{
			ePDSPrim = SGXTQ_PDSPRIMFRAG_TWOSOURCE;
			break;
		}
		case 3:
		{
			ePDSPrim = SGXTQ_PDSPRIMFRAG_THREESOURCE;
			break;
		}
		default:
			return PVRSRV_ERROR_INVALID_PARAMS;
	}
	/*
	 * Create PDS primary program
	 */
	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			ePDSPrim,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	/*
	 * PDS secondary
	 */
	if (psQueueTransfer->Details.sCustomShader.bUseDMAForSAs)
	{
		IMG_UINT32 ui32NumSAs = psQueueTransfer->Details.sCustomShader.ui32NumSAs;
		IMG_UINT uiBlockSize, uiNumBlocks;

		if (ui32NumSAs < 1)
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		/* Using DMA block load for attributes
		 * (e.g. to support complex filter functions) */

		/* Calculate the optimal DMA line-length, up to 16 dwords
		 * (choose a power of two for simplicity) */
		if ((ui32NumSAs & 0xF) == 0)
		{
			uiBlockSize = 0x10;
		}
		else if ((ui32NumSAs & 0x7) == 0)
		{
			uiBlockSize = 0x8;
		}
		else if ((ui32NumSAs & 0x3) == 0)
		{
			uiBlockSize = 0x4;
		}
		else if ((ui32NumSAs & 0x1) == 0)
		{
			uiBlockSize = 0x2;
		}
		else
		{
			uiBlockSize = 0x1;
		}
		uiNumBlocks = ui32NumSAs / uiBlockSize;

		/* 1. Patch the DMA instruction in the PDS sec program */
		SGXTQ_SetDMAState(psPDSValues,
						psQueueTransfer->Details.sCustomShader.sDevVAddrDMASrc,
						uiBlockSize, /* line length = stride <= 16 DWORDS */
						uiNumBlocks, /* number of lines */
						psQueueTransfer->Details.sCustomShader.ui32LineOffset);

		/* 2. Store the necessary patch in the transfer cmd
		 * (we will need it later in the TQ)
		 */
		ePDSSec = SGXTQ_PDSSECFRAG_DMA_ONLY;

		/* Update the number of SAs which we need to allocate for the pixel sec task
		 * This value will be inserted in the ISP control stream below
		 */
		//psTQContext->aui32PDSSecNumAttr[ePDSSec] = psQueueTransfer->Details.sCustomShader.ui32NumSAs;
	}
	else
	{
		/* SAs passed via DOUTA interface (supports up to 3 SAs) */
		switch (psQueueTransfer->Details.sCustomShader.ui32NumSAs)
		{
			case 0:
			{
				ePDSSec = SGXTQ_PDSSECFRAG_BASIC;
				break;
			}
			case 1:
			{
				ePDSSec = SGXTQ_PDSSECFRAG_1ATTR;
				psPDSValues->aui32A[0] = psQueueTransfer->Details.sCustomShader.UseParams[0];
				break;
			}
			case 2: /* TODO make TEXSETUP more generalized, we don't care about one specific purpose*/
			case 3: /* make it SGXTQ_PDSSECFRAG_{2/3/4}ATTR*/
			{
				ePDSSec = SGXTQ_PDSSECFRAG_TEXSETUP;
				psPDSValues->aui32A[0] = psQueueTransfer->Details.sCustomShader.UseParams[0];
				psPDSValues->aui32A[1] = psQueueTransfer->Details.sCustomShader.UseParams[1];
				psPDSValues->aui32A[2] = 0;
				break;
			}
			default:
				return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}
	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
			ePDSSec,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	eError = SGXTQ_CreatePDSSecResource(psTQContext,
			ePDSSec,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_SetupTransferClipRenderBox(psSubmit,
												(IMG_UINT32)sDest.psRect->x0, (IMG_UINT32)sDest.psRect->y0,
												(IMG_UINT32)sDest.psRect->x1, (IMG_UINT32)sDest.psRect->y1,
												sDest.ui32Width, sDest.ui32Height);
	
	if (PVRSRV_OK != eError)
	{
		return eError;
	}
	/*
	 * set up the tsp coordinates
	 */
	/* TODO wouldn't it be better just to use an array?*/
	if (psQueueTransfer->ui32NumSources > 0)
	{
		/* todo : per input layer support for this */
		if ((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERTX) != 0)
		{
			sTSPCoords.ui32Src0U1 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[0].psRect->x0, asSources[0].ui32Width);
			sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[0].psRect->x1, asSources[0].ui32Width);
		}
		else
		{
			sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[0].psRect->x0, asSources[0].ui32Width);
			sTSPCoords.ui32Src0U1 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[0].psRect->x1, asSources[0].ui32Width);
		}
		if ((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERTY) != 0)
		{
			sTSPCoords.ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[0].psRect->y0, asSources[0].ui32Height);
			sTSPCoords.ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[0].psRect->y1, asSources[0].ui32Height);
		}
		else
		{
			sTSPCoords.ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[0].psRect->y0, asSources[0].ui32Height);
			sTSPCoords.ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[0].psRect->y1, asSources[0].ui32Height);
		}
	}
	if (psQueueTransfer->ui32NumSources > 1)
	{
		sTSPCoords.ui32Src1U0 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[1].psRect->x0, asSources[1].ui32Width);
		sTSPCoords.ui32Src1U1 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[1].psRect->x1, asSources[1].ui32Width);
		sTSPCoords.ui32Src1V0 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[1].psRect->y0, asSources[1].ui32Height);
		sTSPCoords.ui32Src1V1 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[1].psRect->y1, asSources[1].ui32Height);
	}

	if (psQueueTransfer->ui32NumSources > 2)
	{
		sTSPCoords.ui32Src2U0 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[2].psRect->x0, asSources[2].ui32Width);
		sTSPCoords.ui32Src2U1 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[2].psRect->x1, asSources[2].ui32Width);
		sTSPCoords.ui32Src2V0 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[2].psRect->y0, asSources[2].ui32Height);
		sTSPCoords.ui32Src2V1 = SGXTQ_FloatIntDiv((IMG_UINT32)asSources[2].psRect->y1, asSources[2].ui32Height);
	}


	/*
	 * Create ISP stream
	 */
	eError = SGXTQ_CreateISPResource(psTQContext,
									psTQContext->apsPDSPrimResources[ePDSPrim],
									psTQContext->apsPDSSecResources[ePDSSec],
									sDest.psRect,
									&sTSPCoords,
									IMG_FALSE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									psQueueTransfer->ui32NumSources,
									psQueueTransfer->bPDumpContinuous,
									0);
	if (PVRSRV_OK != eError)
		return eError;


	/*
	 * create Pixevent handlers
	 */
	eError = SGXTQ_SetPBEState(sDest.psRect,
							sDest.psSurf->eMemLayout,
							sDest.ui32Width,
							sDest.ui32Height,
							sDest.ui32LineStride,
							0,	
							sDest.ui32DevVAddr,
							0,
							SGXTQ_ROTATION_NONE,
							(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE,
							IMG_TRUE,
							pui32PBEState);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}



	/* create EOT */
	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									pui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* create pixev */
	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	SGXTQ_SetupTransferRegs(psTQContext,
							0,
							psSubmit,
							psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
							psQueueTransfer->ui32NumSources,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
							EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
							0,
#endif
							EUR_CR_ISP_RENDER_TYPE_FASTSCALE);

	*pui32PassesRequired = 1;

	return PVRSRV_OK;
}

static PVRSRV_ERROR PrepareClipBlit(SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext,
									SGX_QUEUETRANSFER				* psQueueTransfer,
									IMG_UINT32						ui32Pass,
									SGXTQ_PREP_INTERNAL				* psPassData,
									SGXMKIF_TRANSFERCMD				* psSubmit,
									PVRSRV_TRANSFER_SGX_KICK		* psKick,
									IMG_UINT32						* pui32PassesRequired)
{
	SGXTQ_PDS_UPDATE	sPDSPrimValues;
	SGXTQ_PDS_UPDATE	sPDSSecValues;
	SGXTQ_TSP_SINGLE	asTSPCoords[SGXTQ_MAX_F2DRECTS];
	SGXTQ_TSP_COORDS	sTSPCoordsBG;
	IMG_UINT32			* pui32PBEState = psPassData->aui32PBEState;

	SGXTQ_SURFACE		* psSrcSurf;
	IMG_UINT32			ui32SrcDevVAddr;
	IMG_UINT32			ui32SrcWidth;
	IMG_UINT32			ui32SrcHeight;
	IMG_UINT32			ui32SrcLineStride;
	IMG_UINT32			ui32SrcBytesPerPixel;

	SGXTQ_SURFACE		* psDstSurf;
	IMG_RECT			* psDstRect;
	IMG_UINT32			ui32DstDevVAddr;
	IMG_UINT32			ui32DstWidth;
	IMG_UINT32			ui32DstHeight;
	IMG_UINT32			ui32DstLineStride;
	IMG_UINT32			ui32DstBytesPerPixel;

	PVRSRV_ERROR		eError;

	IMG_UINT32			i;

	IMG_RECT			sDstRect;

	IMG_UINT32			ui32NumClippedRects;
	IMG_UINT32			ui32RectIndex;
	IMG_RECT			asClipRect[SGXTQ_MAX_F2DRECTS];

	PVR_UNREFERENCED_PARAMETER(ui32Pass);

	/* Set up the sync infos - as this is single pass doesn't matter where */ 
	psKick->ui32NumSrcSync = 0;
	psKick->ui32NumDstSync = 0;

	for (i = 0; i < psQueueTransfer->ui32NumSources; i++)
	{
		if (psQueueTransfer->asSources[i].psSyncInfo)
		{
			PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asSources[i].psSyncInfo;

			psKick->ahSrcSyncInfo[psKick->ui32NumSrcSync] = psSyncInfo->hKernelSyncInfo;
			psKick->ui32NumSrcSync++;
		}
	}
	for (i = 0; i < psQueueTransfer->ui32NumDest; i++)
	{
		if (psQueueTransfer->asDests[i].psSyncInfo)
		{
			PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asDests[i].psSyncInfo;

			psKick->ahDstSyncInfo[psKick->ui32NumDstSync] = psSyncInfo->hKernelSyncInfo;
			psKick->ui32NumDstSync++;
		}
	}

	/*basic validation: 1 src and 1 dst (this is a single pass operation)*/
	if (psQueueTransfer->ui32NumSources != 1
		|| psQueueTransfer->ui32NumDest != 1
		|| psQueueTransfer->asSources[0].ui32ChunkStride != 0
		|| psQueueTransfer->asDests[0].ui32ChunkStride != 0)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psSrcSurf = &psQueueTransfer->asSources[0];
	ui32SrcDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf);
	ui32SrcHeight = psSrcSurf->ui32Height;

	psDstSurf = &psQueueTransfer->asDests[0];
	psDstRect = & SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer,0);
	ui32DstDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psDstSurf);
	ui32DstHeight = psDstSurf->ui32Height;

	if (! ISALIGNED(ui32SrcDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT)
			|| ! ISALIGNED(ui32DstDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT)
			|| ! ISALIGNED(ui32DstDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	/* Clip the rectangles */
	ui32NumClippedRects = 0;
	for (ui32RectIndex=0; ui32RectIndex < psQueueTransfer->Details.sClipBlit.ui32RectNum; ui32RectIndex++)
	{
		IMG_INT32 x0,y0,x1,y1;

		x0 = MAX(psQueueTransfer->Details.sClipBlit.psRects[ui32RectIndex].x0, SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0).x0);
		y0 = MAX(psQueueTransfer->Details.sClipBlit.psRects[ui32RectIndex].y0, SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0).y0);
		x1 = MIN(psQueueTransfer->Details.sClipBlit.psRects[ui32RectIndex].x1, SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0).x1);
		y1 = MIN(psQueueTransfer->Details.sClipBlit.psRects[ui32RectIndex].y1, SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0).y1);

		if (x1 > x0 && y1 > y0)
		{
			if (ui32NumClippedRects >= SGXTQ_MAX_F2DRECTS)
			{
				PVR_DPF((PVR_DBG_ERROR, "PrepareClipBlit: Number of visible clip regions exceeds maximum"));
				PVR_DBG_BREAK;
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			asClipRect[ui32NumClippedRects].x0 = x0;
			asClipRect[ui32NumClippedRects].x1 = x1;
			asClipRect[ui32NumClippedRects].y0 = y0;
			asClipRect[ui32NumClippedRects].y1 = y1;

			ui32NumClippedRects++;
		}
	}

	PVR_DPF((PVR_DBG_VERBOSE, "Number of clipped rects: %d\n", ui32NumClippedRects));
	for (ui32RectIndex=0; ui32RectIndex<ui32NumClippedRects; ui32RectIndex++)
	{
		PVR_DPF((PVR_DBG_VERBOSE, "clipped rect [%d] is (%d, %d) to (%d, %d)\n", ui32RectIndex,
				 asClipRect[ui32RectIndex].x0,
				 asClipRect[ui32RectIndex].y0,
				 asClipRect[ui32RectIndex].x1,
				 asClipRect[ui32RectIndex].y1));
	}

	*pui32PassesRequired = 1;

	PVRSRVMemSet(& sPDSPrimValues.asLayers[0].aui32TAGState,
			0,
			sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);

	PVRSRVMemSet(pui32PBEState, 0, sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);

	/* get the pixel information, for blitting the destination to HWBGOBJ */
	eError = SGXTQ_GetPixelFormats(psDstSurf->eFormat,
			IMG_TRUE,
			psDstSurf->eFormat,
			IMG_TRUE,
			SGXTQ_FILTERTYPE_POINT,
			sPDSPrimValues.asLayers[0].aui32TAGState,	
			IMG_NULL,
			& ui32SrcBytesPerPixel,
			pui32PBEState,
			& ui32DstBytesPerPixel,
			0,
			pui32PassesRequired);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}
	if (*pui32PassesRequired != 1)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psSrcSurf,
											ui32SrcBytesPerPixel,
											IMG_TRUE,
											IMG_FALSE,
											& ui32SrcLineStride)) ||
		(PVRSRV_OK != SGXTQ_GetSurfaceStride(psDstSurf,
											ui32DstBytesPerPixel,
											IMG_TRUE,
											IMG_FALSE,
											& ui32DstLineStride)) ||
		(PVRSRV_OK != SGXTQ_GetSurfaceWidth(psSrcSurf,
										ui32SrcBytesPerPixel,
										IMG_TRUE,
										IMG_FALSE,
										& ui32SrcWidth)) ||
		(PVRSRV_OK != SGXTQ_GetSurfaceWidth(psDstSurf,
										ui32DstBytesPerPixel,
										IMG_TRUE,
										IMG_FALSE,
										& ui32DstWidth)))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	
	/*
	 * PDS Primary
	 */
	SGXTQ_SetTAGState(&sPDSPrimValues,
					0,
					ui32DstDevVAddr,
					SGXTQ_FILTERTYPE_POINT,
					ui32DstWidth,
					ui32DstHeight,
					ui32DstLineStride,
					0,
					ui32DstBytesPerPixel,
					IMG_TRUE,
					psDstSurf->eMemLayout);

	sPDSPrimValues.asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
													EURASIA_PDS_DOUTI_TEXISSUE_TC0	|
													EURASIA_PDS_DOUTI_TEXLASTISSUE;

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
	sPDSPrimValues.asLayers[0].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT)	|
													(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif

	SGXTQ_SetUSSEKick(&sPDSPrimValues,
					psTQContext->apsUSEResources[SGXTQ_USEBLIT_NORMAL]->sDevVAddr,
					psTQContext->sUSEExecBase,
					psTQContext->apsUSEResources[SGXTQ_USEBLIT_NORMAL]->uResource.sUSE.ui32NumTempRegs);


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sPDSPrimValues.ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
	sPDSPrimValues.ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

	/* this goes to the accumlation object*/
	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			SGXTQ_PDSPRIMFRAG_SINGLESOURCE,
			&sPDSPrimValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * PDS secondary
	 */
	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
			SGXTQ_PDSSECFRAG_BASIC,
			& sPDSSecValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSSecResource(psTQContext,
			SGXTQ_PDSSECFRAG_BASIC,
			&sPDSSecValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	PVRSRVMemSet((IMG_PBYTE)&(sTSPCoordsBG), 0, sizeof (sTSPCoordsBG));
	sTSPCoordsBG.ui32Src0U1 = FLOAT32_ONE;
	sTSPCoordsBG.ui32Src0V1 = FLOAT32_ONE;

	sDstRect.x0 = 0;
	sDstRect.y0 = 0;
	sDstRect.x1 = (IMG_INT32) ui32DstWidth;
	sDstRect.y1 = (IMG_INT32) ui32DstHeight;

	/* note : we have to create the accumlation ISP stream now before start doing the resources
	 * for the fast 2D stream
	 */
	eError = SGXTQ_CreateISPResource(psTQContext,
									psTQContext->apsPDSPrimResources[SGXTQ_PDSPRIMFRAG_SINGLESOURCE],
									psTQContext->apsPDSSecResources[SGXTQ_PDSSECFRAG_BASIC],
									&sDstRect,
									&sTSPCoordsBG,
									IMG_FALSE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									1,
									psQueueTransfer->bPDumpContinuous,
									0);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	/* inherit some of the resources and the setup from above*/
	PVRSRVMemSet(& sPDSPrimValues.asLayers[0].aui32TAGState,
			0,
			sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);

	/* get the pixel information, for blitting the clip quads*/
	eError = SGXTQ_GetPixelFormats(psSrcSurf->eFormat,
			IMG_TRUE,
			psDstSurf->eFormat,
			IMG_TRUE,
			SGXTQ_FILTERTYPE_POINT,
			sPDSPrimValues.asLayers[0].aui32TAGState,	
			IMG_NULL,
			IMG_NULL,
			IMG_NULL,
			IMG_NULL,
			0,
			pui32PassesRequired);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

#if defined(FIX_HW_BRN_33029)
	(*pui32PassesRequired)++;

	if (*pui32PassesRequired > 2)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}
#else
	if (*pui32PassesRequired != 1)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}
#endif

	SGXTQ_SetTAGState(&sPDSPrimValues,
					0,
					ui32SrcDevVAddr,
					SGXTQ_FILTERTYPE_POINT,
					ui32SrcWidth,
					ui32SrcHeight,
					ui32SrcLineStride,
					0,
					ui32SrcBytesPerPixel,
					IMG_TRUE,
					psSrcSurf->eMemLayout);

	/* this goes into the ISP stream with geometry list*/
	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			SGXTQ_PDSPRIMFRAG_SINGLESOURCE,
			&sPDSPrimValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	if(psQueueTransfer->Details.sClipBlit.bUseSrcRectsForTexCoords == IMG_FALSE)
	{
		ui32SrcWidth = (ui32DstWidth * ui32SrcWidth) / psQueueTransfer->asSources[0].ui32Width;
		ui32SrcHeight = ui32DstHeight;
 		for (ui32RectIndex = 0; ui32RectIndex < ui32NumClippedRects ; ui32RectIndex++)
 		{
 			asTSPCoords[ui32RectIndex].ui32Src0U0 = SGXTQ_FloatIntDiv((IMG_UINT32) asClipRect[ui32RectIndex].x0, ui32SrcWidth);
 			asTSPCoords[ui32RectIndex].ui32Src0U1 = SGXTQ_FloatIntDiv((IMG_UINT32) asClipRect[ui32RectIndex].x1, ui32SrcWidth);
 			asTSPCoords[ui32RectIndex].ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32) asClipRect[ui32RectIndex].y0, ui32SrcHeight);
 			asTSPCoords[ui32RectIndex].ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32) asClipRect[ui32RectIndex].y1, ui32SrcHeight);
 		}
	}
	else
	{
		/* Use src-rects to define texture coords, not the clip-rects */
		PVR_ASSERT(ui32NumClippedRects == SGX_QUEUETRANSFER_NUM_SRC_RECTS(*psQueueTransfer));

		for (ui32RectIndex = 0; ui32RectIndex < ui32NumClippedRects ; ui32RectIndex++)
 		{
			IMG_RECT *psSrcRect = &SGX_QUEUETRANSFER_SRC_RECT(*psQueueTransfer,ui32RectIndex);

 			asTSPCoords[ui32RectIndex].ui32Src0U0 = SGXTQ_FloatIntDiv((IMG_UINT32) psSrcRect->x0, ui32SrcWidth);
 			asTSPCoords[ui32RectIndex].ui32Src0U1 = SGXTQ_FloatIntDiv((IMG_UINT32) psSrcRect->x1, ui32SrcWidth);
 			asTSPCoords[ui32RectIndex].ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32) psSrcRect->y0, ui32SrcHeight);
 			asTSPCoords[ui32RectIndex].ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32) psSrcRect->y1, ui32SrcHeight);
 		}
	}

	/*
	 * Fast 2d ISP stream
	 */
	eError = SGXTQ_CreateISPF2DResource(psTQContext,
										psTQContext->apsPDSPrimResources[SGXTQ_PDSPRIMFRAG_SINGLESOURCE],
										psTQContext->apsPDSSecResources[SGXTQ_PDSSECFRAG_BASIC],
										& asClipRect[0],
										& asTSPCoords[0],
										ui32NumClippedRects,
										IMG_FALSE,
										psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	/*
	 * Set up the PBE
	 */
	eError = SGXTQ_SetPBEState(psDstRect,
							psDstSurf->eMemLayout,
							ui32DstWidth,
							ui32DstHeight,
							ui32DstLineStride,
							0,
							ui32DstDevVAddr,
							0,
							SGXTQ_ROTATION_NONE,
							((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE),
							IMG_TRUE,
							pui32PBEState);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}



	/* And create the pixevent stuff */
	/* create EOT */
	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									pui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* create pixev */
	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_SetupTransferClipRenderBox(psSubmit,
											(IMG_UINT32) psDstRect->x0, (IMG_UINT32) psDstRect->y0,
											(IMG_UINT32) psDstRect->x1, (IMG_UINT32) psDstRect->y1,
											psDstSurf->ui32Width,
											psDstSurf->ui32Height);
	
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	SGXTQ_SetupTransferRegs(psTQContext,
			0,
			psSubmit,
			psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
			1,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
			EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
			0,
#endif
			EUR_CR_ISP_RENDER_TYPE_FAST2D);

#if defined(FIX_HW_BRN_33029)
	if(ui32Pass == 0)
	{
		IMG_PUINT32 pui32RACpuVAddr = IMG_NULL;
		IMG_UINT32	ui32RADevVAddr;
		SGXTQ_RESOURCE * psFast2DResource = IMG_NULL;

		if (! SGXTQ_AcquireCB(psTQContext->psFenceIDMemInfo,
					psTQContext->ui32FenceID,
					psTQContext->hOSEvent,
					psTQContext->psStreamCB,
					248,
					IMG_TRUE,
					(IMG_VOID **) &pui32RACpuVAddr,
					& ui32RADevVAddr,
					IMG_FALSE))
		{
			return PVRSRV_ERROR_TIMEOUT;		
		}

		psFast2DResource = psTQContext->psFast2DISPControlStream;
		PVR_ASSERT(psFast2DResource->sDevVAddr.uiAddr >= psTQContext->sISPStreamBase.uiAddr);
		

		*pui32RACpuVAddr++ = EURASIA_REGIONHEADER0_LASTREGION;
		*pui32RACpuVAddr++ = (psFast2DResource->sDevVAddr.uiAddr - psTQContext->sISPStreamBase.uiAddr) & 0x0FFFFFFF;
		*pui32RACpuVAddr++ = 0;
		*pui32RACpuVAddr = 0;

		psSubmit->sHWRegs.ui32ISPRender	= EUR_CR_ISP_RENDER_TYPE_NORMAL3D;
		psSubmit->sHWRegs.ui32ISPRgnBase	= (ui32RADevVAddr  >> EUR_CR_ISP_RGN_BASE_ADDR_ALIGNSHIFT) << EUR_CR_ISP_RGN_BASE_ADDR_SHIFT;

	}	
#endif

	return PVRSRV_OK;
}


static PVRSRV_ERROR PrepareColourLUTBlit(SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext,
										 SGX_QUEUETRANSFER *psQueueTransfer,
										 IMG_UINT32 ui32Pass,
										 SGXTQ_PREP_INTERNAL *psPassData,
										 SGXMKIF_TRANSFERCMD * psSubmit,
										 PVRSRV_TRANSFER_SGX_KICK *psKick,
										 IMG_UINT32 *pui32PassesRequired)
{
#if defined(SGX_FEATURE_USE_VEC34)
	/* implement me on 543 using native palettes*/
	PVR_UNREFERENCED_PARAMETER(psTQContext);
	PVR_UNREFERENCED_PARAMETER(psQueueTransfer);
	PVR_UNREFERENCED_PARAMETER(ui32Pass);
	PVR_UNREFERENCED_PARAMETER(psPassData);
	PVR_UNREFERENCED_PARAMETER(psSubmit);
	PVR_UNREFERENCED_PARAMETER(psKick);

	*pui32PassesRequired = 1;
	return PVRSRV_ERROR_NOT_SUPPORTED;
#else
	SGXTQ_SURF_DESC sSrc;
	SGXTQ_SURF_DESC sDest;

	SGXTQ_PDS_UPDATE sPDSValues;
	SGXTQ_TSP_COORDS sTSPCoords;
	IMG_UINT32 * pui32PBEState = psPassData->aui32PBEState;

	PVRSRV_ERROR eError;

	IMG_UINT32 i;
	SGXTQ_USEFRAGS eUSEProg;
	SGXTQ_PDSPRIMFRAGS ePDSPrim;
	SGXTQ_PDSSECFRAGS ePDSSec;
	IMG_UINT32 ui32ScaleRatio = 8 / psQueueTransfer->Details.sColourLUT.ui32KeySizeInBits;

	/* Set up the sync infos - can't fail before this*/
	psKick->ui32NumSrcSync = 0;
	psKick->ui32NumDstSync = 0;

	for (i = 0; i < psQueueTransfer->ui32NumSources; i++)
	{
		if (psQueueTransfer->asSources[i].psSyncInfo)
		{
			PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asSources[i].psSyncInfo;

			psKick->ahSrcSyncInfo[psKick->ui32NumSrcSync] = psSyncInfo->hKernelSyncInfo;
			psKick->ui32NumSrcSync++;
		}
	}
	for (i = 0; i < psQueueTransfer->ui32NumDest; i++)
	{
		if (psQueueTransfer->asDests[i].psSyncInfo)
		{
			PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asDests[i].psSyncInfo;

			psKick->ahDstSyncInfo[psKick->ui32NumDstSync] = psSyncInfo->hKernelSyncInfo;
			psKick->ui32NumDstSync++;
		}
	}

	if (psQueueTransfer->ui32NumSources != 1
			|| psQueueTransfer->ui32NumDest != 1
#ifndef BLITLIB
			|| psQueueTransfer->ui32NumSrcRects != 1
			|| psQueueTransfer->ui32NumDestRects != 1
#endif
		)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	*pui32PassesRequired = 1;

	sSrc.psSurf = &psQueueTransfer->asSources[0];
	sSrc.psRect = & SGX_QUEUETRANSFER_SRC_RECT(*psQueueTransfer, 0);
	sSrc.ui32DevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*sSrc.psSurf);
	sSrc.ui32Height = sSrc.psSurf->ui32Height;
	sSrc.ui32TAGFormat = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
	sSrc.ui32BytesPerPixel = 1;

	if (! ISALIGNED(sSrc.ui32DevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	switch (sSrc.psSurf->eFormat)
	{
		case PVRSRV_PIXEL_FORMAT_PAL1:
		case PVRSRV_PIXEL_FORMAT_PAL4:
		{
			IMG_UINT32 ui32StrideGran;

			if (sSrc.psSurf->i32StrideInBytes < 0)
				sSrc.ui32LineStride = (IMG_UINT32) (0 - sSrc.psSurf->i32StrideInBytes);
			else
				sSrc.ui32LineStride = (IMG_UINT32) sSrc.psSurf->i32StrideInBytes;

#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
			if ((sSrc.ui32LineStride & 3) != 0)
			{
				/* stride should be dword aligned*/
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			if (sSrc.psSurf->ui32Width % ui32ScaleRatio != 0)
			{
				/* last column ends mid byte, to support this we have to tell the shader
				 * about the last incomplete byte pixel columns*/
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			sSrc.ui32Width = sSrc.psSurf->ui32Width / ui32ScaleRatio;
			ui32StrideGran = SGXTQ_GetStrideGran(sSrc.ui32Width, sSrc.ui32BytesPerPixel);
			sSrc.ui32Width = (sSrc.ui32Width + ui32StrideGran - 1) & ~(ui32StrideGran - 1);
			if (sSrc.ui32Width > sSrc.ui32LineStride)
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
#else
			ui32StrideGran = SGXTQ_GetStrideGran(sSrc.ui32LineStride, sSrc.ui32BytesPerPixel);

			if (sSrc.ui32LineStride % ui32StrideGran != 0)
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			sSrc.ui32Width = sSrc.ui32LineStride;
#endif
			break;
		}
		case PVRSRV_PIXEL_FORMAT_PAL8:
		{
			/* PRQA S 3415 1 */ /* ignore side effects warning */
			if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(sSrc.psSurf,
													sSrc.ui32BytesPerPixel,
													IMG_TRUE,
													IMG_FALSE,
													& sSrc.ui32LineStride)) ||
				(PVRSRV_OK != SGXTQ_GetSurfaceWidth(sSrc.psSurf,
												   sSrc.ui32BytesPerPixel,
												   IMG_TRUE,
												   IMG_FALSE,
												   & sSrc.ui32Width)))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			break;
		}

		default:
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

	sDest.psSurf = &psQueueTransfer->asDests[0];
	sDest.psRect = & SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0);
	sDest.ui32DevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*sDest.psSurf);
	sDest.ui32Height = sDest.psSurf->ui32Height;

	if (! ISALIGNED(sDest.ui32DevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	PVRSRVMemSet(pui32PBEState, 0, sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);

	/* and set up the pixel related bit in the PBE state*/
	eError = SGXTQ_GetPixelFormats(PVRSRV_PIXEL_FORMAT_UNKNOWN,
			IMG_TRUE,
			sDest.psSurf->eFormat,
			IMG_TRUE,
			SGXTQ_FILTERTYPE_POINT,
			IMG_NULL,
			IMG_NULL,
			IMG_NULL,
			pui32PBEState,
			& sDest.ui32BytesPerPixel,
			ui32Pass,
			IMG_NULL);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* PRQA S 3415 1 */ /* ignore side effects warning */
	if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(sDest.psSurf,
											sDest.ui32BytesPerPixel,
											IMG_FALSE,
											IMG_FALSE,
											& sDest.ui32LineStride)) ||
		(PVRSRV_OK != SGXTQ_GetSurfaceWidth(sDest.psSurf,
										sDest.ui32BytesPerPixel,
										IMG_FALSE,
										IMG_FALSE,
										& sDest.ui32Width)))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_UNREFERENCED_PARAMETER(ui32Pass);
	PVR_UNREFERENCED_PARAMETER(psPassData);

	switch (psQueueTransfer->Details.sColourLUT.ui32KeySizeInBits)
	{
		case 8:
			/* doing texture look ups on the LUT*/
			eUSEProg = SGXTQ_USEBLIT_LUT256;
			ePDSPrim = SGXTQ_PDSPRIMFRAG_SINGLESOURCE;
			break;
		case 4:
			/* PDS DMA-ing 16 SAs for the LUT*/
			eUSEProg = SGXTQ_USEBLIT_LUT16;
			ePDSPrim = SGXTQ_PDSPRIMFRAG_ITER;
			if (psQueueTransfer->Details.sColourLUT.eLUTPxFmt != PVRSRV_PIXEL_FORMAT_ARGB8888)
			{
				/* we can't do any px fmt conversion as it doesn't go through the TF*/
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			break;
		case 1:
			eUSEProg = SGXTQ_USEBLIT_LUT2;
			ePDSPrim = SGXTQ_PDSPRIMFRAG_ITER;
			if (psQueueTransfer->Details.sColourLUT.eLUTPxFmt != PVRSRV_PIXEL_FORMAT_ARGB8888)
			{
				/* we can't do any px fmt conversion as it doesn't go through the TF*/
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			break;
		default:
			return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/*
	 * Create the shader
	 */
	eError = SGXTQ_CreateUSEResource(psTQContext,
			eUSEProg,
			IMG_NULL,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * PDS primary
	 */
	SGXTQ_SetUSSEKick(&sPDSValues,
					psTQContext->apsUSEResources[eUSEProg]->sDevVAddr,
					psTQContext->sUSEExecBase,
					psTQContext->apsUSEResources[eUSEProg]->uResource.sUSE.ui32NumTempRegs);

	switch (ePDSPrim)
	{
		case SGXTQ_PDSPRIMFRAG_SINGLESOURCE:
		{
			PVRSRVMemSet(& sPDSValues.asLayers[0].aui32TAGState,
					0,
					sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);


			SGXTQ_SetTAGState(&sPDSValues,
							  0,
							  sSrc.ui32DevVAddr,
							  SGXTQ_FILTERTYPE_POINT,
							  sSrc.ui32Width,
							  sSrc.ui32Height,
							  sSrc.ui32LineStride,
							  sSrc.ui32TAGFormat,
							  sSrc.ui32BytesPerPixel,
							  IMG_FALSE,
							  sSrc.psSurf->eMemLayout);


			sPDSValues.asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
														EURASIA_PDS_DOUTI_TEXISSUE_TC0	|
														EURASIA_PDS_DOUTI_TEXLASTISSUE;

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
			sPDSValues.asLayers[0].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT) |
				(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			sPDSValues.ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
			sPDSValues.ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif
			break;
		}
		case SGXTQ_PDSPRIMFRAG_ITER:
		{
			sPDSValues.asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_TC0	|
														EURASIA_PDS_DOUTI_TEXISSUE_NONE	|
														EURASIA_PDS_DOUTI_USELASTISSUE	|
														EURASIA_PDS_DOUTI_USEDIM_2D;

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
			sPDSValues.asLayers[0].aui32ITERState[0] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT)	|
														(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			sPDSValues.ui32U1 |= EURASIA_PDS_DOUTU1_ITERATORSDEPENDENCY;
#else
			sPDSValues.ui32U0 |= EURASIA_PDS_DOUTU0_ITERATORSDEPENDENCY;
#endif
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "PrepareColourLUTBlit: Unexpected PDS primary: %d", (IMG_UINT32) ePDSPrim));
			return PVRSRV_ERROR_UNEXPECTED_PRIMARY_FRAG;
		}
	}
	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			ePDSPrim,
			&sPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	/*
	 * PDS secondary
	 */
	PVRSRVMemSet(& sPDSValues.asLayers[0].aui32TAGState,
			0,
			sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);


	switch (psQueueTransfer->Details.sColourLUT.ui32KeySizeInBits)
	{
		case 8:
		{
			/* prepare SAs for a smp2d (on the LUT)*/
			IMG_UINT32 ui32LUTBpp;
			IMG_UINT32 ui32AttrPtr;
			IMG_UINT32 ui32LUTDevVAddr = psQueueTransfer->Details.sColourLUT.sLUTDevVAddr.uiAddr;

			PVRSRVMemSet(sPDSValues.asLayers[0].aui32TAGState, 0, sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);

			eError = SGXTQ_GetPixelFormats(psQueueTransfer->Details.sColourLUT.eLUTPxFmt,
					IMG_TRUE,	
					PVRSRV_PIXEL_FORMAT_ARGB8888,
					IMG_TRUE,	
					SGXTQ_FILTERTYPE_POINT,
					sPDSValues.asLayers[0].aui32TAGState,
					IMG_NULL,
					& ui32LUTBpp,
					IMG_NULL,
					IMG_NULL,
					ui32Pass,
					IMG_NULL);
			if (PVRSRV_OK != eError)
			{
				return eError;
			}

			if (! ISALIGNED(ui32LUTDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			SGXTQ_SetTAGState(&sPDSValues,
							  0,
					ui32LUTDevVAddr,
							  SGXTQ_FILTERTYPE_POINT,
							  1UL << psQueueTransfer->Details.sColourLUT.ui32KeySizeInBits,
							  1UL,
							  1UL << psQueueTransfer->Details.sColourLUT.ui32KeySizeInBits,
							  0,
							  ui32LUTBpp,
							  IMG_TRUE,
							  SGXTQ_MEMLAYOUT_STRIDE);

			for (ui32AttrPtr = 0; ui32AttrPtr < EURASIA_TAG_TEXTURE_STATE_SIZE; ui32AttrPtr++)
			{
				sPDSValues.aui32A[ui32AttrPtr] = sPDSValues.asLayers[0].aui32TAGState[ui32AttrPtr];
			}

			ePDSSec = SGXTQ_PDSSECFRAG_TEXSETUP;
			break;
		}
		case 4:
		case 1:
		{
			IMG_UINT32 ui32AttrPtr;

			/* copy the TAG state to SA direct AW */
			SGXTQ_SetTAGState(&sPDSValues,
							  0,
							  sSrc.ui32DevVAddr,
							  SGXTQ_FILTERTYPE_POINT,
							  sSrc.ui32Width,
							  sSrc.ui32Height,
							  sSrc.ui32LineStride,
							  sSrc.ui32TAGFormat,
							  sSrc.ui32BytesPerPixel,
							  IMG_FALSE,
							  sSrc.psSurf->eMemLayout);

			for (ui32AttrPtr = 0; ui32AttrPtr < EURASIA_TAG_TEXTURE_STATE_SIZE; ui32AttrPtr++)
			{
				sPDSValues.aui32A[ui32AttrPtr] = sPDSValues.asLayers[0].aui32TAGState[ui32AttrPtr];
			}

			sPDSValues.aui32A[ui32AttrPtr++] = SGXTQ_FixedToFloat((sSrc.ui32Width * ui32ScaleRatio) <<
																  FIXEDPT_FRAC);

			SGXTQ_SetDMAState(&sPDSValues,
							  psQueueTransfer->Details.sColourLUT.sLUTDevVAddr,
							  1UL << psQueueTransfer->Details.sColourLUT.ui32KeySizeInBits,
							  1UL,
							  ui32AttrPtr);

#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
			ePDSSec = SGXTQ_PDSSECFRAG_5ATTR_DMA;
#else
			ePDSSec = SGXTQ_PDSSECFRAG_4ATTR_DMA;
#endif
			break;
		}
		default:
			return PVRSRV_ERROR_INVALID_PARAMS;
	}

	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
			ePDSSec,
			&sPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	eError = SGXTQ_CreatePDSSecResource(psTQContext,
			ePDSSec,
			&sPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	/*
	 * Create the ISP stream
	 */
	/* set up the tsp coordinates*/
	sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrc.psRect->x0, sSrc.ui32Width * ui32ScaleRatio);
	sTSPCoords.ui32Src0U1 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrc.psRect->x1, sSrc.ui32Width * ui32ScaleRatio);
	sTSPCoords.ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrc.psRect->y0, sSrc.ui32Height);
	sTSPCoords.ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrc.psRect->y1, sSrc.ui32Height);

	eError = SGXTQ_CreateISPResource(psTQContext,
									psTQContext->apsPDSPrimResources[ePDSPrim],
									psTQContext->apsPDSSecResources[ePDSSec],
									sDest.psRect,
									& sTSPCoords,
									IMG_FALSE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									1,
									psQueueTransfer->bPDumpContinuous,
									0);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	/*
	 * Create the pixevent stuff
	 */
	/* Set up the PBE*/
	eError = SGXTQ_SetPBEState(
							sDest.psRect,
							sDest.psSurf->eMemLayout,
							sDest.ui32Width,
							sDest.ui32Height,
							sDest.ui32LineStride,
							0,
							sDest.ui32DevVAddr,
							0,
							SGXTQ_ROTATION_NONE,
							((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE),
							IMG_TRUE,
							pui32PBEState);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}



	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									pui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
		return eError;


	eError = SGXTQ_SetupTransferClipRenderBox(psSubmit,
											(IMG_UINT32) sDest.psRect->x0, (IMG_UINT32) sDest.psRect->y0,
											(IMG_UINT32) sDest.psRect->x1, (IMG_UINT32) sDest.psRect->y1,
											sDest.ui32Width, sDest.ui32Height);

	if (PVRSRV_OK != eError)
		return eError;

	SGXTQ_SetupTransferRegs(psTQContext,
							0,
							psSubmit,
							psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
							1,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
		EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
		0,
#endif
		EUR_CR_ISP_RENDER_TYPE_FASTSCALE);

	return PVRSRV_OK;
#endif /* SGX_FEATURE_CLUT_TEXTURES || SGX_FEATURE_USE_VEC34*/
}


/* Default: Conformant range BT601 (from our ogles diver)*/
static const SGXTQ_VPBCOEFFS sCoeffs_BT601 = {
/*  Y		U		V		const	shift			*/
	75,		0,		102,	-14267,	6,		/*	R	*/
	149,	-50,	-104,	17354,	7,		/*	G	*/
	37,		65,		0,		-8859,	5};		/*	B	*/


static PVRSRV_ERROR PrepareVideoBlit(SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext,
				  SGX_QUEUETRANSFER									* psQueueTransfer,
				  IMG_UINT32										ui32Pass,
				  SGXTQ_PREP_INTERNAL								*psPassData,
				  SGXMKIF_TRANSFERCMD								* psSubmit,
				  PVRSRV_TRANSFER_SGX_KICK							* psKick,
				  IMG_UINT32										* pui32PassesRequired)
{
	SGXTQ_PDS_UPDATE	sPDSPrimValues;
	SGXTQ_PDS_UPDATE	sPDSSecValues;
	SGXTQ_TSP_COORDS	sTSPCoords;

	SGXTQ_SURFACE		* psSrcSurf;
	SGXTQ_SURFACE		* psDstSurf;
	IMG_RECT*           psSrcRect;
	IMG_RECT*			psDstRect;
	IMG_UINT32			ui32DstDevVAddr;
	IMG_UINT32			ui32DstWidth;
	IMG_UINT32			ui32DstHeight;
	IMG_UINT32			ui32DstLineStride;
	IMG_UINT32			ui32DstBytesPerPixel;

	const SGXTQ_VPBCOEFFS	* psCoeffs;

	IMG_UINT32			ui32NumLayers;

	PVRSRV_ERROR		eError;
	SGXTQ_USEFRAGS		eUSEProg = SGXTQ_USEBLIT_VIDEOPROCESSBLIT_3PLANAR;
	IMG_UINT32			* pui32PBEState = psPassData->aui32PBEState;
	SGXTQ_PDSPRIMFRAGS	ePDSPrim = SGXTQ_PDSPRIMFRAG_THREESOURCE;
	SGXTQ_PDSSECFRAGS	ePDSSec = SGXTQ_PDSSECFRAG_BASIC;
	IMG_UINT32			i;
	IMG_UINT32          ui32NumBitsLostPrecisionR;
	IMG_UINT32          ui32NumBitsLostPrecisionG;

	psSrcRect = & SGX_QUEUETRANSFER_SRC_RECT(*psQueueTransfer, 0);
	psDstSurf = & psQueueTransfer->asDests[0];
	psDstRect = & SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0);
	ui32DstDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psDstSurf);
	ui32DstHeight = psDstSurf->ui32Height;
	ui32DstWidth = psDstSurf->ui32Width;

	*pui32PassesRequired = 1;

	if (! ISALIGNED(ui32DstDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	/* shader produces standard ARGB8888, we set the src to unknown like fill blits
	 * which means that dst could be anything convertable from this format */
	PVRSRVMemSet(pui32PBEState, 0, sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);
	eError = SGXTQ_GetPixelFormats(PVRSRV_PIXEL_FORMAT_UNKNOWN,
			IMG_TRUE,	
			psDstSurf->eFormat,
			(psDstSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
			SGXTQ_FILTERTYPE_POINT,
			IMG_NULL,
			IMG_NULL,
			IMG_NULL,
			pui32PBEState,
			& ui32DstBytesPerPixel,
			ui32Pass,
			pui32PassesRequired);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	if (*pui32PassesRequired != 1)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	/* PRQA S 3415 1 */ /* ignore side effects warning */
	if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psDstSurf,
				ui32DstBytesPerPixel,
				IMG_FALSE,
				IMG_FALSE,
				& ui32DstLineStride)) ||
			(PVRSRV_OK != SGXTQ_GetSurfaceWidth(psDstSurf,
				ui32DstBytesPerPixel,
				IMG_FALSE,
				IMG_FALSE,
				&ui32DstWidth)))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVR_UNREFERENCED_PARAMETER(pui32PassesRequired);
	PVR_UNREFERENCED_PARAMETER(ui32Pass);
	PVR_UNREFERENCED_PARAMETER(psPassData);
	/* Initialize SrcSurf to the first Surface*/
	psSrcSurf = & psQueueTransfer->asSources[0];
	switch (psSrcSurf->eFormat)
	{
		case PVRSRV_PIXEL_FORMAT_I420: /* 8 bit planar 4:2:0 format*/
		case PVRSRV_PIXEL_FORMAT_YV12: /* 8 bit planar 4:2:0 format*/
		{
			IMG_UINT32 ui32YBase, ui32UBase, ui32VBase;

			ui32NumLayers = 3;

			/* let's figure out the base addresses of the planes*/
			ui32YBase = SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf);
			if (psQueueTransfer->Details.sVPBlit.bSeparatePlanes)
			{
				ui32UBase = psQueueTransfer->Details.sVPBlit.asPlaneDevVAddr[0].uiAddr;
				ui32VBase = psQueueTransfer->Details.sVPBlit.asPlaneDevVAddr[1].uiAddr;
			}
			else
			{
				IMG_UINT32 ui32Size = psSrcSurf->ui32Width * psSrcSurf->ui32Height;

				ui32UBase = ui32YBase + ui32Size;
				ui32VBase = ui32UBase + ui32Size / 4;
			}

			if (! ISALIGNED(ui32YBase, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT)
				|| ! ISALIGNED(ui32UBase, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT)
				|| ! ISALIGNED(ui32VBase, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			if (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_YV12)
			{
				IMG_UINT32 ui32Tmp = ui32UBase;

				ui32UBase = ui32VBase;
				ui32VBase = ui32Tmp;
			}

			for (i = 0; i < 3; i++)
			{
				PVRSRVMemSet(& sPDSPrimValues.asLayers[i].aui32TAGState,
						0,
						sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);
			}

			/* let's create the PDS primary. we set up 3 layers iterating Y / U / V*/
			SGXTQ_SetTAGState(&sPDSPrimValues,
					0,
					ui32YBase,
					SGXTQ_FILTERTYPE_POINT,
					psSrcSurf->ui32Width,
					psSrcSurf->ui32Height,
					psSrcSurf->ui32Width,
					EURASIA_PDS_DOUTT1_TEXFORMAT_U8,
					1,
					IMG_FALSE,
					SGXTQ_MEMLAYOUT_STRIDE);

			SGXTQ_SetTAGState(&sPDSPrimValues,
					1,
					ui32UBase,
					SGXTQ_FILTERTYPE_POINT,
					psSrcSurf->ui32Width / 2,
					psSrcSurf->ui32Height / 2,
					psSrcSurf->ui32Width / 2,
					EURASIA_PDS_DOUTT1_TEXFORMAT_U8,
					1,
					IMG_FALSE,
					SGXTQ_MEMLAYOUT_STRIDE);

			SGXTQ_SetTAGState(&sPDSPrimValues,
					2,
					ui32VBase,
					SGXTQ_FILTERTYPE_POINT,
					psSrcSurf->ui32Width / 2,
					psSrcSurf->ui32Height / 2,
					psSrcSurf->ui32Width / 2,
					EURASIA_PDS_DOUTT1_TEXFORMAT_U8,
					1,
					IMG_FALSE,
					SGXTQ_MEMLAYOUT_STRIDE);

			for (i = 0; i < 3; i++)
			{
#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
				/* TODO: decide what to do without chanreplicate */
				sPDSPrimValues.asLayers[i].aui32TAGState[0] |= EURASIA_PDS_DOUTT0_CHANREPLICATE;
#endif
			}

			break;
		}

		case PVRSRV_PIXEL_FORMAT_NV12: // 8-bit planar 4:2:0 format
		{
			IMG_UINT32 ui32Size = psSrcSurf->ui32Width * psSrcSurf->ui32Height;
			IMG_UINT32 ui32YBase, ui32UVBase;
			IMG_UINT32 ui32PlaneStride;
			IMG_UINT32 ui32PlaneWidth;

			eUSEProg = SGXTQ_USEBLIT_VIDEOPROCESSBLIT_2PLANAR;
			ePDSPrim = SGXTQ_PDSPRIMFRAG_TWOSOURCE;
			ePDSSec = SGXTQ_PDSSECFRAG_BASIC;
			ui32NumLayers = 2;

			/* let's figure out the base addresses of the planes*/
			ui32YBase = SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf);
			if (psQueueTransfer->Details.sVPBlit.bSeparatePlanes)
			{
				ui32UVBase = psQueueTransfer->Details.sVPBlit.asPlaneDevVAddr[0].uiAddr;
				if (ui32YBase + ui32Size > ui32UVBase)
				{
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
			}
			else
			{
				ui32UVBase = ui32YBase + ui32Size;
			}

			if (! ISALIGNED(ui32UVBase, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT)
					|| ! ISALIGNED(ui32YBase, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			if (PVRSRV_OK != SGXTQ_GetSurfaceWidth(psSrcSurf, 1, IMG_TRUE, IMG_FALSE, & ui32PlaneWidth)
				|| (PVRSRV_OK != SGXTQ_GetSurfaceStride(psSrcSurf, 1, IMG_TRUE, IMG_FALSE, & ui32PlaneStride)))
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			for (i = 0; i < 2; i++)
			{
				PVRSRVMemSet(& sPDSPrimValues.asLayers[i].aui32TAGState,
						0,
						sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);
			}

			/* let's create the PDS primary. we set up 2 layers iterating Y / UV*/
			SGXTQ_SetTAGState(&sPDSPrimValues,
					0,
					ui32YBase,
					SGXTQ_FILTERTYPE_POINT,
					ui32PlaneWidth,
					psSrcSurf->ui32Height,
					ui32PlaneStride,
					EURASIA_PDS_DOUTT1_TEXFORMAT_U8,
					1,
					IMG_FALSE,
					SGXTQ_MEMLAYOUT_STRIDE);

			SGXTQ_SetTAGState(&sPDSPrimValues,
					1,
					ui32UVBase,
					SGXTQ_FILTERTYPE_POINT,
					ui32PlaneWidth / 2,
					psSrcSurf->ui32Height / 2,
					ui32PlaneStride / 2,
					EURASIA_PDS_DOUTT1_TEXFORMAT_U88,
					2,
					IMG_FALSE,
					SGXTQ_MEMLAYOUT_STRIDE);

#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
			/* TODO: decide what to do without chanreplicate */
			/* replicate Y, leave UV*/
			sPDSPrimValues.asLayers[0].aui32TAGState[0] |= EURASIA_PDS_DOUTT0_CHANREPLICATE;
#endif
			break;
		}

#if defined(EURASIA_PDS_DOUTT1_TEXFORMAT_UYVY)
		case PVRSRV_PIXEL_FORMAT_UYVY:  /* 8-bit packed 4:2:2 format*/
		case PVRSRV_PIXEL_FORMAT_YUY2:  /* 8-bit packed 4:2:2 format*/
		case PVRSRV_PIXEL_FORMAT_YUYV:	/* == YUY2*/
		{
			IMG_UINT32 ui32TagFormat;
			IMG_UINT32 ui32SrcDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf);

			if (! ISALIGNED(ui32SrcDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			eUSEProg = SGXTQ_USEBLIT_VIDEOPROCESSBLIT_PACKED;
			ePDSPrim = SGXTQ_PDSPRIMFRAG_SINGLESOURCE;
			ePDSSec = SGXTQ_PDSSECFRAG_BASIC;
			ui32NumLayers = 1;

			if (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_YUY2
					|| psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_YUYV)
			{
				ui32TagFormat = EURASIA_PDS_DOUTT1_TEXFORMAT_YUY2;
			}
			else
			{
				ui32TagFormat = EURASIA_PDS_DOUTT1_TEXFORMAT_UYVY;
			}

			PVRSRVMemSet(& sPDSPrimValues.asLayers[0].aui32TAGState,
					0,
					sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);

			/* let's create the PDS primary*/
			SGXTQ_SetTAGState(&sPDSPrimValues,
					0,
					ui32SrcDevVAddr,
					SGXTQ_FILTERTYPE_POINT,
					psSrcSurf->ui32Width,
					psSrcSurf->ui32Height,
					psSrcSurf->ui32Width,
					ui32TagFormat,
					2,
					IMG_FALSE,
					SGXTQ_MEMLAYOUT_STRIDE);

			break;
		}
#endif
		/* TODO */
		case PVRSRV_PIXEL_FORMAT_NV11:				/* 8-bit planar 4:1:1 format*/
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_AYUV:	/* 32-bit Combined YUV and alpha*/
		default:
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}
	}

	/* it's all static but to keep the code consistent let's create it*/
	eError = SGXTQ_CreateUSEResource(psTQContext,
			eUSEProg,
			IMG_NULL,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	SGXTQ_SetUSSEKick(&sPDSPrimValues,
			psTQContext->apsUSEResources[eUSEProg]->sDevVAddr,
			psTQContext->sUSEExecBase,
			psTQContext->apsUSEResources[eUSEProg]->uResource.sUSE.ui32NumTempRegs);


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sPDSPrimValues.ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
	sPDSPrimValues.ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

	for (i = 0; i < ui32NumLayers; i++)
	{
		sPDSPrimValues.asLayers[i].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE				|
#if defined(SGX545)
														((i+1) << EURASIA_PDS_DOUTI_TEXISSUE_SHIFT)	|
#else
														(i << EURASIA_PDS_DOUTI_TEXISSUE_SHIFT)		|
#endif
														((i == ui32NumLayers - 1) ?
														 EURASIA_PDS_DOUTI_TEXLASTISSUE : 0);

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
		sPDSPrimValues.asLayers[i].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT) |
														(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif
	}



	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			ePDSPrim,
			&sPDSPrimValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * PDS secondary
	 */
	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
			ePDSSec,
			& sPDSSecValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSSecResource(psTQContext,
			ePDSSec,
			&sPDSSecValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* FIR coefficients */
	psSubmit->bLoadFIRCoefficients = IMG_TRUE;
	if (psQueueTransfer->Details.sVPBlit.bCoeffsGiven)
	{
		psCoeffs = & psQueueTransfer->Details.sVPBlit.sCoeffs;
	}
	else
	{
		/* Default: Conformant range BT601 (from our ogles diver)*/
		psCoeffs = & sCoeffs_BT601;
	}

	psSubmit->sHWRegs.ui32FIRHFilterTable = 3;

	/* Workaround to support 9-bit signed coeffs YR and YG at API, and silently drop lsb where necessary on hardware that doesn't support it */
#if defined (SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
	/* on hardware with coefficients big enough, we don't need the workaround */
	ui32NumBitsLostPrecisionR = 0;
	ui32NumBitsLostPrecisionG = 0;
#else
	if (ui32NumLayers != 1)
	{
		/* workaround not required for planar formats as it's ok to use extra FIR taps */
		ui32NumBitsLostPrecisionR = 0;
		ui32NumBitsLostPrecisionG = 0;
	}
	else
	{
		if (psCoeffs->i16YR < 128)
		{
			/* coeff already in range - workaround not required */
			ui32NumBitsLostPrecisionR = 0;
		}
		else
		{
			/* Non-planar input format with coeffs >= 128 on hardware that does not support it.  Apply workaround */
			ui32NumBitsLostPrecisionR = 1;
		}
		if (psCoeffs->i16YG < 128)
		{
			/* coeff already in range - workaround not required */
			ui32NumBitsLostPrecisionG = 0;
		}
		else
		{
			/* Non-planar input format with coeffs >= 128 on hardware that does not support it.  Apply workaround */
			ui32NumBitsLostPrecisionG = 1;
		}
	}		
#endif

	if (ui32NumLayers == 1)
	{
		psSubmit->sHWRegs.ui32FIRHFilterLeft0 =
			((IMG_UINT32)(psCoeffs->i16YR >> ui32NumBitsLostPrecisionR << EUR_CR_USE_FILTER0_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP0_MASK);
	}
	else
	{
		psSubmit->sHWRegs.ui32FIRHFilterLeft0 =
			(((IMG_UINT32)(psCoeffs->i16YR / 2) << EUR_CR_USE_FILTER0_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP0_MASK)	|
			(((IMG_UINT32)((psCoeffs->i16YR + 1) / 2) << EUR_CR_USE_FILTER0_LEFT_TAP1_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP1_MASK);
	}

	if (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_NV12)
	{
		/* NV12 requires filter coefficients in order Y,V,U in order that they land on the correct taps */
		psSubmit->sHWRegs.ui32FIRHFilterRight0 =
			(((IMG_UINT32)psCoeffs->i16UR >> ui32NumBitsLostPrecisionR << EUR_CR_USE_FILTER0_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER0_RIGHT_TAP6_MASK);
	}
	else
	{
#if defined (SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
		psSubmit->sHWRegs.ui32FIRHFilterCentre0 =
			(((IMG_UINT32)psCoeffs->i16UR << EUR_CR_USE_FILTER0_CENTRE_TAP3_SHIFT) & EUR_CR_USE_FILTER0_CENTRE_TAP3_MASK);
#else
		psSubmit->sHWRegs.ui32FIRHFilterLeft0 |=
			(((IMG_UINT32)psCoeffs->i16UR >> ui32NumBitsLostPrecisionR << EUR_CR_USE_FILTER0_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP3_MASK);
#endif
	}

	if (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_NV12)
	{
		/* NV12 requires filter coefficients in order Y,V,U in order that they land on the correct taps */
#if defined (SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
		psSubmit->sHWRegs.ui32FIRHFilterCentre0 =
			(((IMG_UINT32)psCoeffs->i16VR << EUR_CR_USE_FILTER0_CENTRE_TAP3_SHIFT) & EUR_CR_USE_FILTER0_CENTRE_TAP3_MASK);
#else
		psSubmit->sHWRegs.ui32FIRHFilterLeft0 |=
			(((IMG_UINT32)psCoeffs->i16VR >> ui32NumBitsLostPrecisionR << EUR_CR_USE_FILTER0_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP3_MASK);
#endif
	}
	else
	{
		psSubmit->sHWRegs.ui32FIRHFilterRight0 =
			(((IMG_UINT32)psCoeffs->i16VR >> ui32NumBitsLostPrecisionR << EUR_CR_USE_FILTER0_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER0_RIGHT_TAP6_MASK);
	}

	psSubmit->sHWRegs.ui32FIRHFilterExtra0 =
		(((IMG_UINT32)psCoeffs->i16ConstR >> ui32NumBitsLostPrecisionR << EUR_CR_USE_FILTER0_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER0_EXTRA_CONSTANT_MASK)	|
		((((IMG_UINT32)psCoeffs->i16ShiftR - ui32NumBitsLostPrecisionR) << EUR_CR_USE_FILTER0_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER0_EXTRA_SHR_MASK);


	if (ui32NumLayers == 1)
	{
		psSubmit->sHWRegs.ui32FIRHFilterLeft1 =
			(((IMG_UINT32)psCoeffs->i16YG >> ui32NumBitsLostPrecisionG << EUR_CR_USE_FILTER1_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP0_MASK);
	}
	else
	{
		psSubmit->sHWRegs.ui32FIRHFilterLeft1 =
			(((IMG_UINT32)(psCoeffs->i16YG / 2) << EUR_CR_USE_FILTER1_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP0_MASK)	|
			(((IMG_UINT32)((psCoeffs->i16YG + 1) / 2) << EUR_CR_USE_FILTER1_LEFT_TAP1_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP1_MASK);
	}

	if (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_NV12)
	{
		psSubmit->sHWRegs.ui32FIRHFilterRight1 =
			(((IMG_UINT32)psCoeffs->i16UG >> ui32NumBitsLostPrecisionG << EUR_CR_USE_FILTER1_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER1_RIGHT_TAP6_MASK);
	}
	else
	{
#if defined (SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
		psSubmit->sHWRegs.ui32FIRHFilterCentre1 =
			(((IMG_UINT32)psCoeffs->i16UG << EUR_CR_USE_FILTER1_CENTRE_TAP3_SHIFT) & EUR_CR_USE_FILTER1_CENTRE_TAP3_MASK);
#else
		psSubmit->sHWRegs.ui32FIRHFilterLeft1 |=
			(((IMG_UINT32)psCoeffs->i16UG >> ui32NumBitsLostPrecisionG << EUR_CR_USE_FILTER1_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP3_MASK);
#endif
	}
	
	if (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_NV12)
	{
#if defined (SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
		psSubmit->sHWRegs.ui32FIRHFilterCentre1 =
			(((IMG_UINT32)psCoeffs->i16VG << EUR_CR_USE_FILTER1_CENTRE_TAP3_SHIFT) & EUR_CR_USE_FILTER1_CENTRE_TAP3_MASK);
#else
		psSubmit->sHWRegs.ui32FIRHFilterLeft1 |=
			(((IMG_UINT32)psCoeffs->i16VG >> ui32NumBitsLostPrecisionG << EUR_CR_USE_FILTER1_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP3_MASK);
#endif
	}
	else
	{
		psSubmit->sHWRegs.ui32FIRHFilterRight1 =
			(((IMG_UINT32)psCoeffs->i16VG >> ui32NumBitsLostPrecisionG << EUR_CR_USE_FILTER1_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER1_RIGHT_TAP6_MASK);
	}

	psSubmit->sHWRegs.ui32FIRHFilterExtra1 =
		(((IMG_UINT32)psCoeffs->i16ConstG >> ui32NumBitsLostPrecisionG << EUR_CR_USE_FILTER1_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER1_EXTRA_CONSTANT_MASK)	|
		((((IMG_UINT32)psCoeffs->i16ShiftG - ui32NumBitsLostPrecisionG) << EUR_CR_USE_FILTER1_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER1_EXTRA_SHR_MASK);

	if (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_NV12)
	{
		psSubmit->sHWRegs.ui32FIRHFilterLeft2 =
			(((IMG_UINT32)psCoeffs->i16YB << EUR_CR_USE_FILTER2_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP0_MASK)	|
			(((IMG_UINT32)psCoeffs->i16VB << EUR_CR_USE_FILTER2_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP3_MASK);

		psSubmit->sHWRegs.ui32FIRHFilterRight2 =
			((IMG_UINT32)psCoeffs->i16UB << EUR_CR_USE_FILTER2_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER2_RIGHT_TAP6_MASK;
	}
	else
	{
		psSubmit->sHWRegs.ui32FIRHFilterLeft2 =
			(((IMG_UINT32)psCoeffs->i16YB << EUR_CR_USE_FILTER2_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP0_MASK)	|
			(((IMG_UINT32)psCoeffs->i16UB << EUR_CR_USE_FILTER2_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP3_MASK);

		psSubmit->sHWRegs.ui32FIRHFilterRight2 =
			((IMG_UINT32)psCoeffs->i16VB << EUR_CR_USE_FILTER2_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER2_RIGHT_TAP6_MASK;
	}

	psSubmit->sHWRegs.ui32FIRHFilterExtra2 =
		(((IMG_UINT32)psCoeffs->i16ConstB << EUR_CR_USE_FILTER2_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_CONSTANT_MASK)	|
		(((IMG_UINT32)psCoeffs->i16ShiftB << EUR_CR_USE_FILTER2_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_SHR_MASK);

	/* calculate texture coords of source rect */
	sTSPCoords.ui32Src0U0 =
		SGXTQ_FloatIntDiv((IMG_UINT32)psSrcRect->x0, psSrcSurf->ui32Width);
	sTSPCoords.ui32Src0U1 =
		SGXTQ_FloatIntDiv((IMG_UINT32)psSrcRect->x1, psSrcSurf->ui32Width);
	sTSPCoords.ui32Src0V0 =
		SGXTQ_FloatIntDiv((IMG_UINT32)psSrcRect->y0, psSrcSurf->ui32Height);
	sTSPCoords.ui32Src0V1 =
		SGXTQ_FloatIntDiv((IMG_UINT32)psSrcRect->y1, psSrcSurf->ui32Height);
	if (ui32NumLayers > 1)
	{
		sTSPCoords.ui32Src1U0 = sTSPCoords.ui32Src0U0;
		sTSPCoords.ui32Src1U1 = sTSPCoords.ui32Src0U1;
		sTSPCoords.ui32Src1V0 = sTSPCoords.ui32Src0V0;
		sTSPCoords.ui32Src1V1 = sTSPCoords.ui32Src0V1;
	}
	if (ui32NumLayers > 2)
	{
		sTSPCoords.ui32Src2U0 = sTSPCoords.ui32Src0U0;
		sTSPCoords.ui32Src2U1 = sTSPCoords.ui32Src0U1;
		sTSPCoords.ui32Src2V0 = sTSPCoords.ui32Src0V0;
		sTSPCoords.ui32Src2V1 = sTSPCoords.ui32Src0V1;
	}

	/* all ready to go to ISP*/
	eError = SGXTQ_CreateISPResource(psTQContext,
			psTQContext->apsPDSPrimResources[ePDSPrim],
			psTQContext->apsPDSSecResources[ePDSSec],
			psDstRect,
			&sTSPCoords,
			IMG_FALSE,
			(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
			ui32NumLayers,
			psQueueTransfer->bPDumpContinuous,
			0);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	/* all that left is the pixevent stuff*/

	/*
	 * Set up the PBE
	 */
	eError = SGXTQ_SetPBEState(psDstRect,
			SGXTQ_MEMLAYOUT_OUT_LINEAR,
			ui32DstWidth,
			ui32DstHeight,
			ui32DstLineStride,
			0,
			ui32DstDevVAddr,
			0,
			SGXTQ_ROTATION_NONE,
			((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE),
			IMG_TRUE,
			pui32PBEState);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	/* create EOT */
	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
			pui32PBEState,
			SGXTQ_USEEOTHANDLER_BASIC,
			0, 0,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* create pixev */
	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
			psTQContext->psUSEEORHandler,
			psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
			SGXTQ_PDSPIXEVENTHANDLER_BASIC,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	SGXTQ_SetupTransferRenderBox(psSubmit,
			(IMG_UINT32) SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0).x0,
			(IMG_UINT32) SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0).y0,
			(IMG_UINT32) SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0).x1,
			(IMG_UINT32) SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0).y1);

	SGXTQ_SetupTransferRegs(psTQContext,
			0,
			psSubmit,
			psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
			ui32NumLayers,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
			EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
			0,
#endif
			EUR_CR_ISP_RENDER_TYPE_FASTSCALE);

	/* Set up the sync infos */
	psKick->ui32NumSrcSync = 0;
	psKick->ui32NumDstSync = 0;

	for (i = 0; i < psQueueTransfer->ui32NumSources; i++)
	{
		if (psQueueTransfer->asSources[i].psSyncInfo)
		{
			PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asSources[i].psSyncInfo;

			psKick->ahSrcSyncInfo[psKick->ui32NumSrcSync] = psSyncInfo->hKernelSyncInfo;
			psKick->ui32NumSrcSync++;
		}
	}
	for (i = 0; i < psQueueTransfer->ui32NumDest; i++)
	{
		if (psQueueTransfer->asDests[i].psSyncInfo)
		{
			PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asDests[i].psSyncInfo;

			psKick->ahDstSyncInfo[psKick->ui32NumDstSync] = psSyncInfo->hKernelSyncInfo;
			psKick->ui32NumDstSync++;
		}
	}

	return PVRSRV_OK;
}

static PVRSRV_ERROR PrepareClearTypeBlendOp(SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext,
				  SGX_QUEUETRANSFER									* psQueueTransfer,
				  IMG_UINT32										ui32Pass,
				  SGXTQ_PREP_INTERNAL								* psPassData,
				  SGXMKIF_TRANSFERCMD								* psSubmit,
				  PVRSRV_TRANSFER_SGX_KICK							* psKick,
				  IMG_UINT32										* pui32PassesRequired)
{
	SGXTQ_SURFACE		* psSrcSurf;
	SGXTQ_SURFACE		* psGammaSurf;
	SGXTQ_SURFACE		* psDstSurf;
	IMG_RECT			* psSrcRect;
	IMG_RECT			* psDstRect;
	IMG_UINT32			ui32SrcBytesPerPixel;
	IMG_UINT32			ui32DstBytesPerPixel;

	IMG_UINT32			aui32Limms[6];

	IMG_UINT32			ui32SrcDevVAddr;
	IMG_UINT32			ui32DstDevVAddr;
	IMG_UINT32			ui32GammaDevVAddr;

	SGXTQ_TSP_COORDS	sTSPCoords;

	IMG_UINT32			ui32DstWidth;
	IMG_UINT32			ui32DstHeight;

	IMG_UINT32			ui32SrcWidth;
	IMG_UINT32			ui32SrcHeight;

	IMG_UINT32			ui32SrcLineStride;
	IMG_UINT32			ui32DstLineStride;
	IMG_UINT32			ui32DstTAGWidth = 0;
	IMG_UINT32			ui32DstTAGLineStride = 0;


	IMG_UINT32			* pui32PBEState = psPassData->aui32PBEState;
	SGXTQ_PDS_UPDATE	* psPDSValues = & psPassData->sPDSUpdate;
	PVRSRV_ERROR		eError;

	IMG_RECT			sSrcRect;
	IMG_RECT			sDstRect;

	SGXTQ_USEFRAGS		eUSEProg;
	SGXTQ_PDSPRIMFRAGS	ePDSPrim;
	SGXTQ_PDSSECFRAGS	ePDSSec;

	IMG_DEV_VIRTADDR	sUSEExecAddr;

	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo;

	IMG_UINT32			ui32NumLayers = 2;

	IMG_UINT32			ui32NumTempRegs;

	IMG_UINT32			i;
	
	if((ui32Pass == 0)
			&& (psQueueTransfer->ui32NumSources != 2
				|| psQueueTransfer->ui32NumDest != 1
				|| SGX_QUEUETRANSFER_NUM_SRC_RECTS(*psQueueTransfer) != 1
				|| SGX_QUEUETRANSFER_NUM_DST_RECTS(*psQueueTransfer) != 1))
	{
		PVR_DPF((PVR_DBG_ERROR, "PrepareClearTypeBlendOp: Number of destinations (%u) != 1 (or) sources (%u) != 2",
			psQueueTransfer->ui32NumDest,
			psQueueTransfer->ui32NumSources));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}
	psSrcSurf 	= &(psQueueTransfer->asSources[0]);
	psDstSurf 	= &(psQueueTransfer->asDests[0]);
	psGammaSurf = &(psQueueTransfer->asSources[1]);

	if (psSrcSurf->psSyncInfo)
	{
		psKick->ui32NumSrcSync = 1;
		psSyncInfo = psSrcSurf->psSyncInfo;
		psKick->ahSrcSyncInfo[0] = psSyncInfo->hKernelSyncInfo;
	}
	else
	{
		psKick->ui32NumSrcSync = 0;
		psKick->ahSrcSyncInfo[0] = IMG_NULL;
	}
	if (psDstSurf->psSyncInfo)
	{
		psKick->ui32NumDstSync = 1;
		psSyncInfo = psDstSurf->psSyncInfo;
		psKick->ahDstSyncInfo[0] = psSyncInfo->hKernelSyncInfo;
	}
	else
	{
		psKick->ui32NumDstSync = 0;
		psKick->ahDstSyncInfo[0] = IMG_NULL;
	}
	if (psGammaSurf->psSyncInfo)
	{
		psKick->ui32NumSrcSync += 1;
		psSyncInfo = psGammaSurf->psSyncInfo;
		psKick->ahSrcSyncInfo[1] = psSyncInfo->hKernelSyncInfo;
	}
	else
	{
		psKick->ui32NumSrcSync = 0;
		psKick->ahSrcSyncInfo[1] = IMG_NULL;
	}

	psSrcRect = & SGX_QUEUETRANSFER_SRC_RECT(*psQueueTransfer, 0);
	psDstRect = & SGX_QUEUETRANSFER_DST_RECT(*psQueueTransfer, 0);

	ui32SrcHeight = psSrcSurf->ui32Height;
	ui32DstHeight = psDstSurf->ui32Height;

	ui32SrcDevVAddr 	= SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf);
	ui32DstDevVAddr 	= SGX_TQSURFACE_GET_DEV_VADDR(*psDstSurf);
	ui32GammaDevVAddr 	= SGX_TQSURFACE_GET_DEV_VADDR(*psGammaSurf);


	if (! ISALIGNED(ui32SrcDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT)
		|| ! ISALIGNED(ui32DstDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT)
		|| ! ISALIGNED(ui32DstDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	PVR_DPF((PVR_DBG_VERBOSE, "\t-> ClearTypeBlend from 0x%08x to 0x%08x", ui32SrcDevVAddr, ui32DstDevVAddr));

	/* First pass through.  Need to calculate everything
	   from basics
	 */
	*pui32PassesRequired = 1;

	sSrcRect = *psSrcRect;
	sDstRect = *psDstRect;

	for (i = 0; i < ui32NumLayers; i++)
	{
		PVRSRVMemSet(psPDSValues->asLayers[i].aui32TAGState,
				0,
				sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);
	}

	PVRSRVMemSet(pui32PBEState,
			0,
			sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);


	/* get the pixel information */
	eError = SGXTQ_GetPixelFormats(psSrcSurf->eFormat,
			(psSrcSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
			psDstSurf->eFormat,
			(psDstSurf->ui32ChunkStride == 0) ? IMG_TRUE : IMG_FALSE,
			SGXTQ_FILTERTYPE_POINT,
			psPDSValues->asLayers[0].aui32TAGState,	
			IMG_NULL,
			& ui32SrcBytesPerPixel,
			pui32PBEState,
			& ui32DstBytesPerPixel,
			ui32Pass,
			pui32PassesRequired);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* if the dst is sampled in via the TAG set up the pixel stuff in the second layer*/
	{
		IMG_UINT32 ui32DstSamplingPasses = 1;

		IMG_BOOL bDstChunkStride = (psDstSurf->ui32ChunkStride) ? IMG_FALSE : IMG_TRUE;

		/* get the pixel information */
		eError = SGXTQ_GetPixelFormats(psDstSurf->eFormat,
				bDstChunkStride,
				psDstSurf->eFormat,
				bDstChunkStride,
				SGXTQ_FILTERTYPE_POINT,
				psPDSValues->asLayers[1].aui32TAGState,	
				IMG_NULL,
				IMG_NULL,
				IMG_NULL,
				IMG_NULL,
				ui32Pass,
				& ui32DstSamplingPasses);
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		if (ui32DstSamplingPasses != *pui32PassesRequired)
		{
			/* the number of required passes to sample the destination differs from
			 * the source.
			 */
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}
	}

	/* PRQA S 3415 1 */ /* ignore side effects warning */
	if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psSrcSurf,
											ui32SrcBytesPerPixel,
											IMG_TRUE,
											IMG_FALSE, 
											&ui32SrcLineStride)) ||
		(PVRSRV_OK != SGXTQ_GetSurfaceStride(psDstSurf,
											ui32DstBytesPerPixel,
											IMG_FALSE,
											IMG_FALSE, 
											&ui32DstLineStride)) ||
		(PVRSRV_OK != SGXTQ_GetSurfaceWidth(psSrcSurf,
											ui32SrcBytesPerPixel,
											IMG_TRUE,
											IMG_FALSE, 
											&ui32SrcWidth)) ||
		(PVRSRV_OK != SGXTQ_GetSurfaceWidth(psDstSurf,
											ui32DstBytesPerPixel,
											IMG_FALSE,
											IMG_FALSE, 
											&ui32DstWidth)))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psDstSurf,
										ui32DstBytesPerPixel,
										IMG_TRUE,
										IMG_FALSE,
										& ui32DstTAGLineStride)) ||
		(PVRSRV_OK != SGXTQ_GetSurfaceWidth(psDstSurf,
									ui32DstBytesPerPixel,
									IMG_TRUE,
									IMG_FALSE,
									& ui32DstTAGWidth)))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}


	/* Set the PDS fragments */
	ePDSPrim = SGXTQ_PDSPRIMFRAG_TWOSOURCE;

	if(psQueueTransfer->Details.sClearTypeBlend.ui32Gamma == 0xFFFFFFFF)
	{
		eUSEProg = SGXTQ_USEBLIT_CLEARTYPEBLEND_INVALIDGAMMA;
		ePDSSec = SGXTQ_PDSSECFRAG_1ATTR;
	}
	else
	{
		eUSEProg = SGXTQ_USEBLIT_CLEARTYPEBLEND_GAMMA; 		
		ePDSSec  = SGXTQ_PDSSECFRAG_4ATTR;	
	}

	/*
	 * TSP coords
	 */
	sTSPCoords.ui32Src1U0 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.x0, ui32DstTAGWidth);
	sTSPCoords.ui32Src1U1 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.x1, ui32DstTAGWidth);
	sTSPCoords.ui32Src1V0 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.y0, ui32DstHeight);
	sTSPCoords.ui32Src1V1 = SGXTQ_FloatIntDiv((IMG_UINT32) sDstRect.y1, ui32DstHeight);

	sTSPCoords.ui32Src0U0 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.x0, ui32SrcWidth);
	sTSPCoords.ui32Src0U1 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.x1, ui32SrcWidth);
	sTSPCoords.ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.y0, ui32SrcHeight);
	sTSPCoords.ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32) sSrcRect.y1, ui32SrcHeight);

	/*
	 * Set up the PBE
	 */
	eError = SGXTQ_SetPBEState(&sDstRect,
							psDstSurf->eMemLayout,
							ui32DstWidth,
							ui32DstHeight,
							ui32DstLineStride,
							0,
							ui32DstDevVAddr,
							0,
							SGXTQ_ROTATION_NONE,
							IMG_FALSE,
							IMG_TRUE,
							pui32PBEState);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_SetupTransferClipRenderBox(psSubmit,
											(IMG_UINT32) sDstRect.x0,
											(IMG_UINT32) sDstRect.y0,
											(IMG_UINT32) sDstRect.x1,
											(IMG_UINT32) sDstRect.y1,
											ui32DstWidth,
											ui32DstHeight);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * common PDS values - primaries
	 */
	SGXTQ_SetTAGState(psPDSValues,
					0,
					ui32SrcDevVAddr,
					SGXTQ_FILTERTYPE_POINT,
					ui32SrcWidth,
					ui32SrcHeight,
					ui32SrcLineStride,
					0, 
					ui32SrcBytesPerPixel,
					IMG_TRUE,
					psSrcSurf->eMemLayout);

			
	
	{	/* get the sader */
		SGXTQ_RESOURCE* psResource;
		eError = SGXTQ_CreateUSEResource(psTQContext,
				eUSEProg,
				aui32Limms,
				psQueueTransfer->bPDumpContinuous);
		if (PVRSRV_OK != eError)
		{
			return eError;
		}

		psResource = psTQContext->apsUSEResources[eUSEProg];
		sUSEExecAddr = psResource->sDevVAddr;
		ui32NumTempRegs = psResource->uResource.sUSE.ui32NumTempRegs;
	}	

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
	psPDSValues->asLayers[0].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT) |
													(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif


	SGXTQ_SetUSSEKick(psPDSValues,
					sUSEExecAddr,
					psTQContext->sUSEExecBase,
					ui32NumTempRegs);

	switch (ePDSPrim)
	{
	
		case SGXTQ_PDSPRIMFRAG_TWOSOURCE:
		{
			SGXTQ_SetTAGState(psPDSValues,
							  1,
							  ui32DstDevVAddr,
							  SGXTQ_FILTERTYPE_POINT,
							  ui32DstTAGWidth,
							  ui32DstHeight,
							  ui32DstTAGLineStride,
							  0, //ui32DstTAGFormat,
							  ui32DstBytesPerPixel,
							  IMG_TRUE,
							  psDstSurf->eMemLayout);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
			psPDSValues->ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
			psPDSValues->ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
			psPDSValues->asLayers[1].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT) |
															(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif

			psPDSValues->asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
															EURASIA_PDS_DOUTI_TEXISSUE_TC0;

			psPDSValues->asLayers[1].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
															EURASIA_PDS_DOUTI_TEXISSUE_TC1	|
															EURASIA_PDS_DOUTI_TEXLASTISSUE;
			break;
		}

		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "PrepareClearTypeBlendOp: Unknown PDS primary: %d", (IMG_UINT32) ePDSPrim));
			PVR_DBG_BREAK;
			return PVRSRV_ERROR_UNKNOWN_PRIMARY_FRAG;
		}
	}

	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			ePDSPrim,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
				ePDSSec,
				psPDSValues,
				psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	switch (ePDSSec)
	{
		case SGXTQ_PDSSECFRAG_1ATTR:
			psPDSValues->aui32A[0]	= psQueueTransfer->Details.sClearTypeBlend.ui32Colour;
			break;
	
		case SGXTQ_PDSSECFRAG_4ATTR:
		{
			/* Load SAs in the following order:
			 * SA0 : Colour
			 * SA1 : Colour2
			 * SA3 : GammaTableIndex
			 * SA4 : InverseGammaTableIndex
			 */
			IMG_UINT32 ui32Gamma 				= psQueueTransfer->Details.sClearTypeBlend.ui32Gamma;
			IMG_UINT32 ui32GammaSurfacePitch	= psQueueTransfer->Details.sClearTypeBlend.ui32GammaSurfacePitch;
					
			psPDSValues->aui32A[0]	= psQueueTransfer->Details.sClearTypeBlend.ui32Colour;
			psPDSValues->aui32A[1]	= psQueueTransfer->Details.sClearTypeBlend.ui32Colour2;
			psPDSValues->aui32A[2]	= ui32GammaDevVAddr + ui32Gamma * ui32GammaSurfacePitch;
			psPDSValues->aui32A[3]	= ui32GammaDevVAddr + ui32Gamma * ui32GammaSurfacePitch + 256;
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "PrepareClearTypeBlendOp: Unexpected PDS secondary: %d", (IMG_UINT32) ePDSSec));
			PVR_DBG_BREAK;
			return PVRSRV_ERROR_UNEXPECTED_SECONDARY_FRAG;
		}
	}

	eError = SGXTQ_CreatePDSSecResource(psTQContext,
			ePDSSec,
			psPDSValues,
			psQueueTransfer->bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = SGXTQ_CreateISPResource(psTQContext,
									psTQContext->apsPDSPrimResources[ePDSPrim],
									psTQContext->apsPDSSecResources[ePDSSec],
									&sDstRect,
									&sTSPCoords,
									IMG_FALSE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									ui32NumLayers,
									psQueueTransfer->bPDumpContinuous,
									0);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									pui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}


	SGXTQ_SetupTransferRegs(psTQContext,
			0,
			psSubmit,
			psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
			ui32NumLayers,
			0,
			EUR_CR_ISP_RENDER_TYPE_FASTSCALE);



	/* Specific setup stuff */
	psSubmit->bLoadFIRCoefficients = IMG_FALSE;
	return PVRSRV_OK;
}


static PVRSRV_ERROR PrepareTAtlasBlit(SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext,
									SGX_QUEUETRANSFER				* psQueueTransfer,
									IMG_UINT32						ui32Pass,
									SGXTQ_PREP_INTERNAL				* psPassData,
									SGXMKIF_TRANSFERCMD				* psSubmit,
									PVRSRV_TRANSFER_SGX_KICK		* psKick,
									IMG_UINT32						* pui32PassesRequired)
{
	SGXTQ_PDS_UPDATE	sPDSPrimValues;
	SGXTQ_PDS_UPDATE	sPDSSecValues;
	SGXTQ_TSP_COORDS	sTSPCoordsBG;
	SGXTQ_TSP_SINGLE	asTSPCoords[SGXTQ_MAX_F2DRECTS];
	IMG_UINT32			* pui32PBEState = psPassData->aui32PBEState;

	SGXTQ_TATLASBLITOP	* psTAtlas = & psQueueTransfer->Details.sTAtlas;
	SGXTQ_ALPHA			eAlpha = psQueueTransfer->Details.sTAtlas.eAlpha;
	SGXTQ_USEFRAGS		eUSEProg;
	IMG_BOOL			bTranslucent;

	SGXTQ_SURFACE		* psSrcSurf;
	IMG_UINT32			ui32SrcDevVAddr;
	IMG_UINT32			ui32SrcWidth;
	IMG_UINT32			ui32SrcHeight;
	IMG_UINT32			ui32SrcLineStride;
	IMG_UINT32			ui32SrcBytesPerPixel;

	SGXTQ_SURFACE		* psDstSurf;
	IMG_UINT32			ui32DstDevVAddr;
	IMG_UINT32			ui32DstWidth;
	IMG_UINT32			ui32DstHeight;
	IMG_UINT32			ui32DstLineStride;
	IMG_UINT32			ui32DstBytesPerPixel;

	PVRSRV_ERROR		eError;

	IMG_UINT32			i;
	IMG_RECT			sDstRect;

	/* Set up the sync infos - as this is single pass doesn't matter where */ 
	if (psQueueTransfer->asSources[0].psSyncInfo)
	{
		PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asSources[0].psSyncInfo;

		psKick->ahSrcSyncInfo[psKick->ui32NumSrcSync] = psSyncInfo->hKernelSyncInfo;
		psKick->ui32NumSrcSync = 1;
	}
	else
	{
		psKick->ui32NumSrcSync = 0;
	}
	if (psQueueTransfer->asDests[0].psSyncInfo)
	{
		PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asDests[0].psSyncInfo;

		psKick->ahDstSyncInfo[psKick->ui32NumDstSync] = psSyncInfo->hKernelSyncInfo;
		psKick->ui32NumDstSync++;
	}
	else
	{
		psKick->ui32NumDstSync = 0;
	}

	/* tell the dispatcher to call us 1x*/
	PVR_UNREFERENCED_PARAMETER(ui32Pass);
	* pui32PassesRequired = 1;

	/* basic validation: 1 src and 1 dst (the destination is accumlated as background and optionally blended)
	 * surface sub rectangles / dst clip box are ignored*/
	if (psQueueTransfer->ui32NumSources != 1
		|| psQueueTransfer->ui32NumDest != 1
		|| SGX_QUEUETRANSFER_NUM_SRC_RECTS(*psQueueTransfer) != 0
		|| SGX_QUEUETRANSFER_NUM_DST_RECTS(*psQueueTransfer) != 0
		|| psQueueTransfer->asSources[0].ui32ChunkStride != 0
		|| psQueueTransfer->asDests[0].ui32ChunkStride != 0)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (psTAtlas->ui32NumMappings > SGXTQ_MAX_F2DRECTS)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	psSrcSurf       = & psQueueTransfer->asSources[0];
	ui32SrcDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf);
	ui32SrcHeight   = psSrcSurf->ui32Height;

	psDstSurf       = & psQueueTransfer->asDests[0];
	ui32DstDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psDstSurf);
	ui32DstHeight   = psDstSurf->ui32Height;

	if (! ISALIGNED(ui32SrcDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT)
			|| ! ISALIGNED(ui32DstDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT)
			|| ! ISALIGNED(ui32DstDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT))
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	switch (eAlpha)
	{
		case SGXTQ_ALPHA_NONE:
		{
			eUSEProg = SGXTQ_USEBLIT_NORMAL;
			bTranslucent = IMG_FALSE;
			break;
		}
		case SGXTQ_ALPHA_SOURCE:
		{
			eUSEProg = SGXTQ_USEBLIT_ACCUM_SRC_BLEND;
			bTranslucent = IMG_TRUE;
			break;
		}
		default:
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}
	}

	PVRSRVMemSet(& sPDSPrimValues.asLayers[0].aui32TAGState,
			0,
			sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);

	PVRSRVMemSet(pui32PBEState, 0, sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);

	/* get the pixel information, for blitting the destination to HWBGOBJ */
	eError = SGXTQ_GetPixelFormats(psDstSurf->eFormat,
			IMG_TRUE,
			psDstSurf->eFormat,
			IMG_TRUE,
			SGXTQ_FILTERTYPE_POINT,
			sPDSPrimValues.asLayers[0].aui32TAGState,
			IMG_NULL,
			& ui32SrcBytesPerPixel,
			pui32PBEState,
			& ui32DstBytesPerPixel,
			0,
			pui32PassesRequired);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}
	if (*pui32PassesRequired != 1)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psSrcSurf, ui32SrcBytesPerPixel, IMG_TRUE, IMG_FALSE, & ui32SrcLineStride))
			|| (PVRSRV_OK != SGXTQ_GetSurfaceStride(psDstSurf, ui32DstBytesPerPixel, IMG_TRUE, IMG_FALSE, & ui32DstLineStride))
			|| (PVRSRV_OK != SGXTQ_GetSurfaceWidth(psSrcSurf,  ui32SrcBytesPerPixel, IMG_TRUE, IMG_FALSE, & ui32SrcWidth))
			|| (PVRSRV_OK != SGXTQ_GetSurfaceWidth(psDstSurf,  ui32DstBytesPerPixel, IMG_TRUE, IMG_FALSE, & ui32DstWidth)))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}


	/* tell the PBE/ISP to render every pixel on the destination*/
	sDstRect.x0 = 0;
	sDstRect.y0 = 0;
	sDstRect.x1 = (IMG_INT32) ui32DstWidth;
	sDstRect.y1 = (IMG_INT32) ui32DstHeight;

	
	/*
	 * PDS Primary
	 */
	SGXTQ_SetTAGState(&sPDSPrimValues,
					0,
					ui32DstDevVAddr,
					SGXTQ_FILTERTYPE_POINT,
					ui32DstWidth,
					ui32DstHeight,
					ui32DstLineStride,
					0,
					ui32DstBytesPerPixel,
					IMG_TRUE,
					psDstSurf->eMemLayout);

	sPDSPrimValues.asLayers[0].aui32ITERState[0] =	EURASIA_PDS_DOUTI_USEISSUE_NONE	|
													EURASIA_PDS_DOUTI_TEXISSUE_TC0	|
													EURASIA_PDS_DOUTI_TEXLASTISSUE;

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
	sPDSPrimValues.asLayers[0].aui32ITERState[1] =	(0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT)	|
													(0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif

	SGXTQ_SetUSSEKick(&sPDSPrimValues,
					psTQContext->apsUSEResources[SGXTQ_USEBLIT_NORMAL]->sDevVAddr,
					psTQContext->sUSEExecBase,
					psTQContext->apsUSEResources[SGXTQ_USEBLIT_NORMAL]->uResource.sUSE.ui32NumTempRegs);


#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	sPDSPrimValues.ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
	sPDSPrimValues.ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

	/* this goes to the accumlation object*/
	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			SGXTQ_PDSPRIMFRAG_SINGLESOURCE,
			&sPDSPrimValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * PDS secondary
	 */
	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
			SGXTQ_PDSSECFRAG_BASIC,
			& sPDSSecValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSSecResource(psTQContext,
			SGXTQ_PDSSECFRAG_BASIC,
			&sPDSSecValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	PVRSRVMemSet((IMG_PBYTE)&(sTSPCoordsBG), 0, sizeof (sTSPCoordsBG));
	sTSPCoordsBG.ui32Src0U1 = FLOAT32_ONE;
	sTSPCoordsBG.ui32Src0V1 = FLOAT32_ONE;

	/* note : we have to create the accumlation ISP stream now before start doing the resources
	 * for the fast 2D stream
	 */
	eError = SGXTQ_CreateISPResource(psTQContext,
									psTQContext->apsPDSPrimResources[SGXTQ_PDSPRIMFRAG_SINGLESOURCE],
									psTQContext->apsPDSSecResources[SGXTQ_PDSSECFRAG_BASIC],
									&sDstRect,
									&sTSPCoordsBG,
									IMG_FALSE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									1,
									psQueueTransfer->bPDumpContinuous,
									0);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	/* inherit some of the resources and the setup from above*/
	PVRSRVMemSet(& sPDSPrimValues.asLayers[0].aui32TAGState,
			0,
			sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);

	/* get the pixel information, for blitting the clip quads*/
	eError = SGXTQ_GetPixelFormats(psSrcSurf->eFormat,
			IMG_TRUE,
			psDstSurf->eFormat,
			IMG_TRUE,
			SGXTQ_FILTERTYPE_POINT,
			sPDSPrimValues.asLayers[0].aui32TAGState,	
			IMG_NULL,
			IMG_NULL,
			IMG_NULL,
			IMG_NULL,
			0,
			pui32PassesRequired);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}
	if (*pui32PassesRequired != 1)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	if (eUSEProg != SGXTQ_USEBLIT_NORMAL)
	{
		SGXTQ_SetUSSEKick(& sPDSPrimValues,
				psTQContext->apsUSEResources[eUSEProg]->sDevVAddr,
				psTQContext->sUSEExecBase,
				psTQContext->apsUSEResources[eUSEProg]->uResource.sUSE.ui32NumTempRegs);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
		sPDSPrimValues.ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
		sPDSPrimValues.ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif
	}

	SGXTQ_SetTAGState(& sPDSPrimValues,
					0,
					ui32SrcDevVAddr,
					SGXTQ_FILTERTYPE_POINT,
					ui32SrcWidth,
					ui32SrcHeight,
					ui32SrcLineStride,
					0,
					ui32SrcBytesPerPixel,
					IMG_TRUE,
					psSrcSurf->eMemLayout);

	/* this goes into the ISP stream with geometry list*/
	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			SGXTQ_PDSPRIMFRAG_SINGLESOURCE,
			& sPDSPrimValues,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	for (i = 0; i < psTAtlas->ui32NumMappings; i++)
	{
		IMG_RECT * psRect = & psTAtlas->psSrcRects[i];

		asTSPCoords[i].ui32Src0U0 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->x0, ui32SrcWidth);
		asTSPCoords[i].ui32Src0U1 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->x1, ui32SrcWidth);
		asTSPCoords[i].ui32Src0V0 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->y0, ui32SrcHeight);
		asTSPCoords[i].ui32Src0V1 = SGXTQ_FloatIntDiv((IMG_UINT32) psRect->y1, ui32SrcHeight);
	}

	/*
	 * Fast 2d ISP stream
	 */
	eError = SGXTQ_CreateISPF2DResource(psTQContext,
										psTQContext->apsPDSPrimResources[SGXTQ_PDSPRIMFRAG_SINGLESOURCE],
										psTQContext->apsPDSSecResources[SGXTQ_PDSSECFRAG_BASIC],
										psTAtlas->psDstRects,
										& asTSPCoords[0],
										psTAtlas->ui32NumMappings,
										bTranslucent,
										psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}


	/*
	 * Set up the PBE
	 */
	eError = SGXTQ_SetPBEState(& sDstRect,
							psDstSurf->eMemLayout,
							ui32DstWidth,
							ui32DstHeight,
							ui32DstLineStride,
							0,
							ui32DstDevVAddr,
							0,
							SGXTQ_ROTATION_NONE,
							((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE),
							IMG_TRUE,
							pui32PBEState);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}



	/* And create the pixevent stuff */
	/* create EOT */
	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									pui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* create pixev */
	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	SGXTQ_SetupTransferRenderBox(psSubmit,
			(IMG_UINT32) sDstRect.x0, (IMG_UINT32) sDstRect.y0,
			(IMG_UINT32) sDstRect.x1, (IMG_UINT32) sDstRect.y1);

	SGXTQ_SetupTransferRegs(psTQContext,
			0,
			psSubmit,
			psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
			1,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
			EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
			0,
#endif
			EUR_CR_ISP_RENDER_TYPE_FAST2D);

	return PVRSRV_OK;
}


static const SGXTQ_VPBCOEFFS sInvCoeffs_BT601 =
{
/*  R     , G   , B   , const , shift ,      */
	33    , 65  , 13  , 2048  , 7     , /* Y */
	-11   , -19 , 28  , 8192  , 6     , /* U */
	28    , -24 , -5  , 8192  , 6     , /* V */
};


static PVRSRV_ERROR PrepareARGB2NV12Blit(SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext,
									SGX_QUEUETRANSFER				* psQueueTransfer,
									IMG_UINT32						ui32Pass,
									SGXTQ_PREP_INTERNAL				* psPassData,
									SGXMKIF_TRANSFERCMD				* psSubmit,
									PVRSRV_TRANSFER_SGX_KICK		* psKick,
									IMG_UINT32						* pui32PassesRequired)
{
	SGXTQ_TSP_COORDS	sTSPCoords;

	/* get these from the passdata */
	SGXTQ_PDS_UPDATE	* psPDSUpdate   = & psPassData->sPDSUpdate;
	SGXTQ_PDS_UPDATE	sPDSSecUpdate;
	IMG_UINT32			* pui32PBEState = psPassData->aui32PBEState;

	SGXTQ_USEFRAGS		eUSEProg; 

	SGXTQ_SURFACE		* psSrcSurf;
	IMG_UINT32			ui32SrcDevVAddr;
	IMG_UINT32			* pui32SrcWidth = & psPassData->Details.sARGB2NV12Data.ui32SrcWidth;
	IMG_UINT32			ui32SrcHeight;
	IMG_UINT32			* pui32SrcLineStride = & psPassData->Details.sARGB2NV12Data.ui32SrcLineStride;
	IMG_UINT32			ui32SrcBytesPerPixel;

	SGXTQ_SURFACE		* psDstSurf;
	IMG_UINT32			ui32YDevVAddr;
	IMG_UINT32			ui32UVDevVAddr;
	IMG_UINT32			* pui32DstWidth = & psPassData->Details.sARGB2NV12Data.ui32DstWidth;
	IMG_UINT32			ui32DstHeight;
	IMG_UINT32			* pui32DstLineStride = & psPassData->Details.sARGB2NV12Data.ui32DstLineStride;

	PVRSRV_ERROR		eError;

	IMG_RECT			sDstRect;

	/* tell the dispatcher to call us 2x */
	* pui32PassesRequired = 2;

	/* basic validation: 1 src and 1 dst - no sub texturing */ 
	if (ui32Pass == 0 /* short circuit on the 2nd pass - we are already validated*/
			&& (psQueueTransfer->ui32NumSources != 1
				|| psQueueTransfer->ui32NumDest != 1
				|| SGX_QUEUETRANSFER_NUM_SRC_RECTS(*psQueueTransfer) != 0
				|| SGX_QUEUETRANSFER_NUM_DST_RECTS(*psQueueTransfer) != 0
				|| psQueueTransfer->asSources[0].ui32ChunkStride != 0
				|| psQueueTransfer->asDests[0].ui32ChunkStride != 0))
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/* Set up the sync infos */ 
	if (psQueueTransfer->asSources[0].psSyncInfo)
	{
		PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asSources[0].psSyncInfo;

		psKick->ahSrcSyncInfo[psKick->ui32NumSrcSync] = psSyncInfo->hKernelSyncInfo;
		psKick->ui32NumSrcSync = 1;
	}
	else
	{
		psKick->ui32NumSrcSync = 0;
	}

	if (psQueueTransfer->asDests[0].psSyncInfo)
	{
		PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = psQueueTransfer->asDests[0].psSyncInfo;

		psKick->ahDstSyncInfo[psKick->ui32NumDstSync] = psSyncInfo->hKernelSyncInfo;
		psKick->ui32NumDstSync++;
	}
	else
	{
		psKick->ui32NumDstSync = 0;
	}

	psSrcSurf       = & psQueueTransfer->asSources[0];
	ui32SrcDevVAddr = SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf);
	ui32SrcHeight   = psSrcSurf->ui32Height;

	psDstSurf       = & psQueueTransfer->asDests[0];
	ui32YDevVAddr   = SGX_TQSURFACE_GET_DEV_VADDR(*psDstSurf);
	ui32UVDevVAddr  = psQueueTransfer->Details.sARGB2NV12.sUVDevVAddr.uiAddr;
	ui32DstHeight   = psDstSurf->ui32Height;


	psSubmit->bLoadFIRCoefficients = IMG_TRUE;
	psSubmit->sHWRegs.ui32FIRHFilterTable = 3;

	if (ui32Pass == 0)
	{
		/* write Y Plane */

		/* alignment checks */
		if (! ISALIGNED(ui32SrcDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT)
			|| ! ISALIGNED(ui32YDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT)
			|| ! ISALIGNED(ui32UVDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT))
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}

		/* initialize PBE / TF texturing on pass 0 to 0 */
		PVRSRVMemSet(psPDSUpdate->asLayers[0].aui32TAGState,
				0,
				sizeof(IMG_UINT32) * EURASIA_TAG_TEXTURE_STATE_SIZE);
		PVRSRVMemSet(pui32PBEState, 0, sizeof(IMG_UINT32) * SGXTQ_PBESTATE_WORDS);


		/* get the pixel information to sample the source */
		if (PVRSRV_OK != (eError = SGXTQ_GetPixelFormats(psSrcSurf->eFormat,
				IMG_TRUE,
				PVRSRV_PIXEL_FORMAT_MONO8,
				IMG_TRUE,
				SGXTQ_FILTERTYPE_POINT,
				psPDSUpdate->asLayers[0].aui32TAGState,
				IMG_NULL,
				& ui32SrcBytesPerPixel,
				pui32PBEState,
				IMG_NULL,
				0,
				IMG_NULL)))
		{
			return eError;
		}

		/* check the strides */
		if ((PVRSRV_OK != SGXTQ_GetSurfaceStride(psSrcSurf, ui32SrcBytesPerPixel, IMG_TRUE, IMG_FALSE, pui32SrcLineStride))
				|| (PVRSRV_OK != SGXTQ_GetSurfaceStride(psDstSurf, 1, IMG_TRUE, IMG_FALSE, pui32DstLineStride))
				|| (PVRSRV_OK != SGXTQ_GetSurfaceWidth(psSrcSurf,  ui32SrcBytesPerPixel, IMG_TRUE, IMG_FALSE, pui32SrcWidth))
				|| (PVRSRV_OK != SGXTQ_GetSurfaceWidth(psDstSurf, 1, IMG_TRUE, IMG_FALSE, pui32DstWidth)))
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		if (! ISALIGNED(*pui32DstLineStride / 2, EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT))
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}

		/* tell the PBE/ISP to render every pixel on the destination*/
		sDstRect.x0 = 0;
		sDstRect.y0 = 0;
		sDstRect.x1 = (IMG_INT32) *pui32DstWidth;
		sDstRect.y1 = (IMG_INT32) ui32DstHeight;

		/* set up the TAG to sample each texel (no sub texturing..) */
		SGXTQ_SetTAGState(psPDSUpdate,
				0,
				ui32SrcDevVAddr,
				SGXTQ_FILTERTYPE_POINT,
				* pui32SrcWidth,
				ui32SrcHeight,
				* pui32SrcLineStride,
				0,
				ui32SrcBytesPerPixel,
				IMG_TRUE,
				psSrcSurf->eMemLayout);

		psPDSUpdate->asLayers[0].aui32ITERState[0] = EURASIA_PDS_DOUTI_USEISSUE_NONE
			| EURASIA_PDS_DOUTI_TEXISSUE_TC0
			| EURASIA_PDS_DOUTI_TEXLASTISSUE;

#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
		psPDSUpdate->asLayers[0].aui32ITERState[1] = (0 << EURASIA_PDS_DOUTI1_USESAMPLE_RATE_SHIFT)
			| (0 << EURASIA_PDS_DOUTI1_TEXSAMPLE_RATE_SHIFT);
#endif

		eUSEProg = SGXTQ_USEBLIT_ARGB2NV12_Y_PLANE;

		/*
		 * Set up the PBE
		 */
		eError = SGXTQ_SetPBEState(& sDstRect,
				psDstSurf->eMemLayout,
				* pui32DstWidth,
				ui32DstHeight,
				* pui32DstLineStride,
				0,
				ui32YDevVAddr,
				0,
				SGXTQ_ROTATION_NONE,
				((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_DITHER) ? IMG_TRUE : IMG_FALSE),
				IMG_TRUE,
				pui32PBEState);
		if (eError != PVRSRV_OK)
		{
			return eError;
		}

		/* FIR coefficients */
		psSubmit->sHWRegs.ui32FIRHFilterLeft2 =
			(((IMG_UINT32)sInvCoeffs_BT601.i16YR << EUR_CR_USE_FILTER2_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP0_MASK)
			| (((IMG_UINT32)sInvCoeffs_BT601.i16UR << EUR_CR_USE_FILTER2_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP3_MASK);

		psSubmit->sHWRegs.ui32FIRHFilterRight2 =
			(((IMG_UINT32)sInvCoeffs_BT601.i16VR << EUR_CR_USE_FILTER2_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER2_RIGHT_TAP6_MASK);

		psSubmit->sHWRegs.ui32FIRHFilterExtra2 =
			(((IMG_UINT32)sInvCoeffs_BT601.i16ConstR << EUR_CR_USE_FILTER2_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_CONSTANT_MASK)
			| (((IMG_UINT32)sInvCoeffs_BT601.i16ShiftR << EUR_CR_USE_FILTER2_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_SHR_MASK);
	}
	else
	{
		/* write UV Plane */
		/* set up the TAG to sample 4 texels (no sub texturing..) */
		psPDSUpdate->asLayers[0].aui32TAGState[0] &= EURASIA_PDS_DOUTT0_MINFILTER_CLRMSK | EURASIA_PDS_DOUTT0_MAGFILTER_CLRMSK;
		psPDSUpdate->asLayers[0].aui32TAGState[0] |= SGXTQ_FilterFromEnum(SGXTQ_FILTERTYPE_LINEAR);

		eUSEProg = SGXTQ_USEBLIT_ARGB2NV12_UV_PLANE;

		/*
		 * Set up the PBE
		 */

		/* get the pixel information for writing the UV plane*/
		if (PVRSRV_OK != (eError = SGXTQ_GetPixelFormats(PVRSRV_PIXEL_FORMAT_UNKNOWN,
				IMG_TRUE,
				PVRSRV_PIXEL_FORMAT_MONO16,
				IMG_TRUE,
				SGXTQ_FILTERTYPE_POINT,
				IMG_NULL,
				IMG_NULL,
				IMG_NULL,
				pui32PBEState,
				IMG_NULL,
				0,
				IMG_NULL)))
		{
			return eError;
		}

		/* tell the PBE/ISP that we have quarter of the samples */
		sDstRect.x0 = 0;
		sDstRect.y0 = 0;
		sDstRect.x1 = (IMG_INT32) *pui32DstWidth / 2;
		sDstRect.y1 = (IMG_INT32) ui32DstHeight / 2;

		ui32UVDevVAddr >>= EURASIA_PIXELBE_FBADDR_ALIGNSHIFT;
		*pui32DstLineStride /= 2;
		*pui32DstLineStride >>= EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT;
		*pui32DstLineStride -= 1;

#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
		pui32PBEState[1] &= EURASIA_PIXELBES0HI_FBADDR_CLRMSK;
		ACCUMLATESET(pui32PBEState[1], ui32UVDevVAddr, EURASIA_PIXELBES0HI_FBADDR);

		pui32PBEState[2] &= EURASIA_PIXELBES1LO_LINESTRIDE_CLRMSK;
		ACCUMLATESET(pui32PBEState[2], *pui32DstLineStride, EURASIA_PIXELBES1LO_LINESTRIDE);

		pui32PBEState[5] &= EURASIA_PIXELBES2HI_XMAX_CLRMSK;
		pui32PBEState[5] &= EURASIA_PIXELBES2HI_YMAX_CLRMSK;
		ACCUMLATESET(pui32PBEState[5], ((IMG_UINT32)(sDstRect.x1) - 1), EURASIA_PIXELBES2HI_XMAX);
		ACCUMLATESET(pui32PBEState[5], ((IMG_UINT32)(sDstRect.y1) - 1), EURASIA_PIXELBES2HI_YMAX);
#else
		pui32PBEState[1] &= EURASIA_PIXELBE1S1_XMAX_CLRMSK;
		pui32PBEState[1] &= EURASIA_PIXELBE1S1_YMAX_CLRMSK;
		ACCUMLATESET(pui32PBEState[1], ((IMG_UINT32)(sDstRect.x1-1)), EURASIA_PIXELBE1S1_XMAX);
		ACCUMLATESET(pui32PBEState[1], ((IMG_UINT32)(sDstRect.y1-1)), EURASIA_PIXELBE1S1_YMAX);

#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
		pui32PBEState[4] &= EURASIA_PIXELBE2S2_LINESTRIDE_CLRMSK;
		ACCUMLATESET(pui32PBEState[4], *pui32DstLineStride, EURASIA_PIXELBE2S2_LINESTRIDE);
#else
		pui32PBEState[2] &= EURASIA_PIXELBE2S0_LINESTRIDE_CLRMSK;
		ACCUMLATESET(pui32PBEState[2], *pui32DstLineStride, EURASIA_PIXELBE2S0_LINESTRIDE);
#endif
		pui32PBEState[3] &= EURASIA_PIXELBE2S1_FBADDR_CLRMSK;
		ACCUMLATESET(pui32PBEState[3], ui32UVDevVAddr, EURASIA_PIXELBE2S1_FBADDR);
#endif
		/* FIR coefficients */
		psSubmit->sHWRegs.ui32FIRHFilterLeft1 =
			(((IMG_UINT32)sInvCoeffs_BT601.i16VG << EUR_CR_USE_FILTER1_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP0_MASK)
			| (((IMG_UINT32)sInvCoeffs_BT601.i16UG << EUR_CR_USE_FILTER1_LEFT_TAP1_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP1_MASK)
			| (((IMG_UINT32)sInvCoeffs_BT601.i16YG << EUR_CR_USE_FILTER1_LEFT_TAP2_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP2_MASK);

		psSubmit->sHWRegs.ui32FIRHFilterRight1 = 0;

		psSubmit->sHWRegs.ui32FIRHFilterExtra1 =
			(((IMG_UINT32)sInvCoeffs_BT601.i16ConstG << EUR_CR_USE_FILTER1_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER1_EXTRA_CONSTANT_MASK)
			| (((IMG_UINT32)sInvCoeffs_BT601.i16ShiftG << EUR_CR_USE_FILTER1_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER1_EXTRA_SHR_MASK);

		psSubmit->sHWRegs.ui32FIRHFilterLeft2 =
			(((IMG_UINT32)sInvCoeffs_BT601.i16YB << EUR_CR_USE_FILTER2_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP0_MASK)
			| (((IMG_UINT32)sInvCoeffs_BT601.i16UB << EUR_CR_USE_FILTER2_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP3_MASK);

		psSubmit->sHWRegs.ui32FIRHFilterRight2 =
			(((IMG_UINT32)sInvCoeffs_BT601.i16VB << EUR_CR_USE_FILTER2_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER2_RIGHT_TAP6_MASK);

		psSubmit->sHWRegs.ui32FIRHFilterExtra2 =
			(((IMG_UINT32)sInvCoeffs_BT601.i16ConstB << EUR_CR_USE_FILTER2_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_CONSTANT_MASK)
			| (((IMG_UINT32)sInvCoeffs_BT601.i16ShiftB << EUR_CR_USE_FILTER2_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_SHR_MASK);
	}

	/* create the shader */
	eError = SGXTQ_CreateUSEResource(psTQContext,
			eUSEProg,
			IMG_NULL,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * PDS primary
	 */
	SGXTQ_SetUSSEKick(psPDSUpdate,
					psTQContext->apsUSEResources[eUSEProg]->sDevVAddr,
					psTQContext->sUSEExecBase,
					psTQContext->apsUSEResources[eUSEProg]->uResource.sUSE.ui32NumTempRegs);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	psPDSUpdate->ui32U1 |= EURASIA_PDS_DOUTU1_TEXTUREDEPENDENCY;
#else
	psPDSUpdate->ui32U0 |= EURASIA_PDS_DOUTU0_TEXTUREDEPENDENCY;
#endif

	/* this goes into the ISP stream with geometry list*/
	eError = SGXTQ_CreatePDSPrimResource(psTQContext,
			SGXTQ_PDSPRIMFRAG_SINGLESOURCE,
			psPDSUpdate,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/*
	 * PDS secondary
	 */
	eError = SGXTQ_CreateUSESecondaryResource(psTQContext,
			SGXTQ_PDSSECFRAG_BASIC,
			& sPDSSecUpdate,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	eError = SGXTQ_CreatePDSSecResource(psTQContext,
			SGXTQ_PDSSECFRAG_BASIC,
			& sPDSSecUpdate,
			psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	PVRSRVMemSet((IMG_PBYTE)&(sTSPCoords), 0, sizeof (sTSPCoords));
	sTSPCoords.ui32Src0U1 = FLOAT32_ONE;
	sTSPCoords.ui32Src0V1 = FLOAT32_ONE;

	/* ISP ctrl stream */
	eError = SGXTQ_CreateISPResource(psTQContext,
									psTQContext->apsPDSPrimResources[SGXTQ_PDSPRIMFRAG_SINGLESOURCE],
									psTQContext->apsPDSSecResources[SGXTQ_PDSSECFRAG_BASIC],
									& sDstRect,
									& sTSPCoords,
									IMG_FALSE,
									(psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERSE_TRIANGLE) ? IMG_TRUE : IMG_FALSE,
									1,
									psQueueTransfer->bPDumpContinuous,
									0);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* And create the pixevent stuff */
	/* create EOT */
	eError = SGXTQ_CreateUSEEOTHandler(psTQContext,
									pui32PBEState,
									SGXTQ_USEEOTHANDLER_BASIC,
									0, 0,
									psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	/* create pixev */
	eError = SGXTQ_CreatePDSPixeventHandler(psTQContext,
											psTQContext->psUSEEORHandler,
											psTQContext->apsUSEEOTHandlers[SGXTQ_USEEOTHANDLER_BASIC],
											SGXTQ_PDSPIXEVENTHANDLER_BASIC,
											psQueueTransfer->bPDumpContinuous);
	if (PVRSRV_OK != eError)
	{
		return eError;
	}

	SGXTQ_SetupTransferRenderBox(psSubmit,
			(IMG_UINT32) sDstRect.x0, (IMG_UINT32) sDstRect.y0,
			(IMG_UINT32) sDstRect.x1, (IMG_UINT32) sDstRect.y1);

	SGXTQ_SetupTransferRegs(psTQContext,
			0,
			psSubmit,
			psTQContext->apsPDSPixeventHandlers[SGXTQ_PDSPIXEVENTHANDLER_BASIC],
			1,
#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
			EUR_CR_ISP_RENDER_FAST_SCANDIR_TL2BR,
#else
			0,
#endif
			EUR_CR_ISP_RENDER_TYPE_FASTSCALE);

	return PVRSRV_OK;
}


/*****************************************************************************
 * Function Name		:	SGXSubmitTransfer
 * Inputs				:	hTransferContext - The context created by CreateTransferContext()
 * 							psSubmitTransfer - Contains the prepared Cmd from SGXQueueTransfer plus
 * 							some data from the client
 * Outputs				:	Error status
 * Description			:	If SGXTQ_PREP_SUBMIT_SEPERATE is defined then the client calls this to submit the
 * 							transfer cmds to the hardware.
 *							If SGXTQ_PREP_SUBMIT_SEPERATE isn't defined then this function is meant to be internal
 *							in services.
 ******************************************************************************/
#if defined(SGXTQ_PREP_SUBMIT_SEPERATE)
IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV
#else
#if defined(SGX_FEATURE_PTLA)
IMG_INTERNAL PVRSRV_ERROR
#else
static PVRSRV_ERROR
#endif
#endif
SGXSubmitTransfer(IMG_HANDLE hTransferContext,
				  SGX_SUBMITTRANSFER *psSubmitTransfer)
{
	IMG_UINT32 ui32Count;
	SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext;
	SGXTQ_SUBMITS* psSubmits;
	PVRSRV_ERROR eError;

#if defined(PDUMP)
	const PVRSRV_CONNECTION* psConnection;
	IMG_UINT32 ui32PDumpFlags;
#endif


	psTQContext = (SGXTQ_CLIENT_TRANSFER_CONTEXT *) hTransferContext;
	psSubmits = (SGXTQ_SUBMITS*) psSubmitTransfer->hTransferSubmit;

#if defined(PDUMP)
	psConnection = psTQContext->psDevData->psConnection;
#endif

	/* client may have modified the submit since calling QueueTransfer()*/
	if(psSubmits->ui32NumSubmits > SGXTQ_MAX_COMMANDS)
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXSubmitTransfer: submits greater than maximum value"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/*
	 *  Iterate through each submit requested
	 */
	for (ui32Count = 0; ui32Count < psSubmits->ui32NumSubmits; ui32Count++)
	{
		SGX_CLIENT_CCB				* psCCB = psTQContext->psTransferCCB;
		IMG_UINT32					ui32RoundedCmdSize;
		IMG_UINT32					ui32RealCmdSize;
		IMG_UINT32					* pui32Dest;
		SGXMKIF_TRANSFERCMD			* psTransferCmd;
		SGXMKIF_TRANSFERCMD_SHARED	* psShared;
		PVRSRV_TRANSFER_SGX_KICK	* psKick;
		IMG_UINT32					ui32StatusCount;

		psTransferCmd = &(psSubmits->asTransferCmd[ui32Count]);
		psShared = &psTransferCmd->sShared;
		psKick = &(psSubmits->asKick[ui32Count]);

		PVR_ASSERT(psTransferCmd->ui32NumUpdates <= SGXTQ_MAX_UPDATES);
		/* first let's figure out the actual size by dropping the non-used updates at the end*/
		ui32RealCmdSize = sizeof(SGXMKIF_TRANSFERCMD) - sizeof(SGXMKIF_MEMUPDATE) * (SGXTQ_MAX_UPDATES - psTransferCmd->ui32NumUpdates);
		/* and then get the size based on the CCB granularity*/
		ui32RoundedCmdSize = SGXCalcContextCCBParamSize(ui32RealCmdSize, SGX_CCB_ALLOCGRAN);

		psTransferCmd->ui32Size = ui32RoundedCmdSize;
		pui32Dest = SGXAcquireCCB(psTQContext->psDevData, psCCB, ui32RoundedCmdSize, psTQContext->hOSEvent);
		if (!pui32Dest)
		{
			return PVRSRV_ERROR_TIMEOUT;
		}

		psKick->hCCBMemInfo = psCCB->psCCBClientMemInfo->hKernelMemInfo;

		psKick->ui32SharedCmdCCBOffset = CURRENT_CCB_OFFSET(psCCB) + offsetof(SGXMKIF_TRANSFERCMD, sShared);


		/* Command needs to be synchronised with the TA? */
		psKick->hTASyncInfo = (psTransferCmd->ui32Flags & SGXMKIF_TQFLAGS_TATQ_SYNC) ? psTQContext->psTASyncObject->hKernelSyncInfo : IMG_NULL;

		/* Command needs to be synchronised with 3D? */
		psKick->h3DSyncInfo = (psTransferCmd->ui32Flags & SGXMKIF_TQFLAGS_3DTQ_SYNC) ? psTQContext->ps3DSyncObject->hKernelSyncInfo : IMG_NULL;

		/*
		 * Copy the status values from the submit - (last sub blit only)
		 */
		if (ui32Count >= psSubmits->ui32NumSubmits - 1)
		{
			for (ui32StatusCount = 0; ui32StatusCount < psSubmitTransfer->ui32NumStatusValues; ui32StatusCount++)
			{
				if (psShared->ui32NumStatusVals < SGXTQ_MAX_STATUS)
				{
					IMG_UINT32 ui32UpdAddr = psSubmitTransfer->asMemUpdates[ui32StatusCount].ui32UpdateAddr;
					IMG_UINT32 ui32UpdVal = psSubmitTransfer->asMemUpdates[ui32StatusCount].ui32UpdateVal;

					psShared->sCtlStatusInfo[psShared->ui32NumStatusVals].sStatusDevAddr.uiAddr = ui32UpdAddr;
					psShared->sCtlStatusInfo[psShared->ui32NumStatusVals].ui32StatusValue = ui32UpdVal;
					psShared->ui32NumStatusVals++;
				}
				else
				{
					PVR_DPF((PVR_DBG_ERROR, "SGXSubmitTransfer: attempted to write beyond the bounds of psShared->sCtlStatusInfo array"));
					PVR_DBG_BREAK;
					return PVRSRV_ERROR_CMD_TOO_BIG;
				}
			}
		}


		/* Put the fence into the status updates*/
		if (psShared->ui32NumStatusVals < SGXTQ_MAX_STATUS)
		{
			IMG_UINT32 ui32UpdAddr = psTQContext->psFenceIDMemInfo->sDevVAddr.uiAddr;

			psShared->sCtlStatusInfo[psShared->ui32NumStatusVals].sStatusDevAddr.uiAddr = ui32UpdAddr;
			psShared->sCtlStatusInfo[psShared->ui32NumStatusVals].ui32StatusValue = psSubmits->aui32FenceIDs[ui32Count];
			psShared->ui32NumStatusVals++;
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR, "SGXSubmitTransfer: Can't insert the fence id update into the cmd"));
			return PVRSRV_ERROR_UNABLE_TO_INSERT_FENCE_ID;
		}

		PVR_ASSERT((IMG_UINTPTR_T)pui32Dest-(IMG_UINTPTR_T)psCCB->psCCBClientMemInfo->pvLinAddr + ui32RealCmdSize < psCCB->psCCBClientMemInfo->uAllocSize  );
		/* Copy command structure into the CCB */
		PVRSRVMemCopy(pui32Dest, (IMG_PUINT32) psTransferCmd, ui32RealCmdSize); 

#if defined(PDUMP)
		ui32PDumpFlags = (psSubmitTransfer->bPDumpContinuous ? PDUMP_FLAGS_CONTINUOUS : 0);
		psKick->ui32PDumpFlags = ui32PDumpFlags;
		if (PDUMPISCAPTURINGTEST(psConnection) || psSubmitTransfer->bPDumpContinuous)
		{
			/* Pdump the command from the per context CCB */
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "Transfer command\r\n");
			PDUMPMEM(psConnection,
					pui32Dest,
					psCCB->psCCBClientMemInfo,
					psCCB->ui32CCBDumpWOff,
					sizeof (SGXMKIF_TRANSFERCMD),
					ui32PDumpFlags);

			psKick->ui32CCBDumpWOff = psCCB->ui32CCBDumpWOff + offsetof(SGXMKIF_TRANSFERCMD, sShared);
			UPDATE_CCB_OFFSET(psCCB->ui32CCBDumpWOff, ui32RoundedCmdSize, psCCB->ui32Size);

			/* Make sure we pdumped the update of the WOFF */
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "Update the TQ CCB WOFF\r\n");
			PDUMPMEM(psConnection,
					&psCCB->ui32CCBDumpWOff,
					psCCB->psCCBCtlClientMemInfo,
					offsetof(PVRSRV_SGX_CCB_CTL, ui32WriteOffset),
					sizeof (IMG_UINT32),
					ui32PDumpFlags);
		}
#endif

		psKick->sHWTransferContextDevVAddr = psTQContext->sHWTransferContextDevVAddr;

		/* Update the CCB*/
		UPDATE_CCB_OFFSET(*psCCB->pui32WriteOffset, ui32RoundedCmdSize, psCCB->ui32Size);

		psKick->hDevMemContext = psTQContext->hDevMemContext;

		/* do the submit */
		eError = SGXSubmitTransferBridge(psTQContext->psDevData, psKick);
		if (PVRSRV_OK != eError)
		{
			/* the cmd is already in the queue, - we can't really handle errors this late*/
			PVR_DPF((PVR_DBG_ERROR, "SGXSubmitTransfer: fatal - bridge call returned error"));
			PVR_DBG_BREAK;
			return eError;
		}

#if defined(NO_HARDWARE)
		/* Update read offset */
		UPDATE_CCB_OFFSET(*psCCB->pui32ReadOffset, ui32RoundedCmdSize, psCCB->ui32Size);

		/* write back the fence ID*/
		*((IMG_UINT32 *) psTQContext->psFenceIDMemInfo->pvLinAddr) = psTQContext->ui32FenceID;
#endif

#if defined(PDUMP)
		/*
		 * Never pdump last only poll & associated comments if the process is marked as
		 * persistent (e.g. Xserver).
		 */
		if ((psConnection->ui32SrvFlags & SRV_FLAGS_PERSIST) == 0)
		{
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "LASTONLY{\r\n");
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "Poll for transfer command to complete");
			/* FIXME: when the sync object gets into the kernel we want to change this to polling
			 * on that one instead.*/
#if defined (SUPPORT_SID_INTERFACE)
			PDUMPMEMPOL(psConnection,
					psCCB->psCCBCtlClientMemInfo->hKernelMemInfo,
					offsetof(PVRSRV_SGX_CCB_CTL, ui32ReadOffset),
					psCCB->ui32CCBDumpWOff,
					0xffffffff,
					ui32PDumpFlags);
#else
			PDUMPMEMPOL(psConnection,
					psCCB->psCCBCtlClientMemInfo,
					offsetof(PVRSRV_SGX_CCB_CTL, ui32ReadOffset),
					psCCB->ui32CCBDumpWOff,
					0xffffffff,
					ui32PDumpFlags);
#endif

			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "}LASTONLY\r\n");
		}
#endif
	}

#if defined(SGX_FEATURE_2D_HARDWARE)
	/*
	 *  Iterate through each 2D core submit requested
	 */
	for (ui32Count = 0; ui32Count < psSubmits->ui32Num2DSubmits; ui32Count++)
	{
#if defined(PDUMP) || defined(NO_HARDWARE)
		SGX_CLIENT_CCB *psCCB = psTQContext->ps2DCCB;
#endif
		SGXMKIF_2DCMD_SUBMIT *ps2DSubmit;
		SGXMKIF_2DCMD *ps2DCmd;
		PVRSRV_2D_SGX_KICK *ps2DKick;

		ps2DSubmit = &(psSubmits->as2DSubmit[ui32Count]);
		ps2DCmd = ps2DSubmit->ps2DCmd;
		ps2DKick = &ps2DSubmit->s2DKick;

		/* Command needs to be synchronised with the TA? */
		ps2DKick->hTASyncInfo = (ps2DCmd->ui32Flags & SGXMKIF_TQFLAGS_TATQ_SYNC) ? psTQContext->psTASyncObject->hKernelSyncInfo : IMG_NULL;

		ps2DKick->h3DSyncInfo = (ps2DCmd->ui32Flags & SGXMKIF_TQFLAGS_3DTQ_SYNC) ? psTQContext->ps3DSyncObject->hKernelSyncInfo : IMG_NULL;

		ps2DKick->sHW2DContextDevVAddr = psTQContext->sHW2DContextDevVAddr;

#if defined(PDUMP)
		ui32PDumpFlags = (psSubmitTransfer->bPDumpContinuous ? PDUMP_FLAGS_CONTINUOUS : 0);
		ps2DKick->ui32PDumpFlags = ui32PDumpFlags;
		if (PDUMPISCAPTURINGTEST(psConnection) || psSubmitTransfer->bPDumpContinuous)
		{
			/* Pdump the command from the per context CCB */
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "2D command\r\n");
			PDUMPMEM(psConnection,
					ps2DCmd,
					psCCB->psCCBClientMemInfo,
					psCCB->ui32CCBDumpWOff,
					sizeof (SGXMKIF_2DCMD),
					ui32PDumpFlags);

			ps2DKick->ui32CCBDumpWOff = psCCB->ui32CCBDumpWOff + offsetof(SGXMKIF_2DCMD, sShared);
			UPDATE_CCB_OFFSET(psCCB->ui32CCBDumpWOff, ps2DCmd->ui32Size, psCCB->ui32Size);
			/* Make sure we pdumped the update of the WOFF */
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "Update the 2D CCB WOFF\r\n");
			PDUMPMEM(psConnection,
					&psCCB->ui32CCBDumpWOff,
					psCCB->psCCBCtlClientMemInfo,
					offsetof(PVRSRV_SGX_CCB_CTL, ui32WriteOffset),
					sizeof (IMG_UINT32),
					ui32PDumpFlags);
		}
#endif
		ps2DKick->hDevMemContext = psTQContext->hDevMemContext;

		/* do the submit */
		SGXSubmit2D(psTQContext->psDevData, ps2DKick);

#if defined(NO_HARDWARE)
		/* Update read offset */
		UPDATE_CCB_OFFSET(*psCCB->pui32ReadOffset, ps2DCmd->ui32Size, psCCB->ui32Size);
#endif

#if defined(PDUMP)
		/*
		 * Never pdump last only poll & associated comments if the process is marked as
		 * persistent (e.g. Xserver).
		 */
		if((psConnection->ui32SrvFlags & SRV_FLAGS_PERSIST) == 0)
		{
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "LASTONLY{\r\n");
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "Poll for 2D command to complete");
			/* FIXME: when the sync object gets into the kernel we want to change this to polling
			* on that one instead.*/
#if defined (SUPPORT_SID_INTERFACE)
			PDUMPMEMPOL(psConnection,
						psCCB->psCCBCtlClientMemInfo->hKernelMemInfo,
						offsetof(PVRSRV_SGX_CCB_CTL, ui32ReadOffset),
						psCCB->ui32CCBDumpWOff,
						0xffffffff,
						ui32PDumpFlags);
#else
			PDUMPMEMPOL(psConnection,
						psCCB->psCCBCtlClientMemInfo,
						offsetof(PVRSRV_SGX_CCB_CTL, ui32ReadOffset),
						psCCB->ui32CCBDumpWOff,
						0xffffffff,
						ui32PDumpFlags);
#endif
	
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "}LASTONLY\r\n");
		}
#endif
	}
#endif /* SGX_FEATURE_2D_HARDWARE*/

	return PVRSRV_OK;
}
#endif


#ifdef BLITLIB
/**
 * If its a planar surface, fills the psPlanarInfo and returns the pointer,
 * returns a null pointer otherwise.
 */
static BL_PLANAR_SURFACE_INFO *BL_GetPlanarSurfaceInfo(SGXTQ_SURFACE *psSGXTQSrcSurface,
                                                       BL_PLANAR_SURFACE_INFO *psPlanarInfo)
{
	if (psSGXTQSrcSurface->ui32ChunkStride > 0)
	{
		psPlanarInfo->bIsPlanar = IMG_TRUE;
		psPlanarInfo->i32ChunkStride = psSGXTQSrcSurface->ui32ChunkStride;
		psPlanarInfo->uiPixelBytesPerPlane = sizeof(IMG_UINT32); /*XXX check if this is true for all platforms*/
		psPlanarInfo->uiNChunks = gas_BLExternalPixelTable[psSGXTQSrcSurface->eFormat].ui32BytesPerPixel / psPlanarInfo->uiPixelBytesPerPlane;
		return psPlanarInfo;
	}
	else
	{
		return IMG_NULL;
	}
}

#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
#define IS_HYBRID_TWIDDLING IMG_TRUE
#else
#define IS_HYBRID_TWIDDLING IMG_FALSE
#endif


/*****************************************************************************
 * Function Name		:	BL_WaitForSyncInfo
 * Inputs				:	psTQContext - The SGX TQ surface.
 *							hSyncModObject - 
 *							psSyncInfo - 
 * Returns				:	error code 
 * Description			:	Stalls the caller until there is any outstanding operation
 *							indicated by psSyncInfo, then takes a lock on it as an atomic
 *							operation.
 *							Caller is expected to call PVRSRVModifyCompleteSyncOps() at some
 *							point after this.
 ******************************************************************************/
static PVRSRV_ERROR BL_WaitForSyncInfo(SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext,
#if defined (SUPPORT_SID_INTERFACE)
										IMG_SID							hSyncModObject,
#else
										IMG_HANDLE						hSyncModObject,
#endif
										PVRSRV_CLIENT_SYNC_INFO			* psSyncInfo)
{
	PVRSRV_ERROR	eError = PVRSRV_OK;
	IMG_UINT32		ui32WaitCount = WAIT_TRY_COUNT;

	eError = PVRSRVModifyPendingSyncOps(psTQContext->psDevData->psConnection,
			hSyncModObject,
			psSyncInfo,
			PVRSRV_MODIFYSYNCOPS_FLAGS_WO_INC | PVRSRV_MODIFYSYNCOPS_FLAGS_RO_INC,
			IMG_NULL,
			IMG_NULL);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	
	/* unfortunately this doesn't support flagging wait in kernel yet.
	 * once that's done the loop can be removed and we can simply set
	 * bWait in the API.
	 */ 
	for (ui32WaitCount = WAIT_TRY_COUNT; ui32WaitCount > 0; ui32WaitCount--)
	{
		eError = PVRSRVSyncOpsFlushToModObj(psTQContext->psDevData->psConnection,
				hSyncModObject,
				IMG_FALSE);
		if (eError != PVRSRV_ERROR_RETRY)
		{
			return eError;
		}

		/* we failed in this round, wait some time and try again*/
		if (psTQContext->hOSEvent != IMG_NULL)
		{
			(IMG_VOID)PVRSRVEventObjectWait(psTQContext->psDevData->psConnection,
					psTQContext->hOSEvent);
		}
		else
		{
			PVRSRVWaitus(MAX_HW_TIME_US/WAIT_TRY_COUNT);
		}
		eError = PVRSRV_ERROR_TIMEOUT;
	}
	
	/* and now we should be ready to [RW] the surface*/
	return eError;
}

/*****************************************************************************
 * Function Name		:	BL_SRC_From_SGXTQ_SURFACE
 * Inputs				:	psSGXTQSrcSurface - The SGX TQ surface.
 * Outputs				:	psBLSrc - The blitlib destination surface to be filled.
 * Description			:	Creates a blitlib source surface from a sgx
 * 							transfer queue surface.
 ******************************************************************************/
static BL_OBJECT* BL_SRC_From_SGXTQ_SURFACE(BL_SRC *psBLSrc,
											SGXTQ_SURFACE *psSGXTQSrcSurface)
{
	BL_OBJECT *psResult=0;
	BL_PLANAR_SURFACE_INFO sPlanarInfo;
	IMG_PBYTE pbySrcBaseLinAddr = 0;

	if (psSGXTQSrcSurface->psClientMemInfo != IMG_NULL)
	{
		pbySrcBaseLinAddr = (IMG_PBYTE)psSGXTQSrcSurface->psClientMemInfo->pvLinAddr;
	}

	switch (psSGXTQSrcSurface->eMemLayout)
	{
		case SGXTQ_MEMLAYOUT_STRIDE:
		case SGXTQ_MEMLAYOUT_OUT_LINEAR:
		{
			BLSRCLinearInit(&psBLSrc->linear,
							psSGXTQSrcSurface->i32StrideInBytes,
							psSGXTQSrcSurface->ui32Height,
							psSGXTQSrcSurface->eFormat,
							pbySrcBaseLinAddr + psSGXTQSrcSurface->uiMemoryOffset,
							BL_GetPlanarSurfaceInfo(psSGXTQSrcSurface,
							                        &sPlanarInfo));
			psResult = &psBLSrc->linear.sObject;
			break;
		}
		case SGXTQ_MEMLAYOUT_2D:
		case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
		{
			BLSRCTwiddledInit(&psBLSrc->twiddled,
							  psSGXTQSrcSurface->ui32Width,
							  psSGXTQSrcSurface->ui32Height,
							  psSGXTQSrcSurface->eFormat,
							  pbySrcBaseLinAddr + psSGXTQSrcSurface->uiMemoryOffset,
							  BL_GetPlanarSurfaceInfo(psSGXTQSrcSurface,
							                          &sPlanarInfo),
							  IS_HYBRID_TWIDDLING);
			psResult = &psBLSrc->twiddled.sObject;
			break;
		}
		case SGXTQ_MEMLAYOUT_TILED:
		case SGXTQ_MEMLAYOUT_OUT_TILED:
		{
			BLSRCTiledInit(&psBLSrc->tiled,
			               psSGXTQSrcSurface->i32StrideInBytes,
			               psSGXTQSrcSurface->ui32Height,
			               psSGXTQSrcSurface->eFormat,
			               pbySrcBaseLinAddr + psSGXTQSrcSurface->uiMemoryOffset,
			               BL_GetPlanarSurfaceInfo(psSGXTQSrcSurface,
			                                       &sPlanarInfo));
			psResult = &psBLSrc->tiled.sObject;
			break;
		}
		case SGXTQ_MEMLAYOUT_3D:
		case SGXTQ_MEMLAYOUT_CEM:
		{
			psResult = IMG_NULL;
		}
	}
	return psResult;
}

/*****************************************************************************
 * Function Name		:	BL_DST_From_SGXTQ_SURFACE
 * Inputs				:	psSGXTQSrcSurface - The SGX TQ surface.
 *							psUpstreamObject - The object that feeds pixels to
 *							the surface.
 * Outputs				:	psBLDst - The blitlib destination surface to be filled.
 * Description			:	Creates a blitlib destination surface from a sgx
 * 							transfer queue surface.
 ******************************************************************************/
static BL_OBJECT* BL_DST_From_SGXTQ_SURFACE(BL_DST *puBLDst,
											SGXTQ_SURFACE *psSGXTQDstSurface,
											BL_OBJECT *psUpstreamObject)
{
	BL_OBJECT *psResult=0; /*To avoid the caller to do the casting*/
	BL_PLANAR_SURFACE_INFO sPlanarInfo; /*temp object to be copied if necessary*/
	IMG_PBYTE pbyDstBaseLinAddr = 0;

	if (psSGXTQDstSurface->psClientMemInfo != IMG_NULL)
	{
		pbyDstBaseLinAddr = (IMG_PBYTE)psSGXTQDstSurface->psClientMemInfo->pvLinAddr;
	}

	switch (psSGXTQDstSurface->eMemLayout)
	{
		case SGXTQ_MEMLAYOUT_STRIDE:
		case SGXTQ_MEMLAYOUT_OUT_LINEAR:
		{
			BLDSTLinearInit(&puBLDst->linear,
							psSGXTQDstSurface->i32StrideInBytes,
							psSGXTQDstSurface->ui32Height,
							psSGXTQDstSurface->eFormat,
							&psSGXTQDstSurface->sRect,
							pbyDstBaseLinAddr + psSGXTQDstSurface->uiMemoryOffset,
							psUpstreamObject,
							BL_GetPlanarSurfaceInfo(psSGXTQDstSurface,
							                        &sPlanarInfo));
			psResult = &puBLDst->linear.sObject;
			break;
		}
		case SGXTQ_MEMLAYOUT_2D:
		case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
		{
			BLDSTTwiddledInit(&puBLDst->twiddled,
							  psSGXTQDstSurface->ui32Width,
							  psSGXTQDstSurface->ui32Height,
							  psSGXTQDstSurface->eFormat,
							  &psSGXTQDstSurface->sRect,
							  pbyDstBaseLinAddr + psSGXTQDstSurface->uiMemoryOffset,
							  psUpstreamObject,
							  BL_GetPlanarSurfaceInfo(psSGXTQDstSurface,
							                          &sPlanarInfo),
							  IS_HYBRID_TWIDDLING);
			psResult = &puBLDst->twiddled.sObject;
			break;
		}
		case SGXTQ_MEMLAYOUT_TILED:
		case SGXTQ_MEMLAYOUT_OUT_TILED:
		{
			BLDSTTiledInit(&puBLDst->tiled,
			               psSGXTQDstSurface->i32StrideInBytes,
			               psSGXTQDstSurface->ui32Height,
			               psSGXTQDstSurface->eFormat,
			               &psSGXTQDstSurface->sRect,
			               pbyDstBaseLinAddr + psSGXTQDstSurface->uiMemoryOffset,
			               psUpstreamObject,
			               BL_GetPlanarSurfaceInfo(psSGXTQDstSurface,
			                                       &sPlanarInfo));
			psResult = &puBLDst->tiled.sObject;
			break;
		}
		case SGXTQ_MEMLAYOUT_3D:
		case SGXTQ_MEMLAYOUT_CEM:
		{
			psResult = IMG_NULL;
		}
	}
	return psResult;
}


static PVRSRV_ERROR BlitlibMipgen(SGX_QUEUETRANSFER				* psQueueTransfer,
								SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext)
{
	PVRSRV_ERROR eError;
	IMG_UINT32 ui32Level;
	union
	{
		 BL_OP_SCALE_NEAREST sNearest;
		 BL_OP_SCALE_BILINEAR sBilinear;
	} uScale;

	BL_SRC uBLSrc;
	BL_DST uBLDst;
	BL_OBJECT *psBLSrc=0, *psBLDst=0, *psScale;

	BL_PLANAR_SURFACE_INFO sBLPlanarInfo;
	BL_PLANAR_SURFACE_INFO * psBLPlanarInfo;

	IMG_UINT32 ui32Width, ui32NextMipmapWidth = 1;
	IMG_UINT32 ui32Height, ui32NextMipmapHeight = 1;
	IMG_UINT32 ui32Stride = 0, ui32NextMipmapStride = 0;
	IMG_UINT32 ui32NextMipmapSize = 0;

	IMG_RECT sRect = {0,0,0,0};
	IMG_BYTE		* pbySrc;
	IMG_BYTE		* pbyDst;

	SGXTQ_SURFACE *psTQSurface = &psQueueTransfer->asSources[0];

	IMG_UINT32 ui32BytesPerPixel = gas_BLExternalPixelTable[psTQSurface->eFormat].ui32BytesPerPixel;

	SGXTQ_MIPGENOP *psMipgen = &psQueueTransfer->Details.sMipGen;


	eError = BL_WaitForSyncInfo(psTQContext,
			psTQContext->ahSyncModObjPool[0],
			psTQSurface->psSyncInfo);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	ui32Width = psTQSurface->ui32Width;
	ui32Height = psTQSurface->ui32Height;

	psBLPlanarInfo = BL_GetPlanarSurfaceInfo(psTQSurface,
	                                         &sBLPlanarInfo);

	if (psBLPlanarInfo)
	{
		ui32NextMipmapSize = ui32Width * ui32Height * sBLPlanarInfo.uiPixelBytesPerPlane;
	}
	else
	{
		ui32NextMipmapSize = ui32Width * ui32Height * ui32BytesPerPixel;
	}


	switch (psTQSurface->eMemLayout)
	{
		case SGXTQ_MEMLAYOUT_STRIDE:
		case SGXTQ_MEMLAYOUT_OUT_LINEAR:
		{
			PVR_ASSERT(psTQSurface->i32StrideInBytes > 0);
			ui32Stride = (IMG_UINT32)psTQSurface->i32StrideInBytes;
			psBLSrc = &uBLSrc.linear.sObject;
			psBLDst = &uBLDst.linear.sObject;
			break;
		}
		case SGXTQ_MEMLAYOUT_2D:
		case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
		{
			psBLSrc = &uBLSrc.twiddled.sObject;
			psBLDst = &uBLDst.twiddled.sObject;
			break;
		}
		case SGXTQ_MEMLAYOUT_TILED:
		case SGXTQ_MEMLAYOUT_OUT_TILED:
#if 0
		/* tiled mipmaps don't make much sense */
		{
			PVR_ASSERT(psTQSurface->i32StrideInBytes > 0);
			ui32Stride = (IMG_UINT32)psTQSurface->i32StrideInBytes;
			psBLSrc = &uBLSrc.tiled.sObject;
			psBLDst = &uBLDst.tiled.sObject;
			break;
		}
#endif
		case SGXTQ_MEMLAYOUT_3D:
		case SGXTQ_MEMLAYOUT_CEM:
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}
	}

	pbySrc = (IMG_BYTE*)psTQSurface->psClientMemInfo->pvLinAddr +
			psTQSurface->uiMemoryOffset;

	sRect.x1 = ui32Width;
	sRect.y1 = ui32Height;

	for (ui32Level = 0; ui32Level < psMipgen->ui32Levels; ui32Level++)
	{
		pbyDst = pbySrc + ui32NextMipmapSize;

		/* update dst */
		if (ui32Width > 1)
		{
			ui32NextMipmapWidth = ui32Width / 2;
			ui32NextMipmapStride = ui32Stride / 2;
			ui32NextMipmapSize /= 2;
		}
		if (ui32Height > 1)
		{
			ui32NextMipmapHeight = ui32Height / 2;
			ui32NextMipmapSize /= 2;
		}

		/* init scale op*/
		switch (psQueueTransfer->Details.sBlit.eFilter)
		{
			case SGXTQ_FILTERTYPE_POINT:
			{
				BLOPScaleNearestInit(&uScale.sNearest,
									 &sRect,
									 psBLSrc);
				psScale = &uScale.sNearest.sObject;
				break;
			}
			case SGXTQ_FILTERTYPE_LINEAR:
			{
				BLOPScaleBilinearInit(&uScale.sBilinear,
									  &sRect,
									  psBLSrc);
				psScale = &uScale.sBilinear.sObject;
				break;
			}
			default:
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
		}

		/* init source and destination */
		sRect.x1 = ui32NextMipmapWidth;
		sRect.y1 = ui32NextMipmapHeight;

		switch (psTQSurface->eMemLayout)
		{
			case SGXTQ_MEMLAYOUT_STRIDE:
			case SGXTQ_MEMLAYOUT_OUT_LINEAR:
			{
				BLSRCLinearInit(&uBLSrc.linear,
				                ui32Stride,
				                ui32Height,
				                psTQSurface->eFormat,
				                pbySrc,
				                psBLPlanarInfo);

				BLDSTLinearInit(&uBLDst.linear,
				                ui32NextMipmapStride,
				                ui32NextMipmapHeight,
				                psTQSurface->eFormat,
				                &sRect,
				                pbyDst,
				                psScale,
				                psBLPlanarInfo);
				break;
			}
			case SGXTQ_MEMLAYOUT_2D:
			case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
			{
				BLSRCTwiddledInit(&uBLSrc.twiddled,
				                  ui32Width,
				                  ui32Height,
				                  psTQSurface->eFormat,
				                  pbySrc,
				                  psBLPlanarInfo,
				                  IS_HYBRID_TWIDDLING);

				BLDSTTwiddledInit(&uBLDst.twiddled,
				                  ui32NextMipmapWidth,
				                  ui32NextMipmapHeight,
				                  psTQSurface->eFormat,
				                  &sRect,
				                  pbyDst,
				                  psScale,
				                  psBLPlanarInfo,
				                  IS_HYBRID_TWIDDLING);
				break;
			}
			case SGXTQ_MEMLAYOUT_TILED:
			case SGXTQ_MEMLAYOUT_OUT_TILED:
#if 0
				/* tiled mipmaps don't make much sense */
			{
				BLSRCTiledInit(&uBLSrc.tiled,
				               ui32Stride,
				               ui32Height,
				               psTQSurface->eFormat,
				               pbySrc,
				               psBLPlanarInfo);

				BLDSTTiledInit(&uBLDst.tiled,
				               ui32NextMipmapStride,
				               ui32NextMipmapHeight,
				               psTQSurface->eFormat,
				               &sRect,
				               pbyDst,
				               psScale,
				               psBLPlanarInfo);
				break;
			}
#endif
			case SGXTQ_MEMLAYOUT_3D:
			case SGXTQ_MEMLAYOUT_CEM:
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}
		}

		eError = BL_OBJECT_START(psBLDst);
		if (eError != PVRSRV_OK)
		{
			/* at least finish the op*/
			PVRSRVModifyCompleteSyncOps(psTQContext->psDevData->psConnection,
					psTQContext->ahSyncModObjPool[0]);
			return eError;
		}

		/* update src for next iteration */
		ui32Width = ui32NextMipmapWidth;
		ui32Height = ui32NextMipmapHeight;
		ui32Stride = ui32NextMipmapStride;
		pbySrc = pbyDst;
	}

	return PVRSRVModifyCompleteSyncOps(psTQContext->psDevData->psConnection,
			psTQContext->ahSyncModObjPool[0]);
}

#undef IS_HYBRID_TWIDDLING

/*
 * Does a general blit via the blitlib library.
 */
static PVRSRV_ERROR BlitlibBlit(SGX_QUEUETRANSFER				* psQueueTransfer,
								SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext)
{
	BL_SRC uBLSrc;
	BL_OBJECT *psBLSrc;

	BL_DST uBLDst;
	BL_OBJECT *psBLDst;
	SGXTQ_SURFACE *psTQDstSurface;

	IMG_SIZE_T uiDstSize=0;
	BL_SRC uBLSrcDstCopy;
	BL_OBJECT *psBLSrcDstCopy=0;

	BL_OBJECT *psBLObjectPrevious;

	SGXTQ_BLITOP *psBlitOp;
	IMG_UINT32			i;

	union
	{
		 BL_OP_SCALE_NEAREST sNearest;
		 BL_OP_SCALE_BILINEAR sBilinear;
	} uScale;
	BL_OP_ALPHA_BLEND sBLAlpha;
	BL_OP_ROTATE sBLRotate;
	BL_OP_COLOUR_KEY sBLColourKey;
	BL_OP_FLIP sBLFlip;

	PVRSRV_ERROR		eError = PVRSRV_OK, eError2;
	IMG_BYTE **ppbyBufferCopy = IMG_NULL;


	/* create the source and destination blitlib objects */
	psBLSrc = BL_SRC_From_SGXTQ_SURFACE(&uBLSrc,
										&psQueueTransfer->asSources[0]);
	if (psBLSrc == IMG_NULL)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	eError = BL_WaitForSyncInfo(psTQContext,
			psTQContext->ahSyncModObjPool[0],
			psQueueTransfer->asSources[0].psSyncInfo);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	psTQDstSurface = &psQueueTransfer->asDests[0];

	eError = BL_WaitForSyncInfo(psTQContext,
			psTQContext->ahSyncModObjPool[1],
			psTQDstSurface->psSyncInfo);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	psBlitOp = &psQueueTransfer->Details.sBlit;

	/* if we need to access the source destination (colour key, alpha blending)
	 * it's faster to make a copy in user memory*/
	if (psBlitOp->eColourKey ||
		psBlitOp->eAlpha)
	{
		psBLSrcDstCopy = BL_SRC_From_SGXTQ_SURFACE(&uBLSrcDstCopy,
												psTQDstSurface);
		if (psBLSrcDstCopy == IMG_NULL)
		{
			eError = PVRSRV_ERROR_NOT_SUPPORTED;
			goto Exit;
		}

		/*calculate the size, and point to the target buffer*/
		switch (psQueueTransfer->asDests[0].eMemLayout)
		{
			case SGXTQ_MEMLAYOUT_STRIDE:
			case SGXTQ_MEMLAYOUT_OUT_LINEAR:
			{
				uiDstSize = psTQDstSurface->i32StrideInBytes * psTQDstSurface->ui32Height;
				ppbyBufferCopy = &uBLSrcDstCopy.linear.pbyFBAddr;
				break;
			}
			case SGXTQ_MEMLAYOUT_2D:
			case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
			{
				uiDstSize = psTQDstSurface->ui32Height *
					psTQDstSurface->ui32Width *
					gas_BLExternalPixelTable[psTQDstSurface->eFormat].ui32BytesPerPixel;
				ppbyBufferCopy = &uBLSrcDstCopy.twiddled.pbyFBAddr;
				break;
			}
			case SGXTQ_MEMLAYOUT_TILED:
			case SGXTQ_MEMLAYOUT_OUT_TILED:
			{
				uiDstSize = psTQDstSurface->i32StrideInBytes * psTQDstSurface->ui32Height;
				ppbyBufferCopy = &uBLSrcDstCopy.tiled.pbyFBAddr;
				break;
			}
			case SGXTQ_MEMLAYOUT_3D:
			case SGXTQ_MEMLAYOUT_CEM:
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}
		}
		/*allocate the new buffer and copy the contents of the destination surface*/
		*ppbyBufferCopy = PVRSRVAllocUserModeMem(uiDstSize);
		if (! *ppbyBufferCopy)
		{
			eError = PVRSRV_ERROR_OUT_OF_MEMORY;
			goto Exit;
		}
		PVRSRVMemCopy(*ppbyBufferCopy,
					 (IMG_PBYTE)psTQDstSurface->psClientMemInfo->pvLinAddr + psTQDstSurface->uiMemoryOffset,
					 uiDstSize);
	} /*end destination buffer copy*/


	psBLObjectPrevious = psBLSrc;

	/*SCALE*/
	if (psTQDstSurface->sRect.x0 != psQueueTransfer->asSources[0].sRect.x0 ||
		psTQDstSurface->sRect.x1 != psQueueTransfer->asSources[0].sRect.x1 ||
		psTQDstSurface->sRect.y0 != psQueueTransfer->asSources[0].sRect.y0 ||
		psTQDstSurface->sRect.y1 != psQueueTransfer->asSources[0].sRect.y1)
	{

		switch (psQueueTransfer->Details.sBlit.eFilter)
		{
			case SGXTQ_FILTERTYPE_POINT:
			{
				uScale.sNearest.sUpstreamClipRect = psQueueTransfer->asSources[0].sRect;

				BLOPScaleNearestInit(&uScale.sNearest,
									 &uScale.sNearest.sUpstreamClipRect,
									 psBLObjectPrevious);
				psBLObjectPrevious = &uScale.sNearest.sObject;
				break;
			}
			case SGXTQ_FILTERTYPE_LINEAR:
			{
				uScale.sBilinear.sUpstreamClipRect = psQueueTransfer->asSources[0].sRect;

				BLOPScaleBilinearInit(&uScale.sBilinear,
									  &uScale.sBilinear.sUpstreamClipRect,
									  psBLObjectPrevious);
				psBLObjectPrevious = &uScale.sBilinear.sObject;
				break;
			}
			default:
			{
				eError = PVRSRV_ERROR_NOT_SUPPORTED;
				goto Exit;
			}
		}
	}

	/*FLIP*/
	if (psQueueTransfer->ui32Flags & (SGX_TRANSFER_FLAGS_INVERTX | SGX_TRANSFER_FLAGS_INVERTY))
	{
		IMG_UINT32 ui32Flags;

		ui32Flags = (((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERTX) == 0) ?  0 : BL_OP_FLIP_X)
			| (((psQueueTransfer->ui32Flags & SGX_TRANSFER_FLAGS_INVERTY) == 0) ?  0 : BL_OP_FLIP_Y);

		BLOPFlipInit(&sBLFlip,
					ui32Flags,
					psBLObjectPrevious);
		psBLObjectPrevious = &sBLFlip.sObject;
	}

	/*ALPHA*/
	if (psBlitOp->eAlpha != SGXTQ_ALPHA_NONE)
	{
		switch (psQueueTransfer->Details.sBlit.eAlpha)
		{
			case SGXTQ_ALPHA_GLOBAL:
			{
				BLOPAlphaBlendInit(&sBLAlpha,
								 psBlitOp->byGlobalAlpha/255.0,
								 psBLObjectPrevious,
								 psBLSrcDstCopy);
				psBLObjectPrevious = &sBLAlpha.sObject;
				break;
			}
			default:
			{
				eError = PVRSRV_ERROR_NOT_SUPPORTED;
				goto Exit;
			}
		}
	}

	/*COLOURKEY*/
	if (psBlitOp->eColourKey != SGXTQ_COLOURKEY_NONE)
	{
		BL_OBJECT *psObjectFront;
		BL_OBJECT *psObjectBack;
		switch (psBlitOp->eColourKey)
		{
			case SGXTQ_COLOURKEY_SOURCE:
			{
				psObjectFront = psBLObjectPrevious;
				psObjectBack = psBLSrcDstCopy;
				break;
			}
			case SGXTQ_COLOURKEY_DEST:
			{
				psObjectFront = psBLSrcDstCopy;
				psObjectBack = psBLObjectPrevious;
				break;
			}
			default:
			{
				eError = PVRSRV_ERROR_NOT_SUPPORTED;
				goto Exit;
			}
		}
		BLOPColourKeyInit(&sBLColourKey,
						 (BL_PIXEL*)(&psBlitOp->ui32ColourKey),
						 (BL_PIXEL*)(&psBlitOp->ui32ColourKeyMask),
						 BL_INTERNAL_PX_FMT_ARGB8888,
						 psObjectFront, psObjectBack);

		psBLObjectPrevious = &sBLColourKey.sObject;
	}

	/*ROTATION*/
	if (psBlitOp->eRotation != SGXTQ_ROTATION_NONE)
	{
		BLOPRotateInit(&sBLRotate,
					 psQueueTransfer->Details.sBlit.eRotation,
					 psBLObjectPrevious);
		psBLObjectPrevious = &sBLRotate.sObject;
	}

	psBLDst = BL_DST_From_SGXTQ_SURFACE(&uBLDst,
										psTQDstSurface,
										psBLObjectPrevious);

	if (psBLDst == IMG_NULL)
	{
		eError = PVRSRV_ERROR_NOT_SUPPORTED;
		goto Exit;
	}

	eError = BL_OBJECT_START(psBLDst);

Exit:

	for (i = 0; i < 2; i++)
	{
		eError2 = PVRSRVModifyCompleteSyncOps(psTQContext->psDevData->psConnection,
				psTQContext->ahSyncModObjPool[i]);
		if (eError == PVRSRV_OK)
		{
			eError = eError2;
		}
	}

	if (ppbyBufferCopy && *ppbyBufferCopy)
	{
		PVRSRVFreeUserModeMem(*ppbyBufferCopy);
	}

	return eError;
}

static PVRSRV_ERROR BlitlibFill(SGX_QUEUETRANSFER				* psQueueTransfer,
								SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext)
{
	BL_SRC_SOLID sBLSolid;

	BL_DST uBLDst;
	BL_OBJECT *psBLDst;
	PVRSRV_ERROR	eError, eError2;

	BLSRCSolidInit(&sBLSolid,
				   (BL_PIXEL*)&psQueueTransfer->Details.sFill.ui32Colour,
				   BL_INTERNAL_PX_FMT_ARGB8888,
				   PVRSRV_PIXEL_FORMAT_ARGB8888);

	psBLDst = BL_DST_From_SGXTQ_SURFACE(&uBLDst,
										&psQueueTransfer->asDests[0],
										&sBLSolid.sObject);
	if (!psBLDst)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	eError = BL_WaitForSyncInfo(psTQContext,
			psTQContext->ahSyncModObjPool[0],
			psQueueTransfer->asDests[0].psSyncInfo);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	eError = BL_OBJECT_START(psBLDst);

	eError2= PVRSRVModifyCompleteSyncOps(psTQContext->psDevData->psConnection,
			psTQContext->ahSyncModObjPool[0]);
	if (eError == PVRSRV_OK)
	{
		eError = eError2;
	}
	
	return eError;
}

static PVRSRV_ERROR BlitlibTextureUpload(SGX_QUEUETRANSFER		* psQueueTransfer,
								SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext)
{
	BL_SRC uBLSrc;
	BL_OBJECT *psBLSrc;

	BL_DST uBLDst;
	BL_OBJECT *psBLDst;

	SGXTQ_TEXTURE_UPLOADOP *psUploadOp;
	PVRSRV_ERROR			eError, eError2;

	PVR_DPF((PVR_DBG_MESSAGE, "Using BlitlibTextureUpload..."));

	psUploadOp = &psQueueTransfer->Details.sTextureUpload;

	/* will set everything but the linear address, that is in psUploadOp
	 * I *DON'T* synchronize on the source for uploads. As it's usually user-land
	 * memory only accessible by the client process we might not even have
	 * a syncinfo/meminfo for it.
	 */
	psBLSrc = BL_SRC_From_SGXTQ_SURFACE(&uBLSrc, &psQueueTransfer->asSources[0]);
	if (psBLSrc == IMG_NULL)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	/*now we set the linear address for the src*/
	switch (psQueueTransfer->asSources[0].eMemLayout)
	{
		case SGXTQ_MEMLAYOUT_STRIDE:
		case SGXTQ_MEMLAYOUT_OUT_LINEAR:
		{
			uBLSrc.linear.pbyFBAddr = psUploadOp->pbySrcLinAddr;
			break;
		}
		case SGXTQ_MEMLAYOUT_2D:
		case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
		{
			uBLSrc.twiddled.pbyFBAddr = psUploadOp->pbySrcLinAddr;
			break;
		}
		case SGXTQ_MEMLAYOUT_TILED:
		case SGXTQ_MEMLAYOUT_OUT_TILED:
		{
			uBLSrc.tiled.pbyFBAddr = psUploadOp->pbySrcLinAddr;
		}
		case SGXTQ_MEMLAYOUT_3D:
		case SGXTQ_MEMLAYOUT_CEM:
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}
	}

	psBLDst = BL_DST_From_SGXTQ_SURFACE(&uBLDst,
										&psQueueTransfer->asDests[0],
										psBLSrc);
	if (psBLDst == IMG_NULL)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	eError = BL_WaitForSyncInfo(psTQContext,
			psTQContext->ahSyncModObjPool[0],
			psQueueTransfer->asDests[0].psSyncInfo);
	if (eError != PVRSRV_OK)
	{
		return eError;
}

	eError = BL_OBJECT_START(psBLDst);

	eError2= PVRSRVModifyCompleteSyncOps(psTQContext->psDevData->psConnection,
			psTQContext->ahSyncModObjPool[0]);
	if (eError == PVRSRV_OK)
	{
		eError = eError2;
	}

	return eError;
}

static PVRSRV_ERROR BlitlibClipBlit(SGX_QUEUETRANSFER			* psQueueTransfer,
								SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext)
{
	PVRSRV_ERROR	eError, eError2;
	BL_SRC uBLSrc;
	BL_OBJECT *psBLSrc;

	BL_DST uBLDst;
	BL_OBJECT *psBLDst;

	IMG_UINT32		i;
	IMG_RECT *psRect=0;

	IMG_UINT uiRect;
	SGXTQ_CLIPBLITOP *psClipBlitOp = &psQueueTransfer->Details.sClipBlit;

	if (psQueueTransfer->ui32NumSources != 1
			|| psQueueTransfer->ui32NumDest != 1)
	{
		PVR_DPF((PVR_DBG_ERROR, "BlitlibClipBlit: Number of destinations (%u) or sources (%u) != 1",
			psQueueTransfer->ui32NumDest,
			psQueueTransfer->ui32NumSources));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/*create src and dst blitlib surfaces*/
	psBLSrc = BL_SRC_From_SGXTQ_SURFACE(&uBLSrc, &psQueueTransfer->asSources[0]);
	if (psBLSrc == IMG_NULL)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	eError = BL_WaitForSyncInfo(psTQContext,
			psTQContext->ahSyncModObjPool[0],
			psQueueTransfer->asSources[0].psSyncInfo);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	psBLDst = BL_DST_From_SGXTQ_SURFACE(&uBLDst, &psQueueTransfer->asDests[0], psBLSrc);
	if (psBLDst == IMG_NULL)
	{
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	eError = BL_WaitForSyncInfo(psTQContext,
			psTQContext->ahSyncModObjPool[1],
			psQueueTransfer->asDests[0].psSyncInfo);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	/*point to the dst clip rect*/
	switch (psQueueTransfer->asDests[0].eMemLayout)
	{
		case SGXTQ_MEMLAYOUT_STRIDE:
		case SGXTQ_MEMLAYOUT_OUT_LINEAR:
		{
			psRect = &uBLDst.linear.sClipRect;
			break;
		}
		case SGXTQ_MEMLAYOUT_2D:
		case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
		{
			psRect = &uBLDst.twiddled.sClipRect;
			break;
		}
		case SGXTQ_MEMLAYOUT_TILED:
		case SGXTQ_MEMLAYOUT_OUT_TILED:
		{
			psRect = &uBLDst.tiled.sClipRect;
		}
		case SGXTQ_MEMLAYOUT_3D:
		case SGXTQ_MEMLAYOUT_CEM:
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}
	}

	for (uiRect = 0; uiRect < psClipBlitOp->ui32RectNum; uiRect++)
	{
		/*update the rect and start the pipe*/
		*psRect = psClipBlitOp->psRects[uiRect];
		eError = BL_OBJECT_START(psBLDst);
		if (eError != PVRSRV_OK)
		{
			break;
		}
	}

	for (i = 0; i < 2; i++)
	{
		eError2 = PVRSRVModifyCompleteSyncOps(psTQContext->psDevData->psConnection,
				psTQContext->ahSyncModObjPool[0]);
		if (eError == PVRSRV_OK)
		{
			eError = eError2;
		}
	}

	return eError;
}

/**
 * This is the entry point of the blitlib.
 */
static PVRSRV_ERROR DispatchBlitlib(SGX_QUEUETRANSFER				* psQueueTransfer,
									SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext)
{
	switch (psQueueTransfer->eType)
	{
#if defined(SGX_FEATURE_2D_HARDWARE)
		case SGXTQ_2DHWBLIT:
#endif
		case SGXTQ_BLIT:
			return BlitlibBlit(psQueueTransfer, psTQContext);
		case SGXTQ_FILL:
			return BlitlibFill(psQueueTransfer, psTQContext);
		case SGXTQ_TEXTURE_UPLOAD:
			return BlitlibTextureUpload(psQueueTransfer, psTQContext);
		case SGXTQ_CLIP_BLIT:
			return BlitlibClipBlit(psQueueTransfer, psTQContext);
		case SGXTQ_MIPGEN:
			return BlitlibMipgen(psQueueTransfer, psTQContext);
		case SGXTQ_COLOURLUT_BLIT: /*TODO (last)*/
			/*unsupported cases*/
		case SGXTQ_BUFFERBLT:
		case SGXTQ_CUSTOM:
		case SGXTQ_FULL_CUSTOM:
		case SGXTQ_CUSTOMSHADER_BLIT:
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "Blitlib Unsupported operation type"));
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}
	}
}
#endif


#if !defined(FIX_HW_BRN_27298)
static PVRSRV_ERROR Dispatch3dBlit(IMG_HANDLE hTransferContext,
								   SGX_QUEUETRANSFER *psQueueTransfer)
{
	PVRSRV_ERROR eError;
	PVRSRV_ERROR eAccumlateError = PVRSRV_OK;
	IMG_UINT32 ui32CurPass = 0; /* The current pass through of the system */
	IMG_UINT32 ui32NumPassesRequired = 0; /* Set by prepare functions */
	IMG_UINT32 ui32StatusCount;
	SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext;
	SGXTQ_PREP_INTERNAL sInternalData;

	/* if separate Prepare/Submit then get the next command from the submits array,
	 * otherwise we need only 1 from the stack, as after preparing 1 we kick it straight
	 * away
	 */
	SGXTQ_SUBMITS *psSubmits;
	SGXMKIF_TRANSFERCMD *psTransferCmd = IMG_NULL; /* PRQA S 3197 3 */ /* to keep compiler happy */
	SGXMKIF_TRANSFERCMD_SHARED *psShared = IMG_NULL;
	PVRSRV_TRANSFER_SGX_KICK *psKick = IMG_NULL;

#if defined(SGXTQ_PREP_SUBMIT_SEPERATE)
	psSubmits = (SGXTQ_SUBMITS *) psQueueTransfer->phTransferSubmit;
#else /* SGXTQ_PREP_SUBMIT_SEPERATE*/
	SGXTQ_SUBMITS sSubmits;
	SGX_SUBMITTRANSFER sSubmitTransfer;
	psSubmits = &sSubmits;
	sSubmitTransfer.hTransferSubmit = (IMG_HANDLE) psSubmits;
	sSubmitTransfer.ui32NumStatusValues = 0;	/* PRQA S 3198 */ /* initialise anyway */
#if defined(PDUMP)
	sSubmitTransfer.bPDumpContinuous = psQueueTransfer->bPDumpContinuous;
#endif /* PDUMP*/
#endif /* SGXTQ_PREP_SUBMIT_SEPERATE*/

	PVRSRVMemSet(psSubmits, 0, sizeof (SGXTQ_SUBMITS));

	psTQContext = (SGXTQ_CLIENT_TRANSFER_CONTEXT *) hTransferContext;

	PVRSRVLockMutex(psTQContext->hMutex);

	do /* Keep going as long as the prepare function wants us to */
	{

#if defined(SGX_FEATURE_2D_HARDWARE)
		if (psQueueTransfer->eType != SGXTQ_2DHWBLIT)
#endif
		{
			if (psSubmits->ui32NumSubmits >= SGXTQ_MAX_COMMANDS)
			{
				PVR_DPF((PVR_DBG_ERROR, "There is no space in the submit queue to kick this transfer"));
				eError = PVRSRV_ERROR_CMD_NOT_PROCESSED;
				goto Exit;
			}

			psTransferCmd = &(psSubmits->asTransferCmd[psSubmits->ui32NumSubmits]);
			psShared = &psTransferCmd->sShared;
			psKick = &(psSubmits->asKick[psSubmits->ui32NumSubmits]);

			/* Clear the Cmd first, to stop any fragments reappearing. */
			PVRSRVMemSet(psTransferCmd, 0, sizeof (SGXMKIF_TRANSFERCMD));
			PVRSRVMemSet(psKick, 0, sizeof (PVRSRV_TRANSFER_SGX_KICK));
			/* General stuff required for everything */
			/* copy the flags from the client*/
			if (psQueueTransfer->ui32Flags & SGX_KICKTRANSFER_FLAGS_TATQ_SYNC)
				psTransferCmd->ui32Flags |= SGXMKIF_TQFLAGS_TATQ_SYNC;
			if (psQueueTransfer->ui32Flags & SGX_KICKTRANSFER_FLAGS_3DTQ_SYNC)
				psTransferCmd->ui32Flags |= SGXMKIF_TQFLAGS_3DTQ_SYNC;

			psTransferCmd->ui32FenceID = psTQContext->ui32FenceID;

#if defined(EUR_CR_DOUBLE_PIXEL_PARTITIONS)
			psTransferCmd->sHWRegs.ui32DoublePixelPartitions = 0;
#endif

			psTransferCmd->ui32NumPixelPartitions = SGX_DEFAULT_MAX_NUM_PIXEL_PARTITIONS << EUR_CR_DMS_CTRL_MAX_NUM_PIXEL_PARTITIONS_SHIFT;

#if defined(SGXTQ_PREP_SUBMIT_SEPERATE)
			if(psQueueTransfer->ui32NumStatusValues != 0)
			{
					PVR_DPF((PVR_DBG_ERROR, "SGXQueueTransfer: status values != 0"));
					PVR_DBG_BREAK;
					return PVRSRV_ERROR_INVALID_PARAMS;
			}
#else
			sSubmitTransfer.ui32NumStatusValues = 0;
#endif /* SGXTQ_PREP_SUBMIT_SEPERATE*/

			/* initialize the TQ CBs */
			SGXTQ_BeginCB(psTQContext->psPDSPrimFragSingleSNB);
#if defined(SGXTQ_SUBTILE_TWIDDLING)
			SGXTQ_BeginCB(psTQContext->psUSEEOTSubTwiddledSNB);
#endif
			SGXTQ_BeginCB(psTQContext->psPDSCodeCB);
			SGXTQ_BeginCB(psTQContext->psUSECodeCB);
			SGXTQ_BeginCB(psTQContext->psStreamCB);

			if (psTQContext->psStagingBuffer != IMG_NULL)
				SGXTQ_BeginCB(psTQContext->psStagingBuffer);
		}

		/*
		 * The blit dispatcher
		 */
		switch (psQueueTransfer->eType)
		{
#if defined(SGX_FEATURE_2D_HARDWARE)
			case SGXTQ_2DHWBLIT:
			{
				SGXMKIF_2DCMD_SUBMIT *ps2DCmd;

				if (psSubmits->ui32Num2DSubmits >= SGXTQ_MAX_COMMANDS)
				{
					PVR_DBG_BREAK;
				}

				ps2DCmd = &(psSubmits->as2DSubmit[psSubmits->ui32Num2DSubmits]);

				eError = Prepare2DCoreBlit(psTQContext,
										   psQueueTransfer,
										   ps2DCmd);
				if (eError != PVRSRV_OK)
				{
					goto Exit;
				}
				break;
			}
#endif /* SGX_FEATURE_2D_HARDWARE*/
			case SGXTQ_BLIT:
			{
				if (psQueueTransfer->Details.sBlit.byCustomRop3 == 0xAA) // dest = dest
				{
					eError = PVRSRV_OK;
					goto Exit; // nop so just return
				}
				eError = PrepareNormalBlit(psTQContext,
										   psQueueTransfer,
										   ui32CurPass,
										   &sInternalData,
										   psTransferCmd,
										   psKick,
										   &ui32NumPassesRequired);
				break;
			}
			case SGXTQ_MIPGEN:
			{
				eError = PrepareMipGen(psTQContext,
									   psQueueTransfer,
									   ui32CurPass,
									   &sInternalData,
									   psTransferCmd,
									   psKick,
									   &ui32NumPassesRequired);
				break;
			}
			case SGXTQ_FILL:
			{
				eError = PrepareFillOp(psTQContext,
									   psQueueTransfer,
									   ui32CurPass,
									   &sInternalData,
									   psTransferCmd,
									   psKick,
									   &ui32NumPassesRequired);
				break;
			}
			case SGXTQ_BUFFERBLT:
			{
				eError = PrepareBufferBlt(psTQContext,
										  psQueueTransfer,
										  ui32CurPass,
										  &sInternalData,
										  psTransferCmd,
										  psKick,
										  &ui32NumPassesRequired);
				break;
			}
			case SGXTQ_CUSTOM:
			{
				eError = PrepareCustomOp(psTQContext,
										 psQueueTransfer,
										 ui32CurPass,
										 &sInternalData,
										 psTransferCmd,
										 psKick,
										 &ui32NumPassesRequired);
				break;
			}
			case SGXTQ_FULL_CUSTOM:
			{
				eError = PrepareFullCustomOp(psTQContext,
											 psQueueTransfer,
											 ui32CurPass,
											 &sInternalData,
											 psTransferCmd,
											 psKick,
											 &ui32NumPassesRequired);
				break;
			}
			case SGXTQ_TEXTURE_UPLOAD:
			{
				eError = PrepareTextureUpload(psTQContext,
											  psQueueTransfer,
											  ui32CurPass,
											  &sInternalData,
											  psTransferCmd,
											  psKick,
											  &ui32NumPassesRequired);
				break;
			}
			case SGXTQ_CLIP_BLIT:
			{
				eError = PrepareClipBlit(psTQContext,
										 psQueueTransfer,
										 ui32CurPass,
										 &sInternalData,
										 psTransferCmd,
										 psKick,
										 &ui32NumPassesRequired);
				break;
			}
			case SGXTQ_CUSTOMSHADER_BLIT:
			{
				eError = PrepareCustomShaderBlit(psTQContext,
											psQueueTransfer,
											ui32CurPass,
											&sInternalData,
											psTransferCmd,
											psKick,
											&ui32NumPassesRequired);
				break;
			}
			case SGXTQ_COLOURLUT_BLIT:
			{
				eError = PrepareColourLUTBlit(psTQContext,
											psQueueTransfer,
											ui32CurPass,
											&sInternalData,
											psTransferCmd,
											psKick,
											&ui32NumPassesRequired);
				break;
			}
			case SGXTQ_VIDEO_BLIT:
			{
				eError = PrepareVideoBlit(psTQContext,
											psQueueTransfer,
											ui32CurPass,
											&sInternalData,
											psTransferCmd,
											psKick,
											&ui32NumPassesRequired);
				break;
			}
			case SGXTQ_CLEAR_TYPE_BLEND:
			{
				eError = PrepareClearTypeBlendOp(psTQContext,
												 	psQueueTransfer,
													ui32CurPass,
													&sInternalData,
													psTransferCmd,
													psKick,
													&ui32NumPassesRequired);
				break;
			}
			case SGXTQ_TATLAS_BLIT:
			{
				eError = PrepareTAtlasBlit(psTQContext,
						psQueueTransfer,
						ui32CurPass,
						& sInternalData,
						psTransferCmd,
						psKick,
						& ui32NumPassesRequired);
				break;
			}
			case SGXTQ_ARGB2NV12_BLIT:
			{
				eError = PrepareARGB2NV12Blit(psTQContext,
						psQueueTransfer,
						ui32CurPass,
						& sInternalData,
						psTransferCmd,
						psKick,
						& ui32NumPassesRequired);
				break;
			}

			default:
			{
				/* Operation type not recognised */
				PVR_DPF((PVR_DBG_ERROR, "SGXQueueTransfer: Unknown blit type %d", psQueueTransfer->eType));
				eError = PVRSRV_ERROR_INVALID_PARAMS;
				goto Exit;
			}
		}

		if (eError != PVRSRV_OK)
		{
			/* drop all allocations happened in this pass*/
			SGXTQ_BeginCB(psTQContext->psPDSPrimFragSingleSNB);
#if defined(SGXTQ_SUBTILE_TWIDDLING)
			SGXTQ_BeginCB(psTQContext->psUSEEOTSubTwiddledSNB);
#endif
			SGXTQ_BeginCB(psTQContext->psPDSCodeCB);
			SGXTQ_BeginCB(psTQContext->psUSECodeCB);
			SGXTQ_BeginCB(psTQContext->psStreamCB);
			if (psTQContext->psStagingBuffer != IMG_NULL)
				SGXTQ_BeginCB(psTQContext->psStagingBuffer);

			if (ui32CurPass > 0)
			{
				/* We have the pending already updated, this one sends down a dummy
				 * transfer to update the completes / status values.
				 */
				eAccumlateError = eError;
				ui32NumPassesRequired = ui32CurPass + 1;
				/* PRQA S 0505 1 */ /* psKick setup for all but 2D blit, which exit on error */
				psTransferCmd->ui32Flags |= SGXMKIF_TQFLAGS_DUMMYTRANSFER;
			}
			else
			{
				goto Exit;
			}
		}
		else
		{
#if defined (PDUMP)
#define FALSE_IFNDEF_PDUMP(x) (x)
#else
#define FALSE_IFNDEF_PDUMP(x) IMG_FALSE
#endif

			/* Flush the TQ CBs
			 *
			 * if we bailed out before this point the allocations will be dropped.
			 *
			 * if we miss the submit after this the allocations become part of the
			 * next kick because the fence id won't be updated. This means that they
			 * will be freed when the next successful blit comes back that has been
			 * sent down after this.
			 */
			SGXTQ_FlushCB(psTQContext->psPDSCodeCB,
					FALSE_IFNDEF_PDUMP(psQueueTransfer->bPDumpContinuous));

			SGXTQ_FlushCB(psTQContext->psUSECodeCB,
					FALSE_IFNDEF_PDUMP(psQueueTransfer->bPDumpContinuous));

			SGXTQ_FlushCB(psTQContext->psStreamCB,
					FALSE_IFNDEF_PDUMP(psQueueTransfer->bPDumpContinuous));

			SGXTQ_FlushCB(psTQContext->psPDSPrimFragSingleSNB,
					FALSE_IFNDEF_PDUMP(psQueueTransfer->bPDumpContinuous));

#if defined(SGXTQ_SUBTILE_TWIDDLING)
			SGXTQ_FlushCB(psTQContext->psUSEEOTSubTwiddledSNB,
					FALSE_IFNDEF_PDUMP(psQueueTransfer->bPDumpContinuous));
#endif

			if (psTQContext->psStagingBuffer)
			{
				SGXTQ_FlushCB(psTQContext->psStagingBuffer,
						FALSE_IFNDEF_PDUMP(psQueueTransfer->bPDumpContinuous));
			}

#undef FALSE_IFNDEF_PDUMP
		}

		/* Advance the FenceID */

		psSubmits->aui32FenceIDs[psSubmits->ui32NumSubmits] = ++psTQContext->ui32FenceID;

		/*
		 * copy the status values from the client - on the last sub blit only
		 */
		if (ui32CurPass == (ui32NumPassesRequired - 1)) /* PRQA S 3382 */ /*last blit */
		{
#if defined(SGX_FEATURE_2D_HARDWARE)
			if (psQueueTransfer->eType != SGXTQ_2DHWBLIT)
#endif
			{
				for (ui32StatusCount = 0; ui32StatusCount < psQueueTransfer->ui32NumStatusValues; ui32StatusCount++)
				{
					if (psShared->ui32NumStatusVals < SGXTQ_MAX_STATUS) /* PRQA S 505 */ /* not NULL */
					{
						IMG_UINT32 ui32UpdAddr = psQueueTransfer->asMemUpdates[ui32StatusCount].ui32UpdateAddr;
						IMG_UINT32 ui32UpdVal = psQueueTransfer->asMemUpdates[ui32StatusCount].ui32UpdateVal;

						psShared->sCtlStatusInfo[psShared->ui32NumStatusVals].sStatusDevAddr.uiAddr = ui32UpdAddr;
						psShared->sCtlStatusInfo[psShared->ui32NumStatusVals].ui32StatusValue = ui32UpdVal;
						psShared->ui32NumStatusVals++;

					}
					else
					{
						PVR_DPF((PVR_DBG_ERROR, "SGXSubmitTransfer: attempted to write beyond the bounds of psShared->sCtlStatusInfo array"));
						PVR_DBG_BREAK;
						goto Exit;
					}
				}
			}
		}

		/*
		 * Iterators.
		 */
#if defined(SGX_FEATURE_2D_HARDWARE)
		if (psQueueTransfer->eType == SGXTQ_2DHWBLIT)
		{
			psSubmits->ui32Num2DSubmits++;
		}
		else
#endif /* SGX_FEATURE_2D_HARDWARE*/
		{
			psSubmits->ui32NumSubmits++;
		}

		/*
		 * Sync object flags
		 */
		if (ui32NumPassesRequired > 1)
		{
			if (ui32CurPass != 0)
			{
				psTransferCmd->ui32Flags |= SGXMKIF_TQFLAGS_KEEPPENDING; /* PRQA S 505 */ /* not NULL */
				psKick->ui32Flags |= SGXMKIF_TQFLAGS_KEEPPENDING; /* PRQA S 505 */ /* not NULL */
			}
			if (ui32CurPass != ui32NumPassesRequired - 1)
			{
				psTransferCmd->ui32Flags |= SGXMKIF_TQFLAGS_NOSYNCUPDATE;
				psKick->ui32Flags |= SGXMKIF_TQFLAGS_NOSYNCUPDATE;
			}
		}

#if ! defined(SGXTQ_PREP_SUBMIT_SEPERATE)
		eError = SGXSubmitTransfer(hTransferContext, &sSubmitTransfer);
		if (PVRSRV_OK != eError)
		{
			goto Exit;
		}
		/*
		 * If not the separate path then the next submit can go to the same slot.
		 * That way we won't hit the limit with SGXTQ_MAX_COMMANDS, no matter
		 * how many submits this QueueTransfer requires.
		 */
		psSubmits->ui32NumSubmits = 0;
#if defined(SGX_FEATURE_2D_HARDWARE)
		psSubmits->ui32Num2DSubmits = 0;
#endif /* SGX_FEATURE_2D_HARDWARE*/
#endif /* SGXTQ_PREP_SUBMIT_SEPERATE*/

		ui32CurPass++;
	} while (ui32CurPass < ui32NumPassesRequired);

	eError = eAccumlateError;
Exit:

	PVRSRVUnlockMutex(psTQContext->hMutex);

	return eError;
}

#endif /* FIX_HW_BRN_27298 */


/*****************************************************************************
 * Function Name		:	SGXQueueTransfer
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
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV SGXQueueTransfer(IMG_HANDLE hTransferContext,
										   SGX_QUEUETRANSFER *psQueueTransfer)
{
	PVRSRV_ERROR eError = PVRSRV_ERROR_NOT_SUPPORTED;

#if defined(PDUMP) && defined(SGXTQ_DEBUG_PDUMP_TQINPUT)
	const PVRSRV_CONNECTION* psConnection;
	IMG_UINT32 ui32PDumpFlags = (psQueueTransfer->bPDumpContinuous ? PDUMP_FLAGS_CONTINUOUS : 0);
#endif

	/* Validate parameters */
	if ((psQueueTransfer == IMG_NULL) || (hTransferContext == IMG_NULL))
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXQueueTransfer: invalid parameters"));
		PVR_DBG_BREAK;
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if ((psQueueTransfer->ui32Flags & SGX_TRANSFER_DISPATCH_MASK) ==
			SGX_TRANSFER_DISPATCH_MASK)
	{
		return PVRSRV_ERROR_INVALID_FLAGS;
	}

#if defined(PDUMP) && defined(SGXTQ_DEBUG_PDUMP_TQINPUT)
	{
		SGXTQ_CLIENT_TRANSFER_CONTEXT *psTQContext = (SGXTQ_CLIENT_TRANSFER_CONTEXT *)hTransferContext;
		psConnection = psTQContext->psDevData->psConnection;
		SGXTQ_PDumpTQInput(psConnection, psQueueTransfer, psTQContext->ui32FenceID);
	}
#endif

#if defined (SGX_FEATURE_PTLA)
	if (!(psQueueTransfer->ui32Flags & SGX_TRANSFER_DISPATCH_DISABLE_PTLA))
	{
		eError = SGXQueue2DTransfer(hTransferContext, psQueueTransfer);
		if (eError == PVRSRV_OK)
		{
			return PVRSRV_OK;
		}
	}
#endif

#if !defined (FIX_HW_BRN_27298)
	if ((psQueueTransfer->ui32Flags & SGX_TRANSFER_DISPATCH_DISABLE_3D) == 0)
	{
		eError = Dispatch3dBlit(hTransferContext, psQueueTransfer);
		if (eError == PVRSRV_OK)
		{
			return PVRSRV_OK;
		}
	}
#endif

#if defined (BLITLIB)
	if (!(psQueueTransfer->ui32Flags & SGX_TRANSFER_DISPATCH_DISABLE_SW))
	{
#if defined(PDUMP) && defined(SGXTQ_DEBUG_PDUMP_TQINPUT)
		PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "Blit has been handled by host SW");
#endif
		eError = DispatchBlitlib(psQueueTransfer, (SGXTQ_CLIENT_TRANSFER_CONTEXT *)hTransferContext);
		if (eError == PVRSRV_OK)
		{
			return PVRSRV_OK;
		}
	}
#endif

#if defined(PDUMP) && defined(SGXTQ_DEBUG_PDUMP_TQINPUT)
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "Blit was rejected by the host (%d)", (IMG_UINT32)eError);
#endif

	if (eError == PVRSRV_ERROR_NOT_SUPPORTED)
	{
		PVR_DPF((PVR_DBG_WARNING, "SGXQueueTransfer: all paths failed in TQ, returning error (this is not a bug)"));
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXQueueTransfer: all paths failed"));
	}

	return eError;
}

#endif

/******************************************************************************
 End of file (sgxtransfer_queue.c)
 ******************************************************************************/
