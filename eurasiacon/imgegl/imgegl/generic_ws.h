/******************************************************************************
 * Name         : generic_ws.h
 * Title        : Generic Window System Interface
 * Author       : Marcus Shawcroft
 * Created      : 04/11/2003
 *
 * Copyright    : 2003-2005 by Imagination Technologies Limited.
 * 				  All rights reserved. No part of this software, either
 * 				  material or conceptual may be copied or distributed,
 * 				  transmitted, transcribed, stored in a retrieval system or
 * 				  translated into any human or computer language in any form
 * 				  by any means, electronic, mechanical, manual or other-wise,
 * 				  or disclosed to third parties without the express written
 * 				  permission of Imagination Technologies Limited, HomePark
 * 				  Estate, Kings Langley, Hertfordshire, WD4 8LZ, U.K.
 *
 * Description  : API of window system interface components
 *                which are generic to all window system interfaces.
 *
 * Platform     : ANSI
 *
 * Modifications: 
 * $Log: generic_ws.h $
 *
 *  --- Revision Logs Removed --- 
 ********************************************************************************/

#ifndef _GENERIC_WS_H_
#define _GENERIC_WS_H_

/* Called when the EGL lock must be taken. */
extern void EGLThreadLock(TLS psTls);
extern void EGLThreadUnlock(TLS psTls);

/* These macros should be used when the EGL thread lock is being taken
   around a WSEGL call. The lock is not taken if the WSEGL module has the
   WSEGL_CAP_UNLOCKED capability. */
#define EGLThreadLockWSEGL(dpy, tls) do			\
	{											\
		if (!(dpy)->bUnlockedWSEGL)				\
			EGLThreadLock(tls);					\
	} while(0)

#define EGLThreadUnlockWSEGL(dpy, tls) do		\
	{											\
		if (!(dpy)->bUnlockedWSEGL)				\
			EGLThreadUnlock(tls);				\
	} while (0)

/* These macros should be used when a WSEGL call is being made with the EGL
   thread lock already held. The lock is released if the WSEGL module has
   the WSEGL_CAP_UNLOCKED capability. */
#define EGLReleaseThreadLockWSEGL(dpy, tls) do	\
	{											\
		if ((dpy)->bUnlockedWSEGL)				\
			EGLThreadUnlock(tls);				\
	} while (0)

#define EGLReacquireThreadLockWSEGL(dpy, tls) do	\
	{												\
		if ((dpy)->bUnlockedWSEGL)					\
			EGLThreadLock(tls);						\
	} while(0)

IMG_BOOL LoadWSModule(SrvSysContext *psSysContext, 
					  KEGL_DISPLAY *pDisplay, 
					  IMG_HANDLE *phWSDrvm, 
					  NativeDisplayType nativeDisplay,
					  IMG_CHAR *pszModuleName);

IMG_BOOL UnloadModule(IMG_HANDLE hModule);

#if defined(API_MODULES_RUNTIME_CHECKED)
IMG_INTERNAL IMG_BOOL IsOGLES1ModulePresent(void);
IMG_INTERNAL IMG_BOOL IsOGLES2ModulePresent(void);
IMG_INTERNAL IMG_BOOL IsOVGModulePresent(void);
IMG_INTERNAL IMG_BOOL IsOGLModulePresent(void);
IMG_INTERNAL IMG_BOOL IsOCLModulePresent(void);
#endif

#if defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED)
IMG_INTERNAL IMG_BOOL LoadOGLES1AndGetFunctions(EGLGlobal *psGlobalData);
#endif

#if defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)
IMG_INTERNAL IMG_BOOL LoadOGLES2AndGetFunctions(EGLGlobal *psGlobalData);
#endif

#if defined(SUPPORT_OPENGL) || defined(API_MODULES_RUNTIME_CHECKED)
IMG_INTERNAL IMG_BOOL LoadOGLAndGetFunctions(EGLGlobal *psGlobalData);
#endif

#if (defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX)) || defined(API_MODULES_RUNTIME_CHECKED)
IMG_BOOL IMG_INTERNAL LoadOVGAndGetFunctions(EGLGlobal *psGlobalData);
#endif

#if defined(SUPPORT_OPENCL) || defined(API_MODULES_RUNTIME_CHECKED)
IMG_INTERNAL IMG_BOOL LoadOCLAndGetFunctions(EGLGlobal *psGlobalData);
#endif

/***********************************************************************
 *
 *  FUNCTION   : GWS_CreatePBufferDrawable
 *  PURPOSE    : Create a pbuffer drawable.
 *  PARAMETERS :
 *    In:  services_context - Services context.
 *    In:  pSurface - Surface.
 *    In:  pbuffer_width - Required width in pixels.
 *    In:  pbuffer_height - Required height in pixels.
 *    In:  pbuffer_largest - Allocate largest possible.
 *    In:  pixel_width - Pixel width in bytes.
 *    In:  pixel_format - Pixel format.
 *  RETURNS    :
 *    EGL_SUCCESS: pixmap conforms to config.
 *
 ***********************************************************************/
EGLint GWS_CreatePBufferDrawable(SrvSysContext *services_context,
								 KEGL_SURFACE *pSurface,
								 EGLint pbuffer_width,
								 EGLint pbuffer_height,
								 EGLint pbuffer_largest,
								 EGLint pixel_width,
								 PVRSRV_PIXEL_FORMAT pixel_format);


/***********************************************************************
 *
 *  FUNCTION   : GWS_DeletePBufferDrawable
 *  PURPOSE    : Destroys a PBuffer's render surface command queue and
 *               device memory
 *  PARAMETERS :
 *    In:  psContext - Services context.
 *    In:  pSurface - Surface.
 *  RETURNS    :
 *    IMG_VOID :
 *
 ***********************************************************************/
IMG_VOID GWS_DeletePBufferDrawable(KEGL_SURFACE* psSurface,
								   SrvSysContext *psContext);

#endif /* _GENERIC_WS_H_ */
/*****************************************************************************
 End of file (generic_ws.h)
*****************************************************************************/
