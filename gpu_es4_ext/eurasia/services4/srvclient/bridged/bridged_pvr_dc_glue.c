#include <kernel.h>
#include <display.h>

#include "psp2_pvr_desc.h"

#include "eurasia/include4/services.h"
#include "eurasia/include4/pvr_debug.h"
#include "eurasia/services4/include/servicesint.h"

typedef struct PSP2_SWAPCHAIN {
	SceUID hDispMemUID[3];
	IMG_PVOID pDispBufVaddr[3];
	IMG_UINT32 ui32BufferCount;
	DISPLAY_DIMS sDims;
} PSP2_SWAPCHAIN;

#define PSP2_DISPLAY_ID 0x09dfef50
#define PSP2_DISPLAY_HANDLE 0x921af851

static IMG_BOOL s_flipChainExists = IMG_FALSE;

/*!
 ******************************************************************************
 @Function	PVRSRVEnumerateDeviceClass
 @Description
 Enumerates devices available in a given class.
 On first call, pass valid ptr for pui32DevCount and NULL for pui32DevID,
 On second call, pass same ptr for pui32DevCount and client allocated ptr
 for pui32DevID device id list
 @Input		psConnection : handle for services connection
 @Input		eDeviceClass : device class identifier
 @Output	pui32DevCount : number of devices available in class
 @Output	pui32DevID : list of device ids in the device class
 @Return	PVRSRV_ERROR
 *****************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVEnumerateDeviceClass(const PVRSRV_CONNECTION *psConnection,
	PVRSRV_DEVICE_CLASS DeviceClass,
	IMG_UINT32 *pui32DevCount,
	IMG_UINT32 *pui32DevID)
{

	if (!psConnection || (!psConnection->hServices))
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVEnumerateDeviceClass: Invalid connection"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (pui32DevCount == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVEnumerateDeviceClass: Invalid DevCount"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (DeviceClass == PVRSRV_DEVICE_CLASS_DISPLAY) {

		if (!pui32DevID)
		{
			/* First mode, ask for Device Count */
			*pui32DevCount = 1;
		}
		else
		{
			/* Fill in device id array */
			PVR_ASSERT(*pui32DevCount == devCount);
			pui32DevID[0] = PSP2_DISPLAY_ID;
		}
	}
	else {
		if (!pui32DevID)
		{
			/* First mode, ask for Device Count */
			*pui32DevCount = 0;
		}
	}

	return PVRSRV_OK;
}

/*!
 ******************************************************************************
 @Function	PVRSRVOpenDCDevice
 @Description
 Opens a connection to a display device specified by the device id/index argument.
 Calls into kernel services to retrieve device information structure associated
 with the device id.
 If matching device structure is found it's used to load the appropriate
 external client-side driver library and sets up data structures and a function
 jump table into the external driver
 @Input		psDevData	- handle for services connection
 @Inpu		ui32DeviceID	- device identifier
 @Return	IMG_HANDLE for matching display class device (IMG_NULL on failure)
 ******************************************************************************/
IMG_EXPORT
IMG_HANDLE IMG_CALLCONV PVRSRVOpenDCDevice(const PVRSRV_DEV_DATA *psDevData,
	IMG_UINT32 ui32DeviceID)
{
	PVRSRV_CLIENT_DEVICECLASS_INFO *psDevClassInfo = IMG_NULL;

	if (!psDevData)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVOpenDCDevice: Invalid params"));
		return IMG_NULL;
	}

	/* Alloc client info structure first */
	psDevClassInfo = (PVRSRV_CLIENT_DEVICECLASS_INFO *)PVRSRVAllocUserModeMem(sizeof(PVRSRV_CLIENT_DEVICECLASS_INFO));
	if (psDevClassInfo == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVOpenDCDevice: Alloc failed"));
		return IMG_NULL;
	}

	if (ui32DeviceID != PSP2_DISPLAY_ID)
		goto ErrorExit;

	/* setup private client services structure */
	psDevClassInfo->hServices = psDevData->psConnection->hServices;
	psDevClassInfo->hDeviceKM = PSP2_DISPLAY_HANDLE;

	/* return private client services structure as handle */
	return (IMG_HANDLE)psDevClassInfo;

ErrorExit:
	PVRSRVFreeUserModeMem(psDevClassInfo);
	return IMG_NULL;
}

/*!
 ******************************************************************************
 @Function	PVRSRVGetDCInfo
 @Description
 returns information about the display device
 @Input		hDevice 		- handle for device
 @Input		psDisplayInfo	- display device information
 @Return PVRSRV_ERROR
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVGetDCInfo(IMG_HANDLE hDevice,
	DISPLAY_INFO *psDisplayInfo)
{
	if (!hDevice || !psDisplayInfo)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVGetDCInfo: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	sceClibStrncpy(psDisplayInfo->szDisplayName, "psp2_display", MAX_DISPLAY_NAME_SIZE);

	psDisplayInfo->ui32MaxSwapChainBuffers = 3;
	psDisplayInfo->ui32MaxSwapChains = 1;
	psDisplayInfo->ui32MinSwapInterval = 0;
	psDisplayInfo->ui32MaxSwapInterval = 1;
	psDisplayInfo->ui32PhysicalWidthmm = 110;
	psDisplayInfo->ui32PhysicalHeightmm = 63;

	return PVRSRV_OK;
}

/*!
 ******************************************************************************
 @Function	PVRSRVEnumDCFormats
 @Description	Enumerates displays pixel formats
 @Input hDevice		- device handle
 @Output pui32Count	- number of formats
 @Output psFormat - buffer to return formats in
 @Return	PVRSRV_ERROR
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVEnumDCFormats(IMG_HANDLE	hDevice,
	IMG_UINT32	*pui32Count,
	DISPLAY_FORMAT *psFormat)
{
	if (!pui32Count || !hDevice)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVEnumDCFormats: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	/*
	   call may not preallocate the max psFormats and
	   instead query the count and then enum
	   */
	if (psFormat)
	{
		psFormat[0].pixelformat = PVRSRV_PIXEL_FORMAT_ABGR8888;
		*pui32Count = 1;
	}
	else
	{
		*pui32Count = 1;
	}

	return PVRSRV_OK;
}

/*!
 ******************************************************************************
 @Function	PVRSRVEnumDCDims
 @Description	Enumerates dimensions for a given format
 @Input hDevice		- device handle
 @Input psFormat - buffer to return formats in
 @Output pui32Count	- number of dimensions
 @Output pDims	- number of dimensions
 @Return	PVRSRV_ERROR
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVEnumDCDims(IMG_HANDLE hDevice,
	IMG_UINT32 *pui32Count,
	DISPLAY_FORMAT *psFormat,
	DISPLAY_DIMS *psDims)
{
	IMG_INT32 dispWith, dispHeight;
	IMG_UINT32 dimNum = 4;
	DISPLAY_DIMS asDim[6];

	if (!pui32Count || !hDevice || psFormat->pixelformat != PVRSRV_PIXEL_FORMAT_ABGR8888)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVEnumDCDims: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	asDim[0].ui32Width = 480;
	asDim[0].ui32Height = 272;
	asDim[0].ui32ByteStride = 4 * 512;

	asDim[1].ui32Width = 640;
	asDim[1].ui32Height = 368;
	asDim[1].ui32ByteStride = 4 * 640;

	asDim[2].ui32Width = 720;
	asDim[2].ui32Height = 408;
	asDim[2].ui32ByteStride = 4 * 768;

	asDim[3].ui32Width = 960;
	asDim[3].ui32Height = 544;
	asDim[3].ui32ByteStride = 4 * 960;

	asDim[4].ui32Width = 1280;
	asDim[4].ui32Height = 725;
	asDim[4].ui32ByteStride = 4 * 1280;

	asDim[5].ui32Width = 1920;
	asDim[5].ui32Height = 1088;
	asDim[5].ui32ByteStride = 4 * 1920;

	sceDisplayGetMaximumFrameBufResolution(&dispWith, &dispHeight);

	if (dispWith > 960)
		dimNum = 6;

	/*
	   call may not preallocate the max psFormats and
	   instead query the count and then enum
	   */
	if (psDims)
	{
		IMG_UINT32 i;

		for (i = 0; i < dimNum; i++)
		{
			sceClibMemcpy(&psDims[i], &asDim[i], sizeof(DISPLAY_DIMS));
		}
		*pui32Count = dimNum;
	}
	else
	{
		*pui32Count = dimNum;
	}

	return PVRSRV_OK;
}

/*!
 ******************************************************************************
 @Function	PVRSRVCloseDCDevice
 @Description	Closes connection to a display device specified by the handle
 @Input hDevice - device handle
 @Return	PVRSRV_ERROR
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVCloseDCDevice(const PVRSRV_CONNECTION *psConnection,
	IMG_HANDLE hDevice)
{
	PVRSRV_CLIENT_DEVICECLASS_INFO *psDevClassInfo = IMG_NULL;

	if (!psConnection || !hDevice)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCloseDCDevice: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psDevClassInfo = (PVRSRV_CLIENT_DEVICECLASS_INFO*)hDevice;

	/* free private client structure */
	PVRSRVFreeUserModeMem(psDevClassInfo);

	return PVRSRV_OK;
}

/*!
 ******************************************************************************
 @Function	PVRSRVCreateDCSwapChain
 @Description
 Creates a swap chain on the specified display class device
 @Input hDevice				- device handle
 @Input ui32Flags			- flags
 @Input psDstSurfAttrib		- DST surface attribs
 @Input psSrcSurfAttrib		- SRC surface attribs
 @Input ui32BufferCount		- number of buffers in chain
 @Input ui32OEMFlags		- OEM flags for OEM extended functionality
 @Input pui32SwapChainID 	- ID assigned to swapchain (optional feature)
 @Output phSwapChain		- handle to swapchain
 @Return
	success: PVRSRV_OK
	failure: PVRSRV_ERROR_*
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVCreateDCSwapChain(IMG_HANDLE	hDevice,
	IMG_UINT32	ui32Flags,
	DISPLAY_SURF_ATTRIBUTES *psDstSurfAttrib,
	DISPLAY_SURF_ATTRIBUTES *psSrcSurfAttrib,
	IMG_UINT32	ui32BufferCount,
	IMG_UINT32	ui32OEMFlags,
	IMG_UINT32	*pui32SwapChainID,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID		*phSwapChain)
#else
	IMG_HANDLE	*phSwapChain)
#endif

{
	IMG_INT32 i, x, y;
	PSP2_SWAPCHAIN *psSwapChain;

	if (!hDevice
		|| !psDstSurfAttrib
		|| !psSrcSurfAttrib
		|| !pui32SwapChainID
		|| !phSwapChain
		|| ui32Flags
		|| ui32OEMFlags)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (ui32BufferCount > 3)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Too many buffers"));
		return PVRSRV_ERROR_TOOMANYBUFFERS;
	}

	if (ui32BufferCount == 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Too few buffers"));
		return PVRSRV_ERROR_TOO_FEW_BUFFERS;
	}

	if (s_flipChainExists)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Flip chain already exists"));
		return PVRSRV_ERROR_FLIP_CHAIN_EXISTS;
	}

	psSwapChain = (PSP2_SWAPCHAIN *)PVRSRVAllocUserModeMem(sizeof(PSP2_SWAPCHAIN));
	if (!psSwapChain)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Alloc failed"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psSwapChain->sDims.ui32Width = psSrcSurfAttrib->sDims.ui32Width;
	psSwapChain->sDims.ui32Height = psSrcSurfAttrib->sDims.ui32Height;
	psSwapChain->sDims.ui32ByteStride = psSrcSurfAttrib->sDims.ui32ByteStride;
	psSwapChain->ui32BufferCount = ui32BufferCount;

	for (i = 0; i < ui32BufferCount; i++) {

		psSwapChain->hDispMemUID[i] = sceKernelAllocMemBlock(
			"DCSwapChainBuffer",
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			psSwapChain->sDims.ui32ByteStride * psSwapChain->sDims.ui32Width,
			NULL);

		sceKernelGetMemBlockBase(psSwapChain->hDispMemUID[i], &psSwapChain->pDispBufVaddr[i]);

			// memset the buffer to black
			for (y = 0; y < psSwapChain->sDims.ui32Height; y++) {
				unsigned int *row = (unsigned int *)psSwapChain->pDispBufVaddr[i] + y * (psSwapChain->sDims.ui32ByteStride / 4);
				for (x = 0; x < psSwapChain->sDims.ui32Width; x++) {
					row[x] = 0xff000000;
				}
			}
	}

	s_flipChainExists = IMG_TRUE;

	/* Assign output */
	*phSwapChain = (IMG_SID)psSwapChain;
	/* optional ID (in/out) */
	*pui32SwapChainID = 0;

	return PVRSRV_OK;
}

/*!
 ******************************************************************************
 @Function	PVRSRVGetDCBuffers
 @Description
 Gets buffer handles for buffers in swapchain
 @Input hDevice			- device handle
 @Input hSwapChain		- handle to swapchain
 @Input phBuffer		- pointer to array of buffer handles
							Note: array length defined by ui32BufferCount
							passed to PVRSRVCreateDCSwapChain
 @Return
	success: PVRSRV_OK
	failure: PVRSRV_ERROR_*
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVGetDCBuffers(IMG_HANDLE hDevice,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID    hSwapChain,
	IMG_SID   *phBuffer)
#else
IMG_HANDLE hSwapChain,
IMG_HANDLE *phBuffer)
#endif
{
	IMG_INT32 i;
	PSP2_SWAPCHAIN *psSwapChain;

	if (!hDevice || !hSwapChain || !phBuffer)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVGetDCBuffers: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psSwapChain = (PSP2_SWAPCHAIN *)hSwapChain;

	/* Assign output */
	for (i = 0; i < psSwapChain->ui32BufferCount; i++)
	{
		phBuffer[i] = (IMG_SID)psSwapChain->pDispBufVaddr[i];
	}

	return PVRSRV_OK;
}

