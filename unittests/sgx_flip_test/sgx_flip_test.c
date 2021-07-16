/*!
******************************************************************************
 @file   sgx_flip_test.c

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
$Log: sgx_flip_test.c $
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
#include "pvr_debug.h"

#include "sgxapi.h"
#if defined(__linux__)
#include <unistd.h>
#endif

#undef DPF
#define DPF printf

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

/*
	Size of the array to fill with device IDs 
*/
#define MAX_NUM_DEVICE_IDS 32

/* This the total maximum number of buffers, i.e. backbuffers + primary surface. */
#ifdef __psp2__
#define MAX_SWAP_CHAIN_BUFFERS 4
#else
#define MAX_SWAP_CHAIN_BUFFERS 3
#endif

/* This value should remain constant - or at least it should be greater than
or equal to MAX_SWAP_CHAIN_BUFFERS. */
#ifdef __psp2__
#define NUM_COLOURS 4
#else
#define NUM_COLOURS 3
#endif

#if (MAX_SWAP_CHAIN_BUFFERS > NUM_COLOURS)
#error NUM_COLOURS should be greater than or equal to MAX_SWAP_CHAIN_BUFFERS
#endif


#define SYNCINFO_READOPS_COMPLETE(SYNCINFO)	   ((SYNCINFO)->psSyncData->ui32ReadOpsComplete)
#define SYNCINFO_READOPS_PENDING(SYNCINFO)    ((SYNCINFO)->psSyncData->ui32ReadOpsPending)

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
	PVRSRV_CONNECTION        *psConnection;
	IMG_UINT32               uiNumDevices;
	PVRSRV_DEVICE_IDENTIFIER asDevID[MAX_NUM_DEVICE_IDS];
	PVRSRV_DEV_DATA          asDevData[MAX_NUM_DEVICE_IDS];
	PVRSRV_DEV_DATA         *ps3DDevData = IMG_NULL;
	IMG_UINT32               i;
	IMG_INT32				 j;
	PVRSRV_SGX_CLIENT_INFO   sSGXInfo;
	IMG_UINT32				ui32Count;
	IMG_UINT32				*pui32DeviceID;
	DISPLAY_INFO   			sDisplayInfo;
	DISPLAY_FORMAT			*psPrimFormat;
	DISPLAY_DIMS			*psPrimDims;
#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
	PVRSRV_CLIENT_MEM_INFO	*psSGXSystemBufferMemInfo;
#endif
	PVRSRV_CLIENT_MEM_INFO	*apsBackBufferMemInfo[MAX_SWAP_CHAIN_BUFFERS];
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID					hSwapChain;
	IMG_SID					hDevMemContext;
#else
	IMG_HANDLE				hSwapChain;
	IMG_HANDLE				hDevMemContext;
#endif
	IMG_UINT32				ui32SharedHeapCount;
	PVRSRV_HEAP_INFO		asHeapInfo[PVRSRV_MAX_CLIENT_HEAPS];
	DISPLAY_SURF_ATTRIBUTES	sDstSurfAttrib;
	DISPLAY_SURF_ATTRIBUTES	sSrcSurfAttrib;
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID					ahBuffer[MAX_SWAP_CHAIN_BUFFERS];
#else
	IMG_HANDLE				ahBuffer[MAX_SWAP_CHAIN_BUFFERS];
#endif
	IMG_UINT32				aui32Colour[NUM_COLOURS];
	IMG_UINT32				ui32SwapChainID = 0;
	IMG_UINT32				ui32SwapInterval = 1;
	IMG_HANDLE				hDisplayDevice = IMG_NULL;
#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID					hSystemBuffer = 0;
#else
	IMG_HANDLE				hSystemBuffer = IMG_NULL;
#endif
#endif
#ifdef __psp2__
	IMG_UINT32				ui32OldSelectBuffer = MAX_SWAP_CHAIN_BUFFERS - 1;
#endif
	IMG_UINT32				ui32SelectBuffer = 0;
	IMG_UINT32				ui32NumSwaps = 100;
	IMG_UINT32				ui32NumSwapChainBuffers = MAX_SWAP_CHAIN_BUFFERS;

	for (j = 1; j < argc; j++)
	{
		if (strcmp(argv[j], "-f") == 0)
		{
			j++;
			/* Read number of frames from command line. */
			ui32NumSwaps = atol(argv[j]);
		}
		else if (strcmp(argv[j], "-b") == 0)
		{
			j++;
			/* Read number of swap chain buffers from command line. */
			ui32NumSwapChainBuffers = atol(argv[j]);
			if (ui32NumSwapChainBuffers > MAX_SWAP_CHAIN_BUFFERS)
			{
				DPF("Number of swap chain buffers should not exceed %u\n", MAX_SWAP_CHAIN_BUFFERS);
				exit(-1);
			}
		}
		else
		{
			/* Read swap interval from command line. */
			ui32SwapInterval = atol(argv[j]);
			if (ui32SwapInterval >= 25)
			{
				DPF("Swap interval should be below 25 to avoid internal timeouts\n");
				exit(-1);
			}
		}
	}

	uiNumDevices = 10;

	DPF("----------------------- Start -----------------------\n");

	DPF("Call PVRSRVConnect with a valid argument:\n");

	eResult = PVRSRVConnect(&psConnection, 0);

	FAIL_IF_ERROR(eResult);

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

	DPF("Display Class API: get the system (primary) buffer\n");

#if defined(SUPPORT_PVRSRV_GET_DC_SYSTEM_BUFFER)
	/*
	   Get a handle to the primary surface in the system.
	*/
	eResult = PVRSRVGetDCSystemBuffer(hDisplayDevice, &hSystemBuffer);

	FAIL_IF_ERROR(eResult);
#endif

	for (i = 0; i < ui32SharedHeapCount; i++)
	{
		DPF(".... Shared heap %u - HeapID:0x%x DevVAddr:0x%x Size:0x%x Attr:0x%x\n",
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
	sSrcSurfAttrib.pixelformat = psPrimFormat[0].pixelformat;
	sSrcSurfAttrib.sDims = psPrimDims[0];
	sDstSurfAttrib = sSrcSurfAttrib;

	switch(psPrimFormat[0].pixelformat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		{
			aui32Colour[0] = 0xF800F800;/* full red */
			aui32Colour[1] = 0x001F001F;/* full blue */
			aui32Colour[2] = 0x07E007E0;/* full green */
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		{
			aui32Colour[0] = 0xFFFF0000;/* full alpha, full red */
			aui32Colour[1] = 0xFF0000FF;/* full alpha, full blue */
			aui32Colour[2] = 0xFF00FF00;/* full alpha, full green */
			break;
		}
		default:
		{
			DPF("Display Class API: ERROR unsupported pixel format!\n");
			exit(-1);
		}
	}	

	eResult = PVRSRVCreateDCSwapChain (hDisplayDevice,
										0,
										&sDstSurfAttrib,
										&sSrcSurfAttrib,
										ui32NumSwapChainBuffers, 
										0, 
										&ui32SwapChainID, 
										&hSwapChain);
	FAIL_IF_ERROR(eResult);

	eResult = PVRSRVGetDCBuffers(hDisplayDevice,
								hSwapChain,
								ahBuffer);
	FAIL_IF_ERROR(eResult);

	for (i = 0; i < ui32NumSwapChainBuffers; i++)
	{
		IMG_UINT32 *pui32Tmp, j;
		eResult = PVRSRVMapDeviceClassMemory(ps3DDevData,
			hDevMemContext,
			ahBuffer[i],
			&apsBackBufferMemInfo[i]);
		FAIL_IF_ERROR(eResult);

		/* colour the source */
		pui32Tmp = (IMG_UINT32*)apsBackBufferMemInfo[i]->pvLinAddr;
		for (j = 0; j < apsBackBufferMemInfo[i]->uAllocSize; j += 4)
		{
			*pui32Tmp++ = aui32Colour[i];
		}
	}

	for(i=0; i < ui32NumSwaps; i++)
	{

		eResult = PVRSRVSwapToDCBuffer (hDisplayDevice,
									ahBuffer[ui32SelectBuffer],
									0,
									IMG_NULL,
									ui32SwapInterval,
#if defined (SUPPORT_SID_INTERFACE)
									0);
#else
									IMG_NULL);
#endif

		FAIL_IF_ERROR2(eResult);

		ui32SelectBuffer++;
		if (ui32SelectBuffer == ui32NumSwapChainBuffers)
		{
			ui32SelectBuffer = 0;
		}
	}

	DPF("swapping back to system buffer\n");
	eResult = PVRSRVSwapToDCSystem(hDisplayDevice, hSwapChain);
	FAIL_IF_ERROR(eResult);

	DPF("waiting for flips to complete...\n");

#ifdef __psp2__

	sceKernelDelayThread(1000000);

#else

	{
		IMG_UINT32 i = 0;

		for (i = 0; i < ui32NumSwapChainBuffers; i++)
		{
			IMG_UINT32				ui32Count = 10 * ui32SwapInterval;
			PVRSRV_CLIENT_MEM_INFO	*psRenderSurfMemInfo = apsBackBufferMemInfo[i];

			DPF("Surface %u, read ops complete: %u, pending %u\n", i,
					SYNCINFO_READOPS_COMPLETE(psRenderSurfMemInfo->psClientSyncInfo),
					SYNCINFO_READOPS_PENDING(psRenderSurfMemInfo->psClientSyncInfo));

			while (SYNCINFO_READOPS_COMPLETE(psRenderSurfMemInfo->psClientSyncInfo) < 
				   SYNCINFO_READOPS_PENDING(psRenderSurfMemInfo->psClientSyncInfo))
			{
				/* Wait for flips to complete */
                
				sleep(1);

				ui32Count--;

				PVR_ASSERT(ui32Count > 0);

				if (ui32Count == 0)
				{
					DPF("ERROR: Flip failed to complete\n");
					break;
				}
			}
			
			DPF("Surface %u, read ops complete: %u, pending %u\n", i,
					SYNCINFO_READOPS_COMPLETE(psRenderSurfMemInfo->psClientSyncInfo),
					SYNCINFO_READOPS_PENDING(psRenderSurfMemInfo->psClientSyncInfo));
		}
	}

#endif

	free(psPrimFormat);
	free(psPrimDims);

	DPF("Display Class API: unmap swapchain display surfaces display surface from SGX\n");

	for(i=0; i < ui32NumSwapChainBuffers; i++)
	{
		eResult = PVRSRVUnmapDeviceClassMemory(ps3DDevData,
											   apsBackBufferMemInfo[i]);
		FAIL_IF_ERROR(eResult);	
	}

	eResult = PVRSRVDestroyDCSwapChain(hDisplayDevice, hSwapChain);
	FAIL_IF_ERROR(eResult);

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


	return 0;
}

/******************************************************************************
 End of file (sgx_flip_test.c)
******************************************************************************/
