/**************************************************************************
 * Name         : xform.h
 *
 * Copyright    : 2003-2006 by Imagination Technologies Limited. All rights reserved.
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
 * $Log: xform.h $
 *
 *  --- Revision Logs Removed --- 
 **************************************************************************/
#ifndef _XFORM_H_
#define _XFORM_H_

/* 
** Note: 
**
** Other code assumes that all types >= GLES1_MT_IS2D are also 2D
** Other code assumes that all types >= GLES1_MT_W0001 are also W0001
** Other code assumes that all types >= GLES1_MT_IS2DNR are also 2DNR
**
*/
#define GLES1_MT_GENERAL		0	/* No information */
#define GLES1_MT_W0001		1	/* W row looks like 0 0 0 1 */
#define GLES1_MT_IS2D		2	/* 2D matrix */
#define GLES1_MT_IS2DNR		3	/* 2D non-rotational matrix */
#define GLES1_MT_IDENTITY	4	/* Identity */
#define GLES1_MT_IS2DNRSC	5	/* Screen coords, subset of 2DNR */


/*
** Matrix struct.  This contains a 4x4 matrix as well as function
** pointers used to do a transformation with the matrix.  The function
** pointers are loaded based on the matrix contents attempting to
** avoid unneccesary computation.
*/
struct GLESMatrixRec
{
    IMG_FLOAT afMatrix[4][4];

    /* 
    ** matrixType set to general if nothing is known about this matrix.
    **
    ** matrixType set to GLES1_MT_W0001 if it looks like this:
    ** | . . . 0 |
    ** | . . . 0 |
    ** | . . . 0 |
    ** | . . . 1 |
    **
    ** matrixType set to GLES1_MT_IS2D if it looks like this:
    ** | . . 0 0 |
    ** | . . 0 0 |
    ** | 0 0 . 0 |
    ** | . . . 1 |
    **
    ** matrixType set to GLES1_MT_IS2DNR if it looks like this:
    ** | . 0 0 0 |
    ** | 0 . 0 0 |
    ** | 0 0 . 0 |
    ** | . . . 1 |
    **
    ** 2DNRSC matrixes are difficult to deal with properly, but they
    ** are nicely efficient, so we try anyway.  If a matrix is marked as 
    ** 2DNRSC, it must be verified before it can be believed.  In order to
    ** verify it, you must check that the viewport starts at (0,0), and that
    ** the viewport width and height matches the width and height associated
    ** (below) with the matrix.
    **
    ** matrixType set to GLES1_MT_IS2DNRSC if it looks like this:
    ** | 2/W   0   0   0 |
    ** |   0 2/H   0   0 |
    ** |   0   0   .   0 |
    ** |  -1  -1   .   1 |
    **
    ** Note that the matrix type pickers are incremental.  The matrix
    ** may be marked as GLES1_MT_W001, for example, even if it also
    ** happens to be GLES1_MT_IS2D (as long as the user does not attempt
    ** to confuse OpenGL, everything is picked quickly and efficiently).
    **
    ** 2DNRSC matrixes also guarantee that a z of zero will not get clipped.
    */
    GLenum eMatrixType;

    /*
    ** Width and height that was used, assuming that it is a 2DNRSC 
    ** matrix.
    */
    IMG_UINT32 ui32Width, ui32Height;

    IMG_VOID (*pfnXf2)(GLEScoord *psRes, const IMG_FLOAT *pfData, const GLESMatrix *psMatrix);
    IMG_VOID (*pfnXf3)(GLEScoord *psRes, const IMG_FLOAT *pfData, const GLESMatrix *psMatrix);
    IMG_VOID (*pfnXf4)(GLEScoord *psRes, const IMG_FLOAT *pfData, const GLESMatrix *psMatrix);
};

/*
** Transform struct.  This structure is what the matrix stacks are
** composed of.  inverseTranspose contains the inverse transpose of matrix.
** For the modelView stack, "mvp" will contain the concatenation of
** the modelView and current projection matrix (i.e. the multiplication of
** the two matricies).
*/
typedef struct GLES1TransformRec
{
    GLESMatrix sMatrix;
    GLESMatrix sInverseTranspose;
    GLESMatrix sMvp;

    /* Sequence number tag for mvp */
    IMG_UINT32 ui32Sequence;
    IMG_BOOL bUpdateInverse;

    /* Scaling factor for GL_RESCALE_NORMAL */
    IMG_FLOAT fRescaleFactor;

} GLES1Transform;


/*
** Transformation machinery state.  Contains the state needed to transform
** user coordinates into eye & window coordinates.
*/
typedef struct GLES1TransformMachineRec
{
    /*
    ** Transformation stack.  "modelView" points to the active element in
    ** the stack.
    */
    GLES1Transform *psModelViewStack;
    GLES1Transform *psModelView;

	/*
    ** Modelview palette
    */
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
	GLES1Transform *psMatrixPalette;
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */

    /*
    ** Current projection matrix.  Used to transform eye coordinates into
    ** NTVP (or clip) coordinates.
    */
    GLES1Transform *psProjectionStack;
    GLES1Transform *psProjection;
	IMG_UINT32 ui32ProjectionSequence;

    /*
    ** Texture matrix stack.
    */
    GLES1Transform *apsTextureStack[GLES1_MAX_TEXTURE_UNITS];
    GLES1Transform *apsTexture[GLES1_MAX_TEXTURE_UNITS];

	GLEScoord asEyeClipPlane[GLES1_MAX_CLIP_PLANES];

	IMG_VOID (*pfnDrawPoints)(GLES1Context *gc, IMG_UINT32 ui32Start, IMG_UINT32 ui32Count, IMG_UINT16 *pui16Indices);

} GLES1TransformMachine;


	#define GLES1_CEILF(f)			(ceilf(f))
	#define GLES1_SQRTF(f)			(sqrtf(f))
	#define GLES1_POWF(a,b)			(powf(a,b))
	#define GLES1_ABSF(f)			(fabsf(f))
	#define GLES1_ABS(i)			(abs(i))
	#define GLES1_FLOORF(f)			(floorf(f))
	#define GLES1_SINF(f)			(sinf(f))
	#define GLES1_COSF(f)			(cosf(f))
	#define GLES1_ATANF(f)			(atanf(f))
	#define GLES1_LOGF(f)			(logf(f))
	#define GLES1_FREXP(f,e)		(frexpf(f, (int *)e))


IMG_VOID DoLoadMatrix(GLES1Context *gc, const GLESMatrix *psMatrix);
IMG_VOID DoMultMatrix(GLES1Context *gc, IMG_VOID *pvData, 
IMG_VOID (*pfnMultiply)(GLES1Context *gc, GLESMatrix *psDstMatrix, GLESMatrix *psSrcMatrix, IMG_VOID *pvData));

IMG_VOID MultiplyMatrix(GLES1Context *gc, GLESMatrix *psDstMatrix, GLESMatrix *psSrcMatrix, IMG_VOID *pvData);
IMG_VOID ScaleMatrix(GLES1Context *gc, GLESMatrix *psDstMatrix, GLESMatrix *psSrcMatrix, IMG_VOID *pvData);
IMG_VOID TranslateMatrix(GLES1Context *gc, GLESMatrix *psDstMatrix, GLESMatrix *psSrcMatrix, IMG_VOID *pvData);

IMG_VOID CopyMatrix(GLESMatrix *psDst, const GLESMatrix *psSrc);
IMG_VOID MakeIdentity(GLESMatrix *psMatrix);


IMG_VOID MultMatrix(GLESMatrix *psRes, const GLESMatrix *psSrcA, const GLESMatrix *psSrcB);

IMG_VOID InvertTransposeMatrix(GLESMatrix *psInverse, const GLESMatrix *psSrc);

IMG_VOID PushModelViewMatrix(GLES1Context *gc);
IMG_VOID PopModelViewMatrix(GLES1Context *gc);
IMG_VOID LoadIdentityModelViewMatrix(GLES1Context *gc);

IMG_VOID PushMatrixPaletteMatrix(GLES1Context *gc);
IMG_VOID PopMatrixPaletteMatrix(GLES1Context *gc);
IMG_VOID LoadIdentityMatrixPaletteMatrix(GLES1Context *gc);

IMG_VOID PushProjectionMatrix(GLES1Context *gc);
IMG_VOID PopProjectionMatrix(GLES1Context *gc);
IMG_VOID LoadIdentityProjectionMatrix(GLES1Context *gc);

IMG_VOID PushTextureMatrix(GLES1Context *gc);
IMG_VOID PopTextureMatrix(GLES1Context *gc);
IMG_VOID LoadIdentityTextureMatrix(GLES1Context *gc);

IMG_VOID TransposeMatrix(GLESMatrix *psTranspose, const GLESMatrix *psSrc);
IMG_VOID ComputeInverseTranspose(GLES1Context *gc, GLES1Transform *psTransform);
IMG_FLOAT Normalize(IMG_FLOAT afVout[3], const IMG_FLOAT afVin[3]);

IMG_VOID PickMatrixProcs(GLES1Context *gc, GLESMatrix *psMatrix);
IMG_VOID PickInvTransposeProcs(GLES1Context *gc, GLESMatrix *psMatrix);


IMG_VOID PushProgramMatrix(GLES1Context *gc);
IMG_VOID PopProgramMatrix(GLES1Context *gc);
IMG_VOID LoadIdentityProgramMatrix(GLES1Context *gc);




IMG_VOID SetupTransformLightingProcs(GLES1Context *gc);


IMG_BOOL InitLightingState(GLES1Context *gc);
IMG_VOID FreeLightingState(GLES1Context *gc);
IMG_BOOL InitTransformState(GLES1Context *gc);
IMG_VOID FreeTransformState(GLES1Context *gc);
IMG_VOID InitVertexArrayState(GLES1Context *gc);
IMG_VOID ApplyViewport(GLES1Context *gc);
IMG_VOID ApplyDepthRange(GLES1Context *gc, IMG_FLOAT fZNear, IMG_FLOAT fZFar);

#endif /* _XFORM_H_ */
