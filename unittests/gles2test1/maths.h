/**************************************************************************
 * Name         : main.h
 * Author       : BCB
 * Created      : 07/12/2005
 *
 * Copyright    : 2005-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Date: 2010/02/01 11:15:28 $ $Revision: 1.2 $
 * $Log: maths.h $
 **************************************************************************/

#ifndef __MATHS_H__
#define __MATHS_H__

#ifdef __GNUC__
#define __internal __attribute__((visibility("hidden")))
#else
#define __internal
#endif

void myFrustum(float pMatrix[4][4], float left, float right, float bottom, float top, float zNear, float zFar);
void myPersp(float pMatrix[4][4], float fovy, float aspect, float zNear, float zFar);
void myTranslate(float pMatrix[4][4], float fX, float fY, float fZ);
void myScale(float pMatrix[4][4], float fX, float fY, float fZ);
void myRotate(float pMatrix[4][4], float fX, float fY, float fZ, float angle);
void myIdentity(float pMatrix[4][4]);
void myMultMatrix(float psRes[4][4], float psSrcA[4][4], float psSrcB[4][4]);
void myInvertTransposeMatrix(float pDstMatrix[3][3], float pSrcMatrix[4][4]);

#endif /* __MATHS_H__ */
