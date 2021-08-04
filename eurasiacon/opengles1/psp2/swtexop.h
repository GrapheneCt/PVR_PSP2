#ifndef _PSP2_SWTEXOP_
#define _PSP2_SWTEXOP_

#include "..\context.h"

typedef struct SWTexUploadArg
{
	GLES1Context *gc;
	GLESTexture *psTex;
	IMG_VOID *pvDest;
	const IMG_VOID	*pvSrc;
	IMG_UINT32 ui32Width;
	IMG_UINT32 ui32Height;
	IMG_UINT32 ui32StrideIn;
	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo;
	IMG_SID hOpSyncObj;
} SWTexUploadArg;

IMG_INTERNAL IMG_VOID SWTextureUpload(
	GLES1Context *gc, GLESTexture *psTex, IMG_VOID *pvDest,
	const IMG_VOID	*pvSrc, IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
	IMG_UINT32 ui32StrideIn);

#endif /* _PSP2_SWTEXOP_ */
