/*****************************************************************************
 Name           : FINALISE.C

 Title          : USP shader finalisation

 C Author       : Joe Molleson

 Created        : 02/01/2002

 Copyright      : 2002-2008 by Imagination Technologies Limited.
                : All rights reserved. No part of this software, either
                : material or conceptual may be copied or distributed,
                : transmitted, transcribed, stored in a retrieval system or
                : translated into any human or computer language in any form
                : by any means, electronic, mechanical, manual or otherwise,
                : or disclosed to third parties without the express written
                : permission of Imagination Technologies Limited,
                : Home Park Estate, Kings Langley, Hertfordshire,
                : WD4 8LZ, U.K.

 Description    : Procedures to perform finalisation of a USP shader, prior
                  to the generation of HW-compatible code.

 Program Type   : 32-bit DLL

 Version        : $Revision: 1.117 $

 Modifications  
 $Log: finalise.c $
******************************************************************************/
#include <stdarg.h>
#include <string.h>
#include <memory.h>
#include "img_types.h"
#include "sgxdefs.h"
#include "usp_typedefs.h"
#include "usc.h"
#include "usp.h"
#include "uspbin.h"
#include "hwinst.h"
#include "uspshrd.h"
#include "usp_instblock.h"
#include "usp_resultref.h"
#include "usp_sample.h"
#include "usp_inputdata.h"
#if defined(DEBUG)
#include "use.h"
#endif /* #if defined(DEBUG) */
#include "finalise.h"
#include "usp_texwrite.h"

#if defined(DEBUG) && defined(USP_DECODE_HWSHADER)
#include <stdio.h>
#endif /* defined(DEBUG) && defined(USP_DECODE_HWSHADER) */

/*****************************************************************************
 Name:		FindSmpUnpack

 Purpose:	Finds USP sample unpack block for shader sample blocks.

 Inputs:	psContext	- The current USP execution context
			psSample	- The sample to find sample unpack for.
			psShader	- The USP shader to this sample belongs to.

 Outputs:	none

 Returns:	IMG_TRUE/IMG_FALSE
*****************************************************************************/
static IMG_BOOL FindSmpUnpack(	PUSP_CONTEXT	psContext,
								PUSP_SAMPLE		psSample,
								PUSP_SHADER		psShader)
{
	PUSP_SAMPLEUNPACK psSampleUnpack;

#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
#endif

	for	(psSampleUnpack = psShader->psSampleUnpacks;
		 psSampleUnpack;
		 psSampleUnpack = psSampleUnpack->psNext)
	{
		if(psSampleUnpack->uSmpID == psSample->uSmpID)
		{
			psSample->psSampleUnpack = psSampleUnpack;
			if
			(
				psSampleUnpack->bCanSampleDirectToDest && 
				(psSample->eDirectDestRegType == psSampleUnpack->asDest[0].eType) && 
				(psSample->uDirectDestRegNum == psSampleUnpack->asDest[0].uNum)
			)
			{
				psSampleUnpack->bCanSampleDirectToDest = IMG_TRUE;
			}
			else
			{
				psSampleUnpack->bCanSampleDirectToDest = IMG_FALSE;
			}
			psSample->psSampleUnpack->asSampleInsts = psSample->asSampleInsts_;
			psSample->psSampleUnpack->aeOpcodeForSampleInsts = psSample->aeOpcodeForSampleInsts_;
			psSample->psSampleUnpack->auInstDescFlagsForSampleInsts = psSample->auInstDescFlagsForSampleInsts_;
			psSample->psSampleUnpack->puBaseSampleDestRegCount = &(psSample->uBaseSampleDestRegCount_);
			psSample->psSampleUnpack->puBaseSampleTempDestRegCount = &(psSample->uBaseSampleDestRegCount_);
			if((psSample->eDirectDestRegType == psSample->psSampleUnpack->eDirectSrcRegType) && (psSample->uDirectDestRegNum == psSample->psSampleUnpack->uDirectSrcRegNum))
			{
				psSample->psSampleUnpack->bCanSampleDirectToDest = IMG_TRUE;
			}
			else
			{
				psSample->psSampleUnpack->bCanSampleDirectToDest = IMG_FALSE;
			}
			return IMG_TRUE;
		}
	}
	USP_DBGPRINT(("FindSmpUnpack: Error finding sample unpack for sample: %08u\n", psSample->uSmpID));
	return IMG_FALSE;
}

/*****************************************************************************
 Name:		HandleUSPSampleTextureFormat

 Purpose:	Add texture formats to the USP shader sample blocks i.e. required 
            for finalisation.

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader to add the pre-sampled data into.

 Outputs:	none

 Returns:	IMG_TRUE/IMG_FALSE
*****************************************************************************/
IMG_INTERNAL IMG_BOOL HandleUSPSampleTextureFormat(PUSP_CONTEXT	psContext, 
												   PUSP_SHADER	psShader)
{
	PUSP_SAMPLE		psSample;
	PUSP_INPUT_DATA	psInputData = (psShader->psInputData);

	for	(psSample = psShader->psNonDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{
		if	(!FindSmpUnpack(psContext, psSample, psShader))
		{
			USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error finding sample unpack for non-dependent samples\n"));
			return IMG_FALSE;
		}
	}
	
	for	(psSample = psShader->psDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{
		if	(!FindSmpUnpack(psContext, psSample, psShader))
		{
			USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error finding sample unpack for samples\n"));
			return IMG_FALSE;
		}
	}
	/*
		Decide final texture formats
	*/
	if(!DecideFinalTextureFormats(psContext, psShader))
	{
		USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error deciding final texture formats \n"));
		return IMG_FALSE;
	}

	/*
		Add the pre-sampled data to non-dependent samples they 
		require to input-data for the shader
	*/
	for	(psSample = psShader->psNonDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{
		if	(!USPSampleDataAdd(psSample, psContext, psShader))
		{
			USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error adding pre-sampled data to non-dependent samples\n"));
			return IMG_FALSE;
		}		
	}

	/*
		Add data for dependent samples.
	*/
	for	(psSample = psShader->psDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{		
		if	(!USPSampleDataAdd(psSample, psContext, psShader))
		{
			USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error adding data for dependent samples\n"));
			return IMG_FALSE;
		}
	}

	if(!DecideChunksSampling(psContext, psShader))
	{
		USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error deciding either to sample Non Dep. Chunks as Dep./Non Dep.\n"));
		return IMG_FALSE;
	}

	/*
		For non-dependent samples add the pre-sampled data they require to 
		input-data for the shader
	*/
	for	(psSample = psShader->psNonDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{		
		if(!USPInputDataAddIteratedData(psInputData, psSample, psContext))
		{
			USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error adding iterated data for non-dependent sample to input-data\n"));
			return IMG_FALSE;
		}

		if	(!USPInputDataAddPreSampledData(psShader, psInputData, psSample, psContext))
		{
			USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error adding pre-sampled data for non-dependent sample to input-data\n"));
			return IMG_FALSE;
		}
		
		if	(!USPInputDataAddTexStateData(psInputData, 
										  psSample, 
										  psSample->uTextureIdx, 
										  psSample->sTexChanInfo.uTexChunkMask, 
										  psSample->sTexChanInfo.uTexNonDepChunkMask, 
										  psShader,
										  psContext))
		{
			USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error adding tex-state data for dependent sample to input-data\n"));
			return IMG_FALSE;
		}
	}
	
	/*
		For any pre-sampled texture samples whose results aren't used set the
		entries to dummy values.
	*/
	USPInputDataFixUnusedPreSampledData(psShader, psInputData);

	/*
		For dependent samples add the texture-state they require to the 
		input-data for the shader
	*/
	for	(psSample = psShader->psDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{
		if	(!USPInputDataAddTexStateData(psInputData, 
										  psSample, 
										  psSample->uTextureIdx, 
										  psSample->sTexChanInfo.uTexChunkMask, 
										  psSample->sTexChanInfo.uTexNonDepChunkMask, 
										  psShader,
										  psContext))
		{
			USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error adding tex-state data for dependent sample to input-data\n"));
			return IMG_FALSE;
		}
	}

	/*
		Add texture-state for each dependent sample to the input-data for the shader
	*/
	for	(psSample = psShader->psDepSamples; 
		psSample; 
		psSample = psSample->psNext)
	{
		IMG_UINT32	uTexChunkMask;

		uTexChunkMask = psSample->sTexChanInfo.uTexChunkMask;
		if	(uTexChunkMask != 0)
		{
			if	(!USPInputDataAddTexStateData(psInputData, 
											  psSample, 
											  psSample->uTextureIdx, 
											  uTexChunkMask, 
											  0, 
											  psShader,
											  psContext))
			{
				USP_DBGPRINT(("HandleUSPSampleTextureFormat: Error adding tex-state data for dependent sample to input-data\n"));
				return IMG_FALSE;
			}
		}
	}
	
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		ResetShader

 Purpose:	Initialises a shader prior to finalisation

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader to reset

 Outputs:	none

 Returns:	none
*****************************************************************************/
IMG_INTERNAL IMG_BOOL ResetShader(PUSP_CONTEXT	psContext,
								  PUSP_SHADER	psShader)
{
	PUSP_INSTBLOCK		psInstBlock;
	PUSP_RESULTREF		psResultRef;
	PUSP_PROGDESC		psProgDesc;
	PUSP_SAMPLE			psSample;
	PUSP_SAMPLEUNPACK	psSampleUnpack;

	/*
		Reset the input-data		
	*/
	if	(!USPInputDataReset(psShader->psInputData))
	{
		USP_DBGPRINT(("ResetShader: Error resetting input-data\n"));
		return IMG_FALSE;
	}

	/*
		Reset the instruction-lists for each instruction block in the shader
	*/
	for	(psInstBlock = psShader->psInstBlocks; 
		 psInstBlock; 
		 psInstBlock = psInstBlock->psNext)
	{
		USPInstBlockReset(psInstBlock);
	}

	/*
		Reset the result-references

		NB: Only result-refs generated for pre-compiled instructions will
			remain in the list, as those for USP-generated insts are
			removed when the InstBlocks are reset.
	*/
	for	(psResultRef = psShader->psResultRefs;
		 psResultRef;
		 psResultRef = psResultRef->psNext)
	{
		USPResultRefReset(psResultRef);
	}

	/*
		Reset the non-dependent samples
	*/
	for	(psSample = psShader->psNonDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{
		if	(!USPSampleReset(psSample, psContext))
		{
			USP_DBGPRINT(("ResetShader: Error resetting non-dependent sample\n"));
			return IMG_FALSE;
		}		
	}

	/*
		Reset the dependent samples.
	*/
	for	(psSample = psShader->psDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{
		if	(!USPSampleReset(psSample, psContext))
		{
			USP_DBGPRINT(("ResetShader: Error resetting dependent sample\n"));
			return IMG_FALSE;
		}
	}

	/*
		Reset the sample unpacks.
	*/
	for	(psSampleUnpack = psShader->psSampleUnpacks;
		 psSampleUnpack;
		 psSampleUnpack = psSampleUnpack->psNext)
	{
		if	(!USPSampleUnpackReset(psSampleUnpack, psContext))
		{
			USP_DBGPRINT(("ResetShader: Error resetting sample unpack\n"));
			return IMG_FALSE;
		}		
	}

	/*
		Reset the register register useage info
	*/
	psProgDesc = psShader->psProgDesc;

	psShader->uFinalResultRegNum	= psProgDesc->uDefaultPSResultRegNum;
	psShader->uFinalTempRegCount	= psProgDesc->uTempRegCount;
	psShader->uFinalPARegCount		= psProgDesc->uPARegCount;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		FinaliseResultLocations

 Purpose:	Finalise the location of shader results

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader being finalised

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FinaliseResultLocations(
												PUSP_CONTEXT	psContext,
												PUSP_SHADER		psShader)
{
	PUSP_PROGDESC	psProgDesc;
	PUSP_INPUT_DATA	psInputData;
	//PUSP_SAMPLE		psSample;
	IMG_UINT32		uOrgTempCount;
	IMG_UINT32		uOrgPACount;
	IMG_UINT32		uTempCount;
	IMG_UINT32		uPACount;
	IMG_UINT32		uInputPACount;
	//IMG_UINT32		uTempResultEnd;
	//IMG_UINT32		uPAResultEnd;
	USP_REGTYPE		eResultRegType;
	IMG_UINT32		uResultRegNum;
	USP_REGTYPE		eOrgResultRegType;
	IMG_UINT32		uOrgResultRegNum;
	IMG_UINT32		uDefaultPSResultRegCount;

	psProgDesc			= psShader->psProgDesc;
	psInputData			= psShader->psInputData;
	uOrgTempCount		= psProgDesc->uTempRegCount;
	uOrgPACount			= psProgDesc->uPARegCount;
	uTempCount			= uOrgTempCount;
	uPACount			= uOrgPACount;
	eOrgResultRegType	= psProgDesc->eDefaultPSResultRegType;
	uOrgResultRegNum	= psProgDesc->uDefaultPSResultRegNum;


	/*
		Calculate the overall number of PAs used for iterated and pre-sampled
		data
	*/
	if	(!USPInputDataCalcPARegUse(psInputData, psShader, &uInputPACount))
	{
		USP_DBGPRINT(("FinaliseSampleAndResultLocations: Error finalising input-data locations\n"));
		return IMG_FALSE;
	}
	if	(uInputPACount > uPACount)
	{
		uPACount = uInputPACount;
	}

	if (!(psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_OMASKFEEDBACK))
	{
		uDefaultPSResultRegCount = psProgDesc->uDefaultPSResultRegCount;
	}
	else
	{
		uDefaultPSResultRegCount = psProgDesc->uDefaultPSResultRegCount + 1;
	}
	
	/*
		Calculate the location of the shader results, allowing for registers used
		by iterated and pre-sampled data
	*/
	if(		
		(psProgDesc->uFlags & USP_PC_PROGDESC_FLAGS_RESULT_REFS_NOT_PATCHABLE_WITH_PAS) 
		&& 		
		(psContext->eOutputRegType == USP_OUTPUT_REGTYPE_PA)
	)
	{
		/* 
			If Shader Result References are not Patchable by PA Regs and Shader 
			Result References are required to patched with PA Regs. Then we will 
			set the final Shader Results to default Shader Results so that no 
			Result References in instructions gets patched and we will put moves 
			to actual Final Shader Result Regs in the end. 			
		*/
		eResultRegType = psProgDesc->eDefaultPSResultRegType;
		uResultRegNum = psProgDesc->uDefaultPSResultRegNum;
		if(uPACount < uDefaultPSResultRegCount)
		{
			uPACount = uDefaultPSResultRegCount;
		}
	}
	else
	{
		switch (psContext->eOutputRegType)
		{
			IMG_UINT32	uResultRegStart;
			IMG_UINT32	uResultRegEnd;

			case USP_OUTPUT_REGTYPE_TEMP:
			{
				if	(eOrgResultRegType != USP_REGTYPE_TEMP)
				{
					uResultRegStart = psProgDesc->uPSResultFirstTempReg;
				}
				else
				{
					uResultRegStart = uOrgResultRegNum;
				}
#if 0
				if	(uResultRegStart == uOrgTempCount)
				{
					uResultRegStart = uTempCount;
				}
#endif
				uResultRegEnd = uResultRegStart + 
								uDefaultPSResultRegCount;

				if	(uResultRegEnd > uTempCount)
				{
					uTempCount = uResultRegEnd;
				}
				eResultRegType	= USP_REGTYPE_TEMP;
				uResultRegNum	= uResultRegStart;
				break;
			}

			case USP_OUTPUT_REGTYPE_PA:
			{
				if	(eOrgResultRegType != USP_REGTYPE_PA)
				{
					uResultRegStart = psProgDesc->uPSResultFirstPAReg;
				}
				else
				{
					uResultRegStart = uOrgResultRegNum;
				}

#if 0
				if	(uResultRegStart == uOrgPACount)
				{
					uResultRegStart = uPACount;
				}
#endif

				uResultRegEnd = uResultRegStart + 
								uDefaultPSResultRegCount;

				if	(uResultRegEnd > uPACount)
				{
					uPACount = uResultRegEnd;
				}

				eResultRegType	= USP_REGTYPE_PA;
				uResultRegNum	= uResultRegStart;
				break;
			}

			case USP_OUTPUT_REGTYPE_OUTPUT:
			{
				eResultRegType = USP_REGTYPE_OUTPUT;
				if	(eOrgResultRegType != USP_REGTYPE_OUTPUT)
				{
					uResultRegNum = psProgDesc->uPSResultFirstOutputReg;
				}
				else
				{
					uResultRegNum = uOrgResultRegNum;
				}
				break;
			}

			case USP_OUTPUT_REGTYPE_DEFAULT:
			{
				eResultRegType = psProgDesc->eDefaultPSResultRegType;
				uResultRegNum = psProgDesc->uDefaultPSResultRegNum;
				break;
			}

			default:
			{
				USP_DBGPRINT(("FinaliseResultLocations: Unsupposrted result location\n"));
				return IMG_FALSE;
			}
		}
	}

#if 0
	/*
		Calculate the location for intermediate data used by samples, accounting
		for registers used for inputs and results

		NB: At this point we don't know how many registers will be required
			for samples, so these are not included
	*/
	uTempResultEnd	= uTempCount;
	uPAResultEnd	= uPACount;

	for	(psSample = psShader->psDepSamples;
		 psSample; 
		 psSample = psSample->psNext)
	{
		IMG_UINT32	uBaseSampleRegStart;

		switch (psSample->eBaseSampleRegType)
		{
			case USP_REGTYPE_TEMP:
			{
				uBaseSampleRegStart = psSample->uBaseSampleRegNum;
				if	(uBaseSampleRegStart == uOrgTempCount)
				{
					uBaseSampleRegStart = uTempResultEnd;
				}
				break;
			}

			case USP_REGTYPE_PA:
			{
				uBaseSampleRegStart = psSample->uBaseSampleRegNum;
				if	(uBaseSampleRegStart == uOrgPACount)
				{
					uBaseSampleRegStart = uPAResultEnd;
				}
				break;
			}

			default:
			{
				USP_DBGPRINT(("FinaliseSampleAndResultLocations: Unsupposrted temp sample location\n"));
				return IMG_FALSE;
			}
		}

		psSample->uBaseSampleRegNum = uBaseSampleRegStart;
	}

	for	(psSample = psShader->psNonDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{
		IMG_UINT32	uBaseSampleRegStart;

		switch (psSample->eBaseSampleRegType)
		{
			case USP_REGTYPE_TEMP:
			{
				uBaseSampleRegStart = psSample->uBaseSampleRegNum;
				if	(uBaseSampleRegStart == uOrgTempCount)
				{
					uBaseSampleRegStart = uTempResultEnd;
				}
				break;
			}

			case USP_REGTYPE_PA:
			{
				uBaseSampleRegStart = psSample->uBaseSampleRegNum;
				if	(uBaseSampleRegStart == uOrgPACount)
				{
					uBaseSampleRegStart = uPAResultEnd;
				}
				break;
			}

			default:
			{
				USP_DBGPRINT(("FinaliseSampleAndResultLocations: Unsupposrted temp sample location\n"));
				return IMG_FALSE;
			}
		}

		psSample->uBaseSampleRegNum = uBaseSampleRegStart;
	}
#endif

	psShader->eFinalResultRegType	= eResultRegType;
	psShader->uFinalResultRegNum	= uResultRegNum;
	psShader->uFinalTempRegCount	= uTempCount;
	psShader->uFinalPARegCount		= uPACount;


	return IMG_TRUE;
}

/*****************************************************************************
 Name:		FinaliseRegCounts

 Purpose:	Finalise the number of temporary and PA registers used, accounting
			for those used by samples

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader being finalised

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FinaliseRegCounts(PUSP_CONTEXT	psContext,
										PUSP_SHADER		psShader)
{
	PUSP_SAMPLE	psSample;
	IMG_UINT32	uTempCount;
	IMG_UINT32	uPACount;
	PUSP_TEXTURE_WRITE psTextureWrite;

	#if !defined(DEBUG)
	PVR_UNREFERENCED_PARAMETER(psContext);
	#endif

	/*
		Update the overall temp and PA register counts to account for the number
		of registers used by samples
	*/
	uTempCount	= 0;
	uPACount	= 0;

	for	(psSample = psShader->psDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{
		if (psSample->uSampleTempRegCount > 0)
		{
			IMG_UINT32	uSampleTempRegEnd;
			uSampleTempRegEnd = psSample->uSampleTempRegNum + 
				psSample->uSampleTempRegCount;

			switch (psSample->eSampleTempRegType)
			{
				case USP_REGTYPE_TEMP:
				{
					if	(uSampleTempRegEnd > uTempCount)
					{
						uTempCount = uSampleTempRegEnd;
					}
					break;
				}

				case USP_REGTYPE_PA:
				{
					if	(uSampleTempRegEnd > uPACount)
					{
						uPACount = uSampleTempRegEnd;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(("FinaliseRegCounts: Unsupposrted temp sample location\n"));
					return IMG_FALSE;
				}
			}
		}
		if (psSample->uBaseSampleDestRegCount_ > 0)			
		{
			IMG_UINT32	uBaseSampleDestRegEnd;
			uBaseSampleDestRegEnd = psSample->uBaseSampleDestRegNum + 
				psSample->uBaseSampleDestRegCount_;

			switch (psSample->eBaseSampleDestRegType)
			{
				case USP_REGTYPE_TEMP:
				{
					if	(uBaseSampleDestRegEnd > uTempCount)
					{
						uTempCount = uBaseSampleDestRegEnd;
					}
					break;
				}

				case USP_REGTYPE_PA:
				{
					if	(uBaseSampleDestRegEnd > uPACount)
					{
						uPACount = uBaseSampleDestRegEnd;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(("FinaliseRegCounts: Unsupposrted temp base sample dest location\n"));
					return IMG_FALSE;
				}
			}

			uBaseSampleDestRegEnd = psSample->psSampleUnpack->uBaseSampleUnpackSrcRegNum + 
				psSample->uBaseSampleDestRegCount_;

			switch (psSample->psSampleUnpack->eBaseSampleUnpackSrcRegType)
			{
				case USP_REGTYPE_TEMP:
				{
					if	(uBaseSampleDestRegEnd > uTempCount)
					{
						uTempCount = uBaseSampleDestRegEnd;
					}
					break;
				}

				case USP_REGTYPE_PA:
				{
					if	(uBaseSampleDestRegEnd > uPACount)
					{
						uPACount = uBaseSampleDestRegEnd;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(("FinaliseRegCounts: Unsupposrted temp base sample unpack src location\n"));
					return IMG_FALSE;
				}
			}
		}
		if (psSample->psSampleUnpack->uSampleUnpackTempRegCount > 0)
		{
			IMG_UINT32	uSampleUnpackTempRegEnd;
			uSampleUnpackTempRegEnd = psSample->psSampleUnpack->uSampleUnpackTempRegNum +
								psSample->psSampleUnpack->uSampleUnpackTempRegCount;

			switch (psSample->psSampleUnpack->eSampleUnpackTempRegType)
			{
				case USP_REGTYPE_TEMP:
				{
					if	(uSampleUnpackTempRegEnd > uTempCount)
					{
						uTempCount = uSampleUnpackTempRegEnd;
					}
					break;
				}

				case USP_REGTYPE_PA:
				{
					if	(uSampleUnpackTempRegEnd > uPACount)
					{
						uPACount = uSampleUnpackTempRegEnd;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(("FinaliseRegCounts: Unsupposrted temp sample unpacklocation\n"));
					return IMG_FALSE;
				}
			}
		}		
	}

	for	(psSample = psShader->psNonDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{	
		if (psSample->uSampleTempRegCount > 0)
		{
			IMG_UINT32	uSampleTempRegEnd;
			uSampleTempRegEnd = psSample->uSampleTempRegNum +
								psSample->uSampleTempRegCount;

			switch (psSample->eSampleTempRegType)
			{
				case USP_REGTYPE_TEMP:
				{
					if	(uSampleTempRegEnd > uTempCount)
					{
						uTempCount = uSampleTempRegEnd;
					}
					break;
				}

				case USP_REGTYPE_PA:
				{
					if	(uSampleTempRegEnd > uPACount)
					{
						uPACount = uSampleTempRegEnd;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(("FinaliseRegCounts: Unsupposrted temp sample location\n"));
					return IMG_FALSE;
				}
			}
		}	
		if (psSample->uBaseSampleDestRegCount_ > 0)
		{
			IMG_UINT32	uBaseSampleDestRegEnd;
			uBaseSampleDestRegEnd = psSample->uBaseSampleDestRegNum + 
				psSample->uBaseSampleDestRegCount_;

			switch (psSample->eBaseSampleDestRegType)
			{
				case USP_REGTYPE_TEMP:
				{
					if	(uBaseSampleDestRegEnd > uTempCount)
					{
						uTempCount = uBaseSampleDestRegEnd;
					}
					break;
				}

				case USP_REGTYPE_PA:
				{
					if	(uBaseSampleDestRegEnd > uPACount)
					{
						uPACount = uBaseSampleDestRegEnd;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(("FinaliseRegCounts: Unsupposrted temp base sample dest location\n"));
					return IMG_FALSE;
				}
			}
			
			uBaseSampleDestRegEnd = psSample->psSampleUnpack->uBaseSampleUnpackSrcRegNum + 
				psSample->uBaseSampleDestRegCount_;

			switch (psSample->psSampleUnpack->eBaseSampleUnpackSrcRegType)
			{
				case USP_REGTYPE_TEMP:
				{
					if	(uBaseSampleDestRegEnd > uTempCount)
					{
						uTempCount = uBaseSampleDestRegEnd;
					}
					break;
				}

				case USP_REGTYPE_PA:
				{
					if	(uBaseSampleDestRegEnd > uPACount)
					{
						uPACount = uBaseSampleDestRegEnd;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(("FinaliseRegCounts: Unsupposrted temp base sample unpack src location\n"));
					return IMG_FALSE;
				}
			}
		}	
		if (psSample->psSampleUnpack->uSampleUnpackTempRegCount > 0)
		{
			IMG_UINT32	uSampleUnpackTempRegEnd;
			uSampleUnpackTempRegEnd = psSample->psSampleUnpack->uSampleUnpackTempRegNum +
								psSample->psSampleUnpack->uSampleUnpackTempRegCount;

			switch (psSample->psSampleUnpack->eSampleUnpackTempRegType)
			{
				case USP_REGTYPE_TEMP:
				{
					if	(uSampleUnpackTempRegEnd > uTempCount)
					{
						uTempCount = uSampleUnpackTempRegEnd;
					}
					break;
				}

				case USP_REGTYPE_PA:
				{
					if	(uSampleUnpackTempRegEnd > uPACount)
					{
						uPACount = uSampleUnpackTempRegEnd;
					}
					break;
				}

				default:
				{
					USP_DBGPRINT(("FinaliseRegCounts: Unsupposrted temp sample unpacklocation\n"));
					return IMG_FALSE;
				}
			}
		}		
	}

	/*
	   	Update the temp and PA counts for the texture write code
	*/
	for	(psTextureWrite = psShader->psTextureWrites;
		 psTextureWrite;
		 psTextureWrite = psTextureWrite->psNext)
	{	
		if (psTextureWrite->uTempRegCount > 0)
		{
			IMG_UINT32	uTempRegEnd;
			uTempRegEnd = psTextureWrite->uTempRegStartNum +
							psTextureWrite->uTempRegCount;

			if	(uTempRegEnd > uTempCount)
			{
				uTempCount = uTempRegEnd;
			}
		}	
	}

	/*
		Update the overall temp/pa register counts
	*/
	if	(uTempCount > psShader->uFinalTempRegCount)
	{
		psShader->uFinalTempRegCount = uTempCount;
	}
	if	(uPACount > psShader->uFinalPARegCount)
	{
		psShader->uFinalPARegCount = uPACount;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		FinaliseSamples

 Purpose:	Generate code for dependent and non-dependent texture-samples

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader being finalised

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FinaliseSamples(PUSP_CONTEXT	psContext,
									  PUSP_SHADER	psShader)
{
	PUSP_SAMPLE			psSample;
	PUSP_SAMPLEUNPACK	psSampleUnpack;

	/*
		Generate code for the non-dependent samples in the shader
	*/
	for	(psSample = psShader->psNonDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{
		if	(!USPSampleGenerateCode(psSample, psShader, psContext))
		{
			USP_DBGPRINT(("FinaliseSamples: Error generating code for non-dependent sample\n"));
			return IMG_FALSE;
		}
	}

	/*
		Generate code for the dependent samples in the shader
	*/
	for	(psSample = psShader->psDepSamples;
		 psSample;
		 psSample = psSample->psNext)
	{
		if	(!USPSampleGenerateCode(psSample, psShader, psContext))
		{
			USP_DBGPRINT(("FinaliseSamples: Error generating code for dependent sample\n"));
			return IMG_FALSE;
		}
	}

	/*
		Generate code for the sample unpacks in the shader
	*/
	for	(psSampleUnpack = psShader->psSampleUnpacks;
		 psSampleUnpack;
		 psSampleUnpack = psSampleUnpack->psNext)
	{
		if	(!USPSampleUnpackGenerateCode(psSampleUnpack, psShader, psContext))
		{
			USP_DBGPRINT(("FinaliseSamples: Error generating code for sample unpack\n"));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		RemoveDummyIterations

 Purpose:	Remove any excess dummy iterations that may have been added by
			the USC to prevent the USP overwriting PS outputs when they are
			located in PA registers. 

			These are added by the USC to force any additional iterations or
			pre-sampled data to reside in PA registers that do not clash with
			the location it has chosed for the shader results when they are
			in PA registers. Provided such iterations are the last in the list
			(i.e. the USP did not add any more iterations or non-dependent
			samples), they can safely be removed.

 Inputs:	psShader	- The USP shader being finalised

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL RemoveDummyIterations(PUSP_SHADER	psShader)
{
	PUSP_INPUT_DATA			psInputData;
	PUSP_ITERATED_DATA		psIteratedData;
	IMG_UINT32				uIteratedDataCount;
	PUSP_PRESAMPLED_DATA	psPreSampledData;
	IMG_UINT32				uPreSampledDataCount;
	IMG_UINT32				uTrailingPaddingCount;

	/*
		Ensure that we added no additional iterations or pre-sampled data
	*/
	psInputData = psShader->psInputData;
	uIteratedDataCount = psInputData->uIteratedDataCount;
	uPreSampledDataCount = psInputData->uPreSampledDataCount;

	if	(
			(uIteratedDataCount == 0) ||
			(uIteratedDataCount != psInputData->uOrgIteratedDataCount) ||
			(uPreSampledDataCount != psInputData->uOrgPreSampledDataCount)
		)
	{
		return IMG_TRUE;
	}

	/*
		Ensure that dummy iterations are the only trailing entries
		in the list of PS input data (in terms of register index). 
		If not, removing them would alter the PA registers where
		following data will be placed.
	*/
	psPreSampledData	  = psInputData->psPreSampledData + uPreSampledDataCount - 1;
	psIteratedData		  = psInputData->psIteratedData + uIteratedDataCount - 1;
	uTrailingPaddingCount = 0;

	do 
	{
		if	(
				(psIteratedData->eType != USP_ITERATED_DATA_TYPE_PADDING) ||
				(
					(uPreSampledDataCount > 0) && 
					(psIteratedData->uRegNum < psPreSampledData->uRegNum)
				)
			)
		{
			break;
		}

		uTrailingPaddingCount++;
		psIteratedData--;
	} while (psIteratedData > psInputData->psIteratedData);

	if	(uTrailingPaddingCount == 0)
	{
		return IMG_TRUE;
	}

	/*
		Remove the trailing iterations
	*/
	psInputData->uIteratedDataCount -= uTrailingPaddingCount;

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		FinaliseResults

 Purpose:	Finalise the PS result locations

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader being finalised

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FinaliseResults(PUSP_CONTEXT	psContext,
									  PUSP_SHADER	psShader)
{
	PUSP_RESULTREF	psResultRef;
	PUSP_PROGDESC	psProgDesc;
	PUSP_INSTBLOCK	psLastBlock;
	USP_REGTYPE		eOrgResultRegType;
	IMG_UINT32		uOrgResultRegNum;
	USP_REGFMT		eOrgResultRegFmt;
	USP_REGTYPE		eNewResultRegType;
	IMG_UINT32		uNewResultRegNum;
	IMG_UINT32		uResultRegCount;
	IMG_BOOL		bNoInsts;
	IMG_BOOL		bUseMOVAtEnd;
	IMG_BOOL		bRemapResults;

	/*
		Currently, don't try to modify vertex-shader and geometry-shader results
	*/
	psProgDesc = psShader->psProgDesc;
	if	((psProgDesc->uShaderType == USP_PC_SHADERTYPE_VERTEX) || (psProgDesc->uShaderType == USP_PC_SHADERTYPE_GEOMETRY))
	{
		return IMG_TRUE;	
	}

	/*
		Lookup the default location of the shader results
	*/
	eOrgResultRegType	= psProgDesc->eDefaultPSResultRegType;
	uOrgResultRegNum	= psProgDesc->uDefaultPSResultRegNum;
	eOrgResultRegFmt	= psProgDesc->eDefaultPSResultRegFmt;
	if (!(psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_OMASKFEEDBACK))
	{
		uResultRegCount		= psProgDesc->uDefaultPSResultRegCount;
	}
	else
	{
		uResultRegCount		= psProgDesc->uDefaultPSResultRegCount + 1;
	}

	/*
		Get the new register type and final index for the shader results
	*/
	eNewResultRegType	= psShader->eFinalResultRegType;
	uNewResultRegNum	= psShader->uFinalResultRegNum;
	bRemapResults		= IMG_TRUE;

	/*
		Don't remap if the new location is the same as the default
	*/
	if	(eNewResultRegType == eOrgResultRegType)
	{
		bRemapResults = IMG_FALSE;
	}

	/*
		Check that all result-references can use the requested register-type

		NB: Not required if we aren't relocating the results
	*/
	bUseMOVAtEnd = IMG_FALSE;

	if((psProgDesc->uFlags & USP_PC_PROGDESC_FLAGS_RESULT_REFS_NOT_PATCHABLE) && (bRemapResults == IMG_TRUE))
	{
		bUseMOVAtEnd = IMG_TRUE;
	}
	else if(
		(psProgDesc->uFlags & USP_PC_PROGDESC_FLAGS_RESULT_REFS_NOT_PATCHABLE_WITH_PAS) 
		&& 
		(psContext->eOutputRegType == USP_OUTPUT_REGTYPE_PA)
	)
	{
		psShader->eFinalResultRegType	= USP_REGTYPE_PA;
		psShader->uFinalResultRegNum	= 0;
		eNewResultRegType				= psShader->eFinalResultRegType;
		uNewResultRegNum				= psShader->uFinalResultRegNum;
		bUseMOVAtEnd					= IMG_TRUE;
		bRemapResults					= IMG_TRUE;
	}

	if	(bRemapResults && (bUseMOVAtEnd == IMG_FALSE))
	{
		for	(psResultRef = psShader->psResultRefs;
			 psResultRef;
			 psResultRef = psResultRef->psNext)
		{
			if	(!USPResultRefSupportsRegLocation(psResultRef,
												  eNewResultRegType,
												  uNewResultRegNum))
			{
				bUseMOVAtEnd = IMG_TRUE;
				break;
			}
		}
	}

	/*
		If the shader contains no code or no result-references, then we'll
		definitely need to add a MOV to relocate the results

		MB: If ther shader contains no result-refs, then it must be invalid
			to remap any of the result-registers.
	*/
	bNoInsts = IMG_TRUE;

	for	(psLastBlock = psShader->psInstBlocks;
		 psLastBlock != psShader->psInstBlocksEnd; 
		 psLastBlock = psLastBlock->psNext)
	{
		if	(psLastBlock->uInstCount > 0)
		{
			bNoInsts = IMG_FALSE;
			break;
		}
	}
	psLastBlock = psShader->psInstBlocksEnd;

	if	(bRemapResults)
	{
		bUseMOVAtEnd |= bNoInsts;

		if	(!psShader->psResultRefs)
		{
			bUseMOVAtEnd |= IMG_TRUE;	
		}
	}

	/*
		Either add a MOV to remap the result-registers, or remap each
		reference within the code.
	*/
	if	(bUseMOVAtEnd)
	{
		HW_INST		sHWInst;
		IMG_BOOL	bAddSMLSI;
		IMG_BOOL	bAddSMBO;
		USP_REG		sDestReg;
		USP_REG		sSrc1Reg;

		/*
			Determine whether we need to append an SMLSI before the MOV
		*/
		if	(bNoInsts)
		{
			/*
				No instructions so we assumne the default MOE state (increment
				mode for all operands, base-offset == 0 for all operands). Thus,
				no MOE changes needed.
			*/
			bAddSMLSI	= IMG_FALSE;
			bAddSMBO	= IMG_FALSE;
		}
		else
		{
			PUSP_INSTBLOCK	psLastNonEmptyBlock;
			PUSP_MOESTATE	psFinalMOEState;
			IMG_UINT32		i;

			/*
				Find the final MOE state within the program
			*/
			psLastNonEmptyBlock = psLastBlock;
			while (psLastNonEmptyBlock->uInstCount == 0)
			{
				psLastNonEmptyBlock = psLastNonEmptyBlock->psPrev;
			}
			psFinalMOEState = &psLastNonEmptyBlock->sFinalMOEState;

			/*
				Check whether the final MOE state is OK for the MOV we
				need to append to the program
			*/
			bAddSMBO	= IMG_FALSE;
			bAddSMLSI	= IMG_FALSE;

			for	(i = 0; i < USP_MOESTATE_OPERAND_COUNT; i++)
			{
				if	(psFinalMOEState->auBaseOffset[i] != 0)
				{
					bAddSMBO = IMG_TRUE;
					break;
				}
			}

			for	(i = 0; i < USP_MOESTATE_OPERAND_COUNT; i++)
			{
				if	(psFinalMOEState->abUseSwiz[i] && (psFinalMOEState->auSwiz[i] != 0))
				{
					bAddSMLSI = IMG_TRUE;
					break;
				}
			}
		}

		/*
			Add an SMBO instruction to reset the base-offset state
		*/
		if	(bAddSMBO)
		{
			static const IMG_UINT32	auDefaultBaseOffsets[USP_MOESTATE_OPERAND_COUNT] =
			{
				0, 0, 0, 0
			};

			/*
				Encode an SMBO instruction to set the base-ofsets to zero for all
				operands
			*/
			if	(!HWInstEncodeSMBOInst(&sHWInst, (IMG_PUINT32)auDefaultBaseOffsets))
			{
				USP_DBGPRINT(("FinaliseResults: Error encoding SMBO inst\n"));
				return IMG_FALSE;
			}

			/*
				Append the SMBO at the end of the block of code for this sample
			*/
			if	(!USPInstBlockInsertHWInst(psLastBlock,
										   IMG_NULL,
										   USP_OPCODE_SMBO,
										   &sHWInst,
										   0,
										   psContext,
										   IMG_NULL))
			{
				USP_DBGPRINT(("FinaliseResults: Error addding inst to block\n"));
				return IMG_FALSE;
			}
		}

		/*
			Append instrucitons to reset the MOE state prior to the MOV if
			needed
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

			/*
				Encode a HW SMLSI instruciton to reset the MOE state to
				use increments of 1 for all operands
			*/
			if	(!HWInstEncodeSMLSIInst(&sHWInst,
										(IMG_PBOOL)abUseSwiz,
										(IMG_PINT32)aiIncrements,
										IMG_NULL,
										(IMG_PUINT32)auLimits))
			{
				USP_DBGPRINT(("FinaliseResults: Error encoding SMLSI to reset MOE state\n"));
				return IMG_FALSE;
			}

			/*
				Insert the SMLSI at the end of the program
			*/
			if	(!USPInstBlockInsertHWInst(psLastBlock,
										   IMG_NULL,
										   USP_OPCODE_SMLSI,
										   &sHWInst,
										   0,
										   psContext,
										   IMG_NULL))
			{
				USP_DBGPRINT(("FinaliseResults: Error adding SMLSI to last block\n"));
				return IMG_FALSE;
			}
		}

		/*
			Setup a MOV instruction to copy the shader results to the
			requested register-bank
		*/
		sDestReg.eType		= eNewResultRegType;
		sDestReg.uNum		= uNewResultRegNum;
		sDestReg.eFmt		= eOrgResultRegFmt;
		sDestReg.uComp		= 0;
		sDestReg.eDynIdx	= USP_DYNIDX_NONE;

		sSrc1Reg.eType		= eOrgResultRegType;
		sSrc1Reg.uNum		= uOrgResultRegNum;
		sSrc1Reg.eFmt		= eOrgResultRegFmt;
		sSrc1Reg.uComp		= 0;
		sSrc1Reg.eDynIdx	= USP_DYNIDX_NONE;

		#if defined(SGX_FEATURE_USE_VEC34)
		{
			USP_REG sImdVal;
			sImdVal.eType	= USP_REGTYPE_IMM;
			sImdVal.uNum	= 0xFFFFFFFF;
			sImdVal.eFmt	= sSrc1Reg.eFmt;
			sImdVal.uComp	= sSrc1Reg.uComp;
			sImdVal.eDynIdx = USP_DYNIDX_NONE;
			if	(!HWInstEncodeANDInst(&sHWInst, 
									  uResultRegCount,
									  IMG_FALSE, /* bUsePred */
									  IMG_FALSE, /* bNegatePred */
									  0, /* uPredNum */
									  IMG_TRUE,
									  &sDestReg, 
									  &sSrc1Reg, 
									  &sImdVal))
			{
				USP_DBGPRINT(("FinaliseResults: Error encoding final data movement (AND)\n"));
				return IMG_FALSE;
			}
			/*
				Insert data movement (using AND) instruction at the end of the program
			*/
			if	(!USPInstBlockInsertHWInst(psLastBlock,
										   IMG_NULL,
										   USP_OPCODE_AND,
										   &sHWInst,
										   USP_INSTDESC_FLAG_DEST_USES_RESULT,
										   psContext, 
										   IMG_NULL))
			{
				USP_DBGPRINT(("FinaliseResults: Error adding final data movement (AND) to last block\n"));
				return IMG_FALSE;
			}
		}
		#else
		if	(!HWInstEncodeMOVInst(&sHWInst,
								  uResultRegCount,
								  IMG_TRUE,
								  &sDestReg,
								  &sSrc1Reg))
		{
			USP_DBGPRINT(("FinaliseResults: Error encoding final MOV\n"));
			return IMG_FALSE;
		}

		/*
			Insert the MOV at the end of the program
		*/
		if	(!USPInstBlockInsertHWInst(psLastBlock,
									   IMG_NULL,
									   USP_OPCODE_MOVC,
									   &sHWInst,
									   USP_INSTDESC_FLAG_DEST_USES_RESULT,
									   psContext, 
									   IMG_NULL))
		{
			USP_DBGPRINT(("FinaliseResults: Error adding final MOV to last block\n"));
			return IMG_FALSE;
		}
		#endif /* defined(SGX_FEATURE_USE_VEC34) */
	}
	else
	{
		/*
			Traverse the list of references to the shader results, changing
			the result-reg location if needed, or just fixing-up write-masks
			for final writes otherwise.
		*/
		for	(psResultRef = psShader->psResultRefs;
			 psResultRef;
			 psResultRef = psResultRef->psNext)
		{
			if	(bRemapResults)				
			{
				if	(!USPResultRefChangeRegLocation(psContext,
													psResultRef,
													eNewResultRegType,
													uNewResultRegNum))
				{
					USP_DBGPRINT(("FinaliseResults: Error changing result-ref reg-location\n"));
					return IMG_FALSE;
				}
			}
		}
	}

	/*
		Use the original result location if we couldn't relocate
	*/
	if	(!bRemapResults)
	{
		psShader->eFinalResultRegType	= eOrgResultRegType;
		psShader->uFinalResultRegNum	= uOrgResultRegNum;
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		FinaliseShaderPreamble

 Purpose:	Insert additional instructions at the start of the shader if
			required

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader being finalised

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FinaliseShaderPreamble(PUSP_CONTEXT	psContext,
											 PUSP_SHADER	psShader)
{
	PUSP_INSTBLOCK	psPreambleBlock;
	HW_INST			sPreambleInst;
	IMG_UINT32		i;

	if	(psContext->uPreambleInstCount == 0)
	{
		return IMG_TRUE;
	}

	/*
		Insert the required number of empty instructions at the start of the shader
	*/
	sPreambleInst.uWord0 = sPreambleInst.uWord1 = 0;

	psPreambleBlock = psShader->psPreableBlock;

	for	(i = 0; i < psContext->uPreambleInstCount; i++)
	{
		if	(!USPInstBlockInsertHWInst(psPreambleBlock,
									   IMG_NULL,
									   USP_OPCODE_PREAMBLE,
									   &sPreambleInst,
									   0,
									   psContext,
									   IMG_NULL))
		{
			USP_DBGPRINT(("FinaliseShaderPreamble: Error adding preable NOP\n"));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		FinaliseInstructions

 Purpose:	Finalise the instructions for all instruction blocks

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader being finalised

 Outputs:	none

 Returns:	IMG_FALSE on error
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FinaliseInstructions(PUSP_CONTEXT	psContext,
										   PUSP_SHADER	psShader)
{
	PUSP_INSTBLOCK	psInstBlock;

	USP_UNREFERENCED_PARAMETER(psContext);

	/*
		Setup the NoSched flag within all instruction blocks, recording the
		index of the first instruction within each as we go.
	*/
	for	(psInstBlock = psShader->psInstBlocks; 
		 psInstBlock;
		 psInstBlock = psInstBlock->psNext)
	{
		if	(!USPInstBlockFinaliseInsts(psInstBlock, psContext))
		{
			USP_DBGPRINT(("FinaliseInstructions: Failed to finalise insts for block\n"));
			return IMG_FALSE;
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		FinaliseTextureWriteList

 Purpose:	Finalise list of texture writes within a USP shader

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader being finalised

 Outputs:	none

 Returns:	IMG_TRUE/FALSE on success failure.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FinaliseTextureWriteList(PUSP_CONTEXT psContext,
											   PUSP_SHADER  psShader)
{
	PUSP_TEXTURE_WRITE	psTextureWrite;

	for	(psTextureWrite = psShader->psTextureWrites;
		 psTextureWrite;
		 psTextureWrite = psTextureWrite->psNext)
	{
		if	(!USPTextureWriteGenerateCode(psTextureWrite, psShader, psContext))
		{
			USP_DBGPRINT(("FinaliseTextureWriteList: Error generating code for texture write\n"));
			return IMG_FALSE;
		}
	}
	
	return IMG_TRUE;
}

/*****************************************************************************
 Name:		FinaliseBranches

 Purpose:	Finalise branch destinations within a USP shader

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader being finalised

 Outputs:	none

 Returns:	IMG_TRUE/FALSE on success failure.
*****************************************************************************/
IMG_INTERNAL IMG_BOOL FinaliseBranches(PUSP_CONTEXT	psContext,
									   PUSP_SHADER	psShader)
{
	PUSP_BRANCH	psBranch;

	USP_UNREFERENCED_PARAMETER(psContext);

	/*
		Calculate the jump offset for each branch and fixup the associated
		HW instruction
	*/
	for	(psBranch = psShader->psBranches;
		 psBranch;
		 psBranch = psBranch->psNext)
	{
		IMG_INT32		iInstOffset;
		PUSP_INSTBLOCK	psBranchInstBlock;
		PUSP_INSTBLOCK	psLabelInstBlock;
		PUSP_INST		psBranchInst;

		/*
			Calculate the relative instruction offset from the branch
			instruction to the instruction referenced by the the label.
		*/
		if	(psBranch->psLabel)
		{
			psBranchInstBlock	= psBranch->psInstBlock;
			psLabelInstBlock	= psBranch->psLabel->psInstBlock;
			psBranchInst		= psBranchInstBlock->psFirstInst;

			iInstOffset = (IMG_INT32)(psLabelInstBlock->uFirstInstIdx - 
									  psBranchInstBlock->uFirstInstIdx);

			/*
				Update the branch instruction
			*/
			if	(!HWInstSetBranchOffset(&psBranchInst->sHWInst, iInstOffset))
			{
				/*
					If the offset is too large, make the branch absolute.
				*/
				psBranchInst->sDesc.eOpcode = USP_OPCODE_BA;
				psShader->psProgDesc->uHWFlags |= UNIFLEX_HW_FLAGS_ABS_BRANCHES_USED;

				if	(!HWInstSetBranchAddr(&psBranchInst->sHWInst, psLabelInstBlock->uFirstInstIdx))
				{
					USP_DBGPRINT(( "FinaliseBranchDestinations: Error setting branch address %d\n", psLabelInstBlock->uFirstInstIdx));
					return IMG_FALSE;
				}
			}
		}
	}

	return IMG_TRUE;
}

#if defined(DEBUG) && defined(USP_DECODE_HWSHADER)
/*****************************************************************************
 Name:		DecodeUSECode

 Purpose:	Disassemble a number of USE instructions

 Inputs:	psContext	- The current USP execution context
			psTarget	- The target device
			uInstCount	- The number of USE instructions to decode
			puInsts		- The USE instructions to decode

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID DecodeUSECode(PUSP_CONTEXT		psContext,
							  PSGX_CORE_INFO	psTarget,
							  IMG_UINT32		uInstCount,
							  IMG_PUINT32		puInsts)
{
	IMG_CHAR		pszInst[256];
	IMG_UINT32		uInstIdxWidth, i;
	IMG_PUINT32		puInst;
	PCSGX_CORE_DESC	psTargetDesc = UseAsmGetCoreDesc(psTarget);

	/* Count the number of digits required for the instruction index */
	for (i = 10, uInstIdxWidth = 1; i < uInstCount; i *= 10, uInstIdxWidth++);
	
	/* Disassembvle and print each instruction */
	for (puInst = puInsts, i = 0; i < uInstCount; i++, puInst += 2)
	{
		IMG_UINT32	uInst0 = puInst[0];
		IMG_UINT32	uInst1 = puInst[1];

		USP_DBGPRINT(("%*d: 0x%.8X%.8X\t", (int)uInstIdxWidth, i, uInst1, uInst0));
		UseDisassembleInstruction(psTargetDesc, uInst0, uInst1, pszInst);
		USP_DBGPRINT(("%s\n", pszInst));
	}
}

/*****************************************************************************
 Name:		DecodeTextureControlWord

 Purpose:	Decodes texture control words for one plane

 Inputs:	puWord	- The set of texture control words for this plane
			puMask  - The set of masks to be used for this plane

 Outputs:	szOutString - The decoded texture control word string

 Returns:	none
*****************************************************************************/
static IMG_VOID DecodeTextureControlWord(IMG_UINT32	*puWord, 
										 IMG_UINT32	*puMask, 
										 IMG_CHAR	*szOutString)
{
	IMG_UINT32 uTexCtrWrd = puWord[1] & (~puMask[1]);
	#if defined(EURASIA_PDS_DOUTT0_TEXFEXT)
	IMG_BOOL bExtFmt = IMG_FALSE;
	#endif /* defined(EURASIA_PDS_DOUTT0_TEXFEXT) */

	#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
	IMG_BOOL bReplct = IMG_FALSE;
	#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

	#if defined(SGX_FEATURE_TAG_SWIZZLE)
	IMG_UINT32 uTexSwizzle;

	static char* aszTextureSwizzle1Channel[] = 
	{
		"R---",	/* EURASIA_PDS_DOUTT3_SWIZ_R	= 0	*/
		"R000",	/* EURASIA_PDS_DOUTT3_SWIZ_000R	= 1	*/
		"R111",	/* EURASIA_PDS_DOUTT3_SWIZ_111R	= 2	*/
		"RRRR",	/* EURASIA_PDS_DOUTT3_SWIZ_RRRR	= 3	*/
		"RRR0",	/* EURASIA_PDS_DOUTT3_SWIZ_0RRR	= 4	*/
		"RRR1",	/* EURASIA_PDS_DOUTT3_SWIZ_1RRR	= 5	*/
		"000R",	/* EURASIA_PDS_DOUTT3_SWIZ_R000	= 6	*/
		"111R",	/* EURASIA_PDS_DOUTT3_SWIZ_R111	= 7	*/
		"----",	/* INVALID	= 8	*/
	};

	static char* aszTextureSwizzle2Channel[] = 
	{
		"RG--",	/* EURASIA_PDS_DOUTT3_SWIZ_GR	= 0	*/
		"RG00",	/* EURASIA_PDS_DOUTT3_SWIZ_00GR	= 1	*/
		"RRRG",	/* EURASIA_PDS_DOUTT3_SWIZ_GRRR	= 2	*/
		"GGGR",	/* EURASIA_PDS_DOUTT3_SWIZ_RGGG	= 3	*/
		"RGRG",	/* EURASIA_PDS_DOUTT3_SWIZ_GRGR	= 4	*/
		"GR--",	/* EURASIA_PDS_DOUTT3_SWIZ_RG	= 5	*/
		"----",	/* INVALID	= 6	*/
	};

	static char* aszTextureSwizzle3Channel[] = 
	{
		"RGB-",	/* EURASIA_PDS_DOUTT3_SWIZ_BGR	= 0	*/
		"BGR-",	/* EURASIA_PDS_DOUTT3_SWIZ_RGB	= 1	*/
		"----",	/* INVALID	= 2	*/
	};

	static char* aszTextureSwizzle4Channel[] = 
	{
		"RGBA",	/* EURASIA_PDS_DOUTT3_SWIZ_ABGR	= 0	*/
		"BGRA",	/* EURASIA_PDS_DOUTT3_SWIZ_ARGB	= 1	*/
		"ABGR",	/* EURASIA_PDS_DOUTT3_SWIZ_RGBA	= 2	*/
		"ARGB",	/* EURASIA_PDS_DOUTT3_SWIZ_BGRA	= 3	*/
		"RGB1",	/* EURASIA_PDS_DOUTT3_SWIZ_1BGR	= 4	*/
		"BGR1",	/* EURASIA_PDS_DOUTT3_SWIZ_1RGB	= 5	*/
		"1BGR",	/* EURASIA_PDS_DOUTT3_SWIZ_RGB1 = 6 */
		"1RGB",	/* EURASIA_PDS_DOUTT3_SWIZ_BGR1 = 7 */
		"----",	/* INVALID	= 8	*/
	};

	static char* aszTextureSwizzleYUV422[] = 
	{
		"YUYV_CSC0",	/* EURASIA_PDS_DOUTT3_SWIZ_YUYV_CSC0 = 0 */
		"YVYU_CSC0",	/* EURASIA_PDS_DOUTT3_SWIZ_YVYU_CSC0 = 1 */
		"UYVY_CSC0",	/* EURASIA_PDS_DOUTT3_SWIZ_UYVY_CSC0 = 2 */
		"VYUY_CSC0",	/* EURASIA_PDS_DOUTT3_SWIZ_VYUY_CSC0 = 3 */
		"YUYV_CSC1",	/* EURASIA_PDS_DOUTT3_SWIZ_YUYV_CSC1 = 4 */
		"YVYU_CSC1",	/* EURASIA_PDS_DOUTT3_SWIZ_YVYU_CSC1 = 5 */
		"UYVY_CSC1",	/* EURASIA_PDS_DOUTT3_SWIZ_UYVY_CSC1 = 6 */
		"VYUY_CSC1",	/* EURASIA_PDS_DOUTT3_SWIZ_VYUY_CSC1 = 7 */
		"----",	/* INVALID	= 8	*/
	};

	static char* aszTextureSwizzleYUV420[] = 
	{
		"YUV_CSC0",	/* EURASIA_PDS_DOUTT3_SWIZ_YUV_CSC0 = 0 */
		"YVU_CSC0",	/* EURASIA_PDS_DOUTT3_SWIZ_YVU_CSC0 = 1 */
		"YUV_CSC1",	/* EURASIA_PDS_DOUTT3_SWIZ_YUV_CSC1 = 2 */
		"YVU_CSC1",	/* EURASIA_PDS_DOUTT3_SWIZ_YVU_CSC1 = 3 */
		"----",	/* INVALID	= 4	*/
	};

	#endif /* defined(SGX_FEATURE_TAG_SWIZZLE */

	#if defined(SGX545)
	IMG_UINT32 uUnPackFmt = EURASIA_PDS_DOUTT3_FCONV_NONE;
	IMG_UINT32	uTagSize = 0;
	#endif /* defined(SGX545) */

	#if defined(EURASIA_PDS_DOUTT3_FCNORM)
	IMG_BOOL bIsNormaliseSet = IMG_FALSE;
	#endif /* defined(EURASIA_PDS_DOUTT3_FCNORM) */

	szOutString[0] = IMG_NULL;

	#if defined(EURASIA_PDS_DOUTT0_TEXFEXT)
	bExtFmt = ((puWord[0] & (~puMask[0])) == EURASIA_PDS_DOUTT0_TEXFEXT);
	#endif /* defined(EURASIA_PDS_DOUTT0_TEXFEXT) */

	#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
	bReplct = (((IMG_UINT32)(puWord[0] & (~puMask[0])) & EURASIA_PDS_DOUTT0_CHANREPLICATE) == EURASIA_PDS_DOUTT0_CHANREPLICATE);
	#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

	#if defined(SGX545)
	uUnPackFmt = (puWord[3] & (~puMask[3])) & (~EURASIA_PDS_DOUTT3_FCONV_CLRMSK);
	
	/*
		Get the tag size
	*/
	uTagSize = (puWord[3] & (~puMask[3])) & (~EURASIA_PDS_DOUTT3_TAGDATASIZE_CLRMSK);
	uTagSize = uTagSize >> EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT;
	#endif /* defined(SGX545) */

	#if defined(SGX_FEATURE_TAG_SWIZZLE)
	uTexSwizzle = (puWord[3] & (~puMask[3])) & (~EURASIA_PDS_DOUTT3_SWIZ_CLRMASK);
	uTexSwizzle = uTexSwizzle >> EURASIA_PDS_DOUTT3_SWIZ_SHIFT;
	#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

	#if defined(EURASIA_PDS_DOUTT3_FCNORM)
	bIsNormaliseSet = (((IMG_UINT32)(puWord[3] & (~puMask[3])) & EURASIA_PDS_DOUTT3_FCNORM) == EURASIA_PDS_DOUTT3_FCNORM);
	#endif /* defined(EURASIA_PDS_DOUTT3_FCNORM) */
	
	#if defined(SGX545)
	if(bExtFmt)
	{
		if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_A8)
		{
			strcpy(szOutString, "A8");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_L8)
		{
			strcpy(szOutString, "L8");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_L8A8)
		{
			strcpy(szOutString, "L8A8");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_A_F16)
		{
			strcpy(szOutString, "A_F16");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_L_F16_REP)
		{
			strcpy(szOutString, "L_F16_REP");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_L_F16_A_F16)
		{
			strcpy(szOutString, "L_F16_A_F16");
		}		
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_C0_YUV420_2P_VU)
		{
			strcpy(szOutString, "C0_YUV420_2P_VU");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_C0_YUV420_2P_UV)
		{
			strcpy(szOutString, "C0_YUV420_2P_UV");
		}
		else
		{
			strcpy(szOutString, "INVALID");
		}
	}
	else
	{
		if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4)
		{
			strcpy(szOutString, "A4R4G4B4");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5)
		{
			strcpy(szOutString, "A1R5G5B5");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_R5G6B5)
		{
			strcpy(szOutString, "R5G6B5");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8)
		{
			strcpy(szOutString, "B8G8R8A8");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8)
		{
			strcpy(szOutString, "R8G8B8A8");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_U8)
		{
			strcpy(szOutString, "U8");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_U88)
		{
			strcpy(szOutString, "U88");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_F32)
		{
			strcpy(szOutString, "F32");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616)
		{
			strcpy(szOutString, "F1616");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_F16)
		{
			strcpy(szOutString, "F16");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_U16)
		{
			strcpy(szOutString, "U16");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP)
		{
			strcpy(szOutString, "PVRT2BPP");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP)
		{
			strcpy(szOutString, "PVRT4BPP");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP)
		{
			strcpy(szOutString, "PVRTII2BPP");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP)
		{
			strcpy(szOutString, "PVRTII4BPP");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII)
		{
			strcpy(szOutString, "PVRTIII");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_C0_YUYV)
		{
			strcpy(szOutString, "C0_YUYV");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_C0_UYVY)
		{
			strcpy(szOutString, "C0_UYVY");
		}
		else
		{
			strcpy(szOutString, "INVALID");
		}

	}
	#endif /* defined(SGX545) */

	#if defined(SGX_FEATURE_TAG_SWIZZLE)
	if(bExtFmt)
	{
		
		if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP)
		{
			strcpy(szOutString, "PVRT2BPP");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP)
		{
			strcpy(szOutString, "PVRT4BPP");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP)
		{
			strcpy(szOutString, "PVRTII2BPP");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP)
		{
			strcpy(szOutString, "PVRTII4BPP");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII)
		{
			strcpy(szOutString, "PVRTIII");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_YUV422)
		{
			strcpy(szOutString, "YUV422");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_YUV420_2P)
		{
			strcpy(szOutString, "YUV420_2P");
		}
		else
		{
			strcpy(szOutString, "INVALID");
		}
	}
	else
	{
		if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4)
		{
			strcpy(szOutString, "A4R4G4B4");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5)
		{
			strcpy(szOutString, "A1R5G5B5");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_R5G6B5)
		{
			strcpy(szOutString, "R5G6B5");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_U8888)
		{
			strcpy(szOutString, "U8888");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_U8)
		{
			strcpy(szOutString, "U8");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_U88)
		{
			strcpy(szOutString, "U88");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_F32)
		{
			strcpy(szOutString, "F32");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616)
		{
			strcpy(szOutString, "F1616");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_F16)
		{
			strcpy(szOutString, "F16");
		}
		else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_U16)
		{
			strcpy(szOutString, "U16");
		}
		else
		{
			strcpy(szOutString, "INVALID");
		}

	}

	/* encode texture swizzle in case of SGX543 */

	strcat(szOutString, ".(");

	if(bExtFmt)
	{
		switch(uTexCtrWrd)
		{
			case EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII:
			{
				if(uTexSwizzle>7)
				{
					uTexSwizzle = 8;
				}
				strcat(szOutString, aszTextureSwizzle4Channel[uTexSwizzle]);
				break;
			}

			case EURASIA_PDS_DOUTT1_TEXFORMAT_YUV422:
			{
				if(uTexSwizzle>7)
				{
					uTexSwizzle = 8;
				}
				strcat(szOutString, aszTextureSwizzleYUV422[uTexSwizzle]);
				break;
			}

			case EURASIA_PDS_DOUTT1_TEXFORMAT_YUV420_2P:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_YUV420_3P:
			{
				if(uTexSwizzle>3)
				{
					uTexSwizzle = 4;
				}
				strcat(szOutString, aszTextureSwizzleYUV420[uTexSwizzle]);
				break;
			}

			default:
			{
				strcat(szOutString,"----");
				break;
			}
		}
	}
	else
	{
		switch(uTexCtrWrd)
		{
			case EURASIA_PDS_DOUTT1_TEXFORMAT_U8:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_F16:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_F32:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_U16:
			{
				if(uTexSwizzle>7)
				{
					uTexSwizzle = 8;
				}
				strcat(szOutString, aszTextureSwizzle1Channel[uTexSwizzle]);
				break;
			}

			case EURASIA_PDS_DOUTT1_TEXFORMAT_U88:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_F1616:
			{
				if(uTexSwizzle>5)
				{
					uTexSwizzle = 6;
				}
				strcat(szOutString, aszTextureSwizzle2Channel[uTexSwizzle]);
				break;
			}

			case EURASIA_PDS_DOUTT1_TEXFORMAT_R5G6B5:
			{
				if(uTexSwizzle>1)
				{
					uTexSwizzle = 2;
				}
				strcat(szOutString, aszTextureSwizzle3Channel[uTexSwizzle]);
				break;
			}

			case EURASIA_PDS_DOUTT1_TEXFORMAT_U8888:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4:
			case EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5:
			{
				if(uTexSwizzle>7)
				{
					uTexSwizzle = 8;
				}
				strcat(szOutString, aszTextureSwizzle4Channel[uTexSwizzle]);
				break;
			}

			default:
			{
				strcat(szOutString,"----");
				break;
			}
		}
	}

	strcat(szOutString, ")");

	#endif /* defined(SGX_FEATURE_TAG_SWIZZLE) */

	#if defined(SGX520) || defined(SGX530) || defined(SGX531) || defined(SGX535) || defined(SGX540) || defined(SGX541)
	if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_A4R4G4B4)
	{
		strcpy(szOutString, "A4R4G4B4");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_A1R5G5B5)
	{
		strcpy(szOutString, "A1R5G5B5");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_R5G6B5)
	{
		strcpy(szOutString, "R5G6B5");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_B8G8R8A8)
	{
		strcpy(szOutString, "B8G8R8A8");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_R8G8B8A8)
	{
		strcpy(szOutString, "R8G8B8A8");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_U8)
	{
		strcpy(szOutString, "U8");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_U88)
	{
		strcpy(szOutString, "U88");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_F32)
	{
		strcpy(szOutString, "F32");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_F1616)
	{
		strcpy(szOutString, "F1616");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_F16)
	{
		strcpy(szOutString, "F16");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_U16)
	{
		strcpy(szOutString, "U16");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT2BPP)
	{
		strcpy(szOutString, "PVRT2BPP");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRT4BPP)
	{
		strcpy(szOutString, "PVRT4BPP");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII2BPP)
	{
		strcpy(szOutString, "PVRTII2BPP");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTII4BPP)
	{
		strcpy(szOutString, "PVRTII4BPP");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_PVRTIII)
	{
		strcpy(szOutString, "PVRTIII");
	}	
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_YUY2)
	{
		strcpy(szOutString, "YUY2");
	}
	else if(uTexCtrWrd == EURASIA_PDS_DOUTT1_TEXFORMAT_UYVY)
	{
		strcpy(szOutString, "UYVY");
	}
	else
	{
		strcpy(szOutString, "INVALID");
	}
	#endif /* defined(SGX520) || defined(SGX530) || defined(SGX531) || defined(SGX535) || defined(SGX540) || defined(SGX541) */

	#if defined(EURASIA_PDS_DOUTT0_TEXFEXT)
	if(bExtFmt)
	{
		strcat(szOutString,"_E1");
	}
	else
	{
		strcat(szOutString,"_E0");
	}
	#endif /* defined(EURASIA_PDS_DOUTT0_TEXFEXT) */

	#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
	if(bReplct)
	{
		strcat(szOutString,"_R1");
	}
	else
	{
		strcat(szOutString,"_R0");
	}
	#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

	#if defined(SGX545)
	if(uUnPackFmt == EURASIA_PDS_DOUTT3_FCONV_F32)
	{
		strcat(szOutString,"_UF32");
	}
	else if(uUnPackFmt == EURASIA_PDS_DOUTT3_FCONV_F16)
	{
		strcat(szOutString,"_UF16");
	}
	
	/*
		Dump the tag size
	*/
	sprintf(szOutString, "%s_TAGSIZE(%u)", szOutString, uTagSize);
	#endif /* defined(SGX545) */

	#if defined(EURASIA_PDS_DOUTT3_FCNORM)
	if(bIsNormaliseSet)
	{
		strcat(szOutString,"_NORMALISE");
	}						
	#endif /* defined(EURASIA_PDS_DOUTT3_FCNORM) */

}

/*****************************************************************************
 Name:		DecodeHWShader

 Purpose:	Decode a USP HW shader

 Inputs:	psContext	- The current USP execution context
			psHWShader	- The HW shader to decode

 Outputs:	none

 Returns:	none
*****************************************************************************/
static IMG_VOID DecodeHWShader(PUSP_CONTEXT psContext, PUSP_HW_SHADER psHWShader)
{
	PUSP_HW_SAREG_USE	psSARegUse;
	IMG_UINT32			uFlags;

	USP_DBGPRINT((" Main program instruction count: %d\n", psHWShader->uInstCount));
	USP_DBGPRINT((" SA-update program instruction count: %d\n",
			  psHWShader->uSAUpdateInstCount));

	USP_DBGPRINT((" Main program start: +%d\n", psHWShader->uProgStartInstIdx));
	if	(psHWShader->uPTPhase0EndInstIdx != (IMG_UINT16)-1)
	{
		USP_DBGPRINT((" Phase 0 end: +%d\n", psHWShader->uPTPhase0EndInstIdx));
	}
	if	(psHWShader->uPTPhase1StartInstIdx != (IMG_UINT16)-1)
	{
		USP_DBGPRINT((" Phase 1 start: +%d\n", psHWShader->uPTPhase1StartInstIdx));
	}
	if	(psHWShader->uPTSplitPhase1StartInstIdx != (IMG_UINT16)-1)
	{
		USP_DBGPRINT((" Split Phase 1 start: +%d\n", psHWShader->uPTSplitPhase1StartInstIdx));
	}

	uFlags = psHWShader->uFlags;
	USP_DBGPRINT((" Flags: 0x%8.8X ( %s%s%s%s%s%s%s%s%s%s%s%s)\n",
			  uFlags,
			  (uFlags & USP_HWSHADER_FLAGS_CFASEL_ENABLED_AT_END)?"CSEL_AT_END ":"",
			  (uFlags & USP_HWSHADER_FLAGS_SIMPLE_PS)?"SIMPLE_PS ":"",
			  (uFlags & USP_HWSHADER_FLAGS_SAPROG_LABEL_AT_END)?"SAPROG_LABEL_AT_END ":"",
			  (uFlags & USP_HWSHADER_FLAGS_SPLITALPHACALC)?"SPLITALPHACALC ":"",
			  (uFlags & USP_HWSHADER_FLAGS_SPLITCALC)?"SPLITCALC ":"",
			  (uFlags & USP_HWSHADER_FLAGS_TEXTUREREADS)?"TEXTUREREADS ":"",
			  (uFlags & USP_HWSHADER_FLAGS_DEPTHFEEDBACK)?"DEPTHFEEDBACK ":"",
			  (uFlags & USP_HWSHADER_FLAGS_OMASKFEEDBACK)?"OMASKFEEDBACK ":"",
			  (uFlags & USP_HWSHADER_FLAGS_PER_INSTANCE_MODE)?"PER_INSTANCE_MODE ":"",
			  (uFlags & USP_HWSHADER_FLAGS_LABEL_AT_END)?"LABEL_AT_END ":"",
			  (uFlags & USP_HWSHADER_FLAGS_TEXKILL_USED)?"TEXKILL_USED ":"",
			  (uFlags & USP_HWSHADER_FLAGS_ABS_BRANCHES_USED)?"ABS_BRANCHES_USED ":""));

	USP_DBGPRINT((" Temp reg count: %d\n", psHWShader->uTempRegCount));
	USP_DBGPRINT((" PA reg count: %d (%d reserved)\n", 
			  psHWShader->uPARegCount, 
			  psHWShader->uReservedPARegCount));
	USP_DBGPRINT((" SA reg count: %d\n", psHWShader->uSARegCount));

	psSARegUse = &psHWShader->sSARegUse;
	if	(psSARegUse->uFlags & USP_HW_SAREG_USE_FLAG_MEMCONSTS_USED)
	{
		USP_DBGPRINT((" Mem-consts base-address SA Reg: %d\n", psSARegUse->uMemConstBaseAddrReg));
		USP_DBGPRINT((" Mem-consts count: %d\n", psHWShader->uMemConstCount));
		USP_DBGPRINT((" Mem tex-state count: %d\n", psHWShader->uMemTexStateCount));
	}
	if	(psSARegUse->uFlags & USP_HW_SAREG_USE_FLAG_MEMCONSTS2_USED)
	{
		USP_DBGPRINT((" Mem-consts 2nd base-address SA Reg: %d\n", psSARegUse->uMemConst2BaseAddrReg));
	}
	if	(psSARegUse->uFlags & USP_HW_SAREG_USE_FLAG_SCRATCH_USED)
	{
		USP_DBGPRINT((" Scratch base-address SA Reg: %d\n", psSARegUse->uScratchMemBaseAddrReg));
		USP_DBGPRINT((" Scratch area size: %d\n", psHWShader->uScratchAreaSize));
	}
	if	(psSARegUse->uFlags & USP_HW_SAREG_USE_FLAG_TEXSTATE_USED)
	{
		USP_DBGPRINT((" Reg tex-state state first SA Reg: %d\n", psSARegUse->uTexStateFirstReg));
		USP_DBGPRINT((" Reg tex-state count: %d\n", psHWShader->uRegTexStateCount));
	}
	if	(psSARegUse->uFlags & USP_HW_SAREG_USE_FLAG_REGCONSTS_USED)
	{
		USP_DBGPRINT((" Reg-consts first SA Reg: %d\n", psSARegUse->uRegConstsFirstReg));
		USP_DBGPRINT((" Reg-consts SA Reg count: %d\n", psSARegUse->uRegConstsRegCount));
	}
	if	(psSARegUse->uFlags & USP_HW_SAREG_USE_FLAG_INDEXEDTEMPS_USED)
	{
		USP_DBGPRINT((" Indexed Temp-reg base-address SA Reg: %d\n", psSARegUse->uIndexedTempBaseAddrReg));
		USP_DBGPRINT((" Indexed Temp-reg size: %d\n", psHWShader->uIndexedTempTotalSize));
	}

	USP_DBGPRINT((" Mem base-address adjustment: %d\n", psHWShader->iSAAddressAdjust));
	USP_DBGPRINT((" PS result reg-num: %d\n", psHWShader->uPSResultRegNum));
	{
		IMG_UINT32	i;
		
		USP_DBGPRINT((" Non-aniso texture stages: "));
		for (i = 0; i < (sizeof(psHWShader->auNonAnisoTexStages) / sizeof(psHWShader->auNonAnisoTexStages[0])); i++)
		{
			USP_DBGPRINT(("0x%8.8X", psHWShader->auNonAnisoTexStages[i]));
		}
		USP_DBGPRINT(("\n"));
	}

	if	(psHWShader->uPSInputCount > 0)
	{
		PUSP_HW_PSINPUT_LOAD	psPSInputLoad;
		IMG_UINT32				uPAReg;
		IMG_UINT32				i;

		USP_DBGPRINT((" Iterations/Pre-sampled textures:\n"));

		uPAReg = 0;

		psPSInputLoad = psHWShader->psPSInputLoads;
		for	(i = 0; i < psHWShader->uPSInputCount; i++)
		{
			static char* aszHWPSInputCoord[] = 
			{
				"tc0",	/* USP_HW_PSINPUT_COORD_TC0	= 0	*/
				"tc1",	/* USP_HW_PSINPUT_COORD_TC1	= 1	*/
				"tc2",	/* USP_HW_PSINPUT_COORD_TC2	= 2	*/
				"tc3",	/* USP_HW_PSINPUT_COORD_TC3	= 3	*/
				"tc4",	/* USP_HW_PSINPUT_COORD_TC4	= 4	*/
				"tc5",	/* USP_HW_PSINPUT_COORD_TC5	= 5	*/
				"tc6",	/* USP_HW_PSINPUT_COORD_TC6	= 6	*/
				"tc7",	/* USP_HW_PSINPUT_COORD_TC7	= 7	*/
				"tc8",	/* USP_HW_PSINPUT_COORD_TC8	= 8	*/
				"tc9",	/* USP_HW_PSINPUT_COORD_TC9	= 9	*/
				"v0",	/* USP_HW_PSINPUT_COORD_V0	= 10 */
				"v1",	/* USP_HW_PSINPUT_COORD_V1	= 11 */
				"fog",	/* USP_HW_PSINPUT_COORD_FOG	= 12 */
				"pos",	/* USP_HW_PSINPUT_COORD_POS	= 13 */
				"vprim" /* USP_HW_PSINPUT_COORD_VPRIM = 14*/
			};
			static char* aszHWPSInputFormat[] = 
			{
				"",		/* USP_HW_PSINPUT_FMT_F32	= 0 */
				"f16",	/* USP_HW_PSINPUT_FMT_F16	= 1 */
				"c10",	/* USP_HW_PSINPUT_FMT_C10	= 2 */
				"u8"	/* USP_HW_PSINPUT_FMT_U8	= 3 */
			};
			#if defined(SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT)
			static char* aszSMPConvertFormat[] = 
			{
				"f32",	/* USP_HW_PSINPUT_FMT_F32	= 0 */
				"f16",	/* USP_HW_PSINPUT_FMT_F16	= 1 */
				"c10",	/* USP_HW_PSINPUT_FMT_C10	= 2 */
				"u8"	/* USP_HW_PSINPUT_FMT_U8	= 3 */
				"",		/* USP_HW_PSINPUT_FMT_NONE	= 4 */
			};
			#endif /* defined(SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT) */
			static char* aszHWPSInputCoordDim[] = 
			{
				"", "u", "uv", "uvs", "uvst"
			};

			switch (psPSInputLoad->eType)
			{
				case USP_HW_PSINPUT_TYPE_ITERATION:
				{
					USP_DBGPRINT((" %3d pa%d = %s%s%s%s.%s%s%s uRegCountSet=%hu\n",
							  i,
							  uPAReg,
							  aszHWPSInputFormat[psPSInputLoad->eFormat],
							  (psPSInputLoad->eFormat != USP_HW_PSINPUT_FMT_F32)? "(":"",
							  (psPSInputLoad->uFlags & USP_HW_PSINPUT_FLAG_CENTROID)? "cent(":"",
							  aszHWPSInputCoord[psPSInputLoad->eCoord],
							  aszHWPSInputCoordDim[psPSInputLoad->uCoordDim],
  							  (psPSInputLoad->uFlags & USP_HW_PSINPUT_FLAG_CENTROID)? ")":"",
							  (psPSInputLoad->eFormat != USP_HW_PSINPUT_FMT_F32)? ")":"",
							  psPSInputLoad->uDataSize));
					break;
				}
				case USP_HW_PSINPUT_TYPE_SAMPLE:
				{
					#if defined(SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT)
					USP_DBGPRINT((" %3d pa%d = smp_p%d.%s(%s%s%s%s%s, tex%d.%d [%hu]) uRegCountSet=%hu\n",
							  i,
							  uPAReg,
							  psPSInputLoad->bPackSampleResults,
							  aszSMPConvertFormat[psPSInputLoad->eFormat],
							  (psPSInputLoad->uFlags & USP_HW_PSINPUT_FLAG_PROJECTED)?"proj(":"",
							  (psPSInputLoad->uFlags & USP_HW_PSINPUT_FLAG_CENTROID)?"cent(":"",
							  aszHWPSInputCoord[psPSInputLoad->eCoord],
							  (psPSInputLoad->uFlags & USP_HW_PSINPUT_FLAG_CENTROID)?")":"",
							  (psPSInputLoad->uFlags & USP_HW_PSINPUT_FLAG_PROJECTED)?")":"",
							  psHWShader->psTextCtrWords[psPSInputLoad->uTexCtrWrdIdx].uTextureIdx,
							  psHWShader->psTextCtrWords[psPSInputLoad->uTexCtrWrdIdx].uChunkIdx,
							  psPSInputLoad->uTexCtrWrdIdx,
							  psPSInputLoad->uDataSize));
					#else
					USP_DBGPRINT((" %3d pa%d = smp(%s%s%s%s%s, tex%d.%d [%hu]) uRegCountSet=%hu\n",
							  i,
							  uPAReg,
							  (psPSInputLoad->uFlags & USP_HW_PSINPUT_FLAG_PROJECTED)?"proj(":"",
							  (psPSInputLoad->uFlags & USP_HW_PSINPUT_FLAG_CENTROID)?"cent(":"",
							  aszHWPSInputCoord[psPSInputLoad->eCoord],
							  (psPSInputLoad->uFlags & USP_HW_PSINPUT_FLAG_CENTROID)?")":"",
							  (psPSInputLoad->uFlags & USP_HW_PSINPUT_FLAG_PROJECTED)?")":"",
							  psHWShader->psTextCtrWords[psPSInputLoad->uTexCtrWrdIdx].uTextureIdx,
							  psHWShader->psTextCtrWords[psPSInputLoad->uTexCtrWrdIdx].uChunkIdx,
							  psPSInputLoad->uTexCtrWrdIdx,
							  psPSInputLoad->uDataSize));
					#endif /* defined(SGX_FEATURE_USE_SMP_RESULT_FORMAT_CONVERT) */
					break;
				}
				default:
				{
					USP_DBGPRINT((" Unexpected PS input-type\n"));
					break;
				}
			}
		
			uPAReg += psPSInputLoad->uDataSize;
			psPSInputLoad++;
		}
	}

	if	(psHWShader->uRegTexStateCount > 0)
	{
		PUSP_HW_TEXSTATE_LOAD	psTexStateLoad;
		IMG_UINT32				uTexStateCount;
		IMG_UINT32				i;

		USP_DBGPRINT((" Reg texture-state loads:\n"));

		uTexStateCount	= psHWShader->uRegTexStateCount;
		psTexStateLoad	= psHWShader->psRegTexStateLoads;

		for	(i = 0; i < uTexStateCount; i++)
		{
			USP_DBGPRINT((" %3d sa%d = tex%d.%d [%hu]\n",
					  i,
					  psTexStateLoad->uDestIdx,
					  psHWShader->psTextCtrWords[psTexStateLoad->uTexCtrWrdIdx].uTextureIdx,
					  psHWShader->psTextCtrWords[psTexStateLoad->uTexCtrWrdIdx].uChunkIdx,
					  psTexStateLoad->uTexCtrWrdIdx));

			psTexStateLoad++;
		}
	}

	if	(psHWShader->uMemTexStateCount > 0)
	{
		PUSP_HW_TEXSTATE_LOAD	psTexStateLoad;
		IMG_UINT32				uTexStateCount;
		IMG_UINT32				i;

		USP_DBGPRINT((" Mem texture-state loads:\n"));

		uTexStateCount	= psHWShader->uMemTexStateCount;
		psTexStateLoad	= psHWShader->psMemTexStateLoads;

		for	(i = 0; i < uTexStateCount; i++)
		{
			USP_DBGPRINT((" %3d [sa%d+%d] = tex%d.%d [%hu]\n",
					  i,
					  psSARegUse->uMemConstBaseAddrReg,
					  psTexStateLoad->uDestIdx,
					  psHWShader->psTextCtrWords[psTexStateLoad->uTexCtrWrdIdx].uTextureIdx,
					  psHWShader->psTextCtrWords[psTexStateLoad->uTexCtrWrdIdx].uChunkIdx,
					  psTexStateLoad->uTexCtrWrdIdx));

			psTexStateLoad++;
		}
	}

	if	(psHWShader->uVSInputPARegCount > 0)
	{
		IMG_UINT32	i;

		USP_DBGPRINT((" Num VS-input PA regs: %d\n", psHWShader->uVSInputPARegCount));
		for	(i = 0; i < (psHWShader->uVSInputPARegCount+0x1F) >> 5; i++)
		{
			USP_DBGPRINT(("  pa%d-%d: %8.8X\n", 
					  (i<<5) + 31, 
					  i<<5, 
					  psHWShader->auVSInputsUsed[i]));
		}
	}

	if	(psHWShader->uInstCount > 0)
	{
		USP_DBGPRINT((" Main program:\n"));

		DecodeUSECode(psContext,
					  &psHWShader->sTargetDev,
					  psHWShader->uInstCount,
					  psHWShader->puInsts);

		USP_DBGPRINT(("\n"));
	}

	if	(psHWShader->uSAUpdateInstCount > 0)
	{
		USP_DBGPRINT((" SA-update program:\n"));

		DecodeUSECode(psContext,
					  &psHWShader->sTargetDev,
					  psHWShader->uSAUpdateInstCount,
					  psHWShader->puSAUpdateInsts);

		USP_DBGPRINT(("\n"));
	}

	USP_DBGPRINT((" Total texture control word entries in list %u:\n", psHWShader->uTexCtrWrdLstCount));
	if(psHWShader->uTexCtrWrdLstCount)
	{
		IMG_UINT32	uTexChunkIdx;
		IMG_CHAR	szTexCtrWrd[128];

		for(uTexChunkIdx=0; uTexChunkIdx<psHWShader->uTexCtrWrdLstCount; uTexChunkIdx++)
		{
			szTexCtrWrd[0] = IMG_NULL;

			DecodeTextureControlWord(psHWShader->psTextCtrWords[uTexChunkIdx].auWord, 
				psHWShader->psTextCtrWords[uTexChunkIdx].auMask, 
				szTexCtrWrd);
			

			USP_DBGPRINT((" [%u] TEX %hu CHUNK %hu (%s)\n", 
				uTexChunkIdx, 
				psHWShader->psTextCtrWords[uTexChunkIdx].uTextureIdx, 
				psHWShader->psTextCtrWords[uTexChunkIdx].uChunkIdx, 
				szTexCtrWrd));
		}
	}
	
	if(psHWShader->uShaderOutputsCount > 0)
	{
		USP_DBGPRINT((" Valid Shader Outputs:"));
		{
			IMG_UINT32 uOutputsCountInDwords;
			IMG_UINT32 uDwordIdx;
			uOutputsCountInDwords = (psHWShader->uShaderOutputsCount + 31) / 32;			
			for(uDwordIdx = 0; uDwordIdx < uOutputsCountInDwords; uDwordIdx++)
			{
				USP_DBGPRINT((" %08X", psHWShader->puValidShaderOutputs[uDwordIdx]));
			}
			USP_DBGPRINT(("\n"));
		}
	}
}
#endif /* #if defined(DEBUG) && defined(USP_DECODE_HWSHADER) */

/*****************************************************************************
 Name:		SetTextureControlWords

 Purpose:	Sets texture control words for the given hardware shader.

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader being finalised
			psHWShader  - The hardware shader to set texture control words for

 Outputs:	none

 Returns:	IMG_TRUE/IMG_FALSE on pass/failure
*****************************************************************************/
static IMG_BOOL SetTextureControlWords(PUSP_CONTEXT						psContext, 
									   PUSP_SHADER						psShader,
									   PUSP_HW_SHADER					psHWShader)
{
	
	IMG_UINT32	uPlnIdx;
	IMG_UINT32	uWordCnt;
	IMG_UINT32	uTexChunkIdx;
	IMG_UINT32 uSmpIdx;

	USP_UNREFERENCED_PARAMETER(psContext);

	/* 
		Initailize texture control words and respective masks.
	*/
	for(uTexChunkIdx=0; uTexChunkIdx<psHWShader->uTexCtrWrdLstCount; uTexChunkIdx++)
	{
		for(uWordCnt=0; uWordCnt<USP_TEXSTATE_SIZE; uWordCnt++)
		{
			psHWShader->psTextCtrWords[uTexChunkIdx].auWord[uWordCnt] = 0;
			psHWShader->psTextCtrWords[uTexChunkIdx].auMask[uWordCnt] = (IMG_UINT32)-1;

			psHWShader->psTextCtrWords[uTexChunkIdx].uTextureIdx = (IMG_UINT16)-1;
			psHWShader->psTextCtrWords[uTexChunkIdx].uChunkIdx = (IMG_UINT16)-1;
		}
	}

	for (uSmpIdx=0; uSmpIdx<psShader->uTotalSmpTexCtrWrds; uSmpIdx++)
	{
		if(psShader->psTexCtrWrds[uSmpIdx].bUsed)
		{
			for(uPlnIdx=0; uPlnIdx<USP_TEXFORM_MAX_PLANE_COUNT; uPlnIdx++)
			{
				IMG_UINT16 uTexCtrWrdIdx;

				uTexCtrWrdIdx = psShader->psTexCtrWrds[uSmpIdx].auTexWrdIdx[uPlnIdx];

				if(uTexCtrWrdIdx != (IMG_UINT16)-1)
				{
					psHWShader->psTextCtrWords[uTexCtrWrdIdx].uTextureIdx = 
						psShader->psTexCtrWrds[uSmpIdx].uTextureIdx;

					psHWShader->psTextCtrWords[uTexCtrWrdIdx].uChunkIdx = 
						(IMG_UINT16)uPlnIdx;

					psHWShader->psTextCtrWords[uTexCtrWrdIdx].auWord[1] = 
						psShader->psTexCtrWrds[uSmpIdx].auWords[uPlnIdx];

					psHWShader->psTextCtrWords[uTexCtrWrdIdx].auMask[1] &= 
						EURASIA_PDS_DOUTT1_TEXFORMAT_CLRMSK;
					
					#if	defined(EURASIA_PDS_DOUTT0_TEXFEXT)
					if(psShader->psTexCtrWrds[uSmpIdx].abExtFmt[uPlnIdx])
					{
						psHWShader->psTextCtrWords[uTexCtrWrdIdx].auWord[0] |= 
							EURASIA_PDS_DOUTT0_TEXFEXT;

						psHWShader->psTextCtrWords[uTexCtrWrdIdx].auMask[0] &= 
							~EURASIA_PDS_DOUTT0_TEXFEXT;
					}
					#endif	/* defined(EURASIA_PDS_DOUTT0_TEXFEXT) */

					#if defined(EURASIA_PDS_DOUTT0_CHANREPLICATE)
					if(psShader->psTexCtrWrds[uSmpIdx].abReplicate[uPlnIdx])
					{
						psHWShader->psTextCtrWords[uTexCtrWrdIdx].auWord[0] |= 
							EURASIA_PDS_DOUTT0_CHANREPLICATE;

						psHWShader->psTextCtrWords[uTexCtrWrdIdx].auMask[0] &= 
							~EURASIA_PDS_DOUTT0_CHANREPLICATE;
					}
					#endif /* defined(EURASIA_PDS_DOUTT0_CHANREPLICATE) */

					#if defined(SGX_FEATURE_TAG_SWIZZLE)
					psHWShader->psTextCtrWords[uTexCtrWrdIdx].auWord[3] |= 
						psShader->psTexCtrWrds[uSmpIdx].auSwizzle[uPlnIdx];

					psHWShader->psTextCtrWords[uTexCtrWrdIdx].auMask[3] &= 
						EURASIA_PDS_DOUTT3_SWIZ_CLRMASK;
					#endif /* defined SGX_FEATURE_TAG_SWIZZLE */

					#if defined(SGX545)
					/*
						Set unpacking format
					*/
					{
						IMG_UINT32 uUnPackFormat = EURASIA_PDS_DOUTT3_FCONV_NONE;

						switch(psShader->psTexCtrWrds[uSmpIdx].aeUnPackFmts[uPlnIdx])
						{
							case USP_REGFMT_UNKNOWN:
							{
								uUnPackFormat = EURASIA_PDS_DOUTT3_FCONV_NONE;
								break;
							}
							
							case USP_REGFMT_F32:
							{
								uUnPackFormat = EURASIA_PDS_DOUTT3_FCONV_F32;
								break;
							}

							case USP_REGFMT_F16:
							{
								uUnPackFormat = EURASIA_PDS_DOUTT3_FCONV_F16;
								break;
							}

							default:
							{
								uUnPackFormat = EURASIA_PDS_DOUTT3_FCONV_NONE;
								break;
							}	
						}
						psHWShader->psTextCtrWords[uTexCtrWrdIdx].auWord[3] = 
							uUnPackFormat;

						psHWShader->psTextCtrWords[uTexCtrWrdIdx].auMask[3] &= 
							EURASIA_PDS_DOUTT3_FCONV_CLRMSK;
					}
					#endif /* defined(SGX545) */

					/* Set normalise flag */
					#if defined(EURASIA_PDS_DOUTT3_FCNORM)
					{
						IMG_BOOL bSetNormaliseFlag = IMG_FALSE;
						switch(psShader->psTexCtrWrds[uSmpIdx].aeUnPackFmts[uPlnIdx])
						{
							case USP_REGFMT_F32:
							case USP_REGFMT_F16:
							{
								bSetNormaliseFlag = IMG_TRUE;
								break;
							}

							default:
							{
								bSetNormaliseFlag = IMG_FALSE;
								break;
							}
						}
						if(bSetNormaliseFlag)
						{
							psHWShader->psTextCtrWords[uTexCtrWrdIdx].auWord[3] |= 
								EURASIA_PDS_DOUTT3_FCNORM;

							psHWShader->psTextCtrWords[uTexCtrWrdIdx].auMask[3] &= 
								~EURASIA_PDS_DOUTT3_FCNORM;
						}
					}
					#endif /* defined(EURASIA_PDS_DOUTT3_FCNORM) */

					#if defined(SGX545)
					{
						IMG_UINT32 uTagSize;
						uTagSize = psShader->psTexCtrWrds[uSmpIdx].auTagSize[uPlnIdx];
						uTagSize = (uTagSize << EURASIA_PDS_DOUTT3_TAGDATASIZE_SHIFT) & (~EURASIA_PDS_DOUTT3_TAGDATASIZE_CLRMSK);

						psHWShader->psTextCtrWords[uTexCtrWrdIdx].auWord[3] |= uTagSize;
						psHWShader->psTextCtrWords[uTexCtrWrdIdx].auMask[3] &= EURASIA_PDS_DOUTT3_TAGDATASIZE_CLRMSK;

					}
					#endif /* defined(SGX545) */
				}
			}
		}
	}

	return IMG_TRUE;
}

/*****************************************************************************
 Name:		CreateHWShader

 Purpose:	Create a HW-shader from a finalised USP shader

 Inputs:	psContext	- The current USP execution context
			psShader	- The USP shader being finalised

 Outputs:	none

 Returns:	The created HW shader data, or IMG_NULL on failure
*****************************************************************************/
IMG_INTERNAL PUSP_HW_SHADER CreateHWShader(PUSP_CONTEXT psContext, PUSP_SHADER psShader)
{
	static const USP_HW_PSINPUT_COORD aeIteratedDataTypeToHWPSInputCoord[] = 
	{
		USP_HW_PSINPUT_COORD_TC0,	/* USP_ITERATED_DATA_TYPE_TC0	= 0 */
		USP_HW_PSINPUT_COORD_TC1,	/* USP_ITERATED_DATA_TYPE_TC1	= 1 */
		USP_HW_PSINPUT_COORD_TC2,	/* USP_ITERATED_DATA_TYPE_TC2	= 2 */
		USP_HW_PSINPUT_COORD_TC3,	/* USP_ITERATED_DATA_TYPE_TC3	= 3 */
		USP_HW_PSINPUT_COORD_TC4,	/* USP_ITERATED_DATA_TYPE_TC4	= 4 */
		USP_HW_PSINPUT_COORD_TC5,	/* USP_ITERATED_DATA_TYPE_TC5	= 5 */
		USP_HW_PSINPUT_COORD_TC6,	/* USP_ITERATED_DATA_TYPE_TC6	= 6 */
		USP_HW_PSINPUT_COORD_TC7,	/* USP_ITERATED_DATA_TYPE_TC7	= 7 */
		USP_HW_PSINPUT_COORD_TC8,	/* USP_ITERATED_DATA_TYPE_TC8	= 8 */
		USP_HW_PSINPUT_COORD_TC9,	/* USP_ITERATED_DATA_TYPE_TC9	= 9 */
		USP_HW_PSINPUT_COORD_V0,	/* USP_ITERATED_DATA_TYPE_V0	= 10 */
		USP_HW_PSINPUT_COORD_V1,	/* USP_ITERATED_DATA_TYPE_V1	= 11 */
		USP_HW_PSINPUT_COORD_FOG,	/* USP_ITERATED_DATA_TYPE_FOG	= 12 */
		USP_HW_PSINPUT_COORD_POS,	/* USP_ITERATED_DATA_TYPE_POS	= 13 */
		USP_HW_PSINPUT_COORD_VPRIM,	/* USP_ITERATED_DATA_TYPE_VPRIM	= 14 */
		USP_HW_PSINPUT_COORD_V0		/* USP_ITERATED_DATA_TYPE_PADDING = 15 */
	};
	static const USP_HW_PSINPUT_FMT aeUSPRegFmtToHWPSInputFormat[] =
	{
		USP_HW_PSINPUT_FMT_NONE,	/* USP_REGFMT_UNKNOWN	= 0 */
		USP_HW_PSINPUT_FMT_F32,		/* USP_REGFMT_F32		= 1 */
		USP_HW_PSINPUT_FMT_F16,		/* USP_REGFMT_F16		= 2 */
		USP_HW_PSINPUT_FMT_C10,		/* USP_REGFMT_C10		= 3 */
		USP_HW_PSINPUT_FMT_U8		/* USP_REGFMT_U8		= 4	*/
	};
	static const USP_HW_CONST_FMT aePCToHWConstFormat[] =
	{
		(USP_HW_CONST_FMT)0,		/* Reserved */
		USP_HW_CONST_FMT_F32,		/* USP_PC_CONST_FMT_F32		= 1 */
		USP_HW_CONST_FMT_F16,		/* USP_PC_CONST_FMT_F16		= 2 */
		USP_HW_CONST_FMT_C10,		/* USP_PC_CONST_FMT_C10		= 3 */
		USP_HW_CONST_FMT_U8,        /* USP_PC_CONST_FMT_U8		= 4 */
		USP_HW_CONST_FMT_STATIC		/* USP_PC_CONST_FMT_STATIC	= 5 */
	};

	PUSP_PROGDESC			psProgDesc;
	PUSP_INPUT_DATA			psInputData;
	PUSP_INSTBLOCK			psLastInstBlock;
	PUSP_HW_SHADER			psHWShader;
	PUSP_HW_SAREG_USE		psSARegUse;
	PUSP_LABEL				psLabel;
	PUSP_INST				psLastInst;
	IMG_UINT16				uInstCount;
	IMG_UINT32				uSARegCount;
	IMG_UINT16				uPSInputCount;
	IMG_UINT16				uProgStartInstIdx;
	IMG_UINT16				uPTPhase0EndInstIdx;
	IMG_UINT16				uPTPhase1StartInstIdx;
	IMG_UINT16				uPTSplitPhase1StartInstIdx;
	IMG_BOOL				bMainProgCanSetENDFlag;
	IMG_UINT32				uAllocSize;
	IMG_BOOL				bFailed;
	IMG_INT32				iPlnIdx;
	IMG_INT32				iSmpIdx;
	IMG_UINT32              uUsedTexCnt;

	psHWShader			= IMG_NULL;
	bFailed				= IMG_TRUE;

	psProgDesc	= psShader->psProgDesc;
	psInputData = psShader->psInputData;

	/*
		Determine the overall size of the main program
	*/
	psLastInstBlock	= psShader->psInstBlocksEnd;
	if	(psLastInstBlock)
	{
		uInstCount = (IMG_UINT16)(psLastInstBlock->uFirstInstIdx + 
								  psLastInstBlock->uInstCount);
	}
	else
	{
		uInstCount = 0;
	}

	/*
		Lookup special instruction indexes returned to the driver
	*/
	psLabel = psShader->psProgStartLabel;
	if	(psLabel)
	{
		PUSP_INSTBLOCK	psTargetBlock;

		psTargetBlock = psLabel->psInstBlock;
		if	(psTargetBlock)
		{
			uProgStartInstIdx = (IMG_UINT16)psTargetBlock->uFirstInstIdx;
		}
		else
		{
			uProgStartInstIdx = (IMG_UINT16)uInstCount;
		}
	}
	else
	{
		USP_DBGPRINT(( "CreateHWShader: Main program start label not found\n"));
		goto CreateHWShaderExit;
	}

	psLabel = psShader->psPTPhase0EndLabel;
	if	(psLabel)
	{
		PUSP_INSTBLOCK	psTargetBlock;

		psTargetBlock = psLabel->psInstBlock;
		if	(psTargetBlock)
		{
			uPTPhase0EndInstIdx = (IMG_UINT16)psTargetBlock->uFirstInstIdx;
		}
		else
		{
			uPTPhase0EndInstIdx = (IMG_UINT16)uInstCount;
		}
	}
	else
	{
		uPTPhase0EndInstIdx = (IMG_UINT16)-1;
	}

	psLabel = psShader->psPTPhase1StartLabel;
	if	(psLabel)
	{
		PUSP_INSTBLOCK	psTargetBlock;

		psTargetBlock = psLabel->psInstBlock;
		if	(psTargetBlock)
		{
			uPTPhase1StartInstIdx = (IMG_UINT16)psTargetBlock->uFirstInstIdx;
		}
		else
		{
			uPTPhase1StartInstIdx = (IMG_UINT16)uInstCount;
		}
	}
	else
	{
		uPTPhase1StartInstIdx = (IMG_UINT16)-1;
	}

	psLabel = psShader->psPTSplitPhase1StartLabel;
	if	(psLabel)
	{
		PUSP_INSTBLOCK	psTargetBlock;

		psTargetBlock = psLabel->psInstBlock;
		if	(psTargetBlock)
		{
			uPTSplitPhase1StartInstIdx = (IMG_UINT16)psTargetBlock->uFirstInstIdx;
		}
		else
		{
			uPTSplitPhase1StartInstIdx = (IMG_UINT16)uInstCount;
		}
	}
	else
	{
		uPTSplitPhase1StartInstIdx = (IMG_UINT16)-1;
	}

	/*
		Find the last instruction of the shader
	*/
	psLastInst = IMG_NULL;

	if	(uInstCount > 0)
	{
		/*
			Find the final instruction in the shader (skipping empty blocks
			at the end)
		*/
		while (!psLastInstBlock->psLastInst)
		{
			psLastInstBlock = psLastInstBlock->psPrev;
		}
		psLastInst = psLastInstBlock->psLastInst;
	}

	/*
		Determine whether the final instruction can take the END flag
	*/
	bMainProgCanSetENDFlag = IMG_FALSE;

	if	(uInstCount > 0)
	{
		USP_OPCODE	eLastOpcode;

		/*
			Check whether the final instruction supports the HW END flag
		*/
		eLastOpcode = psLastInst->sDesc.eOpcode;
		bMainProgCanSetENDFlag = HWInstSupportsEnd(eLastOpcode);

		/*
			Don't set the END flag on the last instruction if the shader originally
			ended with a label, and the final instrucion isn't in the very last block
			(which was appended to the shader by the USP, and is therefore after the
			final label).
		*/
		if	(
				(psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_LABEL_AT_END) &&
				(psLastInstBlock != psShader->psInstBlocksEnd)
			)
		{
			bMainProgCanSetENDFlag = IMG_FALSE;
		}
	}

	/*
		Determine the overall size of the HW-shader data
	*/
	uAllocSize = sizeof(USP_HW_SHADER);

	if	(uInstCount > 0)
	{
		uAllocSize += uInstCount * sizeof(HW_INST);
	}
	if	(psProgDesc->uSAUpdateInstCount > 0)
	{
		uAllocSize += psProgDesc->uSAUpdateInstCount * sizeof(HW_INST);
	}
	uPSInputCount = (IMG_UINT16)(psInputData->uIteratedDataCount + psInputData->uPreSampledDataCount);
	if	(uPSInputCount > 0)
	{
		uAllocSize += uPSInputCount * sizeof(USP_HW_PSINPUT_LOAD);
	}
	if	(psProgDesc->uMemConstCount > 0)
	{
		uAllocSize += psProgDesc->uMemConstCount * sizeof(USP_HW_CONST_LOAD);
	}
	if	(psProgDesc->uRegConstCount > 0)
	{
		uAllocSize += psProgDesc->uRegConstCount * sizeof(USP_HW_CONST_LOAD);
	}
	if	(psInputData->uTexStateDataCount > 0)
	{
		uAllocSize += psInputData->uTexStateDataCount * sizeof(USP_HW_TEXSTATE_LOAD);
	}

	/*
		Calculate space required for texture control words 
	*/
	uUsedTexCnt = 0;

	/*
		Calculate size required for texture control words
	*/
	for(iSmpIdx=(IMG_INT32)(psShader->uTotalSmpTexCtrWrds-1); 
		(iSmpIdx>=0) && (uUsedTexCnt==0); 
		iSmpIdx--)
	{
		if(psShader->psTexCtrWrds[iSmpIdx].bUsed)
		{
			for(iPlnIdx=USP_TEXFORM_MAX_PLANE_COUNT-1; iPlnIdx>=0; iPlnIdx--)
			{
				if(psShader->psTexCtrWrds[iSmpIdx].auTexWrdIdx[iPlnIdx] !=
					(IMG_UINT16)-1)
				{
					uUsedTexCnt = 
						psShader->psTexCtrWrds[iSmpIdx].auTexWrdIdx[iPlnIdx] + 1;

					break;
				}
			}
		}
	}

	uAllocSize += uUsedTexCnt * sizeof(USP_TEX_CTR_WORDS);

	if(psShader->psProgDesc->uShaderOutputsCount > 0)
	{
		IMG_UINT32 uShdrOutputFlgsInDwords = (((psShader->psProgDesc->uShaderOutputsCount) + 31) / 32);
		uAllocSize += (uShdrOutputFlgsInDwords * sizeof(IMG_UINT32));
	}

	/*
		Allocate and initialise the top level structure
	*/
	psHWShader = (PUSP_HW_SHADER)psContext->pfnAlloc(uAllocSize);
	if	(!psHWShader)
	{
		USP_DBGPRINT(( "CreateHWShader: Failed to alloc USP_HW_SHADER\n"));
		goto CreateHWShaderExit;
	}

	psHWShader->uFlags = 0;
	if	(psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_TEXKILL_USED)
	{
		psHWShader->uFlags |= USP_HWSHADER_FLAGS_TEXKILL_USED;
	}
	if	(!bMainProgCanSetENDFlag)
	{
		psHWShader->uFlags |= USP_HWSHADER_FLAGS_LABEL_AT_END;
	}
	if	(psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_PER_INSTANCE_MODE)
	{
		psHWShader->uFlags |= USP_HWSHADER_FLAGS_PER_INSTANCE_MODE;
	}
	if	(psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_DEPTHFEEDBACK)
	{
		psHWShader->uFlags |= USP_HWSHADER_FLAGS_DEPTHFEEDBACK;
	}
	if	(psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_OMASKFEEDBACK)
	{
		psHWShader->uFlags |= USP_HWSHADER_FLAGS_OMASKFEEDBACK;
	}
	if	(psShader->psDepSamples)
	{
		psHWShader->uFlags |= USP_HWSHADER_FLAGS_TEXTUREREADS;
	}
	if	(psProgDesc->uFlags & USP_PC_PROGDESC_FLAGS_SPLITFEEDBACKCALC)
	{
		psHWShader->uFlags |= USP_HWSHADER_FLAGS_SPLITALPHACALC;
	}
	if	(psProgDesc->uFlags & USP_PC_PROGDESC_FLAGS_SPLITCALC)
	{
		psHWShader->uFlags |= USP_HWSHADER_FLAGS_SPLITCALC;
	}
	if	(psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_SAPROG_LABEL_AT_END)
	{
		psHWShader->uFlags |= USP_HWSHADER_FLAGS_SAPROG_LABEL_AT_END;
	}
	if (psProgDesc->uHWFlags & UNIFLEX_HW_FLAGS_ABS_BRANCHES_USED)
	{
		psHWShader->uFlags |= USP_HWSHADER_FLAGS_ABS_BRANCHES_USED;
	}

	/*
		For pixel-shaders, set flags to assist the combiner.
	*/
	if	(psProgDesc->uShaderType == USP_PC_SHADERTYPE_PIXEL)
	{
		if	(uInstCount > 0)
		{
			IMG_UINT32	uInstDescFlags;

			/*
				If the very last instruction of the shader is the only
				one that writes the colour result of the shader, then we
				may be able to combine that instruction with the blend code
				for extra performance.
			*/
			uInstDescFlags = psLastInst->sDesc.uFlags;
			if	(
					(uInstDescFlags & USP_INSTDESC_FLAG_DEST_USES_RESULT) &&
					!(psProgDesc->uHWFlags & USP_HWSHADER_FLAGS_LABEL_AT_END) &&
					!(uInstDescFlags & USP_INSTDESC_FLAG_PARTIAL_RESULT_WRITE) && 
					(
						/* 
							There should be no references to the Result of the Shader. 
						*/
						((!(psShader->psResultRefs)) || (!(psShader->psResultRefs->psNext))) 
						|| 
						(
							/* 
								Or we have inserted move instructions at the end to move results to PAs. 
							*/
							(psProgDesc->uFlags & USP_PC_PROGDESC_FLAGS_RESULT_REFS_NOT_PATCHABLE_WITH_PAS) 
							&& 
							(psContext->eOutputRegType == USP_OUTPUT_REGTYPE_PA)
						)
					)
				)
			{
				USP_REPEAT_MODE	eMode;
				IMG_UINT32		uRepeat;				
				if(!HWInstDecodeRepeat(&(psLastInst->sHWInst), psLastInst->sDesc.eOpcode, &eMode, &uRepeat))
				{
					USP_DBGPRINT(("CreateHWShader: Error decoding inst repeat\n"));
					goto CreateHWShaderExit;				
				}
				if(eMode == USP_REPEAT_MODE_NONE)
				{
					psHWShader->uFlags |= USP_HWSHADER_FLAGS_SIMPLE_PS;
				}
			}

			/*
				Flag whether per-operand format control is enabled for
				colour instructions at the end of the program.
			*/
			if	(psLastInst->psInstBlock->sFinalMOEState.bColFmtCtl)
			{
				psHWShader->uFlags |= USP_HWSHADER_FLAGS_CFASEL_ENABLED_AT_END;
			}
		}		
	}

	psHWShader->sTargetDev				= psProgDesc->sTargetDev;
	psHWShader->uInstCount				= (IMG_UINT16)uInstCount;
	psHWShader->puInsts					= (IMG_PUINT32)(psHWShader + 1);
	psHWShader->uProgStartInstIdx		= uProgStartInstIdx;
	psHWShader->uPTPhase0EndInstIdx		= uPTPhase0EndInstIdx;
	psHWShader->uPTPhase1StartInstIdx	= uPTPhase1StartInstIdx;
	psHWShader->uPTSplitPhase1StartInstIdx 
										= uPTSplitPhase1StartInstIdx;
	psHWShader->uSAUpdateInstCount		= psProgDesc->uSAUpdateInstCount;
	psHWShader->puSAUpdateInsts			= (IMG_PUINT32)((PHW_INST)psHWShader->puInsts + 
														uInstCount);
	psHWShader->uPARegCount				= (IMG_UINT16)psShader->uFinalPARegCount;
	psHWShader->uReservedPARegCount		= (IMG_UINT16)psProgDesc->uExtraPARegs;
	psHWShader->uTempRegCount			= (IMG_UINT16)psShader->uFinalTempRegCount;
	psHWShader->uSecTempRegCount		= (IMG_UINT16)psProgDesc->uSecTempRegCount;
	psHWShader->uSARegCount				= 0;
	psHWShader->uPSInputCount			= uPSInputCount;
	psHWShader->psPSInputLoads			= (PUSP_HW_PSINPUT_LOAD)((PHW_INST)psHWShader->puSAUpdateInsts +
																 psProgDesc->uSAUpdateInstCount);
	psHWShader->uMemConstCount			= psProgDesc->uMemConstCount;
	psHWShader->psMemConstLoads			= (PUSP_HW_CONST_LOAD)(psHWShader->psPSInputLoads +
															   uPSInputCount);
	psHWShader->uRegConstCount			= psProgDesc->uRegConstCount;
	psHWShader->psRegConstLoads			= psHWShader->psMemConstLoads + psProgDesc->uMemConstCount;
	psHWShader->uRegTexStateCount		= (IMG_UINT16)psInputData->uRegTexStateDataCount;
	psHWShader->psRegTexStateLoads		= (PUSP_HW_TEXSTATE_LOAD)(psHWShader->psRegConstLoads +
																  psProgDesc->uRegConstCount);
	psHWShader->uMemTexStateCount		= psInputData->uMemTexStateDataCount;
	psHWShader->psMemTexStateLoads		= psHWShader->psRegTexStateLoads + psInputData->uRegTexStateDataCount;
	psHWShader->uIndexedTempTotalSize	= psProgDesc->uIndexedTempTotalSize;
	psHWShader->uScratchAreaSize		= psProgDesc->uScratchAreaSize;
	psHWShader->iSAAddressAdjust		= psProgDesc->iSAAddressAdjust;
	psHWShader->uPSResultRegNum			= (IMG_UINT16)psShader->uFinalResultRegNum;
	memcpy(psHWShader->auNonAnisoTexStages, psProgDesc->auNonAnisoTexStages, sizeof(psHWShader->auNonAnisoTexStages));
	psHWShader->uTexCtrWrdLstCount		= uUsedTexCnt;
	psHWShader->uShaderOutputsCount		= psProgDesc->uShaderOutputsCount;

	if(psHWShader->uTexCtrWrdLstCount)
	{
		psHWShader->psTextCtrWords = 
			(PUSP_TEX_CTR_WORDS)((PUSP_HW_TEXSTATE_LOAD)psHWShader->psMemTexStateLoads + 
			psInputData->uMemTexStateDataCount);
	}
	else
	{
		psHWShader->psTextCtrWords = IMG_NULL;
	}

	if(psHWShader->uTexCtrWrdLstCount)
	{
		psHWShader->puValidShaderOutputs = 
			(IMG_PUINT32)((PUSP_TEX_CTR_WORDS)psHWShader->psTextCtrWords + psHWShader->uTexCtrWrdLstCount);
	}
	else
	{
		psHWShader->puValidShaderOutputs = 
			(IMG_PUINT32)((PUSP_HW_TEXSTATE_LOAD)psHWShader->psMemTexStateLoads + 
			psInputData->uMemTexStateDataCount);
	}

	/*
		Copy the input-usage info for vertex-shaders
	*/
	if	(psProgDesc->uShaderType == USP_PC_SHADERTYPE_VERTEX)
	{
		IMG_UINT32 i;

		psHWShader->uVSInputPARegCount = psProgDesc->uVSInputPARegCount;

		for	(i = 0; i < (psProgDesc->uVSInputPARegCount + 0x1F) >> 5; i++)
		{
			psHWShader->auVSInputsUsed[i] = psProgDesc->auVSInputsUsed[i];
		}
		for	(; i < EURASIA_USE_PRIMATTR_BANK_SIZE>>5; i++)
		{
			psHWShader->auVSInputsUsed[i] = 0;
		}
	}
	else
	{
		psHWShader->uVSInputPARegCount = 0;
	}

	/*
		Setup the SA register useage details
	*/
	psSARegUse = &psHWShader->sSARegUse;

	psSARegUse->uFlags = 0;
	uSARegCount = 0;

	if	(
			(psProgDesc->uSAUsageFlags & USP_PC_SAUSAGEFLAG_MEMCONSTBASE) ||
			(psInputData->uMemTexStateDataCount > 0)
		)
	{
		psSARegUse->uFlags |= USP_HW_SAREG_USE_FLAG_MEMCONSTS_USED;
		psSARegUse->uMemConstBaseAddrReg = psProgDesc->uMemConstBaseAddrSAReg;

		if	(psSARegUse->uMemConstBaseAddrReg >= uSARegCount)
		{
			uSARegCount = psSARegUse->uMemConstBaseAddrReg + 1;
		}
	}

	if	(psProgDesc->uSAUsageFlags & USP_PC_SAUSAGEFLAG_MEMCONSTBASE2)
	{
		psSARegUse->uFlags |= USP_HW_SAREG_USE_FLAG_MEMCONSTS2_USED;
		psSARegUse->uMemConst2BaseAddrReg = psProgDesc->uMemConst2BaseAddrSAReg;

		if	(psSARegUse->uMemConst2BaseAddrReg >= uSARegCount)
		{
			uSARegCount = psSARegUse->uMemConst2BaseAddrReg + 1;
		}
	}

	if	(psProgDesc->uSAUsageFlags & USP_PC_SAUSAGEFLAG_SCRATCHMEMBASE)
	{
		psSARegUse->uFlags |= USP_HW_SAREG_USE_FLAG_SCRATCH_USED;
		psSARegUse->uScratchMemBaseAddrReg = psProgDesc->uScratchMemBaseAddrSAReg;

		if	(psSARegUse->uScratchMemBaseAddrReg >= uSARegCount)
		{
			uSARegCount = psSARegUse->uScratchMemBaseAddrReg + 1;
		}
	}

	if	(psInputData->uRegTexStateDataCount > 0)
	{
		IMG_UINT32	uTexStateLastReg;

		psSARegUse->uFlags |= USP_HW_SAREG_USE_FLAG_TEXSTATE_USED;
		psSARegUse->uTexStateFirstReg = (IMG_UINT16)psInputData->psTexStateData->uRegNum;

		uTexStateLastReg = psSARegUse->uTexStateFirstReg + 
						   (psInputData->uRegTexStateDataCount * USP_TEXSTATE_SIZE) - 1;

		if	(uTexStateLastReg >= uSARegCount)
		{
			uSARegCount = uTexStateLastReg + 1;
		}
	}

	if	(psProgDesc->uSAUsageFlags & USP_PC_SAUSAGEFLAG_REMAPPEDCONSTS)
	{
		IMG_UINT32	uRegConstsLastReg;

		psSARegUse->uFlags |= USP_HW_SAREG_USE_FLAG_REGCONSTS_USED;
		psSARegUse->uRegConstsFirstReg = psProgDesc->uRegConstsFirstSAReg;
		psSARegUse->uRegConstsRegCount = psProgDesc->uRegConstsSARegCount;

		uRegConstsLastReg = psSARegUse->uRegConstsFirstReg + 
							psSARegUse->uRegConstsRegCount - 1;

		if	(uRegConstsLastReg >= uSARegCount)
		{
			uSARegCount = uRegConstsLastReg + 1;
		}
	}

	if	(psProgDesc->uSAUsageFlags & USP_PC_SAUSAGEFLAG_INDEXTEMPMEMBASE)
	{
		psSARegUse->uFlags |= USP_HW_SAREG_USE_FLAG_INDEXEDTEMPS_USED;
		psSARegUse->uIndexedTempBaseAddrReg = psProgDesc->uIndexedTempBaseAddrSAReg;

		if	(psSARegUse->uIndexedTempBaseAddrReg >= uSARegCount)
		{
			uSARegCount = psSARegUse->uIndexedTempBaseAddrReg + 1;
		}
	}

	psHWShader->uSARegCount = (IMG_UINT16)uSARegCount;

	/*
		Setup the array of HW instructions for the main program
	*/
	if	(uInstCount > 0)
	{
		PHW_INST		psHWInst;
		PUSP_INSTBLOCK	psBlock;

		/*
			Copy the code from all the instruction blocks
		*/
		psHWInst = (PHW_INST)psHWShader->puInsts;

		psBlock = psShader->psInstBlocks;
		while (psBlock)
		{
			PUSP_INST psInst;

			psInst = psBlock->psFirstInst;
			if	(psInst)
			{
				for	(;;)
				{
					*psHWInst++ = psInst->sHWInst;

					if	(psInst == psBlock->psLastInst)
					{
						break;
					}
					psInst = psInst->psNext;
				}
			}
		
			psBlock = psBlock->psNext;
		}
	}

	/*
		Setup the array of HW instructions for the SA update program
	*/
	if	(psProgDesc->uSAUpdateInstCount > 0)
	{
		PHW_INST	psSrcHWInst;
		PHW_INST	psDestHWInst;
		IMG_UINT16	uInstCount;

		/*
			Copy the pre-compiled SA-update program
		*/
		uInstCount		= psHWShader->uSAUpdateInstCount;
		psDestHWInst	= (PHW_INST)psHWShader->puSAUpdateInsts;
		psSrcHWInst		= psProgDesc->psSAUpdateInsts;

		while (uInstCount--)
		{
			*psDestHWInst++ = *psSrcHWInst++;
		}
	}

	/*
		Setup the list of iterate/pre-sampled pixel-shader inputs
	*/
	if	(uPSInputCount > 0)
	{
		PUSP_HW_PSINPUT_LOAD	psHWInputLoad;
		PUSP_ITERATED_DATA		psIteratedData;
		PUSP_ITERATED_DATA		psIteratedDataEnd;
		PUSP_PRESAMPLED_DATA	psPreSampledData;
		PUSP_PRESAMPLED_DATA	psPreSampledDataEnd;
		IMG_UINT32				i;

		/*
			Fill in the list of iterated and pre-sampled data, according to
			their destination register order.

			NB: Items in these lists will already have ascending PA register
				numbers, but we may have interleaved interations and pre-sampled
				data.
		*/
		psHWInputLoad		= psHWShader->psPSInputLoads;
		psIteratedData		= psInputData->psIteratedData;
		psIteratedDataEnd	= psIteratedData + psInputData->uIteratedDataCount;
		psPreSampledData	= psInputData->psPreSampledData;
		psPreSampledDataEnd	= psPreSampledData + psInputData->uPreSampledDataCount;

		for	(i = 0; i < uPSInputCount; i++)
		{
			IMG_BOOL bAddIteratedData;

			/*
				Record the iterated and pre-sampled data in increasing PA register
				number.
			*/
			bAddIteratedData = IMG_FALSE;
			if	(psIteratedData < psIteratedDataEnd)
			{
				if	(psPreSampledData >= psPreSampledDataEnd)
				{
					bAddIteratedData = IMG_TRUE;
				}
				else if	(psIteratedData->uRegNum < psPreSampledData->uRegNum)
				{
					bAddIteratedData = IMG_TRUE;
				}
			}

			if	(bAddIteratedData)
			{
				USP_REGFMT	eFmt;

				/*
					Record the details of this iteration
				*/
				psHWInputLoad->eType		= USP_HW_PSINPUT_TYPE_ITERATION;
				psHWInputLoad->uFlags		= 0;

				if	(psIteratedData->bCentroid)
				{
					psHWInputLoad->uFlags	|= USP_HW_PSINPUT_FLAG_CENTROID;
				}

				eFmt = psIteratedData->eRegFmt;
				if(eFmt == USP_REGFMT_UNKNOWN)
				{
					eFmt = USP_REGFMT_F32;
				}

				psHWInputLoad->eCoord				= aeIteratedDataTypeToHWPSInputCoord[psIteratedData->eType];
				psHWInputLoad->uCoordDim			= (IMG_UINT16)psIteratedData->uCompCount;
				psHWInputLoad->uTexCtrWrdIdx		= 0xFFFF;
				psHWInputLoad->bPackSampleResults	= IMG_FALSE;
				psHWInputLoad->eFormat				= aeUSPRegFmtToHWPSInputFormat[eFmt];
				psHWInputLoad->uDataSize			= psIteratedData->uItrSize;

				psIteratedData++;
			}
			else
			{
				/*
					Record the details of this pre-sampled data
				*/
				if(psPreSampledData->uTexCtrWrdIdx == (IMG_UINT16)-1)
				{
					/* 
						Its a dummy entry just record it as an iteration. This 
						entry cannot be used because the original sample chunk 
						for this entry requires more than 1 registers and it 
						is only giving one.
					*/
					psHWInputLoad->eType		= USP_HW_PSINPUT_TYPE_ITERATION;
					psHWInputLoad->uFlags		= 0;

					if	(psIteratedData->bCentroid)
					{
						psHWInputLoad->uFlags	|= USP_HW_PSINPUT_FLAG_CENTROID;
					}

					psHWInputLoad->eCoord				= aeIteratedDataTypeToHWPSInputCoord[psPreSampledData->eCoord];
					psHWInputLoad->uTexCtrWrdIdx		= 0xFFFF;
					psHWInputLoad->bPackSampleResults	= IMG_FALSE;
					psHWInputLoad->uDataSize			= psPreSampledData->uTagSize;

					if(psHWInputLoad->uDataSize == 2)
					{
						psHWInputLoad->uCoordDim = (IMG_UINT16)4;
						psHWInputLoad->eFormat = aeUSPRegFmtToHWPSInputFormat[USP_REGFMT_F16];
					}
					else if(psHWInputLoad->uDataSize == 4)
					{
						psHWInputLoad->uCoordDim = (IMG_UINT16)4;
						psHWInputLoad->eFormat = aeUSPRegFmtToHWPSInputFormat[USP_REGFMT_F32];
					}
					else
					{
						psHWInputLoad->uCoordDim = (IMG_UINT16)1;
						psHWInputLoad->eFormat = aeUSPRegFmtToHWPSInputFormat[USP_REGFMT_U8];
					}
				}
				else
				{
					psHWInputLoad->eType		= USP_HW_PSINPUT_TYPE_SAMPLE;
					psHWInputLoad->uFlags		= 0;

					if	(psPreSampledData->bProjected)
					{
						psHWInputLoad->uFlags	|= USP_HW_PSINPUT_FLAG_PROJECTED;
					}
					if	(psPreSampledData->bCentroid)
					{
						psHWInputLoad->uFlags	|= USP_HW_PSINPUT_FLAG_CENTROID;
					}

					psHWInputLoad->eCoord				= aeIteratedDataTypeToHWPSInputCoord[psPreSampledData->eCoord];
					psHWInputLoad->uCoordDim			= 0;
					psHWInputLoad->uTexCtrWrdIdx		= psPreSampledData->uTexCtrWrdIdx;
					psHWInputLoad->bPackSampleResults	= IMG_FALSE;
					psHWInputLoad->eFormat				= aeUSPRegFmtToHWPSInputFormat[psPreSampledData->eFmtConvert];
					psHWInputLoad->uDataSize			= psPreSampledData->uTagSize;
				}

				psPreSampledData++;
			}

			psHWInputLoad++;
		}
	}

	/*
		Setup the list of in-memory constants
	*/
	if	(psProgDesc->uMemConstCount > 0)
	{
		PUSP_HW_CONST_LOAD	psHWConstLoad;
		PUSP_PC_CONST_LOAD	psPCConstLoad;
		IMG_UINT32			i;

		/*
			Copy the list of pre-compiled in-memory constants
		*/
		psHWConstLoad = psHWShader->psMemConstLoads;
		psPCConstLoad = psProgDesc->psMemConstLoads;

		for	(i = 0; i < psProgDesc->uMemConstCount; i++)
		{
			if	(psPCConstLoad->uFormat == USP_PC_CONST_FMT_STATIC)
			{
				psHWConstLoad->uSrcIdx	= psPCConstLoad->sSrc.uStaticVal;
			}
			else
			{
				psHWConstLoad->uSrcIdx	= psPCConstLoad->sSrc.sConst.uSrcIdx;
			}

			psHWConstLoad->uDestIdx		= psPCConstLoad->uDestIdx;
			psHWConstLoad->uDestShift	= psPCConstLoad->uDestShift;
			psHWConstLoad->uSrcShift	= psPCConstLoad->sSrc.sConst.uSrcShift;
			psHWConstLoad->eFormat		= aePCToHWConstFormat[psPCConstLoad->uFormat];

			psPCConstLoad++;
			psHWConstLoad++;
		}
	}

	/*
		Setup the list of in-register constants
	*/
	if	(psProgDesc->uRegConstCount > 0)
	{
		PUSP_HW_CONST_LOAD	psHWConstLoad;
		PUSP_PC_CONST_LOAD	psPCConstLoad;
		IMG_UINT32			i;

		/*
			Copy the list of pre-compiled in-memory constants

			NB: Currently, destination indexes for in-register constants are
				relative to the base-address supplied to the USC. To be 
				simple and consistent for both texture-state and constants
				reported back from the USP, we make the dest index for 
				constants alsolute here too.
		*/
		psHWConstLoad = psHWShader->psRegConstLoads;
		psPCConstLoad = psProgDesc->psRegConstLoads;

		for	(i = 0; i < psProgDesc->uRegConstCount; i++)
		{
			if	(psPCConstLoad->uFormat == USP_PC_CONST_FMT_STATIC)
			{
				psHWConstLoad->uSrcIdx	= psPCConstLoad->sSrc.uStaticVal;
			}
			else
			{
				psHWConstLoad->uSrcIdx	= psPCConstLoad->sSrc.sConst.uSrcIdx;
			}

			psHWConstLoad->uDestIdx		= (IMG_UINT16)(psPCConstLoad->uDestIdx + 
													   psProgDesc->uRegConstsFirstSAReg);
			psHWConstLoad->uDestShift	= psPCConstLoad->uDestShift;
			psHWConstLoad->uSrcShift	= psPCConstLoad->sSrc.sConst.uSrcShift;
			psHWConstLoad->eFormat		= aePCToHWConstFormat[psPCConstLoad->uFormat];

			psPCConstLoad++;
			psHWConstLoad++;
		}
	}

	/*
		Setup the lists of in-register and in-memory texture-state loads
	*/
	if	(psInputData->uTexStateDataCount > 0)
	{
		PUSP_HW_TEXSTATE_LOAD	psHWRegTexStateLoad;
		PUSP_HW_TEXSTATE_LOAD	psHWMemTexStateLoad;
		PUSP_TEXSTATE_DATA		psTexStateData;
		IMG_UINT32				i;

		/*
			Copy the list of texture-state loads
		*/
		psHWRegTexStateLoad	= psHWShader->psRegTexStateLoads;
		psHWMemTexStateLoad	= psHWShader->psMemTexStateLoads;
		psTexStateData		= psInputData->psTexStateData;

		for	(i = 0; i < psInputData->uTexStateDataCount; i++)
		{
			if	(!psTexStateData->bInMemory)
			{
				psHWRegTexStateLoad->uTexCtrWrdIdx	= psTexStateData->uTexCtrWrdIdx;
				psHWRegTexStateLoad->uTagSize		= psTexStateData->uTagSize;
				psHWRegTexStateLoad->uDestIdx		= (IMG_UINT16)psTexStateData->uRegNum;
				psHWRegTexStateLoad++;
			}
			else
			{
				psHWMemTexStateLoad->uTexCtrWrdIdx	= psTexStateData->uTexCtrWrdIdx;
				psHWMemTexStateLoad->uTagSize		= psTexStateData->uTagSize;
				psHWMemTexStateLoad->uDestIdx		= (IMG_UINT16)(psProgDesc->uTexStateMemOffset +
																   psTexStateData->uRegNum);
				psHWMemTexStateLoad++;
			}

			psTexStateData++;
		}
	}

	/*
		Set texture control words.
		It will never fail.
	*/
	if(!SetTextureControlWords(psContext, psShader, psHWShader))
	{
		USP_DBGPRINT(( "CreateHWShader: Unable to set texture control words\n"));
		goto CreateHWShaderExit;
	}

	/* 
		Sets Shader Outputs that are Valid After Patching.
	*/
	if(psHWShader->uShaderOutputsCount > 0)
	{
		IMG_UINT32 uOutputsCountInDwords;
		IMG_UINT32 uBitsInLastDword;
		IMG_UINT32 uDwordIdx;
		uOutputsCountInDwords = psHWShader->uShaderOutputsCount / 32;
		for(uDwordIdx = 0; uDwordIdx < uOutputsCountInDwords; uDwordIdx++)
		{
			psHWShader->puValidShaderOutputs[uDwordIdx] = psProgDesc->puValidShaderOutputs[uDwordIdx];
		}
		uBitsInLastDword = psHWShader->uShaderOutputsCount % 32;
		if(uBitsInLastDword)
		{
			IMG_UINT32	uBitMask;
			uBitMask = ((IMG_UINT32)(-1));
			uBitMask = (uBitMask << uBitsInLastDword);
			uBitMask = ~uBitMask;
			psHWShader->puValidShaderOutputs[uOutputsCountInDwords]  = (psProgDesc->puValidShaderOutputs[uOutputsCountInDwords]) & uBitMask;
		}
	}
	else
	{
		psHWShader->puValidShaderOutputs = IMG_NULL;
	}

	/*
		HW shader created OK
	*/
	bFailed = IMG_FALSE;

	#if defined(DEBUG) && defined(USP_DECODE_HWSHADER)
	DecodeHWShader(psContext, psHWShader);
	#endif /* #if defined(DEBUG) && defined(DECODE_HWSHADER) */

	CreateHWShaderExit:

	/*
		Cleanup on error
	*/
	if	(bFailed)
	{
		if	(psHWShader)
		{
			psContext->pfnFree(psHWShader);
			psHWShader = IMG_NULL;
		}
	}

	return psHWShader;
}

/******************************************************************************
 End of file (FINALISE.C)
******************************************************************************/

