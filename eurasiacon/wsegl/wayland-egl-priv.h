/*

Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.

See Mesa log at http://cgit.freedesktop.org/mesa/mesa/tree/src/egl/wayland/wayland-egl/wayland-egl-priv.h for
exact contributors to original file.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Some contributions under similar license, 
Copyright (C) Carsten Munk 2011 All rights reserved

*/


#ifndef _WAYLAND_EGL_PRIV_H
#define _WAYLAND_EGL_PRIV_H

#ifdef  __cplusplus
extern "C" {
#endif

/* GCC visibility */
#if defined(__GNUC__) && __GNUC__ >= 4
#define WL_EGL_EXPORT __attribute__ ((visibility("default")))
#else
#define WL_EGL_EXPORT
#endif

#include <stdbool.h>
#include <pvr2d.h>

typedef struct wl_display* NativeDisplayType;  
typedef struct wl_egl_pixmap* NativePixmapType;
typedef struct wl_egl_window* NativeWindowType;

#include "wsegl.h"

#define WAYLANDWSEGL_MAX_BACK_BUFFERS     2
#define WAYLANDWSEGL_MAX_FLIP_BUFFERS 	  2

enum WWSEGL_DRAWABLE_TYPE
{
     WWSEGL_DRAWABLE_TYPE_UNKNOWN,
     WWSEGL_DRAWABLE_TYPE_WINDOW,
     WWSEGL_DRAWABLE_TYPE_PIXMAP,
     WWSEGL_DRAWABLE_TYPE_DRMBUFFER
};

struct wwsegl_drawable_header {
	enum WWSEGL_DRAWABLE_TYPE type;
};
 

struct wl_egl_display {
	struct wl_display *display;
	WSEGLConfig wseglDisplayConfigs[3];
	int context_refcnt;
	PVR2DCONTEXTHANDLE context;
	struct wl_drm *drm;
};

struct wl_egl_window {
	struct wwsegl_drawable_header header;
	
	struct wl_surface *surface;
	struct wl_visual *visual;
	struct wl_egl_display *display;
 
	int width;
	int height;
	int dx;
	int dy;

	int attached_width;
	int attached_height;

	int stridePixels;
	int strideBytes;


	WSEGLPixelFormat   format;
	PVR2DMEMINFO	   *frontBufferPVRMEM;
	PVR2DMEMINFO       *backBuffers[WAYLANDWSEGL_MAX_BACK_BUFFERS];
	struct wl_buffer   *drmbuffers[WAYLANDWSEGL_MAX_BACK_BUFFERS];
	int                numFlipBuffers;
	PVR2DFLIPCHAINHANDLE flipChain;
	PVR2DMEMINFO       *flipBuffers[WAYLANDWSEGL_MAX_FLIP_BUFFERS];
	
	int                 currentBackBuffer;
	int                 backBuffersValid;

	int		   block_swap_buffers;
	int 		   usingFlipBuffers;

};

struct wl_egl_pixmap {
	struct wwsegl_drawable_header header;

	struct wl_egl_display *display;
	struct wl_visual *visual;
	
        int name;
	int width;
	int height;

	int stride;
	unsigned int flags;
	WSEGLPixelFormat   format;
 
 	PVR2DMEMINFO *pvrmem;
};

struct wl_egl_drmbuffer {
        struct wwsegl_drawable_header header;
	struct wl_visual      *visual;
	
	int name;
	int width;
	int height;
	int stride;
        
};



#ifdef  __cplusplus
}
#endif

#endif
