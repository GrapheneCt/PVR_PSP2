#include <kernel.h>

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"
#include "pvr_debug.h"

#include "eurasia/include4/services.h"

/*!
*******************************************************************************
 @Function	PollForValue
 @Description	Polls for a value to match a masked read of sysmem
 @Input		psConnection : Services connection
 @Input		hOSEvent : Handle to OS event object to wait for
 @Input		ui32Value : required value
 @Input		ui32Mask : Mask
 @Input		ui32Waitus : wait between tries (us)
 @Input		ui32Tries : number of tries
 @Return	PVRSRV_ERROR :
******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR PVRSRVPollForValue(const PVRSRV_CONNECTION *psConnection,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID    hOSEvent,
#else
	IMG_HANDLE hOSEvent,
#endif
	volatile IMG_UINT32* pui32LinMemAddr,
	IMG_UINT32 ui32Value,
	IMG_UINT32 ui32Mask,
	IMG_UINT32 ui32Waitus,
	IMG_UINT32 ui32Tries)
{
	IMG_UINT32 uiStart;

	while ((*pui32LinMemAddr & ui32Mask) != ui32Value)
	{
		if (!ui32Tries)
		{
			return PVRSRV_ERROR_TIMEOUT_POLLING_FOR_VALUE;
		}

		if (sceGpuSignalWait(sceKernelGetTLSAddr(0x44), ui32Waitus) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_MESSAGE, "PVRSRVPollForValue: PVRSRVEventObjectWait failed"));
			ui32Tries--;
		}
	}

	return PVRSRV_OK;
}


/*!
 ******************************************************************************
 @Function		PVRSRVGetErrorString
 @Description	Returns a text string relating to the PVRSRV_ERROR enum.
 @Note		case statement used rather than an indexed arrary to ensure text is
			synchronised with the correct enum
 @Input		eError : PVRSRV_ERROR enum
 @Return	const IMG_CHAR * : Text string
 @Note		Must be kept in sync with servicesext.h
******************************************************************************/

IMG_EXPORT
const IMG_CHAR *PVRSRVGetErrorString(PVRSRV_ERROR eError)
{
	/* PRQA S 5087 1 */ /* include file needed here */
#include "eurasia/services4/include/pvrsrv_errors.h"
}