/******************************************************************************
 * Name         : tex.c
 *
 * Copyright    : 2005-2007 by Imagination Technologies Limited.
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
 * $Log: tex.c $
 *****************************************************************************/

#include "context.h"
#include "spanpack.h"
#include "drveglext.h"

#include "psp2/swtexop.h"

#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)

/***********************************************************************************
 Function Name      : CopyTextureFloatRGBA
 Inputs             : pui32Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies floating point RGBA texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureFloatRGBA(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
					   IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					   IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					   IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT32 *pui32RedDest = pui32Dest;
	IMG_UINT32 *pui32GreenDest = pui32RedDest + (psMipLevel->ui32Width * psMipLevel->ui32Height);
	IMG_UINT32 *pui32BlueDest = pui32GreenDest + (psMipLevel->ui32Width * psMipLevel->ui32Height);
	IMG_UINT32 *pui32AlphaDest = pui32BlueDest + (psMipLevel->ui32Width * psMipLevel->ui32Height);

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 4 * sizeof(IMG_UINT32));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pui32RedDest++ = *pui32Src++;
			*pui32GreenDest++ = *pui32Src++;
			*pui32BlueDest++ = *pui32Src++;
			*pui32AlphaDest++ = *pui32Src++;
		}while(--i);

		pui32RedDest += ui32DstRowIncrement;
		pui32GreenDest += ui32DstRowIncrement;
		pui32BlueDest += ui32DstRowIncrement;
		pui32AlphaDest += ui32DstRowIncrement;
		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}

/***********************************************************************************
 Function Name      : CopyTextureFloatRGB
 Inputs             : pui32Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies floating point RGB texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureFloatRGB(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
					  IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					  IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					  IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT32 *pui32RedDest = pui32Dest;
	IMG_UINT32 *pui32GreenDest = pui32RedDest + (psMipLevel->ui32Width * psMipLevel->ui32Height);
	IMG_UINT32 *pui32BlueDest = pui32GreenDest + (psMipLevel->ui32Width * psMipLevel->ui32Height);

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 3 * sizeof(IMG_UINT32));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pui32RedDest++ = *pui32Src++;
			*pui32GreenDest++ = *pui32Src++;
			*pui32BlueDest++ = *pui32Src++;
		}while(--i);

		pui32RedDest += ui32DstRowIncrement;
		pui32GreenDest += ui32DstRowIncrement;
		pui32BlueDest += ui32DstRowIncrement;
		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}

/***********************************************************************************
 Function Name      : CopyTextureFloatLA
 Inputs             : pui32Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies floating point Luminance Alpha texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureFloatLA(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
					 IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					 IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					 IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT32 *pui32LuminanceDest = pui32Dest;
	IMG_UINT32 *pui32AlphaDest = pui32Dest + (psMipLevel->ui32Width * psMipLevel->ui32Height);

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 2 * sizeof(IMG_UINT32));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pui32LuminanceDest++ = *pui32Src++;
			*pui32AlphaDest++ = *pui32Src++;
		}while(--i);

		pui32LuminanceDest += ui32DstRowIncrement;
		pui32AlphaDest += ui32DstRowIncrement;
		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}

#endif /*GLES2_EXTENSION_FLOAT_TEXTURE */ 

#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)

/***********************************************************************************
 Function Name      : CopyTextureHalfFloatRGBA
 Inputs             : pui16Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies half floating point (float16) RGBA texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureHalfFloatRGBA(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src,
					       IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					       IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					       IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 *pui16RedGreenDest = pui16Dest;
	IMG_UINT16 *pui16BlueAlphaDest = pui16Dest + (psMipLevel->ui32Width * psMipLevel->ui32Height * 2);

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 4 * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = (psMipLevel->ui32Width - ui32Width) << 1;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pui16RedGreenDest++ = *pui16Src++;
			*pui16RedGreenDest++ = *pui16Src++;
			*pui16BlueAlphaDest++ = *pui16Src++;
			*pui16BlueAlphaDest++ = *pui16Src++;
		}while(--i);

		pui16RedGreenDest += ui32DstRowIncrement;
		pui16BlueAlphaDest += ui32DstRowIncrement;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTextureHalfFloatRGB
 Inputs             : pui16Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies half floating point (float16) RGB texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureHalfFloatRGB(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src, 
												IMG_UINT32 ui32Width, IMG_UINT32 ui32Height, 
												IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
												IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 *pui16RedGreenDest = pui16Dest;
	IMG_UINT16 *pui16BlueDest;
	IMG_UINT32 ui32ByteOffset, ui32RGPlaneSizeInBytes;
	IMG_UINT32 ui32SrcRowByteIncrement;
	IMG_UINT32 ui32DstRowIncrement;
	
	/* Offset from start of buffer to current dest */
	ui32ByteOffset = (IMG_UINT8 *)pui16Dest - psMipLevel->pui8Buffer;
	ui32RGPlaneSizeInBytes = psMipLevel->ui32Width * psMipLevel->ui32Height * 4;

	pui16BlueDest = (IMG_UINT16 *)(psMipLevel->pui8Buffer + ui32RGPlaneSizeInBytes + (ui32ByteOffset >> 1));

	/* Difference between stride and copied width */
	ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 3 * sizeof(IMG_UINT16));

	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pui16RedGreenDest++ = *pui16Src++;
			*pui16RedGreenDest++ = *pui16Src++;
			*pui16BlueDest++ = *pui16Src++;
		}while(--i);

		pui16RedGreenDest += (ui32DstRowIncrement << 1);
		pui16BlueDest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}

#endif /* GLES2_EXTENSION_HALF_FLOAT_TEXTURE */

#if defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888)
/***********************************************************************************
 Function Name      : CopyTextureBGRA8888toRGBA8888
 Inputs             : pui32Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies 32 bit texture data, swapping the R and B components
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureBGRA8888toRGBA8888(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
													IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
													IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
													IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT32));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}


	do
	{
		i = ui32Width;

		do
		{
			IMG_UINT32 ui32Temp;

			ui32Temp = *pui32Src++;

			*pui32Dest++ = (ui32Temp & 0xFF00FF00) | ((ui32Temp >> 16) & 0xFF) | ((ui32Temp & 0xFF) << 16);

		}while(--i);

		pui32Dest += ui32DstRowIncrement;
		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTextureBGRA8888to5551
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from BGRA 8888 to ARGB 1555 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureBGRA8888to5551(IMG_UINT16 *pui16Dest, const IMG_UINT8 *pui8Src,
												IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
												IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
												IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 4 * sizeof(IMG_UINT8));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}


	do
	{
	
		i = ui32Width;

		do
		{
			/* Blue */
			ui8Temp = (*pui8Src++) >> 3; 
			ui16OutData = ui8Temp  << 0;

			/* Green */
			ui8Temp = (*pui8Src++) >> 3; 
			ui16OutData |= ui8Temp << 5;

			/* Red */
			ui8Temp = (*pui8Src++) >> 3; 
			ui16OutData |= ui8Temp << 10;

			/* Alpha */
			ui8Temp = (*pui8Src++) >> 7; 
			ui16OutData |= ui8Temp << 15;

			*pui16Dest++ = ui16OutData; 
		}
		while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui8Src = (const IMG_UINT8 *)(pui8Src + ui32SrcRowByteIncrement);
	}
	while(--ui32Height);
}

/***********************************************************************************
 Function Name      : CopyTextureBGRA8888to4444
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from BGRA8888 to ARGB4444 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureBGRA8888to4444(IMG_UINT16 *pui16Dest, const IMG_UINT8 *pui8Src,
												IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
												IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
												IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 4 * sizeof(IMG_UINT8));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}


	do
	{
		i = ui32Width;

		do
		{
			/* Blue */
			ui8Temp = (*pui8Src++) >> 4;
			ui16OutData = ui8Temp;

			/* Green */
			ui8Temp = (*pui8Src++) >> 4;
			ui16OutData |= ui8Temp << 4;

			/* Red */
			ui8Temp = (*pui8Src++) >> 4;
			ui16OutData |= ui8Temp << 8;

			/* Alpha */
			ui8Temp = (*pui8Src++) >> 4;
			ui16OutData |= ui8Temp << 12;

			*pui16Dest++ = ui16OutData;

		}while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui8Src += ui32SrcRowByteIncrement;

	}
	while(--ui32Height);
}

#endif /* defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888) */

/***********************************************************************************
 Function Name      : CopyTexturePVRTC
 Inputs             : pui32Src, ui32Width, ui32Height
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies PVRTC texture data
************************************************************************************/
static IMG_VOID CopyTexturePVRTC(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
								IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
								IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
								IMG_BOOL bUseDstStride)
{
	IMG_UINT32 ui32NumBlocks, ui32WidthInBlocks, ui32HeightInBlocks;

	PVR_UNREFERENCED_PARAMETER(ui32SrcStrideInBytes);
	PVR_UNREFERENCED_PARAMETER(bUseDstStride);

	/* Block height is 4 */
	ui32HeightInBlocks = MAX(ui32Height >> 2, 1);

	/* 2Bpp PVRTC encodes 8x4 blocks, 4Bpp PVRTC encodes 4x4 */
	if( (psMipLevel->psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTC2) || 
		(psMipLevel->psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCII2))
	{
		/* Block width is 8 */
		ui32WidthInBlocks = MAX(ui32Width >> 3, 1);
	}
	else
	{
		/* Block width is 4 */
		ui32WidthInBlocks = MAX(ui32Width >> 2, 1);
	}

	ui32NumBlocks = ui32WidthInBlocks * ui32HeightInBlocks;

	if (ui32WidthInBlocks<2)
	{
		/* 
		** Width is less than 2 blocks, so the compressed data includes a replicated block on 
		** the right - Take every second block
		*/
		do
		{
			*pui32Dest++ = pui32Src[0];
			*pui32Dest++ = pui32Src[1];

			pui32Src += 4;
		}
		while(--ui32NumBlocks);
	}
	else
	{
		/* 
		** Just copy the first ui32NumBlocks blocks worth of data. 
		** (If the height is less than 2 blocks, the second ui32NumBlocks blocks worth is a 
		** duplicate of the first)
		*/
		do
		{
			*pui32Dest++ = *pui32Src++;
			*pui32Dest++ = *pui32Src++;
		}
		while(--ui32NumBlocks);
	}
}

/***********************************************************************************
 Function Name      : CopyTextureETC1
 Inputs             : pui32Src, ui32Width, ui32Height
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies ETC1 texture data
************************************************************************************/
static IMG_VOID CopyTextureETC1(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
				IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				 IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i, ui32WidthInBlocks, ui32HeightInBlocks;
	const IMG_UINT8 *pui8Src;
	IMG_UINT8       *pui8Dst;

	PVR_UNREFERENCED_PARAMETER(ui32SrcStrideInBytes);
	PVR_UNREFERENCED_PARAMETER(psMipLevel);
	PVR_UNREFERENCED_PARAMETER(bUseDstStride);

	/* Blocks are 4x4 texels */
	ui32WidthInBlocks  = ALIGNCOUNTINBLOCKS(ui32Width, 2);
	ui32HeightInBlocks = ALIGNCOUNTINBLOCKS(ui32Height, 2);
	pui8Src = (const IMG_UINT8 *)pui32Src;
	pui8Dst = (IMG_UINT8 *)pui32Dest;

	do
	{
		i = ui32WidthInBlocks;

		do
		{
			/* HW byte ordering is other-endian */
			pui8Dst[0] = pui8Src[3];
			pui8Dst[1] = pui8Src[2];
			pui8Dst[2] = pui8Src[1];
			pui8Dst[3] = pui8Src[0];
			pui8Dst[4] = pui8Src[7];
			pui8Dst[5] = pui8Src[6];
			pui8Dst[6] = pui8Src[5];
			pui8Dst[7] = pui8Src[4];

			pui8Src += 8;
			pui8Dst += 8;

		} while(--i);

	} while(--ui32HeightInBlocks);
}


/***********************************************************************************
 Function Name      : CopyTexture32Bits
 Inputs             : pui32Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies 32 bit texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture32Bits(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
					IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT32));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pui32Dest++ = *pui32Src++;

		}while(--i);

		pui32Dest += ui32DstRowIncrement;
		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}

/***********************************************************************************
 Function Name      : CopyTexture16Bits
 Inputs             : pui16Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies 16 bit texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture16Bits(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src,
					IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pui16Dest++ = *pui16Src++;
		}while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}



/***********************************************************************************
 Function Name      : CopyTexture8Bits
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui8Dest
 Returns            : -
 Description        : Copies 8 bit texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture8Bits(IMG_UINT8 *pui8Dest, const IMG_UINT8 *pui8Src,
				       IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				       IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				       IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT8));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pui8Dest++ = *pui8Src++;
		}while(--i);

		pui8Dest += ui32DstRowIncrement;
		pui8Src  += ui32SrcRowByteIncrement;

	}
	while(--ui32Height);
}


#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)

/***********************************************************************************
 Function Name      : CopyTexture16BitIntTo32BitFloat
 Inputs             : pui16Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pf32Dest
 Returns            : -
 Description        : Copies depth texture data from 16-bit unsigned short to 
 					  32-bit float
************************************************************************************/
static IMG_VOID CopyTexture16BitIntTo32BitFloat(IMG_FLOAT *pf32Dest, const IMG_UINT16 *pui16Src,
					IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;

	if (bUseDstStride)
	{
        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pf32Dest++ = GLES_US_TO_FLOAT(*pui16Src++);
		}while(--i);

		pf32Dest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTexture32BitIntTo32BitFloat
 Inputs             : pui32Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pf32Dest
 Returns            : -
 Description        : Copies depth texture data from 32-bit unsigned itn to 
 					  32-bit float
************************************************************************************/
static IMG_VOID CopyTexture32BitIntTo32BitFloat(IMG_FLOAT *pf32Dest, const IMG_UINT32 *pui32Src,
					IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT32));
	IMG_UINT32 ui32DstRowIncrement;

	if (bUseDstStride)
	{
        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pf32Dest++ = GLES_UI_TO_FLOAT(*pui32Src++);
		}while(--i);

		pf32Dest += ui32DstRowIncrement;
		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}

#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */

#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) 
/***********************************************************************************
 Function Name      : CopyTexture24_8DepthStencilTo32BitFloat8BitInt
 Inputs             : pui32Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pf32Dest
 Returns            : -
 Description        : Copies packed 24_8 depth stencil data to 32-bit float and
					  8 bit stencil
************************************************************************************/
static IMG_VOID CopyTexture24_8DepthStencilTo32BitFloat8BitInt( IMG_FLOAT *pf32Dest, 
																const IMG_UINT32 *pui32Src,
																IMG_UINT32 ui32Width, 
																IMG_UINT32 ui32Height,
																IMG_UINT32 ui32SrcStrideInBytes, 
																GLES2MipMapLevel *psMipLevel,
																IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT8 *pui8Dest;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT32));
	IMG_UINT32 ui32DstRowIncrement;

	IMG_UINT32 ui32Offset;

	ui32Offset = psMipLevel->ui32Width * psMipLevel->ui32Height * 4;

	pui8Dest = (IMG_UINT8 *)pf32Dest + ui32Offset;

	if (bUseDstStride)
	{
        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			/*
			** UNSIGNED_INT_24_8_OES
			**
			** 31 30 29 28 27 26 ... 12 11 10 9 8 7 6 5 4 3 2 1 0
			** +----------------------------------+---------------+
			** |           1st Component          | 2nd Component |
			** +----------------------------------+---------------+
			*/
			IMG_UINT32 ui32SrcData = *pui32Src++;

			*pf32Dest++ = GLES_UI24_TO_FLOAT((ui32SrcData >> 8));

			*pui8Dest++ = (IMG_UINT8)(ui32SrcData & 0xFF);

		} while(--i);

		pf32Dest += ui32DstRowIncrement;
		pui8Dest += ui32DstRowIncrement;

		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)pui32Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}

#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

/***********************************************************************************
 Function Name      : CopyTexture5551
 Inputs             : pui16Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 5551 to RGBA 1555 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture5551(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src,
				      IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				      IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				      IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			ui16Temp = *pui16Src++; 
			*pui16Dest++ = (IMG_UINT16)((ui16Temp << 15) | (ui16Temp >> 1));

		}while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}

/***********************************************************************************
 Function Name      : CopyTexture4444
 Inputs             : pui16Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 4444 to RGBA 4444 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture4444(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src,
				      IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				      IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				      IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			ui16Temp = *pui16Src++; 
			*pui16Dest++ = (IMG_UINT16)((ui16Temp << 12) | (ui16Temp >> 4));

		}while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);

	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTexture888X
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui8Dest
 Returns            : -
 Description        : Copies texture data from RGB UB to RGBA8888 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture888X(IMG_UINT8 *pui8Dest, const IMG_UINT8 *pui8Src,
				      IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				      IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
				      IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 3 * sizeof(IMG_UINT8));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	    ui32DstRowIncrement = (psMipLevel->ui32Width - ui32Width) << 2;
	}
	else
	{
	    ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			*pui8Dest++ = *pui8Src++;
			*pui8Dest++ = *pui8Src++;
			*pui8Dest++ = *pui8Src++;
			*pui8Dest++ = 0xFF;

		}while(--i);

		pui8Dest += ui32DstRowIncrement;
		pui8Src += ui32SrcRowByteIncrement;

	}
	while(--ui32Height);
}

/***********************************************************************************
 Function Name      : CopyTexture888to565
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGB888 to RGB565 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture888to565(IMG_UINT16 *pui16Dest, const IMG_UINT8 *pui8Src,
					  IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					  IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					  IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Red, ui8Green, ui8Blue;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 3 * sizeof(IMG_UINT8));
	IMG_UINT32 ui32DstRowIncrement;
	

	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			ui8Red	 = (*pui8Src++) >> 3;
			ui8Green = (*pui8Src++) >> 2;
			ui8Blue	 = (*pui8Src++) >> 3;

			ui16OutData = (ui8Red << 11) | (ui8Green << 5) | ui8Blue;
			*pui16Dest++ = ui16OutData; 

		}while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui8Src += ui32SrcRowByteIncrement;

	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTexture565toRGBX8888
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGB 565 to RGBX 8888 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture565toRGBX8888(IMG_UINT8 *pui8Dest, const IMG_UINT16 *pui16Src,
					       IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					       IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					       IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 ui16Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;
	

	if (bUseDstStride)
	{
	        ui32DstRowIncrement = (psMipLevel->ui32Width - ui32Width) << 2;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i=ui32Width;
		
		do
		{
			ui16Temp=*pui16Src++;
		
			/* Red */
			ui8OutData  = (IMG_UINT8)((ui16Temp >> 11) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++ = ui8OutData;

			/* Green */
			ui8OutData  = (IMG_UINT8)((ui16Temp >>  5) << 2);
			ui8OutData = ui8OutData | (ui8OutData >> 6);
			*pui8Dest++ = ui8OutData;

			/* Blue */
			ui8OutData   = (IMG_UINT8)((ui16Temp >> 0) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++  = ui8OutData;

			/* Alpha */
			*pui8Dest++ = 0xFF;
		}
		while(--i);

		pui8Dest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);
	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTexture5551TO4444
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 5551 to ARGB 4444 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture5551to4444(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src, 
					    IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					    IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					    IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	
	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i=ui32Width;
		
		do
		{
			/* Red */
			*pui16Dest  = ((*pui16Src & 0xF800U) >> 12) << 8;

			/* Green */
			*pui16Dest |= ((*pui16Src & 0x07C0) >> 7) << 4; 

			/* Blue */
			*pui16Dest |= ((*pui16Src & 0x003E) >> 2) << 0;

			/* Alpha */
			*pui16Dest |= (*pui16Src & 0x0001) ? 0xF000U : 0x0000; 

			pui16Dest++;
			pui16Src++;
		}
		while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);
	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTexture5551toBGRA8888
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 5551 to BGRA 8888 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture5551toBGRA8888(IMG_UINT8 *pui8Dest, const IMG_UINT16 *pui16Src,
						IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
						IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 ui16Temp;
	
	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = (psMipLevel->ui32Width - ui32Width) << 2;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i=ui32Width;
		
		do
		{
			ui16Temp=*pui16Src++;

			/* Blue */
			ui8OutData   = (IMG_UINT8)((ui16Temp >> 1) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++  = ui8OutData;

			/* Green */
			ui8OutData  = (IMG_UINT8)((ui16Temp >>  6) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++ = ui8OutData;

			/* Red */
			ui8OutData = (IMG_UINT8)((ui16Temp >> 11) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++ = ui8OutData;

			/* Alpha */
			ui8OutData   = (ui16Temp & 0x0001) ? 0xFFU : 0x0;
			*pui8Dest++  = ui8OutData;
		}
		while(--i);

		pui8Dest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);
	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTexture5551toRGBA8888
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 5551 to RGBA 8888 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture5551toRGBA8888(IMG_UINT8 *pui8Dest, const IMG_UINT16 *pui16Src,
						IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
						IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 ui16Temp;
	
	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = (psMipLevel->ui32Width - ui32Width) << 2;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i=ui32Width;

		do
		{
			ui16Temp=*pui16Src++;

			/* Red */
			ui8OutData  = (IMG_UINT8)((ui16Temp >> 11) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++ = ui8OutData;

			/* Green */
			ui8OutData  = (IMG_UINT8)((ui16Temp >>  6) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++ = ui8OutData;

			/* Blue */
			ui8OutData   = (IMG_UINT8)((ui16Temp >> 1) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++  = ui8OutData;

			/* Alpha */
			ui8OutData   = (ui16Temp & 0x0001) ? 0xFFU : 0x0;
			*pui8Dest++  = ui8OutData;
		}
		while(--i);

		pui8Dest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);
	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTexture4444TO5551
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 4444 to ARGB 1555 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture4444to5551(IMG_UINT16 *pui16Dest, const IMG_UINT16 *pui16Src,
					    IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
					    IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
					    IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i=ui32Width;
		
		do
		{
			/* Red */
			*pui16Dest = ((*pui16Src & 0xF000U) >> 12) << 11;
			*pui16Dest = *pui16Dest | ((*pui16Dest & 0x4000U) >> 4);

			/* Green */
			*pui16Dest |= ((*pui16Src & 0x0F00) >> 8) << 6; 
			*pui16Dest = *pui16Dest | ((*pui16Dest & 0x0200U) >> 4);

			/* Blue */
			*pui16Dest |= ((*pui16Src & 0x00F0) >> 4) << 1;
			*pui16Dest = *pui16Dest | ((*pui16Dest & 0x0010U) >> 4);

			/* Alpha */
			*pui16Dest |= ((*pui16Src & 0x000F) >> 3) << 15 ; 

			pui16Src++;
			pui16Dest++;
		}
		while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);
	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTexture4444toRGBA8888
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 4444 to RGBA 8888 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture4444toRGBA8888(IMG_UINT8 *pui8Dest, const IMG_UINT16 *pui16Src, 
						IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
						IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 ui16Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = (psMipLevel->ui32Width - ui32Width) << 2;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i=ui32Width;
	
		do
		{
			ui16Temp = *pui16Src++;
	
			/* Red */
			ui8OutData  = (IMG_UINT8)(ui16Temp >> 12);
			ui8OutData |= (ui8OutData << 4);
			*pui8Dest++ = ui8OutData;

			/* Green */
			ui8OutData  = (IMG_UINT8)(ui16Temp >>  8);
			ui8OutData &= 0x0F;
			ui8OutData |= (ui8OutData << 4);
			*pui8Dest++ = ui8OutData;

			/* Blue */
			ui8OutData   = (IMG_UINT8)(ui16Temp >> 4);
			ui8OutData &= 0x0F;
			ui8OutData |= (ui8OutData << 4);
			*pui8Dest++  = ui8OutData;

			/* Alpha */
			ui8OutData  = (IMG_UINT8)(ui16Temp >>  0);
			ui8OutData &= 0x0F;
			ui8OutData |= (ui8OutData << 4);
			*pui8Dest++ = ui8OutData;
		}
		while(--i);

		pui8Dest += ui32DstRowIncrement;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)pui16Src + ui32SrcRowByteIncrement);
	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTextureRGBA8888TO5551
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 8888 to ARGB 1555 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureRGBA8888to5551(IMG_UINT16 *pui16Dest, const IMG_UINT8 *pui8Src,
						IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
						IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 4 * sizeof(IMG_UINT8));
	IMG_UINT32 ui32DstRowIncrement;



	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
	
		i = ui32Width;

		do
		{
			/* Red */
			ui8Temp = (*pui8Src++) >> 3; 
			ui16OutData = ui8Temp  << 10;

			/* Green */
			ui8Temp = (*pui8Src++) >> 3; 
			ui16OutData |= ui8Temp << 5;

			/* Blue */
			ui8Temp = (*pui8Src++) >> 3; 
			ui16OutData |= ui8Temp << 0;

			/* Alpha */
			ui8Temp = (*pui8Src++) >> 7; 
			ui16OutData |= ui8Temp << 15;

			*pui16Dest++ = ui16OutData; 
		}
		while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui8Src = (const IMG_UINT8 *)(pui8Src + ui32SrcRowByteIncrement);
	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : CopyTexture8888to4444
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA8888 to ARGB4444 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureRGBA8888to4444(IMG_UINT16 *pui16Dest, const IMG_UINT8 *pui8Src,
						IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLES2MipMapLevel *psMipLevel,
						IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowByteIncrement = ui32SrcStrideInBytes - (ui32Width * 4 * sizeof(IMG_UINT8));
	IMG_UINT32 ui32DstRowIncrement;


	if (bUseDstStride)
	{
	        ui32DstRowIncrement = psMipLevel->ui32Width - ui32Width;
	}
	else
	{
	        ui32DstRowIncrement = 0;
	}

	do
	{
		i = ui32Width;

		do
		{
			/* Red */
			ui8Temp = (*pui8Src++) >> 4;
			ui16OutData = ui8Temp << 8;

			/* Green */
			ui8Temp = (*pui8Src++) >> 4;
			ui16OutData |= ui8Temp << 4;

			/* Blue */
			ui8Temp = (*pui8Src++) >> 4;
			ui16OutData |= ui8Temp;

			/* Alpha */
			ui8Temp = (*pui8Src++) >> 4;
			ui16OutData |= ui8Temp << 12;

			*pui16Dest++ = ui16OutData; 
 
		}while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui8Src += ui32SrcRowByteIncrement;

	}
	while(--ui32Height);
}


/***********************************************************************************
 Function Name      : TexParameterfv
 Inputs             : gc, eTarget, ePname, fParam, ui32Type, vecVersion
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets texture parameter state for the currently bound
					  texture.
************************************************************************************/
static IMG_VOID TexParameterfv(GLES2Context *gc, GLenum eTarget, GLenum ePname, const void *pvParams, IMG_UINT32 ui32Type, IMG_BOOL vecVersion)
{
	GLES2Texture *psTex;
	GLES2TextureParamState *psParamState;
	IMG_UINT32 ui32Target;
	GLenum eParam;

	PVR_UNREFERENCED_PARAMETER(vecVersion);

	switch(eTarget)
	{
		case GL_TEXTURE_2D:
		{
			ui32Target = GLES2_TEXTURE_TARGET_2D;
			break;
		}
		case GL_TEXTURE_CUBE_MAP:
		{
			ui32Target = GLES2_TEXTURE_TARGET_CEM;
			break;
		}
#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_STREAM_IMG:
#endif
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_EXTERNAL_OES:
#endif
#if defined(GLES2_EXTENSION_TEXTURE_STREAM) || defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
		{
			ConvertData(ui32Type, pvParams, GLES_ENUM, &eParam, 1);

			ui32Target = GLES2_TEXTURE_TARGET_STREAM;

			switch (ePname)
			{
				/* Wrap modes are restricted for stream textures */
				case GL_TEXTURE_WRAP_S:
				{
					switch (eParam) 
					{
						case GL_CLAMP_TO_EDGE:
						{
							break;
						}
						default:
						{
							goto bad_enum;
						}
					}

					break;
				}
				case GL_TEXTURE_WRAP_T:
				{
					switch (eParam) 
					{
						case GL_CLAMP_TO_EDGE:
						{
							break;
						}
						default:
						{
							goto bad_enum;
						}
					}

					break;
				}
				/* Filter modes are restricted for stream textures */
				case GL_TEXTURE_MIN_FILTER:
				{
					switch (eParam) 
					{
						case GL_NEAREST:
						case GL_LINEAR:
						{
							break;
						}
						default:
						{
							goto bad_enum;
						}
					}

					break;
				}
				case GL_TEXTURE_MAG_FILTER:
				{
					break;
				}
				default:
				{
					goto bad_enum;
				}
			}

			break;
		}
#endif
		default:
		{
bad_enum:
			SetError(gc, GL_INVALID_ENUM);
			return;
		}
	}

	psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture][ui32Target];
	psParamState = &psTex->sState;

	switch (ePname) 
	{
		/* Wrap modes are stored directly in HW format */
		case GL_TEXTURE_WRAP_S:
		{
			ConvertData(ui32Type, pvParams, GLES_ENUM, &eParam, 1);

			switch (eParam) 
			{
				case GL_REPEAT:
				{
					psParamState->ui32StateWord0 &= EURASIA_PDS_DOUTT0_UADDRMODE_CLRMSK;
					psParamState->ui32StateWord0 |= EURASIA_PDS_DOUTT0_UADDRMODE_REPEAT;

					break;
				}
				case GL_CLAMP_TO_EDGE:
				{
					psParamState->ui32StateWord0 &= EURASIA_PDS_DOUTT0_UADDRMODE_CLRMSK;
					psParamState->ui32StateWord0 |= EURASIA_PDS_DOUTT0_UADDRMODE_CLAMP;

					break;
				}
				case GL_MIRRORED_REPEAT:
				{
					psParamState->ui32StateWord0 &= EURASIA_PDS_DOUTT0_UADDRMODE_CLRMSK;
					psParamState->ui32StateWord0 |= EURASIA_PDS_DOUTT0_UADDRMODE_FLIP;

					break;
				}
				default:
				{
					goto bad_enum;
				}
			}
			psTex->ui32LevelsConsistent = GLES2_TEX_UNKNOWN;

			break;
		}
		case GL_TEXTURE_WRAP_T:
		{
			ConvertData(ui32Type, pvParams, GLES_ENUM, &eParam, 1);

			switch (eParam) 
			{
				case GL_REPEAT:
				{
					psParamState->ui32StateWord0 &= EURASIA_PDS_DOUTT0_VADDRMODE_CLRMSK;
					psParamState->ui32StateWord0 |= EURASIA_PDS_DOUTT0_VADDRMODE_REPEAT;

					break;
				}
				case GL_CLAMP_TO_EDGE:
				{
					psParamState->ui32StateWord0 &= EURASIA_PDS_DOUTT0_VADDRMODE_CLRMSK;
					psParamState->ui32StateWord0 |= EURASIA_PDS_DOUTT0_VADDRMODE_CLAMP;

					break;
				}
				case GL_MIRRORED_REPEAT:
				{
					psParamState->ui32StateWord0 &= EURASIA_PDS_DOUTT0_VADDRMODE_CLRMSK;
					psParamState->ui32StateWord0 |= EURASIA_PDS_DOUTT0_VADDRMODE_FLIP;

					break;
				}
				default:
				{
					goto bad_enum;
				}
			}
			psTex->ui32LevelsConsistent = GLES2_TEX_UNKNOWN;

			break;
		}

		/* Min filter is stored directly in HW format */
		case GL_TEXTURE_MIN_FILTER:
		{
			ConvertData(ui32Type, pvParams, GLES_ENUM, &eParam, 1);

			switch (eParam)
			{
				case GL_NEAREST:
				{
					psParamState->ui32MinFilter =	EURASIA_PDS_DOUTT0_NOTMIPMAP |
													EURASIA_PDS_DOUTT0_MINFILTER_POINT;
					break;
				}
				case GL_LINEAR:
				{
					psParamState->ui32MinFilter =	EURASIA_PDS_DOUTT0_NOTMIPMAP |
													EURASIA_PDS_DOUTT0_MINFILTER_LINEAR;
					break;
				}
				case GL_NEAREST_MIPMAP_NEAREST:
				{
					psParamState->ui32MinFilter =	EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX |
													EURASIA_PDS_DOUTT0_MINFILTER_POINT;
					break;
				}
				case GL_LINEAR_MIPMAP_NEAREST:
				{
					psParamState->ui32MinFilter =	EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX |
													EURASIA_PDS_DOUTT0_MINFILTER_LINEAR;
					break;
				}
				case GL_NEAREST_MIPMAP_LINEAR:
				{
					psParamState->ui32MinFilter =	EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX |
													EURASIA_PDS_DOUTT0_MINFILTER_POINT |
													EURASIA_PDS_DOUTT0_MIPFILTER;
					break;
				}
				case GL_LINEAR_MIPMAP_LINEAR:
				{
					psParamState->ui32MinFilter =	EURASIA_PDS_DOUTT0_MIPMAPCLAMP_MAX |
													EURASIA_PDS_DOUTT0_MINFILTER_LINEAR |
													EURASIA_PDS_DOUTT0_MIPFILTER;
					break;
				}
				default:
				{
					goto bad_enum;
				}
			}

			psTex->ui32LevelsConsistent = GLES2_TEX_UNKNOWN;

			break;
		}
		/* Min filter is stored directly in HW format */
		case GL_TEXTURE_MAG_FILTER:
		{
			ConvertData(ui32Type, pvParams, GLES_ENUM, &eParam, 1);

			switch (eParam)
			{
				case GL_NEAREST:
				{
					psParamState->ui32MagFilter = EURASIA_PDS_DOUTT0_MAGFILTER_POINT;

					break;
				}
				case GL_LINEAR:
				{
					psParamState->ui32MagFilter = EURASIA_PDS_DOUTT0_MAGFILTER_LINEAR;

					break;
				}
				default:
				{
					goto bad_enum;
				}
			}

			break;
		}
		default:
		{
			goto bad_enum;
		}
	}

	gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;
}


/***********************************************************************************
 Function Name      : glTexParameteri
 Inputs             : target, pname, param
 Outputs            : textures
 Returns            : -
 Description        : ENTRYPOINT: Sets texture parameter state for currently bound
					  texture.
************************************************************************************/

GL_APICALL void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexParameteri"));

	GLES2_TIME_START(GLES2_TIMES_glTexParameteri);

	TexParameterfv(gc, target, pname, (const void *)&param, GLES_INT32, IMG_FALSE);

	GLES2_TIME_STOP(GLES2_TIMES_glTexParameteri);
}

/***********************************************************************************
 Function Name      : glTexParameterf
 Inputs             : target, pname, param
 Outputs            : textures
 Returns            : -
 Description        : ENTRYPOINT: Sets texture parameter state for currently bound
					  texture.
************************************************************************************/

GL_APICALL void GL_APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexParameterf"));

	GLES2_TIME_START(GLES2_TIMES_glTexParameterf);

	TexParameterfv(gc, target, pname, (const void *)&param, GLES_FLOAT, IMG_FALSE);

	GLES2_TIME_STOP(GLES2_TIMES_glTexParameterf);
}

/***********************************************************************************
 Function Name      : glTexParameteriv
 Inputs             : target, pname, params
 Outputs            : textures
 Returns            : -
 Description        : ENTRYPOINT: Sets texture parameter state for currently bound
					  texture.
************************************************************************************/

GL_APICALL void GL_APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexParameteriv"));

	GLES2_TIME_START(GLES2_TIMES_glTexParameteriv);

	TexParameterfv(gc, target, pname, (const void *)params, GLES_INT32, IMG_TRUE);

	GLES2_TIME_STOP(GLES2_TIMES_glTexParameteriv);
}

/***********************************************************************************
 Function Name      : glTexParameterfv
 Inputs             : target, pname, params
 Outputs            : textures
 Returns            : -
 Description        : ENTRYPOINT: Sets texture parameter state for currently bound
					  texture.
************************************************************************************/

GL_APICALL void GL_APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexParameterfv"));

	GLES2_TIME_START(GLES2_TIMES_glTexParameterfv);

	TexParameterfv(gc, target, pname, (const void *)params, GLES_FLOAT, IMG_TRUE);

	GLES2_TIME_STOP(GLES2_TIMES_glTexParameterfv);
}


/***********************************************************************************
 Function Name      : glActiveTexture
 Inputs             : texture
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets active texture unit.
					  We need to store this for the state machine, although HW makes
					  no use of it.
************************************************************************************/

GL_APICALL void GL_APIENTRY glActiveTexture(GLenum texture)
{
	IMG_UINT32 ui32Unit = texture - GL_TEXTURE0;
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glActiveTexture"));

	GLES2_TIME_START(GLES2_TIMES_glActiveTexture);

	if (ui32Unit >= GLES2_MAX_TEXTURE_UNITS)
	{
		SetError(gc, GL_INVALID_ENUM);
		return;
	}
	gc->sState.sTexture.ui32ActiveTexture = ui32Unit;
	gc->sState.sTexture.psActive = &gc->sState.sTexture.asUnit[ui32Unit];

	GLES2_TIME_STOP(GLES2_TIMES_glActiveTexture);
}


/***********************************************************************************
 Function Name      : BindTexture
 Inputs             : gc, ui32Unit, ui32Target, ui32Texture
 Outputs            : -
 Returns            : Success
 Description        : Sets current texture for subsequent calls.
					  Will create an internal psTex structure, but no texture data
					  memory is allocated yet. Uses name table.
************************************************************************************/
IMG_INTERNAL IMG_BOOL BindTexture(GLES2Context *gc, IMG_UINT32 ui32Unit, IMG_UINT32 ui32Target, IMG_UINT32 ui32Texture)
{
	GLES2Texture *psTex;
	GLES2Texture *psBoundTexture;
	GLES2NamesArray *psNamesArray;

	GLES_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ];
	/*
	** Retrieve the texture object from the psNamesArray structure.
	*/
	if (ui32Texture == 0)
	{
		psTex = gc->sTexture.psDefaultTexture[ui32Target];
		GLES_ASSERT(IMG_NULL != psTex);
		GLES_ASSERT(0 == psTex->sNamedItem.ui32Name);
	}
	else
	{
		psTex = (GLES2Texture *) NamedItemAddRef(psNamesArray, ui32Texture);
	}

	/*
	** Is this the first time this ui32Name has been bound?
	** If so, create a new texture object and initialize it.
	*/
	if (IMG_NULL == psTex) 
	{
		psTex = CreateTexture(gc, ui32Texture, ui32Target);

		if(!psTex)
		{
			SetError(gc, GL_OUT_OF_MEMORY);
			return IMG_FALSE;
		}

		GLES_ASSERT(ui32Texture == psTex->sNamedItem.ui32Name);

		if(!InsertNamedItem(psNamesArray, (GLES2NamedItem*)psTex))
		{
			/* Use names array free */
			psNamesArray->pfnFree(gc, (GLES2NamedItem*)psTex, IMG_TRUE);
			SetError(gc, GL_OUT_OF_MEMORY);
			return IMG_FALSE;
		}


		/*
		** Now lock to increment refcount.
		*/
		NamedItemAddRef(psNamesArray, ui32Texture);
	}
	else
	{
		/*
		** Retrieved an existing texture object.  Do some
		** sanity checks.
		*/
		GLES_ASSERT(ui32Texture == psTex->sNamedItem.ui32Name);

		/* Must be same type as target it is being bound to */
		if(ui32Target != psTex->ui32TextureTarget)
		{
			SetError(gc, GL_INVALID_OPERATION);
			NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psTex);
			return IMG_FALSE;
		}
	}

	/*
	** Release texture that is being unbound.
	*/
	psBoundTexture = gc->sTexture.apsBoundTexture[ui32Unit][ui32Target];

	if (psBoundTexture && (psBoundTexture->sNamedItem.ui32Name != 0)) 
	{
		/* Ensure that newly bound texture will be validated (cheaply) for texture address */
		if(ui32Target == GLES2_TEXTURE_TARGET_STREAM)
		{
			TextureRemoveResident(gc, psTex);
		}

#if defined(GLES2_EXTENSION_EGL_IMAGE)
		if(psBoundTexture->psEGLImageTarget)
		{
			gc->ui32NumEGLImageTextureTargetsBound--;
		}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

		NamedItemDelRef(gc, psNamesArray, (GLES2NamedItem*)psBoundTexture);
	}

	/*
	** Install the new texture into the correct target in this context.
	*/
	gc->sState.sTexture.asUnit[ui32Unit].psTexture[ui32Target] = &psTex->sState;
	gc->sTexture.apsBoundTexture[ui32Unit][ui32Target] = psTex;

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psBoundTexture != psTex)
	{
		if(psTex->psEGLImageTarget)
		{
			gc->ui32NumEGLImageTextureTargetsBound++;
		}
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : glBindTexture
 Inputs             : target, texture
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current texture for subsequent calls.
************************************************************************************/
GL_APICALL void GL_APIENTRY glBindTexture(GLenum target, GLuint texture)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBindTexture"));

	GLES2_TIME_START(GLES2_TIMES_glBindTexture);

	switch(target)
	{
		case GL_TEXTURE_2D:
		{
			if(BindTexture(gc, gc->sState.sTexture.ui32ActiveTexture, GLES2_TEXTURE_TARGET_2D, texture) != IMG_TRUE)
			{
				GLES2_TIME_STOP(GLES2_TIMES_glBindTexture);
				return;
			}

			break;
		}
		case GL_TEXTURE_CUBE_MAP:
		{
			if(BindTexture(gc, gc->sState.sTexture.ui32ActiveTexture, GLES2_TEXTURE_TARGET_CEM, texture) != IMG_TRUE)
			{
				GLES2_TIME_STOP(GLES2_TIMES_glBindTexture);
				return;
			}

			break;
		}

#if defined(GLES2_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_STREAM_IMG:
#endif
#if defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_EXTERNAL_OES:
#endif
#if defined(GLES2_EXTENSION_TEXTURE_STREAM) || defined(GLES2_EXTENSION_EGL_IMAGE_EXTERNAL)
		{
			if(BindTexture(gc, gc->sState.sTexture.ui32ActiveTexture, GLES2_TEXTURE_TARGET_STREAM, texture) != IMG_TRUE)
			{
				GLES2_TIME_STOP(GLES2_TIMES_glBindTexture);

				return;
			}

			break;
		}
#endif		
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
		}
	}

	gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;

	GLES2_TIME_STOP(GLES2_TIMES_glBindTexture);
}


/***********************************************************************************
 Function Name      : glDeleteTextures
 Inputs             : n, textures
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Deletes a list of n textures.
					  Deletes internal psTex structures for named textures and if they
					  were currently bound, binds default texture.
************************************************************************************/
GL_APICALL void GL_APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures)
{
	GLES2NamesArray *psNamesArray;
	IMG_INT32       i;
	IMG_UINT32      j,k;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDeleteTextures"));

	GLES2_TIME_START(GLES2_TIMES_glDeleteTextures);

	if(!textures)
	{
		/* The application developer is having a bad day */
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteTextures);
		return;
	}

	if(n < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteTextures);
		return;
	}
	else if (n == 0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glDeleteTextures);
		return;
	}

	psNamesArray = gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ];



	/*
	** If a texture that is being deleted is currently bound,
	** bind the default texture to its target.
	*/
	for(i=0; i < n; i++)
	{
		/*
		* If the texture is currently bound, bind the default texture
		* to its target. 
		*/
		for (j=0; j < GLES2_MAX_TEXTURE_UNITS; j++) 
		{
			for(k=0; k < GLES2_TEXTURE_TARGET_MAX; k++)
			{
				GLES2Texture *psTex = gc->sTexture.apsBoundTexture[j][k];
				
				/* Is the texture currently bound? */
				if (psTex->sNamedItem.ui32Name == textures[i]) 
				{

					/* bind the default texture to this target */
					if(BindTexture(gc, j, k, 0) != IMG_TRUE)
					{
						SetError(gc, GL_OUT_OF_MEMORY);
						GLES2_TIME_STOP(GLES2_TIMES_glDeleteTextures);
						return;
					}

					break;
				}
			}
		}

		/*
		 * If the texture is currently attached to the currently bound framebuffer, 
		 * detach it.  See section 4.4.2.2 of GL_EXT_framebuffer_object.
		 */
		RemoveFrameBufferAttachment(gc, IMG_FALSE, textures[i]);
	}

	NamedItemDelRefByName(gc, psNamesArray, (IMG_UINT32)n, (const IMG_UINT32*)textures);


	gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;

	GLES2_TIME_STOP(GLES2_TIMES_glDeleteTextures);
}


/***********************************************************************************
 Function Name      : glGenTextures
 Inputs             : n
 Outputs            : textures
 Returns            : -
 Description        : ENTRYPOINT: Generates a set of n names for textures
************************************************************************************/
GL_APICALL void GL_APIENTRY glGenTextures(GLsizei n, GLuint *textures)
{
	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGenTextures"));

	GLES2_TIME_START(GLES2_TIMES_glGenTextures);

	if (n < 0) 
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glGenTextures);
		return;
	}
	else if (n == 0)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGenTextures);
		return;
	}
	
	if (textures == IMG_NULL) 
	{
		GLES2_TIME_STOP(GLES2_TIMES_glGenTextures);
		return;
	}

	GLES_ASSERT(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ] != IMG_NULL);

	NamesArrayGenNames(gc->psSharedState->apsNamesArray[GLES2_NAMETYPE_TEXOBJ], (IMG_UINT32)n, (IMG_UINT32 *)textures);

	GLES2_TIME_STOP(GLES2_TIMES_glGenTextures);
}


/***********************************************************************************
 Function Name      : IsLegalRange
 Inputs             : gc, target, w, h, border, level
 Outputs            : -
 Returns            : Whether the parameters are legal
 Description        : UTILITY: Checks some texture parameters for legality.
************************************************************************************/

static IMG_BOOL IsLegalRangeTex(GLES2Context *gc, IMG_UINT32 ui32Target,
								GLsizei w, GLsizei h, GLint border, GLint level)
{
	/* The texture spec says to return bad value 
	 * if a border is specified on a non-compressed texture (or a PVRTC texture)
	 */
	if(border)
	{
bad_value:
		SetError(gc, GL_INVALID_VALUE);
		return IMG_FALSE;
	}

	/* Neither width nor height can be negative */
	if(w < 0 || h < 0)
	{
		goto bad_value;
	}

	/* The texture must not be too large */
	if((IMG_UINT32)w > GLES2_MAX_TEXTURE_SIZE || (IMG_UINT32)h > GLES2_MAX_TEXTURE_SIZE)
	{
		goto bad_value;
	}

	/* The level must be in range */
	if(level < 0 || level >= (GLint)GLES2_MAX_TEXTURE_MIPMAP_LEVELS) 
	{
		goto bad_value;
	}

	/* If the mipmap level is nonzero, its size must be a power of two */
	if(level && ((((IMG_UINT32)w & ((IMG_UINT32)w-1)) != 0) || (((IMG_UINT32)h & ((IMG_UINT32)h-1)) != 0)))
	{
			goto bad_value;
	}

	/* For cubemaps, the levels must be square */
	if((ui32Target == GLES2_TEXTURE_TARGET_CEM) && (w != h))
	{
		goto bad_value;
	}

	return IMG_TRUE;
}



/***********************************************************************************
 Function Name      : CheckTexImageArgs
 Inputs             : gc, target, level, width, height, border
 Outputs            : -
 Returns            : texture pointer
 Description        : UTILITY: Checks [copy]teximage params for validity.
************************************************************************************/

static GLES2Texture *CheckTexImageArgs(GLES2Context *gc, GLenum target, GLint level,
									   GLsizei width, GLsizei height, GLint border,
									   IMG_UINT32 *pui32Face, IMG_UINT32 *pui32Level)
{
	GLES2Texture *psTex;
	IMG_UINT32 ui32Target, ui32Face, ui32Level = (IMG_UINT32)level;
	
	ui32Face = 0;

	switch(target)
	{
		case GL_TEXTURE_2D:
			ui32Target = GLES2_TEXTURE_TARGET_2D;
			break;
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		{
			ui32Target = GLES2_TEXTURE_TARGET_CEM;
			ui32Face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X + GLES2_TEXTURE_CEM_FACE_POSX;
			ui32Level = (ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS) + (IMG_UINT32)level;
			break;
		}
		default:
bad_enum:
			SetError(gc, GL_INVALID_ENUM);
			return IMG_NULL;
	}

	if(!IsLegalRangeTex(gc, ui32Target, width, height, border, level))
		return IMG_NULL;

	psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture][ui32Target];

	if(!psTex)
	{
		goto bad_enum;
	}

	*pui32Level = ui32Level;
	*pui32Face = ui32Face;

	return psTex;
}


/***********************************************************************************
 Function Name      : CheckTexSubImageArgs
 Inputs             : gc, target, lod, x, y, width, height, psSubTexFormat  
 Outputs            : pui32Face, pui32Level
 Returns            : texture pointer
 Description        : UTILITY: Checks [copy]texsubimage params for validity.
************************************************************************************/
static GLES2Texture *CheckTexSubImageArgs(GLES2Context *gc, GLenum target, GLint lod, GLint x, GLint y,
										  GLsizei width, GLsizei height, const GLES2TextureFormat *psSubTexFormat,
										  IMG_UINT32 *pui32Face, IMG_UINT32 *pui32Level)
{
	GLES2Texture *psTex;
	IMG_UINT32 ui32Target, ui32Face, ui32Level = (IMG_UINT32)lod;
	IMG_INT32 i32MipWidth, i32MipHeight;
	const GLES2TextureFormat *psTargetTexFormat;

	ui32Face = 0;

	/* The mipmap level must be in range */
	if ((lod < 0) || (lod >= (GLint)GLES2_MAX_TEXTURE_MIPMAP_LEVELS))
	{
		goto bad_value;
	}
	/* Negative values are invalid */
	if ((x < 0) || (y < 0) || (width < 0) || (height < 0))
	{
bad_value:
		SetError(gc, GL_INVALID_VALUE);
		return IMG_NULL;
	}

	switch(target)
	{
		case GL_TEXTURE_2D:
		{
			ui32Target = GLES2_TEXTURE_TARGET_2D;
			break;
		}
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
		{
			ui32Target = GLES2_TEXTURE_TARGET_CEM;
			ui32Face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X + GLES2_TEXTURE_CEM_FACE_POSX;
			ui32Level = (ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS) + (IMG_UINT32)lod;
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			return IMG_NULL;
		}
	}

	psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture][ui32Target];

	GLES_ASSERT(psTex != NULL);

	/* Get the format of the target texture level */
	psTargetTexFormat = psTex->psMipLevel[ui32Level].psTexFormat;

	/* No (Copy/Compressed)TexImage2D, has been called before (Copy/Compressed)TexSubImage2D for this level. */
	if(!psTargetTexFormat)
	{
bad_operation:
		SetError(gc, GL_INVALID_OPERATION);
		return IMG_NULL;
	}
	
	/* (Compressed)TexImage2D with a different base format has been called before (Compressed)TexSubImage2D 
	 *  for this level. A NULL psSubTexFormat is used to denote a CopyTexSubImage2D, which is handled differently.
	 */
	if(psSubTexFormat && (psSubTexFormat->ui32BaseFormatIndex != psTargetTexFormat->ui32BaseFormatIndex))
	{
		goto bad_operation;
	}

	i32MipWidth  = (IMG_INT32)psTex->psMipLevel[ui32Level].ui32Width;
	i32MipHeight = (IMG_INT32)psTex->psMipLevel[ui32Level].ui32Height;

	GLES_ASSERT(i32MipWidth  >= 0);
	GLES_ASSERT(i32MipHeight >= 0);

	/* PVRTC */
	if(psTargetTexFormat->ui32TotalBytesPerTexel > 4 && psTargetTexFormat->ui32NumChunks == 1)
	{
		/* The region must cover the entire mipmap */
		if((x > 0) || (y > 0))
		{
			goto bad_operation;
		}

		if(width != i32MipWidth || height != i32MipHeight)
		{
			goto bad_operation;
		}
	}
	else
	{
		/* The start coordinates must be within the mipmap */
		if(x > i32MipWidth || y > i32MipHeight)
		{
			goto bad_value;
		}

		/* The region must not be too large */
		if((IMG_UINT32)width > GLES2_MAX_TEXTURE_SIZE || (IMG_UINT32)height > GLES2_MAX_TEXTURE_SIZE)
		{
			goto bad_value;
		}

		/* At this point, we know that these are true */
		GLES_ASSERT(x <= (IMG_INT32)GLES2_MAX_TEXTURE_SIZE);
		GLES_ASSERT(x + width <= (IMG_INT32)(2*GLES2_MAX_TEXTURE_SIZE));
		GLES_ASSERT(x + width >= 0); /* <-- No wrapping can take place */
		GLES_ASSERT(y <= (IMG_INT32)GLES2_MAX_TEXTURE_SIZE);
		GLES_ASSERT(y + height <= (IMG_INT32)(2*GLES2_MAX_TEXTURE_SIZE));
		GLES_ASSERT(y + height >= 0); /* <-- No wrapping can take place */

		/* Now we can safely test whether the end coordinates lie within the mipmap */
		if(x + width > i32MipWidth || y + height > i32MipHeight)
		{
			goto bad_value;
		}
	}

	*pui32Level = ui32Level;
	*pui32Face = ui32Face;

	return psTex;
}


#if defined(GLES2_EXTENSION_EGL_IMAGE)
/***********************************************************************************
 Function Name      : UpdateEGLImage
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID UpdateEGLImage(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32Level)
{
	PVR_UNREFERENCED_PARAMETER(ui32Level);

	if(!TextureMakeResident(gc, psTex))
	{
		PVR_DPF((PVR_DBG_ERROR,"UpdateEGLImage: Can't make texture resident"));

		SetError(gc, GL_OUT_OF_MEMORY);
	}
	else
	{
		IMG_UINT32 ui32OffsetInBytes, ui32BytesPerTexel, ui32TopUsize, ui32TopVsize;
		GLES2TextureParamState *psParams;
		EGLImage *psEGLImage;

		psEGLImage = psTex->psEGLImageSource;

		GLES_ASSERT(psEGLImage->ui32Level == ui32Level);

		psParams = &psTex->sState;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
		ui32TopUsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
		ui32TopVsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
		ui32TopUsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
		ui32TopVsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

		ui32BytesPerTexel = psTex->psFormat->ui32TotalBytesPerTexel;

		ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(psEGLImage->ui32Level, ui32TopUsize, ui32TopVsize);

		if((psEGLImage->ui32Target>=EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR) && (psEGLImage->ui32Target<=EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR))
		{
			IMG_UINT32 ui32FaceOffset, ui32Face;
			
			if(psTex->ui32TextureTarget!=GLES2_TEXTURE_TARGET_CEM)
			{
				PVR_DPF((PVR_DBG_ERROR,"GLESGetImageSource: CEM source requested from non-CEM texture"));

				return;
			}

			ui32Face = psEGLImage->ui32Target - EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR;

			ui32FaceOffset = ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);

			if(psTex->ui32HWFlags & GLES2_MIPMAP)
			{
				if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
					(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}

			ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
		}

		psEGLImage->pvLinSurfaceAddress	 = (IMG_VOID*) (((IMG_UINTPTR_T)psTex->psMemInfo->pvLinAddr) + ui32OffsetInBytes);
		psEGLImage->ui32HWSurfaceAddress = psTex->psMemInfo->sDevVAddr.uiAddr + ui32OffsetInBytes;
		psEGLImage->psMemInfo			 = psTex->psMemInfo;
	}
}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */


/***********************************************************************************
 Function Name      : glTexImage2D
 Inputs             : target, level, internalformat, width, height, border, format,
					  type, pixels
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Provides texture data in a specified format &
					  suggests an internal storage format. Data is converted
					  from specified format to internal HW format. Host texture data
					  memory is allocated but data is not twiddled or loaded to HW
					  until use.
************************************************************************************/

GL_APICALL void GL_APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat,
									 GLsizei width, GLsizei height, GLint border, GLenum format,
									 GLenum type, const void *pixels)
{
	GLES2Texture             *psTex;
	IMG_UINT8                *pui8Dest;
	IMG_UINT32               ui32Level, ui32Face = 0;
	PFNCopyTextureData		 pfnCopyTextureData;
	IMG_UINT32               ui32SrcBytesPerPixel;
	const GLES2TextureFormat *psTexFormat;
	GLES2MipMapLevel         *psMipLevel;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexImage2D"));

	GLES2_TIME_START(GLES2_TIMES_glTexImage2D);

	psTex = CheckTexImageArgs(gc, target, level, width, height, border, &ui32Face, &ui32Level);

	if(!psTex)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glTexImage2D);
		return;
	}

#if !defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
	if(internalformat != format)
	{
bad_op:
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glTexImage2D);

		return;
	}
#endif /* !defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

	/* Check the type value for valid enums  */
	switch(type)
	{
		case GL_UNSIGNED_BYTE:
		case GL_UNSIGNED_SHORT_5_6_5:
		case GL_UNSIGNED_SHORT_4_4_4_4:
		case GL_UNSIGNED_SHORT_5_5_5_1:
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
		case GL_FLOAT:
#endif /* defined(GLES2_EXTENSION_FLOAT_TEXTURE) */
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
		case GL_HALF_FLOAT_OES:
#endif /* defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE) */
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
		case GL_UNSIGNED_SHORT:
		case GL_UNSIGNED_INT:
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
		case GL_UNSIGNED_INT_24_8_OES:
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */
		{
			break;
		}
		default:
		{
bad_enum:
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glTexImage2D);

			return;
		}
	}

	switch(format)
	{
		case GL_RGBA:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGBA:
						case GL_RGBA8_OES:
						case GL_RGB5_A1:
						case GL_RGBA4:
						{
							break;
						}
						default:
						{
bad_op:
							SetError(gc, GL_INVALID_OPERATION);

							GLES2_TIME_STOP(GLES2_TIMES_glTexImage2D);

							return;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;

					ui32SrcBytesPerPixel = 4;

					psTexFormat = &TexFormatABGR8888;

					break;
				}
				case GL_UNSIGNED_SHORT_5_5_5_1:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGBA:
						case GL_RGB5_A1:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture5551;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatARGB1555;

					break;
				}
				case GL_UNSIGNED_SHORT_4_4_4_4:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGBA:
						case GL_RGBA4:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture4444;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatARGB4444;

					break;
				}
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
				case GL_FLOAT:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGBA:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTextureFloatRGBA;

					ui32SrcBytesPerPixel = 16;

					psTexFormat = &TexFormatRGBAFloat;

					break;
				}
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
				case GL_HALF_FLOAT_OES:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGBA:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTextureHalfFloatRGBA;

					ui32SrcBytesPerPixel = 8;

					psTexFormat = &TexFormatRGBAHalfFloat;

					break;
				}
#endif
				default:
				{
					goto bad_op;
				}
			}
			break;
		}
#if defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888)
		case GL_BGRA_EXT:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_BGRA_EXT:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;

					ui32SrcBytesPerPixel = 4;

					psTexFormat = &TexFormatARGB8888;

					break;
				}

				default:
				{
					goto bad_op;
				}
			}
			break;
		}
#endif /* defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
		case GL_RGB:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGB:
						case GL_RGB8_OES:
						case GL_RGB565:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture888X;

					ui32SrcBytesPerPixel = 3;

					psTexFormat = &TexFormatXBGR8888;

					break;
				}
				case GL_UNSIGNED_SHORT_5_6_5:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGB:
						case GL_RGB565:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatRGB565;

					break;
				}
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
				case GL_FLOAT:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGB:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTextureFloatRGB;

					ui32SrcBytesPerPixel = 12;

					psTexFormat = &TexFormatRGBFloat;

					break;
				}
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
				case GL_HALF_FLOAT_OES:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGB:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTextureHalfFloatRGB;

					ui32SrcBytesPerPixel = 6;

					psTexFormat = &TexFormatRGBHalfFloat;

					break;
				}
#endif
				default:
				{
					goto bad_op;
				}
			}
			break;
		}
		case GL_ALPHA:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_ALPHA:
						case GL_ALPHA8_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture8Bits;

					ui32SrcBytesPerPixel = 1;

					psTexFormat = &TexFormatAlpha;

					break;
				}
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
				case GL_FLOAT:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_ALPHA:
						case GL_ALPHA8_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;

					ui32SrcBytesPerPixel = 4;

					psTexFormat = &TexFormatFloatAlpha;

					break;
				}
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
				case GL_HALF_FLOAT_OES:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_ALPHA:
						case GL_ALPHA8_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatHalfFloatAlpha;

					break;
				}
#endif
				default:
				{
					goto bad_op;
				}
			}
			break;
		}
		case GL_LUMINANCE:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_LUMINANCE:
						case GL_LUMINANCE8_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture8Bits;

					ui32SrcBytesPerPixel = 1;

					psTexFormat = &TexFormatLuminance;

					break;
				}
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
				case GL_FLOAT:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_LUMINANCE:
						case GL_LUMINANCE8_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;

					ui32SrcBytesPerPixel = 4;

					psTexFormat = &TexFormatFloatLuminance;

					break;
				}
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
				case GL_HALF_FLOAT_OES:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_LUMINANCE:
						case GL_LUMINANCE8_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatHalfFloatLuminance;

					break;
				}
#endif
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
		case GL_LUMINANCE_ALPHA:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_LUMINANCE_ALPHA:
						case GL_LUMINANCE8_ALPHA8_OES:
						case GL_LUMINANCE4_ALPHA4_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatLuminanceAlpha;

					break;
				}
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
				case GL_FLOAT:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_LUMINANCE_ALPHA:
						case GL_LUMINANCE8_ALPHA8_OES:
						case GL_LUMINANCE4_ALPHA4_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTextureFloatLA;

					ui32SrcBytesPerPixel = 8;

					psTexFormat = &TexFormatFloatLuminanceAlpha;

					break;
				}
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
				case GL_HALF_FLOAT_OES:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_LUMINANCE_ALPHA:
						case GL_LUMINANCE8_ALPHA8_OES:
						case GL_LUMINANCE4_ALPHA4_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;

					ui32SrcBytesPerPixel = 4;

					psTexFormat = &TexFormatHalfFloatLuminanceAlpha;

					break;
				}
#endif
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
		case GL_DEPTH_COMPONENT:
		{
			switch(type)
			{
				case GL_UNSIGNED_SHORT:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_DEPTH_COMPONENT:
						case GL_DEPTH_COMPONENT16:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16BitIntTo32BitFloat;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatFloatDepth;

					break;
				}
				case GL_UNSIGNED_INT:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_DEPTH_COMPONENT:
						case GL_DEPTH_COMPONENT16:
						case GL_DEPTH_COMPONENT24_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32BitIntTo32BitFloat;

					ui32SrcBytesPerPixel = 4;

					psTexFormat = &TexFormatFloatDepth;

					break;
				}
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */

#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
		case GL_DEPTH_STENCIL_OES:
		{
			switch(type)
			{
				case GL_UNSIGNED_INT_24_8_OES:
				{
#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_DEPTH_STENCIL_OES:
						case GL_DEPTH24_STENCIL8_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture24_8DepthStencilTo32BitFloat8BitInt;

					ui32SrcBytesPerPixel = 4;

					psTexFormat = &TexFormatFloatDepthU8Stencil;

					break;
				}
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) */

		default:
		{
			goto bad_enum;
		}
	}

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageSource)
	{
		if (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
		{
			TexMgrGhostTexture(gc, psTex);
		}
		else
		{
			/* Inform EGL that the source is being orphaned */
			KEGLUnbindImage(psTex->psEGLImageSource->hImage);

			psTex->psMemInfo = IMG_NULL;
			psTex->psEGLImageSource = IMG_NULL;
		}
	}
	else if(psTex->psEGLImageTarget)
	{
		ReleaseImageFromTexture(gc, psTex);
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */


	psMipLevel = &psTex->psMipLevel[ui32Level];

	/* If the mipmap is attached to any framebuffer, notify it of the change */
	FBOAttachableHasBeenModified(gc, (GLES2FrameBufferAttachable*)psMipLevel);

	/* Allocate memory for the level data */
	pui8Dest = TextureCreateLevel(gc, psTex, ui32Level, (IMG_UINT32)format, psTexFormat, (IMG_UINT32)width, (IMG_UINT32)height);


	if(pixels && pui8Dest)
	{
		IMG_VOID *pvDest = psMipLevel->pui8Buffer;
		const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pixels;
		IMG_UINT32 ui32Align = gc->sState.sClientPixel.ui32UnpackAlignment;
		IMG_UINT32 ui32SrcRowSize = (IMG_UINT32)width * ui32SrcBytesPerPixel;
		IMG_UINT32 ui32Padding = ui32SrcRowSize % ui32Align;

		if(ui32Padding)
		{
			ui32SrcRowSize += (ui32Align - ui32Padding);
		}

		if ((height) && (width))
		{
			(*pfnCopyTextureData)(pvDest, pui8Src, (IMG_UINT32)width, (IMG_UINT32)height,
					      ui32SrcRowSize, psMipLevel, IMG_FALSE);
		}
	}

	/*
	 * remove texture from active list.  If needed, validation will reload
	 */
	TextureRemoveResident(gc, psTex);

	gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;


	GLES2_INC_PIXEL_COUNT(GLES2_TIMES_glTexImage2D, width*height);
	GLES2_TIME_STOP(GLES2_TIMES_glTexImage2D);
}


/***********************************************************************************
 Function Name      : glTexSubImage2D
 Inputs             : target, level, xoffset, yoffset, width, height, format, type,
					  pixels
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Provides texture subdata in a specified format.
					  Host texture data memory is already allocated/may have already
					  been uploaded to HW. Subdata will need to be integrated/uploaded.
************************************************************************************/
GL_APICALL void GL_APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset,
										GLint yoffset, GLsizei width, GLsizei height,
										GLenum format, GLenum type, const void *pixels)
{
	GLES2Texture *psTex;
	IMG_UINT8 *pui8Dest;
	IMG_UINT32 ui32Lod=(IMG_UINT32)level, ui32Level, ui32Face;
	PFNCopyTextureData pfnCopyTextureData;
	IMG_UINT32 ui32SrcBytesPerPixel, ui32DstStride, ui32DstBytesPerPixel;
	const GLES2TextureFormat *psSubTexFormat, *psTargetTexFormat;
	GLES2MipMapLevel *psMipLevel;

	IMG_BOOL bHWSubTextureUploaded = IMG_FALSE;


	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexSubImage2D"));

	GLES2_TIME_START(GLES2_TIMES_glTexSubImage2D);


	switch(format)
	{
		case GL_RGBA:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;
					ui32SrcBytesPerPixel = 4;
					psSubTexFormat = &TexFormatABGR8888;
					break;
				}
				case GL_UNSIGNED_SHORT_5_5_5_1:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture5551;
					ui32SrcBytesPerPixel = 2;
					psSubTexFormat = &TexFormatARGB1555;
					break;
				}
				case GL_UNSIGNED_SHORT_4_4_4_4:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture4444;
					ui32SrcBytesPerPixel = 2;
					psSubTexFormat = &TexFormatARGB4444;
					break;
				}
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
				case GL_FLOAT:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTextureFloatRGBA;
					ui32SrcBytesPerPixel = 16;
					psSubTexFormat = &TexFormatRGBAFloat;
					break;
				}
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
				case GL_HALF_FLOAT_OES:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTextureHalfFloatRGBA;
					ui32SrcBytesPerPixel = 8;
					psSubTexFormat = &TexFormatRGBAHalfFloat;
					break;
				}
#endif
				default:
				{
bad_op:
					SetError(gc, GL_INVALID_OPERATION);
					GLES2_TIME_STOP(GLES2_TIMES_glTexSubImage2D);
					return;
				}
			}
			break;
		}
#if defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888)
		case GL_BGRA_EXT:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;
					ui32SrcBytesPerPixel = 4;
					psSubTexFormat = &TexFormatARGB8888;
					break;
				}

				default:
				{
					goto bad_op;
				}
			}

			break;
		}
#endif /* defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
		case GL_RGB:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture888X;
					ui32SrcBytesPerPixel = 3;
					psSubTexFormat = &TexFormatXBGR8888;
					break;
				}
				case GL_UNSIGNED_SHORT_5_6_5:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;
					ui32SrcBytesPerPixel = 2;
					psSubTexFormat = &TexFormatRGB565;
					break;
				}
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
				case GL_FLOAT:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTextureFloatRGB;
					ui32SrcBytesPerPixel = 12;
					psSubTexFormat = &TexFormatRGBFloat;
					break;
				}
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
				case GL_HALF_FLOAT_OES:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTextureHalfFloatRGB;
					ui32SrcBytesPerPixel = 6;
					psSubTexFormat = &TexFormatRGBHalfFloat;
					break;
				}
#endif
				default:
				{
					goto bad_op;
				}
			}
			break;
		}
		case GL_ALPHA:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture8Bits;
					ui32SrcBytesPerPixel = 1;
					psSubTexFormat = &TexFormatAlpha;
					break;
				}
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
				case GL_FLOAT:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;
					ui32SrcBytesPerPixel = 4;
					psSubTexFormat = &TexFormatFloatAlpha;
					break;
				}
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
				case GL_HALF_FLOAT_OES:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;
					ui32SrcBytesPerPixel = 2;
					psSubTexFormat = &TexFormatHalfFloatAlpha;
					break;
				}
#endif
				default:
				{
					goto bad_op;
				}
			}
			break;
		}
		case GL_LUMINANCE:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture8Bits;
					ui32SrcBytesPerPixel = 1;
					psSubTexFormat = &TexFormatLuminance;
					break;
				}
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
				case GL_FLOAT:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;
					ui32SrcBytesPerPixel = 4;
					psSubTexFormat = &TexFormatFloatLuminance;
					break;
				}
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
				case GL_HALF_FLOAT_OES:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;
					ui32SrcBytesPerPixel = 2;
					psSubTexFormat = &TexFormatHalfFloatLuminance;
					break;
				}
#endif
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
		case GL_LUMINANCE_ALPHA:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;
					ui32SrcBytesPerPixel = 2;
					psSubTexFormat = &TexFormatLuminanceAlpha;
					break;
				}
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
				case GL_FLOAT:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTextureFloatLA;
					ui32SrcBytesPerPixel = 8;
					psSubTexFormat = &TexFormatFloatLuminanceAlpha;
					break;
				}
#endif
#if defined(GLES2_EXTENSION_HALF_FLOAT_TEXTURE)
				case GL_HALF_FLOAT_OES:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;
					ui32SrcBytesPerPixel = 4;
					psSubTexFormat = &TexFormatHalfFloatLuminanceAlpha;
					break;
				}
#endif
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
#if defined(GLES2_EXTENSION_DEPTH_TEXTURE)
		case GL_DEPTH_COMPONENT:
		{
			switch(type)
			{
				case GL_UNSIGNED_SHORT:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16BitIntTo32BitFloat;

					ui32SrcBytesPerPixel = 2;

					psSubTexFormat = &TexFormatFloatDepth;

					break;
				}
				case GL_UNSIGNED_INT:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32BitIntTo32BitFloat;

					ui32SrcBytesPerPixel = 4;

					psSubTexFormat = &TexFormatFloatDepth;

					break;
				}
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
#endif /* defined(GLES2_EXTENSION_DEPTH_TEXTURE) */
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL) 
		case GL_DEPTH_STENCIL_OES:
		{
			switch(type)
			{
				case GL_UNSIGNED_INT_24_8_OES:
				{
					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture24_8DepthStencilTo32BitFloat8BitInt;

					ui32SrcBytesPerPixel = 4;

					psSubTexFormat = &TexFormatFloatDepthU8Stencil;

					break;
				}
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
#endif /* defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)  */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			GLES2_TIME_STOP(GLES2_TIMES_glTexSubImage2D);
			return;
		}
	}

	psTex = CheckTexSubImageArgs(gc, target, level, xoffset, yoffset, width, height, 
								 psSubTexFormat, &ui32Face, &ui32Level);

	if(!psTex)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glTexSubImage2D);
		return;
	}

	psMipLevel = &psTex->psMipLevel[ui32Level];

	/* Get the format of the target texture */
	psTargetTexFormat = psMipLevel->psTexFormat;

	/* Base formats match, now check if type conversion if required */
	if(psSubTexFormat->ePixelFormat != psTargetTexFormat->ePixelFormat)
	{
		switch(psSubTexFormat->ePixelFormat)
		{
			case PVRSRV_PIXEL_FORMAT_ABGR8888:
			{
				switch(psTargetTexFormat->ePixelFormat)
				{
#if defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888)
					case PVRSRV_PIXEL_FORMAT_ARGB8888:
					{
						/* This is actually RGBA to BGRA, but the function is the same */
						pfnCopyTextureData = (PFNCopyTextureData) CopyTextureBGRA8888toRGBA8888;
						break;
					}

#endif /* defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
					case PVRSRV_PIXEL_FORMAT_ARGB4444:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTextureRGBA8888to4444;
						break;
					}
					case PVRSRV_PIXEL_FORMAT_ARGB1555:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTextureRGBA8888to5551;
						break;
					}
					case PVRSRV_PIXEL_FORMAT_RGB565:
					{
						/* HW only supports 8888 so RGB 888 case goes here */	
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture888to565;
						break;
					}
					default:
					{
						/* Should never get here...*/ 
						PVR_DPF((PVR_DBG_ERROR,"glTexSubImage2D: Missing/Unsupported format conversion"));
						goto bad_op;
					}
				}

				break;
			}
#if defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888)
			case PVRSRV_PIXEL_FORMAT_ARGB8888:
			{
				switch(psTargetTexFormat->ePixelFormat)
				{
					case PVRSRV_PIXEL_FORMAT_ABGR8888:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTextureBGRA8888toRGBA8888;
						break;
					}

					case PVRSRV_PIXEL_FORMAT_ARGB4444:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTextureBGRA8888to4444;
						break;
					}

					case PVRSRV_PIXEL_FORMAT_ARGB1555:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTextureBGRA8888to5551;
						break;
					}

					default:
					{
						/* Should never get here...*/ 
						PVR_DPF((PVR_DBG_ERROR,"glTexSubImage2D: Missing/Unsupported format conversion"));
						goto bad_op;
					}
				}

				break;
			}

#endif /* defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
			case PVRSRV_PIXEL_FORMAT_ARGB4444:
			{
				switch(psTargetTexFormat->ePixelFormat)
				{
					case PVRSRV_PIXEL_FORMAT_ABGR8888:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture4444toRGBA8888;
						break;
					}
					case PVRSRV_PIXEL_FORMAT_ARGB1555:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture4444to5551;
						break;
					}
					default:
					{
						/* Should never get here...*/ 
						PVR_DPF((PVR_DBG_ERROR,"glTexSubImage2D: Missing/Unsupported format conversion"));
						goto bad_op;
					}
				}

				break;
			}
			case PVRSRV_PIXEL_FORMAT_ARGB1555:
			{
				switch(psTargetTexFormat->ePixelFormat)
				{
					case PVRSRV_PIXEL_FORMAT_ARGB8888:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture5551toBGRA8888;
						break;
					}
					case PVRSRV_PIXEL_FORMAT_ABGR8888:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture5551toRGBA8888;
						break;
					}
					case PVRSRV_PIXEL_FORMAT_ARGB4444:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture5551to4444;
						break;
					}
					default:
					{
						/* Should never get here...*/ 
						PVR_DPF((PVR_DBG_ERROR,"glTexSubImage2D: Missing/Unsupported format conversion"));
						goto bad_op;
					}
				}

				break;
			}
			case PVRSRV_PIXEL_FORMAT_RGB565:
			{
				switch(psTargetTexFormat->ePixelFormat)
				{
					case PVRSRV_PIXEL_FORMAT_ABGR8888:
					{
						/* HW only supports 8888 so RGB 888 case goes here */	
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture565toRGBX8888;
						break;
					}
					default:
					{
						/* Should never get here...*/ 
						PVR_DPF((PVR_DBG_ERROR,"glTexSubImage2D: Missing/Unsupported format conversion"));
						goto bad_op;
					}
				}

				break;
			}
			default:
			{
				/* Should never get here...*/ 
				PVR_DPF((PVR_DBG_ERROR,"glTexSubImage2D: Missing/Unsupported format conversion"));
				goto bad_op;
			}
		}
	}

	/* Don't need to do anything as there is no data */
	if (!pixels || !height || !width)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glTexSubImage2D);
		return;
	}

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageTarget)
	{
		const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pixels;
		IMG_UINT32 ui32Align = gc->sState.sClientPixel.ui32UnpackAlignment;
		IMG_UINT32 ui32SrcRowSize = (IMG_UINT32)width * ui32SrcBytesPerPixel;
		IMG_VOID *pvDest;
		IMG_UINT32 ui32Padding = ui32SrcRowSize % ui32Align;

		if(ui32Padding)
		{
			ui32SrcRowSize += (ui32Align - ui32Padding);
		}

	    GLES_ASSERT(psTex->psEGLImageTarget->ui32Width==psMipLevel->ui32Width && psTex->psEGLImageTarget->ui32Height==psMipLevel->ui32Height);
		GLES_ASSERT(psMipLevel->pui8Buffer==GLES2_LOADED_LEVEL)

		if(psTex->psEGLImageTarget->bTwiddled)
		{
		    IMG_UINT32 ui32BufferSize;

			ui32DstStride = psMipLevel->ui32Width * psTargetTexFormat->ui32TotalBytesPerTexel;

		    ui32BufferSize = ui32DstStride * psMipLevel->ui32Height;
			
			PVRSRVLockMutex(gc->psSharedState->hTertiaryLock);

			psMipLevel->pui8Buffer = GLES2MallocHeapUNC(gc, ui32BufferSize);

			if(!psMipLevel->pui8Buffer)
			{
			    SetError(gc, GL_OUT_OF_MEMORY);

				PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

				GLES2_TIME_STOP(GLES2_TIMES_glTexSubImage2D);

				return;
			}

			ReadBackTextureData(gc, psTex, 0, 0, psMipLevel->pui8Buffer);

			pvDest = psMipLevel->pui8Buffer + ((IMG_UINT32)yoffset * ui32DstStride) + ((IMG_UINT32)xoffset * psTargetTexFormat->ui32TotalBytesPerTexel);

			(*pfnCopyTextureData)(pvDest, pui8Src, (IMG_UINT32)width, (IMG_UINT32)height, 
								ui32SrcRowSize, psMipLevel, IMG_TRUE);

			TranslateLevel(gc, psTex, 0, 0);

			GLES2FreeAsync(gc, psMipLevel->pui8Buffer);

			psMipLevel->pui8Buffer = GLES2_LOADED_LEVEL;

			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);
		}
		else
		{
			ui32DstStride = psTex->psEGLImageTarget->ui32Stride;

			pvDest = (IMG_UINT8 *)psTex->psEGLImageTarget->psMemInfo->pvLinAddr + ((IMG_UINT32)yoffset * ui32DstStride) + ((IMG_UINT32)xoffset * psTargetTexFormat->ui32TotalBytesPerTexel);

			(*pfnCopyTextureData)(pvDest, pui8Src, (IMG_UINT32)width, (IMG_UINT32)height, 
						      ui32SrcRowSize, psMipLevel, IMG_TRUE);
		}

		GLES2_TIME_STOP(GLES2_TIMES_glTexSubImage2D);

		return;
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

	ui32DstBytesPerPixel = psTargetTexFormat->ui32TotalBytesPerTexel / psTargetTexFormat->ui32NumChunks;

	/* Special case of chunks with different sizes */
#if defined(GLES2_EXTENSION_PACKED_DEPTH_STENCIL)
	if(psTargetTexFormat->ui32BaseFormatIndex == GLES2_DEPTH_STENCIL_TEX_INDEX) 
	{
		ui32DstBytesPerPixel = 4;
	}
#endif
#if defined(GLES2_EXTENSION_FLOAT_TEXTURE)
	if(psTargetTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_B16G16R16F)
	{
		ui32DstBytesPerPixel = 4;
	}
#endif

	ui32DstStride = psMipLevel->ui32Width * ui32DstBytesPerPixel;
	pui8Dest = psMipLevel->pui8Buffer;


	/* copy subtexture data into the host memory 
	   if the host copy of the whole level texture has not been uploaded */

	if(pui8Dest != GLES2_LOADED_LEVEL)
	{
		const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pixels;
		IMG_UINT32 ui32Align = gc->sState.sClientPixel.ui32UnpackAlignment;
		IMG_UINT32 ui32SrcRowSize = (IMG_UINT32)width * ui32SrcBytesPerPixel;
		IMG_VOID *pvDest;
		IMG_UINT32 ui32Padding = ui32SrcRowSize % ui32Align;
		
		if(!pui8Dest)
		{
			SetError(gc, GL_OUT_OF_MEMORY);
			GLES2_TIME_STOP(GLES2_TIMES_glTexSubImage2D);
			return;
		}
		
		pvDest = pui8Dest + ((IMG_UINT32)yoffset * ui32DstStride) + ((IMG_UINT32)xoffset * ui32DstBytesPerPixel);

		if(ui32Padding)
		{
			ui32SrcRowSize += (ui32Align - ui32Padding);
		}

		(*pfnCopyTextureData)(pvDest, pui8Src, (IMG_UINT32)width, (IMG_UINT32)height,
								ui32SrcRowSize, psMipLevel, IMG_TRUE);
	}
	else
	{
	    /* directly upload subtexture data into the device memory
		   if the host copy of the whole level texture has already been uploaded to the device memory */

	    if(!gc->sAppHints.bDisableHWTQTextureUpload)
		{
			PVRSRV_CLIENT_MEM_INFO sMemInfo  = {0};
			GLES2TextureParamState  *psParams = &psTex->sState;
			IMG_UINT32 ui32TexAddr,i;

			const GLES2TextureFormat *psTexFmt = psTex->psFormat;
			IMG_UINT32 ui32BytesPerTexel = psTexFmt->ui32TotalBytesPerTexel;

			/* 
			   When HWTQ is used to upload this texture level,
			   only if the texture resource is being used in the current frame,
			   then NEED to ghost it and create new texture device memory,
			   otherwise NO NEED to ghost at all
			   This optimisation can only be used if we have a valid gc->psRenderSurface
			*/
			if(psTex->psMemInfo)
			{
				if (gc->psRenderSurface)
				{
					if (KRM_IsResourceInUse(&gc->psSharedState->psTextureManager->sKRM,
					                        gc->psRenderSurface,
					                        &gc->psRenderSurface->sRenderStatusUpdate,
					                        &psTex->sResource))
					{
						sMemInfo = *psTex->psMemInfo;
						TexMgrGhostTexture(gc, psTex);
					}
				}
				else  if (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM,
				                               &psTex->sResource))
				{
					sMemInfo = *psTex->psMemInfo;
					TexMgrGhostTexture(gc, psTex);
				}
			}

			if (!psTex->psMemInfo)
			{
			    if(!CreateTextureMemory(gc, psTex))
				{
				    return;
				}

				ui32TexAddr = psTex->psMemInfo->sDevVAddr.uiAddr;

				psParams->aui32StateWord2[0] = 
				  (ui32TexAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
				  EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

				for(i=1; i < psTex->psFormat->ui32NumChunks; i++)
				{
				    psParams->aui32StateWord2[i] = 
					  ((ui32TexAddr + (psTex->ui32ChunkSize * i)) >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) <<
					  EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;
				}

				gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;
			}


			/* and copy the old device memory to the new memory */

			if(((IMG_UINT32)width  == psMipLevel->ui32Width) && 
			   ((IMG_UINT32)height == psMipLevel->ui32Height) &&
			   ((psTex->ui32HWFlags & GLES2_MIPMAP) == 0) &&
			   (psTex->ui32TextureTarget != GLES2_TEXTURE_TARGET_CEM))
			{
				FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psMipLevel, GLES2_SCHEDULE_HW_DISCARD_SCENE);
			}
			else
			{
			    /* Render to mipmap before reading it back */
			    FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psMipLevel, 
										GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_WAIT_FOR_3D);

				if ((sMemInfo.uAllocSize)) /* transfer the whole texture data of all levels from the old device memory to the new memory */
				{
				    CopyTextureData(gc, psTex, 0, &sMemInfo, 0, sMemInfo.uAllocSize);
				}
				/* else: MemInfo hasn't been ghosted, the original device memory is being used. */
			}


			/* directly upload subtexture data to the new device memory, using hardware */
			{
				IMG_UINT32 ui32SubTexBufferSize = (IMG_UINT32)width * (IMG_UINT32)height * psTargetTexFormat->ui32TotalBytesPerTexel;
				IMG_UINT8 *pui8SubTexBuffer = (IMG_UINT8 *) GLES2MallocHeapUNC(gc, ui32SubTexBufferSize);
				const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pixels;
				IMG_UINT32 ui32SrcRowSize = (IMG_UINT32)width * ui32SrcBytesPerPixel;
				IMG_UINT32 ui32OffsetInBytes = 0;

				SGX_QUEUETRANSFER sQueueTransfer;

				GLES2SubTextureInfo sSubTexInfo;
				
				sSubTexInfo.ui32SubTexXoffset = (IMG_UINT32)xoffset;
				sSubTexInfo.ui32SubTexYoffset = (IMG_UINT32)yoffset;
				sSubTexInfo.ui32SubTexWidth   = (IMG_UINT32)width;
				sSubTexInfo.ui32SubTexHeight  = (IMG_UINT32)height;
				sSubTexInfo.pui8SubTexBuffer  = pui8SubTexBuffer;

				if(!pui8SubTexBuffer)
				{
				    SetError(gc, GL_OUT_OF_MEMORY);
					GLES2_TIME_STOP(GLES2_TIMES_glTexSubImage2D);
					return;
				}

#if (defined(DEBUG) || defined(TIMING))
				ui32TextureMemCurrent += ui32SubTexBufferSize;
			
				if (ui32TextureMemCurrent>ui32TextureMemHWM)
				{
				    ui32TextureMemHWM = ui32TextureMemCurrent;
				}
#endif

				if(psTex->ui32HWFlags & GLES2_NONPOW2)
				{
				    /* Non-power-of-two texture: cannot be mipmapped nor a CEM */
				    GLES_ASSERT(ui32Face == 0 && ui32Lod == 0);
					
					ui32OffsetInBytes = 0;

					if (PrepareHWTQTextureUpload(gc, psTex, 
												 ui32OffsetInBytes, psMipLevel,
												 &sSubTexInfo, pfnCopyTextureData,
												 ui32SrcRowSize, pui8Src, &sQueueTransfer))
					{
						if (HWTQTextureUpload(gc, psTex, &sQueueTransfer))
						{
							bHWSubTextureUploaded = IMG_TRUE;
						
							/* Reset bResidence, to prevent going into TextureMakeResident() */
							psTex->bResidence = IMG_TRUE;
						}
					}
				}
				else
				{
					IMG_UINT32 ui32TopUsize, ui32TopVsize;
					
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
					ui32TopUsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
					ui32TopVsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
					ui32TopUsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
					ui32TopVsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

					/* setup ui32OffsetInBytes */
					if (psTex->ui32HWFlags & GLES2_MULTICHUNK)
					{
						IMG_UINT32 ui32BytesPerChunk = psTexFmt->ui32TotalBytesPerTexel / psTexFmt->ui32NumChunks;
						IMG_UINT32 ui32OffsetInChunkInBytes = ui32BytesPerChunk * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
			  
						for (i=0; i < psTexFmt->ui32NumChunks; i++)
						{
							/* Special case of chunks with different sizes */
							if(psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_B16G16R16F)
							{
								if(i==0)
									ui32BytesPerChunk = 4;
								else
									ui32BytesPerChunk = 2;

								ui32OffsetInChunkInBytes = ui32BytesPerChunk * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
							}

							if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
							{
							    IMG_UINT32 ui32FaceOffset = 
								  ui32BytesPerChunk * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);
					
								if(psTex->ui32HWFlags & GLES2_MIPMAP)
								{
									if(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP)
									{
										ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
									}
								}
								ui32OffsetInChunkInBytes += (ui32FaceOffset * ui32Face);
							}
							ui32OffsetInBytes = (i * psTex->ui32ChunkSize) + ui32OffsetInChunkInBytes;
						}
					}
					else
					{
					    ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
								  
						if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
						{
						    IMG_UINT32 ui32FaceOffset = 
							  ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);

							if(psTex->ui32HWFlags & GLES2_MIPMAP)
							{
								if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
									(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
								{
									ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
								}
							}
							ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
						}
					}

				  
					if (PrepareHWTQTextureUpload(gc, psTex, 
												 ui32OffsetInBytes, psMipLevel,
												 &sSubTexInfo, pfnCopyTextureData,
												 ui32SrcRowSize, pui8Src, &sQueueTransfer))
					{
						if (HWTQTextureUpload(gc, psTex, &sQueueTransfer))
						{
							bHWSubTextureUploaded = IMG_TRUE;
						
							/* Reset bResidence, to prevent going into TextureMakeResident() */
							psTex->bResidence = IMG_TRUE;
						}
					}

				}
				
				GLES2FreeAsync(gc, pui8SubTexBuffer);
			}
		}


		/* otherwise use the software subtexture uploading approach
		   readback the whole level texture data and copy subtexture data to the host memory. */

		if(!bHWSubTextureUploaded)
		{
			const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pixels;
			IMG_UINT32 ui32Align = gc->sState.sClientPixel.ui32UnpackAlignment;
			IMG_UINT32 ui32SrcRowSize = (IMG_UINT32)width * ui32SrcBytesPerPixel;
			IMG_UINT32 ui32Padding = ui32SrcRowSize % ui32Align;
			IMG_UINT32 ui32BufferSize = ui32DstStride * psMipLevel->ui32Height;
			IMG_VOID *pvDest;
		
			pui8Dest = GLES2MallocHeapUNC(gc, ui32BufferSize);

			if(!pui8Dest)
			{
				SetError(gc, GL_OUT_OF_MEMORY);
				GLES2_TIME_STOP(GLES2_TIMES_glTexSubImage2D);
				return;
			}

#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent += ui32BufferSize;
		
			if (ui32TextureMemCurrent>ui32TextureMemHWM)
			{
				ui32TextureMemHWM = ui32TextureMemCurrent;
			}
#endif
			/* Don't need to read back for full size subtexture */
			if(((IMG_UINT32)width  == psMipLevel->ui32Width) && ((IMG_UINT32)height == psMipLevel->ui32Height))
			{
				FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psMipLevel, GLES2_SCHEDULE_HW_DISCARD_SCENE);
			}
			else
			{
				/* Render to mipmap before reading it back */
				FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psMipLevel, 
											GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_WAIT_FOR_3D);

				/* otherwise already flushed */
				ReadBackTextureData(gc, psTex, ui32Face, (IMG_UINT32)level, pui8Dest);
			}

			psMipLevel->pui8Buffer = pui8Dest;
			pvDest = pui8Dest + ((IMG_UINT32)yoffset * ui32DstStride) + ((IMG_UINT32)xoffset * ui32DstBytesPerPixel);

			if(ui32Padding)
			{
				ui32SrcRowSize += (ui32Align - ui32Padding);
			}

			(*pfnCopyTextureData)(pvDest, pui8Src, (IMG_UINT32)width, (IMG_UINT32)height,
									ui32SrcRowSize, psMipLevel, IMG_TRUE);

			TextureRemoveResident(gc, psTex);

			gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;
		}
	}


#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageSource || psTex->psEGLImageTarget)
	{
		UpdateEGLImage(gc, psTex, ui32Level);
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */


	GLES2_INC_PIXEL_COUNT(GLES2_TIMES_glTexSubImage2D, width*height);
	GLES2_TIME_STOP(GLES2_TIMES_glTexSubImage2D);
}


/***********************************************************************************
 Function Name      : glCompressedTexImage2D
 Inputs             : target, level, internalformat, width, height, border, format,
					  imageSize, data
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Provides texture data in a pre-compressed format.
					  suggests an internal storage format. Host texture data memory
					  is allocated but not loaded to HW until use. Palette textures are
					  unsupported
************************************************************************************/
GL_APICALL void GL_APIENTRY glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const void *data)
{
	GLES2Texture *psTex;
	IMG_UINT32 ui32Level, ui32Face;	
	IMG_UINT8 *pui8Dest;
	const GLES2TextureFormat *psTexFormat;
	IMG_UINT32 ui32Size;
	IMG_UINT32 ui32Width;
	IMG_UINT32 ui32Height;
	const IMG_VOID *pvPixels = (const IMG_VOID *)data;
	GLES2MipMapLevel *psMipLevel;
	PFNCopyTextureData pfnCopyTextureData;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glCompressedTexImage2D"));

	GLES2_TIME_START(GLES2_TIMES_glCompressedTexImage2D);

	switch(internalformat)
	{
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
		{
			psTexFormat = &TexFormatPVRTC2RGB;
			pfnCopyTextureData = (PFNCopyTextureData)CopyTexturePVRTC;
			break;
		}
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
		{
			psTexFormat = &TexFormatPVRTC2RGBA;
			pfnCopyTextureData = (PFNCopyTextureData)CopyTexturePVRTC;
			break;
		}
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
		{
			psTexFormat = &TexFormatPVRTC4RGB;
			pfnCopyTextureData = (PFNCopyTextureData)CopyTexturePVRTC;
			break;
		}
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
		{
			psTexFormat = &TexFormatPVRTC4RGBA;
			pfnCopyTextureData = (PFNCopyTextureData)CopyTexturePVRTC;
			break;
		}
		case GL_ETC1_RGB8_OES:
		{
			psTexFormat = &TexFormatETC1RGB;
			pfnCopyTextureData = (PFNCopyTextureData)CopyTextureETC1;
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			GLES2_TIME_STOP(GLES2_TIMES_glCompressedTexImage2D);
			return;
		}
	}

	psTex = CheckTexImageArgs(gc, target, level, width, height, border, &ui32Face, &ui32Level);

	if(!psTex)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glCompressedTexImage2D);
		return;
	}

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageSource)
	{
		if (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
		{
			TexMgrGhostTexture(gc, psTex);
		}
		else
		{
			/* Inform EGL that the source is being orphaned */
			KEGLUnbindImage(psTex->psEGLImageSource->hImage);

			psTex->psMemInfo = IMG_NULL;
			psTex->psEGLImageSource = IMG_NULL;
		}
	}
	else if(psTex->psEGLImageTarget)
	{
		ReleaseImageFromTexture(gc, psTex);
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

	if(pfnCopyTextureData==(PFNCopyTextureData)CopyTextureETC1)
	{
		/* Must have at least 1 block */
		ui32Height = ALIGNCOUNTINBLOCKS((IMG_UINT32)height, 2);
		ui32Width  = ALIGNCOUNTINBLOCKS((IMG_UINT32)width, 2);
	}
	else
	{
		/* Must have at least 2x2 blocks */
		ui32Height = MAX((IMG_UINT32)height >> 2, 2);

		/* 2Bpp encodes 8x4 blocks, 4Bpp encodes 4x4 */
		if( (psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTC2) || 
			(psTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCII2))
		{
			ui32Width = MAX((IMG_UINT32)width >> 3, 2);
		}
		else
		{
			ui32Width = MAX((IMG_UINT32)width >> 2, 2);
		}
	}
	
	ui32Size = ui32Width * ui32Height << 3;
	
	if(imageSize != (GLint)ui32Size)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES2_TIME_STOP(GLES2_TIMES_glCompressedTexImage2D);
		return;
	}
	
	psMipLevel = &psTex->psMipLevel[ui32Level];

	pui8Dest = TextureCreateLevel(gc, psTex, ui32Level, internalformat, psTexFormat, (IMG_UINT32)width, (IMG_UINT32)height);
	


	if(pvPixels && pui8Dest)
	{
		IMG_VOID *pvDest = psMipLevel->pui8Buffer;

		if ((ui32Height) && (ui32Width))
		{
			pfnCopyTextureData(pvDest, pvPixels, (IMG_UINT32)width, (IMG_UINT32)height, 0, psMipLevel, IMG_FALSE);
		}
	}
	else
	{
		/*
		* remove texture from active list.  If needed, validation will reload 
		*/
		TextureRemoveResident(gc, psTex);
		GLES2_TIME_STOP(GLES2_TIMES_glCompressedTexImage2D);
		return;
	}
	
	/*
	* remove texture from active list.  If needed, validation will reload 
	*/
	TextureRemoveResident(gc, psTex);

	gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;


	GLES2_INC_PIXEL_COUNT(GLES2_TIMES_glCompressedTexImage2D, width*height);
	GLES2_TIME_STOP(GLES2_TIMES_glCompressedTexImage2D);
}


/***********************************************************************************
 Function Name      : glCompressedTexSubImage2D
 Inputs             : target, level, xoffset, yoffset, width, height, format,
					  imageSize, data
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Provides texture subdata in a pre-compressed format.
					  Host texture data memory is already allocated/may have already
					  been uploaded to HW. Subdata will need to be integrated/uploaded.
************************************************************************************/
GL_APICALL void GL_APIENTRY glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset,
												  GLint yoffset, GLsizei width, GLsizei height,
												  GLenum format, GLsizei imageSize, const void *data)
{
	GLES2Texture *psTex;
	const GLES2TextureFormat *psSubTexFormat;
	const IMG_VOID *pvPixels = (const IMG_VOID *)data;
	IMG_UINT8 *pui8Dest;
	IMG_UINT32 ui32Width;
	IMG_UINT32 ui32Height;
	IMG_UINT32 ui32Size;
	IMG_UINT32 ui32Face, ui32Level;
	GLES2MipMapLevel *psMipLevel;

	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glCompressedTexSubImage2D"));

	GLES2_TIME_START(GLES2_TIMES_glCompressedTexSubImage2D);


	switch(format)
	{
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
		{
			psSubTexFormat = &TexFormatPVRTC2RGB;

			break;
		}
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
		{
			psSubTexFormat = &TexFormatPVRTC2RGBA;

			break;
		}
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
		{
			psSubTexFormat = &TexFormatPVRTC4RGB;

			break;
		}
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
		{
			psSubTexFormat = &TexFormatPVRTC4RGBA;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glCompressedTexSubImage2D);

			return;
		}
	}

	psTex = CheckTexSubImageArgs(gc, target, level, xoffset, yoffset, width, height, 
								 psSubTexFormat, &ui32Face, &ui32Level);

	if(!psTex)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glCompressedTexSubImage2D);

		return;
	}

	psMipLevel = &psTex->psMipLevel[ui32Level];

	/* Must have at least 2x2 blocks */
	ui32Height = MAX((IMG_UINT32)height >> 2, 2);
	
	/* 2Bpp encodes 8x4 blocks, 4Bpp encodes 4x4 */
	if( (psSubTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTC2) || 
		(psSubTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCII2))
	{
		ui32Width = MAX((IMG_UINT32)width >> 3, 2);
	}
	else
	{
		ui32Width = MAX((IMG_UINT32)width >> 2, 2);
	}
	
	ui32Size = ui32Width * ui32Height << 3;

	if(imageSize != (GLint)ui32Size)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES2_TIME_STOP(GLES2_TIMES_glCompressedTexSubImage2D);

		return;
	}

	pui8Dest = psMipLevel->pui8Buffer;

	if(pui8Dest == GLES2_LOADED_LEVEL)
	{
		/* Only support fullsize subtexture */
		pui8Dest = GLES2MallocHeapUNC(gc, imageSize);
		
		if(!pui8Dest)
		{
			SetError(gc, GL_OUT_OF_MEMORY);
			GLES2_TIME_STOP(GLES2_TIMES_glCompressedTexSubImage2D);
			return;
		}

#if (defined(DEBUG) || defined(TIMING))
		ui32TextureMemCurrent += imageSize;

		if (ui32TextureMemCurrent>ui32TextureMemHWM)
		{
			ui32TextureMemHWM = ui32TextureMemCurrent;
		}
#endif	
		psMipLevel->pui8Buffer = pui8Dest;
	}

	if(data && height && width && pui8Dest)
	{
		IMG_VOID *pvDest = psMipLevel->pui8Buffer;

		CopyTexturePVRTC(pvDest, pvPixels, (IMG_UINT32)width, (IMG_UINT32)height, 0, psMipLevel, IMG_TRUE);
		
		GLES2_INC_PIXEL_COUNT(GLES2_TIMES_glCompressedTexSubImage2D, width*height);
	}

	TextureRemoveResident(gc, psTex);

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageSource || psTex->psEGLImageTarget)
	{
		UpdateEGLImage(gc, psTex, ui32Level);
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

	gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;


	GLES2_TIME_STOP(GLES2_TIMES_glCompressedTexSubImage2D);
}


/***********************************************************************************
 Function Name      : glCopyTexImage2D
 Inputs             : target, level, internalformat, x, y, width, height, border 
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Copies data from frambuffer to a texture. 
					  Host texture data memory is allocated and loaded immediately.
					  Will cause a render to take place. 
************************************************************************************/
GL_APICALL void GL_APIENTRY glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat,
										 GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
	GLES2Texture             *psTex;
	IMG_UINT32                ui32Face, ui32Level, i;
	const GLES2TextureFormat *psTexFormat;
	PFNSpanPack               pfnSpanPack;
	GLenum                    type, eTextureHWFormat;
	IMG_UINT8                 *pui8Dest;
	GLES2PixelSpanInfo        sSpanInfo = {0};
	IMG_VOID                  *pvSurfacePointer;
	EGLDrawableParams         *psReadParams;
	GLES2MipMapLevel          *psMipLevel;


	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glCopyTexImage2D"));

	GLES2_TIME_START(GLES2_TIMES_glCopyTexImage2D);

	/* Check arguments and get the right texture being changed */
	psTex = CheckTexImageArgs(gc, target, level, width, height, border, &ui32Face, &ui32Level);

	if (!psTex) 
	{
		GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);

		return;
	}

#if defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT)

	switch(internalformat)
	{
#if defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888)
		case GL_BGRA_EXT:
		{
			internalformat = GL_BGRA_EXT;

			break;
		}
#endif /* defined(GLES2_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
		case GL_RGBA8_OES:
		case GL_RGBA4:
		case GL_RGB5_A1:
		case GL_RGBA:
		{
			internalformat = GL_RGBA;

			break;
		}
		case GL_RGB8_OES:
		case GL_RGB565:
		case GL_RGB:
		{
			internalformat = GL_RGB;

			break;
		}
		case GL_LUMINANCE8_OES:
		case GL_LUMINANCE:
		{
			internalformat = GL_LUMINANCE;

			break;
		}
		case GL_ALPHA8_OES:
		case GL_ALPHA:
		{
			internalformat = GL_ALPHA;

			break;
		}
		case GL_LUMINANCE8_ALPHA8_OES:
		case GL_LUMINANCE4_ALPHA4_OES:
		case GL_LUMINANCE_ALPHA:
		{
			internalformat = GL_LUMINANCE_ALPHA;

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);

			return;
		}
	}

#else  /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

	switch(internalformat)
	{
		case GL_RGB:
		case GL_RGBA:
		case GL_ALPHA:
		case GL_LUMINANCE:
		case GL_LUMINANCE_ALPHA:
		{
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_VALUE);

			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);

			return;
		}
	}

#endif  /* defined(GLES2_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

	if(GetFrameBufferCompleteness(gc) != GL_FRAMEBUFFER_COMPLETE)
	{
		SetError(gc, GL_INVALID_FRAMEBUFFER_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);

		return;
	}

	psReadParams = gc->psReadParams;

	/* No read surface specified */
	if(!psReadParams->psRenderSurface)
	{
bad_op:
		SetError(gc, GL_INVALID_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);

		return;
	}


	/* Generally the texture hw format is the same as the internal format.
	 * In the case of RGB888 textures, we don't have a corresponding HW format, so we
	 * use RGBA8888 textures. This must be reflected in the setup of the copyspan
	 */
	eTextureHWFormat = internalformat;

	switch(psReadParams->ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		{
			switch(internalformat)
			{
				case GL_RGBA:
				{
					pfnSpanPack = SpanPackARGB8888toABGR8888;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatABGR8888;
					break;
				}
				case GL_RGB:
				{
					pfnSpanPack = SpanPackARGB8888toXBGR8888;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatXBGR8888;
					/* Actual HW format is RGBA */
					eTextureHWFormat = GL_RGBA;
					break;
				}
				case GL_LUMINANCE:
				{
					pfnSpanPack = SpanPackARGB8888toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				}
				case GL_ALPHA:
				{
					pfnSpanPack = SpanPackAXXX8888toAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatAlpha;
					break;
				}
				case GL_LUMINANCE_ALPHA:
				{
					pfnSpanPack = SpanPackARGB8888toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminanceAlpha;
					break;
				}
				default:
				{
					/* we should never reach this case */
					goto bad_op;
				}
			}
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		{
			switch(internalformat)
			{
				case GL_RGBA:
				{
					pfnSpanPack = SpanPack32;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatABGR8888;
					break;
				}
				case GL_RGB:
				{
					pfnSpanPack = SpanPackABGR8888toXBGR8888;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatXBGR8888;
					/* Actual HW format is RGBA */
					eTextureHWFormat = GL_RGBA;
					break;
				}
				case GL_LUMINANCE:
				{
					pfnSpanPack = SpanPackABGR8888toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				}
				case GL_ALPHA:
				{
					pfnSpanPack = SpanPackAXXX8888toAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatAlpha;
					break;
				}
				case GL_LUMINANCE_ALPHA:
				{
					pfnSpanPack = SpanPackABGR8888toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminanceAlpha;
					break;
				}
				default:
				{
					/* we should never reach this case */
					goto bad_op;
				}
			}

			break;
		}
		case PVRSRV_PIXEL_FORMAT_XRGB8888:
		{
			switch(internalformat)
			{
				case GL_RGB:
					/* X=junk to X=1, plus red/blue swapping */
					pfnSpanPack = SpanPackXRGB8888to1BGR8888;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatXBGR8888;
					/* Actual HW format is RGBA */
					eTextureHWFormat = GL_RGBA;
					break;
				case GL_LUMINANCE:
					/* L=R (so alpha is ignored) */
					pfnSpanPack = SpanPackARGB8888toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				default:
					/* we should never reach this case */
					goto bad_op;
			}

			break;
		}
		case PVRSRV_PIXEL_FORMAT_XBGR8888:
		{
			switch(internalformat)
			{
				case GL_RGB:
					/* X=junk to X=1 */
					pfnSpanPack = SpanPackXBGR8888to1BGR8888;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatXBGR8888;
					/* Actual HW format is RGBA */
					eTextureHWFormat = GL_RGBA;
					break;
				case GL_LUMINANCE:
					/* L=R (so alpha is ignored) */
					pfnSpanPack = SpanPackABGR8888toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				default:
					/* we should never reach this case */
					goto bad_op;
			}

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			switch(internalformat)
			{
				case GL_RGBA:
				{
					pfnSpanPack = SpanPack16;
					type = GL_UNSIGNED_SHORT_4_4_4_4;
					psTexFormat = &TexFormatARGB4444;
					break;
				}
				case GL_RGB:
				{
					pfnSpanPack = SpanPackARGB4444toRGB565;
					type = GL_UNSIGNED_SHORT_5_6_5;
					psTexFormat = &TexFormatRGB565;
					break;
				}
				case GL_LUMINANCE:
				{
					pfnSpanPack = SpanPackARGB4444toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				}
				case GL_ALPHA:
				{
					pfnSpanPack = SpanPackARGB4444toAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatAlpha;
					break;
				}
				case GL_LUMINANCE_ALPHA:
				{
					pfnSpanPack = SpanPackARGB4444toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminanceAlpha;
					break;
				}
				default:
				{
					/* we should never reach this case */
					goto bad_op;
				}
			}

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			switch(internalformat)
			{
				case GL_RGBA:
				{
					pfnSpanPack = SpanPack16;
					type = GL_UNSIGNED_SHORT_5_5_5_1;
					psTexFormat = &TexFormatARGB1555;
					break;
				}
				case GL_RGB:
				{
					pfnSpanPack = SpanPackARGB1555toRGB565;
					type = GL_UNSIGNED_SHORT_5_6_5;
					psTexFormat = &TexFormatRGB565;
					break;
				}
				case GL_LUMINANCE:
				{
					pfnSpanPack = SpanPackARGB1555toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				}
				case GL_ALPHA:
				{
					pfnSpanPack = SpanPackARGB1555toAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatAlpha;
					break;
				}
				case GL_LUMINANCE_ALPHA:
				{
					pfnSpanPack = SpanPackARGB1555toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminanceAlpha;
					break;
				}
				default:
				{
					/* we should never reach this case */
					goto bad_op;
				}
			}
			break;
		}
		case PVRSRV_PIXEL_FORMAT_RGB565:
		default:
		{
			switch(internalformat)
			{
				case GL_RGB:
				{
					pfnSpanPack = SpanPack16;
					type = GL_UNSIGNED_SHORT_5_6_5;
					psTexFormat = &TexFormatRGB565;
					break;
				}
				case GL_LUMINANCE:
				{
					pfnSpanPack = SpanPackRGB565toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				}
				default:
				{
					/* we should never reach this case */
					goto bad_op;
				}
			}
			break;
		}
	}
	

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageSource)
	{
		if (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
		{
			TexMgrGhostTexture(gc, psTex);
		}
		else
		{
			/* Inform EGL that the source is being orphaned */
			KEGLUnbindImage(psTex->psEGLImageSource->hImage);

			psTex->psMemInfo = IMG_NULL;
			psTex->psEGLImageSource = IMG_NULL;
		}
	}
	else if(psTex->psEGLImageTarget)
	{
		ReleaseImageFromTexture(gc, psTex);
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

	psMipLevel = &psTex->psMipLevel[ui32Level];


	/*********************************************************************************
	 *
	 * Two options to upload supplied data in glCopyTexImage2D
	 *   
	 *  1. If psTex's device memory has already been allocated and uploaded before, 
	 *     && psMipLevel specified by glCopyTexImage2D has exactly same size, 
	 *        levelNo and pixel format as the allocated psTex's certain level.
	 *     Then
	 *        try to use HWTQ Normal blit to transfer texture data from framebuffer
	 *        to texture device memory immediately;
	 *
	 *  2. Otherwise
	 *        use SW to copy back data from framebuffer to user intermediate buffer,
	 *        and then upload to texture device memory when ValidateState().
	 *
	 *********************************************************************************/


	/*********** Option 1 : HWTQ Normal Blit Solution ********************/

	if ( (!gc->sAppHints.bDisableHWTQNormalBlit)        &&
		 (psMipLevel->pui8Buffer == GLES2_LOADED_LEVEL) &&
		 (width != 0) && (height != 0)                  &&
		 (psMipLevel->ui32Width  == (IMG_UINT32)width)  &&
		 (psMipLevel->ui32Height == (IMG_UINT32)height) &&
		 ((psMipLevel->psTexFormat) == psTexFormat)      )
	{
		
		SGX_QUEUETRANSFER sQueueTransfer;
		
		IMG_UINT32 ui32OffsetInBytes = 0;
		IMG_UINT32 ui32Lod=(IMG_UINT32)level;

	    GLES2SubTextureInfo sSrcReadInfo;
		sSrcReadInfo.ui32SubTexXoffset = (IMG_UINT32)x;
		sSrcReadInfo.ui32SubTexYoffset = (IMG_UINT32)y;
		sSrcReadInfo.ui32SubTexWidth   = (IMG_UINT32)width;
		sSrcReadInfo.ui32SubTexHeight  = (IMG_UINT32)height;
		sSrcReadInfo.pui8SubTexBuffer  = IMG_NULL;
		

		if(psTex->ui32HWFlags & GLES2_NONPOW2)
		{
			/* Non-power-of-two texture: cannot be mipmapped nor a CEM */
			GLES_ASSERT(ui32Face == 0 && ui32Level == 0);

			ui32OffsetInBytes = 0;
		}
		else
		{
			GLES2TextureParamState  *psParams = &psTex->sState;
			const GLES2TextureFormat *psTexFmt = psTex->psFormat;
			IMG_UINT32 ui32BytesPerTexel = psTexFmt->ui32TotalBytesPerTexel;
			IMG_UINT32 ui32TopUsize, ui32TopVsize;
					
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
			ui32TopUsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
			ui32TopVsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
			ui32TopUsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
			ui32TopVsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

			/* setup ui32OffsetInBytes */
			if (psTex->ui32HWFlags & GLES2_MULTICHUNK)
			{
				IMG_UINT32 ui32BytesPerChunk = psTexFmt->ui32TotalBytesPerTexel / psTexFmt->ui32NumChunks;
				IMG_UINT32 ui32OffsetInChunkInBytes = ui32BytesPerChunk * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);

				for (i=0; i < psTexFmt->ui32NumChunks; i++)
				{
					/* Special case of chunks with different sizes */
					if(psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_B16G16R16F)
					{
						if(i==0)
						{
						    ui32BytesPerChunk = 4;
						}
						else
						{
						    ui32BytesPerChunk = 2;
						}

						ui32OffsetInChunkInBytes = ui32BytesPerChunk * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
					}
					
					if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
					{
						IMG_UINT32 ui32FaceOffset = 
						  ui32BytesPerChunk * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);
					
						if(psTex->ui32HWFlags & GLES2_MIPMAP)
						{
							if(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP)
							{
								ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
							}
						}

						ui32OffsetInChunkInBytes += (ui32FaceOffset * ui32Face);
					}

					ui32OffsetInBytes = (i * psTex->ui32ChunkSize) + ui32OffsetInChunkInBytes;
				}
			}
			else
		    {
				ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
								  
				if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
				{
					IMG_UINT32 ui32FaceOffset = 
					  ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);

					if(psTex->ui32HWFlags & GLES2_MIPMAP)
					{
						if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
						   (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
						{
							ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
						}
					}

					ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
				}
			}
		}

		/*
		 * The texture may become incomplete, so we need to kick the 3D and
		 * notify all framebuffers attached to this texture
		 */
		FBOAttachableHasBeenModified(gc, (GLES2FrameBufferAttachable*)psMipLevel);

		/* Make sure to flush the read render surface, not the write one */
		if(ScheduleTA(gc, psReadParams->psRenderSurface, GLES2_SCHEDULE_HW_MIDSCENE_RENDER | GLES2_SCHEDULE_HW_LAST_IN_SCENE) != IMG_EGL_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"glCopyTexImage2D: Couldn't flush HW"));

			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);

			return;
		}

		/* HWTQ normal blitting framebuffer surface to texture memory */
		if (PrepareHWTQTextureNormalBlit(gc, psTex, 
										 ui32OffsetInBytes,
										 psMipLevel,
										 IMG_NULL,
										 psReadParams,
										 &sSrcReadInfo,
										 &sQueueTransfer))
		{
			if (HWTQTextureNormalBlit(gc, psTex, psReadParams, &sQueueTransfer))
			{
				GLES2_INC_PIXEL_COUNT(GLES2_TIMES_glCopyTexImage2D, width*height);

				GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);

				return;
			}
		}
	}


	/*********** Option 1 : SW Solution ********************/
	{
		/* Allocate memory for the level data */
		pui8Dest = TextureCreateLevel(gc, psTex, ui32Level, internalformat,
									  psTexFormat, (IMG_UINT32)width, (IMG_UINT32)height);

		/* Copy image data */
		if (pui8Dest)
		{
			if(!SetupReadPixelsSpanInfo(gc, &sSpanInfo, x, y, (IMG_UINT32)width, (IMG_UINT32)height,
										eTextureHWFormat, type, IMG_FALSE, psReadParams))
			{
				GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);

				return;
			}

			/*
			 * The texture may become incomplete, so we need to kick the 3D and
			 * notify all framebuffers attached to this texture
			 */
			FBOAttachableHasBeenModified(gc, (GLES2FrameBufferAttachable*)psMipLevel);

			/* Make sure to flush the read render surface, not the write one */
			if(ScheduleTA(gc, psReadParams->psRenderSurface,
						  GLES2_SCHEDULE_HW_MIDSCENE_RENDER | GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_WAIT_FOR_3D) != IMG_EGL_NO_ERROR)
			{
				PVR_DPF((PVR_DBG_ERROR,"glCopyTexImage2D: Couldn't flush HW"));

				GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);

				return;
			}
			
			pvSurfacePointer = GetStridedSurfaceData(gc, psReadParams, &sSpanInfo);

			if(!pvSurfacePointer)
			{
				PVR_DPF((PVR_DBG_ERROR,"glCopyTexImage2D: Failed to get strided data"));

				GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);

				return;
			}
			
			sSpanInfo.pvOutData = (IMG_VOID *) ((psMipLevel->pui8Buffer) +
												sSpanInfo.ui32DstSkipLines * sSpanInfo.ui32DstRowIncrement +
												sSpanInfo.ui32DstSkipPixels * sSpanInfo.ui32DstGroupIncrement);

			sSpanInfo.pvInData = (IMG_VOID *) ((IMG_UINT8 *)pvSurfacePointer +
											   sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement +
											   sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);


			if ((sSpanInfo.ui32Height) && (sSpanInfo.ui32Width))
			{
				i = sSpanInfo.ui32Height;

				do
				{
				    (*pfnSpanPack)(&sSpanInfo);
				
					sSpanInfo.pvInData = (IMG_UINT8 *)sSpanInfo.pvInData + sSpanInfo.i32SrcRowIncrement;
					sSpanInfo.pvOutData = (IMG_UINT8 *)sSpanInfo.pvOutData + sSpanInfo.ui32DstRowIncrement;
				}
				while(--i);
			}
			
			if(pvSurfacePointer!=psReadParams->pvLinSurfaceAddress)
			{
				GLES2FreeAsync(gc, pvSurfacePointer);
			}
		}

		/*
		 * remove texture from active list.  If needed, validation will reload 
		 */
		TextureRemoveResident(gc, psTex);

		gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;

		GLES2_INC_PIXEL_COUNT(GLES2_TIMES_glCopyTexImage2D, width*height);
		GLES2_TIME_STOP(GLES2_TIMES_glCopyTexImage2D);
	}
}


/***********************************************************************************
 Function Name      : glCopyTexSubImage2D
 Inputs             : target, level, xoffset, yoffset, x, y, width, height
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Copies data from frambuffer to a subtexture region.
					  Host texture data memory is already allocated/may have already
					  been uploaded to HW. Subdata will need to be integrated/uploaded.
************************************************************************************/
GL_APICALL void GL_APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
	GLES2Texture *psTex;
	const GLES2TextureFormat *psTargetTexFormat;
	const GLES2TextureFormat *psTexFormat;
	PFNSpanPack pfnSpanPack;
	GLenum type;
	IMG_UINT8 *pui8Dest;
	GLES2PixelSpanInfo sSpanInfo = {0};
	IMG_VOID *pvSurfacePointer;
	IMG_UINT32 i, ui32Lod=(IMG_UINT32)level, ui32Level, ui32Face;
	IMG_UINT32 ui32DstStride;
	EGLDrawableParams *psReadParams;
	GLES2MipMapLevel *psMipLevel;
	GLenum eBaseHWTextureFormat;
	IMG_UINT32 ui32TopUsize, ui32TopVsize;

	IMG_BOOL bHWCopySubTextureUploaded = IMG_FALSE;


	__GLES2_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glCopyTexSubImage2D"));

	GLES2_TIME_START(GLES2_TIMES_glCopyTexSubImage2D);


	/* Check arguments and get the right texture being changed */
	psTex = CheckTexSubImageArgs(gc, target, level, xoffset, yoffset, width, height, 
								 IMG_NULL, &ui32Face, &ui32Level);

	if (!psTex)
	{
		GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);
		return;
	}

	psMipLevel = &psTex->psMipLevel[ui32Level];

	psTargetTexFormat = psMipLevel->psTexFormat;
	eBaseHWTextureFormat = psMipLevel->eRequestedFormat;
	
	switch(eBaseHWTextureFormat)
	{
		/* Our compressed texture spec is silent on what error to return here
		 * but we'll bow to precedent and set bad operation
		 */
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
		case GL_ETC1_RGB8_OES:
		{
bad_op:
			SetError(gc, GL_INVALID_OPERATION);

			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

			return;
		}
	}

	if(GetFrameBufferCompleteness(gc) != GL_FRAMEBUFFER_COMPLETE)
	{
		SetError(gc, GL_INVALID_FRAMEBUFFER_OPERATION);

		GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

		return;
	}

	psReadParams = gc->psReadParams;

	/* No read surface specified */
	if(!psReadParams->psRenderSurface)
	{
		goto bad_op;
	}

	if(psTargetTexFormat->ui32BaseFormatIndex == GLES2_RGB_TEX_INDEX)
	{
		/* Actual HW format is RGBA */
		eBaseHWTextureFormat = GL_RGBA;
	}


	/* We support conversion from any of our FB formats, 
	 * (including both 8888 orders - from binding a texture as an FBO), to any of our valid texture formats which
	 * has as many or less base internal components. ie, RGBA->RGBA/RGB/L/LA/A, RGB->RGB/L/LA/A.
	 */
	switch(psReadParams->ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_XRGB8888:
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		{
		    /* Setup psTexFormat */
		    psTexFormat = &TexFormatABGR8888;
					
			switch(psTargetTexFormat->ePixelFormat)
			{
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				{
					if(psTargetTexFormat->ui32BaseFormatIndex == GLES2_RGBA_TEX_INDEX)
					{
						pfnSpanPack = SpanPackARGB8888toABGR8888;
						type = GL_UNSIGNED_BYTE;
					}
					else
					{
						pfnSpanPack = SpanPackARGB8888toXBGR8888;
						type = GL_UNSIGNED_BYTE;
					}

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB8888:
				{
					pfnSpanPack = SpanPack32;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_RGB565:
				{
					pfnSpanPack = SpanPackARGB8888toRGB565;
					type = GL_UNSIGNED_SHORT_5_6_5;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB4444:
				{
					pfnSpanPack = SpanPackARGB8888toARGB4444;
					type = GL_UNSIGNED_SHORT_4_4_4_4;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB1555:
				{
					pfnSpanPack = SpanPackARGB8888toARGB1555;
					type = GL_UNSIGNED_SHORT_5_5_5_1;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_A8L8:
				{
					pfnSpanPack = SpanPackARGB8888toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_A8:
				{
					pfnSpanPack = SpanPackAXXX8888toAlpha;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_L8:
				{
					pfnSpanPack = SpanPackARGB8888toLuminance;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_ERROR,"Unknown HW format"));
					goto bad_op;
				}
			}

			break;
		}
		case PVRSRV_PIXEL_FORMAT_XBGR8888:
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		{
		    /* Setup psTexFormat */
		    psTexFormat = &TexFormatABGR8888;

			switch(psTargetTexFormat->ePixelFormat)
			{
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				{
					if(psTargetTexFormat->ui32BaseFormatIndex == GLES2_RGBA_TEX_INDEX)
					{
						pfnSpanPack = SpanPack32;
						type = GL_UNSIGNED_BYTE;
					}
					else
					{
						pfnSpanPack = SpanPackABGR8888toXBGR8888;
						type = GL_UNSIGNED_BYTE;
					}

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB8888:
				{
					pfnSpanPack = SpanPackABGR8888toARGB8888;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_RGB565:
				{
					pfnSpanPack = SpanPackABGR8888toRGB565;
					type = GL_UNSIGNED_SHORT_5_6_5;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB4444:
				{
					pfnSpanPack = SpanPackABGR8888toARGB4444;
					type = GL_UNSIGNED_SHORT_4_4_4_4;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB1555:
				{
					pfnSpanPack = SpanPackABGR8888toARGB1555;
					type = GL_UNSIGNED_SHORT_5_5_5_1;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_A8L8:
				{
					pfnSpanPack = SpanPackABGR8888toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_A8:
				{
					pfnSpanPack = SpanPackAXXX8888toAlpha;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_L8:
				{
					pfnSpanPack = SpanPackABGR8888toLuminance;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_ERROR,"Unknown HW format"));
					goto bad_op;
				}
			}

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
		    /* Setup psTexFormat */
		    psTexFormat = &TexFormatARGB4444;

			switch(psTargetTexFormat->ePixelFormat)
			{
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				{
					if(psTargetTexFormat->ui32BaseFormatIndex == GLES2_RGBA_TEX_INDEX)
					{
						pfnSpanPack = SpanPackARGB4444toABGR8888;
						type = GL_UNSIGNED_BYTE;
					}
					else
					{
						pfnSpanPack = SpanPackARGB4444toXBGR8888;
						type = GL_UNSIGNED_BYTE;
					}

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB8888:
				{
					pfnSpanPack = SpanPackARGB4444toARGB8888;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_RGB565:
				{
					pfnSpanPack = SpanPackARGB4444toRGB565;
					type = GL_UNSIGNED_SHORT_5_6_5;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB4444:
				{
					pfnSpanPack = SpanPack16;
					type = GL_UNSIGNED_SHORT_4_4_4_4;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB1555:
				{
					pfnSpanPack = SpanPackARGB4444toARGB1555;
					type = GL_UNSIGNED_SHORT_5_5_5_1;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_A8L8:
				{
					pfnSpanPack = SpanPackARGB4444toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_A8:
				{
					pfnSpanPack = SpanPackARGB4444toAlpha;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_L8:
				{
					pfnSpanPack = SpanPackARGB4444toLuminance;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_ERROR,"Unknown HW format"));
					goto bad_op;
				}
			}

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
		    /* Setup psTexFormat */
		    psTexFormat = &TexFormatARGB1555;

			switch(psTargetTexFormat->ePixelFormat)
			{
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				{
					if(psTargetTexFormat->ui32BaseFormatIndex == GLES2_RGBA_TEX_INDEX)
					{
						pfnSpanPack = SpanPackARGB1555toABGR8888;
						type = GL_UNSIGNED_BYTE;
					}
					else
					{
						pfnSpanPack = SpanPackARGB1555toXBGR8888;
						type = GL_UNSIGNED_BYTE;
					}

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB8888:
				{
					pfnSpanPack = SpanPackARGB1555toARGB8888;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_RGB565:
				{
					pfnSpanPack = SpanPackARGB1555toRGB565;
					type = GL_UNSIGNED_SHORT_5_6_5;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB4444:
				{
					pfnSpanPack = SpanPackARGB1555toARGB4444;
					type = GL_UNSIGNED_SHORT_4_4_4_4;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_ARGB1555:
				{
					pfnSpanPack = SpanPack16;
					type = GL_UNSIGNED_SHORT_5_5_5_1;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_A8L8:
				{
					pfnSpanPack = SpanPackARGB1555toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_A8:
				{
					pfnSpanPack = SpanPackARGB1555toAlpha;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_L8:
				{
					pfnSpanPack = SpanPackARGB1555toLuminance;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				default:
				{
					PVR_DPF((PVR_DBG_ERROR,"Unknown HW format"));
					goto bad_op;
				}
			}

			break;
		}
		case PVRSRV_PIXEL_FORMAT_RGB565:
		default:
		{
		    /* Setup psTexFormat */
		    psTexFormat = &TexFormatRGB565;

			switch(psTargetTexFormat->ePixelFormat)
			{
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				{
					if(psTargetTexFormat->ui32BaseFormatIndex == GLES2_RGB_TEX_INDEX)
					{
						pfnSpanPack = SpanPackRGB565toXBGR8888;
						type = GL_UNSIGNED_BYTE;
					}
					else
					{
						/* Can't convert from RGB to RGBA */
						goto bad_op;
					}

					break;
				}
				case PVRSRV_PIXEL_FORMAT_RGB565:
				{
					pfnSpanPack = SpanPack16;
					type = GL_UNSIGNED_SHORT_5_6_5;

					break;
				}
				case PVRSRV_PIXEL_FORMAT_L8:
				{
					pfnSpanPack = SpanPackRGB565toLuminance;
					type = GL_UNSIGNED_BYTE;

					break;
				}
				default:
				{
					goto bad_op;
				}
			}
			break;
		}
	}

#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageTarget)
	{
	    IMG_UINT32 ui32BufferSize;

	    GLES_ASSERT(psTex->psEGLImageTarget->ui32Width==psMipLevel->ui32Width && psTex->psEGLImageTarget->ui32Height==psMipLevel->ui32Height);
		GLES_ASSERT(psMipLevel->pui8Buffer==GLES2_LOADED_LEVEL)

		ui32DstStride = psMipLevel->ui32Width * psTargetTexFormat->ui32TotalBytesPerTexel;

	    ui32BufferSize = ui32DstStride * psMipLevel->ui32Height;
		
		PVRSRVLockMutex(gc->psSharedState->hTertiaryLock);

		psMipLevel->pui8Buffer = GLES2MallocHeapUNC(gc, ui32BufferSize);

		if(!psMipLevel->pui8Buffer)
		{
		    SetError(gc, GL_OUT_OF_MEMORY);

			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

			return;
		}

		if(!SetupReadPixelsSpanInfo(gc, &sSpanInfo, x, y, (IMG_UINT32)width, (IMG_UINT32)height, eBaseHWTextureFormat,
									type, IMG_FALSE, psReadParams))
		{
			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

			return;
		}

		if(ScheduleTA(gc, psReadParams->psRenderSurface, GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_WAIT_FOR_3D) != IMG_EGL_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Can't flush HW"));

			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

			return;
		}

		pvSurfacePointer = GetStridedSurfaceData(gc, psReadParams, &sSpanInfo);

		if(!pvSurfacePointer)
		{
			PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Failed to get strided data"));

			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

			return;
		}

		sSpanInfo.pvInData = (IMG_VOID *) ((IMG_UINT8 *)pvSurfacePointer +
									sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement +
									sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);

		sSpanInfo.ui32DstRowIncrement = ui32DstStride;
		sSpanInfo.ui32DstSkipPixels += (IMG_UINT32)xoffset;
		sSpanInfo.ui32DstSkipLines += (IMG_UINT32)yoffset;
		
		sSpanInfo.pvOutData = (IMG_VOID *) (psMipLevel->pui8Buffer +
											sSpanInfo.ui32DstSkipLines * sSpanInfo.ui32DstRowIncrement +
											sSpanInfo.ui32DstSkipPixels * sSpanInfo.ui32DstGroupIncrement);

		sSpanInfo.pvInData = (IMG_VOID *) ((IMG_UINT8 *)pvSurfacePointer +
										   sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement +
										   sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);

		i = sSpanInfo.ui32Height;
		
		do
		{
			(*pfnSpanPack)(&sSpanInfo);

			sSpanInfo.pvInData = (IMG_UINT8 *)sSpanInfo.pvInData + sSpanInfo.i32SrcRowIncrement;
			sSpanInfo.pvOutData = (IMG_UINT8 *)sSpanInfo.pvOutData + sSpanInfo.ui32DstRowIncrement;
		}
		while(--i);

		TranslateLevel(gc, psTex, 0, 0);

		GLES2FreeAsync(gc, psMipLevel->pui8Buffer);

		psMipLevel->pui8Buffer = GLES2_LOADED_LEVEL;

		if(pvSurfacePointer!=psReadParams->pvLinSurfaceAddress)
		{
			GLES2FreeAsync(gc, pvSurfacePointer);
		}

		PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

		GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

		return;
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */


	pui8Dest = psMipLevel->pui8Buffer;
	ui32DstStride = psMipLevel->ui32Width * psTargetTexFormat->ui32TotalBytesPerTexel;



	/***************************************************************************
	 * 
	 * There are three paths to upload subtexture data for glCopyTexSubImage2D 
	 *
	 * 1. Using HWTQ normal blit 
	 *    to directly blit texture resource from framebuffer to device memory;
	 *
	 * 2. Using SW + HWTQ texture upload
	 *    firstly use SW to readback data from framebuffer to user buffer,
	 *    then hse HWTQ texture upload to copy subtexture resource from
	 *    user buffer to device memory;
	 *
	 * 3. Using SW
	 *
	 **************************************************************************/


	/********** PATH 1: using HWTQ normal blit **********/

	if ( (!gc->sAppHints.bDisableHWTQNormalBlit) &&
		 (pui8Dest == GLES2_LOADED_LEVEL)        &&
		 (width != 0) && (height != 0)           &&
		 (psTargetTexFormat == psTexFormat)       )
	{
		SGX_QUEUETRANSFER sQueueTransfer;
		
		IMG_UINT32 ui32OffsetInBytes = 0;
		
		GLES2SubTextureInfo sSrcReadInfo;
		GLES2SubTextureInfo sDstSubTexInfo;

		sSpanInfo.i32ReadX 	= x;
		sSpanInfo.i32ReadY 	= y;
		sSpanInfo.ui32Width = (IMG_UINT32)width;
		sSpanInfo.ui32Height = (IMG_UINT32)height;

		if (!ClipReadPixels(&sSpanInfo, psReadParams))
		{
			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);
			return;
		}

		sSrcReadInfo.ui32SubTexXoffset = sSpanInfo.i32ReadX;
		sSrcReadInfo.ui32SubTexYoffset = sSpanInfo.i32ReadY;
		sSrcReadInfo.ui32SubTexWidth = sSpanInfo.ui32Width;
		sSrcReadInfo.ui32SubTexHeight = sSpanInfo.ui32Height;
		sSrcReadInfo.pui8SubTexBuffer = IMG_NULL;

		sDstSubTexInfo.ui32SubTexXoffset = (IMG_UINT32)xoffset;
		sDstSubTexInfo.ui32SubTexYoffset = (IMG_UINT32)yoffset;
		sDstSubTexInfo.ui32SubTexWidth   = sSpanInfo.ui32Width;
		sDstSubTexInfo.ui32SubTexHeight  = sSpanInfo.ui32Height;
		sDstSubTexInfo.pui8SubTexBuffer  = IMG_NULL;

		/* Setup ui32OffsetInBytes */

		if(psTex->ui32HWFlags & GLES2_NONPOW2)
		{
			/* Non-power-of-two texture: cannot be mipmapped nor a CEM */
			GLES_ASSERT(ui32Face == 0 && ui32Level == 0);

			ui32OffsetInBytes = 0;
		}
		else
		{
			GLES2TextureParamState  *psParams = &psTex->sState;
			const GLES2TextureFormat *psTexFmt = psTex->psFormat;
			IMG_UINT32 ui32BytesPerTexel = psTexFmt->ui32TotalBytesPerTexel;
					
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
			ui32TopUsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
			ui32TopVsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
			ui32TopUsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
			ui32TopVsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

			/* setup ui32OffsetInBytes */
			if (psTex->ui32HWFlags & GLES2_MULTICHUNK)
			{
				IMG_UINT32 ui32BytesPerChunk = psTexFmt->ui32TotalBytesPerTexel / psTexFmt->ui32NumChunks;
				IMG_UINT32 ui32OffsetInChunkInBytes = ui32BytesPerChunk * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);

				for (i=0; i < psTexFmt->ui32NumChunks; i++)
				{
					/* Special case of chunks with different sizes */
					if(psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_B16G16R16F)
					{
						if(i==0)
						{
						    ui32BytesPerChunk = 4;
						}
						else
						{
						    ui32BytesPerChunk = 2;
						}

						ui32OffsetInChunkInBytes = ui32BytesPerChunk * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
					}
					
					if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
					{
						IMG_UINT32 ui32FaceOffset = 
						  ui32BytesPerChunk * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);
					
						if(psTex->ui32HWFlags & GLES2_MIPMAP)
						{
							if(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP)
							{
								ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
							}
						}

						ui32OffsetInChunkInBytes += (ui32FaceOffset * ui32Face);
					}

					ui32OffsetInBytes = (i * psTex->ui32ChunkSize) + ui32OffsetInChunkInBytes;
				}
			}
			else
		    {
				ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
								  
				if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
				{
					IMG_UINT32 ui32FaceOffset = 
					  ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);

					if(psTex->ui32HWFlags & GLES2_MIPMAP)
					{
						if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
						   (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
						{
							ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
						}
					}

					ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
				}
			}
		}

		/*
		 * The texture may become incomplete, so we need to kick the 3D and
		 * notify all framebuffers attached to this texture
		 */
		FBOAttachableHasBeenModified(gc, (GLES2FrameBufferAttachable*)psMipLevel);

		/* Make sure to flush the read render surface, not the write one */
		if(ScheduleTA(gc, psReadParams->psRenderSurface, GLES2_SCHEDULE_HW_MIDSCENE_RENDER | GLES2_SCHEDULE_HW_LAST_IN_SCENE) != IMG_EGL_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Couldn't flush HW"));
			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);
			return;
		}

		/* HWTQ normal blitting framebuffer surface to texture memory */
		if (PrepareHWTQTextureNormalBlit(gc, psTex, 
										 ui32OffsetInBytes,
										 psMipLevel,
										 &sDstSubTexInfo,
										 psReadParams,
										 &sSrcReadInfo,
										 &sQueueTransfer))
		{
			if (HWTQTextureNormalBlit(gc, psTex, psReadParams, &sQueueTransfer))
			{
				/* bHWCopySubTextureUploaded = IMG_TRUE; */
				GLES2_INC_PIXEL_COUNT(GLES2_TIMES_glCopyTexSubImage2D, width*height);

				GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

				return;
			}
		}
	}

	/********** PATH 2 & 3: using SW + HWTQ texture upload & pure SW **********/

	/* 1ST STEP: read back the whole level old data */

	/* if the host copy of the whole level texture has not been uploaded 
	   do nothing about reading back the old data */

	if (pui8Dest != GLES2_LOADED_LEVEL)
	{
		if(!pui8Dest)
		{
			SetError(gc, GL_OUT_OF_MEMORY);

			GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

			return;
		}
	}

	else
	{
	    /* directly transfer the whole level old data into the device memory
		   if the host copy of the whole level texture has already been uploaded */
#if !defined(ANDROID)
	    if ( (!gc->sAppHints.bDisableHWTQTextureUpload) &&
			 (height) && (width) )  /* HWTQ cannot upload zero-sized (sub)texture */
		{
			PVRSRV_CLIENT_MEM_INFO sMemInfo  = {0};
			GLES2TextureParamState  *psParams = &psTex->sState;
			IMG_UINT32 ui32TexAddr;

			/* 
			   When HWTQ is used to upload this texture level,
			   only if the texture resource is being used in the current frame,
			   then NEED to ghost it and create new texture device memory,
			   otherwise NO NEED to ghost at all 
			*/
			if( psTex->psMemInfo && 
				(KRM_IsResourceInUse(&gc->psSharedState->psTextureManager->sKRM,
									 gc->psRenderSurface,
									 &gc->psRenderSurface->sRenderStatusUpdate,
									 &psTex->sResource)))
			{
			    sMemInfo = *psTex->psMemInfo;
	
				TexMgrGhostTexture(gc, psTex);
			}

			if (!psTex->psMemInfo)
			{
			    if(!CreateTextureMemory(gc, psTex))
				{
				    return;
				}

				ui32TexAddr = psTex->psMemInfo->sDevVAddr.uiAddr;

				psParams->aui32StateWord2[0] = 
				  (ui32TexAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
				  EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

				for(i=1; i < psTex->psFormat->ui32NumChunks; i++)
				{
				    psParams->aui32StateWord2[i] = 
					  ((ui32TexAddr + (psTex->ui32ChunkSize * i)) >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) <<
					  EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;
				}

				gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;
			}

			/* and copy the old device memory to the new memory */

			if(((IMG_UINT32)width  == psMipLevel->ui32Width) && 
			   ((IMG_UINT32)height == psMipLevel->ui32Height) &&
			   ((psTex->ui32HWFlags & GLES2_MIPMAP) == 0) &&
			   (psTex->ui32TextureTarget != GLES2_TEXTURE_TARGET_CEM))
			{
				FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psMipLevel, GLES2_SCHEDULE_HW_DISCARD_SCENE);
			}
			else
			{
			    /* Render to mipmap before reading it back */
			    FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psMipLevel, 
										GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_WAIT_FOR_3D);

				if ((sMemInfo.uAllocSize))  /* transfer the whole texture data of all levels from the old device memory to the new memory */
				{
				  CopyTextureData(gc, psTex, 0, &sMemInfo, 0, sMemInfo.uAllocSize);
				}
				/* else: MemInfo hasn't been ghosted, the original device memory is being used. */
			}
		}
#endif
	}


	/* 2ND STEP: Copy the new subtexture data */

	/* Setup for copy image data */

	if(!SetupReadPixelsSpanInfo(gc, &sSpanInfo, x, y, (IMG_UINT32)width, (IMG_UINT32)height, eBaseHWTextureFormat,
								type, IMG_FALSE, psReadParams))
	{
		GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

		return;
	}

	if(ScheduleTA(gc, psReadParams->psRenderSurface, 
		GLES2_SCHEDULE_HW_MIDSCENE_RENDER | GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_WAIT_FOR_3D) != IMG_EGL_NO_ERROR)
	{
		PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Can't flush HW"));

		GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

		return;
	}

	pvSurfacePointer = GetStridedSurfaceData(gc, psReadParams, &sSpanInfo);

	if(!pvSurfacePointer)
	{
		PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Failed to get strided data"));

		GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

		return;
	}


	sSpanInfo.pvInData = (IMG_VOID *) ((IMG_UINT8 *)pvSurfacePointer +
								sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement +
								sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);


	/* if the host copy of the whole level texture has not been uploaded 
	   copy the new subtexture data to the host memory */

	if(pui8Dest != GLES2_LOADED_LEVEL)
	{
	    /* Stride is actually texture level stride, not read area stride */
	    sSpanInfo.ui32DstRowIncrement = ui32DstStride;
		sSpanInfo.ui32DstSkipPixels += (IMG_UINT32)xoffset;
		sSpanInfo.ui32DstSkipLines += (IMG_UINT32)yoffset;

		sSpanInfo.pvOutData = (IMG_VOID *) ((psMipLevel->pui8Buffer) +
						    sSpanInfo.ui32DstSkipLines * sSpanInfo.ui32DstRowIncrement +
						    sSpanInfo.ui32DstSkipPixels * sSpanInfo.ui32DstGroupIncrement);

		if ((sSpanInfo.ui32Height) && (sSpanInfo.ui32Width))
		{
		    i = sSpanInfo.ui32Height;

			do
			{
			    (*pfnSpanPack)(&sSpanInfo);

				sSpanInfo.pvInData = (IMG_UINT8 *)sSpanInfo.pvInData + sSpanInfo.i32SrcRowIncrement;
				sSpanInfo.pvOutData = (IMG_UINT8 *)sSpanInfo.pvOutData + sSpanInfo.ui32DstRowIncrement;
			}
			while(--i);
		}
	}

	else
	{
	    /* directly upload subtexture data into the device memory,
		   if the host copy of the whole level texture has already been uploaded. */
#if !defined(ANDROID)
	    if ( (!gc->sAppHints.bDisableHWTQTextureUpload)     &&
			 (sSpanInfo.ui32Height) && (sSpanInfo.ui32Width) ) /* HWTQ cannot upload zero-sized (sub)texture */
		{
			GLES2TextureParamState  *psParams = &psTex->sState;
			const GLES2TextureFormat *psTexFmt = psTex->psFormat;
			IMG_UINT32 ui32BytesPerTexel = psTexFmt->ui32TotalBytesPerTexel;
			IMG_UINT32 ui32OffsetInBytes = 0;

			IMG_UINT32 ui32SubTexBufferSize = (IMG_UINT32)(sSpanInfo.ui32Width * sSpanInfo.ui32Height * ui32BytesPerTexel);
			IMG_UINT8 *pui8SubTexBuffer = GLES2MallocHeapUNC(gc, ui32SubTexBufferSize);

			SGX_QUEUETRANSFER sQueueTransfer;

			GLES2SubTextureInfo sSubTexInfo;
			sSubTexInfo.ui32SubTexXoffset = (IMG_UINT32)xoffset;
			sSubTexInfo.ui32SubTexYoffset = (IMG_UINT32)yoffset;
			sSubTexInfo.ui32SubTexWidth   = sSpanInfo.ui32Width;
			sSubTexInfo.ui32SubTexHeight  = sSpanInfo.ui32Height;
			sSubTexInfo.pui8SubTexBuffer  = pui8SubTexBuffer;

			if (!pui8SubTexBuffer)
			{
			    SetError(gc, GL_OUT_OF_MEMORY);

				GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

				return;
			}

#if (defined(DEBUG) || defined(TIMING))
			ui32TextureMemCurrent += ui32SubTexBufferSize;
			
			if (ui32TextureMemCurrent>ui32TextureMemHWM)
			{
			    ui32TextureMemHWM = ui32TextureMemCurrent;
			}
#endif
			sSpanInfo.ui32DstRowIncrement = sSpanInfo.ui32Width * ui32BytesPerTexel;
			/*
			sSpanInfo.ui32DstSkipPixels += 0;
			sSpanInfo.ui32DstSkipLines += 0;
			*/
			sSpanInfo.pvOutData = (IMG_VOID *) (pui8SubTexBuffer);
			

			i = sSpanInfo.ui32Height;
				
			do
			{
				(*pfnSpanPack)(&sSpanInfo);
						
				sSpanInfo.pvInData = (IMG_UINT8 *)sSpanInfo.pvInData + sSpanInfo.i32SrcRowIncrement;
				sSpanInfo.pvOutData = (IMG_UINT8 *)sSpanInfo.pvOutData + sSpanInfo.ui32DstRowIncrement;
			}
			while(--i);


			if(psTex->ui32HWFlags & GLES2_NONPOW2)
			{
			    /* Non-power-of-two texture: cannot be mipmapped nor a CEM */
			    GLES_ASSERT(ui32Face == 0 && ui32Lod == 0);

				ui32OffsetInBytes = 0;
				
				if (PrepareHWTQTextureUpload(gc, psTex, 
											 ui32OffsetInBytes, psMipLevel,
											 &sSubTexInfo, IMG_NULL,
											 0, pui8SubTexBuffer, &sQueueTransfer))
				{
					if (HWTQTextureUpload(gc, psTex, &sQueueTransfer))
					{
						bHWCopySubTextureUploaded = IMG_TRUE;

						/* Reset bResidence, to prevent going into TextureMakeResident() */
						psTex->bResidence = IMG_TRUE;
					}
				}

			}
			else
			{
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
				ui32TopUsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
				ui32TopVsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
				ui32TopUsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
				ui32TopVsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

				/* setup ui32OffsetInBytes */
				if (psTex->ui32HWFlags & GLES2_MULTICHUNK)
				{
					IMG_UINT32 ui32BytesPerChunk = psTexFmt->ui32TotalBytesPerTexel / psTexFmt->ui32NumChunks;
					IMG_UINT32 ui32OffsetInChunkInBytes = ui32BytesPerChunk * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
			
					for (i=0; i < psTexFmt->ui32NumChunks; i++)
					{
						/* Special case of chunks with different sizes */
						if(psTexFmt->ePixelFormat == PVRSRV_PIXEL_FORMAT_B16G16R16F)
						{
							if(i==0)
							{
								ui32BytesPerChunk = 4;
							}
							else
							{
								ui32BytesPerChunk = 2;
							}

							ui32OffsetInChunkInBytes = ui32BytesPerChunk * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
						}

						if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
						{
						    IMG_UINT32 ui32FaceOffset = 
							  ui32BytesPerChunk * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);
					
							if(psTex->ui32HWFlags & GLES2_MIPMAP)
							{
								if(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP)
								{
									ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
								}
							}

							ui32OffsetInChunkInBytes += (ui32FaceOffset * ui32Face);
						}

						ui32OffsetInBytes = (i * psTex->ui32ChunkSize) + ui32OffsetInChunkInBytes;
					}
				}
				else
				{
				    ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
								  
					if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
					{
					    IMG_UINT32 ui32FaceOffset = 
						  ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);

						if(psTex->ui32HWFlags & GLES2_MIPMAP)
						{
							if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
								(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
							{
								ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
							}
						}

						ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
					}
				}
				
				if (PrepareHWTQTextureUpload(gc, psTex, 
											 ui32OffsetInBytes, psMipLevel,
											 &sSubTexInfo, IMG_NULL,
											 0, pui8SubTexBuffer, &sQueueTransfer))
				{
					if (HWTQTextureUpload(gc, psTex, &sQueueTransfer))
					{
						bHWCopySubTextureUploaded = IMG_TRUE;

						/* Reset bResidence, to prevent going into TextureMakeResident() */
						psTex->bResidence = IMG_TRUE;
					}
				}
			}

			GLES2FreeAsync(gc, pui8SubTexBuffer);
		}
#endif

		/* otherwise, use the software copy subtexture uploading:
		   readback the whole level texture data,
		   and copy subtexture data to the host memory. */

		if(!bHWCopySubTextureUploaded)
		{
			if ((sSpanInfo.ui32Height) && (sSpanInfo.ui32Width))
			{
			    IMG_UINT32 ui32BufferSize = ui32DstStride * psMipLevel->ui32Height;
				pui8Dest = GLES2MallocHeapUNC(gc, ui32BufferSize);

				if(!pui8Dest)
				{
					SetError(gc, GL_OUT_OF_MEMORY);

					GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);

					return;
				}

#if (defined(DEBUG) || defined(TIMING))
				ui32TextureMemCurrent += ui32BufferSize;

				if (ui32TextureMemCurrent>ui32TextureMemHWM)
				{
					ui32TextureMemHWM = ui32TextureMemCurrent;
				}
#endif
			    /* Don't need to read back for full size subtexture */
			    if(((IMG_UINT32)width  == psMipLevel->ui32Width) && ((IMG_UINT32)height == psMipLevel->ui32Height))
				{
				    FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psMipLevel, GLES2_SCHEDULE_HW_DISCARD_SCENE);
				}
				else
				{
				    /* Render to mipmap before reading it back */
				    FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psMipLevel, 
											GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_WAIT_FOR_3D);

					ReadBackTextureData(gc, psTex, ui32Face, (IMG_UINT32)level, pui8Dest);
				}

				psMipLevel->pui8Buffer = pui8Dest;

				{
					/* Use SpanInfo which has already been set using SetupReadPixelsSpanInfo */

					/* Stride is actually texture level stride, not read area stride */
					sSpanInfo.ui32DstRowIncrement = ui32DstStride;
					sSpanInfo.ui32DstSkipPixels += (IMG_UINT32)xoffset;
					sSpanInfo.ui32DstSkipLines += (IMG_UINT32)yoffset;
					
					sSpanInfo.pvOutData = (IMG_VOID *) (psMipLevel->pui8Buffer +
														sSpanInfo.ui32DstSkipLines * sSpanInfo.ui32DstRowIncrement +
														sSpanInfo.ui32DstSkipPixels * sSpanInfo.ui32DstGroupIncrement);

					sSpanInfo.pvInData = (IMG_VOID *) ((IMG_UINT8 *)pvSurfacePointer +
													   sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement +
													   sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);

					i = sSpanInfo.ui32Height;
					
					do
					{
						(*pfnSpanPack)(&sSpanInfo);

						sSpanInfo.pvInData = (IMG_UINT8 *)sSpanInfo.pvInData + sSpanInfo.i32SrcRowIncrement;
						sSpanInfo.pvOutData = (IMG_UINT8 *)sSpanInfo.pvOutData + sSpanInfo.ui32DstRowIncrement;
					}
					while(--i);
				}
			}

			TextureRemoveResident(gc, psTex);

			gc->ui32DirtyState |= GLES2_DIRTYFLAG_TEXTURE_STATE;
		}
	}


#if defined(GLES2_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageSource || psTex->psEGLImageTarget)
	{
	    UpdateEGLImage(gc, psTex, ui32Level);
	}
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */

	if(pvSurfacePointer!=psReadParams->pvLinSurfaceAddress)
	{
		GLES2FreeAsync(gc, pvSurfacePointer);
	}

	GLES2_INC_PIXEL_COUNT(GLES2_TIMES_glCopyTexSubImage2D, width*height);
	GLES2_TIME_STOP(GLES2_TIMES_glCopyTexSubImage2D);
}

/******************************************************************************
 End of file (tex.c)
******************************************************************************/


