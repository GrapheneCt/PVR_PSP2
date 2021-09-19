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

#define SGX_MT_DEFAULT_THRESHOLD (640 * 480) // Threshold for 4x4 MT configuration

#define ALIGN(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))

#define INCR_MEMSIZE(size, alignment) ui32MemSize = ALIGN(ui32MemSize, alignment); \
 ui32MemSize = (ALIGN(ui32MemSize, PVRSRV_4K_PAGE_SIZE) - ui32MemSize) < size ? ALIGN(ui32MemSize, PVRSRV_4K_PAGE_SIZE) : ui32MemSize; \
 ui32MemSize += size

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
	IMG_UINT32 ui32TilesInX, ui32TilesInY;
	IMG_UINT32 ui32MTilesInX, ui32MTilesInY;
	IMG_UINT32 ui32TilesPerMTileX, ui32TilesPerMTileY;
	IMG_UINT32 ui32MemSize = 0;
	IMG_UINT32 ui32TailPtrsSize;
	IMG_UINT32 ui32RgnSize;
	IMG_UINT32 ui32MacroTileRgnLUTSize;
	IMG_INT i, j;

	if (psAddRTInfo == IMG_NULL || pui32MemSize == IMG_NULL)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

	ui32TilesInX = (psAddRTInfo->ui32NumPixelsX + (EURASIA_ISPREGION_SIZEX - 1)) / EURASIA_ISPREGION_SIZEX;
	ui32TilesInY = (psAddRTInfo->ui32NumPixelsY + (EURASIA_ISPREGION_SIZEY - 1)) / EURASIA_ISPREGION_SIZEY;

	if (psAddRTInfo->ui32Flags & 0x00040000) // SGX_ADDRTFLAGS_CUSTOM_MACROTILE_COUNTS
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
		INCR_MEMSIZE(sizeof(SGXMKIF_HWRENDERDETAILS), 32);
		INCR_MEMSIZE(sizeof(PVRSRV_RESOURCE), 4);

		INCR_MEMSIZE(sizeof(SGXMKIF_HWDEVICE_SYNC_LIST), 32);
		INCR_MEMSIZE(sizeof(PVRSRV_RESOURCE), 4);
	}

	INCR_MEMSIZE(sizeof(IMG_UINT32), 32);
	INCR_MEMSIZE(sizeof(SGXMKIF_HWRTDATASET) + sizeof(SGXMKIF_HWRTDATA) * (psAddRTInfo->ui32NumRTData - 1), 32);

	ui32TailPtrsSize = MAX(NextPowerOfTwo(ui32MTilesInX * ui32TilesPerMTileX), NextPowerOfTwo(ui32MTilesInY * ui32TilesPerMTileY));
	ui32TailPtrsSize = ui32TailPtrsSize * ui32TailPtrsSize * EURASIA_TAILPOINTER_SIZE;

	for (i = 0; i < SGX_FEATURE_MP_CORE_COUNT_TA; i++)
	{
		INCR_MEMSIZE(ui32TailPtrsSize, EURASIA_PARAM_MANAGE_TPC_GRAN);
		INCR_MEMSIZE(EURASIA_PARAM_MANAGE_CONTROL_SIZE, EURASIA_PARAM_MANAGE_CONTROL_GRAN);
		INCR_MEMSIZE(EURASIA_PARAM_MANAGE_OTPM_SIZE, EURASIA_PARAM_MANAGE_OTPM_GRAN);
	}

	INCR_MEMSIZE(SGX_SPECIALOBJECT_SIZE, EURASIA_PARAM_MANAGE_GRAN);
	INCR_MEMSIZE(EURASIA_PARAM_MANAGE_PIM_SIZE, EURASIA_PARAM_MANAGE_PIM_GRAN);

	ui32RgnSize = (ui32MTilesInX * ui32TilesPerMTileX) * (ui32MTilesInY * ui32TilesPerMTileY) * EURASIA_REGIONHEADER_SIZE;
	ui32RgnSize = ALIGN(ui32RgnSize, 1 << 8);

	ui32MacroTileRgnLUTSize = ((ui32MTilesInX * ui32MTilesInY) * sizeof(IMG_UINT32)) * SGX_FEATURE_MP_CORE_COUNT_TA;

	for (i = 0; i < psAddRTInfo->ui32NumRTData; i++)
	{
		for (j = 0; j < SGX_FEATURE_MP_CORE_COUNT_TA; j++)
		{
			INCR_MEMSIZE(ui32RgnSize, EURASIA_PARAM_MANAGE_REGION_GRAN);
		}
		INCR_MEMSIZE(EURASIA_PARAM_MANAGE_STATE_SIZE, EURASIA_PARAM_MANAGE_STATE_GRAN);
		INCR_MEMSIZE(ui32MacroTileRgnLUTSize, 32);
	}

	*pui32MemSize = ALIGN(ui32MemSize, PVRSRV_4K_PAGE_SIZE);

	return PVRSRV_OK;
}