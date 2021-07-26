/*************************************************************************/ /*!
@File           srv.h
@Title          Service Interface API.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Service Interface API.
@License        Strictly Confidential.
*/ /**************************************************************************/

#ifndef _srv_h_
#define _srv_h_

#include "drvegl.h"

typedef struct
{
	EGLint iAlpha;
	EGLint iRed;
	EGLint iGreen;
	EGLint iBlue;
	EGLint iWidth;
}
SRVPixelFormat;

extern const SRVPixelFormat gasSRVPixelFormat[];

IMG_BOOL SRV_ServicesInit(SrvSysContext *psSysContext, IMGEGLAppHints *psAppHints, IMG_FLOAT fCPUSpeed);

IMG_BOOL SRV_ServicesDeInit(SrvSysContext *psSysContext);

IMG_BOOL SRV_CreateSurface(SrvSysContext *psSysContext, KEGL_SURFACE *psSurface);
IMG_BOOL SRV_DestroySurface(SrvSysContext *psSysContext, KEGL_SURFACE *psSurface);

#if defined(SUPPORT_SGX)
/************************************************************************************
 SGX prototypes
************************************************************************************/

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
IMG_INTERNAL IMG_BOOL KEGL_SGXHibernateRenderSurface(SrvSysContext		*psSysContext,
										EGLRenderSurface	*psSurface);
IMG_INTERNAL IMG_BOOL KEGL_SGXAwakeRenderSurface(SrvSysContext		*psSysContext,
									EGLDrawableParams *psParams,
									EGLRenderSurface	*psSurface);
#endif /* defined(EGL_EXTENSION_IMG_EGL_HIBERNATION) */
IMG_BOOL KEGL_SGXResizeRenderSurface(SrvSysContext		*psSysContext,
									 EGLDrawableParams *psParams,
									 IMG_BOOL			bMultiSample,
									 IMG_BOOL			bCreateZSBuffer,
									 EGLRenderSurface	*psSurface);
IMG_BOOL KEGL_SGXDestroyRenderSurface(SrvSysContext *psSysContext, EGLRenderSurface *psSurface);
IMG_BOOL KEGL_SGXCreateRenderSurface(SrvSysContext *psSysContext,
									 EGLDrawableParams *psParams,
									 IMG_BOOL bMultiSample,
									 IMG_BOOL bAllocTwoRT,
									 IMG_BOOL bCreateZSBuffer,
									 EGLRenderSurface *psSurface);
IMG_BOOL SRV_SGXServicesDeInit(SrvSysContext *psSysContext);
IMG_BOOL SRV_SGXServicesInit(SrvSysContext *psSysContext, IMGEGLAppHints *psAppHints);
IMG_RESULT SGXAllocatePBufferDeviceMem(SrvSysContext		*psSysContext,
									   KEGL_SURFACE			*psSurface,
									   EGLint				pbuffer_width,
									   EGLint				pbuffer_height,
									   EGLint				pixel_width,
									   PVRSRV_PIXEL_FORMAT	pixel_format,
									   IMG_UINT32			*pui32Stride);

#if defined(SUPPORT_SGX_LOW_LATENCY_SCHEDULING)

IMG_BOOL KEGL_SGXSetContextPriority(SrvSysContext *psContext, SGX_CONTEXT_PRIORITY *peContextPriority);

#endif
#endif

#if defined(SUPPORT_VGX)
/************************************************************************************
 VGX prototypes
************************************************************************************/
IMG_BOOL KEGL_VGXResizeRenderSurface(SrvSysContext		*psSysContext,
									 EGLDrawableParams *psParams,
									 IMG_BOOL			bMultiSample,
									 IMG_BOOL			bCreateZSBuffer,
									 EGLRenderSurface	*psSurface);
IMG_BOOL KEGL_VGXDestroyRenderSurface(SrvSysContext *psSysContext, EGLRenderSurface *psSurface);
IMG_BOOL KEGL_VGXCreateRenderSurface(SrvSysContext *psSysContext,
									 EGLDrawableParams *psParams,
									 IMG_BOOL bMultiSample,
									 IMG_BOOL bAllocTwoRT,
									 IMG_BOOL bCreateZSBuffer,
									 EGLRenderSurface *psSurface);
IMG_BOOL SRV_VGXServicesDeInit(SrvSysContext *psSysContext);
IMG_BOOL SRV_VGXServicesInit(SrvSysContext *psSysContext, IMGEGLAppHints *psAppHints);
IMG_RESULT VGXAllocatePBufferDeviceMem(SrvSysContext		*psSysContext,
									   KEGL_SURFACE			*psSurface,
									   EGLint				pbuffer_width,
									   EGLint				pbuffer_height,
									   EGLint				pixel_width,
									   PVRSRV_PIXEL_FORMAT	pixel_format,
									   IMG_UINT32			*pui32Stride);
#endif

#endif
