/**************************************************************************
 * Name         : buffers.h
 *
 * Copyright    : 2006-2007 by Imagination Technologies Limited. All rights reserved.
 *              : No part of this software, either material or conceptual 
 *              : may be copied or distributed, transmitted, transcribed,
 *              : stored in a retrieval system or translated into any 
 *              : human or computer language in any form by any means,
 *              : electronic, mechanical, manual or other-wise, or 
 *              : disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Unit 8, HomePark
 *              : Industrial Estate, King's Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: buffers.h $
 **************************************************************************/
#ifndef _BUFFERS_
#define _BUFFERS_

#include "sgxdefs.h"
#include "services.h"

#if defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690)
#define CBUF_NUM_SECATTR_STATE_BLOCKS			SGX_FEATURE_NUM_USE_PIPES
#else
#define CBUF_NUM_SECATTR_STATE_BLOCKS			1
#endif /* defined(FIX_HW_BRN_23687) || defined(FIX_HW_BRN_23690) */

#define CBUF_MAX_DWORDS_PER_PDS_STATE_BLOCK			2
#define CBUF_MAX_DWORDS_PER_SECONDARY_STATE_BLOCK	(CBUF_MAX_DWORDS_PER_PDS_STATE_BLOCK * CBUF_NUM_SECATTR_STATE_BLOCKS)
#define CBUF_MAX_DWORDS_PER_INDEX_LIST_BLOCK		6
#define CBUF_MAX_DWORDS_PER_TERMINATE_BLOCK			1



/*
	Reserved space for each buffer for terminate.
	Include space for a secondary update to free any secondaries
*/
#define VDM_CTRL_TERMINATE_DWORDS	(CBUF_MAX_DWORDS_PER_TERMINATE_BLOCK + CBUF_MAX_DWORDS_PER_PDS_STATE_BLOCK + CBUF_MAX_DWORDS_PER_SECONDARY_STATE_BLOCK)



#define ROUNDUP_SIZE(SIZE, ALIGNSHIFTS) (SIZE + ((1 << ALIGNSHIFTS) - 1)) & ~((1 << ALIGNSHIFTS) - 1);

#define CBUF_TYPE_VDM_CTRL_BUFFER						0
#define CBUF_TYPE_VERTEX_DATA_BUFFER					1
#define CBUF_TYPE_INDEX_DATA_BUFFER						2
#define CBUF_TYPE_PDS_VERT_BUFFER						3
#define CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER		4
#define CBUF_TYPE_MTE_COPY_PREGEN_BUFFER				5
#define CBUF_TYPE_PDS_AUXILIARY_PREGEN_BUFFER			6

#define CBUF_NUM_TA_BUFFERS								7

#define CBUF_TYPE_PDS_FRAG_BUFFER						7
#define CBUF_TYPE_USSE_FRAG_BUFFER						8

#define CBUF_NUM_BUFFERS								9

typedef struct CircularBuffer_TAG
{
	PVRSRV_CLIENT_MEM_INFO		*psMemInfo;		/* MEM_INFO */

	IMG_UINT32                 *pui32BufferBase;			/* Linear base address */	
	IMG_UINT32                  ui32BufferLimitInBytes;		/* Total space in bytes */
	IMG_DEV_VIRTADDR            uDevVirtBase;				/* Device virtual base address */

	volatile IMG_UINT32			*pui32ReadOffset;				/* Read offset */
	IMG_UINT32					ui32ReadOffsetCopy;				/* Local copy of read offset */
	IMG_UINT32					ui32CurrentWriteOffsetInBytes;	/* Write offset */
	IMG_UINT32					ui32CommittedPrimOffsetInBytes;	/* Commited Prim Offset */
	IMG_UINT32					ui32CommittedHWOffsetInBytes;	/* Commited HW(TA/RENDER) offset */

	IMG_UINT32					ui32SingleKickLimitInBytes;

	IMG_DEV_VIRTADDR			uTACtrlKickDevAddr;		/* For TA ctrl stream only, 
														   the control stream start virtual base in the following TA kick */

	IMG_BOOL					bLocked;
	IMG_UINT32					ui32LockCount;

	PVRSRV_DEV_DATA				*psDevData;

	PVRSRV_CLIENT_MEM_INFO      *psStatusUpdateMemInfo; /* device memory for status update */

#if defined(PDUMP)
	IMG_BOOL					bSyncDumped;
#endif /* defined(PDUMP) */

#if defined(DEBUG) || defined(TIMING)
	IMG_UINT32					ui32KickCount;	/* How many times this buffer has caused a kick */
#endif /* defined(DEBUG) || defined(TIMING) */

} CircularBuffer;

CircularBuffer *CBUF_CreateBuffer(PVRSRV_DEV_DATA *ps3DDevData, 
									 IMG_UINT32   ui32BufferType,
									 IMG_HANDLE   hHeapAllocator,
									 IMG_HANDLE   hSyncInfoHeapAllocator,
									 IMG_HANDLE   hOSEvent,
									 IMG_UINT32   ui32Size,
									 IMG_SID	  hPerProcRef);
											 
IMG_VOID CBUF_DestroyBuffer(PVRSRV_DEV_DATA *ps3DDevData, CircularBuffer *psBuffer);

IMG_UINT32 *CBUF_GetBufferSpace(CircularBuffer **apsBuffers, IMG_UINT32 ui32DWordsRequired, IMG_UINT32 ui32BufferType, IMG_BOOL bTerminate);

IMG_VOID CBUF_UpdateBufferPos(CircularBuffer **apsBuffers, IMG_UINT32 ui32DWordsWritten, IMG_UINT32 ui32BufferType);


IMG_DEV_VIRTADDR CBUF_GetBufferDeviceAddress(CircularBuffer **apsBuffers,
											 const IMG_VOID *pvLinAddr, 
											 IMG_UINT32 ui32BufferType);

IMG_VOID CBUF_UpdateBufferCommittedPrimOffsets(CircularBuffer **apsBuffers, 
											   IMG_BOOL		  *pbPrimitivesSinceLastTA,
											   IMG_VOID		  *pvContext,
											   IMG_VOID		 (*pfnScheduleTA)(IMG_VOID *, IMG_BOOL));

#if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2)

IMG_VOID CBUF_UpdateVIBufferCommittedPrimOffsets(CircularBuffer **apsBuffers, 
												 IMG_BOOL *pbPrimitivesSinceLastTA,
												 IMG_VOID *pvContext,
												 IMG_VOID (*pfnScheduleTA)(IMG_VOID *, IMG_BOOL));

#endif /* defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) */


IMG_VOID CBUF_UpdateTACtrlKickBase(CircularBuffer **apsBuffers);

IMG_VOID CBUF_UpdateBufferCommittedHWOffsets(CircularBuffer **apsBuffers, IMG_BOOL bFinalFlush);


#if defined(PDUMP) || defined(DEBUG) || defined(TIMING)
extern const IMG_CHAR * const pszBufferNames[CBUF_NUM_BUFFERS];
#endif /* defined(PDUMP) || defined(DEBUG) || defined(TIMING) */

#if defined(PDUMP) 
IMG_UINT32 CBUF_GetBufferSpaceUsedInBytes(CircularBuffer *psBuffer);
#endif /* defined(PDUMP) */

#endif /* _BUFFERS_ */

