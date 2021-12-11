/******************************************************************************
 * Name         : makemips.c
 * Author       :
 * Created      :
 *
 * Copyright    : 2003-2006 by Imagination Technologies Limited.
 * 				: All rights reserved. No part of this software, either
 * 				: material or conceptual may be copied or distributed,
 * 				: transmitted, transcribed, stored in a retrieval system or
 * 				: translated into any human or computer language in any form
 * 				: by any means, electronic, mechanical, manual or otherwise,
 * 				: or disclosed to third parties without the express written
 * 				: permission of Imagination Technologies Limited,
 * 				: Home Park Estate, Kings Langley, Hertfordshire,
 * 				: WD4 8LZ, U.K.
 *
 * Platform     : ANSI
 *
 * Description	: Make mipmaps for all texture formats of OpenGL ES 2.0
 *
 * $Log: makemips.c $
 *****************************************************************************/

#include "context.h"
#include <stdio.h>

typedef struct 
{
	IMG_UINT32 ui32Width;
	IMG_UINT32 ui32Height;
	IMG_UINT32 ui32Stride;  /* in bytes */
	IMG_VOID   *pvBuffer;   /* texture map data */

} MapInfo;

typedef struct
{
	IMG_UINT32 ui32BlueMask;
	IMG_UINT32 ui32GreenMask;
	IMG_UINT32 ui32RedMask;
	IMG_UINT32 ui32AlphaMask;

} TexelFormat;


/***********************************************************************************
 Function Name      : HardwareMipGen
 Inputs             : gc,  psTex, ui32OffsetInBytes, psLevel  
 Outputs            : -
 Returns            : Success/failure
 Description        : Generate mipmap textures using the device.
************************************************************************************/

static IMG_BOOL HardwareMipGen(GLES2Context *gc,
							   GLES2Texture *psTex,
							   IMG_UINT32 ui32OffsetInBytes,
							   GLES2MipMapLevel *psLevel,
							   IMG_UINT32 *pui32Lod)
{
    IMG_UINT32              ui32Lod       = *pui32Lod;
    IMG_UINT32              ui32BaseLevel = *pui32Lod;
    SGX_QUEUETRANSFER       mipgenQueueTransfer;
    PVRSRV_ERROR		    eResult;
    PVRSRV_PIXEL_FORMAT     ePixelFormat  = PVRSRV_PIXEL_FORMAT_UNKNOWN;
    IMG_UINT32              ui32MaxDim    = (psLevel->ui32Width > psLevel->ui32Height) ? psLevel->ui32Width : psLevel->ui32Height; 
    IMG_BOOL		        bUnSupported  = IMG_FALSE;

    PVR_UNREFERENCED_PARAMETER(gc);
    GLES2MemSet(&mipgenQueueTransfer, 0, sizeof(SGX_QUEUETRANSFER));

    /* Setup HWTQ pixel format */
    switch(psLevel->psTexFormat->ePixelFormat) 
    {
        case PVRSRV_PIXEL_FORMAT_ARGB8888:
        case PVRSRV_PIXEL_FORMAT_ABGR8888:
        case PVRSRV_PIXEL_FORMAT_ARGB1555:
        case PVRSRV_PIXEL_FORMAT_ARGB4444:
        case PVRSRV_PIXEL_FORMAT_RGB565:
        case PVRSRV_PIXEL_FORMAT_A8:
        case PVRSRV_PIXEL_FORMAT_L8:
        case PVRSRV_PIXEL_FORMAT_A8L8:
		{
			ePixelFormat = psLevel->psTexFormat->ePixelFormat;
			break;
		}
        default:
		{
			bUnSupported = IMG_TRUE;
			break;
		}
    }

    if(bUnSupported)
    {
		PVR_DPF((PVR_DBG_ERROR, "HardwareMipGen: Unsupported texture format"));
		return IMG_FALSE;
    }

    mipgenQueueTransfer.eType = SGXTQ_MIPGEN;
    
    /* Setup HWTQ mipmap gen filter type */
    switch(psTex->sState.ui32MinFilter & ~EURASIA_PDS_DOUTT0_MINFILTER_CLRMSK)
    {
        case EURASIA_PDS_DOUTT0_MINFILTER_POINT:
		{
			mipgenQueueTransfer.Details.sMipGen.eFilter = SGXTQ_FILTERTYPE_POINT;
			break;
		}
        case EURASIA_PDS_DOUTT0_MINFILTER_LINEAR:
		{
			mipgenQueueTransfer.Details.sMipGen.eFilter = SGXTQ_FILTERTYPE_LINEAR;
			break;
		}
        case EURASIA_PDS_DOUTT0_MINFILTER_ANISO:
		{
			mipgenQueueTransfer.Details.sMipGen.eFilter = SGXTQ_FILTERTYPE_ANISOTROPIC;
			break;
		}
        default: /* ui32MinFilter is not set in this case, only glGenerateMipmap() is called */
		{
			mipgenQueueTransfer.Details.sMipGen.eFilter = SGXTQ_FILTERTYPE_POINT;
			break;
		}
    }

    while(ui32MaxDim)
    {
		if (ui32Lod < ui32BaseLevel+GLES2_MAX_TEXTURE_MIPMAP_LEVELS)
		{
			ui32Lod++;
			ui32MaxDim >>= 1;
		}
    }
    ui32Lod--;
    mipgenQueueTransfer.Details.sMipGen.ui32Levels = ui32Lod;
    *pui32Lod = ui32Lod;	
    
    /* source surface */
    mipgenQueueTransfer.ui32NumSources = 1;
    
    mipgenQueueTransfer.asSources[0].sDevVAddr.uiAddr = psTex->psMemInfo->sDevVAddr.uiAddr + ui32OffsetInBytes;
    mipgenQueueTransfer.asSources[0].ui32Width = psLevel->ui32Width;
    mipgenQueueTransfer.asSources[0].ui32Height = psLevel->ui32Height;

    mipgenQueueTransfer.asSources[0].i32StrideInBytes = (IMG_INT32)((psLevel->ui32Width) * (psLevel->psTexFormat->ui32TotalBytesPerTexel));

    mipgenQueueTransfer.asSources[0].ui32ChunkStride = 0;  /* Would need to be set for >32bpp formats */
    mipgenQueueTransfer.asSources[0].eFormat = ePixelFormat;
    
    mipgenQueueTransfer.asSources[0].psSyncInfo = psTex->psMemInfo->psClientSyncInfo;

    mipgenQueueTransfer.asDests[0].ui32ChunkStride = 0; /* Would need to be set for >32bpp formats */
    mipgenQueueTransfer.asDests[0].eFormat = ePixelFormat;

    eResult = SGXQueueTransfer(&gc->psSysContext->s3D, gc->psSysContext->hTransferContext, &mipgenQueueTransfer);

    if(eResult != PVRSRV_OK)
    {
		PVRSRV_CLIENT_SYNC_INFO	  *psSrcSyncInfo; /* Src and Dst use the same SyncInfo */

		PVR_DPF((PVR_DBG_ERROR, "HardwareMipGen: Failed to generate texture mipmap levels (error=%d)", eResult));

		/* Setup psSrcSyncInfo */
#if defined(GLES2_EXTENSION_EGL_IMAGE)
		if (psTex->psEGLImageTarget)
		{
			psSrcSyncInfo = psTex->psEGLImageTarget->psMemInfo->psClientSyncInfo;
		}
		else
#endif /* defined(GLES2_EXTENSION_EGL_IMAGE) */
		{
			psSrcSyncInfo = psTex->psMemInfo->psClientSyncInfo;
		}

		/* Check the texture has been uploaded before sw fallback */
		if (psSrcSyncInfo)
		{
#if defined(PDUMP)
			PVRSRVPDumpSyncPol(gc->ps3DDevData->psConnection,
							   psSrcSyncInfo,
							   IMG_FALSE,
							   psSrcSyncInfo->psSyncData->ui32WriteOpsPending,
							   0xFFFFFFFF);
#endif /* defined(PDUMP) */

			/* HardwareMipGen: waiting for previous texture mipgen */
			while (SGX2DQueryBlitsComplete(&gc->psSysContext->s3D, psSrcSyncInfo, IMG_TRUE) != PVRSRV_OK)
			{
			}
		}

		return IMG_FALSE;
    }

    return IMG_TRUE;
}




/***********************************************************************************
 Function Name      : MakeMapLevels8bpp
 Inputs             : pSrcMap - pointer to the source map information
                    : ui32Xscale - width ratio between source and destination
                    : ui32Yscale - height ratio between source and destination
 Outputs            : pDstMap - pointer to the destination map information
 Returns            :
 Description        : Creates a new MIPmap level by sampling the previous map 
                      (2D, 8bpp texture)
************************************************************************************/
static void MakeMapLevel8bpp(const MapInfo *pSrcMap, const MapInfo *pDstMap, const IMG_UINT32 ui32Xscale,
							 const IMG_UINT32 ui32Yscale)
{
	IMG_UINT32 x,y,k;
	IMG_UINT8 *pui8Int1;
	IMG_UINT8 *pui8In;
	IMG_UINT32 ui32SrcInc, ui32DstInc;

	GLES_ASSERT (pSrcMap->ui32Width / pDstMap->ui32Width == ui32Xscale);
	GLES_ASSERT (pSrcMap->ui32Height / pDstMap->ui32Height == ui32Yscale);
	GLES_ASSERT (ui32Xscale<=2 && ui32Yscale<=2);
	GLES_ASSERT (ui32Xscale>1 || ui32Yscale>1);

	ui32SrcInc = pSrcMap->ui32Stride;		/* in bytes */
	ui32DstInc = pDstMap->ui32Stride;		/* in bytes */

	pui8In = (IMG_UINT8 *)pSrcMap->pvBuffer;
	pui8Int1 = (IMG_UINT8 *) pDstMap->pvBuffer;

	for (y = 0; y < pSrcMap->ui32Height; y += ui32Yscale)
	{
		for (k = 0,x = 0; x < pSrcMap->ui32Width; x += ui32Xscale, k++)
		{
			IMG_UINT32 ui32Value;

			if (ui32Xscale==2 && ui32Yscale==2)
			{
				/* Average every 2x2 pixel block in the source to create one new pixel.*/
				ui32Value =  pui8In[x];
				ui32Value += pui8In[x+1];
				ui32Value += pui8In[x + ui32SrcInc];
				ui32Value += pui8In[x + ui32SrcInc + 1];
				ui32Value += 2;
				ui32Value >>= 2;

				pui8Int1[k] = (IMG_UINT8)ui32Value;
			}
			else
			{
				/* Average every 2x1(or 1x2) pixel block in the source to create one new pixel.*/
				ui32Value = pui8In[x];

				if(ui32Xscale==1)
				{
					ui32Value += pui8In[x+ui32SrcInc];
				}
				else
				{
					ui32Value += pui8In[x+1];
				}

				ui32Value++;
				ui32Value >>= 1;

				pui8Int1[k] = (IMG_UINT8)ui32Value;
			}
		}

		pui8Int1 += ui32DstInc;
		pui8In   += ui32SrcInc * 2;
	}
}


/***********************************************************************************
 Function Name      : MakeMapLevel16bpp
 Inputs             : pSrcMap - pointer to the source map information
                    : ui32XScale - width ratio between source and destination
                    : ui32YScale - height ratio between source and destination
                    : psFormat- pointer to the texel format information
 Outputs            : pDstMap - pointer to the destination map information
 Returns            :
 Description        : Creates a new MIPmap level by sampling the previous map 
                      (2D, 16bpp texture). 
************************************************************************************/
static void MakeMapLevel16bpp(const MapInfo *pSrcMap, const MapInfo *pDstMap, const IMG_UINT32 ui32Xscale,
							  const IMG_UINT32 ui32Yscale, const TexelFormat *psFormat)
{
	IMG_UINT32 ui32A1, ui32A2;
	IMG_UINT32 x, y, k;
	IMG_UINT32 ui32RedMask   = psFormat->ui32RedMask,
	           ui32GreenMask = psFormat->ui32GreenMask,
			   ui32BlueMask  = psFormat->ui32BlueMask,
			   ui32AlphaMask = psFormat->ui32AlphaMask;

	GLES_ASSERT (pSrcMap->ui32Width / pDstMap->ui32Width == ui32Xscale);
	GLES_ASSERT (pSrcMap->ui32Height / pDstMap->ui32Height == ui32Yscale);
	GLES_ASSERT(ui32Xscale<=2 && ui32Yscale<=2);
	GLES_ASSERT(ui32Xscale>1 || ui32Yscale>1);

	if((ui32Xscale==2) && (ui32Yscale==2))
	{
		IMG_UINT32 ui32Red, ui32Green, ui32Blue, ui32Alpha;
		IMG_UINT32 ui32SrcInc, ui32DstInc;		
		IMG_UINT16 *pui16Int1;
		IMG_UINT32 *pui32In;

		pui16Int1 = (IMG_UINT16 *) pDstMap->pvBuffer;

		ui32SrcInc = pSrcMap->ui32Stride >> 2; /* In ui32ORDs */
		ui32DstInc = pDstMap->ui32Stride/2; /* IN WORDS */

		/*
			Average every 2x2 pixel block in the source to create one new pixel.
		*/
		pui32In = (IMG_UINT32 *)pSrcMap->pvBuffer;

		for (y=0;y<pSrcMap->ui32Height;y += ui32Yscale)
		{
			for (k=0, x=0; x < pSrcMap->ui32Width; x += ui32Xscale, k++)
			{
				/* Load two 16-bit texels in each 32-bit variable */
				ui32A1 = pui32In[k];
				ui32A2 = pui32In[k + ui32SrcInc];

				/* First source texel is in the lower half of ui32A1 */
				ui32Blue  = ui32A1 & ui32BlueMask;
				ui32Green = ui32A1 & ui32GreenMask;
				ui32Red   = ui32A1 & ui32RedMask;
				ui32Alpha = ui32A1 & ui32AlphaMask;

				/* Second source texel is in the upper half of ui32A1 */
				ui32Blue  += (ui32A1>>16) & ui32BlueMask;
				ui32Green += (ui32A1>>16) & ui32GreenMask;
				ui32Red   += (ui32A1>>16) & ui32RedMask;
				ui32Alpha += (ui32A1>>16) & ui32AlphaMask;

				/* Third source texel is in the lower half of ui32A2 */
				ui32Blue  += ui32A2 & ui32BlueMask;
				ui32Green += ui32A2 & ui32GreenMask;
				ui32Red   += ui32A2 & ui32RedMask;
				ui32Alpha += ui32A2 & ui32AlphaMask;

				/* Fourth source texel is in the upper half of ui32A2 */
				ui32Blue  += (ui32A2>>16) & ui32BlueMask;
				ui32Green += (ui32A2>>16) & ui32GreenMask;
				ui32Red   += (ui32A2>>16) & ui32RedMask;
				ui32Alpha += (ui32A2>>16) & ui32AlphaMask;

				/* Compute the averages dividing by four and apply the mask */
				pui16Int1[k] = (IMG_UINT16)( ((ui32Blue >>2) & ui32BlueMask)
											 |((ui32Green>>2) & ui32GreenMask)
											 |((ui32Red  >>2) & ui32RedMask)
											 |((ui32Alpha>>2) & ui32AlphaMask) );
			}

			pui16Int1 += ui32DstInc;
			pui32In   += ui32SrcInc * 2;
		}
	}
	else
	{
		IMG_UINT32 ui32Red, ui32Green, ui32Blue, ui32Alpha;
		IMG_UINT16 *pui16Int1;
		IMG_UINT16 *pui16In;
		IMG_UINT16 ui16A1, ui16A2;
		IMG_UINT32 ui32SrcInc, ui32DstInc;

		GLES_ASSERT ((ui32Xscale==2 && ui32Yscale==1) || (ui32Xscale==1 && ui32Yscale==2));

		pui16Int1 = (IMG_UINT16 *) pDstMap->pvBuffer; 

		ui32SrcInc = pSrcMap->ui32Stride/2; /* In WORDs */
		ui32DstInc = pDstMap->ui32Stride/2; /* IN WORDS */

		/*
			Average every 2x1 pixel block in the source to create one new pixel.
		*/
		pui16In = pSrcMap->pvBuffer;

		for (y=0;y<pSrcMap->ui32Height;y += ui32Yscale)
		{
			for (k=0, x=0; x < pSrcMap->ui32Width; x += ui32Xscale, k++)
			{
				/* Load the texel values in ui16A1 and ui16A2 */
				ui16A1 = pui16In[x];

				if(ui32Xscale==1 && ui32Yscale==2) 
				{
					ui16A2 = pui16In[x+ui32SrcInc];
				}
				else
				{
					ui16A2 = pui16In[x + 1];
				}

				/* Store each channel of the first texel in a 32-bit variable */
				ui32Blue  = ui16A1 & ui32BlueMask;
				ui32Green = ui16A1 & ui32GreenMask;
				ui32Red   = ui16A1 & ui32RedMask;
				ui32Alpha = ui16A1 & ui32AlphaMask;

				/* Then add the values of the second texel */
				ui32Red   += ui16A2 & ui32RedMask;
				ui32Green += ui16A2 & ui32GreenMask;
				ui32Blue  += ui16A2 & ui32BlueMask;
				ui32Alpha += ui16A2 & ui32AlphaMask;

				/* Compute the average dividing by two and apply mask */
				pui16Int1[k] = (IMG_UINT16)( ((ui32Blue  >>1) & ui32BlueMask)
											 |((ui32Green >>1) & ui32GreenMask )
											 |((ui32Red   >>1) & ui32RedMask)
											 |((ui32Alpha >>1) & ui32AlphaMask));
			}
			
			pui16Int1 += ui32DstInc;
			pui16In += ui32SrcInc * 2;
		}
	}
}


/***********************************************************************************
 Function Name      : MakeMapLevel32bpp
 Inputs             : pSrcMap - pointer to the source map information
                    : ui32XScale - width ratio between source and destination
                    : ui32YScale - height ratio between source and destination
 Outputs            : pDstMap - pointer to the destination map information
 Returns            : 
 Description        : Creates a new MIPmap level by sampling the previous map 
                      (2D, 32bpp texture). 
************************************************************************************/
static void MakeMapLevel32bpp(const MapInfo *pSrcMap, const MapInfo *pDstMap, const IMG_UINT32 ui32Xscale,
							  const IMG_UINT32 ui32Yscale)
{
	GLES_ASSERT (pSrcMap->ui32Width / pDstMap->ui32Width == ui32Xscale);
	GLES_ASSERT (pSrcMap->ui32Height / pDstMap->ui32Height == ui32Yscale);
	GLES_ASSERT(ui32Xscale<=2 && ui32Yscale<=2);
	GLES_ASSERT(ui32Xscale>1 || ui32Yscale>1);

	if (ui32Xscale==2 && ui32Yscale==2)
	{
		IMG_UINT32 x,y,k;
		IMG_UINT32 ui32DstInc, ui32SrcInc;
		IMG_UINT32 *pui32Int1;
		IMG_UINT32 *pui32In;
		IMG_UINT32 ui32Mask;

		pui32In = (IMG_UINT32 *)pSrcMap->pvBuffer;
		
		ui32DstInc = pDstMap->ui32Stride/4;
		ui32SrcInc = pSrcMap->ui32Stride/4; /* in ui32ORDs */

 		pui32Int1 = (IMG_UINT32 *) pDstMap->pvBuffer;

		/*
			Average every 2x2 pixel block in the source to create one new pixel.
		*/
		ui32Mask = 0xfefefefe;

		for (y = 0; y < pSrcMap->ui32Height; y += ui32Yscale)
		{
			for (k = 0, x = 0; x < pSrcMap->ui32Width; x += ui32Xscale, k++)
			{
				IMG_UINT32 ui32InA1, ui32InA2;
				IMG_UINT32 ui32InB1, ui32InB2;

				/* Load the texels in A1, A2, B1 and B2 */
				ui32InA1 = pui32In[x];
				ui32InB1 = pui32In[x + 1];
				ui32InA2 = pui32In[x + ui32SrcInc];
				ui32InB2 = pui32In[x + ui32SrcInc + 1];

				/* The computation here has some rounding errors but it's very fast               */
				/* We are substituting (a+b+c+d)/4 by (((a/2)+(b/2))/2 + ((c/2)+(d/2))/2)         */
				/* ...which is _approximately_ true.                                              */
				/* The masking is needed to remove the carry bits                                 */
				/* All four 8-bit channels are operated at the same time inside a 32-bit variable */
				ui32InA1 &= ui32Mask;
				ui32InA2 &= ui32Mask;

				ui32InA1 >>= 1;
				ui32InA2 >>= 1;
				ui32InA1 += ui32InA2;

				ui32InB1 &= ui32Mask;
				ui32InB2 &= ui32Mask;

				ui32InB1 >>= 1;
				ui32InB2 >>= 1;
				ui32InB1 += ui32InB2;

				ui32InA1 &= ui32Mask;
				ui32InB1 &= ui32Mask;

				ui32InA1 >>= 1;
				ui32InB1 >>= 1;
				ui32InA1 += ui32InB1;

				if (y & ui32Yscale)
				{
					/* Add one to every channel every other row to compensate the rounding error on average */
					/* We could do some serious dithering instead.                                          */
					ui32InA1 += 0x01010101;
				}

				pui32Int1[k] = ui32InA1;
			}

			pui32In   += ui32SrcInc * 2;
			pui32Int1 += ui32DstInc;
		}
	}
	else
	{
		IMG_UINT32 x,y,k;
		IMG_UINT32 ui32DstInc, ui32SrcInc;
		IMG_UINT32 * pui32Int1;
		IMG_UINT32 * pui32In;
		IMG_UINT32 ui32Mask;

		pui32In = (IMG_UINT32 *)pSrcMap->pvBuffer;
		ui32DstInc = pDstMap->ui32Stride/4;
		ui32SrcInc = pSrcMap->ui32Stride/4; /* in ui32ORDs */

		pui32Int1 = (IMG_UINT32 *) pDstMap->pvBuffer;	

		/*
			Average every 2x1 or 1x2 pixel block in the source to create one new pixel.
		*/
		ui32Mask = 0xfefefefe;

		for (y = 0; y < pSrcMap->ui32Height; y += ui32Yscale)
		{
			for (k = 0, x = 0; x < pSrcMap->ui32Width; x += ui32Xscale, k++)
			{
				IMG_UINT32 ui32In1, ui32In2;

				ui32In1 = pui32In[x];

				if (ui32Xscale == 1)
				{
					ui32In2 = pui32In[x + ui32SrcInc];
				}
				else
				{
					ui32In2 = pui32In[x + 1];
				}

				ui32In1 &= ui32Mask;
				ui32In2 &= ui32Mask;

				ui32In1 >>= 1;
				ui32In2 >>= 1;
				ui32In1 += ui32In2;

				pui32Int1[k] = ui32In1;
			}

			pui32Int1 += ui32DstInc;
			pui32In   += ui32SrcInc * 2;
		}
	}
}


/***********************************************************************************
 Function Name      : SetupMipmapTextureControlWords
 Inputs             : gc, psTex
 Outputs            : psTex->sState.sParams
 Returns            : IMG_TRUE if successfull. IMG_FALSE otherwise.
 Description        : Sets up the control words for a texture is going to be used
                      as a render target. Makes the texture resident.
					  NOTE: Do not use with other type of textures.
************************************************************************************/
static IMG_BOOL SetupMipmapTextureControlWords(GLES2Context *gc, GLES2Texture *psTex, IMG_BOOL *pbUnloadTex)
{
    GLES2TextureParamState   *psParams = &psTex->sState;
    const GLES2TextureFormat *psTexFormat = psTex->psMipLevel[0].psTexFormat;
    IMG_UINT32               ui32Width, ui32Height;
    IMG_UINT32               ui32NumLevels, ui32FormatFlags = 0, ui32TexTypeSize = 0;
    IMG_UINT32               aui32StateWord1[GLES2_MAX_CHUNKS];
    IMG_UINT32               ui32WidthLog2, ui32HeightLog2;
    IMG_BOOL                 bStateWord1Changed = IMG_FALSE;
    IMG_UINT32               i;
    IMG_UINT32		         ui32OverloadTexLayout = gc->sAppHints.ui32OverloadTexLayout; /* DO NOT deal with this now */

    *pbUnloadTex = IMG_FALSE;

    /* Restriction Checking */

    /* HWTQ do NOT support non-pow2 texture */
    if ((((psTex->psMipLevel[0].ui32Width)  & (psTex->psMipLevel[0].ui32Width  - 1U)) != 0) ||
		(((psTex->psMipLevel[0].ui32Height) & (psTex->psMipLevel[0].ui32Height - 1U)) != 0))
    {
		return IMG_FALSE;
    }

    /* HWTQ do NOT support float or compressed format textures */
    switch(psTexFormat->ePixelFormat) 
    {
        case PVRSRV_PIXEL_FORMAT_ARGB8888:
        case PVRSRV_PIXEL_FORMAT_ABGR8888:
        case PVRSRV_PIXEL_FORMAT_ARGB1555:
        case PVRSRV_PIXEL_FORMAT_ARGB4444:
        case PVRSRV_PIXEL_FORMAT_RGB565:
        case PVRSRV_PIXEL_FORMAT_A8:
        case PVRSRV_PIXEL_FORMAT_L8:
        case PVRSRV_PIXEL_FORMAT_A8L8:
			break;
        default:
			return IMG_FALSE;
    }

    if (psParams->bAutoMipGen)
    {
		ui32FormatFlags |= GLES2_MIPMAP;
    }

    aui32StateWord1[0] = asSGXPixelFormat[psTex->psMipLevel[0].psTexFormat->ePixelFormat].aui32TAGControlWords[0][1];
    
    ui32Width  = psTex->psMipLevel[0].ui32Width;
    ui32Height = psTex->psMipLevel[0].ui32Height;

    ui32WidthLog2  = psTex->psMipLevel[0].ui32WidthLog2;
    ui32HeightLog2 = psTex->psMipLevel[0].ui32HeightLog2;

    if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
    {
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_CEM;
#else
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_CEM;
#endif
	
		GLES_ASSERT(ui32Width != 0 && ui32Height != 0);
    }
    else
    {
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE) || defined(SGX_FEATURE_HYBRID_TWIDDLING)
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_2D;
#else
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_ARB_2D;
#endif
    }

    if(ui32FormatFlags & GLES2_MIPMAP)
    {
		ui32NumLevels = MAX(ui32WidthLog2, ui32HeightLog2) + 1;
    }
    else
    {
		ui32NumLevels = 1;
    }

    ui32OverloadTexLayout = (ui32OverloadTexLayout & GLES2_HINT_OVERLOAD_TEX_LAYOUT_POW2_MASK) >> GLES2_HINT_OVERLOAD_TEX_LAYOUT_POW2_SHIFT;
    
    if ((ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_TWIDDLED) ||
		(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_DEFAULT))
    {
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
		ui32TexTypeSize |= (ui32WidthLog2  << EURASIA_PDS_DOUTT1_USIZE_SHIFT)	|
			               (ui32HeightLog2 << EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else 
		ui32TexTypeSize |= (((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)  & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  |
			               (((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT) & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) ;
#endif
    }
    else if(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_STRIDE)
    {
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_STRIDE		            |
			               ((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
			               ((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
    }
    else if(ui32OverloadTexLayout == GLES2_HINT_OVERLOAD_TEX_LAYOUT_TILED)
    {
		ui32TexTypeSize |= EURASIA_PDS_DOUTT1_TEXTYPE_TILED                     |
			               ((ui32Width  - 1) << EURASIA_PDS_DOUTT1_WIDTH_SHIFT)	|
			               ((ui32Height - 1) << EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
    }

    /* Has the texture size or format changed? we need to ghost it */
    for(i=0; i < psTexFormat->ui32NumChunks; i++)
    {
		aui32StateWord1[i] |= ui32TexTypeSize;
	
		if(psParams->aui32StateWord1[i] != aui32StateWord1[i])
		{
			bStateWord1Changed = IMG_TRUE;
		}
    }
	
    if(psTex->psMemInfo)
    {
		/* If the state word 1 has changed or
		 * if the texture has changed from non-mipmap to mipmap or
		 */
		if(bStateWord1Changed || (((ui32FormatFlags & GLES2_MIPMAP) != 0) && ((psTex->ui32HWFlags & GLES2_MIPMAP) == 0)))
		{
			/* then we need unload the inconsistent texture */
			*pbUnloadTex = IMG_TRUE;
		}
    }
    
    /* Replace the old state words */
    for(i=0; i < psTexFormat->ui32NumChunks; i++)
    {
		psParams->aui32StateWord1[i] = aui32StateWord1[i];
    }
    
    psTex->ui32NumLevels = ui32NumLevels;
    psTex->ui32HWFlags   = ui32FormatFlags;
    
    return IMG_TRUE;
}


/***********************************************************************************
 Function Name      : HardwareMakeTextureMipmapLevels
 Inputs/Outputs		: tex - pointing to tex structure
 Inputs             : face - the face number if this texture is a cube map. 0 otherwise
 Returns            : IMG_TRUE if successfull. IMG_FALSE otherwise.
 Description        : Generate a full chain of mipmaps from the base level tex data,
                      the generated texture mipmap levels are directly loaded into hardware.
 Limitations        : We only support power-of-two textures. That's all the spec requires.
************************************************************************************/
static IMG_BOOL HardwareMakeTextureMipmapLevels(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32TexTarget)
{
    IMG_UINT32               ui32Lod;
    GLES2MipMapLevel         *psLevel;
    GLES2TextureParamState   *psParams = &psTex->sState;
    const GLES2TextureFormat *psFormat;

    IMG_UINT32               ui32MaxFace, ui32Face, ui32BaseLevel;
    IMG_UINT32               ui32OffsetInBytes = 0, ui32BytesPerPixel;

    IMG_UINT32               ui32TopUsize, ui32TopVsize;
#if defined(DEBUG)
    IMG_UINT32               ui32ImageSize;
#endif
    IMG_UINT32               i, j;
    IMG_BOOL                 bHWTexUploaded;
    IMG_BOOL                 bHWTexMipGened = IMG_FALSE;

    IMG_BOOL                 bUnloadTex = IMG_FALSE;
    PVRSRV_CLIENT_MEM_INFO   sMemInfo  = {0};

    /*********** Check whether appHints are enabled ****************/
    if(gc->sAppHints.bDisableHWTQMipGen      ||
       gc->sAppHints.bDisableHWTQTextureUpload)
    {
		return IMG_FALSE;
    }

    /*********** Check Restriction for HWTQ *************/
    if (((psTex->psMipLevel[0].ui32Width == 1) && (psTex->psMipLevel[0].ui32Height > 1))
#if defined(FIX_HW_BRN_23054)
		||
		((psTex->psMipLevel[0].ui32Width  < EURASIA_ISPREGION_SIZEX) ||
		 (psTex->psMipLevel[0].ui32Height < EURASIA_ISPREGION_SIZEY))
#endif
		)
    {
		return IMG_FALSE;
    }

    /*********** Set max face count ***********/
    switch(ui32TexTarget)
    {
        case GLES2_TEXTURE_TARGET_2D:
		{
			ui32MaxFace = 1;
			break;
		}
        case GLES2_TEXTURE_TARGET_CEM:
		{
			ui32MaxFace = GLES2_TEXTURE_CEM_FACE_MAX;
			break;
		}
        default:
		{
			return IMG_FALSE; /* Unsupported target */
		}
    }

	/* Set texture control words after all fail conditions.
	   If this function completes and says the texture needs to be unloaded,
	   it must be done, or the meminfo is inconsistent with the flags
	*/
    if(!SetupMipmapTextureControlWords(gc, psTex, &bUnloadTex))
    {
		return IMG_FALSE;
    }

    /*********** Set ui32TopUsize & ui32TopVsize *************/ 
#if defined(SGX_FEATURE_TAG_POT_TWIDDLE)
    ui32TopUsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_USIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_USIZE_SHIFT);
    ui32TopVsize = 1U << ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_VSIZE_CLRMSK) >> EURASIA_PDS_DOUTT1_VSIZE_SHIFT);
#else
    ui32TopUsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_WIDTH_CLRMSK)  >> EURASIA_PDS_DOUTT1_WIDTH_SHIFT);
    ui32TopVsize = 1 + ((psParams->aui32StateWord1[0] & ~EURASIA_PDS_DOUTT1_HEIGHT_CLRMSK) >> EURASIA_PDS_DOUTT1_HEIGHT_SHIFT);
#endif


    /****************** TWO CASES **************************

       0. Base level has NULL data
          0.1. exit

       1. Base level has NOT been uploaded:
	      1.1. device memory has not been allocated at all;
	      1.2. device memory has already been allocated, but with wrong size, so needs to be reallocated;
          1.3. device memory has been allocated with the suitable size for all the levels.

       2. Base level has already been uploaded to device memory:
          2.1. device memory does not exist;
	      2.2. device memory has already been allocated, but with wrong size, so needs to be reallocated;
          2.3.  device memory has been allocated with the right size for the full chain.

    ***********************************************************/

    /***** Check whether to save the old MemInfo, which is needed in CASE 2 *****/
    for (ui32Face = 0; ui32Face < ui32MaxFace; ui32Face++)
    {
		ui32BaseLevel = ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
		psLevel = &psTex->psMipLevel[ui32BaseLevel];
	

		/* CASE 0: texture level with NULL data */
		if (psLevel->pui8Buffer == IMG_NULL)
		{
			SetError(gc, GL_OUT_OF_MEMORY);
			return IMG_FALSE;
		}


		/* CASE 1: base level has not been uploaded */
		if (psLevel->pui8Buffer != GLES2_LOADED_LEVEL)
		{
			/*case 1.1: device memory has not been allocated at all */
			if (!psTex->psMemInfo)
			{
				break;
			}
			
			/* case 1.2: device memory has already been allocated, but with wrong size, so needs to be reallocated */
			if ((psTex->psMemInfo) && (bUnloadTex))
			{
				/* Unload the texture data back to local buffer, flush and ghost if necessary */
				UnloadInconsistentTexture(gc, psTex);
		
				break;
			}
			
			/* case 1.3: device memory has been allocated with the suitable size for all the levels */
			if ((psTex->psMemInfo) && (!bUnloadTex))
			{
				/* Render to mipmap before copying it back,
				   need to WAIT_FOR_3D, since ghost should happen later on,
				   although the later HWTQ TexUpload is done by HW sequentially */
				FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psLevel,
										GLES2_SCHEDULE_HW_WAIT_FOR_3D | GLES2_SCHEDULE_HW_LAST_IN_SCENE);
			}
		}
		
		/* CASE 2: base level has already been uploaded to device memory */
		/* (only allocated for base level) */
		if (psLevel->pui8Buffer == GLES2_LOADED_LEVEL)
		{
			/* case 2.1: device memory does not exist, return false */
			if (!psTex->psMemInfo)
			{
				PVR_DPF((PVR_DBG_ERROR,"HardwareMakeTextureMipmapLevels: the loaded memory is null"));
				return IMG_FALSE;
			}
	    
			/* case 2.2: device memory has already been allocated, but with wrong size, so needs to be reallocated */
			if (bUnloadTex)
			{
				/* Unload the texture data back to local buffer, flush and ghost if necessary
				   now psLevel->pui8Buffer != GLES1_LOADED_LEVEL, 
				   AND CASE 2.2 becomes the same as CASE 1.2 */
				UnloadInconsistentTexture(gc, psTex);
				
				break; 
			}
			
			/* case 2.3: device memory has been allocated with the right size for the full chain */
			if (!bUnloadTex)
			{
				/* Render to mipmap before copying it back,
				   need to WAIT_FOR_3D, since ghost could happen later on,
				   although the later HWTQ TexUpload is done by HW sequentially */
				FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psLevel,
										GLES2_SCHEDULE_HW_WAIT_FOR_3D | GLES2_SCHEDULE_HW_LAST_IN_SCENE);
			}
		}
    }

    /*********** Check whether to ghost texture, which may still be needed for CASES 1 & 2 ***********/
    if (psTex->psMemInfo)
    {
		if(KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource))
		{
			sMemInfo = *psTex->psMemInfo;
			
			if(!TexMgrGhostTexture(gc, psTex))
			{
				PVR_DPF((PVR_DBG_ERROR,"HardwareMakeTextureMipmapLevels: Can't ghost the texture"));
				return IMG_FALSE;
			}
		}
    }

    /***** Create new texture device memory if necessary *****/
    if (!psTex->psMemInfo)
    {
		/* Create texture device memory */
		if(!CreateTextureMemory(gc, psTex))
		{
			SetError(gc, GL_OUT_OF_MEMORY);
			return IMG_FALSE;
		}
		
		/* Setup twiddle functions */
		SetupTwiddleFns(psTex);
		
		/* Reset texture data address as new device memory */
		psParams->aui32StateWord2[0] = 
			(psTex->psMemInfo->sDevVAddr.uiAddr >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;
		
		for(i = 1; i < psTex->psFormat->ui32NumChunks; i++)
		{
			psParams->aui32StateWord2[i] = 
				((psTex->psMemInfo->sDevVAddr.uiAddr + (psTex->ui32ChunkSize * i))
				 >> EURASIA_PDS_DOUTT2_TEXADDR_ALIGNSHIFT) << EURASIA_PDS_DOUTT2_TEXADDR_SHIFT;
		}
		
		psTex->bResidence = IMG_FALSE;
    }
	

    /***** HW Mipmap Generation *****/
    for (ui32Face = 0; ui32Face < ui32MaxFace; ui32Face++)
    {
		ui32Lod = 0;
		ui32BaseLevel = ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
		
		psLevel = &psTex->psMipLevel[ui32BaseLevel];
		psFormat = psLevel->psTexFormat;
		
		ui32BytesPerPixel = psFormat->ui32TotalBytesPerTexel;
#if defined(DEBUG)
		ui32ImageSize = psLevel->ui32Width * psLevel->ui32Height * ui32BytesPerPixel;
#endif
		
		/* Calculate ui32OffsetInBytes */
		ui32OffsetInBytes = ui32BytesPerPixel * GetMipMapOffset(ui32Lod, ui32TopUsize, ui32TopVsize);
		
		if(psTex->ui32TextureTarget == GLES2_TEXTURE_TARGET_CEM)
		{
			IMG_UINT32 ui32FaceOffset = 
				ui32BytesPerPixel * GetMipMapOffset(psTex->ui32NumLevels, ui32TopUsize, ui32TopVsize);
			
			if(psTex->ui32HWFlags & GLES2_MIPMAP)
			{
				if(((ui32BytesPerPixel == 1) && (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_8BPP)) ||
				   (ui32TopUsize > EURASIA_TAG_CUBEMAP_NO_ALIGN_SIZE_16_32BPP))
				{
					ui32FaceOffset = ALIGNCOUNT(ui32FaceOffset, EURASIA_TAG_CUBEMAP_FACE_ALIGN);
				}
			}
			ui32OffsetInBytes += (ui32FaceOffset * ui32Face);
		}
		
		
#if defined(DEBUG)
		if(ui32ImageSize + ui32OffsetInBytes > psTex->psMemInfo->uAllocSize)
		{
			PVR_DPF((PVR_DBG_ERROR,"HardwareMakeTextureMipmapLevels: Memory calculation error somewhere"));
			return IMG_FALSE;
		}
#endif
		
		/* CASE 1.1, 1.2, 1.3, 2.2: Base level has NOT been uploaded */
		if (psLevel->pui8Buffer != GLES2_LOADED_LEVEL)
		{
			SGX_QUEUETRANSFER sQueueTransfer;
			
			if (PrepareHWTQTextureUpload(gc, psTex, ui32OffsetInBytes, psLevel,
										 IMG_NULL, IMG_NULL, 0, IMG_NULL, &sQueueTransfer))
			{
				/* Upload base level to new device memory */
				if (HWTQTextureUpload(gc, psTex, &sQueueTransfer))
				{
					bHWTexUploaded = IMG_TRUE;
					
					GLES2FreeAsync(gc, psLevel->pui8Buffer);
					
					psLevel->pui8Buffer = GLES2_LOADED_LEVEL;
				}
				else
				{
failed_hwmipgen:
					/* Copy the data in the old MemInfo to the new MemInfo, in case of any later operation on MemInfo */
					if (sMemInfo.uAllocSize)
					{
						IMG_UINT32          ui32Level;
						GLES2MipMapLevel   *psMipLevel;
						IMG_BOOL            bCopyTexture = IMG_FALSE;
						
						for (i = 0; i < ui32MaxFace; i++)
						{
							for (j = 0; j < GLES2_MAX_TEXTURE_MIPMAP_LEVELS; j++)
							{
								ui32Level = (i * GLES2_MAX_TEXTURE_MIPMAP_LEVELS) + j;
								psMipLevel = &psTex->psMipLevel[ui32Level];
								
								if (psMipLevel->pui8Buffer == GLES2_LOADED_LEVEL)
								{
									bCopyTexture = IMG_TRUE;
								}
								
								if (psMipLevel->ui32Width == 1 && psMipLevel->ui32Height == 1)
								{
									break;
								}
							}
						}
						
						if (bCopyTexture)
						{
							CopyTextureData(gc, psTex, 0, &sMemInfo, 0, sMemInfo.uAllocSize);
						}
					}			
					
					return IMG_FALSE;
				}
			}
			else
			{
				goto failed_hwmipgen;
			}
			
			
			/* Generate mipmap levels */
			if (bHWTexUploaded &&
				HardwareMipGen(gc, psTex, ui32OffsetInBytes, psLevel, &ui32Lod))
			{
				bHWTexMipGened = IMG_TRUE;
			}
			else
			{
				goto failed_hwmipgen;
			}
		}
		/* CASE 2.3: Base level has already been uploaded to device memory (allocated for the full chain)  */
		else if (psLevel->pui8Buffer == GLES2_LOADED_LEVEL)
		{
			/* If the texture has been ghosted, we need to copy the old texture to the new, to ensure we have
			   the level 0 to generate the mipsmaps from */
			if (sMemInfo.uAllocSize)
			{
				CopyTextureData(gc, psTex, 0, &sMemInfo, 0, sMemInfo.uAllocSize);
			}
			/* Generate mipmap levels */
			if (HardwareMipGen(gc, psTex, ui32OffsetInBytes, psLevel, &ui32Lod))
			{
				bHWTexMipGened = IMG_TRUE;
			}
			else
			{
				goto failed_hwmipgen;
			}			  
		}
		
    }
    
    /* Free SW local memory and reset SW structure of texture levels */
    if (bHWTexMipGened)
    {
		for (ui32Face = 0; ui32Face < ui32MaxFace; ui32Face++)
		{
			ui32BaseLevel = ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
			
			psLevel = &psTex->psMipLevel[ui32BaseLevel];
			psFormat = psLevel->psTexFormat;
			
			ui32BytesPerPixel = psFormat->ui32TotalBytesPerTexel;
			
			for (i = 0; i < GLES2_MAX_TEXTURE_MIPMAP_LEVELS; i++)
			{
				psLevel = &psTex->psMipLevel[ui32BaseLevel + i];
				
				/* The local data copy of the texture level is being freed */
				if ((psLevel->pui8Buffer != IMG_NULL) && (psLevel->pui8Buffer != GLES2_LOADED_LEVEL)) 
				{
					GLES2FreeAsync(gc, psLevel->pui8Buffer);
				}
				
				if (i == 0)
				{
					/* Setup local data pointer for level 0 */
					psLevel->pui8Buffer		  = GLES2_LOADED_LEVEL;
				}	
				else if (i < psTex->ui32NumLevels)
				{
					/* Reset SW structure for level 1 --- (ui32NumLevels-1) */
					psLevel->pui8Buffer		  = GLES2_LOADED_LEVEL;
					psLevel->ui32Width		  = MAX(psTex->psMipLevel[ui32BaseLevel].ui32Width  >> i, 1);
					psLevel->ui32Height		  = MAX(psTex->psMipLevel[ui32BaseLevel].ui32Height >> i, 1);
					
					psLevel->ui32ImageSize 	  = ui32BytesPerPixel * (psLevel->ui32Width) * (psLevel->ui32Height);
					psLevel->ui32WidthLog2	  = FloorLog2(psLevel->ui32Width);
					psLevel->ui32HeightLog2	  = FloorLog2(psLevel->ui32Height);
					
					psLevel->psTexFormat	  = psFormat;
					psLevel->eRequestedFormat = psTex->psMipLevel[ui32BaseLevel].eRequestedFormat;
					
					psLevel->ui32Level        = ui32BaseLevel + i;
					psLevel->psTex            = psTex;
				}
				else if (i >= psTex->ui32NumLevels)
				{
					/* Initialise SW structure for levels above (ui32NumLevels-1) */
					psLevel->pui8Buffer		  = IMG_NULL;
					psLevel->ui32Width		  = 0;
					psLevel->ui32Height		  = 0;
					
					psLevel->ui32ImageSize 	  = 0;
					psLevel->ui32WidthLog2	  = 0;
					psLevel->ui32HeightLog2	  = 0;
					
					psLevel->psTexFormat	  = IMG_NULL;
					psLevel->eRequestedFormat = 1;
					
					psLevel->ui32Level        = 0;
					psLevel->psTex            = psTex;
				}
			}
		}
	
		psTex->ui32LevelsConsistent = GLES2_TEX_UNKNOWN;
		
		psTex->bResidence = IMG_TRUE;
		
    }
    
    return IMG_TRUE;
	
}

IMG_VOID MakeTextureMipmapLevelsSoftware(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32Face, IMG_UINT32 ui32MaxFace, IMG_BOOL bIsNonPow2)
{
	IMG_UINT32               ui32Bpp, ui32Width, ui32Height, ui32XScale, ui32YScale, ui32BaseLevel;
	IMG_UINT32               ui32Lod = 0;
	GLES2MipMapLevel         *psSrcLevel, *psDstLevel, *psLevel;
	GLES2TextureFormat       *psFormat;
	MapInfo                  psSrcMap, psDstMap;
	TexelFormat              sTexelFormat;
	IMG_BOOL                 bWasSrcLevelReadBack;
	IMG_UINT8                *pui8Dest;

	for (ui32Face = 0; ui32Face < ui32MaxFace; ui32Face++)
	{
		ui32BaseLevel = ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
		psLevel = &psTex->psMipLevel[ui32BaseLevel];

		psFormat = psLevel->psTexFormat;
		ui32Bpp = psFormat->ui32TotalBytesPerTexel;


		/* Set masks and shifts depending on the texture format */
		switch (psFormat->ePixelFormat)
		{
			/* 8-bit and 32-bit texel formats */
		case PVRSRV_PIXEL_FORMAT_ARGB8888:
		case PVRSRV_PIXEL_FORMAT_ABGR8888:
		case PVRSRV_PIXEL_FORMAT_A8:
		case PVRSRV_PIXEL_FORMAT_L8:
		{
			/* sTexFormat is never used for 8-bit and 32-bit texel formats */
			/* We assign it here to silence the compiler */
			sTexelFormat.ui32BlueMask = 0x0000;
			sTexelFormat.ui32GreenMask = 0x0000;
			sTexelFormat.ui32RedMask = 0x0000;
			sTexelFormat.ui32AlphaMask = 0x0000;
			break;
		}
		/* 16-bit per texel formats */
		case PVRSRV_PIXEL_FORMAT_A8L8:
		{
			sTexelFormat.ui32BlueMask = 0x0000;
			sTexelFormat.ui32GreenMask = 0x0000;
			sTexelFormat.ui32RedMask = 0x00ff;
			sTexelFormat.ui32AlphaMask = 0xff00;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB1555:
		{
			sTexelFormat.ui32BlueMask = 0x001f;
			sTexelFormat.ui32GreenMask = 0x03e0;
			sTexelFormat.ui32RedMask = 0x7c00;
			sTexelFormat.ui32AlphaMask = 0x8000;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_ARGB4444:
		{
			sTexelFormat.ui32BlueMask = 0x000f;
			sTexelFormat.ui32GreenMask = 0x00f0;
			sTexelFormat.ui32RedMask = 0x0f00;
			sTexelFormat.ui32AlphaMask = 0xf000;
			break;
		}
		case PVRSRV_PIXEL_FORMAT_RGB565:
		{
			sTexelFormat.ui32BlueMask = 0x001f;
			sTexelFormat.ui32GreenMask = 0x07e0;
			sTexelFormat.ui32RedMask = 0xf800;
			sTexelFormat.ui32AlphaMask = 0x0000;
			break;
		}
		/* Unknown format */
		default:
		{
			PVR_DPF((PVR_DBG_ERROR, "MakeTextureMipmapLevels: Unknown texture format ! "));
			return IMG_FALSE;
		}
		}

		ui32XScale = 2;
		ui32YScale = 2;

		for (ui32Lod = ui32BaseLevel + 1; ui32Lod < ui32BaseLevel + GLES2_MAX_TEXTURE_MIPMAP_LEVELS; ui32Lod++)
		{
			psSrcLevel = &psTex->psMipLevel[ui32Lod - 1];	/* source level */
			psDstLevel = &psTex->psMipLevel[ui32Lod];		/* target level */

			ui32Width = psSrcLevel->ui32Width >> 1;
			ui32Height = psSrcLevel->ui32Height >> 1;

#if defined(GLES2_EXTENSION_NPOT)
			if (bIsNonPow2 && ((ui32Width < MIN_POW2_MIPLEVEL_SIZE) || (ui32Height < MIN_POW2_MIPLEVEL_SIZE)))
			{
				if (psDstLevel->pui8Buffer && psDstLevel->pui8Buffer != GLES2_LOADED_LEVEL)
				{
#if (defined(DEBUG) || defined(TIMING))
					ui32TextureMemCurrent -= psDstLevel->ui32ImageSize;
#endif
					GLES2FreeAsync(gc, psDstLevel->pui8Buffer);
				}
				psDstLevel->pui8Buffer = IMG_NULL;
			}
			else
#endif /* defined(GLES2_EXTENSION_NPOT) */
			{
				if (ui32Width < 1)
				{
					ui32Width = 1;
					ui32XScale = 1;
				}

				if (ui32Height < 1)
				{
					ui32Height = 1;
					ui32YScale = 1;
				}

				pui8Dest = TextureCreateLevel(gc, psTex, ui32Lod, psSrcLevel->eRequestedFormat,
					psSrcLevel->psTexFormat, ui32Width, ui32Height);

				psSrcMap.ui32Stride = psSrcLevel->ui32Width * ui32Bpp;
				psSrcMap.ui32Width = psSrcLevel->ui32Width;
				psSrcMap.ui32Height = psSrcLevel->ui32Height;

#if defined(GLES2_EXTENSION_NPOT)
				/* Make sure that source size is even */
				if (ui32XScale == 2)
				{
					psSrcMap.ui32Width &= 0xFFFFFFFE;
				}
				if (ui32YScale == 2)
				{
					psSrcMap.ui32Height &= 0xFFFFFFFE;
				}
#endif /* defined(GLES2_EXTENSION_NPOT) */

				/* Handle generating mipmaps of levels that have been already loaded */
				if (psSrcLevel->pui8Buffer == GLES2_LOADED_LEVEL)
				{
					/* The level was loaded to device memory. Read it back and then do the mipmaps */
					/* This code was  copied from glTexSubImage */
					IMG_UINT32 ui32BufferSize = psSrcMap.ui32Stride * psSrcMap.ui32Height;
					IMG_UINT8  *pui8Src = GLES2MallocHeapUNC(gc, ui32BufferSize);

					if (!pui8Src)
					{
						SetError(gc, GL_OUT_OF_MEMORY);

						return IMG_FALSE;
					}

#if (defined(DEBUG) || defined(TIMING))
					ui32TextureMemCurrent += ui32BufferSize;

					if (ui32TextureMemCurrent > ui32TextureMemHWM)
					{
						ui32TextureMemHWM = ui32TextureMemCurrent;
					}
#endif	
					/* Render to mipmap before reading it back */
					FlushAttachableIfNeeded(gc, (GLES2FrameBufferAttachable*)psSrcLevel,
						GLES2_SCHEDULE_HW_LAST_IN_SCENE | GLES2_SCHEDULE_HW_WAIT_FOR_3D);

					ReadBackTextureData(gc, psTex, ui32Face, ui32Lod - 1 - ui32BaseLevel, pui8Src);

					psSrcLevel->pui8Buffer = pui8Src;

					bWasSrcLevelReadBack = IMG_TRUE;
				}
				else
				{
					bWasSrcLevelReadBack = IMG_FALSE;
				}

				psSrcMap.pvBuffer = psSrcLevel->pui8Buffer;

				psDstMap.ui32Width = psDstLevel->ui32Width;
				psDstMap.ui32Height = psDstLevel->ui32Height;
				psDstMap.ui32Stride = psDstLevel->ui32Width * ui32Bpp;
				psDstMap.pvBuffer = psDstLevel->pui8Buffer;

				/* generate a lower resolution map */
				if (pui8Dest)
				{
					switch (ui32Bpp)
					{
					case 1:
					{
						MakeMapLevel8bpp(&psSrcMap, &psDstMap, ui32XScale, ui32YScale);
						break;
					}
					case 2:
					{
						MakeMapLevel16bpp(&psSrcMap, &psDstMap, ui32XScale, ui32YScale, &sTexelFormat);
						break;
					}
					case 4:
					{
						MakeMapLevel32bpp(&psSrcMap, &psDstMap, ui32XScale, ui32YScale);
						break;
					}
					default:
					{
						/* Should never happen */
						PVR_DPF((PVR_DBG_ERROR, "MakeTextureMipmapLevels: Unknown texture format ! "));
						return IMG_FALSE;
					}
					}
				}

				/* Free memory if we read back the source level */
				if (bWasSrcLevelReadBack)
				{
					GLES2FreeAsync(gc, psSrcLevel->pui8Buffer);

					psSrcLevel->pui8Buffer = GLES2_LOADED_LEVEL;
				}
			}

			if (ui32Width == 1 && ui32Height == 1)
			{
				/* Do not continue after we compute a mipmap of 1x1 texels */
				break;
			}
		}
	}

	/* Mark texture as non-resident since we have generated/overwritten some mipmap levels */
	psTex->bResidence = IMG_FALSE;

	/* Update NumLevels to reflect the mipmaps that have been just created */
	psTex->ui32NumLevels = (ui32Lod + 1) % GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
}


/***********************************************************************************
 Function Name      : MakeTextureMipmapLevels
 Inputs/Outputs		: tex - pointing to tex structure
 Inputs             : face - the face number if this texture is a cube map. 0 otherwise
 Returns            : IMG_TRUE if successfull. IMG_FALSE otherwise.
 Description        : Generate a full chain of mipmaps from the base level tex data.
 Limitations        : We only support power-of-two textures. That's all the spec requires.
************************************************************************************/
IMG_BOOL IMG_INTERNAL MakeTextureMipmapLevels(GLES2Context *gc, GLES2Texture *psTex, IMG_UINT32 ui32TexTarget)
{
	IMG_UINT32               ui32Lod = 0;
	GLES2MipMapLevel         *psSrcLevel, *psDstLevel, *psLevel;
	const GLES2TextureFormat *psFormat;
	IMG_UINT32               ui32Bpp, ui32Width, ui32Height, ui32XScale, ui32YScale;
	IMG_UINT32               ui32BaseLevel, ui32MaxFace, ui32Face;
	IMG_UINT8                *pui8Dest;
	MapInfo                  psSrcMap, psDstMap;
	TexelFormat              sTexelFormat;
	IMG_BOOL                 bWasSrcLevelReadBack;
#if defined(GLES2_EXTENSION_NPOT)
	IMG_BOOL                 bIsNonPow2;
#endif /* defined(GLES2_EXTENSION_NPOT) */
	IMG_BOOL                 bHWMipGen = IMG_FALSE;


	GLES_ASSERT(psTex);

	switch(ui32TexTarget)
	{
	    case GLES2_TEXTURE_TARGET_2D:
		{
			ui32MaxFace = 1;
			break;
		}
	    case GLES2_TEXTURE_TARGET_CEM:
		{
			ui32MaxFace = GLES2_TEXTURE_CEM_FACE_MAX;
			break;
		}
	    default:
		{
			return IMG_FALSE; /* Unsupported target */
		}
	}

	psLevel = &psTex->psMipLevel[0];

#if defined(GLES2_EXTENSION_NPOT)

	if (((psLevel->ui32Width & (psLevel->ui32Width-1)) != 0) || ((psLevel->ui32Height & (psLevel->ui32Height-1)) != 0))
	{
		bIsNonPow2 = IMG_TRUE;
	}
	else
	{
		bIsNonPow2 = IMG_FALSE;
	}

#else /* defined(GLES2_EXTENSION_NPOT) */

	if (((psLevel->ui32Width & (psLevel->ui32Width-1)) != 0) || ((psLevel->ui32Height & (psLevel->ui32Height-1)) != 0))
	{
		return IMG_FALSE;
	}

#endif /* defined(GLES2_EXTENSION_NPOT) */

	/* Nothing to do for 1x1 base level */
	if(psLevel->ui32Width == 1 && psLevel->ui32Height == 1)
		return IMG_TRUE;

	/* HW mipmap gen is implemented */

	if (!gc->sAppHints.bDisableHWTQMipGen &&
#if defined(GLES2_EXTENSION_NPOT)
		!bIsNonPow2 &&
#endif
		((psTex->ui32HWFlags & GLES2_NONPOW2) == 0) &&
		((psTex->ui32HWFlags & GLES2_COMPRESSED) == 0) &&
		HardwareMakeTextureMipmapLevels(gc, psTex, ui32TexTarget))
	{
		bHWMipGen = IMG_TRUE;
	}


	/* If the texture is active and it's already been loaded, we need to ghost it before overwriting it.
	 * except if the texture had a single mipmap level. In that case we are simply addig more levels, not
	 * changing the existing one.
	 */
	if(psTex->psMemInfo && !psTex->bResidence &&
	   KRM_IsResourceNeeded(&gc->psSharedState->psTextureManager->sKRM, &psTex->sResource) &&
	   (psTex->ui32NumLevels > 1))
	{
		TexMgrGhostTexture(gc, psTex);
	}

	/* Check Base Level textures for each face */
	for (ui32Face = 0; ui32Face < ui32MaxFace; ui32Face++)
	{
		ui32BaseLevel = ui32Face * GLES2_MAX_TEXTURE_MIPMAP_LEVELS;
		
		psLevel = &psTex->psMipLevel[ui32BaseLevel];
		
		if(!psLevel)
		{
			PVR_DPF((PVR_DBG_ERROR, "MakeTextureMipmapLevels: Base level mipmap is NULL"));

			return IMG_FALSE;
		}

		psFormat = psLevel->psTexFormat;

		if(!psFormat)
		{
			PVR_DPF((PVR_DBG_ERROR, "MakeTextureMipmapLevels: The format of the base level mipmap is NULL"));

			return IMG_FALSE;
		}
	}
		

	/* SW mipmap gen is implemented, if HW mipmap gen failed. */

	if (!bHWMipGen)
	{
		if (!gc->sAppHints.bDisableAsyncTextureOp)
		{
			SWMakeTextureMipmapLevels(gc, psTex, ui32Face, ui32MaxFace, bIsNonPow2);
		}
		else
		{
			MakeTextureMipmapLevelsSoftware(gc, psTex, ui32Face, ui32MaxFace, bIsNonPow2);
		}
	}

	return IMG_TRUE;
}

/******************************************************************************
 End of file (makemips.c)
******************************************************************************/
