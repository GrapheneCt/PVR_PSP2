/*!
******************************************************************************
 @file   sgx_unittest_utils.c

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
$Log: sgx_unittest_utils.c $
******************************************************************************/

#ifdef __psp2__
struct timeval {
	unsigned int tv_sec;     /* seconds */
	unsigned int tv_usec;    /* microseconds */
};

#define gettimeofday sceKernelLibcGettimeofday

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#endif

#include "sgx_unittest_utils.h"
#include "stddef.h"


/*
	Size of the array to fill with device IDs 
*/
#define MAX_NUM_DEVICE_IDS 32

int sgxut_null_dpf(const char *pszFormat, ...)
{
	PVR_UNREFERENCED_PARAMETER(pszFormat);
	return 0;
}


IMG_VOID sgxut_fail_if_error(PVRSRV_ERROR eError)
{
	if (eError != PVRSRV_OK)
	{
		DPF(" FAIL - %s(%u)\n", PVRSRVGetErrorString(eError), eError);
		SGXUT_ERROR_EXIT();
	}
	else
	{
		DPF(" OK\n");
	}
}

IMG_VOID sgxut_fail_if_no_error(PVRSRV_ERROR eError)
{
	if (eError == PVRSRV_OK)
	{
		DPF(" FAIL - expected error code.\n");
		SGXUT_ERROR_EXIT();
	}
	else
	{
		DPF(" OK - %s(%u)\n", PVRSRVGetErrorString(eError), eError);
	}
}

IMG_VOID sgxut_fail_if_no_error_quiet(PVRSRV_ERROR eError)
{
	if (eError == PVRSRV_OK)
	{
		DPF(" FAIL - expected error code.\n");
		SGXUT_ERROR_EXIT();
	}
}

/*
	Define used inside loops - saves printing out lots of OKs:
*/
IMG_VOID sgxut_fail_if_error_quiet(PVRSRV_ERROR eError)
{
	if (eError != PVRSRV_OK)
	{
		DPF(" FAIL - %s(%u)\n", PVRSRVGetErrorString(eError), eError);
		SGXUT_ERROR_EXIT();
	}
}


IMG_UINT32 sgxut_GetBytesPerPixel(PVRSRV_PIXEL_FORMAT eFormat)
{
	IMG_UINT32 ui32BytesPerPixel = 0;
	
	switch (eFormat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		case PVRSRV_PIXEL_FORMAT_RGB555:
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			ui32BytesPerPixel = 2;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_RGB888:
		case PVRSRV_PIXEL_FORMAT_BGR888:
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		case PVRSRV_PIXEL_FORMAT_A2RGB10:
		case PVRSRV_PIXEL_FORMAT_A2BGR10:
		{
			ui32BytesPerPixel = 4;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ABGR16:
		case PVRSRV_PIXEL_FORMAT_ABGR16F:
		{
			ui32BytesPerPixel = 8;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ABGR32F:
		{
			ui32BytesPerPixel = 16;
			break;
		}
		default:
		{
			exit(-1);
			break;
		}
	}
	
	return ui32BytesPerPixel;
}

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
							IMG_CHAR					*acFrameLabel)
{
	PDUMP_PIXEL_FORMAT eDumpFormat;

	if (acFrameLabel == IMG_NULL)
	{
		IMG_CHAR pbyBuffer[128];
		/* create a nice label from the frame number*/
		if (sprintf(pbyBuffer, "fbdump_%d", (int)ui32FrameNum) < 0)
		{
			exit(-1);
		}
		acFrameLabel = pbyBuffer;
	}

	switch(ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		{
			eDumpFormat = PVRSRV_PDUMP_PIXEL_FORMAT_RGB565;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			eDumpFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB1555;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			eDumpFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB4444;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		{
			eDumpFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB8888;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		{
			eDumpFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ABGR8888;
			break;
		}
		default:
		{
			exit(-1);
		}
	}

	PVRSRVPDumpBitmap(psDevData,
						acFrameLabel,
						0,
						ui32Width,
						ui32Height,
						ui32ByteStride,
						psBitmapMemInfo->sDevVAddr,
						hDevMemContext,
						ui32Height * ui32ByteStride,
						eDumpFormat,
						PVRSRV_PDUMP_MEM_FORMAT_STRIDE,
						0);
}
#endif //#if defined(PDUMP)






IMG_VOID sgxut_parse_commandline_option(IMG_CHAR					*arg0,
										IMG_CHAR					*szOption,
										const SGXUT_COMMANDLINE_OPTION	*asCommandLineOptions,
										IMG_UINT32					ui32CommandLineOptionsSize,
										IMG_UINT32					*peOption)
{
	IMG_UINT32	eOption;
	
	if (strcmp(szOption, "-h") == 0)
	{
		DPF("%s options:\n", arg0);
		
		for (eOption = 0;
			 eOption < ui32CommandLineOptionsSize;
			 eOption++)
		{
			if (asCommandLineOptions[eOption].szHelpString != IMG_NULL)
			{
				DPF("%s %s\t%s\n",
					asCommandLineOptions[eOption].szCommandString,
					asCommandLineOptions[eOption].szExtraArgsString,
					asCommandLineOptions[eOption].szHelpString);
			}
		}

		exit(0);
	}
	
	for (eOption = 0;
		 eOption < ui32CommandLineOptionsSize;
		 eOption++)
	{
		if (strcmp(szOption, asCommandLineOptions[eOption].szCommandString) == 0)
		{
			*peOption = asCommandLineOptions[eOption].eOption;
			break;
		}
	}
	
	if (eOption == ui32CommandLineOptionsSize)
	{
		DPF("Unknown option %s\nUse -h for help\n", szOption);
		exit(0);
	}
}


#if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)
/**	@brief	Reads a dword value at a byte-aligned address using the ukernel
 *
 * @param psSGXDevData		- SGX device data
 * @param hDevMemContext	- device mem context
 * @param psDevVAddr		- byte-aligned device virtual address
 * @param pvLinAddr			- CPU virtual address (optional)
 * @param ui32HeapIndex		- heap index of memory heap
 * @return	none
 */
IMG_VOID sgxut_read_devicemem(PVRSRV_DEV_DATA	*psSGXDevData,
							  IMG_HANDLE		 hDevMemContext,
							  IMG_DEV_VIRTADDR	*psDevVAddr,
							  IMG_PVOID			 pvLinAddr)
{
	SGX_MISC_INFO	sSGXMiscInfo;
	PVRSRV_ERROR    eResult;
	IMG_UINT32		ui32DeviceValue;
	IMG_UINT32		ui32CPUValue;

	/*
	 * Query the microkernel to read the memory allocation
	 */
	DPF("Misc Info API: Query the SGX device memory buffer\n");

	sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_MEMREAD;
	sSGXMiscInfo.sDevVAddrSrc = *psDevVAddr;
	sSGXMiscInfo.hDevMemContext = hDevMemContext;

	eResult = SGXGetMiscInfo(psSGXDevData, &sSGXMiscInfo);

	ui32DeviceValue = sSGXMiscInfo.uData.sSGXFeatures.ui32DeviceMemValue;
	DPF("Reading memory (device) at 0x%x: value is %x\n",
			sSGXMiscInfo.sDevVAddrSrc.uiAddr,
			ui32DeviceValue);
	if(pvLinAddr != IMG_NULL)
	{
		ui32CPUValue = *(IMG_UINT32*)(pvLinAddr);
		DPF("Reading memory (CPU)    at 0x%x: value is %x\n",
				(IMG_UINT32)pvLinAddr,
				ui32CPUValue);
		if(ui32DeviceValue != ui32CPUValue)
		{
			DPF("ERROR: Device and CPU values disagree.");
		}
	}
}
#endif


IMG_VOID sgxut_sleep_ms(IMG_UINT32 ui32SleepTimeMS)
{
#ifdef __psp2__
			sceKernelDelayThread(ui32SleepTimeMS * 1000);
#else
			usleep(ui32SleepTimeMS * 1000);
#endif
}


IMG_VOID sgxut_sleep_us(IMG_UINT32 ui32SleepTimeUS)
{
#if defined(LINUX)
	usleep(ui32SleepTimeUS);
#elif defined(__psp2__)
	sceKernelDelayThread(ui32SleepTimeUS);
#else
	PVRSRVWaitus(ui32SleepTimeUS);
#endif
}


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
							   IMG_VOID						*pvCallbackPriv
							   )
{
	PVRSRV_ERROR	eResult;

#if defined(PDUMP)
	PVRSRVPDumpComment(psConnection, "Waiting for buffer sync ops to complete", IMG_FALSE);
#endif

	/*for(;;)
	{
		eResult = PVRSRVSyncOpsFlushToDelta(psConnection, psRenderSurfMemInfo->psClientSyncInfo, 0, IMG_FALSE);

		if (bVerbose)
		{
			if (eResult == PVRSRV_ERROR_RETRY)
			{
				DPF("%s (%u), waiting for sync ops to complete\n",
					sSurfaceName, ui32SurfaceID);
			}
			if (eResult == PVRSRV_OK)
			{
				DPF("%s (%u), sync ops are complete\n",
					sSurfaceName, ui32SurfaceID);
			}
		}

		if (eResult != PVRSRV_ERROR_RETRY)
		{
			break;
		}

		if (pfnCallback != IMG_NULL)
		{
			pfnCallback(pvCallbackPriv);
		}

		if (hOSEventObject != IMG_NULL)
		{
			eResult = PVRSRVEventObjectWait(psConnection, hOSEventObject);
			if (eResult == PVRSRV_ERROR_TIMEOUT)
			{
				DPF("sgxut_wait_for_buffer: PVRSRVEventObjectWait returned PVRSRV_ERROR_TIMEOUT (retrying)\n");
				eResult = PVRSRV_OK;
			}
			sgxut_fail_if_error_quiet(eResult);
		}
		else
		{
			sgxut_sleep_ms(ui32WaitTimeMS);
		}
	}

	sgxut_fail_if_error_quiet(eResult);*/
}


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
							  IMG_BOOL						bVerbose)
{
	PVRSRV_ERROR	eResult;

#if defined(PDUMP)
	PVRSRVPDumpComment(psConnection, "Waiting for buffer sync ops token to complete", IMG_FALSE);
#endif

	/*for(;;)
	{
		eResult = PVRSRVSyncOpsFlushToToken(psConnection,
											#if defined (SUPPORT_SID_INTERFACE)
											psRenderSurfMemInfo->psClientSyncInfo->hKernelSyncInfo,
											#else
											psRenderSurfMemInfo->psClientSyncInfo,
											#endif
											psSyncToken,
											IMG_FALSE); *//* FIXME: lacking services support */

		/*if (bVerbose)
		{
			if (eResult == PVRSRV_ERROR_RETRY)
			{
				DPF("%s (%u), waiting for sync ops token to complete\n",
					sSurfaceName, ui32SurfaceID);
			}
			if (eResult == PVRSRV_OK)
			{
				DPF("%s (%u), sync ops token is complete\n",
					sSurfaceName, ui32SurfaceID);
			}
		}

		if (eResult != PVRSRV_ERROR_RETRY)
		{
			break;
		}
		
		if (hOSEventObject != IMG_NULL)
		{
			eResult = PVRSRVEventObjectWait(psConnection, hOSEventObject);
			if (eResult == PVRSRV_ERROR_TIMEOUT)
			{
				DPF("sgxut_wait_for_token: PVRSRVEventObjectWait returned PVRSRV_ERROR_TIMEOUT (retrying)\n");
				eResult = PVRSRV_OK;
			}
			sgxut_fail_if_error_quiet(eResult);
		}
		else
		{
			sgxut_sleep_ms(ui32WaitTimeMS);
		}
	}
	
	sgxut_fail_if_error_quiet(eResult);*/
}


/* Read current time in milliseconds */
IMG_VOID sgxut_get_current_time(IMG_UINT32 *pui32TimeMS)
{
#if defined(__linux__) || defined(__psp2__)
	struct timeval time;
	gettimeofday(&time, NULL);
	*pui32TimeMS = time.tv_sec * 1000 + time.tv_usec / 1000;
#else
	*pui32TimeMS = 0;
#endif
}


/*!
******************************************************************************

 @Function	sgxut_print_dev_type

 @Description

 Print the device type

 @Input    PVRSRV_DEVICE_TYPE :

 @Return   void  :

******************************************************************************/
IMG_VOID sgxut_print_dev_type(PVRSRV_DEVICE_TYPE eDeviceType)
{
	switch (eDeviceType)
	{
		case PVRSRV_DEVICE_TYPE_UNKNOWN:
		{
			DPF("PVRSRV_DEVICE_ID_UNKNOWN");
			break;
		}
		case PVRSRV_DEVICE_TYPE_SGX:
		{
			DPF("PVRSRV_DEVICE_ID_SGX");
			break;
		}
		case PVRSRV_DEVICE_TYPE_MSVDX:
		{
			DPF("PVRSRV_DEVICE_ID_MSVDX");
			break;
		}
		case PVRSRV_DEVICE_TYPE_EXT:
		{
			DPF("PVRSRV_DEVICE_TYPE_EXT (3rd Party)");
			break;
		}
		default:
		{
			DPF("Not recognised 0x%x", eDeviceType);
			break;
		}
	}
}


/*!
******************************************************************************

 @Function	sgxut_print_sgx_info

 @Description

 Print info about the passed PVRSRV_SGX_CLIENT_INFO structure to stdout

 @Input    *psSGXInfo :

 @Return   void  :

******************************************************************************/
IMG_VOID sgxut_print_sgx_info(PVRSRV_SGX_CLIENT_INFO *psSGXInfo)
{
	/*
		Add anything interesting from the SGX info structure here.
	*/
	DPF(".... ui32ProcessID:%u\n", psSGXInfo->ui32ProcessID);
}


/***********************************************************************************
 Function Name      : sgxut_FloorLog2
 Inputs             : ui32Val
 Outputs            : 
 Returns            : Floor(Log2(ui32Val))
 Description        : Computes the floor of the log base 2 of a unsigned integer - 
					  used mostly for computing log2(2^ui32Val).
***********************************************************************************/
IMG_UINT32 sgxut_FloorLog2(IMG_UINT32 ui32Val)
{
	IMG_UINT32 ui32Ret = 0;

	while (ui32Val >>= 1)
	{
		ui32Ret++;
	}

	return ui32Ret;
}


/***********************************************************************************
 Function Name      : sgxut_SGXInit
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : SGX Common initialisation.
***********************************************************************************/
IMG_VOID
sgxut_SGXInit(IMG_BOOL					bVerbose,
			  PVRSRV_CONNECTION			**ppsConnection,
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
			  PVRSRV_HEAP_INFO			*asHeapInfo)
{
	PVRSRV_ERROR				eResult;
	fnDPF						fnInfo = bVerbose ? DPF : sgxut_null_dpf;
	fnFIE						fnInfoFailIfError = bVerbose ? sgxut_fail_if_error : sgxut_fail_if_error_quiet;
	IMG_UINT32					i;
	IMG_UINT32					uiNumDevices = MAX_NUM_DEVICE_IDS;
	IMG_UINT32					*aui32DeviceID;
	IMG_HANDLE					hDisplayDevice;
	DISPLAY_INFO   				sDisplayInfo;
	DISPLAY_FORMAT				*psDCFormats;
	DISPLAY_DIMS				*psDCDims;
	IMG_UINT32					ui32Count;
	PVRSRV_DEVICE_IDENTIFIER	*psDevID;
	PVRSRV_DEV_DATA				*psDevDataArray;
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID						hDevMemContext;
#else
	IMG_HANDLE					hDevMemContext;
#endif
	IMG_UINT32					ui32SharedHeapCount;

	/**************************************************************************
	*	Connect to PowerVR services
	**************************************************************************/
	fnInfo("Call PVRSRVConnect with a valid argument:\n");
	eResult = PVRSRVConnect(ppsConnection, 0);
	fnInfoFailIfError(eResult);

	/**************************************************************************
	*	Enumerate attached devices
	**************************************************************************/
	fnInfo("Get number of devices from PVRSRVEnumerateDevices:\n");
#if 0
	eResult = PVRSRVEnumerateDevices(*ppsConnection,
									 &uiNumDevices,
									 IMG_NULL);
	fnInfoFailIfError(eResult);
#endif
	/* TODO: Use call to PVRSRVEnumerateDevices to retrieve no. of devices */
	psDevID = malloc(uiNumDevices * sizeof(PVRSRV_DEVICE_IDENTIFIER));
	SGXUT_CHECK_MEM_ALLOC(psDevID);

	eResult = PVRSRVEnumerateDevices(*ppsConnection,
									 &uiNumDevices,
									 psDevID);
	fnInfoFailIfError(eResult);

	fnInfo(".... Reported %u devices\n", (unsigned int) uiNumDevices);

	/* List the devices */
	fnInfo(".... Device Number  | Device Type\n");

	for (i = 0; i < uiNumDevices; i++)
	{
		fnInfo("            %04d    | ", (int)psDevID[i].ui32DeviceIndex);
		if (bVerbose)
		{
			sgxut_print_dev_type(psDevID[i].eDeviceType);
		}
		fnInfo("\n");
	}

	/* TODO: Use call to PVRSRVEnumerateDevices to retrieve no. of devices */
	psDevDataArray = malloc(uiNumDevices * sizeof(PVRSRV_DEV_DATA));
	SGXUT_CHECK_MEM_ALLOC(psDevDataArray);

	/**************************************************************************
	*	Get each device
	**************************************************************************/
	for (i = 0; i < uiNumDevices; i++)
	{
		/*
			Only get services managed devices.
			Display Class API handles external display devices
		 */
		if (psDevID[i].eDeviceType != PVRSRV_DEVICE_TYPE_EXT)
		{
			PVRSRV_DEV_DATA *psDevData = &psDevDataArray[i];

			fnInfo("Attempt to acquire device %d:\n",(unsigned int) psDevID[i].ui32DeviceIndex);

			eResult = PVRSRVAcquireDeviceData ( *ppsConnection,
												psDevID[i].ui32DeviceIndex,
												psDevData,
												PVRSRV_DEVICE_TYPE_UNKNOWN);
			fnInfoFailIfError(eResult);

			/* 
				Print out details about the SGX device.
				At the enumeration stage you should get back the device info
				from which you match a devicetype with index, i.e. we should
				know what index SGX device is and test for it now.
			*/
			if (psDevID[i].eDeviceType == PVRSRV_DEVICE_TYPE_SGX)
			{
				/* save off 3d devdata */
				*psSGXDevData = *psDevData;

				fnInfo("Getting SGX Client info\n");
				memset(psSGXInfo, 0, sizeof(*psSGXInfo));
				eResult = SGXGetClientInfo(psDevData, psSGXInfo);

				fnInfoFailIfError(eResult);

				if (bVerbose)
				{
					sgxut_print_sgx_info(psSGXInfo);
				}
			}
		}
	}

	free(psDevDataArray);
	free(psDevID);

	if(psSGXDevData == IMG_NULL)
	{
		eResult = PVRSRV_ERROR_NO_DEVICEDATA_FOUND;
		fnInfoFailIfError(eResult);		
	}

	/**************************************************************************
	***************************************************************************
	*	Display device setup and initialisation
	***************************************************************************
	**************************************************************************/
	/**************************************************************************
	*	Enumerate display class devices
	**************************************************************************/
	fnInfo("Display Class API: enumerate devices\n");

	/*
	   Count the display devices.
	*/
	eResult = PVRSRVEnumerateDeviceClass(*ppsConnection,
										PVRSRV_DEVICE_CLASS_DISPLAY,
										&ui32Count,
										NULL);
	fnInfoFailIfError(eResult);

	fnInfo("PVRSRVEnumerateDeviceClass() returns %u display device(s)\n", ui32Count);

	if(ui32Count == 0)
	{
		eResult = PVRSRV_ERROR_NO_DC_DEVICES_FOUND;
		fnInfoFailIfError(eResult);
	}

	/**************************************************************************
	*	Get the device ids for the display devices.
	**************************************************************************/
	aui32DeviceID = malloc(sizeof(aui32DeviceID[0]) * ui32Count);
	SGXUT_CHECK_MEM_ALLOC(aui32DeviceID);

	eResult = PVRSRVEnumerateDeviceClass(*ppsConnection,
										 PVRSRV_DEVICE_CLASS_DISPLAY,
										 &ui32Count,
										 aui32DeviceID);
	fnInfoFailIfError(eResult);


	/**************************************************************************
	*	Open Display class devices
	**************************************************************************/
	fnInfo("Display Class API: open device\n");

	/*
	   Pick the first (current) display device.
	*/
	hDisplayDevice = PVRSRVOpenDCDevice(psSGXDevData,
										aui32DeviceID[0]);

	if (hDisplayDevice == NULL)
	{
		eResult = PVRSRV_ERROR_UNABLE_TO_OPEN_DC_DEVICE;
	}

	fnInfoFailIfError(eResult);

	free(aui32DeviceID);

	/**************************************************************************
	*	Get display info
	**************************************************************************/
	fnInfo("Display Class API: Get display info\n");

	eResult = PVRSRVGetDCInfo(hDisplayDevice,
							  &sDisplayInfo);

	fnInfoFailIfError(eResult);

	fnInfo(".... Name:%s\n",					sDisplayInfo.szDisplayName);
	fnInfo(".... MaxSwapChains:%u\n",			sDisplayInfo.ui32MaxSwapChains);
	fnInfo(".... MaxSwapChainBuffers:%u\n",		sDisplayInfo.ui32MaxSwapChainBuffers);
	fnInfo(".... MinSwapInterval:%u\n",			sDisplayInfo.ui32MinSwapInterval);
	fnInfo(".... MaxSwapInterval:%u\n",			sDisplayInfo.ui32MaxSwapInterval);

	fnInfo("Display Class API: enumerate display formats\n");

	/**************************************************************************
	*	Get number of primary pixel formats
	**************************************************************************/
	eResult = PVRSRVEnumDCFormats(hDisplayDevice, &ui32Count, NULL);
	fnInfoFailIfError(eResult);

	psDCFormats = malloc(sizeof(*psDCFormats) * ui32Count);
	SGXUT_CHECK_MEM_ALLOC(psDCFormats);

	/**************************************************************************
	*	Get all primary pixel formats.
	**************************************************************************/
	eResult = PVRSRVEnumDCFormats(hDisplayDevice, &ui32Count, psDCFormats);
	fnInfoFailIfError(eResult);

	for (i = 0; i < ui32Count; i++)
	{
		fnInfo(".... Display format %u - Pixelformat:%u\n", i, psDCFormats[i].pixelformat);
	}

	/**************************************************************************
	*	Get number dimensions for the current pixel format.
	**************************************************************************/
	fnInfo("Display Class API: enumerate display dimensions\n");

	eResult = PVRSRVEnumDCDims(hDisplayDevice, 
							   &ui32Count, 
							   psDCFormats,
							   IMG_NULL);
	fnInfoFailIfError(eResult);

	psDCDims = malloc(sizeof(*psDCDims) * ui32Count);
	SGXUT_CHECK_MEM_ALLOC(psDCDims);


	/**************************************************************************
	*	Get all dimension info for the current pixel format.
	**************************************************************************/
	eResult = PVRSRVEnumDCDims(hDisplayDevice, 
							   &ui32Count, 
							   psDCFormats,
							   psDCDims);
	fnInfoFailIfError(eResult);

	for (i = 0; i < ui32Count; i++)
	{
		fnInfo(".... Display dimensions %u - ByteStride:%u Width:%u Height:%u\n",
			i,
			psDCDims[i].ui32ByteStride,
			psDCDims[i].ui32Width,
			psDCDims[i].ui32Height);
	}

	*psPrimDims = psDCDims[0];
	*psPrimFormat = psDCFormats[0];

	free(psDCFormats);
	free(psDCDims);	

	/**************************************************************************
	***************************************************************************
	*	Memory setup, allocation and initialisation
	***************************************************************************
	**************************************************************************/
	/**************************************************************************
	*	Create Device mem context
	**************************************************************************/
	fnInfo("Attempt to create memory context for SGX:\n");
	eResult = PVRSRVCreateDeviceMemContext(psSGXDevData,
										   &hDevMemContext,
										   &ui32SharedHeapCount,
										   asHeapInfo);

	fnInfoFailIfError(eResult);

	for (i = 0; i < ui32SharedHeapCount; i++)
	{
		fnInfo(".... Shared heap %u - HeapID:0x%x DevVAddr:0x%x "
			"Size:0x%x Attr:0x%x\n",
			i,
			asHeapInfo[i].ui32HeapID,
			asHeapInfo[i].sDevVAddrBase.uiAddr,
			asHeapInfo[i].ui32HeapByteSize,
			asHeapInfo[i].ui32Attribs);
	}

	*phDisplayDevice = hDisplayDevice;
	*phDevMemContext = hDevMemContext;
	*pui32SharedHeapCount = ui32SharedHeapCount;
}


/***********************************************************************************
 Function Name      : sgxut_SGXDeInit
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : SGX Common de-initialisation.
***********************************************************************************/
IMG_VOID
sgxut_SGXDeInit(IMG_BOOL				bVerbose,
				PVRSRV_CONNECTION		*psConnection,
				PVRSRV_DEV_DATA			*psSGXDevData,
				PVRSRV_SGX_CLIENT_INFO	*psSGXInfo,
				IMG_HANDLE				hDisplayDevice,
#if defined (SUPPORT_SID_INTERFACE)
				IMG_SID					hDevMemContext)
#else
				IMG_HANDLE				hDevMemContext)
#endif
{
	PVRSRV_ERROR	eResult;
	fnDPF			fnInfo = bVerbose ? DPF : sgxut_null_dpf;
	fnFIE			fnInfoFailIfError = bVerbose ? sgxut_fail_if_error : sgxut_fail_if_error_quiet;

	/**************************************************************************
	* Destroy device mem context
	**************************************************************************/
	fnInfo("Destroy Device Memory Context\n");
	PVRSRVDestroyDeviceMemContext(psSGXDevData, hDevMemContext);

	/**************************************************************************
	* Close Display device
	**************************************************************************/
	fnInfo("Display Class API: close the device\n");
	eResult = PVRSRVCloseDCDevice(psConnection, hDisplayDevice);
	fnInfoFailIfError(eResult);

	/**************************************************************************
	* Release device info
	**************************************************************************/
	fnInfo("Release SGX Client Info:\n");
	eResult = SGXReleaseClientInfo(psSGXDevData, psSGXInfo);
	fnInfoFailIfError(eResult);

	/**************************************************************************
	* Disconnect from services
	**************************************************************************/
	fnInfo("Disconnect from services:\n");
	eResult = PVRSRVDisconnect(psConnection);
	fnInfoFailIfError(eResult);
}


/******************************************************************************
 End of file (sgx_unittest_utils.c)
******************************************************************************/
