/******************************************************************************
 * Name         : spanpack.c
 *
 * Copyright    : 2005-2008 by Imagination Technologies Limited.
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
 * $Log: spanpack.c $
 *****************************************************************************/

#include "context.h"
#include "spanpack.h"


/* In all cases, the function naming convention is that colour components are in order High -> Low,
 * eg. ARGB is the native HW rendering format 
 */


/* These are when native formats match */

/***********************************************************************************
 Function Name      : SpanPack16
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Copies a span of 16 bit pixels in native HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPack16(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT16 *pui16InData = (const IMG_UINT16 *) psSpanInfo->pvInData;
	IMG_UINT16 *pui16OutData = (IMG_UINT16 *) psSpanInfo->pvOutData;

	/* InData=OutData
	 */
	if(psSpanInfo->i32SrcGroupIncrement == (IMG_INT32)sizeof(IMG_UINT16))
	{
		GLES2MemCopy(psSpanInfo->pvOutData, psSpanInfo->pvInData, psSpanInfo->ui32Width * sizeof(IMG_UINT16));
	}
	else
	{
		i = psSpanInfo->ui32Width;

		do
		{
			*pui16OutData = *pui16InData;

			pui16OutData++;
			pui16InData = (const IMG_UINT16 *)((IMG_UINTPTR_T)((const IMG_UINT8 *)pui16InData + psSpanInfo->i32SrcGroupIncrement));
		}
		while(--i);
	}
}


/***********************************************************************************
 Function Name      : SpanPack32
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of 32 bit pixels in native HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPack32(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT32 *pui32InData = (const IMG_UINT32 *) psSpanInfo->pvInData;
	IMG_UINT32 *pui32OutData = (IMG_UINT32 *) psSpanInfo->pvOutData;


	/* InData=OutData
	 */
	if(psSpanInfo->i32SrcGroupIncrement == (IMG_INT32)sizeof(IMG_UINT32))
	{
		GLES2MemCopy(psSpanInfo->pvOutData, psSpanInfo->pvInData, psSpanInfo->ui32Width * sizeof(IMG_UINT32));
	}
	else
	{
		i = psSpanInfo->ui32Width;

		do
		{
			*pui32OutData = *pui32InData;

			pui32OutData++;

			pui32InData = (const IMG_UINT32 *)((IMG_UINTPTR_T)((const IMG_UINT8 *)pui32InData + psSpanInfo->i32SrcGroupIncrement));
		}
		while(--i);
	}
}


/* These are only for read format conversions to packed non-native HW formats */

/***********************************************************************************
 Function Name      : SpanPackARGB4444toRGBA4444
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of ARGB4444 pixels to RGBA4444 
					  (needed for read format conversion)
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB4444toRGBA4444(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT16 *pui16InData = (const IMG_UINT16 *) psSpanInfo->pvInData;
	IMG_UINT16 *pui16OutData = (IMG_UINT16 *) psSpanInfo->pvOutData;
	IMG_UINT16 ui16Invalue;

	/* InData
	 * 0xf000 A
	 * 0x0f00 R
	 * 0x00f0 G 
	 * 0x000f B
	 * 0xffff
	 *   R G B A   OutData
	 */
	i = psSpanInfo->ui32Width;

	do
	{
		ui16Invalue = *pui16InData;

		*pui16OutData++ = (IMG_UINT16)((ui16Invalue >> 12) | (ui16Invalue << 4));

		pui16InData = (const IMG_UINT16 *)((IMG_UINTPTR_T)((const IMG_UINT8 *)pui16InData + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB1555toRGBA5551
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of ARGB1555 pixels to RGBA5551
					  (needed for read format conversion)
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB1555toRGBA5551(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT16 *pui16InData = (const IMG_UINT16 *) psSpanInfo->pvInData;
	IMG_UINT16 *pui16OutData = (IMG_UINT16 *) psSpanInfo->pvOutData;
	IMG_UINT16 ui16Invalue;

	/* InData
	 * 0x8000 A
	 * 0x7C00 R
	 * 0x03E0 G 
	 * 0x001F B
	 *
	 *   R G B A   OutData
	 */
	i = psSpanInfo->ui32Width;

	do
	{
		ui16Invalue = *pui16InData;

		*pui16OutData++ = (IMG_UINT16)((ui16Invalue >> 15) | (ui16Invalue << 1)); 

		pui16InData = (const IMG_UINT16 *)((IMG_UINTPTR_T)((const IMG_UINT8 *)pui16InData + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}


/* ARGB8888 source */

/***********************************************************************************
 Function Name      : SpanPackARGB8888toABGR8888
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of ARGB8888 pixels into ABGR8888
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB8888toABGR8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT8 *pui8InData = (const IMG_UINT8 *) psSpanInfo->pvInData;
	IMG_UINT8 *pui8OutData = (IMG_UINT8 *) psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		/* Ouput red   channel */
		pui8OutData[0] = pui8InData[2];
		/* Ouput green channel */
		pui8OutData[1] = pui8InData[1];
		/* Ouput blue  channel */
		pui8OutData[2] = pui8InData[0];
		/* Ouput alpha channel */
		pui8OutData[3] = pui8InData[3];

		pui8OutData+= 4;
		pui8InData = &pui8InData[psSpanInfo->i32SrcGroupIncrement];
	}
	while(--i);
}


/***********************************************************************************
 Function Name      : SpanPackARGB8888toXBGR8888
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB8888 pixels to XBGR8888 format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB8888toXBGR8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT8 *pui8Src = (IMG_UINT8 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		pui8Dest[0] = pui8Src[2];
		pui8Dest[1] = pui8Src[1];
		pui8Dest[2] = pui8Src[0];
		pui8Dest[3] = 0xFF;

		pui8Src = pui8Src + psSpanInfo->i32SrcGroupIncrement;
		pui8Dest += 4;
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB8888toRGB565
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB8888 pixels to RGB565 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB8888toRGB565(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Temp;
	IMG_UINT32 i;
	IMG_UINT8 *pui8Src = (IMG_UINT8 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		/* Blue */
		ui8Temp = (pui8Src[0]) >> 3; 
		ui16OutData = (ui8Temp << 0);

		/* Green */
		ui8Temp = (pui8Src[1]) >> 2; 
		ui16OutData |= (ui8Temp << 5);

		/* Red */
		ui8Temp = (pui8Src[2]) >> 3; 
		ui16OutData |= (ui8Temp << 11);

		pui8Src = pui8Src + psSpanInfo->i32SrcGroupIncrement;
		*pui16Dest++ = ui16OutData; 
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB8888toARGB4444
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB8888 pixels to ARGB4444 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB8888toARGB4444(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Temp;
	IMG_UINT32 i;
	IMG_UINT8 *pui8Src = (IMG_UINT8 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		/* Blue */
		ui8Temp = (pui8Src[0]) >> 4; 
		ui16OutData = (ui8Temp << 0);

		/* Green */
		ui8Temp = (pui8Src[1]) >> 4; 
		ui16OutData |= (ui8Temp << 4);

		/* Red */
		ui8Temp = (pui8Src[2]) >> 4; 
		ui16OutData |= (ui8Temp << 8);

		/* Alpha */
		ui8Temp = (pui8Src[3]) >> 4; 
		ui16OutData |= (ui8Temp << 12);

		pui8Src = pui8Src + psSpanInfo->i32SrcGroupIncrement;
		*pui16Dest++ = ui16OutData; 
	}
	while(--i);
}


/***********************************************************************************
 Function Name      : SpanPackARGB8888toARGB1555
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB8888 pixels to ARGB1555 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB8888toARGB1555(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Temp;
	IMG_UINT32 i;
	IMG_UINT8 *pui8Src = (IMG_UINT8 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		/* Blue */
		ui8Temp = (*pui8Src++) >> 3; 
		ui16OutData = (ui8Temp << 0);

		/* Green */
		ui8Temp = (*pui8Src++) >> 3; 
		ui16OutData |= (ui8Temp << 5);

		/* Red */
		ui8Temp = (*pui8Src++) >> 3; 
		ui16OutData |= (ui8Temp  << 10);

		/* Alpha */
		ui8Temp = (*pui8Src++) >> 7; 
		ui16OutData |= (ui8Temp << 15);

		*pui16Dest++ = ui16OutData; 
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB8888toLuminanceAlpha
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB8888 pixels to LA HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB8888toLuminanceAlpha(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32Temp;
	IMG_UINT16 ui16OutData;
	IMG_UINT32 *pui32Src = (IMG_UINT32 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		ui32Temp = *pui32Src;

		ui16OutData = (IMG_UINT16)((ui32Temp & 0xFFFF0000) >> 16);

		*pui16Dest = ui16OutData;

		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui32Src + psSpanInfo->i32SrcGroupIncrement));
		pui16Dest++;

}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB8888toLuminance
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB8888 pixels to L8 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB8888toLuminance(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32Temp;
	IMG_UINT32 *pui32Src = (IMG_UINT32 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		ui32Temp = *pui32Src; 

		*pui8Dest = (IMG_UINT8)((ui32Temp >> 16) & 0xFF);

		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui32Src + psSpanInfo->i32SrcGroupIncrement));
		pui8Dest++;
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackAXXX8888toAlpha
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of AXXX8888 (ARGB/ABGR) pixels to Alpha format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackAXXX8888toAlpha(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32Temp;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;
	IMG_UINT32 *pui32Src = (IMG_UINT32 *)psSpanInfo->pvInData;


	i = psSpanInfo->ui32Width;

	do
	{
		ui32Temp = *pui32Src;
		*pui8Dest = (IMG_UINT8) ((ui32Temp >> 24) & 0xFF);

		pui8Dest++;
		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui32Src + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}

/* ABGR8888 source */

/***********************************************************************************
 Function Name      : SpanPackABGR8888toARGB8888
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ABGR8888 pixels to ARGB8888 format
************************************************************************************/

IMG_INTERNAL IMG_VOID SpanPackABGR8888toARGB8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT8 *pui8Src = (IMG_UINT8 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		pui8Dest[0] = pui8Src[2];
		pui8Dest[1] = pui8Src[1];
		pui8Dest[2] = pui8Src[0];
		pui8Dest[3] = pui8Src[3];

		pui8Src = pui8Src + psSpanInfo->i32SrcGroupIncrement;
		pui8Dest += 4;
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackABGR8888toXBGR8888
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ABGR8888 pixels to XBGR8888 format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackABGR8888toXBGR8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT8 *pui8Src = (IMG_UINT8 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		pui8Dest[0] = pui8Src[0];
		pui8Dest[1] = pui8Src[1];
		pui8Dest[2] = pui8Src[2];
		pui8Dest[3] = 0xFF;

		pui8Src = pui8Src + psSpanInfo->i32SrcGroupIncrement;
		pui8Dest += 4;
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackABGR8888toRGB565
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ABGR8888 pixels to RGB565
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackABGR8888toRGB565(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Temp;
	IMG_UINT32 i;
	IMG_UINT8 *pui8Src = (IMG_UINT8 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		/* Red */
		ui8Temp = (pui8Src[0]) >> 3; 
		ui16OutData = ui8Temp  << 11;

		/* Green */
		ui8Temp = (pui8Src[1]) >> 2; 
		ui16OutData |= ui8Temp << 5;

		/* Blue */
		ui8Temp = (pui8Src[2]) >> 3; 
		ui16OutData |= ui8Temp << 0;

		pui8Src = pui8Src + psSpanInfo->i32SrcGroupIncrement;
		*pui16Dest++ = ui16OutData; 
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackABGR8888toARGB4444
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ABGR8888 pixels ARGB4444 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackABGR8888toARGB4444(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Temp;
	IMG_UINT32 i;
	IMG_UINT8 *pui8Src = (IMG_UINT8 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		/* Red */
		ui8Temp = (pui8Src[0]) >> 4; 
		ui16OutData = (ui8Temp << 8);

		/* Green */
		ui8Temp = (pui8Src[1]) >> 4; 
		ui16OutData |= (ui8Temp << 4);

		/* Blue */
		ui8Temp = (pui8Src[2]) >> 4; 
		ui16OutData |= (ui8Temp << 0);

		/* Alpha */
		ui8Temp = (pui8Src[3]) >> 4; 
		ui16OutData |= (ui8Temp << 12);
		
		pui8Src = pui8Src + psSpanInfo->i32SrcGroupIncrement;
		*pui16Dest++ = ui16OutData; 
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackABGR8888toARGB1555
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ABGR8888 pixels to ARGB1555 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackABGR8888toARGB1555(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT16 ui16OutData;
	IMG_UINT8 ui8Temp;
	IMG_UINT32 i;
	IMG_UINT8 *pui8Src = (IMG_UINT8 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		/* Red */
		ui8Temp = (*pui8Src++) >> 3; 
		ui16OutData = (ui8Temp  << 10);

		/* Green */
		ui8Temp = (*pui8Src++) >> 3; 
		ui16OutData |= (ui8Temp << 5);

		/* Blue */
		ui8Temp = (*pui8Src++) >> 3; 
		ui16OutData |= (ui8Temp << 0);

		/* Alpha */
		ui8Temp = (*pui8Src++) >> 7; 
		ui16OutData |= (ui8Temp << 15);

		*pui16Dest++ = ui16OutData; 
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackABGR8888toLuminanceAlpha
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ABGR8888 pixels to LA HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackABGR8888toLuminanceAlpha(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32Temp;
	IMG_UINT16 ui16OutData;
	IMG_UINT32 *pui32Src = (IMG_UINT32 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		ui32Temp = *pui32Src; 

		ui16OutData = (IMG_UINT16)(((ui32Temp & 0xFF000000) >> 16) | (ui32Temp & 0xFF));

		*pui16Dest = ui16OutData; 

		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui32Src + psSpanInfo->i32SrcGroupIncrement));
		pui16Dest++;

	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackABGR8888toLuminance
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ABGR8888 pixels to Luminance HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackABGR8888toLuminance(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT32 ui32Temp;
	IMG_UINT32 *pui32Src = (IMG_UINT32 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		ui32Temp = *pui32Src; 

		*pui8Dest = (IMG_UINT8)(ui32Temp & 0xFF);

		pui32Src = (IMG_UINT32 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui32Src + psSpanInfo->i32SrcGroupIncrement));
		pui8Dest++;
	}
	while(--i);
}

/* ARGB4444 source */

/***********************************************************************************
 Function Name      : SpanPackARGB4444toABGR8888
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of ARGB4444 pixels to ABGR8888
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB4444toABGR8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT16 *pui16InData = (const IMG_UINT16 *) psSpanInfo->pvInData;
	IMG_UINT8 *pui8OutData = (IMG_UINT8 *) psSpanInfo->pvOutData;
	IMG_UINT16 ui16Invalue;
	IMG_UINT8 ui8Temp;

	i = psSpanInfo->ui32Width;


	/* InData
	 * 0xf000 A
	 * 0x0f00 R
	 * 0x00f0 G 
	 * 0x000f B
	 */
	do
	{
		ui16Invalue = *pui16InData;

		ui8Temp = (IMG_UINT8)((ui16Invalue & 0x0F00) >> 8);

		pui8OutData[0] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		ui8Temp = (IMG_UINT8)((ui16Invalue & 0x00F0) >> 4);

		pui8OutData[1] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		ui8Temp = (IMG_UINT8)(ui16Invalue & 0x000F);

		pui8OutData[2] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		ui8Temp = (IMG_UINT8)((ui16Invalue & 0xF000) >> 12);

		pui8OutData[3] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		pui8OutData+= 4;
		pui16InData = (const IMG_UINT16 *)((IMG_UINTPTR_T)((const IMG_UINT8 *)pui16InData + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB4444toXBGR8888
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of ARGB4444 pixels to XBGR8888
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB4444toXBGR8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT16 *pui16InData = (const IMG_UINT16 *) psSpanInfo->pvInData;
	IMG_UINT8 *pui8OutData = (IMG_UINT8 *) psSpanInfo->pvOutData;
	IMG_UINT16 ui16Invalue;
	IMG_UINT8 ui8Temp;

	i = psSpanInfo->ui32Width;


	/* InData
	 * 0xf000 A
	 * 0x0f00 R
	 * 0x00f0 G 
	 * 0x000f B
	 */
	do
	{
		ui16Invalue = *pui16InData;

		ui8Temp = (IMG_UINT8)((ui16Invalue & 0x0F00) >> 8);

		pui8OutData[0] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		ui8Temp = (IMG_UINT8)((ui16Invalue & 0x00F0) >> 4);

		pui8OutData[1] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		ui8Temp = (IMG_UINT8)(ui16Invalue & 0x000F);

		pui8OutData[2] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		pui8OutData[3] = 0xFF;

		pui8OutData+= 4;
		pui16InData = (const IMG_UINT16 *)((IMG_UINTPTR_T)((const IMG_UINT8 *)pui16InData + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB4444toARGB8888
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of ARGB4444 pixels to ARGB8888
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB4444toARGB8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT16 *pui16InData = (const IMG_UINT16 *) psSpanInfo->pvInData;
	IMG_UINT8 *pui8OutData = (IMG_UINT8 *) psSpanInfo->pvOutData;
	IMG_UINT16 ui16Invalue;
	IMG_UINT8 ui8Temp;

	i = psSpanInfo->ui32Width;


	/* InData
	 * 0xf000 A
	 * 0x0f00 R
	 * 0x00f0 G 
	 * 0x000f B
	 */
	do
	{
		ui16Invalue = *pui16InData;

		/* Blue */
		ui8Temp = (IMG_UINT8)(ui16Invalue & 0x000F);

		pui8OutData[0] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		/* Green */
		ui8Temp = (IMG_UINT8)((ui16Invalue & 0x00F0) >> 4);

		pui8OutData[1] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		/* Red */
		ui8Temp = (IMG_UINT8)((ui16Invalue & 0x0F00) >> 8);

		pui8OutData[2] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		/* Alpha */
		ui8Temp = (IMG_UINT8)((ui16Invalue & 0xF000) >> 12);

		pui8OutData[3] = (IMG_UINT8)(ui8Temp | (ui8Temp << 4));

		pui8OutData+= 4;
		pui16InData = (const IMG_UINT16 *)((IMG_UINTPTR_T)((const IMG_UINT8 *)pui16InData + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB4444toRGB565
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB4444 pixels to RGB565 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB4444toRGB565(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16Temp;
	IMG_UINT8 ui8Red;
	IMG_UINT8 ui8Green;
	IMG_UINT8 ui8Blue;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		ui16Temp = (IMG_UINT16)(*pui16Src & 0x0FFF);

		ui8Red = (IMG_UINT8)((ui16Temp & 0x0F00) >> 7);

		ui8Red |= ((ui8Red & 0x10) >> 4);

		ui8Green = (IMG_UINT8)((ui16Temp & 0x00F0) >> 2);

		ui8Green |= ((ui8Green & 0x30) >> 4);

		ui8Blue = (IMG_UINT8)((ui16Temp & 0x000F) << 1);

		ui8Blue |= ((ui8Blue & 0x10) >> 4);

		*pui16Dest = (IMG_UINT16)((ui8Red << 11) | (ui8Green << 5) | ui8Blue);

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
		pui16Dest++;
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB4444toARGB1555
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB4444 pixels to ARGB1555 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB4444toARGB1555(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;
	IMG_UINT16 ui16OutData, ui16InData;
	IMG_UINT8 ui8Red, ui8Green, ui8Blue;

	i = psSpanInfo->ui32Width;

	do
	{
		ui16InData = *pui16Src;

		/* Alpha */
		ui16OutData = (ui16InData & 0xF000) ? 0x8000U : 0;

		ui8Red = (IMG_UINT8)((ui16InData & 0x0F00) >> 7);

		ui8Red |= ((ui8Red & 0x10) >> 4);

		ui8Green = (IMG_UINT8)((ui16InData & 0x00F0) >> 3);

		ui8Green |= ((ui8Green & 0x10) >> 4);

		ui8Blue = (IMG_UINT8)((ui16InData & 0x000F) << 1);

		ui8Blue |= ((ui8Blue & 0x10) >> 4);

		ui16OutData |= (IMG_UINT16)((ui8Red << 10) | (ui8Green << 5) | ui8Blue);

		*pui16Dest = ui16OutData;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
		pui16Dest++;
	}
	while(--i);
}


/***********************************************************************************
 Function Name      : SpanPackARGB4444toLuminanceAlpha
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB4444 pixels to LA UB HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB4444toLuminanceAlpha(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16Temp;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		ui16Temp = *pui16Src; 
		ui8OutData = (IMG_UINT8)((ui16Temp >> 8) & 0xF);
		ui8OutData |= (ui8OutData << 4);

		pui8Dest[0] = ui8OutData;

		ui8OutData = (IMG_UINT8)((ui16Temp >> 12) & 0xF);
		ui8OutData |= (ui8OutData << 4);

		pui8Dest[1] = ui8OutData;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
		pui8Dest+=2;

	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB4444toLuminance
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB4444 pixels to L8 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB4444toLuminance(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Temp;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		ui8Temp = (IMG_UINT8)((*pui16Src >> 8) & 0xF);
		ui8Temp |= (ui8Temp << 4);

		*pui8Dest = ui8Temp;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
		pui8Dest++;
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB4444toAlpha
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB4444 pixels to A UB HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB4444toAlpha(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Temp;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;


	i = psSpanInfo->ui32Width;

	do
	{
		ui8Temp = (IMG_UINT8)((*pui16Src & 0xF000) >> 8);
		ui8Temp |= (ui8Temp >> 4);

		*pui8Dest = ui8Temp;

		pui8Dest++;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}

/* ARGB1555 source */

/***********************************************************************************
 Function Name      : SpanPackARGB1555toABGR8888
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of ARGB1555 pixels to ABGR8888
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB1555toABGR8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT16 *pui16InData = (const IMG_UINT16 *) psSpanInfo->pvInData;
	IMG_UINT8 *pui8OutData = (IMG_UINT8 *) psSpanInfo->pvOutData;
	IMG_UINT16 ui16InValue;

	/* InData
	 * 0x8000 A
	 * 0x7C00 R
	 * 0x03E0 G 
	 * 0x001F B
	 */
	i = psSpanInfo->ui32Width;

	do 
	{
		ui16InValue = *pui16InData;

		/* The way we are doing the conversion is:
		      1- Put the 5 bits in the top bits of the output byte.
			  2- Fill the lower 3 bits of the output byte with its 3 most significant bits.
		 */

		/* Output the red   channel */
		pui8OutData[0] = (IMG_UINT8)((ui16InValue & 0x7C00) >> 7);
		pui8OutData[0] = pui8OutData[0] | (pui8OutData[0] >> 5);

		/* Output the green channel */
		pui8OutData[1] = (IMG_UINT8)((ui16InValue & 0x03E0) >> 2);
		pui8OutData[1] = pui8OutData[1] | (pui8OutData[1] >> 5);

		/* Output the blue  channel */
		pui8OutData[2] = (IMG_UINT8)((ui16InValue & 0x001F) << 3);
		pui8OutData[2] = pui8OutData[2] | (pui8OutData[2] >> 5);

		/* Output the alpha channel */
		pui8OutData[3] = (ui16InValue & 0x8000)? 0xFFU : 0x00;

		pui8OutData+= 4;
		pui16InData = (const IMG_UINT16 *)((IMG_UINTPTR_T)((const IMG_UINT8 *)pui16InData + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB1555toXBGR8888
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of ARGB1555 pixels to XBGR8888
************************************************************************************/

IMG_INTERNAL IMG_VOID SpanPackARGB1555toXBGR8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT16 *pui16InData = (const IMG_UINT16 *) psSpanInfo->pvInData;
	IMG_UINT8 *pui8OutData = (IMG_UINT8 *) psSpanInfo->pvOutData;
	IMG_UINT16 ui16InValue;

	/* InData
	 * 0x8000 A
	 * 0x7C00 R
	 * 0x03E0 G 
	 * 0x001F B
	 */
	i = psSpanInfo->ui32Width;

	do
	{
		ui16InValue = *pui16InData;

		/* The way we are doing the conversion is:
		      1- Put the 5 bits in the top bits of the output byte.
			  2- Fill the lower 3 bits of the output byte with its 3 most significant bits.
		 */

		/* Output the red   channel */
		pui8OutData[0] = (IMG_UINT8)((ui16InValue & 0x7C00) >> 7);
		pui8OutData[0] = pui8OutData[0] | (pui8OutData[0] >> 5);

		/* Output the green channel */
		pui8OutData[1] = (IMG_UINT8)((ui16InValue & 0x03E0) >> 2);
		pui8OutData[1] = pui8OutData[1] | (pui8OutData[1] >> 5);

		/* Output the blue  channel */
		pui8OutData[2] = (IMG_UINT8)((ui16InValue & 0x001F) << 3);
		pui8OutData[2] = pui8OutData[2] | (pui8OutData[2] >> 5);

		/* Output the alpha channel */
		pui8OutData[3] = 0xFF;

		pui8OutData += 4;
		pui16InData = (const IMG_UINT16 *)((IMG_UINTPTR_T)((const IMG_UINT8 *)pui16InData + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB1555toARGB8888
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of ARGB1555 pixels to ARGB8888
************************************************************************************/

IMG_INTERNAL IMG_VOID SpanPackARGB1555toARGB8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT16 *pui16InData = (const IMG_UINT16 *) psSpanInfo->pvInData;
	IMG_UINT8 *pui8OutData = (IMG_UINT8 *) psSpanInfo->pvOutData;
	IMG_UINT16 ui16InValue;

	/* InData
	 * 0x8000 A
	 * 0x7C00 R
	 * 0x03E0 G 
	 * 0x001F B
	 */
	i = psSpanInfo->ui32Width;

	do
	{
		ui16InValue = *pui16InData;

		/* The way we are doing the conversion is:
		      1- Put the 5 bits in the top bits of the output byte.
			  2- Fill the lower 3 bits of the output byte with its 3 most significant bits.
		 */

		/* Output the blue  channel */
		pui8OutData[0] = (IMG_UINT8)((ui16InValue & 0x001F) << 3);
		pui8OutData[0] = pui8OutData[2] | (pui8OutData[2] >> 5);

		/* Output the green channel */
		pui8OutData[1] = (IMG_UINT8)((ui16InValue & 0x03E0) >> 2);
		pui8OutData[1] = pui8OutData[1] | (pui8OutData[1] >> 5);

		/* Output the red   channel */
		pui8OutData[2] = (IMG_UINT8)((ui16InValue & 0x7C00) >> 7);
		pui8OutData[2] = pui8OutData[0] | (pui8OutData[0] >> 5);

		/* Output the alpha channel */
		pui8OutData[3] = (ui16InValue & 0x8000)? 0xFFU : 0x00;

		pui8OutData+=4;
		pui16InData = (const IMG_UINT16 *)((IMG_UINTPTR_T)((const IMG_UINT8 *)pui16InData + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB1555toRGB565
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB1555 pixels to RGB565 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB1555toRGB565(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16Temp;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		ui16Temp = (IMG_UINT16)(*pui16Src & 0x7FFF);

		/* Move RG up one bit and leave B in place */
		ui16Temp = ((ui16Temp & 0x7FE0) << 1) | (ui16Temp & 0x1F);

		/* Replicate top G bit to bottom of G */
		ui16Temp |= ((ui16Temp & 0x0400) >> 5);

		*pui16Dest = ui16Temp;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
		pui16Dest++;
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB1555toARGB4444
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB1555 pixels to ARGB4444 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB1555toARGB4444(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16InData, ui16OutData;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;
	IMG_UINT16 *pui16Dest = (IMG_UINT16 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		ui16InData = *pui16Src;

		/* alpha */
		ui16OutData = (ui16InData & 0x8000) ? 0xF000U : 0;

		/* red */
		ui16OutData |= (((ui16InData & 0x7C00) >> 11) << 8);

		/* green */
		ui16OutData |= (((ui16InData & 0x03E0) >> 6) << 4);

		/* blue */
		ui16OutData |= (((ui16InData & 0x0001F) >> 1) << 0);

		*pui16Dest = ui16OutData;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
		pui16Dest++;
	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB1555toLuminanceAlpha
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB1555 pixels to LA UB HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB1555toLuminanceAlpha(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16Temp;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		/* Use high bit replication */
		ui16Temp = *pui16Src; 
		ui8OutData = (IMG_UINT8)(((ui16Temp >> 10) & 0x1F) << 3);
		ui8OutData |= (ui8OutData >> 5);

		pui8Dest[0] = ui8OutData;

		if(ui16Temp & 0x8000)
			pui8Dest[1] = 0xFF;
		else
			pui8Dest[1] = 0;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
		pui8Dest+=2;

	}
	while(--i);
}

/***********************************************************************************
 Function Name      : SpanPackARGB1555toLuminance
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB1555 pixels to L8 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB1555toLuminance(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT8 ui8Temp;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		/* Use high bit replication */
		ui8Temp = (IMG_UINT8)(((*pui16Src >> 10) & 0x1F) << 3);
		ui8Temp |= (ui8Temp >> 5);

		*pui8Dest = ui8Temp;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
		pui8Dest++;
	}
	while(--i);
}


/***********************************************************************************
 Function Name      : SpanPackARGB1555toAlpha
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of ARGB1555 pixels to A UB HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackARGB1555toAlpha(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;


	i = psSpanInfo->ui32Width;

	do
	{
		if(*pui16Src & 0x8000)
		{
			*pui8Dest = 0xFF;
		}
		else
		{
			*pui8Dest = 0;
		}

		pui8Dest++;
		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}

/* RGB565 source */


/***********************************************************************************
 Function Name      : SpanPackRGB565toXBGR8888
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of RGB565 pixels to XBGR8888 HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackRGB565toXBGR8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT8 ui8OutData;
	IMG_UINT16 ui16Temp;
	IMG_UINT32 i;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;
	
	do
	{
		ui16Temp = *pui16Src;

		/* Red */
		ui8OutData  = (IMG_UINT8)((ui16Temp >> 11) << 3);
		ui8OutData |= ((ui8OutData & 0xE0U) >> 5);
		*pui8Dest++ = ui8OutData;

		/* Green */
		ui8OutData  = (IMG_UINT8)((ui16Temp >> 5) << 2);
		ui8OutData |= ((ui8OutData & 0xC0U) >> 6);
		*pui8Dest++ = ui8OutData;

		/* Blue */
		ui8OutData  = (IMG_UINT8)((ui16Temp >> 0) << 3);
		ui8OutData |= ((ui8OutData & 0xE0U) >> 5);
		*pui8Dest++ = ui8OutData;

		/* Alpha */
		*pui8Dest++ = 0xFF;

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
	}
	while(--i);
}




/***********************************************************************************
 Function Name      : SpanPackRGB565toLuminance
 Inputs             : psSpanInfo
 Outputs            : -
 Returns            : -
 Description        : Converts a span of RGB565 pixels to L UB HW format
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackRGB565toLuminance(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	IMG_UINT16 ui16Temp;
	IMG_UINT8 ui8OutData;
	IMG_UINT16 *pui16Src = (IMG_UINT16 *)psSpanInfo->pvInData;
	IMG_UINT8 *pui8Dest = (IMG_UINT8 *)psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		ui16Temp = *pui16Src;

		ui8OutData = (IMG_UINT8)((ui16Temp >> 8) & 0xF8);
		ui8OutData |= (ui8OutData >> 5);
		*pui8Dest = ui8OutData; 

		pui16Src = (IMG_UINT16 *)((IMG_UINTPTR_T)((IMG_UINT8 *)pui16Src + psSpanInfo->i32SrcGroupIncrement));
		pui8Dest++;
	}
	while(--i);
}

/* XBGR8888 source */

/***********************************************************************************
 Function Name      : SpanPackXBGR8888to1BGR8888
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of XBGR8888 pixels into ABGR8888 with A=1
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackXBGR8888to1BGR8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	const IMG_UINT32 *pui32InData = (const IMG_UINT32 *) psSpanInfo->pvInData;
	IMG_UINT32 *pui32OutData = (IMG_UINT32 *) psSpanInfo->pvOutData;

	IMG_INT32 i32Increment = psSpanInfo->i32SrcGroupIncrement / 4;
	IMG_UINT32 i = psSpanInfo->ui32Width;

	do
	{
		*pui32OutData = (*pui32InData & 0x00FFFFFF) | 0xFF000000;

		pui32OutData++;
		pui32InData = pui32InData + i32Increment;
	}
	while(--i);
}

/* XRGB8888 source */

/***********************************************************************************
 Function Name      : SpanPackXRGB8888to1BGR8888
 Inputs             : psSpanInfo
 Outputs            : psSpanInfo->pvOutData
 Returns            : -
 Description        : Converts a span of XRGB8888 pixels into ABGR8888 with A=1
************************************************************************************/
IMG_INTERNAL IMG_VOID SpanPackXRGB8888to1BGR8888(const GLES2PixelSpanInfo *psSpanInfo)
{
	IMG_UINT32 i;
	const IMG_UINT8 *pui8InData = (const IMG_UINT8 *) psSpanInfo->pvInData;
	IMG_UINT8 *pui8OutData = (IMG_UINT8 *) psSpanInfo->pvOutData;

	i = psSpanInfo->ui32Width;

	do
	{
		/* Ouput red   channel */
		pui8OutData[0] = pui8InData[2];
		/* Ouput green channel */
		pui8OutData[1] = pui8InData[1];
		/* Ouput blue  channel */
		pui8OutData[2] = pui8InData[0];
		/* Ouput alpha channel */
		pui8OutData[3] = 0xFF;

		pui8OutData+= 4;
		pui8InData = &pui8InData[psSpanInfo->i32SrcGroupIncrement];
	}
	while(--i);
}

/******************************************************************************
 End of file spanpack.c
******************************************************************************/
