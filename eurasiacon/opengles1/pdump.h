/**************************************************************************
 * Name         : pdump.h
 *
 * Copyright    : 2003-2006 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or other-wise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: pdump.h $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/
#ifndef _PDUMP_
#define _PDUMP_

#ifdef PDUMP
#define PDUMP_WRITEREG(gc, addr, val) PDumpReg(gc, addr, val)
#define PDUMP_POLL(gc, addr, val) PDumpPoll(gc, addr, val)
#define PDUMP_STRING(PARAMS) PDumpString PARAMS
#define PDUMP_STRING_CONTINUOUS(PARAMS) PDumpStringContinuous PARAMS
#define PDUMP_MEM(gc, meminfo, offset, size) PDumpMem(gc, meminfo, offset, size)
#define PDUMP_SETFRAME(gc) PDumpSetFrame(gc)
#define PDUMP_ISCAPTURING(gc) PDumpIsCapturing(gc)
#define PDUMP_SYNC(gc) PDumpSync(gc)

IMG_BOOL PDumpReg(GLES1Context *gc, IMG_UINT32 ui32Addr, IMG_UINT32 ui32Data);
IMG_BOOL PDumpPoll(GLES1Context *gc, IMG_UINT32 ui32Addr, IMG_UINT32 ui32Data);

IMG_BOOL PDumpString(GLES1Context *gc, IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);
IMG_BOOL PDumpStringContinuous(GLES1Context *gc, IMG_CHAR *pszFormat, ...) IMG_FORMAT_PRINTF(2, 3);

IMG_BOOL PDumpMem(GLES1Context *gc, PVRSRV_CLIENT_MEM_INFO *psMemInfo, IMG_UINT32 ui32Offset, IMG_UINT32 ui32Size);
IMG_VOID PDumpSetFrame(GLES1Context *gc);
IMG_BOOL PDumpIsCapturing(GLES1Context *gc);
IMG_BOOL PDumpSync(GLES1Context *gc, IMG_VOID *pvAltLinAddr, PVRSRV_CLIENT_SYNC_INFO *psSyncInfo, IMG_UINT32 ui32Offset, IMG_UINT32 ui32Bytes);

#else /* PDUMP */

#define PDUMP_WRITEREG(gc, addr, val)
#define PDUMP_POLL(gc, addr, val)
#define PDUMP_PRIM(gc)
#define PDUMP_STRING(PARAMS)
#define PDUMP_STRING_CONTINUOUS(PARAMS)
#define PDUMP_MEM(gc, meminfo, offset, size)
#define PDUMP_SETFRAME(gc)
#define PDUMP_ISCAPTURING(gc)
#define PDUMP_SYNC(gc)

#endif /* PDUMP */

#endif /* _PDUMP_ */

/*****************************************************************************
 End of file (pdump.h)
*****************************************************************************/

