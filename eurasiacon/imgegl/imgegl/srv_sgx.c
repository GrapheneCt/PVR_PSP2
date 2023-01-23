/*************************************************************************/ /*!
@File           srv_sgx.c
@Title          SGX / Service interface for Khronos EGL.
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    SGX / Service interface for Khronos EGL.
@License        Strictly Confidential.
*/ /**************************************************************************/

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "sgxinfo.h"
#include "servicesint.h"
#include "sgxinfo_client.h"

#include "egl_internal.h"
#if defined(SUPPORT_SGX)
#include "pvr_debug.h"
#include "pvr_metrics.h"
#include "drvegl.h"
#include "srv.h"
#include "sgxmmu.h"
#include "string.h"
#include "dmscalc.h"

#define MAX(a,b) ((a)>(b)?(a):(b))

#define ALIGNCOUNT(X, Y)	(((X) + ((Y) - 1)) & ~((Y) - 1))

#include "usegen.h"
/******************************************************************************
 Function Name      : SRV_SGXServicesInit
 Inputs             : psSysContext
 Outputs            : -
 Returns            : success
 Description        : Initialises the SGX services and fills in the system context.
************************************************************************************/
IMG_INTERNAL IMG_BOOL SRV_SGXServicesInit(SrvSysContext *psSysContext, IMGEGLAppHints *psAppHints)
{
	IMG_UINT32 ui32NumDevices, i;
	PVRSRV_DEVICE_IDENTIFIER asDeviceID[PVRSRV_MAX_DEVICES];
	SGX_TRANSFERCONTEXTCREATE sCreateTransfer;
	PVRSRV_HEAP_INFO asHeapInfo[PVRSRV_MAX_CLIENT_HEAPS];
	IMG_UINT32 ui32ClientHeapCount;

	psSysContext->hIMGEGLAppHints = (IMG_HANDLE)psAppHints;

	if(PVRSRVConnect(&psSysContext->psConnection, 0) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't connect to services"));

		return IMG_FALSE;
	}

	if(PVRSRVEnumerateDevices(psSysContext->psConnection, &ui32NumDevices, asDeviceID) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't enumerate devices"));

		goto ServInit_fail;
	}

	for(i=0; i < ui32NumDevices; i++)
	{
		if(asDeviceID[i].eDeviceClass == PVRSRV_DEVICE_CLASS_3D)
		{

			if(PVRSRVAcquireDeviceData(	psSysContext->psConnection,
										asDeviceID[i].ui32DeviceIndex,
										&psSysContext->s3D,
										PVRSRV_DEVICE_TYPE_UNKNOWN)	!= PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"Couldn't get device info for 3D device"));

				goto ServInit_fail;
			}
		}
	}

	if(PVRSRVCreateDeviceMemContext(&psSysContext->s3D,
									&psSysContext->hDevMemContext,
									&ui32ClientHeapCount,
									asHeapInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't create device memory context"));

		goto ServInit_fail;
	}

#if defined (SUPPORT_OPENGL)
	/*
		Copy heap information to the  system context
	*/
		psSysContext->ui32ClientHeapCount = ui32ClientHeapCount;

		for (i=0; i<ui32ClientHeapCount; i++)
		{
			memcpy(&psSysContext->asSharedHeapInfo[i], &asHeapInfo[i],sizeof(PVRSRV_HEAP_INFO));
		}
#endif

	/*
		Setup heap handles for
	*/
	for(i=0; i<ui32ClientHeapCount; i++)
	{
		switch(HEAP_IDX(asHeapInfo[i].ui32HeapID))
		{
			case SGX_PIXELSHADER_HEAP_ID:
			{
				psSysContext->hUSEFragmentHeap = asHeapInfo[i].hDevMemHeap;
				psSysContext->uUSEFragmentHeapBase = asHeapInfo[i].sDevVAddrBase;
				break;
			}
			case SGX_VERTEXSHADER_HEAP_ID:
			{
				psSysContext->hUSEVertexHeap = asHeapInfo[i].hDevMemHeap;
				psSysContext->uUSEVertexHeapBase = asHeapInfo[i].sDevVAddrBase;
				break;
			}
			case SGX_PDSPIXEL_CODEDATA_HEAP_ID:
			{
				psSysContext->hPDSFragmentHeap = asHeapInfo[i].hDevMemHeap;
				break;
			}
			case SGX_PDSVERTEX_CODEDATA_HEAP_ID:
			{
				psSysContext->hPDSVertexHeap = asHeapInfo[i].hDevMemHeap;
				break;
			}
			case SGX_GENERAL_HEAP_ID:
			{
				psSysContext->hGeneralHeap = asHeapInfo[i].hDevMemHeap;
				break;
			}
			case SGX_SYNCINFO_HEAP_ID:
			{
				psSysContext->hSyncInfoHeap = asHeapInfo[i].hDevMemHeap;
				break;
			}
			default:
			{
				break;
			}
		}
	}

	if(SGXGetClientInfo(&psSysContext->s3D, &psSysContext->sHWInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't get HW info"));
		goto ServInit_fail;
	}

	psSysContext->sHWInfo.uPDSExecBase.uiAddr = 0;
	psSysContext->sHWInfo.uUSEExecBase.uiAddr = 0;

#if defined(__linux__)
#if (defined(TIMING) || defined(DEBUG))
	/*
		Linux running on ARM architecture needs to get the SOCTimerRegister
	*/
	if(psSysContext->sHWInfo.sMiscInfo.ui32StatePresent & PVRSRV_MISC_INFO_TIMER_PRESENT)
	{
		PVRSRVGetTimerRegister(psSysContext->sHWInfo.sMiscInfo.pvSOCTimerRegisterUM);
	}
#endif
#endif

	psSysContext->hDriverMemUID = sceKernelAllocMemBlock("SGXDriver", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, psAppHints->ui32DriverMemorySize, SCE_NULL);
	if (psSysContext->hDriverMemUID <= 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "Out of memory: driver memblock"));
		goto ServInit_fail;
	}

	if (PVRSRVRegisterMemBlock(&psSysContext->s3D, psSysContext->hDriverMemUID, &psSysContext->hPerProcRef, IMG_FALSE))
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRegisterMemBlock failed"));
		goto ServInit_fail;
	}

	psSysContext->h3DParamBufMemUID = sceKernelAllocMemBlock("SGXParamBuffer", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, psAppHints->ui32ParamBufferSize, SCE_NULL);
	if (psSysContext->h3DParamBufMemUID <= 0)
	{
		PVR_DPF((PVR_DBG_ERROR, "Out of memory: 3D param buffer memblock"));
		goto ServInit_fail;
	}

	if (PVRSRVRegisterMemBlock(&psSysContext->s3D, psSysContext->h3DParamBufMemUID, &psSysContext->hPerProcRef, IMG_TRUE))
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVRegisterMemBlock failed"));
		goto ServInit_fail;
	}

	psSysContext->sRenderContext.ui32Flags = SGX_CREATERCTXTFLAGS_CDRAMPB | SGX_CREATERCTXTFLAGS_PERCONTEXT_SYNC_INFO;
	psSysContext->sRenderContext.hDevCookie = psSysContext->s3D.hDevCookie;
	psSysContext->sRenderContext.hDevMemContext = psSysContext->hDevMemContext;
	psSysContext->sRenderContext.ui32PBSize = psAppHints->ui32ParamBufferSize - 0x80000;
	psSysContext->sRenderContext.ui32PBSizeLimit = psSysContext->sRenderContext.ui32PBSize;
	psSysContext->sRenderContext.ui32VisTestResultBufferSize = 8 * sizeof(IMG_UINT32);
	psSysContext->sRenderContext.ui32MaxSACount = PVR_MAX_VS_SECONDARIES;
	psSysContext->sRenderContext.hMemBlockProcRef = psSysContext->hPerProcRef;
	psSysContext->sRenderContext.bPerContextPB = IMG_TRUE;

#if defined (SUPPORT_HYBRID_PB)
	psSysContext->sRenderContext.bPerContextPB = psAppHints->bPerContextPB;
#endif

	if(SGXCreateRenderContext(	&psSysContext->s3D,
								&psSysContext->sRenderContext,
								&psSysContext->hRenderContext,
								&psSysContext->psVisTestResults) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't create render context"));
		goto ServInit_fail;
	}

	sCreateTransfer.hDevMemContext = psSysContext->hDevMemContext;
	sCreateTransfer.hMemBlockProcRef = psSysContext->hPerProcRef;

	if (SGXCreateTransferContext(&psSysContext->s3D,
				                 128 * 1024, /* = SGXTQ_STAGINGBUFFER_MIN_SIZE */
								 &sCreateTransfer,
								 &psSysContext->hTransferContext) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't create transfer queue"));
		goto ServInit_fail;
	}

	return IMG_TRUE;

ServInit_fail:

	/* Cleanup everything as we may have to call this function again */
	SRV_ServicesDeInit(psSysContext);
	return IMG_FALSE;
}

/***********************************************************************************
 Function Name      : SRV_SGXServicesDeInit
 Inputs             : psSysContext
 Outputs            : -
 Returns            : success
 Description        : DeInitialises the services.
************************************************************************************/
IMG_INTERNAL IMG_BOOL SRV_SGXServicesDeInit(SrvSysContext *psSysContext)
{
	if (SGXDestroyTransferContext(&psSysContext->s3D, psSysContext->hTransferContext, CLEANUP_WITH_POLL) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't destroy transfer queue"));
        /* continue cleaning up */
	}
	psSysContext->hTransferContext = (IMG_HANDLE)0;

	if(SGXDestroyRenderContext(&psSysContext->s3D,
                               psSysContext->hRenderContext,
			                   psSysContext->psVisTestResults,
							   CLEANUP_WITH_POLL) != PVRSRV_OK)
	{
	    PVR_DPF((PVR_DBG_ERROR,"Couldn't destroy render context"));
	    /* continue cleaning up */
	}
	psSysContext->hRenderContext = (IMG_HANDLE)0;

	if (psSysContext->h3DParamBufMemUID > 0)
	{
		PVRSRVUnregisterMemBlock(&psSysContext->s3D, psSysContext->h3DParamBufMemUID);
		sceKernelFreeMemBlock(psSysContext->h3DParamBufMemUID);
	}

	if (psSysContext->hDriverMemUID > 0)
	{
		PVRSRVUnregisterMemBlock(&psSysContext->s3D, psSysContext->hDriverMemUID);
		sceKernelFreeMemBlock(psSysContext->hDriverMemUID);
	}

#if (defined(TIMING) || defined(DEBUG))
#if defined(__linux__)
	if(psSysContext->sHWInfo.sMiscInfo.ui32StatePresent & PVRSRV_MISC_INFO_TIMER_PRESENT)
	{
		PVRSRVReleaseTimerRegister();
	}
#endif
#endif

	if(PVRSRVReleaseMiscInfo(psSysContext->psConnection, &psSysContext->sHWInfo.sMiscInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't release Misc info"));

		return IMG_FALSE;
	}

	if(SGXReleaseClientInfo(&psSysContext->s3D, &psSysContext->sHWInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't release HW info"));

		return IMG_FALSE;
	}

#if defined(DEBUG)
	/* Report device memory leaks and free them */
	if(psSysContext->ui32CurrentAllocations)
	{
		IMG_UINT32 i;

		PVR_DPF((PVR_DBG_ERROR,"SRV_SGXServicesDeInit: %d device memory leaks will be freed", psSysContext->ui32CurrentAllocations));

		for(i=0; i<psSysContext->ui32CurrentAllocations; i++)
		{
			DevMemAllocation *psAllocation = &psSysContext->psDevMemAllocations[i];

			PVR_DPF((PVR_DBG_ERROR,"SRV_SGXServicesDeInit: Freeing leak allocated at %s:%d, pointing to device address 0x%X, %d bytes",
				psAllocation->pszFile, psAllocation->u32Line, psAllocation->psMemInfo->sDevVAddr.uiAddr,
				psAllocation->psMemInfo->uAllocSize));

			PVRSRVFreeDeviceMem(psAllocation->psDevData, psAllocation->psMemInfo);
		}
	}

	EGLFree(psSysContext->psDevMemAllocations);

	psSysContext->psDevMemAllocations	 = IMG_NULL;
	psSysContext->ui32CurrentAllocations = 0;
	psSysContext->ui32MaxAllocations	 = 0;
#endif /* defined(DEBUG) */

	if(PVRSRVDestroyDeviceMemContext(&psSysContext->s3D,
									 psSysContext->hDevMemContext) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't destroy device memory context"));

		return IMG_FALSE;
	}
	psSysContext->hDevMemContext = (IMG_HANDLE)0;

	if(PVRSRVDisconnect(psSysContext->psConnection) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"Couldn't disconnect from services"));

		return IMG_FALSE;
	}

	psSysContext->psConnection = IMG_NULL;

	return IMG_TRUE;
}

/*****************************************************************************
 Function Name	: SetupTerminateBuffers
 Inputs			: psSysContext, psTerminateState		- pointer to the terminate state
 Outputs		: none
 Returns		: Success
 Description	: Sets up the terminate buffers.
*****************************************************************************/
static IMG_BOOL SetupTerminateBuffers(SrvSysContext *psSysContext, EGLTerminateState *psTerminateState)
{
	IMG_UINT32 *pui32Buffer;
	IMG_UINT32 ui32Size;

	/* Align to cache line size so we don't need to invalidate on alloc */
	if(IMGEGLALLOCDEVICEMEM(&psSysContext->s3D, psSysContext->hUSEVertexHeap,
							PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
							(USE_TERMINATE_DWORDS << 2),
							EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE,
							&psTerminateState->psTerminateUSEMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"SetupTerminateBuffers: Couldn't allocate USSE terminate buffer"));

		return IMG_FALSE;
	}

	ui32Size = ((PDS_TERMINATE_DWORDS << 2) + (EURASIA_VDMPDS_BASEADDR_CACHEALIGN - 1)) &
				~(EURASIA_VDMPDS_BASEADDR_CACHEALIGN - 1);

	if(IMGEGLALLOCDEVICEMEM(&psSysContext->s3D, psSysContext->hPDSVertexHeap,
							PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
							ui32Size,
							EURASIA_VDMPDS_BASEADDR_CACHEALIGN,
							&psTerminateState->psTerminatePDSMemInfo) != PVRSRV_OK)
	{
		IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psTerminateState->psTerminateUSEMemInfo);

		PVR_DPF((PVR_DBG_ERROR,"SetupTerminateBuffers: Couldn't allocate PDS terminate buffer"));

		return IMG_FALSE;
	}

	ui32Size = ((PDS_TERMINATE_SAUPDATE_DWORDS << 2) + (EURASIA_VDMPDS_BASEADDR_CACHEALIGN - 1)) &
				~(EURASIA_VDMPDS_BASEADDR_CACHEALIGN - 1);

	if(IMGEGLALLOCDEVICEMEM(&psSysContext->s3D, psSysContext->hPDSVertexHeap,
							PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
							ui32Size,
							EURASIA_VDMPDS_BASEADDR_CACHEALIGN,
							&psTerminateState->psSAUpdatePDSMemInfo) != PVRSRV_OK)
	{
		IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psTerminateState->psTerminatePDSMemInfo);
		IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psTerminateState->psTerminateUSEMemInfo);

		PVR_DPF((PVR_DBG_ERROR,"SetupTerminateBuffers: Couldn't allocate PDS SA update terminate buffer"));

		return IMG_FALSE;
	}


	pui32Buffer = (IMG_UINT32 *)psTerminateState->psTerminateUSEMemInfo->pvLinAddr;

	USEGenWriteStateEmitProgram (pui32Buffer, 2, 0, IMG_FALSE);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : SetupZLSRegs
 Inputs             : psRenderSurface, ui32Width, ui32Height, bMultiSample
 Outputs            : -
 Returns            : -
 Description        : Sets up most of the depth/stencil load/store registers
************************************************************************************/
static IMG_VOID SetupZLSRegs(EGLRenderSurface *psRenderSurface, IMG_UINT32 ui32Width, IMG_UINT32 ui32Height, IMG_BOOL bMultiSample)
{
	GLES3DRegisters *ps3DRegs;

	ps3DRegs = &psRenderSurface->s3DRegs;

	if(psRenderSurface->psZSBufferMemInfo)
	{
		IMG_UINT32 ui32ISPZLSControl, ui32ISPZLoadBase, ui32ISPZStoreBase, ui32ISPStencilLoadBase, ui32ISPStencilStoreBase;
		IMG_UINT32 ui32RoundedWidth, ui32RoundedHeight, ui32Extent, ui32Offset;

		ui32RoundedWidth = ALIGNCOUNT(ui32Width, EURASIA_TAG_TILE_SIZEX);
		ui32RoundedHeight = ALIGNCOUNT(ui32Height, EURASIA_TAG_TILE_SIZEX);

		ui32Extent = ui32RoundedWidth >> EURASIA_ISPREGION_SHIFTX;

		if(bMultiSample)
		{
			ui32Extent *= 2;
		}

		ui32ISPZLSControl = ((ui32Extent - 1) << EUR_CR_ISP_ZLSCTL_ZLSEXTENT_SHIFT) |
							EUR_CR_ISP_ZLSCTL_ZLOADFORMAT_F32Z |
							EUR_CR_ISP_ZLSCTL_ZSTOREFORMAT_F32Z |
#if defined(SGX_FEATURE_ZLS_EXTERNALZ)
							EUR_CR_ISP_ZLSCTL_EXTERNALZBUFFER_MASK |
#endif
							EUR_CR_ISP_ZLSCTL_LOADTILED_MASK |
							EUR_CR_ISP_ZLSCTL_STORETILED_MASK;

		/* Put depth at the start of the buffer */
		ui32ISPZLoadBase  = psRenderSurface->psZSBufferMemInfo->sDevVAddr.uiAddr;
		ui32ISPZStoreBase = psRenderSurface->psZSBufferMemInfo->sDevVAddr.uiAddr;

		ui32Offset = ui32RoundedWidth * ui32RoundedHeight * 4;

		if(bMultiSample)
		{
			ui32Offset *= 4;
		}

		/* Put stencil after depth in the buffer */
		ui32ISPStencilLoadBase  = psRenderSurface->psZSBufferMemInfo->sDevVAddr.uiAddr + ui32Offset;
		ui32ISPStencilStoreBase = psRenderSurface->psZSBufferMemInfo->sDevVAddr.uiAddr + ui32Offset;

		GLES_SET_REGISTER(ps3DRegs->sISPZLSControl, EUR_CR_ISP_ZLSCTL, ui32ISPZLSControl);
		GLES_SET_REGISTER(ps3DRegs->sISPZLoadBase, EUR_CR_ISP_ZLOAD_BASE, ui32ISPZLoadBase);
		GLES_SET_REGISTER(ps3DRegs->sISPZStoreBase, EUR_CR_ISP_ZSTORE_BASE, ui32ISPZStoreBase);
		GLES_SET_REGISTER(ps3DRegs->sISPStencilLoadBase, EUR_CR_ISP_STENCIL_LOAD_BASE, ui32ISPStencilLoadBase);
		GLES_SET_REGISTER(ps3DRegs->sISPStencilStoreBase, EUR_CR_ISP_STENCIL_STORE_BASE, ui32ISPStencilStoreBase);
	}
	else
	{
		GLES_SET_REGISTER(ps3DRegs->sISPZLSControl, EUR_CR_ISP_ZLSCTL, 0);
		GLES_SET_REGISTER(ps3DRegs->sISPZLoadBase, EUR_CR_ISP_ZLOAD_BASE, 0);
		GLES_SET_REGISTER(ps3DRegs->sISPZStoreBase, EUR_CR_ISP_ZSTORE_BASE, 0);
		GLES_SET_REGISTER(ps3DRegs->sISPStencilLoadBase, EUR_CR_ISP_STENCIL_LOAD_BASE, 0);
		GLES_SET_REGISTER(ps3DRegs->sISPStencilStoreBase, EUR_CR_ISP_STENCIL_STORE_BASE, 0);
	}
}

/***********************************************************************************
 Function Name      : KEGL_SGXCreateRenderSurface
 Inputs             : psSysContext, psParams, bCreateZSBuffer, bMultisample
 Outputs            : psSurface
 Returns            : success
 Description        : Creates a surface in the services.
************************************************************************************/
IMG_INTERNAL IMG_BOOL KEGL_SGXCreateRenderSurface(SrvSysContext *psSysContext,
												  EGLDrawableParams *psParams,
												  IMG_BOOL bMultiSample,
												  IMG_BOOL bAllocTwoRT,
												  IMG_BOOL bCreateZSBuffer,
												  EGLRenderSurface *psSurface)
{
	SGX_ADDRENDTARG sAddRenderTarget;
	CircularBuffer *psBuffer;
	PVRSRV_CLIENT_MEM_INFO *psMemInfo;
	IMG_UINT32 ui32MultiSample, ui32Align, ui32Size, ui32RTIndex, ui32DriverMemSize;
	IMG_SID hRtMemRef;
	IMGEGLAppHints *psAppHints;
	SceUID hDrvMemBlockForFree;
	IMG_BOOL bUnused;
	const IMG_UINT32 ui32RendersPerFrame = 1; // Default

	psAppHints = (IMGEGLAppHints *)psSysContext->hIMGEGLAppHints;

	ui32Size = psAppHints->ui32PDSFragBufferSize;

	/* Minimum of 8k bytes so that we can ensure 4k could be added in 1 draw call */
	if(ui32Size <= 8 * 1024)
	{
		goto fail_mutex;
	}

	if(bMultiSample)
	{
		ui32MultiSample = 2;
		ui32RTIndex = EGL_RENDER_TARGET_AA_INDEX;
	}
	else
	{
		ui32MultiSample = 0;
		ui32RTIndex = EGL_RENDER_TARGET_NOAA_INDEX;
	}

	psSurface->bMultiSample				= bMultiSample;
	psSurface->bInFrame					= IMG_FALSE;
	psSurface->bInExternalFrame			= IMG_FALSE;
	psSurface->bPrimitivesSinceLastTA	= IMG_FALSE;
#if defined(__psp2__)
	psSurface->ui32initWidth			= psParams->ui32Width;
	psSurface->ui32initHeight			= psParams->ui32Height;
#endif
	
	psSurface->bNeedZSLoadAfterOverflowRender = IMG_FALSE;

	if(PVRSRVCreateMutex(&psSurface->hMutex) != PVRSRV_OK)
	{
		goto fail_mutex;
	}

	sAddRenderTarget.ui32Flags			= SGX_ADDRTFLAGS_USEOGLMODE;
	sAddRenderTarget.ui32NumRTData = ui32RendersPerFrame * 2;
	sAddRenderTarget.ui32MaxQueuedRenders = ui32RendersPerFrame * 3;
	sAddRenderTarget.hRenderContext		= psSysContext->hRenderContext;
	sAddRenderTarget.ui32NumPixelsX		= MAX(1, psParams->ui32Width);
	sAddRenderTarget.ui32NumPixelsY		= MAX(1, psParams->ui32Height);

	if (ui32MultiSample == 2)
	{
		sAddRenderTarget.ui16MSAASamplesInX = 2;
		sAddRenderTarget.ui16MSAASamplesInY = 2;
	}
	else
	{
		sAddRenderTarget.ui16MSAASamplesInX = 1;
		sAddRenderTarget.ui16MSAASamplesInY = 1;
	}

	sAddRenderTarget.eForceScalingInX = SGX_SCALING_NONE;
	sAddRenderTarget.eForceScalingInY = SGX_SCALING_NONE;

	sAddRenderTarget.ui32BGObjUCoord	= 0x3f800000;
	sAddRenderTarget.eRotation			= psParams->eRotationAngle;
	sAddRenderTarget.ui16NumRTsInArray	= 1;

	sAddRenderTarget.ui8MacrotileCountX = 0;
	sAddRenderTarget.ui8MacrotileCountY = 0;
	sAddRenderTarget.bUseExternalUID = IMG_FALSE;
	sAddRenderTarget.ui32MultisampleLocations = 0;

	hRtMemRef = 0;

	SGXGetRenderTargetMemSize(&sAddRenderTarget, &ui32DriverMemSize);

	sAddRenderTarget.i32DataMemblockUID = sceKernelAllocMemBlock("SGXRenderTarget", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, ui32DriverMemSize, SCE_NULL);

	PVRSRVRegisterMemBlock(&psSysContext->s3D, sAddRenderTarget.i32DataMemblockUID, &hRtMemRef, IMG_TRUE);

	sAddRenderTarget.hMemBlockProcRef = hRtMemRef;

	if(SGXAddRenderTarget (&psSysContext->s3D,
							&sAddRenderTarget, &psSurface->ahRenderTarget[ui32RTIndex]) != PVRSRV_OK)
	{
		goto fail_add_rendertarget;
	}

	if(bAllocTwoRT)
	{
		PVR_ASSERT(bMultiSample);

		sAddRenderTarget.ui16MSAASamplesInX = 1;
		sAddRenderTarget.ui16MSAASamplesInY = 1;
		sAddRenderTarget.eForceScalingInX = SGX_SCALING_NONE;
		sAddRenderTarget.eForceScalingInY = SGX_SCALING_NONE;

		hRtMemRef = 0;

		SGXGetRenderTargetMemSize(&sAddRenderTarget, &ui32DriverMemSize);

		sAddRenderTarget.i32DataMemblockUID = sceKernelAllocMemBlock("SGXRenderTargetPairRt", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, ui32DriverMemSize, SCE_NULL);

		PVRSRVRegisterMemBlock(&psSysContext->s3D, sAddRenderTarget.i32DataMemblockUID, &hRtMemRef, IMG_TRUE);

		sAddRenderTarget.hMemBlockProcRef = hRtMemRef;

		if(SGXAddRenderTarget (&psSysContext->s3D,
								&sAddRenderTarget, &psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX]) != PVRSRV_OK)
		{
			goto fail_add_rendertarget1;
		}

		psSurface->bAllocTwoRT = bAllocTwoRT;
	}

	/* Alloc syncinfo. */
	if(PVRSRVAllocSyncInfo(&psSysContext->s3D, &psSurface->psRenderSurfaceSyncInfo) != PVRSRV_OK)
	{
		goto fail_syncinfo_alloc;
	}

	/* Set up the sync info pointers.  psSyncInfo defaults to the sync info for the render surface */
	psSurface->psSyncInfo				= psSurface->psRenderSurfaceSyncInfo;

	if (IMGEGLALLOCDEVICEMEM(	&psSysContext->s3D,
								psSysContext->hSyncInfoHeap,
								PVRSRV_MEM_WRITE | PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_MEM_CACHE_CONSISTENT,
								4,
								0,
								&psSurface->sRenderStatusUpdate.psMemInfo) != PVRSRV_OK)
	{
		goto fail_statusval_alloc;
	}

	PVRSRVMemSet(psSurface->sRenderStatusUpdate.psMemInfo->pvLinAddr, 0, 4);

	psSurface->sRenderStatusUpdate.ui32StatusValue = 1;

#if defined(PDUMP)

	PVRSRVPDumpMem(	psSysContext->psConnection,
					IMG_NULL, psSurface->sRenderStatusUpdate.psMemInfo,
					0, 4,
					PVRSRV_PDUMP_FLAGS_CONTINUOUS);

#endif /* defined(PDUMP) */

	ui32Align = EURASIA_PDS_DATA_CACHE_LINE_SIZE;

	psBuffer = &psSurface->sPDSBuffer;

	if(IMGEGLALLOCDEVICEMEM(&psSysContext->s3D, psSysContext->hPDSFragmentHeap, PVRSRV_MEM_READ,
							ui32Size, ui32Align, &psMemInfo) != PVRSRV_OK)
	{
		goto fail_pds_buffer_alloc;
	}

	psBuffer->psMemInfo = psMemInfo;
	psBuffer->pui32BufferBase			= psMemInfo->pvLinAddr;
	psBuffer->uDevVirtBase				= psMemInfo->sDevVAddr;
	psBuffer->ui32BufferLimitInBytes	= ui32Size;

	/* Ensure 4k could be added in 1 draw call */
	psBuffer->ui32SingleKickLimitInBytes = ui32Size  - (4 * 1024);

	psBuffer->ui32CurrentWriteOffsetInBytes		= 0;
	psBuffer->ui32CommittedPrimOffsetInBytes	= 0;
	psBuffer->ui32CommittedHWOffsetInBytes		= 0;
	psBuffer->uTACtrlKickDevAddr		= psBuffer->uDevVirtBase;

	psBuffer->bLocked					= IMG_FALSE;
	psBuffer->ui32LockCount				= 0;

	psBuffer->psDevData					= &psSysContext->s3D;

	/* Buffer must be big enough only for pixelevent program size * num outstanding renders.
	 * 1K is easily enough.
	 */
	ui32Size = 1024;

	/* Allocate device memory for status update */
	if(IMGEGLALLOCDEVICEMEM(&psSysContext->s3D,
	                        psSysContext->hSyncInfoHeap,
							PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_MEM_CACHE_CONSISTENT,
							4,
							4,
							&psBuffer->psStatusUpdateMemInfo) != PVRSRV_OK)
	{
		goto fail_pds_status_alloc;
	}
	PVRSRVMemSet(psBuffer->psStatusUpdateMemInfo->pvLinAddr, 0, 4);
	psBuffer->pui32ReadOffset = (IMG_UINT32*)psBuffer->psStatusUpdateMemInfo->pvLinAddr;

#if defined(PDUMP)

	PVRSRVPDumpMem(	psSysContext->psConnection,
					IMG_NULL, psBuffer->psStatusUpdateMemInfo,
					0, 4,
					0); // PDUMP_FLAGS_CONTINUOUS

#endif /* defined(PDUMP) */

	psBuffer = &psSurface->sUSSEBuffer;

	if(IMGEGLALLOCDEVICEMEM(&psSysContext->s3D, psSysContext->hUSEFragmentHeap, PVRSRV_MEM_READ,
							ui32Size, ui32Align, &psMemInfo) != PVRSRV_OK)
	{
		goto fail_usse_buffer_alloc;
	}

	psBuffer->psMemInfo = psMemInfo;
	psBuffer->pui32BufferBase			= psMemInfo->pvLinAddr;
	psBuffer->uDevVirtBase				= psMemInfo->sDevVAddr;
	psBuffer->ui32BufferLimitInBytes	= ui32Size;
	psBuffer->ui32SingleKickLimitInBytes = ui32Size;

	psBuffer->ui32CurrentWriteOffsetInBytes		= 0;
	psBuffer->ui32CommittedPrimOffsetInBytes	= 0;
	psBuffer->ui32CommittedHWOffsetInBytes		= 0;
	psBuffer->uTACtrlKickDevAddr		= psBuffer->uDevVirtBase;

	psBuffer->bLocked					= IMG_FALSE;
	psBuffer->ui32LockCount				= 0;

	psBuffer->psDevData					= &psSysContext->s3D;

	/* Allocate device memory for status update */
	if(IMGEGLALLOCDEVICEMEM(&psSysContext->s3D,
	                        psSysContext->hSyncInfoHeap,
							PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_MEM_CACHE_CONSISTENT,
							4,
							4,
							&psBuffer->psStatusUpdateMemInfo) != PVRSRV_OK)
	{
		goto fail_usse_status_alloc;
	}
	PVRSRVMemSet(psBuffer->psStatusUpdateMemInfo->pvLinAddr, 0, 4);
	psBuffer->pui32ReadOffset = (IMG_UINT32*)psBuffer->psStatusUpdateMemInfo->pvLinAddr;

#if defined(PDUMP)

	PVRSRVPDumpMem(	psSysContext->psConnection,
					IMG_NULL, psBuffer->psStatusUpdateMemInfo,
					0, 4,
					0); // PDUMP_FLAGS_CONTINUOUS

#endif /* defined(PDUMP) */

	if(!SetupTerminateBuffers(psSysContext, &psSurface->sTerm))
	{
		goto fail_terminate_alloc;
	}

	if(bCreateZSBuffer)
	{
		IMG_UINT32 ui32SizeInBytes;

		ui32SizeInBytes	 = (psParams->ui32Width  + EURASIA_TAG_TILE_SIZEX-1) & ~(EURASIA_TAG_TILE_SIZEX-1);
		ui32SizeInBytes *= (psParams->ui32Height + EURASIA_TAG_TILE_SIZEY-1) & ~(EURASIA_TAG_TILE_SIZEY-1);

		ui32SizeInBytes *= 5;

		if(bAllocTwoRT || bMultiSample)
		{
			ui32SizeInBytes *= 4;
		}

		ui32SizeInBytes = MAX(1, ui32SizeInBytes);
		ui32SizeInBytes = ALIGNCOUNT(ui32SizeInBytes, 256 * 1024);

		SceKernelAllocMemBlockOpt opt;
		sceClibMemset(&opt, 0, sizeof(SceKernelAllocMemBlockOpt));
		opt.size = sizeof(SceKernelAllocMemBlockOpt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
		opt.alignment = EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT;

		ui32SizeInBytes = ALIGNCOUNT(ui32SizeInBytes, 256 * 1024);

		psSurface->hZSBufferMemBlockUID = sceKernelAllocMemBlock(
			"SGXZSBufferMem",
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			ui32SizeInBytes,
			&opt);

		if (psSurface->hZSBufferMemBlockUID <= 0)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGL_SGXCreateRenderSurface: Couldn't allocate memory for Z buffer"));

			goto fail_zbuffer_alloc;
		}

		psMemInfo = PVRSRVAllocUserModeMem(sizeof(PVRSRV_CLIENT_MEM_INFO));

		sceKernelGetMemBlockBase(psSurface->hZSBufferMemBlockUID, &psMemInfo->pvLinAddr);
		psMemInfo->psNext = IMG_NULL;
		psMemInfo->sDevVAddr.uiAddr = psMemInfo->pvLinAddr;

		if (PVRSRVMapMemoryToGpu(&psSysContext->s3D,
			psSysContext->hDevMemContext,
			0,
			ui32SizeInBytes,
			0,
			psMemInfo->pvLinAddr,
			PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR,
			IMG_NULL) != PVRSRV_OK)
		{
			sceKernelFreeMemBlock(psSurface->hZSBufferMemBlockUID);

			PVR_DPF((PVR_DBG_ERROR, "KEGL_SGXCreateRenderSurface: Couldn't map Z buffer memory to GPU"));

			goto fail_zbuffer_alloc;
		}

		psSurface->psZSBufferMemInfo = psMemInfo;
	}

	/* Set up ZLS registers */
	SetupZLSRegs(psSurface, psParams->ui32Width, psParams->ui32Height, bMultiSample);

	return IMG_TRUE;



fail_zbuffer_alloc:
	IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sTerm.psSAUpdatePDSMemInfo);
	IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sTerm.psTerminatePDSMemInfo);
	IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sTerm.psTerminateUSEMemInfo);

fail_terminate_alloc:
	IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sUSSEBuffer.psStatusUpdateMemInfo);

fail_usse_status_alloc:
	IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sUSSEBuffer.psMemInfo);

fail_usse_buffer_alloc:
	IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sPDSBuffer.psStatusUpdateMemInfo);

fail_pds_status_alloc:
	IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sPDSBuffer.psMemInfo);

fail_pds_buffer_alloc:
	IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sRenderStatusUpdate.psMemInfo);

fail_statusval_alloc:
	PVRSRVFreeSyncInfo(&psSysContext->s3D, psSurface->psRenderSurfaceSyncInfo);

fail_syncinfo_alloc:
	if(bAllocTwoRT && psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX])
	{
		SGXGetRenderTargetDriverMemBlock(&psSysContext->s3D, psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX], &hDrvMemBlockForFree, &bUnused);
		SGXRemoveRenderTarget(&psSysContext->s3D, psSysContext->hRenderContext, psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX]);
		PVRSRVUnregisterMemBlock(&psSysContext->s3D, hDrvMemBlockForFree);
		sceKernelFreeMemBlock(hDrvMemBlockForFree);
	}

fail_add_rendertarget1:
	SGXGetRenderTargetDriverMemBlock(&psSysContext->s3D, psSurface->ahRenderTarget[ui32RTIndex], &hDrvMemBlockForFree, &bUnused);
	SGXRemoveRenderTarget(&psSysContext->s3D, psSysContext->hRenderContext, psSurface->ahRenderTarget[ui32RTIndex]);
	PVRSRVUnregisterMemBlock(&psSysContext->s3D, hDrvMemBlockForFree);
	sceKernelFreeMemBlock(hDrvMemBlockForFree);

fail_add_rendertarget:
	PVRSRVDestroyMutex(psSurface->hMutex);

fail_mutex:
	return IMG_FALSE;
}

/***********************************************************************************
 Function Name      : KEGL_SGXDestroyRenderSurface
 Inputs             : psSysContext
 Outputs            : -
 Returns            : success
 Description        : Removes a surface in the services.
************************************************************************************/
IMG_INTERNAL IMG_BOOL KEGL_SGXDestroyRenderSurface(SrvSysContext *psSysContext, EGLRenderSurface *psSurface)
{
	IMG_BOOL bSuccess = IMG_TRUE;
	SceUID hDrvMemBlockForFree;
	IMG_BOOL bUnused;

	/* Wait for operations on the surface to complete */
	if(PVRSRVPollForValue(psSysContext->psConnection,
							psSysContext->sHWInfo.sMiscInfo.hOSGlobalEvent,
							psSurface->sPDSBuffer.pui32ReadOffset,
							psSurface->sPDSBuffer.ui32CommittedHWOffsetInBytes,
							0xFFFFFFFF,
							1000,
							1000) != PVRSRV_OK)
	{
		bSuccess = IMG_FALSE;
	}

#if defined(PDUMP)
	PVRSRVPDumpMemPol( psSysContext->psConnection,
					   psSurface->sPDSBuffer.psStatusUpdateMemInfo,
					   0, 								/* Offset */
					   psSurface->sPDSBuffer.ui32CommittedHWOffsetInBytes,
					   0xFFFFFFFF, 						/* Mask */
					   PDUMP_POLL_OPERATOR_EQUAL,		/* Operator */
					   0); 								/* Flags */
#endif

	if(IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sTerm.psTerminatePDSMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't free PDS terminate buffer"));

		bSuccess = IMG_FALSE;
	}

	if(IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sTerm.psSAUpdatePDSMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't free PDS SA update terminate buffer"));

		bSuccess = IMG_FALSE;
	}

	if(IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sTerm.psTerminateUSEMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't free USSE terminate buffer"));

		bSuccess = IMG_FALSE;
	}

	if(IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sPDSBuffer.psMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't free PDS buffer"));

		bSuccess = IMG_FALSE;
	}

	if(IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sUSSEBuffer.psMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't free USSE buffer"));

		bSuccess = IMG_FALSE;
	}

	if(PVRSRVFreeSyncInfo(&psSysContext->s3D, psSurface->psRenderSurfaceSyncInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't free sync meminfo"));

		psSurface->psRenderSurfaceSyncInfo	= IMG_NULL;
		psSurface->psSyncInfo				= IMG_NULL;

		bSuccess = IMG_FALSE;
	}

	if(IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sRenderStatusUpdate.psMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't free render status value meminfo"));

		bSuccess = IMG_FALSE;
	}

	if(IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sUSSEBuffer.psStatusUpdateMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't free USSE buffer status update object"));

		bSuccess = IMG_FALSE;
	}

	if(IMGEGLFREEDEVICEMEM(&psSysContext->s3D, psSurface->sPDSBuffer.psStatusUpdateMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't free PDS buffer status update object"));

		bSuccess = IMG_FALSE;
	}

	if(psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX])
	{
		SGXGetRenderTargetDriverMemBlock(&psSysContext->s3D, psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX], &hDrvMemBlockForFree, &bUnused);

		if(SGXRemoveRenderTarget (&psSysContext->s3D,
								psSysContext->hRenderContext,
								psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX]) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't remove render target"));

			bSuccess = IMG_FALSE;
		}

		PVRSRVUnregisterMemBlock(&psSysContext->s3D, hDrvMemBlockForFree);
		sceKernelFreeMemBlock(hDrvMemBlockForFree);
	}

	psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX] = IMG_NULL;

	if(psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX])
	{
		SGXGetRenderTargetDriverMemBlock(&psSysContext->s3D, psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX], &hDrvMemBlockForFree, &bUnused);

		if(SGXRemoveRenderTarget (&psSysContext->s3D,
								psSysContext->hRenderContext,
								psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX]) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't remove render target"));

			bSuccess = IMG_FALSE;
		}

		PVRSRVUnregisterMemBlock(&psSysContext->s3D, hDrvMemBlockForFree);
		sceKernelFreeMemBlock(hDrvMemBlockForFree);
	}

	psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX] = IMG_NULL;

	if(psSurface->psZSBufferMemInfo)
	{
		if(PVRSRVUnmapMemoryFromGpu(&psSysContext->s3D, psSurface->psZSBufferMemInfo->pvLinAddr, 0, IMG_FALSE) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGLDestroyRenderSurface: Couldn't unmap ZLS buffer"));

			bSuccess = IMG_FALSE;
		}

		sceKernelFreeMemBlock(psSurface->hZSBufferMemBlockUID);
	}

	PVRSRVFreeUserModeMem(psSurface->psZSBufferMemInfo);

	psSurface->psZSBufferMemInfo = IMG_NULL;

	
	PVRSRVDestroyMutex(psSurface->hMutex);

	return bSuccess;
}

/***********************************************************************************
 Function Name      : KEGL_SGXResizeRenderSurface
 Inputs             : psSysContext
 Outputs            : -
 Returns            : success
 Description        : Resizes a surface in the services.
************************************************************************************/
IMG_INTERNAL IMG_BOOL KEGL_SGXResizeRenderSurface(SrvSysContext		*psSysContext,
												  EGLDrawableParams *psParams,
												  IMG_BOOL			bMultiSample,
												  IMG_BOOL			bCreateZSBuffer,
												  EGLRenderSurface	*psSurface)
{
	SGX_ADDRENDTARG sAddRenderTarget;
	IMG_BOOL bSuccess = IMG_TRUE, bHadZSBuffer = IMG_FALSE;
	IMG_UINT32 ui32MultiSample = 0, ui32RTIndex;
	IMG_UINT32 ui32SizeInBytes, ui32DriverMemSize;
	IMG_SID hRtMemRef;
	SceUID hDrvMemBlockForFree;
	IMG_BOOL bUnused;
	const IMG_UINT32 ui32RendersPerFrame = 1;

	if(bMultiSample)
	{
		ui32MultiSample = 2;

		psSurface->bMultiSample = IMG_TRUE;
		ui32RTIndex = EGL_RENDER_TARGET_AA_INDEX;
	}
	else
	{
		psSurface->bMultiSample = IMG_FALSE;
		ui32RTIndex = EGL_RENDER_TARGET_NOAA_INDEX;
	}

	/* Wait for operations on the surface to complete */
	if (PVRSRVPollForValue(psSysContext->psConnection,
		psSysContext->sHWInfo.sMiscInfo.hOSGlobalEvent,
		psSurface->sPDSBuffer.pui32ReadOffset,
		psSurface->sPDSBuffer.ui32CommittedHWOffsetInBytes,
		0xFFFFFFFF,
		1000,
		1000) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "KEGL_SGXResizeRenderSurface: Timeout failed on waiting for previous render op"));
	}

#if defined(PDUMP)
	PVRSRVPDumpMemPol( psSysContext->psConnection,
					   psSurface->sPDSBuffer.psStatusUpdateMemInfo,
					   0, 								/* Offset */
					   psSurface->sPDSBuffer.ui32CommittedHWOffsetInBytes,
					   0xFFFFFFFF, 						/* Mask */
					   PDUMP_POLL_OPERATOR_EQUAL,		/* Operator */
					   0); 								/* Flags */
#endif

	if(psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX])
	{
		SGXGetRenderTargetDriverMemBlock(&psSysContext->s3D, psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX], &hDrvMemBlockForFree, &bUnused);

		if(SGXRemoveRenderTarget (&psSysContext->s3D,
								psSysContext->hRenderContext,
								psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX]) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGL_SGXResizeRenderSurface: Couldn't remove render target"));

			bSuccess = IMG_FALSE;
		}

		PVRSRVUnregisterMemBlock(&psSysContext->s3D, hDrvMemBlockForFree);
		sceKernelFreeMemBlock(hDrvMemBlockForFree);

		psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX] = IMG_NULL;
	}

	if(psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX])
	{
		SGXGetRenderTargetDriverMemBlock(&psSysContext->s3D, psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX], &hDrvMemBlockForFree, &bUnused);

		if(SGXRemoveRenderTarget (&psSysContext->s3D,
								psSysContext->hRenderContext,
								psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX]) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGL_SGXResizeRenderSurface: Couldn't remove render target"));

			bSuccess = IMG_FALSE;
		}

		PVRSRVUnregisterMemBlock(&psSysContext->s3D, hDrvMemBlockForFree);
		sceKernelFreeMemBlock(hDrvMemBlockForFree);

		psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX] = IMG_NULL;
	}

	if(psSurface->psZSBufferMemInfo)
	{
		if (PVRSRVUnmapMemoryFromGpu(&psSysContext->s3D, psSurface->psZSBufferMemInfo->pvLinAddr, 0, IMG_FALSE) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "KEGL_SGXResizeRenderSurface: Couldn't unmap ZLS buffer"));

			bSuccess = IMG_FALSE;
		}

		sceKernelFreeMemBlock(psSurface->hZSBufferMemBlockUID);

		PVRSRVFreeUserModeMem(psSurface->psZSBufferMemInfo);

		psSurface->psZSBufferMemInfo = IMG_NULL;

		bHadZSBuffer = IMG_TRUE;
	}

	sAddRenderTarget.ui32Flags			= SGX_ADDRTFLAGS_USEOGLMODE;
	sAddRenderTarget.ui32NumRTData = ui32RendersPerFrame * 2;
	sAddRenderTarget.ui32MaxQueuedRenders = ui32RendersPerFrame * 3;
	sAddRenderTarget.hRenderContext		= psSysContext->hRenderContext;
	sAddRenderTarget.ui32NumPixelsX		= psParams->ui32Width;
	sAddRenderTarget.ui32NumPixelsY		= psParams->ui32Height;
#if defined(__psp2__)
	psSurface->ui32initWidth = psParams->ui32Width;
	psSurface->ui32initHeight = psParams->ui32Height;
#endif

	if (ui32MultiSample == 2)
	{
		sAddRenderTarget.ui16MSAASamplesInX = 2;
		sAddRenderTarget.ui16MSAASamplesInY = 2;
	}
	else
	{
		sAddRenderTarget.ui16MSAASamplesInX = 1;
		sAddRenderTarget.ui16MSAASamplesInY = 1;
	}

	sAddRenderTarget.eForceScalingInX = SGX_SCALING_NONE;
	sAddRenderTarget.eForceScalingInY = SGX_SCALING_NONE;

	sAddRenderTarget.ui32BGObjUCoord	= 0x3f800000;
	sAddRenderTarget.eRotation			= psParams->eRotationAngle;
	sAddRenderTarget.ui16NumRTsInArray	= 1;

	sAddRenderTarget.ui8MacrotileCountX = 0;
	sAddRenderTarget.ui8MacrotileCountY = 0;
	sAddRenderTarget.bUseExternalUID = IMG_FALSE;
	sAddRenderTarget.ui32MultisampleLocations = 0;

	hRtMemRef = 0;

	SGXGetRenderTargetMemSize(&sAddRenderTarget, &ui32DriverMemSize);

	sAddRenderTarget.i32DataMemblockUID = sceKernelAllocMemBlock("SGXRenderTarget", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, ui32DriverMemSize, SCE_NULL);

	PVRSRVRegisterMemBlock(&psSysContext->s3D, sAddRenderTarget.i32DataMemblockUID, &hRtMemRef, IMG_TRUE);

	sAddRenderTarget.hMemBlockProcRef = hRtMemRef;

	if(SGXAddRenderTarget (&psSysContext->s3D,
							&sAddRenderTarget, &psSurface->ahRenderTarget[ui32RTIndex]) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGL_SGXResizeRenderSurface: Couldn't add render target"));

		bSuccess = IMG_FALSE;
	}

	if(psSurface->bAllocTwoRT)
	{
		/* Second render target is opposite AA sense to request */
		if(bMultiSample)
		{
			sAddRenderTarget.ui16MSAASamplesInX = 1;
			sAddRenderTarget.ui16MSAASamplesInY = 1;
			sAddRenderTarget.eForceScalingInX = SGX_SCALING_NONE;
			sAddRenderTarget.eForceScalingInY = SGX_SCALING_NONE;

			ui32RTIndex = EGL_RENDER_TARGET_NOAA_INDEX;
		}
		else
		{
			sAddRenderTarget.ui16MSAASamplesInX = 2;
			sAddRenderTarget.ui16MSAASamplesInY = 2;
			sAddRenderTarget.eForceScalingInX = SGX_SCALING_NONE;
			sAddRenderTarget.eForceScalingInY = SGX_SCALING_NONE;

			ui32RTIndex = EGL_RENDER_TARGET_AA_INDEX;
		}
		hRtMemRef = 0;

		SGXGetRenderTargetMemSize(&sAddRenderTarget, &ui32DriverMemSize);

		sAddRenderTarget.i32DataMemblockUID = sceKernelAllocMemBlock("SGXRenderTargetPairRt", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, ui32DriverMemSize, SCE_NULL);

		PVRSRVRegisterMemBlock(&psSysContext->s3D, sAddRenderTarget.i32DataMemblockUID, &hRtMemRef, IMG_TRUE);

		sAddRenderTarget.hMemBlockProcRef = hRtMemRef;

		if(SGXAddRenderTarget (&psSysContext->s3D,
								&sAddRenderTarget, &psSurface->ahRenderTarget[ui32RTIndex]) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGL_SGXResizeRenderSurface: Couldn't add render target"));

			bSuccess = IMG_FALSE;
		}
	}

	if(bHadZSBuffer || bCreateZSBuffer)
	{
		ui32SizeInBytes	 = (psParams->ui32Width  + EURASIA_TAG_TILE_SIZEX-1) & ~(EURASIA_TAG_TILE_SIZEX-1);
		ui32SizeInBytes *= (psParams->ui32Height + EURASIA_TAG_TILE_SIZEY-1) & ~(EURASIA_TAG_TILE_SIZEY-1);

		ui32SizeInBytes *= 5;

		if(psSurface->bAllocTwoRT || bMultiSample)
		{
			ui32SizeInBytes *= 4;
		}

		SceKernelAllocMemBlockOpt opt;
		sceClibMemset(&opt, 0, sizeof(SceKernelAllocMemBlockOpt));
		opt.size = sizeof(SceKernelAllocMemBlockOpt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
		opt.alignment = EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT;

		ui32SizeInBytes = ALIGNCOUNT(ui32SizeInBytes, 256 * 1024);

		psSurface->hZSBufferMemBlockUID = sceKernelAllocMemBlock(
			"SGXZSBufferMem",
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			ui32SizeInBytes,
			&opt);

		if (psSurface->hZSBufferMemBlockUID <= 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "KEGL_SGXResizeRenderSurface: Couldn't allocate memory for Z buffer"));

			bSuccess = IMG_FALSE;
		}

		psSurface->psZSBufferMemInfo = PVRSRVAllocUserModeMem(sizeof(PVRSRV_CLIENT_MEM_INFO));

		sceKernelGetMemBlockBase(psSurface->hZSBufferMemBlockUID, &psSurface->psZSBufferMemInfo->pvLinAddr);
		psSurface->psZSBufferMemInfo->psNext = IMG_NULL;
		psSurface->psZSBufferMemInfo->sDevVAddr.uiAddr = psSurface->psZSBufferMemInfo->pvLinAddr;

		if (PVRSRVMapMemoryToGpu(&psSysContext->s3D,
			psSysContext->hDevMemContext,
			0,
			ui32SizeInBytes,
			0,
			psSurface->psZSBufferMemInfo->pvLinAddr,
			PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR,
			IMG_NULL) != PVRSRV_OK)
		{
			sceKernelFreeMemBlock(psSurface->hZSBufferMemBlockUID);

			PVR_DPF((PVR_DBG_ERROR, "KEGL_SGXResizeRenderSurface: Couldn't map Z buffer memory to GPU"));

			bSuccess = IMG_FALSE;
		}
	}

	/* Set up ZLS registers */
	SetupZLSRegs(psSurface, psParams->ui32Width, psParams->ui32Height, bMultiSample);


	return bSuccess;
}

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
IMG_INTERNAL IMG_BOOL KEGL_SGXHibernateRenderSurface(SrvSysContext		*psSysContext,
													 EGLRenderSurface	*psSurface)
{
	IMG_BOOL bSuccess = IMG_TRUE;
	SceUID hDrvMemBlockForFree;
	IMG_BOOL bUnused;

	/* Wait for operations on the surface to complete */
	if (PVRSRVPollForValue(psSysContext->psConnection,
		psSysContext->sHWInfo.sMiscInfo.hOSGlobalEvent,
		psSurface->sPDSBuffer.pui32ReadOffset,
		psSurface->sPDSBuffer.ui32CommittedHWOffsetInBytes,
		0xFFFFFFFF,
		1000,
		1000) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "KEGL_SGXResizeRenderSurface: Timeout failed on waiting for previous render op"));
	}

#if defined(PDUMP)
	PVRSRVPDumpMemPol( psSysContext->psConnection,
					   psSurface->sPDSBuffer.psStatusUpdateMemInfo,
					   0, 								/* Offset */
					   psSurface->sPDSBuffer.ui32CommittedHWOffsetInBytes,
					   0xFFFFFFFF, 						/* Mask */
					   PDUMP_POLL_OPERATOR_EQUAL,		/* Operator */
					   0); 								/* Flags */
#endif

	if(psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX])
	{
		SGXGetRenderTargetDriverMemBlock(&psSysContext->s3D, psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX], &hDrvMemBlockForFree, &bUnused);

		if(SGXRemoveRenderTarget (&psSysContext->s3D,
								psSysContext->hRenderContext,
								psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX]) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGL_SGXHibernateRenderSurface: Couldn't remove render target"));

			bSuccess = IMG_FALSE;
		}

		PVRSRVUnregisterMemBlock(&psSysContext->s3D, hDrvMemBlockForFree);
		sceKernelFreeMemBlock(hDrvMemBlockForFree);

		psSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX] = IMG_NULL;
	}

	if(psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX])
	{
		SGXGetRenderTargetDriverMemBlock(&psSysContext->s3D, psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX], &hDrvMemBlockForFree, &bUnused);

		if(SGXRemoveRenderTarget (&psSysContext->s3D,
								psSysContext->hRenderContext,
								psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX]) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGL_SGXHibernateRenderSurface: Couldn't remove render target"));

			bSuccess = IMG_FALSE;
		}

		PVRSRVUnregisterMemBlock(&psSysContext->s3D, hDrvMemBlockForFree);
		sceKernelFreeMemBlock(hDrvMemBlockForFree);

		psSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX] = IMG_NULL;
	}

	if(psSurface->psZSBufferMemInfo)
	{
		if (PVRSRVUnmapMemoryFromGpu(&psSysContext->s3D, psSurface->psZSBufferMemInfo->pvLinAddr, 0, IMG_FALSE) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR, "KEGL_SGXHibernateRenderSurface: Couldn't unmap ZLS buffer"));

			bSuccess = IMG_FALSE;
		}

		sceKernelFreeMemBlock(psSurface->hZSBufferMemBlockUID);

		PVRSRVFreeUserModeMem(psSurface->psZSBufferMemInfo);

		psSurface->psZSBufferMemInfo = IMG_NULL;

#if defined(EGL_EXTENSION_IMG_EGL_HIBERNATION)
		psSurface->bHadZSBuffer = IMG_TRUE;
	}
	else
	{
		psSurface->bHadZSBuffer = IMG_FALSE;
#endif /* defined(EGL_EXTENSION_IMG_EGL_HIBERNATION) */
	}

	return bSuccess;
}

IMG_INTERNAL IMG_BOOL KEGL_SGXAwakeRenderSurface(SrvSysContext		*psSysContext,
												 EGLDrawableParams	*psParams,
												 EGLRenderSurface	*psSurface)
{
	SGX_ADDRENDTARG sAddRenderTarget;
	IMG_BOOL bSuccess = IMG_TRUE;
	IMG_SID hRtMemRef;
	SceUID hDrvMemBlockForFree;
	IMG_BOOL bUnused;
	const IMG_UINT32 ui32RendersPerFrame = 1;

	IMG_UINT32 ui32MultiSample = 0, ui32RTIndex;
	IMG_UINT32 ui32SizeInBytes, ui32DriverMemSize;

	if(psSurface->bMultiSample)
	{
		ui32MultiSample = 2;

		ui32RTIndex = EGL_RENDER_TARGET_AA_INDEX;
	}
	else
	{
		ui32RTIndex = EGL_RENDER_TARGET_NOAA_INDEX;
	}

	sAddRenderTarget.ui32Flags			= SGX_ADDRTFLAGS_USEOGLMODE;
	sAddRenderTarget.ui32NumRTData = ui32RendersPerFrame * 2;
	sAddRenderTarget.ui32MaxQueuedRenders = ui32RendersPerFrame * 3;
	sAddRenderTarget.hRenderContext		= psSysContext->hRenderContext;
	sAddRenderTarget.ui32NumPixelsX		= psParams->ui32Width;
	sAddRenderTarget.ui32NumPixelsY		= psParams->ui32Height;

	if (ui32MultiSample == 2)
	{
		sAddRenderTarget.ui16MSAASamplesInX = 2;
		sAddRenderTarget.ui16MSAASamplesInY = 2;
	}
	else
	{
		sAddRenderTarget.ui16MSAASamplesInX = 1;
		sAddRenderTarget.ui16MSAASamplesInY = 1;
	}

	sAddRenderTarget.eForceScalingInX = SGX_SCALING_NONE;
	sAddRenderTarget.eForceScalingInY = SGX_SCALING_NONE;

	sAddRenderTarget.ui32BGObjUCoord	= 0x3f800000;
	sAddRenderTarget.eRotation			= psParams->eRotationAngle;
	sAddRenderTarget.ui16NumRTsInArray	= 1;

	sAddRenderTarget.ui8MacrotileCountX = 0;
	sAddRenderTarget.ui8MacrotileCountY = 0;
	sAddRenderTarget.bUseExternalUID = IMG_FALSE;
	sAddRenderTarget.ui32MultisampleLocations = 0;

	hRtMemRef = 0;

	SGXGetRenderTargetMemSize(&sAddRenderTarget, &ui32DriverMemSize);

	sAddRenderTarget.i32DataMemblockUID = sceKernelAllocMemBlock("SGXRenderTarget", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, ui32DriverMemSize, SCE_NULL);
	PVRSRVRegisterMemBlock(&psSysContext->s3D, sAddRenderTarget.i32DataMemblockUID, &hRtMemRef, IMG_TRUE);

	sAddRenderTarget.hMemBlockProcRef = hRtMemRef;

	if(SGXAddRenderTarget (&psSysContext->s3D,
							&sAddRenderTarget, &psSurface->ahRenderTarget[ui32RTIndex]) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGL_SGXAwakeRenderSurface: Couldn't add render target"));

		bSuccess = IMG_FALSE;
	}

	if(psSurface->bAllocTwoRT)
	{
		/* Second render target is opposite AA sense to request */
		if(psSurface->bMultiSample)
		{
			sAddRenderTarget.ui16MSAASamplesInX = 1;
			sAddRenderTarget.ui16MSAASamplesInY = 1;
			sAddRenderTarget.eForceScalingInX = SGX_SCALING_NONE;
			sAddRenderTarget.eForceScalingInY = SGX_SCALING_NONE;

			ui32RTIndex = EGL_RENDER_TARGET_NOAA_INDEX;
		}
		else
		{
			sAddRenderTarget.ui16MSAASamplesInX = 2;
			sAddRenderTarget.ui16MSAASamplesInY = 2;
			sAddRenderTarget.eForceScalingInX = SGX_SCALING_NONE;
			sAddRenderTarget.eForceScalingInY = SGX_SCALING_NONE;

			ui32RTIndex = EGL_RENDER_TARGET_AA_INDEX;
		}
		hRtMemRef = 0;

		SGXGetRenderTargetMemSize(&sAddRenderTarget, &ui32DriverMemSize);

		sAddRenderTarget.i32DataMemblockUID = sceKernelAllocMemBlock("SGXRenderTargetPairRt", SCE_KERNEL_MEMBLOCK_TYPE_USER_NC_RW, ui32DriverMemSize, SCE_NULL);
		PVRSRVRegisterMemBlock(&psSysContext->s3D, sAddRenderTarget.i32DataMemblockUID, &hRtMemRef, IMG_TRUE);

		sAddRenderTarget.hMemBlockProcRef = hRtMemRef;

		if(SGXAddRenderTarget (&psSysContext->s3D,
								&sAddRenderTarget, &psSurface->ahRenderTarget[ui32RTIndex]) != PVRSRV_OK)
		{
			PVR_DPF((PVR_DBG_ERROR,"KEGL_SGXAwakeRenderSurface: Couldn't add render target"));

			bSuccess = IMG_FALSE;
		}
	}

	if(psSurface->bHadZSBuffer)
	{
		ui32SizeInBytes	 = (psParams->ui32Width  + EURASIA_TAG_TILE_SIZEX-1) & ~(EURASIA_TAG_TILE_SIZEX-1);
		ui32SizeInBytes *= (psParams->ui32Height + EURASIA_TAG_TILE_SIZEY-1) & ~(EURASIA_TAG_TILE_SIZEY-1);

		ui32SizeInBytes *= 5;

		if(psSurface->bAllocTwoRT || psSurface->bMultiSample)
		{
			ui32SizeInBytes *= 4;
		}

		SceKernelAllocMemBlockOpt opt;
		sceClibMemset(&opt, 0, sizeof(SceKernelAllocMemBlockOpt));
		opt.size = sizeof(SceKernelAllocMemBlockOpt);
		opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
		opt.alignment = EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT;

		ui32SizeInBytes = ALIGNCOUNT(ui32SizeInBytes, 256 * 1024);

		psSurface->hZSBufferMemBlockUID = sceKernelAllocMemBlock(
			"SGXZSBufferMem",
			SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
			ui32SizeInBytes,
			&opt);

		if (psSurface->hZSBufferMemBlockUID <= 0)
		{
			PVR_DPF((PVR_DBG_ERROR, "KEGL_SGXAwakeRenderSurface: Couldn't allocate memory for Z buffer"));

			bSuccess = IMG_FALSE;
		}

		psSurface->psZSBufferMemInfo = PVRSRVAllocUserModeMem(sizeof(PVRSRV_CLIENT_MEM_INFO));

		sceKernelGetMemBlockBase(psSurface->hZSBufferMemBlockUID, &psSurface->psZSBufferMemInfo->pvLinAddr);
		psSurface->psZSBufferMemInfo->psNext = IMG_NULL;
		psSurface->psZSBufferMemInfo->sDevVAddr.uiAddr = psSurface->psZSBufferMemInfo->pvLinAddr;

		if (PVRSRVMapMemoryToGpu(&psSysContext->s3D,
			psSysContext->hDevMemContext,
			0,
			ui32SizeInBytes,
			0,
			psSurface->psZSBufferMemInfo->pvLinAddr,
			PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR,
			IMG_NULL) != PVRSRV_OK)
		{
			sceKernelFreeMemBlock(psSurface->hZSBufferMemBlockUID);

			PVR_DPF((PVR_DBG_ERROR, "KEGL_SGXAwakeRenderSurface: Couldn't map Z buffer memory to GPU"));

			bSuccess = IMG_FALSE;
		}
	}

	/* Set up ZLS registers */
	SetupZLSRegs(psSurface, psParams->ui32Width, psParams->ui32Height, psSurface->bMultiSample);


	return bSuccess;
}
#endif /* defined(EGL_EXTENSION_IMG_EGL_HIBERNATION) */


/***********************************************************************************
 Function Name      : SGXAllocatePBufferDeviceMem
 Inputs             :
 Outputs            : -
 Returns            : success
 Description        : Handles device memory allocation for SGX PBuffer surfaces.
************************************************************************************/
IMG_INTERNAL IMG_RESULT SGXAllocatePBufferDeviceMem(SrvSysContext		*psSysContext,
													KEGL_SURFACE		*psSurface,
													EGLint				pbuffer_width,
													EGLint				pbuffer_height,
													EGLint				pixel_width,
													PVRSRV_PIXEL_FORMAT	pixel_format,
													IMG_UINT32			*pui32Stride)
{
	IMG_UINT32 ui32Size;
	SceKernelAllocMemBlockOpt opt;

	PVR_UNREFERENCED_PARAMETER(pixel_format);

	if(pbuffer_width < EURASIA_TAG_STRIDE_THRESHOLD)
	{
		*pui32Stride = (pbuffer_width + (EURASIA_TAG_STRIDE_ALIGN0 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN0 - 1);
	}
	else
	{
		*pui32Stride = (pbuffer_width + (EURASIA_TAG_STRIDE_ALIGN1 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN1 - 1);
	}

	*pui32Stride *= pixel_width;

	ui32Size = MAX(1, *pui32Stride * pbuffer_height);

	psSurface->u.pbuffer.psMemInfo = PVRSRVAllocUserModeMem(sizeof(PVRSRV_CLIENT_MEM_INFO));

	sceClibMemset(&opt, 0, sizeof(SceKernelAllocMemBlockOpt));
	opt.size = sizeof(SceKernelAllocMemBlockOpt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
	opt.alignment = EURASIA_CACHE_LINE_SIZE;

	psSurface->hPBufferMemBlockUID = sceKernelAllocMemBlock(
		"SGXPBufferMem",
		SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
		ui32Size,
		&opt);

	sceKernelGetMemBlockBase(psSurface->hPBufferMemBlockUID, &psSurface->u.pbuffer.psMemInfo->pvLinAddr);
	psSurface->u.pbuffer.psMemInfo->psNext = IMG_NULL;
	psSurface->u.pbuffer.psMemInfo->sDevVAddr.uiAddr = psSurface->u.pbuffer.psMemInfo->pvLinAddr;

	return PVRSRVMapMemoryToGpu(&psSysContext->s3D,
			psSysContext->hDevMemContext,
			0,
			ui32Size,
			0,
			psSurface->u.pbuffer.psMemInfo->pvLinAddr,
			PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR,
			IMG_NULL);
}


#if defined(SUPPORT_SGX_LOW_LATENCY_SCHEDULING)

/***********************************************************************************
 Function Name      : KEGL_SGXSetContextPriority
 Inputs             : psContext, peContextPriority
 Outputs            : -
 Returns            : success
 Description        : Sets the priority of render and transfer contexts
************************************************************************************/
IMG_INTERNAL IMG_BOOL KEGL_SGXSetContextPriority(SrvSysContext *psContext, SGX_CONTEXT_PRIORITY *peContextPriority)
{
	if(SGXSetContextPriority(&psContext->s3D, peContextPriority, psContext->hRenderContext, psContext->hTransferContext) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"KEGL_SGXSetContextPriority: Failed to set SGX context priority"));

		return IMG_FALSE;
	}

	return IMG_TRUE;
}

#endif /* #if defined(SUPPORT_SGX_LOW_LATENCY_SCHEDULING) */


#endif /* SUPPORT_SGX */
/******************************************************************************
 End of file (srv_sgx.c)
******************************************************************************/

