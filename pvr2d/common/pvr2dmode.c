/*!****************************************************************************
@File           pvr2dmode.c

@Title          PVR2D library display mode functions

@Author         Imagination Technologies

@Date           14/12/2006

@Copyright      Copyright 2006-2007 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or otherwise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform       Generic

@Description    PVR2D library display mode function code

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: pvr2dmode.c $
******************************************************************************/

#include "img_defs.h"
#include "services.h"
#include "pvr2d.h"
#include "pvr2dint.h"

/******************************************************************************
 @Function	ConvertPVRSRVPixelFormatToPVR2D

 @Input    ePixelFormat    : PVRSRV pixel format

 @Output   pePVR2DFormat   : PVR2D pixel format

 @Return   PVR2DERROR      : Error code

 @Description              : Converts PVRSRV_PIXEL_FORMAT to PVR2DFORMAT
******************************************************************************/
static PVR2DERROR ConvertPVRSRVPixelFormatToPVR2D(PVRSRV_PIXEL_FORMAT ePixelFormat,
												  PVR2DFORMAT *pePVR2DFormat)
{
	PVR2DFORMAT ePVR2DFormat;

	switch (ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		{
			ePVR2DFormat = PVR2D_RGB565;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			ePVR2DFormat = PVR2D_ARGB4444;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_RGB888:
		{
			ePVR2DFormat = PVR2D_RGB888;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		{
			ePVR2DFormat = PVR2D_ARGB8888;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			ePVR2DFormat = PVR2D_ARGB1555;
			break;
		}
		default:
		{
			PVR2D_DPF((PVR_DBG_ERROR, "ConvertPVRSRVPixelFormatToPVR2D: Invalid format %d",ePixelFormat));
			return PVR2DERROR_GENERIC_ERROR;
		}
	}

	*pePVR2DFormat = ePVR2DFormat;
	return PVR2D_OK;
}

#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
/******************************************************************************
 @Function	PVR2DGetFrameBuffer

 @Input    hContext        : pvr2d context handle

 @Input    nHeap           : Heap to get, only PVR2D_FB_PRIMARY_SURFACE is valid

 @Output   ppsMemInfo      : pvr2d meminfo for system buffer

 @Return   error           : error code

 @Description              : Gets system surface
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DGetFrameBuffer (PVR2DCONTEXTHANDLE hContext,
                               PVR2D_INT nHeap,
                               PVR2DMEMINFO **ppsMemInfo)
{
	PVR2DCONTEXT *psContext;

	if(!hContext || !ppsMemInfo || (nHeap != PVR2D_FB_PRIMARY_SURFACE))
		return PVR2DERROR_INVALID_PARAMETER;

	psContext=(PVR2DCONTEXT *)hContext;
	if (psContext->hDisplayClassDevice == 0)
		return PVR2DERROR_DEVICE_NOT_PRESENT;

	*ppsMemInfo = &psContext->sPrimaryBuffer;

	return PVR2D_OK;
}
#endif

/******************************************************************************
 @Function	PVR2DGetDeviceInfo

 @Input    hContext        : pvr2d context handle

 @Output   pDisplayInfo    : Mode and capabilities of display device

 @Return   error           : error code

 @Description              : get display device mode and capabilities
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DGetDeviceInfo (PVR2DCONTEXTHANDLE hContext,
                               PVR2DDISPLAYINFO *pDisplayInfo)
{
	PVR2DCONTEXT *psContext;
	DISPLAY_SURF_ATTRIBUTES *psPrimary;
	PVR2DERROR eError;

	if (!hContext)
		return PVR2DERROR_INVALID_PARAMETER;

	if (!pDisplayInfo)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psContext = (PVR2DCONTEXT *)hContext;
	if (psContext->hDisplayClassDevice == 0)
		return PVR2DERROR_DEVICE_NOT_PRESENT;

	psPrimary = &psContext->sPrimary;

	pDisplayInfo->ulWidth  = (PVR2D_ULONG)psPrimary->sDims.ui32Width;
	pDisplayInfo->ulHeight = (PVR2D_ULONG)psPrimary->sDims.ui32Height;
	pDisplayInfo->lStride  = (PVR2D_LONG)psPrimary->sDims.ui32ByteStride;

	/* Get from display class info */
	pDisplayInfo->ulMinFlipInterval = psContext->sDisplayInfo.ui32MinSwapInterval;
	pDisplayInfo->ulMaxFlipInterval = psContext->sDisplayInfo.ui32MaxSwapInterval;
	pDisplayInfo->ulMaxFlipChains   = psContext->sDisplayInfo.ui32MaxSwapChains;
	pDisplayInfo->ulMaxBuffersInChain = psContext->sDisplayInfo.ui32MaxSwapChainBuffers;

	eError = ConvertPVRSRVPixelFormatToPVR2D(psPrimary->pixelformat,
											 &pDisplayInfo->eFormat);

	if (eError != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DGetDeviceInfo: Invalid primary pixel format"));
	}

	return eError;
}



/******************************************************************************
 @Function	PVR2DGetMiscDisplayInfo

 @Input    hContext        : pvr2d context handle

 @Output   pMiscDisplayInfo : Display device info

 @Return   error           : error code

 @Description              : get display device mode and capabilities
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DGetMiscDisplayInfo (PVR2DCONTEXTHANDLE hContext,
									PVR2DMISCDISPLAYINFO *pMiscDisplayInfo )
{
	PVR2DCONTEXT *psContext;
	PVR2D_INT n;

	if (!hContext)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if (!pMiscDisplayInfo)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psContext = (PVR2DCONTEXT *)hContext;

	if (psContext->hDisplayClassDevice == 0)
	{
		return PVR2DERROR_DEVICE_NOT_PRESENT;
	}

	pMiscDisplayInfo->ulPhysicalWidthmm = psContext->sDisplayInfo.ui32PhysicalWidthmm;
	pMiscDisplayInfo->ulPhysicalHeightmm = psContext->sDisplayInfo.ui32PhysicalHeightmm;

	for (n=0; n<10; n++)
	{
		pMiscDisplayInfo->ulUnused[n] = 0; // reserved for future expansion
	}

	return PVR2D_OK;
}


/******************************************************************************
 @Function	PVR2DGetScreenMode

 @Input    hContext        : pvr2d context handle

 @Output    pFormat         : primary surface format

 @Output    plWidth         : primary surface width

 @Output    plHeight        : primary surface height

 @Output    plStride        : primary surface stride

 @Output    piRefreshRate   : primary surface refresh rate

 @Return   error           : error code

 @Description              : get primary surface mode
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DGetScreenMode (PVR2DCONTEXTHANDLE hContext,
                               PVR2DFORMAT *pFormat,
							   PVR2D_LONG *plWidth,
							   PVR2D_LONG *plHeight,
							   PVR2D_LONG *plStride,
							   PVR2D_INT *piRefreshRate)
{
	PVR2DCONTEXT *psContext;
	DISPLAY_SURF_ATTRIBUTES *psPrimary;
	PVR2DERROR eError;

	if (!hContext)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if (!plWidth || !plHeight || !plStride || !piRefreshRate || !pFormat)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psContext = (PVR2DCONTEXT *)hContext;
	if (psContext->hDisplayClassDevice == 0)
		return PVR2DERROR_DEVICE_NOT_PRESENT;

	psPrimary = &psContext->sPrimary;

	*plWidth  = (PVR2D_LONG)psPrimary->sDims.ui32Width;
	*plHeight = (PVR2D_LONG)psPrimary->sDims.ui32Height;
	*plStride = (PVR2D_LONG)psPrimary->sDims.ui32ByteStride;
	*piRefreshRate = 0;

	eError = ConvertPVRSRVPixelFormatToPVR2D(psPrimary->pixelformat,
											 pFormat);
	if (eError != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DGetScreenMode: Invalid primary pixel format"));
	}

	return eError;
}

/******************************************************************************
 End of file (pvr2dmode.c)
******************************************************************************/
