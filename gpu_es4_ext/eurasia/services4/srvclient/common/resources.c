#include <kernel.h>

#include "psp2_pvr_desc.h"

#include "eurasia/include4/services.h"

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