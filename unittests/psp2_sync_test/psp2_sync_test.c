
// The purpose of this test is to probe SW multithreaded memory operations on PSP2 platforms

#include <kernel.h>
#include <ult.h>
#include <libsysmodule.h>
#include <stdlib.h>

SCE_USER_MODULE_LIST("app0:libgpu_es4_ext.suprx");

#include "psp2_pvr_desc.h"
#include "psp2_pvr_defs.h"

#include "img_defs.h"
#include "sgxdefs.h"
#include "services.h"

#include "sgxapi.h"

#define OP_TIME_SEC 5

#undef DPF
#define DPF printf

#ifndef EXIT_FAILURE
#define EXIT_FAILURE              (1)
#endif


#define FAIL_IF_NO_ERROR(val)					\
		if (val == PVRSRV_OK)					\
				{DPF(" FAIL\n"); exit(-1);}		\
		else {DPF(" OK\n");}

#define FAIL_IF_ERROR(val)								\
		if (val!=PVRSRV_OK)								\
		{												\
			DPF(" FAIL - %s\n",PVRSRVGetErrorString(val)); \
			exit(-1);									\
		}												\
		else {DPF(" OK\n");}

unsigned int sceLibcHeapSize = 16 * 1024 * 1024;

static PVRSRV_CLIENT_SYNC_INFO* psOpSyncInfo;
static PVRSRV_CONNECTION* psConnection;

int threadEntry(uint32_t arg)
{
	PVRSRV_ERROR			 eResult;
	IMG_SID					 hSyncObj;

	DPF("OP test thread start\n");

	DPF("Attempt to create sync info mod obj:\n");
	eResult = PVRSRVCreateSyncInfoModObj(psConnection, &hSyncObj);
	FAIL_IF_ERROR(eResult);

	DPF("Attempt to modify sync info mod obj:\n");
	eResult = PVRSRVModifyPendingSyncOps(
		psConnection,
		hSyncObj,
		&psOpSyncInfo,
		1,
		PVRSRV_MODIFYSYNCOPS_FLAGS_RO_INC | PVRSRV_MODIFYSYNCOPS_FLAGS_WO_INC,
		IMG_NULL,
		IMG_NULL);
	FAIL_IF_ERROR(eResult);

	sceKernelDelayThread(OP_TIME_SEC * 1000000);

	DPF("OP test thread end\n");

	DPF("Attempt to complete sync info mod obj:\n");
	eResult = PVRSRVModifyCompleteSyncOps(psConnection, hSyncObj);
	FAIL_IF_ERROR(eResult);

	DPF("Attempt to destroy sync info mod obj:\n");
	eResult = PVRSRVDestroySyncInfoModObj(psConnection, hSyncObj);
	FAIL_IF_ERROR(eResult);
}

int main(int argc, char ** argv)
{
	PVRSRV_ERROR             eResult;
	IMG_UINT32               uiNumDevices;
	PVRSRV_DEVICE_IDENTIFIER asDevID[1];
	PVRSRV_DEV_DATA          asDevData[1];
	PVRSRV_DEV_DATA         *ps3DDevData = IMG_NULL;
	IMG_UINT32               i;
	PVRSRV_SGX_CLIENT_INFO   sSGXInfo;
	int                      loop = 0;
	int                      frameStop = 1;
	int						 exit_point = 0;
	PVRSRV_MISC_INFO		 sMiscInfo;
	int						 ret = 0;

	SceUID pbMemUID;
	SceUID driverMemUID;
	SceUID rtMemUID;
	IMG_SID hRtMem = 0;
	IMG_SID hPbMem = 0;

#if defined (SUPPORT_SID_INTERFACE)
	IMG_SID    hDevMemContext;
#else
	IMG_HANDLE hDevMemContext;
#endif
	IMG_UINT32 ui32SharedHeapCount;
	PVRSRV_HEAP_INFO asHeapInfo[PVRSRV_MAX_CLIENT_HEAPS];

	uiNumDevices = 10;

	DPF("----------------------- Start -----------------------\n");

	DPF("Call PVRSRVConnect with a valid argument:\n");

	eResult = PVRSRVConnect(&psConnection, 0);

	FAIL_IF_ERROR(eResult);

	DPF("Get number of devices from PVRSRVEnumerateDevices:\n");

	eResult = PVRSRVEnumerateDevices(psConnection,
		&uiNumDevices,
		asDevID);
	FAIL_IF_ERROR(eResult);

	DPF(".... Reported %u devices\n", (unsigned int)uiNumDevices);

	/* List the devices */
	DPF(".... Device Number  | Device Type\n");

	for (i = 0; i < uiNumDevices; i++)
	{
		DPF("            %04d    | ", (int)asDevID[i].ui32DeviceIndex);
		DPF("\n");
	}

	/* Get each device... */
	for (i = 0; i < uiNumDevices; i++)
	{
		/*
			Only get services managed devices.
			Display Class API handles external display devices
		 */
		if (asDevID[i].eDeviceType != PVRSRV_DEVICE_TYPE_EXT)
		{
			PVRSRV_DEV_DATA *psDevData = asDevData + i;

			DPF("Attempt to acquire device %d:\n", (unsigned int)asDevID[i].ui32DeviceIndex);

			eResult = PVRSRVAcquireDeviceData(psConnection,
				asDevID[i].ui32DeviceIndex,
				psDevData,
				PVRSRV_DEVICE_TYPE_UNKNOWN);
			FAIL_IF_ERROR(eResult);

			/*
				Print out details about the SGX device.
				At the enumeration stage you should get back the device info
				from which you match a devicetype with index, i.e. we should
				know what index SGX device is and test for it now.
			*/
			if (asDevID[i].eDeviceType == PVRSRV_DEVICE_TYPE_SGX)
			{
				/* save off 3d devdata */
				ps3DDevData = psDevData;

				DPF("Getting SGX Client info\n");
				eResult = SGXGetClientInfo(psDevData, &sSGXInfo);

				FAIL_IF_ERROR(eResult);
			}
		}
	}

	if (ps3DDevData == IMG_NULL)
	{
		eResult = PVRSRV_ERROR_NO_DEVICEDATA_FOUND;
		FAIL_IF_ERROR(eResult);
	}

	DPF("Attempt to create memory context for SGX:\n");
	eResult = PVRSRVCreateDeviceMemContext(ps3DDevData,
		&hDevMemContext,
		&ui32SharedHeapCount,
		asHeapInfo);
	FAIL_IF_ERROR(eResult);

	DPF("Attempt to allocate test sync info:\n");
	eResult = PVRSRVAllocSyncInfo(ps3DDevData, &psOpSyncInfo);
	FAIL_IF_ERROR(eResult);

	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_ULT);

	SceUltUlthreadRuntime runtime;
	SceSize runtimeWorkAreaSize = sceUltUlthreadRuntimeGetWorkAreaSize(2, 1);
	void *runtimeWorkArea = malloc(runtimeWorkAreaSize);

	SceUltUlthreadRuntimeOptParam optParam;
	sceUltUlthreadRuntimeOptParamInitialize(&optParam);
	optParam.oneShotThreadStackSize = 4096;
	optParam.workerThreadAttr = 0;
	optParam.workerThreadCpuAffinityMask = 0;
	optParam.workerThreadOptParam = 0;
	optParam.workerThreadPriority = 64;

	DPF("Attempt to create ULT runtime:\n");
	ret = sceUltUlthreadRuntimeCreate(&runtime,
		"OPTestUltRuntime",
		2,
		1,
		runtimeWorkArea,
		&optParam);
	DPF(" 0x%X\n", ret);

	DPF("Attempt to create and start op thread:\n");
	SceUltUlthread ulthread;
	ret = sceUltUlthreadCreate(&ulthread,
		"OPTestThread",
		threadEntry,
		0,
		SCE_NULL,
		0,
		&runtime,
		SCE_NULL);
	DPF(" 0x%X\n", ret);

	sceKernelDelayThread(OP_TIME_SEC * 1000);

	DPF("Starting wait on OP with SGX2DQueryBlitsComplete:\n");
	do
	{
		eResult = SGX2DQueryBlitsComplete(ps3DDevData, psOpSyncInfo, IMG_TRUE);
	} while (eResult == PVRSRV_ERROR_TIMEOUT);

	FAIL_IF_ERROR(eResult);
}