/*************************************************************************
 * Name         : icvt_f32_vec.c
 * Title        : Pixel Shader Compiler
 * Author       : John Russell
 * Created      : Jan 2002
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
 *
 * Modifications:-
 * $Log: icvt_f32_vec.c $
 * ,
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "uscshrd.h"

#include "bitops.h"


#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

IMG_INTERNAL
IMG_UINT32 ConvertSwizzle(PINTERMEDIATE_STATE	psState,
						  IMG_UINT32			uInSwiz)
/*****************************************************************************
 FUNCTION	: ConvertSwizzle
    
 PURPOSE	: Convert a swizzle from the input format to the intermediate
			  format.

 PARAMETERS	: psState			- Compiler state.
			  uInSwiz			- Swizzle to convert.
			  
 RETURNS	: Converted swizzle..
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uOutSwiz;

	uOutSwiz = 0;
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		IMG_UINT32	uInChan = EXTRACT_CHAN(uInSwiz, uChan);
		IMG_UINT32	uOutChan;

		switch (uInChan)
		{
			case UFREG_SWIZ_X: uOutChan = USEASM_SWIZZLE_SEL_X; break;
			case UFREG_SWIZ_Y: uOutChan = USEASM_SWIZZLE_SEL_Y; break;
			case UFREG_SWIZ_Z: uOutChan = USEASM_SWIZZLE_SEL_Z; break;
			case UFREG_SWIZ_W: uOutChan = USEASM_SWIZZLE_SEL_W; break;
			case UFREG_SWIZ_0: uOutChan = USEASM_SWIZZLE_SEL_0; break;
			case UFREG_SWIZ_1: uOutChan = USEASM_SWIZZLE_SEL_1; break;
			case UFREG_SWIZ_2: uOutChan = USEASM_SWIZZLE_SEL_2; break;
			case UFREG_SWIZ_HALF: uOutChan = USEASM_SWIZZLE_SEL_HALF; break;
			default: imgabort();
		}
		uOutSwiz |= (uOutChan << (USEASM_SWIZZLE_FIELD_SIZE * uChan));
	}
	return uOutSwiz;
}

static
IMG_VOID StoreScalarDestinationF32(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psBlock,
								   PUNIFLEX_INST		psInputInst,
								   PUF_REGISTER			psInputDest,
								   IMG_UINT32			uTempVariable)
/*****************************************************************************
 FUNCTION	: StoreScalarDestinationF32
    
 PURPOSE	: Generate instructions to store into an input instruction
			  destination which is represented in the intermediate format
			  as a set of scalars.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the instructions to.
			  psInputInst		- Input instruction.
			  psInputDest		- Input destination.
			  uTempVariable		- Number of the vector temporary register
								containing the data to store.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		PINST		psPackInst;
		IMG_UINT32	uSrcChan;

		if ((psInputInst->sDest.u.byMask & (1 << uChan)) == 0)
		{
			continue;
		}

		psPackInst = AllocateInst(psState, NULL);
		SetOpcodeAndDestCount(psState, psPackInst, IUNPCKVEC, 1 /* uDestCount */);

		GetDestinationF32(psState, psBlock, psInputDest, uChan, &psPackInst->asDest[0]);

		GetInputPredicateInst(psState, psPackInst, psInputInst->uPredicate, uChan);

		switch (uChan)
		{
			case USC_RED_CHAN: psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X); break;
			case USC_GREEN_CHAN: psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Y, Y, Y, Y); break;
			case USC_BLUE_CHAN: psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Z, Z, Z, Z); break;
			case USC_ALPHA_CHAN: psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(W, W, W, W); break;
			default: imgabort();
		}
		psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psPackInst->asArg[0].uNumber = uTempVariable;
		psPackInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
		for (uSrcChan = 0; uSrcChan < 4; uSrcChan++)
		{
			psPackInst->asArg[1 + uSrcChan].uType = USC_REGTYPE_UNUSEDSOURCE;
		}
		AppendInst(psState, psBlock, psPackInst);
	}

	/*
		Generate extra instructions to store the instruction result into an indexable 
		temporary array.
	*/
	if (psInputDest->eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psBlock, psInputDest, UF_REGFORMAT_F32, USC_TEMPREG_F32INDEXTEMPDEST);
	}
}

static
IMG_VOID GetVertexShaderOutputArgument(PINTERMEDIATE_STATE	psState,
									   IMG_UINT32			uInputNum,
									   IMG_UINT32			uUsedChanMask,
									   PARG					psArg)
/*****************************************************************************
 FUNCTION	: GetVertexShaderOutputArgument
    
 PURPOSE	: Initialize an intermediate argument structure representing
			  a vertex shader output.

 PARAMETERS	: psState			- Compiler state.
			  uInputNum			- Register number of the vertex shader output
								in the input code.
			  uUsedChanMask		- Mask of channels read or written in the
								vertex shader output.
			  psArg				- On return initialized with the intermediate
								representation of the vertex shader output.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;
	IMG_UINT32			uChanIdx;
	IMG_UINT32			uTempReg;

	/*
		Get the temporary register representing this vertex shader output.
	*/
	if (psVS->auVSOutputToVecReg[uInputNum] == USC_UNDEF)
	{
		psVS->auVSOutputToVecReg[uInputNum] = psVS->uVertexShaderOutputsFirstRegNum + uInputNum;
	}
	uTempReg = psVS->auVSOutputToVecReg[uInputNum];

	/*
		Associate the same temporary register with the other components used together as a vector.
	*/
	ASSERT(uInputNum == 0 || psVS->auVSOutputToVecReg[uInputNum - 1] != uTempReg);
	for (uChanIdx = 0; uChanIdx < CHANNELS_PER_INPUT_REGISTER; uChanIdx++)
	{
		IMG_UINT32	uOutRegNum;

		if ((uUsedChanMask >> uChanIdx) == 0)
		{
			break;
		}
		uOutRegNum = uInputNum + uChanIdx;
		ASSERT(psVS->auVSOutputToVecReg[uOutRegNum] == USC_UNDEF || psVS->auVSOutputToVecReg[uOutRegNum] == uTempReg);
		psVS->auVSOutputToVecReg[uOutRegNum] = uTempReg;
	}

	InitInstArg(psArg);
	psArg->uType = USEASM_REGTYPE_TEMP;
	psArg->uNumber = uTempReg;
}

IMG_INTERNAL
IMG_BOOL GetBaseDestinationF32_Vec(PINTERMEDIATE_STATE	psState,
								   PUF_REGISTER			psInputDest,
								   PARG					psDest)
/*****************************************************************************
 FUNCTION	: GetBaseDestinationF32_Vec
    
 PURPOSE	: Convert an input destination to the equivalent intermediate register.

 PARAMETERS	: psState			- Compiler state.
			  psInputDest		- Input destination.
			  psDest			- On return written with the intermediate
								destination.
			  
 RETURNS	: None.
*****************************************************************************/
{
	if (psInputDest->eType == UFREG_TYPE_TEMP)
	{
		psDest->uType = USEASM_REGTYPE_TEMP;
		psDest->uNumber = ConvertTempRegisterNumberF32(psState, psInputDest->uNum, 0);
		return IMG_TRUE;
	}
	else if (psInputDest->eType == UFREG_TYPE_PSOUTPUT)
	{
		ConvertPixelShaderResultArg(psState, psInputDest, psDest);
		return IMG_TRUE;
	}
	else if (psInputDest->eType == UFREG_TYPE_VSOUTPUT && !(psState->uFlags & USC_FLAGS_OUTPUTRELATIVEADDRESSING))
	{
		GetVertexShaderOutputArgument(psState, psInputDest->uNum, psInputDest->u.byMask, psDest);
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID StoreDestinationF32(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psBlock,
							 PUNIFLEX_INST			psInputInst,
							 PUF_REGISTER			psInputDest,
							 IMG_UINT32				uTempVariable)
/*****************************************************************************
 FUNCTION	: StoreDestinationF32
    
 PURPOSE	: Generate any extra instructions needed to update the destination
			  of an input instruction.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the intermediate instructions to.
			  psInputInst		- Input instruction.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uInputPredicate;
	
	uInputPredicate = psInputInst->uPredicate;

	/*
		For the pixel shader depth output
		and (unpacked) pixel shader colour outputs generate 
		instructions to unpack from the vector
		temporary register we used as the destination of the main
		instruction to the individual scalar destinations.
	*/
	if (
			psInputDest->eType == UFREG_TYPE_INDEXABLETEMP ||
			(
				psInputDest->eType == UFREG_TYPE_VSOUTPUT && 
				(psState->uFlags & USC_FLAGS_OUTPUTRELATIVEADDRESSING)
			)
	   )
	{
		StoreScalarDestinationF32(psState, psBlock, psInputInst, psInputDest, uTempVariable);
	}
}

IMG_INTERNAL
IMG_VOID GetDestinationF32_Vec(PINTERMEDIATE_STATE		psState,
							   PUF_REGISTER				psInputDest,
							   PARG						psDest)
/*****************************************************************************
 FUNCTION	: GetDestinationF32_Vec
    
 PURPOSE	: Convert an input instruction destination to an intermediate
			  instruction destination.

 PARAMETERS	: psState			- Compiler state.
			  psInputDest		- Destination of the input instruction.
			  psDest			- Updated with the intermediate format 
								destination.
			  
 RETURNS	: None.
*****************************************************************************/
{
	InitInstArg(psDest);

	/*
		Set the format for the destination register.
	*/
	psDest->eFmt = UF_REGFORMAT_F32;

	/*
		Check if we can write directly to the final destination.
	*/
	if (GetBaseDestinationF32_Vec(psState, psInputDest, psDest))
	{
		return;
	}

	/*
		Write the instruction result to a temporary location. Later we'll emit
		instructions to write to the real destination.
	*/
	psDest->uType = USEASM_REGTYPE_TEMP;
	psDest->uNumber = USC_TEMPREG_CVTVECF32DSTOUT;
}

static
IMG_VOID GetNonTempSourceF32_Vec(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psBlock,
								 PUF_REGISTER			psInputSrc,
								 IMG_UINT32				uChan,
								 PARG					psUnifiedStoreSrc)
/*****************************************************************************
 FUNCTION	: GetNonTempSourceF32_Vec
    
 PURPOSE	: Convert a channel of a non-temporary input instruction source to 
			  an intermediate instruction source.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Basic block to add any new instructions to.
			  psInputSrc		- Source of the input instruction.
			  uChan				- Channel to convert.
			  psUnifiedStoreSrc
								- On return updated with the converted source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	switch (psInputSrc->eType)
	{
		case UFREG_TYPE_HW_CONST:
		{
			IMG_FLOAT	fValue;

			switch (psInputSrc->uNum)
			{
				case HW_CONST_0: fValue = 0.0f; break;
				case HW_CONST_1: fValue = 1.0f; break;
				case HW_CONST_2: fValue = 2.0f; break;
				case HW_CONST_HALF: fValue = 0.5f; break;
				default: imgabort();
			}

			psUnifiedStoreSrc->uType = USEASM_REGTYPE_IMMEDIATE;
			psUnifiedStoreSrc->uNumber = *(IMG_PUINT32)&fValue;
			break;
		}
		case UFREG_TYPE_VSOUTPUT:
		{
			GetVSOutputSource(psState, psUnifiedStoreSrc, psInputSrc, uChan, psBlock);
			break;
		}
		case UFREG_TYPE_INDEXABLETEMP:
		{
			psUnifiedStoreSrc->uType = USEASM_REGTYPE_TEMP;
			psUnifiedStoreSrc->uNumber = GetNextRegister(psState);

			/*
				Generate instructions to load from the indexable temporary
				array into the allocated variable.
			*/
			LoadStoreIndexableTemp(psState, 
								   psBlock, 
								   IMG_TRUE /* bLoad */, 
								   psInputSrc->eFormat, 
								   psInputSrc, 
								   (1 << uChan), 
								   psUnifiedStoreSrc->uNumber, 
								   0 /* uDataReg */);
			break;
		}
		case UFREG_TYPE_IMMEDIATE:
		{
			psUnifiedStoreSrc->uType = USEASM_REGTYPE_IMMEDIATE;
			psUnifiedStoreSrc->uNumber = psInputSrc->uNum;
			break;
		}
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_VOID CopyVecSourceModifiers(PUF_REGISTER			psInputSrc,
							    PVEC_SOURCE_MODIFIER	psSourceMod)
/*****************************************************************************
 FUNCTION	: CopyVecSourceModifiers
    
 PURPOSE	: Convert source modifiers from the input format to the
			  intermediate format.

 PARAMETERS	: psInputSrc		- Source of the input instruction.
			  psSourceMod		- Destination for the converted source
								modifiers.
			  
 RETURNS	: None.
*****************************************************************************/
{
	/*
		Copy input source modifiers to the intermediate instruction.
	*/
	if (psInputSrc->byMod & UFREG_SOURCE_ABS)
	{
		psSourceMod->bAbs = IMG_TRUE;
	}
	if (psInputSrc->byMod & UFREG_SOURCE_NEGATE)
	{
		psSourceMod->bNegate = IMG_TRUE;
	}
}

static
IMG_VOID GetConstSourceF32_Vec(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psBlock,
							   PUF_REGISTER			psInputSrc,
							   PARG					psGPISrc)
/*****************************************************************************
 FUNCTION	: GetConstSourceF32_Vec
    
 PURPOSE	: Convert an input constant register source to the intermediate
			  format.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the intermediate instructions to.
			  psInputSrc		- Input format source.
			  psGPISrc			- Vector intermediate source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	LoadConstantNoHWReg(psState,
						psBlock,
						NULL /* psInsertBeforeInst */,
						psInputSrc,
						0 /* uSrcChan */,
						psGPISrc,
						NULL /* puComponent */,
						UF_REGFORMAT_F32,
						IMG_TRUE /* bVectorResult */,
						IMG_FALSE /* bC10Subcomponent */,
						0 /* uCompOffset */);
}

IMG_INTERNAL
IMG_VOID GetSourceF32_Vec(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psBlock,
						  PUF_REGISTER			psInputSrc,
						  IMG_UINT32			uUsedChanMask,
						  PINST					psInst,
						  IMG_UINT32			uSrcIdx)
/*****************************************************************************
 FUNCTION	: GetSourceF32_Vec
    
 PURPOSE	: Convert an input instruction source to an intermediate
			  instruction source.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add an extra instructions to.
			  psInputSrc		- Source of the input instruction.
			  psSrc				- Updated with the intermediate format 
								source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32				uComp;
	IMG_UINT32				uSMod = (psInputSrc->byMod & UFREG_SMOD_MASK) >> UFREG_SMOD_SHIFT;
	IMG_UINT32				uBaseArgIdx = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR	;
	PARG					psGPISrc = &psInst->asArg[uBaseArgIdx + 0];
	PARG					asUnifiedStoreSrc = &psInst->asArg[uBaseArgIdx + 1];
	PVEC_SOURCE_MODIFIER	psSourceMod = &psInst->u.psVec->asSrcMod[uSrcIdx];
	PUF_REGFORMAT			peSourceFmt = &psInst->u.psVec->aeSrcFmt[uSrcIdx];

	/*
		Initialize the returned data to default values.
	*/
	InitInstArg(psGPISrc);
	psGPISrc->uType = USC_REGTYPE_UNUSEDSOURCE;
	for (uComp = 0; uComp < 4; uComp++)
	{
		InitInstArg(&asUnifiedStoreSrc[uComp]);
		asUnifiedStoreSrc[uComp].uType = USC_REGTYPE_UNUSEDSOURCE;
	}

	*peSourceFmt = UF_REGFORMAT_F32;

	if (psInputSrc->eType == UFREG_TYPE_TEMP)
	{
		psGPISrc->uType = USEASM_REGTYPE_TEMP;
		psGPISrc->uNumber = ConvertTempRegisterNumberF32(psState, psInputSrc->uNum, 0);
		psGPISrc->eFmt = UF_REGFORMAT_F32;
	}
	else if (psInputSrc->eType == UFREG_TYPE_CONST)
	{
		GetConstSourceF32_Vec(psState, psBlock, psInputSrc, psGPISrc);
	}
	else if (psInputSrc->eType == UFREG_TYPE_PSOUTPUT)
	{
		ConvertPixelShaderResultArg(psState, psInputSrc, psGPISrc);
		psGPISrc->eFmt = UF_REGFORMAT_F32;
	}
	else if (psInputSrc->eType == UFREG_TYPE_VSINPUT)
	{
		PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;
		PFIXED_REG_DATA		psVSInputs;

		ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX ||
			   psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY);
#ifdef NON_F32_VERTEX_INPUT
		/*
			Check input has been created correctly
		*/
		psVSInputs = psVS->aVertexShaderInputsFixedRegs[0];
		ASSERT(psVS->uVertexShaderInputCount == 1);
		ASSERT(psVSInputs->sPReg.uType == USEASM_REGTYPE_PRIMATTR);
		ASSERT(psVSInputs->uConsecutiveRegsCount == EURASIA_USE_PRIMATTR_BANK_SIZE); 
#else
		psVSInputs = psVS->psVertexShaderInputsFixedReg;
#endif
		if (psVSInputs->uRegArrayIdx == USC_UNDEF)
		{
			ASSERT(psInputSrc->uNum < psVSInputs->uConsecutiveRegsCount);

			psGPISrc->uType = USEASM_REGTYPE_TEMP; 
			psGPISrc->uNumber = psVSInputs->auVRegNum[psInputSrc->uNum];
		}
		else
		{
			psGPISrc->uType = USC_REGTYPE_REGARRAY; 
			psGPISrc->uNumber = psVSInputs->uRegArrayIdx;
			psGPISrc->uArrayOffset = psInputSrc->uNum * SCALAR_REGISTERS_PER_F32_VECTOR;
			GetRelativeIndex(psState, psInputSrc, psGPISrc);
		}
	}
	else if (psInputSrc->eType == UFREG_TYPE_TEXCOORD ||
			 psInputSrc->eType == UFREG_TYPE_TEXCOORDP ||
			 psInputSrc->eType == UFREG_TYPE_TEXCOORDPIFTC ||
			 psInputSrc->eType == UFREG_TYPE_COL ||
			 psInputSrc->eType == UFREG_TYPE_MISC)
	{
		if (	
				psInputSrc->eType == UFREG_TYPE_MISC && 
				(
					psInputSrc->uNum == UF_MISC_FACETYPE ||
					psInputSrc->uNum == UF_MISC_FACETYPEBIT
				)
			)
		{
			PINST		psMoveInst;
			IMG_UINT32	uBackfaceTemp = GetNextRegister(psState);

			/*
				PREDICATE = True if the triangle is backface.
			*/
			CheckFaceType(psState, psBlock, USC_PREDREG_TEMP, IMG_FALSE /* bFrontFace */); 

			/*
				Make the source 1.0f/0xFFFFFFFF if the triangle is frontface
			*/
			psMoveInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMoveInst, IVMOV);
			psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psMoveInst->asDest[0].uNumber = uBackfaceTemp;
			if (psInputSrc->uNum == UF_MISC_FACETYPE)
			{
				SetupImmediateSource(psMoveInst, 0 /* uArgIdx */, 1.0f, UF_REGFORMAT_F32);
			}
			else
			{
				SetupImmediateSourceU(psMoveInst, 0 /* uArgIdx */, 0xFFFFFFFF /* uImmValue */, UF_REGFORMAT_F32);
			}
			AppendInst(psState, psBlock, psMoveInst);

			/*
				...or -1.0f/00000000 if the triangle is backface.
			*/
			psMoveInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psMoveInst, IVMOV);
			SetPredicate(psState, psMoveInst, USC_PREDREG_TEMP, IMG_FALSE /* bPredNegate */);
			psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psMoveInst->asDest[0].uNumber = uBackfaceTemp;
			if (psInputSrc->uNum == UF_MISC_FACETYPE)
			{
				SetupImmediateSource(psMoveInst, 0 /* uArgIdx */, 1.0f, UF_REGFORMAT_F32);
				psMoveInst->u.psVec->asSrcMod[0].bNegate = IMG_TRUE;
			}
			else
			{
				SetupImmediateSourceU(psMoveInst, 0 /* uArgIdx */, 0 /* uImmValue */, UF_REGFORMAT_F32);
			}
			AppendInst(psState, psBlock, psMoveInst);

			psGPISrc->uType = USEASM_REGTYPE_TEMP;
			psGPISrc->uNumber = uBackfaceTemp;
			psGPISrc->uIndexType = USC_REGTYPE_NOINDEX;
			psGPISrc->uIndexNumber = USC_UNDEF;
			psGPISrc->uIndexArrayOffset = USC_UNDEF;
			psGPISrc->eFmt = psInputSrc->eFormat;
		}
		else
		{
			GetIteratedSource_Vec(psState, psBlock, psInputSrc, psGPISrc);
			*peSourceFmt = psGPISrc->eFmt;
		}
	}
	else if ((psInputSrc->eType == UFREG_TYPE_VSOUTPUT) && !(psState->uFlags & USC_FLAGS_OUTPUTRELATIVEADDRESSING))
	{
		GetVertexShaderOutputArgument(psState, psInputSrc->uNum, uUsedChanMask, psGPISrc);
	}
	else
	{
		for (uComp = 0; uComp < 4; uComp++)
		{
			GetNonTempSourceF32_Vec(psState, psBlock, psInputSrc, uComp, &asUnifiedStoreSrc[uComp]);
		}
	}

	/*
		Generate instructions to apply DirectX type source modifiers.
	*/
	if (uSMod != UFREG_SMOD_NONE)
	{
		GenerateSrcModF32F16_Vec(psState, psBlock, uSMod, psGPISrc, asUnifiedStoreSrc, IMG_FALSE /* bF16 */);
	}

	/*
		Copy NEGATE/ABSOLUTE source modifiers to the intermediate instruction.
	*/
	if (psSourceMod != NULL)
	{
		CopyVecSourceModifiers(psInputSrc, psSourceMod);
	}
}

IMG_INTERNAL
IMG_VOID ConvertIntegerDotProductResult_Vec(PINTERMEDIATE_STATE	psState,
											PCODEBLOCK			psCodeBlock,
											PUNIFLEX_INST		psInputInst,
											IMG_UINT32			uDotProduct,
											IMG_BOOL			bSat)
/*****************************************************************************
 FUNCTION	: ConvertIntegerDotProductResult_Vec
    
 PURPOSE	: Generate instructions for converting the result of an integer
			  dotproduct instruction.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add an extra instructions to.
			  psInputInst		- Input instruction.
			  uDotProduct		- Number of the temporary register containing
								the integer dotproduct result.
			  bSat				- TRUE if the result of the dotproduct must
								be saturated.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psPckInst;
	PINST		psMovInst;
	IMG_UINT32	uDestMask = psInputInst->sDest.u.byMask;
	IMG_UINT32	uConvertTemp = GetNextRegister(psState);

	/*
		Convert the dotproduct result to F32 format.
	*/
	psPckInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psPckInst, IVPCKFLTS16);
	psPckInst->u.psVec->bPackScale = IMG_TRUE;
	psPckInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psPckInst->asDest[0].uNumber = uConvertTemp;
	psPckInst->asDest[0].eFmt = UF_REGFORMAT_F32;
	psPckInst->auDestMask[0] = USC_RED_CHAN_MASK;
	psPckInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psPckInst->asArg[0].uNumber = uDotProduct;
	psPckInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	AppendInst(psState, psCodeBlock, psPckInst);

	/*
		Move the F32 dotproduct result into the destination vector.
	*/
	psMovInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psMovInst, IVMOV);
	GetDestinationF32_Vec(psState, &psInputInst->sDest, &psMovInst->asDest[0]);
	psMovInst->auDestMask[0] = uDestMask;
	psMovInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psMovInst->asArg[0].uNumber = uConvertTemp;
	psMovInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
	psMovInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
	AppendInst(psState, psCodeBlock, psMovInst);

	/*
		Saturate the destination vector.
	*/
	if (bSat)
	{
		/* DEST = MAX(DEST, 0.0f) */
		GenerateMinMax(psState, psCodeBlock, psInputInst, uDestMask, UFOP_MAX, UF_REGFORMAT_F32, 0.0f);
	}

	/*
		Do any extra work to store into a non-temporary input destination.
	*/
	StoreDestinationF32(psState, psCodeBlock, psInputInst, &psInputInst->sDest, USC_TEMPREG_CVTVECF32DSTOUT);
}

IMG_INTERNAL 
PCODEBLOCK ConvertInstToIntermediateF32_Vec(	PINTERMEDIATE_STATE psState, 
												PCODEBLOCK psCodeBlock, 
												PUNIFLEX_INST psInputInst, 
												IMG_BOOL bConditional,
												IMG_BOOL bStaticCond)
{
	PVR_UNREFERENCED_PARAMETER(bStaticCond);

	switch (psInputInst->eOpCode)
	{
		/* Texture sampling instructions. */
		case UFOP_LD:
		case UFOP_LDB:
		case UFOP_LDD:
		case UFOP_LDL:
		case UFOP_LDP:
		case UFOP_LDPIFTC:
		case UFOP_LDC:
		case UFOP_LDCLZ:
		case UFOP_LD2DMS:
		case UFOP_LDGATHER4:
		{
			psCodeBlock = ConvertSamplerInstructionFloatVec(psState, psCodeBlock, psInputInst, bConditional, IMG_FALSE /* bF16 */);
			break;
		}

		case UFOP_ADD:
		case UFOP_MUL:
		case UFOP_MAD:
		case UFOP_MIN:
		case UFOP_MAX:
		case UFOP_FRC:
		case UFOP_DSX:
		case UFOP_DSY:
		case UFOP_CMP:
		case UFOP_CMPLT:
		case UFOP_CEIL:
		case UFOP_TRC:
		case UFOP_SETBGE:
		case UFOP_SETBNE:
		case UFOP_SETBEQ:
		case UFOP_SETBLT:
		case UFOP_MOVCBIT:
		case UFOP_SLT:
		case UFOP_SGE:
		case UFOP_SETP:
		{
			ConvertVectorInstructionF32F16_Vec(psState, psCodeBlock, psInputInst, IMG_FALSE /* bF16 */);
			break;
		}

		case UFOP_DOT2ADD:
		{
			ConvertDOT2ADDInstructionF32F16_Vec(psState, psCodeBlock, psInputInst, IMG_FALSE /* bF16 */);
			break;
		}

		case UFOP_DOT3:
		case UFOP_DOT4:
		{
			if (psInputInst->asSrc[0].eFormat == UF_REGFORMAT_U8 &&
				psInputInst->asSrc[1].eFormat == UF_REGFORMAT_U8)
			{
				ConvertIntegerDot34(psState, psCodeBlock, psInputInst);
			}
			else
			{
				ConvertVectorInstructionF32F16_Vec(psState, psCodeBlock, psInputInst, IMG_FALSE /* bF16 */);
			}
			break;
		}

		case UFOP_DOT4_CP:
		case UFOP_MOV_CP:
		{
			ConvertVectorInstructionF32F16_Vec(psState, psCodeBlock, psInputInst, IMG_FALSE /* bF16 */);
			break;
		}

		case UFOP_RECIP:
		case UFOP_RSQRT:
		case UFOP_EXP:
		case UFOP_LOG:
		{
			ConvertComplexOpInstructionF32F16_Vec(psState, psCodeBlock, psInputInst, IMG_FALSE /* bF16 */);
			break;
		}

		case UFOP_MOV:
		{
			UF_REGFORMAT	eDestFormat = GetRegisterFormat(psState, &psInputInst->sDest);
			UF_REGFORMAT	eSrcFormat = GetRegisterFormat(psState, &psInputInst->asSrc[0]);

			if (eDestFormat == eSrcFormat)
			{
				ConvertVectorInstructionF32F16_Vec(psState, psCodeBlock, psInputInst, IMG_FALSE /* bF16 */);
			}
			else
			{
				ConvertReformatInstructionF16F32_Vec(psState, psCodeBlock, psInputInst, IMG_FALSE /* bF16 */);
			}
			break;
		}

		/* Move to address register instruction. */
		case UFOP_MOVA_RNE:
		{
			ConvertMovaInstructionVecFloat(psState, psCodeBlock, psInputInst, IMG_FALSE /* bF16 */);
			break;
		}

		/* Kill instruction. */
		case UFOP_KPLT:
		case UFOP_KPLE:
		{
			ConvertTexkillInstructionFloatVec(psState, psCodeBlock, psInputInst,  bConditional);
			break;
		}
		
		case UFOP_KILLNZBIT:
		{
			UscFail(psState, UF_ERR_INVALID_OPCODE, "KPNZBIT is currently not supported for this core.\n");
			break;
		}

		/* Bitwise instructions. */
		case UFOP_AND:
		case UFOP_SHL:
		case UFOP_ASR:
		case UFOP_NOT:
		case UFOP_OR:
		case UFOP_SHR:
		case UFOP_XOR:
		{
			ConvertBitwiseInstructionF32(psState, psCodeBlock, psInputInst);
			break;
		}

		case UFOP_MOVVI:
		{
			UscFail(psState, UF_ERR_GENERIC, "Geometry shaders aren't supported on this core.\n");
			break;
		}
		
		case UFOP_NOP: break;

		default:
		{
			imgabort();
			break;
		}
	}
	return psCodeBlock;
}

#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

/* EOF */
