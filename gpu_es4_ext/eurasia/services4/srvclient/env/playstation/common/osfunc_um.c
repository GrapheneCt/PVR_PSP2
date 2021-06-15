#include <kernel.h>

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "eurasia/include4/services.h"
#include "eurasia/include4/pvr_debug.h"
#include "eurasia/services4/include/servicesint.h"
#include "eurasia/services4/include/pvr_bridge.h"

/******************************************************************************
 Function Name      : PVRSRVMemSet
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVMemSet(IMG_VOID *pvDest, IMG_UINT8 ui8Value, IMG_UINT32 ui32Size)
{
	sceClibMemset(pvDest, (IMG_INT)ui8Value, (size_t)ui32Size);
}

/******************************************************************************
 Function Name      : PVRSRVCreateMutex
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV PVRSRVCreateMutex(PVRSRV_MUTEX_HANDLE *phMutex)
{
	SceKernelLwMutexWork *psPVRMutex;
	IMG_INT iError;

	psPVRMutex = PVRSRVAllocUserModeMem(sizeof(SceKernelLwMutexWork));

	if (psPVRMutex == NULL)
	{
		return PVRSRV_ERROR_OUT_OF_MEMORY;
	}

	iError = sceKernelCreateLwMutex(psPVRMutex, "PVRSRVMutex", SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);

	if (iError != SCE_OK)
	{
		PVRSRVFreeUserModeMem(psPVRMutex);
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVCreateMutex: sceKernelCreateLwMutex failed (0x%X)", iError));
		return PVRSRV_ERROR_INIT_FAILURE;
	}

	return PVRSRV_OK;
}

/******************************************************************************
 Function Name      : PVRSRVDestroyMutex
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT PVRSRV_ERROR IMG_CALLCONV PVRSRVDestroyMutex(PVRSRV_MUTEX_HANDLE hMutex)
{
	SceKernelLwMutexWork *psPVRMutex = (SceKernelLwMutexWork *)hMutex;
	IMG_INT iError;

	if (psPVRMutex == NULL)
		return PVRSRV_ERROR_MUTEX_DESTROY_FAILED;

	iError = sceKernelDeleteLwMutex(psPVRMutex);
	if (iError != SCE_OK)
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVDestroyMutex: sceKernelDeleteLwMutex failed (0x%X)", iError));
		return PVRSRV_ERROR_MUTEX_DESTROY_FAILED;
	}

	PVRSRVFreeUserModeMem(psPVRMutex);

	return PVRSRV_OK;
}

/******************************************************************************
 Function Name      : PVRSRVLockMutex
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_VOID IMG_CALLCONV PVRSRVLockMutex(PVRSRV_MUTEX_HANDLE hMutex)
{
	SceKernelLwMutexWork *psPVRMutex = (SceKernelLwMutexWork *)hMutex;
	IMG_INT iError;

	if (psPVRMutex == NULL)
		return;

	sceKernelLockLwMutex(psPVRMutex, 1, NULL);
}

/*****************************************************************************
 Function Name      : PVRSRVUnlockMutex
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_VOID IMG_CALLCONV PVRSRVUnlockMutex(PVRSRV_MUTEX_HANDLE hMutex)
{
	SceKernelLwMutexWork *psPVRMutex = (SceKernelLwMutexWork *)hMutex;
	IMG_INT iError;

	if (psPVRMutex == NULL)
		return;

	sceKernelUnlockLwMutex(psPVRMutex, 1);
}