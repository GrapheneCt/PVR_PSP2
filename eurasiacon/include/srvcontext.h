/*************************************************************************/ /*!
@File           srvcontext.h
@Title          Service interface implementation detail data structure
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Service interface implementation detail data structure
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _SRVCONTEXT_H_
#define _SRVCONTEXT_H_

#include <kernel.h>

#include "services.h"
#if defined(SUPPORT_SGX)
#include "sgxapi.h"
#endif
#if defined(SUPPORT_VGX)
#include "vgxapi.h"
#endif

#ifndef EGL_DEFAULT_PARAMETER_BUFFER_SIZE
#if defined(FORCE_ENABLE_GROW_SHRINK) && defined(SUPPORT_PERCONTEXT_PB)
#define EGL_DEFAULT_PARAMETER_BUFFER_SIZE (1 * 1024 * 1024)
#else
#define EGL_DEFAULT_PARAMETER_BUFFER_SIZE (4 * 1024 * 1024)
#endif
#endif

#ifndef	EGL_DEFAULT_MAX_PARAMETER_BUFFER_SIZE
#define EGL_DEFAULT_MAX_PARAMETER_BUFFER_SIZE	(4 * 1024 * 1024)
#endif

#define EGL_DEFAULT_PDS_FRAG_BUFFER_SIZE	(50 * 1024)


#if defined(DEBUG)
typedef struct DevMemllocation_TAG
{
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;
	PVRSRV_DEV_DATA	       *psDevData;
	IMG_UINT32             u32Line;
	const IMG_CHAR         *pszFile;

} DevMemAllocation;
#endif /* defined(DEBUG) */

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
struct EGLRenderSurfaceTAG;
#endif

typedef struct SrvSysContext_TAG
{
	PVRSRV_CONNECTION 		*psConnection;
	PVRSRV_DEV_DATA 		s3D;
#if defined(SUPPORT_SGX)
	PVRSRV_SGX_CLIENT_INFO 	sHWInfo;
#endif
#if defined(SUPPORT_VGX)
	PVRSRV_VGX_CLIENT_INFO 	sVGXHWInfo;
#endif
	IMG_HANDLE hIMGEGLAppHints;

	IMG_HANDLE hDevMemContext;

#if defined(SUPPORT_SGX)
	IMG_HANDLE hUSEFragmentHeap;
	IMG_HANDLE hUSEVertexHeap;
	IMG_HANDLE hPDSFragmentHeap;
	IMG_HANDLE hPDSVertexHeap;
	IMG_HANDLE hGeneralHeap;
	IMG_HANDLE hSyncInfoHeap;
	IMG_DEV_VIRTADDR uUSEFragmentHeapBase;
	IMG_DEV_VIRTADDR uUSEVertexHeapBase;
#endif

#if defined(SUPPORT_VGX)
	IMG_HANDLE	hGeneralHeap;
#endif

#if defined (SUPPORT_OPENGL)
	PVRSRV_HEAP_INFO asSharedHeapInfo[PVRSRV_MAX_CLIENT_HEAPS];
	IMG_UINT32 ui32ClientHeapCount;
#endif

	IMG_HANDLE hRenderContext;
	PVRSRV_CLIENT_MEM_INFO *psVisTestResults;

#if defined(NEW_TRANSFER_QUEUE)
	IMG_HANDLE hTransferLoadQueue;
#endif

#if defined(TRANSFER_QUEUE)   
	IMG_HANDLE hTransferContext;
#endif

#if defined(DEBUG)
	/* Track device memory allocations */
	IMG_UINT32 ui32CurrentAllocations, ui32MaxAllocations;

	DevMemAllocation *psDevMemAllocations;
#endif /* defined(DEBUG) */

#if defined(TIMING) || defined(DEBUG)
	IMG_FLOAT		fCPUSpeed;
#endif /* defined(TIMING) || defined(DEBUG) */

	SGX_CREATERENDERCONTEXT		sRenderContext;

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
	struct EGLRenderSurfaceTAG	*psHeadSurface;
	IMG_BOOL					bHibernated;
#endif

	/* PSP2-specific */

	SceUID						hDriverMemUID;
	SceUID						h3DParamBufMemUID;
	IMG_SID						hPerProcRef;

} SrvSysContext;


#endif /* _SRVCONTEXT_H_ */

/******************************************************************************
 End of file (srvcontext.h)
******************************************************************************/
