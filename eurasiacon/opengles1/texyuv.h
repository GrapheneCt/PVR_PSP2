/**************************************************************************
 * Name         : texyuv.h
 * Author       : Jonathan Hamilton
 * Created      : 27/09/2010
 *
 * Copyright    : 2010 by Imagination Technologies Limited. All rights reserved.
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
 * $Date: 2011/06/02 10:28:57 $ $Revision: 1.6 $
 * $Log: texyuv.h $
 **************************************************************************/

#ifndef _TEXYUV_
#define _TEXYUV_

#if defined (__cplusplus)
extern "C" {
#endif


#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)

typedef struct TAG_CONVERT_COEFFS_TAG
{
#if defined(EUR_CR_TPU_YUVSCALE)
	IMG_UINT16 aui16Coeff[9];
	IMG_BOOL bScale;
#else
	IMG_UINT16 aui16Coeff[12];
#endif
}TAG_CONVERT_COEFFS;


typedef struct TAG_REGISTERS_TAG
{
	IMG_UINT32			ui32CSC0Coeff01;
	IMG_UINT32			ui32CSC0Coeff23;
	IMG_UINT32			ui32CSC0Coeff45;
	IMG_UINT32			ui32CSC0Coeff67;
	IMG_UINT32			ui32CSC1Coeff01;
	IMG_UINT32			ui32CSC1Coeff23;
	IMG_UINT32			ui32CSC1Coeff45;
	IMG_UINT32			ui32CSC1Coeff67;
#if defined(SGX_FEATURE_TAG_YUV_TO_RGB_HIPREC)
	IMG_UINT32			ui32CSC0Coeff89;
	IMG_UINT32			ui32CSC1Coeff89;
	IMG_UINT32			ui32CSC0Coeff1011;
	IMG_UINT32			ui32CSC1Coeff1011;
#else
	IMG_UINT32			ui32CSC0Coeff8;
	IMG_UINT32			ui32CSC1Coeff8;
	IMG_UINT32			ui32CSCScale;
#endif

}TAG_REGISTERS;

#endif
/* these are the coefficients to be used in the FIRH instruction */
typedef struct FIRH_CONVERT_COEFFS_TAG
{
	IMG_INT16 i16rY;
	IMG_INT16 i16rU;
	IMG_INT16 i16rV;
	IMG_INT16 i16gY;
	IMG_INT16 i16gU;
	IMG_INT16 i16gV;
	IMG_INT16 i16bY;
	IMG_INT16 i16bU;
	IMG_INT16 i16bV;
	IMG_UINT8 ui8rShift;
	IMG_UINT8 ui8gShift;
	IMG_UINT8 ui8bShift;
	IMG_INT16 i16rConst;
	IMG_INT16 i16gConst;
	IMG_INT16 i16bConst;
}FIRH_CONVERT_COEFFS;

typedef struct FIRH_REGISTERS_TAG
{
	IMG_UINT32			ui32Filter0Left;
#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
	IMG_UINT32			ui32Filter0Centre;
#endif
	IMG_UINT32			ui32Filter0Right;
	IMG_UINT32			ui32Filter0Extra;
	IMG_UINT32			ui32Filter1Left;
#if defined(SGX_FEATURE_USE_HIGH_PRECISION_FIR_COEFF)
	IMG_UINT32			ui32Filter1Centre;
#endif
	IMG_UINT32			ui32Filter1Right;
	IMG_UINT32			ui32Filter1Extra;
	IMG_UINT32			ui32Filter2Left;
	IMG_UINT32			ui32Filter2Right;
	IMG_UINT32			ui32Filter2Extra;
	IMG_UINT32			ui32FilterTable;

}FIRH_REGISTERS;

typedef struct YUV_CONVERT_COEFFS_TAG
{
	IMG_BOOL bUseFIRH;

	union 
	{
		FIRH_CONVERT_COEFFS	sFIRH;
#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
		TAG_CONVERT_COEFFS	sTAG;
#endif
	}u;
}YUV_CONVERT_COEFFS;

typedef struct YUV_REGISTERS_TAG
{
	IMG_BOOL bUseFIRH;

	union 
	{
		FIRH_REGISTERS	sFIRH;
#if defined(SGX_FEATURE_TAG_YUV_TO_RGB)
		TAG_REGISTERS	sTAG;
#endif
	}u;
}YUV_REGISTERS;

/* The maximum number of planes per texture  (3 for YV12, Y, U and V) */
#define GLES1_MAX_TEX_PLANES 3

typedef struct GLES1ExternalTexState_TAG {
	YUV_REGISTERS	sYUVRegisters;
	IMG_UINT32		aui32TexWord0[GLES1_MAX_TEX_PLANES];
	IMG_UINT32		aui32TexWord1[GLES1_MAX_TEX_PLANES];
	IMG_UINT32		aui32TexStrideFlag;
} GLES1ExternalTexState;

IMG_VOID InitialiseYUVCoefficients(YUV_REGISTERS *psYUVRegisters, PVRSRV_PIXEL_FORMAT ePixelFormat, IMG_UINT32 ui32Flags);

IMG_VOID CopyYUVRegisters(GLES1Context *gc, YUV_REGISTERS *psYUVRegisters);

/* FIXME: Really should be in some shared external texture header */
IMG_INTERNAL PVRSRV_ERROR CreateExternalTextureState(GLES1ExternalTexState *psExtTexState,
						IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
						PVRSRV_PIXEL_FORMAT ePixelFormat, IMG_UINT32 ui32ByteStride,
						IMG_UINT32 ui32Flags);


#if defined (__cplusplus)
}
#endif
#endif /* _TEXYUV_ */

/*****************************************************************************
 End of file (texyuv.h)
*****************************************************************************/

