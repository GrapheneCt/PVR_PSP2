/*!****************************************************************************
@File           pvr2dinit.c

@Title          PVR2D library initialisation

@Author         Imagination Technologies

@Date           14/12/2006

@Copyright      Copyright 2006-2008 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or otherwise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform       Generic

@Description    PVR2D library initialisation code

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: pvr2dinit.c $
******************************************************************************/

#include "psp2_pvr_defs.h"
#include "psp2_pvr_desc.h"

#include "img_defs.h"
#include "services.h"
#include "pvr2d.h"
#include "pvr2dint.h"

#include "sgxapi.h"

#include <string.h>		/* strncpy */

static SceUID s_psp2DriverMemBlockUID = SCE_UID_INVALID_UID;
static PVR2D_SID s_psp2ProcRefId = 0;

/******************************************************************************
 @Function	PVR2DEnumerateDevices

 @Input    pDeviceInfo          : device info structure

 @Return   int                  : number of devices or error code (< 0)

 @Description                   : enumerate list of display devices 
******************************************************************************/
PVR2D_EXPORT
PVR2D_INT PVR2DEnumerateDevices (PVR2DDEVICEINFO *pDeviceInfo)
{
	IMG_UINT32 uiNumDispDevices;
	PVRSRV_ERROR eSrvError;
	PVRSRV_CONNECTION *psConnection;
	PVR2D_INT iRetVal = PVR2DERROR_GENERIC_ERROR;	/* PRQA S 3197 */ /* initialise anyway */

	/* Connect to services */
	eSrvError = PVRSRVConnect(&psConnection, 0);
	if (eSrvError != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DEnumerateDevices: PVRSRVConnect failed"));

		return PVR2DERROR_DEVICE_UNAVAILABLE;
	}

	/* Find all display class devices */
	eSrvError = PVRSRVEnumerateDeviceClass(psConnection,
			PVRSRV_DEVICE_CLASS_DISPLAY,
			&uiNumDispDevices, IMG_NULL);
	if (eSrvError != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DEnumerateDevices: PVRSRVEnumerateDC failed"));

		iRetVal = PVR2DERROR_DEVICE_UNAVAILABLE;
		goto out;
	}

	if (pDeviceInfo)
	{
		IMG_UINT32	uiNumDevices;
		PVRSRV_DEVICE_IDENTIFIER asDevID[PVRSRV_MAX_DEVICES];
		PVRSRV_DEV_DATA sDevData;
		IMG_UINT32 *puiDisplayIDs;
		IMG_UINT32 i;

		/* Enumerate all devices */
		if (PVRSRVEnumerateDevices(psConnection,
						&uiNumDevices,
						asDevID) != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DEnumerateDevices: PVRSRVEnumerateDevices failed"));
			iRetVal = PVR2DERROR_DEVICE_UNAVAILABLE;
			goto out;
		}

		/* Find device */
		for (i=0; i < uiNumDevices; i++)
		{
			if (asDevID[i].eDeviceClass == PVRSRV_DEVICE_CLASS_3D)
			{
				if (PVRSRVAcquireDeviceData (psConnection,
								asDevID[i].ui32DeviceIndex,
								&sDevData,
								PVRSRV_DEVICE_TYPE_UNKNOWN) != PVRSRV_OK)
				{
					PVR2D_DPF((PVR_DBG_ERROR, "PVR2DEnumerateDevices: PVRSRVAcquireDeviceData failed"));
					iRetVal = PVR2DERROR_IOCTL_ERROR;
					goto out;
				}
			}
		}

		puiDisplayIDs = (IMG_UINT32*)PVR2DCalloc(IMG_NULL,
								sizeof(IMG_UINT32)*uiNumDispDevices);
		if (!puiDisplayIDs)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DEnumerateDevices: PVRSRVMemAlloc failed"));
			iRetVal = PVR2DERROR_MEMORY_UNAVAILABLE;
			goto out;
		}

		/* Find services device IDs for all display class devices */
		eSrvError = PVRSRVEnumerateDeviceClass(psConnection,
						PVRSRV_DEVICE_CLASS_DISPLAY,
						&uiNumDispDevices,
						puiDisplayIDs);
		if (eSrvError != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DEnumerateDevices: PVRSRVEnumerateDC failed"));
			PVR2DFree(IMG_NULL, puiDisplayIDs);
			iRetVal = PVR2DERROR_DEVICE_UNAVAILABLE;
			goto out;
		}

		for (i=0; i < uiNumDispDevices; i++)
		{
			IMG_HANDLE hDisplayClassDevice;

			/* Open all display class devices and find device class IDs*/
			hDisplayClassDevice = PVRSRVOpenDCDevice(&sDevData, puiDisplayIDs[i]);
			
			if (hDisplayClassDevice)
			{
				DISPLAY_INFO sDisplayInfo;

				eSrvError = PVRSRVGetDCInfo(hDisplayClassDevice, &sDisplayInfo);
						
				if (eSrvError != PVRSRV_OK)
				{
					PVR2D_DPF((PVR_DBG_ERROR, "PVR2DEnumerateDevices: PVRSRVGetDCInfo failed"));
					PVR2DFree(IMG_NULL, puiDisplayIDs);
					iRetVal = PVR2DERROR_GENERIC_ERROR;
					goto out;
				}

				// Check for string overrun
				if (strlen(sDisplayInfo.szDisplayName) >= PVR2D_MAX_DEVICE_NAME)
				{
					PVR2D_DPF((PVR_DBG_WARNING, "PVR2DEnumerateDevices: display device name too long, truncating"));
				}
				
				PVRSRVMemCopy(	(IMG_VOID*) pDeviceInfo[i].szDeviceName,	// Dest Size PVR2D_MAX_DEVICE_NAME = 20
								(IMG_VOID*) sDisplayInfo.szDisplayName,		// Source Size MAX_DISPLAY_NAME_SIZE = 50
				#if PVR2D_MAX_DEVICE_NAME <= MAX_DISPLAY_NAME_SIZE			// Prevent overrun if the string length defs change
								PVR2D_MAX_DEVICE_NAME);						// No. of bytes to copy
				#else
								MAX_DISPLAY_NAME_SIZE);
				#endif

				/* Ensure NULL terminated string */
				pDeviceInfo[i].szDeviceName[PVR2D_MAX_DEVICE_NAME-1] = 0;

				pDeviceInfo[i].ulDevID = puiDisplayIDs[i];

				eSrvError = PVRSRVCloseDCDevice(psConnection, hDisplayClassDevice);
				
				if (eSrvError != PVRSRV_OK)
				{
					PVR2D_DPF((PVR_DBG_ERROR, "PVR2DEnumerateDevices: PVRSRVCloseDCDevice failed"));
					PVR2DFree(IMG_NULL, puiDisplayIDs);
					iRetVal = PVR2DERROR_GENERIC_ERROR;
					goto out;
				}
			}
			else
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2DEnumerateDevices: PVRSRVOpenDCDevice failed"));
				PVR2DFree(IMG_NULL, puiDisplayIDs);
				iRetVal = PVR2DERROR_DEVICE_UNAVAILABLE;
				goto out;
			}
		}

		PVR2DFree(IMG_NULL, puiDisplayIDs);
		iRetVal = PVR2D_OK;
	}
	else
	{
		iRetVal = (PVR2D_INT)uiNumDispDevices;
	}

out:
	if (PVRSRVDisconnect(psConnection) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DEnumerateDevices: PVRSRVDisconnect failed"));
	}

	return iRetVal;
}

/******************************************************************************
 @Function	InitDisplayDevice

 @Input    psContext            : pointer to the PVR2D Context

 @Input    ui32DeviceID         : device ID

 @Return   PVR2DERROR           : error code

 @Description                   : DisplayClass portion of context creation
******************************************************************************/
static PVR2DERROR InitDisplayDevice(PVR2DCONTEXT *psContext, IMG_UINT32 ui32DeviceID)
{
	DISPLAY_FORMAT*	psPrimFormat = 0;
	DISPLAY_DIMS* psPrimDims = 0;
	IMG_UINT32 ui32Temp;
	PVR2DERROR eError = PVR2D_OK;
#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
#if defined(SUPPORT_SID_INTERFACE)
	IMG_SID    hSystemBuffer = 0;
#else
	IMG_HANDLE hSystemBuffer = 0;
#endif
#endif
	PVRSRV_CLIENT_MEM_INFO *psMemInfo = (PVRSRV_CLIENT_MEM_INFO *)0;

	/* Open specified display device */
	psContext->hDisplayClassDevice = PVRSRVOpenDCDevice(&psContext->sDevData,
								ui32DeviceID);
	/*
	 * Return PVR2DERROR_DEVICE_NOT_PRESENT if the display device
	 * can't be opened.  PVR2D can be used with reduced functionality
	 * in this case, and this return code tells the caller that the
	 * error shouldn't be regarded as fatal.
	 */
	if (psContext->hDisplayClassDevice == 0)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: OpenDCDevice failed"));
		return PVR2DERROR_DEVICE_NOT_PRESENT;
	}

	/* get display info for this device */
	if (PVRSRVGetDCInfo (psContext->hDisplayClassDevice,
				&psContext->sDisplayInfo) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: (PVRSRVGetDCInfo failed"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto display_cleanup;
	}

	/* get number of primary pixel formats */
	if (PVRSRVEnumDCFormats (psContext->hDisplayClassDevice,
					&ui32Temp,
					IMG_NULL) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: PVRSRVsEnumDCFormats failed"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto display_cleanup;
	}

	psPrimFormat = PVR2DMalloc(psContext,
					sizeof(DISPLAY_FORMAT) * ui32Temp);
	if (!psPrimFormat)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: Not enough memory available"));
		eError = PVR2DERROR_MEMORY_UNAVAILABLE;
		goto display_cleanup;
	}

	/* get all primary pixel formats */
	if (PVRSRVEnumDCFormats(psContext->hDisplayClassDevice,
					&ui32Temp,
					psPrimFormat) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: PVRSRVEnumDCFormats failed"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto display_cleanup;
	}

	/* First format in the list is the current mode of the display */
	psContext->sPrimary.pixelformat = psPrimFormat[0].pixelformat;

	/* get number of dimensions for the current pixel format */
	if (PVRSRVEnumDCDims(psContext->hDisplayClassDevice,
				&ui32Temp,
				&psPrimFormat[0],
				IMG_NULL) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: PVRSRVEnumDCDims failed"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto display_cleanup;
	}

	psPrimDims = PVR2DMalloc(psContext, sizeof(DISPLAY_DIMS) * ui32Temp);
	if (!psPrimDims)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: Not enough memory available"));
		eError = PVR2DERROR_MEMORY_UNAVAILABLE;
		goto display_cleanup;
	}

	/* get dimension info for the current pixel format */
	if (PVRSRVEnumDCDims(psContext->hDisplayClassDevice,
				&ui32Temp,
				&psPrimFormat[0],
				psPrimDims) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: PVRSRVEnumDCDims failed"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto display_cleanup;
	}

	/* First dimension in the list is the current mode of the display */
	psContext->sPrimary.sDims = psPrimDims[0];

#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
	/* Get a handle for the system buffer */
	if (PVRSRVGetDCSystemBuffer(psContext->hDisplayClassDevice,
					 &hSystemBuffer) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: PVRSRVGetDCSystemBuffer failed"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto display_cleanup;
	}

	/* Map the system buffer into device address space*/
	if (PVRSRVMapDeviceClassMemory(&psContext->sDevData,
					psContext->hDevMemContext,
					hSystemBuffer,
					&psMemInfo) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: PVRSRVMapDeviceClassMemory failed"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto display_cleanup;
	}
	PVR2DMEMINFO_INITIALISE(&psContext->sPrimaryBuffer, psMemInfo);
#endif

	goto display_ok;

display_cleanup:

	if (psMemInfo)
	{
		/* Unmap the system buffer from device address space*/
		if (PVRSRVUnmapDeviceClassMemory(&psContext->sDevData, psMemInfo) == PVRSRV_OK)
		{
			psContext->sPrimaryBuffer.pBase = 0;
			psContext->sPrimaryBuffer.ui32DevAddr = 0;
			psContext->sPrimaryBuffer.ui32MemSize = 0;
			psContext->sPrimaryBuffer.ulFlags = 0;
			psContext->sPrimaryBuffer.hPrivateData = 0;
			psContext->sPrimaryBuffer.hPrivateMapData = 0;
		}
		else
		{
			PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: PVRSRVUnmapDeviceClassMemory failed"));
		}
	}

	if (psContext->hDisplayClassDevice)
	{
		if (PVRSRVCloseDCDevice(psContext->psServices, psContext->hDisplayClassDevice) == PVRSRV_OK)
		{
			psContext->hDisplayClassDevice = 0;
		}
		else
		{
			PVR2D_DPF((PVR_DBG_ERROR, "InitDisplayDevice: PVRSRVCloseDCDevice failed"));
		}
	}

display_ok:

	if (psPrimFormat)
	{
		PVR2DFree(psContext, psPrimFormat);
	}

	if (psPrimDims)
	{
		PVR2DFree(psContext, psPrimDims);
	}

	return eError;
}


/******************************************************************************
 @Function	_PVR2DCreateContext

 @Input    ui32Size             : Context size, in bytes

 @Return   PVR2DCONTEXT*        : Newly created context

 @Description                   : Creates a PVR2D device context 
******************************************************************************/
static PVR2DCONTEXT* _PVR2DCreateContext(IMG_UINT32 ui32Size)
{
	return (PVR2DCONTEXT *)PVR2DCalloc(IMG_NULL, ui32Size);
}

/******************************************************************************
 @Function	_PVR2DDestroyContext

 @Input    psContext            : PVR2D context

 @Return   None

 @Description                   : Destroys a PVR2D device context 
******************************************************************************/
static IMG_VOID _PVR2DDestroyContext(PVR2DCONTEXT *psContext)
{
	PVR2DFree(IMG_NULL, psContext);
}

/******************************************************************************
 @Function	PVR2DCreateDeviceContext

 @Input    ulDevID              : device ID 

 @Input    phContext            : ref to Context pointer 

 @Input    ulFlags              :  

 @Return   error                : error code

 @Description                   : create a PVR2D device context 
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DCreateDeviceContext (PVR2D_ULONG ulDevID,
									PVR2DCONTEXTHANDLE* phContext,
									PVR2D_ULONG ulFlags)
{
	PVR2DCONTEXT *psContext;
	PVR2DERROR eError;
	IMG_UINT32	uiNumDevices, i;
	PVRSRV_DEVICE_IDENTIFIER asDevID[PVRSRV_MAX_DEVICES];
	IMG_BOOL bConnectedToServices = IMG_FALSE;
	IMG_BOOL bValidDeviceMemContext = IMG_FALSE;
	IMG_UINT32 ui32ClientHeapCount;
	PVRSRV_HEAP_INFO asHeapInfo[PVRSRV_MAX_CLIENT_HEAPS];
	IMG_BOOL bValid2DHeap = IMG_FALSE;
	IMG_BOOL bValidDriverMemBlock = IMG_FALSE;
	PVRSRV_ERROR eSrvError;

	/* forward flags to Services */
	IMG_UINT32 ui32SrvFlags = 0;

	if (!phContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext - invalid param"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if (ulFlags & PVR2D_XSERVER_PROC)
	{
		ui32SrvFlags |= SRV_FLAGS_PERSIST;
	}
	if (ulFlags & PVR2D_FLAGS_PDUMP_ACTIVE)
	{
		ui32SrvFlags |= SRV_FLAGS_PDUMP_ACTIVE;
	}

	/* create the context */
	psContext = _PVR2DCreateContext(sizeof(PVR2DCONTEXT));
	if(!psContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: Not enough memory available"));
		return PVR2DERROR_MEMORY_UNAVAILABLE;
	}

	psContext->bIsExtRegMemblock = IMG_TRUE;

#if defined(EUR_CR_2D_ALPHA_COMPONENT_ZERO_MASK) && !defined(PVR2D_ALT_2DHW)
	psContext->ui32AlphaRegValue = DEFAULT_ALPHA1555; /* Default 1555 lookup value */
#endif

	/* Connect to services */
	if (PVRSRVConnect(&psContext->psServices, ui32SrvFlags) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: PVRSRVConnect failed"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto cleanup;
	}
	bConnectedToServices = IMG_TRUE;
	
	// Get global event object
	psContext->sMiscInfo.ui32StateRequest = PVRSRV_MISC_INFO_GLOBALEVENTOBJECT_PRESENT;
	if (PVRSRVGetMiscInfo(psContext->psServices, &psContext->sMiscInfo) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: PVRSRVGetMiscInfo failed"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto cleanup;
	}
	
#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2DCreateDeviceContext : Set Frame 0", IMG_FALSE);
	PVRSRVPDumpSetFrame(psContext->psServices, 0);
#endif

	/* Enumerate all devices */
	if (PVRSRVEnumerateDevices(psContext->psServices,
					&uiNumDevices,
					asDevID) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: PVRSRVEnumerateDevices failed"));
		eError = PVR2DERROR_DEVICE_UNAVAILABLE;
		goto cleanup;
	}

	/* Find device */
	for (i=0; i < uiNumDevices; i++)
	{
		if (asDevID[i].eDeviceClass == PVRSRV_DEVICE_CLASS_3D)
		{
			if (PVRSRVAcquireDeviceData (psContext->psServices,
							asDevID[i].ui32DeviceIndex,
							&psContext->sDevData,
							PVRSRV_DEVICE_TYPE_UNKNOWN) != PVRSRV_OK)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: PVRSRVAcquireDeviceData failed"));
				eError = PVR2DERROR_IOCTL_ERROR;
				goto cleanup;
			}
		}
	}

	/*
	 * If a device memory context has already been created for this process, PVR2D will 
	 * get a handle to it. Otherwise create its own device memory context.
	 */
	if(PVRSRVCreateDeviceMemContext(&psContext->sDevData,
									&psContext->hDevMemContext,
									&ui32ClientHeapCount,
									asHeapInfo) != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: PVRSRVCreateDeviceMemContext failed"));
		eError = PVR2DERROR_IOCTL_ERROR;
		goto cleanup;
	}

	bValidDeviceMemContext = IMG_TRUE;
	
	#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2D Init", IMG_FALSE);
	#endif

	/* Get 2D heap handle */
	for(i=0; i<ui32ClientHeapCount; i++)
	{
		switch(HEAP_IDX(asHeapInfo[i].ui32HeapID))
		{
#if defined(SGX_FEATURE_2D_HARDWARE)
			case SGX_2D_HEAP_ID:
			{
				psContext->h2DHeap = asHeapInfo[i].hDevMemHeap;
				psContext->s2DHeapBase = asHeapInfo[i].sDevVAddrBase;
				bValid2DHeap = IMG_TRUE;
				break;
			}
#endif
#if defined(SUPPORT_SGX_GENERAL_MAPPING_HEAP)
			case SGX_GENERAL_MAPPING_HEAP_ID:
			{
				psContext->hGeneralMappingHeap = asHeapInfo[i].hDevMemHeap;
				break;
			}
#endif
			case SGX_GENERAL_HEAP_ID:
			{
#if !defined(SGX_FEATURE_2D_HARDWARE)
				psContext->h2DHeap = asHeapInfo[i].hDevMemHeap;
				psContext->s2DHeapBase = asHeapInfo[i].sDevVAddrBase;
				bValid2DHeap = IMG_TRUE;
#endif
#if !defined(SUPPORT_SGX_GENERAL_MAPPING_HEAP)
				psContext->hGeneralMappingHeap = asHeapInfo[i].hDevMemHeap;
#endif				
				break;
			}

			case SGX_PIXELSHADER_HEAP_ID:
			{
				psContext->hUSEMemHeap = asHeapInfo[i].hDevMemHeap;
				break;
			}
			
			default:
			{
				break;
			}
		}
	}

	if (!bValid2DHeap)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: 2D heap not found"));
		eError = PVR2DERROR_GENERIC_ERROR;
		goto cleanup;
	}

	/*
	 * PVR2D may need to be used on systems with no third party
	 * display driver (e.g. to implement a WSEGL module).
	 * For compatibility with the MBX version of PVR2D, errors
	 * returned by PVRSRVOpenDCDevice are ignored, allowing the
	 * memory allocation/wrapping and blitting services in PVR2D to
	 * be used by client software.
	 */
	 eError = InitDisplayDevice(psContext, ulDevID);
	if (eError != PVR2D_OK && eError != PVR2DERROR_DEVICE_NOT_PRESENT)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: InitDisplayDevice failed"));
		goto cleanup;
	}

#if defined(TRANSFER_QUEUE)	
	/*
		Defer the creation of the transfer context until we need it
	*/
	psContext->hTransferContext = IMG_NULL;
	psContext->ulFlags = ulFlags;
#endif /* TRANSFER_QUEUE */

#if defined(SGX_FEATURE_2D_HARDWARE)

	/*
	 * Presentation blits are not possible if there is no third party
	 * display device.
	 */
	if (psContext->hDisplayClassDevice != 0)
	{
		/* Allocate space for a clip rectangle */
		psContext->psBltClipBlock = (PVR2D_CLIPBLOCK *) PVR2DCalloc(psContext, sizeof(PVR2D_CLIPBLOCK));
		if (psContext->psBltClipBlock)
		{
			psContext->ui32NumClipBlocks = 1;
		}
		else
		{
			/*
			 * Allocation failed - but will attempt realloc later
			 * before it's used (in PVR2DSetPresentBltProperties).
			 * ui32NumClipBlocks should already be zero.
			*/
			PVR_ASSERT(psContext->ui32NumClipBlocks == 0);
		}

		/* No active blocks states: should be zeroed by Calloc */
		PVR_ASSERT(psContext->ui32NumActiveClipBlocks == 0);

		InitPresentBltData(psContext);
	}
#endif /* SGX_FEATURE_2D_HARDWARE */

	if (s_psp2DriverMemBlockUID == SCE_UID_INVALID_UID)
	{
		s_psp2DriverMemBlockUID = sceKernelAllocMemBlock("PVR2DDriverMemBlock", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, 72 * 1024, SCE_NULL);

		if (s_psp2DriverMemBlockUID <= 0)
		{
			eError = PVR2DERROR_MEMORY_UNAVAILABLE;
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: sceKernelAllocMemBlock failed"));
			goto cleanup;
		}

		psContext->bIsExtRegMemblock = IMG_FALSE;
		bValidDriverMemBlock = IMG_TRUE;
	}

	if (PVRSRVRegisterMemBlock(&psContext->sDevData, s_psp2DriverMemBlockUID, &s_psp2ProcRefId, IMG_TRUE))
	{
		eError = PVR2DERROR_MEMORY_UNAVAILABLE;
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: failed to register memblock"));
		goto cleanup;
	}
	
	*phContext = (PVR2DCONTEXTHANDLE)psContext;

	return PVR2D_OK;

cleanup:

#if defined(SGX_FEATURE_2D_HARDWARE) && !defined(PVR2D_ALT_2DHW)
	if(psContext->psBltClipBlock)
	{
		PVR2DFree(psContext, psContext->psBltClipBlock);
	}
#endif

#if defined(TRANSFER_QUEUE)
	if (psContext->hTransferContext)
	{
		eSrvError = SGXDestroyTransferContext(&psContext->sDevData, psContext->hTransferContext, CLEANUP_WITH_POLL);
		if (eSrvError != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: SGXDestroyTransferContext failed"));
		}
	}
#endif

	if (CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(&psContext->sPrimaryBuffer))
	{
		eSrvError = PVRSRVUnmapDeviceClassMemory(&psContext->sDevData,
				CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(&psContext->sPrimaryBuffer));
		if (eSrvError != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: PVRSRVUnmapDeviceClassMemory failed"));
		}
	}
	
	if (psContext->hDisplayClassDevice)
	{
		eSrvError = PVRSRVCloseDCDevice(psContext->psServices,
						psContext->hDisplayClassDevice);
		if (eSrvError != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: PVRSRVCloseDCDevice failed"));
		}
	}

	if (psContext->sMiscInfo.ui32StatePresent)
	{
		eSrvError = PVRSRVReleaseMiscInfo(psContext->psServices, &psContext->sMiscInfo);
		
		if (eSrvError != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: PVRSRVReleaseMiscInfo failed"));
		}
	}
	
	if (bValidDeviceMemContext)
	{
		if (PVRSRVDestroyDeviceMemContext(&psContext->sDevData, psContext->hDevMemContext) != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: PVRSRVDestroyDeviceMemContext failed"));
		}
	}
	
	if (bConnectedToServices)
	{
		if (PVRSRVDisconnect(psContext->psServices) != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DCreateDeviceContext: PVRSRVDisconnect failed"));
		}
	}

	if (bValidDriverMemBlock)
	{
		sceKernelFreeMemBlock(s_psp2DriverMemBlockUID);
	}

	_PVR2DDestroyContext(psContext);

	return eError;
}

/******************************************************************************
 @Function	PVR2DDestroyDeviceContext

 @Input    hContext             : PVR2D Context handle

 @Return   error                : error code

 @Description                   : destroy a PVR2D device context
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DDestroyDeviceContext (PVR2DCONTEXTHANDLE hContext)
{
	PVRSRV_ERROR eSrvError;
	PVR2DERROR eError;
	PVR2DCONTEXT *psContext;
	PVR2D_FLIPCHAIN *psFlipChain, *psCurrent;
	IMG_BOOL bReturnError = IMG_FALSE;
	
	if(!hContext)
		return PVR2D_OK;
	
	psContext = (PVR2DCONTEXT *)hContext;
	
#ifdef PDUMP
	PVRSRVPDumpComment(psContext->psServices, "PVR2D close", IMG_FALSE);
#endif
	
	if (CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(&psContext->sPrimaryBuffer))
	{
		eSrvError = PVRSRVUnmapDeviceClassMemory(&psContext->sDevData,
				CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(&psContext->sPrimaryBuffer));
		if (eSrvError != PVRSRV_OK)
		{
			bReturnError = IMG_TRUE;
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DDestroyDeviceContext: PVRSRVUnmapDeviceClassMemory failed"));
		}
	}

	/* destroy any swapchains left lying around */
	psFlipChain = psContext->psFlipChain;
	while (psFlipChain)
	{
		eError = DestroyFlipChain(psContext, psFlipChain);
		if(eError != PVR2D_OK)
		{
			bReturnError = IMG_TRUE;
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DDestroyDeviceContext: DestroyFlipChain failed"));
			/* Don't exit, carry on and remove any other swap chains */
		}
		psCurrent = psFlipChain;
		psFlipChain = psFlipChain->psNext;
		psCurrent->hContext = IMG_NULL;
		PVR2DFree(psContext, psCurrent);
	}
	
#if defined(TRANSFER_QUEUE)	
	/* Don't free the tranfser context if we didn't create it */
	if (psContext->hTransferContext)
	{
		eSrvError = SGXDestroyTransferContext(&psContext->sDevData, psContext->hTransferContext, CLEANUP_WITH_POLL);
		if (eSrvError != PVRSRV_OK)
		{
			bReturnError = IMG_TRUE;
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DDestroyDeviceContext: SGXDestroyTransferContext failed"));
		}
	}
#endif

	if (s_psp2DriverMemBlockUID > 0)
	{
		eSrvError = PVRSRVUnregisterMemBlock(&psContext->sDevData, s_psp2DriverMemBlockUID);
		if (eSrvError != PVRSRV_OK)
		{
			bReturnError = IMG_TRUE;
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DDestroyDeviceContext: PVRSRVUnregisterMemBlock failed"));
		}
	}

	if (!psContext->bIsExtRegMemblock)
	{
		sceKernelFreeMemBlock(s_psp2DriverMemBlockUID);
		s_psp2DriverMemBlockUID = SCE_UID_INVALID_UID;
	}

	if (psContext->hDisplayClassDevice)
	{
		eSrvError = PVRSRVCloseDCDevice(psContext->psServices,
						psContext->hDisplayClassDevice);
		if (eSrvError != PVRSRV_OK)
		{
			bReturnError = IMG_TRUE;
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DDestroryDeviceContext: PVRSRVCloseDCDevice failed"));
		}
	}

	eSrvError = PVRSRVReleaseMiscInfo(psContext->psServices, &psContext->sMiscInfo);
	if (eSrvError != PVRSRV_OK)
	{
		bReturnError = IMG_TRUE;
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DDestroyDeviceContext: PVRSRVReleaseMiscInfo failed"));
	}

	eSrvError = PVRSRVDestroyDeviceMemContext(&psContext->sDevData, psContext->hDevMemContext);
	if (eSrvError != PVRSRV_OK)
	{
		bReturnError = IMG_TRUE;
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DDestroryDeviceContext: PVRSRVDestroyDeviceMemContext failed"));
	}

	eSrvError = PVRSRVDisconnect(psContext->psServices);
	if (eSrvError != PVRSRV_OK)
	{
		bReturnError = IMG_TRUE;
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DDestroyDeviceContext: PVRSRVDisconnect failed"));
	}

	if(psContext->psBltClipBlock)
	{
		PVR2DFree(psContext, psContext->psBltClipBlock);
	}


	_PVR2DDestroyContext(psContext);

	if (bReturnError)
	{
		return PVR2DERROR_GENERIC_ERROR;
	}
	else
	{
		return PVR2D_OK;
	}
}

#if defined(TRANSFER_QUEUE)			
/******************************************************************************
 @Function	ValidateTransferContext

 @Input     psContext   : PVR2D Context

 @Return	PVR2DERROR : error code

 @Description : Validates the handle to the transfer context in the PVR2D
				context. If there is no context currently, one is created.
				This prevents contexts being created unnecessarily, saving
				space on the (restricted) kernel data heap. PVR2D context
				creation time should also be slightly improved.
				Note that this function should be called from any function
				which utilises psContext->hTransferContext.
******************************************************************************/
PVR2DERROR ValidateTransferContext(PVR2DCONTEXT *psContext)
{
	SGX_TRANSFERCONTEXTCREATE sCreateTransfer;
	PVRSRV_ERROR				eSrvError;

	/* Only create a context if we don't currently have one */
	if (psContext->hTransferContext != IMG_NULL)
	{
		return PVRSRV_OK;
	}

	sCreateTransfer.hDevMemContext = psContext->hDevMemContext;
	sCreateTransfer.hMemBlockProcRef = s_psp2ProcRefId;

	eSrvError = SGXCreateTransferContext(&psContext->sDevData,
										130 * 1024,
										&sCreateTransfer,
										&psContext->hTransferContext);
	if (eSrvError != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "ValidateTransferContext: SGXCreateTransferContext failed"));
		return PVR2DERROR_GENERIC_ERROR;
	}

	return PVR2D_OK;
}
#endif /* TRANSFER_QUEUE */

/******************************************************************************
 @Function	PVR2DGetAPIRev

 @Output	lRevMajor : PVR2D Major revision

 @Output	lRevMinor : PVR2D Minor revision

 @Return	PVR2DERROR : error code

 @Description : Destroy a PVR2D device context
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DGetAPIRev(PVR2D_LONG *lRevMajor, PVR2D_LONG *lRevMinor)
{
	if (lRevMajor && lRevMinor)
	{
		*lRevMajor = PVR2D_REV_MAJOR;
		*lRevMinor = PVR2D_REV_MINOR;
		return PVR2D_OK;
	}
	else
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DGetAPIRev: Invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}
}

/******************************************************************************
 @Function	PVR2DRegisterDriverMemBlockPSP2

 @Input		hMemBlockId : SCE memblock UID

 @Input		hProcRefId : GPU driver per-process reference ID. Pass 0 if first GPU driver allocation

 @Return	PVR2DERROR : error code

 @Description : Register SCE memblock to be used as internal driver memory
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DRegisterDriverMemBlockPSP2(SceUID hMemBlockId, PVR2D_SID hProcRefId)
{
	if (hMemBlockId <= 0)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DRegisterDriverMemBlockPSP2: Invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	s_psp2DriverMemBlockUID = hMemBlockId;
	s_psp2ProcRefId = hProcRefId;
}

/******************************************************************************
 @Function	PVR2DGetProcRefIdPSP2

 @Return	IMG_SID : process reference ID

 @Description : Get internal GPU driver process reference ID for memory allocations
******************************************************************************/
PVR2D_EXPORT
PVR2D_SID PVR2DGetProcRefIdPSP2()
{
	return s_psp2ProcRefId;
}

/******************************************************************************
 End of file (pvr2dinit.c)
******************************************************************************/



