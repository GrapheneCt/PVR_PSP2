/*!****************************************************************************
@File           pvr2dblt.c

@Title          PVR2D library blit command processing

@Author         Imagination Technologies

@Date           14/12/2006

@Copyright      Copyright 2006-2009 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or otherwise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform       Generic

@Description    PVR2D library blit command code

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: pvr2dblt.c $
******************************************************************************/

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "img_defs.h"
#include "services.h"
#include "pvr2d.h"
#include "pvr2dint.h"
#include "sgxapi.h"

// Debug with syncronous blts
// #define SYNC_DEBUG

#define PVR2D_MAX(A, B) ((A) > (B) ? (A) : (B))
#define PVR2D_MIN(A, B) ((A) < (B) ? (A) : (B))

#define PVR2D_TIMEOUT_MS			500		/* 0.5 second timeout */

#ifdef DEBUG
// Force validation on for debug builds
#ifndef PVR2D_VALIDATE_INPUT_PARAMS
#define PVR2D_VALIDATE_INPUT_PARAMS
#endif
#endif

// List of blt destination formats
static const PVR2D_ULONG bValidOutputFormat[64] =
{
	/* 2D Core formats */

	0,		/* PVR2D_1BPP */
	1,		/* PVR2D_RGB565 */
	1,		/* PVR2D_ARGB4444 */
#ifdef SGX_FEATURE_PTLA
	1,		/* PVR2D_RGB888 */
#else
	0,		/* PVR2D_RGB888 */
#endif
	1,		/* PVR2D_ARGB8888 */
	1,		/* PVR2D_ARGB1555 */
	0,		/* PVR2D_ALPHA8 */
	0,		/* PVR2D_ALPHA4 */
	0,		/* PVR2D_PAL2 */
	0,		/* PVR2D_PAL4 */
	0,		/* PVR2D_PAL8 */

	0,		// 0x0B
	0,		// 0x0C
	0,		// 0x0D
	0,		// 0x0E
	0,		// 0x0F

	/* PTLA output formats */

	1,			/* PVR2D_U8 */
	1,			/* PVR2D_U88 */
	1,			/* PVR2D_S8 */
	1,			/* PVR2D_YUV422_YUYV */
	1,			/* PVR2D_YUV422_UYVY */
	1,			/* PVR2D_YUV422_YVYU */
	1,			/* PVR2D_YUV422_VYUY */
	1,			/* PVR2D_YUV420_2PLANE */
	1,			/* PVR2D_YUV420_3PLANE */
	1,			/* PVR2D_2101010ARGB */
	1,			/* PVR2D_888RSGSBS */
	1,			/* PVR2D_16BPP_RAW */
	1,			/* PVR2D_32BPP_RAW */
	1,			/* PVR2D_64BPP_RAW */
	1,			/* PVR2D_128BPP_RAW */

	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0
};

static const PVR2D_ULONG uiColorKeyMask[64] =
{
	0,					/* PVR2D_1BPP */
	CKEY_MASK_565,		/* PVR2D_RGB565 */
	CKEY_MASK_4444,		/* PVR2D_ARGB4444 */
	0x00FFFFFFUL,		/* PVR2D_RGB888 */
	CKEY_MASK_8888,		/* PVR2D_ARGB8888 */
	CKEY_MASK_1555,		/* PVR2D_ARGB1555 */
	0,					/* PVR2D_ALPHA8 */
	0,					/* PVR2D_ALPHA4 */
	0,					/* PVR2D_PAL2 */
	0,					/* PVR2D_PAL4 */
	0,					/* PVR2D_PAL8 */

	0,		// 0x0B
	0,		// 0x0C
	0,		// 0x0D
	0,		// 0x0E
	0,		// 0x0F

	0,					/* PVR2D_U8 */
	0,					/* PVR2D_U88 */
	0,					/* PVR2D_S8 */
	0xFFFFFFFFUL,		/* PVR2D_YUV422_YUYV */
	0xFFFFFFFFUL,		/* PVR2D_YUV422_UYVY */
	0xFFFFFFFFUL,		/* PVR2D_YUV422_YVYU */
	0xFFFFFFFFUL,		/* PVR2D_YUV422_VYUY */
	0,					/* PVR2D_YUV420_2PLANE */
	0,					/* PVR2D_YUV420_3PLANE */
	0,					/* PVR2D_2101010ARGB */
	0x00FFFFFFUL,		/* PVR2D_888RSGSBS */
	0,					/* PVR2D_16BPP_RAW */
	0,					/* PVR2D_32BPP_RAW */
	0,					/* PVR2D_64BPP_RAW */
	0,					/* PVR2D_128BPP_RAW */
	
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0
};

#if defined(PVR2D_ALT_2DHW) || defined(PDUMP)
// Palette size required by source format
static const IMG_UINT32 ui32PaletteSize[64] =
{
	2,		/* PVR2D_1BPP */
	0,		/* PVR2D_RGB565 */
	0,		/* PVR2D_ARGB4444 */
	0,		/* PVR2D_RGB888 */
	0,		/* PVR2D_ARGB8888 */
	0,		/* PVR2D_ARGB1555 */
	0,		/* PVR2D_ALPHA8 */
	0,		/* PVR2D_ALPHA4 */
	4,		/* PVR2D_PAL2 */
	16,		/* PVR2D_PAL4 */
	256,	/* PVR2D_PAL8 */

	0,		// 0x0B
	0,		// 0x0C
	0,		// 0x0D
	0,		// 0x0E
	0,		// 0x0F

	0,		/* PVR2D_U8 */
	0,		/* PVR2D_U88 */
	0,		/* PVR2D_S8 */
	0,		/* PVR2D_YUV422_YUYV */
	0,		/* PVR2D_YUV422_UYVY */
	0,		/* PVR2D_YUV422_YVYU */
	0,		/* PVR2D_YUV422_VYUY */
	0,		/* PVR2D_YUV420_2PLANE */
	0,		/* PVR2D_YUV420_3PLANE */
	0,		/* PVR2D_2101010ARGB */
	0,		/* PVR2D_888RSGSBS */
	0,		/* PVR2D_16BPP_RAW */
	0,		/* PVR2D_32BPP_RAW */
	0,		/* PVR2D_64BPP_RAW */
	0,		/* PVR2D_128BPP_RAW */
	
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,
	0,0,0

};
#endif


#if defined(PVR2D_ALT_2DHW) || defined(PVR2D_VALIDATE_INPUT_PARAMS)

// Primary blt attribute flags
#define BLT_FLAG_DEST				0x01UL
#define BLT_FLAG_SOURCE				0x02UL
#define BLT_FLAG_PATTERN			0x04UL
#define BLT_FLAG_MASK				0x08UL
#define BLT_FLAG_COLOURFILL			0x10UL
#define BLT_FLAG_BLACKORWHITE		0x20UL
#define BLT_FLAG_COMMONBLT			0x40UL
#define BLT_FLAG_CUSTOMROP			0x80UL


// Rop code input surfaces
#define DPS		(BLT_FLAG_DEST|BLT_FLAG_PATTERN|BLT_FLAG_SOURCE)
#define PS		(BLT_FLAG_PATTERN|BLT_FLAG_SOURCE)
#define DS		(BLT_FLAG_DEST|BLT_FLAG_SOURCE)
#define DSX		(BLT_FLAG_DEST|BLT_FLAG_SOURCE|BLT_FLAG_CUSTOMROP)
#define DP		(BLT_FLAG_DEST|BLT_FLAG_PATTERN)
#define DPX		(BLT_FLAG_DEST|BLT_FLAG_PATTERN|BLT_FLAG_CUSTOMROP)
#define D		(BLT_FLAG_DEST)
#define DX		(BLT_FLAG_DEST|BLT_FLAG_CUSTOMROP)
#define S		(BLT_FLAG_SOURCE)
#define SX		(BLT_FLAG_SOURCE|BLT_FLAG_CUSTOMROP)
#define SC		(BLT_FLAG_SOURCE|BLT_FLAG_COMMONBLT)
#define P		(BLT_FLAG_PATTERN)
#define PX		(BLT_FLAG_PATTERN|BLT_FLAG_CUSTOMROP)
#define PC		(BLT_FLAG_PATTERN|BLT_FLAG_COMMONBLT)
#define BW		(BLT_FLAG_BLACKORWHITE|BLT_FLAG_COLOURFILL|BLT_FLAG_COMMONBLT)


/** Defines a mapping with different rops (Raster Operations) and the surfaces
 *  that the rop requires to read from and the type of ROP */
static const IMG_BYTE ui8RopSurfs[256] =
{
/*   0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F */
	BW, DPS,DPS,PS, DPS,DPX,DPS,DPS,DPS,DPS,DPX,DPS,PS, DPS,DPS,PX,   /* 0 */
	DPS,DSX,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,  /* 1 */
	DPS,DPS,DSX,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,  /* 2 */
	PS, DPS,DPS,SX, DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,PS, DPS,DPS,PS,   /* 3 */
	DPS,DPS,DPS,DPS,DSX,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,  /* 4 */
	DPX,DPS,DPS,DPS,DPS,DX, DPS,DPS,DPS,DPS,DPX,DPS,DPS,DPS,DPS,DPX,  /* 5 */
	DPS,DPS,DPS,DPS,DPS,DPS,DSX,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,  /* 6 */
	DPS,DPS,DPS,DPS,DPS,DPS,DPS,DSX,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,  /* 7 */
	DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DSX,DPS,DPS,DPS,DPS,DPS,DPS,DPS,  /* 8 */
	DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DSX,DPS,DPS,DPS,DPS,DPS,DPS,  /* 9 */
	DPX,DPS,DPS,DPS,DPS,DPX,DPS,DPS,DPS,DPS,DX, DPS,DPS,DPS,DPS,DPX,  /* A */
	DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DSX,DPS,DPS,DPS,DPS,  /* B */
	PS, DPS,DPS,PS, DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,SC, DPS,DPS,PS,   /* C */
	DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DSX,DPS,DPS,  /* D */
	DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DPS,DSX,DPS,  /* E */
	PC, DPS,DPS,PS, DPS,DPX,DPS,DPS,DPS,DPS,DPX,DPS,PS, DPS,DPS,BW    /* F */
};

#endif//#if defined(PVR2D_ALT_2DHW) || defined(PVR2D_VALIDATE_INPUT_PARAMS)


#if defined(PVR2D_VALIDATE_INPUT_PARAMS)

// Deal with out of bounds and badly behaved rectangles
static PVR2DERROR ValidateParams (	PVR2DBLTINFO *pBltInfo,
									PVR2D_ULONG  ulNumClipRects,
									PVR2DRECT    *pClipRect );
#endif//#if defined(PVR2D_VALIDATE_INPUT_PARAMS)

#if defined(PVR2D_ALT_2DHW)


static const IMG_UINT32 ui32PaletteSizeInBits[64] =
{
	1,		/* PVR2D_1BPP */
	0,		/* PVR2D_RGB565 */
	0,		/* PVR2D_ARGB4444 */
	0,		/* PVR2D_RGB888 */
	0,		/* PVR2D_ARGB8888 */
	0,		/* PVR2D_ARGB1555 */
	0,		/* PVR2D_ALPHA8 */
	0,		/* PVR2D_ALPHA4 */
	2,		/* PVR2D_PAL2 */
	4,		/* PVR2D_PAL4 */
	8,		/* PVR2D_PAL8 */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0
};


static PVR2DERROR AltPresentBlt(const PVR2DCONTEXTHANDLE hContext,
								const PPVR2DMEMINFO pSrc,
								const PVR2DRECT* pClipRect);

static PVR2DERROR AltBltClipped(PVR2DCONTEXTHANDLE hContext,
								PVR2DBLTINFO *pBltInfo,
								PVR2D_ULONG ulNumClipRects,
								PVR2DRECT *pClipRect);
#endif /* PVR2D_ALT_2DHW */

#if defined(PVR2D_ALT_2DHW) || defined(PVR2D_VALIDATE_INPUT_PARAMS)

// Detect the rop4 input surfaces
static IMG_UINT32 GetSurfs (const IMG_UINT32 ui32Rop4, const IMG_BOOL bValidPattern, const IMG_BOOL bValidMask)
{
	IMG_UINT32 iu32SurfsPresent = (IMG_UINT32)ui8RopSurfs[(IMG_BYTE)ui32Rop4]; // rop3 surfaces

	if (bValidMask)
	{
		// rop4 surfaces
		iu32SurfsPresent |= BLT_FLAG_MASK | (IMG_UINT32)ui8RopSurfs[(IMG_BYTE)(ui32Rop4 >> 8)];
		iu32SurfsPresent &= ~(IMG_UINT32)BLT_FLAG_COMMONBLT; // Not trivual
	}

	// Solid fill or pattern fill ?
	if (((iu32SurfsPresent & BLT_FLAG_PATTERN) != 0) && (bValidPattern == IMG_FALSE))
	{
		iu32SurfsPresent &= ~BLT_FLAG_PATTERN; // Take out pattern
		iu32SurfsPresent |= BLT_FLAG_COLOURFILL; // Put in the fill colour
	}

	return iu32SurfsPresent;
}

#endif /* PVR2D_ALT_2DHW */

IMG_INTERNAL PVRSRV_PIXEL_FORMAT GetPvrSrvPixelFormat(const PVR2DFORMAT Format)
{
	IMG_UINT32 ui32Format = (IMG_UINT32)Format;

	if (Format & PVR2D_FORMAT_PVRSRV)
	{
		// PVRSRV format, no conversion required
		/* PRQA S 1482 1 */ /* enum cast is correct */
		return (PVRSRV_PIXEL_FORMAT)(ui32Format & PVR2D_FORMAT_MASK);
	}

	ui32Format &= PVR2D_FORMAT_MASK;

	switch (ui32Format)
	{
		case PVR2D_ARGB8888:		return PVRSRV_PIXEL_FORMAT_ARGB8888;
		case PVR2D_RGB565:			return PVRSRV_PIXEL_FORMAT_RGB565;
		case PVR2D_ARGB4444:		return PVRSRV_PIXEL_FORMAT_ARGB4444;
		case PVR2D_ARGB1555:		return PVRSRV_PIXEL_FORMAT_ARGB1555;
		case PVR2D_ALPHA8:			return PVRSRV_PIXEL_FORMAT_A8;
		case PVR2D_RGB888:			return PVRSRV_PIXEL_FORMAT_RGB888;
		case PVR2D_1BPP:			return PVRSRV_PIXEL_FORMAT_PAL1;
		case PVR2D_PAL2:			return PVRSRV_PIXEL_FORMAT_PAL2;
		case PVR2D_PAL4:			return PVRSRV_PIXEL_FORMAT_PAL4;
		case PVR2D_PAL8:			return PVRSRV_PIXEL_FORMAT_PAL8;
		case PVR2D_U8:				return PVRSRV_PIXEL_FORMAT_MONO8;
		case PVR2D_U88:				return PVRSRV_PIXEL_FORMAT_G8R8_UINT;
		case PVR2D_S8:				return PVRSRV_PIXEL_FORMAT_R8_SINT;
		case PVR2D_YUV422_YUYV:		return PVRSRV_PIXEL_FORMAT_YUYV;
		case PVR2D_YUV422_UYVY:		return PVRSRV_PIXEL_FORMAT_UYVY;
		case PVR2D_YUV422_YVYU:		return PVRSRV_PIXEL_FORMAT_YVYU;
		case PVR2D_YUV422_VYUY:		return PVRSRV_PIXEL_FORMAT_VYUY;
		case PVR2D_YUV420_2PLANE:	return PVRSRV_PIXEL_FORMAT_NV12;
		case PVR2D_YUV420_3PLANE:	return PVRSRV_PIXEL_FORMAT_YV12;
		case PVR2D_2101010ARGB:		return PVRSRV_PIXEL_FORMAT_A2RGB10;
		case PVR2D_888RSGSBS:		return PVRSRV_PIXEL_FORMAT_B8G8R8_SINT;
		case PVR2D_16BPP_RAW:		return PVRSRV_PIXEL_FORMAT_R16;
		case PVR2D_32BPP_RAW:		return PVRSRV_PIXEL_FORMAT_X8R8G8B8;
		case PVR2D_64BPP_RAW:		return PVRSRV_PIXEL_FORMAT_A16B16G16R16;
		case PVR2D_128BPP_RAW:		return PVRSRV_PIXEL_FORMAT_A32B32G32R32;
		default:					return PVRSRV_PIXEL_FORMAT_UNKNOWN;
	}
}

#ifdef PDUMP // Capture PVR2DBlt surfaces
static void PdumpInputSurfaces(PVR2DCONTEXT *psContext, IMG_BOOL bSrcIsPresent, PVR2DBLTINFO *pBltInfo);
static void PdumpOutputSurface(PVR2DCONTEXT *psContext, PVR2DBLTINFO *pBltInfo);
static void PdumpPresentBltOutputSurface(PVR2DCONTEXT *psContext, PVR2DMEMINFO	*pDstMemInfo, PVR2DRECT *prcDstRect);
//
// CAPTURE_PRESENTBLT_INPUT should not normally be defined.
// The assumption can be made that the PresentBlt source has originated from a 3D render and therefore does not need recapturing.
// On a no-hardware build the LDB would overwrite the render output and result in corrupt pixels.
// CAPTURE_PRESENTBLT_INPUT is only needed for testing PresentBlt in isolation where the source originates from host writes.
//
#if defined(CAPTURE_PRESENTBLT_INPUT)
static void PdumpPresentBltInputSurface(PVR2DCONTEXT *psContext, PVR2DMEMINFO	*pSrcMemInfo, PVR2DRECT *prcSrcRect);
#endif//#if defined(CAPTURE_PRESENTBLT_INPUT)
#endif//#ifdef PDUMP

#ifdef PVR2D_ALT_2DHW
static const IMG_UINT32 aPVR2DBitsPP[64] =
{
	1, /*PVR2D_1BPP*/
	16, /*PVR2D_RGB565*/
	16, /*PVR2D_ARGB4444*/
	24, /*PVR2D_RGB888*/
	32, /*PVR2D_ARGB8888*/
	16, /*PVR2D_ARGB1555*/
	8, /*PVR2D_ALPHA8*/
	4, /*PVR2D_ALPHA4*/
	2, /*PVR2D_PAL2*/
	4, /*PVR2D_PAL4*/
	8, /*PVR2D_PAL8*/
	0,0,0,0,0, /* reserved */
	8, /* PVR2D_U8 */
	16, /* PVR2D_U88 */
	8, /* PVR2D_S8 */
	16, /* PVR2D_YUV422_YUYV */
	16, /* PVR2D_YUV422_UYVY */
	16, /* PVR2D_YUV422_YVYU */
	16, /* PVR2D_YUV422_VYUY */
	12, /* PVR2D_YUV420_2PLANE */
	12, /* PVR2D_YUV420_3PLANE */
	32, /* PVR2D_2101010ARGB */
	24, /* PVR2D_888RSGSBS */
	16, /* PVR2D_16BPP_RAW */
	32, /* PVR2D_32BPP_RAW */
	64, /* PVR2D_64BPP_RAW */
	128, /* PVR2D_128BPP_RAW */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0
};
#endif

#if defined (PDUMP) && defined (PVR2D_ALT_2DHW)
static const PDUMP_PIXEL_FORMAT aPdumpFormat[64] =
{
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,	/*PVR2D_1BPP*/
	PVRSRV_PDUMP_PIXEL_FORMAT_RGB565,		/*PVR2D_RGB565*/
	PVRSRV_PDUMP_PIXEL_FORMAT_ARGB4444,		/*PVR2D_ARGB4444*/
	PVRSRV_PDUMP_PIXEL_FORMAT_RGB888,		/*PVR2D_RGB888*/
	PVRSRV_PDUMP_PIXEL_FORMAT_ARGB8888,		/*PVR2D_ARGB8888*/
	PVRSRV_PDUMP_PIXEL_FORMAT_ARGB1555,		/*PVR2D_ARGB1555*/
	PVRSRV_PDUMP_PIXEL_FORMAT_A8,			/*PVR2D_ALPHA8*/
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,	/*PVR2D_ALPHA4*/
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,	/*PVR2D_PAL2*/
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,	/*PVR2D_PAL4*/
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,	/*PVR2D_PAL8*/
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_RGB8,			/* PVR2D_U8 */
	PVRSRV_PDUMP_PIXEL_FORMAT_RGB565,		/* PVR2D_U88 */
	PVRSRV_PDUMP_PIXEL_FORMAT_RGB8,			/* PVR2D_S8 */
	PVRSRV_PDUMP_PIXEL_FORMAT_Y0UY1V_8888,	/* PVR2D_YUV422_YUYV */
	PVRSRV_PDUMP_PIXEL_FORMAT_UY0VY1_8888,	/* PVR2D_YUV422_UYVY */
	PVRSRV_PDUMP_PIXEL_FORMAT_Y0VY1U_8888,	/* PVR2D_YUV422_YVYU */
	PVRSRV_PDUMP_PIXEL_FORMAT_VY0UY1_8888,	/* PVR2D_YUV422_VYUY */
	PVRSRV_PDUMP_PIXEL_FORMAT_420PL12YUV8,	/* PVR2D_YUV420_2PLANE */
	PVRSRV_PDUMP_PIXEL_FORMAT_YUV_PL8,		/* PVR2D_YUV420_3PLANE */
	PVRSRV_PDUMP_PIXEL_FORMAT_ARGB8888,		/* PVR2D_2101010ARGB */
	PVRSRV_PDUMP_PIXEL_FORMAT_RGB888,		/* PVR2D_888RSGSBS */
	PVRSRV_PDUMP_PIXEL_FORMAT_RGB565,		/* PVR2D_16BPP_RAW */
	PVRSRV_PDUMP_PIXEL_FORMAT_ARGB8888,		/* PVR2D_32BPP_RAW */
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,	/* PVR2D_64BPP_RAW */
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,	/* PVR2D_128BPP_RAW */
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED,
	PVRSRV_PDUMP_PIXEL_FORMAT_UNSUPPORTED
};
#endif//#ifdef PDUMP

/* PRQA L:END_TBL_DECLARE */

#if defined(SGX_FEATURE_2D_HARDWARE)

/* Maximum size of data for 2d blit command (in 32 bit words) */
#define PVR2D_MAX_BLIT_CMD_SIZE		64

/* state information */
typedef struct
{
	IMG_BOOL bSrcIsPresent;
	IMG_BOOL bDstIsPresent;
	/* Copy order is right to left */
	IMG_BOOL bRTL;
	/* Copy order is bottom to top */
	IMG_BOOL bBTT;
	IMG_UINT32 ui32BltRectControl;

	/* Array into which we build the command */
	IMG_UINT32 aui32BltCmd[PVR2D_MAX_BLIT_CMD_SIZE];
	IMG_UINT32* pui32BltCmd; /* Current position in array */
} PVR2D_BLTSTATE;

#define	BLT_STARTX(state, left, width) \
	((state)->bRTL ? (left) + (width) - 1: (left))
#define	BLT_STARTY(state, top, height) \
	((state)->bBTT ? (top) + (height) -1: (top))

/*
	Lookup to convert PVR2D format to EURASIA2D format
	NB One table as Source and Pattern formats have the same bit patterns
	Destination is a subset of these
*/
static const IMG_UINT32 aConvertPVR2DToSGX2D[64] =
{
 /*PVR2D_1BPP*/		EURASIA2D_SRC_1_PAL,	/* = EURASIA2D_PAT_1_PAL,  *NO DEST* */
 /*PVR2D_RGB565*/	EURASIA2D_SRC_565RGB,	/* = EURASIA2D_PAT_565RGB = EURASIA2D_DST_565RGB */
 /*PVR2D_ARGB4444*/	EURASIA2D_SRC_4444ARGB,	/* = EURASIA2D_PAT_4444ARGB = EURASIA2D_DST_4444ARGB */
 /*PVR2D_RGB888*/	EURASIA2D_SRC_0888ARGB,	/* = EURASIA2D_PAT_0888ARGB = EURASIA2D_DST_0888ARGB */
 /*PVR2D_ARGB8888*/	EURASIA2D_SRC_8888ARGB,	/* = EURASIA2D_PAT_8888ARGB = EURASIA2D_DST_8888ARGB */
 /*PVR2D_ARGB1555*/	EURASIA2D_SRC_1555ARGB,	/* = EURASIA2D_PAT_1555ARGB = EURASIA2D_DST_1555ARGB*/
 /*PVR2D_ALPHA8*/	EURASIA2D_SRC_8_ALPHA,	/* = EURASIA2D_PAT_8_ALPHA, *NO DEST* */
 /*PVR2D_ALPHA4*/	EURASIA2D_SRC_4_ALPHA,	/* = EURASIA2D_PAT_4_ALPHA, *NO DEST* */
 /*PVR2D_PAL2*/		EURASIA2D_SRC_2_PAL,	/* = EURASIA2D_PAT_2_PAL, *NO DEST* */
 /*PVR2D_PAL4*/		EURASIA2D_SRC_4_PAL,	/* = EURASIA2D_PAT_4_PAL, *NO DEST* */
 /*PVR2D_PAL8*/		EURASIA2D_SRC_8_PAL,	/* = EURASIA2D_PAT_8_PAL, *NO DEST* */
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0
};

static const IMG_UINT32 aRequiresPalette[64] =
{
	1,	/*PVR2D_1BPP*/
	0,	/*PVR2D_RGB565*/
	0,	/*PVR2D_ARGB4444*/
	0,	/*PVR2D_RGB888*/
	0,	/*PVR2D_ARGB8888*/
	0,	/*PVR2D_ARGB1555*/
	0,	/*PVR2D_ALPHA8*/
	0,	/*PVR2D_ALPHA4*/
	1,	/*PVR2D_PAL2*/
	1,	/*PVR2D_PAL4*/
	1,	/*PVR2D_PAL8*/
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0
};

static const IMG_UINT32 aConvertPVR2DAlphaOp[] =
{
	EURASIA2D_SRCALPHA_OP_ZERO,	// PVR2D_BLEND_OP_ZERO = 0,
	EURASIA2D_SRCALPHA_OP_ONE,	// PVR2D_BLEND_OP_ONE = 1,
	EURASIA2D_SRCALPHA_OP_SRC,	// PVR2D_BLEND_OP_SRC = 2,
	EURASIA2D_SRCALPHA_OP_DST,	// PVR2D_BLEND_OP_DST = 3,
	EURASIA2D_SRCALPHA_OP_GBL,	// PVR2D_BLEND_OP_GLOBAL = 4,
	EURASIA2D_SRCALPHA_OP_SG,	// PVR2D_BLEND_OP_SRC_PLUS_GLOBAL = 5,
	EURASIA2D_SRCALPHA_OP_DG	// PVR2D_BLEND_OP_DST_PLUS_GLOBAL = 6
};

#endif /* SGX_FEATURE_2D_HARDWARE */
//#endif /* PVR2D_ALT_2DHW */


#ifdef PDUMP
// Simplified itoa for +ve integers base 10
static PVR2D_VOID i2a(PVR2D_INT n, PVR2D_CHAR* s)
{
	PVR2D_CHAR c;
	PVR2D_INT i=0;

	do
	{
		s[i++] = (PVR2D_CHAR)((n%10) + '0');
		n/=10;
	} while(n);

	s[i--] = (PVR2D_CHAR)'\0';

	// un-reverse the order
	/* PRQA S 3356,3359,3201 6 */ /* i can be greater than n */
	while(n<i)
	{
		c = s[n];
		s[n++] = s[i];
		s[i--] = c;
	}
}
#endif//#ifdef PDUMP


#if defined(SGX_FEATURE_2D_HARDWARE) && !defined(PVR2D_ALT_2DHW)

static PVR2DERROR PVR2DBltVia2DCore(PVR2DCONTEXTHANDLE hContext,
						   PVR2DBLTINFO *pBltInfo,
						   PVR2D_ULONG ulNumClipRects,
						   PVR2DRECT *pClipRect);



/*!****************************************************************************
 @Function	_2DClipControl

 @Input		pClipRect : clipping region

 @Input		pBltInfo : PVR2D blit info structure

 @Input		psBltState : Blt state information structure

 @Return	void : none

 @Description : Submit clipping region
******************************************************************************/
static void _2DClipControl(const PVR2DRECT		*pClipRect,
						   const PVR2DBLTINFO	*pBltInfo,
						   PVR2D_BLTSTATE		*psBltState)
{
	PVR2DRECT	sRelRect;

#ifdef DEBUG /* Warning fix for C4127 */
	IMG_UINT32 ui32ClipCountMax = EURASIA2D_CLIPCOUNT_MAX;

	/* This function assumes there is at most one clipping region  */
	PVR_ASSERT(ui32ClipCountMax == 1);
#endif

	PVR2D_DPF((PVR_DBG_VERBOSE, "2DClipControl"));


	/*
	 * The coordinates of the clipping rectangle are * relative to the
	 * destination rectangle.
	 */
	sRelRect.left =
		(pClipRect->left > pBltInfo->DstX) ?
			pClipRect->left - pBltInfo->DstX :
			0;
	sRelRect.top =
		(pClipRect->top > pBltInfo->DstY) ?
			pClipRect->top - pBltInfo->DstY :
			0;
	sRelRect.right =
		(pClipRect->right < (pBltInfo->DstX + pBltInfo->DSizeX)) ?
			pClipRect->right - pBltInfo->DstX :
			pBltInfo->DSizeX;
	sRelRect.bottom =
		(pClipRect->bottom < (pBltInfo->DstY + pBltInfo->DSizeY)) ?
			pClipRect->bottom - pBltInfo->DstY :
			pBltInfo->DSizeY;

	psBltState->ui32BltRectControl |= EURASIA2D_CLIP_ENABLE;

	*psBltState->pui32BltCmd++ = EURASIA2D_CLIP_BH
			| (((IMG_UINT32)sRelRect.right << EURASIA2D_CLIP_XMAX_SHIFT) & EURASIA2D_CLIP_XMAX_MASK)
			| (((IMG_UINT32)sRelRect.left << EURASIA2D_CLIP_XMIN_SHIFT) & EURASIA2D_CLIP_XMIN_MASK);

	*psBltState->pui32BltCmd++ =
		(((IMG_UINT32)sRelRect.bottom << EURASIA2D_CLIP_YMAX_SHIFT) & EURASIA2D_CLIP_YMAX_MASK)
		| (((IMG_UINT32)sRelRect.top << EURASIA2D_CLIP_YMIN_SHIFT) & EURASIA2D_CLIP_YMIN_MASK);
}

/*!****************************************************************************
 @Function	_PVR2DCKControl

 @Input		pBltInfo : PVR2D blit info structure

 @Input		psBltState : Blt state information structure

 @Return	void : none

 @Description : Colour Key control code
******************************************************************************/
static void _PVR2DCKControl(const PVR2DBLTINFO	*pBltInfo,
							PVR2D_BLTSTATE		*psBltState)
{
	PVR2DFORMAT Format;

	if ((psBltState->bSrcIsPresent) && ((pBltInfo->BlitFlags & PVR2D_BLIT_COLKEY_DEST) == 0))
	{
		// Key colour is on source surface
		PVR2D_DPF((PVR_DBG_VERBOSE, "PVR2DCKControl: Source %08lX", pBltInfo->ColourKey));
		psBltState->ui32BltRectControl |= EURASIA2D_SRCCK_REJECT;  // Don't copy if source colour = ColourKey
		*psBltState->pui32BltCmd++ = EURASIA2D_CTRL_BH | EURASIA2D_SRCCK_CTRL;
		Format = pBltInfo->SrcFormat;
	}
	else
	{
		// Key colour is on destination surface
		PVR2D_DPF((PVR_DBG_VERBOSE, "PVR2DCKControl: Destination %08lX", pBltInfo->ColourKey));
		psBltState->ui32BltRectControl |= EURASIA2D_DSTCK_PASS; // Copy if dest colour = ColourKey
		*psBltState->pui32BltCmd++ = EURASIA2D_CTRL_BH | EURASIA2D_DSTCK_CTRL;
		Format = pBltInfo->DstFormat;
	}

	*psBltState->pui32BltCmd++ = pBltInfo->ColourKey;
	*psBltState->pui32BltCmd++ = uiColorKeyMask[Format & PVR2D_FORMAT_MASK]; /* CK MASK value */
}


/*!****************************************************************************
 @Function	_PVR2DAlphaBlendControl

 @Input		pBltInfo : PVR2D blit info structure

 @Input		psBltState : Blt state information structure

 @Return	error : PVR2D error code

 @Description : Alpha blend control code
******************************************************************************/
static PVR2DERROR _PVR2DAlphaBlendControl(const PVR2DBLTINFO	*pBltInfo,
										  PVR2D_BLTSTATE		*psBltState,
										  PVR2DCONTEXT			*psContext)
{
	IMG_UINT32 uAlphaBlendControlWord1;
	IMG_UINT32 uAlphaBlendControlWord2;
	const PVR2D_ALPHABLT *pAlpha = pBltInfo->pAlpha;

	// ////////////////////////////////////////////////////////////////////////
	// per-pixel alpha-blending
	// ////////////////////////////////////////////////////////////////////////
	if (((pBltInfo->BlitFlags & PVR2D_BLIT_PERPIXEL_ALPHABLEND_ENABLE) != 0) &&
		((pBltInfo->BlitFlags & PVR2D_BLIT_GLOBAL_ALPHA_ENABLE) != 0))
	{
		// Premul Source and Global Alpha
		PVR2D_DPF((PVR_DBG_VERBOSE, "PVR2D Alpha Blend : Source and Global"));

		// Cdest = Aglob*Csrc + (1-(Aglob*Asrc))*Cdest
		uAlphaBlendControlWord1 = EURASIA2D_SRCALPHA_OP_GBL
							| EURASIA2D_DSTALPHA_OP_SRC
							| EURASIA2D_DSTALPHA_INVERT
							| EURASIA2D_PRE_MULTIPLICATION_ENABLE; // (Aglob*Asrc)

		uAlphaBlendControlWord1 |= (pBltInfo->GlobalAlphaValue << EURASIA2D_GBLALPHA_SHIFT)
										 & EURASIA2D_GBLALPHA_MASK;

		uAlphaBlendControlWord2 = EURASIA2D_SRCALPHA_OP_GBL |
							 EURASIA2D_DSTALPHA_OP_SRC |
							 EURASIA2D_DSTALPHA_INVERT;

		uAlphaBlendControlWord2 |= (pBltInfo->GlobalAlphaValue << EURASIA2D_GBLALPHA_SHIFT)
										 & EURASIA2D_GBLALPHA_MASK;
	}
	else if (pBltInfo->BlitFlags & PVR2D_BLIT_PERPIXEL_ALPHABLEND_ENABLE)
	{
		PVR2D_DPF((PVR_DBG_VERBOSE, "PVR2DAlphaBlendControl: Per Pixel. Fmt = 0x%lx", pBltInfo->SrcFormat));

		// Source alpha
		if (pBltInfo->AlphaBlendingFunc == PVR2D_ALPHA_OP_SRC_DSTINV)
		{
			// Non premul source alpha
			// Blend op for Alpha : DstA = SrcA*SrcA + DstA*(1-SrcA)
			uAlphaBlendControlWord1 =  EURASIA2D_SRCALPHA_OP_ONE |
								 EURASIA2D_DSTALPHA_OP_SRC |
								 EURASIA2D_DSTALPHA_INVERT;

			// Blend op for RGB : DstC = SrcC*SrcA + DstC*(1-SrcA)
			uAlphaBlendControlWord2 = EURASIA2D_SRCALPHA_OP_SRC |
								 EURASIA2D_DSTALPHA_OP_SRC |
								 EURASIA2D_DSTALPHA_INVERT;
		}
		else if (pBltInfo->AlphaBlendingFunc == PVR2D_ALPHA_OP_SRCP_DSTINV)
		{
			// Premultiplied source alpha
			// Blend op for Alpha : DstA = 1*SrcA + DstA*(1-SrcA)
			uAlphaBlendControlWord1 =  EURASIA2D_SRCALPHA_OP_ONE |
								 EURASIA2D_DSTALPHA_OP_SRC |
								 EURASIA2D_DSTALPHA_INVERT;

			// Blend op for RGB : DstC = 1*SrcCpremul + DstC*(1-SrcA)
			uAlphaBlendControlWord2 = EURASIA2D_SRCALPHA_OP_ONE |
								 EURASIA2D_DSTALPHA_OP_SRC |
								 EURASIA2D_DSTALPHA_INVERT;
		}
		else
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DAlphaBlendControl: Invalid alpha function 0x%x", pBltInfo->AlphaBlendingFunc));
			return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
		}
	}
	else if (pBltInfo->BlitFlags & PVR2D_BLIT_GLOBAL_ALPHA_ENABLE)
	{
		// /////////////////////////////////////////////////////////////////////////
		// global alpha blending
		// /////////////////////////////////////////////////////////////////////////

		PVR2D_DPF((PVR_DBG_VERBOSE, "PVR2DAlphaBlendControl: Global. Value = 0x%x", pBltInfo->GlobalAlphaValue));

		uAlphaBlendControlWord1 = EURASIA2D_SRCALPHA_OP_GBL |
							 EURASIA2D_DSTALPHA_OP_GBL |
							 EURASIA2D_DSTALPHA_INVERT;

		uAlphaBlendControlWord1 |= (pBltInfo->GlobalAlphaValue << EURASIA2D_GBLALPHA_SHIFT) & EURASIA2D_GBLALPHA_MASK;

		uAlphaBlendControlWord2 = EURASIA2D_SRCALPHA_OP_GBL |
							 EURASIA2D_DSTALPHA_OP_GBL |
							 EURASIA2D_DSTALPHA_INVERT;

		uAlphaBlendControlWord2 |= (pBltInfo->GlobalAlphaValue << EURASIA2D_GBLALPHA_SHIFT) & EURASIA2D_GBLALPHA_MASK;
	}
	/* PRQA S 3355 6 */ /* ignore misused of enums */
	else if (((pBltInfo->BlitFlags & PVR2D_BLIT_FULLY_SPECIFIED_ALPHA_ENABLE) != 0)
			 && (pAlpha != IMG_NULL)
			 && (pAlpha->eAlpha1 <= PVR2D_BLEND_OP_DST_PLUS_GLOBAL)
			 && (pAlpha->eAlpha2 <= PVR2D_BLEND_OP_DST_PLUS_GLOBAL)
			 && (pAlpha->eAlpha3 <= PVR2D_BLEND_OP_DST_PLUS_GLOBAL)
			 && (pAlpha->eAlpha4 <= PVR2D_BLEND_OP_DST_PLUS_GLOBAL))
	{
		// /////////////////////////////////////////////////////////////////////////
		// fully specified alpha blending
		// /////////////////////////////////////////////////////////////////////////

		// Word1 ALPHA_1 field : alpha src op
		uAlphaBlendControlWord1 = aConvertPVR2DAlphaOp[pAlpha->eAlpha1];

		if (pAlpha->bAlpha1Invert)
		{
			uAlphaBlendControlWord1 |= EURASIA2D_SRCALPHA_INVERT; // word 1 src invert
		}

		// Word1 ALPHA_3 field : alpha dst op
		uAlphaBlendControlWord1 |= (aConvertPVR2DAlphaOp[pAlpha->eAlpha3] << 4);

		if (pAlpha->bAlpha3Invert)
		{
			uAlphaBlendControlWord1 |= EURASIA2D_DSTALPHA_INVERT; // word 1 dst invert
		}

		// Word 2 ALPHA_2 field : rgb src op
		uAlphaBlendControlWord2 = aConvertPVR2DAlphaOp[pAlpha->eAlpha2];

		if (pAlpha->bAlpha2Invert)
		{
			uAlphaBlendControlWord2 |= EURASIA2D_SRCALPHA_INVERT; // word 2 src invert
		}

		// Word 2 ALPHA_4 field : rgb dst op
		uAlphaBlendControlWord2 |= (aConvertPVR2DAlphaOp[pAlpha->eAlpha4] << 4);

		if (pAlpha->bAlpha4Invert)
		{
			uAlphaBlendControlWord2 |= EURASIA2D_DSTALPHA_INVERT; // word 2 dst invert
		}

		// Word 1 premul blend modifier
		if (pAlpha->bPremulAlpha)
		{
			uAlphaBlendControlWord1 |= EURASIA2D_PRE_MULTIPLICATION_ENABLE; // premultiplication
		}

		// Word 1 zero alpha transparency stage
		if (pAlpha->bTransAlpha)
		{
			uAlphaBlendControlWord1 |= EURASIA2D_ZERO_SOURCE_ALPHA_ENABLE; // zero alpha transparent
		}

		// Global alpha
		uAlphaBlendControlWord1 |= ((IMG_UINT32)pAlpha->uGlobalA   << EURASIA2D_GBLALPHA_SHIFT); // Word 1 global for alpha
		uAlphaBlendControlWord2 |= ((IMG_UINT32)pAlpha->uGlobalRGB << EURASIA2D_GBLALPHA_SHIFT); // Word 2 global for rgb

		/*
			Once the 1555 lookup registers have been updated then all
			1555 surfaces from this context will be promoted to the 1555 lookup format.
		*/
		if (pAlpha->bUpdateAlphaLookup)
		{
#if defined(EUR_CR_2D_ALPHA_COMPONENT_ZERO_MASK) && !defined(PVR2D_ALT_2DHW)
			psContext->ui32AlphaRegValue = ALPHA_1555(pAlpha->uAlphaLookup0, pAlpha->uAlphaLookup1);
#else
			PVR2D_DPF((PVR_DBG_ERROR, "1555 Alpha Lookup is not available on this 2D Core revision"));
			return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
#endif
		}
	}
	else
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	// Enable alpha for this blt
	psBltState->ui32BltRectControl |= EURASIA2D_ALPHA_ENABLE;

	// Control stream data for alpha blend
	*psBltState->pui32BltCmd++ = EURASIA2D_CTRL_BH | EURASIA2D_ALPHA_CTRL;
	*psBltState->pui32BltCmd++ = uAlphaBlendControlWord1;
	*psBltState->pui32BltCmd++ = uAlphaBlendControlWord2;
	return PVR2D_OK;
}

/*!****************************************************************************
 @Function	_PVR2DMaskControl

 @Input		psContext : PVR2D context

 @Input		pBltInfo : PVR2D blit info structure

 @Input		psBltState : Blt state information structure

 @Return	void : none

 @Description : Source Mask control code
******************************************************************************/
static void  _PVR2DMaskControl(const PVR2DCONTEXT	*psContext,
							   const PVR2DBLTINFO	*pBltInfo,
							   PVR2D_BLTSTATE		*psBltState)
{
	IMG_UINT32 uPortData;

	PVR2D_DPF((PVR_DBG_VERBOSE, "PVR2DMaskControl: %ld x %ld", pBltInfo->MaskX, pBltInfo->MaskY));

	uPortData =  EURASIA2D_MASK_SURF_BH;
	uPortData |= ((IMG_UINT32)pBltInfo->MaskStride << EURASIA2D_MASK_STRIDE_SHIFT)
							& EURASIA2D_MASK_STRIDE_MASK;
	*psBltState->pui32BltCmd++ = uPortData;

	/* mask dev address */
	uPortData = ((pBltInfo->pMaskMemInfo->ui32DevAddr - psContext->s2DHeapBase.uiAddr) >> EURASIA2D_MASK_ADDR_ALIGNSHIFT
							<< EURASIA2D_MASK_ADDR_SHIFT) & EURASIA2D_MASK_ADDR_MASK;
	*psBltState->pui32BltCmd++ = uPortData;

	/* mask offset (no offset) */
	uPortData =  EURASIA2D_MASK_OFF_BH;
	uPortData |= ((IMG_UINT32)BLT_STARTX(psBltState, 0, pBltInfo->SizeX) << EURASIA2D_MASKOFF_XSTART_SHIFT) & EURASIA2D_MASKOFF_XSTART_MASK;
	uPortData |= ((IMG_UINT32)BLT_STARTY(psBltState, 0, pBltInfo->SizeY) << EURASIA2D_MASKOFF_YSTART_SHIFT) & EURASIA2D_MASKOFF_YSTART_MASK;
	*psBltState->pui32BltCmd++ = uPortData;
}


/*****************************************************************************
 @Function	_PVR2DPatternControl

 @Input		psContext : PVR2D context

 @Input		pBltInfo : PVR2D blit info structure

 @Input		psBltState : Blt state information structure

 @Return	error : PVR2D error code

 @Description : Pattern control code
******************************************************************************/
static PVR2DERROR _PVR2DPatternControl(	const PVR2DCONTEXT *psContext,
										const PVR2DBLTINFO *pBltInfo,
										PVR2D_BLTSTATE	*psBltState, IMG_UINT32 uFormat)
{
	IMG_UINT32 uPortData;
	IMG_INT32 i32XStart,i32YStart;
	IMG_INT32 i32ModCalc;

	PVR2D_DPF((PVR_DBG_VERBOSE, "PVR2DPatternControl: %ld x %ld, Fmt = 0x%lx", pBltInfo->SizeX, pBltInfo->SizeY, pBltInfo->SrcFormat));

	if (uFormat > PVR2D_PAL8 || uFormat == PVR2D_ALPHA8 || uFormat == PVR2D_ALPHA4)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPatternControl: illegal pattern format 0x%lx", pBltInfo->SrcFormat));
		return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
	}

	/* SGX 2D core internal pattern memory is 16x16 pixels */
	if (pBltInfo->SizeX > 16 || pBltInfo->SizeX < 1 || pBltInfo->SizeY > 16 || pBltInfo->SizeY < 1)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPatternControl: illegal pattern size"));
		return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
	}

	/* Check that the pattern offset is within the pattern boundry */
	if (pBltInfo->SrcX >= pBltInfo->SizeX || pBltInfo->SrcY >= pBltInfo->SizeY)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPatternControl: pattern offset is outside the pattern boundry"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	uPortData =  EURASIA2D_PAT_SURF_BH; /* pattern surface BH */
	uPortData |= aConvertPVR2DToSGX2D[uFormat];
	uPortData |= ((IMG_UINT32)pBltInfo->SrcStride << EURASIA2D_PAT_STRIDE_SHIFT) & EURASIA2D_PAT_STRIDE_MASK;
	*psBltState->pui32BltCmd++ = uPortData;

	/* pattern address. alloc'd base + byte offset */
	uPortData = ((pBltInfo->pSrcMemInfo->ui32DevAddr - psContext->s2DHeapBase.uiAddr + (IMG_UINT32)pBltInfo->SrcOffset)
						>> EURASIA2D_PAT_ADDR_ALIGNSHIFT << EURASIA2D_PAT_ADDR_SHIFT) & EURASIA2D_PAT_ADDR_MASK;

	*psBltState->pui32BltCmd++ = uPortData;

	/* Pattern control (offset, width & height) */
	uPortData =  EURASIA2D_PAT_BH;  /* pattern control BH */

	/* X and Y start (initial pixel offset) */
	if (pBltInfo->BlitFlags & (PVR2D_BLIT_ROT_90|PVR2D_BLIT_ROT_180|PVR2D_BLIT_ROT_270))
	{
		/* Rotated pattern blts require a modulus calc for the correct X and Y pattern offsets */
		if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_90)
		{
			/* 90 rotation */
			i32ModCalc = (pBltInfo->SrcY + pBltInfo->DSizeX) % pBltInfo->SizeY;
			i32XStart = (i32ModCalc) ? (pBltInfo->SizeY - i32ModCalc) : 0;
			i32XStart = (i32XStart==pBltInfo->SizeY) ? 0 : i32XStart;
			i32YStart = pBltInfo->SrcX;
		}
		else if(pBltInfo->BlitFlags & PVR2D_BLIT_ROT_180)
		{
			// 180 rotation
			i32ModCalc = (pBltInfo->SrcX + pBltInfo->DSizeX) % pBltInfo->SizeX;
			i32XStart = (i32ModCalc) ? (pBltInfo->SizeX - i32ModCalc) : 0;
			i32XStart = (i32XStart==pBltInfo->SizeX) ? 0 : i32XStart;
			i32ModCalc = (pBltInfo->SrcY + pBltInfo->DSizeY) % pBltInfo->SizeY;
			i32YStart = (i32ModCalc) ? (pBltInfo->SizeY - i32ModCalc) : 0;
			i32YStart = (i32YStart==pBltInfo->SizeY) ? 0 : i32YStart;
		}
		else
		{
			// 270 rotation
			i32ModCalc = (pBltInfo->SrcX + pBltInfo->DSizeY) % pBltInfo->SizeX;
			i32YStart = (i32ModCalc) ? (pBltInfo->SizeX - i32ModCalc) : 0;
			i32YStart = (i32YStart==pBltInfo->SizeX) ? 0 : i32YStart;
			i32XStart = pBltInfo->SrcY;
		}
	}
	else
	{
		/* No rotation */
		i32XStart = pBltInfo->SrcX;
		i32YStart = pBltInfo->SrcY;
	}

	/* Patten offset */
	uPortData |= ((IMG_UINT32)i32XStart << EURASIA2D_PAT_XSTART_SHIFT) & EURASIA2D_PAT_XSTART_MASK;
	uPortData |= ((IMG_UINT32)i32YStart << EURASIA2D_PAT_YSTART_SHIFT) & EURASIA2D_PAT_YSTART_MASK;

	/* Pattern size */
	uPortData |= ((IMG_UINT32)pBltInfo->SizeX << EURASIA2D_PAT_WIDTH_SHIFT) & EURASIA2D_PAT_WIDTH_MASK;
	uPortData |= ((IMG_UINT32)pBltInfo->SizeY << EURASIA2D_PAT_HEIGHT_SHIFT) & EURASIA2D_PAT_HEIGHT_MASK;

	*psBltState->pui32BltCmd++ = uPortData;

	/* Check for pattern palette */
	if (aRequiresPalette[uFormat])
	{
		if (!pBltInfo->pPalMemInfo)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPatternControl: No pattern palette supplied"));
			return PVR2DERROR_INVALID_PARAMETER;
		}

		uPortData = (pBltInfo->pPalMemInfo->ui32DevAddr - psContext->s2DHeapBase.uiAddr + (IMG_UINT32)pBltInfo->PalOffset);

		/* Check for palette alignment errors */
		if (uPortData & 0x0F)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPatternControl: Pattern palette alignment error"));
			return PVR2DERROR_INVALID_PARAMETER;
		}

		/* Send pattern palette header */
		*psBltState->pui32BltCmd++ = EURASIA2D_PAT_PAL_BH | uPortData;
	}

	return PVR2D_OK;
}


/*****************************************************************************
 @Function	PVR2DTraceBlt

 @Input		psBltState : Blt state information structure

 @Return	void : none

 @Description : trace the blit
******************************************************************************/
#if defined(DEBUG) && defined (PVR2D_TRACE_BLIT)
static void PVR2DTraceBlt(PVR2D_BLTSTATE *psBltState)
{
	IMG_UINT32 ui32Count;
	IMG_UINT32 i;

	/* Count in 4byte words - current pointer minus start of the array */
	ui32Count = ((IMG_UINT8*)psBltState->pui32BltCmd - (IMG_UINT8*)&psBltState->aui32BltCmd[0]) >> 2;

	PVR2D_DPF((PVR_DBG_ERROR, "----PVR2D BLIT----"));

	for (i = 0; i < ui32Count; i++)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "2D[%02d]: 0x%08x", i, psBltState->aui32BltCmd[i]));
	}
}
#else
#define PVR2DTraceBlt(x)
#endif

/*****************************************************************************
 @Function	_PVR2DSurfaceDetection

 @Input		pBltInfo : PVR2D blit info structure

 @Input		psBltState : Blt state information structure

 @Return	void : none

 @Description : detect presence of fill, surface and pattern
******************************************************************************/
static void _PVR2DSurfaceDetection(const PVR2DBLTINFO	*pBltInfo,
								   PVR2D_BLTSTATE		*psBltState)
{
	IMG_BYTE byRop3a;
	IMG_BYTE byRop3b;

	byRop3a = (IMG_BYTE)pBltInfo->CopyCode;
	byRop3b = (IMG_BYTE)(pBltInfo->CopyCode>>8);

	/*
		Legacy support : hard code rop3a to be destination (0xAA) and use the CopyCode as rop3b.
		But this is the reverse of what is normally done.
		WinCE needs rop3b to be the destination so from now on this must be user defined.
	*/
	if (pBltInfo->pMaskMemInfo)
	{
		/* Mask enabled */
		if (!byRop3b || byRop3b==byRop3a)
		{
			/* Legacy support : hard code rop3a to be destination */
			byRop3b = byRop3a;
			byRop3a = 0xAA;
		}
		/* else the caller must have defined the rop4 in the usual way */
	}
	else
	{
		/* No mask, so set ROPA = ROPB as required by SGX */
		byRop3b = byRop3a;
	}

	/* Put the rop code in the 2D control stream header word */
	psBltState->ui32BltRectControl |= (((IMG_UINT32)byRop3b<<EURASIA2D_ROP3B_SHIFT) | byRop3a);

	/* Legacy support: If it's a mask blt and rop3a is a nop then detect rop3b surfaces */
	if (pBltInfo->pMaskMemInfo && byRop3a == 0xAA)
	{
		byRop3a = byRop3b;
	}

	/*
		This tests that Source (0xCC) is set and it isn't Whiteness (0xFF)
	*/
	if ( ( (byRop3a>>2) ^ byRop3a ) & 0x33 )
	{
		psBltState->bSrcIsPresent = IMG_TRUE;
	}

	/*
		This tests that Pattern (0xF0) is set and it isn't Whiteness (0xFF)
	*/
	if ( ( (byRop3a>>4) ^ byRop3a ) & 0x0F )
	{
		if (pBltInfo->BlitFlags & PVR2D_BLIT_PAT_SURFACE_ENABLE)
		{
			/*
				Pattern is another surface. Nothing to change as
				psBltState->ui32BltRectControl Pattern Control is
				initialised to EURASIA2D_USE_PAT
			*/
		}
		else
		{
			/* Pattern is a solid colour */
			psBltState->ui32BltRectControl &= EURASIA2D_PAT_CLRMASK;
			psBltState->ui32BltRectControl |= EURASIA2D_USE_FILL;
		}
	}

	PVR2D_DPF((PVR_DBG_VERBOSE, "PVR2D SurfDetect: rop4 %04X:%s %s", psBltState->ui32BltRectControl & EURASIA2D_ROP4_MASK, (psBltState->bSrcIsPresent?" SOURCE":""),((psBltState->ui32BltRectControl & EURASIA2D_USE_PAT)?"PATTERN":"FILL")));
}
#endif //#if defined(SGX_FEATURE_2D_HARDWARE) && !defined(PVR2D_ALT_2DHW)




/*****************************************************************************
 @Function	PVR2DBltClipped

 @Input		hContext : PVR2D context handle

 @Input		pBltInfo : PVR2D blit info structure

 @Input		ulNumClipRects : number of clip rectangles

 @Input		pClipRects : array of clip rectangles

 @Return	error : PVR2D error code

 @Description : submit clipped blit
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DBltClipped(PVR2DCONTEXTHANDLE hContext,
						   PVR2DBLTINFO *pBltInfo,
						   PVR2D_ULONG ulNumClipRects,
						   PVR2DRECT *pClipRect)
{
#if defined(PVR2D_ALT_2DHW)

	// Blt via 3D core
	PVR2DERROR Ret = AltBltClipped (hContext, pBltInfo, ulNumClipRects, pClipRect);
	return Ret;

#else /* PVR2D_ALT_2DHW */
#if defined(SGX_FEATURE_2D_HARDWARE) && defined(TRANSFER_QUEUE)

	// Blt via 2D Core
	PVR2DERROR Ret;

	if (!pBltInfo)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	#if defined(FIX_HW_BRN_22694) // (SGX535 rev < 1.1.3)

	// HW BRN 22694 : Incorrect 2D Core output for blit size of X=8 and Y > 8.
	// Workaround: If x=8 & y>8, split the blt into two parts in the Y direction
	if ((pBltInfo->DSizeX == 8) && (pBltInfo->DSizeY > 8))
	{
		// Do width 4 blt
		pBltInfo->DSizeX -= 4;
		pBltInfo->SizeX -= 4;

		Ret = PVR2DBltVia2DCore(hContext, pBltInfo, ulNumClipRects, pClipRect);

		if (Ret == PVR2D_OK)
		{
			// Do the second Blt
			pBltInfo->DstX += 4;
			pBltInfo->SrcX += 4;

			Ret = PVR2DBltVia2DCore(hContext, pBltInfo, ulNumClipRects, pClipRect);

			pBltInfo->DstX -= 4;
			pBltInfo->SrcX -= 4;
		}
		pBltInfo->DSizeX += 4;
		pBltInfo->SizeX += 4;
	}
	else
	{
		Ret = PVR2DBltVia2DCore(hContext, pBltInfo, ulNumClipRects, pClipRect);
	}

	#else//#if defined(FIX_HW_BRN_22694)

	// Blt via 2D Core (SGX535 rev >= 1.1.3)
	Ret = PVR2DBltVia2DCore(hContext, pBltInfo, ulNumClipRects, pClipRect);

	#endif//#if defined(FIX_HW_BRN_22694)

	return Ret;

#else /* #if defined(SGX_FEATURE_2D_HARDWARE) && defined(TRANSFER_QUEUE) */

	// No blt available
	PVR_UNREFERENCED_PARAMETER(hContext);
	PVR_UNREFERENCED_PARAMETER(pBltInfo);
	PVR_UNREFERENCED_PARAMETER(ulNumClipRects);
	PVR_UNREFERENCED_PARAMETER(pClipRect);

	PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Blits not supported"));
	return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;

#endif /* #if defined(SGX_FEATURE_2D_HARDWARE) && defined(TRANSFER_QUEUE) */
#endif /* PVR2D_ALT_2DHW */

}//PVR2DBltClipped


#if defined(SGX_FEATURE_2D_HARDWARE) && !defined(PVR2D_ALT_2DHW)
static PVR2DERROR PVR2DBltVia2DCore(PVR2DCONTEXTHANDLE hContext,
						   PVR2DBLTINFO *pBltInfo,
						   PVR2D_ULONG ulNumClipRects,
						   PVR2DRECT *pClipRect)
{
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PVRSRV_CLIENT_MEM_INFO *pSrcMemInfo;
	PVRSRV_CLIENT_MEM_INFO *pDstMemInfo;
	PVRSRV_ERROR srvErr;
	PVR2DERROR pvr2dError;
	IMG_UINT32 ui32HwDstFormat;
	PVR2DRECT rcDst;
	PVR2D_BLTSTATE sBltState;
	IMG_UINT32 ui32BltCmdSizeBytes;
	IMG_UINT32 uPortData;
	IMG_INT32 i32XStart,i32YStart;
	PVRSRV_CLIENT_SYNC_INFO *psSrcSyncInfo;
	IMG_UINT32 ui32NumSrcSyncs;
	SGX_QUEUETRANSFER sBlitInfo;
	IMG_UINT32 uDstFormat;
	IMG_UINT32 ui32HwSrcFormat;
	IMG_UINT32 uSrcFormat;
	PVR2DRECT rcSrc;

	PVR2D_DPF((PVR_DBG_VERBOSE, "PVR2DBltClipped (queued blit) %ld clip rects", ulNumClipRects));

	/* Validate params */
	if (!psContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!pBltInfo->pDstMemInfo)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	/* Validate the TQ context (creates one if necessary) */
	pvr2dError = ValidateTransferContext(psContext);
	if (pvr2dError != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR,"PVR2DBltClipped:  ValidateTransferContext failed\n"));
		return pvr2dError;
	}

	if (ulNumClipRects)
	{
		if (!pClipRect || (ulNumClipRects > EURASIA2D_CLIPCOUNT_MAX))
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Invalid clip rect params"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
	}

	PVRSRVMemSet(&sBlitInfo,0,sizeof(sBlitInfo));

#if defined(PVR2D_VALIDATE_INPUT_PARAMS)
	// Deal with out of bounds and badly behaved rectangles
	pvr2dError = ValidateParams (pBltInfo, ulNumClipRects, pClipRect);
	if (pvr2dError != PVR2D_OK)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}
#endif

	if( pBltInfo->DSizeX <= 0 || pBltInfo->DSizeY <= 0)
	{
		return PVR2DERROR_INVALID_PARAMETER;
	}

	/* Initialise */
	sBltState.bSrcIsPresent = IMG_FALSE;
	sBltState.bDstIsPresent = IMG_FALSE;

	/* Default copy order is top left to bottom right */
	sBltState.bRTL = IMG_FALSE;
	sBltState.bBTT = IMG_FALSE;

	sBltState.ui32BltRectControl = EURASIA2D_BLIT_BH;
	sBltState.ui32BltRectControl |= EURASIA2D_DSTCK_DISABLE | EURASIA2D_SRCCK_DISABLE;
	sBltState.ui32BltRectControl |= EURASIA2D_USE_PAT;

	sBltState.pui32BltCmd = &sBltState.aui32BltCmd[0];

	_PVR2DSurfaceDetection(pBltInfo, &sBltState);

	/* Apply rotation */
	if (pBltInfo->BlitFlags & (PVR2D_BLIT_ROT_90|PVR2D_BLIT_ROT_180|PVR2D_BLIT_ROT_270))
	{
		if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_90)
		{
			sBltState.ui32BltRectControl |= EURASIA2D_ROT_90DEGS;
		}
		else if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_180)
		{
			sBltState.ui32BltRectControl |= EURASIA2D_ROT_180DEGS;
		}
		else
		{
			sBltState.ui32BltRectControl |= EURASIA2D_ROT_270DEGS;
		}
	}
	
	// Validate destination format
	if (pBltInfo->DstFormat & PVR2D_FORMAT_PVRSRV)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: 2D Core does not support 3D formats"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	uDstFormat = pBltInfo->DstFormat & PVR2D_FORMAT_MASK;

	if ((uDstFormat > PVR2D_NO_OF_FORMATS) || (!bValidOutputFormat[uDstFormat]))
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Illegal dest pixel format 0x%x", uDstFormat));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	ui32HwDstFormat = aConvertPVR2DToSGX2D[uDstFormat];

	// Dest rect
	rcDst.left	 = pBltInfo->DstX;
	rcDst.right	 = rcDst.left + pBltInfo->DSizeX;
	rcDst.top	 = pBltInfo->DstY;
	rcDst.bottom = rcDst.top + pBltInfo->DSizeY;

	// Do we need to check for badly behaved dest rect ?

	// EURASIA2D_DST_CTRL_BH

	// Destination surface: format and stride
	*sBltState.pui32BltCmd++ =
					 EURASIA2D_DST_SURF_BH
				   | ui32HwDstFormat
				   | (((IMG_UINT32)pBltInfo->DstStride << EURASIA2D_DST_STRIDE_SHIFT) & EURASIA2D_DST_STRIDE_MASK);

	// Destination surface: phys address
	*sBltState.pui32BltCmd++ =
					(((pBltInfo->pDstMemInfo->ui32DevAddr - psContext->s2DHeapBase.uiAddr + (IMG_UINT32)pBltInfo->DstOffset)
					 >> EURASIA2D_DST_ADDR_ALIGNSHIFT) << EURASIA2D_DST_ADDR_SHIFT)
					 & EURASIA2D_DST_ADDR_MASK;

	// ALPHA BLENDING
	if (pBltInfo->BlitFlags & (PVR2D_BLIT_PERPIXEL_ALPHABLEND_ENABLE | PVR2D_BLIT_GLOBAL_ALPHA_ENABLE | PVR2D_BLIT_FULLY_SPECIFIED_ALPHA_ENABLE))
	{
		pvr2dError =_PVR2DAlphaBlendControl(pBltInfo, &sBltState, psContext);
		if (pvr2dError != PVR2D_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: AlphaBlendControl failed (0x%x)", pvr2dError));

			return pvr2dError;
		}
	}

	// Source
	uSrcFormat = pBltInfo->SrcFormat & PVR2D_FORMAT_MASK;

	if (sBltState.bSrcIsPresent)
	{
		if (!pBltInfo->pSrcMemInfo)
		{
			return PVR2DERROR_INVALID_PARAMETER;
		}

		// 2D Core does not support 3D formats
		if (pBltInfo->SrcFormat & PVR2D_FORMAT_PVRSRV)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: 2D Core does not support 3D formats"));
			return PVR2DERROR_INVALID_PARAMETER;
		}

		if( pBltInfo->SizeX < 0 || pBltInfo->SizeY < 0)
		{
			return PVR2DERROR_INVALID_PARAMETER;
		}

		/* Is scaling required? */
		if (pBltInfo->BlitFlags & (PVR2D_BLIT_ROT_90|PVR2D_BLIT_ROT_270))
		{
			if( (pBltInfo->SizeX != pBltInfo->DSizeY) ||
				 (pBltInfo->SizeY != pBltInfo->DSizeX))
			{
				/* Scaling not supported on SGX */
				return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
			}
		}
		else
		{
			if( (pBltInfo->SizeX != pBltInfo->DSizeX) ||
				 (pBltInfo->SizeY != pBltInfo->DSizeY))
			{
				/* Scaling not supported on SGX */
				return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
			}
		}

		if (uSrcFormat > PVR2D_PAL8)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Unknown src format 0x%x", uSrcFormat));
			return PVR2DERROR_GENERIC_ERROR;
		}

		ui32HwSrcFormat = aConvertPVR2DToSGX2D[uSrcFormat];

		// Add const rgb for A8 and A4 format alpha-only source surface
		if (ui32HwSrcFormat == EURASIA2D_SRC_8_ALPHA || ui32HwSrcFormat == EURASIA2D_SRC_4_ALPHA)
		{
			sBltState.ui32BltRectControl &= (~EURASIA2D_PAT_MASK); // zero this bit to add a fill colour to the command stream
		}

#if defined(EUR_CR_2D_ALPHA_COMPONENT_ZERO_MASK)
		if ((psContext->ui32AlphaRegValue != DEFAULT_ALPHA1555) && (ui32HwSrcFormat == EURASIA2D_SRC_1555ARGB))
		{
			// Promote 1555 to 1555 lookup format for core rev >= 111
			ui32HwSrcFormat = EURASIA2D_SRC_1555ARGB_LOOKUP;
		}
#endif
		// Src rect
		rcSrc.left = pBltInfo->SrcX;
		rcSrc.right = rcSrc.left + pBltInfo->SizeX;
		rcSrc.top = pBltInfo->SrcY;
		rcSrc.bottom = rcSrc.top + pBltInfo->SizeY;

		// EURASIA2D_SRC_CTRL_BH

		// Source format and stride
		*sBltState.pui32BltCmd++ = EURASIA2D_SRC_SURF_BH
						| ui32HwSrcFormat
						| (((IMG_UINT32)pBltInfo->SrcStride << EURASIA2D_SRC_STRIDE_SHIFT) & EURASIA2D_SRC_STRIDE_MASK);

		// Source surface address
		*sBltState.pui32BltCmd++ =
						(((pBltInfo->pSrcMemInfo->ui32DevAddr - psContext->s2DHeapBase.uiAddr + (IMG_UINT32)pBltInfo->SrcOffset)
						>> EURASIA2D_SRC_ADDR_ALIGNSHIFT) << EURASIA2D_SRC_ADDR_SHIFT)
						& EURASIA2D_SRC_ADDR_MASK;

		// Check for source palette
		if (aRequiresPalette[uSrcFormat])
		{
			if (!pBltInfo->pPalMemInfo)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: No source palette supplied"));
				return PVR2DERROR_INVALID_PARAMETER;
			}

			uPortData = (pBltInfo->pPalMemInfo->ui32DevAddr - psContext->s2DHeapBase.uiAddr + (IMG_UINT32)pBltInfo->PalOffset);

			// Check for palette alignment errors
			if (uPortData & 0x0F)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Source palette alignment error"));
				return PVR2DERROR_INVALID_PARAMETER;
			}

			/* Send source palette header */
			*sBltState.pui32BltCmd++ = EURASIA2D_SRC_PAL_BH | uPortData;
		}

		//  Source X and Y start
		i32XStart = rcSrc.left;
		i32YStart = rcSrc.top;

		//  Source X and Y start : Test for blt rotation from unrotated src to rotated dst
		if (pBltInfo->BlitFlags & (PVR2D_BLIT_ROT_90|PVR2D_BLIT_ROT_180|PVR2D_BLIT_ROT_270))
		{
			if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_90)
			{
				// 90 degree rotation
				i32YStart += pBltInfo->SizeY -1;
			}
			else if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_180)
			{
				// 180 degree rotation
				i32XStart += pBltInfo->SizeX -1;
				i32YStart += pBltInfo->SizeY -1;
			}
			else
			{
				// 270 degree rotation
				i32XStart  += pBltInfo->SizeX -1;
			}
		}
		// Allow caller to override the copy order
		else if (pBltInfo->BlitFlags & (PVR2D_BLIT_COPYORDER_TL2BR|PVR2D_BLIT_COPYORDER_BR2TL|PVR2D_BLIT_COPYORDER_TR2BL|PVR2D_BLIT_COPYORDER_BL2TR))
		{
			if (pBltInfo->BlitFlags & PVR2D_BLIT_COPYORDER_BR2TL)
			{
				sBltState.ui32BltRectControl |= EURASIA2D_COPYORDER_BR2TL;
				sBltState.bRTL = IMG_TRUE;
				sBltState.bBTT = IMG_TRUE;
			}
			else if(pBltInfo->BlitFlags & PVR2D_BLIT_COPYORDER_TR2BL)
			{
				sBltState.ui32BltRectControl |= EURASIA2D_COPYORDER_TR2BL;
				sBltState.bRTL = IMG_TRUE;
			}
			else if(pBltInfo->BlitFlags & PVR2D_BLIT_COPYORDER_BL2TR)
			{
				sBltState.ui32BltRectControl |= EURASIA2D_COPYORDER_BL2TR;
				sBltState.bBTT = IMG_TRUE;
			}
			// else EURASIA2D_COPYORDER_TL2BR, which is zero so no need to OR it in
			i32XStart = BLT_STARTX(&sBltState, pBltInfo->SrcX, pBltInfo->SizeX);
			i32YStart = BLT_STARTY(&sBltState, pBltInfo->SrcY, pBltInfo->SizeY);
		}
		// Source X and Y start : Test for possible intersection of Src and Dst
		// only if on same surface with no relative rotation
		else if (pBltInfo->pSrcMemInfo == pBltInfo->pDstMemInfo)
		{
			// Check forward/backward blit requirements
			if (		(rcSrc.bottom > rcDst.top)
					&&	(rcSrc.top < rcDst.bottom)
					&&	(rcSrc.right > rcDst.left)
					&&	(rcSrc.left < rcDst.right))
			{
				/* Is copy order right to left ? */
				sBltState.bRTL = (IMG_BOOL)(rcSrc.left < rcDst.left);
				/* Is copy order bottom to top ? */
				sBltState.bBTT = (IMG_BOOL)(rcSrc.top < rcDst.top);

				sBltState.ui32BltRectControl |=
					sBltState.bRTL ?
						(sBltState.bBTT ?
							EURASIA2D_COPYORDER_BR2TL :
							EURASIA2D_COPYORDER_TR2BL) :
						(sBltState.bBTT ? EURASIA2D_COPYORDER_BL2TR :
							EURASIA2D_COPYORDER_TL2BR);

				i32XStart = BLT_STARTX(&sBltState, pBltInfo->SrcX, pBltInfo->SizeX);
				i32YStart = BLT_STARTY(&sBltState, pBltInfo->SrcY, pBltInfo->SizeY);
			}
		}

		// EURASIA2D_SRC_OFF_BH

		// Source offset
		*sBltState.pui32BltCmd++ = EURASIA2D_SRC_OFF_BH
						| (((IMG_UINT32)i32XStart << EURASIA2D_SRCOFF_XSTART_SHIFT) & EURASIA2D_SRCOFF_XSTART_MASK)
						| (((IMG_UINT32)i32YStart << EURASIA2D_SRCOFF_YSTART_SHIFT) & EURASIA2D_SRCOFF_YSTART_MASK);
	}
	else
	{
		/* Copy order is left to right, top to bottom */
		sBltState.ui32BltRectControl |= EURASIA2D_COPYORDER_TL2BR;
	}

	// Clip Control
	if (ulNumClipRects)
	{
		_2DClipControl(pClipRect, pBltInfo, &sBltState);
	}

	// COLOUR KEY
	if (pBltInfo->BlitFlags & PVR2D_BLIT_CK_ENABLE)
	{
		/* colour key enabled */
		_PVR2DCKControl(pBltInfo, &sBltState);
	}


	// MASK CONTROL
	if (pBltInfo->pMaskMemInfo)
	{
		_PVR2DMaskControl(psContext, pBltInfo, &sBltState);
	}

	// PATTERN CONTROL
	if (pBltInfo->BlitFlags & PVR2D_BLIT_PAT_SURFACE_ENABLE)
	{
		if (!pBltInfo->pSrcMemInfo)
		{
			return PVR2DERROR_INVALID_PARAMETER;
		}
		pvr2dError =_PVR2DPatternControl(psContext, pBltInfo, &sBltState, uSrcFormat);
		if (pvr2dError != PVR2D_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: PatternControl failed (0x%x)", pvr2dError));
			return pvr2dError;
		}
	}

	// BLIT START

	// BltRectControl sets: Texture Copy Order, SRC & DST Colourkey Control,
	// Clipping Control, Alpha Control, Pattern Control and ROP3_B & ROP3_A
	*sBltState.pui32BltCmd++ = sBltState.ui32BltRectControl;

	// NB: if pattern surface is placed here, it wouldn't work
	if ((sBltState.ui32BltRectControl & EURASIA2D_PAT_MASK) == EURASIA2D_USE_FILL)
	{
		*sBltState.pui32BltCmd++ = pBltInfo->Colour & EURASIA2D_FILLCOLOUR_MASK;
	}

	// Dest start
	*sBltState.pui32BltCmd++ =
				(((IMG_UINT32)BLT_STARTX(&sBltState, rcDst.left, pBltInfo->DSizeX) << EURASIA2D_DST_XSTART_SHIFT) & EURASIA2D_DST_XSTART_MASK)
				| (((IMG_UINT32)BLT_STARTY(&sBltState, rcDst.top, pBltInfo->DSizeY) << EURASIA2D_DST_YSTART_SHIFT) & EURASIA2D_DST_YSTART_MASK);

	// Dest size
	*sBltState.pui32BltCmd++ =
				  (( (IMG_UINT32)(rcDst.right - rcDst.left) << EURASIA2D_DST_XSIZE_SHIFT) & EURASIA2D_DST_XSIZE_MASK)
				| (( (IMG_UINT32)(rcDst.bottom - rcDst.top) << EURASIA2D_DST_YSIZE_SHIFT) & EURASIA2D_DST_YSIZE_MASK);

	PVR2DTraceBlt(&sBltState);

	ui32BltCmdSizeBytes = (IMG_UINT32)((IMG_UINT8*)sBltState.pui32BltCmd - (IMG_UINT8*)&sBltState.aui32BltCmd[0]);
	PVR_ASSERT(ui32BltCmdSizeBytes <= (PVR2D_MAX_BLIT_CMD_SIZE << 2));

	ui32NumSrcSyncs = 0;
	psSrcSyncInfo = IMG_NULL;
	
	// If there is a source then patch in the sync object - also for patterns
	if (pBltInfo->pSrcMemInfo && (sBltState.bSrcIsPresent || ((pBltInfo->BlitFlags & PVR2D_BLIT_PAT_SURFACE_ENABLE) != 0)))
	{
		PVRSRV_CLIENT_SYNC_INFO *psSyncInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(pBltInfo->pSrcMemInfo)->psClientSyncInfo;

		if (psSyncInfo && (psSyncInfo != CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(pBltInfo->pDstMemInfo)->psClientSyncInfo))
		{
			ui32NumSrcSyncs = 1;
			psSrcSyncInfo = psSyncInfo;
		}
	}

#ifdef PDUMP // Capture 2D Core input surfaces
	PdumpInputSurfaces( psContext,
						(sBltState.bSrcIsPresent||((pBltInfo->BlitFlags & PVR2D_BLIT_PAT_SURFACE_ENABLE) != 0))
						? IMG_TRUE : IMG_FALSE, pBltInfo);
#endif//#ifdef PDUMP

	// Blt via 2D core only, not via 3D core or blitlib (because TQ blt params have not been set up)
	sBlitInfo.ui32Flags = SGX_TRANSFER_DISPATCH_DISABLE_SW;
	sBlitInfo.eType = SGXTQ_2DHWBLIT;
	sBlitInfo.Details.s2DBlit.ui32CtrlSizeInDwords = (ui32BltCmdSizeBytes >> 2);
	sBlitInfo.Details.s2DBlit.pui32CtrlStream = sBltState.aui32BltCmd;

#if defined(EUR_CR_2D_ALPHA_COMPONENT_ZERO_MASK)
	sBlitInfo.Details.s2DBlit.ui32AlphaRegValue = psContext->ui32AlphaRegValue;
#endif

	if (sBltState.bSrcIsPresent)
	{
		pSrcMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBltInfo->pSrcMemInfo->hPrivateData;
		SGX_TQSURFACE_SET_ADDR(sBlitInfo.asSources[0], pSrcMemInfo, pBltInfo->SrcOffset);
	}
	pDstMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBltInfo->pDstMemInfo->hPrivateData;
	SGX_TQSURFACE_SET_ADDR(sBlitInfo.asDests[0], pDstMemInfo, pBltInfo->DstOffset);

	sBlitInfo.ui32NumSources = ui32NumSrcSyncs;
	sBlitInfo.asSources[0].psSyncInfo = psSrcSyncInfo;
	sBlitInfo.ui32NumDest = 1;
	sBlitInfo.asDests[0].psSyncInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(pBltInfo->pDstMemInfo)->psClientSyncInfo;

#if defined(PDUMP)
	sBlitInfo.bPDumpContinuous = IMG_FALSE;
	PVRSRVPDumpComment(psContext->psServices, "PVR2D:SGX2DQueueBlit", IMG_FALSE);
#endif
	
	// Transfer queue (2D Core BltClipped)
	srvErr = SGXQueueTransfer(psContext->hTransferContext, &sBlitInfo);

	if (srvErr != PVRSRV_OK)
	{
		if (srvErr == PVRSRV_ERROR_TIMEOUT)
		{
			return PVR2DERROR_DEVICE_UNAVAILABLE;
		}
		else
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Couldn't do the blit (0x%x)", srvErr));
			return PVR2DERROR_GENERIC_ERROR;
		}
	}

#ifdef SYNC_DEBUG
	// Debug with synchronous blts
	PVR2DQueryBlitsComplete(hContext, pBltInfo->pDstMemInfo, 1);
#endif

#ifdef PDUMP // Capture 2D Core output surface
	PVRSRVPDumpComment(psContext->psServices, "PVR2D: poll for 2D Core blt complete", IMG_FALSE);
	PVRSRVPDumpSyncPol(	psContext->psServices,
#if defined (SUPPORT_SID_INTERFACE)
						pDstMemInfo->psClientSyncInfo->hKernelSyncInfo,
#else
						pDstMemInfo->psClientSyncInfo,
#endif
						IMG_FALSE,
						pDstMemInfo->psClientSyncInfo->psSyncData->ui32LastOpDumpVal,
						0xFFFFFFFF);
	PdumpOutputSurface(psContext, pBltInfo);
#endif

	return PVR2D_OK;
}
#endif //#if defined(SGX_FEATURE_2D_HARDWARE) && !defined(PVR2D_ALT_2DHW)


/******************************************************************************
 @Function	PVR2DBlt

 @Input		hContext : PVR2D context handle

 @Input		pBltInfo : PVR2D blit info structure

 @Return	error : PVR2D error code

 @Description : submit blit request
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DBlt (PVR2DCONTEXTHANDLE hContext, PVR2DBLTINFO *pBltInfo)
{
	return PVR2DBltClipped(hContext, pBltInfo, 0, IMG_NULL);
}



/******************************************************************************
 @Function	LocalAbsDiff

 @Input		Two IMG_UINT32's

 @Return	returns the difference between the two inputs

 @Description : unsigned diff (compiler should inline this)
******************************************************************************/
static IMG_UINT32 LocalAbsDiff(const IMG_UINT32 a, const IMG_UINT32 b)
{
	if (a > b)
	{
		return a - b;
	}
	else
	{
		return b - a;
	}
}


/******************************************************************************
 @Function	OpsComplete

 @Input		pMemInfo : Client Mem info structure

 @Return	PVR2D_TRUE if all reading and writing have completed, else PVR2D_FALSE

 @Description : tests if all reading and writing have completed on a surface
******************************************************************************/
static PVR2D_BOOL OpsComplete(const PVRSRV_CLIENT_MEM_INFO *psMemInfo,
						const IMG_UINT32 ui32WriteOpsPending, const IMG_UINT32 ui32ReadOpsPending,
						const IMG_UINT32 ui32ReadOps2Pending)
{
	const IMG_UINT32 ui32WriteOpsComplete = psMemInfo->psClientSyncInfo->psSyncData->ui32WriteOpsComplete;
	const IMG_UINT32 ui32ReadOpsComplete = psMemInfo->psClientSyncInfo->psSyncData->ui32ReadOpsComplete;
	const IMG_UINT32 ui32ReadOps2Complete = psMemInfo->psClientSyncInfo->psSyncData->ui32ReadOps2Complete;

	if ( LocalAbsDiff(ui32WriteOpsPending, ui32WriteOpsComplete) > 0x80000000 )
	{
		// Op complete counter has wrapped so the opposite to the normal rule applies
		if (ui32WriteOpsComplete > ui32WriteOpsPending)
		{
			// op not complete
			return PVR2D_FALSE;
		}
	}
	else if (ui32WriteOpsComplete < ui32WriteOpsPending) // normal rule
	{
		// op not complete
		return PVR2D_FALSE;
	}

	if ( LocalAbsDiff(ui32ReadOpsPending, ui32ReadOpsComplete) > 0x80000000 )
	{
		// Op complete counter has wrapped so the opposite to the normal rule applies
		if (ui32ReadOpsComplete > ui32ReadOpsPending)
		{
			// op not complete
			return PVR2D_FALSE;
		}
	}
	else if (ui32ReadOpsComplete < ui32ReadOpsPending) // normal rule
	{
		// op not complete
		return PVR2D_FALSE;
	}

	if ( LocalAbsDiff(ui32ReadOps2Pending, ui32ReadOps2Complete) > 0x80000000 )
	{
		// Op complete counter has wrapped so the opposite to the normal rule applies
		if (ui32ReadOps2Complete > ui32ReadOps2Pending)
		{
			// op not complete
			return PVR2D_FALSE;
		}
	}
	else if (ui32ReadOps2Complete < ui32ReadOps2Pending) // normal rule
	{
		// op not complete
		return PVR2D_FALSE;
	}

	return PVR2D_TRUE;
}

/******************************************************************************
 @Function	PVR2DQueryBlitsComplete

 @Input		hContext : PVR2D context handle

 @Input		pMemInfo : PVR2D Surface info structure

 @Input		uiWaitForComplete : Boolean to signal whether to wait for complete

 @Return	error : PVR2D error code

 @Description : test if blits are complete on a surface
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DQueryBlitsComplete(PVR2DCONTEXTHANDLE hContext,
	const PVR2DMEMINFO *pMemInfo,
	PVR2D_UINT uiWaitForComplete)
{
#if defined(PVR2D_ALT_2DHW) || defined(SGX_FEATURE_2D_HARDWARE)
	const PVR2DCONTEXT				*psContext = (PVR2DCONTEXT *)hContext;
	const PVRSRV_CLIENT_MEM_INFO	*psClientMemInfo;

	PVRSRV_ERROR	eRet;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DQueryBlitsComplete - Invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!pMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DQueryBlitsComplete: Invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	psClientMemInfo = (PVRSRV_CLIENT_MEM_INFO *)pMemInfo->hPrivateData;

	if (!psClientMemInfo || !psClientMemInfo->psClientSyncInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DQueryBlitsComplete: Invalid MemInfo"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	eRet = SGX2DQueryBlitsComplete(&psContext->sDevData, psClientMemInfo->psClientSyncInfo, uiWaitForComplete);

	if (eRet == PVRSRV_OK)
	{
		// Completed
		return PVR2D_OK;
	}
	else if (eRet == PVRSRV_ERROR_CMD_NOT_PROCESSED)
	{
		// if !uiWaitForComplete and blt not complete
		return PVR2DERROR_BLT_NOTCOMPLETE;
	}
	else if (eRet == PVRSRV_ERROR_TIMEOUT)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DQueryBlitsComplete: timeout. sync object failed to complete"));
		return PVR2DERROR_BLT_NOTCOMPLETE; // Timed out
	}

	return PVR2D_OK;

#else /* PVR2D_ALT_2DHW || SGX_FEATURE_2D_HARDWARE */

	PVR_UNREFERENCED_PARAMETER(hContext);
	PVR_UNREFERENCED_PARAMETER(pMemInfo);
	PVR_UNREFERENCED_PARAMETER(uiWaitForComplete);

	// No outstanding blts
	return PVR2D_OK;
#endif /* PVR2D_ALT_2DHW || SGX_FEATURE_2D_HARDWARE */

} // PVR2DQueryBlitsComplete


#if defined(SGX_FEATURE_2D_HARDWARE) && !defined(PVR2D_ALT_2DHW)
static void SetPresentBltSrcStride(PVR2DCONTEXT *psContext, IMG_INT32 i32SrcStride)
{
	psContext->aui32PresentBltMiscStateBlock[PRESENT_BLT_MISC_SRC_HEADER] &= EURASIA2D_SRC_STRIDE_CLRMASK;
	psContext->aui32PresentBltMiscStateBlock[PRESENT_BLT_MISC_SRC_HEADER] |= (((IMG_UINT32)i32SrcStride << EURASIA2D_SRC_STRIDE_SHIFT) & EURASIA2D_SRC_STRIDE_MASK);
}

static void SetPresentBltSize(PVR2DCONTEXT *psContext, IMG_UINT32 ui32DstWidth, IMG_UINT32 ui32DstHeight)
{
	psContext->aui32PresentBltBlock[PRESENT_BLT_SIZE] = ((ui32DstWidth << EURASIA2D_DST_XSIZE_SHIFT) & EURASIA2D_DST_XSIZE_MASK) |
										 ((ui32DstHeight << EURASIA2D_DST_YSIZE_SHIFT) & EURASIA2D_DST_YSIZE_MASK);

}

static void SetPresentBltPos(PVR2DCONTEXT *psContext, IMG_INT32 i32XPos, IMG_INT32 i32YPos)
{
	psContext->aui32PresentBltBlock[PRESENT_BLT_START] = (((IMG_UINT32)i32XPos << EURASIA2D_DST_XSTART_SHIFT) & EURASIA2D_DST_XSTART_MASK) |
										 (((IMG_UINT32)i32YPos  << EURASIA2D_DST_YSTART_SHIFT) & EURASIA2D_DST_YSTART_MASK);
}

static void GetPresentBltPos(PVR2DCONTEXT *psContext, IMG_INT32* i32XPos, IMG_INT32* i32YPos)
{
	*i32XPos = (IMG_INT32)((psContext->aui32PresentBltBlock[PRESENT_BLT_START] & EURASIA2D_DST_XSTART_MASK) >> EURASIA2D_DST_XSTART_SHIFT);
	*i32YPos = (IMG_INT32)((psContext->aui32PresentBltBlock[PRESENT_BLT_START] & EURASIA2D_DST_YSTART_MASK) >> EURASIA2D_DST_YSTART_SHIFT);
}

static void WriteClipBlock(PVR2D_CLIPBLOCK *psBltClipBlock, PVR2DRECT *pClipRect, const IMG_INT32 i32XPos, const IMG_INT32 i32YPos)
{
	psBltClipBlock->aui32Data[CLIPBLOCK_HEADER] = EURASIA2D_CLIP_BH

		| (((IMG_UINT32)(pClipRect->right-i32XPos) << EURASIA2D_CLIP_XMAX_SHIFT) & EURASIA2D_CLIP_XMAX_MASK)
												| (((IMG_UINT32)(pClipRect->left-i32XPos) << EURASIA2D_CLIP_XMIN_SHIFT) & EURASIA2D_CLIP_XMIN_MASK);

	psBltClipBlock->aui32Data[CLIPBLOCK_YMAXMIN] = (((IMG_UINT32)(pClipRect->bottom-i32YPos) << EURASIA2D_CLIP_YMAX_SHIFT) & EURASIA2D_CLIP_YMAX_MASK)
												 | (((IMG_UINT32)(pClipRect->top-i32YPos) << EURASIA2D_CLIP_YMIN_SHIFT) & EURASIA2D_CLIP_YMIN_MASK);
}
#endif /* SGX_FEATURE_2D_HARDWARE && !PVR2D_ALT_2DHW */


/******************************************************************************
 @Function	PVR2DSetPresentBltProperties

 @Input		hContext : pvr2d context handle

 @Input		ulPropertyMask : Mask of properties to set

 @Input		lSrcStride : Stride of back buffer

 @Input		ulDstWidth : The width of the blit

 @Input		ulDstHeight : The height of the blit

 @Input		lDstXPos : X Position on dest surface for presentation

 @Input		lDstYPos : Y Position on dest surface for presentation

 @Input		ulNumClipRects : Number of cliprects present in pClipRects param

 @Input		pClipRects : List of ulNumClipRects to apply to presentation

 @Input		ulSwapInterval  : Number of display intervals to wait before
							  presentation(currently unsupported)

 @Return	error : PVR2D error code

 @Description : set presentation properties for flipping or blitting
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DSetPresentBltProperties (PVR2DCONTEXTHANDLE hContext,
										PVR2D_ULONG ulPropertyMask,
										PVR2D_LONG lSrcStride,
										PVR2D_ULONG ulDstWidth,
										PVR2D_ULONG ulDstHeight,
										PVR2D_LONG lDstXPos,
										PVR2D_LONG lDstYPos,
										PVR2D_ULONG ulNumClipRects,
										PVR2DRECT *pClipRects,
										PVR2D_ULONG ulSwapInterval)
{
	PVR2DCONTEXT *psContext;
	PVR2D_CLIPBLOCK *psBltClipBlock=0;
	IMG_UINT32 ulNumClipBlocks = ulNumClipRects;

	PVR_UNREFERENCED_PARAMETER(ulSwapInterval);

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DSetPresentBltProperties - Invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	psContext = (PVR2DCONTEXT *)hContext;

	if(!psContext->hDisplayClassDevice)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DSetPresentBltProperties - ERROR: No Display Surface to present to"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_INTERVAL)
	{
		PVR2D_DPF((PVR_DBG_WARNING,
			"PVR2DSetPresentBltProperties - swap interval not currently supported for presentation blitting!"));
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_CLIPRECTS)
	{
		if(ulNumClipBlocks > psContext->ui32NumClipBlocks)
		{
			psContext->psBltClipBlock = PVR2DRealloc(psContext, psContext->psBltClipBlock, ulNumClipBlocks * sizeof(PVR2D_CLIPBLOCK));
			if(!psContext->psBltClipBlock)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2DSetPresentBltProperties error - Out of memory!"));
				return PVR2DERROR_MEMORY_UNAVAILABLE;
			}

			psContext->ui32NumClipBlocks = ulNumClipBlocks;
		}

		psContext->ui32NumActiveClipBlocks = ulNumClipBlocks;
		psBltClipBlock = psContext->psBltClipBlock;
	}

#if defined(PVR2D_ALT_2DHW)

	if (ulPropertyMask & PVR2D_PRESENT_PROPERTY_SRCSTRIDE)
	{
		psContext->lPresentBltSrcStride  = lSrcStride;
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_DSTSIZE)
	{
		psContext->ulPresentBltDstWidth  = ulDstWidth;
		psContext->ulPresentBltDstHeight  = ulDstHeight;
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_DSTPOS)
	{
		psContext->lPresentBltDstXPos  =  lDstXPos;
		psContext->lPresentBltDstYPos  =  lDstYPos;
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_CLIPRECTS)
	{
		while(ulNumClipBlocks)
		{
			ulNumClipBlocks--;
			psBltClipBlock->rcRect = *pClipRects++;	/* PRQA S 0505 */ /* not null ptr */
			psBltClipBlock++;
		}
	}

	PVR_UNREFERENCED_PARAMETER(ulSwapInterval);

	return PVR2D_OK;

#else /* PVR2D_ALT_2DHW */
#if defined(SGX_FEATURE_2D_HARDWARE)

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_SRCSTRIDE)
	{
		SetPresentBltSrcStride(psContext, lSrcStride);
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_DSTSIZE)
	{
		SetPresentBltSize(psContext, ulDstWidth, ulDstHeight);
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_DSTPOS)
	{
		SetPresentBltPos(psContext, lDstXPos, lDstYPos);
	}

	if(ulPropertyMask & PVR2D_PRESENT_PROPERTY_CLIPRECTS)
	{
		IMG_INT32 i32XPos, i32YPos;

		GetPresentBltPos(psContext, &i32XPos, &i32YPos);

		if(ulNumClipRects > 0)
		{
			psContext->aui32PresentBltBlock[PRESENT_BLT_HEADER] |= EURASIA2D_CLIP_ENABLE;
		}
		else
		{
			psContext->aui32PresentBltBlock[PRESENT_BLT_HEADER] &= ~EURASIA2D_CLIP_ENABLE;
		}

		for(; ulNumClipRects > 0; ulNumClipRects--)
		{
			WriteClipBlock(psBltClipBlock++, pClipRects++, i32XPos, i32YPos);	/* PRQA S 0509 */ /* not null ptr */
		}
	}

	return PVR2D_OK;

#else /* SGX_FEATURE_2D_HARDWARE */

	PVR_UNREFERENCED_PARAMETER(hContext);
	PVR_UNREFERENCED_PARAMETER(ulPropertyMask);
	PVR_UNREFERENCED_PARAMETER(lSrcStride);
	PVR_UNREFERENCED_PARAMETER(ulDstWidth);
	PVR_UNREFERENCED_PARAMETER(ulDstHeight);
	PVR_UNREFERENCED_PARAMETER(lDstXPos);
	PVR_UNREFERENCED_PARAMETER(lDstYPos);
	PVR_UNREFERENCED_PARAMETER(ulNumClipRects);
	PVR_UNREFERENCED_PARAMETER(pClipRects);
	PVR_UNREFERENCED_PARAMETER(ulSwapInterval);

	PVR2D_DPF((PVR_DBG_ERROR, "PVR2DSetPresentBltProperties: Blits not supported"));

	return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;

#endif /* SGX_FEATURE_2D_HARDWARE */
#endif /* PVR2D_ALT_2DHW */

} // PVR2DSetPresentBltProperties


/******************************************************************************
 @Function	PVR2DPresentBlt

 @Input		hContext : PVR2D context handle

 @Input		pMemInfo : PVR2D mem info structure for source of blit

 @Input		renderID : Meta data for present

 @Return	error : PVR2D error code

 @Description : submit queued presentation blit
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DPresentBlt (PVR2DCONTEXTHANDLE hContext,
							PVR2DMEMINFO *pMemInfo,
							PVR2D_LONG lRenderID)
{
#if defined(PVR2D_ALT_2DHW)
	PVR2DERROR Ret=PVR2D_OK;
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	IMG_UINT32	ui32ClipBlocks;
	PVR2D_CLIPBLOCK *psBltClipBlock;

	PVR_UNREFERENCED_PARAMETER(lRenderID);

	if (!psContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentBlt - ERROR: Invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if (!pMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentBlt - ERROR: Invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if (!psContext->hDisplayClassDevice)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentBlt - ERROR: No Display Surface to present to"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	ui32ClipBlocks = psContext->ui32NumActiveClipBlocks;

	if (ui32ClipBlocks)
	{
		// Process the clip list if there is one
		psBltClipBlock = psContext->psBltClipBlock;

		while (ui32ClipBlocks)
		{
			ui32ClipBlocks--;
			Ret = AltPresentBlt (hContext, pMemInfo, &psBltClipBlock->rcRect);

			if (Ret != PVR2D_OK)
			{
				break;
			}

			psBltClipBlock++;
		}
	}
	else
	{
		Ret = AltPresentBlt (hContext, pMemInfo, 0); // No clip rect
	}

	return Ret;

#else /* PVR2D_ALT_2DHW */
#if defined(SGX_FEATURE_2D_HARDWARE) && defined(TRANSFER_QUEUE)
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo;
	IMG_UINT32 ui32NumSrcSyncs = 0;
	IMG_UINT32 ui32BltSize, i, j;
	SGX_QUEUETRANSFER sBlitInfo;
	PVRSRV_ERROR srvErr;
	PVR2DERROR Ret;

	PVR_UNREFERENCED_PARAMETER(lRenderID);

	PVRSRVMemSet(&sBlitInfo,0,sizeof(sBlitInfo));

	if(!psContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentBlt - ERROR: Invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if(!psContext->hDisplayClassDevice)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentBlt - ERROR: No Display Surface to present to"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	if(!pMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentBlt - ERROR: Invalid params"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	/* Validate the TQ context (creates one if necessary) */
	Ret = ValidateTransferContext(psContext);
	if (Ret != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR,"PVR2DPresentBlt:  ValidateTransferContext failed\n"));
		return Ret;
	}

	psSyncInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(pMemInfo)->psClientSyncInfo;

	if(psSyncInfo)
	{
		ui32NumSrcSyncs = 1;
	}

	/* adjust the source address of the blit */
	psContext->aui32PresentBltMiscStateBlock[PRESENT_BLT_MISC_SRC_ADDR] = (((pMemInfo->ui32DevAddr - psContext->s2DHeapBase.uiAddr)
											>>	EURASIA2D_SRC_ADDR_ALIGNSHIFT)
											<<	EURASIA2D_SRC_ADDR_SHIFT)
											&	EURASIA2D_SRC_ADDR_MASK;

#if defined(PDUMP) && defined(CAPTURE_PRESENTBLT_INPUT)
	{
		PVR2DRECT rcSrcRect;

		// Get source rectangle
		rcSrcRect.top = 0; // PVR2D PresentBlt source offset is always 0
		rcSrcRect.left = 0;
		rcSrcRect.bottom = psContext->aui32PresentBltBlock[PRESENT_BLT_SIZE] & EURASIA2D_DST_YSIZE_MASK;
		rcSrcRect.right = (psContext->aui32PresentBltBlock[PRESENT_BLT_SIZE] & EURASIA2D_DST_XSIZE_MASK)>>EURASIA2D_DST_XSIZE_SHIFT;

		// Capture present blt input surface
		PdumpPresentBltInputSurface(psContext, pMemInfo, &rcSrcRect);
	}
#endif//#if defined(PDUMP)

	/* Setup the common data */
#if defined(PDUMP)
	sBlitInfo.bPDumpContinuous = IMG_FALSE;
#endif
	sBlitInfo.eType = SGXTQ_2DHWBLIT;
	sBlitInfo.ui32NumSources = ui32NumSrcSyncs;
	sBlitInfo.ui32Flags = SGX_TRANSFER_DISPATCH_DISABLE_SW;
	sBlitInfo.asSources[0].psSyncInfo = psSyncInfo;
	sBlitInfo.ui32NumDest = 1;
	sBlitInfo.asDests[0].psSyncInfo = CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(&psContext->sPrimaryBuffer)->psClientSyncInfo;

	if (psContext->ui32NumActiveClipBlocks == 0)
	{
		ui32BltSize = 0;

		/* knit misc. state and blit blocks together */
		for(i = 0; i < SIZEOF_PRESENT_BLT_MISC_STATE_BLOCK; i++)
		{
			psContext->aui32PresentBltData[ui32BltSize++] = psContext->aui32PresentBltMiscStateBlock[i];
		}
		for(i=0; i < SIZEOF_PRESENT_BLT_BLOCK; i++)
		{
			psContext->aui32PresentBltData[ui32BltSize++] = psContext->aui32PresentBltBlock[i];
		}

		PVR_ASSERT(ui32BltSize <= SIZEOF_PRESENT_BLT_DATA);

		/* Setup the rest of the command */
		sBlitInfo.Details.s2DBlit.ui32CtrlSizeInDwords = ui32BltSize;
		sBlitInfo.Details.s2DBlit.pui32CtrlStream = psContext->aui32PresentBltData;
		
		/* Transfer queue (2D Core PresentBlt) */
		srvErr = SGXQueueTransfer(psContext->hTransferContext, &sBlitInfo);

		if (srvErr != PVRSRV_OK)
		{
			if (srvErr == PVRSRV_ERROR_TIMEOUT)
			{
				return PVR2DERROR_DEVICE_UNAVAILABLE;
			}
			else
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentBlt: Couldn't do the blit (0x%x)", srvErr));
				return PVR2DERROR_GENERIC_ERROR;
			}
		}

		#ifdef SYNC_DEBUG
			// Debug with synchronous blts
			PVR2DQueryBlitsComplete(hContext, &psContext->sPrimaryBuffer, 1);
		#endif
	}
	else
	{
		for(j = 0; j<psContext->ui32NumActiveClipBlocks; j++)
		{
			ui32BltSize = 0;

			/* knit misc. state, clip and blit blocks together */
			for(i = 0; i <  SIZEOF_PRESENT_BLT_MISC_STATE_BLOCK; i++)
			{
				psContext->aui32PresentBltData[ui32BltSize++] = psContext->aui32PresentBltMiscStateBlock[i];
			}

			for(i = 0; i < SIZEOF_CLIPBLOCK; i++)
			{
				psContext->aui32PresentBltData[ui32BltSize++] = psContext->psBltClipBlock[j].aui32Data[i];
			}

			for(i = 0; i <  SIZEOF_PRESENT_BLT_BLOCK; i++)
			{
				psContext->aui32PresentBltData[ui32BltSize++] = psContext->aui32PresentBltBlock[i];
			}

			PVR_ASSERT(ui32BltSize <= SIZEOF_PRESENT_BLT_DATA);

			/* Setup the rest of the command */

			/* 2D Core commands are managed by the uKernel and go via the TQ API */
			sBlitInfo.Details.s2DBlit.ui32CtrlSizeInDwords = ui32BltSize;
			sBlitInfo.Details.s2DBlit.pui32CtrlStream = psContext->aui32PresentBltData;
	
			/* Transfer queue (2D Core PresentBlt) */
			srvErr = SGXQueueTransfer(psContext->hTransferContext, &sBlitInfo);

			if (srvErr != PVRSRV_OK)
			{
				if (srvErr == PVRSRV_ERROR_TIMEOUT)
				{
					return PVR2DERROR_DEVICE_UNAVAILABLE;
				}
				else
				{
					PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentBlt: Couldn't do the blit (0x%x)", srvErr));
					return PVR2DERROR_GENERIC_ERROR;
				}
			}

			#ifdef SYNC_DEBUG
				// Debug with synchronous blts
				PVR2DQueryBlitsComplete(hContext, &psContext->sPrimaryBuffer, 1);
			#endif
		}
	}

#if defined(PDUMP)
	{
		PVR2DRECT rcDstRect;

		// Get dest rectangle
		rcDstRect.top = (PVR2D_LONG)(psContext->aui32PresentBltBlock[PRESENT_BLT_START] & EURASIA2D_DST_YSTART_MASK);

		rcDstRect.left = (PVR2D_LONG)((psContext->aui32PresentBltBlock[PRESENT_BLT_START]
			 & EURASIA2D_DST_XSTART_MASK)>>EURASIA2D_DST_XSTART_SHIFT);

		rcDstRect.bottom = rcDstRect.top
			+ (PVR2D_LONG)(psContext->aui32PresentBltBlock[PRESENT_BLT_SIZE] & EURASIA2D_DST_YSIZE_MASK);

		rcDstRect.right = rcDstRect.left
			+ (PVR2D_LONG)((psContext->aui32PresentBltBlock[PRESENT_BLT_SIZE] & EURASIA2D_DST_XSIZE_MASK)>>EURASIA2D_DST_XSIZE_SHIFT);

		// Capture present blt output after the blt
		PdumpPresentBltOutputSurface(psContext, pMemInfo, &rcDstRect);
	}
#endif//#if defined(PDUMP)

	return PVR2D_OK;

#else /* SGX_FEATURE_2D_HARDWARE */
	PVR_UNREFERENCED_PARAMETER(hContext);
	PVR_UNREFERENCED_PARAMETER(pMemInfo);
	PVR_UNREFERENCED_PARAMETER(lRenderID);

	PVR2D_DPF((PVR_DBG_ERROR, "PVR2DPresentBlt: Blits not supported"));

	return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;

#endif /* SGX_FEATURE_2D_HARDWARE */
#endif /* PVR2D_ALT_2DHW */

} // PVR2DPresentBlt


#if defined(SGX_FEATURE_2D_HARDWARE) && !defined(PVR2D_ALT_2DHW)
void IMG_INTERNAL InitPresentBltData(PVR2DCONTEXT *psContext)
{
	IMG_UINT32 ui32Format;

	switch(psContext->sPrimary.pixelformat)
	{
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
			ui32Format = EURASIA2D_DST_8888ARGB;
			break;
		case PVRSRV_PIXEL_FORMAT_RGB565:
			ui32Format = EURASIA2D_DST_565RGB;
			break;
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
			ui32Format = EURASIA2D_DST_1555ARGB;
			break;
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
			ui32Format = EURASIA2D_DST_4444ARGB;
			break;
		default:
			return;
	}

	/* Initialise blit to zero sized blit from primary to primary */
	psContext->aui32PresentBltMiscStateBlock[PRESENT_BLT_MISC_DST_HEADER] = EURASIA2D_DST_SURF_BH | ui32Format | psContext->sPrimary.sDims.ui32ByteStride;

	psContext->aui32PresentBltMiscStateBlock[PRESENT_BLT_MISC_DST_ADDR] =
		(((psContext->sPrimaryBuffer.ui32DevAddr - psContext->s2DHeapBase.uiAddr)
		>>	EURASIA2D_DST_ADDR_ALIGNSHIFT)
		<<	EURASIA2D_DST_ADDR_SHIFT)
		&	EURASIA2D_DST_ADDR_MASK;

	psContext->aui32PresentBltMiscStateBlock[PRESENT_BLT_MISC_SRC_HEADER] = EURASIA2D_SRC_SURF_BH |
										ui32Format | psContext->sPrimary.sDims.ui32ByteStride;

	psContext->aui32PresentBltMiscStateBlock[PRESENT_BLT_MISC_SRC_ADDR] = (((psContext->sPrimaryBuffer.ui32DevAddr - psContext->s2DHeapBase.uiAddr)
										>>	EURASIA2D_SRC_ADDR_ALIGNSHIFT)
										<<	EURASIA2D_SRC_ADDR_SHIFT)
										&	EURASIA2D_SRC_ADDR_MASK;

	psContext->aui32PresentBltMiscStateBlock[PRESENT_BLT_MISC_SRC_OFF] = EURASIA2D_SRC_OFF_BH |
		(0 << EURASIA2D_SRCOFF_XSTART_SHIFT) | (0 << EURASIA2D_SRCOFF_YSTART_SHIFT);

	psContext->aui32PresentBltBlock[PRESENT_BLT_HEADER] = EURASIA2D_BLIT_BH
									   | EURASIA2D_USE_PAT
									   | EURASIA2D_ROP3_SRCCOPY;

	psContext->aui32PresentBltBlock[PRESENT_BLT_START] = (0 << EURASIA2D_DST_XSTART_SHIFT)
									   | (0 << EURASIA2D_DST_YSTART_SHIFT);

	psContext->aui32PresentBltBlock[PRESENT_BLT_SIZE] = (0 << EURASIA2D_DST_XSIZE_SHIFT)
									   | (0 << EURASIA2D_DST_YSIZE_SHIFT);
} // InitPresentBltData

#endif /* SGX_FEATURE_2D_HARDWARE && !PVR2D_ALT_2DHW */


#if defined(PVR2D_ALT_2DHW) || defined(PVR2D_VALIDATE_INPUT_PARAMS)
// max/min/intersect
static PVR2D_BOOL bIntersect(PVR2DRECT *prclIn, PVR2DRECT *prclClip, PVR2DRECT *prclOut)
{
	prclOut->left  = PVR2D_MAX(prclIn->left, prclClip->left);
	prclOut->right = PVR2D_MIN(prclIn->right, prclClip->right);

	if (prclOut->left < prclOut->right)
	{
		prclOut->top = PVR2D_MAX(prclIn->top, prclClip->top);
		prclOut->bottom = PVR2D_MIN(prclIn->bottom, prclClip->bottom);

		if (prclOut->top < prclOut->bottom)
		{
			return PVR2D_TRUE;
		}
	}
	return PVR2D_FALSE;
}
#endif//#if defined(PVR2D_ALT_2DHW) || defined(PVR2D_VALIDATE_INPUT_PARAMS)

#if defined(PVR2D_ALT_2DHW)

// pClipRect - Only areas within the clip rect can be written to.
static PVR2DERROR AltPresentBlt (const PVR2DCONTEXTHANDLE hContext, const PPVR2DMEMINFO pSrc, const PVR2DRECT* pClipRect)
{
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	SGX_QUEUETRANSFER sBlitInfo;
	PVRSRV_ERROR eResult;
	PVR2DERROR ePVR2DResult;
	PPVR2DMEMINFO pDst = &psContext->sPrimaryBuffer;
	const PVRSRV_CLIENT_MEM_INFO *pSrcMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pSrc->hPrivateData;
	const PVRSRV_CLIENT_MEM_INFO *pDstMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pDst->hPrivateData;
	PVR2DRECT rcSrcRect; // internal src offset needed for clip
	PVR2DRECT rcDstRect;
	PVR2DRECT rcClipDeltas;
	PVR2D_LONG SrcWidth;
	PVR2D_LONG SrcHeight;
	PVR2D_LONG BltWidth;
	PVR2D_LONG BltHeight;

	/* Validate the TQ context (creates one if necessary) */
	ePVR2DResult = ValidateTransferContext(psContext);
	if (ePVR2DResult != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR,"AltPresentBlt: ValidateTransferContext failed\n"));
		return ePVR2DResult;
	}

	PVRSRVMemSet(&sBlitInfo,0,sizeof(sBlitInfo));

	if (psContext->ulPresentBltDstHeight > 0)
	{
		rcDstRect.top    = psContext->lPresentBltDstYPos;
		rcDstRect.bottom = rcDstRect.top + (PVR2D_LONG)psContext->ulPresentBltDstHeight;
	}
	else
	{
		// Full screen height
		rcDstRect.top    = 0L;
		rcDstRect.bottom = (PVR2D_LONG)psContext->sPrimary.sDims.ui32Height;
	}

	if (psContext->ulPresentBltDstWidth > 0)
	{
		rcDstRect.left  = psContext->lPresentBltDstXPos;
		rcDstRect.right = rcDstRect.left + (PVR2D_LONG)psContext->ulPresentBltDstWidth;
	}
	else
	{
		// Full screen width
		rcDstRect.left  = 0L;
		rcDstRect.right = (PVR2D_LONG)psContext->sPrimary.sDims.ui32Width;
	}

	BltWidth  = rcDstRect.right - rcDstRect.left;
	BltHeight = rcDstRect.bottom - rcDstRect.top;
	SrcWidth  = BltWidth;
	SrcHeight = BltHeight;

	rcSrcRect.top  = 0L;
	rcSrcRect.left = 0L;
	rcSrcRect.bottom = BltHeight;
	rcSrcRect.right  = BltWidth;

	if (pClipRect)
	{
		// How many pixels are being clipped off each side
		rcClipDeltas.top = pClipRect->top - rcDstRect.top;
		rcClipDeltas.left =  pClipRect->left - rcDstRect.left;
		rcClipDeltas.bottom =  rcDstRect.bottom - pClipRect->bottom;
		rcClipDeltas.right =  rcDstRect.right - pClipRect->right;

		// Apply deltas to source and destination
		rcDstRect.top += rcClipDeltas.top;
		rcDstRect.left += rcClipDeltas.left;
		rcDstRect.bottom -= rcClipDeltas.bottom;
		rcDstRect.right -= rcClipDeltas.right;
		rcSrcRect.top += rcClipDeltas.top;
		rcSrcRect.left += rcClipDeltas.left;
		rcSrcRect.bottom -= rcClipDeltas.bottom;
		rcSrcRect.right -= rcClipDeltas.right;
	}

#if defined(PDUMP)
	sBlitInfo.bPDumpContinuous = IMG_FALSE;
#endif
	// Source copy
	sBlitInfo.eType = SGXTQ_BLIT;
	sBlitInfo.ui32NumSources = 1;
	sBlitInfo.asSources[0].eFormat = psContext->sPrimary.pixelformat;
	sBlitInfo.asSources[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
	sBlitInfo.asSources[0].i32StrideInBytes = psContext->lPresentBltSrcStride;
	sBlitInfo.asSources[0].psSyncInfo = pSrcMemInfo->psClientSyncInfo;

	SGX_TQSURFACE_SET_ADDR(sBlitInfo.asSources[0], pSrcMemInfo, 0);

	// No scaling
	sBlitInfo.asSources[0].ui32Height = (IMG_UINT32)SrcHeight;
	sBlitInfo.asSources[0].ui32Width  = (IMG_UINT32)SrcWidth;

	SGX_QUEUETRANSFER_NUM_SRC_RECTS(sBlitInfo) = 1;
	SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).x0 = rcSrcRect.left;
	SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).x1 = rcSrcRect.right;
	SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).y0 = rcSrcRect.top;
	SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).y1 = rcSrcRect.bottom;

	/* sPrimary destination */
	sBlitInfo.ui32NumDest = 1;
	sBlitInfo.asDests[0].eFormat =  psContext->sPrimary.pixelformat;
	sBlitInfo.asDests[0].eMemLayout =  SGXTQ_MEMLAYOUT_STRIDE /* SGXTQ_MEMLAYOUT_OUT_LINEAR*/;
	sBlitInfo.asDests[0].i32StrideInBytes = (IMG_INT32)psContext->sPrimary.sDims.ui32ByteStride;
	sBlitInfo.asDests[0].psSyncInfo = pDstMemInfo->psClientSyncInfo;

	SGX_TQSURFACE_SET_ADDR(sBlitInfo.asDests[0], pDstMemInfo, 0);

	sBlitInfo.asDests[0].ui32ChunkStride = 0;
	sBlitInfo.asDests[0].ui32Height = psContext->sPrimary.sDims.ui32Height;
	sBlitInfo.asDests[0].ui32Width = psContext->sPrimary.sDims.ui32Width;

	SGX_QUEUETRANSFER_NUM_DST_RECTS(sBlitInfo) = 1;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).x0 = rcDstRect.left;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).x1 = rcDstRect.right;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).y0 = rcDstRect.top;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).y1 = rcDstRect.bottom;

#if defined(PDUMP) && defined(CAPTURE_PRESENTBLT_INPUT)
	PdumpPresentBltInputSurface(psContext, pSrc, &rcSrcRect);
	PVRSRVPDumpComment(psContext->psServices, "PVR2DPresentBlt:SGXQueueTransfer", IMG_FALSE);
#endif

	// Transfer queue blt (3D Core present blt)
	eResult = SGXQueueTransfer(psContext->hTransferContext, &sBlitInfo);

	if (eResult != PVRSRV_OK)
	{
		if (eResult == PVRSRV_ERROR_TIMEOUT)
		{
			return PVR2DERROR_DEVICE_UNAVAILABLE;
		}
		else
		{
			PVR2D_DPF((PVR_DBG_ERROR, "AltPresentBlt: Couldn't do the blit (0x%x)", eResult));
			return PVR2DERROR_GENERIC_ERROR;
		}
	}
	
#ifdef SYNC_DEBUG
	// Debug with synchronous blts
	PVR2DQueryBlitsComplete(hContext, pDst, 1);
#endif

#if defined(PDUMP) // Capture the presentblt output surface
	PdumpPresentBltOutputSurface(psContext, pDst, &rcDstRect);
#endif

	return PVR2D_OK;

} // AltPresentBlt


/*****************************************************************************
 @Function	AltBltClipped

 @Input		hContext : PVR2D context handle

 @Input		pBltInfo : PVR2D blit info structure

 @Input		ulNumClipRects : number of clip rectangles

 @Input		pClipRects : array of clip rectangles

 @Return	error : PVR2D error code

 @Description : submit clipped blit to TQ when 2D hardware is not available
******************************************************************************/

static PVR2DERROR AltBltClipped (	PVR2DCONTEXTHANDLE hContext,
									PVR2DBLTINFO *pBltInfo,
									PVR2D_ULONG ulNumClipRects,
									PVR2DRECT *pClipRect)
{
	// Use alternative option for 2d if needed
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;
	PVRSRV_CLIENT_MEM_INFO *pSrcMemInfo;
	PVRSRV_CLIENT_MEM_INFO *pDstMemInfo;
	SGX_QUEUETRANSFER sBlitInfo;
	PVR2DERROR ePVR2DResult;
	PVRSRV_ERROR eResult;
	IMG_UINT32 ui32BltFlags;
	IMG_BOOL bExtendedDstFormat;
	IMG_BOOL bExtendedSrcFormat=IMG_FALSE;
	IMG_BOOL bValidMask;
	IMG_BOOL bValidPattern;
	PVR2DRECT rcSrcRect; // internal src offset needed for clip
	PVR2DRECT rcDstRect;
	PVR2DRECT rcClipDeltas;
	PVR2DRECT rcDestClipped;
	PVR2D_ULONG DstFormat,SrcFormat=0;
	SGXTQ_MEMLAYOUT eMemLayoutDst;
	
	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}
	if (!pBltInfo || !pBltInfo->pDstMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: invalid param"));
		return PVR2DERROR_INVALID_PARAMETER;
	}
	if (ulNumClipRects)
	{
		if (!pClipRect || (ulNumClipRects > 1))
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Invalid clip rect params"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
	}
	if (pBltInfo->BlitFlags & PVR2D_BLIT_FULLY_SPECIFIED_ALPHA_ENABLE)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: PVR2D_BLIT_FULLY_SPECIFIED_ALPHA_ENABLE is a 2D Core feature"));
		return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
	}
	
#if defined(PVR2D_VALIDATE_INPUT_PARAMS)
	{
		PVR2DERROR pvr2dError;

		// Deal with out of bounds and badly behaved rectangles
		pvr2dError = ValidateParams (pBltInfo, ulNumClipRects, pClipRect);
		if (pvr2dError != PVR2D_OK)
		{
			return PVR2DERROR_INVALID_PARAMETER;
		}
	}
#endif

	/* Validate the TQ context (creates one if necessary) */
	ePVR2DResult = ValidateTransferContext(psContext);
	if (ePVR2DResult != PVR2D_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR,"AltBltClipped: ValidateTransferContext failed\n"));
		return ePVR2DResult;
	}

	PVRSRVMemSet(&sBlitInfo,0,sizeof(sBlitInfo));
	
	bExtendedDstFormat = (pBltInfo->DstFormat&PVR2D_FORMAT_PVRSRV)?IMG_TRUE:IMG_FALSE;
	
	DstFormat = (PVR2D_ULONG)pBltInfo->DstFormat & PVR2D_FORMAT_MASK;

	if (pBltInfo->DstFormat & PVR2D_FORMAT_LAYOUT_TWIDDLED)
	{
		eMemLayoutDst = SGXTQ_MEMLAYOUT_OUT_TWIDDLED;
	}
	else if (pBltInfo->DstFormat & PVR2D_FORMAT_LAYOUT_TILED)
	{
		eMemLayoutDst = SGXTQ_MEMLAYOUT_OUT_TILED;
	}
	else
	{
		eMemLayoutDst = SGXTQ_MEMLAYOUT_STRIDE;
	}

	// Validate destination format
	if (!bExtendedDstFormat)
	{
		if ( (DstFormat>(PVR2D_ULONG)PVR2D_NO_OF_FORMATS) || !bValidOutputFormat[DstFormat])
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Illegal dest pixel format 0x%lx", DstFormat));
			return PVR2DERROR_INVALID_PARAMETER;
		}
	}

	// Default blt type
#if defined(PDUMP)
	sBlitInfo.bPDumpContinuous = IMG_FALSE;
#endif
	
	// Use 2D core or PTLA or 3D core or software to do the blt as required depending on what is available.
	if (pBltInfo->BlitFlags & (PVR2D_BLIT_PATH_2DCORE|PVR2D_BLIT_PATH_3DCORE|PVR2D_BLIT_PATH_SWBLT))
	{
		// Caller is controlling the path
		sBlitInfo.ui32Flags = SGX_TRANSFER_DISPATCH_DISABLE_PTLA | SGX_TRANSFER_DISPATCH_DISABLE_3D | SGX_TRANSFER_DISPATCH_DISABLE_SW;

		if (pBltInfo->BlitFlags & PVR2D_BLIT_PATH_2DCORE)
		{
			// Enable 2D/PTLA option
			sBlitInfo.ui32Flags &= ~SGX_TRANSFER_DISPATCH_DISABLE_PTLA;
		}
		if (pBltInfo->BlitFlags & PVR2D_BLIT_PATH_3DCORE)
		{
			// Enable 3D option
			sBlitInfo.ui32Flags &= ~SGX_TRANSFER_DISPATCH_DISABLE_3D;
		}
		if (pBltInfo->BlitFlags & PVR2D_BLIT_PATH_SWBLT)
		{
			// Enable software blt option
			sBlitInfo.ui32Flags &= ~SGX_TRANSFER_DISPATCH_DISABLE_SW;
		}
	}
	else
	{
		// Specify the default path (use lowest power core if it's capable else fail)
#if defined(SGX_FEATURE_PTLA)
		sBlitInfo.ui32Flags |= SGX_TRANSFER_DISPATCH_DISABLE_SW | SGX_TRANSFER_DISPATCH_DISABLE_3D;
#else
		sBlitInfo.ui32Flags |= SGX_TRANSFER_DISPATCH_DISABLE_SW;
#endif
	}

	// Via TQ API
	sBlitInfo.eType = SGXTQ_BLIT;

	// Source and dest rects
	rcDstRect.top = pBltInfo->DstY;
	rcDstRect.left = pBltInfo->DstX;
	rcDstRect.bottom = pBltInfo->DstY + pBltInfo->DSizeY;
	rcDstRect.right = pBltInfo->DstX + pBltInfo->DSizeX;

	rcSrcRect.top = pBltInfo->SrcY;
	rcSrcRect.left = pBltInfo->SrcX;
	rcSrcRect.bottom = pBltInfo->SrcY + pBltInfo->SizeY;
	rcSrcRect.right = pBltInfo->SrcX + pBltInfo->SizeX;

	// A nonzero pMaskMemInfo field indicates a rop 4 with valid mask surface
	bValidMask = (IMG_BOOL)(pBltInfo->pMaskMemInfo != 0);

	// The PVR2D_BLIT_PAT_SURFACE_ENABLE flag indicates a valid pattern surface
	bValidPattern = (IMG_BOOL)((pBltInfo->BlitFlags & PVR2D_BLIT_PAT_SURFACE_ENABLE) != 0);

	// Find what surfaces are used in the rop code
	ui32BltFlags = GetSurfs (pBltInfo->CopyCode, bValidPattern, bValidMask);

	// Legacy mask support
	if (bValidMask)
	{
		IMG_BYTE byRop3b = (IMG_BYTE)(pBltInfo->CopyCode>>8);

		/* Legacy support : if mask enabled and byRop3b=0 then add the transparency rop 0xAA */
		if (byRop3b == 0)
		{
			pBltInfo->CopyCode = ((pBltInfo->CopyCode & 0xFFUL) << 8) | 0xAA; // 0xAA is dest (nop)
			ui32BltFlags = GetSurfs( pBltInfo->CopyCode, bValidPattern, bValidMask);
		}
	}
	else // else there's no mask so only 8 bits of the rop code is needed.
	// so do rop3 processing here :
	if (ui32BltFlags == BW)
	{
		switch ((IMG_BYTE)pBltInfo->CopyCode)
		{
			case 0x00:
			{
				// Change blackness to a solid fill rop
				pBltInfo->Colour = 0;
				break;
			}

			case 0xFF:
			{
				// Change whiteness to a solid fill rop
				pBltInfo->Colour = 0xFFFFFFFF;
				break;
			}
		}
	}

	// Limit the scope to common rops for now
	if (ui32BltFlags & BLT_FLAG_CUSTOMROP)
	{
		// Rop is supported as a custom rop
		if( ui32BltFlags & BLT_FLAG_COLOURFILL)
		{
			sBlitInfo.Details.sFill.byCustomRop3 = (IMG_BYTE)pBltInfo->CopyCode; // Rop code support
		}
		else
		{
			sBlitInfo.Details.sBlit.byCustomRop3 = (IMG_BYTE)pBltInfo->CopyCode; // Rop code support
		}
	}
	else if ((ui32BltFlags & BLT_FLAG_COMMONBLT) == 0)
	{
		// If it's not a common rop then fail
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Rop code not supported : %04lX", pBltInfo->CopyCode));
		return PVR2DERROR_NOT_YET_IMPLEMENTED;
	}

	// Rop4 / Masks not supported
	if (ui32BltFlags & BLT_FLAG_MASK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: Masks not supported"));
		return PVR2DERROR_NOT_YET_IMPLEMENTED;
	}

	// Support for one clip rect done by modifying src and dst rect
	if (ulNumClipRects && pClipRect)
	{
		// Find intersection of clip rect and dest rect
		if (bIntersect(&rcDstRect, pClipRect, &rcDestClipped))
		{
			// How many pixels are being clipped off each side
			rcClipDeltas.top = rcDestClipped.top - rcDstRect.top;
			rcClipDeltas.left =  rcDestClipped.left - rcDstRect.left;
			rcClipDeltas.bottom =  rcDstRect.bottom - rcDestClipped.bottom;
			rcClipDeltas.right =  rcDstRect.right - rcDestClipped.right;

			// Apply deltas to source rect applying dest -> source rotation
			if ((pBltInfo->BlitFlags & (PVR2D_BLIT_ROT_90|PVR2D_BLIT_ROT_180|PVR2D_BLIT_ROT_270)) != 0)
			{
				if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_90)
				{
					rcSrcRect.top += rcClipDeltas.right;
					rcSrcRect.left += rcClipDeltas.top;
					rcSrcRect.bottom -= rcClipDeltas.left;
					rcSrcRect.right -= rcClipDeltas.bottom;
				}
				else if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_270)
				{
					rcSrcRect.top += rcClipDeltas.left;
					rcSrcRect.left += rcClipDeltas.bottom;
					rcSrcRect.bottom -= rcClipDeltas.right;
					rcSrcRect.right -= rcClipDeltas.top;
				}
				else
				{
					// 180
					rcSrcRect.top += rcClipDeltas.bottom;
					rcSrcRect.left += rcClipDeltas.right;
					rcSrcRect.bottom -= rcClipDeltas.top;
					rcSrcRect.right -= rcClipDeltas.left;
				}
			}
			else
			{
				// No rotation
				rcSrcRect.top += rcClipDeltas.top;
				rcSrcRect.left += rcClipDeltas.left;
				rcSrcRect.bottom -= rcClipDeltas.bottom;
				rcSrcRect.right -= rcClipDeltas.right;
			}

			// Use clipped dest rect
			rcDstRect = rcDestClipped;
		}
		else
		{
			// No intersection therefore no pixels written
			return PVR2D_OK;
		}
	}

	// Get Dest client mem
	pDstMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBltInfo->pDstMemInfo->hPrivateData;

	if (!pDstMemInfo)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: null pDstMemInfo"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if (ui32BltFlags & (BLT_FLAG_SOURCE|BLT_FLAG_PATTERN)) // Source copy and pattern blts
	{
		if (!pBltInfo->pSrcMemInfo)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: rop needs a source"));
			return PVR2DERROR_INVALID_PARAMETER;
		}

		pSrcMemInfo = (PVRSRV_CLIENT_MEM_INFO*)pBltInfo->pSrcMemInfo->hPrivateData;

		if (!pSrcMemInfo)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: null pSrcMemInfo"));
			return PVR2DERROR_INVALID_PARAMETER;
		}

		// Validate source format
		bExtendedSrcFormat = (pBltInfo->SrcFormat & PVR2D_FORMAT_PVRSRV) ? IMG_TRUE : IMG_FALSE;

		SrcFormat = pBltInfo->SrcFormat & PVR2D_FORMAT_MASK;

		if (!bExtendedSrcFormat && (SrcFormat > PVR2D_NO_OF_FORMATS))
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: source format not supported"));
			return PVR2DERROR_INVALID_PARAMETER;
		}

		// Source copy
		sBlitInfo.ui32NumSources = 1;
		sBlitInfo.asSources[0].eFormat = GetPvrSrvPixelFormat(SrcFormat);

		if (pBltInfo->SrcFormat & PVR2D_FORMAT_LAYOUT_TWIDDLED)
		{
			sBlitInfo.asSources[0].eMemLayout = SGXTQ_MEMLAYOUT_OUT_TWIDDLED;
		}
		else if (pBltInfo->SrcFormat & PVR2D_FORMAT_LAYOUT_TILED)
		{
			sBlitInfo.asSources[0].eMemLayout = SGXTQ_MEMLAYOUT_OUT_TILED;
		}
		else
		{
			sBlitInfo.asSources[0].eMemLayout = SGXTQ_MEMLAYOUT_STRIDE;
		}

		sBlitInfo.asSources[0].i32StrideInBytes = pBltInfo->SrcStride;
		sBlitInfo.asSources[0].psSyncInfo = pSrcMemInfo->psClientSyncInfo;

		if((pBltInfo->BlitFlags & PVR2D_BLIT_NO_SRC_SYNC_INFO) != 0)
		{
			/*
				The PVR2D_BLIT_NO_SRC_SYNC_INFO flag prevents SGX from waiting for pending read operations to
				complete before issuing the blit, and also prevents any source SyncInfo members being updated after the blit.
			*/
			sBlitInfo.asSources[0].psSyncInfo = IMG_NULL;
		}

		if (pBltInfo->SrcOffset >= pBltInfo->pSrcMemInfo->ui32MemSize)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: source memory offset specified is larger than the source memory size"));
			return PVR2DERROR_INVALID_PARAMETER;
		}

		SGX_TQSURFACE_SET_ADDR(sBlitInfo.asSources[0], pSrcMemInfo, pBltInfo->SrcOffset);

		if (pBltInfo->uSrcChromaPlane1 >= pBltInfo->pSrcMemInfo->ui32MemSize)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: uSrcChromaPlane1 offset is larger than the source memory size"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
		sBlitInfo.asSources[0].ui32ChromaPlaneOffset[0] = pBltInfo->uSrcChromaPlane1;

		if (pBltInfo->uSrcChromaPlane2 >= pBltInfo->pSrcMemInfo->ui32MemSize)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: uSrcChromaPlane2 offset is larger than the source memory size"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
		sBlitInfo.asSources[0].ui32ChromaPlaneOffset[1] = pBltInfo->uSrcChromaPlane2;
		sBlitInfo.asSources[0].ui32ChunkStride = 0;

		// Source surface size
		if (pBltInfo->SrcSurfHeight)
		{
			sBlitInfo.asSources[0].ui32Height = pBltInfo->SrcSurfHeight;
		}
		else
		{
			// Legacy support
			// Old PVR2D apps wont set the surface sizes, but they also won't be using new features such as rotation,
			// so we can set the size of the surface to be the extent used for the blt.
			sBlitInfo.asSources[0].ui32Height = (IMG_UINT32)(pBltInfo->SrcY + pBltInfo->SizeY);
		}
		if (pBltInfo->SrcSurfWidth)
		{
			sBlitInfo.asSources[0].ui32Width = pBltInfo->SrcSurfWidth;
		}
		else
		{
			// Legacy support
			sBlitInfo.asSources[0].ui32Width = (IMG_UINT32)(pBltInfo->SrcX + pBltInfo->SizeX);
		}

		SGX_QUEUETRANSFER_NUM_SRC_RECTS(sBlitInfo) = 1;
		SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).x0 = rcSrcRect.left;
		SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).x1 = rcSrcRect.right;
		SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).y0 = rcSrcRect.top;
		SGX_QUEUETRANSFER_SRC_RECT(sBlitInfo, 0).y1 = rcSrcRect.bottom;

		// Palette support
		if (!bExtendedSrcFormat)
		{
			if (ui32PaletteSize[SrcFormat])
			{
				// Support for palletised source copy blt
				if (pBltInfo->pPalMemInfo && (pBltInfo->BlitFlags==0) && (pBltInfo->CopyCode==PVR2DROPcopy))
				{
					sBlitInfo.eType = SGXTQ_COLOURLUT_BLIT;
					sBlitInfo.Details.sColourLUT.ui32KeySizeInBits = ui32PaletteSizeInBits[SrcFormat];
					sBlitInfo.Details.sColourLUT.eLUTPxFmt = PVRSRV_PIXEL_FORMAT_ARGB8888;
					sBlitInfo.Details.sColourLUT.sLUTDevVAddr.uiAddr = pBltInfo->pPalMemInfo->ui32DevAddr + pBltInfo->PalOffset;
				}
				else
				{
					return PVR2DERROR_INVALID_PARAMETER;
				}
			}
			// Pattern support
			else if (ui32BltFlags & BLT_FLAG_PATTERN)
			{
				sBlitInfo.Details.sBlit.bEnablePattern = IMG_TRUE;
			}
		}

		if (pBltInfo->BlitFlags & PVR2D_BLIT_ISSUE_STATUS_UPDATES)
		{
			/*
				This will cause SGX to write the value of the source SyncInfo’s ReadOpsPending value
				to it’s ReadOpsComplete member on completion of the blit. The PVR2D_BLIT_ISSUE_STATUS_UPDATES
				flag is to be used in conjunction with PVR2D_BLIT_NO_SRC_SYNC_INFO only.
			*/
			sBlitInfo.ui32NumStatusValues			 = 1;
			sBlitInfo.asMemUpdates[0].ui32UpdateVal  = pSrcMemInfo->psClientSyncInfo->psSyncData->ui32ReadOpsPending;
			sBlitInfo.asMemUpdates[0].ui32UpdateAddr = pSrcMemInfo->psClientSyncInfo->sReadOpsCompleteDevVAddr.uiAddr;
		}
	}
	else if (ui32BltFlags & BLT_FLAG_COLOURFILL) // Solid fill blts
	{
		// Solid colour fill
		sBlitInfo.eType = SGXTQ_FILL;
		sBlitInfo.ui32NumSources = 0;
		sBlitInfo.Details.sFill.ui32Colour = pBltInfo->Colour;
	}
	else if (ui32BltFlags == DX)
	{
		// Allow this special case for Dst-only blts
		if (pBltInfo->CopyCode == 0x55)
		{
			// Rop 0x55 is DEST INVERT
			// This can be done with SGXTQ_FILL with pattern fill colour=0 and the PAT OR NOT DEST rop
			sBlitInfo.eType = SGXTQ_FILL;
			sBlitInfo.ui32NumSources = 0;
			sBlitInfo.Details.sFill.ui32Colour = 0;
			sBlitInfo.Details.sFill.byCustomRop3 = 0xF5; /* The 0xF5 case with zero fill does the same thing */
		}
		// Allow all the other dest fill rops to continue through here, ie./ colour fills with a destination rop.
	}
	else
	{
		// Disallow all other rop codes for the time being
		return PVR2DERROR_NOT_YET_IMPLEMENTED;
	}

	// SGXTQ_BLIT Alpha and Colour Key for source copy
	if (sBlitInfo.eType == SGXTQ_BLIT)
	{
		if (((pBltInfo->BlitFlags & PVR2D_BLIT_GLOBAL_ALPHA_ENABLE) != 0) &&
		    ((pBltInfo->BlitFlags & PVR2D_BLIT_PERPIXEL_ALPHABLEND_ENABLE) != 0))
		{
			// Premultiplied Source Alpha with Global Alpha
			sBlitInfo.Details.sBlit.byGlobalAlpha = pBltInfo->GlobalAlphaValue;
			sBlitInfo.Details.sBlit.eAlpha = SGXTQ_ALPHA_PREMUL_SOURCE_WITH_GLOBAL;
		}
		else if (pBltInfo->BlitFlags & PVR2D_BLIT_GLOBAL_ALPHA_ENABLE)
		{
			// Global Alpha
			sBlitInfo.Details.sBlit.byGlobalAlpha = pBltInfo->GlobalAlphaValue;
			sBlitInfo.Details.sBlit.eAlpha = SGXTQ_ALPHA_GLOBAL;
		}
		else if (pBltInfo->BlitFlags & PVR2D_BLIT_PERPIXEL_ALPHABLEND_ENABLE)
		{
			if (pBltInfo->AlphaBlendingFunc == PVR2D_ALPHA_OP_SRC_DSTINV)
			{
				// Non premultiplied Source Alpha
				sBlitInfo.Details.sBlit.eAlpha = SGXTQ_ALPHA_SOURCE;
			}
			else if (pBltInfo->AlphaBlendingFunc == PVR2D_ALPHA_OP_SRCP_DSTINV)
			{
				// Premultiplied Source Alpha
				sBlitInfo.Details.sBlit.eAlpha = SGXTQ_ALPHA_PREMUL_SOURCE;
			}
		}

		// Colour key via transfer queue
		if (((pBltInfo->BlitFlags & PVR2D_BLIT_CK_ENABLE) != 0) && !bExtendedDstFormat && !bExtendedSrcFormat)
		{
			PVR2DFORMAT Format;

			if (((ui32BltFlags & BLT_FLAG_SOURCE) != 0) &&
			    ((pBltInfo->BlitFlags & PVR2D_BLIT_COLKEY_DEST) == 0))
			{
				// Key colour is on the source surface
				sBlitInfo.Details.sBlit.eColourKey = SGXTQ_COLOURKEY_SOURCE;
				Format = SrcFormat;
			}
			else
			{
				// Key colour is on the dest surface
				sBlitInfo.Details.sBlit.eColourKey = SGXTQ_COLOURKEY_DEST;
				Format = DstFormat;
			}
			sBlitInfo.Details.sBlit.ui32ColourKey = pBltInfo->ColourKey;

			if (((pBltInfo->BlitFlags & PVR2D_BLIT_COLKEY_MASKED) != 0) && pBltInfo->ColourKeyMask)
			{
				sBlitInfo.Details.sBlit.ui32ColourKeyMask = pBltInfo->ColourKeyMask; // Caller's mask
			}
			else
			{
				sBlitInfo.Details.sBlit.ui32ColourKeyMask = uiColorKeyMask[Format]; // Default mask for that format
			}
		}

	}// SGXTQ_BLIT Alpha and colour key

	// Transfer Queue dest surface
	sBlitInfo.ui32NumDest = 1;
	sBlitInfo.asDests[0].eFormat =  GetPvrSrvPixelFormat(DstFormat);
	sBlitInfo.asDests[0].eMemLayout =  eMemLayoutDst;
	sBlitInfo.asDests[0].i32StrideInBytes = pBltInfo->DstStride;
	sBlitInfo.asDests[0].psSyncInfo = pDstMemInfo->psClientSyncInfo;

	if (pBltInfo->DstOffset >= pBltInfo->pDstMemInfo->ui32MemSize)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: destination memory offset specified is larger than the destination memory size"));
		return PVR2DERROR_INVALID_PARAMETER;
	}
	SGX_TQSURFACE_SET_ADDR(sBlitInfo.asDests[0], pDstMemInfo, pBltInfo->DstOffset);

	if (pBltInfo->uDstChromaPlane1 >= pBltInfo->pDstMemInfo->ui32MemSize)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: uDstChromaPlane1 offset is larger than the memory size"));
		return PVR2DERROR_INVALID_PARAMETER;
	}
	sBlitInfo.asDests[0].ui32ChromaPlaneOffset[0] = pBltInfo->uDstChromaPlane1;

	if (pBltInfo->uDstChromaPlane2 >= pBltInfo->pDstMemInfo->ui32MemSize)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: uDstChromaPlane2 offset is larger than the memory size"));
		return PVR2DERROR_INVALID_PARAMETER;
	}
	sBlitInfo.asDests[0].ui32ChromaPlaneOffset[1] = pBltInfo->uDstChromaPlane2;
	sBlitInfo.asDests[0].ui32ChunkStride = 0;

	// Dest surface size
	if (pBltInfo->DstSurfHeight)
	{
		sBlitInfo.asDests[0].ui32Height = pBltInfo->DstSurfHeight;
	}
	else
	{
		// Legacy support
		sBlitInfo.asDests[0].ui32Height = (IMG_UINT32)(pBltInfo->DstY + pBltInfo->DSizeY);
	}
	if (pBltInfo->DstSurfWidth)
	{
		sBlitInfo.asDests[0].ui32Width = pBltInfo->DstSurfWidth;
	}
	else
	{
		// Legacy support
		sBlitInfo.asDests[0].ui32Width = (IMG_UINT32)(pBltInfo->DstX + pBltInfo->DSizeX);
	}
	
	// Destination rectangle
	SGX_QUEUETRANSFER_NUM_DST_RECTS(sBlitInfo) = 1;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).x0 = rcDstRect.left;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).x1 = rcDstRect.right;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).y0 = rcDstRect.top;
	SGX_QUEUETRANSFER_DST_RECT(sBlitInfo, 0).y1 = rcDstRect.bottom;

	// TQ blt rotation
	if (pBltInfo->BlitFlags & (PVR2D_BLIT_ROT_90|PVR2D_BLIT_ROT_180|PVR2D_BLIT_ROT_270))
	{
#if defined (SGX_FEATURE_PTLA)
		// PTLA 2D core
		if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_90)
		{
			sBlitInfo.Details.sBlit.eRotation = SGXTQ_ROTATION_90;
		}
		else if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_180)
		{
			sBlitInfo.Details.sBlit.eRotation = SGXTQ_ROTATION_180;
		}
		else if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_270)
		{
			sBlitInfo.Details.sBlit.eRotation = SGXTQ_ROTATION_270;
		}
#else
		// 3D Core - apply rotation to layer 0 (source)
		if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_90)
		{
			sBlitInfo.ui32Flags |= SGX_TRANSFER_LAYER_0_ROT_90;
		}
		else if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_180)
		{
			sBlitInfo.ui32Flags |= SGX_TRANSFER_LAYER_0_ROT_180;
		}
		else if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_270)
		{
			sBlitInfo.ui32Flags |= SGX_TRANSFER_LAYER_0_ROT_270;
		}
#endif
	}
	// Copy order (mutually exclusive to rotation)
	else if ((pBltInfo->BlitFlags & (PVR2D_BLIT_COPYORDER_BR2TL|PVR2D_BLIT_COPYORDER_TR2BL|PVR2D_BLIT_COPYORDER_BL2TR))!=0)
	{
		if ((pBltInfo->BlitFlags & PVR2D_BLIT_COPYORDER_BR2TL)!=0)
		{
			sBlitInfo.Details.sBlit.eCopyOrder = SGXTQ_COPYORDER_BR2TL;
		}
		else if ((pBltInfo->BlitFlags & PVR2D_BLIT_COPYORDER_TR2BL)!=0)
		{
			sBlitInfo.Details.sBlit.eCopyOrder = SGXTQ_COPYORDER_TR2BL;
		}
		else
		{
			sBlitInfo.Details.sBlit.eCopyOrder = SGXTQ_COPYORDER_BL2TR;
		}
	}

#if defined(PDUMP) // Capture input surfaces
	PdumpInputSurfaces(psContext, (ui32BltFlags&(BLT_FLAG_SOURCE|BLT_FLAG_PATTERN))?IMG_TRUE:IMG_FALSE, pBltInfo);

	PVRSRVPDumpComment(psContext->psServices, "PVR2D:SGXQueueTransfer", IMG_FALSE);
#endif//#if defined(PDUMP)
	
	// Transfer queue blt (3D Core BltClipped)
	eResult = SGXQueueTransfer(psContext->hTransferContext, &sBlitInfo);
	
	if (eResult != PVRSRV_OK)
	{
		if (eResult == PVRSRV_ERROR_TIMEOUT)
		{
			return PVR2DERROR_DEVICE_UNAVAILABLE;
		}
		else
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2DBltClipped: SGXQueueTransfer failed with error 0x%08X", eResult));
			return PVR2DERROR_GENERIC_ERROR;
		}
	}

#ifdef SYNC_DEBUG
	// Debug with synchronous blts
	PVR2DQueryBlitsComplete(hContext, pBltInfo->pDstMemInfo, 1);
#endif

#if defined(PDUMP) // Output surface
	PVRSRVPDumpComment(psContext->psServices, "PVR2D: poll for Transfer Queue op complete", IMG_FALSE);
	PVRSRVPDumpSyncPol(	psContext->psServices,
						pDstMemInfo->psClientSyncInfo,
						IMG_FALSE,
						pDstMemInfo->psClientSyncInfo->psSyncData->ui32LastOpDumpVal,
						0xFFFFFFFF);
	PdumpOutputSurface(psContext, pBltInfo);
#endif//#if defined(PDUMP)

	// Debug test to confirm it completed
	#if 0
	eResult = PollForValue(&pDstMemInfo->psClientSyncInfo->psSyncData->ui32WriteOpsComplete,
							pDstMemInfo->psClientSyncInfo->psSyncData->ui32WriteOpsPending,
							pDstMemInfo->psClientSyncInfo->psSyncData->ui32WriteOpsPending,1000, 1000);
	#endif

	return PVR2D_OK;

}// AltBltClipped


#endif // #if defined(PVR2D_ALT_2DHW)

#if defined(PVR2D_VALIDATE_INPUT_PARAMS)
/*!****************************************************************************
 @Function	ValidateParams

 @Input		Inputs are all rectangles passed into a blt command

 @Return	PVR2DERROR : indicates error if input params are beyond repair

 @Description : Validates and repairs the input rectangles submitted to a blt command.
	Deals with out of bounds and badly behaved rectangles

	DEBUG BUILD - returns an error if the destination rectangle goes outside dest surface
	RELEASE BUILD - corrects input params if PVR2D_VALIDATE_INPUT_PARAMS is defined
******************************************************************************/
static PVR2DERROR ValidateParams (	PVR2DBLTINFO *pBltInfo,
									PVR2D_ULONG ulNumClipRects,
									PVR2DRECT *pClipRect )
{
	PVR2DRECT rcDstSurf;
	PVR2DRECT rcDstRect;
	PVR2DRECT rcValidDstRect;
	PVR2DRECT rcValidSrcRect;
	PVR2DRECT rcBltDeltas;
	PVR2DRECT rcSrcRect;
	PVR2DRECT rcSrcSurf;
	PVR2DRECT rcValidClipRect;
	IMG_BOOL bValidMask;
	IMG_BOOL bValidPattern;
	IMG_UINT32 ui32BltFlags;
	PVR2D_BOOL bChanged=PVR2D_FALSE;
	IMG_INT32 i32NegStrideOffset;

	// Dst rect : width and height must be >0
	if (pBltInfo->DSizeY <= 0 || pBltInfo->DSizeX <= 0)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "Dst rect width/height must be >0"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	// For older legacy apps, surface sizes were not specified and so no boundry checking is possible
	if (!pBltInfo->DstSurfHeight && !pBltInfo->DstSurfWidth)
	{
		return PVR2D_OK; // old apps don't allow error checking
	}

	// get dest rect
	rcDstRect.top = pBltInfo->DstY;
	rcDstRect.bottom = rcDstRect.top + pBltInfo->DSizeY;
	rcDstRect.left = pBltInfo->DstX;
	rcDstRect.right = rcDstRect.left + pBltInfo->DSizeX;

	// get dest surf
	rcDstSurf.top    = 0;
	rcDstSurf.bottom = (PVR2D_LONG)pBltInfo->DstSurfHeight;
	rcDstSurf.left   = 0;
	rcDstSurf.right  = (PVR2D_LONG)pBltInfo->DstSurfWidth;

	// get source surface rect
	rcSrcSurf.top    = 0;
	rcSrcSurf.bottom = (PVR2D_LONG)pBltInfo->SrcSurfHeight;
	rcSrcSurf.left   = 0;
	rcSrcSurf.right  = (PVR2D_LONG)pBltInfo->SrcSurfWidth;

	// get source rect
	rcSrcRect.top    = pBltInfo->SrcY;
	rcSrcRect.bottom = pBltInfo->SrcY + pBltInfo->SizeY;
	rcSrcRect.left   = pBltInfo->SrcX;
	rcSrcRect.right  = pBltInfo->SrcX + pBltInfo->SizeX;

	// enumerate the input surfaces implied by the ROP code
	bValidMask = (IMG_BOOL)(pBltInfo->pMaskMemInfo != 0);
	bValidPattern = (IMG_BOOL)((pBltInfo->BlitFlags & PVR2D_BLIT_PAT_SURFACE_ENABLE) != 0);
	ui32BltFlags = GetSurfs (pBltInfo->CopyCode, bValidPattern, bValidMask);

	if (ui32BltFlags & BLT_FLAG_PATTERN)
	{
		if (pBltInfo->SizeY <= 0 || pBltInfo->SizeX <= 0)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "Pattern size must be > 0"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
		
		// Check that pattern size is not larger than the pattern surface
		if (	(pBltInfo->SrcSurfWidth  &&  (pBltInfo->SizeX > (PVR2D_LONG)pBltInfo->SrcSurfWidth))
			||	(pBltInfo->SrcSurfHeight &&  (pBltInfo->SizeY > (PVR2D_LONG)pBltInfo->SrcSurfHeight))	)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "Pattern cannot be larger than pattern surface"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
		
		// Correction for pattern x-start outside pattern surface
		if (pBltInfo->SrcX < 0)
		{
			PVR2D_LONG mod;
			mod = (-pBltInfo->SrcX) % pBltInfo->SizeX;
			pBltInfo->SrcX = pBltInfo->SizeX - mod;
		}
		else if (pBltInfo->SrcX > pBltInfo->SizeX)
		{
			PVR2D_LONG mod;
			mod = pBltInfo->SrcX % pBltInfo->SizeX;
			pBltInfo->SrcX = mod;
		}
	
		// Correction for pattern y-start outside pattern surface
		if (pBltInfo->SrcY < 0)
		{
			PVR2D_LONG mod;
			mod = (-pBltInfo->SrcY) % pBltInfo->SizeY;
			pBltInfo->SrcY = pBltInfo->SizeY - mod;
		}
		else if (pBltInfo->SrcY > pBltInfo->SizeY)
		{
			PVR2D_LONG mod;
			mod = pBltInfo->SrcY % pBltInfo->SizeY;
			pBltInfo->SrcY = mod;
		}
	}

	// find intersection of source rect with source surf
	if (ui32BltFlags & BLT_FLAG_SOURCE)
	{
		// Src width and height must be >0
		if (pBltInfo->SizeY <= 0 || pBltInfo->SizeX <= 0)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "Src rect width/height must be >0"));
			return PVR2DERROR_INVALID_PARAMETER;
		}

		// For older legacy apps, surface sizes were not specified and so boundry checking is not possible
		if ( ((pBltInfo->DstSurfHeight && pBltInfo->DstSurfWidth) != 0)
			// Source rect validation only supported for unrotated case at present
			&& ((pBltInfo->BlitFlags&(PVR2D_BLIT_ROT_90|PVR2D_BLIT_ROT_180|PVR2D_BLIT_ROT_270)) == 0) )
		{
			// Source surface sizes specified so continue
			if (bIntersect(&rcSrcSurf, &rcSrcRect, &rcValidSrcRect))
			{
				// Calc how many pixels are being taken off each side to prevent this blt going out of bounds
				rcBltDeltas.top = rcValidSrcRect.top - rcSrcRect.top;
				rcBltDeltas.left =  rcValidSrcRect.left - rcSrcRect.left;
				rcBltDeltas.bottom =  rcSrcRect.bottom - rcValidSrcRect.bottom;
				rcBltDeltas.right =  rcSrcRect.right - rcValidSrcRect.right;

				if (rcBltDeltas.top || rcBltDeltas.left || rcBltDeltas.bottom || rcBltDeltas.right)
				{
					PVR2D_DPF((PVR_DBG_ERROR, "PVR2D - Source rectangle goes outside the source surface"));

					// Update src rect
					rcSrcRect = rcValidSrcRect;
					pBltInfo->SrcY = rcSrcRect.top;
					pBltInfo->SizeY = rcSrcRect.bottom - rcSrcRect.top;
					pBltInfo->SrcX = rcSrcRect.left;
					pBltInfo->SizeX = rcSrcRect.right - rcSrcRect.left;

					// Subtract the same deltas from the dest rect
					rcDstRect.top += rcBltDeltas.top;
					rcDstRect.left += rcBltDeltas.left;
					rcDstRect.bottom -= rcBltDeltas.bottom;
					rcDstRect.right -= rcBltDeltas.right;

					pBltInfo->DstY = rcDstRect.top;
					pBltInfo->DSizeY = rcDstRect.bottom - rcDstRect.top;
					pBltInfo->DstX = rcDstRect.left;
					pBltInfo->DSizeX = rcDstRect.right - rcDstRect.left;
				}
			}
			else
			{
				// Source rect does not intersect source surface
				PVR2D_DPF((PVR_DBG_ERROR, "No intersection of source surf and source rect"));
				return PVR2DERROR_INVALID_PARAMETER;
			}
		}
	}

	// find intersection of dest rect with dest surf
	if (bIntersect(&rcDstSurf, &rcDstRect, &rcValidDstRect))
	{
		// Calc how many pixels are being taken off each side to prevent this blt going out of bounds
		rcBltDeltas.top = rcValidDstRect.top - rcDstRect.top;
		rcBltDeltas.left =  rcValidDstRect.left - rcDstRect.left;
		rcBltDeltas.bottom =  rcDstRect.bottom - rcValidDstRect.bottom;
		rcBltDeltas.right =  rcDstRect.right - rcValidDstRect.right;

		if (rcBltDeltas.top || rcBltDeltas.left || rcBltDeltas.bottom || rcBltDeltas.right)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PVR2D - Blt rectangle goes outside the destination surface"));
			bChanged = PVR2D_TRUE;
		}
	}
	else
	{
		PVR2D_DPF((PVR_DBG_ERROR, "No intersection of dst surf and dst rect"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	if (bChanged != PVR2D_FALSE)
	{
		// Set valid dest rect
		pBltInfo->DstY = rcValidDstRect.top;
		pBltInfo->DSizeY = rcValidDstRect.bottom - rcValidDstRect.top;
		pBltInfo->DstX = rcValidDstRect.left;
		pBltInfo->DSizeX = rcValidDstRect.right - rcValidDstRect.left;

		// If the ROP implies a src then apply the intersection deltas to the src rect to give correct src offset.
		if (ui32BltFlags & BLT_FLAG_SOURCE)
		{
			// Apply blt correction deltas to source rect applying dest -> source rotation
			if (pBltInfo->BlitFlags&(PVR2D_BLIT_ROT_90|PVR2D_BLIT_ROT_180|PVR2D_BLIT_ROT_270))
			{
				if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_90)
				{
					rcSrcRect.top += rcBltDeltas.right;
					rcSrcRect.left += rcBltDeltas.top;
					rcSrcRect.bottom -= rcBltDeltas.left;
					rcSrcRect.right -= rcBltDeltas.bottom;
				}
				else if (pBltInfo->BlitFlags & PVR2D_BLIT_ROT_270)
				{
					rcSrcRect.top += rcBltDeltas.left;
					rcSrcRect.left += rcBltDeltas.bottom;
					rcSrcRect.bottom -= rcBltDeltas.right;
					rcSrcRect.right -= rcBltDeltas.top;
				}
				else
				{
					// 180
					rcSrcRect.top += rcBltDeltas.bottom;
					rcSrcRect.left += rcBltDeltas.right;
					rcSrcRect.bottom -= rcBltDeltas.top;
					rcSrcRect.right -= rcBltDeltas.left;
				}
			}
			else
			{
				// No rotation
				rcSrcRect.top += rcBltDeltas.top;
				rcSrcRect.left += rcBltDeltas.left;
				rcSrcRect.bottom -= rcBltDeltas.bottom;
				rcSrcRect.right -= rcBltDeltas.right;
			}

			// set src rect
			pBltInfo->SrcY = rcSrcRect.top;
			pBltInfo->SizeY = rcSrcRect.bottom - rcSrcRect.top;
			pBltInfo->SrcX = rcSrcRect.left;
			pBltInfo->SizeX = rcSrcRect.right - rcSrcRect.left;
		}
	}

	// Clip rect cannot go outside the dest surface so check it and correct it
	if (ulNumClipRects && pClipRect)
	{
		// Find intersection of clip rect with dest surf
		if (bIntersect(&rcDstSurf, pClipRect, &rcValidClipRect))
		{
			rcBltDeltas.top = rcValidClipRect.top - pClipRect->top;
			rcBltDeltas.left =  rcValidClipRect.left - pClipRect->left;
			rcBltDeltas.bottom =  pClipRect->bottom - rcValidClipRect.bottom;
			rcBltDeltas.right =  pClipRect->right - rcValidClipRect.right;

			// Detect clip rectangle going outside of destination surface
			if (rcBltDeltas.top || rcBltDeltas.left || rcBltDeltas.bottom || rcBltDeltas.right)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PVR2D - Clip rectangle goes outside the destination surface"));
				*pClipRect = rcValidClipRect; // structure copy
			}
		}
		else
		{
			PVR2D_DPF((PVR_DBG_ERROR, "No intersection of dst surf and clip rect"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
	}

	// Validate source memory for source and patterns
	if (ui32BltFlags & (BLT_FLAG_SOURCE|BLT_FLAG_PATTERN))
	{
		// Validate negative source stride
		if (pBltInfo->SrcStride < 0)
		{
			i32NegStrideOffset = (IMG_INT32)(pBltInfo->SrcSurfHeight-1) * pBltInfo->SrcStride;

			if (((IMG_INT32)pBltInfo->SrcOffset + i32NegStrideOffset) < 0)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "Negative stride : SrcOffset too small"));
				return PVR2DERROR_INVALID_PARAMETER;
			}
		}

		if (!pBltInfo->pSrcMemInfo)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "NUL pSrcMemInfo"));
			return PVR2DERROR_INVALID_PARAMETER;
		}

		if (pBltInfo->SrcOffset > pBltInfo->pSrcMemInfo->ui32MemSize)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "SrcOffset too large, conflicts with pSrcMemInfo->ui32MemSize"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
	}

	// Validate negative dest stride
	if (pBltInfo->DstStride < 0)
	{
		i32NegStrideOffset = (IMG_INT32)(pBltInfo->DstSurfHeight-1) * pBltInfo->DstStride;

		if (((IMG_INT32)pBltInfo->DstOffset + i32NegStrideOffset) < 0)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "Negative stride : DstOffset too small"));
			return PVR2DERROR_INVALID_PARAMETER;
		}
	}

	// Validate dest memory
	if (pBltInfo->DstOffset > pBltInfo->pDstMemInfo->ui32MemSize)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "DstOffset too large conflicts with pDstMemInfo->ui32MemSize"));
		return PVR2DERROR_INVALID_PARAMETER;
	}

	return PVR2D_OK;
}


#endif//#if defined(PVR2D_VALIDATE_INPUT_PARAMS)


#ifdef PDUMP // Capture blt input surfaces

static void PdumpInputSurfaces(PVR2DCONTEXT *psContext, IMG_BOOL bSrcIsPresent, PVR2DBLTINFO *pBltInfo)
{
	IMG_INT32 i32Delta;
	PVRSRV_ERROR srvErr;
	IMG_BOOL bIsCapturing;
	IMG_INT32 i32PdumpStride;
	IMG_INT32 i32MemOffset;
	IMG_SIZE_T uiCaptureSize;
	PVRSRV_CLIENT_MEM_INFO * pClientMemInfo;
	IMG_INT32 i32Y0, i32Y1;
	IMG_UINT32 uSrcFormat;

	srvErr = PVRSRVPDumpIsCapturing(psContext->psServices, &bIsCapturing);

	if ((srvErr!=PVRSRV_OK) || !bIsCapturing)
	{
		return;
	}

	// Pdump SOURCE
	if (bSrcIsPresent && ((pBltInfo->SrcFormat & (PVR2D_SURFACE_PDUMP|PVR2D_BLTRECT_PDUMP)) != 0))
	{
		uSrcFormat = pBltInfo->SrcFormat & PVR2D_FORMAT_MASK;

		// Wait for any outstanding activity to end before copying the input surfaces to the pdump file
		PVR2DQueryBlitsComplete((PVR2DCONTEXTHANDLE)psContext, pBltInfo->pSrcMemInfo, 1);

		i32MemOffset   = (IMG_INT32)pBltInfo->SrcOffset;
		i32PdumpStride = pBltInfo->SrcStride;

		// Src negative stride support
		if (pBltInfo->SrcStride < 0)
		{
			i32PdumpStride = 0 - pBltInfo->SrcStride; // Map to positive stride

			if (pBltInfo->SrcSurfHeight)
			{
				// Subtract neg stride offset, the no. of bytes from pixel 0,0 to the start of the surface
				i32MemOffset -= (IMG_INT32)(pBltInfo->SrcSurfHeight-1) * i32PdumpStride;

				// Invert the source offsets
				i32Y0 = (IMG_INT32)pBltInfo->SrcSurfHeight - (pBltInfo->SrcY + pBltInfo->SizeY);
				i32Y1 = (IMG_INT32)pBltInfo->SrcSurfHeight - pBltInfo->SrcY;
			}
			else
			{
				PVRSRVPDumpComment(psContext->psServices, "PVR2D Source Error - neg stride pdump needs surf size", IMG_FALSE);
				i32Y0 = 0L;
				i32Y1 = 0L;
			}
		}
		else
		{
			i32Y0 = pBltInfo->SrcY;
			i32Y1 = pBltInfo->SrcY + pBltInfo->SizeY;
		}

		if ((pBltInfo->SrcFormat & PVR2D_SURFACE_PDUMP) != 0)
		{
			// Capture entire surface
			i32Delta = 0;
			uiCaptureSize = (IMG_SIZE_T)pBltInfo->SrcSurfHeight * i32PdumpStride;
		}
		else
		{
			// PVR2D_BLTRECT_PDUMP
			// Capture the source from the first pixel that will be read
			i32Delta = i32Y0 * i32PdumpStride; // Start capturing memory from this address
			uiCaptureSize = (IMG_SIZE_T)((i32Y1 - i32Y0) * i32PdumpStride);
		}

		pClientMemInfo = (PVRSRV_CLIENT_MEM_INFO *) pBltInfo->pSrcMemInfo->hPrivateData;

		if (!uiCaptureSize || (uiCaptureSize > pClientMemInfo->uAllocSize))
		{
			uiCaptureSize = pClientMemInfo->uAllocSize;
			i32MemOffset = 0;
			i32Delta = 0;
		}

		PVRSRVPDumpComment(psContext->psServices, "PVR2D:PdumpInputSurfaces PVRSRVPDumpMem source pixels", IMG_FALSE);

		srvErr = PVRSRVPDumpMem(	psContext->psServices,
						pBltInfo->pSrcMemInfo->pBase,	/* base address (host) */
						pClientMemInfo,					/* base info (device) */
						(IMG_UINT32)(i32MemOffset + i32Delta),		/* offset */
						(IMG_UINT32)uiCaptureSize,				/* bytes */
						0);

		if (srvErr != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PdumpInputSurfaces: PVRSRVPDumpMem failed for 2D source"));
		}

		// Palette support
		if (ui32PaletteSize[uSrcFormat] && pBltInfo->pPalMemInfo)
		{
			PVRSRVPDumpComment(psContext->psServices, "PVR2D: Pdump Source Palette", IMG_FALSE);

			pClientMemInfo = (PVRSRV_CLIENT_MEM_INFO *) pBltInfo->pPalMemInfo->hPrivateData;

			srvErr = PVRSRVPDumpMem(	psContext->psServices,
							pBltInfo->pPalMemInfo->pBase,				/* base address (host) */
							pClientMemInfo,								/* base info (device) */
							pBltInfo->PalOffset,						/* offset */
							ui32PaletteSize[uSrcFormat]<<2,	/* bytes */
							0);

			if (srvErr != PVRSRV_OK)
			{
				PVR2D_DPF((PVR_DBG_ERROR, "PdumpInputSurfaces: PVRSRVPDumpMem failed for 2D source palette"));
			}
		}
	}

	// Pdump MASK surface
	if (pBltInfo->pMaskMemInfo)
	{
		PVRSRVPDumpComment(psContext->psServices, "PVR2D:PdumpInputSurfaces PVRSRVPDumpMem mask surface", IMG_FALSE);
		pClientMemInfo = (PVRSRV_CLIENT_MEM_INFO *) pBltInfo->pMaskMemInfo->hPrivateData;

		i32MemOffset  = (IMG_INT32)pBltInfo->MaskOffset;
		i32PdumpStride = pBltInfo->MaskStride;

		// Negative stride mask support
		if (pBltInfo->MaskStride < 0)
		{
			i32PdumpStride = 0 - pBltInfo->MaskStride; // Map to positive stride

			if (pBltInfo->MaskSurfHeight)
			{
				// Subtract neg stride offset, the no. of bytes from pixel 0,0 to the start of the surface
				i32MemOffset -= ((IMG_INT32)(pBltInfo->MaskSurfHeight-1) * i32PdumpStride);
			}
			else
			{
				PVRSRVPDumpComment(psContext->psServices, "PVR2D Mask Error - neg stride pdump needs surf size", IMG_FALSE);
			}
		}

		// Capture the source from the first pixel that will be read
		uiCaptureSize = (IMG_SIZE_T)pBltInfo->MaskSurfHeight * (IMG_SIZE_T)i32PdumpStride;

		if (!uiCaptureSize || (uiCaptureSize > pClientMemInfo->uAllocSize))
		{
			uiCaptureSize = pClientMemInfo->uAllocSize;
			i32MemOffset = 0;
		}

		srvErr = PVRSRVPDumpMem(	psContext->psServices,
						pBltInfo->pMaskMemInfo->pBase,	/* base address (host) */
						pClientMemInfo,					/* base info (device) */
						(IMG_UINT32)i32MemOffset,		/* offset */
						(IMG_UINT32)uiCaptureSize,		/* bytes */
						0);

		if (srvErr != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PdumpInputSurfaces: PVRSRVPDumpMem failed for 2D mask"));
		}
	}

	// Capture destination if required
	if ((pBltInfo->DstFormat & (PVR2D_SURFACE_PDUMP|PVR2D_BLTRECT_PDUMP))!=0)
	{
		PVR2DQueryBlitsComplete((PVR2DCONTEXTHANDLE)psContext, pBltInfo->pDstMemInfo, 1);

		// i32PdumpStride must be positive
		// i32MemOffset is offset from alloc base to start of positive stride surface
		// i32Delta is offset from start of surface to start of blt rectangle
		if (pBltInfo->DstStride < 0)  // negative stride support
		{
			i32PdumpStride = - pBltInfo->DstStride; // map to positive stride
			i32MemOffset = (IMG_INT32)(pBltInfo->DstOffset - ((pBltInfo->DstSurfHeight-1) * (PVR2D_ULONG)i32PdumpStride));
			i32Delta = (IMG_INT32)pBltInfo->DstSurfHeight - (pBltInfo->DstY + pBltInfo->DSizeY);
		}
		else
		{
			i32PdumpStride = pBltInfo->DstStride;
			i32MemOffset = (IMG_INT32)pBltInfo->DstOffset;
			i32Delta = pBltInfo->DstY;
		}
		
		PVRSRVPDumpComment(psContext->psServices, "PVR2D:PdumpInputSurfaces : PVRSRVPDumpMem destination pixels", IMG_FALSE);
		
		if ((pBltInfo->DstFormat & PVR2D_SURFACE_PDUMP) != 0)
		{
			// Capture entire dest surface
			i32Delta = 0;
			uiCaptureSize = (IMG_SIZE_T)(i32PdumpStride * pBltInfo->DstSurfHeight); // size of surface
		}
		else
		{
			// PVR2D_BLTRECT_PDUMP
			// Capture dest rect pixels 
			i32Delta *= i32PdumpStride; // offset from start of surface to start of blt rectangle
			uiCaptureSize = (IMG_SIZE_T)(pBltInfo->DSizeY * i32PdumpStride); // no of bytes to capture
		}

		i32MemOffset += i32Delta; // capture from start of blt rectangle
		pClientMemInfo = (PVRSRV_CLIENT_MEM_INFO *) pBltInfo->pDstMemInfo->hPrivateData;
		
		// Check for overrun
		if (pClientMemInfo->uAllocSize < (uiCaptureSize + (IMG_SIZE_T)i32MemOffset))
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PdumpInputSurfaces: uiCaptureSize error for 2D dest"));
			
			// fall back to capture the entire allocation
			uiCaptureSize = pClientMemInfo->uAllocSize;
			i32MemOffset = 0;
		}
		
		srvErr = PVRSRVPDumpMem(psContext->psServices,
							pBltInfo->pDstMemInfo->pBase,			/* base address (host) */
							pClientMemInfo,							/* base address (device) */
				 			(IMG_UINT32)i32MemOffset,				/* offset */
				 			(IMG_UINT32)uiCaptureSize,				/* bytes */
				 			0);

		if (srvErr != PVRSRV_OK)
		{
			PVR2D_DPF((PVR_DBG_ERROR, "PdumpInputSurfaces: PVRSRVPDumpMem failed for 2D dest"));
		}
	}

}//PdumpInputSurfaces

#ifdef PVR2D_ALT_2DHW
static IMG_UINT32 GetPo2(IMG_UINT32 a)
{
	a--;
	a |= a >> 1;
	a |= a >> 2;
	a |= a >> 4;
	a |= a >> 8;
	a |= a >> 16;
	a++;
	return a;
}
#endif

// PdumpOutputSurface waits for writes to finish before capturing the output
static void PdumpOutputSurface(PVR2DCONTEXT *psContext, PVR2DBLTINFO *pBltInfo)
{
#ifdef PVR2D_ALT_2DHW
	IMG_INT32 i32Y0, i32Y1;
	IMG_DEV_VIRTADDR addr;
	IMG_CHAR szBuf[20] = {'B','l','t'};	/* PRQA S 0686 */ /* Only part prefill array */
	PVRSRV_ERROR srvErr;
	IMG_BOOL bIsCapturing;
	IMG_INT32 i32PdumpStride;
	const PVR2D_ULONG DstFormat = (PVR2D_ULONG)pBltInfo->DstFormat & PVR2D_FORMAT_MASK;
	IMG_INT32 ui32BitmapWidth, ui32BitmapHeight;
	
	srvErr = PVRSRVPDumpIsCapturing(psContext->psServices, &bIsCapturing);

	if ((srvErr!=PVRSRV_OK) || !bIsCapturing)
	{
		return;
	}

	i32PdumpStride = pBltInfo->DstStride;

	// Address of pixel 0,0
	addr.uiAddr = pBltInfo->pDstMemInfo->ui32DevAddr + pBltInfo->DstOffset;

	if (pBltInfo->DstFormat & PVR2D_FORMAT_LAYOUT_TWIDDLED)
	{
		// Support npo2 in po2 mem footprint
		ui32BitmapWidth =  GetPo2(pBltInfo->DSizeX);
		ui32BitmapHeight = GetPo2(pBltInfo->DSizeY);
		i32Y0 = 0;
		i32Y1 = ui32BitmapHeight;
	}
	else
	{
		// Negative stride support
		if (pBltInfo->DstStride < 0)
		{
			i32PdumpStride = 0 - pBltInfo->DstStride; // Map to positive stride

			if (pBltInfo->DstSurfHeight)
			{
				// Address of surface mem
				addr.uiAddr -= (pBltInfo->DstSurfHeight-1) * (IMG_UINT32)i32PdumpStride;
			}
			else
			{
				PVRSRVPDumpComment(psContext->psServices, "PVR2D DumpBitmap Error - neg stride pdump needs surf size", IMG_FALSE);
			}

			// Invert the dest offsets
			i32Y0 = (IMG_INT)pBltInfo->DstSurfHeight - (pBltInfo->DstY + pBltInfo->DSizeY);
			i32Y1 = (IMG_INT)pBltInfo->DstSurfHeight - pBltInfo->DstY;
		}
		else
		{
			i32Y0 = pBltInfo->DstY;
			i32Y1 = pBltInfo->DstY + pBltInfo->DSizeY;
		}
		
		// Dump the blt output after the blt has completed
		addr.uiAddr += (IMG_UINT32)(i32Y0 * i32PdumpStride); // Y start
		addr.uiAddr += ((((IMG_UINT32)pBltInfo->DstX * aPVR2DBitsPP[DstFormat])+7U)>>3); // X start
		
		ui32BitmapWidth = pBltInfo->DSizeX;
		ui32BitmapHeight = pBltInfo->DSizeY;
	}
	
	// Wait for writes to finish before capturing the output
	PVR2DQueryBlitsComplete((PVR2DCONTEXTHANDLE)psContext, pBltInfo->pDstMemInfo, 1);

	// Incrementing file name for bitmap dump
	i2a (psContext->lBltCount++, szBuf+3);

#else
	PVR_UNREFERENCED_PARAMETER(pBltInfo);
#endif//#ifdef PVR2D_ALT_2DHW

	PVRSRVPDumpComment(psContext->psServices, "PVR2D:PdumpOutputSurface PVRSRVPDumpBitmap blt output", IMG_FALSE);

#ifdef PVR2D_ALT_2DHW
	srvErr = PVRSRVPDumpBitmap(&psContext->sDevData,
	 					szBuf,
						0,
						ui32BitmapWidth,								/* width */
						ui32BitmapHeight,								/* height */
						(IMG_UINT32)i32PdumpStride,						/* stride */
						addr,											/* base address (device) */
						psContext->hDevMemContext,						/* memory context (device) */
						(IMG_UINT32)((i32Y1 - i32Y0) * i32PdumpStride),	/* size */
						aPdumpFormat[DstFormat],
						PVRSRV_PDUMP_MEM_FORMAT_STRIDE,
						0);
	
	if (srvErr != PVRSRV_OK)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PdumpOutputSurface: PVRSRVPDumpBitmap failed"));
	}

#endif//#ifdef PVR2D_ALT_2DHW

}//PdumpOutputSurface

#if defined(CAPTURE_PRESENTBLT_INPUT)
static void PdumpPresentBltInputSurface(PVR2DCONTEXT *psContext, PVR2DMEMINFO	*pSrcMemInfo, PVR2DRECT *prcSrcRect)
{
	IMG_UINT32 ui32BytesPerPixel;
	IMG_INT32 i32Delta;
	IMG_INT32 i32SrcStride;
	PVRSRV_ERROR srvErr;
	IMG_BOOL bIsCapturing;

	srvErr = PVRSRVPDumpIsCapturing(psContext->psServices, &bIsCapturing);

	if ((srvErr!=PVRSRV_OK) || !bIsCapturing)
	{
		return;
	}

#if defined(PVR2D_ALT_2DHW)
	i32SrcStride = psContext->lPresentBltSrcStride;
#else
#if defined(SGX_FEATURE_2D_HARDWARE)
	// Extract source stride from 2D source data
	i32SrcStride = psContext->aui32PresentBltMiscStateBlock[PRESENT_BLT_MISC_SRC_HEADER] & EURASIA2D_SRC_STRIDE_MASK;

	// Sign extend if negative stride
	if (i32SrcStride & 0x00004000)
	{
		i32SrcStride|= 0xFFFF8000;
	}
#endif
#endif//#if defined(PVR2D_ALT_2DHW)

	ui32BytesPerPixel = (psContext->sPrimary.pixelformat == PVRSRV_PIXEL_FORMAT_ARGB8888)?4:2;

	// Capture the blt rectangle
	i32Delta = (prcSrcRect->top * i32SrcStride) + (prcSrcRect->left * ui32BytesPerPixel);

	// Wait for any outstanding activity to end before copying the input surfaces to the pdump file
	PVR2DQueryBlitsComplete((PVR2DCONTEXTHANDLE)psContext, pSrcMemInfo, 1);

	PVRSRVPDumpComment(psContext->psServices, "PVR2D:PdumpPresentBltInputSurface PVRSRVPDumpMem presentBlt source", IMG_FALSE);
	PVRSRVPDumpMem(	psContext->psServices,
					pSrcMemInfo->pBase,
					CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(pSrcMemInfo),
					i32Delta,
					((prcSrcRect->bottom-prcSrcRect->top)*i32SrcStride) - (prcSrcRect->left * ui32BytesPerPixel),
					0);
}
#endif//#if defined(CAPTURE_PRESENTBLT_INPUT)

static void PdumpPresentBltOutputSurface(PVR2DCONTEXT *psContext, PVR2DMEMINFO	*pDstMemInfo, PVR2DRECT *prcDstRect)
{
	IMG_UINT32 ui32BytesPerPixel;
	IMG_INT32 i32Delta;
	IMG_DEV_VIRTADDR addr;
	IMG_UINT32 ui32Height, ui32Width;
	PDUMP_PIXEL_FORMAT ePixelFormat;
	IMG_CHAR szFileNameBuf[20] = {'P','b','l','t'};	/* PRQA S 0686 */ /* Only part prefill array */
	PVRSRV_ERROR srvErr;
	IMG_BOOL bIsCapturing;

	srvErr = PVRSRVPDumpIsCapturing(psContext->psServices, &bIsCapturing);

	if ((srvErr!=PVRSRV_OK) || !bIsCapturing)
	{
		return;
	}

	switch(psContext->sPrimary.pixelformat)
	{
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		{
			ePixelFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB8888;
			ui32BytesPerPixel = 4;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			ePixelFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB1555;
			ui32BytesPerPixel = 2;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			ePixelFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB4444;
			ui32BytesPerPixel = 2;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_RGB565:
		{
			ePixelFormat = PVRSRV_PDUMP_PIXEL_FORMAT_RGB565;
			ui32BytesPerPixel = 2;
			break;
		}
		default: return;
	}

	// Capture the blt rectangle
	i32Delta = (prcDstRect->top * (IMG_INT32)psContext->sPrimary.sDims.ui32ByteStride) + (prcDstRect->left * (IMG_INT32)ui32BytesPerPixel);
	addr.uiAddr = pDstMemInfo->ui32DevAddr + (IMG_UINT32)i32Delta;

	// Incrementing file name for bitmap dump
	i2a (psContext->lPresentBltCount++, szFileNameBuf+4);

	// Wait for any outstanding activity to end before copying the destination surface
	PVR2DQueryBlitsComplete((PVR2DCONTEXTHANDLE)psContext, pDstMemInfo, 1);

	ui32Height = (IMG_UINT32)(prcDstRect->bottom - prcDstRect->top);
	ui32Width  = (IMG_UINT32)(prcDstRect->right - prcDstRect->left);

	PVRSRVPDumpComment(psContext->psServices, "PVR2D:PdumpPresentBltOutputSurface PVRSRVPDumpBitmap output pixels", IMG_FALSE);
	PVRSRVPDumpBitmap(&psContext->sDevData,
	 					szFileNameBuf,
						0,
						ui32Width,
						ui32Height,
						psContext->sPrimary.sDims.ui32ByteStride,
						addr,
						psContext->hDevMemContext,
						ui32Height*psContext->sPrimary.sDims.ui32ByteStride,
						ePixelFormat,
						PVRSRV_PDUMP_MEM_FORMAT_STRIDE,
						0);
}

#endif//#ifdef PDUMP


/*****************************************************************************
 @Function	PVR2DSet1555Alpha

 @Input		hContext : PVR2D context handle

 @Input		Alpha0 : 8 bit alpha value when 1555 source alpha is 0

 @Input		Alpha1 : 8 bit alpha value when 1555 source alpha is 1

  @Return	error : PVR2D error code

 @Description : Allows the caller to know if blts will go via 2D or 3D Core
******************************************************************************/
PVR2D_EXPORT
PVR2DERROR PVR2DSet1555Alpha (PVR2DCONTEXTHANDLE hContext, PVR2D_UCHAR Alpha0, PVR2D_UCHAR Alpha1)
{
#if defined(EUR_CR_2D_ALPHA_COMPONENT_ZERO_MASK) && !defined(PVR2D_ALT_2DHW)
	PVR2DCONTEXT *psContext = (PVR2DCONTEXT *)hContext;

	if (!hContext)
	{
		PVR2D_DPF((PVR_DBG_ERROR, "PVR2DSet1555Alpha - Invalid context"));
		return PVR2DERROR_INVALID_CONTEXT;
	}

	/* Update the 1555 Lookup alpha values */
	psContext->ui32AlphaRegValue = ALPHA_1555( ((PVR2D_ULONG)Alpha0), ((PVR2D_ULONG)Alpha1));
	return PVR2D_OK;
#else
	PVR_UNREFERENCED_PARAMETER(hContext);
	PVR_UNREFERENCED_PARAMETER(Alpha0);
	PVR_UNREFERENCED_PARAMETER(Alpha1);
	return PVR2DERROR_HW_FEATURE_NOT_SUPPORTED;
#endif
}


/******************************************************************************
 End of file (pvr2dblt.c)
******************************************************************************/
