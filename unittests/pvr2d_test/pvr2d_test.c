#include <kernel.h>
#include <stdio.h>

#include "img_defs.h"

#include <pvr2d.h>

SCE_USER_MODULE_LIST("app0:libgpu_es4_ext.suprx", "app0:libpvr2d.suprx");

#define BLIT_BPP 4
#define WAIT_BETWEEN_SWAPS_SEC 5

#undef DPF
#define DPF printf

#ifndef EXIT_FAILURE
#define EXIT_FAILURE              (1)
#endif

#define FAIL_IF_NO_ERROR(val)					\
		if (val == PVR2D_OK)					\
				{DPF(" FAIL\n"); exit(-1);}		\
		else {DPF(" OK\n");}

#define FAIL_IF_ERROR(val)								\
		if (val!=PVR2D_OK)								\
		{												\
			DPF(" FAIL - ");							\
			PrintError(val);							\
			DPF("\n");									\
			exit(-1);									\
		}												\
		else {DPF(" OK\n");}

static PVR2D_VOID PrintError(PVR2DERROR eResult)
{
	switch (eResult)
	{
	case PVR2DERROR_INVALID_PARAMETER:
		DPF("PVR2DERROR_INVALID_PARAMETER");
		break;
	case PVR2DERROR_DEVICE_UNAVAILABLE:
		DPF("PVR2DERROR_DEVICE_UNAVAILABLE");
		break;
	case PVR2DERROR_INVALID_CONTEXT:
		DPF("PVR2DERROR_INVALID_CONTEXT");
		break;
	case PVR2DERROR_MEMORY_UNAVAILABLE:
		DPF("PVR2DERROR_MEMORY_UNAVAILABLE");
		break;
	case PVR2DERROR_DEVICE_NOT_PRESENT:
		DPF("PVR2DERROR_DEVICE_NOT_PRESENT");
		break;
	case PVR2DERROR_IOCTL_ERROR:
		DPF("PVR2DERROR_IOCTL_ERROR");
		break;
	case PVR2DERROR_GENERIC_ERROR:
		DPF("PVR2DERROR_GENERIC_ERROR");
		break;
	case PVR2DERROR_BLT_NOTCOMPLETE:
		DPF("PVR2DERROR_BLT_NOTCOMPLETE");
		break;
	case PVR2DERROR_HW_FEATURE_NOT_SUPPORTED:
		DPF("PVR2DERROR_HW_FEATURE_NOT_SUPPORTED");
		break;
	case PVR2DERROR_NOT_YET_IMPLEMENTED:
		DPF("PVR2DERROR_NOT_YET_IMPLEMENTED");
		break;
	case PVR2DERROR_MAPPING_FAILED:
		DPF("PVR2DERROR_MAPPING_FAILED");
		break;
	default:
		DPF("unrecognized error 0x%08X", eResult);
		break;
	}
}

static PVR2DERROR LoadExternalImage(PVR2DCONTEXTHANDLE hContext, IMG_PCHAR pszFilename, PVR2DMEMINFO **ppsMemInfo)
{
	FILE *fd;
	IMG_PVOID pvBuffer;
	IMG_UINT32 ui32Size;
	PVR2DERROR eResult = PVR2D_OK;

	DPF("Loading external image\n");

	fd = fopen(pszFilename, "rb");
	if (fd == IMG_NULL)
	{
		DPF("Failed to open file %s!", pszFilename);
		return PVR2DERROR_GENERIC_ERROR;
	}

	fseek(fd, 0, SEEK_END);
	ui32Size = ftell(fd);
	fseek(fd, 0, SEEK_SET);

	DPF("Try calling PVR2DMemAlloc to allopcate memory with size 0x%X\n", ui32Size);

	eResult = PVR2DMemAlloc(hContext, ui32Size, 0x10, PVR2D_PSP2_MEM_MAIN, ppsMemInfo);
	if (eResult != PVR2D_OK)
	{
		DPF("PVR2DMemAlloc failed to allocate memory with error %d", eResult);
		goto close;
	}

	fread((*ppsMemInfo)->pBase, 1, ui32Size, fd);

close:
	fclose(fd);

	return eResult;
}

int main(int argc, char ** argv)
{
	PVR2DERROR eError;
	PVR2D_ULONG lRevMajor;
	PVR2D_ULONG lRevMinor;
	PVR2D_ULONG ulFlipChainId;
	PVR2D_ULONG ulBufNum;
	PVR2D_LONG lDispWidth;
	PVR2D_LONG lDispHeight;
	PVR2D_LONG lDispStride;
	PVR2D_LONG lUnused;
	PVR2D_INT devNum;
	IMG_SID hProcRef;
	PVR2DDEVICEINFO *pDevInfo;
	PVR2DCONTEXTHANDLE hPvr2dCtx;
	PVR2DMISCDISPLAYINFO sMiscDispInfo;
	PVR2DFLIPCHAINHANDLE hFlipChainHandle;
	PVR2DMEMINFO *psBufMemInfo[10];
	PVR2DFORMAT eDispForm;
	PVR2DBLTINFO sBlitInfo;

	DPF("----------------------- Start -----------------------\n");

	DPF("Try calling PVR2DGetAPIRev\n");

	eError = PVR2DGetAPIRev(&lRevMajor, &lRevMinor);

	FAIL_IF_ERROR(eError);

	DPF("PVR2D API Revision: %u.%u\n", lRevMajor, lRevMinor);

	DPF("Try calling PVR2DEnumerateDevices to get number of devices\n");

	devNum = PVR2DEnumerateDevices(0);
	if (devNum < 1)
	{
		DPF("PVR2DEnumerateDevices(0) returned %d!\n", devNum);
		FAIL_IF_ERROR(-1);
	}

	FAIL_IF_ERROR(PVR2D_OK);

	DPF("Total device num: %d\n", devNum);

	DPF("Try allocating memory for device info\n");

	pDevInfo = (PVR2DDEVICEINFO *)malloc(devNum * sizeof(PVR2DDEVICEINFO));

	if (!pDevInfo)
	{
		DPF("malloc failed\n");
		FAIL_IF_ERROR(-1);
	}

	FAIL_IF_ERROR(PVR2D_OK);

	DPF("Try calling PVR2DEnumerateDevices to get device info\n");

	eError = PVR2DEnumerateDevices(pDevInfo);

	FAIL_IF_ERROR(eError);

	for (PVR2D_INT i = 0; i < devNum; i++)
	{
		DPF("Device num: %d\n", i);
		DPF("Device name: %s\n", pDevInfo[i].szDeviceName);
		DPF("Device id: 0x%X\n\n", pDevInfo[i].ulDevID);
	}

	DPF("Try creating context for device 0x%X\n", pDevInfo[0].ulDevID);

	eError = PVR2DCreateDeviceContext(pDevInfo[0].ulDevID, &hPvr2dCtx, 0);

	FAIL_IF_ERROR(eError);

	DPF("Try calling PVR2DGetProcRefIdPSP2\n");

	hProcRef = PVR2DGetProcRefIdPSP2();

	DPF("Driver process reference ID: %d\n", hProcRef);

	DPF("Try calling PVR2DGetMiscDisplayInfo\n");

	eError = PVR2DGetMiscDisplayInfo(hPvr2dCtx, &sMiscDispInfo);

	DPF("Display physical dimensions in mm: %ux%u\n", sMiscDispInfo.ulPhysicalWidthmm, sMiscDispInfo.ulPhysicalHeightmm);

	FAIL_IF_ERROR(eError);

	DPF("Try calling PVR2DCreateFlipChain\n");

	eError = PVR2DCreateFlipChain(hPvr2dCtx, 0, 2, 960, 544, PVR2D_ARGB8888, &lDispStride, &ulFlipChainId, &hFlipChainHandle);

	FAIL_IF_ERROR(eError);

	DPF("Created flip chain with ID: 0x%X\n", ulFlipChainId);

	DPF("Try calling PVR2DGetScreenMode\n");

	eError = PVR2DGetScreenMode(hPvr2dCtx, &eDispForm, &lDispWidth, &lDispHeight, &lDispStride, &lUnused);

	FAIL_IF_ERROR(eError);

	DPF("Current display format: %d\n", eDispForm);
	DPF("Current display width: %d\n", lDispWidth);
	DPF("Current display height: %d\n", lDispHeight);
	DPF("Current display stride: %d\n", lDispStride);

	DPF("Try calling PVR2DGetFlipChainBuffers to get number of buffers\n");

	eError = PVR2DGetFlipChainBuffers(hPvr2dCtx, hFlipChainHandle, &ulBufNum, IMG_NULL);

	FAIL_IF_ERROR(eError);

	if (ulBufNum < 2)
	{
		DPF("PVR2DGetFlipChainBuffers returned invalid buffer number\n");
		FAIL_IF_ERROR(-1);
	}

	DPF("Total buffer num: %u\n", ulBufNum);

	DPF("Try calling PVR2DGetFlipChainBuffers to get buffer info\n");

	eError = PVR2DGetFlipChainBuffers(hPvr2dCtx, hFlipChainHandle, &ulBufNum, &psBufMemInfo);

	FAIL_IF_ERROR(eError);

	for (PVR2D_INT i = 0; i < ulBufNum; i++)
	{
		DPF("Buffer num: %d\n", i);
		DPF("Buffer memory base: 0x%X\n", psBufMemInfo[i]->pBase);
		DPF("Buffer memory size: 0x%X\n", psBufMemInfo[i]->ui32MemSize);
		DPF("Buffer memory flags: 0x%X\n", psBufMemInfo[i]->ulFlags);
	}

	DPF("Try waiting on PVR2DQueryBlitsComplete without transfer OPs\n");

	eError = PVR2DQueryBlitsComplete(hPvr2dCtx, psBufMemInfo[0], IMG_TRUE);

	if (eError != PVR2D_OK)
	{
		DPF("Result: ");
		PrintError(eError);
		DPF("\n");
	}

	DPF("Prerotate through display buffers\n");

	eError = PVR2DPresentFlip(hPvr2dCtx, hFlipChainHandle, psBufMemInfo[0], 0);

	FAIL_IF_ERROR(eError);

	eError = PVR2DPresentFlip(hPvr2dCtx, hFlipChainHandle, psBufMemInfo[1], 0);

	FAIL_IF_ERROR(eError);

	DPF("----------------------- fill op begin -----------------------\n");

	DPF("Try waiting on PVR2DQueryBlitsComplete without transfer OPs\n");

	eError = PVR2DQueryBlitsComplete(hPvr2dCtx, psBufMemInfo[0], IMG_TRUE);

	if (eError != PVR2D_OK)
	{
		DPF("Result: ");
		PrintError(eError);
		DPF("\n");
	}

	memset(&sBlitInfo, 0, sizeof(PVR2DBLTINFO));

	sBlitInfo.CopyCode = PVR2DPATROPcopy;
	sBlitInfo.BlitFlags = PVR2D_BLIT_PATH_2DCORE;
	sBlitInfo.Colour = 0xFFFF0000; // Solid Blue
	sBlitInfo.SrcFormat = PVR2D_ABGR8888;
	sBlitInfo.DstFormat = PVR2D_ABGR8888;

	sBlitInfo.pSrcMemInfo = psBufMemInfo[0];
	sBlitInfo.SrcOffset = 0;
	sBlitInfo.SrcStride = 960 * BLIT_BPP;
	sBlitInfo.pDstMemInfo = psBufMemInfo[0];
	sBlitInfo.DstOffset = 0;
	sBlitInfo.DstStride = 960 * BLIT_BPP;

	sBlitInfo.SrcX = 960 / 2;
	sBlitInfo.SrcY = 544 / 2;
	sBlitInfo.DstX = 960 / 2;
	sBlitInfo.DstY = 544 / 2;
	sBlitInfo.SizeX = 960 / 2;
	sBlitInfo.SizeY = 544 / 2;
	sBlitInfo.DSizeX = 960 / 2;
	sBlitInfo.DSizeY = 544 / 2;
	sBlitInfo.SrcSurfWidth = 960;
	sBlitInfo.SrcSurfHeight = 544;
	sBlitInfo.DstSurfWidth = 960;
	sBlitInfo.DstSurfHeight = 544;

	DPF("Try PVR2DBlt on back buffer with color 0x%X\n", sBlitInfo.Colour);

	eError = PVR2DBlt(hPvr2dCtx, &sBlitInfo);

	FAIL_IF_ERROR(eError);

	memset(&sBlitInfo, 0, sizeof(PVR2DBLTINFO));

	sBlitInfo.CopyCode = PVR2DPATROPcopy;
	sBlitInfo.BlitFlags = PVR2D_BLIT_PATH_2DCORE;
	sBlitInfo.Colour = 0xFF0000FF; // Solid Red
	sBlitInfo.SrcFormat = PVR2D_ABGR8888;
	sBlitInfo.DstFormat = PVR2D_ABGR8888;

	sBlitInfo.pSrcMemInfo = psBufMemInfo[0];
	sBlitInfo.SrcOffset = 0;
	sBlitInfo.SrcStride = 960 * BLIT_BPP;
	sBlitInfo.pDstMemInfo = psBufMemInfo[0];
	sBlitInfo.DstOffset = 0;
	sBlitInfo.DstStride = 960 * BLIT_BPP;

	sBlitInfo.SrcX = 0;
	sBlitInfo.SrcY = 0;
	sBlitInfo.DstX = 0;
	sBlitInfo.DstY = 0;
	sBlitInfo.SizeX = 960 / 2;
	sBlitInfo.SizeY = 544 / 2;
	sBlitInfo.DSizeX = 960 / 2;
	sBlitInfo.DSizeY = 544 / 2;
	sBlitInfo.SrcSurfWidth = 960;
	sBlitInfo.SrcSurfHeight = 544;
	sBlitInfo.DstSurfWidth = 960;
	sBlitInfo.DstSurfHeight = 544;

	DPF("Try PVR2DBlt on back buffer with color 0x%X\n", sBlitInfo.Colour);

	eError = PVR2DBlt(hPvr2dCtx, &sBlitInfo);

	FAIL_IF_ERROR(eError);

	DPF("Try PVR2DQueryBlitsComplete\n");

	eError = PVR2DQueryBlitsComplete(hPvr2dCtx, psBufMemInfo[0], IMG_TRUE);

	if (eError != PVR2D_OK)
	{
		DPF("Result: ");
		PrintError(eError);
		DPF("\n");
	}

	DPF("Try to swap to filled display buffer\n");

	eError = PVR2DPresentFlip(hPvr2dCtx, hFlipChainHandle, psBufMemInfo[0], 0);

	FAIL_IF_ERROR(eError);

	DPF("Waiting %u seconds...\n", WAIT_BETWEEN_SWAPS_SEC);
	sceKernelDelayThread(WAIT_BETWEEN_SWAPS_SEC * 1000000);

	DPF("----------------------- rotate begin -----------------------\n");

	memset(&sBlitInfo, 0, sizeof(PVR2DBLTINFO));

	sBlitInfo.CopyCode = PVR2DROPcopy;
	sBlitInfo.BlitFlags = PVR2D_BLIT_PATH_2DCORE | PVR2D_BLIT_ROT_180;
	sBlitInfo.SrcFormat = PVR2D_ARGB8888;
	sBlitInfo.DstFormat = PVR2D_ARGB8888;

	sBlitInfo.pSrcMemInfo = psBufMemInfo[0];
	sBlitInfo.SrcOffset = 0;
	sBlitInfo.SrcStride = 960 * BLIT_BPP;
	sBlitInfo.pDstMemInfo = psBufMemInfo[1];
	sBlitInfo.DstOffset = 0;
	sBlitInfo.DstStride = 960 * BLIT_BPP;

	sBlitInfo.SrcX = 0;
	sBlitInfo.SrcY = 0;
	sBlitInfo.DstX = 0;
	sBlitInfo.DstY = 0;
	sBlitInfo.SizeX = 960;
	sBlitInfo.SizeY = 544;
	sBlitInfo.DSizeX = 960;
	sBlitInfo.DSizeY = 544;
	sBlitInfo.SrcSurfWidth = 960;
	sBlitInfo.SrcSurfHeight = 544;
	sBlitInfo.DstSurfWidth = 960;
	sBlitInfo.DstSurfHeight = 544;

	DPF("Try PVR2DBlt front buffer->back buffer copy with 180 PVR2D_BLIT_ROT_180\n", sBlitInfo.Colour);

	eError = PVR2DBlt(hPvr2dCtx, &sBlitInfo);

	FAIL_IF_ERROR(eError);

	DPF("Try PVR2DQueryBlitsComplete\n");

	eError = PVR2DQueryBlitsComplete(hPvr2dCtx, psBufMemInfo[1], IMG_TRUE);

	if (eError != PVR2D_OK)
	{
		DPF("Result: ");
		PrintError(eError);
		DPF("\n");
	}

	DPF("Try to swap to rotated display buffer\n");

	eError = PVR2DPresentFlip(hPvr2dCtx, hFlipChainHandle, psBufMemInfo[1], 0);

	FAIL_IF_ERROR(eError);

	DPF("Waiting %u seconds...\n", WAIT_BETWEEN_SWAPS_SEC);
	sceKernelDelayThread(WAIT_BETWEEN_SWAPS_SEC * 1000000);

	DPF("---------------------End test OK---------------------\n");
}