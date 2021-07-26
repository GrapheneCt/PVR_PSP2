/*************************************************************************/ /*!
@Title          Thread-local storage: Common interface definition.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Strictly Confidential.
*/ /**************************************************************************/
#ifndef __COMMON_TLS_H_
#define __COMMON_TLS_H_

#if defined (__cplusplus)
extern "C" {
#endif

#include "img_types.h"

/* Note: instead of having lots of nested #ifdefs here we are relying on the
 * compiler generating an obvious warning/error if TLS_PREFIX gets re-defined.
 * We could manually generate the error if we know of any toolchains that won't
 * complain. 
 */

/* Note: Please remember to update all the #pragma inlines if you change the
 * API */

#if defined(OGLES2_MODULE)
    #define TLS_PREFIX(name) OGLES2##name
	#if defined(INLINE_IS_PRAGMA)
		#pragma inline(OGLES2GetTLSValue)
		#pragma inline(OGLES2SetTLSValue)
		#if defined(DISABLE_THREADS)
		#pragma inline(OGLES2_GetTLSID)
		#pragma inline(OGLES2_GetTLSValue)
		#pragma inline(OGLES2_SetTLSValue)
		#endif
	#endif
#endif
#if defined(OGLES1_MODULE) 
	#define TLS_PREFIX(name) OGL##name
	#if defined(INLINE_IS_PRAGMA)
		#pragma inline(OGLES1GetTLSValue)
		#pragma inline(OGLES1SetTLSValue)
		#if defined(DISABLE_THREADS)
		#pragma inline(OGLES1_GetTLSID)
		#pragma inline(OGLES1_GetTLSValue)
		#pragma inline(OGLES1_SetTLSValue)
		#endif
	#endif
#endif
#if defined(IMGEGL_MODULE)
	#define TLS_PREFIX(name) IMGEGL##name
	#if defined(INLINE_IS_PRAGMA)
		#pragma inline(IMGEGLGetTLSValue)
		#pragma inline(IMGEGLSetTLSValue)
		#if defined(DISABLE_THREADS)
		#pragma inline(IMGEGL_GetTLSID)
		#pragma inline(IMGEGL_GetTLSValue)
		#pragma inline(IMGEGL_SetTLSValue)
		#endif
	#endif
#endif
#if defined(WSEGL_MODULE)
	#define TLS_PREFIX(name) WSEGL##name
	#if defined(INLINE_IS_PRAGMA)
		#pragma inline(WSEGLGetTLSValue)
		#pragma inline(WSEGLSetTLSValue)
		#if defined(DISABLE_THREADS)
		#pragma inline(WSEGL_GetTLSID)
		#pragma inline(WSEGL_GetTLSValue)
		#pragma inline(WSEGL_SetTLSValue)
		#endif
	#endif
#endif
#if defined(OGLDESK_MODULE)
	#define TLS_PREFIX(name) OGLDESK##name
	#if defined(INLINE_IS_PRAGMA)
		#pragma inline(OGLDESKGetTLSValue)
		#pragma inline(OGLDESKSetTLSValue)
		#if defined(DISABLE_THREADS)
		#pragma inline(OGLDESK_GetTLSID)
		#pragma inline(OGLDESK_GetTLSValue)
		#pragma inline(OGLDESK_SetTLSValue)
		#endif
	#endif
#endif
#if defined(OPENVG_MODULE)
	#define TLS_PREFIX(name) OVG##name
	#if defined(INLINE_IS_PRAGMA)
		#pragma inline(OVGGetTLSValue)
		#pragma inline(OVGSetTLSValue)
		#if defined(DISABLE_THREADS)
		#pragma inline(OVG_GetTLSID)
		#pragma inline(OVG_GetTLSValue)
		#pragma inline(OVG_SetTLSValue)
		#endif
	#endif
#endif
#if defined(OPENCL_MODULE)
	#define TLS_PREFIX(name) OCL##name
	#if defined(INLINE_IS_PRAGMA)
		#pragma inline(OCLGetTLSValue)
		#pragma inline(OCLSetTLSValue)
		#if defined(DISABLE_THREADS)
		#pragma inline(OCL_GetTLSID)
		#pragma inline(OCL_GetTLSValue)
		#pragma inline(OCL_SetTLSValue)
		#endif
	#endif
#endif	
#if defined(SRVUM_MODULE)
	#define TLS_PREFIX(name) SRVUM##name
	#if defined(INLINE_IS_PRAGMA)
		#pragma inline(SRVUMGetTLSValue)
		#pragma inline(SRVUMSetTLSValue)
		#if defined(DISABLE_THREADS)
		#pragma inline(SRVUM_GetTLSID)
		#pragma inline(SRVUM_GetTLSValue)
		#pragma inline(SRVUM_SetTLSValue)
		#endif
	#endif
#endif

#ifndef TLS_PREFIX
	#error "Unknown module"
#endif


#if !defined(DISABLE_THREADS)

/* This is a bit yucky, but as a special case the Services Client UM driver
 * needs to export the TLS API to srvinit (Services 4.0) */
#if defined(SRVUM_MODULE)
IMG_EXPORT IMG_UINT32 TLS_PREFIX(_GetTLSID(IMG_VOID));
IMG_EXPORT IMG_VOID *TLS_PREFIX(_GetTLSValue(IMG_VOID));
IMG_EXPORT IMG_BOOL TLS_PREFIX(_SetTLSValue(IMG_VOID *pvA));
IMG_EXPORT IMG_VOID TLS_PREFIX(_InitialiseTLS(IMG_VOID));
#else
IMG_INTERNAL IMG_UINT32 TLS_PREFIX(_GetTLSID(IMG_VOID));
IMG_INTERNAL IMG_VOID *TLS_PREFIX(_GetTLSValue(IMG_VOID));
IMG_INTERNAL IMG_BOOL TLS_PREFIX(_SetTLSValue(IMG_VOID *pvA));
IMG_INTERNAL IMG_VOID TLS_PREFIX(_InitialiseTLS(IMG_VOID));
#endif

static INLINE IMG_VOID* TLS_PREFIX(GetTLSValue(IMG_VOID))
{
	return TLS_PREFIX(_GetTLSValue());
}

static INLINE IMG_BOOL TLS_PREFIX(SetTLSValue(IMG_VOID *pvA))
{
	return TLS_PREFIX(_SetTLSValue(pvA));
}

#else

#if defined(SRVUM_MODULE)
extern IMG_EXPORT IMG_VOID *TLS_PREFIX(g_pvTLS);
#else
extern IMG_INTERNAL IMG_VOID *TLS_PREFIX(g_pvTLS);
#endif

static INLINE IMG_UINT32 TLS_PREFIX(_GetTLSID(IMG_VOID))
{
	return 0;
}

static INLINE IMG_VOID* TLS_PREFIX(_GetTLSValue(IMG_VOID))
{
	return TLS_PREFIX(g_pvTLS);
}

static INLINE IMG_VOID* TLS_PREFIX(GetTLSValue(IMG_VOID))
{
	return TLS_PREFIX(g_pvTLS);
}

static INLINE IMG_BOOL TLS_PREFIX(_SetTLSValue(IMG_VOID *pvA))
{
	TLS_PREFIX(g_pvTLS) = pvA;
	return IMG_TRUE;
}

static INLINE IMG_BOOL TLS_PREFIX(SetTLSValue(IMG_VOID *pvA))
{
	TLS_PREFIX(g_pvTLS) = pvA;
	return IMG_TRUE;
}

#endif

#if defined (__cplusplus)
}
#endif
#endif /* __COMMON_TLS_H_ */

/******************************************************************************
 End of file (common_tls.h)
******************************************************************************/
