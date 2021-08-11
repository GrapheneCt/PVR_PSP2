/**************************************************************************
 * Name         : metrics.h
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
 * $Log: metrics.h $
 **************************************************************************/

#ifndef __gl_metrics_h_
#define __gl_metrics_h_

#include "img_types.h"
#include "usc.h"

typedef enum MetricsLogTAG
{
	/* GLSL METRICS */
	METRICS_PARSETREE        = 0,
	METRICS_AST              = 1,
	METRICS_ICODE            = 2,
	METRICS_VFLEXTRANS       = 3,
	METRICS_PFLEXTRANS       = 4,
	METRICS_TOTAL            = 5,
	METRICS_TOKENLIST        = 6,
	METRICS_PARSECONTEXT     = 7,
	METRICS_SHUTDOWN         = 8,
	METRICS_PREPROCESSOR     = 9,
	METRICS_UNIFLEXINPUTGEN  = 10,
	METRICS_UNIFLEXOUTPUTGEN = 11,

	/* UNIFLEX METRICS */
	METRICS_USC_BASE                                     = 12, /* Dummy value used to separate the usc metrics from the glsl metrics */
	METRICS_USC_INTERMEDIATE_CODE_GENERATION             = METRICS_USC_BASE + USC_METRICS_INTERMEDIATE_CODE_GENERATION,
	METRICS_USC_VALUE_NUMBERING                          = METRICS_USC_BASE + USC_METRICS_VALUE_NUMBERING,
	METRICS_USC_FLATTEN_CONDITIONALS_AND_ELIMINATE_MOVES = METRICS_USC_BASE + USC_METRICS_FLATTEN_CONDITIONALS_AND_ELIMINATE_MOVES,
	METRICS_USC_INTEGER_OPTIMIZATIONS                    = METRICS_USC_BASE + USC_METRICS_INTEGER_OPTIMIZATIONS,
	METRICS_USC_MERGE_IDENTICAL_INSTRUCTIONS             = METRICS_USC_BASE + USC_METRICS_MERGE_IDENTICAL_INSTRUCTIONS,
	METRICS_USC_INSTRUCTION_SELECTION                    = METRICS_USC_BASE + USC_METRICS_INSTRUCTION_SELECTION,
	METRICS_USC_REGISTER_ALLOCATION                      = METRICS_USC_BASE + USC_METRICS_REGISTER_ALLOCATION,
	METRICS_USC_C10_REGISTER_ALLOCATION                  = METRICS_USC_BASE + USC_METRICS_C10_REGISTER_ALLOCATION,
	METRICS_USC_FINALISE_SHADER                          = METRICS_USC_BASE + USC_METRICS_FINALISE_SHADER,

	/* CUSTOM (CONFIGURABLE) UNIFLEX METRICS */
	METRICS_USC_CUSTOM_TIMER_A                           = METRICS_USC_BASE + USC_METRICS_CUSTOM_TIMER_A,
	METRICS_USC_CUSTOM_TIMER_B                           = METRICS_USC_BASE + USC_METRICS_CUSTOM_TIMER_B,
	METRICS_USC_CUSTOM_TIMER_C                           = METRICS_USC_BASE + USC_METRICS_CUSTOM_TIMER_C,

	METRICS_LASTMETRIC                                   = METRICS_USC_BASE + USC_METRICS_LAST, /* Always needs to be last entry */
} MetricsLog;


typedef enum MetricsStatsLogTAG	
{
	METRICCOUNT_NUMTOKENS                     = 0,
	METRICCOUNT_NUMPARSETREEBRANCHES          = 1,
	METRICCOUNT_NUMPARSETREEBRANCHESCREATED   = 2,
	METRICCOUNT_MAXNUMPARSETREEBRANCHES       = 3,
	METRICCOUNT_NUMICINSTRUCTIONS             = 4,
	METRICCOUNT_NUMCOMPILATIONS               = 5,

	METRICCOUNT_LASTMETRIC                    = 6, /* Always needs to be last entry */
} MetricsStatsLog; 


#ifdef METRICS

IMG_VOID	MetricsInit(IMG_VOID *pvCompilerPrivateData);

IMG_VOID	MetricStart(IMG_VOID *pvCompilerPrivateData, MetricsLog eMetricsLog);
IMG_VOID	MetricFinish(IMG_VOID *pvCompilerPrivateData, MetricsLog eMetricsLog);
IMG_FLOAT	MetricsGetTime(IMG_VOID *pvCompilerPrivateData, MetricsLog eMetricsLog);

IMG_VOID	MetricsAccumCounter(IMG_VOID *pvCompilerPrivateData, MetricsStatsLog eMetricsStat, IMG_UINT32 u32Count);
IMG_UINT32	MetricsGetCounter(IMG_VOID *pvCompilerPrivateData, MetricsStatsLog eMetricsStat);

#else

#define MetricsInit(a)

#define MetricStart(a,b)
#define MetricFinish(a,b)
#define MetricsGetTime(a,b) 0.0f

#define MetricsAccumCounter(a,b,c)
#define MetricsGetCounter(a,b) 1.0f

#endif //METRICS

#endif // __gl_metrics_h_
