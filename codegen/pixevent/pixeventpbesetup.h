/******************************************************************************
 * Name         : pixeventpbesetup.h
 *
 * Copyright    : 2009 by Imagination Technologies Limited.
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
 * $Log: pixeventpbesetup.h $
 *****************************************************************************/
#ifndef _PIXEVENTPBESETUP_H_
#define _PIXEVENTPBESETUP_H_

#include "img_3dtypes.h"

/**
 * These are parameters specific to the surface being set up
 * and hence can be typically set up at surface creation
 * time.
 */
typedef struct _PBE_SURF_PARAMS_
{
	PVRSRV_PIXEL_FORMAT eFormat;
	IMG_DEV_VIRTADDR sAddress;
	IMG_MEMLAYOUT eMemLayout;

	IMG_UINT32 ui32LineStrideInPixels;

	IMG_SCALING eScaling;

	IMG_BOOL bEnableDithering;

	IMG_BOOL bZOnlyRender;

#if defined(SGX_FEATURE_PBE_GAMMACORRECTION)
	IMG_BOOL bEnableGamma;
#endif

} PBE_SURF_PARAMS, *PPBE_SURF_PARAMS;

/**
 * These parameters are generally render-specific and
 * need to be set up at the time WritePBEEmitState
 * is called
 */
typedef struct _PBE_RENDER_PARAMS_
{
	PVRSRV_ROTATION eRotation;

	/*
	 * Clipping params are in terms of
	 * pixels and are inclusive
	 */
	IMG_UINT32 ui32MinXClip;
	IMG_UINT32 ui32MaxXClip;

	IMG_UINT32 ui32MinYClip;
	IMG_UINT32 ui32MaxYClip;

	/*
	 * Whether to choose SrcSelect or DstOffset
	 * will be determined based on the pixel format
	 */
	union
	{
		IMG_UINT32 ui32SrcSelect;
		IMG_UINT32 ui32DstOffset;
	} uSel;

} PBE_RENDER_PARAMS, *PPBE_RENDER_PARAMS;

IMG_VOID WritePBEEmitState(PBE_SURF_PARAMS *psSurfParams,
						   PBE_RENDER_PARAMS *psRenderParams,
						   IMG_PUINT32 pui32Base);

#endif /* defined(PIXEVENTPBESETUP) */
