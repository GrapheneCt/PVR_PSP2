#include <kernel.h>
#include <display.h>

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "eurasia/include4/services.h"
#include "eurasia/include4/pvr_debug.h"
#include "eurasia/include4/sgxapi_km.h"
#include "eurasia/services4/include/servicesint.h"
#include "eurasia/services4/include/pvr_bridge.h"

IMG_RESULT PVRSRVBridgeCall(IMG_HANDLE hServices,
	IMG_UINT32 ui32FunctionID,
	IMG_VOID *pvParamIn,
	IMG_UINT32 ui32InBufferSize,
	IMG_VOID *pvParamOut,
	IMG_UINT32	ui32OutBufferSize)
{
	PVRSRV_ERROR eError = PVRSRV_OK;
	PVRSRV_BRIDGE_PACKAGE dispatchPacket;

	IMG_UINT32 ptr = hServices;
	void **ptr2 = (void **)(ptr + 4);

	dispatchPacket.hKernelServices = *ptr2;
	dispatchPacket.pvParamOut = pvParamOut;
	dispatchPacket.ui32OutBufferSize = ui32OutBufferSize;
	dispatchPacket.ui32Size = sizeof(PVRSRV_BRIDGE_PACKAGE);
	dispatchPacket.ui32BridgeID = ui32FunctionID;
	dispatchPacket.pvParamIn = pvParamIn;
	dispatchPacket.ui32InBufferSize = ui32InBufferSize;
	while (eError = PVRSRV_BridgeDispatchKM(ui32FunctionID, &dispatchPacket), eError == PVRSRV_ERROR_RETRY) {
		sceKernelDelayThread(201);
	}

	return eError;
}

/*!
 ******************************************************************************
 @Function	PVRSRVGetDeviceMemHeapInfo
 @Descriptiongets heap info
 @Input		psDevData : device and  ioctl connection info
 @Input		hDevMemContext
 @Output	pui32ClientHeapCount
 @Output	psHeapInfo
 @Return	PVRSRV_ERROR
 ******************************************************************************/
IMG_INTERNAL
PVRSRV_ERROR IMG_CALLCONV PVRSRVGetDeviceMemHeapInfo(const PVRSRV_DEV_DATA *psDevData,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID     hDevMemContext,
#else
	IMG_HANDLE hDevMemContext,
#endif
	IMG_UINT32 *pui32ClientHeapCount,
	PVRSRV_HEAP_INFO *psHeapInfo)
{
	PVRSRV_BRIDGE_IN_GET_DEVMEM_HEAPINFO sIn;
	PVRSRV_BRIDGE_OUT_GET_DEVMEM_HEAPINFO sOut;
	PVRSRV_ERROR eError = PVRSRV_OK;
	IMG_UINT32 i;

	PVRSRVMemSet(&sIn, 0x00, sizeof(sIn));
	PVRSRVMemSet(&sOut, 0x00, sizeof(sOut));


	if (!psDevData
		|| !hDevMemContext
		|| !pui32ClientHeapCount
		|| !psHeapInfo)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVGetDeviceMemHeapInfo: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	sIn.hDevCookie = psDevData->hDevCookie;
	sIn.hDevMemContext = hDevMemContext;

	if (PVRSRVBridgeCall(psDevData->psConnection->hServices,
		PVRSRV_BRIDGE_GET_DEVMEM_HEAPINFO,
		&sIn,
		sizeof(PVRSRV_BRIDGE_IN_GET_DEVMEM_HEAPINFO),
		&sOut,
		sizeof(PVRSRV_BRIDGE_OUT_GET_DEVMEM_HEAPINFO)))
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVGetDeviceMemHeapInfo: BridgeCall failed"));
		return PVRSRV_ERROR_BRIDGE_CALL_FAILED;
	}

	if (sOut.eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVGetDeviceMemHeapInfo: Error %d returned", sOut.eError));
		return sOut.eError;
	}

	/* the shared heap information */
	*pui32ClientHeapCount = sOut.ui32ClientHeapCount;
	for (i = 0; i < sOut.ui32ClientHeapCount; i++)
	{
		psHeapInfo[i] = sOut.sHeapInfo[i];
	}

	return eError;
}

/*!
 ******************************************************************************
 @Function	PVRSRVMapDeviceClassMemory
 @Description
 @Input		psDevData
 @Input		hDevMemContext
 @Input		hDeviceClassBuffer
 @Input		psMemInfo
 @Return	PVRSRV_ERROR
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVMapDeviceClassMemory(const PVRSRV_DEV_DATA *psDevData,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID    hDevMemContext,
	IMG_SID    hDeviceClassBuffer,
#else
	IMG_HANDLE hDevMemContext,
	IMG_HANDLE hDeviceClassBuffer,
#endif
	PVRSRV_CLIENT_MEM_INFO **ppsMemInfo)
{
	PVRSRV_CLIENT_MEM_INFO *psMemInfoIn = IMG_NULL;
	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = IMG_NULL;
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!psDevData || !ppsMemInfo || !hDeviceClassBuffer)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVMapDeviceClassMemory: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psMemInfoIn = (PVRSRV_CLIENT_MEM_INFO *)hDeviceClassBuffer;

	eError = PVRSRVMapMemoryToGpu(
		psDevData,
		hDevMemContext,
		0,
		psMemInfoIn->uAllocSize,
		0,
		psMemInfoIn->pvLinAddr,
		PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR,
		IMG_NULL);

	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVMapDeviceClassMemory: Failed to map memory to GPU"));
		return PVRSRV_ERROR_FAILED_TO_MAP_PAGE_TABLE;
	}

	eError = PVRSRVAllocSyncInfo(psDevData, &psSyncInfo);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVMapDeviceClassMemory: Failed to alloc sync info"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psMemInfoIn->psClientSyncInfo = psSyncInfo;
	*ppsMemInfo = psMemInfoIn;

	return PVRSRV_OK;
}

/*!
 ******************************************************************************
 @Function	PVRSRVUnmapDeviceClassMemory
 @Description
 @Input		psDevData
 @Input		psMemInfo
 @Return	PVRSRV_ERROR
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR PVRSRVUnmapDeviceClassMemory(const PVRSRV_DEV_DATA *psDevData,
	PVRSRV_CLIENT_MEM_INFO *psMemInfo)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	if (!psDevData || !psMemInfo)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVUnmapDeviceClassMemory: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	eError = PVRSRVUnmapMemoryFromGpu(
		psDevData,
		psMemInfo->pvLinAddr,
		0,
		IMG_NULL);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVUnmapDeviceClassMemory: Failed to unmap memory from GPU"));
		return PVRSRV_ERROR_FAILED_TO_MAP_PAGE_TABLE;
	}

	eError = PVRSRVFreeSyncInfo(psDevData, psMemInfo->psClientSyncInfo);
	if (eError != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVMapDeviceClassMemory: Failed to free sync info"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	return PVRSRV_OK;
}
