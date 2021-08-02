/******************************************************************************
 Name           : USP_INPUTDATA.C

 Title          : Input data handling for the USP

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

 Description    : Code to record and manage the input data for a shader,
                  including iterations, pre-sampled textures and texture-state.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.34 $

 Modifications  :
 $Log: usp_inputdata.c $
******************************************************************************/
#include <string.h>
#include <memory.h>
#include "img_types.h"
#include "sgxdefs.h"
#include "usp_typedefs.h"
#include "usc.h"
#include "usp.h"
#include "hwinst.h"
#include "uspbin.h"
#include "uspshrd.h"
#include "usp_inputdata.h"

/*****************************************************************************
 Name:		USPInputDataGetPreSampledData

 Purpose:	Lookup details about pre-sampled data for a texture chunk

 Inputs:	psInputData		- Data describing the input data for the shader
			uTextureIdx		- Index of the texture to be sampled
			uChunkIdx		- Index of the chunk to be sampled
			uTexCtrWrdIdx	- Index of the texture control word entry for 
							  this input data
			uCoordIdx		- Index of the texture-coordinates used for 
							  the sample
			bProjected		- Whether the coordinates were projected or not
			eFmtConvert		- The conversion format for the sample

 Outputs:	none

 Returns:	Information describing the requedted pre-sampled texture-chunk
*****************************************************************************/
IMG_INTERNAL PUSP_PRESAMPLED_DATA USPInputDataGetPreSampledData(
											PUSP_INPUT_DATA			psInputData,
											IMG_UINT32				uTextureIdx,
											IMG_UINT32				uChunkIdx,
											IMG_UINT16				uTexCtrWrdIdx,
											USP_ITERATED_DATA_TYPE	eCoord,
											IMG_BOOL				bProjected,
											USP_REGFMT				eFmtConvert)
{
	PUSP_PRESAMPLED_DATA	psPreSampledData;
	PUSP_PRESAMPLED_DATA	psPreSampledDataEnd;

	/*
		Find the requested texture-chunk in the list of all pre-sampled data
	*/
	psPreSampledData		= psInputData->psPreSampledData;
	psPreSampledDataEnd		= psPreSampledData +
							  psInputData->uPreSampledDataCount;

	while (psPreSampledData < psPreSampledDataEnd)
	{
		if	(
				(psPreSampledData->uTextureIdx		== uTextureIdx) &&
				(psPreSampledData->uChunkIdx		== uChunkIdx) &&
				(psPreSampledData->eCoord			== eCoord) &&
				((uTexCtrWrdIdx == (IMG_UINT16)-1) || (psPreSampledData->uTexCtrWrdIdx	== uTexCtrWrdIdx)) && 
				(psPreSampledData->bProjected		== bProjected) && 
				(psPreSampledData->eFmtConvert		== eFmtConvert)
			)
		{
			return psPreSampledData;
		}
		psPreSampledData++;
	}

	return IMG_NULL;
}

/*****************************************************************************
 Name:		USPInputDataGetIteratedData

 Purpose:	Lookup details about an iterated input value

 Inputs:	psInputData	- Data describing the input data for the shader
			uRegNum		- The register index used to reference the iterated
						  data

 Outputs:	none

 Returns:	Information describing the requedted iterated data
*****************************************************************************/
IMG_INTERNAL PUSP_ITERATED_DATA USPInputDataGetIteratedData(
												PUSP_INPUT_DATA	psInputData,
												IMG_UINT32		uRegNum)
{
	PUSP_ITERATED_DATA	psIteratedData;
	PUSP_ITERATED_DATA	psIteratedDataEnd;

	/*
		Find the requested iterated data
	*/
	psIteratedData		= psInputData->psIteratedData;
	psIteratedDataEnd	= psIteratedData + psInputData->uIteratedDataCount;

	while (psIteratedData < psIteratedDataEnd)
	{
		if	(
				(uRegNum >= psIteratedData->uRegNum) &&
				(uRegNum < (psIteratedData->uRegNum + 
							psIteratedData->uRegCount))
			)
		{
			return psIteratedData;
		}
		psIteratedData++;
	}

	return IMG_NULL;
}

/*****************************************************************************
 Name:		USPInputDataGetTexStateData

 Purpose:	Lookup details about the state for a chunk of texture data

 Inputs:	psInputData		- Data describing the input data for the shader
			uTextureIdx		- Index of the texture to be sampled
			uChunkIdx		- Index of the chunk to be sampled
			uTexCtrWrdIdx	- Index of the texture control word entry for 
							  this input data

 Outputs:	none

 Returns:	Information describing the requedted iterated data
*****************************************************************************/
IMG_INTERNAL PUSP_TEXSTATE_DATA USPInputDataGetTexStateData(
												PUSP_INPUT_DATA	psInputData,
												IMG_UINT32		uTextureIdx,
												IMG_UINT32		uChunkIdx,
												IMG_UINT32		uTexCtrWrdIdx)
{
	PUSP_TEXSTATE_DATA	psTexStateData;
	PUSP_TEXSTATE_DATA	psTexStateDataEnd;

	/*
		Find the requested iterated data
	*/
	psTexStateData		= psInputData->psTexStateData;
	psTexStateDataEnd	= psTexStateData + psInputData->uTexStateDataCount;

	while (psTexStateData < psTexStateDataEnd)
	{
		if	(
				(psTexStateData->uTextureIdx == uTextureIdx) &&
				(psTexStateData->uChunkIdx	 == uChunkIdx) && 
				(psTexStateData->uTexCtrWrdIdx == (IMG_UINT16)uTexCtrWrdIdx)
			)
		{
			return psTexStateData;
		}
		psTexStateData++;
	}

	return IMG_NULL;
}

/*****************************************************************************
 Name:		USPInputDataCalcPARegUse

 Purpose:	Calculate PA register indexes for the iterated and pre-sampled
			input data

 Inputs:	psInputData		- Data describing the input data for the shader
			psShader		- The shader that the input-data is from

 Outputs:	puPARegCount	- The number of PA registers used (including
							  reserved PAs).

 Returns:	TRUE/FALSE to indicate success or failure
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPInputDataCalcPARegUse(PUSP_INPUT_DATA	psInputData,
											   PUSP_SHADER		psShader,
											   IMG_PUINT32		puPARegCount)
{
	IMG_UINT32	uIteratedDataEnd;
	IMG_UINT32	uPreSampledDataEnd;
	IMG_UINT32	uRegNum;

	/*
		Lookup the number of PAs used for the iterated and pre-sampled data
	*/
	if	(psInputData->uIteratedDataCount > 0)
	{
		PUSP_ITERATED_DATA psLastIteratedData;

		psLastIteratedData = psInputData->psIteratedData +
							 (psInputData->uIteratedDataCount - 1);

		uIteratedDataEnd = psLastIteratedData->uRegNum +
						   psLastIteratedData->uRegCount;
	}
	else
	{
		uIteratedDataEnd = 0;
	}

	if	(psInputData->uPreSampledDataCount > 0)
	{
		PUSP_PRESAMPLED_DATA psLastPreSampledData;

		psLastPreSampledData = psInputData->psPreSampledData + 
							   (psInputData->uPreSampledDataCount - 1);

		uPreSampledDataEnd = psLastPreSampledData->uRegNum +
							 psLastPreSampledData->uRegCount;
	}
	else
	{
		uPreSampledDataEnd = 0;
	}

	/*
		Find the first unused PA register
	*/
	if	(uIteratedDataEnd > uPreSampledDataEnd)
	{
		uRegNum = uIteratedDataEnd;
	}
	else
	{
		uRegNum = uPreSampledDataEnd;
	}
	uRegNum += psShader->psProgDesc->uExtraPARegs;

	/*
		Return the number of PA regs used
	*/
	*puPARegCount = uRegNum;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInputDataAddTexStateData

 Purpose:	Add a texture-state data to the input-data for a shader

			NB: This must be done once the texture-formats for all texture
				known (i.e. at finalisation).

 Inputs:	psInputData			- The input data to update
			psSample			- The dependent sample requiring 
								  texture-state data
			uTextureIdx			- The id of the texture for this sample
			uTexChunkMask		- The mask of the chunks to use
			uTexNonDepChunkMask	- The mask of non dependent chunks
			psShader			- The shader that the input data is for
			psContext			- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPInputDataAddTexStateData(
												PUSP_INPUT_DATA	psInputData,
												PUSP_SAMPLE		psSample,
												IMG_UINT32		uTextureIdx,
												IMG_UINT32		uTexChunkMask,
												IMG_UINT32		uTexNonDepChunkMask, 
												PUSP_SHADER		psShader,
												PUSP_CONTEXT	psContext)
{
	PUSP_TEXSTATE_DATA	psTexStateData;
	PUSP_TEXSTATE_DATA	psTexStateDataEnd;
	IMG_UINT32			uTexStateDataCount;
	IMG_UINT32			uChunkIdx;
	PUSP_PROGDESC		psProgDesc;
	IMG_UINT16			uTexCtrWrdIdx;

	PVR_UNREFERENCED_PARAMETER(psSample);

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	/*
		Add entries for each of the required texture-chunks
	*/
	psProgDesc			= psShader->psProgDesc;
	uTexStateDataCount	= psInputData->uTexStateDataCount;
	psTexStateData		= psInputData->psTexStateData;

	for	(uChunkIdx = 0; uChunkIdx < USP_MAX_TEXTURE_CHUNKS; uChunkIdx++)
	{
		PUSP_TEXSTATE_DATA	psChunkTexState;

		/*
			Ignore unused chunks
		*/
		if	(!(uTexChunkMask & (1 << uChunkIdx)) || (uTexNonDepChunkMask & (1 << uChunkIdx)))
		{
			continue;
		}

		uTexCtrWrdIdx = 
					psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTexWrdIdx[uChunkIdx];

		/*
			Look through the list to see if this texture chunk has already
			been added.
		*/
		psTexStateDataEnd = psTexStateData + uTexStateDataCount;

		for	(psChunkTexState = psTexStateData;
			 psChunkTexState < psTexStateDataEnd;
			 psChunkTexState++)
		{
			if	(
					(psChunkTexState->uTextureIdx == uTextureIdx) &&
					(psChunkTexState->uChunkIdx	  == uChunkIdx) && 
					(psChunkTexState->uTexCtrWrdIdx == uTexCtrWrdIdx)
				)
			{
				break;
			}
		}

		/*
			Add a new entry for the texture-state for this chunk
		*/
		if	(psChunkTexState == psTexStateDataEnd)
		{
			PUSP_TEXSTATE_DATA	psPrevTexStateData;
			IMG_UINT32			uRegNum;

			/*
				Make sure we have space for another entry
			*/
			if	(uTexStateDataCount == psInputData->uMaxTexStateData)
			{
				USP_DBGPRINT(("USPInputDataAddTexStateData: Maximum number of texture-state entries reached"));
				return IMG_FALSE;
			}

			/*
				Calculate the register location for the texture-state words

				NB: Texture-state is assumed to be packed into contiguous SA
					registers. For now, we put it after any constants that
					the USc has remapped to SA registers.
			*/
			if	(uTexStateDataCount > 0)
			{
				/*
					Put the state-data for this chunk immediate after the
					previous set.
				*/
				psPrevTexStateData = psChunkTexState - 1;

				uRegNum = psPrevTexStateData->uRegNum + 
						  psPrevTexStateData->uRegCount;
			}
			else
			{
				PUSP_PROGDESC	psProgDesc;
				IMG_UINT32		uSAUsageFlags;

				psPrevTexStateData = IMG_NULL;

				/*
					If we have been told where to put the texture-state words,
					use that as the starting point. Otherwise, put them after
					the last SA reg used for other input stuff. 
				*/
				psProgDesc		= psShader->psProgDesc;
				uSAUsageFlags	= psProgDesc->uSAUsageFlags;

				if	(uSAUsageFlags & USP_PC_SAUSAGEFLAG_TEXTURESTATE)
				{
					uRegNum = psProgDesc->uTexStateFirstSAReg;
				}
				else 
				{
					uRegNum = 0;

					if	(uRegNum <= (IMG_UINT32)(psProgDesc->uRegConstsFirstSAReg +
												 psProgDesc->uRegConstsSARegCount))
					{
						uRegNum = psProgDesc->uRegConstsFirstSAReg +
								  psProgDesc->uRegConstsSARegCount;
					}

					if	(uSAUsageFlags & USP_PC_SAUSAGEFLAG_MEMCONSTBASE)
					{
						if	(uRegNum <= psProgDesc->uMemConstBaseAddrSAReg)
						{
							uRegNum = psProgDesc->uMemConstBaseAddrSAReg + 1;
						}
					}

					if	(uSAUsageFlags & USP_PC_SAUSAGEFLAG_MEMCONSTBASE2)
					{
						if	(uRegNum <= psProgDesc->uMemConst2BaseAddrSAReg)
						{
							uRegNum = psProgDesc->uMemConst2BaseAddrSAReg + 1;
						}
					}

					if	(uSAUsageFlags & USP_PC_SAUSAGEFLAG_SCRATCHMEMBASE)
					{
						if	(uRegNum <= psProgDesc->uScratchMemBaseAddrSAReg)
						{
							uRegNum = psProgDesc->uScratchMemBaseAddrSAReg + 1;
						}
					}

					if	(uSAUsageFlags & USP_PC_SAUSAGEFLAG_INDEXTEMPMEMBASE)
					{
						if	(uRegNum <= psProgDesc->uIndexedTempBaseAddrSAReg)
						{
							uRegNum = psProgDesc->uIndexedTempBaseAddrSAReg + 1;
						}
					}
				}
			}

			#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
			/*
				Source arguments to the SMP instruction are in units of 64-bit registers
				on this core so round the starting SA register number up to the next
				64-bit register boundary.
			*/
			uRegNum = (uRegNum + SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGN - 1) & ~(SGXVEC_USE_VEC2_REGISTER_NUMBER_ALIGN - 1);
			#endif /* defined(SGX_FEATURE_UNIFIED_STORE_64BITS) */

			/*
				Add a new entry for texture-state for this chunk of texture
			*/
			psChunkTexState->uTextureIdx	= uTextureIdx;
			psChunkTexState->uChunkIdx		= uChunkIdx;
			psChunkTexState->uTexCtrWrdIdx	= psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTexWrdIdx[uChunkIdx];
			psChunkTexState->uTagSize		= psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTagSize[uChunkIdx];
			psChunkTexState->uRegCount		= USP_TEXSTATE_SIZE;
			psChunkTexState->uRegNum		= uRegNum;
			psChunkTexState->bInMemory		= IMG_FALSE;

			/*
				If the texture-state for this chunk cannot fit into the SA-regs, it
				must be loaded from memory as needed. All subsequent texture-state
				will automatically get put into memory too.
			*/
			if	(!psPrevTexStateData || !psPrevTexStateData->bInMemory)
			{
				IMG_UINT32	uLastSAReg;

				uLastSAReg = psProgDesc->uRegConstsFirstSAReg +
							 psProgDesc->uRegConstsMaxSARegCount;

				if	((uRegNum + USP_TEXSTATE_SIZE) > uLastSAReg)
				{
					psChunkTexState->uRegNum	= 0;
					psChunkTexState->bInMemory	= IMG_TRUE;
				}
			}
			else
			{
				psChunkTexState->bInMemory = psPrevTexStateData->bInMemory;
			}

			if	(psChunkTexState->bInMemory)
			{
				psInputData->uMemTexStateDataCount++;
			}
			else
			{
				psInputData->uRegTexStateDataCount++;
			}

			uTexStateDataCount++;
		}
	}

	psInputData->uTexStateDataCount = uTexStateDataCount;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInputDataFixUnusedPreSampledData

 Purpose:	For any pre-sampled texture samples whose results aren't used set the
			entries to dummy values.

 Inputs:	psShader	- The USP shader
			psInputData	- The input data to update

 Outputs:	none

 Returns:	nothing
*****************************************************************************/
IMG_INTERNAL
IMG_VOID USPInputDataFixUnusedPreSampledData(PUSP_SHADER		psShader, 
											 PUSP_INPUT_DATA	psInputData)
{
	IMG_UINT32	uPreSampledTexIdx;

	for	(uPreSampledTexIdx = 0;
		 uPreSampledTexIdx < psInputData->uPreSampledDataCount;
		 uPreSampledTexIdx++)
	{
		PUSP_PRESAMPLED_DATA	psPreSampledTex;

		psPreSampledTex = psInputData->psPreSampledData + uPreSampledTexIdx;
		if (psPreSampledTex->uChunkIdx == USP_PRESAMPLED_DATA_CHUNKIDX_RESERVED)
		{
			IMG_UINT32 uSmpIdx;
			psPreSampledTex->uChunkIdx = 0;

			if(psPreSampledTex->uTexCtrWrdIdx == (IMG_UINT16)-1)
			{

				/*
					Set the texture control word list entry index for this input data
				*/
				for(uSmpIdx=0; uSmpIdx<psShader->uTotalSmpTexCtrWrds; uSmpIdx++)
				{
					if(psShader->psTexCtrWrds[uSmpIdx].uTextureIdx == psPreSampledTex->uTextureIdx)
					{
						#if defined(SGX_FEATURE_TAG_UNPACK_RESULT)
						if(psShader->psTexCtrWrds[uSmpIdx].aeUnPackFmts[0] == 
							psPreSampledTex->eFmtConvert)
						#endif /* defined(SGX_FEATURE_TAG_UNPACK_RESULT) */
						{
							if(psShader->psTexCtrWrds[uSmpIdx].auTexWrdIdx[0] != (IMG_UINT16)-1)
							{
								psPreSampledTex->uTexCtrWrdIdx = 
									psShader->psTexCtrWrds[uSmpIdx].auTexWrdIdx[0];

								psPreSampledTex->uTagSize = 
									psShader->psTexCtrWrds[uSmpIdx].auTagSize[0];

								psPreSampledTex->eFmtConvert = 
									psShader->psTexCtrWrds[uSmpIdx].aeUnPackFmts[0];
								break;
							}
						}
					}
				}

				/*
					Compiler decided to unpack texture results using hardware but patcher did not
				*/
				if(psPreSampledTex->uTexCtrWrdIdx == (IMG_UINT16)-1)
				{
					psPreSampledTex->uTextureIdx = UNIFLEX_TEXTURE_NONE;
					psPreSampledTex->uChunkIdx = 0;
					psPreSampledTex->uTexCtrWrdIdx = (IMG_UINT16)-1;
					psPreSampledTex->uTagSize = (IMG_INT16)psPreSampledTex->uRegCount;
					psPreSampledTex->eFmtConvert = USP_REGFMT_UNKNOWN;
					psPreSampledTex->bProjected = IMG_FALSE;
					psPreSampledTex->bCentroid = IMG_FALSE;
					psPreSampledTex->eCoord = USP_ITERATED_DATA_TYPE_V0;
				}
			}
		}
	}
}

/*****************************************************************************
 Name:		USPInputDataAddPreSampledData

 Purpose:	Add a pre-sampled texture to the input-data for a shader

			NB: This must be done once the texture-formats for all texture
				known (i.e. at finalisation).

 Inputs:	psShader	- The USP shader
			psInputData	- The input data to update
			psSample	- The non-dependent sample requiring pre-sampled data
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPInputDataAddPreSampledData(
												PUSP_SHADER		psShader,
												PUSP_INPUT_DATA	psInputData,
												PUSP_SAMPLE		psSample,
												PUSP_CONTEXT	psContext)
{
	PUSP_PRESAMPLED_DATA	psPreSampledData;
	PUSP_PRESAMPLED_DATA	psPreSampledDataEnd;
	IMG_UINT32				uPreSampledDataCount;
	IMG_UINT32				uTextureIdx;
	IMG_UINT32				uChunkIdx;
	IMG_UINT16				uTexCtrWrdIdx;
	USP_ITERATED_DATA_TYPE	eCoord;
	IMG_BOOL				bProjected;
	IMG_BOOL				bCentroid;
	PUSP_TEX_CHAN_INFO		psTexChanInfo;
	IMG_UINT32				uTagSize;
	USP_REGFMT				eUnPackFmt;
	#if defined(SGX_FEATURE_TAG_SWIZZLE)
	IMG_UINT32				uHWSwizzle;
	#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	/*
		Add entries for each of the required texture-chunks
	*/
	uPreSampledDataCount	= psInputData->uPreSampledDataCount;
	psPreSampledData		= psInputData->psPreSampledData;

	psTexChanInfo	= &psSample->sTexChanInfo;
	uTextureIdx		= psSample->uTextureIdx;
	eCoord			= psSample->eCoord;
	bProjected		= psSample->bProjected;
	bCentroid		= psSample->bCentroid;

	for	(uChunkIdx = 0; uChunkIdx < USP_MAX_TEXTURE_CHUNKS; uChunkIdx++)
	{
		PUSP_PRESAMPLED_DATA	psPreSampledTex;

		/*
			Ignore unused chunks
		*/
		if	(!(psTexChanInfo->uTexChunkMask & (1 << uChunkIdx)) || !(psTexChanInfo->uTexNonDepChunkMask & (1 << uChunkIdx)))
		{
			continue;
		}

		/*
			Get the texture control words index in the list
		*/
		uTexCtrWrdIdx = 
			psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTexWrdIdx[uChunkIdx];

		/*
			Get how many registers will be written by this chunck
		*/
		uTagSize = 
			psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTagSize[uChunkIdx];

		/*
			Get the unpacking format for dependent samples
		*/
		eUnPackFmt = 
			psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].aeUnPackFmts[uChunkIdx];
		
		#if defined(SGX_FEATURE_TAG_SWIZZLE)
		/* Get the selected hardware swizzle */
		uHWSwizzle = 
			psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auSwizzle[uChunkIdx];
		#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

		/*
			Look through the list to see if this texture chunk has already
			been added (or for a pre-reserved entry we can use)
		*/
		psPreSampledDataEnd	= psPreSampledData + uPreSampledDataCount;

		for	(psPreSampledTex = psPreSampledData;
			 psPreSampledTex < psPreSampledDataEnd;
			 psPreSampledTex++)
		{
			if	(
					(psPreSampledTex->uTextureIdx == uTextureIdx) &&
					(
						(psPreSampledTex->uChunkIdx	== USP_PRESAMPLED_DATA_CHUNKIDX_RESERVED) ||
						(psPreSampledTex->uChunkIdx	== uChunkIdx)
					) && 
					(psPreSampledTex->eCoord	  == eCoord) &&
					(psPreSampledTex->bProjected  == bProjected) &&
					(psPreSampledTex->bCentroid	  == bCentroid) &&
					(psPreSampledTex->eFmtConvert == eUnPackFmt)
				)
			{
				if(psPreSampledTex->uChunkIdx != USP_PRESAMPLED_DATA_CHUNKIDX_RESERVED)
				{
					if(psPreSampledTex->uTexCtrWrdIdx == uTexCtrWrdIdx)
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
		}

		/*
			Add a new entry for this texture-chunk

			NB: The register-index for all pre-sampled textures is
				calculated later.
		*/
		if	(psPreSampledTex == psPreSampledDataEnd)
		{
			IMG_UINT32	uRegCountForChunk;
			IMG_UINT32	uIteratedDataEnd;
			IMG_UINT32	uPreSampledDataEnd;
			IMG_UINT32	uRegNum;

			/*
				Find the first unused PA register
			*/
			if	(psInputData->uIteratedDataCount > 0)
			{
				PUSP_ITERATED_DATA psLastIteratedData;

				psLastIteratedData = psInputData->psIteratedData + 
									 (psInputData->uIteratedDataCount - 1);

				uIteratedDataEnd = psLastIteratedData->uRegNum + 
								   psLastIteratedData->uRegCount;
			}
			else
			{
				uIteratedDataEnd = 0;
			}

			if	(uPreSampledDataCount > 0)
			{
				PUSP_PRESAMPLED_DATA psLastPreSampledData;

				psLastPreSampledData = psInputData->psPreSampledData +
									   (uPreSampledDataCount - 1);

				uPreSampledDataEnd = psLastPreSampledData->uRegNum +
									 psLastPreSampledData->uRegCount;
			}
			else
			{
				uPreSampledDataEnd = 0;
			}

			if	(uPreSampledDataEnd > uIteratedDataEnd)
			{
				uRegNum = uPreSampledDataEnd;
			}
			else
			{
				uRegNum = uIteratedDataEnd;	
			}
			
			uPreSampledDataCount++;

			/*
				Make sure we have space for another entry
			*/
			if	(uPreSampledDataCount > psInputData->uMaxPreSampledData)
			{
				USP_DBGPRINT(("USPInputDataAddPreSampledData: Maximum number of pre-sampled tex-chunks reached"));
				return IMG_FALSE;
			}

			uRegCountForChunk =	psTexChanInfo->auNumRegsForChunk[uChunkIdx];

			psPreSampledTex->uTextureIdx	= uTextureIdx;
			psPreSampledTex->uChunkIdx		= uChunkIdx;
			psPreSampledTex->uTexCtrWrdIdx	= psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTexWrdIdx[uChunkIdx];
			psPreSampledTex->uTagSize		= psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTagSize[uChunkIdx];
			psPreSampledTex->eFmtConvert	= psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].aeUnPackFmts[uChunkIdx];
			psPreSampledTex->eCoord			= eCoord;
			psPreSampledTex->bProjected		= bProjected;
			psPreSampledTex->bCentroid		= bCentroid;
			psPreSampledTex->uRegCount		= uRegCountForChunk;
			psPreSampledTex->uRegNum		= uRegNum;

			#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
			if(uTagSize>1 && psPreSampledTex->uRegNum%2)
			{
				USP_PRESAMPLED_DATA	sPreSampledTex;
				
				sPreSampledTex = *psPreSampledTex;
				/*
					This chunk requires the regsiter to be 64-bit aligned 
					but entry is giving only 32-bit aligned. So this 
					entry cannot be used. It will be turned into a dummy 
					iteration later. Convert it to a dummy entry now.
				*/
				psPreSampledTex->uTextureIdx = UNIFLEX_TEXTURE_NONE;
				psPreSampledTex->uChunkIdx = 0;
				psPreSampledTex->uTexCtrWrdIdx = (IMG_UINT16)-1;
				psPreSampledTex->uTagSize = 1;
				psPreSampledTex->eFmtConvert = USP_REGFMT_UNKNOWN;
				psPreSampledTex->bProjected = IMG_FALSE;
				psPreSampledTex->bCentroid = IMG_FALSE;
				psPreSampledTex->eCoord = USP_ITERATED_DATA_TYPE_V0;
				psPreSampledTex->uRegCount = 1;
				uRegNum++;

				/* Now move to the next entry */
				psPreSampledTex++;

				uPreSampledDataCount++;

				/*
					Make sure we have space for another entry
				*/
				if	(uPreSampledDataCount == psInputData->uMaxPreSampledData)
				{
					USP_DBGPRINT(("USPInputDataAddPreSampledData: Maximum number of pre-sampled tex-chunks reached"));
					return IMG_FALSE;
				}

				*psPreSampledTex = sPreSampledTex;
				psPreSampledTex->uRegNum = uRegNum;
			}
			#endif /* defined(SGX_FEATURE_UNIFIED_STORE_64BITS) */
		}
		else if	(psPreSampledTex->uChunkIdx	== USP_PRESAMPLED_DATA_CHUNKIDX_RESERVED)
		{
			if(uTagSize > psPreSampledTex->uRegCount)
			{
				IMG_UINT32			uRegCountForChunk;
				IMG_UINT32			uIteratedDataEnd;
				IMG_UINT32			uPreSampledDataEnd;
				IMG_UINT32			uRegNum;
				USP_PRESAMPLED_DATA	sPreSampledTex;

				sPreSampledTex = *psPreSampledTex;

				/*
					This chunk requires more resgiters than the entry is 
					giving. So this entry cannot be used. It will be turned 
					into a dummy iteration later. Convert it to a dummy entry 
					now.
				*/
				psPreSampledTex->uTextureIdx = UNIFLEX_TEXTURE_NONE;
				psPreSampledTex->uChunkIdx = 0;
				psPreSampledTex->uTexCtrWrdIdx = (IMG_UINT16)-1;
				psPreSampledTex->uTagSize = 1;
				psPreSampledTex->eFmtConvert = USP_REGFMT_UNKNOWN;
				psPreSampledTex->bProjected = IMG_FALSE;
				psPreSampledTex->bCentroid = IMG_FALSE;
				psPreSampledTex->eCoord = USP_ITERATED_DATA_TYPE_V0;

				/*
					Add a new entry at the end. Set the count of registers to 
					what is actually required by this chunk.
				*/
				psPreSampledTex = psPreSampledDataEnd;

				/*
					Find the first unused PA register
				*/
				if	(psInputData->uIteratedDataCount > 0)
				{
					PUSP_ITERATED_DATA psLastIteratedData;

					psLastIteratedData = psInputData->psIteratedData + 
										 (psInputData->uIteratedDataCount - 1);

					uIteratedDataEnd = psLastIteratedData->uRegNum + 
									   psLastIteratedData->uRegCount;
				}
				else
				{
					uIteratedDataEnd = 0;
				}

				if	(uPreSampledDataCount > 0)
				{
					PUSP_PRESAMPLED_DATA psLastPreSampledData;

					psLastPreSampledData = psInputData->psPreSampledData +
										   (uPreSampledDataCount - 1);

					uPreSampledDataEnd = psLastPreSampledData->uRegNum +
										 psLastPreSampledData->uRegCount;
				}
				else
				{
					uPreSampledDataEnd = 0;
				}

				if	(uPreSampledDataEnd > uIteratedDataEnd)
				{
					uRegNum = uPreSampledDataEnd;
				}
				else
				{
					uRegNum = uIteratedDataEnd;	
				}

				uPreSampledDataCount++;

				/*
					Make sure we have space for another entry
				*/
				if	(uPreSampledDataCount == psInputData->uMaxPreSampledData)
				{
					USP_DBGPRINT(("USPInputDataAddPreSampledData: Maximum number of pre-sampled tex-chunks reached"));
					return IMG_FALSE;
				}

				uRegCountForChunk =	psTexChanInfo->auNumRegsForChunk[uChunkIdx];

				psPreSampledTex->uTextureIdx	= sPreSampledTex.uTextureIdx;
				psPreSampledTex->uChunkIdx		= uChunkIdx;
				psPreSampledTex->uTexCtrWrdIdx	= psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTexWrdIdx[uChunkIdx];
				psPreSampledTex->uTagSize		= psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTagSize[uChunkIdx];
				psPreSampledTex->eFmtConvert	= psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].aeUnPackFmts[uChunkIdx];
				psPreSampledTex->eCoord			= sPreSampledTex.eCoord;
				psPreSampledTex->bProjected		= sPreSampledTex.bProjected;
				psPreSampledTex->bCentroid		= sPreSampledTex.bCentroid;
				psPreSampledTex->uRegCount		= uRegCountForChunk;
				psPreSampledTex->uRegNum		= uRegNum;

				#if defined(SGX_FEATURE_UNIFIED_STORE_64BITS)
				if(((eUnPackFmt == USP_REGFMT_F16) || (eUnPackFmt == USP_REGFMT_F32)) && 
				   (psPreSampledTex->uRegNum%2))
				{
					sPreSampledTex = *psPreSampledTex;
					/*
						This chunk requires the regsiter to be 64-bit aligned 
						but entry is giving only 32-bit aligned. So this 
						entry cannot be used. It will be turned into a dummy 
						iteration later. Convert it to a dummy entry now.
					*/
					psPreSampledTex->uTextureIdx = UNIFLEX_TEXTURE_NONE;
					psPreSampledTex->uChunkIdx = 0;
					psPreSampledTex->uTexCtrWrdIdx = (IMG_UINT16)-1;
					psPreSampledTex->uTagSize = 1;
					psPreSampledTex->eFmtConvert = USP_REGFMT_UNKNOWN;
					psPreSampledTex->bProjected = IMG_FALSE;
					psPreSampledTex->bCentroid = IMG_FALSE;
					psPreSampledTex->eCoord = USP_ITERATED_DATA_TYPE_V0;
					psPreSampledTex->uRegCount = 1;
					uRegNum++;
					
					/* Now move to the next entry */
					psPreSampledTex++;

					uPreSampledDataCount++;

					/*
						Make sure we have space for another entry
					*/
					if	(uPreSampledDataCount == psInputData->uMaxPreSampledData)
					{
						USP_DBGPRINT(("USPInputDataAddPreSampledData: Maximum number of pre-sampled tex-chunks reached"));
						return IMG_FALSE;
					}

					*psPreSampledTex = sPreSampledTex;
					psPreSampledTex->uRegNum = uRegNum;

				}
				#endif
			}
			else
			{
				/*
					If we found a suitable pre-reserved entry, record the chunk we
					want to load there.
				*/
				psPreSampledTex->uChunkIdx = uChunkIdx;		

				if(psPreSampledTex->uTexCtrWrdIdx == (IMG_UINT16)-1)
				{
					/*
						Set the texture control word list entry index for this input data
					*/
					if(psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTexWrdIdx[uChunkIdx] != (IMG_UINT16)-1)
					{
						psPreSampledTex->uTexCtrWrdIdx	= 
							psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTexWrdIdx[uChunkIdx];
									
						psPreSampledTex->uTagSize		= 
							psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].auTagSize[uChunkIdx];

						psPreSampledTex->eFmtConvert	= 
							psSample->psShader->psTexCtrWrds[psSample->uTexCtrWrdIdx].aeUnPackFmts[uChunkIdx];
					}
				}
			}
		}
	}

	psInputData->uPreSampledDataCount = uPreSampledDataCount;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInputDataAddIteratedData

 Purpose:	Add a iterated data to the input-data for a shader

 Inputs:	psInputData	- The input data to update
			psSample	- The non-dependent sample requiring iterated data
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPInputDataAddIteratedData(PUSP_INPUT_DATA	psInputData, 
												  PUSP_SAMPLE		psSample, 
												  PUSP_CONTEXT		psContext)
{
	PUSP_ITERATED_DATA		psIteratedData;
	PUSP_ITERATED_DATA	psIteratedDataEnd;
	IMG_UINT32				uIteratedDataCount;
	USP_ITERATED_DATA_TYPE	eCoord;
	PUSP_TEX_CHAN_INFO		psTexChanInfo;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	/*
		Add entries for each of the required texture-chunks
	*/
	uIteratedDataCount	= psInputData->uIteratedDataCount;
	psIteratedData		= psInputData->psIteratedData;

	psTexChanInfo	= &psSample->sTexChanInfo;
	eCoord			= psSample->eCoord;

	if(!(psTexChanInfo->uTexNonDepChunkMask))
	{
		return IMG_TRUE;
	}

	if(
		psTexChanInfo->uTexChunkMask && 
		psSample->bNonDependent == IMG_TRUE && 
		psTexChanInfo->uTexNonDepChunkMask != psTexChanInfo->uTexChunkMask
	)
	{
		PUSP_ITERATED_DATA	psIteratedTexData;

		psIteratedDataEnd	= psIteratedData + uIteratedDataCount;

		for	(psIteratedTexData = psIteratedData;
			 psIteratedTexData < psIteratedDataEnd;
			 psIteratedTexData++)
		{
			if(
				psIteratedTexData->eType == eCoord && 
				psIteratedTexData->uCompCount == psSample->uTexDim && 
				psIteratedTexData->eRegFmt == USP_REGFMT_F32
			)
			{
				break;
			}
		}

		/*
			Add a new entry for this texture coordinate
		*/
		if	(psIteratedTexData == psIteratedDataEnd)
		{
			IMG_UINT32	uIteratedDataEnd;
			IMG_UINT32	uPreSampledDataEnd;
			IMG_UINT32	uRegNum;

			/*
				Find the first unused PA register
			*/
			if	(uIteratedDataCount > 0)
			{
				PUSP_ITERATED_DATA psLastIteratedData;

				psLastIteratedData = psInputData->psIteratedData + 
									 (psInputData->uIteratedDataCount - 1);

				uIteratedDataEnd = psLastIteratedData->uRegNum + 
								   psLastIteratedData->uRegCount;
			}
			else
			{
				uIteratedDataEnd = 0;
			}

			if	(psInputData->uPreSampledDataCount > 0)
			{
				PUSP_PRESAMPLED_DATA psLastPreSampledData;

				psLastPreSampledData = psInputData->psPreSampledData +
									   ((psInputData->uPreSampledDataCount) - 1);

				uPreSampledDataEnd = psLastPreSampledData->uRegNum +
									 psLastPreSampledData->uRegCount;
			}
			else
			{
				uPreSampledDataEnd = 0;
			}

			if	(uPreSampledDataEnd > uIteratedDataEnd)
			{
				uRegNum = uPreSampledDataEnd;
			}
			else
			{
				uRegNum = uIteratedDataEnd;	
			}

			/*
				Make sure we have space for another entry
			*/
			if	(uIteratedDataCount == psInputData->uMaxIteratedData)
			{
				USP_DBGPRINT(("USPAddIteratedData: Maximum number of iterations reached"));
				return IMG_FALSE;
			}

			psIteratedTexData->uCompCount = psSample->uTexDim;
			psIteratedTexData->eRegFmt = USP_REGFMT_F32;
			psIteratedTexData->eType = psSample->eCoord;
			psIteratedTexData->uRegNum = uRegNum;
			psIteratedTexData->uRegCount = psSample->uTexDim;
			psIteratedTexData->uItrSize = (IMG_UINT16)psSample->uTexDim;
			{
				IMG_UINT32 uI;
				for(uI=0; uI<psSample->uTexDim; uI++)
				{
					psIteratedTexData->aeRegChan[uI] = 0;
					psIteratedTexData->aeRegOff[uI] = uI;
				}
			}
			psIteratedTexData->bCentroid = IMG_FALSE;
			uIteratedDataCount++;
		}
	}

	psInputData->uIteratedDataCount = uIteratedDataCount;
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInputDataGetIteratedDataForNonDepSample

 Purpose:	Lookup details about an iterated value for a non dependent sample

 Inputs:	psInputData	- Data describing the input data for the shader
			psSample	- The non-dependent sample requiring iterated data
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	Information describing the requested iterated data
*****************************************************************************/
IMG_INTERNAL PUSP_ITERATED_DATA USPInputDataGetIteratedDataForNonDepSample(PUSP_INPUT_DATA	psInputData, 
																		   PUSP_SAMPLE		psSample, 
																		   PUSP_CONTEXT		psContext)
{
	PUSP_ITERATED_DATA		psIteratedData;
	PUSP_ITERATED_DATA		psIteratedDataEnd;
	IMG_UINT32				uIteratedDataCount;
	USP_ITERATED_DATA_TYPE	eCoord;
	IMG_BOOL				bCentroid;
	PUSP_TEX_CHAN_INFO		psTexChanInfo;
	
	PVR_UNREFERENCED_PARAMETER(psContext);	

	/*
		Add entries for each of the required texture-chunks
	*/
	uIteratedDataCount	= psInputData->uIteratedDataCount;
	psIteratedData		= psInputData->psIteratedData;

	psTexChanInfo	= &psSample->sTexChanInfo;
	eCoord			= psSample->eCoord;
	bCentroid		= psSample->bCentroid;

	//TODO: remove following commented code
	/*if(!(psTexChanInfo->uTexNonDepChunkMask))
	{
		return IMG_NULL;
	}*/

	if(
		psTexChanInfo->uTexChunkMask && 
		psSample->bNonDependent == IMG_TRUE && 
		psTexChanInfo->uTexNonDepChunkMask != psTexChanInfo->uTexChunkMask
	)
	{
		PUSP_ITERATED_DATA	psIteratedTexData;

		psIteratedDataEnd	= psIteratedData + uIteratedDataCount;

		for	(psIteratedTexData = psIteratedData;
			 psIteratedTexData < psIteratedDataEnd;
			 psIteratedTexData++)
		{
			if(
				psIteratedTexData->eType == eCoord && 
				psIteratedTexData->uCompCount == psSample->uTexDim && 
				psIteratedTexData->bCentroid == bCentroid && //TODO: confirm this code.
				psIteratedTexData->eRegFmt == USP_REGFMT_F32
			)
			{
				return psIteratedTexData;
			}
		}		
	}
	return IMG_NULL;
}

/*****************************************************************************
 Name:		USPInputDataAddPCInputData

 Purpose:	Record the pre-compiled iterated and pre-sampled data for a shader

 Inputs:	psInputData			- The input data to update
			uPCInputLoadCount	- The number of items in the PC input-load
								  list
			psPCInputLoads		- The list of pre-compiled input-loads
								  describing iterated and pre-sampled
								  textures
			psContext			- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPInputDataAddPCInputData(
									PUSP_INPUT_DATA			psInputData,
									IMG_UINT32				uPCInputLoadCount,
									PUSP_PC_PSINPUT_LOAD	psPCInputLoads,
									PUSP_CONTEXT			psContext)
{
	PUSP_PC_PSINPUT_LOAD	psPCInputLoad;
	PUSP_PC_PSINPUT_LOAD	psPCInputLoadEnd;
	IMG_UINT32				uIteratedDataCount;
	IMG_UINT32				uPreSampledDataCount;
	IMG_UINT32				uRegCount;
	IMG_UINT32				uTextureIdx;
	IMG_UINT32				uCompIdx;
	IMG_UINT32				uRegNum;
	USP_ITERATED_DATA_TYPE	eType;
	
	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif /* #if !defined(DEBUG)*/

	/*
		Add input-data for each entry in the PC input list
	*/
	uIteratedDataCount		= 0;
	uPreSampledDataCount	= 0;
	psPCInputLoad			= psPCInputLoads;
	psPCInputLoadEnd		= psPCInputLoad + uPCInputLoadCount;
	uRegNum					= 0;

	while (psPCInputLoad < psPCInputLoadEnd)
	{
		/*
			Extract information about the iterated value/texture-coordinate used for
			the pre-sample
		*/
		switch (psPCInputLoad->uCoord)
		{
			case USP_PC_PSINPUT_COORD_TC0:
			{
				eType = USP_ITERATED_DATA_TYPE_TC0;
				break;
			}
			case USP_PC_PSINPUT_COORD_TC1:
			{
				eType = USP_ITERATED_DATA_TYPE_TC1;
				break;
			}
			case USP_PC_PSINPUT_COORD_TC2: 
			{
				eType = USP_ITERATED_DATA_TYPE_TC2;
				break;
			}
			case USP_PC_PSINPUT_COORD_TC3: 
			{
				eType = USP_ITERATED_DATA_TYPE_TC3;
				break;
			}
			case USP_PC_PSINPUT_COORD_TC4: 
			{
				eType = USP_ITERATED_DATA_TYPE_TC4;
				break;
			}
			case USP_PC_PSINPUT_COORD_TC5: 
			{
				eType = USP_ITERATED_DATA_TYPE_TC5;
				break;
			}
			case USP_PC_PSINPUT_COORD_TC6: 
			{
				eType = USP_ITERATED_DATA_TYPE_TC6;
				break;
			}
			case USP_PC_PSINPUT_COORD_TC7: 
			{
				eType = USP_ITERATED_DATA_TYPE_TC7;
				break;
			}
			case USP_PC_PSINPUT_COORD_TC8: 
			{
				eType = USP_ITERATED_DATA_TYPE_TC8;
				break;
			}
			case USP_PC_PSINPUT_COORD_TC9: 
			{
				eType = USP_ITERATED_DATA_TYPE_TC9;
				break;
			}
			case USP_PC_PSINPUT_COORD_V0: 
			{
				eType = USP_ITERATED_DATA_TYPE_V0;
				break;
			}
			case USP_PC_PSINPUT_COORD_V1: 
			{
				eType = USP_ITERATED_DATA_TYPE_V1;
				break;
			}
			case USP_PC_PSINPUT_COORD_FOG: 
			{
				eType = USP_ITERATED_DATA_TYPE_FOG;
				break;
			}
			case USP_PC_PSINPUT_COORD_POS: 
			{
				eType = USP_ITERATED_DATA_TYPE_POS;
				break;
			}
			case USP_PC_PSINPUT_COORD_VPRIM: 
			{
				eType = USP_ITERATED_DATA_TYPE_VPRIM;
				break;
			}
			default:
			{
				USP_DBGPRINT(("USPInputDataAddPCInputData: Invalid iterated data-type"));
				return IMG_FALSE;
			}
		}

		if	(psPCInputLoad->uFlags & USP_PC_PSINPUT_FLAG_ITERATION)
		{
			PUSP_ITERATED_DATA	psIteratedData;
			USP_REGFMT			uCompFormat;
			IMG_UINT32			uBitsPerComp;
			IMG_UINT32			uCompsPerReg;
			IMG_UINT32			uRegChansPerComp;
			IMG_UINT32			uCompCount;
			IMG_UINT32			uRegChan;
			IMG_UINT32			uRegOffset;
			IMG_BOOL			bCentroid;
			IMG_UINT16			uItrSize;

			uItrSize = psPCInputLoad->uDataSize;

			/*
				Detect padding added by the USC to stop the USP from overwriting
				outputs when they are in PA registers
			*/
			if	(psPCInputLoad->uFlags & USP_PC_PSINPUT_FLAG_PADDING)
			{
				eType = USP_ITERATED_DATA_TYPE_PADDING;
			}

			/*
				Calculate the number of input-registers needed for the iterated data
			*/
			switch (psPCInputLoad->uFormat)
			{
				case USP_PC_PSINPUT_FMT_F32:
				{
					uCompFormat			= USP_REGFMT_F32;
					uBitsPerComp		= 32;
					uCompsPerReg		= 1;
					uRegChansPerComp	= 4;
					break;
				}
				case USP_PC_PSINPUT_FMT_F16:
				{
					uCompFormat			= USP_REGFMT_F16;
					uBitsPerComp		= 16;
					uCompsPerReg		= 2;
					uRegChansPerComp	= 2;
					break;
				}
				case USP_PC_PSINPUT_FMT_C10:
				{
					uCompFormat			= USP_REGFMT_C10;
					uBitsPerComp		= 10;
					uCompsPerReg		= 3;
					uRegChansPerComp	= 1;
					break;
				}
				case USP_PC_PSINPUT_FMT_U8:
				{
					uCompFormat			= USP_REGFMT_U8;
					uBitsPerComp		= 8;
					uCompsPerReg		= 4;
					uRegChansPerComp	= 1;
					break;
				}
				default:
				{
					USP_DBGPRINT(("USPInputDataAddPCInputData: invalid iterated data format"));
					return IMG_FALSE;
				}
			}

			uCompCount	= psPCInputLoad->uCoordDim;
			uRegCount	= psPCInputLoad->uDataSize;
			bCentroid	= (IMG_BOOL)((psPCInputLoad->uFlags & USP_PC_PSINPUT_FLAG_CENTROID) != 0);

			/*
				Make sure we have space for another entry
			*/
			if	(uIteratedDataCount == psInputData->uMaxIteratedData)
			{
				USP_DBGPRINT(("USPInputDataAddPCInputData: Maximum number of iterated items reached"));
				return IMG_FALSE;
			}

			/*
				Record details about the iterated value
			*/
			psIteratedData = psInputData->psIteratedData + uIteratedDataCount;

			psIteratedData->eType		= eType;
			psIteratedData->uCompCount	= uCompCount;
			psIteratedData->bCentroid	= bCentroid;
			psIteratedData->eRegFmt		= uCompFormat;
			psIteratedData->uRegCount	= uRegCount;
			psIteratedData->uRegNum		= uRegNum;
			psIteratedData->uItrSize	= uItrSize;

			/*
				Calculate the register offset and channel for each component
			*/
			uRegChan	= 0;
			uRegOffset	= 0;

			for	(uCompIdx = 0; uCompIdx < uCompCount; )
			{
				psIteratedData->aeRegOff[uCompIdx]	= uRegOffset;
				psIteratedData->aeRegChan[uCompIdx]	= uRegChan;

				uCompIdx++;
				if	(!(uCompIdx % uCompsPerReg))
				{
					uRegOffset++;
					uRegChan = 0;
				}
				else
				{
					uRegChan = (uRegChan + uRegChansPerComp) & 3;
				}
			}

			#if defined(DEBUG)
			if (uRegOffset > uRegCount)
			{
				USP_DBGPRINT(("USPInputDataAddPCInputData: Unexpected result size for iteration"));
			}
			#endif /* defined(DEBUG) */

			uRegOffset = uRegCount;
			uIteratedDataCount++;
		}
		else
		{
			PUSP_PRESAMPLED_DATA	psPreSampledData;
			IMG_BOOL				bProjected;
			IMG_BOOL				bCentroid;

			/*
				Pre-compiled texture-samples only represent the data for a single
				chunk of texture data, so they consure one PA reg only.
			*/
			uTextureIdx	= psPCInputLoad->uTexture;
			uRegCount	= psPCInputLoad->uDataSize;
			bProjected	= (IMG_BOOL)((psPCInputLoad->uFlags & USP_PC_PSINPUT_FLAG_PROJECTED) != 0);
			bCentroid	= (IMG_BOOL)((psPCInputLoad->uFlags & USP_PC_PSINPUT_FLAG_CENTROID) != 0);

			/*
				Make sure we have space for another entry
			*/
			if	(uPreSampledDataCount == psInputData->uMaxPreSampledData)
			{
				USP_DBGPRINT(("USPInputDataAddPCInputData: Maximum number of pre-sampled items reached"));
				return IMG_FALSE;
			}

			/*
				Record details about the pre-sampled texture
			*/
			psPreSampledData = psInputData->psPreSampledData + uPreSampledDataCount;
			psPreSampledData->uTextureIdx	= uTextureIdx;
			psPreSampledData->uChunkIdx		= USP_PRESAMPLED_DATA_CHUNKIDX_RESERVED;
			psPreSampledData->uTexCtrWrdIdx = (IMG_UINT16)-1;

			switch(psPCInputLoad->uFormat)
			{
				case USP_PC_PSINPUT_FMT_F32:
				{
					psPreSampledData->eFmtConvert = USP_REGFMT_F32;
					break;
				}

				case USP_PC_PSINPUT_FMT_F16:
				{
					psPreSampledData->eFmtConvert = USP_REGFMT_F16;
					break;
				}

				default:
				{
					psPreSampledData->eFmtConvert = USP_REGFMT_UNKNOWN;
					if(uRegCount>1)
					{
						USP_DBGPRINT(("USPInputDataAddPCInputData: No format conversion requires only 1 register "));
						return IMG_FALSE;
					}
					break;
				}
			}

			psPreSampledData->uTagSize		= 0;
			psPreSampledData->eCoord		= eType;
			psPreSampledData->bProjected	= bProjected;
			psPreSampledData->bCentroid		= bCentroid;
			psPreSampledData->uRegCount		= uRegCount;
			psPreSampledData->uRegNum		= uRegNum;

 			uPreSampledDataCount++;
		}

		uRegNum += uRegCount;
		psPCInputLoad++;
	}

	psInputData->uOrgIteratedDataCount		= uIteratedDataCount;
	psInputData->uIteratedDataCount			= uIteratedDataCount;
	psInputData->uOrgPreSampledDataCount	= uPreSampledDataCount;	
	psInputData->uPreSampledDataCount		= uPreSampledDataCount;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInputDataCreate

 Purpose:	Create and initialise data describing thr in[put-data for a shader

 Inputs:	psContext	- The current USP execution context

 Outputs:	none

 Returns:	The created input-data structure
*****************************************************************************/
IMG_INTERNAL PUSP_INPUT_DATA USPInputDataCreate(PUSP_CONTEXT psContext)
{
	PUSP_INPUT_DATA	psInputData;

	/*
		Create an input-data structure with empty input-lists
	*/
	psInputData = (PUSP_INPUT_DATA)psContext->pfnAlloc(sizeof(USP_INPUT_DATA));
	if	(!psInputData)
	{
		USP_DBGPRINT(("USPInputDataCreate: Failed to alloc shader input-data\n"));
	}
	else
	{
		memset(psInputData, 0, sizeof(USP_INPUT_DATA));
	}

	return psInputData;
}

/*****************************************************************************
 Name:		USPInputDataCreateIteratedDataList

 Purpose:	Allocate a list for the iterated input data to a shader

 Inputs:	psInputData			- The input-data structure to update
			uMaxIteratedData	- The maximum number of entries in the list
			psContext			- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on failure. IMG_TRUE otherwise.
*****************************************************************************/
IMG_INTERNAL 
IMG_BOOL USPInputDataCreateIteratedDataList(PUSP_INPUT_DATA	psInputData,
											IMG_UINT32		uMaxIteratedData,
											PUSP_CONTEXT	psContext)
{
	/*
		Create the list of iterated data
	*/
	if	(uMaxIteratedData > 0)
	{
		PUSP_ITERATED_DATA	psIteratedData;
		IMG_UINT32			uAllocSize;

		uAllocSize = uMaxIteratedData * sizeof(USP_ITERATED_DATA);

		psIteratedData = (PUSP_ITERATED_DATA)psContext->pfnAlloc(uAllocSize);
		if	(!psIteratedData)
		{
			USP_DBGPRINT(("USPInputDataCreateIteratedDataList: Failed to alloc list of iterated data\n"));
			return IMG_FALSE;
		}

		psInputData->uIteratedDataCount		= 0;
		psInputData->uMaxIteratedData		= uMaxIteratedData;
		psInputData->psIteratedData			= psIteratedData;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInputDataCreatePreSampledDataList

 Purpose:	Allocate a list for the pre-sampled input data to a shader

 Inputs:	psInputData			- The input-data structure to update
			uMaxPreSampledData	- The maximum number of entries in the list
			psContext			- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on failure. IMG_TRUE otherwise.
*****************************************************************************/
IMG_INTERNAL
IMG_BOOL USPInputDataCreatePreSampledDataList(PUSP_INPUT_DATA	psInputData,
											  IMG_UINT32		uMaxPreSampledData,
											  PUSP_CONTEXT		psContext)
{
	/*
		Create the list of pre-sampled texture-chunks
	*/
	if	(uMaxPreSampledData > 0)
	{
		PUSP_PRESAMPLED_DATA	psPreSampledData;
		IMG_UINT32				uAllocSize;

		uAllocSize = uMaxPreSampledData * sizeof(USP_PRESAMPLED_DATA);

		psPreSampledData = (PUSP_PRESAMPLED_DATA)psContext->pfnAlloc(uAllocSize);
		if	(!psPreSampledData)
		{
			USP_DBGPRINT(("USPInputDataCreatePreSampledDataList: Failed to alloc list of pre-sampled data"));
			return IMG_FALSE;
		}

		psInputData->uIteratedDataCount			= 0;
		psInputData->uOrgIteratedDataCount		= 0;
		psInputData->uPreSampledDataCount		= 0;
		psInputData->uOrgPreSampledDataCount	= 0;
		psInputData->uMaxPreSampledData			= uMaxPreSampledData;
		psInputData->psPreSampledData			= psPreSampledData;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInputDataCreateTexStateDataList

 Purpose:	Allocate a list for the texture-state input data to a shader

 Inputs:	psInputData			- The input-data structure to update
			uMaxTexStateData	- The maximum number of entries in the list
			psContext			- The current USP execution context

 Outputs:	none

 Returns:	IMG_FALSE on failure. IMG_TRUE otherwise.
*****************************************************************************/
IMG_INTERNAL
IMG_BOOL USPInputDataCreateTexStateDataList(PUSP_INPUT_DATA	psInputData,
											IMG_UINT32		uMaxTexStateData,
											PUSP_CONTEXT	psContext)
{
	/*
		Create the list of texture-state data
	*/
	if	(uMaxTexStateData > 0)
	{
		PUSP_TEXSTATE_DATA		psTexStateData;
		IMG_UINT32				uAllocSize;

		uAllocSize = uMaxTexStateData * sizeof(USP_TEXSTATE_DATA);

		psTexStateData = (PUSP_TEXSTATE_DATA)psContext->pfnAlloc(uAllocSize);
		if	(!psTexStateData)
		{
			USP_DBGPRINT(("USPInputDataCreate: Failed to alloc list of tex-state data"));
			return IMG_FALSE;
		}

		psInputData->uTexStateDataCount		= 0;
		psInputData->uRegTexStateDataCount	= 0;
		psInputData->uMemTexStateDataCount	= 0;
		psInputData->uMaxTexStateData		= uMaxTexStateData;
		psInputData->psTexStateData			= psTexStateData;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		USPInputDataDestroy

 Purpose:	Cleanup data describing the input data for a shader

 Inputs:	psInputData	- The input-data to be destroyed
			psContext	- The current USP execution context

 Outputs:	none

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_VOID USPInputDataDestroy(PUSP_INPUT_DATA	psInputData,
										  PUSP_CONTEXT		psContext)
{
	if	(psInputData->psIteratedData)
	{
		psContext->pfnFree(psInputData->psIteratedData);
	}
	if	(psInputData->psPreSampledData)
	{
		psContext->pfnFree(psInputData->psPreSampledData);
	}
	if	(psInputData->psTexStateData)
	{
		psContext->pfnFree(psInputData->psTexStateData);
	}
	psContext->pfnFree(psInputData);
}

/*****************************************************************************
 Name:		USPInputDataReset

 Purpose:	Reset the input-data for a shader prior to finalisation

 Inputs:	psInputData	- The input-data to reset

 Outputs:	none

 Returns:	IMG_FALSE on failure. IMG_TRUE otherwise.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL USPInputDataReset(PUSP_INPUT_DATA psInputData)
{
	IMG_UINT32 uPreSampDataIdx;
	/*
		Reset the lists of pre-sampled and iterated input-data
	*/
	psInputData->uIteratedDataCount		= psInputData->uOrgIteratedDataCount;
	psInputData->uPreSampledDataCount	= psInputData->uOrgPreSampledDataCount;
	psInputData->uTexStateDataCount		= 0;
	psInputData->uRegTexStateDataCount	= 0;
	psInputData->uMemTexStateDataCount	= 0;

	/*
		Reset the original pre-sampled input list
	*/
	for(uPreSampDataIdx = 0; 
		uPreSampDataIdx < psInputData->uPreSampledDataCount; 
		uPreSampDataIdx++)
	{
		psInputData->psPreSampledData[uPreSampDataIdx].uChunkIdx = 
			USP_PRESAMPLED_DATA_CHUNKIDX_RESERVED;
		psInputData->psPreSampledData[uPreSampDataIdx].uTexCtrWrdIdx = 
			(IMG_UINT16)-1;
		psInputData->psPreSampledData[uPreSampDataIdx].uTagSize = 0;
	}

	return IMG_TRUE;
}

/******************************************************************************
 End of file (USP_INPUTDATA.C)
******************************************************************************/

