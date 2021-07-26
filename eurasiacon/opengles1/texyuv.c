/******************************************************************************
 * Name         : texyuv.c
 * Author       : Jonathan Hamilton
 * Created      : 27/09/2010
 *
 * Copyright    : 2007-2010 by Imagination Technologies Limited.
 *              : All rights reserved. No part of this software, either
 *              : material or conceptual may be copied or distributed,
 *              : transmitted, transcribed, stored in a retrieval system or
 *              : translated into any human or computer language in any form
 *              : by any means,electronic, mechanical, manual or otherwise, 
 *              : or disclosed to third parties without the express written
 *              : permission of Imagination Technologies Limited, 
 *              : Home Park Estate, Kings Langley, Hertfordshire,
 *              : WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Description	: YUV texture support functions, based on texstream.c r1.50
 *
 * Modifications:-
 * $Log: texyuv.c $
 **************************************************************************/

#include "context.h"

#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)

static IMG_VOID SetupYUVCoefficients(YUV_REGISTERS *psYUVRegisters, PVRSRV_PIXEL_FORMAT ePixelFormat, YUV_CONVERT_COEFFS *psCoeffs);

#if defined(GENERATE_YUV_COEFFICIENTS)

typedef struct YUV_CSC_COEFF_TAG
{
	double rY,rCb,rCr;
	double gY,gCb,gCr;
	double bY,bCb,bCr;
}YUV_CSC_COEFF;

#define ROUND(x) (x>=0 ? x+0.5 : x-0.5)


/***********************************************************************************
 Function Name      : CheckCoeffs
 Inputs             : Ycoeff, Ucoeff , Vcoeff, ConstantTerm
 Outputs            : 
 Returns            : IMG_BOOL
 Description        : Checks if the specified coefficients are within the ranges required 
					  and returns true if they are else false.
************************************************************************************/
static IMG_BOOL CheckCoeffs(IMG_DOUBLE Ycoeff, IMG_DOUBLE Ucoeff, IMG_DOUBLE Vcoeff, IMG_DOUBLE ConstantTerm, IMG_UINT8 nBitY, IMG_UINT8 nBitU, IMG_UINT8 nBitV, IMG_UINT32 nBitConstant)
{
	if((IMG_INT32)ROUND(Ycoeff) > (1<<(nBitY-1))-1 || (IMG_INT32)ROUND(Ycoeff)<-(1<<(nBitY-1)))	//s8
	{
		return IMG_TRUE;
	}
	if(((IMG_INT32)ROUND(Ucoeff)>(1<<(nBitU-1))-1) || ((IMG_INT32)ROUND(Ucoeff)<-(1<<(nBitU-1))))	// s8
	{
		return IMG_TRUE;
	}
	if(((IMG_INT32)ROUND(Vcoeff)>(1<<(nBitV-1))-1) || ((IMG_INT32)ROUND(Vcoeff)<-(1<<(nBitV-1))))
	{
		return IMG_TRUE;
	}
	if((IMG_INT32)ROUND(ConstantTerm) > (1<<(nBitConstant-1))-1 || (IMG_INT32)ROUND(ConstantTerm)<-(1<<(nBitConstant-1)))	//s8
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}


/***********************************************************************************
 Function Name      : ScaleTransferMatrix
 Inputs             : *sTransferMatrix, YColumScale, CbColumScale, CrColumnScale
 Outputs            :  
 Returns            : void
 Description        : Scales the tranfer matrix depending on the input/output
					  nominal ranges.
************************************************************************************/
static IMG_VOID ScaleTransferMatrix(YUV_CSC_COEFF *psTransferMatrix, double YColumScale, double CbColumScale, double CrColumnScale)
{
	/* First column of the transfer matrix */
	psTransferMatrix->rY *= YColumScale;
	psTransferMatrix->gY *= YColumScale;
	psTransferMatrix->bY *= YColumScale;

	/* Second column of the transfer matrix */
	psTransferMatrix->rCb *= CbColumScale;
	psTransferMatrix->gCb *= CbColumScale;
	psTransferMatrix->bCb *= CbColumScale;

	/* Third column of the transfer matrix */
	psTransferMatrix->rCr *= CrColumnScale;
	psTransferMatrix->gCr *= CrColumnScale;
	psTransferMatrix->bCr *= CrColumnScale;
}

#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)

/***********************************************************************************
 Function Name      : ConvertCoeffs
 Inputs             : Ycoeff, Ucoeff, Vcoeff, ConstantTerm
 Outputs            : *pY, *pU, *pV, *constant
 Returns            : void
 Description        : Converts a floating point function in the form
					  a*yCoeff + b*uCoeff + c * vCoeff + d
					  Into a fixed point function
************************************************************************************/
static IMG_VOID ConvertTAGCoeffs(IMG_DOUBLE Ycoeff, IMG_DOUBLE Ucoeff, IMG_DOUBLE Vcoeff, IMG_DOUBLE ConstantTerm,
						IMG_INT16 *pY, IMG_INT16 *pU, IMG_INT16 *pV, IMG_INT16 *constant)
{
#if defined(EUR_CR_TPU_YUVSCALE)
	/* Coeffs are 1.2.7 signed fixed point numbers. Multiply by 1<<7 */
	Ycoeff *= 128;
	Ucoeff *= 128;
	Vcoeff *= 128;


	*pY = ((IMG_INT16)ROUND(Ycoeff) & 0x3FF);
	*pU = ((IMG_INT16)ROUND(Ucoeff) & 0x3FF);
	*pV = ((IMG_INT16)ROUND(Vcoeff) & 0x3FF);
	
	/* Constant term will be ignored */
	*constant = ((IMG_INT16)ROUND(ConstantTerm) & 0x1FFF);
#else
	/* Coeffs are 1.2.9 signed fixed point numbers. Multiply by 1<<9 */
	Ycoeff *= 512;
	Ucoeff *= 512;
	Vcoeff *= 512;

	/* Constants are 1.11.1 signed fixed point numbers. Multiply by 1<<1 */
	ConstantTerm *= 2;


	*pY = ((IMG_INT16)ROUND(Ycoeff) & 0xFFF);
	*pU = ((IMG_INT16)ROUND(Ucoeff) & 0xFFF);
	*pV = ((IMG_INT16)ROUND(Vcoeff) & 0xFFF);
	*constant = ((IMG_INT16)ROUND(ConstantTerm) & 0x1FFF);
#endif
}
#endif /* defined(SGX_FEATURE_TAG_YUV_TO_RGB) */


#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
#define NUM_FIRH_COEFF_BITS		10
#else
#define NUM_FIRH_COEFF_BITS		8
#endif

#define NUM_FIRH_CONSTANT_BITS	16

/***********************************************************************************
 Function Name      : ConvertFIRHCoeffs
 Inputs             : Ycoeff, Ucoeff, Vcoeff, ConstantTerm
 Outputs            : *pY, *pU, *pV, *constant, *pShift
 Returns            : void
 Description        : Converts a floating point function in the form
					  a*yCoeff + b*uCoeff + c * vCoeff + d
					  Into a fixed point function of the forrm
					  (a*pY + b * pU + c * pV + constant)>>pShift
************************************************************************************/
static IMG_VOID ConvertFIRHCoeffs(IMG_DOUBLE Ycoeff, IMG_DOUBLE Ucoeff, IMG_DOUBLE Vcoeff, IMG_DOUBLE ConstantTerm,
						IMG_INT16 *pY, IMG_INT16 *pU, IMG_INT16 *pV, IMG_INT16 *constant, IMG_UINT8 *pShift, 
						IMG_UINT8 nBitY, IMG_UINT8 nBitU, IMG_UINT8 nBitV)
{
	*pShift = 0;

	Ycoeff *= (1 << NUM_FIRH_COEFF_BITS);
	Ucoeff *= (1 << NUM_FIRH_COEFF_BITS);
	Vcoeff *= (1 << NUM_FIRH_COEFF_BITS);
	ConstantTerm *= (1 << NUM_FIRH_COEFF_BITS);
	*pShift = NUM_FIRH_COEFF_BITS;

	/*
		What we want to do is scale up the coefficients so that they just fit into their
		allowed bits, so we are using signed maths giving us coefficients can be between +-128.
		The constant can be between =- 32767.
		The divide can be between 0 and 256 (on powers of two only).
		A mathematical approach would be nice, but for simplicity do an iterative compare
		and divide. Until something fits.
	*/
	while(CheckCoeffs(Ycoeff, Ucoeff, Vcoeff, ConstantTerm, nBitY, nBitU, nBitV, NUM_FIRH_CONSTANT_BITS))
	{
		Ycoeff /= 2;
		Ucoeff /= 2;
		Vcoeff /= 2;
		ConstantTerm /= 2;
		(*pShift)--;
	}
	*pY = (IMG_INT16)ROUND(Ycoeff);
	*pU = (IMG_INT16)ROUND(Ucoeff);
	*pV = (IMG_INT16)ROUND(Vcoeff);
	*constant = (IMG_INT16)ROUND(ConstantTerm);
}

#endif /* defined(GENERATE_YUV_COEFFICIENTS) */ 

IMG_INTERNAL IMG_VOID InitialiseYUVCoefficients(YUV_REGISTERS *psYUVRegisters, PVRSRV_PIXEL_FORMAT ePixelFormat, IMG_UINT32 ui32Flags)
{
	YUV_CONVERT_COEFFS sYUVCoeffs;
#if defined(GENERATE_YUV_COEFFICIENTS)
	IMG_DOUBLE RGB_scale, Y_scale, Cb_scale, Cr_scale;
	IMG_DOUBLE RGB_offset, Y_offset, CbCr_offset;
	IMG_DOUBLE constant;
	YUV_CSC_COEFF sTransferMatrix;
	YUV_CONVERT_COEFFS *psCoeffs;

	/*
		ITU-R BT.601, BT.709 transfer matrices from DXVA 2.0
		Video Color Field definitions Design Spec(Version 0.03).
		[R', G', B'] values are in the range [0, 1], Y' is in the range [0,1]
		and [Pb, Pr] components are in the range [-0.5, 0.5].
	*/
	static const YUV_CSC_COEFF s601 =
	{
		1,		-0.000001,		1.402,
		1,		-0.344136,		-0.714136,
		1,		1.772,			0	
	};

	static const YUV_CSC_COEFF s709 =
	{
		1,		0,				1.5748,
		1,		-0.187324,		-0.468124,
		1,		1.8556,			0	
	};

	psCoeffs = &sYUVCoeffs;

#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
	sYUVCoeffs.bUseFIRH = IMG_FALSE;

#if defined(FIX_HW_BRN_32951)
	if(ePixelFormat == PVRSRV_PIXEL_FORMAT_NV12 || ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12)
	{
		sYUVCoeffs.bUseFIRH = IMG_TRUE;
	}
#endif

#else
	sYUVCoeffs.bUseFIRH = IMG_TRUE;

#endif

	if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_BT709)
	{
		GLES1MemCopy(&sTransferMatrix, &s709, sizeof(YUV_CSC_COEFF));
	}
	else
	{
		GLES1MemCopy(&sTransferMatrix, &s601, sizeof(YUV_CSC_COEFF));
	}

	if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_FULL_RANGE)
	{
		/* Y has a range of [0, 255] and Cb, Cr have a range of [0, 255] */
		Y_scale = 255;
		Y_offset = 0;
		Cb_scale = Cr_scale = 255;
		CbCr_offset = 128;
	}
	else
	{
		/* Y has a range of [16, 235] and Cb, Cr have a range of [16, 240] */
		Y_scale = 219;
		Y_offset = 16;
		Cb_scale = Cr_scale = 224;
		CbCr_offset = 128;
	}

	RGB_scale = 255;
	RGB_offset = 0; 

	if(Y_scale != 1 || Cb_scale != 1 || Cr_scale != 1)
	{
		/* Each column of the transfer matrix has to be scaled by the excursion of each component */
		ScaleTransferMatrix(&sTransferMatrix, 1/Y_scale, 1/Cb_scale, 1/Cr_scale); 
	}
	if(RGB_scale != 1)
	{
		/* All the values in the transfer matrix have to be multiplied by the excursion of the RGB components */
		ScaleTransferMatrix(&sTransferMatrix, RGB_scale, RGB_scale, RGB_scale);
	}

	constant = ((-1 * Y_offset)* sTransferMatrix.rY) + (-1 * CbCr_offset * sTransferMatrix.rCb) + (-1 * CbCr_offset * sTransferMatrix.rCr);

	if(sYUVCoeffs.bUseFIRH)
	{
		/* Convert transform operation from floating point to fixed point */
		/* Y coefficient uses 2 taps, so add 1 bit. Result will later be divided by 2 and split between the taps */
		ConvertFIRHCoeffs(sTransferMatrix.rY, sTransferMatrix.rCb, sTransferMatrix.rCr, constant,	// input coefficients
					&psCoeffs->u.sFIRH.i16rY, &psCoeffs->u.sFIRH.i16rU, &psCoeffs->u.sFIRH.i16rV, // tranfer matrix coefficients for R
					&psCoeffs->u.sFIRH.i16rConst,&psCoeffs->u.sFIRH.ui8rShift, NUM_FIRH_COEFF_BITS + 1, NUM_FIRH_COEFF_BITS, NUM_FIRH_COEFF_BITS);

		constant = ((-1 * Y_offset)* sTransferMatrix.gY) + (-1 * CbCr_offset * sTransferMatrix.gCb) + (-1 * CbCr_offset * sTransferMatrix.gCr);

		/* Convert transform operation from floating point to fixed point */
		/* Y coefficient uses 2 taps, so add 1 bit. Result will later be divided by 2 and split between the taps */
		ConvertFIRHCoeffs(sTransferMatrix.gY, sTransferMatrix.gCb, sTransferMatrix.gCr, constant,	// input coefficients
					&psCoeffs->u.sFIRH.i16gY, &psCoeffs->u.sFIRH.i16gU, &psCoeffs->u.sFIRH.i16gV, // tranfer matrix coefficients for G
					&psCoeffs->u.sFIRH.i16gConst,&psCoeffs->u.sFIRH.ui8gShift, NUM_FIRH_COEFF_BITS + 1, NUM_FIRH_COEFF_BITS, NUM_FIRH_COEFF_BITS);

		constant = ((-1 * Y_offset)* sTransferMatrix.bY) + (-1 * CbCr_offset * sTransferMatrix.bCb) + (-1 * CbCr_offset * sTransferMatrix.bCr);

		/* Convert transform operation from floating point to fixed point */
		/* Y coefficient only has 1 tap*/
		ConvertFIRHCoeffs(sTransferMatrix.bY, sTransferMatrix.bCb, sTransferMatrix.bCr, constant,	// input coefficients
					&psCoeffs->u.sFIRH.i16bY, &psCoeffs->u.sFIRH.i16bU, &psCoeffs->u.sFIRH.i16bV, // tranfer matrix coefficients for B
					&psCoeffs->u.sFIRH.i16bConst,&psCoeffs->u.sFIRH.ui8bShift, NUM_FIRH_COEFF_BITS, NUM_FIRH_COEFF_BITS, NUM_FIRH_COEFF_BITS);

	}
#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
	else
	{
		/* Convert transform operation from floating point to fixed point */
		ConvertTAGCoeffs(sTransferMatrix.rY, sTransferMatrix.rCb, sTransferMatrix.rCr, constant,	// input coefficients
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[0],
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[3],
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[6], // tranfer matrix coefficients for R
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[9]);

		constant = ((-1 * Y_offset)* sTransferMatrix.gY) + (-1 * CbCr_offset * sTransferMatrix.gCb) + (-1 * CbCr_offset * sTransferMatrix.gCr);

		/* Convert transform operation from floating point to fixed point */
		ConvertTAGCoeffs(sTransferMatrix.gY, sTransferMatrix.gCb, sTransferMatrix.gCr, constant,	// input coefficients
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[1],
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[4],
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[7], // tranfer matrix coefficients for G
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[10]);

		constant = ((-1 * Y_offset)* sTransferMatrix.bY) + (-1 * CbCr_offset * sTransferMatrix.bCb) + (-1 * CbCr_offset * sTransferMatrix.bCr);

		/* Convert transform operation from floating point to fixed point */
		ConvertTAGCoeffs(sTransferMatrix.bY, sTransferMatrix.bCb, sTransferMatrix.bCr, constant,	// input coefficients
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[2],
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[5],
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[8], // tranfer matrix coefficients for B
					(IMG_INT16 *)&psCoeffs->u.sTAG.aui16Coeff[11]);


	}
	
#endif /* SGX_FEATURE_TAG_YUV_TO_RGB */

#else /* GENERATE_YUV_COEFFICIENTS */

#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
	sYUVCoeffs.bUseFIRH = IMG_FALSE;

#if defined(FIX_HW_BRN_32951)
	if(ePixelFormat == PVRSRV_PIXEL_FORMAT_NV12 || ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12)
	{
		sYUVCoeffs.bUseFIRH = IMG_TRUE;
	}
#endif

#else
	sYUVCoeffs.bUseFIRH = IMG_TRUE;

#endif


	if(sYUVCoeffs.bUseFIRH)
	{
#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
		if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_FULL_RANGE)
		{
			if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_BT709)
			{
				/* Full range BT709 */
				sYUVCoeffs.u.sFIRH.i16rY = 128;
				sYUVCoeffs.u.sFIRH.i16rU = 0;
				sYUVCoeffs.u.sFIRH.i16rV = 202;
				sYUVCoeffs.u.sFIRH.i16gY = 256;
				sYUVCoeffs.u.sFIRH.i16gU = -48;
				sYUVCoeffs.u.sFIRH.i16gV = -120;
				sYUVCoeffs.u.sFIRH.i16bY = 128;
				sYUVCoeffs.u.sFIRH.i16bU = 239;
				sYUVCoeffs.u.sFIRH.i16bV = 0;
				sYUVCoeffs.u.sFIRH.ui8rShift = 7;
				sYUVCoeffs.u.sFIRH.ui8gShift = 8;
				sYUVCoeffs.u.sFIRH.ui8bShift = 7;
				sYUVCoeffs.u.sFIRH.i16rConst = -25802;
				sYUVCoeffs.u.sFIRH.i16gConst = 21478;
				sYUVCoeffs.u.sFIRH.i16bConst = -30402;
			}
			else
			{
				/* Full range BT601 */
				sYUVCoeffs.u.sFIRH.i16rY = 128;
				sYUVCoeffs.u.sFIRH.i16rU = 0;
				sYUVCoeffs.u.sFIRH.i16rV = 179;
				sYUVCoeffs.u.sFIRH.i16gY = 128;
				sYUVCoeffs.u.sFIRH.i16gU = -44;
				sYUVCoeffs.u.sFIRH.i16gV = -91;
				sYUVCoeffs.u.sFIRH.i16bY = 128;
				sYUVCoeffs.u.sFIRH.i16bU = 227;
				sYUVCoeffs.u.sFIRH.i16bV = 0;
				sYUVCoeffs.u.sFIRH.ui8rShift = 7;
				sYUVCoeffs.u.sFIRH.ui8gShift = 7;
				sYUVCoeffs.u.sFIRH.ui8bShift = 7;
				sYUVCoeffs.u.sFIRH.i16rConst = -22970;
				sYUVCoeffs.u.sFIRH.i16gConst = 17339;
				sYUVCoeffs.u.sFIRH.i16bConst = -29032;
			}
		}
		else
		{
			if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_BT709)
			{
				/* Conformant range BT709 */
				sYUVCoeffs.u.sFIRH.i16rY = 149;
				sYUVCoeffs.u.sFIRH.i16rU = 0;
				sYUVCoeffs.u.sFIRH.i16rV = 229;
				sYUVCoeffs.u.sFIRH.i16gY = 298;
				sYUVCoeffs.u.sFIRH.i16gU = -55;
				sYUVCoeffs.u.sFIRH.i16gV = -136;
				sYUVCoeffs.u.sFIRH.i16bY = 75;
				sYUVCoeffs.u.sFIRH.i16bU = 135;
				sYUVCoeffs.u.sFIRH.i16bV = 0;
				sYUVCoeffs.u.sFIRH.ui8rShift = 7;
				sYUVCoeffs.u.sFIRH.ui8gShift = 8;
				sYUVCoeffs.u.sFIRH.ui8bShift = 6;
				sYUVCoeffs.u.sFIRH.i16rConst = -31757;
				sYUVCoeffs.u.sFIRH.i16gConst = 19681;
				sYUVCoeffs.u.sFIRH.i16bConst = -18497;
			}
			else
			{
				/* Default: Conformant range BT601 */
				sYUVCoeffs.u.sFIRH.i16rY = 149;
				sYUVCoeffs.u.sFIRH.i16rU = 0;
				sYUVCoeffs.u.sFIRH.i16rV = 204;
				sYUVCoeffs.u.sFIRH.i16gY = 149;
				sYUVCoeffs.u.sFIRH.i16gU = -50;
				sYUVCoeffs.u.sFIRH.i16gV = -104;
				sYUVCoeffs.u.sFIRH.i16bY = 75;
				sYUVCoeffs.u.sFIRH.i16bU = 129;
				sYUVCoeffs.u.sFIRH.i16bV = 0;
				sYUVCoeffs.u.sFIRH.ui8rShift = 7;
				sYUVCoeffs.u.sFIRH.ui8gShift = 7;
				sYUVCoeffs.u.sFIRH.ui8bShift = 6;
				sYUVCoeffs.u.sFIRH.i16rConst = -28534;
				sYUVCoeffs.u.sFIRH.i16gConst = 17354;
				sYUVCoeffs.u.sFIRH.i16bConst = -17717;
			}
		}
#else
		if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_FULL_RANGE)
		{
			if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_BT709)
			{
				/* Full range BT709 */
				sYUVCoeffs.u.sFIRH.i16rY = 64;
				sYUVCoeffs.u.sFIRH.i16rU = 0;
				sYUVCoeffs.u.sFIRH.i16rV = 101;
				sYUVCoeffs.u.sFIRH.i16gY = 128;
				sYUVCoeffs.u.sFIRH.i16gU = -24;
				sYUVCoeffs.u.sFIRH.i16gV = -60;
				sYUVCoeffs.u.sFIRH.i16bY = 64;
				sYUVCoeffs.u.sFIRH.i16bU = 119;
				sYUVCoeffs.u.sFIRH.i16bV = 0;
				sYUVCoeffs.u.sFIRH.ui8rShift = 6;
				sYUVCoeffs.u.sFIRH.ui8gShift = 7;
				sYUVCoeffs.u.sFIRH.ui8bShift = 6;
				sYUVCoeffs.u.sFIRH.i16rConst = -12901;
				sYUVCoeffs.u.sFIRH.i16gConst = 10739;
				sYUVCoeffs.u.sFIRH.i16bConst = -15201;
			}
			else
			{
				/* Full range BT601 */
				sYUVCoeffs.u.sFIRH.i16rY = 64;
				sYUVCoeffs.u.sFIRH.i16rU = 0;
				sYUVCoeffs.u.sFIRH.i16rV = 90;
				sYUVCoeffs.u.sFIRH.i16gY = 128;
				sYUVCoeffs.u.sFIRH.i16gU = -44;
				sYUVCoeffs.u.sFIRH.i16gV = -91;
				sYUVCoeffs.u.sFIRH.i16bY = 64;
				sYUVCoeffs.u.sFIRH.i16bU = 113;
				sYUVCoeffs.u.sFIRH.i16bV = 0;
				sYUVCoeffs.u.sFIRH.ui8rShift = 6;
				sYUVCoeffs.u.sFIRH.ui8gShift = 7;
				sYUVCoeffs.u.sFIRH.ui8bShift = 6;
				sYUVCoeffs.u.sFIRH.i16rConst = -11485;
				sYUVCoeffs.u.sFIRH.i16gConst = 17339;
				sYUVCoeffs.u.sFIRH.i16bConst = -14516;
			}
		}
		else
		{
			if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_BT709)
			{
				/* Conformant range BT709 */
				sYUVCoeffs.u.sFIRH.i16rY = 75;
				sYUVCoeffs.u.sFIRH.i16rU = 0;
				sYUVCoeffs.u.sFIRH.i16rV = 115;
				sYUVCoeffs.u.sFIRH.i16gY = 149;
				sYUVCoeffs.u.sFIRH.i16gU = -27;
				sYUVCoeffs.u.sFIRH.i16gV = -68;
				sYUVCoeffs.u.sFIRH.i16bY = 37;
				sYUVCoeffs.u.sFIRH.i16bU = 68;
				sYUVCoeffs.u.sFIRH.i16bV = 0;
				sYUVCoeffs.u.sFIRH.ui8rShift = 6;
				sYUVCoeffs.u.sFIRH.ui8gShift = 7;
				sYUVCoeffs.u.sFIRH.ui8bShift = 5;
				sYUVCoeffs.u.sFIRH.i16rConst = -15878;
				sYUVCoeffs.u.sFIRH.i16gConst = 9840;
				sYUVCoeffs.u.sFIRH.i16bConst = -9249;
			}
			else
			{
				/* Default: Conformant range BT601 */
				sYUVCoeffs.u.sFIRH.i16rY = 75;
				sYUVCoeffs.u.sFIRH.i16rU = 0;
				sYUVCoeffs.u.sFIRH.i16rV = 102;
				sYUVCoeffs.u.sFIRH.i16gY = 149;
				sYUVCoeffs.u.sFIRH.i16gU = -50;
				sYUVCoeffs.u.sFIRH.i16gV = -104;
				sYUVCoeffs.u.sFIRH.i16bY = 37;
				sYUVCoeffs.u.sFIRH.i16bU = 65;
				sYUVCoeffs.u.sFIRH.i16bV = 0;
				sYUVCoeffs.u.sFIRH.ui8rShift = 6;
				sYUVCoeffs.u.sFIRH.ui8gShift = 7;
				sYUVCoeffs.u.sFIRH.ui8bShift = 5;
				sYUVCoeffs.u.sFIRH.i16rConst = -14267;
				sYUVCoeffs.u.sFIRH.i16gConst = 17354;
				sYUVCoeffs.u.sFIRH.i16bConst = -8859;
			}
		}
#endif
	}
#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
	else
	{
#if defined(EUR_CR_TPU_YUVSCALE)
		if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_FULL_RANGE)
		{
			if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_BT709)
			{
				sYUVCoeffs.u.sTAG.aui16Coeff[0] = 0x080;
				sYUVCoeffs.u.sTAG.aui16Coeff[1] = 0x080;
				sYUVCoeffs.u.sTAG.aui16Coeff[2] = 0x080;
				sYUVCoeffs.u.sTAG.aui16Coeff[3] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[4] = 0x3E8;
				sYUVCoeffs.u.sTAG.aui16Coeff[5] = 0xEE;
				sYUVCoeffs.u.sTAG.aui16Coeff[6] = 0xCA;
				sYUVCoeffs.u.sTAG.aui16Coeff[7] = 0x3C4;
				sYUVCoeffs.u.sTAG.aui16Coeff[8] = 0;
			}
			else
			{
				/* BT601 */
				sYUVCoeffs.u.sTAG.aui16Coeff[0] = 0x080;
				sYUVCoeffs.u.sTAG.aui16Coeff[1] = 0x080;
				sYUVCoeffs.u.sTAG.aui16Coeff[2] = 0x080;
				sYUVCoeffs.u.sTAG.aui16Coeff[3] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[4] = 0x3D4;
				sYUVCoeffs.u.sTAG.aui16Coeff[5] = 0xE3;
				sYUVCoeffs.u.sTAG.aui16Coeff[6] = 0xB3;
				sYUVCoeffs.u.sTAG.aui16Coeff[7] = 0x3A5;
				sYUVCoeffs.u.sTAG.aui16Coeff[8] = 0;
			}

			sYUVCoeffs.u.sTAG.bScale = IMG_TRUE;
		}
		else
		{
			if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_BT709)
			{
				sYUVCoeffs.u.sTAG.aui16Coeff[0] = 0x095;
				sYUVCoeffs.u.sTAG.aui16Coeff[1] = 0x095;
				sYUVCoeffs.u.sTAG.aui16Coeff[2] = 0x095;
				sYUVCoeffs.u.sTAG.aui16Coeff[3] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[4] = 0x3E5;
				sYUVCoeffs.u.sTAG.aui16Coeff[5] = 0x10E;
				sYUVCoeffs.u.sTAG.aui16Coeff[6] = 0xE5;
				sYUVCoeffs.u.sTAG.aui16Coeff[7] = 0x3BC;
				sYUVCoeffs.u.sTAG.aui16Coeff[8] = 0;
			}
			else
			{
				/* BT601 */
				sYUVCoeffs.u.sTAG.aui16Coeff[0] = 0x095;
				sYUVCoeffs.u.sTAG.aui16Coeff[1] = 0x095;
				sYUVCoeffs.u.sTAG.aui16Coeff[2] = 0x095;
				sYUVCoeffs.u.sTAG.aui16Coeff[3] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[4] = 0x3CE;
				sYUVCoeffs.u.sTAG.aui16Coeff[5] = 0x102;
				sYUVCoeffs.u.sTAG.aui16Coeff[6] = 0xCC;
				sYUVCoeffs.u.sTAG.aui16Coeff[7] = 0x398;
				sYUVCoeffs.u.sTAG.aui16Coeff[8] = 0;
			}

			sYUVCoeffs.u.sTAG.bScale = IMG_FALSE;
		}
#else
		if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_FULL_RANGE)
		{
			if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_BT709)
			{
				sYUVCoeffs.u.sTAG.aui16Coeff[0] = 0x200;
				sYUVCoeffs.u.sTAG.aui16Coeff[1] = 0x200;
				sYUVCoeffs.u.sTAG.aui16Coeff[2] = 0x200;
				sYUVCoeffs.u.sTAG.aui16Coeff[3] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[4] = 0xFA0;
				sYUVCoeffs.u.sTAG.aui16Coeff[5] = 0x3B6;
				sYUVCoeffs.u.sTAG.aui16Coeff[6] = 0x326;
				sYUVCoeffs.u.sTAG.aui16Coeff[7] = 0xF10;
				sYUVCoeffs.u.sTAG.aui16Coeff[8] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[9] = 0x1E6D;
				sYUVCoeffs.u.sTAG.aui16Coeff[10] = 0xA8;
				sYUVCoeffs.u.sTAG.aui16Coeff[11] = 0x1E25;
			}
			else
			{
				/* BT601 */
				sYUVCoeffs.u.sTAG.aui16Coeff[0] = 0x200;
				sYUVCoeffs.u.sTAG.aui16Coeff[1] = 0x200;
				sYUVCoeffs.u.sTAG.aui16Coeff[2] = 0x200;
				sYUVCoeffs.u.sTAG.aui16Coeff[3] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[4] = 0xF50;
				sYUVCoeffs.u.sTAG.aui16Coeff[5] = 0x38B;
				sYUVCoeffs.u.sTAG.aui16Coeff[6] = 0x2CE;
				sYUVCoeffs.u.sTAG.aui16Coeff[7] = 0xE92;
				sYUVCoeffs.u.sTAG.aui16Coeff[8] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[9] = 0x1E99;
				sYUVCoeffs.u.sTAG.aui16Coeff[10] = 0x10F;
				sYUVCoeffs.u.sTAG.aui16Coeff[11] = 0x1E3A;
			}
		}
		else
		{
			if(ui32Flags & PVRSRV_BC_FLAGS_YUVCSC_BT709)
			{
				sYUVCoeffs.u.sTAG.aui16Coeff[0] = 0x254;
				sYUVCoeffs.u.sTAG.aui16Coeff[1] = 0x254;
				sYUVCoeffs.u.sTAG.aui16Coeff[2] = 0x254;
				sYUVCoeffs.u.sTAG.aui16Coeff[3] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[4] = 0xF93;
				sYUVCoeffs.u.sTAG.aui16Coeff[5] = 0x43A;
				sYUVCoeffs.u.sTAG.aui16Coeff[6] = 0x396;
				sYUVCoeffs.u.sTAG.aui16Coeff[7] = 0xEEF;
				sYUVCoeffs.u.sTAG.aui16Coeff[8] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[9] = 0x1E10;
				sYUVCoeffs.u.sTAG.aui16Coeff[10] = 0x9A;
				sYUVCoeffs.u.sTAG.aui16Coeff[11] = 0x1DBE;
			}
			else
			{
				/* BT601 */
				sYUVCoeffs.u.sTAG.aui16Coeff[0] = 0x254;
				sYUVCoeffs.u.sTAG.aui16Coeff[1] = 0x254;
				sYUVCoeffs.u.sTAG.aui16Coeff[2] = 0x254;
				sYUVCoeffs.u.sTAG.aui16Coeff[3] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[4] = 0xF37;
				sYUVCoeffs.u.sTAG.aui16Coeff[5] = 0x409;
				sYUVCoeffs.u.sTAG.aui16Coeff[6] = 0x331;
				sYUVCoeffs.u.sTAG.aui16Coeff[7] = 0xE60;
				sYUVCoeffs.u.sTAG.aui16Coeff[8] = 0;
				sYUVCoeffs.u.sTAG.aui16Coeff[9] = 0x1E42;
				sYUVCoeffs.u.sTAG.aui16Coeff[10] = 0x10F;
				sYUVCoeffs.u.sTAG.aui16Coeff[11] = 0x1DD6;
			}
		}
#endif /* EUR_CR_TPU_YUVSCALE */
	}
#endif /* SGX_FEATURE_TAG_YUV_TO_RGB */

#endif /* defined(GENERATE_YUV_COEFFICIENTS) */

	SetupYUVCoefficients(psYUVRegisters, ePixelFormat, &sYUVCoeffs);
}

static IMG_VOID SetupYUVCoefficients(YUV_REGISTERS *psYUVRegisters, PVRSRV_PIXEL_FORMAT ePixelFormat, YUV_CONVERT_COEFFS *psCoeffs)
{
	psYUVRegisters->bUseFIRH = psCoeffs->bUseFIRH;

	if(psYUVRegisters->bUseFIRH)
	{
		switch(ePixelFormat)
		{
			case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY:
			case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV:
			{
				/* We use taps 0,1,2,6 and 0, 3 and 6 which means a filter offset of either 3 or 5
					XyuvXyuvXyuv	XyuvXyuvXyuv	XyuvXyuvXyuv
						0D000CBA		0D000CBA	  0C00B00A
				*/
				psYUVRegisters->u.sFIRH.ui32Filter0Left =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16rV << EUR_CR_USE_FILTER0_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP0_MASK) | 
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16rU << EUR_CR_USE_FILTER0_LEFT_TAP1_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP1_MASK) | 
					((((IMG_UINT32)((psCoeffs->u.sFIRH.i16rY + 1) / 2)) << EUR_CR_USE_FILTER0_LEFT_TAP2_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP2_MASK);
				
				psYUVRegisters->u.sFIRH.ui32Filter0Right =
					((((IMG_UINT32)(psCoeffs->u.sFIRH.i16rY / 2)) << EUR_CR_USE_FILTER0_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER0_RIGHT_TAP6_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter0Extra =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16rConst << EUR_CR_USE_FILTER0_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER0_EXTRA_CONSTANT_MASK) | 
					((psCoeffs->u.sFIRH.ui8rShift << EUR_CR_USE_FILTER0_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER0_EXTRA_SHR_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter1Left =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16gV << EUR_CR_USE_FILTER1_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP0_MASK) | 
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16gU << EUR_CR_USE_FILTER1_LEFT_TAP1_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP1_MASK) | 
					((((IMG_UINT32)((psCoeffs->u.sFIRH.i16gY + 1) / 2)) << EUR_CR_USE_FILTER1_LEFT_TAP2_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP2_MASK);
				
				psYUVRegisters->u.sFIRH.ui32Filter1Right =
					((((IMG_UINT32)(psCoeffs->u.sFIRH.i16gY / 2)) << EUR_CR_USE_FILTER1_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER1_RIGHT_TAP6_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter1Extra =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16gConst << EUR_CR_USE_FILTER1_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER1_EXTRA_CONSTANT_MASK) | 
					((psCoeffs->u.sFIRH.ui8gShift << EUR_CR_USE_FILTER1_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER1_EXTRA_SHR_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter2Left =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16bY << EUR_CR_USE_FILTER2_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP0_MASK) | 
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16bU << EUR_CR_USE_FILTER2_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP3_MASK);
				
				psYUVRegisters->u.sFIRH.ui32Filter2Right =
					((IMG_UINT32)psCoeffs->u.sFIRH.i16bV << EUR_CR_USE_FILTER2_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER2_RIGHT_TAP6_MASK;
				
				psYUVRegisters->u.sFIRH.ui32Filter2Extra =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16bConst << EUR_CR_USE_FILTER2_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_CONSTANT_MASK) | 
					((psCoeffs->u.sFIRH.ui8bShift << EUR_CR_USE_FILTER2_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_SHR_MASK);

#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
				psYUVRegisters->u.sFIRH.ui32Filter0Centre = 0;
				psYUVRegisters->u.sFIRH.ui32Filter1Centre = 0;
#endif
				
				psYUVRegisters->u.sFIRH.ui32FilterTable = 3;
				break;
			}
			case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY:
			case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU:
			{
				/* We use taps 0,1,2,6 and 0, 3 and 6 which means a filter offset of either 3 or 5
					XyvuXyvuXyvu	XyvuXyvuXyvu	XyvuXyvuXyvu
						0D000CBA		0D000CBA	  0C00B00A
				*/
				psYUVRegisters->u.sFIRH.ui32Filter0Left =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16rU << EUR_CR_USE_FILTER0_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP0_MASK) | 
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16rV << EUR_CR_USE_FILTER0_LEFT_TAP1_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP1_MASK) | 
					((((IMG_UINT32)((psCoeffs->u.sFIRH.i16rY + 1) / 2)) << EUR_CR_USE_FILTER0_LEFT_TAP2_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP2_MASK);
				
				psYUVRegisters->u.sFIRH.ui32Filter0Right =
					((((IMG_UINT32)(psCoeffs->u.sFIRH.i16rY / 2)) << EUR_CR_USE_FILTER0_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER0_RIGHT_TAP6_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter0Extra =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16rConst << EUR_CR_USE_FILTER0_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER0_EXTRA_CONSTANT_MASK) | 
					((psCoeffs->u.sFIRH.ui8rShift << EUR_CR_USE_FILTER0_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER0_EXTRA_SHR_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter1Left =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16gU << EUR_CR_USE_FILTER1_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP0_MASK) | 
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16gV << EUR_CR_USE_FILTER1_LEFT_TAP1_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP1_MASK) | 
					((((IMG_UINT32)((psCoeffs->u.sFIRH.i16gY + 1) / 2)) << EUR_CR_USE_FILTER1_LEFT_TAP2_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP2_MASK);
				
				psYUVRegisters->u.sFIRH.ui32Filter1Right =
					((((IMG_UINT32)(psCoeffs->u.sFIRH.i16gY / 2)) << EUR_CR_USE_FILTER1_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER1_RIGHT_TAP6_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter1Extra =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16gConst << EUR_CR_USE_FILTER1_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER1_EXTRA_CONSTANT_MASK) | 
					((psCoeffs->u.sFIRH.ui8gShift << EUR_CR_USE_FILTER1_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER1_EXTRA_SHR_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter2Left =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16bY << EUR_CR_USE_FILTER2_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP0_MASK) | 
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16bV << EUR_CR_USE_FILTER2_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP3_MASK);
				
				psYUVRegisters->u.sFIRH.ui32Filter2Right =
					((IMG_UINT32)psCoeffs->u.sFIRH.i16bU << EUR_CR_USE_FILTER2_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER2_RIGHT_TAP6_MASK;
				
				psYUVRegisters->u.sFIRH.ui32Filter2Extra =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16bConst << EUR_CR_USE_FILTER2_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_CONSTANT_MASK) | 
					((psCoeffs->u.sFIRH.ui8bShift << EUR_CR_USE_FILTER2_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_SHR_MASK);

#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
				psYUVRegisters->u.sFIRH.ui32Filter0Centre = 0;
				psYUVRegisters->u.sFIRH.ui32Filter1Centre = 0;
#endif

				psYUVRegisters->u.sFIRH.ui32FilterTable = 3;

				break;
			}
			case PVRSRV_PIXEL_FORMAT_NV12:
			case PVRSRV_PIXEL_FORMAT_YV12:
			{
#if defined(FIX_HW_BRN_32951)

				/* NV12 */
				/* We use taps 0,3,5,6 and 0, 3 and 6 which means a filter offset of 7
					1yyyvuuuvuuu	1yyyvuuuvuuu	1yyyvuuuvuuu
					0DC0B00A		0DC0B00A	    0C00B00A
				*/

				/* YV12 */
				/* We use taps 0,3,5,6 and 0, 3 and 6 which means a filter offset of 6
					1yyy1vvvu000	1yyy1vvvu000	1yyy1vvvu000
					 0DC0B00A		 0DC0B00A	     0C00B00A
				*/

#else
				/* NV12 */
				/* We use taps 0,3,5,6 and 0, 3 and 6 which means a filter offset of 7
					yyyyvuvuvuvu	yyyyvuvuvuvu	yyyyvuvuvuvu
					0DC0B00A		0DC0B00A	    0C00B00A
				*/

				/* YV12 */
				/* We use taps 0,3,5,6 and 0, 3 and 6 which means a filter offset of 6
					yyyyvvvvuuuu	yyyyvvvvuuuu	yyyyvvvvuuuu
					 0DC0B00A		 0DC0B00A	     0C00B00A
				*/
#endif

				psYUVRegisters->u.sFIRH.ui32Filter0Left =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16rU << EUR_CR_USE_FILTER0_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP0_MASK);

#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
				psYUVRegisters->u.sFIRH.ui32Filter0Centre =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16rV << EUR_CR_USE_FILTER0_CENTRE_TAP3_SHIFT) & EUR_CR_USE_FILTER0_CENTRE_TAP3_MASK);
#else
				psYUVRegisters->u.sFIRH.ui32Filter0Left |=
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16rV << EUR_CR_USE_FILTER0_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER0_LEFT_TAP3_MASK);
#endif
			
				psYUVRegisters->u.sFIRH.ui32Filter0Right = 		
					((((IMG_UINT32)((psCoeffs->u.sFIRH.i16rY + 1) / 2)) << EUR_CR_USE_FILTER0_RIGHT_TAP5_SHIFT) & EUR_CR_USE_FILTER0_RIGHT_TAP5_MASK) |
					((((IMG_UINT32)(psCoeffs->u.sFIRH.i16rY / 2)) << EUR_CR_USE_FILTER0_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER0_RIGHT_TAP6_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter0Extra = 
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16rConst << EUR_CR_USE_FILTER0_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER0_EXTRA_CONSTANT_MASK) | 
					((psCoeffs->u.sFIRH.ui8rShift << EUR_CR_USE_FILTER0_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER0_EXTRA_SHR_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter1Left =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16gU << EUR_CR_USE_FILTER1_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP0_MASK);
				
#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
				psYUVRegisters->u.sFIRH.ui32Filter1Centre = 		
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16gV << EUR_CR_USE_FILTER1_CENTRE_TAP3_SHIFT) & EUR_CR_USE_FILTER1_CENTRE_TAP3_MASK);
#else
				psYUVRegisters->u.sFIRH.ui32Filter1Left |=
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16gV << EUR_CR_USE_FILTER1_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER1_LEFT_TAP3_MASK);

#endif
				psYUVRegisters->u.sFIRH.ui32Filter1Right = 		
					((((IMG_UINT32)((psCoeffs->u.sFIRH.i16gY + 1) / 2)) << EUR_CR_USE_FILTER1_RIGHT_TAP5_SHIFT) & EUR_CR_USE_FILTER1_RIGHT_TAP5_MASK) |
					((((IMG_UINT32)(psCoeffs->u.sFIRH.i16gY / 2)) << EUR_CR_USE_FILTER1_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER1_RIGHT_TAP6_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter1Extra = 
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16gConst << EUR_CR_USE_FILTER1_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER1_EXTRA_CONSTANT_MASK) | 
					((psCoeffs->u.sFIRH.ui8gShift << EUR_CR_USE_FILTER1_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER1_EXTRA_SHR_MASK);

				psYUVRegisters->u.sFIRH.ui32Filter2Left =
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16bU << EUR_CR_USE_FILTER2_LEFT_TAP0_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP0_MASK) | 
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16bV << EUR_CR_USE_FILTER2_LEFT_TAP3_SHIFT) & EUR_CR_USE_FILTER2_LEFT_TAP3_MASK);
				
				psYUVRegisters->u.sFIRH.ui32Filter2Right = 
					((IMG_UINT32)psCoeffs->u.sFIRH.i16bY << EUR_CR_USE_FILTER2_RIGHT_TAP6_SHIFT) & EUR_CR_USE_FILTER2_RIGHT_TAP6_MASK;
				
				psYUVRegisters->u.sFIRH.ui32Filter2Extra = 
					(((IMG_UINT32)psCoeffs->u.sFIRH.i16bConst << EUR_CR_USE_FILTER2_EXTRA_CONSTANT_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_CONSTANT_MASK) | 
					((psCoeffs->u.sFIRH.ui8bShift << EUR_CR_USE_FILTER2_EXTRA_SHR_SHIFT) & EUR_CR_USE_FILTER2_EXTRA_SHR_MASK);

				psYUVRegisters->u.sFIRH.ui32FilterTable = 3;
				break;
			}
			default:
				break;
		}
	}
#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
	else
	{
		switch(ePixelFormat)
		{
			case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY:
			case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV:
			case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY:
			case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU:
			case PVRSRV_PIXEL_FORMAT_NV12:
			case PVRSRV_PIXEL_FORMAT_YV12:
			{
				psYUVRegisters->u.sTAG.ui32CSC0Coeff01 = (psCoeffs->u.sTAG.aui16Coeff[1] << EUR_CR_TPU_YUV00_C01_SHIFT) | psCoeffs->u.sTAG.aui16Coeff[0];
				psYUVRegisters->u.sTAG.ui32CSC0Coeff23 = (psCoeffs->u.sTAG.aui16Coeff[3] << EUR_CR_TPU_YUV01_C10_SHIFT) | psCoeffs->u.sTAG.aui16Coeff[2];
				psYUVRegisters->u.sTAG.ui32CSC0Coeff45 = (psCoeffs->u.sTAG.aui16Coeff[5] << EUR_CR_TPU_YUV02_C12_SHIFT) | psCoeffs->u.sTAG.aui16Coeff[4];
				psYUVRegisters->u.sTAG.ui32CSC0Coeff67 = (psCoeffs->u.sTAG.aui16Coeff[7] << EUR_CR_TPU_YUV03_C21_SHIFT) | psCoeffs->u.sTAG.aui16Coeff[6];
#if defined(EUR_CR_TPU_YUVSCALE)
				psYUVRegisters->u.sTAG.ui32CSC0Coeff8 = psCoeffs->u.sTAG.aui16Coeff[8];
#else
				psYUVRegisters->u.sTAG.ui32CSC0Coeff89 = (psCoeffs->u.sTAG.aui16Coeff[9] << EUR_CR_TPU_YUV04_D00_SHIFT) | psCoeffs->u.sTAG.aui16Coeff[8];
				psYUVRegisters->u.sTAG.ui32CSC0Coeff1011 = (psCoeffs->u.sTAG.aui16Coeff[11] << EUR_CR_TPU_YUV05_D20_SHIFT) | psCoeffs->u.sTAG.aui16Coeff[10];
#endif

#if defined(EUR_CR_TPU_YUVSCALE)
#if defined(EUR_CR_TPU_YUVSCALE_DISABLE_MASK)
				psYUVRegisters->u.sTAG.ui32CSCScale = (psCoeffs->u.sTAG.bScale ? 0 : EUR_CR_TPU_YUVSCALE_DISABLE_MASK);
#else
				psYUVRegisters->u.sTAG.ui32CSCScale = (psCoeffs->u.sTAG.bScale ? 0 : EUR_CR_TPU_YUVSCALE_DISABLE_Y0_MASK);
#endif
#endif
				break;
			}
			default:
				break;
		}
	}

#endif /* SGX_FEATURE_TAG_YUV_TO_RGB */

}


IMG_INTERNAL IMG_VOID CopyYUVRegisters(GLES1Context *gc, YUV_REGISTERS *psYUVRegisters)
{
	if(psYUVRegisters->bUseFIRH)
	{
		gc->sKickTA.sKickTACommon.ui32Filter0Left = psYUVRegisters->u.sFIRH.ui32Filter0Left;
		gc->sKickTA.sKickTACommon.ui32Filter0Right = psYUVRegisters->u.sFIRH.ui32Filter0Right;
		gc->sKickTA.sKickTACommon.ui32Filter0Extra = psYUVRegisters->u.sFIRH.ui32Filter0Extra;
		gc->sKickTA.sKickTACommon.ui32Filter1Left = psYUVRegisters->u.sFIRH.ui32Filter1Left;
		gc->sKickTA.sKickTACommon.ui32Filter1Right = psYUVRegisters->u.sFIRH.ui32Filter1Right;
		gc->sKickTA.sKickTACommon.ui32Filter1Extra = psYUVRegisters->u.sFIRH.ui32Filter1Extra;
		gc->sKickTA.sKickTACommon.ui32Filter2Left = psYUVRegisters->u.sFIRH.ui32Filter2Left;
		gc->sKickTA.sKickTACommon.ui32Filter2Right = psYUVRegisters->u.sFIRH.ui32Filter2Right;
		gc->sKickTA.sKickTACommon.ui32Filter2Extra = psYUVRegisters->u.sFIRH.ui32Filter2Extra;
		gc->sKickTA.sKickTACommon.ui32FilterTable = psYUVRegisters->u.sFIRH.ui32FilterTable;

#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
		gc->sKickTA.sKickTACommon.ui32Filter0Centre = psYUVRegisters->u.sFIRH.ui32Filter0Centre;
		gc->sKickTA.sKickTACommon.ui32Filter1Centre = psYUVRegisters->u.sFIRH.ui32Filter1Centre;
#endif
	}
#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
	else
	{
		gc->sKickTA.sKickTACommon.ui32CSC0Coeff01 = psYUVRegisters->u.sTAG.ui32CSC0Coeff01;
		gc->sKickTA.sKickTACommon.ui32CSC0Coeff23 = psYUVRegisters->u.sTAG.ui32CSC0Coeff23;
		gc->sKickTA.sKickTACommon.ui32CSC0Coeff45 = psYUVRegisters->u.sTAG.ui32CSC0Coeff45;
		gc->sKickTA.sKickTACommon.ui32CSC0Coeff67 = psYUVRegisters->u.sTAG.ui32CSC0Coeff67;
#if defined(EUR_CR_TPU_YUVSCALE)
		gc->sKickTA.sKickTACommon.ui32CSC0Coeff8 = psYUVRegisters->u.sTAG.ui32CSC0Coeff8;
#if defined(EUR_CR_TPU_YUVSCALE_DISABLE_MASK)
		gc->sKickTA.sKickTACommon.ui32CSCScale = psYUVRegisters->u.sTAG.ui32CSCScale;
#endif
#else
		gc->sKickTA.sKickTACommon.ui32CSC0Coeff89 = psYUVRegisters->u.sTAG.ui32CSC0Coeff89;
		gc->sKickTA.sKickTACommon.ui32CSC0Coeff1011 = psYUVRegisters->u.sTAG.ui32CSC0Coeff1011;
#endif
	}
#endif /* SGX_FEATURE_TAG_YUV_TO_RGB */

}

IMG_INTERNAL PVRSRV_ERROR CreateExternalTextureState(GLES1ExternalTexState *psExtTexState,
													IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
													PVRSRV_PIXEL_FORMAT ePixelFormat, IMG_UINT32 ui32ByteStride,
													IMG_UINT32 ui32Flags)
{
	IMG_UINT32 ui32Index, ui32StreamPlanes = 0;
	IMG_UINT32 ui32TexSize, ui32TexelStride;
	IMG_BOOL abUseStride[GLES1_MAX_TEX_PLANES] = {IMG_FALSE, IMG_FALSE, IMG_FALSE};
	IMG_UINT32 ui32WidthLog2, ui32HeightLog2;

	ui32TexSize = 1;
	ui32WidthLog2 = 0;

	/* find the bounding texture sizes (power of 2) */
	while(ui32TexSize < ui32Width)
	{
		ui32TexSize <<= 1;
		ui32WidthLog2++;
	}

	if(ui32WidthLog2 > GLES1_MAX_TEXTURE_MIPMAP_LEVELS)
	{
		PVR_DPF((PVR_DBG_ERROR,"CreateExternalTextureState: Unsupported width"));

		return PVRSRV_ERROR_INIT_FAILURE;
	}

	ui32TexSize = 1;
	ui32HeightLog2 = 0;

	/* find the bounding texture sizes (power of 2) */
	while(ui32TexSize < ui32Height)
	{
		ui32TexSize <<= 1;
		ui32HeightLog2++;
	}

	if(ui32HeightLog2 > GLES1_MAX_TEXTURE_MIPMAP_LEVELS)
	{
		PVR_DPF((PVR_DBG_ERROR,"CreateExternalTextureState: Unsupported height"));

		return PVRSRV_ERROR_INIT_FAILURE;
	}

#if(EURASIA_TAG_STRIDE_THRESHOLD > 0)
	if(ui32Width < EURASIA_TAG_STRIDE_THRESHOLD)
	{
		ui32TexelStride = (ui32Width + (EURASIA_TAG_STRIDE_ALIGN0 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN0 - 1);
	}
	else
#endif
	{
		ui32TexelStride = (ui32Width + (EURASIA_TAG_STRIDE_ALIGN1 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN1 - 1);
	}

	switch(ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		{
			/* if not the expected stride, try the texture stride extension */
			if(ui32ByteStride != (ui32TexelStride << 1))
			{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
				if((ui32ByteStride & 15U) == 0)
				{
					abUseStride[0] = IMG_TRUE;
				}
				else
#endif
				{
					PVR_DPF((PVR_DBG_ERROR,": Unsupported stride"));

					return PVRSRV_ERROR_INIT_FAILURE;
				}
			}
			
			ui32StreamPlanes += 1;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		case PVRSRV_PIXEL_FORMAT_XRGB8888:
		case PVRSRV_PIXEL_FORMAT_XBGR8888:
		{
			if(ui32ByteStride != (ui32TexelStride << 2))
			{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
				if((ui32ByteStride & 15U) == 0)
				{
					abUseStride[0] = IMG_TRUE;
				}
				else
#endif
				{
					PVR_DPF((PVR_DBG_ERROR,"CreateExternalTextureState: Unsupported stride"));

					return PVRSRV_ERROR_INIT_FAILURE;
				}
			}

			ui32StreamPlanes += 1;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			if(ui32ByteStride != (ui32TexelStride << 1))
			{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
				if((ui32ByteStride & 15U) == 0)
				{
					abUseStride[0] = IMG_TRUE;
				}
				else
#endif
				{
					PVR_DPF((PVR_DBG_ERROR,"CreateExternalTextureState: Unsupported stride"));

					return PVRSRV_ERROR_INIT_FAILURE;
				}
			}

			ui32StreamPlanes += 1;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY:
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY:
		{
			if(ui32ByteStride != (ui32TexelStride << 1))
			{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
				if((ui32ByteStride & 15U) == 0)
				{
					abUseStride[0] = IMG_TRUE;
				}
				else
#endif
				{
					PVR_DPF((PVR_DBG_ERROR,"CreateExternalTextureState: Unsupported stride"));

					return PVRSRV_ERROR_INIT_FAILURE;
				}
			}

			InitialiseYUVCoefficients(&(psExtTexState->sYUVRegisters), ePixelFormat, 
											ui32Flags);

			ui32StreamPlanes += 1;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV:
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU:
		{
			if(ui32ByteStride != (ui32TexelStride << 1))
			{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
				if((ui32ByteStride & 15U) == 0)
				{
					abUseStride[0] = IMG_TRUE;
				}
				else
#endif
				{
					PVR_DPF((PVR_DBG_ERROR,"CreateExternalTextureState: Unsupported stride"));

					return PVRSRV_ERROR_INIT_FAILURE;
				}
			}
		
			InitialiseYUVCoefficients(&(psExtTexState->sYUVRegisters), ePixelFormat, 
											ui32Flags);

			ui32StreamPlanes += 1;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_NV12:
		case PVRSRV_PIXEL_FORMAT_YV12:
		{
			IMG_UINT32 ui32UVPlaneWidth = ui32Width >> 1;
			IMG_UINT32 ui32UVTexelStride;

			/* Now check second plane restrictions */
#if(EURASIA_TAG_STRIDE_THRESHOLD > 0)
			if(ui32UVPlaneWidth< EURASIA_TAG_STRIDE_THRESHOLD)
			{
				ui32UVTexelStride = (ui32UVPlaneWidth + (EURASIA_TAG_STRIDE_ALIGN0 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN0 - 1);
			}
			else
#endif
			{
				ui32UVTexelStride = (ui32UVPlaneWidth + (EURASIA_TAG_STRIDE_ALIGN1 - 1)) & ~(EURASIA_TAG_STRIDE_ALIGN1 - 1);
			}


			if(ui32ByteStride != ui32TexelStride || ((ui32ByteStride >> 1) != ui32UVTexelStride))
			{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
				if(((ui32ByteStride & 15U) == 0) &&
				   ((((ui32ByteStride >> 1) & 15U) == 0) || ePixelFormat != PVRSRV_PIXEL_FORMAT_YV12))
				{
					abUseStride[0] = IMG_TRUE;
					abUseStride[1] = IMG_TRUE;

					if(ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12)
					{
						abUseStride[2] = IMG_TRUE;
					}
				}
				else
#endif
				{
					PVR_DPF((PVR_DBG_ERROR,"CreateExternalTextureState: Unsupported stride"));

					return PVRSRV_ERROR_INIT_FAILURE;
				}
			}

			InitialiseYUVCoefficients(&(psExtTexState->sYUVRegisters), ePixelFormat, 
											ui32Flags);

			ui32StreamPlanes += 2;

			if(ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12)
			{
				ui32StreamPlanes++;
			}
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"CreateExternalTextureState: Unsupported pixel format %d", ePixelFormat));

			return PVRSRV_ERROR_INIT_FAILURE;
		}
	}

	psExtTexState->aui32TexWord1[0] = EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE | 
				asSGXPixelFormat[ePixelFormat].aui32TAGControlWords[0][1] |
				((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
				((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);

	if(ePixelFormat == PVRSRV_PIXEL_FORMAT_NV12 || ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12)
	{
		psExtTexState->aui32TexWord1[1] = EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				 |
			asSGXPixelFormat[ePixelFormat].aui32TAGControlWords[1][1] |
			(((ui32Width >> 1)  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  |
			(((ui32Height >> 1) - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) ;

		if(ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12)
		{
			psExtTexState->aui32TexWord1[2] = EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE				 |
				asSGXPixelFormat[ePixelFormat].aui32TAGControlWords[2][1] |
				(((ui32Width >> 1)  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  |
				(((ui32Height >> 1) - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) ;
		}
	}

	psExtTexState->aui32TexStrideFlag = 0;

	for (ui32Index = 0; ui32Index < ui32StreamPlanes; ++ui32Index)
	{
		psExtTexState->aui32TexWord0[ui32Index] = 0;

		if (abUseStride[ui32Index])
		{
			IMG_UINT32 ui32Stride = ui32ByteStride;
		
			/* U and V planes on YV12 have 1/2 stride of Y plane */
			if((ePixelFormat == PVRSRV_PIXEL_FORMAT_YV12) && (ui32Index > 0))
			{
				ui32Stride >>= 1;
			}

			/* set the bit in stride flag */
			psExtTexState->aui32TexStrideFlag |= 1U << ui32Index;

			/* set the stride information in DOUTT0 and DOUTT1 */
			ui32Stride >>= 2;
			ui32Stride -= 1;

#if defined(EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT)
			psExtTexState->aui32TexWord1[ui32Index] &= EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK;
			psExtTexState->aui32TexWord1[ui32Index] |= EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT;

			psExtTexState->aui32TexWord0[ui32Index] &= (EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK);

			psExtTexState->aui32TexWord0[ui32Index] |= 
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDELO_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK) |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK);

#if defined(EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK)
			psExtTexState->aui32TexWord0[ui32Index] &= (EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK);

			psExtTexState->aui32TexWord0[ui32Index] |= 
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEEX_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEEX_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK);
#endif

#else /* EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT */

#if defined(SGX_FEATURE_VOLUME_TEXTURES)
			psExtTexState->aui32TexWord0[ui32Index] &= (EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK);

			psExtTexState->aui32TexWord0[ui32Index] |= EURASIA_PDS_DOUTT0_STRIDE |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDELO_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK) |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK);
	
#else /* SGX_FEATURE_VOLUME_TEXTURES */
			psExtTexState->aui32TexWord0[ui32Index] &= (EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK & EURASIA_PDS_DOUTT0_STRIDELO2_CLRMSK);

#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
			psExtTexState->aui32TexWord0[ui32Index] &= EURASIA_PDS_DOUTT0_STRIDEEX0_CLRMSK;
#endif

			psExtTexState->aui32TexWord0[ui32Index] |= EURASIA_PDS_DOUTT0_STRIDE |
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEEX0_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEEX0_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEEX0_CLRMSK) |
#endif
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO2_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDELO2_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO2_CLRMSK) |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT) << EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK);
	
			psExtTexState->aui32TexWord1[ui32Index] &= EURASIA_PDS_DOUTT1_STRIDELO1_CLRMSK;

#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
			psExtTexState->aui32TexWord1[ui32Index] &= EURASIA_PDS_DOUTT1_STRIDEEX1_CLRMSK;
#endif

			psExtTexState->aui32TexWord1[ui32Index] |= 
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
				(((ui32Stride >> EURASIA_PDS_DOUTT1_STRIDEEX1_STRIDE_SHIFT) << EURASIA_PDS_DOUTT1_STRIDEEX1_SHIFT) & ~EURASIA_PDS_DOUTT1_STRIDEEX1_CLRMSK) |
#endif
				(((ui32Stride >> EURASIA_PDS_DOUTT1_STRIDELO1_STRIDE_SHIFT) << EURASIA_PDS_DOUTT1_STRIDELO1_SHIFT) & ~EURASIA_PDS_DOUTT1_STRIDELO1_CLRMSK);
	
#endif /* SGX_FEATURE_VOLUME_TEXTURES */
#endif /* defined(EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT) */ 
		}
	}

	return PVRSRV_OK;
}



#endif /* GLES1_EXTENSION_TEXTURE_STREAM || GLES1_EXTENSION_EGL_IMAGE_EXTERNAL */
