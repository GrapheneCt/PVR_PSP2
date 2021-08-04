/******************************************************************************
 * Name         : sgxif.c
 *
 * Copyright    : 2006-2010 by Imagination Technologies Limited.
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
 * $Log: sgxif.c $
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#include "context.h"
#include "sgxmmu.h"

#define SYNCINFO_WRITEOPS_COMPLETE(SYNCINFO)  ((SYNCINFO)->psSyncData->ui32WriteOpsComplete)
#define SYNCINFO_WRITEOPS_PENDING(SYNCINFO)   ((SYNCINFO)->psSyncData->ui32WriteOpsPending)

#define SYNCINFO_READOPS_COMPLETE(SYNCINFO)	   ((SYNCINFO)->psSyncData->ui32ReadOpsComplete)
#define SYNCINFO_READOPS_PENDING(SYNCINFO)    ((SYNCINFO)->psSyncData->ui32ReadOpsPending)

#if defined(EUR_CR_ISP_ZLSCTL_MSTOREEN_MASK)
#define SGX_DRAWMASK_STORE_ENABLE_MASK	EUR_CR_ISP_ZLSCTL_MSTOREEN_MASK
#define SGX_DRAWMASK_LOAD_ENABLE_MASK	EUR_CR_ISP_ZLSCTL_MSTOREEN_MASK
#else
#define SGX_DRAWMASK_STORE_ENABLE_MASK	EUR_CR_ISP_ZLSCTL_STOREMASK_MASK
#define SGX_DRAWMASK_LOAD_ENABLE_MASK	EUR_CR_ISP_ZLSCTL_LOADMASK_MASK
#endif

/***********************************************************************************
 Function Name      : WaitForTA
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Waits for VDM control stream to finish.
************************************************************************************/
IMG_INTERNAL IMG_VOID WaitForTA(GLES1Context *gc)
{
	volatile IMG_UINT32 *pui32ReadOffset = (IMG_UINT32*)gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->psStatusUpdateMemInfo->pvLinAddr;

	GLES1_TIME_START(GLES1_TIMER_WAITING_FOR_TA_TIME);

	sceGpuSignalWait(sceKernelGetTLSAddr(0x44), 1000 * GLES1_DEFAULT_WAIT_RETRIES);

#if defined(PDUMP)

	PDUMP_STRING((gc, "Wait for TA \r\n"));

	if(PDUMP_ISCAPTURING(gc) && !gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->bSyncDumped)
	{
		PDUMP_STRING((gc, "Dumping sync object for %s buffer\r\n",pszBufferNames[CBUF_TYPE_VDM_CTRL_BUFFER]));

		PDUMP_MEM( gc,
					gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->psStatusUpdateMemInfo,
					0,
					sizeof(IMG_UINT32));

		gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->bSyncDumped = IMG_TRUE;
	}

	PVRSRVPDumpMemPol( gc->ps3DDevData->psConnection,
					   gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->psStatusUpdateMemInfo,
					   0, 								/* Offset */
					   gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->ui32CommittedHWOffsetInBytes,
					   0xFFFFFFFF, 						/* Mask */
					   PDUMP_POLL_OPERATOR_EQUAL,		/* Operator */
					   0); 								/* Flags */

#endif /* PDUMP */

    GLES1_TIME_STOP(GLES1_TIMER_WAITING_FOR_TA_TIME);      
}


/***********************************************************************************
 Function Name      : WaitForRender - Sync objects version
 Inputs             : gc, psRenderSurfaceSyncInfo
 Outputs            : -
 Returns            : -
 Description        : Waits for the hardware to complete a render. 
************************************************************************************/
static IMG_VOID WaitForRender(GLES1Context *gc, PVRSRV_CLIENT_SYNC_INFO *psRenderSurfaceSyncInfo)
{
    IMG_UINT32 ui32TriesLeft = GLES1_DEFAULT_WAIT_RETRIES;

	GLES1_ASSERT(psRenderSurfaceSyncInfo);
	
    GLES1_TIME_START(GLES1_TIMER_WAITING_FOR_3D_TIME);

	// TODOPSP2: will this even work?
	while(SGX2DQueryBlitsComplete(&gc->psSysContext->s3D, psRenderSurfaceSyncInfo, IMG_FALSE) != PVRSRV_OK)
	{
		if(!ui32TriesLeft)
		{
			PVR_DPF((PVR_DBG_ERROR, "WaitForRender: Timeout"));

			GLES1_TIME_STOP(GLES1_TIMER_WAITING_FOR_3D_TIME);

			return;
		}

		if(sceGpuSignalWait(sceKernelGetTLSAddr(0x44), 100000) != SCE_OK)
		{
			PVR_DPF((PVR_DBG_MESSAGE, "WaitForRender: sceGpuSignalWait failed"));
			ui32TriesLeft--;
		}
	}


#if defined(PDUMP)

	PDUMP_STRING((gc, "Wait for Render \r\n"));

	PVRSRVPDumpSyncPol(gc->ps3DDevData->psConnection, 
						psRenderSurfaceSyncInfo,
						IMG_FALSE,
						psRenderSurfaceSyncInfo->psSyncData->ui32LastOpDumpVal,
						0xFFFFFFFF);


#endif

	GLES1_TIME_STOP(GLES1_TIMER_WAITING_FOR_3D_TIME);
}


/***********************************************************************************
 Function Name      : SetupZLSControlReg
 Inputs             : gc, psRenderSurface, bOverflowRender, ui32ClearFlags
 Outputs            : -
 Returns            : -
 Description        : Sets up the force/enable depth/stencil load/store flags
 	 	 	 	 	  in the ZLS control register
************************************************************************************/
static IMG_VOID SetupZLSControlReg(GLES1Context *gc, EGLRenderSurface *psRenderSurface, IMG_BOOL bOverflowRender, IMG_UINT32 ui32ClearFlags)
{
	if(psRenderSurface->hEGLSurface && psRenderSurface->psZSBufferMemInfo)
	{
		IMG_UINT32 ui32ISPZLSControl;
	
		ui32ISPZLSControl = psRenderSurface->s3DRegs.sISPZLSControl.ui32RegVal;

		ui32ISPZLSControl &= (EUR_CR_ISP_ZLSCTL_ZLSEXTENT_MASK |
							  EUR_CR_ISP_ZLSCTL_ZLOADFORMAT_MASK |
							  EUR_CR_ISP_ZLSCTL_ZSTOREFORMAT_MASK |
#if defined(SGX_FEATURE_ZLS_EXTERNALZ)
							  EUR_CR_ISP_ZLSCTL_EXTERNALZBUFFER_MASK |
#endif
							  EUR_CR_ISP_ZLSCTL_LOADTILED_MASK |
							  EUR_CR_ISP_ZLSCTL_STORETILED_MASK);

		/* Set load/store bits depending on which Z buffer mode is enabled */
		switch(gc->sAppHints.ui32ExternalZBufferMode)
		{
			case EXTERNAL_ZBUFFER_MODE_ALLOC_UPFRONT_USED_ALWAYS:
			case EXTERNAL_ZBUFFER_MODE_ALLOC_ONDEMAND_USED_ALWAYS:
			{
				/*  Force z/s store */
				ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_FORCEZSTORE_MASK |
									 EUR_CR_ISP_ZLSCTL_ZSTOREEN_MASK |
				                   	 EUR_CR_ISP_ZLSCTL_SSTOREEN_MASK;

				/* We can only optimise the load if we're currently the active frame buffer */
				if(gc->sFrameBuffer.psActiveFrameBuffer == &gc->sFrameBuffer.sDefaultFrameBuffer)
				{
					/* Only do a z/s load if we're NOT doing a fullscreen z/s clear */
					if(((gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE) != 0) && !gc->bFullScreenScissor)
					{
						ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_ZLOADEN_MASK |
											 EUR_CR_ISP_ZLSCTL_SLOADEN_MASK |
											 EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK;
					}
					else
					{
						if((ui32ClearFlags & GLES1_CLEARFLAG_DEPTH) == 0)
						{
							ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_ZLOADEN_MASK | EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK;
						}

						if((ui32ClearFlags & GLES1_CLEARFLAG_STENCIL) == 0)
						{
							ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_SLOADEN_MASK | EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK;
						}
					}
				}
				else
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_ZLOADEN_MASK |
										 EUR_CR_ISP_ZLSCTL_SLOADEN_MASK |
										 EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK;
				}

				break;
			}
			case EXTERNAL_ZBUFFER_MODE_ALLOC_UPFRONT_USED_ASNEEDED:
			case EXTERNAL_ZBUFFER_MODE_ALLOC_ONDEMAND_USED_ASNEEDED:
			{
				/* Last render was an overflow so load ZS */
				if(psRenderSurface->bNeedZSLoadAfterOverflowRender)
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK |
										 EUR_CR_ISP_ZLSCTL_ZLOADEN_MASK | 
										 EUR_CR_ISP_ZLSCTL_SLOADEN_MASK;
				}

				/* This render is an overflow so store ZS */
				if(bOverflowRender)
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_FORCEZSTORE_MASK |
										 EUR_CR_ISP_ZLSCTL_ZSTOREEN_MASK |
					                   	 EUR_CR_ISP_ZLSCTL_SSTOREEN_MASK;
				}

				break;
			}
			case EXTERNAL_ZBUFFER_MODE_ALLOC_NEVER_USED_NEVER:
			{
				/* Shouldn't be possible to hit this condition */
				GLES1_ASSERT(gc->sAppHints.ui32ExternalZBufferMode!=EXTERNAL_ZBUFFER_MODE_ALLOC_NEVER_USED_NEVER);

				break;
			}
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"SetupZLSRegs: Bad external Z Buffer Mode (%d)", gc->sAppHints.ui32ExternalZBufferMode));

				break;
			}
		}

		GLES_SET_REGISTER(psRenderSurface->s3DRegs.sISPZLSControl, EUR_CR_ISP_ZLSCTL, ui32ISPZLSControl);
	}
	else
	{
		if(!psRenderSurface->hEGLSurface && (bOverflowRender || psRenderSurface->bNeedZSLoadAfterOverflowRender))
		{
			IMG_UINT32 ui32ISPZLSControl;

			ui32ISPZLSControl = psRenderSurface->s3DRegs.sISPZLSControl.ui32RegVal;

			ui32ISPZLSControl &= (EUR_CR_ISP_ZLSCTL_ZLSEXTENT_MASK |
								  EUR_CR_ISP_ZLSCTL_ZLOADFORMAT_MASK |
								  EUR_CR_ISP_ZLSCTL_ZSTOREFORMAT_MASK |
#if defined(SGX_FEATURE_ZLS_EXTERNALZ)
								  EUR_CR_ISP_ZLSCTL_EXTERNALZBUFFER_MASK |
#endif
								  EUR_CR_ISP_ZLSCTL_LOADTILED_MASK |
								  EUR_CR_ISP_ZLSCTL_STORETILED_MASK);

			/* Last render was an overflow so load ZS */
			if(psRenderSurface->bNeedZSLoadAfterOverflowRender)
			{
				if(psRenderSurface->s3DRegs.sISPZLoadBase.ui32RegVal)
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_ZLOADEN_MASK;
				}

				if(psRenderSurface->s3DRegs.sISPStencilLoadBase.ui32RegVal)
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_SLOADEN_MASK;
				}

				if(ui32ISPZLSControl & (EUR_CR_ISP_ZLSCTL_ZLOADEN_MASK|EUR_CR_ISP_ZLSCTL_SLOADEN_MASK))
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK;
				}
			}

			/* This render is an overflow so store ZS */
			if(bOverflowRender)
			{
				if(psRenderSurface->s3DRegs.sISPZLoadBase.ui32RegVal)
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_ZSTOREEN_MASK;
				}

				if(psRenderSurface->s3DRegs.sISPStencilLoadBase.ui32RegVal)
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_SSTOREEN_MASK;
				}

				if(ui32ISPZLSControl & (EUR_CR_ISP_ZLSCTL_ZSTOREEN_MASK|EUR_CR_ISP_ZLSCTL_SSTOREEN_MASK))
				{
					ui32ISPZLSControl |= EUR_CR_ISP_ZLSCTL_FORCEZSTORE_MASK;
				}
 			}

			GLES_SET_REGISTER(psRenderSurface->s3DRegs.sISPZLSControl, EUR_CR_ISP_ZLSCTL, ui32ISPZLSControl);
		}
	}
}	


/***********************************************************************************
 Function Name      : SetupZLSAddressSizeFormat
 Inputs             : psRenderSurface, ui32RoundedWidth, ui32RoundedHeight
 Outputs            : -
 Returns            : -
 Description        : Sets up the addresses, size and format ZLS registers
************************************************************************************/
static IMG_VOID SetupZLSAddressSizeFormat(EGLRenderSurface *psRenderSurface,
										  IMG_UINT32 ui32RoundedWidth,
										  IMG_UINT32 ui32RoundedHeight)
{

	IMG_UINT32 ui32ISPZLoadBase, ui32ISPZStoreBase, ui32ISPStencilLoadBase, ui32ISPStencilStoreBase;
	IMG_UINT32 ui32ISPZLSControl, ui32Extent, ui32Offset;

	GLES1_ASSERT(psRenderSurface->hEGLSurface && psRenderSurface->psZSBufferMemInfo);

	ui32Extent = ui32RoundedWidth/EURASIA_ISPREGION_SIZEX;

	if(psRenderSurface->bMultiSample)
	{
		ui32Extent *= 2;
	}

	ui32ISPZLSControl = psRenderSurface->s3DRegs.sISPZLSControl.ui32RegVal;

	ui32ISPZLSControl &= ~(EUR_CR_ISP_ZLSCTL_ZLSEXTENT_MASK |
						   EUR_CR_ISP_ZLSCTL_ZLOADFORMAT_MASK |
						   EUR_CR_ISP_ZLSCTL_ZSTOREFORMAT_MASK |
#if defined(SGX_FEATURE_ZLS_EXTERNALZ)
						   EUR_CR_ISP_ZLSCTL_EXTERNALZBUFFER_MASK |
#endif
						   EUR_CR_ISP_ZLSCTL_LOADTILED_MASK |
						   EUR_CR_ISP_ZLSCTL_STORETILED_MASK);

	ui32ISPZLSControl |= ((ui32Extent - 1) << EUR_CR_ISP_ZLSCTL_ZLSEXTENT_SHIFT) |
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

	if(psRenderSurface->bMultiSample)
	{
		ui32Offset *= 4;
	}

	/* Put stencil after depth in the buffer */
	ui32ISPStencilLoadBase  = psRenderSurface->psZSBufferMemInfo->sDevVAddr.uiAddr + ui32Offset;
	ui32ISPStencilStoreBase = psRenderSurface->psZSBufferMemInfo->sDevVAddr.uiAddr + ui32Offset;

	GLES_SET_REGISTER(psRenderSurface->s3DRegs.sISPZLSControl, EUR_CR_ISP_ZLSCTL, ui32ISPZLSControl);
	GLES_SET_REGISTER(psRenderSurface->s3DRegs.sISPZLoadBase, EUR_CR_ISP_ZLOAD_BASE, ui32ISPZLoadBase);
	GLES_SET_REGISTER(psRenderSurface->s3DRegs.sISPZStoreBase, EUR_CR_ISP_ZSTORE_BASE, ui32ISPZStoreBase);
	GLES_SET_REGISTER(psRenderSurface->s3DRegs.sISPStencilLoadBase, EUR_CR_ISP_STENCIL_LOAD_BASE, ui32ISPStencilLoadBase);
	GLES_SET_REGISTER(psRenderSurface->s3DRegs.sISPStencilStoreBase, EUR_CR_ISP_STENCIL_STORE_BASE, ui32ISPStencilStoreBase);
}


/***********************************************************************************
 Function Name      : GLESInitRegs
 Inputs             : gc, ui32ClearFlags
 Outputs            : gc->psRenderSurface->sTARegs, gc->psRenderSurface->s3DRegs
 Returns            : -
 Description        : Sets up some registers.
************************************************************************************/
static IMG_VOID GLESInitRegs(GLES1Context *gc, IMG_UINT32 ui32ClearFlags)
{
	GLESTARegisters *psTARegs = &gc->psRenderSurface->sTARegs;
	GLES3DRegisters *ps3DRegs = &gc->psRenderSurface->s3DRegs;
	GLES1_FUINT32 sFPUVal;
	IMG_UINT32 ui32MTERegister, ui32ISPIPFMisc;
#if !defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
	IMG_INT32 i32DepthBias;

	/* Depth bias: units adjust of SGX_FEATURE_DEPTH_MANTISSA_BITS */
	/* i32DepthBias = (-SGX_FEATURE_DEPTH_MANTISSA_BITS) & EUR_CR_ISP_DBIAS_UNITSADJ_MASK; */
	i32DepthBias = -SGX_FEATURE_DEPTH_MANTISSA_BITS;
	i32DepthBias = (IMG_INT32)((*((IMG_UINT32*)(&i32DepthBias))) & EUR_CR_ISP_DBIAS_UNITSADJ_MASK);
#endif


	/* Set OpenGL mode, enable W-clamping and set a threshold of 2 for global macro-tile */
	ui32MTERegister = (1 << EUR_CR_MTE_CTRL_WCLAMPEN_SHIFT)	|
					  (1 << EUR_CR_MTE_CTRL_OPENGL_SHIFT)	|
#if !defined(FIX_HW_BRN_28493) && !defined(FIX_HW_BRN_35498)
					  (1 << EUR_CR_MTE_CTRL_NUM_PARTITIONS_SHIFT)	|
#endif
					  (2 << EUR_CR_MTE_CTRL_GLOBALMACROTILETHRESH_SHIFT);

	GLES_SET_REGISTER(psTARegs->sMTEControl, EUR_CR_MTE_CTRL, ui32MTERegister);

	/* Initialise SW 3D registers */
	GLES_SET_REGISTER(ps3DRegs->sPixelBE, EUR_CR_PIXELBE, 0);

	/* Ensure no pass spawn on first object (assumed to be colormasked on) */
	ui32ISPIPFMisc = (0xF << EUR_CR_ISP_IPFMISC_UPASSSTART_SHIFT);

	GLES_SET_REGISTER(ps3DRegs->sISPIPFMisc, EUR_CR_ISP_IPFMISC, ui32ISPIPFMisc);

#if defined (SGX545)
	/* Set DX10 & OGL sampling (0.5,0.5) for thin lines */
	GLES_SET_REGISTER(ps3DRegs->sISPFPUCtrl, EUR_CR_ISP_FPUCTRL, (1 << EUR_CR_ISP_FPUCTRL_SAMPLE_POS_SHIFT));
#else /* defined (SGX545) */
	sFPUVal.fVal = 1.0e-20f;
	GLES_SET_REGISTER(ps3DRegs->sISPPerpendicular, EUR_CR_ISP_PERPENDICULAR, sFPUVal.ui32Val);
	GLES_SET_REGISTER(ps3DRegs->sISPCullValue, EUR_CR_ISP_CULLVALUE, sFPUVal.ui32Val);
#endif /* defined (SGX545) */

#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
	GLES_SET_REGISTER(ps3DRegs->sISPDepthBias, EUR_CR_ISP_DBIAS0,  gc->sState.sPolygon.i32Units);
	GLES_SET_REGISTER(ps3DRegs->sISPDepthBias1, EUR_CR_ISP_DBIAS1, gc->sState.sPolygon.factor.ui32Val);
#else
	GLES_SET_REGISTER(ps3DRegs->sISPDepthBias, EUR_CR_ISP_DBIAS, (IMG_UINT32)i32DepthBias);
#endif

	SetupZLSControlReg(gc, gc->psRenderSurface, IMG_FALSE, ui32ClearFlags);
	
	/* Background object depth & stencil/viewport registers */
	sFPUVal.fVal = GLES1_One;
	GLES_SET_REGISTER(ps3DRegs->sISPBackgroundObject, EUR_CR_ISP_BGOBJ, (1 << EUR_CR_ISP_BGOBJ_MASK_SHIFT) |
																   (1 << EUR_CR_ISP_BGOBJ_ENABLEBGTAG_SHIFT));
	GLES_SET_REGISTER(ps3DRegs->sISPBackgroundObjectDepth, EUR_CR_ISP_BGOBJDEPTH, sFPUVal.ui32Val);

	GLES_SET_REGISTER(ps3DRegs->sEDMPixelPDSExec, EUR_CR_EVENT_PIXEL_PDS_EXEC, 0);
	GLES_SET_REGISTER(ps3DRegs->sEDMPixelPDSData, EUR_CR_EVENT_PIXEL_PDS_DATA, 0);
	GLES_SET_REGISTER(ps3DRegs->sEDMPixelPDSInfo, EUR_CR_EVENT_PIXEL_PDS_INFO, 0);
}


/***********************************************************************************
 Function Name      : ValidateMemory
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : UTILITY: It checks if memory has been invalidated by a power event
						by looking at the dummy texture mem_info
************************************************************************************/
static IMG_BOOL ValidateMemory(GLES1Context *gc)
{
#if defined(LOCAL_MEMORY_ARCHITECTURE)
	if ((gc->psRenderSurface != IMG_NULL) && (gc->psRenderSurface->psTARenderInfo != NULL))
	{
		return !gc->psRenderSurface->psTARenderInfo->psSharedData->bMemoryDirty;
	}
#else
	PVR_UNREFERENCED_PARAMETER(gc);
#endif
	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : InsertItemIntoFlushList
 Inputs             : gc, psRenderSurface, psTex
 Outputs            : -
 Returns            : Success/failure
 Description        : Inserts an item into the shared flush list
************************************************************************************/
static IMG_BOOL InsertItemIntoFlushList(GLES1Context *gc, EGLRenderSurface *psRenderSurface, GLESTexture *psTex)
{
	GLES1SurfaceFlushList *psItem;

	psItem = GLES1Malloc(gc, sizeof(GLES1SurfaceFlushList));

	if(psItem == IMG_NULL)
	{
		return IMG_FALSE;
	}

	PVRSRVLockMutex(gc->psSharedState->hFlushListLock);

	psItem->gc				= gc;
	psItem->psRenderSurface = psRenderSurface;
	psItem->psTex			= psTex;
	psItem->psNext			= IMG_NULL;

	/* Insert at the end of the list */
	if(gc->psSharedState->psFlushList==IMG_NULL)
	{
		gc->psSharedState->psFlushList = psItem;
	}
	else
	{
		GLES1SurfaceFlushList *psInsertPoint;

		psInsertPoint = gc->psSharedState->psFlushList;

		while(psInsertPoint->psNext)
		{
			psInsertPoint = psInsertPoint->psNext;
		}

		psInsertPoint->psNext = psItem;
	}

	PVRSRVUnlockMutex(gc->psSharedState->hFlushListLock);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : StartFrame
 Inputs             : gc, pui32ClearFlags
 Outputs            : -
 Returns            : -
 Description        : Starts a new frame. Initialises registers
************************************************************************************/
static IMG_BOOL StartFrame(GLES1Context *gc, IMG_UINT32 *pui32ClearFlags, PVRSRV_CLIENT_SYNC_INFO *psAccumSyncInfo)
{
	EGLRenderSurface *psRenderSurface = gc->psRenderSurface;
	GLES3DRegisters *ps3DRegs = &gc->psRenderSurface->s3DRegs;
	IMG_UINT32 ui32Right, ui32Bottom;
	IMG_BOOL bFullScreenObject = IMG_FALSE;

	/* Try to get rid of texture and shader ghosts */
	KRM_DestroyUnneededGhosts(gc, &gc->psSharedState->psTextureManager->sKRM);
	KRM_DestroyUnneededGhosts(gc, &gc->psSharedState->sUSEShaderVariantKRM);
	
	GLESInitRegs(gc, *pui32ClearFlags);

	/* The CBUF_GetBufferSpace code will spin as long as there are outstanding HW ops, and only then fail, 
	 * so we should not be able to fail this call as we are at the start of a frame. 
	 */ 
	if(SetupPixelEventProgram(gc, &psRenderSurface->sPixelBEState, IMG_FALSE) != GLES1_NO_ERROR)
	{
		return IMG_FALSE;
	}

	ps3DRegs->sEDMPixelPDSData = psRenderSurface->sPixelBEState.sEventPixelData;
	ps3DRegs->sEDMPixelPDSExec = psRenderSurface->sPixelBEState.sEventPixelExec;
	ps3DRegs->sEDMPixelPDSInfo = psRenderSurface->sPixelBEState.sEventPixelInfo;

	/* The CBUF_GetBufferSpace code will spin as long as there are outstanding HW ops, and only then fail, 
	 * so we should not be able to fail this call as we are at the start of a frame. 
	 */ 
	if(SetupBGObject(gc, IMG_FALSE, psRenderSurface->aui32HWBGObjPDSState) != GLES1_NO_ERROR)
	{
		return IMG_FALSE;
	}

	/*
		Snap Terminate clip rect to nearest 16 pixel units outside drawable rectangle.
	*/
	ui32Right = (gc->psDrawParams->ui32Width + EURASIA_TARNDCLIP_LR_ALIGNSIZE - 1) & 
											~(EURASIA_TARNDCLIP_LR_ALIGNSIZE - 1);
	ui32Bottom = (gc->psDrawParams->ui32Height + EURASIA_TARNDCLIP_TB_ALIGNSIZE - 1) & 
											~(EURASIA_TARNDCLIP_TB_ALIGNSIZE - 1);

	/*
		Work out positions in 16 pixel units
	*/
	ui32Right  = (ui32Right >> EURASIA_TARNDCLIP_LR_ALIGNSHIFT) - 1;
	ui32Bottom = (ui32Bottom >> EURASIA_TARNDCLIP_TB_ALIGNSHIFT) - 1;

	psRenderSurface->ui32TerminateRegion =	(0 << EURASIA_TARNDCLIP_LEFT_SHIFT)				|
											(ui32Right << EURASIA_TARNDCLIP_RIGHT_SHIFT)	|
											(0 << EURASIA_TARNDCLIP_TOP_SHIFT)				|
											(ui32Bottom << EURASIA_TARNDCLIP_BOTTOM_SHIFT);

	/* Set region clip to full screen */
	CalcRegionClip(gc, IMG_NULL, psRenderSurface->aui32RegionClipWord);
	
	psRenderSurface->bLastDrawMaskFullScreenEnable = IMG_TRUE;

	gc->psRenderSurface->sLastDrawMask.i32X = 0;
	gc->psRenderSurface->sLastDrawMask.i32Y = 0;
	
	gc->psRenderSurface->sLastDrawMask.ui32Width = gc->psDrawParams->ui32Width;
	gc->psRenderSurface->sLastDrawMask.ui32Height = gc->psDrawParams->ui32Height;

#if defined(FIX_HW_BRN_23259) || defined(FIX_HW_BRN_23020) || defined(FIX_HW_BRN_26704)

	/* 
	 * The HW BGO doesn't initialise the depth properly, so insert a fullscreen depth clear object (of depth 1.0)
	 * if we're not loading depth and there's either no depth clear, or a non-fullscreen depth clear, at SOF
	 */
	if(((ps3DRegs->sISPZLSControl.ui32RegVal & EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK) == 0) &&
	  (((*pui32ClearFlags & GLES1_CLEARFLAG_DEPTH) == 0) || (((gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE) != 0) && !gc->bFullScreenScissor)))
	{
		if(SendClearPrims(gc, GLES1_CLEARFLAG_DEPTH, IMG_TRUE, 1.0f) != GLES1_NO_ERROR)
		{
			return IMG_FALSE;
		}		

		bFullScreenObject = IMG_TRUE;
	}

#endif /* defined(FIX_HW_BRN_23259) || defined(FIX_HW_BRN_23020) */

	/* If the first object in the scene isn't a full screen clear, we may need to accumulate */
	if(((*pui32ClearFlags & GLES1_CLEARFLAG_COLOR) == 0) || (((gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE) != 0) && !gc->bFullScreenScissor))
	{
		/* SW accumulate object if accum address is different to render address, otherwise HW BG object
		 * will suffice.
		 */
		if(!psRenderSurface->bInExternalFrame &&
			gc->psDrawParams->ui32AccumHWAddress != gc->psDrawParams->ui32HWSurfaceAddress)
		{
			IMG_BOOL bFullScreenDepthClear;

			if( (((gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE) != 0) && !gc->bFullScreenScissor) ||
				((*pui32ClearFlags & GLES1_CLEARFLAG_DEPTH) == 0))
			{
				bFullScreenDepthClear = IMG_FALSE;
			}
			else
			{
				/* Remove the depth clear flag as the accumulation object will handle it */
				*pui32ClearFlags &= ~GLES1_CLEARFLAG_DEPTH;

				bFullScreenDepthClear = IMG_TRUE;
			}

			/* The CBUF_GetBufferSpace code will spin as long as there are outstanding HW ops, and only then fail, 
			 * so we should not be able to fail this call as we are at the start of a frame. 
			 */ 
			if(SendAccumulateObject(gc, bFullScreenDepthClear, gc->sState.sDepth.fClear) != GLES1_NO_ERROR)
			{
				return IMG_FALSE;
			}		
			
			bFullScreenObject = IMG_TRUE;

#if defined(ANDROID)
			if(psAccumSyncInfo)
			{
				GLES1_ASSERT(psRenderSurface->ui32NumSrcSyncs == 0);
				psRenderSurface->apsSrcSurfSyncInfo[0] = psAccumSyncInfo;
				psRenderSurface->ui32NumSrcSyncs = 1;
			}
#else
			PVR_UNREFERENCED_PARAMETER(psAccumSyncInfo);
#endif
		}
	}
	else
	{
		bFullScreenObject = IMG_TRUE;
	}

#if defined(FIX_HW_BRN_32044)
	/* Insert a fullscreen viewport enable if Z loading and no other fullscreen object has been sent */
	if(!bFullScreenObject && (ps3DRegs->sISPZLSControl.ui32RegVal & EUR_CR_ISP_ZLSCTL_FORCEZLOAD_MASK))
	{
		gc->psRenderSurface->bLastDrawMaskFullScreenEnable = IMG_FALSE;

		SendDrawMaskRect(gc, IMG_NULL, IMG_TRUE);
	}
#endif

	/* Reset the render surface flags */
	psRenderSurface->bInFrame = IMG_TRUE;
	psRenderSurface->bFirstKick = IMG_TRUE;
	psRenderSurface->bInExternalFrame = IMG_TRUE;

	/* Add FBO surface to flush list for this context */
	if(psRenderSurface->hEGLSurface == IMG_NULL)
	{
		GLES1FrameBufferAttachable *psColorAttachment;
		GLESTexture *psTex;

		psColorAttachment = gc->sFrameBuffer.psActiveFrameBuffer->apsAttachment[GLES1_COLOR_ATTACHMENT];

		if(psColorAttachment && (psColorAttachment->eAttachmentType==GL_TEXTURE))
		{
			psTex = ((GLESMipMapLevel *)psColorAttachment)->psTex;
		}
		else
		{
			psTex = IMG_NULL;
		}

		if(!InsertItemIntoFlushList(gc, psRenderSurface, psTex))
		{
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : PrepareToDraw
 Inputs             : gc, ui32ClearFlags
 Outputs            : -
 Returns            : -
 Description        : Called prior to primitive drawing. Sends required 
					  clears/drawmasks. Starts a frame if appropriate.
************************************************************************************/
IMG_INTERNAL IMG_BOOL PrepareToDraw(GLES1Context *gc, IMG_UINT32 *pui32ClearFlags, IMG_BOOL bTakeLock)
{
	EGLDrawableParams *psDrawParams = gc->psDrawParams;
	EGLDrawableParams sParams;
	IMG_BOOL bClear = IMG_FALSE;
	PVRSRV_MUTEX_HANDLE hSurfaceMutex = IMG_NULL;


	GLES1_TIME_START(GLES1_TIMER_PREPARE_TO_DRAW_TIME);

	if(*pui32ClearFlags)
		bClear = IMG_TRUE;

	if(gc->psRenderSurface)
	{
		if(bTakeLock)
		{
			PVRSRVLockMutex(gc->psRenderSurface->hMutex);
		}

		hSurfaceMutex = gc->psRenderSurface->hMutex;
	}

	if(gc->psRenderSurface && gc->psRenderSurface->bInFrame)
	{
#if defined(FIX_HW_BRN_25077)
		if(gc->bDrawMaskInvalid && gc->psMode->ui32StencilBits)
		{
			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_RENDERSTATE;
		}
#endif

		if(gc->bDrawMaskInvalid && !bClear)
		{
			if(SendDrawMaskForPrimitive(gc) != GLES1_NO_ERROR)
			{
				SetError(gc, GL_OUT_OF_MEMORY);
				PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);

				GLES1_TIME_STOP(GLES1_TIMER_PREPARE_TO_DRAW_TIME);

				return IMG_FALSE;
			}
		}
	
#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
		if(!bClear && ((gc->ui32RasterEnables & GLES1_RS_POLYOFFSET_ENABLE) != 0) && 
		   (gc->sPrim.eCurrentPrimitiveType>=GLES1_PRIMTYPE_TRIANGLE) &&
		  (gc->sPrim.eCurrentPrimitiveType<=GLES1_PRIMTYPE_TRIANGLE_FAN))
		{

			if(gc->psRenderSurface->ui32DepthBiasState == EGL_RENDER_DEPTHBIAS_UNUSED)
			{
				GLES_SET_REGISTER(gc->psRenderSurface->s3DRegs.sISPDepthBias, EUR_CR_ISP_DBIAS0, gc->sState.sPolygon.i32Units);
				GLES_SET_REGISTER(gc->psRenderSurface->s3DRegs.sISPDepthBias1, EUR_CR_ISP_DBIAS1, gc->sState.sPolygon.factor.ui32Val);

				gc->psRenderSurface->ui32DepthBiasState = EGL_RENDER_DEPTHBIAS_REGISTER_USED;
			}
			else if(gc->psRenderSurface->ui32DepthBiasState == EGL_RENDER_DEPTHBIAS_REGISTER_USED)
			{
				if((gc->psRenderSurface->s3DRegs.sISPDepthBias.ui32RegVal != gc->sState.sPolygon.i32Units) ||
					(gc->psRenderSurface->s3DRegs.sISPDepthBias1.ui32RegVal != gc->sState.sPolygon.factor.ui32Val))
				{
					if(SendDepthBiasPrims(gc) != GLES1_NO_ERROR)
					{
						SetError(gc, GL_OUT_OF_MEMORY);
						PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);

						GLES1_TIME_STOP(GLES1_TIMER_PREPARE_TO_DRAW_TIME);

						return IMG_FALSE;
					}
				}
			}
			else if((gc->psRenderSurface->fLastFactorInObject != gc->sState.sPolygon.factor.fVal) ||
					(gc->psRenderSurface->i32LastUnitsInObject != gc->sState.sPolygon.i32Units))
			{
				if(SendDepthBiasPrims(gc) != GLES1_NO_ERROR)
				{
					SetError(gc, GL_OUT_OF_MEMORY);
					PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);

					GLES1_TIME_STOP(GLES1_TIMER_PREPARE_TO_DRAW_TIME);

					return IMG_FALSE;
				}
			}
		}
#endif
	}
	else
	{
		PVRSRV_CLIENT_SYNC_INFO *psAccumSyncInfo = IMG_NULL;

		/* Check for resize/invalid drawable if we are using an EGL surface */
		if(gc->sFrameBuffer.psActiveFrameBuffer == &gc->sFrameBuffer.sDefaultFrameBuffer)
		{
			/* No render surface -> invalidated drawable */
			if(!gc->psRenderSurface || 
			   !gc->psRenderSurface->bInExternalFrame)
			{
				IMG_BOOL bCreateZSBuffer;

				if(!KEGLGetDrawableParameters(gc->hEGLSurface, &sParams, IMG_TRUE))
				{
					PVR_DPF((PVR_DBG_ERROR,"PrepareToDraw: KEGLGetDrawableParameters() failed"));
		
					/* Unlock old mutex (which may not be in gc->psRenderSurface anymore) */
					if(hSurfaceMutex)
					{
						PVRSRVUnlockMutex(hSurfaceMutex);
					}

					GLES1_TIME_STOP(GLES1_TIMER_PREPARE_TO_DRAW_TIME);

					return IMG_FALSE;
				}
				
				/* Unlock old mutex (which may not be in gc->psRenderSurface anymore) */
				if(hSurfaceMutex)
				{
					PVRSRVUnlockMutex(hSurfaceMutex);
				}

				GLES1_ASSERT(sParams.psRenderSurface);
				PVRSRVLockMutex(sParams.psRenderSurface->hMutex);

				/* If the last frame went into SPM create a ZS buffer if we don't already have one */
				if(gc->bSPMOutOfMemEvent && !sParams.psRenderSurface->psZSBufferMemInfo &&
					(gc->sAppHints.ui32ExternalZBufferMode!=EXTERNAL_ZBUFFER_MODE_ALLOC_NEVER_USED_NEVER))
				{
					PVR_DPF((PVR_DBG_WARNING,"PrepareToDraw: Creating ZS buffer for SPM"));

					bCreateZSBuffer = IMG_TRUE;
				}
				else
				{
					bCreateZSBuffer = IMG_FALSE;
				}

				gc->bSPMOutOfMemEvent = IMG_FALSE;

				if((sParams.ui32Width  != gc->psDrawParams->ui32Width) ||
				   (sParams.ui32Height != gc->psDrawParams->ui32Height) ||
				    bCreateZSBuffer)
				{
					IMG_BOOL bMultiSample;

					if(gc->psMode->ui32AntiAliasMode && ((gc->ui32FrameEnables & GLES1_FS_MULTISAMPLE_ENABLE) != 0))
					{
						bMultiSample = IMG_TRUE;
					}
					else
					{
						bMultiSample = IMG_FALSE;
					}

					ChangeDrawableParams(gc, &gc->sFrameBuffer.sDefaultFrameBuffer, 
										&gc->sFrameBuffer.sDefaultFrameBuffer.sReadParams, &sParams);
					
					if(!KEGLResizeRenderSurface(gc->psSysContext, 
												&sParams, 
												bMultiSample, 
												bCreateZSBuffer,
												gc->psRenderSurface))
					{
						PVR_DPF((PVR_DBG_ERROR,"PrepareToDraw: KEGLResizeRenderSurface() failed"));
						SetError(gc, GL_OUT_OF_MEMORY);
						PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);

						GLES1_TIME_STOP(GLES1_TIMER_PREPARE_TO_DRAW_TIME);

						return IMG_FALSE;
					}

					gc->psRenderSurface->bFirstKick       = IMG_TRUE;
					gc->psRenderSurface->bInFrame         = IMG_FALSE;
					gc->psRenderSurface->bInExternalFrame = IMG_FALSE;
				}
				else
				{
					if(!gc->psRenderSurface)
					{
						ChangeDrawableParams(gc, &gc->sFrameBuffer.sDefaultFrameBuffer, 
											&gc->sFrameBuffer.sDefaultFrameBuffer.sReadParams, &sParams);
					}
					
					/* Copy new draw params */
					gc->sFrameBuffer.sDefaultFrameBuffer.sDrawParams = sParams;
				}

				/* If draw params address has changed (due to back buffer swapping), update the read params
				 * if they point to the same surface
				 */ 
				if(gc->psDrawParams->psRenderSurface == gc->psReadParams->psRenderSurface)
				{
					gc->sFrameBuffer.sDefaultFrameBuffer.sReadParams = sParams;
				}

				if(sParams.psAccumSyncInfo != sParams.psSyncInfo)
				{
					psAccumSyncInfo = sParams.psAccumSyncInfo;
				}
			}
		}

		if(!StartFrame(gc, pui32ClearFlags, psAccumSyncInfo))
		{
			PVR_DPF((PVR_DBG_ERROR,"Start Frame failed"));
			PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);

			return IMG_FALSE;
		}

		/* 
		** Drawmask inits to all on unless ZLoad with viewport - check if drawmask object is 
		** necessary 
		*/
		if (((gc->ui32FrameEnables & GLES1_FS_SCISSOR_ENABLE) != 0) ||
			!((gc->sState.sViewport.i32X == 0) && (gc->sState.sViewport.i32Y == 0) &&
			(gc->sState.sViewport.ui32Width == psDrawParams->ui32Width) &&
			(gc->sState.sViewport.ui32Height == psDrawParams->ui32Height)))
		{
			gc->bDrawMaskInvalid = IMG_TRUE;
		}

		if(gc->bDrawMaskInvalid && !*pui32ClearFlags)
		{
			/* The CBUF_GetBufferSpace code will spin as long as there are outstanding HW ops, and only then fail, 
			 * so we should not be able to fail this call as we are at the start of a frame. 
			 */ 
			if(SendDrawMaskForPrimitive(gc) != GLES1_NO_ERROR)
			{
				SetError(gc, GL_OUT_OF_MEMORY);

				GLES1_TIME_STOP(GLES1_TIMER_PREPARE_TO_DRAW_TIME);
				PVRSRVUnlockMutex(gc->psRenderSurface->hMutex);

				return IMG_FALSE;
			}
		}

#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)

		gc->psRenderSurface->ui32DepthBiasState = EGL_RENDER_DEPTHBIAS_UNUSED;

		if(!bClear && ((gc->ui32RasterEnables & GLES1_RS_POLYOFFSET_ENABLE) != 0) && 
		   (gc->sPrim.eCurrentPrimitiveType>=GLES1_PRIMTYPE_TRIANGLE) &&
		  (gc->sPrim.eCurrentPrimitiveType<=GLES1_PRIMTYPE_TRIANGLE_FAN))
		{
			gc->psRenderSurface->ui32DepthBiasState = EGL_RENDER_DEPTHBIAS_REGISTER_USED;
		}
#endif
	}


	GLES1_TIME_STOP(GLES1_TIMER_PREPARE_TO_DRAW_TIME);

	return IMG_TRUE;
}





#if defined(PDUMP)

/***********************************************************************************
 Function Name      : PDumpBufferObject
 Inputs             : gc, pvFunctionContext, psItem
 Outputs            : -
 Returns            : -
 Description        : PDumps a buffer object. Used by ScheduleTA()
************************************************************************************/
static IMG_VOID PDumpBufferObject(GLES1Context *gc, const IMG_VOID* pvFunctionContext, GLES1NamedItem *psItem)
{
	GLESBufferObject *psBufferObject = (GLESBufferObject*)psItem;

	PVR_UNREFERENCED_PARAMETER(pvFunctionContext);

	GLES1_ASSERT(psBufferObject);

	if (psBufferObject->psMemInfo && !psBufferObject->bDumped)
	{
		PDUMP_STRING((gc, "Dumping buffer object at device virtual address 0x%x\r\n",
						psBufferObject->psMemInfo->sDevVAddr.uiAddr));

		PDUMP_MEM(gc, psBufferObject->psMemInfo, 0,
		          psBufferObject->psMemInfo->uAllocSize);
	
		psBufferObject->bDumped = IMG_TRUE;
	}
}

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)

/***********************************************************************************
 Function Name      : PDumpVertexArrayObject
 Inputs             : gc, pvFunctionContext, psItem
 Outputs            : -
 Returns            : -
 Description        : PDumps a buffer object. Used by ScheduleTA()
************************************************************************************/
static IMG_VOID PDumpVertexArrayObject(GLES1Context *gc, const IMG_VOID* pvFunctionContext, GLES1NamedItem *psItem)
{
	GLES1VertexArrayObject *psVAO = (GLES1VertexArrayObject*)psItem;

	PVR_UNREFERENCED_PARAMETER(pvFunctionContext);

	GLES1_ASSERT(psVAO);

	if (psVAO->psMemInfo && !psVAO->bDumped)
	{
		PDUMP_STRING((gc, "Dumping vertex array object PDS program at device virtual address 0x%x\r\n",
						psVAO->psMemInfo->sDevVAddr.uiAddr));

		PDUMP_MEM(gc, psVAO->psMemInfo, 0, psVAO->psMemInfo->uAllocSize);
		
		psVAO->bDumped = IMG_TRUE;
	}
}
#endif

/***********************************************************************************
 Function Name      : PDumpTerminate
 Inputs             : gc, psItem
 Outputs            : -
 Returns            : -
 Description        : PDumps the terminate state. Used in ScheduleTA()
************************************************************************************/
static IMG_VOID PDumpTerminate(GLES1Context *gc, EGLTerminateState *psTerminateState)
{
	PDUMP_STRING((gc, "Dumping Terminate SA update PDS Program at device virtual address 0x%x\r\n", 
							psTerminateState->psSAUpdatePDSMemInfo->sDevVAddr.uiAddr));
	
	PDUMP_MEM(gc, psTerminateState->psSAUpdatePDSMemInfo, 0, psTerminateState->psSAUpdatePDSMemInfo->uAllocSize);

	PDUMP_STRING((gc, "Dumping Terminate State PDS Program at device virtual address 0x%x\r\n",
		psTerminateState->psTerminatePDSMemInfo->sDevVAddr.uiAddr));

	PDUMP_MEM(gc, psTerminateState->psTerminatePDSMemInfo, 0, psTerminateState->psTerminatePDSMemInfo->uAllocSize);

	PDUMP_STRING((gc, "Dumping Terminate State USE Program at device virtual address 0x%x\r\n",
		psTerminateState->psTerminateUSEMemInfo->sDevVAddr.uiAddr));

	PDUMP_MEM(gc, psTerminateState->psTerminateUSEMemInfo, 0, psTerminateState->psTerminateUSEMemInfo->uAllocSize);
}


/***********************************************************************************
 Function Name      : PDumpShaders
 Inputs             : gc,
 Outputs            : -
 Returns            : -
 Description        : PDumps any shaders that haven't already been dumped
************************************************************************************/
static IMG_VOID PDumpShaders(GLES1Context *gc)
{
	UCH_UseCodeBlock *psCodeBlock;
	UCH_UseCodeHeap   *apsHeap[4];
	IMG_UINT32 i, ui32Offset;

	apsHeap[0] = gc->psSharedState->psUSEVertexCodeHeap;
	apsHeap[1] = gc->psSharedState->psUSEFragmentCodeHeap;
	apsHeap[2] = gc->psSharedState->psPDSFragmentCodeHeap;
	apsHeap[3] = gc->psSharedState->psPDSVertexCodeHeap;

	/* For every heap, dump all blocks that have been allocated.
	   Blocks owned by USE variant ghosts are tricky as they have no pointer back to the ghost that owns them.
	*/
	for(i=0; i < 4; ++i)
	{
		psCodeBlock = apsHeap[i]->psAllocatedBlockList;

		while(psCodeBlock)
		{
			if(!psCodeBlock->bDumped)
			{
				ui32Offset = (IMG_UINT32)((IMG_UINT8 *)psCodeBlock->pui32LinAddress - (IMG_UINT8 *) psCodeBlock->psCodeMemory->pvLinAddr);
				
				PDUMP_STRING((gc, "Dumping USSE/PDS Vertex/Fragment Code Block at device virtual address 0x%x\r\n",
					psCodeBlock->psCodeMemory->sDevVAddr.uiAddr + ui32Offset));
				PDUMP_MEM(gc, psCodeBlock->psCodeMemory, ui32Offset, psCodeBlock->ui32Size);
				
				psCodeBlock->bDumped = IMG_TRUE;
			}
			psCodeBlock = psCodeBlock->psNext;
		}
	}
}	

/***********************************************************************************
 Function Name      : PDumpTextures
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : PDumps any textures that haven't already been dumped, including ghosts.
************************************************************************************/
static IMG_VOID PDumpTextures(GLES1Context *gc)
{
	KRMResource *psResource;
	GLESTexture       *psTex;
	GLESGhost         *psGhost;

	/* Iterate all textures in the list*/
	psResource = gc->psSharedState->psTextureManager->sKRM.psResourceList;

	while(psResource)
	{
		/* Unavoidable tricky pointer arithmetic */
		psTex = (GLESTexture*)((IMG_UINTPTR_T)psResource -offsetof(GLESTexture, sResource));

		/* This function will only dump textures that are marked */
		PDumpTexture(gc, psTex);

		psResource = psResource->psNext;
	}

	if(PDUMP_ISCAPTURING(gc))
	{
		/* Iterate the ghosts in the list */
		psResource = gc->psSharedState->psTextureManager->sKRM.psGhostList;

		while(psResource)
		{
			/* Unavoidable tricky pointer arithmetic */
			psGhost = (GLESGhost*)((IMG_UINTPTR_T)psResource -offsetof(GLESGhost, sResource));

			if(!psGhost->bDumped)
			{
				PDUMP_STRING((gc, "Dumping ghost of texture\r\n"));
				PDUMP_MEM(gc, psGhost->psMemInfo, 0, psGhost->ui32Size);
				psGhost->bDumped = IMG_TRUE;
			}

			psResource = psResource->psNext;
		}
	}
}

#endif /* defined(PDUMP) */


/***********************************************************************************
 Function Name      : KickUnFlushed_ScheduleTA
 Inputs             : pvContext, pvRenderSurface
 Outputs            : 
 Returns            : 
 Description        : Called by the KRM code when kicking un-flushed resources
************************************************************************************/
IMG_INTERNAL IMG_VOID KickUnFlushed_ScheduleTA(IMG_VOID *pvContext, IMG_VOID *pvRenderSurface)
{
	EGLRenderSurface *psRenderSurface;
	IMG_UINT32 ui32Flags;
	GLES1Context *gc;

	gc = (GLES1Context *)pvContext;
	
	if (pvRenderSurface)
	{
		ui32Flags = GLES1_SCHEDULE_HW_LAST_IN_SCENE;

		psRenderSurface = (EGLRenderSurface *)pvRenderSurface;

		if(psRenderSurface->hEGLSurface)
		{
			ui32Flags |= GLES1_SCHEDULE_HW_MIDSCENE_RENDER;
		}
	}
	else
	{
		ui32Flags = 0;

		psRenderSurface = gc->psRenderSurface;
	}

	FlushRenderSurface(gc, psRenderSurface, ui32Flags);
}	


/***********************************************************************************
 Function Name      : KickLimit_ScheduleTA
 Inputs             : pvContext, bLastInScene
 Outputs            : 
 Returns            : 
 Description        : Called by the circular buffer code to kick the TA when
 					  the single-kick-limit has been exceeded for a buffer
************************************************************************************/
IMG_INTERNAL IMG_VOID KickLimit_ScheduleTA(IMG_VOID *pvContext, IMG_BOOL bLastInScene)
{
	GLES1Context *gc = (GLES1Context *)pvContext;
	IMG_UINT32 ui32Flags = 0;

	if (bLastInScene)
	{
		ui32Flags = GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_MIDSCENE_RENDER;
	}

	ScheduleTA(gc, gc->psRenderSurface, ui32Flags);
}	

/***********************************************************************************
 Function Name      : DoKickTA
 Inputs             : gc, psRenderSurface, ui32Flags
 Outputs            : -
 Returns            : Success
 Description        : Unconditionally kicks a TA/render. Only to be called by ScheduleTA!
************************************************************************************/
static IMG_EGLERROR DoKickTA(GLES1Context *gc, EGLRenderSurface *psRenderSurface, IMG_UINT32 ui32KickFlags)
{
	SGX_KICKTA *psKickTA = &gc->sKickTA;
	SGX_KICKTA_OUTPUT sKickTAOutput;
	IMG_VOID *pvPDumpData = IMG_NULL;
	IMG_UINT32 ui32Flags = 0;
	GLESTARegisters *psTARegs = &psRenderSurface->sTARegs;
	GLES3DRegisters *ps3DRegs = &psRenderSurface->s3DRegs;
	IMG_UINT32 *pui32PBEState = psRenderSurface->sPixelBEState.aui32EmitWords;
#if defined(PDUMP)
#if defined(SGX545)
	IMG_UINT32 ui32PBESideband = psRenderSurface->sPixelBEState.ui32SidebandWord;
#endif /* defined(SGX545) */
	SGX_KICKTA_PDUMP sKickTAPDump = {0};
	SGX_KICKTA_DUMPBITMAP sDumpBitmap;
	SGX_KICKTA_DUMP_BUFFER sDumpBuffer[CBUF_NUM_BUFFERS];
#endif
	PVRSRV_ERROR eError;
	IMG_UINT32 i, j;
	IMG_BOOL bOverflowRender = IMG_FALSE;

	/* Discard is like kicking a render */
	if(ui32KickFlags & GLES1_SCHEDULE_HW_DISCARD_SCENE)
	{
		ui32KickFlags |= GLES1_SCHEDULE_HW_LAST_IN_SCENE;
	}

	if(ui32KickFlags & GLES1_SCHEDULE_HW_MIDSCENE_RENDER)
	{
		bOverflowRender = IMG_TRUE;
	}


	if(psRenderSurface->bFirstKick)
	{
		IMG_BOOL bMultiSample  = IMG_FALSE;
		IMG_BOOL bDither = IMG_FALSE;
		IMG_BOOL bMultiSampleChanged = IMG_FALSE;
		IMG_BOOL bDitherChanged = IMG_FALSE;

		if(psRenderSurface->hEGLSurface && gc->psMode->ui32AntiAliasMode && ((gc->ui32FrameEnables & GLES1_FS_MULTISAMPLE_ENABLE) != 0))
		{
			bMultiSample = IMG_TRUE;
		}

		if(gc->ui32FrameEnables & GLES1_FS_DITHER_ENABLE)
		{
			bDither = IMG_TRUE;
		}

		if(psRenderSurface->bMultiSample!=bMultiSample)
		{
			bMultiSampleChanged = IMG_TRUE;
		}

		if(psRenderSurface->sPixelBEState.bDither!=bDither)
		{
			bDitherChanged = IMG_TRUE;
		}

		if(bMultiSampleChanged)
		{
			psRenderSurface->bMultiSample = bMultiSample;

			GLESInitRegs(gc, 0);
		}

		if(bMultiSampleChanged || bDitherChanged)
		{
			if(SetupPixelEventProgram(gc, &psRenderSurface->sPixelBEState, IMG_TRUE) != GLES1_NO_ERROR)
			{
				PVR_DPF((PVR_DBG_ERROR,"ScheduleTA: SetupPixelEventProgram() failed"));
			
				return IMG_EGL_GENERIC_ERROR;
			}

			ps3DRegs->sEDMPixelPDSData = psRenderSurface->sPixelBEState.sEventPixelData;
			ps3DRegs->sEDMPixelPDSExec = psRenderSurface->sPixelBEState.sEventPixelExec;
			ps3DRegs->sEDMPixelPDSInfo = psRenderSurface->sPixelBEState.sEventPixelInfo;
		}

		ui32Flags |= SGX_KICKTA_FLAGS_FIRSTTAPROD;
	}

	/* Try to create a ZS buffer for an overflow render */
	if(psRenderSurface->hEGLSurface && psRenderSurface->bDepthStencilBits && bOverflowRender &&
	  (gc->sAppHints.ui32ExternalZBufferMode!=EXTERNAL_ZBUFFER_MODE_ALLOC_NEVER_USED_NEVER))
	{
		if(!psRenderSurface->psZSBufferMemInfo)
		{
			IMG_UINT32 ui32SizeInBytes, ui32RoundedWidth, ui32RoundedHeight;

#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
			ui32RoundedWidth = (pui32PBEState[5] & ~EURASIA_PIXELBES2HI_XMAX_CLRMSK) >> EURASIA_PIXELBES2HI_XMAX_SHIFT;
			ui32RoundedHeight = (pui32PBEState[5] & ~EURASIA_PIXELBES2HI_YMAX_CLRMSK) >> EURASIA_PIXELBES2HI_YMAX_SHIFT;
#else
			ui32RoundedWidth = (pui32PBEState[1] & ~EURASIA_PIXELBE1S1_XMAX_CLRMSK) >> EURASIA_PIXELBE1S1_XMAX_SHIFT;
			ui32RoundedHeight = (pui32PBEState[1] & ~EURASIA_PIXELBE1S1_YMAX_CLRMSK) >> EURASIA_PIXELBE1S1_YMAX_SHIFT;
#endif

			/* Align width and height to tile size */
			ui32RoundedWidth = ALIGNCOUNT(ui32RoundedWidth + 1, EURASIA_TAG_TILE_SIZEX);
			ui32RoundedHeight = ALIGNCOUNT(ui32RoundedHeight + 1, EURASIA_TAG_TILE_SIZEY);

			/* 4 bytes for depth, 1 byte for stencil */
			ui32SizeInBytes	= 5 * ui32RoundedWidth * ui32RoundedHeight;

			if(psRenderSurface->bMultiSample)
			{
				ui32SizeInBytes *= 4;
			}

			PVR_DPF((PVR_DBG_WARNING,"DoKickTA: Creating ZS buffer for overflow"));

			SceKernelAllocMemBlockOpt opt;
			sceClibMemset(&opt, 0, sizeof(SceKernelAllocMemBlockOpt));
			opt.size = sizeof(SceKernelAllocMemBlockOpt);
			opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
			opt.alignment = EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT;

			psRenderSurface->hZSBufferMemBlockUID = sceKernelAllocMemBlock(
				"SGXZSBufferMem",
				SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
				ui32SizeInBytes,
				&opt);

			if (psRenderSurface->hZSBufferMemBlockUID <= 0)
			{
				PVR_DPF((PVR_DBG_ERROR, "DoKickTA: Couldn't allocate memory for Z buffer"));
				psRenderSurface->psZSBufferMemInfo = IMG_NULL;
				goto skip_zs_alloc;
			}

			psRenderSurface->psZSBufferMemInfo = PVRSRVAllocUserModeMem(sizeof(PVRSRV_CLIENT_MEM_INFO));

			sceKernelGetMemBlockBase(psRenderSurface->hZSBufferMemBlockUID, &psRenderSurface->psZSBufferMemInfo->pvLinAddr);
			psRenderSurface->psZSBufferMemInfo->psNext = IMG_NULL;
			psRenderSurface->psZSBufferMemInfo->sDevVAddr.uiAddr = psRenderSurface->psZSBufferMemInfo->pvLinAddr;

			PVRSRVMapMemoryToGpu(gc->ps3DDevData,
				gc->psSysContext->hDevMemContext,
				0,
				ui32SizeInBytes,
				0,
				psRenderSurface->psZSBufferMemInfo->pvLinAddr,
				PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR,
				IMG_NULL);

skip_zs_alloc:

			if (psRenderSurface->psZSBufferMemInfo != IMG_NULL)
			{
				SetupZLSAddressSizeFormat(psRenderSurface, ui32RoundedWidth, ui32RoundedHeight);
			}
		}

		if(psRenderSurface->psZSBufferMemInfo)
		{
			SetupZLSControlReg(gc, psRenderSurface, IMG_TRUE, 0);
		}
	}
	else if(!psRenderSurface->hEGLSurface && bOverflowRender)
	{
		SetupZLSControlReg(gc, psRenderSurface, IMG_TRUE, 0);
	}

	if(ui32KickFlags & GLES1_SCHEDULE_HW_LAST_IN_SCENE)
	{
		ui32Flags |= SGX_KICKTA_FLAGS_TERMINATE;

		GLES1_INC_COUNT(GLES1_TIMER_KICK_3D, 1);
	}

	/* 
	 * If the vertex shader heap has had allocations/frees, we may need to
	 * flush the USE code cache. Note, the USSE code cache is always flushed
	 * on a 3D kick, so we don't need to worry about the fragment shader
	 * heap.
	 */	
	if(gc->psSharedState->psUSEVertexCodeHeap->bDirtySinceLastTAKick)
	{
		ui32Flags |= SGX_KICKTA_FLAGS_FLUSHUSECACHE;
		
		gc->psSharedState->psUSEVertexCodeHeap->bDirtySinceLastTAKick = IMG_FALSE;
	}

	if(ui32KickFlags & GLES1_SCHEDULE_HW_DISCARD_SCENE)
	{
		ui32Flags |= SGX_KICKTA_FLAGS_ABORT; 
	}
	else
	{
		ui32Flags |= SGX_KICKTA_FLAGS_KICKRENDER;
	}

	/* signal whether there is an attached depth buffer */
	if(ps3DRegs->sISPZLoadBase.ui32RegVal)
	{
		ui32Flags |= SGX_KICKTA_FLAGS_DEPTHBUFFER;			
	}

	/* signal whether there is an attached stencil buffer */
	if(ps3DRegs->sISPStencilLoadBase.ui32RegVal)
	{
		ui32Flags |= SGX_KICKTA_FLAGS_STENCILBUFFER;			
	}

	/* Switch fragment buffers to render surface being rendered (not currently bound one) */
	if(psRenderSurface != gc->psRenderSurface)
	{
		gc->apsBuffers[CBUF_TYPE_PDS_FRAG_BUFFER] = &psRenderSurface->sPDSBuffer;
		gc->apsBuffers[CBUF_TYPE_USSE_FRAG_BUFFER] = &psRenderSurface->sUSSEBuffer;
	}

	/* Modify the first three dwords for PDS state for pixel shader */
	psKickTA->sKickTACommon.aui32SpecObject[0] = psRenderSurface->aui32HWBGObjPDSState[0];
	psKickTA->sKickTACommon.aui32SpecObject[1] = psRenderSurface->aui32HWBGObjPDSState[1];
	psKickTA->sKickTACommon.aui32SpecObject[2] = psRenderSurface->aui32HWBGObjPDSState[2];
	
	psKickTA->hRenderContext = gc->psSysContext->hRenderContext;

	if(psRenderSurface->bMultiSample)
	{
		psKickTA->hRTDataSet = psRenderSurface->ahRenderTarget[EGL_RENDER_TARGET_AA_INDEX];

		GLES1_ASSERT(psKickTA->hRTDataSet);
	}
	else
	{
		psKickTA->hRTDataSet =  psRenderSurface->ahRenderTarget[EGL_RENDER_TARGET_NOAA_INDEX];

		GLES1_ASSERT(psKickTA->hRTDataSet);
	}
	
	psKickTA->sKickTACommon.ui32ISPBGObjDepth = ps3DRegs->sISPBackgroundObjectDepth.ui32RegVal;
	psKickTA->sKickTACommon.ui32ISPBGObj = ps3DRegs->sISPBackgroundObject.ui32RegVal;
	psKickTA->sKickTACommon.ui32ISPIPFMisc = ps3DRegs->sISPIPFMisc.ui32RegVal;
#if !defined(SGX545)
	psKickTA->sKickTACommon.ui32ISPPerpendicular = ps3DRegs->sISPPerpendicular.ui32RegVal;
	psKickTA->sKickTACommon.ui32ISPCullValue = ps3DRegs->sISPCullValue.ui32RegVal;
#endif /* !defined(SGX545) */

	psKickTA->sKickTACommon.sISPZLoadBase.uiAddr = ps3DRegs->sISPZLoadBase.ui32RegVal;
	psKickTA->sKickTACommon.sISPZStoreBase.uiAddr = ps3DRegs->sISPZLoadBase.ui32RegVal;
	psKickTA->sKickTACommon.sISPStencilLoadBase.uiAddr = ps3DRegs->sISPStencilLoadBase.ui32RegVal;
	psKickTA->sKickTACommon.sISPStencilStoreBase.uiAddr	= ps3DRegs->sISPStencilLoadBase.ui32RegVal;

	psKickTA->sKickTACommon.bSeperateStencilLoad = IMG_TRUE;
	psKickTA->sKickTACommon.bSeperateStencilStore = IMG_TRUE;

	psKickTA->sKickTACommon.ui32ZLSCtl = ps3DRegs->sISPZLSControl.ui32RegVal;
	psKickTA->sKickTACommon.ui32ISPValidId = 0;
	psKickTA->sKickTACommon.ui32ISPDBias = ps3DRegs->sISPDepthBias.ui32RegVal;
#if defined(SGX_FEATURE_DEPTH_BIAS_OBJECTS)
	psKickTA->sKickTACommon.ui32ISPDBias1 = ps3DRegs->sISPDepthBias1.ui32RegVal;
	psKickTA->sKickTACommon.ui32ISPDBias2 = 0;
#endif
	psKickTA->sKickTACommon.ui32ISPSceneBGObj = ps3DRegs->sISPBackgroundObject.ui32RegVal;

	psKickTA->sKickTACommon.ui32EDMPixelPDSAddr = ps3DRegs->sEDMPixelPDSExec.ui32RegVal;
	psKickTA->sKickTACommon.ui32EDMPixelPDSDataSize = ps3DRegs->sEDMPixelPDSData.ui32RegVal;
	psKickTA->sKickTACommon.ui32EDMPixelPDSInfo = ps3DRegs->sEDMPixelPDSInfo.ui32RegVal;

	psKickTA->sKickTACommon.ui32PixelBE = ps3DRegs->sPixelBE.ui32RegVal;

	if(OutputTerminateState(gc, psRenderSurface, ui32KickFlags) != GLES1_NO_ERROR)
	{
		PVR_DPF((PVR_DBG_ERROR, "OutputTerminateState failed, even though space should have been reserved for it"));

		return IMG_EGL_GENERIC_ERROR;
	}

	j = 0;
	for(i=0; i < CBUF_NUM_TA_BUFFERS; i++)
	{
		if(gc->apsBuffers[i])
		{
			psKickTA->asTAStatusUpdate[j].sCtlStatus.ui32StatusValue = gc->apsBuffers[i]->ui32CommittedPrimOffsetInBytes;

			j++;
		}
	}

	/* Add sync info needed for the TA kick KRM */
	GLES1_ASSERT(psKickTA->sKickTACommon.ui32NumTAStatusVals<SGX_MAX_TA_STATUS_VALS);

	/* Point the next TA status update object to the TA KRM device memory */
	psKickTA->asTAStatusUpdate[psKickTA->sKickTACommon.ui32NumTAStatusVals].hKernelMemInfo = gc->sKRMTAStatusUpdate.psMemInfo->hKernelMemInfo;

	psKickTA->asTAStatusUpdate[psKickTA->sKickTACommon.ui32NumTAStatusVals].sCtlStatus.sStatusDevAddr.uiAddr = gc->sKRMTAStatusUpdate.psMemInfo->sDevVAddr.uiAddr;

	psKickTA->asTAStatusUpdate[psKickTA->sKickTACommon.ui32NumTAStatusVals].sCtlStatus.ui32StatusValue = gc->sKRMTAStatusUpdate.ui32StatusValue;

	/* Increment the status value in the TA KRM Status Object */
	gc->sKRMTAStatusUpdate.ui32StatusValue++;

	/* Increase count of status values */
	psKickTA->sKickTACommon.ui32NumTAStatusVals++;


	/* Setup the two 3D Status Update Objects (PDS and USSE) to point */
	/* to the status update device memory in the buffers  */
    gc->sKickTA.as3DStatusUpdate[0].hKernelMemInfo = gc->apsBuffers[CBUF_TYPE_PDS_FRAG_BUFFER]->psStatusUpdateMemInfo->hKernelMemInfo;
    gc->sKickTA.as3DStatusUpdate[0].sCtlStatus.sStatusDevAddr.uiAddr = gc->apsBuffers[CBUF_TYPE_PDS_FRAG_BUFFER]->psStatusUpdateMemInfo->sDevVAddr.uiAddr;

    gc->sKickTA.as3DStatusUpdate[1].hKernelMemInfo = gc->apsBuffers[CBUF_TYPE_USSE_FRAG_BUFFER]->psStatusUpdateMemInfo->hKernelMemInfo;
    gc->sKickTA.as3DStatusUpdate[1].sCtlStatus.sStatusDevAddr.uiAddr = gc->apsBuffers[CBUF_TYPE_USSE_FRAG_BUFFER]->psStatusUpdateMemInfo->sDevVAddr.uiAddr;

	/* Set the status value that will be copied to the device memory by */
	/* the uKernel on the next 3D complete event */
	gc->sKickTA.as3DStatusUpdate[0].sCtlStatus.ui32StatusValue = gc->apsBuffers[CBUF_TYPE_PDS_FRAG_BUFFER]->ui32CommittedPrimOffsetInBytes;
	gc->sKickTA.as3DStatusUpdate[1].sCtlStatus.ui32StatusValue = gc->apsBuffers[CBUF_TYPE_USSE_FRAG_BUFFER]->ui32CommittedPrimOffsetInBytes;


	/* There may be 2 separate sync objects for EGL surfaces. All surfaces have a sync object associated with
	 * the render surface. Some EGL surfaces (ie window surfaces) may have a per back-buffer sync-object. 
	 * In that case, use the per back-buffer sync. 
	 */
	psKickTA->sPixEventUpdateDevAddr.uiAddr = 0;
	psKickTA->psPixEventUpdateList   = IMG_NULL;
	psKickTA->psHWBgObjPrimPDSUpdateList  = &psKickTA->sKickTACommon.aui32SpecObject[2];
	
	if(psRenderSurface->hEGLSurface)
	{
		if(psRenderSurface == gc->sFrameBuffer.sDefaultFrameBuffer.sReadParams.psRenderSurface)
		{
			psKickTA->ppsRenderSurfSyncInfo = &gc->sFrameBuffer.sDefaultFrameBuffer.sReadParams.psSyncInfo;
		}
		else
		{
			psKickTA->ppsRenderSurfSyncInfo = &gc->sFrameBuffer.sDefaultFrameBuffer.sDrawParams.psSyncInfo;
		}
	}
	else
	{
		psKickTA->ppsRenderSurfSyncInfo = &psRenderSurface->psSyncInfo;
	}

	/* setup the src syncs */
	psKickTA->sKickTACommon.ui32NumSrcSyncs = psRenderSurface->ui32NumSrcSyncs;
	for(i = 0; i < psRenderSurface->ui32NumSrcSyncs; i++)
	{
		if(psRenderSurface->apsSrcSurfSyncInfo[i])
		{
			psKickTA->apsSrcSurfSyncInfo[i] = psRenderSurface->apsSrcSurfSyncInfo[i];
		}
	}

	/* TARegs */
	psKickTA->sKickTACommon.sTABufferCtlStreamBase = gc->apsBuffers[CBUF_TYPE_VDM_CTRL_BUFFER]->uTACtrlKickDevAddr;
	psKickTA->sKickTACommon.ui32MTECtrl = psTARegs->sMTEControl.ui32RegVal;

#if defined(PDUMP)
	PDUMP_SETFRAME(gc);

	/* Dump all textures (this call will check for PDUMP_ISCAPTURING inside) */
	PDumpTextures(gc);

	if(PDUMP_ISCAPTURING(gc))
	{
		IMG_UINT32 ui32Temp;

		/* Dump all buffer objects */
		NamesArrayMapFunction(gc, gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_BUFOBJ], PDumpBufferObject, IMG_NULL);

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)
		/* Dump all vertex array  objects */
		NamesArrayMapFunction(gc, gc->apsNamesArray[GLES1_NAMETYPE_VERARROBJ - GLES1_MAX_SHAREABLE_NAMETYPE], PDumpVertexArrayObject, IMG_NULL);
#endif

		if(gc->sVAOMachine.sDefaultVAO.psMemInfo && !gc->sVAOMachine.sDefaultVAO.bDumped)
		{
			PDUMP_STRING((gc, "Dumping Default VAO PDS Code at device virtual address 0x%x\r\n",gc->sVAOMachine.sDefaultVAO.psMemInfo->sDevVAddr.uiAddr));
			PDUMP_MEM(gc, gc->sVAOMachine.sDefaultVAO.psMemInfo, 0, gc->sVAOMachine.sDefaultVAO.psMemInfo->uAllocSize);
			gc->sVAOMachine.sDefaultVAO.bDumped = IMG_TRUE;
		}

		if(!gc->bPrevFrameDumped)
		{
			PDUMP_STRING((gc, "Dumping Dummy white texture at device virtual address 0x%x\r\n",
						gc->psSharedState->psTextureManager->psWhiteDummyTexture->sDevVAddr.uiAddr));
			PDUMP_MEM(gc, gc->psSharedState->psTextureManager->psWhiteDummyTexture, 0, 
						gc->psSharedState->psTextureManager->psWhiteDummyTexture->uAllocSize);

			PDUMP_STRING((gc, "Dumping Dummy Frag USSE Code at device virtual address 0x%x\r\n",gc->sProgram.psDummyFragUSECode->sDevVAddr.uiAddr));
			PDUMP_MEM(gc, gc->sProgram.psDummyFragUSECode, 0, gc->sProgram.psDummyFragUSECode->uAllocSize);
			
			PDUMP_STRING((gc, "Dumping Dummy Vert USSE Code at device virtual address 0x%x\r\n",gc->sProgram.psDummyVertUSECode->sDevVAddr.uiAddr));
			PDUMP_MEM(gc, gc->sProgram.psDummyVertUSECode, 0, gc->sProgram.psDummyVertUSECode->uAllocSize);

#if defined(FIX_HW_BRN_31988)
			PDUMP_STRING((gc, "Dumping BRN 31988 Frag USSE Code at device virtual address 0x%x\r\n",gc->sProgram.psBRN31988FragUSECode->sDevVAddr.uiAddr));
			PDUMP_MEM(gc, gc->sProgram.psBRN31988FragUSECode, 0, gc->sProgram.psBRN31988FragUSECode->uAllocSize);
#endif

			gc->bPrevFrameDumped = IMG_TRUE;
		}
		
		PDumpTerminate(gc, &psRenderSurface->sTerm);

		PDumpShaders(gc);

		sKickTAPDump.psBufferArray = &sDumpBuffer[0];
		sKickTAPDump.ui32BufferArraySize = 0;

		for(i=0; i < CBUF_NUM_BUFFERS; i++)
		{
			CircularBuffer *psBuffer = gc->apsBuffers[i];	
			
			if(psBuffer)
			{
				sDumpBuffer[sKickTAPDump.ui32BufferArraySize].hKernelMemInfo = psBuffer->psMemInfo->hKernelMemInfo;
				sDumpBuffer[sKickTAPDump.ui32BufferArraySize].pvLinAddr = psBuffer->psMemInfo->pvLinAddr;
				sDumpBuffer[sKickTAPDump.ui32BufferArraySize].pszName = (IMG_CHAR *)pszBufferNames[i];	/* PRQA S 0311 */ /* Safe const cast here. */
				sDumpBuffer[sKickTAPDump.ui32BufferArraySize].ui32BufferSize = psBuffer->ui32BufferLimitInBytes;
				sDumpBuffer[sKickTAPDump.ui32BufferArraySize].ui32BackEndLength = psBuffer->ui32BufferLimitInBytes - psBuffer->ui32CommittedHWOffsetInBytes;
				sDumpBuffer[sKickTAPDump.ui32BufferArraySize].ui32End = psBuffer->ui32CurrentWriteOffsetInBytes;
				sDumpBuffer[sKickTAPDump.ui32BufferArraySize].ui32SpaceUsed = CBUF_GetBufferSpaceUsedInBytes(psBuffer);
				sDumpBuffer[sKickTAPDump.ui32BufferArraySize].ui32Start = psBuffer->ui32CommittedHWOffsetInBytes;
				sDumpBuffer[sKickTAPDump.ui32BufferArraySize].hCtrlKernelMemInfo = gc->apsBuffers[i]->psStatusUpdateMemInfo->hKernelMemInfo;
				sDumpBuffer[sKickTAPDump.ui32BufferArraySize].sCtrlDevVAddr.uiAddr = gc->apsBuffers[i]->psStatusUpdateMemInfo->sDevVAddr.uiAddr;

				sKickTAPDump.ui32BufferArraySize++;
			}
		}

		/*
			Initialize the read offsets into the various circular buffers. Done only once.
		*/
		for(i=0; i<CBUF_NUM_BUFFERS; i++)
		{
			if(gc->apsBuffers[i] && !gc->apsBuffers[i]->bSyncDumped)
			{
				PDUMP_STRING((gc, "Dumping sync object for %s buffer\r\n",pszBufferNames[i]));

				PDumpMem( gc,
						  gc->apsBuffers[i]->psStatusUpdateMemInfo,
						  0,
						  sizeof(IMG_UINT32));

				gc->apsBuffers[i]->bSyncDumped = IMG_TRUE;
			}
		}

		if(gc->psSharedState->bMustDumpSequentialStaticIndices)
		{
			if(gc->psSharedState->psSequentialStaticIndicesMemInfo)
			{
				PDUMP_STRING((gc, "Dumping DrawArrays sequential static index buffer\r\n"));

				PDUMP_MEM(gc, gc->psSharedState->psSequentialStaticIndicesMemInfo, 0, gc->psSharedState->psSequentialStaticIndicesMemInfo->uAllocSize);
			}

			gc->psSharedState->bMustDumpSequentialStaticIndices = IMG_FALSE;
		}

		if(gc->psSharedState->bMustDumpLineStripStaticIndices)
		{
			if(gc->psSharedState->psLineStripStaticIndicesMemInfo)
			{
				PDUMP_STRING((gc, "Dumping DrawArrays line strip static index buffer\r\n"));

				PDUMP_MEM(gc, gc->psSharedState->psLineStripStaticIndicesMemInfo, 0, gc->psSharedState->psLineStripStaticIndicesMemInfo->uAllocSize);
			}

			gc->psSharedState->bMustDumpLineStripStaticIndices = IMG_FALSE;
		}

#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
		sDumpBitmap.sDevBaseAddr.uiAddr = pui32PBEState[1] & ~EURASIA_PIXELBES0HI_FBADDR_CLRMSK;

		ui32Temp = (pui32PBEState[0] & ~EURASIA_PIXELBES0LO_MEMLAYOUT_CLRMSK) >> EURASIA_PIXELBES0LO_MEMLAYOUT_SHIFT;
		
		if(ui32Temp == EURASIA_PIXELBES0LO_MEMLAYOUT_TWIDDLED)
		{
			sDumpBitmap.ui32Flags	= SGX_KICKTA_DUMPBITMAP_FLAGS_TWIDDLED;
		}
		else
		{
			sDumpBitmap.ui32Flags = 0;
		}

		ui32Temp = (pui32PBEState[0] & ~EURASIA_PIXELBES0LO_PACKMODE_CLRMSK);
	
		switch(ui32Temp)
		{
			default:
			case EURASIA_PIXELBES0LO_PACKMODE_R5G6B5:
			{
				sDumpBitmap.ui32PDUMPFormat = PVRSRV_PDUMP_PIXEL_FORMAT_RGB565;
				sDumpBitmap.ui32BytesPP = 2;
				break;
			}
			case EURASIA_PIXELBES0LO_PACKMODE_A1R5G5B5:
			{
				sDumpBitmap.ui32PDUMPFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB1555;
				sDumpBitmap.ui32BytesPP = 2;
				break;
			}
			case EURASIA_PIXELBES0LO_PACKMODE_A4R4G4B4:
			{
				sDumpBitmap.ui32PDUMPFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB4444;
				sDumpBitmap.ui32BytesPP = 2;
				break;
			}
			case EURASIA_PIXELBES0LO_PACKMODE_U8888:
			{
				ui32Temp = (pui32PBEState[0] & ~EURASIA_PIXELBES0LO_SWIZ_CLRMSK) >> EURASIA_PIXELBES0LO_SWIZ_SHIFT;

				if(ui32Temp == EURASIA_PIXELBES0LO_SWIZ_ABGR)
				{
					sDumpBitmap.ui32PDUMPFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ABGR8888;
				}
				else
				{
					sDumpBitmap.ui32PDUMPFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB8888;
				}
				sDumpBitmap.ui32BytesPP = 4;
				break;
			}
		}

		ui32Temp = (pui32PBEState[5] & ~EURASIA_PIXELBES2HI_XMAX_CLRMSK) >> EURASIA_PIXELBES2HI_XMAX_SHIFT;

		sDumpBitmap.ui32Width = ui32Temp + 1;
	
		ui32Temp = (pui32PBEState[5] & ~EURASIA_PIXELBES2HI_YMAX_CLRMSK) >> EURASIA_PIXELBES2HI_YMAX_SHIFT;

		sDumpBitmap.ui32Height = ui32Temp + 1;
		
		ui32Temp = (pui32PBEState[2] & ~EURASIA_PIXELBES1LO_LINESTRIDE_CLRMSK) >> EURASIA_PIXELBES1LO_LINESTRIDE_SHIFT;
	
		sDumpBitmap.ui32Stride = ((ui32Temp + 1 )<< EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT) * sDumpBitmap.ui32BytesPP;

#else /* SGX_FEATURE_UNIFIED_STORE_64BITS */
		sDumpBitmap.sDevBaseAddr.uiAddr = pui32PBEState[3] & ~EURASIA_PIXELBE2S1_FBADDR_CLRMSK;

#if defined(SGX545)
		ui32Temp = (ui32PBESideband & ~EURASIA_PIXELBE2SB_MEMLAYOUT_CLRMSK) >> EURASIA_PIXELBE2SB_MEMLAYOUT_SHIFT;

		if(ui32Temp == EURASIA_PIXELBE2SB_MEMLAYOUT_TWIDDLED)
		{
			sDumpBitmap.ui32Flags	= SGX_KICKTA_DUMPBITMAP_FLAGS_TWIDDLED;
		}
		else
		{
			sDumpBitmap.ui32Flags = 0;
		}
#else /* defined(SGX545) */
		ui32Temp = (pui32PBEState[2] & ~EURASIA_PIXELBE2S0_MEMLAYOUT_CLRMSK) >> EURASIA_PIXELBE2S0_MEMLAYOUT_SHIFT;
		
		if(ui32Temp == EURASIA_PIXELBE2S0_MEMLAYOUT_TWIDDLED)
		{
			sDumpBitmap.ui32Flags	= SGX_KICKTA_DUMPBITMAP_FLAGS_TWIDDLED;
		}
		else
		{
			sDumpBitmap.ui32Flags = 0;
		}
#endif /* defined(SGX545) */


#if defined(SGX545)
		ui32Temp = (ui32PBESideband & ~EURASIA_PIXELBE2SB_PACKMODE_CLRMSK);
#else /* defined(SGX545) */
		ui32Temp = (pui32PBEState[2] & ~EURASIA_PIXELBE2S0_PACKMODE_CLRMSK);
#endif /* defined(SGX545) */
	
		switch(ui32Temp)
		{
			default:
#if defined(SGX545)
			case EURASIA_PIXELBE2SB_PACKMODE_R5G6B5:
#else /* defined(SGX545) */
			case EURASIA_PIXELBE2S0_PACKMODE_R5G6B5:
#endif /* defined(SGX545) */
			{
				sDumpBitmap.ui32PDUMPFormat = PVRSRV_PDUMP_PIXEL_FORMAT_RGB565;
				sDumpBitmap.ui32BytesPP = 2;
				break;
			}
#if defined(SGX545)
			case EURASIA_PIXELBE2SB_PACKMODE_A1R5G5B5:
#else /* defined(SGX545) */
			case EURASIA_PIXELBE2S0_PACKMODE_A1R5G5B5:
#endif /* defined(SGX545) */
			{
				sDumpBitmap.ui32PDUMPFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB1555;
				sDumpBitmap.ui32BytesPP = 2;
				break;
			}
#if defined(SGX545)
			case EURASIA_PIXELBE2SB_PACKMODE_A4R4G4B4:
#else /* defined(SGX545) */
			case EURASIA_PIXELBE2S0_PACKMODE_A4R4G4B4:
#endif /* defined(SGX545) */
			{
				sDumpBitmap.ui32PDUMPFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB4444;
				sDumpBitmap.ui32BytesPP = 2;
				break;
			}
#if defined(SGX545)
			case EURASIA_PIXELBE2SB_PACKMODE_A8R8G8B8:
#else /* defined(SGX545) */
			case EURASIA_PIXELBE2S0_PACKMODE_A8R8G8B8:
#endif /* defined(SGX545) */
			{
				sDumpBitmap.ui32PDUMPFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ARGB8888;
				sDumpBitmap.ui32BytesPP = 4;
				break;
			}
#if defined(SGX545)
			case EURASIA_PIXELBE2SB_PACKMODE_A8B8G8R8:
#else /* defined(SGX545) */
			case EURASIA_PIXELBE2S0_PACKMODE_R8G8B8A8:
#endif /* defined(SGX545) */
			{
				sDumpBitmap.ui32PDUMPFormat = PVRSRV_PDUMP_PIXEL_FORMAT_ABGR8888;
				sDumpBitmap.ui32BytesPP = 4;
				break;
			}
		}

		ui32Temp = (pui32PBEState[1] & ~EURASIA_PIXELBE1S1_XMAX_CLRMSK) >> EURASIA_PIXELBE1S1_XMAX_SHIFT;

		sDumpBitmap.ui32Width = ui32Temp + 1;
	
		ui32Temp = (pui32PBEState[1] & ~EURASIA_PIXELBE1S1_YMAX_CLRMSK) >> EURASIA_PIXELBE1S1_YMAX_SHIFT;

		sDumpBitmap.ui32Height = ui32Temp + 1;

#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
		ui32Temp = (pui32PBEState[4] & ~EURASIA_PIXELBE2S2_LINESTRIDE_CLRMSK) >> EURASIA_PIXELBE2S2_LINESTRIDE_SHIFT;
#else
		ui32Temp = (pui32PBEState[2] & ~EURASIA_PIXELBE2S0_LINESTRIDE_CLRMSK) >> EURASIA_PIXELBE2S0_LINESTRIDE_SHIFT;
#endif
	
		sDumpBitmap.ui32Stride = ((ui32Temp + 1 )<< EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT) * sDumpBitmap.ui32BytesPP;
#endif /* SGX_FEATURE_UNIFIED_STORE_64BITS */

		sprintf(sDumpBitmap.pszName, "outfb%%lu");

		sKickTAPDump.psPDumpBitmapArray = &sDumpBitmap;
		sKickTAPDump.ui32PDumpBitmapSize = 1;

		pvPDumpData = &sKickTAPDump;
	}
#endif

	if(ui32KickFlags & GLES1_SCHEDULE_HW_BBOX_RENDER)
	{
		ui32Flags |= SGX_KICKTA_FLAGS_MIDSCENENOFREE |
					 SGX_KICKTA_FLAGS_BBOX_TERMINATE;
	}
	else
	{
		SGX_STATUS_UPDATE *ps3DStatusUpdate;

		GLES1_ASSERT(psKickTA->sKickTACommon.ui32Num3DStatusVals<SGX_MAX_3D_STATUS_VALS);

		gc->sKickTA.sKickTACommon.ui32Num3DStatusVals++;

		ps3DStatusUpdate = &gc->sKickTA.as3DStatusUpdate[gc->sKickTA.sKickTACommon.ui32Num3DStatusVals - 1];

	    ps3DStatusUpdate->hKernelMemInfo				   = psRenderSurface->sRenderStatusUpdate.psMemInfo->hKernelMemInfo;
	    ps3DStatusUpdate->sCtlStatus.sStatusDevAddr.uiAddr = psRenderSurface->sRenderStatusUpdate.psMemInfo->sDevVAddr.uiAddr;
		ps3DStatusUpdate->sCtlStatus.ui32StatusValue	   = psRenderSurface->sRenderStatusUpdate.ui32StatusValue;

		if(ui32KickFlags & (GLES1_SCHEDULE_HW_LAST_IN_SCENE|GLES1_SCHEDULE_HW_DISCARD_SCENE))
		{
			psRenderSurface->sRenderStatusUpdate.ui32StatusValue++;
		}

		if(ui32KickFlags & GLES1_SCHEDULE_HW_POST_BBOX_RENDER)
		{
			ui32Flags |= SGX_KICKTA_FLAGS_RENDEREDMIDSCENE |
						 SGX_KICKTA_FLAGS_RENDEREDMIDSCENENOFREE;
		}

		ui32Flags |= SGX_KICKTA_FLAGS_RESETTPC;
	}

	if (ui32Flags & (SGX_KICKTA_FLAGS_TERMINATE|SGX_KICKTA_FLAGS_ABORT))
	{
		ui32Flags |= SGX_KICKTA_FLAGS_TQ3D_SYNC;
		psKickTA->hTQContext = gc->psSysContext->hTransferContext;
	}

	psKickTA->sKickTACommon.ui32KickFlags = ui32Flags |
#if defined(SGX_FEATURE_ALPHATEST_COEFREORDER)
											SGX_KICKTA_FLAGS_ABC_REORDER |
#endif /* defined(SGX_FEATURE_ALPHATEST_COEFREORDER) */
											SGX_KICKTA_FLAGS_COLOURALPHAVALID |
											SGX_KICKTA_FLAGS_FLUSHDATACACHE	| 
											SGX_KICKTA_FLAGS_FLUSHPDSCACHE;



	psKickTA->sKickTACommon.ui32Frame = gc->ui32FrameNum;

	psKickTA->sKickTACommon.ui32SceneWidth = psRenderSurface->ui32Width;
	psKickTA->sKickTACommon.ui32SceneHeight = psRenderSurface->ui32Height;
	psKickTA->sKickTACommon.ui32ValidRegionXMax = psRenderSurface->ui32Width - 1;
	psKickTA->sKickTACommon.ui32ValidRegionYMax = psRenderSurface->ui32Height - 1;
	psKickTA->sKickTACommon.ui16PrimitiveSplitThreshold = 1000;

	GLES1_TIME_START(GLES1_TIMER_SGXKICKTA_TIME);

	eError = SGXKickTA(gc->ps3DDevData, psKickTA, &sKickTAOutput, pvPDumpData, IMG_NULL);

	GLES1_TIME_STOP(GLES1_TIMER_SGXKICKTA_TIME);

	if(eError!=PVRSRV_OK)
	{
		if((ui32KickFlags & GLES1_SCHEDULE_HW_BBOX_RENDER) == 0)
		{
			/* Remove the render status update from the 3D status list */
			gc->sKickTA.sKickTACommon.ui32Num3DStatusVals--;

			if(ui32KickFlags & (GLES1_SCHEDULE_HW_LAST_IN_SCENE|GLES1_SCHEDULE_HW_DISCARD_SCENE))
			{
				/* Decrement the render status update value */
				psRenderSurface->sRenderStatusUpdate.ui32StatusValue--;
			}

			/* Decrement the KRMTA status update value */
			gc->sKRMTAStatusUpdate.ui32StatusValue--;
		}

		/* Remove the KRMTA status update from the TA status list */
		psKickTA->sKickTACommon.ui32NumTAStatusVals--;

		/* Switch fragment buffer back to currently bound render surface */
		if(psRenderSurface != gc->psRenderSurface)
		{
			/* If currently bound render surface has already been destroyed - don't set apsBuffers to junk */
			if(gc->psRenderSurface)
			{
				gc->apsBuffers[CBUF_TYPE_PDS_FRAG_BUFFER] = &gc->psRenderSurface->sPDSBuffer;
				gc->apsBuffers[CBUF_TYPE_USSE_FRAG_BUFFER] = &gc->psRenderSurface->sUSSEBuffer;
			}
			else
			{
				gc->apsBuffers[CBUF_TYPE_PDS_FRAG_BUFFER] = IMG_NULL;
				gc->apsBuffers[CBUF_TYPE_USSE_FRAG_BUFFER] = IMG_NULL;
			}
		}

		PVR_DPF((PVR_DBG_ERROR, "DoKickTA: SGXKickTA() failed with error %d", eError));

		return IMG_EGL_GENERIC_ERROR;
	}

	if((ui32KickFlags & GLES1_SCHEDULE_HW_BBOX_RENDER) == 0)
	{
		gc->sKickTA.sKickTACommon.ui32Num3DStatusVals--;
	}

	if(sKickTAOutput.bSPMOutOfMemEvent && 
	  (gc->sAppHints.ui32ExternalZBufferMode!=EXTERNAL_ZBUFFER_MODE_ALLOC_NEVER_USED_NEVER) &&
	  (gc->sFrameBuffer.psActiveFrameBuffer == &gc->sFrameBuffer.sDefaultFrameBuffer))
	{
		gc->bSPMOutOfMemEvent = IMG_TRUE;
	}

	psRenderSurface->bPrimitivesSinceLastTA = IMG_FALSE;

	CBUF_UpdateBufferCommittedHWOffsets(gc->apsBuffers, (ui32KickFlags & GLES1_SCHEDULE_HW_LAST_IN_SCENE) ? IMG_TRUE : IMG_FALSE);

	CBUF_UpdateTACtrlKickBase(gc->apsBuffers);

	/* Remove buffer objects from list of status vals */
	for(i=j; i < psKickTA->sKickTACommon.ui32NumTAStatusVals; i++)
	{
		psKickTA->asTAStatusUpdate[i].hKernelMemInfo = 0;
		psKickTA->asTAStatusUpdate[i].sCtlStatus.sStatusDevAddr.uiAddr = 0;
	}

	psKickTA->sKickTACommon.ui32NumTAStatusVals = j;

	if((ui32KickFlags & (GLES1_SCHEDULE_HW_LAST_IN_SCENE|GLES1_SCHEDULE_HW_BBOX_RENDER)) == GLES1_SCHEDULE_HW_LAST_IN_SCENE)
	{
		psRenderSurface->bFirstKick = IMG_TRUE;
		psRenderSurface->bInFrame = IMG_FALSE;

		/* clear the source texture dependency count */
		psRenderSurface->ui32NumSrcSyncs = 0;


		/* Remove from flush list */
		if((ui32KickFlags & GLES1_SCHEDULE_HW_IGNORE_FLUSHLIST) == 0)
		{
			if(psRenderSurface->hEGLSurface == IMG_NULL)
			{
				GLES1SurfaceFlushList **ppsFlushList;
				IMG_BOOL bFound;
			
				PVRSRVLockMutex(gc->psSharedState->hFlushListLock);
	 
				bFound = IMG_FALSE;
				ppsFlushList = &gc->psSharedState->psFlushList;

				while(*ppsFlushList)
				{
					if((*ppsFlushList)->psRenderSurface == psRenderSurface)
					{
						GLES1SurfaceFlushList *psItem;
						
						psItem = *ppsFlushList;

						*ppsFlushList = (*ppsFlushList)->psNext;

						GLES1Free(gc, psItem);

						bFound = IMG_TRUE;
					}
					else
					{
						ppsFlushList = &((*ppsFlushList)->psNext);
					}
				}

				PVRSRVUnlockMutex(gc->psSharedState->hFlushListLock);

				GLES1_ASSERT(bFound);
			}
		}

		if(bOverflowRender)
		{
			psRenderSurface->bNeedZSLoadAfterOverflowRender = IMG_TRUE;
		}
		else
		{
			psRenderSurface->bNeedZSLoadAfterOverflowRender = IMG_FALSE;
		}

		/* Don't increment frame count if we are rendering for a buffer overflow, or this is an FBO */
		if(!bOverflowRender && psRenderSurface->hEGLSurface)
		{
#if defined(EMULATOR)
			printf("\nFrame num %lu", gc->ui32FrameNum);
#endif
			gc->ui32FrameNum++;
		}

		/* Make sure that we re-emit all the necessary fragment state */
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_FP_STATE;
	}
	else
	{
		psRenderSurface->bFirstKick = IMG_FALSE;
	}

	/* Switch fragment buffer back to currently bound render surface */
	if(psRenderSurface != gc->psRenderSurface)
	{
		/* If currently bound render surface has already been destroyed - don't set apsBuffers to junk */
		if(gc->psRenderSurface)
		{
			gc->apsBuffers[CBUF_TYPE_PDS_FRAG_BUFFER] = &gc->psRenderSurface->sPDSBuffer;
			gc->apsBuffers[CBUF_TYPE_USSE_FRAG_BUFFER] = &gc->psRenderSurface->sUSSEBuffer;
		}
		else
		{
			gc->apsBuffers[CBUF_TYPE_PDS_FRAG_BUFFER] = IMG_NULL;
			gc->apsBuffers[CBUF_TYPE_USSE_FRAG_BUFFER] = IMG_NULL;
		}
	}

#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)
	gc->sSmallKickTA.ui32NumIndicesThisFrame	+= gc->sSmallKickTA.ui32NumIndicesThisTA;
	gc->sSmallKickTA.ui32NumPrimitivesThisFrame += gc->sSmallKickTA.ui32NumPrimitivesThisTA;

	gc->sSmallKickTA.ui32NumIndicesThisTA	 = 0;
	gc->sSmallKickTA.ui32NumPrimitivesThisTA = 0;

	/* Render: calculate new TA kick thresholds */
	if(((ui32KickFlags & GLES1_SCHEDULE_HW_LAST_IN_SCENE) != 0) &&
	   ((gc->sAppHints.ui32KickTAMode == 3) || (gc->sAppHints.ui32KickTAMode == 4)))
	{
		IMG_UINT32 ui32NewKickThreshold = 0;

		if(gc->sAppHints.ui32KickTAMode == 3)
		{
			/* Mode 3: primitives. */
			ui32NewKickThreshold = (gc->sSmallKickTA.ui32NumPrimitivesThisFrame / gc->sAppHints.ui32KickTAThreshold) + 1;
		}
		else if(gc->sAppHints.ui32KickTAMode == 4)
		{
			/* Mode 4: indices */
			ui32NewKickThreshold = (gc->sSmallKickTA.ui32NumIndicesThisFrame / gc->sAppHints.ui32KickTAThreshold) + 1;
		}

		if(gc->sSmallKickTA.ui32KickThreshold != 0xFFFFFFFF)
		{
			/* Take average of last threshold and newly calculated value */
			gc->sSmallKickTA.ui32KickThreshold = (gc->sSmallKickTA.ui32KickThreshold + ui32NewKickThreshold) >> 1;
		}
		else
		{
			/* First threshold is the calculated value */
			gc->sSmallKickTA.ui32KickThreshold = ui32NewKickThreshold;
		}

		gc->sSmallKickTA.ui32NumIndicesThisFrame	= 0;
		gc->sSmallKickTA.ui32NumPrimitivesThisFrame = 0;
	}
#endif /* defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH) */
	

	/* Make sure that we re-emit all the necessary MTE state.

	   DIRTYFLAG_VAO_ATTRIB_POINTER needs to be set for either zero or non-zero VAO, 
	   since PDS vertex shader program should be regenerated, non-zero VAOs contain
	   current states, which have to be recopied to circular buffer after kicking TA. */
	gc->ui32DirtyMask |= (GLES1_DIRTYFLAG_VAO_ATTRIB_POINTER |
						  GLES1_DIRTYFLAG_VERTPROG_CONSTANTS);

	/* Only re-emit the fragment program constants after a render */
	if(!psRenderSurface->bInFrame)
	{
		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;
	}

	gc->ui32EmitMask |=	(GLES1_EMITSTATE_MTE_STATE_OUTPUTSELECTS |
						 GLES1_EMITSTATE_MTE_STATE_VIEWPORT | 
						 GLES1_EMITSTATE_MTE_STATE_ISP |
						 GLES1_EMITSTATE_MTE_STATE_CONTROL |
						 GLES1_EMITSTATE_MTE_STATE_REGION_CLIP |
						 GLES1_EMITSTATE_PDS_FRAGMENT_STATE |
						 GLES1_EMITSTATE_PDS_FRAGMENT_SECONDARY_STATE |
						 GLES1_EMITSTATE_MTE_STATE_TAMISC);

	return IMG_EGL_NO_ERROR;
}

/***********************************************************************************
 Function Name      : ScheduleTA
 Inputs             : gc, psRenderSurface, ui32Flags
 Outputs            : -
 Returns            : Success
 Description        : Kicks a TA/render and waits for the hardware if required.
************************************************************************************/
IMG_INTERNAL IMG_EGLERROR ScheduleTA(GLES1Context *gc, EGLRenderSurface *psRenderSurface, IMG_UINT32 ui32KickFlags)
{
	IMG_EGLERROR eError;

	if(!ValidateMemory(gc))
	{
		return IMG_EGL_MEMORY_INVALID_ERROR;
	}

	/*
	 * Determine whether we have to call the services or not.
	 */
	if(psRenderSurface->bInFrame)
	{
		if(psRenderSurface->bPrimitivesSinceLastTA           ||
		   ((ui32KickFlags & (GLES1_SCHEDULE_HW_LAST_IN_SCENE|GLES1_SCHEDULE_HW_DISCARD_SCENE)) != 0))
		{
			/* Yes, do kick the TA/3D */
			eError = DoKickTA(gc, psRenderSurface, ui32KickFlags);

			if(eError != IMG_EGL_NO_ERROR)
			{
				return eError;
			}
		}
	}

	/*
	 * Now wait for the hardware if we've been asked to.
	 */
	if(ui32KickFlags & GLES1_SCHEDULE_HW_WAIT_FOR_TA)
	{
		/* Wait for the TA */
		WaitForTA(gc);
	}

	if(ui32KickFlags & GLES1_SCHEDULE_HW_WAIT_FOR_3D)
	{
		/* There may be 2 separate sync objects for EGL surfaces. All surfaces have a sync object associated with
		* the render surface. Some EGL surfaces (ie window surfaces) may have a per back-buffer sync-object. 
		* In that case, use the per back-buffer sync. 
		*/
		if(psRenderSurface->hEGLSurface)
		{
			if(psRenderSurface == gc->sFrameBuffer.sDefaultFrameBuffer.sReadParams.psRenderSurface)
			{
				/* Wait for the 3D core */
				WaitForRender(gc, gc->sFrameBuffer.sDefaultFrameBuffer.sReadParams.psSyncInfo);
			}
			else
			{
				/* Wait for the 3D core */
				WaitForRender(gc, gc->sFrameBuffer.sDefaultFrameBuffer.sDrawParams.psSyncInfo);
			}
		}
		else
		{
			/* Wait for the 3D core */
			WaitForRender(gc, psRenderSurface->psSyncInfo);
		}
	}

	return IMG_EGL_NO_ERROR;
}


/******************************************************************************
 End of file (sgxif.c)
******************************************************************************/

