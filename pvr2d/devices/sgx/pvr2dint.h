/*!****************************************************************************
@File           pvr2dint.h

@Title          PVR2D library internal header file

@Author         Imagination Technologies

@Date           14/12/2006

@Copyright      Copyright 2006-2007 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or otherwise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform       Generic

@Description    PVR2D library internal definitions

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: pvr2dint.h $
******************************************************************************/

#ifndef PVR2D_INT_H
#define PVR2D_INT_H

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "sgxapi.h"
#include "sgxmmu.h"
#include "pvr2d.h"
#include "pvr_debug.h"

#if defined (__cplusplus)
extern "C" {
#endif

#ifdef PVR2D_ALT_2DHW
#ifdef SGX_FEATURE_2D_HARDWARE
#undef SGX_FEATURE_2D_HARDWARE
#endif
#endif

/* Maximum number of clipping rectangles */
#define MAX_CLIP_RECTS		1

#define	CLIPBLOCK_HEADER	0
#define	CLIPBLOCK_YMAXMIN	1
#define	SIZEOF_CLIPBLOCK	2

/*
	clip block blt command fragment structure
*/
typedef struct _PVR2D_CLIPBLOCK
{
#if defined PVR2D_ALT_2DHW
	PVR2DRECT rcRect; // rectangles describing the un-occluded areas of the surface to be presented
#else
	IMG_UINT32 aui32Data[SIZEOF_CLIPBLOCK];
#endif

} PVR2D_CLIPBLOCK;


/* surface ownership */
typedef enum
{
	PVR2D_MEMINFO_ALLOCATED = 0,	/* allocated buffer */
	PVR2D_MEMINFO_DISPLAY_OWNED,	/* display buffer */
	PVR2D_MEMINFO_WRAPPED,			/* wrapped buffer */
	PVR2D_MEMINFO_MAPPED			/* mapped buffer */

}PVR2DMEMINFOTYPE;

/* 
	private buffer structure used to associate a 3rd party
	display class buffer handle with a PVR2D MemInfo
*/

typedef struct _PVR2D_BUFFER
{
	PVR2DMEMINFO sMemInfo;
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID  	hDisplayBuffer;
#else
	IMG_HANDLE	hDisplayBuffer;
#endif
	PVR2DMEMINFOTYPE eType;
}PVR2D_BUFFER;

/*
	Flip chain structure
*/
typedef struct _PVR2D_FLIPCHAIN
{
	DISPLAY_SURF_ATTRIBUTES sSrcSurfAttrib;
	DISPLAY_SURF_ATTRIBUTES sDstSurfAttrib;

	PVR2DRECT *psClipRects;
	IMG_UINT32 ui32NumClipRects;
	IMG_UINT32 ui32NumActiveClipRects;

	IMG_UINT32 ui32SwapInterval;

	PVR2D_BUFFER *psBuffer;
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID    hDCSwapChain;
#else
	IMG_HANDLE hDCSwapChain;
#endif
	IMG_UINT32 ui32NumBuffers;

	PVR2DCONTEXTHANDLE *hContext;	

	struct _PVR2D_FLIPCHAIN	*psNext;

}PVR2D_FLIPCHAIN;

#define	PRESENT_BLT_HEADER			0
#define	PRESENT_BLT_START			1
#define	PRESENT_BLT_SIZE			2
#define SIZEOF_PRESENT_BLT_BLOCK		3

#define	PRESENT_BLT_MISC_DST_HEADER 		0
#define	PRESENT_BLT_MISC_DST_ADDR		1
#define	PRESENT_BLT_MISC_SRC_HEADER		2
#define	PRESENT_BLT_MISC_SRC_ADDR		3
#define	PRESENT_BLT_MISC_SRC_OFF		4
#define	SIZEOF_PRESENT_BLT_MISC_STATE_BLOCK	5

#define SIZEOF_PRESENT_BLT_DATA		(SIZEOF_CLIPBLOCK + \
					 SIZEOF_PRESENT_BLT_BLOCK + \
					 SIZEOF_PRESENT_BLT_MISC_STATE_BLOCK)

/******************************************************************************
 @Strctured	  PVR2DCONTEXT
 
 @Description: context structure for PVR2D
******************************************************************************/
typedef struct _PVR2DCONTEXT
{
	PVRSRV_CONNECTION				*psServices;
	IMG_HANDLE						hDisplayClassDevice;
	PVRSRV_DEV_DATA					sDevData;
	DISPLAY_SURF_ATTRIBUTES			sPrimary;
	PVR2DMEMINFO					sPrimaryBuffer;
	DISPLAY_INFO					sDisplayInfo;
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID							hDevMemContext;

	/* Handle for 2D heap */
	IMG_SID							h2DHeap;
	IMG_DEV_VIRTADDR				s2DHeapBase;

	/* general mapping heap */
	IMG_SID							hGeneralMappingHeap;
#else
	IMG_HANDLE						hDevMemContext;

	/* Handle for 2D heap */
	IMG_HANDLE						h2DHeap;
	IMG_DEV_VIRTADDR				s2DHeapBase;

	/* general mapping heap */
	IMG_HANDLE						hGeneralMappingHeap;
#endif

	/* present blt params */
	IMG_UINT32						ui32NumClipBlocks;
	IMG_UINT32						ui32NumActiveClipBlocks;
	PVR2D_CLIPBLOCK					*psBltClipBlock;

	IMG_HANDLE						hTransferContext;
	PVR2D_ULONG						ulFlags;
#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID							hUSEMemHeap;
#else
	IMG_HANDLE						hUSEMemHeap;
#endif

#if defined(PVR2D_ALT_2DHW)
	/* Use alternative option for 2D if needed */
	/* present blt params */
	PVR2D_LONG						lPresentBltSrcStride;
	PVR2D_ULONG						ulPresentBltDstWidth;
	PVR2D_ULONG						ulPresentBltDstHeight;
	PVR2D_LONG						lPresentBltDstXPos;
	PVR2D_LONG						lPresentBltDstYPos;
	PVR2D_ULONG						ulPresentBltNumClipRects;
	PVR2DRECT						*pPresentBltClipRects;
	PVR2D_ULONG						ulPresentBltSwapInterval;

#else /* PVR2D_ALT_2DHW */
#if defined(SGX_FEATURE_2D_HARDWARE)

	/* misc state block blt command fragment */
	IMG_UINT32						aui32PresentBltMiscStateBlock[SIZEOF_PRESENT_BLT_MISC_STATE_BLOCK];

	/* blt block blt command fragment */
	IMG_UINT32						aui32PresentBltBlock[SIZEOF_PRESENT_BLT_BLOCK];
	
	/* combined HW blt command */
	IMG_UINT32						aui32PresentBltData[SIZEOF_PRESENT_BLT_DATA];
	
	/* 1555 Lookup alpha value */
	IMG_UINT32						ui32AlphaRegValue;
	
#endif /* SGX_FEATURE_2D_HARDWARE */
#endif /* PVR2D_ALT_2DHW */



	/* List of flip chains created on the context */
	PVR2D_FLIPCHAIN					*psFlipChain;
	
#ifdef PDUMP
	PVR2D_LONG						lBltCount;
	PVR2D_LONG						lPresentBltCount;
#endif

	PVRSRV_MISC_INFO				sMiscInfo;		/* contains the global event object */

	/* PSP2-specific */

	IMG_BOOL						bIsExtRegMemblock;

}PVR2DCONTEXT;

/* Internal rop macros */
#define PVR2DROP_NEEDPAT( ROp )  ((( (ROp) ^ ((ROp)<<4)) & (PVR2DROP3_PATMASK*0x0101)) != 0 )
#define PVR2DROP_NEEDSRC( ROp )  ((( (ROp) ^ ((ROp)<<2)) & (PVR2DROP3_SRCMASK*0x0101)) != 0 )
#define PVR2DROP_NEEDDST( ROp )  ((( (ROp) ^ ((ROp)<<1)) & (PVR2DROP3_DSTMASK*0x0101)) != 0 )
#define PVR2DROP_NEEDMASK( ROp ) ((( (ROp) ^ ((ROp)>>8)) &  PVR2DROP3_MASK           ) != 0 )

/* 1555 Alpha Lookup register defs, only valid for 2D Core */
#ifdef EUR_CR_2D_ALPHA_COMPONENT_ZERO_MASK
#define ALPHA_1555(A0,A1)				(((IMG_UINT32)A0 & EUR_CR_2D_ALPHA_COMPONENT_ZERO_MASK) | (((IMG_UINT32)A1 << EUR_CR_2D_ALPHA_COMPONENT_ONE_SHIFT) & EUR_CR_2D_ALPHA_COMPONENT_ONE_MASK))
#define DEFAULT_ALPHA1555_LOOKUP_0		0x00 /* 8-bit alpha when 1555 source alpha is 0 */
#define DEFAULT_ALPHA1555_LOOKUP_1		0xFF /* 8-bit alpha when 1555 source alpha is 1 */
#define DEFAULT_ALPHA1555				ALPHA_1555(DEFAULT_ALPHA1555_LOOKUP_0, DEFAULT_ALPHA1555_LOOKUP_1)
#endif

/* Internal prototypes */
void InitPresentBltData(PVR2DCONTEXT *psContext);
PVR2DERROR DestroyFlipChain(PVR2DCONTEXT *psContext, PVR2D_FLIPCHAIN *psFlipChain);
PVRSRV_PIXEL_FORMAT GetPvrSrvPixelFormat(const PVR2DFORMAT Format);
#if defined(TRANSFER_QUEUE)
PVR2DERROR ValidateTransferContext(PVR2DCONTEXT *psContext);
#endif

/* debug macros */
#if defined (DEBUG)
#define PVR2D_DPF PVR_DPF
#else /* DEBUG */
#define PVR2D_DPF(s)
#endif /* DEBUG */



#define PVR2DMalloc(X, Y)		PVRSRVAllocUserModeMem(Y)
#define PVR2DRealloc(X, Y, Z)		PVRSRVReallocUserModeMem(Y, Z)
#define PVR2DCalloc(X, Y)		PVRSRVCallocUserModeMem(Y)
#define PVR2DFree(X, Y)			PVRSRVFreeUserModeMem(Y)


/*
 * Initialise a PVR2DMEMINFO structure from a pointer to a
 * PVRSRV_CLIENT_MEM_INFO structure.
 */
#define	PVR2DMEMINFO_INITIALISE(d, s) \
{ \
	(d)->hPrivateData = (IMG_VOID *)(s); \
	(d)->hPrivateMapData = (IMG_VOID *)(s->hKernelMemInfo); \
	(d)->ui32DevAddr = (IMG_UINT32) (s)->sDevVAddr.uiAddr; \
	(d)->ui32MemSize = (s)->uAllocSize; \
	(d)->pBase = (s)->pvLinAddr;\
	(d)->ulFlags = (s)->ui32Flags;\
} 

/*
 * Obtain a pointer to a PVRSRV_CLIENT_MEM_INFO structure from a pointer
 * to a PVR2DMEMINFO structure.
 */
#define	CLIENT_MEM_INFO_FROM_PVR2DMEMINFO(m) \
	((PVRSRV_CLIENT_MEM_INFO *)((m)->hPrivateData))

#define	PVR2D_MMU_PAGE_SIZE	SGX_MMU_PAGE_SIZE
#define	PVR2D_MMU_PAGE_MASK	(SGX_MMU_PAGE_SIZE - 1)
#define	PVR2D_MMU_PAGE_SHIFT	SGX_MMU_PAGE_SHIFT


/* Common Prototypes */
IMG_UINT32 GetBpp(const PVR2DFORMAT Format);


#if defined (__cplusplus)
}
#endif

#endif /* PVR2D_INT_H */

/******************************************************************************
 End of file (pvr2dint.h)
******************************************************************************/
