/******************************************************************************
 * Name         : tex.c
 *
 * Copyright    : 2003-2009 by Imagination Technologies Limited.
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
#include "texformat.h"
#include "spanpack.h"
#include "drveglext.h"

#if defined(GLES1_EXTENSION_PVRTC)
/***********************************************************************************
 Function Name      : CopyTexturePVRTC
 Inputs             : pui32Src, ui32Width, ui32Height
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies PVRTC texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexturePVRTC(IMG_UINT32 *pui32Dest, const IMG_UINT32 *pui32Src,
				       IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				       IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
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
#endif /* defined(GLES1_EXTENSION_PVRTC) */


#if defined(GLES1_EXTENSION_ETC1)
/***********************************************************************************
 Function Name      : CopyTextureETC1
 Inputs             : pui32Src, ui32Width, ui32Height
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies ETC1 texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureETC1(IMG_UINT32 *pui32Dest, IMG_UINT32 *pui32Src,
				      IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				      IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				      IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i, ui32WidthInBlocks, ui32HeightInBlocks;
	IMG_UINT8 *pui8Src, *pui8Dst;

	PVR_UNREFERENCED_PARAMETER(ui32SrcStrideInBytes);
	PVR_UNREFERENCED_PARAMETER(psMipLevel);
	PVR_UNREFERENCED_PARAMETER(bUseDstStride);

	/* Blocks are 4x4 texels */
	ui32WidthInBlocks  = MAX(ui32Width >> 2, 1);
	ui32HeightInBlocks = MAX(ui32Height >> 2, 1);

	pui8Src = (IMG_UINT8 *)pui32Src;
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
#endif /* defined(GLES1_EXTENSION_ETC1) */


#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
/***********************************************************************************
 Function Name      : CopyTexture8888BGRAto8888RGBA
 Inputs             : pui32Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies 32 bit texture data, swapping the R and B components
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture8888BGRAto8888RGBA(IMG_UINT32 *pui32Dest, IMG_UINT32 *pui32Src,
													IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
													IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
													IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT32))) >> 2;
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
		pui32Src = pui32Src + ui32SrcRowIncrement;

	}
	while(--ui32Height);
}
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */


/***********************************************************************************
 Function Name      : CopyTexture32Bits
 Inputs             : pui32Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies 32 bit texture data
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture32Bits(IMG_UINT32 *pui32Dest, IMG_UINT32 *pui32Src,
					IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
					IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT32))) >> 2;
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
		pui32Src = pui32Src + ui32SrcRowIncrement;

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
IMG_INTERNAL IMG_VOID CopyTexture16Bits(IMG_UINT16 *pui16Dest, IMG_UINT16 *pui16Src,
					IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
					IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16))) >> 1;
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
		pui16Src = pui16Src + ui32SrcRowIncrement;

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
IMG_INTERNAL IMG_VOID CopyTexture8Bits(IMG_UINT8 *pui8Dest, IMG_UINT8 *pui8Src,
				       IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				       IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
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

/***********************************************************************************
 Function Name      : CopyTexture5551
 Inputs             : pui16Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 5551 to RGBA 1555 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture5551(IMG_UINT16 *pui16Dest, IMG_UINT16 *pui16Src,
				      IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				      IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				      IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16))) >> 1;
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
		pui16Src = pui16Src + ui32SrcRowIncrement;

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
IMG_INTERNAL IMG_VOID CopyTexture4444(IMG_UINT16 *pui16Dest, IMG_UINT16 *pui16Src,
				      IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				      IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
				      IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16))) >> 1;
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
		pui16Src = pui16Src + ui32SrcRowIncrement;

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
IMG_INTERNAL IMG_VOID CopyTexture888X(IMG_UINT8 *pui8Dest, IMG_UINT8 *pui8Src,
				      IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
				      IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
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
IMG_INTERNAL IMG_VOID CopyTexture888to565(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src,
					  IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					  IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
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
IMG_INTERNAL IMG_VOID CopyTexture565toRGBX8888(IMG_UINT8 *pui8Dest, IMG_UINT16 *pui16Src,
					       IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					       IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
					       IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 ui16Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16))) >> 1;
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
			ui8OutData  = (IMG_UINT8) ((ui16Temp >> 11) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++ = ui8OutData;

			/* Green */
			ui8OutData  = (IMG_UINT8) ((ui16Temp >>  5) << 2);
			ui8OutData = ui8OutData | (ui8OutData >> 6);
			*pui8Dest++ = ui8OutData;

			/* Blue */
			ui8OutData   = (IMG_UINT8) ((ui16Temp >> 0) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++  = ui8OutData;

			/* Alpha */
			*pui8Dest++ = 0xFF;
		}
		while(--i);

		pui8Dest += ui32DstRowIncrement;
		pui16Src = pui16Src + ui32SrcRowIncrement;
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
IMG_INTERNAL IMG_VOID CopyTexture5551to4444(IMG_UINT16 *pui16Dest, IMG_UINT16 *pui16Src, 
					    IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
					    IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
					    IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	
	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16))) >> 1;
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
			*pui16Dest  = (IMG_UINT16)(((*pui16Src & 0xF800) >> 12) << 8);

			/* Green */
			*pui16Dest |= (IMG_UINT16)(((*pui16Src & 0x07C0) >> 7) << 4); 

			/* Blue */
			*pui16Dest |= (IMG_UINT16)(((*pui16Src & 0x003E) >> 2) << 0);

			/* Alpha */
			*pui16Dest |= (IMG_UINT16)((*pui16Src & 0x0001) ? 0xF000 : 0x0000); 

			pui16Dest++;
			pui16Src++;
		}
		while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui16Src = pui16Src + ui32SrcRowIncrement;
	}
	while(--ui32Height);
}


#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
/***********************************************************************************
 Function Name      : CopyTexture5551toBGRA8888
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 5551 to BGRA 8888 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture5551toBGRA8888(IMG_UINT8 *pui8Dest, IMG_UINT16 *pui16Src,
						IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
						IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 ui16Temp;
	
	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16))) >> 1;
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
			ui8OutData   = (IMG_UINT8) ((ui16Temp >> 1) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++  = ui8OutData;

			/* Green */
			ui8OutData  = (IMG_UINT8) ((ui16Temp >>  6) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++ = ui8OutData;

			/* Red */
			ui8OutData  = (IMG_UINT8) ((ui16Temp >> 11) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++ = ui8OutData;

			/* Alpha */
			ui8OutData   = (IMG_UINT8)((ui16Temp & 0x0001) ? 0xFFU : 0x0);
			*pui8Dest++  = ui8OutData;
		}
		while(--i);

		pui8Dest += ui32DstRowIncrement;
		pui16Src = pui16Src + ui32SrcRowIncrement;
	}
	while(--ui32Height);
}
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */


/***********************************************************************************
 Function Name      : CopyTexture5551toRGBA8888
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 5551 to RGBA 8888 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture5551toRGBA8888(IMG_UINT8 *pui8Dest, IMG_UINT16 *pui16Src,
						IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
						IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 ui16Temp;
	
	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16))) >> 1;
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
			ui8OutData  = (IMG_UINT8) ((ui16Temp >> 11) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++ = ui8OutData;

			/* Green */
			ui8OutData  = (IMG_UINT8) ((ui16Temp >>  6) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++ = ui8OutData;

			/* Blue */
			ui8OutData   = (IMG_UINT8) ((ui16Temp >> 1) << 3);
			ui8OutData = ui8OutData | (ui8OutData >> 5);
			*pui8Dest++  = ui8OutData;

			/* Alpha */
			ui8OutData   = (IMG_UINT8)((ui16Temp & 0x0001) ? 0xFFU : 0x0);
			*pui8Dest++  = ui8OutData;
		}
		while(--i);

		pui8Dest += ui32DstRowIncrement;
		pui16Src = pui16Src + ui32SrcRowIncrement;
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
IMG_INTERNAL IMG_VOID CopyTexture4444to5551(IMG_UINT16 *pui16Dest, IMG_UINT16 *pui16Src,
					    IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
					    IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
					    IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16))) >> 1;
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
			*pui16Dest = (IMG_UINT16)(((*pui16Src & 0xF000) >> 12) << 11);
			*pui16Dest = *pui16Dest | ((*pui16Dest & 0x4000U) >> 4);

			/* Green */
			*pui16Dest |= (IMG_UINT16)(((*pui16Src & 0x0F00) >> 8) << 6); 
			*pui16Dest = *pui16Dest | ((*pui16Dest & 0x0200U) >> 4);

			/* Blue */
			*pui16Dest |= (IMG_UINT16)(((*pui16Src & 0x00F0) >> 4) << 1);
			*pui16Dest = *pui16Dest | ((*pui16Dest & 0x0010U) >> 4);

			/* Alpha */
			*pui16Dest |= (IMG_UINT16)(((*pui16Src & 0x000F) >> 3) << 15); 

			pui16Src++;
			pui16Dest++;
		}
		while(--i);

		pui16Dest += ui32DstRowIncrement;
		pui16Src = pui16Src + ui32SrcRowIncrement;
	}
	while(--ui32Height);
}


#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
/***********************************************************************************
 Function Name      : CopyTexture4444toBGRA8888
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 4444 to BGRA 8888 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture4444toBGRA8888(IMG_UINT8 *pui8Dest, IMG_UINT16 *pui16Src,
						IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
						IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 ui16Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16))) >> 1;
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
			
			/* Blue */
			ui8OutData   = (IMG_UINT8) ((ui16Temp >>  4) << 4);
			ui8OutData   = ui8OutData | (ui8OutData >> 4);
			*pui8Dest++  = ui8OutData;

			/* Green */
			ui8OutData  = (IMG_UINT8) ((ui16Temp >>  8) << 4);
			ui8OutData   = ui8OutData | (ui8OutData >> 4);
			*pui8Dest++ = ui8OutData;

			/* Red */
			ui8OutData  = (IMG_UINT8) ((ui16Temp >> 12) << 4);
			ui8OutData   = ui8OutData | (ui8OutData >> 4);
			*pui8Dest++ = ui8OutData;

			/* Alpha */
			ui8OutData  = (IMG_UINT8) ((ui16Temp >>  0) << 4);
			ui8OutData   = ui8OutData | (ui8OutData >> 4);
			*pui8Dest++ = ui8OutData;
		}
		while(--i);

		pui8Dest += ui32DstRowIncrement;
		pui16Src = pui16Src + ui32SrcRowIncrement;
	}
	while(--ui32Height);
}
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */


/***********************************************************************************
 Function Name      : CopyTexture4444toRGBA8888
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA 4444 to RGBA 8888 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTexture4444toRGBA8888(IMG_UINT8 *pui8Dest, IMG_UINT16 *pui16Src, 
						IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
						IMG_BOOL bUseDstStride)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 ui16Temp;

	/* Difference between stride and copied width */
	IMG_UINT32 ui32SrcRowIncrement = (ui32SrcStrideInBytes - (ui32Width * sizeof(IMG_UINT16))) >> 1;
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
		pui16Src = pui16Src + ui32SrcRowIncrement;
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
IMG_INTERNAL IMG_VOID CopyTextureRGBA8888to5551(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src,
						IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
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
		pui8Src = pui8Src + ui32SrcRowByteIncrement;
	}
	while(--ui32Height);
}


#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
/***********************************************************************************
 Function Name      : CopyTextureBGRA8888to5551
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from BGRA 8888 to ARGB 1555 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureBGRA8888to5551(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src,
						IMG_UINT32 ui32Width,  IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
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
		pui8Src = pui8Src + ui32SrcRowByteIncrement;
	}
	while(--ui32Height);
}
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */


/***********************************************************************************
 Function Name      : CopyTexture8888to4444
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from RGBA8888 to ARGB4444 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureRGBA8888to4444(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src,
						IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
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


#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
/***********************************************************************************
 Function Name      : CopyTextureBGRA8888to4444
 Inputs             : pui8Src, ui32Width, ui32Height,
					  ui32SrcStrideInBytes, psMipLevel
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from BGRA8888 to ARGB4444 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID CopyTextureBGRA8888to4444(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src,
						IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
						IMG_UINT32 ui32SrcStrideInBytes, GLESMipMapLevel *psMipLevel,
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
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */




#if defined(NO_UNALIGNED_ACCESS)
/***********************************************************************************
 Function Name      : READ_UNALIGNED_32
 Inputs             : pui32Val
 Returns            : 32bit value pointed to by pui32Val
 Description        : If pui32Val isn't DWORD aligned, reads bytes, otherwise reads DWORD
************************************************************************************/
static IMG_UINT32 READ_UNALIGNED_32(const IMG_UINT32 *pui32Val)
{	// Cast pointer to INT32 and see if it's already DWORD aligned
	if ((((IMG_UINT32)pui32Val)&3)==0)
	{	// If so then it's safe to just read and return the DWORD
		return *pui32Val;
	}
	else
	{	// Otherwise, build it up from the individual bytes
		const IMG_UINT8 * pui8Val=(const IMG_UINT8 *)pui32Val;
		return	(((IMG_UINT32)pui8Val[3])<<24)|
				(((IMG_UINT32)pui8Val[2])<<16)|
				(((IMG_UINT32)pui8Val[1])<<8)|
				 ((IMG_UINT32)pui8Val[0]);
	}	
}


/***********************************************************************************
 Function Name      : READ_UNALIGNED_16
 Inputs             : pui16Val
 Returns            : 16bit value pointed to by pui16Val
 Description        : If pui16Val isn't WORD aligned, reads bytes, otherwise reads WORD
************************************************************************************/
static IMG_UINT16 READ_UNALIGNED_16(const IMG_UINT16 *pui16Val)
{	// Cast pointer to INT32 and see if it's already WORD aligned
	if ((((IMG_UINT32)pui16Val)&1)==0)
	{	// If so then it's safe to just read and return the WORD
		return *pui16Val;
	}
	else
	{	// Otherwise, build it up from the individual bytes
		const IMG_UINT8 * pui8Val=(const IMG_UINT8 *)pui16Val;
		return (((IMG_UINT16)pui8Val[1])<<8)|pui8Val[0];
	}
}
#else
#define READ_UNALIGNED_32(pui32Val) ((IMG_UINT32)(*(const IMG_UINT32 *)(pui32Val)))
#define READ_UNALIGNED_16(pui16Val) ((IMG_UINT16)(*(const IMG_UINT16 *)(pui16Val)))
#endif


/***********************************************************************************
 Function Name      : Copy888Palette4Span
 Inputs             : pui8Src, ui32Width, pvPalette
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies texture data from 4bit Palette with 888 format
					  to RGBA8888 HW format
************************************************************************************/
static IMG_VOID Copy888Palette4Span(IMG_UINT32 *pui32Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Width,
									const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32Temp;
	IMG_UINT8 ui8Invalue0, ui8Invalue1;
	const IMG_UINT8 *pui8Palette = (const IMG_UINT8 *)pvPalette;
	IMG_UINT8 aui8PalTemp[3];

	i = ui32Width;

	do
	{
		ui8Invalue0 = *pui8Src++;
		ui8Invalue1 = ui8Invalue0 & 0xF;
		ui8Invalue0 >>= 4;

		aui8PalTemp[0] = pui8Palette[ui8Invalue0*3];
		aui8PalTemp[1] = pui8Palette[ui8Invalue0*3+1];
		aui8PalTemp[2] = pui8Palette[ui8Invalue0*3+2];

		ui32Temp = 0xFF000000 | (aui8PalTemp[2] << 16) | (aui8PalTemp[1] << 8) | aui8PalTemp[0];

		*pui32Dest++ = ui32Temp;

		aui8PalTemp[0] = pui8Palette[ui8Invalue1*3];
		aui8PalTemp[1] = pui8Palette[ui8Invalue1*3+1];
		aui8PalTemp[2] = pui8Palette[ui8Invalue1*3+2];

		ui32Temp = 0xFF000000 | (aui8PalTemp[2] << 16) | (aui8PalTemp[1] << 8) | aui8PalTemp[0];

		*pui32Dest++ = ui32Temp;

		i -= 2;
	}
	while(i);

}


/***********************************************************************************
 Function Name      : Copy888Palette4Level1xN
 Inputs             : pui8Src, ui32Height, pvPalette
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies texture data from 4bit Palette with 888 format
					  to RGBA8888 HW format, whole level 1xN
************************************************************************************/
static IMG_VOID Copy888Palette4Level1xN(IMG_UINT32 *pui32Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Height,
									const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32Temp;
	IMG_UINT8 ui8Invalue0, ui8Invalue1;
	const IMG_UINT8 *pui8Palette = (const IMG_UINT8 *)pvPalette;
	IMG_UINT8 aui8PalTemp[3];

	if(ui32Height == 1)
	{
		ui8Invalue0 = *pui8Src;
		ui8Invalue0 >>= 4;

		aui8PalTemp[0] = pui8Palette[ui8Invalue0*3];
		aui8PalTemp[1] = pui8Palette[ui8Invalue0*3+1];
		aui8PalTemp[2] = pui8Palette[ui8Invalue0*3+2];

		ui32Temp = 0xFF000000 | (aui8PalTemp[2] << 16) | (aui8PalTemp[1] << 8) | aui8PalTemp[0];

		*pui32Dest = ui32Temp;

	}
	else
	{
		i = ui32Height;

		do
		{
			ui8Invalue0 = *pui8Src++;
			ui8Invalue1 = ui8Invalue0 & 0xF;
			ui8Invalue0 >>= 4;

			aui8PalTemp[0] = pui8Palette[ui8Invalue0*3];
			aui8PalTemp[1] = pui8Palette[ui8Invalue0*3+1];
			aui8PalTemp[2] = pui8Palette[ui8Invalue0*3+2];

			ui32Temp = 0xFF000000 | (aui8PalTemp[2] << 16) | (aui8PalTemp[1] << 8) | aui8PalTemp[0];

			*pui32Dest++ = ui32Temp;

			aui8PalTemp[0] = pui8Palette[ui8Invalue1*3];
			aui8PalTemp[1] = pui8Palette[ui8Invalue1*3+1];
			aui8PalTemp[2] = pui8Palette[ui8Invalue1*3+2];

			ui32Temp = 0xFF000000 | (aui8PalTemp[2] << 16) | (aui8PalTemp[1] << 8) | aui8PalTemp[0];

			*pui32Dest++ = ui32Temp;

			i -= 2;
		}
		while(i);
	}
}


/***********************************************************************************
 Function Name      : Copy888Palette8Span
 Inputs             : pui8Src, ui32Width, pvPalette
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies texture data from 8bit Palette with 888 format
					  to RGBA8888 HW format
************************************************************************************/
static IMG_VOID Copy888Palette8Span(IMG_UINT32 *pui32Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Width, 
									const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32Temp;
	IMG_UINT8 ui8Invalue;
	const IMG_UINT8 *pui8Palette = (const IMG_UINT8 *)pvPalette;
	IMG_UINT8 aui8PalTemp[3];

	i = ui32Width;

	do
	{
		ui8Invalue = *pui8Src++;

		aui8PalTemp[0] = pui8Palette[ui8Invalue*3];
		aui8PalTemp[1] = pui8Palette[ui8Invalue*3+1];
		aui8PalTemp[2] = pui8Palette[ui8Invalue*3+2];

		ui32Temp = 0xFF000000 | (aui8PalTemp[2] << 16) | (aui8PalTemp[1] << 8) | aui8PalTemp[0];

		*pui32Dest++ = ui32Temp;
	}
	while(--i);
}


/***********************************************************************************
 Function Name      : Copy8888Palette4Span
 Inputs             : pui8Src, ui32Width, pvPalette
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies texture data from 4bit Palette with 8888 format
					  to RGBA8888 HW format
************************************************************************************/
static IMG_VOID Copy8888Palette4Span(IMG_UINT32 *pui32Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Width,
									 const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue0, ui8Invalue1;
	const IMG_UINT32 *pui32Palette = (const IMG_UINT32 *)pvPalette;
	IMG_UINT32 ui32PalTemp;

	i = ui32Width;

	do
	{
		ui8Invalue0 = *pui8Src++;
		ui8Invalue1 = ui8Invalue0 & 0xF;
		ui8Invalue0 >>= 4;

		ui32PalTemp = READ_UNALIGNED_32( &pui32Palette[ui8Invalue0] );

		*pui32Dest++ = ui32PalTemp; 

		ui32PalTemp = READ_UNALIGNED_32( &pui32Palette[ui8Invalue1] );

		*pui32Dest++ = ui32PalTemp; 

		i -= 2;
	}
	while(i);

}


/***********************************************************************************
 Function Name      : Copy8888Palette4Level1xN
 Inputs             : pui8Src, ui32Height, pvPalette
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies texture data from 4bit Palette with 8888 format
					  to RGBA8888 HW format, whole level 1xN
************************************************************************************/
static IMG_VOID Copy8888Palette4Level1xN(IMG_UINT32 *pui32Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Height,
									 const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue0, ui8Invalue1;
	const IMG_UINT32 *pui32Palette = (const IMG_UINT32 *)pvPalette;
	IMG_UINT32 ui32PalTemp;

	if(ui32Height == 1)
	{
		ui8Invalue0 = *pui8Src;
		ui8Invalue0 >>= 4;

		ui32PalTemp = READ_UNALIGNED_32( &pui32Palette[ui8Invalue0] );

		*pui32Dest = ui32PalTemp; 
	}
	else
	{
		i = ui32Height;

		do 
		{
			ui8Invalue0 = *pui8Src++;
			ui8Invalue1 = ui8Invalue0 & 0xF;
			ui8Invalue0 >>= 4;

			ui32PalTemp = READ_UNALIGNED_32( &pui32Palette[ui8Invalue0] );

			*pui32Dest++ = ui32PalTemp; 

			ui32PalTemp = READ_UNALIGNED_32( &pui32Palette[ui8Invalue1] );

			*pui32Dest++ = ui32PalTemp; 

			i -= 2;
		}
		while(i);
	}
}


/***********************************************************************************
 Function Name      : Copy8888Palette8Span
 Inputs             : pui8Src, ui32Width, pvPalette
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies texture data from 8bit Palette with 8888 format
					  to RGBA8888 HW format
************************************************************************************/
static IMG_VOID Copy8888Palette8Span(IMG_UINT32 *pui32Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Width,
									 const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue;
	const IMG_UINT32 *pui32Palette = (const IMG_UINT32 *)pvPalette;
	IMG_UINT32 ui32PalTemp;

	i = ui32Width;

	do
	{
		ui8Invalue = *pui8Src++;

		ui32PalTemp = READ_UNALIGNED_32( &pui32Palette[ui8Invalue] );

		*pui32Dest++ = ui32PalTemp; 
	}
	while(--i);
}


/***********************************************************************************
 Function Name      : Copy565Palette4Span
 Inputs             : pui8Src, ui32Width, pvPalette
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from 4bit Palette with 565 format
					  to RGB565 HW format
************************************************************************************/
static IMG_VOID Copy565Palette4Span(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Width,
									const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue0, ui8Invalue1;
	const IMG_UINT16 *pui16Palette = (const IMG_UINT16 *)pvPalette;

	i = ui32Width;

	do
	{
		ui8Invalue0 = *pui8Src++;
		ui8Invalue1 = ui8Invalue0 & 0xF;
		ui8Invalue0 >>= 4;

		*pui16Dest++ = READ_UNALIGNED_16( &pui16Palette[ui8Invalue0] );
		*pui16Dest++ = READ_UNALIGNED_16( &pui16Palette[ui8Invalue1] );

		i -= 2;
	}
	while(i);
}


/***********************************************************************************
 Function Name      : Copy565Palette4Level1xN
 Inputs             : pui8Src, ui32Height, pvPalette
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies texture data from 4bit Palette with 565 format
					  to RGBA565 HW format, whole level 1xN
************************************************************************************/
static IMG_VOID Copy565Palette4Level1xN(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Height,
									const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue0, ui8Invalue1;
	const IMG_UINT16 *pui16Palette = (const IMG_UINT16 *)pvPalette;

	if(ui32Height == 1)
	{
		ui8Invalue0 = *pui8Src;
		ui8Invalue0 >>= 4;

		*pui16Dest = READ_UNALIGNED_16( &pui16Palette[ui8Invalue0] );
	}
	else
	{
		i = ui32Height;

		do
		{
			ui8Invalue0 = *pui8Src++;
			ui8Invalue1 = ui8Invalue0 & 0xF;
			ui8Invalue0 >>= 4;

			*pui16Dest++ = READ_UNALIGNED_16( &pui16Palette[ui8Invalue0] );
			*pui16Dest++ = READ_UNALIGNED_16( &pui16Palette[ui8Invalue1] );

			i -= 2;
		}
		while(i);
	}
}


/***********************************************************************************
 Function Name      : Copy565Palette8Span
 Inputs             : pui8Src, ui32Width, pvPalette
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from 8bit Palette with 565 format
					  to RGB565 HW format
************************************************************************************/
static IMG_VOID Copy565Palette8Span(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Width,
									const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue;
	const IMG_UINT16 *pui16Palette = (const IMG_UINT16 *)pvPalette;

	i = ui32Width;

	do
	{
		ui8Invalue = *pui8Src++;

		*pui16Dest++ = READ_UNALIGNED_16( &pui16Palette[ui8Invalue] );
	}
	while(--i);
}


/***********************************************************************************
 Function Name      : Copy4444Palette4Span
 Inputs             : pui8Src, ui32Width, pvPalette
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from 4bit Palette with 4444 format
					  to RGBA4444 HW format
************************************************************************************/
static IMG_VOID Copy4444Palette4Span(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Width,
									 const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue0, ui8Invalue1;
	const IMG_UINT16 *pu16Palette = (const IMG_UINT16 *)pvPalette;
	IMG_UINT16 ui16PalTemp;

	i = ui32Width;

	do
	{
		ui8Invalue0 = *pui8Src++;
		ui8Invalue1 = ui8Invalue0 & 0xF;
		ui8Invalue0 >>= 4;

		ui16PalTemp = READ_UNALIGNED_16( &pu16Palette[ui8Invalue0] );
		*pui16Dest++ = (ui16PalTemp << 12) | (ui16PalTemp >> 4);

		ui16PalTemp = READ_UNALIGNED_16( &pu16Palette[ui8Invalue1] );
		*pui16Dest++ = (ui16PalTemp << 12) | (ui16PalTemp >> 4);

		i -= 2;
	}
	while(i);
}


/***********************************************************************************
 Function Name      : Copy4444Palette4Level1xN
 Inputs             : pui8Src, ui32Height, pvPalette
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies texture data from 4bit Palette with 4444 format
					  to RGBA4444 HW format, whole level 1xN
************************************************************************************/
static IMG_VOID Copy4444Palette4Level1xN(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Height,
									 const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue0, ui8Invalue1;
	const IMG_UINT16 *pu16Palette = (const IMG_UINT16 *)pvPalette;
	IMG_UINT16 ui16PalTemp;

	if(ui32Height == 1)
	{
		ui8Invalue0 = *pui8Src;

		ui16PalTemp = READ_UNALIGNED_16( &pu16Palette[ui8Invalue0 >> 4]);

		*pui16Dest = (ui16PalTemp << 12) | (ui16PalTemp >> 4);
	}
	else
	{
		i = ui32Height;

		do
		{
			ui8Invalue0 = *pui8Src++;
			ui8Invalue1 = ui8Invalue0 & 0xF;
			ui8Invalue0 >>= 4;

			ui16PalTemp = READ_UNALIGNED_16( &pu16Palette[ui8Invalue0] );
			*pui16Dest++ = (ui16PalTemp << 12) | (ui16PalTemp >> 4);

			ui16PalTemp = READ_UNALIGNED_16( &pu16Palette[ui8Invalue1] );
			*pui16Dest++ = (ui16PalTemp << 12) | (ui16PalTemp >> 4);

			i -= 2;
		}
		while(i);
	}
}


/***********************************************************************************
 Function Name      : Copy4444Palette8Span
 Inputs             : pui8Src, ui32Width, pvPalette
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from 8bit Palette with 4444 format
					  to RGBA4444 HW format
************************************************************************************/
static IMG_VOID Copy4444Palette8Span(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Width,
									 const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue;
	const IMG_UINT16 *pui16Palette = (const IMG_UINT16 *)pvPalette;
	IMG_UINT16 ui16PalTemp;

	i = ui32Width;

	do
	{
		ui8Invalue = *pui8Src++;
		ui16PalTemp = READ_UNALIGNED_16( &pui16Palette[ui8Invalue] );

		*pui16Dest++ = (ui16PalTemp << 12) | (ui16PalTemp >> 4);
	}
	while(--i);
}


/***********************************************************************************
 Function Name      : Copy5551Palette4Span
 Inputs             : pui8Src, ui32Width, pvPalette
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from 4bit Palette with 5551 format
					  to RGBA1555 HW format
************************************************************************************/
static IMG_VOID Copy5551Palette4Span(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Width,
									 const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue0, ui8Invalue1;
	const IMG_UINT16 *pui16Palette = (const IMG_UINT16 *)pvPalette;
	IMG_UINT16 ui16PalTemp;

	i = ui32Width;

	do
	{
		ui8Invalue0 = *pui8Src++;
		ui8Invalue1 = ui8Invalue0 & 0xF;
		ui8Invalue0 >>= 4;

		ui16PalTemp = READ_UNALIGNED_16( &pui16Palette[ui8Invalue0] );
		*pui16Dest++ = (ui16PalTemp << 15) | (ui16PalTemp >> 1);

		ui16PalTemp = READ_UNALIGNED_16( &pui16Palette[ui8Invalue1] );
		*pui16Dest++ = (ui16PalTemp << 15) | (ui16PalTemp >> 1);

		i -= 2;
	}
	while(i);

}


/***********************************************************************************
 Function Name      : Copy5551Palette4Level1xN
 Inputs             : pui8Src, ui32Height, pvPalette
 Outputs            : pui32Dest
 Returns            : -
 Description        : Copies texture data from 4bit Palette with 5551 format
					  to RGBA5551 HW format, whole level 1xN
************************************************************************************/
static IMG_VOID Copy5551Palette4Level1xN(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Height,
									 const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue0, ui8Invalue1;
	const IMG_UINT16 *pui16Palette = (const IMG_UINT16 *)pvPalette;
	IMG_UINT16 ui16PalTemp;

	if(ui32Height == 1)
	{
		ui8Invalue0 = *pui8Src;

		ui16PalTemp = READ_UNALIGNED_16( &pui16Palette[ui8Invalue0 >> 4] );

		*pui16Dest = (ui16PalTemp << 15) | (ui16PalTemp >> 1);
	}
	else
	{
		i = ui32Height;

		do
		{
			ui8Invalue0 = *pui8Src++;
			ui8Invalue1 = ui8Invalue0 & 0xF;
			ui8Invalue0 >>= 4;

			ui16PalTemp = READ_UNALIGNED_16( &pui16Palette[ui8Invalue0] );
			*pui16Dest++ = (ui16PalTemp << 15) | (ui16PalTemp >> 1);

			ui16PalTemp = READ_UNALIGNED_16( &pui16Palette[ui8Invalue1] );
			*pui16Dest++ = (ui16PalTemp << 15) | (ui16PalTemp >> 1);

			i -= 2;
		}
		while(i);
	}
}


/***********************************************************************************
 Function Name      : Copy5551Palette8Span
 Inputs             : pui8Src, ui32Width, pvPalette
 Outputs            : pui16Dest
 Returns            : -
 Description        : Copies texture data from 8bit Palette with 5551 format
					  to RGBA1555 HW format
************************************************************************************/
static IMG_VOID Copy5551Palette8Span(IMG_UINT16 *pui16Dest, IMG_UINT8 *pui8Src, IMG_UINT32 ui32Width,
									 const IMG_VOID *pvPalette)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Invalue;
	const IMG_UINT16 *pui16Palette = (const IMG_UINT16 *)pvPalette;
	IMG_UINT16 ui16PalTemp;

	i = ui32Width;

	do
	{
		ui8Invalue = *pui8Src++;
		ui16PalTemp = READ_UNALIGNED_16( &pui16Palette[ui8Invalue] );

		*pui16Dest++ = (ui16PalTemp << 15) | (ui16PalTemp >> 1);
	}
	while(--i);
}


#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
/***********************************************************************************
 Function Name      : SetTextureFormat
 Inputs             : psTex
 Outputs            : psBufferDevice-
 Returns            : None
 Description        : Converts a pvr pixel format to the texture format
************************************************************************************/
IMG_INTERNAL IMG_VOID  SetTextureFormat(GLESTexture *psTex, GLES1StreamDevice *psBufferDevice)
{
	switch(psBufferDevice->sBufferInfo.pixelformat)
	{
		case PVRSRV_PIXEL_FORMAT_RGB565:
		default:
		{
			psTex->psFormat = &TexFormatRGB565;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			psTex->psFormat = &TexFormatARGB4444;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		{
			psTex->psFormat = &TexFormatABGR8888;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_UYVY:
		{
			psTex->psFormat = &TexFormatUYVY;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_VYUY:
		{
			psTex->psFormat = &TexFormatVYUY;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YUYV:
		{
			psTex->psFormat = &TexFormatYUYV;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_FOURCC_ORG_YVYU:
		{
			psTex->psFormat = &TexFormatYVYU;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_NV12:
		{
			psTex->psFormat = &TexFormat2PlaneYUV420;

			break;
		}
		case PVRSRV_PIXEL_FORMAT_YV12:
		{
			psTex->psFormat = &TexFormat3PlaneYUV420;

			break;
		}
	}
}
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) */

/***********************************************************************************
 Function Name      : glActiveTexture
 Inputs             : texture
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets active texture unit.
					  We need to store this for the state machine, although HW makes 
					  no use of it.
************************************************************************************/
GL_API void GL_APIENTRY glActiveTexture(GLenum texture)
{
	IMG_UINT32 ui32Unit = texture - GL_TEXTURE0;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glActiveTexture"));

	GLES1_TIME_START(GLES1_TIMES_glActiveTexture);

	if (ui32Unit >= GLES1_MAX_TEXTURE_UNITS)
	{
		SetError(gc, GL_INVALID_ENUM);
		return;
	}

	gc->sState.sTexture.ui32ActiveTexture = ui32Unit;
	gc->sState.sTexture.psActive = &gc->sState.sTexture.asUnit[ui32Unit];

	GLES1_TIME_STOP(GLES1_TIMES_glActiveTexture);
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
IMG_INTERNAL IMG_BOOL BindTexture(GLES1Context *gc, IMG_UINT32 ui32Unit, IMG_UINT32 ui32Target, IMG_UINT32 ui32Texture)
{
	GLESTexture *psTex;
	GLESTexture *psBoundTexture;
	GLES1NamesArray *psNamesArray;

	GLES1_ASSERT(IMG_NULL != gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ]);

	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ];

	/*
	** Retrieve the texture object from the psNamesArray structure.
	*/
	if (ui32Texture == 0)
	{
		psTex = gc->sTexture.psDefaultTexture[ui32Target];
		GLES1_ASSERT(IMG_NULL != psTex);
		GLES1_ASSERT(0 == psTex->sNamedItem.ui32Name);
	}
	else
	{
		psTex = (GLESTexture *) NamedItemAddRef(psNamesArray, ui32Texture);
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

		GLES1_ASSERT(ui32Texture == psTex->sNamedItem.ui32Name);

		if(!InsertNamedItem(psNamesArray, (GLES1NamedItem*)psTex))
		{
			/* Use names array free */
			psNamesArray->pfnFree(gc, (GLES1NamedItem*)psTex, IMG_TRUE);
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
		GLES1_ASSERT(ui32Texture == psTex->sNamedItem.ui32Name);

		/* Must be same type as target it is being bound to */
		if(ui32Target != psTex->ui32TextureTarget)
		{
			SetError(gc, GL_INVALID_OPERATION);
			NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psTex);
			return IMG_FALSE;
		}
	}

	/*
	** Release texture that is being unbound.
	*/
	psBoundTexture = gc->sTexture.apsBoundTexture[ui32Unit][ui32Target];

	if (psBoundTexture && (psBoundTexture->sNamedItem.ui32Name != 0)) 
	{
#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psBoundTexture->psEGLImageTarget)
		{
			gc->ui32NumEGLImageTextureTargetsBound--;
		}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

		NamedItemDelRef(gc, psNamesArray, (GLES1NamedItem*)psBoundTexture);
	}

	/*
	** Install the new texture into the correct target in this context.
	*/
	gc->sState.sTexture.asUnit[ui32Unit].psTexture[ui32Target] = &psTex->sState;
	gc->sTexture.apsBoundTexture[ui32Unit][ui32Target] = psTex;

	if(psBoundTexture != psTex)
	{
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		/* Ensure that newly bound texture will be validated (cheaply) for texture address */
		if(ui32Target == GLES1_TEXTURE_TARGET_STREAM)
		{
			TextureRemoveResident(gc, psTex);
		}
#endif
#if defined(GLES1_EXTENSION_EGL_IMAGE)
		if(psTex->psEGLImageTarget)
		{
			gc->ui32NumEGLImageTextureTargetsBound++;
		}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE;
	}

	return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : glBindTexture
 Inputs             : target, texture
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Sets current texture for subsequent calls.
************************************************************************************/
GL_API void GL_APIENTRY glBindTexture(GLenum target, GLuint texture)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glBindTexture"));

	GLES1_TIME_START(GLES1_TIMES_glBindTexture);

	switch(target)
	{
		case GL_TEXTURE_2D:
		{
			if(BindTexture(gc, gc->sState.sTexture.ui32ActiveTexture, GLES1_TEXTURE_TARGET_2D, texture) != IMG_TRUE)
			{
				GLES1_TIME_STOP(GLES1_TIMES_glBindTexture);

				return;
			}

			break;
		}
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_OES:
		{
			if(BindTexture(gc, gc->sState.sTexture.ui32ActiveTexture, GLES1_TEXTURE_TARGET_CEM, texture) != IMG_TRUE)
			{
				GLES1_TIME_STOP(GLES1_TIMES_glBindTexture);

				return;
			}

			break;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_STREAM_IMG:
#endif
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_EXTERNAL_OES:
#endif
#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		{
			if(BindTexture(gc, gc->sState.sTexture.ui32ActiveTexture, GLES1_TEXTURE_TARGET_STREAM, texture) != IMG_TRUE)
			{
				GLES1_TIME_STOP(GLES1_TIMES_glBindTexture);

				return;
			}

			break;
		}
#endif /* defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)*/
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glBindTexture);
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
GL_API void GL_APIENTRY glDeleteTextures(GLsizei n, const GLuint *textures)
{
	GLES1NamesArray *psNamesArray;
	IMG_INT32 i;
	IMG_UINT32 j,k;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glDeleteTextures"));

	GLES1_TIME_START(GLES1_TIMES_glDeleteTextures);

	if(!textures)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteTextures);

		return;
	}

	if(n < 0)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glDeleteTextures);

		return;

	}
	else if (n == 0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glDeleteTextures);

		return;
	}
	
	psNamesArray = gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ];

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
		for (j=0; j < GLES1_MAX_TEXTURE_UNITS; j++) 
		{
			for(k=0; k < GLES1_TEXTURE_TARGET_MAX; k++)
			{
				GLESTexture *psTex = gc->sTexture.apsBoundTexture[j][k];

				/* Is the texture currently bound? */
				if (psTex->sNamedItem.ui32Name == textures[i]) 
				{
					/* bind the default texture to this target */
					if(BindTexture(gc, j, k, 0) != IMG_TRUE)
					{
						SetError(gc, GL_OUT_OF_MEMORY);

						GLES1_TIME_STOP(GLES1_TIMES_glDeleteTextures);

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

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE;

	GLES1_TIME_STOP(GLES1_TIMES_glDeleteTextures);
}


/***********************************************************************************
 Function Name      : glGenTextures
 Inputs             : n
 Outputs            : textures
 Returns            : -
 Description        : ENTRYPOINT: Generates a set of n names for textures
************************************************************************************/
GL_API void GL_APIENTRY glGenTextures(GLsizei n, GLuint *textures)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glGenTextures"));

	GLES1_TIME_START(GLES1_TIMES_glGenTextures);

	if (n < 0)
	{
		SetError(gc, GL_INVALID_VALUE);
		GLES1_TIME_STOP(GLES1_TIMES_glGenTextures);
		return;
	}
	else if (n == 0)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glGenTextures);
		return;
	}
	
	if (textures == IMG_NULL) 
	{
		GLES1_TIME_STOP(GLES1_TIMES_glGenTextures);
		return;
	}

	GLES1_ASSERT(gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ] != NULL);

	NamesArrayGenNames(gc->psSharedState->apsNamesArray[GLES1_NAMETYPE_TEXOBJ], (IMG_UINT32)n, (IMG_UINT32 *)textures);

	GLES1_TIME_STOP(GLES1_TIMES_glGenTextures);
}


/***********************************************************************************
 Function Name      : IsLegalRangeTex
 Inputs             : gc, ui32Taret, w, h, border, level, bIsPalette
 Outputs            : -
 Returns            : Whether the parameters are legal
 Description        : UTILITY: Checks some texture parameters for legality.
************************************************************************************/
static IMG_BOOL IsLegalRangeTex(GLES1Context *gc, IMG_UINT32 ui32Target, GLsizei w, GLsizei h, GLint border, GLint level, IMG_BOOL bIsPalette)
{
	IMG_UINT32 ui32Width = (IMG_UINT32)w;
	IMG_UINT32 ui32Height = (IMG_UINT32)h;


	/* The texture spec says to return bad value 
	 * if a border is specified.
	 */
	if(border)
	{
bad_value:
		SetError(gc, GL_INVALID_VALUE);
		return IMG_FALSE;
	}

	if ((w < 0) || ((ui32Width & (ui32Width - 1)) != 0))
	{
		goto bad_value;
	}

	if ((h < 0) || ((ui32Height & (ui32Height - 1))) != 0)
	{
		goto bad_value;
	}

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
	/* For cubemaps, the levels must be square */
	if((ui32Target == GLES1_TEXTURE_TARGET_CEM) && (w != h))
	{
		goto bad_value;
	}
#endif

	if(bIsPalette)
	{
		if ((level > 0) || (GLES1_ABS(level) >= GLES1_MAX_TEXTURE_MIPMAP_LEVELS)) 
		{
			goto bad_value;
		}
	}
	else
	{
		if ((level < 0) || (level >= GLES1_MAX_TEXTURE_MIPMAP_LEVELS)) 
		{
			goto bad_value;
		}
	}

	return IMG_TRUE;
}



/***********************************************************************************
 Function Name      : CheckTexImageArgs
 Inputs             : gc, target, level, bIsPalette, width, height, border  
 Outputs            : -
 Returns            : texture pointer
 Description        : UTILITY: Checks [copy]teximage params for validity.
************************************************************************************/
static GLESTexture *CheckTexImageArgs(GLES1Context *gc, GLenum target, GLint level, IMG_BOOL bIsPalette,
									  GLsizei width, GLsizei height, GLint border,
									  IMG_UINT32 *pui32Face, IMG_UINT32 *pui32Level)
{
	GLESTexture *psTex;
	IMG_UINT32 ui32Target, ui32Face, ui32Level = (IMG_UINT32)level;

	ui32Face = 0;

	switch(target)
	{
		case GL_TEXTURE_2D:
		{
			ui32Target = GLES1_TEXTURE_TARGET_2D;
			break;
		}

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X_OES:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y_OES:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_OES:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z_OES:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_OES:
		{
			ui32Target = GLES1_TEXTURE_TARGET_CEM;
			ui32Face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + GLES1_TEXTURE_CEM_FACE_POSX;
			ui32Level = (ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS) + (IMG_UINT32)level;
			break;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
		default:
		{
bad_enum:
			SetError(gc, GL_INVALID_ENUM);
			return IMG_NULL;
		}
	}
	
	if(!IsLegalRangeTex(gc, ui32Target, width, height, border, level, bIsPalette))
	{
		return IMG_NULL;
	}

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
static GLESTexture *CheckTexSubImageArgs(GLES1Context *gc, GLenum target, GLint lod, GLint x, GLint y,
										  GLsizei width, GLsizei height, const GLESTextureFormat *psSubTexFormat,
										  IMG_UINT32 *pui32Face, IMG_UINT32 *pui32Level)
{
	GLESTexture *psTex;
	IMG_UINT32 ui32Target, ui32Face, ui32Level = (IMG_UINT32)lod;
	IMG_INT32 i32MipWidth, i32MipHeight;
	const GLESTextureFormat *psTargetTexFormat;

	ui32Face = 0;

	/* Negative values are invalid */
	if ((x < 0) || (y < 0) || (width < 0) || (height < 0))
	{
bad_value:
		SetError(gc, GL_INVALID_VALUE);
		return IMG_NULL;
	}

	/* The mipmap level must be in range */
	if ((lod < 0) || (lod >= GLES1_MAX_TEXTURE_MIPMAP_LEVELS))
	{
		goto bad_value;
	}

	switch(target)
	{
		case GL_TEXTURE_2D:
		{
			ui32Target = GLES1_TEXTURE_TARGET_2D;
			break;
		}
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_X_OES:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Y_OES:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_OES:
		case GL_TEXTURE_CUBE_MAP_POSITIVE_Z_OES:
		case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_OES:
		{
			ui32Target = GLES1_TEXTURE_TARGET_CEM;
			ui32Face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + GLES1_TEXTURE_CEM_FACE_POSX;
			ui32Level = (ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS) + (IMG_UINT32)lod;
			break;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			return IMG_NULL;
		}
	}

	psTex = gc->sTexture.apsBoundTexture[gc->sState.sTexture.ui32ActiveTexture][ui32Target];

	GLES1_ASSERT(psTex != NULL);

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

	GLES1_ASSERT(i32MipWidth  >= 0);
	GLES1_ASSERT(i32MipHeight >= 0);


#if defined(GLES1_EXTENSION_PVRTC)
	/* PVRTC */
	if(psTargetTexFormat->ui32TotalBytesPerTexel > 4)
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
#endif /* defined(GLES1_EXTENSION_PVRTC) */
	{
		/* The start coordinates must be within the mipmap */
		if(x > i32MipWidth || y > i32MipHeight)
		{
			goto bad_value;
		}

		/* The region must not be too large */
		if((IMG_UINT32)width > GLES1_MAX_TEXTURE_SIZE || (IMG_UINT32)height > GLES1_MAX_TEXTURE_SIZE)
		{
			goto bad_value;
		}

		/* At this point, we know that these are true */
		GLES1_ASSERT(x <= (IMG_INT32)GLES1_MAX_TEXTURE_SIZE);
		GLES1_ASSERT(x + width <= (IMG_INT32)(2*GLES1_MAX_TEXTURE_SIZE));
		GLES1_ASSERT(x + width >= 0); /* <-- No wrapping can take place */
		GLES1_ASSERT(y <= (IMG_INT32)GLES1_MAX_TEXTURE_SIZE);
		GLES1_ASSERT(y + height <= (IMG_INT32)(2*GLES1_MAX_TEXTURE_SIZE));
		GLES1_ASSERT(y + height >= 0); /* <-- No wrapping can take place */

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


#if defined(GLES1_EXTENSION_EGL_IMAGE)
/***********************************************************************************
 Function Name      : UpdateEGLImage
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID UpdateEGLImage(GLES1Context *gc, GLESTexture *psTex, IMG_UINT32 ui32Level)
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
		GLESTextureParamState *psParams;
		EGLImage *psEGLImage;

		psEGLImage = psTex->psEGLImageSource;

		GLES1_ASSERT(psEGLImage->ui32Level == ui32Level);

		psParams = &psTex->sState;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
		ui32TopUsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
		ui32TopVsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
		ui32TopUsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
		ui32TopVsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

		ui32BytesPerTexel = psTex->psFormat->ui32TotalBytesPerTexel;

		ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(psEGLImage->ui32Level, ui32TopUsize, ui32TopVsize);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)

		if((psEGLImage->ui32Target>=EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR) && (psEGLImage->ui32Target<=EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR))
		{
			IMG_UINT32 ui32FaceOffset, ui32Face;
			
			if(psTex->ui32TextureTarget!=GLES1_TEXTURE_TARGET_CEM)
			{
				PVR_DPF((PVR_DBG_ERROR,"GLESGetImageSource: CEM source requested from non-CEM texture"));

				return;
			}

			ui32Face = psEGLImage->ui32Target - EGL_GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR;

			ui32FaceOffset = ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);
			
			if(psTex->ui32HWFlags & GLES1_MIPMAP)
			{
				if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
					(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}

			ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
		}

#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

		psEGLImage->pvLinSurfaceAddress	 = (IMG_VOID*) (((IMG_UINTPTR_T)psTex->psMemInfo->pvLinAddr) + ui32OffsetInBytes);
		psEGLImage->ui32HWSurfaceAddress = psTex->psMemInfo->sDevVAddr.uiAddr + ui32OffsetInBytes;
		psEGLImage->psMemInfo			 = psTex->psMemInfo;
	}
}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */


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
GL_API void GL_APIENTRY glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, 
						 GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
	GLESTexture             *psTex;
	IMG_UINT8               *pui8Dest;
	IMG_UINT32               ui32Level, ui32Face = 0;
	PFNCopyTextureData		 pfnCopyTextureData;
	IMG_UINT32               ui32SrcBytesPerPixel;
	const GLESTextureFormat *psTexFormat;
	GLESMipMapLevel         *psMipLevel;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexImage2D"));

	GLES1_TIME_START(GLES1_TIMES_glTexImage2D);

	psTex = CheckTexImageArgs(gc, target, level, IMG_FALSE, width, height, border, &ui32Face, &ui32Level);


	if(!psTex)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glTexImage2D);
		return;
	}

#if !defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
	if((GLenum)internalformat != format)
	{
bad_op:
		SetError(gc, GL_INVALID_OPERATION);

		GLES1_TIME_STOP(GLES1_TIMES_glTexImage2D);

		return;
	}
#endif /* !defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

	/* Check the type value for valid enums  */
	switch(type)
	{
		case GL_UNSIGNED_BYTE:
		case GL_UNSIGNED_SHORT_5_6_5:
		case GL_UNSIGNED_SHORT_4_4_4_4:
		case GL_UNSIGNED_SHORT_5_5_5_1:
		{
			break;
		}
		default:
		{
bad_enum:
			SetError(gc, GL_INVALID_ENUM);

			GLES1_TIME_STOP(GLES1_TIMES_glTexImage2D);

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
#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGBA:
						case GL_RGBA8_OES:
						case GL_RGB5_A1_OES:
						case GL_RGBA4_OES:
						{
							break;
						}
						default:
						{
bad_op:
							SetError(gc, GL_INVALID_OPERATION);

							GLES1_TIME_STOP(GLES1_TIMES_glTexImage2D);

							return;
						}
					}
#endif /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture32Bits;

					ui32SrcBytesPerPixel = 4;

					psTexFormat = &TexFormatABGR8888;

					break;
				}
				case GL_UNSIGNED_SHORT_5_5_5_1:
				{
#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGBA:
						case GL_RGB5_A1_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture5551;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatARGB1555;

					break;
				}
				case GL_UNSIGNED_SHORT_4_4_4_4:
				{
#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGBA:
						case GL_RGBA4_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture4444;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatARGB4444;

					break;
				}
				default:
				{
					goto bad_op;
				}
			}
			break;
		}
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
		case GL_BGRA_EXT:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
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
#endif /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */
					
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
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
		case GL_RGB:
		{
			switch(type)
			{
				case GL_UNSIGNED_BYTE:
				{
#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGB:
						case GL_RGB8_OES:
						case GL_RGB565_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture888X;

					ui32SrcBytesPerPixel = 3;

					psTexFormat = &TexFormatXBGR8888;

					break;
				}
				case GL_UNSIGNED_SHORT_5_6_5:
				{
#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
					switch(internalformat)
					{
						case GL_RGB:
						case GL_RGB565_OES:
						{
							break;
						}
						default:
						{
							goto bad_op;
						}
					}
#endif /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatRGB565;

					break;
				}
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
#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
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
#endif /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture8Bits;

					ui32SrcBytesPerPixel = 1;

					psTexFormat = &TexFormatAlpha;

					break;
				}
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
#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
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
#endif /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture8Bits;

					ui32SrcBytesPerPixel = 1;

					psTexFormat = &TexFormatLuminance;

					break;
				}
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
#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)
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
#endif /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

					pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;

					ui32SrcBytesPerPixel = 2;

					psTexFormat = &TexFormatLuminanceAlpha;

					break;
				}
				default:
				{
					goto bad_op;
				}
			}

			break;
		}
		default:
		{
			goto bad_enum;
		}
	}

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	/* if a color buffer of a pbuffer is bound to the texture, we will perform an implicit release */
	if(psTex->hPBuffer)
	{
		ReleasePbufferFromTexture(gc, psTex);
	}
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

#if defined(GLES1_EXTENSION_EGL_IMAGE)
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
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */


	psMipLevel = &psTex->psMipLevel[ui32Level];

	/* If the mipmap is attached to any framebuffer, notify it of the change */
	FBOAttachableHasBeenModified(gc, (GLES1FrameBufferAttachable*)psMipLevel);

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
	
	if(level == 0 && psTex->sState.bGenerateMipmap)
	{
		MakeTextureMipmapLevels(gc, psTex, ui32Face);
	}

	/* 
	 * remove texture from active list.  If needed, validation will reload 
	 */
	TextureRemoveResident(gc, psTex);


	GLES1_INC_PIXEL_COUNT(GLES1_TIMES_glTexImage2D, width*height);

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

	GLES1_TIME_STOP(GLES1_TIMES_glTexImage2D);
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
GL_API void GL_APIENTRY glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
							GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
{
	GLESTexture *psTex;
	IMG_UINT8 *pui8Dest;
	IMG_UINT32 ui32Lod=(IMG_UINT32)level, ui32Face, ui32Level;
	PFNCopyTextureData pfnCopyTextureData;
	IMG_UINT32 ui32SrcBytesPerPixel, ui32DstStride;
	const GLESTextureFormat *psSubTexFormat, *psTargetTexFormat;
	GLESMipMapLevel *psMipLevel;
	
	IMG_BOOL bHWSubTextureUploaded = IMG_FALSE;


	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexSubImage2D"));

	GLES1_TIME_START(GLES1_TIMES_glTexSubImage2D);

	switch(format)
	{
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
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
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
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

				default:
				{
bad_op:
					SetError(gc, GL_INVALID_OPERATION);
					GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);
					return;
				}
			}

			break;
		}

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

				default:
				{
					goto bad_op;
				}
			}

			break;
		}

		case GL_ALPHA:
		{
			if(type != GL_UNSIGNED_BYTE)
			{
				goto bad_op;
			}

			pfnCopyTextureData = (PFNCopyTextureData) CopyTexture8Bits;
			ui32SrcBytesPerPixel = 1;
			psSubTexFormat = &TexFormatAlpha;
			break;
		}

		case GL_LUMINANCE:
		{
			if(type != GL_UNSIGNED_BYTE)
			{
				goto bad_op;
			}

			pfnCopyTextureData = (PFNCopyTextureData) CopyTexture8Bits;
			ui32SrcBytesPerPixel = 1;
			psSubTexFormat = &TexFormatLuminance;
			break;
		}

		case GL_LUMINANCE_ALPHA:
		{
			if(type != GL_UNSIGNED_BYTE)
			{
				goto bad_op;
			}

			pfnCopyTextureData = (PFNCopyTextureData) CopyTexture16Bits;
			ui32SrcBytesPerPixel = 2;
			psSubTexFormat = &TexFormatLuminanceAlpha;
			break;
		}

		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);
			return;
		}
	}

	psTex = CheckTexSubImageArgs(gc, target, level, xoffset, yoffset, width, height, 
								 psSubTexFormat, &ui32Face, &ui32Level);

	if(!psTex)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);
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
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
					case PVRSRV_PIXEL_FORMAT_ARGB8888:
					{
						/* This is actually RGBA to BGRA, but the function is the same */
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture8888BGRAto8888RGBA;
						break;
					}

#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
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
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
			case PVRSRV_PIXEL_FORMAT_ARGB8888:
			{
				switch(psTargetTexFormat->ePixelFormat)
				{
					case PVRSRV_PIXEL_FORMAT_ABGR8888:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture8888BGRAto8888RGBA;
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

#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */

			case PVRSRV_PIXEL_FORMAT_ARGB4444:
			{
				switch(psTargetTexFormat->ePixelFormat)
				{
#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE) || defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
					case PVRSRV_PIXEL_FORMAT_ARGB8888:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture4444toBGRA8888;
						break;
					}

#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) || defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
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
#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE) || defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
					case PVRSRV_PIXEL_FORMAT_ARGB8888:
					{
						pfnCopyTextureData = (PFNCopyTextureData) CopyTexture5551toBGRA8888;
						break;
					}
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) || defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
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
		GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);
		return;
	}

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageTarget)
	{
		IMG_UINT8 *pui8Src = (IMG_UINT8 *)pixels;	/* PRQA S 0311 */ /* Safe const cast here. */
		IMG_UINT32 ui32Align = gc->sState.sClientPixel.ui32UnpackAlignment;
		IMG_UINT32 ui32SrcRowSize = (IMG_UINT32)width * ui32SrcBytesPerPixel;
		IMG_VOID *pvDest;
		IMG_UINT32 ui32Padding = ui32SrcRowSize % ui32Align;

		if(ui32Padding)
		{
			ui32SrcRowSize += (ui32Align - ui32Padding);
		}

	    GLES1_ASSERT(psTex->psEGLImageTarget->ui32Width==psMipLevel->ui32Width && psTex->psEGLImageTarget->ui32Height==psMipLevel->ui32Height);
		GLES1_ASSERT(psMipLevel->pui8Buffer==GLES1_LOADED_LEVEL)

		if(psTex->psEGLImageTarget->bTwiddled)
		{
		    IMG_UINT32 ui32BufferSize;

			ui32DstStride = psMipLevel->ui32Width * psTargetTexFormat->ui32TotalBytesPerTexel;

		    ui32BufferSize = ui32DstStride * psMipLevel->ui32Height;
			
			PVRSRVLockMutex(gc->psSharedState->hTertiaryLock);

			psMipLevel->pui8Buffer = GLES1MallocHeapUNC(gc, ui32BufferSize);

			if(!psMipLevel->pui8Buffer)
			{
			    SetError(gc, GL_OUT_OF_MEMORY);

				PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

				GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);

				return;
			}

			ReadBackTextureData(gc, psTex, 0, 0, psMipLevel->pui8Buffer);

			pvDest = psMipLevel->pui8Buffer + ((IMG_UINT32)yoffset * ui32DstStride) + ((IMG_UINT32)xoffset * psTargetTexFormat->ui32TotalBytesPerTexel);

			(*pfnCopyTextureData)(pvDest, pui8Src, (IMG_UINT32)width, (IMG_UINT32)height, 
						      ui32SrcRowSize, psMipLevel, IMG_TRUE);
			TranslateLevel(gc, psTex, 0, 0);

			GLES1FreeHeapUNC(gc, psMipLevel->pui8Buffer);

			psMipLevel->pui8Buffer = GLES1_LOADED_LEVEL;

			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);
		}
		else
		{
			ui32DstStride = psTex->psEGLImageTarget->ui32Stride;

			pvDest = (IMG_UINT8 *)psTex->psEGLImageTarget->psMemInfo->pvLinAddr + ((IMG_UINT32)yoffset * ui32DstStride) + ((IMG_UINT32)xoffset * psTargetTexFormat->ui32TotalBytesPerTexel);

			(*pfnCopyTextureData)(pvDest, pui8Src, (IMG_UINT32)width, (IMG_UINT32)height, 
						      ui32SrcRowSize, psMipLevel, IMG_TRUE);
		}

		GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);

		return;
	}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */


	pui8Dest = psMipLevel->pui8Buffer;

	ui32DstStride = psMipLevel->ui32Width * psTargetTexFormat->ui32TotalBytesPerTexel;


#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	if(psTex->hPBuffer)
	{
		EGLDrawableParams sParams;
		const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pixels;
		IMG_UINT32 ui32Align = gc->sState.sClientPixel.ui32UnpackAlignment;
		IMG_UINT32 ui32SrcRowSize = (IMG_UINT32)width * ui32SrcBytesPerPixel;
		IMG_VOID *pvDest;
		IMG_UINT32 ui32Padding = ui32SrcRowSize % ui32Align;

		if (!KEGLGetDrawableParameters(psTex->hPBuffer, &sParams, IMG_TRUE))
		{
			PVR_DPF((PVR_DBG_ERROR,"glTexSubImage2D: Can't get drawable info"));
			GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);
			return;
		}

		if(ui32Padding)
		{
			ui32SrcRowSize += (ui32Align - ui32Padding);
		}

		ui32DstStride = sParams.ui32Stride;
		pui8Dest = (IMG_UINT8 *)sParams.pvLinSurfaceAddress;
		pvDest = pui8Dest + ((IMG_UINT32)yoffset * ui32DstStride) + ((IMG_UINT32)xoffset * psTargetTexFormat->ui32TotalBytesPerTexel);

		/* What about ghosting? */
		(*pfnCopyTextureData)(pvDest, pui8Src, (IMG_UINT32)width, (IMG_UINT32)height, 
					      ui32SrcRowSize, psMipLevel, IMG_TRUE);

		GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);
		return;
	}
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */
	
	

	/* copy subtexture data into the host memory 
	   if the host copy of the whole level texture has not been uploaded */
	if(pui8Dest != GLES1_LOADED_LEVEL)
	{
	    const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pixels;
		IMG_UINT32 ui32Align = gc->sState.sClientPixel.ui32UnpackAlignment;
		IMG_UINT32 ui32SrcRowSize = (IMG_UINT32)width * ui32SrcBytesPerPixel;
		IMG_VOID *pvDest;
		IMG_UINT32 ui32Padding = ui32SrcRowSize % ui32Align;

		if(!pui8Dest)
		{
			SetError(gc, GL_OUT_OF_MEMORY);

			GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);

			return;
		}
		
		pvDest = pui8Dest + ((IMG_UINT32)yoffset * ui32DstStride) + ((IMG_UINT32)xoffset * psTargetTexFormat->ui32TotalBytesPerTexel);

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
			GLESTextureParamState  *psParams = &psTex->sState;

			const GLESTextureFormat *psTexFmt = psTex->psFormat;
			IMG_UINT32 ui32BytesPerTexel = psTexFmt->ui32TotalBytesPerTexel;

			/* if the current texture is being used, then ghost the texture 
			   and create new texture device memory */

			if(psTex->psMemInfo && 
			   (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource)))
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

				psParams->ui32StateWord2 = 
				        (psTex->psMemInfo->sDevVAddr.uiAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
				        EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
			}

			/* HWUpload Step 1: copy the old device memory to the new memory */

			if(((IMG_UINT32)width  == psMipLevel->ui32Width) && 
			   ((IMG_UINT32)height == psMipLevel->ui32Height) &&
			   ((psTex->ui32HWFlags & GLES1_MIPMAP) == 0)
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
			   && (psTex->ui32TextureTarget != GLES1_TEXTURE_TARGET_CEM)
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
			   )
			{
		        FlushAttachableIfNeeded(gc, (GLES1FrameBufferAttachable*)psMipLevel, GLES1_SCHEDULE_HW_DISCARD_SCENE);
			}
			else
			{
		        /* Render to mipmap before reading it back */
		        FlushAttachableIfNeeded(gc, (GLES1FrameBufferAttachable*)psMipLevel,
						GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_WAIT_FOR_3D);

				if (sMemInfo.uAllocSize)  /* transfer the whole texture data of all levels from the old device memory to the new memory */
				{
				    CopyTextureData(gc, psTex, 0, &sMemInfo, 0, sMemInfo.uAllocSize);
				}
				/* else: MemInfo hasn't been ghosted, the original device memory is being used. */
			}

			/* HWUpload Step 2: directly upload subtexture data to the new device memory, using hardware */
			{
				IMG_UINT32 ui32SubTexBufferSize;
				IMG_UINT8 *pui8SubTexBuffer;
				IMG_UINT8 *pui8Src = (IMG_UINT8 *)(IMG_UINTPTR_T)pixels;
				IMG_UINT32 ui32SrcRowSize = (IMG_UINT32)width * ui32SrcBytesPerPixel;
				IMG_UINT32 ui32OffsetInBytes;
				IMG_UINT32 ui32TopUsize, ui32TopVsize;

				SGX_QUEUETRANSFER sQueueTransfer;

				GLESSubTextureInfo sSubTexInfo;		

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
				ui32TopUsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
				ui32TopVsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
				ui32TopUsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
				ui32TopVsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

				ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
				if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
				{
					IMG_UINT32 ui32FaceOffset = 
					ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);

					if(psTex->ui32HWFlags & GLES1_MIPMAP)
					{
						if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
							(ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
						{
							ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
						}
					}

					ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
				}	
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */


				ui32SubTexBufferSize = (IMG_UINT32)width * (IMG_UINT32)height * psTargetTexFormat->ui32TotalBytesPerTexel;
				pui8SubTexBuffer = (IMG_UINT8 *) GLES1MallocHeapUNC(gc, ui32SubTexBufferSize);

				if(!pui8SubTexBuffer)
				{
				    SetError(gc, GL_OUT_OF_MEMORY);
					GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);
					return;
				}
				
				/* Setup sSubTexInfo */
				sSubTexInfo.ui32SubTexXoffset = (IMG_UINT32)xoffset;
				sSubTexInfo.ui32SubTexYoffset = (IMG_UINT32)yoffset;
				sSubTexInfo.ui32SubTexWidth   = (IMG_UINT32)width;
				sSubTexInfo.ui32SubTexHeight  = (IMG_UINT32)height;
				sSubTexInfo.pui8SubTexBuffer  = pui8SubTexBuffer;


#if (defined(DEBUG) || defined(TIMING))
				ui32TextureMemCurrent += ui32SubTexBufferSize;
				
				if (ui32TextureMemCurrent>ui32TextureMemHWM)
				{
			        ui32TextureMemHWM = ui32TextureMemCurrent;
				}
#endif

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
					
				GLES1FreeHeapUNC(gc, pui8SubTexBuffer);
			}
		}


		/* otherwise, use the software subtexture uploading:
		   readback the whole level texture data and copy subtexture data to the host memory. */
        if(!bHWSubTextureUploaded)
		{
			const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pixels;
			IMG_UINT32 ui32Align = gc->sState.sClientPixel.ui32UnpackAlignment;
			IMG_UINT32 ui32SrcRowSize = (IMG_UINT32)width * ui32SrcBytesPerPixel;
			IMG_UINT32 ui32Padding = ui32SrcRowSize % ui32Align;
			IMG_UINT32 ui32BufferSize = ui32DstStride * psMipLevel->ui32Height;
			IMG_VOID *pvDest;

			pui8Dest = GLES1MallocHeapUNC(gc, ui32BufferSize);
	
			if(!pui8Dest)
			{
				SetError(gc, GL_OUT_OF_MEMORY);
				GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);
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
				FlushAttachableIfNeeded(gc, (GLES1FrameBufferAttachable*)psMipLevel, GLES1_SCHEDULE_HW_DISCARD_SCENE);
			}
			else
			{
				/* Render to mipmap before reading it back */
				FlushAttachableIfNeeded(gc, (GLES1FrameBufferAttachable*)psMipLevel,
										GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_WAIT_FOR_3D);

				/* otherwise already flushed */
				ReadBackTextureData(gc, psTex, ui32Face, (IMG_UINT32)level, pui8Dest);
			}

			psMipLevel->pui8Buffer = pui8Dest;
	
			pvDest = pui8Dest + ((IMG_UINT32)yoffset * ui32DstStride) + ((IMG_UINT32)xoffset * psTargetTexFormat->ui32TotalBytesPerTexel);

			if(ui32Padding)
			{
				ui32SrcRowSize += (ui32Align - ui32Padding);
			}

			(*pfnCopyTextureData)(pvDest, pui8Src, (IMG_UINT32)width, (IMG_UINT32)height, 
						ui32SrcRowSize, psMipLevel, IMG_TRUE);

			TextureRemoveResident(gc, psTex);

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
		}
	}


	if(level == 0 && psTex->sState.bGenerateMipmap)
	{
		MakeTextureMipmapLevels(gc, psTex, ui32Face);
	}


#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageSource || psTex->psEGLImageTarget)
	{
		UpdateEGLImage(gc, psTex, ui32Level);
	}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */


	GLES1_INC_PIXEL_COUNT(GLES1_TIMES_glTexSubImage2D, width*height);
	GLES1_TIME_STOP(GLES1_TIMES_glTexSubImage2D);
}



/***********************************************************************************
 Function Name      : glCompressedTexImage2D
 Inputs             : target, level, internalformat, width, height, border, format,
					  imageSize, data
 Outputs            : -
 Returns            : -
 Description        : ENTRYPOINT: Provides texture data in a pre-compressed format.
					  suggests an internal storage format. Host texture data memory
					  is allocated but not loaded to HW until use. Palette data will need
					  decompression first.
************************************************************************************/
GL_API void GL_APIENTRY glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, 
								   GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data)
{
	GLESTexture *psTex;
	IMG_UINT32 ui32Level, ui32Face;	
	IMG_UINT8 *pui8Dest;
	IMG_UINT32 i;
	PFNCopyPaletteSpan pfnCopySpan = IMG_NULL;
	PFNCopyPaletteSpan pfnCopyLevel1xN = IMG_NULL;
	IMG_UINT32 ui32SrcBitsPerPixel;
	const GLESTextureFormat *psTexFormat;
	IMG_UINT32 ui32NumLevels, ui32PaletteSize, ui32Size;
	IMG_UINT32 ui32Width = (IMG_UINT32)width;
	IMG_UINT32 ui32Height = (IMG_UINT32)height;
	const IMG_VOID *pvPixels = (const IMG_VOID *)data;
	GLESMipMapLevel *psMipLevel;
#if defined(GLES1_EXTENSION_PVRTC) || defined(GLES1_EXTENSION_ETC1)
	PFNCopyTextureData pfnCopyTextureData = IMG_NULL;
#endif /* defined(GLES1_EXTENSION_PVRTC) || defined(GLES1_EXTENSION_ETC1) */

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glCompressedTexImage2D"));

	GLES1_TIME_START(GLES1_TIMES_glCompressedTexImage2D);

	ui32PaletteSize = 0;

	switch(internalformat)
	{
		case GL_PALETTE4_RGB8_OES:
		{
			ui32PaletteSize = 3 * (1 << 4);
			ui32SrcBitsPerPixel = 4;
			pfnCopySpan = (PFNCopyPaletteSpan) Copy888Palette4Span;
			pfnCopyLevel1xN = (PFNCopyPaletteSpan) Copy888Palette4Level1xN;
			psTexFormat = &TexFormatXBGR8888;
			break;
		}
		case GL_PALETTE4_RGBA8_OES:
		{
			ui32PaletteSize = 4 * (1 << 4);
			ui32SrcBitsPerPixel = 4;
			pfnCopySpan = (PFNCopyPaletteSpan) Copy8888Palette4Span;
			pfnCopyLevel1xN = (PFNCopyPaletteSpan) Copy8888Palette4Level1xN;
			psTexFormat = &TexFormatABGR8888;
			break;
		}
		case GL_PALETTE4_R5_G6_B5_OES:
		{
			ui32PaletteSize = 2 * (1 << 4);
			ui32SrcBitsPerPixel = 4;
			pfnCopySpan = (PFNCopyPaletteSpan) Copy565Palette4Span;
			pfnCopyLevel1xN = (PFNCopyPaletteSpan) Copy565Palette4Level1xN;
			psTexFormat = &TexFormatRGB565;
			break;
		}
		case GL_PALETTE4_RGBA4_OES:
		{
			ui32PaletteSize = 2 * (1 << 4);
			ui32SrcBitsPerPixel = 4;
			pfnCopySpan = (PFNCopyPaletteSpan) Copy4444Palette4Span;
			pfnCopyLevel1xN = (PFNCopyPaletteSpan) Copy4444Palette4Level1xN;
			psTexFormat = &TexFormatARGB4444;
			break;
		}
		case GL_PALETTE4_RGB5_A1_OES:
		{
			ui32PaletteSize = 2 * (1 << 4);
			ui32SrcBitsPerPixel = 4;
			pfnCopySpan = (PFNCopyPaletteSpan) Copy5551Palette4Span;
			pfnCopyLevel1xN = (PFNCopyPaletteSpan) Copy5551Palette4Level1xN;
			psTexFormat = &TexFormatARGB1555;
			break;
		}
		case GL_PALETTE8_RGB8_OES:
		{
			ui32PaletteSize = 3 * (1 << 8);
			ui32SrcBitsPerPixel = 8;
			pfnCopySpan = (PFNCopyPaletteSpan) Copy888Palette8Span;
			psTexFormat = &TexFormatXBGR8888;
			break;
		}
		case GL_PALETTE8_RGBA8_OES:
		{
			ui32PaletteSize = 4 * (1 << 8);
			ui32SrcBitsPerPixel = 8;
			pfnCopySpan = (PFNCopyPaletteSpan) Copy8888Palette8Span;
			psTexFormat = &TexFormatABGR8888;
			break;
		}
		case GL_PALETTE8_R5_G6_B5_OES:
		{
			ui32PaletteSize = 2 * (1 << 8);
			ui32SrcBitsPerPixel = 8;
			pfnCopySpan = (PFNCopyPaletteSpan) Copy565Palette8Span;
			psTexFormat = &TexFormatRGB565;
			break;
		}
		case GL_PALETTE8_RGBA4_OES:
		{
			ui32PaletteSize = 2 * (1 << 8);
			ui32SrcBitsPerPixel = 8;
			pfnCopySpan = (PFNCopyPaletteSpan) Copy4444Palette8Span;
			psTexFormat = &TexFormatARGB4444;
			break;
		}
		case GL_PALETTE8_RGB5_A1_OES:
		{
			ui32PaletteSize = 2 * (1 << 8);
			ui32SrcBitsPerPixel = 8;
			pfnCopySpan = (PFNCopyPaletteSpan) Copy5551Palette8Span;
			psTexFormat = &TexFormatARGB1555;
			break;
		}
#if defined(GLES1_EXTENSION_PVRTC)
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
		{
			ui32SrcBitsPerPixel = 64;
			pfnCopyTextureData = (PFNCopyTextureData)CopyTexturePVRTC;
			psTexFormat = &TexFormatPVRTC2RGB;
			break;
		}
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
		{
			ui32SrcBitsPerPixel = 64;
			pfnCopyTextureData = (PFNCopyTextureData)CopyTexturePVRTC;
			psTexFormat = &TexFormatPVRTC2RGBA;
			break;
		}
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
		{
			ui32SrcBitsPerPixel = 64;
			pfnCopyTextureData = (PFNCopyTextureData)CopyTexturePVRTC;
			psTexFormat = &TexFormatPVRTC4RGB;
			break;
		}
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
		{
			ui32SrcBitsPerPixel = 64;
			pfnCopyTextureData = (PFNCopyTextureData)CopyTexturePVRTC;
			psTexFormat = &TexFormatPVRTC4RGBA;
			break;
		}
#endif /* defined(GLES1_EXTENSION_PVRTC) */
#if defined(GLES1_EXTENSION_ETC1)
		case GL_ETC1_RGB8_OES:
		{
			ui32SrcBitsPerPixel = 64;
			pfnCopyTextureData = (PFNCopyTextureData)CopyTextureETC1;
			psTexFormat = &TexFormatETC1RGB8;
			break;
		}
#endif /* defined(GLES1_EXTENSION_ETC1) */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexImage2D);
			return;
		}
	}

	psTex = CheckTexImageArgs(gc, target, level, ui32PaletteSize ? IMG_TRUE : IMG_FALSE, 
							  width, height, border, &ui32Face, &ui32Level);

	if(!psTex)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexImage2D);
		return;
	}

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	/* if a color buffer of a pbuffer is bound to the texture, we will perform an implicit release */
	if(psTex->hPBuffer)
	{
		ReleasePbufferFromTexture(gc, psTex);
	}
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

#if defined(GLES1_EXTENSION_EGL_IMAGE)
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
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

	if(ui32PaletteSize)
	{
		ui32NumLevels = (IMG_UINT32)GLES1_ABS(level) + 1;

		ui32Size = 0;

		if(ui32NumLevels > 1)
		{
			for(i=0; i < GLES1_MAX_TEXTURE_MIPMAP_LEVELS; i++)
			{
				if(ui32Width == 1 && ui32Height == 1)
				{
					break;
				}

				ui32Size += ((ui32Width * ui32Height * ui32SrcBitsPerPixel) >> 3);

				ui32Width >>= 1;
				ui32Height >>= 1;

				if(ui32Width == 0)
				{
					ui32Width = 1;
				}

				if(ui32Height == 0)
				{
					ui32Height = 1;
				}
			}

			/* Cover 1x1 level - add 1 byte whether 4bit or 8bit palette entry */
			ui32Size += 1;

			if(i != (ui32NumLevels - 1))
			{
bad_value:
				SetError(gc, GL_INVALID_VALUE);

				GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexImage2D);

				return;
			}
		}
		else
		{
			ui32Size = MAX(1, (ui32Width * ui32Height * ui32SrcBitsPerPixel) >> 3);
		}
		
		if(imageSize != (GLint)(ui32Size + ui32PaletteSize))
		{
			goto bad_value;
		}
		
		ui32Width = (IMG_UINT32)width;
		ui32Height = (IMG_UINT32)height;

		pvPixels = (const IMG_UINT8 *)pvPixels + ui32PaletteSize;

		/* Allocate memory for the level data */
		for(i=0; i< ui32NumLevels; i++)
		{
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
			if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
			{
				ui32Face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + GLES1_TEXTURE_CEM_FACE_POSX;

				ui32Level = (ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS) + i;
			}
			else
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
			{
				ui32Level = i;
			}

			psMipLevel = &psTex->psMipLevel[ui32Level];

			/* If the mipmap is attached to any framebuffer, notify it of the change */
			FBOAttachableHasBeenModified(gc, (GLES1FrameBufferAttachable*)psMipLevel);

			/* Allocate memory for the level data */
			pui8Dest = TextureCreateLevel(gc, psTex, ui32Level, (IMG_UINT32)internalformat, psTexFormat, (IMG_UINT32)ui32Width, (IMG_UINT32)ui32Height);


			if(data && pui8Dest)
			{
				IMG_VOID *pvDest = psMipLevel->pui8Buffer;
				const IMG_UINT8 *pui8Src = (const IMG_UINT8 *)pvPixels;
				IMG_UINT32 ui32SrcRowSize = (ui32Width * ui32SrcBitsPerPixel) >> 3;
				IMG_UINT32 ui32DstRowSize = ui32Width * psTex->psFormat->ui32TotalBytesPerTexel;
				IMG_UINT32 j;
				
				if ((ui32Height) && (ui32Width))
				{ 
					if((ui32SrcBitsPerPixel == 8 || ui32Width != 1) && (pfnCopySpan != IMG_NULL))
					{
						j = ui32Height;

						do
						{
							(*pfnCopySpan)(pvDest, pui8Src, ui32Width, data);

							pvDest = (IMG_UINT8 *)pvDest + ui32DstRowSize;
							pui8Src += ui32SrcRowSize;
						}
						while(--j);
					}
					else if((ui32SrcBitsPerPixel != 8 && ui32Width == 1) && (pfnCopyLevel1xN != IMG_NULL)) 
					{
						(*pfnCopyLevel1xN)(pvDest, pui8Src, ui32Height, data);
					}
					else
					{
						PVR_DPF((PVR_DBG_ERROR,"glCompressedTexImage2D: Wrong internalformat, palette or width combination"));
						TextureRemoveResident(gc, psTex);
						GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexImage2D);
						return;
					}
				}
			}
			else
			{
				/*
				* remove texture from active list.  If needed, validation will reload 
				*/
				TextureRemoveResident(gc, psTex);
				GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexImage2D);
				return;
			}

			pvPixels = (const IMG_UINT8 *)pvPixels + ((ui32Width * ui32Height * ui32SrcBitsPerPixel) >> 3);

			ui32Width >>= 1;
			ui32Height >>= 1;

			if(ui32Width == 0)
			{
				ui32Width = 1;
			}

			if(ui32Height == 0)
			{
				ui32Height = 1;
			}
		}
	
		/* 
  		 * remove texture from active list.  If needed, validation will reload 
		 */
		TextureRemoveResident(gc, psTex);

	}
#if defined(GLES1_EXTENSION_PVRTC) || defined(GLES1_EXTENSION_ETC1)
	else /* Compressed texture format */
	{

#if defined(GLES1_EXTENSION_ETC1)
		if(pfnCopyTextureData==(PFNCopyTextureData)CopyTextureETC1)
		{
			/* Must have at least 1 block */
			ui32Height = MAX((IMG_UINT32)height >> 2, 1);
			ui32Width  = MAX((IMG_UINT32)width >> 2, 1);
		}
		else
#endif /* defined(GLES1_EXTENSION_ETC1) */
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
			goto bad_value;
		}
			
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		{
			ui32Face = target - GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + GLES1_TEXTURE_CEM_FACE_POSX;

			ui32Level = (ui32Face * GLES1_MAX_TEXTURE_MIPMAP_LEVELS) + (IMG_UINT32)level;
		}
		else
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
		{
			ui32Level = (IMG_UINT32)level;
		}

		psMipLevel = &psTex->psMipLevel[ui32Level];

		/* If the mipmap is attached to any framebuffer, notify it of the change */
		FBOAttachableHasBeenModified(gc, (GLES1FrameBufferAttachable*)psMipLevel);

		/* Allocate memory for the level data */
		pui8Dest = TextureCreateLevel(gc, psTex, ui32Level, (IMG_UINT32)internalformat, psTexFormat, (IMG_UINT32)width, (IMG_UINT32)height);

		if(pvPixels && pui8Dest)
		{
			IMG_VOID *pvDest = psMipLevel->pui8Buffer;

			if ((height) && (width))
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

			GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexImage2D);

			return;
		}

		/*
		 * remove texture from active list.  If needed, validation will reload
		 */
		TextureRemoveResident(gc, psTex);

	}

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

#endif /* defined(GLES1_EXTENSION_PVRTC) || defined(GLES1_EXTENSION_ETC1)*/


	GLES1_INC_PIXEL_COUNT(GLES1_TIMES_glCompressedTexImage2D, width*height);
	GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexImage2D);
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
GL_API void GL_APIENTRY glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, 
									  GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data)
{
	GLESTexture *psTex;
	const GLESTextureFormat *psSubTexFormat;
	const IMG_VOID *pvPixels = (const IMG_VOID *)data;
	IMG_UINT8 *pui8Dest;
	IMG_UINT32 ui32Width;
	IMG_UINT32 ui32Height;
	IMG_UINT32 ui32Size;
	IMG_UINT32 ui32Face;
	IMG_UINT32 ui32Level;
	GLESMipMapLevel *psMipLevel;

	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glCompressedTexSubImage2D"));

	GLES1_TIME_START(GLES1_TIMES_glCompressedTexSubImage2D);

	switch(format)
	{
#if defined(GLES1_EXTENSION_PVRTC)
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
		{
			/* SrcBitsPerPixel = 64 */
			psSubTexFormat = &TexFormatPVRTC2RGB;

			break;
		}
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
		{
			/* SrcBitsPerPixel = 64 */
			psSubTexFormat = &TexFormatPVRTC2RGBA;

			break;
		}
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
		{
			/* SrcBitsPerPixel = 64 */
			psSubTexFormat = &TexFormatPVRTC4RGB;

			break;
		}
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
		{
			/* SrcBitsPerPixel = 64 */
			psSubTexFormat = &TexFormatPVRTC4RGBA;

			break;
		}
#endif /* defined(GLES1_EXTENSION_PVRTC) */
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexSubImage2D);
			return;
		}
	}

	psTex = CheckTexSubImageArgs(gc, target, level, xoffset, yoffset, width, height, 
								 psSubTexFormat, &ui32Face, &ui32Level);

	if(!psTex)
	{
		GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexSubImage2D);
		return;
	}

	psMipLevel = &psTex->psMipLevel[ui32Level];

	/* Must have at least 2x2 blocks */
	ui32Height = MAX((IMG_UINT32)height >> 2, 2);
	
#if defined(GLES1_EXTENSION_PVRTC)
	/* 2Bpp encodes 8x4 blocks, 4Bpp encodes 4x4 */
	if( (psSubTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTC2) || 
		(psSubTexFormat->ePixelFormat == PVRSRV_PIXEL_FORMAT_PVRTCII2))
	{
		ui32Width = MAX((IMG_UINT32)width >> 3, 2);
	}
	else
#endif /* defined(GLES1_EXTENSION_PVRTC) */
	{
		ui32Width = MAX((IMG_UINT32)width >> 2, 2);
	}
	
	ui32Size = ui32Width * ui32Height << 3;

	if(imageSize != (GLint)ui32Size)
	{
		SetError(gc, GL_INVALID_VALUE);

		GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexSubImage2D);

		return;
	}

	pui8Dest = psMipLevel->pui8Buffer;

	if(pui8Dest == GLES1_LOADED_LEVEL)
	{
		/* Only support fullsize subtexture */
		pui8Dest = GLES1MallocHeapUNC(gc, imageSize);
		
		if(!pui8Dest)
		{
			SetError(gc, GL_OUT_OF_MEMORY);
			GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexSubImage2D);
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

		CopyTexturePVRTC(pvDest, pvPixels, (IMG_UINT32)width, (IMG_UINT32)height, 0, psMipLevel, IMG_FALSE);

		GLES1_INC_PIXEL_COUNT(GLES1_TIMES_glCompressedTexSubImage2D, width*height);
	}

	TextureRemoveResident(gc, psTex);

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageSource || psTex->psEGLImageTarget)
	{
		UpdateEGLImage(gc, psTex, ui32Level);
	}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE;

	GLES1_TIME_STOP(GLES1_TIMES_glCompressedTexSubImage2D);
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
GL_API void GL_APIENTRY glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat,
							 GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
	GLESTexture              *psTex;
	IMG_UINT32                ui32Face, ui32Level, i;
	const GLESTextureFormat  *psTexFormat;
	PFNSpanPack               pfnSpanPack;
	GLenum                    type, eTextureHWFormat;
	IMG_UINT8                *pui8Dest;
	GLESPixelSpanInfo         sSpanInfo = {0};
	IMG_VOID                 *pvSurfacePointer;
	EGLDrawableParams        *psReadParams;
	GLESMipMapLevel          *psMipLevel;

	__GLES1_GET_CONTEXT();


	PVR_DPF((PVR_DBG_CALLTRACE,"glCopyTexImage2D"));

	GLES1_TIME_START(GLES1_TIMES_glCopyTexImage2D);

	/* Check arguments and get the right texture being changed */
	psTex = CheckTexImageArgs(gc, target, level, IMG_FALSE, width, height, border, &ui32Face, &ui32Level);

	if (!psTex) 
	{
		GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);

		return;
	}

#if defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT)

	switch(internalformat)
	{
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
		case GL_BGRA_EXT:
		{
			internalformat = GL_BGRA_EXT;

			break;
		}
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
		case GL_RGBA8_OES:
		case GL_RGBA4_OES:
		case GL_RGB5_A1_OES:
		case GL_RGBA:
		{
			internalformat = GL_RGBA;

			break;
		}
		case GL_RGB8_OES:
		case GL_RGB565_OES:
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

			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);

			return;
		}
	}

#else  /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

	switch(internalformat)
	{
		case GL_RGB:
		case GL_RGBA:
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
		case GL_BGRA_EXT:
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
		case GL_ALPHA:
		case GL_LUMINANCE:
		case GL_LUMINANCE_ALPHA:
		{
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_VALUE);

			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);

			return;
		}
	}

#endif  /* defined(GLES1_EXTENSION_REQUIRED_INTERNAL_FORMAT) */

	if(GetFrameBufferCompleteness(gc) != GL_FRAMEBUFFER_COMPLETE_OES)
	{
		SetError(gc, GL_INVALID_FRAMEBUFFER_OPERATION_OES);

		GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);

		return;
	}

	psReadParams = gc->psReadParams;

	/* No read surface specified */
	if(!psReadParams->psRenderSurface)
	{
bad_op:
		SetError(gc, GL_INVALID_OPERATION);

		GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);

		return;
	}


	/* Generally the texture hw format is the same as the internal format.
	 * In the case of RGB888 textures, we don't have a corresponding HW format, so we
	 * use RGBA8888 textures. This must be reflected in the setup of the copyspan
	 */
	eTextureHWFormat = internalformat;

	switch(gc->psReadParams->ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		{
			switch(internalformat)
			{
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
				case GL_BGRA_EXT:
				{
					pfnSpanPack = SpanPack32;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatARGB8888;
					break;
				}
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
				case GL_RGBA:
					pfnSpanPack = SpanPackARGB8888toABGR8888;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatABGR8888;
					break;
				case GL_RGB:
					pfnSpanPack = SpanPackARGB8888toXBGR8888;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatXBGR8888;
					/* Actual HW format is RGBA */
					eTextureHWFormat = GL_RGBA;
					break;
				case GL_LUMINANCE:
					pfnSpanPack = SpanPackARGB8888toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				case GL_ALPHA:
					pfnSpanPack = SpanPackAXXX8888toAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatAlpha;
					break;
				case GL_LUMINANCE_ALPHA:
					pfnSpanPack = SpanPackARGB8888toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminanceAlpha;
					break;
				default:
					/* we should never reach this case */
					goto bad_op;

			}
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		{
			switch(internalformat)
			{
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
				case GL_BGRA_EXT:
				{
					pfnSpanPack = SpanPackABGR8888toARGB8888;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatARGB8888;

					break;
				}
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
				case GL_RGBA:
					pfnSpanPack = SpanPack32;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatABGR8888;
					break;
				case GL_RGB:
					pfnSpanPack = SpanPackABGR8888toXBGR8888;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatXBGR8888;
					/* Actual HW format is RGBA */
					eTextureHWFormat = GL_RGBA;
					break;
				case GL_LUMINANCE:
					pfnSpanPack = SpanPackABGR8888toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				case GL_ALPHA:
					pfnSpanPack = SpanPackAXXX8888toAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatAlpha;
					break;
				case GL_LUMINANCE_ALPHA:
					pfnSpanPack = SpanPackABGR8888toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminanceAlpha;
					break;
				default:
					/* we should never reach this case */
					goto bad_op;
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
					pfnSpanPack = SpanPack16;
					type = GL_UNSIGNED_SHORT_4_4_4_4;
					psTexFormat = &TexFormatARGB4444;
					break;
				case GL_RGB:
					pfnSpanPack = SpanPackARGB4444toRGB565;
					type = GL_UNSIGNED_SHORT_5_6_5;
					psTexFormat = &TexFormatRGB565;
					break;
				case GL_LUMINANCE:
					pfnSpanPack = SpanPackARGB4444toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				case GL_ALPHA:
					pfnSpanPack = SpanPackARGB4444toAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatAlpha;
					break;
				case GL_LUMINANCE_ALPHA:
					pfnSpanPack = SpanPackARGB4444toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminanceAlpha;
					break;
				default:
					/* we should never reach this case */
					goto bad_op;
			}

			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			switch(internalformat)
			{
				case GL_RGBA:
					pfnSpanPack = SpanPack16;
					type = GL_UNSIGNED_SHORT_5_5_5_1;
					psTexFormat = &TexFormatARGB1555;
					break;
				case GL_RGB:
					pfnSpanPack = SpanPackARGB1555toRGB565;
					type = GL_UNSIGNED_SHORT_5_6_5;
					psTexFormat = &TexFormatRGB565;
					break;
				case GL_LUMINANCE:
					pfnSpanPack = SpanPackARGB1555toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				case GL_ALPHA:
					pfnSpanPack = SpanPackARGB1555toAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatAlpha;
					break;
				case GL_LUMINANCE_ALPHA:
					pfnSpanPack = SpanPackARGB1555toLuminanceAlpha;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminanceAlpha;
					break;
				default:
					/* we should never reach this case */
					goto bad_op;

			}
			break;
		}
		case PVRSRV_PIXEL_FORMAT_RGB565:
		default:
		{
			switch(internalformat)
			{
				case GL_RGB:
					pfnSpanPack = SpanPack16;
					type = GL_UNSIGNED_SHORT_5_6_5;
					psTexFormat = &TexFormatRGB565;
					break;
				case GL_LUMINANCE:
					pfnSpanPack = SpanPackRGB565toLuminance;
					type = GL_UNSIGNED_BYTE;
					psTexFormat = &TexFormatLuminance;
					break;
				default:
					/* we should never reach this case */
					goto bad_op;
			}

			break;
		}
	}

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	/* if a color buffer of a pbuffer is bound to the texture, we will perform an implicit release */
	if(psTex->hPBuffer)
	{
		ReleasePbufferFromTexture(gc, psTex);
	}
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */

#if defined(GLES1_EXTENSION_EGL_IMAGE)
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
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */


	psMipLevel = &psTex->psMipLevel[ui32Level];



	/*********************************************************************************
	 *
	 * There are two options to upload supplied data in glCopyTexImage2D
	 *   
	 *  1. If psTex's device memory has already been allocated and uploaded before, 
	 *     && psMipLevel specified by glCopyTexImage2D has exactly same size, 
	 *        levelNo and pixel format as the allocated psTex's certain level,
	 *     && do not need to GenerateMipmap.
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
		 (psMipLevel->pui8Buffer == GLES1_LOADED_LEVEL) &&
		 (width != 0) && (height != 0)                  &&
		 (psMipLevel->ui32Width  == (IMG_UINT32)width)  &&
		 (psMipLevel->ui32Height == (IMG_UINT32)height) &&
		 ((psMipLevel->psTexFormat) == psTexFormat)     &&
		 !(level == 0 && psTex->sState.bGenerateMipmap) )
	{
		
		SGX_QUEUETRANSFER sQueueTransfer;
		IMG_UINT32 ui32TopUsize, ui32TopVsize;
		IMG_UINT32 ui32OffsetInBytes;
		IMG_UINT32 ui32Lod=(IMG_UINT32)level;

	    GLESSubTextureInfo sSrcReadInfo;

		GLESTextureParamState *psParams = &psTex->sState;
		IMG_UINT32 ui32BytesPerTexel = psTex->psFormat->ui32TotalBytesPerTexel;


		/* Setup ui32OffsetInBytes */
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
		ui32TopUsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
		ui32TopVsize = 1U << ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
		ui32TopUsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
		ui32TopVsize = 1 + ((psParams->ui32StateWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

		ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		{
			IMG_UINT32 ui32FaceOffset = 
			  ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);
			
			if(psTex->ui32HWFlags & GLES1_MIPMAP)
			{
				if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
				   (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}
			
			ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
		}	
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */


		/* Setup sSrcReadInfo */
		sSrcReadInfo.ui32SubTexXoffset = (IMG_UINT32)x;
		sSrcReadInfo.ui32SubTexYoffset = (IMG_UINT32)y;
		sSrcReadInfo.ui32SubTexWidth   = (IMG_UINT32)width;
		sSrcReadInfo.ui32SubTexHeight  = (IMG_UINT32)height;
		sSrcReadInfo.pui8SubTexBuffer  = IMG_NULL;

		/*
		 * The texture may become incomplete, so we need to kick the 3D and
		 * notify all framebuffers attached to this texture
		 */
		FBOAttachableHasBeenModified(gc, (GLES1FrameBufferAttachable*)psMipLevel);

		/* Make sure to flush the read render surface, not the write one */
		if(ScheduleTA(gc, psReadParams->psRenderSurface, GLES1_SCHEDULE_HW_MIDSCENE_RENDER | GLES1_SCHEDULE_HW_LAST_IN_SCENE) != IMG_EGL_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"glCopyTexImage2D: Can't flush HW"));

			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);

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
				GLES1_INC_PIXEL_COUNT(GLES1_TIMES_glCopyTexImage2D, width*height);
				GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);

				return;
			}
		}
	}


	/*********** Option 2: SW Solution ********************/
	{
	    /* Allocate memory for the level data */
	    pui8Dest = TextureCreateLevel(gc, psTex, ui32Level, internalformat,	psTexFormat, (IMG_UINT32)width, (IMG_UINT32)height);


		/* Copy image data */
		if (pui8Dest) 
		{
			if(!SetupReadPixelsSpanInfo(gc, &sSpanInfo, x, y, (IMG_UINT32)width, (IMG_UINT32)height,
										eTextureHWFormat, type, IMG_FALSE, psReadParams))
			{
				GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);

				return;
			}

			/*
			 * The texture may become incomplete, so we need to kick the 3D and
			 * notify all framebuffers attached to this texture
			 */
			FBOAttachableHasBeenModified(gc, (GLES1FrameBufferAttachable*)psMipLevel);

			/* Make sure to flush the read render surface, not the write one */
			if(ScheduleTA(gc, psReadParams->psRenderSurface, 
						  GLES1_SCHEDULE_HW_MIDSCENE_RENDER | GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_WAIT_FOR_3D) != IMG_EGL_NO_ERROR)
			{
				PVR_DPF((PVR_DBG_ERROR,"glCopyTexImage2D: Can't flush HW"));
				
				GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);
				
				return;
			}

			pvSurfacePointer = GetStridedSurfaceData(gc, psReadParams, &sSpanInfo);
			
			if(!pvSurfacePointer)
			{
				PVR_DPF((PVR_DBG_ERROR,"glCopyTexImage2D: Failed to get strided data"));

				GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);
				
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
				GLES1Free(gc, pvSurfacePointer);
			}
		}

		if(level == 0 && psTex->sState.bGenerateMipmap)
		{
			MakeTextureMipmapLevels(gc, psTex, ui32Face);
		}

		/* 
		 * remove texture from active list.  If needed, validation will reload 
		 */
		TextureRemoveResident(gc, psTex);

		gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

		GLES1_INC_PIXEL_COUNT(GLES1_TIMES_glCopyTexImage2D, width*height);
		GLES1_TIME_STOP(GLES1_TIMES_glCopyTexImage2D);

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
GL_API void GL_APIENTRY glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset,
								GLint x, GLint y, GLsizei width, GLsizei height)
{
	GLESTexture *psTex;
	const GLESTextureFormat *psTargetTexFormat;
	const GLESTextureFormat *psTexFormat;
	PFNSpanPack pfnSpanPack;
	GLenum type;
	IMG_UINT8 *pui8Dest;
	GLESPixelSpanInfo sSpanInfo = {0};
	IMG_VOID *pvSurfacePointer;
	IMG_UINT32 i, ui32Lod=(IMG_UINT32)level, ui32Level, ui32Face;
	IMG_UINT32 ui32DstStride;
	EGLDrawableParams *psReadParams;
	GLESMipMapLevel *psMipLevel;
	GLenum eBaseHWTextureFormat;
	IMG_UINT32 ui32TopUsize, ui32TopVsize;

	IMG_BOOL bHWCopySubTextureUploaded = IMG_FALSE;

	__GLES1_GET_CONTEXT();
	

	PVR_DPF((PVR_DBG_CALLTRACE,"glCopyTexSubImage2D"));

	GLES1_TIME_START(GLES1_TIMES_glCopyTexSubImage2D);

	/* Check arguments and get the right texture being changed */
	psTex = CheckTexSubImageArgs(gc, target, level, xoffset, yoffset, width, height, 
								 IMG_NULL, &ui32Face, &ui32Level);

	if (!psTex) 
	{
		GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);

		return;
	}

	psMipLevel = &psTex->psMipLevel[ui32Level];

	psTargetTexFormat = psMipLevel->psTexFormat;
	eBaseHWTextureFormat = psMipLevel->eRequestedFormat;
	
	switch(eBaseHWTextureFormat)
	{
		case GL_RGB:
		case GL_RGBA:
#if defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888)
		case GL_BGRA_EXT:
#endif /* defined(GLES1_EXTENSION_TEXTURE_FORMAT_BGRA8888) */
		case GL_ALPHA:
		case GL_LUMINANCE:
		case GL_LUMINANCE_ALPHA:
		{
			break;
		}
		/*
		 * Set bad operation for attempts to do a copy tex image to a palette
		 * texture. This is what the Khronos spec says.
		 */
		case GL_PALETTE4_RGB8_OES:
		case GL_PALETTE4_RGBA8_OES:
		case GL_PALETTE4_R5_G6_B5_OES:
		case GL_PALETTE4_RGBA4_OES:
		case GL_PALETTE4_RGB5_A1_OES:
		case GL_PALETTE8_RGB8_OES:
		case GL_PALETTE8_RGBA8_OES:
		case GL_PALETTE8_R5_G6_B5_OES:
		case GL_PALETTE8_RGBA4_OES:
		case GL_PALETTE8_RGB5_A1_OES:
#if defined(GLES1_EXTENSION_PVRTC)
		/* Our compressed texture spec is silent on what error to return here
		 * but we'll bow to precedent and set bad operation
		 */
		case GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG:
		case GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG:
		case GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG:
		case GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG:
#endif /* defined(GLES1_EXTENSION_PVRTC) */
#if defined(GLES1_EXTENSION_ETC1)
		case GL_ETC1_RGB8_OES:
#endif /* defined(GLES1_EXTENSION_ETC1) */
		{
bad_op:
			SetError(gc, GL_INVALID_OPERATION);
			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
			return;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
			return;
		}
	}

	if(GetFrameBufferCompleteness(gc) != GL_FRAMEBUFFER_COMPLETE_OES)
	{
		SetError(gc, GL_INVALID_FRAMEBUFFER_OPERATION_OES);
		GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
		return;
	}

	psReadParams = gc->psReadParams;

	/* No read surface specified */
	if(!psReadParams->psRenderSurface)
	{
		goto bad_op;
	}

	if(psTargetTexFormat->ui32BaseFormatIndex == GLES1_RGB_TEX_INDEX)
	{
		/* Actual HW format is RGBA */
		eBaseHWTextureFormat = GL_RGBA;
	}


	/* We support conversion from any of our FB formats, 
	 * (including both 8888 orders - from binding a texture as an FBO), to any of our valid texture formats which
	 * has as many or less base internal components. ie, RGBA->RGBA/RGB/L/LA/A, RGB->RGB/L/LA/A.
	 */
	switch(gc->psReadParams->ePixelFormat)
	{
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		case PVRSRV_PIXEL_FORMAT_XRGB8888:
		{
		    /* Setup psTexFormat */
		    psTexFormat = &TexFormatABGR8888;

			switch(psTargetTexFormat->ePixelFormat)
			{
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				{
					if(psTargetTexFormat->ui32BaseFormatIndex == GLES1_RGBA_TEX_INDEX)
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
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		case PVRSRV_PIXEL_FORMAT_XBGR8888:
		{
		    /* Setup psTexFormat */
		    psTexFormat = &TexFormatABGR8888;

			switch(psTargetTexFormat->ePixelFormat)
			{
				case PVRSRV_PIXEL_FORMAT_ABGR8888:
				{
					if(psTargetTexFormat->ui32BaseFormatIndex == GLES1_RGBA_TEX_INDEX)
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
					if(psTargetTexFormat->ui32BaseFormatIndex == GLES1_RGBA_TEX_INDEX)
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
					if(psTargetTexFormat->ui32BaseFormatIndex == GLES1_RGBA_TEX_INDEX)
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
					if(psTargetTexFormat->ui32BaseFormatIndex == GLES1_RGB_TEX_INDEX)
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

#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageTarget)
	{
	    IMG_UINT32 ui32BufferSize;

	    GLES1_ASSERT(psTex->psEGLImageTarget->ui32Width==psMipLevel->ui32Width && psTex->psEGLImageTarget->ui32Height==psMipLevel->ui32Height);
		GLES1_ASSERT(psMipLevel->pui8Buffer==GLES1_LOADED_LEVEL)

		ui32DstStride = psMipLevel->ui32Width * psTargetTexFormat->ui32TotalBytesPerTexel;

	    ui32BufferSize = ui32DstStride * psMipLevel->ui32Height;
		
		PVRSRVLockMutex(gc->psSharedState->hTertiaryLock);

		psMipLevel->pui8Buffer = GLES1MallocHeapUNC(gc, ui32BufferSize);

		if(!psMipLevel->pui8Buffer)
		{
		    SetError(gc, GL_OUT_OF_MEMORY);

			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);

			return;
		}

		if(!SetupReadPixelsSpanInfo(gc, &sSpanInfo, x, y, (IMG_UINT32)width, (IMG_UINT32)height, eBaseHWTextureFormat,
									type, IMG_FALSE, psReadParams))
		{
			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);

			return;
		}

		if(ScheduleTA(gc, psReadParams->psRenderSurface, 
			GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_WAIT_FOR_3D) != IMG_EGL_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Can't flush HW"));

			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);

			return;
		}

		pvSurfacePointer = GetStridedSurfaceData(gc, psReadParams, &sSpanInfo);

		if(!pvSurfacePointer)
		{
			PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Failed to get strided data"));

			PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);

			return;
		}

		sSpanInfo.pvInData = (IMG_VOID *) ((IMG_UINT8 *)pvSurfacePointer +
									sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement +
									sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);

		sSpanInfo.ui32DstRowIncrement = ui32DstStride;
		sSpanInfo.ui32DstSkipPixels += xoffset;	/* PRQA S 3760 */ /* xoffset is always positive. */
		sSpanInfo.ui32DstSkipLines += yoffset;	/* PRQA S 3760 */ /* yoffset is always positive. */
		
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

		GLES1FreeHeapUNC(gc, psMipLevel->pui8Buffer);

		psMipLevel->pui8Buffer = GLES1_LOADED_LEVEL;

		if(pvSurfacePointer!=psReadParams->pvLinSurfaceAddress)
		{
			GLES1Free(gc, pvSurfacePointer);
		}

		PVRSRVUnlockMutex(gc->psSharedState->hTertiaryLock);

		GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);

		return;
	}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */


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
		 (pui8Dest == GLES1_LOADED_LEVEL)        &&
		 (width != 0) && (height != 0)           &&
		 (psTargetTexFormat == psTexFormat)      &&
		 !(level == 0 && psTex->sState.bGenerateMipmap) )
	{

		SGX_QUEUETRANSFER sQueueTransfer;
		
	    GLESSubTextureInfo sSrcReadInfo;
	    GLESSubTextureInfo sDstSubTexInfo;

		IMG_UINT32 ui32BytesPerTexel = psTex->psFormat->ui32TotalBytesPerTexel;
		IMG_UINT32 ui32OffsetInBytes;
		
		/* Setup ui32OffsetInBytes */
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
		ui32TopUsize = 1U << ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
		ui32TopVsize = 1U << ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
		ui32TopUsize = 1 + ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
		ui32TopVsize = 1 + ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

		ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
		{
			IMG_UINT32 ui32FaceOffset = 
			  ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);
			
			if(psTex->ui32HWFlags & GLES1_MIPMAP)
			{
				if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
				   (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}
			
			ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
		}	
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */

		sSpanInfo.i32ReadX 	= x;
		sSpanInfo.i32ReadY 	= y;
		sSpanInfo.ui32Width = (IMG_UINT32)width;
		sSpanInfo.ui32Height = (IMG_UINT32)height;

		if (!ClipReadPixels(&sSpanInfo, psReadParams))
		{
			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
			return;
		}

		sSrcReadInfo.ui32SubTexXoffset = sSpanInfo.i32ReadX;
		sSrcReadInfo.ui32SubTexYoffset = sSpanInfo.i32ReadY;
		sSrcReadInfo.ui32SubTexWidth = sSpanInfo.ui32Width;
		sSrcReadInfo.ui32SubTexHeight = sSpanInfo.ui32Height;
		sSrcReadInfo.pui8SubTexBuffer = IMG_NULL;

		sDstSubTexInfo.ui32SubTexXoffset = (IMG_UINT32)xoffset + sSpanInfo.ui32DstSkipPixels;
		sDstSubTexInfo.ui32SubTexYoffset = (IMG_UINT32)yoffset + sSpanInfo.ui32DstSkipLines;

		sDstSubTexInfo.ui32SubTexWidth   = sSpanInfo.ui32Width;
		sDstSubTexInfo.ui32SubTexHeight  = sSpanInfo.ui32Height;
		sDstSubTexInfo.pui8SubTexBuffer  = IMG_NULL;

		/*
		 * The texture may become incomplete, so we need to kick the 3D and
		 * notify all framebuffers attached to this texture
		 */
		FBOAttachableHasBeenModified(gc, (GLES1FrameBufferAttachable*)psMipLevel);

		/* Make sure to flush the read render surface, not the write one */
		if(ScheduleTA(gc, psReadParams->psRenderSurface, GLES1_SCHEDULE_HW_MIDSCENE_RENDER | GLES1_SCHEDULE_HW_LAST_IN_SCENE) != IMG_EGL_NO_ERROR)
		{
			PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Couldn't flush HW"));
			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
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
				GLES1_INC_PIXEL_COUNT(GLES1_TIMES_glCopyTexSubImage2D, width*height);
				GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);

				return;
			}
		}
	}


	/********** PATH 2 & 3: using SW + HWTQ texture upload & pure SW **********/

	/* 1ST STEP: read back the whole level old data */

	/* if the host copy of the whole level texture has not been uploaded 
	   do nothing about reading back the old data */

	if (pui8Dest != GLES1_LOADED_LEVEL)
	{
		if(!pui8Dest)
		{
			SetError(gc, GL_OUT_OF_MEMORY);
			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
			return;
		}
	}
	else
	{
	    /* directly transfer the whole level old data into the device memory
		   if the host copy of the whole level texture has already been uploaded */

	    if ( (!gc->sAppHints.bDisableHWTQTextureUpload) &&
			 (height) && (width) )  /* HWTQ cannot upload zero-sized (sub)texture */
		{
			PVRSRV_CLIENT_MEM_INFO sMemInfo  = {0};
			GLESTextureParamState    *psParams = &psTex->sState;

			/* if the current texture is being used, then ghost the texture 
			   and create new texture device memory */

			if(psTex->psMemInfo && 
			   (KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource)))
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

				psParams->ui32StateWord2 = 
				        (psTex->psMemInfo->sDevVAddr.uiAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << 
				        EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;

				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
			}


			/* and copy the old device memory to the new memory */
			if(((IMG_UINT32)width  == psMipLevel->ui32Width) && 
			   ((IMG_UINT32)height == psMipLevel->ui32Height) && 
			   ((psTex->ui32HWFlags & GLES1_MIPMAP) == 0)
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
			   && (psTex->ui32TextureTarget != GLES1_TEXTURE_TARGET_CEM)
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
			   )
			{
			    FlushAttachableIfNeeded(gc, (GLES1FrameBufferAttachable*)psMipLevel, GLES1_SCHEDULE_HW_DISCARD_SCENE);
			}
			else
			{
			    /* Render to mipmap before reading it back */
			    FlushAttachableIfNeeded(gc, (GLES1FrameBufferAttachable*)psMipLevel,
										GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_WAIT_FOR_3D);
			
				if ((sMemInfo.uAllocSize)) /* transfer the whole texture data of all levels from the old device memory to the new memory */
				{
				    CopyTextureData(gc, psTex, 0, &sMemInfo, 0, sMemInfo.uAllocSize);
				}
				/*else: MemInfo hasn't been ghosted, the original one is being used. */
			}
		}
	}


	/* 2ND STEP: Copy the new subtexture data */

	/* Setup for copy image data */

	if(!SetupReadPixelsSpanInfo(gc, &sSpanInfo, x, y, (IMG_UINT32)width, (IMG_UINT32)height, eBaseHWTextureFormat, 
								type, IMG_FALSE, psReadParams))
	{
		GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
		return;
	}

	if(ScheduleTA(gc, psReadParams->psRenderSurface, 
				  GLES1_SCHEDULE_HW_MIDSCENE_RENDER | GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_WAIT_FOR_3D) != IMG_EGL_NO_ERROR)
	{
	    PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Can't flush HW"));
		GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
		return;
	}

	pvSurfacePointer = GetStridedSurfaceData(gc, psReadParams, &sSpanInfo);

	if(!pvSurfacePointer)
	{
		PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Failed to get strided data"));

		GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);

		return;
	}
	

	/* deal with rendering to texture */

#if defined(EGL_EXTENSION_RENDER_TO_TEXTURE)
	if(psTex->hPBuffer)
	{
        EGLDrawableParams sParams;
		
		if (!KEGLGetDrawableParameters(psTex->hPBuffer, &sParams, IMG_TRUE))
		{
	        PVR_DPF((PVR_DBG_ERROR,"glCopyTexSubImage2D: Can't get drawable info"));

			GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);

			return;
		}

		/* Stride is actually texture level stride, not read area stride */
		sSpanInfo.ui32DstRowIncrement = ui32DstStride;
		sSpanInfo.ui32DstSkipPixels += (IMG_UINT32)xoffset;
		sSpanInfo.ui32DstSkipLines += (IMG_UINT32)yoffset;

		sSpanInfo.pvInData = (IMG_VOID *) ((IMG_UINT8 *)pvSurfacePointer +
										   sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement +
										   sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);

		sSpanInfo.pvOutData = (IMG_VOID *) ( (IMG_UINT8 *)(sParams.pvLinSurfaceAddress) +
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
#endif /* defined(EGL_EXTENSION_RENDER_TO_TEXTURE) */
	{
	    /* if the host copy of the whole level texture has not been uploaded 
		   copy the new subtexture data to the host memory */
	    if (pui8Dest != GLES1_LOADED_LEVEL)
		{
		    sSpanInfo.ui32DstRowIncrement = ui32DstStride;
			sSpanInfo.ui32DstSkipPixels += (IMG_UINT32)xoffset;
			sSpanInfo.ui32DstSkipLines += (IMG_UINT32)yoffset;

			sSpanInfo.pvInData = (IMG_VOID *) ((IMG_UINT8 *)pvSurfacePointer +
											   sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement +
											   sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);

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

			if ( (!gc->sAppHints.bDisableHWTQTextureUpload)     && 
				 (sSpanInfo.ui32Height) && (sSpanInfo.ui32Width) ) /* HWTQ cannot upload zero-sized (sub)texture */
			{
			    IMG_UINT32 ui32BytesPerTexel = psTex->psFormat->ui32TotalBytesPerTexel;
				IMG_UINT32 ui32OffsetInBytes;
				IMG_UINT32 ui32SubTexBufferSize;
				IMG_UINT8 *pui8SubTexBuffer;

				SGX_QUEUETRANSFER sQueueTransfer;
				GLESSubTextureInfo sSubTexInfo;

#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
				ui32TopUsize = 1U << ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
				ui32TopVsize = 1U << ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
				ui32TopUsize = 1 + ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
				ui32TopVsize = 1 + ((psTex->sState.ui32StateWord1 & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif

				/* Setup ui32OffsetInBytes */
				ui32OffsetInBytes = ui32BytesPerTexel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);

#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
				if(psTex->ui32TextureTarget == GLES1_TEXTURE_TARGET_CEM)
				{
					IMG_UINT32 ui32FaceOffset = 
					  ui32BytesPerTexel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);
			
					if(psTex->ui32HWFlags & GLES1_MIPMAP)
					{
						if(((ui32BytesPerTexel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
						   (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
						{
							ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
						}
					}
			
					ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
				}	
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */


				ui32SubTexBufferSize = (IMG_UINT32)(sSpanInfo.ui32Width * sSpanInfo.ui32Height * psTargetTexFormat->ui32TotalBytesPerTexel);
				pui8SubTexBuffer = GLES1MallocHeapUNC(gc, ui32SubTexBufferSize);

				if (!pui8SubTexBuffer)
				{
					SetError(gc, GL_OUT_OF_MEMORY);
					GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
					return;
				}

				/* Setup sSubTexInfo */
				sSubTexInfo.ui32SubTexXoffset = (IMG_UINT32)xoffset;
				sSubTexInfo.ui32SubTexYoffset = (IMG_UINT32)yoffset;
				sSubTexInfo.ui32SubTexWidth   = sSpanInfo.ui32Width;
				sSubTexInfo.ui32SubTexHeight  = sSpanInfo.ui32Height;
				sSubTexInfo.pui8SubTexBuffer  = pui8SubTexBuffer;


#if (defined(DEBUG) || defined(TIMING))
				ui32TextureMemCurrent += ui32SubTexBufferSize;

				if (ui32TextureMemCurrent>ui32TextureMemHWM)
				{
					ui32TextureMemHWM = ui32TextureMemCurrent;
				}
#endif
				/* prepare data */
				sSpanInfo.ui32DstRowIncrement = sSpanInfo.ui32Width * psTargetTexFormat->ui32TotalBytesPerTexel;
				/*
				sSpanInfo.ui32DstSkipPixels += 0;
				sSpanInfo.ui32DstSkipLines += 0;
				*/
				sSpanInfo.pvInData = (IMG_VOID *) ((IMG_UINT8 *)pvSurfacePointer +
												   sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement +
												   sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);

				sSpanInfo.pvOutData = (IMG_VOID *) (pui8SubTexBuffer);


				i = sSpanInfo.ui32Height;
				
				do
				{
					(*pfnSpanPack)(&sSpanInfo);
					
					sSpanInfo.pvInData = (IMG_UINT8 *)sSpanInfo.pvInData + sSpanInfo.i32SrcRowIncrement;
					sSpanInfo.pvOutData = (IMG_UINT8 *)sSpanInfo.pvOutData + sSpanInfo.ui32DstRowIncrement;
				}
				while(--i);

				
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

				GLES1FreeHeapUNC(gc, pui8SubTexBuffer);
			}

			/* otherwise, use the software copy subtexture uploading:
			   readback the whole level texture data,
			   and copy subtexture data to the host memory. */

			if(!bHWCopySubTextureUploaded)
			{
				if ((sSpanInfo.ui32Height) && (sSpanInfo.ui32Width))
				{
				    IMG_UINT32 ui32BufferSize = ui32DstStride * psMipLevel->ui32Height;
					pui8Dest = GLES1MallocHeapUNC(gc, ui32BufferSize);
				
					if(!pui8Dest)
					{
						SetError(gc, GL_OUT_OF_MEMORY);
						GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
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
					    FlushAttachableIfNeeded(gc, (GLES1FrameBufferAttachable*)psMipLevel, GLES1_SCHEDULE_HW_DISCARD_SCENE);
					}
					else
					{
					    /* Render to mipmap before reading it back */
					    FlushAttachableIfNeeded(gc, (GLES1FrameBufferAttachable*)psMipLevel, 
												GLES1_SCHEDULE_HW_LAST_IN_SCENE | GLES1_SCHEDULE_HW_WAIT_FOR_3D);

						/* otherwise already flushed */
						ReadBackTextureData(gc, psTex, ui32Face, (IMG_UINT32)level, pui8Dest);
					}

					psMipLevel->pui8Buffer = pui8Dest;


					/* if (pui8Dest) */
					{
						/* Stride is actually texture level stride, not read area stride */
						sSpanInfo.ui32DstRowIncrement = ui32DstStride;
						sSpanInfo.ui32DstSkipPixels += (IMG_UINT32)xoffset;
						sSpanInfo.ui32DstSkipLines += (IMG_UINT32)yoffset;

						sSpanInfo.pvInData = (IMG_VOID *) ((IMG_UINT8 *)pvSurfacePointer +
														   sSpanInfo.i32ReadY * sSpanInfo.i32SrcRowIncrement +
														   sSpanInfo.i32ReadX * sSpanInfo.i32SrcGroupIncrement);

						sSpanInfo.pvOutData = (IMG_VOID *) ((psMipLevel->pui8Buffer) +
															sSpanInfo.ui32DstSkipLines * sSpanInfo.ui32DstRowIncrement +
															sSpanInfo.ui32DstSkipPixels * sSpanInfo.ui32DstGroupIncrement);


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

				gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE|GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
			}
		}
	}


	if(level == 0 && psTex->sState.bGenerateMipmap)
	{
		MakeTextureMipmapLevels(gc, psTex, ui32Face);
	}


#if defined(GLES1_EXTENSION_EGL_IMAGE)
	if(psTex->psEGLImageSource || psTex->psEGLImageTarget)
	{
		UpdateEGLImage(gc, psTex, ui32Level);
	}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

	if(pvSurfacePointer!=psReadParams->pvLinSurfaceAddress)
	{
		GLES1Free(gc, pvSurfacePointer);
	}

	GLES1_INC_PIXEL_COUNT(GLES1_TIMES_glCopyTexSubImage2D, width*height);
	GLES1_TIME_STOP(GLES1_TIMES_glCopyTexSubImage2D);
}



/***********************************************************************************
 Function Name      : glTexEnv[f/fv/x/xv]
 Inputs             : target, pname, param
 Outputs            : 
 Returns            : -
 Description        : ENTRYPOINT: Sets texture environment state of active texture 
					  unit. Cannot setup state words yet, as texture blend is a 
					  combination of the texture environment and the format of the 
					  currently bound texture.
************************************************************************************/
#if defined(PROFILE_COMMON)

/***********************************************************************************
 Function Name      : TexEnvfv
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID TexEnvfv(GLES1Context *gc, GLenum target, GLenum pname, const GLfloat *params)
{
	IMG_UINT32 ui32DirtyBits = 0;

	switch (target)
	{
		case GL_TEXTURE_ENV:
		{
			switch (pname)
			{
				case GL_TEXTURE_ENV_MODE:
				{
					GLenum e = (GLenum) params[0];

					switch(e)
					{
						case GL_MODULATE:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_MODULATE_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_DECAL:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_DECAL_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_BLEND:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_BLEND_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_REPLACE:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_REPLACE_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_ADD:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_ADD_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_COMBINE:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_COMBINE_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						default:
						{
							SetError(gc, GL_INVALID_ENUM);
						}
					}

					break;
				}
				case GL_TEXTURE_ENV_COLOR:
				{
					gc->sState.sTexture.psActive->sEnv.sColor.fRed = params[0];
					gc->sState.sTexture.psActive->sEnv.sColor.fGreen = params[1];
					gc->sState.sTexture.psActive->sEnv.sColor.fBlue = params[2];
					gc->sState.sTexture.psActive->sEnv.sColor.fAlpha = params[3];

					gc->sState.sTexture.psActive->sEnv.ui32Color = ColorConvertToHWFormat(&gc->sState.sTexture.psActive->sEnv.sColor);

					ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

					break;
				}
				case GL_COMBINE_RGB:
				case GL_COMBINE_ALPHA:
				case GL_SRC0_RGB:
				case GL_SRC1_RGB:
				case GL_SRC2_RGB:
				case GL_SRC0_ALPHA:
				case GL_SRC1_ALPHA:
				case GL_SRC2_ALPHA:
				case GL_OPERAND0_RGB:
				case GL_OPERAND1_RGB:
				case GL_OPERAND2_RGB:
				case GL_OPERAND0_ALPHA:
				case GL_OPERAND1_ALPHA:
				case GL_OPERAND2_ALPHA:
				case GL_RGB_SCALE:
				case GL_ALPHA_SCALE:
				{
					TexEnvCombine(gc, pname, (GLenum) params[0]);

					ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

					break;
				}
				default:
				{
					SetError(gc, GL_INVALID_ENUM);
				}
			}
			break;
		}
		case GL_POINT_SPRITE_OES:
		{
			if(pname == GL_COORD_REPLACE_OES)
			{
				IMG_BOOL bReplace = params[0] ? IMG_TRUE : IMG_FALSE;

				if(gc->sState.sTexture.psActive->sEnv.bPointSpriteReplace != bReplace)
				{
					gc->sState.sTexture.psActive->sEnv.bPointSpriteReplace = bReplace;

					ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_VERTEX_PROGRAM;
				}
			}
			else
			{
				SetError(gc, GL_INVALID_ENUM);
			}

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
		}
	}

	gc->ui32DirtyMask |= ui32DirtyBits | GLES1_DIRTYFLAG_TEXTURE_STATE;
}	



GL_API void GL_APIENTRY glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexEnvfv"));

	GLES1_TIME_START(GLES1_TIMES_glTexEnvfv);

	TexEnvfv(gc, target, pname, params);

	GLES1_TIME_STOP(GLES1_TIMES_glTexEnvfv);
}


GL_API void GL_APIENTRY glTexEnvf(GLenum target, GLenum pname, GLfloat param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexEnvf"));

	GLES1_TIME_START(GLES1_TIMES_glTexEnvf);

	switch(pname)
	{
		case GL_TEXTURE_ENV_MODE:
		case GL_COMBINE_RGB:
		case GL_COMBINE_ALPHA:
		case GL_SRC0_RGB:
		case GL_SRC1_RGB:
		case GL_SRC2_RGB:
		case GL_SRC0_ALPHA:
		case GL_SRC1_ALPHA:
		case GL_SRC2_ALPHA:
		case GL_OPERAND0_RGB:
		case GL_OPERAND1_RGB:
		case GL_OPERAND2_RGB:
		case GL_OPERAND0_ALPHA:
		case GL_OPERAND1_ALPHA:
		case GL_OPERAND2_ALPHA:
		case GL_RGB_SCALE:
		case GL_ALPHA_SCALE:
		case GL_COORD_REPLACE_OES:
		{
			TexEnvfv(gc, target, pname, &param);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glTexEnvf);
}

#endif /* PROFILE_COMMON */ 


/***********************************************************************************
 Function Name      : TexEnvxv
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID TexEnvxv(GLES1Context *gc, GLenum target, GLenum pname, const GLfixed *params)
{
	IMG_UINT32 ui32DirtyBits = 0;

	switch (target)
	{
		case GL_TEXTURE_ENV:
		{
			switch(pname)
			{
				case GL_TEXTURE_ENV_MODE:
				{
					GLenum e = (GLenum) params[0];

					switch(e)
					{
						case GL_MODULATE:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_MODULATE_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_DECAL:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_DECAL_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_BLEND:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_BLEND_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_REPLACE:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_REPLACE_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_ADD:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_ADD_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_COMBINE:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_COMBINE_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						default:
						{
							SetError(gc, GL_INVALID_ENUM);
						}
					}

					break;
				}
				case GL_TEXTURE_ENV_COLOR:
				{
					IMG_FLOAT afParams[4];

					afParams[0] = FIXED_TO_FLOAT(params[0]);
					afParams[1] = FIXED_TO_FLOAT(params[1]);
					afParams[2] = FIXED_TO_FLOAT(params[2]);
					afParams[3] = FIXED_TO_FLOAT(params[3]);

					gc->sState.sTexture.psActive->sEnv.sColor.fRed = afParams[0];
					gc->sState.sTexture.psActive->sEnv.sColor.fGreen = afParams[1];
					gc->sState.sTexture.psActive->sEnv.sColor.fBlue = afParams[2];
					gc->sState.sTexture.psActive->sEnv.sColor.fAlpha = afParams[3];

					gc->sState.sTexture.psActive->sEnv.ui32Color = ColorConvertToHWFormat(&gc->sState.sTexture.psActive->sEnv.sColor);

					ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

					break;
				}
				case GL_COMBINE_RGB:
				case GL_COMBINE_ALPHA:
				case GL_SRC0_RGB:
				case GL_SRC1_RGB:
				case GL_SRC2_RGB:
				case GL_SRC0_ALPHA:
				case GL_SRC1_ALPHA:
				case GL_SRC2_ALPHA:
				case GL_OPERAND0_RGB:
				case GL_OPERAND1_RGB:
				case GL_OPERAND2_RGB:
				case GL_OPERAND0_ALPHA:
				case GL_OPERAND1_ALPHA:
				case GL_OPERAND2_ALPHA:
				{
					TexEnvCombine(gc, pname, (GLenum) params[0]);
					
					ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;
					
					break;
				}
				case GL_RGB_SCALE:
				case GL_ALPHA_SCALE:
				{
					GLenum eParam = (IMG_UINT32)FIXED_TO_LONG(params[0]);

					TexEnvCombine(gc, pname, eParam);

					ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

					break;
				}
				default:
				{
					SetError(gc, GL_INVALID_ENUM);
				}
			}
			break;
		}
		case GL_POINT_SPRITE_OES:
		{
			if(pname == GL_COORD_REPLACE_OES)
			{
				IMG_BOOL bReplace = params[0] ? IMG_TRUE : IMG_FALSE;

				if(gc->sState.sTexture.psActive->sEnv.bPointSpriteReplace != bReplace)
				{
					gc->sState.sTexture.psActive->sEnv.bPointSpriteReplace = bReplace;

					ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_VERTEX_PROGRAM;
				}
			}
			else
			{
				SetError(gc, GL_INVALID_ENUM);
			}

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
		}
	}

	gc->ui32DirtyMask |= ui32DirtyBits | GLES1_DIRTYFLAG_TEXTURE_STATE;
}	


GL_API void GL_APIENTRY glTexEnvxv(GLenum target, GLenum pname, const GLfixed *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexEnvxv"));

	GLES1_TIME_START(GLES1_TIMES_glTexEnvxv);

	TexEnvxv(gc, target, pname, params);

	GLES1_TIME_STOP(GLES1_TIMES_glTexEnvxv);
}


GL_API void GL_APIENTRY glTexEnvx(GLenum target, GLenum pname, GLfixed param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexEnvx"));

	GLES1_TIME_START(GLES1_TIMES_glTexEnvx);

	switch(pname)
	{
		case GL_TEXTURE_ENV_MODE:
		case GL_COMBINE_RGB:
		case GL_COMBINE_ALPHA:
		case GL_SRC0_RGB:
		case GL_SRC1_RGB:
		case GL_SRC2_RGB:
		case GL_SRC0_ALPHA:
		case GL_SRC1_ALPHA:
		case GL_SRC2_ALPHA:
		case GL_OPERAND0_RGB:
		case GL_OPERAND1_RGB:
		case GL_OPERAND2_RGB:
		case GL_OPERAND0_ALPHA:
		case GL_OPERAND1_ALPHA:
		case GL_OPERAND2_ALPHA:
		case GL_RGB_SCALE:
		case GL_ALPHA_SCALE:
		case GL_COORD_REPLACE_OES:
		{
			TexEnvxv(gc, target, pname, &param);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);

			break;
		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glTexEnvx);
}


/***********************************************************************************
 Function Name      : TexEnviv
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID TexEnviv(GLES1Context *gc, GLenum target, GLenum pname, const GLint *params)
{
	IMG_UINT32 ui32DirtyBits = 0;

	switch (target)
	{
		case GL_TEXTURE_ENV:
		{
			switch (pname)
			{
				case GL_TEXTURE_ENV_MODE:
				{
					GLenum e = (GLenum) params[0];

					switch(e)
					{
						case GL_MODULATE:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_MODULATE_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_DECAL:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_DECAL_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_BLEND:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_BLEND_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_REPLACE:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_REPLACE_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_ADD:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_ADD_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						case GL_COMBINE:
						{
							gc->sState.sTexture.psActive->sEnv.ui32Mode = GLES1_COMBINE_INDEX;

							ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

							break;
						}
						default:
						{
							SetError(gc, GL_INVALID_ENUM);
						}
					}

					break;
				}
				case GL_TEXTURE_ENV_COLOR:
				{
					IMG_FLOAT afParams[4];

					afParams[0] = GLES1_INT_TO_FLOAT(params[0]);
					afParams[1] = GLES1_INT_TO_FLOAT(params[1]);
					afParams[2] = GLES1_INT_TO_FLOAT(params[2]);
					afParams[3] = GLES1_INT_TO_FLOAT(params[3]);

					gc->sState.sTexture.psActive->sEnv.sColor.fRed = afParams[0];
					gc->sState.sTexture.psActive->sEnv.sColor.fGreen = afParams[1];
					gc->sState.sTexture.psActive->sEnv.sColor.fBlue = afParams[2];
					gc->sState.sTexture.psActive->sEnv.sColor.fAlpha = afParams[3];
					gc->sState.sTexture.psActive->sEnv.ui32Color = ColorConvertToHWFormat(&gc->sState.sTexture.psActive->sEnv.sColor);

					ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGPROG_CONSTANTS;

					break;
				}
				case GL_COMBINE_RGB:
				case GL_COMBINE_ALPHA:
				case GL_SRC0_RGB:
				case GL_SRC1_RGB:
				case GL_SRC2_RGB:
				case GL_SRC0_ALPHA:
				case GL_SRC1_ALPHA:
				case GL_SRC2_ALPHA:
				case GL_OPERAND0_RGB:
				case GL_OPERAND1_RGB:
				case GL_OPERAND2_RGB:
				case GL_OPERAND0_ALPHA:
				case GL_OPERAND1_ALPHA:
				case GL_OPERAND2_ALPHA:
				case GL_RGB_SCALE:
				case GL_ALPHA_SCALE:
				{
					TexEnvCombine(gc, pname, (GLenum) params[0]);

					ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM;

					break;
				}
				default:
				{
					SetError(gc, GL_INVALID_ENUM);
				}
			}

			break;
		}
		case GL_POINT_SPRITE_OES:
		{
			if(pname == GL_COORD_REPLACE_OES)
			{
				IMG_BOOL bReplace = params[0] ? IMG_TRUE : IMG_FALSE;

				if(gc->sState.sTexture.psActive->sEnv.bPointSpriteReplace != bReplace)
				{
					gc->sState.sTexture.psActive->sEnv.bPointSpriteReplace = bReplace;

					ui32DirtyBits |= GLES1_DIRTYFLAG_FRAGMENT_PROGRAM | GLES1_DIRTYFLAG_VERTEX_PROGRAM;
				}
			}
			else
			{
				SetError(gc, GL_INVALID_ENUM);
			}

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
		}
	}

	gc->ui32DirtyMask |= ui32DirtyBits | GLES1_DIRTYFLAG_TEXTURE_STATE;
}	


GL_API void GL_APIENTRY glTexEnviv(GLenum target, GLenum pname, const GLint *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexEnviv"));

	GLES1_TIME_START(GLES1_TIMES_glTexEnviv);

	TexEnviv(gc, target, pname, params);

	GLES1_TIME_STOP(GLES1_TIMES_glTexEnviv);
}


GL_API void GL_APIENTRY glTexEnvi(GLenum target, GLenum pname, GLint param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexEnvi"));

	GLES1_TIME_START(GLES1_TIMES_glTexEnvi);

	switch(pname)
	{
		case GL_TEXTURE_ENV_MODE:
		case GL_COMBINE_RGB:
		case GL_COMBINE_ALPHA:
		case GL_SRC0_RGB:
		case GL_SRC1_RGB:
		case GL_SRC2_RGB:
		case GL_SRC0_ALPHA:
		case GL_SRC1_ALPHA:
		case GL_SRC2_ALPHA:
		case GL_OPERAND0_RGB:
		case GL_OPERAND1_RGB:
		case GL_OPERAND2_RGB:
		case GL_OPERAND0_ALPHA:
		case GL_OPERAND1_ALPHA:
		case GL_OPERAND2_ALPHA:
		case GL_RGB_SCALE:
		case GL_ALPHA_SCALE:
		case GL_COORD_REPLACE_OES:
		{
			TexEnviv(gc, target, pname, &param);

			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			break;

		}
	}

	GLES1_TIME_STOP(GLES1_TIMES_glTexEnvi);
}

/***********************************************************************************
 Function Name      : TexParameterfv
 Inputs             : gc, eTarget, ePname, pvParams
 Outputs            : -
 Returns            : -
 Description        : UTILITY: Sets texture parameter state for the currently bound
					  texture.
************************************************************************************/
static IMG_VOID TexParameterfv(GLES1Context *gc, GLenum eTarget, GLenum ePname, const IMG_VOID *pvParams, IMG_UINT32 ui32Type, IMG_BOOL vecVersion)
{
	GLESTexture *psTex;
	GLESTextureParamState *psParamState;
	IMG_UINT32 ui32Target;
	GLenum eParam;

	switch(eTarget)
	{
		case GL_TEXTURE_2D:
		{
			ui32Target = GLES1_TEXTURE_TARGET_2D;

			break;
		}
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
		case GL_TEXTURE_CUBE_MAP_OES:
		{
			ConvertData(ui32Type, pvParams, GLES1_ENUM, &eParam, 1);

			ui32Target = GLES1_TEXTURE_TARGET_CEM;

			switch (ePname) 
			{
				/* Wrap modes are restricted for cube map textures */
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
			}

			break;
		}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */
#if defined(GLES1_EXTENSION_TEXTURE_STREAM)
		case GL_TEXTURE_STREAM_IMG:
#endif
#if defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		case GL_TEXTURE_EXTERNAL_OES:
#endif
#if defined(GLES1_EXTENSION_TEXTURE_STREAM) || defined(GLES1_EXTENSION_EGL_IMAGE_EXTERNAL)
		{
			ConvertData(ui32Type, pvParams, GLES1_ENUM, &eParam, 1);

			ui32Target = GLES1_TEXTURE_TARGET_STREAM;

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
#if defined(GLES1_EXTENSION_DRAW_TEXTURE)
			    case GL_TEXTURE_CROP_RECT_OES:
				{
				    break;
				}
#endif /* defined(GLES1_EXTENSION_DRAW_TEXTURE) */
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
#if defined(GLES1_EXTENSION_DRAW_TEXTURE)
		case GL_TEXTURE_CROP_RECT_OES:
		{
			if (vecVersion)
			{
				IMG_INT32 i32CropRect[4];
				ConvertData(ui32Type, pvParams, GLES1_INT32, i32CropRect, 4);

				psParamState->i32CropRectU = i32CropRect[0];
				psParamState->i32CropRectV = i32CropRect[1];
				psParamState->i32CropRectW = i32CropRect[2];
				psParamState->i32CropRectH = i32CropRect[3];

				break;
			}
			else
			{
				goto bad_enum;
			}
		}
#endif /* defined(GLES1_EXTENSION_DRAW_TEXTURE) */
		/* Wrap modes are stored directly in TSP stateword1 format */
		case GL_TEXTURE_WRAP_S:
		{
			ConvertData(ui32Type, pvParams, GLES1_ENUM, &eParam, 1);

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
#if defined(GLES1_EXTENSION_TEX_MIRRORED_REPEAT)
				case GL_MIRRORED_REPEAT_OES:
				{
					psParamState->ui32StateWord0 &= EURASIA_PDS_DOUTT0_UADDRMODE_CLRMSK;
					psParamState->ui32StateWord0 |= EURASIA_PDS_DOUTT0_UADDRMODE_FLIP;

					break;
				}
#endif /* defined(GLES1_EXTENSION_TEX_MIRRORED_REPEAT) */
				default:
				{
					goto bad_enum;
				}
			}
			break;
		}
		case GL_TEXTURE_WRAP_T:
		{
			ConvertData(ui32Type, pvParams, GLES1_ENUM, &eParam, 1);

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
#if defined(GLES1_EXTENSION_TEX_MIRRORED_REPEAT)
				case GL_MIRRORED_REPEAT_OES:
				{
					psParamState->ui32StateWord0 &= EURASIA_PDS_DOUTT0_VADDRMODE_CLRMSK;
					psParamState->ui32StateWord0 |= EURASIA_PDS_DOUTT0_VADDRMODE_FLIP;

					break;
				}
#endif /* defined(GLES1_EXTENSION_TEX_MIRRORED_REPEAT) */
				default:
				{
					goto bad_enum;
				}
			}
			break;
		}
		/* Min filter is stored directly in TSP stateword2 format */
		case GL_TEXTURE_MIN_FILTER:
		{
			ConvertData(ui32Type, pvParams, GLES1_ENUM, &eParam, 1);

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

			psTex->ui32LevelsConsistent = GLES1_TEX_UNKNOWN;

			gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE;

			break;
		}
		/* Mag filter is stored directly in TSP stateword2 format */
		case GL_TEXTURE_MAG_FILTER:
		{
			ConvertData(ui32Type, pvParams, GLES1_ENUM, &eParam, 1);

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
		case GL_GENERATE_MIPMAP:
		{
			ConvertData(ui32Type, pvParams, GLES1_ENUM, &eParam, 1);

			if(eParam > GL_TRUE)
			{
				goto bad_enum;
			}

			psParamState->bGenerateMipmap = eParam ? IMG_TRUE : IMG_FALSE;

#if defined(GLES1_EXTENSION_EGL_IMAGE)
			if(psParamState->bGenerateMipmap && psTex->psEGLImageTarget)
			{
				ReleaseImageFromTexture(gc, psTex);
			}
#endif /* defined(GLES1_EXTENSION_EGL_IMAGE) */

			break;
		}
		default:
		{
			goto bad_enum;
		}
	}

	gc->ui32DirtyMask |= GLES1_DIRTYFLAG_TEXTURE_STATE;
}


/***********************************************************************************
 Function Name      : glTexParameter[f/x]
 Inputs             : target, pname, param
 Outputs            : textures
 Returns            : -
 Description        : ENTRYPOINT: Sets texture parameter state for currently bound
					  texture.
************************************************************************************/
#if defined(PROFILE_COMMON)

GL_API void GL_APIENTRY glTexParameterf(GLenum target, GLenum pname, GLfloat param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexParameterf"));

	GLES1_TIME_START(GLES1_TIMES_glTexParameterf);

	TexParameterfv(gc, target, pname, (void *)&param, GLES1_FLOAT, IMG_FALSE);

	GLES1_TIME_STOP(GLES1_TIMES_glTexParameterf);
}
#endif

GL_API void GL_APIENTRY glTexParameterx(GLenum target, GLenum pname, GLfixed param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexParameterx"));

	GLES1_TIME_START(GLES1_TIMES_glTexParameterx);

	TexParameterfv(gc, target, pname, (void *)&param, GLES1_FIXED, IMG_FALSE);

	GLES1_TIME_STOP(GLES1_TIMES_glTexParameterx);
}



GL_API void GL_APIENTRY glTexParameteri(GLenum target, GLenum pname, GLint param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexParameteri"));

	GLES1_TIME_START(GLES1_TIMES_glTexParameteri);

	TexParameterfv(gc, target, pname, (void *)&param, GLES1_INT32, IMG_FALSE);

	GLES1_TIME_STOP(GLES1_TIMES_glTexParameteri);
}

#if defined(PROFILE_COMMON)
GL_API void GL_APIENTRY glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexParameterfv"));

	GLES1_TIME_START(GLES1_TIMES_glTexParameterfv);

	TexParameterfv(gc, target, pname, (const void *)params, GLES1_FLOAT, IMG_TRUE);

	GLES1_TIME_STOP(GLES1_TIMES_glTexParameterfv);
}
#endif /* PROFILE_COMMON */

GL_API void GL_APIENTRY glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexParameteriv"));

	GLES1_TIME_START(GLES1_TIMES_glTexParameteriv);

	TexParameterfv(gc, target, pname, (const void *)params, GLES1_INT32, IMG_TRUE);

	GLES1_TIME_STOP(GLES1_TIMES_glTexParameteriv);
}


GL_API void GL_APIENTRY glTexParameterxv(GLenum target, GLenum pname, const GLfixed *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexParameterxv"));

	GLES1_TIME_START(GLES1_TIMES_glTexParameterxv);

	TexParameterfv(gc, target, pname, (const void *)params, GLES1_FIXED, IMG_TRUE);

	GLES1_TIME_STOP(GLES1_TIMES_glTexParameterxv);
}


#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
/***********************************************************************************
 Function Name      : TexGenfv
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
static IMG_VOID TexGenfv(GLES1Context *gc, GLenum coord, GLenum pname, GLenum param)
{
	switch(pname) 
	{
		case GL_TEXTURE_GEN_MODE_OES:
		{
			switch(param) 
			{
#if defined(GLES1_EXTENSION_TEX_CUBE_MAP)
				case GL_NORMAL_MAP_OES:
				case GL_REFLECTION_MAP_OES:
				{
					if(coord != GL_TEXTURE_GEN_STR_OES)
					{
						SetError(gc, GL_INVALID_ENUM);
						return;
					}

					gc->sState.sTexture.psActive->eTexGenMode = param;

					break;
				}
#endif
				default:
				{
					SetError(gc, GL_INVALID_ENUM);
					return;
				}
			}
			break;
		}
		default:
		{
			SetError(gc, GL_INVALID_ENUM);
			return;
		}
	}
}

/***********************************************************************************
 Function Name      : glTexGeniOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glTexGeniOES(GLenum coord, GLenum pname, GLint param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexGeniOES"));

	GLES1_TIME_START(GLES1_TIMES_glTexGeniOES);

	TexGenfv(gc, coord, pname, (GLenum)param);

	GLES1_TIME_STOP(GLES1_TIMES_glTexGeniOES);
}


/***********************************************************************************
 Function Name      : glTexGenivOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glTexGenivOES(GLenum coord, GLenum pname, const GLint *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexGenivOES"));

	GLES1_TIME_START(GLES1_TIMES_glTexGenivOES);

	TexGenfv(gc, coord, pname, (GLenum)params[0]);


	GLES1_TIME_STOP(GLES1_TIMES_glTexGenivOES);
}


#if defined(PROFILE_COMMON)
/***********************************************************************************
 Function Name      : glTexGenfOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glTexGenfOES(GLenum coord, GLenum pname, GLfloat param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexGenfOES"));

	GLES1_TIME_START(GLES1_TIMES_glTexGenfOES);

	TexGenfv(gc, coord, pname, (GLenum)param);



	GLES1_TIME_STOP(GLES1_TIMES_glTexGenfOES);
}	


/***********************************************************************************
 Function Name      : glTexGenfvOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glTexGenfvOES(GLenum coord, GLenum pname, const GLfloat *params)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexGenfvOES"));

	GLES1_TIME_START(GLES1_TIMES_glTexGenfvOES);

	TexGenfv(gc, coord, pname, (GLenum)params[0]);



	GLES1_TIME_STOP(GLES1_TIMES_glTexGenfvOES);
}
#endif /* defined(PROFILE_COMMON) */


/***********************************************************************************
 Function Name      : glTexGenxOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glTexGenxOES(GLenum coord, GLenum pname, GLfixed param)
{
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexGenxOES"));

	GLES1_TIME_START(GLES1_TIMES_glTexGenxOES);

	TexGenfv(gc, coord, pname, (GLenum)param);

	GLES1_TIME_STOP(GLES1_TIMES_glTexGenxOES);
}


/***********************************************************************************
 Function Name      : glTexGenxvOES
 Inputs             : 
 Outputs            : 
 Returns            : 
 Description        : 
************************************************************************************/
GL_API_EXT void GL_APIENTRY glTexGenxvOES(GLenum coord, GLenum pname, const GLfixed *params)
{
	GLenum param;
	__GLES1_GET_CONTEXT();

	PVR_DPF((PVR_DBG_CALLTRACE,"glTexGenxvOES"));

	GLES1_TIME_START(GLES1_TIMES_glTexGenxvOES);

	param = (GLenum)params[0];

	TexGenfv(gc, coord, pname, param);

	GLES1_TIME_STOP(GLES1_TIMES_glTexGenxvOES);
}
#endif /* defined(GLES1_EXTENSION_TEX_CUBE_MAP) */


/******************************************************************************
 End of file (tex.c)
******************************************************************************/

