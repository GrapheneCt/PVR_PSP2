/*!****************************************************************************
@File           sgxtransfer_utils.c

@Title          Device specific transfer queue routines

@Author         Imagination Technologies

@date           08/02/06

@Copyright      Copyright 2007-2010 by Imagination Technologies Limited.
                All rights reserved. No part of this software, either material
                or conceptual may be copied or distributed, transmitted,
                transcribed, stored in a retrieval system or translated into
                any human or computer language in any form by any means,
                electronic, mechanical, manual or other-wise, or disclosed
                to third parties without the express written permission of
                Imagination Technologies Limited, Home Park Estate,
                Kings Langley, Hertfordshire, WD4 8LZ, U.K.

@Platform       Generic

@Description    Device specific functions

@DoxygenVer

******************************************************************************/

/******************************************************************************
Modifications :-
$Log: sgxtransfer_utils.c $
.#
 --- Revision Logs Removed --- 
******************************************************************************/

#if defined(TRANSFER_QUEUE)
#include <stddef.h>

#include "img_types.h"
#include "services.h"
#include "servicesext.h"
#include "sgxapi.h"
#include "servicesint.h"
#include "pvr_debug.h"
#include "pixevent.h"
#include "pvr_bridge.h"
#include "sgxinfo.h"
#include "sgxinfo_client.h"
#include "sgxtransfer_client.h"
#include "sgxutils_client.h"
#include "sgx_bridge_um.h"
#include "pdump_um.h"
#include "usecodegen.h"

#include "sgxpixfmts.h"

/* disable warning about unused static globals */
/* PRQA S 3207 END_PDS_HEADERS */
#include "pds_tq_0src_primary.h"
#include "pds_tq_1src_primary.h"
#include "pds_tq_2src_primary.h"
#include "pds_tq_3src_primary.h"
#include "pds_iter_primary.h"

#include "pds_secondary.h"
#include "pds_dma_secondary.h"
#include "pds_1attr_secondary.h"
#include "pds_3attr_secondary.h"
#include "pds_4attr_secondary.h"
#include "pds_4attr_dma_secondary.h"
#include "pds_5attr_dma_secondary.h"
#include "pds_6attr_secondary.h"
#include "pds_7attr_secondary.h"


#include "pds.h"
/* PRQA L:END_PDS_HEADERS */

#include "transferqueue_use_labels.h"
#include "subtwiddled_eot_labels.h"
/* PRQA L:END_PDS_HEADERS */

#ifndef FIXEDPT_FRAC
#define FIXEDPT_FRAC	20
#endif
#define FIXEDPT_INT		(32-FIXEDPT_FRAC)
#define FLOAT32_MANT    23

#define FLOAT16_MANT	10

#if defined(SGX545)
	#define PRMSTART_MODE 0
#endif

/*****************************************************************************

 @Function	ByteToF16

 @Description	It's all in the name...

 @Input		byte - value to be used

 @Return	ui32Float16

*****************************************************************************/
IMG_INTERNAL IMG_UINT32 SGXTQ_ByteToF16(IMG_UINT32 byte)
{


	/* float16 (aka half or binary16) is 1 sign bit, 5 exponent bits,
		10 mantissa bits, with an exponent bias of 15 */
	/* float (aka single or binary32) is 1 sign bit, 8 exponent bits,
		23 mantissa bits, with an exponent bias of 127 */

	IMG_UINT32 ui32Float16, ui32Float32;
	IMG_UINT32 ui32SExponent, ui32SMantissa;
	IMG_UINT32 ui32HExponent, ui32HMantissa;
	IMG_INT32 i32SignedExponent;

	byte = byte & 0xFF;
	if (byte == 0)
	{
		ui32Float16 = 0x0;
	}
	else if (byte == 255)
	{
		/* 1.0 is 0011 1100 0000 0000b */
		ui32Float16 = 0x00003c00;
	}
	else
	{
		/* We want a float in the range of 0.0 to 1.0. */
		ui32Float32 = SGXTQ_FloatIntDiv(byte, 255);
		/* Convert float to half, using knowledge of range...*/
		/* Ignore zero, denormalised, NaN and infinity cases.*/
		/* Also ignore the sign, since we know it is always positive.*/
		ui32SExponent = ui32Float32 & 0x7F800000;
		i32SignedExponent = (IMG_INT)(ui32SExponent >> 23) - 127 + 15;
		ui32HExponent =  (IMG_UINT32)(i32SignedExponent) << 10;
		ui32SMantissa = ui32Float32 & 0x007FFFFF;
		ui32HMantissa = ui32SMantissa >> 13;
		ui32Float16 = ui32HExponent | ui32HMantissa;
	}
	return ui32Float16;
}
/*****************************************************************************

 @Function	SGXTQ_FindNearestLog2

 @Description	It's all in the name...

 @Input		ui32Num - value to be used

 @Return	Have a guess!

*****************************************************************************/
static INLINE IMG_UINT32 SGXTQ_FindNearestLog2(IMG_UINT32 ui32Num)
{
	IMG_UINT32  ui32Result = 0;

	if (ui32Num)
		ui32Num --;

	if (ui32Num & 0xffff0000)
	{
		ui32Result += 16;
		ui32Num >>= 16;
	}
	if (ui32Num & 0x0000ff00)
	{
		ui32Result += 8;
		ui32Num >>= 8;
	}
	if (ui32Num & 0x000000f0)
	{
		ui32Result += 4;
		ui32Num >>= 4;
	}

	while (ui32Num)
	{
		ui32Result ++;
		ui32Num >>= 1;
	}

	return ui32Result;
}


/*!
******************************************************************************
 @Function	SGXTQ_FixedIntDiv
 @Description	Divides two 16-bit integers and returns a fixed point result.
 @Input		ui16A
 @Input		ui16B
 @Return	result
******************************************************************************/
IMG_INTERNAL IMG_UFIXED SGXTQ_FixedIntDiv(IMG_UINT16 ui16A, IMG_UINT16 ui16B)
{
	if (ui16A == 0U || ui16B == 0U)
	{
		return 0;
	}

	PVR_ASSERT((((IMG_UINT32)ui16A) & ~((1UL << FIXEDPT_INT) - 1U)) == 0);
	return (((IMG_UINT32)ui16A) << FIXEDPT_FRAC) / ui16B;
}


/*!
******************************************************************************
 @Function	SGXTQ_FloatIntDiv
 @Description	Does what you'd expect, slowly.
 @Input		ui32A
 @Input		ui32B
 @Return	result
******************************************************************************/
IMG_INTERNAL IMG_UINT32 SGXTQ_FloatIntDiv(IMG_UINT32 ui32A, IMG_UINT32 ui32B)
{
	if (ui32A == 0UL || ui32B == 0UL)
		return 0UL;
	if (ui32A == ui32B)
		return FLOAT32_ONE;

	return SGXTQ_FixedToFloat(SGXTQ_FixedIntDiv((IMG_UINT16)ui32A, (IMG_UINT16)ui32B));
}


/*!
******************************************************************************
 @Function	CountLeadingZeroes
 @Description	Does what you'd expect.
 @Input		ui32Num
 @Return	result
******************************************************************************/
static INLINE IMG_UINT32 CountLeadingZeroes(IMG_UINT32 ui32Num)
{
	IMG_UINT32 i = 0;

	if ((ui32Num & 0xffff0000) == 0)
	{
		ui32Num <<= 16;
		i += 16;
	}
	if ((ui32Num & 0xff000000) == 0)
	{
		ui32Num <<= 8;
		i += 8;
	}
	if ((ui32Num & 0xf0000000) == 0)
	{
		ui32Num <<= 4;
		i += 4;
	}
	if ((ui32Num & 0xc0000000) == 0)
	{
		ui32Num <<= 2;
		i += 2;
	}
	if ((ui32Num & 0x80000000) == 0)
	{
		i += 1;
		if ((ui32Num & 0x40000000) == 0)
			i += 1;
	}

	return i;
}

/*!
******************************************************************************
 @Function	SGXTQ_FixedToFloat
 @Description
	Convert the 32-bit fixed point format defined in this file to an IEEE-754 32-bit
 	float.
 @Input		ufxVal
 @Return	IEEE-754 32-bit float hexadecimal representation
******************************************************************************/
IMG_INTERNAL IMG_UINT32 SGXTQ_FixedToFloat(IMG_UFIXED ufxVal)
{
	IMG_UINT32 ui32OnePos;
	IMG_UINT32 ui32FracSize;
	IMG_UINT32 ui32Result;
	IMG_UINT8  ui8Exp;
    IMG_UINT32 ui32Exp;

	/* TODO: Could probably be made faster */
	if (!ufxVal)
		return 0;

	/* Find position of leading 1. */
	ui32OnePos = CountLeadingZeroes(ufxVal) + 1;

	/* Calcuulate unbiased exponent. */
    ui32Exp = FIXEDPT_INT - (IMG_UINT8)ui32OnePos + 127;
	ui8Exp = (IMG_UINT8) ui32Exp;

	/* How big is the resulting fractional part? */
	ui32FracSize = 32UL - ui32OnePos;

	/* Mask off leading 1. */
	ufxVal &= (1UL << ui32FracSize) - 1;

	if (ui32FracSize > FLOAT32_MANT)
	{
		IMG_UINT32 ui32Sig;
		IMG_UINT32 ui32Excess;

		/* Round and truncate. */
		ui32Excess = ui32FracSize - FLOAT32_MANT;
		ui32Sig = ufxVal + (1UL << (ui32Excess - 1));
		ui32Result = ((ui8Exp) << FLOAT32_MANT) + (ui32Sig >> (ui32Excess));
	}
	else
		ui32Result = ((ui8Exp) << FLOAT32_MANT) + (ufxVal << (FLOAT32_MANT - ui32FracSize));

	return ui32Result;
}

/*!
******************************************************************************
 @Function	SGXTQ_FixedToFloat
 @Description	Convert the 16-bit fixed point format
 @Input		ufxVal
 @Return	16-bit float hexadecimal representation
******************************************************************************/
IMG_INTERNAL IMG_UINT32 SGXTQ_FixedToF16(IMG_UFIXED ufxVal)
{
	IMG_UINT32 ui32OnePos;
	IMG_UINT32 ui32FracSize;
	IMG_UINT32 ui32Result;
	IMG_UINT8  ui8Exp;
    IMG_UINT32 ui32Exp;

	/* TODO: Could probably be made faster */
	if (!ufxVal)
		return 0;

	/* Find position of leading 1. */
	ui32OnePos = CountLeadingZeroes(ufxVal) + 1;

	/* Calculate unbiased exponent. */
	ui32Exp = FIXEDPT_INT - (IMG_UINT8)ui32OnePos + 15;
    ui8Exp  = (IMG_UINT8) ui32Exp;


	/* How big is the resulting fractional part? */
	ui32FracSize = 32UL - ui32OnePos;

	/* Mask off leading 1. */
	ufxVal &= (1UL << ui32FracSize) - 1;

	if (ui32FracSize > FLOAT16_MANT)
	{
		IMG_UINT32 ui32Sig;
		IMG_UINT32 ui32Excess;

		/* Round and truncate. */
		ui32Excess = ui32FracSize - FLOAT16_MANT;
		ui32Sig = ufxVal + (1UL << (ui32Excess - 1));
		ui32Result = ((ui8Exp) << FLOAT16_MANT) + (ui32Sig >> (ui32Excess));
	}
	else
		ui32Result = ((ui8Exp) << FLOAT16_MANT) + (ufxVal << (FLOAT16_MANT - ui32FracSize));

	return ui32Result;
}


INLINE static PVRSRV_ERROR SGXTQ_GetPBEPackedPassThrough(IMG_UINT32	ui32BitsPP,
														 IMG_UINT32	* pui32PBEPackMode)
{
	switch (ui32BitsPP)
	{
		case 32:
		{
#if defined(EURASIA_PIXELBE2SB_PACKMODE_PT1)
			*pui32PBEPackMode = EURASIA_PIXELBE2SB_PACKMODE_PT1;
#else
#if defined(EURASIA_PIXELBES0LO_PACKMODE_PT1)
			*pui32PBEPackMode = EURASIA_PIXELBES0LO_PACKMODE_PT1;
#else
			*pui32PBEPackMode = EURASIA_PIXELBE2S0_PACKMODE_PT1;
#endif
#endif
			break;
		}
		case 64:
		{
#if defined(EURASIA_PIXELBE2SB_PACKMODE_PT2)
			*pui32PBEPackMode = EURASIA_PIXELBE2SB_PACKMODE_PT2;
#else
#if defined(EURASIA_PIXELBES0LO_PACKMODE_PT2)
			*pui32PBEPackMode = EURASIA_PIXELBES0LO_PACKMODE_PT2;
#else
			*pui32PBEPackMode = EURASIA_PIXELBE2S0_PACKMODE_PT2;
#endif
#endif
			break;
		}
		case 128:
		{
#if defined(EURASIA_PIXELBE2SB_PACKMODE_PT4)
			*pui32PBEPackMode = EURASIA_PIXELBE2SB_PACKMODE_PT4;
#else
#if defined(EURASIA_PIXELBES0LO_PACKMODE_PT4)
			*pui32PBEPackMode = EURASIA_PIXELBES0LO_PACKMODE_PT4;
#else
			*pui32PBEPackMode = EURASIA_PIXELBE2S0_PACKMODE_PT4;
#endif
#endif
			break;
		}
		case 256:
		{
#if defined(EURASIA_PIXELBE2SB_PACKMODE_PT8)
			*pui32PBEPackMode = EURASIA_PIXELBE2SB_PACKMODE_PT8;
#else
#if defined(EURASIA_PIXELBES0LO_PACKMODE_PT8)
			*pui32PBEPackMode = EURASIA_PIXELBES0LO_PACKMODE_PT8;
#else
			*pui32PBEPackMode = EURASIA_PIXELBE2S0_PACKMODE_PT8;
#endif
#endif
			break;
		}
		default:
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}
	}
	return PVRSRV_OK;
}

INLINE static IMG_UINT32 SGXTQ_GetMONOSrcSel(IMG_UINT32			ui32PBEPackMode,
											PVRSRV_PIXEL_FORMAT	eSrcPix,
											PVRSRV_PIXEL_FORMAT	eDstPix)
{
	switch (ui32PBEPackMode)
	{
#if defined(EURASIA_PIXELBE2SB_PACKMODE_MONO8)
		case EURASIA_PIXELBE2SB_PACKMODE_MONO8:
#else
#if defined(EURASIA_PIXELBES0LO_PACKMODE_MONO8)
		case EURASIA_PIXELBES0LO_PACKMODE_MONO8:
#else
		case EURASIA_PIXELBE2S0_PACKMODE_MONO8:
#endif
#endif
		{
			if (eDstPix == eSrcPix)
			{
				return 3;
			}
			else
			{
				switch (eDstPix)
				{
					case PVRSRV_PIXEL_FORMAT_A8:
					case PVRSRV_PIXEL_FORMAT_MONO8:
					{
						return 0;
					}
					case PVRSRV_PIXEL_FORMAT_R8:
					default:
					{
						return 3;
					}
				}
			}
		}
#if defined(EURASIA_PIXELBE2SB_PACKMODE_MONO16)
		case EURASIA_PIXELBE2SB_PACKMODE_MONO16:
#else
#if defined(EURASIA_PIXELBES0LO_PACKMODE_MONO16)
		case EURASIA_PIXELBES0LO_PACKMODE_MONO16:
#else
		case EURASIA_PIXELBE2S0_PACKMODE_MONO16:
#endif
#endif
		{
			return 1;
		}
		default:
			return 0;
	}
}

INLINE static IMG_VOID SGXTQ_SetGenericState(IMG_UINT32 ui32Bpp, 
																						IMG_UINT32 *pui32TagState, 
																						IMG_UINT32 *pui32PBEPackMode)
{

	/* overwrite with generalized PackMode and TagState */
	switch(ui32Bpp)
	{
		case 8: 
			pui32TagState[1] &= EURASIA_PDS_DOUTT1_TEXFORMAT_CLRMSK;
			pui32TagState[1] |= EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
#if defined(EURASIA_PIXELBES0LO_PACKMODE_MONO8)
			pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_TEXFEXT;
			pui32TagState[3] &= EURASIA_PDS_DOUTT3_SWIZ_CLRMASK;
			pui32TagState[3] |= EURASIA_PDS_DOUTT3_SWIZ_R; 
			*pui32PBEPackMode = EURASIA_PIXELBES0LO_PACKMODE_MONO8 | EURASIA_PIXELBES0LO_SWIZ_R;
#else
#if defined(EURASIA_PIXELBE2SB_PACKMODE_MONO8)
			pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_TEXFEXT;
			*pui32PBEPackMode = EURASIA_PIXELBE2SB_PACKMODE_MONO8;
#else
			pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_CHANREPLICATE;
			*pui32PBEPackMode = EURASIA_PIXELBE2S0_PACKMODE_MONO8;
#endif
#endif
			break;

		case 16: 
			pui32TagState[1] &= EURASIA_PDS_DOUTT1_TEXFORMAT_CLRMSK;
			pui32TagState[1] |= EURASIA_PDS_DOUTT1_TEXFORMAT_U88; 
#if defined(EURASIA_PIXELBES0LO_PACKMODE_MONO16)
			pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_TEXFEXT;
			pui32TagState[3] &= EURASIA_PDS_DOUTT3_SWIZ_CLRMASK;
			pui32TagState[3] |= EURASIA_PDS_DOUTT3_SWIZ_R; 
			*pui32PBEPackMode = EURASIA_PIXELBES0LO_PACKMODE_MONO16 | EURASIA_PIXELBES0LO_SWIZ_R; 
#else
#if defined(EURASIA_PIXELBE2SB_PACKMODE_MONO16)
			pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_TEXFEXT;
			*pui32PBEPackMode = EURASIA_PIXELBE2SB_PACKMODE_MONO16;
#else
			pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_CHANREPLICATE;
			*pui32PBEPackMode = EURASIA_PIXELBE2S0_PACKMODE_MONO16;
#endif
#endif
			break;

		case 32:
#if defined(EURASIA_PIXELBES0LO_PACKMODE_U8888)
			pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_TEXFEXT;
			pui32TagState[1] &= EURASIA_PDS_DOUTT1_TEXFORMAT_CLRMSK;
			pui32TagState[1] |= EURASIA_PDS_DOUTT1_TEXFORMAT_U8888; 
			pui32TagState[3] &= EURASIA_PDS_DOUTT3_SWIZ_CLRMASK;
			pui32TagState[3] |= EURASIA_PDS_DOUTT3_SWIZ_ABGR; 
			*pui32PBEPackMode = EURASIA_PIXELBES0LO_PACKMODE_U8888 | EURASIA_PIXELBES0LO_SWIZ_ABGR;
#else
			pui32TagState[1] &= EURASIA_PDS_DOUTT1_TEXFORMAT_CLRMSK;
			pui32TagState[1] |= EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8; 
#if defined(EURASIA_PIXELBE2SB_PACKMODE_A8R8G8B8)
			pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_TEXFEXT;
			*pui32PBEPackMode = EURASIA_PIXELBE2SB_PACKMODE_A8R8G8B8; 	
#else
			pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_CHANREPLICATE;
			*pui32PBEPackMode = EURASIA_PIXELBE2S0_PACKMODE_A8R8G8B8;	
#endif
#endif
			break;
		default:;
			/* Keep old configuration */
	}

} 				

/*****************************************************************************
 * Function Name		:	SGXTQ_GetPixelFormats
 * Inputs				:	eSrcPix		- set to PVRSRV_PIXEL_FORMAT_UNKNOWN if don't
 *										  have a src
 *							eDstPix		- Dst pixel format
 *							pui32TAGFormatFormat DOUTT dwords, pass in the appropriate
 *										  bit of the PDSValues. set to IMG_NULL when don't
 *										  have a src.
 *							pbTagPlanarizerNeeded if not IMG_NULL then pointed value
 *										  will be set to IMG_TRUE if the compress texture 
 										  chunking is needed
 *							pui32PBEState PBE state ptr.
 *							*pui32PassesRequired pointed value will be set to number of required
 *										  passes if that's not 1
 * Returns				:	PVRSRV_ERROR  error if we can't handle the formats
 * Description			:	Calculates the TAG/PBE states for given src/dst pixel format.
******************************************************************************/
IMG_INTERNAL PVRSRV_ERROR SGXTQ_GetPixelFormats(PVRSRV_PIXEL_FORMAT	eSrcPix,
										IMG_BOOL					bSrcPacked,
										PVRSRV_PIXEL_FORMAT			eDstPix,
										IMG_BOOL					bDstPacked,
										SGXTQ_FILTERTYPE 			eFilter,
										IMG_UINT32					* pui32TagState,
										IMG_BOOL					* pbTagPlanarizerNeeded,
										IMG_UINT32					* pui32TAGBpp,
										IMG_UINT32					* pui32PBEState,
										IMG_UINT32					* pui32PBEBpp,
										IMG_UINT32					ui32Pass,
										IMG_UINT32					* pui32PassesRequired)
{
	IMG_UINT32				ui32PBEPackMode = EURASIA_PIXELBE_PACKMODE_NONE;
	IMG_UINT32				ui32SrcSel = 0;
	PVRSRV_ERROR			eError;

	PVR_UNREFERENCED_PARAMETER(pbTagPlanarizerNeeded);

	if (eSrcPix == PVRSRV_PIXEL_FORMAT_UNKNOWN)
	{

		const SGX_PIXEL_FORMAT	* psDstPixelFmt = & asSGXPixelFormat[(IMG_UINT32)eDstPix];
		IMG_UINT32				ui32DstBitsPP = psDstPixelFmt->ui32TotalBitsPerPixel;

		if (psDstPixelFmt->ui32NumChunks == 0)
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}

		if (eDstPix == PVRSRV_PIXEL_FORMAT_A8B8G8R8_SINT
				|| eDstPix == PVRSRV_PIXEL_FORMAT_A8B8G8R8_SNORM)
		{
			/* we have to do the signed unsigned conversion, but otherwise this is a PT1*/
			(IMG_VOID)SGXTQ_GetPBEPackedPassThrough(SGXTQ_BITS_IN_PLANE, & ui32PBEPackMode);

			if (pui32PBEBpp != IMG_NULL)
			{
				*pui32PBEBpp = SGXTQ_BITS_IN_PLANE >> 3;
			}
		}
		else if (psDstPixelFmt->ui32NumChunks == 1)	/* UNKNOWN to a single chunk */
		{
			/* codegen gives PT1 expecting a shader to work around - but here
			 * we already have a specialized shader hence the UNKNOWN*/
			if ((eDstPix == PVRSRV_PIXEL_FORMAT_ARGB8332)
			|| (eDstPix == PVRSRV_PIXEL_FORMAT_A2RGB10)
			|| (eDstPix == PVRSRV_PIXEL_FORMAT_A2BGR10))
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			ui32PBEPackMode = psDstPixelFmt->ui32PBEFormat;

			ui32SrcSel = SGXTQ_GetMONOSrcSel(ui32PBEPackMode, eSrcPix, eDstPix);

			if (pui32PBEBpp != IMG_NULL)
			{
				*pui32PBEBpp = ui32DstBitsPP >> 3;
			}
		}
		else
		{
			/* UNKNOWN to multi chunked - packed*/
			if (bDstPacked)
			{
				eError = SGXTQ_GetPBEPackedPassThrough(psDstPixelFmt->ui32TotalBitsPerPixel,
						&ui32PBEPackMode);
				if (PVRSRV_OK != eError)
				{
					return eError;
				}
				if (pui32PassesRequired != IMG_NULL)
				{
					*pui32PassesRequired *= ui32DstBitsPP >> 5;
				}
				ui32SrcSel = ui32Pass;
				if (pui32PBEBpp != IMG_NULL)
				{
					*pui32PBEBpp = ui32DstBitsPP >> 3;
				}
			}
			/* UNKNOWN to multi chunked - with dw planes.*/
			else
			{
				if (pui32PassesRequired != IMG_NULL)
				{
					*pui32PassesRequired *= psDstPixelFmt->ui32NumChunks;

					/* make sure we agree with codegen */
					PVR_ASSERT(* pui32PassesRequired == ui32DstBitsPP / SGXTQ_BITS_IN_PLANE);
				}

				(IMG_VOID)SGXTQ_GetPBEPackedPassThrough(SGXTQ_BITS_IN_PLANE, & ui32PBEPackMode);

				/* per plane */
				if (pui32PBEBpp != IMG_NULL)
				{
					*pui32PBEBpp = SGXTQ_BITS_IN_PLANE >> 3;
				}
			}
		}
	}
	else /* End: Source pixel format is unknown*/
	{
		if (eDstPix == PVRSRV_PIXEL_FORMAT_UNKNOWN)
		{

			const SGX_PIXEL_FORMAT	* psSrcPixelFmt = & asSGXPixelFormat[(IMG_UINT32) eSrcPix];
			IMG_UINT32				ui32SrcBitsPP = psSrcPixelFmt->ui32TotalBitsPerPixel;
	
			if (psSrcPixelFmt->ui32NumChunks == 0)
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}
	
			if (pui32TagState != IMG_NULL)
			{
				IMG_UINT32	i;
	
	#if defined(EURASIA_PDS_DOUTT0_TEXFEXT)
				pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_TEXFEXT;
	#endif
	#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
				pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_CHANREPLICATE;
	#endif
	
				pui32TagState[1] &= EURASIA_PDS_DOUTT1_TEXFORMAT_CLRMSK;
	
	#if defined(SGX_FEATURE_TAG_SWIZZLE)
				pui32TagState[3] &= EURASIA_PDS_DOUTT3_SWIZ_CLRMASK;
	#endif
	
				/* if we have 1 chunk it is necessary that we call this on the first pass*/
				for (i = 0; i < EURASIA_TAG_TEXTURE_STATE_SIZE; i++)
				{
					pui32TagState[i] |= psSrcPixelFmt->aui32TAGControlWords[ui32Pass][i];
				}
			}
	
			/* single source */
			if (psSrcPixelFmt->ui32NumChunks == 1)
			{
				if (! bSrcPacked )
				{
					/* There is no single chunk planar pixel */
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
			}
			else
			{
				/* TODO : support me! */
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}

			if (pui32TAGBpp != IMG_NULL)
			{
				*pui32TAGBpp = (ui32SrcBitsPP / psSrcPixelFmt->ui32NumChunks) >> 3;
			}

		}
		else /* End: Destination pixel format is unknown*/
		{

			/* Source and Destination Pixel formats are both known */ 
			const SGX_PIXEL_FORMAT	* psSrcPixelFmt = & asSGXPixelFormat[(IMG_UINT32) eSrcPix];
			IMG_UINT32				ui32SrcBitsPP = psSrcPixelFmt->ui32TotalBitsPerPixel;
			const SGX_PIXEL_FORMAT	* psDstPixelFmt = & asSGXPixelFormat[(IMG_UINT32)eDstPix];
			IMG_UINT32				ui32DstBitsPP = psDstPixelFmt->ui32TotalBitsPerPixel;
	
			if (psSrcPixelFmt->ui32NumChunks == 0)
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}
	
			if (pui32TagState != IMG_NULL)
			{
				IMG_UINT32	i;
	
	#if defined(EURASIA_PDS_DOUTT0_TEXFEXT)
				pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_TEXFEXT;
	#endif
	#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
				pui32TagState[0] &= ~ EURASIA_PDS_DOUTT0_CHANREPLICATE;
	#endif
	
				pui32TagState[1] &= EURASIA_PDS_DOUTT1_TEXFORMAT_CLRMSK;
	
	#if defined(SGX_FEATURE_TAG_SWIZZLE)
				pui32TagState[3] &= EURASIA_PDS_DOUTT3_SWIZ_CLRMASK;
	#endif
	
				/* if we have 1 chunk it is necessary that we call this on the first pass*/
				for (i = 0; i < EURASIA_TAG_TEXTURE_STATE_SIZE; i++)
				{
					pui32TagState[i] |= psSrcPixelFmt->aui32TAGControlWords[ui32Pass][i];
				}
			}
	
			/* single source */
			if (psSrcPixelFmt->ui32NumChunks == 1)
			{
				if (psDstPixelFmt->ui32NumChunks == 1)
				{
					if ((eDstPix == PVRSRV_PIXEL_FORMAT_ARGB8332)	/* TODO use special shader*/
						|| (eDstPix == PVRSRV_PIXEL_FORMAT_A2RGB10)
						|| (eDstPix == PVRSRV_PIXEL_FORMAT_A2BGR10))
					{
						return PVRSRV_ERROR_NOT_SUPPORTED;
					}
	
					if ((eDstPix != eSrcPix) && (!bSrcPacked || !bDstPacked))
					{
						/* There is no single chunk planar pixel */
						return PVRSRV_ERROR_INVALID_PARAMS;
					}
	
					ui32PBEPackMode = psDstPixelFmt->ui32PBEFormat;
	
					/* The pixel format definitions (sgxXXXpixfmts.h) have repeatedly */
					/* been modified in the past, in order to work around HW bugs. */
					/* Since the PBE setup is derived from those definitions, those */
					/* workarounds have led to incorrect values for the PBE setup. */
					/* To minimize the impact of such changes onto the PBE setup, */
					/* we bypass the pixel format definitions for simple cases. */
					/* For instance, if the source's and destination's pixel formats */
					/* are identical, then we just use a generic PBE setup */
					/* which doesn't imply any pixel format conversion */
	
					if (eDstPix == eSrcPix && eFilter == SGXTQ_FILTERTYPE_POINT)
					{
						/* overwrite PBE and TagState setup */
						SGXTQ_SetGenericState(psSrcPixelFmt->ui32TotalBitsPerPixel, pui32TagState,&ui32PBEPackMode);
					}
						
					ui32SrcSel = SGXTQ_GetMONOSrcSel(ui32PBEPackMode, eSrcPix, eDstPix);
	
					if (pui32TAGBpp != IMG_NULL)
					{
						*pui32TAGBpp = ui32SrcBitsPP >> 3;
					}
					if (pui32PBEBpp != IMG_NULL)
					{
						*pui32PBEBpp = ui32DstBitsPP >> 3;
					}
				}
				else
				{
					/* TODO not supported yet*/
					return PVRSRV_ERROR_NOT_SUPPORTED;
				}
			}
			else
			{
				if (psDstPixelFmt->ui32NumChunks == 1)
				{
					/* TODO not supported yet*/
					return PVRSRV_ERROR_NOT_SUPPORTED;
				}
				else
				{
					if (psSrcPixelFmt->ui32NumChunks == psDstPixelFmt->ui32NumChunks)
					{
						if (pui32PassesRequired != IMG_NULL)
						{
							*pui32PassesRequired *= psSrcPixelFmt->ui32NumChunks; 
						}
					}
					else
					{
						return PVRSRV_ERROR_NOT_SUPPORTED;
					}
	
					if (bSrcPacked)
					{
						/* TODO : support me! */
						return PVRSRV_ERROR_NOT_SUPPORTED;
					}
					else
					{
						if (bDstPacked)
						{
							eError = SGXTQ_GetPBEPackedPassThrough(psDstPixelFmt->ui32TotalBitsPerPixel,
									&ui32PBEPackMode);
							if (PVRSRV_OK != eError)
							{
								return eError;
							}
							ui32SrcSel = ui32Pass;
							if (pui32PBEBpp != IMG_NULL)
							{
								*pui32PBEBpp = ui32DstBitsPP >> 3;
							}
						}
						else
						{
							(IMG_VOID)SGXTQ_GetPBEPackedPassThrough(SGXTQ_BITS_IN_PLANE, & ui32PBEPackMode);
	
							/* per plane */
							if (pui32PBEBpp != IMG_NULL)
							{
								*pui32PBEBpp = SGXTQ_BITS_IN_PLANE >> 3;
							}
	
							PVR_ASSERT((pui32PassesRequired == IMG_NULL)
									|| (* pui32PassesRequired == ui32DstBitsPP / SGXTQ_BITS_IN_PLANE));
						}
					}
	
					if (pui32TAGBpp != IMG_NULL)
					{
						*pui32TAGBpp = (ui32SrcBitsPP / psSrcPixelFmt->ui32NumChunks) >> 3;
					}
				}
			}
		}
	}
	
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4) && ! defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	if( (eSrcPix != eDstPix) &&
		((eSrcPix == PVRSRV_PIXEL_FORMAT_A2RGB10) ||
		 (eSrcPix == PVRSRV_PIXEL_FORMAT_A2B10G10R10) ||
		 (eSrcPix == PVRSRV_PIXEL_FORMAT_A2B10G10R10_UNORM)) )
	{
		pui32TagState[3] &= EURASIA_PDS_DOUTT3_FCONV_CLRMSK;
		pui32TagState[3] |= (EURASIA_PDS_DOUTT3_FCONV_C10);//convert to c10
		
		if ( eSrcPix == PVRSRV_PIXEL_FORMAT_A2B10G10R10_UNORM )
		{
			pui32TagState[3] |= (EURASIA_PDS_DOUTT3_FCNORM);//normalize the input data
		}
	}
#endif	
	if (pui32PBEState != IMG_NULL)
	{
		PVR_ASSERT(ui32PBEPackMode != EURASIA_PIXELBE_PACKMODE_NONE);

		/*
		 * set bits in the PBE state: packmode + srcsel (if applicable) srcsel is set to 0
		 * for multi chunk, as we have no idea about the current pass. The "Prepares" have
		 * to set that, though they could cache the rest across passes.
		 */
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)

		pui32PBEState[0] &= EURASIA_PIXELBES0LO_PACKMODE_CLRMSK	&
							EURASIA_PIXELBES0LO_SWIZ_CLRMSK		&
							EURASIA_PIXELBES0LO_PMODEEXT_CLRMSK;

		PVR_ASSERT(0 == (ui32PBEPackMode &
							EURASIA_PIXELBES0LO_PACKMODE_CLRMSK	&
							EURASIA_PIXELBES0LO_SWIZ_CLRMSK		&
							EURASIA_PIXELBES0LO_PMODEEXT_CLRMSK));

		pui32PBEState[0] |= ui32PBEPackMode;

#else /* SGX_FEATURE_UNIFIED_STORE_64BITS */

#if defined(EURASIA_PIXELBE2S0_PACKMODE_CLRMSK)

		ACCUMLATESETNS(pui32PBEState[2], ui32PBEPackMode, EURASIA_PIXELBE2S0_PACKMODE);

#else

		ACCUMLATESETNS(pui32PBEState[SGXTQ_PBESTATE_SBINDEX], ui32PBEPackMode, EURASIA_PIXELBE2SB_PACKMODE);
#endif /* EURASIA_PIXELBE2S0_PACKMODE_CLRMSK */
#endif /* SGX_FEATURE_UNIFIED_STORE_64BITS */

#if defined(EURASIA_PIXELBESB_SRCSEL_CLRMSK)
		ACCUMLATESET(pui32PBEState[SGXTQ_PBESTATE_SBINDEX], ui32SrcSel, EURASIA_PIXELBESB_SRCSEL);
#else
		ACCUMLATESET(pui32PBEState[SGXTQ_PBESTATE_SBINDEX], ui32SrcSel, EURASIA_PIXELBE2SB_SRCSEL);
#endif
	}

	return PVRSRV_OK;
}


IMG_INTERNAL IMG_UINT32 SGXTQ_FilterFromEnum(SGXTQ_FILTERTYPE eFilter)
{
	switch (eFilter)
	{
		case SGXTQ_FILTERTYPE_POINT:
			return EURASIA_PDS_DOUTT0_MAGFILTER_POINT | EURASIA_PDS_DOUTT0_MINFILTER_POINT;
		case SGXTQ_FILTERTYPE_LINEAR:
			return EURASIA_PDS_DOUTT0_MAGFILTER_LINEAR | EURASIA_PDS_DOUTT0_MINFILTER_LINEAR;
		case SGXTQ_FILTERTYPE_ANISOTROPIC:
			return EURASIA_PDS_DOUTT0_MAGFILTER_ANISO | EURASIA_PDS_DOUTT0_MINFILTER_ANISO;
		default:
			PVR_DPF((PVR_DBG_ERROR,"SGXTQ_FilterFromEnum: Unrecognised filter type"));
			PVR_DBG_BREAK;
			return EURASIA_PDS_DOUTT0_MAGFILTER_POINT;
	}
}

IMG_INTERNAL IMG_UINT32 SGXTQ_RotationFromEnum(SGXTQ_ROTATION eRotation)
{
	IMG_UINT32 ui32Rotation;

	switch (eRotation)
	{
		case SGXTQ_ROTATION_NONE:
		{
	#if defined(SGX545)
			ui32Rotation = EURASIA_PIXELBE2S0_ROTATE_0DEG << EURASIA_PIXELBE2S0_ROTATE_SHIFT;
	#else
	#if defined(EURASIA_PIXELBES0HI_ROTATE_SHIFT)
			ui32Rotation =  EURASIA_PIXELBES0HI_ROTATE_0DEG << EURASIA_PIXELBES0HI_ROTATE_SHIFT;
	#else
			ui32Rotation =  EURASIA_PIXELBE2S1_ROTATE_0DEG << EURASIA_PIXELBE2S1_ROTATE_SHIFT;
	#endif
	#endif
			break;
		}
		case SGXTQ_ROTATION_90:
		{
	#if defined(SGX545)
			ui32Rotation =  EURASIA_PIXELBE2S0_ROTATE_90DEG << EURASIA_PIXELBE2S0_ROTATE_SHIFT;
	#else
	#if defined(EURASIA_PIXELBES0HI_ROTATE_SHIFT)
			ui32Rotation =  EURASIA_PIXELBES0HI_ROTATE_90DEG << EURASIA_PIXELBES0HI_ROTATE_SHIFT;
	#else
			ui32Rotation =  EURASIA_PIXELBE2S1_ROTATE_90DEG << EURASIA_PIXELBE2S1_ROTATE_SHIFT;
	#endif
	#endif
			break;
		}
		case SGXTQ_ROTATION_180:
		{
	#if defined(SGX545)
			ui32Rotation =  EURASIA_PIXELBE2S0_ROTATE_180DEG << EURASIA_PIXELBE2S0_ROTATE_SHIFT;
	#else
	#if defined(EURASIA_PIXELBES0HI_ROTATE_SHIFT)
			ui32Rotation =  EURASIA_PIXELBES0HI_ROTATE_180DEG << EURASIA_PIXELBES0HI_ROTATE_SHIFT;
	#else
			ui32Rotation =  EURASIA_PIXELBE2S1_ROTATE_180DEG << EURASIA_PIXELBE2S1_ROTATE_SHIFT;
	#endif
	#endif
			break;
		}
		case SGXTQ_ROTATION_270:
		{
	#if defined(SGX545)
			ui32Rotation =  EURASIA_PIXELBE2S0_ROTATE_270DEG << EURASIA_PIXELBE2S0_ROTATE_SHIFT;
	#else
	#if defined(EURASIA_PIXELBES0HI_ROTATE_SHIFT)
			ui32Rotation =  EURASIA_PIXELBES0HI_ROTATE_270DEG << EURASIA_PIXELBES0HI_ROTATE_SHIFT;
	#else
			ui32Rotation =  EURASIA_PIXELBE2S1_ROTATE_270DEG << EURASIA_PIXELBE2S1_ROTATE_SHIFT;
	#endif
	#endif
			break;
		}
		default:
		{
			PVR_DPF((PVR_DBG_ERROR,"SGXTQ_RotationFromEnum: Unrecognised rotation mode"));
			PVR_DBG_BREAK;
	#if defined(SGX545)
			ui32Rotation =  EURASIA_PIXELBE2S0_ROTATE_0DEG << EURASIA_PIXELBE2S0_ROTATE_SHIFT;
	#else
	#if defined(EURASIA_PIXELBES0HI_ROTATE_SHIFT)
			ui32Rotation =  EURASIA_PIXELBES0HI_ROTATE_0DEG << EURASIA_PIXELBES0HI_ROTATE_SHIFT;
	#else
			ui32Rotation =  EURASIA_PIXELBE2S1_ROTATE_0DEG << EURASIA_PIXELBE2S1_ROTATE_SHIFT;
	#endif
	#endif
		}
	}

	return ui32Rotation;
}

/*****************************************************************************
 * Function Name		:	SGXTQ_MemLayoutFromEnum
 * Inputs				:	eMemLayout	- the mem layout
 *							bIsInput	- is it expected to be an input texture?
 * Returns				:	DOUTT / PBE state dword
 * Description			:	Calculates the PBE memlayout / TAG memlayout dword
******************************************************************************/
IMG_INTERNAL IMG_UINT32 SGXTQ_MemLayoutFromEnum(SGXTQ_MEMLAYOUT eMemLayout, IMG_BOOL bIsInput)
{
	if (bIsInput)
	{
		switch (eMemLayout)
		{
			case SGXTQ_MEMLAYOUT_2D:			
#if defined(SGX_FEATURE_TAG_NPOT_TWIDDLE)
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) 
				return EURASIA_PDS_DOUTT1_TEXTYPE_2D;
#else
				return EURASIA_PDS_DOUTT1_TEXTYPE_ARB_2D;
#endif
#else
				return EURASIA_PDS_DOUTT1_TEXTYPE_2D;
#endif
			case SGXTQ_MEMLAYOUT_3D:
#if defined(SGX_FEATURE_VOLUME_TEXTURES)
#if defined(SGX_FEATURE_TAG_NPOT_TWIDDLE)
				return EURASIA_PDS_DOUTT1_TEXTYPE_ARB_3D;
#else
				return EURASIA_PDS_DOUTT1_TEXTYPE_3D;
#endif
#else
				PVR_DPF((PVR_DBG_ERROR,
							"SGXTQ_MemLayoutFromEnum: 3D textures not supported on this core"));
				PVR_DBG_BREAK;
				/* TODO : propagate error down in the call stack*/
				return EURASIA_PDS_DOUTT1_TEXTYPE_2D;
#endif
#if !defined(SGX_FEATURE_TAG_POT_TWIDDLE) && !defined(SGX_FEATURE_HYBRID_TWIDDLING)
			case SGXTQ_MEMLAYOUT_CEM:
				return EURASIA_PDS_DOUTT1_TEXTYPE_ARB_CEM;
#else
			case SGXTQ_MEMLAYOUT_CEM:
				return EURASIA_PDS_DOUTT1_TEXTYPE_CEM;
#endif
			case SGXTQ_MEMLAYOUT_STRIDE:
#if defined(EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT)
				/* on muse the utilization of the stride extension is indicated by a separate
				 * texture format. On other cores it's usually triggered by a TAG state flag.
				 */
				return EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT;
#else
				return EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE;
#endif
			case SGXTQ_MEMLAYOUT_TILED:
				return EURASIA_PDS_DOUTT1_TEXTYPE_TILED;
			default:
			{
				switch (eMemLayout)
				{
					case SGXTQ_MEMLAYOUT_OUT_LINEAR:
						return SGXTQ_MemLayoutFromEnum(SGXTQ_MEMLAYOUT_STRIDE, bIsInput);	/* PRQA S 3670 */ /* recursive call */
					case SGXTQ_MEMLAYOUT_OUT_TILED:
						return SGXTQ_MemLayoutFromEnum(SGXTQ_MEMLAYOUT_TILED, bIsInput);	/* PRQA S 3670 */ /* recursive call */
					case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
						return SGXTQ_MemLayoutFromEnum(SGXTQ_MEMLAYOUT_2D, bIsInput);		/* PRQA S 3670 */ /* recursive call */
					default:
						PVR_DPF((PVR_DBG_ERROR,
									"SGXTQ_MemLayoutFromEnum: Can't translate %d memory layout into an input layout.",
									(IMG_UINT32)eMemLayout));
						PVR_DBG_BREAK;
						/* TODO : propagate error down in the call stack*/
						return SGXTQ_MemLayoutFromEnum(SGXTQ_MEMLAYOUT_STRIDE, bIsInput);	/* PRQA S 3670 */ /* recursive call */
				}
			}
		}
	}
	else
	{
		switch (eMemLayout)
		{
#if defined(SGX545)
			case SGXTQ_MEMLAYOUT_OUT_LINEAR:
				return EURASIA_PIXELBE2SB_MEMLAYOUT_LINEAR;
			case SGXTQ_MEMLAYOUT_OUT_TILED:
				PVR_DPF((PVR_DBG_ERROR, "SGXTQ_MemLayoutFromEnum: Tile mode not supported by SGX545!"));
				PVR_DBG_BREAK;
				return EURASIA_PIXELBE2SB_MEMLAYOUT_LINEAR;
			case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
				return EURASIA_PIXELBE2SB_MEMLAYOUT_TWIDDLED;

#else
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
			case SGXTQ_MEMLAYOUT_OUT_LINEAR:
				return EURASIA_PIXELBES0LO_MEMLAYOUT_LINEAR;
			case SGXTQ_MEMLAYOUT_OUT_TILED:
				return EURASIA_PIXELBES0LO_MEMLAYOUT_TILED;
			case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
				return EURASIA_PIXELBES0LO_MEMLAYOUT_TWIDDLED;

#else
			case SGXTQ_MEMLAYOUT_OUT_LINEAR:
				return EURASIA_PIXELBE2S0_MEMLAYOUT_LINEAR;
			case SGXTQ_MEMLAYOUT_OUT_TILED:
				return EURASIA_PIXELBE2S0_MEMLAYOUT_TILED;
			case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
				return EURASIA_PIXELBE2S0_MEMLAYOUT_TWIDDLED;

#endif
#endif
			default:
			{
				switch (eMemLayout)
				{
					case SGXTQ_MEMLAYOUT_STRIDE:
						return SGXTQ_MemLayoutFromEnum(SGXTQ_MEMLAYOUT_OUT_LINEAR, bIsInput);	/* PRQA S 3670 */ /* recursive call */
					case SGXTQ_MEMLAYOUT_2D:
						return SGXTQ_MemLayoutFromEnum(SGXTQ_MEMLAYOUT_OUT_TWIDDLED, bIsInput);	/* PRQA S 3670 */ /* recursive call */
					case SGXTQ_MEMLAYOUT_TILED:
						return SGXTQ_MemLayoutFromEnum(SGXTQ_MEMLAYOUT_OUT_TILED, bIsInput);	/* PRQA S 3670 */ /* recursive call */
					default:
						PVR_DPF((PVR_DBG_ERROR,
									"SGXTQ_MemLayoutFromEnum: Can't translate %d memory layout into an output layout.",
									(IMG_UINT32)eMemLayout));
						PVR_DBG_BREAK;
						/* TODO : propagate error down in the call stack*/
						return SGXTQ_MemLayoutFromEnum(SGXTQ_MEMLAYOUT_OUT_LINEAR, bIsInput);	/* PRQA S 3670 */ /* recursive call */
				}
			}
		}
	}
	/* This should be unreachable*/
}


/*****************************************************************************
 * Function Name		:	SGXTQ_ShaderFromAlpha
 * Inputs				:	eAlpha		- alpha blending selector
 * Outputs				:	peUSEProg	- selected shader
 *						:	pePDSSec	- required secondary for selected shader
 * Returns				:	Shader enum that does eAlpha type blending
 * Description			:	if eAlpha is SGXTQ_ALPHA_NONE then peUSEProg & pePDSSec
 *							remain untuched.
******************************************************************************/
IMG_INTERNAL IMG_VOID SGXTQ_ShaderFromAlpha(SGXTQ_ALPHA eAlpha,
											SGXTQ_USEFRAGS* peUSEProg,
											SGXTQ_PDSSECFRAGS* pePDSSec)
{
	switch (eAlpha)
	{
		case SGXTQ_ALPHA_SOURCE:
		{
			*peUSEProg = SGXTQ_USEBLIT_SRC_BLEND;
			*pePDSSec = SGXTQ_PDSSECFRAG_BASIC;
			break;
		}
		case SGXTQ_ALPHA_PREMUL_SOURCE:
		{
			*peUSEProg = SGXTQ_USEBLIT_PREMULSRC_BLEND;
			*pePDSSec = SGXTQ_PDSSECFRAG_BASIC;
			break;
		}
		case SGXTQ_ALPHA_GLOBAL:
		{
			*peUSEProg = SGXTQ_USEBLIT_GLOBAL_BLEND;
			*pePDSSec = SGXTQ_PDSSECFRAG_1ATTR;
			break;
		}
		case SGXTQ_ALPHA_PREMUL_SOURCE_WITH_GLOBAL:
		{
			*peUSEProg = SGXTQ_USEBLIT_PREMULSRCWITHGLOBAL_BLEND;
			*pePDSSec = SGXTQ_PDSSECFRAG_1ATTR;
			break;
		}
		case SGXTQ_ALPHA_NONE:
			break;
	}
}

/*****************************************************************************
 * Function Name		:	SGXTQ_ShaderFromRop
 * Inputs				:	byCustomRop3	- rop selector
 * Outputs				:	peUSEProg		- selected shader
 *							pePDSSec		-
 *						:	pui32NumLayers	- number of input texture layers
 * Returns				:	Error for invalid ROP.
******************************************************************************/
IMG_INTERNAL PVRSRV_ERROR SGXTQ_ShaderFromRop(IMG_BYTE byCustomRop3,
										SGXTQ_USEFRAGS* peUSEProg,
										SGXTQ_PDSPRIMFRAGS* pePDSPrim,
										IMG_UINT32* pui32NumLayers)
{
	*pui32NumLayers = 2;
	*pePDSPrim = SGXTQ_PDSPRIMFRAG_TWOSOURCE;
	switch (byCustomRop3)
	{
		case 0x11:
			*peUSEProg = SGXTQ_USEBLIT_ROP_NOTSANDNOTD;
			break;
		case 0x22:
			*peUSEProg = SGXTQ_USEBLIT_ROP_NOTSANDD;
			break;
		case 0x33:
			*pui32NumLayers = 1;
			*pePDSPrim = SGXTQ_PDSPRIMFRAG_SINGLESOURCE;
			*peUSEProg = SGXTQ_USEBLIT_ROP_NOTS;
			break;
		case 0x44:
			*peUSEProg = SGXTQ_USEBLIT_ROP_SANDNOTD;
			break;
		case 0x55:
			*peUSEProg = SGXTQ_USEBLIT_ROP_NOTD;
			break;
		case 0x66:
			*peUSEProg = SGXTQ_USEBLIT_ROP_SXORD;
			break;
		case 0x77:
			*peUSEProg = SGXTQ_USEBLIT_ROP_NOTSORNOTD;
			break;
		case 0x88:
			*peUSEProg = SGXTQ_USEBLIT_ROP_SANDD;
			break;
		case 0x99:
			*peUSEProg = SGXTQ_USEBLIT_ROP_NOTSXORD;
			break;
		case 0xaa:
			*peUSEProg = SGXTQ_USEBLIT_ROP_D;
			break;
		case 0xbb:
			*peUSEProg = SGXTQ_USEBLIT_ROP_NOTSORD;
			break;
		case 0xcc:
			*pui32NumLayers = 1;
			*pePDSPrim = SGXTQ_PDSPRIMFRAG_SINGLESOURCE;
			*peUSEProg = SGXTQ_USEBLIT_ROP_S;
			break;
		case 0xdd:
			*peUSEProg = SGXTQ_USEBLIT_ROP_SORNOTD;
			break;
		case 0xee:
			*peUSEProg = SGXTQ_USEBLIT_ROP_SORD;
			break;
		default:
			return PVRSRV_ERROR_INVALID_PARAMS;
	}
	return PVRSRV_OK;
}

/*****************************************************************************
 * Function Name		:	SGXTQ_WriteUSEMovToReg
 * Inputs				:	pui32Address- LinAddr for the instruction
 * 							ui32OutputReg - USE output register no.
 * 							ui32value	- Value for the MOV
 * 							ui32Dst1BankSelect - Destination bank select
 * 							ui32End - encoding for .end or 0 otherwise
 * Description			:	Writes the `mov.skipinv oX, #value' USE intruction
******************************************************************************/
INLINE static IMG_VOID SGXTQ_WriteUSEMovToReg(IMG_PUINT32	pui32LinAddr,
										IMG_UINT32			ui32Reg,
										IMG_UINT32			ui32Value,
										IMG_UINT32			ui32Dst1BankSelect,
										IMG_UINT32			ui32End)
{
	/* Check reg is within range */
	CHECKOVERFLOW(ui32Reg, EURASIA_USE0_DST_CLRMSK, EURASIA_USE0_DST_SHIFT);

	*pui32LinAddr++ = (ui32Reg << EURASIA_USE0_DST_SHIFT)					|
					  (((ui32Value >> 0) << EURASIA_USE0_LIMM_IMML21_SHIFT)	&
					   (~EURASIA_USE0_LIMM_IMML21_CLRMSK));

	*pui32LinAddr = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT)						|
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
					(EURASIA_USE1_OTHER_OP2_LIMM << EURASIA_USE1_OTHER_OP2_SHIFT)			|
					(EURASIA_USE1_EPRED_ALWAYS << EURASIA_USE1_LIMM_EPRED_SHIFT)			|
					(ui32Dst1BankSelect << EURASIA_USE1_D1BANK_SHIFT)						|
					(((ui32Value >> 21) << EURASIA_USE1_LIMM_IMM2521_SHIFT)					&
					 (~EURASIA_USE1_LIMM_IMM2521_CLRMSK))									|
					(((ui32Value >> 26) << EURASIA_USE1_LIMM_IMM3126_SHIFT)					&
					 (~EURASIA_USE1_LIMM_IMM3126_CLRMSK))									|
					EURASIA_USE1_SKIPINV													|
					ui32End;
}


/*****************************************************************************
 * Function Name		:	SGXTQ_GetStrideGran
 * Inputs				:	ui32LineStride	- The stride in pixels
						:	ui32BytesPerPixel - the bytes per pixel
 * Returns				:	the stride granuality
******************************************************************************/
IMG_INTERNAL IMG_UINT32 SGXTQ_GetStrideGran(IMG_UINT32 ui32LineStride, IMG_UINT32 ui32BytesPerPixel)
{
	IMG_UINT32 ui32StrideGran;

#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
	PVR_ASSERT(ui32BytesPerPixel != 0);
	ui32StrideGran = (ui32BytesPerPixel < 4 ? 4 / ui32BytesPerPixel : 1); 
	PVR_UNREFERENCED_PARAMETER(ui32LineStride);
#else
#if EURASIA_TAG_STRIDE_THRESHOLD
	ui32StrideGran = (ui32LineStride < EURASIA_TAG_STRIDE_THRESHOLD
									? EURASIA_TAG_STRIDE_ALIGN0
									: EURASIA_TAG_STRIDE_ALIGN1);
#else
	PVR_UNREFERENCED_PARAMETER(ui32LineStride);
	ui32StrideGran = EURASIA_TAG_STRIDE_ALIGN1;
#endif
#endif

#if defined(FIX_HW_BRN_26518) || defined(FIX_HW_BRN_27408)
	PVR_ASSERT(ui32BytesPerPixel != 0);
	ui32StrideGran = (((ui32StrideGran * ui32BytesPerPixel) + 15U) & ~ 15U) / ui32BytesPerPixel;
#else
	PVR_UNREFERENCED_PARAMETER(ui32BytesPerPixel);
#endif

	return ui32StrideGran;
}


/*****************************************************************************
 * Function Name		:	SGXTQ_GetSurfaceStride
 * Inputs				:	psSurf			- The surface
 *							ui32BytesPP		- Bytes per pixel on the surface
 *							bIsInput		- HW is less flexible on input textures,
 *											  and workaround code treats them differently
 *							bStridedBlitEnabled		- does the calling code support the
 *											  workaround in the shader
 * Outputs				:	the stride in pixels
 * Returns				:	Error for invalid surfaces
 * Description			:	Extracts the stride from the surface structure.
 *							Stride of twiddled textures is same as the width.
 *
 *							We have 3 implementations dealing with texture strides.
 *							1. If the hw has the TEXTURESTRIDE_EXTENSION we can give
 *							   a dword aligned stride to the TAG 
 *							2/a. If we don't have the extension, the stride is bound to a
 *							   large granuality otherwise we fail on the texture. The width
 *							   is set to the same as the stride.
 *							2/b. If we don't have the extension, we round up the stride, set
 *							   the width to the same value, and do USE_ISSUEs on the iterators
 *							   instead of TAG_ISSUE.
 *							   With the texel coordinates the shader can sample in the pixels
 *							   from the correct addresses.
******************************************************************************/
IMG_INTERNAL PVRSRV_ERROR SGXTQ_GetSurfaceStride(SGXTQ_SURFACE* psSurf,
												IMG_UINT32		ui32BytesPP,
												IMG_BOOL		bIsInput,
												IMG_BOOL		bStridedBlitEnabled,
												IMG_UINT32*		pui32LineStride)
{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION) && ! defined(FIX_HW_BRN_26518) && ! defined(FIX_HW_BRN_27408)
	PVR_UNREFERENCED_PARAMETER(bStridedBlitEnabled);
#endif

	switch (psSurf->eMemLayout)
	{
		case SGXTQ_MEMLAYOUT_2D:
		case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
		{
			*pui32LineStride = psSurf->ui32Width;

#if defined(SGX_FEATURE_HYBRID_TWIDDLING)
			{
				IMG_UINT32 ui32TileSize; /* hybrid twiddling tile*/
				
				if ((! bIsInput) || psSurf->ui32Width > 8 || psSurf->ui32Height > 8)
				{
					ui32TileSize = 16;
				}
				else
				{
					ui32TileSize = MIN(psSurf->ui32Width, psSurf->ui32Height);
					if ((ui32TileSize & (ui32TileSize - 1)) != 0)
					{
						ui32TileSize = 1 << (SGXTQ_FindNearestLog2(ui32TileSize) + 1); 
					}
				}
				if ((psSurf->ui32Width % ui32TileSize != 0)
				|| (psSurf->ui32Height % ui32TileSize != 0))
				{
					PVR_DPF((PVR_DBG_WARNING, "hybrid twiddled texture dimension is not multiple of the twiddled tile"));
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
			}
#else
			/* is it power of 2?*/
			if (((psSurf->ui32Width - 1) & psSurf->ui32Width) != 0UL ||
				((psSurf->ui32Height - 1) & psSurf->ui32Height) != 0UL)
			{
				PVR_DPF((PVR_DBG_WARNING, "twiddled texture with non power of 2 dimensions"));
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
#endif
			break;
		}
		case SGXTQ_MEMLAYOUT_TILED:
		case SGXTQ_MEMLAYOUT_OUT_TILED:
		{
			/* Tiled memory layout is by definition a board of full 32 x 32 pix
			 * boxes. This has nothing to do with the HW tile size. The width and height
			 * must be 32 pixel aligned. (thats the step when the PBE calculates the tile
			 * address from the tile X/Y) presuming not clipped boxes.
			 */
			if (((psSurf->ui32Width & (EURASIA_TAG_TILE_SIZEX - 1)) != 0) ||
				((psSurf->ui32Height & (EURASIA_TAG_TILE_SIZEY - 1)) != 0))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
			/* don't break here*/
		}
		case SGXTQ_MEMLAYOUT_STRIDE:
		case SGXTQ_MEMLAYOUT_OUT_LINEAR:
		{

			IMG_UINT32 ui32UnsignedStride;

			if (ui32BytesPP == 0)
				return PVRSRV_ERROR_INVALID_PARAMS;

			if (psSurf->i32StrideInBytes < 0)
				ui32UnsignedStride = (IMG_UINT32)(0 - psSurf->i32StrideInBytes);
			else
				ui32UnsignedStride = (IMG_UINT32)psSurf->i32StrideInBytes;

			*pui32LineStride = ui32UnsignedStride / ui32BytesPP;

			if (bIsInput)
			{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
#if defined(SGX545)
				if ((ui32UnsignedStride >> EURASIA_PDS_DOUTT0_STRIDE_ALIGNSHIFT) > (1 << 13))
#else
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
				if ((ui32UnsignedStride >> EURASIA_PDS_DOUTT0_STRIDE_ALIGNSHIFT) > (1 << 15))
#else
#if defined(SGX520) || defined(SGX530)
				if ((ui32UnsignedStride >> EURASIA_PDS_DOUTT0_STRIDE_ALIGNSHIFT) > (1 << 11))
#else
				if ((ui32UnsignedStride >> EURASIA_PDS_DOUTT0_STRIDE_ALIGNSHIFT) > (1 << 12))
#endif /* 520 - 530 */
#endif /* SGX_FEATURE_TEXTURE_32K_STRIDE*/
#endif /* SGX545*/
				{
					return PVRSRV_ERROR_NOT_SUPPORTED;
				}
#endif /* SGX_FEATURE_TEXTURESTRIDE_EXTENSION*/

#if defined(EURASIA_TAG_MAX_PIXEL_STRIDE)
				/* even if we have the bits in dwords on the TAG interface the TAG is limited in pixels as well.*/
				if (*pui32LineStride > EURASIA_TAG_MAX_PIXEL_STRIDE)
				{
					return PVRSRV_ERROR_NOT_SUPPORTED;
				}
#endif /* EURASIA_TAG_MAX_PIXEL_STRIDE*/

#if defined(FIX_HW_BRN_29967)
				if (*pui32LineStride >= (1 << 16))
				{
					return PVRSRV_ERROR_NOT_SUPPORTED;
				}
#endif

				if (! bStridedBlitEnabled)
				{
					IMG_UINT32 ui32StrideGran = SGXTQ_GetStrideGran(*pui32LineStride, ui32BytesPP);

					if (*pui32LineStride % ui32StrideGran != 0)
					{
						return PVRSRV_ERROR_INVALID_PARAMS;
					}
				}
			}
			else
			{
				/* This is 2pix alignment on most cores*/
				if (! ISALIGNED(*pui32LineStride, EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT))
				{
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
#if defined(FIX_HW_BRN_30842)
				if (* pui32LineStride < EURASIA_ISPREGION_SIZEX)
				{
					return PVRSRV_ERROR_INVALID_PARAMS;
				}
#endif
			}
			break;
		}
		default:
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}
	return PVRSRV_OK;
}


/*****************************************************************************
 * Function Name		:	SGXTQ_GetSurfaceWidth
 * Inputs				:	psSurf			- The surface
 *							ui32BytesPP		- Bytes per pixel on the surface
 *							bStridedBlitEnabled		- does the calling code support the
 *											  workaround in the shader
 * Outputs				:	pui32RightEdge	- the width in pixels
 * Returns				:	Error for invalid surfaces
 * Description			:	Gives back the x pixel coordinates of the rightmost
 *							column on the surface. (Where U is 1.0)
 *							This can be used as the denominator to get U texel coordinates.
******************************************************************************/
IMG_INTERNAL PVRSRV_ERROR SGXTQ_GetSurfaceWidth(SGXTQ_SURFACE*	psSurf,
												IMG_UINT32		ui32BytesPP,
												IMG_BOOL		bIsInput,
												IMG_BOOL		bStridedBlitEnabled,
												IMG_UINT32* 	pui32RightEdge)
{
	if (psSurf->eMemLayout == SGXTQ_MEMLAYOUT_STRIDE
			|| psSurf->eMemLayout == SGXTQ_MEMLAYOUT_OUT_LINEAR
			|| psSurf->eMemLayout == SGXTQ_MEMLAYOUT_TILED
			|| psSurf->eMemLayout == SGXTQ_MEMLAYOUT_OUT_TILED)
	{
		if (bIsInput)
		{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
			IMG_UINT32 ui32LineStride;

			*pui32RightEdge = psSurf->ui32Width;
			if (PVRSRV_OK != SGXTQ_GetSurfaceStride(psSurf,
						ui32BytesPP,
						bIsInput,
						bStridedBlitEnabled,
						&ui32LineStride))
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}

			if (*pui32RightEdge > ui32LineStride)
			{
				return PVRSRV_ERROR_INVALID_PARAMS;
			}
#else
			/* if we don't have the stride extension, we still can have independent stride,
			 * by giving the stride to the TAG instead of the width, and use the same value for
			 * TSP coordinate calculations.
			 */
			return SGXTQ_GetSurfaceStride(psSurf,
					ui32BytesPP,
					bIsInput,
					bStridedBlitEnabled,
					pui32RightEdge);

#endif /* SGX_FEATURE_TEXTURESTRIDE_EXTENSION*/
		}
		else
		{
			/* For the PBE the width doesn't matter for strided textures*/
			*pui32RightEdge = psSurf->ui32Width;
		}
	}
	else
	{
		return SGXTQ_GetSurfaceStride(psSurf,
				ui32BytesPP,
				bIsInput,
				bStridedBlitEnabled,
				pui32RightEdge);
	}
	return PVRSRV_OK;
}


/*****************************************************************************
 * Function Name		:	SGXTQ_SetTAGState
 * Inputs				:	ui32LayerNo		- texture layer no
 *							eFilter
 *							ui32Width		- Input texture width (in pixels)
 *											  In normal case this should be obtained by calling
 *											  SGXTQ_GetSurfaceWidth
 *							ui32Height		- Input texture height (in pixels)
 *							ui32Stride		- Input texture stride (in pixels)
 *											  In normal case this should be obtained by calling
 *											  SGXTQ_GetSurfaceStride
 *							ui32TAGFormat
 *							ui32BytesPP		- Bytes per pixel on the surface
 *							eMemLayout		- Input memory layout
 * Outputs				:	psPDSUpdate		- The TAG state ready for a DOUTT
 * Description			:	Accumlate sets the TAG state into the PDS DOUTT dwords
 *
 * TODO					:	REMOVE PIXEL FORMAT RELATED STUFF IT COMES FROM SGXTQ_GetPixelFormats()
******************************************************************************/
IMG_INTERNAL IMG_VOID SGXTQ_SetTAGState(SGXTQ_PDS_UPDATE* psPDSUpdate,
										IMG_UINT32 ui32LayerNo,
										IMG_UINT32 ui32SrcDevVAddr,
										SGXTQ_FILTERTYPE eFilter,
										IMG_UINT32 ui32Width,
										IMG_UINT32 ui32Height,
										IMG_UINT32 ui32Stride,
										IMG_UINT32 ui32TAGFormat,
										IMG_UINT32 ui32BytesPP,
										IMG_BOOL bNewPixelHandling,
										SGXTQ_MEMLAYOUT eMemLayout)
{
	IMG_UINT32	*pui32TAGState;

	PVR_ASSERT(ui32LayerNo < (SGXTQ_NUM_HWBGOBJS - 1));

	/* PRQA S 3689 1 */ /* PVR_ASSERT() should catch bounds overflow */
	pui32TAGState = & psPDSUpdate->asLayers[ui32LayerNo].aui32TAGState[0];

	pui32TAGState[0] &= EURASIA_PDS_DOUTT0_DADJUST_CLRMSK	&
		EURASIA_PDS_DOUTT0_ANISOCTL_CLRMSK					&
		EURASIA_PDS_DOUTT0_MINFILTER_CLRMSK					&
		EURASIA_PDS_DOUTT0_MAGFILTER_CLRMSK					&
		EURASIA_PDS_DOUTT0_UADDRMODE_CLRMSK					&
		EURASIA_PDS_DOUTT0_VADDRMODE_CLRMSK					&
#if defined(SGX_FEATURE_VOLUME_TEXTURES)
		EURASIA_PDS_DOUTT0_SADDRMODE_CLRMSK					&
#endif
		EURASIA_PDS_DOUTT0_MIPMAPCLAMP_CLRMSK;

	pui32TAGState[0] |=
		0UL << EURASIA_PDS_DOUTT0_DADJUST_SHIFT	|
		EURASIA_PDS_DOUTT0_ANISOCTL_NONE		|
		SGXTQ_FilterFromEnum(eFilter);

#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
	if (eMemLayout != SGXTQ_MEMLAYOUT_STRIDE &&
		eMemLayout != SGXTQ_MEMLAYOUT_OUT_LINEAR)
#endif
	{
		pui32TAGState[0] |=
			EURASIA_PDS_DOUTT0_UADDRMODE_CLAMP	|
			EURASIA_PDS_DOUTT0_VADDRMODE_CLAMP	|
			EURASIA_PDS_DOUTT0_NOTMIPMAP;
	}

#if defined(SGX_FEATURE_VOLUME_TEXTURES)
	if (eMemLayout == SGXTQ_MEMLAYOUT_3D)
		pui32TAGState[0] |= EURASIA_PDS_DOUTT0_SADDRMODE_CLAMP;
#endif

	if (! bNewPixelHandling)
	{
		ACCUMLATESETNS(pui32TAGState[1], ui32TAGFormat, EURASIA_PDS_DOUTT1_TEXFORMAT);
	}

	pui32TAGState[1] &= EURASIA_PDS_DOUTT1_TEXTYPE_CLRMSK;
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)			&& \
	! defined(EURASIA_PDS_DOUTT1_TEXTYPE_NP2_STRIDE_EXT)	&& \
	! defined(SGX_FEATURE_VOLUME_TEXTURES)
	/* the above condition isn't exactly the right one but the hw
	 * interface is a bit inconsitent here anyway, if the extension is on
	 * some cores require duplicating the memlayout some don't*/
	if (eMemLayout != SGXTQ_MEMLAYOUT_STRIDE
		&& eMemLayout != SGXTQ_MEMLAYOUT_OUT_LINEAR)
	{
		pui32TAGState[1] |= SGXTQ_MemLayoutFromEnum(eMemLayout, IMG_TRUE);
	}
#else
	pui32TAGState[1] |= SGXTQ_MemLayoutFromEnum(eMemLayout, IMG_TRUE);
#endif


#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
	if (eMemLayout == SGXTQ_MEMLAYOUT_2D
		|| eMemLayout == SGXTQ_MEMLAYOUT_OUT_TWIDDLED)
	{
		/*
		 * Find number of tiles
		 */
		IMG_UINT32 ui32U = EURASIA_PDS_DOUTT_TEXSIZE1;
		IMG_UINT32 ui32V = EURASIA_PDS_DOUTT_TEXSIZE1;

		ui32U += SGXTQ_FindNearestLog2(ui32Width);
		ui32V += SGXTQ_FindNearestLog2(ui32Height);

		ACCUMLATESET(pui32TAGState[1], ui32V, EURASIA_PDS_DOUTT1_VSIZE);
		ACCUMLATESET(pui32TAGState[1], ui32U, EURASIA_PDS_DOUTT1_USIZE);
	}
	else
#endif
	{
#if defined(SGX_FEATURE_TEXTURESTRIDE_EXTENSION)
		if (eMemLayout == SGXTQ_MEMLAYOUT_STRIDE
			|| eMemLayout == SGXTQ_MEMLAYOUT_OUT_LINEAR)
		{
			/* we have a stride in pixels, convert it to dwords*/
			ui32Stride = (ui32Stride * ui32BytesPP) >> EURASIA_PDS_DOUTT0_STRIDE_ALIGNSHIFT;

			/* otherwise underflow*/
			PVR_ASSERT(ui32Stride != 0);
			ui32Stride--;	/* PRQA S 3382 */ /* relies on PVR_ASSERT */

			/* QAC override of truncation warning */
			/* PRQA S 3381,277 END_TRUNCATE */

#if defined(SGX543) || defined(SGX544) || defined(SGX554)
			pui32TAGState[0] &= EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK	&
				EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK					&
				EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK;

			pui32TAGState[0] |=
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEEX_STRIDE_SHIFT)					<<
				  EURASIA_PDS_DOUTT0_STRIDEEX_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEEX_CLRMSK) |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT)					<<
				  EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK) |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO_STRIDE_SHIFT)					<<
				  EURASIA_PDS_DOUTT0_STRIDELO_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK);
#else /* 543 || 544 || 554 */
#if defined(SGX545)
			pui32TAGState[0] &= EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK	&
				EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK;

			pui32TAGState[0] |=
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT)					<<
				  EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK) |
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO_STRIDE_SHIFT)					<<
				  EURASIA_PDS_DOUTT0_STRIDELO_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK);
#else /* 545 */
			pui32TAGState[0] &=
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
				EURASIA_PDS_DOUTT0_STRIDEEX0_CLRMSK					&
#endif
#if defined(SGX_FEATURE_VOLUME_TEXTURES)
				EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK					&
#else
				EURASIA_PDS_DOUTT0_STRIDELO2_CLRMSK					&
#endif
				EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK;

			pui32TAGState[0] |=
#if defined(EURASIA_PDS_DOUTT0_STRIDE)
				EURASIA_PDS_DOUTT0_STRIDE														|
#endif
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEEX0_STRIDE_SHIFT)						<<
				  EURASIA_PDS_DOUTT0_STRIDEEX0_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEEX0_CLRMSK)	|
#endif
#if defined(SGX_FEATURE_VOLUME_TEXTURES)
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO_STRIDE_SHIFT)						<<
				  EURASIA_PDS_DOUTT0_STRIDELO_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO_CLRMSK)		|
#else
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDELO2_STRIDE_SHIFT)						<<
				  EURASIA_PDS_DOUTT0_STRIDELO2_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDELO2_CLRMSK)	|
#endif
				(((ui32Stride >> EURASIA_PDS_DOUTT0_STRIDEHI_STRIDE_SHIFT)						<<
				  EURASIA_PDS_DOUTT0_STRIDEHI_SHIFT) & ~EURASIA_PDS_DOUTT0_STRIDEHI_CLRMSK);

#if defined(FIX_HW_BRN_28922)
			pui32TAGState[0] |= 1 << EURASIA_PDS_DOUTT0_MIPMAPCLAMP_SHIFT;
#endif

			pui32TAGState[1] &=
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
				EURASIA_PDS_DOUTT1_STRIDEEX1_CLRMSK	&
#endif
#if ! defined(SGX_FEATURE_VOLUME_TEXTURES)
				EURASIA_PDS_DOUTT1_STRIDELO1_CLRMSK	&
#endif
				0xffffffff;

			pui32TAGState[1] |=
#if defined(SGX_FEATURE_TEXTURE_32K_STRIDE)
				(((ui32Stride >> EURASIA_PDS_DOUTT1_STRIDEEX1_STRIDE_SHIFT)						<<
				  EURASIA_PDS_DOUTT1_STRIDEEX1_SHIFT) & ~EURASIA_PDS_DOUTT1_STRIDEEX1_CLRMSK)	|
#endif
#if !defined(SGX_FEATURE_VOLUME_TEXTURES)
				(((ui32Stride >> EURASIA_PDS_DOUTT1_STRIDELO1_STRIDE_SHIFT)						<<
				  EURASIA_PDS_DOUTT1_STRIDELO1_SHIFT) & ~EURASIA_PDS_DOUTT1_STRIDELO1_CLRMSK)	|
#endif
				0UL;
#endif /* SGX545 */
#endif /* SGX543 || SGX544 || SGX554 */
		}
#else /* SGX_FEATURE_TEXTURESTRIDE_EXTENSION*/

		/* QAC override end */
		/* PRQA L:END_TRUNCATE */

		PVR_UNREFERENCED_PARAMETER(ui32BytesPP);

		/* if we don't have the stride extension, we still can have independent stride,
		 * by passing down the stride, and than stop at the width with the U.
		 */
		ui32Width = ui32Stride;

#endif /* SGX_FEATURE_TEXTURESTRIDE_EXTENSION*/

		ACCUMLATESET(pui32TAGState[1], ui32Width - 1, EURASIA_PDS_DOUTT1_WIDTH);
		ACCUMLATESET(pui32TAGState[1], ui32Height - 1, EURASIA_PDS_DOUTT1_HEIGHT);
	}

	CHECKBITSNOTLOST(ui32SrcDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT);
	ui32SrcDevVAddr >>= EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT;
	ACCUMLATESET(pui32TAGState[2], ui32SrcDevVAddr, EURASIA_PDS_DOUTT2_TEXADDR);

#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
#if defined(SGX_FEATURE_PDS_EXTENDED_SOURCES)
	/* FIXME: needs swizzle */
	/* will be moved to SGXTQ_GetPixelFormats() */
#else
	pui32TAGState[3] |=	EURASIA_PDS_DOUTT3_CMPMODE_GREATEREQUAL |
		(1UL << EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT);
#endif
#endif
}

/*****************************************************************************
 * Function Name		:	SGXTQ_SetUSSEKick
 * Inputs				:	sUSEExecAddr
 *							sUSEExecBase
 * 							ui32NumTempsRegs
 * Outputs				:	psPDSUpdate		- The PDS values ready for a DOUTU
 *
 * Description			:	Calculates the PDS DOUTU dwords
 * Note					:	It doesn't set any dependencies.
******************************************************************************/
IMG_INTERNAL IMG_VOID SGXTQ_SetUSSEKick(SGXTQ_PDS_UPDATE	*psPDSUpdate,
										IMG_DEV_VIRTADDR	sUSEExecAddr,
										IMG_DEV_VIRTADDR	sUSEExecBase,
										IMG_UINT32			ui32NumTempRegs)
{
	psPDSUpdate->ui32U0 = 0;

	SetUSEExecutionAddress(&psPDSUpdate->ui32U0, 0, sUSEExecAddr, sUSEExecBase, SGXTQ_USE_CODE_BASE_INDEX);

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	psPDSUpdate->ui32TempRegs = ui32NumTempRegs;
#endif

	ui32NumTempRegs = ALIGNFIELD(ui32NumTempRegs, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);

#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	CHECKOVERFLOW(ui32NumTempRegs, EURASIA_PDS_DOUTU0_TRC_CLRMSK, EURASIA_PDS_DOUTU0_TRC_SHIFT);
	psPDSUpdate->ui32U0 |= (ui32NumTempRegs << EURASIA_PDS_DOUTU0_TRC_SHIFT) & ~EURASIA_PDS_DOUTU0_TRC_CLRMSK;

	psPDSUpdate->ui32U1 = EURASIA_PDS_DOUTU1_SDSOFT;

	psPDSUpdate->ui32U2 = 0;
#else
	CHECKOVERFLOW(ui32NumTempRegs, EURASIA_PDS_DOUTU1_TRC_CLRMSK, EURASIA_PDS_DOUTU1_TRC_SHIFT);
	psPDSUpdate->ui32U1 =	EURASIA_PDS_DOUTU1_PUNCHTHROUGH_DISABLED |
							((ui32NumTempRegs << EURASIA_PDS_DOUTU1_TRC_SHIFT) & ~EURASIA_PDS_DOUTU1_TRC_CLRMSK);

	psPDSUpdate->ui32U2 = EURASIA_PDS_DOUTU2_SDSOFT;
#endif
}


/*****************************************************************************
 * Function Name		:	SGXTQ_SetDMAState
 * Inputs				:	sDevVaddr
 *							sLineLen
 *							sLineNo
 * Outputs				:	psPDSUpdate		- The PDS values ready for a DOUTD
 * Description			:	Calculates the PDS DOUTD dwords
******************************************************************************/
IMG_INTERNAL IMG_VOID SGXTQ_SetDMAState(SGXTQ_PDS_UPDATE	*psPDSUpdate,
										IMG_DEV_VIRTADDR	sDevVaddr,
										IMG_UINT32			ui32LineLen,
										IMG_UINT32			ui32LineNo,
										IMG_UINT32			ui32Offset)
{
	IMG_UINT32 ui32NumDWords = ui32LineNo * ui32LineLen;

	CHECKOVERFLOW(sDevVaddr.uiAddr, EURASIA_PDS_DOUTD0_SBASE_CLRMSK, EURASIA_PDS_DOUTD0_SBASE_SHIFT);
	psPDSUpdate->ui32D0 = sDevVaddr.uiAddr << EURASIA_PDS_DOUTD0_SBASE_SHIFT;

#if (EURASIA_PDS_DOUTD1_MAXBURST == EURASIA_PDS_DOUTD1_BSIZE_MAX)
	CHECKOVERFLOW(ui32NumDWords - 1, EURASIA_PDS_DOUTD1_BSIZE_CLRMSK, EURASIA_PDS_DOUTD1_BSIZE_SHIFT);
#else
	CHECKOVERFLOW(ui32LineLen - 1, EURASIA_PDS_DOUTD1_BSIZE_CLRMSK, EURASIA_PDS_DOUTD1_BSIZE_SHIFT);
	CHECKOVERFLOW(ui32LineLen - 1, EURASIA_PDS_DOUTD1_STRIDE_CLRMSK, EURASIA_PDS_DOUTD1_STRIDE_SHIFT);
	CHECKOVERFLOW(ui32LineNo - 1, EURASIA_PDS_DOUTD1_BLINES_CLRMSK, EURASIA_PDS_DOUTD1_BLINES_SHIFT);
#endif
	CHECKOVERFLOW(ui32Offset, EURASIA_PDS_DOUTD1_AO_CLRMSK, EURASIA_PDS_DOUTD1_AO_SHIFT);
	psPDSUpdate->ui32D1 = ((ui32Offset << EURASIA_PDS_DOUTD1_AO_SHIFT) & ~EURASIA_PDS_DOUTD1_AO_CLRMSK)					|
#if (EURASIA_PDS_DOUTD1_MAXBURST == EURASIA_PDS_DOUTD1_BSIZE_MAX)
						  (((ui32NumDWords - 1U) << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) & ~EURASIA_PDS_DOUTD1_BSIZE_CLRMSK);
#else
						  (((ui32LineLen - 1U) << EURASIA_PDS_DOUTD1_BSIZE_SHIFT) & ~EURASIA_PDS_DOUTD1_BSIZE_CLRMSK)	|
						  (((ui32LineNo - 1U) << EURASIA_PDS_DOUTD1_BLINES_SHIFT) & ~EURASIA_PDS_DOUTD1_BLINES_CLRMSK)	|
						  (((ui32LineLen - 1U) << EURASIA_PDS_DOUTD1_STRIDE_SHIFT) & ~EURASIA_PDS_DOUTD1_STRIDE_CLRMSK);
#endif

	psPDSUpdate->ui32DMASize = ui32NumDWords;
}


#if ! defined(SGX_FEATURE_USE_VEC34)
/*****************************************************************************
 * Function Name		:	SGXTQ_SetupStridedBlit
 * Inputs				: 	psSrcSurf		- the input surface.
 *							eUSEProg		- the shader (must be a *STRIDE* type of TQ shaders)
 *							ui32SrcLineStride
 *							ui32SrcBytesPerPixel
 *							ui32PassesRequired - how many passes do we have?
 *							ui32CurPass		- which Pass is this?
 * Outputs				:	pui32TAGState	- clears and sets the appropriate fields in the pointed TAG state
 *							aui32Limms		- the load immediates for the shader
 * Returns				:	eError			- not PVRSRV_OK if the strided blit cannot be done
 *
 * Description			:	Patches the appropriate TQ USE shader pointed by eUSEProg. Also overwrites
 *							the width and height in the pointed TAG state.
 *
 * Note					:	This MUST be called after a full TAG setup (calling SGXTQ_SetTAGState), but
 *							before patching the PDS primaries.
 *
 * TODO					:	On subsequent passes we could simply skip most of the setup if we had space
 *							for storing it in the PassDetails.
******************************************************************************/
IMG_INTERNAL PVRSRV_ERROR SGXTQ_SetupStridedBlit(IMG_UINT32					* pui32TAGState,
											SGXTQ_SURFACE					* psSrcSurf,
											SGXTQ_USEFRAGS					eUSEProg,
											IMG_UINT32						ui32SrcLineStride,
											IMG_UINT32						ui32SrcBytesPerPixel,
											IMG_UINT32						ui32PassesRequired,
											IMG_UINT32						ui32CurPass,
											IMG_UINT32						* aui32Limms)
{
	IMG_UINT32 ui32HWStride;
	IMG_UINT32 ui32FWidth;
	IMG_UINT32 ui32FStride;
	IMG_UINT32 ui32FHeight;
	IMG_UINT32 ui32HWHeight;
	IMG_UINT32 ui32FHWHeight;
	IMG_UINT32 ui32NumTexels;

	if (psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_QWVU8888 ||
		psSrcSurf->eFormat == PVRSRV_PIXEL_FORMAT_XLVU8888)
	{
		if (ui32SrcLineStride % 32 == 0)
		{
			PVR_DPF((PVR_DBG_WARNING, "Doing strided blit with aligned src stride"));
			ui32HWStride = ui32SrcLineStride;
		}
		else
		{
			ui32HWStride = (ui32SrcLineStride - (ui32SrcLineStride % 32) + 32);
		}
		ui32NumTexels = ui32SrcLineStride * psSrcSurf->ui32Height;
	}
	else
	{
		if (ui32SrcLineStride % 32 == 0)
		{
			PVR_DPF((PVR_DBG_WARNING, "Doing strided blit with aligned src stride"));
			ui32HWStride = ui32SrcLineStride * ui32PassesRequired;
		}
		else
		{
			ui32HWStride = (ui32SrcLineStride - (ui32SrcLineStride % 32) + 32) * ui32PassesRequired;
		}
		ui32NumTexels = ui32SrcLineStride * psSrcSurf->ui32Height * ui32PassesRequired;
	}

	ui32FWidth = SGXTQ_FixedToFloat(ui32SrcLineStride << FIXEDPT_FRAC);
	ui32FHeight = SGXTQ_FixedToFloat(psSrcSurf->ui32Height << FIXEDPT_FRAC);
	ui32FStride = SGXTQ_FixedToFloat(ui32HWStride << FIXEDPT_FRAC);

	ui32HWHeight = (ui32NumTexels / ui32HWStride);
	/* There's an incomplete line that needs dealing with */
	if (ui32HWHeight * ui32HWStride < ui32NumTexels)
	{
		/* same page ? */
		if (((SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf) + ui32HWStride * ui32HWHeight * ui32SrcBytesPerPixel) & ~4095UL) ==
			((SGX_TQSURFACE_GET_DEV_VADDR(*psSrcSurf) + ui32HWStride * (ui32HWHeight + 1) * ui32SrcBytesPerPixel) & ~4095UL))
		{
			ui32HWHeight += 1;
		}
		else
		{
			return PVRSRV_ERROR_BLIT_SETUP_FAILED;
		}
	}

	ui32FHWHeight = SGXTQ_FixedToFloat((ui32HWHeight) << FIXEDPT_FRAC);

	/* New TAG state (we are going to do USE issues instead of TAG issues but the
	 * TAG state is going to be used in the shader, passed in the secondaries).
	 * We already had a TAG setup, all we want to clear is the width & height*/
	pui32TAGState[1] &=  EURASIA_PDS_DOUTT1_WIDTH_CLRMSK & EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK;

	ACCUMLATESET(pui32TAGState[1], ui32HWStride - 1, EURASIA_PDS_DOUTT1_WIDTH);
	ACCUMLATESET(pui32TAGState[1], ui32HWHeight - 1, EURASIA_PDS_DOUTT1_HEIGHT);

	if (eUSEProg == SGXTQ_USEBLIT_STRIDE_HIGHBPP)
	{
		/* Higher Bpp requires us to give the USE program the current
		 * passes and the required passes
		 */
		IMG_UINT32 ui32FNumPasses, ui32FCurPass;

		ui32FNumPasses = SGXTQ_FixedToFloat(ui32PassesRequired << FIXEDPT_FRAC);
		ui32FCurPass = SGXTQ_FixedToFloat(ui32CurPass << FIXEDPT_FRAC);

		aui32Limms[4] = ui32FNumPasses;
		aui32Limms[5] = ui32FCurPass;
	}

	aui32Limms[0] = ui32FHeight;
	aui32Limms[1] = ui32FWidth;
	aui32Limms[2] = ui32FStride;
	aui32Limms[3] = ui32FHWHeight;

	return PVRSRV_OK;
}
#endif /* ! SGX_FEATURE_USE_VEC34*/

/*****************************************************************************
 * Function Name		:	SGXTQ_SetPBEState
 * Inputs				:	psDstRect			- the PBE clip
 *							eMemLayout			- memory layout, linear, twiddled etc.
 *							ui32DstWidth		- surface dimension
 *							ui32DstHeight		- surface dimension
 *							ui32DstLineStride	- surface dimension
 *							ui32DstPBEPackMode	- pixel pack mode
 *							ui32DstDevVAddr		- frame buffer address
 *							ui32SrcSel			- When pixel pack mode is MONO8 or MONO16 ui32SrcSel
 *												  indicates source channel within output buffer that
 *												  should be used for source data. When PMODE is PT1
 *												  through PT8, indicates offset of data on output.
 *							eRotation			- PBE rotation
 * Outputs				:	aui32PBEState		- resulting PBE state
 * Description			:	Calculates the PBE pixemit state dwords
******************************************************************************/
IMG_INTERNAL PVRSRV_ERROR SGXTQ_SetPBEState(IMG_RECT	* psDstRect,
										SGXTQ_MEMLAYOUT	eMemLayout,
										IMG_UINT32		ui32DstWidth,
										IMG_UINT32		ui32DstHeight,
										IMG_UINT32		ui32DstLineStride,
										IMG_UINT32		ui32DstPBEPackMode,
										IMG_UINT32		ui32DstDevVAddr,
										IMG_UINT32		ui32SrcSel,
										SGXTQ_ROTATION	eRotation,
										IMG_BOOL		bEnableDithering,
										IMG_BOOL		bNewPixelHandling,
										IMG_UINT32		* aui32PBEState)
{
	IMG_UINT32 ui32PBEStride;
	IMG_UINT32 ui32Count;

	if (ui32DstLineStride <= ((1UL << EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT) - 1) &&
			psDstRect->y1 - psDstRect->y0 == 1)
	{
		/* if we have only 1 line the stride shouldn't matter, so we prevent the assertion
		 * going off on the stride underflow*/
		ui32PBEStride = 0;
	}
	else
	{
		ui32PBEStride = (ui32DstLineStride >> EURASIA_PIXELBE_LINESTRIDE_ALIGNSHIFT) - 1;	/* PRQA S 3382 */
	}

	ui32Count = EURASIA_ISPREGION_SIZEX * EURASIA_ISPREGION_SIZEY / EURASIA_TAPDSSTATE_PIPECOUNT;

#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)

	ACCUMLATESET(aui32PBEState[0], ui32Count, EURASIA_PIXELBES0LO_COUNT);

	if (! bNewPixelHandling)
	{
		ACCUMLATESETNS(aui32PBEState[0], ui32DstPBEPackMode, EURASIA_PIXELBES0LO_PACKMODE);
	}

	ACCUMLATESET(aui32PBEState[0], SGXTQ_MemLayoutFromEnum(eMemLayout, IMG_FALSE), EURASIA_PIXELBES0LO_MEMLAYOUT);


	CHECKBITSNOTLOST(ui32DstDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT);
	ui32DstDevVAddr >>= EURASIA_PIXELBE_FBADDR_ALIGNSHIFT;
	ACCUMLATESET(aui32PBEState[1], ui32DstDevVAddr, EURASIA_PIXELBES0HI_FBADDR);


	ACCUMLATESET(aui32PBEState[1], SGXTQ_RotationFromEnum(eRotation), EURASIA_PIXELBES0HI_ROTATE);


	ACCUMLATESET(aui32PBEState[2], ui32PBEStride, EURASIA_PIXELBES1LO_LINESTRIDE);

	/* we don't set anything to 1 HI */
	aui32PBEState[3] = 0;

	ACCUMLATESET(aui32PBEState[4], (IMG_UINT32)(psDstRect->x0), EURASIA_PIXELBES2LO_XMIN);
	ACCUMLATESET(aui32PBEState[4], (IMG_UINT32)(psDstRect->y0), EURASIA_PIXELBES2LO_YMIN);


	if (eMemLayout == SGXTQ_MEMLAYOUT_2D ||
			eMemLayout == SGXTQ_MEMLAYOUT_OUT_TWIDDLED)
	{
		IMG_UINT32 ui32XSize;
		IMG_UINT32 ui32YSize;

		ui32XSize = SGXTQ_FindNearestLog2(MAX(1, ui32DstWidth >> EURASIA_PIXELBES2LO_XSIZE_ALIGNSHIFT));
		ACCUMLATESET(aui32PBEState[4], ui32XSize, EURASIA_PIXELBES2LO_XSIZE);
		ui32YSize = SGXTQ_FindNearestLog2(MAX(1, ui32DstHeight >> EURASIA_PIXELBES2LO_YSIZE_ALIGNSHIFT));
		ACCUMLATESET(aui32PBEState[4], ui32YSize, EURASIA_PIXELBES2LO_YSIZE);
	}


	ACCUMLATESET(aui32PBEState[5], ((IMG_UINT32)(psDstRect->x1) - 1), EURASIA_PIXELBES2HI_XMAX);
	ACCUMLATESET(aui32PBEState[5], ((IMG_UINT32)(psDstRect->y1) - 1), EURASIA_PIXELBES2HI_YMAX);

	aui32PBEState[SGXTQ_PBESTATE_SBINDEX] |= EURASIA_PIXELBESB_TILERELATIVE;
	aui32PBEState[SGXTQ_PBESTATE_SBINDEX] |= (bEnableDithering ? EURASIA_PIXELBESB_DITHER : 0);

	if (! bNewPixelHandling)
	{
		ACCUMLATESET(aui32PBEState[SGXTQ_PBESTATE_SBINDEX], ui32SrcSel, EURASIA_PIXELBESB_SRCSEL);
	}

#else /* SGX_FEATURE_UNIFIED_STORE_64BITS */

#if defined(FIX_HW_BRN_23054)
	{
		IMG_INT32 i32ClampX0, i32ClampX1, i32ClampY0, i32ClampY1;

		/*
		 * get pixels in the last tile
		 */
		i32ClampX0 = MAX(0L, psDstRect->x0);
		i32ClampX1 = MIN((IMG_INT32)ui32DstWidth, psDstRect->x1);
		i32ClampY0 = MAX(0L, psDstRect->y0);
		i32ClampY1 = MIN((IMG_INT32)ui32DstHeight, psDstRect->y1);

		if (i32ClampX1 > 16)
		{
			i32ClampX0 = MAX((IMG_INT32)((IMG_UINT32)(i32ClampX1 - 1) & ~ 15UL), i32ClampX0);
		}
		if (i32ClampY1 > 16)
		{
			i32ClampY0 = MAX((IMG_INT32)((IMG_UINT32)(i32ClampY1 - 1) & ~ 15UL), i32ClampY0);
		}

		/* if we cared about bpp and rot this could be more fine tuned */
		if (eMemLayout == SGXTQ_MEMLAYOUT_2D
			|| eMemLayout == SGXTQ_MEMLAYOUT_OUT_TWIDDLED)
		{
			if ((i32ClampX1 - i32ClampX0 <= 7)
				|| (i32ClampY1 - i32ClampY0 <= 7))
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}
		}
		else
		{
			if ((i32ClampX1 - i32ClampX0 <= 3)
				|| (i32ClampY1 - i32ClampY0 <= 3))
			{
				return PVRSRV_ERROR_NOT_SUPPORTED;
			}
		}
	}
#endif
	ACCUMLATESET(aui32PBEState[0], psDstRect->x0, EURASIA_PIXELBE1S0_XMIN);
	ACCUMLATESET(aui32PBEState[0], psDstRect->y0, EURASIA_PIXELBE1S0_YMIN);


	if (eMemLayout == SGXTQ_MEMLAYOUT_2D || eMemLayout == SGXTQ_MEMLAYOUT_OUT_TWIDDLED)
	{
#if ! defined(SGX545)
		IMG_UINT32 ui32XSize;
		IMG_UINT32 ui32YSize;

		ui32XSize = SGXTQ_FindNearestLog2(MAX(1, ui32DstWidth >> EURASIA_PIXELBE1S0_XSIZE_ALIGNSHIFT));
		ACCUMLATESET(aui32PBEState[0], ui32XSize, EURASIA_PIXELBE1S0_XSIZE);
		ui32YSize = SGXTQ_FindNearestLog2(MAX(1, ui32DstHeight >> EURASIA_PIXELBE1S0_YSIZE_ALIGNSHIFT));
		ACCUMLATESET(aui32PBEState[0], ui32YSize, EURASIA_PIXELBE1S0_YSIZE);
#else
		if (((IMG_INT32)ui32DstWidth != psDstRect->x1) || ((IMG_INT32)ui32DstHeight != psDstRect->y1))
		{
			return PVRSRV_ERROR_NOT_SUPPORTED;
		}
#endif
	}

	ACCUMLATESET(aui32PBEState[1], ((IMG_UINT32)(psDstRect->x1-1)), EURASIA_PIXELBE1S1_XMAX);
	ACCUMLATESET(aui32PBEState[1], ((IMG_UINT32)(psDstRect->y1-1)), EURASIA_PIXELBE1S1_YMAX);

#if defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949)
	aui32PBEState[1] |= EURASIA_PIXELBE1S1_NOADVANCE;	
#endif /* defined(FIX_HW_BRN_23460) || defined(FIX_HW_BRN_23949) */


	ACCUMLATESET(aui32PBEState[2], ui32Count, EURASIA_PIXELBE2S0_COUNT);
#if !defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
	ACCUMLATESET(aui32PBEState[2], ui32PBEStride, EURASIA_PIXELBE2S0_LINESTRIDE);
#endif
#if !defined(SGX545)
	if (! bNewPixelHandling)
	{
		ACCUMLATESETNS(aui32PBEState[2], ui32DstPBEPackMode, EURASIA_PIXELBE2S0_PACKMODE);
	}
#endif

#if defined(SGX545)
	aui32PBEState[2] &= EURASIA_PIXELBE2S0_ROTATE_CLRMSK;
	aui32PBEState[2] |= SGXTQ_RotationFromEnum(eRotation);
#else
	ACCUMLATESET(aui32PBEState[2], SGXTQ_MemLayoutFromEnum(eMemLayout, IMG_FALSE), EURASIA_PIXELBE2S0_MEMLAYOUT);
#endif


	CHECKBITSNOTLOST(ui32DstDevVAddr, EURASIA_PIXELBE_FBADDR_ALIGNSHIFT);
	ui32DstDevVAddr >>= EURASIA_PIXELBE_FBADDR_ALIGNSHIFT;
	ACCUMLATESET(aui32PBEState[3], ui32DstDevVAddr, EURASIA_PIXELBE2S1_FBADDR);
#if !defined(SGX545)
	aui32PBEState[3] &= EURASIA_PIXELBE2S1_ROTATE_CLRMSK;
	aui32PBEState[3] |= SGXTQ_RotationFromEnum(eRotation);
#endif


#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
	ACCUMLATESET(aui32PBEState[4], ui32PBEStride, EURASIA_PIXELBE2S2_LINESTRIDE);
#endif


	if (! bNewPixelHandling)
	{
		ACCUMLATESET(aui32PBEState[SGXTQ_PBESTATE_SBINDEX], ui32SrcSel, EURASIA_PIXELBE2SB_SRCSEL);
	}
#if defined(SGX545)
	if (! bNewPixelHandling)
	{
		ACCUMLATESETNS(aui32PBEState[SGXTQ_PBESTATE_SBINDEX], ui32DstPBEPackMode, EURASIA_PIXELBE2SB_PACKMODE);
	}
#endif
	aui32PBEState[SGXTQ_PBESTATE_SBINDEX] |= EURASIA_PIXELBE2SB_TILERELATIVE;
#if defined(SGX545)
	ACCUMLATESET(aui32PBEState[SGXTQ_PBESTATE_SBINDEX], SGXTQ_MemLayoutFromEnum(eMemLayout, IMG_FALSE), EURASIA_PIXELBE2SB_MEMLAYOUT);
#endif
	aui32PBEState[SGXTQ_PBESTATE_SBINDEX] |= (bEnableDithering ? EURASIA_PIXELBE2SB_DITHER : 0);
#endif /* SGX_FEATURE_UNIFIED_STORE_64BITS */

	return PVRSRV_OK;
}

static INLINE PVRSRV_ERROR SGXTQ_CreateResource(SGXTQ_CLIENT_TRANSFER_CONTEXT	*psTQContext,
										SGXTQ_RESOURCE							*psResource,
										IMG_VOID								**ppvLinAddr,
										IMG_BOOL								bPDumpContinuous)
{
	if (psResource->eStorage == SGXTQ_STORAGE_CB ||
		psResource->eStorage == SGXTQ_STORAGE_NBUFFER)
	{
		IMG_UINT32 			ui32NonalignedSize = psResource->uStorage.sCB.ui32Size;
		const IMG_UINT32	*pui32Src = psResource->uStorage.sCB.pui32SrcAddr;
		SGXTQ_CB			* psCB = psResource->uStorage.sCB.psCB;

		if (! SGXTQ_AcquireCB(psTQContext->psFenceIDMemInfo,
					psTQContext->ui32FenceID,
					psTQContext->hOSEvent,
					psCB,
					ui32NonalignedSize,
					IMG_TRUE,
					ppvLinAddr,
					& psResource->sDevVAddr.uiAddr,
					bPDumpContinuous))
		{
			return PVRSRV_ERROR_TIMEOUT;
		}

		if (psResource->eStorage == SGXTQ_STORAGE_CB)
		{
			PVRSRVMemCopy(*ppvLinAddr, pui32Src, ui32NonalignedSize);
		}
	}

	return PVRSRV_OK;
}




IMG_INTERNAL PVRSRV_ERROR SGXTQ_CreateUSEEOTHandler(SGXTQ_CLIENT_TRANSFER_CONTEXT	*psTQContext,
									  IMG_UINT32									*aui32PBEState,
									  SGXTQ_USEEOTHANDLER							eEot,
									  IMG_UINT32									ui32Tmp6,
									  IMG_UINT32									ui32Tmp7,
									  IMG_BOOL										bPDumpContinuous)
{
	SGXTQ_RESOURCE		* psResource = psTQContext->apsUSEEOTHandlers[eEot];	/* PRQA S 3689 */ /* not out of bounds */

#if ! defined(SGXTQ_SUBTILE_TWIDDLING)
	PVR_UNREFERENCED_PARAMETER(ui32Tmp6);
	PVR_UNREFERENCED_PARAMETER(ui32Tmp7);
#endif

	switch (eEot)
	{
		case SGXTQ_USEEOTHANDLER_BASIC:
		{
			IMG_UINT32 			* pui32Inst;
			SGXTQ_CB_RESOURCE	* psCBResource = & psResource->uStorage.sCB;
			SGXTQ_CB			* psCB = psCBResource->psCB;

			if (! SGXTQ_AcquireCB(psTQContext->psFenceIDMemInfo,
						psTQContext->ui32FenceID,
						psTQContext->hOSEvent,
						psCB,
						psCBResource->ui32Size,
						IMG_TRUE,
						(IMG_VOID**) &pui32Inst,
						& psResource->sDevVAddr.uiAddr,
						bPDumpContinuous))
			{
				return PVRSRV_ERROR_TIMEOUT;
			}

			WriteEndOfTileUSSECode (pui32Inst, aui32PBEState, aui32PBEState[SGXTQ_PBESTATE_SBINDEX]
#if defined(SGX_FEATURE_RENDER_TARGET_ARRAYS)
					, IMG_NULL
#endif
#if defined(SGX_FEATURE_WRITEBACK_DCU)
					, IMG_FALSE
#endif
					);

			break;
		}
#if defined(SGXTQ_SUBTILE_TWIDDLING)
		case SGXTQ_USEEOTHANDLER_SUBTWIDDLED:
		{
			PVRSRV_ERROR	eError;
			IMG_PBYTE		pbyLinAddr;
			union
			{
				IMG_PBYTE		pby;
				IMG_PUINT32		pui32;
			} Temp;

			IMG_UINT32		i;

			eError = SGXTQ_CreateResource(psTQContext,
					psResource,
					(IMG_VOID **)&pbyLinAddr,
					bPDumpContinuous);
			if (PVRSRV_OK != eError)
			{
				return eError;
			}

			/* Further patches on EOT*/
			Temp.pby = pbyLinAddr +TQUSE_PIXELEVENTSUBTWIDDLED_SUBDIM_OFFSET;

			SGXTQ_WriteUSEMovToReg(Temp.pui32, 7, ui32Tmp7, EURASIA_USE1_D1STDBANK_TEMP, 0);
			Temp.pby += EURASIA_USE_INSTRUCTION_SIZE;
			SGXTQ_WriteUSEMovToReg(Temp.pui32, 6, ui32Tmp6, EURASIA_USE1_D1STDBANK_TEMP, 0);

			/* write the PBE state */
			/* todo: call the PDS pixevent; this is EOT.*/
			Temp.pby = pbyLinAddr + TQUSE_PIXELEVENTSUBTWIDDLED_PBESTATE_BYTE_OFFSET;

			for (i = 0; i < SGXTQ_PBESTATE_SBINDEX; i++)
			{
				/* we really need to rearrange the register layout */
				/* PRQA S 3356,3359,3201 2 */ /* some builds do allow i to be > 3*/
				if (i > 3) 
				{
					SGXTQ_WriteUSEMovToReg(Temp.pui32, i + 8, (aui32PBEState[i]), EURASIA_USE1_D1STDBANK_TEMP, 0);
				}
				else
				{
					SGXTQ_WriteUSEMovToReg(Temp.pui32, i, (aui32PBEState[i]), EURASIA_USE1_D1STDBANK_TEMP, 0);
				}
				Temp.pby += EURASIA_USE_INSTRUCTION_SIZE;
			}

			/* write the sideband*/
#if defined(FIX_HW_BRN_30842)
			Temp.pby = pbyLinAddr + TQUSE_PIXELEVENTSUBTWIDDLED_SIDEBAND_BYTE_OFFSET;
			/* emitpix2 r2, r3, #aui32PBEState[SGXTQ_PBESTATE_SBINDEX] or
			 * emitpix  r0, r2, r12, #sb
			 */
			*Temp.pui32 = (EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT)			|
							(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX])							<<
							  EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT)							&
							 ~EURASIA_USE0_EMIT_SIDEBAND_101_96_CLRMSK)							|
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
							(0UL << EURASIA_USE0_SRC0_SHIFT)									|
							((2UL >> 1) << EURASIA_USE0_SRC1_SHIFT)								|
							((12UL >> 1 ) << EURASIA_USE0_SRC2_SHIFT);
#else
							(2UL << EURASIA_USE0_SRC0_SHIFT)									|
							(3UL << EURASIA_USE0_SRC1_SHIFT)									|
#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
							(12UL << EURASIA_USE0_SRC2_SHIFT);
#else
							0;
#endif
#endif

			Temp.pby += EURASIA_USE_INSTRUCTION_SIZE / 2;
			*Temp.pui32 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT)						|
							(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT)			|
							(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX] >> 12)							<<
							  EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT)								&
							 ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK)							|
							(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
							(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT)	|
							(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX] >> 6)							<<
							  EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT)								&
							 ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK)							|
							(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT)				|
							(0UL << EURASIA_USE1_EMIT_INCP_SHIFT);

#else
			if ((ui32Tmp6 & SGXTQ_SUBTWIDDLED_TMP6_2PIPE_MODE_MASK) == 0)
			{
				Temp.pby = pbyLinAddr + TQUSE_PIXELEVENTSUBTWIDDLED_SIDEBAND_SINGLEPIPE_BYTE_OFFSET;

				/* fast path*/
				*Temp.pui32 = (EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT)|
					(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX])							<<
					  EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT)							&
					 ~EURASIA_USE0_EMIT_SIDEBAND_101_96_CLRMSK)							|
					EURASIA_USE0_EMIT_FREEP												|	
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
					(0UL << EURASIA_USE0_SRC0_SHIFT)									|
					((2UL >> 1) << EURASIA_USE0_SRC1_SHIFT)								|
					((12UL >> 1 ) << EURASIA_USE0_SRC2_SHIFT);
#else
					(2UL << EURASIA_USE0_SRC0_SHIFT)									|
					(3UL << EURASIA_USE0_SRC1_SHIFT)									|
#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
					(12UL << EURASIA_USE0_SRC2_SHIFT);
#else
					0;
#endif
#endif

				Temp.pby += EURASIA_USE_INSTRUCTION_SIZE / 2;
				*Temp.pui32 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT)			|
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT)			|
					(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX] >> 12)							<<
					  EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT)								&
					 ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK)							|
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
					(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT)	|
					(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX] >> 6)							<<
					  EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT)								&
					 ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK)							|
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT)				|
					(0UL << EURASIA_USE1_EMIT_INCP_SHIFT)									|
					EURASIA_USE1_END;
			}
			else
			{
				/* 2 emits per pipe, 1 dummy, 1 valid*/

				Temp.pby = pbyLinAddr + TQUSE_PIXELEVENTSUBTWIDDLED_SIDEBAND_2PIPES_FIRST_BYTE_OFFSET;

				/* don't forget to set NOADVANCE high for this and set low for the next one again*/
				*Temp.pui32 = (EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT)|
					(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX])							<<
					  EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT)							&
					 ~EURASIA_USE0_EMIT_SIDEBAND_101_96_CLRMSK)							|
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
					(0UL << EURASIA_USE0_SRC0_SHIFT)									|
					((2UL >> 1) << EURASIA_USE0_SRC1_SHIFT)								|
					((12UL >> 1 ) << EURASIA_USE0_SRC2_SHIFT);
#else
					(2UL << EURASIA_USE0_SRC0_SHIFT)									|
					(3UL << EURASIA_USE0_SRC1_SHIFT)									|
#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
					(12UL << EURASIA_USE0_SRC2_SHIFT);
#else
					0;
#endif
#endif

				Temp.pby += EURASIA_USE_INSTRUCTION_SIZE / 2;
				*Temp.pui32 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT)			|
					(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT)			|
					(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX] >> 12)							<<
					  EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT)								&
					 ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK)							|
					(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
					(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT)	|
					(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX] >> 6)							<<
					  EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT)								&
					 ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK)							|
					(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT)				|
					(0UL << EURASIA_USE1_EMIT_INCP_SHIFT);
					;

				Temp.pby = pbyLinAddr + TQUSE_PIXELEVENTSUBTWIDDLED_SIDEBAND_2PIPES_SECOND_BYTE_OFFSET;

				*Temp.pui32 = (EURASIA_USE0_S1STDBANK_TEMP << EURASIA_USE0_S1BANK_SHIFT)|
					(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX])							<<
					  EURASIA_USE0_EMIT_SIDEBAND_101_96_SHIFT)							&
					 ~EURASIA_USE0_EMIT_SIDEBAND_101_96_CLRMSK)							|
					EURASIA_USE0_EMIT_FREEP												|	
#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
					(0UL << EURASIA_USE0_SRC0_SHIFT)									|
					((2UL >> 1) << EURASIA_USE0_SRC1_SHIFT)								|
					((12UL >> 1 ) << EURASIA_USE0_SRC2_SHIFT);
#else
					(2UL << EURASIA_USE0_SRC0_SHIFT)									|
					(3UL << EURASIA_USE0_SRC1_SHIFT)									|
#if defined(SGX_FEATURE_PIXELBE_32K_LINESTRIDE)
					(12UL << EURASIA_USE0_SRC2_SHIFT);
#else
				0;
#endif
#endif

				Temp.pby += EURASIA_USE_INSTRUCTION_SIZE / 2;
				*Temp.pui32 = (EURASIA_USE1_OP_SPECIAL << EURASIA_USE1_OP_SHIFT)					|
							(EURASIA_USE1_OTHER_OP2_EMIT << EURASIA_USE1_OTHER_OP2_SHIFT)			|
							(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX] >> 12)							<<
							  EURASIA_USE1_EMIT_SIDEBAND_109_108_SHIFT)								&
							 ~EURASIA_USE1_EMIT_SIDEBAND_109_108_CLRMSK)							|
							(EURASIA_USE1_SPECIAL_OPCAT_OTHER << EURASIA_USE1_SPECIAL_OPCAT_SHIFT)	|
							(EURASIA_USE1_EMIT_TARGET_PIXELBE << EURASIA_USE1_EMIT_TARGET_SHIFT)	|
							(((aui32PBEState[SGXTQ_PBESTATE_SBINDEX] >> 6)							<<
							  EURASIA_USE1_EMIT_SIDEBAND_107_102_SHIFT)								&
							 ~EURASIA_USE1_EMIT_SIDEBAND_107_102_CLRMSK)							|
							(EURASIA_USE1_S0STDBANK_TEMP << EURASIA_USE1_S0BANK_SHIFT)				|
							(0UL << EURASIA_USE1_EMIT_INCP_SHIFT);

			}
#endif
			break;
		}
#endif
		default:
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}


	return PVRSRV_OK;
}


/**************************************************************************//**
@brief          Allocates a secondary USSE program if necessary, and sets up the
				PDS DOUTUs accordingly. 

@param          psTQContext
@param          ePDSSec
@param          psPDSValues
@param          bPDumpContinuous

@returns        eError
******************************************************************************/
IMG_INTERNAL
PVRSRV_ERROR SGXTQ_CreateUSESecondaryResource(SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext,
											SGXTQ_PDSSECFRAGS				ePDSSec,
											SGXTQ_PDS_UPDATE				* psPDSValues,
											IMG_BOOL						bPDumpContinuous)
{
#if ! defined(SGX_FEATURE_SECONDARY_REQUIRES_USE_KICK)
	if (ePDSSec != SGXTQ_PDSSECFRAG_BASIC)
#endif
	{
		SGXTQ_RESOURCE	* psResource;
		PVRSRV_ERROR	eError;

		switch (ePDSSec)
		{
#if defined(FIX_HW_BRN_31988)
			/* the ones with DOUTD ; THIS LIST HAS TO HAVE ALL DOUTD-ING SECONDARIES*/
			case SGXTQ_PDSSECFRAG_DMA_ONLY:
			case SGXTQ_PDSSECFRAG_4ATTR_DMA:
			case SGXTQ_PDSSECFRAG_5ATTR_DMA:
			{
				eError = SGXTQ_CreateUSEResource(psTQContext,
						SGXTQ_USESECONDARY_UPDATE_DMA,
						IMG_NULL,
						bPDumpContinuous);
				if (PVRSRV_OK != eError)
				{
					return eError;
				}

				psResource = psTQContext->apsUSEResources[SGXTQ_USESECONDARY_UPDATE_DMA];


				break;
			}
#endif
			default:
			{
				eError = SGXTQ_CreateUSEResource(psTQContext,
						SGXTQ_USESECONDARY_UPDATE,
						IMG_NULL,
						bPDumpContinuous);
				if (PVRSRV_OK != eError)
				{
					return eError;
				}

				psResource = psTQContext->apsUSEResources[SGXTQ_USESECONDARY_UPDATE];
			}
		}
		SGXTQ_SetUSSEKick(psPDSValues,
				psResource->sDevVAddr,
				psTQContext->sUSEExecBase,
				psResource->uResource.sUSE.ui32NumTempRegs);

	}
	return PVRSRV_OK;
}


IMG_INTERNAL PVRSRV_ERROR SGXTQ_CreatePDSPixeventHandler(SGXTQ_CLIENT_TRANSFER_CONTEXT	* psTQContext,
									  SGXTQ_RESOURCE									* psEORHandler,
									  SGXTQ_RESOURCE									* psEOTHandler,
									  SGXTQ_PDSPIXEVENTHANDLER							ePixEvent,
									  IMG_BOOL											bPDumpContinuous)
{
	SGXTQ_RESOURCE			* psResource = psTQContext->apsPDSPixeventHandlers[ePixEvent];	/* PRQA S 3689 */ /* not out of bounds */
	SGXTQ_PDS_RESOURCE		* psPDSResource = & psResource->uResource.sPDS;
	IMG_UINT32				ui32RequiredSize = psResource->uStorage.sCB.ui32Size;
	SGXTQ_CB				* psCB = psResource->uStorage.sCB.psCB;
	PDS_PIXEL_EVENT_PROGRAM sPixelEventProg;
	IMG_PUINT32				pui32LinAddr;
	IMG_UINT32 				ui32TempRegs;

	if (! SGXTQ_AcquireCB(psTQContext->psFenceIDMemInfo,
				psTQContext->ui32FenceID,
				psTQContext->hOSEvent,
				psCB,
				ui32RequiredSize,
				IMG_TRUE,
				(IMG_VOID **) &pui32LinAddr,
				& psResource->sDevVAddr.uiAddr,
				bPDumpContinuous))
	{
		return PVRSRV_ERROR_TIMEOUT;
	}

	sPixelEventProg.aui32EOTUSETaskControl[0] = 0;

	SetUSEExecutionAddress(&sPixelEventProg.aui32EOTUSETaskControl[0],
			0,
			psEOTHandler->sDevVAddr,
			psTQContext->sUSEExecBase,
			SGXTQ_USE_CODE_BASE_INDEX);

	/* USE controller doesn't like PERINSTANCE with emitpix*/
	sPixelEventProg.aui32EOTUSETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL;

	ui32TempRegs = ALIGNFIELD(psEOTHandler->uResource.sUSE.ui32NumTempRegs, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	CHECKOVERFLOW(ui32TempRegs, EURASIA_PDS_DOUTU0_TRC_CLRMSK, EURASIA_PDS_DOUTU0_TRC_SHIFT);

	sPixelEventProg.aui32EOTUSETaskControl[0] |= (ui32TempRegs << EURASIA_PDS_DOUTU0_TRC_SHIFT) &
		~EURASIA_PDS_DOUTU0_TRC_CLRMSK;
#if (SGX_FEATURE_USE_NUMBER_PC_BITS == 20)
	sPixelEventProg.aui32EOTUSETaskControl[2] = EURASIA_PDS_DOUTU2_PBENABLE;
#else
	sPixelEventProg.aui32EOTUSETaskControl[1] |= EURASIA_PDS_DOUTU1_PBENABLE;
	sPixelEventProg.aui32EOTUSETaskControl[2] = 0;
#endif

#else
	CHECKOVERFLOW(ui32TempRegs, EURASIA_PDS_DOUTU1_TRC_CLRMSK, EURASIA_PDS_DOUTU1_TRC_SHIFT);

	sPixelEventProg.aui32EOTUSETaskControl[1] |= (ui32TempRegs << EURASIA_PDS_DOUTU1_TRC_SHIFT) &
		~EURASIA_PDS_DOUTU1_TRC_CLRMSK;

	sPixelEventProg.aui32EOTUSETaskControl[2] = EURASIA_PDS_DOUTU2_PBENABLE;
#endif

	/* Should never hit PTOFF, but set to EOT anyway */
	sPixelEventProg.aui32PTOFFUSETaskControl[0] = sPixelEventProg.aui32EOTUSETaskControl[0];
	sPixelEventProg.aui32PTOFFUSETaskControl[1] = sPixelEventProg.aui32EOTUSETaskControl[1];
	sPixelEventProg.aui32PTOFFUSETaskControl[2] = sPixelEventProg.aui32EOTUSETaskControl[2];

	/* set up the EOR handler*/
	sPixelEventProg.aui32EORUSETaskControl[0] = 0;

	SetUSEExecutionAddress(&sPixelEventProg.aui32EORUSETaskControl[0],
			0,
			psEORHandler->sDevVAddr,
			psTQContext->sUSEExecBase,
			SGXTQ_USE_CODE_BASE_INDEX);

	sPixelEventProg.aui32EORUSETaskControl[1] = EURASIA_PDS_DOUTU1_MODE_PARALLEL;

	ui32TempRegs = ALIGNFIELD(psEORHandler->uResource.sUSE.ui32NumTempRegs, EURASIA_PDS_DOUTU_TRC_ALIGNSHIFT);
#if defined(SGX_FEATURE_USE_UNLIMITED_PHASES)
	CHECKOVERFLOW(ui32TempRegs, EURASIA_PDS_DOUTU0_TRC_CLRMSK, EURASIA_PDS_DOUTU0_TRC_SHIFT);

	sPixelEventProg.aui32EORUSETaskControl[0] |=	(ui32TempRegs << EURASIA_PDS_DOUTU0_TRC_SHIFT) &
		~EURASIA_PDS_DOUTU0_TRC_CLRMSK;

#if (SGX_FEATURE_USE_NUMBER_PC_BITS == 20)
	sPixelEventProg.aui32EORUSETaskControl[2] = EURASIA_PDS_DOUTU2_PBENABLE | (1UL << EURASIA_PDS_DOUTU2_ENDOFRENDER_SHIFT);;
#else
	sPixelEventProg.aui32EORUSETaskControl[1] |= EURASIA_PDS_DOUTU1_PBENABLE | (1UL << EURASIA_PDS_DOUTU1_ENDOFRENDER_SHIFT);
	sPixelEventProg.aui32EORUSETaskControl[2] = 0;
#endif

#else
	CHECKOVERFLOW(ui32TempRegs, EURASIA_PDS_DOUTU1_TRC_CLRMSK, EURASIA_PDS_DOUTU1_TRC_SHIFT);

	sPixelEventProg.aui32EORUSETaskControl[1] |=	(ui32TempRegs << EURASIA_PDS_DOUTU1_TRC_SHIFT) &
		~EURASIA_PDS_DOUTU1_TRC_CLRMSK;

	sPixelEventProg.aui32EORUSETaskControl[2] = EURASIA_PDS_DOUTU2_PBENABLE |
		(1UL << EURASIA_PDS_DOUTU2_ENDOFRENDER_SHIFT);
#endif

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
			ui32TempRegs = MAX(psEORHandler->uResource.sUSE.ui32NumTempRegs,
					psEOTHandler->uResource.sUSE.ui32NumTempRegs);

			psPDSResource->ui32TempRegs = ui32TempRegs;
#endif


	switch (ePixEvent)
	{
		case SGXTQ_PDSPIXEVENTHANDLER_BASIC:
		{

			PDSGeneratePixelEventProgram(&sPixelEventProg, pui32LinAddr, IMG_FALSE, IMG_FALSE, 0);
			break;
		}
		case SGXTQ_PDSPIXEVENTHANDLER_TILEXY:
		{
			PDSGeneratePixelEventProgramTileXY(&sPixelEventProg, pui32LinAddr);
			break;
		}
		default:
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}
	psPDSResource->ui32DataLen = sPixelEventProg.ui32DataSize;

	return PVRSRV_OK;
}

IMG_INTERNAL IMG_VOID SGXTQ_SetupPixeventRegs(SGXTQ_CLIENT_TRANSFER_CONTEXT	*psTQContext,
										SGXMKIF_TRANSFERCMD					*psSubmit,
										SGXTQ_RESOURCE						*psPixEvent)
{
	IMG_UINT32			ui32Tmp;
	SGXTQ_PDS_RESOURCE	* psPDSPix = &psPixEvent->uResource.sPDS;

	ui32Tmp = psPixEvent->sDevVAddr.uiAddr;

#if defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
	PVR_UNREFERENCED_PARAMETER(psTQContext);
#else
	psSubmit->sHWRegs.ui32PDSExecBase = psTQContext->sPDSExecBase.uiAddr;
#if !defined(SGX_FEATURE_EDM_VERTEX_PDSADDR_FULL_RANGE)
	PVR_ASSERT(ui32Tmp >= psTQContext->sPDSExecBase.uiAddr);
	ui32Tmp -= psTQContext->sPDSExecBase.uiAddr;
#endif
#endif /* defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE) */

	CHECKBITSNOTLOST(ui32Tmp, EUR_CR_EVENT_PIXEL_PDS_EXEC_ADDR_ALIGNSHIFT);
	ui32Tmp >>= EUR_CR_EVENT_PIXEL_PDS_EXEC_ADDR_ALIGNSHIFT;
	CHECKOVERFLOW(ui32Tmp, ~EUR_CR_EVENT_PIXEL_PDS_EXEC_ADDR_MASK, EUR_CR_EVENT_PIXEL_PDS_EXEC_ADDR_SHIFT);
	psSubmit->sHWRegs.ui32EDMPixelPDSExec = ui32Tmp << EUR_CR_EVENT_PIXEL_PDS_EXEC_ADDR_SHIFT;

	CHECKBITSNOTLOST(psPDSPix->ui32DataLen, EUR_CR_EVENT_PIXEL_PDS_DATA_SIZE_ALIGNSHIFT);
	psSubmit->sHWRegs.ui32EDMPixelPDSData = (psPDSPix->ui32DataLen >> EUR_CR_EVENT_PIXEL_PDS_DATA_SIZE_ALIGNSHIFT) <<
											EUR_CR_EVENT_PIXEL_PDS_DATA_SIZE_SHIFT;

#if defined(FIX_HW_BRN_23070) && defined(FIX_HW_BRN_23615)
	PVR_ASSERT(psPDSPix->ui32Attributes == 0);
#endif

	ui32Tmp = psPDSPix->ui32Attributes;
#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	ui32Tmp += psPDSPix->ui32TempRegs;
#endif
	ui32Tmp = ALIGNFIELD(ui32Tmp * sizeof(IMG_UINT32), EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_ALIGNSHIFT);
	CHECKOVERFLOW(ui32Tmp, ~EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_MASK, EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_SHIFT);
	psSubmit->sHWRegs.ui32EDMPixelPDSInfo = ui32Tmp << EUR_CR_EVENT_PIXEL_PDS_INFO_ATTRIBUTE_SIZE_SHIFT;

	psSubmit->sHWRegs.ui32EDMPixelPDSInfo |=
#if defined(EUR_CR_EVENT_PIXEL_PDS_INFO_DM_PIXEL)
											EUR_CR_EVENT_PIXEL_PDS_INFO_DM_PIXEL			|
#endif
#if defined(FIX_HW_BRN_23615)
#if defined(FIX_HW_BRN_23070)
											EUR_CR_EVENT_PIXEL_PDS_INFO_PNS_MASK			|
#else
											(psPDSPix->ui32Attributes == 0					?
											EUR_CR_EVENT_PIXEL_PDS_INFO_PNS_MASK			:
											EUR_CR_EVENT_PIXEL_PDS_INFO_USESECEXEC_MASK)	|
#endif /* FIX_HW_BRN_23070 */
#else
#if defined(EUR_CR_EVENT_PIXEL_PDS_INFO_PNS_MASK)
											EUR_CR_EVENT_PIXEL_PDS_INFO_PNS_MASK			|
#endif /* EUR_CR_EVENT_PIXEL_PDS_INFO_PNS_MASK*/
#endif /* FIX_HW_BRN_23615*/
#if defined(EUR_CR_EVENT_PIXEL_PDS_INFO_USE_PIPELINE_BOTH)
											EUR_CR_EVENT_PIXEL_PDS_INFO_USE_PIPELINE_BOTH	|
#endif /* EUR_CR_EVENT_PIXEL_PDS_INFO_USE_PIPELINE_BOTH*/
#if defined(EUR_CR_EVENT_PIXEL_PDS_INFO_ODS_SHIFT)
											(127UL << EUR_CR_EVENT_PIXEL_PDS_INFO_ODS_SHIFT)|
#endif
											0;


}

IMG_INTERNAL IMG_VOID SGXTQ_SetupTransferRegs(SGXTQ_CLIENT_TRANSFER_CONTEXT	*psTQContext,
										IMG_UINT32							ui32BIFTile0Config,
										SGXMKIF_TRANSFERCMD					*psSubmit,
										SGXTQ_RESOURCE						*psPixEvent,
										IMG_UINT32							ui32NumLayers,
										IMG_UINT32							ui32ScanDirection,
										IMG_UINT32							ui32ISPRenderType)
{
	IMG_UINT32			ui32Tmp;
	SGXTQ_RESOURCE		* psHWBGObjResource;

#if ! defined(SUPPORT_DISPLAYCONTROLLER_TILING)
	PVR_UNREFERENCED_PARAMETER(ui32BIFTile0Config);
#endif

#if ! defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
	PVR_UNREFERENCED_PARAMETER(ui32ScanDirection);
#endif

	/* Set HW registers */
	psSubmit->sHWRegs.ui32ISPBgObj = EUR_CR_ISP_BGOBJ_ENABLEBGTAG_MASK;
	psSubmit->sHWRegs.ui32ISPBgObjDepth = FLOAT32_ONE;
	psSubmit->sHWRegs.ui32ISPRender = ui32ISPRenderType;

#if defined(SGX_FEATURE_NATIVE_BACKWARD_BLIT)
	psSubmit->sHWRegs.ui32ISPRender |= ui32ScanDirection;
#endif

	psSubmit->sHWRegs.ui32ISPIPFMisc    = EUR_CR_ISP_IPFMISC_PROCESSEMPTY_MASK;

	PVR_ASSERT(ui32NumLayers < SGXTQ_NUM_HWBGOBJS);
	psHWBGObjResource = psTQContext->apsISPResources[ui32NumLayers];	/* PRQA S 3689 */ /* arrary not out of bounds */

	CHECKBITSNOTLOST(psTQContext->sISPStreamBase.uiAddr, EUR_CR_BIF_3D_REQ_BASE_ADDR_ALIGNSHIFT);
	psSubmit->sHWRegs.ui32Bif3DReqBase = (psTQContext->sISPStreamBase.uiAddr >> EUR_CR_BIF_3D_REQ_BASE_ADDR_ALIGNSHIFT) <<
										EUR_CR_BIF_3D_REQ_BASE_ADDR_SHIFT;

	if (ui32ISPRenderType != EUR_CR_ISP_RENDER_TYPE_FASTSCALE)
	{
		SGXTQ_RESOURCE * psFast2DResource = psTQContext->psFast2DISPControlStream;

		PVR_ASSERT(psFast2DResource->sDevVAddr.uiAddr >= psTQContext->sISPStreamBase.uiAddr);
		ui32Tmp = psFast2DResource->sDevVAddr.uiAddr - psTQContext->sISPStreamBase.uiAddr;
		CHECKBITSNOTLOST(ui32Tmp, EUR_CR_ISP_RGN_BASE_ADDR_ALIGNSHIFT);
		ui32Tmp >>= EUR_CR_ISP_RGN_BASE_ADDR_ALIGNSHIFT;
		CHECKOVERFLOW(ui32Tmp, ~EUR_CR_ISP_RGN_BASE_ADDR_MASK, EUR_CR_ISP_RGN_BASE_ADDR_SHIFT);

		psSubmit->sHWRegs.ui32ISPRgnBase = ui32Tmp << EUR_CR_ISP_RGN_BASE_ADDR_SHIFT;
		psSubmit->sHWRegs.ui32ISPBgObj |= EUR_CR_ISP_BGOBJ_MASK_MASK;
	}

	PVR_ASSERT(psHWBGObjResource->sDevVAddr.uiAddr >= psTQContext->sISPStreamBase.uiAddr);
	ui32Tmp = psHWBGObjResource->sDevVAddr.uiAddr - psTQContext->sISPStreamBase.uiAddr;
	CHECKBITSNOTLOST(ui32Tmp, EUR_CR_ISP_BGOBJTAG_VERTEXPTR_ALIGNSHIFT);
	ui32Tmp >>= EUR_CR_ISP_BGOBJTAG_VERTEXPTR_ALIGNSHIFT;
	CHECKOVERFLOW(ui32Tmp, ~EUR_CR_ISP_BGOBJTAG_VERTEXPTR_MASK, EUR_CR_ISP_BGOBJTAG_VERTEXPTR_SHIFT);
	psSubmit->sHWRegs.ui32ISPBgObjTag = (ui32Tmp << EUR_CR_ISP_BGOBJTAG_VERTEXPTR_SHIFT) |
										((2 * ui32NumLayers) << EUR_CR_ISP_BGOBJTAG_TSPDATASIZE_SHIFT);


	psSubmit->sHWRegs.ui323DAAMode = 0;

#if defined(EUR_CR_3D_AA_MODE_GLOBALREGISTER_MASK)
	/* Enable global sample positions */
	psSubmit->sHWRegs.ui323DAAMode		|= EUR_CR_3D_AA_MODE_GLOBALREGISTER_MASK;
#endif

	psSubmit->sHWRegs.ui32ISPMultiSampCtl = (0x8UL << EUR_CR_MTE_MULTISAMPLECTL_MSAA_X0_SHIFT) |
											(0x8UL << EUR_CR_MTE_MULTISAMPLECTL_MSAA_Y0_SHIFT);

	/* set up the pixevent handling */
	SGXTQ_SetupPixeventRegs(psTQContext, psSubmit, psPixEvent);

	psSubmit->sHWRegs.ui32PixelBE		=	(0x80UL << EUR_CR_PIXELBE_ALPHATHRESHOLD_SHIFT);

#if defined(SUPPORT_DISPLAYCONTROLLER_TILING)
	psSubmit->sHWRegs.ui32BIFTile0Config = ui32BIFTile0Config;
#endif
}



/* PRQA S 0881 4 */ /* ignore 'order of evaluation' warning */
#define UpdateDOUTUs(name, pui32Dest)																			\
	PDSTQ_##name##SetDOUTU0(pui32Dest, psPDSValues->ui32U0);													\
	PDSTQ_##name##SetDOUTU1(pui32Dest, psPDSValues->ui32U1);													\
	PDSTQ_##name##SetDOUTU2(pui32Dest, psPDSValues->ui32U2);


/* PRQA S 0881 8 */ /* ignore 'order of evaluation' warning */
#if (EURASIA_PDS_DOUTI_STATE_SIZE == 2)
#define UpdateDOUTIs(name, layer, pui32Dest)																	\
	PDSTQ_##name##SetDOUTI0_SRC##layer(pui32Dest, psPDSValues->asLayers[layer].aui32ITERState[0]);				\
	PDSTQ_##name##SetDOUTI1_SRC##layer(pui32Dest, psPDSValues->asLayers[layer].aui32ITERState[1]);
#else
#define UpdateDOUTIs(name, layer, pui32Dest)																	\
	PDSTQ_##name##SetDOUTI0_SRC##layer(pui32Dest, psPDSValues->asLayers[layer].aui32ITERState[0]);
#endif


/* PRQA S 0881 12 */ /* ignore 'order of evaluation' warning */
#if (EURASIA_TAG_TEXTURE_STATE_SIZE == 4)
#define UpdateDOUTTs(name, layer, pui32Dest)																	\
	PDSTQ_##name##SetDOUTT0_SRC##layer(pui32Dest, psPDSValues->asLayers[layer].aui32TAGState[0]);				\
	PDSTQ_##name##SetDOUTT1_SRC##layer(pui32Dest, psPDSValues->asLayers[layer].aui32TAGState[1]);				\
	PDSTQ_##name##SetDOUTT2_SRC##layer(pui32Dest, psPDSValues->asLayers[layer].aui32TAGState[2]);				\
	PDSTQ_##name##SetDOUTT3_SRC##layer(pui32Dest, psPDSValues->asLayers[layer].aui32TAGState[3]);
#else
#define UpdateDOUTTs(name, layer, pui32Dest)																	\
	PDSTQ_##name##SetDOUTT0_SRC##layer(pui32Dest, psPDSValues->asLayers[layer].aui32TAGState[0]);				\
	PDSTQ_##name##SetDOUTT1_SRC##layer(pui32Dest, psPDSValues->asLayers[layer].aui32TAGState[1]);				\
	PDSTQ_##name##SetDOUTT2_SRC##layer(pui32Dest, psPDSValues->asLayers[layer].aui32TAGState[2]);
#endif

/* PRQA S 0881 16 */ /* ignore 'order of evaluation' warning */
#define UpdateDOUTA0(name, pui32Dest)																			\
	PDSTQ_##name##SetDOUTA0(pui32Dest, psPDSValues->aui32A[0]);
#define UpdateDOUTA1(name, pui32Dest)																			\
	PDSTQ_##name##SetDOUTA1(pui32Dest, psPDSValues->aui32A[1]);
#define UpdateDOUTA2(name, pui32Dest)																			\
	PDSTQ_##name##SetDOUTA2(pui32Dest, psPDSValues->aui32A[2]);
#define UpdateDOUTA3(name, pui32Dest)																			\
	PDSTQ_##name##SetDOUTA3(pui32Dest, psPDSValues->aui32A[3]);
#define UpdateDOUTA4(name, pui32Dest)																			\
	PDSTQ_##name##SetDOUTA4(pui32Dest, psPDSValues->aui32A[4]);
#define UpdateDOUTA5(name, pui32Dest)																			\
	PDSTQ_##name##SetDOUTA5(pui32Dest, psPDSValues->aui32A[5]);
#define UpdateDOUTA6(name, pui32Dest)																			\
	PDSTQ_##name##SetDOUTA6(pui32Dest, psPDSValues->aui32A[6]);
#define UpdateDOUTA7(name, pui32Dest)																			\
	PDSTQ_##name##SetDOUTA7(pui32Dest, psPDSValues->aui32A[7]);

/* PRQA S 0881 3 */ /* ignore 'order of evaluation' warning */
#define UpdateDOUTDs(name, pui32Dest)																			\
	PDSTQ_##name##SetDOUTD0(pui32Dest, psPDSValues->ui32D0);													\
	PDSTQ_##name##SetDOUTD1(pui32Dest, psPDSValues->ui32D1);

/*****************************************************************************
 * Function Name		:	SGXTQ_CreatePDSPrimResource
 * Inputs				:	TBD
 * Outputs				:	-
 * Description			:
 * Note					:
******************************************************************************/
IMG_INTERNAL PVRSRV_ERROR SGXTQ_CreatePDSPrimResource(SGXTQ_CLIENT_TRANSFER_CONTEXT*	psTQContext,
										SGXTQ_PDSPRIMFRAGS								ePDSPrim,
										SGXTQ_PDS_UPDATE								*psPDSValues,
										IMG_BOOL										bPDumpContinuous)
{
	PVRSRV_ERROR		eError;
	IMG_UINT32 			*pui32LinAddr = IMG_NULL;
	SGXTQ_RESOURCE		* psResource = psTQContext->apsPDSPrimResources[ePDSPrim];	/* PRQA S 3689 */ /* appears to be within bounds */
#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	SGXTQ_PDS_RESOURCE	* psPDSResource = & psResource->uResource.sPDS;
#endif

	eError = SGXTQ_CreateResource(psTQContext,
			psResource,
			(IMG_VOID *)&pui32LinAddr,
			bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

#if defined(SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	psPDSResource->ui32TempRegs = psPDSValues->ui32TempRegs;
#endif

	switch (ePDSPrim)
	{
		case SGXTQ_PDSPRIMFRAG_KICKONLY:
		{
			UpdateDOUTUs(KICKONLY, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSPRIMFRAG_ITER:
		{
			UpdateDOUTUs(ITER, pui32LinAddr)
			UpdateDOUTIs(ITER, 0, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSPRIMFRAG_SINGLESOURCE:
		{
			UpdateDOUTUs(1SRC, pui32LinAddr)
			UpdateDOUTIs(1SRC, 0, pui32LinAddr)
			UpdateDOUTTs(1SRC, 0, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSPRIMFRAG_TWOSOURCE:
		{
			UpdateDOUTUs(2SRC, pui32LinAddr)
			UpdateDOUTIs(2SRC, 0, pui32LinAddr)
			UpdateDOUTTs(2SRC, 0, pui32LinAddr)
			UpdateDOUTIs(2SRC, 1, pui32LinAddr)
			UpdateDOUTTs(2SRC, 1, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSPRIMFRAG_THREESOURCE:
		{
			UpdateDOUTUs(3SRC, pui32LinAddr)
			UpdateDOUTIs(3SRC, 0, pui32LinAddr)
			UpdateDOUTTs(3SRC, 0, pui32LinAddr)
			UpdateDOUTIs(3SRC, 1, pui32LinAddr)
			UpdateDOUTTs(3SRC, 1, pui32LinAddr)
			UpdateDOUTIs(3SRC, 2, pui32LinAddr)
			UpdateDOUTTs(3SRC, 2, pui32LinAddr)
			break;
		}
		default:
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}
	return PVRSRV_OK;
}


/*****************************************************************************
 * Function Name		:	SGXTQ_CreatePDSSecResource
 * Inputs				:	TBD
 * Outputs				:	-
 * Description			:
 * Note					:
******************************************************************************/
IMG_INTERNAL PVRSRV_ERROR SGXTQ_CreatePDSSecResource(SGXTQ_CLIENT_TRANSFER_CONTEXT*	psTQContext,
										SGXTQ_PDSSECFRAGS							ePDSSec,
										SGXTQ_PDS_UPDATE							*psPDSValues,
										IMG_BOOL									bPDumpContinuous)
{
	PVRSRV_ERROR		eError;
	IMG_UINT32			*pui32LinAddr = IMG_NULL;
	SGXTQ_RESOURCE		* psResource = psTQContext->apsPDSSecResources[ePDSSec];	/* PRQA S 3689 */ /* appears to be within bounds */

	eError = SGXTQ_CreateResource(psTQContext,
			psResource,
			(IMG_VOID *)&pui32LinAddr,
			bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}

	switch (ePDSSec)
	{
		case SGXTQ_PDSSECFRAG_BASIC:
		{
#if defined(SGX_FEATURE_SECONDARY_REQUIRES_USE_KICK)
			UpdateDOUTUs(BASIC, pui32LinAddr)
#endif
			break;
		}
		case SGXTQ_PDSSECFRAG_DMA_ONLY:
		{
			psResource->uResource.sPDS.ui32Attributes = psPDSValues->ui32DMASize;
			UpdateDOUTUs(DMA_ONLY, pui32LinAddr)
			UpdateDOUTDs(DMA_ONLY, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSSECFRAG_1ATTR:
		{
			UpdateDOUTUs(1ATTR, pui32LinAddr)
			UpdateDOUTA0(1ATTR, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSSECFRAG_3ATTR:
		{
			UpdateDOUTUs(3ATTR, pui32LinAddr)
			UpdateDOUTA0(3ATTR, pui32LinAddr)
			UpdateDOUTA1(3ATTR, pui32LinAddr)
			UpdateDOUTA2(3ATTR, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSSECFRAG_4ATTR:
		{
			UpdateDOUTUs(4ATTR, pui32LinAddr)
			UpdateDOUTA0(4ATTR, pui32LinAddr)
			UpdateDOUTA1(4ATTR, pui32LinAddr)
			UpdateDOUTA2(4ATTR, pui32LinAddr)
			UpdateDOUTA3(4ATTR, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSSECFRAG_4ATTR_DMA:
		{
			psResource->uResource.sPDS.ui32Attributes = psPDSValues->ui32DMASize + 4;
			UpdateDOUTUs(4ATTR_DMA, pui32LinAddr)
			UpdateDOUTDs(4ATTR_DMA, pui32LinAddr)
			UpdateDOUTA0(4ATTR_DMA, pui32LinAddr)
			UpdateDOUTA1(4ATTR_DMA, pui32LinAddr)
			UpdateDOUTA2(4ATTR_DMA, pui32LinAddr)
			UpdateDOUTA3(4ATTR_DMA, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSSECFRAG_5ATTR_DMA:
		{
			psResource->uResource.sPDS.ui32Attributes = psPDSValues->ui32DMASize + 5;
			UpdateDOUTUs(5ATTR_DMA, pui32LinAddr)
			UpdateDOUTDs(5ATTR_DMA, pui32LinAddr)
			UpdateDOUTA0(5ATTR_DMA, pui32LinAddr)
			UpdateDOUTA1(5ATTR_DMA, pui32LinAddr)
			UpdateDOUTA2(5ATTR_DMA, pui32LinAddr)
			UpdateDOUTA3(5ATTR_DMA, pui32LinAddr)
			UpdateDOUTA4(5ATTR_DMA, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSSECFRAG_6ATTR:
		{
			UpdateDOUTUs(6ATTR, pui32LinAddr)
			UpdateDOUTA0(6ATTR, pui32LinAddr)
			UpdateDOUTA1(6ATTR, pui32LinAddr)
			UpdateDOUTA2(6ATTR, pui32LinAddr)
			UpdateDOUTA3(6ATTR, pui32LinAddr)
			UpdateDOUTA4(6ATTR, pui32LinAddr)
			UpdateDOUTA5(6ATTR, pui32LinAddr)
			break;
		}
		case SGXTQ_PDSSECFRAG_7ATTR:
		{
			UpdateDOUTUs(7ATTR, pui32LinAddr)
			UpdateDOUTA0(7ATTR, pui32LinAddr)
			UpdateDOUTA1(7ATTR, pui32LinAddr)
			UpdateDOUTA2(7ATTR, pui32LinAddr)
			UpdateDOUTA3(7ATTR, pui32LinAddr)
			UpdateDOUTA4(7ATTR, pui32LinAddr)
			UpdateDOUTA5(7ATTR, pui32LinAddr)
			UpdateDOUTA6(7ATTR, pui32LinAddr)
			break;
		}
		default:
		{
			return PVRSRV_ERROR_INVALID_PARAMS;
		}
	}
	return PVRSRV_OK;
}



/*****************************************************************************
 * Function Name		:	SGXTQ_CreateUSEResource
 * Inputs				:	TBD
 * Outputs				:	-
 * Description			:
 * Note					:
******************************************************************************/
IMG_INTERNAL PVRSRV_ERROR SGXTQ_CreateUSEResource(SGXTQ_CLIENT_TRANSFER_CONTEXT*	psTQContext,
										SGXTQ_USEFRAGS								eUSEId,
										IMG_UINT32									*aui32USELimms,
										IMG_BOOL									bPDumpContinuous)
{
	PVRSRV_ERROR		eError ;
	IMG_UINT32			* pui32LinAddr = IMG_NULL;
	SGXTQ_RESOURCE		* psResource = psTQContext->apsUSEResources[eUSEId];

	/* Create space in the CB and copy the requested USE resource */
	eError = SGXTQ_CreateResource(psTQContext,
			psResource,
			(IMG_VOID*)&pui32LinAddr,
			bPDumpContinuous);
	if (eError != PVRSRV_OK)
	{
		return eError;
	}
	switch (eUSEId)
	{
#if !defined(SGX_FEATURE_USE_VEC34)
		case SGXTQ_USEBLIT_STRIDE:
		{
			SGXTQ_WriteUSEMovToReg(pui32LinAddr, 2, aui32USELimms[0], EURASIA_USE1_D1STDBANK_TEMP, 0);
			pui32LinAddr += EURASIA_USE_INSTRUCTION_SIZE / sizeof(IMG_UINT32);
			SGXTQ_WriteUSEMovToReg(pui32LinAddr, 3, aui32USELimms[1], EURASIA_USE1_D1STDBANK_TEMP, 0);
			pui32LinAddr += EURASIA_USE_INSTRUCTION_SIZE / sizeof(IMG_UINT32);
			SGXTQ_WriteUSEMovToReg(pui32LinAddr, 4, aui32USELimms[2], EURASIA_USE1_D1STDBANK_TEMP, 0);
			pui32LinAddr += EURASIA_USE_INSTRUCTION_SIZE / sizeof(IMG_UINT32);
			SGXTQ_WriteUSEMovToReg(pui32LinAddr, 6, aui32USELimms[3], EURASIA_USE1_D1STDBANK_TEMP, 0);
			break;
		}
		case SGXTQ_USEBLIT_STRIDE_HIGHBPP:
		{
			pui32LinAddr += EURASIA_USE_INSTRUCTION_SIZE / sizeof(IMG_UINT32);
			SGXTQ_WriteUSEMovToReg(pui32LinAddr, 3, aui32USELimms[1], EURASIA_USE1_D1STDBANK_TEMP, 0);
			pui32LinAddr += EURASIA_USE_INSTRUCTION_SIZE / sizeof(IMG_UINT32);
			SGXTQ_WriteUSEMovToReg(pui32LinAddr, 4, aui32USELimms[2], EURASIA_USE1_D1STDBANK_TEMP, 0);
			pui32LinAddr += EURASIA_USE_INSTRUCTION_SIZE / sizeof(IMG_UINT32);
			SGXTQ_WriteUSEMovToReg(pui32LinAddr, 6, aui32USELimms[3], EURASIA_USE1_D1STDBANK_TEMP, 0);
			pui32LinAddr += EURASIA_USE_INSTRUCTION_SIZE / sizeof(IMG_UINT32);
			SGXTQ_WriteUSEMovToReg(pui32LinAddr, 7, aui32USELimms[4], EURASIA_USE1_D1STDBANK_TEMP, 0);
			pui32LinAddr += EURASIA_USE_INSTRUCTION_SIZE / sizeof(IMG_UINT32);
			SGXTQ_WriteUSEMovToReg(pui32LinAddr, 8, aui32USELimms[5], EURASIA_USE1_D1STDBANK_TEMP, 0);
			break;
		}
#endif
		case SGXTQ_USEBLIT_FILL:
		{
			/* patch the fill blit program with the requested colour */
			IMG_UINT32	ui32PatchOffset = (TQUSE_FILLBLITCOMMAND_OFFSET - TQUSE_FILLBLIT_OFFSET) / sizeof(IMG_UINT32);

			SGXTQ_WriteUSEMovToReg(pui32LinAddr + ui32PatchOffset, 0, aui32USELimms[0], EURASIA_USE1_D1STDBANK_OUTPUT, EURASIA_USE1_END);
			break;
		}
		default: /* stop the compiler moaning*/
			break;
	}

	return PVRSRV_OK;
}

IMG_INTERNAL IMG_VOID SGXTQ_SetupTransferRenderBox(SGXMKIF_TRANSFERCMD *psSubmit,
											IMG_UINT32 x0,
											IMG_UINT32 y0,
											IMG_UINT32 x1,
											IMG_UINT32 y1)
{
	/*
		ISP Render box is specified in number of tiles; round down for left and top, round up for bottom and right.
		The x0,x1,y0,y1 are passed in in pixels
	*/
	psSubmit->sHWRegs.ui32ISPRenderBox1 =   (x0 >> EURASIA_ISPREGION_SHIFTX) << EUR_CR_ISP_RENDBOX1_X_SHIFT
        |   (y0  >> EURASIA_ISPREGION_SHIFTY) << EUR_CR_ISP_RENDBOX1_Y_SHIFT;
    psSubmit->sHWRegs.ui32ISPRenderBox2 =   (((x1  + EURASIA_ISPREGION_SIZEX - 1) >> EURASIA_ISPREGION_SHIFTX) - 1) << EUR_CR_ISP_RENDBOX2_X_SHIFT |
        (((y1 + EURASIA_ISPREGION_SIZEY - 1) >> EURASIA_ISPREGION_SHIFTY) - 1) << EUR_CR_ISP_RENDBOX2_Y_SHIFT;


#if defined(EUR_CR_4Kx4K_RENDER)
	if (x1 > EURASIA_PARAM_VF_X_MAXIMUM || y1 > EURASIA_PARAM_VF_Y_MAXIMUM)
	{
		psSubmit->sHWRegs.ui324KRender	=	EUR_CR_4KX4K_RENDER_MODE_MASK;
	}
	else
	{
		psSubmit->sHWRegs.ui324KRender	=	0;
	}
#endif
}


IMG_INTERNAL PVRSRV_ERROR SGXTQ_SetupTransferClipRenderBox(SGXMKIF_TRANSFERCMD *psSubmit,
															IMG_UINT32 x0,
															IMG_UINT32 y0,
															IMG_UINT32 x1,
															IMG_UINT32 y1,
															IMG_UINT32 ui32DstWidth,
															IMG_UINT32 ui32DstHeight)
{
	/*
		renderbox must be the intersection between the DST rect and surface
	*/
	if(ui32DstWidth == 0 || ui32DstHeight == 0)
		return PVRSRV_ERROR_INVALID_PARAMS;
	

	/* clip right */
	if(x0 > ui32DstWidth)
	{
		x0 = ui32DstWidth;
	}
	if(x1 > ui32DstWidth)
	{
		x1 = ui32DstWidth;
	}

	/* clip bottom */
	if(y0 > ui32DstHeight)
	{
		y0 = ui32DstHeight;
	}
	if(y1 > ui32DstHeight)
	{
		y1 = ui32DstHeight;
	}

	SGXTQ_SetupTransferRenderBox(psSubmit, x0, y0, x1, y1);

	return PVRSRV_OK;
}


/*****************************************************************************
 * Function Name		:	SGXTQ_WritePDSStatesAndDMS
 * Inputs				:	ppui32Stream			- the ISP input stream ptr
 *							psPrimary				- the Primary PDS program
 *							psSecondary				- the Secondary PDS program
 *							bConservativeResUsage	- allocate as small amount of resources
 *													  as possible. Used for custom shaders.
 * Outputs				:	ppui32Stream			- Advanced stream ptr
 * Description			:	Writes the PDS state words and the DMS info into
 *							the ISP stream.
 * Note					:	Modifies (*ppui32Stream)
******************************************************************************/
static INLINE IMG_VOID SGXTQ_WritePDSStatesAndDMS(IMG_UINT32					**ppui32Stream,
									  SGXTQ_CLIENT_TRANSFER_CONTEXT				*psTQContext,
									  SGXTQ_RESOURCE							*psPrimary,
									  SGXTQ_RESOURCE							*psSecondary,
									  IMG_BOOL									bConservativeResUsage)
{
	IMG_UINT32			ui32PDSPrimBase;
	IMG_UINT32			ui32PDSSecBase;
	IMG_UINT32			ui32Tmp;
	IMG_UINT32			ui32NumRegs;
	IMG_UINT32			ui32SecAttrs;

	SGXTQ_PDS_RESOURCE	* psPDSPrim = &psPrimary->uResource.sPDS;
	SGXTQ_PDS_RESOURCE	* psPDSSec = &psSecondary->uResource.sPDS;

	/* PDS base addresses */
#if ! defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
	PVR_ASSERT(psSecondary->sDevVAddr.uiAddr >= psTQContext->sPDSExecBase.uiAddr);
	ui32PDSSecBase = psSecondary->sDevVAddr.uiAddr - psTQContext->sPDSExecBase.uiAddr;
	PVR_ASSERT(psPrimary->sDevVAddr.uiAddr >= psTQContext->sPDSExecBase.uiAddr);
	ui32PDSPrimBase = psPrimary->sDevVAddr.uiAddr - psTQContext->sPDSExecBase.uiAddr;
#else
	PVR_UNREFERENCED_PARAMETER(psTQContext);
	ui32PDSSecBase = psSecondary->sDevVAddr.uiAddr;
	ui32PDSPrimBase = psPrimary->sDevVAddr.uiAddr;
#endif
	CHECKBITSNOTLOST(ui32PDSSecBase, EURASIA_PDS_BASEADD_ALIGNSHIFT);
	CHECKBITSNOTLOST(ui32PDSPrimBase, EURASIA_PDS_BASEADD_ALIGNSHIFT);
#if ! defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
	CHECKOVERFLOW(ui32PDSSecBase >> EURASIA_PDS_BASEADD_ALIGNSHIFT,
			EURASIA_PDS_BASEADD_CLRMSK, EURASIA_PDS_BASEADD_SHIFT);
	CHECKOVERFLOW(ui32PDSPrimBase >> EURASIA_PDS_BASEADD_ALIGNSHIFT,
			EURASIA_PDS_BASEADD_CLRMSK, EURASIA_PDS_BASEADD_SHIFT);
#endif

	 /* Secondary PDS program */
	CHECKBITSNOTLOST(psPDSSec->ui32DataLen, EURASIA_PDS_DATASIZE_ALIGNSHIFT);
	ui32Tmp = psPDSSec->ui32DataLen >> EURASIA_PDS_DATASIZE_ALIGNSHIFT;
	CHECKOVERFLOW(ui32Tmp, EURASIA_PDS_DATASIZE_CLRMSK, EURASIA_PDS_DATASIZE_SHIFT);
	*(*ppui32Stream)++ = (((ui32PDSSecBase >> EURASIA_PDS_BASEADD_ALIGNSHIFT)			<<
							EURASIA_PDS_BASEADD_SHIFT) & ~EURASIA_PDS_BASEADD_CLRMSK)	|
						((ui32Tmp << EURASIA_PDS_DATASIZE_SHIFT) & ~EURASIA_PDS_DATASIZE_CLRMSK);

	/* PDS data */
#if defined (SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
	ui32NumRegs = ALIGNFIELD(psPDSPrim->ui32TempRegs + psPDSPrim->ui32Attributes, EURASIA_PDS_PIXELSIZE_ALIGNSHIFT);
#else
	ui32NumRegs = ALIGNFIELD(psPDSPrim->ui32Attributes, EURASIA_PDS_PIXELSIZE_ALIGNSHIFT);
#endif
	CHECKOVERFLOW(ui32NumRegs, EURASIA_PDS_PIXELSIZE_CLRMSK, EURASIA_PDS_PIXELSIZE_SHIFT);
	ui32SecAttrs = ALIGNFIELD(psPDSSec->ui32Attributes, (EURASIA_PDS_SECATTRSIZE_ALIGNSHIFT - 2));
	CHECKOVERFLOW(ui32SecAttrs, EURASIA_PDS_SECATTRSIZE_CLRMSK, EURASIA_PDS_SECATTRSIZE_SHIFT);

	if (bConservativeResUsage)
	{
#if defined (SGX_FEATURE_UNIFIED_TEMPS_AND_PAS)
		**ppui32Stream =  (4UL << EURASIA_PDS_PDSTASKSIZE_SHIFT) |
                          (0UL  << EURASIA_PDS_USETASKSIZE_SHIFT);
#else
		**ppui32Stream =  ((EURASIA_PDS_PDSTASKSIZE_MAX << EURASIA_PDS_PDSTASKSIZE_SHIFT) & ~ EURASIA_PDS_PDSTASKSIZE_CLRMSK) |
                          (EURASIA_PDS_USETASKSIZE_MAXSIZE << EURASIA_PDS_USETASKSIZE_SHIFT);
#endif
	}
	else
	{
		**ppui32Stream = 	((EURASIA_PDS_PDSTASKSIZE_MAX << EURASIA_PDS_PDSTASKSIZE_SHIFT) & ~ EURASIA_PDS_PDSTASKSIZE_CLRMSK)	|
							(EURASIA_PDS_USETASKSIZE_MAXSIZE << EURASIA_PDS_USETASKSIZE_SHIFT);
	}
	*(*ppui32Stream)++ |=
#if defined(SGX_FEATURE_PIXEL_PDSADDR_FULL_RANGE)
						(((ui32PDSSecBase & EURASIA_PDS_BASEADD_MSB) != 0) ? EURASIA_PDS_BASEADD_SEC_MSB : 0)					|
						(((ui32PDSPrimBase & EURASIA_PDS_BASEADD_MSB) != 0) ? EURASIA_PDS_BASEADD_PRIM_MSB : 0)					|
#else
						((ui32SecAttrs > 0) ? EURASIA_PDS_USE_SEC_EXEC : 0)														|
#endif
						(ui32SecAttrs << EURASIA_PDS_SECATTRSIZE_SHIFT)															|
						((ui32NumRegs << EURASIA_PDS_PIXELSIZE_SHIFT) & ~EURASIA_PDS_PIXELSIZE_CLRMSK);


	/* Primary PDS programs */
	CHECKBITSNOTLOST(psPDSPrim->ui32DataLen, EURASIA_PDS_DATASIZE_ALIGNSHIFT);
	ui32Tmp = psPDSPrim->ui32DataLen >> EURASIA_PDS_DATASIZE_ALIGNSHIFT;
	CHECKOVERFLOW(ui32Tmp, EURASIA_PDS_DATASIZE_CLRMSK, EURASIA_PDS_DATASIZE_SHIFT);
	*(*ppui32Stream)++ = (((ui32PDSPrimBase >> EURASIA_PDS_BASEADD_ALIGNSHIFT)			<<
							EURASIA_PDS_BASEADD_SHIFT) & ~EURASIA_PDS_BASEADD_CLRMSK)	|
						((ui32Tmp << EURASIA_PDS_DATASIZE_SHIFT) & ~EURASIA_PDS_DATASIZE_CLRMSK);


}


/*****************************************************************************
 * Function Name		:	SGXTQ_CreateISPResource
 * Inputs				:	psTQContext				- The context
 *							psPrimary				- PDS primary program
 *							psSecondary				- PDS seconday program
 *							psDstRect				- ISP geometry
 *							psTSPCoords				- TSP courdinate sets
 *							bConservativeResUsage	- allocate as small amount of resources
 *													  as possible. Used for custom shaders.
 *							bInvertTriangle			- reverse 1st source triangle vertex ordering
 *													  from ABC to ACB			
 *							ui32NumLayers			- the number of input texture layers
 * Outputs				:
 * Description			:	Creates the ISP input stream.
******************************************************************************/
IMG_INTERNAL PVRSRV_ERROR SGXTQ_CreateISPResource(SGXTQ_CLIENT_TRANSFER_CONTEXT	*psTQContext,
									  SGXTQ_RESOURCE							*psPrimary,
									  SGXTQ_RESOURCE							*psSecondary,
									  IMG_RECT									*psDstRect,
									  SGXTQ_TSP_COORDS							*psTSPCoords,
									  IMG_BOOL									bConservativeResUsage,
									  IMG_BOOL									bInvertTriangle,
									  IMG_UINT32								ui32NumLayers,
									  IMG_BOOL									bPDumpContinuous,
									  IMG_UINT32								ui32Flags)
{
	SGXTQ_RESOURCE	* psResource;
	IMG_UINT32		ui32Size;
	IMG_UINT32		ui32SizeInBytes;
	IMG_UINT32		ui32Top;
	IMG_UINT32		ui32Bottom;
	IMG_UINT32		ui32Left;
	IMG_UINT32		ui32Right;
	SGXTQ_CB		* psCB = psTQContext->psStreamCB;
	IMG_UINT32		* pui32CtrlStream;
#if defined(PVRSRV_NEED_PVR_ASSERT)
	IMG_UINT32		* pui32StreamBase;
#endif
	IMG_UINT32		Layer0_Vertex0_U;
	IMG_UINT32		Layer0_Vertex0_V;
	IMG_UINT32		Layer0_Vertex1_U;
	IMG_UINT32		Layer0_Vertex1_V;
	IMG_UINT32		Layer0_Vertex2_U;
	IMG_UINT32		Layer0_Vertex2_V;

	PVR_ASSERT(ui32NumLayers < SGXTQ_NUM_HWBGOBJS);
	/* PRQA S 3689 1 */ /* PVR_ASSERT() should catch bounds overflow */
	psResource = psTQContext->apsISPResources[ui32NumLayers];

	ui32Size = (11 + 6 * ui32NumLayers);
#if defined(SGX545)
	ui32Size += 6;
#endif
	ui32SizeInBytes = ui32Size * sizeof(IMG_UINT32);

	if (! SGXTQ_AcquireCB(psTQContext->psFenceIDMemInfo,
				psTQContext->ui32FenceID,
				psTQContext->hOSEvent,
				psCB,
				ui32SizeInBytes,
				IMG_TRUE,
				(IMG_VOID **) &pui32CtrlStream,
				& psResource->sDevVAddr.uiAddr,
				bPDumpContinuous))
	{
		return PVRSRV_ERROR_TIMEOUT;
	}

#if defined(PVRSRV_NEED_PVR_ASSERT)
	pui32StreamBase = pui32CtrlStream;
#endif

	SGXTQ_WritePDSStatesAndDMS(&pui32CtrlStream,
			psTQContext,
			psPrimary,
			psSecondary,
			bConservativeResUsage);

	// Apply front-end rotation (required for rotated alpha blend)
	if ((ui32Flags & (SGX_TRANSFER_LAYER_0_ROT_90|SGX_TRANSFER_LAYER_0_ROT_180|SGX_TRANSFER_LAYER_0_ROT_270))!=0)
	{
		if ((ui32Flags & SGX_TRANSFER_LAYER_0_ROT_90)!=0)
		{
			// rotate 90
			Layer0_Vertex0_U = psTSPCoords->ui32Src0U0;
			Layer0_Vertex0_V = psTSPCoords->ui32Src0V1;
			Layer0_Vertex1_U = psTSPCoords->ui32Src0U0;
			Layer0_Vertex1_V = psTSPCoords->ui32Src0V0;
			Layer0_Vertex2_U = psTSPCoords->ui32Src0U1;
			Layer0_Vertex2_V = psTSPCoords->ui32Src0V1;
		}
		else if ((ui32Flags & SGX_TRANSFER_LAYER_0_ROT_180)!=0)
		{
			// rotate 180
			Layer0_Vertex0_U = psTSPCoords->ui32Src0U1;
			Layer0_Vertex0_V = psTSPCoords->ui32Src0V1;
			Layer0_Vertex1_U = psTSPCoords->ui32Src0U0;
			Layer0_Vertex1_V = psTSPCoords->ui32Src0V1;
			Layer0_Vertex2_U = psTSPCoords->ui32Src0U1;
			Layer0_Vertex2_V = psTSPCoords->ui32Src0V0;
		}
		else
		{
			// rotate 270
			Layer0_Vertex0_U = psTSPCoords->ui32Src0U1;
			Layer0_Vertex0_V = psTSPCoords->ui32Src0V0;
			Layer0_Vertex1_U = psTSPCoords->ui32Src0U1;
			Layer0_Vertex1_V = psTSPCoords->ui32Src0V1;
			Layer0_Vertex2_U = psTSPCoords->ui32Src0U0;
			Layer0_Vertex2_V = psTSPCoords->ui32Src0V0;
		}
	}
	else
	{
		// unrotated
		Layer0_Vertex0_U = psTSPCoords->ui32Src0U0;
		Layer0_Vertex0_V = psTSPCoords->ui32Src0V0;
		Layer0_Vertex1_U = psTSPCoords->ui32Src0U1;
		Layer0_Vertex1_V = psTSPCoords->ui32Src0V0;
		Layer0_Vertex2_U = psTSPCoords->ui32Src0U0;
		Layer0_Vertex2_V = psTSPCoords->ui32Src0V1;
	}

	/* UV pairs for 3 vertices per layer*/
    if (ui32NumLayers > 0)
	{
		/* vertex 0 - U0 */
	    *pui32CtrlStream++ = Layer0_Vertex0_U;
	    /* vertex 0 - V0 */
	    *pui32CtrlStream++ = Layer0_Vertex0_V;
	}
	if (ui32NumLayers > 1)
	{
		/* vertex 0 - U1 */
	    *pui32CtrlStream++ = psTSPCoords->ui32Src1U0;
	    /* vertex 0 - V1 */
	    *pui32CtrlStream++ = psTSPCoords->ui32Src1V0;
	}
	if (ui32NumLayers > 2)
	{
		/* vertex 0 - U2 */
	    *pui32CtrlStream++ = psTSPCoords->ui32Src2U0;
	    /* vertex 0 - V2 */
	    *pui32CtrlStream++ = psTSPCoords->ui32Src2V0;
	}
	if (ui32NumLayers > 0)
	{
		if (bInvertTriangle)
		{
		    /* vertex 1 - U0 */
		    *pui32CtrlStream++ = psTSPCoords->ui32Src0U0;
		    /* vertex 1 - V0 */
		    *pui32CtrlStream++ = psTSPCoords->ui32Src0V1;
		}
		else
		{
			/* vertex 1 - U0 */
			*pui32CtrlStream++ = Layer0_Vertex1_U;
			/* vertex 1 - V0 */
			*pui32CtrlStream++ = Layer0_Vertex1_V;
		}
	}
	if (ui32NumLayers > 1)
	{
		/* vertex 1 - U1 */
		*pui32CtrlStream++ = psTSPCoords->ui32Src1U1;
		/* vertex 1 - V1 */
		*pui32CtrlStream++ = psTSPCoords->ui32Src1V0;
	}
	if (ui32NumLayers > 2)
	{
		/* vertex 1 - U2 */
		*pui32CtrlStream++ = psTSPCoords->ui32Src2U1;
		/* vertex 1 - V2 */
		*pui32CtrlStream++ = psTSPCoords->ui32Src2V0;
	}
	if (ui32NumLayers > 0)
	{
		if (bInvertTriangle)
		{
			/* vertex 2 - U0 */
			*pui32CtrlStream++ = psTSPCoords->ui32Src0U1;
			/* vertex 2 - V0 */
			*pui32CtrlStream++ = psTSPCoords->ui32Src0V0;
		}
		else
		{
			/* vertex 2 - U0 */
			*pui32CtrlStream++ = Layer0_Vertex2_U;
			/* vertex 2 - V0 */
			*pui32CtrlStream++ = Layer0_Vertex2_V;
		}
	}
	if (ui32NumLayers > 1)
	{
		/* vertex 2 - U1 */
		*pui32CtrlStream++ = psTSPCoords->ui32Src1U0;
		/* vertex 2 - V1 */
		*pui32CtrlStream++ = psTSPCoords->ui32Src1V1;
	}
	if (ui32NumLayers > 2)
	{
		/* vertex 2 - U2 */
		*pui32CtrlStream++ = psTSPCoords->ui32Src2U0;
		/* vertex 2 - V2 */
		*pui32CtrlStream++ = psTSPCoords->ui32Src2V1;
	}

#if defined(SGX545)
	/* Vtx format */
	*pui32CtrlStream++ = 0;
	/* TSP format */
	*pui32CtrlStream++	=	((2 * ui32NumLayers) << EURASIA_PARAM_VF_TSP_SIZE_SHIFT)
						|	((2 * ui32NumLayers) << EURASIA_PARAM_VF_TSP_CLIP_SIZE_SHIFT);
	/* Texcoord16 format word */
	*pui32CtrlStream++	= 0;
	/* Texcoord1D format word */
	*pui32CtrlStream++	= 0;
	/* Texcoord S present word */
	*pui32CtrlStream++	= 0;
	/* Texcoord T present word */
	*pui32CtrlStream++	= 0;
#else
	/* Vtx format */
	*pui32CtrlStream++ = (2 * ui32NumLayers) << EURASIA_PARAM_VF_TSP_SIZE_SHIFT;
	/* TSP format */
	*pui32CtrlStream++ = 0;
#endif

	/* ISP vertex data (X, Y, Z) */

	/* render normally goes -1k -> (3k - 1) : 1k bias with 4 bit fractional 12 bit integer.
	 * if 4k_x_4k is set we can go from -1k -> (7k - 1) - hence the name 4k.
	 * (bias unaffected - 13 bit int, 3 bit fract)
	 */ 
#if defined(EUR_CR_4Kx4K_RENDER)
	if (psDstRect->x1 > EURASIA_PARAM_VF_X_MAXIMUM || psDstRect->y1 > EURASIA_PARAM_VF_Y_MAXIMUM)
	{
		ui32Top		= ((IMG_UINT32)(psDstRect->y0 + 2 * EURASIA_PARAM_VF_Y_OFFSET) << (EURASIA_PARAM_VF_Y_FRAC - 1));
		ui32Bottom	= ((IMG_UINT32)(psDstRect->y1 + 2 * EURASIA_PARAM_VF_Y_OFFSET) << (EURASIA_PARAM_VF_Y_FRAC - 1));
		ui32Left	= ((IMG_UINT32)(psDstRect->x0 + 2 * EURASIA_PARAM_VF_X_OFFSET) << (EURASIA_PARAM_VF_X_FRAC - 1));
		ui32Right	= ((IMG_UINT32)(psDstRect->x1 + 2 * EURASIA_PARAM_VF_X_OFFSET) << (EURASIA_PARAM_VF_X_FRAC - 1));
	}
	else
#endif
	{
		ui32Top		= ((IMG_UINT32)(psDstRect->y0 + EURASIA_PARAM_VF_Y_OFFSET) << EURASIA_PARAM_VF_Y_FRAC);
		ui32Bottom	= ((IMG_UINT32)(psDstRect->y1 + EURASIA_PARAM_VF_Y_OFFSET) << EURASIA_PARAM_VF_Y_FRAC);
		ui32Left	= ((IMG_UINT32)(psDstRect->x0 + EURASIA_PARAM_VF_X_OFFSET) << EURASIA_PARAM_VF_X_FRAC);
		ui32Right	= ((IMG_UINT32)(psDstRect->x1 + EURASIA_PARAM_VF_X_OFFSET) << EURASIA_PARAM_VF_X_FRAC);
	}

#if defined(SGX545)
 	/* X0Y0 (7..0 of X0, 23..0 of Y0) */
	*pui32CtrlStream++ = (ui32Left << EURASIA_PARAM_VF_X_EVEN0_SHIFT) |
						 ((ui32Top << EURASIA_PARAM_VF_Y_EVEN0_SHIFT) & 0x00ffffff);

	/* Z0X0 - (15..0 of Z0 is 0, 31..16 of X0) */
	*pui32CtrlStream++ = ((ui32Left >> EURASIA_PARAM_VF_X_EVEN1_ALIGNSHIFT) << EURASIA_PARAM_VF_X_EVEN1_SHIFT);

	/* Y1Z0 - (15..0 of Y1, 31..16 of Z0) */
	*pui32CtrlStream++ = (ui32Top << EURASIA_PARAM_VF_Y_ODD0_SHIFT);

	/* X1Y1 - (15..0 of X1, 31..16 of Y1) */
	*pui32CtrlStream++ = (ui32Right << EURASIA_PARAM_VF_X_ODD1_SHIFT) |
						 (((ui32Top >> EURASIA_PARAM_VF_Y_ODD1_ALIGNSHIFT) << EURASIA_PARAM_VF_Y_ODD1_SHIFT) & 0xff);

	/* Z1 - (31..0 of Z1) - static !*/
	*pui32CtrlStream++ = 0;

	/* X2Y2 - (7..0 of X2, 23..0 of Y2) */
	*pui32CtrlStream++ = (ui32Left << EURASIA_PARAM_VF_X_EVEN0_SHIFT) |
						 ((ui32Bottom << EURASIA_PARAM_VF_Y_EVEN0_SHIFT) & 0xffffff);

	/* Z2X2 - Replaced */
	*pui32CtrlStream++ = ((ui32Left >> EURASIA_PARAM_VF_X_EVEN1_ALIGNSHIFT) << EURASIA_PARAM_VF_X_EVEN1_SHIFT);

	/* --Z2 - Replaced static !*/
	*pui32CtrlStream++ = 0;		/* PRQA S 3199 */ /* keep incrementanyway */
#else
    /* X0Y0 */
    *pui32CtrlStream++ = (ui32Left << EURASIA_PARAM_VF_X_SHIFT) | (ui32Top << EURASIA_PARAM_VF_Y_SHIFT);

	/* Z0 - static! */
	*pui32CtrlStream++ = FLOAT32_ONE;

	/* X1Y1 */
	*pui32CtrlStream++ = (ui32Right << EURASIA_PARAM_VF_X_SHIFT) | (ui32Top << EURASIA_PARAM_VF_Y_SHIFT);

	/* Z1 - static */
	*pui32CtrlStream++ = FLOAT32_ONE;

	/* X2Y2 */
	*pui32CtrlStream++ = (ui32Left << EURASIA_PARAM_VF_X_SHIFT) | (ui32Bottom << EURASIA_PARAM_VF_Y_SHIFT);

	/* Z3 - static */
	*pui32CtrlStream++ = FLOAT32_ONE;		/* PRQA S 3199 */ /* keep incrementanyway */
#endif

	PVR_ASSERT(pui32CtrlStream - pui32StreamBase  == (IMG_INT32)ui32Size);

	return PVRSRV_OK;
}


IMG_INTERNAL PVRSRV_ERROR SGXTQ_CreateISPF2DResource(SGXTQ_CLIENT_TRANSFER_CONTEXT	*psTQContext,
													 SGXTQ_RESOURCE					*psPrimary,
													 SGXTQ_RESOURCE					*psSecondary,
													 IMG_RECT						*psDstRect,
													 SGXTQ_TSP_SINGLE				*psTSPCoords,
													 IMG_UINT32                     ui32NumRects,
													 IMG_BOOL						bTranslucent,
													 IMG_BOOL						bPDumpContinuous)
{
	SGXTQ_RESOURCE	* psResource = psTQContext->psFast2DISPControlStream;
	SGXTQ_CB		* psCB = psTQContext->psStreamCB;
	IMG_UINT32		ui32SizeInBytes;
	IMG_PUINT32		pui32CtrlStream;
	IMG_UINT32		ui32Top;
	IMG_UINT32		ui32Bottom;
	IMG_UINT32		ui32Left;
	IMG_UINT32		ui32Right;
	IMG_UINT32		ui32NumCtrlStreams;
	IMG_UINT32      ui32NumPrimBlocks;
	IMG_UINT32      ui32NumRectsInPrimBlock; 
	IMG_UINT32      ui32NumPrimsInPrimBlock;
	IMG_UINT32      ui32NumVertsInPrimBlock;
	IMG_UINT32		ui32ISPAPtr;
	IMG_UINT32		ui32CtrlStreamBase;
	IMG_UINT32		aui32PDSAndDMS[3];
#if defined(PVRSRV_NEED_PVR_ASSERT)
	IMG_PUINT32		pui32StreamBase;
#endif
	IMG_UINT32		*pui32Tmp;
	IMG_UINT32		i;

    /*    +-----------+  ---
     *    | HDR       |   ^
     *    | VTXPTR    +-+ |
     *    | MSK       | | |
     *    +-----------+ | |  max 16 dwords or 32 on 545
     *    | HDR       | | |
     *    | VTXPTR    | | |
     *    | MSK       | | |
     *    +-----------+ | |
     *    | ...       | | |
     *    +-----------+ | |
     *  +-+ LINKPTR   | | v
     *  | +-----------+ |---
     *  +>| HDR       | |
     *    | VTXPTR    | |
     *    | MSK       | |
     *    +-----------+ |
     *    | TERMINATE | |
     *    +-----------+ |
     *    |           | | points to ISPA
     *    |PRIMBLOCK0 |<+
     *    +-----------+
	 */

	if (ui32NumRects == 0)
	{
		return PVRSRV_ERROR_INVALID_PARAMS;
	}

#if defined(SGX545)
#define SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK			8
#define SGXTQ_MAX_ISP_PRIM_BLOCKS_IN_CTRL_STREAM	10
#define SGXTQ_CTRL_STREAM_BLOCK_SIZE_IN_BYTES		(4 * 32)
#define SGXTQ_PRIM_BLOCK_RECTANGLE_SIZE_IN_BYTES	(4 * 19)
#define SGXTQ_PRIM_BLOCK_SIZE_IN_BYTES				(SGXTQ_PRIM_BLOCK_RECTANGLE_SIZE_IN_BYTES * SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK + 11 * 4)
#else
#define SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK			4
#define SGXTQ_MAX_ISP_PRIM_BLOCKS_IN_CTRL_STREAM	5
#define SGXTQ_CTRL_STREAM_BLOCK_SIZE_IN_BYTES		(4 * 16)
#define SGXTQ_PRIM_BLOCK_RECTANGLE_SIZE_IN_BYTES	(4 * 17)
#define SGXTQ_PRIM_BLOCK_SIZE_IN_BYTES				(SGXTQ_PRIM_BLOCK_RECTANGLE_SIZE_IN_BYTES * SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK + 7 * 4)
#endif

	ui32NumPrimBlocks  = (ui32NumRects + SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK - 1) / SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK;
	ui32NumCtrlStreams = (ui32NumPrimBlocks + SGXTQ_MAX_ISP_PRIM_BLOCKS_IN_CTRL_STREAM - 1) / SGXTQ_MAX_ISP_PRIM_BLOCKS_IN_CTRL_STREAM; 

	/* space we use up for the ctrl streams*/
	ui32SizeInBytes = ui32NumCtrlStreams * SGXTQ_CTRL_STREAM_BLOCK_SIZE_IN_BYTES;
	/* and the space we use up for the primitive blks*/
	ui32SizeInBytes += ui32NumPrimBlocks * SGXTQ_PRIM_BLOCK_SIZE_IN_BYTES;
	/* how many primitives are not present in the last stream ?*/
	ui32SizeInBytes -= ((SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK - (ui32NumRects & (SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK - 1))) &
			(SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK - 1)) * SGXTQ_PRIM_BLOCK_RECTANGLE_SIZE_IN_BYTES;

	if (! SGXTQ_AcquireCB(psTQContext->psFenceIDMemInfo,
				psTQContext->ui32FenceID,
				psTQContext->hOSEvent,
				psCB,
				ui32SizeInBytes,
				IMG_TRUE,
				(IMG_VOID **) &pui32CtrlStream,
				& psResource->sDevVAddr.uiAddr,
				bPDumpContinuous))
	{
		return PVRSRV_ERROR_TIMEOUT;
	}

#if defined(PVRSRV_NEED_PVR_ASSERT)
	pui32StreamBase = pui32CtrlStream;
#endif

#if defined(SGX_FEATURE_MP)
	* pui32CtrlStream++ = EURASIA_PARAM_OBJTYPE_PIM;
#endif

	/* get numbers for the first primitive block*/
	ui32NumRectsInPrimBlock = ui32NumPrimBlocks > 1 ? SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK : ui32NumRects;
	ui32NumPrimsInPrimBlock = ui32NumRectsInPrimBlock << 1;
	ui32NumVertsInPrimBlock = ui32NumRectsInPrimBlock << 2;

	/* get the ISPA ptr for the first Primitive block*/
	ui32ISPAPtr = psResource->sDevVAddr.uiAddr - psTQContext->sISPStreamBase.uiAddr +
		ui32NumCtrlStreams * SGXTQ_CTRL_STREAM_BLOCK_SIZE_IN_BYTES + 12 + 32 * ui32NumRectsInPrimBlock; 
	/* check the alignment up front*/
	CHECKBITSNOTLOST(ui32ISPAPtr, EURASIA_PARAM_PB1_VERTEXPTR_ALIGNSHIFT);
	/* keep compilers happy by avoiding constant asserts*/
	CHECKBITSNOTLOST(ui32ISPAPtr + SGXTQ_PRIM_BLOCK_SIZE_IN_BYTES, EURASIA_PARAM_PB1_VERTEXPTR_ALIGNSHIFT);

	/* keep track of the next ctrl stream base*/
	ui32CtrlStreamBase = psResource->sDevVAddr.uiAddr - psTQContext->sISPStreamBase.uiAddr; 
	
	for (i = 0; i < ui32NumPrimBlocks; i++)
	{

		/* HDR */
		*pui32CtrlStream++ = EURASIA_PARAM_OBJTYPE_PRIMBLOCK
			| (ui32NumVertsInPrimBlock - 1U) << EURASIA_PARAM_PB0_VERTEXCOUNT_SHIFT
			| (2UL << EURASIA_PARAM_PB0_ISPSTATESIZE_SHIFT)
#if defined(EURASIA_PARAM_PB0_READISPSTATE)
			| EURASIA_PARAM_PB0_READISPSTATE
#endif
#if defined(SGX545)
			| EURASIA_PARAM_PB0_MASKCTRL_MASKPRES << EURASIA_PARAM_PB0_MASKCTRL_SHIFT
			| PRMSTART_MODE << EURASIA_PARAM_PB0_PRMSTART_SHIFT
#else
			| EURASIA_PARAM_PB0_PRIMMASKPRES
#endif
			| ((1UL << ui32NumVertsInPrimBlock) - 1U) << EURASIA_PARAM_PB0_MASKBYTE0_SHIFT;


		/* VTXPTR*/
		*pui32CtrlStream++ = ((ui32NumPrimsInPrimBlock - 1) << EURASIA_PARAM_PB1_PRIMCOUNT_SHIFT)
#if defined(EURASIA_PARAM_PB1_READISPSTATE)
			| EURASIA_PARAM_PB1_READISPSTATE
#endif
			| (((ui32ISPAPtr >> EURASIA_PARAM_PB1_VERTEXPTR_ALIGNSHIFT) << EURASIA_PARAM_PB1_VERTEXPTR_SHIFT) & ~EURASIA_PARAM_PB1_VERTEXPTR_CLRMSK);
		ui32ISPAPtr += SGXTQ_PRIM_BLOCK_SIZE_IN_BYTES;

		/* MSK */
#if defined(SGX545)
		*pui32CtrlStream++ =
			((((1UL << ui32NumPrimsInPrimBlock) - 1U) << EURASIA_PARAM_PB2_PRIMMASKL_SHIFT) & ~EURASIA_PARAM_PB2_PRIMMASKL_CLRMSK)
			| ((((1UL << ui32NumPrimsInPrimBlock) - 1U) << EURASIA_PARAM_PB2_PRIMMASKR_SHIFT) & ~EURASIA_PARAM_PB2_PRIMMASKR_CLRMSK);
#else
		*pui32CtrlStream++ =
			(((1UL << ui32NumPrimsInPrimBlock) - 1U) << EURASIA_PARAM_PB2_PRIMMASK_SHIFT) & ~EURASIA_PARAM_PB2_PRIMMASK_CLRMSK;
#endif

		if (i + 1 == ui32NumPrimBlocks)
		{
			/* terminate */
			*pui32CtrlStream = EURASIA_PARAM_OBJTYPE_STREAMTERM;
			pui32CtrlStream = (IMG_PUINT32)(((IMG_SIZE_T)pui32CtrlStream +
						(SGXTQ_CTRL_STREAM_BLOCK_SIZE_IN_BYTES - 1)) & ~ (SGXTQ_CTRL_STREAM_BLOCK_SIZE_IN_BYTES - 1));
		}
		else if (i % SGXTQ_MAX_ISP_PRIM_BLOCKS_IN_CTRL_STREAM == SGXTQ_MAX_ISP_PRIM_BLOCKS_IN_CTRL_STREAM - 1)
		{
			ui32CtrlStreamBase += SGXTQ_CTRL_STREAM_BLOCK_SIZE_IN_BYTES;
			/* link ptr */
			*pui32CtrlStream = EURASIA_PARAM_OBJTYPE_STREAMLINK
				| (((ui32CtrlStreamBase >> EURASIA_PARAM_STREAMLINK_ALIGNSHIFT) << EURASIA_PARAM_STREAMLINK_SHIFT) & ~ EURASIA_PARAM_STREAMLINK_CLRMSK);

			pui32CtrlStream = (IMG_PUINT32)(((IMG_SIZE_T)pui32CtrlStream +
						(SGXTQ_CTRL_STREAM_BLOCK_SIZE_IN_BYTES - 1)) & ~ (SGXTQ_CTRL_STREAM_BLOCK_SIZE_IN_BYTES - 1));
		}

		if (i + 2 == ui32NumPrimBlocks)
		{
			/* get numbers for the last primitive block*/
			ui32NumRectsInPrimBlock = ui32NumRects & (SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK - 1);
			if (ui32NumRectsInPrimBlock == 0)
			{
				ui32NumRectsInPrimBlock = SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK;
			}
			ui32NumPrimsInPrimBlock = ui32NumRectsInPrimBlock << 1;
			ui32NumVertsInPrimBlock = ui32NumRectsInPrimBlock << 2;

			ui32ISPAPtr -= (SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK - ui32NumRectsInPrimBlock) * 32;
		}

	}
		

	pui32Tmp = & aui32PDSAndDMS[0];
	SGXTQ_WritePDSStatesAndDMS(& pui32Tmp, psTQContext, psPrimary, psSecondary, IMG_FALSE);

	/* get numbers for the primitive blocks*/
	ui32NumRectsInPrimBlock = SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK;

	for (i = 0; i < ui32NumPrimBlocks; i++)
	{
		IMG_UINT32 j;

		/* shrink on the last one */
		if (i + 1 == ui32NumPrimBlocks)
		{
			ui32NumRectsInPrimBlock = ui32NumRects & (SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK - 1);
			if (ui32NumRectsInPrimBlock == 0)
			{
				ui32NumRectsInPrimBlock = SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK;
			}
		}

		for (j = 0; j < 3; j++)
		{
			*pui32CtrlStream++ = aui32PDSAndDMS[j];
		}

		for (j = 0; j < ui32NumRectsInPrimBlock; j++)
		{
			IMG_UINT32 ui32RectIndex = i * SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK + j;

			*pui32CtrlStream++ = psTSPCoords[ui32RectIndex].ui32Src0U0;
			*pui32CtrlStream++ = psTSPCoords[ui32RectIndex].ui32Src0V0;

			*pui32CtrlStream++ = psTSPCoords[ui32RectIndex].ui32Src0U0;
			*pui32CtrlStream++ = psTSPCoords[ui32RectIndex].ui32Src0V1;

			*pui32CtrlStream++ = psTSPCoords[ui32RectIndex].ui32Src0U1;
			*pui32CtrlStream++ = psTSPCoords[ui32RectIndex].ui32Src0V1;

			*pui32CtrlStream++ = psTSPCoords[ui32RectIndex].ui32Src0U1;
			*pui32CtrlStream++ = psTSPCoords[ui32RectIndex].ui32Src0V0;
		}
		/* ISP state word A/B
		 * index data
		 * vertex fmt word
		 * tsp data format word
		 */
		/* ISP state word A */
		/* CONST */
		*pui32CtrlStream++ = (bTranslucent ? EURASIA_ISPA_PASSTYPE_TRANS : EURASIA_ISPA_PASSTYPE_OPAQUE)
			| EURASIA_ISPA_DCMPMODE_ALWAYS
			| EURASIA_ISPA_LINEFILLLASTPIXEL
			| EURASIA_ISPA_BPRES
			| EURASIA_ISPA_OBJTYPE_TRI;
	
		/* ISP B*/
		/* CONST */
		*pui32CtrlStream++ = 0;

		/* Index data
		 * NOTE: For Eurasia variants with more than 32 primitives in a primitive block,
		 * only one triangle can be packed into a dword.
		 */
		for (j = 0; j < ui32NumRectsInPrimBlock; j++)
		{

			*pui32CtrlStream++ = EURASIA_PARAM_EDGEFLAG1CA
				| (0U + 4 * j) << EURASIA_PARAM_INDEX1C_SHIFT
				| EURASIA_PARAM_EDGEFLAG1BC
				| (3U + 4 * j) << EURASIA_PARAM_INDEX1B_SHIFT
				| EURASIA_PARAM_EDGEFLAG1AB
				| (1U + 4 * j) << EURASIA_PARAM_INDEX1A_SHIFT
				| EURASIA_PARAM_EDGEFLAG0CA
				| (1U + 4 * j) << EURASIA_PARAM_INDEX0C_SHIFT
				| EURASIA_PARAM_EDGEFLAG0BC
				| (2U + 4 * j) << EURASIA_PARAM_INDEX0B_SHIFT
				| EURASIA_PARAM_EDGEFLAG0AB
				| (3U + 4 * j) << EURASIA_PARAM_INDEX0A_SHIFT;
		}
	
#if defined(SGX545)
		/* Vertex format word */
		/* CONST */
		*pui32CtrlStream++ = 0;

		/* on muse here is the place for MSAA dword, point pitch is also skipped*/

		/* TSP data format word */
		/* CONST */
		*pui32CtrlStream++ = 2UL << EURASIA_PARAM_VF_TSP_SIZE_SHIFT	|
							 2UL << EURASIA_PARAM_VF_TSP_CLIP_SIZE_SHIFT;
	
		/* Texcoord16 format word */
		/* CONST */
		*pui32CtrlStream++	= 0;
		/* Texcoord1D format word */
		/* CONST */
		*pui32CtrlStream++	= 0;
		/* Texcoord S present word */
		/* CONST */
		*pui32CtrlStream++	= 0;
		/* Texcoord T present word */
		/* CONST */
		*pui32CtrlStream++	= 0;
#else
		/* Vertex format word */
		/* CONST */
		*pui32CtrlStream++ = 2UL << EURASIA_PARAM_VF_TSP_SIZE_SHIFT;

		/* TSP data format word */
		/* CONST */
		*pui32CtrlStream++ = 0;
#endif

		for (j = 0; j < ui32NumRectsInPrimBlock; j++)
		{
			IMG_UINT32 ui32RectIndex = i * SGXTQ_MAX_ISP_RECTS_IN_PRIM_BLOCK + j;

#if defined(EUR_CR_4Kx4K_RENDER)
			if (psDstRect->x1 > EURASIA_PARAM_VF_X_MAXIMUM || psDstRect->y1 > EURASIA_PARAM_VF_Y_MAXIMUM)
			{
				ui32Top		= ((IMG_UINT32)(psDstRect[ui32RectIndex].y0 + 2 * EURASIA_PARAM_VF_Y_OFFSET) << (EURASIA_PARAM_VF_Y_FRAC - 1));
				ui32Bottom	= ((IMG_UINT32)(psDstRect[ui32RectIndex].y1 + 2 * EURASIA_PARAM_VF_Y_OFFSET) << (EURASIA_PARAM_VF_Y_FRAC - 1));
				ui32Left	= ((IMG_UINT32)(psDstRect[ui32RectIndex].x0 + 2 * EURASIA_PARAM_VF_X_OFFSET) << (EURASIA_PARAM_VF_X_FRAC - 1));
				ui32Right	= ((IMG_UINT32)(psDstRect[ui32RectIndex].x1 + 2 * EURASIA_PARAM_VF_X_OFFSET) << (EURASIA_PARAM_VF_X_FRAC - 1));
			}
			else
#endif
			{
				ui32Top		= ((IMG_UINT32)(psDstRect[ui32RectIndex].y0 + EURASIA_PARAM_VF_Y_OFFSET) << EURASIA_PARAM_VF_Y_FRAC);
				ui32Bottom	= ((IMG_UINT32)(psDstRect[ui32RectIndex].y1 + EURASIA_PARAM_VF_Y_OFFSET) << EURASIA_PARAM_VF_Y_FRAC);
				ui32Left	= ((IMG_UINT32)(psDstRect[ui32RectIndex].x0 + EURASIA_PARAM_VF_X_OFFSET) << EURASIA_PARAM_VF_X_FRAC);
				ui32Right	= ((IMG_UINT32)(psDstRect[ui32RectIndex].x1 + EURASIA_PARAM_VF_X_OFFSET) << EURASIA_PARAM_VF_X_FRAC);
			}

#if defined(SGX545)
			/* X0Y0 (7..0 of X0, 23..0 of Y0) */
			*pui32CtrlStream++ = (ui32Left << EURASIA_PARAM_VF_X_EVEN0_SHIFT) |
								((ui32Top << EURASIA_PARAM_VF_Y_EVEN0_SHIFT) & 0x00ffffff);

			/* Z0X0 - (15..0 of Z0 is 0, 31..16 of X0) */
			*pui32CtrlStream++ = ((ui32Left >> EURASIA_PARAM_VF_X_EVEN1_ALIGNSHIFT) << EURASIA_PARAM_VF_X_EVEN1_SHIFT);
			
			/* Y1Z0 - (15..0 of Y1, 31..16 of Z0) */
			*pui32CtrlStream++ = (ui32Bottom << EURASIA_PARAM_VF_Y_ODD0_SHIFT);
		
			/* X1Y1 - (15..0 of X1, 31..16 of Y1) */
			*pui32CtrlStream++ = (ui32Left << EURASIA_PARAM_VF_X_ODD1_SHIFT) |
								(((ui32Bottom >> EURASIA_PARAM_VF_Y_ODD1_ALIGNSHIFT) << EURASIA_PARAM_VF_Y_ODD1_SHIFT) & 0xff);
		
			/* Z1 - (31..0 of Z1) */
			/* CONST */
			*pui32CtrlStream++ = 0;
		
			/* X2Y2 - (7..0 of X2, 23..0 of Y2) */
			*pui32CtrlStream++ = (ui32Right << EURASIA_PARAM_VF_X_EVEN0_SHIFT) |
								((ui32Bottom << EURASIA_PARAM_VF_Y_EVEN0_SHIFT) & 0xffffff);
		
			/* Z2X2 */
			*pui32CtrlStream++ = ((ui32Right >> EURASIA_PARAM_VF_X_EVEN1_ALIGNSHIFT) << EURASIA_PARAM_VF_X_EVEN1_SHIFT);
		
			/* Y3Z2 */
			*pui32CtrlStream++ = (ui32Top << EURASIA_PARAM_VF_Y_ODD0_SHIFT);
		
			/* x3y3 */
			*pui32CtrlStream++ = (ui32Right << EURASIA_PARAM_VF_X_ODD1_SHIFT) |
								(((ui32Top >> EURASIA_PARAM_VF_Y_ODD1_ALIGNSHIFT) << EURASIA_PARAM_VF_Y_ODD1_SHIFT) & 0xff);
		
			/* z3 */
			/* CONST */
			*pui32CtrlStream++ = 0;		/* PRQA S 3199 */ /* keep incrementanyway */
#else
			*pui32CtrlStream++ = (ui32Left << EURASIA_PARAM_VF_X_SHIFT) | (ui32Top << EURASIA_PARAM_VF_Y_SHIFT);
			/* CONST */
			*pui32CtrlStream++ = FLOAT32_ONE;
		
			*pui32CtrlStream++ = (ui32Left << EURASIA_PARAM_VF_X_SHIFT) | (ui32Bottom << EURASIA_PARAM_VF_Y_SHIFT);
			/* CONST */
			*pui32CtrlStream++ = FLOAT32_ONE;
		
			*pui32CtrlStream++ = (ui32Right << EURASIA_PARAM_VF_X_SHIFT) | (ui32Bottom << EURASIA_PARAM_VF_Y_SHIFT);
			/* CONST */
			*pui32CtrlStream++ = FLOAT32_ONE;
		
			*pui32CtrlStream++ = (ui32Right << EURASIA_PARAM_VF_X_SHIFT) | (ui32Top << EURASIA_PARAM_VF_Y_SHIFT);
			/* CONST */
			*pui32CtrlStream++ = FLOAT32_ONE;		/* PRQA S 3199 */ /* keep incrementanyway */
#endif
		}
	}

	PVR_ASSERT((pui32CtrlStream - pui32StreamBase) * sizeof(IMG_UINT32) == ui32SizeInBytes);

	return PVRSRV_OK;
}


/*****************************************************************************

 @Function	SGXTQ_ClampInputRects

 @Description

 Takes the Src and Dst rects and clamps them to the dimensions of their
 respective surface.

 @Input psSrcRect 		- pointer to the src input IMG_RECT.
 @Input	ui32SrcWidth 	- width of the src surface in pixels.
 @Input ui32SrcHeight 	- height of the src surface in pixels.

 @Input	psDstRect 		- pointer to the dst input IMG_RECT.
 @Input ui32DstWidth 	- width of the dst surface in pixels.
 @Input	ui32DstHeight 	- height of the dst surface in pixels.

 @Return - none.

*****************************************************************************/
IMG_INTERNAL IMG_VOID SGXTQ_ClampInputRects(IMG_RECT *psSrcRect,
										IMG_UINT32 ui32SrcWidth,
										IMG_UINT32 ui32SrcHeight,
										IMG_RECT *psDstRect,
										IMG_UINT32 ui32DstWidth,
										IMG_UINT32 ui32DstHeight)
{
#if defined(DEBUG)
	/* ensure the src is entirely on surface */
	PVR_ASSERT(psSrcRect->x0 >= 0);
	PVR_ASSERT(psSrcRect->y0 >= 0);
	PVR_ASSERT(psSrcRect->x1 >= 0);
	PVR_ASSERT(psSrcRect->y1 >= 0);
	PVR_ASSERT(psSrcRect->x0 < (IMG_INT32)ui32SrcWidth);
	PVR_ASSERT(psSrcRect->y0 < (IMG_INT32)ui32SrcHeight);
	PVR_ASSERT(psSrcRect->x1 <= (IMG_INT32)ui32SrcWidth);
	PVR_ASSERT(psSrcRect->y1 <= (IMG_INT32)ui32SrcHeight);

	/* make sure the atleast some of the dst is on the surface */
	PVR_ASSERT(psDstRect->y0 < (IMG_INT32)ui32DstHeight);
	PVR_ASSERT(psDstRect->x0 < (IMG_INT32)ui32DstWidth);
	PVR_ASSERT(psDstRect->y1 >= 0);
	PVR_ASSERT(psDstRect->x1 >= 0);
#else
	PVR_UNREFERENCED_PARAMETER(ui32SrcWidth);
	PVR_UNREFERENCED_PARAMETER(ui32SrcHeight);
#endif

	if (psDstRect->x0 < 0 )
	{
		psSrcRect->x0 -= psDstRect->x0;
		psDstRect->x0 -= psDstRect->x0;
	}

	if (psDstRect->y0 < 0 )
	{
		psSrcRect->y0 -= psDstRect->y0;
		psDstRect->y0 -= psDstRect->y0;
	}

	if (psDstRect->x1 > (IMG_INT32)ui32DstWidth)
	{
		psSrcRect->x1 -= (psDstRect->x1 - (IMG_INT32)ui32DstWidth);
		psDstRect->x1 -= (psDstRect->x1 - (IMG_INT32)ui32DstWidth);
	}

	if (psDstRect->y1 > (IMG_INT32)ui32DstHeight)
	{
		psSrcRect->y1 -= (psDstRect->y1 - (IMG_INT32)ui32DstHeight);
		psDstRect->y1 -= (psDstRect->y1 - (IMG_INT32)ui32DstHeight);
	}
}


/*****************************************************************************
 * Function Name		:	SGXTQ_CopyToStagingBuffer
 * Inputs				:	pvSBLinAddr				- the base address of the destination data	
 *							ui32SBStrideInBytes		- the byte stride of the destination data line 
 *							pbySrcLinAddr			- the base address of the source data
 *							ui32SrcStrideInBytes	- the byte stride of the source data line 
 *							ui32BytesPP				- the number of bytes in a pixel excluding its stride
 *							ui32PixelByteStride		- the number of bytes in a pixel including its stride
 *							ui32HeightToCopy		- the number of lines to copy
 *							ui32WidthToCopy			- the number of pixels to copy per line
 * Outputs				:	None
 * Description			:	Copies and packs pixel data. Resulting data is always planar or single chunk while
 *							it's capable of reading multi-chunk packed sources. If the pixel stride is equal
 *							to or less than the pixel size it assumes planar - or single-chunk packed sources.
 *							Otherwise the source is treated as multi-chunk packed, where it only copies the
 *							current chunk pointed by the pbySrcLinAddr. Copying all chunks requires multiple
 *							calls to this function, and offseting the base address to start with the particular
 *							chunk.
******************************************************************************/
IMG_INTERNAL IMG_VOID SGXTQ_CopyToStagingBuffer(IMG_VOID						*pvSBLinAddr,
									IMG_UINT32									ui32SBStrideInBytes,
									IMG_PBYTE									pbySrcLinAddr,
									IMG_UINT32									ui32SrcStrideInBytes,
									IMG_UINT32									ui32BytesPP,
									IMG_UINT32									ui32PixelByteStride,
									IMG_UINT32									ui32HeightToCopy,
									IMG_UINT32									ui32WidthToCopy)
{
	if (ui32PixelByteStride <= ui32BytesPP)
	{
		IMG_UINT32 ui32SizeToCopy = ui32WidthToCopy * ui32BytesPP;

		PVR_ASSERT(ui32SizeToCopy <= ui32SBStrideInBytes);

		while (ui32HeightToCopy != 0)
		{
			/* copy only width bpp bytes*/
			PVRSRVMemCopy(pvSBLinAddr, pbySrcLinAddr, ui32SizeToCopy);

			/* Update the source pointer by the original stride of the texture.*/
			pbySrcLinAddr += ui32SrcStrideInBytes;
			/* and the staging buffer pointer with the SB stride */
			pvSBLinAddr = ((IMG_PBYTE)pvSBLinAddr) + ui32SBStrideInBytes;

			ui32HeightToCopy--;
		}
	}
	else
	{
		while ((IMG_INT32)ui32HeightToCopy-- > 0)
		{
			IMG_PBYTE	pbySrcPtr = pbySrcLinAddr;
			IMG_PBYTE	pbyDstPtr = (IMG_PBYTE)pvSBLinAddr;
			IMG_UINT32	i;

			for (i = 0; i < ui32WidthToCopy; i++)
			{
				switch (ui32BytesPP) /* don't break in this switch */
				{
					case 8: *pbyDstPtr++ = *pbySrcPtr++;
					case 7: *pbyDstPtr++ = *pbySrcPtr++;
					case 6: *pbyDstPtr++ = *pbySrcPtr++;
					case 5: *pbyDstPtr++ = *pbySrcPtr++;
					case 4: *pbyDstPtr++ = *pbySrcPtr++;
					case 3: *pbyDstPtr++ = *pbySrcPtr++;
					case 2: *pbyDstPtr++ = *pbySrcPtr++;
					case 1: *pbyDstPtr++ = *pbySrcPtr++;
				}
				/* destination is always planar or single chunk,
				 * step over the other chunks on the source.
				 */
				pbySrcPtr += ui32PixelByteStride - ui32BytesPP;
			}
			pbySrcLinAddr += ui32SrcStrideInBytes;
			pvSBLinAddr = ((IMG_PBYTE)pvSBLinAddr) + ui32SBStrideInBytes;
		}
	}
}

#if defined(PDUMP) && defined(SGXTQ_DEBUG_PDUMP_TQINPUT)
static IMG_PCHAR SGXTQ_FilterToStr(SGXTQ_FILTERTYPE eFilter)
{
	switch (eFilter)
	{
		case SGXTQ_FILTERTYPE_POINT:
			return "Point";
		case SGXTQ_FILTERTYPE_LINEAR:
			return "Linear";
		case SGXTQ_FILTERTYPE_ANISOTROPIC:
			return "Anisotropic";
		default:
			return "!!!";
	}
}

static IMG_PCHAR SGXTQ_RotationToStr(SGXTQ_ROTATION eRotation)
{
	switch (eRotation)
	{
		case SGXTQ_ROTATION_NONE:
			return "0 deg";
		case SGXTQ_ROTATION_90:
			return "90 deg";
		case SGXTQ_ROTATION_180:
			return "180 deg";
		case SGXTQ_ROTATION_270:
			return "270 deg";
		default:
			return "!!!";
	}
}

static IMG_PCHAR SGXTQ_AlphaToStr(SGXTQ_ALPHA eAlpha)
{
	switch (eAlpha)
	{
		case SGXTQ_ALPHA_NONE:
			return "No alpha";
		case SGXTQ_ALPHA_SOURCE:
			return "source alpha";
		case SGXTQ_ALPHA_PREMUL_SOURCE:
			return "premultiplied source alpha";
		case SGXTQ_ALPHA_GLOBAL:
			return "global alpha";
		case SGXTQ_ALPHA_PREMUL_SOURCE_WITH_GLOBAL:
			return "global alpha & premultiplied source";
		default:
			return "!!!";
	}
}

static IMG_PCHAR SGXTQ_BoolToStr(IMG_BOOL bBool)
{
	return (bBool ? "True" : "False");
}

static IMG_PCHAR SGXTQ_MemLayoutToStr(SGXTQ_MEMLAYOUT eMemLayout)
{
	switch (eMemLayout)
	{
		case SGXTQ_MEMLAYOUT_2D:
			return "2D (twiddled input)";
		case SGXTQ_MEMLAYOUT_3D:
			return "3D";
		case SGXTQ_MEMLAYOUT_CEM:
			return "CEM";
		case SGXTQ_MEMLAYOUT_STRIDE:
			return "Strided (input)";
		case SGXTQ_MEMLAYOUT_TILED:
			return "Tiled (input)";
		case SGXTQ_MEMLAYOUT_OUT_LINEAR:
			return "Strided (output)";
		case SGXTQ_MEMLAYOUT_OUT_TILED:
			return "Tiled (output)";
		case SGXTQ_MEMLAYOUT_OUT_TWIDDLED:
			return "Twiddled (output)";
		default:
			return "!!!";
	}
}

static IMG_PCHAR SGXTQ_PixelFmtToStr(PVRSRV_PIXEL_FORMAT eFormat)
{
	switch (eFormat)
	{
		case PVRSRV_PIXEL_FORMAT_UNKNOWN: return "UNKNOWN";
		case PVRSRV_PIXEL_FORMAT_RGB565: return "RGB565";
		case PVRSRV_PIXEL_FORMAT_RGB555: return "RGB555";
		case PVRSRV_PIXEL_FORMAT_RGB888: return "RGB888";
		case PVRSRV_PIXEL_FORMAT_BGR888: return "BGR888";
		case PVRSRV_PIXEL_FORMAT_GREY_SCALE: return "GREY_SCALE";
		case PVRSRV_PIXEL_FORMAT_PAL12: return "PAL12";
		case PVRSRV_PIXEL_FORMAT_PAL8: return "PAL8";
		case PVRSRV_PIXEL_FORMAT_PAL4: return "PAL4";
		case PVRSRV_PIXEL_FORMAT_PAL2: return "PAL2";
		case PVRSRV_PIXEL_FORMAT_PAL1: return "PAL1";
		case PVRSRV_PIXEL_FORMAT_ARGB1555: return "ARGB1555";
		case PVRSRV_PIXEL_FORMAT_ARGB4444: return "ARGB4444";
		case PVRSRV_PIXEL_FORMAT_ARGB8888: return "ARGB8888";
		case PVRSRV_PIXEL_FORMAT_ABGR8888: return "ABGR8888";
		case PVRSRV_PIXEL_FORMAT_YV12: return "YV12";
		case PVRSRV_PIXEL_FORMAT_I420: return "I420";
		case PVRSRV_PIXEL_FORMAT_IMC2: return "IMC2";
		case PVRSRV_PIXEL_FORMAT_XRGB8888: return "XRGB8888";
		case PVRSRV_PIXEL_FORMAT_XBGR8888: return "XBGR8888";
		case PVRSRV_PIXEL_FORMAT_XRGB4444: return "XRGB4444";
		case PVRSRV_PIXEL_FORMAT_ARGB8332: return "ARGB8332";
		case PVRSRV_PIXEL_FORMAT_A2RGB10: return "A2RGB10";
		case PVRSRV_PIXEL_FORMAT_A2BGR10: return "A2BGR10";
		case PVRSRV_PIXEL_FORMAT_P8: return "P8";
		case PVRSRV_PIXEL_FORMAT_L8: return "L8";
		case PVRSRV_PIXEL_FORMAT_A8L8: return "A8L8";
		case PVRSRV_PIXEL_FORMAT_A4L4: return "A4L4";
		case PVRSRV_PIXEL_FORMAT_L16: return "L16";
		case PVRSRV_PIXEL_FORMAT_L6V5U5: return "L6V5U5";
		case PVRSRV_PIXEL_FORMAT_V8U8: return "V8U8";
		case PVRSRV_PIXEL_FORMAT_V16U16: return "V16U16";
		case PVRSRV_PIXEL_FORMAT_QWVU8888: return "QWVU8888";
		case PVRSRV_PIXEL_FORMAT_XLVU8888: return "XLVU8888";
		case PVRSRV_PIXEL_FORMAT_QWVU16: return "QWVU16";
		case PVRSRV_PIXEL_FORMAT_D16: return "D16";
		case PVRSRV_PIXEL_FORMAT_D24S8: return "D24S8";
		case PVRSRV_PIXEL_FORMAT_D24X8: return "D24X8";
		case PVRSRV_PIXEL_FORMAT_ABGR16: return "ABGR16";
		case PVRSRV_PIXEL_FORMAT_ABGR16F: return "ABGR16F";
		case PVRSRV_PIXEL_FORMAT_ABGR32: return "ABGR32";
		case PVRSRV_PIXEL_FORMAT_ABGR32F: return "ABGR32F";
		case PVRSRV_PIXEL_FORMAT_B10GR11: return "B10GR11";
		case PVRSRV_PIXEL_FORMAT_GR88: return "GR88";
		case PVRSRV_PIXEL_FORMAT_BGR32: return "BGR32";
		case PVRSRV_PIXEL_FORMAT_GR32: return "GR32";
		case PVRSRV_PIXEL_FORMAT_E5BGR9: return "E5BGR9";
		case PVRSRV_PIXEL_FORMAT_RESERVED1: return "RESERVED1";
		case PVRSRV_PIXEL_FORMAT_RESERVED2: return "RESERVED2";
		case PVRSRV_PIXEL_FORMAT_RESERVED3: return "RESERVED3";
		case PVRSRV_PIXEL_FORMAT_RESERVED4: return "RESERVED4";
		case PVRSRV_PIXEL_FORMAT_RESERVED5: return "RESERVED5";
		case PVRSRV_PIXEL_FORMAT_R8G8_B8G8: return "R8G8_B8G8";
		case PVRSRV_PIXEL_FORMAT_G8R8_G8B8: return "G8R8_G8B8";
		case PVRSRV_PIXEL_FORMAT_NV11: return "NV11";
		case PVRSRV_PIXEL_FORMAT_NV12: return "NV12";
		case PVRSRV_PIXEL_FORMAT_YUY2: return "YUY2";
		case PVRSRV_PIXEL_FORMAT_YUV420: return "YUV420";
		case PVRSRV_PIXEL_FORMAT_YUV444: return "YUV444";
		case PVRSRV_PIXEL_FORMAT_VUY444: return "VUY444";
		case PVRSRV_PIXEL_FORMAT_YUYV: return "YUYV";
		case PVRSRV_PIXEL_FORMAT_YVYU: return "YVYU";
		case PVRSRV_PIXEL_FORMAT_UYVY: return "UYVY";
		case PVRSRV_PIXEL_FORMAT_VYUY: return "VYUY";
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY: return "FOURCC_ORG_UYVY";
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV: return "FOURCC_ORG_YUYV";
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU: return "FOURCC_ORG_YVYU";
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY: return "FOURCC_ORG_VYUY";
		case PVRSRV_PIXEL_FORMAT_A32B32G32R32: return "A32B32G32R32";
		case PVRSRV_PIXEL_FORMAT_A32B32G32R32F: return "A32B32G32R32F";
		case PVRSRV_PIXEL_FORMAT_A32B32G32R32_UINT: return "A32B32G32R32_UINT";
		case PVRSRV_PIXEL_FORMAT_A32B32G32R32_SINT: return "A32B32G32R32_SINT";
		case PVRSRV_PIXEL_FORMAT_B32G32R32: return "B32G32R32";
		case PVRSRV_PIXEL_FORMAT_B32G32R32F: return "B32G32R32F";
		case PVRSRV_PIXEL_FORMAT_B32G32R32_UINT: return "B32G32R32_UINT";
		case PVRSRV_PIXEL_FORMAT_B32G32R32_SINT: return "B32G32R32_SINT";
		case PVRSRV_PIXEL_FORMAT_G32R32: return "G32R32";
		case PVRSRV_PIXEL_FORMAT_G32R32F: return "G32R32F";
		case PVRSRV_PIXEL_FORMAT_G32R32_UINT: return "G32R32_UINT";
		case PVRSRV_PIXEL_FORMAT_G32R32_SINT: return "G32R32_SINT";
		case PVRSRV_PIXEL_FORMAT_D32F: return "D32F";
		case PVRSRV_PIXEL_FORMAT_R32: return "R32";
		case PVRSRV_PIXEL_FORMAT_R32F: return "R32F";
		case PVRSRV_PIXEL_FORMAT_R32_UINT: return "R32_UINT";
		case PVRSRV_PIXEL_FORMAT_R32_SINT: return "R32_SINT";
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16: return "A16B16G16R16";
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16F: return "A16B16G16R16F";
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16_SINT: return "A16B16G16R16_SINT";
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16_SNORM: return "A16B16G16R16_SNORM";
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16_UINT: return "A16B16G16R16_UINT";
		case PVRSRV_PIXEL_FORMAT_A16B16G16R16_UNORM: return "A16B16G16R16_UNORM";
		case PVRSRV_PIXEL_FORMAT_G16R16: return "G16R16";
		case PVRSRV_PIXEL_FORMAT_G16R16F: return "G16R16F";
		case PVRSRV_PIXEL_FORMAT_G16R16_UINT: return "G16R16_UINT";
		case PVRSRV_PIXEL_FORMAT_G16R16_UNORM: return "G16R16_UNORM";
		case PVRSRV_PIXEL_FORMAT_G16R16_SINT: return "G16R16_SINT";
		case PVRSRV_PIXEL_FORMAT_G16R16_SNORM: return "G16R16_SNORM";
		case PVRSRV_PIXEL_FORMAT_R16: return "R16";
		case PVRSRV_PIXEL_FORMAT_R16F: return "R16F";
		case PVRSRV_PIXEL_FORMAT_R16_UINT: return "R16_UINT";
		case PVRSRV_PIXEL_FORMAT_R16_UNORM: return "R16_UNORM";
		case PVRSRV_PIXEL_FORMAT_R16_SINT: return "R16_SINT";
		case PVRSRV_PIXEL_FORMAT_R16_SNORM: return "R16_SNORM";
		case PVRSRV_PIXEL_FORMAT_A8B8G8R8: return "A8B8G8R8";
		case PVRSRV_PIXEL_FORMAT_A8B8G8R8_UINT: return "A8B8G8R8_UINT";
		case PVRSRV_PIXEL_FORMAT_A8B8G8R8_UNORM: return "A8B8G8R8_UNORM";
		case PVRSRV_PIXEL_FORMAT_A8B8G8R8_SINT: return "A8B8G8R8_SINT";
		case PVRSRV_PIXEL_FORMAT_A8B8G8R8_SNORM: return "A8B8G8R8_SNORM";
		case PVRSRV_PIXEL_FORMAT_G8R8: return "G8R8";
		case PVRSRV_PIXEL_FORMAT_G8R8_UINT: return "G8R8_UINT";
		case PVRSRV_PIXEL_FORMAT_G8R8_UNORM: return "G8R8_UNORM";
		case PVRSRV_PIXEL_FORMAT_G8R8_SINT: return "G8R8_SINT";
		case PVRSRV_PIXEL_FORMAT_G8R8_SNORM: return "G8R8_SNORM";
		case PVRSRV_PIXEL_FORMAT_A8: return "A8";
		case PVRSRV_PIXEL_FORMAT_R8: return "R8";
		case PVRSRV_PIXEL_FORMAT_R8_UINT: return "R8_UINT";
		case PVRSRV_PIXEL_FORMAT_R8_UNORM: return "R8_UNORM";
		case PVRSRV_PIXEL_FORMAT_R8_SINT: return "R8_SINT";
		case PVRSRV_PIXEL_FORMAT_R8_SNORM: return "R8_SNORM";
		case PVRSRV_PIXEL_FORMAT_A2B10G10R10: return "A2B10G10R10";
		case PVRSRV_PIXEL_FORMAT_A2B10G10R10_UNORM: return "A2B10G10R10_UNORM";
		case PVRSRV_PIXEL_FORMAT_A2B10G10R10_UINT: return "A2B10G10R10_UINT";
		case PVRSRV_PIXEL_FORMAT_B10G11R11: return "B10G11R11";
		case PVRSRV_PIXEL_FORMAT_B10G11R11F: return "B10G11R11F";
		case PVRSRV_PIXEL_FORMAT_X24G8R32: return "X24G8R32";
		case PVRSRV_PIXEL_FORMAT_G8R24: return "G8R24";
		case PVRSRV_PIXEL_FORMAT_E5B9G9R9: return "E5B9G9R9";
		case PVRSRV_PIXEL_FORMAT_R1: return "R1";
		case PVRSRV_PIXEL_FORMAT_RESERVED6: return "RESERVED6";
		case PVRSRV_PIXEL_FORMAT_RESERVED7: return "RESERVED7";
		case PVRSRV_PIXEL_FORMAT_RESERVED8: return "RESERVED8";
		case PVRSRV_PIXEL_FORMAT_RESERVED9: return "RESERVED9";
		case PVRSRV_PIXEL_FORMAT_RESERVED10: return "RESERVED10";
		case PVRSRV_PIXEL_FORMAT_RESERVED11: return "RESERVED11";
		case PVRSRV_PIXEL_FORMAT_RESERVED12: return "RESERVED12";
		case PVRSRV_PIXEL_FORMAT_RESERVED13: return "RESERVED13";
		case PVRSRV_PIXEL_FORMAT_RESERVED14: return "RESERVED14";
		case PVRSRV_PIXEL_FORMAT_RESERVED15: return "RESERVED15";
		case PVRSRV_PIXEL_FORMAT_RESERVED16: return "RESERVED16";
		case PVRSRV_PIXEL_FORMAT_RESERVED17: return "RESERVED17";
		case PVRSRV_PIXEL_FORMAT_RESERVED18: return "RESERVED18";
		case PVRSRV_PIXEL_FORMAT_RESERVED19: return "RESERVED19";
		case PVRSRV_PIXEL_FORMAT_RESERVED20: return "RESERVED20";
		case PVRSRV_PIXEL_FORMAT_FORCE_I32: return "FORCE_I32";
		default:
			return "!!!";
	}
}

static IMG_VOID SGXTQ_PdumpSurfaceDecode(IMG_CONST PVRSRV_CONNECTION* psConnection,
									SGXTQ_SURFACE* psSurf,
									IMG_UINT32 ui32PDumpFlags)
{
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tsDevVAddr:\t\t\t0x%08x", SGX_TQSURFACE_GET_DEV_VADDR(*psSurf));
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32Width, Height:\t%d x %d", psSurf->ui32Width, psSurf->ui32Height);
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\ti32StrideInBytes:\t%d", psSurf->i32StrideInBytes);
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\teFormat:\t\t\t%s", SGXTQ_PixelFmtToStr(psSurf->eFormat));
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\teMemLayout:\t\t\t%s", SGXTQ_MemLayoutToStr(psSurf->eMemLayout));
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32ChunkStride:\t%d", psSurf->ui32ChunkStride);
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tpsSyncInfo:\t%p", psSurf->psSyncInfo);
	if (psSurf->psSyncInfo)
	{
		PVRSRV_CLIENT_SYNC_INFO * psSyncInfo = psSurf->psSyncInfo;
		PVRSRV_SYNC_DATA * psSyncData = psSyncInfo->psSyncData;
		PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\tui32WriteOpsPending:\t%d", psSyncData->ui32WriteOpsPending);
		PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\tui32WriteOpsComplete:\t%d", psSyncData->ui32WriteOpsComplete);

		PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\tui32ReadOpsPending:\t%d", psSyncData->ui32ReadOpsPending);
		PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\tui32ReadOpsComplete:\t%d", psSyncData->ui32ReadOpsComplete);

		PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\tui32LastOpDumpVal:\t%d", psSyncData->ui32LastOpDumpVal);
		PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\tui32LastReadOpDumpVal:\t%d", psSyncData->ui32LastReadOpDumpVal);
	}
}


/*****************************************************************************
 * Function Name		:	SGXTQ_PDumpTQInput
 * Inputs				:	psConnection				- Connection to services
 * 							psQueueTransfer				- the transfer cmd from the client
 * Outputs				:	None
 * Returns				:	None
 * Description			:	Writes the entire TQ input into Pdump comments.
******************************************************************************/
IMG_INTERNAL IMG_VOID SGXTQ_PDumpTQInput(IMG_CONST PVRSRV_CONNECTION	* psConnection,
									SGX_QUEUETRANSFER					* psQueueTransfer,
									IMG_UINT32							ui32FenceID)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32Val;
	IMG_UINT32 ui32PDumpFlags = (psQueueTransfer->bPDumpContinuous ? PDUMP_FLAGS_CONTINUOUS : 0);

	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "== TQ Input =[%8.8x]==================", ui32FenceID);
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tui32Flags:\t\t\t\t%d", psQueueTransfer->ui32Flags);
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tbPDumpContinuous:\t\t\t\t%d", psQueueTransfer->bPDumpContinuous);
	switch (psQueueTransfer->eType)
	{

		case SGXTQ_BLIT:
		{
			SGXTQ_BLITOP* psBlit = &psQueueTransfer->Details.sBlit;
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tBlit");
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\teFilter:\t\t\t%s", SGXTQ_FilterToStr(psBlit->eFilter));
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32ColourKey:\t\t%d", psBlit->ui32ColourKey);
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\teRotation:\t\t\t%s", SGXTQ_RotationToStr(psBlit->eRotation));
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\teAlpha:\t\t\t\t%s", SGXTQ_AlphaToStr(psBlit->eAlpha));
			ui32Val = (IMG_UINT32)psBlit->byGlobalAlpha;
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tbyGlobalAlpha:\t\t%x", ui32Val);
			ui32Val = (IMG_UINT32)psBlit->byCustomRop3;
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tbyCustomRop3:\t\t%x", ui32Val);
#if defined(SUPPORT_DISPLAYCONTROLLER_TILING)
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32BIFTile0config:\t\t%d", psBlit->ui32BIFTile0Config);
#endif

			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tsUSExecAddr:\t\t0x%08x", psBlit->sUSEExecAddr.uiAddr);
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tUSEParams:\t\t\t%d %d", psBlit->UseParams[0], psBlit->UseParams[1]);
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tbEnablePattern:\t\t%s", SGXTQ_BoolToStr(psBlit->bEnablePattern));
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tbSingleSource:\t\t%s", SGXTQ_BoolToStr(psBlit->bSingleSource));
			break;
		}
#if defined(SGX_FEATURE_2D_HARDWARE)
		case SGXTQ_2DHWBLIT:
		{
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t2DHWBlit");
			break;
		}
#endif
		case SGXTQ_MIPGEN:
		{
			SGXTQ_MIPGENOP * psMipgen = &psQueueTransfer->Details.sMipGen;
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tMipGen");
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\teFilter:\t\t\t%s", SGXTQ_FilterToStr(psMipgen->eFilter));
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32Levels:\t\t\t%d", psMipgen->ui32Levels);
			break;
		}
		case SGXTQ_FILL:
		{
			SGXTQ_FILLOP * psFill = &psQueueTransfer->Details.sFill;
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tFill");
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32Colour:\t\t\t%8.8x", psFill->ui32Colour);
			ui32Val = (IMG_UINT32)psFill->byCustomRop3;
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tbyCustomRop3:\t\t%x", ui32Val);
#if defined(SUPPORT_DISPLAYCONTROLLER_TILING)
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32BIFTile0config:\t\t%d", psFill->ui32BIFTile0Config);
#endif
			break;
		}
		case SGXTQ_BUFFERBLT:
		{
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tBufferBlt");
			break;
		}
		case SGXTQ_CUSTOM:
		{
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tCustom");
			break;
		}
		case SGXTQ_FULL_CUSTOM:
		{
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tFull_Custom");
			break;
		}
		case SGXTQ_TEXTURE_UPLOAD:
		{
			SGXTQ_TEXTURE_UPLOADOP * psUpload =  &psQueueTransfer->Details.sTextureUpload;
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tUpload");
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tpbySrcLinAddr:\t\t%p", psUpload->pbySrcLinAddr);
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32BytesPP:\t\t%d", psUpload->ui32BytesPP);
			break;
		}
		case SGXTQ_CLIP_BLIT:
		{
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tClip_Blit");
			break;
		}
		case SGXTQ_CUSTOMSHADER_BLIT:
		{
			SGXTQ_CUSTOMSHADEROP * psCShader =  &psQueueTransfer->Details.sCustomShader;

			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tCustomSHaderBlit");
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tsUSExecAddr:\t\t0x%08x", psCShader->sUSEExecAddr.uiAddr);
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32NumPAs:\t\t%d", psCShader->ui32NumPAs);
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32NumSAs:\t\t%d", psCShader->ui32NumSAs);
			for (i = 0; i < psCShader->ui32NumSAs; i++)
			{
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\tSA[%d]:\t\t%d", i, psCShader->UseParams[i]);
			}
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32NumTempRegs:\t\t%d", psCShader->ui32NumTempRegs);
			break;
		}
		case SGXTQ_COLOURLUT_BLIT:
		{
			SGXTQ_COLOURLUTOP * psLUT =  &psQueueTransfer->Details.sColourLUT;
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\teLUTPxFmt:\t\t\t%s", SGXTQ_PixelFmtToStr(psLUT->eLUTPxFmt));
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tui32KeySizeInBits:\t%d", psLUT->ui32KeySizeInBits);
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tsLUTDevVAddr:\t\t0x%08x", psLUT->sLUTDevVAddr.uiAddr);
			break;
		}
		case SGXTQ_VIDEO_BLIT:
		{
			SGXTQ_VPBLITOP * psVPB =  &psQueueTransfer->Details.sVPBlit;
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tVideo Proccessing Blit");
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tbCoeffsGiven:\t\t%s", SGXTQ_BoolToStr(psVPB->bCoeffsGiven));
			if (psVPB->bCoeffsGiven)
			{
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16YR (Luma - red):\t\t%d", psVPB->sCoeffs.i16YR);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16UR (ChrU - red):\t\t%d", psVPB->sCoeffs.i16UR);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16VR (ChrV - red):\t\t%d", psVPB->sCoeffs.i16VR);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16VR (Const - red):\t\t%d", psVPB->sCoeffs.i16ConstR);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16VR (Shift - red):\t\t%d", psVPB->sCoeffs.i16ShiftR);

				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16YG (Luma - green):\t\t%d", psVPB->sCoeffs.i16YG);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16UG (ChrU - green):\t\t%d", psVPB->sCoeffs.i16UG);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16VG (ChrV - green):\t\t%d", psVPB->sCoeffs.i16VG);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16VG (Const - green):\t\t%d", psVPB->sCoeffs.i16ConstG);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16VG (Shift - green):\t\t%d", psVPB->sCoeffs.i16ShiftG);

				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16YB (Luma - blue):\t\t%d", psVPB->sCoeffs.i16YB);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16UB (ChrU - blue):\t\t%d", psVPB->sCoeffs.i16UB);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16VB (ChrV - blue):\t\t%d", psVPB->sCoeffs.i16VB);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16VB (Const - blue):\t\t%d", psVPB->sCoeffs.i16ConstB);
				PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\t\ti16VB (Shift - blue):\t\t%d", psVPB->sCoeffs.i16ShiftB);
			}
			break;
		}
		case SGXTQ_ARGB2NV12_BLIT:
		{
			SGXTQ_ARGB2NV12OP * psNV12 = &psQueueTransfer->Details.sARGB2NV12;
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tARGB 2 NV12 encoder Blit");
			PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\t\tsUVDevVAddr:\t%8.8x", psNV12->sUVDevVAddr.uiAddr);
			break;
		}
		case SGXTQ_CLEAR_TYPE_BLEND:
		case SGXTQ_TATLAS_BLIT:
		{
			
		}
	}

	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tui32NumSources:\t\t\t%d", psQueueTransfer->ui32NumSources);
	for (i = 0; i < psQueueTransfer->ui32NumSources; i++)
	{
		SGXTQ_PdumpSurfaceDecode(psConnection, &psQueueTransfer->asSources[i], ui32PDumpFlags);
	}
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tui32NumDest:\t\t\t%d", psQueueTransfer->ui32NumDest);
	for (i = 0; i < psQueueTransfer->ui32NumDest; i++)
	{
		SGXTQ_PdumpSurfaceDecode(psConnection, &psQueueTransfer->asDests[i], ui32PDumpFlags);
	}

	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tui32NumSrcRect:\t\t\t%d", SGX_QUEUETRANSFER_NUM_SRC_RECTS(*(psQueueTransfer)));

	for (i = 0; i < SGX_QUEUETRANSFER_NUM_SRC_RECTS(*(psQueueTransfer)); i++)
	{
		PDUMPCOMMENTWITHFLAGSF(psConnection,
				ui32PDumpFlags,
				"\t\t(%d, %d) X (%d, %d)",
				SGX_QUEUETRANSFER_SRC_RECT(*(psQueueTransfer), i).x0,
				SGX_QUEUETRANSFER_SRC_RECT(*(psQueueTransfer), i).y0,
				SGX_QUEUETRANSFER_SRC_RECT(*(psQueueTransfer), i).x1,
				SGX_QUEUETRANSFER_SRC_RECT(*(psQueueTransfer), i).y1);
	}
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tui32NumDestRect:\t\t%d", SGX_QUEUETRANSFER_NUM_DST_RECTS(*(psQueueTransfer)));
	for (i = 0; i < SGX_QUEUETRANSFER_NUM_DST_RECTS(*(psQueueTransfer)); i++)
	{
			PDUMPCOMMENTWITHFLAGSF(psConnection,
				ui32PDumpFlags,
				"\t\t(%d, %d) X (%d, %d)",
				SGX_QUEUETRANSFER_DST_RECT(*(psQueueTransfer), i).x0,
				SGX_QUEUETRANSFER_DST_RECT(*(psQueueTransfer), i).y0,
				SGX_QUEUETRANSFER_DST_RECT(*(psQueueTransfer), i).x1,
				SGX_QUEUETRANSFER_DST_RECT(*(psQueueTransfer), i).y1);
	}

#if defined(SGXTQ_PREP_SUBMIT_SEPERATE)
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tPrepare/Submit separated");
#endif

	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "\tui32NumStatusValues:\t%d", psQueueTransfer->ui32NumStatusValues);
	for (i = 0; i < psQueueTransfer->ui32NumStatusValues; i++)
	{
		PVRSRV_MEMUPDATE* psMemUpdates = & psQueueTransfer->asMemUpdates[0];
		PDUMPCOMMENTWITHFLAGSF(psConnection,
				0,
				"\t\t0x%08x <- %d",
				psMemUpdates[i].ui32UpdateAddr,
				psMemUpdates[i].ui32UpdateVal);
	}

	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "=========================================\r\n");
}
#endif//#if defined(PDUMP) && defined(SGXTQ_DEBUG_PDUMP_TQINPUT)

/*****************************************************************************
 * Function Name		:	SGXTQ_PDump2DCmd
 * Inputs				:	psConnection				- Connection to services
 * 							psQueueTransfer				- the transfer cmd from the client
 *							ps2DCmd						- 2D control stream
 * Outputs				:	None
 * Returns				:	None
 * Description			:	Dumps the 2D control stream to Pdump comments
******************************************************************************/
#if defined(SGX_FEATURE_2D_HARDWARE) && defined(PDUMP)
IMG_INTERNAL IMG_VOID SGXTQ_PDump2DCmd(IMG_CONST PVRSRV_CONNECTION* psConnection,
										SGX_QUEUETRANSFER * psQueueTransfer,
										PSGXMKIF_2DCMD ps2DCmd)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32PDumpFlags = (psQueueTransfer->bPDumpContinuous ? PDUMP_FLAGS_CONTINUOUS : 0);
	
	PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "%s command stream",
#if defined(SGX_FEATURE_PTLA)
			"PTLA");
#else
			"2D Core");
#endif

	if (ps2DCmd->ui32CtrlSizeInDwords > SGX_MAX_2D_BLIT_CMD_SIZE)
	{
		return;
	}

	for (i = 0; i < ps2DCmd->ui32CtrlSizeInDwords ; i++)
	{
		PDUMPCOMMENTWITHFLAGSF(psConnection, ui32PDumpFlags, "0x%08X",	ps2DCmd->aui32CtrlStream[i]);
	}
}
#endif//#if defined(SGX_FEATURE_2D_HARDWARE) && defined(PDUMP)

#endif /* TRANSFER_QUEUE */

/******************************************************************************
 End of file (sgxtransfer_utils.c)
******************************************************************************/
