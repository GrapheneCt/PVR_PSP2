/******************************************************************************
 * Name         : osglue.h
 *
 * Copyright    : 2005-2009 by Imagination Technologies Limited.
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
 * $Log: osglue.h $
 *****************************************************************************/

#ifndef _OSGLUE_
#define _OSGLUE_

/******************************************************************************
   Memory manipulation functions
******************************************************************************/
#if ((defined(__linux__) || defined(__psp2__)) && !defined(DEBUG))
	#include <string.h>

	#define	GLES2MemCopy(X,Y,Z)		memcpy(X, Y, Z)
	#define	GLES2MemSet(X,Y,Z)		memset(X, Y, Z)
#else
	#define	GLES2MemCopy(X,Y,Z)		PVRSRVMemCopy(X, Y, Z)
	#define	GLES2MemSet(X,Y,Z)		PVRSRVMemSet(X, Y, Z)
#endif


/******************************************************************************
	Allocation functions
******************************************************************************/
#if defined(__linux__) || defined(__psp2__)

	#if defined(DEBUG) && !defined(__psp2__)

		extern IMG_EXPORT IMG_PVOID PVRSRVAllocUserModeMemTracking(IMG_UINT32 ui32Size, IMG_CHAR *pszFileName, IMG_UINT32 ui32LineNumber);
		extern IMG_EXPORT IMG_PVOID PVRSRVCallocUserModeMemTracking(IMG_UINT32 ui32Size, IMG_CHAR *pszFileName, IMG_UINT32 ui32LineNumber);
		extern IMG_EXPORT IMG_VOID  PVRSRVFreeUserModeMemTracking(IMG_VOID *pvMem);
		extern IMG_EXPORT IMG_PVOID PVRSRVReallocUserModeMemTracking(IMG_VOID *pvMem, IMG_UINT32 ui32NewSize, IMG_CHAR *pszFileName, IMG_UINT32 ui32LineNumber);

		#define GLES2Malloc(X,Y)	(IMG_VOID*)PVRSRVAllocUserModeMemTracking(Y, __FILE__, __LINE__)
		#define GLES2Calloc(X,Y)	(IMG_VOID*)PVRSRVCallocUserModeMemTracking(Y, __FILE__, __LINE__)
		#define GLES2Realloc(X,Y,Z)	(IMG_VOID*)PVRSRVReallocUserModeMemTracking(Y, Z, __FILE__, __LINE__)
		#define GLES2Free(X,Y)				   PVRSRVFreeUserModeMemTracking(Y)

	#else /* DEBUG */

		#define GLES2Malloc(X,Y)	(IMG_VOID*)malloc(Y)
		#define GLES2Calloc(X,Y)	(IMG_VOID*)calloc(1, Y)
		#define GLES2Realloc(X,Y,Z)	(IMG_VOID*)realloc(Y, Z)

	#endif /* DEBUG */

#else

	#define GLES2Malloc(X,Y)	(IMG_VOID*)PVRSRVAllocUserModeMem(Y)
	#define GLES2Calloc(X,Y) 	(IMG_VOID*)PVRSRVCallocUserModeMem(Y)
	#define GLES2Realloc(X,Y,Z)	(IMG_VOID*)PVRSRVReallocUserModeMem(Y, Z)
	#define GLES2Free(X,Y)				   PVRSRVFreeUserModeMem(Y)

#endif

/******************************************************************************
	Other functions
******************************************************************************/
IMG_INTERNAL IMG_VOID GLES2Sleep(IMG_UINT32 ui32MilliSeconds);


#endif /* _OSGLUE_ */

/******************************************************************************
 End of file (osglue.h)
******************************************************************************/