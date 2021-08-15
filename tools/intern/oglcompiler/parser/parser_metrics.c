/**************************************************************************
 * Name         : metrics.c
 * Author       : James McCarthy
 * Created      : 11/10/2004
 *
 * Copyright    : 2000-2004 by Imagination Technologies Limited. All rights reserved.
 *              : No part of this software, either material or conceptual 
 *              : may be copied or distributed, transmitted, transcribed,
 *              : stored in a retrieval system or translated into any 
 *              : human or computer language in any form by any means,
 *              : electronic, mechanical, manual or other-wise, or 
 *              : disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, Unit 8, HomePark
 *              : Industrial Estate, King's Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Modifications:-
 * $Log: metrics.c $
 **************************************************************************/
#include "metrics.h"
#include "glsltree.h"
#include <string.h>

#ifdef METRICS


#ifdef STANDALONE

	/* Use clock() although the granularity is bad */
	#include <time.h>

#else

#define TIMING 1
#include "pvr_metrics.h"

#endif


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
IMG_INTERNAL IMG_VOID MetricsInit(IMG_VOID *pvCompilerPrivateData)
{
	GLSLCompilerPrivateData *psPrivateData = (GLSLCompilerPrivateData*)pvCompilerPrivateData;

#ifdef STANDALONE
	psPrivateData->fCPUSpeed = (IMG_FLOAT)CLOCKS_PER_SEC;
#else /* !STANDALONE */
	psPrivateData->fCPUSpeed = PVRSRVMetricsGetCPUFreq();
#endif

	memset(&psPrivateData->au32Time[0][0], 0, sizeof(psPrivateData->au32Time));
	memset(psPrivateData->au32Counter, 0, sizeof(psPrivateData->au32Counter));
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
IMG_INTERNAL IMG_VOID MetricStart(IMG_VOID *pvCompilerPrivateData, MetricsLog eMetricsLog)
{
	GLSLCompilerPrivateData *psPrivateData = (GLSLCompilerPrivateData*)pvCompilerPrivateData;

	if (eMetricsLog < METRICS_LASTMETRIC)
	{
#ifdef STANDALONE
		psPrivateData->au32Time[eMetricsLog][0] = clock();
#else /* !STANDALONE */
		psPrivateData->au32Time[eMetricsLog][0] = PVRSRVMetricsTimeNow();
#endif
	}
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
IMG_INTERNAL IMG_VOID MetricFinish(IMG_VOID *pvCompilerPrivateData, MetricsLog eMetricsLog)
{
	GLSLCompilerPrivateData *psPrivateData = (GLSLCompilerPrivateData*)pvCompilerPrivateData;

	if (eMetricsLog < METRICS_LASTMETRIC)
	{
#ifdef STANDALONE
		psPrivateData->au32Time[eMetricsLog][1] += clock() - psPrivateData->au32Time[eMetricsLog][0];
#else /* !STANDALONE */
		psPrivateData->au32Time[eMetricsLog][1] += PVRSRVMetricsTimeNow() - psPrivateData->au32Time[eMetricsLog][0];
#endif
	}
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
IMG_INTERNAL IMG_VOID MetricsAccumCounter(IMG_VOID *pvCompilerPrivateData, MetricsStatsLog eMetricsStat, IMG_UINT32 u32Count)
{
	GLSLCompilerPrivateData *psPrivateData = (GLSLCompilerPrivateData*)pvCompilerPrivateData;

	if (eMetricsStat < METRICCOUNT_LASTMETRIC)
	{
		psPrivateData->au32Counter[eMetricsStat] += u32Count;
	}
}


/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
IMG_INTERNAL IMG_FLOAT MetricsGetTime(IMG_VOID *pvCompilerPrivateData, MetricsLog eMetricsLog)
{
	GLSLCompilerPrivateData *psPrivateData = (GLSLCompilerPrivateData*)pvCompilerPrivateData;

	if (eMetricsLog < METRICS_LASTMETRIC)
	{
		return psPrivateData->au32Time[eMetricsLog][1] / psPrivateData->fCPUSpeed;
	}
	else
	{
		return 0.0f;
	}
}

/******************************************************************************
 * Function Name: 
 *
 * Inputs       : 
 * Outputs      : -
 * Returns      : 
 * Globals Used : -
 *
 * Description  :  
 *****************************************************************************/
IMG_INTERNAL IMG_UINT32 MetricsGetCounter(IMG_VOID *pvCompilerPrivateData, MetricsStatsLog eMetricsStat)
{
	GLSLCompilerPrivateData *psPrivateData = (GLSLCompilerPrivateData*)pvCompilerPrivateData;

	if (eMetricsStat < METRICCOUNT_LASTMETRIC)
	{
		return psPrivateData->au32Counter[eMetricsStat];
	}
	else
	{ 
		return 0;
	}
}


#else
#endif
