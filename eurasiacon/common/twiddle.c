/******************************************************************************
 * Name         : twiddle.c
 * Author       : James McCarthy
 * Created      : 14/11/2006
 *
 * Copyright    : 2003-2008 by Imagination Technologies Limited.
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
 * $Log: twiddle.c $
 *****************************************************************************/

#include "imgextensions.h"
#include "img_defs.h"
#include "sgxdefs.h"
#include "twiddle.h"
#include "twidtabs.h"


#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define ISTRIDE				32
#define SMALL_TEXTURE_16BPP	32
#define SMALL_TEXTURE_32BPP	32



#if defined(SGX_FEATURE_HYBRID_TWIDDLING)

/**********************************************************************************
 ******************* Code for SGX_FEATURE_HYBRID_TWIDDLING ************************
 **********************************************************************************/

#define IDQW(ptr, offset, psrc, srcoff) \
		(((IMG_UINT32 *) ptr)[(4*(offset))]= ((IMG_UINT32 *)psrc)[(4*(srcoff))]);\
        (((IMG_UINT32 *) ptr)[(4*(offset)+1)]=((IMG_UINT32*)psrc)[(4*(srcoff)+1)]);\
		(((IMG_UINT32 *) ptr)[(4*(offset)+2)]=((IMG_UINT32*)psrc)[(4*(srcoff)+2)]); \
		(((IMG_UINT32 *) ptr)[(4*(offset)+3)]=((IMG_UINT32*)psrc)[(4*(srcoff)+3)])


/******************************************************************************
 * Function Name: TwiddleCoord   INTERNAL ONLY
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/

static IMG_UINT32 TwiddleCoord(IMG_UINT32 ui32Width, IMG_UINT32 ui32Height, IMG_UINT32 x, IMG_UINT32 y)
{

	IMG_UINT32 inter, i, b, ui32BitsWidth = 0, ui32BitsHeight = 0, j = 0;
	
	/*
		PVRTC textures arrive in twiddled format, so we need to know how
		to anti-twiddle them, hence the following...
		
		It's not fast, but it works.
	*/
	
	/* find out how many bits we need for each dimension... */
	for(i = 0; i < 32; i++)
	{
		b = 1 << i;
		if((ui32Width & b) != 0)
		{
			ui32BitsWidth = i;
		}
		if((ui32Height & b) != 0)
		{
			ui32BitsHeight = i;
		}
	}
	
	/* twiddle the address */
	inter = 0;
	for(i = 0; i < MAX(ui32BitsWidth, ui32BitsHeight); i++)
	{
		IMG_UINT32 p = 0;
		
		if(i < ui32BitsHeight)
		{
			inter |= (y & (1<<i)) << (j);
			p++;
		}
		if(i < ui32BitsWidth)
		{
			inter |= (x & (1<<i)) << (j+p);
			p++;
		}
		
		j += p-1;
	}
	
	/* Now send back the twiddled address... */
	return inter;
}



/******************************************************************************
 * Function Name: HybridTwiddleCoord   INTERNAL ONLY
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/

static IMG_UINT32 HybridTwiddleCoord(IMG_UINT32 x, IMG_UINT32 y)
{
    static IMG_UINT32 s_ui32TwiddleTable[EURASIA_TAG_NP2TWID_MAXTILEDIM * EURASIA_TAG_NP2TWID_MAXTILEDIM];
	static IMG_BOOL   s_bInitialised = IMG_FALSE;

	if (!s_bInitialised)
	{
		IMG_UINT32 ui32Result, bit, tx, ty;

		for (tx=0; tx < EURASIA_TAG_NP2TWID_MAXTILEDIM; tx++)
		{
			for (ty=0; ty < EURASIA_TAG_NP2TWID_MAXTILEDIM; ty++)		   
			{
				ui32Result = 0;

				for (bit = 0; bit < (EURASIA_TAG_NP2TWID_MAXTILEDIM_LOG2); bit++)
				{
				    ui32Result |= ( (tx & (1<<bit)) << (bit+1) );   /* = ((x & (1<<bit)) >> bit) << (2*bit + 1) */
					ui32Result |= ( (ty & (1<<bit)) << bit );       /* = ((y & (1<<bit)) >> bit) << (2*bit)     */
				}
				s_ui32TwiddleTable[tx + (ty * EURASIA_TAG_NP2TWID_MAXTILEDIM)] = ui32Result;
			}
		}
		s_bInitialised = IMG_TRUE;
	}

	return s_ui32TwiddleTable[x + (y * EURASIA_TAG_NP2TWID_MAXTILEDIM)];
}


/******************************************************************************
 * Function Name: HybridTwiddleBlockCoord   INTERNAL ONLY
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/
static IMG_UINT32 HybridTwiddleBlockCoord(IMG_UINT32 ui32BlockWidth, IMG_UINT32 ui32BlockHeight, IMG_UINT32 x, IMG_UINT32 y)
{
	static IMG_UINT32 s_ui32TwiddleTable[EURASIA_TAG_NP2TWID_MAXTILEDIM * EURASIA_TAG_NP2TWID_MAXTILEDIM];
	static IMG_UINT32 s_ui32BlockWidth = 0xFFFFFFFF, s_ui32BlockHeight = 0xFFFFFFFF;
	static IMG_BOOL   s_bInitialised = IMG_FALSE;
	
	if(!s_bInitialised || (s_ui32BlockWidth != ui32BlockWidth) || (s_ui32BlockHeight != ui32BlockHeight))
	{
		/*
			If we've not already done so then build the twiddling
			table. Note that the max tile size is 16*16 so we can
			easily work out all possibilities.
		*/
		IMG_UINT32 inter, i, j, b, tx, ty, ui32BitsWidth=0, ui32BitsHeight=0;
		
		/* find out how many bits we need for each dimension... */
		for(i = 0; i < 32; i++)
		{
			b = 1 << i;
			if((ui32BlockWidth & b) != 0)
			{
				ui32BitsWidth = i;
			}
			if((ui32BlockHeight & b) != 0)
			{
				ui32BitsHeight = i;
			}
		}
		
		/* calculate the twiddled addresses... */
		for(tx = 0; tx < ui32BlockWidth; tx++)
		{
			for(ty = 0; ty < ui32BlockHeight; ty++)
			{
				inter = 0, j = 0;
				
				for(i = 0; i < MAX(ui32BitsWidth, ui32BitsHeight); i++)
				{
					IMG_UINT32 p = 0;
					
					if(i < ui32BitsHeight)
					{
						inter |= (ty & (1<<i)) << j;
						p++;
					}
					if(i < ui32BitsWidth)
					{
						inter |= (tx & (1<<i)) << (j + p);
						p++;
					}
					
					j += p-1;
				}
				
				s_ui32TwiddleTable[tx + (ty * ui32BlockWidth)] = inter;
			}
		}
		
		s_bInitialised = IMG_TRUE;
		s_ui32BlockWidth = ui32BlockWidth;
		s_ui32BlockHeight = ui32BlockHeight;
	}
	
	/*
		Now send back the twiddled address...
	*/
	return s_ui32TwiddleTable[x + (y * ui32BlockWidth)];
}




/******************************************************************************
 * Function Name: GetTileSize
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/

IMG_INTERNAL IMG_UINT32 GetTileSize(IMG_UINT32 ui32Width, IMG_UINT32 ui32Height)
{
	IMG_UINT32 ui32TexSize;

	ui32TexSize = MIN(ui32Width, ui32Height);

	if(ui32TexSize >= 16)
	{
		return 16;
	}
	if(ui32TexSize < 8)
	{
		if (ui32TexSize >= 4)
		{
			return 4;
		}
		if (ui32TexSize == 1)
		{
			return 1;
		}
		return 2;	
	}
	return 8;
}


/******************************************************************************
 * Function Name: DeTwiddleAddress8bpp
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/

IMG_INTERNAL IMG_VOID DeTwiddleAddress8bpp ( IMG_VOID    *pvDestAddress, 
											const IMG_VOID *pvSrcPixels, 
											IMG_UINT32  ui32Width, 
											IMG_UINT32  ui32Height, 
											IMG_UINT32  ui32StrideIn)
{
	IMG_UINT8 *pui8Pixels8   = (IMG_UINT8 *)pvSrcPixels;
	IMG_UINT8 *pui8TileAddr8 = (IMG_UINT8 *)pvDestAddress;
	IMG_UINT8 *pui8Dest8     = (IMG_UINT8 *)pvDestAddress;
	IMG_BOOL   bPow2 = IMG_TRUE;
	IMG_UINT32 ui32TileSize     = GetTileSize(ui32Width, ui32Height);
	IMG_UINT32 ui32TileSizeSqrd = ui32TileSize * ui32TileSize;
	IMG_UINT32 ui32TileX, ui32TileY;
	IMG_UINT32 ui32TileCountX, ui32TileCountY;
	IMG_UINT32 ui32TexX, ui32TexY;
	IMG_UINT32 ui32TileOffset;
	IMG_UINT32 ui32TwidCoord;


	if( (ui32TileSize > 1) && ((ui32Width % ui32TileSize) || (ui32Height % ui32TileSize)) )
	{
		bPow2 = IMG_FALSE;
	}

	ui32TileSizeSqrd = ui32TileSize * ui32TileSize;

	ui32TileCountX = ( (ui32Width  + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;
	ui32TileCountY = ( (ui32Height + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;

	if(bPow2)
	{
	    for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				pui8TileAddr8 = &pui8Dest8[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];

				ui32TileOffset = ( ((ui32TileSize * ui32StrideIn) * ui32TileY) + (ui32TileSize * ui32TileX) );

				for( ui32TexX = 0; ui32TexX < ui32TileSize; ++ui32TexX )
				{
					for( ui32TexY = 0; ui32TexY < ui32TileSize; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui8TileAddr8[ ui32TwidCoord ] = pui8Pixels8[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32StrideIn)) ];
					}
				}
			}
		}
	}
	else
	{
		IMG_UINT32 ui32EndX, ui32EndY;

		for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				ui32EndX = ui32TileSize;
				ui32EndY = ui32TileSize;
		
				pui8TileAddr8 = &pui8Dest8[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];
				ui32TileOffset = ( ((ui32TileSize * ui32StrideIn) * ui32TileY) + (ui32TileSize * ui32TileX) );

				/* Adjust values for a partially filled tile */
				if( ((ui32TileX * ui32TileSize) + ui32TileSize) > ui32Width )
				{
					ui32EndX = ui32Width % ui32TileSize;
				}
				if( ((ui32TileY * ui32TileSize) + ui32TileSize) > ui32Height )
				{
					ui32EndY = ui32Height % ui32TileSize;
				}

				for( ui32TexX = 0; ui32TexX < ui32EndX; ++ui32TexX )
				{
					for( ui32TexY = 0 ; ui32TexY < ui32EndY; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui8TileAddr8[ ui32TwidCoord ] = pui8Pixels8[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32StrideIn)) ];
					}
				}
			}
		}
	}
}		


/******************************************************************************
 * Function Name: DeTwiddleAddress16bpp
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/

IMG_INTERNAL IMG_VOID DeTwiddleAddress16bpp( IMG_VOID    *pvDestAddress, 
											const IMG_VOID *pvSrcPixels, 
											IMG_UINT32  ui32Width, 
											IMG_UINT32  ui32Height, 
											IMG_UINT32  ui32StrideIn)
{
	IMG_UINT16 *pui16Pixels16   = (IMG_UINT16 *)pvSrcPixels;
	IMG_UINT16 *pui16TileAddr16 = (IMG_UINT16 *) pvDestAddress;
	IMG_UINT16 *pui16Dest16     = (IMG_UINT16 *) pvDestAddress;
	IMG_BOOL   bPow2 = IMG_TRUE;
	IMG_UINT32 ui32TileSize = GetTileSize(ui32Width, ui32Height);
	IMG_UINT32 ui32TileSizeSqrd = ui32TileSize * ui32TileSize;
	IMG_UINT32 ui32TileX, ui32TileY;
	IMG_UINT32 ui32TileCountX, ui32TileCountY;
	IMG_UINT32 ui32TexX, ui32TexY;
	IMG_UINT32 ui32TileOffset;
	IMG_UINT32 ui32TwidCoord;


	if( (ui32TileSize > 1) && ((ui32Width % ui32TileSize) || (ui32Height % ui32TileSize)) )
	{
		bPow2 = IMG_FALSE;
	}

	ui32TileSizeSqrd = ui32TileSize * ui32TileSize;

	ui32TileCountX = ( (ui32Width  + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;
	ui32TileCountY = ( (ui32Height + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;

	if(bPow2)
	{
	    for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				pui16TileAddr16 = &pui16Dest16[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];

				ui32TileOffset = ( ((ui32TileSize * ui32StrideIn) * ui32TileY) + (ui32TileSize * ui32TileX) );

				for( ui32TexX = 0; ui32TexX < ui32TileSize; ++ui32TexX )
				{
					for( ui32TexY = 0; ui32TexY < ui32TileSize; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui16TileAddr16[ ui32TwidCoord ] = pui16Pixels16[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32StrideIn)) ];
					}
				}
			}
		}
	}
	else
	{
		IMG_UINT32 ui32EndX, ui32EndY;

		for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				ui32EndX = ui32TileSize;
				ui32EndY = ui32TileSize;
		
				pui16TileAddr16 = &pui16Dest16[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];
				ui32TileOffset = ( ((ui32TileSize * ui32StrideIn) * ui32TileY) + (ui32TileSize * ui32TileX) );

				/* Adjust values for a partially filled tile */
				if( ((ui32TileX * ui32TileSize) + ui32TileSize) > ui32Width )
				{
					ui32EndX = ui32Width % ui32TileSize;
				}
				if( ((ui32TileY * ui32TileSize) + ui32TileSize) > ui32Height )
				{
					ui32EndY = ui32Height % ui32TileSize;
				}

				for( ui32TexX = 0; ui32TexX < ui32EndX; ++ui32TexX )
				{
					for( ui32TexY = 0 ; ui32TexY < ui32EndY; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui16TileAddr16[ ui32TwidCoord ] = pui16Pixels16[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32StrideIn)) ];
					}
				}
			}
		}
	}
}		


/******************************************************************************
 * Function Name: DeTwiddleAddress32bpp
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/

IMG_INTERNAL IMG_VOID DeTwiddleAddress32bpp( IMG_VOID    *pvDestAddress, 
											const IMG_VOID *pvSrcPixels, 
											IMG_UINT32  ui32Width, 
											IMG_UINT32  ui32Height, 
											IMG_UINT32  ui32StrideIn)
{
	IMG_UINT32 *pui32Pixels32   = (IMG_UINT32 *)pvSrcPixels;
	IMG_UINT32 *pui32TileAddr32 = (IMG_UINT32 *) pvDestAddress;
	IMG_UINT32 *pui32Dest32     = (IMG_UINT32 *) pvDestAddress;
	IMG_BOOL   bPow2 = IMG_TRUE;
	IMG_UINT32 ui32TileSize = GetTileSize(ui32Width, ui32Height);
	IMG_UINT32 ui32TileSizeSqrd;
	IMG_UINT32 ui32TileX, ui32TileY;
	IMG_UINT32 ui32TileCountX, ui32TileCountY;
	IMG_UINT32 ui32TexX, ui32TexY;
	IMG_UINT32 ui32TileOffset;
	IMG_UINT32 ui32TwidCoord;


	if( (ui32TileSize > 1) && ((ui32Width % ui32TileSize) || (ui32Height % ui32TileSize)) )
	{
		bPow2 = IMG_FALSE;
	}

	ui32TileSizeSqrd = ui32TileSize * ui32TileSize;

	ui32TileCountX = ( (ui32Width  + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;
	ui32TileCountY = ( (ui32Height + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;

	if(bPow2)
	{
	    for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				pui32TileAddr32 = &pui32Dest32[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];

				ui32TileOffset = ( ((ui32TileSize * ui32StrideIn) * ui32TileY) + (ui32TileSize * ui32TileX) );

				for( ui32TexX = 0; ui32TexX < ui32TileSize; ++ui32TexX )
				{
					for( ui32TexY = 0; ui32TexY < ui32TileSize; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);
					
						pui32TileAddr32[ ui32TwidCoord ] = pui32Pixels32[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32StrideIn)) ];
					}
				}
			}
		}
	}
	else
	{
		IMG_UINT32 ui32EndX, ui32EndY;

		for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				ui32EndX = ui32TileSize;
				ui32EndY = ui32TileSize;
		
				pui32TileAddr32 = &pui32Dest32[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];
				ui32TileOffset = ( ((ui32TileSize * ui32StrideIn) * ui32TileY) + (ui32TileSize * ui32TileX) );

				/* Adjust values for a partially filled tile */
				if( ((ui32TileX * ui32TileSize) + ui32TileSize) > ui32Width )
				{
					ui32EndX = ui32Width % ui32TileSize;
				}
				if( ((ui32TileY * ui32TileSize) + ui32TileSize) > ui32Height )
				{
					ui32EndY = ui32Height % ui32TileSize;
				}

				for( ui32TexX = 0; ui32TexX < ui32EndX; ++ui32TexX )
				{
					for( ui32TexY = 0 ; ui32TexY < ui32EndY; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui32TileAddr32[ ui32TwidCoord ] = pui32Pixels32[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32StrideIn)) ];
					}
				}
			}
		}
	}
}		


static IMG_UINT32 GetPVRTC2bppTileSize(IMG_INT32 nUnCompressedWidth, IMG_INT32 nUnCompressedHeight)
{
	IMG_UINT32 ui32TileSize = GetTileSize( nUnCompressedWidth, nUnCompressedHeight);
	return (ui32TileSize < 8) ? 8 : ui32TileSize;
}

/******************************************************************************
 * Function Name: DeTwiddleAddressPVRTC2
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/

IMG_INTERNAL IMG_VOID DeTwiddleAddressPVRTC2( IMG_VOID    *pvDestAddress, 
											const IMG_VOID *pvSrcPixels, 
											IMG_UINT32  ui32Width, 
											IMG_UINT32  ui32Height, 
											IMG_UINT32  ui32StrideIn)
{
    IMG_UINT64 *pui64PixIn  = (IMG_UINT64 *)pvSrcPixels;
    IMG_UINT64 *pui64PixOut = (IMG_UINT64 *)pvDestAddress;
	IMG_UINT64 *pui64TileAddr = (IMG_UINT64 *)pvDestAddress;
	IMG_UINT32  ui32TileSize = GetPVRTC2bppTileSize(ui32Width, ui32Height);
	IMG_UINT32  ui32TileSizeSqrd;
	IMG_UINT32  ui32TileX, ui32TileY;
	IMG_UINT32  ui32TileCountX, ui32TileCountY;
	IMG_UINT32  ui32TexX, ui32TexY;
	IMG_UINT32  ui32TileOffset;
	IMG_UINT32  ui32BlockCoord, ui32TwidCoord;
	IMG_UINT32 ui32EndX, ui32EndY;
	
	ui32TileSizeSqrd = ui32TileSize * ui32TileSize;
	
	ui32TileCountX = ( (ui32Width  + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;
	ui32TileCountY = ( (ui32Height + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;


	for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
	{
		for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
		{		
			ui32EndX = ui32TileSize;
			ui32EndY = ui32TileSize;

			pui64TileAddr = &pui64PixOut[ ((ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd) >> 5 ];

			ui32TileOffset = ( ((ui32TileSize * ui32StrideIn) * ui32TileY) + (ui32TileSize * ui32TileX) );

			/* Adjust values for a partially filled tile */
			if( ((ui32TileX * ui32TileSize) + ui32TileSize) > ui32Width )
			{
				ui32EndX = ui32Width % ui32TileSize;
			}
			if( ((ui32TileY * ui32TileSize) + ui32TileSize) > ui32Height )
			{
				ui32EndY = ui32Height % ui32TileSize;
			}

			for( ui32TexX = 0; ui32TexX < ui32EndX; ui32TexX += 8)
			{
				for( ui32TexY = 0 ; ui32TexY < ui32EndY; ui32TexY += 4)
				{
					ui32BlockCoord = TwiddleCoord( ui32Width >> 3, 
													ui32Height >> 2, 
													(ui32TexX + (ui32TileSize * ui32TileX)) >> 3,
													(ui32TexY + (ui32TileSize * ui32TileY)) >> 2 );

					ui32TwidCoord = HybridTwiddleBlockCoord( (ui32TileSize >> 3),
																(ui32TileSize >> 2),
																(ui32TexX >> 3),
																(ui32TexY >> 2) );
			
					pui64TileAddr[ui32TwidCoord] = pui64PixIn[ui32BlockCoord];
				}
			}
		}
	}
}


static IMG_UINT32 GetPVRTC4bppTileSize(IMG_INT32 nUnCompressedWidth, IMG_INT32 nUnCompressedHeight)
{
	IMG_UINT32 ui32TileSize = GetTileSize( nUnCompressedWidth, nUnCompressedHeight);
	return (ui32TileSize < 4) ? 4 : ui32TileSize;
}

/******************************************************************************
 * Function Name: DeTwiddleAddressPVRTC4
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DeTwiddleAddressPVRTC4( IMG_VOID    *pvDestAddress, 
												const IMG_VOID *pvSrcPixels, 
												IMG_UINT32  ui32Width, 
												IMG_UINT32  ui32Height, 
												IMG_UINT32  ui32StrideIn)
{
    IMG_UINT64 *pui64PixIn  = (IMG_UINT64 *)pvSrcPixels;
    IMG_UINT64 *pui64PixOut = (IMG_UINT64 *)pvDestAddress;
	IMG_UINT64 *pui64TileAddr = (IMG_UINT64 *)pvDestAddress;
	IMG_UINT32  ui32TileSize = GetPVRTC4bppTileSize(ui32Width, ui32Height);
	IMG_UINT32  ui32TileSizeSqrd;
	IMG_UINT32  ui32TileX, ui32TileY;
	IMG_UINT32  ui32TileCountX, ui32TileCountY;
	IMG_UINT32  ui32TexX, ui32TexY;
	IMG_UINT32  ui32TileOffset;
	IMG_UINT32  ui32BlockCoord, ui32TwidCoord;
	IMG_UINT32 ui32EndX, ui32EndY;


	ui32TileSizeSqrd = ui32TileSize * ui32TileSize;
	
	ui32TileCountX = ( (ui32Width  + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;
	ui32TileCountY = ( (ui32Height + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;


	for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
	{
		for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
		{		
			ui32EndX = ui32TileSize;
			ui32EndY = ui32TileSize;

			pui64TileAddr = &pui64PixOut[ ((ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd) >> 4 ];

			ui32TileOffset = ( ((ui32TileSize * ui32StrideIn) * ui32TileY) + (ui32TileSize * ui32TileX) );

			/* Adjust values for a partially filled tile */
			if( ((ui32TileX * ui32TileSize) + ui32TileSize) > ui32Width )
			{
				ui32EndX = ui32Width % ui32TileSize;
			}
			if( ((ui32TileY * ui32TileSize) + ui32TileSize) > ui32Height )
			{
				ui32EndY = ui32Height % ui32TileSize;
			}

			for( ui32TexX = 0; ui32TexX < ui32EndX; ui32TexX += 4)
			{
				for( ui32TexY = 0 ; ui32TexY < ui32EndY; ui32TexY += 4)
				{
					ui32BlockCoord = TwiddleCoord( ui32Width >> 2, 
													ui32Height >> 2, 
													(ui32TexX + (ui32TileSize * ui32TileX)) >> 2,
													(ui32TexY + (ui32TileSize * ui32TileY)) >> 2 );

					ui32TwidCoord = HybridTwiddleBlockCoord( (ui32TileSize >> 2),
																(ui32TileSize >> 2),
																(ui32TexX >> 2),
																(ui32TexY >> 2) );
			
					pui64TileAddr[ui32TwidCoord] = pui64PixIn[ui32BlockCoord];
				}
			}
		}
	}
}



/******************************************************************************
 * Function Name: DeTwiddleAddressETC1
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID DeTwiddleAddressETC1( IMG_VOID	*pvDestAddress,
											const IMG_VOID *pvSrcPixels,
											IMG_UINT32 ui32Width,
											IMG_UINT32 ui32Height,
											IMG_UINT32 ui32StrideIn)
{
	IMG_UINT64 *pui64Src = IMG_NULL;
	IMG_UINT64 *pui64Dest = IMG_NULL;
	IMG_UINT32 ui32TileSize;
	IMG_UINT32 ui32TileSizeSqrd;
	IMG_UINT32 ui32TileX, ui32TileY;
	IMG_UINT32 ui32TileCountX, ui32TileCountY;
	IMG_UINT32 ui32TexX, ui32TexY;

	ui32TileSize = MIN(4, GetTileSize( ui32Width, ui32Height));

	ui32TileCountX = (( ui32Width + (ui32TileSize-1)) & ~(ui32TileSize-1)) / ui32TileSize;
	ui32TileCountY = (( ui32Height + (ui32TileSize-1)) & ~(ui32TileSize-1)) / ui32TileSize;

	ui32TileSizeSqrd = ui32TileSize * ui32TileSize;

	pui64Src = ( IMG_UINT64 *)pvSrcPixels;
	pui64Dest = ( IMG_UINT64 *)pvDestAddress; 

	for( ui32TileX = 0; ui32TileX <  ui32TileCountX; ++ui32TileX)
	{
		for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY)
		{
			IMG_UINT32 ui32DestTileIndex = (ui32TileX + ui32TileY * ui32TileCountX) * ui32TileSizeSqrd;

			for( ui32TexX = 0 ; ui32TexX < ui32TileSize; ++ui32TexX) 
			{	
				IMG_UINT32 ui32SourceOffsetX = ui32TexX + ui32TileSize * ui32TileX;

				for(  ui32TexY = 0 ; ui32TexY < ui32TileSize; ++ui32TexY )
				{
					/* Each ui32TexX, ui32TexY pair references a 64-bit ETC block */

					IMG_UINT32 ui32SourceOffsetY = (ui32TexY + ui32TileSize * ui32TileY) * ui32StrideIn;
					
					pui64Dest[ ui32DestTileIndex + HybridTwiddleBlockCoord( ui32TileSize, ui32TileSize, ui32TexX, ui32TexY) ] =
						pui64Src[ ui32SourceOffsetX + ui32SourceOffsetY ];
				}
			}
		}
    }
}


/******************************************************************************
 * Function Name: ReadBackTwiddle8bpp
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ReadBackTwiddle8bpp ( IMG_VOID *pvDest, 
											const IMG_VOID *pvSrc, 
											IMG_UINT32 ui32Log2Width, 
											IMG_UINT32 ui32Log2Height, 
											IMG_UINT32 ui32Width, 
											IMG_UINT32 ui32Height, 
											IMG_UINT32 ui32DstStride)
{
	IMG_UINT8 *pui8TileAddr8 = (IMG_UINT8 *) pvSrc;
	IMG_UINT8 *pui8Dest8     = (IMG_UINT8 *) pvSrc;
	IMG_UINT8 *pui8Pixels8   = (IMG_UINT8 *) pvDest;
	IMG_BOOL   bPow2 = IMG_TRUE;
	IMG_UINT32 ui32TileSize = GetTileSize(ui32Width, ui32Height);
	IMG_UINT32 ui32TileSizeSqrd = ui32TileSize * ui32TileSize;
	IMG_UINT32 ui32TileX, ui32TileY;
	IMG_UINT32 ui32TileCountX, ui32TileCountY;
	IMG_UINT32 ui32TexX, ui32TexY;
	IMG_UINT32 ui32TileOffset;
	IMG_UINT32 ui32TwidCoord;

	PVR_UNREFERENCED_PARAMETER(ui32Log2Width);
	PVR_UNREFERENCED_PARAMETER(ui32Log2Height);

	if( (ui32TileSize > 1) && ((ui32Width % ui32TileSize) || (ui32Height % ui32TileSize)) )
	{
		bPow2 = IMG_FALSE;
	}

	ui32TileSizeSqrd = ui32TileSize * ui32TileSize;

	ui32TileCountX = ( (ui32Width  + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;
	ui32TileCountY = ( (ui32Height + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;

	if(bPow2)
	{
	    for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				pui8TileAddr8 = &pui8Dest8[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];

				ui32TileOffset = ( ((ui32TileSize * ui32DstStride) * ui32TileY) + (ui32TileSize * ui32TileX) );

				for( ui32TexX = 0; ui32TexX < ui32TileSize; ++ui32TexX )
				{
					for( ui32TexY = 0; ui32TexY < ui32TileSize; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui8Pixels8[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32DstStride)) ] = pui8TileAddr8[ ui32TwidCoord ];
					}
				}
			}
		}
	}
	else
	{
		IMG_UINT32 ui32EndX, ui32EndY;

		for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				ui32EndX = ui32TileSize;
				ui32EndY = ui32TileSize;
		
				pui8TileAddr8 = &pui8Dest8[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];
				ui32TileOffset = ( ((ui32TileSize * ui32DstStride) * ui32TileY) + (ui32TileSize * ui32TileX) );

				/* Adjust values for a partially filled tile */
				if( ((ui32TileX * ui32TileSize) + ui32TileSize) > ui32Width )
				{
					ui32EndX = ui32Width % ui32TileSize;
				}
				if( ((ui32TileY * ui32TileSize) + ui32TileSize) > ui32Height )
				{
					ui32EndY = ui32Height % ui32TileSize;
				}

				for( ui32TexX = 0; ui32TexX < ui32EndX; ++ui32TexX )
				{
					for( ui32TexY = 0 ; ui32TexY < ui32EndY; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui8Pixels8[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32DstStride)) ] = pui8TileAddr8[ ui32TwidCoord ];
					}
				}
			}
		}
	}
}


/******************************************************************************
 * Function Name: ReadBackTwiddle16bpp
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ReadBackTwiddle16bpp ( IMG_VOID *pvDest, 
											const IMG_VOID *pvSrc, 
											IMG_UINT32 ui32Log2Width, 
											IMG_UINT32 ui32Log2Height, 
											IMG_UINT32 ui32Width, 
											IMG_UINT32 ui32Height, 
											IMG_UINT32 ui32DstStride)
{
	IMG_UINT16 *pui16TileAddr16 = (IMG_UINT16 *) pvSrc;
	IMG_UINT16 *pui16Dest16     = (IMG_UINT16 *) pvSrc;
	IMG_UINT16 *pui16Pixels16   = (IMG_UINT16 *) pvDest;
	IMG_BOOL   bPow2 = IMG_TRUE;
	IMG_UINT32 ui32TileSize = GetTileSize(ui32Width, ui32Height);
	IMG_UINT32 ui32TileSizeSqrd = ui32TileSize * ui32TileSize;
	IMG_UINT32 ui32TileX, ui32TileY;
	IMG_UINT32 ui32TileCountX, ui32TileCountY;
	IMG_UINT32 ui32TexX, ui32TexY;
	IMG_UINT32 ui32TileOffset;
	IMG_UINT32 ui32TwidCoord;

	PVR_UNREFERENCED_PARAMETER(ui32Log2Width);
	PVR_UNREFERENCED_PARAMETER(ui32Log2Height);

	if( (ui32TileSize > 1) && ((ui32Width % ui32TileSize) || (ui32Height % ui32TileSize)) )
	{
		bPow2 = IMG_FALSE;
	}

	ui32TileSizeSqrd = ui32TileSize * ui32TileSize;

	ui32TileCountX = ( (ui32Width  + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;
	ui32TileCountY = ( (ui32Height + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;

	if(bPow2)
	{
	    for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				pui16TileAddr16 = &pui16Dest16[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];

				ui32TileOffset = ( ((ui32TileSize * ui32DstStride) * ui32TileY) + (ui32TileSize * ui32TileX) );

				for( ui32TexX = 0; ui32TexX < ui32TileSize; ++ui32TexX )
				{
					for( ui32TexY = 0; ui32TexY < ui32TileSize; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui16Pixels16[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32DstStride)) ] = pui16TileAddr16[ ui32TwidCoord ];
					}
				}
			}
		}
	}
	else
	{
		IMG_UINT32 ui32EndX, ui32EndY;

		for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				ui32EndX = ui32TileSize;
				ui32EndY = ui32TileSize;
		
				pui16TileAddr16 = &pui16Dest16[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];
				ui32TileOffset = ( ((ui32TileSize * ui32DstStride) * ui32TileY) + (ui32TileSize * ui32TileX) );

				/* Adjust values for a partially filled tile */
				if( ((ui32TileX * ui32TileSize) + ui32TileSize) > ui32Width )
				{
					ui32EndX = ui32Width % ui32TileSize;
				}
				if( ((ui32TileY * ui32TileSize) + ui32TileSize) > ui32Height )
				{
					ui32EndY = ui32Height % ui32TileSize;
				}

				for( ui32TexX = 0; ui32TexX < ui32EndX; ++ui32TexX )
				{
					for( ui32TexY = 0 ; ui32TexY < ui32EndY; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui16Pixels16[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32DstStride)) ] = pui16TileAddr16[ ui32TwidCoord ];
					}
				}
			}
		}
	}
}


/******************************************************************************
 * Function Name: ReadBackTwiddle32bpp
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ReadBackTwiddle32bpp ( IMG_VOID *pvDest, 
											const IMG_VOID *pvSrc, 
											IMG_UINT32 ui32Log2Width, 
											IMG_UINT32 ui32Log2Height, 
											IMG_UINT32 ui32Width, 
											IMG_UINT32 ui32Height, 
											IMG_UINT32 ui32DstStride)
{
	IMG_UINT32 *pui32TileAddr32 = (IMG_UINT32 *) pvSrc;
	IMG_UINT32 *pui32Dest32     = (IMG_UINT32 *) pvSrc;
	IMG_UINT32 *pui32Pixels32   = (IMG_UINT32 *) pvDest;
	IMG_BOOL   bPow2 = IMG_TRUE;
	IMG_UINT32 ui32TileSize = GetTileSize(ui32Width, ui32Height);
	IMG_UINT32 ui32TileSizeSqrd = ui32TileSize * ui32TileSize;
	IMG_UINT32 ui32TileX, ui32TileY;
	IMG_UINT32 ui32TileCountX, ui32TileCountY;
	IMG_UINT32 ui32TexX, ui32TexY;
	IMG_UINT32 ui32TileOffset;
	IMG_UINT32 ui32TwidCoord;

	PVR_UNREFERENCED_PARAMETER(ui32Log2Width);
	PVR_UNREFERENCED_PARAMETER(ui32Log2Height);

	if( (ui32TileSize > 1) && ((ui32Width % ui32TileSize) || (ui32Height % ui32TileSize)) )
	{
		bPow2 = IMG_FALSE;
	}

	ui32TileSizeSqrd = ui32TileSize * ui32TileSize;

	ui32TileCountX = ( (ui32Width  + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;
	ui32TileCountY = ( (ui32Height + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;

	if(bPow2)
	{
	    for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				pui32TileAddr32 = &pui32Dest32[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];

				ui32TileOffset = ( ((ui32TileSize * ui32DstStride) * ui32TileY) + (ui32TileSize * ui32TileX) );

				for( ui32TexX = 0; ui32TexX < ui32TileSize; ++ui32TexX )
				{
					for( ui32TexY = 0; ui32TexY < ui32TileSize; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui32Pixels32[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32DstStride)) ] = pui32TileAddr32[ ui32TwidCoord ];
					}
				}
			}
		}
	}
	else
	{
		IMG_UINT32 ui32EndX, ui32EndY;

		for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
		{
			for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
			{
				ui32EndX = ui32TileSize;
				ui32EndY = ui32TileSize;
		
				pui32TileAddr32 = &pui32Dest32[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];
				ui32TileOffset = ( ((ui32TileSize * ui32DstStride) * ui32TileY) + (ui32TileSize * ui32TileX) );

				/* Adjust values for a partially filled tile */
				if( ((ui32TileX * ui32TileSize) + ui32TileSize) > ui32Width )
				{
					ui32EndX = ui32Width % ui32TileSize;
				}
				if( ((ui32TileY * ui32TileSize) + ui32TileSize) > ui32Height )
				{
					ui32EndY = ui32Height % ui32TileSize;
				}

				for( ui32TexX = 0; ui32TexX < ui32EndX; ++ui32TexX )
				{
					for( ui32TexY = 0 ; ui32TexY < ui32EndY; ++ui32TexY )
					{
						ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

						pui32Pixels32[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32DstStride)) ] = pui32TileAddr32[ ui32TwidCoord ];
					}
				}
			}
		}
	}
}


/******************************************************************************
 * Function Name: ReadBackTwiddleETC1
 * Inputs       : 
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 * Description  : 
 *****************************************************************************/
IMG_INTERNAL IMG_VOID ReadBackTwiddleETC1( IMG_VOID *pvDest, 
											const IMG_VOID *pvSrc, 
											IMG_UINT32 ui32Log2Width, 
											IMG_UINT32 ui32Log2Height, 
											IMG_UINT32 ui32Width, 
											IMG_UINT32 ui32Height,
											IMG_UINT32 ui32DstStride)

{
	IMG_UINT64 *pui32TileAddr64 = (IMG_UINT64 *) pvSrc;
	IMG_UINT64 *pui32Dest64     = (IMG_UINT64 *) pvSrc;
	IMG_UINT64 *pui32Pixels64   = (IMG_UINT64 *) pvDest;
	IMG_UINT32 ui32TileSize = GetTileSize(ui32Width, ui32Height);
	IMG_UINT32 ui32TileSizeSqrd = ui32TileSize * ui32TileSize;
	IMG_UINT32 ui32TileX, ui32TileY;
	IMG_UINT32 ui32TileCountX, ui32TileCountY;
	IMG_UINT32 ui32TexX, ui32TexY;
	IMG_UINT32 ui32TileOffset;
	IMG_UINT32 ui32TwidCoord;
	IMG_UINT32 ui32EndX, ui32EndY;

	PVR_UNREFERENCED_PARAMETER(ui32Log2Width);
	PVR_UNREFERENCED_PARAMETER(ui32Log2Height);

	ui32TileSizeSqrd = ui32TileSize * ui32TileSize;

	ui32TileCountX = ( (ui32Width  + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;
	ui32TileCountY = ( (ui32Height + (ui32TileSize-1)) & ~(ui32TileSize-1) ) / ui32TileSize;

	for( ui32TileX = 0; ui32TileX < ui32TileCountX; ++ui32TileX )
	{
		for( ui32TileY = 0; ui32TileY < ui32TileCountY; ++ui32TileY )
		{
			ui32EndX = ui32TileSize;
			ui32EndY = ui32TileSize;
	
			pui32TileAddr64 = &pui32Dest64[(ui32TileX + (ui32TileY * ui32TileCountX)) * ui32TileSizeSqrd ];
			ui32TileOffset = ( ((ui32TileSize * ui32DstStride) * ui32TileY) + (ui32TileSize * ui32TileX) );

			/* Adjust values for a partially filled tile */
			if( ((ui32TileX * ui32TileSize) + ui32TileSize) > ui32Width )
			{
				ui32EndX = ui32Width % ui32TileSize;
			}
			if( ((ui32TileY * ui32TileSize) + ui32TileSize) > ui32Height )
			{
				ui32EndY = ui32Height % ui32TileSize;
			}

			for( ui32TexX = 0; ui32TexX < ui32EndX; ++ui32TexX )
			{
				for( ui32TexY = 0 ; ui32TexY < ui32EndY; ++ui32TexY )
				{
					ui32TwidCoord = HybridTwiddleCoord(ui32TexX, ui32TexY);

					pui32Pixels64[ ui32TileOffset + (ui32TexX + (ui32TexY * ui32DstStride)) ] = pui32TileAddr64[ ui32TwidCoord ];
				}
			}
		}
	}
}


#else /* defined(SGX_FEATURE_HYBRID_TWIDDLING) */

/**********************************************************************************
 ******************* Code for non SGX_FEATURE_HYBRID_TWIDDLING ********************
 **********************************************************************************/


#define IW(ptr,offset,val)		(((IMG_UINT32 *) ptr)[(offset)]=(val))

/*****************************************************************************/

/*  This writes sequentially and reads randomly calculating source with given stride*/
#define TWIDDLE_32_TEXELSWC_STRIDE(Src,Dst,Str)\
	Dst[0] = Src[0]; \
	Dst[1] = Src[Str]; \
	Dst[2] = Src[1]; \
	Dst[3] = Src[Str+1]; \
	Dst[4] = Src[2*Str]; \
	Dst[5] = Src[3*Str]; \
	Dst[6] = Src[(2*Str)+1]; \
	Dst[7] = Src[(3*Str)+1]; \
	Dst[8] = Src[2]; \
	Dst[9] = Src[Str+2]; \
	Dst[10] = Src[3]; \
	Dst[11] = Src[Str+3]; \
	Dst[12] = Src[(2*Str)+2]; \
	Dst[13] = Src[(3*Str)+2]; \
	Dst[14] = Src[(2*Str)+3]; \
	Dst[15] = Src[(3*Str)+3]; \
	Dst[16] = Src[(4*Str)]; \
	Dst[17] = Src[(5*Str)]; \
	Dst[18] = Src[(4*Str)+1]; \
	Dst[19] = Src[(5*Str)+1]; \
	Dst[20] = Src[(6*Str)]; \
	Dst[21] = Src[(7*Str)]; \
	Dst[22] = Src[(6*Str)+1]; \
	Dst[23] = Src[(7*Str)+1]; \
	Dst[24] = Src[(4*Str)+2]; \
	Dst[25] = Src[(5*Str)+2]; \
	Dst[26] = Src[(4*Str)+3]; \
	Dst[27] = Src[(5*Str)+3]; \
	Dst[28] = Src[(6*Str)+2]; \
	Dst[29] = Src[(7*Str)+2]; \
	Dst[30] = Src[(6*Str)+3]; \
	Dst[31] = Src[(7*Str)+3];

/* 
	As above, but this combines two reads into one write.
	May produce less and faster code on some processor architectures.
*/
#define TWIDDLE_32_TEXELSWC_16BPP_STRIDE_SHIFT(Src, Dst,Str, Shft) \
	Dst[0] = (((IMG_UINT32)Src[Str] << Shft) | Src[0]); \
	Dst[1] = (((IMG_UINT32)Src[Str+1] << Shft) | Src[1]); \
	Dst[2] = (((IMG_UINT32)Src[3*Str] << Shft) | Src[2*Str]); \
	Dst[3] = (((IMG_UINT32)Src[(3*Str)+1] << Shft) | Src[(2*Str)+1]); \
	Dst[4] = (((IMG_UINT32)Src[Str+2] << Shft) | Src[2]); \
	Dst[5] = (((IMG_UINT32)Src[Str+3] << Shft) | Src[3]); \
	Dst[6] = (((IMG_UINT32)Src[(3*Str)+2] << Shft) | Src[(2*Str)+2]); \
	Dst[7] = (((IMG_UINT32)Src[(3*Str)+3] << Shft) | Src[(2*Str)+3]); \
	Dst[8] = (((IMG_UINT32)Src[(5*Str)] << Shft) | Src[(4*Str)]); \
	Dst[9] = (((IMG_UINT32)Src[(5*Str)+1] << Shft) | Src[(4*Str)+1]); \
	Dst[10] = (((IMG_UINT32)Src[(7*Str)] << Shft) | Src[(6*Str)]); \
	Dst[11] = (((IMG_UINT32)Src[(7*Str)+1] << Shft) | Src[(6*Str)+1]); \
	Dst[12] = (((IMG_UINT32)Src[(5*Str)+2] << Shft) | Src[(4*Str)+2]); \
	Dst[13] = (((IMG_UINT32)Src[(5*Str)+3] << Shft) | Src[(4*Str)+3]); \
	Dst[14] = (((IMG_UINT32)Src[(7*Str)+2] << Shft) | Src[(6*Str)+2]); \
	Dst[15] = (((IMG_UINT32)Src[(7*Str)+3] << Shft) | Src[(6*Str)+3]); 


/******************************************************************************
 * Function Name: GetRectangularOffset   INTERNAL ONLY
 *****************************************************************************/
static IMG_VOID GetRectangularOffset(IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
									 IMG_UINT32 ui32Pitch, IMG_UINT32 *pui32Offset,
									 IMG_UINT32 *pui32Loop, IMG_UINT32 *pui32Square)
{
	/* get the offset for twiddle rectangular textures */
	if( ui32Width > ui32Height )
	{
		*pui32Square = ui32Height;
		*pui32Offset = ui32Height; 
		*pui32Loop = ui32Width / ui32Height; 
	}
	else
	{
		*pui32Square = ui32Width;
		*pui32Offset = ui32Width*ui32Pitch;
		*pui32Loop = ui32Height / ui32Width;
	}
} /* GetRectangularOffset */

/******************************************************************************
 * Function Name: InitLookupTable32   INTERNAL ONLY
 *
 * Inputs       : ui32Stride, IMG_UINT32 
 *
 * Outputs      : pui32LookupTable
 * Returns      : None
 * Globals Used : None
 *
 * Description  : Lookup table, containing pixel offsets for a 32x32 texture,
 *	 	  optimized for use on the host to exploit any WriteCombining by
 *		  performing 'random' reads and linear writes.
 *
 *****************************************************************************/
static 
void InitLookupTable32(IMG_UINT32 ui32Stride, IMG_UINT32 *pui32LookupTable)
{
	pui32LookupTable[0] =  0;
	pui32LookupTable[1] =  4;
	pui32LookupTable[2] =  8*ui32Stride;
	pui32LookupTable[3] =  8*ui32Stride + 4;
	pui32LookupTable[4] =  8;
	pui32LookupTable[5] =  12;
	pui32LookupTable[6] =  8*ui32Stride + 8;
	pui32LookupTable[7] =  8*ui32Stride + 12;
	pui32LookupTable[8] =  16*ui32Stride;
	pui32LookupTable[9] =  16*ui32Stride + 4;
	pui32LookupTable[10] = 24*ui32Stride;
	pui32LookupTable[11] = 24*ui32Stride + 4;
	pui32LookupTable[12] = 16*ui32Stride + 8;
	pui32LookupTable[13] = 16*ui32Stride + 12;
	pui32LookupTable[14] = 24*ui32Stride + 8;
	pui32LookupTable[15] = 24*ui32Stride + 12;
	pui32LookupTable[16] = 16;
	pui32LookupTable[17] = 20;
	pui32LookupTable[18] = 8*ui32Stride +  16;
	pui32LookupTable[19] = 8*ui32Stride +  20;
	pui32LookupTable[20] = 24;
	pui32LookupTable[21] = 28;
	pui32LookupTable[22] = 8*ui32Stride  + 24;
	pui32LookupTable[23] = 8*ui32Stride  + 28;
	pui32LookupTable[24] = 16*ui32Stride + 16;
	pui32LookupTable[25] = 16*ui32Stride + 20;
	pui32LookupTable[26] = 24*ui32Stride + 16;
	pui32LookupTable[27] = 24*ui32Stride + 20;
	pui32LookupTable[28] = 16*ui32Stride + 24;
	pui32LookupTable[29] = 16*ui32Stride + 28;
	pui32LookupTable[30] = 24*ui32Stride + 24;
	pui32LookupTable[31] = 24*ui32Stride + 28;
}



/******************************************************************************
 ********  FAST 16 BIT TWIDDLING CODE *****************************************
 *****************************************************************************/

/* Writes a IMG_UINT16 at the specified address */
#define IW16(ptr,offset,val)		(((IMG_UINT16 *) ptr)[(offset)]=(val))

/* Writes 16bpp pixel pairs to specified address */
#define PixelPair16bpp(pDst, pSrc, DstIndex, pSrc2) \
	IW (pDst, DstIndex, ( (*(pSrc)) | ((IMG_UINT32)(*(pSrc2))<<16) ))

/* Writes eight 16bpp pixel pairs to specified address */
#define EightPairs16bpp(pDst, pSrc, ui32Pitch2, pSrc2) \
	PixelPair16bpp(pDst, pSrc,          0, pSrc2);\
	PixelPair16bpp(pDst, pSrc+1,        1, pSrc2+1);\
	PixelPair16bpp(pDst, pSrc+ui32Pitch2,   2, pSrc2+ui32Pitch2);\
	PixelPair16bpp(pDst, pSrc+ui32Pitch2+1, 3, pSrc2+ui32Pitch2+1);\
	PixelPair16bpp(pDst, pSrc+2,        4, pSrc2+2);\
	PixelPair16bpp(pDst, pSrc+3,        5, pSrc2+3);\
	PixelPair16bpp(pDst, pSrc+ui32Pitch2+2, 6, pSrc2+ui32Pitch2+2);\
	PixelPair16bpp(pDst, pSrc+ui32Pitch2+3, 7, pSrc2+ui32Pitch2+3)


/******************************************************************************
 * Function Name: Write8x8Pixels16bpp   INTERNAL ONLY
 *
 * Inputs       : pui16Pixels, pPixels2, ui32Pitch, *pui32Address.
 *
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 *
 * Description  : Twiddle 8x8 16bit Pixels and write into texture memory. 
 *
 *****************************************************************************/
static IMG_VOID Write8x8Pixels16bpp(const IMG_UINT16 *pui16Pixels,
									IMG_UINT32 ui32Pitch2,
									IMG_UINT32 **ppui32Address,
									const IMG_UINT16 *pui16Pixels2)
{
	IMG_UINT32 ui32Pitch4 = ui32Pitch2 << 1;

	EightPairs16bpp((*ppui32Address),      pui16Pixels,				ui32Pitch2, pui16Pixels2);
	EightPairs16bpp(((*ppui32Address)+8),  pui16Pixels+ui32Pitch4,  ui32Pitch2, pui16Pixels2+ui32Pitch4);
	EightPairs16bpp(((*ppui32Address)+16), pui16Pixels+4,			ui32Pitch2, pui16Pixels2+4);
	EightPairs16bpp(((*ppui32Address)+24), pui16Pixels+ui32Pitch4+4, ui32Pitch2, pui16Pixels2+ui32Pitch4+4);
	(*ppui32Address) += 32;
}


/******************************************************************************
 * Function Name: TwiddleSmallTexture16bpp   INTERNAL ONLY
 *
 * Inputs       : *pPixels, ui32Pitch, *pui32Address, ui32Width.
 *
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 *
 * Description  : Twiddle the small (ui32Width<32) 16bit texture and write into texture memory.  
 *
 *****************************************************************************/
static IMG_VOID TwiddleSmallTexture16bpp(IMG_UINT32 *pui32Address,
										 IMG_UINT32 ui32Width,
										 IMG_UINT32 ui32Pitch,	/*  the Stride value of the source */
										 const IMG_UINT16 *pui16Pixels)
{
	IMG_UINT32 ui32Pitch2 = ui32Pitch << 1;

	switch(ui32Width)
	{
		case  1:
		{
			/* 1x1 */
			*((IMG_UINT16 *)pui32Address) = pui16Pixels[0];
			break;
		}
		case  2:
		{
			/* 2x2 */
			IW( pui32Address, 0, (pui16Pixels[0] | ((IMG_UINT32)pui16Pixels[ui32Pitch] << 16)) );
			IW( pui32Address, 1, (pui16Pixels[1] | ((IMG_UINT32)pui16Pixels[ui32Pitch+1] << 16)) );
			break;
		}
		case  4:
		{
			/* 4x4 */
			IMG_UINT32 ui32Pitch3 = ui32Pitch2 + ui32Pitch;

			IW( pui32Address, 0, (pui16Pixels[0] | ((IMG_UINT32)pui16Pixels[ui32Pitch] << 16)));
			IW( pui32Address, 1, (pui16Pixels[1] | ((IMG_UINT32)pui16Pixels[ui32Pitch+1] << 16)));
			IW( pui32Address, 2, (pui16Pixels[ui32Pitch2] | ((IMG_UINT32)pui16Pixels[ui32Pitch3] << 16)));
			IW( pui32Address, 3, (pui16Pixels[ui32Pitch2+1] | ((IMG_UINT32)pui16Pixels[ui32Pitch3+1] << 16)));
			IW( pui32Address, 4, (pui16Pixels[2] | ((IMG_UINT32)pui16Pixels[ui32Pitch+2] << 16)));
			IW( pui32Address, 5, (pui16Pixels[3] | ((IMG_UINT32)pui16Pixels[ui32Pitch+3] << 16)));
			IW( pui32Address, 6, (pui16Pixels[ui32Pitch2+2] | ((IMG_UINT32)pui16Pixels[ui32Pitch3+2] << 16)));
			IW( pui32Address, 7, (pui16Pixels[ui32Pitch2+3] | ((IMG_UINT32)pui16Pixels[ui32Pitch3+3] << 16)));
			break;
		}
		case  8:
		{
			/* 8x8 */
			Write8x8Pixels16bpp(pui16Pixels, ui32Pitch2, &pui32Address, pui16Pixels+ui32Pitch);
			break;
		}
		case  16:
		{
			/* 16x16 texture */
			IMG_UINT32 ui32Pitch8 = ui32Pitch << 3;

			Write8x8Pixels16bpp(pui16Pixels,			ui32Pitch2, &pui32Address, pui16Pixels+ui32Pitch);
			Write8x8Pixels16bpp(pui16Pixels+ui32Pitch8,	ui32Pitch2, &pui32Address, pui16Pixels+ui32Pitch8+ui32Pitch);
			Write8x8Pixels16bpp(pui16Pixels+8,			ui32Pitch2, &pui32Address, pui16Pixels+8+ui32Pitch);
			Write8x8Pixels16bpp(pui16Pixels+ui32Pitch8+8,ui32Pitch2, &pui32Address, pui16Pixels+ui32Pitch8+8+ui32Pitch);
			break;
		}
	}
} /* TwiddleSmallTexture16bpp */


/*****************************************************************************
 FUNCTION	: Twiddle32x3216bpp

 PURPOSE	: Twiddles all textures of sizes greater than and including 32 by
 			  32, as larger sizes can just repeat this case. 32x32 is taken as
 			  the base case for the recursion as a 32 by 32 texture (at 16bpp)
 			  exactly fills the 2k intermediary buffer. The method for filling
 			  the buffer is similar to the 'SmallTextureXbpp' case, except 
 			  that the loop always runs ISTRIDE^2 times. 
 			  The block copy is slightly different in that we have a 32 by 32
 			  square which is written out as a rectangle (except for the 
 			  32x32 case itself, which remains square). The destination 
 			  pointer is updated inside the structure as it is
 			  only ever incremented, the source	pointer however requires a 
 			  more 'random' pattern and is passed around an offset.

 PARAMETERS	: pwPixIn - pointer to the part of the source texture
			  ui32StrideIn - 	Stride of the Src
			  ui32StrideOut - Stride of the Dest
			  ui32TexWidth - Texture Width 
			  pdwDestAddress - pointer to the Dest

 RETURNS	: none
*****************************************************************************/
static IMG_VOID Twiddle32x3216bpp(const IMG_UINT16 * pui16PixIn,
								  IMG_UINT32 ui32StrideIn,
								  IMG_UINT32 ui32StrideOut,
								  IMG_UINT32 ui32TexWidth,
								  IMG_UINT32 ** ppui32DestAddress,
								  IMG_UINT32 *pui32LookupTable) 
{
	IMG_UINT32 i;
	IMG_UINT32 *pui32DestAddress; 
	IMG_UINT32 gdwPixelsWritten = 0;

	pui32DestAddress = *ppui32DestAddress;

	for (i=0;i<ISTRIDE;i++)
	{
		const IMG_UINT16 *pui16IPix;
		pui16IPix = (const IMG_UINT16 *)(pui32LookupTable[i] + pui16PixIn);

		TWIDDLE_32_TEXELSWC_16BPP_STRIDE_SHIFT(pui16IPix, pui32DestAddress, ui32StrideIn, 16);

		gdwPixelsWritten += ISTRIDE;
		pui32DestAddress += ISTRIDE/2;

		if (gdwPixelsWritten == ui32TexWidth)
		{
			pui32DestAddress += (ui32StrideOut - ui32TexWidth)/2;
			gdwPixelsWritten = 0;
		}
	}

	/* Update the pointer to the destination */
	*ppui32DestAddress = pui32DestAddress;
}

/*****************************************************************************
 FUNCTION	: RecursiveTwid16bpp

 PURPOSE	: Controls the recursive call, exactly as RecursiveTwid8bpp does.

 PARAMETERS	: ui32CurrentWidth - each recursive step halves the width
			  pwPixIn - pointer to the part of the source texture
			  ui32StrideIn - 	Stride of the Src
			  ui32StrideOut - Stride of the Dest
			  ui32TexWidth - Texture Width 
			  pdwDestAddress - pointer to the Dest

 RETURNS	: none
*****************************************************************************/
static IMG_VOID RecursiveTwid16bpp(IMG_UINT32	ui32CurrentWidth,
								   const IMG_UINT16	*pui16PixIn,
								   IMG_UINT32	ui32StrideIn,
								   IMG_UINT32	ui32StrideOut,
								   IMG_UINT32	ui32TexWidth,
								   IMG_UINT32	**ppui32DestAddress,
								   IMG_UINT32	*pui32LookupTable)
{

	if (ui32CurrentWidth == SMALL_TEXTURE_16BPP)
	{
		Twiddle32x3216bpp(pui16PixIn, ui32StrideIn, ui32StrideOut, ui32TexWidth, ppui32DestAddress, pui32LookupTable);		
	}
	else
	{
		IMG_UINT32 ui32Offset;

		ui32CurrentWidth = ui32CurrentWidth >> 1;

		/* Override QAC suggestion and use recursive call. */
		/* PRQA S 3670 ++ */
		/* Top left sub-square */
		RecursiveTwid16bpp(ui32CurrentWidth, pui16PixIn, ui32StrideIn, ui32StrideOut, ui32TexWidth, ppui32DestAddress, pui32LookupTable);

		/* Bottom left sub-square */
		ui32Offset = ui32CurrentWidth * ui32StrideIn;
		RecursiveTwid16bpp(ui32CurrentWidth, pui16PixIn + ui32Offset, ui32StrideIn, ui32StrideOut, ui32TexWidth, ppui32DestAddress, pui32LookupTable);	

		/* Top right sub-square */
		ui32Offset = ui32CurrentWidth;
		RecursiveTwid16bpp(ui32CurrentWidth, pui16PixIn + ui32Offset, ui32StrideIn, ui32StrideOut, ui32TexWidth, ppui32DestAddress, pui32LookupTable);

		/* Bottom right sub-square */
		ui32Offset = (ui32CurrentWidth * ui32StrideIn) + ui32CurrentWidth;
		RecursiveTwid16bpp(ui32CurrentWidth, pui16PixIn + ui32Offset, ui32StrideIn, ui32StrideOut, ui32TexWidth, ppui32DestAddress, pui32LookupTable);
		/* PRQA S 3670 -- */
	}
}	/* RecursiveTwid16bpp */

/******************************************************************************
 * Function Name: TwiddleRectangularTexture16bpp    INTERNAL ONLY
 *
 * Inputs       : pui32Address, ui32Width, ui32Height, ui32Pitch, *pPixels, 
 *
 * Outputs      : None
 *
 * Returns      : None
 *
 * Description  : Twiddle rectangular 16bit texture into the texture memory.
 *
 *****************************************************************************/
static IMG_VOID TwiddleRectangularTexture16bpp(IMG_UINT32 *pui32Address,
											   IMG_UINT32 ui32Width,
											   IMG_UINT32 ui32Height,
											   IMG_UINT32 ui32Pitch,	/*  the Stride value of the source */
											   const IMG_UINT16 *pui16Pixels,
											   IMG_UINT32* pui32LookupTable)
{

	const IMG_UINT16 *pui16Src;
	IMG_UINT32 ui32Offset, ui32Loop, ui32Square, i;

	IMG_UINT32 ui32Pitch2 = ui32Pitch << 1;
	IMG_UINT32 ui32Pitch8 = ui32Pitch << 3;

	/* get the offset */
	GetRectangularOffset(ui32Width, ui32Height, ui32Pitch, &ui32Offset, &ui32Loop, &ui32Square);

	if(ui32Square < SMALL_TEXTURE_16BPP) 
	{
		switch(ui32Square)
		{
			case  1:
			{
				/* 1x1 */
				IMG_UINT16 *pui16Address = (IMG_UINT16 *)pui32Address;
				pui16Src = pui16Pixels;

				for(i=0; i<ui32Loop; i++)
				{
					IW16( pui16Address, 0, pui16Src[0]);
					pui16Address++;
					pui16Src += ui32Offset;
				}
				break;
			}
			case  2:
			{
				/* 2x2 */
				pui16Src = pui16Pixels;

				for(i=0; i<ui32Loop; i++)
				{
					IW( pui32Address, 0, (pui16Src[0] | ((IMG_UINT32)pui16Src[ui32Pitch] << 16)) );
					IW( pui32Address, 1, (pui16Src[1] | ((IMG_UINT32)pui16Src[ui32Pitch+1] << 16)) );
					pui32Address += 2;
					pui16Src += ui32Offset;
				}
				break;
			}
			case  4:
			{
				/* 4x4 */
				IMG_UINT32 ui32Pitch3 = ui32Pitch2 + ui32Pitch;
				pui16Src = pui16Pixels;

				for(i=0; i<ui32Loop; i++)
				{
					IW( pui32Address, 0, (pui16Src[0] | ((IMG_UINT32)pui16Src[ui32Pitch] << 16)));
					IW( pui32Address, 1, (pui16Src[1] | ((IMG_UINT32)pui16Src[ui32Pitch+1] << 16)));
					IW( pui32Address, 2, (pui16Src[ui32Pitch2] | ((IMG_UINT32)pui16Src[ui32Pitch3] << 16)));
					IW( pui32Address, 3, (pui16Src[ui32Pitch2+1] | ((IMG_UINT32)pui16Src[ui32Pitch3+1] << 16)));
					IW( pui32Address, 4, (pui16Src[2] | ((IMG_UINT32)pui16Src[ui32Pitch+2] << 16)));
					IW( pui32Address, 5, (pui16Src[3] | ((IMG_UINT32)pui16Src[ui32Pitch+3] << 16)));
					IW( pui32Address, 6, (pui16Src[ui32Pitch2+2] | ((IMG_UINT32)pui16Src[ui32Pitch3+2] << 16)));
					IW( pui32Address, 7, (pui16Src[ui32Pitch2+3] | ((IMG_UINT32)pui16Src[ui32Pitch3+3] << 16)));
					pui32Address += 8;
					pui16Src += ui32Offset;
				}
				break;
			}
			case  8:	   /* 8x8 */
			{
				pui16Src = pui16Pixels;

				for(i=0; i<ui32Loop; i++)
				{
					Write8x8Pixels16bpp(pui16Src, ui32Pitch2, &pui32Address, pui16Src+ui32Pitch);
					pui16Src += ui32Offset;
				}
				break;
			}
			case  16:	/* 16x16 texture */
			{
				pui16Src = pui16Pixels;
	
				for(i=0; i<ui32Loop; i++)
				{
					Write8x8Pixels16bpp(pui16Src,          ui32Pitch2, &pui32Address, pui16Src+ui32Pitch);
					Write8x8Pixels16bpp(pui16Src+ui32Pitch8,   ui32Pitch2, &pui32Address, pui16Src+ui32Pitch8+ui32Pitch);
					Write8x8Pixels16bpp(pui16Src+8,        ui32Pitch2, &pui32Address, pui16Src+8+ui32Pitch);
					Write8x8Pixels16bpp(pui16Src+ui32Pitch8+8, ui32Pitch2, &pui32Address, pui16Src+ui32Pitch8+8+ui32Pitch);
					pui16Src += ui32Offset;
				}
				break;
			}
		}
	}
	else
	{
		pui16Src = pui16Pixels;
		InitLookupTable32(ui32Pitch, pui32LookupTable);

		for(i=0; i<ui32Loop; i++)
		{
			RecursiveTwid16bpp(ui32Square, pui16Src, ui32Pitch, ui32Square, ui32Square, &pui32Address, pui32LookupTable);
			pui16Src += ui32Offset;
		}
	}
} /* TwiddleRectangularTexture16bpp */



/******************************************************************************
 * Function Name: DeTwiddleAddress16bpp
 *
 * Inputs       : pDestAddress, pSrcPixels, ui32Width, ui32Height, ui32StrideIn.
 *
 * Outputs      : -
 * Returns      : -
 *
 * Description  : Twiddle the texture and write into texture memory. 
 *****************************************************************************/
IMG_VOID IMG_INTERNAL DeTwiddleAddress16bpp(IMG_VOID		*pvDestAddress,
												const IMG_VOID	*pvSrcPixels,
												IMG_UINT32  	ui32Width,
												IMG_UINT32  	ui32Height,
												IMG_UINT32  	ui32StrideIn)
{
	IMG_UINT32 *pui32DestAddress = (IMG_UINT32 *)pvDestAddress;
	const IMG_UINT16 *pui16Pixels = (const IMG_UINT16 *) pvSrcPixels;
	IMG_UINT32 ui32LookupTable[32];

	if(ui32Width == ui32Height)
	{
		/* normal squared texture */
		if(ui32Width < SMALL_TEXTURE_16BPP)
		{
			TwiddleSmallTexture16bpp(pui32DestAddress, ui32Width, ui32StrideIn, pui16Pixels);
		}
		else
		{
			InitLookupTable32(ui32StrideIn, ui32LookupTable);
			RecursiveTwid16bpp(ui32Width, pui16Pixels, ui32StrideIn, ui32Width, ui32Width, &pui32DestAddress, ui32LookupTable);
		}
	}
	else
	{
		/* non squared texture */
		TwiddleRectangularTexture16bpp(pui32DestAddress, ui32Width, ui32Height, ui32StrideIn, pui16Pixels, ui32LookupTable);
	}
} /* DeTwiddleAddress16bpp*/


/******************************************************************************
 * Function Name: Untwiddle16Bpp   INTERNAL ONLY
 *
 * Inputs       : ui32DestStrideInBytes, pui16Src, ui32DestX1, ui32DestY1, ui32DestX2, ui32DestY2 
 *
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 *
 * Description  : Untwiddle Translates (twiddled order src to scan dest)
 *
 *****************************************************************************/
static IMG_VOID	Untwiddle16Bpp(IMG_UINT16 *pui16Dest,
							   IMG_UINT32 ui32DestStrideInPixels,
							   const IMG_UINT16 *pui16Src,
							   IMG_UINT32 ui32DestX1,
							   IMG_UINT32 ui32DestY1,
							   IMG_UINT32 ui32DestX2,
							   IMG_UINT32 ui32DestY2)
{
	IMG_UINT32 ui32CountY, ui32CountX;
	IMG_UINT32 ui32DestScanInc = ui32DestStrideInPixels - ((ui32DestX2 - ui32DestX1));
	register IMG_UINT16 *pui16CurrentDest = pui16Dest;

	for (ui32CountY = ui32DestY1; ui32CountY < ui32DestY2; ui32CountY++)
	{
		for (ui32CountX = ui32DestX1; ui32CountX < ui32DestX2; ui32CountX++)
		{
			*pui16CurrentDest = pui16Src[GET_TWIDDLED_ADDRESS(ui32CountX,ui32CountY)];
			pui16CurrentDest++;
		}
		pui16CurrentDest += ui32DestScanInc;
	}
}

/******************************************************************************
 * Function Name: ReadBackTwiddle16bpp   INTERNAL ONLY
 *
 * Inputs       : ui32DestStrideInBytes, pui16Src, ui32DestX1, ui32DestY1, ui32DestX2, ui32DestY2 
 *
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 *
 * Description  : Untwiddles 16bpp texture using rectangular lookup table
 *
 *****************************************************************************/
IMG_VOID IMG_INTERNAL ReadBackTwiddle16bpp(IMG_VOID   *pvDest,
											   const IMG_VOID   *pvSrc,
											   IMG_UINT32  ui32Log2Width,
											   IMG_UINT32  ui32Log2Height,
											   IMG_UINT32  ui32Width,
											   IMG_UINT32  ui32Height,
											   IMG_UINT32  ui32DstStride)
{

	IMG_UINT16 *pui16Address = (IMG_UINT16 *)pvDest;
	const IMG_UINT16 *pui16Pixels = (const IMG_UINT16 *)pvSrc;

	IMG_UINT32 ui32SrcWidth = (1UL << ui32Log2Width);	
	IMG_UINT32 ui32SrcHeight = (1UL << ui32Log2Height);

	IMG_UINT32 ui32TwiddledSize = MIN(ui32SrcWidth,ui32SrcHeight);

	if (ui32SrcWidth > ui32SrcHeight)
	{
		IMG_INT32 i32CurrentX = 0;
		IMG_INT32 i32CurrentWidth = (IMG_INT32)ui32Width;

		while (i32CurrentWidth > 0)
		{
			IMG_INT32 i32BlockX = i32CurrentX;
			IMG_INT32 i32BlockWidth = i32CurrentWidth;

			if (i32BlockX < 0)
				i32BlockX = 0;

			if (i32BlockX < (IMG_INT32)ui32TwiddledSize)
			{
				if (i32BlockWidth > (IMG_INT32)ui32TwiddledSize)
					i32BlockWidth = (IMG_INT32)ui32TwiddledSize;

				Untwiddle16Bpp(pui16Address, ui32DstStride, pui16Pixels, (IMG_UINT32)i32BlockX, 
								0, (IMG_UINT32)(i32BlockX + i32BlockWidth), ui32Height);

				pui16Pixels += (ui32TwiddledSize * ui32TwiddledSize);
				pui16Address += (IMG_UINT32)i32BlockWidth;
			}
			i32CurrentX -= (IMG_INT32)ui32TwiddledSize;
			i32CurrentWidth -= (IMG_INT32)ui32TwiddledSize;
		}
	}
	else
	{
		IMG_INT32 i32CurrentY = 0;
		IMG_INT32 i32CurrentHeight = (IMG_INT32)ui32Height;

		while (i32CurrentHeight > 0)
		{
			IMG_INT32 i32BlockY = i32CurrentY;
			IMG_INT32 i32BlockHeight = i32CurrentHeight;

			if (i32BlockY < 0)
				i32BlockY = 0;

			if (i32BlockY < (IMG_INT32)ui32TwiddledSize)
			{
				if (i32BlockHeight > (IMG_INT32)ui32TwiddledSize)
					i32BlockHeight = (IMG_INT32)ui32TwiddledSize;

				Untwiddle16Bpp(pui16Address, ui32DstStride, pui16Pixels, 0, (IMG_UINT32)i32BlockY, 
								ui32Width, (IMG_UINT32)(i32BlockY + i32BlockHeight));

				pui16Pixels += (ui32TwiddledSize * ui32TwiddledSize);
				pui16Address += (IMG_UINT32)i32BlockHeight * ui32DstStride;
			}
			i32CurrentY -= (IMG_INT32)ui32TwiddledSize;
			i32CurrentHeight -= (IMG_INT32)ui32TwiddledSize;
		}
	}
}



/******************************************************************************
 ********  FAST 32 BIT TWIDDLING CODE *****************************************
 *****************************************************************************/
 
#define EightPairs32bpp(pui32Dst, pui32Src, i32Pitch2, pui32Src2) \
	(pui32Dst)[0]  = (pui32Src)[0];                               \
	(pui32Dst)[1]  = (pui32Src2)[0];                              \
	(pui32Dst)[2]  = (pui32Src)[1];                               \
	(pui32Dst)[3]  = (pui32Src2)[1];                              \
	(pui32Dst)[4]  = (pui32Src)[(i32Pitch2)];                     \
	(pui32Dst)[5]  = (pui32Src2)[(i32Pitch2)];                    \
	(pui32Dst)[6]  = (pui32Src)[(i32Pitch2)+1];                   \
	(pui32Dst)[7]  = (pui32Src2)[(i32Pitch2)+1];                  \
	(pui32Dst)[8]  = (pui32Src)[2];                               \
	(pui32Dst)[9]  = (pui32Src2)[2];                              \
	(pui32Dst)[10] = (pui32Src)[3];                               \
	(pui32Dst)[11] = (pui32Src2)[3];                              \
	(pui32Dst)[12] = (pui32Src)[(i32Pitch2)+2];                   \
	(pui32Dst)[13] = (pui32Src2)[(i32Pitch2)+2];                  \
	(pui32Dst)[14] = (pui32Src)[(i32Pitch2)+3];                   \
	(pui32Dst)[15] = (pui32Src2)[(i32Pitch2)+3];

/******************************************************************************
 * Function Name: Write8x8Pixels32bpp   INTERNAL ONLY
 *
 * Inputs       : pui32Pixels, pPixels2, ui32Pitch, *pui32Address.
 *				  
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 *
 * Description  : Twiddle 8x8 32bit Pixels and write into texture memory. 
 *
 *****************************************************************************/
static IMG_VOID Write8x8Pixels32bpp(const IMG_UINT32  *pui32Pixels,
									IMG_UINT32   ui32Pitch2,
									IMG_UINT32 **ppui32Address,
									const IMG_UINT32  *pui32Pixels2)
{
	IMG_UINT32 ui32Pitch4 = ui32Pitch2 << 1;

	EightPairs32bpp((*ppui32Address),      pui32Pixels,				 ui32Pitch2, pui32Pixels2);
	EightPairs32bpp(((*ppui32Address)+16), pui32Pixels+ui32Pitch4,   ui32Pitch2, pui32Pixels2+ui32Pitch4);
	EightPairs32bpp(((*ppui32Address)+32), pui32Pixels+4,		 	 ui32Pitch2, pui32Pixels2+4);
	EightPairs32bpp(((*ppui32Address)+48), pui32Pixels+ui32Pitch4+4, ui32Pitch2, pui32Pixels2+ui32Pitch4+4);
	(*ppui32Address) += 64;
}


/******************************************************************************
 * Function Name: TwiddleSmallTexture32bpp   INTERNAL ONLY
 *
 * Inputs       : *pPixels, ui32Pitch, *pui32Address, ui32Width.
 *
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 *
 * Description  : Twiddle the small (ui32Width<32) 32bit texture and write into texture memory.  
 *
 *****************************************************************************/
static IMG_VOID TwiddleSmallTexture32bpp(IMG_UINT32 *pui32Address,
										 IMG_UINT32  ui32Width,
										 IMG_UINT32  ui32Pitch,	/*  the Stride value of the source */
										 const IMG_UINT32 *pui32Pixels)
{
	IMG_UINT32 ui32Pitch2 = ui32Pitch << 1;

	switch(ui32Width)
	{
		case  1:
		{
			/* 1x1 */
			IW( pui32Address, 0, pui32Pixels[0]);
			break;
		}
		case  2:
		{
			/* 2x2 */
			IW( pui32Address, 0, pui32Pixels[0]);
			IW( pui32Address, 1, pui32Pixels[ui32Pitch]);
			IW( pui32Address, 2, pui32Pixels[1]);
			IW( pui32Address, 3, pui32Pixels[ui32Pitch+1]);
			break;
		}
		case  4:
		{
			/* 4x4 */
			IMG_UINT32 ui32Pitch3 = ui32Pitch2 + ui32Pitch;

			IW( pui32Address, 0,  pui32Pixels[0]);
			IW( pui32Address, 1,  pui32Pixels[ui32Pitch]);
			IW( pui32Address, 2,  pui32Pixels[1]);
			IW( pui32Address, 3,  pui32Pixels[ui32Pitch+1]);

			IW( pui32Address, 4,  pui32Pixels[ui32Pitch2]);
			IW( pui32Address, 5,  pui32Pixels[ui32Pitch3]);
			IW( pui32Address, 6,  pui32Pixels[ui32Pitch2+1]);
			IW( pui32Address, 7,  pui32Pixels[ui32Pitch3+1]);

			IW( pui32Address, 8,  pui32Pixels[2]);
			IW( pui32Address, 9,  pui32Pixels[ui32Pitch+2]);
			IW( pui32Address, 10, pui32Pixels[3]);
			IW( pui32Address, 11, pui32Pixels[ui32Pitch+3]);

			IW( pui32Address, 12, pui32Pixels[ui32Pitch2+2]);
			IW( pui32Address, 13, pui32Pixels[ui32Pitch3+2]);
			IW( pui32Address, 14, pui32Pixels[ui32Pitch2+3]);
			IW( pui32Address, 15, pui32Pixels[ui32Pitch3+3]);
			break;
		}
		case  8:
		{
			/* 8x8 */
			Write8x8Pixels32bpp(pui32Pixels, ui32Pitch2, &pui32Address, pui32Pixels+ui32Pitch);
			break;
		}
		case  16:
		{
			/* 16x16 texture */
			IMG_UINT32 ui32Pitch8 = ui32Pitch << 3;

			Write8x8Pixels32bpp(pui32Pixels,			 ui32Pitch2, &pui32Address, pui32Pixels+ui32Pitch);
			Write8x8Pixels32bpp(pui32Pixels+ui32Pitch8,	 ui32Pitch2, &pui32Address, pui32Pixels+ui32Pitch8+ui32Pitch);
			Write8x8Pixels32bpp(pui32Pixels+8,			 ui32Pitch2, &pui32Address, pui32Pixels+8+ui32Pitch);
			Write8x8Pixels32bpp(pui32Pixels+ui32Pitch8+8,ui32Pitch2, &pui32Address, pui32Pixels+ui32Pitch8+8+ui32Pitch);
			break;
		}
	}
} /* TwiddleSmallTexture32bpp */


/*****************************************************************************
 FUNCTION	: Twiddle32x3232bpp
    
 PURPOSE	: Twiddles all textures of sizes greater than and including 32 by
 			  32, as larger sizes can just repeat this case. 32x32 is taken as
 			  the base case for the recursion as a 32 by 32 texture (at 32bpp)
 			  exactly fills the 2k intermediary buffer. The method for filling
 			  the buffer is similar to the 'SmallTextureXbpp' case, except 
 			  that the loop always runs ISTRIDE^2 times. 

 PARAMETERS	: pwPixIn - pointer to the part of the source texture
			  ui32StrideIn - 	Stride of the Src
 			  ui32StrideOut - Stride of the Dest
 			  ui32TexWidth - Texture Width 
 			  pdwDestAddress - pointer to the Dest
 			  			  
 RETURNS	: none
*****************************************************************************/
static IMG_VOID Twiddle32x3232bpp(const IMG_UINT32 *pui32PixIn,
								  IMG_UINT32 ui32StrideIn,
								  IMG_UINT32 ui32StrideOut,
								  IMG_UINT32 ui32TexWidth,
								  IMG_UINT32 ** ppui32DestAddress,
								  IMG_UINT32 *pui32LookupTable) 
{
	IMG_UINT32 i;
	IMG_UINT32 *pui32PixOut; 
	IMG_UINT32 gdwPixelsWritten = 0;

	pui32PixOut = (IMG_UINT32 *) (*ppui32DestAddress);
			   
	for (i=0;i<ISTRIDE;i++)
	{
		const IMG_UINT32 *pui32IPix;
		pui32IPix = pui32LookupTable[i] + pui32PixIn;
		TWIDDLE_32_TEXELSWC_STRIDE(pui32IPix, pui32PixOut,ui32StrideIn);

		gdwPixelsWritten += ISTRIDE;
		pui32PixOut += ISTRIDE;

		if (gdwPixelsWritten == ui32TexWidth)
		{
			pui32PixOut += ui32StrideOut - ui32TexWidth;  /* move to the next row */
			gdwPixelsWritten = 0;
		}
	}
	
	/* Update the pointer to the destination */
	*ppui32DestAddress = pui32PixOut;
}

/*****************************************************************************
 FUNCTION	: RecursiveTwid32bpp
    
 PURPOSE	: Controls the recursive call, exactly as RecursiveTwid8bpp does.

 PARAMETERS	: ui32CurrentWidth - each recursive step halves the width
 			  pwPixIn - pointer to the part of the source texture
			  ui32StrideIn - 	Stride of the Src
 			  ui32StrideOut - Stride of the Dest
 			  ui32TexWidth - Texture Width 
 			  pdwDestAddress - pointer to the Dest
 			  			  
 RETURNS	: none
*****************************************************************************/
static IMG_VOID RecursiveTwid32bpp(IMG_UINT32	ui32CurrentWidth,
								   const IMG_UINT32	*pui32PixIn,
								   IMG_UINT32	ui32StrideIn,
								   IMG_UINT32	ui32StrideOut,
								   IMG_UINT32	ui32TexWidth,
								   IMG_UINT32	**ppui32DestAddress,
								   IMG_UINT32	*pui32LookupTable32)
{

	if (ui32CurrentWidth == SMALL_TEXTURE_32BPP)
	{
		Twiddle32x3232bpp(pui32PixIn, ui32StrideIn, ui32StrideOut, ui32TexWidth, ppui32DestAddress, pui32LookupTable32);
	}
	else
	{
		IMG_UINT32 ui32Offset;

		ui32CurrentWidth = ui32CurrentWidth >> 1;
		
		/* Override QAC suggestion and use recursive call. */
		/* PRQA S 3670 ++ */
		/* Top left sub-square */
		RecursiveTwid32bpp(ui32CurrentWidth, pui32PixIn, ui32StrideIn, ui32StrideOut, ui32TexWidth, ppui32DestAddress, pui32LookupTable32);

		/* Bottom left sub-square */
		ui32Offset = ui32CurrentWidth * ui32StrideIn;
		RecursiveTwid32bpp(ui32CurrentWidth, pui32PixIn + ui32Offset, ui32StrideIn, ui32StrideOut, ui32TexWidth, ppui32DestAddress, pui32LookupTable32);	

		/* Top right sub-square */
		ui32Offset = ui32CurrentWidth;
		RecursiveTwid32bpp(ui32CurrentWidth, pui32PixIn + ui32Offset, ui32StrideIn, ui32StrideOut, ui32TexWidth, ppui32DestAddress, pui32LookupTable32);

		/* Bottom right sub-square */
		ui32Offset = (ui32CurrentWidth * ui32StrideIn) + ui32CurrentWidth;
		RecursiveTwid32bpp(ui32CurrentWidth, pui32PixIn + ui32Offset, ui32StrideIn, ui32StrideOut, ui32TexWidth, ppui32DestAddress, pui32LookupTable32);
		/* PRQA S 3670 -- */
	}
}	/* RecursiveTwid32bpp */

/******************************************************************************
 * Function Name: TwiddleRectangularTexture32bpp    INTERNAL ONLY (TwiddleRectangularTexture32bpp)
 *
 * Inputs       : pui32Address, ui32Width, ui32Height, ui32Pitch, *pPixels, 
 *
 * Outputs      : None
 *
 * Returns      : None
 *
 * Description  : Twiddle rectangular 32bit texture into the texture memory.
 *				  
 *****************************************************************************/
static IMG_VOID TwiddleRectangularTexture32bpp(IMG_UINT32 *pui32Address,
											   IMG_UINT32  ui32Width,
											   IMG_UINT32  ui32Height,
											   IMG_UINT32  ui32Pitch,	/*  the Stride value of the source */
											   const IMG_UINT32 *pui32Pixels,
											   IMG_UINT32 *pui32LookupTable)
{

	const IMG_UINT32 *pui32Src;
	IMG_UINT32 ui32Offset, ui32Loop, ui32Square, i;

	IMG_UINT32 ui32Pitch2 = ui32Pitch << 1;
	IMG_UINT32 ui32Pitch8 = ui32Pitch << 3;

	/* get the offset */
	GetRectangularOffset(ui32Width, ui32Height, ui32Pitch, &ui32Offset, &ui32Loop, &ui32Square);

	if(ui32Square < SMALL_TEXTURE_32BPP) 
	{
		switch(ui32Square)
		{
			case  1:
			{
				/* 1x1 */
				pui32Src = pui32Pixels;

				for(i=0; i<ui32Loop; i++)
				{
					IW( pui32Address, 0, pui32Src[0]);
					pui32Address++;
					pui32Src += ui32Offset;
				}
				break;
			}
			case  2:
			{
				/* 2x2 */
				pui32Src = pui32Pixels;

				for(i=0; i<ui32Loop; i++)
				{
					IW( pui32Address, 0, pui32Src[0]);
					IW( pui32Address, 1, pui32Src[ui32Pitch]);
					IW( pui32Address, 2, pui32Src[1]);
					IW( pui32Address, 3, pui32Src[ui32Pitch+1]);
					pui32Address += 4;
					pui32Src += ui32Offset;
				}
				break;
			}
			case  4:
			{
				/* 4x4 */
				IMG_UINT32 ui32Pitch3 = ui32Pitch2 + ui32Pitch;
				pui32Src = pui32Pixels;

				for(i=0; i<ui32Loop; i++)
				{
					IW( pui32Address, 0, pui32Src[0]);
					IW( pui32Address, 1, pui32Src[ui32Pitch]);
					IW( pui32Address, 2, pui32Src[1]);
					IW( pui32Address, 3, pui32Src[ui32Pitch+1]);

					IW( pui32Address, 4, pui32Src[ui32Pitch2]);
					IW( pui32Address, 5, pui32Src[ui32Pitch3]);
					IW( pui32Address, 6, pui32Src[ui32Pitch2+1]);
					IW( pui32Address, 7, pui32Src[ui32Pitch3+1]);

					IW( pui32Address, 8, pui32Src[2]);
					IW( pui32Address, 9, pui32Src[ui32Pitch+2]);
					IW( pui32Address, 10, pui32Src[3]);
					IW( pui32Address, 11, pui32Src[ui32Pitch+3]);

					IW( pui32Address, 12, pui32Src[ui32Pitch2+2]);
					IW( pui32Address, 13, pui32Src[ui32Pitch3+2]);
					IW( pui32Address, 14, pui32Src[ui32Pitch2+3]);
					IW( pui32Address, 15, pui32Src[ui32Pitch3+3]);
					pui32Address += 16;
					pui32Src += ui32Offset;
				}
				break;
			}
			case  8:	   /* 8x8 */
			{
				pui32Src = pui32Pixels;

				for(i=0; i<ui32Loop; i++)
				{
					Write8x8Pixels32bpp(pui32Src, ui32Pitch2, &pui32Address, pui32Src+ui32Pitch);
					pui32Src += ui32Offset;
				}
				break;
			}
			case  16:	/* 16x16 texture */
			{
				pui32Src = pui32Pixels;

				for(i=0; i<ui32Loop; i++)
				{
					Write8x8Pixels32bpp(pui32Src,              ui32Pitch2, &pui32Address, pui32Src+ui32Pitch);
					Write8x8Pixels32bpp(pui32Src+ui32Pitch8,   ui32Pitch2, &pui32Address, pui32Src+ui32Pitch8+ui32Pitch);
					Write8x8Pixels32bpp(pui32Src+8,            ui32Pitch2, &pui32Address, pui32Src+8+ui32Pitch);
					Write8x8Pixels32bpp(pui32Src+ui32Pitch8+8, ui32Pitch2, &pui32Address, pui32Src+ui32Pitch8+8+ui32Pitch);
					pui32Src += ui32Offset;
				}
				break;
			}
		}
	}
	else
	{
		pui32Src = pui32Pixels;
	
		InitLookupTable32(ui32Pitch, pui32LookupTable);

		for(i=0; i<ui32Loop; i++)
		{
			RecursiveTwid32bpp(ui32Square, pui32Src, ui32Pitch, ui32Square, ui32Square, &pui32Address, pui32LookupTable);
			pui32Src += ui32Offset;
		}
	}
} /* TwiddleRectangularTexture32bpp */



/******************************************************************************
 * Function Name: DeTwiddleAddress32bpp (TextureTwiddle32bpp)
 *
 * Inputs       : pDestAddress, pSrcPixels, ui32Width, ui32Height, ui32StrideIn.
 *				  
 * Outputs      : -
 * Returns      : -
 *
 * Description  : Twiddle the texture and write into texture memory. 
 *****************************************************************************/
IMG_VOID IMG_INTERNAL DeTwiddleAddress32bpp(IMG_VOID		*pvDestAddress,
												const IMG_VOID	*pvSrcPixels, 
												IMG_UINT32		ui32Width,
												IMG_UINT32		ui32Height,
												IMG_UINT32		ui32StrideIn)
{
	IMG_UINT32 *pui32DestAddress = (IMG_UINT32 *)pvDestAddress;
	const IMG_UINT32 *pui32Pixels = (const IMG_UINT32 *) pvSrcPixels;
	IMG_UINT32 aui32LookupTable32[32];

	if(ui32Width == ui32Height)
	{
		/* normal squared texture */
		if(ui32Width < SMALL_TEXTURE_32BPP)
		{
			TwiddleSmallTexture32bpp(pui32DestAddress, ui32Width, ui32StrideIn, pui32Pixels);	
		}
		else
		{
			InitLookupTable32(ui32StrideIn, aui32LookupTable32);
			RecursiveTwid32bpp(ui32Width, pui32Pixels, ui32StrideIn, ui32Width, ui32Width, &pui32DestAddress, aui32LookupTable32);
		}
	}
	else
	{
		/* non squared texture */
		TwiddleRectangularTexture32bpp(pui32DestAddress, ui32Width, ui32Height, ui32StrideIn, pui32Pixels, aui32LookupTable32);
	}
} /* DeTwiddleAddress32bpp*/


/******************************************************************************
 * Function Name: Untwiddle32bpp   INTERNAL ONLY
 *
 * Inputs       : ui32DestStrideInBytes, pui32Src, ui32DestX1, ui32DestY1, ui32DestX2, ui32DestY2 
 *
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 *
 * Description  : Untwiddle Translates (twiddled order src to scan dest)
 *
 *****************************************************************************/
static IMG_VOID Untwiddle32bpp(IMG_UINT32 *pui32Dest,
							   IMG_UINT32  ui32DestStrideInPixels,
							   const IMG_UINT32 *pui32Src,
							   IMG_UINT32  ui32DestX1,
							   IMG_UINT32  ui32DestY1,
							   IMG_UINT32  ui32DestX2,
							   IMG_UINT32  ui32DestY2)
{
	IMG_UINT32 ui32CountY, ui32CountX;
	IMG_UINT32 ui32DestScanInc = ui32DestStrideInPixels - ((ui32DestX2 - ui32DestX1));
	register IMG_UINT32 *pui32CurrentDest = pui32Dest;

	for (ui32CountY = ui32DestY1; ui32CountY < ui32DestY2; ui32CountY++)
	{
		for (ui32CountX = ui32DestX1; ui32CountX < ui32DestX2; ui32CountX++)
		{
			*pui32CurrentDest = pui32Src[GET_TWIDDLED_ADDRESS(ui32CountX,ui32CountY)];
			pui32CurrentDest++;
		}
		pui32CurrentDest += ui32DestScanInc;
	}
}

/******************************************************************************
 * Function Name: ReadBackTwiddle32bpp   INTERNAL ONLY
 *
 * Inputs       : ui32DestStrideInBytes, pui32Src, ui32DestX1, ui32DestY1, ui32DestX2, ui32DestY2 
 *
 * Outputs      : None
 * Returns      : None
 * Globals Used : None
 *
 * Description  : Untwiddles 32bpp texture using rectangular lookup table
 *
 *****************************************************************************/
IMG_VOID IMG_INTERNAL ReadBackTwiddle32bpp(IMG_VOID			*pvDest,
											   const IMG_VOID	*pvSrc,
											   IMG_UINT32		ui32Log2Width,
											   IMG_UINT32		ui32Log2Height,
											   IMG_UINT32		ui32Width,
											   IMG_UINT32		ui32Height,
											   IMG_UINT32		ui32DstStride)
{

	IMG_UINT32 *pui32Address = (IMG_UINT32 *)pvDest;
	const IMG_UINT32 *pui32Pixels = (const IMG_UINT32 *)pvSrc;

	IMG_UINT32 ui32SrcWidth = (1UL << ui32Log2Width);	
	IMG_UINT32 ui32SrcHeight = (1UL << ui32Log2Height);

	IMG_UINT32 ui32TwiddledSize = MIN(ui32SrcWidth,ui32SrcHeight);

	if (ui32SrcWidth > ui32SrcHeight)
	{
		IMG_INT32 i32CurrentX = 0;
		IMG_INT32 i32CurrentWidth = (IMG_INT32)ui32Width;

		while (i32CurrentWidth > 0)
		{
			IMG_INT32 i32BlockX = i32CurrentX;
			IMG_INT32 i32BlockWidth = i32CurrentWidth;

			if (i32BlockX < 0)
				i32BlockX = 0;

			if (i32BlockX < (IMG_INT32)ui32TwiddledSize)
			{
				if (i32BlockWidth > (IMG_INT32)ui32TwiddledSize)
					i32BlockWidth = (IMG_INT32)ui32TwiddledSize;

				Untwiddle32bpp(pui32Address,
							   ui32DstStride,
							   pui32Pixels,
							   (IMG_UINT32)i32BlockX,
							   0,
							   (IMG_UINT32)(i32BlockX + i32BlockWidth),
							   ui32Height);

				pui32Pixels += (ui32TwiddledSize * ui32TwiddledSize);
				pui32Address += (IMG_UINT32)i32BlockWidth;
			}
			i32CurrentX -= (IMG_INT32)ui32TwiddledSize;
			i32CurrentWidth -= (IMG_INT32)ui32TwiddledSize;
		}
	}
	else
	{
		IMG_INT32 i32CurrentY = 0;
		IMG_INT32 i32CurrentHeight = (IMG_INT32)ui32Height;

		while (i32CurrentHeight > 0)
		{
			IMG_INT32 i32BlockY = i32CurrentY;
			IMG_INT32 i32BlockHeight = i32CurrentHeight;

			if (i32BlockY < 0)
				i32BlockY = 0;

			if (i32BlockY < (IMG_INT32)ui32TwiddledSize)
			{
				if (i32BlockHeight > (IMG_INT32)ui32TwiddledSize)
					i32BlockHeight = (IMG_INT32)ui32TwiddledSize;

				Untwiddle32bpp(pui32Address,
							   ui32DstStride,
							   pui32Pixels,
							   0,
							   (IMG_UINT32)i32BlockY,
							   ui32Width,
							   (IMG_UINT32)(i32BlockY + i32BlockHeight));

				pui32Pixels += (ui32TwiddledSize * ui32TwiddledSize);
				pui32Address += (IMG_UINT32)i32BlockHeight * ui32DstStride;
			}
			i32CurrentY -= (IMG_INT32)ui32TwiddledSize;
			i32CurrentHeight -= (IMG_INT32)ui32TwiddledSize;
		}
	}
}


/***********************************************************************************
 Function Name      : getTwiddledAddress
 Inputs             : ui32UPos, ui32VPos, ui32USize, ui32VSize
 Outputs            : -
 Returns            : Twiddled address
 Description        : Interleaves UV addresses to form a twiddled address 
************************************************************************************/
static IMG_UINT32 getTwiddledAddress(IMG_UINT32 ui32UPos, IMG_UINT32 ui32VPos, IMG_UINT32 ui32USize, IMG_UINT32 ui32VSize)
{
	IMG_UINT32 ui32Valids=0;
	IMG_UINT32 ui32Mask = 1;
	IMG_UINT32 ui32UValid,ui32VValid;
	IMG_UINT32 ui32Addr = 0;

	while(ui32VSize || ui32USize)
	{
		ui32UValid = ui32VValid = 0;

		if(ui32VSize)
		{
			ui32Addr |= (ui32VPos & ui32Mask) << ui32Valids;

			ui32VSize--;

			ui32VValid++;
		}

		if(ui32USize)
		{
			ui32Addr |= (ui32UPos & ui32Mask) << (ui32Valids + ui32VValid);

			ui32USize--;

			ui32UValid++;
		}

		ui32Mask <<= 1;

		ui32Valids += ui32UValid + ui32VValid - 1;	/* PRQA S 3382 */ /* It is impossible to wraparound past zero. */
	}

	return ui32Addr;
}

/***********************************************************************************
 Function Name      : ReadBackTwiddle[8/ETC1]bpp
 Inputs             : pvSrc, ui32Log2Width, ui32Log2Height, ui32X, ui32Y, ui32Width, 
					  ui32Height, ui32DstStride
 Outputs            : pvDest
 Returns            : -
 Description        : Reads a subreagion back from a twiddled texture
					    using random (twiddled) read, sequential write.
                      All arguments are given in pixels.
                      ui32Log2Width, ui32Log2Height refer to the source raster.
                      ui32DstStride refers to the destination raster.
************************************************************************************/
IMG_INTERNAL IMG_VOID ReadBackTwiddle8bpp(IMG_VOID *pvDest, const IMG_VOID *pvSrc, IMG_UINT32 ui32Log2Width, 
											IMG_UINT32 ui32Log2Height, IMG_UINT32 ui32Width, IMG_UINT32 ui32Height, 
											IMG_UINT32 ui32DstStride)
{
	IMG_UINT32 ui32XCount, ui32YCount, ui32Address;
	IMG_UINT8 *pui8Address = (IMG_UINT8 *)pvDest;
	const IMG_UINT8 *pui8Pixels = (const IMG_UINT8 *)pvSrc;
	
	for(ui32YCount=0; ui32YCount < ui32Height; ui32YCount++)
	{
		for(ui32XCount=0; ui32XCount < ui32Width; ui32XCount++)
		{
			ui32Address = getTwiddledAddress(ui32XCount, ui32YCount, ui32Log2Width, ui32Log2Height);
		
			pui8Address[ui32YCount*ui32DstStride + ui32XCount] = pui8Pixels[ui32Address];
		}
	}
}

IMG_INTERNAL IMG_VOID ReadBackTwiddleETC1(IMG_VOID *pvDest, const IMG_VOID *pvSrc, IMG_UINT32 ui32Log2Width, 
											IMG_UINT32 ui32Log2Height, IMG_UINT32 ui32Width, IMG_UINT32 ui32Height,
											IMG_UINT32 ui32DstStride)
{
	IMG_UINT32 ui32XCount, ui32YCount, ui32Address;
	IMG_UINT32 *pui32Address = (IMG_UINT32 *)pvDest;
	const IMG_UINT32 *pui32Pixels = (const IMG_UINT32 *)pvSrc;

	for(ui32YCount=0;ui32YCount < ui32Height;ui32YCount++)
	{
		for(ui32XCount=0; ui32XCount < ui32Width; ui32XCount++)
		{
			ui32Address = 2 * getTwiddledAddress(ui32XCount, ui32YCount, ui32Log2Width, ui32Log2Height);
		
			pui32Address[2 * (ui32YCount*ui32DstStride + ui32XCount)] = pui32Pixels[ui32Address];
			pui32Address[2 * (ui32YCount*ui32DstStride + ui32XCount) + 1] = pui32Pixels[ui32Address + 1];
		}
	}
}

/***********************************************************************************
 Function Name      : getUV
 Inputs             : ui32Val, ui32USize, ui32VSize
 Outputs            : pui32Pos
 Returns            : -
 Description        : Deinterleaves UV addresses from twiddled address
************************************************************************************/
static IMG_VOID getUV(IMG_UINT32 *pui32Pos, IMG_UINT32 ui32Val, IMG_UINT32 ui32USize, IMG_UINT32 ui32VSize)
{
	IMG_INT32 i32Valids=0;
	IMG_UINT32 ui32Mask = 1;
	IMG_UINT32 ui32UValid,ui32VValid;

	while(ui32VSize || ui32USize)
	{
		ui32UValid = ui32VValid = 0;

		if(ui32VSize)
		{
			pui32Pos[1] |= (ui32Val & ui32Mask) >> i32Valids;

			ui32Mask <<= 1;

			ui32VSize--;

			ui32VValid++;
		}

		if(ui32USize)
		{
			pui32Pos[0] |= (ui32Val & ui32Mask) >> ((IMG_UINT32)i32Valids + ui32VValid);

			ui32Mask <<= 1;

			ui32USize--;

			ui32UValid++;
		}

		i32Valids += (IMG_INT32)ui32UValid + (IMG_INT32)ui32VValid - 1;
	}
}

/***********************************************************************************
 Function Name      : DeTwiddleAddress[8bpp/ETC]
 Inputs             : pvSrc, ui32Width, ui32Height, ui32StrideIn
 Outputs            : pvDest
 Returns            : -
 Description        : Twiddles a whole texture using random read, sequential write.
************************************************************************************/
IMG_INTERNAL IMG_VOID DeTwiddleAddress8bpp( IMG_VOID *pvDest,
											const IMG_VOID *pvSrc,
											IMG_UINT32 ui32Width,
											IMG_UINT32 ui32Height,
											IMG_UINT32 ui32StrideIn)
{
	IMG_UINT32 ui32Count, ui32Address;
	IMG_UINT32 ui32Temp, aui32Pos[2];
	IMG_UINT32 ui32USize = 0, ui32VSize = 0;
	IMG_UINT8 *pui8Address = (IMG_UINT8 *)pvDest;
	const IMG_UINT8 *pui8Pixels = (const IMG_UINT8 *)pvSrc;

	/* Calculate power of 2 sizes */
	ui32Temp = ui32Width;

	while(ui32Temp>1)
	{
		ui32USize++;

		ui32Temp >>=1;
	}

	ui32Temp = ui32Height;

	while(ui32Temp>1)
	{
		ui32VSize++;

		ui32Temp >>=1;
	}
	
	if(ui32Width > 1 && ui32Height > 1)
	{
		/* Optimises writes for 2x2 block */
		for(ui32Count = 0; ui32Count < ui32Width*ui32Height; ui32Count += 4)
		{
			aui32Pos[0]=0; /*U*/
			aui32Pos[1]=0; /*V*/
			
			getUV(aui32Pos, ui32Count, ui32USize, ui32VSize);
			ui32Address = (aui32Pos[1] * ui32StrideIn) + aui32Pos[0];
			
			pui8Address[ui32Count]		= pui8Pixels[ui32Address];
			pui8Address[ui32Count+1]	= pui8Pixels[ui32Address + ui32StrideIn];
			pui8Address[ui32Count+2]	= pui8Pixels[ui32Address + 1];
			pui8Address[ui32Count+3]	= pui8Pixels[ui32Address + ui32StrideIn + 1];
		}
	}
	else
	{
		for(ui32Count = 0; ui32Count < ui32Width*ui32Height; ui32Count++)
		{
			aui32Pos[0]=0; /*U*/
			aui32Pos[1]=0; /*V*/
			
			getUV(aui32Pos, ui32Count, ui32USize, ui32VSize);
			ui32Address = (aui32Pos[1] * ui32StrideIn) + aui32Pos[0];			

			pui8Address[ui32Count] = pui8Pixels[ui32Address];
		}
	}
}

IMG_INTERNAL IMG_VOID DeTwiddleAddressETC1(IMG_VOID	*pvDest,
											const IMG_VOID	*pvSrc,
											IMG_UINT32 ui32Width,
											IMG_UINT32 ui32Height,
											IMG_UINT32 ui32StrideIn)
{
	IMG_UINT32 ui32Count, ui32Address;
	IMG_UINT32 ui32Temp, aui32Pos[2];
	IMG_UINT32 ui32USize = 0, ui32VSize = 0;
	IMG_UINT32 *pui32Address = (IMG_UINT32 *)pvDest;
	const IMG_UINT32 *pui32Pixels = (const IMG_UINT32 *)pvSrc;
	IMG_UINT32 ui32Stride = ui32StrideIn * 2;

	/* Calculate power of 2 sizes */
	ui32Temp = ui32Width;

	while(ui32Temp>1)
	{
		ui32USize++;

		ui32Temp >>=1;
	}

	ui32Temp = ui32Height;

	while(ui32Temp>1)
	{
		ui32VSize++;

		ui32Temp >>=1;
	}

	if(ui32Width > 1 && ui32Height > 1)
	{
		/* Optimises writes for 2x2 block */
		for(ui32Count = 0; ui32Count < ui32Width*ui32Height; ui32Count += 4)
		{
			aui32Pos[0]=0; /*U*/
			aui32Pos[1]=0; /*V*/
			
			getUV(aui32Pos, ui32Count, ui32USize, ui32VSize);
			ui32Address = 2 * ((aui32Pos[1] * ui32StrideIn) + aui32Pos[0]);
			
			pui32Address[ui32Count * 2]		= pui32Pixels[ui32Address];
			pui32Address[ui32Count * 2 + 1]	= pui32Pixels[ui32Address + 1];
		
			pui32Address[ui32Count * 2 + 2]	= pui32Pixels[ui32Address + ui32Stride];
			pui32Address[ui32Count * 2 + 3]	= pui32Pixels[ui32Address + ui32Stride + 1];
		
			pui32Address[ui32Count * 2 + 4]	= pui32Pixels[ui32Address + 2];
			pui32Address[ui32Count * 2 + 5]	= pui32Pixels[ui32Address + 3];
			
			pui32Address[ui32Count * 2 + 6]	= pui32Pixels[ui32Address + ui32Stride + 2];
			pui32Address[ui32Count * 2 + 7]	= pui32Pixels[ui32Address + ui32Stride + 3];
		}
	}
	else
	{
		for(ui32Count = 0; ui32Count < ui32Width*ui32Height; ui32Count++)
		{
			aui32Pos[0]=0; /*U*/
			aui32Pos[1]=0; /*V*/
			
			getUV(aui32Pos, ui32Count, ui32USize, ui32VSize);
			ui32Address = 2 * ((aui32Pos[1] * ui32StrideIn) + aui32Pos[0]);			

			pui32Address[ui32Count * 2]		= pui32Pixels[ui32Address];
			pui32Address[ui32Count * 2 + 1]	= pui32Pixels[ui32Address + 1];
		}
	}
}


#endif /* defined(SGX_FEATURE_HYBRID_TWIDDLING) */



/******************************************************************************
 End of file (twiddle.c)
******************************************************************************/

