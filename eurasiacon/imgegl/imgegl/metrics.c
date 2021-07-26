/******************************************************************************
 * Name         : metrics.c
 *
 * Copyright    : 2007 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: metrics.c $
 *
 *  --- Revision Logs Removed --- 
 *****************************************************************************/

#if defined(TIMING) || defined(DEBUG)

#include "egl_internal.h"
#include "tls.h"


/***********************************************************************************
 Function Name      : InitMetrics
 Inputs             : psTls
 Outputs            : -
 Returns            : Success
 Description        : Initialises the metrics data for a thread
************************************************************************************/
IMG_INTERNAL IMG_BOOL InitIMGEGLMetrics(TLS psTls)
{
	PVRSRVMemSet(psTls->asTimes, 0, sizeof(PVR_Temporal_Data)*IMGEGL_NUM_TIMERS);

	if(!psTls->fCPUSpeed)
	{
		psTls->fCPUSpeed = PVRSRVMetricsGetCPUFreq();

		if(!psTls->fCPUSpeed)
		{
			PVR_DPF((PVR_DBG_ERROR,"InitMetrics: Failed to get CPU freq"));

			return IMG_FALSE;
		}
		else
		{
			psTls->fCPUSpeed = 1000.0f/psTls->fCPUSpeed;
		}
	}

	return IMG_TRUE;
}


static IMG_CHAR * const FunctionNames[] =
{
	"eglGetError                      ",
	"eglGetDisplay                    ",
	"eglInitialize                    ",
	"eglTerminate                     ",
	"eglQueryString                   ",
	"eglGetConfigs                    ",
	"eglChooseConfig                  ",
	"eglGetConfigAttrib               ",
	"eglCreateWindowSurface           ",
	"eglCreatePbufferSurface          ",
	"eglCreatePixmapSurface           ",
	"eglDestroySurface                ",
	"eglQuerySurface                  ",
	"eglBindAPI                       ",
	"eglQueryAPI                      ",
	"eglWaitClient                    ",
	"eglReleaseThread                 ",
	"eglCreatePbufferFromClientBuffer ",
	"eglSurfaceAttrib                 ",
	"eglBindTexImage                  ",
	"eglReleaseTexImage               ",
	"eglSwapInterval                  ",
	"eglCreateContext                 ",
	"eglDestroyContext                ",
	"eglMakeCurrent                   ",
	"eglGetCurrentContext             ",
	"eglGetCurrentSurface             ",
	"eglGetCurrentDisplay             ",
	"eglQueryContext                  ",
	"eglWaitGL                        ",
	"eglWaitNative                    ",
	"eglSwapBuffers                   ",
	"eglCopyBuffers                   ",
	"eglGetProcAddress                ",
	"eglSetDisplayPropertySymbian     ",
	"eglCreateImageKHR                ",
	"eglDestroyImageKHR               ",
	"eglCreateSyncKHR                 ",
	"eglDestroySyncKHR                ",
	"eglClientWaitSyncKHR             ",
	"eglGetSyncAttribKHR              "
};


/***********************************************************************************
 Function Name      : OutputMetrics
 Inputs             : psTls
 Outputs            : -
 Returns            : -
 Description        : Outputs the metrics data for a thread 
************************************************************************************/
IMG_INTERNAL IMG_VOID OutputIMGEGLMetrics(TLS psTls)
{
	IMG_UINT32 ui32TotalFrames, ui32ProfiledFrames, ui32Loop;

	if (psTls->psGlobalData->sAppHints.bDisableMetricsOutput)
	{
		return;
	}

	ui32TotalFrames    = psTls->asTimes[IMGEGL_COUNTER_SWAP_BUFFERS_COUNT].ui32Count & 0x7FFFFFFFL;
	ui32ProfiledFrames = psTls->asTimes[IMGEGL_TIMER_IMGeglSwapBuffers].ui32Count & 0x7FFFFFFFL;

	for(ui32Loop=0; ui32Loop<IMGEGL_NUM_TIMERS; ui32Loop++)
	{
		if(psTls->asTimes[ui32Loop].ui32Count & 0x80000000L)
		{
			PVR_TRACE(("Warning: IMGEGL Timer %d in ON position", ui32Loop));

			psTls->asTimes[ui32Loop].ui32Count &= 0x7FFFFFFFL;
		}
	}

	/* Dump out the data */
	if(psTls->psGlobalData->sAppHints.bDumpProfileData)
	{
		PVR_TRACE(("------------------------------------------------------------- "));
		PVR_TRACE(("   IMGEGL Profiling - (%d frames out of %d total)", ui32ProfiledFrames, ui32TotalFrames));
		PVR_TRACE(("------------------------------------------------------------- "));
		PVR_TRACE((""));

		if(ui32ProfiledFrames)
		{
			PVR_TRACE((" Function                       Number of calls | Calls/Frame | Time/Call | Time/Frame"));

			for(ui32Loop=0; ui32Loop<sizeof(FunctionNames)/sizeof(IMG_CHAR *); ui32Loop++)
			{
				PVR_Temporal_Data *psTimer = &psTls->asTimes[IMGEGL_TIMER_IMGeglGetError+ui32Loop];

				if(psTimer->ui32Count)
				{
					IMG_UINT32 ui32Count = psTimer->ui32Count;
					IMG_FLOAT fTotalTime = (IMG_FLOAT)psTimer->ui32Total * psTls->fCPUSpeed;

					PVR_TRACE(("%s %10d  %10d       %10f   %10f", FunctionNames[ui32Loop], 
																  ui32Count, 
																  ui32Count/ui32ProfiledFrames,
																  fTotalTime/(IMG_FLOAT)ui32Count,
																  fTotalTime/(IMG_FLOAT)ui32ProfiledFrames));
				}
			}
		}
		else
		{
			PVR_TRACE((" Function                       Number of calls | Time/Call"));

			for(ui32Loop=0; ui32Loop<sizeof(FunctionNames)/sizeof(IMG_CHAR *); ui32Loop++)
			{
				PVR_Temporal_Data *psTimer = &psTls->asTimes[IMGEGL_TIMER_IMGeglGetError+ui32Loop];

				if(psTimer->ui32Count)
				{
					IMG_UINT32 ui32Count = psTimer->ui32Count;
					IMG_FLOAT fTotalTime = (IMG_FLOAT)psTimer->ui32Total * psTls->fCPUSpeed;

					PVR_TRACE(("%s %10d     %10f", FunctionNames[ui32Loop],
												   ui32Count,
												   fTotalTime/(IMG_FLOAT)ui32Count));
				}
			}
		}
	}

	PVR_TRACE((""));
}

#else /* defined(TIMING) || defined(DEBUG) */


#endif /* defined(TIMING) || defined(DEBUG) */

/******************************************************************************
 End of file (metrics.c)
******************************************************************************/



