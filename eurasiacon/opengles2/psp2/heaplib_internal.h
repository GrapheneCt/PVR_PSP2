/*                 -*- mode: c; tab-width: 4; indent-tabs-mode: t; -*- */
/* SCE CONFIDENTIAL
 PlayStation(R)Vita Programmer Tool Runtime Library Release 03.570.011
 *
 *      Copyright (C) 2011 Sony Computer Entertainment Inc.
 *                        All Rights Reserved.
 *
 */
/*
 * 
 * 
 * 
 * 
 * 
 *
 *                        Heap Library
 *
 *                      heaplib_internal.h
 *
 *       Version        Date            Design      Log
 *  --------------------------------------------------------------------
 *       0.00           2011-01-15      kono        import from PSP library
 */

#ifndef _HEAPLIB_INTERNAL_H
#define _HEAPLIB_INTERNAL_H

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
extern "C" {
#endif	/* defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus) */

#define USE_HEAPINFO			1
#define USE_SPOILMEMORY_AT_FREE	1


/* ---- user level functions ---- */

typedef struct SceHeapMspaceLink {
	struct SceHeapMspaceLink *next;
	struct SceHeapMspaceLink *prev;

	SceUID	uid;
	SceSize	size;
	SceClibMspace msp;
} SceHeapMspaceLink;

static __inline__ int _sceHeapIsPointerInBound(const SceHeapMspaceLink *mp, const void *ptr)
{
	if (((const void *)(mp + 1) <= ptr) && (ptr < (const void *)(((const char *)(mp + 1)) + mp->size))) {
		return (1);
	}
	return (0);
}

typedef struct SceHeapWorkInternal {
	SceUIntPtr	magic;				/* == (SceUIntPtr)(&Eheap+1) */
	int          bsize;				/* extended block size */

	SceKernelLwMutexWork lwmtx;		/*J lwmutexのためのワークエリア */

	char         name[SCE_UID_NAMELEN + 1];
#if USE_HEAPINFO
	struct {
		int arena;					/*J カーネルから割り当てた総メモリサイズ */
		int ordblks;				/*J 標準ブロックの数                     */
		int hblks;					/*J 保持ブロック数                       */
	} info;
#endif	/* USE_HEAPINFO */

	SceHeapMspaceLink	prim;
	unsigned int		memblockType;
	PVRSRV_DEV_DATA *psDevData;
	IMG_SID hDevMemContext;
} SceHeapWorkInternal;

#define SCE_HEAP_OFFSET_TO_VALID_HEAP	768				// FIXME:
#define SCE_HEAP_MSPACE_LINK_OVERHEAD	720

#if defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus)
}
#endif	/* defined(_LANGUAGE_C_PLUS_PLUS)||defined(__cplusplus)||defined(c_plusplus) */

#endif /* _HEAPLIB_INTERNAL_H */

/* Local variables: */
/* tab-width: 4 */
/* End: */
/* vi:set tabstop=4: */
