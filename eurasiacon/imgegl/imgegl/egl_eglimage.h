/******************************************************************************
 * Name         : egl_eglimage.h
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
 * $Log: egl_eglimage.h $
 *****************************************************************************/

#ifndef _EGL_EGLIMAGE_H_
#define _EGL_EGLIMAGE_H_

#if defined(__cplusplus)
extern "C" {
#endif

EGLBoolean IMGeglDestroyImageKHR(EGLDisplay eglDpy, EGLImageKHR eglImage);

EGLImageKHR IMGeglCreateImageKHR(EGLDisplay eglDpy, EGLContext eglContext, EGLenum target, EGLClientBuffer buffer, EGLint *pAttribList);

IMG_VOID TerminateImages(KEGL_DISPLAY *psDpy);


#if defined(EGL_EXTENSION_NOK_IMAGE_SHARED)

EGLNativeSharedImageTypeNOK IMGeglCreateSharedImageNOK(EGLDisplay eglDpy, EGLImageKHR eglImage, EGLint *pAttribList);

EGLBoolean IMGeglDestroySharedImageNOK(EGLDisplay eglDpy, EGLNativeSharedImageTypeNOK nokImage);

EGLBoolean IMGeglQueryImageNOK(EGLDisplay eglDpy, EGLImageKHR eglImage, EGLint attribute, EGLint *value);

#endif

#if defined(__cplusplus)
}
#endif

#endif /*_EGL_EGLIMAGE_H_*/

/******************************************************************************
 End of file (egl_eglimage.h)
******************************************************************************/

