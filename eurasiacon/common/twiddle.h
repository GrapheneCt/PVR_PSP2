/**************************************************************************
* Name         : twiddle.h
* Author       : jakub.lamik
* Created      : 02/10/2005
*
* Copyright    : 2003-2008 by Imagination Technologies Limited. All rights reserved.
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
* $Log: twiddle.h $
**************************************************************************/

#ifndef _TWIDDLE_
#define _TWIDDLE_


#if defined(SGX_FEATURE_HYBRID_TWIDDLING)

IMG_UINT32 GetTileSize(IMG_UINT32 ui32Width, IMG_UINT32 ui32Height);

IMG_VOID DeTwiddleAddressPVRTC2( IMG_VOID    *pvDestAddress, 
								const IMG_VOID *pvSrcPixels, 
								IMG_UINT32  ui32Width, 
								IMG_UINT32  ui32Height, 
								IMG_UINT32  ui32StrideIn);

IMG_VOID DeTwiddleAddressPVRTC4( IMG_VOID    *pvDestAddress, 
								const IMG_VOID *pvSrcPixels, 
								IMG_UINT32  ui32Width, 
								IMG_UINT32  ui32Height, 
								IMG_UINT32  ui32StrideIn);


#endif /* defined(SGX_FEATURE_HYBRID_TWIDDLING) */


IMG_VOID DeTwiddleAddress8bpp(IMG_VOID   *pvDestAddress,
								const IMG_VOID   *pvSrcPixels, 
								IMG_UINT32  ui32Width,
								IMG_UINT32  ui32Height,
								IMG_UINT32  ui32StrideIn);

IMG_VOID ReadBackTwiddle8bpp( IMG_VOID   *pvDest,
								const IMG_VOID   *pvSrc, 
								IMG_UINT32  ui32Log2Width, 
								IMG_UINT32  ui32Log2Height, 
								IMG_UINT32  ui32Width, 
								IMG_UINT32  ui32Height, 
								IMG_UINT32  ui32DstStride);	


IMG_VOID DeTwiddleAddress16bpp(IMG_VOID   *pvDestAddress,
								const IMG_VOID   *pvSrcPixels, 
								IMG_UINT32  ui32Width,
								IMG_UINT32  ui32Height,
								IMG_UINT32  ui32StrideIn);

IMG_VOID ReadBackTwiddle16bpp( IMG_VOID   *pvDest,
								const IMG_VOID   *pvSrc, 
								IMG_UINT32  ui32Log2Width, 
								IMG_UINT32  ui32Log2Height, 
								IMG_UINT32  ui32Width, 
								IMG_UINT32  ui32Height, 
								IMG_UINT32  ui32DstStride);	

IMG_VOID DeTwiddleAddress32bpp(IMG_VOID   *pvDestAddress,
								const IMG_VOID   *pvSrcPixels, 
								IMG_UINT32  ui32Width,
								IMG_UINT32  ui32Height,
								IMG_UINT32  ui32StrideIn);	

IMG_VOID ReadBackTwiddle32bpp( IMG_VOID   *pvDest,
								const IMG_VOID   *pvSrc, 
								IMG_UINT32  ui32Log2Width, 
								IMG_UINT32  ui32Log2Height, 
								IMG_UINT32  ui32Width, 
								IMG_UINT32  ui32Height, 
								IMG_UINT32  ui32DstStride);

IMG_VOID DeTwiddleAddressETC1(IMG_VOID	*pvDest,
								const IMG_VOID	*pvSrc,
								IMG_UINT32 ui32Width,
								IMG_UINT32 ui32Height,
								IMG_UINT32 ui32StrideIn);

IMG_VOID ReadBackTwiddleETC1(IMG_VOID *pvDest, const IMG_VOID *pvSrc, 
							 IMG_UINT32 ui32Log2Width, 
							 IMG_UINT32 ui32Log2Height, 
							 IMG_UINT32 ui32Width, 
							 IMG_UINT32 ui32Height,
							 IMG_UINT32 ui32DstStride);


#endif /* _TWIDDLE_ */

/******************************************************************************
 End of file (twiddle.h)
******************************************************************************/

