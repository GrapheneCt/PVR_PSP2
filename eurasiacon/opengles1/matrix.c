/******************************************************************************
 * Name         : matrix.c
 *
 * Copyright    : 2003-2008 by Imagination Technologies Limited.
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
 * $Log: matrix.c $
 *****************************************************************************/

#include "context.h"

/***********************************************************************************
 Function Name      : XForm2
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex with z=0,w=1 by a general matrix
************************************************************************************/
static IMG_VOID XForm2(GLEScoord *psRes, const IMG_FLOAT afV[2], const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];

	psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + psMatrix->afMatrix[3][0];
	psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + psMatrix->afMatrix[3][1];
	psRes->fZ = fX*psMatrix->afMatrix[0][2] + fY*psMatrix->afMatrix[1][2] + psMatrix->afMatrix[3][2];
	psRes->fW = fX*psMatrix->afMatrix[0][3] + fY*psMatrix->afMatrix[1][3] + psMatrix->afMatrix[3][3];
}


/***********************************************************************************
 Function Name      : XForm3
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex with w=1 by a general matrix
************************************************************************************/
static IMG_VOID XForm3(GLEScoord *psRes, const IMG_FLOAT afV[3], const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];
	IMG_FLOAT fZ = afV[2];

	psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] +
				fZ*psMatrix->afMatrix[2][0] + psMatrix->afMatrix[3][0];

	psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] +
				fZ*psMatrix->afMatrix[2][1] + psMatrix->afMatrix[3][1];

	psRes->fZ = fX*psMatrix->afMatrix[0][2] + fY*psMatrix->afMatrix[1][2] +
				fZ*psMatrix->afMatrix[2][2] + psMatrix->afMatrix[3][2];

	psRes->fW = fX*psMatrix->afMatrix[0][3] + fY*psMatrix->afMatrix[1][3] +
				fZ*psMatrix->afMatrix[2][3] + psMatrix->afMatrix[3][3];
}


/***********************************************************************************
 Function Name      : XForm4
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex by a general matrix
************************************************************************************/
static IMG_VOID XForm4(GLEScoord *psRes, const IMG_FLOAT afV[4], const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];
	IMG_FLOAT fZ = afV[2];
	IMG_FLOAT fW = afV[3];

	/* PRQA S 3341 1 */ /* 1.0f can be exactly expressed in binary using IEEE float format */
	if (fW == ((GLfloat) 1.0))
	{
		psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + fZ*psMatrix->afMatrix[2][0] + psMatrix->afMatrix[3][0];
		psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + fZ*psMatrix->afMatrix[2][1] + psMatrix->afMatrix[3][1];
		psRes->fZ = fX*psMatrix->afMatrix[0][2] + fY*psMatrix->afMatrix[1][2] + fZ*psMatrix->afMatrix[2][2] + psMatrix->afMatrix[3][2];
		psRes->fW = fX*psMatrix->afMatrix[0][3] + fY*psMatrix->afMatrix[1][3] + fZ*psMatrix->afMatrix[2][3] + psMatrix->afMatrix[3][3];
	}
	else
	{
		psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + fZ*psMatrix->afMatrix[2][0] + fW*psMatrix->afMatrix[3][0];
		psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + fZ*psMatrix->afMatrix[2][1] + fW*psMatrix->afMatrix[3][1];
		psRes->fZ = fX*psMatrix->afMatrix[0][2] + fY*psMatrix->afMatrix[1][2] + fZ*psMatrix->afMatrix[2][2] + fW*psMatrix->afMatrix[3][2];
		psRes->fW = fX*psMatrix->afMatrix[0][3] + fY*psMatrix->afMatrix[1][3] + fZ*psMatrix->afMatrix[2][3] + fW*psMatrix->afMatrix[3][3];
	}
}

/*
 * This compile flag would allow for optimised matrix transforms based on matrix type tracking -
 * currently disabled.
 */
#ifdef EXAMPLE_MATRIX_FN_PICKING
	/*
	 * The following is illustrative code demonstrating how depending on the
	 * type of matrix, different more optimal transform functions can be 
	 * selected when doing SW transformation and lighting
	 *
	 * Developers who wish to enable this type of functionality should replace 
	 * the define above with one of their own - and ensure the define has a vendor
	 * specific portion to its name
	 */

/***********************************************************************************
 Function Name      : XForm2_W
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex with z=0,w=1 by a matrix with
					  w column of [0 0 0 1].
************************************************************************************/
static IMG_VOID XForm2_W(GLEScoord *psRes, const IMG_FLOAT afV[2], const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];

	psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + psMatrix->afMatrix[3][0];
	psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + psMatrix->afMatrix[3][1];
	psRes->fZ = fX*psMatrix->afMatrix[0][2] + fY*psMatrix->afMatrix[1][2] + psMatrix->afMatrix[3][2];
	psRes->fW = ((IMG_FLOAT) 1.0);
}


/***********************************************************************************
 Function Name      : XForm3_W
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex with w=1 by a matrix with
					  w column of [0 0 0 1].
************************************************************************************/
static IMG_VOID XForm3_W(GLEScoord *psRes, const IMG_FLOAT afV[3], const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];
	IMG_FLOAT fZ = afV[2];

	psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + fZ*psMatrix->afMatrix[2][0] + psMatrix->afMatrix[3][0];
	psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + fZ*psMatrix->afMatrix[2][1] + psMatrix->afMatrix[3][1];
	psRes->fZ = fX*psMatrix->afMatrix[0][2] + fY*psMatrix->afMatrix[1][2] + fZ*psMatrix->afMatrix[2][2] + psMatrix->afMatrix[3][2];
	psRes->fW = ((IMG_FLOAT) 1.0);
}


/***********************************************************************************
 Function Name      : XForm4_W
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex by a matrix with
					  w column of [0 0 0 1].
************************************************************************************/
static IMG_VOID XForm4_W(GLEScoord *psRes, const IMG_FLOAT afV[4], const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];
	IMG_FLOAT fZ = afV[2];
	IMG_FLOAT fW = afV[3];

	if (fW == ((IMG_FLOAT) 1.0)) 
	{
		psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + fZ*psMatrix->afMatrix[2][0] + psMatrix->afMatrix[3][0];
		psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + fZ*psMatrix->afMatrix[2][1] + psMatrix->afMatrix[3][1];
		psRes->fZ = fX*psMatrix->afMatrix[0][2] + fY*psMatrix->afMatrix[1][2] + fZ*psMatrix->afMatrix[2][2] + psMatrix->afMatrix[3][2];
	}
	else
	{
		psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + fZ*psMatrix->afMatrix[2][0] + fW*psMatrix->afMatrix[3][0];
		psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + fZ*psMatrix->afMatrix[2][1] + fW*psMatrix->afMatrix[3][1];
		psRes->fZ = fX*psMatrix->afMatrix[0][2] + fY*psMatrix->afMatrix[1][2] + fZ*psMatrix->afMatrix[2][2] + fW*psMatrix->afMatrix[3][2];
	}
	psRes->fW = fW;
}


/***********************************************************************************
 Function Name      : XForm2_2DW
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex with z=0,w=1 by a matrix like
						 | . . 0 0 |
						 | . . 0 0 |
						 | 0 0 . 0 |
						 | . . . 1 |
************************************************************************************/
static IMG_VOID XForm2_2DW(GLEScoord *psRes, const IMG_FLOAT afV[2], const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];

	psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + psMatrix->afMatrix[3][0];
	psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + psMatrix->afMatrix[3][1];
	psRes->fZ = psMatrix->afMatrix[3][2];
	psRes->fW = ((IMG_FLOAT) 1.0);
}


/***********************************************************************************
 Function Name      : XForm3_2DW
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex with w=1 by a matrix like
						 | . . 0 0 |
						 | . . 0 0 |
						 | 0 0 . 0 |
						 | . . . 1 |
************************************************************************************/
static IMG_VOID XForm3_2DW(GLEScoord *psRes, const IMG_FLOAT afV[3], const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];
	IMG_FLOAT fZ = afV[2];

	psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + psMatrix->afMatrix[3][0];
	psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + psMatrix->afMatrix[3][1];
	psRes->fZ = fZ*psMatrix->afMatrix[2][2] + psMatrix->afMatrix[3][2];
	psRes->fW = ((IMG_FLOAT) 1.0);
}


/***********************************************************************************
 Function Name      : XForm4_2DW
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex by a matrix like
						 | . . 0 0 |
						 | . . 0 0 |
						 | 0 0 . 0 |
						 | . . . 1 |
************************************************************************************/
static IMG_VOID XForm4_2DW(GLEScoord *psRes, const IMG_FLOAT afV[4], const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];
	IMG_FLOAT fZ = afV[2];
	IMG_FLOAT fW = afV[3];

	if (fW == ((IMG_FLOAT) 1.0))
	{
		psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + psMatrix->afMatrix[3][0];
		psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + psMatrix->afMatrix[3][1];
		psRes->fZ = fZ*psMatrix->afMatrix[2][2] + psMatrix->afMatrix[3][2];
	}
	else
	{
		psRes->fX = fX*psMatrix->afMatrix[0][0] + fY*psMatrix->afMatrix[1][0] + fW*psMatrix->afMatrix[3][0];
		psRes->fY = fX*psMatrix->afMatrix[0][1] + fY*psMatrix->afMatrix[1][1] + fW*psMatrix->afMatrix[3][1];
		psRes->fZ = fZ*psMatrix->afMatrix[2][2] + fW*psMatrix->afMatrix[3][2];
	}
	psRes->fW = fW;
}


/***********************************************************************************
 Function Name      : XForm2_2DNRW
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex with z=0,w=1 by a matrix like
						 | . 0 0 0 |
						 | 0 . 0 0 |
						 | 0 0 . 0 |
						 | . . . 1 |
************************************************************************************/
static IMG_VOID XForm2_2DNRW(GLEScoord *psRes, const IMG_FLOAT afV[2], const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];

	psRes->fX = fX*psMatrix->afMatrix[0][0] + psMatrix->afMatrix[3][0];
	psRes->fY = fY*psMatrix->afMatrix[1][1] + psMatrix->afMatrix[3][1];
	psRes->fZ = psMatrix->afMatrix[3][2];
	psRes->fW = ((IMG_FLOAT) 1.0);
}


/***********************************************************************************
 Function Name      : XForm3_2DNRW
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex with w=1 by a matrix like
						 | . 0 0 0 |
						 | 0 . 0 0 |
						 | 0 0 . 0 |
						 | . . . 1 |
************************************************************************************/
static IMG_VOID XForm3_2DNRW(GLEScoord *psRes, const IMG_FLOAT afV[3],
							 const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];
	IMG_FLOAT fZ = afV[2];

	psRes->fX = fX*psMatrix->afMatrix[0][0] + psMatrix->afMatrix[3][0];
	psRes->fY = fY*psMatrix->afMatrix[1][1] + psMatrix->afMatrix[3][1];
	psRes->fZ = fZ*psMatrix->afMatrix[2][2] + psMatrix->afMatrix[3][2];
	psRes->fW = ((IMG_FLOAT) 1.0);
}


/***********************************************************************************
 Function Name      : XForm4_2DNRW
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex by a matrix like
						 | . 0 0 0 |
						 | 0 . 0 0 |
						 | 0 0 . 0 |
						 | . . . 1 |
************************************************************************************/
static IMG_VOID XForm4_2DNRW(GLEScoord *psRes, const IMG_FLOAT afV[4], 
							 const GLESMatrix *psMatrix)
{
	IMG_FLOAT fX = afV[0];
	IMG_FLOAT fY = afV[1];
	IMG_FLOAT fZ = afV[2];
	IMG_FLOAT fW = afV[3];

	if (fW == ((IMG_FLOAT) 1.0)) 
	{
		psRes->fX = fX*psMatrix->afMatrix[0][0] + psMatrix->afMatrix[3][0];
		psRes->fY = fY*psMatrix->afMatrix[1][1] + psMatrix->afMatrix[3][1];
		psRes->fZ = fZ*psMatrix->afMatrix[2][2] + psMatrix->afMatrix[3][2];
	}
	else
	{
		psRes->fX = fX*psMatrix->afMatrix[0][0] + fW*psMatrix->afMatrix[3][0];
		psRes->fY = fY*psMatrix->afMatrix[1][1] + fW*psMatrix->afMatrix[3][1];
		psRes->fZ = fZ*psMatrix->afMatrix[2][2] + fW*psMatrix->afMatrix[3][2];
	}

	psRes->fW = fW;
}


/***********************************************************************************
 Function Name      : XForm2_Identity
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex with z=0,w=1 by an identity matrix
************************************************************************************/
static IMG_VOID XForm2_Identity(GLEScoord *psRes, const IMG_FLOAT afV[2], const GLESMatrix *psMatrix)
{
	psRes->fX = afV[0];
	psRes->fY = afV[1];
	psRes->fZ = GLES1_Zero;
	psRes->fW = GLES1_One;
}


/***********************************************************************************
 Function Name      : XForm3_Identity
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex with w=1 by an identity matrix
************************************************************************************/
static IMG_VOID XForm3_Identity(GLEScoord *psRes, const IMG_FLOAT afV[3], 
								const GLESMatrix *psMatrix)
{
	psRes->fX = afV[0];
	psRes->fY = afV[1];
	psRes->fZ = afV[2];
	psRes->fW = GLES1_One;
}


/***********************************************************************************
 Function Name      : XForm4_Identity
 Inputs             : afV, psMatrix
 Outputs            : psRes
 Returns            : -
 Description        : Transforms an incoming vertex by an identity matrix
************************************************************************************/
static IMG_VOID XForm4_Identity(GLEScoord *psRes, const IMG_FLOAT afV[4], 
								const GLESMatrix *psMatrix)
{
	psRes->fX = afV[0];
	psRes->fY = afV[1];
	psRes->fZ = afV[2];
	psRes->fW = afV[3];
}


#endif

/***********************************************************************************
 Function Name      : PickMatrixProcs
 Inputs             : gc, psMatrix
 Outputs            : psMatrix
 Returns            : -
 Description        : Picks transform procs for a matrix depending on its type
************************************************************************************/

IMG_INTERNAL IMG_VOID PickMatrixProcs(GLES1Context *gc, GLESMatrix *psMatrix)
{
	PVR_UNREFERENCED_PARAMETER(gc);
#ifdef EXAMPLE_MATRIX_FN_PICKING
	/*
	 * The following is illustrative code demonstrating how depending on the
	 * type of matrix, different more optimal transform functions can be 
	 * selected when doing SW transformation and lighting
	 *
	 * Developers who wish to enable this type of functionality should replace
	 * the define above with one of their own - and ensure the define has a vendor
	 * specific portion to its name
	 */
	switch(psMatrix->eMatrixType) 
	{
		case GLES1_MT_GENERAL:
		{
			psMatrix->pfnXf2 = XForm2;
			psMatrix->pfnXf3 = XForm3;
			psMatrix->pfnXf4 = XForm4;

			break;
		}
		case GLES1_MT_W0001:
		{
			psMatrix->pfnXf2 = XForm2_W;
			psMatrix->pfnXf3 = XForm3_W;
			psMatrix->pfnXf4 = XForm4_W;

			break;
		}
		case GLES1_MT_IS2D:
		{
			psMatrix->pfnXf2 = XForm2_2DW;
			psMatrix->pfnXf3 = XForm3_2DW;
			psMatrix->pfnXf4 = XForm4_2DW;

			break;
		}
		case GLES1_MT_IS2DNR:
		case GLES1_MT_IS2DNRSC:
		{
			psMatrix->pfnXf2 = XForm2_2DNRW;
			psMatrix->pfnXf3 = XForm3_2DNRW;
			psMatrix->pfnXf4 = XForm4_2DNRW;

			break;
		}
		case GLES1_MT_IDENTITY:	/* probably never hit */
		{
			psMatrix->pfnXf2 = XForm2_Identity;
			psMatrix->pfnXf3 = XForm3_Identity;
			psMatrix->pfnXf4 = XForm4_Identity;

			break;
		}
	}
#else
	psMatrix->pfnXf2 = XForm2;
	psMatrix->pfnXf3 = XForm3;
	psMatrix->pfnXf4 = XForm4;
#endif
}


/***********************************************************************************
 Function Name      : PickInvTransposeProcs
 Inputs             : gc, psMatrix
 Outputs            : psMatrix
 Returns            : -
 Description        : Picks transform procs for a matrix depending on its type
************************************************************************************/
IMG_INTERNAL IMG_VOID PickInvTransposeProcs(GLES1Context *gc, GLESMatrix *psMatrix)
{
	PVR_UNREFERENCED_PARAMETER(gc);

	psMatrix->pfnXf4 = XForm4;

#ifdef EXAMPLE_MATRIX_FN_PICKING
	/*
	 * The following is illustrative code demonstrating how depending on the
	 * type of matrix, different more optimal transform functions can be 
	 * selected when doing SW transformation and lighting
	 *
	 * Developers who wish to enable this type of functionality should replace 
	 * the define above with one of their own - and ensure the define has a vendor
	 * specific portion to its name
	 */
	switch(psMatrix->eMatrixType)
	{
		case GLES1_MT_GENERAL:
		{
			psMatrix->pfnXf3 = XForm4;

			break;
		}
		case GLES1_MT_W0001:
		{
			psMatrix->pfnXf3 = XForm3_W;

			break;
		}
		case GLES1_MT_IS2D:
		{
			psMatrix->pfnXf3 = XForm3_2DW;

			break;
		}
		case GLES1_MT_IS2DNR:
		case GLES1_MT_IS2DNRSC:
		case GLES1_MT_IDENTITY:	/* probably never hit */
		{
			psMatrix->pfnXf3 = XForm3_2DNRW;

			break;
		}
	}
#else
	psMatrix->pfnXf3 = XForm4;
#endif
}


/***********************************************************************************
 Function Name      : TransposeMatrix
 Inputs             : psSrc
 Outputs            : psTranspose
 Returns            : -
 Description        : UTILITY: Computes the transpose of a matrix.
************************************************************************************/
IMG_INTERNAL IMG_VOID TransposeMatrix(GLESMatrix *psTranspose, const GLESMatrix *psSrc)
{
	psTranspose->afMatrix[0][0] = psSrc->afMatrix[0][0];
	psTranspose->afMatrix[0][1] = psSrc->afMatrix[1][0];
	psTranspose->afMatrix[0][2] = psSrc->afMatrix[2][0];
	psTranspose->afMatrix[0][3] = psSrc->afMatrix[3][0];
	psTranspose->afMatrix[1][0] = psSrc->afMatrix[0][1];
	psTranspose->afMatrix[1][1] = psSrc->afMatrix[1][1];
	psTranspose->afMatrix[1][2] = psSrc->afMatrix[2][1];
	psTranspose->afMatrix[1][3] = psSrc->afMatrix[3][1];
	psTranspose->afMatrix[2][0] = psSrc->afMatrix[0][2];
	psTranspose->afMatrix[2][1] = psSrc->afMatrix[1][2];
	psTranspose->afMatrix[2][2] = psSrc->afMatrix[2][2];
	psTranspose->afMatrix[2][3] = psSrc->afMatrix[3][2];
	psTranspose->afMatrix[3][0] = psSrc->afMatrix[0][3];
	psTranspose->afMatrix[3][1] = psSrc->afMatrix[1][3];
	psTranspose->afMatrix[3][2] = psSrc->afMatrix[2][3];
	psTranspose->afMatrix[3][3] = psSrc->afMatrix[3][3];

}


/***********************************************************************************
 Function Name      : ComputeInverseTranspose
 Inputs             : gc, psTransform
 Outputs            : -
 Returns            : -
 Description        : Puts the inverse transpose of a matrix in the 
					  transform->inverseTranspose slot, and computes the rescale 
					  factor value if applicable.
************************************************************************************/
IMG_INTERNAL IMG_VOID ComputeInverseTranspose(GLES1Context *gc, GLES1Transform *psTransform)
{
	(*gc->sProcs.sMatrixProcs.pfnInvertTranspose)(&psTransform->sInverseTranspose, &psTransform->sMatrix);

	if (gc->ui32TnLEnables & GLES1_TL_RESCALE_ENABLE) 
	{
		IMG_FLOAT  fFactor;
		GLESMatrix *psMatrix = &psTransform->sInverseTranspose;

		fFactor = GLES1_SQRTF(psMatrix->afMatrix[0][2]*psMatrix->afMatrix[0][2] +
							  psMatrix->afMatrix[1][2]*psMatrix->afMatrix[1][2] +
							  psMatrix->afMatrix[2][2]*psMatrix->afMatrix[2][2]);

		/* PRQA S 3341 1 */ /* GLES1_Zero can be exactly expressed in binary using IEEE float format */
		if (fFactor == GLES1_Zero)
		{
			psTransform->fRescaleFactor = GLES1_One;
		}
		else
		{
			psTransform->fRescaleFactor = GLES1_One / fFactor;
		}
	}

	psTransform->bUpdateInverse = IMG_FALSE;
}

/*
** inverse = invert(transpose(src))

This code uses Cramer's Rule to calculate the matrix inverse.
In general, the inverse transpose has this form:

[          ] -t    [                                   ]
[          ]       [             -t             -t t   ]
[  Q    P  ]       [   S(SQ - PT)     -(SQ - PT)  T    ]
[          ]       [                                   ]
[          ]       [                                   ]
[          ]    =  [                                   ]
[          ]       [        -1  t                      ]
[          ]       [     -(Q  P)             1         ]
[  T    S  ]       [   -------------   -------------   ]
[          ]       [         -1  t t         -1  t t   ]
[          ]       [   S - (Q  P) T    S - (Q  P) T    ]

But in the usual case that P,S == [0, 0, 0, 1], this is enough:

[          ] -t    [                                   ]
[          ]       [         -t              -t t      ]
[  Q    0  ]       [        Q              -Q  T       ]
[          ]       [                                   ]
[          ]       [                                   ]
[          ]    =  [                                   ]
[          ]       [                                   ]
[          ]       [                                   ]
[  T    1  ]       [        0                1         ]
[          ]       [                                   ]
[          ]       [                                   ]

*/

/***********************************************************************************
 Function Name      : InvertTransposeMatrix
 Inputs             : psSrc
 Outputs            : psInverse
 Returns            : -
 Description        : UTILITY: Computes the inverse transpose of a matrix.
************************************************************************************/
IMG_INTERNAL IMG_VOID InvertTransposeMatrix(GLESMatrix *psInverse, const GLESMatrix *psSrc)
{
	IMG_FLOAT fX00, fX01, fX02;
	IMG_FLOAT fX10, fX11, fX12;
	IMG_FLOAT fX20, fX21, fX22;
	IMG_FLOAT fRcp;

	/* propagate matrix type & branch if general */
	psInverse->eMatrixType = psSrc->eMatrixType;

	if(psInverse->eMatrixType) 
	{
		IMG_FLOAT fZ00, fZ01, fZ02;
		IMG_FLOAT fZ10, fZ11, fZ12;
		IMG_FLOAT fZ20, fZ21, fZ22;

		/* read 3x3 matrix into registers */
		fX00 = psSrc->afMatrix[0][0];
		fX01 = psSrc->afMatrix[0][1];
		fX02 = psSrc->afMatrix[0][2];
		fX10 = psSrc->afMatrix[1][0];
		fX11 = psSrc->afMatrix[1][1];
		fX12 = psSrc->afMatrix[1][2];
		fX20 = psSrc->afMatrix[2][0];
		fX21 = psSrc->afMatrix[2][1];
		fX22 = psSrc->afMatrix[2][2];

		/* compute first three 2x2 cofactors */
		fZ20 = fX01*fX12 - fX11*fX02;
		fZ10 = fX21*fX02 - fX01*fX22;
		fZ00 = fX11*fX22 - fX12*fX21;

		/* compute 3x3 determinant & its reciprocal */
		fRcp = fX20*fZ20 + fX10*fZ10 + fX00*fZ00;

		/* PRQA S 3341 1 */ /* GLES1_Zero can be exactly expressed in binary using IEEE float format */
		if (fRcp == GLES1_Zero)
		{
			return;
		}
		
		fRcp = GLES1_One/fRcp;

		/* compute other six 2x2 cofactors */
		fZ01 = fX20*fX12 - fX10*fX22;
		fZ02 = fX10*fX21 - fX20*fX11;
		fZ11 = fX00*fX22 - fX20*fX02;
		fZ12 = fX20*fX01 - fX00*fX21;
		fZ21 = fX10*fX02 - fX00*fX12;
		fZ22 = fX00*fX11 - fX10*fX01;

		/* multiply all cofactors by reciprocal */
		psInverse->afMatrix[0][0] = fZ00*fRcp;
		psInverse->afMatrix[0][1] = fZ01*fRcp;
		psInverse->afMatrix[0][2] = fZ02*fRcp;
		psInverse->afMatrix[1][0] = fZ10*fRcp;
		psInverse->afMatrix[1][1] = fZ11*fRcp;
		psInverse->afMatrix[1][2] = fZ12*fRcp;
		psInverse->afMatrix[2][0] = fZ20*fRcp;
		psInverse->afMatrix[2][1] = fZ21*fRcp;
		psInverse->afMatrix[2][2] = fZ22*fRcp;

		/* read translation vector & negate */
		fX00 = -psSrc->afMatrix[3][0];
		fX01 = -psSrc->afMatrix[3][1];
		fX02 = -psSrc->afMatrix[3][2];

		/* store bottom row of inverse transpose */
		psInverse->afMatrix[3][0] = GLES1_Zero;
		psInverse->afMatrix[3][1] = GLES1_Zero;
		psInverse->afMatrix[3][2] = GLES1_Zero;
		psInverse->afMatrix[3][3] = GLES1_One;

		/* finish by tranforming translation vector */
		psInverse->afMatrix[0][3] = psInverse->afMatrix[0][0]*fX00 +
			psInverse->afMatrix[0][1]*fX01 +
			psInverse->afMatrix[0][2]*fX02;
		psInverse->afMatrix[1][3] = psInverse->afMatrix[1][0]*fX00 +
			psInverse->afMatrix[1][1]*fX01 +
			psInverse->afMatrix[1][2]*fX02;
		psInverse->afMatrix[2][3] = psInverse->afMatrix[2][0]*fX00 +
			psInverse->afMatrix[2][1]*fX01 +
			psInverse->afMatrix[2][2]*fX02;
	}
	else
	{
		IMG_FLOAT fX30, fX31, fX32;
		IMG_FLOAT fY01, fY02, fY03, fY12, fY13, fY23;
		IMG_FLOAT fZ02, fZ03, fZ12, fZ13, fZ22, fZ23, fZ32, fZ33;

#define fX03 fX01
#define fX13 fX11
#define fX23 fX21
#define fX33 fX31
#define fZ00 fX02
#define fZ10 fX12
#define fZ20 fX22
#define fZ30 fX32
#define fZ01 fX03
#define fZ11 fX13
#define fZ21 fX23
#define fZ31 fX33

		/* read 1st two columns of matrix into registers */
		fX00 = psSrc->afMatrix[0][0];
		fX01 = psSrc->afMatrix[0][1];
		fX10 = psSrc->afMatrix[1][0];
		fX11 = psSrc->afMatrix[1][1];
		fX20 = psSrc->afMatrix[2][0];
		fX21 = psSrc->afMatrix[2][1];
		fX30 = psSrc->afMatrix[3][0];
		fX31 = psSrc->afMatrix[3][1];

		/* compute all six 2x2 determinants of 1st two columns */
		fY01 = fX00*fX11 - fX10*fX01;
		fY02 = fX00*fX21 - fX20*fX01;
		fY03 = fX00*fX31 - fX30*fX01;
		fY12 = fX10*fX21 - fX20*fX11;
		fY13 = fX10*fX31 - fX30*fX11;
		fY23 = fX20*fX31 - fX30*fX21;

		/* read 2nd two columns of matrix into registers */
		fX02 = psSrc->afMatrix[0][2];
		fX03 = psSrc->afMatrix[0][3];
		fX12 = psSrc->afMatrix[1][2];
		fX13 = psSrc->afMatrix[1][3];
		fX22 = psSrc->afMatrix[2][2];
		fX23 = psSrc->afMatrix[2][3];
		fX32 = psSrc->afMatrix[3][2];
		fX33 = psSrc->afMatrix[3][3];

		/* compute all 3x3 cofactors for 2nd two columns */
		fZ33 = fX02*fY12 - fX12*fY02 + fX22*fY01;
		fZ23 = fX12*fY03 - fX32*fY01 - fX02*fY13;
		fZ13 = fX02*fY23 - fX22*fY03 + fX32*fY02;
		fZ03 = fX22*fY13 - fX32*fY12 - fX12*fY23;
		fZ32 = fX13*fY02 - fX23*fY01 - fX03*fY12;
		fZ22 = fX03*fY13 - fX13*fY03 + fX33*fY01;
		fZ12 = fX23*fY03 - fX33*fY02 - fX03*fY23;
		fZ02 = fX13*fY23 - fX23*fY13 + fX33*fY12;

		/* compute all six 2x2 determinants of 2nd two columns */
		fY01 = fX02*fX13 - fX12*fX03;
		fY02 = fX02*fX23 - fX22*fX03;
		fY03 = fX02*fX33 - fX32*fX03;
		fY12 = fX12*fX23 - fX22*fX13;
		fY13 = fX12*fX33 - fX32*fX13;
		fY23 = fX22*fX33 - fX32*fX23;

		/* read 1st two columns of matrix into registers */
		fX00 = psSrc->afMatrix[0][0];
		fX01 = psSrc->afMatrix[0][1];
		fX10 = psSrc->afMatrix[1][0];
		fX11 = psSrc->afMatrix[1][1];
		fX20 = psSrc->afMatrix[2][0];
		fX21 = psSrc->afMatrix[2][1];
		fX30 = psSrc->afMatrix[3][0];
		fX31 = psSrc->afMatrix[3][1];

		/* compute all 3x3 cofactors for 1st column */
		fZ30 = fX11*fY02 - fX21*fY01 - fX01*fY12;
		fZ20 = fX01*fY13 - fX11*fY03 + fX31*fY01;
		fZ10 = fX21*fY03 - fX31*fY02 - fX01*fY23;
		fZ00 = fX11*fY23 - fX21*fY13 + fX31*fY12;

		/* compute 4fX4 determinant & its reciprocal */
		fRcp = fX30*fZ30 + fX20*fZ20 + fX10*fZ10 + fX00*fZ00;

		/* PRQA S 3341 1 */ /* GLES1_Zero can be exactly expressed in binary using IEEE float format */
		if (fRcp == GLES1_Zero)
		{
			return;
		}

		fRcp = GLES1_One/fRcp;

		/* compute all 3x3 cofactors for 2nd column */
		fZ31 = fX00*fY12 - fX10*fY02 + fX20*fY01;
		fZ21 = fX10*fY03 - fX30*fY01 - fX00*fY13;
		fZ11 = fX00*fY23 - fX20*fY03 + fX30*fY02;
		fZ01 = fX20*fY13 - fX30*fY12 - fX10*fY23;

		/* multiply all 3fX3 cofactors by reciprocal */
		psInverse->afMatrix[0][0] = fZ00*fRcp;
		psInverse->afMatrix[0][1] = fZ01*fRcp;
		psInverse->afMatrix[1][0] = fZ10*fRcp;
		psInverse->afMatrix[0][2] = fZ02*fRcp;
		psInverse->afMatrix[2][0] = fZ20*fRcp;
		psInverse->afMatrix[0][3] = fZ03*fRcp;
		psInverse->afMatrix[3][0] = fZ30*fRcp;
		psInverse->afMatrix[1][1] = fZ11*fRcp;
		psInverse->afMatrix[1][2] = fZ12*fRcp;
		psInverse->afMatrix[2][1] = fZ21*fRcp;
		psInverse->afMatrix[1][3] = fZ13*fRcp;
		psInverse->afMatrix[3][1] = fZ31*fRcp;
		psInverse->afMatrix[2][2] = fZ22*fRcp;
		psInverse->afMatrix[2][3] = fZ23*fRcp;
		psInverse->afMatrix[3][2] = fZ32*fRcp;
		psInverse->afMatrix[3][3] = fZ33*fRcp;
	}
}

/***********************************************************************************
 Function Name      : CopyMatrix
 Inputs             : psSrc
 Outputs            : psDst
 Returns            : -
 Description        : UTILITY: Compies a matrix from src to dst - preserves matrix type
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyMatrix(GLESMatrix *psDst, const GLESMatrix *psSrc)
{
	psDst->eMatrixType = psSrc->eMatrixType;
	psDst->afMatrix[0][0] = psSrc->afMatrix[0][0];
	psDst->afMatrix[0][1] = psSrc->afMatrix[0][1];
	psDst->afMatrix[0][2] = psSrc->afMatrix[0][2];
	psDst->afMatrix[0][3] = psSrc->afMatrix[0][3];

	psDst->afMatrix[1][0] = psSrc->afMatrix[1][0];
	psDst->afMatrix[1][1] = psSrc->afMatrix[1][1];
	psDst->afMatrix[1][2] = psSrc->afMatrix[1][2];
	psDst->afMatrix[1][3] = psSrc->afMatrix[1][3];

	psDst->afMatrix[2][0] = psSrc->afMatrix[2][0];
	psDst->afMatrix[2][1] = psSrc->afMatrix[2][1];
	psDst->afMatrix[2][2] = psSrc->afMatrix[2][2];
	psDst->afMatrix[2][3] = psSrc->afMatrix[2][3];

	psDst->afMatrix[3][0] = psSrc->afMatrix[3][0];
	psDst->afMatrix[3][1] = psSrc->afMatrix[3][1];
	psDst->afMatrix[3][2] = psSrc->afMatrix[3][2];
	psDst->afMatrix[3][3] = psSrc->afMatrix[3][3];
}


/***********************************************************************************
 Function Name      : MakeIdentity
 Inputs             : psMatrix
 Outputs            : psMatrix
 Returns            : -
 Description        : UTILITY: Makes a matrix the identity
************************************************************************************/
IMG_INTERNAL IMG_VOID MakeIdentity(GLESMatrix *psMatrix)
{
	IMG_FLOAT zer = GLES1_Zero;
	IMG_FLOAT one = GLES1_One;

	psMatrix->afMatrix[0][0] = one; psMatrix->afMatrix[0][1] = zer;
	psMatrix->afMatrix[0][2] = zer; psMatrix->afMatrix[0][3] = zer;
	psMatrix->afMatrix[1][0] = zer; psMatrix->afMatrix[1][1] = one;
	psMatrix->afMatrix[1][2] = zer; psMatrix->afMatrix[1][3] = zer;
	psMatrix->afMatrix[2][0] = zer; psMatrix->afMatrix[2][1] = zer;
	psMatrix->afMatrix[2][2] = one; psMatrix->afMatrix[2][3] = zer;
	psMatrix->afMatrix[3][0] = zer; psMatrix->afMatrix[3][1] = zer;
	psMatrix->afMatrix[3][2] = zer; psMatrix->afMatrix[3][3] = one;
	psMatrix->eMatrixType = GLES1_MT_IDENTITY;
}

/***********************************************************************************
 Function Name      : MultMatrix
 Inputs             : psSrcA, psSrcB
 Outputs            : psRes
 Returns            : -
 Description        : UTILITY: Multiplies two matrices together.
************************************************************************************/
IMG_INTERNAL IMG_VOID MultMatrix(GLESMatrix *psRes, const GLESMatrix *psSrcA, const GLESMatrix *psSrcB)
{
	IMG_FLOAT fB00, fB01, fB02, fB03;
	IMG_FLOAT fB10, fB11, fB12, fB13;
	IMG_FLOAT fB20, fB21, fB22, fB23;
	IMG_FLOAT fB30, fB31, fB32, fB33;
	GLint i;

	fB00 = psSrcB->afMatrix[0][0]; fB01 = psSrcB->afMatrix[0][1];
	fB02 = psSrcB->afMatrix[0][2]; fB03 = psSrcB->afMatrix[0][3];
	fB10 = psSrcB->afMatrix[1][0]; fB11 = psSrcB->afMatrix[1][1];
	fB12 = psSrcB->afMatrix[1][2]; fB13 = psSrcB->afMatrix[1][3];
	fB20 = psSrcB->afMatrix[2][0]; fB21 = psSrcB->afMatrix[2][1];
	fB22 = psSrcB->afMatrix[2][2]; fB23 = psSrcB->afMatrix[2][3];
	fB30 = psSrcB->afMatrix[3][0]; fB31 = psSrcB->afMatrix[3][1];
	fB32 = psSrcB->afMatrix[3][2]; fB33 = psSrcB->afMatrix[3][3];

	for (i = 0; i < 4; i++)
	{
		psRes->afMatrix[i][0] = psSrcA->afMatrix[i][0]*fB00 + psSrcA->afMatrix[i][1]*fB10	+ psSrcA->afMatrix[i][2]*fB20 + psSrcA->afMatrix[i][3]*fB30;
		psRes->afMatrix[i][1] = psSrcA->afMatrix[i][0]*fB01 + psSrcA->afMatrix[i][1]*fB11	+ psSrcA->afMatrix[i][2]*fB21 + psSrcA->afMatrix[i][3]*fB31;
		psRes->afMatrix[i][2] = psSrcA->afMatrix[i][0]*fB02 + psSrcA->afMatrix[i][1]*fB12	+ psSrcA->afMatrix[i][2]*fB22 + psSrcA->afMatrix[i][3]*fB32;
		psRes->afMatrix[i][3] = psSrcA->afMatrix[i][0]*fB03 + psSrcA->afMatrix[i][1]*fB13	+ psSrcA->afMatrix[i][2]*fB23 + psSrcA->afMatrix[i][3]*fB33;
	}
}


/***********************************************************************************
 Function Name      : DoMultMatrix
 Inputs             : gc, pvData, pfnMultiply
 Outputs            : -
 Returns            : -
 Description        : Multiplies two matrices together using multiply function.
					  Uses src matrix based on current eMatrixMode.
************************************************************************************/
IMG_INTERNAL IMG_VOID DoMultMatrix(GLES1Context *gc, IMG_VOID *pvData,
		IMG_VOID (*pfnMultiply)(GLES1Context *gc, GLESMatrix *psDstMatrix, GLESMatrix *psSrcMatrix, IMG_VOID *pvData))
{
	GLES1Transform *psTransform;

	switch (gc->sState.eMatrixMode) 
	{
		case GL_MODELVIEW:
		{
			psTransform = gc->sTransform.psModelView;

			(*pfnMultiply)(gc, &psTransform->sMatrix, &psTransform->sMatrix, pvData);

			(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

			psTransform->bUpdateInverse = IMG_TRUE;

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
		case GL_PROJECTION:
		{
			psTransform = gc->sTransform.psProjection;

			(*pfnMultiply)(gc, &psTransform->sMatrix, &psTransform->sMatrix, pvData);

			(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

			psTransform->bUpdateInverse = IMG_TRUE;

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
		case GL_TEXTURE:
		{
			psTransform = gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture];

			(*pfnMultiply)(gc, &psTransform->sMatrix, &psTransform->sMatrix, pvData);

			(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

			gc->ui32DirtyMask |= (GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS);

			break;
		}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_PALETTE_OES:
		{
			psTransform = &gc->sTransform.psMatrixPalette[gc->sState.sCurrent.ui32MatrixPaletteIndex];

			(*pfnMultiply)(gc, &psTransform->sMatrix, &psTransform->sMatrix, pvData);

			(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

			psTransform->bUpdateInverse = IMG_TRUE;

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
	}
}


/***********************************************************************************
 Function Name      : DoLoadMatrix
 Inputs             : gc, psMatrix
 Outputs            : -
 Returns            : -
 Description        : Loads a src matrix based on current eMatrixMode with data.
************************************************************************************/
IMG_INTERNAL IMG_VOID DoLoadMatrix(GLES1Context *gc, const GLESMatrix *psMatrix)
{
	GLES1Transform *psTransform;

	PVR_UNREFERENCED_PARAMETER(psMatrix);

	switch (gc->sState.eMatrixMode) 
	{
		case GL_MODELVIEW:
		{
			psTransform = gc->sTransform.psModelView;

			(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

			psTransform->bUpdateInverse = IMG_TRUE;

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
		case GL_PROJECTION:
		{
			psTransform = gc->sTransform.psProjection;

			(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

			psTransform->bUpdateInverse = IMG_TRUE;

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
		case GL_TEXTURE:
		{
			psTransform = gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture];

			(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

			gc->ui32DirtyMask |= (GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS);

			break;
		}
#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
		case GL_MATRIX_PALETTE_OES:
		{
			psTransform = &gc->sTransform.psMatrixPalette[gc->sState.sCurrent.ui32MatrixPaletteIndex];

			(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

			psTransform->bUpdateInverse = IMG_TRUE;

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

			break;
		}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */
	}
}

/*
** Assuming that psSrcA->eMatrixType and psSrcB->eMatrixType are already correct,
** and dst = a * b, then compute dst's matrix type.
*/

/***********************************************************************************
 Function Name      : PickMatrixType
 Inputs             : psSrcA, psSrcB
 Outputs            : psDst
 Returns            : -
 Description        : Assuming that psSrcA->eMatrixType and psSrcB->eMatrixType are already correct,
					  and dst = a * b, then compute dst's matrix type.
************************************************************************************/
static IMG_VOID PickMatrixType(GLESMatrix *psDst, GLESMatrix *psSrcA, GLESMatrix *psSrcB)
{
	switch(psSrcA->eMatrixType)
	{
		case GLES1_MT_GENERAL:
		{
			psDst->eMatrixType = psSrcA->eMatrixType;

			break;
		}
		case GLES1_MT_W0001:
		{
			if(psSrcB->eMatrixType == GLES1_MT_GENERAL) 
			{
				psDst->eMatrixType = psSrcB->eMatrixType;
			}
			else 
			{
				psDst->eMatrixType = psSrcA->eMatrixType;
			}

			break;
		}
		case GLES1_MT_IS2D:
		{
			if(psSrcB->eMatrixType < GLES1_MT_IS2D) 
			{
				psDst->eMatrixType = psSrcB->eMatrixType;
			}
			else
			{
				psDst->eMatrixType = psSrcA->eMatrixType;
			}

			break;
		}
		case GLES1_MT_IS2DNR:
		{
			if(psSrcB->eMatrixType < GLES1_MT_IS2DNR) 
			{
				psDst->eMatrixType = psSrcB->eMatrixType;
			}
			else
			{
				psDst->eMatrixType = psSrcA->eMatrixType;
			}

			break;
		}
		case GLES1_MT_IDENTITY:
		{
			if(psSrcB->eMatrixType == GLES1_MT_IS2DNRSC) 
			{
				psDst->ui32Width = psSrcB->ui32Width;
				psDst->ui32Height = psSrcB->ui32Height;
			}

			psDst->eMatrixType = psSrcB->eMatrixType;

			break;
		}
		case GLES1_MT_IS2DNRSC:
		{
			if(psSrcB->eMatrixType == GLES1_MT_IDENTITY) 
			{
				psDst->eMatrixType = GLES1_MT_IS2DNRSC;
				psDst->ui32Width = psSrcA->ui32Width;
				psDst->ui32Height = psSrcA->ui32Height;
			}
			else if(psSrcB->eMatrixType < GLES1_MT_IS2DNR) 
			{
				psDst->eMatrixType = psSrcB->eMatrixType;
			}
			else
			{
				psDst->eMatrixType = GLES1_MT_IS2DNR;
			}

			break;
		}
	}
}


/***********************************************************************************
 Function Name      : MultiplyMatrix
 Inputs             : psSrcMatrix, pvData
 Outputs            : psDstMatrix
 Returns            : -
 Description        : Multiply the first matrix by the second one keeping track of 
					  the matrix type of the newly combined matrix.
************************************************************************************/
IMG_INTERNAL IMG_VOID MultiplyMatrix(GLES1Context *gc, GLESMatrix *psDstMatrix, GLESMatrix *psSrcMatrix, IMG_VOID *pvData)
{
	GLESMatrix *psTempMatrix;

	psTempMatrix = pvData;

	(*gc->sProcs.sMatrixProcs.pfnMult)(psDstMatrix, psTempMatrix, psSrcMatrix);

	PickMatrixType(psDstMatrix, psTempMatrix, psSrcMatrix);
}


struct GLESScaleRec
{
	IMG_FLOAT fX,fY,fZ;
};


/***********************************************************************************
 Function Name      : ScaleMatrix
 Inputs             : gc, psSrcMatrix, pvData
 Outputs            : psDstMatrix
 Returns            : -
 Description        : Scale a matrix by a scalevector.
************************************************************************************/
IMG_INTERNAL IMG_VOID ScaleMatrix(GLES1Context *gc, GLESMatrix *psDstMatrix, GLESMatrix *psSrcMatrix, IMG_VOID *pvData)
{
	struct GLESScaleRec *psScale;
	IMG_FLOAT fX,fY,fZ;
	IMG_FLOAT fM0, fM1, fM2, fM3;

	PVR_UNREFERENCED_PARAMETER(gc);

	if (psSrcMatrix->eMatrixType > GLES1_MT_IS2DNR)
	{
		psDstMatrix->eMatrixType = GLES1_MT_IS2DNR;
	}

	psScale = pvData;

	fX = psScale->fX;
	fY = psScale->fY;
	fZ = psScale->fZ;

	fM0 = fX * psSrcMatrix->afMatrix[0][0];
	fM1 = fX * psSrcMatrix->afMatrix[0][1];
	fM2 = fX * psSrcMatrix->afMatrix[0][2];
	fM3 = fX * psSrcMatrix->afMatrix[0][3];
	psDstMatrix->afMatrix[0][0] = fM0;
	psDstMatrix->afMatrix[0][1] = fM1;
	psDstMatrix->afMatrix[0][2] = fM2;
	psDstMatrix->afMatrix[0][3] = fM3;

	fM0 = fY * psSrcMatrix->afMatrix[1][0];
	fM1 = fY * psSrcMatrix->afMatrix[1][1];
	fM2 = fY * psSrcMatrix->afMatrix[1][2];
	fM3 = fY * psSrcMatrix->afMatrix[1][3];
	psDstMatrix->afMatrix[1][0] = fM0;
	psDstMatrix->afMatrix[1][1] = fM1;
	psDstMatrix->afMatrix[1][2] = fM2;
	psDstMatrix->afMatrix[1][3] = fM3;

	fM0 = fZ * psSrcMatrix->afMatrix[2][0];
	fM1 = fZ * psSrcMatrix->afMatrix[2][1];
	fM2 = fZ * psSrcMatrix->afMatrix[2][2];
	fM3 = fZ * psSrcMatrix->afMatrix[2][3];
	psDstMatrix->afMatrix[2][0] = fM0;
	psDstMatrix->afMatrix[2][1] = fM1;
	psDstMatrix->afMatrix[2][2] = fM2;
	psDstMatrix->afMatrix[2][3] = fM3;
}


struct GLESTranslationRec
{
	IMG_FLOAT fX,fY,fZ;
};


/***********************************************************************************
 Function Name      : TranslateMatrix
 Inputs             : gc, psSrcMatrix, pvData
 Outputs            : psDstMatrix
 Returns            : -
 Description        : Translate a matrix by a translation vector.
************************************************************************************/
IMG_INTERNAL IMG_VOID TranslateMatrix(GLES1Context *gc, GLESMatrix *psDstMatrix, GLESMatrix *psSrcMatrix, IMG_VOID *pvData)
{
	struct GLESTranslationRec *psTrans;
	IMG_FLOAT fX,fY,fZ;
	IMG_FLOAT fM30, fM31, fM32, fM33;

	PVR_UNREFERENCED_PARAMETER(gc);

	if (psSrcMatrix->eMatrixType > GLES1_MT_IS2DNR) 
	{
		psDstMatrix->eMatrixType = GLES1_MT_IS2DNR;
	}

	psTrans = pvData;

	fX = psTrans->fX;
	fY = psTrans->fY;
	fZ = psTrans->fZ;

	fM30 =	fX * psSrcMatrix->afMatrix[0][0] + fY * psSrcMatrix->afMatrix[1][0] + fZ * 
			psSrcMatrix->afMatrix[2][0] + psSrcMatrix->afMatrix[3][0];

	fM31 =	fX * psSrcMatrix->afMatrix[0][1] + fY * psSrcMatrix->afMatrix[1][1] + fZ * 
			psSrcMatrix->afMatrix[2][1] + psSrcMatrix->afMatrix[3][1];

	fM32 =	fX * psSrcMatrix->afMatrix[0][2] + fY * psSrcMatrix->afMatrix[1][2] + fZ * 
			psSrcMatrix->afMatrix[2][2] + psSrcMatrix->afMatrix[3][2];

	fM33 =	fX * psSrcMatrix->afMatrix[0][3] + fY * psSrcMatrix->afMatrix[1][3] + fZ * 
			psSrcMatrix->afMatrix[2][3] + psSrcMatrix->afMatrix[3][3];

	psDstMatrix->afMatrix[3][0] = fM30;
	psDstMatrix->afMatrix[3][1] = fM31;
	psDstMatrix->afMatrix[3][2] = fM32;
	psDstMatrix->afMatrix[3][3] = fM33;
}


/***********************************************************************************
 Function Name      : PushModelViewMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Pushes the current modelview matrix on the stack
************************************************************************************/
IMG_INTERNAL IMG_VOID PushModelViewMatrix(GLES1Context *gc)
{
	GLES1Transform **ppsTransform, *psTransform, *psStack;

	ppsTransform = &gc->sTransform.psModelView;
	psStack = gc->sTransform.psModelViewStack;
	psTransform = *ppsTransform;

	if (psTransform < &psStack[GLES1_MAX_MODELVIEW_STACK_DEPTH-1]) 
	{
		psTransform[1] = psTransform[0];
		*ppsTransform = psTransform + 1;
	}
	else
	{
		SetError(gc, GL_STACK_OVERFLOW);
	}

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
}


/***********************************************************************************
 Function Name      : PopModelViewMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Pops the top off the modelview matrix stack
************************************************************************************/
IMG_INTERNAL IMG_VOID PopModelViewMatrix(GLES1Context *gc)
{
	GLES1Transform **ppsTransform, *psTransform, *psStack;

	ppsTransform = &gc->sTransform.psModelView;
	psStack = gc->sTransform.psModelViewStack;
	psTransform = *ppsTransform;

	if (psTransform > &psStack[0])
	{
		*ppsTransform = psTransform - 1;
	}
	else
	{
		SetError(gc, GL_STACK_UNDERFLOW);

		return;
	}

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
}


/***********************************************************************************
 Function Name      : LoadIdentityModelViewMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Loads the current modelview matrix with the identity.
************************************************************************************/
IMG_INTERNAL IMG_VOID LoadIdentityModelViewMatrix(GLES1Context *gc)
{
	GLES1Transform *psTransform;

	psTransform = gc->sTransform.psModelView;

	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sMatrix);

	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sInverseTranspose);

	(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

	(*gc->sProcs.pfnPickInvTransposeProcs)(gc, &psTransform->sInverseTranspose);

	psTransform->bUpdateInverse = IMG_FALSE;

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
}


#if defined(GLES1_EXTENSION_MATRIX_PALETTE)

/***********************************************************************************
 Function Name      : PushMatrixPaletteMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Pushes the current matrix palette matrix on the stack
************************************************************************************/
IMG_INTERNAL IMG_VOID PushMatrixPaletteMatrix(GLES1Context *gc)
{
	/*
	// There's no stack so should this produce an overflow error?
	*/
	PVR_UNREFERENCED_PARAMETER(gc);
}


/***********************************************************************************
 Function Name      : PopMatrixPaletteMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Pops the top off the matrix palette matrix stack
************************************************************************************/
IMG_INTERNAL IMG_VOID PopMatrixPaletteMatrix(GLES1Context *gc)
{
	/*
	// There's no stack so should this produce an underflow error?
	*/
	PVR_UNREFERENCED_PARAMETER(gc);
}


/***********************************************************************************
 Function Name      : LoadIdentityMatrixPaletteMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Loads the current matrix palette matrix with the identity.
************************************************************************************/
IMG_INTERNAL IMG_VOID LoadIdentityMatrixPaletteMatrix(GLES1Context *gc)
{
	GLES1Transform *psTransform;

	psTransform = &gc->sTransform.psMatrixPalette[gc->sState.sCurrent.ui32MatrixPaletteIndex];

	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sMatrix);

	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sInverseTranspose);

	(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

	(*gc->sProcs.pfnPickInvTransposeProcs)(gc, &psTransform->sInverseTranspose);

	psTransform->bUpdateInverse = IMG_FALSE;

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
}

#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */

/***********************************************************************************
 Function Name      : PushProjectionMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Pushes the current projection matrix on the stack
************************************************************************************/
IMG_INTERNAL IMG_VOID PushProjectionMatrix(GLES1Context *gc)
{
	GLES1Transform **ppsTransform, *psTransform, *psStack;

	ppsTransform = &gc->sTransform.psProjection;
	psStack = gc->sTransform.psProjectionStack;
	psTransform = *ppsTransform;

	if (psTransform < &psStack[GLES1_MAX_PROJECTION_STACK_DEPTH-1]) 
	{
		psTransform[1].sMatrix = psTransform[0].sMatrix;
		*ppsTransform = psTransform + 1;
	}
	else
	{
		SetError(gc, GL_STACK_OVERFLOW);
	}

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
}


/***********************************************************************************
 Function Name      : PopProjectionMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Pops the top off the projection matrix stack
************************************************************************************/
IMG_INTERNAL IMG_VOID PopProjectionMatrix(GLES1Context *gc)
{
	GLES1Transform **ppsTransform, *psTransform, *psStack;

	ppsTransform = &gc->sTransform.psProjection;
	psStack = gc->sTransform.psProjectionStack;
	psTransform = *ppsTransform;

	if (psTransform > &psStack[0]) 
	{
		*ppsTransform = psTransform - 1;
	}
	else
	{
		SetError(gc, GL_STACK_UNDERFLOW);

		return;
	}

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
}


/***********************************************************************************
 Function Name      : LoadIdentityProjectionMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Loads the current projection matrix with the identity.
************************************************************************************/
IMG_INTERNAL IMG_VOID LoadIdentityProjectionMatrix(GLES1Context *gc)
{
	GLES1Transform *psTransform;

	psTransform = gc->sTransform.psProjection;

	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sMatrix);

	(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

	(*gc->sProcs.pfnPickInvTransposeProcs)(gc, &psTransform->sInverseTranspose);

	psTransform->bUpdateInverse = IMG_FALSE;

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;
}


/***********************************************************************************
 Function Name      : PushTextureMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Pushes the current texture matrix on the stack
************************************************************************************/
IMG_INTERNAL IMG_VOID PushTextureMatrix(GLES1Context *gc)
{
	GLES1Transform **ppsTransform, *psTransform, *psStack;

	ppsTransform = &gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture];
	psStack = gc->sTransform.apsTextureStack[gc->sState.sTexture.ui32ActiveTexture];
	psTransform = *ppsTransform;

	if (psTransform < &psStack[GLES1_MAX_TEXTURE_STACK_DEPTH-1]) 
	{
		psTransform[1].sMatrix = psTransform[0].sMatrix;
		*ppsTransform = psTransform + 1;
	}
	else
	{
		SetError(gc, GL_STACK_OVERFLOW);
	}

	gc->ui32DirtyMask |= (GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS);
}


/***********************************************************************************
 Function Name      : PopTextureMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Pops the top off the texture matrix stack
************************************************************************************/
IMG_INTERNAL IMG_VOID PopTextureMatrix(GLES1Context *gc)
{
	GLES1Transform **ppsTransform, *psTransform, *psStack;

	ppsTransform = &gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture];
	psStack = gc->sTransform.apsTextureStack[gc->sState.sTexture.ui32ActiveTexture];
	psTransform = *ppsTransform;

	if (psTransform > &psStack[0])
	{
		*ppsTransform = psTransform - 1;
	}
	else
	{
		SetError(gc, GL_STACK_UNDERFLOW);
		return;
	}
	
	gc->ui32DirtyMask |= (GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS);
}


/***********************************************************************************
 Function Name      : LoadIdentityTextureMatrix
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Loads the current texture matrix with the identity.
************************************************************************************/
IMG_INTERNAL IMG_VOID LoadIdentityTextureMatrix(GLES1Context *gc)
{
	GLES1Transform *psTransform = gc->sTransform.apsTexture[gc->sState.sTexture.ui32ActiveTexture];

	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sMatrix);
	(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);

	gc->ui32DirtyMask |= (GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS);
}

/***********************************************************************************
 Function Name      : InitTransformState
 Inputs             : gc
 Outputs            : -
 Returns            : Success
 Description        : Initialises transform machine and state
************************************************************************************/

IMG_INTERNAL IMG_BOOL InitTransformState(GLES1Context *gc)
{
	IMG_UINT32 i;
	GLES1Transform *psTransform;

	GLEScoord sFrontBackClip[2] = {	{  0.0f,  0.0f,   1.0f,  1.0f },
									{  0.0f,  0.0f,  -1.0f,  1.0f }};

	/* Allocate memory for matrix stacks */
	gc->sTransform.psModelViewStack = (GLES1Transform *)GLES1Calloc(gc, GLES1_MAX_MODELVIEW_STACK_DEPTH * sizeof(GLES1Transform));

	if (!gc->sTransform.psModelViewStack)
	{
		FreeTransformState(gc);

		return IMG_FALSE;
	}

#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
	gc->sTransform.psMatrixPalette = (GLES1Transform *)GLES1Calloc(gc, GLES1_MAX_PALETTE_MATRICES * sizeof(GLES1Transform));

	if (!gc->sTransform.psMatrixPalette)
	{
		FreeTransformState(gc);

		return IMG_FALSE;
	}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */

	gc->sTransform.psProjectionStack = (GLES1Transform *)GLES1Calloc(gc, GLES1_MAX_PROJECTION_STACK_DEPTH * sizeof(GLES1Transform));

	if (!gc->sTransform.psProjectionStack)
	{
		FreeTransformState(gc);

		return IMG_FALSE;
	}

	for (i=0; i<GLES1_MAX_TEXTURE_UNITS; i++) 
	{
		gc->sTransform.apsTextureStack[i] = (GLES1Transform *)GLES1Calloc(gc, GLES1_MAX_TEXTURE_STACK_DEPTH * sizeof(GLES1Transform));

		if (!gc->sTransform.apsTextureStack[i])
		{
			FreeTransformState(gc);

			return IMG_FALSE;
		}
	}

	/*
	** Allocate memory for clipping temporaries.
	** Each plane can potentially add two temporary vertices, even though
	** the overall number of vertices can only increase by one per plane.
	*/
	gc->sState.eMatrixMode = GL_MODELVIEW;

	gc->sState.sViewport.fZNear = GLES1_Zero;
	gc->sState.sViewport.fZFar = GLES1_One;

	gc->sState.sViewport.fZScale  = GLES1_Half;
	gc->sState.sViewport.fZCenter = GLES1_Half;

	gc->sState.sViewport.sFrontBackClip[0] = sFrontBackClip[0];
	gc->sState.sViewport.sFrontBackClip[1] = sFrontBackClip[1];


	gc->sTransform.psModelView = psTransform = &gc->sTransform.psModelViewStack[0];
	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sMatrix);
	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sInverseTranspose);
	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sMvp);
	(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);
	(*gc->sProcs.pfnPickInvTransposeProcs)(gc, &psTransform->sInverseTranspose);
	psTransform->bUpdateInverse = IMG_FALSE;
/*	__GL_DELAY_VALIDATE_MASK(gc, __GL_DIRTY_MODELVIEW);*/

#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
	for(i=0; i<GLES1_MAX_PALETTE_MATRICES; i++)
	{
		psTransform = &gc->sTransform.psMatrixPalette[i];
		(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sMatrix);
		(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sInverseTranspose);
		(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sMvp);
		(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);
		(*gc->sProcs.pfnPickInvTransposeProcs)(gc, &psTransform->sInverseTranspose);
		psTransform->bUpdateInverse = IMG_FALSE;
	}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */

	gc->sTransform.psProjection = psTransform = &gc->sTransform.psProjectionStack[0];
	(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sMatrix);
	(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);
/*	(*gc->sProcs.pfnpickMvpMatrixProcs)(gc, &gc->sTransform.psModelView->mvp);*/

	for (i=0; i<GLES1_MAX_TEXTURE_UNITS; ++i)
	{
		gc->sTransform.apsTexture[i] = psTransform = &gc->sTransform.apsTextureStack[i][0];
		(*gc->sProcs.sMatrixProcs.pfnMakeIdentity)(&psTransform->sMatrix);
		(*gc->sProcs.pfnPickMatrixProcs)(gc, &psTransform->sMatrix);
	}

	gc->sState.sCurrent.asAttrib[AP_NORMAL].fZ = GLES1_One;

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : FreeTransformState
 Inputs             : gc
 Outputs            : -
 Returns            : -
 Description        : Frees transform machine and state
************************************************************************************/
IMG_INTERNAL IMG_VOID FreeTransformState(GLES1Context *gc)
{
	IMG_UINT32 i;

	if(gc->sTransform.psModelViewStack)
	{
		GLES1Free(gc, gc->sTransform.psModelViewStack);

		gc->sTransform.psModelViewStack = IMG_NULL;
	}

#if defined(GLES1_EXTENSION_MATRIX_PALETTE)
	if (gc->sTransform.psMatrixPalette)
	{
		GLES1Free(gc, gc->sTransform.psMatrixPalette);

		gc->sTransform.psMatrixPalette = IMG_NULL;
	}
#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */

	if(gc->sTransform.psProjectionStack)
	{
		GLES1Free(gc, gc->sTransform.psProjectionStack);

		gc->sTransform.psProjectionStack = IMG_NULL;
	}

	for (i=0; i<GLES1_MAX_TEXTURE_UNITS; i++)
	{
		if(gc->sTransform.apsTextureStack[i])
		{
			GLES1Free(gc, gc->sTransform.apsTextureStack[i]);

			gc->sTransform.apsTextureStack[i] = IMG_NULL;
		}
	}
}


#if defined(GLES1_EXTENSION_MATRIX_PALETTE)

/***********************************************************************************
 Function Name      : glCurrentPaletteMatrixOES
 Inputs             : matrixpaletteindex
 Outputs            : None
 Returns            : None
 Description        : Sets the current matrix palette index
************************************************************************************/
GL_API_EXT void GL_APIENTRY glCurrentPaletteMatrixOES(GLuint matrixpaletteindex)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glCurrentPaletteMatrixOES"));

	GLES1_TIME_START(GLES1_TIMES_glCurrentPaletteMatrixOES);

	if (matrixpaletteindex >= GLES1_MAX_PALETTE_MATRICES)
	{
		SetError(gc, GL_INVALID_VALUE);
	}
	else
	{
		IMG_UINT32 ui32MaxMatrixPaletteIndex;

		gc->sState.sCurrent.ui32MatrixPaletteIndex = matrixpaletteindex;

		ui32MaxMatrixPaletteIndex = MAX(gc->sPrim.ui32MaxMatrixPaletteIndex, matrixpaletteindex);

		if(gc->sPrim.ui32MaxMatrixPaletteIndex!=ui32MaxMatrixPaletteIndex)
		{
			gc->sPrim.ui32MaxMatrixPaletteIndex = ui32MaxMatrixPaletteIndex;

			gc->ui32DirtyMask |= (GLES1_DIRTYFLAG_VERTEX_PROGRAM | GLES1_DIRTYFLAG_VERTPROG_CONSTANTS);
		}
	}


	GLES1_TIME_STOP(GLES1_TIMES_glCurrentPaletteMatrixOES);
}


/***********************************************************************************
 Function Name      : glLoadPaletteFromModelViewMatrixOES
 Inputs             : None
 Outputs            : None
 Returns            : None
 Description        : Copies the model view matrix into the matrix palette indexed
					  using the current matrix palette index
************************************************************************************/
GL_API_EXT void GL_APIENTRY glLoadPaletteFromModelViewMatrixOES(void)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glLoadPaletteFromModelViewMatrixOES"));

	GLES1_TIME_START(GLES1_TIMES_glLoadPaletteFromModelViewMatrixOES);

	gc->sTransform.psMatrixPalette[gc->sState.sCurrent.ui32MatrixPaletteIndex] = *gc->sTransform.psModelView;

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_VERTPROG_CONSTANTS;

	GLES1_TIME_STOP(GLES1_TIMES_glLoadPaletteFromModelViewMatrixOES);
}

#endif /* defined(GLES1_EXTENSION_MATRIX_PALETTE) */


/******************************************************************************
 End of file (matrix.c)
******************************************************************************/

