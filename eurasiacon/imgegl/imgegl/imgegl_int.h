/******************************************************************************
 * Name         : wsegl.h
 *
 * Copyright    : 2005-2009 by Imagination Technologies Limited.
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
 * $Log: imgegl.h $
 *****************************************************************************/

#if !defined(__IMGEGL_H__)
#define __IMGEGL_H__

#ifdef __cplusplus
extern "C" {
#endif

__declspec(dllexport) EGLint IMGeglGetError(void);

__declspec(dllexport) EGLDisplay IMGeglGetDisplay(NativeDisplayType display);
__declspec(dllexport) EGLBoolean IMGeglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
__declspec(dllexport) EGLBoolean IMGeglTerminate(EGLDisplay dpy);
__declspec(dllexport) const char * IMGeglQueryString(EGLDisplay dpy, EGLint name);
__declspec(dllexport) void (* IMGeglGetProcAddress(const char *procname))(void);

__declspec(dllexport) EGLBoolean IMGeglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
__declspec(dllexport) EGLBoolean IMGeglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
__declspec(dllexport) EGLBoolean IMGeglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);

__declspec(dllexport) EGLSurface IMGeglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, NativeWindowType window, const EGLint *attrib_list);
__declspec(dllexport) EGLSurface IMGeglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, NativePixmapType pixmap, const EGLint *attrib_list);
__declspec(dllexport) EGLSurface IMGeglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
__declspec(dllexport) EGLBoolean IMGeglDestroySurface(EGLDisplay dpy, EGLSurface surface);
__declspec(dllexport) EGLBoolean IMGeglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);

__declspec(dllexport) EGLContext IMGeglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_list, const EGLint *attrib_list);
__declspec(dllexport) EGLBoolean IMGeglDestroyContext(EGLDisplay dpy, EGLContext ctx);
__declspec(dllexport) EGLBoolean IMGeglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
__declspec(dllexport) EGLContext IMGeglGetCurrentContext(void);
__declspec(dllexport) EGLSurface IMGeglGetCurrentSurface(EGLint readdraw);
__declspec(dllexport) EGLDisplay IMGeglGetCurrentDisplay(void);
__declspec(dllexport) EGLBoolean IMGeglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value);

__declspec(dllexport) EGLBoolean IMGeglWaitGL(void);
__declspec(dllexport) EGLBoolean IMGeglWaitNative(EGLint engine);
__declspec(dllexport) EGLBoolean IMGeglSwapBuffers(EGLDisplay dpy, EGLSurface draw);
__declspec(dllexport) EGLBoolean IMGeglCopyBuffers(EGLDisplay dpy, EGLSurface surface, NativePixmapType target);

__declspec(dllexport) EGLBoolean IMGeglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);
__declspec(dllexport) EGLBoolean IMGeglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
__declspec(dllexport) EGLBoolean IMGeglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
__declspec(dllexport) EGLBoolean IMGeglSwapInterval(EGLDisplay dpy, EGLint interval);

__declspec(dllexport) EGLSurface IMGeglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer,
															  EGLConfig config, const EGLint *attrib_list);
__declspec(dllexport) EGLBoolean IMGeglBindAPI(EGLenum api);
__declspec(dllexport) EGLenum    IMGeglQueryAPI(void);
__declspec(dllexport) EGLBoolean IMGeglWaitClient(void);
__declspec(dllexport) EGLBoolean IMGeglReleaseThread(void);

#ifdef __cplusplus
}
#endif


#endif /* __IMGEGL_H__ */

/******************************************************************************
 End of file (imgegl.h)
******************************************************************************/
