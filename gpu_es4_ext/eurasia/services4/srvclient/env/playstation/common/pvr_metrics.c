/*****************************************************************************
 * File				pvr_metrics.c
 * 
 * Title			Metrics related functions
 * 
 * Author			Imagination Technologies
 * 
 * date   			30th April 2007
 *  
 * Copyright       	Copyright 2007-2010 by Imagination Technologies Limited.
 *                 	All rights reserved. No part of this software, either
 *                 	material or conceptual may be copied or distributed,
 *                 	transmitted, transcribed, stored in a retrieval system
 *                 	or translated into any human or computer language in any
 *                 	form by any means, electronic, mechanical, manual or
 *                 	other-wise, or disclosed to third parties without the
 *                 	express written permission of Imagination Technologies
 *                 	Limited, Unit 8, HomePark Industrial Estate,
 *                 	King's Langley, Hertfordshire, WD4 8LZ, U.K.
 * 
 * $Log: pvr_metrics.c $
 */

#include <string.h>
#include <kernel.h>
#include <power.h>
#include <appmgr.h>

#include "services.h"
#include "pvr_debug.h"
#include "pvr_metrics.h"


#if defined(DEBUG) || defined(TIMING)

static volatile IMG_UINT32 *pui32TimerRegister;

typedef struct tagLARGE_INTEGER
{
	IMG_UINT32 	LowPart;
	IMG_UINT32	HighPart;

} LARGE_INTEGER;

typedef union tagLI
{
	LARGE_INTEGER LargeInt;
	IMG_INT64 LongLong;

} LI;



/***********************************************************************************
 Function Name      : PVRSRVGetTimerRegister
 Inputs             : psConnection (connection info), psMiscInfo (misc. info)
 Outputs            : -	
 Returns            : PVRSRV_ERROR
 Description        : get timer register from misc info 
************************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVGetTimerRegister(IMG_VOID	*pvSOCTimerRegisterUM)
{
	pui32TimerRegister = pvSOCTimerRegisterUM;
}

/***********************************************************************************
 Function Name      : PVRSRVRelaseTimerRegister
 Inputs             : psConnection (connection info), psMiscInfo (misc. info)
 Outputs            : -	
 Returns            : PVRSRV_ERROR
 Description        : get timer register from misc info 
************************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVReleaseTimerRegister(IMG_VOID)
{
	pui32TimerRegister = IMG_NULL;
}

/***********************************************************************************
 Function Name      : PVRSRVMetricsSleep
 Inputs             : ui32MilliSeconds
 Outputs            : -
 Returns            : -
 Description        : Relinquish the time slice for a specified amount of time
************************************************************************************/
static IMG_VOID PVRSRVMetricsSleep(IMG_UINT32 ui32MilliSeconds)
{
	sceKernelDelayThread(ui32MilliSeconds * 1000);
}


/***********************************************************************************
 Function Name      : PVRSRVMetricsTimeNow
 Inputs             : -
 Outputs            : -
 Returns            : Time
 Description        : Unsigned int value being the current 32bit clock tick with ref
					  to the CPU Speed.
************************************************************************************/
IMG_EXPORT IMG_UINT32 PVRSRVMetricsTimeNow(IMG_VOID)
{
	return PVRSRVClockus();
}


/***********************************************************************************
 Function Name      : PVRSRVMetricsGetCPUFreq
 Inputs             : -
 Outputs            : -
 Returns            : Float value being the CPU frequency.
 Description        : Gets the CPU frequency
************************************************************************************/
IMG_EXPORT IMG_FLOAT PVRSRVMetricsGetCPUFreq(IMG_VOID)
{
	IMG_UINT32 ui32Time1;
	IMG_FLOAT fTPS;

	fTPS = (IMG_FLOAT)scePowerGetArmClockFrequency();

	return fTPS;
}

#define BUFLEN 12

/***********************************************************************************
 Function Name      : PVRSRVInitProfileOutput
 Inputs             : ppvFileInfo
 Outputs            : -
 Returns            : -
 Description        : Inits the profile output file
************************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVInitProfileOutput(IMG_VOID **ppvFileInfo)
{
	IMG_CHAR appname[BUFLEN];
	IMG_CHAR fullPath[256];
	IMG_CHAR *slash;
	SceUID fd;

	sceAppMgrAppParamGetString(SCE_KERNEL_PROCESS_ID_SELF, 12, appname, 12);

	sceClibSnprintf(fullPath, 256, "%s%s%s", "ux0:data/PVRSRV_Profile/", appname, "_profile.txt");

	fd = sceIoOpen(fullPath, SCE_O_WRONLY | SCE_O_CREAT, 0);

	if (fd <= 0)
	{
		PVR_DPF((PVR_DBG_WARNING, "%s: Failed to open %s for writing",
			__func__, fullPath));
	}

	*ppvFileInfo = (IMG_VOID *)fd;
}


/***********************************************************************************
 Function Name      : PVRSRVDeInitProfileOutput
 Inputs             : ppvFileInfo
 Outputs            : -
 Returns            : -
 Description        : Ends the profile output file by closing it
************************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVDeInitProfileOutput(IMG_VOID **ppvFileInfo)
{
	if(*ppvFileInfo <= 0)
		return;

	sceIoClose((SceUID)*ppvFileInfo);
	*ppvFileInfo = NULL;
}


/***********************************************************************************
 Function Name      : PVRSRVProfileOutput
 Inputs             : pvFileInfo, psString
 Outputs            : -
 Returns            : -
 Description        : Prints the string to the profile file and to the console
************************************************************************************/
IMG_EXPORT IMG_VOID PVRSRVProfileOutput(IMG_VOID *pvFileInfo, const IMG_CHAR *psString)
{
	IMG_CHAR newline[2];
	IMG_UINT32 len;
	SceUID fd = (SceUID)pvFileInfo;
	sceClibStrncpy(newline, "\n", 2);

	if(pvFileInfo > 0)
	{
		len = sceClibStrnlen(psString, 1024);
		sceIoWrite(fd, psString, len - 1);
		sceIoWrite(fd, newline, sizeof(newline));
	}

	PVR_TRACE(("%s", psString));
}


#endif  /* defined (TIMING) || defined (DEBUG) */







