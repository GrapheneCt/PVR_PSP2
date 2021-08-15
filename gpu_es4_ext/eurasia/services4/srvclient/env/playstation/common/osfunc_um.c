#include <kernel.h>
#include <libheap.h>

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "eurasia/include4/services.h"
#include "eurasia/include4/pvr_debug.h"
#include "eurasia/services4/include/servicesint.h"
#include "eurasia/services4/include/pvr_bridge.h"

static ScePVoid s_userModeHeap = SCE_NULL;

int _PVRSRVCreateUserModeHeap()
{
	s_userModeHeap = sceHeapCreateHeap("PVRSRVUserModeHeap", 1 * 1024 * 1024, 0, SCE_NULL);

	if (!s_userModeHeap)
	{
		PVR_DPF((PVR_DBG_ERROR, "_PVRSRVCreateUserModeHeap, failed to create user mode heap"));
		return SCE_ERROR_ERRNO_ENOMEM;
	}

	return SCE_OK;
}

/******************************************************************************
 Function Name      : PVRSRVAllocUserModeMem
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_PVOID IMG_CALLCONV PVRSRVAllocUserModeMem(IMG_UINT32 ui32Size)
{
	return (IMG_PVOID)sceHeapAllocHeapMemory(s_userModeHeap, ui32Size);
}


/******************************************************************************
 Function Name      : PVRSRVCallocUserModeMem
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_PVOID IMG_CALLCONV PVRSRVCallocUserModeMem(IMG_UINT32 ui32Size)
{
	IMG_PVOID ret = (IMG_PVOID)sceHeapAllocHeapMemory(s_userModeHeap, ui32Size);

	if (!ret)
	{
		return IMG_NULL;
	}

	sceClibMemset(ret, 0, ui32Size);

	return ret;
}


/******************************************************************************
 Function Name      : PVRSRVReallocUserModeMem
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_PVOID IMG_CALLCONV PVRSRVReallocUserModeMem(IMG_PVOID pvBase, IMG_SIZE_T uNewSize)
{
	return (IMG_PVOID)sceHeapReallocHeapMemory(s_userModeHeap, pvBase, uNewSize);
}


/******************************************************************************
 Function Name      : PVRSRVFreeUserModeMem
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_VOID IMG_CALLCONV PVRSRVFreeUserModeMem(IMG_PVOID pvMem)
{
	sceHeapFreeHeapMemory(s_userModeHeap, pvMem);
}

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
 Function Name      : PVRSRVMemCopy
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
                      
******************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVMemCopy(IMG_VOID 	*pvDst,
								const IMG_VOID 	*pvSrc,
								IMG_UINT32 	ui32Size)
{
	sceClibMemcpy(pvDst, pvSrc, ui32Size);
}

/******************************************************************************
 Function Name      : PVRSRVLoadLibrary
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_HANDLE PVRSRVLoadLibrary(const IMG_CHAR *szLibraryName)
{
	IMG_HANDLE	hLib;

	hLib = (IMG_HANDLE)sceKernelLoadStartModule(szLibraryName, 0, NULL, 0, NULL, NULL);

	if (hLib < 0) 
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVLoadLibrary, sceKernelLoadStartModule failed to open library"));
		return PVRSRV_ERROR_UNABLE_TO_LOAD_LIBRARY;
	}
	else
	{
		return hLib;
	}
}

/******************************************************************************
 Function Name      : PVRSRVUnloadLibrary
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT PVRSRV_ERROR PVRSRVUnloadLibrary(IMG_HANDLE hExtDrv)
{
	if (hExtDrv != IMG_NULL)
	{
		if (sceKernelStopUnloadModule((SceUID)hExtDrv, 0, NULL, 0, NULL, NULL) == 0)
		{
			return PVRSRV_OK;
		}
		else
		{
			PVR_DPF((PVR_DBG_ERROR, "PVRSRVUnloadLibrary, sceKernelStopUnloadModule failed to close library"));
		}
	}
	else
	{
		PVR_DPF((PVR_DBG_ERROR, "PVRSRVUnloadLibrary, invalid hExtDrv"));
	}

	return PVRSRV_ERROR_UNLOAD_LIBRARY_FAILED;
}

/******************************************************************************
 Function Name      : PVRSRVGetLibFuncAddr
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT PVRSRV_ERROR PVRSRVGetLibFuncAddr(IMG_HANDLE hExtDrv, const IMG_CHAR *szFunctionName, IMG_VOID **ppvFuncAddr)
{
	PVR_DPF((PVR_DBG_ERROR, "PVRSRVGetLibFuncAddr is unimplemented"));
	return PVRSRV_ERROR_UNABLE_GET_FUNC_ADDR;
}

/******************************************************************************
 Function Name      : PVRSRVClockus
 Inputs             :
 Outputs            :
 Returns            :
 Description        :
******************************************************************************/
IMG_EXPORT IMG_UINT32 PVRSRVClockus(IMG_VOID)
{
	return sceKernelGetProcessTimeLow();
}

/******************************************************************************
 Function Name      : PVRSRVWaitus
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVWaitus(IMG_UINT32 ui32Timeus)
{
	sceKernelDelayThread(ui32Timeus);
}

/******************************************************************************
 Function Name      : PVRSRVReleaseThreadQuanta
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVReleaseThreadQuanta(IMG_VOID)
{

}

/******************************************************************************
 Function Name      : PVRSRVGetCurrentProcessID
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_EXPORT IMG_UINT32 PVRSRVGetCurrentProcessID(IMG_VOID)
{
	return (IMG_UINT32)sceKernelGetProcessId();
}

/******************************************************************************
 Function Name      : PVRSRVSetLocale
 Inputs             : Category, Locale
 Outputs            :
 Returns            : Current Locale if Locale is NULL
 Description        : Thin wrapper on posix setlocale

******************************************************************************/
IMG_EXPORT IMG_CHAR * PVRSRVSetLocale(const IMG_CHAR *pszLocale)
{
	PVR_DPF((PVR_DBG_ERROR, "PVRSRVSetLocale is unimplemented"));
	return IMG_NULL;
}

/*****************************************************************************
 Function Name      : OSTermClient
 Inputs             : pszMsg - message string to display
					  ui32ErrCode - error identifier
 Outputs            : none
 Returns            : void
 Description        : Terminates a client app which has become unusable.
******************************************************************************/
IMG_INTERNAL
IMG_VOID IMG_CALLCONV OSTermClient(IMG_CHAR* pszMsg, IMG_UINT32 ui32ErrCode)
{
#if defined(PVRSRV_NEED_PVR_DPF)
	PVR_DPF((PVR_DBG_ERROR, "%s: %s (error %d).", __func__, pszMsg, ui32ErrCode));
#else
	PVR_UNREFERENCED_PARAMETER(pszMsg);
	PVR_UNREFERENCED_PARAMETER(ui32ErrCode);
#endif

#if defined(PVRSRV_CLIENT_RESET_ON_HWTIMEOUT)
	sceKernelExitProcess(0); /* send terminate signal */
#endif
}

/*****************************************************************************
 Function Name      : OSEventObjectWait
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_INTERNAL
PVRSRV_ERROR IMG_CALLCONV OSEventObjectWait(const PVRSRV_CONNECTION *psConnection,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_EVENTSID hOSEvent)
#else
IMG_HANDLE hOSEvent)
#endif
{
	PVR_DPF((PVR_DBG_ERROR, "OSEventObjectWait is unimplemented"));
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

/*****************************************************************************
 Function Name      : OSEventObjectOpen
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_INTERNAL
PVRSRV_ERROR OSEventObjectOpen(IMG_CONST PVRSRV_CONNECTION *psConnection,
	PVRSRV_EVENTOBJECT *psEventObject,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_EVENTSID *phOSEvent)
#else
	IMG_HANDLE *phOSEvent)
#endif
{
	PVR_DPF((PVR_DBG_ERROR, "OSEventObjectOpen is unimplemented"));
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

/*****************************************************************************
 Function Name      : OSEventObjectClose
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_INTERNAL
PVRSRV_ERROR OSEventObjectClose(IMG_CONST PVRSRV_CONNECTION *psConnection,
	PVRSRV_EVENTOBJECT *psEventObject,
#if defined (SUPPORT_SID_INTERFACE)
	IMG_EVENTSID hOSEvent)
#else
	IMG_HANDLE hOSEvent)
#endif
{
	PVR_DPF((PVR_DBG_ERROR, "OSEventObjectClose is unimplemented"));
	return PVRSRV_ERROR_NOT_SUPPORTED;
}

/*****************************************************************************
 Function Name      : OSIsProcessPrivileged
 Inputs             :
 Outputs            :
 Returns            :
 Description        :

******************************************************************************/
IMG_INTERNAL
IMG_BOOL IMG_CALLCONV OSIsProcessPrivileged(IMG_VOID)
{
	return IMG_TRUE;
}

IMG_INTERNAL PVRSRV_ERROR OSFlushCPUCacheRange(IMG_VOID *pvRangeAddrStart,
	IMG_VOID *pvRangeAddrEnd)
{
	PVR_UNREFERENCED_PARAMETER(pvRangeAddrStart);
	PVR_UNREFERENCED_PARAMETER(pvRangeAddrEnd);

	return PVRSRV_ERROR_NOT_SUPPORTED;
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
#if defined(THREAD_SAFE)
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

	*phMutex = (PVRSRV_MUTEX_HANDLE)psPVRMutex;
#else
	*phMutex = 1;
#endif

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
#if defined(THREAD_SAFE)
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
#endif

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
#if defined(THREAD_SAFE)
	SceKernelLwMutexWork *psPVRMutex = (SceKernelLwMutexWork *)hMutex;
	IMG_INT iError;

	if (psPVRMutex == NULL)
		return;

	sceKernelLockLwMutex(psPVRMutex, 1, NULL);
#endif
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
#if defined(THREAD_SAFE)
	SceKernelLwMutexWork *psPVRMutex = (SceKernelLwMutexWork *)hMutex;
	IMG_INT iError;

	if (psPVRMutex == NULL)
		return;
	sceKernelUnlockLwMutex(psPVRMutex, 1);
#endif
}