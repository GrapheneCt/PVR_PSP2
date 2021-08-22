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
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: eglglue.c $
 *****************************************************************************/

#include <string.h> /* For strcmp */
#include <ult.h>
#include <libsysmodule.h>
#include "context.h"
#include "eglapi.h"
#include "pvr_debug.h"
#include "osglue.h"
#include "pvrversion.h"
#include "gles2errata.h"

#include "psp2/swtexop.h"	
#include "psp2/libheap_custom.h"

#include "pds_mte_state_copy_sizeof.h"
#include "pds_aux_vtx_sizeof.h"

extern const IMG_CHAR * const pszRenderer;


/***********************************************************************************
 Function Name      : _GLES2CreateContext
 Inputs             : None
 Outputs            : None
 Returns            : GLES2Context*
 Description        : Create the GLES2Context context structure
************************************************************************************/
static GLES2Context* _GLES2CreateContext(IMG_VOID)
{
	return (GLES2Context *)GLES2Calloc(0, sizeof(GLES2Context));
}

/***********************************************************************************
 Function Name      : _GLES2DestroyContext
 Inputs             : gc
 Outputs            : None
 Returns            : None
 Description        : Destroy the GLES2Context context structure
************************************************************************************/
static IMG_VOID _GLES2DestroyContext(GLES2Context *gc)
{
	GLES2Free(0, gc);
}

/***********************************************************************************
 Function Name      : FreeContextSharedState
 Inputs             : gc
 Outputs            : -
 Returns            : Success.
 Description        : Frees the data structures that are shared between multiple contexts.
************************************************************************************/
static IMG_VOID FreeContextSharedState(GLES2Context *gc)
{
	IMG_UINT32 i;
	IMG_INT32 j;
	IMG_BOOL bDoFree = IMG_FALSE;
	GLES2ContextSharedState *psSharedState = gc->psSharedState;
	
	/* Frame buffer objects must be deleted first so that their attachments (renderbuffer/texture) 
	   can be removed safely */
	GLES2NameType aeNameType[GLES2_MAX_SHAREABLE_NAMETYPE] =
	{
		GLES2_NAMETYPE_FRAMEBUFFER, 
		GLES2_NAMETYPE_TEXOBJ, 
		GLES2_NAMETYPE_PROGRAM, 
		GLES2_NAMETYPE_BUFOBJ,
		GLES2_NAMETYPE_RENDERBUFFER
	};
	/* Silently ignore NULL */
	if(!psSharedState)
	{
		return;
	}

	/* *** CRITICAL SECTION *** */
	PVRSRVLockMutex(psSharedState->hPrimaryLock);
	
	GLES_ASSERT(psSharedState->ui32RefCount > 0);

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
		KRM_WaitForAllResources(&psSharedState->psTextureManager->sKRM,   GLES2_DEFAULT_WAIT_RETRIES);
		KRM_WaitForAllResources(&psSharedState->sUSEShaderVariantKRM, GLES2_DEFAULT_WAIT_RETRIES);


		/* Destroy all the shareable resources */
		for(i = 0; i < GLES2_MAX_SHAREABLE_NAMETYPE; ++i)
		{
			j = aeNameType[i];
			if (j < GLES2_MAX_SHAREABLE_NAMETYPE)
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


		if(psSharedState->psSequentialStaticIndicesMemInfo)
		{
			GLES2FREEDEVICEMEM(gc->ps3DDevData, psSharedState->psSequentialStaticIndicesMemInfo);
		}

		if(psSharedState->psLineStripStaticIndicesMemInfo)
		{
			GLES2FREEDEVICEMEM(gc->ps3DDevData, psSharedState->psLineStripStaticIndicesMemInfo);
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
		GLES2MemSet(psSharedState, 0, sizeof(GLES2ContextSharedState));

		GLES2Free(IMG_NULL, psSharedState);
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
static IMG_BOOL CreateSharedState(GLES2Context *gc, GLES2Context *psShareContext)
{
	GLES2ContextSharedState *psSharedState;
	GLES2NameType aeNameType[GLES2_MAX_SHAREABLE_NAMETYPE] =
	{
		GLES2_NAMETYPE_TEXOBJ, 
		GLES2_NAMETYPE_PROGRAM, 
		GLES2_NAMETYPE_BUFOBJ,
		GLES2_NAMETYPE_RENDERBUFFER, 
		GLES2_NAMETYPE_FRAMEBUFFER
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
			GLES_ASSERT(psShareContext->psSharedState);

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
			psSharedState = GLES2Calloc(gc, sizeof(GLES2ContextSharedState));

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

				GLES2Free(IMG_NULL, psSharedState);

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

				GLES2Free(IMG_NULL, psSharedState);

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

				GLES2Free(IMG_NULL, psSharedState);

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

				GLES2Free(IMG_NULL, psSharedState);

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
			psSharedState->psUSEVertexCodeHeap = UCH_CodeHeapCreate(gc->ps3DDevData, 
																	UCH_USE_CODE_HEAP_TYPE, 
																	gc->psSysContext->hUSEVertexHeap,
																	psSharedState->hSecondaryLock,
																	gc->psSysContext->hPerProcRef);

			if(!psSharedState->psUSEVertexCodeHeap)
			{
				PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: Failed to create USSE vertex code heap!\n"));

				FreeContextSharedState(gc);

				return IMG_FALSE;
			}

			psSharedState->psUSEFragmentCodeHeap = UCH_CodeHeapCreate(gc->ps3DDevData, 
																		UCH_USE_CODE_HEAP_TYPE, 
																		gc->psSysContext->hUSEFragmentHeap,
																		psSharedState->hSecondaryLock,
																		gc->psSysContext->hPerProcRef);

			if(!psSharedState->psUSEFragmentCodeHeap)
			{
				PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: Failed to create USSE fragment code heap!\n"));

				FreeContextSharedState(gc);

				return IMG_FALSE;
			}

			psSharedState->psPDSFragmentCodeHeap = UCH_CodeHeapCreate(gc->ps3DDevData, 
																		UCH_PDS_CODE_HEAP_TYPE, 
																		gc->psSysContext->hPDSFragmentHeap,
																		psSharedState->hSecondaryLock,
																		gc->psSysContext->hPerProcRef);

			if(!psSharedState->psPDSFragmentCodeHeap)
			{
				PVR_DPF((PVR_DBG_ERROR, "CreateSharedState: Failed to create PDS fragment code heap!\n"));

				FreeContextSharedState(gc);

				return IMG_FALSE;
			}

			/* Initialize the shareable names arrays */
			for(i = 0; i < GLES2_MAX_SHAREABLE_NAMETYPE; ++i)
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

	GLES_ASSERT(gc->psSharedState);

	return IMG_TRUE;
}


#if defined(DEBUG)
static FILE *OpenShaderAnalysisFile(void)
{
	#define SHADER_ANALYSIS_TXT "ux0:data/gles/shaderanalysis.txt"

	SceUID fd = sceIoDopen("ux0:data/gles");

	if (fd <= 0)
	{
		sceIoMkdir("ux0:data/gles", 0777);
	}
	else
	{
		sceIoDclose(fd);
	}

#if defined(SUPPORT_ANDROID_PLATFORM)
#define I_DIR				"/data/data/"
#define E_DIR				"/sdcard/Android/data/"
	char szPath[PATH_MAX], szCmdName[PATH_MAX], *pszCmdNameOffset;
	FILE *pFile, *pShaderAnalysisHandle;

	/* If we can't figure out the process name, we have to abort */
	pFile = fopen("/proc/self/cmdline", "r");
	if(!pFile)
		goto exit_out;

	if(!fgets(szCmdName, PATH_MAX, pFile))
		goto exit_out;

	pszCmdNameOffset = strrchr(szCmdName, '/') + 1;
	pszCmdNameOffset = (pszCmdNameOffset) ? szCmdName : pszCmdNameOffset + 1;
	fclose(pFile);

	/* Try internal memory first. No need for sdcard then. */

	snprintf(szPath, PATH_MAX - 1, I_DIR "%s/" SHADER_ANALYSIS_TXT, pszCmdNameOffset);
	szPath[PATH_MAX - 1] = 0;

	pShaderAnalysisHandle = fopen(szPath, "wt");
	if(pShaderAnalysisHandle)
		return pShaderAnalysisHandle;

	/* Doesn't exist or is non-writable; try external memory */

	snprintf(szPath, PATH_MAX - 1, E_DIR "%s/" SHADER_ANALYSIS_TXT, pszCmdNameOffset);
	szPath[PATH_MAX - 1] = 0;

	pShaderAnalysisHandle = fopen(szPath, "wt");
	if(pShaderAnalysisHandle)
		return pShaderAnalysisHandle;

	/* Nothing worked. Try the default location */
exit_out:
#undef I_DIR
#undef E_DIR
#endif /* defined(SUPPORT_ANDROID_PLATFORM) */

	return fopen(SHADER_ANALYSIS_TXT, "wt");
}
#undef SHADER_ANALYSIS_TXT
#endif /* defined(DEBUG) */


/***********************************************************************************
 Function Name      : InitContext
 Inputs             : gc, psShareContext, psMode
 Outputs            : -
 Returns            : Success
 Description        : Initialises a context - may setup shared name tables from a 
					  sharecontext. Also sets up default framebuffer
************************************************************************************/
static IMG_BOOL InitContext(GLES2Context *gc, GLES2Context *psShareContext, EGLcontextMode *psMode)
{
#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)
	GLES2NameType aeNameType[GLES2_MAX_UNSHAREABLE_NAMETYPE] =
	{
		GLES2_NAMETYPE_VERARROBJ
	};
#endif /* defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT) */

	IMG_UINT32 i, j;

	GLES_ASSERT(gc);
	
	GetApplicationHints(&gc->sAppHints, psMode);
	
#if defined(FIX_HW_BRN_26922)
	if(!AllocateBRN26922Mem(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: AllocateBRN26922Mem failed"));

		goto FAILED_AllocateBRN26922Mem;
	}
#endif /* defined(FIX_HW_BRN_26922) */

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
			"OGLES2UNCHeap",
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
			"OGLES2CDRAMHeap",
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
	gc->pvUltRuntimeWorkArea = GLES2Malloc(gc, ui32UltRuntimeWorkAreaSize);
	gc->pvUltRuntime = GLES2Malloc(gc, _SCE_ULT_ULTHREAD_RUNTIME_SIZE);
	gc->pvUltThreadStorage = GLES2Malloc(gc, 4 * gc->sAppHints.ui32SwTexOpMaxUltNum);
	GLES2MemSet(gc->pvUltThreadStorage, 0, 4 * gc->sAppHints.ui32SwTexOpMaxUltNum);

	SceUltUlthreadRuntimeOptParam sUltOptParam;
	sceUltUlthreadRuntimeOptParamInitialize(&sUltOptParam);
	sUltOptParam.oneShotThreadStackSize = 4 * 1024 + (4 * 1024 * (IMG_UINT32)((IMG_FLOAT)gc->sAppHints.ui32SwTexOpMaxUltNum / 4.0f));
	sUltOptParam.workerThreadAttr = 0;
	sUltOptParam.workerThreadCpuAffinityMask = gc->sAppHints.ui32SwTexOpThreadAffinity;
	sUltOptParam.workerThreadOptParam = 0;
	sUltOptParam.workerThreadPriority = gc->sAppHints.ui32SwTexOpThreadPriority;

	i = sceUltUlthreadRuntimeCreate(gc->pvUltRuntime,
		"OGLES2UltRuntime",
		gc->sAppHints.ui32SwTexOpMaxUltNum,
		gc->sAppHints.ui32SwTexOpThreadNum,
		gc->pvUltRuntimeWorkArea,
		&sUltOptParam);

	if (i != SCE_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "InitContext: Couldn't create ULT runtime: 0x%X", i));

		goto FAILED_sceUltUlthreadRuntimeCreate;
	}

	gc->bSwTexOpFin = IMG_FALSE;
	gc->hSwTexOpThrd = sceKernelCreateThread("OGLES2AsyncTexOpCl", texOpAsyncCleanupThread, SCE_KERNEL_LOWEST_PRIORITY_USER, SCE_KERNEL_4KiB, 0, 0, SCE_NULL);
	sceKernelStartThread(gc->hSwTexOpThrd, 4, &gc);

#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)
	/* Initialize the unshareable names arrays */
	for (i = 0; i < GLES2_MAX_UNSHAREABLE_NAMETYPE; i++)
	{
		gc->apsNamesArray[i] = CreateNamesArray(gc, aeNameType[i], (PVRSRV_MUTEX_HANDLE)0);

		if(!gc->apsNamesArray[i])
		{
			PVR_DPF((PVR_DBG_ERROR,"InitContext: Couldn't create unshareable names array %d", i));

			DestroyNamesArray(gc, gc->apsNamesArray[i]);

			goto FAILED_TASync;
		}
	}
#endif /* defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT) */

	if(GLES2ALLOCDEVICEMEM(gc->ps3DDevData, 
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

	if(!HashTableCreate(gc, &gc->sProgram.sPDSFragmentVariantHashTable, STATEHASH_LOG2TABLESIZE, STATEHASH_MAXNUMENTRIES, DestroyHashedPDSVariant))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: HashTableCreate failed"));

		goto FAILED_CreateHashTable;

	}

	if(!CreateTextureState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: CreateTextureState failed"));

		goto FAILED_CreateTextureState;
	}

	if(!CreateProgramState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: CreateProgramState failed"));

		goto FAILED_CreateProgramState;
	}

	if(!CreateBufObjState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: CreateBufObjState failed"));

		goto FAILED_CreateBufObjState;
	}

	if(!CreateFrameBufferState(gc, psMode))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: CreateRenderBufferState failed"));

		goto FAILED_CreateFrameBufferState;
	}

	if(!CreateVertexArrayObjectState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: CreateVertexArrayObjectState failed"));

		goto FAILED_CreateVertexArrayObjectState;
	}

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

	if(!InitSpecialUSECodeBlocks(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"InitContext: InitSpecialUSECodeBlocks failed"));

		goto FAILED_InitSpecialUSECodeBlocks;
	}

	gc->sState.sRaster.ui32ColorMask = GLES2_COLORMASK_ALL;

	gc->sState.sRaster.sClearColor.fRed   = GLES2_Zero;
	gc->sState.sRaster.sClearColor.fGreen = GLES2_Zero;
	gc->sState.sRaster.sClearColor.fBlue  = GLES2_Zero;
	gc->sState.sRaster.sClearColor.fAlpha = GLES2_Zero;

	gc->sState.sRaster.ui32BlendFactor = ((GLES2_BLENDFACTOR_ONE << GLES2_BLENDFACTOR_RGBSRC_SHIFT)		|
											(GLES2_BLENDFACTOR_ONE << GLES2_BLENDFACTOR_ALPHASRC_SHIFT) |
											(GLES2_BLENDFACTOR_ZERO << GLES2_BLENDFACTOR_RGBDST_SHIFT)	|
											(GLES2_BLENDFACTOR_ZERO << GLES2_BLENDFACTOR_ALPHADST_SHIFT));

	gc->sState.sRaster.ui32BlendEquation =	(GLES2_BLENDFUNC_ADD << GLES2_BLENDFUNC_RGB_SHIFT) | 
											(GLES2_BLENDFUNC_ADD << GLES2_BLENDFUNC_ALPHA_SHIFT);

	gc->sState.sRaster.sBlendColor.fRed   = GLES2_Zero;
	gc->sState.sRaster.sBlendColor.fGreen = GLES2_Zero;
	gc->sState.sRaster.sBlendColor.fBlue  = GLES2_Zero;
	gc->sState.sRaster.sBlendColor.fAlpha = GLES2_Zero;

	gc->sState.sLine.fWidth = GLES2_One;

	gc->sState.sPolygon.eCullMode = GL_BACK;
	gc->sState.sPolygon.eFrontFaceDirection = GL_CCW;
	gc->sState.sPolygon.factor.fVal = GLES2_Zero;
	gc->sState.sPolygon.fUnits = GLES2_Zero;

	gc->sState.sStencil.ui32FFStencil = EURASIA_ISPC_SCMP_ALWAYS	|
										EURASIA_ISPC_SOP1_KEEP		|
										EURASIA_ISPC_SOP2_KEEP		|
										EURASIA_ISPC_SOP3_KEEP		|
										(GLES2_MAX_STENCIL_VALUE << EURASIA_ISPC_SCMPMASK_SHIFT) |
										(GLES2_MAX_STENCIL_VALUE << EURASIA_ISPC_SWMASK_SHIFT);
	
	gc->sState.sStencil.ui32BFStencil = EURASIA_ISPC_SCMP_ALWAYS	|
										EURASIA_ISPC_SOP1_KEEP		|
										EURASIA_ISPC_SOP2_KEEP		|
										EURASIA_ISPC_SOP3_KEEP		|
										(GLES2_MAX_STENCIL_VALUE << EURASIA_ISPC_SCMPMASK_SHIFT) |
										(GLES2_MAX_STENCIL_VALUE << EURASIA_ISPC_SWMASK_SHIFT);

	gc->sState.sStencil.ui32FFStencilRef = (0 << EURASIA_ISPA_SREF_SHIFT);

	gc->sState.sStencil.ui32BFStencilRef = (0 << EURASIA_ISPA_SREF_SHIFT);
	gc->sState.sStencil.ui32Clear = 0;

	gc->sState.sStencil.ui32FFStencilCompareMaskIn = GLES2_MAX_STENCIL_VALUE;
	gc->sState.sStencil.ui32BFStencilCompareMaskIn = GLES2_MAX_STENCIL_VALUE;

	gc->sState.sStencil.ui32FFStencilWriteMaskIn = GLES2_MAX_STENCIL_VALUE;
	gc->sState.sStencil.ui32BFStencilWriteMaskIn = GLES2_MAX_STENCIL_VALUE;

	gc->sState.sStencil.i32FFStencilRefIn = 0;
	gc->sState.sStencil.i32BFStencilRefIn = 0;

	gc->sState.sStencil.ui32MaxFBOStencilVal = GLES2_MAX_STENCIL_VALUE;

	gc->sState.sMultisample.bSampleCoverageInvert = IMG_FALSE;
	gc->sState.sMultisample.fSampleCoverageValue = GLES2_One;

	gc->sState.sDepth.ui32TestFunc = EURASIA_ISPA_DCMPMODE_LT;
	gc->sState.sDepth.fClear = GLES2_One;

	gc->ui32Enables = GLES2_DITHER_ENABLE;

	gc->sState.sClientPixel.ui32UnpackAlignment = 4;
	gc->sState.sClientPixel.ui32PackAlignment = 4;

	gc->ui32DirtyState = GLES2_DIRTYFLAG_ALL;
	gc->ui32EmitMask   = GLES2_EMITSTATE_ALL;

	/*
	 * Set up attrib array default values
	 */
	for (i=0; i < GLES2_MAX_VERTEX_ATTRIBS; ++i) 
	{
		GLES2AttribArrayPointerState *psAPState = &(gc->sVAOMachine.sDefaultVAO.asVAOState[AP_VERTEX_ATTRIB0 + i]);

		psAPState->ui32StreamTypeSize = GLES2_STREAMTYPE_FLOAT | (4 << GLES2_STREAMSIZE_SHIFT);

		gc->sVAOMachine.asAttribPointer[AP_VERTEX_ATTRIB0 + i].psState = psAPState;
		gc->sVAOMachine.asAttribPointer[AP_VERTEX_ATTRIB0 + i].ui32Size = 16;
		gc->sVAOMachine.asAttribPointer[AP_VERTEX_ATTRIB0 + i].ui32Stride = 16;

		gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + i].fX = GLES2_Zero;
		gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + i].fY = GLES2_Zero;
		gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + i].fZ = GLES2_Zero;
		gc->sVAOMachine.asCurrentAttrib[AP_VERTEX_ATTRIB0 + i].fW = GLES2_One;
	}

	for(i=0; i < GLES2_HINT_NUMHINTS; i++)
	{
		gc->sState.sHints.eHint[i] = GL_DONT_CARE;
	}

	j = 0;
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
				PVR_DPF((PVR_DBG_MESSAGE,"InitContext: ignoring buffer type CBUF_TYPE_PDS_VERT_SECONDARY_PREGEN_BUFFER"));

				hMemHeap = gc->psSysContext->hPDSVertexHeap;
				ui32Size = 4;

				break;
			}
			case CBUF_TYPE_MTE_COPY_PREGEN_BUFFER:
			{
				hMemHeap = gc->psSysContext->hPDSVertexHeap;

				if(gc->sAppHints.bEnableStaticMTECopy)
				{
					ui32Size = ((gc->sAppHints.ui32DefaultPregenMTECopyBufferSize / GLES2_ALIGNED_MTE_COPY_PROG_SIZE) * GLES2_ALIGNED_MTE_COPY_PROG_SIZE);
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

		gc->apsBuffers[i] = CBUF_CreateBuffer(gc->ps3DDevData, i, hMemHeap, gc->psSysContext->hSyncInfoHeap, gc->psSysContext->sHWInfo.sMiscInfo.hOSGlobalEvent, ui32Size, gc->psSysContext->hPerProcRef);
	
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
				if(SetupMTEPregenBuffer(gc) != GLES2_NO_ERROR)
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

	if(BuildExtensionString(gc) != IMG_TRUE)
	{
		goto FAILED_BuildExtensionString;
	}


#if defined(SGX_FEATURE_SW_VDM_CONTEXT_SWITCH)

	gc->sSmallKickTA.ui32NumIndicesThisTA   	= 0;
	gc->sSmallKickTA.ui32NumPrimitivesThisTA	= 0;
	gc->sSmallKickTA.ui32NumIndicesThisFrame	= 0;
	gc->sSmallKickTA.ui32NumPrimitivesThisFrame	= 0;

	/* Default value to ensure no TA splitting on the first frame */
	gc->sSmallKickTA.ui32KickThreshold = 0xFFFFFFFF;

#endif

#if defined(DEBUG)
	if(gc->sAppHints.bDumpShaderAnalysis)
	{
		gc->pShaderAnalysisHandle = OpenShaderAnalysisFile();
	}
#endif /* defined(DEBUG) */

	PDUMP_STRING_CONTINUOUS((gc, "PDump from OpenGL ES 2.0 driver, version %s. HW variant %s, %d\n",PVRVERSION_STRING, pszRenderer, SGX_CORE_REV));

	return IMG_TRUE;


	/* Clean up any memory */

FAILED_BuildExtensionString:

FAILED_CBUF_CreateBuffer:
	for(i=0; i < CBUF_NUM_TA_BUFFERS; i++)
	{
		if(gc->apsBuffers[i])
		{
			CBUF_DestroyBuffer(gc->ps3DDevData, gc->apsBuffers[i]);
		}
	}

FAILED_InitSpecialUSECodeBlocks:

	FreeVertexArrayObjectState(gc);

FAILED_CreateVertexArrayObjectState:

	FreeFrameBufferState(gc);

FAILED_CreateFrameBufferState:

	FreeBufObjState(gc);

FAILED_CreateBufObjState:

	FreeProgramState(gc);

FAILED_CreateProgramState:

	FreeTextureState(gc);

FAILED_CreateTextureState:

	HashTableDestroy(gc, &gc->sProgram.sPDSFragmentVariantHashTable);

FAILED_CreateHashTable:

	GLES2FREEDEVICEMEM(gc->ps3DDevData, gc->sKRMTAStatusUpdate.psMemInfo);

FAILED_TASync:

	FreeContextSharedState(gc);

	GLES2Free(IMG_NULL, gc->pvUltThreadStorage);
	sceUltUlthreadRuntimeDestroy((SceUltUlthreadRuntime *)gc->pvUltRuntime);
	GLES2Free(IMG_NULL, gc->pvUltRuntimeWorkArea);
	GLES2Free(IMG_NULL, gc->pvUltRuntime);

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

#if defined(FIX_HW_BRN_26922)

	FreeBRN26922Mem(gc);

FAILED_AllocateBRN26922Mem:

#endif /* defined(FIX_HW_BRN_26922) */

	return IMG_FALSE;
}	


/***********************************************************************************
 Function Name      : DeInitContext
 Inputs             : gc
 Outputs            : 
 Returns            : 
 Description        : Destroys the given context. 
************************************************************************************/
static IMG_BOOL DeInitContext(GLES2Context *gc)
{
	IMG_UINT32 i;
	IMG_BOOL bPass = IMG_TRUE;

	if(!gc->psSharedState)
	{
		/* If there's no shared state then the rest of the context hasn't been initialised */
		return IMG_TRUE;
	}

#if defined(DEBUG)
	if(gc->pShaderAnalysisHandle)
		fclose(gc->pShaderAnalysisHandle);
#endif
	
	/* Free vao state */
	FreeVertexArrayObjectState(gc);

	/* Wait all the VAO attached resources */ 
	KRM_WaitForAllResources(&gc->sVAOKRM, GLES2_DEFAULT_WAIT_RETRIES);

#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)
	/* Destroy all the unshareable resources: currently only VAO attached resources */
	for (i = 0; i < GLES2_MAX_UNSHAREABLE_NAMETYPE; i++)
	{
		if(gc->apsNamesArray[i])
		{
			DestroyNamesArray(gc, gc->apsNamesArray[i]);
		}
	}	
#endif /* defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT) */

	/* Destroy the TA kick VAO manager, 
	   this must be after Destroy unshareable name arrays (vao name array in this case) */
	KRM_Destroy(gc, &gc->sVAOKRM);

	HashTableDestroy(gc, &gc->sProgram.sPDSFragmentVariantHashTable);

	if(!FreeTextureState(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"DeInitContext: FreeTextureState failed"));
		bPass = IMG_FALSE;
	}

	FreeProgramState(gc);

	FreeBufObjState(gc);

	/* FrameBuffers must be freed _after_ freeing textures due to FBOs. See FreeTexture() */
	FreeFrameBufferState(gc);

	FreeContextSharedState(gc);

	/* TexStreamsState must be freed after freeing all the default and named textures, 
	   which might refer to stream buffer devices */
#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
	if(FreeTexStreamState(gc) != PVRSRV_OK)
	{
		PVR_DPF((PVR_DBG_ERROR,"DeInitContext: FreeTexStreamState failed"));

		bPass = IMG_FALSE;
	}
#endif

	GLES2FREEDEVICEMEM(gc->ps3DDevData, gc->sKRMTAStatusUpdate.psMemInfo);

#if defined(FIX_HW_BRN_26922)
	FreeBRN26922Mem(gc);
#endif /* defined(FIX_HW_BRN_26922) */

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

	/* The UniPatch context must be freed _after_ the shared state is freed because the latter
	   may try to free shaders and that requires a working UniPatch context.
	*/
	PVRUniPatchDestroyContext(gc->sProgram.pvUniPatchContext);
	gc->sProgram.pvUniPatchContext = IMG_NULL;

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

	GLES2Free(IMG_NULL, gc->pvUltThreadStorage);
	sceUltUlthreadRuntimeDestroy((SceUltUlthreadRuntime *)gc->pvUltRuntime);
	GLES2Free(IMG_NULL, gc->pvUltRuntimeWorkArea);
	GLES2Free(IMG_NULL, gc->pvUltRuntime);

	return bPass;
}	


/***********************************************************************************
 Function Name      : GLES2CreateGC
 Inputs             : psSysContext, psMode, hShareContext
 Outputs            : phContext
 Returns            : Success
 Description        : Called to create an graphics context
 ************************************************************************************/
static IMG_BOOL GLES2CreateGC(SrvSysContext *psSysContext,
							  EGLContextHandle *phContext,
							  EGLcontextMode *psMode,
							  EGLContextHandle hShareContext)
{
	GLES2Context *gc, *psShareContext;

	psShareContext = (GLES2Context *)hShareContext;

	gc = _GLES2CreateContext();

	if (!gc)
	{
		PVR_DPF((PVR_DBG_ERROR,"GLES2CreateGC: Can't alloc memory for the gc"));

		return IMG_FALSE;
	}

	gc->psSysContext = psSysContext;
	gc->ps3DDevData  = &psSysContext->s3D;
	
	if (!InitContext(gc, psShareContext, psMode))
	{
		PVR_DPF((PVR_DBG_ERROR,"GLES2CreateGC: Failed to init the gc"));

		goto Failed_GC_Creation;
	}

	//TODOPSP2: hi again Rinne!
	IMG_PVOID pvDummy = GLES2Malloc(0, 4);
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
	GLES2Free(IMG_NULL, pvDummy);

#if defined (TIMING) || defined (DEBUG)
	if (!InitMetrics(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"GLES2CreateGC: Metrics init failed"));

		goto Failed_GC_Creation;
	}

	InitProfileData(gc);
#endif

	*phContext = (EGLContextHandle)gc;

	return IMG_TRUE;


Failed_GC_Creation:

	DeInitContext(gc);

	_GLES2DestroyContext(gc);

	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : GLES2DestroyGC
 Inputs             : hContext
 Outputs            : -
 Returns            : Success
 Description        : Called to destroy a graphics context
************************************************************************************/
static IMG_BOOL GLES2DestroyGC(EGLContextHandle hContext)	
{
	GLES2Context *gc = (GLES2Context *)hContext;
	IMG_BOOL bReturnValue = IMG_TRUE;

	if (!DeInitContext(gc))
	{
		PVR_DPF((PVR_DBG_ERROR,"GLES2DestroyGC: Failed to deinit the gc"));

		bReturnValue = IMG_FALSE;
	}

	DestroyExtensionString(gc);

	_GLES2DestroyContext(gc);

	return bReturnValue;
}


/***********************************************************************************
 Function Name      : GLES2MakeCurrentGC
 Inputs             : psWriteDrawable, psReadDrawable, hContext
 Outputs            : -
 Returns            : Success
 Description        : Make a read/write drawable current on a GC
************************************************************************************/
static IMG_EGLERROR GLES2MakeCurrentGC(EGLRenderSurface *psWriteDrawable,
										EGLRenderSurface *psReadDrawable,
										EGLContextHandle hContext)
{
	GLES2Context *gc = (GLES2Context *)hContext;

	__GLES2_SET_CONTEXT(gc);

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
				PVR_DPF((PVR_DBG_ERROR,"GLES2MakeCurrentGC: Invalid drawable - what do we do?"));

				goto BAD_CONTEXT;
			}

			if(psReadDrawable)
			{
				if (!KEGLGetDrawableParameters(psReadDrawable->hEGLSurface, &sReadParams, IMG_TRUE))
				{
					PVR_DPF((PVR_DBG_ERROR,"GLES2MakeCurrentGC: Invalid drawable - what do we do?"));

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
				PVR_DPF((PVR_DBG_ERROR,"GLES2MakeCurrentGC: Invalid drawable - what do we do?"));

				goto BAD_CONTEXT;
			}
		}

		if((psWriteDrawable && (sDrawParams.ui32Width == 0 || sDrawParams.ui32Height == 0)) ||
			(psReadDrawable && (sReadParams.ui32Width == 0 || sReadParams.ui32Height == 0)))
		{
			PVR_DPF((PVR_DBG_ERROR,"GLES2MakeCurrentGC: Invalid drawable - what do we do?"));

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
			ApplyDepthRange(gc, GLES2_Zero, GLES2_One);

			gc->bFullScreenScissor = IMG_TRUE;
			gc->bFullScreenViewport = IMG_TRUE;
			gc->bHasBeenCurrent = IMG_TRUE;
		}
	}

	return IMG_EGL_NO_ERROR;

BAD_CONTEXT:
	__GLES2_SET_CONTEXT(0);

	return IMG_EGL_NO_ERROR;
}


/***********************************************************************************
 Function Name      : GLES2MakeUnCurrentGC
 Inputs             : -
 Outputs            : -
 Returns            : -
 Description        : Clear the thread reference to the current context
************************************************************************************/
static IMG_VOID GLES2MakeUnCurrentGC(void)
{
	EGLRenderSurface *psSurface;

	__GLES2_GET_CONTEXT();

	/* Remove references to the default surface in all resource managers */
	psSurface = gc->sFrameBuffer.sDefaultFrameBuffer.sDrawParams.psRenderSurface;
	
	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->psTextureManager->sKRM, psSurface);
	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->sUSEShaderVariantKRM, psSurface);

	/* Get rid of texture and shader ghosts */
	KRM_DestroyUnneededGhosts(gc, &gc->psSharedState->psTextureManager->sKRM);
	KRM_DestroyUnneededGhosts(gc, &gc->psSharedState->sUSEShaderVariantKRM);

	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->sBufferObjectKRM, gc);

	/* If the currently bound surface is the default surface, break the linkage */
	if(gc->psRenderSurface == psSurface)
	{
		gc->psRenderSurface = IMG_NULL;
	}

	__GLES2_SET_CONTEXT(0);
}


/***********************************************************************************
 Function Name      : GLES2FlushBuffersGC
 Inputs             : hContext, psSurface, bFlushAllSurfaces, bNewExternalFrame, bWaitForHW
 Outputs            : -
 Returns            : Success
 Description        : Flush any buffered graphics commands
************************************************************************************/
static IMG_EGLERROR GLES2FlushBuffersGC(EGLContextHandle hContext,
										 EGLRenderSurface *psSurface,
										 IMG_BOOL bFlushAllSurfaces,
										 IMG_BOOL bNewExternalFrame,
										 IMG_BOOL bWaitForHW)
{
	GLES2Context *gc = (GLES2Context *)hContext;
	IMG_EGLERROR eError = IMG_EGL_NO_ERROR;
	EGLRenderSurface *psRenderSurface;
	
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
			ui32Flags |= GLES2_SCHEDULE_HW_LAST_IN_SCENE;
			GLES2_INC_COUNT(GLES2_TIMER_SWAP_BUFFERS, 1);
		}

		if(bWaitForHW)
		{
			ui32Flags |= GLES2_SCHEDULE_HW_WAIT_FOR_3D;
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

	return eError;
}

/***********************************************************************************
 Function Name      : GLES2MarkRenderSurfaceAsInvalid
 Inputs             : hContext
 Outputs            : -
 Returns            : -
 Description        : Indicates to OpenGLES2 that its current render surface pointer
					  is now invalid
************************************************************************************/
static IMG_VOID GLES2MarkRenderSurfaceAsInvalid(EGLContextHandle hContext)
{
	GLES2Context *gc = (GLES2Context *)hContext;
	EGLRenderSurface *psSurface;

	psSurface = gc->sFrameBuffer.sDefaultFrameBuffer.sDrawParams.psRenderSurface;
	
	/* Discard the scene */
	ScheduleTA(gc, psSurface, GLES2_SCHEDULE_HW_DISCARD_SCENE|GLES2_SCHEDULE_HW_WAIT_FOR_3D);

	/* Remove references to the default surface in all resource managers */
	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->psTextureManager->sKRM, psSurface);
	KRM_RemoveAttachmentPointReferences(&gc->psSharedState->sUSEShaderVariantKRM, psSurface);
	
	/* If the currently bound surface is the default surface, break the linkage */
	if(gc->psRenderSurface == psSurface)
	{
		gc->psRenderSurface = IMG_NULL;
	}
}

#if defined(EGL_EXTENSION_KHR_IMAGE)
extern IMG_EGLERROR GLESGetImageSource(EGLContextHandle hContext, IMG_UINT32 ui32Source, IMG_UINT32 ui32Name, IMG_UINT32 ui32Level, EGLImage *psEGLImage);
#endif

/***********************************************************************************
 Function Name      : GLES2GetProcAddress
 Inputs             : procname
 Outputs            : -
 Returns            : Pointer to function
 Description        : Returns the address of the named function
************************************************************************************/
typedef IMG_VOID (IMG_CALLCONV *GetProcAddressReturnFunc)(IMG_VOID);

#define COMPARE_AND_RETURN(func)                   \
	if(!strcmp(procname, #func)) {                 \
		return (GetProcAddressReturnFunc)&func;    \
	}

#define COMPARE_AND_RETURN_OPTION(string, string1, func)			\
	if(!strcmp(procname, #string) || !strcmp(procname, #string1)) { \
		return (GetProcAddressReturnFunc)&func;						\
	}


static GetProcAddressReturnFunc GLES2GetProcAddress(const char *procname)
{
	/* Note that this is O(n) with a very small n */

	COMPARE_AND_RETURN(glMapBufferOES)
	COMPARE_AND_RETURN(glUnmapBufferOES)
	COMPARE_AND_RETURN(glGetBufferPointervOES)

#if defined(GLES2_EXTENSION_EGL_IMAGE)

    COMPARE_AND_RETURN(glEGLImageTargetTexture2DOES)
    COMPARE_AND_RETURN(glEGLImageTargetRenderbufferStorageOES)

#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

#if defined(GLES2_EXTENSION_MULTI_DRAW_ARRAYS)

	COMPARE_AND_RETURN_OPTION(glMultiDrawArrays, glMultiDrawArraysEXT, glMultiDrawArraysEXT)
	COMPARE_AND_RETURN_OPTION(glMultiDrawElements, glMultiDrawElementsEXT, glMultiDrawElementsEXT)

#endif /*defined(GLES2_EXTENSION_MULTI_DRAW_ARRAYS) */

#if defined(GLES2_EXTENSION_TEXTURE_STREAM)

	COMPARE_AND_RETURN(glGetTexStreamDeviceAttributeivIMG)
	COMPARE_AND_RETURN(glGetTexStreamDeviceNameIMG)
	COMPARE_AND_RETURN(glTexBindStreamIMG)

#endif /* #if defined(GLES2_EXTENSION_TEXTURE_STREAM) */

#if defined(GLES2_EXTENSION_GET_PROGRAM_BINARY)
	COMPARE_AND_RETURN(glGetProgramBinaryOES)
	COMPARE_AND_RETURN(glProgramBinaryOES)
#endif /* GLES2_EXTENSION_GET_PROGRAM_BINARY */

#if defined(GLES2_EXTENSION_VERTEX_ARRAY_OBJECT)
	COMPARE_AND_RETURN(glBindVertexArrayOES)
	COMPARE_AND_RETURN(glDeleteVertexArraysOES)
	COMPARE_AND_RETURN(glGenVertexArraysOES)
	COMPARE_AND_RETURN(glIsVertexArrayOES)
#endif /* GLES2_EXTENSION_VERTEX_ARRAY_OBJECT */

#if defined(GLES2_EXTENSION_DISCARD_FRAMEBUFFER)
	COMPARE_AND_RETURN(glDiscardFramebufferEXT)
#endif /* GLES2_EXTENSION_DISCARD_FRAMEBUFFER */

#if defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE)
	COMPARE_AND_RETURN(glRenderbufferStorageMultisampleIMG)
	COMPARE_AND_RETURN(glFramebufferTexture2DMultisampleIMG)
#endif /* defined(GLES2_EXTENSION_MULTISAMPLED_RENDER_TO_TEXTURE) */

#undef COMPARE_AND_RETURN

	return IMG_NULL;
}	


/***********************************************************************************
			Function table used by glGetString to pass the addresses
					of the GLES2 functions to IMGEGL
************************************************************************************/
IMG_INTERNAL const IMG_OGLES2EGL_Interface sGLES2FunctionTable =
{
	IMG_OGLES2_FUNCTION_TABLE_VERSION,

	GLES2GetProcAddress,

	GLES2CreateGC,
	GLES2DestroyGC,
	GLES2MakeCurrentGC,
	GLES2MakeUnCurrentGC,
	GLES2FlushBuffersGC,

#if defined(EGL_EXTENSION_KHR_IMAGE)
	GLESGetImageSource,
#endif /* defined(EGL_EXTENSION_KHR_IMAGE) */

	GLES2MarkRenderSurfaceAsInvalid,

#if defined(EGL_EXTENSION_ANDROID_BLOB_CACHE) 

#endif

};


/******************************************************************************
 End of file (eglglue.c)
******************************************************************************/

