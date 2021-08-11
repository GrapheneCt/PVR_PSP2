#ifndef _PSP2_SWTEXOP_
#define _PSP2_SWTEXOP_

#include "..\context.h"

typedef struct SWTexUploadArg
{
	GLES2Context *gc;
	GLES2Texture *psTex;
	GLES2MipMapLevel *psMipLevel;
	IMG_UINT32 ui32OffsetInBytes;
	GLES2TextureFormat *psTexFmt;
	IMG_UINT32 ui32Face;
	IMG_UINT32 ui32Lod;
	IMG_UINT32 ui32TopUsize;
	IMG_UINT32 ui32TopVsize;
	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo;
	IMG_SID hOpSyncObj;
} SWTexUploadArg;

typedef struct SWTexMipGenArg
{
	GLES2Context *gc;
	GLES2Texture *psTex;
	IMG_UINT32 ui32Face;
	IMG_UINT32 ui32MaxFace;
	IMG_BOOL bIsNonPow2;
	PVRSRV_CLIENT_SYNC_INFO *psSyncInfo;
	IMG_SID hOpSyncObj;
} SWTexMipGenArg;

IMG_INTERNAL IMG_VOID SWTextureUpload(
	GLES2Context *gc, GLES2Texture *psTex, GLES2MipMapLevel *psMipLevel, IMG_UINT32 ui32OffsetInBytes, GLES2TextureFormat *psTexFmt,
	IMG_UINT32 ui32Face, IMG_UINT32 ui32Lod, IMG_UINT32 ui32TopUsize, IMG_UINT32 ui32TopVsize);

IMG_INTERNAL IMG_BOOL SWMakeTextureMipmapLevels(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32Face, IMG_UINT32 ui32MaxFace, IMG_BOOL bIsNonPow2);

IMG_INT32 texOpAsyncCleanupThread(IMG_UINT32 argSize, IMG_VOID *pArgBlock);

IMG_VOID texOpAsyncAddForCleanup(GLES2Context *gc, IMG_PVOID pvPtr);

#endif /* _PSP2_SWTEXOP_ */
