/******************************************************************************
 * Name         : profile.h
 *
 * Copyright    : 2006-2009 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system
 *              : or translated into any human or computer language in any
 *              : form by any means, electronic, mechanical, manual or otherwise,
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited,
 *              : Home Park Estate, King's Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * $Log: profile.h $
 *****************************************************************************/
#ifndef _PROFILE_H_
#define _PROFILE_H_

#include "drvgl2.h" 	/* For define of GL_TRIANGLE_FAN */

#if defined (__cplusplus)
extern "C" {
#endif

#if defined (TIMING) || defined (DEBUG)

#define PROFILE_DRAW_NO_VBO					0
#define PROFILE_DRAW_VERTEX_VBO				1
#define PROFILE_DRAW_INDEX_VBO				2
#define PROFILE_DRAW_INDEX_AND_VERTEX_VBO	3

#define GLES2_NUMBER_PROFILE_ENABLES	11


typedef struct DrawCallDataRec
{
	IMG_UINT32 ui32CallCount[GL_TRIANGLE_FAN+1];
	IMG_UINT32 ui32VertexCount[GL_TRIANGLE_FAN+1];

} DrawCallData;

typedef struct StateMetricDataRec
{
	IMG_UINT32 ui32Count;

	IMG_UINT32 ui32Enables;

	struct StateMetricDataRec *psNext;
	struct StateMetricDataRec *psPrevious;

} StateMetricData;


/* ********************************************************* */

#if defined(DEBUG) || (TIMING_LEVEL > 1)

#define GLES2_PROFILE_INCREMENT_DRAWARRAYS_CALLCOUNT(Y)		\
													{\
														if(GLES2_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if(gc->sAppHints.bDumpProfileData) \
															{ \
																IMG_UINT32 ui32Group = 0; \
																GLES2VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine); \
																if(psVAOMachine->ui32ControlWord & ATTRIBARRAY_SOURCE_BUFOBJ) \
																{ \
																    ui32Group += PROFILE_DRAW_VERTEX_VBO; \
																} \
																gc->sDrawArraysCalls[ui32Group].ui32CallCount[Y]++; \
															} \
														} \
													}

#define GLES2_PROFILE_INCREMENT_DRAWARRAYS_VERTEXCOUNT(Y,Z)	\
													{\
														if(GLES2_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if(gc->sAppHints.bDumpProfileData) \
															{ \
																IMG_UINT32 ui32Group = 0; \
																GLES2VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine); \
																if(psVAOMachine->ui32ControlWord & ATTRIBARRAY_SOURCE_BUFOBJ) \
																{ \
																    ui32Group += PROFILE_DRAW_VERTEX_VBO; \
																} \
																gc->sDrawArraysCalls[ui32Group].ui32VertexCount[Y]+=Z; \
															} \
														} \
													}

#define GLES2_PROFILE_INCREMENT_DRAWELEMENTS_CALLCOUNT(Y)		\
													{\
														if(GLES2_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if(gc->sAppHints.bDumpProfileData) \
															{ \
																IMG_UINT32 ui32Group = 0; \
																GLES2VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine); \
																if(psVAOMachine->ui32ControlWord & ATTRIBARRAY_SOURCE_BUFOBJ) \
																{ \
																    ui32Group += PROFILE_DRAW_VERTEX_VBO; \
																} \
                                                                if (psVAOMachine->psBoundElementBuffer) \
																{ \
																    ui32Group += PROFILE_DRAW_INDEX_VBO; \
																} \
                                                                gc->sDrawElementsCalls[ui32Group].ui32CallCount[Y]++; \
															} \
														} \
													}

#define GLES2_PROFILE_INCREMENT_DRAWELEMENTS_VERTEXCOUNT(Y,Z)	\
													{\
														if(GLES2_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if(gc->sAppHints.bDumpProfileData) \
															{ \
																IMG_UINT32 ui32Group = 0; \
																GLES2VertexArrayObjectMachine *psVAOMachine = &(gc->sVAOMachine); \
																if(psVAOMachine->ui32ControlWord & ATTRIBARRAY_SOURCE_BUFOBJ) \
																{ \
																    ui32Group += PROFILE_DRAW_VERTEX_VBO; \
																} \
																if (psVAOMachine->psBoundElementBuffer) \
																{ \
																    ui32Group += PROFILE_DRAW_INDEX_VBO; \
																} \
																gc->sDrawElementsCalls[ui32Group].ui32VertexCount[Y]+=Z; \
															} \
														} \
													}

#define GLES2_PROFILE_ADD_STATE_METRIC				{\
														if(GLES2_CHECK_BETWEEN_START_END_FRAME) \
														{ \
															if (gc->sAppHints.bDumpProfileData) \
															{ \
																AddToStateMetric(gc); \
															} \
														} \
													}

#define GLES2_PROFILE_ADD_REDUNDANT_ENABLE(X)		{\
														if(GLES2_CHECK_BETWEEN_START_END_FRAME) \
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
#else

#define GLES2_PROFILE_INCREMENT_DRAWARRAYS_CALLCOUNT(Y)
#define GLES2_PROFILE_INCREMENT_DRAWARRAYS_VERTEXCOUNT(Y,Z)
#define GLES2_PROFILE_INCREMENT_DRAWELEMENTS_CALLCOUNT(Y)
#define GLES2_PROFILE_INCREMENT_DRAWELEMENTS_VERTEXCOUNT(Y,Z)
#define GLES2_PROFILE_ADD_STATE_METRIC
#define GLES2_PROFILE_ADD_REDUNDANT_ENABLE(X)

#endif /* (TIMING_LEVEL > 1) */

IMG_VOID InitProfileData(GLES2Context *gc);
IMG_VOID ProfileOutputTotals(GLES2Context *gc);
IMG_VOID AddToStateMetric(GLES2Context *gc);
IMG_VOID AddToCopyTypeMetric(GLES2Context *gc, IMG_UINT32 ui32DrawType, IMG_UINT32 ui32PrimType);
IMG_VOID IncrementCopyTypeMetric(GLES2Context *gc, IMG_UINT32 ui32DrawType, IMG_UINT32 ui32PrimType);

#else /* defined (TIMING) || defined (DEBUG) */

#define GLES2_PROFILE_INCREMENT_DRAWARRAYS_CALLCOUNT(Y)
#define GLES2_PROFILE_INCREMENT_DRAWARRAYS_VERTEXCOUNT(Y,Z)
#define GLES2_PROFILE_INCREMENT_DRAWELEMENTS_CALLCOUNT(Y)
#define GLES2_PROFILE_INCREMENT_DRAWELEMENTS_VERTEXCOUNT(Y,Z)
#define GLES2_PROFILE_ADD_STATE_METRIC
#define GLES2_PROFILE_ADD_REDUNDANT_ENABLE(X)

#endif /* defined (TIMING) || defined (DEBUG) */

#if defined(__cplusplus)
}
#endif

#endif /* _PROFILE_H_ */

/******************************************************************************
 End of file (profile.h)
******************************************************************************/


