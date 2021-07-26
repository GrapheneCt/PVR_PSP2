/******************************************************************************
 * Name         : profile.h
 *
 * Copyright    : 2006-2008 by Imagination Technologies Limited.
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
 * $Log: profile.h $
 *****************************************************************************/
#ifndef _PROFILE_H_
#define _PROFILE_H_

#if defined (__cplusplus)
extern "C" {
#endif

#if defined (TIMING) || defined (DEBUG)

#define PROFILE_DRAW_NO_VBO					0
#define PROFILE_DRAW_VERTEX_VBO				1
#define PROFILE_DRAW_INDEX_VBO				2
#define PROFILE_DRAW_INDEX_AND_VERTEX_VBO	3

#define GLES1_NUMBER_PROFILE_ENABLES	11


typedef struct DrawCallDataRec
{
	IMG_UINT32 ui32CallCount[GL_TRIANGLE_FAN+1];
	IMG_UINT32 ui32VertexCount[GL_TRIANGLE_FAN+1];

} DrawCallData;

typedef struct StateMetricDataRec
{
	IMG_UINT32 ui32Count;

	IMG_UINT32 ui32RasterEnables;
	IMG_UINT32 ui32TnLEnables;
	IMG_UINT32 ui32IgnoredEnables;
	IMG_UINT32 ui32FrameEnables;

	struct StateMetricDataRec *psNext;
	struct StateMetricDataRec *psPrevious;

} StateMetricData;


/* ********************************************************* */

#if defined(DEBUG) || (TIMING_LEVEL > 1)

#define GLES1_PROFILE_INCREMENT_DRAWARRAYS_CALLCOUNT(Y)		\
													{\
														if(GLES1_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if(gc->sAppHints.bDumpProfileData) \
															{ \
																IMG_UINT32 ui32Group = 0; \
																GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine); \
																if(psVAOMachine->ui32ControlWord & ATTRIBARRAY_SOURCE_BUFOBJ) \
																{ \
																	ui32Group += PROFILE_DRAW_VERTEX_VBO; \
																} \
																gc->sDrawArraysCalls[ui32Group].ui32CallCount[Y]++; \
															} \
														} \
													}

#define GLES1_PROFILE_INCREMENT_DRAWARRAYS_VERTEXCOUNT(Y,Z)	\
													{\
														if(GLES1_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if(gc->sAppHints.bDumpProfileData) \
															{ \
																IMG_UINT32 ui32Group = 0; \
																GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine); \
																if(psVAOMachine->ui32ControlWord & ATTRIBARRAY_SOURCE_BUFOBJ) \
																{ \
																	ui32Group += PROFILE_DRAW_VERTEX_VBO; \
																} \
																gc->sDrawArraysCalls[ui32Group].ui32VertexCount[Y]+=Z; \
															} \
														} \
													}

#define GLES1_PROFILE_INCREMENT_DRAWELEMENTS_CALLCOUNT(Y)		\
													{\
														if(GLES1_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if(gc->sAppHints.bDumpProfileData) \
															{ \
																IMG_UINT32 ui32Group = 0; \
																GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine); \
																if(psVAOMachine->ui32ControlWord & ATTRIBARRAY_SOURCE_BUFOBJ) \
																{ \
																	ui32Group += PROFILE_DRAW_VERTEX_VBO; \
																} \
																if (INDEX_BUFFER_OBJECT(gc)) \
																{ \
																	ui32Group += PROFILE_DRAW_INDEX_VBO; \
																} \
																gc->sDrawElementsCalls[ui32Group].ui32CallCount[Y]++; \
															} \
														} \
													}

#define GLES1_PROFILE_INCREMENT_DRAWELEMENTS_VERTEXCOUNT(Y,Z)	\
													{\
														if(GLES1_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if(gc->sAppHints.bDumpProfileData) \
															{ \
																IMG_UINT32 ui32Group = 0; \
																GLES1VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine); \
																if(psVAOMachine->ui32ControlWord & ATTRIBARRAY_SOURCE_BUFOBJ) \
																{ \
																	ui32Group += PROFILE_DRAW_VERTEX_VBO; \
																} \
																if (INDEX_BUFFER_OBJECT(gc)) \
																{ \
																	ui32Group += PROFILE_DRAW_INDEX_VBO; \
																} \
																gc->sDrawElementsCalls[ui32Group].ui32VertexCount[Y]+=Z; \
															} \
														} \
													}

#define GLES1_PROFILE_ADD_STATE_METRIC				{\
														if(GLES1_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if (gc->sAppHints.bDumpProfileData) \
															{ \
																AddToStateMetric(gc); \
															} \
														} \
													}

#define GLES1_PROFILE_ADD_REDUNDANT_ENABLE(X)		{\
														if(GLES1_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if(gc->sAppHints.bDumpProfileData && \
															  (gc->ui32Enables == ui32Enables)) \
															{ \
																gc->ui32RedundantEnables[X]++; \
															} \
															else \
															{ \
																gc->ui32ValidEnables[X]++; \
															} \
														} \
													}
#define GLES1_PROFILE_ADD_REDUNDANT_STATE(Z,W)		{\
													if(gc->ui32##W##Enables == ui32##W##Enables)\
														gc->ui32RedundantStates##W[Z]++; \
													else \
														gc->ui32ValidStates##W[Z]++; \
													}
#else

#define GLES1_PROFILE_INCREMENT_DRAWARRAYS_CALLCOUNT(Y)
#define GLES1_PROFILE_INCREMENT_DRAWARRAYS_VERTEXCOUNT(Y,Z)
#define GLES1_PROFILE_INCREMENT_DRAWELEMENTS_CALLCOUNT(Y)
#define GLES1_PROFILE_INCREMENT_DRAWELEMENTS_VERTEXCOUNT(Y,Z)
#define GLES1_PROFILE_ADD_STATE_METRIC
#define GLES1_PROFILE_ADD_REDUNDANT_STATE(Z,W)

#endif /* (TIMING_LEVEL > 1) */

IMG_VOID InitProfileData(GLES1Context *gc);
IMG_VOID ProfileOutputTotals(GLES1Context *gc);
IMG_VOID AddToStateMetric(GLES1Context *gc);
IMG_VOID AddToCopyTypeMetric(GLES1Context *gc, IMG_UINT32 ui32DrawType, IMG_UINT32 ui32PrimType);
IMG_VOID IncrementCopyTypeMetric(GLES1Context *gc, IMG_UINT32 ui32DrawType, IMG_UINT32 ui32PrimType);

#else /* defined (TIMING) || defined (DEBUG) */

#define GLES1_PROFILE_INCREMENT_DRAWARRAYS_CALLCOUNT(Y)
#define GLES1_PROFILE_INCREMENT_DRAWARRAYS_VERTEXCOUNT(Y,Z)
#define GLES1_PROFILE_INCREMENT_DRAWELEMENTS_CALLCOUNT(Y)
#define GLES1_PROFILE_INCREMENT_DRAWELEMENTS_VERTEXCOUNT(Y,Z)
#define GLES1_PROFILE_ADD_STATE_METRIC
#define GLES1_PROFILE_ADD_REDUNDANT_STATE(Z,W)

#endif /* defined (TIMING) || defined (DEBUG) */

#if defined(__cplusplus)
}
#endif

#endif /* _PROFILE_H_ */

/******************************************************************************
 End of file (profile.h)
******************************************************************************/
