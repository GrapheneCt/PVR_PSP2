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

IMG_IMPORT EGLint IMGeglGetError(void);

IMG_IMPORT EGLDisplay IMGeglGetDisplay(NativeDisplayType display);
IMG_IMPORT EGLBoolean IMGeglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
IMG_IMPORT EGLBoolean IMGeglTerminate(EGLDisplay dpy);
IMG_IMPORT const char * IMGeglQueryString(EGLDisplay dpy, EGLint name);
IMG_IMPORT void (* IMGeglGetProcAddress(const char *procname))(void);

IMG_IMPORT EGLBoolean IMGeglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
IMG_IMPORT EGLBoolean IMGeglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
IMG_IMPORT EGLBoolean IMGeglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);

IMG_IMPORT EGLSurface IMGeglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, NativeWindowType window, const EGLint *attrib_list);
IMG_IMPORT EGLSurface IMGeglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, NativePixmapType pixmap, const EGLint *attrib_list);
IMG_IMPORT EGLSurface IMGeglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
IMG_IMPORT EGLBoolean IMGeglDestroySurface(EGLDisplay dpy, EGLSurface surface);
IMG_IMPORT EGLBoolean IMGeglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);

IMG_IMPORT EGLContext IMGeglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_list, const EGLint *attrib_list);
IMG_IMPORT EGLBoolean IMGeglDestroyContext(EGLDisplay dpy, EGLContext ctx);
IMG_IMPORT EGLBoolean IMGeglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
IMG_IMPORT EGLContext IMGeglGetCurrentContext(void);
IMG_IMPORT EGLSurface IMGeglGetCurrentSurface(EGLint readdraw);
IMG_IMPORT EGLDisplay IMGeglGetCurrentDisplay(void);
IMG_IMPORT EGLBoolean IMGeglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value);

IMG_IMPORT EGLBoolean IMGeglWaitGL(void);
IMG_IMPORT EGLBoolean IMGeglWaitNative(EGLint engine);
IMG_IMPORT EGLBoolean IMGeglSwapBuffers(EGLDisplay dpy, EGLSurface draw);
IMG_IMPORT EGLBoolean IMGeglCopyBuffers(EGLDisplay dpy, EGLSurface surface, NativePixmapType target);

IMG_IMPORT EGLBoolean IMGeglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value);
IMG_IMPORT EGLBoolean IMGeglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
IMG_IMPORT EGLBoolean IMGeglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
IMG_IMPORT EGLBoolean IMGeglSwapInterval(EGLDisplay dpy, EGLint interval);

IMG_IMPORT EGLSurface IMGeglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer,
															  EGLConfig config, const EGLint *attrib_list);
IMG_IMPORT EGLBoolean IMGeglBindAPI(EGLenum api);
IMG_IMPORT EGLenum    IMGeglQueryAPI(void);
IMG_IMPORT EGLBoolean IMGeglWaitClient(void);
IMG_IMPORT EGLBoolean IMGeglReleaseThread(void);

#ifdef __cplusplus
}
#endif


#endif /* __IMGEGL_H__ */

/******************************************************************************
 End of file (imgegl.h)
******************************************************************************/
