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

	PVR_DPF((PVR_DBG_VERBOSE, "SGX2DQueryBlitsComplete"));

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

