/******************************************************************************
 * Name         : blobcache.c
 *
 * Copyright    : 2011 by Imagination Technologies Limited.
 *                All rights reserved. No part of this software, either
 *                material or conceptual may be copied or distributed,
 *                transmitted, transcribed, stored in a retrieval system or
 *                translated into any human or computer language in any form
 *                by any means, electronic, mechanical, manual or otherwise,
 *                or disclosed to third parties without the express written
 *                permission of Imagination Technologies Limited, Home Park
 *                Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *****************************************************************************/

#include "egl_internal.h"
#include "tls.h"
#include "generic_ws.h"
#include "blobcache.h"

#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE) 


IMG_EXPORT IMG_VOID IMG_CALLCONV KEGLSetBlob(const IMG_VOID* pvKey, IMG_UINT32 ui32KeySize, const IMG_VOID * pvBlob, IMG_UINT32 ui32BlobSize)
{
	TLS psTls = IMGEGLGetTLSValue();
	if (psTls==IMG_NULL)
	{
		PVR_DPF((PVR_DBG_WARNING, "KEGLSetBlob: EGL not initialised"));
		return;
	}
		
	EGLThreadLock(psTls);

	if(psTls->psGlobalData->pfnSetBlob)
	{
		psTls->psGlobalData->pfnSetBlob(pvKey, ui32KeySize, pvBlob, ui32BlobSize);
	}

	EGLThreadUnlock(psTls);
}

IMG_EXPORT IMG_UINT32 IMG_CALLCONV KEGLGetBlob(const IMG_VOID* pvKey, IMG_UINT32 ui32KeySize, IMG_VOID * pvBlob, IMG_UINT32 ui32BlobSize)
{
	IMG_UINT32 ui32ReturnBlobSize = 0;
	TLS psTls = IMGEGLGetTLSValue();

	if (psTls==IMG_NULL)
	{
		PVR_DPF((PVR_DBG_WARNING, "KEGLGetBlob: EGL not initialised"));
		return 0;
	}

	EGLThreadLock(psTls);

	if(psTls->psGlobalData->pfnGetBlob)
	{
		ui32ReturnBlobSize = psTls->psGlobalData->pfnGetBlob(pvKey, ui32KeySize, pvBlob, ui32BlobSize);
	}

	EGLThreadUnlock(psTls);

	return ui32ReturnBlobSize;
}

/***********************************************************************************
 Function Name      : IMGeglSetBlobCacheFuncsANDROID
 Inputs             : eglDpy, set, get
 Outputs            : 
 Returns            : 
 Description        : Set caching functions
************************************************************************************/
IMG_INTERNAL void IMGeglSetBlobCacheFuncsANDROID(	EGLDisplay eglDpy, 
														EGLSetBlobFunc set, 
														EGLGetBlobFunc get)
{
	KEGL_DISPLAY *psDpy;
	TLS psTls;
	
	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglSetBlobCacheFuncsANDROID"));

	psTls = TLS_Open(_TlsInit);
	
	if (psTls==IMG_NULL)
	{
		return;
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglSetBlobCacheFuncsANDROID);

	psDpy = GetKEGLDisplay(psTls, eglDpy);

	if (!psDpy)
	{
		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglSetBlobCacheFuncsANDROID);

		return;
	}

	EGLThreadLock(psTls);

	psTls->psGlobalData->pfnSetBlob = set;
	psTls->psGlobalData->pfnGetBlob = get;

	EGLThreadUnlock(psTls);

	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglSetBlobCacheFuncsANDROID);

	return;
}

#endif /* EGL_EXTENSION_ANDROID_BLOB_CACHE */

/******************************************************************************
 End of file (blobcache.c)
******************************************************************************/
