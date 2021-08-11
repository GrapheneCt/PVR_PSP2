/******************************************************************************
 * Name         : eglglue.c
 *
 * Copyright    : 2005-2010 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park  Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: eglglue.c $
 *****************************************************************************/

#include "context.h"
#include "pvrversion.h"
#include "psp2/swtexop.h"
#include "psp2/libheap_custom.h"
#include <string.h>
#include <ult.h>
#include <libsysmodule.h>

#include "pds_mte_state_copy_sizeof.h"
#include "pds_aux_vtx_sizeof.h"

extern const IMG_CHAR * const pszRenderer;

/***********************************************************************************
 Function Name      : _GLES1CreateContext
 Inputs             : None
 Outputs            : None
 Returns            : GLES1Context*
 Description        : Create the GLES1Context context structure
************************************************************************************/
static GLES1Context* _GLES1CreateContext(IMG_VOID)
{
	return (GLES1Context *)GLES1Calloc(0, sizeof(GLES1Context));
}

/***********************************************************************************
 Function Name      : _GLES1DestroyContext
 Inputs             : gc
 Outputs            : None
 Returns            : None
 Description        : Destroy the GLES1Context context structure
************************************************************************************/
static IMG_VOID _GLES1DestroyContext(GLES1Context *gc)
{
	GLES1Free(IMG_NULL, gc);
}


/***********************************************************************************
 Function Name      : FreeContextSharedState
 Inputs             : gc
 Outputs            : -
 Returns            : Success.
 Description        : Frees the data structures that are shared between multiple contexts.
************************************************************************************/
static IMG_VOID FreeContextSharedState(GLES1Context *gc)
{
	IMG_UINT32 i;
	IMG_BOOL bDoFree = IMG_FALSE;
	GLES1ContextSharedState *psSharedState = gc->psSharedState;
	/*
	** Frame buffer objects must be deleted first so that their attachments (renderbuffer/texture)
	** can be removed safely
	*/
	static const GLES1NameType aeNameType[GLES1_MAX_SHAREABLE_NAMETYPE] =
	{
		  GLES1_NAMETYPE_FRAMEBUFFER,
		  GLES1_NAMETYPE_TEXOBJ,
		  GLES1_NAMETYPE_BUFOBJ,
		  GLES1_NAMETYPE_RENDERBUFFER
	};

	/* Silently ignore NULL */
	if(!psSharedState)
	{
		return;
	}

	/* *** CRITICAL SECTION *** */
	PVRSRVLockMutex(psSharedState->hPrimaryLock);

	GLES1_ASSERT(psSharedState->ui32RefCount > 0);

	if(psSharedState->ui32RefCount == 1)
	{
		bDoFree = IMG_TRUE;
	}

	psSharedState->ui32RefCount--;

	PVRSRVUnlockMutex(psSharedState->hPrimaryLock);
	/* *** END OF CRITICAL SECTION *** */


	/* This was the last reference to this shared state. Time to free it for good */
	if(bDoFree)
	{
		PVRSRV_ERROR eError;

		/* Make the managers wait for the resources in their lists */
		KRM_WaitForAllResources(&psSharedState->psTextureManager->sKRM, GLES1_DEFAULT_WAIT_RETRIES);
		KRM_WaitForAllResources(&psSharedState->sUSEShaderVariantKRM,   GLES1_DEFAULT_WAIT_RETRIES);
		KRM_WaitForAllResources(&psSharedState->sPDSVariantKRM,   GLES1_DEFAULT_WAIT_RETRIES);


		/* Destroy all resources */
		for(i = 0; i < GLES1_MAX_SHAREABLE_NAMETYPE; ++i)
		{
			int j = aeNameType[i];
			if (j < GLES1_MAX_SHAREABLE_NAMETYPE)
			{
				if(psSharedState->apsNamesArray[j])
				{
					DestroyNamesArray(gc, psSharedState->apsNamesArray[j]);
				}
			}
		}


		/* Free the texture manager _after_ destroying the textures so the active list is empty */
		/* XXX: Does the above comment still apply? --DavidG Oct2006 */
		if(psSharedState->psTextureManager)
		{
			ReleaseTextureManager(gc, psSharedState->psTextureManager);
		}

		/* Destroy the PDS code variant manager _before_ destroying the heaps since it uses them */
		KRM_Destroy(gc, &psSharedState->sPDSVariantKRM);

		/* Destroy the USSE code variant manager _before_ destroying the heaps since it uses them */
		KRM_Destroy(gc, &psSharedState->sUSEShaderVariantKRM);

		/* Destroy the TA kick buffer object manager */
		KRM_Destroy(gc, &psSharedState->sBufferObjectKRM);


		/* Free the code heaps _after_ destroying the shaders and programs so the heaps are empty */
		if(psSharedState->psUSEVertexCodeHeap)
		{
			UCH_CodeHeapDestroy(psSharedState->psUSEVertexCodeHeap);
		}

		if(psSharedState->psUSEFragmentCodeHeap)
		{
			UCH_CodeHeapDestroy(psSharedState->psUSEFragmentCodeHeap);
		}

		if(psSharedState->psPDSFragmentCodeHeap)
		{
			UCH_CodeHeapDestroy(psSharedState->psPDSFragmentCodeHeap);
		}

		if(psSharedState->psPDSVertexCodeHeap)
		{
			UCH_CodeHeapDestroy(psSharedState->psPDSVertexCodeHeap);
		}

		if(psSharedState->psSequentialStaticIndicesMemInfo)
		{
			GLES1FREEDEVICEMEM(gc->ps3DDevData, psSharedState->psSequentialStaticIndicesMemInfo);
		}

		if(psSharedState->psLineStripStaticIndicesMemInfo)
		{
			GLES1FREEDEVICEMEM(gc->ps3DDevData, psSharedState->psLineStripStaticIndicesMemInfo);
		}

		if (psSharedState->hFlushListLock)
		{
			eError = PVRSRVDestroyMutex(psSharedState->hFlushListLock);

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "FreeContextSharedState: PVRSRVDestroyMutex failed on hFlushListLock (%d)", eError));
			}
		}

		if (psSharedState->hTertiaryLock)
		{
			eError = PVRSRVDestroyMutex(psSharedState->hTertiaryLock);

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "FreeContextSharedState: PVRSRVDestroyMutex failed on hTertiaryLock (%d)", eError));
			}
		}

		if (psSharedState->hSecondaryLock)
		{
			eError = PVRSRVDestroyMutex(psSharedState->hSecondaryLock);

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "FreeContextSharedState: PVRSRVDestroyMutex failed on hSecondaryLock (%d)", eError));
			}
		}

		if (psSharedState->hPrimaryLock)
		{
			eError = PVRSRVDestroyMutex(psSharedState->hPrimaryLock);

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR, "FreeContextSharedState: PVRSRVDestroyMutex failed on hPrimaryLock (%d)", eError));
			}
		}


		/* Clear for safety */
		GLES1MemSet(psSharedState, 0, sizeof(GLES1ContextSharedState));

		GLES1Free(IMG_NULL, psSharedState);
	}

	gc->psSharedState = IMG_NULL;
}

/***********************************************************************************
 Function Name      : CreateSharedState
 Inputs             : gc, psShareContext
 Outputs            : -
 Returns            : Success
 Description        : Initialises the data shared between contexts.
************************************************************************************/
static IMG_BOOL CreateSharedState(GLES1Context *gc, GLES1Context *psShareContext)
{
	GLES1ContextSharedState *psSharedState;
	static const GLES1NameType aeNameType[GLES1_MAX_SHAREABLE_NAMETYPE] =
	{
		  GLES1_NAMETYPE_TEXOBJ,
		  GLES1_NAMETYPE_BUFOBJ,
		  GLES1_NAMETYPE_RENDERBUFFER,
		  GLES1_NAMETYPE_FRAMEBUFFER
	};

	IMG_UINT32 i;

	/*
	 * Init shared structures unless we're sharing them
	 * with another context, in which they are already initialized.
	 */
	if(!gc->psSharedState)
	{
		if(psShareContext)
		{
			/* Get the shared state from the other context */
			GLES1_ASSERT(psShareContext->psSharedState);

			/* *** START CRITICAL SECTION *** */
			PVRSRVLockMutex(psShareContext->psSharedState->hPrimaryLock);

			gc->psSharedState = psShareContext->psSharedState;
			gc->psSharedState->ui32RefCount++;

			PVRSRVUnlockMutex(psShareContext->psSharedState->hPrimaryLock);
			/* *** END CRITICAL SECTION *** */
		}
		else
		{
			PVRSRV_ERROR eError;

			/* Create shared state */
			psSharedState = GLES1Calloc(gc, sizeof(GLES1ContextSharedState));

			if(!psSharedState)
			{
				return IMG_FALSE;
			}

			gc->psSharedState = psSharedState;

			psSharedState->ui32RefCount = 1;

			eError = PVRSRVCreateMutex(&psSharedState->hPrimaryLock);

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"CreateSharedState: PVRSRVCreateMutex failed on hPrimaryLock (%d)", eError));

				GLES1Free(IMG_NULL, psSharedState);

				return IMG_FALSE;
			}

			eError = PVRSRVCreateMutex(&psSharedState->hSecondaryLock);

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"CreateSharedState: PVRSRVCreateMutex failed on hSecondaryLock (%d)", eError));

				eError = PVRSRVDestroyMutex(psSharedState->hPrimaryLock);

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: PVRSRVDestroyMutex failed on hPrimaryLock (%d)", eError));
				}

				GLES1Free(IMG_NULL, psSharedState);

				return IMG_FALSE;
			}

			eError = PVRSRVCreateMutex(&psSharedState->hTertiaryLock);

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"CreateSharedState: PVRSRVCreateMutex failed on hTertiaryLock (%d)", eError));

				eError = PVRSRVDestroyMutex(psSharedState->hSecondaryLock);

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: PVRSRVDestroyMutex failed on hSecondaryLock (%d)", eError));
				}

				eError = PVRSRVDestroyMutex(psSharedState->hPrimaryLock);

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: PVRSRVDestroyMutex failed on hPrimaryLock (%d)", eError));
				}

				GLES1Free(IMG_NULL, psSharedState);

				return IMG_FALSE;
			}

			eError = PVRSRVCreateMutex(&psSharedState->hFlushListLock);

			if (eError != PVRSRV_OK)
			{
				PVR_DPF((PVR_DBG_ERROR,"CreateSharedState: PVRSRVCreateMutex failed on hFlushListLock (%d)", eError));

				eError = PVRSRVDestroyMutex(psSharedState->hTertiaryLock);

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: PVRSRVDestroyMutex failed on hTertiaryLock (%d)", eError));
				}

				eError = PVRSRVDestroyMutex(psSharedState->hSecondaryLock);

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: PVRSRVDestroyMutex failed on hSecondaryLock (%d)", eError));
				}

				eError = PVRSRVDestroyMutex(psSharedState->hPrimaryLock);

				if (eError != PVRSRV_OK)
				{
					PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: PVRSRVDestroyMutex failed on hPrimaryLock (%d)", eError));
				}

				GLES1Free(IMG_NULL, psSharedState);

				return IMG_FALSE;
			}


			/* Initialize the texture manager.
			 * Make sure that gc->psSharedState points to the right place before calling this
			 */
			psSharedState->psTextureManager = CreateTextureManager(gc);

			if(!psSharedState->psTextureManager)
			{
				PVR_DPF((PVR_DBG_ERROR,"CreateSharedState: Couldn't initialise the texture manager"));

				FreeContextSharedState(gc);

				return IMG_FALSE;
			}

			/* Initialize the PDS code variant manager */
			if(!KRM_Initialize(&psSharedState->sPDSVariantKRM,
								KRM_TYPE_3D,
							    IMG_TRUE,
								psSharedState->hSecondaryLock,
								gc->ps3DDevData,
								gc->psSysContext->sHWInfo.sMiscInfo.hOSGlobalEvent,
								ReclaimPDSVariantMemKRM,
								IMG_TRUE,
								DestroyPDSVariantGhostKRM))
			{
				PVR_DPF((PVR_DBG_ERROR,"CreateSharedState: Couldn't initialise the USSE code variant manager"));

				FreeContextSharedState(gc);

				return IMG_FALSE;
			}

			/* Initialize the USSE code variant manager */
			if(!KRM_Initialize(&psSharedState->sUSEShaderVariantKRM,
								KRM_TYPE_3D,
							    IMG_TRUE,
								psSharedState->hSecondaryLock,
								gc->ps3DDevData,
								gc->psSysContext->sHWInfo.sMiscInfo.hOSGlobalEvent,
								ReclaimUSEShaderVariantMemKRM,
								IMG_TRUE,
								DestroyUSECodeVariantGhostKRM))
			{
				PVR_DPF((PVR_DBG_ERROR,"CreateSharedState: Couldn't initialise the USSE code variant manager"));

				FreeContextSharedState(gc);

				return IMG_FALSE;
			}

			/* Initialize the TA kick buffer object manager */
			if(!KRM_Initialize(&psSharedState->sBufferObjectKRM,
								KRM_TYPE_TA,
							    IMG_TRUE,
								psSharedState->hSecondaryLock,
								gc->ps3DDevData,
								gc->psSysContext->sHWInfo.sMiscInfo.hOSGlobalEvent,
								ReclaimBufferObjectMemKRM,
								IMG_TRUE,
								DestroyBufferObjectGhostKRM))
			{
				PVR_DPF((PVR_DBG_ERROR,"CreateSharedState: Couldn't initialise the TA kick buffer object manager"));

				FreeContextSharedState(gc);

				return IMG_FALSE;
			}

			/* Initialize the code memory heaps */
			psSharedState->psUSEVertexCodeHeap = UCH_CodeHeapCreate(gc->ps3DDevData, UCH_USE_CODE_HEAP_TYPE,
																	gc->psSysContext->hUSEVertexHeap, psSharedState->hSecondaryLock,
																	gc->psSysContext->hPerProcRef);

			if(!psSharedState->psUSEVertexCodeHeap)
			{
				PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: Failed to create USSE vertex code heap!\n"));

				FreeContextSharedState(gc);

				return IMG_FALSE;
			}

			psSharedState->psUSEFragmentCodeHeap = UCH_CodeHeapCreate(gc->ps3DDevData, UCH_USE_CODE_HEAP_TYPE,
																	  gc->psSysContext->hUSEFragmentHeap, psSharedState->hSecondaryLock,
																	  gc->psSysContext->hPerProcRef);

			if(!psSharedState->psUSEFragmentCodeHeap)
			{
				PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: Failed to create USSE fragment code heap!\n"));

				FreeContextSharedState(gc);

				return IMG_FALSE;
			}

			psSharedState->psPDSFragmentCodeHeap = UCH_CodeHeapCreate(gc->ps3DDevData, UCH_PDS_CODE_HEAP_TYPE,
																	  gc->psSysContext->hPDSFragmentHeap, psSharedState->hSecondaryLock,
																	  gc->psSysContext->hPerProcRef);

			if(!psSharedState->psPDSFragmentCodeHeap)
			{
				PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: Failed to create PDS fragment code heap!\n"));

				FreeContextSharedState(gc);

				return IMG_FALSE;
			}

			psSharedState->psPDSVertexCodeHeap = UCH_CodeHeapCreate(gc->ps3DDevData, UCH_PDS_CODE_HEAP_TYPE,
																	gc->psSysContext->hPDSVertexHeap, psSharedState->hSecondaryLock,
																	gc->psSysContext->hPerProcRef);

			if(!psSharedState->psPDSVertexCodeHeap)
			{
				PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: Failed to create PDS vertex code heap!\n"));
				FreeContextSharedState(gc);
				return IMG_FALSE;
			}

			/* Initialize the names arrays */
			for(i = 0; i < GLES1_MAX_SHAREABLE_NAMETYPE; ++i)
			{
				psSharedState->apsNamesArray[i] = CreateNamesArray(gc, aeNameType[i], psSharedState->hPrimaryLock);

				if(!psSharedState->apsNamesArray[i])
				{
					PVR_DPF((PVR_DBG_ERROR,"CreateSharedState: Couldn't create names array %d", i));

					FreeContextSharedState(gc);

					return IMG_FALSE;
				}
			}
		}
	}

	GLES1_ASSERT(gc->psSharedState);

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : InitContext
 Inputs             : gc, psShareContext, psMode
 Outputs            : -
 Returns            : Success
 Description        : Initialises a context - may setup shared name tables from a
					  sharecontext. Also sets up default framebuffer
 ************************************************************************************/
static IMG_BOOL InitContext(GLES1Context *gc, GLES1Context *psShareContext, EGLcontextMode *psMode)
{
	IMG_UINT32 *pui32Buffer;
#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)
	GLES1NameType aeNameType[GLES1_MAX_UNSHAREABLE_NAMETYPE] =
	{
		  GLES1_NAMETYPE_VERARROBJ
	};
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */

	IMG_UINT32 i,j;

	GLES1_ASSERT(gc);

	GetApplicationHints(&gc->sAppHints, psMode);

	if(!CreateSharedState(gc, psShareContext))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: CreateSharedState failed"));

		goto FAILED_CreateSharedState;
	}

	SceHeapOptParam heapOpt;
	heapOpt.size = sizeof(SceHeapOptParam);
	heapOpt.memblockType = SCE_HEAP_OPT_MEMBLOCK_TYPE_USER;

	if (gc->sAppHints.ui32UNCTexHeapSize)
	{
		heapOpt.memblockType = SCE_HEAP_OPT_MEMBLOCK_TYPE_USER_NC;

		gc->pvUNCHeap = sceHeapCreateHeap(gc->ps3DDevData,
			gc->psSysContext->hDevMemContext,
			"OGLES1UNCHeap",
			gc->sAppHints.ui32UNCTexHeapSize,
			gc->sAppHints.bEnableUNCAutoExtend ? SCE_HEAP_AUTO_EXTEND : 0,
			&heapOpt);

		if (!gc->pvUNCHeap)
		{
			PVR_DPF((PVR_DBG_ERROR, "InitContext: Couldn't create UNC heap"));

			return IMG_FALSE;
		}
	}

	if (gc->sAppHints.ui32CDRAMTexHeapSize)
	{
		heapOpt.memblockType = SCE_HEAP_OPT_MEMBLOCK_TYPE_CDRAM;

		gc->pvCDRAMHeap = sceHeapCreateHeap(gc->ps3DDevData,
			gc->psSysContext->hDevMemContext,
			"OGLES1CDRAMHeap",
			gc->sAppHints.ui32UNCTexHeapSize,
			gc->sAppHints.bEnableCDRAMAutoExtend ? SCE_HEAP_AUTO_EXTEND : 0,
			&heapOpt);

		if (!gc->pvCDRAMHeap)
		{
			PVR_DPF((PVR_DBG_ERROR, "InitContext: Couldn't create CDRAM heap"));
			if (gc->pvUNCHeap)
			{
				sceHeapDeleteHeap(gc->pvUNCHeap);
			}

			return IMG_FALSE;
		}
	}

	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_ULT);

	IMG_UINT32 ui32UltRuntimeWorkAreaSize = sceUltUlthreadRuntimeGetWorkAreaSize(gc->sAppHints.ui32SwTexOpMaxUltNum, gc->sAppHints.ui32SwTexOpThreadNum);
	IMG_PVOID pvUltRuntimeWorkArea = GLES1Malloc(gc, ui32UltRuntimeWorkAreaSize);
	gc->pvUltRuntime = GLES1Malloc(gc, _SCE_ULT_ULTHREAD_RUNTIME_SIZE);

	SceUltUlthreadRuntimeOptParam sUltOptParam;
	sceUltUlthreadRuntimeOptParamInitialize(&sUltOptParam);
	sUltOptParam.oneShotThreadStackSize = 4 * 1024 + (4 * 1024 * (IMG_UINT32)((IMG_FLOAT)gc->sAppHints.ui32SwTexOpMaxUltNum / 4.0f));
	sUltOptParam.workerThreadAttr = 0;
	sUltOptParam.workerThreadCpuAffinityMask = gc->sAppHints.ui32SwTexOpThreadAffinity;
	sUltOptParam.workerThreadOptParam = 0;
	sUltOptParam.workerThreadPriority = gc->sAppHints.ui32SwTexOpThreadPriority;

	i = sceUltUlthreadRuntimeCreate(gc->pvUltRuntime,
		"OGLES1UltRuntime",
		gc->sAppHints.ui32SwTexOpMaxUltNum,
		gc->sAppHints.ui32SwTexOpThreadNum,
		pvUltRuntimeWorkArea,
		&sUltOptParam);

	if (i != SCE_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "InitContext: Couldn't create ULT runtime: 0x%X", i));

		goto FAILED_sceUltUlthreadRuntimeCreate;
	}

	gc->bSwTexOpFin = IMG_FALSE;
	gc->hSwTexOpThrd = sceKernelCreateThread("OGLES1AsyncTexOpCl", texOpAsyncCleanupThread, SCE_KERNEL_LOWEST_PRIORITY_USER, SCE_KERNEL_4KiB, 0, 0, SCE_NULL);
	sceKernelStartThread(gc->hSwTexOpThrd, 4, &gc);

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)
	/* Initialize the unshareable names arrays */
	for (i = 0; i < GLES1_MAX_UNSHAREABLE_NAMETYPE; i++)
	{
		gc->apsNamesArray[i] = CreateNamesArray(gc, aeNameType[i], (PVRSRV_MUTEX_HANDLE)0);

		if(!gc->apsNamesArray[i])
		{
			PVR_DPF((PVR_DBG_ERROR,"InitContext: Couldn't create unshareable names array %d", i));

			DestroyNamesArray(gc, gc->apsNamesArray[i]);

			goto FAILED_TASync;
		}
	}
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */

	if (GLES1ALLOCDEVICEMEM(gc->ps3DDevData,
							gc->psSysContext->hSyncInfoHeap,
							PVRSRV_MEM_WRITE | PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ | PVRSRV_MEM_CACHE_CONSISTENT,
							4,
							0,
							&gc->sKRMTAStatusUpdate.psMemInfo) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "InitContext: Failed to create TA sync object"));

		goto FAILED_TASync;
	}

	gc->sKRMTAStatusUpdate.ui32StatusValue = 1;

	PVRSRVMemSet(gc->sKRMTAStatusUpdate.psMemInfo->pvLinAddr, 0, 4);

	/*
	** Initialize larger subsystems by calling their init codes.
	*/
	SetupTransformLightingProcs(gc);

	InitTexCombineState(gc);

	if(!InitTransformState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: Couldn't init transform state"));

		goto FAILED_InitTransformState;
	}

	if(!InitLightingState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: Couldn't init lighting state"));

		goto FAILED_InitLightingState;
	}

	for(i=0; i < GLES1_HINT_NUMHINTS; i++)
	{
		gc->sState.sHints.eHint[i] = GL_DONT_CARE;
	}

	j=0;
	for(i=0; i < CBUF_NUM_TA_BUFFERS; i++)
	{
		IMG_UINT32 ui32Size;
		IMG_HANDLE hMemHeap;

		switch(i)
		{
			case CBUF_TYPE_PDS_VERT_BUFFER:
			{
				hMemHeap = gc->psSysContext->hPDSVertexHeap;
				ui32Size = gc->sAppHints.ui32DefaultPDSVertBufferSize;

				break;
			}
			case CBUF_TYPE_VERTEX_DATA_BUFFER:
			{
				hMemHeap = gc->psSysContext->hGeneralHeap;
				ui32Size = gc->sAppHints.ui32DefaultVertexBufferSize;

				break;
			}
			case CBUF_TYPE_INDEX_DATA_BUFFER:
			{
				hMemHeap = gc->psSysContext->hGeneralHeap;
				ui32Size = gc->sAppHints.ui32DefaultIndexBufferSize;

				break;
			}
			case CBUF_TYPE_VDM_CTRL_BUFFER:
			{
				hMemHeap = gc->psSysContext->hGeneralHeap;
				ui32Size = gc->sAppHints.ui32DefaultVDMBufferSize;

				break;
			}
			case CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER:
			{
				hMemHeap = gc->psSysContext->hPDSVertexHeap;

				if(gc->sAppHints.bEnableStaticPDSVertex)
				{
					ui32Size = ((gc->sAppHints.ui32DefaultPregenPDSVertBufferSize / ALIGNED_PDS_AUXILIARY_PROG_SIZE) * ALIGNED_PDS_AUXILIARY_PROG_SIZE);
				}
				else
				{
					PVR_DPF((PVR_DBG_WARNING,"InitContext: ignoring buffer type CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER"));
					ui32Size = 4;
				}


				break;
			}
			case CBUF_TYPE_MTE_COPY_PREGEN_BUFFER:
			{
				hMemHeap = gc->psSysContext->hPDSVertexHeap;

				if(gc->sAppHints.bEnableStaticMTECopy)
				{
					ui32Size = ((gc->sAppHints.ui32DefaultPregenMTECopyBufferSize / GLES1_ALIGNED_MTE_COPY_PROG_SIZE) * GLES1_ALIGNED_MTE_COPY_PROG_SIZE);
				}
				else
				{
					PVR_DPF((PVR_DBG_WARNING,"InitContext: ignoring buffer type CBUF_TYPE_MTE_COPY_PREGEN_BUFFER"));
					ui32Size = 4;
				}

				break;
			}
			case CBUF_TYPE_PDS_AUXILIARY_PREGEN_BUFFER:
			{
				/* Unused */
				gc->apsBuffers[i] = IMG_NULL;
				continue;
			}
			default:
			{
				PVR_DPF((PVR_DBG_ERROR,"InitContext: Invalid buffer type"));

				goto FAILED_CBUF_CreateBuffer;
			}
		}

		gc->apsBuffers[i] = CBUF_CreateBuffer(gc->ps3DDevData, 
												i, 
												hMemHeap, 
												gc->psSysContext->hSyncInfoHeap, 
												gc->psSysContext->sHWInfo.sMiscInfo.hOSGlobalEvent, 
												ui32Size,
												gc->psSysContext->hPerProcRef);

		if(!gc->apsBuffers[i])
		{
			PVR_DPF((PVR_DBG_ERROR,"InitContext: Failed to create buffer %u",i));

			goto FAILED_CBUF_CreateBuffer;
		}

		/* Point the TA Kick status update to the device memory reserved for status updates in the buffer */
		gc->sKickTA.asTAStatusUpdate[j].hKernelMemInfo = gc->apsBuffers[i]->psStatusUpdateMemInfo->hKernelMemInfo;
		gc->sKickTA.asTAStatusUpdate[j].sCtlStatus.sStatusDevAddr.uiAddr = gc->apsBuffers[i]->psStatusUpdateMemInfo->sDevVAddr.uiAddr;

		/* set the read offset pointer in the buffer */
		gc->apsBuffers[i]->pui32ReadOffset = (IMG_UINT32*)gc->apsBuffers[i]->psStatusUpdateMemInfo->pvLinAddr;

		j++;

		if(i == CBUF_TYPE_MTE_COPY_PREGEN_BUFFER)
		{
			if(gc->sAppHints.bEnableStaticMTECopy)
			{
				if(SetupMTEPregenBuffer(gc) != GLES1_NO_ERROR)
				{
					PVR_DPF((PVR_DBG_ERROR,"InitContext: Failed to fill pregen buffer %u",i));

					goto FAILED_CBUF_CreateBuffer;
				}
			}
		}

	}

	gc->sKickTA.sKickTACommon.ui32NumTAStatusVals = j;

	/* Setup 3D status val sync */
	gc->sKickTA.sKickTACommon.ui32Num3DStatusVals = 2;

	if(!InitFFTNLState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: InitFFTNLState failed"));

		goto FAILED_InitFFTNLState;
	}


	if(!HashTableCreate(gc, &gc->sProgram.sFFTextureBlendHashTable, STATEHASH_LOG2TABLESIZE, STATEHASH_MAXNUMENTRIES, DestroyHashedBlendState
#ifdef HASHTABLE_DEBUG
		, "FFTexBlend"
#endif
		))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: HashTableCreate for FFtextureblend failed"));

		goto FAILED_CreateHashTableFFTexBlend;
	}

	if(!HashTableCreate(gc, &gc->sProgram.sPDSFragmentVariantHashTable, STATEHASH_LOG2TABLESIZE, STATEHASH_MAXNUMENTRIES, DestroyHashedPDSVariant
#ifdef HASHTABLE_DEBUG
		, "PDSFragVar"
#endif
		))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: HashTableCreate for PDSFragVariant failed"));

		goto FAILED_CreateHashTablePDSFragVariant;
	}

	if(!HashTableCreate(gc, &gc->sProgram.sPDSFragmentSAHashTable, STATEHASH_LOG2TABLESIZE, STATEHASH_MAXNUMENTRIES, DestroyHashedPDSFragSA
#ifdef HASHTABLE_DEBUG
		, "PDSFragSA"
#endif
		))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: HashTableCreate for PDSFragSA failed"));

		goto FAILED_CreateHashTablePDSFragSA;
	}

	if(!CreateTextureState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: CreateTextureState failed"));

		goto FAILED_CreateTextureState;
	}

	if(!CreateBufObjState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: CreateBufObjState failed"));

		goto FAILED_CreateBufObjState;
	}

	if(!CreateFrameBufferState(gc, psMode))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: CreateFrameBufferState failed"));

		goto FAILED_CreateFrameBufferState;
	}

	if(!CreateVertexArrayObjectState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: CreateVertexArrayObjectState failed"));

		goto FAILED_CreateVertexArrayObjectState;
	}

	/* Initialise VertexArrayState after CreateVertexArrayObjectState() */
	InitVertexArrayState(gc);


	/* Initialise the TA kick VAO manager */
	if(!KRM_Initialize(&gc->sVAOKRM,
					    KRM_TYPE_TA,
					    IMG_FALSE,
					    0,  /* PVRSRV_MUTEX_HANDLE */
					    gc->ps3DDevData,
					    gc->psSysContext->sHWInfo.sMiscInfo.hOSGlobalEvent,
					    ReclaimVAOMemKRM,
					    IMG_TRUE,
					    DestroyVAOGhostKRM))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: Couldn't initialise the TA kick VAO manager"));

		goto FAILED_CreateVertexArrayObjectState;
	}


	if(GLES1ALLOCDEVICEMEM(gc->ps3DDevData,
							gc->psSysContext->hUSEFragmentHeap,
							PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
							EURASIA_USE_INSTRUCTION_SIZE,
							EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE,
							&gc->sProgram.psDummyFragUSECode) == PVRSRV_OK)
	{
		pui32Buffer = (IMG_UINT32 *)gc->sProgram.psDummyFragUSECode->pvLinAddr;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

		BuildPHASLastPhase ((PPVR_USE_INST) pui32Buffer, EURASIA_USE1_OTHER2_PHAS_END);

#else
		/* Write a NOP to the buffer */
		pui32Buffer[0] = 0;
		pui32Buffer[1] = (EURASIA_USE1_FLOWCTRL_OP2_NOP << EURASIA_USE1_FLOWCTRL_OP2_SHIFT) |
		                 (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
		                 EURASIA_USE1_END;

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "InitContext: Failed to create Dummy USSE code block\n"));

		goto FAILED_DummyFragUSECode;
	}

#if defined(FIX_HW_BRN_31988)
	if(GLES1ALLOCDEVICEMEM(gc->ps3DDevData,
						   gc->psSysContext->hUSEFragmentHeap,
						   PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
						   EURASIA_USE_INSTRUCTION_SIZE * 6,	   /* size of PHAS/NOP/TST/LDAD/WDF/NOP */
						   EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE,
						   &gc->sProgram.psBRN31988FragUSECode) == PVRSRV_OK)
	{
	    IMG_UINT32 *pui32BufferBase;

		pui32Buffer = pui32BufferBase = (IMG_UINT32 *)gc->sProgram.psBRN31988FragUSECode->pvLinAddr;
		
		BuildPHASLastPhase ((PPVR_USE_INST) pui32BufferBase, 0);

		pui32Buffer += USE_INST_LENGTH;

		pui32Buffer = USEGenWriteBRN31988Fragment(pui32Buffer);

		BuildNOP((PPVR_USE_INST)pui32Buffer, EURASIA_USE1_END, IMG_FALSE);

		GLES1_ASSERT((IMG_UINT32)(pui32Buffer - pui32BufferBase) <= EURASIA_USE_INSTRUCTION_SIZE * 5);
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "CreateProgramState: Failed to BRN31988 Frag USSE code block\n"));

		goto FAILED_31988FragUSECode;
	}
#endif

	if(GLES1ALLOCDEVICEMEM(gc->ps3DDevData,
							gc->psSysContext->hUSEVertexHeap,
							PVRSRV_MEM_READ | PVRSRV_MEM_NO_SYNCOBJ,
							EURASIA_USE_INSTRUCTION_SIZE,
							EURASIA_USE_INSTRUCTION_CACHE_LINE_SIZE,
							&gc->sProgram.psDummyVertUSECode) == PVRSRV_OK)
	{
		IMG_UINT32 *pui32Buffer = (IMG_UINT32 *)gc->sProgram.psDummyVertUSECode->pvLinAddr;

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)

		BuildPHASLastPhase ((PPVR_USE_INST) pui32Buffer, EURASIA_USE1_OTHER2_PHAS_END);

#else
		/* Write a NOP to the buffer */
		pui32Buffer[0] = 0;
		pui32Buffer[1] = (EURASIA_USE1_FLOWCTRL_OP2_NOP << EURASIA_USE1_FLOWCTRL_OP2_SHIFT) |
						 (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT) |
						 EURASIA_USE1_END;

#endif /* defined(SGX_FEATURE_USE_UNLIMITED_PHASES) */

	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "InitContext: Failed to create Dummy USSE code block\n"));

		goto FAILED_DummyVertUSECode;
	}

	/* this need to have the dummy USE program already in place*/
	if(gc->sAppHints.bEnableStaticPDSVertex)
	{
		if(SetupPDSVertexPregenBuffer(gc) != GLES1_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"InitContext: Failed to fill pregen buffer %u",i));

			goto FAILED_DummyVertUSECode;
		}
	}

	if(!InitSpecialUSECodeBlocks(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: InitSpecialUSECodeBlocks failed"));

		goto FAILED_InitSpecialUSECodeBlocks;
	}

	gc->sState.sRaster.ui32ColorMask = GLES1_COLORMASK_ALL;

	gc->sState.sRaster.sClearColor.fRed = GLES1_Zero;
	gc->sState.sRaster.sClearColor.fGreen = GLES1_Zero;
	gc->sState.sRaster.sClearColor.fBlue = GLES1_Zero;
	gc->sState.sRaster.sClearColor.fAlpha = GLES1_Zero;
	gc->sState.sRaster.ui32ClearColor = 0;

	gc->sState.sRaster.ui32LogicOp = GL_COPY;

	gc->sState.sRaster.ui32BlendFunction = ((GLES1_BLENDFACTOR_ONE << GLES1_BLENDFACTOR_RGBSRC_SHIFT)	|
											(GLES1_BLENDFACTOR_ONE << GLES1_BLENDFACTOR_ALPHASRC_SHIFT) |
											(GLES1_BLENDFACTOR_ZERO << GLES1_BLENDFACTOR_RGBDST_SHIFT)	|
											(GLES1_BLENDFACTOR_ZERO << GLES1_BLENDFACTOR_ALPHADST_SHIFT));

	gc->sState.sRaster.ui32BlendEquation =	(GLES1_BLENDMODE_ADD << GLES1_BLENDMODE_RGB_SHIFT) |
											(GLES1_BLENDMODE_ADD << GLES1_BLENDMODE_ALPHA_SHIFT);

	gc->sState.sRaster.ui32AlphaTestFunc = GL_ALWAYS;
	gc->sState.sRaster.ui32AlphaTestRef = 0;

	gc->sState.sPoint.pfPointSize = &gc->sState.sPoint.fAliasedSize;
	gc->sState.sPoint.fSmoothSize = GLES1_One;
	gc->sState.sPoint.fAliasedSize = GLES1_One;
	gc->sState.sPoint.fRequestedSize = GLES1_One;

	gc->sState.sPoint.fClampMin = GLES1_Zero;
	gc->sState.sPoint.fClampMax = MAX(GLES1_ALIASED_POINT_SIZE_MAX, GLES1_SMOOTH_POINT_SIZE_MAX);
	gc->sState.sPoint.pfMinPointSize = &gc->sState.sPoint.fMinAliasedSize;
	gc->sState.sPoint.pfMaxPointSize = &gc->sState.sPoint.fMaxAliasedSize;
	gc->sState.sPoint.fMinAliasedSize = gc->sState.sPoint.fClampMin;
	gc->sState.sPoint.fMaxAliasedSize = (IMG_FLOAT)GLES1_ALIASED_POINT_SIZE_MAX;
	gc->sState.sPoint.fMinSmoothSize = gc->sState.sPoint.fClampMin;
	gc->sState.sPoint.fMaxSmoothSize = GLES1_SMOOTH_POINT_SIZE_MAX;
	gc->sState.sPoint.afAttenuation[0] = GLES1_One;
	gc->sState.sPoint.afAttenuation[1] = GLES1_Zero;
	gc->sState.sPoint.afAttenuation[2] = GLES1_Zero;
	gc->sState.sPoint.afAttenuation[3] = GLES1_Half;

	gc->sState.sLine.pfLineWidth = &gc->sState.sLine.fAliasedWidth;
	gc->sState.sLine.fSmoothWidth = GLES1_One;
	gc->sState.sLine.fAliasedWidth = GLES1_One;

	gc->sState.sPolygon.eCullMode = GL_BACK;
	gc->sState.sPolygon.eFrontFaceDirection = GL_CCW;
	gc->sState.sPolygon.factor.fVal = GLES1_Zero;
	gc->sState.sPolygon.fUnits = GLES1_Zero;

#if defined(EURASIA_MTE_SHADE_GOURAUD)
	gc->sState.sShade.ui32ShadeModel = EURASIA_MTE_SHADE_GOURAUD;
#endif

	gc->sState.sFog.eMode 	  		= GL_EXP;
	gc->sState.sFog.fDensity  		= GLES1_One;
	gc->sState.sFog.fEnd 	  		= GLES1_One;
	gc->sState.sFog.fStart 	  		= GLES1_Zero;
	gc->sState.sFog.fOneOverEMinusS = GLES1_One;
	gc->sState.sFog.ui32Color 		= 0;

	gc->sState.sStencil.ui32Stencil = EURASIA_ISPC_SCMP_ALWAYS	|
									  EURASIA_ISPC_SOP1_KEEP	|
									  EURASIA_ISPC_SOP2_KEEP	|
									  EURASIA_ISPC_SOP3_KEEP	|
									  (GLES1_MAX_STENCIL_VALUE << EURASIA_ISPC_SCMPMASK_SHIFT) |
									  (GLES1_MAX_STENCIL_VALUE << EURASIA_ISPC_SWMASK_SHIFT);
	gc->sState.sStencil.ui32StencilRef = (0 << EURASIA_ISPA_SREF_SHIFT);
	gc->sState.sStencil.ui32Clear = 0;

	gc->sState.sStencil.ui32StencilCompareMaskIn = GLES1_MAX_STENCIL_VALUE;
	gc->sState.sStencil.ui32StencilWriteMaskIn	 = GLES1_MAX_STENCIL_VALUE;
	gc->sState.sStencil.i32StencilRefIn		 = 0;
	gc->sState.sStencil.ui32MaxFBOStencilVal = GLES1_MAX_STENCIL_VALUE;

	gc->sState.sDepth.ui32TestFunc = EURASIA_ISPA_DCMPMODE_LT;
	gc->sState.sDepth.fClear = GLES1_One;

	gc->sState.sMultisample.bSampleCoverageInvert = IMG_FALSE;
	gc->sState.sMultisample.fSampleCoverageValue = GLES1_One;

	gc->ui32FrameEnables = GLES1_FS_DITHER_ENABLE | GLES1_FS_MULTISAMPLE_ENABLE;

	gc->sState.sClientPixel.ui32UnpackAlignment = 4;
	gc->sState.sClientPixel.ui32PackAlignment = 4;


	/*
	** Setup Current state
	*/
	gc->sState.sCurrent.asAttrib[AP_COLOR].fX = GLES1_One;
	gc->sState.sCurrent.asAttrib[AP_COLOR].fY = GLES1_One;
	gc->sState.sCurrent.asAttrib[AP_COLOR].fZ = GLES1_One;
	gc->sState.sCurrent.asAttrib[AP_COLOR].fW = GLES1_One;

	gc->sState.sCurrent.asAttrib[AP_NORMAL].fZ = GLES1_One;

	for (i=0; i < GLES1_MAX_TEXTURE_UNITS; i++)
	{
		gc->sState.sCurrent.asAttrib[AP_TEXCOORD0+i].fW = GLES1_One;
	}

#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
	gc->sState.sCurrent.ui32MatrixPaletteIndex = 0;
#endif


#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)

	gc->sSmallKickTA.ui32NumIndicesThisTA   	= 0;
	gc->sSmallKickTA.ui32NumPrimitivesThisTA	= 0;
	gc->sSmallKickTA.ui32NumIndicesThisFrame	= 0;
	gc->sSmallKickTA.ui32NumPrimitivesThisFrame	= 0;

	/* Default value to ensure no TA splitting on the first frame */
	gc->sSmallKickTA.ui32KickThreshold = 0xFFFFFFFF;

#endif

	PDUMP_STRING_CONTINUOUS((gc, "PDump from OpenGL ES 1.1 driver, version %s. HW variant %s, %d\n",PVRVERSION_STRING, pszRenderer, SGX_CORE_REV));

	gc->ui32DirtyMask = GLES1_DIRTYFLAG_ALL;
	gc->ui32EmitMask  = GLES1_EMITSTATE_ALL;

	return IMG_TRUE;


	/* Clean up any memory */

FAILED_InitSpecialUSECodeBlocks:

	GLES1FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psDummyVertUSECode);

FAILED_DummyVertUSECode:

#if defined(FIX_HW_BRN_31988)
	GLES1FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psBRN31988FragUSECode);

FAILED_31988FragUSECode:
#endif
	GLES1FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psDummyFragUSECode);

FAILED_DummyFragUSECode:

	FreeVertexArrayObjectState(gc);

FAILED_CreateVertexArrayObjectState:

	FreeFrameBufferState(gc);

FAILED_CreateFrameBufferState:

	FreeBufObjState(gc);

FAILED_CreateBufObjState:

	FreeTextureState(gc);

FAILED_CreateTextureState:

	HashTableDestroy(gc, &gc->sProgram.sPDSFragmentSAHashTable);

FAILED_CreateHashTablePDSFragSA:

	HashTableDestroy(gc, &gc->sProgram.sPDSFragmentVariantHashTable);

FAILED_CreateHashTablePDSFragVariant:

	HashTableDestroy(gc, &gc->sProgram.sFFTextureBlendHashTable);

FAILED_CreateHashTableFFTexBlend:

	FreeFFTNLState(gc);

FAILED_InitFFTNLState:

FAILED_CBUF_CreateBuffer:
	for(i=0; i<CBUF_NUM_TA_BUFFERS; i++)
	{
		if(gc->apsBuffers[i])
		{
			CBUF_DestroyBuffer(gc->ps3DDevData, gc->apsBuffers[i]);
		}
	}

	FreeLightingState(gc);

FAILED_InitLightingState:

	FreeTransformState(gc);

FAILED_InitTransformState:

	GLES1FREEDEVICEMEM(gc->ps3DDevData, gc->sKRMTAStatusUpdate.psMemInfo);

FAILED_TASync:

	FreeContextSharedState(gc);

	sceUltUlthreadRuntimeDestroy((SceUltUlthreadRuntime *)gc->pvUltRuntime);

FAILED_sceUltUlthreadRuntimeCreate:

	if (gc->pvUNCHeap)
	{
		sceHeapDeleteHeap(gc->pvUNCHeap);
	}
	if (gc->pvCDRAMHeap)
	{
		sceHeapDeleteHeap(gc->pvCDRAMHeap);
	}

FAILED_CreateSharedState:

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : DeInitContext
 Inputs             :
 Outputs            :
 Returns            :
 Description        :
************************************************************************************/
static IMG_BOOL DeInitContext(GLES1Context *gc)
{
	IMG_UINT32 i;
	IMG_BOOL bPass = IMG_TRUE;

	if(!gc->psSharedState)
	{
		/* If there's no shared state then the rest of the context hasn't been initialised */
		return IMG_TRUE;
	}

	HashTableDestroy(gc, &gc->sProgram.sPDSFragmentVariantHashTable);

	HashTableDestroy(gc, &gc->sProgram.sFFTextureBlendHashTable);

	HashTableDestroy(gc, &gc->sProgram.sPDSFragmentSAHashTable);

	FreeFFTNLState(gc);

#if defined(FIX_HW_BRN_31988)
	GLES1FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psBRN31988FragUSECode);
#endif

	GLES1FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psDummyFragUSECode);
	GLES1FREEDEVICEMEM(gc->ps3DDevData, gc->sProgram.psDummyVertUSECode);

	FreeTransformState(gc);

	FreeLightingState(gc);

	FreeProgramState(gc);

	if(FreeTextureState(gc) != IMG_TRUE)
	{
		PVR_DPF((PVR_DBG_ERROR,"DeInitContext: FreeTextureState failed"));

		bPass = IMG_FALSE;
	}

	FreeBufObjState(gc);

	/* FrameBuffers must be freed _after_ freeing textures due to FBOs. See FreeTexture() */
	FreeFrameBufferState(gc);

	/* Free vao state */
	FreeVertexArrayObjectState(gc);

	/* Destroy the TA kick VAO manager */
	KRM_Destroy(gc, &gc->sVAOKRM);

	FreeContextSharedState(gc);

#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)
	/* Destroy all the unshareable resources */
	for (i = 0; i < GLES1_MAX_UNSHAREABLE_NAMETYPE; i++)
	{
		if(gc->apsNamesArray[i])
		{
			DestroyNamesArray(gc, gc->apsNamesArray[i]);
		}
	}
#endif /* defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT) */

	/* TexStreamsState must be freed after freeing all the default and named textures, 
	   which might refer to stream buffer devices */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
	if(FreeTexStreamState(gc) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"DeInitContext: FreeTexStreamState failed"));

		bPass = IMG_FALSE;
	}
#endif

	GLES1FREEDEVICEMEM(gc->ps3DDevData, gc->sKRMTAStatusUpdate.psMemInfo);

#if defined (TIMING) || defined (DEBUG)
	OutputMetrics(gc);
#endif

	for(i=0; i < CBUF_NUM_TA_BUFFERS; i++)
	{
		if(gc->apsBuffers[i])
		{
			CBUF_DestroyBuffer(gc->ps3DDevData, gc->apsBuffers[i]);
		}
	}

	gc->bSwTexOpFin = IMG_TRUE;

	sceKernelWaitThreadEnd(gc->hSwTexOpThrd, SCE_NULL, SCE_NULL);

	if (gc->pvUNCHeap)
	{
		sceHeapDeleteHeap(gc->pvUNCHeap);
	}
	if (gc->pvCDRAMHeap)
	{
		sceHeapDeleteHeap(gc->pvCDRAMHeap);
	}

	sceUltUlthreadRuntimeDestroy((SceUltUlthreadRuntime *)gc->pvUltRuntime);

	return bPass;
}


/***********************************************************************************
 Function Name      : GLESCreateGC
 Inputs             : psSysContext, psMode, hShareContext
 Outputs            : phContext
 Returns            : Success
 Description        : Called to create an graphics context
 ************************************************************************************/
static IMG_BOOL GLESCreateGC(SrvSysContext *psSysContext,
							 EGLContextHandle *phContext,
							 EGLcontextMode *psMode,
							 EGLContextHandle hShareContext)
{
	GLES1Context *gc, *psShareContext;

	psShareContext = (GLES1Context *)hShareContext;

	gc = _GLES1CreateContext();

	if (!gc)
	{
		PVR_DPF((PVR_DBG_ERROR,"GLESCreateGC: Can't alloc memory for the gc"));

		return IMG_FALSE;
	}

	gc->psSysContext = psSysContext;
	gc->ps3DDevData  = &psSysContext->s3D;

	if(BuildExtensionString(gc) != IMG_TRUE)
	{
		PVR_DPF((PVR_DBG_ERROR,"GLESCreateGC: Failed to create extension string"));

		goto Failed_GC_Creation;
	}

	if (!InitContext(gc, psShareContext, psMode))
	{
		PVR_DPF((PVR_DBG_ERROR,"GLESCreateGC: Failed to init the gc"));

		goto Failed_GC_Creation;
	}

	//TODOPSP2: hi Rinne!
	IMG_PVOID pvDummy = GLES1Malloc(0, 4);
	SceKernelMemBlockInfo sMbInfo;
	sMbInfo.size = sizeof(SceKernelMemBlockInfo);
	sceKernelGetMemBlockInfoByAddr(pvDummy, &sMbInfo);
	PVRSRVMapMemoryToGpu(
		gc->ps3DDevData,
		gc->psSysContext->hDevMemContext,
		0,
		sMbInfo.mappedSize,
		0,
		sMbInfo.mappedBase,
		PVRSRV_MEM_READ | PVRSRV_MEM_WRITE | PVRSRV_MEM_USER_SUPPLIED_DEVVADDR,
		IMG_NULL);
	GLES1Free(IMG_NULL, pvDummy);

#if defined (TIMING) || defined (DEBUG)
	if (!InitMetrics(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"GLESCreateGC: Metrics init failed"));

		goto Failed_GC_Creation;
	}

	InitProfileData(gc);
#endif

	*phContext = (EGLContextHandle)gc;

	return IMG_TRUE;


Failed_GC_Creation:

	DeInitContext(gc);

	_GLES1DestroyContext(gc);

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : GLESDestroyGC
 Inputs             : hContext
 Outputs            : -
 Returns            : Success
 Description        : Called to destroy a graphics context
************************************************************************************/
static IMG_BOOL GLESDestroyGC(EGLContextHandle hContext)
{
	GLES1Context *gc = (GLES1Context *)hContext;
	IMG_BOOL bReturnValue;

	bReturnValue = IMG_TRUE;

	if (!DeInitContext(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"GLESDestroyGC: Failed to deinit the gc"));

		bReturnValue = IMG_FALSE;
	}

	DestroyExtensionString(gc);

	_GLES1DestroyContext(gc);

	return bReturnValue;
}


/***********************************************************************************
 Function Name      : GLESMakeCurrentGC
 Inputs             : psWriteDrawable, psReadDrawable, hContext
 Outputs            : -
 Returns            : Success
 Description        : Make a read/write drawable current on a GC
************************************************************************************/
static IMG_EGLERROR GLESMakeCurrentGC(EGLRenderSurface *psWriteDrawable,
									  EGLRenderSurface *psReadDrawable,
									  EGLContextHandle hContext)
{
	GLES1Context *gc = (GLES1Context *)hContext;

	__GLES1_SET_CONTEXT(gc);

	/* If no gc - we are making "uncurrent" */
	if(gc)
	{
		EGLDrawableParams sDrawParams, sReadParams;
		/* Please the compiler */
		sDrawParams.ui32Width  = 0;
		sDrawParams.ui32Height = 0;

		if (psWriteDrawable)
		{
			if (!KEGLGetDrawableParameters(psWriteDrawable->hEGLSurface, &sDrawParams, IMG_TRUE))
			{
				PVR_DPF((PVR_DBG_ERROR,"GLESMakeCurrentGC: Invalid drawable - what do we do?"));

				goto BAD_CONTEXT;
			}

			if(psReadDrawable)
			{
				if (!KEGLGetDrawableParameters(psReadDrawable->hEGLSurface, &sReadParams, IMG_TRUE))
				{
					PVR_DPF((PVR_DBG_ERROR,"GLESMakeCurrentGC: Invalid drawable - what do we do?"));

					goto BAD_CONTEXT;
				}
			}
			else
			{
				/* Fake up read params, but mark the rendersurface as null, as there is no valid readsurface */
				sReadParams = sDrawParams;
				sReadParams.psRenderSurface = IMG_NULL;
			}
		}
		else
		{
			if (!KEGLGetDrawableParameters(psReadDrawable->hEGLSurface, &sReadParams, IMG_TRUE))
			{
				PVR_DPF((PVR_DBG_ERROR,"GLESMakeCurrentGC: Invalid drawable - what do we do?"));

				goto BAD_CONTEXT;
			}
		}

		if((psWriteDrawable && (sDrawParams.ui32Width == 0 || sDrawParams.ui32Height == 0)) ||
			(psReadDrawable && (sReadParams.ui32Width == 0 || sReadParams.ui32Height == 0)))
		{
			PVR_DPF((PVR_DBG_ERROR,"GLESMakeCurrentGC: Invalid drawable - what do we do?"));

			goto BAD_CONTEXT;
		}

		/* If we're making a new default framebuffer current but the currently
		 * bound FBO is not the default FBO, just update the default
		 * framebuffer's parameters and don't update the GC.
		 */
		if(gc->sFrameBuffer.psActiveFrameBuffer != &gc->sFrameBuffer.sDefaultFrameBuffer)
		{
			gc->sFrameBuffer.sDefaultFrameBuffer.sReadParams = sReadParams;
			gc->sFrameBuffer.sDefaultFrameBuffer.sDrawParams = sDrawParams;
		}
		else
		{
			ChangeDrawableParams(gc, &gc->sFrameBuffer.sDefaultFrameBuffer, &sReadParams, &sDrawParams);
		}

		if(!gc->bHasBeenCurrent)
		{
			gc->sState.sViewport.i32X = 0;
			gc->sState.sViewport.i32Y = 0;
			gc->sState.sViewport.ui32Width = sDrawParams.ui32Width;
			gc->sState.sViewport.ui32Height = sDrawParams.ui32Height;

			gc->sState.sScissor.i32ScissorX = 0;
			gc->sState.sScissor.i32ScissorY = 0;
			gc->sState.sScissor.ui32ScissorWidth = sDrawParams.ui32Width;
			gc->sState.sScissor.ui32ScissorHeight = sDrawParams.ui32Height;
		
			gc->sState.sScissor.i32ClampedWidth = (IMG_INT32)gc->sState.sScissor.ui32ScissorWidth;
			gc->sState.sScissor.i32ClampedHeight = (IMG_INT32)gc->sState.sScissor.ui32ScissorHeight;

			ApplyViewport(gc);
			ApplyDepthRange(gc, GLES1_Zero, GLES1_One);

			gc->bFullScreenScissor = IMG_TRUE;
			gc->bFullScreenViewport = IMG_TRUE;
			gc->bHasBeenCurrent = IMG_TRUE;
		}
	}

	return IMG_EGL_NO_ERROR;

BAD_CONTEXT:
	__GLES1_SET_CONTEXT(0);

	return IMG_EGL_NO_ERROR;
}


/***********************************************************************************
 Function Name      : GLESMakeUnCurrentGC
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Clear the thread reference to the current context
************************************************************************************/
static IMG_VOID GLESMakeUnCurrentGC(void)
{
	EGLRenderSurface *psSurface;

	__GLES1_GET_CONTEXT();

	/* Remove references to the default surface in all resource managers */
	psSurface = gc->sFrameBuffer.sDefaultFrameBuffer.sDrawParams.psRenderSurface;

	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->psTextureManager->sKRM, psSurface);
	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->sUSEShaderVariantKRM, psSurface);
	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->sPDSVariantKRM, psSurface);

	/* Get rid of texture and shader ghosts */
	KRM_DestroyUnneededGhosts(gc, &gc->psSharedState->psTextureManager->sKRM);
	KRM_DestroyUnneededGhosts(gc, &gc->psSharedState->sUSEShaderVariantKRM);

	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->sBufferObjectKRM, gc);


	/* If the currently bound surface is the default surface, break the linkage */
	if(gc->psRenderSurface == psSurface)
	{
		gc->psRenderSurface = IMG_NULL;
	}

	__GLES1_SET_CONTEXT(0);
}


/***********************************************************************************
 Function Name      : GLESFlushBuffersGC
 Inputs             : hContext, bNewExternalFrame, bWaitForHW
 Outputs            : -
 Returns            : Success
 Description        : Flush any buffered graphics commands
************************************************************************************/
static IMG_EGLERROR GLESFlushBuffersGC(EGLContextHandle hContext,
									   EGLRenderSurface *psSurface, 
									   IMG_BOOL bFlushAllSurfaces,
									   IMG_BOOL bNewExternalFrame,
									   IMG_BOOL bWaitForHW)
{
	GLES1Context *gc = (GLES1Context *)hContext;
	IMG_EGLERROR eError = IMG_EGL_NO_ERROR;
	EGLRenderSurface *psRenderSurface;

	GLES1_SWAP_TIME_START(GLES1_TIMER_SWAP_BUFFERS_TIME);

	if(psSurface)
	{
		psRenderSurface = psSurface;
	}
	else
	{
		psRenderSurface = gc->sFrameBuffer.sDefaultFrameBuffer.sDrawParams.psRenderSurface;
	}

#if defined(TIMING) || defined(DEBUG)
	GetFrameTime(gc);
#endif

	if(bFlushAllSurfaces)
	{
		if(!FlushAllUnflushedFBO(gc, bWaitForHW))
		{
			eError = IMG_EGL_GENERIC_ERROR;
		}
	}

	if(psRenderSurface)
	{
		IMG_UINT32 ui32Flags = 0;

		if (psRenderSurface->bInFrame)
		{
			ui32Flags |= GLES1_SCHEDULE_HW_LAST_IN_SCENE;

			GLES1_INC_COUNT(GLES1_TIMER_SWAP_BUFFERS_COUNT, 1);
		}

		if(bWaitForHW)
		{
			ui32Flags |= GLES1_SCHEDULE_HW_WAIT_FOR_3D;
		}

		if(psRenderSurface->bInFrame || bWaitForHW)
		{
			eError = ScheduleTA(gc, psRenderSurface, ui32Flags);
		}

		if((eError == IMG_EGL_NO_ERROR) && bNewExternalFrame)
		{
			psRenderSurface->bInExternalFrame = IMG_FALSE;
		}
	}

    GLES1_SWAP_TIME_STOP(GLES1_TIMER_SWAP_BUFFERS_TIME);

	return eError;
}

#if defined(ANDROID)
static IMG_EGLERROR GLESFlushBuffersGCNoContext(IMG_BOOL bNewExternalFrame,
												IMG_BOOL bWaitForHW)
{
	__GLES1_GET_CONTEXT_RETURN(IMG_EGL_GENERIC_ERROR);

	return GLESFlushBuffersGC(gc, IMG_NULL, IMG_TRUE, bNewExternalFrame, bWaitForHW);
}
#endif

/***********************************************************************************
 Function Name      : GLESMarkRenderSurfaceAsInvalid
 Inputs             : hContext
 Outputs            : -
 Returns            : -
 Description        : Indicates to OpenGLES that its current render surface pointer
					  is now invalid
************************************************************************************/
static IMG_VOID GLESMarkRenderSurfaceAsInvalid(EGLContextHandle hContext)
{
	GLES1Context *gc = (GLES1Context *)hContext;
	EGLRenderSurface *psSurface;

	psSurface = gc->sFrameBuffer.sDefaultFrameBuffer.sDrawParams.psRenderSurface;

	/* Discard the scene */
	ScheduleTA(gc, psSurface, GLES1_SCHEDULE_HW_DISCARD_SCENE|GLES1_SCHEDULE_HW_WAIT_FOR_3D);

	/* Remove references to the default surface in all resource managers */
	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->psTextureManager->sKRM, psSurface);
	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->sUSEShaderVariantKRM, psSurface);
	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->sPDSVariantKRM, psSurface);

	/* If the currently bound surface is the default surface, break the linkage */
	if(gc->psRenderSurface == psSurface)
	{
		gc->psRenderSurface = IMG_NULL;
	}
}


typedef void (IMG_CALLCONV *GL_PROC)(IMG_VOID);

#define COMPARE_AND_RETURN(func)	\
	if(!strcmp(procname, #func))	\
	{                 				\
		return (GL_PROC)&func;		\
	}

#define COMPARE_AND_RETURN_OPTION(string, string1, func)			\
	if(!strcmp(procname, #string) || !strcmp(procname, #string1)) { \
		return (GL_PROC)&func;										\
	}

/***********************************************************************************
 Function Name      : GLESGetProcAddress
 Inputs             : procname
 Outputs            : -
 Returns            : Pointer to function
 Description        : Returns the address of the named function
************************************************************************************/
static IMG_VOID (IMG_CALLCONV *GLESGetProcAddress(const char *procname))(IMG_VOID)
{
/***********************************************************
					Required extensions
 ***********************************************************/

	COMPARE_AND_RETURN(glPointSizePointerOES)


/***********************************************************
					Optional extensions
 ***********************************************************/

#if defined(GLES1_EXTENSION_MATRIX_PALETTE)

	COMPARE_AND_RETURN(glCurrentPaletteMatrixOES)
	COMPARE_AND_RETURN(glLoadPaletteFromModelViewMatrixOES)
	COMPARE_AND_RETURN(glMatrixIndexPointerOES)
	COMPARE_AND_RETURN(glWeightPointerOES)

#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */


#if defined(GLES1_EXTENSION_DRAW_TEXTURE)

	COMPARE_AND_RETURN(glDrawTexsOES)
	COMPARE_AND_RETURN(glDrawTexiOES)

#if defined(PROFILE_COMMON)

	COMPARE_AND_RETURN(glDrawTexfOES)

#endif /* PROFILE_COMMON */

	COMPARE_AND_RETURN(glDrawTexxOES)
	COMPARE_AND_RETURN(glDrawTexsvOES)
	COMPARE_AND_RETURN(glDrawTexivOES)

#if defined(PROFILE_COMMON)

	COMPARE_AND_RETURN(glDrawTexfvOES)

#endif /* PROFILE_COMMON */

	COMPARE_AND_RETURN(glDrawTexxvOES)

#endif /* defined(GLES1_EXTENSION_DRAW_TEXTURE) */


#if defined(GLES1_EXTENSION_QUERY_MATRIX)

	COMPARE_AND_RETURN(glQueryMatrixxOES)

#endif /* defined(GLES1_EXTENSION_QUERY_MATRIX) */


/***********************************************************
					Extension pack
 ***********************************************************/

#if defined(GLES1_EXTENSION_BLEND_SUBTRACT)

	COMPARE_AND_RETURN(glBlendEquationOES)

#endif /* defined(GLES1_EXTENSION_BLEND_SUBTRACT) */


#if defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE)

	COMPARE_AND_RETURN(glBlendEquationSeparateOES)

#endif /* defined(GLES1_EXTENSION_BLEND_EQ_SEPARATE) */


#if defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE)

	COMPARE_AND_RETURN(glBlendFuncSeparateOES)

#endif /* defined(GLES1_EXTENSION_BLEND_FUNC_SEPARATE) */


#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)

	COMPARE_AND_RETURN(glTexGeniOES)
	COMPARE_AND_RETURN(glTexGenivOES)

#if defined(PROFILE_COMMON)
	COMPARE_AND_RETURN(glTexGenfOES)
	COMPARE_AND_RETURN(glTexGenfvOES)
#endif /* PROFILE_COMMON */

	COMPARE_AND_RETURN(glTexGenxOES)
	COMPARE_AND_RETURN(glTexGenxvOES)
	COMPARE_AND_RETURN(glGetTexGenivOES)

#if defined(PROFILE_COMMON)
	COMPARE_AND_RETURN(glGetTexGenfvOES)
#endif /* PROFILE_COMMON */

	COMPARE_AND_RETURN(glGetTexGenxvOES)

#else /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */


#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

#if defined(GLES1_EXTENSION_FRAME_BUFFER_OBJECTS)

	COMPARE_AND_RETURN(glIsRenderbufferOES)
	COMPARE_AND_RETURN(glBindRenderbufferOES)
	COMPARE_AND_RETURN(glDeleteRenderbuffersOES)
	COMPARE_AND_RETURN(glGenRenderbuffersOES)
	COMPARE_AND_RETURN(glRenderbufferStorageOES)
	COMPARE_AND_RETURN(glGetRenderbufferParameterivOES)
	COMPARE_AND_RETURN(glIsFramebufferOES)
	COMPARE_AND_RETURN(glBindFramebufferOES)
	COMPARE_AND_RETURN(glDeleteFramebuffersOES)
	COMPARE_AND_RETURN(glGenFramebuffersOES)
	COMPARE_AND_RETURN(glCheckFramebufferStatusOES)
	COMPARE_AND_RETURN(glFramebufferTexture2DOES)
	COMPARE_AND_RETURN(glFramebufferRenderbufferOES)
	COMPARE_AND_RETURN(glGetFramebufferAttachmentParameterivOES)
	COMPARE_AND_RETURN(glGenerateMipmapOES)

#endif /* defined(GLES1_EXTENSION_FRAME_BUFFER_OBJECTS) */


/***********************************************************
					IMG extensions
 ***********************************************************/

#if defined(GLES1_EXTENSION_TEXTURE_STREAM)

	COMPARE_AND_RETURN(glGetTexStreamDeviceAttributeivIMG)
	COMPARE_AND_RETURN(glGetTexStreamDeviceNameIMG)
	COMPARE_AND_RETURN(glTexBindStreamIMG)

#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */

#if defined(GLES1_EXTENSION_MAP_BUFFER)

	COMPARE_AND_RETURN(glGetBufferPointervOES)
	COMPARE_AND_RETURN(glMapBufferOES)
	COMPARE_AND_RETURN(glUnmapBufferOES)

#endif /* defined(GLES1_EXTENSION_MAP_BUFFER) */

#if defined(GLES1_EXTENSION_MULTI_DRAW_ARRAYS)

	COMPARE_AND_RETURN_OPTION(glMultiDrawArrays, glMultiDrawArraysEXT, glMultiDrawArraysEXT)
	COMPARE_AND_RETURN_OPTION(glMultiDrawElements, glMultiDrawElementsEXT, glMultiDrawElementsEXT)

#endif

#if defined(GLES1_EXTENSION_EGL_IMAGE)
    COMPARE_AND_RETURN(glEGLImageTargetTexture2DOES)
    COMPARE_AND_RETURN(glEGLImageTargetRenderbufferStorageOES)
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */


#if defined(GLES1_EXTENSION_VERTEX_ARRAY_OBJECT)
	COMPARE_AND_RETURN(glBindVertexArrayOES)
	COMPARE_AND_RETURN(glDeleteVertexArraysOES)
	COMPARE_AND_RETURN(glGenVertexArraysOES)
	COMPARE_AND_RETURN(glIsVertexArrayOES)
#endif /* GLES1_EXTENSION_VERTEX_ARRAY_OBJECT */


	return IMG_NULL;

}


/***********************************************************************************
			Function table used by glGetString to pass the addresses
					of the GLES functions to IMGEGL
************************************************************************************/
IMG_INTERNAL const IMG_OGLES1EGL_Interface sGLES1FunctionTable =
{
	IMG_OGLES1_FUNCTION_TABLE_VERSION,

	GLESGetProcAddress,

	GLESCreateGC,
	GLESDestroyGC,
	GLESMakeCurrentGC,
	GLESMakeUnCurrentGC,
	GLESFlushBuffersGC,

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	GLESBindTexImage,
	GLESReleaseTexImage,
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

#if defined(EGL_EXTENSION_KHR_IMAGE)
	GLESGetImageSource,
#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

	GLESMarkRenderSurfaceAsInvalid,
#if defined(ANDROID)
	GLESFlushBuffersGCNoContext,
#endif

};


/******************************************************************************
 End of file (eglglue.c)
******************************************************************************/

