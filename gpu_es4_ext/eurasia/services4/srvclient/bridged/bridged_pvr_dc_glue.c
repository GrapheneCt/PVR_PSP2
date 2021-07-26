#include <kernel.h>
#include <display.h>

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))

#include "eurasia/include4/services.h"
#include "eurasia/include4/pvr_debug.h"
#include "eurasia/services4/include/servicesint.h"
#include "eurasia/services4/include/pvr_bridge.h"

#define PSP2_DISPLAY_ID 0x09dfef50

typedef struct PSP2_SWAPCHAIN {
	PVRSRV_CONNECTION *psConnection;
	SceUID hSwapChainReadyEvf;
	SceUID hSwapChainPendingEvf;
	SceUID hSwapChainThread;
	SceUID hDispMemUID[PSP2_SWAPCHAIN_MAX_BUFFER_NUM];
	IMG_PVOID pDispBufVaddr[PSP2_SWAPCHAIN_MAX_BUFFER_NUM];
	PVRSRV_CLIENT_MEM_INFO sDispMemInfo[PSP2_SWAPCHAIN_MAX_BUFFER_NUM];
	IMG_UINT32 ui32BufferCount;
	DISPLAY_DIMS sDims;
} PSP2_SWAPCHAIN;

static PVRSRV_CLIENT_SYNC_INFO *s_psOldBufSyncInfo = IMG_NULL;

static IMG_BOOL s_flipChainExists = IMG_FALSE;
static IMG_UINT32 s_ui32CurrentSwapChainIdx = 0;
static IMG_PVOID s_pvCurrentNewBuf[PSP2_SWAPCHAIN_MAX_PENDING_COUNT];
static IMG_SID s_hKernelSwapChainSync[PSP2_SWAPCHAIN_MAX_PENDING_COUNT];
static IMG_UINT32 s_ui32CurrentSwapInterval = 0;
static SceUID s_hSwapChainReadyEvf = SCE_UID_INVALID_UID;
static SceUID s_hSwapChainPendingEvf = SCE_UID_INVALID_UID;

static IMG_INT32 _dcSwapChainThread(SceSize argSize, void *pArgBlock)
{
	SceDisplayFrameBuf fbInfo;
	PVRSRV_CONNECTION *psConnection;
	PSP2_SWAPCHAIN *psSwapChain = *(PSP2_SWAPCHAIN **)pArgBlock;

	psConnection = psSwapChain->psConnection;
	fbInfo.size = sizeof(SceDisplayFrameBuf);
	fbInfo.pitch = psSwapChain->sDims.ui32ByteStride / 4;
	fbInfo.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
	fbInfo.width = psSwapChain->sDims.ui32Width;
	fbInfo.height = psSwapChain->sDims.ui32Height;
	
	sceKernelSetEventFlag(s_hSwapChainReadyEvf, 1);

	while (s_flipChainExists) {

		sceKernelWaitEventFlag(s_hSwapChainPendingEvf, 1, SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);

		PVRSRVWaitSyncOp(s_hKernelSwapChainSync[s_ui32CurrentSwapChainIdx], IMG_NULL);

		fbInfo.base = s_pvCurrentNewBuf[s_ui32CurrentSwapChainIdx];
		sceDisplaySetFrameBuf(&fbInfo, SCE_DISPLAY_UPDATETIMING_NEXTVSYNC);
		sceDisplayWaitVblankStartMulti(s_ui32CurrentSwapInterval);

		PVRSRVModifyCompleteSyncOps(psConnection, s_hKernelSwapChainSync[s_ui32CurrentSwapChainIdx]);

		sceKernelSetEventFlag(s_hSwapChainReadyEvf, 1);
	}

	return sceKernelExitDeleteThread(0);
}

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
			PVR_ASSERT(*pui32DevCount == 1);
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
	if (!psDevData || ui32DeviceID != PSP2_DISPLAY_ID)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVOpenDCDevice: Invalid params"));
		return IMG_NULL;
	}

	return (IMG_HANDLE)psDevData->psConnection;
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
	if (!psConnection || !hDevice)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCloseDCDevice: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	return PVRSRV_OK;
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

	psDisplayInfo->ui32MaxSwapChainBuffers = PSP2_SWAPCHAIN_MAX_BUFFER_NUM;
	psDisplayInfo->ui32MaxSwapChains = 1;
	psDisplayInfo->ui32MinSwapInterval = PSP2_SWAPCHAIN_MIN_INTERVAL;
	psDisplayInfo->ui32MaxSwapInterval = PSP2_SWAPCHAIN_MAX_INTERVAL;
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

	asDim[0].ui32Width = 960;
	asDim[0].ui32Height = 544;
	asDim[0].ui32ByteStride = 4 * 960;

	asDim[1].ui32Width = 480;
	asDim[1].ui32Height = 272;
	asDim[1].ui32ByteStride = 4 * 512;

	asDim[2].ui32Width = 640;
	asDim[2].ui32Height = 368;
	asDim[2].ui32ByteStride = 4 * 640;

	asDim[3].ui32Width = 720;
	asDim[3].ui32Height = 408;
	asDim[3].ui32ByteStride = 4 * 768;

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
	IMG_UINT32 alignedSize;
	IMG_UINT32 swapChainAff = 0;
	IMG_INT32 i, x, y;
	PSP2_SWAPCHAIN *psSwapChain;
	PVRSRV_CONNECTION *psConnection;

	if (!hDevice
		|| !psDstSurfAttrib
		|| !psSrcSurfAttrib
		|| !pui32SwapChainID
		|| !phSwapChain
		|| ui32Flags)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (ui32BufferCount > PSP2_SWAPCHAIN_MAX_BUFFER_NUM)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Too many buffers"));
		return PVRSRV_ERROR_TOOMANYBUFFERS;
	}

	if (ui32BufferCount == 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Too few buffers"));
		return PVRSRV_ERROR_TOO_FEW_BUFFERS;
	}

	if ((ui32Flags & PVRSRV_CREATE_SWAPCHAIN_SHARED) || (ui32Flags & PVRSRV_CREATE_SWAPCHAIN_QUERY))
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Attempt to use unsupported flags"));
		return PVRSRV_ERROR_NOT_SUPPORTED;
	}

	if (s_flipChainExists)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Flip chain already exists"));
		return PVRSRV_ERROR_FLIP_CHAIN_EXISTS;
	}

	if (ui32OEMFlags)
		swapChainAff = ui32OEMFlags;
	else
		swapChainAff = SCE_KERNEL_CPU_MASK_USER_0;

	SceUID readyEvfId = sceKernelCreateEventFlag("DCSwapChainReadyEvf", 0, 0, NULL);
	if (readyEvfId < 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Failed to create swap chain ready event"));
		return PVRSRV_ERROR_UNABLE_TO_CREATE_EVENT;
	}

	s_hSwapChainReadyEvf = readyEvfId;

	SceUID pendingEvfId = sceKernelCreateEventFlag("DCSwapChainPendingEvf", 0, 0, NULL);
	if (pendingEvfId < 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Failed to create swap chain pending event"));
		return PVRSRV_ERROR_UNABLE_TO_CREATE_EVENT;
	}

	s_hSwapChainPendingEvf = pendingEvfId;

	SceUID thrdId = sceKernelCreateThread("DCSwapChainThread", _dcSwapChainThread, 64, SCE_KERNEL_THREAD_STACK_SIZE_MIN, 0, swapChainAff, NULL);
	if (thrdId < 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Failed to create swap chain thread"));
		return PVRSRV_ERROR_UNABLE_TO_CREATE_THREAD;
	}

	psConnection = (PVRSRV_CONNECTION *)hDevice;

	psSwapChain = (PSP2_SWAPCHAIN *)PVRSRVAllocUserModeMem(sizeof(PSP2_SWAPCHAIN));
	if (!psSwapChain)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateDCSwapChain: Alloc failed"));
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	psSwapChain->psConnection = psConnection;
	psSwapChain->hSwapChainReadyEvf = readyEvfId;
	psSwapChain->hSwapChainThread = thrdId;
	psSwapChain->sDims.ui32Width = psSrcSurfAttrib->sDims.ui32Width;
	psSwapChain->sDims.ui32Height = psSrcSurfAttrib->sDims.ui32Height;
	psSwapChain->sDims.ui32ByteStride = psSrcSurfAttrib->sDims.ui32ByteStride;
	psSwapChain->ui32BufferCount = ui32BufferCount;

	alignedSize = ALIGN(psSwapChain->sDims.ui32ByteStride * psSwapChain->sDims.ui32Height, 256 * 1024);

	for (i = 0; i < PSP2_SWAPCHAIN_MAX_PENDING_COUNT; i++) {
		PVRSRVCreateSyncInfoModObj((PVRSRV_CONNECTION *)hDevice, &s_hKernelSwapChainSync[i]);
	}

	for (i = 0; i < ui32BufferCount; i++) {

		psSwapChain->hDispMemUID[i] = sceKernelAllocMemBlock(
			"DCSwapChainBuffer",
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			alignedSize,
			NULL);

		sceKernelGetMemBlockBase(psSwapChain->hDispMemUID[i], &psSwapChain->pDispBufVaddr[i]);

		psSwapChain->sDispMemInfo[i].pvLinAddr = psSwapChain->pDispBufVaddr[i];
		psSwapChain->sDispMemInfo[i].pvLinAddrKM = psSwapChain->pDispBufVaddr[i];
		psSwapChain->sDispMemInfo[i].sDevVAddr.uiAddr = psSwapChain->pDispBufVaddr[i];
		psSwapChain->sDispMemInfo[i].uAllocSize = alignedSize;
		psSwapChain->sDispMemInfo[i].ui32Flags = PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR;
		psSwapChain->sDispMemInfo[i].ui32ClientFlags = PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR;

		// memset the buffer to black
		for (y = 0; y < psSwapChain->sDims.ui32Height; y++) {
			unsigned int *row = (unsigned int *)psSwapChain->pDispBufVaddr[i] + y * (psSwapChain->sDims.ui32ByteStride / 4);
			for (x = 0; x < psSwapChain->sDims.ui32Width; x++) {
				row[x] = 0xff000000;
			}
		}

	}

	s_flipChainExists = IMG_TRUE;

	sceKernelStartThread(thrdId, 4, &psSwapChain);

	/* Assign output */
	*phSwapChain = (IMG_SID)psSwapChain;
	/* optional ID (in/out) */
	*pui32SwapChainID = 0;

	return PVRSRV_OK;
}

/*!
 ******************************************************************************
 @Function	PVRSRVDestroyDCSwapChain
 @Description
 Destroy a swap chain on the specified display class device
 @Input hDevice			- device handle
 @Input hSwapChain		- handle to swapchain
 @Return
	success: PVRSRV_OK
	failure: PVRSRV_ERROR_*
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVDestroyDCSwapChain(IMG_HANDLE hDevice,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID    hSwapChain)
#else
IMG_HANDLE hSwapChain)
#endif
{
	IMG_INT32 i;
	PSP2_SWAPCHAIN *psSwapChain;

	if (!hDevice || !hSwapChain)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVDestroyDCSwapChain: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	psSwapChain = (PSP2_SWAPCHAIN *)hSwapChain;

	for (i = 0; i < psSwapChain->ui32BufferCount; i++) {
		sceKernelFreeMemBlock(psSwapChain->hDispMemUID[i]);
	}

	s_psOldBufSyncInfo = IMG_NULL;

	s_flipChainExists = IMG_FALSE;

	sceKernelSetEventFlag(s_hSwapChainPendingEvf, 1);
	sceKernelWaitThreadEnd(psSwapChain->hSwapChainThread, NULL, NULL);

	sceKernelDeleteEventFlag(psSwapChain->hSwapChainReadyEvf);
	sceKernelDeleteEventFlag(psSwapChain->hSwapChainPendingEvf);

	for (i = 0; i < PSP2_SWAPCHAIN_MAX_PENDING_COUNT; i++) {
		PVRSRVDestroySyncInfoModObj((PVRSRV_CONNECTION *)hDevice, s_hKernelSwapChainSync[i]);
	}

	PVRSRVFreeUserModeMem(psSwapChain);

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
		phBuffer[i] = (IMG_SID)&psSwapChain->sDispMemInfo[i];
	}

	return PVRSRV_OK;
}

/*!
 ******************************************************************************
 @Function	PVRSRVSwapToDCBuffer
 @Description		Swap display to a display class buffer
 @Input hDevice		- device handle
 @Input hBuffer		- buffer handle
 @Input ui32ClipRectCount - clip rectangle count
 @Input psClipRect - array of clip rects
 @Input ui32SwapInterval - number of Vsync intervals between swaps
 @Input hPrivateTag - Private Tag to passed through display
 handling pipeline (audio sync etc.)
 @Return
	success: PVRSRV_OK
	failure: PVRSRV_ERROR_*
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVSwapToDCBuffer(IMG_HANDLE	hDevice,
	IMG_SID		hBuffer,
	IMG_UINT32	ui32ClipRectCount,
	IMG_RECT	*psClipRect,
	IMG_UINT32	ui32SwapInterval,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID		hPrivateTag)
#else
	IMG_HANDLE	hPrivateTag)
#endif
{
	IMG_UINT32 uiSwapSyncNum = 0;
	PVRSRV_ERROR eError = PVRSRV_OK;
	PVRSRV_OP_CLIENT_SYNC_INFO syncInfoArg;
	PVRSRV_CLIENT_MEM_INFO *psBufMemInfoOld = IMG_NULL;
	PVRSRV_CLIENT_MEM_INFO *psBufMemInfoNew = IMG_NULL;

	if (!hDevice || !hBuffer)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVSwapToDCBuffer: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (ui32ClipRectCount)
	{
		if (!psClipRect
			|| (ui32ClipRectCount > PVRSRV_MAX_DC_CLIP_RECTS))
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRVSwapToDCBuffer: Invalid rect count (%d)", ui32ClipRectCount));
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

	if (ui32SwapInterval > PSP2_SWAPCHAIN_MAX_INTERVAL || ui32SwapInterval < PSP2_SWAPCHAIN_MIN_INTERVAL)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVSwapToDCBuffer2KM: Invalid swap interval. Requested %u, Allowed range %u-%u",
			ui32SwapInterval, PSP2_SWAPCHAIN_MIN_INTERVAL, PSP2_SWAPCHAIN_MAX_INTERVAL));
		return PVRSRV_ERROR_INVALID_SWAPINTERVAL;
	}

	psBufMemInfoNew = (PVRSRV_CLIENT_MEM_INFO *)hBuffer;

	if (!s_psOldBufSyncInfo) {
		syncInfoArg.psInfoOld = psBufMemInfoNew->psClientSyncInfo;
		syncInfoArg.psInfoNew = IMG_NULL;
		uiSwapSyncNum = 1;
	}
	else {
		syncInfoArg.psInfoOld = s_psOldBufSyncInfo;
		syncInfoArg.psInfoNew = psBufMemInfoNew->psClientSyncInfo;
		uiSwapSyncNum = 2;
	}

	s_psOldBufSyncInfo = psBufMemInfoNew->psClientSyncInfo;

	sceKernelWaitEventFlag(s_hSwapChainReadyEvf, 1, SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);

	s_ui32CurrentSwapChainIdx++;

	if (s_ui32CurrentSwapChainIdx == PSP2_SWAPCHAIN_MAX_PENDING_COUNT)
		s_ui32CurrentSwapChainIdx = 0;

	s_pvCurrentNewBuf[s_ui32CurrentSwapChainIdx] = psBufMemInfoNew->pvLinAddr;
	s_ui32CurrentSwapInterval = ui32SwapInterval;

	eError = PVRSRVModifyPendingSyncOps(
		(PVRSRV_CONNECTION *)hDevice,
		s_hKernelSwapChainSync[s_ui32CurrentSwapChainIdx],
		&syncInfoArg,
		uiSwapSyncNum,
		PVRSRV_MODIFYSYNCOPS_FLAGS_RO_INC,
		IMG_NULL,
		IMG_NULL);

	sceKernelSetEventFlag(s_hSwapChainPendingEvf, 1);

	return eError;
}

/*!
 ******************************************************************************
 @Function	PVRSRVSwapToDCSystem
 @Description
 Swap display to the display's system buffer
 @Input hDevice		- device handle
 @Input hSwapChain	- swapchain handle
 @Return
	success: PVRSRV_OK
	failure: PVRSRV_ERROR_*
 ******************************************************************************/
IMG_EXPORT
PVRSRV_ERROR IMG_CALLCONV PVRSRVSwapToDCSystem(IMG_HANDLE hDevice,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID    hSwapChain)
#else
IMG_HANDLE hSwapChain)
#endif
{
	if (!hDevice || !hSwapChain)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVSwapToDCSystem: Invalid params"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	return PVRSRV_OK;
}