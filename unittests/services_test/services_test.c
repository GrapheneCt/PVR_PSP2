/*!
******************************************************************************
 @file   services_test.c

 @brief  Test PowerVR SGX services

 @Author PowerVR

 @date   24/07/2006

         <b>Copyright 2003-2010 by Imagination Technologies Limited.</b>\n
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
		Simple test harness for services interface

 <b>Platform:</b>\n
		Generic

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: services_test.c $
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
			DPF(" FAIL - %s\n",PVRSRVGetErrorString(val)); \
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
#define CHECK_MEM_ALLOC(val) if (val==0) {DPF(" Test Memory Allocation FAILED\n");}

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
******************************************************************************

 @Function	main

 @Description	Test program for services functionality.

******************************************************************************/
#if !defined(LINUX) && !defined(__psp2__)
int __cdecl main (int argc, char **argv)
#else
int main(int argc, char ** argv)
#endif
{
	PVRSRV_ERROR             eResult;
	PVRSRV_CONNECTION*       psConnection;
	IMG_UINT32               uiNumDevices;
	PVRSRV_DEVICE_IDENTIFIER asDevID[MAX_NUM_DEVICE_IDS];
	PVRSRV_DEV_DATA          asDevData[MAX_NUM_DEVICE_IDS];
	PVRSRV_DEV_DATA         *ps3DDevData = IMG_NULL;
	IMG_UINT32               i;
	PVRSRV_SGX_CLIENT_INFO   sSGXInfo;
	int                      loop = 0;
	int                      frameStop = 1;
	int						 exit_point = 0;
	PVRSRV_MISC_INFO		 sMiscInfo;

#ifdef __linux__
	int c;
#endif

#ifdef __psp2__
	SceUID pbMemUID;
	SceUID driverMemUID;
	SceUID rtMemUID;
	IMG_SID hRtMem = 0;
	IMG_SID hPbMem = 0;
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

	SGX_CREATERENDERCONTEXT sCreateRenderContext;
	SGX_ADDRENDTARG sAddRTInfo = {0};
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID    hDevMemContext;
#else
	IMG_HANDLE hDevMemContext;
#endif
	IMG_HANDLE hRenderContext;
	PVRSRV_CLIENT_MEM_INFO *psVisTestResultMemInfo;
	IMG_HANDLE hRTDataSet;
	IMG_UINT32 ui32SharedHeapCount;
	PVRSRV_HEAP_INFO asHeapInfo[PVRSRV_MAX_CLIENT_HEAPS];

	/* may want to define a structure to hang this lot off */
	IMG_HANDLE		hDisplayDevice = IMG_NULL;
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
	if(argc >= 3)
	{
		/*
			Specifies a point to exit (for testing resman).
			Omit or pass zero to run complete test.
		*/
		exit_point = atol(argv[2]);
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

	DPF(".... Reported %u devices\n", (unsigned int) uiNumDevices);

	/* List the devices */
	DPF(".... Device Number  | Device Type\n");

	for (i = 0; i < uiNumDevices; i++)
	{
		DPF("            %04d    | ", (int)asDevID[i].ui32DeviceIndex);
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

			DPF("Attempt to acquire device %d:\n",(unsigned int) asDevID[i].ui32DeviceIndex);

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

	CHECK_MEM_ALLOC(psPrimFormat);

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
#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
	DPF("Display Class API: map display surface to SGX\n");

	eResult = PVRSRVMapDeviceClassMemory(ps3DDevData,
										 hDevMemContext,
										 hSystemBuffer,
										 &psSGXSystemBufferMemInfo);

	FAIL_IF_ERROR(eResult);
#endif
	DPF("Attempt to create rendering context for SGX:\n");

#ifdef __psp2__

	driverMemUID = sceKernelAllocMemBlock("SGXDriver", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, 0x100000, NULL);

	PVRSRVMapMemoryToGpuByUID(ps3DDevData, driverMemUID, &hPbMem, 0);

	pbMemUID = sceKernelAllocMemBlock("SGXParamBuffer", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, 4 * 1024 * 1024, NULL);

	PVRSRVMapMemoryToGpuByUID(ps3DDevData, pbMemUID, &hPbMem, 1);

	sCreateRenderContext.ui32Flags = 2 | 4;
	sCreateRenderContext.ui32PBSize = 4 * 1024 * 1024 - 0x80000;
	sCreateRenderContext.ui32PBSizeLimit = sCreateRenderContext.ui32PBSize;
	sCreateRenderContext.hPbMem = hPbMem;
	sCreateRenderContext.bPerContextPB = IMG_TRUE;

#else

	sCreateRenderContext.ui32Flags = 0;
	sCreateRenderContext.ui32PBSize = 4 * 1024 * 1024;
	sCreateRenderContext.ui32PBSizeLimit = 0;

#endif

	sCreateRenderContext.hDevCookie = ps3DDevData->hDevCookie;
	sCreateRenderContext.hDevMemContext = hDevMemContext;
	sCreateRenderContext.ui32MaxSACount = 128;

	/*
	   We don't need to test the visibility buffer, but don't pass 0 since it
	   might confuse the memory manager in srvkm.
	*/
	sCreateRenderContext.ui32VisTestResultBufferSize = 1;

	eResult = SGXCreateRenderContext(ps3DDevData,
		&sCreateRenderContext,
		&hRenderContext,
		&psVisTestResultMemInfo);

	FAIL_IF_ERROR(eResult);

	DPF("Attempt to add render target for SGX:\n");

	sAddRTInfo.ui32Flags = 0;
	sAddRTInfo.hRenderContext = hRenderContext;
	sAddRTInfo.hDevCookie = ps3DDevData->hDevCookie;
	sAddRTInfo.ui32NumPixelsX = 320;
	sAddRTInfo.ui32NumPixelsY = 240;
	sAddRTInfo.ui16MSAASamplesInX	= 1;
	sAddRTInfo.ui16MSAASamplesInY	= 1;
	sAddRTInfo.eForceScalingInX		= SGX_SCALING_NONE;
	sAddRTInfo.eForceScalingInY		= SGX_SCALING_NONE;
	sAddRTInfo.ui32BGObjUCoord = 0;
	sAddRTInfo.ui16NumRTsInArray = 1;

#ifdef __psp2__

	rtMemUID = sceKernelAllocMemBlock("SGXRenderTarget", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, 0x100000, NULL);

	PVRSRVMapMemoryToGpuByUID(ps3DDevData, rtMemUID, &hRtMem, 1);

	sAddRTInfo.eRotation = PVRSRV_ROTATE_0;
	sAddRTInfo.ui32RendersPerFrame = 1;
	sAddRTInfo.ui32RendersPerQueueSwap = 3;
	sAddRTInfo.ui8MacrotileCountX = 0;
	sAddRTInfo.ui8MacrotileCountY = 0;
	sAddRTInfo.i32DataMemblockUID = rtMemUID;
	sAddRTInfo.bUseExternalUID = IMG_FALSE;
	sAddRTInfo.hDataMemory = hRtMem;
	sAddRTInfo.ui32MultisampleLocations = 0;

#endif

	eResult = SGXAddRenderTarget(ps3DDevData,
								&sAddRTInfo,
								&hRTDataSet);
	FAIL_IF_ERROR(eResult);
	CHECK_EXIT_POINT(1, exit_point);

	/* Memstats request is not supported by kernel driver on PSP2 */
#ifndef __psp2__

	/* Gather some mem stats and DDK version */
	DPF("Gather some mem stats:\n");
	sMiscInfo.ui32StateRequest = PVRSRV_MISC_INFO_MEMSTATS_PRESENT;
	sMiscInfo.pszMemoryStr = PVRSRVAllocUserModeMem(10*4096);
	sMiscInfo.ui32MemoryStrLen = 10*4096;
	DPF("mem: 0x%X\n", sMiscInfo.pszMemoryStr);

	eResult = PVRSRVGetMiscInfo(psConnection, &sMiscInfo);
	FAIL_IF_ERROR(eResult);

	/* print memory stats */
	DPF("%s", sMiscInfo.pszMemoryStr);
	PVRSRVFreeUserModeMem(sMiscInfo.pszMemoryStr);

	/* lightweight mem stats: print free mem only */
	DPF("Gather lightweight mem stats:\n");
	sMiscInfo.ui32StateRequest = PVRSRV_MISC_INFO_FREEMEM_PRESENT;
	sMiscInfo.pszMemoryStr = PVRSRVAllocUserModeMem(10*4096);
	sMiscInfo.ui32MemoryStrLen = 10*4096;

	eResult = PVRSRVGetMiscInfo(psConnection, &sMiscInfo);
	FAIL_IF_ERROR(eResult);
	
	/* print free mem stats */
	DPF("%s", sMiscInfo.pszMemoryStr);
	PVRSRVFreeUserModeMem(sMiscInfo.pszMemoryStr);

#endif

	/* Buffer for DDK version string */
	sMiscInfo.ui32StateRequest = PVRSRV_MISC_INFO_DDKVERSION_PRESENT;
	sMiscInfo.pszMemoryStr = PVRSRVAllocUserModeMem(256);
	sMiscInfo.ui32MemoryStrLen = 256;
	eResult = PVRSRVGetMiscInfo(psConnection, &sMiscInfo);
	FAIL_IF_ERROR(eResult);

	/* print DDK version */
	DPF("DDK version:\n");
	DPF("%d.%d.%d.%d\n",
			sMiscInfo.aui32DDKVersion[0],
			sMiscInfo.aui32DDKVersion[1],
			sMiscInfo.aui32DDKVersion[2],
			sMiscInfo.aui32DDKVersion[3]);
	DPF("DDK version string: %s\n", sMiscInfo.pszMemoryStr);
	PVRSRVFreeUserModeMem(sMiscInfo.pszMemoryStr);

	/*
		Final Cleanup.
	*/
	DPF("Attempt to remove render target:\n");
	eResult = SGXRemoveRenderTarget(ps3DDevData,
									hRenderContext,
									hRTDataSet);
	FAIL_IF_ERROR(eResult);

#ifdef __psp2__

	PVRSRVUnmapMemoryFromGpuByUID(ps3DDevData, rtMemUID);
	sceKernelFreeMemBlock(rtMemUID);

#endif

	DPF("Attempt to destroy render context:\n");
	eResult = SGXDestroyRenderContext(ps3DDevData,
									  hRenderContext,
									  psVisTestResultMemInfo,
									  CLEANUP_WITH_POLL);
	FAIL_IF_ERROR(eResult);

#ifdef __psp2__

	PVRSRVUnmapMemoryFromGpuByUID(ps3DDevData, driverMemUID);
	sceKernelFreeMemBlock(driverMemUID);

	PVRSRVUnmapMemoryFromGpuByUID(ps3DDevData, pbMemUID);
	sceKernelFreeMemBlock(pbMemUID);

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
 End of file (services_test.c)
******************************************************************************/
