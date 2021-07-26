/*

Copyright (C) 1999-2007  Brian Paul   All Rights Reserved.

See Mesa log at http://cgit.freedesktop.org/mesa/mesa/tree/src/egl/wayland/wayland-egl/wayland-egl.c for
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

*/

#include <string.h>
#include <stdbool.h>
#include <kernel.h>

#include "wayland-egl-priv.h"

WL_EGL_EXPORT void
wl_egl_window_resize(struct wl_egl_window *egl_window,
		     int width, int height,
		     int dx, int dy)
{
	egl_window->width  = width;
	egl_window->height = height;
	egl_window->dx     = dx;
	egl_window->dy     = dy;
	egl_window->backBuffersValid = 0;
}

WL_EGL_EXPORT struct wl_egl_window *
wl_egl_window_create(struct wl_surface *surface,
		     int width, int height)
{
	struct wl_egl_window *egl_window;
 
	egl_window = malloc(sizeof *egl_window);
	if (!egl_window)
    	return NULL;

        egl_window->header.type = WWSEGL_DRAWABLE_TYPE_WINDOW;
	egl_window->surface = surface;
	egl_window->display = NULL;
	egl_window->block_swap_buffers = 0;
	sceClibMemset(egl_window->backBuffers, 0, sizeof(egl_window->backBuffers));
	egl_window->currentBackBuffer = -1;
	egl_window->backBuffersValid = 0;
	wl_egl_window_resize(egl_window, width, height, 0, 0);
	egl_window->attached_width  = 0;
	egl_window->attached_height = 0;
	egl_window->numFlipBuffers = 0;
        egl_window->usingFlipBuffers = 0;
	return egl_window;
}

WL_EGL_EXPORT void
wl_egl_window_destroy(struct wl_egl_window *egl_window)
{
	PVRSRVFreeUserModeMem(egl_window);
}

WL_EGL_EXPORT void
wl_egl_window_get_attached_size(struct wl_egl_window *egl_window,
				int *width, int *height)
{
	if (width)
		*width = egl_window->attached_width;
	if (height)
		*height = egl_window->attached_height;
}

WL_EGL_EXPORT struct wl_egl_pixmap *
wl_egl_pixmap_create(int width, int height,
		     uint32_t flags)
{
	struct wl_egl_pixmap *egl_pixmap;
	
	egl_pixmap = PVRSRVAllocUserModeMem(sizeof *egl_pixmap);
	if (egl_pixmap == NULL)
		return NULL;
                
        egl_pixmap->header.type  = WWSEGL_DRAWABLE_TYPE_PIXMAP;
	egl_pixmap->display = NULL;
	egl_pixmap->width   = width;
	egl_pixmap->height  = height;
	egl_pixmap->name    = 0;
	egl_pixmap->stride  = 0;
	egl_pixmap->flags   = flags;

	return egl_pixmap;
}

WL_EGL_EXPORT void
wl_egl_pixmap_destroy(struct wl_egl_pixmap *egl_pixmap)
{
	PVRSRVFreeUserModeMem(egl_pixmap);
}

WL_EGL_EXPORT struct wl_buffer *
wl_egl_pixmap_create_buffer(struct wl_egl_pixmap *egl_pixmap)
{
	if (egl_pixmap->name == 0)
		return NULL;

        return NULL;
/*	return wl_drm_create_buffer(egl_display->drm, egl_pixmap->name,
				    egl_pixmap->width, egl_pixmap->height,
				    egl_pixmap->stride, egl_pixmap->visual); */
}
