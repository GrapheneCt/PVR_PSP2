/*!
******************************************************************************
 @file   sgx_unittest_utils.h

 @brief  Common utilities for SGX unit tests

 @Author PowerVR

 @date   5 Nov 2008

         <b>Copyright 2008- by Imagination Technologies Limited.</b>\n
         All rights reserved.  No part of this software, either material or
         conceptual may be copied or distributed, transmitted, transcribed,
         stored in a retrieval system or translated into any human or computer
         language in any form by any means, electronic, mechanical, manual or
         otherwise, or disclosed to third parties without the express written
         permission of Imagination Technologies Limited, Home Park Estate,
         King's Langley, Hertfordshire, WD4 8LZ, U.K.

 @Description:
         Common utilities for SGX unit tests.

 @Platform:
         Generic
******************************************************************************/

/******************************************************************************
Modifications :-
$Log: sgx_unittest_utils.h $
******************************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


#include "img_defs.h"

#include "sgxdefs.h"
#include "services.h"
#include "pvr_debug.h"

#include "sgxapi.h"
#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#endif

#include "sgxmmu.h"

typedef int (*fnDPF)(const char *, ...) IMG_FORMAT_PRINTF(1, 2);
int sgxut_null_dpf(const char *pszFormat, ...) IMG_FORMAT_PRINTF(1, 2);

#undef DPF
#define DPF printf

#define SGXUT_ERROR_EXIT() IMG_ABORT()

/* Fail-if-error function prototype */
typedef IMG_VOID (*fnFIE)(PVRSRV_ERROR eError);
IMG_VOID sgxut_fail_if_error(PVRSRV_ERROR eError);
IMG_VOID sgxut_fail_if_no_error(PVRSRV_ERROR eError);
IMG_VOID sgxut_fail_if_no_error_quiet(PVRSRV_ERROR eError);
IMG_VOID sgxut_fail_if_error_quiet(PVRSRV_ERROR eError);

/*
	Define used to check allocs in test program, doesn't exit to verify stability.
*/
#define SGXUT_CHECK_MEM_ALLOC(val)					\
	if (val==0)										\
	{												\
		DPF(" Test Memory Allocation FAILED\n");	\
		SGXUT_ERROR_EXIT();							\
	}


#define MIN(a,b)	((a)<(b)?(a):(b))
#define MAX(a,b)	((a)>(b)?(a):(b))
#define ABS(a)		MAX((a), -(a))

#define SGXUT_FLOAT_TO_LONG(x)	(* (long *)( & x)) 

#define SGXUT_ALIGNFIELD(V, S)   (((V) + ((1 << (S)) - 1)) >> (S))

#ifndef M_PI
#define M_PI	3.14159265358979323846
#endif

/*
	Size of the array to fill with device IDs 
*/
#define MAX_NUM_DEVICE_IDS 32



typedef struct
{
	IMG_CHAR	*szCommandString;
	IMG_UINT32	eOption;
	IMG_CHAR	*szExtraArgsString;
	IMG_CHAR	*szHelpString;
} SGXUT_COMMANDLINE_OPTION;


IMG_VOID sgxut_parse_commandline_option(IMG_CHAR						*arg0,
										IMG_CHAR						*szOption,
									 	const SGXUT_COMMANDLINE_OPTION	*asCommandLineOptions,
										IMG_UINT32						ui32CommandLineOptionsSize,
										IMG_UINT32						*peOption);
#if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)
IMG_VOID sgxut_read_devicemem(PVRSRV_DEV_DATA	*psSGXDevData,
							  IMG_HANDLE		 hDevMemContext,
							  IMG_DEV_VIRTADDR	*psDevVAddr,
							  IMG_PVOID			 pvLinAddr);
#endif
IMG_VOID sgxut_sleep_ms(IMG_UINT32 ui32SleepTimeMS);
IMG_VOID sgxut_sleep_us(IMG_UINT32 ui32SleepTimeUS);
IMG_VOID sgxut_wait_for_buffer(IMG_CONST PVRSRV_CONNECTION	*psConnection,
							   PVRSRV_CLIENT_MEM_INFO		*psRenderSurfMemInfo,
							   IMG_UINT32					ui32WaitTimeMS,
							   IMG_CHAR						*sSurfaceName,
							   IMG_UINT32					ui32SurfaceID,
#if defined (SUPPORT_SID_INTERFACE)
							   IMG_UINT32					hOSEventObject,
#else
							   IMG_HANDLE					hOSEventObject,
#endif
							   IMG_BOOL						bVerbose,
							   IMG_VOID						(*pfnCallback)(IMG_VOID *),
							   IMG_VOID						*pvCallbackPriv);
IMG_VOID sgxut_wait_for_token(IMG_CONST PVRSRV_CONNECTION	*psConnection,
							  PVRSRV_CLIENT_MEM_INFO		*psRenderSurfMemInfo,
							  const PVRSRV_SYNC_TOKEN		*psSyncToken,
							  IMG_UINT32					ui32WaitTimeMS,
							  IMG_CHAR						*sSurfaceName,
							  IMG_UINT32					ui32SurfaceID,
#if defined (SUPPORT_SID_INTERFACE)
							  IMG_UINT32					hOSEventObject,
#else
							  IMG_HANDLE					hOSEventObject,
#endif
							  IMG_BOOL						bVerbose);

IMG_VOID sgxut_get_current_time(IMG_UINT32 *pui32TimeMS);
IMG_VOID sgxut_print_dev_type(PVRSRV_DEVICE_TYPE eDeviceType);
IMG_VOID sgxut_print_sgx_info(PVRSRV_SGX_CLIENT_INFO *psSGXInfo);
IMG_UINT32 sgxut_FloorLog2(IMG_UINT32 ui32Val);

IMG_UINT32 sgxut_GetBytesPerPixel(PVRSRV_PIXEL_FORMAT eFormat);



IMG_VOID sgxut_SGXInit(IMG_BOOL					bVerbose,
					   PVRSRV_CONNECTION		**ppsConnection,
					   PVRSRV_DEV_DATA			*psSGXDevData,
					   PVRSRV_SGX_CLIENT_INFO	*psSGXInfo,
					   IMG_HANDLE				*phDisplayDevice,
					   DISPLAY_FORMAT			*psPrimFormat,
					   DISPLAY_DIMS				*psPrimDims,
#if defined (SUPPORT_SID_INTERFACE)
					   IMG_SID					*phDevMemContext,
#else
					   IMG_HANDLE				*phDevMemContext,
#endif
					   IMG_UINT32				*pui32SharedHeapCount,
					   PVRSRV_HEAP_INFO			*asHeapInfo);

IMG_VOID sgxut_SGXDeInit(IMG_BOOL				bVerbose,
						 PVRSRV_CONNECTION		*psConnection,
						 PVRSRV_DEV_DATA		*psSGXDevData,
						 PVRSRV_SGX_CLIENT_INFO	*psSGXInfo,
						 IMG_HANDLE				hDisplayDevice,
#if defined (SUPPORT_SID_INTERFACE)
						 IMG_SID				hDevMemContext);
#else
						 IMG_HANDLE				hDevMemContext);
#endif

#if defined(PDUMP)
IMG_VOID sgxut_PDumpBitmap(IMG_CONST PVRSRV_DEV_DATA	*psDevData,
							PVRSRV_CLIENT_MEM_INFO		*psBitmapMemInfo,
#if defined (SUPPORT_SID_INTERFACE)
							IMG_SID						hDevMemContext,
#else
							IMG_HANDLE					hDevMemContext,
#endif
							PVRSRV_PIXEL_FORMAT			ePixelFormat,	
							IMG_UINT32					ui32Width,
							IMG_UINT32					ui32Height,
							IMG_UINT32					ui32ByteStride,
							IMG_UINT32					ui32FrameNum,
							IMG_CHAR					*acFrameLabel);
#endif

/******************************************************************************
 End of file (sgx_unittest_utils.h)
******************************************************************************/
