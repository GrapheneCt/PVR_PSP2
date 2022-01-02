#include <kernel.h>
#include <taihen.h>

#include "psp2_pvr_defs.h"

#define PSP2_EXTENDED_HEAP_SIZE SCE_KERNEL_1MiB

int _PVRSRVCreateUserModeHeap(SceClibMspace mspace);
SceUID sceKernelGetModuleIdByAddr(const void *module_addr);

int __module_stop(SceSize argc, const void *args)
{
	return SCE_KERNEL_STOP_SUCCESS;
}

int __module_exit()
{
	return SCE_KERNEL_STOP_SUCCESS;
}

int _sceGpuUserStart(SceClibMspace *mspace, SceUID *memblockId)
{
	SceInt32 ret = SCE_KERNEL_START_SUCCESS;
	ScePVoid base;

	*memblockId = sceKernelAllocMemBlock("SceGpuUserHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, PSP2_EXTENDED_HEAP_SIZE, 0);
	if (*memblockId < 0) {
		sceClibPrintf("_sceGpuUserStart: Failed to allocate heap storage with error 0x%X\n", *memblockId);
		ret = SCE_KERNEL_START_NO_RESIDENT;
	}
	else {
		ret = sceKernelGetMemBlockBase(*memblockId, &base);
		if (ret < 0) {
			sceClibPrintf("_sceGpuUserStart: Failed to get address of heap storage with error 0x%X\n");
			sceKernelFreeMemBlock(*memblockId);
			ret = SCE_KERNEL_START_NO_RESIDENT;
		}
		else {
			*mspace = sceClibMspaceCreate(base, PSP2_EXTENDED_HEAP_SIZE);
			if (*mspace == SCE_NULL) {
				sceClibPrintf("_sceGpuUserStart: Failed to create internal heap\n");
				sceKernelFreeMemBlock(*memblockId);
				ret = SCE_KERNEL_START_NO_RESIDENT;
			}
			else {
				ret = SCE_KERNEL_START_SUCCESS;
			}
		}
	}

	return ret;
}

int __module_start(SceSize argc, void *args)
{
	SceInt32 ret;
	PVRSRV_CONNECTION* psConnection;
	SceUID gpuEs4Modid = SCE_UID_INVALID_UID;
	SceUID uheapMemblockId = SCE_UID_INVALID_UID;
	SceClibMspace uheapMspace = SCE_NULL;
	ScePVoid dataAddr = SCE_NULL;
	SceKernelModuleInfo modInfo;
	SceKernelModuleInfo modInfo2;

	PVRSRVConnect(&psConnection, 0);
	PVRSRVDisconnect(psConnection);

	gpuEs4Modid = sceKernelGetModuleIdByAddr(psConnection);
	if (gpuEs4Modid < 0)
		return SCE_KERNEL_START_NO_RESIDENT;

	modInfo.size = sizeof(SceKernelModuleInfo);
	sceKernelGetModuleInfo(gpuEs4Modid, &modInfo);

	dataAddr = modInfo.segments[1].vaddr;

	sceClibMspaceDestroy(*(SceClibMspace *)(dataAddr + 0x8));
	sceKernelFreeMemBlock(*(SceUID *)(dataAddr + 0xC));

	ret = _sceGpuUserStart(dataAddr + 0x8, dataAddr + 0xC);
	if (ret != SCE_KERNEL_START_SUCCESS)
		return ret;

	_PVRSRVCreateUserModeHeap(*(SceClibMspace *)(dataAddr + 0x8));

	return SCE_KERNEL_START_SUCCESS;
}