/******************************************************************************
 * Name         : eglentrypoints.c
 * Created      : 10/11/2005
 *
 * Copyright    : 2005-2008 by Imagination Technologies Limited.
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
 * $Log: eglentrypoints.c $
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "drvegl.h"
#include "imgegl.h"

EGLAPI EGLint EGLAPIENTRY eglGetError(void)
{
	return IMGeglGetError();
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetDisplay(NativeDisplayType nativeDisplay)
{
	return IMGeglGetDisplay(nativeDisplay);
}

EGLAPI EGLBoolean EGLAPIENTRY eglInitialize(EGLDisplay eglDpy, EGLint *major, EGLint *minor)
{
	return IMGeglInitialize(eglDpy, major, minor);
}

EGLAPI EGLBoolean EGLAPIENTRY eglTerminate(EGLDisplay eglDpy)
{
	return IMGeglTerminate(eglDpy);
}

EGLAPI const char * EGLAPIENTRY eglQueryString(EGLDisplay eglDpy, EGLint name)
{
	return IMGeglQueryString(eglDpy, name);
}

EGLAPI void (* EGLAPIENTRY eglGetProcAddress(const char *procname))(void)
{
	return IMGeglGetProcAddress(procname);
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigs(EGLDisplay eglDpy, EGLConfig *configs,
											EGLint config_size, EGLint *num_config)
{
	return IMGeglGetConfigs(eglDpy, configs, config_size, num_config);
}

EGLAPI EGLBoolean EGLAPIENTRY eglChooseConfig(EGLDisplay eglDpy,
											  const EGLint *pAttribList,
											  EGLConfig *configs,
											  EGLint config_size,
											  EGLint *num_config)
{
	return IMGeglChooseConfig(eglDpy, pAttribList, configs, config_size, num_config);
}

EGLAPI EGLBoolean EGLAPIENTRY eglGetConfigAttrib(EGLDisplay eglDpy,
												 EGLConfig eglCfg,
												 EGLint attribute,
												 EGLint *value)
{
	return IMGeglGetConfigAttrib(eglDpy, eglCfg, attribute, value);
}

EGLAPI EGLSurface EGLAPIENTRY eglCreateWindowSurface(EGLDisplay eglDpy,
													 EGLConfig eglCfg,
													 NativeWindowType window,
													 const EGLint *pAttribList)
{
	return IMGeglCreateWindowSurface(eglDpy, eglCfg, window, pAttribList);
}

EGLAPI EGLSurface EGLAPIENTRY eglCreatePixmapSurface(EGLDisplay eglDpy,
													 EGLConfig eglCfg,
													 NativePixmapType pixmap,
													 const EGLint *pAttribList)
{
	return IMGeglCreatePixmapSurface(eglDpy, eglCfg, pixmap, pAttribList);
}

EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferSurface(EGLDisplay eglDpy,
													  EGLConfig eglCfg,
													  const EGLint *pAttribList)
{
	return IMGeglCreatePbufferSurface(eglDpy, eglCfg, pAttribList);
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroySurface(EGLDisplay eglDpy, EGLSurface pSurface)
{
	return IMGeglDestroySurface(eglDpy, pSurface);
}

EGLAPI EGLBoolean EGLAPIENTRY eglQuerySurface(EGLDisplay eglDpy,
											  EGLSurface pSurface,
											  EGLint attribute,
											  EGLint *value)
{
	return IMGeglQuerySurface(eglDpy, pSurface, attribute, value);
}

EGLAPI EGLContext EGLAPIENTRY eglCreateContext(EGLDisplay eglDpy,
											   EGLConfig eglCfg,
											   EGLContext pShareList,
											   const EGLint *pAttribList)
{
	return IMGeglCreateContext(eglDpy, eglCfg, pShareList, pAttribList);
}

EGLAPI EGLBoolean EGLAPIENTRY eglDestroyContext(EGLDisplay eglDpy, EGLContext pCtx)
{
	return IMGeglDestroyContext(eglDpy, pCtx);
}

EGLAPI EGLBoolean EGLAPIENTRY eglMakeCurrent(EGLDisplay eglDpy,
											 EGLSurface draw,
											 EGLSurface read,
											 EGLContext pCtx)
{
	return IMGeglMakeCurrent(eglDpy, draw, read, pCtx);
}

EGLAPI EGLContext EGLAPIENTRY eglGetCurrentContext(void)
{
	return IMGeglGetCurrentContext();
}

EGLAPI EGLSurface EGLAPIENTRY eglGetCurrentSurface(EGLint readdraw)
{
	return IMGeglGetCurrentSurface(readdraw);
}

EGLAPI EGLDisplay EGLAPIENTRY eglGetCurrentDisplay(void)
{
	return IMGeglGetCurrentDisplay();
}

EGLAPI EGLBoolean EGLAPIENTRY eglQueryContext(EGLDisplay eglDpy,
											  EGLContext pCtx,
											  EGLint attribute,
											  EGLint *value)
{
	return IMGeglQueryContext(eglDpy, pCtx, attribute, value);
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitGL(void)
{
	return IMGeglWaitGL();
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitNative(EGLint engine)
{
	return IMGeglWaitNative(engine);
}

EGLAPI EGLBoolean EGLAPIENTRY eglSwapBuffers(EGLDisplay eglDpy, EGLSurface draw)
{
	return IMGeglSwapBuffers(eglDpy, draw);
}

EGLAPI EGLBoolean EGLAPIENTRY eglCopyBuffers(EGLDisplay eglDpy,
											 EGLSurface pSurface,
											 NativePixmapType target)
{
	return IMGeglCopyBuffers(eglDpy, pSurface, target);
}


EGLAPI EGLBoolean EGLAPIENTRY eglSwapInterval(EGLDisplay eglDpy, EGLint interval)
{
	return IMGeglSwapInterval(eglDpy, interval);
}

EGLAPI EGLBoolean EGLAPIENTRY eglSurfaceAttrib(EGLDisplay eglDpy, 
												EGLSurface pSurface, 
												EGLint attribute, 
												EGLint value)
{
	return IMGeglSurfaceAttrib(eglDpy, pSurface, attribute, value);
}

EGLAPI EGLBoolean EGLAPIENTRY eglBindTexImage(EGLDisplay eglDpy, 
											   EGLSurface pSurface, 
											   EGLint buffer)
{
	return IMGeglBindTexImage(eglDpy, pSurface, buffer);
}

EGLAPI EGLBoolean EGLAPIENTRY eglReleaseTexImage(EGLDisplay eglDpy, 
												  EGLSurface pSurface, 
												  EGLint buffer)
{
	return IMGeglReleaseTexImage(eglDpy, pSurface, buffer);
}


#if defined(EGL_MODULE)

EGLAPI EGLSurface EGLAPIENTRY eglCreatePbufferFromClientBuffer(EGLDisplay dpy,
																EGLenum buftype,
																EGLClientBuffer buffer,
																EGLConfig config,
																const EGLint *attrib_list)
{
	return IMGeglCreatePbufferFromClientBuffer(dpy, buftype, buffer, config, attrib_list);
}


EGLAPI EGLBoolean EGLAPIENTRY eglBindAPI(EGLenum api)
{
	return IMGeglBindAPI(api);
}


EGLAPI EGLenum EGLAPIENTRY eglQueryAPI(void)
{
	return IMGeglQueryAPI();
}

EGLAPI EGLBoolean EGLAPIENTRY eglWaitClient(void)
{
	return IMGeglWaitClient();
}


EGLAPI EGLBoolean EGLAPIENTRY eglReleaseThread(void)
{
	return IMGeglReleaseThread();
}

#endif /* MODULE_EGL */

/******************************************************************************
 End of file (eglentrypoints.c)
******************************************************************************/
