/*!****************************************************************************
@File           pvr2dblt3d.c

@Title          PVR2D library 3D blit

@Author         Imagination Technologies

@Date           14/12/2006

@Copyright      Copyright 2006-2009 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or otherwise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform       Generic

@Description    PVR2D library 3D blit for scaling and custom use code

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: pvr2dblt3d.c $
******************************************************************************/

/* PRQA S 5013++ */ /* disable basic types warning */
#include <memory.h>
#include "img_defs.h"
#include "services.h"
#include "pvr2d.h"
#include "pvr2dint.h"
#include "sgxapi.h"

#define INCLUDE_PVR2D_FORMAT_BITS_PER_PIXEL_TABLE
#define INCLUDE_PVRSRV_FORMAT_BITS_PER_PIXEL_TABLE
#include "pvr2dutils.h"

#define PDUMP_ENTIRE_SURFACE

static IMG_BOOL IsYUV(IMG_UINT32 ui32PvrSrvFormat);
static PVR2DERROR Do3DVideoBlt(PVR2DCONTEXT *psContext, PPVR2D_3DBLT_EXT pBlt3D, SGX_QUEUETRANSFER *pBlitInfo);
static PVR2DERROR DoNV12Blt(PVR2DCONTEXT *psContext, PPVR2D_3DBLT_EXT pBlt3D, SGX_QUEUETRANSFER *pBlitInfo);

#ifdef PDUMP
static PVR2DERROR PdumpSurface(PVR2DCONTEXT *psContext, PVR2D_SURFACE *pSurf, PVR2DRECT *prcRegion, IMG_CHAR* pPDumpComment);
#endif

/*****************************************************************************
 @Function	PVR2DLoadUseCode

 @Input		hContext : PVR2D context handle

 @Input		pUseCode : USSE instruction bytes to load into device memory

 @Input		UseCodeSize : number of USSE instruction bytes to load into device memory

 @Output	pUseCodeHandle : handle of loaded USSE code, for use as input to PVR2DBlt3D()

 @Return	error : PVR2D error code

 @Description : loads USSE code to be executed for each pixel during a 3DBlt
******************************************************************************/
// PVR2DUFreeUseCode must be called to free up the device memory.
PVR2D_EXPORT
PVR2DERROR PVR2DLoadUseCode (const PVR2DCONTEXTHANDLE hContext, const PVR2D_UCHAR	*pUseCode,
									const PVR2D_ULONG UseCodeSize, PVR2D_HANDLE *pUseCodeHandle)
{
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PPVRSRV_CLIENT_MEM_INFO psMemInfo;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DLoadUseCode - Invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!pUseCode || !UseCodeSize || !pUseCodeHandle)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DLoadUseCode - Invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	// Alloc the device memory from the correct heap
    if (PVRSRVAllocDeviceMem((IMG_HANDLE)&psContext->sDevData,
                   psContext->hUSEMemHeap,
                   PVRSRV_MEM_READ | PVRSRV_MEM_WRITE,
                   UseCodeSize,
                   EURASIA_PDS_DOUTU_PHASE_START_ALIGN,
                   &psMemInfo) != PVRSRV_OK)
    {
        return PVR2DERROR_MEMORY_UNAVAILABLE;
    }
    
    // Copy over the USE code
    PVRSRVMemCopy(psMemInfo->pvLinAddr, pUseCode, UseCodeSize);

#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2DLoadUseCode", IMG_FALSE);

	PVRSRVPDumpMem(	psContext->psServices,
					psMemInfo->pvLinAddr,	/* IMG_PVOID */
					psMemInfo,				/* PVRSRV_CLIENT_MEM_INFO */
					0,						/* offset */
					UseCodeSize,			/* bytes */
					0);
#endif
    
    // Return the structure
	*pUseCodeHandle = (PVR2D_HANDLE)psMemInfo;
	return PVR2D_OK;
}


/*****************************************************************************
 @Function	PVR2DFreeUseCode

 @Input		hContext : PVR2D context handle

 @Input		hUseCodeHandle : handle of loaded USSE code to free (unload)

 @Return	error : PVR2D error code

 @Description : unloads USSE code
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DFreeUseCode (const PVR2DCONTEXTHANDLE hContext, const PVR2D_HANDLE hUseCodeHandle)
{
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PPVRSRV_CLIENT_MEM_INFO psMemInfo = (PPVRSRV_CLIENT_MEM_INFO)hUseCodeHandle;
	
	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DFreeUseCode - Invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!hUseCodeHandle)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DFreeUseCode - Invalid handle"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	PVRSRVFreeDeviceMem(&psContext->sDevData, psMemInfo);
	return PVR2D_OK;
}


/*****************************************************************************
 @Function	PVR2DBlt3D

 @Input		hContext : PVR2D context handle

 @Input		pBlt3D : Structure containing blt parameters

 @Return	error : PVR2D error code

 @Description : Does blt using the 3D Core, supports scaling and custom USSE code
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DBlt3D (const PVR2DCONTEXTHANDLE hContext, const PPVR2D_3DBLT pBlt3D)
{
#if defined(TRANSFER_QUEUE)
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PPVRSRV_CLIENT_MEM_INFO pUseBltCode;
	SGX_QUEUETRANSFER sBlitInfo;
	const PVRSRV_CLIENT_MEM_INFO *pSrcMemInfo;
	const PVRSRV_CLIENT_MEM_INFO *pDstMemInfo;
	PVRSRV_ERROR eResult;
	PVR2DERROR ePVR2DResult;
	
	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBlt3D - Invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!pBlt3D || !pBlt3D->sDst.pSurfMemInfo)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	/* Validate the TQ context (creates one if necessary) */
	ePVR2DResult = ValidateTransferContext(psContext);
	if (ePVR2DResult != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR,"PVR2DBlt3D: ValidateTransferContext failed\n"));
		return ePVR2DResult;
	}

	// Init SGX_QUEUETRANSFER
	PVRSRVMemSet(&sBlitInfo,0,sizeof(sBlitInfo));
	
	// Set up the destination surface
	pDstMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBlt3D->sDst.pSurfMemInfo->hPrivateData;
	sBlitInfo.asDests[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
	sBlitInfo.asDests[0].i32StrideInBytes = pBlt3D->sDst.Stride;
	sBlitInfo.asDests[0].psSyncInfo = pDstMemInfo->psClientSyncInfo;

	// Dest surface address
	SGX_TQSURFACE_SET_ADDR(sBlitInfo.asDests[0], pDstMemInfo, 0);

	sBlitInfo.asDests[0].ui32Height = pBlt3D->sDst.SurfHeight;
	sBlitInfo.asDests[0].ui32Width = pBlt3D->sDst.SurfWidth;
	sBlitInfo.asDests[0].eFormat = GetPvrSrvPixelFormat(pBlt3D->sDst.Format);

	SGX_QUEUETRANSFER_NUM_DST_RECTS(sBlitInfo) = 1;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).x0 = pBlt3D->rcDest.left;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).x1 = pBlt3D->rcDest.right;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).y0 = pBlt3D->rcDest.top;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).y1 = pBlt3D->rcDest.bottom;
	
	// Route standard types via the SGXTQ_BLIT path for the time being
	sBlitInfo.eType = SGXTQ_BLIT;
	sBlitInfo.asSources[0].eFormat = GetPvrSrvPixelFormat(pBlt3D->sSrc.Format);
	pSrcMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBlt3D->sSrc.pSurfMemInfo->hPrivateData;
	sBlitInfo.asSources[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
	sBlitInfo.asSources[0].i32StrideInBytes = pBlt3D->sSrc.Stride;
	sBlitInfo.asSources[0].psSyncInfo = pSrcMemInfo->psClientSyncInfo;
	
	// Source surface address
	SGX_TQSURFACE_SET_ADDR(sBlitInfo.asSources[0], pSrcMemInfo, 0);
	
	sBlitInfo.asSources[0].ui32Height = pBlt3D->sSrc.SurfHeight;
	sBlitInfo.asSources[0].ui32Width = pBlt3D->sSrc.SurfWidth;

	SGX_QUEUETRANSFER_NUM_SRC_RECTS(sBlitInfo) = 1;
	SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).x0 = pBlt3D->rcSource.left;
	SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).x1 = pBlt3D->rcSource.right;
	SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).y0 = pBlt3D->rcSource.top;
	SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).y1 = pBlt3D->rcSource.bottom;
	
	sBlitInfo.Details.sBlit.eFilter = SGXTQ_FILTERTYPE_POINT; // SGXTQ_FILTERTYPE_LINEAR   SGXTQ_FILTERTYPE_ANISOTROPIC
	
	// USE code for this blt
	pUseBltCode = (PPVRSRV_CLIENT_MEM_INFO)pBlt3D->hUseCode;
	sBlitInfo.Details.sBlit.sUSEExecAddr.uiAddr = (pUseBltCode) ? pUseBltCode->sDevVAddr.uiAddr : 0;
	sBlitInfo.Details.sBlit.UseParams[0] = pBlt3D->UseParams[0];
	sBlitInfo.Details.sBlit.UseParams[1] = pBlt3D->UseParams[1];
	
#ifndef BLITLIB
		sBlitInfo.ui32NumSources = SGX_QUEUETRANSFER_NUM_SRC_RECTS(sBlitInfo);
		sBlitInfo.ui32NumDest = SGX_QUEUETRANSFER_NUM_DST_RECTS(sBlitInfo);
#endif

#ifdef PDUMP
#ifdef PDUMP_ENTIRE_SURFACE
	// Capture the source and destination surfaces
	PdumpSurface(psContext, &pBlt3D->sSrc, 0, "PVR2DBlt3D: Source pixels");
	PdumpSurface(psContext, &pBlt3D->sDst, 0,   "PVR2DBlt3D  Dest pixels (pre-blt)");
#else
	// Capture the blt source and destination pixels
	PdumpSurface(psContext, &pBlt3D->sSrc, &pBlt3D->rcSource, "PVR2DBlt3D: Source pixels");
	PdumpSurface(psContext, &pBlt3D->sDst, &pBlt3D->rcDest,   "PVR2DBlt3D  Dest pixels (pre-blt)");
#endif//#ifdef PDUMP_ENTIRE_SURFACE
#endif//#ifdef PDUMP
		
#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2DBlt3D: calling SGXQueueTransfer", IMG_FALSE);
#endif//#ifdef PDUMP

	// Don't route this to PTLA or software blt library.
	sBlitInfo.ui32Flags |= SGX_TRANSFER_DISPATCH_DISABLE_PTLA | SGX_TRANSFER_DISPATCH_DISABLE_SW;

	// Transfer queue blt
	eResult = SGXQueueTransfer(psContext->hTransferContext, &sBlitInfo);

#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2DBlt3D: back from SGXQueueTransfer", IMG_FALSE);
#endif
	
	if (eResult != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBlt3D: SGXQueueTransfer failed 0x%08X", eResult));
		return PVR2DERROR_GENERIC_ERROR;
	}
	
	return PVR2D_OK;

#else//#if defined(TRANSFER_QUEUE)

	if(!hContext || !pBlt3D)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	return PVR2DERROR_GENERIC_ERROR;

#endif//#if defined(TRANSFER_QUEUE)
}


#ifdef PDUMP

// Capture a region of a 2D surface for the PDUMP file
static PVR2DERROR PdumpSurface(PVR2DCONTEXT* psContext, PVR2D_SURFACE* pSurf, PVR2DRECT* prcRegion, IMG_CHAR* pPDumpComment)
{
	const PVR2DMEMINFO *pMemInfo = pSurf->pSurfMemInfo;
	const IMG_UINT32 ui32BitsPerPixel = GetBpp(pSurf->Format);
	PVRSRV_ERROR srvErr;
	IMG_BOOL bIsCapturing;
	IMG_UINT32 ui32Delta;
	IMG_UINT32 ui32ByteCount;

	srvErr = PVRSRVPDumpIsCapturing(psContext->psServices, &bIsCapturing);

	if ((srvErr!=PVRSRV_OK) || !bIsCapturing)
	{
		return PVR2DERROR_GENERIC_ERROR;
	}

	ui32Delta = pSurf->SurfOffset;

	PVR_ASSERT(pSurf->Stride >= 0);

	// Wait for writes to finish before capturing the output
	PVR2DQueryBlitsComplete((PVR2DCONTEXTHANDLE)psContext, (const PVR2DMEMINFO*)pMemInfo, 1);
	
	// If a sub-region has been defined then capture it else capture the entire surface
	if (((pSurf->Format & PVR2D_FORMAT_PVRSRV) != 0) && IsYUV(pSurf->Format)) // support YUV
	{
		// special case - capture all YUV surface memory
		ui32ByteCount = pMemInfo->ui32MemSize - ui32Delta;
	}
	else if (prcRegion) // capture sub-region
	{
		// Get the offset to the required pixels required for the pdump
		ui32Delta += ((IMG_UINT32)(prcRegion->top * pSurf->Stride)) + (((IMG_UINT32)prcRegion->left * ui32BitsPerPixel)>>3);

		// Calc no of bytes in the blt region
		ui32ByteCount = (IMG_UINT32)((prcRegion->bottom - prcRegion->top) * pSurf->Stride) - (((IMG_UINT32)prcRegion->left * ui32BitsPerPixel)>>3);
	}
	else // capture surface
	{
		// No sub-region defined so capture entire surface
		ui32ByteCount = pSurf->SurfHeight * (IMG_UINT32)pSurf->Stride;
	}

	PVR_ASSERT((ui32ByteCount + ui32Delta) <= pMemInfo->ui32MemSize);

	if ((ui32ByteCount + ui32Delta) > pMemInfo->ui32MemSize)
	{
		// prevent going out of bounds
		return PVR2DERROR_GENERIC_ERROR;
	}

	/* 2DO : add pdump support for PVR2DBlt3D with negative stride */
	if (pPDumpComment)
	{
		srvErr = PVRSRVPDumpComment(psContext->psServices, pPDumpComment, IMG_FALSE);

		if (srvErr != PVRSRV_OK)
		{
			return PVR2DERROR_GENERIC_ERROR;
		}
	}
	
	srvErr = PVRSRVPDumpMem(psContext->psServices,
						pMemInfo->pBase,									/* lin */
						(PVRSRV_CLIENT_MEM_INFO *)pMemInfo->hPrivateData,	/* PVRSRV_CLIENT_MEM_INFO */
					 	ui32Delta,											/* offset */
					 	ui32ByteCount,										/* size */
					 	0);

	if (srvErr != PVRSRV_OK)
	{
		return PVR2DERROR_GENERIC_ERROR;
	}

	return PVR2D_OK;

}//PdumpInputSurfaces

#endif//#ifdef PDUMP



/*****************************************************************************
 @Function	PVR2DBlt3DExt

 @Input		hContext : PVR2D context handle

 @Input		pBlt3D : Structure containing blt parameters

 @Return	error : PVR2D error code

 @Description : Does blt using the 3D Core, supports scaling and custom USSE code
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DBlt3DExt (const PVR2DCONTEXTHANDLE hContext, const PPVR2D_3DBLT_EXT pBlt3D)
{
#if defined(TRANSFER_QUEUE)
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PPVRSRV_CLIENT_MEM_INFO pUseBltCode;
	SGX_QUEUETRANSFER sBlitInfo;
	const PVRSRV_CLIENT_MEM_INFO *pSrcMemInfo;
	const PVRSRV_CLIENT_MEM_INFO *pDstMemInfo;
	PVRSRV_CLIENT_MEM_INFO *pSrc2MemInfo;
	PVRSRV_ERROR eResult;
	PVR2D_INT no_of_inputs=0;
	PVR2DERROR ePVR2DResult;
	
	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBlt3DExt - Invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!pBlt3D || !pBlt3D->sDst.pSurfMemInfo)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	/* Validate the TQ context (creates one if necessary) */
	ePVR2DResult = ValidateTransferContext(psContext);
	if (ePVR2DResult != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR,"PVR2DBlt3DExt: ValidateTransferContext failed\n"));
		return ePVR2DResult;
	}

	// Init SGX_QUEUETRANSFER
	PVRSRVMemSet(&sBlitInfo,0,sizeof(sBlitInfo));
	
	// Set up the destination surface
	pDstMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBlt3D->sDst.pSurfMemInfo->hPrivateData;
	sBlitInfo.asDests[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
	sBlitInfo.asDests[0].i32StrideInBytes = pBlt3D->sDst.Stride;
	sBlitInfo.asDests[0].psSyncInfo = pDstMemInfo->psClientSyncInfo;

	// Dest surface address
	SGX_TQSURFACE_SET_ADDR(sBlitInfo.asDests[0], pDstMemInfo, 0);

	// Add planeOffsets[0] to get to Y plane of destination
	sBlitInfo.asDests[0].sDevVAddr.uiAddr += pDstMemInfo->planeOffsets[0];

	sBlitInfo.asDests[0].ui32Height = pBlt3D->sDst.SurfHeight;
	sBlitInfo.asDests[0].ui32Width = pBlt3D->sDst.SurfWidth;
	sBlitInfo.asDests[0].eFormat = GetPvrSrvPixelFormat(pBlt3D->sDst.Format);

	SGX_QUEUETRANSFER_NUM_DST_RECTS(sBlitInfo) = 1;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).x0 = pBlt3D->rcDest.left;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).x1 = pBlt3D->rcDest.right;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).y0 = pBlt3D->rcDest.top;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).y1 = pBlt3D->rcDest.bottom;


	// Detect YUV source and call DoVideoBlt()
	if (pBlt3D->sSrc.pSurfMemInfo)
	{
		sBlitInfo.asSources[0].eFormat = GetPvrSrvPixelFormat(pBlt3D->sSrc.Format);

		if (IsYUV(sBlitInfo.asSources[0].eFormat))
		{
			return Do3DVideoBlt(psContext, pBlt3D, &sBlitInfo);
		}
	}


	// Detect NV12 destination and call DoNV12Blt()
	sBlitInfo.asDests[0].eFormat = GetPvrSrvPixelFormat(pBlt3D->sDst.Format);
	if (sBlitInfo.asDests[0].eFormat == PVRSRV_PIXEL_FORMAT_NV12)
	{
		return DoNV12Blt(psContext, pBlt3D, &sBlitInfo);
	}
	
	if (!pBlt3D->pSrc2 && pBlt3D->sSrc.pSurfMemInfo && !pBlt3D->bDisableDestInput)
	{
		// Route standard types via the SGXTQ_BLIT path for the time being
		sBlitInfo.eType = SGXTQ_BLIT;
		sBlitInfo.asSources[0].eFormat = GetPvrSrvPixelFormat(pBlt3D->sSrc.Format);
		pSrcMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBlt3D->sSrc.pSurfMemInfo->hPrivateData;
		sBlitInfo.asSources[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
		sBlitInfo.asSources[0].i32StrideInBytes = pBlt3D->sSrc.Stride;
		sBlitInfo.asSources[0].psSyncInfo = pSrcMemInfo->psClientSyncInfo;

		// Source surface address
		SGX_TQSURFACE_SET_ADDR(sBlitInfo.asSources[0], pSrcMemInfo, 0);

		sBlitInfo.asSources[0].ui32Height = pBlt3D->sSrc.SurfHeight;
		sBlitInfo.asSources[0].ui32Width = pBlt3D->sSrc.SurfWidth;

		SGX_QUEUETRANSFER_NUM_SRC_RECTS(sBlitInfo) = 1;
		SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).x0 = pBlt3D->rcSource.left;
		SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).x1 = pBlt3D->rcSource.right;
		SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).y0 = pBlt3D->rcSource.top;
		SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).y1 = pBlt3D->rcSource.bottom;
		
		sBlitInfo.Details.sBlit.eFilter = SGXTQ_FILTERTYPE_POINT; // SGXTQ_FILTERTYPE_LINEAR   SGXTQ_FILTERTYPE_ANISOTROPIC
		
		// USE code for this blt
		pUseBltCode = (PPVRSRV_CLIENT_MEM_INFO)pBlt3D->hUseCode;
		sBlitInfo.Details.sBlit.sUSEExecAddr.uiAddr = (pUseBltCode) ? pUseBltCode->sDevVAddr.uiAddr : 0;
		sBlitInfo.Details.sBlit.uiNumTemporaryRegisters = pBlt3D->uiNumTemporaryRegisters;
		sBlitInfo.Details.sBlit.UseParams[0] = pBlt3D->UseParams[0];
		sBlitInfo.Details.sBlit.UseParams[1] = pBlt3D->UseParams[1];
		
#ifdef PDUMP
#ifdef PDUMP_ENTIRE_SURFACE
		// Capture the source and destination surfaces
		PdumpSurface(psContext, &pBlt3D->sSrc, 0, "PVR2DBlt3D: Source pixels");
		PdumpSurface(psContext, &pBlt3D->sDst, 0,   "PVR2DBlt3D  Dest pixels (pre-blt)");
#else
		// Capture the blt source and destination pixels
		PdumpSurface(psContext, &pBlt3D->sSrc, &pBlt3D->rcSource, "PVR2DBlt3D: Source pixels");
		PdumpSurface(psContext, &pBlt3D->sDst, &pBlt3D->rcDest,   "PVR2DBlt3D  Dest pixels (pre-blt)");
#endif//#ifdef PDUMP_ENTIRE_SURFACE
#endif//#ifdef PDUMP
		
	}
	else
	{
		// Route non-standard types through SGXTQ_CUSTOMSHADER_BLIT path
	
		// Set up custom shader blt type
		sBlitInfo.eType = SGXTQ_CUSTOMSHADER_BLIT;
		pUseBltCode = (PPVRSRV_CLIENT_MEM_INFO)pBlt3D->hUseCode;
		sBlitInfo.Details.sCustomShader.sUSEExecAddr.uiAddr = (pUseBltCode)?pUseBltCode->sDevVAddr.uiAddr:0;
		sBlitInfo.Details.sCustomShader.ui32NumSAs = 2;
		sBlitInfo.Details.sCustomShader.UseParams[0] = pBlt3D->UseParams[0];
		sBlitInfo.Details.sCustomShader.UseParams[1] = pBlt3D->UseParams[1];
		sBlitInfo.Details.sCustomShader.ui32NumTempRegs = pBlt3D->uiNumTemporaryRegisters;
		
		// Source 1 is the first input (if needed)
		if (pBlt3D->sSrc.pSurfMemInfo)
		{
			pSrcMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBlt3D->sSrc.pSurfMemInfo->hPrivateData;
			sBlitInfo.asSources[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
			sBlitInfo.asSources[0].i32StrideInBytes = pBlt3D->sSrc.Stride;
			sBlitInfo.asSources[0].psSyncInfo = pSrcMemInfo->psClientSyncInfo;

			// Source surface address
			SGX_TQSURFACE_SET_ADDR(sBlitInfo.asSources[0], pSrcMemInfo, 0);

			sBlitInfo.asSources[0].ui32Height = pBlt3D->sSrc.SurfHeight;
			sBlitInfo.asSources[0].ui32Width = pBlt3D->sSrc.SurfWidth;
			sBlitInfo.asSources[0].eFormat = GetPvrSrvPixelFormat(pBlt3D->sSrc.Format);
			sBlitInfo.Details.sCustomShader.ui32NumPAs++;

			SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).x0 = pBlt3D->rcSource.left;
			SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).x1 = pBlt3D->rcSource.right;
			SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).y0 = pBlt3D->rcSource.top;
			SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).y1 = pBlt3D->rcSource.bottom;
			SGX_QUEUETRANSFER_NUM_SRC_RECTS(sBlitInfo)++;
			no_of_inputs=1;
			
#ifdef PDUMP
#ifdef PDUMP_ENTIRE_SURFACE
			// Capture the source surface
			PdumpSurface(psContext, &pBlt3D->sSrc, 0, "PVR2DBlt3D: Source pixels");
#else
			// Capture the source rectangle
			PdumpSurface(psContext, &pBlt3D->sSrc, &pBlt3D->rcSource, "PVR2DBlt3D: Source pixels");
#endif//#ifdef PDUMP_ENTIRE_SURFACE
#endif//#ifdef PDUMP
		}
		
		// Source 2 is the next input (if needed)
		if (pBlt3D->pSrc2)
		{
			pSrc2MemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBlt3D->pSrc2->pSurfMemInfo->hPrivateData;
			sBlitInfo.asSources[no_of_inputs].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
			sBlitInfo.asSources[no_of_inputs].i32StrideInBytes = pBlt3D->pSrc2->Stride;
			
			// Source surface address
			SGX_TQSURFACE_SET_ADDR(sBlitInfo.asSources[no_of_inputs], pSrc2MemInfo, 0);

			sBlitInfo.asSources[no_of_inputs].ui32Height = pBlt3D->pSrc2->SurfHeight;
			sBlitInfo.asSources[no_of_inputs].ui32Width = pBlt3D->pSrc2->SurfWidth;
			sBlitInfo.asSources[no_of_inputs].eFormat = GetPvrSrvPixelFormat(pBlt3D->pSrc2->Format);
			sBlitInfo.Details.sCustomShader.ui32NumPAs++;
			
			if (pBlt3D->prcSource2)
			{
				SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).x0 = pBlt3D->prcSource2->left;
				SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).x1 = pBlt3D->prcSource2->right;
				SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).y0 = pBlt3D->prcSource2->top;
				SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).y1 = pBlt3D->prcSource2->bottom;

			}
			else
			{
				SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).x0 = pBlt3D->rcSource.left;
				SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).x1 = pBlt3D->rcSource.right;
				SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).y0 = pBlt3D->rcSource.top;
				SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).y1 = pBlt3D->rcSource.bottom;

			}
			SGX_QUEUETRANSFER_NUM_SRC_RECTS(sBlitInfo)++;

			no_of_inputs++;
			
#ifdef PDUMP
			// Capture the source2 surface
			PdumpSurface(psContext, pBlt3D->pSrc2, 0, "PVR2DBlt3D: Source2 pixels");
#endif		
		}

		// Destination is the next input (if needed)
		if (!pBlt3D->bDisableDestInput)
		{
			sBlitInfo.asSources[no_of_inputs].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
			sBlitInfo.asSources[no_of_inputs].i32StrideInBytes = pBlt3D->sDst.Stride;
			sBlitInfo.asSources[no_of_inputs].psSyncInfo = pDstMemInfo->psClientSyncInfo;

			SGX_TQSURFACE_SET_ADDR(sBlitInfo.asSources[no_of_inputs], pDstMemInfo, 0);

			sBlitInfo.asSources[no_of_inputs].ui32Height = pBlt3D->sDst.SurfHeight;
			sBlitInfo.asSources[no_of_inputs].ui32Width = pBlt3D->sDst.SurfWidth;
			sBlitInfo.asSources[no_of_inputs].eFormat = GetPvrSrvPixelFormat(pBlt3D->sDst.Format);
			sBlitInfo.Details.sCustomShader.ui32NumPAs++;

			SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).x0 = pBlt3D->rcDest.left;
			SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).x1 = pBlt3D->rcDest.right;
			SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).y0 = pBlt3D->rcDest.top;
			SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, no_of_inputs).y1 = pBlt3D->rcDest.bottom;
			SGX_QUEUETRANSFER_NUM_SRC_RECTS(sBlitInfo)++;

#ifdef PDUMP
#ifdef PDUMP_ENTIRE_SURFACE
			// Capture the dest surface
			PdumpSurface(psContext, &pBlt3D->sDst, 0, "PVR2DBlt3D: Dest pixels");
#else
			// Capture the dest rectangle
			PdumpSurface(psContext, &pBlt3D->sDst, &pBlt3D->rcDest, "PVR2DBlt3D: Dest pixels");
#endif//#ifdef PDUMP_ENTIRE_SURFACE
#endif//#ifdef PDUMP
		}
	}

#ifndef BLITLIB
		sBlitInfo.ui32NumSources = SGX_QUEUETRANSFER_NUM_SRC_RECTS(sBlitInfo);
		sBlitInfo.ui32NumDest = SGX_QUEUETRANSFER_NUM_DST_RECTS(sBlitInfo);
#endif

	// Don't route this to PTLA or software blt library.
	sBlitInfo.ui32Flags |= SGX_TRANSFER_DISPATCH_DISABLE_PTLA | SGX_TRANSFER_DISPATCH_DISABLE_SW;

#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2DBlt3D: calling SGXQueueTransfer", IMG_FALSE);
#endif//#ifdef PDUMP

	// Transfer queue blt
	eResult = SGXQueueTransfer(psContext->hTransferContext, &sBlitInfo);

#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2DBlt3D: back from SGXQueueTransfer", IMG_FALSE);
#endif
	
	if (eResult != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBlt3D: SGXQueueTransfer failed 0x%08X", eResult));
		return PVR2DERROR_GENERIC_ERROR;
	}
	
	return PVR2D_OK;

#else//#if defined(TRANSFER_QUEUE)

	if(!hContext || !pBlt3D)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	return PVR2DERROR_GENERIC_ERROR;

#endif//#if defined(TRANSFER_QUEUE)
}


// Get  bits per pixel
IMG_INTERNAL IMG_UINT32 GetBpp(const PVR2DFORMAT Format)
{
	IMG_UINT32 ui32Format = (IMG_UINT32)Format & PVR2D_FORMAT_MASK;

	if (Format & PVR2D_FORMAT_PVRSRV)
	{
		if (ui32Format < PVRSRV_PIXEL_FORMAT_MAX)
		{
			return aPvrsrvFormatBitsPerPixel[ui32Format];
		}
		else
		{
			return 0;
		}
	}

	if (ui32Format < PVR2D_FORMAT_BITS_PER_PIXEL_MAX)
	{
		return aPvr2DFormatBitsPerPixel[ui32Format];
	}

	return 0;
}

static IMG_BOOL IsYUV(IMG_UINT32 ui32PvrSrvFormat)
{
	switch (ui32PvrSrvFormat & PVR2D_FORMAT_MASK)
	{
		// single plane
		case PVRSRV_PIXEL_FORMAT_YUYV:
		case PVRSRV_PIXEL_FORMAT_UYVY:
		case PVRSRV_PIXEL_FORMAT_YVYU:
		case PVRSRV_PIXEL_FORMAT_VYUY:
		// two planes
		case PVRSRV_PIXEL_FORMAT_NV12:
		// three planes
		case PVRSRV_PIXEL_FORMAT_YV12:
			return IMG_TRUE;
	}
	return IMG_FALSE;
}


static PVR2DERROR Do3DVideoBlt(PVR2DCONTEXT *psContext,
							   PPVR2D_3DBLT_EXT pBlt3D,
							   SGX_QUEUETRANSFER *pBlitInfo)
{
	const PVRSRV_CLIENT_MEM_INFO *pSrcMemInfo;
	PVRSRV_ERROR eResult;
	PVR2DERROR ePVR2DResult;

	if (pBlt3D->pSrc2 // one source and one dest is the only YUV blt type that we support
		|| !pBlt3D->sSrc.pSurfMemInfo
		|| !pBlt3D->bDisableDestInput) // dest must not be an input - YUV alpha blend not supported
	{
		return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
	}

	/* Validate the TQ context (creates one if necessary) */
	ePVR2DResult = ValidateTransferContext(psContext);
	if (ePVR2DResult != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR,"Do3DVideoBlt: ValidateTransferContext failed\n"));
		return ePVR2DResult;
	}

	pSrcMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBlt3D->sSrc.pSurfMemInfo->hPrivateData;
	
	pBlitInfo->ui32NumSources = 1;
	pBlitInfo->ui32NumDest = 1;
	pBlitInfo->eType = SGXTQ_VIDEO_BLIT;
	pBlitInfo->asSources[0].eFormat = GetPvrSrvPixelFormat(pBlt3D->sSrc.Format);
	pBlitInfo->asSources[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
	pBlitInfo->asSources[0].i32StrideInBytes = pBlt3D->sSrc.Stride;
	pBlitInfo->asSources[0].psSyncInfo = pSrcMemInfo->psClientSyncInfo;
	SGX_TQSURFACE_SET_ADDR(pBlitInfo->asSources[0], pSrcMemInfo, 0);
	pBlitInfo->asSources[0].ui32Height = pBlt3D->sSrc.SurfHeight;
	pBlitInfo->asSources[0].ui32Width = pBlt3D->sSrc.SurfWidth;
	SGX_QUEUETRANSFER_NUM_SRC_RECTS((*pBlitInfo)) = 1;
	SGX_QUEUETRANSFER_SRC_RECT((*pBlitInfo), 0).x0 = pBlt3D->rcSource.left;
	SGX_QUEUETRANSFER_SRC_RECT((*pBlitInfo), 0).x1 = pBlt3D->rcSource.right;
	SGX_QUEUETRANSFER_SRC_RECT((*pBlitInfo), 0).y0 = pBlt3D->rcSource.top;
	SGX_QUEUETRANSFER_SRC_RECT((*pBlitInfo), 0).y1 = pBlt3D->rcSource.bottom;
	
	pBlitInfo->Details.sVPBlit.bCoeffsGiven = IMG_FALSE; // use the default set of coefficients

	
#ifdef PDUMP
#ifdef PDUMP_ENTIRE_SURFACE
		// Capture the source and destination surfaces
		PdumpSurface(psContext, &pBlt3D->sSrc, 0, "PVR2DBlt3D: Source pixels");
		PdumpSurface(psContext, &pBlt3D->sDst, 0,   "PVR2DBlt3D  Dest pixels (pre-blt)");
#else
		// Capture the blt source and destination pixels
		PdumpSurface(psContext, &pBlt3D->sSrc, &pBlt3D->rcSource, "PVR2DBlt3D: Source pixels");
		PdumpSurface(psContext, &pBlt3D->sDst, &pBlt3D->rcDest,   "PVR2DBlt3D  Dest pixels (pre-blt)");
#endif//#ifdef PDUMP_ENTIRE_SURFACE
#endif//#ifdef PDUMP
	
	// Don't route this to PTLA or software blt library.
	pBlitInfo->ui32Flags |= SGX_TRANSFER_DISPATCH_DISABLE_PTLA | SGX_TRANSFER_DISPATCH_DISABLE_SW;
	
#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2DBlt3D: calling SGXQueueTransfer", IMG_FALSE);
#endif//#ifdef PDUMP
	
	// Transfer queue blt
	eResult = SGXQueueTransfer(psContext->hTransferContext, pBlitInfo);
	
#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2DBlt3D: back from SGXQueueTransfer", IMG_FALSE);
#endif
	
	if (eResult != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBlt3D: SGXQueueTransfer failed 0x%08X", eResult));
		return PVR2DERROR_GENERIC_ERROR;
	}
	
	return PVR2D_OK;
}

static PVR2DERROR DoNV12Blt(PVR2DCONTEXT *psContext,
							   PPVR2D_3DBLT_EXT pBlt3D,
							   SGX_QUEUETRANSFER *pBlitInfo)
{
	const PVRSRV_CLIENT_MEM_INFO *pSrcMemInfo;
	PVRSRV_ERROR eResult;
	PVR2DERROR ePVR2DResult;

	if (pBlt3D->pSrc2 // one source and one dest is the only YUV blt type that we support
		|| !pBlt3D->sSrc.pSurfMemInfo
		|| !pBlt3D->bDisableDestInput) // dest must not be an input - YUV alpha blend not supported
	{
		return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
	}

	/* Validate the TQ context (creates one if necessary) */
	ePVR2DResult = ValidateTransferContext(psContext);
	if (ePVR2DResult != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR,"DoNV12Blt: ValidateTransferContext failed\n"));
		return ePVR2DResult;
	}

	pSrcMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBlt3D->sSrc.pSurfMemInfo->hPrivateData;
	
	pBlitInfo->ui32NumSources = 1;
	pBlitInfo->ui32NumDest = 1;
	pBlitInfo->eType = SGXTQ_ARGB2NV12_BLIT;

	pBlitInfo->asSources[0].eFormat = GetPvrSrvPixelFormat(pBlt3D->sSrc.Format);
	pBlitInfo->asSources[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
	pBlitInfo->asSources[0].i32StrideInBytes = pBlt3D->sSrc.Stride;
	pBlitInfo->asSources[0].psSyncInfo = pSrcMemInfo->psClientSyncInfo;
	SGX_TQSURFACE_SET_ADDR(pBlitInfo->asSources[0], pSrcMemInfo, 0);
	pBlitInfo->asSources[0].ui32Height = pBlt3D->sSrc.SurfHeight;
	pBlitInfo->asSources[0].ui32Width = pBlt3D->sSrc.SurfWidth;

	SGX_QUEUETRANSFER_NUM_SRC_RECTS((*pBlitInfo)) = 0;
	SGX_QUEUETRANSFER_NUM_DST_RECTS((*pBlitInfo)) = 0;

	// get the address of the UV plane
	if (pBlt3D->pDstExt->uChromaPlane1 == 0)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}
	pBlitInfo->Details.sARGB2NV12.sUVDevVAddr.uiAddr = pSrcMemInfo->sDevVAddr.uiAddr + pBlt3D->pDstExt->uChromaPlane1;

#ifdef PDUMP
#ifdef PDUMP_ENTIRE_SURFACE
	// Capture the source and destination surfaces
	PdumpSurface(psContext, &pBlt3D->sSrc, 0, "PVR2DBlt3D: Source pixels");
	PdumpSurface(psContext, &pBlt3D->sDst, 0,   "PVR2DBlt3D  Dest pixels (pre-blt)");
#else
	// Capture the blt source and destination pixels
	PdumpSurface(psContext, &pBlt3D->sSrc, &pBlt3D->rcSource, "PVR2DBlt3D: Source pixels");
	PdumpSurface(psContext, &pBlt3D->sDst, &pBlt3D->rcDest,   "PVR2DBlt3D  Dest pixels (pre-blt)");
#endif//#ifdef PDUMP_ENTIRE_SURFACE
#endif//#ifdef PDUMP
	
	// Don't route this to PTLA or software blt library.
	pBlitInfo->ui32Flags |= SGX_TRANSFER_DISPATCH_DISABLE_PTLA | SGX_TRANSFER_DISPATCH_DISABLE_SW;
	
#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2DBlt3D: calling SGXQueueTransfer", IMG_FALSE);
#endif//#ifdef PDUMP
	
	// Transfer queue blt
	eResult = SGXQueueTransfer(psContext->hTransferContext, pBlitInfo);
	
#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2DBlt3D: back from SGXQueueTransfer", IMG_FALSE);
#endif
	
	if (eResult != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBlt3D: SGXQueueTransfer failed 0x%08X", eResult));
		return PVR2DERROR_GENERIC_ERROR;
	}
	
	return PVR2D_OK;
}
