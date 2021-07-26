/******************************************************************************
* Name         : metrics.h
*
* Copyright    : 2007-2008 by Imagination Technologies Limited.
*              : All rights reserved. No part of this software, either material
*              : or conceptual may be copied or distributed, transmitted,
*              : transcribed, stored in a retrieval system or translated into
*              : any human or computer language in any form by any means,
*              : electronic, mechanical, manual or otherwise, or disclosed
*              : to third parties without the express written permission of
*              : Imagination Technologies Limited, Home Park Estate,
*              : Kings Langley, Hertfordshire, WD4 8LZ, U.K.
*
* Platform     : ANSI
*
* $Log: metrics.h $
******************************************************************************/
#ifndef _METRICS_
#define _METRICS_

#if defined (__cplusplus)
extern "C" {
#endif

#if defined(TIMING) || defined(DEBUG)

IMG_BOOL InitIMGEGLMetrics(TLS psTls);
IMG_VOID OutputIMGEGLMetrics(TLS psTls);

#define METRICS_GROUP_ENABLED				0x0000001F 

/* 
	Timer groups
	- each new timer has to belong to a group
	- a timer is enabled only if its group is enabled (at build time)
*/
#define IMGEGL_METRICS_GROUP_ENTRYPOINTS	0x00000001
#define IMGEGL_METRICS_GROUP_EXAMPLE1		0x00000002
#define IMGEGL_METRICS_GROUP_EXAMPLE2		0x00000004
#define IMGEGL_METRICS_GROUP_EXAMPLE3		0x00000008
#define IMGEGL_METRICS_GROUP_EXAMPLE4		0x00000010

/*
	Timer defines
*/
#define IMGEGL_METRIC_DUMMY					0


#if (METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE1)

	#define IMGEGL_TIMER_EXAMPLE1_TIME		1

#else /*(METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE1)*/

	#define IMGEGL_TIMER_EXAMPLE1_TIME		0

#endif /*(METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE1)*/


#if (METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE2)

	#define IMGEGL_TIMER_EXAMPLE2_TIME		10

#else /*(METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE2)*/

	#define IMGEGL_TIMER_EXAMPLE2_TIME		0

#endif /*(METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE2)*/


#if (METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE3)

	#define IMGEGL_TIMER_EXAMPLE3_TIME		20

#else /*(METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE3)*/

	#define IMGEGL_TIMER_EXAMPLE3_TIME		0

#endif /*(METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE3)*/


#if (METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE4)

	#define IMGEGL_TIMER_EXAMPLE4_TIME		30

#else /*(METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE4)*/

	#define IMGEGL_TIMER_EXAMPLE4_TIME		0

#endif /*(METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_EXAMPLE4)*/


/* 
	Rather than use IMGEGL_TIMER_IMGeglSwapBuffers as a swap count IMGEGL_COUNTER_SWAP_BUFFERS_COUNT is kept
	as a separate counter to prevent problems when using a timer within IMGEGL_TIMER_IMGeglSwapBuffers().
	The timer-start macro overloads the top bit of the call count as a 'in use' flag which
	would mess up any profile start/stop calculations if we used the IMGEGL_TIMER_IMGeglSwapBuffers count
*/
#define IMGEGL_COUNTER_SWAP_BUFFERS_COUNT						99


#if (METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_ENTRYPOINTS)

	/* entry point times */
	#define IMGEGL_TIMER_IMGeglGetError							100
	#define IMGEGL_TIMER_IMGeglGetDisplay						101
	#define IMGEGL_TIMER_IMGeglInitialize						102
	#define IMGEGL_TIMER_IMGeglTerminate						103
	#define IMGEGL_TIMER_IMGeglQueryString						104
	#define IMGEGL_TIMER_IMGeglGetConfigs						105
	#define IMGEGL_TIMER_IMGeglChooseConfig						106
	#define IMGEGL_TIMER_IMGeglGetConfigAttrib					107
	#define IMGEGL_TIMER_IMGeglCreateWindowSurface				108
	#define IMGEGL_TIMER_IMGeglCreatePbufferSurface				109
	#define IMGEGL_TIMER_IMGeglCreatePixmapSurface				110
	#define IMGEGL_TIMER_IMGeglDestroySurface					111
	#define IMGEGL_TIMER_IMGeglQuerySurface						112
	#define IMGEGL_TIMER_IMGeglBindAPI							113
	#define IMGEGL_TIMER_IMGeglQueryAPI							114
	#define IMGEGL_TIMER_IMGeglWaitClient						115
	#define IMGEGL_TIMER_IMGeglReleaseThread					116
	#define IMGEGL_TIMER_IMGeglCreatePbufferFromClientBuffer	117
	#define IMGEGL_TIMER_IMGeglSurfaceAttrib					118
	#define IMGEGL_TIMER_IMGeglBindTexImage						119
	#define IMGEGL_TIMER_IMGeglReleaseTexImage					120
	#define IMGEGL_TIMER_IMGeglSwapInterval						121
	#define IMGEGL_TIMER_IMGeglCreateContext					122
	#define IMGEGL_TIMER_IMGeglDestroyContext					123
	#define IMGEGL_TIMER_IMGeglMakeCurrent						124
	#define IMGEGL_TIMER_IMGeglGetCurrentContext				125
	#define IMGEGL_TIMER_IMGeglGetCurrentSurface				126
	#define IMGEGL_TIMER_IMGeglGetCurrentDisplay				127
	#define IMGEGL_TIMER_IMGeglQueryContext						128
	#define IMGEGL_TIMER_IMGeglWaitGL							129
	#define IMGEGL_TIMER_IMGeglWaitNative						130
	#define IMGEGL_TIMER_IMGeglSwapBuffers						131
	#define IMGEGL_TIMER_IMGeglCopyBuffers						132
	#define IMGEGL_TIMER_IMGeglGetProcAddress					133
	#define IMGEGL_TIMER_IMGeglSetDisplayPropertySymbian		134
	#define IMGEGL_TIMER_IMGeglCreateImageKHR					135
	#define IMGEGL_TIMER_IMGeglDestroyImageKHR					136
	#define IMGEGL_TIMER_IMGeglCreateSyncKHR					137
	#define IMGEGL_TIMER_IMGeglDestroySyncKHR					138
	#define IMGEGL_TIMER_IMGeglClientWaitSyncKHR				139
	#define IMGEGL_TIMER_IMGeglGetSyncAttribKHR					140
	#define IMGEGL_TIMER_IMGeglSignalSyncKHR					141
	#define IMGEGL_TIMER_IMGeglCreateSharedImageNOK				142
	#define IMGEGL_TIMER_IMGeglDestroySharedImageNOK			143
	#define IMGEGL_TIMER_IMGeglQueryImageNOK					144
	#define IMGEGL_TIMER_IMGeglSetBlobCacheFuncsANDROID			145

#else /* (METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_ENTRYPOINTS) */

	/* entry point times */
	#define IMGEGL_TIMER_IMGeglGetError							0
	#define IMGEGL_TIMER_IMGeglGetDisplay						0
	#define IMGEGL_TIMER_IMGeglInitialize						0
	#define IMGEGL_TIMER_IMGeglTerminate						0
	#define IMGEGL_TIMER_IMGeglQueryString						0
	#define IMGEGL_TIMER_IMGeglGetConfigs						0
	#define IMGEGL_TIMER_IMGeglChooseConfig						0
	#define IMGEGL_TIMER_IMGeglGetConfigAttrib					0
	#define IMGEGL_TIMER_IMGeglCreateWindowSurface				0
	#define IMGEGL_TIMER_IMGeglCreatePbufferSurface				0
	#define IMGEGL_TIMER_IMGeglCreatePixmapSurface				0
	#define IMGEGL_TIMER_IMGeglDestroySurface					0
	#define IMGEGL_TIMER_IMGeglQuerySurface						0
	#define IMGEGL_TIMER_IMGeglBindAPI							0
	#define IMGEGL_TIMER_IMGeglQueryAPI							0
	#define IMGEGL_TIMER_IMGeglWaitClient						0
	#define IMGEGL_TIMER_IMGeglReleaseThread					0
	#define IMGEGL_TIMER_IMGeglCreatePbufferFromClientBuffer	0
	#define IMGEGL_TIMER_IMGeglSurfaceAttrib					0
	#define IMGEGL_TIMER_IMGeglBindTexImage						0
	#define IMGEGL_TIMER_IMGeglReleaseTexImage					0
	#define IMGEGL_TIMER_IMGeglSwapInterval						0
	#define IMGEGL_TIMER_IMGeglCreateContext					0
	#define IMGEGL_TIMER_IMGeglDestroyContext					0
	#define IMGEGL_TIMER_IMGeglMakeCurrent						0
	#define IMGEGL_TIMER_IMGeglGetCurrentContext				0
	#define IMGEGL_TIMER_IMGeglGetCurrentSurface				0
	#define IMGEGL_TIMER_IMGeglGetCurrentDisplay				0
	#define IMGEGL_TIMER_IMGeglQueryContext						0
	#define IMGEGL_TIMER_IMGeglWaitGL							0
	#define IMGEGL_TIMER_IMGeglWaitNative						0
	#define IMGEGL_TIMER_IMGeglSwapBuffers						0
	#define IMGEGL_TIMER_IMGeglCopyBuffers						0
	#define IMGEGL_TIMER_IMGeglGetProcAddress					0
	#define IMGEGL_TIMER_IMGeglSetDisplayPropertySymbian		0
	#define IMGEGL_TIMER_IMGeglCreateImageKHR					0
	#define IMGEGL_TIMER_IMGeglDestroyImageKHR					0
	#define IMGEGL_TIMER_IMGeglCreateSyncKHR					0
	#define IMGEGL_TIMER_IMGeglDestroySyncKHR					0
	#define IMGEGL_TIMER_IMGeglClientWaitSyncKHR				0
	#define IMGEGL_TIMER_IMGeglGetSyncAttribKHR					0
	#define IMGEGL_TIMER_IMGeglSignalSyncKHR					0
	#define IMGEGL_TIMER_IMGeglCreateSharedImageNOK				0
	#define IMGEGL_TIMER_IMGeglDestroySharedImageNOK			0
	#define IMGEGL_TIMER_IMGeglQueryImageNOK					0
	#define IMGEGL_TIMER_IMGeglSetBlobCacheFuncsANDROID			0

#endif /* (METRICS_GROUP_ENABLED & IMGEGL_METRICS_GROUP_ENTRYPOINTS) */

#define IMGEGL_NUM_TIMERS 146

/******************************************************
These are the new metrics, which basically 
redirect to those defined in pvr_metrics
*******************************************************/

#define IMGEGL_CALLS(X)					PVR_MTR_CALLS(psTls->asTimes[X])

#define IMGEGL_TIME_PER_CALL(X)			PVR_MTR_TIME_PER_CALL(psTls->asTimes[X], psTls->fCPUSpeed)

#define IMGEGL_MAX_TIME(X)				PVR_MTR_MAX_TIME(psTls->asTimes[X], psTls->fCPUSpeed)
									

#define IMGEGL_TIME_PER_FRAME(X)		PVR_MTR_TIME_PER_FRAME(psTls->asTimes[IMGEGL_COUNTER_SWAP_BUFFERS_COUNT], psTls->asTimes[X], psTls->fCPUSpeed)	

#define IMGEGL_METRIC_PER_FRAME(X)		PVR_MTR_METRIC_PER_FRAME(psTls->asTimes[IMGEGL_COUNTER_SWAP_BUFFERS_COUNT], psTls->asTimes[X])

#define IMGEGL_PARAM_PER_FRAME(X)		PVR_MTR_PARAM_PER_FRAME(psTls->asTimes[IMGEGL_COUNTER_SWAP_BUFFERS_COUNT], psTls->asTimes[X])	


#define IMGEGL_CHECK_BETWEEN_START_END_FRAME	PVR_MTR_CHECK_BETWEEN_START_END_FRAME(psTls->psGlobalData->sAppHints, psTls->asTimes[IMGEGL_COUNTER_SWAP_BUFFERS_COUNT])


#define IMGEGL_TIME_RESET(X)			PVR_MTR_TIME_RESET(psTls->asTimes[X])

#define IMGEGL_TIME_START(X)			{                                                                                           \
											int tmp = X;                                                                               \
											if(tmp)                                                                                    \
											{                                                                                          \
												PVR_MTR_TIME_START(psTls->asTimes[X], psTls->psGlobalData->sAppHints, psTls->asTimes[IMGEGL_COUNTER_SWAP_BUFFERS_COUNT])  \
											}                                                                                          \
										}
	                                 
#define IMGEGL_TIME_SUSPEND(X)			PVR_MTR_TIME_SUSPEND(psTls->asTimes[X])

#define IMGEGL_TIME_RESUME(X)			PVR_MTR_TIME_RESUME(psTls->asTimes[X])

#define IMGEGL_TIME_STOP(X)				{                                                                                          \
											int tmp = X;                                                                              \
											if(tmp)                                                                                   \
											{                                                                                         \
												PVR_MTR_TIME_STOP(psTls->asTimes[X], psTls->psGlobalData->sAppHints, psTls->asTimes[IMGEGL_COUNTER_SWAP_BUFFERS_COUNT])  \
											}                                                                                         \
										}

#define IMGEGL_SWAP_TIME_START(X)		PVR_MTR_SWAP_TIME_START(psTls->asTimes[X])

#define IMGEGL_SWAP_TIME_STOP(X)		PVR_MTR_SWAP_TIME_STOP(psTls->asTimes[X])



#define IMGEGL_TIME_RESET_SUM(X)		PVR_MTR_TIME_RESET_SUM(psTls->asTimes[X]) 

#define IMGEGL_INC_PERFRAME_COUNT(X)	PVR_MTR_INC_PERFRAME_COUNT(psTls->asTimes[X])

#define IMGEGL_RESET_PERFRAME_COUNT(X)	PVR_MTR_RESET_PERFRAME_COUNT(psTls->asTimes[X])

#define IMGEGL_DECR_COUNT(Y)			PVR_MTR_DECR_COUNT(psTls->asTimes[X])


#if defined(TIMING) && (TIMING_LEVEL < 1)

#define IMGEGL_TIME1_START(X)
#define IMGEGL_TIME1_STOP(X)
#define IMGEGL_INC_COUNT(Y,X)

#else /* defined(TIMING) && (TIMING_LEVEL < 1)*/

#define IMGEGL_TIME1_START(X)			PVR_MTR_TIME_START(psTls->asTimes[X], psTls->psGlobalData->sAppHints, psTls->asTimes[IMGEGL_COUNTER_SWAP_BUFFERS_COUNT])
#define IMGEGL_TIME1_STOP(X)			PVR_MTR_TIME_STOP(psTls->asTimes[X], psTls->psGlobalData->sAppHints, psTls->asTimes[IMGEGL_COUNTER_SWAP_BUFFERS_COUNT])
#define IMGEGL_INC_COUNT(Y,X)			{                                                                                            \
											int tmp = Y;                                                                               \
											if(tmp)                                                                                    \
											{                                                                                          \
												PVR_MTR_INC_COUNT(Y, psTls->asTimes[Y], X, psTls->psGlobalData->sAppHints, psTls->asTimes[IMGEGL_COUNTER_SWAP_BUFFERS_COUNT], IMGEGL_COUNTER_SWAP_BUFFERS_COUNT) \
											}                                                                                          \
										}
						
#endif /* defined(TIMING) && (TIMING_LEVEL < 1)*/ 


#else /* !(defined (TIMING) || defined (DEBUG)) */

#define NUM_TIMERS

#define IMGEGL_TIME_RESET(X)
#define IMGEGL_TIME_START(X)
#define IMGEGL_TIME_SUSPEND(X)
#define IMGEGL_TIME_RESUME(X)
#define IMGEGL_TIME_STOP(X)
#define IMGEGL_SWAP_TIME_START(X)
#define IMGEGL_SWAP_TIME_STOP(X)
#define IMGEGL_TIME1_START(X)
#define IMGEGL_TIME1_STOP(X)
#define IMGEGL_INC_COUNT(X,Y)
#define IMGEGL_DECR_COUNT(X)
#define IMGEGL_INC_PERFRAME_COUNT(X)
#define IMGEGL_RESET_PERFRAME_COUNT(X)
#define IMGEGL_TIME_RESET_SUM(X)

#define IMGEGL_RESET_FRAME()

#endif /* defined (TIMING) || defined (DEBUG) */


#if defined(__cplusplus)
}
#endif

#endif /* _METRICS_ */

/**************************************************************************
 End of file (metrics.h)
**************************************************************************/
