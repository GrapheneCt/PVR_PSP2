/*************************************************************************/ /*!
@Title          Service Interface.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Service interface for Khronos EGL.
@License        Strictly Confidential.
*/ /**************************************************************************/

#include "egl_internal.h"
#include "pvr_debug.h"
#include "pvr_metrics.h"
#include "drvegl.h"
#include "drveglext.h"
#include "srv.h"
#include "sgxmmu.h"
#include "string.h"
#if defined(DEBUG)
#	include "tls.h"
#endif

IMG_INTERNAL const SRVPixelFormat gasSRVPixelFormat[] =
{
	{ 0, 0, 0, 0, 0  }, /* 0  PVRSRV_PIXEL_FORMAT_UNKNOWN */
	{ 0, 5, 6, 5, 16 }, /* 1  PVRSRV_PIXEL_FORMAT_RGB565 */
	{ 0, 5, 5, 5, 16 }, /* 2  PVRSRV_PIXEL_FORMAT_RGB555 */
	{ 0, 8, 8, 8, 24 }, /* 3  PVRSRV_PIXEL_FORMAT_RGB888 */
	{ 0, 8, 8, 8, 24 }, /* 4  PVRSRV_PIXEL_FORMAT_BGR888 */
	{ 0, 0, 0, 0, 0  }, /* 5  Unused */
	{ 0, 0, 0, 0, 0  }, /* 6  Unused */
	{ 0, 0, 0, 0, 0  }, /* 7  Unused */
	{ 0, 0, 0, 0, 0  }, /* 8  PVRSRV_PIXEL_FORMAT_GREY_SCALE */
	{ 0, 0, 0, 0, 0  }, /* 9  Unused */
	{ 0, 0, 0, 0, 0  }, /* 10 Unused */
	{ 0, 0, 0, 0, 0  }, /* 11 Unused */
	{ 0, 0, 0, 0, 0  }, /* 12 Unused */
	{ 0, 0, 0, 0, 0  }, /* 13 PVRSRV_PIXEL_FORMAT_PAL12 */
	{ 0, 0, 0, 0, 0  }, /* 14 PVRSRV_PIXEL_FORMAT_PAL8 */
	{ 0, 0, 0, 0, 0  }, /* 15 PVRSRV_PIXEL_FORMAT_PAL4 */
	{ 0, 0, 0, 0, 0  }, /* 16 PVRSRV_PIXEL_FORMAT_PAL2 */
	{ 0, 0, 0, 0, 0  }, /* 17 PVRSRV_PIXEL_FORMAT_PAL1 */
	{ 1, 5, 5, 5, 16 }, /* 18 PVRSRV_PIXEL_FORMAT_ARGB1555 */
	{ 4, 4, 4, 4, 16 }, /* 19 PVRSRV_PIXEL_FORMAT_ARGB4444 */
	{ 8, 8, 8, 8, 32 }, /* 20 PVRSRV_PIXEL_FORMAT_ARGB8888 */
	{ 8, 8, 8, 8, 32 }, /* 21 PVRSRV_PIXEL_FORMAT_ABGR8888 */
	{ 0, 0, 0, 0, 0  }, /* 22 PVRSRV_PIXEL_FORMAT_YV12 */
	{ 0, 0, 0, 0, 0  }, /* 23 PVRSRV_PIXEL_FORMAT_I420 */
	{ 0, 0, 0, 0, 0  }, /* 24 Unused */
	{ 0, 0, 0, 0, 0  }, /* 25 PVRSRV_PIXEL_FORMAT_IMC2 */
	{ 0, 8, 8, 8, 32 }, /* 26 PVRSRV_PIXEL_FORMAT_XRGB8888 */
	{ 0, 8, 8, 8, 32 }  /* 27 PVRSRV_PIXEL_FORMAT_XBGR8888 */
};


/******************************************************************************
 Function Name      : SRV_ServicesInit
 Inputs             : psSysContext
 Outputs            : -
 Returns            : success
 Description        : Initialises the services and fills in the system context.
************************************************************************************/
IMG_INTERNAL IMG_BOOL SRV_ServicesInit(SrvSysContext *psSysContext, IMGEGLAppHints *psAppHints, IMG_FLOAT fCPUSpeed)
{
	IMG_BOOL bRetVal = IMG_FALSE;
#if defined(SUPPORT_SGX)
	bRetVal = SRV_SGXServicesInit(psSysContext, psAppHints);
#endif

#if defined(SUPPORT_VGX)
	bRetVal |= SRV_VGXServicesInit(psSysContext, psAppHints);
#endif

#if defined(TIMING) || defined(DEBUG)
	psSysContext->fCPUSpeed = fCPUSpeed;
#else
	(void)(fCPUSpeed);
#endif

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
	psSysContext->psHeadSurface = IMG_NULL;
	psSysContext->bHibernated = IMG_FALSE;
#endif
	return bRetVal;
}


/***********************************************************************************
 Function Name      : SRV_ServicesDeInit
 Inputs             : psSysContext
 Outputs            : -
 Returns            : success
 Description        : DeInitialises the services.
************************************************************************************/
IMG_INTERNAL IMG_BOOL SRV_ServicesDeInit(SrvSysContext *psSysContext)
{
	IMG_BOOL bRetVal = IMG_FALSE;
#if defined(SUPPORT_SGX)
	bRetVal = SRV_SGXServicesDeInit(psSysContext);
#endif

#if defined(SUPPORT_VGX)
	bRetVal |= SRV_VGXServicesDeInit(psSysContext);
#endif

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
	psSysContext->psHeadSurface = IMG_NULL;
#endif
	return bRetVal;
}

/***********************************************************************************
 Function Name      : SRV_CreateSurface
 Inputs             : psSysContext
 Outputs            : psSurface
 Returns            : success
 Description        : Creates a surface in the services (wrapper fn)
************************************************************************************/
IMG_INTERNAL IMG_BOOL SRV_CreateSurface(SrvSysContext *psSysContext, KEGL_SURFACE *psSurface)
{
	EGLDrawableParams sParams;
	IMGEGLAppHints *psAppHints;
#if defined(SUPPORT_SGX)	
	IMG_BOOL bForceExternalZSBuffer;
#endif
	IMG_BOOL bSuccess;
	IMG_BOOL bMultiSample = IMG_FALSE;
	IMG_BOOL bCreateZSBuffer = IMG_FALSE;
	IMG_BOOL bAllocTwoRT = IMG_FALSE;

	psAppHints = (IMGEGLAppHints *)psSysContext->hIMGEGLAppHints;

	if(!KEGLGetDrawableParameters(psSurface, &sParams, IMG_FALSE))
	{
		PVR_DPF((PVR_DBG_ERROR,"SRV_CreateSurface: Couldn't get drawable params"));

		return IMG_FALSE;
	}

	if(((CFGC_GetAttrib(psSurface->psCfg, EGL_SAMPLE_BUFFERS) > 0) && 
		(CFGC_GetAttrib(psSurface->psCfg, EGL_SAMPLES) > 1)))
	{
		bMultiSample = IMG_TRUE;
		
		if(CFGC_GetAttrib(psSurface->psCfg, EGL_RENDERABLE_TYPE) & EGL_OPENGL_ES_BIT)
		{
			bAllocTwoRT = IMG_TRUE;
		}
	}
	
#if defined(SUPPORT_SGX)
	
	switch(psAppHints->ui32ExternalZBufferMode)
	{
		case EXTERNAL_ZBUFFER_MODE_ALLOC_UPFRONT_USED_ALWAYS:
		case EXTERNAL_ZBUFFER_MODE_ALLOC_UPFRONT_USED_ASNEEDED:
		{
			bForceExternalZSBuffer = IMG_TRUE;

			break;
		}
		case EXTERNAL_ZBUFFER_MODE_ALLOC_ONDEMAND_USED_ALWAYS:
		case EXTERNAL_ZBUFFER_MODE_ALLOC_ONDEMAND_USED_ASNEEDED:
		case EXTERNAL_ZBUFFER_MODE_ALLOC_NEVER_USED_NEVER:
		{
			bForceExternalZSBuffer = IMG_FALSE;

			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"SRV_CreateSurface: Bad external Z Buffer Mode (%d)", psAppHints->ui32ExternalZBufferMode));

			return IMG_FALSE;
		}
	}

	/* If we meet the Depth Buffer Size criteria, force external Z buffer on */
	if(sParams.ui32Width <= psAppHints->ui32DepthBufferXSize && 
		sParams.ui32Height <= psAppHints->ui32DepthBufferYSize)
	{
		bForceExternalZSBuffer = IMG_TRUE;
	}

	if((CFGC_GetAttrib(psSurface->psCfg, EGL_DEPTH_SIZE) > 0) || 
		(CFGC_GetAttrib(psSurface->psCfg, EGL_STENCIL_SIZE) > 0))
	{
		psSurface->sRenderSurface.bDepthStencilBits = IMG_TRUE;
	
		if( bForceExternalZSBuffer)
		{
			bCreateZSBuffer = IMG_TRUE;
		}
	}

#endif

#if defined(SUPPORT_VGX)

	switch(psAppHints->ui32ExternalVGXStencilBufferMode)
	{
		case EXTERNAL_VGX_STENCIL_BUFFER_MODE_ALLOC_NOAA_UPFRONT:
		case EXTERNAL_VGX_STENCIL_BUFFER_MODE_ALLOC_4X_UPFRONT:
		case EXTERNAL_VGX_STENCIL_BUFFER_MODE_ALLOC_8X_UPFRONT:
		{
			bCreateZSBuffer = IMG_TRUE;

			break;
		}
		default:
		{    
			bCreateZSBuffer = IMG_FALSE;
			break;			
		}    
	}
	
#endif


	bSuccess = KEGLCreateRenderSurface(psSysContext,
										&sParams,
										bMultiSample,
										bAllocTwoRT,
										bCreateZSBuffer,
										&psSurface->sRenderSurface);

	psSurface->sRenderSurface.hEGLSurface = psSurface;

	return bSuccess;
}


/***********************************************************************************
 Function Name      : SRV_DestroySurface
 Inputs             : psSysContext, psSurface
 Outputs            : -
 Returns            : success
 Description        : Removes a surface in the services.
************************************************************************************/
IMG_INTERNAL IMG_BOOL SRV_DestroySurface(SrvSysContext *psSysContext, KEGL_SURFACE *psSurface)
{
	IMG_BOOL bSuccess;

	bSuccess = KEGLDestroyRenderSurface(psSysContext, &psSurface->sRenderSurface);

	return bSuccess;
}

/***********************************************************************************
 Function Name      : KEGLCreateRenderSurface
 Inputs             : psSysContext, psParams, bCreateZSBuffer, bMultisample
 Outputs            : psSurface
 Returns            : success
 Description        : Creates a surface in the services.
************************************************************************************/
IMG_EXPORT IMG_BOOL IMG_CALLCONV KEGLCreateRenderSurface(SrvSysContext *psSysContext,
														EGLDrawableParams *psParams,
														IMG_BOOL bMultiSample,
														IMG_BOOL bAllocTwoRT,
														IMG_BOOL bCreateZSBuffer,
														EGLRenderSurface *psSurface)
{
	IMG_BOOL bRetVal = IMG_FALSE;

#if defined(SUPPORT_SGX)
	bRetVal = KEGL_SGXCreateRenderSurface(psSysContext,
										  psParams,
										  bMultiSample,
										  bAllocTwoRT,
										  bCreateZSBuffer,
										  psSurface);
#endif

#if defined(SUPPORT_VGX)
	bRetVal |= KEGL_VGXCreateRenderSurface(psSysContext,
										   psParams,
										   bMultiSample,
										   bAllocTwoRT,
										   bCreateZSBuffer,
										   psSurface);
#endif

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
	if(bRetVal == IMG_TRUE)
	{
		psSurface->psNextSurfaceAll = psSysContext->psHeadSurface;
		psSysContext->psHeadSurface = psSurface;
		psSurface->eRotationAngle = psParams->eRotationAngle;
		psSurface->ui32Width = psParams->ui32Width;
		psSurface->ui32Height = psParams->ui32Height;
	}
#endif

	return bRetVal;
}


/***********************************************************************************
 Function Name      : KEGLDestroyRenderSurface
 Inputs             : psSysContext
 Outputs            : -
 Returns            : success
 Description        : Removes a surface in the services.
************************************************************************************/
IMG_EXPORT IMG_BOOL IMG_CALLCONV KEGLDestroyRenderSurface(SrvSysContext *psSysContext, EGLRenderSurface *psSurface)
{
	IMG_BOOL bRetVal = IMG_FALSE;

	PVR_ASSERT(psSysContext != IMG_NULL);
	PVR_ASSERT(psSurface != IMG_NULL);

#if defined(SUPPORT_SGX)
	bRetVal = KEGL_SGXDestroyRenderSurface(psSysContext, psSurface);
#endif

#if defined(SUPPORT_VGX)
	bRetVal |= KEGL_VGXDestroyRenderSurface(psSysContext, psSurface);
#endif

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
	if(bRetVal == IMG_TRUE)
	{
		EGLRenderSurface **ppsSurf;
		int found_count = 0;

		for (ppsSurf = &psSysContext->psHeadSurface; *ppsSurf != IMG_NULL; ppsSurf = &((*ppsSurf)->psNextSurfaceAll))
		{
			if (*ppsSurf == psSurface)
			{
				++found_count;
				*ppsSurf = (*ppsSurf)->psNextSurfaceAll;
				break;
			}
		}
		if(found_count != 1)
		{
			return IMG_FALSE;
		}
	}
#endif

	return bRetVal;
}

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
IMG_INTERNAL IMG_BOOL KEGLHibernateRenderSurface(SrvSysContext	*psSysContext,
											   EGLRenderSurface	*psSurface)
{
	IMG_BOOL bRetVal = IMG_FALSE;
#if defined(SUPPORT_SGX)
	bRetVal = KEGL_SGXHibernateRenderSurface(psSysContext,
											 psSurface);
#endif

#if defined(SUPPORT_VGX)
	bRetVal |= KEGL_VGXHibernateRenderSurface(psSysContext,
											  psSurface);
#endif

	return bRetVal;
}

IMG_INTERNAL IMG_BOOL KEGLAwakeRenderSurface(SrvSysContext		*psSysContext,
										   EGLRenderSurface		*psSurface)
{
	/* This is dummy structure except for the values we fill in here */
	EGLDrawableParams sParams;
	IMG_BOOL bRetVal = IMG_FALSE;

	sParams.eRotationAngle	= psSurface->eRotationAngle;
	sParams.ui32Width		= psSurface->ui32Width; 
	sParams.ui32Height		= psSurface->ui32Height;

#if defined(SUPPORT_SGX)
	bRetVal = KEGL_SGXAwakeRenderSurface(psSysContext,
										 &sParams,
										 psSurface);
#endif

#if defined(SUPPORT_VGX)
	bRetVal |= KEGL_VGXAwakeRenderSurface(psSysContext,
										  &sParams,
										  psSurface);
#endif

	return bRetVal;
}
#endif /* defined(EGL_EXTENSION_IMG_EGL_HIBERNATION) */


/***********************************************************************************
 Function Name      : KEGLResizeRenderSurface
 Inputs             : psSysContext
 Outputs            : -
 Returns            : success
 Description        : Resizes a surface in the services.
************************************************************************************/
IMG_EXPORT IMG_BOOL IMG_CALLCONV KEGLResizeRenderSurface(SrvSysContext		*psSysContext,
														 EGLDrawableParams	*psParams,
														 IMG_BOOL			bMultiSample,
														 IMG_BOOL			bCreateZSBuffer,
														 EGLRenderSurface	*psSurface)
{
	IMG_BOOL bRetVal = IMG_FALSE;
#if defined(SUPPORT_SGX)
	bRetVal = KEGL_SGXResizeRenderSurface(psSysContext,
										  psParams,
										  bMultiSample,
										  bCreateZSBuffer,
										  psSurface);
#endif

#if defined(SUPPORT_VGX)
	bRetVal |= KEGL_VGXResizeRenderSurface(psSysContext,
										   psParams,
										   bMultiSample,
										   bCreateZSBuffer,
										   psSurface);
#endif

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
	if(bRetVal == IMG_TRUE)
	{
		psSurface->eRotationAngle = psParams->eRotationAngle;
		psSurface->ui32Width = psParams->ui32Width;
		psSurface->ui32Height = psParams->ui32Height;
	}
#endif

	return bRetVal;
}


#if defined(DEBUG)
/***********************************************************************************
 Function Name      : KEGLAllocDeviceMemTrack
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV KEGLAllocDeviceMemTrack(SrvSysContext *psSysContext, const IMG_CHAR *pszFile, IMG_UINT32 u32Line,
															 PVRSRV_DEV_DATA *psDevData, IMG_HANDLE hDevMemHeap, IMG_UINT32 ui32Attribs,
															 IMG_UINT32 ui32Size, IMG_UINT32 ui32Alignment, PVRSRV_CLIENT_MEM_INFO **ppsMemInfo)
{
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;
	DevMemAllocation *psAllocation;
	IMG_UINT32 u32NewAllocations;
	PVRSRV_ERROR eError, eError2;
	TLS psTls;
	
	psTls = IMGEGLGetTLSValue();

	if (psTls==IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLAllocDeviceMemTrack: No Current thread"));

		return PVRSRV_ERROR_HANDLE_NOT_FOUND;
	}

	*ppsMemInfo = IMG_NULL;

	PVRSRVLockMutex(psTls->psGlobalData->hEGLMemTrackingResource);

	if(psSysContext->ui32CurrentAllocations==psSysContext->ui32MaxAllocations)
	{
		if(psSysContext->ui32MaxAllocations)
		{
			u32NewAllocations = psSysContext->ui32MaxAllocations * 2;
		}
		else
		{
			u32NewAllocations = 8;
		}

		psAllocation = EGLRealloc(psSysContext->psDevMemAllocations, sizeof(SrvSysContext)*u32NewAllocations);

		if(psAllocation)
		{
			psSysContext->psDevMemAllocations = psAllocation;

			psSysContext->ui32MaxAllocations = u32NewAllocations;
		}
		else
		{
			PVRSRVUnlockMutex(psTls->psGlobalData->hEGLMemTrackingResource);

			return PVRSRV_ERROR_OUT_OF_MEMORY;
		}
	}

	eError = PVRSRVAllocDeviceMem(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, psSysContext->hPerProcRef, ppsMemInfo);

	if(eError == PVRSRV_OK)
	{
		psAllocation = &psSysContext->psDevMemAllocations[psSysContext->ui32CurrentAllocations];

		psAllocation->psMemInfo = *ppsMemInfo;
		psAllocation->psDevData = psDevData;
		psAllocation->u32Line   = u32Line;
		psAllocation->pszFile   = pszFile;

		psSysContext->ui32CurrentAllocations++;
	}

	psMemInfo = *ppsMemInfo;

	if (!(ui32Attribs & PVRSRV_MEM_NO_SYNCOBJ))
	{
		eError2 = PVRSRVAllocSyncInfo(psDevData, &psMemInfo->psClientSyncInfo);
		if (eError2 != PVRSRV_OK)
		{
			PVRSRVFreeDeviceMem(psDevData, psMemInfo);
			ppsMemInfo = IMG_NULL;
			eError = eError2;
		}
	}
	else
	{
		psMemInfo->psClientSyncInfo = IMG_NULL;
	}

	PVRSRVUnlockMutex(psTls->psGlobalData->hEGLMemTrackingResource);

	return eError;
}


/***********************************************************************************
 Function Name      : KEGLFreeDeviceMemTrack
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV KEGLFreeDeviceMemTrack(SrvSysContext *psSysContext, const IMG_CHAR *pszFile, IMG_UINT32 u32Line,
															PVRSRV_DEV_DATA	*psDevData, PVRSRV_CLIENT_MEM_INFO *psMemInfo)
{
	IMG_UINT32 i, ui32PointerPosition;
	PVRSRV_ERROR eError;
	IMG_BOOL bFound;
	TLS psTls;
	
	psTls = IMGEGLGetTLSValue();

	if (psTls==IMG_NULL)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLFreeDeviceMemTrack: No Current thread"));

		return PVRSRV_ERROR_HANDLE_NOT_FOUND;
	}

	/* Ignore NULL silently */
	if(!psMemInfo)
	{
		return PVRSRV_OK;
	}
	
	ui32PointerPosition = 0;

	bFound = IMG_FALSE;

	PVRSRVLockMutex(psTls->psGlobalData->hEGLMemTrackingResource);

	/* Try to find the pointer in the array of allocations */
	for(i=0; i<psSysContext->ui32CurrentAllocations; i++)
	{
		if(psSysContext->psDevMemAllocations[i].psMemInfo==psMemInfo)
		{
			bFound = IMG_TRUE;

			ui32PointerPosition = i;

			break;
		}
	}

	if(bFound && (psDevData!=psSysContext->psDevMemAllocations[ui32PointerPosition].psDevData))
	{
		PVR_DPF((PVR_DBG_WARNING, "KEGLFreeDeviceMemTrack: The DevData used to allocate the mem block at device virtual address 0x%x was %p, but the one used to free it is %p. %s:%d",
			psSysContext->psDevMemAllocations[ui32PointerPosition].psMemInfo->sDevVAddr.uiAddr,
			psSysContext->psDevMemAllocations[ui32PointerPosition].psDevData, psDevData, pszFile, u32Line));
	}

	eError = PVRSRV_OK;

	if(bFound)
	{
		eError = PVRSRVFreeDeviceMem(psDevData, psMemInfo);

		if (psMemInfo->psClientSyncInfo)
		{
			PVRSRVFreeSyncInfo(psDevData, psMemInfo->psClientSyncInfo);
		}

		/* Replace the freed pointer with the last pointer in the array of allocations */
		psSysContext->psDevMemAllocations[ui32PointerPosition] = psSysContext->psDevMemAllocations[--psSysContext->ui32CurrentAllocations];
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLFreeDeviceMemTrack: Refused to deallocate a pointer %p that was never allocated! %s:%d.", psMemInfo, pszFile, u32Line));
	}

	PVRSRVUnlockMutex(psTls->psGlobalData->hEGLMemTrackingResource);

	return eError;
}

#else 

#if !defined(__linux__) 

IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV KEGLAllocDeviceMemTrack(SrvSysContext *psSysContext, const IMG_CHAR *pszFile, IMG_UINT32 ui32Line,
															 PVRSRV_DEV_DATA *psDevData, IMG_HANDLE hDevMemHeap, IMG_UINT32 ui32Attribs,
															 IMG_UINT32 ui32Size, IMG_UINT32 ui32Alignment, PVRSRV_CLIENT_MEM_INFO **ppsMemInfo)
{
	PVR_UNREFERENCED_PARAMETER(psSysContext);
	PVR_UNREFERENCED_PARAMETER(pszFile);
	PVR_UNREFERENCED_PARAMETER(ui32Line);
	PVR_UNREFERENCED_PARAMETER(psDevData);
	PVR_UNREFERENCED_PARAMETER(hDevMemHeap);
	PVR_UNREFERENCED_PARAMETER(ui32Attribs);
	PVR_UNREFERENCED_PARAMETER(ui32Size);
	PVR_UNREFERENCED_PARAMETER(ui32Alignment);
	PVR_UNREFERENCED_PARAMETER(ppsMemInfo);
	
	return PVRSRV_OK;
}	

IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV KEGLFreeDeviceMemTrack(SrvSysContext *psSysContext, const IMG_CHAR *pszFile, IMG_UINT32 ui32Line,
															PVRSRV_DEV_DATA	*psDevData, PVRSRV_CLIENT_MEM_INFO *psMemInfo)
{
	PVR_UNREFERENCED_PARAMETER(psSysContext);
	PVR_UNREFERENCED_PARAMETER(pszFile);
	PVR_UNREFERENCED_PARAMETER(ui32Line);
	PVR_UNREFERENCED_PARAMETER(psDevData);
	PVR_UNREFERENCED_PARAMETER(psMemInfo);

	return PVRSRV_OK;
}	

#endif /* !defined(__linux__) */

/***********************************************************************************
 Function Name      : KEGLAllocDeviceMemPsp2
 Inputs             :
 Outputs            :
 Returns            :
 Description        :
************************************************************************************/
IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV KEGLAllocDeviceMemPsp2(SrvSysContext *psSysContext,
	PVRSRV_DEV_DATA *psDevData, IMG_HANDLE hDevMemHeap, IMG_UINT32 ui32Attribs,
	IMG_UINT32 ui32Size, IMG_UINT32 ui32Alignment, PVRSRV_CLIENT_MEM_INFO **ppsMemInfo)
{
	PVRSRV_ERROR eError;
	PVRSRV_CLIENT_MEM_INFO *psMemInfo = *ppsMemInfo;

	eError = PVRSRVAllocDeviceMem(psDevData, hDevMemHeap, ui32Attribs, ui32Size, ui32Alignment, psSysContext->hPerProcRef, ppsMemInfo);

	if (eError == PVRSRV_OK)
	{
		if (!(ui32Attribs & PVRSRV_MEM_NO_SYNCOBJ))
		{
			eError = PVRSRVAllocSyncInfo(psDevData, &psMemInfo->psClientSyncInfo);
			if (eError != PVRSRV_OK)
			{
				PVRSRVFreeDeviceMem(psDevData, psMemInfo);
				ppsMemInfo = IMG_NULL;
				return eError;
			}
		}
		else
		{
			psMemInfo->psClientSyncInfo = IMG_NULL;
		}
	}

	return eError;
}

/***********************************************************************************
 Function Name      : KEGLFreeDeviceMemPsp2
 Inputs             :
 Outputs            :
 Returns            :
 Description        :
************************************************************************************/
IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV KEGLFreeDeviceMemPsp2(SrvSysContext *psSysContext,
	PVRSRV_DEV_DATA	*psDevData, PVRSRV_CLIENT_MEM_INFO *psMemInfo)
{
	PVRSRV_ERROR eError;

	eError = PVRSRVFreeDeviceMem(psDevData, psMemInfo);

	if (psMemInfo->psClientSyncInfo)
	{
		eError = PVRSRVFreeSyncInfo(psDevData, psMemInfo->psClientSyncInfo);
	}

	return eError;
}

#endif /* defined(DEBUG) */


/******************************************************************************
 End of file (srv.c)
******************************************************************************/

