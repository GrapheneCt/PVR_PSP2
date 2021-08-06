
#include <kernel.h>
#include <ult.h>

#include "..\context.h"
#include "swtexop.h"

int32_t sceKernelAtomicAddAndGet32(volatile int32_t* ptr, int32_t value);

static IMG_VOID *pvAsDstPtr[1024];
static IMG_UINT32 ui32AsDstReqTime[1024];

static IMG_INT32 _SWTextureUploadEntry(IMG_UINT32 arg)
{
	SWTexUploadArg *psArg = (SWTexUploadArg *)arg;

	psArg->psTex->pfnTextureTwiddle(psArg->pvDest, psArg->pvSrc, psArg->ui32Width,
		psArg->ui32Height, psArg->ui32StrideIn);

	sceKernelAtomicAddAndGet32(&psArg->gc->ui32AsyncTexOpNum, -1);
	PVRSRVModifyCompleteSyncOps(psArg->gc->psSysContext->psConnection, psArg->hOpSyncObj);
	PVRSRVDestroySyncInfoModObj(psArg->gc->psSysContext->psConnection, psArg->hOpSyncObj);

	GLES1Free(psArg->gc, psArg);
	SceUltUlthread *psSelf;
	sceUltUlthreadGetSelf(&psSelf);
	GLES1Free(0, psSelf);

	return 0;
}

static IMG_INT32 _SWTextureMipGenEntry(IMG_UINT32 arg)
{
	SWTexMipGenArg *psArg = (SWTexMipGenArg *)arg;

	MakeTextureMipmapLevels(psArg->gc, psArg->psTex, psArg->ui32Face);

	sceKernelAtomicAddAndGet32(&psArg->gc->ui32AsyncTexOpNum, -1);
	if (psArg->psSyncInfo)
	{
		PVRSRVModifyCompleteSyncOps(psArg->gc->psSysContext->psConnection, psArg->hOpSyncObj);
		PVRSRVDestroySyncInfoModObj(psArg->gc->psSysContext->psConnection, psArg->hOpSyncObj);
	}

	GLES1Free(psArg->gc, psArg);
	SceUltUlthread *psSelf;
	sceUltUlthreadGetSelf(&psSelf);
	GLES1Free(0, psSelf);

	return 0;
}

IMG_INTERNAL IMG_VOID SWTextureUpload(
	GLES1Context *gc, GLESTexture *psTex, IMG_VOID *pvDest,
	const IMG_VOID	*pvSrc, IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
	IMG_UINT32 ui32StrideIn)
{
	PVRSRV_ERROR eResult;
	IMG_PVOID arg = GLES1Malloc(gc, sizeof(SWTexUploadArg));
	SWTexUploadArg *psArg = (SWTexUploadArg *)arg;
	psArg->gc = gc;
	psArg->psTex = psTex;
	psArg->pvDest = pvDest;
	psArg->pvSrc = pvSrc;
	psArg->ui32Height = ui32Height;
	psArg->ui32StrideIn = ui32StrideIn;
	psArg->ui32Width = ui32Width;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if (psTex->psEGLImageTarget)
	{
		psArg->psSyncInfo = psTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
	}
	else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
	{
		psArg->psSyncInfo = psTex->psMemInfo->psClientSyncInfo;
	}

	do
	{
		eResult = SGX2DQueryBlitsComplete(gc->ps3DDevData, psArg->psSyncInfo, IMG_TRUE);
	} while (eResult == PVRSRV_ERROR_TIMEOUT);

	PVRSRVCreateSyncInfoModObj(gc->psSysContext->psConnection, &psArg->hOpSyncObj);

	PVRSRVModifyPendingSyncOps(
		gc->psSysContext->psConnection,
		psArg->hOpSyncObj,
		&psArg->psSyncInfo,
		1,
		PVRSRV_MODIFYSYNCOPS_FLAGS_WO_INC,
		IMG_NULL,
		IMG_NULL);

	sceKernelAtomicAddAndGet32(&gc->ui32AsyncTexOpNum, 1);

	sceUltUlthreadCreate(
		GLES1Malloc(gc, _SCE_ULT_ULTHREAD_SIZE),
		"OGLES1SWTextureUpload",
		_SWTextureUploadEntry,
		(IMG_UINT32)arg,
		SCE_NULL,
		0,
		gc->pvUltRuntime,
		SCE_NULL);
}

IMG_INTERNAL IMG_BOOL SWMakeTextureMipmapLevels(GLES1Context *gc, GLESTexture *psTex, IMG_UINT32 ui32Face)
{
	PVRSRV_ERROR eResult;
	IMG_PVOID arg = GLES1Malloc(gc, sizeof(SWTexMipGenArg));
	SWTexMipGenArg *psArg = (SWTexMipGenArg *)arg;
	psArg->gc = gc;
	psArg->psTex = psTex;
	psArg->ui32Face = ui32Face;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if (psTex->psEGLImageTarget)
	{
		psArg->psSyncInfo = psTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
	}
	else
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */
	{
		if (psTex->psMemInfo)
		{
			psArg->psSyncInfo = psTex->psMemInfo->psClientSyncInfo;
		}
		else
		{
			psArg->psSyncInfo = IMG_NULL;
		}
	}

	if (psArg->psSyncInfo)
	{
		do
		{
			eResult = SGX2DQueryBlitsComplete(gc->ps3DDevData, psArg->psSyncInfo, IMG_TRUE);
		} while (eResult == PVRSRV_ERROR_TIMEOUT);

		PVRSRVCreateSyncInfoModObj(gc->psSysContext->psConnection, &psArg->hOpSyncObj);

		PVRSRVModifyPendingSyncOps(
			gc->psSysContext->psConnection,
			psArg->hOpSyncObj,
			&psArg->psSyncInfo,
			1,
			PVRSRV_MODIFYSYNCOPS_FLAGS_WO_INC,
			IMG_NULL,
			IMG_NULL);
	}

	sceKernelAtomicAddAndGet32(&gc->ui32AsyncTexOpNum, 1);

	sceUltUlthreadCreate(
		GLES1Malloc(gc, _SCE_ULT_ULTHREAD_SIZE),
		"OGLES1SWTextureMipGen",
		_SWTextureMipGenEntry,
		(IMG_UINT32)arg,
		SCE_NULL,
		0,
		gc->pvUltRuntime,
		SCE_NULL);

	return IMG_TRUE;
}

IMG_VOID texOpAsyncAddForCleanup(GLES1Context *gc, IMG_PVOID pvPtr)
{
	IMG_UINT32 i = 0;

	for (i = 0; i < 1024; i++)
	{
		if (pvAsDstPtr[i] == IMG_NULL)
		{
			ui32AsDstReqTime[i] = sceKernelGetProcessTimeLow();
			pvAsDstPtr[i] = pvPtr;
			return;
		}
	}

	GLES1FreeHeapUNC(gc, pvPtr);
}

IMG_INT32 texOpAsyncCleanupThread(IMG_UINT32 argSize, IMG_VOID *pArgBlock)
{
	IMG_UINT32 i = 0;
	IMG_UINT32 ui32TriggerTime = 0;
	GLES1Context *gc = *(GLES1Context **)pArgBlock;

	sceClibMemset(pvAsDstPtr, 0, 1024 * 4);
	sceClibMemset(&ui32AsDstReqTime[0], 0, 1024 * 4);

	while (1)
	{
		ui32TriggerTime = sceKernelGetProcessTimeLow();

		for (i = 0; i < 1024; i++)
		{
			if (pvAsDstPtr[i] != IMG_NULL)
			{
				if ((ui32TriggerTime - ui32AsDstReqTime[i]) > gc->sAppHints.ui32OGLES1SwTexOpCleanupDelay)
				{
					GLES1FreeHeapUNC(gc, pvAsDstPtr[i]);
					pvAsDstPtr[i] = IMG_NULL;
				}
			}
		}

		sceKernelDelayThread(100000);
	}
}