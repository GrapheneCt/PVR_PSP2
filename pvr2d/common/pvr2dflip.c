/*!****************************************************************************
@File           pvr2dflip.c

@Title          PVR2D library flip processing

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

@Description    PVR2D library flip code

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
 $Log: pvr2dflip.c $
******************************************************************************/

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "img_defs.h"
#include "services.h"
#include "pvr2d.h"
#include "pvr2dint.h"

static const PVRSRV_PIXEL_FORMAT aConvertPVR2DPixelFormatToPVRSRV[] =
{
	PVRSRV_PIXEL_FORMAT_UNKNOWN,	/*PVR2D_1BPP*/
	PVRSRV_PIXEL_FORMAT_RGB565,		/*PVR2D_RGB565*/
	PVRSRV_PIXEL_FORMAT_ARGB4444,	/*PVR2D_ARGB4444*/
	PVRSRV_PIXEL_FORMAT_RGB888,		/*PVR2D_RGB888*/
	PVRSRV_PIXEL_FORMAT_ARGB8888,	/*PVR2D_ARGB8888*/
	PVRSRV_PIXEL_FORMAT_ARGB1555,	/*PVR2D_ARGB1555*/
	PVRSRV_PIXEL_FORMAT_UNKNOWN,
	PVRSRV_PIXEL_FORMAT_UNKNOWN,
	PVRSRV_PIXEL_FORMAT_UNKNOWN,
	PVRSRV_PIXEL_FORMAT_UNKNOWN,
	PVRSRV_PIXEL_FORMAT_UNKNOWN,
	PVRSRV_PIXEL_FORMAT_UNKNOWN,
	PVRSRV_PIXEL_FORMAT_UNKNOWN
};

/******************************************************************************
 @Function	PVR2DCreateFlipChain

 @Input		hContext		: pvr2d context handle

 @Input		ulFlags			: Flip Chain Flags

 @Input		ulNumBuffers	: number of buffers in flip chain

 @Input		ulWidth			: The pixel width of the flip chain to create

 @Input		ulHeight		: The pixel height of the flip chain to create

 @Input		pulFlipChainID	: If PVR2D_CREATE_FLIPCHAIN_QUERY is set, this contains
 							the unique identifier of the flip chain to query. Must
 							be between 0 and the maximum number of flip chains
 							supported by the device.

 @Output	plStride		: Stride of the buffers

 @Output	pulFlipChainID	: If PVR2D_CREATE_FLIPCHAIN_QUERY is not set, this returns
 							the ID of the newly created flip chain. This is only
 							returned if PVR2D_CREATE_FLIPCHAIN_SHARED is set.

 @Output	phFlipChain		The flip chain that has been created

 @Return	PVR2D_OK - Success
			PVR2DERROR_GENERIC - The flip chain cannot be created
			PVR2DERROR_INVALID_PARAMETER - A parameter is invalid
			PVR2DERROR_INVALID_CONTEXT	 - The context is invalid

 @Description:

 Creates a flip chain with a given width and height. The number of buffers
 specified are used to allocate/associate buffers for the flip chain.
 On any given display device the number of flip chains which can be created
 may be 0 or more. The memory for the flip buffers may have been previously
 allocated outside of PVR2D. If the PVR2D_CREATE_FLIPCHAIN_QUERY flag is set,
 this call simply gets a reference to a pre-existing flip chain specified by
 *pulFlipChainID. If it is not set, a new flip chain is created and a unique
 flipchain ID will be returned if PVR2D_CREATE_FLIPCHAIN_SHARED is also set.
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DCreateFlipChain (PVR2DCONTEXTHANDLE hContext,
								PVR2D_ULONG ulFlags,
								PVR2D_ULONG ulNumBuffers,
								PVR2D_ULONG ulWidth,
								PVR2D_ULONG ulHeight,
								PVR2DFORMAT eFormat,
								PVR2D_LONG  *plStride,
								PVR2D_ULONG *pulFlipChainID,
								PVR2DFLIPCHAINHANDLE *phFlipChain)
{
	PVR2DCONTEXT *psContext;
	PVR2DERROR eError;
	PVRSRV_ERROR ePVRSRVErr;
	PVR2D_FLIPCHAIN *psFlipChain;
	IMG_UINT32 i, ui32Flags = 0;
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID *phBuffer = IMG_NULL;
#else
	IMG_HANDLE *phBuffer = IMG_NULL;
#endif
	PVRSRV_PIXEL_FORMAT ePVRSRVPixelFormat;
	IMG_UINT32 ui32FlipChainID;

	/* Check incoming parameters */
	if(	(hContext == IMG_NULL)
	||	(phFlipChain == IMG_NULL)
	||	(plStride == IMG_NULL)
	||	(eFormat >= PVR2D_NO_OF_FORMATS))
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateFlipChain: Invalid parameter"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if ((ulFlags & PVR2D_CREATE_FLIPCHAIN_QUERY) != 0 && pulFlipChainID == IMG_NULL)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateFlipChain: Pointer to flip chain ID not set"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if (pulFlipChainID != IMG_NULL)
	{
		ui32FlipChainID = *pulFlipChainID;
	}

	/* Set the PVR2D context */
	psContext = (PVR2DCONTEXT*)hContext;
	if (psContext->hDisplayClassDevice == 0)
		return PVR2DERROR_DEVICE_NOT_PRESENT;

	if(!psContext->sDisplayInfo.ui32MaxSwapChains)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateFlipChain: No flip chains supported on this device"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psFlipChain = PVR2DCalloc(psContext, sizeof(PVR2D_FLIPCHAIN));
	if(!psFlipChain)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateFlipChain: can't create flipchain"));
		return PVR2DERROR_MEMORY_UNAVAILABLE;
	}

	/* Store context */
	psFlipChain->hContext = hContext;

	/* Initial clip states: should be zeroed by Calloc */
	PVR_ASSERT(psFlipChain->ui32NumClipRects == 0);
	PVR_ASSERT(psFlipChain->ui32NumActiveClipRects == 0);

	ePVRSRVPixelFormat = aConvertPVR2DPixelFormatToPVRSRV[eFormat];

	/* Setup DST attributes */
	psFlipChain->sDstSurfAttrib.pixelformat = ePVRSRVPixelFormat;
	psFlipChain->sDstSurfAttrib.sDims.ui32Width = ulWidth;
	psFlipChain->sDstSurfAttrib.sDims.ui32Height = ulHeight;
	psFlipChain->sDstSurfAttrib.sDims.ui32ByteStride = psContext->sPrimary.sDims.ui32ByteStride;

	/* Setup SRC attributes */
	psFlipChain->sSrcSurfAttrib.pixelformat = ePVRSRVPixelFormat;
	psFlipChain->sSrcSurfAttrib.sDims.ui32Width = ulWidth;
	psFlipChain->sSrcSurfAttrib.sDims.ui32Height = ulHeight;
	psFlipChain->sSrcSurfAttrib.sDims.ui32ByteStride = psContext->sPrimary.sDims.ui32ByteStride;

	if(ulFlags & PVR2D_CREATE_FLIPCHAIN_SHARED)
	{
		ui32Flags |= PVRSRV_CREATE_SWAPCHAIN_SHARED;
	}

	if(ulFlags & PVR2D_CREATE_FLIPCHAIN_QUERY)
	{
		if(ulFlags & PVR2D_CREATE_FLIPCHAIN_SHARED)
		{
			ui32Flags |= PVRSRV_CREATE_SWAPCHAIN_QUERY;
		}
		else
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateFlipChain: Invalid parameter"));
			eError = PVR2DERROR_GENERIC_ERROR;
			goto ErrorExit;
		}
	}

    if(ulFlags & PVR2D_CREATE_FLIPCHAIN_OEMOVERLAY)
    {
       ui32Flags |= PVR2D_CREATE_FLIPCHAIN_OEMOVERLAY;
    }

    if(ulFlags & PVR2D_CREATE_FLIPCHAIN_AS_BLITCHAIN)
    {
       ui32Flags |= PVR2D_CREATE_FLIPCHAIN_AS_BLITCHAIN;
    }	

	/* Create a flip chain on the chosen display device */
	ePVRSRVErr = PVRSRVCreateDCSwapChain(psContext->hDisplayClassDevice,
						ui32Flags,
						&psFlipChain->sDstSurfAttrib,
						&psFlipChain->sSrcSurfAttrib,
						ulNumBuffers,
						0, /* ui32OEMFlags */
						&ui32FlipChainID,
						&psFlipChain->hDCSwapChain);
	if(ePVRSRVErr != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateFlipChain: can't create flipchain"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto ErrorExit;
	}

	psFlipChain->psBuffer = PVR2DCalloc(psContext,
						sizeof(PVR2D_BUFFER) * ulNumBuffers);
	if(!psFlipChain->psBuffer)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateFlipChain: can't allocate system memory for flipchain structure"));
		eError = PVR2DERROR_MEMORY_UNAVAILABLE;
		goto ErrorExit;
	}

	psFlipChain->ui32NumBuffers = ulNumBuffers;

	phBuffer = PVR2DMalloc(psContext, sizeof(IMG_HANDLE) * ulNumBuffers);
	if(!phBuffer)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateFlipChain: can't allocate system memory for buffers handles"));
		eError = PVR2DERROR_MEMORY_UNAVAILABLE;
		goto ErrorExit;
	}


	/* Get information for the swapchain's buffers */
	ePVRSRVErr = PVRSRVGetDCBuffers(psContext->hDisplayClassDevice,
					psFlipChain->hDCSwapChain,
					phBuffer);
	if(ePVRSRVErr != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateFlipChain: couldn't get buffers"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto ErrorExit;
	}

	for(i=0; i < psFlipChain->ui32NumBuffers; i++)
	{
		PVRSRV_CLIENT_MEM_INFO *psSrvMemInfo;
		PVR2D_BUFFER *psBuffer = &psFlipChain->psBuffer[i];

		psBuffer->hDisplayBuffer = phBuffer[i];

		/* Map the system buffer into device address space */
		if(PVRSRVMapDeviceClassMemory(&psContext->sDevData,
						psContext->hDevMemContext,
						psBuffer->hDisplayBuffer,
						&psSrvMemInfo) != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateFlipChain: can't map buffer into device address space"));
			eError = PVR2DERROR_GENERIC_ERROR;
			goto ErrorExit;
		}
		PVR2DMEMINFO_INITIALISE(&psBuffer->sMemInfo, psSrvMemInfo);
		psBuffer->eType = PVR2D_MEMINFO_DISPLAY_OWNED;
	}

	PVR2DFree(psContext, phBuffer);

	/* Initialise to default value */
	psFlipChain->ui32SwapInterval = 1;

	/* Return the values */
	*plStride = (PVR2D_LONG)psFlipChain->sSrcSurfAttrib.sDims.ui32ByteStride;
	*phFlipChain = (IMG_HANDLE)psFlipChain;

	/*
		insert the flip chain we created into the head of our list
		note: queries don't apply
	*/
	if((ulFlags & PVR2D_CREATE_FLIPCHAIN_QUERY) == 0)
	{
		/* insert at the head */
		psFlipChain->psNext = psContext->psFlipChain;
		psContext->psFlipChain = psFlipChain;
	}

	if (pulFlipChainID != IMG_NULL)
	{
		*pulFlipChainID = ui32FlipChainID;
	}

	return PVR2D_OK;

ErrorExit:

	DestroyFlipChain(psContext, psFlipChain);

	if(phBuffer)
	{
		PVR2DFree(psContext, phBuffer);
	}

	PVR2DFree(psContext, psFlipChain);

	return eError;
}

/******************************************************************************
 @Function	DestroyFlipChain

 @Input		psContext		: pvr2d context

 @Input		psFlipChain		: The flip chain to destroy.

 @Return	PVR2D_OK - Success
			PVR2DERROR_GENERIC - The flip chain cannot be destroyed
			PVR2DERROR_INVALID_PARAMETER - The flip chain handle is invalid
			PVR2DERROR_INVALID_CONTEXT - The context is invalid

 @Description:

 Internal API to destroy a flip chain
******************************************************************************/
IMG_INTERNAL
PVR2DERROR DestroyFlipChain(PVR2DCONTEXT *psContext, PVR2D_FLIPCHAIN *psFlipChain)
{
	PVR2DERROR eError = PVR2D_OK;
	PVR2D_BUFFER *psBuffer;
	IMG_UINT32 i;
	PVRSRV_ERROR ePVRSRVError = PVRSRV_ERROR_RETRY;
	
	if(psFlipChain->psBuffer)
	{
		for(i=0; i < psFlipChain->ui32NumBuffers; i++)
		{
			psBuffer = &psFlipChain->psBuffer[i];

			// Wait for outstanding read/write ops to complete before freeing the memory
			eError = PVR2DQueryBlitsComplete((PVR2DCONTEXTHANDLE)psContext, &psBuffer->sMemInfo, 1);
			if (eError != PVR2D_OK)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "DestroyFlipChain -> PVR2DQueryBlitsComplete failed"));
				continue;
			}

			if(CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(&psBuffer->sMemInfo))
			{
				if(PVRSRVUnmapDeviceClassMemory(&psContext->sDevData,
								CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(&psBuffer->sMemInfo)) != PVRSRV_OK)
				{
					PVR2D_DPF((PVR_DBG_ERROR, "DestroyFlipChain: can't unmap buffers from device address space"));
				}
			}
		}

		PVR2DFree(psContext, psFlipChain->psBuffer);
		psFlipChain->psBuffer = IMG_NULL;
	}

	if(psFlipChain->hDCSwapChain)
	{
		if(PVRSRVDestroyDCSwapChain(psContext->hDisplayClassDevice,
									psFlipChain->hDCSwapChain) != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DDestroyFlipChain: can't destroy flipchain"));
			eError = PVR2DERROR_GENERIC_ERROR;
		}
		else
		{
			psFlipChain->hDCSwapChain = 0;
		}
	}

	return eError;
}


/******************************************************************************
 @Function	PVR2DDestroyFlipChain

 @Input		hContext		: pvr2d context handle

 @Input		hFlipChain		: The flip chain to destroy.

 @Return	PVR2D_OK - Success
			PVR2DERROR_GENERIC - The flip chain cannot be destroyed
			PVR2DERROR_INVALID_PARAMETER - The flip chain handle is invalid
			PVR2DERROR_INVALID_CONTEXT - The context is invalid

 @Description:

 Destroys resources associated with a flip chain. The memory for the flip buffers
 may have been previously allocated outside of PVR2D in which case this call simply
 breaks the association between the buffers and the flip chain.
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DDestroyFlipChain (PVR2DCONTEXTHANDLE hContext,
								  PVR2DFLIPCHAINHANDLE hFlipChain)
{
	PVR2DCONTEXT *psContext;
	PVR2D_FLIPCHAIN *psFlipChain = (PVR2D_FLIPCHAIN *)hFlipChain;
	PVR2D_FLIPCHAIN **ppsNode;

	PVR2DERROR eError = PVR2D_OK;

	/* Check incoming parameters */
	if(	(hContext == IMG_NULL)
	||	(hFlipChain == IMG_NULL))
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DDestroyFlipChain: Invalid parameter"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psContext = (PVR2DCONTEXT *)hContext;
	if (psContext->hDisplayClassDevice == 0)
		return PVR2DERROR_DEVICE_NOT_PRESENT;

	/* Check if psContext matches the one from the FlipChain */
	if(hContext != psFlipChain->hContext)
	{
		PVR2D_DPF((PVR_DBG_WARNING, "PVR2DDestroyFlipChain: Context different from the one in FlipChain"));
		return eError;
	}

	eError = DestroyFlipChain (psContext, psFlipChain);
	if(eError != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_WARNING, "PVR2DDestroyFlipChain: DestroyFlipChain failed"));
		return eError;
	}

	/*
		Find the flip chain in our list.
		Note: if the flip chain was created as a shared query
		it won't be in the list.
	*/
	ppsNode = &psContext->psFlipChain;
	while(*ppsNode)
	{
		if(*ppsNode == psFlipChain)
		{
			/* found it, remove from the list */
			*ppsNode = psFlipChain->psNext;
			break;
		}

		/* advance to the next node */
		ppsNode = &((*ppsNode)->psNext);
	}

	psFlipChain->hContext = IMG_NULL;
	PVR2DFree(psContext, psFlipChain);

	return eError;
}


/******************************************************************************
 @Function	PVR2DGetFlipChainBuffers

 @Input		hContext		: pvr2d context handle

 @Input		hFlipChain		: The flip chain to destroy.

 @Output	pulNumBuffers	: The number of buffers in this list is the number
								with which the flip chain was created

 @Output	psMemInfo		: An array of pointers to buffers associated with this flip chain.

 @Return	PVR2D_OK - Success
			PVR2DERROR_INVALID_PARAMETER - The flip chain is invalid

 @Description:

 Gets the buffers associated with a flip chain.

 Note : The caller must first call this function with ppsMemInfo set to NULL
 to get the number of buffers. Then the caller must allocate enough memory to
 contain that number of PVR2DMEMINFO pointers and call the function a second
 time, passing in the allocated memory via the ppsMemInfo parameter to
 contain the output (array of pointers).
 *****************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DGetFlipChainBuffers (PVR2DCONTEXTHANDLE hContext,
									PVR2DFLIPCHAINHANDLE hFlipChain,
									PVR2D_ULONG *pulNumBuffers,
									PVR2DMEMINFO *psMemInfo[])
{
	PVR2D_FLIPCHAIN *psFlipChain = (PVR2D_FLIPCHAIN *)hFlipChain;
	IMG_UINT32 i;

	/* Check incoming parameters */
	if(	(hContext == IMG_NULL)
	||	(hFlipChain == IMG_NULL)
	||	(pulNumBuffers == IMG_NULL))
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DGetFlipChainBuffers: Invalid parameter"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	/* Return the information */
	*pulNumBuffers = psFlipChain->ui32NumBuffers;

	if (psMemInfo)
	{
		for(i=0; i < psFlipChain->ui32NumBuffers; i++)
		{
			psMemInfo[i] = &psFlipChain->psBuffer[i].sMemInfo;
		}
	}

	return PVR2D_OK;
}


/******************************************************************************
 @Function	PVR2DPresentFlip

 @Input		hContext	: pvr2d context handle

 @Input		hFlipChain	: The flip chain to flip from.

 @Input		psMemInfo	: The buffer to flip to.

 @Input		hPrivateData	: Private data associated with this flip

 @Return	PVR2D_OK - Success
			PVR2DERROR_INVALID_PARAMETER - A parameter is invalid
			PVR2DERROR_DEVICE_UNAVAILABLE - Device cannot complete request
			PVR2DERROR_INVALID_CONTEXT - The context is invalid

 @Description:

 The present properties are used to perform a (potentially clipped, sized) flip
 from the specified buffer to the display. The buffer must be one which has been
 retrieved from PVR2DGetFlipChainBuffers.
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DPresentFlip (PVR2DCONTEXTHANDLE hContext,
							PVR2DFLIPCHAINHANDLE hFlipChain,
							PVR2DMEMINFO *psMemInfo,
							PVR2D_LONG lRenderID)
{
	PVR2D_FLIPCHAIN *psFlipChain = (PVR2D_FLIPCHAIN *)hFlipChain;
	PVR2D_BUFFER *psBuffer = (PVR2D_BUFFER *)psMemInfo;
	PVR2DCONTEXT *psContext= (PVR2DCONTEXT *)hContext;
	PVRSRV_ERROR eError = PVRSRV_ERROR_RETRY;

	/* check incoming parameters */
	if(	(hContext == IMG_NULL)
	||	(hFlipChain == IMG_NULL))
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentFlip: Invalid parameter"));
		return PVR2DERROR_INVALID_PARAMETER;		
	}

	if (psMemInfo)
	{
		if (psBuffer->eType != PVR2D_MEMINFO_DISPLAY_OWNED)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentFlip: Buffer not part of flip chain"));
			return PVR2DERROR_INVALID_PARAMETER;			
		}
	
		if (psContext->hDisplayClassDevice == 0)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentFlip: Device not present"));
			return PVR2DERROR_DEVICE_NOT_PRESENT;			
		}

		eError = PVRSRVSwapToDCBuffer (psContext->hDisplayClassDevice,
										psBuffer->hDisplayBuffer,
										psFlipChain->ui32NumActiveClipRects,
										(IMG_RECT *)psFlipChain->psClipRects,
										psFlipChain->ui32SwapInterval,
#if defined (SUPPORT_SID_INTERFACE)
										(IMG_SID)lRenderID);
#else
										(IMG_HANDLE)lRenderID);
#endif

		if(eError != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentFlip: Can't flip to specified buffer"));
			return PVR2DERROR_DEVICE_UNAVAILABLE;
		}
	}

	return PVR2D_OK;
}

/******************************************************************************
 @Function	PVR2DSetPresentFlipProperties

 @Input    hContext        : pvr2d context handle

 @Input    hFlipChain      : Optional flip chain to set properties for

 @Input    ulPropertyMask  : Mask of properties to set

 @Input    ulDstXPos       : X Position on dest surface for presentation

 @Input    ulDstYPos       : Y Position on dest surface for presentation

 @Input    ulNumClipRects  : Number of cliprects present in pClipRects param

 @Input    pClipRects	   : List of ulNumClipRects Clip rectangles to apply to
                             presentation

 @Input    ulSwapInterval  : Number of display intervals to wait before flipping

 @Return   error           : error code

 @Description              : set presentation properties for flipping or blitting
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DSetPresentFlipProperties (	PVR2DCONTEXTHANDLE hContext,
											PVR2DFLIPCHAINHANDLE hFlipChain,
											PVR2D_ULONG ulPropertyMask,
											PVR2D_LONG  lDstXPos,
											PVR2D_LONG  lDstYPos,
											PVR2D_ULONG ulNumClipRects,
											PVR2DRECT   *pClipRects,
											PVR2D_ULONG ulSwapInterval)
{
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PVR2D_FLIPCHAIN *psFlipChain = (PVR2D_FLIPCHAIN *)hFlipChain;

	if (!hContext || !hFlipChain)
		return PVR2DERROR_INVALID_PARAMETER;

	if (psContext->hDisplayClassDevice == 0)
		return PVR2DERROR_DEVICE_NOT_PRESENT;

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_SRCSTRIDE)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DSetPresentFlipProperties error - Can't set src stride for a flipchain!"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_DSTSIZE)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DSetPresentFlipProperties error - Can't set dst size for a flipchain!"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_INTERVAL)
	{
		psFlipChain->ui32SwapInterval = ulSwapInterval;
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_DSTPOS)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DSetPresentFlipProperties error - PVR2D_PRESENT_PROPERTY_DSTPOS is unimplemented!"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_CLIPRECTS)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DSetPresentFlipProperties error - PVR2D_PRESENT_PROPERTY_CLIPRECTS is unimplemented!"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	return PVR2D_OK;
}

/******************************************************************************
 End of file (pvr2dflip.c)
******************************************************************************/
