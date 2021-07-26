/******************************************************************************
 * Name         : pixeventpbesetup.c
 *
 * Copyright    : 2009-2010 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: pixeventpbesetup.c $
 *****************************************************************************/

#include "img_defs.h"
#include "sgxdefs.h"
#include "servicesext.h"
#include "img_3dtypes.h"

#include "sgxpixfmts.h"
#include "pixeventpbesetup.h"

/*
 * WritePBEEmitState is designed to set up the PBE state words
 * required for calling WriteEndOfTileUSSECode
 */
#if defined(SGX543) || defined(SGX544) || defined(SGX554)
#define EMIT_SRC0LO 0
#define EMIT_SRC0HI	1
#define EMIT_SRC1LO	2
#define EMIT_SRC1HI	3
#define EMIT_SRC2LO	4
#define EMIT_SRC2HI	5
#define EMIT_SIDEBAND 6
#else
#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
#define EMIT1_SRC0	0
#define EMIT1_SRC1	1
#define EMIT2_SRC0	2
#define EMIT2_SRC1	3
#define EMIT2_SRC2	4
#define EMIT2_SIDEBAND	5
#else
#define EMIT1_SRC0	0
#define EMIT1_SRC1	1
#define EMIT2_SRC0	2
#define EMIT2_SRC1	3
#define EMIT2_SIDEBAND	4
#endif
#endif

#if !defined(SGX545)
/***********************************************************************************
 Function Name      : FloorLog2
 Inputs             : ui32Val
 Outputs            : 
 Returns            : Floor(Log2(ui32Val))
 Description        : Computes the floor of the log base 2 of a unsigned integer - 
					  used mostly for computing log2(2^ui32Val).
***********************************************************************************/
static IMG_UINT32 FloorLog2(IMG_UINT32 ui32Val)
{
	IMG_UINT32 ui32Ret = 0;

	while (ui32Val >>= 1)
	{
		ui32Ret++;
	}

	return ui32Ret;
}
#endif /* !defined(SGX545) */



/***********************************************************************//**
* Write the PBE state words in the correct order for setting up the PBE
* through the common EndOfTile setup code
*
* @param psSurfParams		Static parameters based on the surface being
*							rendered to
* @param psRenderParams		Parameters that may vary per render
**************************************************************************/
IMG_INTERNAL IMG_VOID WritePBEEmitState (PBE_SURF_PARAMS *psSurfParams,
										 PBE_RENDER_PARAMS *psRenderParams,
										 IMG_PUINT32 pui32Base)
{
	IMG_UINT32 ui32MemLayout;
	IMG_UINT32 ui32Rotation;
	IMG_UINT32 ui32Scaling;
	IMG_UINT32 ui32PBEFmt;
	IMG_UINT32 ui32Count;

	if(psSurfParams->bZOnlyRender)
	{
#if defined(FIX_HW_BRN_26922)
		ui32Count =	1;
#else
		ui32Count =	0;
#endif
	}
	else
	{
		ui32Count =	(EURASIA_ISPREGION_SIZEX * EURASIA_ISPREGION_SIZEY) / EURASIA_TAPDSSTATE_PIPECOUNT;
	}

	ui32PBEFmt = asSGXPixelFormat[psSurfParams->eFormat].ui32PBEFormat;

#if defined (FIX_HW_BRN_26352)    
	{
		IMG_UINT32 ui32WidthDiff = ((psRenderParams->ui32MaxXClip + EURASIA_ISPREGION_SIZEX) & ~(EURASIA_ISPREGION_SIZEX - 1)) - 
									(psRenderParams->ui32MaxXClip + 1);  
		IMG_UINT32 ui32HeightDiff = ((psRenderParams->ui32MaxYClip + EURASIA_ISPREGION_SIZEY) & ~(EURASIA_ISPREGION_SIZEY - 1)) - 
									(psRenderParams->ui32MaxYClip + 1);
		IMG_UINT32 ui32BytesPerPixel = (asSGXPixelFormat[psSurfParams->eFormat].ui32TotalBitsPerPixel + 7) >> 3;
		IMG_UINT32 ui32PBEOffset = 0;

		switch (psRenderParams->eRotation)
		{
			case PVRSRV_ROTATE_90:
				ui32PBEOffset = (ui32HeightDiff * ui32BytesPerPixel) & ~3U;
				break;
			case PVRSRV_ROTATE_180:
				ui32PBEOffset = ((ui32WidthDiff * ui32BytesPerPixel) & ~3U) + 
								(ui32HeightDiff * ui32BytesPerPixel * psSurfParams->ui32LineStrideInPixels);
				break;
			case PVRSRV_ROTATE_270:
				ui32PBEOffset = ui32WidthDiff * ui32BytesPerPixel * psSurfParams->ui32LineStrideInPixels;
				break;
			default:
				break;
		}

		psSurfParams->sAddress.uiAddr -= ui32PBEOffset;
	}
#endif     


	/* Write each one in order */
#if defined(SGX545)
	/* Emit 1, Src 0 */
	pui32Base[EMIT1_SRC0] = psRenderParams->ui32MinXClip << EURASIA_PIXELBE1S0_XMIN_SHIFT |
		psRenderParams->ui32MinYClip << EURASIA_PIXELBE1S0_YMIN_SHIFT;

	/* Emit 1, Src 1 */
	pui32Base[EMIT1_SRC1] = psRenderParams->ui32MaxXClip << EURASIA_PIXELBE1S1_XMAX_SHIFT |
		psRenderParams->ui32MaxYClip << EURASIA_PIXELBE1S1_YMAX_SHIFT;

	/* Emit 2, Src 0 */
	switch (psRenderParams->eRotation)
	{
		case PVRSRV_ROTATE_0:
		case PVRSRV_FLIP_Y:
			ui32Rotation = EURASIA_PIXELBE2S0_ROTATE_0DEG << EURASIA_PIXELBE2S0_ROTATE_SHIFT;
			break;
		case PVRSRV_ROTATE_90:
			ui32Rotation = EURASIA_PIXELBE2S0_ROTATE_90DEG << EURASIA_PIXELBE2S0_ROTATE_SHIFT;
			break;
		case PVRSRV_ROTATE_180:
			ui32Rotation = EURASIA_PIXELBE2S0_ROTATE_180DEG << EURASIA_PIXELBE2S0_ROTATE_SHIFT;
			break;
		case PVRSRV_ROTATE_270:
			ui32Rotation = EURASIA_PIXELBE2S0_ROTATE_270DEG << EURASIA_PIXELBE2S0_ROTATE_SHIFT;
			break;
		default:
			ui32Rotation = EURASIA_PIXELBE2S0_ROTATE_0DEG << EURASIA_PIXELBE2S0_ROTATE_SHIFT;
			break;
	}

	switch (psSurfParams->eScaling)
	{
		case IMG_SCALING_NONE:
			ui32Scaling = EURASIA_PIXELBE2S0_SCALE_NONE <<
				EURASIA_PIXELBE2S0_SCALE_SHIFT;
			break;
		case IMG_SCALING_AA:
			ui32Scaling = EURASIA_PIXELBE2S0_SCALE_AA <<
				EURASIA_PIXELBE2S0_SCALE_SHIFT;
			break;
		case IMG_SCALING_UPSCALE:
			ui32Scaling = (IMG_UINT32) (EURASIA_PIXELBE2S0_SCALE_UPSCALE <<
										EURASIA_PIXELBE2S0_SCALE_SHIFT);
			break;
		case IMG_SCALING_UPAA:
			ui32Scaling = (IMG_UINT32) (EURASIA_PIXELBE2S0_SCALE_AAUPSCALE <<
										EURASIA_PIXELBE2S0_SCALE_SHIFT);
			break;
		default:
			ui32Scaling = EURASIA_PIXELBE2S0_SCALE_NONE <<
				EURASIA_PIXELBE2S0_SCALE_SHIFT;
			break;
	}


	pui32Base[EMIT2_SRC0] =  ui32Rotation |
		ui32Scaling |
		((psSurfParams->ui32LineStrideInPixels-1) >> EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT) <<
		EURASIA_PIXELBE2S0_LINESTRIDE_SHIFT |
		ui32Count << EURASIA_PIXELBE2S0_COUNT_SHIFT;

	/* Emit 2, Src 1 */
	pui32Base[EMIT2_SRC1] = (psSurfParams->sAddress.uiAddr >> EURASIA_PIXELBE_FBADDR_ALIGNSHIFT) <<
		EURASIA_PIXELBE2S1_FBADDR_SHIFT;

	/* Sideband */
	switch (psSurfParams->eMemLayout)
	{
		case IMG_MEMLAYOUT_HYBRIDTWIDDLED:
			ui32MemLayout = EURASIA_PIXELBE2SB_MEMLAYOUT_TWIDDLED << EURASIA_PIXELBE2SB_MEMLAYOUT_SHIFT;
			break;
		case IMG_MEMLAYOUT_STRIDED:
			ui32MemLayout = EURASIA_PIXELBE2SB_MEMLAYOUT_LINEAR << EURASIA_PIXELBE2SB_MEMLAYOUT_SHIFT;
			break;
		default:
			ui32MemLayout = EURASIA_PIXELBE2SB_MEMLAYOUT_LINEAR << EURASIA_PIXELBE2SB_MEMLAYOUT_SHIFT;
			break;
	}

	pui32Base[EMIT2_SIDEBAND] = ui32MemLayout |
		(ui32PBEFmt & ~EURASIA_PIXELBE2SB_PACKMODE_CLRMSK) |
		EURASIA_PIXELBE2SB_TILERELATIVE |
		(psSurfParams->bEnableDithering ? EURASIA_PIXELBE2SB_DITHER : 0) |
		(psSurfParams->bEnableGamma ? EURASIA_PIXELBE2SB_GAMMACORRECT : 0);

	switch (ui32PBEFmt)
	{
		case EURASIA_PIXELBE2SB_PACKMODE_PT1:
		case EURASIA_PIXELBE2SB_PACKMODE_PT2:
		case EURASIA_PIXELBE2SB_PACKMODE_PT4:
		case EURASIA_PIXELBE2SB_PACKMODE_PT8:
			pui32Base[EMIT2_SIDEBAND] |= psRenderParams->uSel.ui32DstOffset << EURASIA_PIXELBE2SB_SRCSEL_SHIFT;
			break;
		case EURASIA_PIXELBE2SB_PACKMODE_R5G6B5:
		case EURASIA_PIXELBE2SB_PACKMODE_A1R5G5B5:
		case EURASIA_PIXELBE2SB_PACKMODE_A4R4G4B4:
		case EURASIA_PIXELBE2SB_PACKMODE_A8R3G3B2:
		case EURASIA_PIXELBE2SB_PACKMODE_MONO16:
		case EURASIA_PIXELBE2SB_PACKMODE_MONO8:
		case EURASIA_PIXELBE2SB_PACKMODE_PBYTE:
		case EURASIA_PIXELBE2SB_PACKMODE_PWORD:
			pui32Base[EMIT2_SIDEBAND] |= psRenderParams->uSel.ui32SrcSelect << EURASIA_PIXELBE2SB_SRCSEL_SHIFT;
			break;
		default:
			break;
	}
#else /* SGX545 */

#if defined(SGX543) || defined(SGX544) || defined(SGX554)

	/* Src 0 Lo*/
	ui32PBEFmt &= ~(EURASIA_PIXELBES0LO_PACKMODE_CLRMSK & EURASIA_PIXELBES0LO_PMODEEXT_CLRMSK & EURASIA_PIXELBES0LO_SWIZ_CLRMSK);
	

	switch (psSurfParams->eMemLayout)
	{
		case IMG_MEMLAYOUT_TWIDDLED:
			ui32MemLayout = EURASIA_PIXELBES0LO_MEMLAYOUT_TWIDDLED << EURASIA_PIXELBES0LO_MEMLAYOUT_SHIFT;
			break;
		case IMG_MEMLAYOUT_STRIDED:
			ui32MemLayout = EURASIA_PIXELBES0LO_MEMLAYOUT_LINEAR << EURASIA_PIXELBES0LO_MEMLAYOUT_SHIFT;
			break;
		case IMG_MEMLAYOUT_TILED:
			ui32MemLayout = EURASIA_PIXELBES0LO_MEMLAYOUT_TILED << EURASIA_PIXELBES0LO_MEMLAYOUT_SHIFT;
			break;
		default:
			ui32MemLayout = EURASIA_PIXELBES0LO_MEMLAYOUT_LINEAR << EURASIA_PIXELBES0LO_MEMLAYOUT_SHIFT;
			break;
	}

	pui32Base[EMIT_SRC0LO] = ui32MemLayout	| ui32PBEFmt | (ui32Count << EURASIA_PIXELBES0LO_COUNT_SHIFT);

	switch (psRenderParams->eRotation)
	{
		case PVRSRV_ROTATE_0:
		case PVRSRV_FLIP_Y:
			ui32Rotation = EURASIA_PIXELBES0HI_ROTATE_0DEG << EURASIA_PIXELBES0HI_ROTATE_SHIFT;
			break;
		case PVRSRV_ROTATE_90:
			ui32Rotation = EURASIA_PIXELBES0HI_ROTATE_90DEG << EURASIA_PIXELBES0HI_ROTATE_SHIFT;
			break;
		case PVRSRV_ROTATE_180:
			ui32Rotation = EURASIA_PIXELBES0HI_ROTATE_180DEG << EURASIA_PIXELBES0HI_ROTATE_SHIFT;
			break;
		case PVRSRV_ROTATE_270:
			ui32Rotation = EURASIA_PIXELBES0HI_ROTATE_270DEG << EURASIA_PIXELBES0HI_ROTATE_SHIFT;
			break;
		default:
			ui32Rotation = EURASIA_PIXELBES0HI_ROTATE_0DEG << EURASIA_PIXELBES0HI_ROTATE_SHIFT;
			break;
	}

	/* Src 0 Hi */
	pui32Base[EMIT_SRC0HI] = ui32Rotation |
							(psSurfParams->sAddress.uiAddr >> EURASIA_PIXELBE_FBADDR_ALIGNSHIFT) <<
							EURASIA_PIXELBES0HI_FBADDR_SHIFT;;

	/* Src 1 Lo */
	pui32Base[EMIT_SRC1LO] = ((psSurfParams->ui32LineStrideInPixels-1) >> EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT) <<
							 EURASIA_PIXELBES1LO_LINESTRIDE_SHIFT;

	/* Src 2 Lo */
	if (psSurfParams->eMemLayout == IMG_MEMLAYOUT_TWIDDLED)
	{
		IMG_UINT32 ui32SizeInTilesX = (psRenderParams->ui32MaxXClip + EURASIA_PIXELBES2LO_XSIZE_ALIGN) >> EURASIA_PIXELBES2LO_XSIZE_ALIGNSHIFT;
		IMG_UINT32 ui32SizeInTilesY = (psRenderParams->ui32MaxYClip + EURASIA_PIXELBES2LO_YSIZE_ALIGN) >> EURASIA_PIXELBES2LO_YSIZE_ALIGNSHIFT;

		pui32Base[EMIT_SRC2LO] = (FloorLog2(ui32SizeInTilesX) << EURASIA_PIXELBES2LO_XSIZE_SHIFT) |
								 (FloorLog2(ui32SizeInTilesY) << EURASIA_PIXELBES2LO_YSIZE_SHIFT);
	}
	else
	{
		pui32Base[EMIT_SRC2LO] = 0;
	}


	pui32Base[EMIT_SRC2LO] |= (psRenderParams->ui32MinXClip << EURASIA_PIXELBES2LO_XMIN_SHIFT) |
							  (psRenderParams->ui32MinYClip << EURASIA_PIXELBES2LO_YMIN_SHIFT);


	/* Src 2 Hi */
	pui32Base[EMIT_SRC2HI] = (psRenderParams->ui32MaxXClip << EURASIA_PIXELBES2HI_XMAX_SHIFT) |
							 (psRenderParams->ui32MaxYClip << EURASIA_PIXELBES2HI_YMAX_SHIFT);


	switch (psSurfParams->eScaling)
	{
		default:
		case IMG_SCALING_NONE:
			ui32Scaling = EURASIA_PIXELBESB_SCALE_NONE << EURASIA_PIXELBESB_SCALE_SHIFT;
			break;
		case IMG_SCALING_AA:
			ui32Scaling = EURASIA_PIXELBESB_SCALE_AA << EURASIA_PIXELBESB_SCALE_SHIFT;
			break;
		case IMG_SCALING_UPSCALE:
			ui32Scaling = (IMG_UINT32) (EURASIA_PIXELBESB_SCALE_UPSCALE <<	EURASIA_PIXELBESB_SCALE_SHIFT);
			break;
		case IMG_SCALING_UPAA:
			ui32Scaling = (IMG_UINT32) (EURASIA_PIXELBESB_SCALE_AAUPSCALE << EURASIA_PIXELBESB_SCALE_SHIFT);
			break;
	}

	/* Sideband */
	pui32Base[EMIT_SIDEBAND] = ui32Scaling | EURASIA_PIXELBESB_TILERELATIVE |
								(psSurfParams->bEnableDithering ? EURASIA_PIXELBESB_DITHER : 0) |
								(psSurfParams->bEnableGamma ? EURASIA_PIXELBESB_GAMMACORRECT : 0);

	/* FIXME: deal with srcsel /dstoffset */

#else /* SGX543 || SGX544 || SGX554 */

	/* Emit 1, Src 0 */
	if (psSurfParams->eMemLayout == IMG_MEMLAYOUT_TWIDDLED)
	{
		IMG_UINT32 ui32SizeInTilesX = (psRenderParams->ui32MaxXClip + EURASIA_PIXELBE1S0_XSIZE_ALIGN) >> EURASIA_PIXELBE1S0_XSIZE_ALIGNSHIFT;
		IMG_UINT32 ui32SizeInTilesY = (psRenderParams->ui32MaxYClip + EURASIA_PIXELBE1S0_YSIZE_ALIGN) >> EURASIA_PIXELBE1S0_YSIZE_ALIGNSHIFT;

		pui32Base[EMIT1_SRC0] = (FloorLog2(ui32SizeInTilesX) << EURASIA_PIXELBE1S0_XSIZE_SHIFT) |
								(FloorLog2(ui32SizeInTilesY) << EURASIA_PIXELBE1S0_YSIZE_SHIFT);
	}
	else
	{
		pui32Base[EMIT1_SRC0] = 0;
	}
	pui32Base[EMIT1_SRC0] |= psRenderParams->ui32MinXClip << EURASIA_PIXELBE1S0_XMIN_SHIFT |
							 psRenderParams->ui32MinYClip << EURASIA_PIXELBE1S0_YMIN_SHIFT;

	/* Emit 1, Src 1 */
	pui32Base[EMIT1_SRC1] = psRenderParams->ui32MaxXClip << EURASIA_PIXELBE1S1_XMAX_SHIFT |
							psRenderParams->ui32MaxYClip << EURASIA_PIXELBE1S1_YMAX_SHIFT;

#if defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)
	pui32Base[EMIT1_SRC1]  |= EURASIA_PIXELBE1S1_NOADVANCE;
#endif 

	/* Emit 2, Src 0 */

	switch (psSurfParams->eMemLayout)
	{
		case IMG_MEMLAYOUT_TWIDDLED:
			ui32MemLayout = EURASIA_PIXELBE2S0_MEMLAYOUT_TWIDDLED << EURASIA_PIXELBE2S0_MEMLAYOUT_SHIFT;
			break;
		case IMG_MEMLAYOUT_TILED:
			ui32MemLayout = EURASIA_PIXELBE2S0_MEMLAYOUT_TILED << EURASIA_PIXELBE2S0_MEMLAYOUT_SHIFT;
			break;
		case IMG_MEMLAYOUT_STRIDED:
			ui32MemLayout = EURASIA_PIXELBE2S0_MEMLAYOUT_LINEAR << EURASIA_PIXELBE2S0_MEMLAYOUT_SHIFT;
			break;
		default:
			ui32MemLayout = EURASIA_PIXELBE2S0_MEMLAYOUT_LINEAR << EURASIA_PIXELBE2S0_MEMLAYOUT_SHIFT;
			break;
	}
	
	pui32Base[EMIT2_SRC0] =  (ui32PBEFmt & ~EURASIA_PIXELBE2S0_PACKMODE_CLRMSK) |
		ui32MemLayout |
#if !defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
		((psSurfParams->ui32LineStrideInPixels-1) >> EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT) <<
		EURASIA_PIXELBE2S0_LINESTRIDE_SHIFT |
#endif
		ui32Count << EURASIA_PIXELBE2S0_COUNT_SHIFT;
	
	/* Emit 2, Src 1 */
	switch (psRenderParams->eRotation)
	{
		case PVRSRV_ROTATE_0:
		case PVRSRV_FLIP_Y:
			ui32Rotation = EURASIA_PIXELBE2S1_ROTATE_0DEG << EURASIA_PIXELBE2S1_ROTATE_SHIFT;
			break;
		case PVRSRV_ROTATE_90:
			ui32Rotation = EURASIA_PIXELBE2S1_ROTATE_90DEG << EURASIA_PIXELBE2S1_ROTATE_SHIFT;
			break;
		case PVRSRV_ROTATE_180:
			ui32Rotation = EURASIA_PIXELBE2S1_ROTATE_180DEG << EURASIA_PIXELBE2S1_ROTATE_SHIFT;
			break;
		case PVRSRV_ROTATE_270:
			ui32Rotation = EURASIA_PIXELBE2S1_ROTATE_270DEG << EURASIA_PIXELBE2S1_ROTATE_SHIFT;
			break;
		default:
			ui32Rotation = EURASIA_PIXELBE2S1_ROTATE_0DEG << EURASIA_PIXELBE2S1_ROTATE_SHIFT;
			break;
	}

	pui32Base[EMIT2_SRC1] = (psSurfParams->sAddress.uiAddr >> EURASIA_PIXELBE_FBADDR_ALIGNSHIFT) <<
		EURASIA_PIXELBE2S1_FBADDR_SHIFT |
		ui32Rotation;

	/* Emit 2, Src 2 */
#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
	pui32Base[EMIT2_SRC2] = ((psSurfParams->ui32LineStrideInPixels-1) >> EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT) <<
							EURASIA_PIXELBE2S2_LINESTRIDE_SHIFT;
#endif

	/* Sideband */
	switch (psSurfParams->eScaling)
	{
		case IMG_SCALING_NONE:
			ui32Scaling = EURASIA_PIXELBE2SB_SCALE_NONE <<
				EURASIA_PIXELBE2SB_SCALE_SHIFT;
			break;
		case IMG_SCALING_AA:
			ui32Scaling = EURASIA_PIXELBE2SB_SCALE_AA <<
				EURASIA_PIXELBE2SB_SCALE_SHIFT;
			break;
		case IMG_SCALING_UPSCALE:
			ui32Scaling = EURASIA_PIXELBE2SB_SCALE_UPSCALE <<
				EURASIA_PIXELBE2SB_SCALE_SHIFT;
			break;
		case IMG_SCALING_UPAA:
			ui32Scaling = EURASIA_PIXELBE2SB_SCALE_AAUPSCALE <<
				EURASIA_PIXELBE2SB_SCALE_SHIFT;
			break;
		default:
			ui32Scaling = EURASIA_PIXELBE2SB_SCALE_NONE <<
				EURASIA_PIXELBE2SB_SCALE_SHIFT;
			break;
	}

	pui32Base[EMIT2_SIDEBAND] = ui32Scaling |
		EURASIA_PIXELBE2SB_TILERELATIVE |
		(psSurfParams->bEnableDithering ? EURASIA_PIXELBE2SB_DITHER : 0);

#if defined(FIX_HW_BRN_26922)
	if(psSurfParams->bZOnlyRender)
	{
		pui32Base[EMIT2_SIDEBAND] &= ~EURASIA_PIXELBE2SB_TILERELATIVE;
	}

#endif

	switch (ui32PBEFmt)
	{
		case EURASIA_PIXELBE2S0_PACKMODE_PT1:
		case EURASIA_PIXELBE2S0_PACKMODE_PT2:
		case EURASIA_PIXELBE2S0_PACKMODE_PT4:
		case EURASIA_PIXELBE2S0_PACKMODE_PT8:
			pui32Base[EMIT2_SIDEBAND] |= psRenderParams->uSel.ui32DstOffset << EURASIA_PIXELBE2SB_SRCSEL_SHIFT;
			break;
		case EURASIA_PIXELBE2S0_PACKMODE_R5G6B5:
		case EURASIA_PIXELBE2S0_PACKMODE_A1R5G5B5:
		case EURASIA_PIXELBE2S0_PACKMODE_A4R4G4B4:
		case EURASIA_PIXELBE2S0_PACKMODE_A8R3G3B2:
		case EURASIA_PIXELBE2S0_PACKMODE_MONO16:
		case EURASIA_PIXELBE2S0_PACKMODE_MONO8:
		case EURASIA_PIXELBE2S0_PACKMODE_PBYTE:
		case EURASIA_PIXELBE2S0_PACKMODE_PWORD:
			pui32Base[EMIT2_SIDEBAND] |= psRenderParams->uSel.ui32SrcSelect << EURASIA_PIXELBE2SB_SRCSEL_SHIFT;
			break;
		default:
			break;
	}
#endif /* SGX543 || SGX544 || SGX554 */
#endif /* SGX545 */
}
