/******************************************************************************
 * Name         : egl_sync.h
 *
 * Copyright    : 2009 by Imagination Technologies Limited.
 *                All rights reserved. No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system or
 *                translated into any human or computer language in any form
 *                by any means, electronic, mechanical, manual or otherwise,
 *                or disclosed to third parties without the express written
 *                permission of Imagination Technologies Limited, Home Park
 *                Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * $Log: egl_sync.h $
 *****************************************************************************/

#ifndef _EGL_SYNC_H_
#define _EGL_SYNC_H_

#if defined(__cplusplus)
extern "C" {
#endif

/* KEGL_SYNC defined in egl_internal.h */
struct _KEGL_SYNC_
{
	KEGL_SYNC                   *psNextSync;
	KEGL_DISPLAY                *psDpy;
	KEGL_CONTEXT                *psContext; /* context in which sync object was created */

	EGLenum                     eglSyncType;
	EGLenum                     eglSyncStatus;
	EGLenum                     eglSyncCondition;

	PVRSRV_CLIENT_MEM_INFO		*psSyncMemInfo;
	SGX_QUEUETRANSFER			sQueueTransfer;
	IMG_UINT32                  ui32WaitValue;

	EGLint                      refcount;
	EGLBoolean                  bIsDeleting;
	EGLBoolean                  bIsGhosted;
	
#if defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)
	PVRSRV_SEMAPHORE_HANDLE     hReusableSemaphore;
	IMG_UINT64                  ui64WaitLevel;          /* Used to protect unwanted signals. It is ~584.9 years before this wraps even with signals every nano-second. */
#endif /* defined(EGL_EXTENSION_KHR_REUSABLE_SYNC) */
};

IMG_VOID TerminateSyncs(KEGL_DISPLAY * psDpy, SrvSysContext *psSysContext);

EGLSyncKHR IMGeglCreateSyncKHR(EGLDisplay eglDpy, EGLenum type, const EGLint *piAttribList);

EGLBoolean IMGeglDestroySyncKHR(EGLDisplay eglDpy, EGLSyncKHR sync);

EGLint IMGeglClientWaitSyncKHR(EGLDisplay eglDpy, EGLSyncKHR *sync, EGLint flags, EGLTimeKHR timeout);

EGLBoolean IMGeglGetSyncAttribKHR(EGLDisplay eglDpy, EGLSyncKHR sync, EGLint attribute, EGLint *pValue);

#if defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)
EGLBoolean IMGeglSignalSyncKHR(EGLDisplay eglDpy, EGLSyncKHR sync, EGLenum mode);
#endif /* defined(EGL_EXTENSION_KHR_REUSABLE_SYNC) */

#if defined(__cplusplus)
}
#endif

#endif /*_EGL_SYNC_H_*/

/******************************************************************************
 End of file (egl_sync.h)
******************************************************************************/
