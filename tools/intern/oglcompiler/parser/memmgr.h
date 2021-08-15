/******************************************************************************
 * Name         : memmgr.h
 * Author       : James McCarthy
 * Created      : 23/03/2005
 *
 * Copyright    : 2000-2007 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Home Park
 *              : Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: memmgr.h $
 *****************************************************************************/

#ifndef __gl_memmgr_h_
#define __gl_memmgr_h_

#include "img_types.h"

#undef USE_MEMMGR_ALLOC_TRACKING

#if defined(STANDALONE)

	/* We can safely include malloc since standalone build are Windows-only (possibly Linux in the future) */

#  include <malloc.h>
#  include <locale.h>

#  if defined(DEBUG)

	/* For standalone debug builds we use memory tracking to ease debugging */

#    define USE_MEMMGR_ALLOC_TRACKING 1

	IMG_VOID *DebugMemAllocfn(IMG_UINT32  uSizeInBytes,
						      IMG_UINT32  uLineNumber,
						      const IMG_CHAR *pszFileName,
						      IMG_BOOL  bInitToZero,
						      IMG_VOID *pvRealloc);

	IMG_VOID DebugMemFreefn(IMG_VOID *MemToFree, IMG_UINT32 uLineNumber, const IMG_CHAR *pszFileName);

	IMG_VOID DisplayUnfreedMem(IMG_VOID);

#    define DebugMemAlloc(a)          DebugMemAllocfn(a, __LINE__, __FILE__, IMG_FALSE, IMG_NULL)
#    define DebugMemCalloc(a)         DebugMemAllocfn(a, __LINE__, __FILE__, IMG_TRUE,  IMG_NULL)
#    define DebugMemRealloc(a,b)      DebugMemAllocfn(b, __LINE__, __FILE__, IMG_FALSE, a)

#    define DebugMemAlloc2(a,b,c)     DebugMemAllocfn(a, b, c, IMG_FALSE, IMG_NULL)
#    define DebugMemCalloc2(a,b,c)    DebugMemAllocfn(a, b, c, IMG_TRUE,  IMG_NULL)
#    define DebugMemRealloc2(a,b,c,d) DebugMemAllocfn(b, c, d, IMG_FALSE, a)

#    define DebugMemFree(a)           DebugMemFreefn(a, __LINE__, __FILE__)
#    define DebugMemFree2(a,b,c)      DebugMemFreefn(a, b, c)

#    define DebugMemAllocNoTrack(a)	  malloc(a)
#    define DebugMemFreeNoTrack(a)	  free(a)

#  else /* !defined DEBUG */

    /* For standalone release builds we use the native malloc/free */

#    define DebugMemAlloc(a)          malloc(a)
#    define DebugMemCalloc(a)         calloc(a, 1)
#    define DebugMemRealloc(a,b)      realloc(a,b)

#    define DebugMemAlloc2(a,b,c)     malloc(a)
#    define DebugMemCalloc2(a,b,c)    calloc(a, 1)
#    define DebugMemRealloc2(a,b,c,d) realloc(a,b)

#    define DebugMemFree(a)           free(a)
#    define DebugMemFree2(a,b,c)      free(a)

#    define DebugMemAllocNoTrack(a)	  malloc(a)
#    define DebugMemFreeNoTrack(a)	  free(a)

#    define DisplayUnfreedMem()

#  endif /* !defined DEBUG */

#    define GLSLSetLocale(a)          setlocale(LC_ALL,(a))


#else /* !defined STANDALONE */

    /* For non-standalone builds we always use the services */

#  include "services.h"


#if defined(__linux__) || defined(__psp2__)
#if defined(DEBUG) && !defined(__psp2__)

	#define DebugMemAlloc(X)			PVRSRVAllocUserModeMemTracking(X, __FILE__, __LINE__)
	#define DebugMemCalloc(X)			PVRSRVCallocUserModeMemTracking(X, __FILE__, __LINE__)
	#define DebugMemRealloc(X,Y)		PVRSRVReallocUserModeMemTracking(X, Y, __FILE__, __LINE__)

	#define DebugMemAlloc2(X,Y,Z)		PVRSRVAllocUserModeMemTracking(X, __FILE__, __LINE__)
	#define DebugMemCalloc2(X,Y,Z)		PVRSRVCallocUserModeMemTracking(X, __FILE__, __LINE__)
	#define DebugMemRealloc2(X,Y,Z,A)	PVRSRVReallocUserModeMemTracking(X, Y, __FILE__, __LINE__)

	#define DebugMemFree(X)				PVRSRVFreeUserModeMemTracking(X)
	#define DebugMemFree2(X,Y,Z)		PVRSRVFreeUserModeMemTracking(X)

	#define DebugMemAllocNoTrack(X)		malloc(X)
	#define DebugMemFreeNoTrack(X)		free(X)

#else
	#define DebugMemAlloc(X)			malloc(X)
	#define DebugMemCalloc(X)			calloc(1,X)
	#define DebugMemRealloc(X,Y)		realloc(X, Y)

	#define DebugMemAlloc2(X,Y,Z)		malloc(X)
	#define DebugMemCalloc2(X,Y,Z)		calloc(1,X)
	#define DebugMemRealloc2(X,Y,Z,A)	realloc(X, Y)

	#define DebugMemFree(X)				free(X)
	#define DebugMemFree2(X,Y,Z)		free(X)

	#define DebugMemAllocNoTrack(X)		malloc(X)
	#define DebugMemFreeNoTrack(X)		free(X)

#endif

#else /* defined(__linux__) */

	#define DebugMemAlloc(a)            PVRSRVAllocUserModeMem(a)
	#define DebugMemCalloc(a)           PVRSRVCallocUserModeMem(a)
	#define DebugMemRealloc(a,b)        PVRSRVReallocUserModeMem(a,b)

	#define DebugMemAlloc2(a,b,c)       PVRSRVAllocUserModeMem(a) 
	#define DebugMemCalloc2(a,b,c)      PVRSRVCallocUserModeMem(a)
	#define DebugMemRealloc2(a,b,c,d)   PVRSRVReallocUserModeMem(a,b)

	#define DebugMemFree(a)             PVRSRVFreeUserModeMem(a)
	#define DebugMemFree2(a,b,c)        PVRSRVFreeUserModeMem(a)

	#define DebugMemAllocNoTrack(a)		PVRSRVAllocUserModeMem(a)
	#define DebugMemFreeNoTrack(a)		PVRSRVFreeUserModeMem(a)

#endif /* defined(__linux__) */


#  define DisplayUnfreedMem()

#  define GLSLSetLocale(a)			  PVRSRVSetLocale((a))


#endif

#ifdef DUMP_LOGFILES
IMG_VOID DebugDumpMemoryAllocLog(IMG_VOID);
#endif


/* Memory pool for same-sized objects. Simple segregated storage. */
typedef struct MemHeapTAG
{
	IMG_UINT32   uHeapItemSizeInBytes;
	IMG_UINT32   uHeapSizeInBytes;
	IMG_BYTE    *pbHeap;
	IMG_BYTE    *pbEndOfHeap;
	IMG_BYTE    *pbCurrentWaterMark;

	IMG_VOID    *pvFreeListHead;

#if defined(DUMP_LOGFILES) || defined(DEBUG)
	IMG_CHAR    *pszHeapCreationInfo;
	IMG_UINT32   uHeapOverflows;
#endif
} MemHeap;

MemHeap *DebugCreateHeapfn(IMG_UINT32 uItemSizeInBytes, IMG_UINT32 uNumHeapItems
#if defined(DEBUG) || defined(DUMP_LOGFILES)
						   , IMG_UINT32 uLineNumber, const IMG_CHAR *pszFileName
#endif
						   );

IMG_VOID DebugDestroyHeapfn(MemHeap *psMemHeap
#if defined(STANDALONE) && defined(DEBUG)
							, IMG_UINT32 uLineNumber, const IMG_CHAR *pszFileName
#endif
							);

IMG_VOID *DebugAllocHeapItemfn(MemHeap *psMemHeap
#if defined(STANDALONE) && defined(DEBUG)
							   , IMG_UINT32 uLineNumber, const IMG_CHAR *pszFileName
#endif
							   );

IMG_VOID DebugFreeHeapItemfn(MemHeap *psMemHeap, IMG_VOID *pvItem);


#if defined(DEBUG) || defined(DUMP_LOGFILES)
#  define DebugCreateHeap(a,b)      DebugCreateHeapfn(a, b, __LINE__, __FILE__)
#else
#  define DebugCreateHeap(a,b)      DebugCreateHeapfn(a, b)
#endif

#if defined(STANDALONE) && defined(DEBUG)
#  define DebugDestroyHeap(a)       DebugDestroyHeapfn(a, __LINE__, __FILE__)
#  define DebugAllocHeapItem(a)     DebugAllocHeapItemfn(a, __LINE__, __FILE__)
#  define DebugFreeHeapItem(a, b)   DebugFreeHeapItemfn(a, b)
#else
#  define DebugDestroyHeap(a)       DebugDestroyHeapfn(a)
#  define DebugAllocHeapItem(a)     DebugAllocHeapItemfn(a)
#  define DebugFreeHeapItem(a, b)   DebugFreeHeapItemfn(a, b)
#endif

/* Use these for speed comparisons */
/*
#define DebugCreateHeap(a,b) ((MemHeap*)a)
#define DebugDestroyHeap(a)    (0)
#define DebugAllocHeapItem(a)  DebugMemAlloc((IMG_INT32)a)
#define DebugFreeHeapItem(a,b) DebugMemFree(b)
*/

IMG_VOID DebugDisplayMemoryStats(IMG_VOID);

#endif //__gl_memmgr_h_

/******************************************************************************
 End of file (memmgr.h)
******************************************************************************/
