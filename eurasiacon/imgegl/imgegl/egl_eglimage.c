/*!****************************************************************************
@File           egl_eglimage.c

@Title          eglImage support

@Author         Imagination Technologies

@Date           2008/10/13

@Copyright      Copyright 2008 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or otherwise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform      	generic egl

@Description    eglImage implementation

******************************************************************************/
/******************************************************************************
Modifications :-
$Log: egl_eglimage.c $

 --- Revision Logs Removed --- 
******************************************************************************/
#include "egl_internal.h"
#include "drvegl.h"
#include "drveglext.h"

#include "img_types.h"
#include "tls.h"
#include "generic_ws.h"
#include "pvr_debug.h"

#if defined(EGL_EXTENSION_KHR_IMAGE)

#include "egl_eglimage.h"

#if 0
#define DBG_EGLIMAGE(A)		PVRSRVTrace A
#else
#define DBG_EGLIMAGE(A)
#endif


struct _KEGL_IMAGE_
{
	/* Next image on this display */
	KEGL_IMAGE		*psNextImage;
	
	EGLImage		sEGLImage;

	/* Number of bound targets */
	IMG_UINT32		ui32RefCount;

	/* Did we get a call to eglDestroyImage? */
	IMG_BOOL		bHandleValid;

	/* Was a drawable allocated? (used when created from pixmap) */
	IMG_HANDLE		hALlocatedDrawable;

	KEGL_DISPLAY	*psDpy;

#if defined(EGL_EXTENSION_NOK_IMAGE_SHARED)	
	IMG_BOOL		bIsSharedImageNOK;
#endif /* defined(EGL_EXTENSION_NOK_IMAGE_SHARED) */
};

#if defined(EGL_EXTENSION_NOK_IMAGE_SHARED)

#define align_to( val, align ) ((val + align-1) & ~(align-1))
#define KEGL_SHARED_IMAGE_NOK_METADATA_MAGIC 0xbeeffeeb
#define EGL_NOK_SHARED_IMAGE_XPROC_WORKAROUND

/* How EGL_NOK_image_shared is implemented:
   When application calls IMGeglCreateSharedImageNOK it creates a device memory filled with copy
   of given EGLImage with EGLimage template (KEGL_SHARED_IMAGE_NOK_METADATA) attached at the end.

   When other application imports memory it uses metadata to create EGLimage structure that points
   to imported memory */
	
/* Image template/metadata attached to exported data */
typedef struct _KEGL_SHARED_IMAGE_NOK_METADATA_
{
	/* Magic token, to decrease probability of mapping different exported mem */
	IMG_UINT32				iMagic;

	/* Template of EGLImage to be created. Contains all image metadata. */
	EGLImage				sEGLImageTemplate;
 
} KEGL_SHARED_IMAGE_NOK_METADATA;


/* Shared image created by CreateSharedImage extension */
struct _KEGL_SHARED_IMAGE_NOK_
{		
	/* Next Shared iamge */
	struct _KEGL_SHARED_IMAGE_NOK_ *psNext;

	/* Handle to exported EXPORT_AREA */
	IMG_HANDLE						hExportHandle;

	/* Meminfo of EXPORT_AREA */
	PVRSRV_CLIENT_MEM_INFO			*psExportMemInfo;

	/* Pointer to exported area */
	KEGL_SHARED_IMAGE_NOK_METADATA	*psMetadata;

	/* Display that owns this shared image */
	KEGL_DISPLAY					*psDpy;
};

static struct _KEGL_IMAGE_* ImportSharedImageNOK( SrvSysContext *psSysContext, IMG_HANDLE hExportHandle ); 

static void _SharedImageNOKDelete( KEGL_SHARED_IMAGE_NOK  *psNOKSharedImage );

static KEGL_SHARED_IMAGE_NOK_METADATA *GetSharedImageNOKMetadata( PVRSRV_CLIENT_MEM_INFO *psExportMemInfo );

#endif /* defined(EGL_EXTENSION_NOK_IMAGE_SHARED) */

/***********************************************************************************
 Function Name      : _IsEGLImage
 Inputs             : psDpy, psInputImage
 Outputs            : -
 Returns            : TRUE/FALSE
 Description        : Search the images associated with a specified display for
                      a given image to check handle validity
************************************************************************************/
static IMG_BOOL _IsEGLImage(KEGL_DISPLAY *psDpy, KEGL_IMAGE *psInputImage)
{
	KEGL_IMAGE *psImage;

	PVR_ASSERT(psDpy!=IMG_NULL);

	for (psImage=psDpy->psHeadImage; psImage!=IMG_NULL; psImage=psImage->psNextImage)
	{
		if ((psImage==psInputImage)
			 && (psImage->bHandleValid))
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : _DpyImageUnlink
 Inputs             : psDpy, psImage
 Outputs            : -
 Returns            : -
 Description        : Unlink an image from a display.
************************************************************************************/
static IMG_VOID _DpyImageUnlink(KEGL_DISPLAY *psDpy, KEGL_IMAGE *psKEGLImage)
{
	KEGL_IMAGE **ppsKEGLImage;

	PVR_ASSERT(psDpy!=IMG_NULL);
	PVR_ASSERT(psKEGLImage!=IMG_NULL);

	for (ppsKEGLImage = &psDpy->psHeadImage; 
		 *ppsKEGLImage!=IMG_NULL; 
		 ppsKEGLImage = &((*ppsKEGLImage)->psNextImage))
	{
		if (*ppsKEGLImage == psKEGLImage)
		{
			*ppsKEGLImage = psKEGLImage->psNextImage;

			return;
		}
	}
}


/***********************************************************************************
 Function Name      : _ImageDelete
 Inputs             : psKEGLImage
 Outputs            : -
 Returns            : -
 Description        : Frees an EGLImage structure and (if needed) any device memory
************************************************************************************/
static IMG_VOID _ImageDelete(KEGL_IMAGE *psKEGLImage)
{
	TLS psTls = IMGEGLGetTLSValue();
	SrvSysContext *psSysContext;

	DBG_EGLIMAGE(("_ImageDelete: Destroy KEGL_IMAGE 0x%x", psKEGLImage));

	if (psTls == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"_ImageDelete: No Current thread"));
		return;
	}

	/* If a drawable was allocated, free it now */
	if (psKEGLImage->hALlocatedDrawable)
	{
		EGLReleaseThreadLockWSEGL(psKEGLImage->psDpy, psTls);
		psKEGLImage->psDpy->pWSEGL_FT->pfnWSEGL_DeleteDrawable(psKEGLImage->hALlocatedDrawable);
		EGLReacquireThreadLockWSEGL(psKEGLImage->psDpy, psTls);

		/* Meminfo will have been freed by the call to WSEGL_DeleteDrawable() */
		psKEGLImage->sEGLImage.psMemInfo = IMG_NULL;
	}

	psSysContext = &psTls->psGlobalData->sSysContext;

	/*	EGLThreadLock(psTls); */

		_DpyImageUnlink(psKEGLImage->psDpy, psKEGLImage);

	/*	EGLThreadUnlock(psTls); */

#if defined(EGL_EXTENSION_NOK_IMAGE_SHARED) 
	if( !psKEGLImage->bIsSharedImageNOK )
#endif /* defined(EGL_EXTENSION_NOK_IMAGE_SHARED) */
	{

		/* Need to free device memory.
		 * Note that if we allocated the drawable (see above) we do not own the 
		 * meminfo, so it is not freed here, but in the call to WSEGL_DeleteDrawable() 
		 */
		if (psKEGLImage->sEGLImage.psMemInfo)
		{
			IMGEGLFREEDEVICEMEM(&psSysContext->s3D, 
								psKEGLImage->sEGLImage.psMemInfo);	
		}
	}
#if defined(EGL_EXTENSION_NOK_IMAGE_SHARED) 
	else
	{
		if (psKEGLImage->sEGLImage.psMemInfo)
		{
			PVRSRVUnmapDeviceMemory( &psSysContext->s3D, 
								psKEGLImage->sEGLImage.psMemInfo );
		}
	}
#endif /* defined(EGL_EXTENSION_NOK_IMAGE_SHARED) */

	EGLFree(psKEGLImage);
}


/***********************************************************************************
 Function Name      : BindImage
 Inputs             : hImage
 Outputs            : hImage
 Returns            : -
 Description        : Called by a client when adding an image target. 
************************************************************************************/
static IMG_VOID BindImage(IMG_VOID *hImage)
{
	KEGL_IMAGE *psKEGLImage = (KEGL_IMAGE *)hImage;

	PVR_ASSERT(psKEGLImage->bHandleValid == IMG_TRUE);

	psKEGLImage->ui32RefCount++;
	
	DBG_EGLIMAGE(("BindImage: Bind KEGL_IMAGE 0x%x (ref %d)", hImage, psKEGLImage->ui32RefCount));
}


/***********************************************************************************
 Function Name      : UnbindImage
 Inputs             : hImage
 Outputs            : hImage
 Returns            : -
 Description        : Called by a client when removing an image target
************************************************************************************/
static IMG_VOID UnbindImage(IMG_VOID *hImage)
{
	KEGL_IMAGE *psKEGLImage = (KEGL_IMAGE *)hImage;
	
	PVR_ASSERT(psKEGLImage->ui32RefCount > 0);
	
	psKEGLImage->ui32RefCount--;

	DBG_EGLIMAGE(("UnbindImage: Unbind KEGL_IMAGE 0x%x (ref %d)", hImage, psKEGLImage->ui32RefCount));

	if(psKEGLImage->ui32RefCount == 0)
	{
		_ImageDelete(psKEGLImage);
	}
}


/***********************************************************************************
 Function Name      : KEGLBindImage
 Inputs             : hImage
 Outputs            : hImage
 Returns            : -
 Description        : Called by a client when adding an image target. 
************************************************************************************/
IMG_EXPORT IMG_VOID IMG_CALLCONV KEGLBindImage(IMG_VOID *hImage)
{	
	BindImage(hImage);
}


/***********************************************************************************
 Function Name      : KEGLUnbindImage
 Inputs             : hImage
 Outputs            : hImage
 Returns            : -
 Description        : Called by a client when removing an image target
************************************************************************************/
IMG_EXPORT IMG_VOID IMG_CALLCONV KEGLUnbindImage(IMG_VOID *hImage)
{
	TLS psTls = IMGEGLGetTLSValue();
	if (psTls==IMG_NULL)
	{
		PVR_DPF((PVR_DBG_WARNING, "KEGLUnbindImage: EGL not initialised"));
		return;
	}

	EGLThreadLock(psTls);
	UnbindImage(hImage);
	EGLThreadUnlock(psTls);
}


/***********************************************************************************
 Function Name      : KEGLGetImageSource
 Inputs             : hEGLImage
 Outputs            : ppsImage
 Returns            : IMG_TRUE is hEGLImage is a valid EGLImage handle
 Description        : Validates the hEGLImage handle and returns and EGLImage
                      structure for that handle.
************************************************************************************/
IMG_EXPORT IMG_BOOL IMG_CALLCONV KEGLGetImageSource(EGLImageKHR    hEGLImage, 
													EGLImage    ** ppsImage)
{
	KEGL_IMAGE * psKEGLImage;
	IMG_UINT32   i;
	TLS          psTls;

	psTls = IMGEGLGetTLSValue();

	if (psTls==IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLGetImageSource: No Current thread"));
		return IMG_FALSE;
	}

	psKEGLImage = (KEGL_IMAGE *)hEGLImage;

	for (i=0; i<EGL_MAX_NUM_DISPLAYS; i++)
	{
		KEGL_DISPLAY *psDpy;

		psDpy = &psTls->psGlobalData->asDisplay[i];

		if(psDpy->isInitialised)
		{
			if(_IsEGLImage(psDpy, psKEGLImage))
			{
				*ppsImage = &psKEGLImage->sEGLImage;
				return IMG_TRUE;
			}
		}
	}

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : IMGeglCreateImageKHR
 Inputs             : dpy, ctx, target, buffer, attr_list)
 Outputs            : -
 Returns            : Pointer to an EGLImage
 Description        : Create an EGLImage
************************************************************************************/
IMG_INTERNAL EGLImageKHR IMGeglCreateImageKHR(EGLDisplay        eglDpy, 
											  EGLContext        eglContext, 
											  EGLenum           target, 
											  EGLClientBuffer   buffer, 
											  EGLint          * pAttribList)
{

	IMG_UINT32      ui32Level;
	KEGL_DISPLAY    *psDpy;
	KEGL_IMAGE      *psKEGLImage;
	TLS             psTls;
	KEGL_CONTEXT    *psCtx = (KEGL_CONTEXT *)eglContext;

	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglCreateImageKHR"));

	psTls = TLS_Open(_TlsInit);

	if (psTls==IMG_NULL)
	{
		return EGL_NO_IMAGE_KHR;
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglCreateImageKHR);

	psTls->lastError = EGL_SUCCESS;

	psDpy = GetKEGLDisplay(psTls, eglDpy);

	if (!psDpy)
	{
		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

		return EGL_NO_IMAGE_KHR;
	}

	if(!IsEGLContext(psDpy, psCtx))
	{
		psTls->lastError = EGL_BAD_CONTEXT;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

		return EGL_NO_IMAGE_KHR;
	}

	/* Default to 0 */
	ui32Level = 0;

	if (pAttribList!=IMG_NULL)
	{
		while (*pAttribList!=EGL_NONE)
		{
			EGLint attrib, value;

			attrib = *pAttribList++;
			value = *pAttribList++;

			switch (attrib)
			{
				case EGL_IMAGE_PRESERVED_KHR:
				{
					/* We always preserve */
					break;
				}

#if defined(EGL_EXTENSION_KHR_GL_TEXTURE_2D_IMAGE) || defined(EGL_EXTENSION_KHR_GL_TEXTURE_CUBEMAP_IMAGE) || defined(EGL_EXTENSION_KHR_GL_RENDERBUFFER_IMAGE)
				case EGL_GL_TEXTURE_LEVEL_KHR:
				{
					ui32Level = (IMG_UINT32)value;

					break;
				}
#endif /* defined(EGL_EXTENSION_KHR_GL_TEXTURE_2D_IMAGE) || defined(EGL_EXTENSION_KHR_GL_TEXTURE_CUBEMAP_IMAGE) || defined(EGL_EXTENSION_KHR_GL_RENDERBUFFER_IMAGE) */
				default:
				{
					psTls->lastError = EGL_BAD_PARAMETER;
		
					IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

					return EGL_NO_IMAGE_KHR;
				}
			}
		}
	}

	switch(target)
	{
#if defined(ANDROID)
		case EGL_NATIVE_BUFFER_ANDROID:
#else
		case EGL_NATIVE_PIXMAP_KHR:
#endif
		{
			WSEGLDrawableParams sSourceParams, sRenderParams;
			WSEGLError eError;
			EGLImage *psEGLImage;
			WSEGLRotationAngle ignored;

			if(eglContext!=EGL_NO_CONTEXT)
			{
				psTls->lastError = EGL_BAD_CONTEXT;	

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}
#if 0
//       * If the resource specified by <dpy>, <ctx>, <target>, <buffer> and
//         <attr_list> is itself an EGLImage sibling, the error
//         EGL_BAD_ACCESS is generated.
#endif

			psKEGLImage = EGLCalloc(sizeof(KEGL_IMAGE));

			if (psKEGLImage == 0)
			{
				psTls->lastError = EGL_BAD_ALLOC;

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}

			EGLThreadLockWSEGL(psDpy, psTls);

			/* Create pixmap */
			eError = psDpy->pWSEGL_FT->pfnWSEGL_CreatePixmapDrawable(psDpy->hDisplay, 
																	 IMG_NULL,
																	 &psKEGLImage->hALlocatedDrawable, 
																	 (NativePixmapType)buffer, 
																	 &ignored);
			if (!eError)
			{
				eError = psDpy->pWSEGL_FT->pfnWSEGL_GetDrawableParameters(psKEGLImage->hALlocatedDrawable, 
																		  &sSourceParams, 
																		  &sRenderParams);
			}
			else
			{
				PVRSRVMemSet(&sRenderParams,0,sizeof(WSEGLDrawableParams));
			}
			
			if (eError!=WSEGL_SUCCESS)
			{
				psTls->lastError = EGL_BAD_PARAMETER;

				if (psKEGLImage->hALlocatedDrawable)
				{
					psDpy->pWSEGL_FT->pfnWSEGL_DeleteDrawable(psKEGLImage->hALlocatedDrawable);

					psKEGLImage->hALlocatedDrawable = (WSEGLDrawableHandle)0;
				}

				EGLFree(psKEGLImage);

				EGLThreadUnlockWSEGL(psDpy, psTls);

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}

			EGLThreadUnlockWSEGL(psDpy, psTls);

			psEGLImage = &psKEGLImage->sEGLImage;

			switch(sRenderParams.ePixelFormat)
			{
				case WSEGL_PIXELFORMAT_ARGB8888:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_ARGB8888;

					break;
				}
				case WSEGL_PIXELFORMAT_ABGR8888:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_ABGR8888;

					break;
				}
				case WSEGL_PIXELFORMAT_XRGB8888:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_XRGB8888;

					break;
				}
				case WSEGL_PIXELFORMAT_XBGR8888:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_XBGR8888;

					break;
				}
				case WSEGL_PIXELFORMAT_1555:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_ARGB1555;

					break;
				}
				case WSEGL_PIXELFORMAT_4444:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_ARGB4444;

					break;
				}
				case WSEGL_PIXELFORMAT_YUYV:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV;
					break;
				}
				case WSEGL_PIXELFORMAT_UYVY:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY;
					break;
				}
				case WSEGL_PIXELFORMAT_NV12:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_NV12;
					break;
				}
				case WSEGL_PIXELFORMAT_YV12:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_YV12;
					break;
				}
				default:
				{
					psEGLImage->ePixelFormat = PVRSRV_PIXEL_FORMAT_RGB565;

					break;
				}
			}

			psEGLImage->ui32Width            = sRenderParams.ui32Width;
			psEGLImage->ui32Height           = sRenderParams.ui32Height;

			/* Note: psEGLImage->ui32Stride is in bytes, 
			 * sRenderParams.ui32Stride is in pixels
			 */
			psEGLImage->ui32Stride           = sRenderParams.ui32Stride << aWSEGLPixelFormatBytesPerPixelShift[sRenderParams.ePixelFormat];;
			psEGLImage->pvLinSurfaceAddress  = sRenderParams.pvLinearAddress;
			psEGLImage->ui32HWSurfaceAddress = sRenderParams.ui32HWAddress;
#if defined(SUPPORT_NV12_FROM_2_HWADDRS)
			psEGLImage->ui32HWSurfaceAddress2 = sRenderParams.ui32HWAddress2;
#endif
			psEGLImage->ui32Flags = (sRenderParams.ulFlags & WSEGL_FLAGS_YUV_FULL_RANGE) ? 
										EGLIMAGE_FLAGS_YUV_FULL_RANGE : EGLIMAGE_FLAGS_YUV_CONFORMANT_RANGE;
			psEGLImage->ui32Flags |= (sRenderParams.ulFlags & WSEGL_FLAGS_YUV_BT709) ? EGLIMAGE_FLAGS_YUV_BT709 : EGLIMAGE_FLAGS_YUV_BT601;

			psEGLImage->hImage               = (IMG_VOID *)psKEGLImage;
			
			psKEGLImage->bHandleValid = IMG_TRUE;

			BindImage(psKEGLImage);         /* bind EGLImageKHR to KEGL_IMAGE */

			psEGLImage->ui32Target           = (IMG_UINT32)target;
			psEGLImage->ui32Buffer           = (IMG_UINT32)buffer;
			psEGLImage->ui32Level            = ui32Level;
				
			psEGLImage->psMemInfo = (PVRSRV_CLIENT_MEM_INFO *)(sRenderParams.hPrivateData);

			if(sRenderParams.ulFlags & WSEGL_FLAGS_EGLIMAGE_COMPOSITION_SYNC)
			{
				psEGLImage->ui32Flags |= EGLIMAGE_FLAGS_COMPOSITION_SYNC;
			}

			psKEGLImage->psDpy = psDpy;

			EGLThreadLock(psTls);

			/* Chain the new image onto the displays image list */
			psKEGLImage->psNextImage = psDpy->psHeadImage;
			psDpy->psHeadImage = psKEGLImage;

			EGLThreadUnlock(psTls);

			IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

		
			return (EGLImageKHR)psKEGLImage;
		}

#if defined(EGL_EXTENSION_KHR_GL_TEXTURE_2D_IMAGE)
		case EGL_GL_TEXTURE_2D_KHR:
#endif /* defined(EGL_EXTENSION_KHR_GL_TEXTURE_2D_IMAGE) */

#if defined(EGL_EXTENSION_KHR_GL_TEXTURE_CUBEMAP_IMAGE)
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR:
		case EGL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR:
#endif /* defined(EGL_EXTENSION_KHR_GL_TEXTURE_CUBEMAP_IMAGE) */

#if defined(EGL_EXTENSION_KHR_GL_RENDERBUFFER_IMAGE)
		case EGL_GL_RENDERBUFFER_KHR:
#endif /* defined(EGL_EXTENSION_KHR_GL_RENDERBUFFER_IMAGE) */

		{
#if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)
			IMG_EGLERROR eError;
#endif /* defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED) */

			if(eglContext==EGL_NO_CONTEXT)
			{
				psTls->lastError = EGL_BAD_CONTEXT;
	
				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}

			psKEGLImage = EGLCalloc(sizeof(KEGL_IMAGE));

			if (psKEGLImage == 0)
			{
				psTls->lastError = EGL_BAD_ALLOC;

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}

#if defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED)
			if (
				(psCtx->eContextType == IMGEGL_CONTEXT_OPENGLES1) &&
				psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1]
			)
			{
				PVR_ASSERT(psTls->psGlobalData->bHaveOGLES1Functions)
				PVR_ASSERT(psTls->psGlobalData->spfnOGLES1.pfnGLESGetImageSource)

				eError = psTls->psGlobalData->spfnOGLES1.pfnGLESGetImageSource(psCtx->hClientContext,
																			   (IMG_UINT32)target,
																				(IMG_UINT32)buffer,
																				ui32Level,
																				&psKEGLImage->sEGLImage);
			}
			else
#endif /* defined(SUPPORT_OPENGLES1) || defined(API_MODULES_RUNTIME_CHECKED) */

#if defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)
			if (
				(psCtx->eContextType == IMGEGL_CONTEXT_OPENGLES2) &&
				psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES2]
			)
			{
				PVR_ASSERT(psTls->psGlobalData->bHaveOGLES2Functions)
				PVR_ASSERT(psTls->psGlobalData->spfnOGLES2.pfnGLESGetImageSource)

				eError = psTls->psGlobalData->spfnOGLES2.pfnGLESGetImageSource(psCtx->hClientContext,
																			   (IMG_UINT32)target,
																				(IMG_UINT32)buffer,
																				ui32Level,
																				&psKEGLImage->sEGLImage);
			}
			else
#endif /* defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED) */

			{
				PVR_DPF((PVR_DBG_WARNING,"IMGeglCreateContext: Invalid client API"));

				EGLFree(psKEGLImage);

				psTls->lastError = EGL_BAD_MATCH;

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}

#if defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED)

#   if defined(API_MODULES_RUNTIME_CHECKED)
			if(
				psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES1] ||
				psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENGLES2]
			)
#   endif
			{
				switch(eError)
				{
					case IMG_EGL_NO_ERROR:
					{
						psKEGLImage->psDpy = psDpy;

						EGLThreadLock(psTls);

						/* Chain the new image onto the displays image list */
						psKEGLImage->psNextImage = psDpy->psHeadImage;
						psDpy->psHeadImage = psKEGLImage;

						psKEGLImage->sEGLImage.hImage = (IMG_VOID *)psKEGLImage;
						psKEGLImage->bHandleValid = IMG_TRUE;

						/* Increment reference counter twice */
						BindImage(psKEGLImage);     /* bind EGLImageKHR to KEGL_IMAGE */
						BindImage(psKEGLImage);     /* bind texture to KEGL_IMAGE */

						psKEGLImage->sEGLImage.ui32Target = (IMG_UINT32)target;
						psKEGLImage->sEGLImage.ui32Buffer = (IMG_UINT32)buffer;
						psKEGLImage->sEGLImage.ui32Level  = ui32Level;

						EGLThreadUnlock(psTls);

						IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

						return (EGLImageKHR)psKEGLImage;
					}
					case IMG_EGL_BAD_MATCH:
					{
						psTls->lastError = EGL_BAD_MATCH;

						break;
					}
					case IMG_EGL_BAD_PARAMETER:
					{
						psTls->lastError = EGL_BAD_PARAMETER;

						break;
					}
					case IMG_EGL_BAD_ACCESS:
					{
						psTls->lastError = EGL_BAD_ACCESS;

						break;
					}
					default:
					{
						PVR_DPF((PVR_DBG_ERROR, "IMGeglCreateImageKHR: Generic error"));

						psTls->lastError = EGL_BAD_PARAMETER;

						break;
					}
				}
			
				EGLFree(psKEGLImage);

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}
#endif /* defined(SUPPORT_OPENGLES1) || defined(SUPPORT_OPENGLES2) || defined(API_MODULES_RUNTIME_CHECKED) */

		}

#if defined(EGL_EXTENSION_KHR_VG_PARENT_IMAGE)
		case EGL_VG_PARENT_IMAGE_KHR:
		{

#if  defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED)

#   if defined(API_MODULES_RUNTIME_CHECKED)
			if(psTls->psGlobalData->bApiModuleDetected[IMGEGL_CONTEXT_OPENVG])
#   endif
			{
				IMG_EGLERROR eError;

				if((eglContext == EGL_NO_CONTEXT) || psCtx->eContextType != IMGEGL_CONTEXT_OPENVG)
				{
					psTls->lastError = EGL_BAD_MATCH;
	
					IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

					return EGL_NO_IMAGE_KHR;
				}

				psKEGLImage = EGLCalloc(sizeof(KEGL_IMAGE));

				if (psKEGLImage == 0)
				{
					psTls->lastError = EGL_BAD_ALLOC;

					IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

					return EGL_NO_IMAGE_KHR;
				}

				PVR_ASSERT(psTls->psGlobalData->bHaveOVGFunctions)
				PVR_ASSERT(psTls->psGlobalData->spfnOVG.pfnVGGetImageSource)

				eError = psTls->psGlobalData->spfnOVG.pfnVGGetImageSource(psCtx->hClientContext,
																		 (IMG_UINT32)target,
																		 (IMG_UINT32)buffer,
																		 &psKEGLImage->sEGLImage);
				switch(eError)
				{
					case IMG_EGL_NO_ERROR:
					{
						psKEGLImage->psDpy = psDpy;

						EGLThreadLock(psTls);

						/* Chain the new image onto the displays image list */
						psKEGLImage->psNextImage = psDpy->psHeadImage;
						psDpy->psHeadImage = psKEGLImage;

						psKEGLImage->sEGLImage.hImage = (IMG_VOID *)psKEGLImage;
						psKEGLImage->bHandleValid = IMG_TRUE;
	
						/* Increment reference counter twice */
						BindImage(psKEGLImage);     /* bind EGLImageKHR to KEGL_IMAGE */
						BindImage(psKEGLImage);     /* bind vgImage to KEGL_IMAGE */

						EGLThreadUnlock(psTls);

						IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

						return (EGLImageKHR)psKEGLImage;
					}
					case IMG_EGL_BAD_MATCH:
					{
						psTls->lastError = EGL_BAD_MATCH;

						break;
					}
					case IMG_EGL_BAD_PARAMETER:
					{
						psTls->lastError = EGL_BAD_PARAMETER;

						break;
					}
					case IMG_EGL_BAD_ACCESS:
					{
						psTls->lastError = EGL_BAD_ACCESS;

						break;
					}
					default:
					{
						PVR_DPF((PVR_DBG_ERROR, "IMGeglCreateImageKHR: Generic error"));

						psTls->lastError = EGL_BAD_PARAMETER;

						break;
					}
				}

				EGLFree(psKEGLImage);

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}

#   if defined(API_MODULES_RUNTIME_CHECKED)
			else
			{
				psTls->lastError = EGL_BAD_PARAMETER;

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}
#   endif

#else /* defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) */

			psTls->lastError = EGL_BAD_PARAMETER;

			IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

			return EGL_NO_IMAGE_KHR;

#endif /* defined(SUPPORT_OPENVG) || defined(SUPPORT_OPENVGX) || defined(API_MODULES_RUNTIME_CHECKED) */
		}
#endif /* defined(EGL_EXTENSION_KHR_VG_PARENT_IMAGE) */

#if defined(EGL_EXTENSION_IMG_CL_IMAGE)
		case EGL_CL_IMAGE_IMG:
		{
#if  defined(SUPPORT_OPENCL)
			IMG_EGLERROR eError;

			if(eglContext == EGL_NO_CONTEXT)
			{
				psTls->lastError = EGL_BAD_MATCH;

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}

			psKEGLImage = EGLCalloc(sizeof(KEGL_IMAGE));

			if (psKEGLImage == 0)
			{
				psTls->lastError = EGL_BAD_ALLOC;

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}

			PVR_ASSERT(psTls->psGlobalData->bHaveOCLFunctions)
			PVR_ASSERT(psTls->psGlobalData->spfnOCL.pfnCLGetImageSource)

			eError = psTls->psGlobalData->spfnOCL.pfnCLGetImageSource(psCtx->hClientContext,
																	 (IMG_UINT32)target,
																	 (IMG_UINT32)buffer,
																	 &psKEGLImage->sEGLImage);
			switch(eError)
			{
				case IMG_EGL_NO_ERROR:
				{
					psKEGLImage->psDpy = psDpy;

					EGLThreadLock(psTls);

					/* Chain the new image onto the displays image list */
					psKEGLImage->psNextImage = psDpy->psHeadImage;
					psDpy->psHeadImage = psKEGLImage;

					psKEGLImage->sEGLImage.hImage = (IMG_VOID *)psKEGLImage;
					psKEGLImage->bHandleValid = IMG_TRUE;
	
					/* Increment reference counter twice */
					BindImage(psKEGLImage);     /* bind EGLImageKHR to KEGL_IMAGE */
					BindImage(psKEGLImage);     /* bind vgImage to KEGL_IMAGE */

					EGLThreadUnlock(psTls);

					IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

					return (EGLImageKHR)psKEGLImage;
				}
				case IMG_EGL_BAD_MATCH:
				{
					psTls->lastError = EGL_BAD_MATCH;

					break;
				}
				case IMG_EGL_BAD_PARAMETER:
				{
					psTls->lastError = EGL_BAD_PARAMETER;

					break;
				}
				case IMG_EGL_BAD_ACCESS:
				{
					psTls->lastError = EGL_BAD_ACCESS;

					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_ERROR, "IMGeglCreateImageKHR: Generic error"));

					psTls->lastError = EGL_BAD_PARAMETER;

					break;
				}
			}

			EGLFree(psKEGLImage);

			IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

			return EGL_NO_IMAGE_KHR;
#else /* defined(SUPPORT_OPENCL) */
			psTls->lastError = EGL_BAD_PARAMETER;

			IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

			return EGL_NO_IMAGE_KHR;
#endif /* defined(SUPPORT_OPENCL) */
		}
#endif /* defined(EGL_EXTENSION_IMG_CL_IMAGE) */

#if defined(EGL_EXTENSION_NOK_IMAGE_SHARED) 
		case EGL_SHARED_IMAGE_NOK:
		{
			struct _KEGL_IMAGE_ *psSharedImage = ImportSharedImageNOK( &psTls->psGlobalData->sSysContext, (IMG_HANDLE)buffer );

			if( !psSharedImage ) 
			{
				psTls->lastError = EGL_BAD_PARAMETER;

				IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

				return EGL_NO_IMAGE_KHR;
			}

			psSharedImage->psDpy = psDpy;

			/* Chain the new image onto the displays image list */
			EGLThreadLock(psTls);
			psSharedImage->psNextImage = psDpy->psHeadImage;
			psDpy->psHeadImage = psSharedImage;
			EGLThreadUnlock(psTls);   

			IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

			return (EGLImageKHR)psSharedImage;
		}
#endif /* defined(EGL_EXTENSION_NOK_IMAGE_SHARED) */
		default:
		{
			psTls->lastError = EGL_BAD_PARAMETER;

			IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateImageKHR);

			return EGL_NO_IMAGE_KHR;
		}
	}
}	


/***********************************************************************************
 Function Name      : IMGeglDestroyImageKHR
 Inputs             : dpy, image
 Outputs            : -
 Returns            : EGL_TRUE - Success
                      EGL_FALSE - Failure.
 Description        : Destroy an EGLImage
************************************************************************************/
IMG_INTERNAL EGLBoolean IMGeglDestroyImageKHR(EGLDisplay eglDpy, EGLImageKHR eglImage)
{

	KEGL_DISPLAY * psDpy;
	TLS            psTls;
	KEGL_IMAGE   * psKEGLImage = (KEGL_IMAGE *)eglImage;

	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglDestroyImageKHR"));

	psTls = TLS_Open(_TlsInit);

	if (psTls==IMG_NULL)
	{
		return EGL_FALSE;
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglDestroyImageKHR);

	psTls->lastError = EGL_SUCCESS;
	
	psDpy = GetKEGLDisplay(psTls, eglDpy);
	if (!psDpy)
	{
		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroyImageKHR);
		return EGL_FALSE;
	}

	EGLThreadLock(psTls);
	if(!_IsEGLImage(psDpy, psKEGLImage))
	{
		psTls->lastError = EGL_BAD_PARAMETER;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroyImageKHR);
		EGLThreadUnlock(psTls);
		return EGL_FALSE;
	}

	if(psDpy != psKEGLImage->psDpy)
	{
		psTls->lastError = EGL_BAD_MATCH;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroyImageKHR);
		EGLThreadUnlock(psTls);
		return EGL_FALSE;
	}

//	EGLThreadLock(psTls);

	/* Mark the handle as invalid, they KEGL_IMAGE may still be alive 
	 * if targets are alive, but the handle should not be used to 
	 * add any more targets 
	 */
	psKEGLImage->bHandleValid = IMG_FALSE;

	UnbindImage(psKEGLImage);

	EGLThreadUnlock(psTls);

	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroyImageKHR);

	return EGL_TRUE;
}	


/***********************************************************************************
 Function Name      : TerminateImages
 Inputs             : psDpy 
 Outputs            : -
 Returns            : void
 Description        : call destroy on remaining eglImages.
************************************************************************************/
IMG_INTERNAL IMG_VOID TerminateImages(KEGL_DISPLAY * psDpy)
{
	KEGL_IMAGE *psKEGLImage = psDpy->psHeadImage;

	/* search the images associated with a specified display and mark or delete */
	while(psKEGLImage)
	{
		KEGL_IMAGE *psNextKEGLImage = psKEGLImage->psNextImage;

		if(psKEGLImage->psDpy == psDpy)
		{
			/* if it's used release later otherwise delete */
			if (psKEGLImage->bHandleValid)
			{
				/* Do an "IMGeglDestroyImageKHR" without all the checking */
				psKEGLImage->bHandleValid = IMG_FALSE;

				UnbindImage(psKEGLImage);
			}
		}

		psKEGLImage = psNextKEGLImage;
	}	

#if defined(EGL_EXTENSION_NOK_IMAGE_SHARED)
	/* destroy all SharedImageNOK objects */
	{
		KEGL_SHARED_IMAGE_NOK *psKEGLSharedImageNOK = psDpy->psHeadSharedImageNOK;

		while(psKEGLSharedImageNOK)
		{
			KEGL_SHARED_IMAGE_NOK *psKEGLNextSharedImageNOK = psKEGLSharedImageNOK->psNext;
			_SharedImageNOKDelete( psKEGLSharedImageNOK );
			psKEGLSharedImageNOK = psKEGLNextSharedImageNOK;
		}	
	}

#endif /* defined(EGL_EXTENSION_NOK_IMAGE_SHARED) */
}


#else /* defined(EGL_EXTENSION_KHR_IMAGE) */


#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

#if defined(EGL_EXTENSION_NOK_IMAGE_SHARED)


/***********************************************************************************
 Function Name      : FindSharedImageNOKExport
 Inputs             : psSysContext, hNokExport
 Outputs            : -
 Returns            : Pointer to meminfo
 Description        : Find SharedImageNOK Export area (Image data + metadata) using export handle.
************************************************************************************/
static PVRSRV_CLIENT_MEM_INFO* FindSharedImageNOKExport( SrvSysContext *psSysContext, IMG_HANDLE hNokExport )
{
	PVRSRV_ERROR eResult;
	PVRSRV_CLIENT_MEM_INFO* psMemInfo;

	eResult = PVRSRVMapDeviceMemory(&psSysContext->s3D,
									hNokExport,
									psSysContext->hGeneralHeap,
									&psMemInfo);
	if(eResult != PVRSRV_OK) 
	{
		PVR_DPF((PVR_DBG_ERROR, "FindSharedImageNOKExport: Failed to map memory"));	
		return NULL;
	}
   
	/* Check magic token */
	if( GetSharedImageNOKMetadata(psMemInfo)->iMagic != KEGL_SHARED_IMAGE_NOK_METADATA_MAGIC ) 
	{
		PVR_DPF((PVR_DBG_ERROR, "FindSharedImageNOKExport: Failed to recognize memory as shared image"));	
		PVRSRVUnmapDeviceMemory(&psSysContext->s3D, psMemInfo );
		return NULL;
	}

	return psMemInfo;
}


/***********************************************************************************
 Function Name      : GetSharedImageNOKMetadata
 Inputs             : psExportMemInfo
 Outputs            : -
 Returns            : Pointer to metadata of exported image
 Description        : Returns pointer of metadata contained inside of exported memory
************************************************************************************/
static KEGL_SHARED_IMAGE_NOK_METADATA *GetSharedImageNOKMetadata( PVRSRV_CLIENT_MEM_INFO *psExportMemInfo ) 
{
	return (KEGL_SHARED_IMAGE_NOK_METADATA *)( (char*)psExportMemInfo->pvLinAddr + psExportMemInfo->uAllocSize - 
											   sizeof( KEGL_SHARED_IMAGE_NOK_METADATA ) );
}


/***********************************************************************************
 Function Name      : CreateSharedImageNOKTemplate
 Inputs             : psKEGLSharedImage, pSrc, psSysContext
 Outputs            : -
 Returns            : IMG_TRUE is everything went fine
 Description        : Store data of pSrc EGLImage as template to create new 
                      EGLImages using EGL_SHARED_IMAGE_NOK. It also allocates
                      storage (device memory) for shared data (image content)
                      and fills it.
************************************************************************************/
static IMG_BOOL CreateSharedImageNOKTemplate( KEGL_SHARED_IMAGE_NOK *psKEGLSharedImage, EGLImage *pSrc, SrvSysContext *psSysContext ) 
{
	EGLImage *pDest;
	IMG_UINT32 ui32ExportSize = align_to(pSrc->psMemInfo->uAllocSize, 32);
	IMG_UINT32 ui32AllocFlags = PVRSRV_MEM_READ | PVRSRV_MEM_WRITE;

	/* Add space for metadata */
	ui32ExportSize += sizeof(KEGL_SHARED_IMAGE_NOK_METADATA);
	ui32ExportSize = align_to(ui32ExportSize, 4096);
	
	psKEGLSharedImage->psExportMemInfo = IMG_NULL;
#if defined(EGL_NOK_SHARED_IMAGE_XPROC_WORKAROUND)
	ui32AllocFlags |= PVRSRV_MEM_XPROC;
#endif
	if(PVRSRVAllocDeviceMem(&psSysContext->s3D, psSysContext->hGeneralHeap,
							ui32AllocFlags,
							ui32ExportSize,
							4096,
							&psKEGLSharedImage->psExportMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "CreateSharedImageNOKTemplate: Failed to allocate device memory"));	
		goto err;
	}

	psKEGLSharedImage->hExportHandle = IMG_NULL;
	if(PVRSRVExportDeviceMem(&psSysContext->s3D, 
							  psKEGLSharedImage->psExportMemInfo,
							  &psKEGLSharedImage->hExportHandle) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "CreateSharedImageNOKTemplate: Failed to export device memory"));
		goto err;
	}

	/* Initialize metadata */
	psKEGLSharedImage->psMetadata = GetSharedImageNOKMetadata( psKEGLSharedImage->psExportMemInfo );
	PVRSRVMemSet( psKEGLSharedImage->psMetadata, 0, sizeof( *psKEGLSharedImage->psMetadata ) );
	psKEGLSharedImage->psMetadata->iMagic = KEGL_SHARED_IMAGE_NOK_METADATA_MAGIC;
	pDest = &psKEGLSharedImage->psMetadata->sEGLImageTemplate;

	/* Set metadata template fields */
	pDest->ui32Width = pSrc->ui32Width;
	pDest->ui32Height = pSrc->ui32Height;
	pDest->ePixelFormat = pSrc->ePixelFormat;
	pDest->ui32Stride = pSrc->ui32Stride;

	pDest->psMemInfo = psKEGLSharedImage->psExportMemInfo;
	pDest->pvLinSurfaceAddress = psKEGLSharedImage->psExportMemInfo->pvLinAddr;
	pDest->ui32HWSurfaceAddress = psKEGLSharedImage->psExportMemInfo->sDevVAddr.uiAddr;

	pDest->ui32Target = pSrc->ui32Target;
	pDest->ui32Buffer = pSrc->ui32Buffer;
	pDest->ui32Level = pSrc->ui32Level;
	pDest->bTwiddled = pSrc->bTwiddled;

	/* If it's twiddled - just copy, if not - try to twiddle it */
	PVRSRVMemCopy( pDest->psMemInfo->pvLinAddr, pSrc->psMemInfo->pvLinAddr, pSrc->psMemInfo->uAllocSize );

	return IMG_TRUE;

 err:
	if (psKEGLSharedImage->psExportMemInfo != IMG_NULL)
	{
		PVRSRVFreeDeviceMem(&psSysContext->s3D, psKEGLSharedImage->psExportMemInfo);
	}

	return IMG_FALSE;
}



/***********************************************************************************
 Function Name      : EGLImageFromSharedImageNOKTemplate
 Inputs             : pDest, pExport, psDataMemInfo
 Outputs            : -
 Returns            : -
 Description        : Use template stored in petadata and meminfo of exported image 
                      data to fill fields of EGLImage struct.
************************************************************************************/
static IMG_VOID EGLImageFromSharedImageNOKTemplate(EGLImage *pDest,
													KEGL_SHARED_IMAGE_NOK_METADATA *pMetadata,
													PVRSRV_CLIENT_MEM_INFO* psDataMemInfo )
{
	/* Get information from template */
	pDest->ui32Width = pMetadata->sEGLImageTemplate.ui32Width;
	pDest->ui32Height = pMetadata->sEGLImageTemplate.ui32Height;
	pDest->ePixelFormat = pMetadata->sEGLImageTemplate.ePixelFormat;
	pDest->ui32Stride = pMetadata->sEGLImageTemplate.ui32Stride;
	pDest->ui32Target = pMetadata->sEGLImageTemplate.ui32Target;
	pDest->ui32Buffer = pMetadata->sEGLImageTemplate.ui32Buffer;
	pDest->ui32Level = pMetadata->sEGLImageTemplate.ui32Level;
	pDest->bTwiddled = pMetadata->sEGLImageTemplate.bTwiddled;   

	/* Setup imported data meminfo */
	pDest->psMemInfo = psDataMemInfo;
	pDest->pvLinSurfaceAddress = pDest->psMemInfo->pvLinAddr;
	pDest->ui32HWSurfaceAddress = pDest->psMemInfo->sDevVAddr.uiAddr;
}


/***********************************************************************************
 Function Name      : ImportSharedImageNOK
 Inputs             : psSysContext, hExportHandle
 Outputs            : -
 Returns            : KEGL_IMAGE *
 Description        : Using handle of Shared image export area this function creates
                      KEGL_IMAGE structure that's a reference to original one.
************************************************************************************/
static struct _KEGL_IMAGE_* ImportSharedImageNOK( SrvSysContext *psSysContext, IMG_HANDLE hExportHandle ) 
{	
	struct _KEGL_IMAGE_ *psSharedImage;
	PVRSRV_CLIENT_MEM_INFO *psExportMemInfo = FindSharedImageNOKExport( psSysContext, hExportHandle );
	KEGL_SHARED_IMAGE_NOK_METADATA *psMetadata;

	if(! psExportMemInfo ) 
	{
		return NULL;
	}

	psMetadata = GetSharedImageNOKMetadata( psExportMemInfo );

	psSharedImage = EGLMalloc( sizeof(*psSharedImage) );
	PVRSRVMemSet( psSharedImage, 0 , sizeof( *psSharedImage ) );

	psSharedImage->bIsSharedImageNOK = IMG_TRUE;
	psSharedImage->bHandleValid = IMG_TRUE;

	EGLImageFromSharedImageNOKTemplate( &psSharedImage->sEGLImage, psMetadata, psExportMemInfo );

	psSharedImage->sEGLImage.hImage = psSharedImage;

	return psSharedImage;
}


/***********************************************************************************
 Function Name      : _DpySharedImageNOKUnlink
 Inputs             : psDpy, psNOKSharedImage
 Outputs            : -
 Returns            : -
 Description        : Unlink an NOK Shared Image from a display.
************************************************************************************/
static IMG_VOID _DpySharedImageNOKUnlink(KEGL_DISPLAY *psDpy, KEGL_SHARED_IMAGE_NOK *psNOKSharedImage)
{
	KEGL_SHARED_IMAGE_NOK **ppsNOKSharedImage;

	PVR_ASSERT(psDpy!=IMG_NULL);
	PVR_ASSERT(psNOKSharedImage!=IMG_NULL);

	for (ppsNOKSharedImage = &psDpy->psHeadSharedImageNOK; 
		 *ppsNOKSharedImage!=IMG_NULL; 
		 ppsNOKSharedImage = &((*ppsNOKSharedImage)->psNext))
	{
		if (*ppsNOKSharedImage == psNOKSharedImage)
		{
			*ppsNOKSharedImage = psNOKSharedImage->psNext;
			return;
		}
	}
}



/***********************************************************************************
 Function Name      : _SharedImageNOKDelete
 Inputs             : psNOKSharedImage
 Outputs            : -
 Returns            : -
 Description        : Frees an KEGL_SHARED_IMAGE_NOK structure and (if needed) any device memory
************************************************************************************/
static void _SharedImageNOKDelete( KEGL_SHARED_IMAGE_NOK  *psNOKSharedImage ) 
{
	TLS psTls = IMGEGLGetTLSValue();
	SrvSysContext *psSysContext;

	if (psTls == IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"_SharedImageNOKDelete: No Current thread"));
		return;
	}

	psSysContext = &psTls->psGlobalData->sSysContext;

	PVRSRVFreeDeviceMem(&psSysContext->s3D, psNOKSharedImage->psExportMemInfo );

	_DpySharedImageNOKUnlink( psNOKSharedImage->psDpy, psNOKSharedImage );

	EGLFree( psNOKSharedImage );
}



/***********************************************************************************
 Function Name      : IMGeglCreateSharedImageNOK
 Inputs             : eglDpt, eglImage, pAttribList
 Outputs            : -
 Returns            : Handle to exported data
 Description        : Create an SharedImageNOK
************************************************************************************/
IMG_INTERNAL EGLNativeSharedImageTypeNOK IMGeglCreateSharedImageNOK(
												EGLDisplay eglDpy, 
												EGLImageKHR eglImage, 
												EGLint *pAttribList)
{
	KEGL_DISPLAY *psDpy;
	TLS psTls;
	KEGL_IMAGE   * psKEGLImage = (KEGL_IMAGE *)eglImage;
	KEGL_SHARED_IMAGE_NOK  *psSharedImage;
	SrvSysContext *psSysContext;
	
	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglCreateSharedImageNOK"));

	psTls = TLS_Open(_TlsInit);
	
	if (psTls==IMG_NULL)
	{
		return (EGLNativeSharedImageTypeNOK)0;		
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglCreateSharedImageNOK);

	psTls->lastError = EGL_SUCCESS;

	/* Get display */
	psDpy = GetKEGLDisplay(psTls, eglDpy);
	if (!psDpy)
	{
		psTls->lastError = EGL_BAD_DISPLAY;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateSharedImageNOK);

		return (EGLNativeSharedImageTypeNOK)0;		
	}

	/* Get image */
	if(!_IsEGLImage(psDpy, psKEGLImage))
	{
		psTls->lastError = EGL_BAD_PARAMETER;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroyImageKHR);

		return (EGLNativeSharedImageTypeNOK)0;		
	}

	/* Get attributes */
	if( pAttribList != NULL && 
		pAttribList[0] != 0 ) 
	{
		psTls->lastError = EGL_BAD_PARAMETER;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroyImageKHR);

		return (EGLNativeSharedImageTypeNOK)0;
	}

	/* Alloc NOK_SHARED_IMAGE struct */
	psSysContext = &psTls->psGlobalData->sSysContext;

	psSharedImage = EGLMalloc( sizeof(*psSharedImage) );

	/* Initialize shared area */
	psSharedImage->psDpy = psDpy;

	/* Create EGLImage template from passed EGLImage */
	if(!CreateSharedImageNOKTemplate( psSharedImage,
									  &psKEGLImage->sEGLImage,
									  psSysContext ) ) 
	{
		psTls->lastError = EGL_BAD_ALLOC;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroyImageKHR);

		return (EGLNativeSharedImageTypeNOK)0;
	}

	/* Chain the new image onto the displays image list */
	EGLThreadLock(psTls);
	psSharedImage->psNext = psDpy->psHeadSharedImageNOK;
	psDpy->psHeadSharedImageNOK = psSharedImage;
	EGLThreadUnlock(psTls);   
		  
	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglCreateSharedImageNOK);

	return (EGLNativeSharedImageTypeNOK)psSharedImage->hExportHandle;
}

/***********************************************************************************
 Function Name      : IMGeglDestroySharedImageNOK
 Inputs             : eglDpy, hNOKSharedImage
 Outputs            : -
 Returns            : EGL_TRUE if everything went fine
 Description        : Destroy a SharedImageNOK
************************************************************************************/
IMG_INTERNAL EGLBoolean IMGeglDestroySharedImageNOK(EGLDisplay eglDpy, 
													EGLNativeSharedImageTypeNOK hNOKSharedImage)
{
	KEGL_SHARED_IMAGE_NOK  *psSharedImage;
	KEGL_DISPLAY *psDpy;
	TLS psTls;
	
	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglDestroySharedImageNOK"));

	psTls = TLS_Open(_TlsInit);
	
	if (psTls==IMG_NULL)
	{
		return EGL_FALSE;
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglDestroySharedImageNOK);

	psTls->lastError = EGL_SUCCESS;

	/* Get display */
	psDpy = GetKEGLDisplay(psTls, eglDpy);
	if (!psDpy)
	{
		psTls->lastError = EGL_BAD_DISPLAY;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroySharedImageNOK);

		return EGL_FALSE;
	}

	EGLThreadLock(psTls);
	psSharedImage = psDpy->psHeadSharedImageNOK;
	while(psSharedImage != NULL) 
	{
		if( psSharedImage->hExportHandle == (IMG_HANDLE)hNOKSharedImage )
			break;
		psSharedImage = psSharedImage->psNext;
	}
	EGLThreadUnlock(psTls);

	if( psSharedImage == NULL ) 
	{
		psTls->lastError = EGL_BAD_PARAMETER;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroySharedImageNOK);

		return EGL_FALSE;
	}

	/* Lock, cause we are changing list of images in display */
	EGLThreadLock(psTls);
	_SharedImageNOKDelete( psSharedImage );
	EGLThreadUnlock(psTls);

	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglDestroySharedImageNOK);

	return EGL_TRUE;
}


/***********************************************************************************
 Function Name      : IMGeglQueryImageNOK
 Inputs             : eglDpy, eglImage, attribute
 Outputs            : *value
 Returns            : EGL_TRUE if everything went fine
 Description        : Query a attribute of EGLImage (Width, Height...)
************************************************************************************/
IMG_INTERNAL EGLBoolean IMGeglQueryImageNOK(EGLDisplay eglDpy, 
											EGLImageKHR eglImage, 
											EGLint attribute, 
											EGLint *value)
{
	KEGL_DISPLAY *psDpy;
	KEGL_IMAGE   *psKEGLImage = (KEGL_IMAGE *)eglImage;
	TLS psTls;
	
	PVR_DPF((PVR_DBG_CALLTRACE, "IMGeglQueryImageNOK"));

	psTls = TLS_Open(_TlsInit);
	
	if (psTls==IMG_NULL)
	{
		return EGL_FALSE;
	}

	IMGEGL_TIME_START(IMGEGL_TIMER_IMGeglQueryImageNOK);

	psTls->lastError = EGL_SUCCESS;

	/* Get display */
	psDpy = GetKEGLDisplay(psTls, eglDpy);
	if (!psDpy)
	{
		psTls->lastError = EGL_BAD_DISPLAY;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglQueryImageNOK);

		return EGL_FALSE;
	}

	/* Confirm that we have EGLImage */
	if(!_IsEGLImage(psDpy, psKEGLImage))
	{
		psTls->lastError = EGL_BAD_PARAMETER;

		IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglQueryImageNOK);

		return EGL_FALSE;
	}


	switch( attribute ) 
	{
	case EGL_WIDTH:
		{
			*value = psKEGLImage->sEGLImage.ui32Width;
			break;
		}
	case EGL_HEIGHT:
		{
			*value = psKEGLImage->sEGLImage.ui32Height;
			break;
		}
	default:
		{
			psTls->lastError = EGL_BAD_ATTRIBUTE;

			IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglQueryImageNOK);

			return EGL_FALSE;
		}
	}

	IMGEGL_TIME_STOP(IMGEGL_TIMER_IMGeglQueryImageNOK);

	return EGL_TRUE;
}

#endif /* defined(EGL_EXTENSION_NOK_IMAGE_SHARED) */


/*****************************************************************************
 End of file (egl_eglimage.c)
******************************************************************************/

