#include <kernel.h>
#include <display.h>

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "eurasia/include4/services.h"
#include "eurasia/include4/pvr_debug.h"
#include "eurasia/include4/sgxapi_km.h"
#include "eurasia/services4/include/servicesint.h"
#include "eurasia/services4/include/pvr_bridge.h"
#include "eurasia/services4/include/sgx_bridge.h"

/*!
*******************************************************************************
 @Function	SGX2DQueryBlitsComplete
 @Description	Queries if blits have finished or wait for completion
 @Input		psDevData :
 @Input		psSyncInfo :
 @Input		bWaitForComplete : Single query or wait
 @Return	PVRSRV_ERROR :
******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV SGX2DQueryBlitsComplete(PVRSRV_DEV_DATA *psDevData,
	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo,
	IMG_BOOL bWaitForComplete)
{
	PVRSRV_BRIDGE_RETURN sBridgeOut;
	PVRSRV_BRIDGE_IN_2DQUERYBLTSCOMPLETE sBridgeIn;

	PVRSRVMemSet(&sBridgeOut, 0x00, sizeof(sBridgeOut));
	PVRSRVMemSet(&sBridgeIn, 0x00, sizeof(sBridgeIn));

	if (!psDevData || !psSyncInfo)
	{
		PVR_DPF((PVR_DBG_ERROR, "SGX2DQueryBlitsComplete: Invalid psMiscInfo"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	sBridgeIn.hDevCookie = psDevData->hDevCookie;
	sBridgeIn.psSyncInfo = psSyncInfo;
	sBridgeIn.bWaitForComplete = bWaitForComplete;

	if (PVRSRVBridgeCall(psDevData->psConnection->hServices,
		PVRSRV_BRIDGE_SGX_2DQUERYBLTSCOMPLETE,
		&sBridgeIn,
		sizeof(sBridgeIn),
		&sBridgeOut,
		sizeof(sBridgeOut)))
	{
		PVR_DPF((PVR_DBG_ERROR, "SGX2DQueryBlitsComplete: BridgeCall failed"));
		return PVRSRV_ERROR_BRIDGE_CALL_FAILED;
	}

	return sBridgeOut.eError;
}

/*!
*******************************************************************************
 @Function	SGXSetContextPriority
 @Description	Sets the scheduling priority for contexts
 @Input		psDevData :
 @Input		ePriority :
 @Input		hRenderContext :
 @Input		hTransferContext :
 @Return	PVRSRV_ERROR :
******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV SGXSetContextPriority(PVRSRV_DEV_DATA *psDevData,
	SGX_CONTEXT_PRIORITY *pePriority,
	IMG_HANDLE hRenderContext,
	IMG_HANDLE hTransferContext)
{
	PVRSRV_BRIDGE_IN_SGX_SET_CONTEXT_PRIORITY sBridgeIn;
	PVRSRV_BRIDGE_OUT_SGX_SET_CONTEXT_PRIORITY sBridgeOut;

	PVR_UNREFERENCED_PARAMETER(hTransferContext);

	if (!psDevData || !pePriority)
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXSetContextPriority: Invalid parameters."));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	PVRSRVMemSet(&sBridgeIn, 0x00, sizeof(sBridgeIn));
	PVRSRVMemSet(&sBridgeOut, 0x00, sizeof(sBridgeOut));

	if (hRenderContext != IMG_NULL)
	{
		sBridgeIn.ePriority = *pePriority;
		sBridgeIn.hRenderContext = hRenderContext;

		if (PVRSRVBridgeCall(psDevData->psConnection->hServices,
			PVRSRV_BRIDGE_SGX_SET_CONTEXT_PRIORITY,
			&sBridgeIn,
			sizeof(sBridgeIn),
			&sBridgeOut,
			sizeof(sBridgeOut)))
		{
			PVR_DPF((PVR_DBG_ERROR, "SGXSetContextPriority: BridgeCall failed"));
			return PVRSRV_ERROR_BRIDGE_CALL_FAILED;
		}

		*pePriority = sBridgeOut.ePriority;
	}

	return sBridgeOut.eError;
}
