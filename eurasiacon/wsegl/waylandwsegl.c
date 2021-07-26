/**
** Primary part of this is:
**
** Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception   
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.

** Copyright (C) 2011 Carsten Munk. All rights reserved
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "wsegl.h"
#include "pvr2d.h"
#include "wayland-egl-priv.h"

static WSEGLCaps const wseglDisplayCaps[] = {
    {WSEGL_CAP_WINDOWS_USE_HW_SYNC, 0},
    {WSEGL_CAP_PIXMAPS_USE_HW_SYNC, 6},
    {WSEGL_NO_CAPS, 0}
};


wl_egl_display*
wl_egl_display_create(struct wl_display *display)
{
	wl_egl_display *egl_display;

	egl_display = malloc(sizeof(wl_egl_display));
	if (!egl_display)
		return NULL;
	egl_display->display = display;

	egl_display->context_refcnt = 0;
	egl_display->context = 0;

	egl_display->wseglDisplayConfigs[0].ui32DrawableType = WSEGL_DRAWABLE_WINDOW;
	egl_display->wseglDisplayConfigs[0].ePixelFormat = WSEGL_PIXELFORMAT_8888;
	egl_display->wseglDisplayConfigs[0].ulNativeRenderable = WSEGL_FALSE;
	egl_display->wseglDisplayConfigs[0].ulFrameBufferLevel = 0;
	egl_display->wseglDisplayConfigs[0].ulNativeVisualID = 0;
	egl_display->wseglDisplayConfigs[0].hNativeVisual = 0;
	egl_display->wseglDisplayConfigs[0].eTransparentType = WSEGL_OPAQUE;
        egl_display->wseglDisplayConfigs[0].ulTransparentColor = 0;
        	
	egl_display->wseglDisplayConfigs[1].ui32DrawableType = WSEGL_DRAWABLE_PIXMAP;
	egl_display->wseglDisplayConfigs[1].ePixelFormat = WSEGL_PIXELFORMAT_8888;
	egl_display->wseglDisplayConfigs[1].ulNativeRenderable = WSEGL_FALSE;
	egl_display->wseglDisplayConfigs[1].ulFrameBufferLevel = 0;
	egl_display->wseglDisplayConfigs[1].ulNativeVisualID = 0;
	egl_display->wseglDisplayConfigs[1].hNativeVisual = 0;
	egl_display->wseglDisplayConfigs[1].eTransparentType = WSEGL_OPAQUE;
        egl_display->wseglDisplayConfigs[1].ulTransparentColor = 0;

	egl_display->wseglDisplayConfigs[2].ui32DrawableType = WSEGL_NO_DRAWABLE;
	egl_display->wseglDisplayConfigs[2].ePixelFormat = 0;
	egl_display->wseglDisplayConfigs[2].ulNativeRenderable = 0;
	egl_display->wseglDisplayConfigs[2].ulFrameBufferLevel = 0;
	egl_display->wseglDisplayConfigs[2].ulNativeVisualID = 0;
	egl_display->wseglDisplayConfigs[2].hNativeVisual = 0;
	egl_display->wseglDisplayConfigs[2].eTransparentType = 0;
        egl_display->wseglDisplayConfigs[2].ulTransparentColor = 0;

	
        return egl_display;
}

WL_EGL_EXPORT void
wl_egl_display_destroy(struct wl_egl_display *egl_display)
{

	free(egl_display);
}

/* PVR2D Context handling */
static int wseglFetchContext(struct wl_egl_display *nativeDisplay)
{
   int numDevs;
   PVR2DDEVICEINFO *devs;
   unsigned long devId;
   
   if (nativeDisplay->context_refcnt > 0)
     return 1;
   
   numDevs = PVR2DEnumerateDevices(0);
   if (numDevs <= 0)
     return 0;
     
   devs = (PVR2DDEVICEINFO *)malloc(sizeof(PVR2DDEVICEINFO) * numDevs);
   
   if (!devs)
     return 0;
     
   if (PVR2DEnumerateDevices(devs) != PVR2D_OK)
   {
     free(devs);
     return 0;
   }   
   
   devId = devs[0].ulDevID;
   free(devs);
   if (PVR2DCreateDeviceContext(devId, &nativeDisplay->context, 0) != PVR2D_OK)
     return 0;
   
   nativeDisplay->context_refcnt++;
   
   return 1;         
}

static void wseglReleaseContext(struct wl_egl_display *nativeDisplay)
{
   if (nativeDisplay->context_refcnt > 0)
   {
      nativeDisplay->context_refcnt--;
      if (nativeDisplay->context_refcnt == 0)
      {
         PVR2DDestroyDeviceContext(nativeDisplay->context);
         nativeDisplay->context = 0;
      }
   }
}

static PVR2DFORMAT wsegl2pvr2dformat(WSEGLPixelFormat format)
{
    switch (format)
    {
       case WSEGL_PIXELFORMAT_565:
          return PVR2D_RGB565;
       case WSEGL_PIXELFORMAT_4444:
          return PVR2D_ARGB4444;
       case WSEGL_PIXELFORMAT_8888:
          return PVR2D_ARGB8888;
       default:
          assert(0);
    }
}


/* Determine if nativeDisplay is a valid display handle */
static WSEGLError wseglIsDisplayValid(NativeDisplayType nativeDisplay)
{
  return WSEGL_SUCCESS;
}

/* Helper routines for pixel formats */
static WSEGLPixelFormat getwseglPixelFormat(struct wl_egl_display *egldisplay)
{
       if (egldisplay->var.bits_per_pixel == 16) {
          if (egldisplay->var.red.length == 5 && egldisplay->var.green.length == 6 &&
              egldisplay->var.blue.length == 5 && egldisplay->var.red.offset == 11 &&
              egldisplay->var.green.offset == 5 && egldisplay->var.blue.offset == 0) {
              return WSEGL_PIXELFORMAT_565;
          }
          if (egldisplay->var.red.length == 4 && egldisplay->var.green.length == 4 &&
              egldisplay->var.blue.length == 4 && egldisplay->var.transp.length == 4 &&
              egldisplay->var.red.offset == 8 && egldisplay->var.green.offset == 4 &&
              egldisplay->var.blue.offset == 0 && egldisplay->var.transp.offset == 12) {
              return WSEGL_PIXELFORMAT_4444;
          }
       } else if (egldisplay->var.bits_per_pixel == 32) {
          if (egldisplay->var.red.length == 8 && egldisplay->var.green.length == 8 &&
              egldisplay->var.blue.length == 8 && egldisplay->var.transp.length == 8 &&
              egldisplay->var.red.offset == 16 && egldisplay->var.green.offset == 8 &&
              egldisplay->var.blue.offset == 0 && egldisplay->var.transp.offset == 24) {
              return WSEGL_PIXELFORMAT_8888;
          }
       }
       else
        assert(0);  
    return WSEGL_SUCCESS;
}



static void
drm_handle_device(void *data, struct wl_drm *drm, const char *device)
{
    wl_drm_authenticate(drm, 0);
}

static void
drm_handle_authenticated(void *data, struct wl_drm *drm)
{
}

/* Initialize a native display for use with WSEGL */
static WSEGLError wseglInitializeDisplay
    (NativeDisplayType nativeDisplay, WSEGLDisplayHandle *display,
     const WSEGLCaps **caps, WSEGLConfig **configs)
{
    struct wl_egl_display *egldisplay = wl_egl_display_create((struct wl_display *) nativeDisplay);

    if (wseglFetchContext(egldisplay) != 1)
    {
       wl_egl_display_destroy(egldisplay);
       return WSEGL_OUT_OF_MEMORY;
    }


    egldisplay->wseglDisplayConfigs[0].ePixelFormat = WSEGL_PIXELFORMAT_8888;
    egldisplay->wseglDisplayConfigs[1].ePixelFormat = WSEGL_PIXELFORMAT_8888;

    *display = (WSEGLDisplayHandle)egldisplay;
    *caps = wseglDisplayCaps;
    *configs = egldisplay->wseglDisplayConfigs;
    return WSEGL_SUCCESS;
}

/* Close the WSEGL display */
static WSEGLError wseglCloseDisplay(WSEGLDisplayHandle display)
{
   struct wl_egl_display *egldisplay = (struct wl_egl_display *) display;
   wseglReleaseContext(egldisplay);
   assert(egldisplay->context == 0);
   wl_egl_display_destroy(egldisplay);
   
   return WSEGL_SUCCESS;
}

static WSEGLError allocateBackBuffers(struct wl_egl_display *egldisplay, NativeWindowType nativeWindow)
{
    int index;
    
    if (nativeWindow->numFlipBuffers)
    {
        long stride = 0;
        
        unsigned long flipId = 0;
        unsigned long numBuffers;
        assert(PVR2DCreateFlipChain(egldisplay->context, 0,
                                 //PVR2D_CREATE_FLIPCHAIN_SHARED |
                                 //PVR2D_CREATE_FLIPCHAIN_QUERY,
                                 nativeWindow->numFlipBuffers,
                                 nativeWindow->width,
                                 nativeWindow->height,
                                 wsegl2pvr2dformat(nativeWindow->format),
                                 &stride, &flipId, &(nativeWindow->flipChain))
                == PVR2D_OK); 
        PVR2DGetFlipChainBuffers(egldisplay->context,
                                     nativeWindow->flipChain,
                                     &numBuffers,
                                     nativeWindow->flipBuffers);
        for (index = 0; index < numBuffers; ++index)
        {
             nativeWindow->backBuffers[index] = nativeWindow->flipBuffers[index];
        }
    }
    else {
	    for (index = 0; index < WAYLANDWSEGL_MAX_BACK_BUFFERS; ++index)
	    {
		if (PVR2DMemAlloc(egldisplay->context,
			      nativeWindow->strideBytes * nativeWindow->height,
	 		      128, 0,
			      &(nativeWindow->backBuffers[index])) != PVR2D_OK)
	        {
		       assert(0);
					       
		       while (--index >= 0)
				PVR2DMemFree(egldisplay->context, nativeWindow->backBuffers[index]);
			       
	               memset(nativeWindow->backBuffers, 0, sizeof(nativeWindow->backBuffers));
		       
		       nativeWindow->backBuffersValid = 0;
   		       
			       
		       return WSEGL_OUT_OF_MEMORY;
	        }
	    }
    }
    nativeWindow->backBuffersValid = 1;
    nativeWindow->currentBackBuffer = 0;
    return WSEGL_SUCCESS;
}


int wseglPixelFormatBytesPP(WSEGLPixelFormat format)
{
    if (format == WSEGL_PIXELFORMAT_565)
       return 2;
    else
    if (format == WSEGL_PIXELFORMAT_8888)
       return 4;
    else
       assert(0);
   
}


/* Create the WSEGL drawable version of a native window */
static WSEGLError wseglCreateWindowDrawable
    (WSEGLDisplayHandle display, WSEGLConfig *config,
     WSEGLDrawableHandle *drawable, NativeWindowType nativeWindow,
     WSEGLRotationAngle *rotationAngle)
{
    struct wl_egl_display *egldisplay = (struct wl_egl_display *) display;
    int index;
    /* Framebuffer */
    if (nativeWindow == NULL)
    {
       PVR2DDISPLAYINFO displayInfo;

       assert(egldisplay->display == NULL);

       /* Let's create a fake wl_egl_window to simplify code */

       nativeWindow = wl_egl_window_create(NULL, egldisplay->var.xres, egldisplay->var.yres);
       nativeWindow->format = getwseglPixelFormat(egldisplay);
       nativeWindow->display = egldisplay;

       assert(PVR2DGetDeviceInfo(egldisplay->context, &displayInfo) == PVR2D_OK);

       if (displayInfo.ulMaxFlipChains > 0 && displayInfo.ulMaxBuffersInChain > 0)
              nativeWindow->numFlipBuffers = displayInfo.ulMaxBuffersInChain;
       if (nativeWindow->numFlipBuffers > WAYLANDWSEGL_MAX_FLIP_BUFFERS)
              nativeWindow->numFlipBuffers = WAYLANDWSEGL_MAX_FLIP_BUFFERS;

       /* Workaround for broken devices, seen in debugging */
       if (nativeWindow->numFlipBuffers < 2)
              nativeWindow->numFlipBuffers = 0;
    }
    else
    {
       nativeWindow->display = egldisplay;
           nativeWindow->format = WSEGL_PIXELFORMAT_8888;
    }

    /* We can't do empty buffers, so let's make a 8x8 one. */
    if (nativeWindow->width == 0 || nativeWindow->height == 0)
    {
        nativeWindow->width = nativeWindow->height = 8;
    }


    /* If we don't have back buffers allocated already */
    if (!(nativeWindow->backBuffers[0] && nativeWindow->backBuffersValid))
    {
       nativeWindow->stridePixels = (nativeWindow->width + 7) & ~7; 
       nativeWindow->strideBytes = nativeWindow->stridePixels * wseglPixelFormatBytesPP(nativeWindow->format);

       if (allocateBackBuffers(egldisplay, nativeWindow) == WSEGL_OUT_OF_MEMORY)
          return WSEGL_OUT_OF_MEMORY;

	   PVR2DGetFrameBuffer(egldisplay->context, PVR2D_FB_PRIMARY_SURFACE, &nativeWindow->frontBufferPVRMEM);
    }      
  
    *drawable = (WSEGLDrawableHandle) nativeWindow; /* Reuse the egldisplay */
    *rotationAngle = WSEGL_ROTATE_0;
    return WSEGL_SUCCESS;
}

/* Create the WSEGL drawable version of a native pixmap */
static WSEGLError wseglCreatePixmapDrawable
    (WSEGLDisplayHandle display, WSEGLConfig *config,
     WSEGLDrawableHandle *drawable, NativePixmapType nativePixmap,
     WSEGLRotationAngle *rotationAngle)
{
    struct wl_egl_display *egldisplay = (struct wl_egl_display *) display;
    struct wwsegl_drawable_header *drawable_header = (struct wwsegl_drawable_header *)nativePixmap;

    if (drawable_header->type == WWSEGL_DRAWABLE_TYPE_DRMBUFFER)
    {
        struct wl_egl_drmbuffer *drmbuffer = (struct wl_egl_drmbuffer *)nativePixmap;
        struct wl_egl_pixmap *pixmap = wl_egl_pixmap_create(drmbuffer->width, drmbuffer->height, 0);
        pixmap->display = egldisplay;
        pixmap->stride = drmbuffer->stride;
        pixmap->name = drmbuffer->name;
        pixmap->format = WSEGL_PIXELFORMAT_8888;
        assert(PVR2DMemMap(egldisplay->context, 0, (void *)pixmap->name, &pixmap->pvrmem) == PVR2D_OK);
        *drawable = (WSEGLDrawableHandle) pixmap;         
        *rotationAngle = WSEGL_ROTATE_0;
        return WSEGL_SUCCESS;
     }
     else
     assert(0);
}

/* Delete a specific drawable */
static WSEGLError wseglDeleteDrawable(WSEGLDrawableHandle _drawable)
{
   struct wl_egl_window *drawable = (struct wl_egl_window *) _drawable;

   int index;
   int numBuffers = WAYLANDWSEGL_MAX_BACK_BUFFERS;

   if (drawable->header.type == WWSEGL_DRAWABLE_TYPE_WINDOW)
   {
       
      for (index = 0; index < numBuffers; ++index) {
         if (drawable->drmbuffers[index])
            wl_buffer_destroy(drawable->drmbuffers[index]);
         if (drawable->backBuffers[index])
            PVR2DMemFree(drawable->display->context, drawable->backBuffers[index]);
            
    }
    memset(drawable->backBuffers, 0, sizeof(drawable->backBuffers));
  
    drawable->backBuffersValid = 0;

    return WSEGL_SUCCESS;
   }
   else if (drawable->header.type == WWSEGL_DRAWABLE_TYPE_PIXMAP)
   {
      struct wl_egl_pixmap *pixmap = (struct wl_egl_pixmap *)drawable;
      PVR2DMemFree(pixmap->display->context, pixmap->pvrmem);
   }
   else assert(0);
   return WSEGL_SUCCESS;
}

/* Swap the contents of a drawable to the screen */
static WSEGLError wseglSwapDrawable
    (WSEGLDrawableHandle _drawable, unsigned long data)
{
    struct wl_egl_window *drawable = (struct wl_egl_window *) _drawable;
    struct wl_callback *callback;

    if (drawable->numFlipBuffers)
    {
        PVR2DPresentFlip(drawable->display->context, drawable->flipChain, drawable->backBuffers[drawable->currentBackBuffer], 0);
    }
    else
    {
       PVR2DBLTINFO blit;

       memset(&blit, 0, sizeof(blit));
    
       blit.CopyCode = PVR2DROPcopy;
       blit.BlitFlags = PVR2D_BLIT_DISABLE_ALL;
       blit.pSrcMemInfo = drawable->backBuffers[drawable->currentBackBuffer];
       blit.SrcStride = drawable->strideBytes;
       blit.SrcX = 0;
       blit.SrcY = 0;
       blit.SizeX = drawable->width;
       blit.SizeY = drawable->height;
       blit.SrcFormat = wsegl2pvr2dformat(drawable->format);

       blit.pDstMemInfo = drawable->frontBufferPVRMEM;
       blit.DstStride = drawable->strideBytes; 
       blit.DstX = 0;
       blit.DstY = 0;
       blit.DSizeX = drawable->width;
       blit.DSizeY = drawable->height;
       blit.DstFormat = wsegl2pvr2dformat(drawable->format);
       PVR2DBlt(drawable->display->context, &blit); 
       PVR2DQueryBlitsComplete
          (drawable->display->context, drawable->frontBufferPVRMEM, 1);                      
    }
    
    drawable->currentBackBuffer   
      = (drawable->currentBackBuffer + 1) % WAYLANDWSEGL_MAX_BACK_BUFFERS;

    return WSEGL_SUCCESS;
}

/* Set the swap interval of a window drawable */
static WSEGLError wseglSwapControlInterval
    (WSEGLDrawableHandle drawable, unsigned long interval)
{
    return WSEGL_SUCCESS;
}

 /* Flush native rendering requests on a drawable */
static WSEGLError wseglWaitNative
    (WSEGLDrawableHandle drawable, unsigned long engine)
{
    return WSEGL_SUCCESS;
}

/* Copy color data from a drawable to a native pixmap */
static WSEGLError wseglCopyFromDrawable
    (WSEGLDrawableHandle _drawable, NativePixmapType nativePixmap)
{
    return WSEGL_SUCCESS;
}

/* Copy color data from a PBuffer to a native pixmap */
static WSEGLError wseglCopyFromPBuffer
    (void *address, unsigned long width, unsigned long height,
     unsigned long stride, WSEGLPixelFormat format,
     NativePixmapType nativePixmap)
{
    return WSEGL_SUCCESS;
}

static int wseglGetBuffers(struct wl_egl_window *drawable, PVR2DMEMINFO **source, PVR2DMEMINFO **render)
{
  if (!drawable->backBuffersValid)
      return 0;
  *render = drawable->backBuffers[drawable->currentBackBuffer];
  *source = drawable->backBuffers
  [(drawable->currentBackBuffer + WAYLANDWSEGL_MAX_BACK_BUFFERS - 1) %
                 WAYLANDWSEGL_MAX_BACK_BUFFERS];
  return 1;
}                                                   


/* Return the parameters of a drawable that are needed by the EGL layer */
static WSEGLError wseglGetDrawableParameters
    (WSEGLDrawableHandle _drawable, WSEGLDrawableParams *sourceParams,
     WSEGLDrawableParams *renderParams)
{
    struct wl_egl_window *eglwindow = (struct wl_egl_window *) _drawable;
    PVR2DMEMINFO *source, *render;

    if (eglwindow->header.type == WWSEGL_DRAWABLE_TYPE_PIXMAP)
    {
        struct wl_egl_pixmap *pixmap = (struct wl_egl_pixmap *) _drawable;
        
        sourceParams->ui32Width = pixmap->width;
        sourceParams->ui32Height = pixmap->height;
        sourceParams->ui32Stride = pixmap->stride / 4;
        sourceParams->ePixelFormat = WSEGL_PIXELFORMAT_8888;   
        sourceParams->pvLinearAddress = pixmap->pvrmem->pBase;
        sourceParams->ui32HWAddress = pixmap->pvrmem->ui32DevAddr;
        sourceParams->hPrivateData = pixmap->pvrmem->hPrivateData;

        renderParams->ui32Width = pixmap->width;
        renderParams->ui32Height = pixmap->height;
        renderParams->ui32Stride = pixmap->stride / 4;
        renderParams->ePixelFormat = WSEGL_PIXELFORMAT_8888;
        renderParams->pvLinearAddress = pixmap->pvrmem->pBase;
        renderParams->ui32HWAddress = pixmap->pvrmem->ui32DevAddr;
        renderParams->hPrivateData = pixmap->pvrmem->hPrivateData;

        return WSEGL_SUCCESS;
    }

    if (!wseglGetBuffers(eglwindow, &source, &render))
    {
       return WSEGL_BAD_DRAWABLE;
    }
    
    sourceParams->ui32Width = eglwindow->width;
    sourceParams->ui32Height = eglwindow->height;
    sourceParams->ui32Stride = eglwindow->stridePixels;
    sourceParams->ePixelFormat = eglwindow->format;   
    sourceParams->pvLinearAddress = source->pBase;
    sourceParams->ui32HWAddress = source->ui32DevAddr;
    sourceParams->hPrivateData = source->hPrivateData;

    renderParams->ui32Width = eglwindow->width;
    renderParams->ui32Height = eglwindow->height;
    renderParams->ui32Stride = eglwindow->stridePixels;
    renderParams->ePixelFormat = eglwindow->format;
    renderParams->pvLinearAddress = render->pBase;
    renderParams->ui32HWAddress = render->ui32DevAddr;
    renderParams->hPrivateData = render->hPrivateData;

    return WSEGL_SUCCESS;

}

static WSEGL_FunctionTable const wseglFunctions = {
    WSEGL_VERSION,
    wseglIsDisplayValid,
    wseglInitializeDisplay,
    wseglCloseDisplay,
    wseglCreateWindowDrawable,
    wseglCreatePixmapDrawable,
    wseglDeleteDrawable,
    wseglSwapDrawable,
    wseglSwapControlInterval,
    wseglWaitNative,
    wseglCopyFromDrawable,
    wseglCopyFromPBuffer,
    wseglGetDrawableParameters
};

/* Return the table of WSEGL functions to the EGL implementation */
const WSEGL_FunctionTable *WSEGL_GetFunctionTablePointer(void)
{
    return &wseglFunctions;
}

