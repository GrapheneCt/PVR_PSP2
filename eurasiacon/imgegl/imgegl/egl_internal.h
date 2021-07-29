/* 
 * Name         : egl_internal.h
 * Title        : Khronos EGL Internal Definitions.
 * Author       : Marcus Shawcroft
 * Created      : 4 Nov 2003
 *
 * Copyright    : 2003-2005 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form by
 *              : any means, electronic, mechanical, manual or otherwise, or
 *              : disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Description  : Definitions internal to the EGL implementation.
 * 
 * Platform     : ALL
 *
 * Modifications	: 
 * $Log: egl_internal.h $  
 *
 *  --- Revision Logs Removed ---   
 *
 */

#ifndef _EGL_INTERNAL_H_
#define _EGL_INTERNAL_H_

#include <stdlib.h>
#include "imgextensions.h"
#include "cfg_core.h"
#include "pvr_debug.h"
#include "wsegl.h"
#include "services.h"
#include "buffers.h"
#include "eglapi_int.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(API_MODULES_RUNTIME_CHECKED)

#   define IMGEGL_API_OPENGL       0
#   define IMGEGL_API_OPENGLES     1
#   define IMGEGL_API_OPENVG       2
#   define IMGEGL_NUMBER_OF_APIS   3

#else /* defined(API_MODULES_RUNTIME_CHECKED) */ 

#   if defined (SUPPORT_OPENGL)
#       define IMGEGL_API_OPENGL		0
#       if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2)
#           define IMGEGL_API_OPENGLES		1
#           if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
#               define IMGEGL_API_OPENVG		2
#               define IMGEGL_NUMBER_OF_APIS	3
#           else /* defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) */
#               define IMGEGL_NUMBER_OF_APIS	2
#           endif  /* defined(SUPPORT_OPENVG)  || defined(SUPPORT_OPENVGX)*/
#       else /* defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) */
#           if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
#               define IMGEGL_API_OPENVG		1
#               define IMGEGL_NUMBER_OF_APIS	2
#           else /* defined(SUPPORT_OPENVG)  || defined(SUPPORT_OPENVGX)*/
#               define IMGEGL_NUMBER_OF_APIS	1
#           endif /* defined(SUPPORT_OPENVG)  || defined(SUPPORT_OPENVGX)*/
#       endif  /* defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) */
#   else /* defined (SUPPORT_OPENGL)*/
#       if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2)
#           define IMGEGL_API_OPENGLES		0
#           if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
#               define IMGEGL_API_OPENVG		1
#               define IMGEGL_NUMBER_OF_APIS	2
#           else /* defined(SUPPORT_OPENVG)  || defined(SUPPORT_OPENVGX)*/
#               define IMGEGL_NUMBER_OF_APIS	1
#           endif  /* defined(SUPPORT_OPENVG)  || defined(SUPPORT_OPENVGX)*/
#       else /* defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) */
#           if defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)
#               define IMGEGL_API_OPENVG		0
#               define IMGEGL_NUMBER_OF_APIS	1
#           else /* defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)*/
#               error "At least one client API must be supported"
#           endif /* defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)*/
#       endif /* defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) */
#   endif /* defined (SUPPORT_OPENGL)*/

#endif /* defined(API_MODULES_RUNTIME_CHECKED) */ 

#define IMGEGL_API_NONE IMGEGL_NUMBER_OF_APIS


typedef enum 
{
	IMGEGL_CONTEXT_OPENVG		= 0,
	IMGEGL_CONTEXT_OPENGLES1	= 1,
	IMGEGL_CONTEXT_OPENGLES2	= 2,
	IMGEGL_CONTEXT_OPENGL		= 3,
	IMGEGL_CONTEXT_OPENCL		= 4,
	IMGEGL_CONTEXT_TYPEMAX		= 5

} IMGEGL_CONTEXT_TYPE;


/* Forward declarations */
typedef struct _KEGL_SURFACE_ KEGL_SURFACE;
typedef struct _KEGL_CONTEXT_ KEGL_CONTEXT;
typedef EGLint KEGL_CONFIG_INDEX;

#if defined(EGL_EXTENSION_KHR_IMAGE)
typedef struct _KEGL_IMAGE_ KEGL_IMAGE;
#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

#if defined(EGL_EXTENSION_KHR_FENCE_SYNC) || defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)
typedef struct _KEGL_SYNC_ KEGL_SYNC;
#endif /* defined(EGL_EXTENSION_KHR_FENCE_SYNC) || defined(EGL_EXTENSION_KHR_REUSABLE_SYNC) */

#if defined(EGL_EXTENSION_NOK_IMAGE_SHARED) 
typedef struct _KEGL_SHARED_IMAGE_NOK_ KEGL_SHARED_IMAGE_NOK;
#endif /* defined(EGL_EXTENSION_NOK_IMAGE_SHARED) */

typedef struct tls_tag *TLS;

typedef struct KEGL_DISPLAY_TAG
{
	EGLBoolean isInitialised;

	EGLBoolean bHasBeenInitialised;    

	NativeDisplayType nativeDisplay;
	
	/* head of the surface list for this display */
	KEGL_SURFACE *psHeadSurface;
	
	/* head of the context list for this display */
	KEGL_CONTEXT *psHeadContext;

#if defined(EGL_EXTENSION_KHR_IMAGE)
	/* head of the image list for this display */
	KEGL_IMAGE *psHeadImage;
#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

#if defined(EGL_EXTENSION_KHR_FENCE_SYNC) || defined(EGL_EXTENSION_KHR_REUSABLE_SYNC)
	KEGL_SYNC *psHeadSync;
#endif /* defined(EGL_EXTENSION_KHR_FENCE_SYNC) || defined(EGL_EXTENSION_KHR_REUSABLE_SYNC) */

#if defined(EGL_EXTENSION_NOK_IMAGE_SHARED) 
	KEGL_SHARED_IMAGE_NOK *psHeadSharedImageNOK;
#endif /* defined(EGL_EXTENSION_NOK_IMAGE_SHARED) */

	/* handle for the window system display */
	WSEGLDisplayHandle	hDisplay;

	/* Function pointers for the window system calls */
	WSEGL_FunctionTable *pWSEGL_FT;

	/* Library handle for the window system module */	
	IMG_HANDLE hWSDrv;

	const WSEGLCaps *psCapabilities;

	IMG_UINT32 ui32MinSwapInterval;
	IMG_UINT32 ui32MaxSwapInterval;

	IMG_BOOL bUseHWForWindowSync;
	IMG_BOOL bUseHWForPixmapSync;

	/* True if the EGL lock should be released around calls to WSEGL
	   functions */
	IMG_BOOL bUnlockedWSEGL;

	WSEGLConfig *psConfigs;
	
	IMG_UINT32 ui32NumConfigs;

} KEGL_DISPLAY;


typedef enum egl_surface_type_tag
{
	EGL_SURFACE_WINDOW,
	EGL_SURFACE_PIXMAP,
	EGL_SURFACE_PBUFFER

} egl_surface_type;


struct _KEGL_SURFACE_
{
	/* Next surface on this display */
	struct _KEGL_SURFACE_ *psNextSurface;
	
	/* this surface is current */
	EGLint currentCount;

	EGLint refCount;

	/* In surfaces that are current, ie have a currentCount > 0,
	   boundThread contains the systems identity for the binding
	   thread. For non current surfaces, boundThread is not defined. */
	EGLint boundThread;

	/* this surface is marked for deletion as soon as it is no longer current */
	EGLBoolean isDeleting;

	/* enumerated type - window, pixmap, pbuffer */
	egl_surface_type type;

	union
	{
		struct
		{
			IMG_VOID			*native;
			WSEGLDrawableHandle	hDrawable;
			WSEGLConfig			sConfig;
			IMG_UINT32			ui32SwapCount;
		} window;
		struct
		{
			IMG_VOID			*native;
			WSEGLDrawableHandle	hDrawable;
			WSEGLConfig			sConfig;
		} pixmap;
		struct
		{
			IMG_UINT32 ui32ByteStride;

			IMG_UINT32 ui32PixelWidth;
			IMG_UINT32 ui32PixelHeight;
			PVRSRV_PIXEL_FORMAT	ePixelFormat;
			PVRSRV_CLIENT_MEM_INFO *psMemInfo;
			PVRSRV_PIXEL_FORMAT eTextureFormat;
			IMG_BOOL bTexture;
			IMG_BOOL bMipMap;
			IMG_UINT32 ui32Level;
			IMG_HANDLE hTexture;
			IMG_BOOL bLargest;

		} pbuffer;
	} u;

	SceUID hPBufferMemBlockUID;
  
	EGLint iSwapBehaviour;
	EGLint iMultiSampleResolve;

	/* Rotation of drawable */
	WSEGLRotationAngle eRotationAngle;

	/* configuration of this surface */
	KEGL_CONFIG *psCfg;

	/* display associated with this surface */
	KEGL_DISPLAY *psDpy;
	
	EGLRenderSurface sRenderSurface;
};



struct _KEGL_CONTEXT_
{
	/* Next context on this display */
	struct _KEGL_CONTEXT_ *psNextContext;
	
	EGLBoolean isFirstMakeCurrent;

	/* Indicates that the context is bound as the current context. Note in
	   a multi threaded environment there may be multiple current contexts. */
	EGLBoolean isCurrent;

	/* In contexts that are marked current, boundThread contains the
	   systems identitiy for the binding thread. For non current
	   contexts, boundThread is not defined. */
	EGLint boundThread;

	/* Indicates that this context is in the process of deleting
	   itself. Deletion will complete when the context becomes
	   un-bound. */
	EGLBoolean isDeleting;

	/* The display on which this context was created. */
	EGLDisplay eglDpy;

	KEGL_DISPLAY *psDpy;
	
	/* The configuration with which this context was created. */
	KEGL_CONFIG *psCfg;

	/* The client context handled created for this context. */
	EGLContextHandle hClientContext;
	
	EGLcontextMode contextMode;

	/* The API that this context was created for */
	IMGEGL_CONTEXT_TYPE eContextType;
};


typedef struct IMGEGLAppHints_TAG
{
	IMG_UINT32	ui32PDSFragBufferSize;
	IMG_UINT32	ui32ParamBufferSize;
	IMG_UINT32	ui32DriverMemorySize;

	IMG_UINT32	ui32ExternalZBufferMode;
	IMG_UINT32	ui32DepthBufferXSize;
	IMG_UINT32	ui32DepthBufferYSize;
#if defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED)
	IMG_UINT32 ui32ExternalVGXStencilBufferMode;
#endif
	IMG_CHAR	szWindowSystem[APPHINT_MAX_STRING_SIZE];

#if defined (TIMING) || defined (DEBUG)
	IMG_BOOL	bDumpProfileData;
	IMG_UINT32	ui32ProfileStartFrame;
	IMG_UINT32	ui32ProfileEndFrame;
	IMG_BOOL	bDisableMetricsOutput;
#endif /* (TIMING) || (DEBUG) */

#if defined (SUPPORT_HYBRID_PB)
	IMG_BOOL	bPerContextPB;
#endif
} IMGEGLAppHints;



IMG_INTERNAL IMG_VOID *ENV_CreateGlobalData(IMG_UINT32 ui32Size, IMG_VOID (*pfDeinitCallback)(IMG_VOID *));
IMG_INTERNAL IMG_VOID *ENV_GetGlobalData(IMG_VOID);
IMG_INTERNAL IMG_VOID ENV_DestroyGlobalData(IMG_VOID);
#if defined(API_MODULES_RUNTIME_CHECK_BY_NAME)
IMG_INTERNAL IMG_BOOL ENV_FileExists(const IMG_CHAR *pszFileName);
#endif



extern const IMG_UINT32 aWSEGLPixelFormatBytesPerPixelShift[];

KEGL_DISPLAY * GetKEGLDisplay(TLS psTls, EGLDisplay eglDpy);

IMG_BOOL IsEGLContext(KEGL_DISPLAY *psDpy, KEGL_CONTEXT *psInputContext);

/************************************************************************/
/*						   Allocation functions 						*/
/************************************************************************/
#if (defined(__linux__) && !defined(DEBUG))

	#define	EGLMalloc(X)	(IMG_VOID*)malloc(X)
	#define	EGLCalloc(X)	(IMG_VOID*)calloc(1, X)
	#define	EGLRealloc(X,Y)	(IMG_VOID*)realloc(X, Y)
	#define	EGLFree(X)				   free(X)

#else

	#if defined(__linux__)

		extern IMG_EXPORT IMG_PVOID PVRSRVAllocUserModeMemTracking(IMG_UINT32 ui32Size, IMG_CHAR *pszFileName, IMG_UINT32 ui32LineNumber);
		extern IMG_EXPORT IMG_PVOID PVRSRVCallocUserModeMemTracking(IMG_UINT32 ui32Size, IMG_CHAR *pszFileName, IMG_UINT32 ui32LineNumber);
		extern IMG_EXPORT IMG_VOID  PVRSRVFreeUserModeMemTracking(IMG_VOID *pvMem);
		extern IMG_EXPORT IMG_PVOID PVRSRVReallocUserModeMemTracking(IMG_VOID *pvMem, IMG_UINT32 ui32NewSize, IMG_CHAR *pszFileName, IMG_UINT32 ui32LineNumber);

		#define	EGLMalloc(X)	(IMG_VOID*)PVRSRVAllocUserModeMemTracking(X, __FILE__, __LINE__)
		#define	EGLCalloc(X)	(IMG_VOID*)PVRSRVCallocUserModeMemTracking(X, __FILE__, __LINE__)
		#define	EGLRealloc(X,Y)	(IMG_VOID*)PVRSRVReallocUserModeMemTracking(X, Y, __FILE__, __LINE__)
		#define	EGLFree(X)				   PVRSRVFreeUserModeMemTracking(X)

	#else

		#define	EGLMalloc(X)	(IMG_VOID*)PVRSRVAllocUserModeMem(X)
		#define	EGLCalloc(X)	(IMG_VOID*)PVRSRVCallocUserModeMem(X)
		#define	EGLRealloc(X,Y)	(IMG_VOID*)PVRSRVReallocUserModeMem(X, Y)
		#define	EGLFree(X)				   PVRSRVFreeUserModeMem(X)

	#endif

#endif


#if defined(DEBUG)

#define IMGEGLALLOCDEVICEMEM(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo) \
		 KEGLAllocDeviceMemTrack(psSysContext, __FILE__, __LINE__, psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo)

#define IMGEGLFREEDEVICEMEM(psDevData, psMemInfo) \
		 KEGLFreeDeviceMemTrack(psSysContext, __FILE__, __LINE__, psDevData, psMemInfo)


#else /* defined(DEBUG) */

#define IMGEGLALLOCDEVICEMEM(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo) \
		 KEGLAllocDeviceMemPsp2(psSysContext, psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, ppsMemInfo)

#define IMGEGLFREEDEVICEMEM(psDevData, psMemInfo) \
		 KEGLFreeDeviceMemPsp2(psSysContext, psDevData, psMemInfo)

#endif /* defined(DEBUG) */

#if defined(__cplusplus)
}
#endif

#endif /*_EGL_INTERNAL_H_*/
/******************************************************************************
 End of file (egl_internal.h)
******************************************************************************/

