/******************************************************************************
 * Name         : tnlstate.h
 *
 * Copyright    : 2003-2006 by Imagination Technologies Limited.
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
 * $Log: tnlstate.h $
 **************************************************************************/

#ifndef _TNLSTATE_
#define _TNLSTATE_

#include "xform.h"

#define AP_VERTEX			0
#define AP_NORMAL			1
#define AP_COLOR			2
#define AP_TEXCOORD0		3
#define AP_TEXCOORD1		4
#define AP_TEXCOORD2		5
#define AP_TEXCOORD3		6

#define AP_POINTSIZE		7
#define AP_WEIGHTARRAY		8
#define AP_MATRIXINDEX		9

#define GLES1_MAX_NUMBER_OF_VERTEX_ATTRIBS 10

#define VARRAY_VERT_ENABLE			(1UL << AP_VERTEX)
#define VARRAY_NORMAL_ENABLE		(1UL << AP_NORMAL)
#define VARRAY_COLOR_ENABLE			(1UL << AP_COLOR)
#define VARRAY_TEXCOORD0_ENABLE		(1UL << AP_TEXCOORD0)
#define VARRAY_TEXCOORD1_ENABLE		(1UL << AP_TEXCOORD1)
#define VARRAY_TEXCOORD2_ENABLE		(1UL << AP_TEXCOORD2)
#define VARRAY_TEXCOORD3_ENABLE		(1UL << AP_TEXCOORD3)

#define VARRAY_POINTSIZE_ENABLE		(1UL << AP_POINTSIZE)
#define VARRAY_WEIGHTARRAY_ENABLE	(1UL << AP_WEIGHTARRAY)
#define VARRAY_MATRIXINDEX_ENABLE	(1UL << AP_MATRIXINDEX)

#define GLES1_MAX_ATTRIBS_ARRAY	GLES1_MAX_NUMBER_OF_VERTEX_ATTRIBS

#define VARRAY_PROVOKE	VARRAY_VERT_ENABLE



#define AP_DUMMY			GLES1_MAX_ATTRIBS_ARRAY


typedef struct GLESCurrentStateRec
{
	GLEScoord asAttrib[GLES1_MAX_ATTRIBS_ARRAY];

#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
	IMG_UINT32 ui32MatrixPaletteIndex;
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */

} GLESCurrentState;


typedef struct GLESFogStateRec
{
	GLenum eMode;
	IMG_UINT32 ui32Color;
	GLEScolor sColor;
	IMG_FLOAT fDensity, fStart, fEnd;
	IMG_FLOAT fOneOverEMinusS;

} GLESFogState;


#define GLES1_STREAMTYPE_BYTE		0x00000000
#define GLES1_STREAMTYPE_UBYTE		0x00000001
#define GLES1_STREAMTYPE_SHORT		0x00000002
#define GLES1_STREAMTYPE_USHORT		0x00000003
#define GLES1_STREAMTYPE_FLOAT		0x00000004
#define GLES1_STREAMTYPE_HALFFLOAT	0x00000005
#define GLES1_STREAMTYPE_FIXED		0x00000006

#define GLES1_STREAMTYPE_MAX		0x00000007

#define GLES1_STREAMTYPE_MASK		0x00000007

#define GLES1_STREAMNORM_BIT		0x00000008

#define GLES1_STREAMSIZE_SHIFT		4
#define GLES1_STREAMSIZE_MASK		0x000000F0


extern IMG_VOID (* const CopyData[4][GLES1_STREAMTYPE_MAX])(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);
extern IMG_VOID (* const MemCopyData[4][GLES1_STREAMTYPE_MAX])(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);

#if defined(NO_UNALIGNED_ACCESS)
extern IMG_VOID (* const CopyDataShortAligned[4][GLES1_STREAMTYPE_MAX])(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);
extern IMG_VOID (* const CopyDataByteAligned[4][GLES1_STREAMTYPE_MAX])(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);
#endif


typedef struct GLES1AttribArrayPointerStateRec
{
	const IMG_UINT8	*pui8Pointer;

	IMG_UINT32	ui32StreamTypeSize;

	IMG_UINT32	ui32UserStride;

	GLESBufferObject *psBufObj;

} GLES1AttribArrayPointerState;


typedef struct GLES1AttribArrayPointerMachineRec
{
	IMG_UINT32	ui32Stride;

	IMG_UINT32	ui32Size;

	IMG_BOOL	bIsCurrentState;

	const IMG_UINT8	*pui8CopyPointer;
	IMG_UINT32	     ui32CopyStreamTypeSize;
	IMG_UINT32	     ui32CopyStride;
	const IMG_UINT8	*pui8SrcPointer;
	IMG_UINT8	    *pui8DstPointer;
	IMG_UINT32	     ui32DstSize;

#if defined(NO_UNALIGNED_ACCESS)
	IMG_VOID	(*pfnCopyData[4])(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);
#else
	IMG_VOID	(*pfnCopyData)(const IMG_VOID *pvSrc, IMG_VOID * IMG_RESTRICT pvDst, IMG_UINT32 ui32SrcStride, IMG_UINT32 ui32Count);
#endif

	const IMG_VOID	*pvPDSSrcAddress;

    GLES1AttribArrayPointerState *psState;

} GLES1AttribArrayPointerMachine;


/*
   Attrib array control word flags
*/
	#define ATTRIBARRAY_SOURCE_BUFOBJ		0x00000001UL /* At least one attrib is a buffer object */
	#define ATTRIBARRAY_SOURCE_VARRAY		0x00000002UL /* At least one attrib is vertex array */
	#define ATTRIBARRAY_SOURCE_CURRENT		0x00000004UL /* At least one attrib is current state */
	#define ATTRIBARRAY_MAP_BUFOBJ			0x00000008UL /* At least one buffer object is mapped currently */
	#define ATTRIBARRAY_BAD_BUFOBJ			0x00000010UL /* At least one attrib is a buffer object with no meminfo */


/* 
	The vertex pointer state is assigned as follows:

	0	Position
	1	Normal
	2	Color
	3	TexCoord1
	4	TexCoord2
	5	Point Size
	6	Weight Array
	7	Matrix Index
	8	VertexAttrib 0 - this is a special case since it is aliased with position 0
	9	VertexAttrib 1
	10	VertexAttrib 2
	11	VertexAttrib 3
	12	VertexAttrib 4
	13	VertexAttrib 5
	14	VertexAttrib 6
	15	VertexAttrib 7
*/

/*
** Matrix handling function pointers.  These functions are used to compute a
** matrix, not to use a matrix for computations.
*/
typedef struct GLESMatrixProcsRec
{
	IMG_VOID (*pfnCopy)(GLESMatrix *psDest, const GLESMatrix *psSrc);

	IMG_VOID (*pfnInvertTranspose)(GLESMatrix *psDest, const GLESMatrix *psSrc);

	IMG_VOID (*pfnMakeIdentity)(GLESMatrix *psDest);

	IMG_VOID (*pfnMult)(GLESMatrix *psDest, const GLESMatrix *psSrcA, const GLESMatrix *psSrcB);

} GLESMatrixProcs;


struct GLESProcsRec
{
	/* Procs used to operate on individual matricies */
	GLESMatrixProcs sMatrixProcs;

	/* Matrix stack operations */
	IMG_VOID (*pfnPushMatrix)(GLES1Context *gc);
	IMG_VOID (*pfnPopMatrix)(GLES1Context *gc);
	IMG_VOID (*pfnLoadIdentity)(GLES1Context *gc);

	IMG_VOID (*pfnPickMatrixProcs)(GLES1Context *gc, GLESMatrix *psMatrix);
	IMG_VOID (*pfnPickInvTransposeProcs)(GLES1Context *gc, GLESMatrix *psMatrix);

	/* Recompute the inverse transpose of a given model-view matrix */
	IMG_VOID (*pfnComputeInverseTranspose)(GLES1Context *gc, GLES1Transform *psTransform);
	/* Normalize an incoming normal coordinate & return square of length*/
	IMG_FLOAT (*pfnNormalize)(IMG_FLOAT afDst[3], const IMG_FLOAT afSrc[3]);
};



typedef struct GLESMaterialStateRec
{
	GLEScolor sAmbient;			/* unscaled */
	GLEScolor sDiffuse;			/* unscaled */
	GLEScolor sSpecular;		/* unscaled */
	GLEScolor sEmissive;		/* scaled */
	GLEScoord sSpecularExponent;

} GLESMaterialState;


typedef struct GLESLightModelStateRec
{
	GLEScolor sAmbient;			/* scaled */
	IMG_BOOL bTwoSided;

} GLESLightModelState;


typedef struct GLESLightSourceStateRec
{
	GLEScolor sAmbient;			/* scaled */
	GLEScolor sDiffuse;			/* scaled */
	GLEScolor sSpecular;		/* scaled */
	GLEScoord sPosition;
	GLEScoord sPositionEye;
	GLEScoord sDirection;
	GLEScoord sSpotDirectionEye;
	IMG_FLOAT fSpotLightExponent;
	IMG_FLOAT fSpotLightCutOffAngle;
	IMG_FLOAT afAttenuation[4];

} GLESLightSourceState;


typedef struct GLESLightStateRec
{
/*
 * GLenum shadingModel;
 *
 *	Stored in ui32ISPTSPWord
 */
	GLESLightModelState sModel;
	GLESMaterialState sMaterial;
	GLESLightSourceState *psSource;

	GLenum eColorMaterialParam;

} GLESLightState;


#define GLES1_LIGHT_ATTENUATION_CONSTANT	0
#define GLES1_LIGHT_ATTENUATION_LINEAR		1
#define GLES1_LIGHT_ATTENUATION_QUADRATIC	2



#endif /* _TNLSTATE_ */

/******************************************************************************
 End of file (tnlstate.h)
******************************************************************************/

