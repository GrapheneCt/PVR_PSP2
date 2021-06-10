#ifndef SCE_GXM_INTERNAL_PVR_DEFS_H
#define SCE_GXM_INTERNAL_PVR_DEFS_H

// Contains platform-specific definitions for PSP2

#include <kernel.h>

#include "eurasia/include4/services.h"

#define PVRSRV_PSP2_GENERIC_MEMORY_ATTRIB (PVRSRV_MEM_READ \
                                          | PVRSRV_MEM_WRITE \
                                          | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC \
                                          | PVRSRV_MEM_CACHE_CONSISTENT \
                                          | PVRSRV_MEM_NO_SYNCOBJ)

#define PVRSRV_PSP2_GENERIC_MEMORY_ATTRIB_NC (PVRSRV_MEM_READ \
                                          | PVRSRV_MEM_WRITE \
                                          | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC \
                                          | PVRSRV_MEM_NO_SYNCOBJ)

#define PVRSRV_PSP2_GENERIC_MEMORY_ATTRIB_RDONLY_NC (PVRSRV_MEM_READ \
                                          | PVRSRV_HAP_NO_GPU_VIRTUAL_ON_ALLOC \
                                          | PVRSRV_MEM_NO_SYNCOBJ)

#define SGX_PIXELSHADER_SHARED_HEAP_ID	0x12
#define SGX_VERTEXSHADER_SHARED_HEAP_ID	0x13

typedef struct PVRSRVHeapInfoPsp2 {
	SceInt32 generalHeapId;
	SceInt32 vertexshaderHeapId;
	SceInt32 pixelshaderHeapId;
	SceUInt32 vertexshaderHeapSize;
	SceUInt32 pixelshaderHeapSize;
	SceInt32 vertexshaderSharedHeapId;
	SceInt32 pixelshaderSharedHeapId;
	SceUInt32 vertexshaderSharedHeapSize;
	SceUInt32 pixelshaderSharedHeapSize;
	SceInt32 syncinfoHeapId;
	SceInt32 vpbTiledHeapId;
} PVRSRVHeapInfoPsp2;

int PVRSRVMapHwMemory(PVRSRV_DEV_DATA *psDevData, SceUID memblockUid, IMG_SID *phMemHandle, IMG_BOOL flag); // SceGpuEs4User_0EA7458D
int PVRSRVUnmapHwMemory(PVRSRV_DEV_DATA *psDevData, SceUID memblockUid); // SceGpuEs4User_156A6B70

typedef struct _SGXTQ_PSP2_DOWNSCALEOP_
{
	IMG_UINT32 ui32SrcFormat;
	IMG_PVOID  pSrcAddr;
	IMG_UINT32 ui32SrcPixelOffset;
	IMG_UINT32 ui32SrcPixelSize;
	IMG_UINT32 ui32DstFormat;
	IMG_PVOID  pDstAddr;
	IMG_UINT32 ui32ControlWords;
	IMG_UINT32 ui32DstPixelOffset;
	IMG_UINT32 ui3DstPixelSize;
} SGXTQ_PSP2_DOWNSCALEOP;

typedef struct _SGXTQ_PSP2_FILLOP_
{
	IMG_UINT32 ui32SrcFormat;
	IMG_PVOID  pSrcAddr;
	IMG_UINT32 ui32SrcFormat2;
	IMG_PVOID  pSrcAddr2;
	IMG_UINT32 ui32ControlWords;
	IMG_UINT32 ui32FillColor;
	IMG_UINT32 ui32SrcPixelOffset;
	IMG_UINT32 ui32SrcPixelSize;
} SGXTQ_PSP2_FILLOP;

typedef struct _SGX_PSP2_CONTROL_STREAM_
{
	union {
		SGXTQ_PSP2_DOWNSCALEOP	sDownscale;
		SGXTQ_PSP2_FILLOP		sFill;
	} uData;
} SGX_PSP2_CONTROL_STREAM;

IMG_IMPORT
PVRSRV_ERROR IMG_CALLCONV SGXTransferControlStream(
	SGX_PSP2_CONTROL_STREAM *psControlStream, 
	IMG_UINT32 ui32ControlStreamWords, 
	PVRSRV_DEV_DATA *psDevData, 
	IMG_HANDLE hTransferContext, 
	IMG_HANDLE hSyncObj, 
	IMG_UINT32 ui32SyncFlags, 
	IMG_UINT32 ui32SyncFlags2, 
	IMG_HANDLE hNotification);

#endif