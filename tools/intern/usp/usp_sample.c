 /*****************************************************************************
 Name           : USP_SAMPLE.C

 Title          : Texture-sample handling

 C Author       : Joe Molleson

 Created        : 02/01/2002

 Copyright      : 2002-2010 by Imagination Technologies Limited.
                : All rights reserved. No part of this software, either
                : material or conceptual may be copied or distributed,
                : transmitted, transcribed, stored in a retrieval system or
                : translated into any human or computer language in any form
                : by any means, electronic, mechanical, manual or otherwise,
                : or disclosed to third parties without the express written
                : permission of Imagination Technologies Limited,
                : Home Park Estate, Kings Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description    : Routines to handle fixing-up of dependent and non-dependent
                  samples upon finalising a shader.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.162 $

 Modifications :
 $Log: usp_sample.c $
******************************************************************************/
#include <string.h>
#include "img_types.h"
#include "sgxdefs.h"
#include "usp_typedefs.h"
#include "usc.h"
#include "usp.h"
#include "hwinst.h"
#include "uspbin.h"
#include "uspshrd.h"
#include "usp_sample.h"
#include "usp_instblock.h"
#include "usp_inputdata.h"

#if defined(SGX_FEATURE_USE_VEC34)
#define NUM_INTERNAL_REGISTERS				(12)
#else
#define NUM_INTERNAL_REGISTERS_NON_VEC		(3)
#endif /* defined(SGX_FEATURE_USE_VEC34) */
#define USP_RGB_CHAN_MASK					(7)

/*****************************************************************************
 Name:		CalcTexChanBitCountAndFmt

 Purpose:	Get the number of bits for a texture channel, and the format
			of the sampled data

 Inputs:	psTexForm			- The format of the texture being sampled
			uChanIdx			- Index of the channel of interest
			bGammaEnabled		- Whether gamma correction is enabled

Outputs:	puNumBitsInTex		- The number of bits for the channel
								  within the texture
			puNumBitsInReg		- The number of bits for the channel
								  within the resulting sampled data
			peChanFormInReg		- The format in which the sampled channel
								  will be stored

 Returns:	none
*****************************************************************************/
static IMG_VOID CalcTexChanBitCountAndFmt(PUSP_TEX_CHAN_FORMAT	psTexForm,
										  IMG_UINT32			uChanIdx,
										  IMG_BOOL				bGammaEnabled,
										  IMG_PUINT32			puNumBitsInTex,
										  IMG_PUINT32			puNumBitsInReg,
										  PUSP_CHAN_FORMAT		peChanFormInReg)
{
	static const IMG_UINT32 auChanFormBitCount[] =
	{
		8,	/* USP_CHAN_FORMAT_U8 */
		8,	/* USP_CHAN_FORMAT_S8 */
		16,	/* USP_CHAN_FORMAT_U16 */
		16,	/* USP_CHAN_FORMAT_S16 */
		16,	/* USP_CHAN_FORMAT_F16 */
		32,	/* USP_CHAN_FORMAT_F32 */
		24, /* USP_CHAN_FORMAT_U24 */
		32, /* USP_CHAN_FORMAT_U32 */
		32, /* USP_CHAN_FORMAT_I32 */
	};
	USP_CHAN_FORMAT	eChanForm;
	IMG_UINT32		uChanBitCount;

	/*
		Get the number of bits needed for the sampled channel within the texture
	*/
	eChanForm		= psTexForm->aeChanForm[uChanIdx];
	uChanBitCount	= auChanFormBitCount[eChanForm];

	if	(puNumBitsInTex)
	{
		*puNumBitsInTex = uChanBitCount;
	}

	/*
		Get the number of bits needed for the sampled channel data within the
		destination register

		NB: Currently, only U8 format data can be gamma corrected, in which
			case it is output from the SMP instruction as 16-bit.
	*/
	if	(bGammaEnabled)
	{
		if	(eChanForm == USP_CHAN_FORMAT_U8)
		{
			eChanForm		= USP_CHAN_FORMAT_U16;
			uChanBitCount	= 16;
		}
	}

	if	(puNumBitsInReg)
	{
		*puNumBitsInReg = uChanBitCount;
	}

	if	(peChanFormInReg)
	{
		*peChanFormInReg = eChanForm;
	}
}

/*****************************************************************************
 Name:		IsChanSwizzleRequired

 Purpose:	Checks if the swizzle can be ignored. For some texture formats 
			that are having same components replicated in different channels 
			swizzles can be ignored.

 Inputs:	psContext		- The current USP execution context
			psSample		- The sample to check swizzle ignore option for
			psSamplerDesc	- The channel data for the texture
			uChannel		- The channel to check swizzle ignore option for
			uNewChannel     - The new channel to use instead

 Returns:	IMG_TRUE/IMG_FALSE if swizzle can be ignored or not.
*****************************************************************************/
static IMG_BOOL IsChanSwizzleRequired(PUSP_CONTEXT				psContext, 
									  PUSP_SAMPLE				psSample,
									  PUSP_SAMPLER_CHAN_DESC	psSamplerDesc,
									  IMG_UINT32				uChannel,
									  IMG_UINT32				uNewChannel)
{
	IMG_BOOL bSwizRequired = IMG_TRUE;
	IMG_UINT32 uWord0;

	USP_TEXTURE_FORMAT  eTexFmt = 
		psContext->asSamplerDesc[psSample->uTextureIdx].sTexFormat.eTexFmt;

	#if defined(SGX545)
	IMG_BOOL bExtFmt0;
	IMG_BOOL bExtFmt1;
	#endif /* defined(SGX545) */

	PVR_UNREFERENCED_PARAMETER(psSamplerDesc);

	#if defined(SGX545)
	PVR_UNREFERENCED_PARAMETER(eTexFmt);
	#else
	#if !defined(SGX_FEATURE_TAG_SWIZZLE)
	PVR_UNREFERENCED_PARAMETER(uChannel);
	PVR_UNREFERENCED_PARAMETER(uNewChannel);
	#endif
	#endif

	uWord0 = 
		psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auWords[0];

	#if defined(SGX545)
	bExtFmt0 = 
		psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].abExtFmt[0];
	bExtFmt1 = 
		psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].abExtFmt[1];
	#endif /* defined(SGX545) */

	#if defined(SGX545)
	if(bExtFmt0)
	{
		if((uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_A8) || 
		   (uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_L8) || 
		   (uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_L8A8) ||
		   (uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_L_F16_REP) || 
		   (uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_L_F16_A_F16))
		{
			if(((USP_CHAN_SWIZZLE)uChannel<USP_CHAN_SWIZZLE_CHAN_3) 
				&& 
			   ((USP_CHAN_SWIZZLE)uNewChannel<USP_CHAN_SWIZZLE_CHAN_3))
			{
				bSwizRequired = IMG_FALSE;
			}
		}
	}
	#else
	#if defined(SGX_FEATURE_TAG_SWIZZLE)
	if(((uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_U8) && (eTexFmt == USP_TEXTURE_FORMAT_A8)) || 
	   ((uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_U8) && (eTexFmt == USP_TEXTURE_FORMAT_L8)) || 
	   ((uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_U88) && (eTexFmt == USP_TEXTURE_FORMAT_L8A8)) || 
	   ((uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_F16) && (eTexFmt == USP_TEXTURE_FORMAT_AF16)) ||
	   ((uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_F16) && (eTexFmt == USP_TEXTURE_FORMAT_LF16)) ||
	   ((uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616) && (eTexFmt == USP_TEXTURE_FORMAT_LF16AF16)))
	{
		if(((USP_CHAN_SWIZZLE)uChannel<USP_CHAN_SWIZZLE_CHAN_3) 
			&& 
		   ((USP_CHAN_SWIZZLE)uNewChannel<USP_CHAN_SWIZZLE_CHAN_3))
		{
			bSwizRequired = IMG_FALSE;
		}
	}
	else if((uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_U8) && 
		(eTexFmt == USP_TEXTURE_FORMAT_I8))
	{
		bSwizRequired = IMG_FALSE;
	}
	else if((uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_F16) && 
		(eTexFmt == USP_TEXTURE_FORMAT_IF16))
	{
		bSwizRequired = IMG_FALSE;
	}
	#endif
	#endif

	#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
	if((uWord0 == EURASIA_PDS_DOUTT1_TEXFORMAT_U8) && 
	   (eTexFmt == USP_TEXTURE_FORMAT_I8))
	{
		bSwizRequired = IMG_FALSE;
	}
	#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

	return bSwizRequired;
}

/*****************************************************************************
 Name:		GetTexChanInfo

 Purpose:	Calculate offsets and indices representing the location of
			the channels of a texture that need to be sampled

 Inputs:	psContext			- The current USP execution context
			psSample			- The USP sample to get texture channel
								  information for
			psSamplerDesc		- Data describing the texture being sampled

 Outputs:	psTexChanInfo		- Structure to fill out describing the
								  texture channels to be sampled.

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
static IMG_BOOL GetTexChanInfo(PUSP_CONTEXT				psContext,
							   PUSP_SAMPLE				psSample,
							   PUSP_SAMPLER_CHAN_DESC	psSamplerDesc,
							   PUSP_TEX_CHAN_INFO		psTexChanInfo)
{
	PUSP_TEX_CHAN_FORMAT	psTexForm;
	USP_TEX_TYPE	eTexType;
	IMG_UINT32		uUYVTexChanMask;
	IMG_BOOL		bGammaEnabled;
	IMG_UINT32		uBitsPerChunk;
	IMG_UINT32		uBitsPerReg;
	IMG_UINT32		uDestRegBitsStart;
	IMG_UINT32		uDestRegBitsEnd;
	IMG_UINT32		uChanBitsStart;
	IMG_UINT32		uChanBitsEnd;
	IMG_UINT32		uRegCount;
	IMG_UINT32		uPrevChunkIdx;
	IMG_UINT32		uTexChanSampleMask;
	IMG_UINT32		i;
	IMG_UINT32		uSampleMask;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	/*
		Lookup the format of the texture being sampled
	*/
	psTexForm		= &psSamplerDesc->sTexChanFormat;
	bGammaEnabled	= psSamplerDesc->bGammaEnabled;

	/*
		Detect YUV textures
	*/
	uUYVTexChanMask = 0;

	for	(i = 0; i < USP_MAX_SAMPLE_CHANS; i++)
	{
		USP_CHAN_CONTENT	eChanContent;

		eChanContent = psTexForm->aeChanContent[i];
		if	(
				(eChanContent == USP_CHAN_CONTENT_Y) ||
				(eChanContent == USP_CHAN_CONTENT_U) ||
				(eChanContent == USP_CHAN_CONTENT_V)
			)
		{
			USP_CHAN_FORMAT		eChanFormat;

			eChanFormat = psTexForm->aeChanForm[i];
			if	(
					(eChanFormat != USP_CHAN_FORMAT_U8) &&
					(eChanFormat != USP_CHAN_FORMAT_S8) &&
					(eChanFormat != psTexForm->aeChanForm[0])
				)
			{
				USP_DBGPRINT(("GetTexChanInfo: Unsupported UYV format: all channels must be U8, S8 or O8\n"));
				return IMG_FALSE;
			}

			uUYVTexChanMask |= 1 << i;
		}
	}

	if	(uUYVTexChanMask == 0)
	{
		eTexType = USP_TEX_TYPE_NORMAL;
	}
	else if	(uUYVTexChanMask == 0x7)
	{
		eTexType = USP_TEX_TYPE_YUV;
	}
	else
	{
		/*
			For UYV textures, all 3 channels must be present in the texture for CSC
		*/
		USP_DBGPRINT(("GetTexChanInfo: Invalid YUV texture-format content: Not all UYV channels present\n"));
		return IMG_FALSE;
	}

	psTexChanInfo->eTexType = eTexType;

	/*
		Determine which channels of the texture need to be sampled,
		corresponding to the destination components for this sample.
	*/
	uTexChanSampleMask = 0;

	if(psSample->bSamplerUnPacked)
	{
		uSampleMask = psSample->uUnpackedMask;
	}
	else
	{
		uSampleMask = psSample->psSampleUnpack->uMask;
	}

	for	(i = 0; i < USP_MAX_SAMPLE_CHANS; i++)
	{
		if	(uSampleMask & (1 << i))
		{
			IMG_UINT32			uCompIdx;
			IMG_UINT32			uTexChanIdx;
			USP_CHAN_SWIZZLE	eTexChanSwizzle;
			USP_CHAN_CONTENT	eTexChanContent;

			/*
				Lookup the texture-channel that has to be sampled to get the
				required data

				NB: For YUV textures, this indicates how the R, G and B channels from
					the post-CSC data should be mapped to the destination components.
					This information is used to minimise unpacking code when performing
					CSC, by generating the RGB components in the required order.
			*/
			uCompIdx		= (psSample->uSwizzle >> (i*3)) & 0x7;
			eTexChanSwizzle	= psTexForm->aeChanSwizzle[uCompIdx];

			psTexChanInfo->aeTexChanForComp[i] = eTexChanSwizzle;

			/*
				Can we ignore swizzle.
			*/
			if((i!=(IMG_UINT32)eTexChanSwizzle) && (eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3))
			{
				if(!IsChanSwizzleRequired(psContext, psSample, psSamplerDesc, eTexChanSwizzle, i))
				{
					psTexChanInfo->aeTexChanForComp[i] = (USP_CHAN_SWIZZLE)i;
					eTexChanSwizzle = (USP_CHAN_SWIZZLE)i;
				}
			}

			/*
				Record a mask of which texture-channels need to be sampled
			*/
			switch (eTexChanSwizzle)
			{
				case USP_CHAN_SWIZZLE_CHAN_0:
				case USP_CHAN_SWIZZLE_CHAN_1:
				case USP_CHAN_SWIZZLE_CHAN_2:
				case USP_CHAN_SWIZZLE_CHAN_3:
				{
					/*
						For YUV textures, using the R,B or G channel data after CSC
						will require all 3 YUV channels from the texture to be sampled.

						There is no Alpha channel data generated by the CSC code, so
						it is invalid to try and use it for YUV formats.
					*/
					if	(eTexType == USP_TEX_TYPE_YUV)
					{
						if	(eTexChanSwizzle < USP_CHAN_SWIZZLE_CHAN_3)
						{
							uTexChanSampleMask |= uUYVTexChanMask;
							continue;
						}
						else
						{
							USP_DBGPRINT(("GetTexChanInfo: Attempt to use Alpha channel from YUV texture\n"));
							return IMG_FALSE;
						}
					}
					else
					{
						USP_CHAN_CONTENT	eChanContent;

						/*
							What does this channel of the sampled data contain?
						*/
						eChanContent = psTexForm->aeChanContent[eTexChanSwizzle];
						switch (eChanContent)
						{
							case USP_CHAN_CONTENT_DATA:
							{
								/*
									Real data, so definitely need to sample this channel
									of the texture.
								*/
								uTexChanIdx = eTexChanSwizzle;
								break;
							}

							case USP_CHAN_CONTENT_ONE:
							{
								/*
									If the destination or sample format is F32, source one
									from a constant rather than the sampled data
								*/
								if	(	(!(psSample->bSamplerUnPacked)) && (
										#if !defined(SGX_FEATURE_TAG_SWIZZLE)
										(psSample->psSampleUnpack->asDest[i].eFmt == USP_REGFMT_F32) ||
										#endif /* !defined(SGX_FEATURE_TAG_SWIZZLE) */
										 (psTexForm->aeChanForm[eTexChanSwizzle] == USP_CHAN_FORMAT_F32))
									)
								{
									/*
										We'll source the data from an immediate or builtin constant
										so no need to sample anything for this destination channel.
									*/
									continue;
								}
								else
								{
									/*
										Sample the one from this channel of the texture
									*/
									uTexChanIdx = eTexChanSwizzle;
								}

								break;
							}

							case USP_CHAN_CONTENT_ZERO:
							{
								/*
									If the destination or sample format is F32, source zero
									from a constant rather than the sampled data
								*/
								if	(	(!(psSample->bSamplerUnPacked)) && (
										#if !defined(SGX_FEATURE_TAG_SWIZZLE)
										(psSample->psSampleUnpack->asDest[i].eFmt == USP_REGFMT_F32) ||
										#endif /* !defined(SGX_FEATURE_TAG_SWIZZLE) */
										 (psTexForm->aeChanForm[eTexChanSwizzle] == USP_CHAN_FORMAT_F32))
									)
								{
									/*
										We'll source the data from an immediate or builtin constant
										so no need to sample anything for this destination channel.
									*/
									continue;
								}
								else
								{
									/*
										Sample the zero from this channel of the texture
									*/
									uTexChanIdx = eTexChanSwizzle;
								}

								break;
							}

							case USP_CHAN_CONTENT_NONE:
							default:
							{
								USP_DBGPRINT(("SetupChanSrcRegs: Invalid texture-format content\n"));
								return IMG_FALSE;
							}
						}
					}
					break;
				}

				case USP_CHAN_SWIZZLE_ONE:
				{
					IMG_BOOL	bUseHWConst;

					/*
						If the sampled data will contain one in any channel, use that
						rather than the builtin HW 1.0 constant (unless the sample result
						is required in F32 format, in which case the constant is a better
						idea).
					*/
					bUseHWConst = IMG_TRUE;
					uTexChanIdx = 0;

					#if !defined(SGX_FEATURE_TAG_SWIZZLE)
					if	(psSample->psSampleUnpack->asDest[i].eFmt != USP_REGFMT_F32)
					#endif /* !defined(SGX_FEATURE_TAG_SWIZZLE) */
					{
						IMG_UINT32	j;

						for	(j = 0; j < USP_MAX_SAMPLE_CHANS; j++)
						{
							if	(psTexForm->aeChanContent[j] == USP_CHAN_CONTENT_ONE)
							{
								uTexChanIdx = j;
								bUseHWConst = IMG_FALSE;
								break;
							}
						}
					}

					if	(bUseHWConst)
					{
						continue;
					}

					break;
				}

				case USP_CHAN_SWIZZLE_ZERO:
				{
					IMG_BOOL	bUseHWConst;

					/*
						If the sampled data will contain zero in any channel, use that
						rather than the builtin HW 0.0 constant (unless the sample result
						is required in F32 format, in which case the constant is a better
						idea).
					*/
					bUseHWConst = IMG_TRUE;
					uTexChanIdx = 0;

					#if !defined(SGX_FEATURE_TAG_SWIZZLE)
					if	(psSample->psSampleUnpack->asDest[i].eFmt != USP_REGFMT_F32)
					#endif /* !defined(SGX543) */
					{
						IMG_UINT32	j;

						for	(j = 0; j < USP_MAX_SAMPLE_CHANS; j++)
						{
							if	(psTexForm->aeChanContent[j] == USP_CHAN_CONTENT_ZERO)
							{
								uTexChanIdx = j;
								bUseHWConst = IMG_FALSE;
								break;
							}
						}
					}

					if	(bUseHWConst)
					{
						continue;
					}

					break;
				}

				default:
				{
					USP_DBGPRINT(("GetTexChanInfo: Invalid texture channel swizzle\n"));
					return IMG_FALSE;
				}
			}

			eTexChanContent = psTexForm->aeChanContent[uTexChanIdx];
			if	(eTexChanContent == USP_CHAN_CONTENT_NONE)
			{
				USP_DBGPRINT(("GetTexChanInfo: Attempt to sample texture channel described as USP_CHAN_CONTENT_NONE\n"));
				return IMG_FALSE;
			}

			uTexChanSampleMask |= 1 << uTexChanIdx;
		}	
	}

	psTexChanInfo->uTexChanSampleMask = uTexChanSampleMask;

	/*
		Calculate which chunks of sampled texture data each of the required
		texture-channels will come from, and where the result for each
		channel and chunk will be stored.
	*/
	uRegCount			= 0;
	uBitsPerChunk		= psTexForm->auChunkSize[0] * 8;
	uBitsPerReg			= 32;
	uChanBitsStart		= 0;
	uChanBitsEnd		= 0;
	uDestRegBitsStart	= 0;
	uDestRegBitsEnd		= 0;
	uPrevChunkIdx		= 0;

	psTexChanInfo->uTexChunkMask = 0;

	for	(i = 0; i < USP_MAX_SAMPLE_CHANS; i++)
	{
		IMG_UINT32		uChanBitCountInTex;
		IMG_UINT32		uChanBitCountInReg;
		IMG_UINT32		uChunkIdx;
		IMG_UINT32		uDestRegIdx;
		USP_CHAN_FORMAT	eChanDataFmt;

		/*
			Stop once we have calculated the data for all the texture channels we need to sample
		*/
		if	(!uTexChanSampleMask)
		{
			break;
		}

		/*
			Determine the number of bits, and data-format, for this channel
			within the texture
		*/
		CalcTexChanBitCountAndFmt(&(psSample->sSamplerDesc.sTexChanFormat), 
								  i,
								  bGammaEnabled,
								  &uChanBitCountInTex,
								  &uChanBitCountInReg,
								  &eChanDataFmt);

		/*
			Update bit-positions within a texel to account for this channel
			in the texture.
		*/
		uChanBitsStart	= uChanBitsEnd;
		uChanBitsEnd	= uChanBitsStart + uChanBitCountInTex;

		/*
			Get the chunk of texture data containing the data for this
			texture channel
		*/
		if	(
				uChanBitCountInTex > 0 &&
				(uChanBitsStart / uBitsPerChunk) !=
				((uChanBitsEnd - 1) / uBitsPerChunk)
			)
		{
			USP_DBGPRINT(("GetTexChanLocations: Channel data not within a single chunk\n"));
			return IMG_FALSE;
		}

		uChunkIdx = uChanBitsStart / uBitsPerChunk;

		/*
			Start a new destination register when we start a new chunk
		*/
		if	(uChunkIdx != uPrevChunkIdx)
		{
			if	(uDestRegBitsEnd % uBitsPerReg)
			{
				uDestRegBitsEnd += uBitsPerReg - (uDestRegBitsEnd % uBitsPerReg);
			}

			uBitsPerChunk = psTexForm->auChunkSize[uChunkIdx] * 8;
			uPrevChunkIdx = uChunkIdx;
		}

		/*
			Calculate a bit offset for this channel's data within registers
		*/
		uDestRegBitsStart	= uDestRegBitsEnd;
		uDestRegBitsEnd		= uDestRegBitsStart + uChanBitCountInReg;

		/*
			Ignore texture channels that we aren't going to sample
		*/
		if	(!(uTexChanSampleMask & (1 << i)))
		{
			continue;
		}
		uTexChanSampleMask &= ~(1 << i);

		/*
			Get the register where this channel's data should be
			stored
		*/
		if	(
				(uDestRegBitsStart / uBitsPerReg) != 
				((uDestRegBitsEnd - 1) / uBitsPerReg)
			)
		{
			USP_DBGPRINT(("GetTexChanLocations: Sampled data not within a single register\n"));
			return IMG_FALSE;
		}

		uDestRegIdx = uDestRegBitsStart / uBitsPerReg;

		/*
			Record the destination register location and format for this
			sampled texture channel.
			
			NB:	These are used later when we copy and/or convert the raw
				sampled data to the required destination for each channel.
		*/
		psTexChanInfo->auDestRegForChan[i]		= uDestRegIdx;
		psTexChanInfo->auDestRegCompForChan[i]	= (uDestRegBitsStart % uBitsPerReg) >> 3;
		psTexChanInfo->aeDataFmtForChan[i]		= eChanDataFmt;
		psTexChanInfo->auChunkIdxForChan[i]		= uChunkIdx;

		/*
			Record the register for this chunk
		*/

		#if defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554)
		if	(!(psTexChanInfo->uTexChunkMask & (1 << uChunkIdx)))
		{
			psTexChanInfo->auDestRegForChunk[uChunkIdx] = uRegCount;
			psTexChanInfo->auNumRegsForChunk[uChunkIdx] = 
				((psTexForm->auChunkSize[uChunkIdx] * 8) + uBitsPerReg - 1)/uBitsPerReg;

			psTexChanInfo->uTexChunkMask |= 1 << uChunkIdx;

			uRegCount += psTexChanInfo->auNumRegsForChunk[uChunkIdx];
		}
		#else
		if	(!(psTexChanInfo->uTexChunkMask & (1 << uChunkIdx)))
		{
			psTexChanInfo->auDestRegForChunk[uChunkIdx] = uDestRegIdx;
			psTexChanInfo->auNumRegsForChunk[uChunkIdx] = 1;

			psTexChanInfo->uTexChunkMask |= 1 << uChunkIdx;
		}
		else
		{
			IMG_UINT32	uDestRegForChunk;

			uDestRegForChunk = psTexChanInfo->auDestRegForChunk[uChunkIdx];
			psTexChanInfo->auNumRegsForChunk[uChunkIdx] = uDestRegIdx - uDestRegForChunk + 1;
		}

		uRegCount = uDestRegIdx + 1;
		#endif /* defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554) */
	}

	/*
		Finally record the overall number of registers used	
	*/
	psTexChanInfo->uRegCount = uRegCount;

	if	(!psSample->bNonDependent)
	{
		psSample->sTexChanInfo.uTexNonDepChunkMask = 0;
	}
	return IMG_TRUE;
}

static IMG_BOOL SaveInternalRegs(PUSP_SAMPLE	psSample, 
								 PUSP_CONTEXT	psContext, 
								 IMG_UINT32*	puHWInstCount, 
								 IMG_BOOL		bSkipInv, 
								 IMG_UINT32		uInstDescFlags, 
								 IMG_UINT32*	puSampleTempsUsed, 
								 IMG_UINT32*	puSampleTempRegToStoreIRegs)
{
	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/
	if(
		psSample->bNonDependent && 
		psSample->sTexChanInfo.uTexChunkMask && 
		(psSample->sTexChanInfo.uTexChunkMask != psSample->sTexChanInfo.uTexNonDepChunkMask) && 
		(psSample->uIRegsLiveBefore)
	)
	{			
		PHW_INST psHWInst;
		IMG_UINT32 uHWInstCount = *puHWInstCount;
		IMG_UINT32 uI=0;
		IMG_UINT32 uSampleTempReg = psSample->uSampleTempRegNum + (*puSampleTempsUsed);
		PUSP_MOESTATE psMOEState = &psSample->psInstBlock->sFinalMOEState;
		*puSampleTempRegToStoreIRegs = uSampleTempReg;
		
		if(psSample->uC10IRegsLiveBefore)
		{
			if(!psMOEState->bColFmtCtl)
			{
				psHWInst			= &psSample->asSampleInsts_[uHWInstCount];
				if	(!HWInstEncodeSETFCInst(psHWInst,
											IMG_TRUE, 
											psMOEState->bEFOFmtCtl))
				{
					USP_DBGPRINT(("SaveInternalRegs: Error encoding SETFC inst\n"));
					return IMG_FALSE;
				}
				psMOEState->bColFmtCtl = IMG_TRUE;
				uHWInstCount++;
			}
		}

		#if defined(SGX_FEATURE_USE_VEC34)
		while(uI<NUM_INTERNAL_REGISTERS)
		#else
		while(uI<NUM_INTERNAL_REGISTERS_NON_VEC)
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
		{
			if(psSample->uIRegsLiveBefore & (1 << uI))
			{				
				if(psSample->uC10IRegsLiveBefore & (1 << uI))
				{
					USP_REG sDestReg;
					USP_REG sSrc1Reg;
					USP_REG sSrc2Reg;

					{
						psHWInst			= &psSample->asSampleInsts_[uHWInstCount];

						sDestReg.eType		= psSample->eSampleTempRegType;
						sDestReg.uNum		= uSampleTempReg;
						sDestReg.eFmt		= USP_REGFMT_C10;
						sDestReg.uComp		= 0;
						sDestReg.eDynIdx	= USP_DYNIDX_NONE;

						sSrc1Reg.eType		= USP_REGTYPE_INTERNAL;
						sSrc1Reg.uNum		= uI;
						sSrc1Reg.eFmt		= USP_REGFMT_C10;
						sSrc1Reg.uComp		= 0;
						sSrc1Reg.eDynIdx	= USP_DYNIDX_NONE;

						sSrc2Reg.eType		= USP_REGTYPE_IMM;
						sSrc2Reg.uNum		= 0;
						sSrc2Reg.eFmt		= USP_REGFMT_C10;
						sSrc2Reg.uComp		= 0;
						sSrc2Reg.eDynIdx	= USP_DYNIDX_NONE;
						
						if(!
							HWInstEncodeSOPWMInst(
								psMOEState, 
								psHWInst, 
								bSkipInv, 
								EURASIA_USE1_SOP2WM_COP_ADD, 
								EURASIA_USE1_SOP2WM_AOP_ADD, 
								EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT, 
								EURASIA_USE1_SOP2WM_SEL1_ZERO, 
								EURASIA_USE1_SOP2WM_MOD2_NONE, 
								EURASIA_USE1_SOP2WM_SEL2_ZERO, 
								USP_RGB_CHAN_MASK, 
								&sDestReg, 
								&sSrc1Reg, 
								&sSrc2Reg
							)
						)
						{
							USP_DBGPRINT(("SampleTextureData: Error encoding SOPWM inst\n"));
							return IMG_FALSE;
						}
						psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_SOPWM;
						psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;
						uHWInstCount++;
						uSampleTempReg++;
					}
					{
						psHWInst	= &psSample->asSampleInsts_[uHWInstCount];

						sDestReg.eType		= psSample->eSampleTempRegType;
						sDestReg.uNum		= uSampleTempReg;
						sDestReg.eFmt		= USP_REGFMT_C10;
						sDestReg.uComp		= 0;
						sDestReg.eDynIdx	= USP_DYNIDX_NONE;

						sSrc1Reg.eType		= USP_REGTYPE_INTERNAL;
						sSrc1Reg.uNum		= uI;
						sSrc1Reg.eFmt		= USP_REGFMT_C10;
						sSrc1Reg.uComp		= 3;
						sSrc1Reg.eDynIdx	= USP_DYNIDX_NONE;

						sSrc2Reg.eType		= USP_REGTYPE_IMM;
						sSrc2Reg.uNum		= 0;
						sSrc2Reg.eFmt		= USP_REGFMT_C10;
						sSrc2Reg.uComp		= 0;
						sSrc2Reg.eDynIdx	= USP_DYNIDX_NONE;
						
						if	(!HWInstEncodePCKUNPCKInst(psHWInst,
													   bSkipInv,
													   USP_PCKUNPCK_FMT_C10,
													   USP_PCKUNPCK_FMT_C10,
													   IMG_FALSE,
													   1,
													   &sDestReg,
													   0,
													   1,
													   &sSrc1Reg,
													   &sSrc2Reg))
						{
							USP_DBGPRINT(("SampleTextureData: Error encoding PCKUNPCK inst\n"));
							return IMG_FALSE;
						}
						psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_PCKUNPCK;
						psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;
						uHWInstCount++;
						uSampleTempReg++;
					}
				}
				else
				{
					USP_REG sDestReg;
					USP_REG sSrc1Reg;
					{
						psHWInst			= &psSample->asSampleInsts_[uHWInstCount];

						sDestReg.eType		= psSample->eSampleTempRegType;
						sDestReg.uNum		= uSampleTempReg;
						sDestReg.eFmt		= USP_REGFMT_F32;
						sDestReg.uComp		= 0;
						sDestReg.eDynIdx	= USP_DYNIDX_NONE;

						sSrc1Reg.eType		= USP_REGTYPE_INTERNAL;
						sSrc1Reg.uNum		= uI;
						sSrc1Reg.eFmt		= USP_REGFMT_F32;
						sSrc1Reg.uComp		= 0;
						sSrc1Reg.eDynIdx	= USP_DYNIDX_NONE;

						#if defined(SGX_FEATURE_USE_VEC34)
						{							
							IMG_UINT32 uDestAlignOffset = uSampleTempReg % 2;							
							IMG_UINT32 auSrcSelect[4];
							IMG_UINT32 uChanIdx;
							sDestReg.uNum	-= uDestAlignOffset;
							sSrc1Reg.uNum	= uI/4;
							for(uChanIdx = 0; uChanIdx < 4; uChanIdx++)
							{
								auSrcSelect[uChanIdx] = uI % 4;
							}								
							HWInstEncodeVectorPCKUNPCKInst(psHWInst, bSkipInv, USP_PCKUNPCK_FMT_F32, USP_PCKUNPCK_FMT_F32, IMG_FALSE, 1<<uDestAlignOffset, &sDestReg, IMG_TRUE, auSrcSelect, 0, 0, &sSrc1Reg, &sSrc1Reg);
							psSample->aeOpcodeForSampleInsts_[uHWInstCount] = USP_OPCODE_PCKUNPCK;
						}
						#else
						if	(!HWInstEncodeMOVInst(psHWInst,
												  1,
												  bSkipInv,
												  &sDestReg,
												  &sSrc1Reg))
						{
							USP_DBGPRINT(("SampleTextureData: Error encoding MOV HW-inst\n"));
							return IMG_FALSE;
						}
						psSample->aeOpcodeForSampleInsts_[uHWInstCount] = USP_OPCODE_MOVC;
						#endif /* defined(SGX_FEATURE_USE_VEC34) */
					}

					psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;
					uHWInstCount++;
					uSampleTempReg++;
				}
			}		
			uI++;
		}
		*puSampleTempsUsed = uSampleTempReg - psSample->uSampleTempRegNum;
		*puHWInstCount = uHWInstCount;
	}
	return IMG_TRUE;
}

static IMG_BOOL ReStoreInternalRegs(PUSP_SAMPLE		psSample, 
									PUSP_CONTEXT	psContext, 
									IMG_UINT32*		puHWInstCount, 
									IMG_BOOL		bSkipInv, 
									IMG_UINT32		uInstDescFlags, 
									IMG_UINT32		uSMPInstCount, 
									IMG_UINT32		uSampleTempRegToStoreIRegs)
/*****************************************************************************
 Name:		ReStoreInternalRegs

 Purpose:	Restore Internal registers saved before wdf instruction.

 Inputs:	psSample			- The USP sample to restore internal 
								  registers for.
		    puHWInstCount		- Sample's hardware instruction count input 
								  and output.
		    bSkipInv			- Either to skip environment or not.
			uSMPInstCount		- Number of dependent chunks samples for the 
								  sample.
			uSampleTempRegToStoreIRegs
								- Sample temporary register used while storing 
								  internal registers.

 Outputs:	puHWInstCount		- Hardware instruction count incremented to 
								  include restoring instrucions.

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
{		
	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/
	if(
		uSMPInstCount > 0 && 
		psSample->bNonDependent && 
		psSample->sTexChanInfo.uTexChunkMask && 
		(psSample->sTexChanInfo.uTexChunkMask != psSample->sTexChanInfo.uTexNonDepChunkMask) && 
		(psSample->uIRegsLiveBefore)
	)
	{
		PHW_INST psHWInst;
		IMG_UINT32 uHWInstCount = *puHWInstCount;
		IMG_UINT32 uI=0;
		IMG_UINT32 uSampleTempReg = uSampleTempRegToStoreIRegs;
		IMG_BOOL bFirstInst = IMG_TRUE;
		
		#if defined(SGX_FEATURE_USE_VEC34)
		while(uI<NUM_INTERNAL_REGISTERS)
		#else
		while(uI<NUM_INTERNAL_REGISTERS_NON_VEC)
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
		{
			if(psSample->uIRegsLiveBefore & (1 << uI))
			{				
				if(psSample->uC10IRegsLiveBefore & (1 << uI))
				{
					USP_REG sDestReg;
					USP_REG sSrc1Reg;
					USP_REG sSrc2Reg;

					{
						psHWInst			= &psSample->asSampleInsts_[uHWInstCount];

						sDestReg.eType		= USP_REGTYPE_INTERNAL;
						sDestReg.uNum		= uI;
						sDestReg.eFmt		= USP_REGFMT_C10;
						sDestReg.uComp		= 0;
						sDestReg.eDynIdx	= USP_DYNIDX_NONE;

						sSrc1Reg.eType		= psSample->eSampleTempRegType;
						sSrc1Reg.uNum		= uSampleTempReg;
						sSrc1Reg.eFmt		= USP_REGFMT_C10;
						sSrc1Reg.uComp		= 0;
						sSrc1Reg.eDynIdx	= USP_DYNIDX_NONE;

						sSrc2Reg.eType		= psSample->eSampleTempRegType;
						sSrc2Reg.uNum		= uSampleTempReg + 1;
						sSrc2Reg.eFmt		= USP_REGFMT_C10;
						sSrc2Reg.uComp		= 0;
						sSrc2Reg.eDynIdx	= USP_DYNIDX_NONE;
						
						if	(!HWInstEncodePCKUNPCKInst(psHWInst,
													   bSkipInv,
													   USP_PCKUNPCK_FMT_C10,
													   USP_PCKUNPCK_FMT_C10,
													   IMG_TRUE,
													   0xF,
													   &sDestReg,
													   0,
													   1,
													   &sSrc1Reg,
													   &sSrc2Reg))
						{
							USP_DBGPRINT(("SampleTextureData: Error encoding PCKUNPCK inst\n"));
							return IMG_FALSE;
						}
						psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_PCKUNPCK;
						if(bFirstInst == IMG_FALSE)
						{
							psSample->auInstDescFlagsForSampleInsts_[uHWInstCount] = uInstDescFlags;
						}
						else
						{
							psSample->auInstDescFlagsForSampleInsts_[uHWInstCount] = 
								(uInstDescFlags & (~USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE));
						}
						uHWInstCount++;
						uSampleTempReg+=2;
						bFirstInst = IMG_FALSE;
					}				
				}
				else
				{
					USP_REG sDestReg;
					USP_REG sSrc1Reg;
					{
						psHWInst			= &psSample->asSampleInsts_[uHWInstCount];

						sDestReg.eType		= USP_REGTYPE_INTERNAL;
						sDestReg.uNum		= uI;
						sDestReg.eFmt		= USP_REGFMT_F32;
						sDestReg.uComp		= 0;
						sDestReg.eDynIdx	= USP_DYNIDX_NONE;					

						sSrc1Reg.eType		= psSample->eSampleTempRegType;
						sSrc1Reg.uNum		= uSampleTempReg;
						sSrc1Reg.eFmt		= USP_REGFMT_F32;
						sSrc1Reg.uComp		= 0;
						sSrc1Reg.eDynIdx	= USP_DYNIDX_NONE;

						#if defined(SGX_FEATURE_USE_VEC34)
						{
							IMG_UINT32 auSrcSelect[4];
							IMG_UINT32 uSrcAlignOffset = uSampleTempReg % 2;
							IMG_UINT32 uChanIdx;
							sDestReg.uNum	= uI/4;
							sSrc1Reg.uNum	-= uSrcAlignOffset;
							for(uChanIdx = 0; uChanIdx < 4; uChanIdx++)
							{
								auSrcSelect[uChanIdx] = uSrcAlignOffset;
							}								
							HWInstEncodeVectorPCKUNPCKInst(psHWInst, bSkipInv, USP_PCKUNPCK_FMT_F32, USP_PCKUNPCK_FMT_F32, IMG_FALSE, 1<<(uI%4), &sDestReg, IMG_TRUE, auSrcSelect, 0, 0, &sSrc1Reg, &sSrc1Reg);
							psSample->aeOpcodeForSampleInsts_[uHWInstCount] = USP_OPCODE_PCKUNPCK;
						}
						#else
						if	(!HWInstEncodeMOVInst(psHWInst,
												  1,
												  bSkipInv,
												  &sDestReg,
												  &sSrc1Reg))
						{
							USP_DBGPRINT(("SampleTextureData: Error encoding MOV HW-inst\n"));
							return IMG_FALSE;
						}
						psSample->aeOpcodeForSampleInsts_[uHWInstCount] = USP_OPCODE_MOVC;
						#endif /* defined(SGX_FEATURE_USE_VEC34) */
					}

					if(bFirstInst == IMG_FALSE)
					{
						psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;
					}
					else
					{
						psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= 
							(uInstDescFlags & (~USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE));
					}
					uHWInstCount++;
					uSampleTempReg+=1;
				}			
			}		
			uI++;
		}
		*puHWInstCount = uHWInstCount;
	}
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		UpdateSampleDestCahnnels

 Purpose:	Records the destination channels that will use this chunk of 
			sampled data

 Inputs:	psSample		- The sample to record dest channels for
			psTexChanInfo	- The associated text channel info
			psHWInst		- The sample instruction
			uChunkIdx		- The chunk to update info for

 Outputs:	none

 Returns:	IMG_TRUE
*****************************************************************************/
static IMG_BOOL UpdateSampleDestCahnnels(PUSP_SAMPLE		psSample, 
										 PUSP_TEX_CHAN_INFO	psTexChanInfo,
										 PHW_INST			psHWInst,
										 IMG_UINT32			uChunkIdx)
{
			
	IMG_UINT32 uCompIdx;

	for	(uCompIdx = 0; uCompIdx < USP_MAX_SAMPLE_CHANS; uCompIdx++)
	{
		if	(psSample->psSampleUnpack->uMask & (1 << uCompIdx))
		{
			IMG_UINT32	uTexChanIdx;	

			uTexChanIdx	= psTexChanInfo->aeTexChanForComp[uCompIdx];
			if	(uTexChanIdx <= USP_MAX_SAMPLE_CHANS)
			{
				IMG_UINT32	uTexChunkIdx;

				uTexChunkIdx = psTexChanInfo->auChunkIdxForChan[uTexChanIdx];
				if	(uTexChunkIdx == uChunkIdx)
				{
					psSample->apsSMPInsts[uCompIdx] = psHWInst;
				}
			}
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		SampleNormaliseCoordinates

 Purpose:	Add instructions to normalise and center the coordinates on
            unnormalised coordinates within a sample

 Inputs:	psSample             - The dependent sample to generate code for
			psInputData          - The input-data available to the shader
			psContext            - The current USP execution context
			puHWInstCount        - The current number of hardware instructions generated
			puSampleTempRegCount - The current number of temps used

 Outputs:	none

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
static IMG_BOOL SampleNormaliseCoordinates(PUSP_SAMPLE     psSample,
                                           PUSP_CONTEXT    psContext,
                                           IMG_UINT32      uInstDescFlags,
                                           IMG_PUINT32     puHWInstCount,
                                           IMG_PUINT32     puSampleTempRegCount)
{
	IMG_UINT32 uHWInstCount        = 0;
	IMG_UINT32 uSampleTempRegCount = 0;
	IMG_UINT32 uTextureWidth       = 0;
	IMG_UINT32 uTextureHeight      = 0;
	IMG_BOOL   bFasterPath         = IMG_FALSE;
	IMG_UINT32 i                   = 0;
	USP_REG    sImmRcp             = {0};
	USP_REG    sImmOff             = {0};
	USP_REG    sCoord              = {0};
	USP_REG    sCoordY             = {0};
	USP_REG    sTmpX               = {0};
	USP_REG    sTmpY               = {0};
	PHW_INST   psHWInst            = IMG_NULL;
	PUSP_REG   psOrigX             = IMG_NULL;
	PUSP_REG   psOrigY             = IMG_NULL;

	/* 0.0f */
	static const USP_REG sHWConstZero = 
	{
		USP_REGTYPE_SPECIAL,
#if defined(SGX_FEATURE_USE_VEC34)
		SGXVEC_USE_SPECIAL_CONSTANT_ZERO_ZERO,
#else
		EURASIA_USE_SPECIAL_CONSTANT_ZERO,
#endif /* defined(SGX_FEATURE_USE_VEC34) */
		USP_REGFMT_F32,
		0,
		USP_DYNIDX_NONE
	};

	/* 1.0f */
	static const USP_REG sHWConstOne = 
	{
		USP_REGTYPE_SPECIAL,
#if defined(SGX_FEATURE_USE_VEC34)
		SGXVEC_USE_SPECIAL_CONSTANT_ONE_ONE,
#else
		EURASIA_USE_SPECIAL_CONSTANT_FLOAT1,
#endif /* defined(SGX_FEATURE_USE_VEC34) */
		USP_REGFMT_F16,
		0,
		USP_DYNIDX_NONE
	};

	/* Get current number of hardware instructions and temps */
	uHWInstCount        = *puHWInstCount;
	uSampleTempRegCount = *puSampleTempRegCount;

	/*
	  Add instructions to normalize the co-ordinates of the texture
	  sample using the information given through the driver.

	  Only performed if PVRUniPatchSetNormalizedCoords is called
	  and set to IMG_FALSE.

	  Also performs centering on the coordinates to ensure they
	  sample at the correct position.
	*/
	if(psContext->asSamplerDesc[psSample->uTextureIdx].bNormalizeCoords)
	{
		/* Setup up temporaries */
		sImmRcp.eType	= psSample->eSampleTempRegType;
		sImmRcp.uNum	= psSample->uSampleTempRegNum + uSampleTempRegCount++;

		sTmpX.eType	    = psSample->eSampleTempRegType;
		sTmpX.uNum	    = psSample->uSampleTempRegNum + uSampleTempRegCount++;

		sTmpY.eType	    = psSample->eSampleTempRegType;
		sTmpY.uNum	    = psSample->uSampleTempRegNum + uSampleTempRegCount++;

#if defined(SGX_FEATURE_USE_VEC34)
		/*
		   For cores with a vector instruction set, make sure the 
		   X co-ordinate is in an even numbered register
		*/
		if(sTmpX.uNum % 2 != 0)
		{
			IMG_UINT32 ui32Temp = sTmpY.uNum;

			sTmpX.uNum--;
			sTmpY.uNum--;
			sImmRcp.uNum = ui32Temp;
		}
#endif /* defined(SGX_FEATURE_USE_VEC34) */

		/* Retrieve texture information */
		uTextureWidth  = psContext->asSamplerDesc[psSample->uTextureIdx].uTextureWidth;
		uTextureHeight = psContext->asSamplerDesc[psSample->uTextureIdx].uTextureHeight;

		/*
		  For a linear filter use the hardware constant zero so we perform
		  no offsetting, otherwise we load in the value with a limm.
		 */
		if(psContext->asSamplerDesc[psSample->uTextureIdx].bLinearFilter)
		{
			sImmOff.eType	= USP_REGTYPE_SPECIAL;
			sImmOff.uNum	= EURASIA_USE_SPECIAL_CONSTANT_ZERO;
			sImmOff.eFmt    = USP_REGFMT_F32;
		}
		else
		{
			sImmOff.eType	= psSample->eSampleTempRegType;
			sImmOff.uNum	= psSample->uSampleTempRegNum + uSampleTempRegCount++;
		}

		USP_DBGPRINT((" CL: Normalising sample %u with texture dimensions (%u,%u)\n",psSample->uTextureIdx,uTextureWidth,uTextureHeight));
		USP_DBGPRINT((" CL: Sample is using a %s filter\n",psContext->asSamplerDesc[psSample->uTextureIdx].bLinearFilter ? "linear" : "nonlinear"));
		USP_DBGPRINT((" CL: tempRcp %u, tempOff %u\n",sImmRcp.uNum,sImmOff.uNum));

		/* Decode sample instruction to determine coordinate registers */
		if(!HWInstDecodeSrc0BankAndNum(USP_FMTCTL_NONE,
		                               IMG_FALSE,
		                               &psSample->sBaseInst,
		                               &sCoord))
		{
			USP_DBGPRINT(("SampleNormaliseCoordinates: Error decoding src0 of sample instruction \n"));
			return IMG_FALSE;
		}

		/* Store Y coordinate location for encoding directly */
		sCoordY.eType = sCoord.eType;
		sCoordY.uNum  = sCoord.uNum + 1;
		sCoordY.eFmt  = sCoord.eFmt;

		/* Setup pointers */
		psOrigX = &sCoord;
		psOrigY = &sCoordY;
		
		/* If width and height are the same we can reduce number of limm's */
		if( uTextureWidth == uTextureHeight )
		{
			bFasterPath = IMG_TRUE;
		}

		/*
		  If we are performing a linear filtered sample do not center the
		  coordinates as we alter the results, therefore don't floor them.
		 */
		if( !psContext->asSamplerDesc[psSample->uTextureIdx].bLinearFilter )
		{
			psHWInst = &psSample->asSampleInsts_[uHWInstCount];

#if !defined(SGX_FEATURE_USE_VEC34)
			/* fsubfloor X, 0, X */
			if(!HWInstEncodeFRCInst(psHWInst,
			                        IMG_TRUE,
			                        1,
			                        &sTmpX,
			                        (PUSP_REG)&sHWConstZero,
			                        psOrigX))
			{
				USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding FRC instruction\n"));
				return IMG_FALSE;
			}

			psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_FRC;
			psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

			uHWInstCount++;

			/* fsubfloor Y, 0, Y */
			psHWInst = &psSample->asSampleInsts_[uHWInstCount];

			if(!HWInstEncodeFRCInst(psHWInst,
			                        IMG_TRUE,
			                        1,
			                        &sTmpY,
			                        (PUSP_REG)&sHWConstZero,
			                        psOrigY))
			{
				USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding FRC instruction\n"));
				return IMG_FALSE;
			}

			psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_FRC;
			psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

			uHWInstCount++;
#else
			/* fsubfloor X, 0, X */
			if(!HWInstEncodeVFRCInst(psHWInst,
			                       	 IMG_TRUE,
			                         1,
			                         &sTmpX,
			                         (PUSP_REG)&sHWConstZero,
									 SGXVEC_USE_VECMAD_SRC1SWIZZLE_XXXX,
			                         psOrigX,
									 SGXVEC_USE_VECMAD_SRC1SWIZZLE_XYZW))
			{
				USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding VFRC instruction\n"));
				return IMG_FALSE;
			}

			psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_VFRC;
			psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

			uHWInstCount++;
#endif

			/* fmov X, X.neg */
			psHWInst = &psSample->asSampleInsts_[uHWInstCount];

#if !defined(SGX_FEATURE_USE_VEC34)
			if(!HWInstEncodeFMADInst(psHWInst,
			                         IMG_TRUE,
			                         1,
			                         &sTmpX,
			                         &sTmpX,
			                         (PUSP_REG)&sHWConstOne,
			                         (PUSP_REG)&sHWConstZero))
			{
				USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding FMAD instruction\n"));
				return IMG_FALSE;
			}

			/* Negate src0 */
			psHWInst->uWord1 |= (EURASIA_USE_SRCMOD_NEGATE << EURASIA_USE1_SRC0MOD_SHIFT);

			psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_FMAD16;
			psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

			uHWInstCount++;
			
			/* fmov Y, Y.neg */
			psHWInst = &psSample->asSampleInsts_[uHWInstCount];

			if(!HWInstEncodeFMADInst(psHWInst,
			                         IMG_TRUE,
			                         1,
			                         &sTmpY,
			                         &sTmpY,
			                         (PUSP_REG)&sHWConstOne,
			                         (PUSP_REG)&sHWConstZero))
			{
				USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding FMAD instruction\n"));
				return IMG_FALSE;
			}

			/* Negate src0 */
			psHWInst->uWord1 |= (EURASIA_USE_SRCMOD_NEGATE << EURASIA_USE1_SRC0MOD_SHIFT);

			psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_FMAD16;
			psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

			uHWInstCount++;
#else
			if(!HWInstEncodeVMADInst(psHWInst,
			                         IMG_TRUE,
			                         1,
			                         &sTmpX,
									 0xF,
			                         &sTmpX,
									 SGXVEC_USE_VECMAD_SRC0SWIZZLE_XYZW,
			                         (PUSP_REG)&sHWConstOne,
									 SGXVEC_USE_VECMAD_SRC1SWIZZLE_XXXX,
			                         (PUSP_REG)&sHWConstZero,
									 SGXVEC_USE_VECMAD_SRC2SWIZZLE_XXXX))
			{
				USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding VMAD instruction\n"));
				return IMG_FALSE;
			}

			psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_VMAD;
			psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

			/* Negate src1 */
			psHWInst->uWord1 |= (0x01 << SGXVEC_USE1_VECNONMAD_SRC1MOD_SHIFT);

			uHWInstCount++;
#endif

			/*
			  The coordinates have been modified so point original at the newly
			  modified ones.
			*/
			psOrigX = &sTmpX;
			psOrigY = &sTmpY;
		}

		if( bFasterPath )
		{
			IMG_FLOAT 	fRcp 		= 1.0f / (IMG_FLOAT)uTextureWidth;
			IMG_FLOAT 	fOff 		= 0.5f / (IMG_FLOAT)uTextureWidth;

			psHWInst = &psSample->asSampleInsts_[uHWInstCount];
			
			/* LIMM fRcp */
			if(!HWInstEncodeLIMMInst(psHWInst,
			                         1,
			                         &sImmRcp,
			                         *(IMG_PUINT32)((IMG_PFLOAT)&fRcp)))
			{
				USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding LIMM instruction\n"));
				return IMG_FALSE;
			}

			psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_LIMM;
			psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;
			
			uHWInstCount++;

			/* Don't load offset with linear filter */
			if(!psContext->asSamplerDesc[psSample->uTextureIdx].bLinearFilter)
			{
				psHWInst = &psSample->asSampleInsts_[uHWInstCount];

				/* LIMM fOff */
				if(!HWInstEncodeLIMMInst(psHWInst,
				                         1,
				                         &sImmOff,
				                         *(IMG_PUINT32)((IMG_PFLOAT)&fOff)))
				{
					USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding LIMM instruction\n"));
					return IMG_FALSE;
				}

				psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_LIMM;
				psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;
			
				uHWInstCount++;
			}
			
			psHWInst = &psSample->asSampleInsts_[uHWInstCount];

#if !defined(SGX_FEATURE_USE_VEC34)
			/* FMAD X, X, RCP, OFF */
			if(!HWInstEncodeFMADInst(psHWInst,
			                         IMG_TRUE,
			                         1,
			                         &sTmpX,
			                         psOrigX,
			                         &sImmRcp,
			                         &sImmOff))
			{
				USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding FMAD instruction\n"));
				return IMG_FALSE;
			}

			psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_FMAD16;
			psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

			uHWInstCount++;
			psHWInst = &psSample->asSampleInsts_[uHWInstCount];

			/* FMAD Y, Y, RCP, OFF */
			if(!HWInstEncodeFMADInst(psHWInst,
			                         IMG_TRUE,
			                         1,
			                         &sTmpY,
			                         psOrigY,
			                         &sImmRcp,
			                         &sImmOff))
			{
				USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding FMAD instruction\n"));
				return IMG_FALSE;
			}

			psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_FMAD16;
			psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

			uHWInstCount++;
#else
			{
				IMG_UINT32 	ui32RcpSwiz = 0;
				IMG_UINT32 	ui32OffSwiz = 0;

				/* If the register is even use an X swizzle, otherwise use a Y swizzle */
				if((sImmRcp.uNum % 2) == 0)
				{
					ui32RcpSwiz = SGXVEC_USE_VECMAD_SRC1SWIZZLE_XXXX;
				}
				else
				{
					ui32RcpSwiz = SGXVEC_USE_VECMAD_SRC1SWIZZLE_YYYY;
				}

				if((sImmOff.uNum % 2) == 0)
				{
					ui32OffSwiz = SGXVEC_USE_VECMAD_SRC1SWIZZLE_XXXX;
				}
				else
				{
					ui32OffSwiz = SGXVEC_USE_VECMAD_SRC1SWIZZLE_YYYY;
				}

				if(!HWInstEncodeVMADInst(psHWInst,
										 IMG_TRUE,
										 1,
										 &sTmpX,
										 0xF,
										 psOrigX,
										 SGXVEC_USE_VECMAD_SRC0SWIZZLE_XYZW,
										 &sImmRcp,
										 ui32RcpSwiz,
										 &sImmOff,
										 ui32OffSwiz))
				{
					USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding VMAD instruction\n"));
					return IMG_FALSE;
				}

				psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_VMAD;
				psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

				uHWInstCount++;
			}
#endif
		}
		else
		{
			IMG_FLOAT  fRcp[2];
			IMG_FLOAT  fOff[2];

			fRcp[0] = 1.0f / (IMG_FLOAT)uTextureWidth;
			fRcp[1] = 1.0f / (IMG_FLOAT)uTextureHeight;

			fOff[0] = 0.5f / (IMG_FLOAT)uTextureWidth;
			fOff[1] = 0.5f / (IMG_FLOAT)uTextureHeight;

			for( i=0; i < 2; ++i )
			{
				psHWInst = &psSample->asSampleInsts_[uHWInstCount];
				
				/* LIMM RCP */
				if(!HWInstEncodeLIMMInst(psHWInst,
				                         1,
				                         &sImmRcp,
				                         *(IMG_PUINT32)(&fRcp[i])))
				{
					USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding LIMM instruction\n"));
					return IMG_FALSE;
				}

				psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_LIMM;
				psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;
			
				uHWInstCount++;
				
				if( !psContext->asSamplerDesc[psSample->uTextureIdx].bLinearFilter )
				{
					psHWInst = &psSample->asSampleInsts_[uHWInstCount];
					
					/* LIMM OFF */
					if(!HWInstEncodeLIMMInst(psHWInst,
					                         1,
					                         &sImmOff,
					                         *(IMG_PUINT32)(&fOff[i])))
					{
						USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding LIMM instruction\n"));
						return IMG_FALSE;
					}

					psHWInst = &psSample->asSampleInsts_[uHWInstCount];
					psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_LIMM;
					psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;
			
					uHWInstCount++;
				}
				
				psHWInst = &psSample->asSampleInsts_[uHWInstCount];

#if !defined(SGX_FEATURE_USE_VEC34)
				/* FMAD C, C, RCP, OFF */
				if(!HWInstEncodeFMADInst(psHWInst,
				                         IMG_TRUE,
				                         1,
				                         i == 0 ? &sTmpX : &sTmpY,
				                         i == 0 ? psOrigX : psOrigY,
				                         &sImmRcp,
				                         &sImmOff))
				{
					USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding FMAD instruction\n"));
					return IMG_FALSE;
				}

				psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_FMAD16;
				psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;
#else
				{
					IMG_UINT32 	ui32RcpSwiz = 0;
					IMG_UINT32 	ui32OffSwiz = 0;

					/* If the register is even use an X swizzle, otherwise use a Y swizzle */
					if((sImmRcp.uNum % 2) == 0)
					{
						ui32RcpSwiz = SGXVEC_USE_VECMAD_SRC1SWIZZLE_XXXX;
					}
					else
					{
						ui32RcpSwiz = SGXVEC_USE_VECMAD_SRC1SWIZZLE_YYYY;
					}

					if((sImmOff.uNum % 2) == 0)
					{
						ui32OffSwiz = SGXVEC_USE_VECMAD_SRC2SWIZZLE_XXXX;
					}
					else
					{
						ui32OffSwiz = SGXVEC_USE_VECMAD_SRC2SWIZZLE_YYYY;
					}

					/* FMAD C, C, RCP, OFF */
					if(!HWInstEncodeVMADInst(psHWInst,
											 IMG_TRUE,
											 1,
											 &sTmpX,
											 1 << i,
											 psOrigX,
											 i == 0 ? SGXVEC_USE_VECMAD_SRC0SWIZZLE_XXXX : SGXVEC_USE_VECMAD_SRC0SWIZZLE_YYYY,
											 &sImmRcp,
											 ui32RcpSwiz,
											 &sImmRcp,
											 ui32OffSwiz))
					{
						USP_DBGPRINT(("SampleNormaliseCoordinates: Error encoding VMAD instruction\n"));
						return IMG_FALSE;
					}
				}

				psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_VMAD;
				psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;
#endif
				uHWInstCount++;
			}
		}

		/* Update current number of hardware instructions and temps */
		*puHWInstCount        = uHWInstCount;
		*puSampleTempRegCount = uSampleTempRegCount;

		/*
		  Encode coordinate registers as they have been replaced
		  by normalised (and possibly centered) temporaries
		*/
		if(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE,
		                               psSample->eBaseInstOpcode,
		                               IMG_FALSE,
		                               &psSample->sBaseInst,
		                               &sTmpX))
		{
			USP_DBGPRINT(("SampleNormaliseCoordinates: Error decoding src0 of sample instruction \n"));
			return IMG_FALSE;
		}

	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		SampleTextureData

 Purpose:	Add instructions to get the texture data for a dependent sample

 Inputs:	psSample	- The dependent sample to generate code for
			psInputData	- The input-data available to the shader
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
static IMG_BOOL SampleTextureData(PUSP_SAMPLE		psSample,
								  PUSP_INPUT_DATA	psInputData,
								  PUSP_CONTEXT		psContext)
{
	PUSP_SHADER			psShader;
	PUSP_PROGDESC		psProgDesc;
	PUSP_TEX_CHAN_INFO	psTexChanInfo;
	IMG_UINT32			uTextureIdx;
	IMG_UINT32			uChunkIdx;
	USP_OPCODE			eBaseInstOpcode;
	PHW_INST			psBaseInst;
	IMG_UINT32			uInstDescFlags;
	PHW_INST			psHWInst;
	IMG_UINT32			uHWInstCount;
	IMG_UINT32			uSMPInstCount;
	IMG_UINT			uLDInstCount;
	USP_REG				asTexStateReg[USP_MAX_TEXTURE_CHUNKS];
	IMG_UINT32			uSampleTempRegCount;
	IMG_UINT32			uBaseSampleDestRegCount;	
	IMG_BOOL			bSkipInv;
	PUSP_TEXSTATE_DATA	psPrevMemTexStateData;
	PHW_INST			psPrevLoadHWInst;
	IMG_UINT32			uPrevLoadFetchCount;
	IMG_UINT32			uSampleTempRegToStoreIRegs = 0;
	PUSP_REG			psANDSrcReg = IMG_NULL;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	psShader		= psSample->psShader;
	psProgDesc		= psShader->psProgDesc;
	psTexChanInfo	= &psSample->sTexChanInfo;
	uTextureIdx		= psSample->uTextureIdx;
	uInstDescFlags	= psSample->uInstDescFlags & ~(USP_INSTDESC_FLAG_DEST_USES_RESULT);

	bSkipInv		= (IMG_BOOL)(!(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV));

	/*
		Setup source locations for all the texture-state needed

		NB: Doing this before generating the sample code allows texture-state to
			be fetched from memory more efficiently.
	*/
	psPrevMemTexStateData	= IMG_NULL;
	psPrevLoadHWInst		= IMG_NULL;
	uPrevLoadFetchCount		= 0;
	uHWInstCount			= psSample->uSampleInstCount_;
	uSampleTempRegCount		= psSample->uSampleTempRegCount;
	uBaseSampleDestRegCount	= psSample->uBaseSampleDestRegCount_;
	uLDInstCount			= 0;

	if(!SaveInternalRegs(psSample, psContext, &uHWInstCount, bSkipInv, uInstDescFlags, &uSampleTempRegCount, &uSampleTempRegToStoreIRegs))
	{
		USP_DBGPRINT(("SampleTextureData: Failed to save Internal registers"));
		return IMG_FALSE;
	}

	for	(uChunkIdx = 0; uChunkIdx < USP_MAX_TEXTURE_CHUNKS; uChunkIdx++)
	{
		PUSP_TEXSTATE_DATA	psTexStateData;
		PUSP_REG			psTexStateReg;

		/*
			Ignore texture chunks that we don't need to sampled
		*/
		if	(!(psTexChanInfo->uTexChunkMask & (1 << uChunkIdx)) || (psTexChanInfo->uTexNonDepChunkMask  & (1 << uChunkIdx)))
		{
			continue;
		}

		/*
			Fetch details of the texture-state needed for this chunk
		*/
		psTexStateData = USPInputDataGetTexStateData(psInputData,
													 uTextureIdx,
													 uChunkIdx,
													 psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTexWrdIdx[uChunkIdx]);

		if	(!psTexStateData)
		{
			USP_DBGPRINT(("SampleTextureData: Texture-state data for texture %d/chunk %d not found\n", uTextureIdx, uChunkIdx));
			return IMG_FALSE;
		}

		/*
			Setup a register location for the texture-state for this chunk, loading it from
			memory if required
		*/
		psTexStateReg = &asTexStateReg[uChunkIdx];

		if	(psTexStateData->bInMemory)
		{
			USP_REG		sBaseAddrReg;
			USP_REG		sAddrOffReg;
			IMG_BOOL	bMergeLoad;

			/*
				Check whether the texture-state for this chunk is contiguous with
				the last state loaded.
			*/
			bMergeLoad = IMG_FALSE;

			if	(psPrevMemTexStateData)
			{
				IMG_UINT32	uPrevTexStateEnd;

				uPrevTexStateEnd = psPrevMemTexStateData->uRegNum + 
								   psPrevMemTexStateData->uRegCount;

				if	(uPrevTexStateEnd == psTexStateData->uRegNum)
				{
					IMG_UINT32	uMergedFetchCount;

					uMergedFetchCount = uPrevLoadFetchCount + psTexStateData->uRegCount;
					if	(uMergedFetchCount <= HW_INST_LD_MAX_REPEAT_COUNT)
					{
						bMergeLoad = IMG_TRUE;
					}
				}
			}

			/*
				Setup or modify a load instruction to fetch this texture-state
				from memory.
			*/
			if	(!bMergeLoad)
			{
				/*
					Setup the SA-register containing the base address for all in-memory
					constants and texture-state
				*/
				sBaseAddrReg.eType		= USP_REGTYPE_SA;
				sBaseAddrReg.uNum		= psProgDesc->uMemConstBaseAddrSAReg;
				sBaseAddrReg.eFmt		= USP_REGFMT_UNKNOWN;
				sBaseAddrReg.uComp		= 0;
				sBaseAddrReg.eDynIdx	= USP_DYNIDX_NONE;

				/*
					Setup the offset (in DWORDs) to the texture-state for this chunk

					NB: All in-memory texture-state is placed immediately after any
						constants.
				*/
				sAddrOffReg.eType		= USP_REGTYPE_IMM;
				sAddrOffReg.uNum		= psProgDesc->uTexStateMemOffset + 
										  psTexStateData->uRegNum;
				sAddrOffReg.eFmt		= USP_REGFMT_UNKNOWN;
				sAddrOffReg.uComp		= 0;
				sAddrOffReg.eDynIdx		= USP_DYNIDX_NONE;

				/*
					Setup the location where the texture-state is to be loaded
				*/
				psTexStateReg->eType	= psSample->eSampleTempRegType;
				psTexStateReg->uNum		= psSample->uSampleTempRegNum + uSampleTempRegCount;
				psTexStateReg->eFmt		= USP_REGFMT_UNKNOWN;
				psTexStateReg->uComp	= 0;
				psTexStateReg->eDynIdx	= USP_DYNIDX_NONE;

				uSampleTempRegCount += psTexStateData->uRegCount;

				/*
					Encode a LDAD instruction to fetch the texture-state				
				*/
				psHWInst = &psSample->asSampleInsts_[uHWInstCount];

				if	(!HWInstEncodeLDInst(psHWInst,
										 bSkipInv,
										 psTexStateData->uRegCount,
										 psTexStateReg,
										 &sBaseAddrReg,
										 &sAddrOffReg,
										 0))
				{
					USP_DBGPRINT(("SampleTextureData: Error encoding LD inst\n"));
					return IMG_FALSE;
				}

				psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_LD;
				psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= 0;

				/*
					Record information about the load added for this texture-state,
					so that we can potentially merge other loads with it.
				*/
				psPrevLoadHWInst		= psHWInst;
				uPrevLoadFetchCount		= psTexStateData->uRegCount;

				uLDInstCount++;
				uHWInstCount++;
			}
			else
			{
				/*
					Just modify the fetch count for the preceeding load to
					fetch this texture-state along with previous state.
				*/
				uPrevLoadFetchCount += psTexStateData->uRegCount;

				if	(!HWInstEncodeLDInstFetchCount(psPrevLoadHWInst,
												   uPrevLoadFetchCount))
				{
					USP_DBGPRINT(("SampleTextureData: Error encoding LD inst fetch count\n"));
					return IMG_FALSE;
				}

				/*
					Setup the location where the texture-state is to be loaded
				*/
				psTexStateReg->eType	= psSample->eSampleTempRegType;
				psTexStateReg->uNum		= psSample->uSampleTempRegNum + uSampleTempRegCount;
				psTexStateReg->eFmt		= USP_REGFMT_UNKNOWN;
				psTexStateReg->uComp	= 0;
				psTexStateReg->eDynIdx	= USP_DYNIDX_NONE;

				uSampleTempRegCount += psTexStateData->uRegCount;
			}

			psPrevMemTexStateData = psTexStateData;
		}
		else
		{
			/*
				Just source the texture-sate directly from SA registers
			*/
			psTexStateReg->eType	= USP_REGTYPE_SA;
			psTexStateReg->uNum		= psTexStateData->uRegNum;
			psTexStateReg->eFmt		= USP_REGFMT_UNKNOWN;
			psTexStateReg->uComp	= 0;
			psTexStateReg->eDynIdx	= USP_DYNIDX_NONE;
		}
	}

	/*
	  Texture coordinates may require normalising, a decision which
	  is told to the driver at runtime.
	 */
	if(!SampleNormaliseCoordinates(psSample,
	                               psContext,
	                               uInstDescFlags,
	                               &uHWInstCount,
	                               &uSampleTempRegCount))
	{
		USP_DBGPRINT(("SampleTextureData: Error encoding normalisation code\n"));
		return IMG_FALSE;
	}

	/*
		Add a WDF instruction to wait for the preceeding LD instructions
		to complete if any texture-state was loaded from memrory
	*/
	if	(uLDInstCount > 0)
	{
		/*
			Encode a WDF to wait for the DRC counter used by the LD inst

			NB: IRegs cannot be live immediately before a WDF, since the instruction
				causes a deschedule (and thus loss of all IReg data).
		*/
		psHWInst = &psSample->asSampleInsts_[uHWInstCount];

		if	(!HWInstEncodeWDFInst(psHWInst, 0))
		{
			USP_DBGPRINT(("SampleTextureData: Error encoding WDF inst\n"));
			return IMG_FALSE;
		}

		psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_WDF;
		psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= 0;

		uHWInstCount++;

		/*
			Record the temporaries used for loading texture-state only
		*/
		psSample->uSampleTempRegCount	= uSampleTempRegCount;
	}

	/*
		Generate sample instructions for each chunk that needs to be sampled		
	*/
	eBaseInstOpcode		= psSample->eBaseInstOpcode;
	psBaseInst			= &psSample->sBaseInst;
	uSMPInstCount		= 0;
	
#if 0	// this code is not needed anymore as the base sample destination register will be probably aligned in case of sgx543
	#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
	if(((psSample->uBaseSampleDestRegNum + uNumTempsUsedBySampling) % 2) &&
	   ((psSample->sSamplerDesc.sTexChanFormat.aeChanForm[0] == USP_CHAN_FORMAT_F16) || 
	    (psSample->sSamplerDesc.sTexChanFormat.aeChanForm[0] == USP_CHAN_FORMAT_F32)))
	{
		psSample->uUnAlignedTempCount++;		
	}
	#endif /* defined(SGX_FEATURE_UNIFIED_STORE_64BITS) */
#endif
	
	for	(uChunkIdx = 0; uChunkIdx < USP_MAX_TEXTURE_CHUNKS; uChunkIdx++)
	{
		PUSP_REG	psChunkReg;

		/*
			Ignore texture chunks that we don't need to sample
		*/
		if	(!(psTexChanInfo->uTexChunkMask & (1 << uChunkIdx)) || (psTexChanInfo->uTexNonDepChunkMask  & (1 << uChunkIdx)))
		{
			continue;
		}

		/*
			Setup the register location for the sampled texture-data for
			this chunk
		*/
		psChunkReg = &psSample->asChunkReg[uChunkIdx];

		psChunkReg->eType	= psSample->eBaseSampleDestRegType;
		psChunkReg->uNum	= psSample->uBaseSampleDestRegNum + uBaseSampleDestRegCount;
		psChunkReg->eFmt	= USP_REGFMT_UNKNOWN;
		psChunkReg->uComp	= 0;
		psChunkReg->eDynIdx	= USP_DYNIDX_NONE;
		
		psSample->abChunkNotSampledToTempReg[uChunkIdx] = IMG_FALSE;

		uBaseSampleDestRegCount += psTexChanInfo->auNumRegsForChunk[uChunkIdx];

		/*
			Modify the base sample instruction's dest and src1 operands to
			reference the correct registers
		*/
		psHWInst	= &psSample->asSampleInsts_[uHWInstCount];
		*psHWInst	= *psBaseInst;

		if(psSample->bNonDependent)
		{			
			USP_REG sSrc0Reg;
			PUSP_ITERATED_DATA psIteratedData;
			
			/* 
				Encodes the dimensions of the non dependent sample when it becomes dependent.
			*/
			if(!HWInstEncodeSMPInstCoordDim(psHWInst, psSample->uTexDim))
			{
				USP_DBGPRINT(("SampleTextureData: Error encoding coordinate dimensions for SMP inst\n"));
				return IMG_FALSE;
			}
			
			/* 
				Get the iterated data for the coordinates of the non dependent sample. 
			*/
			psIteratedData = USPInputDataGetIteratedDataForNonDepSample(psInputData, psSample, psContext);
			if(psIteratedData == IMG_NULL)
			{
				USP_DBGPRINT(("SampleTextureData: Error searching iterated data for the coordinates of non dependent sample to make dependent\n"));
				return IMG_FALSE;
			}

			sSrc0Reg.eType = USP_REGTYPE_PA;
			sSrc0Reg.uNum = psIteratedData->uRegNum;
			/* 
				At the moment coordinate register formate for non dependent samples 
				is not passed by USC to USP so it is assumed to be F32 always.
			*/
			sSrc0Reg.eFmt = USP_REGFMT_F32;
			sSrc0Reg.uComp = 0;
			sSrc0Reg.eDynIdx = USP_DYNIDX_NONE;
			if(!HWInstEncodeSrc0BankAndNum(USP_FMTCTL_NONE, 
										   eBaseInstOpcode,
										   IMG_TRUE, 
										   psHWInst, 
										   &sSrc0Reg))
			{
				USP_DBGPRINT(("SampleTextureData: Error encoding src0 (tex-coordinates) for SMP inst\n"));
				return IMG_FALSE;
			}
		}

		#if defined(SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT)
		if(!HWInstEncodeSMPInstUnPackingFormat(psHWInst, psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].aeUnPackFmts[uChunkIdx]))
		{
			USP_DBGPRINT(("SampleTextureData: Error encoding SMP instruction result unpacking format\n"));
			return IMG_FALSE;
		}
		#endif /* defined(SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT) */

		if	(!HWInstEncodeSrc1BankAndNum(USP_FMTCTL_NONE,
										 eBaseInstOpcode,
										 psHWInst,
										 &asTexStateReg[uChunkIdx]))
		{
			USP_DBGPRINT(("SampleTextureData: Error encoding src1 (tex-state) for SMP inst\n"));
			return IMG_FALSE;
		}

		/*
			It is used if AND instruction is required later for depth F32 texture
		*/
		psANDSrcReg = psChunkReg;

		if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
										 eBaseInstOpcode,
										 psHWInst,
										 psChunkReg))
		{
			USP_DBGPRINT(("SampleTextureData: Error encoding dest for SMP inst\n"));
			return IMG_FALSE;
		}

		psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= eBaseInstOpcode;
		psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

		/*
			Absolute is required by hardware for Depth F32 texture
		*/
		if((psContext->asSamplerDesc[psSample->uTextureIdx].sTexFormat.eTexFmt) 
			!= USP_TEXTURE_FORMAT_DF32)
		{
			/*
				Record the destination channels that will use this chunk of 
				sampled data
			*/
			UpdateSampleDestCahnnels(psSample, psTexChanInfo, psHWInst, uChunkIdx);
		}

		uHWInstCount++;
		uSMPInstCount++;
	}

	/*
		Add a WDF instruction to wait for the preceeding SMP instructions
		to complete
	*/
	if	(uSMPInstCount > 0)
	{
		/*
			Encode a WDF to wait for the DRC counter used by the SMP inst

			NB: IRegs cannot be live immediately before a WDF, since the instruction
				causes a deschedule (and thus loss of all IReg data).
		*/
		psHWInst = &psSample->asSampleInsts_[uHWInstCount];

		if	(!HWInstEncodeWDFInst(psHWInst, psSample->uBaseInstDRCNum))
		{
			USP_DBGPRINT(("SampleTextureData: Error encoding WDF inst\n"));
			return IMG_FALSE;
		}

		psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_WDF;
		psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags &
																  (~USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE);

		uHWInstCount++;
	}

	/*
		Absolute is required by hardware for Depth F32 texture
	*/
	if(((psContext->asSamplerDesc[psSample->uTextureIdx].sTexFormat.eTexFmt)
		== USP_TEXTURE_FORMAT_DF32)	
		&& 
		(psANDSrcReg != IMG_NULL))
	{
		USP_REG		sImdVal;

		sImdVal.eType = USP_REGTYPE_IMM;
		sImdVal.eFmt = USP_REGFMT_F32;
		sImdVal.uNum = 0x7FFFFFFF;
		sImdVal.eDynIdx = USP_DYNIDX_NONE;
		sImdVal.uComp = 0;

		psHWInst = &psSample->asSampleInsts_[uHWInstCount];

		/*
			Encode AND F32 data clearing top Bit
		*/
		if	(!HWInstEncodeANDInst(psHWInst,
								  1,
								  IMG_FALSE, /* bUsePred */
								  IMG_FALSE, /* bNegatePred */
								  0, /* uPredNum */
								  bSkipInv,
								  psANDSrcReg,
								  psANDSrcReg,
								  &sImdVal))
		{
			USP_DBGPRINT(("SampleTextureData: Error encoding AND inst\n"));
			return IMG_FALSE;
		}
		psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_AND;
		psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

		uChunkIdx = 0;
		
		/*
			Record the destination channels that will use this chunk of sampled
			data
		*/
		UpdateSampleDestCahnnels(psSample, psTexChanInfo, psHWInst, uChunkIdx);

		uHWInstCount++;
	}

	if(!ReStoreInternalRegs(psSample, psContext, &uHWInstCount, bSkipInv, uInstDescFlags, uSMPInstCount, uSampleTempRegToStoreIRegs))
	{
		USP_DBGPRINT(("SampleTextureData: Failed to resotre Internal registers\n"));
		return IMG_FALSE;
	}

	/*
		Record the number of temporaries and instructions needed for the
		sampling code
	*/
	psSample->uSampleTempRegCount		= uSampleTempRegCount;
	psSample->uSampleInstCount_			= uHWInstCount;
	psSample->uBaseSampleDestRegCount_	= uBaseSampleDestRegCount;
	psSample->uNumSMPInsts				= uSMPInstCount;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		ColourSpaceConvertTextureData

 Purpose:	Add instructions to perform colour-space conversion on the
			raw sampled texture-data (either dependent or non-dependent)

 Inputs:	psSample	- The dependent sample to generate code for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
static IMG_BOOL ColourSpaceConvertTextureData(PUSP_SAMPLE		psSample,
											  PUSP_CONTEXT		psContext)
{
	/*
		Filter centre and coefficients for single-plane XYUV, for each
		of the RGB channels we may need to generate
	*/
	static const IMG_INT32	aiXYUV_SOffForRGBChan[] = {5, 3, 3};
	static const IMG_UINT32	auXYUV_CoeffSelForRGBChan[] = {2, 1, 0};

	/*
		Filter centre and coefficients for dual-plane YUV420, for each
		of the RGB channels we may need to generate
	*/
	static const IMG_INT32	aiYUV420_SOffForRGBChan[] = {7, 7, 7};
	static const IMG_UINT32	auYUV420_CoeffSelForRGBChan[] = {2, 1, 0};

	/*
		Filter centre and coefficients for three-plane YUV420, for each
		of the RGB channels we may need to generate
	*/
	static const IMG_INT32	aiYUV420_3Plane_SOffForRGBChan[] = {6, 6, 6};

	PUSP_TEX_CHAN_INFO	psTexChanInfo;
	IMG_UINT32			uChunkIdx;
	IMG_UINT32			uNumYUVChunks;
	IMG_UINT32			uRGBTexChanMask;
	IMG_UINT32			uDestChanMask;
	IMG_UINT32			uDestChanIdx;
	IMG_UINT32			uInstDescFlags;
	IMG_BOOL			bSkipInv;
	PUSP_REG			apsYUVChunkReg[USP_MAX_TEXTURE_CHUNKS];
	PUSP_REG			apsFIRHSrcReg[3];
	IMG_UINT32			uFIRHInstCount;
	const IMG_INT32*	piSOffForRGBChan;
	const IMG_UINT32*	puCoeffSelForRGBChan;
	USP_REG				sRGBResultReg;
	IMG_UINT32			uHWInstCount;
	PHW_INST			psLastFIRHInst;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	/*
		Nothing to do if CSC conversion is not needed for the current texture format,
		or we didn't need to sample the texture at all
	*/
	psTexChanInfo = &psSample->sTexChanInfo;
	if	(
			(psTexChanInfo->eTexType != USP_TEX_TYPE_YUV) ||
			(psTexChanInfo->uTexChunkMask == 0)
		)
	{
		return IMG_TRUE;
	}

	/*
		Lookup the registers where we sampled the chunks of YUV data
	*/
	uNumYUVChunks = 0;

	for	(uChunkIdx = 0; uChunkIdx < USP_MAX_TEXTURE_CHUNKS; uChunkIdx++)
	{
		if	((psTexChanInfo->uTexChunkMask & (1 << uChunkIdx)))
		{
			apsYUVChunkReg[uNumYUVChunks] = &psSample->asChunkReg[uChunkIdx];
			uNumYUVChunks++;
		}
	}

	/*
		Setup the source registers and options for the FIRH instructions, based
		upon the YUV format

		NB: Currently we only support two types of YUV format:
			single plane XYUV, or dual-plane YUV420.
	*/
	if (uNumYUVChunks == 1)
	{
		apsFIRHSrcReg[0]		= apsYUVChunkReg[0];
		apsFIRHSrcReg[1]		= apsYUVChunkReg[0];
		apsFIRHSrcReg[2]		= apsYUVChunkReg[0];
		piSOffForRGBChan		= aiXYUV_SOffForRGBChan;
		puCoeffSelForRGBChan	= auXYUV_CoeffSelForRGBChan;
	}
	else if (uNumYUVChunks == 2)
	{
		apsFIRHSrcReg[0]		= apsYUVChunkReg[1];
		apsFIRHSrcReg[1]		= apsYUVChunkReg[1];
		apsFIRHSrcReg[2]		= apsYUVChunkReg[0];
		piSOffForRGBChan		= aiYUV420_SOffForRGBChan;
		puCoeffSelForRGBChan	= auYUV420_CoeffSelForRGBChan;
	}
	else if (uNumYUVChunks == 3)
	{
		apsFIRHSrcReg[0]		= apsYUVChunkReg[2];
		apsFIRHSrcReg[1]		= apsYUVChunkReg[1];
		apsFIRHSrcReg[2]		= apsYUVChunkReg[0];
		piSOffForRGBChan		= aiYUV420_3Plane_SOffForRGBChan;
		puCoeffSelForRGBChan	= auYUV420_CoeffSelForRGBChan;
	}
	else
	{
		USP_DBGPRINT(("ColourSpaceConvertTextureData: Unexpected number of YUV chunks\n"));
		return IMG_FALSE;
	}

	/*
		Currently we only generate 888 RGBx data here. So we can use a single destination
		register for all FIRH instructions
	*/
	sRGBResultReg.eType		= psSample->psSampleUnpack->eSampleUnpackTempRegType;
	sRGBResultReg.uNum		= psSample->psSampleUnpack->uSampleUnpackTempRegNum + psSample->psSampleUnpack->uSampleUnpackTempRegCount;
	sRGBResultReg.uComp		= 0;
	sRGBResultReg.eDynIdx	= USP_DYNIDX_NONE;
	sRGBResultReg.eFmt		= USP_REGFMT_U8;

	/*
		For each RGB channel that is needed for the sample's result, add
		FIRH instrucitons to convert from YUV.
	*/
	uInstDescFlags = psSample->psSampleUnpack->uInstDescFlags & ~(USP_INSTDESC_FLAG_DEST_USES_RESULT |
												  USP_INSTDESC_FLAG_SRC0_USES_RESULT |
												  USP_INSTDESC_FLAG_SRC1_USES_RESULT |
												  USP_INSTDESC_FLAG_SRC2_USES_RESULT |
												  USP_INSTDESC_FLAG_PARTIAL_RESULT_WRITE);

	bSkipInv		= (IMG_BOOL)(!(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV));
	uHWInstCount	= psSample->psSampleUnpack->uUnpackInstCount;
	uFIRHInstCount	= 0;
	psLastFIRHInst	= IMG_NULL;
	uRGBTexChanMask	= 0;
	uDestChanMask	= 0;

	for (uDestChanIdx = 0; uDestChanIdx < USP_MAX_SAMPLE_CHANS; uDestChanIdx++)
	{
		USP_CHAN_SWIZZLE	eTexChanSwizzle;

		/*
			Skip this result channel if we didn't need to sample it
		*/
		if	(!(psSample->psSampleUnpack->uMask & (1 << uDestChanIdx)))
		{
			continue;
		}

		/*
			Add instruction to calculate the R,G or B data we need for this
			destination channel

			NB: For YUV texture formats, the auTexChanSwizzle array indicates how
				the R/G/B channels we generate should be mapped to each dest
				component. Since FIRH allows us to specifiy which byte of the dest
				is being written, we can perform any channel swizzling here (but
				format conversion will still need unpacking code).
		*/
		eTexChanSwizzle = psTexChanInfo->aeTexChanForComp[uDestChanIdx];
		switch (eTexChanSwizzle)
		{
			case USP_CHAN_SWIZZLE_CHAN_0:
			case USP_CHAN_SWIZZLE_CHAN_1:
			case USP_CHAN_SWIZZLE_CHAN_2:
			{
				USP_CHAN_FORMAT		eTexChanFormat;
				IMG_UINT32			uRGBTexChan;
				IMG_INT32			iSoff;
				IMG_UINT32			uCoeffSel;
				PHW_INST			psHWInst;

				/*
					Only generate one each of R, G or B
				*/
				uRGBTexChan = eTexChanSwizzle - USP_CHAN_SWIZZLE_CHAN_0;
				if	(uRGBTexChanMask & (1 << uRGBTexChan))
				{
					break;
				}

				/*
					Generate an FIRH instruction to generate R, B or B in this
					channel of the destination
				*/
				psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
				eTexChanFormat	= psTexChanInfo->aeDataFmtForChan[0];
				uCoeffSel		= puCoeffSelForRGBChan[uRGBTexChan];
				iSoff			= piSOffForRGBChan[uRGBTexChan];
				{
					USP_SRC_FORMAT eSrcFormat;
					switch(eTexChanFormat)
					{
						case USP_CHAN_FORMAT_U8:
						{
							eSrcFormat = USP_SRC_FORMAT_U8;
							break;
						}
						case USP_CHAN_FORMAT_S8:
						{
							eSrcFormat = USP_SRC_FORMAT_S8;
							break;
						}
						default:
						{
							USP_DBGPRINT(("ColourSpaceConvertTextureData: Invalid texture channel format\n"));
							return IMG_FALSE;
						}
					}

					if	(!HWInstEncodeFIRHInst(psHWInst,
											   bSkipInv,
											   eSrcFormat,
											   uCoeffSel,			// uCoeffSel
											   iSoff,				// iSoff
											   uDestChanIdx,		// uPoff
											   &sRGBResultReg,		// Dest
											   apsFIRHSrcReg[0],	// Src0
											   apsFIRHSrcReg[1],	// Src1
											   apsFIRHSrcReg[2]))	// Src2
					{
						USP_DBGPRINT(("ColourSpaceConvertTextureData: Error encoding FIRH\n"));
						return IMG_FALSE;
					}
				}

				psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount]			= USP_OPCODE_FIRH;
				psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount]	= uInstDescFlags;

				/*
					Following FIRH instructions will use the IRegs from the previous one, so
					we must flag that IRegs are live before them
				*/
				uInstDescFlags |= USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;

				uHWInstCount++;
				uFIRHInstCount++;

				/*
					Don't generate this channel again	
				*/
				uRGBTexChanMask |= 1 << uRGBTexChan; 
				uDestChanMask	|= 1 << uDestChanIdx;

				/*
					Record the last FIRH instruciton we have generated, as this
					writes all the R, G or B data we have generated.
				*/
				psLastFIRHInst = psHWInst;

				break;
			}

			case USP_CHAN_SWIZZLE_CHAN_3:
			{
				USP_DBGPRINT(("ColourSpaceConvertTextureData: Attempt to use Alpha channel of YUV texture\n"));
				return IMG_FALSE;
			}

			case USP_CHAN_SWIZZLE_ONE:
			case USP_CHAN_SWIZZLE_ZERO:
			{
				/*
					One/Zero constants will be added during unpacking.
				*/
				continue;
			}

			default:
			{
				USP_DBGPRINT(("ColourSpaceConvertTextureData: Unexpected tex-chan swizzle\n"));
				return IMG_FALSE;
			}
		}
	}

	/*
		If we added any colour-space conversion code, regenerate the texture-info to match
		the data we will have produced here.

		NB: Since we incorporated the required swizzle when we were generating the R, G
			and B channel data (FIRH allows you to specify which destination channel it
			writes), there will never be a need to swizzle the data we generate during
			unpacking. This is why the identity swizzle is used for those channels in
			the new texture format, and also for the corresponding components of the
			swizzle associated with the sample.
	*/
	if	(uFIRHInstCount > 0)
	{
		USP_SAMPLER_CHAN_DESC	sNewSamplerDesc;
		PUSP_SAMPLER_CHAN_DESC	psOrgSamplerDesc;
		IMG_UINT16				uOrgSampleSwizzle;
		IMG_UINT16				uNewSampleSwizzle;

		/*
			Generate a new sampler-description to match the RGB data we have generated
		*/
		psOrgSamplerDesc = &psSample->sSamplerDesc;

		for	(uDestChanIdx = 0; uDestChanIdx < USP_MAX_SAMPLE_CHANS; uDestChanIdx++)
		{
			static const USP_CHAN_SWIZZLE auIdentitySwizzle[] = 
			{
				USP_CHAN_SWIZZLE_CHAN_0,
				USP_CHAN_SWIZZLE_CHAN_1,
				USP_CHAN_SWIZZLE_CHAN_2,
				USP_CHAN_SWIZZLE_CHAN_3
			};
			USP_CHAN_SWIZZLE	eSwizzle;
			USP_CHAN_CONTENT	eContent;

			if	(!(psSample->psSampleUnpack->uMask & (1 << uDestChanIdx)))
			{
				/*
					Don't need to write this channel, so no data in our 'pseudo' RGB format
				*/
				eContent = USP_CHAN_CONTENT_NONE;
				eSwizzle = USP_CHAN_SWIZZLE_ZERO;
			}
			else if (!(uDestChanMask & (1 << uDestChanIdx)))
			{
				/*
					Dest channel written, but we didn't generate any data specifically
					for it. Either:

					a)	It's using the same data as another channel (we only generate 
						R, G or B once)
					b)	A constant ONE/ZERO is required.

					If it's the former, then the swizzle associated with the sample
					will be setup to replicate another channel, so this channel will
					never be used (so the swizzle in the format is irrelevant). 
					
					If it's the latter, we need to copy the original swizzle into our
					'pseudo' RGB format.
				*/
				eContent = USP_CHAN_CONTENT_NONE;
				eSwizzle = psOrgSamplerDesc->sTexChanFormat.aeChanSwizzle[uDestChanIdx];
			}
			else
			{	   
				/*
					We have generated the required R, G or B data for this dest channel,
					so we can use an identity swizzle.
				*/
				eContent = USP_CHAN_CONTENT_DATA;
				eSwizzle = auIdentitySwizzle[uDestChanIdx];
			}

			sNewSamplerDesc.sTexChanFormat.aeChanContent[uDestChanIdx]	= eContent;
			sNewSamplerDesc.sTexChanFormat.aeChanForm[uDestChanIdx]		= USP_CHAN_FORMAT_U8;
			sNewSamplerDesc.sTexChanFormat.aeChanSwizzle[uDestChanIdx]	= eSwizzle;
			sNewSamplerDesc.sTexChanFormat.auChunkSize[uDestChanIdx]	= 4;
		}
		sNewSamplerDesc.bAnisoEnabled = psOrgSamplerDesc->bAnisoEnabled;
		sNewSamplerDesc.bGammaEnabled = psOrgSamplerDesc->bGammaEnabled;

		psSample->sSamplerDesc = sNewSamplerDesc;

		/*
			Update the swizzle associated with the sample, to account for the
			location of the R, G or B data. Doing so allows us to minimise unpacking
			cost (when no format-conversion is needed).
		*/
		uOrgSampleSwizzle = psSample->uSwizzle;
		uNewSampleSwizzle = uOrgSampleSwizzle;

		for	(uDestChanIdx = 0; uDestChanIdx < USP_MAX_SAMPLE_CHANS; uDestChanIdx++)
		{
			if	(uDestChanMask & (1 << uDestChanIdx))
			{
				IMG_UINT32	uOtherDestChan;
				IMG_UINT32	uOrgSwizChan;

				uOrgSwizChan = (uOrgSampleSwizzle >> (uDestChanIdx * 3)) & 0x7;

				for	(uOtherDestChan = 0; uOtherDestChan < USP_MAX_SAMPLE_CHANS; uOtherDestChan++)
				{
					IMG_UINT32	uOtherSwizChan;

					uOtherSwizChan = (uOrgSampleSwizzle >> (uOtherDestChan * 3)) & 0x7;
					if	(uOtherSwizChan == uOrgSwizChan)
					{
						uNewSampleSwizzle &= ~(0x7 << (uOtherDestChan * 3));
						uNewSampleSwizzle |= uDestChanIdx << (uOtherDestChan * 3);
					}
				}
			}
		}

		psSample->uSwizzle = uNewSampleSwizzle;

		/*
			Re-calculate how the data for this texture is organised
		*/
		if	(!GetTexChanInfo(psContext,
							 psSample,
							 &psSample->sSamplerDesc,
							 &psSample->sTexChanInfo))
		{
			USP_DBGPRINT(("ColourSpaceConvertTextureData: Error calculating texture-info\n"));
			return IMG_FALSE;
		}

		/*
			Record where we have 'sampled' the single chunk of RGBx data
		*/
		psSample->asChunkReg[0] = sRGBResultReg;
		psSample->abChunkNotSampledToTempReg[0] = IMG_FALSE;

		/*
			Flag that the sample is dependent-now, since unpacking will use the
			results of the FIRH instructions (essentially, the sampling code)
		*/
		psSample->bNonDependent = IMG_FALSE;

		/*
			The last FIRH instruction generates all R, G or B data we need.
		*/
		for	(uDestChanIdx = 0; uDestChanIdx < USP_MAX_SAMPLE_CHANS; uDestChanIdx++)
		{
			if	(uDestChanMask & (1 << uDestChanIdx))
			{
				psSample->apsSMPInsts[uDestChanIdx] = psLastFIRHInst;
			}
		}

		/*
			Account for the additional instructions and temp-reg we have used

			NB: The last FIRH instructions effectively become the sampling instruction
				for this sample (at-least with respect to unpacking). However,
				any instructions or temps used by previous sampline code must still
				be accounted for.

				Additionally, if we generated multiple FIRH instructions, all but the
				last must write to a temporary location. Therefore, regardless of 
				whether we modify the final FIRH later (to write directly to the intended
				destination) we must ensure the temp used for previous FIRHs is 
				accounted-for. To do this, we pretend the sampling code doesn't use any
				temps if we generate multiple FIRHs (the unpacking code will subtract this
				value from the overall temp-requirements for the sample if it manages
				to sample directly to the intended destination for all channels).
		*/
		//psSample->uNumTempRegsUsed++;
		(psSample->psSampleUnpack->uSampleUnpackTempRegCount)++;
		psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;
		if	(uFIRHInstCount > 1)
		{
			psSample->psSampleUnpack->uBaseSampleDestRegCount = 0;
		}
		else
		{
			psSample->psSampleUnpack->uBaseSampleDestRegCount = 1;
		}
		psSample->uNumSMPInsts = 1;
		psSample->psSampleUnpack->asSampleInsts = psSample->psSampleUnpack->asUnpackInsts;
		psSample->psSampleUnpack->aeOpcodeForSampleInsts = psSample->psSampleUnpack->aeOpcodeForUnpackInsts;
		psSample->psSampleUnpack->auInstDescFlagsForSampleInsts = psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts;
		psSample->psSampleUnpack->puBaseSampleDestRegCount = &(psSample->psSampleUnpack->uSampleUnpackTempRegCount);
		psSample->psSampleUnpack->puBaseSampleTempDestRegCount = &(psSample->psSampleUnpack->uBaseSampleDestRegCount);
	}

	return IMG_TRUE;
}
//#endif

/******************************************************************************
 Name:		FindPreSampledTextureData

 Purpose:	Setup the location of the pre-sampled texture-data needed by a
			non-dependent sample

 Inputs:	psSample	- The non-dependent sample to update
			psInputData	- The input-data available for the shader
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL FindPreSampledTextureData(PUSP_SAMPLE		psSample,
										  PUSP_INPUT_DATA	psInputData,
										  PUSP_CONTEXT		psContext)
{
	PUSP_TEX_CHAN_INFO		psTexChanInfo;
	USP_ITERATED_DATA_TYPE	eCoord;
	IMG_UINT32				uTextureIdx;
	IMG_UINT32				uChunkIdx;
	IMG_BOOL				bProjected;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	/*
		Get the location of the pre-sampled texture-data for each
		chunk that is needed by the sample
	*/
	psTexChanInfo	= &psSample->sTexChanInfo;
	uTextureIdx		= psSample->uTextureIdx;
	eCoord			= psSample->eCoord;
	bProjected		= psSample->bProjected;

	for	(uChunkIdx = 0; uChunkIdx < USP_MAX_TEXTURE_CHUNKS; uChunkIdx++)
	{
		PUSP_PRESAMPLED_DATA	psPreSampledData;
		PUSP_REG				psChunkReg;
		IMG_BOOL				bChunkRegIsPA;

		/*
			Ignore unused chunks
		*/
		if	(!(psTexChanInfo->uTexChunkMask & (1 << uChunkIdx)) || !(psTexChanInfo->uTexNonDepChunkMask  & (1 << uChunkIdx)))
		{
			continue;
		}

		/*
			Lookup information describing the chosen location for
			this pre-sampled texture-chunk, and save it in the sample
			to be used when unpacking the data.
		*/
		psPreSampledData = USPInputDataGetPreSampledData(psInputData,
														 uTextureIdx,
														 uChunkIdx,
														 psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTexWrdIdx[uChunkIdx],
														 eCoord,
														 bProjected,
														 psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].aeUnPackFmts[uChunkIdx]);

		if	(!psPreSampledData)
		{
			USP_DBGPRINT(("FindPreSampledTextureData: PreSampled texture not found\n"));
			return IMG_FALSE;
		}

		/*
			Record the register details of the location for this sampled chunk
		*/
		psChunkReg = &psSample->asChunkReg[uChunkIdx];

		psChunkReg->eType	= USP_REGTYPE_PA;
		psChunkReg->uNum	= psPreSampledData->uRegNum;
		psChunkReg->eFmt	= USP_REGFMT_UNKNOWN;
		psChunkReg->uComp	= 0;
		psChunkReg->eDynIdx	= USP_DYNIDX_NONE;
		bChunkRegIsPA		= IMG_TRUE;
		
		psSample->abChunkNotSampledToTempReg[uChunkIdx] = IMG_TRUE;

		/*
			Absolute is required by hardware for Depth F32 texture
		*/
		if
		(
			(	(psContext->asSamplerDesc[psSample->uTextureIdx].sTexFormat.eTexFmt)
				== USP_TEXTURE_FORMAT_DF32
			) ||
			(
				(	
					(psSample->psSampleUnpack->eDirectSrcRegType != psSample->eSmpNdpDirectSrcRegType) ||
					(psSample->psSampleUnpack->uDirectSrcRegNum != psSample->uSmpNdpDirectSrcRegNum)
				) && 
				(
					!((psSample->psShader->psProgDesc->uUFFlags) & UF_MSAATRANS) && 
					(psSample->eSmpNdpDirectSrcRegType != USP_REGTYPE_PA)
				)
			)
		)	
		{
			IMG_UINT32 uChunkRegIdx;
			USP_REG		sChunkReg = *psChunkReg;
			for(uChunkRegIdx = 0; uChunkRegIdx < psTexChanInfo->auNumRegsForChunk[uChunkIdx]; uChunkRegIdx++)
			{
				USP_REG		sImdVal;
				USP_REG		sTempReg;			
				PHW_INST	psHWInst;
				IMG_UINT32	uHWInstCount = psSample->uSampleInstCount_;
				IMG_BOOL	bSkipInv;
				IMG_UINT32	uInstDescFlags;

				uInstDescFlags	= psSample->uInstDescFlags & ~(USP_INSTDESC_FLAG_DEST_USES_RESULT);

				bSkipInv = (IMG_BOOL)(!(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV));


				sImdVal.eType = USP_REGTYPE_IMM;
				sImdVal.eFmt = USP_REGFMT_F32;
				if(	(psContext->asSamplerDesc[psSample->uTextureIdx].sTexFormat.eTexFmt)
					== USP_TEXTURE_FORMAT_DF32 )
				{
					sImdVal.uNum = 0x7FFFFFFF;
				}
				else
				{
					sImdVal.uNum = 0xFFFFFFFF;
				}
				sImdVal.eDynIdx = USP_DYNIDX_NONE;
				sImdVal.uComp = 0;

				psHWInst = &psSample->asSampleInsts_[uHWInstCount];

				if
				(
					(
						((psSample->psShader->psProgDesc->uShaderType) == USP_PC_SHADERTYPE_PIXEL) 
						&& 
						((psSample->psShader->psProgDesc->uUFFlags) & UF_MSAATRANS)
					) ||
					(
						(psSample->psSampleUnpack->eDirectSrcRegType != psSample->eSmpNdpDirectSrcRegType) ||
						(psSample->psSampleUnpack->uDirectSrcRegNum != psSample->uSmpNdpDirectSrcRegNum)			
					)
				)
				{
					
					/*
						PA register cannot be re-written in this case.
						New temporary register is required.
					*/
					IMG_UINT32	uBaseSampleDestRegCount = (psSample->uBaseSampleDestRegCount_);
					sTempReg.eType = psSample->eBaseSampleDestRegType;
					sTempReg.eFmt = USP_REGFMT_F32;
					sTempReg.uNum = (psSample->uBaseSampleDestRegNum) + uBaseSampleDestRegCount;
					sTempReg.eDynIdx = USP_DYNIDX_NONE; 
					sTempReg.uComp = 0;
					uBaseSampleDestRegCount++;				
					psSample->uBaseSampleDestRegCount_ = uBaseSampleDestRegCount;
					bChunkRegIsPA = IMG_FALSE;
				}
				else
				{
					sTempReg = sChunkReg;
					bChunkRegIsPA = IMG_TRUE;
				}

				/*
					Encode AND F32 data clearing top Bit
				*/				
				if	(!HWInstEncodeANDInst(psHWInst,
										  1,
										  IMG_FALSE, /* bUsePred */
										  IMG_FALSE, /* bNegatePred */
										  0,
										  bSkipInv,
										  &sTempReg,
										  &sChunkReg,
										  &sImdVal))
				{
					USP_DBGPRINT(("FindPreSampledTextureData: Error encoding AND inst\n"));
					return IMG_FALSE;
				}
				psSample->aeOpcodeForSampleInsts_[uHWInstCount]			= USP_OPCODE_AND;
				psSample->auInstDescFlagsForSampleInsts_[uHWInstCount]	= uInstDescFlags;

				/* 
					Update sample instruction references for each channel
				*/
				/*{
					IMG_UINT32 uChanIdx;
					for(uChanIdx=0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
					{
						psSample->apsSMPInsts[uChunkRegIdx] = psHWInst;
					}
				}*/
				UpdateSampleDestCahnnels(psSample, psTexChanInfo, psHWInst, uChunkIdx);

				/*
					Update the chunk register
				*/
				if(uChunkRegIdx == 0)
				{
					psSample->asChunkReg[uChunkIdx] = sTempReg;
					psSample->abChunkNotSampledToTempReg[uChunkIdx] = bChunkRegIsPA;
				}

				uHWInstCount++;
				psSample->uSampleInstCount_ = uHWInstCount;
				sChunkReg.uNum++;
			}
		}
	}

	return IMG_TRUE;
}

/******************************************************************************
 Name:		SetupChunkRegsForUnpackPhase

 Purpose:	Setup sample chunk regs. in context of unpacking phase of sampling

 Inputs:	psSample	- The sample to update chunk regs

 Outputs:	none

 Returns:	Nothing
*****************************************************************************/
static IMG_VOID SetupChunkRegsForUnpackPhase(PUSP_SAMPLE		psSample)
{
	IMG_UINT32			uChunkIdx;
	PUSP_TEX_CHAN_INFO	psTexChanInfo;	

	psTexChanInfo	= &psSample->sTexChanInfo;
	for	(uChunkIdx = 0; uChunkIdx < USP_MAX_TEXTURE_CHUNKS; uChunkIdx++)
	{		
		/*
			Ignore texture chunks which are not sampled
		*/
		if	(
				(!((psTexChanInfo->uTexChunkMask & (1 << uChunkIdx)) || (psTexChanInfo->uTexNonDepChunkMask  & (1 << uChunkIdx)))) ||
				psSample->abChunkNotSampledToTempReg[uChunkIdx]
			)
		{
			continue;
		}
		psSample->asChunkReg[uChunkIdx].uNum = psSample->psSampleUnpack->uBaseSampleUnpackSrcRegNum + (psSample->asChunkReg[uChunkIdx].uNum - psSample->uBaseSampleDestRegNum);
		psSample->asChunkReg[uChunkIdx].eType = psSample->psSampleUnpack->eBaseSampleUnpackSrcRegType;		
	}
	psSample->psSampleUnpack->uBaseSampleUnpackSrcRegCount = psSample->uBaseSampleDestRegCount_;
}

/******************************************************************************
 Name:		CalcSrcRegForTexChan

 Purpose:	Setup the location of the source data for a specific texture
			channel

 Inputs:	psSample			- The sample for which the source-reg is needed
			psTexChanInfo		- Information describing the arrangement of 
								  sampled texture data
			uTexChanIdx			- Index of the texture 
			psSrcReg			- The location to setup
			peDataFmtForChan	- The format of the texture-data

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID CalcSrcRegForTexChan(PUSP_SAMPLE		psSample,
									 PUSP_TEX_CHAN_INFO	psTexChanInfo,
									 IMG_UINT32			uTexChanIdx,
									 PUSP_REG			psSrcReg,
									 PUSP_CHAN_FORMAT	peDataFmtForChan)
{
	static const USP_REGFMT aeUSPRegFmtForChanFmt[] = 
	{
		USP_REGFMT_U8,			/* USP_CHAN_FORMAT_U8 */
		USP_REGFMT_UNKNOWN,		/* USP_CHAN_FORMAT_S8 */
		USP_REGFMT_UNKNOWN,		/* USP_CHAN_FORMAT_U16 */
		USP_REGFMT_UNKNOWN,		/* USP_CHAN_FORMAT_S16 */
		USP_REGFMT_F16,			/* USP_CHAN_FORMAT_F16 */
		USP_REGFMT_F32,			/* USP_CHAN_FORMAT_F32 */
		USP_REGFMT_UNKNOWN		/* USP_CHAN_FORMAT_U24 */
	};

	USP_CHAN_FORMAT	eDataFmtForChan;
	IMG_UINT32		uChunkIdx;
	IMG_UINT32		uChunkRegNum;
	IMG_UINT32		uChunkRegOff;
	IMG_UINT32		uChanRegOff;
	IMG_UINT32		uChanRegNum;

	/*
		Calculate the register that the data for this channel has been 
		sampled into
	*/
	eDataFmtForChan	= psTexChanInfo->aeDataFmtForChan[uTexChanIdx];
	uChunkIdx		= psTexChanInfo->auChunkIdxForChan[uTexChanIdx];
	uChunkRegNum	= psSample->asChunkReg[uChunkIdx].uNum;
	uChunkRegOff	= psTexChanInfo->auDestRegForChunk[uChunkIdx];
	uChanRegOff		= psTexChanInfo->auDestRegForChan[uTexChanIdx];
	uChanRegNum		= uChunkRegNum + (uChanRegOff - uChunkRegOff);

	/*
		Setup the source register for this channel of the texture
	*/
	psSrcReg->eType		= psSample->asChunkReg[uChunkIdx].eType;
	psSrcReg->uNum		= uChanRegNum;
	psSrcReg->uComp		= psTexChanInfo->auDestRegCompForChan[uTexChanIdx];
	psSrcReg->eFmt		= aeUSPRegFmtForChanFmt[eDataFmtForChan];
	psSrcReg->eDynIdx	= USP_DYNIDX_NONE;

	/*
		Return the original format of the texture-data
	*/
	*peDataFmtForChan = eDataFmtForChan;
}

/*
	Indices into the hardware's special constant bank.
*/
#if defined(SGX_FEATURE_USE_VEC34)
#define HW_CONST_FLOAT32_ONE	SGXVEC_USE_SPECIAL_CONSTANT_ONE_ONE
#define HW_CONST_FLOAT16_ONE	SGXVEC_USE_SPECIAL_CONSTANT_F16_ONE_ONE_ONE_ONE
#define HW_CONST_ZERO			SGXVEC_USE_SPECIAL_CONSTANT_ZERO_ZERO
#define HW_CONST_FFFFFFFF		SGXVEC_USE_SPECIAL_CONSTANT_FFFFFFFF
#else /* defined(SGX_FEATURE_USE_VEC34) */
#define HW_CONST_FLOAT32_ONE	EURASIA_USE_SPECIAL_CONSTANT_FLOAT1
#define HW_CONST_FLOAT16_ONE	EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE
#define HW_CONST_ZERO			EURASIA_USE_SPECIAL_CONSTANT_ZERO
#define HW_CONST_FFFFFFFF		EURASIA_USE_SPECIAL_CONSTANT_INT32ONE
#endif /* defined(SGX_FEATURE_USE_VEC34) */

/******************************************************************************
 Name:		SetupChanSrcRegs

 Purpose:	Setup the location of the source data to be copied into each
			channel of the destination for a sample

 Inputs:	psSample	- The non-dependent sample to update
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL SetupChanSrcRegs(PUSP_SAMPLE	psSample,
								 PUSP_CONTEXT	psContext)
{
	PUSP_TEX_CHAN_INFO    psTexChanInfo;
	PUSP_TEX_CHAN_FORMAT  psTexFormat;
	IMG_UINT32			  uChanIdx;
	IMG_UINT32			  uConstChanMask;
	IMG_UINT32			  auConstChanVal[USP_MAX_SAMPLE_CHANS];

	#if	!defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif	/* #if	!defined(DEBUG) */

	/*
		Generate instructions to save the sampled data for each channel
		into the required final register locations
	*/
	psTexChanInfo	= &psSample->sTexChanInfo;
	psTexFormat		= &psSample->sSamplerDesc.sTexChanFormat;
	uConstChanMask	= 0;

	for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		USP_CHAN_SWIZZLE	eTexChanSwizzle;
		USP_CHAN_FORMAT		eDataFmtForChan;
		PUSP_REG			psChanReg;

		/*
			Skip this channel if we didn't need to sample it
		*/
		if	(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
		{
			continue;
		}

		/*
			Setup the source register for the texture channel to be
			sampled into this channel of the destination
		*/
		eTexChanSwizzle = psTexChanInfo->aeTexChanForComp[uChanIdx];
		psChanReg		= &psSample->asChanReg[uChanIdx];
		eDataFmtForChan	= (USP_CHAN_FORMAT)0;

		switch (eTexChanSwizzle)
		{
			case USP_CHAN_SWIZZLE_CHAN_0:
			case USP_CHAN_SWIZZLE_CHAN_1:
			case USP_CHAN_SWIZZLE_CHAN_2:
			case USP_CHAN_SWIZZLE_CHAN_3:
			{
				/*
					What does this channel of the sampled data contain?
				*/
				switch (psTexFormat->aeChanContent[eTexChanSwizzle])
				{
					case USP_CHAN_CONTENT_DATA:
					case USP_CHAN_CONTENT_Y:
					case USP_CHAN_CONTENT_U:
					case USP_CHAN_CONTENT_V:
					{
						/*
							Setup the source register for this channel of the texture
						*/
						CalcSrcRegForTexChan(psSample, 
											 psTexChanInfo, 
											 eTexChanSwizzle, 
											 psChanReg,
											 &eDataFmtForChan);

						break;
					}

					case USP_CHAN_CONTENT_ONE:
					{
						/*
							If the destination or sample format is F32, source zero from an
							immediate or builtin constant rather than the sampled-data.
						*/
						if	(	(!(psSample->bSamplerUnPacked)) && 
								((psSample->psSampleUnpack->asDest[uChanIdx].eFmt == USP_REGFMT_F32) ||
								 (psTexFormat->aeChanForm[eTexChanSwizzle] == USP_CHAN_FORMAT_F32))
							)
						{
							/*
								Flag that we need to source one for this channel
							*/
							uConstChanMask |= 1 << uChanIdx;
							auConstChanVal[uChanIdx] = 1;
						}
						else
						{
							/*
								Setup the source register for this channel of the texture
							*/
							CalcSrcRegForTexChan(psSample, 
												 psTexChanInfo, 
												 eTexChanSwizzle, 
												 psChanReg,
												 &eDataFmtForChan);
						}

						break;
					}

					case USP_CHAN_CONTENT_ZERO:
					{
						/*
							If the destination or sample format is F32, source zero from an
							immediate or builtin constant rather than the sampled-data.
						*/
						if	(	(!(psSample->bSamplerUnPacked)) && 
								((psSample->psSampleUnpack->asDest[uChanIdx].eFmt == USP_REGFMT_F32) ||
								 (psTexFormat->aeChanForm[eTexChanSwizzle] == USP_CHAN_FORMAT_F32))
							)
						{
							/*
								Flag that we need to source zero for this channel
							*/
							uConstChanMask |= 1 << uChanIdx;
							auConstChanVal[uChanIdx] = 0;
						}
						else
						{
							/*
								Setup the source register for this channel of the texture
							*/
							CalcSrcRegForTexChan(psSample, 
												 psTexChanInfo, 
												 eTexChanSwizzle, 
												 psChanReg,
												 &eDataFmtForChan);
						}

						break;
					}

					case USP_CHAN_CONTENT_NONE:
					default:
					{
						USP_DBGPRINT(("SetupChanSrcRegs: Invalid texture-format content\n"));
						return IMG_FALSE;
					}
				}

				break;
			}

			case USP_CHAN_SWIZZLE_ONE:
			{
				IMG_BOOL bUseHWConst;

				/*
					If the sampled texture-data contains a one, use that instead
					of a constant, to simplify the unpacking code.

					NB: For F32 destinations there is no scope for combining the write
						of the destination with that of another channel, so using the
						sampled-data as a constant source has no advantages over an
						immediate or builtin constant register. 

						For F32 sampled-data, using an immediate or builtin constant
						avoids having to sample that chunk of the texture altogether,
						so although it may lead to an extra unpacking instruction, it
						reduces the amount of sampling code too. Thus, we don't use
						the sampled data in that case.
				*/
				bUseHWConst = IMG_TRUE;

				if	(psSample->psSampleUnpack->asDest[uChanIdx].eFmt != USP_REGFMT_F32)
				{
					IMG_UINT32	uTexChanIdx;

					for	(uTexChanIdx = 0; uTexChanIdx < USP_MAX_SAMPLE_CHANS; uTexChanIdx++)
					{
						if	(
								(psTexFormat->aeChanContent[uTexChanIdx] == USP_CHAN_CONTENT_ONE) &&
								(psTexFormat->aeChanForm[uTexChanIdx] != USP_CHAN_FORMAT_F32)
							)
						{
							CalcSrcRegForTexChan(psSample, 
												 psTexChanInfo, 
												 uTexChanIdx, 
												 psChanReg,
												 &eDataFmtForChan);

							bUseHWConst = IMG_FALSE;
							break;
						}
					}
				}

				if	(bUseHWConst)
				{
					/*
						Flag that we need to source one for this channel
					*/
					uConstChanMask |= 1 << uChanIdx;
					auConstChanVal[uChanIdx] = 1;
				}

				break;
			}

			case USP_CHAN_SWIZZLE_ZERO:
			{
				IMG_BOOL bUseHWConst;

				/*
					If the sampled texture-data contains a zero, use that instead
					of a constant, to simplify the unpacking code.

					NB: For F32 destinations there is no scope for combining the write
						of the destination with that of another channel, so using the
						sampled-data as a constant source has no advantages over an
						immediate or builtin constant register. 

						For F32 sampled-data, using an immediate or builtin constant
						avoids having to sample that chunk of the texture altogether,
						so although it may lead to an extra unpacking instruction, it
						reduces the amount of sampling code too. Thus, we don't use
						the sampled data in that case.
				*/
				bUseHWConst	= IMG_TRUE;

				if	(psSample->psSampleUnpack->asDest[uChanIdx].eFmt != USP_REGFMT_F32)
				{
					IMG_UINT32	uTexChanIdx;

					for	(uTexChanIdx = 0; uTexChanIdx < USP_MAX_SAMPLE_CHANS; uTexChanIdx++)
					{
						if	(
								(psTexFormat->aeChanContent[uTexChanIdx] == USP_CHAN_CONTENT_ZERO) &&
								(psTexFormat->aeChanForm[uTexChanIdx] != USP_CHAN_FORMAT_F32)
							)
						{
							CalcSrcRegForTexChan(psSample, 
												 psTexChanInfo, 
												 uTexChanIdx, 
												 psChanReg,
												 &eDataFmtForChan);

							bUseHWConst = IMG_FALSE;
							break;
						}
					}
				}

				if	(bUseHWConst)
				{
					/*
						Flag that we need to source zero for this channel
					*/
					uConstChanMask |= 1 << uChanIdx;
					auConstChanVal[uChanIdx] = 0;
				}

				break;
			}

			default:
			{
				USP_DBGPRINT(("SetupChanSrcRegs: Invalid texture channel swizzle\n"));
				return IMG_FALSE;
			}
		}

		/*
			Record the source data format for this destnation channel, provided
			it isn't to be filled with a constant
		*/
		if	(!(uConstChanMask & (1 << uChanIdx)))
		{
			psSample->aeDataFmtForChan[uChanIdx] = eDataFmtForChan;
		}
	}

	if	(uConstChanMask)
	{
		IMG_UINT32	uSampledChanMask;

		/*
			Setup a mask of the destination channels using the sampled
			data (i.e. all those that will not have constant data)
		*/
		uSampledChanMask = psSample->psSampleUnpack->uMask & ~uConstChanMask;

		/*
			Setup source register into for channels that need to be filled with
			zero or one (and aren't coming from the sampled-data)
		*/
		for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
		{
			USP_CHAN_FORMAT		eDataFmtForChan;
			PUSP_REG			psChanReg;
			USP_REGFMT			eConstRegFmt;

			/*
				Skip this channel if it doesn't need a constant value
			*/
			if	(!(uConstChanMask & (1 << uChanIdx)))
			{
				continue;
			}

			/*
				Pick the most suitable format for the constant, to minimise
				the unpacking code required
			*/
			eDataFmtForChan = (USP_CHAN_FORMAT)0;
			eConstRegFmt	= USP_REGFMT_UNKNOWN;

			if	(uSampledChanMask & ((1 << uChanIdx) - 1))
			{
				IMG_INT32			iSampledChanIdx;

				/*
					Look for the preceding channel that's using sampled data
				*/
				iSampledChanIdx = uChanIdx - 1;
				while (iSampledChanIdx >= 0)
				{
					if	(uSampledChanMask & (1 << iSampledChanIdx))
					{
						eDataFmtForChan	= psSample->aeDataFmtForChan[iSampledChanIdx];
						eConstRegFmt	= psSample->asChanReg[iSampledChanIdx].eFmt;
						break;
					}
					iSampledChanIdx--;
				}
			}
			else if (uSampledChanMask & ((1 << USP_MAX_SAMPLE_CHANS) - (2 << uChanIdx)))
			{
				IMG_UINT32			uSampledChanIdx;

				/*
					Look for following channel that's using sampled data
				*/
				uSampledChanIdx = uChanIdx + 1;
				while (uSampledChanIdx < USP_MAX_SAMPLE_CHANS)
				{
					if	(uSampledChanMask & (1 << uSampledChanIdx))
					{
						eDataFmtForChan	= psSample->aeDataFmtForChan[uSampledChanIdx];
						eConstRegFmt	= psSample->asChanReg[uSampledChanIdx].eFmt;
						break;
					}
					uSampledChanIdx++;
				}
			}
			else
			{
				static const USP_CHAN_FORMAT aeRegFmtToChanFmt[] = 
				{
					USP_CHAN_FORMAT_U8,		/* USP_REGFMT_UNKNOWN	= 0	*/
					USP_CHAN_FORMAT_F32,	/* USP_REGFMT_F32		= 1	*/
					USP_CHAN_FORMAT_F16,	/* USP_REGFMT_F16		= 2	*/
					USP_CHAN_FORMAT_U8,		/* USP_REGFMT_C10		= 3	*/
					USP_CHAN_FORMAT_U8		/* USP_REGFMT_U8		= 4	*/
				};

				/*
					All channels are constant, so just use the same format
					as the destination
				*/
				eConstRegFmt	= psSample->psSampleUnpack->asDest[uChanIdx].eFmt;
				eDataFmtForChan	= aeRegFmtToChanFmt[eConstRegFmt];
			}

			/*
				Setup register info to source immediate 0 or 1

				NB: When the shosen format for the constant matches that of the
					destination, we use the same component as the destination to 
					potentially minimise the number of unpacking instructions
			*/
			psChanReg = &psSample->asChanReg[uChanIdx];

			if	(auConstChanVal[uChanIdx] == 0)
			{
				/*
					The builtin 0.0 constant can be used for all formats
				*/
				psChanReg->eType	= USP_REGTYPE_SPECIAL;
				psChanReg->uNum		= HW_CONST_ZERO;
				if(eConstRegFmt == USP_REGFMT_UNKNOWN)
				{
					/* Use F32 as constant format */

					psChanReg->eFmt	= USP_REGFMT_F32;
				}
				else
				{
					psChanReg->eFmt	= eConstRegFmt;
				}
				psChanReg->eDynIdx	= USP_DYNIDX_NONE;

				if	(eConstRegFmt == psSample->psSampleUnpack->asDest[uChanIdx].eFmt)
				{
					psChanReg->uComp = psSample->psSampleUnpack->asDest[uChanIdx].uComp;
				}
				else
				{
					psChanReg->uComp = 0;
				}
			}
			else
			{
				/*
					Use the appropriate builtin constant for the chosen format.
				*/
				switch (eConstRegFmt)
				{
					case USP_REGFMT_UNKNOWN:
					case USP_REGFMT_F32:
					{
						psChanReg->eType	= USP_REGTYPE_SPECIAL;
						psChanReg->uNum		= HW_CONST_FLOAT32_ONE;
						psChanReg->eFmt		= USP_REGFMT_F32;
						psChanReg->uComp	= 0;
						psChanReg->eDynIdx	= USP_DYNIDX_NONE;
						break;
					}

					case USP_REGFMT_F16:
					{
						/*
							Use the builtin HW constant containing 1.0:1.0 in F16

							NB: This constant is only present on HW with an extended ALU.
								For other HW, we must use the F32 1.0 constant.
						*/
						#if defined(SGX_FEATURE_EXTENDED_USE_ALU)
						psChanReg->eType	= USP_REGTYPE_SPECIAL;
						psChanReg->uNum		= HW_CONST_FLOAT16_ONE;
						psChanReg->eFmt		= USP_REGFMT_F16;
						psChanReg->eDynIdx	= USP_DYNIDX_NONE;

						if	(psSample->psSampleUnpack->asDest[uChanIdx].eFmt == USP_REGFMT_F16)
						{
							psChanReg->uComp = psSample->psSampleUnpack->asDest[uChanIdx].uComp;
						}
						else
						{
							psChanReg->uComp = 0;
						}
						#else
						psChanReg->eType	= USP_REGTYPE_SPECIAL;
						psChanReg->uNum		= HW_CONST_FLOAT1;
						psChanReg->eFmt		= USP_REGFMT_F32;
						psChanReg->uComp	= 0;
						psChanReg->eDynIdx	= USP_DYNIDX_NONE;
						#endif

						break;
					}

					case USP_REGFMT_C10:
					case USP_REGFMT_U8:
					{
						/*
							Use the builting 0xFFFFFFFF constant as 4-channels of U8 0xFF (1.0)

							NB: This constant is only present on HW with an extended ALU.
								For other HW, we must use the F32 1.0 constant.
						*/
						psChanReg->eType	= USP_REGTYPE_SPECIAL;
						psChanReg->uNum		= HW_CONST_FFFFFFFF;
						psChanReg->eFmt		= USP_REGFMT_U8;
						psChanReg->uComp	= psSample->psSampleUnpack->asDest[uChanIdx].uComp;
						psChanReg->eDynIdx	= USP_DYNIDX_NONE;

						if	(eConstRegFmt == psSample->psSampleUnpack->asDest[uChanIdx].eFmt)
						{
							psChanReg->uComp = psSample->psSampleUnpack->asDest[uChanIdx].uComp;
						}
						else
						{
							psChanReg->uComp = 0;
						}

						break;
					}

					default:
					{
						USP_DBGPRINT(("SetupChanSrcRegs: Invalid constant format\n"));
						return IMG_FALSE;
					}
				}
			}

			/*
				Record the source data format for this destnation channel
			*/
			psSample->aeDataFmtForChan[uChanIdx] = eDataFmtForChan;
		}
	}

	return IMG_TRUE;
}

/******************************************************************************
 Name:		SampleInstReadsReg

 Purpose:	Determine whether a generated sample instruction references a 
			specific register through it's source operands

 Inputs:	psSample	- The dependent sample to examine
			psHWInst	- Pointer to the SMP/FIRH inst to check
			psReg		- The register to check
			psContext	- The current USP execution context
			pbRegRead	- Set to IMG_TRUE/IMG_FALSE to indicate whether the
						  SMP instruction might read the register
			
 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL SampleInstReadsReg(PUSP_SAMPLE	psSample,
								   PHW_INST		psHWInst,
								   PUSP_REG		psReg,
								   PUSP_CONTEXT	psContext,
								   IMG_PBOOL	pbRegRead)
{
	IMG_UINT32		uHWInstIdx;
	USP_OPCODE		eOpcode;
	PUSP_MOESTATE	psMOEState;
	USP_REG			sSrcReg;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif

	psMOEState	= &psSample->psInstBlock->sFinalMOEState;

	/*
		Lookup the opcode of the specified sample instruction, rather than decoding it
	*/
	uHWInstIdx	= psHWInst - psSample->psSampleUnpack->asSampleInsts;
	eOpcode		= psSample->psSampleUnpack->aeOpcodeForSampleInsts[uHWInstIdx];

	switch (eOpcode)
	{
		case USP_OPCODE_SMP:
		case USP_OPCODE_SMPBIAS:
		case USP_OPCODE_SMPREPLACE:
		case USP_OPCODE_SMPGRAD:
		{
			IMG_UINT32	uCoordDim;
			USP_REGFMT	eCoordFmt;

			/*
				Extract the texture coordinate data format and the
				dimensionality of the source texture afrom the texture sample
				instruction.
			*/
			uCoordDim = HWInstDecodeSMPInstCoordDim(psHWInst);
			eCoordFmt = HWInstDecodeSMPInstCoordFmt(psHWInst);

			/*
				Check whether the specified register may be used by the texture 
				coordinate source (HW source 0)
			*/
			if	(!HWInstDecodeSMPOperandBankAndNum(psMOEState,
												   eOpcode,
												   psHWInst,
												   USP_OPERANDIDX_SRC0,
												   &sSrcReg))
			{
				USP_DBGPRINT(("SampleInstReadsReg: Error decoding sample-inst source 0\n"));
				return IMG_FALSE;
			}

			if	(sSrcReg.eType == psReg->eType)
			{
				IMG_UINT32	uSrcRegCount;

				if	(sSrcReg.eDynIdx != USP_DYNIDX_NONE)
				{
					*pbRegRead = IMG_TRUE;
					return IMG_TRUE;
				}

				switch (eCoordFmt)
				{
					case USP_REGFMT_F32:
					{
						uSrcRegCount = uCoordDim;
						break;
					}
					case USP_REGFMT_F16:
					{
						uSrcRegCount = uCoordDim / 2;
						break;
					}
					case USP_REGFMT_C10:
					{
						uSrcRegCount = 1;
						break;
					}
					default:
					{
						USP_DBGPRINT(("SampleInstReadsReg: Unexpected coord data-format\n"));
						return IMG_FALSE;
					}
				}

				if	(
						(sSrcReg.uNum <= psReg->uNum) &&
						((sSrcReg.uNum + uSrcRegCount) > psReg->uNum)
					)
				{
					*pbRegRead = IMG_TRUE;
					return IMG_TRUE;
				}
			}

			/*
				Check whether the specified register may be used by the gradiant
				or LOD bias/replace source if appropriate (HW source 2).

				NB: These are always F32 format
			*/
			if	(
					(eOpcode == USP_OPCODE_SMPBIAS) ||
					(eOpcode == USP_OPCODE_SMPREPLACE) ||
					(eOpcode == USP_OPCODE_SMPGRAD)
				)
			{
				if	(!HWInstDecodeSMPOperandBankAndNum(psMOEState,
													   eOpcode,
													   psHWInst,
													   USP_OPERANDIDX_SRC2,
													   &sSrcReg))
				{
					USP_DBGPRINT(("SampleInstReadsReg: Error decoding sample-inst source 2\n"));
					return IMG_FALSE;
				}

				if	(sSrcReg.eType == psReg->eType)
				{
					if	(sSrcReg.eDynIdx != USP_DYNIDX_NONE)
					{
						*pbRegRead = IMG_TRUE;
						return IMG_TRUE;
					}

					if	(eOpcode == USP_OPCODE_SMPGRAD)
					{
						IMG_UINT32	uSrcRegCount;

						#if defined(SGX_FEATURE_USE_VEC34)
						if (eCoordFmt == USP_REGFMT_F32)
						{
							if (uCoordDim == 3)
							{
								uSrcRegCount = SGXVEC_USE_SMP_F32_3D_GRADIENTS_SIZE;
							}
							else
							{
								/*
									2 F32 values for each dimension.
								*/
								uSrcRegCount = uCoordDim * 2;
							}
						}
						else
						{
							if (uCoordDim == 3)
							{
								uSrcRegCount = SGXVEC_USE_SMP_F16_3D_GRADIENTS_SIZE;
							}
							else
							{
								/*
									2 F16 values for each dimension.
								*/
								uSrcRegCount = uCoordDim;
							}
						}
						#else /* defined(SGX_FEATURE_USE_VEC34) */
						uSrcRegCount = uCoordDim * 2;
						#endif /* defined(SGX_FEATURE_USE_VEC34) */

						if	(
								(sSrcReg.uNum <= psReg->uNum) &&
								((sSrcReg.uNum + uSrcRegCount) > psReg->uNum)
							)
						{
							{
								*pbRegRead = IMG_TRUE;
								return IMG_TRUE;
							}
						}
					}
					else
					{
						if	(sSrcReg.uNum == psReg->uNum)
						{
							*pbRegRead = IMG_TRUE;
							return IMG_TRUE;
						}
					}
				}
			}
			break;
		}

		case USP_OPCODE_FIRH:
		case USP_OPCODE_AND:
		{
			IMG_UINT32	uSrcIdx;

			/*
				Check whether the specified register is read through any of the 
				three instruction sources
			*/
			for	(uSrcIdx = USP_OPERANDIDX_SRC0; uSrcIdx <= USP_OPERANDIDX_SRC2; uSrcIdx++)
			{
				if	(!HWInstDecodeOperandBankAndNum(psMOEState,
													eOpcode,
													psHWInst,
													uSrcIdx,
													&sSrcReg))
				{
					USP_DBGPRINT(("SampleInstReadsReg: Error decoding sample-inst source 0\n"));
					return IMG_FALSE;
				}

				if	(sSrcReg.eType == psReg->eType)
				{
					if	(sSrcReg.eDynIdx != USP_DYNIDX_NONE)
					{
						*pbRegRead = IMG_TRUE;
						return IMG_TRUE;
					}

					if	(sSrcReg.uNum == psReg->uNum)
					{
						*pbRegRead = IMG_TRUE;
						return IMG_TRUE;
					}
				}
			}
			break;
		}

		default:
		{
			USP_DBGPRINT(("SampleInstReadsReg: Unexpected sampling instruction\n"));
			return IMG_FALSE;
		}
	}

	/*
		Reg not read by the instruction
	*/
	*pbRegRead = IMG_FALSE;
	return IMG_TRUE;
}

static IMG_BOOL UpdateSampleChunkFormat(PUSP_SAMPLE		psSample, 
										IMG_UINT32		uChunkIdx, 
										USP_CHAN_FORMAT	eNewChanFmt)
/******************************************************************************
 Name:		UpdateSampleChunkFormat

 Purpose:	Update the format of all the channles associated with the given 
			chunk

 Inputs:	psSample		- The sample to update format for
			uChunkIdx		- The index of chunk to to update format for
			eNewChanFmt		- The new channel format

 Outputs:	none

 Returns:	IMG_TRUE on success
*****************************************************************************/
{
	PUSP_TEX_CHAN_INFO	psTexChanInfo;
	IMG_UINT32			uChanIdx;

	psTexChanInfo = &psSample->sTexChanInfo;

	/*
		First find the channel for this chunk
	*/
	for(uChanIdx=0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		if(psTexChanInfo->auChunkIdxForChan[uChanIdx] == uChunkIdx)
		{
			psTexChanInfo->aeDataFmtForChan[uChanIdx] = eNewChanFmt;
		}
	}

	return IMG_TRUE;
}

static IMG_BOOL IsSamplingIntegerTextureChunk(PUSP_SAMPLE		psSample, 
											  IMG_UINT32		uChunkIdx,
											  PUSP_CHAN_FORMAT	peChanForm)
/******************************************************************************
 Name:		IsSamplingIntegerTextureChunk

 Purpose:	Checks whether the current chunk of a sample returns integer 
			results

 Inputs:	psSample		- The sample to examine
			uChunkIdx		- The index of chunk to examine

 Outputs:	peChanForm		- The channel format for all the channels written 
							  by this chunk

 Returns:	IMG_TRUE or IMG_FALSE
*****************************************************************************/
{
	PUSP_TEX_CHAN_INFO	psTexChanInfo;
	IMG_UINT32			uChanIdx;
	USP_CHAN_FORMAT		eChanForm;

	psTexChanInfo = &psSample->sTexChanInfo;

	/*
		First find the channel for this chunk
	*/
	for(uChanIdx=0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		if(psTexChanInfo->auChunkIdxForChan[uChanIdx] == uChunkIdx)
		{
			break;
		}
	}

	/*
		Just a safe guard, hopefully it will never happen
	*/
	if(uChanIdx >= USP_MAX_SAMPLE_CHANS)
	{
		uChanIdx = 0;
	}

	/*
		Now get the data format for this channel
	*/
	eChanForm = psTexChanInfo->aeDataFmtForChan[uChanIdx];


	if(eChanForm == USP_CHAN_FORMAT_U16 || 
	   eChanForm == USP_CHAN_FORMAT_S16 || 
	   eChanForm == USP_CHAN_FORMAT_U24 || 
	   eChanForm == USP_CHAN_FORMAT_U8)
	{
		*peChanForm = eChanForm;
		return IMG_TRUE;
	}

	return IMG_FALSE;
}

static USP_REGFMT GetSampleDestinationFormat(PUSP_SAMPLE	psSample)
/******************************************************************************
 Name:		GetSampleDestinationFormat

 Purpose:	Returns the sample destination format 

 Inputs:	psSample		- The sample to get destination format for

 Outputs:	none

 Returns:	The sample destionation regsiter format USP_REGFMT
*****************************************************************************/
{
	USP_REGFMT	eDestFmt;
	IMG_UINT32	uCompIdx;
	IMG_UINT32	uCompMask;

	/*
		Currently, we assume the same data format will be required for all
		components of the destination, despite the USP interface allowing
		a completely independent register specification (including format)
		for each component.
	*/
	uCompMask = psSample->psSampleUnpack->uMask;
	for	(uCompIdx = 0; uCompIdx < USP_MAX_SAMPLE_CHANS; uCompIdx++)
	{
		if	(uCompMask & (1 << uCompIdx))
		{
			break;
		}
	}
	eDestFmt = psSample->psSampleUnpack->asDest[uCompIdx].eFmt;

	return eDestFmt;
}

/******************************************************************************
 Name:		GetSampleChannelFormat

 Purpose:	Returns the channel format for the given channel of the sample. 
			For the channel that requires hardware constant register returns 
			the format of hardware constnat register instead.

 Inputs:	psSample	- The smaple to get channel format for
			uChanIdx	- The channel to get format for

 Outputs:	none

 Returns:	The channel format
*****************************************************************************/
static USP_CHAN_FORMAT	GetSampleChannelFormat(PUSP_SAMPLE	psSample, 
											   IMG_UINT32	uChanIdx)
{
	/*
		Hardware constant registers have their own format. Use the hardware 
		constant register format for channels that are using hardware 
		constant.
	*/
	if(psSample->asChanReg[uChanIdx].eType == USP_REGTYPE_SPECIAL)
	{
		if(psSample->asChanReg[uChanIdx].uNum == HW_CONST_FLOAT32_ONE)
		{
			return USP_CHAN_FORMAT_F32; 
		}
		
		#if defined(SGX_FEATURE_EXTENDED_USE_ALU)
		else if(psSample->asChanReg[uChanIdx].uNum == HW_CONST_FLOAT16_ONE)
		{
			return USP_CHAN_FORMAT_F16; 
		}
		else if(psSample->asChanReg[uChanIdx].uNum == HW_CONST_FFFFFFFF)
		{
			return USP_CHAN_FORMAT_U8; 
		}
		#endif /* defined(SGX_FEATURE_EXTENDED_USE_ALU) */
	}

	return psSample->aeDataFmtForChan[uChanIdx]; 
}

/******************************************************************************
 Name:		UnpackChunksFromTemp

 Purpose:	Check whether 

 Inputs:	psSample		- The non-dependent sample to examine
			psDestReg		- Destination for an unpacking instruction
			uDestWriteMask	- Write mask being used for the destination
			uChanIdx		- Index of the next channel to be unpacked
			psContext		- The current USP execution context
			pbChunksMoved	- Indicates whether chunks of pre-sampled data
							  needed to be moved to temporaries for unpacking

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL UnpackChunksFromTemp(PUSP_SAMPLE	psSample,
									 PUSP_REG		psDestReg,
									 IMG_UINT32		uDestWriteMask,
									 IMG_UINT32		uChanIdx,
									 PUSP_CONTEXT	psContext,
									 IMG_PBOOL		pbChunksMoved)
{
	static const USP_PCKUNPCK_FMT aePCKUNPCKFmtForChanFmt[] =
	{
		USP_PCKUNPCK_FMT_U8,	/* USP_CHAN_FORMAT_U8 */
		USP_PCKUNPCK_FMT_S8,	/* USP_CHAN_FORMAT_S8 */
		USP_PCKUNPCK_FMT_U16,	/* USP_CHAN_FORMAT_U16 */
		USP_PCKUNPCK_FMT_S16,	/* USP_CHAN_FORMAT_S16 */
		USP_PCKUNPCK_FMT_F16,	/* USP_CHAN_FORMAT_F16 */
		USP_PCKUNPCK_FMT_F32,	/* USP_CHAN_FORMAT_F32 */
		USP_PCKUNPCK_FMT_INVALID/* USP_CHAN_FORMAT_U24 */
	};

	PUSP_TEX_CHAN_INFO	psTexChanInfo;
	IMG_UINT32			uChunksToMove;
	IMG_UINT32			uNumChunksToMove;
	IMG_UINT32			uChunkIdx;
	IMG_UINT32			uOtherChanIdx;
	IMG_UINT32			uHWInstCount;

	/*
		By default sampled data is moved
	*/
	*pbChunksMoved = IMG_FALSE;

	/*
		Ignore depednent reads here, as the sampled data is always placed in
		a temporary location and therefore cannot overlap the required final
		destination.
	*/
	if	(!psSample->bNonDependent)
	{
		return IMG_TRUE;
	}

	/*
		Work out which pre-sampled chunks must be copied to a temporary before
		unpacking
	*/
	uHWInstCount		= psSample->psSampleUnpack->uUnpackInstCount;
	psTexChanInfo		= &psSample->sTexChanInfo;
	uChunksToMove		= 0;
	uNumChunksToMove	= 0;

	/*
		Check whether the destination for this channel interferes
		with the source data for any following channels
	*/
	for	(uOtherChanIdx = uChanIdx; 
		 uOtherChanIdx < USP_MAX_SAMPLE_CHANS; 
		 uOtherChanIdx++)
	{
		USP_CHAN_SWIZZLE	eTexChanSwizzle;
		USP_CHAN_FORMAT		eSrcDataFmt;
		PUSP_REG			psSrcReg;
		IMG_UINT32			uSrcUseMask;

		/*
			Ignore unused channels
		*/
		if	(!(psSample->psSampleUnpack->uMask & (1 << uOtherChanIdx)))
		{
			continue;
		}

		/*
			We need to copy the pre-sampled data to a temporary
			location if the source data for this channel overlaps
			with the destination for the other one.
		*/
		psSrcReg = &psSample->asChanReg[uOtherChanIdx];

		if	(psDestReg->eType != psSrcReg->eType)
		{
			continue;
		}

		if	(
				(psDestReg->eDynIdx == USP_DYNIDX_NONE) &&
				(psDestReg->uNum	!= psSrcReg->uNum)
			)
		{
			continue;
		}

		eSrcDataFmt	= GetSampleChannelFormat(psSample, uChanIdx);
		switch (eSrcDataFmt)
		{
			case USP_CHAN_FORMAT_U8:
			case USP_CHAN_FORMAT_S8:
			{
				uSrcUseMask = 0x1 << psSrcReg->uComp;
				break;
			}
			case USP_CHAN_FORMAT_U16:
			case USP_CHAN_FORMAT_S16:
			case USP_CHAN_FORMAT_F16:
			{
				uSrcUseMask = 0x3 << psSrcReg->uComp;
				break;
			}
			case USP_CHAN_FORMAT_U24:
			{
				uSrcUseMask = 0x7;
				break;
			}
			case USP_CHAN_FORMAT_F32:
			{
				uSrcUseMask = 0xF;
				break;
			}
			default:
			{
				USP_DBGPRINT(("UnpackFromTemp: Unhandled source data format\n"));
				return IMG_FALSE;
			}
		}

		if	(!(uDestWriteMask & uSrcUseMask))
		{
			continue;
		}

		/*
			Record that we need to copy the source data for this
			channel to a temporary location
		*/
		eTexChanSwizzle = psTexChanInfo->aeTexChanForComp[uOtherChanIdx];
		if (eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
		{
			uChunkIdx		= psTexChanInfo->auChunkIdxForChan[eTexChanSwizzle];

			if	(!(uChunksToMove & (1 << uChunkIdx)))
			{
				uChunksToMove |= 1 << uChunkIdx;
				uNumChunksToMove++;
				break;
			}
		}
	}

	/*
		Insert instructions to relocate the sampled source data for
		channels that need it
	*/
	if	(uNumChunksToMove > 0)
	{
		IMG_BOOL	bSkipInv;
		IMG_UINT32	uInstDescFlags;
		IMG_UINT32	uChunksRemappedToF16;

		uInstDescFlags = psSample->uInstDescFlags & ~(USP_INSTDESC_FLAG_DEST_USES_RESULT |
													  USP_INSTDESC_FLAG_SRC0_USES_RESULT |
													  USP_INSTDESC_FLAG_SRC1_USES_RESULT |
													  USP_INSTDESC_FLAG_SRC2_USES_RESULT |
													  USP_INSTDESC_FLAG_PARTIAL_RESULT_WRITE);

		bSkipInv = (IMG_BOOL)(!(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV));

		uChunksRemappedToF16 = 0;

		for (uChunkIdx = 0; uChunkIdx < USP_MAX_TEXTURE_CHUNKS; uChunkIdx++)
		{
			PUSP_REG	psChunkReg;
			IMG_UINT32	uChunkRegIdx;
			USP_REGTYPE	eNewRegType = psSample->psSampleUnpack->eSampleUnpackTempRegType;
			IMG_UINT32	uNewRegNum = psSample->psSampleUnpack->uSampleUnpackTempRegNum;
			USP_REG		sChunkReg;

			if	(!(uChunksToMove & (1 << uChunkIdx)))
			{
				continue;
			}

			/*
				Add a MOV instruction to the unpacking code to relocate
				the pre-sampled chunk to a temporary register.
			*/
			psChunkReg = &psSample->asChunkReg[uChunkIdx];

			sChunkReg = *psChunkReg;

			for (uChunkRegIdx = 0; uChunkRegIdx < psTexChanInfo->auNumRegsForChunk[uChunkIdx]; uChunkRegIdx++)
			{
				IMG_UINT32		uChunkRegOff;
				USP_REG			sTempReg;
				PHW_INST		psHWInst;
				PUSP_OPCODE		peOpcode;
				IMG_PUINT32		puInstDescFlags;
				USP_CHAN_FORMAT	eChanForm;

				sTempReg.eType		= psSample->psSampleUnpack->eSampleUnpackTempRegType;
				sTempReg.uNum		= psSample->psSampleUnpack->uSampleUnpackTempRegNum + 
									  psTexChanInfo->auDestRegForChunk[uChunkIdx] + uChunkRegIdx;
				sTempReg.uComp		= 0;
				sTempReg.eFmt		= USP_REGFMT_UNKNOWN;
				sTempReg.eDynIdx	= USP_DYNIDX_NONE;

				psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
				peOpcode		= &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
				puInstDescFlags	= &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

				if(IsSamplingIntegerTextureChunk(psSample, uChunkIdx, &eChanForm) && 
					((eChanForm == USP_CHAN_FORMAT_U16) || 
					 (eChanForm == USP_CHAN_FORMAT_S16)) && 
					GetSampleDestinationFormat(psSample) == USP_REGFMT_U8)
				{
					IMG_UINT32	uWriteMask;
					USP_REG		sChunkReg2 = sChunkReg;
					/* 
						We can unpack here and save some instructions. Scale 
						flag is ignored for integer to integer conversions. 
						So here it can be scaled.
					*/

					/* Set it just like we are doing move */
					sChunkReg2.uComp = 2;

					/* Set that this chunk is remapped to F16 */
					uChunksRemappedToF16 |= (1 << uChunkIdx);

					#if defined(SGX_FEATURE_USE_VEC34)
					uWriteMask = 0x3;
					#else
					uWriteMask = 0xF;
					#endif /* defined(SGX_FEATURE_USE_VEC34) */

					if	(!HWInstEncodePCKUNPCKInst(psHWInst,
												   bSkipInv,
												   USP_PCKUNPCK_FMT_F16,
												   aePCKUNPCKFmtForChanFmt[eChanForm],
												   IMG_TRUE,
												   uWriteMask,
												   &sTempReg,
												   0,
												   1,
												   &sChunkReg,
												   &sChunkReg2))
					{
						USP_DBGPRINT(("UnpackFromTemp: Error encoding PCKUNPCK inst\n"));
						return IMG_FALSE;
					}
					*peOpcode = USP_OPCODE_PCKUNPCK;
				}
				else
				{
					#if defined(SGX_FEATURE_USE_VEC34)
					{
						USP_REG sImdVal;
						sImdVal.eType	= USP_REGTYPE_IMM;
						sImdVal.uNum	= 0xFFFFFFFF;
						sImdVal.eFmt	= sChunkReg.eFmt;
						sImdVal.uComp	= sChunkReg.uComp;
						sImdVal.eDynIdx = USP_DYNIDX_NONE;
						if	(!HWInstEncodeANDInst(psHWInst, 
												  1,
												  IMG_FALSE, /* bUsePred */
												  IMG_FALSE, /* bNegatePred */
												  0, /* uPredNum */
												  bSkipInv,
												  &sTempReg, 
												  &sChunkReg, 
												  &sImdVal))
						{
							USP_DBGPRINT(("UnpackFromTemp: Error encoding data movement (AND) HW-inst\n"));
							return IMG_FALSE;
						}
						*peOpcode = USP_OPCODE_AND;
					}
					#else
					if	(!HWInstEncodeMOVInst(psHWInst,
											  1,
											  bSkipInv,
											  &sTempReg,
											  &sChunkReg))
					{
						USP_DBGPRINT(("UnpackFromTemp: Error encoding MOV HW-inst\n"));
						return IMG_FALSE;
					}
					*peOpcode = USP_OPCODE_MOVC;
					#endif /* defined(SGX_FEATURE_USE_VEC34) */
				}

				*puInstDescFlags = uInstDescFlags;

				uHWInstCount++;

				if(!uChunkRegIdx)
				{
					/*
						Update the chunk register to new register only in first iteration
					*/
					eNewRegType	= sTempReg.eType;
					uNewRegNum	= sTempReg.uNum;
				}

				/*
					Update the number of intermediate registers used for this sample
				*/
				uChunkRegOff = sTempReg.uNum - psSample->psSampleUnpack->uSampleUnpackTempRegNum;
				if	(uChunkRegOff >= psSample->psSampleUnpack->uSampleUnpackTempRegCount)
				{
					psSample->psSampleUnpack->uSampleUnpackTempRegCount = uChunkRegOff + 1;
				}

				/* 
					Move to next resgister as chunk size might be greater than 4
				*/
				sChunkReg.uNum++;
			}

			/*
				If this chunk is remapped to F16
			*/
			if(uChunksRemappedToF16 & (1 << uChunkIdx))
			{
				/*
					Update the format of all channels that forms this chunk
				*/
				UpdateSampleChunkFormat(psSample, uChunkIdx, USP_CHAN_FORMAT_F16);
			}

			/*
				Update the location of the pre-sampled data
			*/
			psSample->asChunkReg[uChunkIdx].eType = eNewRegType;
			psSample->asChunkReg[uChunkIdx].uNum = uNewRegNum;

		}

		/*
			Now that we have relocated some of the sample-data, update
			the source register for each destination channel.
		*/
		if	(!SetupChanSrcRegs(psSample, psContext))
		{
			USP_DBGPRINT(("UnpackFromTemp: Error updating src regs for each channel\n"));
			return IMG_FALSE;
		}

		/*
			Update the number of instructions we have used
		*/
		psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;

		/*
			Indicate that we have moved at-least one chunk to a temporary
		*/
		*pbChunksMoved = IMG_TRUE;
	}

	return IMG_TRUE;
}

#if defined(SGX_FEATURE_TAG_UNPACK_RESULT)
/******************************************************************************
 Name:		UpdateSampleDestinationReg

 Purpose:	Updates the sample destination reg with the destination reg. In 
			case if we can use destination reg directly.

 Inputs:	psContext		- The current USP execution context
			uChanIdx		- The sample channel to update dest for
			psSample		- The sample to update dest for
			sFinalDestReg	- The new dest reg

 Outputs:	none

 Returns:	IMG_TRUE/IMG_FALSE
*****************************************************************************/
static IMG_BOOL UpdateSampleDestinationReg(PUSP_CONTEXT	psContext,
										   IMG_UINT32	uChanIdx,
										   PUSP_SAMPLE	psSample, 
										   USP_REG		sFinalDestReg)
{
	IMG_UINT32	uSampleInstIdx;
	PHW_INST	psSampleInst;
	HW_INST		sTempSampleInst;
	USP_OPCODE	eSampleInstOpcode;
	IMG_BOOL	bDestRegUpdated = IMG_TRUE;
	IMG_UINT32	uInstDescFlags;

	psSampleInst = psSample->apsSMPInsts[uChanIdx];
	uSampleInstIdx = psSampleInst - psSample->psSampleUnpack->asSampleInsts;
	eSampleInstOpcode = psSample->psSampleUnpack->aeOpcodeForSampleInsts[uSampleInstIdx];

	sTempSampleInst = *psSampleInst;

	/*
		Now check can we use dest reg as a dest of sample
	*/
	if	(HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE, 
									eSampleInstOpcode, 
									&sTempSampleInst, 
									&sFinalDestReg))
	{
			IMG_BOOL	bSMPReadsDest;

			/*
				Check whether the destination register may be used
				by one of the sources for the sample instructions.

				NB:	All SMP instructions generated for a sample will be
					identical with respect to the sources they use (apart from
					the state source for SMP), so we only need to check for
					clashes with one inst here.
			*/
			bSMPReadsDest = IMG_FALSE;
			if	(psSample->uNumSMPInsts > 1)
			{
				if (!SampleInstReadsReg(psSample,
										psSampleInst,
										&sFinalDestReg,
										psContext,
										&bSMPReadsDest))
				{
					;
				}
			}
			if(!bSMPReadsDest)
			{
				HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
										   eSampleInstOpcode,
										   psSampleInst, 
										   &sFinalDestReg);
				/*
					Now that this sample instruction writes directly
					to the intended destination, rather than an
					intermediate temporary, it may reference a result
					or input register.
				*/
				uInstDescFlags = psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx];

				uInstDescFlags |= psSample->psSampleUnpack->uInstDescFlags &
													  (USP_INSTDESC_FLAG_DEST_USES_RESULT);

				psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx] = uInstDescFlags;
			}
			else
			{
				bDestRegUpdated = IMG_FALSE;
			}

	}
	return bDestRegUpdated;
}
#endif /* defined(SGX_FEATURE_TAG_UNPACK_RESULT) */

#if defined(SGX545)
/******************************************************************************
 Name:		AreDestRegsConsecutive

 Purpose:	Checks if we can use destination regs directly as sample 
			destination.

 Inputs:	psSample	- The sample to check dest for
			uRegCount	- The number of regs to check
			uChanPerReg - The number of channels per register
			eFmt        - The format of the register

 Outputs:	none

 Returns:	IMG_TRUE/IMG_FALSE
*****************************************************************************/
static IMG_BOOL AreDestRegsConsecutive(PUSP_SAMPLE	psSample, 
									   IMG_UINT32	uRegCount, 
									   IMG_UINT32	uChanPerReg,
									   USP_REGFMT	eFmt)
{
	IMG_UINT32 uChanIdx;
	IMG_BOOL bDestRegsConsecutive = IMG_TRUE;

	PVR_UNREFERENCED_PARAMETER(eFmt);

	for(uChanIdx=0; uChanIdx<(uRegCount*uChanPerReg); uChanIdx++)
	{
		if(psSample->psSampleUnpack->asDest[uChanIdx].eType == USP_REGTYPE_TEMP)
		{
			if(!(((psSample->psSampleUnpack->asDest[uChanIdx].eFmt) == 
				(psSample->asChanReg[uChanIdx].eFmt)) 
				&& 
				((psSample->psSampleUnpack->asDest[uChanIdx].uComp) == 
				(psSample->asChanReg[uChanIdx].uComp))))
			{
				bDestRegsConsecutive = IMG_FALSE;
			}
			else
			{
				if(uChanIdx>0)
				{
					IMG_INT32 iDestNumDiff;
					IMG_INT32 iChanNumDiff;

					iDestNumDiff = 
						(psSample->psSampleUnpack->asDest[uChanIdx].uNum) - (psSample->psSampleUnpack->asDest[uChanIdx-1].uNum);

					iChanNumDiff = 
						(psSample->asChanReg[uChanIdx].uNum) - (psSample->asChanReg[uChanIdx-1].uNum);
					
					if((iChanNumDiff<0) || 
					   (iDestNumDiff != iChanNumDiff))
					{
						bDestRegsConsecutive = IMG_FALSE;
					}
				}
			}
		}
		else
		{
			bDestRegsConsecutive = IMG_FALSE;
		}
	}
	return bDestRegsConsecutive;
}

/******************************************************************************
 Name:		GetTextureUnpackingFormat

 Purpose:	Gets texture unpacking format.

 Inputs:	psContext		- The current USP execution context
			psTexCtrWrds	- The texture control words
			uTexCtrWrdIdx   - The texture ctrl word index to use
			uPlnIdx			- The index of plane to get info for


 Outputs:   puRegsSetCount  - The number of registers set by this unpacking 
							  format
			puChanPerReg	- The number of channels per register
			peRegFmt		- The unpacked register format
			pbMultiPlane	- Is this texture multi plane
			pbSwizOnData    - Is swizzle required after unpacking

 Returns:	IMG_TRUE/IMG_FALSE
*****************************************************************************/
static IMG_BOOL GetTextureUnpackingFormat(PUSP_CONTEXT				psContext, 
										  PUSP_SHDR_TEX_CTR_WORDS	psTexCtrWrds, 
										  IMG_UINT32				uTexCtrWrdIdx, 
										  IMG_UINT32				uPlnIdx,
										  IMG_PUINT32				puRegsSetCount, 
										  IMG_PUINT32				puChanPerReg, 
										  USP_REGFMT				*peRegFmt,
										  IMG_BOOL					*pbMultiPlane,
										  IMG_BOOL					*pbSwizOnData)
{
	IMG_UINT32	uRegsSetCount = (IMG_UINT32)-1;
	IMG_UINT32	uChanPerReg = (IMG_UINT32)-1;
	USP_REGFMT	eRegFmt = USP_REGFMT_UNKNOWN;
	IMG_BOOL	bValid = IMG_TRUE;
	IMG_BOOL	bMultiPlane = IMG_FALSE;
	IMG_BOOL	bSwizOnData = IMG_FALSE;
	IMG_UINT32	uTexIdx = psTexCtrWrds[uTexCtrWrdIdx].uTextureIdx;
	USP_REGFMT	eUnPackFmt = psTexCtrWrds[uTexCtrWrdIdx].aeUnPackFmts[uPlnIdx];
	IMG_UINT32	uTexFormat0 = psTexCtrWrds[uTexCtrWrdIdx].auWords[0];
	IMG_UINT32	uTexFormat1 = psTexCtrWrds[uTexCtrWrdIdx].auWords[1];

	USP_TEX_FORMAT sTexFormat = 
		psContext->asSamplerDesc[uTexIdx].sTexFormat;

	/*
		Is it a non-identity swizzle
	*/
	if((sTexFormat.aeChanSwizzle[0] !=  USP_CHAN_SWIZZLE_CHAN_0) || 
	   (sTexFormat.aeChanSwizzle[1] !=  USP_CHAN_SWIZZLE_CHAN_1) || 
	   (sTexFormat.aeChanSwizzle[2] !=  USP_CHAN_SWIZZLE_CHAN_2) || 
	   (sTexFormat.aeChanSwizzle[3] !=  USP_CHAN_SWIZZLE_CHAN_3))
	{
		bSwizOnData = IMG_TRUE;
	}

	if(eUnPackFmt == USP_REGFMT_UNKNOWN)
	{
		bValid = IMG_FALSE;
	}
	else if(eUnPackFmt == USP_REGFMT_F32)
	{
		eRegFmt = USP_REGFMT_F32;

		if(psTexCtrWrds[uTexCtrWrdIdx].abExtFmt[0])
		{
			if((uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_A8) || 
			   (uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_L8) || 
			   (uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_L8A8))
			{
				uChanPerReg = 1;
				uRegsSetCount = 4;
			}
			else if((uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_L_F16_REP) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_L_F16_A_F16))
			{
				uChanPerReg = 1;
				uRegsSetCount = 4;
			}
			else if(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_A_F16)
			{
				uChanPerReg = 1;
				uRegsSetCount = 2;
				bSwizOnData = IMG_TRUE;
			}	
			else
			{
				bValid = IMG_FALSE;
			}
		}
		else
		{
			if((uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_U8) && 
				(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt == 
				USP_TEXTURE_FORMAT_I8))
			{
				uChanPerReg = 1;
				uRegsSetCount = 1;
				bSwizOnData = IMG_TRUE;
			}
			else if((uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616) && 
				(uTexFormat1 == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616) && 
				(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt == 
				USP_TEXTURE_FORMAT_RGBA_F16))
			{
				uChanPerReg = 1;
				uRegsSetCount = 4;
				bMultiPlane = IMG_TRUE;
			}
			else if((uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616) && 
				(uTexFormat1 == EURASIA_PDS_DOUTT1_TEXFORMAT_F16) && 
				(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt == 
				USP_TEXTURE_FORMAT_RGB_F16))
			{
				uChanPerReg = 1;
				uRegsSetCount = 3;
				bMultiPlane = IMG_TRUE;
			}
			else if((uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_F16) && 
				(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt == 
				USP_TEXTURE_FORMAT_IF16))
			{
				uChanPerReg = 1;
				uRegsSetCount = 1;
				bSwizOnData = IMG_TRUE;
			}
			else if((uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8) ||
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII) ||
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP) ||
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP))
			{
				uChanPerReg = 1;
				uRegsSetCount = 4;
				bSwizOnData = IMG_TRUE;
			}
			else
			{
				bValid = IMG_FALSE;
			}
		}
	}
	else if(eUnPackFmt == USP_REGFMT_F16)
	{
		if(psTexCtrWrds[uTexCtrWrdIdx].abExtFmt[0])
		{
			eRegFmt = USP_REGFMT_F16;

			if((uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_A8) || 
			   (uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_L8) || 
			   (uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_L8A8))
			{	
				uChanPerReg = 2;
				uRegsSetCount = 2;
			}
			else
			{
				bValid = IMG_FALSE;
			}
		}
		else
		{
			if((uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_U8) && 
				(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt == 
				USP_TEXTURE_FORMAT_I8))
			{
				uChanPerReg = 1;
				uRegsSetCount = 1;
				bSwizOnData = IMG_TRUE;
			}
			else if((uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8) ||
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII) ||
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP) ||
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP) || 
				(uTexFormat0 == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP))
			{
				uChanPerReg = 2;
				uRegsSetCount = 2;
			}
			else
			{
				bValid = IMG_FALSE;
			}
		}
	}

	if(bValid)
	{
		*puChanPerReg = uChanPerReg;
		*puRegsSetCount = uRegsSetCount;
		*peRegFmt = eRegFmt;
		*pbMultiPlane = bMultiPlane;
		*pbSwizOnData = bSwizOnData;
	}

	return bValid;
}

/******************************************************************************
 Name:		CanUseDestDirectForUnpackedTexture

 Purpose:	Try to directly use sample destination register for unpacking on 
			545.

 Inputs:	psSample	- The dependent sample to generate code for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL CanUseDestDirectForUnpackedTexture(PUSP_SAMPLE	psSample, 
												   PUSP_CONTEXT	psContext, 
												   IMG_PUINT32	puChansSampledDirect)
{
	IMG_UINT32 uRegsSetCount;
	IMG_UINT32 uChanPerReg;
	USP_REGFMT eRegFmt;
	IMG_UINT32 uChansSampledDirect = 0;
	IMG_BOOL bMultiPlane = IMG_FALSE;

	if((!(psSample->bNonDependent)) && (psSample->bSamplerUnPacked))
	{
		IMG_UINT32 uChansToIgnore = 0;
		IMG_BOOL bSwizOnData;

		if(!GetTextureUnpackingFormat(psContext, 
			psSample->psShader->psTexCtrWrds, psSample->uTexCtrWrdIdx, 
			0, &uRegsSetCount, &uChanPerReg, &eRegFmt, 
			&bMultiPlane, &bSwizOnData))
		{
			return IMG_FALSE;
		}

		if(bMultiPlane || bSwizOnData)
		{
			/*
				Do not use direct destination writing for 
				multi plane or swizzled texture formats.
			*/
			*puChansSampledDirect = 0;
			return IMG_TRUE;
		}

		if((eRegFmt == USP_REGFMT_F32) || 
		   (eRegFmt == USP_REGFMT_F16))
		{
			
			if(AreDestRegsConsecutive(psSample, uRegsSetCount, uChanPerReg, eRegFmt))
			{
				if(UpdateSampleDestinationReg(psContext, 0, psSample, psSample->psSampleUnpack->asDest[0]))
				{
					IMG_UINT32 uChanIdx;

					for(uChanIdx=0; 
						uChanIdx<((uRegsSetCount*uChanPerReg)-uChansToIgnore); 
						uChanIdx++)
					{
						uChansSampledDirect |= 1<<uChanIdx;
					}

					*puChansSampledDirect = uChansSampledDirect;

					//psSample->uNumTempRegsUsed -= uRegsSetCount;
					*(psSample->psSampleUnpack->puBaseSampleDestRegCount) -= uRegsSetCount;

					return IMG_TRUE;
				}
			}
		}

		/*
			we cannot use destination register directly for such 
			sample instructions
		*/
		*puChansSampledDirect = 0;
		return IMG_TRUE;
	}

	return IMG_FALSE;
}
#endif /* defined(SGX545) */

#if defined(SGX_FEATURE_USE_VEC34)
/******************************************************************************
 Name:		IsSamplingMultiPlane

 Purpose:	Checks is the given set of texture control words sampeling 
            multiplane texture

 Inputs:	psTexCtrWrds	- The set of texture control words to check in

 Outputs:	none

 Returns:	IMG_TRUE/IMG_FALSE
*****************************************************************************/
static IMG_BOOL IsSamplingMultiPlane(PUSP_SHDR_TEX_CTR_WORDS	psTexCtrWrds)
{
	IMG_UINT32				uPlnIdx;
	IMG_UINT32				uTexPlaneCount = 0;
	IMG_BOOL				bMultiPlane = IMG_FALSE;

	for(uPlnIdx=0; uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; uPlnIdx++)
	{
		if(psTexCtrWrds->auWords[uPlnIdx] != (IMG_UINT32)-1)
		{
			uTexPlaneCount++;
		}
	}

	if(uTexPlaneCount>1)
	{
		bMultiPlane = IMG_TRUE;
	}

	return bMultiPlane;
}

/******************************************************************************
 Name:		IsWriteMaskUsingConsecutiveSourceSelect

 Purpose:	Checks if the destination mask is reading from consecutive 
            channels/registers

 Inputs:	uWriteMask		- The destination write mask
			puSrcChanSelect	- The source channel selection


 Outputs:	none

 Returns:	IMG_TRUE/IMG_FALSE
*****************************************************************************/
static IMG_BOOL IsWriteMaskUsingConsecutiveSourceSelect(IMG_UINT32	uWriteMask, 
														IMG_PUINT32	puSrcChanSelect)
{
	IMG_UINT32 uChanIdx;

	for(uChanIdx=0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		if((1<<uChanIdx) & uWriteMask)
		{
			if(puSrcChanSelect[uChanIdx] != uChanIdx)
			{
				return IMG_FALSE;
			}
		}
	}

	return IMG_TRUE;
}

/******************************************************************************
 Name:		CanUseVectorUnpack

 Purpose:	Unpacks using vector pack instruction on SGX543

 Inputs:	psSample	- The sample to generate code for
			psContext	- The current USP execution context
			eDestFmt	- Destination format to unpack to.

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL CanUseVectorUnpack(PUSP_SAMPLE	psSample,
								   PUSP_CONTEXT	psContext, 
								   USP_REGFMT	eDestFmt)
{
	USP_REG				sSampleSrc = psSample->asChunkReg[0];
	USP_REG				sSampleSrc2 = psSample->asChunkReg[0];
	USP_REG				sSampleDest = psSample->psSampleUnpack->asDest[0];
	IMG_UINT32			uWriteMask = 0;
	IMG_UINT32			uOneWriteMask = 0;
	IMG_UINT32			uZeroWriteMask = 0;
	IMG_UINT32			auSrcChanSelect[USP_MAX_SAMPLE_CHANS];
	IMG_UINT32			uChanIdx;
	IMG_UINT32			uHWInstCount = psSample->psSampleUnpack->uUnpackInstCount;
	PHW_INST			psHWInst;
	USP_PCKUNPCK_FMT	ePCKUNPCKDestFmt = USP_PCKUNPCK_FMT_INVALID;
	USP_PCKUNPCK_FMT	ePCKUNPCKSrcFmt = USP_PCKUNPCK_FMT_INVALID;
	PUSP_OPCODE			peOpcode;
	IMG_BOOL			bSkipInv;
	IMG_UINT32			uInstDescFlags;
	IMG_PUINT32			puInstDescFlags;
	IMG_BOOL			bScale = IMG_TRUE;
	IMG_BOOL			bScaleHWConstant = IMG_TRUE;
	IMG_BOOL			abSmpSrcGivesFmt[USP_MAX_SAMPLE_CHANS];
	IMG_UINT32			uChanIdxUsedForSmpSrcFmt;

	static const USP_REG sHWConstZero =
	{
		USP_REGTYPE_SPECIAL,
		SGXVEC_USE_SPECIAL_CONSTANT_ZERO_ZERO,
		USP_REGFMT_F32,
		0,
		USP_DYNIDX_NONE
	};

	static const USP_REG sHWConstOne =
	{
		USP_REGTYPE_SPECIAL,
		SGXVEC_USE_SPECIAL_CONSTANT_ONE_ONE,
		USP_REGFMT_F32,
		0,
		USP_DYNIDX_NONE
	};

	/* 
		Count of bits that are set in a nibble
	*/
	static const IMG_UINT32	auSetBitsCount[] = {0 /* 0x0 */, 
												1 /* 0x1 */, 
												1 /* 0x2 */, 
												2 /* 0x3 */, 
												1 /* 0x4 */, 
												2 /* 0x5 */, 
												2 /* 0x6 */, 
												3 /* 0x7 */, 
												1 /* 0x8 */, 
												2 /* 0x9 */,
												2 /* 0xA */, 
												3 /* 0xB */, 
												2 /* 0xC */, 
												3 /* 0xD */, 
												3 /* 0xE */, 
												4 /* 0xF */};

	sSampleSrc.eFmt = USP_REGFMT_UNKNOWN;

	/* 
		For C10 destinations use old unpacking code 
	*/
	if(eDestFmt == USP_REGFMT_C10)
	{
		return IMG_FALSE;
	}

	/* 
		Form instruction description flag for unpacking code 
	*/
	{
		uInstDescFlags = psSample->psSampleUnpack->uInstDescFlags;
#if 0
		& 
			~(USP_INSTDESC_FLAG_SRC0_USES_RESULT | 
			  USP_INSTDESC_FLAG_SRC1_USES_RESULT | 
			  USP_INSTDESC_FLAG_SRC2_USES_RESULT);

		if((!psSample->bNonDependent) || 
		   (psSample->bNonDependent && (psSample->sTexChanInfo.uTexChunkMask) && 
		   ((psSample->sTexChanInfo.uTexChunkMask) != (psSample->sTexChanInfo.uTexNonDepChunkMask))))
		{
			uInstDescFlags &= ~USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
		}
#endif
		bSkipInv = (IMG_BOOL)(!(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV));
	}

	/*
		No channel id is used to get data format for sample source register.
		It is used if sample source register format cannot be represented by 
		sample channel register.eFmt and channel data format has to be used 
		instead e.g. in case of U16, S16, S8 etc.
	*/
	uChanIdxUsedForSmpSrcFmt = (IMG_UINT32)-1;

	for (uChanIdx = 0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		/* 
			Initialize channel source selection to no channel
		*/
		auSrcChanSelect[uChanIdx] = (IMG_UINT32)-1;

		/*
			Assume that the format is always given by sample channel register
		*/
		abSmpSrcGivesFmt[uChanIdx] = IMG_TRUE;

		/*
			Try to get the sample source register format. Sample  source 
			register is generated from sample chunk register that does 
			not have a register format.
		*/
		if((psSample->asChanReg[uChanIdx].eFmt != USP_REGFMT_UNKNOWN) && 
		   (psSample->asChanReg[uChanIdx].eType != USP_REGTYPE_SPECIAL))
		{
			sSampleSrc.eFmt = 
				psSample->asChanReg[uChanIdx].eFmt;
		}
		else
		{
			/*
				Some formats cannot be represented by source register format
			*/
			if(psSample->asChanReg[uChanIdx].eType != USP_REGTYPE_SPECIAL)
			{
				if(uChanIdxUsedForSmpSrcFmt == (IMG_UINT32)-1)
				{
					uChanIdxUsedForSmpSrcFmt = uChanIdx;
				}

				/*
					Set that where to get format for this channel
				*/
				abSmpSrcGivesFmt[uChanIdx] = IMG_FALSE;
			}
		}

		/* 
			Try to form destination register for partial writes 
		*/
		if(sSampleDest.eFmt == USP_REGFMT_UNKNOWN)
		{
			if(psSample->psSampleUnpack->asDest[uChanIdx].eFmt != USP_REGFMT_UNKNOWN)
			{
				sSampleDest = psSample->psSampleUnpack->asDest[uChanIdx];
				switch(sSampleDest.eFmt)
				{
					case USP_REGFMT_U8:
					{
						sSampleDest.uComp = 0;
						break;
					}

					case USP_REGFMT_F16:
					{
						/* 
							Such destination cannot be formed 
						*/
						if(((IMG_INT32)sSampleDest.uNum - (IMG_INT32)(uChanIdx/2)) < 0)
						{
							return IMG_FALSE;
						}

						sSampleDest.uNum -= (uChanIdx/2);
						sSampleDest.uComp = 0;
						break;
					}

					case USP_REGFMT_F32:
					{
						/* 
							Such destination cannot be formed 
						*/
						if(((IMG_INT32)sSampleDest.uNum - (IMG_INT32)uChanIdx) < 0)
						{
							return IMG_FALSE;
						}

						sSampleDest.uNum -= uChanIdx;
						sSampleDest.uComp = 0;
						break;
					}

					/* 
						For all other formats use old code
					*/
					default:
					{
						return IMG_FALSE;
						break;
					}
				}
			}
		}
	}

	/* 
		Sample is not required
	*/
	if((sSampleSrc.eFmt == USP_REGFMT_UNKNOWN) && 
	   (uChanIdxUsedForSmpSrcFmt == (IMG_UINT32)-1))
	{
		*(psSample->psSampleUnpack->puBaseSampleDestRegCount) -= (*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount));
		if(psSample->psSampleUnpack->puBaseSampleTempDestRegCount != psSample->psSampleUnpack->puBaseSampleDestRegCount)
		{
			//psSample->uBaseSampleDestRegCount = 0;
			(*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount)) = 0;
		}
		//psSample->uUnAlignedTempCount = 0;		
		psSample->uSampleInstCount_ = 0;
		psSample->uNumSMPInsts = 0;
	}
	else if(sSampleSrc.eFmt == USP_REGFMT_F32)
	{
		/* 
		F32 unpacking requires two source registers.
		The current code assumes 
		sSampleSrc2.uNum == sSampleSrc.uNum+2
		*/

		sSampleSrc2 = sSampleSrc;

		if(sSampleSrc2.eType != USP_REGTYPE_SPECIAL)
		{
			sSampleSrc2.uNum = sSampleSrc2.uNum + 2;
		}

	}

	/* 
		Check destination register alignment
	*/
	if(sSampleDest.eFmt != USP_REGFMT_U8)
	{
		if(sSampleDest.uNum%2)
		{
			return IMG_FALSE;
		}
	}

	/*
		Check if it is integer to integer unapcking than remapping is 
		required. Remapping is handled only in old code now.
	*/
	if((eDestFmt == USP_REGFMT_U8) && 
	   (uChanIdxUsedForSmpSrcFmt != (IMG_UINT32)-1))
	{
		switch(psSample->aeDataFmtForChan[uChanIdxUsedForSmpSrcFmt])
		{
			/*
				These are all integer formats so we have to scale them first 
				to pack to U8
			*/
			case USP_CHAN_FORMAT_U16:
			case USP_CHAN_FORMAT_S16:
			{
				return IMG_FALSE;
				break;
			}

			case USP_CHAN_FORMAT_S8:
			{
				break;
			}

			default:
			{
				return IMG_FALSE;
				break;
			}
		}
	}

	switch(eDestFmt)
	{
		
		case USP_REGFMT_U8:
		{
			ePCKUNPCKDestFmt = USP_PCKUNPCK_FMT_U8;
			break;
		}

		case USP_REGFMT_F16:
		{
			ePCKUNPCKDestFmt = USP_PCKUNPCK_FMT_F16;
			break;
		}

		case USP_REGFMT_F32:
		{
			ePCKUNPCKDestFmt = USP_PCKUNPCK_FMT_F32;
			break;
		}

		default:
		{
			return IMG_FALSE;
			break;
		}
	}

	if(sSampleSrc.eFmt != USP_REGFMT_U8)
	{
		if(sSampleSrc.eFmt == USP_REGFMT_UNKNOWN)
		{
			if(uChanIdxUsedForSmpSrcFmt != (IMG_UINT32)-1)
			{
				/*
					Some channel formats are only given by channel data format
				*/
				switch(psSample->aeDataFmtForChan[uChanIdxUsedForSmpSrcFmt])
				{
					case USP_CHAN_FORMAT_U16:
					case USP_CHAN_FORMAT_S16:
					{
						/*
							64-bit alignment required
						*/
						if(sSampleSrc.uNum%2)
						{
							return IMG_FALSE;
						}
						break;
					}

					case USP_CHAN_FORMAT_S8:
					{
						break;
					}

					default:
					{
						return IMG_FALSE;
						break;
					}
				}
			}
		}
		else
		{
			if(sSampleSrc.uNum%2)
			{
				return IMG_FALSE;
			}
		}
	}

	switch(sSampleSrc.eFmt)
	{
		case USP_REGFMT_U8:
		{
			ePCKUNPCKSrcFmt = USP_PCKUNPCK_FMT_U8;
			break;
		}

		case USP_REGFMT_F16:
		{
			ePCKUNPCKSrcFmt = USP_PCKUNPCK_FMT_F16;
			break;
		}

		case USP_REGFMT_F32:
		{
			ePCKUNPCKSrcFmt = USP_PCKUNPCK_FMT_F32;
			break;
		}

		case USP_REGFMT_UNKNOWN:
		{
			if(uChanIdxUsedForSmpSrcFmt != (IMG_UINT32)-1)
			{
				/*
					Some channel formats are only given by channel data format
				*/
				switch(psSample->aeDataFmtForChan[uChanIdxUsedForSmpSrcFmt])
				{
					case USP_CHAN_FORMAT_U16:
					{
						ePCKUNPCKSrcFmt = USP_PCKUNPCK_FMT_U16;
						break;
					}

					case USP_CHAN_FORMAT_S16:
					{
						ePCKUNPCKSrcFmt = USP_PCKUNPCK_FMT_S16;
						break;
					}

					case USP_CHAN_FORMAT_S8:
					{
						ePCKUNPCKSrcFmt = USP_PCKUNPCK_FMT_S8;
						break;
					}

					default:
					{
						return IMG_FALSE;
						break;
					}
				}
			}
			else
			{
				ePCKUNPCKSrcFmt = USP_PCKUNPCK_FMT_F32;
			}
			break;
		}

		default:
		{
			return IMG_FALSE;
			break;
		}
	}

	/*
		Set scaling for texture channel contents
	*/
	switch(ePCKUNPCKDestFmt)
	{
		case USP_PCKUNPCK_FMT_F32:
		{
			switch(ePCKUNPCKSrcFmt)
			{
				case USP_PCKUNPCK_FMT_U8:
				case USP_PCKUNPCK_FMT_S8:
				case USP_PCKUNPCK_FMT_U16:
				case USP_PCKUNPCK_FMT_S16:
				case USP_PCKUNPCK_FMT_F16:
				{
					bScale = IMG_TRUE;
					break;
				}
				default:
				{
					bScale = IMG_FALSE;
					break;
				}
			}
			break;
		}

		case USP_PCKUNPCK_FMT_F16:
		{
			switch(ePCKUNPCKSrcFmt)
			{
				case USP_PCKUNPCK_FMT_U8:
				case USP_PCKUNPCK_FMT_S8:
				case USP_PCKUNPCK_FMT_U16:
				case USP_PCKUNPCK_FMT_S16:
				case USP_PCKUNPCK_FMT_F32:
				{
					bScale = IMG_TRUE;
					break;
				}
				default:
				{
					bScale = IMG_FALSE;
					break;
				}
			}
			break;
		}

		case USP_PCKUNPCK_FMT_U8:
		{
			switch(ePCKUNPCKSrcFmt)
			{
				case USP_PCKUNPCK_FMT_F32:
				case USP_PCKUNPCK_FMT_S8:
				case USP_PCKUNPCK_FMT_U16:
				case USP_PCKUNPCK_FMT_S16:
				case USP_PCKUNPCK_FMT_F16:
				{
					bScale = IMG_TRUE;
					break;
				}
				default:
				{
					bScale = IMG_FALSE;
					break;
				}
			}
			break;
		}

		default:
		{
			return IMG_FALSE;
			break;
		}
	}

	/*
		Set scaling for hardware constants.
	*/
	switch(ePCKUNPCKDestFmt)
	{
		case USP_PCKUNPCK_FMT_F32:
		{
			bScaleHWConstant = IMG_FALSE;
			break;
		}

		case USP_PCKUNPCK_FMT_F16:
		{
			bScaleHWConstant = IMG_TRUE;
			break;
		}

		case USP_PCKUNPCK_FMT_U8:
		{
			bScaleHWConstant = IMG_TRUE;
			break;
		}

		default:
		{
			return IMG_FALSE;
			break;
		}
	}


	for(uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		PUSP_REG	psDest;
		PUSP_REG	psSrc;

		if(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
		{
			continue;
		}
		else
		{
			/*
				Generate the destination write mask
			*/
			psDest = &psSample->psSampleUnpack->asDest[uChanIdx];
			psSrc = &psSample->asChanReg[uChanIdx];
			if((psDest->eType == sSampleDest.eType) && (psDest->eFmt != USP_REGFMT_UNKNOWN))
			{
				if(psDest->eFmt == USP_REGFMT_F32)
				{
					if((psDest->uNum - sSampleDest.uNum) < 4)
					{
						if(psSrc->eType == USP_REGTYPE_SPECIAL)
						{
							if(psSrc->uNum == HW_CONST_ZERO)
							{
								uZeroWriteMask |= (1 << (psDest->uNum - sSampleDest.uNum));
							}
							else
							{
								uOneWriteMask |= (1 << (psDest->uNum - sSampleDest.uNum));
							}
						}
						else
						{
							uWriteMask |= (1 << (psDest->uNum - sSampleDest.uNum));
						}
					}
					else
					{
						return IMG_FALSE;
					}
				}
				else if(psDest->eFmt == USP_REGFMT_F16)
				{
					if((psDest->uNum - sSampleDest.uNum) < 2)
					{
						if(psSrc->eType == USP_REGTYPE_SPECIAL)
						{
							if(psSrc->uNum == HW_CONST_ZERO)
							{
								uZeroWriteMask |= (1 << (((psDest->uNum - sSampleDest.uNum) * 2) + (psDest->uComp/2)));
							}
							else
							{
								uOneWriteMask |= (1 << (((psDest->uNum - sSampleDest.uNum) * 2) + (psDest->uComp/2)));
							}
						}
						else
						{
							uWriteMask |= (1 << (((psDest->uNum - sSampleDest.uNum) * 2) + (psDest->uComp/2)));
						}
					}
					else
					{
						return IMG_FALSE;
					}
				}
				else if(psDest->eFmt == USP_REGFMT_U8)
				{
					if(psDest->uNum == sSampleDest.uNum)
					{
						if(psSrc->eType == USP_REGTYPE_SPECIAL)
						{
							if(psSrc->uNum == HW_CONST_ZERO)
							{
								uZeroWriteMask |= (1 << psDest->uComp);
							}
							else
							{
								uOneWriteMask |= (1 << psDest->uComp);
							}
						}
						else
						{
							uWriteMask |= (1 << psDest->uComp);
						}
					}
					else
					{
						return IMG_FALSE;
					}
				}
			}

			/*
				Select source channels to use
			*/
			if((psSrc->eType == sSampleSrc.eType) && 
			   (psSrc->eType != USP_REGTYPE_SPECIAL) && 
			   ((psSrc->eFmt != USP_REGFMT_UNKNOWN) || (!abSmpSrcGivesFmt[uChanIdx])))
			{
				if(psSrc->eFmt == USP_REGFMT_F32)
				{
					if((psSrc->uNum - sSampleSrc.uNum) < 4)
					{
						auSrcChanSelect[uChanIdx] = 
							psSrc->uNum - sSampleSrc.uNum;
					}
					else
					{
						return IMG_FALSE;
					}
				}
				else if(psSrc->eFmt == USP_REGFMT_F16)
				{
					if((psSrc->uNum - sSampleSrc.uNum) < 2)
					{
						auSrcChanSelect[uChanIdx] = 
							((psSrc->uNum - sSampleSrc.uNum)*2) + (psSrc->uComp/2);
					}
					else
					{
						return IMG_FALSE;
					}
				}
				else if(psSrc->eFmt == USP_REGFMT_U8)
				{
					if(psSrc->uNum == sSampleSrc.uNum)
					{
						auSrcChanSelect[uChanIdx] = psSrc->uComp;
					}
				}
				/* 
					Only formats that cannot be given by source channel 
					register format will reach here
				*/
				else if(!abSmpSrcGivesFmt[uChanIdx])
				{
					switch(psSample->aeDataFmtForChan[uChanIdx])
					{
						case USP_CHAN_FORMAT_U16:
						case USP_CHAN_FORMAT_S16:
						{
							if((psSrc->uNum - sSampleSrc.uNum) < 2)
							{
								auSrcChanSelect[uChanIdx] = 
									((psSrc->uNum - sSampleSrc.uNum)*2) + (psSrc->uComp/2);
							}
							else
							{
								return IMG_FALSE;
							}
							break;
						}

						case USP_CHAN_FORMAT_S8:
						{
							if(psSrc->uNum == sSampleSrc.uNum)
							{
								auSrcChanSelect[uChanIdx] = psSrc->uComp;
							}
							break;
						}

						default:
						{
							return IMG_FALSE;
							break;
						}
					}
				}
				else
				{
					return IMG_FALSE;
				}
			}
		}
	}

	/*
		Try to patch the sample destination with destination register directly
	*/
	if((!psSample->bNonDependent) && 
	   (!IsSamplingMultiPlane(&psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx])) && 
	   ((psSample->psSampleUnpack->uLive & ~uWriteMask) == 0 && uZeroWriteMask == 0x0 && uOneWriteMask == 0x0) && 
	   IsWriteMaskUsingConsecutiveSourceSelect(uWriteMask, auSrcChanSelect) &&
	   (sSampleDest.eType == USP_REGTYPE_TEMP || sSampleDest.eType == USP_REGTYPE_PA) && 
	   (sSampleDest.eFmt == sSampleSrc.eFmt) &&
	   (psSample->psSampleUnpack->bCanSampleDirectToDest))
	{
		IMG_UINT32 uChanIdx;

		for(uChanIdx=0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
		{
			if ((1<<uChanIdx) & uWriteMask)
			{
				break;
			}
		}

		if (uChanIdx == USP_MAX_SAMPLE_CHANS)
		{
			USP_DBGPRINT(("CanUseVectorUnpack: Error finding a used channel.\n"));
			return IMG_FALSE;
		}

		/* Try to use destination directly */
		if(sSampleDest.eFmt == USP_REGFMT_U8)
		{
			if(UpdateSampleDestinationReg(psContext, uChanIdx, psSample, sSampleDest))
			{
				/* 
					Sample Temporary dest. registers are not required as we
					are sampling directly to destination 
				*/
				//psSample->uNumTempRegsUsed -= psSample->uSampleTempCount;
				*(psSample->psSampleUnpack->puBaseSampleDestRegCount) -= (*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount));
				if(psSample->psSampleUnpack->puBaseSampleTempDestRegCount != psSample->psSampleUnpack->puBaseSampleDestRegCount)
				{
					//psSample->uBaseSampleDestRegCount = 0;
					(*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount)) = 0;
				}
				//psSample->uUnAlignedTempCount = 0;
				//psSample->uBaseSampleDestRegCount = 0;
				return IMG_TRUE;
			}
		}
		else if((sSampleDest.eFmt == USP_REGFMT_F32) || (sSampleDest.eFmt == USP_REGFMT_F16))
		{
			/* F16/F32 destination should be aligned to 64-bits on SGX543 */
			if((sSampleDest.uNum%2) == 0)
			{
				if(UpdateSampleDestinationReg(psContext, uChanIdx, psSample, sSampleDest))
				{
					/* 
						Sample Temporary dest. registers are not required as we 
						are sampling directly to destination 
					*/
					//psSample->uNumTempRegsUsed -= psSample->uSampleTempCount;
					*(psSample->psSampleUnpack->puBaseSampleDestRegCount) -= (*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount));
					if(psSample->psSampleUnpack->puBaseSampleTempDestRegCount != psSample->psSampleUnpack->puBaseSampleDestRegCount)
					{
						//psSample->uBaseSampleDestRegCount = 0;
						(*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount)) = 0;
					}
					//psSample->uUnAlignedTempCount = 0;
					//psSample->uBaseSampleDestRegCount = 0;
					return IMG_TRUE;
				}
			}

		}
	}

	/* Is unpacking required */
	if((uWriteMask) && 
		!(((sSampleDest.uNum == sSampleSrc.uNum) && 
		   (sSampleDest.eType == sSampleSrc.eType) && 
		   (sSampleDest.eFmt == sSampleSrc.eFmt) && 
		   (sSampleDest.uComp == sSampleSrc.uComp)) && 
		  IsWriteMaskUsingConsecutiveSourceSelect(uWriteMask, auSrcChanSelect))
	   )
	{
		if(ePCKUNPCKDestFmt ==  USP_PCKUNPCK_FMT_U8 && ePCKUNPCKSrcFmt == USP_PCKUNPCK_FMT_U8 && auSetBitsCount[uWriteMask] > 2)
		{
			if(uWriteMask == 0xF)
			{
				/* Its a straight writing */
				USP_REG sImdVal;

				psHWInst = &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
				peOpcode = &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
				puInstDescFlags = &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

				sImdVal.eType	= USP_REGTYPE_IMM;
				sImdVal.uNum	= 0xFFFFFFFF;
				sImdVal.eFmt	= sSampleSrc.eFmt;
				sImdVal.uComp	= sSampleSrc.uComp;
				sImdVal.eDynIdx = USP_DYNIDX_NONE;
				if	(!HWInstEncodeANDInst(psHWInst, 
										  1,
										  IMG_FALSE, /* bUsePred */
										  IMG_FALSE, /* bNegatePred */
										  0, /* uPredNum */
										  bSkipInv,
										  &sSampleDest, 
										  &sSampleSrc, 
										  &sImdVal))
				{
					USP_DBGPRINT(("CanUseVectorUnpack: Error encoding data movement (AND) HW-inst\n"));
					return IMG_FALSE;
				}
				*peOpcode = USP_OPCODE_AND;
				*puInstDescFlags = uInstDescFlags;

				uHWInstCount++;
				psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;
			}
			else
			{
				/* Two unpack instructions are required on SGX543. Unpacking 
				   more than two channels from U8 to U8 simultaneously is not 
				   valid on SGX543.
				*/

				/* Unpack to destination channels that require texture data */
				psHWInst = &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
				peOpcode = &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
				puInstDescFlags = &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

				if(!HWInstEncodeVectorPCKUNPCKInst(psHWInst, bSkipInv, 
					ePCKUNPCKDestFmt, ePCKUNPCKSrcFmt, bScale, uWriteMask & 0x3 , 
					&sSampleDest, IMG_TRUE, auSrcChanSelect, 0, 0, &sSampleSrc, &sSampleSrc2))
				{
					USP_DBGPRINT(("CanUseVectorUnpack: Error encoding pack instruction\n"));
					return IMG_FALSE;
				}
				*peOpcode = USP_OPCODE_PCKUNPCK;
				*puInstDescFlags = uInstDescFlags;

				uHWInstCount++;
				psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;

				/* Unpack to destination channels that require texture data */
				psHWInst = &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
				peOpcode = &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
				puInstDescFlags = &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

				if(!HWInstEncodeVectorPCKUNPCKInst(psHWInst, bSkipInv, 
					ePCKUNPCKDestFmt, ePCKUNPCKSrcFmt, bScale, uWriteMask & 0xC, 
					&sSampleDest, IMG_TRUE, auSrcChanSelect, 0, 0, &sSampleSrc, &sSampleSrc2))
				{
					USP_DBGPRINT(("CanUseVectorUnpack: Error encoding pack instruction\n"));
					return IMG_FALSE;
				}
				*peOpcode = USP_OPCODE_PCKUNPCK;
				*puInstDescFlags = uInstDescFlags;

				uHWInstCount++;
				psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;

			}
		}
		else
		{
			/* Unpack to destination channels that require texture data */
			psHWInst = &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
			peOpcode = &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
			puInstDescFlags = &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

			if(!HWInstEncodeVectorPCKUNPCKInst(psHWInst, bSkipInv, 
				ePCKUNPCKDestFmt, ePCKUNPCKSrcFmt, bScale, uWriteMask, 
				&sSampleDest, IMG_TRUE, auSrcChanSelect, 0, 0, &sSampleSrc, &sSampleSrc2))
			{
				USP_DBGPRINT(("CanUseVectorUnpack: Error encoding pack instruction\n"));
				return IMG_FALSE;
			}
			*peOpcode = USP_OPCODE_PCKUNPCK;
			*puInstDescFlags = uInstDescFlags;

			uHWInstCount++;
			psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;
		}
	}

	/* Now use channel select for hardware constants */
	auSrcChanSelect[0] = 0;
	auSrcChanSelect[1] = 0;
	auSrcChanSelect[2] = 0;
	auSrcChanSelect[3] = 0;

	/* Unpack to destination channels that require zero */
	if(uZeroWriteMask)
	{
		psHWInst = &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
		peOpcode = &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
		puInstDescFlags = &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

		if(!HWInstEncodeVectorPCKUNPCKInst(psHWInst, bSkipInv, 
			ePCKUNPCKDestFmt, USP_PCKUNPCK_FMT_F32, bScaleHWConstant, 
			uZeroWriteMask, &sSampleDest, IMG_TRUE, auSrcChanSelect, 0, 0, 
			(PUSP_REG)&sHWConstZero, (PUSP_REG)&sHWConstZero))
		{
			USP_DBGPRINT(("CanUseVectorUnpack: Error encoding pack instruction\n"));
			return IMG_FALSE;
		}
		
		*peOpcode = USP_OPCODE_PCKUNPCK;
		*puInstDescFlags = uInstDescFlags;

		uHWInstCount++;
		psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;
	}

	/* Unpack to destination channels that require one */
	if(uOneWriteMask)
	{
		psHWInst = &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
		peOpcode = &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
		puInstDescFlags = &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

		if(!HWInstEncodeVectorPCKUNPCKInst(psHWInst, bSkipInv, 
			ePCKUNPCKDestFmt, USP_PCKUNPCK_FMT_F32, bScaleHWConstant, 
			uOneWriteMask, &sSampleDest, IMG_TRUE, auSrcChanSelect, 0, 0, 
			(PUSP_REG)&sHWConstOne, (PUSP_REG)&sHWConstOne))
		{
			USP_DBGPRINT(("CanUseVectorUnpack: Error encoding pack instruction\n"));
			return IMG_FALSE;
		}
		
		*peOpcode = USP_OPCODE_PCKUNPCK;
		*puInstDescFlags = uInstDescFlags;

		uHWInstCount++;
		psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;
	}

	return IMG_TRUE;
}
#endif /* defined(SGX_FEATURE_USE_VEC34) */

/******************************************************************************
 Name:		IsUsingAndForDF32Texture

 Purpose:	Returns IMG_TRUE if ABS operation is applied for Depth F32 
            textures

 Inputs:	psContext	- The current USP execution context
			psSample	- The sample to get flag for
			uChanIdx	- The channel to get flag for 

 Outputs:	none

 Returns:	IMG_TRUE/IMG_FALSE
*****************************************************************************/
static IMG_BOOL IsUsingAndForDF32Texture(PUSP_CONTEXT	psContext,
										 PUSP_SAMPLE	psSample,
										 IMG_UINT32		uChanIdx)
{
	IMG_UINT32 bAndForDF32Tex = IMG_FALSE;

	if((psContext->asSamplerDesc
		[psSample->uTextureIdx].sTexFormat.eTexFmt) == 
		USP_TEXTURE_FORMAT_DF32)
	{
		PHW_INST	psAndInst;
		IMG_UINT32	uAndInstIdx;
		USP_OPCODE	eAndInstOpcode;
		/*
			Lookup details of the sample instruction used to read
			this chunk
		*/
		psAndInst		= psSample->apsSMPInsts[uChanIdx];
		uAndInstIdx		= psAndInst - psSample->psSampleUnpack->asSampleInsts;
		eAndInstOpcode	= psSample->psSampleUnpack->aeOpcodeForSampleInsts[uAndInstIdx];
		
		if(eAndInstOpcode == USP_OPCODE_AND)
		{
			bAndForDF32Tex = IMG_TRUE;
		}
	}

	return bAndForDF32Tex;
}

#if defined(SGX_FEATURE_USE_VEC34)
/******************************************************************************
 Name:		CanUnpackTwoChannels

 Purpose:	Checks if two channels can be packed simultaneously.

 Inputs:	eDstFmt			- The destination format
			psSrc			- The source register
			eDataFmtForChan	- Some input formats cannot be represented by 
							  psSrc->eFmt. For these formats use input channle 
							  data format.

 Outputs:	none

 Returns:	IMG_TRUE/IMG_FALSE
*****************************************************************************/
static IMG_BOOL CanUnpackTwoChannels(USP_REGFMT			eDstFmt,	
									 PUSP_REG			psSrc, 
									 USP_CHAN_FORMAT	eDataFmtForChan)
{
	switch(eDstFmt)
	{
		case USP_REGFMT_F32:
		case USP_REGFMT_F16:
		case USP_REGFMT_U8:
		{
			switch(psSrc->eFmt)
			{
				case USP_REGFMT_F32:
				case USP_REGFMT_F16:
				case USP_REGFMT_U8:
				{
					return IMG_TRUE;
					break;
				}

				/* 
					Some input formats cannot be represented by psSrc->eFmt
				*/
				case USP_REGFMT_UNKNOWN:
				{
					switch(eDataFmtForChan)
					{
						case USP_CHAN_FORMAT_U16:
						case USP_CHAN_FORMAT_S16:
						case USP_CHAN_FORMAT_S8:
						{
							return IMG_TRUE;
							break;
						}

						default:
						{
							return IMG_FALSE;
							break;
						}
					}
					break;
				}

				default:
				{
					return IMG_FALSE;
					break;
				}
			}
			break;
		}

		/*
			Right now multi channel unpacking is allowed only 
			for float to float unpacking
		*/
		default:
		{
			return IMG_FALSE;
			break;
		}
	}
}
#endif /* defined(SGX_FEATURE_USE_VEC34) */

/******************************************************************************
 Name:		UnpackTextureDataToF32

 Purpose:	Unpack sampled texture data to F32 results

 Inputs:	psSample	- The dependent sample to generate code for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL UnpackTextureDataToF32(PUSP_SAMPLE	psSample,
									   PUSP_CONTEXT	psContext)
{
	static const USP_PCKUNPCK_FMT aePCKUNPCKFmtForChanFmt[] =
	{
		USP_PCKUNPCK_FMT_U8,	/* USP_CHAN_FORMAT_U8 */
		USP_PCKUNPCK_FMT_S8,	/* USP_CHAN_FORMAT_S8 */
		USP_PCKUNPCK_FMT_U16,	/* USP_CHAN_FORMAT_U16 */
		USP_PCKUNPCK_FMT_S16,	/* USP_CHAN_FORMAT_S16 */
		USP_PCKUNPCK_FMT_F16,	/* USP_CHAN_FORMAT_F16 */
		USP_PCKUNPCK_FMT_F32,	/* USP_CHAN_FORMAT_F32 */
		USP_PCKUNPCK_FMT_INVALID/* USP_CHAN_FORMAT_U24 */
	};
	static const USP_REG sHWConstZero =
	{
		USP_REGTYPE_SPECIAL,
		HW_CONST_ZERO,
		USP_REGFMT_F32,
		0,
		USP_DYNIDX_NONE
	};

	IMG_BOOL	bSkipInv;
	IMG_UINT32	uChanIdx;
	IMG_UINT32	uHWInstCount;
	IMG_UINT32	uInstDescFlags;
	IMG_UINT32	uChansSampledDirect;
	IMG_BOOL	bFirstUnpackInst;
	IMG_BOOL	bAndForDF32Tex = IMG_FALSE;
	IMG_BOOL	uChansUpdatedMask = 0;
	IMG_BOOL	bChunkNotSampledToTempReg = IMG_FALSE;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	#if defined(SGX_FEATURE_USE_VEC34)
	if(CanUseVectorUnpack(psSample, psContext, USP_REGFMT_F32))
	{
		return IMG_TRUE;
	}
	#endif /* defined(SGX_FEATURE_USE_VEC34) */

	/*
		Look for sample instructions that can sample directly to the
		intended destination
	*/
	uChansSampledDirect = 0;
	if (psSample->psSampleUnpack->bCanSampleDirectToDest)
	{
		#if defined(SGX545)
		if(!CanUseDestDirectForUnpackedTexture(psSample, psContext, &uChansSampledDirect))
		#endif /* defined(SGX545) */
		{
			for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
			{
				IMG_BOOL bChanChunkNonDep = IMG_FALSE;

				/*
					Skip this channel if we didn't need to sample it
				*/
				if	(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
				{
					continue;	
				}

				/*
					Only channels that are in the same format as the destination
					(i.e F32 here) can be sampled directly
				*/
				if	(GetSampleChannelFormat(psSample, uChanIdx) == USP_CHAN_FORMAT_F32)
				{
					PUSP_REG	psChanReg;
					PUSP_REG	psDestReg;
					USP_REG		sFinalDestReg;

					/*
						Get the source and destination registers to use for this 
						component of the result
					*/
					psChanReg = &psSample->asChanReg[uChanIdx];
					psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

					/* 
						Get either this channel is sampled as Dependent or 
						Non Dependent. 
					*/
					if	(psSample->bNonDependent)
					{
						USP_CHAN_SWIZZLE	eTexChanSwizzle;			
						eTexChanSwizzle		= (psSample->sTexChanInfo).aeTexChanForComp[uChanIdx];
						if (eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
						{
							IMG_UINT32			uChunkIdx;	

							uChunkIdx			= (psSample->sTexChanInfo).auChunkIdxForChan[eTexChanSwizzle];
							bChanChunkNonDep	= (((psSample->sTexChanInfo).uTexNonDepChunkMask & (1 << uChunkIdx)) ? IMG_TRUE : IMG_FALSE);
							bChunkNotSampledToTempReg = psSample->abChunkNotSampledToTempReg[uChunkIdx];
						}
					}

					/*
						If the result of this sample forms part of the overall shader result,
						account for the required result-remapping.

						NB: This is not needed for non-dependent samples, since they will be
							patched correctly as part of result-remapping. For non-dependent
							samples that don't need any unpacking code, there would be no
							instructions to 'patch', so we muct account for the final
							destination here.
					*/
					sFinalDestReg = *psDestReg;

					if	(psSample->psSampleUnpack->uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT)
					{
						PUSP_SHADER		psShader;
						PUSP_PROGDESC	psProgDesc;
						USP_REGTYPE		eFinalResultRegType;
						IMG_UINT32		uFinalResultRegNum;
						IMG_UINT32		uOrgResultRegNum;

						/*
							Remap the destination to the final result register
						*/
						psShader	= psSample->psShader;
						psProgDesc	= psShader->psProgDesc;

						eFinalResultRegType	= psShader->eFinalResultRegType;
						uFinalResultRegNum	= psShader->uFinalResultRegNum;
						uOrgResultRegNum	= psProgDesc->uDefaultPSResultRegNum;

						sFinalDestReg.eType	= eFinalResultRegType;
						sFinalDestReg.uNum	= (psDestReg->uNum - uOrgResultRegNum) + 
											  uFinalResultRegNum;
					}

					/*
						Is ABS operation is used for Depth F32 textures
					*/
					bAndForDF32Tex = IsUsingAndForDF32Texture(psContext, psSample, uChanIdx);

					/*
						Determine whether the data for this channel can be (or has already
						been) sampled directly to the destination register.
					*/
					if	(bChanChunkNonDep && !bAndForDF32Tex && bChunkNotSampledToTempReg)
					{
						if	(
								(sFinalDestReg.eDynIdx == USP_DYNIDX_NONE) &&
								(sFinalDestReg.eType	== psChanReg->eType) &&
								(sFinalDestReg.uNum	== psChanReg->uNum)
							)
						{
							/*
								No unpacking code needed, since the pre-sampled
								data is in the right place already for this
								channel of the destination.
							*/
							uChansSampledDirect |= 1 << uChanIdx;
						}
					}
					else
					{
						PUSP_TEX_CHAN_INFO    psTexChanInfo;
						PUSP_TEX_CHAN_FORMAT  psTexFormat;
						USP_CHAN_SWIZZLE	  eTexChanSwizzle;

						/*
							Did we sample the texture for this channel of the
							destination?
						*/
						psTexFormat		= &psSample->sSamplerDesc.sTexChanFormat;
						psTexChanInfo	= &psSample->sTexChanInfo;
						eTexChanSwizzle = psTexChanInfo->aeTexChanForComp[uChanIdx];

						if	(
								(eTexChanSwizzle >= USP_CHAN_SWIZZLE_CHAN_0) &&
								(eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
							)
						{
							USP_CHAN_CONTENT	eTexChanContent;
							IMG_UINT32			uTexChunkIdx;
							IMG_UINT32			uNumSMPDestRegs;

							/*
								Get the number of destination registers for the SMP instruction which
								writes the data for this channel. We can only change the SMP to write
								direct if it writes only one register.
							*/
							uTexChunkIdx		= psSample->sTexChanInfo.auChunkIdxForChan[eTexChanSwizzle];
							uNumSMPDestRegs		= psSample->sTexChanInfo.auNumRegsForChunk[uTexChunkIdx];

							eTexChanContent = psTexFormat->aeChanContent[eTexChanSwizzle];
							if	((eTexChanContent == USP_CHAN_CONTENT_DATA) && 
								!(uChansUpdatedMask & (1 << eTexChanSwizzle)) &&
								 uNumSMPDestRegs == 1)
							{
								HW_INST		sTempSampleInst;
								PHW_INST	psSampleInst;
								IMG_UINT32	uSampleInstIdx;
								USP_OPCODE	eSampleInstOpcode;

								/*
									Lookup details of the sample instruction used to read
									this chunk
								*/
								psSampleInst		= psSample->apsSMPInsts[uChanIdx];
								uSampleInstIdx		= psSampleInst - psSample->psSampleUnpack->asSampleInsts;
								eSampleInstOpcode	= psSample->psSampleUnpack->aeOpcodeForSampleInsts[uSampleInstIdx];

								/*
									Is the destination register-type one that is supported
									by the SMP instruction?
								*/
								sTempSampleInst	= *psSampleInst;

								if	(HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
																eSampleInstOpcode,
																&sTempSampleInst,
																&sFinalDestReg))
								{
									IMG_BOOL	bSMPReadsDest;

									/*
										Check whether the destination register may be used
										by one of the sources for the sample instructions.

										NB:	All SMP instructions generated for a sample will be
											identical with respect to the sources they use (apart from
											the state source for SMP), so we only need to check for
											clashes with one inst here.
									*/
									if	(!SampleInstReadsReg(psSample,
															 psSampleInst,
															 &sFinalDestReg,
															 psContext,
															 &bSMPReadsDest))
									{
										USP_DBGPRINT(("UnpackTextureDataToF32: Error checking for SMP inst that references dest\n"));
										return IMG_FALSE;
									}

									/*
										Only sample directly to the destination register
										if it isn't used through any of the sources.
									*/
									if	(!bSMPReadsDest)
									{
										IMG_UINT32	uInstDescFlags;
										IMG_UINT32	uChunkIdx;
										IMG_UINT32	uChunkRegOff;

										/*
											Modify the sample instruction to write
											directly to the destination register.

											NB: We don't use the 'final' destination register
												here (i.e. after result-remapping) since the
												operands should be remapped just like any other
												instruction.
										*/
										HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
																   eSampleInstOpcode,
																   psSampleInst, 
																   psDestReg);

										/*
											Now that this sample instruction writes directly
											to the intended destination, rather than an
											intermediate temporary, it may reference a result
											or input register.
										*/
										uInstDescFlags = psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx];

										uInstDescFlags |= psSample->psSampleUnpack->uInstDescFlags &
														  (USP_INSTDESC_FLAG_DEST_USES_RESULT);								

										psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx] = uInstDescFlags;

										/*
											Record the new location of the data for
											this chunk
										*/
										uChunkIdx = psTexChanInfo->auChunkIdxForChan[eTexChanSwizzle];										
										psSample->asChunkReg[uChunkIdx] = *psDestReg; 

										/*
											Update the number of intermediate registers used
										*/
										uChunkRegOff = psTexChanInfo->auDestRegForChunk[uChunkIdx];
										if	((uChunkRegOff + 1) == (*(psSample->psSampleUnpack->puBaseSampleDestRegCount)))
										{
											/*
												For dependent depth F32 textures temporary register is always used
											*/
											if(!bAndForDF32Tex || bChanChunkNonDep)
											{												
												//psSample->uNumTempRegsUsed--;
												(*(psSample->psSampleUnpack->puBaseSampleDestRegCount))--;												 
												if(psSample->psSampleUnpack->puBaseSampleTempDestRegCount !=  psSample->psSampleUnpack->puBaseSampleDestRegCount)
												{
													(*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount))--;
												}
												#if 0
												#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
												if((psSample->uUnAlignedTempCount>0) && 
												   (psSample->uUnAlignedTempCount == psSample->uSampleTempCount))
												{
													//psSample->uNumTempRegsUsed -= psSample->uUnAlignedTempCount;
													psSample->uSampleTempCount -= psSample->uUnAlignedTempCount;
													psSample->uUnAlignedTempCount = 0;
												}
												#endif /* defined(SGX_FEATURE_UNIFIED_STORE_64BITS) */
												#endif
											}
										}

										/*
											Set sample dest is patched for this channel
										*/
										uChansUpdatedMask |= 1 << eTexChanSwizzle;

										uChansSampledDirect |= 1 << uChanIdx;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	/*
		If we modified one or more sample instructions, recalculate the source
		registers for the data for each channel
	*/
	if	(uChansSampledDirect)
	{
		if	(!SetupChanSrcRegs(psSample, psContext))
		{
			USP_DBGPRINT(("UnpackTextureDataToF32: Error updating src regs for each channel\n"));
			return IMG_FALSE;
		}
	}
	   
	/*
		Check for non-dependent reads that need to be unpacked from a temporary
		location
	*/
	if	(psSample->bNonDependent)
	{
		PUSP_REG	psPrevDestReg;
		IMG_UINT32	uPrevNumUnpackInsts;
		IMG_UINT32	uNumUnpackInsts;

		psPrevDestReg		= IMG_NULL;
		uNumUnpackInsts		= 0;
		uPrevNumUnpackInsts	= 0;
		uChanIdx			= 0;

		while (uChanIdx < USP_MAX_SAMPLE_CHANS)
		{
			USP_CHAN_FORMAT	eDataFmtForChan;
			PUSP_REG		psDestReg;
			IMG_BOOL		bChunksMoved;

			/*
				Skip this channel if we didn't need to sample it
			*/
			if	(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
			{
				uChanIdx++;				
				continue;
			}
			
			/* 
				Get either this channel has been sampled as Dependent 
				or Non Dependent. 
			*/			
			{
				USP_CHAN_SWIZZLE	eTexChanSwizzle;		
				eTexChanSwizzle		= (psSample->sTexChanInfo).aeTexChanForComp[uChanIdx];
				if (eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
				{
					IMG_UINT32			uChunkIdx;		

					uChunkIdx			= (psSample->sTexChanInfo).auChunkIdxForChan[eTexChanSwizzle];
					if(!(((psSample->sTexChanInfo).uTexNonDepChunkMask & (1 << uChunkIdx))))
					{
						uChanIdx++;
						continue;
					}
				}
			}

			/*
				Get the source and destination registers to use for this channel
			*/
			psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

			/*
				Detemine whether this channel can be unpacked/copied together
				with previous channels
			*/
			eDataFmtForChan	= GetSampleChannelFormat(psSample, uChanIdx);
			switch (eDataFmtForChan)
			{
				case USP_CHAN_FORMAT_U8:
				case USP_CHAN_FORMAT_S8:
				case USP_CHAN_FORMAT_U16:
				case USP_CHAN_FORMAT_S16:
				case USP_CHAN_FORMAT_F16:
				{
					/*
						We'll add a (new) PCKUPCK to handle this component
					*/
					uNumUnpackInsts++;

					break;
				}

				case USP_CHAN_FORMAT_F32:
				{
					/*
						We'll add a MOV to handle this component
					*/
					uNumUnpackInsts++;

					break;
				}

				case USP_CHAN_FORMAT_U24:
				{
					USP_DBGPRINT(("UnpackTextureDataToF32: U24 source data not yet supported\n"));
					return IMG_FALSE;
				}

				default:
				{
					USP_DBGPRINT(("UnpackTextureDataToF32: Unhandled source data format\n"));
					return IMG_FALSE;
				}
			}

			/*
				Check whether the last unpacking instruction potentially
				overwrites source data needed for the unpacking of later
				channels.
			*/
			bChunksMoved = IMG_FALSE;

			if	(uNumUnpackInsts != uPrevNumUnpackInsts)
			{
				if	(uNumUnpackInsts > 1)
				{
					USP_REG	sFinalDestReg;

					/*
						Is this unpack instruction writing to a result-reg?
					*/
					if	(psSample->psSampleUnpack->uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT)
					{
						PUSP_SHADER		psShader;
						PUSP_PROGDESC	psProgDesc;
						USP_REGTYPE		eFinalResultRegType;
						IMG_UINT32		uFinalResultRegNum;
						IMG_UINT32		uOrgResultRegNum;

						/*
							Remap the destination to the final result register
						*/
						psShader	= psSample->psShader;
						psProgDesc	= psShader->psProgDesc;

						eFinalResultRegType	= psShader->eFinalResultRegType;
						uFinalResultRegNum	= psShader->uFinalResultRegNum;
						uOrgResultRegNum	= psProgDesc->uDefaultPSResultRegNum;

						sFinalDestReg		= *psPrevDestReg;
						sFinalDestReg.eType	= eFinalResultRegType;
						sFinalDestReg.uNum	= (psPrevDestReg->uNum - uOrgResultRegNum) +
											  uFinalResultRegNum;
						psPrevDestReg		= &sFinalDestReg;
					}

					if	(!UnpackChunksFromTemp(psSample,
											   psPrevDestReg,
											   0xF,
											   uChanIdx,
											   psContext,
											   &bChunksMoved))
					{
						USP_DBGPRINT(("UnpackTextureDataToF32: Error testing whether non-dep data should be unpacked from temps\n"));
						return IMG_FALSE;
					}
				}

				uPrevNumUnpackInsts	= uNumUnpackInsts;
			}

			/*
				Restart if we have altered the source data for any chunks
			*/
			if	(bChunksMoved)
			{
				uNumUnpackInsts		= 0;
				uPrevNumUnpackInsts	= 0;
				uChanIdx			= 0;
			}
			else
			{
				psPrevDestReg = psDestReg;
				uChanIdx++;
			}
		}
	}

	/*
		Generate instructions to save the sampled data for each channel
		into the required final register locations
	*/

	/*
		Set up the instruction description flags for the unpacking instructions.
	*/
	/*
		Although the sources of sample unpack can not be results of the shader
	*/
	uInstDescFlags	= psSample->psSampleUnpack->uInstDescFlags;
#if 0 
	& ~(USP_INSTDESC_FLAG_SRC0_USES_RESULT |
												  USP_INSTDESC_FLAG_SRC1_USES_RESULT |
												  USP_INSTDESC_FLAG_SRC2_USES_RESULT);
	if((!psSample->bNonDependent))
	{
		/*	
			If this is a dependent texture sample then the USC has ensured the internal
			registers aren't live immediately after the sample instruction (even if they
			were live immediately before).

			For a non-dependent texture sample even if the USP was forced to generate 
			an SMP+WDF instruction sequence then the internal registers (whose contents
			are lost when the WDF executes) have been restored by this point.
		*/
		uInstDescFlags &= ~USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
	}
#endif
	
	bSkipInv			= (IMG_BOOL)(!(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV));
	uHWInstCount		= psSample->psSampleUnpack->uUnpackInstCount;
	bFirstUnpackInst	= IMG_TRUE;

	for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		USP_CHAN_FORMAT	eDataFmtForChan;
		PHW_INST		psHWInst;
		PUSP_OPCODE		peOpcode;
		IMG_PUINT32		puInstDescFlags;
		PUSP_REG		psChanReg;
		PUSP_REG		psDestReg;

		/*
			Skip this channel if we didn't need to sample it, or it has been
			sampled directly to the the destination
		*/
		if	(!((psSample->psSampleUnpack->uMask ^ uChansSampledDirect) & (1 << uChanIdx)))
		{
			continue;	
		}

		/*
			Get the source and destination registers to use for this 
			component of the result
		*/
		psChanReg = &psSample->asChanReg[uChanIdx];
		psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

		/*
			Generate an instruction to copy and convert the data for this
			channel
		*/
		psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
		peOpcode		= &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
		puInstDescFlags	= &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

		eDataFmtForChan	= GetSampleChannelFormat(psSample, uChanIdx);
		switch (eDataFmtForChan)
		{
			case USP_CHAN_FORMAT_U8:
			case USP_CHAN_FORMAT_S8:
			case USP_CHAN_FORMAT_U16:
			case USP_CHAN_FORMAT_S16:
			case USP_CHAN_FORMAT_F16:
			{
				/*
					Encode a PCKUNPCK instruction to copy and convert
					the sampled channel to F32 in the required register
				*/
				if	(!HWInstEncodePCKUNPCKInst(psHWInst,
											   bSkipInv,
											   USP_PCKUNPCK_FMT_F32,
											   aePCKUNPCKFmtForChanFmt[eDataFmtForChan],
											   IMG_TRUE,
											   0xF,
											   psDestReg,
											   0,
											   1,
											   psChanReg,
											   (PUSP_REG)&sHWConstZero))
				{
					USP_DBGPRINT(("UnpackTextureDataToF32: Error encoding PCKUNPCK inst\n"));
					return IMG_FALSE;
				}

				*peOpcode = USP_OPCODE_PCKUNPCK;

				*puInstDescFlags = uInstDescFlags;				

				bFirstUnpackInst = IMG_FALSE;
				uHWInstCount++;

				break;
			}

			case USP_CHAN_FORMAT_F32:
			{
				/*
					Copy the F32 data using an an unconditional MOV instruction
				*/
				#if defined(SGX_FEATURE_USE_VEC34)
				{
					USP_REG sImdVal;
					sImdVal.eType	= USP_REGTYPE_IMM;
					sImdVal.uNum	= 0xFFFFFFFF;
					sImdVal.eFmt	= psChanReg->eFmt;
					sImdVal.uComp	= psChanReg->uComp;
					sImdVal.eDynIdx = USP_DYNIDX_NONE;
					if	(!HWInstEncodeANDInst(psHWInst, 
											  1,
											  IMG_FALSE, /* bUsePred */
											  IMG_FALSE, /* bNegatePred */
											  0, /* uPredNum */
											  bSkipInv,
											  psDestReg, 
											  psChanReg, 
											  &sImdVal))
					{
						USP_DBGPRINT(("UnpackTextureDataToF32: Error encoding data movement (AND) HW-inst\n"));
						return IMG_FALSE;
					}
					*peOpcode = USP_OPCODE_AND;
				}
				#else
				if	(!HWInstEncodeMOVInst(psHWInst,
										  1,
										  bSkipInv,
										  psDestReg,
										  psChanReg))
				{
					USP_DBGPRINT(("UnpackTextureDataToF32: Error encoding MOV inst\n"));
					return IMG_FALSE;
				}
				*peOpcode = USP_OPCODE_MOVC;
				#endif /* defined(SGX_FEATURE_USE_VEC34) */

				*puInstDescFlags = uInstDescFlags;				

				bFirstUnpackInst = IMG_FALSE;
				uHWInstCount++;
				break;
			}

			case USP_CHAN_FORMAT_U24:
			{
				USP_DBGPRINT(("UnpackTextureDataToF32: U24 source data not yet supported\n"));
				return IMG_FALSE;
			}

			case USP_CHAN_FORMAT_S32:
		    case USP_CHAN_FORMAT_U32:
			case USP_CHAN_FORMAT_INVALID:
			{
				USP_DBGPRINT(("UnpackTextureDataToF32: Invalid channel format\n"));
				return IMG_FALSE;
			}
		}

		/*
			If the first unpacking instruction writes to an IReg, then it must
			be live before any following unpack code.
		*/
		if	(!bFirstUnpackInst)
		{
			if	(psDestReg->eType == USP_REGTYPE_INTERNAL)
			{
				uInstDescFlags |= USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
			}
		}
	}

	/*
		Record the number of instructions generated to unpack the texture
	*/
	psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;

	return IMG_TRUE;
}

/******************************************************************************
 Name:		UnpackTextureDataToU32

 Purpose:	Unpack sampled texture data to U32 results

 Inputs:	psSample	- The dependent sample to generate code for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL UnpackTextureDataToU32(PUSP_SAMPLE	psSample,
									   PUSP_CONTEXT	psContext)
{
#if defined(SGX_FEATURE_USE_VEC34)
	USP_DBGPRINT(("%s:%u: Implement Me\n",__FILE__,__LINE__));
	PVR_UNREFERENCED_PARAMETER (psSample);
	PVR_UNREFERENCED_PARAMETER (psContext);
	return IMG_FALSE;
#else /* defined(SGX_FEATURE_USE_VEC34) */

	static const USP_PCKUNPCK_FMT aePCKUNPCKFmtForChanFmt[] =
	{
		USP_PCKUNPCK_FMT_U8,	/* USP_CHAN_FORMAT_U8 */
		USP_PCKUNPCK_FMT_S8,	/* USP_CHAN_FORMAT_S8 */
		USP_PCKUNPCK_FMT_U16,	/* USP_CHAN_FORMAT_U16 */
		USP_PCKUNPCK_FMT_S16,	/* USP_CHAN_FORMAT_S16 */
		USP_PCKUNPCK_FMT_F16,	/* USP_CHAN_FORMAT_F16 */
		USP_PCKUNPCK_FMT_F32,	/* USP_CHAN_FORMAT_F32 */
		USP_PCKUNPCK_FMT_INVALID/* USP_CHAN_FORMAT_U24 */
	};

	static const USP_REG sHWConstZero =
	{
		USP_REGTYPE_SPECIAL,
		HW_CONST_ZERO,
		USP_REGFMT_U16,
		0,
		USP_DYNIDX_NONE
	};
	
	IMG_BOOL   bSkipInv;
	IMG_UINT32 uChanIdx;
	IMG_UINT32 uHWInstCount;
	IMG_UINT32 uChansSampledDirect;
	IMG_BOOL   bFirstUnpackInst;
	IMG_UINT32 uInstDescFlags            = 0;
	IMG_UINT32 uChansUpdatedMask         = 0;
	IMG_BOOL   bChunkNotSampledToTempReg = IMG_FALSE;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	/*
	  Look for sample instructions that can sample directly to the
	  intended destination
	 */
	uChansSampledDirect = 0;
	if (psSample->psSampleUnpack->bCanSampleDirectToDest)
	{
		for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
		{
			IMG_BOOL bChanChunkNonDep = IMG_FALSE;
			
			/*
			  Skip this channel if we didn't need to sample it
			*/
			if	(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
			{
				continue;	
			}

			/*
			  Only channels that are in the same format as the destination
			  (i.e U32 here) can be sampled directly
			 */
			if  (GetSampleChannelFormat(psSample, uChanIdx) == USP_CHAN_FORMAT_U32)
			{
				PUSP_REG	psChanReg;
				PUSP_REG	psDestReg;
				USP_REG		sFinalDestReg;

				/*
				  Get the source and destination registers to use for this 
				  component of the result
				*/
				psChanReg = &psSample->asChanReg[uChanIdx];
				psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

				if( psSample->bNonDependent )
				{
					USP_DBGPRINT(("%s:%u: Non dependent samples not supported yet.\n",__FILE__,__LINE__));
					return IMG_FALSE;
				}

				sFinalDestReg = *psDestReg;

				if( psSample->psSampleUnpack->uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT )
				{
					PUSP_SHADER		psShader;
					PUSP_PROGDESC	psProgDesc;
					USP_REGTYPE		eFinalResultRegType;
					IMG_UINT32		uFinalResultRegNum;
					IMG_UINT32		uOrgResultRegNum;

					/*
					  Remap the destination to the final result register
					*/
					psShader	= psSample->psShader;
					psProgDesc	= psShader->psProgDesc;

					eFinalResultRegType	= psShader->eFinalResultRegType;
					uFinalResultRegNum	= psShader->uFinalResultRegNum;
					uOrgResultRegNum	= psProgDesc->uDefaultPSResultRegNum;

					sFinalDestReg.eType	= eFinalResultRegType;
					sFinalDestReg.uNum	= (psDestReg->uNum - uOrgResultRegNum) + 
						uFinalResultRegNum;
				}

				if	(bChanChunkNonDep && bChunkNotSampledToTempReg)
				{
					if	(
						(sFinalDestReg.eDynIdx == USP_DYNIDX_NONE) &&
						(sFinalDestReg.eType	== psChanReg->eType) &&
						(sFinalDestReg.uNum	== psChanReg->uNum)
						)
					{
						/*
						  No unpacking code needed, since the pre-sampled
						  data is in the right place already for this
						  channel of the destination.
						*/
						uChansSampledDirect |= 1 << uChanIdx;
					}
				}
				else
				{
						PUSP_TEX_CHAN_INFO    psTexChanInfo;
						PUSP_TEX_CHAN_FORMAT  psTexFormat;
						USP_CHAN_SWIZZLE	  eTexChanSwizzle;

						/*
							Did we sample the texture for this channel of the
							destination?
						*/
						psTexFormat		= &psSample->sSamplerDesc.sTexChanFormat;
						psTexChanInfo	= &psSample->sTexChanInfo;
						eTexChanSwizzle = psTexChanInfo->aeTexChanForComp[uChanIdx];

						if	(
								(eTexChanSwizzle >= USP_CHAN_SWIZZLE_CHAN_0) &&
								(eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
							)
						{
							USP_CHAN_CONTENT	eTexChanContent;
							IMG_UINT32			uTexChunkIdx;
							IMG_UINT32			uNumSMPDestRegs;

							/*
								Get the number of destination registers for the SMP instruction which
								writes the data for this channel. We can only change the SMP to write
								direct if it writes only one register.
							*/
							uTexChunkIdx		= psSample->sTexChanInfo.auChunkIdxForChan[eTexChanSwizzle];
							uNumSMPDestRegs		= psSample->sTexChanInfo.auNumRegsForChunk[uTexChunkIdx];

							eTexChanContent = psTexFormat->aeChanContent[eTexChanSwizzle];
							if	((eTexChanContent == USP_CHAN_CONTENT_DATA) && 
								!(uChansUpdatedMask & (1 << eTexChanSwizzle)) &&
								 uNumSMPDestRegs == 1)
							{
								HW_INST		sTempSampleInst;
								PHW_INST	psSampleInst;
								IMG_UINT32	uSampleInstIdx;
								USP_OPCODE	eSampleInstOpcode;

								/*
									Lookup details of the sample instruction used to read
									this chunk
								*/
								psSampleInst		= psSample->apsSMPInsts[uChanIdx];
								uSampleInstIdx		= psSampleInst - psSample->psSampleUnpack->asSampleInsts;
								eSampleInstOpcode	= psSample->psSampleUnpack->aeOpcodeForSampleInsts[uSampleInstIdx];

								/*
									Is the destination register-type one that is supported
									by the SMP instruction?
								*/
								sTempSampleInst	= *psSampleInst;

								if	(HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
																eSampleInstOpcode,
																&sTempSampleInst,
																&sFinalDestReg))
								{
									IMG_BOOL	bSMPReadsDest;

									/*
										Check whether the destination register may be used
										by one of the sources for the sample instructions.

										NB:	All SMP instructions generated for a sample will be
											identical with respect to the sources they use (apart from
											the state source for SMP), so we only need to check for
											clashes with one inst here.
									*/
									if	(!SampleInstReadsReg(psSample,
															 psSampleInst,
															 &sFinalDestReg,
															 psContext,
															 &bSMPReadsDest))
									{
										USP_DBGPRINT(("UnpackTextureDataToF32: Error checking for SMP inst that references dest\n"));
										return IMG_FALSE;
									}

									/*
										Only sample directly to the destination register
										if it isn't used through any of the sources.
									*/
									if	(!bSMPReadsDest)
									{
										IMG_UINT32	uInstDescFlags;
										IMG_UINT32	uChunkIdx;
										IMG_UINT32	uChunkRegOff;

										/*
											Modify the sample instruction to write
											directly to the destination register.

											NB: We don't use the 'final' destination register
												here (i.e. after result-remapping) since the
												operands should be remapped just like any other
												instruction.
										*/
										HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
																   eSampleInstOpcode,
																   psSampleInst, 
																   psDestReg);

										/*
											Now that this sample instruction writes directly
											to the intended destination, rather than an
											intermediate temporary, it may reference a result
											or input register.
										*/
										uInstDescFlags = psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx];

										uInstDescFlags |= psSample->psSampleUnpack->uInstDescFlags &
														  (USP_INSTDESC_FLAG_DEST_USES_RESULT);								

										psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx] = uInstDescFlags;

										/*
											Record the new location of the data for
											this chunk
										*/
										uChunkIdx = psTexChanInfo->auChunkIdxForChan[eTexChanSwizzle];										
										psSample->asChunkReg[uChunkIdx] = *psDestReg; 

										/*
											Update the number of intermediate registers used
										*/
										uChunkRegOff = psTexChanInfo->auDestRegForChunk[uChunkIdx];
										if	((uChunkRegOff + 1) == (*(psSample->psSampleUnpack->puBaseSampleDestRegCount)))
										{
											/*
												For dependent depth F32 textures temporary register is always used
											*/
											if(bChanChunkNonDep)
											{												
												//psSample->uNumTempRegsUsed--;
												(*(psSample->psSampleUnpack->puBaseSampleDestRegCount))--;												 
												if(psSample->psSampleUnpack->puBaseSampleTempDestRegCount !=  psSample->psSampleUnpack->puBaseSampleDestRegCount)
												{
													(*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount))--;
												}
											}
										}

										/*
											Set sample dest is patched for this channel
										*/
										uChansUpdatedMask |= 1 << eTexChanSwizzle;

										uChansSampledDirect |= 1 << uChanIdx;
									}
								}
							}
						}
					}
			}
		}
	}

	/* Currently only dependent samples supported */
	if( psSample->bNonDependent )
	{
		USP_DBGPRINT(("%s:%u: Non dependent samples not supported yet.\n",__FILE__,__LINE__));
		return IMG_FALSE;
	}

	bSkipInv			= (IMG_BOOL)(!(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV));
	uHWInstCount		= psSample->psSampleUnpack->uUnpackInstCount;
	bFirstUnpackInst	= IMG_TRUE;
	
	for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		USP_CHAN_FORMAT	eDataFmtForChan;
		PHW_INST		psHWInst;
		PUSP_OPCODE		peOpcode;
		IMG_PUINT32		puInstDescFlags;
		PUSP_REG		psChanReg;
		PUSP_REG		psDestReg;

		/*
			Skip this channel if we didn't need to sample it, or it has been
			sampled directly to the the destination
		*/
		if	(!((psSample->psSampleUnpack->uMask ^ uChansSampledDirect) & (1 << uChanIdx)))
		{
			continue;	
		}

		/*
			Get the source and destination registers to use for this 
			component of the result
		*/
		psChanReg = &psSample->asChanReg[uChanIdx];
		psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

		/*
			Generate an instruction to copy and convert the data for this
			channel
		*/
		psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
		peOpcode		= &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
		puInstDescFlags	= &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

		eDataFmtForChan	= GetSampleChannelFormat(psSample, uChanIdx);

		switch (eDataFmtForChan)
		{
			case USP_CHAN_FORMAT_U8:
		    case USP_CHAN_FORMAT_U16:
			{
				/*
				  PCK D1 U16 <- U8
				 */
				
				/*
				  Encode a PCKUNPCK instruction to copy and convert
				  the sampled channel to U16 in the required register
				*/
				if	(!HWInstEncodePCKUNPCKInst(psHWInst,
					                           bSkipInv,
					                           USP_PCKUNPCK_FMT_U16,
					                           aePCKUNPCKFmtForChanFmt[eDataFmtForChan],
					                           IMG_FALSE,
					                           0xF,
					                           psDestReg,
					                           1,
					                           0,
					                           psChanReg,
					                           (PUSP_REG)&sHWConstZero))
				{
					USP_DBGPRINT(("UnpackTextureDataToF32: Error encoding PCKUNPCK inst\n"));
					return IMG_FALSE;
				}

				*peOpcode        = USP_OPCODE_PCKUNPCK;
				*puInstDescFlags = uInstDescFlags;				
				bFirstUnpackInst = IMG_FALSE;
				uHWInstCount++;
				
				break;
			}
		    case USP_CHAN_FORMAT_U32:
		    {
			    /*
			      Copy the data using an unconditional MOV instruction
			     */

				#if defined(SGX_FEATURE_USE_VEC34)
				{
					USP_REG sImdVal;
					sImdVal.eType	= USP_REGTYPE_IMM;
					sImdVal.uNum	= 0xFFFFFFFF;
					sImdVal.eFmt	= psChanReg->eFmt;
					sImdVal.uComp	= psChanReg->uComp;
					sImdVal.eDynIdx = USP_DYNIDX_NONE;
					if	(!HWInstEncodeANDInst(psHWInst, 
											  1,
											  IMG_FALSE, /* bUsePred */
											  IMG_FALSE, /* bNegatePred */
											  0, /* uPredNum */
											  bSkipInv,
											  psDestReg, 
											  psChanReg, 
											  &sImdVal))
					{
						USP_DBGPRINT(("UnpackTextureDataToF32: Error encoding data movement (AND) HW-inst\n"));
						return IMG_FALSE;
					}
					*peOpcode = USP_OPCODE_AND;
				}
				#else
			    if  (!HWInstEncodeMOVInst(psHWInst,
			                              1,
			                              bSkipInv,
			                              psDestReg,
			                              psChanReg))
			    {
				    USP_DBGPRINT(("UnpackTextureDataToU32: Error encoding MOV inst\n"));
				    return IMG_FALSE;
			    }

			    *peOpcode        = USP_OPCODE_MOVC;
				#endif /* defined(SGX_FEATURE_USE_VEC34) */

			    *puInstDescFlags = uInstDescFlags;
			    bFirstUnpackInst = IMG_FALSE;
			    uHWInstCount++;
			    
			    break;
		    }
		    default:
		    {
			    USP_DBGPRINT(("Invalid destination channel format.\n"));
			    return IMG_FALSE;
		    }
		}

		if  (!bFirstUnpackInst)
		{
			if  (psDestReg->eType == USP_REGTYPE_INTERNAL)
			{
				uInstDescFlags |= USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
			}
		}
	}

	/*
		Record the number of instructions generated to unpack the texture
	*/
	psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;
	
	return IMG_TRUE;
	#endif/* defined(SGX_FEATURE_USE_VEC34) */
}

/******************************************************************************
 Name:		UnpackTextureDataToS32

 Purpose:	Unpack sampled texture data to S32 results

 Inputs:	psSample	- The dependent sample to generate code for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL UnpackTextureDataToS32(PUSP_SAMPLE  psSample,
                                       PUSP_CONTEXT psContext)
{
#if defined(SGX_FEATURE_USE_VEC34)
	PVR_UNREFERENCED_PARAMETER (psSample);
	PVR_UNREFERENCED_PARAMETER (psContext);
	USP_DBGPRINT(("%s:%u: Implement Me\n",__FILE__,__LINE__));
	return IMG_FALSE;
#else /* defined(SGX_FEATURE_USE_VEC34) */

	static const USP_PCKUNPCK_FMT aePCKUNPCKFmtForChanFmt[] =
	{
		USP_PCKUNPCK_FMT_U8,	/* USP_CHAN_FORMAT_U8 */
		USP_PCKUNPCK_FMT_S8,	/* USP_CHAN_FORMAT_S8 */
		USP_PCKUNPCK_FMT_U16,	/* USP_CHAN_FORMAT_U16 */
		USP_PCKUNPCK_FMT_S16,	/* USP_CHAN_FORMAT_S16 */
		USP_PCKUNPCK_FMT_F16,	/* USP_CHAN_FORMAT_F16 */
		USP_PCKUNPCK_FMT_F32,	/* USP_CHAN_FORMAT_F32 */
		USP_PCKUNPCK_FMT_INVALID/* USP_CHAN_FORMAT_U24 */
	};

	static const USP_REG sHWConstZero =
	{
		USP_REGTYPE_SPECIAL,
		HW_CONST_ZERO,
		USP_REGFMT_U16,
		0,
		USP_DYNIDX_NONE
	};
	
	IMG_BOOL        bSkipInv;
	IMG_UINT32      uChanIdx;
	IMG_UINT32      uHWInstCount;
	IMG_UINT32      uChansSampledDirect;
	IMG_BOOL        bFirstUnpackInst;
	IMG_UINT32      uInstDescFlags            = 0;
	IMG_UINT32      uChansUpdatedMask         = 0;
	IMG_BOOL        bChunkNotSampledToTempReg = IMG_FALSE;
	
	USP_CHAN_FORMAT	eDataFmtForChan;
	PHW_INST		psHWInst;
	PUSP_OPCODE		peOpcode;
	IMG_PUINT32		puInstDescFlags;
	
	IMG_UINT32      uMask                     = 0;
	IMG_UINT32      uSampleTempRegCount       = 0;
	USP_REG         sTempReg                  = {0};
	
	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	/*
	  Look for sample instructions that can sample directly to the
	  intended destination
	 */
	uChansSampledDirect = 0;
	if (psSample->psSampleUnpack->bCanSampleDirectToDest)
	{
		for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
		{
			IMG_BOOL bChanChunkNonDep = IMG_FALSE;
			
			/*
			  Skip this channel if we didn't need to sample it
			*/
			if	(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
			{
				continue;	
			}

			/*
			  Only channels that are in the same format as the destination
			  (i.e S32 here) can be sampled directly
			 */
			if  (GetSampleChannelFormat(psSample, uChanIdx) == USP_CHAN_FORMAT_S32)
			{
				PUSP_REG	psChanReg;
				PUSP_REG	psDestReg;
				USP_REG		sFinalDestReg;

				/*
				  Get the source and destination registers to use for this 
				  component of the result
				*/
				psChanReg = &psSample->asChanReg[uChanIdx];
				psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

				if( psSample->bNonDependent )
				{
					USP_DBGPRINT(("%s:%u: Non dependent samples not supported yet.\n",__FILE__,__LINE__));
					return IMG_FALSE;
				}

				sFinalDestReg = *psDestReg;

				if( psSample->psSampleUnpack->uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT )
				{
					PUSP_SHADER		psShader;
					PUSP_PROGDESC	psProgDesc;
					USP_REGTYPE		eFinalResultRegType;
					IMG_UINT32		uFinalResultRegNum;
					IMG_UINT32		uOrgResultRegNum;

					/*
					  Remap the destination to the final result register
					*/
					psShader	= psSample->psShader;
					psProgDesc	= psShader->psProgDesc;

					eFinalResultRegType	= psShader->eFinalResultRegType;
					uFinalResultRegNum	= psShader->uFinalResultRegNum;
					uOrgResultRegNum	= psProgDesc->uDefaultPSResultRegNum;

					sFinalDestReg.eType	= eFinalResultRegType;
					sFinalDestReg.uNum	= (psDestReg->uNum - uOrgResultRegNum) + 
						uFinalResultRegNum;
				}

				if	(bChanChunkNonDep && bChunkNotSampledToTempReg)
				{
					if	(
						(sFinalDestReg.eDynIdx == USP_DYNIDX_NONE) &&
						(sFinalDestReg.eType	== psChanReg->eType) &&
						(sFinalDestReg.uNum	== psChanReg->uNum)
						)
					{
						/*
						  No unpacking code needed, since the pre-sampled
						  data is in the right place already for this
						  channel of the destination.
						*/
						uChansSampledDirect |= 1 << uChanIdx;
					}
				}
				else
				{
						PUSP_TEX_CHAN_INFO    psTexChanInfo;
						PUSP_TEX_CHAN_FORMAT  psTexFormat;
						USP_CHAN_SWIZZLE	  eTexChanSwizzle;

						/*
							Did we sample the texture for this channel of the
							destination?
						*/
						psTexFormat		= &psSample->sSamplerDesc.sTexChanFormat;
						psTexChanInfo	= &psSample->sTexChanInfo;
						eTexChanSwizzle = psTexChanInfo->aeTexChanForComp[uChanIdx];

						if	(
								(eTexChanSwizzle >= USP_CHAN_SWIZZLE_CHAN_0) &&
								(eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
							)
						{
							USP_CHAN_CONTENT	eTexChanContent;
							IMG_UINT32			uTexChunkIdx;
							IMG_UINT32			uNumSMPDestRegs;

							/*
								Get the number of destination registers for the SMP instruction which
								writes the data for this channel. We can only change the SMP to write
								direct if it writes only one register.
							*/
							uTexChunkIdx		= psSample->sTexChanInfo.auChunkIdxForChan[eTexChanSwizzle];
							uNumSMPDestRegs		= psSample->sTexChanInfo.auNumRegsForChunk[uTexChunkIdx];

							eTexChanContent = psTexFormat->aeChanContent[eTexChanSwizzle];
							if	((eTexChanContent == USP_CHAN_CONTENT_DATA) && 
								!(uChansUpdatedMask & (1 << eTexChanSwizzle)) &&
								 uNumSMPDestRegs == 1)
							{
								HW_INST		sTempSampleInst;
								PHW_INST	psSampleInst;
								IMG_UINT32	uSampleInstIdx;
								USP_OPCODE	eSampleInstOpcode;

								/*
									Lookup details of the sample instruction used to read
									this chunk
								*/
								psSampleInst		= psSample->apsSMPInsts[uChanIdx];
								uSampleInstIdx		= psSampleInst - psSample->psSampleUnpack->asSampleInsts;
								eSampleInstOpcode	= psSample->psSampleUnpack->aeOpcodeForSampleInsts[uSampleInstIdx];

								/*
									Is the destination register-type one that is supported
									by the SMP instruction?
								*/
								sTempSampleInst	= *psSampleInst;

								if	(HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
																eSampleInstOpcode,
																&sTempSampleInst,
																&sFinalDestReg))
								{
									IMG_BOOL	bSMPReadsDest;

									/*
										Check whether the destination register may be used
										by one of the sources for the sample instructions.

										NB:	All SMP instructions generated for a sample will be
											identical with respect to the sources they use (apart from
											the state source for SMP), so we only need to check for
											clashes with one inst here.
									*/
									if	(!SampleInstReadsReg(psSample,
															 psSampleInst,
															 &sFinalDestReg,
															 psContext,
															 &bSMPReadsDest))
									{
										USP_DBGPRINT(("UnpackTextureDataToF32: Error checking for SMP inst that references dest\n"));
										return IMG_FALSE;
									}

									/*
										Only sample directly to the destination register
										if it isn't used through any of the sources.
									*/
									if	(!bSMPReadsDest)
									{
										IMG_UINT32	uInstDescFlags;
										IMG_UINT32	uChunkIdx;
										IMG_UINT32	uChunkRegOff;

										/*
											Modify the sample instruction to write
											directly to the destination register.

											NB: We don't use the 'final' destination register
												here (i.e. after result-remapping) since the
												operands should be remapped just like any other
												instruction.
										*/
										HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
																   eSampleInstOpcode,
																   psSampleInst, 
																   psDestReg);

										/*
											Now that this sample instruction writes directly
											to the intended destination, rather than an
											intermediate temporary, it may reference a result
											or input register.
										*/
										uInstDescFlags = psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx];

										uInstDescFlags |= psSample->psSampleUnpack->uInstDescFlags &
														  (USP_INSTDESC_FLAG_DEST_USES_RESULT);								

										psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx] = uInstDescFlags;

										/*
											Record the new location of the data for
											this chunk
										*/
										uChunkIdx = psTexChanInfo->auChunkIdxForChan[eTexChanSwizzle];										
										psSample->asChunkReg[uChunkIdx] = *psDestReg; 

										/*
											Update the number of intermediate registers used
										*/
										uChunkRegOff = psTexChanInfo->auDestRegForChunk[uChunkIdx];
										if	((uChunkRegOff + 1) == (*(psSample->psSampleUnpack->puBaseSampleDestRegCount)))
										{
											/*
												For dependent depth F32 textures temporary register is always used
											*/
											if(bChanChunkNonDep)
											{												
												//psSample->uNumTempRegsUsed--;
												(*(psSample->psSampleUnpack->puBaseSampleDestRegCount))--;												 
												if(psSample->psSampleUnpack->puBaseSampleTempDestRegCount !=  psSample->psSampleUnpack->puBaseSampleDestRegCount)
												{
													(*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount))--;
												}
											}
										}

										/*
											Set sample dest is patched for this channel
										*/
										uChansUpdatedMask |= 1 << eTexChanSwizzle;

										uChansSampledDirect |= 1 << uChanIdx;
									}
								}
							}
						}
					}
			}
		}
	}

	/* Currently only dependent samples supported */
	if( psSample->bNonDependent )
	{
		USP_DBGPRINT(("%s:%u: Non dependent samples not supported yet.\n",__FILE__,__LINE__));
		return IMG_FALSE;
	}

	bSkipInv			= (IMG_BOOL)(!(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV));
	uHWInstCount		= psSample->psSampleUnpack->uUnpackInstCount;
	bFirstUnpackInst	= IMG_TRUE;

	/*
	  If the channel format is S8/S16 for the first channel we assume the remaining
	  channels are also the same format.

	  This allows us to move the LIMM instruction outside the loop since it will
	  only be required once (but only for S8/S16 channels).
	 */
	{
		IMG_BOOL bRequireLIMM = IMG_FALSE;

		/*
		  First check that we require the LIMM.
		 */
		for(uChanIdx=0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
		{
			if	(!((psSample->psSampleUnpack->uMask ^ uChansSampledDirect) & (1 << uChanIdx)))
			{
				continue;	
			}

			bRequireLIMM = IMG_TRUE;
		}

		if( bRequireLIMM )
		{
			/* Setup a single temporary register */
			uSampleTempRegCount = psSample->psSampleUnpack->uSampleUnpackTempRegCount;

			sTempReg.eType = psSample->psSampleUnpack->eSampleUnpackTempRegType;
			sTempReg.uNum  = psSample->psSampleUnpack->uSampleUnpackTempRegNum + uSampleTempRegCount++;

			/* Update temporary count */
			psSample->psSampleUnpack->uSampleUnpackTempRegCount = uSampleTempRegCount;
			
			/* Assumption: Channel 0 is the same format as the others */
			eDataFmtForChan	= GetSampleChannelFormat(psSample, 0);

#if defined(DEBUG)
			for(uChanIdx=1; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
			{
				if(GetSampleChannelFormat(psSample, uChanIdx) != eDataFmtForChan)
				{
					USP_DBGPRINT(("UnpackTextureDataToS32: Channel format mismatch!\n"));
					return IMG_FALSE;
				}
			}
#endif/*defined(DEBUG)*/
			
			/*
			  Our mask is always the 16bit signed version because we will always
			  unpack to signed 16 bit integers.
			*/
			uMask = 0xFFFF8000;
			
			psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
			peOpcode		= &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
			puInstDescFlags	= &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

			/*
			  Load in our mask for the destination
			*/
			if(!HWInstEncodeLIMMInst(psHWInst,
			                         1,
			                         &sTempReg,
			                         uMask))
			{
				USP_DBGPRINT(("UnpackTextureDataToS32: Error encoding LIMM instruction\n"));
				return IMG_FALSE;
			}

			*peOpcode        = USP_OPCODE_LIMM;
			*puInstDescFlags = uInstDescFlags;				
			uHWInstCount++;
		}
	}
	
	for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		PUSP_REG		psChanReg;
		PUSP_REG		psDestReg;

		/*
			Skip this channel if we didn't need to sample it, or it has been
			sampled directly to the the destination
		*/
		if	(!((psSample->psSampleUnpack->uMask ^ uChansSampledDirect) & (1 << uChanIdx)))
		{
			continue;	
		}

		/*
			Get the source and destination registers to use for this 
			component of the result
		*/
		psChanReg = &psSample->asChanReg[uChanIdx];
		psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

		/*
			Generate an instruction to copy and convert the data for this
			channel
		*/
		psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
		peOpcode		= &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
		puInstDescFlags	= &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

		eDataFmtForChan	= GetSampleChannelFormat(psSample, uChanIdx);

		switch (eDataFmtForChan)
		{
			case USP_CHAN_FORMAT_S8:
		    case USP_CHAN_FORMAT_S16:
			{
				/*
				  Encode a PCKUNPCK instruction to copy and convert
				  the sampled channel to U16 in the required register
				*/
				if	(!HWInstEncodePCKUNPCKInst(psHWInst,
					                           bSkipInv,
					                           USP_PCKUNPCK_FMT_S16,
					                           aePCKUNPCKFmtForChanFmt[eDataFmtForChan],
					                           IMG_FALSE,
					                           0xF,
					                           psDestReg,
					                           1,
					                           0,
					                           psChanReg,
					                           (PUSP_REG)&sHWConstZero))
				{
					USP_DBGPRINT(("UnpackTextureDataToS32: Error encoding PCKUNPCK inst\n"));
					return IMG_FALSE;
				}

				*peOpcode        = USP_OPCODE_PCKUNPCK;
				*puInstDescFlags = uInstDescFlags;				
				uHWInstCount++;

				psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
				peOpcode		= &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
				puInstDescFlags	= &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];
				
				/*
				  Check if the high bit is set, if so then we set the top 16 bits				  
				 */
				if(!HWInstEncodeTESTInst(psHWInst,
										 1, /* uRepeatCount */
										 EURASIA_USE1_TEST_CRCOMB_AND,
				                         psDestReg,/*ignored*/
										 psDestReg,
				                         &sTempReg,
										 0, /* predicate destination */
										 EURASIA_USE1_TEST_STST_NONE,
										 EURASIA_USE1_TEST_ZTST_NOTZERO,
										 EURASIA_USE1_TEST_CHANCC_SELECT0,
										 EURASIA_USE0_TEST_ALUSEL_BITWISE,
										 EURASIA_USE0_TEST_ALUOP_BITWISE_AND))
				{
					USP_DBGPRINT(("UnpackTextureDataToS32: Unable to encode HW TEST instruction\n"));
					return IMG_FALSE;
				}

				*peOpcode        = USP_OPCODE_TEST;
				*puInstDescFlags = uInstDescFlags;				
				uHWInstCount++;

				psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
				peOpcode		= &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
				puInstDescFlags	= &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

				/*
				  The high bit was set (i.e. either the 8th or 16th bit), so set the appropriate bits
				  by ORing in our mask.
				 */
				if(!HWInstEncodeORInst(psHWInst,
				                       1,
				                       bSkipInv,
				                       psDestReg,
				                       psDestReg,
				                       &sTempReg))
				{
					USP_DBGPRINT(("UnpackTextureDataToS32: Unable to encode HW OR instruction\n"));
					return IMG_FALSE;
				}

				/* Predicate on register 0 */
				{
					psHWInst->uWord1 |=  ((EURASIA_USE1_EPRED_P0) << EURASIA_USE1_EPRED_SHIFT) &
						(~EURASIA_USE1_EPRED_CLRMSK);
				}

				*peOpcode        = USP_OPCODE_OR;
				*puInstDescFlags = uInstDescFlags;				
				uHWInstCount++;
				
				bFirstUnpackInst = IMG_FALSE;
				break;
			}
		    case USP_CHAN_FORMAT_S32:
		    {
			    /*
			      Copy the data using an unconditional MOV instruction
			     */
				#if defined(SGX_FEATURE_USE_VEC34)
				{
					USP_REG sImdVal;
					sImdVal.eType	= USP_REGTYPE_IMM;
					sImdVal.uNum	= 0xFFFFFFFF;
					sImdVal.eFmt	= psChanReg->eFmt;
					sImdVal.uComp	= psChanReg->uComp;
					sImdVal.eDynIdx = USP_DYNIDX_NONE;
					if	(!HWInstEncodeANDInst(psHWInst, 
											  1,
											  IMG_FALSE, /* bUsePred */
											  IMG_FALSE, /* bNegatePred */
											  0, /* uPredNum */
											  bSkipInv,
											  psDestReg, 
											  psChanReg, 
											  &sImdVal))
					{
						USP_DBGPRINT(("UnpackTextureDataToF32: Error encoding data movement (AND) HW-inst\n"));
						return IMG_FALSE;
					}
					*peOpcode = USP_OPCODE_AND;
				}
				#else
			    if  (!HWInstEncodeMOVInst(psHWInst,
			                              1,
			                              bSkipInv,
			                              psDestReg,
			                              psChanReg))
			    {
				    USP_DBGPRINT(("UnpackTextureDataToS32: Error encoding MOV inst\n"));
				    return IMG_FALSE;
			    }

			    *peOpcode        = USP_OPCODE_MOVC;
				#endif /* defined(SGX_FEATURE_USE_VEC34) */

			    *puInstDescFlags = uInstDescFlags;
			    bFirstUnpackInst = IMG_FALSE;
			    uHWInstCount++;
			    break;
		    }
		    default:
		    {
			    USP_DBGPRINT(("UnpackTextureDataToS32: Invalid destination channel format.\n"));
			    return IMG_FALSE;
		    }
		}

		if  (!bFirstUnpackInst)
		{
			if  (psDestReg->eType == USP_REGTYPE_INTERNAL)
			{
				uInstDescFlags |= USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
			}
		}
	}

	/*
		Record the number of instructions generated to unpack the texture
	*/
	psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;
	
	return IMG_TRUE;
#endif /* defined(SGX_FEATURE_USE_VEC34) */
}

static IMG_BOOL RemapInteger16Channel(PUSP_SAMPLE		psSample, 
									  PUSP_CONTEXT		psContext, 
									  PHW_INST			psHWInst, 
									  IMG_BOOL			bSkipInv, 
									  PUSP_OPCODE		peOpcode, 
									  IMG_PUINT32		puInstDescFlags, 
									  IMG_UINT32		uInstDescFlags, 
									  IMG_PUINT32		puHwInstCount, 
									  IMG_UINT32		uChanIdx, 
									  USP_PCKUNPCK_FMT	ePCKUNPCKSrcFmt,
									  IMG_PUINT32		puChunksRemapped, 
									  IMG_PBOOL			pbRemappidDone)
/******************************************************************************
 Name:		RemapInteger16Channel

 Purpose:	Remaps the U16 sample results to the F16 temporary register for 
			this channel. To make it effecient it also remaps all the channels
			that belong to the chunk. Where it first finds chunk for the given 
			channel.

 Inputs:	psSample		- The sample to remap U16 results for
			psContext		- The current USP execution context
			psHWInst		- The location where to put generated hardware 
							  instructions
			bSkipInv		- Whether the SkipInv flag should be set for the 
							  generated hardware instructions
			peOpcode		- Memory location where to set instruction OP 
							  codes
			puInstDescFlags	- Memory location where to set instruction 
							  description flags
			uInstDescFlags	- Instruction desription flag to set into new 
							  instructions
			uChanIdx		- The channel to remap results for
			ePCKUNPCKSrcFmt - The source format U16/S16
			puChunksRemapped- The bit mask that tels which chuncks are 
							  remapped already. On return it sets the 
							  corresponding bit to 1 for the chunk that is 
							  just remapped.

 Outputs:	puHwInstCount	- Total number of hardware instructions written
							  for this result remap
			puChunksRemapped- The bit mask that tels which chuncks are 
							  remapped already. On return it sets the 
							  corresponding bit to 1 for the chunk that is 
							  just remapped.
			pbRemappidDone	- Set to IMG_TRUE if remapping is done on this 
							  call IMG_FALSE otherwise

 Returns:	IMG_FALSE on error
*****************************************************************************/
{
	PUSP_TEX_CHAN_INFO	psTexChanInfo = &psSample->sTexChanInfo;
	IMG_UINT32			uHWInstCount = 0;
	IMG_UINT32			uRegIdx;
	IMG_UINT32			uChunkIdx;
	USP_REG				sSrcReg1;
	USP_REG				sSrcReg2;
	USP_REG				sDstReg;
	USP_REGTYPE			eNewDestRegType;
	IMG_UINT32			uNewDestRegNum;
	IMG_UINT32			uWtiteMask;
	USP_CHAN_SWIZZLE	eChanSwizzle;

	/*
		Get the chunk index for this channel. 
	*/
	eChanSwizzle = psTexChanInfo->aeTexChanForComp[uChanIdx];
	if (eChanSwizzle > USP_CHAN_SWIZZLE_CHAN_3)
	{
		return IMG_TRUE;
	}
	uChunkIdx = psTexChanInfo->auChunkIdxForChan[eChanSwizzle];

	if((*puChunksRemapped) & (1<<uChunkIdx))
	{
		/* 
			This chunk is remapped already and no remapping is done this time
		*/
		*pbRemappidDone = IMG_FALSE;
		return IMG_TRUE;
	}

	/*
		Save the first new register that will now be used for this chunk
	*/
	eNewDestRegType = psSample->psSampleUnpack->eSampleUnpackTempRegType;
	uNewDestRegNum = psSample->psSampleUnpack->uSampleUnpackTempRegNum + psSample->psSampleUnpack->uSampleUnpackTempRegCount;

	/*
		Some chunks might be writing multiple registers
	*/
	for(uRegIdx=0; uRegIdx<psTexChanInfo->auNumRegsForChunk[uChunkIdx]; uRegIdx++)
	{
		sDstReg.eType = psSample->psSampleUnpack->eSampleUnpackTempRegType;
		sDstReg.eDynIdx = USP_DYNIDX_NONE;
		sDstReg.eFmt	= USP_REGFMT_F16;
		sDstReg.uNum = psSample->psSampleUnpack->uSampleUnpackTempRegNum + psSample->psSampleUnpack->uSampleUnpackTempRegCount;
		sDstReg.uComp = 0;

		sSrcReg1.eType = psSample->asChunkReg[uChunkIdx].eType;
		sSrcReg1.eDynIdx = psSample->asChunkReg[uChunkIdx].eDynIdx;
		sSrcReg1.eFmt = psSample->asChunkReg[uChunkIdx].eFmt;
		sSrcReg1.uNum = psSample->asChunkReg[uChunkIdx].uNum + uRegIdx;
		sSrcReg1.uComp = 0;

		sSrcReg2 = sSrcReg1;
		
		sSrcReg2.uComp = 2;

		#if defined(SGX_FEATURE_USE_VEC34)
		uWtiteMask = 0x3;
		#else
		uWtiteMask = 0xF;
		#endif /* defined(SGX_FEATURE_USE_VEC34) */

		if	(!HWInstEncodePCKUNPCKInst(psHWInst,
									   bSkipInv, 
									   USP_PCKUNPCK_FMT_F16, 
									   ePCKUNPCKSrcFmt, 
									   IMG_TRUE, 
									   uWtiteMask, 
									   &sDstReg, 
									   0, 
									   1, 
									   &sSrcReg1, 
									   &sSrcReg2))
		{
			USP_DBGPRINT(("RemapInteger16Channel: Error encoding PCKUNPCK inst\n"));
			return IMG_FALSE;
		}

		*peOpcode = USP_OPCODE_PCKUNPCK;

		/*
			Now that this unpack instruction writes to a intermediate register, 
			it can never reference a result-register.
		*/
		*puInstDescFlags = uInstDescFlags & (~USP_INSTDESC_FLAG_DEST_USES_RESULT);

		/* 
			Update the unpack instruction count 
		*/
		uHWInstCount++;
		*puHwInstCount = uHWInstCount;

		/* 
			One temporary is used for result remapping 
		*/
		psSample->psSampleUnpack->uSampleUnpackTempRegCount++;

		/*
			Move to memory reserved for next instruction
		*/
		psHWInst++;

		/*
			Move to next memory location reserved for next instruction
		*/
		peOpcode++;
		puInstDescFlags++;
	}
	
	/* 
		Set that the results for this chunk are remapped 
	*/
	(*puChunksRemapped) |= (1<<uChunkIdx);

	/*
		Update the format of all channels that forms this chunk
	*/
	UpdateSampleChunkFormat(psSample, uChunkIdx, USP_CHAN_FORMAT_F16);

	/*
		Update the chunk register
	*/
	psSample->asChunkReg[uChunkIdx].eType = eNewDestRegType;
	psSample->asChunkReg[uChunkIdx].uNum = uNewDestRegNum;

	/*
		Now that we have relocated some of the sample-data, update
		the source register for each destination channel.
	*/
	if	(!SetupChanSrcRegs(psSample, psContext))
	{
		USP_DBGPRINT(("RemapInteger16Channel: Error updating src regs for each channel\n"));
		return IMG_FALSE;
	}

	/*
		Set that the remapping is done on this call
	*/
	*pbRemappidDone = IMG_TRUE;

	return IMG_TRUE;
}

/******************************************************************************
 Name:		UnpackTextureDataToU8C10

 Purpose:	Unpack sampled texture data to U8 results

 Inputs:	psSample	- The dependent sample to generate code for
			psContext	- The current USP execution context
			eDestFmt	- Destination format to unpack to.

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL UnpackTextureDataToU8C10(PUSP_SAMPLE	psSample,
										 PUSP_CONTEXT	psContext,
										 USP_REGFMT		eDestFmt)
{
	static const USP_PCKUNPCK_FMT aePCKUNPCKFmtForChanFmt[] =
	{
		USP_PCKUNPCK_FMT_U8,	/* USP_CHAN_FORMAT_U8 */
		USP_PCKUNPCK_FMT_S8,	/* USP_CHAN_FORMAT_S8 */
		USP_PCKUNPCK_FMT_U16,	/* USP_CHAN_FORMAT_U16 */
		USP_PCKUNPCK_FMT_S16,	/* USP_CHAN_FORMAT_S16 */
		USP_PCKUNPCK_FMT_F16,	/* USP_CHAN_FORMAT_F16 */
		USP_PCKUNPCK_FMT_F32,	/* USP_CHAN_FORMAT_F32 */
		USP_PCKUNPCK_FMT_INVALID/* USP_CHAN_FORMAT_U24 */
	};
	static const USP_REG sHWConstZero = 
	{
		USP_REGTYPE_SPECIAL,
		HW_CONST_ZERO,
		USP_REGFMT_U8,
		0,
		USP_DYNIDX_NONE
	};

	PUSP_MOESTATE	psMOEState;
	IMG_UINT32		uInstDescFlags;
	IMG_BOOL		bSkipInv;
	PHW_INST		psHWInst;
	PUSP_OPCODE		peOpcode;
	IMG_PUINT32		puInstDescFlags;
	USP_OPCODE		ePrevInstOpcode;
	IMG_UINT32		uChanIdx = 0;
	IMG_UINT32		uHWInstCount;
	PHW_INST		psPrevHWInst;
	PUSP_REG		psPrevDestReg;
	PUSP_REG		psPrevChanReg;
	IMG_UINT32		uChansSampledDirect;
	IMG_BOOL		bFirstUnpackInst;
	IMG_BOOL		bPrevFirstUnpackInst;
	IMG_UINT32		uChansToConvertToC10;
	PUSP_REG		psDestForConvertToC10;
	IMG_BOOL		bChanChunkNonDep = IMG_FALSE;
	IMG_UINT32		uChansChunkIdx = 0;
	IMG_UINT32		uChunksRemappedMask;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	#if defined(SGX_FEATURE_USE_VEC34)
	if(CanUseVectorUnpack(psSample, psContext, eDestFmt))
	{
		return IMG_TRUE;
	}
	#endif /* defined(SGX_FEATURE_USE_VEC34) */

	/*
		Check whether the data can be sampled directly to the destination
	*/
	uChansSampledDirect = 0;
	psPrevDestReg		= IMG_NULL;
	psPrevChanReg		= IMG_NULL;

	if	(psSample->psSampleUnpack->bCanSampleDirectToDest)
	{
		if(
				#if defined(SGX545)
				!CanUseDestDirectForUnpackedTexture(psSample, psContext, &uChansSampledDirect) &&
				#endif /* defined(SGX545) */
				eDestFmt == USP_REGFMT_U8
		  )
		{	
			for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
			{
				USP_CHAN_FORMAT	eDataFmtForChan;
				IMG_UINT32		uChunkIdx;
				PUSP_REG		psChanReg;
				PUSP_REG		psDestReg;
				USP_REG			sFinalDestReg;

				/*
					SMP/FIRH does not support partial writes, so all live channels must
					be replaced

					NB: If a destination channel isn't live, then it's okay overwrite it.
					
						If we are fixing partial writes, and the sample is performing the
						first write of a shader result register, then it's also OK to sample
						all channels whether they are needed or not			
				*/
				if	(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
				{			
					{
						if	(psSample->psSampleUnpack->uLive & (1 << uChanIdx))
						{
							/*
								Cannot sample any channels direct to the destination
							*/
							break;
						}
						continue;
					}
				}

				/*
					All sampled channels must be U8 format (since the destination
					is expecting U8 data).
				*/
				eDataFmtForChan	= GetSampleChannelFormat(psSample, uChanIdx);
				if	(eDataFmtForChan != USP_CHAN_FORMAT_U8)
				{
					continue;	
				}

				/*
					Components within the sampled data cannot be needed for
					anything other than the corresponding component of the
					destination.
				*/
				psChanReg = &psSample->asChanReg[uChanIdx];
				psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

				if	(psDestReg->uComp != psChanReg->uComp)
				{
					continue;
				}

				/*
					The same destination and source registers must be used for
					all sampled channels.
				*/
				if	(psPrevDestReg && psPrevChanReg)
				{
					if	(
							(psPrevDestReg->uNum	!= psDestReg->uNum)  ||
							(psPrevDestReg->eType	!= psDestReg->eType) ||
							(psPrevDestReg->eFmt	!= psDestReg->eFmt)	 ||
							(psPrevChanReg->eType	!= psChanReg->eType) ||
							(psPrevChanReg->uNum	!= psChanReg->uNum)
						)
					{
						continue;
					}
				}

				{
					USP_CHAN_SWIZZLE	eTexChanSwizzle;			
					eTexChanSwizzle		= (psSample->sTexChanInfo).aeTexChanForComp[uChanIdx];
					if (eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
					{
						uChunkIdx			= (psSample->sTexChanInfo).auChunkIdxForChan[eTexChanSwizzle];
						uChansChunkIdx		= uChunkIdx;

						/* 
							Get either this channel is sampled as Dependent or 
							Non Dependent. 
						*/
						if	(psSample->bNonDependent)
						{			
							bChanChunkNonDep	= (((psSample->sTexChanInfo).uTexNonDepChunkMask & (1 << uChunkIdx)) ? IMG_TRUE : IMG_FALSE);				
						}
					}
				}

				/*
					If the result of this sample forms part of the overall shader result,
					account for the required result-remapping.

					NB: This is not needed for non-dependent samples, since they will be
						patched correctly as part of result-remapping. For non-dependent
						samples that don't need any unpacking code, there would be no
						instructions to 'patch', so we muct account for the final
						destination here.
				*/
				sFinalDestReg = *psDestReg;

				if	(psSample->psSampleUnpack->uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT)
				{
					PUSP_SHADER		psShader;
					PUSP_PROGDESC	psProgDesc;
					USP_REGTYPE		eFinalResultRegType;
					IMG_UINT32		uFinalResultRegNum;
					IMG_UINT32		uOrgResultRegNum;

					/*
						Remap the destination to the final result register
					*/
					psShader	= psSample->psShader;
					psProgDesc	= psShader->psProgDesc;

					eFinalResultRegType	= psShader->eFinalResultRegType;
					uFinalResultRegNum	= psShader->uFinalResultRegNum;
					uOrgResultRegNum	= psProgDesc->uDefaultPSResultRegNum;

					sFinalDestReg.eType	= eFinalResultRegType;
					sFinalDestReg.uNum	= (psDestReg->uNum - uOrgResultRegNum) + 
										  uFinalResultRegNum;
				}

				if	(bChanChunkNonDep == IMG_TRUE)
				{
					/*
						For non-dependent samples, the required destination must match
						the location of the pre-sampled data. For dependent-samples
						it must only be a supported register-type.
					*/
					if	(
							(sFinalDestReg.eType != psChanReg->eType) ||
							(sFinalDestReg.uNum  != psChanReg->uNum)
						)
					{
						continue;
					}
				}
				else
				{
					PUSP_TEX_CHAN_INFO    psTexChanInfo;
					PUSP_TEX_CHAN_FORMAT  psTexFormat;
					USP_CHAN_SWIZZLE      eTexChanSwizzle;
					USP_CHAN_CONTENT      eChanContent;
					PHW_INST              psSampleInst;
					IMG_UINT32            uSampleInstIdx;
					USP_OPCODE            eSampleInstOpcode;
					HW_INST               sTempSampleInst;

					/*
						Cannot sample directly to the dest if we didn't sample the
						texture for this channel
					*/
					psTexFormat		= &psSample->sSamplerDesc.sTexChanFormat;
					psTexChanInfo	= &psSample->sTexChanInfo;
					eTexChanSwizzle = psTexChanInfo->aeTexChanForComp[uChanIdx];

					if	(
							(eTexChanSwizzle < USP_CHAN_SWIZZLE_CHAN_0) ||
							(eTexChanSwizzle > USP_CHAN_SWIZZLE_CHAN_3)
						)
					{
						continue;
					}

					eChanContent = psTexFormat->aeChanContent[eTexChanSwizzle];
					if	(eChanContent == USP_CHAN_CONTENT_NONE)
					{
						continue;
					}

					/*
						Lookup details of the sample instruction used to read
						this chunk
					*/
					psSampleInst		= psSample->apsSMPInsts[uChanIdx];
					uSampleInstIdx		= psSampleInst - psSample->psSampleUnpack->asSampleInsts;
					eSampleInstOpcode	= psSample->psSampleUnpack->aeOpcodeForSampleInsts[uSampleInstIdx];

					/*
						Is the final destination register-type one that is supported
						by the SMP instruction?
					*/
					sTempSampleInst = *psSampleInst;

					if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
													 eSampleInstOpcode,
													 &sTempSampleInst,
													 &sFinalDestReg))
					{
						continue;
					}

					/*
						If we generated multiple sample instructions, ensure that none of
						them are using the destination register as a source
					*/
					if	(psSample->uNumSMPInsts > 1)
					{
						IMG_UINT32	uNextChanIdx;
						
						for (uNextChanIdx = uChanIdx + 1; uNextChanIdx < USP_MAX_SAMPLE_CHANS; uNextChanIdx++)
						{
							PHW_INST	psNextSampleInst;
							IMG_BOOL	bSMPReadsDest;

							/*
								Check whether the destination register may be used
								by one of the sources for the sample instructions.

								NB:	All SMP/FIRH instructions generated for a sample will be
									identical with respect to the sources they use (apart from
									the state source for SMP), so we only need to check for
									clashes with one inst here.
							*/
							psNextSampleInst = psSample->apsSMPInsts[uNextChanIdx];
							if	(!psNextSampleInst)
							{
								continue;
							}

							if	(!SampleInstReadsReg(psSample,
													 psNextSampleInst,
													 &sFinalDestReg,
													 psContext,
													 &bSMPReadsDest))
							{
								USP_DBGPRINT(("UnpackTextureDataToU8C10: Error checking for SMP inst that references dest\n"));
								return IMG_FALSE;
							}

							if	(bSMPReadsDest)
							{
								break;
							}
						}

						/*
							Cannot sample this channel directly to the destination as it would
							overwrite data needed for other sampling insts
						*/
						if	(uNextChanIdx < USP_MAX_SAMPLE_CHANS)
						{
							continue;
						}
					}
				}

				psPrevDestReg = psDestReg;
				psPrevChanReg = psChanReg;

				/*
					Flag that we can sample this channel directly to the destination
				*/
				uChansSampledDirect |= 1 << uChanIdx;
			}

			/*
				Cannot sample any channels direct to the dest if it would overwrite existing data	
			*/
			if	(uChanIdx < USP_MAX_SAMPLE_CHANS)
			{
				uChansSampledDirect = 0;	
			}		
		}
	}

	/*
		Update sampling instructions for channels that can be sampled directly
		to the destination

		NB: This code relies on the fact that the sampled data for all channels
			come from one chunk of data - even though there may be multiple
			sampling instructions 'fetching' that chunk (in the case of CSC)
	*/
	if	(uChansSampledDirect)
	{
		if((!psSample->bNonDependent) || ((psSample->bNonDependent) && (!((psSample->sTexChanInfo.uTexNonDepChunkMask) & (1<<uChansChunkIdx)))))
		{
			PUSP_TEX_CHAN_INFO	psTexChanInfo;
			IMG_UINT32			uChansUsingSampledData;

			/*
				Modify the generated sample instructions...

				NB: All channels that can be sampled direct to the destination must
					have the same destination register, so we can set the same dest
					for all contributing sample instructions here.
			*/
			psTexChanInfo			= &psSample->sTexChanInfo;
			uChansUsingSampledData	= 0;

			for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
			{
				PHW_INST	psSampleInst;
				IMG_UINT32	uInstDescFlags;
				IMG_UINT32	uSampleInstIdx;
				USP_OPCODE	eSampleInstOpcode;
				IMG_UINT32	uChunkIdx;

				/*
					Ignore unused channels
				*/
				if	(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
				{
					continue;
				}

				/*
					Record if this channel is using sampled data, so we can tell
					later if all channels using sampled data are sampling directly
					to the destination, and therefore we don't need any temporary
					registers for sampling anymore.
				*/
				psSampleInst = psSample->apsSMPInsts[uChanIdx];
				if	(psSampleInst)
				{
					uChansUsingSampledData |= 1 << uChanIdx;
				}

				/*
					Don't modify sample instructions for channels we are not sampling directly
				*/
				if	(!(uChansSampledDirect & (1 << uChanIdx)))
				{
					continue;
				}

				/*
					Update the sample instruction to write the sampled data directly
					into the destination register.
				*/
				psSampleInst		= psSample->apsSMPInsts[uChanIdx];
				uSampleInstIdx		= psSampleInst - psSample->psSampleUnpack->asSampleInsts;
				eSampleInstOpcode	= psSample->psSampleUnpack->aeOpcodeForSampleInsts[uSampleInstIdx];

				if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
												 eSampleInstOpcode,
												 psSampleInst,
												 psPrevDestReg))
				{
					USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding dest for SMP inst\n"));
					return IMG_FALSE;
				}

				/*
					Record the new location of the data for this destination channel (assuming
					all channels are using the same chunk of sampled data)
				*/
				uChunkIdx = psTexChanInfo->auChunkIdxForChan[uChanIdx];

				psSample->asChunkReg[uChunkIdx] = *psPrevDestReg; 

				/*
					Now that this sample instruction writes directly to the intended
					destination, rather than an intermediate register, it may
					reference a result-register.
				*/
				uInstDescFlags = psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx];

				uInstDescFlags |= psSample->psSampleUnpack->uInstDescFlags & 
								  (USP_INSTDESC_FLAG_DEST_USES_RESULT);				

				psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx] = uInstDescFlags;
			}

			/*
				If all channels are sampled directly then we don't need any temp dests for
				the sampling code
			*/
			if	(uChansSampledDirect == uChansUsingSampledData)
			{	
				//psSample->uNumTempRegsUsed -= psSample->uSampleTempCount;
				if(psSample->psSampleUnpack->puBaseSampleDestRegCount != psSample->psSampleUnpack->puBaseSampleTempDestRegCount)
				{
					(*(psSample->psSampleUnpack->puBaseSampleDestRegCount))-=(*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount));
				}
				(*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount)) = 0;

				#if defined(SGX_FEATURE_USE_VEC34)
				psSample->uUnAlignedTempCount = 0;
				#endif /* defined(SGX_FEATURE_USE_VEC34) */
			}
		}

		/*
			Update the source location of the sampled data
		*/
		if	(!SetupChanSrcRegs(psSample, psContext))
		{
			USP_DBGPRINT(("UnpackTextureDataToU8C10: Error updating src regs for each channel\n"));
			return IMG_FALSE;
		}
	}

	/*
		Check for non-dependent reads that need to be unpacked from a temporary
		location
	*/
	if	(psSample->bNonDependent)
	{
		USP_OPCODE	eInstOpcode;
		IMG_UINT32	uIdentityCopyMask;
		IMG_UINT32	uInstWriteMask;
		IMG_UINT32	uPrevInstWriteMask;
		IMG_UINT32	uPrevNumUnpackInsts;
		IMG_UINT32	uNumUnpackInsts;

		psPrevDestReg		= IMG_NULL;
		psPrevChanReg		= IMG_NULL;
		ePrevInstOpcode		= USP_OPCODE_INVALID;
		uPrevInstWriteMask	= 0;
		uIdentityCopyMask	= 0;
		eInstOpcode			= USP_OPCODE_INVALID;
		uInstWriteMask		= 0;
		uNumUnpackInsts		= 0;
		uPrevNumUnpackInsts	= 0;
		uChanIdx			= 0;

		while (uChanIdx < USP_MAX_SAMPLE_CHANS)
		{
			USP_CHAN_FORMAT	eDataFmtForChan;
			PUSP_REG		psChanReg;
			PUSP_REG		psDestReg;
			IMG_BOOL		bChunksMoved;

			/*
				Skip this channel if we didn't need to sample it
			*/
			if	(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
			{
				uChanIdx++;
				continue;
			}

			/* 
				Get either this channel has been sampled as Dependent 
				or Non Dependent. 
			*/			
			{
				USP_CHAN_SWIZZLE	eTexChanSwizzle;				
				eTexChanSwizzle		= (psSample->sTexChanInfo).aeTexChanForComp[uChanIdx];
				if (eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
				{
					IMG_UINT32			uChunkIdx;

					uChunkIdx			= (psSample->sTexChanInfo).auChunkIdxForChan[eTexChanSwizzle];
					if(!(((psSample->sTexChanInfo).uTexNonDepChunkMask & (1 << uChunkIdx))))
					{
						uChanIdx++;
						continue;
					}
				}
			}

			/*
				Get the source and destination registers to use for this channel
			*/
			psChanReg = &psSample->asChanReg[uChanIdx];
			psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

			/*
				Detemine whether this channel can be unpacked/copied together
				with previous channels
			*/
			eDataFmtForChan	= GetSampleChannelFormat(psSample, uChanIdx);
			switch (eDataFmtForChan)
			{
				case USP_CHAN_FORMAT_U8:
				{
					IMG_BOOL bCanMergeInst;

					/*
						Can we perform a direct component-to-component copy,
						and haven't previously added a PCKUNPCK (which could
						also handle this component)?
					*/
					if	(
							(psDestReg->uComp == psChanReg->uComp) &&
							(ePrevInstOpcode != USP_OPCODE_PCKUNPCK)
						)
					{
						IMG_BOOL bSeperateAlphaCopy;
	
						/*
							Check whether we can copy this sampled channel along
							with the preceeding one
						*/
						bCanMergeInst		= IMG_FALSE;
						bSeperateAlphaCopy	= IMG_FALSE;

						if	(ePrevInstOpcode == USP_OPCODE_SOPWM)
						{
							if	(
									(psPrevDestReg->uNum	== psDestReg->uNum)  &&
									(psPrevDestReg->eType	== psDestReg->eType) &&
									(psPrevDestReg->eFmt	== psDestReg->eFmt)
								)
							{
								if	(
										(psPrevChanReg->eType	== psChanReg->eType) &&
										(psPrevChanReg->uNum	== psChanReg->uNum)	 &&
										(psPrevChanReg->eFmt	== psChanReg->eFmt)
									)
								{
									bCanMergeInst = IMG_TRUE;
								}
								else
								{
									if	((psDestReg->uComp == 3) && (uPrevInstWriteMask == 0x7))
									{
										bCanMergeInst		= IMG_TRUE;	
										bSeperateAlphaCopy	= IMG_TRUE;
									}
								}
							}
						}

						if	(bCanMergeInst)
						{
							if	(bSeperateAlphaCopy)
							{
								/*
									We'll change the previous SOPWM into a SOP2 that
									writes all components. No future instructions can
									be combined with it.
								*/
								uPrevInstWriteMask |= 1 << uChanIdx;
								ePrevInstOpcode	= USP_OPCODE_INVALID;
							}
							else
							{
								/*
									We will copy this channel alpng with the preceeding ones.
								*/
								uPrevInstWriteMask |= 1 << uChanIdx;
							}
						}
						else
						{
							/*
								We will generate a (new) SOPWM instruction to
								copy this channel.
							*/
							eInstOpcode		= USP_OPCODE_SOPWM;
							uInstWriteMask	= 1 << uChanIdx;

							uNumUnpackInsts++;
						}
					}
					else
					{
						/*
							Can we can copy/convert this channel using the
							preceeding instruction
						*/
						bCanMergeInst = IMG_FALSE;
						if	(ePrevInstOpcode == USP_OPCODE_PCKUNPCK)
						{
							if	(
									(psPrevDestReg->uNum	== psDestReg->uNum)  &&
									(psPrevDestReg->eType	== psDestReg->eType) &&
									(psPrevDestReg->eFmt	== psDestReg->eFmt)	 &&
									(psPrevChanReg->eFmt	== psChanReg->eFmt)
									#if defined(SGX_FEATURE_USE_VEC34)
									&& CanUnpackTwoChannels(psDestReg->eFmt, psChanReg, eDataFmtForChan) 
									&& (psPrevChanReg->eType == psChanReg->eType)
									#endif /* defined(SGX_FEATURE_USE_VEC34) */
								)
							{
								bCanMergeInst = IMG_TRUE;
							}
						}

						if	(ePrevInstOpcode == USP_OPCODE_SOPWM)
						{
							static const IMG_UINT32 auMaskSetBitCount[] =
							{
								0, 1, 1, 2,
								1, 2, 2, 3,
								1, 2, 2, 3,
								2, 3, 3, 4
							};

							if	(auMaskSetBitCount[uPrevInstWriteMask] == 1)
							{
								if	(
										(psPrevDestReg->uNum	== psDestReg->uNum)  &&
										(psPrevDestReg->eType	== psDestReg->eType) &&
										(psPrevDestReg->eFmt	== psDestReg->eFmt)	 &&
										(psPrevChanReg->eFmt	== psChanReg->eFmt)
										#if defined(SGX_FEATURE_USE_VEC34)
										&& CanUnpackTwoChannels(psDestReg->eFmt, psChanReg, eDataFmtForChan)
										#endif /* defined(SGX_FEATURE_USE_VEC34) */
									)
								{
									bCanMergeInst = IMG_TRUE;
								}
							}
						}

						if	(bCanMergeInst)
						{
							/*
								We'll change the previous SOPWM to a PCKUPCK that
								handles both this and the previous coponent. 
							*/
							ePrevInstOpcode		= USP_OPCODE_INVALID;
							uPrevInstWriteMask	= (1 << psPrevDestReg->uComp) |
												  (1 << psDestReg->uComp);
						}
						else
						{
							/*
								We'll add a (new) PCKUPCK to handle this component
							*/
							uInstWriteMask	= 1 << psDestReg->uComp,
							eInstOpcode		= USP_OPCODE_PCKUNPCK;

							uNumUnpackInsts++;
						}
					}

					/*
						Record which channels we have done an identity copy on, so we
						can ignore them when checking for ovewriting data
					*/
					if	(psDestReg->uComp == psChanReg->uComp)
					{
						uIdentityCopyMask |= 1 << psDestReg->uComp;
					}

					break;
				}

				case USP_CHAN_FORMAT_S8:
				case USP_CHAN_FORMAT_U16:
				case USP_CHAN_FORMAT_S16:
				case USP_CHAN_FORMAT_F16:
				case USP_CHAN_FORMAT_F32:
				{
					IMG_BOOL bCanMergeInst;

					/*
						Check whether we can unpack this channel with the
						preceeding one
					*/
					bCanMergeInst = IMG_FALSE;
					if	(ePrevInstOpcode == USP_OPCODE_PCKUNPCK)
					{
						if	(
								(psPrevDestReg->uNum	== psDestReg->uNum)  &&
								(psPrevDestReg->eType	== psDestReg->eType) &&
								(psPrevDestReg->eFmt	== psDestReg->eFmt)	 &&
								(psPrevChanReg->eFmt	== psChanReg->eFmt)
								#if defined(SGX_FEATURE_USE_VEC34)
								&& (psPrevChanReg->eType == psChanReg->eType) 
								&& CanUnpackTwoChannels(psDestReg->eFmt, psChanReg, eDataFmtForChan)
								#endif /* defined(SGX_FEATURE_USE_VEC34) */
							)
						{
							bCanMergeInst = IMG_TRUE;
						}
					}

					if	(bCanMergeInst)
					{
						/*
							We'll change the previous SOPWM to a PCKUPCK that
							handles both this and the previous coponent.
						*/
						ePrevInstOpcode		= USP_OPCODE_INVALID;
						uPrevInstWriteMask	= (1 << psPrevDestReg->uComp) |
											  (1 << psDestReg->uComp);
					}
					else
					{
						/*
							We'll add a (new) PCKUPCK to handle this component
						*/
						uInstWriteMask	= 1 << psDestReg->uComp,
						eInstOpcode		= USP_OPCODE_PCKUNPCK;

						uNumUnpackInsts++;
					}

					break;
				}

				case USP_CHAN_FORMAT_U24:
				{
					USP_DBGPRINT(("UnpackTextureDataToU8C10: U24 source data not yet supported\n"));
					return IMG_FALSE;
				}

				default:
				{
					USP_DBGPRINT(("UnpackTextureDataToU8C10: Unhandled source data format\n"));
					return IMG_FALSE;
				}
			}

			/*
				Check whether the last unpacking instruction potentially
				overwrites source data needed for the unpacking of later
				channels.
			*/
			bChunksMoved = IMG_FALSE;

			if	(uNumUnpackInsts != uPrevNumUnpackInsts)
			{
				if	(uNumUnpackInsts > 1)
				{
					USP_REG	sFinalDestReg;

					/*
						Is this unpack instruction writing to a result-reg?
					*/
					if	(psSample->psSampleUnpack->uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT)
					{
						PUSP_SHADER		psShader;
						PUSP_PROGDESC	psProgDesc;
						USP_REGTYPE		eFinalResultRegType;
						IMG_UINT32		uFinalResultRegNum;
						IMG_UINT32		uOrgResultRegNum;

						/*
							Remap the destination to the final result register
						*/
						psShader	= psSample->psShader;
						psProgDesc	= psShader->psProgDesc;

						eFinalResultRegType	= psShader->eFinalResultRegType;
						uFinalResultRegNum	= psShader->uFinalResultRegNum;
						uOrgResultRegNum	= psProgDesc->uDefaultPSResultRegNum;

						sFinalDestReg		= *psPrevDestReg;
						sFinalDestReg.eType	= eFinalResultRegType;
						sFinalDestReg.uNum	= (psPrevDestReg->uNum - uOrgResultRegNum) +
											  uFinalResultRegNum;
						psPrevDestReg		= &sFinalDestReg;						
					}

					if	(!UnpackChunksFromTemp(psSample,
											   psPrevDestReg,
											   uPrevInstWriteMask & ~uIdentityCopyMask,
											   uChanIdx,
											   psContext,
											   &bChunksMoved))
					{
						USP_DBGPRINT(("UnpackTextureDataToU8C10: Error testing whether non-dep data should be unpacked from temps\n"));
						return IMG_FALSE;
					}
				}

				uPrevInstWriteMask	= uInstWriteMask;
				ePrevInstOpcode		= eInstOpcode;
				uPrevNumUnpackInsts	= uNumUnpackInsts;
			}

			/*
				Restart if we have altered the source data for any chunks
			*/
			if	(bChunksMoved)
			{
				ePrevInstOpcode		= USP_OPCODE_INVALID;
				uPrevInstWriteMask	= 0;
				eInstOpcode			= USP_OPCODE_INVALID;
				uInstWriteMask		= 0;
				uNumUnpackInsts		= 0;
				uPrevNumUnpackInsts	= 0;
				uChanIdx			= 0;
			}
			else
			{
				psPrevDestReg = psDestReg;
				psPrevChanReg = psChanReg;
				uChanIdx++;
			}
		}
	}

	/*
		Generate instructions to save the sampled data for each channel
		into the required final register locations
	*/
	psMOEState		= &psSample->psSampleUnpack->psInstBlock->sFinalMOEState;
	uInstDescFlags	= psSample->psSampleUnpack->uInstDescFlags;
#if 0
	 & ~(USP_INSTDESC_FLAG_SRC0_USES_RESULT |
												   USP_INSTDESC_FLAG_SRC1_USES_RESULT |
												   USP_INSTDESC_FLAG_SRC2_USES_RESULT);
	if	(!psSample->bNonDependent)
	{
		uInstDescFlags &= ~USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
	}
#endif

	bSkipInv		= (IMG_BOOL)(!(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV));
	uHWInstCount	= psSample->psSampleUnpack->uUnpackInstCount;

	ePrevInstOpcode			= USP_OPCODE_INVALID;
	psPrevDestReg			= IMG_NULL;
	psPrevChanReg			= IMG_NULL;
	psPrevHWInst			= IMG_NULL;
	bFirstUnpackInst		= IMG_TRUE;
	bPrevFirstUnpackInst	= IMG_FALSE;
	uChansToConvertToC10	= 0;
	psDestForConvertToC10	= IMG_NULL;

	/*
		Set that no chunk is remapped for U16 -> F16 remapping
	*/
	uChunksRemappedMask = 0;

	for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		USP_CHAN_FORMAT	eDataFmtForChan;
		IMG_BOOL		bWasFirstUnpackInst;
		PUSP_REG		psChanReg;
		PUSP_REG		psDestReg;

		/*
			Skip this channel if we didn't need to sample it or it has been sampled
			directly to the destination already
		*/
		if	(!((psSample->psSampleUnpack->uMask ^ uChansSampledDirect) & (1 << uChanIdx)))
		{
			continue;
		}

		/*
			Note whether the instructions for this channel are forming the
			first unpacking instructions
		*/
		bWasFirstUnpackInst = bFirstUnpackInst;

		/*
			Get the source and destination registers to use for this channel
		*/
		psChanReg = &psSample->asChanReg[uChanIdx];
		psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

		/*
			Generate instructions to copy and convert the data for this
			channel
		*/
		psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
		peOpcode		= &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
		puInstDescFlags	= &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

		eDataFmtForChan	= GetSampleChannelFormat(psSample, uChanIdx);
		switch (eDataFmtForChan)
		{
			case USP_CHAN_FORMAT_U8:
			{
				IMG_BOOL bCanMergeInst;

				/*
					Are we performing a direct component-to-component copy,
					and haven't previously added a PCKUNPCK (which could
					also handle this component)?
				*/
				if	(
						(psDestReg->uComp == psChanReg->uComp) &&
						(ePrevInstOpcode != USP_OPCODE_PCKUNPCK)
					)
				{
					IMG_BOOL bSeperateAlphaCopy;

					/*
						Check whether we can copy this sampled channel along
						with the preceeding one
					*/
					bCanMergeInst		= IMG_FALSE;
					bSeperateAlphaCopy	= IMG_FALSE;

					if	(ePrevInstOpcode == USP_OPCODE_SOPWM)
					{
						if	(
								(psPrevDestReg->uNum	== psDestReg->uNum)  &&
								(psPrevDestReg->eType	== psDestReg->eType) &&
								(psPrevDestReg->eFmt	== psDestReg->eFmt)
							)
						{
							if	(
									(psPrevChanReg->eType	== psChanReg->eType) &&
									(psPrevChanReg->uNum	== psChanReg->uNum)	 &&
									(psPrevChanReg->eFmt	== psChanReg->eFmt)
								)
							{
								bCanMergeInst = IMG_TRUE;
							}
							else
							{
								IMG_UINT32	uWriteMask;

								uWriteMask = HWInstDecodeSOPWMInstWriteMask(psPrevHWInst);
								if	((psDestReg->uComp == 3) && (uWriteMask == 0x7))
								{
									bCanMergeInst		= IMG_TRUE;	
									bSeperateAlphaCopy	= IMG_TRUE;
								}
							}
						}
					}

					if	(bCanMergeInst)
					{
						IMG_UINT32	uWriteMask;

						/*
							Change the previous SOPWM into a SOP2, so that 4th component
							can be copied from a differentregister.
						*/
						if	(bSeperateAlphaCopy)
						{
							if	(!HWInstEncodeSOP2Inst(psMOEState,
													   psPrevHWInst,
													   bSkipInv,
													   EURASIA_USE0_SOP2_COP_ADD,
													   EURASIA_USE1_SOP2_CMOD1_COMPLEMENT,
													   EURASIA_USE1_SOP2_CSEL1_ZERO,
													   EURASIA_USE1_SOP2_CMOD2_NONE,
													   EURASIA_USE1_SOP2_CSEL2_ZERO,
													   EURASIA_USE0_SOP2_AOP_ADD,
													   EURASIA_USE1_SOP2_AMOD1_NONE,
													   EURASIA_USE1_SOP2_ASEL1_ZERO,
													   EURASIA_USE1_SOP2_AMOD2_COMPLEMENT,
													   EURASIA_USE1_SOP2_ASEL2_ZERO,
													   psDestReg,
													   psPrevChanReg,
													   psChanReg))
							{
								USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding SOP2 inst\n"));
								return IMG_FALSE;
							}

							peOpcode[-1] = USP_OPCODE_SOP2;

							/*
								No more components can be copied using this
								instruction
							*/
							ePrevInstOpcode = USP_OPCODE_INVALID;
							psPrevHWInst	= IMG_NULL;
						}
						else
						{
							/*
								Simply add this component to those copied by the
								previous SOPWM instruction.
							*/
							uWriteMask = HWInstDecodeSOPWMInstWriteMask(psPrevHWInst);
							uWriteMask |= 1 << psDestReg->uComp;

							/*
								Add this component to those copied previously
							*/
							if	(!HWInstEncodeSOPWMInstWriteMask(psPrevHWInst, uWriteMask))
							{
								USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding SOPWM write-mask\n"));
								return IMG_FALSE;
							}

							/*
								If we are copying all 4 components directly and not doing a
								format conversion, replace the SOPWM with a MOV, as it's more efficient
							*/
							if	(uWriteMask == 0xF && psDestReg->eFmt == psChanReg->eFmt)
							{
								#if defined(SGX_FEATURE_USE_VEC34)
								{
									USP_REG sImdVal;
									sImdVal.eType	= USP_REGTYPE_IMM;
									sImdVal.uNum	= 0xFFFFFFFF;
									sImdVal.eFmt	= psChanReg->eFmt;
									sImdVal.uComp	= psChanReg->uComp;
									sImdVal.eDynIdx = USP_DYNIDX_NONE;
									if	(!HWInstEncodeANDInst(psPrevHWInst, 
															  1,
															  IMG_FALSE, /* bUsePred */
															  IMG_FALSE, /* bNegatePred */
															  0, /* uPredNum */
															  bSkipInv,
															  psDestReg, 
															  psChanReg, 
															  &sImdVal))
									{
										USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding data movement (AND) HW-inst\n"));
										return IMG_FALSE;
									}
									peOpcode[-1] = USP_OPCODE_AND;
								}
								#else
								if	(!HWInstEncodeMOVInst(psPrevHWInst,
														  1,
														  bSkipInv,
														  psDestReg,
														  psChanReg))
								{
									USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding MOV inst\n"));
									return IMG_FALSE;
								}
								peOpcode[-1] = USP_OPCODE_MOVC;
								#endif /* defined(SGX_FEATURE_USE_VEC34) */

								/*
									No more components can be copied using this
									instruction
								*/
								ePrevInstOpcode = USP_OPCODE_INVALID;
								psPrevHWInst	= IMG_NULL;
							}
						}
					}
					else
					{
						/*
							If the dest is in C10 format, insert a MOE SETFC inst
							if needed before the SOPWM, so that we can use per-
							operand format selection.

							NB: The source data will never be C10, so we only need
								to consider the dest here.
						*/
						if	(psDestReg->eFmt == USP_REGFMT_C10)
						{
							if	(!psMOEState->bColFmtCtl)
							{
								if	(!HWInstEncodeSETFCInst(psHWInst,
															IMG_TRUE, 
															psMOEState->bEFOFmtCtl))
								{
									USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding SETFC inst\n"));
									return IMG_FALSE;
								}

								psMOEState->bColFmtCtl = IMG_TRUE;

								*peOpcode			= USP_OPCODE_SETFC;
								*puInstDescFlags	= uInstDescFlags & (USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE |
																		USP_INSTDESC_FLAG_DONT_SKIPINV);

								ePrevInstOpcode		= USP_OPCODE_SETFC;
								psPrevHWInst		= psHWInst;

								uHWInstCount++;
								psHWInst++;
								peOpcode++;
								puInstDescFlags++;
							}
						}

						/*
							Encode a SOPWM instruction to copy this 8-bit
							channel
						*/
						if	(!HWInstEncodeSOPWMInst(psMOEState,
													psHWInst,
													bSkipInv,
													EURASIA_USE1_SOP2WM_COP_ADD,
													EURASIA_USE1_SOP2WM_AOP_ADD,
													EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT,
													EURASIA_USE1_SOP2WM_SEL1_ZERO,
													EURASIA_USE1_SOP2WM_MOD2_NONE,
													EURASIA_USE1_SOP2WM_SEL2_ZERO,
													1 << psDestReg->uComp,
													psDestReg,
													psChanReg,
													(PUSP_REG)&sHWConstZero))
						{
							USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding SOPWM inst\n"));
							return IMG_FALSE;
						}

						*peOpcode = USP_OPCODE_SOPWM;

						*puInstDescFlags = uInstDescFlags;						

						uHWInstCount++;

						/*
							At most 3 more components can be copied by this SOPWM
						*/
						ePrevInstOpcode		= USP_OPCODE_SOPWM;
						psPrevHWInst		= psHWInst;
						bFirstUnpackInst	= IMG_FALSE;
					}
				}
				else
				{
					USP_PCKUNPCK_FMT	ePCKUNPCKDestFmt;

					/*
						Determine the appropriate dest format for PCKUNPCK

						NB: Currently, the same format should be specified for
							all components of the result.
					*/
					if	(psDestReg->eFmt == USP_REGFMT_U8)
					{
						ePCKUNPCKDestFmt = USP_PCKUNPCK_FMT_U8;
					}
					else
					{
						ePCKUNPCKDestFmt = USP_PCKUNPCK_FMT_C10;
					}

					/*
						Check whether we can copy/convert this channel
						using the preceeding instruction
					*/
					bCanMergeInst = IMG_FALSE;

					if	(ePrevInstOpcode == USP_OPCODE_PCKUNPCK)
					{
						if	(
								(psPrevDestReg->uNum	== psDestReg->uNum)  &&
								(psPrevDestReg->eType	== psDestReg->eType) &&
								(psPrevDestReg->eFmt	== psDestReg->eFmt)
								#if defined(SGX_FEATURE_USE_VEC34)
								&& (psPrevChanReg->eType == psChanReg->eType) 
								&& CanUnpackTwoChannels(psDestReg->eFmt, psChanReg, eDataFmtForChan)
								#endif /* defined(SGX_FEATURE_USE_VEC34) */
							)
						{
							USP_PCKUNPCK_FMT ePrevPCKUNPCKSrcFmt;

							ePrevPCKUNPCKSrcFmt = HWInstDecodePCKUNPCKInstSrcFormat(psPrevHWInst);
							if	(ePrevPCKUNPCKSrcFmt == USP_PCKUNPCK_FMT_U8)
							{
								bCanMergeInst = IMG_TRUE;
							}
						}
					}

					if	(ePrevInstOpcode == USP_OPCODE_SOPWM)
					{
						static const IMG_UINT32 auMaskSetBitCount[] =
						{
							0, 1, 1, 2,
							1, 2, 2, 3,
							1, 2, 2, 3,
							2, 3, 3, 4
						};
						IMG_UINT32	uWriteMask;

						uWriteMask = HWInstDecodeSOPWMInstWriteMask(psPrevHWInst);
						if	(auMaskSetBitCount[uWriteMask] == 1)
						{
							if	(
									(psPrevDestReg->uNum	== psDestReg->uNum)  &&
									(psPrevDestReg->eType	== psDestReg->eType) &&
									(psPrevDestReg->eFmt	== psDestReg->eFmt)	 &&
									(psPrevChanReg->eFmt	== psChanReg->eFmt)
									#if defined(SGX_FEATURE_USE_VEC34)
									&& CanUnpackTwoChannels(psDestReg->eFmt, psChanReg, eDataFmtForChan)
									#endif /* defined(SGX_FEATURE_USE_VEC34) */
								)
							{
								bCanMergeInst = IMG_TRUE;
							}
						}
					}

					if	(bCanMergeInst)
					{
						IMG_UINT32	uWriteMask;
						PUSP_REG	psSrc1Reg;
						PUSP_REG	psSrc2Reg;

						/*
							Replace the last instruction with one that copies
							both this and the preceeding component
						*/
						uWriteMask = (1 << psPrevDestReg->uComp) |
									 (1 << psDestReg->uComp);

						#if defined(SGX_FEATURE_USE_VEC34)
						psSrc1Reg = psPrevChanReg;
						psSrc2Reg = psChanReg;
						#else
						if	(psPrevDestReg->uComp < psDestReg->uComp)
						{
							psSrc1Reg = psPrevChanReg;
							psSrc2Reg = psChanReg;
						}
						else
						{
							psSrc1Reg = psChanReg;
							psSrc2Reg = psPrevChanReg;
						}
						#endif /* defined(SGX_FEATURE_USE_VEC34) */

						/*
							U8<->C10 is not supported by PCKUNPCK. To work around this,
							we pack/copy the components in U8 format (i.e. no conversion)
							to a temporary location, and add a SOPWM at the end to convert
							to C10 into the required destination.
						*/
						if	(ePCKUNPCKDestFmt == USP_PCKUNPCK_FMT_C10)
						{
							USP_REG	sTempDestReg;

							sTempDestReg.eType		= psSample->psSampleUnpack->eSampleUnpackTempRegType;
							sTempDestReg.uNum		= psSample->psSampleUnpack->uSampleUnpackTempRegNum + psSample->psSampleUnpack->uSampleUnpackTempRegCount;
							sTempDestReg.eDynIdx	= USP_DYNIDX_NONE;
							sTempDestReg.uComp		= 0;
							sTempDestReg.eFmt		= USP_REGFMT_U8;

							if	(!HWInstEncodePCKUNPCKInst(psPrevHWInst,
														   bSkipInv,
														   USP_PCKUNPCK_FMT_U8,
														   USP_PCKUNPCK_FMT_U8,
														   IMG_FALSE,
														   uWriteMask,
														   &sTempDestReg,
														   psPrevDestReg->uComp,
														   psDestReg->uComp,
														   psSrc1Reg,
														   psSrc2Reg))
							{
								USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding PCKUNPCK inst\n"));
								return IMG_FALSE;
							}

							uChansToConvertToC10 |= uWriteMask;
							if	(!psDestForConvertToC10)
							{
								psDestForConvertToC10 = psDestReg;
							}						

							/*
								If the instruction we have replaced was for the first
								unpacking code, then re-flag that we have yet to generate
								the first unpacking code, and restore the original state of
								the IREGS_LIVE_BEFORE instruction description flag (as this
								is set once we generate the first unpacking code).
							*/
							if	(bPrevFirstUnpackInst)
							{
								bFirstUnpackInst = IMG_TRUE;

								uInstDescFlags &= ~USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
								if	(psSample->bNonDependent && (psSample->sTexChanInfo.uTexChunkMask == psSample->sTexChanInfo.uTexNonDepChunkMask))
								{
									uInstDescFlags |= psSample->psSampleUnpack->uInstDescFlags & USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
								}
							}
						}
						else
						{
							if	(!HWInstEncodePCKUNPCKInst(psPrevHWInst,
														   bSkipInv,
														   ePCKUNPCKDestFmt,
														   USP_PCKUNPCK_FMT_U8,
														   IMG_FALSE,
														   uWriteMask,
														   psDestReg,
														   psPrevDestReg->uComp,
														   psDestReg->uComp,
														   psSrc1Reg,
														   psSrc2Reg))
							{
								USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding PCKUNPCK inst\n"));
								return IMG_FALSE;
							}
						}

						peOpcode[-1] = USP_OPCODE_PCKUNPCK;

						/*
							No more components can be copied using this
							instruction
						*/
						ePrevInstOpcode = USP_OPCODE_INVALID;
						psPrevHWInst	= IMG_NULL;
					}
					else
					{
						/*
							U8<->C10 is not supported by PCKUNPCK. To work around this,
							we pack/copy the components in U8 format (i.e. no conversion)
							to a temporary location, and add a SOPWM at the end to convert
							to C10 into the required destination.
						*/
						if	(ePCKUNPCKDestFmt == USP_PCKUNPCK_FMT_C10)
						{
							USP_REG	sTempDestReg;

							sTempDestReg.eType		= psSample->psSampleUnpack->eSampleUnpackTempRegType;
							sTempDestReg.uNum		= psSample->psSampleUnpack->uSampleUnpackTempRegNum + psSample->psSampleUnpack->uSampleUnpackTempRegCount;
							sTempDestReg.eDynIdx	= USP_DYNIDX_NONE;
							sTempDestReg.uComp		= 0;
							sTempDestReg.eFmt		= USP_REGFMT_U8;

							if	(!HWInstEncodePCKUNPCKInst(psHWInst,
														   bSkipInv,
														   USP_PCKUNPCK_FMT_U8,
														   USP_PCKUNPCK_FMT_U8,
														   IMG_FALSE,
														   1 << psDestReg->uComp,
														   &sTempDestReg,
														   psDestReg->uComp,
														   1,
														   psChanReg,
														   (PUSP_REG)&sHWConstZero))
							{
								USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding PCKUNPCK inst\n"));
								return IMG_FALSE;
							}

							uChansToConvertToC10 |= 1 << psDestReg->uComp;
							if	(!psDestForConvertToC10)
							{
								psDestForConvertToC10 = psDestReg;
							}
						}
						else
						{
							/*
								Add an instruction to copy the component
								between the two registers
							*/
							if	(!HWInstEncodePCKUNPCKInst(psHWInst,
														   bSkipInv,
														   ePCKUNPCKDestFmt,
														   USP_PCKUNPCK_FMT_U8,
														   IMG_FALSE,
														   1 << psDestReg->uComp,
														   psDestReg,
														   psDestReg->uComp,
														   1,
														   psChanReg,
														   (PUSP_REG)&sHWConstZero))
							{
								USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding PCKUNPCK inst\n"));
								return IMG_FALSE;
							}
						}

						*peOpcode = USP_OPCODE_PCKUNPCK;

						*puInstDescFlags = uInstDescFlags;						

						uHWInstCount++;

						/*
							2 samples can be unpacked to the same destination register,
							so we may be able to merge another instruction with this one.
						*/
						psPrevHWInst		= psHWInst;
						ePrevInstOpcode		= USP_OPCODE_PCKUNPCK;

						/*
							Don't flag that we have generated the first unpacking 
							instruction if the PCKUNPCK we have just generated is
							writing to a temporary location. 
							
							NB:	This ensures that the first unpacking instruction 
								that does actually write to a result-register gets
								flagged as such in it's instruction-description
								flags, and also that we do not incorrectly flag that
								the IRegs are live prior to the next instruction.
						*/
						if	(ePCKUNPCKDestFmt != USP_PCKUNPCK_FMT_C10)
						{
							bFirstUnpackInst = IMG_FALSE;
						}
					}
				}

				break;
			}

			case USP_CHAN_FORMAT_S8:
			case USP_CHAN_FORMAT_U16:
			case USP_CHAN_FORMAT_S16:
			case USP_CHAN_FORMAT_F16:
			case USP_CHAN_FORMAT_F32:
			{
				USP_PCKUNPCK_FMT	ePCKUNPCKSrcFmt;
				USP_PCKUNPCK_FMT	ePCKUNPCKDestFmt;
				IMG_BOOL			bCanMergeInst;

				ePCKUNPCKSrcFmt = aePCKUNPCKFmtForChanFmt[eDataFmtForChan];

				/*
					Determine the appropriate dest format for PCKUNPCK

					NB: Currently, the same format should be specified for
						all components of the result.
				*/
				if	(psDestReg->eFmt == USP_REGFMT_U8)
				{
					ePCKUNPCKDestFmt = USP_PCKUNPCK_FMT_U8;
				}
				else
				{
					ePCKUNPCKDestFmt = USP_PCKUNPCK_FMT_C10;
				}

				/*
					Check whether we can unpack this channel with the
					preceeding one
				*/
				bCanMergeInst = IMG_FALSE;

				if	(ePrevInstOpcode == USP_OPCODE_PCKUNPCK)
				{
					if	(
							(psPrevDestReg->uNum	== psDestReg->uNum)  &&
							(psPrevDestReg->eType	== psDestReg->eType) &&
							(psPrevDestReg->eFmt	== psDestReg->eFmt)
							#if defined(SGX_FEATURE_USE_VEC34)
							&& (psPrevChanReg->eType == psChanReg->eType) 
							&& CanUnpackTwoChannels(psDestReg->eFmt, psChanReg, eDataFmtForChan)
							#endif /* defined(SGX_FEATURE_USE_VEC34) */
						)
					{
						USP_PCKUNPCK_FMT ePrevPCKUNPCKSrcFmt;

						ePrevPCKUNPCKSrcFmt = HWInstDecodePCKUNPCKInstSrcFormat(psPrevHWInst);
						if	(ePrevPCKUNPCKSrcFmt == ePCKUNPCKSrcFmt)
						{
							bCanMergeInst = IMG_TRUE;
						}
					}
				}

				if	(bCanMergeInst)
				{
					IMG_UINT32	uWriteMask;
					PUSP_REG	psSrc1Reg;
					PUSP_REG	psSrc2Reg;

					/*
						Replace the last instruction with one that unpacks
						both this and the preceeding sample
					*/
					uWriteMask = (1 << psPrevDestReg->uComp) |
								 (1 << psDestReg->uComp);

					#if defined(SGX_FEATURE_USE_VEC34)
					psSrc1Reg = psPrevChanReg;
					psSrc2Reg = psChanReg;
					#else
					if	(psPrevDestReg->uComp < psDestReg->uComp)
					{
						psSrc1Reg = psPrevChanReg;
						psSrc2Reg = psChanReg;
					}
					else
					{
						psSrc1Reg = psChanReg;
						psSrc2Reg = psPrevChanReg;
					}
					#endif /* defined(SGX_FEATURE_USE_VEC34) */

					if	(!HWInstEncodePCKUNPCKInst(psPrevHWInst,
												   bSkipInv,
												   ePCKUNPCKDestFmt,
												   ePCKUNPCKSrcFmt,
												   IMG_TRUE,
												   uWriteMask,
												   psDestReg,
												   psPrevDestReg->uComp,
												   psDestReg->uComp,
												   psSrc1Reg,
												   psSrc2Reg))
					{
						USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding PCKUNPCK inst\n"));
						return IMG_FALSE;
					}

					/*
						No more components can be copied using this
						instruction
					*/
					ePrevInstOpcode = USP_OPCODE_INVALID;
					psPrevHWInst	= IMG_NULL;
				}
				else
				{
					if((ePCKUNPCKSrcFmt == USP_PCKUNPCK_FMT_U16) || 
					   (ePCKUNPCKSrcFmt == USP_PCKUNPCK_FMT_S16))
					{
						IMG_UINT32 uRemapHWInstCount = 0;
						IMG_BOOL bRemappingDone = IMG_FALSE;
						if	(!RemapInteger16Channel(psSample, psContext, 
							psHWInst, bSkipInv, peOpcode, puInstDescFlags, 
							uInstDescFlags, &uRemapHWInstCount, uChanIdx, 
							ePCKUNPCKSrcFmt, &uChunksRemappedMask, 
							&bRemappingDone))
						{
							USP_DBGPRINT(("UnpackTextureDataToU8C10: Error remapping U16 results \n"));
							return IMG_FALSE;
						}

						/*
							Now the data format is changed to F16
						*/
						ePCKUNPCKSrcFmt = USP_PCKUNPCK_FMT_F16;
						
						if(bRemappingDone)
						{
							/*
								Update the instruction counter and move to 
								next memory location
							*/

							uHWInstCount += uRemapHWInstCount;

							ePrevInstOpcode		= USP_OPCODE_PCKUNPCK;
							psPrevHWInst		= psHWInst + (uRemapHWInstCount-1);
							bFirstUnpackInst	= IMG_FALSE;

							peOpcode += uRemapHWInstCount;
							puInstDescFlags += uRemapHWInstCount;
							psHWInst += uRemapHWInstCount;
						}

					}

					/*
						Setup an instruction to unpack this sample
					*/
					if	(!HWInstEncodePCKUNPCKInst(psHWInst,
												   bSkipInv,
												   ePCKUNPCKDestFmt,
												   ePCKUNPCKSrcFmt,
												   IMG_TRUE,
												   1 << psDestReg->uComp,
												   psDestReg,
												   psDestReg->uComp,
												   1,
												   psChanReg,
												   (PUSP_REG)&sHWConstZero))
					{
						USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding PCKUNPCK inst\n"));
						return IMG_FALSE;
					}

					*peOpcode = USP_OPCODE_PCKUNPCK;

					*puInstDescFlags = uInstDescFlags;					

					uHWInstCount++;

					/*
						2 samples can be unpacked to the same destination register,
						so we may be able to merge another instruction with this one.
					*/
					ePrevInstOpcode		= USP_OPCODE_PCKUNPCK;
					psPrevHWInst		= psHWInst;
					bFirstUnpackInst	= IMG_FALSE;
				}

				break;
			}

			case USP_CHAN_FORMAT_U24:
			{
				USP_DBGPRINT(("UnpackTextureDataToU8C10: U24 source data not yet supported\n"));
				return IMG_FALSE;
			}

			default:
			{
				USP_DBGPRINT(("UnpackTextureDataToU8C10: Unhandled source data format\n"));
				return IMG_FALSE;
			}
		}

		/*
			If we have generated the first unpacking code, and are unpacking
			to an IReg, then the IRegs will be live prior to all following
			unpacking instructions.
		*/
		if	(!bFirstUnpackInst)
		{
			if	(psDestReg->eType == USP_REGTYPE_INTERNAL)
			{
				uInstDescFlags |= USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
			}
		}

		psPrevDestReg			= psDestReg;
		psPrevChanReg			= psChanReg;
		bPrevFirstUnpackInst	= (IMG_BOOL)(bWasFirstUnpackInst && !bFirstUnpackInst);
	}

	/*
		Add a final SOPWM to convert the unpacked data to C10 when avoiding
		U8->C10 PCKUNPCKs
	*/
	if	(uChansToConvertToC10)
	{
		USP_REG	sTempDestReg;

		psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
		peOpcode		= &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
		puInstDescFlags	= &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

		/*
			Setup the temporary used to hold the U8 data we need to convert
		*/
		sTempDestReg.eType		= psSample->psSampleUnpack->eSampleUnpackTempRegType;
		sTempDestReg.uNum		= psSample->psSampleUnpack->uSampleUnpackTempRegNum + psSample->psSampleUnpack->uSampleUnpackTempRegCount;
		sTempDestReg.eDynIdx	= USP_DYNIDX_NONE;
		sTempDestReg.uComp		= 0;
		sTempDestReg.eFmt		= USP_REGFMT_U8;

		/*
			insert a MOE SETFC inst if needed before the SOPWM, so that we can
			use per-operand format selection.
		*/
		if	(!psMOEState->bColFmtCtl)
		{
			if	(!HWInstEncodeSETFCInst(psHWInst, IMG_TRUE, psMOEState->bEFOFmtCtl))
			{
				USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding SETFC inst\n"));
				return IMG_FALSE;
			}

			psMOEState->bColFmtCtl = IMG_TRUE;

			*peOpcode			= USP_OPCODE_SETFC;
			*puInstDescFlags	= uInstDescFlags & (USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE |
													USP_INSTDESC_FLAG_DONT_SKIPINV);

			uHWInstCount++;
			psHWInst++;
			peOpcode++;
			puInstDescFlags++;
		}

		/*
			Encode a SOPWM instruction to convert the U8 data to C10

			NB: We assume all channels are going to the same C10 destination
		*/
		if	(!HWInstEncodeSOPWMInst(psMOEState,
									psHWInst,
									bSkipInv,
									EURASIA_USE1_SOP2WM_COP_ADD,
									EURASIA_USE1_SOP2WM_AOP_ADD,
									EURASIA_USE1_SOP2WM_MOD1_COMPLEMENT,
									EURASIA_USE1_SOP2WM_SEL1_ZERO,
									EURASIA_USE1_SOP2WM_MOD2_NONE,
									EURASIA_USE1_SOP2WM_SEL2_ZERO,
									uChansToConvertToC10,
									psDestForConvertToC10,
									&sTempDestReg,
									(PUSP_REG)&sHWConstZero))
		{
			USP_DBGPRINT(("UnpackTextureDataToU8C10: Error encoding SOPWM inst\n"));
			return IMG_FALSE;
		}

		*peOpcode = USP_OPCODE_SOPWM;

		*puInstDescFlags = uInstDescFlags;		

		uHWInstCount++;

		/*
			Account for the extra temporary used
		*/
		psSample->psSampleUnpack->uSampleUnpackTempRegCount++;
	}

	/*
		Record the number of instructions generated to unpack the texture
	*/
	psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;

	return IMG_TRUE;
}

/******************************************************************************
 Name:		UnpackTextureDataToF16

 Purpose:	Unpack sampled texture data to F16 results

 Inputs:	psSample	- The dependent sample to generate code for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL UnpackTextureDataToF16(PUSP_SAMPLE	psSample,
									   PUSP_CONTEXT	psContext)
{
	static const USP_PCKUNPCK_FMT aePCKUNPCKFmtForChanFmt[] =
	{
		USP_PCKUNPCK_FMT_U8,	/* USP_CHAN_FORMAT_U8 */
		USP_PCKUNPCK_FMT_S8,	/* USP_CHAN_FORMAT_S8 */
		USP_PCKUNPCK_FMT_U16,	/* USP_CHAN_FORMAT_U16 */
		USP_PCKUNPCK_FMT_S16,	/* USP_CHAN_FORMAT_S16 */
		USP_PCKUNPCK_FMT_F16,	/* USP_CHAN_FORMAT_F16 */
		USP_PCKUNPCK_FMT_F32,	/* USP_CHAN_FORMAT_F32 */
		USP_PCKUNPCK_FMT_INVALID/* USP_CHAN_FORMAT_U24 */
	};
	static const USP_REG sHWConstZero = 
	{
		USP_REGTYPE_SPECIAL,
		HW_CONST_ZERO,
		USP_REGFMT_F16,
		0,
		USP_DYNIDX_NONE
	};

	IMG_UINT32		uInstDescFlags;
	IMG_BOOL		bSkipInv;
	USP_OPCODE		ePrevInstOpcode;
	IMG_UINT32		uChanIdx;
	IMG_UINT32		uPrevChanIdx;
	IMG_UINT32		uHWInstCount;
	PHW_INST		psPrevHWInst;
	PUSP_REG		psPrevDestReg;
	PUSP_REG		psPrevChanReg;
	USP_CHAN_FORMAT	ePrevChanFmt;
	IMG_UINT32		uChansSampledDirect;
	IMG_BOOL		bFirstUnpackInst;
	IMG_BOOL		bPrevChanChunkNonDep;

	PUSP_TEX_CHAN_INFO    psTexChanInfo;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	#if defined(SGX_FEATURE_USE_VEC34)
	if(CanUseVectorUnpack(psSample, psContext, USP_REGFMT_F16))
	{
		return IMG_TRUE;
	}
	#endif /* defined(SGX_FEATURE_USE_VEC34) */

	/*
		Look for sample instructions that can sample directly to the intended
		destination
	*/
	uChansSampledDirect	= 0;
	ePrevInstOpcode		= USP_OPCODE_INVALID;
	psPrevDestReg		= IMG_NULL;
	psPrevChanReg		= IMG_NULL;
	ePrevChanFmt		= USP_CHAN_FORMAT_INVALID;
	uPrevChanIdx		= 0;
	bPrevChanChunkNonDep = IMG_FALSE;

	psTexChanInfo = &psSample->sTexChanInfo;
	
	if(psSample->psSampleUnpack->bCanSampleDirectToDest)
	{
		#if defined(SGX545)
		if(!CanUseDestDirectForUnpackedTexture(psSample, psContext, &uChansSampledDirect))
		#endif /* defined(SGX545) */
		{
			for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
			{
				PUSP_REG	psChanReg;
				PUSP_REG	psDestReg;
				IMG_BOOL	bChanChunkNonDep;

				/*
					Skip this channel if we didn't need to sample it
				*/
				if	(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
				{
					continue;
				}

				{
					USP_CHAN_SWIZZLE	eTexChanSwizzle;
					eTexChanSwizzle		= psTexChanInfo->aeTexChanForComp[uChanIdx];
					if (eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
					{
						IMG_UINT32			uChunkIdx;

						uChunkIdx			= psTexChanInfo->auChunkIdxForChan[eTexChanSwizzle];
						bChanChunkNonDep	= ((psTexChanInfo->uTexNonDepChunkMask & (1 << uChunkIdx)) ? IMG_TRUE : IMG_FALSE);
					}
					else
					{
						bChanChunkNonDep	= IMG_FALSE;
					}
				}


				/*
					Get the source and destination registers to use for this channel
				*/
				psChanReg = &psSample->asChanReg[uChanIdx];
				psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

				/*
					Only channels that are in the same format as the destination
					(i.e F16 here) can be sampled directly
				*/
				if	(GetSampleChannelFormat(psSample, uChanIdx) == USP_CHAN_FORMAT_F16)
				{
					IMG_BOOL	bCanMergeInst;

					/*
						Check whether we can unpack this channel with the
						preceeding one
					*/
					bCanMergeInst = IMG_FALSE;

					if	(ePrevInstOpcode == USP_OPCODE_PCKUNPCK)
					{
						if	(
								(psPrevDestReg->uNum	== psDestReg->uNum)  &&
								(psPrevDestReg->eType	== psDestReg->eType) &&
								(psPrevDestReg->eFmt	== psDestReg->eFmt)	 &&
								(psPrevChanReg->eFmt	== psChanReg->eFmt)
								#if defined(SGX_FEATURE_USE_VEC34)
								&& (psPrevChanReg->eType == psChanReg->eType) 
								&& CanUnpackTwoChannels(psDestReg->eFmt, psChanReg, GetSampleChannelFormat(psSample, uChanIdx))
								#endif /* defined(SGX_FEATURE_USE_VEC34) */
							)
						{
							bCanMergeInst = IMG_TRUE;
						}
					}

					if	(bCanMergeInst)
					{
						USP_REG	sFinalDestReg;
						/*
							Have we just sampled the first or second two F16-channels
							together, in order, from the same register?
						*/
						if	(
								(psPrevChanReg->eType == psChanReg->eType) && 
								(psPrevChanReg->uNum == psChanReg->uNum) && 
								(
									(psPrevChanReg->uComp == 0) &&
									(psChanReg->uComp == 2)
								)
							)
						{

							/*
								If the result of this sample forms part of the overall shader result,
								account for the required result-remapping.
							*/
							sFinalDestReg = *psPrevDestReg;

							if	(psSample->psSampleUnpack->uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT)
							{
								PUSP_SHADER		psShader;
								PUSP_PROGDESC	psProgDesc;
								USP_REGTYPE		eFinalResultRegType;
								IMG_UINT32		uFinalResultRegNum;
								IMG_UINT32		uOrgResultRegNum;

								/*
									Remap the destination to the final result register
								*/
								psShader	= psSample->psShader;
								psProgDesc	= psShader->psProgDesc;

								eFinalResultRegType	= psShader->eFinalResultRegType;
								uFinalResultRegNum	= psShader->uFinalResultRegNum;
								uOrgResultRegNum	= psProgDesc->uDefaultPSResultRegNum;

								sFinalDestReg.eType	= eFinalResultRegType;
								sFinalDestReg.uNum	= (psPrevDestReg->uNum - uOrgResultRegNum) + 
													  uFinalResultRegNum;
							}

							/*
								Check whether these two components can be sampled
								directly into the destination register.
							*/
							if(bPrevChanChunkNonDep	== bChanChunkNonDep && bChanChunkNonDep == IMG_TRUE)
							{
								if	(
										(sFinalDestReg.eDynIdx	== USP_DYNIDX_NONE) &&
										(sFinalDestReg.eType	== psChanReg->eType) &&
										(sFinalDestReg.uNum		== psChanReg->uNum)
									)
								{
									uChansSampledDirect |= (1 << uChanIdx) |
														   (1 << uPrevChanIdx);
								}
							}
							else
							{
								PUSP_TEX_CHAN_INFO    psTexChanInfo;
								PUSP_TEX_CHAN_FORMAT  psTexFormat;
								USP_CHAN_SWIZZLE      ePrevTexChanSwizzle;
								USP_CHAN_SWIZZLE      eTexChanSwizzle;

								/*
									Make sure we sampled the texture for these
									two channels of the destination.
								*/
								psTexFormat			= &psSample->sSamplerDesc.sTexChanFormat;
								psTexChanInfo		= &psSample->sTexChanInfo;

								eTexChanSwizzle		= psTexChanInfo->aeTexChanForComp[uChanIdx];
								ePrevTexChanSwizzle	= psTexChanInfo->aeTexChanForComp[uChanIdx - 1];

								if	(
										(ePrevTexChanSwizzle >= USP_CHAN_SWIZZLE_CHAN_0) &&
										(ePrevTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3) &&
										(eTexChanSwizzle >= USP_CHAN_SWIZZLE_CHAN_0) &&
										(eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
									)
								{
									USP_CHAN_CONTENT	ePrevChanContent;
									USP_CHAN_CONTENT	eChanContent;

									eChanContent		= psTexFormat->aeChanContent[eTexChanSwizzle];
									ePrevChanContent	= psTexFormat->aeChanContent[ePrevTexChanSwizzle];

									if	(
											(eChanContent == USP_CHAN_CONTENT_DATA) &&
											(ePrevChanContent == USP_CHAN_CONTENT_DATA)
										)
									{
										PHW_INST	psSampleInst;
										PHW_INST	psPrevSampleInst;

										/*
											Make sure that the data for this and the previous
											channel were fetched by the same instruction
										*/
										psSampleInst		= psSample->apsSMPInsts[uChanIdx];
										psPrevSampleInst	= psSample->apsSMPInsts[uChanIdx - 1];

										if	(psSampleInst == psPrevSampleInst)
										{
											HW_INST		sTempSampleInst;
											IMG_UINT32	uSampleInstIdx;
											USP_OPCODE	eSampleInstOpcode;

											uSampleInstIdx		= psSampleInst - psSample->psSampleUnpack->asSampleInsts;
											eSampleInstOpcode	= psSample->psSampleUnpack->aeOpcodeForSampleInsts[uSampleInstIdx];

											/*
												Is the destination register-type one that is supported
												by the SMP instruction?
											*/
											sTempSampleInst = *psSampleInst;

											if	(HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
																			eSampleInstOpcode,
																			&sTempSampleInst,
																			&sFinalDestReg))
											{
												IMG_BOOL bSMPReadsDest;

												/*
													Check whether the destination register may be used
													by one of the sources for the sample instruction.
												*/
												if	(!SampleInstReadsReg(psSample,
																		 psSampleInst,
																		 &sFinalDestReg,
																		 psContext,
																		 &bSMPReadsDest))
												{
													USP_DBGPRINT(("UnpackTextureDataToF16: Error checking for SMP inst that references dest\n"));
													return IMG_FALSE;
												}

												/*
													Only sample directly to the destination register
													if it isn't used through any of the sources.
												*/
												if	(!bSMPReadsDest)
												{
													IMG_UINT32	uInstDescFlags;
													IMG_UINT32	uChunkIdx;
													IMG_UINT32	uChunkRegOff;

													/*
														Update the sample instruction to write the
														sampled data directly into the destination
														register.

														NB: We don't use the final result register here
															(i.e. after result remapping) because this
															instruciton will be updated like any other.
													*/
													if	(!HWInstEncodeDestBankAndNum(USP_FMTCTL_NONE,
																					 eSampleInstOpcode,
																					 psSampleInst,
																					 psPrevDestReg))
													{
														USP_DBGPRINT(("UnpackTextureDataToF16: Error encoding dest for SMP inst\n"));
														return IMG_FALSE;
													}

													/*
														Now that this sample instruction writes directly
														to the intended destination, rather than an
														intermediate temporary, it may reference a result-
														register.
													*/
													uInstDescFlags = psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx];

													uInstDescFlags |= psSample->psSampleUnpack->uInstDescFlags & 
																	  (USP_INSTDESC_FLAG_DEST_USES_RESULT);											

													psSample->psSampleUnpack->auInstDescFlagsForSampleInsts[uSampleInstIdx] = uInstDescFlags;

													/*
														Record the new location of the data for
														this chunk and recalculate the source reg
														for all destination channels.
													*/
													uChunkIdx = psTexChanInfo->auChunkIdxForChan[ePrevTexChanSwizzle];
													psSample->asChunkReg[uChunkIdx] = *psPrevDestReg; 

													/*
														Update the number of intermediate registers used
													*/
													uChunkRegOff = psTexChanInfo->auDestRegForChunk[uChunkIdx];
													if	((uChunkRegOff + 1) == (*(psSample->psSampleUnpack->puBaseSampleDestRegCount)))
													{
														if(psSample->psSampleUnpack->puBaseSampleTempDestRegCount != psSample->psSampleUnpack->puBaseSampleDestRegCount)
														{
															(*(psSample->psSampleUnpack->puBaseSampleTempDestRegCount))--;
														}
														//psSample->uNumTempRegsUsed--;
														(*(psSample->psSampleUnpack->puBaseSampleDestRegCount))--;														
														#if 0
														#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
														if((psSample->uUnAlignedTempCount>0) && 
														   (psSample->uUnAlignedTempCount == psSample->uSampleTempCount))
														{
															//psSample->uNumTempRegsUsed -= psSample->uUnAlignedTempCount;
															psSample->uSampleTempCount -= psSample->uUnAlignedTempCount;
															psSample->uUnAlignedTempCount = 0;
														}
														#endif /* defined(SGX_FEATURE_UNIFIED_STORE_64BITS) */
														#endif
													}

													uChansSampledDirect |= (1 << uChanIdx) |
																		   (1 << uPrevChanIdx);
												}
											}
										}
									}
								}
							}
						}

						/*
							Cannot pack more then 2 source-values per instruction,
						*/
						ePrevInstOpcode = USP_OPCODE_INVALID;
					}
					else
					{
						/*
							2 samples can be unpacked to the same destination register,
							so we may be able to merge another instruction with this one.
						*/
						ePrevInstOpcode = USP_OPCODE_PCKUNPCK;
					}
				}

				psPrevDestReg	= psDestReg;
				psPrevChanReg	= psChanReg;
				uPrevChanIdx	= uChanIdx;
				bPrevChanChunkNonDep	= bChanChunkNonDep;
			}
		}
	}

	/*
		If we modified one or more sample instructions, recalculate the source
		registers for the data for each channel
	*/
	if	(uChansSampledDirect)
	{
		if	(!SetupChanSrcRegs(psSample, psContext))
		{
			USP_DBGPRINT(("UnpackTextureDataToF16: Error updating src regs for each channel\n"));
			return IMG_FALSE;
		}
	}

	/*
		Check for non-dependent reads that need to be unpacked from a temporary
		location
	*/
	if	(psSample->bNonDependent)
	{
		USP_OPCODE	eInstOpcode;
		IMG_UINT32	uInstWriteMask;
		IMG_UINT32	uPrevInstWriteMask;
		IMG_UINT32	uPrevNumUnpackInsts;
		IMG_UINT32	uNumUnpackInsts;

		psPrevDestReg		= IMG_NULL;
		psPrevChanReg		= IMG_NULL;
		ePrevInstOpcode		= USP_OPCODE_INVALID;
		uPrevInstWriteMask	= 0;
		eInstOpcode			= USP_OPCODE_INVALID;
		uInstWriteMask		= 0;
		uNumUnpackInsts		= 0;
		uPrevNumUnpackInsts	= 0;
		uChanIdx			= 0;

		while (uChanIdx < USP_MAX_SAMPLE_CHANS)
		{
			USP_CHAN_FORMAT	eDataFmtForChan;
			PUSP_REG		psChanReg;
			PUSP_REG		psDestReg;
			IMG_BOOL		bChunksMoved;

			/*
				Skip this channel if we didn't need to sample it
			*/
			if	(!(psSample->psSampleUnpack->uMask & (1 << uChanIdx)))
			{
				{
					USP_CHAN_SWIZZLE	eTexChanSwizzle;
					eTexChanSwizzle		= psTexChanInfo->aeTexChanForComp[uChanIdx];
					if (eTexChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3)
					{
						IMG_UINT32			uChunkIdx;
						IMG_BOOL			bChanChunkNonDep;

						uChunkIdx			= psTexChanInfo->auChunkIdxForChan[eTexChanSwizzle];
						bChanChunkNonDep	= ((psTexChanInfo->uTexNonDepChunkMask & (1 << uChunkIdx)) ? IMG_TRUE : IMG_FALSE);
						if(bChanChunkNonDep == IMG_FALSE)
						{
							uChanIdx++;
							continue;
						}
					}
				}
				uChanIdx++;
				continue;
			}

			/*
				Get the source and destination registers to use for this channel
			*/
			psChanReg = &psSample->asChanReg[uChanIdx];
			psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

			/*
				Detemine whether this channel can be unpacked/copied together
				with previous channels
			*/
			eDataFmtForChan	= GetSampleChannelFormat(psSample, uChanIdx);
			switch (eDataFmtForChan)
			{
				case USP_CHAN_FORMAT_U8:
				case USP_CHAN_FORMAT_S8:
				case USP_CHAN_FORMAT_U16:
				case USP_CHAN_FORMAT_S16:
				case USP_CHAN_FORMAT_F16:
				case USP_CHAN_FORMAT_F32:
				{
					IMG_BOOL bCanMergeInst;

					/*
						Check whether we can unpack this channel with the
						preceeding one
					*/
					bCanMergeInst = IMG_FALSE;

					if	(ePrevInstOpcode == USP_OPCODE_PCKUNPCK)
					{
						if	(
								(psPrevDestReg->uNum	== psDestReg->uNum)  &&
								(psPrevDestReg->eType	== psDestReg->eType) &&
								(psPrevDestReg->eFmt	== psDestReg->eFmt)	 &&
								(psPrevChanReg->eFmt	== psChanReg->eFmt)
								#if defined(SGX_FEATURE_USE_VEC34)
								&& (psPrevChanReg->eType == psChanReg->eType) 
								&& CanUnpackTwoChannels(psDestReg->eFmt, psChanReg, eDataFmtForChan)
								#endif /* defined(SGX_FEATURE_USE_VEC34) */
							)
						{
							bCanMergeInst = IMG_TRUE;
						}
					}

					if	(bCanMergeInst)
					{
						/*
							We will unpack the data for this channel along with the
							precedina one
						*/
						uPrevInstWriteMask |= 0x3 << psDestReg->uComp;
					}
					else
					{
						/*
							We will generate a (new) PCKUNPCK instruction to
							copy this channel.
						*/
						eInstOpcode		= USP_OPCODE_PCKUNPCK;
						uInstWriteMask	= 0x3 << psDestReg->uComp;

						uNumUnpackInsts++;
					}

					break;
				}

				case USP_CHAN_FORMAT_U24:
				{
					USP_DBGPRINT(("UnpackTextureDataToF16: U24 source data not yet supported\n"));
					return IMG_FALSE;
				}

				default:
				{
					USP_DBGPRINT(("UnpackTextureDataToF16: Unhandled source data format\n"));
					return IMG_FALSE;
				}
			}

			/*
				Check whether the last unpacking instruction potentially
				overwrites source data needed for the unpacking of later
				channels.
			*/
			bChunksMoved = IMG_FALSE;

			if	(uNumUnpackInsts != uPrevNumUnpackInsts)
			{
				if	(uNumUnpackInsts > 1)
				{
					USP_REG	sFinalDestReg;

					/*
						Is this unpack instruction writing to a result-reg?
					*/
					if	(psSample->psSampleUnpack->uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT)
					{
						PUSP_SHADER		psShader;
						PUSP_PROGDESC	psProgDesc;
						USP_REGTYPE		eFinalResultRegType;
						IMG_UINT32		uFinalResultRegNum;
						IMG_UINT32		uOrgResultRegNum;

						/*
							Remap the destination to the final result register
						*/
						psShader	= psSample->psShader;
						psProgDesc	= psShader->psProgDesc;

						eFinalResultRegType	= psShader->eFinalResultRegType;
						uFinalResultRegNum	= psShader->uFinalResultRegNum;
						uOrgResultRegNum	= psProgDesc->uDefaultPSResultRegNum;

						sFinalDestReg		= *psPrevDestReg;
						sFinalDestReg.eType	= eFinalResultRegType;
						sFinalDestReg.uNum	= (psPrevDestReg->uNum - uOrgResultRegNum) +
											  uFinalResultRegNum;
						psPrevDestReg		= &sFinalDestReg;						
					}

					if	(!UnpackChunksFromTemp(psSample,
											   psPrevDestReg,
											   uPrevInstWriteMask,
											   uChanIdx,
											   psContext,
											   &bChunksMoved))
					{
						USP_DBGPRINT(("UnpackTextureDataToF16: Error testing whether non-dep data should be unpacked from temps\n"));
						return IMG_FALSE;
					}
				}

				uPrevInstWriteMask	= uInstWriteMask;
				ePrevInstOpcode		= eInstOpcode;
				uPrevNumUnpackInsts	= uNumUnpackInsts;
			}

			/*
				Restart if we have altered the source data for any chunks
			*/
			if	(bChunksMoved)
			{
				ePrevInstOpcode		= USP_OPCODE_INVALID;
				uPrevInstWriteMask	= 0;
				eInstOpcode			= USP_OPCODE_INVALID;
				uInstWriteMask		= 0;
				uNumUnpackInsts		= 0;
				uPrevNumUnpackInsts	= 0;
				uChanIdx			= 0;
			}
			else
			{
				psPrevDestReg = psDestReg;
				psPrevChanReg = psChanReg;
				uChanIdx++;
			}
		}
	}

	/*
		Generate instructions to save the sampled data for each channel
		into the required final register locations
	*/
	uInstDescFlags	= psSample->psSampleUnpack->uInstDescFlags;
#if 0
	& ~(USP_INSTDESC_FLAG_SRC0_USES_RESULT |
												   USP_INSTDESC_FLAG_SRC1_USES_RESULT |
												   USP_INSTDESC_FLAG_SRC2_USES_RESULT);

	if	((!psSample->bNonDependent) || (psSample->bNonDependent && psTexChanInfo->uTexChunkMask && (psTexChanInfo->uTexChunkMask != psTexChanInfo->uTexNonDepChunkMask)) )
	{
		uInstDescFlags &= ~USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
	}
#endif

	bSkipInv			= (IMG_BOOL)(!(uInstDescFlags & USP_INSTDESC_FLAG_DONT_SKIPINV));
	ePrevInstOpcode		= USP_OPCODE_INVALID;
	psPrevHWInst		= IMG_NULL;
	psPrevDestReg		= IMG_NULL;
	psPrevChanReg		= IMG_NULL;
	ePrevChanFmt		= USP_CHAN_FORMAT_INVALID;
	uHWInstCount		= psSample->psSampleUnpack->uUnpackInstCount;
	bFirstUnpackInst	= IMG_TRUE;

	for (uChanIdx = 0; uChanIdx < USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		USP_CHAN_FORMAT	eDataFmtForChan;
		PHW_INST		psHWInst;
		PUSP_OPCODE		peOpcode;
		IMG_PUINT32		puInstDescFlags;
		PUSP_REG		psChanReg;
		PUSP_REG		psDestReg;

		/*
			Skip this channel if we didn't need to sample it, or it has been
			samplied directly to the destination
		*/
		if	(!((psSample->psSampleUnpack->uMask ^ uChansSampledDirect) & (1 << uChanIdx)))
		{
			continue;
		}

		/*
			Get the source and destination registers to use for this channel
		*/
		psChanReg = &psSample->asChanReg[uChanIdx];
		psDestReg = &psSample->psSampleUnpack->asDest[uChanIdx];

		/*
			Generate instructions to copy and convert the data for this
			channel
		*/
		psHWInst		= &psSample->psSampleUnpack->asUnpackInsts[uHWInstCount];
		peOpcode		= &psSample->psSampleUnpack->aeOpcodeForUnpackInsts[uHWInstCount];
		puInstDescFlags	= &psSample->psSampleUnpack->auInstDescFlagsForUnpackInsts[uHWInstCount];

		eDataFmtForChan	= GetSampleChannelFormat(psSample, uChanIdx);
		switch (eDataFmtForChan)
		{
			case USP_CHAN_FORMAT_U8:
			case USP_CHAN_FORMAT_S8:
			case USP_CHAN_FORMAT_U16:
			case USP_CHAN_FORMAT_S16:
			case USP_CHAN_FORMAT_F16:
			case USP_CHAN_FORMAT_F32:
			{
				USP_PCKUNPCK_FMT	ePCKUNPCKSrcFmt;
				IMG_BOOL			bCanMergeInst;
				IMG_BOOL			bScale;

				ePCKUNPCKSrcFmt = aePCKUNPCKFmtForChanFmt[eDataFmtForChan];
				bScale			= (IMG_BOOL)(eDataFmtForChan != USP_CHAN_FORMAT_F16);

				/*
					Check whether we can unpack this channel with the
					preceeding one
				*/
				bCanMergeInst = IMG_FALSE;

				if	(ePrevInstOpcode == USP_OPCODE_PCKUNPCK)
				{
					if	(
							(psPrevDestReg->uNum	== psDestReg->uNum)  &&
							(psPrevDestReg->eType	== psDestReg->eType) &&
							(psPrevDestReg->eFmt	== psDestReg->eFmt)	 &&
							(psPrevChanReg->eFmt	== psChanReg->eFmt)	 &&
							(ePrevChanFmt			== eDataFmtForChan)
							#if defined(SGX_FEATURE_USE_VEC34)
							&& (psPrevChanReg->eType == psChanReg->eType) 
							&& CanUnpackTwoChannels(psDestReg->eFmt, psChanReg, eDataFmtForChan)
							#endif /* defined(SGX_FEATURE_USE_VEC34) */
						)
					{
						bCanMergeInst = IMG_TRUE;
					}
				}

				if	(bCanMergeInst)
				{
					IMG_UINT32	uWriteMask;
					PUSP_REG	psSrc1Reg;
					PUSP_REG	psSrc2Reg;

					/*
						Replace the last instruction with one that unpacks
						both this and the preceeding sample
					*/
					#if defined(SGX_FEATURE_USE_VEC34)
					uWriteMask = (0x1 << (psPrevDestReg->uComp/2)) | 
								 (0x1 << (psDestReg->uComp/2));
					
					psSrc1Reg = psPrevChanReg;
					psSrc2Reg = psChanReg;
					#else
					uWriteMask = (0x3 << psPrevDestReg->uComp) | 
								 (0x3 << psDestReg->uComp);

					if	(psPrevDestReg->uComp < psDestReg->uComp)
					{
						psSrc1Reg = psPrevChanReg;
						psSrc2Reg = psChanReg;
					}
					else
					{
						psSrc1Reg = psChanReg;
						psSrc2Reg = psPrevChanReg;
					}
					#endif /* defined(SGX_FEATURE_USE_VEC34) */

					if	(!HWInstEncodePCKUNPCKInst(psPrevHWInst,
												   bSkipInv,
												   USP_PCKUNPCK_FMT_F16,
												   ePCKUNPCKSrcFmt,
												   bScale,
												   uWriteMask,
												   psDestReg,
												   (psPrevDestReg->uComp/2),
												   (psDestReg->uComp/2),
												   psSrc1Reg,
												   psSrc2Reg))
					{
						USP_DBGPRINT(("UnpackTextureDataToF16: Error encoding PCKUNPCK inst\n"));
						return IMG_FALSE;
					}

					/*
						Cannot pack more then 2 source-values per instruction,
					*/
					ePrevInstOpcode = USP_OPCODE_INVALID;
					psPrevHWInst	= IMG_NULL;
				}
				else
				{
					/*
						Add an instruction to unpack this sample
					*/
					if	(!HWInstEncodePCKUNPCKInst(psHWInst,
												   bSkipInv,
												   USP_PCKUNPCK_FMT_F16,
												   ePCKUNPCKSrcFmt,
												   bScale,
												   #if defined(SGX_FEATURE_USE_VEC34)
												   0x1 << (psDestReg->uComp/2),
												   #else
												   0x3 << psDestReg->uComp,
												   #endif /* defined(SGX_FEATURE_USE_VEC34) */
												   psDestReg,
												   (psDestReg->uComp/2),
												   1,
												   psChanReg,
												   (PUSP_REG)&sHWConstZero))
					{
						USP_DBGPRINT(("UnpackTextureDataToF16: Error encoding PCKUNPCK inst\n"));
						return IMG_FALSE;
					}

					*peOpcode = USP_OPCODE_PCKUNPCK;

					*puInstDescFlags = uInstDescFlags;					

					uHWInstCount++;

					/*
						2 samples can be unpacked to the same destination register,
						so we may be able to merge another instruction with this one.
					*/
					ePrevInstOpcode		= USP_OPCODE_PCKUNPCK;
					psPrevHWInst		= psHWInst;
					bFirstUnpackInst	= IMG_FALSE;
				}

				break;
			}

			case USP_CHAN_FORMAT_U24:
			{
				USP_DBGPRINT(("UnpackTextureDataToF16: U24 source data not yet supported\n"));
				return IMG_FALSE;
			}

		    case USP_CHAN_FORMAT_S32:
		    case USP_CHAN_FORMAT_U32:
			case USP_CHAN_FORMAT_INVALID:
			{
				USP_DBGPRINT(("UnpackTextureDataToF16: Invalid channel format\n"));
				return IMG_FALSE;
			}
		}

		/*
			If the first unpacking instruction writes to an IReg, then it must
			be live before any following unpack code.
		*/
		if	(!bFirstUnpackInst)
		{
			if	(psDestReg->eType == USP_REGTYPE_INTERNAL)
			{
				uInstDescFlags |= USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
			}
		}

		psPrevDestReg = psDestReg;
		psPrevChanReg = psChanReg;
		ePrevChanFmt = eDataFmtForChan;
	}

	/*
		Record the number of instructions generated to unpack the texture
	*/
	psSample->psSampleUnpack->uUnpackInstCount = uHWInstCount;

	return IMG_TRUE;
}

/******************************************************************************
 Name:		UnpackTextureData

 Purpose:	Add instructions to store sampled texture-data into the required
			destination registers

 Inputs:	psSample	- The dependent sample to generate code for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL UnpackTextureData(PUSP_SAMPLE	psSample,
								  PUSP_CONTEXT	psContext)
{
	USP_REGFMT	eDestFmt;
	IMG_UINT32	uCompIdx;
	IMG_UINT32	uCompMask;

	/*
		Currently, we assume the same data format will be required for all
		components of the destination, despite the USP interface allowing
		a completely independent register specification (including format)
		for each component.
	*/
	uCompMask = psSample->psSampleUnpack->uMask;
	for	(uCompIdx = 0; uCompIdx < USP_MAX_SAMPLE_CHANS; uCompIdx++)
	{
		if	(uCompMask & (1 << uCompIdx))
		{
			break;
		}
	}
	eDestFmt = psSample->psSampleUnpack->asDest[uCompIdx].eFmt;

	/*
	  Because USC normally samples to F32, in OpenCL we use the
	  original texture precision provided in the sample input
	  instruction to decide which format to unpack too.
	 */
	if( psSample->uTexPrecision != USP_REGFMT_UNKNOWN )
	{
		eDestFmt = psSample->uTexPrecision;
	}
	
	switch (eDestFmt)
	{
		case USP_REGFMT_F32:
		{
			if	(!UnpackTextureDataToF32(psSample, psContext))
			{
				USP_DBGPRINT(("UnpackTextureData: Error unpacking texture-data to F32\n"));
				return IMG_FALSE;
			}
			break;
		}

	    case USP_REGFMT_U32:
	    {
		    if  (!UnpackTextureDataToU32(psSample, psContext))
		    {
			    USP_DBGPRINT(("UnpackTextureData: Error unpacking texture-data to U32\n"));
			    return IMG_FALSE;
		    }
		    break;
	    }

	    case USP_REGFMT_I32:
	    {
		    if  (!UnpackTextureDataToS32(psSample, psContext))
		    {
			    USP_DBGPRINT(("UnpackTextureData: Error unpacking texture-data to S32\n"));
			    return IMG_FALSE;
		    }
		    break;
	    }

		case USP_REGFMT_F16:
		{
			if	(!UnpackTextureDataToF16(psSample, psContext))
			{
				USP_DBGPRINT(("UnpackTextureData: Error unpacking texture-data to F16\n"));
				return IMG_FALSE;
			}
			break;
		}

		case USP_REGFMT_U8:
		case USP_REGFMT_C10:
		{
			if	(!UnpackTextureDataToU8C10(psSample, psContext, eDestFmt))
			{
				USP_DBGPRINT(("UnpackTextureData: Error unpacking texture-data to U8/C10\n"));
				return IMG_FALSE;
			}
			break;
		}

		default:
		{
			USP_DBGPRINT(("UnpackTextureData: Unrecognised dest format\n"));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/******************************************************************************
 Name:		AddSampleCode

 Purpose:	Add the HW instructions generated for a sample to the associated
			code block

 Inputs:	psSample	- The dependent sample to generate code for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL AddSampleCode(PUSP_SAMPLE psSample, PUSP_CONTEXT psContext)
{
	IMG_UINT32	i;

	/*
		Add the instructions to sample the texture
	*/
	for	(i = 0; i < psSample->uSampleInstCount_; i++)
	{
		if	(!USPInstBlockInsertHWInst(psSample->psInstBlock,
									   IMG_NULL,
									   psSample->aeOpcodeForSampleInsts_[i],
									   &psSample->asSampleInsts_[i],
									   psSample->auInstDescFlagsForSampleInsts_[i],
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("AddSampleCode: Error adding HW-inst to block\n"));
			return IMG_FALSE;
		}
	}	
	
	return IMG_TRUE;
}

/******************************************************************************
 Name:		AddSampleUnpackCode

 Purpose:	Add the HW instructions generated for a sample unpack to the 
			associated code block

 Inputs:	psSampleUnpack
						- The sample unpack to generate code for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
static IMG_BOOL AddSampleUnpackCode(PUSP_SAMPLEUNPACK psSampleUnpack, PUSP_CONTEXT psContext)
{
	IMG_UINT32	i;

	/*
		Add the instructions to unpack the texture
	*/
	for	(i = 0; i < psSampleUnpack->uUnpackInstCount; i++)
	{
		if	(!USPInstBlockInsertHWInst(psSampleUnpack->psInstBlock,
									   IMG_NULL,
									   psSampleUnpack->aeOpcodeForUnpackInsts[i],
									   &psSampleUnpack->asUnpackInsts[i],
									   psSampleUnpack->auInstDescFlagsForUnpackInsts[i],
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("AddSampleUnpackCode: Error	adding HW-inst to block\n"));
			return IMG_FALSE;
		}
	}
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPSampleDataAdd

 Purpose:	Adds the pre-sampled data i.e. required as input-data for the 
			shader

 Inputs:	psSample	- The sample to add data into
            psContext	- The current USP execution context
			psShader	- The USP shader

 Outputs:	none

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPSampleDataAdd(PUSP_SAMPLE	psSample, 
									   PUSP_CONTEXT	psContext, 
									   PUSP_SHADER	psShader)
{
	/*
		Lookup data for the original texture being sampled
	*/
	USP_UNREFERENCED_PARAMETER(psShader);
	/* psSample->sSamplerDesc = psShader->asSamplerChanDesc[psSample->uTextureIdx]; */

	/*
		Pre-calculate how the data for this texture is organised
	*/
	if	(!GetTexChanInfo(psContext,
						 psSample,
						 &psSample->sSamplerDesc,
						 &psSample->sTexChanInfo))
	{
		USP_DBGPRINT(("USPSampleDataAdd: Error calculating texture-info\n"));
		return IMG_FALSE;
	}
	
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPSampleReset

 Purpose:	Reset a dependent or non-dependent sample ready for finalisation

 Inputs:	psSample	- The sample to reset
			psContext	- The USP compilation context
                          (unused here)

 Outputs:	none

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPSampleReset(PUSP_SAMPLE	psSample,
									 PUSP_CONTEXT	psContext)
{
	IMG_UINT32	uCompIdx;
	IMG_UINT32	uChunkIdx;
	
	USP_UNREFERENCED_PARAMETER(psContext);

	/*
		Restore the MOE state expected at the beginning and end of the
		instruction-block for this sample
	*/
	psSample->psInstBlock->sInitialMOEState	= psSample->sMOEState;
	psSample->psInstBlock->sFinalMOEState	= psSample->sMOEState;

	/*
		No HW-instructions generated yet
	*/
	psSample->uSampleInstCount_			= 0;
	psSample->uSampleTempRegCount		= 0;
	psSample->uBaseSampleDestRegCount_	= 0;
	psSample->uUnAlignedTempCount		= 0;

	/*
		Reset unpacking info
	*/
	psSample->bSamplerUnPacked = IMG_FALSE;
	psSample->uUnpackedMask = 0;


	psSample->uNumSMPInsts = 0;
	for	(uCompIdx = 0; uCompIdx < USP_MAX_SAMPLE_CHANS; uCompIdx++)
	{
		psSample->apsSMPInsts[uCompIdx] = IMG_NULL;
	}

	/*
		Restore the original required swizzle (in case we have overridden it
		to minimise unpacking)
	*/
	psSample->uSwizzle = psSample->uOrgSwizzle;

	/*
		Record whether this is dependent or not (in case we have overridden it)
	*/
	psSample->bNonDependent = psSample->bOrgNonDependent;

	/*
		Restore the original base pre-compiled reg-index
	*/
	//psSample->uBaseSampleRegNum = psSample->uOrgBaseSampleRegNum;

	psSample->uTexCtrWrdIdx = (IMG_UINT16)-1;
	for (uChunkIdx = 0; uChunkIdx < USP_MAX_TEXTURE_CHUNKS; uChunkIdx++)
	{
		psSample->abChunkNotSampledToTempReg[uChunkIdx] = IMG_FALSE;
	}

	/*
	  	Reset texture input precision
	 */
	psSample->uTexPrecision = USP_REGFMT_UNKNOWN;
	
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPSampleUnpackReset

 Purpose:	Reset a sample unpack ready for finalisation

 Inputs:	psSampleUnpack
						- The sample unpack to reset
			psContext	- The USP compilation context
                          (unused here)

 Outputs:	none

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPSampleUnpackReset(	PUSP_SAMPLEUNPACK	psSampleUnpack,
											PUSP_CONTEXT		psContext)
{
	USP_UNREFERENCED_PARAMETER(psContext);
	
	psSampleUnpack->psInstBlock->sInitialMOEState	= psSampleUnpack->sMOEState;
	psSampleUnpack->psInstBlock->sFinalMOEState	= psSampleUnpack->sMOEState;
	
	psSampleUnpack->uUnpackInstCount	  = 0;

	/*
		Reset temporary registers used for unpacking.
	*/
	psSampleUnpack->uSampleUnpackTempRegCount = 0;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		ResetMOEStateBeforeSample

 Purpose:	Reset the MOE state prior to a sample if needed

 Inputs:	psSample		- The sample to reset the MOE state for
		    psContext		- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on failure. IMG_TRUE otherwise.
*****************************************************************************/
static IMG_BOOL ResetMOEStateBeforeSample(PUSP_SAMPLE	psSample,
										  PUSP_CONTEXT	psContext)
{
	PUSP_INSTBLOCK	psInstBlock;
	PUSP_MOESTATE	psMOEState;
	HW_INST			sHWInst;
	IMG_BOOL		bAddSMLSI;
	IMG_BOOL		bAddSMBO;
	IMG_UINT32		i;

	psInstBlock	= psSample->psInstBlock;
	psMOEState	= &psInstBlock->sInitialMOEState;

	/*
		Currently, only non-repeating instructions are generated for
		a sample. Check whether the MOE state prior to this sample
		could effect the sampling/unpack code.

		NB: The routines to encode instruction operands can account for
			MOE per-operand format-control state, so we don't have to
			reset that if it's enabled.
	*/
	bAddSMBO	= IMG_FALSE;
	bAddSMLSI	= IMG_FALSE;

	for	(i = 0; i < USP_MOESTATE_OPERAND_COUNT; i++)
	{
		if	(psMOEState->auBaseOffset[i] != 0)
		{
			bAddSMBO = IMG_TRUE;
			break;
		}
	}

	for	(i = 0; i < USP_MOESTATE_OPERAND_COUNT; i++)
	{
		if	(psMOEState->abUseSwiz[i])
		{
			IMG_UINT32	uZeroChan;

			/* Get the swizzle select for the first iteration. */
			uZeroChan = psMOEState->auSwiz[i] & EURASIA_USE_MOESWIZZLE_VALUE_MASK;

			/*
				Is there a change to the register numbers on the first MOE
				iteration.
			*/
			if (uZeroChan != 0)
			{
				bAddSMLSI = IMG_TRUE;
				break;
			}
		}
	}

	/*
		Add an SMBO instruction to the start of the block to reset the MOE
		base/offsets for all operands
	*/
	if	(bAddSMBO)
	{
		static const IMG_UINT32	auDefaultBaseOffsets[USP_MOESTATE_OPERAND_COUNT] =
		{
			0, 0, 0, 0
		};
		IMG_UINT32	uSMBOInstDescFlags;

		/*
			Encode an SMBO instruction to set the base-ofsets to zero for all
			operands
		*/
		if	(!HWInstEncodeSMBOInst(&sHWInst, (IMG_PUINT32)auDefaultBaseOffsets))
		{
			USP_DBGPRINT(("ResetMOEStateBeforeSample: Error encoding SMBO inst\n"));
			return IMG_FALSE;
		}

		/*
			Append the SMBO at the end of the block of code for this sample
		*/
		uSMBOInstDescFlags = psSample->uInstDescFlags &
							 USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
									
		if	(!USPInstBlockInsertHWInst(psInstBlock,
									   IMG_NULL,
									   USP_OPCODE_SMBO,
									   &sHWInst,
									   uSMBOInstDescFlags,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("ResetMOEStateBeforeSample: Error addding inst to block\n"));
			return IMG_FALSE;
		}
	}

	/*
		Add an SMLSI instruction to the start of the block, to reset the
		MOE swizzle/increment state.
	*/
	if	(bAddSMLSI)
	{
		static const IMG_BOOL abUseSwiz[USP_MOESTATE_OPERAND_COUNT] =
		{
			IMG_FALSE, IMG_FALSE, IMG_FALSE, IMG_FALSE
		};
		static const IMG_INT32 aiIncrements[USP_MOESTATE_OPERAND_COUNT] =
		{
			1, 1, 1, 1
		};
		static const IMG_UINT32 auLimits[] =
		{
			0, 0, 0
		};
		IMG_UINT32	uSMLSIInstDescFlags;

		/*
			Encode a SMLSI instruction to use increments of 1 for all operands
		*/
		if	(!HWInstEncodeSMLSIInst(&sHWInst,
									(IMG_PBOOL)abUseSwiz,
									(IMG_PINT32)aiIncrements,
									IMG_NULL,
									(IMG_PUINT32)auLimits))
		{
			USP_DBGPRINT(("ResetMOEStateBeforeSample: Error encoding SMLSI to reset MOE state\n"));
			return IMG_FALSE;
		}

		/*
			Append the SMLSI at the end of the block of code for this sample
		*/
		uSMLSIInstDescFlags = psSample->uInstDescFlags &
							  USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;

		if	(!USPInstBlockInsertHWInst(psInstBlock,
									   IMG_NULL,
									   USP_OPCODE_SMLSI,
									   &sHWInst,
									   uSMLSIInstDescFlags,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("ResetMOEStateBeforeSample: Error adding SMLSI to last block\n"));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		ResetMOEStateBeforeSampleUnpack

 Purpose:	Reset the MOE state prior to a sample unpack if needed

 Inputs:	psSampleUnapck	- The sample unpack to reset the MOE state for
		    psContext		- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on failure. IMG_TRUE otherwise.
*****************************************************************************/
static IMG_BOOL ResetMOEStateBeforeSampleUnpack(	PUSP_SAMPLEUNPACK	psSampleUnpack,
													PUSP_CONTEXT		psContext)
{
	PUSP_INSTBLOCK	psInstBlock;
	PUSP_MOESTATE	psMOEState;
	HW_INST			sHWInst;
	IMG_BOOL		bAddSMLSI;
	IMG_BOOL		bAddSMBO;
	IMG_UINT32		i;

	psInstBlock	= psSampleUnpack->psInstBlock;
	psMOEState	= &psInstBlock->sInitialMOEState;

	/*
		Currently, only non-repeating instructions are generated for
		a sample unpack. Check whether the MOE state prior to this sample unpack
		could effect the sampling unpack code.

		NB: The routines to encode instruction operands can account for
			MOE per-operand format-control state, so we don't have to
			reset that if it's enabled.
	*/
	bAddSMBO	= IMG_FALSE;
	bAddSMLSI	= IMG_FALSE;

	for	(i = 0; i < USP_MOESTATE_OPERAND_COUNT; i++)
	{
		if	(psMOEState->auBaseOffset[i] != 0)
		{
			bAddSMBO = IMG_TRUE;
			break;
		}
	}

	for	(i = 0; i < USP_MOESTATE_OPERAND_COUNT; i++)
	{
		if	(psMOEState->abUseSwiz[i])
		{
			IMG_UINT32	uZeroChan;

			/* Get the swizzle select for the first iteration. */
			uZeroChan = psMOEState->auSwiz[i] & EURASIA_USE_MOESWIZZLE_VALUE_MASK;

			/*
				Is there a change to the register numbers on the first MOE
				iteration.
			*/
			if (uZeroChan != 0)
			{
				bAddSMLSI = IMG_TRUE;
				break;
			}
		}
	}

	/*
		Add an SMBO instruction to the start of the block to reset the MOE
		base/offsets for all operands
	*/
	if	(bAddSMBO)
	{
		static const IMG_UINT32	auDefaultBaseOffsets[USP_MOESTATE_OPERAND_COUNT] =
		{
			0, 0, 0, 0
		};
		IMG_UINT32	uSMBOInstDescFlags;

		/*
			Encode an SMBO instruction to set the base-ofsets to zero for all
			operands
		*/
		if	(!HWInstEncodeSMBOInst(&sHWInst, (IMG_PUINT32)auDefaultBaseOffsets))
		{
			USP_DBGPRINT(("ResetMOEStateBeforeSampleUnpack: Error encoding SMBO inst\n"));
			return IMG_FALSE;
		}

		/*
			Append the SMBO at the end of the block of code for this sample unpack
		*/
		uSMBOInstDescFlags = psSampleUnpack->uInstDescFlags &
							 USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
									
		if	(!USPInstBlockInsertHWInst(psInstBlock,
									   IMG_NULL,
									   USP_OPCODE_SMBO,
									   &sHWInst,
									   uSMBOInstDescFlags,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("ResetMOEStateBeforeSampleUnpack: Error addding inst to block\n"));
			return IMG_FALSE;
		}
	}

	/*
		Add an SMLSI instruction to the start of the block, to reset the
		MOE swizzle/increment state.
	*/
	if	(bAddSMLSI)
	{
		static const IMG_BOOL abUseSwiz[USP_MOESTATE_OPERAND_COUNT] =
		{
			IMG_FALSE, IMG_FALSE, IMG_FALSE, IMG_FALSE
		};
		static const IMG_INT32 aiIncrements[USP_MOESTATE_OPERAND_COUNT] =
		{
			1, 1, 1, 1
		};
		static const IMG_UINT32 auLimits[] =
		{
			0, 0, 0
		};
		IMG_UINT32	uSMLSIInstDescFlags;

		/*
			Encode a SMLSI instruction to use increments of 1 for all operands
		*/
		if	(!HWInstEncodeSMLSIInst(&sHWInst,
									(IMG_PBOOL)abUseSwiz,
									(IMG_PINT32)aiIncrements,
									IMG_NULL,
									(IMG_PUINT32)auLimits))
		{
			USP_DBGPRINT(("ResetMOEStateBeforeSampleUnpack: Error encoding SMLSI to reset MOE state\n"));
			return IMG_FALSE;
		}

		/*
			Append the SMLSI at the end of the block of code for this sample unpack
		*/
		uSMLSIInstDescFlags = psSampleUnpack->uInstDescFlags &
							  USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;

		if	(!USPInstBlockInsertHWInst(psInstBlock,
									   IMG_NULL,
									   USP_OPCODE_SMLSI,
									   &sHWInst,
									   uSMLSIInstDescFlags,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("ResetMOEStateBeforeSampleUnpack: Error adding SMLSI to last block\n"));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		RestoreMOEStateAfterSample

 Purpose:	Restore the MOE state after to a sample if needed

 Inputs:	psSample	- The sample to restore the MOE state for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on failure. IMG_TRUE otherwise.
*****************************************************************************/
static IMG_BOOL RestoreMOEStateAfterSample(PUSP_SAMPLE	psSample,
										   PUSP_CONTEXT	psContext)
{
	PUSP_INSTBLOCK	psInstBlock;
	PUSP_MOESTATE	psMOEState;
	PUSP_MOESTATE	psOrgMOEState;
	IMG_UINT32		uInstDescFlags;
	HW_INST			sHWInst;
	IMG_BOOL		bAddSMLSI;
	IMG_BOOL		bAddSMBO;
	IMG_BOOL		bAddSETFC;
	IMG_UINT32		i;

	/*
		Compare the original MOE state prior to the sample with the state
		used, to determine whether we need to restore it.
	*/
	psInstBlock		= psSample->psInstBlock;
	psMOEState		= &psInstBlock->sFinalMOEState;
	psOrgMOEState	= &psInstBlock->sInitialMOEState;
	bAddSMLSI		= IMG_FALSE;
	bAddSMBO		= IMG_FALSE;
	bAddSETFC		= IMG_FALSE;

	for	(i = 0; i < USP_MOESTATE_OPERAND_COUNT; i++)
	{
		if	(psOrgMOEState->abUseSwiz[i] != psMOEState->abUseSwiz[i])
		{
			bAddSMLSI = IMG_TRUE;
		}

		if	(psMOEState->abUseSwiz[i])
		{
			if	(psOrgMOEState->auSwiz[i] != psMOEState->auSwiz[i])
			{
				bAddSMLSI = IMG_TRUE;
			}
		}
		else
		{
			if	(psOrgMOEState->aiInc[i] != psMOEState->aiInc[i])
			{
				bAddSMLSI = IMG_TRUE;
			}
		}

		if	(psOrgMOEState->auBaseOffset[i] != psMOEState->auBaseOffset[i])
		{
			bAddSMBO = IMG_TRUE;
		}
	}

	if	(
			(psOrgMOEState->bColFmtCtl != psMOEState->bColFmtCtl) ||
			(psOrgMOEState->bEFOFmtCtl != psMOEState->bEFOFmtCtl)
		)
	{
		bAddSETFC = IMG_TRUE;
	}

	if	(!(bAddSETFC || bAddSMBO || bAddSMLSI))
	{
		return IMG_TRUE;
	}

	/*
		If the IRegs are live before the sample, or the sample writes to
		an IReg destination, then the IRegs must be live before the MOE
		insts too
	*/
	uInstDescFlags = psSample->uInstDescFlags & 
					 USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;

	for	(i = 0; i < USP_MAX_SAMPLE_CHANS; i++)
	{
		if	(psSample->eBaseSampleDestRegType == USP_REGTYPE_INTERNAL)
		{
			uInstDescFlags |= USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
			break;
		}
	}

	/*
		Restore the MOE base-offsets for all operands if needed
	*/
	if	(bAddSMBO)
	{
		/*
			Encode an SMBO instruction to restore the base-ofsets for all operands
		*/
		if	(!HWInstEncodeSMBOInst(&sHWInst, psOrgMOEState->auBaseOffset))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSample: Error encoding SMBO inst to restore MOE state\n"));
			return IMG_FALSE;
		}

		/*
			Append the SMBO at the end of the block of code for this sample
		*/
		if	(!USPInstBlockInsertHWInst(psInstBlock,
									   IMG_NULL,
									   USP_OPCODE_SMBO,
									   &sHWInst,
									   uInstDescFlags,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSample: Error addding inst to int-block\n"));
			return IMG_FALSE;
		}
	}

	/*
		Restore the MOE swizzle/increment state for all operands if needed
	*/
	if	(bAddSMLSI)
	{
		static const IMG_UINT32 auLimits[] =
		{
			0, 0, 0
		};

		/*
			Encode a SMLSI instruction to restore the increment/swizzle mode
			for all operands
		*/
		if	(!HWInstEncodeSMLSIInst(&sHWInst,
									psOrgMOEState->abUseSwiz,
									psOrgMOEState->aiInc,
									psOrgMOEState->auSwiz,
									(IMG_PUINT32)auLimits))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSample: Error encoding SMLSI to restore MOE state\n"));
			return IMG_FALSE;
		}

		/*
			Append the SMLSI at the end of the block of code for this sample
		*/
		if	(!USPInstBlockInsertHWInst(psInstBlock,
									   IMG_NULL,
									   USP_OPCODE_SMLSI,
									   &sHWInst,
									   uInstDescFlags,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSample: Error adding SMLSI to inst-block\n"));
			return IMG_FALSE;
		}
	}

	/*
		Restore the original format-control state
	*/
	if	(bAddSETFC)
	{
		/*
			Encode a SETFC instruction to restore the EFO/colour format-control
		*/
		if	(!HWInstEncodeSETFCInst(&sHWInst,
									psOrgMOEState->bColFmtCtl,
									psOrgMOEState->bEFOFmtCtl))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSample: Error encoding SETFC to restore MOE state\n"));
			return IMG_FALSE;
		}

		/*
			Append the SETFC at the end of the block of code for this sample
		*/
		if	(!USPInstBlockInsertHWInst(psInstBlock,
									   IMG_NULL,
									   USP_OPCODE_SETFC,
									   &sHWInst,
									   uInstDescFlags,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSample: Error adding SETFC to inst-block\n"));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		RestoreMOEStateAfterSampleUnpack

 Purpose:	Restore the MOE state after to a sample unpack if needed

 Inputs:	psSampleUnpack
						- The sample unpack to restore the MOE state for
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on failure. IMG_TRUE otherwise.
*****************************************************************************/
static IMG_BOOL RestoreMOEStateAfterSampleUnpack(	PUSP_SAMPLEUNPACK	psSampleUnpack,
													PUSP_CONTEXT		psContext)
{
	PUSP_INSTBLOCK	psInstBlock;
	PUSP_MOESTATE	psMOEState;
	PUSP_MOESTATE	psOrgMOEState;
	IMG_UINT32		uInstDescFlags;
	HW_INST			sHWInst;
	IMG_BOOL		bAddSMLSI;
	IMG_BOOL		bAddSMBO;
	IMG_BOOL		bAddSETFC;
	IMG_UINT32		i;

	/*
		Compare the original MOE state prior to the sample unpack with the state
		used, to determine whether we need to restore it.
	*/
	psInstBlock		= psSampleUnpack->psInstBlock;
	psMOEState		= &psInstBlock->sFinalMOEState;
	psOrgMOEState	= &psInstBlock->sInitialMOEState;
	bAddSMLSI		= IMG_FALSE;
	bAddSMBO		= IMG_FALSE;
	bAddSETFC		= IMG_FALSE;

	for	(i = 0; i < USP_MOESTATE_OPERAND_COUNT; i++)
	{
		if	(psOrgMOEState->abUseSwiz[i] != psMOEState->abUseSwiz[i])
		{
			bAddSMLSI = IMG_TRUE;
		}

		if	(psMOEState->abUseSwiz[i])
		{
			if	(psOrgMOEState->auSwiz[i] != psMOEState->auSwiz[i])
			{
				bAddSMLSI = IMG_TRUE;
			}
		}
		else
		{
			if	(psOrgMOEState->aiInc[i] != psMOEState->aiInc[i])
			{
				bAddSMLSI = IMG_TRUE;
			}
		}

		if	(psOrgMOEState->auBaseOffset[i] != psMOEState->auBaseOffset[i])
		{
			bAddSMBO = IMG_TRUE;
		}
	}

	if	(
			(psOrgMOEState->bColFmtCtl != psMOEState->bColFmtCtl) ||
			(psOrgMOEState->bEFOFmtCtl != psMOEState->bEFOFmtCtl)
		)
	{
		bAddSETFC = IMG_TRUE;
	}

	if	(!(bAddSETFC || bAddSMBO || bAddSMLSI))
	{
		return IMG_TRUE;
	}

	/*
		If the IRegs are live before the sample unpack, or the sample unpack writes to
		an IReg destination, then the IRegs must be live before the MOE
		insts too
	*/
	uInstDescFlags = psSampleUnpack->uInstDescFlags & 
					 USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;

	for	(i = 0; i < USP_MAX_SAMPLE_CHANS; i++)
	{
		if	(psSampleUnpack->asDest[i].eType == USP_REGTYPE_INTERNAL)
		{
			uInstDescFlags |= USP_INSTDESC_FLAG_IREGS_LIVE_BEFORE;
			break;
		}
	}

	/*
		Restore the MOE base-offsets for all operands if needed
	*/
	if	(bAddSMBO)
	{
		/*
			Encode an SMBO instruction to restore the base-ofsets for all operands
		*/
		if	(!HWInstEncodeSMBOInst(&sHWInst, psOrgMOEState->auBaseOffset))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSampleUnpack: Error encoding SMBO inst to restore MOE state\n"));
			return IMG_FALSE;
		}

		/*
			Append the SMBO at the end of the block of code for this sample unpack
		*/
		if	(!USPInstBlockInsertHWInst(psInstBlock,
									   IMG_NULL,
									   USP_OPCODE_SMBO,
									   &sHWInst,
									   uInstDescFlags,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSampleUnpack: Error addding inst to int-block\n"));
			return IMG_FALSE;
		}
	}

	/*
		Restore the MOE swizzle/increment state for all operands if needed
	*/
	if	(bAddSMLSI)
	{
		static const IMG_UINT32 auLimits[] =
		{
			0, 0, 0
		};

		/*
			Encode a SMLSI instruction to restore the increment/swizzle mode
			for all operands
		*/
		if	(!HWInstEncodeSMLSIInst(&sHWInst,
									psOrgMOEState->abUseSwiz,
									psOrgMOEState->aiInc,
									psOrgMOEState->auSwiz,
									(IMG_PUINT32)auLimits))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSampleUnpack: Error encoding SMLSI to restore MOE state\n"));
			return IMG_FALSE;
		}

		/*
			Append the SMLSI at the end of the block of code for this sample
		*/
		if	(!USPInstBlockInsertHWInst(psInstBlock,
									   IMG_NULL,
									   USP_OPCODE_SMLSI,
									   &sHWInst,
									   uInstDescFlags,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSampleUnpack: Error adding SMLSI to inst-block\n"));
			return IMG_FALSE;
		}
	}

	/*
		Restore the original format-control state
	*/
	if	(bAddSETFC)
	{
		/*
			Encode a SETFC instruction to restore the EFO/colour format-control
		*/
		if	(!HWInstEncodeSETFCInst(&sHWInst,
									psOrgMOEState->bColFmtCtl,
									psOrgMOEState->bEFOFmtCtl))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSampleUnpack: Error encoding SETFC to restore MOE state\n"));
			return IMG_FALSE;
		}

		/*
			Append the SETFC at the end of the block of code for this sample unpack
		*/
		if	(!USPInstBlockInsertHWInst(psInstBlock,
									   IMG_NULL,
									   USP_OPCODE_SETFC,
									   &sHWInst,
									   uInstDescFlags,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("RestoreMOEStateAfterSampleUnpack: Error adding SETFC to inst-block\n"));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPSampleGenerateCode

 Purpose:	Generate instructions for a texture-sample

 Inputs:	psSample	- The sample to generate code for,
			psShader	- The USP shader containing the sample
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPSampleGenerateCode(PUSP_SAMPLE		psSample,
											PUSP_SHADER		psShader,
											PUSP_CONTEXT	psContext)
{
	PUSP_INPUT_DATA	psInputData;

	psInputData	= psShader->psInputData;

	/*
		Ignore samples that don't do anything.
	*/
	if	(!psSample->psSampleUnpack->uMask)
	{
		return IMG_TRUE;
	}

	/*
		Reset the MOE state if needed
	*/
	if	(!ResetMOEStateBeforeSample(psSample, psContext))
	{
		USP_DBGPRINT(("USPSampleGenerateCode: Error resetting MOE state\n"));
		return IMG_FALSE;
	}

	/*
		Is this a dependent or non-dependent sample?
	*/
	{
		/*
			Add instructions to sample the texture data		
		*/
		if	(!SampleTextureData(psSample, psInputData, psContext))
		{
			USP_DBGPRINT(("USPSampleGenerateCode: Error sampling texture-data\n"));
			return IMG_FALSE;
		}
	}
	{
		/*
			Get the location of the pre-sampled chunks of texture data needed
			by this non-dependent sample
		*/
		if	(!FindPreSampledTextureData(psSample, psInputData, psContext))
		{
			USP_DBGPRINT(("USPSampleGenerateCode: Error finding pre-sampled texture-data\n"));
			return IMG_FALSE;
		}
	}

	SetupChunkRegsForUnpackPhase(psSample);

	/*
		Add instructions to perform colour-space conversion if needed
	*/
	if	(!ColourSpaceConvertTextureData(psSample, psContext))
	{
		USP_DBGPRINT(("USPSampleGenerateCode: Error colour-space converting texture-data\n"));
		return IMG_FALSE;
	}

	/*
		Setup the source data location for each channel of the result
	*/
	if	(!SetupChanSrcRegs(psSample, psContext))
	{
		USP_DBGPRINT(("USPSampleGenerateCode: Error calculating src data regs\n"));
		return IMG_FALSE;
	}

	/*
		Add instructions to copy and convert the sampled data into the
		required destination registers.
	*/
	if	(!UnpackTextureData(psSample, psContext))
	{
		USP_DBGPRINT(("USPSampleGenerateCode: Error unpacking texture-data\n"));
		return IMG_FALSE;
	}

	/*
		Add the generated HW-instructions to the associated USP instruction
		block
	*/
	if	(!AddSampleCode(psSample, psContext))
	{
		USP_DBGPRINT(("USPSampleGenerateCode: Error adding HW-insts to block\n"));
		return IMG_FALSE;
	}

	/*
		Restore the expected MOE state if we reset it before
	*/
	if	(!RestoreMOEStateAfterSample(psSample, psContext))
	{
		USP_DBGPRINT(("USPSampleGenerateCode: Error restoring MOE state\n"));
		return IMG_FALSE;
	}	

	return IMG_TRUE;
}


/*****************************************************************************
 Name:		USPSampleUnpackGenerateCode

 Purpose:	Generate instructions for a sample unpack

 Inputs:	psSampleUnpack
						- The sample unpack to generate code for,
			psShader	- The USP shader containing the sample
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPSampleUnpackGenerateCode(	PUSP_SAMPLEUNPACK	psSampleUnpack,
													PUSP_SHADER			psShader,
													PUSP_CONTEXT		psContext)
{
	PUSP_INPUT_DATA	psInputData;

	psInputData	= psShader->psInputData;

	/*
		Ignore sample unpacks that don't do anything.
	*/
	if	(!psSampleUnpack->uMask)
	{
		return IMG_TRUE;
	}

	/*
		Reset the MOE state if needed
	*/
	if	(!ResetMOEStateBeforeSampleUnpack(psSampleUnpack, psContext))
	{
		USP_DBGPRINT(("USPSampleUnpackGenerateCode: Error resetting MOE state\n"));
		return IMG_FALSE;
	}

	/*
		Add the generated HW-instructions to the associated sample unpack block
	*/
	if	(!AddSampleUnpackCode(psSampleUnpack, psContext))
	{
		USP_DBGPRINT(("USPSampleUnpackGenerateCode: Error adding HW-insts to associated sample unpack block\n"));
		return IMG_FALSE;
	}

	/*
		Restore the expected MOE state if we reset it before
	*/
	if	(!RestoreMOEStateAfterSampleUnpack(psSampleUnpack, psContext))
	{
		USP_DBGPRINT(("USPSampleUnpackGenerateCode: Error restoring MOE state\n"));
		return IMG_FALSE;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPSampleCreate

 Purpose:	Create and initialise USP sample info

 Inputs:	psContext	- The current USP context
			psShader	- The shader that the sample-block is for
			psMOEState	- The MOE state prior to the sample

 Outputs:	none

 Returns:	The created USP sample data
*****************************************************************************/
IMG_INTERNAL PUSP_SAMPLE USPSampleCreate(PUSP_CONTEXT	psContext,
										 PUSP_SHADER	psShader,
										 PUSP_MOESTATE	psMOEState)
{
	PUSP_SAMPLE		psSample;
	PUSP_INSTBLOCK	psInstBlock;
	IMG_BOOL		bFailed;

	/*
		Sample not created yet
	*/
	bFailed		= IMG_TRUE;
	psInstBlock	= IMG_NULL;
	psSample	= IMG_NULL;

	/*
		Allocate a block of instructions large enough to hold the maximum
		number of instructions needed for a sample:

		+ Max number of instructions to sample the texture
		+ Max number of instructions to unpack/copy the sampled data to the destination
		+ 6 instructions for MOE state changes (worst case)
		+ 1 instruction for a NOP to support sync-start
		+ 2 instructions for nops to support nosched
	*/
	psInstBlock = USPInstBlockCreate(psContext,
									 psShader,
									 USP_SAMPLE_MAX_SAMPLE_INSTS +									 
									 6 + 1 + 2,
									 0,
									 IMG_TRUE,
									 psMOEState);
	if	(!psInstBlock)
	{
		USP_DBGPRINT(( "USPSampleCreate: Failed to alloc insts\n"));
		goto USPSampleCreateExit;
	}

	/*
		Allocate and initialise the sample data
	*/
	psSample = (PUSP_SAMPLE)psContext->pfnAlloc(sizeof(USP_SAMPLE));
	if	(!psSample)
	{
		USP_DBGPRINT(( "USPSampleCreate: Failed to alloc USP sample data\n"));
		goto USPSampleCreateExit;
	}
	memset(psSample, 0, sizeof(USP_SAMPLE));

	/*
		Reset unpacking info
	*/
	psSample->bSamplerUnPacked = IMG_FALSE;
	psSample->uUnpackedMask = 0;

	psSample->psShader		= psShader;
	psSample->psInstBlock	= psInstBlock;
	psSample->sMOEState		= *psMOEState;
	psSample->uTexCtrWrdIdx = (IMG_UINT16)-1;

	/*
		Sample created OK
	*/
	bFailed = IMG_FALSE;

	USPSampleCreateExit:

	/*
		Cleanup on failure
	*/
	if	(bFailed)
	{
		if	(psInstBlock)
		{
			USPInstBlockDestroy(psInstBlock, psContext);
		}
		if	(psSample)
		{
			psContext->pfnFree(psSample);
			psSample = IMG_NULL;
		}
	}

	return psSample;
}

/*****************************************************************************
 Name:		USPSampleDestroy

 Purpose:	Destroy USP sample data previously created using USPSampleCreate

 Inputs:	psSample	- The USP sample data to be destroyed
			psContext	- The current USP context

 Outputs:	none

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_VOID USPSampleDestroy(PUSP_SAMPLE	psSample,
									   PUSP_CONTEXT	psContext)
{
	/*
		We only destroy the sample info, not the associated instruction block.
		This is destroyed along with all the others for the shader.
	*/
	psContext->pfnFree(psSample);
}

/*****************************************************************************
 Name:		USPSampleUnpackCreate

 Purpose:	Create and initialise USP sample unpack info

 Inputs:	psContext	- The current USP context
			psShader	- The shader that the sample unpack-block is for
			psMOEState	- The MOE state prior to the sample unpack

 Outputs:	none

 Returns:	The created USP sample unpack data
*****************************************************************************/
IMG_INTERNAL PUSP_SAMPLEUNPACK USPSampleUnpackCreate(	PUSP_CONTEXT	psContext,
														PUSP_SHADER		psShader,
														PUSP_MOESTATE	psMOEState)
{
	PUSP_SAMPLEUNPACK	psSampleUnpack;
	PUSP_INSTBLOCK		psInstBlock;
	IMG_BOOL			bFailed;

	/*
		Sample not created yet
	*/
	bFailed			= IMG_TRUE;
	psInstBlock		= IMG_NULL;
	psSampleUnpack	= IMG_NULL;

	/*
		Allocate a block of instructions large enough to hold the maximum
		number of instructions needed for a sample unpack :
		
		+ Max number of instructions to unpack/copy the sampled data to the destination
		+ 6 instructions for MOE state changes (worst case)
		+ 1 instruction for a NOP to support sync-start
		+ 2 instructions for nops to support nosched
	*/
	psInstBlock = USPInstBlockCreate(psContext,
									 psShader,									 
									 USP_SAMPLE_MAX_UNPACK_INSTS +
									 6 + 1 + 2,
									 0,
									 IMG_TRUE,
									 psMOEState);
	if	(!psInstBlock)
	{
		USP_DBGPRINT(( "USPSampleUnpackCreate: Failed to alloc insts\n"));
		goto USPSampleUnpackCreateExit;
	}

	/*
		Allocate and initialise the sample data
	*/
	psSampleUnpack = (PUSP_SAMPLEUNPACK)psContext->pfnAlloc(sizeof(USP_SAMPLEUNPACK));
	if	(!psSampleUnpack)
	{
		USP_DBGPRINT(( "USPSampleUnpackCreate: Failed to alloc USP sample unpack data\n"));
		goto USPSampleUnpackCreateExit;
	}
	memset(psSampleUnpack, 0, sizeof(USP_SAMPLEUNPACK));

	/*
		Reset unpacking info
	*/
	psSampleUnpack->bSamplerUnPacked = IMG_FALSE;
	psSampleUnpack->uUnpackedMask = 0;

	psSampleUnpack->psShader		= psShader;
	psSampleUnpack->psInstBlock		= psInstBlock;
	psSampleUnpack->sMOEState		= *psMOEState;

	/*
		Sample created OK
	*/
	bFailed = IMG_FALSE;

	USPSampleUnpackCreateExit:

	/*
		Cleanup on failure
	*/
	if	(bFailed)
	{
		if	(psInstBlock)
		{
			USPInstBlockDestroy(psInstBlock, psContext);
		}
		if	(psSampleUnpack)
		{
			psContext->pfnFree(psSampleUnpack);
			psSampleUnpack = IMG_NULL;
		}
	}

	return psSampleUnpack;
}

/*****************************************************************************
 Name:		USPSampleUnpackDestroy

 Purpose:	Destroy USP sample unpack data previously created using 
			USPSampleUnpackCreate

 Inputs:	psSampleUnpack
						- The USP sample unpack data to be destroyed
			psContext	- The current USP context

 Outputs:	none

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_VOID USPSampleUnpackDestroy(	PUSP_SAMPLEUNPACK	psSampleUnpack,
												PUSP_CONTEXT		psContext)
{
	/*
		We only destroy the sample unpack info, not the associated instruction block.
		This is destroyed along with all the others for the shader.
	*/
	psContext->pfnFree(psSampleUnpack);
}

/* 
	Code to Sample Non Dependent Samples using available PDS Command List 
	Entries and PA registers. 
*/
static const IMG_UINT32		auChunkMaskToPAsCount[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
static const IMG_UINT32		auFirstSetBit[16] = {0, 1, 2, 1, 4, 1, 2, 1, 8, 1, 2, 1, 4, 1, 2, 1};
static const IMG_UINT32		auIgnoreFirstSetBit[16] = {0, 0, 0, 2, 0, 4, 4, 6, 0, 8, 8, 10, 8, 12, 12, 14};

/*
	Data describing Coordinate dimensions etc. 
*/
typedef struct _COORDINATE_DETAILS_
{
	USP_ITERATED_DATA_TYPE	eCoord;
	IMG_UINT32				uCoordDimensions;
	IMG_BOOL				bCentroid;
} COORDINATE_DETAILS;

/*
	Data describing Coordinate fetching etc. 
*/
typedef struct _COORDINATE_INFO_
{
	IMG_BOOL				bIteratedByUSC;
	IMG_UINT32				uPARegsUsedByCoordinate;
	union
	{
		PUSP_ITERATED_DATA		psCoordIteratedData;
		COORDINATE_DETAILS		sCoordinateDetails;
	}u;
} COORDINATE_INFO;

/*
	Data describing Texture Chunks and all the Sample 
	Blocks needing those chunks.
*/
typedef struct _TEX_CHUNKS_SAMPLES_
{
	IMG_UINT32				uTexIdx;
	IMG_BOOL				bProjected;
	IMG_UINT32				uChunksMask;
	IMG_UINT32				uNonDepChunksMask;
	IMG_UINT32				uPAOrEntriesCount;
	SAMPLES_LIST*			psChunksSamples;	
	PUSP_SAMPLE				psReservedChunkSample;
	IMG_UINT32				uReservedChunkBit;
	IMG_BOOL				bAllChunksDepOrNDep;
} TEX_CHUNKS_SAMPLES;

/*
	Data to list all Texture Chunks belonging to 
	particular Coordinate group.
*/
typedef struct _TEX_CHUNKS_SAMPLES_LIST_
{
	TEX_CHUNKS_SAMPLES		sTexChunksSamples;
	struct _TEX_CHUNKS_SAMPLES_LIST_* 
							psNext;
} TEX_CHUNKS_SAMPLES_LIST;

/*
	Data decribing Texture Chunks and Samples Group of 
	particular Coordinate.
*/
typedef struct _TEX_CHUNKS_SAMPLES_GROUP
{
	COORDINATE_INFO			sCoordinateInfo;
	TEX_CHUNKS_SAMPLES_LIST*
							psTexChunksSamplesList;
	IMG_UINT32				uTexChnksSmpsPAOrEntrsCount;	//count of pa or command list entries needed by all the Texture Chunks Samples of a group	
} TEX_CHUNKS_SAMPLES_GROUP;

/*
	Data decribing cost interms of PA register and PDS Commmand List Entries 
	needed to patch the Shader.
*/
typedef struct _TEX_CHUNKS_SAMPLES_GROUPS_COST_
{
	IMG_UINT32	uListEntriesUsedCount;
	IMG_UINT32	uPARegsNeededCount;
} TEX_CHUNKS_SAMPLES_GROUPS_COST;

static IMG_VOID AddSampleToTexChunksSamples(
											TEX_CHUNKS_SAMPLES*			psTexChunksSamples, 
											PUSP_SAMPLE					psSample, 
											SAMPLES_LIST**				ppsFreeSampleListEntry)
/*****************************************************************************
 Name:		AddSampleToTexChunksSamplesGroup

 Purpose:	Adds Sample to Texture Chunks and Samples list.

 Inputs:	psTexChunksSamples 
						- Texture Chunks and Samples list.
			psSample	- Sample to add.			
			ppsFreeSampleListEntry 
						- Next free Samples List entry.		    

 Outputs:	None.

 Returns:	Nothing.
*****************************************************************************/
{
	(*ppsFreeSampleListEntry)->psSample = psSample;
	(*ppsFreeSampleListEntry)->psNext = IMG_NULL;
	if(psTexChunksSamples->psChunksSamples == IMG_NULL)
	{
		psTexChunksSamples->psChunksSamples = (*ppsFreeSampleListEntry);
	}
	else
	{
		SAMPLES_LIST*	psLastSamplesListEntry = psTexChunksSamples->psChunksSamples;
		while(psLastSamplesListEntry->psNext != IMG_NULL)
		{
			psLastSamplesListEntry = psLastSamplesListEntry->psNext;
		}
		psLastSamplesListEntry->psNext = (*ppsFreeSampleListEntry);
	}
	(psTexChunksSamples->uChunksMask) |= (psSample->sTexChanInfo.uTexChunkMask);
	(*ppsFreeSampleListEntry)++;	
	return;
}

static IMG_VOID AddSampleToTexChunksSamplesGroup(
													TEX_CHUNKS_SAMPLES_GROUP* 
																		psTexChunksSamplesGroup, 
													PUSP_SAMPLE			psSample, 
													TEX_CHUNKS_SAMPLES_LIST** 
																		ppsFreeTexChunksSamplesListEntry, 
													SAMPLES_LIST**		ppsFreeSampleListEntry)
/*****************************************************************************
 Name:		AddSampleToTexChunksSamplesGroup

 Purpose:	Adds Sample to Texture Chunks and Samples Group.

 Inputs:	psTexChunksSamplesGroups 
						- Texture Chunks and Samples group.
			psSample	- Sample to add.
			ppsFreeTexChunksSamplesListEntry 
						- Next free Texture Chunks and Samples List entry.
			ppsFreeSampleListEntry 
						- Next free Samples List entry.		    

 Outputs:	None.

 Returns:	Nothing.
*****************************************************************************/
{
	TEX_CHUNKS_SAMPLES_LIST*	psTexChunksSamplesListEntry;
	TEX_CHUNKS_SAMPLES_LIST*	psPrevTexChunksSamplesListEntry;
	if(!(psSample->sTexChanInfo.uTexChunkMask))
	{
		return;
	}
	psTexChunksSamplesListEntry = psTexChunksSamplesGroup->psTexChunksSamplesList;
	psPrevTexChunksSamplesListEntry = psTexChunksSamplesListEntry;
	while(psTexChunksSamplesListEntry)
	{
		if(
			((psTexChunksSamplesListEntry->sTexChunksSamples.uTexIdx) == (psSample->uTextureIdx)) && 
			((psTexChunksSamplesListEntry->sTexChunksSamples.bProjected) == (psSample->bProjected))
		)
		{
			AddSampleToTexChunksSamples(&(psTexChunksSamplesListEntry->sTexChunksSamples), psSample, ppsFreeSampleListEntry);
			return;
		}
		psPrevTexChunksSamplesListEntry = psTexChunksSamplesListEntry;
		psTexChunksSamplesListEntry = psTexChunksSamplesListEntry->psNext;
	}
	{
		psTexChunksSamplesListEntry				= *ppsFreeTexChunksSamplesListEntry;		
		(*ppsFreeTexChunksSamplesListEntry)++;
		psTexChunksSamplesListEntry->psNext = IMG_NULL;
		if(psPrevTexChunksSamplesListEntry != IMG_NULL)
		{
			psPrevTexChunksSamplesListEntry->psNext = psTexChunksSamplesListEntry;
		}
		else
		{
			psTexChunksSamplesGroup->psTexChunksSamplesList = psTexChunksSamplesListEntry;
		}
		{
			TEX_CHUNKS_SAMPLES*	psTexChunksSamples		= &(psTexChunksSamplesListEntry->sTexChunksSamples);		
			psTexChunksSamples->uTexIdx					= psSample->uTextureIdx;
			psTexChunksSamples->bProjected				= psSample->bProjected;
			psTexChunksSamples->uChunksMask				= 0;
			psTexChunksSamples->uNonDepChunksMask		= 0;
			psTexChunksSamples->psChunksSamples			= IMG_NULL;
			psTexChunksSamples->psReservedChunkSample	= psSample;
			psTexChunksSamples->uReservedChunkBit		= auFirstSetBit[psSample->sTexChanInfo.uTexChunkMask];
			if(psSample->sTexChanInfo.eTexType != USP_TEX_TYPE_YUV)
			{
				psTexChunksSamples->bAllChunksDepOrNDep	= IMG_FALSE;
			}
			else
			{
				psTexChunksSamples->bAllChunksDepOrNDep	= IMG_TRUE;
			}
			psSample->sTexChanInfo.uTexChunkMask		= auIgnoreFirstSetBit[psSample->sTexChanInfo.uTexChunkMask];			
		}
		if(psSample->sTexChanInfo.uTexChunkMask)
		{
			AddSampleToTexChunksSamples(&(psTexChunksSamplesListEntry->sTexChunksSamples), psSample, ppsFreeSampleListEntry);			
		}
		return;
	}
}
/*
	To point to memory allocated for Chunks Sampling Decision data.
*/
typedef struct _CHUNKS_DECISION_DATA_
{
	TEX_CHUNKS_SAMPLES_GROUP*	asTexChunksSamplesGroups;
	TEX_CHUNKS_SAMPLES_LIST*	asTexChunksSamplesListEntries;
	SAMPLES_LIST*				asSampleListEntries;
}CHUNKS_DECISION_DATA;

static IMG_BOOL GroupTexChunksSamplesByTexCoords(PUSP_CONTEXT		psContext, 
												 PUSP_SHADER		psShader, 
												 TEX_CHUNKS_SAMPLES_GROUP**	
																	ppsTexChunksSamplesGroups, 
												 IMG_UINT32*		puTexChunksSamplesGroupsCount, 
												 CHUNKS_DECISION_DATA* 
																	psChunksDecisionData)
/*****************************************************************************
 Name:		GroupTexChunksSamplesByTexCoords

 Purpose:	Groups Texture Chunks and Samples by Texture Coordinates.

 Inputs:	psContext	- The current USP context.
			psShader	- The shader contaning the sample blocks.
			ppsTexChunksSamplesGroups 
						- Will point to Texture Chunks and Samples groups.
			puTexChunksSamplesGroupsCount 
						- Will contain Texture Chunks and Samples groups 
						  Count.
			psTexChunksSamplesGroupsCost 
						- Will contain the PA registers and List Entries 
						  usage count.
		    psChunksDecisionData 
						- Will contain pointer to memory allocated for 
						  Chunks Decision data.

 Outputs:	None.

 Returns:	Nothing.
*****************************************************************************/
{
	PUSP_SAMPLE		psSample;
	TEX_CHUNKS_SAMPLES_GROUP* 
					asTexChunksSamplesGroups;
	TEX_CHUNKS_SAMPLES_LIST* 
					asTexChunksSamplesListEntries;
	TEX_CHUNKS_SAMPLES_LIST* 
					psFreeTexChunksSamplesListEntry;
	SAMPLES_LIST*	asSampleListEntries;
	SAMPLES_LIST*	psFreeSampleListEntry;
	IMG_UINT32		uTexChunksSamplesGroupsCount = 0;	
	{
		IMG_UINT32	uSamplesCount = 0;
		for(psSample = psShader->psNonDepSamples; 
			psSample; 
			psSample = psSample->psNext)
		{
			uSamplesCount++;
		}
		if(uSamplesCount)
		{
			asTexChunksSamplesGroups = (TEX_CHUNKS_SAMPLES_GROUP*)psContext->pfnAlloc(uSamplesCount * sizeof(TEX_CHUNKS_SAMPLES_GROUP));
			if(!asTexChunksSamplesGroups)
			{
				USP_DBGPRINT(( "GroupTexChunksSamplesByTexCoords: Failed to allocate memory for Texture Chunks Samples Groups\n"));
				return IMG_FALSE;
			}
		}
		else
		{
			asTexChunksSamplesGroups = IMG_NULL;
		}		
		psChunksDecisionData->asTexChunksSamplesGroups = asTexChunksSamplesGroups;
		if(uSamplesCount)
		{
			asTexChunksSamplesListEntries = (TEX_CHUNKS_SAMPLES_LIST*)psContext->pfnAlloc(uSamplesCount * sizeof(TEX_CHUNKS_SAMPLES_LIST));
			if(!asTexChunksSamplesListEntries)
			{
				USP_DBGPRINT(( "GroupTexChunksSamplesByTexCoords: Failed to allocate memory for Texture Chunks Samples List Entries\n"));
				return IMG_FALSE;
			}
		}
		else
		{
			asTexChunksSamplesListEntries = IMG_NULL;
		}		
		psChunksDecisionData->asTexChunksSamplesListEntries = asTexChunksSamplesListEntries;
		psFreeTexChunksSamplesListEntry = asTexChunksSamplesListEntries;		
		if(uSamplesCount)
		{
			asSampleListEntries = (SAMPLES_LIST*)psContext->pfnAlloc(uSamplesCount * sizeof(SAMPLES_LIST));
			if(!asSampleListEntries)
			{
				USP_DBGPRINT(( "GroupTexChunksSamplesByTexCoords: Failed to allocate memory for Sample List Entries\n"));
				return IMG_FALSE;
			}
		}
		else
		{
			asSampleListEntries = IMG_NULL;
		}		
		psChunksDecisionData->asSampleListEntries = asSampleListEntries;
		psFreeSampleListEntry = asSampleListEntries;		
	}
	for(psSample = psShader->psNonDepSamples; 
		psSample; 
		psSample = psSample->psNext)
	{
		PUSP_ITERATED_DATA			psIteratedTexCord;
		TEX_CHUNKS_SAMPLES_GROUP*	psTexChunksSamplesGroup = IMG_NULL;
		psIteratedTexCord = USPInputDataGetIteratedDataForNonDepSample(psShader->psInputData, psSample, psContext);
		if(!(psSample->sTexChanInfo.uTexChunkMask))
		{
			continue;
		}
		if(psIteratedTexCord != IMG_NULL)
		{
			IMG_UINT32 uTexChunksSamplesGroupIdx;
			for(
				uTexChunksSamplesGroupIdx = 0; 
				uTexChunksSamplesGroupIdx<uTexChunksSamplesGroupsCount; 
				uTexChunksSamplesGroupIdx++
			)
			{
				if(
					(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx].sCoordinateInfo.bIteratedByUSC == IMG_TRUE) 
					&& 
					(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx].sCoordinateInfo.u.psCoordIteratedData == psIteratedTexCord)
				)
				{
					AddSampleToTexChunksSamplesGroup(
						&(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx]), 
						psSample, 
						&psFreeTexChunksSamplesListEntry, 
						&psFreeSampleListEntry);					
					uTexChunksSamplesGroupIdx = uTexChunksSamplesGroupsCount + 1; //to break loop
				}
			}
			if(uTexChunksSamplesGroupIdx == uTexChunksSamplesGroupsCount)
			{
				psTexChunksSamplesGroup = &(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx]);
				psTexChunksSamplesGroup->sCoordinateInfo.bIteratedByUSC = IMG_TRUE;
				psTexChunksSamplesGroup->sCoordinateInfo.u.psCoordIteratedData = psIteratedTexCord;				
				psTexChunksSamplesGroup->psTexChunksSamplesList = IMG_NULL;							
				AddSampleToTexChunksSamplesGroup(psTexChunksSamplesGroup, psSample, &psFreeTexChunksSamplesListEntry, &psFreeSampleListEntry);				
				uTexChunksSamplesGroupsCount++;
			}
		}
		else
		{
			IMG_UINT32 uTexChunksSamplesGroupIdx;
			for(
				uTexChunksSamplesGroupIdx = 0; 
				uTexChunksSamplesGroupIdx<uTexChunksSamplesGroupsCount; 
				uTexChunksSamplesGroupIdx++)
			{
				if(
					(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx].sCoordinateInfo.bIteratedByUSC == IMG_FALSE) 
					&& 
					(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx].sCoordinateInfo.u.sCoordinateDetails.eCoord == psSample->eCoord) 
					&& 
					(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx].sCoordinateInfo.u.sCoordinateDetails.bCentroid == psSample->bCentroid) 
				)
				{
					if((asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx].sCoordinateInfo.u.sCoordinateDetails.uCoordDimensions) < (psSample->uTexDim))
					{
						asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx].sCoordinateInfo.u.sCoordinateDetails.uCoordDimensions = psSample->uTexDim;
					}
					AddSampleToTexChunksSamplesGroup(&(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx]), psSample, &psFreeTexChunksSamplesListEntry, &psFreeSampleListEntry);
					uTexChunksSamplesGroupIdx = uTexChunksSamplesGroupsCount + 1; //to break loop
				}
			}
			if(uTexChunksSamplesGroupIdx == uTexChunksSamplesGroupsCount)
			{
				psTexChunksSamplesGroup = &(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx]);
				psTexChunksSamplesGroup->sCoordinateInfo.bIteratedByUSC = IMG_FALSE;
				psTexChunksSamplesGroup->sCoordinateInfo.u.sCoordinateDetails.eCoord = psSample->eCoord;
				psTexChunksSamplesGroup->sCoordinateInfo.u.sCoordinateDetails.bCentroid = psSample->bCentroid;
				psTexChunksSamplesGroup->sCoordinateInfo.u.sCoordinateDetails.uCoordDimensions = psSample->uTexDim;				
				psTexChunksSamplesGroup->psTexChunksSamplesList = IMG_NULL;				
				AddSampleToTexChunksSamplesGroup(psTexChunksSamplesGroup, psSample, &psFreeTexChunksSamplesListEntry, &psFreeSampleListEntry);
				uTexChunksSamplesGroupsCount++;
			}
		}
	}
	*ppsTexChunksSamplesGroups = asTexChunksSamplesGroups;
	*puTexChunksSamplesGroupsCount = uTexChunksSamplesGroupsCount;
	return IMG_TRUE;
}

static IMG_VOID SetCostsOfTexChunksSampsGroups(TEX_CHUNKS_SAMPLES_GROUP		asTexChunksSamplesGroups[], 
											   IMG_UINT32					uTexChunksSamplesGroupsCount, 
											   TEX_CHUNKS_SAMPLES_GROUPS_COST*
																			psTexChunksSamplesGroupsCost, 
											   IMG_UINT32*					puLstEntrsNededByNonDep)
/*****************************************************************************
 Name:		SetCostsOfTexChunksSampsGroups

 Purpose:	Set PA registers and PDS Command List Entries cost of Texture 
			Chunks and Samples Groups.

 Inputs:	asTexChunksSamplesGroups 
						- Texture Chunks and Samples groups.
			uTexChunksSamplesGroupsCount 
						- Texture Chunks and Samples groups Count.
			psTexChunksSamplesGroupsCost 
						- Will contain the PA registers and List Entries 
						  usage count.
		    puLstEntrsNededByNonDep 
						- Pointed memory will be update with Command List 
						  Entries Count needed by all Non Dep Samples.

 Outputs:	None.

 Returns:	Nothing.
*****************************************************************************/
{
	IMG_UINT32 uTexChunksSamplesGroupIdx;
	IMG_UINT32 uLstEntrsNededByNonDep = 0;
	psTexChunksSamplesGroupsCost->uListEntriesUsedCount = 0;
	psTexChunksSamplesGroupsCost->uPARegsNeededCount = 0;
	for(
		uTexChunksSamplesGroupIdx = 0; 
		uTexChunksSamplesGroupIdx<uTexChunksSamplesGroupsCount; 
		uTexChunksSamplesGroupIdx++
	)
	{
		TEX_CHUNKS_SAMPLES_GROUP*	psTexChunksSamplesGroup = &(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx]);		
		TEX_CHUNKS_SAMPLES_LIST*	psTexChunksSamplesList = psTexChunksSamplesGroup->psTexChunksSamplesList;
		psTexChunksSamplesGroup->uTexChnksSmpsPAOrEntrsCount = 0;
		while(psTexChunksSamplesList)
		{
			TEX_CHUNKS_SAMPLES* psTexChunksSamples;
			psTexChunksSamples = &(psTexChunksSamplesList->sTexChunksSamples);
			psTexChunksSamples->uPAOrEntriesCount = auChunkMaskToPAsCount[psTexChunksSamples->uChunksMask];

			(psTexChunksSamplesGroup->uTexChnksSmpsPAOrEntrsCount) = 
				(psTexChunksSamplesGroup->uTexChnksSmpsPAOrEntrsCount) + psTexChunksSamples->uPAOrEntriesCount;			
			/* 
				PA register used by Reserved Chunk
			*/
			(psTexChunksSamplesGroupsCost->uPARegsNeededCount)++;
			/* 
				List Entry used by Reserved Chunk
			*/
			(psTexChunksSamplesGroupsCost->uListEntriesUsedCount)++;
			psTexChunksSamplesList = psTexChunksSamplesList->psNext;
		}
		uLstEntrsNededByNonDep += (psTexChunksSamplesGroup->uTexChnksSmpsPAOrEntrsCount);
	}
	*puLstEntrsNededByNonDep = uLstEntrsNededByNonDep;
	return;
}

static IMG_VOID AddCostOfIteratedData(TEX_CHUNKS_SAMPLES_GROUPS_COST* 
														psTexChunksSamplesGroupsCost, 
									  PUSP_ITERATED_DATA 
														psIteratedData, 
									  IMG_UINT32		uIteratedDataCount)
/*****************************************************************************
 Name:		SetCostsOfTexChunksSampsGroups

 Purpose:	Adds PA registers and PDS Command List Entries needed by already 
			iterated data.

 Inputs:	psTexChunksSamplesGroupsCost 
						- Will contain the PA registers and List Entries 
						  usage count.
		    puLstEntrsNededByNonDep 
						- Pointed memory will be update with Command List 
						  Entries Count needed.
			psIteratedData 
						- Iterated data.
			uIteratedDataCount 
						- Iterated data count.

 Outputs:	None.

 Returns:	Nothing.
*****************************************************************************/
{
	IMG_UINT32 uI;
	for(uI=0; uI<uIteratedDataCount; uI++)
	{
		(psTexChunksSamplesGroupsCost->uPARegsNeededCount) += (psIteratedData->uRegCount);
		(psTexChunksSamplesGroupsCost->uListEntriesUsedCount)++;
		psIteratedData++;
	}
}

static IMG_VOID FetchAllNonDepSmpsAsNonDep(
											TEX_CHUNKS_SAMPLES_GROUP*	asTexChunksSamplesGroups, 
											IMG_UINT32					uTexChunksSamplesGroupsCount)
/*****************************************************************************
 Name:		FetchAllNonDepSmpsAsNonDep

 Purpose:	Fetch all Non Dependent Samples as Non Dependent Samples.

 Inputs:	asTexChunksSamplesGroups 
						- Texture Chunks and Samples groups.
			uTexChunksSamplesGroupsCount 
						- Texture Chunks and Samples groups Count.			

 Outputs:	None.

 Returns:	IMG_TRUE if shader is patchable using allowed Command List 
			Entries, IMG_FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32 uGroupIdx;
	for(uGroupIdx = 0; uGroupIdx<uTexChunksSamplesGroupsCount; uGroupIdx++)
	{
		TEX_CHUNKS_SAMPLES_GROUP* psTexChunksSamplesGroup = &(asTexChunksSamplesGroups[uGroupIdx]);		
		{
			TEX_CHUNKS_SAMPLES_LIST* psTexChunksSamplesList = psTexChunksSamplesGroup->psTexChunksSamplesList;
			while(psTexChunksSamplesList)
			{
				if(psTexChunksSamplesList->sTexChunksSamples.psReservedChunkSample)
				{
					USP_TEX_CHAN_INFO*	psTexChanInfo = &(psTexChunksSamplesList->sTexChunksSamples.psReservedChunkSample->sTexChanInfo);
					psTexChanInfo->uTexChunkMask |= (psTexChunksSamplesList->sTexChunksSamples.uReservedChunkBit);
					psTexChanInfo->uTexNonDepChunkMask = psTexChanInfo->uTexChunkMask;
				}
				{
					SAMPLES_LIST* psSamplesList = psTexChunksSamplesList->sTexChunksSamples.psChunksSamples;
					while(psSamplesList)
					{
						USP_TEX_CHAN_INFO*	psTexChanInfo = &(psSamplesList->psSample->sTexChanInfo);
						psTexChanInfo->uTexNonDepChunkMask = psTexChanInfo->uTexChunkMask;
						psSamplesList = psSamplesList->psNext;
					}
				}
				psTexChunksSamplesList = psTexChunksSamplesList->psNext;
			}
		}
	}
	return;
}

#define	USP_MAXIMUM_PDS_DOUTI_COMMANDS (31)

/* 
	Fetch all texture coordinates by iterated texture coordinates only.
*/
static IMG_BOOL FetchAllTexChunkSmpsByItdCoordsOnly(PUSP_CONTEXT		psContext, 
													TEX_CHUNKS_SAMPLES_GROUP 
																		asTexChunksSamplesGroups[], 
													IMG_UINT32 			uTexChunksSamplesGroupsCount, 
													TEX_CHUNKS_SAMPLES_GROUPS_COST* 
																		psTexChunksSamplesGroupsCost)
/*****************************************************************************
 Name:		FetchAllTexChunkSmpsByItdCoordsOnly

 Purpose:	Fetch all Non Dependent Samples as Dependent Sample Instructions 
			using Iterated Coordinates.

 Inputs:	psContext	- The current USP execution context
			asTexChunksSamplesGroups 
						- Texture Chunks and Samples groups.
			uTexChunksSamplesGroupsCount 
						- Texture Chunks and Samples groups Count.
			psTexChunksSamplesGroupsCost 
						- Will contain the PA registers and List Entries 
						  usage count.		    

 Outputs:	None.

 Returns:	IMG_TRUE if shader is patchable using allowed Command List 
			Entries, IMG_FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32 uGroupIdx;
	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/
	for(uGroupIdx = 0; uGroupIdx<uTexChunksSamplesGroupsCount; uGroupIdx++)
	{
		TEX_CHUNKS_SAMPLES_GROUP* psTexChunksSamplesGroup = &(asTexChunksSamplesGroups[uGroupIdx]);
		if(psTexChunksSamplesGroup->sCoordinateInfo.bIteratedByUSC == IMG_TRUE)
		{
			psTexChunksSamplesGroup->sCoordinateInfo.uPARegsUsedByCoordinate = 0;
		}
		else
		{
			psTexChunksSamplesGroup->sCoordinateInfo.uPARegsUsedByCoordinate = psTexChunksSamplesGroup->sCoordinateInfo.u.sCoordinateDetails.uCoordDimensions;
			(psTexChunksSamplesGroupsCost->uListEntriesUsedCount)++;
			(psTexChunksSamplesGroupsCost->uPARegsNeededCount) += (psTexChunksSamplesGroup->sCoordinateInfo.uPARegsUsedByCoordinate);
		}
		{
			TEX_CHUNKS_SAMPLES_LIST* psTexChunksSamplesList = psTexChunksSamplesGroup->psTexChunksSamplesList;
			while(psTexChunksSamplesList)
			{
				SAMPLES_LIST* psSamplesList = psTexChunksSamplesList->sTexChunksSamples.psChunksSamples;
				while(psSamplesList)
				{
					psSamplesList->psSample->sTexChanInfo.uTexNonDepChunkMask = 0;
					psSamplesList = psSamplesList->psNext;
				}
				psTexChunksSamplesList = psTexChunksSamplesList->psNext;
			}
		}
	}
	if((psTexChunksSamplesGroupsCost->uListEntriesUsedCount) <= USP_MAXIMUM_PDS_DOUTI_COMMANDS)
	{
		return IMG_TRUE;
	}
	else
	{
		USP_DBGPRINT(( "FetchAllTexChunkSmpsByItdCoordsOnly: Not enough command list entries avaiable to even iterate all texture coordinates\n"));
		return IMG_FALSE;
	}
}

#define MAX_UINT32_VALUE ((IMG_UINT32)(-1))

static IMG_BOOL DoSelectionsForMinimumPAUsage(PUSP_CONTEXT		psContext, 
											  TEX_CHUNKS_SAMPLES_GROUP 
																asTexChunksSamplesGroups[], 
											  IMG_UINT32 		uTexChunksSamplesGroupsCount, 
											  TEX_CHUNKS_SAMPLES_GROUPS_COST* 
											  					psTexChunksSamplesGroupsCost, 
											  IMG_UINT32		uPARegsAvailable)
/*****************************************************************************
 Name:		DoSelectionsForMinimumPAUsage

 Purpose:	Do maximum usage of PA registers and Command List Entrie 
			available.

 Inputs:	psContext	- The current USP execution context
			asTexChunksSamplesGroups 
						- Texture Chunks and Samples groups.
			uTexChunksSamplesGroupsCount 
						- Texture Chunks and Samples groups Count.
			psTexChunksSamplesGroupsCost 
						- Will contain the PA registers and List Entries 
						  usage count.
		    uPARegsAvailable 
						- PA register available to patch the Shader.

 Outputs:	None.

 Returns:	Nothing.
*****************************************************************************/
{		
	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/
	while((psTexChunksSamplesGroupsCost->uListEntriesUsedCount) <= USP_MAXIMUM_PDS_DOUTI_COMMANDS)
	{
		IMG_UINT32	uGroupIdx;
		IMG_FLOAT	fHighestValue = 1.0f;
		IMG_UINT32	uHighestValueIdx = MAX_UINT32_VALUE;
		IMG_UINT32	uAvailableListEntries = (USP_MAXIMUM_PDS_DOUTI_COMMANDS - (psTexChunksSamplesGroupsCost->uListEntriesUsedCount));
		for(
			uGroupIdx = 0; 
			(uGroupIdx<uTexChunksSamplesGroupsCount); 
			uGroupIdx++
		)
		{
			TEX_CHUNKS_SAMPLES_GROUP*	psTexChunksSmpsGroup = &(asTexChunksSamplesGroups[uGroupIdx]);
			if(psTexChunksSmpsGroup->sCoordinateInfo.uPARegsUsedByCoordinate != 0)
			{
				IMG_FLOAT fDividend;
				IMG_FLOAT fDivisor;
				fDividend = (IMG_FLOAT)psTexChunksSmpsGroup->sCoordinateInfo.uPARegsUsedByCoordinate;
				fDivisor = (IMG_FLOAT)psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount;
				if(((fDividend/fDivisor) > fHighestValue) && ((psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount) <= (uAvailableListEntries + 1)))
				{
					fHighestValue = (fDividend/fDivisor);
					uHighestValueIdx = uGroupIdx;
				}
			}
		}
		if(uHighestValueIdx != MAX_UINT32_VALUE)
		{
			TEX_CHUNKS_SAMPLES_GROUP*	psTexChunksSmpsGroup = &(asTexChunksSamplesGroups[uHighestValueIdx]);
			TEX_CHUNKS_SAMPLES_LIST*	psTexChunksSamplesList = (psTexChunksSmpsGroup->psTexChunksSamplesList);
			while(psTexChunksSamplesList)
			{
				TEX_CHUNKS_SAMPLES* psTexChunksSamples = &(psTexChunksSamplesList->sTexChunksSamples);				
				IMG_UINT32	uChunkIdx = 0;
				while((uChunkIdx<USP_MAX_TEXTURE_CHUNKS))
				{
					if((psTexChunksSamples->uChunksMask & (1<<uChunkIdx)))
					{
						psTexChunksSamples->uNonDepChunksMask = psTexChunksSamples->uNonDepChunksMask | (1<<uChunkIdx);					
						(psTexChunksSamples->uPAOrEntriesCount)--;
						(psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount)--;				
						(psTexChunksSamplesGroupsCost->uPARegsNeededCount)++;
						(psTexChunksSamplesGroupsCost->uListEntriesUsedCount)++;
					}
					uChunkIdx++;
				}
				{
					SAMPLES_LIST* psSamplesList = (psTexChunksSamples->psChunksSamples);
					while(psSamplesList)
					{
						USP_TEX_CHAN_INFO *psTexChanInfo = &(psSamplesList->psSample->sTexChanInfo);
						(psTexChanInfo->uTexNonDepChunkMask) |= (psTexChanInfo->uTexChunkMask & psTexChunksSamples->uNonDepChunksMask);
						psSamplesList = psSamplesList->psNext;
					}
				}
				psTexChunksSamplesList = psTexChunksSamplesList->psNext;
			}			
			if(psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount == 0)
			{
				(psTexChunksSamplesGroupsCost->uPARegsNeededCount) -= (psTexChunksSmpsGroup->sCoordinateInfo.uPARegsUsedByCoordinate);
				(psTexChunksSamplesGroupsCost->uListEntriesUsedCount)--;
				psTexChunksSmpsGroup->sCoordinateInfo.uPARegsUsedByCoordinate = 0;
			}
		}
		else
		{
			break;
		}
	}
	if((psTexChunksSamplesGroupsCost->uPARegsNeededCount) <= uPARegsAvailable)
	{
		return IMG_TRUE;
	}
	else
	{
		USP_DBGPRINT(("DoSelectionsForMinimumPAUsage: Not enough PA registers available to Patch Shader\n"));
		return IMG_FALSE;
	}
}

static IMG_VOID DoMaxPAAndEntriesUsage(	
										TEX_CHUNKS_SAMPLES_GROUP 
															asTexChunksSamplesGroups[], 
										IMG_UINT32 			uTexChunksSamplesGroupsCount, 
										TEX_CHUNKS_SAMPLES_GROUPS_COST* 
										  					psTexChunksSamplesGroupsCost, 
										IMG_UINT32			uPARegsAvailable)
/*****************************************************************************
 Name:		DoMaxPAAndEntriesUsage

 Purpose:	Do maximum usage of PA registers and Command List Entrie 
			available.

 Inputs:	asTexChunksSamplesGroups 
						- Texture Chunks and Samples groups.
			uTexChunksSamplesGroupsCount 
						- Texture Chunks and Samples groups Count.
			psTexChunksSamplesGroupsCost 
						- Will contain the PA registers and List Entries 
						  usage count.
		    uPARegsAvailable 
						- PA register available to patch the Shader.

 Outputs:	None.

 Returns:	Nothing.
*****************************************************************************/
{		
	while(
		((psTexChunksSamplesGroupsCost->uListEntriesUsedCount) < USP_MAXIMUM_PDS_DOUTI_COMMANDS) && 
		((psTexChunksSamplesGroupsCost->uPARegsNeededCount) < uPARegsAvailable)
	)
	{
		IMG_UINT32	uGroupIdx;
		IMG_FLOAT	fHighestValue = -1.0f;
		IMG_UINT32	uHighestValueIdx = MAX_UINT32_VALUE;				
		for(
			uGroupIdx = 0; 
			(uGroupIdx<uTexChunksSamplesGroupsCount); 
			uGroupIdx++
		)
		{
			TEX_CHUNKS_SAMPLES_GROUP*	psTexChunksSmpsGroup = &(asTexChunksSamplesGroups[uGroupIdx]);			
			{
				IMG_FLOAT fDividend;
				IMG_FLOAT fDivisor;
				fDividend = (IMG_FLOAT)psTexChunksSmpsGroup->sCoordinateInfo.uPARegsUsedByCoordinate;
				fDivisor = (IMG_FLOAT)psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount;
				if((fDividend/fDivisor) > fHighestValue)
				{
					fHighestValue = (fDividend/fDivisor);
					uHighestValueIdx = uGroupIdx;
				}
			}
		}
		if(uHighestValueIdx != MAX_UINT32_VALUE)
		{
			TEX_CHUNKS_SAMPLES_GROUP*	psTexChunksSmpsGroup = &(asTexChunksSamplesGroups[uHighestValueIdx]);
			TEX_CHUNKS_SAMPLES_LIST*	psTexChunksSamplesList = (psTexChunksSmpsGroup->psTexChunksSamplesList);
			TEX_CHUNKS_SAMPLES_LIST*	psLastTexChunksSamplesList = psTexChunksSamplesList;
			while(
				psTexChunksSamplesList && 				
				((psTexChunksSamplesGroupsCost->uListEntriesUsedCount) < USP_MAXIMUM_PDS_DOUTI_COMMANDS) && 
				((psTexChunksSamplesGroupsCost->uPARegsNeededCount) < uPARegsAvailable)
			)
			{
				TEX_CHUNKS_SAMPLES* psTexChunksSamples = &(psTexChunksSamplesList->sTexChunksSamples);				
				IMG_UINT32	uChunkIdx;
				if(psTexChunksSamples->bAllChunksDepOrNDep == IMG_FALSE)
				{
					for(
						uChunkIdx = 0; 
						(uChunkIdx<USP_MAX_TEXTURE_CHUNKS) && 					
						((psTexChunksSamplesGroupsCost->uListEntriesUsedCount) < USP_MAXIMUM_PDS_DOUTI_COMMANDS) && 
						((psTexChunksSamplesGroupsCost->uPARegsNeededCount) < uPARegsAvailable); 
						uChunkIdx++
					)
					{
						if(psTexChunksSamples->uChunksMask & (1<<uChunkIdx))
						{
							psTexChunksSamples->uNonDepChunksMask = psTexChunksSamples->uNonDepChunksMask | (1<<uChunkIdx);					
							(psTexChunksSamples->uPAOrEntriesCount)--;
							(psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount)--;				
							(psTexChunksSamplesGroupsCost->uPARegsNeededCount)++;
							(psTexChunksSamplesGroupsCost->uListEntriesUsedCount)++;
						}
					}
				}
				else
				{
					if(
						(((psTexChunksSamplesGroupsCost->uListEntriesUsedCount) + (psTexChunksSamples->uPAOrEntriesCount)) <= USP_MAXIMUM_PDS_DOUTI_COMMANDS) && 
						(((psTexChunksSamplesGroupsCost->uPARegsNeededCount) + (psTexChunksSamples->uPAOrEntriesCount)) <= uPARegsAvailable)
					)
					{
						for(
							uChunkIdx = 0; 
							(uChunkIdx<USP_MAX_TEXTURE_CHUNKS); 
							uChunkIdx++
						)
						{
							if(psTexChunksSamples->uChunksMask & (1<<uChunkIdx))
							{
								psTexChunksSamples->uNonDepChunksMask = psTexChunksSamples->uNonDepChunksMask | (1<<uChunkIdx);					
								(psTexChunksSamples->uPAOrEntriesCount)--;
								(psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount)--;				
								(psTexChunksSamplesGroupsCost->uPARegsNeededCount)++;
								(psTexChunksSamplesGroupsCost->uListEntriesUsedCount)++;
							}
						}
					}
				}
				{
					SAMPLES_LIST* psSamplesList = (psTexChunksSamples->psChunksSamples);
					while(psSamplesList)
					{
						USP_TEX_CHAN_INFO *psTexChanInfo = &(psSamplesList->psSample->sTexChanInfo);
						(psTexChanInfo->uTexNonDepChunkMask) |= (psTexChanInfo->uTexChunkMask & psTexChunksSamples->uNonDepChunksMask);
						psSamplesList = psSamplesList->psNext;
					}
				}				
				psLastTexChunksSamplesList = psTexChunksSamplesList;
				psTexChunksSamplesList = psTexChunksSamplesList->psNext;
			}
			if(
				((psTexChunksSamplesGroupsCost->uListEntriesUsedCount) == USP_MAXIMUM_PDS_DOUTI_COMMANDS)				
			)
			{
				/* 
					Loop broken due to shortage of List entries, shifting List entry from 
					coordinate to last sample's needed entry might free some more PA 
					registers. 
				*/
				if(
					((psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount) == 1) && 
					((psTexChunksSmpsGroup->sCoordinateInfo.uPARegsUsedByCoordinate) >= 1)
				)
				{
					while(psLastTexChunksSamplesList->sTexChunksSamples.uPAOrEntriesCount != 1)
					{
						psLastTexChunksSamplesList = psLastTexChunksSamplesList->psNext;
					}
					{
						TEX_CHUNKS_SAMPLES* psTexChunksSamples = &(psLastTexChunksSamplesList->sTexChunksSamples);					
						IMG_UINT32 uBitToSet = ((~(psTexChunksSamples->uNonDepChunksMask)) & (psTexChunksSamples->uChunksMask));
						{
							psTexChunksSamples->uNonDepChunksMask = (psTexChunksSamples->uNonDepChunksMask) | uBitToSet;
							(psTexChunksSamples->uPAOrEntriesCount)--;
							(psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount)--;				
							(psTexChunksSamplesGroupsCost->uPARegsNeededCount)++;
							(psTexChunksSamplesGroupsCost->uListEntriesUsedCount)++;						
						}				
						{						
							SAMPLES_LIST* psSamplesList = (psTexChunksSamples->psChunksSamples);
							while(psSamplesList)
							{
								USP_TEX_CHAN_INFO *psTexChanInfo = &(psSamplesList->psSample->sTexChanInfo);
								(psTexChanInfo->uTexNonDepChunkMask) |= (psTexChanInfo->uTexChunkMask & uBitToSet);
								psSamplesList = psSamplesList->psNext;
							}
						}
					}
				}
			}
			else if((psTexChunksSamplesGroupsCost->uPARegsNeededCount) == uPARegsAvailable)
			{
				/* 
					Loop broken due to shortage of PA registers, shifting List entry from 
					coordinate to last few PA's registers needed might allow more usage of 
					List entries. 
				*/
				IMG_UINT32	uAvailableListEntries = (USP_MAXIMUM_PDS_DOUTI_COMMANDS - (psTexChunksSamplesGroupsCost->uListEntriesUsedCount));
				if(
					((psTexChunksSamplesGroupsCost->uPARegsNeededCount) == uPARegsAvailable) && 
					((psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount) <= (uAvailableListEntries + 1))  && 
					((psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount) <= (psTexChunksSmpsGroup->sCoordinateInfo.uPARegsUsedByCoordinate))
				)
				{

					psTexChunksSamplesList = psLastTexChunksSamplesList;					
					while(psTexChunksSamplesList)
					{
						TEX_CHUNKS_SAMPLES* psTexChunksSamples = &(psTexChunksSamplesList->sTexChunksSamples);				
						IMG_UINT32	uChunkIdx;
						for( 
							uChunkIdx = 0; 
							(uChunkIdx<USP_MAX_TEXTURE_CHUNKS); 
							uChunkIdx++
						)
						{
							if(
								(psTexChunksSamples->uChunksMask & (1<<uChunkIdx)) && 
								(!(psTexChunksSamples->uNonDepChunksMask & (1<<uChunkIdx)))
							)
							{
								psTexChunksSamples->uNonDepChunksMask = psTexChunksSamples->uNonDepChunksMask | (1<<uChunkIdx);					
								(psTexChunksSamples->uPAOrEntriesCount)--;
								(psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount)--;				
								(psTexChunksSamplesGroupsCost->uPARegsNeededCount)++;
								(psTexChunksSamplesGroupsCost->uListEntriesUsedCount)++;
							}
						}				
						{
							SAMPLES_LIST* psSamplesList = (psTexChunksSamples->psChunksSamples);
							while(psSamplesList)
							{
								USP_TEX_CHAN_INFO *psTexChanInfo = &(psSamplesList->psSample->sTexChanInfo);
								(psTexChanInfo->uTexNonDepChunkMask) |= (psTexChanInfo->uTexChunkMask & psTexChunksSamples->uNonDepChunksMask);
								psSamplesList = psSamplesList->psNext;
							}
						}						
						psTexChunksSamplesList = psTexChunksSamplesList->psNext;
					}
				}
			}
			if(psTexChunksSmpsGroup->uTexChnksSmpsPAOrEntrsCount == 0)
			{
				(psTexChunksSamplesGroupsCost->uPARegsNeededCount) -= (psTexChunksSmpsGroup->sCoordinateInfo.uPARegsUsedByCoordinate);
				if(psTexChunksSmpsGroup->sCoordinateInfo.bIteratedByUSC == IMG_FALSE)
				{
					(psTexChunksSamplesGroupsCost->uListEntriesUsedCount)--;
				}
				psTexChunksSmpsGroup->sCoordinateInfo.uPARegsUsedByCoordinate = 0;
			}
		}
		else
		{
			break;
		}
	}
	return;
}

static IMG_VOID AddReservedNonDepTexSampleChunks(TEX_CHUNKS_SAMPLES_GROUP 
																asTexChunksSamplesGroups[], 
												 IMG_UINT32 	uTexChunksSamplesGroupsCount)
/*****************************************************************************
 Name:		AddReservedNonDepTexSampleChunks

 Purpose:	Add Reserved Non Dep Samples to the Chunks Decision data.

 Inputs:	asTexChunksSamplesGroups 
						- Texture Chunks and Samples groups.
			uTexChunksSamplesGroupsCount 
						- Texture Chunks and Samples groups Count.

 Outputs:	None.

 Returns:	Nothing.
*****************************************************************************/
{	
	IMG_UINT32 uTexChunksSamplesGroupIdx;
	for(
		uTexChunksSamplesGroupIdx = 0; 
		uTexChunksSamplesGroupIdx<uTexChunksSamplesGroupsCount; 
		uTexChunksSamplesGroupIdx++
	)
	{
		TEX_CHUNKS_SAMPLES_GROUP*	psTexChunksSamplesGroup = &(asTexChunksSamplesGroups[uTexChunksSamplesGroupIdx]);		
		TEX_CHUNKS_SAMPLES_LIST*	psTexChunksSamplesList = psTexChunksSamplesGroup->psTexChunksSamplesList;		
		while(psTexChunksSamplesList)
		{
			TEX_CHUNKS_SAMPLES* psTexChunksSamples = &(psTexChunksSamplesList->sTexChunksSamples);
			USP_TEX_CHAN_INFO*	psTexChanInfo = &(psTexChunksSamples->psReservedChunkSample->sTexChanInfo);
			(psTexChanInfo->uTexChunkMask) |= psTexChunksSamples->uReservedChunkBit;
			if((psTexChunksSamplesList->sTexChunksSamples.bAllChunksDepOrNDep) == IMG_FALSE)
			{
				(psTexChanInfo->uTexNonDepChunkMask) |= psTexChunksSamples->uReservedChunkBit;
			}
			else
			{
				if(psTexChanInfo->uTexNonDepChunkMask)
				{
					(psTexChanInfo->uTexNonDepChunkMask) |= psTexChunksSamples->uReservedChunkBit;
				}				
			}			
			psTexChunksSamplesList = psTexChunksSamplesList->psNext;
		}		
	}
	return;
}

#if 0
#include <stdio.h>
static IMG_VOID PrintTexChunksSampsGroups(TEX_CHUNKS_SAMPLES_GROUP*	asTexChunksSamplesGroups, 
										  IMG_UINT32				uTexChunksSamplesGroupsCount)
{
	IMG_UINT32 uI;
	for(uI=0; uI<uTexChunksSamplesGroupsCount; uI++)
	{
		TEX_CHUNKS_SAMPLES_GROUP* psTexChunksSampsGroup = &(asTexChunksSamplesGroups[uI]);
		printf("coordinate information: ");
		if(psTexChunksSampsGroup->sCoordinateInfo.bIteratedByUSC)
		{
			printf(" :iterated by usc: ");
			if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_TC0)
			{
				printf(" :tc0: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_TC1)
			{
				printf(" :tc1: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_TC2)
			{
				printf(" :tc2: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_TC3)
			{
				printf(" :tc3: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_TC4)
			{
				printf(" :tc4: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_TC5)
			{
				printf(" :tc5: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_TC6)
			{
				printf(" :tc6: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_TC7)
			{
				printf(" :tc7: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_TC8)
			{
				printf(" :tc8: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_TC9)
			{
				printf(" :tc9: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_V0)
			{
				printf(" :v0: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_V1)
			{
				printf(" :v1: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_FOG)
			{
				printf(" :fog: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_POS)
			{
				printf(" :pos: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.psCoordIteratedData->eType == USP_ITERATED_DATA_TYPE_VPRIM)
			{
				printf(" :vprim: ");
			}
		}
		else
		{
			printf(" :not iterated by usc: ");			
			if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_TC0)
			{
				printf(" :tc0: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_TC1)
			{
				printf(" :tc1: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_TC2)
			{
				printf(" :tc2: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_TC3)
			{
				printf(" :tc3: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_TC4)
			{
				printf(" :tc4: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_TC5)
			{
				printf(" :tc5: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_TC6)
			{
				printf(" :tc6: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_TC7)
			{
				printf(" :tc7: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_TC8)
			{
				printf(" :tc8: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_TC9)
			{
				printf(" :tc9: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_V0)
			{
				printf(" :v0: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_V1)
			{
				printf(" :v1: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_FOG)
			{
				printf(" :fog: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_POS)
			{
				printf(" :pos: ");
			}
			else if((psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.eCoord == USP_ITERATED_DATA_TYPE_VPRIM)
			{
				printf(" :vprim: ");
			}
			printf(" :%lu: ", (psTexChunksSampsGroup->sCoordinateInfo).u.sCoordinateDetails.uCoordDimensions);
		}
		printf("\n");
		{		
			TEX_CHUNKS_SAMPLES_LIST* psTexChunksSamplesList;
			psTexChunksSamplesList = psTexChunksSampsGroup->psTexChunksSamplesList;
			while(psTexChunksSamplesList)
			{
				SAMPLES_LIST* psSamplesList;
				IMG_UINT32		auChunkMaskToPasCount[16] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
				IMG_UINT32		auIgnoreFirstSetBit[16] = {0, 0, 0, 2, 0, 4, 4, 6, 0, 8, 8, 10, 8, 12, 12, 14};
				psSamplesList = psTexChunksSamplesList->sTexChunksSamples.psChunksSamples;
				while(psSamplesList != IMG_NULL)
				{
					printf(" :TEX%lu:", psSamplesList->psSample->uTextureIdx);
					printf("%lu:", auChunkMaskToPasCount[auIgnoreFirstSetBit[psSamplesList->psSample->sTexChanInfo.uTexNonDepChunkMask]]);
					psSamplesList = psSamplesList->psNext;
				}
				
				psTexChunksSamplesList = psTexChunksSamplesList->psNext;
			}
		}
		printf("\n");
	}
}
#endif

/* 
	Gives maximum number of PA registers available for iteration or pre sampled data commands. 
*/
static IMG_BOOL GetMaxPARegsAvailableForCommands(PUSP_CONTEXT	psContext, 
												 PUSP_SHADER	psShader, 
												 IMG_UINT32*	puPARegsAvailable)
/*****************************************************************************
 Name:		GetMaxPARegsAvailableForCommands

 Purpose:	Gives maximum number of PA registers available for iteration or 
			pre sampled data commands. 

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader to get registers for
			puPARegsAvailable 
						- Pointer to memory where register count will be 
						  stored. 

 Outputs:	none

 Returns:	Nothing.
*****************************************************************************/
{
	PUSP_PROGDESC	psProgDesc = psShader->psProgDesc;
	IMG_UINT32		uPARegsAvailable = EURASIA_USE_PRIMATTR_BANK_SIZE - (psProgDesc->uExtraPARegs);	
	if(psContext->eOutputRegType == USP_OUTPUT_REGTYPE_PA)
	{
		IMG_UINT32 uDefaultPSResultRegCount;
		if (!(psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_OMASKFEEDBACK))
		{
			uDefaultPSResultRegCount = psProgDesc->uDefaultPSResultRegCount;
		}
		else
		{
			uDefaultPSResultRegCount = psProgDesc->uDefaultPSResultRegCount + 1;
		}
		if(psProgDesc->uPSResultFirstPAReg == psProgDesc->uPARegCount)
		{
			if(uPARegsAvailable < uDefaultPSResultRegCount)
			{
				USP_DBGPRINT(( "GetMaxPARegsAvailableForCommands: Not enough PA registers available for Result of Shader\n"));
				return IMG_FALSE;
			}
			uPARegsAvailable -= uDefaultPSResultRegCount;
		}
	}
	*puPARegsAvailable = uPARegsAvailable;
	return IMG_TRUE;
}

static IMG_VOID FreeChunksDecisionData(
										PUSP_CONTEXT			psContext, 
										CHUNKS_DECISION_DATA*	psChunksDecisionData)
/*****************************************************************************
 Name:		FreeChunksDecisionData

 Purpose:	Free memory allocated for Chunk Descision data manipulation.

 Inputs:	psContext	- The current USP context.
			psChunksDecisionData 
						- Point to the Chunk Decision data memory.

 Outputs:	None.

 Returns:	Nothing.
*****************************************************************************/
{
	if(psChunksDecisionData->asSampleListEntries != IMG_NULL)
	{
		psContext->pfnFree(psChunksDecisionData->asSampleListEntries);
	}
	psChunksDecisionData->asSampleListEntries = IMG_NULL;
	if(psChunksDecisionData->asTexChunksSamplesListEntries != IMG_NULL)
	{
		psContext->pfnFree(psChunksDecisionData->asTexChunksSamplesListEntries);
	}
	psChunksDecisionData->asTexChunksSamplesListEntries = IMG_NULL;
	if(psChunksDecisionData->asTexChunksSamplesGroups != IMG_NULL)
	{
		psContext->pfnFree(psChunksDecisionData->asTexChunksSamplesGroups);
	}
	psChunksDecisionData->asTexChunksSamplesGroups = IMG_NULL;
}

IMG_INTERNAL 
IMG_BOOL DecideChunksSampling(
							  PUSP_CONTEXT	psContext, 
							  PUSP_SHADER	psShader)
/*****************************************************************************
 Name:		DecideChunksSampling

 Purpose:	Decides either to sample chunks as dependent or non dependent.

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader to decide chunk sampling for

 Outputs:	None.

 Returns:	IMG_TRUE if successful IMG_FALSE otherwise.
*****************************************************************************/
{
	TEX_CHUNKS_SAMPLES_GROUP* 
					asTexChunksSamplesGroups = IMG_NULL;
	IMG_UINT32		uTexChunksSamplesGroupsCount = 0;	
	TEX_CHUNKS_SAMPLES_GROUPS_COST 
					sTexChunksSamplesGroupsCost;
	IMG_UINT32		uPARegsAvailable;
	CHUNKS_DECISION_DATA
					sChunksDecisionData;
	/* 
		Command List Entries needed by Non Dependent Samples. 
	*/
	IMG_UINT32		uLstEntrsNededByNonDep = 0;

	if(!GetMaxPARegsAvailableForCommands(psContext, psShader, &uPARegsAvailable))
	{
		USP_DBGPRINT(( "DecideChunksSampling: Not enough PA registers available to Patch Shader\n"));
		return IMG_FALSE;
	}

	if(!GroupTexChunksSamplesByTexCoords(psContext, psShader, &asTexChunksSamplesGroups, &uTexChunksSamplesGroupsCount, &sChunksDecisionData))
	{
		USP_DBGPRINT(( "DecideChunksSampling: Failed to group Texture Chunks Samples by Texture Coordinates\n"));
		return IMG_FALSE;
	}

	SetCostsOfTexChunksSampsGroups(asTexChunksSamplesGroups, uTexChunksSamplesGroupsCount, &sTexChunksSamplesGroupsCost, &uLstEntrsNededByNonDep);

	uLstEntrsNededByNonDep += sTexChunksSamplesGroupsCost.uPARegsNeededCount;
	
	AddCostOfIteratedData(&sTexChunksSamplesGroupsCost, psShader->psInputData->psIteratedData, psShader->psInputData->uIteratedDataCount);
	
	if(
		((uLstEntrsNededByNonDep + sTexChunksSamplesGroupsCost.uListEntriesUsedCount) <= USP_MAXIMUM_PDS_DOUTI_COMMANDS) && 
		((sTexChunksSamplesGroupsCost.uPARegsNeededCount) <= uPARegsAvailable)
	)
	{
		FetchAllNonDepSmpsAsNonDep(asTexChunksSamplesGroups, uTexChunksSamplesGroupsCount);
		FreeChunksDecisionData(psContext, &sChunksDecisionData);
		return IMG_TRUE;
	}

	if(!FetchAllTexChunkSmpsByItdCoordsOnly(psContext, asTexChunksSamplesGroups, uTexChunksSamplesGroupsCount, &sTexChunksSamplesGroupsCost))
	{
		USP_DBGPRINT(( "DecideChunksSampling: Not enough command list entries available\n"));
		return IMG_FALSE;
	}

	if(!DoSelectionsForMinimumPAUsage(psContext, asTexChunksSamplesGroups, uTexChunksSamplesGroupsCount, &sTexChunksSamplesGroupsCost, uPARegsAvailable))
	{
		USP_DBGPRINT(( "DecideChunksSampling: Not enough PA registers available to Patch Shader\n"));
		return IMG_FALSE;
	}
	
	DoMaxPAAndEntriesUsage(asTexChunksSamplesGroups, uTexChunksSamplesGroupsCount, &sTexChunksSamplesGroupsCost, uPARegsAvailable);

	AddReservedNonDepTexSampleChunks(asTexChunksSamplesGroups, uTexChunksSamplesGroupsCount);

#if 0
	PrintTexChunksSampsGroups(asTexChunksSamplesGroups, uTexChunksSamplesGroupsCount);	

	printf("total list entries needed: %lu\n", sTexChunksSamplesGroupsCost.uListEntriesUsedCount);
	printf("total pa regsiters needed: %lu\n", sTexChunksSamplesGroupsCost.uPARegsNeededCount);
#endif
	FreeChunksDecisionData(psContext, &sChunksDecisionData);
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		AddSampleTextureControlWords

 Purpose:	Adds an entry in texture control words list for the given sampler.

 Inputs:	psContext		- The USP shader 
			psSample		- The sampler to add texture control words for
			psTexCtrWrds	- The set of texture control words to add

 Outputs:	none

 Returns:	An index in texture control word list where entry is added
*****************************************************************************/
static IMG_UINT32 AddSampleTextureControlWords(PUSP_SHADER				psShader, 
											   PUSP_SAMPLE				psSample, 
											   PUSP_SHDR_TEX_CTR_WORDS	psTexCtrWrds)
{
	IMG_BOOL	bIsEqual;
	IMG_UINT32	uSmpIdx;
	IMG_UINT32	uFirstFreeIdx = psShader->uTotalSmpTexCtrWrds;

	/*
		Try to add or find first free entry
	*/
	for(uSmpIdx=0; uSmpIdx<psShader->uTotalSmpTexCtrWrds; uSmpIdx++)
	{
		PUSP_SHDR_TEX_CTR_WORDS	psTexCtrWrdsEntry = 
			&(psShader->psTexCtrWrds[uSmpIdx]);
		
		/*
			Do comparison
		*/
		if(psTexCtrWrdsEntry->bUsed)
		{
			IMG_UINT32 uPlnIdx;

			bIsEqual = IMG_TRUE;

			if(psTexCtrWrdsEntry->uTextureIdx != psTexCtrWrds->uTextureIdx)
			{
				bIsEqual = IMG_FALSE;
			}
			else
			{
				for(uPlnIdx=0; uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; uPlnIdx++)
				{
					if(psTexCtrWrdsEntry->auWords[uPlnIdx] != psTexCtrWrds->auWords[uPlnIdx])
					{
						bIsEqual = IMG_FALSE;
					}

					if(psTexCtrWrdsEntry->abExtFmt[uPlnIdx] != psTexCtrWrds->abExtFmt[uPlnIdx])
					{
						bIsEqual = IMG_FALSE;
					}

					if(psTexCtrWrdsEntry->abReplicate[uPlnIdx] != psTexCtrWrds->abReplicate[uPlnIdx])
					{
						bIsEqual = IMG_FALSE;
					}

					if(psTexCtrWrdsEntry->aeUnPackFmts[uPlnIdx] != psTexCtrWrds->aeUnPackFmts[uPlnIdx])
					{
						bIsEqual = IMG_FALSE;
					}

					if(psTexCtrWrdsEntry->auSwizzle[uPlnIdx] != psTexCtrWrds->auSwizzle[uPlnIdx])
					{
						bIsEqual = IMG_FALSE;
					}

					if(psTexCtrWrdsEntry->abMinPack[uPlnIdx] != psTexCtrWrds->abMinPack[uPlnIdx])
					{
						bIsEqual = IMG_FALSE;
					}

					if(psTexCtrWrdsEntry->auTagSize[uPlnIdx] != psTexCtrWrds->auTagSize[uPlnIdx])
					{
						bIsEqual = IMG_FALSE;
					}
				}
			}
			
			/*
				This set already exist in old list so use it
			*/
			if(bIsEqual)
			{
				psSample->uTexCtrWrdIdx = uSmpIdx;
				return uSmpIdx;
			}
		}
		else
		{
			if(uFirstFreeIdx == psShader->uTotalSmpTexCtrWrds)
			{
				uFirstFreeIdx = uSmpIdx;
			}
		}
	}

	/*
		New entry is required for this set of texture control words
	*/
	if(uFirstFreeIdx < psShader->uTotalSmpTexCtrWrds)
	{
		IMG_UINT16 uLastWrdIdx = (IMG_UINT16)-1;
		IMG_UINT32 uPlnIdx;

		/*
			Find a last index that is used for texture control words
		*/
		if(uFirstFreeIdx == 0)
		{
			uLastWrdIdx = 0;
		}
		else
		{
			for(uPlnIdx=0; uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; uPlnIdx++)
			{
				IMG_UINT16 uWrdIdx = 
					psShader->psTexCtrWrds[uFirstFreeIdx-1].auTexWrdIdx[uPlnIdx];
				if(uWrdIdx != (IMG_UINT16)-1)
				{
					uLastWrdIdx = uWrdIdx;
					uLastWrdIdx++;
				}
			}
		}
		psShader->psTexCtrWrds[uFirstFreeIdx] = *psTexCtrWrds;

		/*
			Add indexes for indivisal texture control words
		*/
		for(uPlnIdx=0; uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; uPlnIdx++)
		{
			if(psShader->psTexCtrWrds[uFirstFreeIdx].auWords[uPlnIdx] != 
				(IMG_UINT32)-1)
			{
				psShader->psTexCtrWrds[uFirstFreeIdx].auTexWrdIdx[uPlnIdx] = 
				uLastWrdIdx;
				
				uLastWrdIdx++;
			}
		}

		return uFirstFreeIdx;
	}
	else
	{
		return (IMG_UINT32)-1;
	}
}

/*****************************************************************************
 Name:		SetDefaultTextureControlWords

 Purpose:	Sets default texture control words for the given usp texture 
			format

 Inputs:	psContext		- The current USP execution context
			uTexIdx			- Id of the given texture

 Outputs:	psTexCtrWrds	- The set of texture control words for given 
							  texture
			puRegMask		- The mask to tel how many consecutive registers 
							  will be written

 Returns:	IMG_TRUE/IMG_FALSE on pass/failure
*****************************************************************************/
static IMG_BOOL SetDefaultTextureControlWords(PUSP_CONTEXT				psContext, 
											  IMG_UINT32				uTexIdx, 
											  PUSP_SHDR_TEX_CTR_WORDS	psTexCtrWrds, 
											  IMG_PUINT32				puRegMask)
{
	IMG_UINT32 auTextFmt[USP_TEXFORM_MAX_PLANE_COUNT];
	IMG_BOOL bValidFmt = IMG_TRUE;
	IMG_UINT32 uPlnIdx;
	IMG_UINT16 uTagSize[USP_TEXFORM_MAX_PLANE_COUNT];

	#if defined(EURASIA_PDS_DOUTT0_TEXFEXT)
	IMG_BOOL abUseExtFmt[USP_TEXFORM_MAX_PLANE_COUNT];
	#endif /* defined(EURASIA_PDS_DOUTT0_TEXFEXT) */
			
	#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
	IMG_BOOL abChanReplct[USP_TEXFORM_MAX_PLANE_COUNT];
	#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

	#if defined(SGX_FEATURE_TAG_SWIZZLE)
	IMG_UINT32	auSwizzle[USP_TEXFORM_MAX_PLANE_COUNT];
	IMG_BOOL	abMinPack[USP_TEXFORM_MAX_PLANE_COUNT];
	#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

	/*
		Might be required for multi result reg formats
	*/
	PVR_UNREFERENCED_PARAMETER(puRegMask);

	for(uPlnIdx=0; uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; uPlnIdx++)
	{
		auTextFmt[uPlnIdx] = (IMG_UINT32)-1;
		uTagSize[uPlnIdx] = 0;

		#if defined(EURASIA_PDS_DOUTT0_TEXFEXT)
		abUseExtFmt[uPlnIdx] = IMG_FALSE;
		#endif /* defined(EURASIA_PDS_DOUTT0_TEXFEXT) */
			
		#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
		abChanReplct[uPlnIdx] = IMG_FALSE;
		#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

		#if defined(SGX_FEATURE_TAG_SWIZZLE)
		auSwizzle[uPlnIdx] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
		abMinPack[uPlnIdx] = IMG_FALSE;
		#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
	}

	switch(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt)
	{
		case USP_TEXTURE_FORMAT_A4R4G4B4:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4;
			
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ARGB;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_A1R5G5B5:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ARGB;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_R5G6B5:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_R5G6B5;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_RGB;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_B8G8R8A8:
		{
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8888;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ARGB;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_R8G8B8A8:
		{
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8888;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_I8:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_RRRR;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
			abChanReplct[0] = IMG_TRUE;
			#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_A8:
		{
			#if defined(SGX545)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_A8;
			abUseExtFmt[0] = IMG_TRUE;
			#else
            #if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R000;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
			abChanReplct[0] = IMG_TRUE;
			#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_L8:
		{
		    #if defined(SGX545)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_L8;
			abUseExtFmt[0] = IMG_TRUE;
			#else
            #if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_1RRR;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
			abChanReplct[0] = IMG_TRUE;
			#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_L8A8:
		{
			#if defined(SGX545)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_L8A8;
			abUseExtFmt[0] = IMG_TRUE;
			#else
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U88;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_GRRR;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U88;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
			abChanReplct[0] = IMG_TRUE;
			#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

			uTagSize[0] = 1;
			break;
		}
		
		case USP_TEXTURE_FORMAT_RGBA_F32:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;
			auTextFmt[2] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;
			auTextFmt[3] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[1] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[2] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[3] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX543) */

			uTagSize[0] = 1;
			uTagSize[1] = 1;
			uTagSize[2] = 1;
			uTagSize[3] = 1;
			break;
		}

	    case USP_TEXTURE_FORMAT_RGBA_UNORM8:
		{
#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8888;
#else
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8;
#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */
			
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
		    break;
		}
		
		case USP_TEXTURE_FORMAT_RGBA_U8:
	    {
			#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8888;
			#else
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8;
			#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */
			
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
		    break;
	    }
		case USP_TEXTURE_FORMAT_RGBA_S8:
	    {
			#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_S8888;
			#else
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8;
			#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */
			
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_U16:
	    {
			#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U1616;
		    auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_U1616;
			#else
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_GR;
			auSwizzle[1] = EURASIA_PDS_DOUTT3_SWIZ_GR;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			uTagSize[1] = 1;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_S16:
	    {
			#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_S1616;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_S1616;
			#else
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_GR;
			auSwizzle[1] = EURASIA_PDS_DOUTT3_SWIZ_GR;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			uTagSize[1] = 1;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_UNORM16:
	    {
			#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U1616;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_U1616;
			#else
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_GR;
			auSwizzle[1] = EURASIA_PDS_DOUTT3_SWIZ_GR;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			uTagSize[1] = 1;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_U32:
	    {
			#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U32;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_U32;
		    auTextFmt[2] = EURASIA_PDS_DOUTT1_TEXFORMAT_U32;
			auTextFmt[3] = EURASIA_PDS_DOUTT1_TEXFORMAT_U32;
			#else
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			auTextFmt[2] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			auTextFmt[3] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[1] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[2] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[3] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX543) */

			uTagSize[0] = 1;
			uTagSize[1] = 1;
			uTagSize[2] = 1;
			uTagSize[3] = 1;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_S32:
	    {
			#if defined(SGX543) || defined(SGX544) || defined(SGX554)
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_S32;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_S32;
		    auTextFmt[2] = EURASIA_PDS_DOUTT1_TEXFORMAT_S32;
			auTextFmt[3] = EURASIA_PDS_DOUTT1_TEXFORMAT_S32;
			#else
		    auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			auTextFmt[2] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			auTextFmt[3] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			#endif /* defined(SGX543) || defined(SGX544) || defined(SGX554) */

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[1] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[2] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[3] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX543) */

			uTagSize[0] = 1;
			uTagSize[1] = 1;
			uTagSize[2] = 1;
			uTagSize[3] = 1;
		    break;
	    }
		case USP_TEXTURE_FORMAT_RGB_F32:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;
			auTextFmt[2] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[1] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[2] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			uTagSize[1] = 1;
			uTagSize[2] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_IF32:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_AF32:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_LF32:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_LF32AF32:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			auSwizzle[1] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			uTagSize[1] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_DF32:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F32;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_RGBA_F16:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F1616;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_F1616;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_GR;
			auSwizzle[1] = EURASIA_PDS_DOUTT3_SWIZ_GR;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			uTagSize[1] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_RGB_F16:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F1616;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_F16;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_GR;
			auSwizzle[1] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			uTagSize[1] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_IF16:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F16;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_RRRR;
			uTagSize[0] = 2;
			#else
			uTagSize[0] = 1;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
			
			break;
		}

		case USP_TEXTURE_FORMAT_AF16:
		{
			#if defined(SGX545)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_A_F16;
			abUseExtFmt[0] = IMG_TRUE;
			uTagSize[0] = 1;
			#else
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F16;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R000;
			uTagSize[0] = 2;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F16;
			uTagSize[0] = 1;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			break;
		}

		case USP_TEXTURE_FORMAT_LF16:
		{
			#if defined(SGX545)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_L_F16_REP;
			abUseExtFmt[0] = IMG_TRUE;
			uTagSize[0] = 2;
			#else
            #if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F16;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_1RRR;
			uTagSize[0] = 2;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F16;
			uTagSize[0] = 1;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			break;
		}

		case USP_TEXTURE_FORMAT_LF16AF16:
		{
			#if defined(SGX545)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_L_F16_A_F16;
			abUseExtFmt[0] = IMG_TRUE;
			uTagSize[0] = 2;
			#else
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F1616;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_GRRR;
			uTagSize[0] = 2;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_F1616;
			uTagSize[0] = 1;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			break;
		}

		case USP_TEXTURE_FORMAT_DU16:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U16;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_IU16:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U16;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_R;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_PVRT2BPP_RGBA:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP;
			
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
			abUseExtFmt[0] = IMG_TRUE;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_PVRT2BPP_RGB:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_1BGR;
			abUseExtFmt[0] = IMG_TRUE;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_PVRT4BPP_RGBA:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
			abUseExtFmt[0] = IMG_TRUE;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_PVRT4BPP_RGB:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_1BGR;
			abUseExtFmt[0] = IMG_TRUE;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_PVRTII2BPP_RGBA:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
			abUseExtFmt[0] = IMG_TRUE;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_PVRTII2BPP_RGB:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_1BGR;
			abUseExtFmt[0] = IMG_TRUE;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_PVRTII4BPP_RGBA:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
			abUseExtFmt[0] = IMG_TRUE;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_PVRTII4BPP_RGB:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_1BGR;
			abUseExtFmt[0] = IMG_TRUE;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_PVRTIII:
		{
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII;

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
			abUseExtFmt[0] = IMG_TRUE;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_YUYV:
		{
			#if defined(SGX545)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_C0_YUYV;
			#else
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_YUV422;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_VYUY_CSC0;
			abUseExtFmt[0] = IMG_TRUE;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_YUY2;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			uTagSize[0] = 1;
			break;
		}
		case USP_TEXTURE_FORMAT_YVYU:
		{
			#if defined(SGX545)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_C0_YVYU;
			abUseExtFmt[0] = IMG_TRUE;
			#else
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_YUV422;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_UYVY_CSC0;
			abUseExtFmt[0] = IMG_TRUE;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_YUY2;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			uTagSize[0] = 1;
			break;
		}		
		case USP_TEXTURE_FORMAT_UYVY:
		{
			#if defined(SGX545)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_C0_UYVY;
			#else
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_YUV422;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_YVYU_CSC0;
			abUseExtFmt[0] = IMG_TRUE;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_UYVY;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			uTagSize[0] = 1;
			break;
		}
		case USP_TEXTURE_FORMAT_VYUY:
		{
			#if defined(SGX545)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_C0_VYUY;
			abUseExtFmt[0] = IMG_TRUE;
			#else
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_YUV422;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_YUYV_CSC0;
			abUseExtFmt[0] = IMG_TRUE;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_UYVY;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			uTagSize[0] = 1;
			break;
		}

		case USP_TEXTURE_FORMAT_C0_YUV420_2P_UV:
		{
			#if defined(SGX545)
			#if defined(FIX_HW_BRN_32951)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_L8;
			abUseExtFmt[0] = IMG_TRUE;
			uTagSize[0] = 1;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_L8A8;
			abUseExtFmt[1] = IMG_TRUE;
			uTagSize[1] = 1;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_C0_YUV420_2P_UV;
			abUseExtFmt[0] = IMG_TRUE;
			uTagSize[0] = 1;
			#endif
			#else
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_YUV420_2P;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_YVU_CSC0;
			abUseExtFmt[0] = IMG_TRUE;
			uTagSize[0] = 1;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_U88;
			uTagSize[0] = 1;
			uTagSize[1] = 1;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			break;
		}
		case USP_TEXTURE_FORMAT_C0_YUV420_3P_UV:
		{
			#if defined(SGX545)
			#if defined(FIX_HW_BRN_32951)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_L8;
			abUseExtFmt[0] = IMG_TRUE;
			uTagSize[0] = 1;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_L8;
			abUseExtFmt[1] = IMG_TRUE;
			uTagSize[1] = 1;
			auTextFmt[2] = EURASIA_PDS_DOUTT1_TEXFORMAT_L8;
			abUseExtFmt[2] = IMG_TRUE;
			uTagSize[2] = 1;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_C0_YUV420_3P;
			abUseExtFmt[0] = IMG_TRUE;
			uTagSize[0] = 1;
			#endif
			#else
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_YUV420_3P;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_YVU_CSC0;
			abUseExtFmt[0] = IMG_TRUE;
			uTagSize[0] = 1;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
			auTextFmt[1] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
			auTextFmt[2] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8;
			uTagSize[0] = 1;
			uTagSize[1] = 1;
 			uTagSize[2] = 1;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			break;
		}

		default:
		{
			/*
				We cannot fail at this point. Hence use defaults.
			*/
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_U8888;
			auSwizzle[0] = EURASIA_PDS_DOUTT3_SWIZ_ABGR;
			#else
			auTextFmt[0] = EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			uTagSize[0] = 1;

			bValidFmt = IMG_TRUE;
			break;
		}
	}

	if(bValidFmt)
	{
		IMG_UINT32 uPlnIdx;

		/*
			Set the texture id
		*/
		psTexCtrWrds->uTextureIdx = (IMG_UINT16)uTexIdx;

		for(uPlnIdx=0; uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; uPlnIdx++)
		{
			psTexCtrWrds->auWords[uPlnIdx] = auTextFmt[uPlnIdx];

			psTexCtrWrds->auTexWrdIdx[uPlnIdx] = (IMG_UINT16)-1;

			#if defined(EURASIA_PDS_DOUTT0_TEXFEXT)
			psTexCtrWrds->abExtFmt[uPlnIdx] = abUseExtFmt[uPlnIdx];
			#endif /* defined(EURASIA_PDS_DOUTT0_TEXFEXT) */
			
			#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
			psTexCtrWrds->abReplicate[uPlnIdx] = abChanReplct[uPlnIdx];
			#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			psTexCtrWrds->auSwizzle[uPlnIdx] = auSwizzle[uPlnIdx];
			psTexCtrWrds->abMinPack[uPlnIdx] = abMinPack[uPlnIdx];
			#endif

			#if defined(SGX545)
			psTexCtrWrds->aeUnPackFmts[uPlnIdx] = USP_REGFMT_UNKNOWN;
			#endif /* defined(SGX545) */

			psTexCtrWrds->auTagSize[uPlnIdx] = uTagSize[uPlnIdx];
		}
		psTexCtrWrds->bUsed = IMG_TRUE;
	}

	return bValidFmt;
}

static IMG_BOOL RearrangeTextureSwizzle(PUSP_CONTEXT			psContext,
										PUSP_TEX_FORMAT			psTexFormat,
										PUSP_TEX_CHAN_FORMAT	psTexChanFormat)
/*****************************************************************************
 Name:		RearrangeTextureSwizzle

 Purpose:	Rearranges texture swizzle according to user supplied input.

 Inputs:	psContext		- The current USP execution context
            psTexFormat		- The input texture format

 Outputs:	psTexChanFormat	- The rearranged swizzle will be put here

 Returns:	IMG_TRUE if successful IMG_FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32 uChanIdx;
	USP_CHAN_SWIZZLE eChanSwizzle;
	USP_CHAN_SWIZZLE aeChanSwizzle[USP_TEXFORM_CHAN_COUNT];

	PVR_UNREFERENCED_PARAMETER(psContext);

	/*
		Copy the default swizzle values
	*/
	for(uChanIdx=0; uChanIdx<USP_TEXFORM_CHAN_COUNT; uChanIdx++)
	{
		aeChanSwizzle[uChanIdx] = 
			psTexChanFormat->aeChanSwizzle[uChanIdx];
	}

	/*
		Rearrange the swizzle
	*/
	for(uChanIdx=0; uChanIdx<USP_TEXFORM_CHAN_COUNT; uChanIdx++)
	{
		eChanSwizzle = psTexFormat->aeChanSwizzle[uChanIdx];

		if((IMG_UINT32)eChanSwizzle != uChanIdx)
		{

			if((eChanSwizzle >= USP_CHAN_SWIZZLE_CHAN_0) 
				&& 
			   (eChanSwizzle <= USP_CHAN_SWIZZLE_CHAN_3))
			{
				aeChanSwizzle[uChanIdx] = 
					psTexChanFormat->aeChanSwizzle[(IMG_UINT32)eChanSwizzle];
			}
			else if(eChanSwizzle == USP_CHAN_SWIZZLE_ONE)
			{
				aeChanSwizzle[uChanIdx] = 
					USP_CHAN_SWIZZLE_ONE;
			}
			else if(eChanSwizzle == USP_CHAN_SWIZZLE_ZERO)
			{
				aeChanSwizzle[uChanIdx] = 
					USP_CHAN_SWIZZLE_ZERO;
			}
			else
			{
				/*
					Invalid input data
				*/
				return IMG_FALSE;
			}
		}
			
	}

	/*
		Copy the newly rearranged swizzle
	*/
	for(uChanIdx=0; uChanIdx<USP_TEXFORM_CHAN_COUNT; uChanIdx++)
	{
		psTexChanFormat->aeChanSwizzle[uChanIdx] = 
			aeChanSwizzle[uChanIdx];
			
	}

	return IMG_TRUE;
}

static IMG_BOOL GenerateUSPTextureFormatInfo(PUSP_CONTEXT	       psContext,
											 PUSP_TEX_FORMAT       psTexFormat,
											 PUSP_TEX_CHAN_FORMAT  psTexChanFormat)
/*****************************************************************************
 Name:		GenerateUSPTextureFormatInfo

 Purpose:	Generates internal USP texture format info.

 Inputs:	psContext	    - The current USP execution context
            psTexFormat     - The input texture format

 Outputs:	psTexChanFormat	- The internal representation of texture format

 Returns:	IMG_TRUE if successful IMG_FALSE otherwise.
*****************************************************************************/
{
	IMG_BOOL bIsValidTexFmt =  IMG_TRUE;
	PVR_UNREFERENCED_PARAMETER(psContext);
	
	switch(psTexFormat->eTexFmt)
	{
		case USP_TEXTURE_FORMAT_A4R4G4B4:
		case USP_TEXTURE_FORMAT_A1R5G5B5:
		case USP_TEXTURE_FORMAT_B8G8R8A8:
		case USP_TEXTURE_FORMAT_R8G8B8A8:
		case USP_TEXTURE_FORMAT_PVRT2BPP_RGBA:
		case USP_TEXTURE_FORMAT_PVRT4BPP_RGBA:
		case USP_TEXTURE_FORMAT_PVRTII2BPP_RGBA:
		case USP_TEXTURE_FORMAT_PVRTII4BPP_RGBA:
		case USP_TEXTURE_FORMAT_PVRTIII:		
	    case USP_TEXTURE_FORMAT_RGBA_UNORM8:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#endif /* defined(SGX_FEATURE_USE_VEC34) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_R5G6B5:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_ONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#endif /* defined(SGX_FEATURE_USE_VEC34) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_PVRT2BPP_RGB:
		case USP_TEXTURE_FORMAT_PVRT4BPP_RGB:
		case USP_TEXTURE_FORMAT_PVRTII2BPP_RGB:
		case USP_TEXTURE_FORMAT_PVRTII4BPP_RGB:
		{
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_ONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;
			#endif /* defined(SGX_FEATURE_USE_VEC34) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_YUYV:
		{
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;
			#else
			#if defined(SGX545)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_V;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_U;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_Y;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;
            #endif /* defined(SGX545) */
            #endif /* defined(SGX_FEATURE_USE_VEC34) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_UYVY:
		{
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;
			#else
			#if defined(SGX545)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_V;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_U;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_Y;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;
            #endif /* defined(SGX545) */
            #endif /* defined(SGX_FEATURE_USE_VEC34) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_C0_YUV420_2P_UV:
		{
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#else
			#if defined(SGX545) 
			#if defined(FIX_HW_BRN_32951)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_Y;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_U;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_V;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 1;
			psTexChanFormat->auChunkSize[1] = 2;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#endif
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_Y;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_U;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_V;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 1;
			psTexChanFormat->auChunkSize[1] = 2;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
            #endif /* defined(SGX545) */
            #endif /* defined(SGX_FEATURE_USE_VEC34) */

			break;
		}
		case USP_TEXTURE_FORMAT_C0_YUV420_3P_UV:
		{
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#else
			#if defined(SGX545) 
			#if defined(FIX_HW_BRN_32951)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_Y;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_U;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_V;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 1;
			psTexChanFormat->auChunkSize[1] = 1;
			psTexChanFormat->auChunkSize[2] = 1;
			psTexChanFormat->auChunkSize[3] = 0;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#endif
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_Y;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_U;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_V;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 1;
			psTexChanFormat->auChunkSize[1] = 1;
			psTexChanFormat->auChunkSize[2] = 1;
			psTexChanFormat->auChunkSize[3] = 0;
            #endif /* defined(SGX545) */
            #endif /* defined(SGX_FEATURE_USE_VEC34) */

			break;
		}		
		case USP_TEXTURE_FORMAT_I8:
		{
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX545)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_0;
			#else
            #if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
            #endif /* defined(SGX_FEATURE_USE_VEC34) */
            #endif /* defined(SGX545) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_A8:
		{
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX545)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_ZERO;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_ZERO;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_ZERO;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_ZERO;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_ZERO;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_ZERO;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
            #endif /* defined(SGX_FEATURE_USE_VEC34) */
            #endif /* defined(SGX545) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_L8:
		{
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX545)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_ONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_ONE;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;
            #endif /* defined(SGX_FEATURE_USE_VEC34) */
            #endif /* defined(SGX545) */ 
			
			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_L8A8:
		{			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX545)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
            #endif /* defined(SGX_FEATURE_USE_VEC34) */
            #endif /* defined(SGX545) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_RGBA_F16:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 4;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_RGB_F16:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 2;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_IF16:
		{
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

			psTexChanFormat->auChunkSize[0] = 8;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_0;

			psTexChanFormat->auChunkSize[0] = 2;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
			break;
		}
		case USP_TEXTURE_FORMAT_AF16:
		{
			#if defined(SGX545)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_ZERO;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_1;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#else
			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_ZERO;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_ZERO;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_ZERO;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

			psTexChanFormat->auChunkSize[0] = 8;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_0;

			psTexChanFormat->auChunkSize[0] = 2;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
            #endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */
            #endif /* defined(SGX545) */

			break;
		}
		case USP_TEXTURE_FORMAT_LF16:
		{
			#if defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_ONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

			psTexChanFormat->auChunkSize[0] = 8;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 2;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#endif /* defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554) */
			break;
		}
		case USP_TEXTURE_FORMAT_LF16AF16:
		{
			#if defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554)
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

			psTexChanFormat->auChunkSize[0] = 8;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#else
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_1;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			#endif /* defined(SGX545) || defined(SGX543) || defined(SGX544) || defined(SGX554) */
			break;
		}
		case USP_TEXTURE_FORMAT_RGBA_F32: 
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F32;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 4;
			psTexChanFormat->auChunkSize[2] = 4;
			psTexChanFormat->auChunkSize[3] = 4;
			break;
		}
	    case USP_TEXTURE_FORMAT_RGBA_U8:
	    {
		    psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#endif /* defined(SGX_FEATURE_USE_VEC34) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_U16:
	    {
		    psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 4;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_UNORM16:
	    {
		    psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 4;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_U32:
	    {
		    psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
		    psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
		    psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
		    psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
		    psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U32;
		    psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U32;
		    psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U32;
		    psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U32;

		    psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
		    psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
		    psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
		    psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

		    psTexChanFormat->auChunkSize[0] = 4;
		    psTexChanFormat->auChunkSize[1] = 4;
		    psTexChanFormat->auChunkSize[2] = 4;
		    psTexChanFormat->auChunkSize[3] = 4;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_S8:
	    {
		    psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_S8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_S8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_S8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_S8;

			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#endif /* defined(SGX_FEATURE_USE_VEC34) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_S16:
	    {
		    psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_S16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_S16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_S16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_S16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 4;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
		    break;
	    }
	    case USP_TEXTURE_FORMAT_RGBA_S32:
	    {
		    psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
		    psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
		    psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
		    psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
		    psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_S32;
		    psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_S32;
		    psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_S32;
		    psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_S32;

		    psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
		    psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
		    psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
		    psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

		    psTexChanFormat->auChunkSize[0] = 4;
		    psTexChanFormat->auChunkSize[1] = 4;
		    psTexChanFormat->auChunkSize[2] = 4;
		    psTexChanFormat->auChunkSize[3] = 4;
		    break;
	    }
		case USP_TEXTURE_FORMAT_RGB_F32:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F32;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 4;
			psTexChanFormat->auChunkSize[2] = 4;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_IF32:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F32;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_0;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_AF32:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F32;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_0;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_LF32:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F32;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_LF32AF32:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F32;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_1;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 4;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_DF32:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_F32;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_F32;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_DU16:
		{
			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U16;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U16;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U16;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U16;

			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_ONE;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_ZERO;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_0;

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
		case USP_TEXTURE_FORMAT_IU16:
        {
            psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
            psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_NONE;
            psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_NONE;
            psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_NONE;
            
            psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U16;
            psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U16;
            psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U16;
            psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U16;

            psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
            psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
            psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
            psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_0;

            psTexChanFormat->auChunkSize[0] = 4;
            psTexChanFormat->auChunkSize[1] = 0;
            psTexChanFormat->auChunkSize[2] = 0;
            psTexChanFormat->auChunkSize[3] = 0;
            break;
        }
		default:
		{
			/*
				We cannot fail at this point. Hence use defaults.
			*/
			bIsValidTexFmt =  IMG_FALSE;

			psTexChanFormat->aeChanContent[0] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[1] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[2] = USP_CHAN_CONTENT_DATA;
			psTexChanFormat->aeChanContent[3] = USP_CHAN_CONTENT_DATA;
			
			psTexChanFormat->aeChanForm[0] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[1] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[2] = USP_CHAN_FORMAT_U8;
			psTexChanFormat->aeChanForm[3] = USP_CHAN_FORMAT_U8;

			#if defined(SGX_FEATURE_USE_VEC34)
			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#else
			psTexChanFormat->aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
			psTexChanFormat->aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
			psTexChanFormat->aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
			psTexChanFormat->aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;
			#endif /* defined(SGX_FEATURE_USE_VEC34) */

			psTexChanFormat->auChunkSize[0] = 4;
			psTexChanFormat->auChunkSize[1] = 0;
			psTexChanFormat->auChunkSize[2] = 0;
			psTexChanFormat->auChunkSize[3] = 0;
			break;
		}
	}

	/*
		Rearrange swizzle according to user input
	*/
	RearrangeTextureSwizzle(psContext, psTexFormat, psTexChanFormat);

	return bIsValidTexFmt;
}

#if defined(SGX_FEATURE_TAG_UNPACK_RESULT)
/*****************************************************************************
 Name:		DecideTextureUnPackingFormat

 Purpose:	Decides texture unpacking format.

 Inputs:	psContext		- The current USP execution context
			psShader		- The USP shader
            psSample		- The sample to decide texture format for
			psSampChanDesc	- The default texture channel description 

 Outputs:   psTexCtrWrds    - The set of texture control words for this 
                              texture

 Returns:	IMG_TRUE if successful IMG_FALSE otherwise.
*****************************************************************************/
static IMG_BOOL DecideTextureUnPackingFormat(PUSP_CONTEXT				psContext, 
											 PUSP_SHADER				psShader,
											 PUSP_SAMPLE				psSample,
											 PUSP_SAMPLER_CHAN_DESC		psSampChanDesc,
											 PUSP_SHDR_TEX_CTR_WORDS	psTexCtrWrds)
{
	IMG_UINT32			uTexIdx = psSample->uTextureIdx;
	USP_REGFMT			eDestFmt = USP_REGFMT_UNKNOWN;
	IMG_UINT32			uChanIdx;
	IMG_UINT32			uUnpackedMask = 0;
	USP_REGFMT			ePreSampledFmtConvert;
	USP_TEXTURE_FORMAT	eTexFmt = 
		psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt;
	
	PVR_UNREFERENCED_PARAMETER(psShader);
	
	/*
		Get the destination format
	*/
	for(uChanIdx=0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		if((psSample->psSampleUnpack->asDest[uChanIdx].eFmt == USP_REGFMT_F32) || 
			(psSample->psSampleUnpack->asDest[uChanIdx].eFmt == USP_REGFMT_F16))
		{
			eDestFmt = psSample->psSampleUnpack->asDest[uChanIdx].eFmt;
		}
	}
	
	/* Only use format conversion for single plane formats */
	#if defined(SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT)
	if(!(((eDestFmt == USP_REGFMT_F32) || (eDestFmt == USP_REGFMT_F16)) && !IsSamplingMultiPlane(psTexCtrWrds)))
	{
		return IMG_FALSE;
	}
	#endif /* defined(SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT) */
	
	{
		/*
			Is there any format used
		*/
		if(eDestFmt != USP_REGFMT_UNKNOWN)
		{
			IMG_BOOL bFormatChanged = IMG_TRUE;
			USP_TEX_CHAN_FORMAT sTexChanFormat;
			USP_SHDR_TEX_CTR_WORDS sTexCtrWrds;

			/*
				Get default values for unpacked texture results 
			*/
			sTexChanFormat = psSampChanDesc->sTexChanFormat;

			/*
				Get default texture control words
			*/
			sTexCtrWrds = *psTexCtrWrds;

			if(psSample->bNonDependent)
			{
				ePreSampledFmtConvert = USP_REGFMT_UNKNOWN;

				/* Try to find the pre-sampled texture load entry for this sample */
				if(USPInputDataGetPreSampledData(psShader->psInputData, 
					uTexIdx, USP_PRESAMPLED_DATA_CHUNKIDX_RESERVED, (IMG_UINT16)-1, 
					psSample->eCoord, psSample->bProjected, eDestFmt))
				{
					ePreSampledFmtConvert = eDestFmt;
				}
				else if(USPInputDataGetPreSampledData(psShader->psInputData, 
					uTexIdx, USP_PRESAMPLED_DATA_CHUNKIDX_RESERVED, (IMG_UINT16)-1, 
					psSample->eCoord, psSample->bProjected, USP_REGFMT_F32))
				{
					ePreSampledFmtConvert = USP_REGFMT_F32;
				}
				else if(USPInputDataGetPreSampledData(psShader->psInputData, 
					uTexIdx, USP_PRESAMPLED_DATA_CHUNKIDX_RESERVED, (IMG_UINT16)-1, 
					psSample->eCoord, psSample->bProjected, USP_REGFMT_F16))
				{
					ePreSampledFmtConvert = USP_REGFMT_F16;
				}

				if(ePreSampledFmtConvert == USP_REGFMT_UNKNOWN)
				{
					return IMG_FALSE;
				}

				eDestFmt = ePreSampledFmtConvert;
			}

			/*
				Unpacking to F32 format.
			*/
			if(eDestFmt == USP_REGFMT_F32)
			{
				/*
					Set texture format to F32
				*/
				sTexChanFormat.aeChanForm[0] = USP_CHAN_FORMAT_F32;
				sTexChanFormat.aeChanForm[1] = USP_CHAN_FORMAT_F32;
				sTexChanFormat.aeChanForm[2] = USP_CHAN_FORMAT_F32;
				sTexChanFormat.aeChanForm[3] = USP_CHAN_FORMAT_F32;

				if(sTexCtrWrds.abExtFmt[0])
				{
					#if defined(SGX_FEATURE_TAG_SWIZZLE)
					if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP) ||
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP))
					{
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 16;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 4;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else
					{
						bFormatChanged = IMG_FALSE;
					}
					#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

					#if defined(SGX545)
					if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_L8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_L8A8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_L_F16_REP) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_L_F16_A_F16))
					{
						/*
							For all these formats swap is not required for U8/F16 to F32 conversion 
							done by hardware
						*/
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 16;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 4; 

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else if(sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A_F16)
					{
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_1;

						sTexChanFormat.auChunkSize[0] = 8;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 2;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}					
					else
					{
						bFormatChanged = IMG_FALSE;
					}
					#endif /* defined(SGX545) */
				}
				else
				{
					#if defined(SGX_FEATURE_TAG_SWIZZLE)
					if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_R5G6B5) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8 && eTexFmt == USP_TEXTURE_FORMAT_I8) ||
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8 && eTexFmt == USP_TEXTURE_FORMAT_A8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8 && eTexFmt == USP_TEXTURE_FORMAT_L8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U88 && eTexFmt == USP_TEXTURE_FORMAT_L8A8) ||
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_F16 && eTexFmt == USP_TEXTURE_FORMAT_IF16) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_F16 && eTexFmt == USP_TEXTURE_FORMAT_AF16) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_F16 && eTexFmt == USP_TEXTURE_FORMAT_LF16) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616 && eTexFmt == USP_TEXTURE_FORMAT_LF16AF16) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8888 && eTexFmt == USP_TEXTURE_FORMAT_B8G8R8A8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8888 && eTexFmt == USP_TEXTURE_FORMAT_R8G8B8A8))
					{
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 16;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 4; 

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else
					{
						bFormatChanged = IMG_FALSE;
					}
					#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

					#if defined(SGX545)
					if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8) && 
						(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt == 
						USP_TEXTURE_FORMAT_I8))
					{
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_0;

						sTexChanFormat.auChunkSize[0] = 4;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 1;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616) && 
						(!(sTexCtrWrds.abExtFmt[1])) && 
						(sTexCtrWrds.auWords[1] == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616) && 
						(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt == 
						USP_TEXTURE_FORMAT_RGBA_F16))
					{
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 8;
						sTexChanFormat.auChunkSize[1] = 8;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 2;
						sTexCtrWrds.auTagSize[1] = 2;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;
						sTexCtrWrds.aeUnPackFmts[1] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616) && 
						(!(sTexCtrWrds.abExtFmt[1])) && 
						(sTexCtrWrds.auWords[1] == EURASIA_PDS_DOUTT1_TEXFORMAT_F16) && 
						(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt == 
						USP_TEXTURE_FORMAT_RGB_F16))
					{
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

						sTexChanFormat.auChunkSize[0] = 8;
						sTexChanFormat.auChunkSize[1] = 4;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 2;
						sTexCtrWrds.auTagSize[1] = 1;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;
						sTexCtrWrds.aeUnPackFmts[1] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_F16) && 
						(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt == 
						USP_TEXTURE_FORMAT_IF16))
					{
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_0;

						sTexChanFormat.auChunkSize[0] = 4;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 1;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else if(((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP) || 
						(sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP) ||
					    (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP) || 
					    (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP))
						&& 
						((eTexFmt == USP_TEXTURE_FORMAT_PVRT2BPP_RGB) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRT4BPP_RGB) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRTII2BPP_RGB) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRTII4BPP_RGB)))
					{
						/*
							Swap red and blue for U8 to F32 conversion done by hardware
						*/
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

						sTexChanFormat.auChunkSize[0] = 16;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 4;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else if(((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP) || 
						(sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP) ||
					    (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP) || 
					    (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP))
						&& 
						((eTexFmt == USP_TEXTURE_FORMAT_PVRT2BPP_RGBA) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRT4BPP_RGBA) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRTII2BPP_RGBA) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRTII4BPP_RGBA)))
					{
						/*
							Swap red and blue for U8 to F32 conversion done by hardware
						*/
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 16;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 4;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8) ||
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII))
					{
						/*
							Swap red and blue for U8 to F32 conversion done by hardware
						*/
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 16;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 4;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F32;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else
					{
						bFormatChanged = IMG_FALSE;
					}
					#endif /* defined(SGX545) */
				}
			}
			else if(eDestFmt == USP_REGFMT_F16)
			{	
				/*
					Set texture format to F16
				*/
				sTexChanFormat.aeChanForm[0] = USP_CHAN_FORMAT_F16;
				sTexChanFormat.aeChanForm[1] = USP_CHAN_FORMAT_F16;
				sTexChanFormat.aeChanForm[2] = USP_CHAN_FORMAT_F16;
				sTexChanFormat.aeChanForm[3] = USP_CHAN_FORMAT_F16;

				if(sTexCtrWrds.abExtFmt[0])
				{
					#if defined(SGX_FEATURE_TAG_SWIZZLE)
					if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP) ||
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP))
					{
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 8;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 2;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F16;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else
					{
						bFormatChanged = IMG_FALSE;
					}
					#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

					#if defined(SGX545)
					if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_L8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_L8A8))
					{
						/*
							Swap is already done by hardware for U8 to F16 unpacking
						*/
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 8;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 2;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F16;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else
					{
						bFormatChanged = IMG_FALSE;
					}
					#endif /* defined(SGX545) */
				}
				else
				{
					#if defined(SGX_FEATURE_TAG_SWIZZLE)
					if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_R5G6B5) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8 && eTexFmt == USP_TEXTURE_FORMAT_I8) ||
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8 && eTexFmt == USP_TEXTURE_FORMAT_A8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8 && eTexFmt == USP_TEXTURE_FORMAT_L8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U88 && eTexFmt == USP_TEXTURE_FORMAT_L8A8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8888 && eTexFmt == USP_TEXTURE_FORMAT_B8G8R8A8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8888 && eTexFmt == USP_TEXTURE_FORMAT_R8G8B8A8))
					{
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 8;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 2; 

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F16;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else
					{
						bFormatChanged = IMG_FALSE;
					}
					#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

					#if defined(SGX545)
					if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_U8) && 
						(psContext->asSamplerDesc[uTexIdx].sTexFormat.eTexFmt == 
						USP_TEXTURE_FORMAT_I8))
					{
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_0;

						sTexChanFormat.auChunkSize[0] = 4;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 1;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F16;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else if((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8) || 
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8) ||
					   (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII))
					{
						/*
							Swap is already done by hardware for U8 to F16 unpacking
						*/
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 8;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 2;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F16;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else if(((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP) || 
						(sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP) ||
					    (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP) || 
					    (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP))
						&& 
						((eTexFmt == USP_TEXTURE_FORMAT_PVRT2BPP_RGB) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRT4BPP_RGB) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRTII2BPP_RGB) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRTII4BPP_RGB)))
					{
						/*
							Swap red and blue for U8 to F16 conversion done by hardware
						*/
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_ONE;

						sTexChanFormat.auChunkSize[0] = 8;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 2;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F16;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else if(((sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP) || 
						(sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP) ||
					    (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP) || 
					    (sTexCtrWrds.auWords[0] == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP))
						&& 
						((eTexFmt == USP_TEXTURE_FORMAT_PVRT2BPP_RGBA) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRT4BPP_RGBA) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRTII2BPP_RGBA) || 
						 (eTexFmt == USP_TEXTURE_FORMAT_PVRTII4BPP_RGBA)))
					{
						/*
							Swap red and blue for U8 to F32 conversion done by hardware
						*/
						sTexChanFormat.aeChanSwizzle[0] = USP_CHAN_SWIZZLE_CHAN_0;
						sTexChanFormat.aeChanSwizzle[1] = USP_CHAN_SWIZZLE_CHAN_1;
						sTexChanFormat.aeChanSwizzle[2] = USP_CHAN_SWIZZLE_CHAN_2;
						sTexChanFormat.aeChanSwizzle[3] = USP_CHAN_SWIZZLE_CHAN_3;

						sTexChanFormat.auChunkSize[0] = 8;
						sTexChanFormat.auChunkSize[1] = 0;
						sTexChanFormat.auChunkSize[2] = 0;
						sTexChanFormat.auChunkSize[3] = 0;

						sTexCtrWrds.auTagSize[0] = 2;

						sTexCtrWrds.aeUnPackFmts[0] = USP_REGFMT_F16;

						uUnpackedMask = USP_DESTMASK_FULL;
					}
					else
					{
						bFormatChanged = IMG_FALSE;
					}
					#endif /* defined(SGX545) */
				}
			}
			else
			{
				return IMG_FALSE;
			}
			
			if(bFormatChanged)
			{

				/*
					Set changed texture control words
				*/
				*psTexCtrWrds = sTexCtrWrds;
				psTexCtrWrds->bUsed = IMG_TRUE;

				/*
					Rearrange swizzle according to user input
				*/
				RearrangeTextureSwizzle(psContext, 
					&(psContext->asSamplerDesc[uTexIdx].sTexFormat), 
					&sTexChanFormat);

				/*
					Set the unpacked texture format
				*/
				psSampChanDesc->sTexChanFormat = sTexChanFormat;

				/*
					Indicate that unpacking is used on this sampler
				*/
				psSample->bSamplerUnPacked = IMG_TRUE;

				/*
					Set the unpacked sampler mask
				*/
				psSample->uUnpackedMask = uUnpackedMask;

			}
			return bFormatChanged;
		}
	}
	return IMG_FALSE;
}
#endif /* defined(SGX_FEATURE_TAG_UNPACK_RESULT) */

static IMG_VOID SetTextureChanFlags(PUSP_CONTEXT			psContext, 
									IMG_UINT32				uTexIdx, 
									PUSP_SAMPLER_CHAN_DESC	psSampChanDesc)
/*****************************************************************************
 Name:		SetTextureChanFlags

 Purpose:	Sets sampler flags according to user inputs.

 Inputs:	psContext		- The current USP execution context
			uTexIdx			- The texture id
			psSampChanDesc	- The internal channel represenation for the 
			                  given sampler

 Outputs:	none

 Returns:	none
*****************************************************************************/
{
	psSampChanDesc->bAnisoEnabled = 
		psContext->asSamplerDesc[uTexIdx].bAnisoEnabled;
	
	psSampChanDesc->bGammaEnabled = 
		psContext->asSamplerDesc[uTexIdx].bAnisoEnabled;
}

#if defined(SGX_FEATURE_TAG_SWIZZLE)
/*****************************************************************************
 Name:		DecideTextureSwizzle

 Purpose:	Decides texture hardware swizzle on SGX543.

 Inputs:	psContext		- The current USP execution context
			psShader		- The USP shader
            psSample		- The sample to decide texture swizzle for
			psSampChanDesc	- The default texture channel description 

 Outputs:   psTexCtrWrds    - The set of texture control words for this 
                              texture

 Returns:	IMG_TRUE if successful IMG_FALSE otherwise.
*****************************************************************************/
static IMG_BOOL DecideTextureSwizzle(PUSP_CONTEXT				psContext, 
									 PUSP_SHADER				psShader, 
									 PUSP_SAMPLE				psSample, 
									 PUSP_SAMPLER_CHAN_DESC		psSampChanDesc, 
									 PUSP_SHDR_TEX_CTR_WORDS	psTexCtrWrds)
{
	IMG_UINT32 uSwizzleIdx;
	IMG_UINT32 uTexFmtChannelCount;

	IMG_UINT32 auChanSelect[USP_MAX_SAMPLE_CHANS];
	IMG_UINT32 auTexChanDef[USP_MAX_SAMPLE_CHANS];
	IMG_UINT32 auNewTexChanDef[USP_MAX_SAMPLE_CHANS];
	IMG_UINT32 uChanIdx;
	IMG_UINT32 uEndRange;
	IMG_UINT16 uHWSwizzle;
	IMG_UINT16 uNewHWSwizzle = (IMG_UINT16)0;
	IMG_PUINT16 puXChannelSwizzle;

	static const IMG_UINT16 au4ChannelSwizzle[] = 
	{
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_B, UFREG_SWIZ_A) /* EURASIA_PDS_DOUTT3_SWIZ_ABGR */,
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_B, UFREG_SWIZ_G, UFREG_SWIZ_R, UFREG_SWIZ_A) /* EURASIA_PDS_DOUTT3_SWIZ_ARGB */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_A, UFREG_SWIZ_B, UFREG_SWIZ_G, UFREG_SWIZ_R) /* EURASIA_PDS_DOUTT3_SWIZ_RGBA */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_A, UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_B) /* EURASIA_PDS_DOUTT3_SWIZ_BGRA */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_B, UFREG_SWIZ_1) /* EURASIA_PDS_DOUTT3_SWIZ_1BGR */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_B, UFREG_SWIZ_G, UFREG_SWIZ_R, UFREG_SWIZ_1) /* EURASIA_PDS_DOUTT3_SWIZ_1RGB */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_1, UFREG_SWIZ_B, UFREG_SWIZ_G, UFREG_SWIZ_R) /* EURASIA_PDS_DOUTT3_SWIZ_RGB1 */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_1, UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_B) /* EURASIA_PDS_DOUTT3_SWIZ_BGR1 */
	};

	static const IMG_UINT16 au3ChannelSwizzle[] = 
	{
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_B, UFREG_SWIZ_1) /* EURASIA_PDS_DOUTT3_SWIZ_BGR */,
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_B, UFREG_SWIZ_G, UFREG_SWIZ_R, UFREG_SWIZ_1) /* EURASIA_PDS_DOUTT3_SWIZ_RGB */
	};

	static const IMG_UINT16 au2ChannelSwizzle[] = 
	{
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_2, UFREG_SWIZ_2) /* EURASIA_PDS_DOUTT3_SWIZ_GR */,
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_0, UFREG_SWIZ_0) /* EURASIA_PDS_DOUTT3_SWIZ_00GR */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_G) /* EURASIA_PDS_DOUTT3_SWIZ_GRRR */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_G, UFREG_SWIZ_G, UFREG_SWIZ_G, UFREG_SWIZ_R) /* EURASIA_PDS_DOUTT3_SWIZ_RGGG */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_R, UFREG_SWIZ_G) /* EURASIA_PDS_DOUTT3_SWIZ_GRGR */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_2, UFREG_SWIZ_2) /* EURASIA_PDS_DOUTT3_SWIZ_RG */
	};

	static const IMG_UINT16 au1ChannelSwizzle[] = 
	{
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_2, UFREG_SWIZ_2, UFREG_SWIZ_2) /* EURASIA_PDS_DOUTT3_SWIZ_R */,
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_0, UFREG_SWIZ_0, UFREG_SWIZ_0) /* EURASIA_PDS_DOUTT3_SWIZ_000R */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_1, UFREG_SWIZ_1, UFREG_SWIZ_1) /* EURASIA_PDS_DOUTT3_SWIZ_111R */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_R) /* EURASIA_PDS_DOUTT3_SWIZ_RRRR */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_0) /* EURASIA_PDS_DOUTT3_SWIZ_0RRR */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_R, UFREG_SWIZ_1) /* EURASIA_PDS_DOUTT3_SWIZ_1RRR */,
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_0, UFREG_SWIZ_0, UFREG_SWIZ_0, UFREG_SWIZ_R) /* EURASIA_PDS_DOUTT3_SWIZ_R000 */, 
		UFREG_ENCODE_SWIZ(UFREG_SWIZ_1, UFREG_SWIZ_1, UFREG_SWIZ_1, UFREG_SWIZ_R) /* EURASIA_PDS_DOUTT3_SWIZ_R111 */
	};

	PVR_UNREFERENCED_PARAMETER(psShader);
	PVR_UNREFERENCED_PARAMETER(psContext);

	/*
		No swizzle in the shader for this sample.
		Also hardware swizzle cannot be used for multi plane formats
	*/
	if(psSample->uSwizzle == UFREG_SWIZ_NONE || IsSamplingMultiPlane(psTexCtrWrds))
	{
		return IMG_FALSE;
	}

	uSwizzleIdx = psTexCtrWrds->auSwizzle[0] >> EURASIA_PDS_DOUTT3_SWIZ_SHIFT;

	if(psTexCtrWrds->abExtFmt[0])
	{
		switch(psTexCtrWrds->auWords[0])
		{
			case EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII:
			{
				puXChannelSwizzle = (IMG_PUINT16)au4ChannelSwizzle;
				uTexFmtChannelCount = 4;
				uEndRange = 7;
				break;
			}

			default:
			{
				return IMG_FALSE;
				break;
			}
		}
	}
	else
	{
		switch(psTexCtrWrds->auWords[0])
		{
			case EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_U8888:
			{
				puXChannelSwizzle = (IMG_PUINT16)au4ChannelSwizzle;
				uTexFmtChannelCount = 4;
				uEndRange = 7;
				break;
			}

			case EURASIA_PDS_DOUTT1_TEXFORMAT_R5G6B5:
			{
				puXChannelSwizzle = (IMG_PUINT16)au3ChannelSwizzle;
				uTexFmtChannelCount = 3;
				uEndRange = 1;
				break;
			}

			case EURASIA_PDS_DOUTT1_TEXFORMAT_U88:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_F1616:
			{
				puXChannelSwizzle = (IMG_PUINT16)au2ChannelSwizzle;
				uTexFmtChannelCount = 2;
				uEndRange = 5;
				break;
			}

			case EURASIA_PDS_DOUTT1_TEXFORMAT_U8:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_U16:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_F16:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_F32:
			{
				puXChannelSwizzle = (IMG_PUINT16)au1ChannelSwizzle;
				uTexFmtChannelCount = 1;
				uEndRange = 7;
				break;
			}

			default:
			{
				return IMG_FALSE;
				break;
			}
		}
	}

	/* Invalid value for hardware swizzle */
	if(uSwizzleIdx>uEndRange)
	{
		return IMG_FALSE;
	}
	else
	{
		/* Get the hardware swizzle */
		uHWSwizzle = puXChannelSwizzle[uSwizzleIdx];
	}

	/* Get channel form of shader swizzle */
	for(uChanIdx=0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		auChanSelect[uChanIdx] = (IMG_UINT32)((psSample->uSwizzle >> (uChanIdx*3)) & 0x7);
	}

	/* Get channel form of selected hardware swizzle */
	for(uChanIdx=0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		auTexChanDef[uChanIdx] = (IMG_UINT32)((uHWSwizzle >> (uChanIdx*3)) & 0x7);
	}

	/* Rearrange hardware swizzle according to user swizzle */
	for(uChanIdx=0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		if((auChanSelect[uChanIdx]>=0) && (auChanSelect[uChanIdx]<=3))
		{
			uNewHWSwizzle |= (IMG_UINT16)((auTexChanDef[auChanSelect[uChanIdx]] & 0x7) << (uChanIdx*3));
			auNewTexChanDef[uChanIdx] = auTexChanDef[auChanSelect[uChanIdx]];
		}
		else if((auChanSelect[uChanIdx]== UFREG_SWIZ_1) || (auChanSelect[uChanIdx] == UFREG_SWIZ_0))
		{
			uNewHWSwizzle |= (IMG_UINT16)((auChanSelect[uChanIdx] & 0x7) << (uChanIdx*3));
			auNewTexChanDef[uChanIdx] = auChanSelect[uChanIdx];
		}
		else
		{
			/* This swizzle value is not supported by hardware */ 
			return IMG_FALSE;
		}
	}

	/* Does hardware supports this swizzle */
	for(uSwizzleIdx=0; uSwizzleIdx<=uEndRange; uSwizzleIdx++)
	{
		if(puXChannelSwizzle[uSwizzleIdx] == uNewHWSwizzle)
		{
			break;
		}
	}

	if(uSwizzleIdx>uEndRange)
	{
		/* Hardware does not support this swizzle */
		return IMG_FALSE;
	}

	/* Set texture channel content information */
	for(uChanIdx=0; uChanIdx<USP_MAX_SAMPLE_CHANS; uChanIdx++)
	{
		if((auNewTexChanDef[uChanIdx]>=0) && (auNewTexChanDef[uChanIdx]<=3))
		{
			psSampChanDesc->sTexChanFormat.aeChanContent[uChanIdx] = USP_CHAN_CONTENT_DATA;
		}
		else if(auNewTexChanDef[uChanIdx] == UFREG_SWIZ_1)
		{
			psSampChanDesc->sTexChanFormat.aeChanContent[uChanIdx] = USP_CHAN_CONTENT_ONE;
		}
		else if(auNewTexChanDef[uChanIdx] == UFREG_SWIZ_0)
		{
			psSampChanDesc->sTexChanFormat.aeChanContent[uChanIdx] = USP_CHAN_CONTENT_ZERO;
		}
		else
		{
			/* This swizzle value is not supported by hardware */ 
			return IMG_FALSE;
		}
	}

	psSample->uSwizzle = UFREG_SWIZ_NONE;

	psTexCtrWrds->auSwizzle[0] = uSwizzleIdx << EURASIA_PDS_DOUTT3_SWIZ_SHIFT;

	return IMG_TRUE;
}

static IMG_BOOL IsTextureUsedConsistently(PUSP_SHADER	psShader,
										  PUSP_SAMPLE	psSample, 
										  PSAMPLES_LIST	psTexSamplesList)
/*****************************************************************************
 Name:		IsTextureUsedConsistently

 Purpose:	Checks if a particular texture is used consistently.

 Inputs:	psShader			- The USP shader
			psSample			- The sample to check consistency for
			psTexSamplesList	- The list of samples to check consistency

 Outputs:	none

 Returns:	IMG_TRUE if texture used consistently IMG_FALSE otherwise.
*****************************************************************************/
{
	PSAMPLES_LIST	psTexSamples;
	IMG_BOOL bSamplesConsistent = IMG_TRUE;

	/*
		Hardware swizzle handling is not supported for multi plane formats
	*/
	if(IsSamplingMultiPlane(&(psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx])))
	{
		bSamplesConsistent = IMG_FALSE;
	}

	for(psTexSamples = psTexSamplesList;
		psTexSamples && bSamplesConsistent;
		psTexSamples = psTexSamples->psNext)
	{
		PUSP_SAMPLE	psSample2;
		/* Samples to compare are of same type */
		if(psSample->bNonDependent == psTexSamples->psSample->bNonDependent)
		{
			psSample2 = psTexSamples->psSample;

			if((psSample != psSample2) && 
			   (psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].aeUnPackFmts[0] == 
				psShader->psTexCtrWrds[psSample2->uTexCtrWrdIdx].aeUnPackFmts[0]))
			{
				if(psSample->uOrgSwizzle != psSample2->uOrgSwizzle)
				{
					bSamplesConsistent = IMG_FALSE;
				}
			}
		}
	}

	return bSamplesConsistent;
}
#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

IMG_INTERNAL 
IMG_BOOL DecideFinalTextureFormats(PUSP_CONTEXT	psContext,
								   PUSP_SHADER	psShader)
/*****************************************************************************
 Name:		DecideFinalTextureFormats

 Purpose:	Decides final texture formats.

 Inputs:	psContext 	 - The current USP execution context
			psShader	 - The USP shader to decide chunk sampling for

 Outputs:	none

 Returns:	IMG_TRUE if successful IMG_FALSE otherwise.
*****************************************************************************/
{

	TEX_SAMPLES_LIST *psTexFormats;

	for(psTexFormats = (psShader->psUsedTexFormats);
		psTexFormats;
		psTexFormats = psTexFormats->psNext)
	{
		PSAMPLES_LIST psTexSamples;

		for(psTexSamples = (psTexFormats->psList);
			psTexSamples;
			psTexSamples = psTexSamples->psNext)
		{
			USP_SAMPLER_CHAN_DESC	sSampChanDesc;
			USP_SHDR_TEX_CTR_WORDS	sTexCtrWrds;
			IMG_UINT32				uPlnIdx;
			IMG_UINT32				uTexIdx = 
				psTexSamples->psSample->uTextureIdx;

			/*
				Initalize to not set
			*/
			sTexCtrWrds.bUsed = IMG_FALSE;
			for(uPlnIdx=0; uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; uPlnIdx++)
			{
				sTexCtrWrds.auWords[uPlnIdx] = (IMG_UINT32)-1;
				
				sTexCtrWrds.auTexWrdIdx[uPlnIdx] = (IMG_UINT16)-1;
				
				sTexCtrWrds.abExtFmt[uPlnIdx] = IMG_FALSE;
				
				sTexCtrWrds.abReplicate[uPlnIdx] = IMG_FALSE;
				
				sTexCtrWrds.aeUnPackFmts[uPlnIdx] = USP_REGFMT_UNKNOWN;
				
				sTexCtrWrds.auSwizzle[uPlnIdx] = (IMG_UINT32)-1;
				
				sTexCtrWrds.abMinPack[uPlnIdx] = IMG_FALSE;
				
				sTexCtrWrds.auTagSize[uPlnIdx] = 0;
			}

			/*
				Set flags passed by user
			*/
			SetTextureChanFlags(psContext, uTexIdx, &sSampChanDesc);

			/* 
				Get default values
			*/
			GenerateUSPTextureFormatInfo(psContext, 
				&(psContext->asSamplerDesc[uTexIdx].sTexFormat), 
				&(sSampChanDesc.sTexChanFormat));

			/*
				Get default texture control words
			*/
			SetDefaultTextureControlWords(psContext, uTexIdx, &sTexCtrWrds, 
				&(psTexSamples->psSample->uUnpackedMask));

			#if defined(SGX_FEATURE_TAG_UNPACK_RESULT)
			/*
				Decide unpacking format
			*/
			DecideTextureUnPackingFormat(psContext, psShader, 
				psTexSamples->psSample, &sSampChanDesc, &sTexCtrWrds);
			#endif /* defined(SGX_FEATURE_TAG_UNPACK_RESULT) */

			#if defined(SGX_FEATURE_TAG_SWIZZLE)
			/*
				Decide swizzle for dependent samples only first. First 
				iteration counts the format conversion for non-dependent 
				samples that is required to calculate swizzle. Another 
				iteration is required to count swizzle for non-dependent 
				samples after getting the format conversion in first 
				iteration.
			*/
			if(!psTexSamples->psSample->bNonDependent)
			{
				/*
					Decide what hardware swizzle to use 
				*/
				DecideTextureSwizzle(psContext, psShader, psTexSamples->psSample, 
					&sSampChanDesc, &sTexCtrWrds);
			}
			#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

			/*
				Update the sample texture channel info
			*/
			psTexSamples->psSample->sSamplerDesc = sSampChanDesc;

			if(sTexCtrWrds.bUsed)
			{
				#if defined(SGX_FEATURE_TAG_SWIZZLE)
				/*
					On SGX543 swizzle is calculated later for non-depedent 
					samples
				*/
				if(psTexSamples->psSample->bNonDependent)
				{
					for(uPlnIdx=0; 
						uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; 
						uPlnIdx++)
					{
						sTexCtrWrds.auSwizzle[uPlnIdx] =(IMG_UINT32)-1;
					}
				}
				#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

				/*
					Add the texture control words to the list
				*/
				psTexSamples->psSample->uTexCtrWrdIdx = 
					AddSampleTextureControlWords(psShader, psTexSamples->psSample, &sTexCtrWrds);
			}
		}
	}

	#if defined(SGX_FEATURE_TAG_SWIZZLE)
	/*
		This iteration decides hardware swizzle for non-depedent samples
	*/
	for(psTexFormats = (psShader->psUsedTexFormats);
		psTexFormats;
		psTexFormats = psTexFormats->psNext)
	{
		PSAMPLES_LIST psTexSamples;
		for(psTexSamples = (psTexFormats->psList);
			psTexSamples;
			psTexSamples = psTexSamples->psNext)
		{
			if(psTexSamples->psSample->bNonDependent)
			{
				USP_SAMPLER_CHAN_DESC	sSampChanDesc;
				USP_SHDR_TEX_CTR_WORDS	sTexCtrWrds;
				USP_SHDR_TEX_CTR_WORDS	sDefaultTexCtrWrds;
				IMG_BOOL bSwizzleCalcRequired = IMG_TRUE;
				IMG_UINT32 uPlnIdx;

				/*
					Get the old definitions
				*/
				sSampChanDesc = psTexSamples->psSample->sSamplerDesc;
				sTexCtrWrds = psShader->psTexCtrWrds[psTexSamples->psSample->uTexCtrWrdIdx];

				/*
					Do we need to calculate the hardware swizzle
				*/
				for(uPlnIdx=0; 
					(uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT) && bSwizzleCalcRequired; 
					uPlnIdx++)
				{
					if(sTexCtrWrds.auSwizzle[uPlnIdx] != (IMG_UINT32)-1)
					{
						bSwizzleCalcRequired = IMG_FALSE;
					}
				}

				/*
					Get default texture swizzle values
				*/
				SetDefaultTextureControlWords(psContext, psTexSamples->psSample->uTextureIdx, &sDefaultTexCtrWrds, 
					&(psTexSamples->psSample->uUnpackedMask));

				if(bSwizzleCalcRequired)
				{
					for(uPlnIdx=0; 
						uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; 
						uPlnIdx++)
					{
						sTexCtrWrds.auSwizzle[uPlnIdx] = sDefaultTexCtrWrds.auSwizzle[uPlnIdx];
					}
				}

			    if(IsTextureUsedConsistently(psShader, psTexSamples->psSample, psTexFormats->psList))
				{
					if(bSwizzleCalcRequired)
					{
						/*
							It is possible to decide what hardware swizzle to use 
						*/
						DecideTextureSwizzle(psContext, psShader, psTexSamples->psSample, 
							&sSampChanDesc, &sTexCtrWrds);

					}
					else
					{
						/*
							Calculate the hardware swizzle to use 
						*/
						DecideTextureSwizzle(psContext, psShader, psTexSamples->psSample, 
							&sSampChanDesc, &sDefaultTexCtrWrds);

						/* 
							It must match with the already calculated swizzle
						*/
						for(uPlnIdx=0; 
							uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; 
							uPlnIdx++)
						{
							if(sDefaultTexCtrWrds.auSwizzle[uPlnIdx] != 
							   psShader->psTexCtrWrds[psTexSamples->psSample->uTexCtrWrdIdx].auSwizzle[uPlnIdx])
							{
								/* not possible! */
								USP_DBGPRINT(("DecideFinalTextureFormats: Mismatch between calculated and required hardware swizzle\n"));
								return IMG_FALSE;
							}
						}
					}
				}

				/*
					Update the swizzle
				*/
				if(bSwizzleCalcRequired)
				{
					/*
						Set the calculated swizzle
					*/
					for(uPlnIdx=0; 
						uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; 
						uPlnIdx++)
					{
						psShader->psTexCtrWrds[psTexSamples->psSample->uTexCtrWrdIdx].auSwizzle[uPlnIdx] = 
							sTexCtrWrds.auSwizzle[uPlnIdx];
					}
				}
				/*
					Update the sample texture channel info
				*/
				psTexSamples->psSample->sSamplerDesc = sSampChanDesc;

			}
		}
	}
	#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

	return IMG_TRUE;
}

/******************************************************************************
 End of file (USP_SAMPLE.C)
******************************************************************************/

