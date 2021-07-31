#include <display.h>

#include "drveglplatform.h"
#include "wsegl.h"


#include "psp2_pvr_defs.h"
#include "sgxapi_km.h"
#include "services.h"
#include "pvr_debug.h"

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))

static WSEGLCaps asWSCaps[] =
{
	{ WSEGL_CAP_MIN_SWAP_INTERVAL, PSP2_SWAPCHAIN_MIN_INTERVAL },
	{ WSEGL_CAP_MAX_SWAP_INTERVAL, PSP2_SWAPCHAIN_MAX_INTERVAL },
	{ WSEGL_CAP_WINDOWS_USE_HW_SYNC, 1 },
	{ WSEGL_NO_CAPS, 0 }
};

static WSEGLConfig asDispConfigs[3];

static WSEGLError WSEGL_IsDisplayValid(NativeDisplayType hNativeDisplay)
{
	WSEGL_UNREFERENCED_PARAMETER(hNativeDisplay);

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_InitialiseDisplay(NativeDisplayType hNativeDisplay,
	WSEGLDisplayHandle *phDisplay,
	const WSEGLCaps **psCapabilities,
	WSEGLConfig **psConfigs)
{
	WSEGL_UNREFERENCED_PARAMETER(hNativeDisplay);

	asDispConfigs[0].ui32DrawableType = WSEGL_DRAWABLE_WINDOW;
	asDispConfigs[0].ePixelFormat = WSEGL_PIXELFORMAT_ABGR8888;
	asDispConfigs[0].ulNativeRenderable = WSEGL_FALSE;
	asDispConfigs[0].ulFrameBufferLevel = 0;
	asDispConfigs[0].ulNativeVisualID = 0;
	asDispConfigs[0].hNativeVisual = 0;
	asDispConfigs[0].eTransparentType = WSEGL_OPAQUE;
	asDispConfigs[0].ulTransparentColor = 0;

	asDispConfigs[1].ui32DrawableType = WSEGL_DRAWABLE_PIXMAP;
	asDispConfigs[1].ePixelFormat = WSEGL_PIXELFORMAT_ABGR8888;
	asDispConfigs[1].ulNativeRenderable = WSEGL_FALSE;
	asDispConfigs[1].ulFrameBufferLevel = 0;
	asDispConfigs[1].ulNativeVisualID = 0;
	asDispConfigs[1].hNativeVisual = 0;
	asDispConfigs[1].eTransparentType = WSEGL_OPAQUE;
	asDispConfigs[1].ulTransparentColor = 0;

	asDispConfigs[2].ui32DrawableType = WSEGL_NO_DRAWABLE;
	asDispConfigs[2].ePixelFormat = 0;
	asDispConfigs[2].ulNativeRenderable = 0;
	asDispConfigs[2].ulFrameBufferLevel = 0;
	asDispConfigs[2].ulNativeVisualID = 0;
	asDispConfigs[2].hNativeVisual = 0;
	asDispConfigs[2].eTransparentType = 0;
	asDispConfigs[2].ulTransparentColor = 0;

	*phDisplay = IMG_NULL;
	*psCapabilities = &asWSCaps[0];
	*psConfigs = &asDispConfigs[0];

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_CloseDisplay(WSEGLDisplayHandle hDisplay)
{
	WSEGL_UNREFERENCED_PARAMETER(hDisplay);

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_CreateWindowDrawable(WSEGLDisplayHandle hDisplay,
	WSEGLConfig *psConfig,
	WSEGLDrawableHandle *phDrawable,
	NativeWindowType hNativeWindow,
	WSEGLRotationAngle *eRotationAngle,
	PVRSRV_CONNECTION *psConnection)
{
	IMG_UINT32 ui32DispWith, ui32DispHeight, ui32BufNum;
	IMG_BOOL bIsAllowedHd = IMG_FALSE;
	DISPLAY_SURF_ATTRIBUTES sDispSurfAttrib;
	PVRSRV_ERROR eSrvError;

	sceDisplayGetMaximumFrameBufResolution(&ui32DispWith, &ui32DispHeight);

	if (ui32DispWith > 960)
	{
		bIsAllowedHd = IMG_TRUE;
	}

	sDispSurfAttrib.pixelformat = PVRSRV_PIXEL_FORMAT_A8B8G8R8;
	sDispSurfAttrib.sDims.ui32Width = 0;
	sDispSurfAttrib.sDims.ui32Height = 0;
	sDispSurfAttrib.sDims.ui32ByteStride = 0;

	switch (hNativeWindow->windowSize)
	{
	case PSP2_WINDOW_960X544:
		sDispSurfAttrib.sDims.ui32Width = 960;
		sDispSurfAttrib.sDims.ui32Height = 544;
		sDispSurfAttrib.sDims.ui32ByteStride = 4 * 960;
		break;
	case PSP2_WINDOW_480X272:
		sDispSurfAttrib.sDims.ui32Width = 480;
		sDispSurfAttrib.sDims.ui32Height = 272;
		sDispSurfAttrib.sDims.ui32ByteStride = 4 * 512;
		break;
	case PSP2_WINDOW_640X368:
		sDispSurfAttrib.sDims.ui32Width = 640;
		sDispSurfAttrib.sDims.ui32Height = 368;
		sDispSurfAttrib.sDims.ui32ByteStride = 4 * 640;
		break;
	case PSP2_WINDOW_720X408:
		sDispSurfAttrib.sDims.ui32Width = 720;
		sDispSurfAttrib.sDims.ui32Height = 408;
		sDispSurfAttrib.sDims.ui32ByteStride = 4 * 768;
		break;
	}

	if (bIsAllowedHd)
	{
		switch (hNativeWindow->windowSize)
		{
		case PSP2_WINDOW_1280X725:
			sDispSurfAttrib.sDims.ui32Width = 1280;
			sDispSurfAttrib.sDims.ui32Height = 725;
			sDispSurfAttrib.sDims.ui32ByteStride = 4 * 1280;
			break;
		case PSP2_WINDOW_1920X1088:
			sDispSurfAttrib.sDims.ui32Width = 1920;
			sDispSurfAttrib.sDims.ui32Height = 1088;
			sDispSurfAttrib.sDims.ui32ByteStride = 4 * 1920;
			break;
		}
	}

	if (sDispSurfAttrib.sDims.ui32Width == 0)
	{
		return WSEGL_BAD_NATIVE_WINDOW;
	}

	hNativeWindow->psConnection = psConnection;

	if (hNativeWindow->numFlipBuffers > PSP2_SWAPCHAIN_MAX_BUFFER_NUM)
		ui32BufNum = PSP2_SWAPCHAIN_MAX_BUFFER_NUM;
	else
		ui32BufNum = hNativeWindow->numFlipBuffers;

	eSrvError = PVRSRVCreateDCSwapChain((IMG_HANDLE)psConnection,
		0,
		&sDispSurfAttrib,
		&sDispSurfAttrib,
		ui32BufNum,
		hNativeWindow->flipChainThrdAffinity,
		&hNativeWindow->swapChainId,
		&hNativeWindow->swapChain);
	if (eSrvError != PVRSRV_OK)
	{
		return WSEGL_CANNOT_INITIALISE;
	}

	/**************************************************************************
	*	Retrieve and map swap buffers
	**************************************************************************/
	eSrvError = PVRSRVGetDCBuffers((IMG_HANDLE)psConnection,
		hNativeWindow->swapChain,
		hNativeWindow->ahSwapChainBuffers);
	if (eSrvError != PVRSRV_OK)
	{
		return WSEGL_CANNOT_INITIALISE;
	}

	for (IMG_INT32 i = 0; i < ui32BufNum; i++)
	{
		eSrvError = PVRSRVMapDeviceClassMemory(hNativeWindow->psDevData,
			hNativeWindow->hDevMemContext,
			hNativeWindow->ahSwapChainBuffers[i],
			&hNativeWindow->apsSwapBufferMemInfo[i]);
		if (eSrvError != PVRSRV_OK)
		{
			return WSEGL_CANNOT_INITIALISE;
		}
	}

	PVRSRVSwapToDCBuffer(psConnection,
		hNativeWindow->ahSwapChainBuffers[0],
		0,
		IMG_NULL,
		hNativeWindow->swapInterval,
		0);

	hNativeWindow->currBufIdx = 1;
	*phDrawable = (WSEGLDrawableHandle)hNativeWindow;
	*eRotationAngle = WSEGL_ROTATE_0;

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_CreatePixmapDrawable(WSEGLDisplayHandle hDisplay,
	WSEGLConfig *psConfig,
	WSEGLDrawableHandle *phDrawable,
	NativePixmapType hNativePixmap,
	WSEGLRotationAngle *eRotationAngle)
{
	IMG_UINT32 ui32MemSize;
	PVRSRV_ERROR eError;

	if (hNativePixmap->memType == SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW)
		ui32MemSize = ALIGN(hNativePixmap->sizeX * hNativePixmap->sizeY * 4, 256 * 1024);
	else if (hNativePixmap->memType == SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW || hNativePixmap->memType == SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW)
		ui32MemSize = ALIGN(hNativePixmap->sizeX * hNativePixmap->sizeY * 4, 1024 * 1024);
	else
		ui32MemSize = ALIGN(hNativePixmap->sizeX * hNativePixmap->sizeY * 4, 4 * 1024);

	hNativePixmap->memUID = sceKernelAllocMemBlock(
		"WSEGLPixmap",
		hNativePixmap->memType,
		ui32MemSize,
		SCE_NULL);

	if (hNativePixmap->memUID <= 0)
	{
		return WSEGL_OUT_OF_MEMORY;
	}

	sceKernelGetMemBlockBase(hNativePixmap->memUID, &hNativePixmap->memBase);

	eError = PVRSRVMapMemoryToGpu(
		hNativePixmap->psDevData,
		hNativePixmap->hDevMemContext,
		0,
		ui32MemSize,
		0,
		hNativePixmap->memBase,
		PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR,
		IMG_NULL);
	if (eError != PVRSRV_OK)
	{
		sceKernelFreeMemBlock(hNativePixmap->memUID);
		return WSEGL_BAD_DRAWABLE;
	}

	*phDrawable = (WSEGLDrawableHandle)hNativePixmap;
	*eRotationAngle = WSEGL_ROTATE_0;

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_DeleteDrawable(WSEGLDrawableHandle hDrawable)
{
	Psp2DrawableType eType = *(Psp2DrawableType *)hDrawable;
	NativeWindowType window;
	NativePixmapType pixmap;

	switch (eType)
	{
	case PSP2_DRAWABLE_TYPE_WINDOW:
		window = (NativeWindowType)hDrawable;

		for (IMG_INT32 i = 0; i < window->numFlipBuffers; i++)
		{
			PVRSRVUnmapDeviceClassMemory(window->psDevData, window->apsSwapBufferMemInfo[i]);
		}

		PVRSRVDestroyDCSwapChain((IMG_HANDLE)window->psConnection, window->swapChain);
		break;

	case PSP2_DRAWABLE_TYPE_PIXMAP:
		pixmap = (NativePixmapType)hDrawable;
		PVRSRVUnmapMemoryFromGpu(pixmap->psDevData, pixmap->memBase, 0, IMG_FALSE);
		sceKernelFreeMemBlock(pixmap->memUID);
		break;

	default:
		return WSEGL_BAD_DRAWABLE;
	}

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_SwapDrawable(WSEGLDrawableHandle hDrawable, unsigned long ui32Data)
{
	NativeWindowType window = (NativeWindowType)hDrawable;

	if (window->type != PSP2_DRAWABLE_TYPE_WINDOW)
	{
		return WSEGL_BAD_DRAWABLE;
	}

	PVRSRVSwapToDCBuffer((IMG_HANDLE)window->psConnection,
		window->ahSwapChainBuffers[window->currBufIdx],
		0,
		IMG_NULL,
		window->swapInterval,
		0);

	window->currBufIdx = (window->currBufIdx + 1) % window->numFlipBuffers;
}

static WSEGLError WSEGL_SwapControlInterval(WSEGLDrawableHandle hDrawable, unsigned long ulInterval)
{
	NativeWindowType window = (NativeWindowType)hDrawable;

	if (window->type != PSP2_DRAWABLE_TYPE_WINDOW)
	{
		return WSEGL_BAD_DRAWABLE;
	}

	window->swapInterval = ulInterval;

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_WaitNative(WSEGLDrawableHandle hDrawable, unsigned long ui32Engine)
{
	PVR_DPF((PVR_DBG_ERROR, "%s: Unimplemented", __func__));
	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_CopyFromDrawable(WSEGLDrawableHandle hDrawable, NativePixmapType hNativePixmap)
{
	PVR_DPF((PVR_DBG_ERROR, "%s: Unimplemented", __func__));
	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_CopyFromPBuffer(void *pvAddress,
	unsigned long ui32Width,
	unsigned long ui32Height,
	unsigned long ui32Stride,
	WSEGLPixelFormat ePixelFormat,
	NativePixmapType hNativePixmap)
{
	PVR_DPF((PVR_DBG_ERROR, "%s: Unimplemented", __func__));
	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_GetDrawableParameters(WSEGLDrawableHandle hDrawable,
	WSEGLDrawableParams *psSourceParams,
	WSEGLDrawableParams *psRenderParams)
{
	Psp2DrawableType eType = *(Psp2DrawableType *)hDrawable;
	NativeWindowType window;
	NativePixmapType pixmap;
	PVRSRV_CLIENT_MEM_INFO *bufMemInfo;

	sceClibMemset(psSourceParams, 0, sizeof(WSEGLDrawableParams));
	sceClibMemset(psRenderParams, 0, sizeof(WSEGLDrawableParams));

	switch (eType)
	{
	case PSP2_DRAWABLE_TYPE_WINDOW:
		window = (NativeWindowType)hDrawable;

		bufMemInfo = (PVRSRV_CLIENT_MEM_INFO *)window->apsSwapBufferMemInfo[(window->currBufIdx + window->numFlipBuffers - 1) % window->numFlipBuffers];

		psSourceParams->ePixelFormat = WSEGL_PIXELFORMAT_ABGR8888;
		psSourceParams->pvLinearAddress = bufMemInfo->pvLinAddr;
		psSourceParams->ui32HWAddress = bufMemInfo->pvLinAddr;
		psSourceParams->hPrivateData = bufMemInfo;

		switch (window->windowSize)
		{
		case PSP2_WINDOW_960X544:
			psSourceParams->ui32Width = 960;
			psSourceParams->ui32Height = 544;
			psSourceParams->ui32Stride = 960;
			break;
		case PSP2_WINDOW_480X272:
			psSourceParams->ui32Width = 480;
			psSourceParams->ui32Height = 272;
			psSourceParams->ui32Stride = 512;
			break;
		case PSP2_WINDOW_640X368:
			psSourceParams->ui32Width = 640;
			psSourceParams->ui32Height = 368;
			psSourceParams->ui32Stride = 640;
			break;
		case PSP2_WINDOW_720X408:
			psSourceParams->ui32Width = 720;
			psSourceParams->ui32Height = 408;
			psSourceParams->ui32Stride = 768;
			break;
		case PSP2_WINDOW_1280X725:
			psSourceParams->ui32Width = 1280;
			psSourceParams->ui32Height = 725;
			psSourceParams->ui32Stride = 1280;
			break;
		case PSP2_WINDOW_1920X1088:
			psSourceParams->ui32Width = 1920;
			psSourceParams->ui32Height = 1088;
			psSourceParams->ui32Stride = 1920;
			break;
		}

		sceClibMemcpy(psRenderParams, psSourceParams, sizeof(WSEGLDrawableParams));

		bufMemInfo = (PVRSRV_CLIENT_MEM_INFO *)window->apsSwapBufferMemInfo[window->currBufIdx];

		psRenderParams->pvLinearAddress = bufMemInfo->pvLinAddr;
		psRenderParams->ui32HWAddress = bufMemInfo->pvLinAddr;
		psRenderParams->hPrivateData = bufMemInfo;

		break;

	case PSP2_DRAWABLE_TYPE_PIXMAP:
		pixmap = (NativePixmapType)hDrawable;
		
		psSourceParams->ui32Width = pixmap->sizeX;
		psSourceParams->ui32Height = pixmap->sizeY;
		psSourceParams->ui32Stride = pixmap->sizeX;
		psSourceParams->ePixelFormat = WSEGL_PIXELFORMAT_8888;
		psSourceParams->pvLinearAddress = pixmap->memBase;
		psSourceParams->ui32HWAddress = pixmap->memBase;

		sceClibMemcpy(psRenderParams, psSourceParams, sizeof(WSEGLDrawableParams));

		break;

	default:
		return WSEGL_BAD_DRAWABLE;
	}

	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_ConnectDrawable(WSEGLDrawableHandle hDrawable)
{
	PVR_DPF((PVR_DBG_ERROR, "%s: Unimplemented", __func__));
	return WSEGL_SUCCESS;
}

static WSEGLError WSEGL_DisconnectDrawable(WSEGLDrawableHandle hDrawable)
{
	PVR_DPF((PVR_DBG_ERROR, "%s: Unimplemented", __func__));
	return WSEGL_SUCCESS;
}

static const WSEGL_FunctionTable sFunctionTable =
{
	WSEGL_VERSION,
	WSEGL_IsDisplayValid,
	WSEGL_InitialiseDisplay,
	WSEGL_CloseDisplay,
	WSEGL_CreateWindowDrawable,
	WSEGL_CreatePixmapDrawable,
	WSEGL_DeleteDrawable,
	WSEGL_SwapDrawable,
	WSEGL_SwapControlInterval,
	WSEGL_WaitNative,
	WSEGL_CopyFromDrawable,
	WSEGL_CopyFromPBuffer,
	WSEGL_GetDrawableParameters,
	WSEGL_ConnectDrawable,
	WSEGL_DisconnectDrawable,
};

WSEGL_EXPORT const WSEGL_FunctionTable *WSEGL_GetFunctionTablePointer(void)
{
	return &sFunctionTable;
}

int __module_stop(SceSize argc, const void *args)
{
	return SCE_KERNEL_STOP_SUCCESS;
}

int __module_exit()
{
	return SCE_KERNEL_STOP_SUCCESS;
}

int __module_start(SceSize argc, void *args)
{
	return SCE_KERNEL_START_SUCCESS;
}