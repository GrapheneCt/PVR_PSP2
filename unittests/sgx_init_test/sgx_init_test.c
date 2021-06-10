/*!
******************************************************************************
 @file   sgx_init_test.c

 @brief  Test PowerVR SGX services init

 @Author PowerVR

 @date   24/11/2007

         <b>Copyright 2007-2010 by Imagination Technologies Limited.</b>\n
         All rights reserved.  No part of this software, either
         material or conceptual may be copied or distributed,
         transmitted, transcribed, stored in a retrieval system
         or translated into any human or computer language in any
         form by any means, electronic, mechanical, manual or
         other-wise, or disclosed to third parties without the
         express written permission of Imagination Technologies
         Limited, Unit 8, HomePark Industrial Estate,
         King's Langley, Hertfordshire, WD4 8LZ, U.K.

 <b>Description:</b>\n
		Simple test for services initialisation

 <b>Platform:</b>\n
		Generic

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: sgx_init_test.c $
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __psp2__
#include <kernel.h>

SCE_USER_MODULE_LIST("app0:gpu_es4_ext.suprx");

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#endif

#include "img_defs.h"
#include "sgxdefs.h"
#include "services.h"

#include "sgxapi.h"
#ifdef __linux__
#include <unistd.h>
#endif

#define INTERNAL_TEST
#include "sgx_options.h"

#undef DPF
#define DPF printf

#ifndef EXIT_FAILURE
#define EXIT_FAILURE              (1)
#endif

#define FAIL_IF_NO_ERROR(val)					\
		if (val == PVRSRV_OK)					\
				{DPF(" FAIL\n"); exit(-1);}		\
		else {DPF(" OK\n");}

#define FAIL_IF_ERROR(val)								\
		if (val!=PVRSRV_OK)								\
		{												\
			DPF(" FAIL - %s\n",PVRSRVGetErrorString(val));	\
			exit(-1);									\
		}												\
		else {DPF(" OK\n");}


/*
	Define used inside loops - saves printing out lots of OKs:
*/
#define FAIL_IF_ERROR2(val)	if (val!=PVRSRV_OK) {DPF(" FAIL - %s\n", PVRSRVGetErrorString(val));	exit(-1);}

/*
	Define used to check allocs in test program, doesn't exit to verify stability.
*/
#define CHECK_MEM_ALLOC(val) if (val==0) {DPF(" Test Memory Allocation FAILED\n");exit(-1);}

#define CHECK_EXIT_POINT(this_exit_point, exit_point_selected)				\
	if (this_exit_point == exit_point_selected)								\
	{																		\
		/* Test resman by exiting while still holding various resources. */	\
		DPF("Reached exit point %u. Exiting.", this_exit_point);			\
		exit(0);															\
	}

/* Size of the array to fill with device IDs */
#define MAX_NUM_DEVICE_IDS 32


/*!
******************************************************************************

 @Function	print_dev_type

 @Description

 Print the device type

 @Input    PVRSRV_DEVICE_TYPE :

 @Return   void  :

******************************************************************************/
static void print_dev_type(PVRSRV_DEVICE_TYPE eDeviceType)
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

 @Function	print_sgx_info

 @Description

 Print info about the passed PVRSRV_SGX_CLIENT_INFO structure to stdout

 @Input    *psSGXInfo :

 @Return   void  :

******************************************************************************/
static void print_sgx_info(PVRSRV_SGX_CLIENT_INFO *psSGXInfo)
{
	/*
		Add anything interesting from the SGX info structure here.
	*/
	DPF(".... ui32ProcessID:%u\n", psSGXInfo->ui32ProcessID);
}

/*!
 ************************************************

 @function	decode_dword

 @description	Converts a 32-bit uint to an array of 4 bytes (least significant byte first)

 @Input		IMG_UINT32

 @Output	char array
 ************************************************
 */
static IMG_VOID SGXUT_decode_dword(IMG_UINT32 ui32In, IMG_CHAR *acOut)
{
	IMG_INT i;
	IMG_UINT32 ui32Mask = 0xFFU, ui32Temp;

	for (i=0; i<4; i++)
	{
		ui32Temp = ui32In & ui32Mask;
		ui32Temp >>= (8*i);
		acOut[i] = (IMG_CHAR) ui32Temp;
		ui32Mask <<= 8;
	}
}

/*!
 ************************************************

 @function SGXUT_decode_build_options

 @description	Describe the build options returned from microkernel
				 (SGXGetMiscInfo) query

 @input		IMG_UINT32 : build options
 @input		IMG_UINT32 : mask

 @output	none
 ************************************************
 */
static IMG_VOID SGXUT_decode_build_option(IMG_UINT32 ui32In, IMG_UINT32 ui32Mask)
{
	if ( (ui32In & ui32Mask) != 0)
	{
		DPF("enabled\n");
	}
	else
	{
		DPF("disabled\n");
	}
}

#if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)
/*!
******************************************************************************

 @Function SGXUT_readmem

 @Description	Read memory from ukernel

 @Input		*psSGXDevData : device data for SGX-type device
 @Input		asHeapInfo : heap allocation info
 @Input		ui32SharedHeapCount : heap count (includes per-context heaps)
 @Input		hDevMemContext : device memory context of this app

 @Return	void
******************************************************************************/
static IMG_VOID SGXUT_readmem(PVRSRV_DEV_DATA *psSGXDevData,
							  PVRSRV_HEAP_INFO	*asHeapInfo,
							  IMG_UINT32		ui32SharedHeapCount,
							  IMG_HANDLE		hDevMemContext)
{
	SGX_MISC_INFO			sSGXMiscInfo;
	PVRSRV_ERROR            eResult;

	PVRSRV_CLIENT_MEM_INFO  *psDeviceMemInfo;
	PVRSRV_CLIENT_MEM_INFO  *psDeviceDestMemInfo;
	IMG_DEV_VIRTADDR		sDevVAddr;
	IMG_PVOID				pvLinAddr;

	IMG_UINT32				ui32ByteOffset = 1;
	IMG_UINT32				ui32InitMemValue = 0x1234, ui32DeviceValue;
	IMG_HANDLE				hVertexShaderHeap, hKernelDataHeap;
	IMG_UINT				i;

	/* Find the kernel-video data heap */

	for(i=0; i < ui32SharedHeapCount; i++)
	{
		switch(HEAP_IDX(asHeapInfo[i].ui32HeapID))
		{
			case SGX_VERTEXSHADER_HEAP_ID:
			{
				hVertexShaderHeap		= asHeapInfo[i].hDevMemHeap;
				break;
			}
			case SGX_KERNEL_DATA_HEAP_ID:
			{
				hKernelDataHeap			= asHeapInfo[i].hDevMemHeap;
			}
			default:
			{
				break;
			}
		}
	}

	/*
	 * Allocate device-addressable memory and check its value from
	 * within the microkernel
	 */
	PVRSRVAllocDeviceMem(psSGXDevData,
			/* hKernelDataHeap, */
			hVertexShaderHeap,
			PVRSRV_MEM_READ | PVRSRV_MEM_WRITE,
			16, 4,
			&psDeviceMemInfo);

	/* Apply offset */
	sDevVAddr = psDeviceMemInfo->sDevVAddr;
	pvLinAddr = psDeviceMemInfo->pvLinAddr;
	sDevVAddr.uiAddr += ui32ByteOffset;
	pvLinAddr = (IMG_PVOID)((IMG_CHAR*)pvLinAddr + ui32ByteOffset);

	/* Initialise value in device-addressable memory
	 */
	DPF("Writing memory (CPU)    at 0x%x: value is %x\n",
			(IMG_UINTPTR_T)pvLinAddr,
			ui32InitMemValue);
	PVRSRVMemCopy(pvLinAddr, (IMG_PVOID)&ui32InitMemValue, sizeof(IMG_UINT32));

	/* Configure the address to read and its context in the microkernel */
	DPF("\nMisc Info API: Query the SGX device memory buffer\n");
	sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_MEMREAD;
	sSGXMiscInfo.sDevVAddrSrc = sDevVAddr;
	sSGXMiscInfo.hDevMemContext = hDevMemContext;

	eResult = SGXGetMiscInfo(psSGXDevData, &sSGXMiscInfo);

	ui32DeviceValue = sSGXMiscInfo.uData.sSGXFeatures.ui32DeviceMemValue;
	DPF("Reading memory (device) at 0x%x: value is %x\n",
			sSGXMiscInfo.sDevVAddrSrc.uiAddr,
			ui32DeviceValue);
	if(ui32DeviceValue != ui32InitMemValue)
	{
		DPF("ERROR: Device and CPU values disagree.\n\n");
	}

	{
		/* Allocate dest buffer for GPU memcopy */
		PVRSRVAllocDeviceMem(psSGXDevData,
				hKernelDataHeap,
				/* hVertexShaderHeap, */
				PVRSRV_MEM_WRITE,
				16, 4,
				&psDeviceDestMemInfo);
	
		/* Configure the mem copy */
		DPF("\nMisc Info API: Perform a device-->device 4-byte memcopy\n");
		sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_MEMCOPY;
		sSGXMiscInfo.sDevVAddrSrc = sDevVAddr;
		sSGXMiscInfo.sDevVAddrDest = psDeviceDestMemInfo->sDevVAddr;
		
		eResult = SGXGetMiscInfo(psSGXDevData, &sSGXMiscInfo);
	
		ui32DeviceValue = *(IMG_UINT32*)psDeviceDestMemInfo->pvLinAddr;
		DPF("Reading memory (device) at 0x%x: value is %x\n",
				sSGXMiscInfo.sDevVAddrDest.uiAddr,
				ui32DeviceValue);
		if(ui32DeviceValue != ui32InitMemValue)
		{
			DPF("ERROR: Device src and dest values disagree.\n\n");
		}
	}
	
	/* Free allocations */
	PVRSRVFreeDeviceMem(psSGXDevData, psDeviceMemInfo);
	PVRSRVFreeDeviceMem(psSGXDevData, psDeviceDestMemInfo);
}
#endif

/*!
******************************************************************************

 @Function SGXUT_query_sgx_corerev

 @Description	Get the core revision and DDK build from the ukernel

 @Input		*psSGXDevData : device data for SGX-type device
 @Return	void
******************************************************************************/
static IMG_VOID SGXUT_query_sgx_corerev(PVRSRV_DEV_DATA *psSGXDevData)
{
	SGX_MISC_INFO			sSGXMiscInfo;
	PVRSRV_ERROR            eResult;
	IMG_UINT32				ui32CoreRev, ui32CoreId, ui32CoreIdSW, ui32CoreRevSW;
	IMG_UINT32				ui32DDKVersion, ui32DDKBuild, ui32BuildOptions;
	IMG_CHAR				acValues[4], acSGXCoreNamedId[16];

	/*
	 * Get the host driver DDK version (c.f. ukernel DDK version below)
	 */
	DPF("Misc Info API: Query the SGX features from host driver\n");

	sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_DRIVER_SGXREV;
	eResult = SGXGetMiscInfo(psSGXDevData, &sSGXMiscInfo);
	FAIL_IF_ERROR(eResult);

	ui32DDKVersion = sSGXMiscInfo.uData.sSGXFeatures.ui32DDKVersion;
	ui32DDKBuild = sSGXMiscInfo.uData.sSGXFeatures.ui32DDKBuild;
	SGXUT_decode_dword(ui32DDKVersion, acValues);
	DPF(".... SGX Host driver DDK version: %d.%d.%d.%d\n",
		acValues[2], acValues[1], acValues[0], ui32DDKBuild);
#if defined(PVRSRV_USSE_EDM_STATUS_DEBUG)
	DPF(".... SGX Microkernel status buffer location: CPUVAddr: 0x%08X, DevVAddr: 0x%08X\n",
			(IMG_UINTPTR_T)sSGXMiscInfo.uData.sSGXFeatures.pvEDMStatusBuffer,
			sSGXMiscInfo.uData.sSGXFeatures.sDevVAEDMStatusBuffer.uiAddr);
#endif
	/*
	 * Query: request SGX core revision, DDK version and build options
	 * from the microkernel
	 */
	DPF("Misc Info API: Query the SGX features from microkernel\n");

	sSGXMiscInfo.eRequest = SGX_MISC_INFO_REQUEST_SGXREV;

	/* Submit the query */
	eResult = SGXGetMiscInfo(psSGXDevData, &sSGXMiscInfo);
	FAIL_IF_ERROR(eResult);

	ui32CoreRev = sSGXMiscInfo.uData.sSGXFeatures.ui32CoreRev;
	ui32CoreId = sSGXMiscInfo.uData.sSGXFeatures.ui32CoreID >> 16;
	ui32DDKVersion = sSGXMiscInfo.uData.sSGXFeatures.ui32DDKVersion;
	ui32DDKBuild = sSGXMiscInfo.uData.sSGXFeatures.ui32DDKBuild;
	ui32CoreIdSW = sSGXMiscInfo.uData.sSGXFeatures.ui32CoreIdSW;
	ui32CoreRevSW = sSGXMiscInfo.uData.sSGXFeatures.ui32CoreRevSW;
	ui32BuildOptions = sSGXMiscInfo.uData.sSGXFeatures.ui32BuildOptions;

	/* Decode and print output for HW data (from registers) */
	SGXUT_decode_dword(ui32CoreRev, acValues);
	DPF(".... Hardware core designer: %d, HW core revision: %d.%d.%d\n",
		acValues[3], acValues[2], acValues[1], acValues[0]);

	/* Identify SGX product from core ID */
	switch (ui32CoreId)
	{
	case 0x0112U:	sprintf(acSGXCoreNamedId, "SGX 520/530");
					break;
	case 0x0113U:	sprintf(acSGXCoreNamedId, "SGX 535");
					break;
	case 0x0114U:	sprintf(acSGXCoreNamedId, "SGX 540");
					break;
	case 0x0115U:	sprintf(acSGXCoreNamedId, "SGX 545");
					break;
	case 0x0116U:	sprintf(acSGXCoreNamedId, "SGX 531");
					break;
	case 0x0118U:	sprintf(acSGXCoreNamedId, "SGX 54x-MP");
					break;
	default:		strcpy(acSGXCoreNamedId, "");
	}

	if( strcmp(acSGXCoreNamedId, "") != 0)
	{
		DPF(".... Hardware core ID: %x, name: %s\n", ui32CoreId, acSGXCoreNamedId);
	}
	else
	{
		DPF(".... Hardware core ID: %x\n", ui32CoreId);
	}

	/* Decode and print output for software data (from SGX microkernel) */
	SGXUT_decode_dword(ui32DDKVersion, acValues);
	DPF(".... SGX microkernel DDK version: %d.%d.%d.%d\n",
		acValues[2], acValues[1], acValues[0], ui32DDKBuild);

	DPF(".... SGX microkernel software core ID: SGX %d, revision: %x\n",
			ui32CoreIdSW, ui32CoreRevSW);

	/* Commonly used debug options (from ukernel) */
	DPF("SGX microkernel build options\n");
	DPF(".... DEBUG: ");
	SGXUT_decode_build_option(ui32BuildOptions, DEBUG_SET_OFFSET);
	DPF(".... PDUMP: ");
	SGXUT_decode_build_option(ui32BuildOptions, PDUMP_SET_OFFSET);
	DPF(".... PVRSRV_USSE_EDM_STATUS_DEBUG: ");
	SGXUT_decode_build_option(ui32BuildOptions, PVRSRV_USSE_EDM_STATUS_DEBUG_SET_OFFSET);
	DPF(".... SUPPORT_HW_RECOVERY: ");
	SGXUT_decode_build_option(ui32BuildOptions, SUPPORT_HW_RECOVERY_SET_OFFSET);

	/* Other options in no particular order */
	DPF(".... PVR_SECURE_HANDLES: ");
	SGXUT_decode_build_option(ui32BuildOptions, PVR_SECURE_HANDLES_SET_OFFSET);
	DPF(".... SGX_BYPASS_SYSTEM_CACHE: ");
	SGXUT_decode_build_option(ui32BuildOptions, SGX_BYPASS_SYSTEM_CACHE_SET_OFFSET);
	DPF(".... SGX_DMS_AGE_ENABLE: ");
	SGXUT_decode_build_option(ui32BuildOptions, SGX_DMS_AGE_ENABLE_SET_OFFSET);

	DPF(".... SGX_FAST_DPM_INIT: ");
	SGXUT_decode_build_option(ui32BuildOptions, SGX_FAST_DPM_INIT_SET_OFFSET);
	DPF(".... SGX_FEATURE_WRITEBACK_DCU: ");
	SGXUT_decode_build_option(ui32BuildOptions, SGX_FEATURE_DCU_SET_OFFSET);
	DPF(".... SGX_FEATURE_MP: ");
	SGXUT_decode_build_option(ui32BuildOptions, SGX_FEATURE_MP_SET_OFFSET);
	DPF(".... SGX_FEATURE_MP_CORE_COUNT: ");
	DPF("%d\n", ((ui32BuildOptions >> SGX_FEATURE_MP_CORE_COUNT_SET_OFFSET) &
			SGX_FEATURE_MP_CORE_COUNT_SET_MASK) +1);

	DPF(".... SGX_FEATURE_MULTITHREADED_UKERNEL: ");
	SGXUT_decode_build_option(ui32BuildOptions, SGX_FEATURE_MULTITHREADED_UKERNEL_SET_OFFSET);
	DPF(".... SGX_FEATURE_OVERLAPPED_SPM: ");
	SGXUT_decode_build_option(ui32BuildOptions, SGX_FEATURE_OVERLAPPED_SPM_SET_OFFSET);
	DPF(".... SGX_FEATURE_SYSTEM_CACHE: ");
	SGXUT_decode_build_option(ui32BuildOptions, SGX_FEATURE_SYSTEM_CACHE_SET_OFFSET);

	DPF(".... SGX_SUPPORT_HWPROFILING: ");
	SGXUT_decode_build_option(ui32BuildOptions, SGX_SUPPORT_HWPROFILING_SET_OFFSET);
	DPF(".... SUPPORT_ACTIVE_POWER_MANAGEMENT: ");
	SGXUT_decode_build_option(ui32BuildOptions, SUPPORT_ACTIVE_POWER_MANAGEMENT_SET_OFFSET);
	DPF(".... SUPPORT_DISPLAYCONTROLLER_TILING: ");
	SGXUT_decode_build_option(ui32BuildOptions, SUPPORT_DISPLAYCONTROLLER_TILING_SET_OFFSET);
	DPF(".... SUPPORT_PERCONTEXT_PB: ");
	SGXUT_decode_build_option(ui32BuildOptions, SUPPORT_PERCONTEXT_PB_SET_OFFSET);

	DPF(".... SUPPORT_SGX_HWPERF: ");
	SGXUT_decode_build_option(ui32BuildOptions, SUPPORT_SGX_HWPERF_SET_OFFSET);
	DPF(".... SUPPORT_SGX_MMU_DUMMY_PAGE: ");
	SGXUT_decode_build_option(ui32BuildOptions, SUPPORT_SGX_MMU_DUMMY_PAGE_SET_OFFSET);
	DPF(".... SUPPORT_SGX_PRIORITY_SCHEDULING: ");
	SGXUT_decode_build_option(ui32BuildOptions, SUPPORT_SGX_PRIORITY_SCHEDULING_SET_OFFSET);
	DPF(".... USE_SUPPORT_NO_TA3D_OVERLAP: ");
	SGXUT_decode_build_option(ui32BuildOptions, USE_SUPPORT_NO_TA3D_OVERLAP_SET_OFFSET);
}

 /*!
******************************************************************************

 @Function	main
 
 @Description	Test program for services functionality.
 
******************************************************************************/
/* PRQA S 5013 5 */ /* using basic types for declaration*/
#if !defined(LINUX) && !defined(__psp2__)
int __cdecl main (int argc, char **argv)
#else
int main(int argc, char ** argv)
#endif
{
	PVRSRV_ERROR             eResult;
	PVRSRV_CONNECTION        *psConnection;
	IMG_UINT32               uiNumDevices;
	PVRSRV_DEVICE_IDENTIFIER asDevID[MAX_NUM_DEVICE_IDS];
	PVRSRV_DEV_DATA          asDevData[MAX_NUM_DEVICE_IDS];
	PVRSRV_DEV_DATA         *ps3DDevData = IMG_NULL;
	IMG_UINT32               i;
	PVRSRV_SGX_CLIENT_INFO   sSGXInfo;
	IMG_INT                  loop = 0;
	IMG_INT                  frameStop = 1;
#ifdef __linux__
	IMG_INT                  c;
#endif

	/* Display class API */
	IMG_UINT32		ui32Count;
	IMG_UINT32		*pui32DeviceID;
	DISPLAY_INFO    sDisplayInfo;
	DISPLAY_FORMAT	*psPrimFormat;
	DISPLAY_DIMS	*psPrimDims;
#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
	PVRSRV_CLIENT_MEM_INFO *psSGXSystemBufferMemInfo;
#endif
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID    hDevMemContext;
#else
	IMG_HANDLE hDevMemContext;
#endif
	IMG_UINT32 ui32SharedHeapCount;
	PVRSRV_HEAP_INFO asHeapInfo[PVRSRV_MAX_CLIENT_HEAPS];

	/* may want to define a structure to hang this lot off */
	IMG_HANDLE		hDisplayDevice;
#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID			hSystemBuffer = 0;
#else
	IMG_HANDLE		hSystemBuffer = IMG_NULL;
#endif
#endif

#ifdef __linux__
#define OPTS "q"
	while ((c = getopt (argc, argv, OPTS)) != -1)
	{
		switch (c)
		{
			case 'q':
			{
				break;
			}
			default:
			{
                DPF ("Illegal option %c.\n"
	                    "Valid options are "OPTS" (quick)\n",
	                    c);

				exit (EXIT_FAILURE);
			}
		}
	}
#else
	if(argc >= 2)
	{
		frameStop = atol(argv[1]);
	}
#endif

start_again:
	uiNumDevices = 10;

	DPF("----------------------- Start -----------------------\n");

	DPF("Try calling PVRSRVConnect with an invalid argument:\n");

	eResult = PVRSRVConnect(NULL, 0);

	FAIL_IF_NO_ERROR(eResult);

	DPF("Call PVRSRVConnect with a valid argument:\n");

	eResult = PVRSRVConnect(&psConnection, 0);

	FAIL_IF_ERROR(eResult);

	DPF("Try calling PVRSRVEnumerateDevices with invalid puiNumDevices:\n");

	eResult = PVRSRVEnumerateDevices(psConnection,
									NULL,
									NULL);
	FAIL_IF_NO_ERROR(eResult);

	DPF("Get number of devices from PVRSRVEnumerateDevices:\n");

	eResult = PVRSRVEnumerateDevices(psConnection,
									&uiNumDevices,
									asDevID);
	FAIL_IF_ERROR(eResult);

	DPF(".... Reported %u devices\n", (IMG_UINT) uiNumDevices);

	/* List the devices */
	DPF(".... Device Number  | Device Type\n");

	for (i = 0; i < uiNumDevices; i++)
	{
		DPF("            %04d    | ", (IMG_INT)asDevID[i].ui32DeviceIndex);
		print_dev_type(asDevID[i].eDeviceType);
		DPF("\n");
	}

	/* Get each device... */
	for (i = 0; i < uiNumDevices; i++)
	{
		/* 
			Only get services managed devices.
			Display Class API handles external display devices
		 */
		if (asDevID[i].eDeviceType != PVRSRV_DEVICE_TYPE_EXT)
		{
			PVRSRV_DEV_DATA *psDevData = asDevData + i;

			DPF("Attempt to acquire device %d:\n",(IMG_UINT) asDevID[i].ui32DeviceIndex);

			eResult = PVRSRVAcquireDeviceData ( psConnection,
												asDevID[i].ui32DeviceIndex,
												psDevData,
												PVRSRV_DEVICE_TYPE_UNKNOWN);
			FAIL_IF_ERROR(eResult);

			/* 
				Print out details about the SGX device.
				At the enumeration stage you should get back the device info
				from which you match a devicetype with index, i.e. we should
				know what index SGX device is and test for it now.
			*/
			if (asDevID[i].eDeviceType == PVRSRV_DEVICE_TYPE_SGX)
			{	
				/* save off 3d devdata */
				ps3DDevData = psDevData;
				
				DPF("Getting SGX Client info\n");
				eResult = SGXGetClientInfo(psDevData, &sSGXInfo);

				FAIL_IF_ERROR(eResult);
		
				print_sgx_info(&sSGXInfo);
			}
		}
	}

	if(ps3DDevData == IMG_NULL)
	{
		eResult = PVRSRV_ERROR_NO_DEVICEDATA_FOUND;
		/* PRQA S 3201,3355,3358 1 */ /* ignore warning about unreachable code */
		FAIL_IF_ERROR(eResult);		
	}

	DPF("Display Class API: enumerate devices\n");

	/*
	   Count the display devices.
	*/
	eResult = PVRSRVEnumerateDeviceClass(psConnection,
										PVRSRV_DEVICE_CLASS_DISPLAY,
										&ui32Count,
										NULL);
	FAIL_IF_ERROR(eResult);

	DPF("PVRSRVEnumerateDeviceClass() returns %u display device(s)\n", ui32Count);

	if(ui32Count == 0)
	{
		eResult = PVRSRV_ERROR_NO_DC_DEVICES_FOUND;
		/* PRQA S 3201,3355,3358 1 */ /* ignore warning about unreachable code */
		FAIL_IF_ERROR(eResult);
	}

	/*
	   Get the device ids for the devices.
	*/
	pui32DeviceID = malloc(sizeof(*pui32DeviceID) * ui32Count);

	CHECK_MEM_ALLOC(pui32DeviceID);

	eResult = PVRSRVEnumerateDeviceClass(psConnection,
								PVRSRV_DEVICE_CLASS_DISPLAY,
								&ui32Count,
								pui32DeviceID);
	FAIL_IF_ERROR(eResult);

	DPF("Attempt to create memory context for SGX:\n");
	eResult = PVRSRVCreateDeviceMemContext(ps3DDevData,
										   &hDevMemContext,
										   &ui32SharedHeapCount,
										   asHeapInfo);
	FAIL_IF_ERROR(eResult);

	DPF("Display Class API: open device\n");

	/*
	   Pick the first (current) display device.
	*/
	hDisplayDevice = PVRSRVOpenDCDevice(ps3DDevData,
										*pui32DeviceID);

	if (hDisplayDevice == NULL)
	{
		eResult = PVRSRV_ERROR_UNABLE_TO_OPEN_DC_DEVICE;
	}

	FAIL_IF_ERROR(eResult);

	free(pui32DeviceID);

	DPF("Display Class API: Get display info\n");

	eResult = PVRSRVGetDCInfo(hDisplayDevice,
							  &sDisplayInfo);

	FAIL_IF_ERROR(eResult);

	DPF(".... Name:%s\n", sDisplayInfo.szDisplayName);
	DPF(".... MaxSwapChains:%u\n", sDisplayInfo.ui32MaxSwapChains);
	DPF(".... MaxSwapChainBuffers:%u\n", sDisplayInfo.ui32MaxSwapChainBuffers);
	DPF(".... MinSwapInterval:%u\n", sDisplayInfo.ui32MinSwapInterval);
	DPF(".... MaxSwapInterval:%u\n", sDisplayInfo.ui32MaxSwapInterval);
	
	DPF("Display Class API: enumerate display formats\n");

	/*
	   Get number of primary pixel formats.
	*/
	eResult = PVRSRVEnumDCFormats(hDisplayDevice, 
								  &ui32Count,
								  NULL);
	FAIL_IF_ERROR(eResult);

	psPrimFormat = malloc(sizeof(*psPrimFormat) * ui32Count);

	CHECK_MEM_ALLOC(psPrimFormat);

	/*
	   Get all primary pixel formats.
	*/
	eResult = PVRSRVEnumDCFormats(hDisplayDevice, 
								  &ui32Count, 
								  psPrimFormat);
	FAIL_IF_ERROR(eResult);

	for (i = 0; i < ui32Count; i++)
	{
		DPF(".... Display format %u - Pixelformat:%u\n", i, psPrimFormat[i].pixelformat);
	}

	DPF("Display Class API: enumerate display dimensions\n");

	/*
	   Get number dimensions for the current pixel format.
	*/
	eResult = PVRSRVEnumDCDims(hDisplayDevice, 
							   &ui32Count, 
							   psPrimFormat,
							   NULL);
	FAIL_IF_ERROR(eResult);

	psPrimDims = malloc(sizeof(*psPrimDims) * ui32Count);

	CHECK_MEM_ALLOC(psPrimDims);

	/*
	   Get all dimension info for the current pixel format.
	*/
	eResult = PVRSRVEnumDCDims(hDisplayDevice, 
							   &ui32Count, 
							   psPrimFormat,
							   psPrimDims);
	FAIL_IF_ERROR(eResult);

	for (i = 0; i < ui32Count; i++)
	{
		DPF(".... Display dimensions %u - ByteStride:%u Width:%u Height:%u\n",
			i,
			psPrimDims[i].ui32ByteStride,
			psPrimDims[i].ui32Width,
			psPrimDims[i].ui32Height);
	}

	free(psPrimFormat);
	free(psPrimDims);

#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
	DPF("Display Class API: get the system (primary) buffer\n");

	/*
	   Get a handle to the primary surface in the system.
	*/
	eResult = PVRSRVGetDCSystemBuffer(hDisplayDevice, &hSystemBuffer);

	FAIL_IF_ERROR(eResult);
#endif

	for (i = 0; i < ui32SharedHeapCount; i++)
	{
		DPF(".... Shared heap %u - HeapID:0x%x DevVAddr:0x%x "
			"Size:0x%x Attr:0x%x\n",
			i,
			asHeapInfo[i].ui32HeapID,
			asHeapInfo[i].sDevVAddrBase.uiAddr,
			asHeapInfo[i].ui32HeapByteSize,
			asHeapInfo[i].ui32Attribs);
	}

	DPF("Display Class API: map display surface to SGX\n");

#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
	eResult = PVRSRVMapDeviceClassMemory(ps3DDevData,
										 hDevMemContext,
										 hSystemBuffer,
										 &psSGXSystemBufferMemInfo);

	FAIL_IF_ERROR(eResult);
#endif
	SGXUT_query_sgx_corerev(ps3DDevData);
#if defined(SUPPORT_SGX_EDM_MEMORY_DEBUG)
	SGXUT_readmem(ps3DDevData, asHeapInfo, ui32SharedHeapCount, hDevMemContext);
#endif

#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
	DPF("Display Class API: unmap display surface from SGX\n");

	eResult = PVRSRVUnmapDeviceClassMemory(ps3DDevData,
										   psSGXSystemBufferMemInfo);
	FAIL_IF_ERROR(eResult);
#endif

	DPF("Attempt to destroy memory context for SGX:\n");
	eResult = PVRSRVDestroyDeviceMemContext(ps3DDevData,
										   hDevMemContext);		   
	FAIL_IF_ERROR(eResult);

	DPF("Display Class API: close the device\n");
	eResult = PVRSRVCloseDCDevice(psConnection, hDisplayDevice);

	FAIL_IF_ERROR(eResult);

	for (i = 0; i < uiNumDevices; i++)
	{
		if (asDevID[i].eDeviceType != PVRSRV_DEVICE_TYPE_EXT)
		{
			PVRSRV_DEV_DATA *psDevData = asDevData + i;

			if (asDevID[i].eDeviceType == PVRSRV_DEVICE_TYPE_SGX)
			{	
				DPF("SGXReleaseClientInfo:\n");

				eResult = SGXReleaseClientInfo(psDevData, &sSGXInfo);

				FAIL_IF_ERROR(eResult);
			}
		}
	}

	DPF("PVRSRVDisconnect:\n");

	eResult = PVRSRVDisconnect(psConnection);

	FAIL_IF_ERROR(eResult);

	DPF("---------------------End loop %d---------------------\n", ++loop);

	if (loop < frameStop)
	{
		goto start_again;
	}
	
	return 0;
}

/******************************************************************************
 End of file (sgx_init_test.c)
******************************************************************************/
