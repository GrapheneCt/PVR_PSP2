
#include <kernel.h>
#include <ult.h>

#include "..\context.h"
#include "..\texture.h"
#include "swtexop.h"

int32_t sceKernelAtomicAddAndGet32(volatile int32_t* ptr, int32_t value);

static IMG_VOID *pvAsDstPtr[8192];

static IMG_INT32 _SWTextureUploadEntry(IMG_UINT32 arg)
{
	SWTexUploadArg *psArg = (SWTexUploadArg *)arg;

	TextureUpload(&psArg->psTex, &psArg->psMipLevel, psArg->ui32OffsetInBytes, &psArg->psTexFmt, psArg->ui32Face, psArg->ui32Lod, psArg->ui32TopUsize, psArg->ui32TopVsize);

	sceKernelAtomicAddAndGet32(&psArg->gc->ui32AsyncTexOpNum, -1);

	PVRSRVModifyCompleteSyncOps(psArg->gc->psSysContext->psConnection, psArg->hOpSyncObj);
	PVRSRVDestroySyncInfoModObj(psArg->gc->psSysContext->psConnection, psArg->hOpSyncObj);

	GLES2Free(IMG_NULL, psArg);
	SceUltUlthread *psSelf;
	sceUltUlthreadGetSelf(&psSelf);
	GLES2Free(IMG_NULL, psSelf);

	return sceUltUlthreadExit(0);
}

static IMG_INT32 _SWTextureMipGenEntry(IMG_UINT32 arg)
{
	SWTexMipGenArg *psArg = (SWTexMipGenArg *)arg;

	MakeTextureMipmapLevelsSoftware(psArg->gc, psArg->psTex, psArg->ui32Face, psArg->ui32MaxFace, psArg->bIsNonPow2);

	sceKernelAtomicAddAndGet32(&psArg->gc->ui32AsyncTexOpNum, -1);

	if (psArg->psSyncInfo)
	{
		PVRSRVModifyCompleteSyncOps(psArg->gc->psSysContext->psConnection, psArg->hOpSyncObj);
		PVRSRVDestroySyncInfoModObj(psArg->gc->psSysContext->psConnection, psArg->hOpSyncObj);
	}

	GLES2Free(IMG_NULL, psArg);
	SceUltUlthread *psSelf;
	sceUltUlthreadGetSelf(&psSelf);
	GLES2Free(IMG_NULL, psSelf);

	return sceUltUlthreadExit(0);
}

IMG_INTERNAL IMG_VOID SWTextureUpload(
	GLES2Context *gc, GLES2Texture *psTex, GLES2MipMapLevel *psMipLevel, IMG_UINT32 ui32OffsetInBytes, GLES2TextureFormat *psTexFmt,
	IMG_UINT32 ui32Face, IMG_UINT32 ui32Lod, IMG_UINT32 ui32TopUsize, IMG_UINT32 ui32TopVsize)
{
	PVRSRV_ERROR eResult;
	IMG_PVOID arg = GLES2Malloc(gc, sizeof(SWTexUploadArg));
	SWTexUploadArg *psArg = (SWTexUploadArg *)arg;
	psArg->gc = gc;
	GLES2MemCopy(&psArg->psTex, psTex, sizeof(GLES2Texture));
	GLES2MemCopy(&psArg->psMipLevel, psMipLevel, sizeof(GLES2MipMapLevel));
	GLES2MemCopy(&psArg->psTexFmt, psTexFmt, sizeof(GLES2TextureFormat));
	psArg->ui32OffsetInBytes = ui32OffsetInBytes;
	psArg->ui32Face = ui32Face;
	psArg->ui32Lod = ui32Lod;
	psArg->ui32TopUsize = ui32TopUsize;
	psArg->ui32TopVsize = ui32TopVsize;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if (psTex->psEGLImageTarget)
	{
		psArg->psSyncInfo = psTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
	}
	else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
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

	int ret = sceUltUlthreadCreate(
		GLES2Malloc(gc, _SCE_ULT_ULTHREAD_SIZE),
		"OGLES2SWTextureUpload",
		_SWTextureUploadEntry,
		(IMG_UINT32)arg,
		SCE_NULL,
		0,
		gc->pvUltRuntime,
		SCE_NULL);
}

IMG_INTERNAL IMG_BOOL SWMakeTextureMipmapLevels(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32Face, IMG_UINT32 ui32MaxFace, IMG_BOOL bIsNonPow2)
{
	PVRSRV_ERROR eResult;
	IMG_PVOID arg = GLES2Malloc(gc, sizeof(SWTexMipGenArg));
	SWTexMipGenArg *psArg = (SWTexMipGenArg *)arg;
	psArg->gc = gc;
	psArg->psTex = psTex;
	psArg->ui32Face = ui32Face;
	psArg->ui32MaxFace = ui32MaxFace;
	psArg->bIsNonPow2 = bIsNonPow2;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if (psTex->psEGLImageTarget)
	{
		psArg->psSyncInfo = psTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
	}
	else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
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
		GLES2Malloc(gc, _SCE_ULT_ULTHREAD_SIZE),
		"OGLES2SWTextureMipGen",
		_SWTextureMipGenEntry,
		(IMG_UINT32)arg,
		SCE_NULL,
		0,
		gc->pvUltRuntime,
		SCE_NULL);

	return IMG_TRUE;
}

IMG_VOID texOpAsyncAddForCleanup(GLES2Context *gc, IMG_PVOID pvPtr)
{
	IMG_UINT32 i = 0;

	for (i = 0; i < sizeof(pvAsDstPtr) / 4; i++)
	{
		if (pvAsDstPtr[i] == IMG_NULL)
		{
			pvAsDstPtr[i] = pvPtr;
			return;
		}
	}

	PVR_DPF((PVR_DBG_WARNING, "texOpAsyncAddForCleanup: not enough space in free queue"));

	GLES2Free(gc, pvPtr);
}

IMG_INT32 texOpAsyncCleanupThread(IMG_UINT32 argSize, IMG_VOID *pArgBlock)
{
	IMG_UINT32 i = 0;
	IMG_UINT32 ui32TriggerTime = 0;
	GLES2Context *gc = *(GLES2Context **)pArgBlock;

	sceClibMemset(pvAsDstPtr, 0, sizeof(pvAsDstPtr));

	while (!gc->bSwTexOpFin)
	{
		ui32TriggerTime = sceKernelGetProcessTimeLow();

		for (i = 0; i < sizeof(pvAsDstPtr) / 4; i++)
		{
			if (pvAsDstPtr[i] != IMG_NULL && !gc->ui32AsyncTexOpNum)
			{
				SGXWaitTransfer(gc->ps3DDevData, gc->psSysContext->hTransferContext);
				GLES2Free(gc, pvAsDstPtr[i]);
				pvAsDstPtr[i] = IMG_NULL;
			}
		}

		sceKernelDelayThread(gc->sAppHints.ui32SwTexOpCleanupDelay);
	}

	return sceKernelExitDeleteThread(0);
}