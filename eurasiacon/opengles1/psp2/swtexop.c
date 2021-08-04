
#include <kernel.h>
#include <ult.h>

#include "..\context.h"
#include "swtexop.h"

int32_t sceKernelAtomicAddAndGet32(volatile int32_t* ptr, int32_t value);

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

IMG_INTERNAL IMG_VOID SWTextureUpload(
	GLES1Context *gc, GLESTexture *psTex, IMG_VOID *pvDest,
	const IMG_VOID	*pvSrc, IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
	IMG_UINT32 ui32StrideIn)
{
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

	PVRSRVCreateSyncInfoModObj(psArg->gc->psSysContext->psConnection, &psArg->hOpSyncObj);

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