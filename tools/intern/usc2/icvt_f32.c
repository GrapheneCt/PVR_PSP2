/*************************************************************************
 * Name         : icvt_f32.c
 * Title        : UNIFLEX-to-intermediate code conversion for F32 data format
 * Created      : July 2006
 *
 * Copyright    : 2002-2006 by Imagination Technologies Limited. All rights reserved.
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
 * Modifications:-
 * $Log: icvt_f32.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"
#include <limits.h>

static
IMG_VOID StoreIntoSpecialDest(PINTERMEDIATE_STATE	psState,
							  PCODEBLOCK			psCodeBlock,
							  PUNIFLEX_INST			psInputInst,
							  PUF_REGISTER			psInputDest)
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		StoreIntoVectorDest(psState, psCodeBlock, psInputInst, psInputDest);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		PVR_UNREFERENCED_PARAMETER(psInputInst);

		if (psInputDest->eType == UFREG_TYPE_INDEXABLETEMP)
		{
			StoreIndexableTemp(psState, psCodeBlock, psInputDest, UF_REGFORMAT_F32, USC_TEMPREG_F32INDEXTEMPDEST);
		}
	}
}

IMG_INTERNAL
IMG_UINT32 ConvertTempRegisterNumberF32(PINTERMEDIATE_STATE		psState,
										IMG_UINT32				uInputRegisterNumber,
										IMG_UINT32				uChan)
/*****************************************************************************
 FUNCTION	: ConvertTempRegisterNumberF32
    
 PURPOSE	: Convert an input temporary register number to a temporary register
			  number in the intermediate format..

 PARAMETERS	: uInputRegisterNumber			- The input register number.
			  uChan							- The channel within the input register.
			  
 RETURNS	: The converted number.
*****************************************************************************/
{
	IMG_UINT32	uRegNum;
	
	ASSERT(uInputRegisterNumber < psState->uInputTempRegisterCount);
	uRegNum = USC_TEMPREG_MAXIMUM + uInputRegisterNumber * CHANNELS_PER_INPUT_REGISTER + uChan;
	ASSERT(uRegNum < psState->uNumRegisters);

	return uRegNum;
}

static IMG_BOOL IsBetweenValidShaderOutputRanges(PINTERMEDIATE_STATE	psState, 
												 IMG_UINT32 			uOutputIndex, 
												 IMG_UINT32				*puPackedOutputIndex, 
												 IMG_UINT32				*puPackedRangeEnd)
/*****************************************************************************
 FUNCTION	: IsBetweenValidShaderOutputRanges

 PURPOSE	: Tests whether the given output index is with in the Valid Shader
			  Output ranges.

 PARAMETERS	: psState					- Compiler state.
			  uOutputIndex				- Shader output index.

 RETURNS	: IMG_TRUE	- If index is in between Valid Shader output range.
			  IMG_FALSE	- If index is NOT in between any Valid Shader output
						  range.
*****************************************************************************/
{
	PUNIFLEX_RANGE	psFoundRange;
	IMG_UINT32		uFoundRangeIdx;

	uFoundRangeIdx = SearchRangeList(&psState->sValidShaderOutPutRanges, uOutputIndex, &psFoundRange);

	if (uFoundRangeIdx != USC_UNDEF)
	{
		PUNIFLEX_RANGE	psPackedFoundRange = &psState->psPackedValidOutPutRanges[uFoundRangeIdx];

		*puPackedOutputIndex = psPackedFoundRange->uRangeStart + (uOutputIndex - psFoundRange->uRangeStart);
		*puPackedRangeEnd = psPackedFoundRange->uRangeEnd;
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

static
IMG_VOID GetPSOutputSource(PINTERMEDIATE_STATE	psState,
						   PARG					psHwSource,
						   PUF_REGISTER			psInputSource,
						   IMG_UINT32			uSrcChan)
/*****************************************************************************
 FUNCTION	: GetPSOutputSource
    
 PURPOSE	: Converts an input source for a pixel shader output register to
			  a source in the intermediate format.

 PARAMETERS	: psState					- Compiler state.
			  psHwSource				- On return contains the intermediate source.
			  psInputSource				- Input register.
			  uChan						- Channel in the input register.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(psInputSource->uNum < UFREG_OUTPUT_OMASK);

	InitInstArg(psHwSource);
	ConvertPixelShaderResultArg(psState, psInputSource, psHwSource);
	psHwSource->uNumber += uSrcChan;
}

IMG_INTERNAL
IMG_VOID GetVSOutputSource(PINTERMEDIATE_STATE	psState,
						   PARG					psHwSource,
						   PUF_REGISTER			psInputSource,
						   IMG_UINT32			uChan, 
						   PCODEBLOCK			psCodeBlock)
/*****************************************************************************
 FUNCTION	: GetVSOutputSource
    
 PURPOSE	: Converts an input source for a vertex shader output register to
			  a source in the intermediate format.

 PARAMETERS	: psState					- Compiler state.
			  psHwSource				- On return contains the intermediate source.
			  psInputSource				- Input register.
			  uChan						- Channel in the input register.
			  psCodeBlock				- Block to emit intermediate instructions if 
										  needed.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uOutputIndex = psInputSource->uNum + uChan;
	PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;

	if (psInputSource->uNum != UNIFLEX_VSOUTPUT_CLIPPOS)
	{
		IMG_BOOL	bValidOutput;
		IMG_UINT32	uPackedIndex;
		IMG_UINT32	uPackedRangeEnd;

		uPackedIndex = uPackedRangeEnd = USC_UNDEF;
		if (psInputSource->eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
		{
			bValidOutput = IsBetweenValidShaderOutputRanges(psState, uOutputIndex, &uPackedIndex, &uPackedRangeEnd);
		}
		else
		{
			if (GetBit(psState->psSAOffsets->puValidShaderOutputs, uOutputIndex))
			{
				IMG_UINT32	uIdx;

				bValidOutput = IMG_TRUE;

				/*
					Pack register numbers so the valid outputs are all consecutive.
				*/
				uPackedIndex = 0;
				for (uIdx = 0; uIdx < uOutputIndex; uIdx++)
				{
					if (GetBit(psState->puPackedShaderOutputs, uIdx))
					{
						uPackedIndex++;
					}
				}
				uPackedRangeEnd = uPackedIndex + 1;
			}
			else
			{
				bValidOutput = IMG_FALSE;
			}
		}

		if (!bValidOutput)
		{
			/*
				Point to a temporary register which is never read.
			*/
			psHwSource->uType = USEASM_REGTYPE_TEMP;
			psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
			psHwSource->uIndexNumber = USC_UNDEF;
			psHwSource->uIndexArrayOffset = USC_UNDEF;
			psHwSource->uNumber = USC_TEMPREG_DUMMY;
		}
		else
		{
			/*
				For vertex shader keeps track of the maximum output written.
			*/
			if (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_GEOMETRY)
			{
				psVS->uVertexShaderNumOutputs = max(psVS->uVertexShaderNumOutputs, uPackedRangeEnd);
			}

			if (psState->uFlags & USC_FLAGS_OUTPUTRELATIVEADDRESSING)
			{
				/*
					Point the destination to the register array representing
					the vertex shader inputs.
				*/
				psHwSource->uType = USC_REGTYPE_REGARRAY;
				psHwSource->uNumber = psVS->uVertexShaderOutputsArrayIdx;
				psHwSource->uArrayOffset = uPackedIndex;

				/*
					Convert the input dynamic index to the intermediate format.
				*/
				GetRelativeIndex(psState, psInputSource, psHwSource);

				/*
					For geometry shaders the vertex shader outputs are implicitly
					indexed by the current offset into the output buffer.
				*/
				if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY)
				{
					if (psHwSource->uIndexType == USC_REGTYPE_NOINDEX)
					{
						/*
							Use the output offset as an index.
						*/
						psHwSource->uIndexType = USEASM_REGTYPE_TEMP;
						psHwSource->uIndexNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;
						psHwSource->uIndexArrayOffset = 0;
					}
					else
					{
						PINST		psOutputBuffIdxAddInst;
						IMG_UINT32	uUserIndexScale;

						/*
							Get the units of the shader supplied index.
						*/
						uUserIndexScale = psHwSource->uIndexStrideInBytes;
						ASSERT((uUserIndexScale % LONG_SIZE) == 0);
						uUserIndexScale /= LONG_SIZE;
					
						/*
							Sum the output offset and the shader supplied index.
						*/
						psOutputBuffIdxAddInst = AllocateInst(psState, IMG_NULL);
						SetOpcode(psState, psOutputBuffIdxAddInst, IIMA16);
						psOutputBuffIdxAddInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
						psOutputBuffIdxAddInst->asDest[0].uNumber = GetNextRegister(psState);
						psOutputBuffIdxAddInst->asArg[0].uType = psHwSource->uIndexType;
						psOutputBuffIdxAddInst->asArg[0].uNumber = psHwSource->uIndexNumber;
						psOutputBuffIdxAddInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
						psOutputBuffIdxAddInst->asArg[1].uNumber = uUserIndexScale;
						psOutputBuffIdxAddInst->asArg[2].uType = USEASM_REGTYPE_TEMP;
						psOutputBuffIdxAddInst->asArg[2].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;			
						AppendInst(psState, psCodeBlock, psOutputBuffIdxAddInst);

						/*
							Use the sum as an index.
						*/
						psHwSource->uIndexType = USEASM_REGTYPE_TEMP;
						psHwSource->uIndexNumber = psOutputBuffIdxAddInst->asDest[0].uNumber;
						psHwSource->uIndexArrayOffset = 0;
					}

					psHwSource->uIndexStrideInBytes = LONG_SIZE;
				}
			}
			else
			{
				ASSERT(psInputSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE);
				ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX);

				/*
					Point the destination to the range of temporary registers representing the
					vertex shader outputs.
				*/
				psHwSource->uType = USEASM_REGTYPE_TEMP;
				psHwSource->uNumber = psVS->uVertexShaderOutputsFirstRegNum + uPackedIndex;
			}
		}
	}
	else
	{
		/*
			The vertex output is going to a temporary register.
		*/
		psState->uFlags |= USC_FLAGS_VSCLIPPOS_USED;

		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber = USC_TEMPREG_VSCLIPPOS + uChan;
	}
}

IMG_INTERNAL 
IMG_VOID GetDestinationF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUF_REGISTER psDest, IMG_UINT32 uChan, PARG psHwSource)
/*****************************************************************************
 FUNCTION	: GetDestinationF32
    
 PURPOSE	: Convert a destination register in the input format into the intermediate format.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Code block to insert any extra instructions needed.
			  psDest				- Destination register in the input format.
			  dwChan				- Channel in the destination.
			  psHwSource			- Pointer to the structure to fill out with the register in
									  the intermediate format.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psCodeBlock);

	InitInstArg(psHwSource);

	if (psDest->eType == UFREG_TYPE_PSOUTPUT)
	{
		ConvertPixelShaderResultArg(psState, psDest, psHwSource);
		psHwSource->uNumber += uChan;
	}
	else if (psDest->eType == UFREG_TYPE_VSOUTPUT)
	{
		GetVSOutputSource(psState, psHwSource, psDest, uChan, psCodeBlock);
	}
	else if (psDest->eType == UFREG_TYPE_INDEXABLETEMP)
	{
		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber = USC_TEMPREG_F32INDEXTEMPDEST + uChan;
	}
	else
	{
		ASSERT(psDest->eType == UFREG_TYPE_TEMP);
		ASSERT(psDest->eRelativeIndex == UFREG_RELATIVEINDEX_NONE);
		
		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber = ConvertTempRegisterNumberF32(psState, psDest->uNum, uChan);
	}
}

IMG_INTERNAL 
IMG_VOID GenerateDestModF32(PINTERMEDIATE_STATE psState, 
							PCODEBLOCK			psCodeBlock, 
							PARG				psDest, 
							IMG_UINT32			uSat, 
							IMG_UINT32			uScale, 
							IMG_UINT32			uPredSrc, 
							IMG_BOOL			bPredNegate)
/*****************************************************************************
 FUNCTION	: GenerateDestModF32
    
 PURPOSE	: Create instructions to a destination modifier.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Block to insert the instructions in.
			  psDest				- The destination register.
			  dwSat					- Saturation type to apply.
			  dwScale				- Scale to apply.
			  uPredSrc, bPredNegate	- Predicate for the instruction;
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST psInst;

	/* Apply a scale (just multiply the destination register by the appropriate constant. */
	if (uScale != UFREG_DMOD_SCALEMUL1)
	{
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IFMUL);
		SetPredicate(psState, psInst, uPredSrc, bPredNegate);
		psInst->asDest[0] = *psDest;
		psInst->asArg[0] = *psDest;
		psInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
		switch (uScale)
		{
			case UFREG_DMOD_SCALEMUL2: 
				psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2; 
				break;
			case UFREG_DMOD_SCALEMUL4: 	
				psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT4;
				break;
			case UFREG_DMOD_SCALEMUL8: 
				psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT8; 
				break;
			case UFREG_DMOD_SCALEDIV2: 
				psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER2; 	
				break;
			case UFREG_DMOD_SCALEDIV4: 
				psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER4; 
				break;
			case UFREG_DMOD_SCALEDIV8:
				psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER8; 
				break;
			case UFREG_DMOD_SCALEDIV16: 
				psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER16;
				break;
		}
		AppendInst(psState, psCodeBlock, psInst);
	}

	/* Apply a saturation by doing a min and max. */
	if (uSat != UFREG_DMOD_SATNONE)
	{
		#if defined(SUPPORT_SGX545)
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FCLAMP)
		{
			/* 
			   On sgx540, use clamping:
			   ifclampmin dest, psDest, x0, x1 
			   or
			   ifmax dest, psDest, 0.0  (for UFREG_DMOD_SATZEROMAX)
			*/
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, INVALID_MAXARG);
			SetPredicate(psState, psInst, uPredSrc, bPredNegate);

			psInst->asArg[0] = *psDest;
			psInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
			psInst->asArg[2].uType = USEASM_REGTYPE_FPCONSTANT;

			switch (uSat)
			{
				case UFREG_DMOD_SATZEROONE: 
				{
					/* ifclampmin dest, psDest, 0.0, 1.0 */
					SetOpcode(psState, psInst, IFMINMAX);
					psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
					psInst->asArg[2].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1; 
					break;
				}
				case UFREG_DMOD_SATZEROMAX: 
					/* ifclampmax dest, psDest, psDest, 0.0 */
					SetOpcode(psState, psInst, IFMAX);
					psInst->asArg[0] = *psDest;
					psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO; 
					break;
				case UFREG_DMOD_SATNEGONEONE:
				{
					/* ifclampmin dest, psDest, -1.0, 1.0 */
					SetOpcode(psState, psInst, IFMINMAX);
					psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
					psInst->u.psFloat->asSrcMod[1].bNegate = IMG_TRUE;
					psInst->asArg[2].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
					break;
				}
			}

			psInst->asDest[0] = *psDest;

			AppendInst(psState, psCodeBlock, psInst);
		}
		else
		#endif /* defined(SUPPORT_SGX545) */
		{
			/* 
			   Non-sgx540 code: 
			   max dest, psDest, x0
			   min dest, psDest, x1
			*/
			IMG_UINT32 i;
			for (i = 0; i < 2; i++)
			{
				psInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psInst, (i == 0) ? IFMAX : IFMIN);
				SetPredicate(psState, psInst, uPredSrc, bPredNegate);
				psInst->asDest[0] = *psDest;
				psInst->asArg[0] = *psDest;
				psInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
				switch (uSat)
				{
					case UFREG_DMOD_SATZEROONE: 
					{
						/* x0 = 0.0, x1 = 1.0 */
						psInst->asArg[1].uNumber = (i == 0) ? 
							EURASIA_USE_SPECIAL_CONSTANT_ZERO : 
							EURASIA_USE_SPECIAL_CONSTANT_FLOAT1; 
						break;
					}
					case UFREG_DMOD_SATZEROMAX: 
						/* x0 = 0.0, x1 = 0.0 */
						psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
						break;
					case UFREG_DMOD_SATNEGONEONE:
					{
						/* x0 = -1.0, x1 = 1.0 */
						psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
						if (i == 0)
						{
							psInst->u.psFloat->asSrcMod[1].bNegate = IMG_TRUE;
						}
						break;
					}
				}
				AppendInst(psState, psCodeBlock, psInst);

				if (uSat == UFREG_DMOD_SATZEROMAX)
				{
					break;
				}
			}
		}
	}
}

IMG_INTERNAL
IMG_VOID GetRelativeIndex(PINTERMEDIATE_STATE psState, 
						  PUF_REGISTER psSource,
						  PARG psHwSource)
/*****************************************************************************
 FUNCTION	: GetRelativeIndex
    
 PURPOSE	: Set up a relatively addressing register.

 PARAMETERS	: psState			- Compiler state.
			  psSource			- Input source register.
			  psHwSource		- Output source register.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	/*
		Check for relative addressing.
	*/
	if (psSource->eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
	{
		psHwSource->uIndexType = USEASM_REGTYPE_TEMP;
		if (psSource->eRelativeIndex == UFREG_RELATIVEINDEX_AL)
		{	
			ASSERT(psState->uD3DLoopIndex != (IMG_UINT32)-1);
			psHwSource->uIndexNumber = psState->uD3DLoopIndex;
		}
		else
		{
			psHwSource->uIndexNumber = USC_TEMPREG_ADDRESS + (psSource->eRelativeIndex - UFREG_RELATIVEINDEX_A0X);
		}
		psHwSource->uIndexArrayOffset = 0;
		if ((psState->uCompilerFlags & UF_GLSL) == 0)
		{
			psHwSource->uIndexStrideInBytes = DQWORD_SIZE;
		}
		else
		{
			psHwSource->uIndexStrideInBytes = psSource->u1.uRelativeStrideInComponents * LONG_SIZE;
		}
	}
	else
	{
		psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
		psHwSource->uIndexNumber = USC_UNDEF;
		psHwSource->uIndexArrayOffset = USC_UNDEF;
	}
}

IMG_INTERNAL 
IMG_VOID LoadIteratedValue(PINTERMEDIATE_STATE psState, 
					       PCODEBLOCK psCodeBlock, 
						   UF_REGTYPE eType,
						   PUF_REGISTER psSource,
						   IMG_UINT32 uSrcChan,
						   IMG_UINT32 uChan,
						   PARG psHwSource)
/*****************************************************************************
 FUNCTION	: LoadIteratedValue
    
 PURPOSE	: Generate code to load an iterated value (texture coordinate, vertex colour or position).

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block where the instructions to load the value should be added.
			  eType				- Type to load.
			  psSource			- Input source register.
			  uSrc				- Number of the source in the instruction.
			  uSrcChan			- Source channel after applying the swizzle.
			  uChan				- Source channel before applying the swizzle.
			  psHwSource		- USE register to fill out with the iterated value.
			  
 RETURNS	: None.
*****************************************************************************/
{
	UNIFLEX_ITERATION_TYPE	eIterationType;
	IMG_UINT32				uCoordinate;
	IMG_UINT32				uIterTempNum;
	IMG_UINT32				uArrayIdx;
	PUNIFLEX_RANGE			psRange;

	/*
		Convert from the USC register type to a hardware iteration source.
	*/
	switch (eType)
	{
		case UFREG_TYPE_COL: 
		{
			eIterationType = UNIFLEX_ITERATION_TYPE_COLOUR;
			uCoordinate = psSource->uNum;
			uSrcChan = (uSrcChan < 3) ? (2 - uSrcChan) : 3;
			break;
		}
		case UFREG_TYPE_TEXCOORD:
		case UFREG_TYPE_TEXCOORDP:
		{
			eIterationType = UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE;
			uCoordinate = psSource->uNum;
			break;
		}
		case UFREG_TYPE_MISC: 
		{
			uCoordinate = 0;
			if (psSource->uNum == UF_MISC_POSITION)
			{
				eIterationType = UNIFLEX_ITERATION_TYPE_POSITION;
			}
			else if (psSource->uNum == UF_MISC_FOG)
			{
				eIterationType = UNIFLEX_ITERATION_TYPE_FOG;
			}
			else if (psSource->uNum == UF_MISC_VPRIM)
			{
				ASSERT((psSource->eFormat == UF_REGFORMAT_I32) || (psSource->eFormat == UF_REGFORMAT_U32));				
				eIterationType = UNIFLEX_ITERATION_TYPE_VPRIM;
			}
			else
			{
				imgabort();
			}
			break;
		}
		default: imgabort();
	}

	/*
		Look for a dynamically indexed range containing this source.
	*/
	uArrayIdx = USC_UNDEF;
	psRange = NULL;
	if (eType == UFREG_TYPE_TEXCOORD)
	{
		uArrayIdx = SearchRangeList(&psState->psSAOffsets->sShaderInputRanges, psSource->uNum, &psRange);
	}

	InitInstArg(psHwSource);
	if (uArrayIdx != USC_UNDEF)
	{
		psHwSource->uType = USC_REGTYPE_REGARRAY;
		psHwSource->uNumber = psState->sShader.psPS->asTextureCoordinateArrays[uArrayIdx].uRegArrayIdx;
		psHwSource->uArrayOffset = (psSource->uNum - psRange->uRangeStart) * CHANNELS_PER_INPUT_REGISTER + uSrcChan;
	}
	else
	{
		/*
			Find the register destination for this iteration.
		*/
		uIterTempNum = 
			GetIteratedValue(psState, eIterationType, uCoordinate, UNIFLEX_TEXLOAD_FORMAT_F32, 4 /* uNumAttributes */);

		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber = uIterTempNum + uSrcChan;
	}
	GetRelativeIndex(psState, psSource, psHwSource);

	/* Apply projection to the texture coordinate. */
	if (eType == UFREG_TYPE_TEXCOORDP && uChan != 3)
	{
		PINST				psInst;
		UF_REGISTER			sSrc;
		IMG_UINT32			uTemp = GetNextRegister(psState);

		sSrc = *psSource;
		sSrc.u.uSwiz = UFREG_SWIZ_NONE;
		sSrc.byMod = UFREG_SMOD_NONE;

		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IFRCP);
		psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asDest[0].uNumber = uTemp;
		GetSourceF32(psState, 
					 psCodeBlock, 
					 &sSrc, 
					 3, 
					 &psInst->asArg[0], 
					 IMG_TRUE, 
					 &psInst->u.psFloat->asSrcMod[0]);
		AppendInst(psState, psCodeBlock, psInst);

		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IFMUL);
		psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asDest[0].uNumber = uTemp;
		psInst->asArg[0] = psInst->asDest[0];
		psInst->asArg[1] = *psHwSource;
		AppendInst(psState, psCodeBlock, psInst);

		*psHwSource = psInst->asDest[0];
	}
}

IMG_INTERNAL 
IMG_VOID ApplySourceModF32(PINTERMEDIATE_STATE psState, 
						   PCODEBLOCK psCodeBlock, 
						   PARG psHwSource, 
						   IMG_PUINT32 puComponent,
						   IMG_UINT32 uSMod)
/*****************************************************************************
 FUNCTION	: ApplySourceModF32
    
 PURPOSE	: Generate code to apply a source modifier.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block where the instructions to apply the source modifier should be added.
			  psHwSource		- USE register to which the source modifier should be applied.
			  uSMod				- Source modifier type.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uTemp = GetNextRegister(psState);

	switch (uSMod)
	{
		case UFREG_SMOD_COMPLEMENT:
		{
			/*
				SOURCE = 1 - SOURCE
			*/
			PINST psInst = AllocateInst(psState, IMG_NULL);

			SetOpcode(psState, psInst, IFADD);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = uTemp;
			psInst->asArg[0] = *psHwSource;
			psInst->u.psFloat->asSrcMod[0].bNegate = IMG_TRUE;
			if (puComponent != NULL)
			{
				psInst->u.psFloat->asSrcMod[0].uComponent = *puComponent;
			}
			psInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
			psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
			AppendInst(psState, psCodeBlock, psInst);
			break;
		}
		case UFREG_SMOD_BIAS:
		{
			PINST psInst;

			/*
				SOURCE = SOURCE - 0.5
			*/
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IFADD);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = uTemp;
			psInst->asArg[0] = *psHwSource;
			if (puComponent != NULL)
			{
				psInst->u.psFloat->asSrcMod[0].uComponent = *puComponent;
			}
			psInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
			psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER2;
			psInst->u.psFloat->asSrcMod[1].bNegate = IMG_TRUE;
			AppendInst(psState, psCodeBlock, psInst);
			break;
		}
		case UFREG_SMOD_SIGNED:
		{
			/*
				SOURCE = 2 * SOURCE - 1
			*/
			PINST psInst = AllocateInst(psState, IMG_NULL);

			SetOpcode(psState, psInst, IFMAD);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = uTemp;
			psInst->asArg[0] = *psHwSource;
			if (puComponent != NULL)
			{
				psInst->u.psFloat->asSrcMod[0].uComponent = *puComponent;
			}
			psInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
			psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2;
			psInst->asArg[2].uType = USEASM_REGTYPE_FPCONSTANT;
			psInst->asArg[2].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
			psInst->u.psFloat->asSrcMod[2].bNegate = IMG_TRUE;
			AppendInst(psState, psCodeBlock, psInst);
			break;
		}
		case UFREG_SMOD_TIMES2:
		{
			/*
				SOURCE = SOURCE * 2
			*/
			PINST psInst = AllocateInst(psState, IMG_NULL);

			SetOpcode(psState, psInst, IFMUL);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = uTemp;
			psInst->asArg[0] = *psHwSource;
			if (puComponent != NULL)
			{
				psInst->u.psFloat->asSrcMod[0].uComponent = *puComponent;
			}
			psInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
			psInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2;
			AppendInst(psState, psCodeBlock, psInst);	
			break;
		}
	}

	if (uSMod != UFREG_SMOD_NONE)
	{
		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber = uTemp;
		psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
		psHwSource->uIndexNumber = USC_UNDEF;
		psHwSource->uIndexArrayOffset = USC_UNDEF;
		psHwSource->eFmt = UF_REGFORMAT_F32;
		if (puComponent != NULL)
		{
			*puComponent = 0;
		}
	}
}

static
IMG_VOID GetMiscSourceF32(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psCodeBlock,
						  PUF_REGISTER			psSource,
						  IMG_UINT32			uSrcChan,
						  IMG_UINT32			uChan,
						  PARG					psHwSource)
/*****************************************************************************
 FUNCTION	: GetMiscSourceF32
    
 PURPOSE	: Convert a source register of UFREG_TYPE_MISC in the input format into the intermediate format.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Code block to insert any extra instructions needed.
			  psSource				- Source register in the input format.
			  uSrcChan				- Source channel which is swizzled in to the loaded channel.
			  uChan					- Channel to load from the source.
			  psHwSource			- Pointer to the structure to fill out with the register in
									  the intermediate format.
			  
 RETURNS	: None.
*****************************************************************************/
{
	switch (psSource->uNum)
	{
		case UF_MISC_FACETYPE:
		case UF_MISC_FACETYPEBIT:
		{		
			PINST		psMoveInst;
			IMG_UINT32	uBackfaceTemp = GetNextRegister(psState);

			/*
				PREDICATE = True if the triangle is backface.
			*/
			CheckFaceType(psState, psCodeBlock, USC_PREDREG_TEMP, IMG_FALSE /* bFrontFace */); 

			/*
				Make the source 1.0f/0xFFFFFFFF if the triangle is frontface
			*/
			psMoveInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMoveInst, IMOV);
			psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psMoveInst->asDest[0].uNumber = uBackfaceTemp;
			psMoveInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
			if (psSource->uNum == UF_MISC_FACETYPE)
			{
				psMoveInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
			}
			else
			{
				psMoveInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_INT32ONE;
			}
			AppendInst(psState, psCodeBlock, psMoveInst);

			/*
				...or -1.0f/00000000 if the triangle is backface.
			*/
			if (psSource->uNum == UF_MISC_FACETYPE)
			{
				psMoveInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMoveInst, IFMOV);
				SetPredicate(psState, psMoveInst, USC_PREDREG_TEMP, IMG_FALSE /* bPredNegate */);
				psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psMoveInst->asDest[0].uNumber = uBackfaceTemp;
				psMoveInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
				psMoveInst->asArg[0].uNumber = uBackfaceTemp;
				psMoveInst->u.psFloat->asSrcMod[0].bNegate = IMG_TRUE;
				AppendInst(psState, psCodeBlock, psMoveInst);
			}
			else
			{
				psMoveInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMoveInst, IMOV);
				SetPredicate(psState, psMoveInst, USC_PREDREG_TEMP, IMG_FALSE /* bPredNegate */);
				psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psMoveInst->asDest[0].uNumber = uBackfaceTemp;
				psMoveInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
				psMoveInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
				AppendInst(psState, psCodeBlock, psMoveInst);
			}

			psHwSource->uType = USEASM_REGTYPE_TEMP;
			psHwSource->uNumber = uBackfaceTemp;
			break;
		}
		case UF_MISC_VPRIM:
		{
			if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY)
			{
				psHwSource->uType = USEASM_REGTYPE_TEMP;
				psHwSource->uNumber = USC_TEMPREG_PRIMITIVE_ID;	
			}
			else
			{
				LoadIteratedValue(psState, psCodeBlock, UFREG_TYPE_MISC, psSource, uSrcChan, uChan, psHwSource);
			}
			break;
		}
		case UF_MISC_RTAIDX:
		{
			PINST psInst;
			
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, ILOADRTAIDX);					
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = GetNextRegister(psState);
			AppendInst(psState, psCodeBlock, psInst);
					
			psHwSource->uType = USEASM_REGTYPE_TEMP;
			psHwSource->uNumber = psInst->asDest[0].uNumber;
			break;
		}
		case UF_MISC_POSITION:
		case UF_MISC_FOG:
		{
			LoadIteratedValue(psState, psCodeBlock, UFREG_TYPE_MISC, psSource, uSrcChan, uChan, psHwSource);
			break;
		}
		case UF_MISC_MSAASAMPLEIDX:
		{
			#if defined(SUPPORT_SGX545)
			if ((psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_SAMPLE_NUMBER_SPECIAL_REGISTER) != 0)
			{
				PINST	psInst;

				psInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psInst, IMOV);
				psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psInst->asDest[0].uNumber = GetNextRegister(psState);
				psInst->asArg[0].uType = USEASM_REGTYPE_GLOBAL;
				psInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_BANK_SAMPLENUMBER;
				AppendInst(psState, psCodeBlock, psInst);
					
				psHwSource->uType = USEASM_REGTYPE_TEMP;
				psHwSource->uNumber = psInst->asDest[0].uNumber;
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				UscFail(psState, UF_ERR_GENERIC, "MSAA sample index isn't supported on this core.\n");
			}
			break;
		}
		default: imgabort();
	}
}

IMG_INTERNAL 
IMG_VOID GetSourceF32(PINTERMEDIATE_STATE psState, 
					  PCODEBLOCK psCodeBlock, 
					  PUF_REGISTER psSource, 
					  IMG_UINT32 uChan, 
					  PARG psHwSource, 
					  IMG_BOOL bAllowSourceMod,
					  PFLOAT_SOURCE_MODIFIER psSourceMod)
/*****************************************************************************
 FUNCTION	: GetSourceF32
    
 PURPOSE	: Convert a source register in the input format into the intermediate format.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Code block to insert any extra instructions needed.
			  psSource				- Source register in the input format.
			  dwSrc					- The number of the source.
			  dwChan				- Channel in the destination.
			  psHwSource			- Pointer to the structure to fill out with the register in
									  the intermediate format.
			  bAllowSourceMod		- IMG_TRUE if the instruction can allow negate and absolute modifiers on the source.
			  psSourceMod			- Returns the source modifiers.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 uSMod = (psSource->byMod & UFREG_SMOD_MASK) >> UFREG_SMOD_SHIFT;
	IMG_UINT32 uSrcChan = EXTRACT_CHAN(psSource->u.uSwiz, uChan);
	IMG_BOOL   bNegate = IMG_FALSE;

	InitInstArg(psHwSource);

	if (uSrcChan >= UFREG_SWIZ_1)
	{
		/*
			Convert a swizzle of a constant value into a channel with a hardware constant.
		*/
		psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
		switch (uSrcChan)
		{
			case UFREG_SWIZ_1: psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1; break;
			case UFREG_SWIZ_2: psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2; break;
			case UFREG_SWIZ_HALF: psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER2; break;
			case UFREG_SWIZ_0: psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO; break;
		}
	}
	else if (GetRegisterFormat(psState, psSource) == UF_REGFORMAT_F16)
	{
		GetSingleSourceConvertToF32(psState, psCodeBlock, psSource, psHwSource, uChan, bAllowSourceMod, psSourceMod);
		return;
	}
	else
	{
		switch (psSource->eType)
		{
			case UFREG_TYPE_TEMP:
			{
				ASSERT(psSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE);

				psHwSource->uType = USEASM_REGTYPE_TEMP;
				psHwSource->uNumber = ConvertTempRegisterNumberF32(psState, psSource->uNum, uSrcChan);
				break;
			}
			case UFREG_TYPE_PSOUTPUT:
			{
				GetPSOutputSource(psState, psHwSource, psSource, uSrcChan);
				break;
			}
			case UFREG_TYPE_MISC:
			{
				GetMiscSourceF32(psState, psCodeBlock, psSource, uSrcChan, uChan, psHwSource);
				break;
			}
			case UFREG_TYPE_COL:
			case UFREG_TYPE_TEXCOORD:
			case UFREG_TYPE_TEXCOORDP:
			case UFREG_TYPE_TEXCOORDPIFTC:
			{
				UF_REGTYPE eType = psSource->eType;
				if (eType == UFREG_TYPE_TEXCOORDPIFTC)
				{
					ASSERT(psSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE);
					if (GetBit(psState->psSAOffsets->auProjectedCoordinateMask, psSource->uNum))
					{
						eType = UFREG_TYPE_TEXCOORDP;
					}
					else
					{
						eType = UFREG_TYPE_TEXCOORD;
					}
				}
				LoadIteratedValue(psState, psCodeBlock, eType, psSource, uSrcChan, uChan, psHwSource);
				break;
			}
			case UFREG_TYPE_HW_CONST:
			{
				psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
				switch (psSource->uNum)
				{
					case HW_CONST_1: psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1; break;
					case HW_CONST_2: psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2; break;
					case HW_CONST_HALF: psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER2; break;
					case HW_CONST_0: psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO; break;
				}
				break;
			}

			case UFREG_TYPE_IMMEDIATE:
			{
				psHwSource->uType = USEASM_REGTYPE_IMMEDIATE;
				psHwSource->uNumber = psSource->uNum;
				break;
			}

			case UFREG_TYPE_CONST:
			{
				LoadConstant(psState, 
							 psCodeBlock, 
							 psSource, 
							 uSrcChan, 
							 psHwSource, 
							 NULL,
							 bAllowSourceMod, 
							 &bNegate, 
							 UF_REGFORMAT_F32,
							 IMG_FALSE /* bC10Subcomponent */,
							 0);				
				break;
			}
			case UFREG_TYPE_VSINPUT:
			{
				IMG_UINT32			uInputOffset;
				PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;
#ifdef NON_F32_VERTEX_INPUT
				/*
					Check vs input has been created correctly
				*/
				PFIXED_REG_DATA		psVSInputs = psVS->aVertexShaderInputsFixedRegs[0];
				ASSERT(psVS->uVertexShaderInputCount == 1);
				ASSERT(psVSInputs->sPReg.uType == USEASM_REGTYPE_PRIMATTR);
				ASSERT(psVSInputs->uConsecutiveRegsCount == EURASIA_USE_PRIMATTR_BANK_SIZE); 
#else
				PFIXED_REG_DATA		psVSInputs = psVS->psVertexShaderInputsFixedReg;
#endif
				ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX);
				uInputOffset = psSource->uNum * CHANS_PER_REGISTER + uSrcChan;
				if (psVSInputs->uRegArrayIdx == USC_UNDEF)
				{
					ASSERT(uInputOffset < psVSInputs->uConsecutiveRegsCount);

					psHwSource->uType = USEASM_REGTYPE_TEMP; 
					psHwSource->uNumber = psVSInputs->auVRegNum[uInputOffset];
				}
				else
				{
					psHwSource->uType = USC_REGTYPE_REGARRAY;
					psHwSource->uNumber = psVSInputs->uRegArrayIdx;
					psHwSource->uArrayOffset = uInputOffset;
					GetRelativeIndex(psState, psSource, psHwSource);
				}
				break;
			}
			case UFREG_TYPE_VSOUTPUT:
			{
				GetVSOutputSource(psState, psHwSource, psSource, uSrcChan, psCodeBlock);
				break;
			}
			case UFREG_TYPE_INDEXABLETEMP:
			{
				IMG_UINT32	uTemp = GetNextRegister(psState);

				LoadStoreIndexableTemp(psState, 
									   psCodeBlock, 
									   IMG_TRUE, 
									   psSource->eFormat, 
									   psSource, 
									   (1U << uSrcChan), 
									   uTemp, 
									   0);

				psHwSource->uType = USEASM_REGTYPE_TEMP;
				psHwSource->uNumber = uTemp;
				break;
			}
			default: imgabort();
		}
	}

	/*
		Apply DirectX source modifiers which aren't supported directly by the hardware.
	*/
	ApplySourceModF32(psState, psCodeBlock, psHwSource, NULL /* puComponent */, uSMod);

	/*
		Apply the negate and absolute modifiers directly in the instruction if it supports
		them.
	*/
	if (bAllowSourceMod)
	{
		psSourceMod->uComponent = 0;
		if (psSource->byMod & UFREG_SOURCE_ABS) 
		{
			psSourceMod->bAbsolute = IMG_TRUE;
			bNegate = IMG_FALSE;
		}
		else
		{
			psSourceMod->bAbsolute = IMG_FALSE;
		}	
		if (psSource->byMod & UFREG_SOURCE_NEGATE) 
		{
			psSourceMod->bNegate = bNegate ? IMG_FALSE : IMG_TRUE;
		}
		else
		{
			psSourceMod->bNegate = bNegate ? IMG_TRUE : IMG_FALSE;
		}
	}
}

IMG_INTERNAL 
IMG_BOOL DestChanOverlapsSrc(PINTERMEDIATE_STATE psState,
							 PUNIFLEX_INST psSrc, 
							 IMG_UINT32 uDestChan)
/*****************************************************************************
 FUNCTION	: DestChanOverlapsSrc
    
 PURPOSE	: Checks if a channel in the destination overlaps one of the channels
			  used from one of the sources.

 PARAMETERS	: psState	- Internal compiler state
			  psSrc		- Instruction to check.
			  uDestChan	- Channel to check.
			  
 RETURNS	: IMG_TRUE or IMG_FALSE.
*****************************************************************************/
{
	IMG_UINT32 uArg, uChan;

	for (uArg = 0; uArg < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; uArg++)
	{
		if (psSrc->sDest.eType == psSrc->asSrc[uArg].eType &&
			psSrc->sDest.uNum == psSrc->asSrc[uArg].uNum)
		{
			IMG_UINT32 uMask;

			switch (psSrc->eOpCode)
			{
				case UFOP_DOT3:	uMask = 0x7; break;
				case UFOP_DOT4: 
				case UFOP_DOT4_CP: uMask = 0xf; break;
				case UFOP_DOT2ADD: uMask = (uArg != 2) ? 0x3 : 0x4; break;
				case UFOP_MUL:
				case UFOP_ADD:
				case UFOP_MAD: 
				case UFOP_MOV:
				case UFOP_RSQRT:
				case UFOP_RECIP:
				case UFOP_SQRT:
				case UFOP_LOG:
				case UFOP_EXP:
				case UFOP_MOV_CP:
				{
					uMask = psSrc->sDest.u.byMask;
					break;
				}
				case UFOP_SINCOS2:
				{
					uMask = psSrc->sDest.u.byMask | psSrc->sDest2.u.byMask;
					break;
				}
				case UFOP_SINCOS: uMask = 0x8; break;
				default: imgabort();
			}

			for (uChan = 0; uChan < 4; uChan++)
			{
				if (uMask & (1U << uChan))
				{
					IMG_UINT32 uSrcChan = (psSrc->asSrc[uArg].u.uSwiz >> (uChan * 3)) & 0x7;
					if (uSrcChan == uDestChan)
					{
						return IMG_TRUE;
					}
				}
			}
		}
	}
	return IMG_FALSE;
}

static IMG_VOID MoveF32VectorToInputDest(PINTERMEDIATE_STATE	psState,
										 PCODEBLOCK				psCodeBlock,
										 PUNIFLEX_INST			psInputInst,
										 IMG_UINT32				uSwizzle,
										 IMG_UINT32				uVecStartReg)
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (psInputInst->sDest.u.byMask & (1U << uChan))
		{
			PINST		psMoveInst;
			IMG_UINT32	uSrcChan;

			uSrcChan = EXTRACT_CHAN(uSwizzle, uChan);
			ASSERT(uSrcChan >= UFREG_SWIZ_X && uSrcChan <= UFREG_SWIZ_W);

			psMoveInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMoveInst, IMOV);
			GetDestinationF32(psState, psCodeBlock, &psInputInst->sDest, uChan, &psMoveInst->asDest[0]);
			psMoveInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psMoveInst->asArg[0].uNumber = uVecStartReg + uSrcChan;
			GetInputPredicateInst(psState, psMoveInst, psInputInst->uPredicate, uChan);
			AppendInst(psState, psCodeBlock, psMoveInst);
		}
	}
}

static IMG_VOID ConvertDotProductInstructionF32(PINTERMEDIATE_STATE psState, 
												PCODEBLOCK psCodeBlock, 
												PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertDotProductInstruction
    
 PURPOSE	: Convert an input dot2add/dot3/dot4 instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 uChanCount;
	IMG_UINT32 uDestChan = UINT_MAX;
	IMG_UINT32 i;
	PINST psInst = NULL;
	IMG_UINT32 uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32 uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_BOOL bTempDest = IMG_FALSE;
	IMG_UINT32 uPredSrc = USC_PREDREG_NONE;
	IMG_BOOL bPredNegate = IMG_FALSE;
	IMG_UINT32 uDotProductTemp = GetNextRegister(psState);

	if (psSrc->sDest.u.byMask == 0)
	{
		return;
	}

	if ((psSrc->uPredicate & UF_PRED_COMP_MASK) == UF_PRED_XYZW)
	{
		bTempDest = IMG_TRUE;
	}
	else
	{
		for (i = 0; i < 4; i++)
		{
			if ((psSrc->sDest.u.byMask & (1U << i)) != 0 && !DestChanOverlapsSrc(psState, psSrc, i))
			{
				uDestChan = i;
				break;
			}
		}	
		if (i == 4)
		{
			bTempDest = IMG_TRUE;
		}
	}

	switch (psSrc->eOpCode)
	{
		case UFOP_DOT2ADD: uChanCount = 2; break;
		case UFOP_DOT3: uChanCount = 3; break;
		case UFOP_DOT4: 
		case UFOP_DOT4_CP: uChanCount = 4; break;
		default: imgabort(); return;
	}

	/* 
	   Emit dot product.
	   DOT2ADD d, s0, s0, s2  
	   --> fmul x, s0, s0; fmadd x, s1, s1, x; fadd d, x, s2
	   DOT3 d, s0, s0  
	   --> fmul x, s0, s0; fmadd x, s1, s1, x; fmadd d, s2, s2, x; 
	   DOT4 d, s0, s0  
	   --> fmul x, s0, s0; fmadd x, s1, s1, x; fmadd x, s2, s2, x; fmadd d, s3, s3, x; 
	*/
	for (i = 0; i < uChanCount; i++)
	{
		IMG_UINT32 uSrcChan = i;
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, (i != 0) ? IFMAD : IFMUL);
		if (!bTempDest && i == (uChanCount - 1) && psSrc->eOpCode != UFOP_DOT4_CP)
		{
			GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, uDestChan, &psInst->asDest[0]);

			/* Apply any predicate. */
			GetInputPredicate(psState, &uPredSrc, &bPredNegate, psSrc->uPredicate, uDestChan);
			SetPredicate(psState, psInst, uPredSrc, bPredNegate);
		}
		else
		{
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = uDotProductTemp;
		}
		GetSourceF32(psState, 
					 psCodeBlock, 
					 &psSrc->asSrc[0], 
					 uSrcChan, 
					 &psInst->asArg[0], 
					 IMG_TRUE,
					 &psInst->u.psFloat->asSrcMod[0]);
		GetSourceF32(psState, 
					 psCodeBlock, 
					 &psSrc->asSrc[1], 
					 uSrcChan, 
					 &psInst->asArg[1], 
					 IMG_TRUE,
					 &psInst->u.psFloat->asSrcMod[1]);
		if (i > 0)
		{
			psInst->asArg[2].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[2].uNumber = uDotProductTemp;
		}
		AppendInst(psState, psCodeBlock, psInst);
	}
	/* dp2add has an extra add */
	if (psSrc->eOpCode == UFOP_DOT2ADD)
	{
		PINST psAddInst;
		psAddInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psAddInst, IFADD);
		SetPredicate(psState, psAddInst, uPredSrc, bPredNegate);
		psAddInst->asDest[0] = psInst->asDest[0];
		GetSourceF32(psState, 
					 psCodeBlock, 
					 &psSrc->asSrc[2], 
					 USC_Z_CHAN, 
					 &psAddInst->asArg[0], 
					 IMG_TRUE,
					 &psAddInst->u.psFloat->asSrcMod[0]);
		psAddInst->asArg[1] = psInst->asDest[0];
		AppendInst(psState, psCodeBlock, psAddInst);
	}

	/* Do any destination modifiers. */
	GenerateDestModF32(psState, psCodeBlock, &psInst->asDest[0], uSat, uScale, uPredSrc, bPredNegate);

	/* Output a clipplane result. */
	if (psSrc->eOpCode == UFOP_DOT4_CP)
	{
		IMG_UINT32	uClipplane = psSrc->asSrc[2].uNum;
		PINST		psDPCInst;
		PINST		psMovInst;
		IMG_UINT32	uDPCSrcTemp = GetNextRegister(psState);

		ASSERT(psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT);

		/*
			TEMP = 0
		*/
		psMovInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMovInst, IMOV);
		psMovInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMovInst->asDest[0].uNumber = uDPCSrcTemp;
		psMovInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
		psMovInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
		AppendInst(psState, psCodeBlock, psMovInst);

		/*
			DPC		OUT, OUT, TEMP
		*/
		psDPCInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psDPCInst, IFDPC);
		GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, uDestChan, &psDPCInst->asDest[0]);
		GetInputPredicateInst(psState, psInst, psSrc->uPredicate, uDestChan);
		psDPCInst->asDest[0] = psInst->asDest[0];
		psDPCInst->u.psDpc->uClipplane = uClipplane;
		psDPCInst->asArg[0] = psInst->asDest[0];
		psDPCInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
		psDPCInst->asArg[1].uNumber = uDPCSrcTemp;
		AppendInst(psState, psCodeBlock, psDPCInst);
	}

	/*
		Broadcast the result of the other channels.
	*/
	for (i = 0; i < 4; i++)
	{
		if ((psSrc->sDest.u.byMask & (1U << i)) != 0 && (bTempDest || i != uDestChan || psSrc->eOpCode == UFOP_DOT4_CP))
		{
			PINST psMoveInst;
			psMoveInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMoveInst, IMOV);
			GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, i, &psMoveInst->asDest[0]);
			/* Apply any predicate. */
			GetInputPredicateInst(psState, psMoveInst, psSrc->uPredicate, i);
			psMoveInst->asArg[0] = psInst->asDest[0];
			AppendInst(psState, psCodeBlock, psMoveInst);
		}
	}

	/* Store into a destination register that needs special handling. */
	StoreIntoSpecialDest(psState, psCodeBlock, psSrc, &psSrc->sDest);
}

IMG_INTERNAL
IMG_VOID ConvertBitwiseInstructionF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertBitwiseInstructionF32
    
 PURPOSE	: Convert an input bitwise instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IOPCODE eHwOpcode;
	PINST psInst;
	IMG_UINT32 uTempDest = 0;
	IMG_UINT32 puRepDest[4] = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
	IMG_UINT32 auTempDestNum[4] = {USC_UNDEF, USC_UNDEF, USC_UNDEF, USC_UNDEF};
	IMG_UINT32 i, j, k;
	/* When enabled, we need to release the lock at the end of the function */
	IMG_BOOL bNeedsRelease = IMG_FALSE;
	ARG sAtomicNew, sAddress;

	ASSERT(psSrc->sDest.byMod == 0);

	/* Select the appropiate hardware opcode. */
	switch (psSrc->eOpCode)
	{
		case UFOP_ATOM_AND:
			GenerateAtomicLock(psState, psCodeBlock);
			bNeedsRelease = IMG_TRUE;
		case UFOP_AND: eHwOpcode = IAND; break;
		case UFOP_SHL: eHwOpcode = ISHL; break;
		case UFOP_ASR: eHwOpcode = IASR; break;
		case UFOP_NOT: eHwOpcode = INOT; break;
		case UFOP_ATOM_OR:
			GenerateAtomicLock(psState, psCodeBlock);
			bNeedsRelease = IMG_TRUE;
		case UFOP_OR: eHwOpcode = IOR; break;
		case UFOP_SHR: eHwOpcode = ISHR; break;
		case UFOP_ATOM_XOR:
			GenerateAtomicLock(psState, psCodeBlock);
			bNeedsRelease = IMG_TRUE;
		case UFOP_XOR: eHwOpcode = IXOR; break;
		default: imgabort();
	}

	for (i = 0; i < 4; i++)
	{
		/* Ignore components that aren't in the instruction mask. */
		if ((psSrc->sDest.u.byMask & (1U << i)) == 0)
		{
			continue;
		}
		/* Ignore this component if it is the same as an earlier one. */
		if (puRepDest[i] != UINT_MAX)
		{
			continue;
		}

		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, eHwOpcode);

		/* Get the values of the instruction sources. */
		for (j = 0; j < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; j++)
		{
			ASSERT(psSrc->asSrc[j].byMod == 0);
			ASSERT(j < g_psInstDesc[INVALID_MAXARG].uDefaultArgumentCount);
			GetSourceTypeless(psState, 
							  psCodeBlock, 
							  &psSrc->asSrc[j], 
							  i, 
							  &psInst->asArg[j], 
							  IMG_FALSE,
							  NULL);
		}

		for (j = (i + 1); j < 4; j++)
		{
			if ((psSrc->sDest.u.byMask & (1U << j)) != 0 && puRepDest[j] == UINT_MAX)
			{
				/* 
				   Check if the result for this component should be replicated to another component.
				*/
				if ((psSrc->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_XYZW)
				{
					puRepDest[j] = i;
					for (k = 0; k < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; k++)
					{
						if (EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, i) != EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, j))
						{
							puRepDest[j] = UINT_MAX;
						}
					}
				}
				else
				{
					puRepDest[j] = UINT_MAX;
				}
				/* Check if we are writing a component which is used later in the instruction. */
					if (puRepDest[j] == UINT_MAX)
					{
					for (k = 0; k < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; k++)
					{
						if (psSrc->sDest.eType == psSrc->asSrc[k].eType &&
							psSrc->sDest.uNum == psSrc->asSrc[k].uNum &&
							i == EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, j))
						{
							uTempDest |= (1U << i);
						}
					}
				}
			}
		}

		if (uTempDest & (1U << i))
		{
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = auTempDestNum[i] = GetNextRegister(psState);
		}
		else
		{
			GetInputPredicateInst(psState, psInst, psSrc->uPredicate, i);
			GetDestinationTypeless(psState, psCodeBlock, &psSrc->sDest, i, &psInst->asDest[0]);
		}

		/* Atomic functions require loading one of the arguments */
		if (bNeedsRelease)
		{
			/* Generate a temporary register */
			InitInstArg(&sAtomicNew);
			sAtomicNew.uType = USEASM_REGTYPE_TEMP;
			sAtomicNew.uNumber = GetNextRegister(psState);
			sAtomicNew.eFmt = psInst->asDest[0].eFmt;
			/* Remember to store the old result later */
			sAddress = psInst->asArg[0];
			GenerateAtomicLoad(psState, psCodeBlock, &sAddress, &(psInst->asDest[0]));
			/* Switch the registers so that the operation uses the loaded value
			 and make sure we return the loaded value, rather than the one calculated */
			psInst->asArg[0] = psInst->asDest[0];
			psInst->asDest[0] = sAtomicNew;
		}

		/* Insert the new instruction in the current block. */
		AppendInst(psState, psCodeBlock, psInst);

		if (bNeedsRelease)
			GenerateAtomicStore(psState, psCodeBlock, &sAddress, &(psInst->asDest[0]));
	}

	for (i = 0; i < 4; i++)
	{
		if ((uTempDest & (1U << i)) != 0 || puRepDest[i] != UINT_MAX)
		{
			PINST psMoveInst;

			psMoveInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMoveInst, IMOV);
			GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, i, &psMoveInst->asDest[0]);
			if (puRepDest[i] != UINT_MAX)
			{
				GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, puRepDest[i], &psMoveInst->asArg[0]);
			}
			else
			{
				psMoveInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
				psMoveInst->asArg[0].uNumber = auTempDestNum[i];
			}

			GetInputPredicateInst(psState, psMoveInst, psSrc->uPredicate, i);

			AppendInst(psState, psCodeBlock, psMoveInst);
		}
	}

	/* Store into a destination register that needs special handling. */
	StoreIntoSpecialDest(psState, psCodeBlock, psSrc, &psSrc->sDest);

	if (bNeedsRelease)
		GenerateAtomicRelease(psState, psCodeBlock);
}

IMG_INTERNAL
IMG_VOID ConvertVertexInputInstructionTypeless(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertVertexInputInstructionTypeless

 PURPOSE	: Converts vertex input instruction to intermediate form.

 PARAMETERS	: psState			- Compiler state.              
              psCodeBlock		- Resulting intermediate code is appended here.
              psSrc				- points to input Vertex Input instruction.	  

 RETURNS    : NONE.
 
 OUTPUT		: NONE.
*****************************************************************************/
{
	IMG_UINT32 i;	
	IMG_UINT32 uIndexRegNumber;
	IMG_UINT32 uStaticIndex;

	ConvertVertexInputInstructionCore(psState, psCodeBlock, psSrc, psSrc->uPredicate, &uIndexRegNumber, &uStaticIndex);

	for (i = 0; i < 4; i++)
	{	
		PINST psInVertLoadInstr;

		/* Ignore components that aren't in the instruction mask. */
		if ((psSrc->sDest.u.byMask & (1U << i)) == 0)
		{
			continue;
		}

		psInVertLoadInstr = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInVertLoadInstr, IMOV);
		GetInputPredicateInst(psState, psInVertLoadInstr, psSrc->uPredicate, i);
		GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, i, &psInVertLoadInstr->asDest[0]);
		psInVertLoadInstr->asArg[0].uType = USC_REGTYPE_GSINPUT;
		psInVertLoadInstr->asArg[0].uNumber = i + uStaticIndex;
		psInVertLoadInstr->asArg[0].uIndexType = USEASM_REGTYPE_TEMP;
		psInVertLoadInstr->asArg[0].uIndexNumber = uIndexRegNumber;
		psInVertLoadInstr->asArg[0].uIndexArrayOffset = 0;
		psInVertLoadInstr->asArg[0].uIndexStrideInBytes = LONG_SIZE;
		AppendInst(psState, psCodeBlock, psInVertLoadInstr);
	}
	return;
}

static IMG_VOID CopySourceModifiers(PINTERMEDIATE_STATE		psState,
									PCODEBLOCK				psCodeBlock,
									PINST					psInst,
									FLOAT_SOURCE_MODIFIER	asSrcMod[])
/*****************************************************************************
 FUNCTION	: CopySourceModifiers
    
 PURPOSE	: Copy input source modifiers to an intermediate instruction.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInst			- Intermediate instruction.
			  asSrcMod			- Source modifiers to copy.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	if (g_psInstDesc[psInst->eOpcode].eType == INST_TYPE_FLOAT)
	{
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			/*
				Check for an instruction which doesn't support negation on some sources.
			*/
			if (!CanHaveSourceModifier(psState, 
									   psInst, 
									   uArg, 
									   asSrcMod[uArg].bNegate,
									   asSrcMod[uArg].bAbsolute))
			{
				/*
					Check if we can swap the sources to avoid the restriction.
				*/
				if (
						uArg == 0 &&
						InstSource01Commute(psState, psInst) &&
						CanHaveSourceModifier(psState, psInst, 0, asSrcMod[1].bNegate, asSrcMod[1].bAbsolute) &&
						CanUseSrc(psState, psInst,0, psInst->asArg[1].uType, psInst->asArg[1].uIndexType)
				   )
				{
					/*
						Swap the instruction sources.
					*/
					CommuteSrc01(psState, psInst);
					psInst->u.psFloat->asSrcMod[0] = asSrcMod[1];
					asSrcMod[1] = asSrcMod[0];
				}
				else
				{
					/*
						Apply the source modifier in a separate instruction and
						replace the source by the result.
					*/
					ApplyFloatSourceModifier(psState, 
											 psCodeBlock, 
											 NULL /* psInsertBeforeInst */,
											 &psInst->asArg[uArg], 
											 &psInst->asArg[uArg],
											 UF_REGFORMAT_F32 /* eSrcFmt */,
											 &asSrcMod[uArg]);
				}
			}
			else
			{
				/*
					Apply the source modifier directly in the instruction.
				*/
				psInst->u.psFloat->asSrcMod[uArg] = asSrcMod[uArg];
			}
		}
	}
	else
	{	
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			/*
				Apply any source modifier in a separate instruction and
				replace the source by the result.
			*/
			ApplyFloatSourceModifier(psState, 
									 psCodeBlock, 
									 NULL /* psInsertBeforeInst */,
									 &psInst->asArg[uArg], 
									 &psInst->asArg[uArg],
									 UF_REGFORMAT_F32 /* eSrcFmt */,
									 &asSrcMod[uArg]);
		}
	}
}

static IMG_VOID ConvertVectorInstructionF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertVectorInstruction
    
 PURPOSE	: Convert an input vector instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IOPCODE eHwOpcode;
	PINST psInst;
	IMG_UINT32 uTempDest = 0;
	IMG_UINT32 puRepDest[4] = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
	IMG_UINT32 auTempDestNum[4] = {USC_UNDEF, USC_UNDEF, USC_UNDEF, USC_UNDEF};
	IMG_UINT32 i, j, k;
	IMG_UINT32 uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32 uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32 uNumInputSrcArgs;

	/* Select the appropiate hardware opcode. */
	switch (psSrc->eOpCode)
	{
		case UFOP_ADD: eHwOpcode = IFADD; break;  
		case UFOP_MUL: eHwOpcode = IFMUL; break; 
		case UFOP_MAD: eHwOpcode = IFMAD; break; 
		case UFOP_FRC: eHwOpcode = IFSUBFLR; break; 
		case UFOP_DSX: eHwOpcode = IFDSX; break; 
		case UFOP_DSY: eHwOpcode = IFDSY; break; 
		case UFOP_MIN: eHwOpcode = IFMIN; break; 
		case UFOP_MAX: eHwOpcode = IFMAX; break; 
		case UFOP_MOV: eHwOpcode = IFMOV; break;
		case UFOP_RECIP: eHwOpcode = IFRCP; break;
		case UFOP_RSQRT: eHwOpcode = IFRSQ; break;
		case UFOP_EXP: eHwOpcode = IFEXP; break;
		case UFOP_LOG: eHwOpcode = IFLOG; break;
		case UFOP_MOV_CP: eHwOpcode = IFMOV; break;
		case UFOP_CEIL: eHwOpcode = IFSUBFLR; break;
		case UFOP_TRC: eHwOpcode = IFTRC; break;
		#if defined(SUPPORT_SGX545)
		case UFOP_SQRT: eHwOpcode = IFSQRT; break;
		case UFOP_RNE: eHwOpcode = IRNE; break;
		case UFOP_RND: eHwOpcode = IRNE; break;
		#endif /* defined(SUPPORT_SGX545) */
		case UFOP_SETBGE:
		case UFOP_SETBNE:
		case UFOP_SETBEQ:
		case UFOP_SETBLT:
		{
			eHwOpcode = ITESTMASK;
			break;
		}
		default: imgabort();
	}

	/*
		Get the number of arguments to copy from the input instruction
		to the intermediate instruction.
	*/
	if (psSrc->eOpCode != UFOP_MOV_CP)
	{
		uNumInputSrcArgs = g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs;
	}
	else
	{
		uNumInputSrcArgs = 1;
	}

	for (i = 0; i < 4; i++)
	{
		FLOAT_SOURCE_MODIFIER	asSrcMod[UNIFLEX_MAXIMUM_SOURCE_ARGUMENT_COUNT];	
		IMG_UINT32				uPredSrc = USC_PREDREG_NONE;
		IMG_BOOL				bPredNegate = IMG_FALSE;

		/* Ignore components that aren't in the instruction mask. */
		if ((psSrc->sDest.u.byMask & (1U << i)) == 0)
		{
			continue;
		}
		/* Ignore this component if it is the same as an earlier one. */
		if (puRepDest[i] != UINT_MAX)
		{
			continue;
		}

		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, eHwOpcode);

		/* Get the values of the instruction sources. */
		for (j = 0; j < uNumInputSrcArgs; j++)
		{
			ASSERT(j < g_psInstDesc[INVALID_MAXARG].uDefaultArgumentCount);
			GetSourceF32(psState, 
						 psCodeBlock, 
						 &psSrc->asSrc[j], 
						 i, 
						 &psInst->asArg[j], 
						 IMG_TRUE, 
						 &asSrcMod[j]);
		}

		/*
			Duplicate the sources for these instructions.
		*/
		if (psSrc->eOpCode == UFOP_DSX || 
			psSrc->eOpCode == UFOP_DSY ||
			psSrc->eOpCode == UFOP_FRC)
		{
			psInst->asArg[1] = psInst->asArg[0];
			asSrcMod[1] = asSrcMod[0];
		}

		/*
			Convert CEIL(X) -> -FLR(-X)
		*/
		if (psSrc->eOpCode == UFOP_CEIL)
		{
			psInst->asArg[1] = psInst->asArg[0];
			asSrcMod[1] = asSrcMod[0];
			asSrcMod[1].bNegate = !asSrcMod[1].bNegate;

			InitInstArg(&psInst->asArg[0]);
			psInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
			psInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
			InitFloatSrcMod(&asSrcMod[0]);
			
		}

		/*
			Set up the test parameters for a SETBxx instruction.
		*/
		if (psSrc->eOpCode == UFOP_SETBGE ||
			psSrc->eOpCode == UFOP_SETBNE ||
			psSrc->eOpCode == UFOP_SETBEQ ||
			psSrc->eOpCode == UFOP_SETBLT)
		{
			/*
				Generate ALU_RESULT = SOURCE1 - SOURCE2
			*/
			psInst->u.psTest->eAluOpcode = IFSUB;

			/*
				Test the ALU result against zero and produce 0xFFFFFFFF if the test passes or
				0x00000000 if the test fails.
			*/
			switch (psSrc->eOpCode)
			{
				case UFOP_SETBEQ: psInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO; break;
				case UFOP_SETBNE: psInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO; break;
				case UFOP_SETBGE: psInst->u.psTest->sTest.eType = TEST_TYPE_GTE_ZERO; break;
				case UFOP_SETBLT: psInst->u.psTest->sTest.eType = TEST_TYPE_LT_ZERO; break;
				default: imgabort();
			}
			psInst->u.psTest->sTest.eMaskType = USEASM_TEST_MASK_DWORD;
		}

		/* Override the opcode if the source to a move doesn't have a modifier. */
		if (eHwOpcode == IFMOV && 
			!asSrcMod[0].bAbsolute &&
			!asSrcMod[0].bNegate)
		{
			SetOpcode(psState, psInst, IMOV);
		}

		/*
			Copy any input source modifiers to the intermediate instruction.
		*/
		CopySourceModifiers(psState, psCodeBlock, psInst, asSrcMod);

		for (j = (i + 1); j < 4; j++)
		{
			if ((psSrc->sDest.u.byMask & (1U << j)) != 0 && puRepDest[j] == UINT_MAX)
			{
				/* 
				   Check if the result for this component should be replicated to another component.
				*/
				if ((psSrc->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_XYZW)
				{
					puRepDest[j] = i;
					for (k = 0; k < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; k++)
					{
						if (EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, i) != EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, j))
						{
							puRepDest[j] = UINT_MAX;
						}
					}
				}
				else
				{
					puRepDest[j] = UINT_MAX;
				}
				/* Check if we are writing a component which is used later in the instruction. */
				if (puRepDest[j] == UINT_MAX)
				{
					for (k = 0; k < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; k++)
					{
						if (psSrc->sDest.eType == psSrc->asSrc[k].eType &&
							psSrc->sDest.uNum == psSrc->asSrc[k].uNum &&
							i == EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, j))
						{
							uTempDest |= (1U << i);
						}
					}
				}
			}
		}

		if (uTempDest & (1U << i))
		{
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = auTempDestNum[i] = GetNextRegister(psState);
		}
		else
		{
			GetInputPredicate(psState, &uPredSrc, &bPredNegate, psSrc->uPredicate, i);
			SetPredicate(psState, psInst, uPredSrc, bPredNegate);
			GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, i, &psInst->asDest[0]);
		}

		/* Insert the new instruction in the current block. */
		AppendInst(psState, psCodeBlock, psInst);

		/* Do any destination modifiers. */
		GenerateDestModF32(psState, psCodeBlock, &psInst->asDest[0], uSat, uScale, uPredSrc, bPredNegate);

		/* Output a clipplane result. */
		if (i == 0 && psSrc->eOpCode == UFOP_MOV_CP)
		{
			IMG_UINT32	uClipplane;
			PINST		psDPCInst;
			PINST		psMovInst;
			IMG_UINT32	uTempSrc = GetNextRegister(psState);

			ASSERT(psSrc->asSrc[1].eType == UFREG_TYPE_CLIPPLANE);
			uClipplane = psSrc->asSrc[1].uNum;

			ASSERT(psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT);

			/*
				TEMP = 0
			*/
			psMovInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMovInst, IMOV);
			psMovInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psMovInst->asDest[0].uNumber = uTempSrc;
			psMovInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
			psMovInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
			AppendInst(psState, psCodeBlock, psMovInst);

			/*	
				DPC		OUT, OUT, TEMP

				The DPC instruction has two sources: one of which is the
				clipplane result to test and the other contains constant
				zero.
			*/
			psDPCInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psDPCInst, IFDPC);
			psDPCInst->asDest[0] = psInst->asDest[0];
			psDPCInst->u.psDpc->uClipplane = uClipplane;
			psDPCInst->asArg[0] = psInst->asDest[0];
			psDPCInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
			psDPCInst->asArg[1].uNumber = uTempSrc;
			AppendInst(psState, psCodeBlock, psDPCInst);
		}
	}

	for (i = 0; i < 4; i++)
	{
		if ((uTempDest & (1U << i)) != 0 || puRepDest[i] != UINT_MAX)
		{
			PINST psMoveInst;

			psMoveInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMoveInst, IMOV);
			GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, i, &psMoveInst->asDest[0]);
			if (puRepDest[i] != UINT_MAX)
			{
				GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, puRepDest[i], &psMoveInst->asArg[0]);
			}
			else
			{
				psMoveInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
				psMoveInst->asArg[0].uNumber = auTempDestNum[i];
			}

			GetInputPredicateInst(psState, psMoveInst, psSrc->uPredicate, i);

			AppendInst(psState, psCodeBlock, psMoveInst);
		}
	}

	/* Store into a destination register that needs special handling. */
	StoreIntoSpecialDest(psState, psCodeBlock, psSrc, &psSrc->sDest);
}

static
IMG_VOID CreateComparisonF32(PINTERMEDIATE_STATE psState, 
						     PCODEBLOCK psCodeBlock, 
							 IMG_UINT32 uPredDest,
							 UFREG_COMPOP uCompOp, 
							 PARG psSrc1,
							 PFLOAT_SOURCE_MODIFIER psSrc1Mod,
							 PARG psSrc2, 
							 PFLOAT_SOURCE_MODIFIER psSrc2Mod,
							 IMG_UINT32 uCompPredSrc,
							 IMG_BOOL bCompPredNegate,
							 IMG_BOOL bInvert)
/*****************************************************************************
 FUNCTION	: CreateComparisonF32
    
 PURPOSE	: Generate instructions to do a comparison between two registers and 
			  put the result in USE register p0.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uCompOp			- Comparison type.
			  psSrc1			- First source to be compared.
			  psSrc1Mod			- Source modifiers on the first source.
			  psSrc2			- Second source to be compared.
			  psSrc2Mod			- Source modifiers on the second source.
			  dwChan			- Channel within the sources to be compared.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psInst;
	IMG_UINT32	uSrc1Component = psSrc1Mod->uComponent;
	IMG_UINT32	uSrc2Component = psSrc2Mod->uComponent;
	
	if (bInvert)
	{
		uCompOp = g_puInvertCompOp[uCompOp];
	}

	psInst = AllocateInst(psState, IMG_NULL);
	SetOpcodeAndDestCount(psState, psInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);

	SetPredicate(psState, psInst, uCompPredSrc, bCompPredNegate);

	MakePredicateArgument(&psInst->asDest[TEST_PREDICATE_DESTIDX], uPredDest);
	psInst->asArg[0] = *psSrc1;
	psInst->asArg[1] = *psSrc2;

	if (psSrc1Mod->bAbsolute)
	{
		PINST psMoveInst = AllocateInst(psState, IMG_NULL);

		SetOpcode(psState, psMoveInst, IFMOV);
		psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMoveInst->asDest[0].uNumber = GetNextRegister(psState);
		psMoveInst->asArg[0] = *psSrc1;
		psMoveInst->u.psFloat->asSrcMod[0] = *psSrc1Mod;
		psMoveInst->u.psFloat->asSrcMod[0].bNegate = IMG_FALSE;
		AppendInst(psState, psCodeBlock, psMoveInst);

		psInst->asArg[0] = psMoveInst->asDest[0];
		uSrc1Component = 0;
	}

	if (psSrc2Mod->bAbsolute)
	{
		PINST psMoveInst = AllocateInst(psState, IMG_NULL);

		SetOpcode(psState, psMoveInst, IFMOV);
		psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMoveInst->asDest[0].uNumber = GetNextRegister(psState);
		psMoveInst->asArg[0] = *psSrc2;
		psMoveInst->u.psFloat->asSrcMod[0] = *psSrc2Mod;
		psMoveInst->u.psFloat->asSrcMod[0].bNegate = IMG_FALSE;
		AppendInst(psState, psCodeBlock, psMoveInst);

		psInst->asArg[1] = psMoveInst->asDest[0];
		uSrc2Component = 0;
	}

	if (psSrc1Mod->bNegate)
	{
		if (psSrc2Mod->bNegate)
		{
			/* -A op -B -> -A + B op 0 -> -(A - B) op 0 */
			psInst->u.psTest->eAluOpcode = IFSUB;
			CompOpToTest(psState, g_puNegateCompOp[uCompOp], &psInst->u.psTest->sTest);
		}
		else
		{
			/* -A op B -> -A - B op 0 -> -(A + B) op 0 */
			psInst->u.psTest->eAluOpcode = IFADD;
			CompOpToTest(psState, g_puNegateCompOp[uCompOp], &psInst->u.psTest->sTest);
		}
	}
	else if (psSrc2Mod->bNegate)
	{
		/* A op -B -> A + B op 0 */
		psInst->u.psTest->eAluOpcode = IFADD;
		CompOpToTest(psState, uCompOp, &psInst->u.psTest->sTest);
	}
	else
	{
		psInst->u.psTest->eAluOpcode = IFSUB;
		CompOpToTest(psState, uCompOp, &psInst->u.psTest->sTest);
	}

	SetComponentSelect(psState, psInst, 0 /* uArgIdx */, uSrc1Component);
	SetComponentSelect(psState, psInst, 1 /* uArgIdx */, uSrc2Component);

	AppendInst(psState, psCodeBlock, psInst);
}

IMG_INTERNAL
IMG_VOID CreateComparisonFloat(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psCodeBlock,
							   IMG_UINT32			uPredDest,
							   UFREG_COMPOP			uCompOp,
							   PUF_REGISTER			psSrc1, 
							   PUF_REGISTER			psSrc2, 
							   IMG_UINT32			uChan,
							   UFREG_COMPCHANOP		eChanOp,
							   IMG_UINT32			uCompPredSrc,
							   IMG_BOOL				bCompPredNegate,
							   IMG_BOOL				bInvert)
/*****************************************************************************
 FUNCTION	: CreateComparisonFloat
    
 PURPOSE	: Generate instructions to do a comparison between two registers 
			  each in either F32 or F16 format and put the result in a USE
			  predicate register.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uCompOp			- Comparison type.
			  psSrc1			- First source to be compared.
			  psSrc2			- Second source to be compared.
			  uChan				- Channel within the sources to be compared.
			  eChanOp			- Operator to combine the TEST results on multiple
								channels.
			  uCompPredSrc		- Source predicate for the comparison.
			  bCompPredNegate
			  bInvert			- If TRUE invert the logic of the test.
			  
 RETURNS	: None.
*****************************************************************************/
{
	ARG						asOutSrc[2];
	FLOAT_SOURCE_MODIFIER	asOutSrcMod[2];
	IMG_UINT32				uArg;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		USEASM_TEST_CHANSEL	eChanSel;

		switch (eChanOp)
		{
			case UFREG_COMPCHANOP_ORALL: eChanSel = USEASM_TEST_CHANSEL_ORALL; break;
			case UFREG_COMPCHANOP_ANDALL: eChanSel = USEASM_TEST_CHANSEL_ANDALL; break;
			case UFREG_COMPCHANOP_NONE: eChanSel = (USEASM_TEST_CHANSEL)(USEASM_TEST_CHANSEL_C0 + uChan); break;
			default: imgabort();
		}

		GenerateComparisonF32F16_Vec(psState,
									 psCodeBlock,
									 uPredDest,
									 uCompOp,
									 psSrc1,
									 psSrc2,
									 eChanSel,
									 uCompPredSrc,
									 bCompPredNegate,
									 bInvert);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ASSERT(eChanOp == UFREG_COMPCHANOP_NONE);

	for (uArg = 0; uArg < 2; uArg++)
	{
		PUF_REGISTER	psInSrc;

		if (uArg == 0)
		{
			psInSrc = psSrc1;
		}
		else
		{
			psInSrc = psSrc2;
		}

		if (GetRegisterFormat(psState, psInSrc) == UF_REGFORMAT_F16)
		{
			IMG_UINT32	uComponent;

			GetSingleSourceF16(psState, 
							   psCodeBlock, 
							   psInSrc, 
							   &asOutSrc[uArg], 
							   &uComponent,
							   uChan, 
							   IMG_TRUE, 
							   &asOutSrcMod[uArg],
							   IMG_TRUE);
			asOutSrcMod[uArg].uComponent = uComponent;
		}
		else
		{
			ASSERT(GetRegisterFormat(psState, psInSrc) == UF_REGFORMAT_F32);

			GetSourceF32(psState, 
						 psCodeBlock,
						 psInSrc, 
						 uChan, 
						 &asOutSrc[uArg], 
						 IMG_TRUE,
						 &asOutSrcMod[uArg]);
		}
	}

	CreateComparisonF32(psState,
						psCodeBlock,
						uPredDest,
						uCompOp,
						&asOutSrc[0],
						&asOutSrcMod[0],
						&asOutSrc[1],
						&asOutSrcMod[1],
						uCompPredSrc,
						bCompPredNegate,
						bInvert);
}

IMG_INTERNAL
IMG_VOID ConvertSetpInstructionNonC10(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertSetpInstructionNonC10
    
 PURPOSE	: Convert an input SETP instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32  uCompOp;
	IMG_UINT32	uChanOp;
	IMG_BOOL	bExpandChanOp;
	IMG_VOID	(*pfCreateComparison)(PINTERMEDIATE_STATE	psState,
									  PCODEBLOCK			psCodeBlock,
									  IMG_UINT32			uPredDest,
									  UFREG_COMPOP			uCompOp,
									  PUF_REGISTER			psSrc1, 
									  PUF_REGISTER			psSrc2, 
									  IMG_UINT32			uChan,
									  UFREG_COMPCHANOP		eChanOp,
									  IMG_UINT32			uCompPredSrc,
									  IMG_BOOL				bCompPredNegate,
									  IMG_BOOL				bInvert);

	uCompOp = psSrc->asSrc[1].uNum & (~UFREG_COMPCHANOP_MASK);
	uChanOp = ((psSrc->asSrc[1].uNum & UFREG_COMPCHANOP_MASK) >>  UFREG_COMPCHANOP_SHIFT);

	/*
		Check instruction validity.
	*/
	if (uChanOp)
	{
		/* User can select only one predicate channel for ANDALL/ORALL operations */
		/* SETP with ANDALL/ORALL cannot be predicated now */
		if( (!g_abSingleBitSet[psSrc->sDest.u.byMask]) || (psSrc->uPredicate) )
		{
			imgabort();
		}
	}

	/*
		Are we doing an integer comparison?
	*/
	switch (psSrc->asSrc[0].eFormat)
	{
		case UF_REGFORMAT_F32:
		case UF_REGFORMAT_F16:
		{
			pfCreateComparison = CreateComparisonFloat;
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
			{
				bExpandChanOp = IMG_FALSE;
			}
			else
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */	
			{
				bExpandChanOp = IMG_TRUE;
			}
			break;
		}
		case UF_REGFORMAT_I32:
		case UF_REGFORMAT_U32:
		{
			ASSERT(psSrc->asSrc[2].eFormat == psSrc->asSrc[0].eFormat);
			pfCreateComparison = CreateComparisonInt32;
			bExpandChanOp = IMG_TRUE;
			break;
		}
		case UF_REGFORMAT_I16:
		case UF_REGFORMAT_U16:
		{
			ASSERT(psSrc->asSrc[2].eFormat == psSrc->asSrc[0].eFormat);
			pfCreateComparison = CreateComparisonInt16;
			bExpandChanOp = IMG_TRUE;
			break;
		}
		default: imgabort();
	}

	/* New code to handle 'and all' and 'or all' component operations */
	if (uChanOp != UFREG_COMPCHANOP_NONE)
	{
		IMG_UINT32	uChanPred;
		IMG_UINT32	uPredDest;

		/*
			Get the single predicate channel written by the instruction.
		*/
		ASSERT(g_abSingleBitSet[psSrc->sDest.u.byMask]);
		uChanPred = (IMG_UINT32)g_aiSingleComponent[psSrc->sDest.u.byMask];

		/* 
			Create the instruction to do the comparison. 
		*/
		ASSERT(psSrc->sDest.uNum < psState->uInputPredicateRegisterCount);
		uPredDest = USC_PREDREG_P0X + (psSrc->sDest.uNum * CHANNELS_PER_INPUT_REGISTER) + uChanPred;

		if (bExpandChanOp)
		{
			/*
				Expand
					SETP	P, A, ANDALL(==), B
				->
					TEST	P, A.X, ==, B.X
				  P TEST	P, A.Y, ==, B.X
				  P TEST	P, A.Z, ==, B.Z
				  P TEST	P, A.W, ==, B.W
			*/
			for (uChan = 0; uChan < 4; uChan++)
			{
				IMG_UINT32	uPredSrc;
				IMG_BOOL	bPredNegate;

				ASSERT(psSrc->uPredicate == UF_PRED_NONE);
				if (uChan > 0)
				{
					uPredSrc = uPredDest;
					if (uChanOp == UFREG_COMPCHANOP_ORALL)
					{
						bPredNegate = IMG_TRUE;
					}
					else
					{
						bPredNegate = IMG_FALSE;
					}
				}
				else
				{
					uPredSrc = USC_PREDREG_NONE;
					bPredNegate = IMG_FALSE;
				}

				pfCreateComparison(psState, 
								   psCodeBlock, 
								   uPredDest, 
								   (UFREG_COMPOP)uCompOp, 
								   &psSrc->asSrc[0], 
								   &psSrc->asSrc[2], 
								   uChan, 
								   UFREG_COMPCHANOP_NONE,
								   uPredSrc, 
								   bPredNegate,
								   IMG_FALSE);
			}
		}
		else
		{
			pfCreateComparison(psState, 
							   psCodeBlock, 
							   uPredDest, 
							   (UFREG_COMPOP)uCompOp, 
							   &psSrc->asSrc[0], 
							   &psSrc->asSrc[2], 
							   USC_UNDEF /* uChan */, 
							   uChanOp,
							   USC_PREDREG_NONE, 
							   IMG_FALSE /* bPredNegate */,
							   IMG_FALSE);
		}
	}
	else
	{
		for (uChan = 0; uChan < 4; uChan++)
		{
			IMG_UINT32	uPredDest;
			IMG_UINT32	uPredSrc;
			IMG_BOOL	bPredNegate;

			/* Ignore this component if it isn't included in the instruction mask. */
			if ((psSrc->sDest.u.byMask & (1U << uChan)) == 0)
			{
				continue;
			}

			/* 
				Create the instruction to do the comparison. 
			*/
			ASSERT(psSrc->sDest.uNum < psState->uInputPredicateRegisterCount);
			uPredDest = USC_PREDREG_P0X + (psSrc->sDest.uNum * CHANNELS_PER_INPUT_REGISTER) + uChan;

			GetInputPredicate(psState, &uPredSrc, &bPredNegate, psSrc->uPredicate, uChan);

			pfCreateComparison(psState,
							   psCodeBlock,
							   uPredDest,
							   (UFREG_COMPOP)uCompOp,
							   &psSrc->asSrc[0],
							   &psSrc->asSrc[2],
							   uChan,
							   UFREG_COMPCHANOP_NONE,
							   uPredSrc,
							   bPredNegate,
							   IMG_FALSE);
		}
	}
}

static
IMG_VOID ApplySourceMod(PINTERMEDIATE_STATE	psState, PCODEBLOCK psCodeBlock, PARG psSrc, IMG_UINT8 byMod)
{
	if ((byMod & UFREG_SOURCE_NEGATE) != 0 || (byMod & UFREG_SOURCE_ABS) != 0)
	{
		PINST	psInst;

		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IFMOV);
		psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asDest[0].uNumber = GetNextRegister(psState);
		psInst->asArg[0] = *psSrc;
		psInst->u.psFloat->asSrcMod[0].bNegate = (byMod & UFREG_SOURCE_NEGATE) ? IMG_TRUE : IMG_FALSE;
		psInst->u.psFloat->asSrcMod[0].bAbsolute = (byMod & UFREG_SOURCE_ABS) ? IMG_TRUE : IMG_FALSE;
		AppendInst(psState, psCodeBlock, psInst);

		*psSrc = psInst->asDest[0];
	}
}

static
IMG_VOID ApplyMultiformatSourceMod(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUF_REGISTER psInputSrc, PARG psSrc)
/*****************************************************************************
 FUNCTION	: ApplyMultiformatSourceMod
    
 PURPOSE	: Apply a source modifier to an intermediate instruction source
			  depending on the format of the source data.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputSrc		- Source modifier and source format.
			  psSrc				- Intermediate source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	switch (psInputSrc->eFormat)
	{
		case UF_REGFORMAT_I8_UN:
		case UF_REGFORMAT_U8_UN:
		case UF_REGFORMAT_U8:
		case UF_REGFORMAT_I16:
		case UF_REGFORMAT_U16:
		case UF_REGFORMAT_I32:
		case UF_REGFORMAT_U32:
		{
			if (psInputSrc->byMod & UFREG_SOURCE_ABS)
				IntegerAbsolute(psState, psCodeBlock, psSrc, psSrc);
			if (psInputSrc->byMod & UFREG_SOURCE_NEGATE)
				IntegerNegate(psState, psCodeBlock, NULL, psSrc, psSrc);
			break;
		}
		case UF_REGFORMAT_F16:
		case UF_REGFORMAT_F32:
		{
			ApplySourceMod(psState, psCodeBlock, psSrc, psInputSrc->byMod);
			break;
		}
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_VOID ConvertComparisonInstructionF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertComparisonInstruction
    
 PURPOSE	: Convert an input comparison instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST psInst;
	IMG_UINT32 uTempDest = 0;
	IMG_UINT32 puRepDest[4] = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
	IMG_UINT32 auTempDest[4] = {USC_UNDEF, USC_UNDEF, USC_UNDEF, USC_UNDEF};
	IMG_UINT32 i, j, k;
	IMG_UINT32 uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32 uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_BOOL bSrc0Neg, bSrc0Abs;
	ARG sSrc1 = USC_ARG_DEFAULT_VALUE;
	ARG sSrc2 = USC_ARG_DEFAULT_VALUE;
	TEST_TYPE eTest;
	IMG_UINT32 uTrueSrc;
	const IMG_BOOL bAtomic = psSrc->eOpCode == UFOP_ATOM_CMPXCHG;
	ARG sAddress;
	ARG sOldValue;
	InitInstArg(&sOldValue);

	if (psSrc->eOpCode == UFOP_CMP || psSrc->eOpCode == UFOP_CMPLT)
	{
		bSrc0Neg = (psSrc->asSrc[0].byMod & UFREG_SOURCE_NEGATE) ? IMG_TRUE : IMG_FALSE;
		bSrc0Abs = (psSrc->asSrc[0].byMod & UFREG_SOURCE_ABS) ? IMG_TRUE : IMG_FALSE;

		/*
			Convert the instruction type (CMP or CMPLT) and the source modifier on
			source 0 to a MOVC test type.
		*/
		GetCMPTestType(psState, psSrc, bSrc0Neg, bSrc0Abs, &eTest, &uTrueSrc);
	}
	else
	{
		ASSERT(psSrc->eOpCode == UFOP_MOVCBIT || bAtomic);
		bSrc0Neg = bSrc0Abs = IMG_FALSE;

		/*
			RESULT = SRC0 != 0 ? SRC1 : SRC2
		*/
		eTest = TEST_TYPE_NEQ_ZERO;
		uTrueSrc = USC_UNDEF;
	}

	if (bAtomic)
		GenerateAtomicLock(psState, psCodeBlock);

	for (i = 0; i < 4; i++)
	{
		IMG_UINT32	uPredSrc;
		IMG_BOOL	bPredNegate;
		ARG			sSrc0;

		/* Ignore this component if it isn't included in the instruction mask. */
		if ((psSrc->sDest.u.byMask & (1U << i)) == 0)
		{
			continue;
		}
		/* If this component is the same as earlier one then it has already been computed. */
		if (puRepDest[i] != UINT_MAX)
		{
			continue;
		}

		/* Check if this instruction needs special handling because of the instruction swizzle. */
		for (j = (i + 1); j < 4; j++)
		{
			if ((psSrc->sDest.u.byMask & (1U << j)) != 0 && puRepDest[j] == UINT_MAX)
			{
				if ((psSrc->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_XYZW)
				{
					/* Check if the calculation for this component is the same as a later one. */
					puRepDest[j] = i;
					for (k = 0; k < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; k++)
					{
						if (EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, i) != EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, j))
						{
							puRepDest[j] = UINT_MAX;
						}
					}
				}
				else
				{
					puRepDest[j] = UINT_MAX;
				}
				if (puRepDest[j] == UINT_MAX)
				{
					/* Check if the component we are writing in the destination is used later in the instruction. */
					for (k = 0; k < g_asInputInstDesc[psSrc->eOpCode].uNumSrcArgs; k++)
					{
						if (psSrc->sDest.eType == psSrc->asSrc[k].eType &&
							psSrc->sDest.uNum == psSrc->asSrc[k].uNum &&
							i == EXTRACT_CHAN(psSrc->asSrc[k].u.uSwiz, j))
						{
							uTempDest |= (1U << i);
						}
					}
				}
			}
		}

		/*
			For CMP/CND/MOVCBIT/ATOM_CMPXCHG get the additional sources and apply any source modifiers.
		*/
		GetSourceF32(psState, psCodeBlock, &psSrc->asSrc[1], i, &sSrc1, IMG_FALSE, NULL);
		GetSourceF32(psState, psCodeBlock, &psSrc->asSrc[2], i, &sSrc2, IMG_FALSE, NULL);

		ApplyMultiformatSourceMod(psState, psCodeBlock, &psSrc->asSrc[1], &sSrc1);
		ApplyMultiformatSourceMod(psState, psCodeBlock, &psSrc->asSrc[2], &sSrc2);

		/*
			Otherwise for UFOP_CMP/UFOP_CMPLT we don't have source modifiers on source 0 in MOVC so if we
			couldn't accommodate the source modifier by changing the test do the modifier in
			a seperate instruction.
		*/
		if (bSrc0Neg && !bSrc0Abs)
		{
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IFMOV);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = USC_TEMPREG_DRDEST;
			GetSourceF32(psState, 
						 psCodeBlock, 
						 &psSrc->asSrc[0], 
						 i, 
						 &psInst->asArg[0], 
						 IMG_TRUE, 
						 &psInst->u.psFloat->asSrcMod[0]);
			AppendInst(psState, psCodeBlock, psInst);

			sSrc0 = psInst->asDest[0];
		}
		else
		{
			GetSourceF32(psState, psCodeBlock, &psSrc->asSrc[0], i, &sSrc0, IMG_FALSE, NULL);

			/* We need to load the argument and generate the condition register */
			if (psSrc->eOpCode == UFOP_ATOM_CMPXCHG)
			{
				/* Save address */
				sAddress = sSrc0;
				/* Generate a register to keep the MOVCBIT result (which will be stored) */
				sOldValue.uType = USEASM_REGTYPE_TEMP;
				sOldValue.uNumber = GetNextRegister(psState);
				sOldValue.eFmt = sSrc0.eFmt;
				/* Generate another register for the condition */
				InitInstArg(&sSrc0);
				sSrc0.uType = USEASM_REGTYPE_TEMP;
				sSrc0.uNumber = GetNextRegister(psState);
				sSrc0.eFmt = sOldValue.eFmt;
				/* Do memory load */
				GenerateAtomicLoad(psState, psCodeBlock, &sAddress, &sOldValue);
				/* Old value has been loaded in, now we need to do the compare */
				psInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psInst, IXOR);
				psInst->asDest[0] = sSrc0;    /* Destination is MOVCBIT's condition register */
				psInst->asArg[0] = sOldValue; /* the old value loaded */
				psInst->asArg[1] = sSrc1;     /* The compare value */
				AppendInst(psState, psCodeBlock, psInst);
				/* Set the argument to MOVCBIT as the old value rather than the compare value */
				sSrc1 = sOldValue;
			}
		}

		/*
			Do the MOVC instruction itself.
		*/
		psInst = AllocateInst(psState, IMG_NULL);
		GetInputPredicate(psState, &uPredSrc, &bPredNegate, psSrc->uPredicate, i);
		SetPredicate(psState, psInst, uPredSrc, bPredNegate);

		if (eTest == TEST_TYPE_ALWAYS_TRUE)
		{
			SetOpcode(psState, psInst, IMOV);
			psInst->asArg[0] = (uTrueSrc == 1) ? sSrc1 : sSrc2;
		}
		else
		{
			if (psSrc->eOpCode == UFOP_MOVCBIT)
			{
				if (psState->uCompilerFlags & UF_OPENCL)
				{
					if (psSrc->asSrc[0].eFormat == UF_REGFORMAT_U8_UN ||
						psSrc->asSrc[0].eFormat == UF_REGFORMAT_I8_UN)
					{
						SetOpcode(psState, psInst, IMOVC_U8);
					}
					else if (psSrc->asSrc[0].eFormat == UF_REGFORMAT_U16 ||
							 psSrc->asSrc[0].eFormat == UF_REGFORMAT_I16)
					{
						SetOpcode(psState, psInst, IMOVC_I16);
					}
					else
					{
						SetOpcode(psState, psInst, IMOVC_I32);
					}
				}
				else
				{
					SetOpcode(psState, psInst, IMOVC_I32);
				}
			}
			else
			{
				SetOpcode(psState, psInst, IMOVC);
			}
			
			psInst->u.psMovc->eTest = eTest;

			/*
				Copy the intermediate form of the source arguments to the instruction.
			*/
			psInst->asArg[0] = sSrc0;
			psInst->asArg[1] = sSrc1;
			psInst->asArg[2] = sSrc2;	
		}

		if (uTempDest & (1U << i))
		{
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = auTempDest[i] = GetNextRegister(psState);
		}
		else
		{
			GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, i, &psInst->asDest[0]);
		}

		AppendInst(psState, psCodeBlock, psInst);

		/* Do any destination modifiers. */
		GenerateDestModF32(psState, 
						   psCodeBlock, 
						   &psInst->asDest[0], 
						   uSat, 
						   uScale, 
						   uPredSrc, 
						   bPredNegate);

		if (psSrc->eOpCode == UFOP_ATOM_CMPXCHG)
		{
			ARG sTemp;
			/* Store the result of the MOVCBIT into the address */
			GenerateAtomicStore(psState, psCodeBlock, &sAddress, &psInst->asDest[0]);
			/* Restore the mem loaded value onto sDest */
			sTemp = psInst->asDest[0];
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IMOV);
			psInst->asDest[0] = sTemp;
			psInst->asArg[0] = sOldValue;
			AppendInst(psState, psCodeBlock, psInst);
		}
	}

	for (i = 0; i < 4; i++)
	{
		if ((uTempDest & (1U << i)) != 0 || puRepDest[i] != UINT_MAX)
		{
			PINST psMoveInst;

			psMoveInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMoveInst, IMOV);
			GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, i, &psMoveInst->asDest[0]);
			if (puRepDest[i] != UINT_MAX)
			{
				GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, puRepDest[i], &psMoveInst->asArg[0]);
			}
			else
			{
				psMoveInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
				psMoveInst->asArg[0].uNumber = auTempDest[i];
			}
			GetInputPredicateInst(psState, psMoveInst, psSrc->uPredicate, i);
			AppendInst(psState, psCodeBlock, psMoveInst);
		}
	}

	/* Store into a destination register that needs special handling. */
	StoreIntoSpecialDest(psState, psCodeBlock, psSrc, &psSrc->sDest);

	if (bAtomic)
		GenerateAtomicRelease(psState, psCodeBlock);
}

static IMG_VOID Extract10Or2BitUnsignedInt(
											PINTERMEDIATE_STATE	psState, 
											PCODEBLOCK			psCodeBlock, 
											USC_CHANNELFORM		eFormat, 
											IMG_UINT32			uSrcRegType, 
											IMG_UINT32			uSrcRegNum, 
											IMG_UINT32			uByteOffset, 
											PARG				psDest)
/*****************************************************************************
 FUNCTION	: Extract10Or2BitUnsignedInt
    
 PURPOSE	: Extracts 10 or 2 bits unsiged integer from upper/significant 
			part of 16 bits.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  eFormat			- Format of the channel.
			  uSrcRegType		- Channel.
			  uSrcRegNum
			  uByteOffset
			  psDest			- Returns the register containing the zero
								extended result.
			  
 RETURNS	: None.
*****************************************************************************/
{	
	PINST		psPckInst;
	IMG_UINT32	uTempDest = GetNextRegister(psState);

	/*
		Move the result of the texture sample into the low 
		16-bits.
	*/
	psPckInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psPckInst, IUNPCKU16U16);	
	psPckInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psPckInst->asDest[0].uNumber = uTempDest;
	psPckInst->asArg[0].uType = uSrcRegType;
	psPckInst->asArg[0].uNumber = uSrcRegNum;
	SetPCKComponent(psState, psPckInst, 0 /* uArgIdx */, uByteOffset);
	psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psPckInst->asArg[1].uNumber = 0;
	AppendInst(psState, psCodeBlock, psPckInst);

	psPckInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psPckInst, ISHR);	
	psPckInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psPckInst->asDest[0].uNumber = uTempDest;
	psPckInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psPckInst->asArg[0].uNumber = uTempDest;	
	psPckInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	if(eFormat == USC_CHANNELFORM_UINT16_UINT10)
	{
		psPckInst->asArg[1].uNumber = 6;
	}
	else
	{
		ASSERT(eFormat == USC_CHANNELFORM_UINT16_UINT2);
		psPckInst->asArg[1].uNumber = 14;
	}
	AppendInst(psState, psCodeBlock, psPckInst);

	/*
		Return the register containing the unpacked result.
	*/
	psDest->uType = USEASM_REGTYPE_TEMP;
	psDest->uNumber = uTempDest;
}

IMG_INTERNAL
IMG_VOID UnpackTextureChannelToF32(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psCodeBlock,
								   IMG_UINT32			uFormat,
								   PARG					psDest,
								   IMG_UINT32			uPredSrc,
								   IMG_BOOL				bPredSrcNeg,
								   IMG_UINT32			uSrcRegType,
								   IMG_UINT32			uSrcRegNum,
								   IMG_UINT32			uSrcByteOffset,
								   IMG_BOOL				bIntegerChannels)
/*****************************************************************************
 FUNCTION	: UnpackTextureChannelToF32
    
 PURPOSE	: Generates code to unpack a single channel of a texture result
			  to F32 format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uFormat			- Format of the texture channel.
			  psDest			- Register to set to the unpacked data.
			  uPredSrc			- Predicate to control update of the destination.
			  bPredSrcNeg
			  uSrcRegType		- Register type of the data to unpack.
			  uSrcRegNum		- Register number of the data to unpack.
			  uSrcByteOffset	- Byte offset of the data to unpack within the
								register.
			  bIntegerChannels	- TRUE if channels with a constant value 
								should be written with integer 1 or 0. Otherwise
								they are written with floating point 1 or 0.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST	psInst;

	psInst = AllocateInst(psState, IMG_NULL);

	SetPredicate(psState, psInst, uPredSrc, bPredSrcNeg);

	switch (uFormat)
	{
		case USC_CHANNELFORM_ONE:
		case USC_CHANNELFORM_ZERO:
		{
			SetOpcode(psState, psInst, IMOV);

			if (bIntegerChannels)
			{
				/*
					Move INT32 1 or 0 into the destination.
				*/
				psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
				if (uFormat == USC_CHANNELFORM_ZERO)
				{
					psInst->asArg[0].uNumber = 0;
				}
				else
				{
					psInst->asArg[0].uNumber = 1;
				}
			}
			else
			{
				/*
					Move F32 1 or 0 into the destination.
				*/
				psInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
				if (uFormat == USC_CHANNELFORM_ZERO)
				{
					psInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
				}
				else
				{
					psInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
				}
			}
			break;
		}
		case USC_CHANNELFORM_F32:
		case USC_CHANNELFORM_UINT32:
		case USC_CHANNELFORM_SINT32:
		{
			SetOpcode(psState, psInst, IMOV);
				
			ASSERT(uSrcByteOffset == 0);
			psInst->asArg[0].uType = uSrcRegType;
			psInst->asArg[0].uNumber = uSrcRegNum;
			break;
		}
		case USC_CHANNELFORM_DF32:
		{
			SetOpcode(psState, psInst, IAND);

			ASSERT(uSrcByteOffset == 0);
			psInst->asArg[0].uType = uSrcRegType;
			psInst->asArg[0].uNumber = uSrcRegNum;
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;		
			psInst->asArg[1].uNumber = 0x7fffffff;
			break;
		}
		case USC_CHANNELFORM_UINT8:
		case USC_CHANNELFORM_UINT16:
		{
			IMG_UINT32	uSourceBitWidth;
			ARG			sSrc;

			if (uFormat == USC_CHANNELFORM_UINT8)
			{
				uSourceBitWidth = 8;
			}
			else
			{
				uSourceBitWidth = 16;
			}

			InitInstArg(&sSrc);
			sSrc.uType = uSrcRegType;
			sSrc.uNumber = uSrcRegNum; 

			SetOpcode(psState, psInst, IMOV);
			ZeroExtendInteger(psState, 
							  psCodeBlock, 
							  uSourceBitWidth, 
							  &sSrc,
							  uSrcByteOffset, 
							  &psInst->asArg[0]);
			break;
		}
		case USC_CHANNELFORM_SINT8:
		case USC_CHANNELFORM_SINT16:
		{
			IMG_UINT32	uSourceBitWidth;
			ARG			sSrc;

			if (uFormat == USC_CHANNELFORM_SINT8)
			{
				uSourceBitWidth = 8;
			}
			else
			{
				uSourceBitWidth = 16;
			}

			InitInstArg(&sSrc);
			sSrc.uType = uSrcRegType;
			sSrc.uNumber = uSrcRegNum;

			SetOpcode(psState, psInst, IMOV);
			SignExtendInteger(psState, 
							  psCodeBlock, 
							  uSourceBitWidth, 
							  &sSrc,
							  uSrcByteOffset, 
							  &psInst->asArg[0]);
			break;
		}
		case USC_CHANNELFORM_UINT16_UINT10:
		case USC_CHANNELFORM_UINT16_UINT2:
		{
			SetOpcode(psState, psInst, IMOV);
			Extract10Or2BitUnsignedInt(psState, 
									   psCodeBlock, 
									   uFormat, 
									   uSrcRegType, 
									   uSrcRegNum, 
									   uSrcByteOffset, 
									   &psInst->asArg[0]);
			break;
		}
		case USC_CHANNELFORM_U24:
		{
			IMG_UINT32	uUnpackDest = GetNextRegister(psState);

			UnpackU24Channel(psState, psCodeBlock, uSrcRegNum, uSrcByteOffset, uUnpackDest);

			SetOpcode(psState, psInst, IMOV);
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = uUnpackDest;
			break;
		}
		default:
		{
			UF_REGFORMAT	eFmt = UF_REGFORMAT_F32;
			switch (uFormat)
			{
				case USC_CHANNELFORM_U8: 
					SetOpcode(psState, psInst, IUNPCKF32U8); 	
					psInst->u.psPck->bScale = IMG_TRUE;
					eFmt = UF_REGFORMAT_U8;
					break;
				case USC_CHANNELFORM_S8: 
					SetOpcode(psState, psInst, IUNPCKF32S8); 
					psInst->u.psPck->bScale = IMG_TRUE;
					eFmt = UF_REGFORMAT_UNTYPED;
					break;
				case USC_CHANNELFORM_U16: 
					SetOpcode(psState, psInst, IUNPCKF32U16); 
					psInst->u.psPck->bScale = IMG_TRUE;
					break;
				case USC_CHANNELFORM_S16: 
					SetOpcode(psState, psInst, IUNPCKF32S16); 
					psInst->u.psPck->bScale = IMG_TRUE;
					break;
				case USC_CHANNELFORM_F16: 
					SetOpcode(psState, psInst, IUNPCKF32F16); 
					eFmt = UF_REGFORMAT_F16; 
					break;
				case USC_CHANNELFORM_U8_UN:
					SetOpcode(psState, psInst, IUNPCKF32U8); 
					break;
				case USC_CHANNELFORM_C10:
					SetOpcode(psState, psInst, IUNPCKF32C10); 
					eFmt = UF_REGFORMAT_C10;
					break;
				default: imgabort();
			}
			psInst->asArg[0].uType = uSrcRegType;
			psInst->asArg[0].uNumber = uSrcRegNum;
			SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uSrcByteOffset);
			psInst->asArg[0].eFmt = eFmt;
		}
		break;
	}

	psInst->asDest[0] = *psDest;

	AppendInst(psState, psCodeBlock, psInst);
}

IMG_INTERNAL
IMG_VOID GetGradientsF32(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 PUNIFLEX_INST			psInputInst,
						 IMG_UINT32				uGradDim,
						 IMG_UINT32				uCoord,
						 PSAMPLE_GRADIENTS		psGradients)
/*****************************************************************************
 FUNCTION	: GetGradientsF32
    
 PURPOSE	: Extract gradients for a texture sample instruction

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Texture load instruction.
			  uGradMask		- Mask of gradients to get.
			  uCoord		- Coordinate to get gradients for.

 OUTPUT		: None.
			  
 RETURNS	: None.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		GetGradientsF16F32Vec(psState, psCodeBlock, psInputInst, uGradDim, uCoord, UF_REGFORMAT_F32, psGradients);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ASSERT(psGradients->eGradFormat == UF_REGFORMAT_F32);

	/* Get the gradients for this access. */
	if (uGradDim > 0)
	{
		IMG_UINT32 uChan;
		
		for (uChan = 0; uChan < uGradDim; uChan++)
		{
			PINST					psMoveInst;
			PUF_REGISTER			psInputSrc = &psInputInst->asSrc[2 + uCoord];
			IMG_UINT32				uDestOffset = uChan * 2 + uCoord;
			FLOAT_SOURCE_MODIFIER	sSourceMod;

			psMoveInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMoveInst, IMOV);
			psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psMoveInst->asDest[0].uNumber = psGradients->uGradStart + uDestOffset;
			GetSourceF32(psState, 
						 psCodeBlock, 
						 psInputSrc, 
						 uChan, 
						 &psMoveInst->asArg[0], 
						 IMG_TRUE,
						 &sSourceMod);
			if (sSourceMod.bNegate ||
				sSourceMod.bAbsolute)
			{
				SetOpcode(psState, psMoveInst, IFMOV);
				psMoveInst->u.psFloat->asSrcMod[0] = sSourceMod;
			}
			AppendInst(psState, psCodeBlock, psMoveInst);
		}
	}
}

IMG_INTERNAL
IMG_VOID GetSampleIdxF32(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 PUNIFLEX_INST			psInputInst,
						 PARG					psSampleIdx)
/*****************************************************************************
 FUNCTION	: GetSampleIdxF32
    
 PURPOSE	: Extract sample idx for a texture sample instruction

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Texture load instruction.
			  psSampleIdx	- Sample Idx. update related info.

 OUTPUT		: None.
			  
 RETURNS	: None.
*****************************************************************************/
{		
	PINST					psMoveInst;
	PUF_REGISTER			psInputSrc = &psInputInst->asSrc[2];	
	FLOAT_SOURCE_MODIFIER	sSourceMod;

	psMoveInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psMoveInst, IMOV);
	psMoveInst->asDest[0] = *psSampleIdx;
	GetSourceF32(psState, 
				 psCodeBlock, 
				 psInputSrc, 
				 0, 
				 &psMoveInst->asArg[0], 
				 IMG_TRUE,
				 &sSourceMod);
	if (sSourceMod.bNegate ||
		sSourceMod.bAbsolute)
	{
		SetOpcode(psState, psMoveInst, IFMOV);
		psMoveInst->u.psFloat->asSrcMod[0] = sSourceMod;
	}
	AppendInst(psState, psCodeBlock, psMoveInst);
}

IMG_INTERNAL
IMG_VOID GetLODAdjustmentF32(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 PUNIFLEX_INST			psInputInst,
							 PSAMPLE_LOD_ADJUST		psLODAdjust)
/*****************************************************************************
 FUNCTION	: GetLODAdjustmentF32

 PURPOSE	: Generate instructions to put the LOD adjustment value in
			  a fixed register.

 PARAMETERS	: psState		- Compiler state
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Input instruction.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_BOOL				bFMov = IMG_FALSE;
	PINST					psMoveInst;
	PUF_REGISTER			psSrcArg;
	PFLOAT_SOURCE_MODIFIER	psSrcMod;

	if(psInputInst->eOpCode != UFOP_LD2DMS)
	{
		psSrcArg = &psInputInst->asSrc[2];
	}
	else
	{
		psSrcArg = &psInputInst->asSrc[3];
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		GetLODAdjustmentF16F32Vec(psState, psCodeBlock, psInputInst, UF_REGFORMAT_F32, psLODAdjust);
		return;
	}
	#endif /* defined(SUPPORT_SGX543)|| defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ASSERT(psLODAdjust->eLODFormat == UF_REGFORMAT_F32);

	psMoveInst = AllocateInst(psState, IMG_NULL);
	if ((psSrcArg->byMod & UFREG_SOURCE_NEGATE) != 0 || (psSrcArg->byMod & UFREG_SOURCE_ABS) != 0)
	{
		SetOpcode(psState, psMoveInst, IFMOV);
		psSrcMod = &psMoveInst->u.psFloat->asSrcMod[0];
		bFMov = IMG_TRUE;
	}
	else
	{
		SetOpcode(psState, psMoveInst, IMOV);
		psSrcMod = NULL;
	}
	psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psMoveInst->asDest[0].uNumber = psLODAdjust->uLODTemp;
	GetSourceF32(psState, 
				 psCodeBlock, 
				 psSrcArg, 
				 3, 
				 &psMoveInst->asArg[0], 
				 bFMov,
				 psSrcMod);
	AppendInst(psState, psCodeBlock, psMoveInst);
}

IMG_INTERNAL
IMG_VOID ConvertTextureArrayIndexToU16(PINTERMEDIATE_STATE	psState,
									   PCODEBLOCK			psCodeBlock,
									   IMG_UINT32			uTexture,
									   PARG					psFltArg,
									   IMG_UINT32			uFltComponent,
									   IMG_UINT32			uArrayIndexTemp)
/*****************************************************************************
 FUNCTION	: ConvertTextureArrayIndexToU16

 PURPOSE	: Convert an index into a texture array to U16 format.

 PARAMETERS	: psState		- Compiler state
			  psCodeBlock	- Block for any extra instructions.
			  uTexture		- Index of the texture to sample.
			  psFltArg		- Index in floating point format.
			  uFltComponent	- For an F16 index the component within a 32-bit
							register to use.
			  eArrayIndexFormat
							- Format of the array index.
			  uArrayIndexTemp
							- Destination register for the converted array index.
			  

 RETURNS	: None.
*****************************************************************************/
{
	PINST psMinInst;
	PINST psPackInst;
	
	/*
		Clamp the array index to the length of the array.
	*/
	psMinInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psMinInst, IFMIN);

	psMinInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psMinInst->asDest[0].uNumber = uArrayIndexTemp;

	LoadMaxTexarrayIndex(psState, psCodeBlock, uTexture, &psMinInst->asArg[0]);

	psMinInst->asArg[1] = *psFltArg;
	SetComponentSelect(psState, psMinInst, 1 /* uArgIdx */, uFltComponent);

	AppendInst(psState, psCodeBlock, psMinInst);
	
	/*
		Convert the array index to U16 format.
	*/
	psPackInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psPackInst, IPCKU16F32);
	psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psPackInst->asDest[0].uNumber = uArrayIndexTemp;
	psPackInst->asDest[0].eFmt = UF_REGFORMAT_F32;		
	psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psPackInst->asArg[0].uNumber = uArrayIndexTemp;
	psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psPackInst->asArg[1].uNumber = 0;
	AppendInst(psState, psCodeBlock, psPackInst);	
}

IMG_INTERNAL 
IMG_VOID GetSampleCoordinateArrayIndexF32(PINTERMEDIATE_STATE	psState, 
										  PCODEBLOCK			psCodeBlock, 
										  PUNIFLEX_INST			psInputInst, 
										  IMG_UINT32			uSrcChan, 
										  IMG_UINT32			uTexture,
										  IMG_UINT32			uArrayIndexTemp)
/*****************************************************************************
 FUNCTION	: GetSampleCoordinateArrayIndexF32
    
 PURPOSE	: Extract coordinates for a texture sample instruction

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psSrc			-
			  uCoordMask	- Indicate which coordinates to use.
			  bProjected	- Whether a projected sample.

 OUTPUT		: psCbase		- Base register of coordinates.
			  
 RETURNS	: None.
*****************************************************************************/
{	
	ARG				sF32Arg;
	PUF_REGISTER	psSrcArg;
	
	psSrcArg = &psInputInst->asSrc[0];
	GetSourceF32(psState, 
				 psCodeBlock, 
				 psSrcArg, 
				 uSrcChan, 
				 &sF32Arg, 
				 IMG_FALSE /* bAllowSourceMod */,
				 NULL /* psSourceMod */);
	ConvertTextureArrayIndexToU16(psState, psCodeBlock, uTexture, &sF32Arg, 0 /* uFltComponent */, uArrayIndexTemp);
}

IMG_INTERNAL
IMG_VOID GetProjectionF32(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psCodeBlock,
						  PUNIFLEX_INST			psInputInst,
						  PSAMPLE_PROJECTION	psProj,
						  UF_REGFORMAT			eCoordFormat)
/*****************************************************************************
 FUNCTION	: GetProjectionF32

 PURPOSE	: Extract the location of the value to project the texture coordinates
			  by from an input texture sample instruction.

 PARAMETERS	: psState		- Compiler state
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Input texture sample instruction.
			  psProj		- Returns detail of the projection value.

 RETURNS	: None.
*****************************************************************************/
{
	FLOAT_SOURCE_MODIFIER	sSourceMod;

	ASSERT(eCoordFormat == UF_REGFORMAT_F32);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		GetProjectionF16F32Vec(psState, psCodeBlock, psInputInst, psProj, eCoordFormat, IMG_FALSE /* bF16 */);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	psProj->uProjChannel = 0;
	psProj->uProjMask = USC_ALL_CHAN_MASK;

	GetSourceF32(psState,
				 psCodeBlock,
				 &psInputInst->asSrc[0],
				 USC_W_CHAN,
				 &psProj->sProj,
				 IMG_TRUE /* bAllowSourceMod */,
				 &sSourceMod);

	/*
		The projection source has a negate or absolute source modifier emit extra instructions
		to apply it.
	*/
	ApplyFloatSourceModifier(psState,
							 psCodeBlock, 
							 NULL /* psInsertBeforeInst */,
							 &psProj->sProj, 
							 &psProj->sProj, 
							 UF_REGFORMAT_F32, 
							 &sSourceMod);
}

IMG_INTERNAL 
IMG_VOID GetSampleCoordinatesF32(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psCodeBlock,
								 PUNIFLEX_INST			psInputInst,
								 IMG_UINT32				uDimensionality,
								 PSAMPLE_COORDINATES	psCoordinates,
								 IMG_BOOL				bPCF)
/*****************************************************************************
 FUNCTION	: GetSampleCoordinatesF32
    
 PURPOSE	: Extract coordinates for a texture sample instruction

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psSrc			-
			  uCoordMask	- Indicate which coordinates to use.
			  bProjected	- Whether a projected sample.

 OUTPUT		: psCbase		- Base register of coordinates.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psMoveInst;
	IMG_UINT32	uChan;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		GetSampleCoordinatesF16F32Vec(psState, 
									  psCodeBlock, 
									  psInputInst, 
									  uDimensionality, 
									  psCoordinates,
									  bPCF);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ASSERT(psCoordinates->eCoordFormat == UF_REGFORMAT_F32);

	/* Get the coordinates for this access. */
	for (uChan = 0; uChan < uDimensionality; uChan++)
	{
		IMG_BOOL				bFMov = IMG_FALSE;
		PUF_REGISTER			psSrcArg;
		IMG_UINT32				uSrcChan;
		PFLOAT_SOURCE_MODIFIER	psSourceMod;

		/*
			For LDC/LDCLZ take the PCF comparison argument from the third source.
		*/
		if (
				(psInputInst->eOpCode == UFOP_LDC || psInputInst->eOpCode == UFOP_LDCLZ) && 
				uChan == (uDimensionality - 1) &&
				bPCF
		   )
		{
			psSrcArg = &psInputInst->asSrc[2];
			uSrcChan = 3;
		}
		else
		{
			psSrcArg = &psInputInst->asSrc[0];
			uSrcChan = uChan;
		}

		psMoveInst = AllocateInst(psState, IMG_NULL);
		if ((psSrcArg->byMod & UFREG_SOURCE_NEGATE) != 0 ||
			(psSrcArg->byMod & UFREG_SOURCE_ABS) != 0)
		{
			SetOpcode(psState, psMoveInst, IFMOV);
			psSourceMod = &psMoveInst->u.psFloat->asSrcMod[0];
			bFMov = IMG_TRUE;
		}
		else
		{
			SetOpcode(psState, psMoveInst, IMOV);
			psSourceMod = NULL;
		}
		psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMoveInst->asDest[0].uNumber = psCoordinates->uCoordStart + uChan;
		GetSourceF32(psState, 
					 psCodeBlock, 
					 psSrcArg, 
					 uSrcChan, 
					 &psMoveInst->asArg[0], 
					 bFMov,
					 psSourceMod);
		AppendInst(psState, psCodeBlock, psMoveInst);
	}
}

#if defined(OUTPUT_USCHW)
static
IMG_VOID UnpackTexture(PINTERMEDIATE_STATE		psState, 
					   PCODEBLOCK				psCodeBlock, 
					   PSAMPLE_RESULT_LAYOUT	psSampleResultLayout,
					   PSAMPLE_RESULT			psSampleResult,
					   PUNIFLEX_INST			psInputInst,
					   IMG_UINT32				uMask)
/*****************************************************************************
 FUNCTION	: UnpackTexture
    
 PURPOSE	: Generates code to unpack a texture.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uTexture			- Number of the texture to unpack.
			  uSrcRegType		- Register type of the data to unpack.
			  uSrcRegNum		- Register number of the data to unpack.
			  psInputInst		- Texture load instruction.
			  uMask				- Mask of channel to unpack.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32			uChan;
	IMG_BOOL			bIntegerChannels;
	IMG_UINT32			uSat = (psInputInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32			uScale = (psInputInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;

	/*
		First check if the texture is a format we unpack to integer (not F32). This
		changes the interpretation of channels with a constant (1 or 0) value.
	*/
	bIntegerChannels = IMG_FALSE;
	for (uChan = 0; uChan < psSampleResultLayout->uChanCount; uChan++)
	{
		PSAMPLE_RESULT_LOCATION	psLocation;
		USC_CHANNELFORM			eChanForm;

		psLocation = &psSampleResultLayout->asChannelLocation[uChan];
		eChanForm = psLocation->eFormat;

		if (eChanForm == USC_CHANNELFORM_UINT8 ||
			eChanForm == USC_CHANNELFORM_UINT16 ||
			eChanForm == USC_CHANNELFORM_UINT32 ||
			eChanForm == USC_CHANNELFORM_SINT8 ||
			eChanForm == USC_CHANNELFORM_SINT16 ||
			eChanForm == USC_CHANNELFORM_SINT32 || 
			eChanForm == USC_CHANNELFORM_UINT16_UINT10 || 
			eChanForm == USC_CHANNELFORM_UINT16_UINT2)
		{
			bIntegerChannels = IMG_TRUE;
		}
	}

	for (uChan = 0; uChan < psSampleResultLayout->uChanCount; uChan++)
	{
		IMG_UINT32	uDestChan;

		uDestChan = uChan % CHANNELS_PER_INPUT_REGISTER;

		if (uMask & (1U << uDestChan))
		{
			USC_CHANNELFORM			eFormat;
			IMG_UINT32				uSrcRegNum, uSrcChanOffset;
			ARG						sDest;
			IMG_UINT32				uPredSrc;
			IMG_BOOL				bPredSrcNeg;
			UF_REGISTER				sInputDest;

			GetTextureSampleChannelLocation(psState,
											psSampleResultLayout, 
											psSampleResult, 
											uChan,
											&eFormat, 
											&uSrcRegNum, 
											#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
											NULL /* pbSrcIsVector */,
											#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
											&uSrcChanOffset);

			sInputDest = psInputInst->sDest;
			sInputDest.uNum += (uChan / CHANNELS_PER_INPUT_REGISTER);
			GetDestinationF32(psState, psCodeBlock, &sInputDest, uDestChan, &sDest); 
			GetInputPredicate(psState, &uPredSrc, &bPredSrcNeg, psInputInst->uPredicate, uDestChan);

			UnpackTextureChannelToF32(psState,
									  psCodeBlock,
									  eFormat,
									  &sDest,
									  uPredSrc,
									  bPredSrcNeg,
									  USEASM_REGTYPE_TEMP,
									  uSrcRegNum,
									  uSrcChanOffset,
									  bIntegerChannels);

			/* Do any destination modifiers. */
			GenerateDestModF32(psState, 
							   psCodeBlock, 
							   &sDest, 
							   uSat, 
							   uScale, 
							   uPredSrc, 
							   bPredSrcNeg);
		}
	}

	/* Store into a destination register that needs special handling. */
	StoreIntoSpecialDest(psState, psCodeBlock, psInputInst, &psInputInst->sDest);
}

static
PCODEBLOCK ConvertSamplerInstructionF32USCHW(PINTERMEDIATE_STATE	psState, 
											 PCODEBLOCK				psCodeBlock, 
											 PUNIFLEX_INST			psSrc, 
											 IMG_BOOL				bConditional)
/*****************************************************************************
 FUNCTION	: ConvertSamplerInstructionF32USCHW
    
 PURPOSE	: Convert an input texture sample instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  bConditional		- Whether the instruction is conditionally executed.
			  
 RETURNS	: None.
*****************************************************************************/

{
	SAMPLE_RESULT_LAYOUT sSampleResultLayout;
	SAMPLE_RESULT sSampleResult;

	/* Get the raw results of the texture load in a register. */
 	psCodeBlock = ConvertSamplerInstructionCore(psState, 
												psCodeBlock, 
												psSrc, 
												&sSampleResultLayout,
												&sSampleResult, 
												bConditional);
	
	/* Unpack the result to floating point. */
	UnpackTexture(psState, 
				  psCodeBlock, 
				  &sSampleResultLayout,
				  &sSampleResult, 
				  psSrc, 
				  psSrc->sDest.u.byMask);

	/* Free memory allocated by ConvertSamplerInstructionCore. */
	FreeSampleResultLayout(psState, &sSampleResultLayout);

	return psCodeBlock;
}
#endif /* defined(OUTPUT_USCHW) */

#if defined(OUTPUT_USPBIN)
static
PCODEBLOCK ConvertSamplerInstructionF32USPBIN(PINTERMEDIATE_STATE psState, 
											  PCODEBLOCK psCodeBlock, 
											  PUNIFLEX_INST psInputInst, 
											  IMG_BOOL bConditional)
/*****************************************************************************
 FUNCTION	: ConvertSamplerInstructionF32USPBIN
    
 PURPOSE	: Convert an input texture sample instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  bConditional		- Whether the instruction is conditionally executed.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChanMask;
	IMG_UINT32	uChanSwizzle;
	ARG			asDest[CHANNELS_PER_INPUT_REGISTER];

	uChanMask		= psInputInst->sDest.u.byMask;
	uChanSwizzle	= psInputInst->asSrc[1].u.uSwiz;

	/* Setup the location where we want the data to be sampled */
	if	((psInputInst->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_NONE || psInputInst->sDest.byMod != 0)
	{
		MakeArgumentSet(asDest, 
						CHANNELS_PER_INPUT_REGISTER, 
						USEASM_REGTYPE_TEMP, 
						GetNextRegisterCount(psState, CHANNELS_PER_INPUT_REGISTER), 
						UF_REGFORMAT_F32);

		uChanSwizzle = UFREG_SWIZ_NONE;
	}
	else
	{
		IMG_UINT32	uDest;

		for (uDest = 0; uDest < CHANNELS_PER_INPUT_REGISTER; uDest++)
		{
			GetDestinationF32(psState, psCodeBlock, &psInputInst->sDest, uDest, &asDest[uDest]);
		}
	}

	/* Get the raw results of the texture load in a register. */
	psCodeBlock = ConvertSamplerInstructionCoreUSP(psState, 
								  psCodeBlock, 
								  psInputInst, 
								  uChanMask,
								  uChanSwizzle,
								  asDest,
								  bConditional);
	
	/* Unpack the result to floating point. */
	if	((psInputInst->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_NONE || psInputInst->sDest.byMod != 0)
	{
		IMG_UINT32	uSat = (psInputInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
		IMG_UINT32	uScale = (psInputInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;

		if (uSat != UFREG_DMOD_SATNONE || uScale != UFREG_DMOD_SCALEMUL1)
		{
			IMG_UINT32	uChan;

			for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
			{
				if ((psInputInst->sDest.u.byMask & (1U << uChan)) != 0)
				{
					GenerateDestModF32(psState, 
									   psCodeBlock, 
									   &asDest[uChan], 
									   uSat, 
									   uScale, 	
									   USC_PREDREG_NONE, 
									   IMG_FALSE /* bPredNegate */);
				}
			}	
		}

		MoveF32VectorToInputDest(psState, psCodeBlock, psInputInst, psInputInst->asSrc[1].u.uSwiz, asDest[0].uNumber);
	}

	return psCodeBlock;
}
#endif	/* defined(OUTPUT_USPBIN) */

IMG_INTERNAL
PCODEBLOCK ConvertSamplerInstructionF32(PINTERMEDIATE_STATE	psState, 
										PCODEBLOCK			psCodeBlock, 
										PUNIFLEX_INST		psSrc,
										IMG_BOOL			bConditional)
/*****************************************************************************
 FUNCTION	: ConvertSamplerInstructionF32
    
 PURPOSE	: Convert an input texture sample instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  bConditional		- Whether the instruction is conditionally executed.
			  
 RETURNS	: None.
*****************************************************************************/
{
	return CHOOSE_USPBIN_OR_USCHW_FUNC(ConvertSamplerInstructionF32, (psState, psCodeBlock, psSrc, bConditional));
}

#if defined(OUTPUT_USCHW)
static IMG_VOID ConvertLoadTiledInstructionF32(PINTERMEDIATE_STATE	psState, 
											   PCODEBLOCK			psCodeBlock, 
											   PUNIFLEX_INST		psInputInst)
/*****************************************************************************
 FUNCTION	: ConvertLoadTiledInstructionF32
    
 PURPOSE	: Convert an input load tiled instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	SAMPLE_RESULT_LAYOUT	sSampleResultLayout;
	SAMPLE_RESULT			sSampleResult;
	ARG						asSrc[2];
	IMG_UINT32				uSrc;

	/*
		Convert the base and stride sources to the intermediate format.
	*/
	for (uSrc = 0; uSrc < 2; uSrc++)
	{
		GetSourceF32(psState,
					 psCodeBlock,
					 &psInputInst->asSrc[uSrc],
					 USC_X_CHAN,
					 &asSrc[uSrc],
					 IMG_FALSE /* bAllowSourceMod */,
					 NULL /* psSourceMod */);
		ASSERT(psInputInst->asSrc[uSrc].byMod == 0);
	}


	/* Get the raw results of the texture load in a register. */
 	ConvertLoadTiledInstructionCore(psState, 
									psCodeBlock, 
									psInputInst, 
									&asSrc[0], 
									&asSrc[1], 
									&sSampleResultLayout, 
									&sSampleResult);
	
	/* Unpack the result to 32 bits. */
	UnpackTexture(psState, 
				  psCodeBlock, 
				  &sSampleResultLayout,
				  &sSampleResult,
				  psInputInst, 
				  psInputInst->sDest.u.byMask);

	/*
		Free details of the result of the texture sample.
	*/
	FreeSampleResultLayout(psState, &sSampleResultLayout);
}

static IMG_VOID ConvertLoadBufferInstructionF32(PINTERMEDIATE_STATE	psState, 
												PCODEBLOCK			psCodeBlock, 
												PUNIFLEX_INST		psSrc, 
												IMG_BOOL			bConditional)
/*****************************************************************************
 FUNCTION	: ConvertLoadBufferInstructionF32
    
 PURPOSE	: Convert an input load buffer instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  bConditional		- Whether the instruction is conditionally executed.
			  
 RETURNS	: None.
*****************************************************************************/
{
	SAMPLE_RESULT_LAYOUT sSampleResultLayout;
	SAMPLE_RESULT sSampleResult;

	/* Get the raw results of the texture load in a register. */
 	ConvertLoadBufferInstructionCore(psState, psCodeBlock, psSrc, &sSampleResultLayout, &sSampleResult, bConditional);
	
	/* Unpack the result to 32 bits. */
	UnpackTexture(psState, 
				  psCodeBlock, 
				  &sSampleResultLayout,
				  &sSampleResult,
				  psSrc, 
				  psSrc->sDest.u.byMask);

	/*
		Free details of the result of the texture sample.
	*/
	FreeSampleResultLayout(psState, &sSampleResultLayout);
}
#endif /* defined(OUTPUT_USCHW) */

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
/*****************************************************************************
 FUNCTION	: ConvertNormaliseF32
    
 PURPOSE	: Convert a normalise instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
static 
IMG_VOID ConvertNormaliseF32(PINTERMEDIATE_STATE psState, 
							 PCODEBLOCK psCodeBlock, 
							 PUNIFLEX_INST psSrc)
{
	const IMG_UINT32 uNumArgs = 3;
	const IMG_UINT32 uNumDests = 3;
	PINST psInst = NULL;
	IMG_UINT32 uDrc = 0;
	IMG_UINT32 uCtr;
	const IMG_UINT32 uTempDestNum = GetNextRegisterCount(psState, uNumDests);
	
	ASSERT(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FNORMALISE);
	ASSERT(!(psState->psTargetBugs->ui32Flags & (SGX_BUG_FLAGS_FIX_HW_BRN_25580 | SGX_BUG_FLAGS_FIX_HW_BRN_25582)));

	/* 
	   ufop_nrm dst, src

	   fnrm dst, src.x, src.y, src.z, drc
	*/

	/* Set up the normalise */
	psInst = AllocateInst(psState, IMG_NULL);
	SetOpcodeAndDestCount(psState, psInst, IFNRM, uNumDests);

	/* Set up the intermediate destination */
	for (uCtr = 0; uCtr < uNumDests; uCtr++)
	{
		psInst->asDest[uCtr].uType = USEASM_REGTYPE_TEMP;
		psInst->asDest[uCtr].uNumber = uTempDestNum + uCtr;
	}

	/* Set up the source modifiers. */
	psInst->u.psNrm->bSrcModNegate = IMG_FALSE;
	if (psSrc->asSrc[0].byMod & UFREG_SOURCE_NEGATE)
	{
		psInst->u.psNrm->bSrcModNegate = IMG_TRUE;
	}
	psInst->u.psNrm->bSrcModAbsolute = IMG_FALSE;
	if (psSrc->asSrc[0].byMod & UFREG_SOURCE_ABS)
	{
		psInst->u.psNrm->bSrcModAbsolute = IMG_TRUE;
	}

	/* Set up the sources */
	for (uCtr = 0; uCtr < uNumArgs; uCtr ++)
	{
		GetSourceF32(psState, 
					 psCodeBlock, 
					 &psSrc->asSrc[0], 
					 uCtr, 
					 &psInst->asArg[uCtr], 
					 IMG_FALSE,
					 NULL);
	}

	/* Apply predicates */
	GetInputPredicateInst(psState, psInst, psSrc->uPredicate, 0);

	/* Set the drc register */
	psInst->asArg[NRM_DRC_SOURCE].uType = USEASM_REGTYPE_DRC;
	psInst->asArg[NRM_DRC_SOURCE].uNumber = uDrc;
	/* Add instruction to codeblock */
	AppendInst(psState, psCodeBlock, psInst);

	/* Copy each channel of the result to the destination registers */
	for (uCtr = 0; uCtr < uNumDests; uCtr ++)
	{
		if (!(psSrc->sDest.u.byMask & (1U << uCtr)))
		{
			/* This channel not written */
			continue;
		}

		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IMOV);

		GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, uCtr, &psInst->asDest[0]);

		psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asArg[0].uNumber = uTempDestNum + uCtr;

		AppendInst(psState, psCodeBlock, psInst);
	}
}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

#if defined(SUPPORT_SGX545)
/*****************************************************************************
 FUNCTION	: ConvertSincosF32HWSupport
    
 PURPOSE	: Convert an sincos instruction to the intermediate format where the
			  target processor directly supports sin and cos instruction..

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
static IMG_VOID ConvertSincosF32HWSupport(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
{
	IMG_UINT32 uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32 uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32 uChan;
	PINST	psMulInst;

	/*
		TEMP = SRC * (2/PI)
	*/
	psMulInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psMulInst, IFMUL);
	psMulInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psMulInst->asDest[0].uNumber = GetNextRegister(psState);
	GetSourceF32(psState, 
				 psCodeBlock, 
				 &psSrc->asSrc[0], 
				 3, 
				 &psMulInst->asArg[0], 
				 IMG_TRUE, 
				 &psMulInst->u.psFloat->asSrcMod[0]);
	psMulInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
	psMulInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2OVERPI;
	AppendInst(psState, psCodeBlock, psMulInst);

	for (uChan = 0; uChan < 2; uChan++)
	{
		PINST		psSCInst;
		IMG_UINT32	uPredSrc = USC_PREDREG_NONE;
		IMG_BOOL	bPredNegate = IMG_FALSE;

		if (!(psSrc->sDest.u.byMask & (1U << uChan)))
		{
			continue;
		}

		/*
			DEST = SIN(TEMP) or
			DEST = COS(TEMP)
		*/
		psSCInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psSCInst, (uChan == 0) ? IFCOS : IFSIN);
		GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, uChan, &psSCInst->asDest[0]);
		GetInputPredicate(psState, &uPredSrc, &bPredNegate, psSrc->uPredicate, uChan);
		SetPredicate(psState, psSCInst, uPredSrc, bPredNegate);
		psSCInst->asArg[0] = psMulInst->asDest[0];
		AppendInst(psState, psCodeBlock, psSCInst);

		/*
			Apply destination modifiers.
		*/
		GenerateDestModF32(psState, 
						   psCodeBlock, 
						   &psSCInst->asDest[0], 
						   uSat, 
						   uScale, 
						   uPredSrc, 
						   bPredNegate);
	}
}

/*****************************************************************************
 FUNCTION	: ConvertSincos2F32HWSupport
    
 PURPOSE	: Convert an sincos instruction to the intermediate format where the
			  target processor directly supports sin and cos instruction..

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
static IMG_VOID ConvertSincos2F32HWSupport(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
{
	IMG_UINT32 uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32 uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32 uSat2 = (psSrc->sDest2.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32 uScale2 = (psSrc->sDest2.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32 uChan;	

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{		
		PINST		psMulInst;
		IMG_UINT32	uPredSrc = USC_PREDREG_NONE;
		IMG_BOOL	bPredNegate = IMG_FALSE;

		/*
			Convert any predicate on the input instruction.
		*/
		GetInputPredicate(psState, &uPredSrc, &bPredNegate, psSrc->uPredicate, uChan);

		/*
			TEMP = SRC * (2/PI)
		*/
		psMulInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMulInst, IFMUL);
		psMulInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMulInst->asDest[0].uNumber = USC_TEMPREG_TEMPSRC;
		GetSourceF32(psState, 
					 psCodeBlock, 
					 &psSrc->asSrc[0], 
					 uChan, 
					 &psMulInst->asArg[0], 
					 IMG_TRUE,
					 &psMulInst->u.psFloat->asSrcMod[0]);
		psMulInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
		psMulInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2OVERPI;
		AppendInst(psState, psCodeBlock, psMulInst);	
		if ((psSrc->sDest.u.byMask & (1U << uChan)))
		{
			PINST	psSCInst;
			/*
				DEST = SIN(TEMP)
			*/
			psSCInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psSCInst, IFSIN);
			GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, uChan, &psSCInst->asDest[0]);
			SetPredicate(psState, psSCInst, uPredSrc, bPredNegate);
			psSCInst->asArg[0] = psMulInst->asDest[0];
			AppendInst(psState, psCodeBlock, psSCInst);

			/*
				Apply destination modifiers.
			*/
			GenerateDestModF32(psState, 
							   psCodeBlock, 
							   &psSCInst->asDest[0], 
							   uSat, 
							   uScale, 
							   uPredSrc, 
							   bPredNegate);
		}
		if ((psSrc->sDest2.u.byMask & (1U << uChan)))
		{
			PINST	psSCInst;
			/*
				DEST = COS(TEMP)
			*/
			psSCInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psSCInst, IFCOS);
			GetDestinationF32(psState, psCodeBlock, &psSrc->sDest2, uChan, &psSCInst->asDest[0]);
			SetPredicate(psState, psSCInst, uPredSrc, bPredNegate);
			psSCInst->asArg[0] = psMulInst->asDest[0];
			AppendInst(psState, psCodeBlock, psSCInst);

			/*
				Apply destination modifiers.
			*/
			GenerateDestModF32(psState, 
							   psCodeBlock, 
							   &psSCInst->asDest[0], 
							   uSat2, 
							   uScale2, 
							   uPredSrc, 
							   bPredNegate);
		}

	}	
}
#endif /* defined(SUPPORT_SGX545) */

static IMG_VOID ConvertLogpInstructionF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertLogpInstruction
    
 PURPOSE	: Convert an input logp instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32 uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	PINST psInst;
	IMG_UINT32 uAbsSrc = USC_UNDEF;

	if ((psSrc->sDest.u.byMask & (1U << 0)) || (psSrc->sDest.u.byMask & (1U << 1)))
	{
		uAbsSrc = GetNextRegister(psState);

		/* TEMP = ABS(SRC.W) */
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IFMOV);
		psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asDest[0].uNumber = uAbsSrc;
		GetSourceF32(psState, 
					 psCodeBlock, 
					 &psSrc->asSrc[0], 
					 USC_W_CHAN /* uChan */, 
					 &psInst->asArg[0], 
					 IMG_FALSE, 
					 NULL);
		psInst->u.psFloat->asSrcMod[0].bAbsolute = IMG_TRUE;
		AppendInst(psState, psCodeBlock, psInst);
	}

	if (psSrc->sDest.u.byMask & (1U << 0))
	{
		ARG sDestX;

		GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, 0, &sDestX);

		/* DEST.x = (*(IMG_PUINT32)&SRC.W) >> 23 */
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, ISHR);
		psInst->asDest[0] = sDestX;
		psInst->asArg[BITWISE_SHIFTEND_SOURCE].uType = USEASM_REGTYPE_TEMP;
		psInst->asArg[BITWISE_SHIFTEND_SOURCE].uNumber = uAbsSrc;
		psInst->asArg[BITWISE_SHIFT_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[BITWISE_SHIFT_SOURCE].uNumber = 23;
		AppendInst(psState, psCodeBlock, psInst);

		/* DEST.x = DEST.x - 127 */
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IIMA16);
		psInst->asDest[0] = sDestX;
		psInst->asArg[0] = sDestX;
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 1;
		psInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[2].uNumber = 127;
		psInst->u.psIma16->bNegateSource2 = IMG_TRUE;
		AppendInst(psState, psCodeBlock, psInst);

		/* DEST.x = (FLOAT)DEST.X */
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IUNPCKF32S16);
		psInst->asDest[0] = sDestX;
		psInst->asArg[0] = sDestX;
		AppendInst(psState, psCodeBlock, psInst);

		GenerateDestModF32(psState, psCodeBlock, &sDestX, uSat, uScale, USC_PREDREG_NONE, IMG_FALSE);
	}

	if (psSrc->sDest.u.byMask & (1U << 1))
	{	
		ARG sDestY;

		GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, 1, &sDestY);

		/* DEST.Y = SRC.W & 0x7FFFFF */
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IAND);
		psInst->asDest[0] = sDestY;
		psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asArg[0].uNumber = uAbsSrc;
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0x7fffff;
		AppendInst(psState, psCodeBlock, psInst);

		/* DEST.Y = DEST.Y | 0x3F800000 */
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IOR);
		psInst->asDest[0] = sDestY;
		psInst->asArg[0] = sDestY;
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0x3f800000;
		AppendInst(psState, psCodeBlock, psInst);

		GenerateDestModF32(psState, psCodeBlock, &sDestY, uSat, uScale, USC_PREDREG_NONE, IMG_FALSE);
	}

	if (psSrc->sDest.u.byMask & (1U << 2))
	{
		/* DEST.z = LOG(SRC.W) */
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IFLOG);
		GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, 2, &psInst->asDest[0]);
		GetSourceF32(psState, 
					 psCodeBlock, 
					 &psSrc->asSrc[0],
					 USC_W_CHAN, 
					 &psInst->asArg[0], 
					 IMG_TRUE, 
					 &psInst->u.psFloat->asSrcMod[0]);
		psInst->u.psFloat->asSrcMod[0].bAbsolute = IMG_TRUE;
		AppendInst(psState, psCodeBlock, psInst);

		GenerateDestModF32(psState, psCodeBlock, &psInst->asDest[0], uSat, uScale, USC_PREDREG_NONE, IMG_FALSE);
	}

	if (psSrc->sDest.u.byMask & (1U << 3))
	{
		/* DEST.W = 1 */
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IMOV);
		GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, 3, &psInst->asDest[0]);
		psInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
		psInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
		AppendInst(psState, psCodeBlock, psInst);

		GenerateDestModF32(psState, psCodeBlock, &psInst->asDest[0], uSat, uScale, USC_PREDREG_NONE, IMG_FALSE);
	}

	/* Store into a destination register that needs special handling. */
	StoreIntoSpecialDest(psState, psCodeBlock, psSrc, &psSrc->sDest);
}

static IMG_VOID ConvertTexkillInstructionBit(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psInputInst, IMG_BOOL bConditional)
/*****************************************************************************
 FUNCTION	: ConvertTexkillInstructionBit
    
 PURPOSE	: Convert a UFOP_KILLNZBIT instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psInst;

	/* Create a test instruction to check for non-zero bits. */
	psInst = AllocateInst(psState, IMG_NULL);
	SetOpcodeAndDestCount(psState, psInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
	psInst->u.psTest->eAluOpcode = IOR;
	if (psInputInst->asSrc[0].byMod & UFREG_SOURCE_NEGATE)
	{
		psInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
	}
	else
	{
		psInst->u.psTest->sTest.eType = TEST_TYPE_EQ_ZERO;
	}
	/* Get the intermediate version of the source for comparison. */
	GetSourceF32(psState,
				 psCodeBlock,
				 &psInputInst->asSrc[0], 
				 UFREG_SWIZ_Z, 
				 &psInst->asArg[0], 
				 IMG_FALSE,
				 NULL);
	psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[1].uNumber = 0;

	/*
		Do we need to combine the result from the instruction
		with previous texkill results.
	*/
	if (bConditional || (psState->uFlags & USC_FLAGS_TEXKILL_PRESENT) != 0)
	{
		SetPredicate(psState, psInst, USC_PREDREG_TEXKILL, IMG_FALSE /* bPredNegate */);
	}
	/*
		Do we need to initialise the texkill predicate register at the
		start of the program? Yes if it is written inside a conditionally
		executed block.
	*/
	if ((psState->uFlags & USC_FLAGS_TEXKILL_PRESENT) == 0)
	{
		if (bConditional)
		{
			psState->uFlags |= USC_FLAGS_INITIALISE_TEXKILL;
		}
		else
		{
			psState->uFlags &= ~USC_FLAGS_INITIALISE_TEXKILL;
		}
	}
	psState->uFlags |= USC_FLAGS_TEXKILL_PRESENT;
	MakePredicateArgument(&psInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEXKILL);

	AppendInst(psState, psCodeBlock, psInst);
}

IMG_INTERNAL 
IMG_VOID ConvertTexkillInstructionFloat(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc, IMG_BOOL bConditional, IMG_BOOL bFloat32)
/*****************************************************************************
 FUNCTION	: ConvertTexkillInstruction
    
 PURPOSE	: Convert an input texkill instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 i, j;
	IMG_BOOL bNonConstSwiz, bAlwaysTrue;

	/* abs(A) < 0 -> IMG_FALSE */
	if ((psSrc->asSrc[0].byMod & UFREG_SOURCE_ABS) != 0 && 
		(psSrc->asSrc[0].byMod & UFREG_SOURCE_NEGATE) == 0 && 
		psSrc->eOpCode != UFOP_KPLE)
	{
		return;
	}

	bNonConstSwiz = IMG_TRUE;
	for (j = 0; j < 4; j++)
	{
		if (EXTRACT_CHAN(psSrc->asSrc[0].u.uSwiz, j) >= UFREG_SWIZ_1)
		{
			bNonConstSwiz = IMG_FALSE;
			break;
		}
	}

	bAlwaysTrue = IMG_FALSE;
	/* -abs(A) <= 0 -> IMG_TRUE */
	if (psSrc->eOpCode == UFOP_KPLE && 
		(psSrc->asSrc[0].byMod & UFREG_SOURCE_ABS) != 0 && 
		(psSrc->asSrc[0].byMod & UFREG_SOURCE_NEGATE) != 0)
	{
		bAlwaysTrue = IMG_TRUE;
	}
	/* -1 < 0 -> IMG_TRUE */
	if (bNonConstSwiz &&
		psSrc->asSrc[0].eType == UFREG_TYPE_HW_CONST &&
		psSrc->asSrc[0].uNum == HW_CONST_1 &&
		(psSrc->asSrc[0].byMod & UFREG_SOURCE_NEGATE) != 0)
	{
		bAlwaysTrue = IMG_TRUE;
	}
	
	if (bAlwaysTrue)
	{
		PINST psInst = AllocateInst(psState, IMG_NULL);
		SetOpcodeAndDestCount(psState, psInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
		psInst->u.psTest->eAluOpcode = IFMOV;
		MakePredicateArgument(&psInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEXKILL);
		psInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
		psInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
		psInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;	
		AppendInst(psState, psCodeBlock, psInst);

		if ((psState->uFlags & USC_FLAGS_TEXKILL_PRESENT) == 0)
		{
			if (bConditional)
			{
				psState->uFlags |= USC_FLAGS_INITIALISE_TEXKILL;
			}
			else
			{
				psState->uFlags &= ~USC_FLAGS_INITIALISE_TEXKILL;
			}
		}
		psState->uFlags |= USC_FLAGS_TEXKILL_PRESENT;
	}
	else
	{
		for (i = 0; i < 4; i++)
		{
			PINST psInst;

			for (j = 0; j < i; j++)
			{
				if (EXTRACT_CHAN(psSrc->asSrc[0].u.uSwiz, i) == EXTRACT_CHAN(psSrc->asSrc[0].u.uSwiz, j))
				{
					break;
				}
			}
			if (j < i)
			{
				continue;
			}
			
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcodeAndDestCount(psState, psInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
			if (bConditional || (psState->uFlags & USC_FLAGS_TEXKILL_PRESENT) != 0)
			{
				SetPredicate(psState, psInst, USC_PREDREG_TEXKILL, IMG_FALSE /* bPredNegate */);
			}
			if ((psState->uFlags & USC_FLAGS_TEXKILL_PRESENT) == 0)
			{
				if (bConditional)
				{
					psState->uFlags |= USC_FLAGS_INITIALISE_TEXKILL;
				}
				else
				{
					psState->uFlags &= ~USC_FLAGS_INITIALISE_TEXKILL;
				}
			}
			psState->uFlags |= USC_FLAGS_TEXKILL_PRESENT;
			MakePredicateArgument(&psInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEXKILL);
			if (psSrc->eOpCode == UFOP_KPLE)
			{
				if (psSrc->asSrc[0].byMod & UFREG_SOURCE_ABS)
				{
					/* abs(A) > 0 -> A != 0 */
					CompOpToTest(psState, UFREG_COMPOP_NE, &psInst->u.psTest->sTest);
				}
				else
				{
					/* A > 0 */
					CompOpToTest(psState, UFREG_COMPOP_GT, &psInst->u.psTest->sTest);
				}
			}
			else
			{
				if (psSrc->asSrc[0].byMod & UFREG_SOURCE_ABS)
				{
					/* -abs(A) >= 0 -> abs(A) <= 0 -> A == 0 */
					CompOpToTest(psState, UFREG_COMPOP_EQ, &psInst->u.psTest->sTest);
				}
				else
				{
					/* A >= 0 */
					CompOpToTest(psState, UFREG_COMPOP_GE, &psInst->u.psTest->sTest);
				}
			}
			if (psSrc->asSrc[0].byMod & UFREG_SOURCE_NEGATE)
			{
				psInst->u.psTest->eAluOpcode = IFSUB;
				psInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
				psInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;	
				if (bFloat32)
				{
					GetSourceF32(psState, 
								 psCodeBlock, 
								 &psSrc->asSrc[0], 
								 i, 
								 &psInst->asArg[1], 
								 IMG_FALSE, 
								 NULL);
				}
				else
				{
					IMG_UINT32	 uComponent;
					GetSingleSourceF16(psState, 
									   psCodeBlock, 
									   &psSrc->asSrc[0], 
									   &psInst->asArg[1], 
									   &uComponent,
									   i, 
									   IMG_FALSE, 
									   NULL, 
									   IMG_TRUE); 
					psInst->u.psFloat->asSrcMod[1].uComponent = uComponent;
				}
			}
			else
			{
				psInst->u.psTest->eAluOpcode = IFMOV;
				if (bFloat32)
				{
					GetSourceF32(psState, 
								 psCodeBlock, 
								 &psSrc->asSrc[0], 
								 i, 
								 &psInst->asArg[0], 
								 IMG_FALSE, 
								 NULL);
				}
				else
				{
					IMG_UINT32	uComponent;

					GetSingleSourceF16(psState, 
									   psCodeBlock, 
									   &psSrc->asSrc[0], 
									   &psInst->asArg[0],
									   &uComponent,
									   i, 
									   IMG_FALSE, 
									   NULL,
									   IMG_TRUE); 
					psInst->u.psFloat->asSrcMod[0].uComponent = uComponent;
				}
			}

			AppendInst(psState, psCodeBlock, psInst);
		}
	}
}

IMG_INTERNAL
IMG_VOID ConvertIntegerDot34(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertIntegerDot34
    
 PURPOSE	: 

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_BOOL	bDot3 = (psSrc->eOpCode == UFOP_DOT3) ? IMG_TRUE : IMG_FALSE;
	PINST		psInst;
	IMG_UINT32	uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_BOOL	bOffset = IMG_FALSE;
	IMG_UINT32	uChan;
	IMG_UINT32	uArg;
	IMG_UINT32	uDotResultTempNum;
	IMG_BOOL	bSat;

	/* Skip if no channels are written. */
	if (psSrc->sDest.u.byMask == 0)
	{
		return;
	}

	/* Find a channel to write into */
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (psSrc->sDest.u.byMask & (1U << uChan))
		{
			break;
		}
	}

	/*
		Get a register to hold the result of the dotproduct in integer format.
	*/
	uDotResultTempNum = GetNextRegister(psState);

	/* Generate the dotproduct instruction. */
	psInst = AllocateInst(psState, IMG_NULL);

	SetOpcode(psState, psInst, IFP16DOT);

	psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psInst->asDest[0].uNumber = uDotResultTempNum;

	if ((psSrc->asSrc[0].byMod & UFREG_SMOD_MASK) != UFREG_SMOD_NONE)
	{
		ASSERT((psSrc->asSrc[0].byMod & UFREG_SMOD_MASK) == UFREG_SMOD_SIGNED);
		ASSERT((psSrc->asSrc[1].byMod & UFREG_SMOD_MASK) == UFREG_SMOD_SIGNED);

		psInst->u.psDot34->bOffset = IMG_TRUE;
		bOffset = IMG_TRUE;
		uScale += 2;

		ASSERT(uScale <= UFREG_DMOD_SCALEMUL8);
	}

	/*
		Set the length of the source vectors.
	*/
	if (bDot3)
	{
		psInst->u.psDot34->uVecLength = 3;
	}
	else
	{
		psInst->u.psDot34->uVecLength = 4;
	}

	psInst->u.psDot34->uDot34Scale = uScale;
	for (uArg = 0; uArg < 2; uArg++)
	{
		IMG_BYTE bySourceMod;

		ASSERT(psSrc->asSrc[uArg].eType == UFREG_TYPE_COL || psSrc->asSrc[uArg].eType == UFREG_TYPE_TEMP);

		bySourceMod = (IMG_BYTE)(psSrc->asSrc[uArg].byMod & ~UFREG_SMOD_MASK);
		GetSourceC10(psState, psCodeBlock, &psSrc->asSrc[uArg], bySourceMod, 
			&psInst->asArg[uArg], bDot3 ? 7 : 15, IMG_FALSE, IMG_FALSE, psSrc->asSrc[uArg].eFormat);
	}
	AppendInst(psState, psCodeBlock, psInst);

	bSat = IMG_FALSE;
	if (bOffset && uSat == UFREG_DMOD_SATZEROONE)
	{
		bSat = IMG_TRUE;
	}
	
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		ConvertIntegerDotProductResult_Vec(psState, psCodeBlock, psSrc, uDotResultTempNum, bSat);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		/* Unpack the result of the dotproduct to floating point */
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IUNPCKF32S16);
		psInst->u.psPck->bScale = IMG_TRUE;
		GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, uChan, &psInst->asDest[0]);
		psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asArg[0].uNumber = uDotResultTempNum;
		AppendInst(psState, psCodeBlock, psInst);

		/* Apply any saturation modifier */
		if (bSat)
		{
			PINST	psSatInst;

			psSatInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psSatInst, IFMAX);
			psSatInst->asDest[0] = psInst->asDest[0];
			psSatInst->asArg[0] = psInst->asDest[0];
			psSatInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
			psSatInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;

			AppendInst(psState, psCodeBlock, psSatInst);
		}

		/* Replicate the result to all channels. */
		uChan++;
		for (; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
		{
			if (psSrc->sDest.u.byMask & (1U << uChan))
			{
				PINST	psMoveInst;

				psMoveInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMoveInst, IMOV);
				GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, uChan, &psMoveInst->asDest[0]);
				psMoveInst->asArg[0] = psInst->asDest[0];
				AppendInst(psState, psCodeBlock, psMoveInst);
			}
		}

		/* Store into a destination register that needs special handling. */
		StoreIntoSpecialDest(psState, psCodeBlock, psSrc, &psSrc->sDest);
	}
}

static IMG_VOID ConvertInteger32ToF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psInputInst)
/*****************************************************************************
 FUNCTION	: ConvertInteger32ToF32
    
 PURPOSE	: Convert an input MOV from either an I32 or a U32 format register 
			  to an F32 format destination to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_BOOL		bTempDest;
	IMG_UINT32		uChan;
	UF_REGFORMAT	eSrcFmt = GetRegisterFormat(psState, &psInputInst->asSrc[0]);
	IMG_UINT32		uScaleFactorTemp;
	IMG_UINT32		uSat = (psInputInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32		uScale = (psInputInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32		uTempDestStartNum;

	/*
		Use a temporary for the destination to avoid conversion instructions for earlier channels overwriting 
		source data for later channels.
	*/
	bTempDest = IMG_FALSE;
	if (psInputInst->sDest.eType == psInputInst->asSrc[0].eType &&
		psInputInst->sDest.uNum == psInputInst->asSrc[0].uNum)
	{
		bTempDest = IMG_TRUE;
		uTempDestStartNum = GetNextRegisterCount(psState, CHANNELS_PER_INPUT_REGISTER);
	}
	else
	{
		uTempDestStartNum = USC_UNDEF;
	}

	/*
		Get a register containing 65536.
	*/
	{
		PINST		psLimmInst;
		IMG_FLOAT	f65536;

		f65536 = (IMG_FLOAT)65536;

		uScaleFactorTemp = GetNextRegister(psState);
		psLimmInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psLimmInst, IMOV);
		psLimmInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psLimmInst->asDest[0].uNumber = uScaleFactorTemp;
		psLimmInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
		psLimmInst->asArg[0].uNumber = *(IMG_PUINT32)&f65536;
		AppendInst(psState, psCodeBlock, psLimmInst);
	}

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		IMG_UINT32				uPackTemp;
		ARG						sSrc;
		FLOAT_SOURCE_MODIFIER	sSrcMod;
		IMG_UINT32				uHalf;
		PINST					psMadInst;
		IMG_UINT32				uPredSrc = USC_PREDREG_NONE;
		IMG_BOOL				bPredNegate = IMG_FALSE;

		if ((psInputInst->sDest.u.byMask & (1U << uChan)) == 0)
		{
			continue;
		}

		/*
			Get the register containing the integer value to be converted.
		*/
		GetSourceF32(psState, psCodeBlock, &psInputInst->asSrc[0], uChan, &sSrc, IMG_TRUE, &sSrcMod);

		/*
			Apply any absolute modifier to the source register.
		*/
		if (sSrcMod.bAbsolute)
		{
			IntegerAbsolute(psState, psCodeBlock, &sSrc, &sSrc);
		}
		/*
			Apply any negate modifier to the source register.
		*/
		if (sSrcMod.bNegate)
		{
			IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, &sSrc, &sSrc);
		}

		/*
			Get two registers containing each word of the source converted to F32.
		*/
		uPackTemp = GetNextRegisterCount(psState, 2);
		for (uHalf = 0; uHalf < 2; uHalf++)
		{
			PINST		psPackInst;

			psPackInst = AllocateInst(psState, IMG_NULL);
			if (eSrcFmt == UF_REGFORMAT_I32 && uHalf == 1)
			{
				SetOpcode(psState, psPackInst, IUNPCKF32S16);
			}
			else
			{
				SetOpcode(psState, psPackInst, IUNPCKF32U16);
			}
			psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asDest[0].uNumber = uPackTemp + uHalf;
			psPackInst->asArg[0] = sSrc;
			SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uHalf * 2);
			AppendInst(psState, psCodeBlock, psPackInst);
		}

		/*
			RESULT = LOW_HALF + HIGH_HALF * 65536
		*/
		psMadInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMadInst, IFMAD);
		if (bTempDest)
		{
			psMadInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psMadInst->asDest[0].uNumber = uTempDestStartNum + uChan;
		}
		else
		{
			GetDestinationF32(psState, psCodeBlock, &psInputInst->sDest, uChan, &psMadInst->asDest[0]);
			GetInputPredicate(psState, &uPredSrc, &bPredNegate, psInputInst->uPredicate, uChan);
			SetPredicate(psState, psMadInst, uPredSrc, bPredNegate);
		}
		psMadInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psMadInst->asArg[0].uNumber = uPackTemp + 1;
		psMadInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
		psMadInst->asArg[1].uNumber = uScaleFactorTemp;
		psMadInst->asArg[2].uType = USEASM_REGTYPE_TEMP;
		psMadInst->asArg[2].uNumber = uPackTemp + 0;

		AppendInst(psState, psCodeBlock, psMadInst);

		GenerateDestModF32(psState, psCodeBlock, &psMadInst->asDest[0], uSat, uScale, 	
						   uPredSrc, 
						   bPredNegate);
	}

	if (bTempDest)
	{
		MoveF32VectorToInputDest(psState, psCodeBlock, psInputInst, UFREG_SWIZ_NONE, uTempDestStartNum);
	}

	/* Store into a destination register that needs special handling. */
	StoreIntoSpecialDest(psState, psCodeBlock, psInputInst, &psInputInst->sDest);
}

static 
IMG_VOID ConvertReformatInstructionF32(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertReformatInstructionF32
    
 PURPOSE	: Convert an input MOV with different source and destination formats to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32		uChan;
	UF_REGFORMAT	eSrcFmt = GetRegisterFormat(psState, &psSrc->asSrc[0]);
	ARG				sC10Arg = USC_ARG_DEFAULT_VALUE;
	IMG_UINT32		uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32		uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32		uC10Swiz = UINT_MAX;
	IMG_BOOL		bTempDest;

	if (eSrcFmt == UF_REGFORMAT_U32 || eSrcFmt == UF_REGFORMAT_I32)
	{
		ConvertInteger32ToF32(psState, psCodeBlock, psSrc);
		return;
	}

	if (eSrcFmt == UF_REGFORMAT_C10 || eSrcFmt == UF_REGFORMAT_U8)
	{
		IMG_BOOL	bIgnoreSwiz;
		IMG_UINT32	uC10Mask;

		if (CanOverrideSwiz(&psSrc->asSrc[0]))
		{
			uC10Swiz = psSrc->asSrc[0].u.uSwiz;
			bIgnoreSwiz = IMG_TRUE;
			uC10Mask = SwizzleMask(uC10Swiz, psSrc->sDest.u.byMask);
		}
		else
		{	
			uC10Swiz = UFREG_SWIZ_NONE;
			bIgnoreSwiz = IMG_FALSE;
			uC10Mask = psSrc->sDest.u.byMask;
		}

		GetSourceC10(psState, psCodeBlock, &psSrc->asSrc[0], psSrc->asSrc[0].byMod, &sC10Arg, uC10Mask, 
			bIgnoreSwiz, IMG_FALSE, psSrc->asSrc[0].eFormat);
	}
	
	/*
		Use a temporary for the destination to avoid conversion instructions for earlier channels overwriting 
		source data for later channels.
	*/
	bTempDest = IMG_FALSE;
	if (psSrc->sDest.eType == psSrc->asSrc[0].eType &&
		psSrc->sDest.uNum == psSrc->asSrc[0].uNum)
	{
		bTempDest = IMG_TRUE;
	}

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		PINST		psPackInst;
		IMG_UINT32	uPredSrc = USC_PREDREG_NONE;
		IMG_BOOL	bPredNegate = IMG_FALSE;

		if ((psSrc->sDest.u.byMask & (1U << uChan)) == 0)
		{
			continue;
		}

		psPackInst = AllocateInst(psState, IMG_NULL);
		switch (eSrcFmt)
		{
			case UF_REGFORMAT_F16: 
			{
				SetOpcode(psState, psPackInst, IUNPCKF32F16);

				if (psState->uCompilerFlags & UF_OPENCL)
				{
					/* We're doing the pack here, OpenCL uses full F32 registers for F16 */
					psSrc->asSrc[0].eFormat = UF_REGFORMAT_F32;

					GetSourceF32(psState, 
						psCodeBlock, 
						&psSrc->asSrc[0], 
						uChan, 
						&psPackInst->asArg[0], 
						IMG_FALSE /* bAllowSourceMod */,
						NULL /* psSrcMod */);

					psSrc->asSrc[0].eFormat = UF_REGFORMAT_F16;
				}
				else
				{
					FLOAT_SOURCE_MODIFIER	sSrcMod;
					IMG_UINT32				uComponent;

					GetSingleSourceF16(psState, 
						psCodeBlock, 
						&psSrc->asSrc[0], 
						&psPackInst->asArg[0], 
						&uComponent,
						uChan, 
						IMG_TRUE, 
						&sSrcMod,
						IMG_FALSE);
					ApplyFloatSourceModifier(psState, 
						psCodeBlock, 
						NULL /* psInsertBeforeInst */,
						&psPackInst->asArg[0], 
						&psPackInst->asArg[0], 
						UF_REGFORMAT_F16, 
						&sSrcMod);
					SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uComponent);
				}
				break;
			}
			case UF_REGFORMAT_I16:
			case UF_REGFORMAT_U16:
			{
				SetOpcode(psState, psPackInst, eSrcFmt == UF_REGFORMAT_I16 ? IUNPCKF32S16 : IUNPCKF32U16);
				GetSourceF32(psState, 
							 psCodeBlock, 
							 &psSrc->asSrc[0], 
							 uChan, 
							 &psPackInst->asArg[0], 
							 IMG_FALSE /* bAllowSourceMod */,
							 NULL /* psSrcMod */);
				break;
			}
			case UF_REGFORMAT_C10:
			case UF_REGFORMAT_U8:
			{
				IMG_UINT32	uSwizzledChan = EXTRACT_CHAN(uC10Swiz, uChan);
				SetOpcode(psState, psPackInst, (sC10Arg.eFmt == UF_REGFORMAT_C10) ? IUNPCKF32C10 : IUNPCKF32U8);
				psPackInst->asArg[0] = sC10Arg;
				psPackInst->u.psPck->bScale = IMG_TRUE;
				if (uSwizzledChan <= UFREG_SWIZ_A)
				{
					SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, ConvertInputChannelToIntermediate(psState, uSwizzledChan));
				}
				break;
			}
			case UF_REGFORMAT_U8_UN:
			case UF_REGFORMAT_I8_UN:
			{
				SetOpcode(psState, psPackInst, eSrcFmt == UF_REGFORMAT_I8_UN ? IUNPCKF32S8 : IUNPCKF32U8);
				GetSourceF32(psState, 
							 psCodeBlock, 
							 &psSrc->asSrc[0], 
							 uChan, 
							 &psPackInst->asArg[0], 
							 IMG_FALSE /* bAllowSourceMod */,
							 NULL /* psSrcMod */);
				break;
			}
			default: imgabort();
		}

		if (bTempDest)
		{
			psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asDest[0].uNumber = USC_TEMPREG_TEMPDEST + uChan;
		}
		else
		{
			GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, uChan, &psPackInst->asDest[0]);
			GetInputPredicate(psState, &uPredSrc, &bPredNegate, psSrc->uPredicate, uChan);
			SetPredicate(psState, psPackInst, uPredSrc, bPredNegate);
		}

		AppendInst(psState, psCodeBlock, psPackInst);

		GenerateDestModF32(psState, psCodeBlock, &psPackInst->asDest[0], uSat, uScale, uPredSrc, bPredNegate);
	}

	if (bTempDest)
	{
		for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
		{
			if (psSrc->sDest.u.byMask & (1U << uChan))
			{
				PINST	psMoveInst;

				psMoveInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMoveInst, IMOV);
				GetDestinationF32(psState, psCodeBlock, &psSrc->sDest, uChan, &psMoveInst->asDest[0]);
				psMoveInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
				psMoveInst->asArg[0].uNumber = USC_TEMPREG_TEMPDEST + uChan;
				GetInputPredicateInst(psState, psMoveInst, psSrc->uPredicate, uChan);
				AppendInst(psState, psCodeBlock, psMoveInst);
			}
		}
	}

	/* Store into a destination register that needs special handling. */
	StoreIntoSpecialDest(psState, psCodeBlock, psSrc, &psSrc->sDest);
}

IMG_INTERNAL
IMG_VOID ConvertAddressValue(PINTERMEDIATE_STATE		psState,
							 PCODEBLOCK					psCodeBlock,
							 IMG_UINT32					uRegIndexDest,
							 PARG						psSrc,
							 IMG_UINT32					uSourceComponent,
							 IMG_BOOL					bU8Src,
							 IMG_UINT32					uPredSrc,
							 IMG_BOOL					bPredSrcNegate)
/*****************************************************************************
 FUNCTION	: ConvertAddressValue
    
 PURPOSE	: Convert an address value to integer.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uRegIndexDest		- Register to update with the address value.
			  psSrc				- 
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psInst;

	/*
		Convert the address value from the source format (F32, U16 or U8)
		to 16-bit integer.
	*/
	psInst = AllocateInst(psState, IMG_NULL);
	if (bU8Src)
	{
		SetOpcode(psState, psInst, IUNPCKU16U8);
		SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uSourceComponent);
	}
	else
	{	
		SetOpcode(psState, psInst, IPCKS16F32);
	}
	psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psInst->asDest[0].uNumber = uRegIndexDest;
	SetPredicate(psState, psInst, uPredSrc, bPredSrcNegate);
	psInst->asArg[0] = *psSrc;
	psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[1].uNumber = 0;
	
	AppendInst(psState, psCodeBlock, psInst);
}


IMG_INTERNAL 
IMG_VOID ConvertMovaInstructionFloat(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc, IMG_BOOL bFloat32)
/*****************************************************************************
 FUNCTION	: ConvertMovaInstructionFloat
    
 PURPOSE	: Convert an input MOVA to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  bFloat32			- Type of the MOVA source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < 4; uChan++)
	{
		if (psSrc->sDest.u.byMask & (1U << uChan))
		{
			ARG						sSrc;
			IMG_UINT32				uPredSrc;
			IMG_BOOL				bPredSrcNegate;
			FLOAT_SOURCE_MODIFIER	sMod;

			/*
				Get any predicate for this channel.
			*/
			GetInputPredicate(psState, &uPredSrc, &bPredSrcNegate, psSrc->uPredicate, uChan);

			/* Get the source for the address offset. */
			if (bFloat32)
			{
				GetSourceF32(psState, psCodeBlock, &psSrc->asSrc[0], uChan, &sSrc, IMG_TRUE, &sMod);
			}
			else
			{
				PINST		psPackInst;
				IMG_UINT32	uComponent;

				psPackInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psPackInst, IUNPCKF32F16);
				psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psPackInst->asDest[0].uNumber = GetNextRegister(psState);
				GetSingleSourceF16(psState, 
								   psCodeBlock, 
								   &psSrc->asSrc[0], 
								   &psPackInst->asArg[0], 
								   &uComponent,
								   uChan, 
								   IMG_TRUE, 
								   &sMod,
								   IMG_FALSE);
				SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uComponent);
				AppendInst(psState, psCodeBlock, psPackInst);

				sSrc = psPackInst->asDest[0];
			}

			/*
				Apply any source modifiers.
			*/
			ApplyFloatSourceModifier(psState, 
									 psCodeBlock, 
									 NULL /* psInsertBeforeInst */,
									 &sSrc, 
									 &sSrc, 
									 UF_REGFORMAT_F32, 
									 &sMod);

			/*
				Convert the address value to integer.
			*/
			ConvertAddressValue(psState, 
								psCodeBlock, 
								USC_TEMPREG_ADDRESS + uChan,
								&sSrc,
								0 /* uSourceComponent */,
								IMG_FALSE /* bU8Src */,
								uPredSrc,
								bPredSrcNegate);
		}
	}
}

IMG_INTERNAL 
PCODEBLOCK ConvertInstToIntermediateF32(PINTERMEDIATE_STATE psState, 
									  	PCODEBLOCK psCodeBlock, 
									  	PUNIFLEX_INST psSrc, 
									  	IMG_BOOL bConditional,
									  	IMG_BOOL bStaticCond)
/*****************************************************************************
 FUNCTION	: ConvertInstToIntermediate
    
 PURPOSE	: Convert an input instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  bConditional		- Whether the instruction is conditionally executed.
			  bStaticCond		- Whether condition is static
			  
 RETURNS	: None.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		psCodeBlock = ConvertInstToIntermediateF32_Vec(	psState,
														psCodeBlock,
														psSrc,
														bConditional,
														bStaticCond);
		return psCodeBlock;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	switch (psSrc->eOpCode)
	{
		/* Texture sampling instructions. */
		case UFOP_LD:
		case UFOP_LDB:
		case UFOP_LDD:
		case UFOP_LDL:
		case UFOP_LDP:
		case UFOP_LDC:
		case UFOP_LDCLZ:
		case UFOP_LDPIFTC:
		case UFOP_LD2DMS:
		case UFOP_LDGATHER4:
		{
			psCodeBlock = ConvertSamplerInstructionF32(psState, psCodeBlock, psSrc, 
											 bConditional && !bStaticCond);
			break;
		}

		case UFOP_LDBUF:
		{
			#if defined(OUTPUT_USPBIN)
			if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
			{
				UscFail(psState, UF_ERR_INVALID_OPCODE, "LDBUF not supported for USP Binary output.\n");
			}
			#endif /*  defined(OUTPUT_USPBIN) */
			#if defined(OUTPUT_USPBIN) && defined(OUTPUT_USCHW)
			else
			#endif /* defined(OUTPUT_USPBIN) && defined(OUTPUT_USCHW) */
			#if defined(OUTPUT_USCHW)
			{
				ConvertLoadBufferInstructionF32(psState, psCodeBlock, psSrc, 
											bConditional && !bStaticCond);
			}
			#endif /* defined(OUTPUT_USCHW) */

			break;
		}

		case UFOP_LDTILED:
		{
			#if defined(OUTPUT_USPBIN)
			if (psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
			{
				UscFail(psState, UF_ERR_INVALID_OPCODE, "LDTILED not supported for USP Binary output.\n");
			}
			#endif /*  defined(OUTPUT_USPBIN) */
			#if defined(OUTPUT_USPBIN) && defined(OUTPUT_USCHW)
			else
			#endif /* defined(OUTPUT_USPBIN) && defined(OUTPUT_USCHW) */
			#if defined(OUTPUT_USCHW)
			{
				ConvertLoadTiledInstructionF32(psState, psCodeBlock, psSrc);
			}
			#endif /* defined(OUTPUT_USCHW) */

			break;
		}

		/* Vector instructions. */
		case UFOP_ADD:
		case UFOP_MUL:
		case UFOP_MAD:
		case UFOP_FRC:
		case UFOP_DSX:
		case UFOP_DSY:
		case UFOP_MIN:
		case UFOP_MAX:
		case UFOP_MOV:
		case UFOP_RECIP:
		case UFOP_RSQRT:
		case UFOP_EXP:
		case UFOP_LOG:
		case UFOP_SQRT:
		case UFOP_MOV_CP:
		case UFOP_CEIL:
		case UFOP_TRC:
		case UFOP_RNE:
		case UFOP_RND:
		case UFOP_SETBGE:
		case UFOP_SETBNE:
		case UFOP_SETBEQ:
		case UFOP_SETBLT:
		{
			if (psSrc->eOpCode == UFOP_MOV && GetRegisterFormat(psState, &psSrc->asSrc[0]) != UF_REGFORMAT_F32)
			{
				ConvertReformatInstructionF32(psState, psCodeBlock, psSrc);
			}
			else
			{
				ConvertVectorInstructionF32(psState, psCodeBlock, psSrc);
			}
			break;
		}

		/* Dot products. */
		case UFOP_DOT2ADD:
		case UFOP_DOT3:
		case UFOP_DOT4:
		case UFOP_DOT4_CP:
		{
			if (psSrc->asSrc[0].eFormat == UF_REGFORMAT_U8 &&
				psSrc->asSrc[1].eFormat == UF_REGFORMAT_U8)
			{
				ConvertIntegerDot34(psState, psCodeBlock, psSrc);
			}
			else
			{
				ConvertDotProductInstructionF32(psState, psCodeBlock, psSrc);
			}
			break;
		}

		/* Vector comparison instructions. */
		case UFOP_CMP:
		case UFOP_CMPLT:
		case UFOP_MOVCBIT:
		{
			ConvertComparisonInstructionF32(psState, psCodeBlock, psSrc);
			break;
		}
		case UFOP_SETP:
		{
			ConvertSetpInstructionNonC10(psState, psCodeBlock, psSrc);
			break;
		}

		/* Logp instruction. */
		case UFOP_LOGP:
		{
			ConvertLogpInstructionF32(psState, psCodeBlock, psSrc);
			break;
		}

		/* Move to address register instruction. */
		case UFOP_MOVA_RNE:
		{
			ConvertMovaInstructionFloat(psState, psCodeBlock, psSrc, IMG_TRUE);
			break;
		}

		/* Kill instruction. */
		case UFOP_KPLT:
		case UFOP_KPLE:
		{
			ConvertTexkillInstructionFloat(psState, psCodeBlock, psSrc, bConditional, IMG_TRUE);
			break;
		}
		case UFOP_KILLNZBIT:
		{
			ConvertTexkillInstructionBit(psState, psCodeBlock, psSrc, bConditional);
			break;
		}

		/* Macro instruction. */
		case UFOP_SINCOS: 
		{
			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_SQRT_SIN_COS)
			{
				ConvertSincosF32HWSupport(psState, psCodeBlock, psSrc);
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				imgabort();
			}
			break;
		}

		/* Macro instruction. */
		case UFOP_SINCOS2: 
		{
			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_SQRT_SIN_COS)
			{
				ConvertSincos2F32HWSupport(psState, psCodeBlock, psSrc);
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				imgabort();
			}
			break;
		}

		case UFOP_NOP: break;

		#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
		case UFOP_NRM:
		{
			ConvertNormaliseF32(psState, psCodeBlock, psSrc);
			break;
		}
		#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

		/* Bitwise instructions. */
		case UFOP_AND:
		case UFOP_SHL:
		case UFOP_ASR:
		case UFOP_NOT:
		case UFOP_OR:
		case UFOP_SHR:
		case UFOP_XOR:
		{
			ConvertBitwiseInstructionF32(psState, psCodeBlock, psSrc);
			break;
		}
		case UFOP_MOVVI:
		{
			ConvertVertexInputInstructionTypeless(psState, psCodeBlock, psSrc);
			break;
		}

		default: imgabort();
	}
	return psCodeBlock;
}

/* EOF */
