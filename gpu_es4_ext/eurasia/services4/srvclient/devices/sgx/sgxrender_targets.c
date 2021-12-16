/*!****************************************************************************
@File           render_targets.c
@Title          Render Target routines
@Platform       PSP2
@Description    Render Target functions
******************************************************************************/

#include <psp2_pvr_desc.h>
#include <sgxapi.h>
#include <sgx_mkif_km.h>
#include <sgx_mkif_client.h>
#include <servicesint.h>
#include <pvr_debug.h>

#define USE_GXM_RT_SIZE_CALC

#ifdef USE_GXM_RT_SIZE_CALC
#include <gxm.h>
#endif

#define SGX_MT_DEFAULT_THRESHOLD (640 * 480) // Threshold for 4x4 MT configuration

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))

#define INCR_MEMSIZE(memSize, size, alignment) \
 memSize = (ALIGN(memSize, PVRSRV_4K_PAGE_SIZE) - memSize) < (size + (memSize & (alignment - 1))) ? ALIGN(memSize, PVRSRV_4K_PAGE_SIZE) : ALIGN(memSize, alignment); \
 memSize += size

static IMG_UINT32 NextPowerOfTwo(IMG_UINT32 ui32Value) {
	ui32Value--;
	ui32Value |= ui32Value >> 1;
	ui32Value |= ui32Value >> 2;
	ui32Value |= ui32Value >> 4;
	ui32Value |= ui32Value >> 8;
	ui32Value |= ui32Value >> 16;
	ui32Value++;

	return ui32Value;
}

/*****************************************************************************
FUNCTION	: SGXGetRenderTargetDriverMemSize
PURPOSE	: Calculates size of required driver memory block.
PARAMETERS	: psAddRTInfo, pui32DriverMemSize
RETURNS	: PVRSRV_ERROR
 *****************************************************************************/
IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV
SGXGetRenderTargetMemSize(SGX_ADDRENDTARG *psAddRTInfo, IMG_UINT32 *pui32MemSize)
{
#ifdef USE_GXM_RT_SIZE_CALC
	SceGxmRenderTargetParams sRtTmpParam;

	if (psAddRTInfo->ui16MSAASamplesInX == 2)
	{
		sRtTmpParam.multisampleMode = SCE_GXM_MULTISAMPLE_4X;
	}
	else
	{
		sRtTmpParam.multisampleMode = SCE_GXM_MULTISAMPLE_NONE;
	}

	sRtTmpParam.driverMemBlock = SCE_UID_INVALID_UID;
	sRtTmpParam.flags = 0;
	sRtTmpParam.scenesPerFrame = 1;
	sRtTmpParam.width = psAddRTInfo->ui32NumPixelsX;
	sRtTmpParam.height = psAddRTInfo->ui32NumPixelsY;
	sRtTmpParam.multisampleLocations = 0;
	sceGxmGetRenderTargetMemSize(&sRtTmpParam, pui32MemSize);

	return PVRSRV_OK;

#else
	IMG_UINT32 ui32TilesInX, ui32TilesInY;
	IMG_UINT32 ui32MTilesInX, ui32MTilesInY;
	IMG_UINT32 ui32TilesPerMTileX, ui32TilesPerMTileY;
	IMG_UINT32 ui32TailPtrsSize;
	IMG_UINT32 ui32RgnSize;
	IMG_UINT32 ui32MacroTileRgnLUTSize;
	IMG_UINT32 ui32KernelDataMemSize[2] = { 0 }, ui323DParamMemSize[2] = { 0 }, ui32SyncDataMemSize = 0, ui32TADataMemSize = 0;
	IMG_UINT32 ui32MemSize = 0;
	IMG_INT i, j;

	if (psAddRTInfo == IMG_NULL || pui32MemSize == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR, "SGXGetRenderTargetMemSize: Invalid pointer"));
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	if (psAddRTInfo->ui32Flags & SGX_ADDRTFLAGS_CUSTOM_MACROTILE_COUNTS)
	{
		if (psAddRTInfo->ui8MacrotileCountX > 4 || psAddRTInfo->ui8MacrotileCountX < 1)
		{
			PVR_DPF((PVR_DBG_ERROR, "SGXGetRenderTargetMemSize: Macrotile count on the X axis (%d macrotiles) is not within the valid range [1, 4]", psAddRTInfo->ui8MacrotileCountX));
			return PVRSRV_ERROR_INVALID_PARAMS;
		}

		if (psAddRTInfo->ui8MacrotileCountY > 4 || psAddRTInfo->ui8MacrotileCountY < 1)
		{
			PVR_DPF((PVR_DBG_ERROR, "SGXGetRenderTargetMemSize: Macrotile count on the Y axis (%d macrotiles) is not within the valid range [1, 4]", psAddRTInfo->ui8MacrotileCountY));
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}

	ui32TilesInX = (psAddRTInfo->ui32NumPixelsX + (EURASIA_ISPREGION_SIZEX - 1)) / EURASIA_ISPREGION_SIZEX;
	ui32TilesInY = (psAddRTInfo->ui32NumPixelsY + (EURASIA_ISPREGION_SIZEY - 1)) / EURASIA_ISPREGION_SIZEY;

	if (psAddRTInfo->ui32Flags & SGX_ADDRTFLAGS_CUSTOM_MACROTILE_COUNTS)
	{
		ui32MTilesInX = psAddRTInfo->ui8MacrotileCountX;
		ui32MTilesInY = psAddRTInfo->ui8MacrotileCountY;

		if (psAddRTInfo->ui16MSAASamplesInX == 1)
		{
			ui32TilesPerMTileX = ALIGN((ui32TilesInX + (ui32MTilesInX - 1)) / ui32MTilesInX, 4);
		}
		else
		{
			ui32TilesPerMTileX = ALIGN((ui32TilesInX + (ui32MTilesInX - 1)) / ui32MTilesInX, 2);
		}
		if (psAddRTInfo->ui16MSAASamplesInY == 1)
		{
			ui32TilesPerMTileY = ALIGN((ui32TilesInY + (ui32MTilesInY - 1)) / ui32MTilesInY, 4);
		}
		else
		{
			ui32TilesPerMTileY = ALIGN((ui32TilesInY + (ui32MTilesInY - 1)) / ui32MTilesInY, 2);
		}

		if ((ui32MTilesInX > 2) || (ui32MTilesInY > 2))
		{
			ui32MTilesInX = ui32MTilesInY = 4;
		}
		else
		{
			ui32MTilesInX = ui32MTilesInY = 2;
		}
	}
	else
	{
		if ((psAddRTInfo->ui32NumPixelsX * psAddRTInfo->ui32NumPixelsY) > SGX_MT_DEFAULT_THRESHOLD)
		{
			ui32MTilesInX = ui32MTilesInY = 4;

			if (psAddRTInfo->ui16MSAASamplesInX == 1)
			{
				ui32TilesPerMTileX = ALIGN((ui32TilesInX + 3) / 4, 4);
			}
			else
			{
				ui32TilesPerMTileX = ALIGN((ui32TilesInX + 3) / 4, 2);
			}
			if (psAddRTInfo->ui16MSAASamplesInY == 1)
			{
				ui32TilesPerMTileY = ALIGN((ui32TilesInY + 3) / 4, 4);
			}
			else
			{
				ui32TilesPerMTileY = ALIGN((ui32TilesInY + 3) / 4, 2);
			}
		}
		else
		{
			ui32MTilesInX = ui32MTilesInY = 2;

			if (psAddRTInfo->ui16MSAASamplesInX == 1)
			{
				ui32TilesPerMTileX = ALIGN((ui32TilesInX + 1) / 2, 4);
			}
			else
			{
				ui32TilesPerMTileX = ALIGN((ui32TilesInX + 1) / 2, 2);
			}
			if (psAddRTInfo->ui16MSAASamplesInY == 1)
			{
				ui32TilesPerMTileY = ALIGN((ui32TilesInY + 1) / 2, 4);
			}
			else
			{
				ui32TilesPerMTileY = ALIGN((ui32TilesInY + 1) / 2, 2);
			}
		}
	}

	ui32TilesPerMTileX *= psAddRTInfo->ui16MSAASamplesInX;
	ui32TilesPerMTileY *= psAddRTInfo->ui16MSAASamplesInY;

	for (i = 0; i < psAddRTInfo->ui32MaxQueuedRenders; i++)
	{
		INCR_MEMSIZE(ui32KernelDataMemSize[0], sizeof(SGXMKIF_HWRENDERDETAILS), 32);
		INCR_MEMSIZE(ui32SyncDataMemSize, sizeof(PVRSRV_RESOURCE), 4);

		INCR_MEMSIZE(ui32KernelDataMemSize[0], sizeof(SGXMKIF_HWDEVICE_SYNC_LIST) + sizeof(PVRSRV_DEVICE_SYNC_OBJECT), 32);
		INCR_MEMSIZE(ui32SyncDataMemSize, sizeof(PVRSRV_RESOURCE), 4);
	}

	INCR_MEMSIZE(ui32KernelDataMemSize[1], sizeof(IMG_UINT32), 32);
	INCR_MEMSIZE(ui32KernelDataMemSize[1], sizeof(SGXMKIF_HWRTDATASET) + sizeof(SGXMKIF_HWRTDATA) * (psAddRTInfo->ui32NumRTData - 1), 32);

	ui32TailPtrsSize = MAX(NextPowerOfTwo(ui32MTilesInX * ui32TilesPerMTileX), NextPowerOfTwo(ui32MTilesInY * ui32TilesPerMTileY));
	ui32TailPtrsSize = ui32TailPtrsSize * ui32TailPtrsSize * EURASIA_TAILPOINTER_SIZE;

	for (i = 0; i < SGX_FEATURE_MP_CORE_COUNT_TA; i++)
	{
		INCR_MEMSIZE(ui32TADataMemSize, ui32TailPtrsSize, EURASIA_PARAM_MANAGE_TPC_GRAN);
		INCR_MEMSIZE(ui32TADataMemSize, EURASIA_PARAM_MANAGE_CONTROL_SIZE, EURASIA_PARAM_MANAGE_CONTROL_GRAN);
		INCR_MEMSIZE(ui32TADataMemSize, EURASIA_PARAM_MANAGE_OTPM_SIZE, EURASIA_PARAM_MANAGE_OTPM_GRAN);
	}

	INCR_MEMSIZE(ui323DParamMemSize[0], SGX_SPECIALOBJECT_SIZE, EURASIA_PARAM_MANAGE_GRAN);
	INCR_MEMSIZE(ui323DParamMemSize[1], EURASIA_PARAM_MANAGE_PIM_SIZE, EURASIA_PARAM_MANAGE_PIM_GRAN);

	ui32RgnSize = (ui32MTilesInX * ui32TilesPerMTileX) * (ui32MTilesInY * ui32TilesPerMTileY) * EURASIA_REGIONHEADER_SIZE;

	ui32MacroTileRgnLUTSize = ((ui32MTilesInX * ui32MTilesInY) * sizeof(IMG_UINT32)) * SGX_FEATURE_MP_CORE_COUNT_TA;

	for (i = 0; i < psAddRTInfo->ui32NumRTData; i++)
	{
		for (j = 0; j < SGX_FEATURE_MP_CORE_COUNT_TA; j++)
		{
			INCR_MEMSIZE(ui32TADataMemSize, ui32RgnSize, EURASIA_PARAM_MANAGE_REGION_GRAN);
		}
		INCR_MEMSIZE(ui32TADataMemSize, EURASIA_PARAM_MANAGE_STATE_SIZE, EURASIA_PARAM_MANAGE_STATE_GRAN);
		INCR_MEMSIZE(ui32KernelDataMemSize[1], ui32MacroTileRgnLUTSize, 32);
	}

	for (int i = 0; i < 2; i++)
	{
		ui32MemSize += ALIGN(ui323DParamMemSize[i], PVRSRV_4K_PAGE_SIZE);
		ui32MemSize += ALIGN(ui32KernelDataMemSize[i], PVRSRV_4K_PAGE_SIZE);
	}

	ui32MemSize += ALIGN(ui32SyncDataMemSize, PVRSRV_4K_PAGE_SIZE);
	ui32MemSize += ALIGN(ui32TADataMemSize, PVRSRV_4K_PAGE_SIZE);

	*pui32MemSize = ui32MemSize;

	return PVRSRV_OK;
#endif
}