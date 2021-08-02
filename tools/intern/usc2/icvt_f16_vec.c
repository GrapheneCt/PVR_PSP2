/*************************************************************************
 * Name         : icvt_f16_vec.c
 * Title        : UNIFLEX-to-intermediate code conversion for F16 data format
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
 * $Log: icvt_f16_vec.c $
 * 
 *  --- Revision Logs Removed --- 
 **************************************************************************/

#include "uscshrd.h"
#include "sgxformatconvert.h"

#include "bitops.h"

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)

IMG_INTERNAL
IMG_VOID SetupVectorSource(PINTERMEDIATE_STATE	psState,
						   PINST				psInst,
						   IMG_UINT32			uSrcIdx,
						   PARG					psSrc)
{
	IMG_UINT32	uChan;
	IMG_UINT32	uArgBase = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR;

	SetSrcFromArg(psState, psInst, uArgBase + 0, psSrc);

	for (uChan = 0; uChan < 4; uChan++)
	{
		SetSrcUnused(psState, psInst, uArgBase + 1 + uChan);
	}

	psInst->u.psVec->aeSrcFmt[uSrcIdx] = psSrc->eFmt;
}

IMG_INTERNAL
IMG_VOID SetupTempVecSource(PINTERMEDIATE_STATE	psState,
							PINST				psInst,
						    IMG_UINT32			uSrcIdx,
							IMG_UINT32			uTempNum,
							UF_REGFORMAT		eTempFormat)
{
	ARG	sTempSrc;

	MakeArg(psState, USEASM_REGTYPE_TEMP, uTempNum, eTempFormat, &sTempSrc);
	SetupVectorSource(psState, psInst, uSrcIdx, &sTempSrc);
}

static
IMG_VOID StoreDestinationF16(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psBlock,
							 PUNIFLEX_INST			psInputInst,
							 PUF_REGISTER			psInputDest)
/*****************************************************************************
 FUNCTION	: StoreDestinationF16
    
 PURPOSE	: Generate any extra instructions needed to update the destination
			  of an input instruction.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the intermediate instructions to.
			  psInputInst		- Input instruction.
			  psInputDest		- Input destination.
			  
 RETURNS	: None.
*****************************************************************************/
{
	/*
		Check for an output which is always written with F32 data.
	*/
	if (psInputInst->sDest.eType == UFREG_TYPE_VSOUTPUT)
	{
		IMG_UINT32	uConvertTemp;
		PINST		psPackInst;
		IMG_UINT32	uChan;
		IMG_BOOL	bDirectToDest;

		uConvertTemp = GetNextRegister(psState);

		bDirectToDest = IMG_FALSE;
		if ((psState->uFlags & USC_FLAGS_OUTPUTRELATIVEADDRESSING) == 0)
		{
			bDirectToDest = IMG_TRUE;
		}

		/*
			Convert the instruction's result from F16 to F32 format.
		*/
		psPackInst = AllocateInst(psState, NULL);
		SetOpcode(psState, psPackInst, IVMOV);
		if (bDirectToDest)
		{
			GetDestinationF32_Vec(psState, &psInputInst->sDest, &psPackInst->asDest[0]);
		}
		else
		{
			psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asDest[0].uNumber = uConvertTemp;
			psPackInst->asDest[0].eFmt = UF_REGFORMAT_F32;
		}
		psPackInst->auDestMask[0] = psInputDest->u.byMask;
		psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psPackInst->asArg[0].uNumber = USC_TEMPREG_CVTVECF16DSTOUT;
		psPackInst->asArg[0].eFmt = UF_REGFORMAT_F16;
		for (uChan = 0; uChan < CHANS_PER_REGISTER; uChan++)
		{
			psPackInst->asArg[1 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
		}
		psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psPackInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F16;
		AppendInst(psState, psBlock, psPackInst);

		/*
			Generate extra instructions to store the F32 format result into
			the destination.
		*/
		if (!bDirectToDest)
		{
			StoreDestinationF32(psState, psBlock, psInputInst, psInputDest, uConvertTemp);
		}

		return;
	}
	if (psInputInst->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		IMG_UINT32	uRegIdx;

		/*
			Unpack from the vector result of the instructions to a set of
			scalar registers.
		*/
		for (uRegIdx = 0; uRegIdx < SCALAR_REGISTERS_PER_F16_VECTOR; uRegIdx++)
		{
			PINST		psUnpckInst;
			IMG_UINT32	uSrcChan;

			psUnpckInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psUnpckInst, IUNPCKVEC);
			psUnpckInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psUnpckInst->asDest[0].uNumber = USC_TEMPREG_F16INDEXTEMPDEST + uRegIdx;
			psUnpckInst->asDest[0].eFmt = UF_REGFORMAT_F16;
			psUnpckInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psUnpckInst->asArg[0].uNumber = USC_TEMPREG_CVTVECF16DSTOUT;
			psUnpckInst->asArg[0].eFmt = UF_REGFORMAT_F16;
			psUnpckInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F16;
			switch (uRegIdx)
			{
				case 0: psUnpckInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, X, X); break;
				case 1: psUnpckInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Z, W, X, X); break;
			}
			for (uSrcChan = 0; uSrcChan < 4; uSrcChan++)
			{
				psUnpckInst->asArg[1 + uSrcChan].uType = USC_REGTYPE_UNUSEDSOURCE;
			}
			AppendInst(psState, psBlock, psUnpckInst);
		}

		/*
			Generate instructions to store the instruction result into the indexable 
			temporary array.
		*/
		StoreIndexableTemp(psState, psBlock, psInputDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);

		return;
	}
}

static
IMG_VOID GetDestinationF16_Vec(PINTERMEDIATE_STATE		psState,
							   PUF_REGISTER				psInputDest,
							   PARG						psDest)
/*****************************************************************************
 FUNCTION	: GetDestinationF16_Vec
    
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
	psDest->eFmt = UF_REGFORMAT_F16;

	/*
		Check for destinations we can write directly with F16 data.
	*/
	if (psInputDest->eType == UFREG_TYPE_TEMP)
	{
		psDest->uType = USEASM_REGTYPE_TEMP;
		psDest->uNumber = ConvertTempRegisterNumberF32(psState, psInputDest->uNum, 0);
		return;
	}
	if (psInputDest->eType == UFREG_TYPE_PSOUTPUT)
	{
		ConvertPixelShaderResultArg(psState, psInputDest, psDest);
		psDest->eFmt = UF_REGFORMAT_F16;
		return;
	}
	
	/*
		Write the instruction result to a temporary location. Later we'll emit
		instructions to write to the real destination.
	*/
	psDest->uType = USEASM_REGTYPE_TEMP;
	psDest->uNumber = USC_TEMPREG_CVTVECF16DSTOUT;
}

static
IMG_VOID GetVectorPredicateInst(PINTERMEDIATE_STATE psState, PINST psInst, PUNIFLEX_INST psInputInst)
/*****************************************************************************
 FUNCTION	: GetVectorPredicateInst
    
 PURPOSE	: Convert an input predicate to the intermediate form.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to apply the intermediate predicate
								to.
			  uPredicate		- Input predicate.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uPredicate = psInputInst->uPredicate;

	if ((uPredicate & UF_PRED_COMP_MASK) == UF_PRED_XYZW)
	{
		if (g_abSingleBitSet[psInputInst->sDest.u.byMask])
		{
			GetInputPredicateInst(psState, psInst, uPredicate, g_aiSingleComponent[psInputInst->sDest.u.byMask]);
		}
		else
		{
			IMG_UINT32	uChan;

			/*
				Set a vector of four predicates - each controlling the update of
				the corresponding channel of the destination.
			*/
			MakePredicatePerChan(psState, psInst);
			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				IMG_UINT32	uPredSrc;
				IMG_BOOL	bPredNegate;

				GetInputPredicate(psState, &uPredSrc, &bPredNegate, uPredicate, uChan);
				SetPredicateAtIndex(psState, psInst, uPredSrc, bPredNegate, uChan);
			}
		}
	}
	else
	{
		/*
			Otherwise use the general predicate conversion function.
		*/
		GetInputPredicateInst(psState, psInst, uPredicate, USC_RED_CHAN);
	}
}

static
IMG_VOID SetupImmediateSourcesU(IMG_UINT32		uImmValue,
								UF_REGFORMAT	eImmFormat,
								IMG_UINT32		uChanCount,
							    PARG			asArg)
/*****************************************************************************
 FUNCTION	: SetupImmediateSourcesU
    
 PURPOSE	: Set up an array of instruction sources to reference a vector of a repeated
			  immediate value.

 PARAMETERS	: uImmValue		- Immediate value.
			  eImmFormat	- Format of the immediate data.
			  uChanCount	- Count of channels in the vector.
			  asArg			- Array of arguments to set up.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < uChanCount; uChan++)
	{
		asArg[uChan].uType = USEASM_REGTYPE_IMMEDIATE;
		asArg[uChan].uNumber = uImmValue;
		asArg[uChan].eFmt = eImmFormat;
	}
}

static
IMG_VOID SetupImmediateSources(IMG_FLOAT	fImmValue,
							   UF_REGFORMAT	eFormat,
							   PARG			asArg)
/*****************************************************************************
 FUNCTION	: SetupImmediateSources
    
 PURPOSE	: Set up an array of instruction sources to reference a vector of a repeated
			  immediate value.

 PARAMETERS	: fImmValue		- Immediate value.
			  eFormat		- Format to convert the immediate value to.a
			  asArg			- Array of arguments to set up.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChanCount;
	IMG_UINT32	uImmValue;

	if (eFormat == UF_REGFORMAT_F32)
	{
		uChanCount = 4;
		uImmValue = *(IMG_PUINT32)&fImmValue;
	}
	else
	{
		IMG_UINT32	uF16Value;

		uChanCount = 2;

		uF16Value = ConvertF32ToF16(fImmValue);
		uImmValue = (uF16Value << 0) | (uF16Value << 16);
	}

	SetupImmediateSourcesU(uImmValue, eFormat, uChanCount, asArg);
}

IMG_INTERNAL
IMG_VOID SetupImmediateSourceU(PINST			psInst,
							  IMG_UINT32		uArgIdx,
							  IMG_UINT32		uImmValue,
							  UF_REGFORMAT		eImmFormat)
/*****************************************************************************
 FUNCTION	: SetupImmediateSourceU
    
 PURPOSE	: Set up an instruction source to reference a vector of a repeated
			  immediate value.

 PARAMETERS	: psInst		- Instruction whose argument is to be modified.
			  uArgIdx		- Index of the source to modify.
			  uImmValue		- Immediate value.
			  eImmFormat	- Format of the immediate data.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uBaseArgIdx = uArgIdx * SOURCE_ARGUMENTS_PER_VECTOR;

	psInst->asArg[uBaseArgIdx + 0].uType = USC_REGTYPE_UNUSEDSOURCE;

	SetupImmediateSourcesU(uImmValue, eImmFormat, 4 /* uChanCount */, &psInst->asArg[uBaseArgIdx + 1]);

	psInst->u.psVec->auSwizzle[uArgIdx] = USEASM_SWIZZLE(X, Y, Z, W);
	psInst->u.psVec->aeSrcFmt[uArgIdx] = eImmFormat;
}

IMG_INTERNAL
IMG_VOID SetupImmediateSource(PINST			psInst,
							  IMG_UINT32	uArgIdx,
							  IMG_FLOAT		fImmValue,
							  UF_REGFORMAT	eFormat)
/*****************************************************************************
 FUNCTION	: SetupImmediateSource
    
 PURPOSE	: Set up an instruction source to reference a vector of a repeated
			  immediate value.

 PARAMETERS	: psInst		- Instruction whose argument is to be modified.
			  uArgIdx		- Index of the source to modify.
			  fImmValue		- Immediate value.
			  eFormat		- Format to convert the immediate value to.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uBaseArgIdx = uArgIdx * SOURCE_ARGUMENTS_PER_VECTOR;

	psInst->asArg[uBaseArgIdx + 0].uType = USC_REGTYPE_UNUSEDSOURCE;

	SetupImmediateSources(fImmValue, eFormat, &psInst->asArg[uBaseArgIdx + 1]);

	psInst->u.psVec->auSwizzle[uArgIdx] = USEASM_SWIZZLE(X, Y, Z, W);
	psInst->u.psVec->aeSrcFmt[uArgIdx] = eFormat;
}

IMG_INTERNAL
IMG_VOID ApplyProjectionVec(PINTERMEDIATE_STATE	psState,
							PCODEBLOCK			psBlock,
							PINST				psInsertBeforeInst,
							IMG_UINT32			uResultRegNum,
							PARG				psProjArg,
							PINST				psProjSrcInst,
							IMG_UINT32			uProjSrcIdx,
							IMG_UINT32			uProjTemp,
							IMG_UINT32			uProjChannel,
							UF_REGFORMAT		eCoordFormat,
							PINST				psCoordSrcInst,
							IMG_UINT32			uCoordSrcIdx,
							IMG_UINT32			uInTemp,
							IMG_UINT32			uCoordMask)
/*****************************************************************************
 FUNCTION	: ApplyProjection
    
 PURPOSE	: Generate instructions to apply projection to iterated data.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the intermediate instructions to.
			  eTexCoordFmt		- Format of the data to be projected.	
			  uInputRegNum		- The number of the register containing
								the unprojected data.
			  uResultRegNum		- The number of the register to write
								the projected data into.
			  uProjChan			- Channel to divide by.
			  uCoordMask		- Mask of channels to project.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psRCPInst;
	PINST		psMULInst;
	IMG_UINT32	uOneOverWTemp;
	IMG_UINT32	uChan;

	uOneOverWTemp = GetNextRegister(psState);

	/*
		RCP		TEMP, TCn.W
	*/
	psRCPInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psRCPInst, IVRCP);
	SetDest(psState, psRCPInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uOneOverWTemp, eCoordFormat);
	psRCPInst->auDestMask[0] = psRCPInst->auLiveChansInDest[0] = USC_W_CHAN_MASK;
	if (psProjArg != NULL || psProjSrcInst != NULL)
	{
		if (psProjSrcInst != NULL)
		{
			MoveSrc(psState, psRCPInst, 0 /* uCopyToIdx */, psProjSrcInst, uProjSrcIdx);
		}
		else
		{
			psRCPInst->asArg[0] = *psProjArg;
		}
		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			psRCPInst->asArg[1 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
		}
		psRCPInst->u.psVec->aeSrcFmt[0] = eCoordFormat;
	}
	else
	{
		SetupTempVecSource(psState, psRCPInst, 0, uProjTemp, eCoordFormat);
	}
	switch (uProjChannel)
	{
		case 0: psRCPInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X); break;
		case 1: psRCPInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Y, Y, Y, Y); break;
		case 2: psRCPInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Z, Z, Z, Z); break;
		case 3: psRCPInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(W, W, W, W); break;
	}
	InsertInstBefore(psState, psBlock, psRCPInst, psInsertBeforeInst);

	/*
		MUL		RESULT, TCn, TEMP.W
	*/
	psMULInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psMULInst, IVMUL);
	psMULInst->auDestMask[0] = psMULInst->auLiveChansInDest[0] = uCoordMask;
	SetDest(psState, psMULInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uResultRegNum, eCoordFormat);
	SetupTempVecSource(psState, psMULInst, 0 /* uSrcIdx */, uOneOverWTemp, eCoordFormat);
	psMULInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(W, W, W, W);
	if (psCoordSrcInst != NULL)
	{
		IMG_UINT32	uBaseArgIdx;

		uBaseArgIdx = SOURCE_ARGUMENTS_PER_VECTOR * 1;
		MoveSrc(psState, psMULInst, uBaseArgIdx + 0, psCoordSrcInst, uCoordSrcIdx);
		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			psMULInst->asArg[uBaseArgIdx + 1 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
		}
		psMULInst->u.psVec->aeSrcFmt[1] = eCoordFormat;
	}
	else
	{
		SetupTempVecSource(psState, psMULInst, 1 /* uSrcIdx */, uInTemp, eCoordFormat);
	}
	psMULInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);
	InsertInstBefore(psState, psBlock, psMULInst, psInsertBeforeInst);
}

static
IMG_BOOL IsProjectedSource(PINTERMEDIATE_STATE	psState,
						   PUF_REGISTER			psInputSrc)
/*****************************************************************************
 FUNCTION	: IsProjectedSource
    
 PURPOSE	: Check for an input source register where the channels in the
			  source register needs to be divided by the W channel.

 PARAMETERS	: psState			- Compiler state.
			  psInputSrc		- Input source to check.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if (psInputSrc->eType == UFREG_TYPE_TEXCOORDP)
	{
		return IMG_TRUE;
	}
	if (psInputSrc->eType == UFREG_TYPE_TEXCOORDPIFTC)
	{
		ASSERT(psInputSrc->eRelativeIndex == UFREG_RELATIVEINDEX_NONE);
		if (GetBit(psState->psSAOffsets->auProjectedCoordinateMask, psInputSrc->uNum))
		{
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID GetIteratedSource_Vec(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psBlock,
							   PUF_REGISTER			psInputSrc,
							   PARG					psGPISrc)
/*****************************************************************************
 FUNCTION	: GetIteratedSource_Vec
    
 PURPOSE	: Get a source containing the value of an iterated pixel shader
			  input.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to add the intermediate instructions to.
			  psInputSrc		- Input format source.
			  psGPISrc			- On return contains the intermediate iteration
								result.
			  
 RETURNS	: None.
*****************************************************************************/
{
	UNIFLEX_ITERATION_TYPE		eIterationType;
	IMG_UINT32					uCoordinate;
	UNIFLEX_TEXLOAD_FORMAT		eIterFormat;
	IMG_UINT32					uResultRegNum;
	IMG_UINT32					uPreProjRegNum;
	UF_REGFORMAT				eIterationFormat;

	/*
		Convert the input register type to an iteration type.
	*/
	switch (psInputSrc->eType)
	{
		case UFREG_TYPE_COL:
		{
			eIterationType = UNIFLEX_ITERATION_TYPE_COLOUR;
			uCoordinate = psInputSrc->uNum;
			eIterationFormat = psInputSrc->eFormat;
			break;
		}
		case UFREG_TYPE_TEXCOORD:
		case UFREG_TYPE_TEXCOORDP:
		case UFREG_TYPE_TEXCOORDPIFTC:
		{
			IMG_UINT32		uArrayIdx;
			PUNIFLEX_RANGE	psRange;

			uArrayIdx = SearchRangeList(&psState->psSAOffsets->sShaderInputRanges, psInputSrc->uNum, &psRange);
			if (uArrayIdx != USC_UNDEF)
			{
				psGPISrc->uType = USC_REGTYPE_REGARRAY;
				psGPISrc->uNumber = psState->sShader.psPS->asTextureCoordinateArrays[uArrayIdx].uRegArrayIdx;
				psGPISrc->uArrayOffset = (psInputSrc->uNum - psRange->uRangeStart) * SCALAR_REGISTERS_PER_F32_VECTOR;
				GetRelativeIndex(psState, psInputSrc, psGPISrc);
				return;
			}

			eIterationType = UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE;
			uCoordinate = psInputSrc->uNum;
			eIterationFormat = psInputSrc->eFormat;
			break;
		}
		case UFREG_TYPE_MISC:
		{
			/*
				The hardware only supports iterating these pixel shader inputs to
				F32 format.
			*/
			eIterationFormat = UF_REGFORMAT_F32;
			uCoordinate = 0;
			if (psInputSrc->uNum == UF_MISC_POSITION)
			{
				eIterationType = UNIFLEX_ITERATION_TYPE_POSITION;
			}
			else if (psInputSrc->uNum == UF_MISC_FOG)
			{
				eIterationType = UNIFLEX_ITERATION_TYPE_FOG;
			}
			else if (psInputSrc->uNum == UF_MISC_VPRIM)
			{
				ASSERT(psInputSrc->eFormat == UF_REGFORMAT_I32 || psInputSrc->eFormat == UF_REGFORMAT_U32);				
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
		Convert the register format to an iteration format.
	*/
	switch (eIterationFormat)
	{
		case UF_REGFORMAT_F16: eIterFormat = UNIFLEX_TEXLOAD_FORMAT_F16; break;
		case UF_REGFORMAT_F32: eIterFormat = UNIFLEX_TEXLOAD_FORMAT_F32; break;
		default: imgabort();
	}

	/*
		Set up to iterate the data before the start of the shader.
	*/
	uPreProjRegNum = GetIteratedValue(psState, eIterationType, uCoordinate, eIterFormat, 1 /* uNumAttributes */);

	/*
		If required generate code to divide the UVS channels by the T channel.
	*/
	if (IsProjectedSource(psState, psInputSrc))
	{
		uResultRegNum = GetNextRegister(psState);
		ApplyProjectionVec(psState, 
						   psBlock, 
						   NULL /* psInsertBeforeInst */,
						   uResultRegNum,
						   NULL /* psProjArg */,
						   NULL /* psProjSrcInst */,
						   USC_UNDEF /* uProjSrcIdx */,
						   uPreProjRegNum,
						   USC_W_CHAN /* uProjChannel */,
						   psInputSrc->eFormat, 
						   NULL /* psCoordSrcInst */,
						   USC_UNDEF /* uCoordSrcIdx */,
						   uPreProjRegNum,
						   USC_ALL_CHAN_MASK /* uCoordMask */);
	}
	else
	{
		uResultRegNum = uPreProjRegNum;
	}

	/*
		If the format of the iteration result differs from the
		format expected by the instruction then insert a conversion instruction.
	*/
	if (eIterationFormat != psInputSrc->eFormat)
	{
		PINST	psCvtInst;

		psCvtInst = AllocateInst(psState, NULL);
		SetOpcode(psState, psCvtInst, IVPCKFLTFLT);
		psCvtInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psCvtInst->asDest[0].uNumber = GetNextRegister(psState);
		psCvtInst->asDest[0].eFmt = psInputSrc->eFormat;
		SetupTempVecSource(psState, psCvtInst, 0, uResultRegNum, eIterationFormat);
		psCvtInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psCvtInst->u.psVec->aeSrcFmt[0] = eIterationFormat;
		AppendInst(psState, psBlock, psCvtInst);

		uResultRegNum = psCvtInst->asDest[0].uNumber;
	}

	/*
		Return the details of the vector register containing the
		iteration result.
	*/
	psGPISrc->uType = USEASM_REGTYPE_TEMP;
	psGPISrc->uNumber = uResultRegNum;
	psGPISrc->eFmt = psInputSrc->eFormat;
}


IMG_INTERNAL
IMG_VOID GenerateMinMax(PINTERMEDIATE_STATE	psState,
						PCODEBLOCK			psCodeBlock,
						PUNIFLEX_INST		psInputInst,
						IMG_UINT32			uDestMask,
						UF_OPCODE			eOperation,
						UF_REGFORMAT		eFormat,
						IMG_FLOAT			fConst)
/*****************************************************************************
 FUNCTION	: GenerateMinMax
    
 PURPOSE	: Generate intermediate instructions to clamp an input instruction
			  to a minimum/maximum.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Input instruction whose result is to be clamped.
			  uDestMask			- Mask of channel to clamp.
			  eOperation		- Whether to do a MIN or a MAX.
			  eFormat			- Format of the data to clamp.
			  fConst			- Constant to clamp to (Can be -1, 1 or 0).
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psClampInst;
	IOPCODE		eHwOpcode;
	IMG_UINT32	uChan;

	psClampInst = AllocateInst(psState, NULL);
	if (eOperation == UFOP_MIN)
	{
		eHwOpcode = IVMIN;
	}
	else
	{
		ASSERT(eOperation == UFOP_MAX);
		eHwOpcode = IVMAX;
	}
	SetOpcode(psState, psClampInst, eHwOpcode);

	/*	
		Add the value to clamp as the destination and first source argument.
	*/
	if (eFormat == UF_REGFORMAT_F16)
	{
		GetDestinationF16_Vec(psState, &psInputInst->sDest, &psClampInst->asDest[0]);
		GetDestinationF16_Vec(psState, &psInputInst->sDest, &psClampInst->asArg[0]);
	}
	else
	{
		GetDestinationF32_Vec(psState, &psInputInst->sDest, &psClampInst->asDest[0]);
		GetDestinationF32_Vec(psState, &psInputInst->sDest, &psClampInst->asArg[0]);
	}
	for (uChan = 0; uChan < 4; uChan++)
	{
		psClampInst->asArg[1 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
	}
	psClampInst->auDestMask[0] = uDestMask;

	psClampInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psClampInst->u.psVec->aeSrcFmt[0] = eFormat;
	
	/*
		Add the constant to clamp to as the second source argument.
	*/
	if (fConst == -1.0f || fConst == 1.0f)
	{
		SetupImmediateSource(psClampInst, 1 /* uArgIdx */, 1.0f, eFormat);
		if (fConst == -1.0f)
		{
			psClampInst->u.psVec->asSrcMod[1].bNegate = IMG_TRUE;
		}
	}
	else
	{
		ASSERT(fConst == 0.0f);
		SetupImmediateSource(psClampInst, 1 /* uArgIdx */, 0.0f, eFormat);
	}

	AppendInst(psState, psCodeBlock, psClampInst);
}

IMG_INTERNAL
IMG_VOID GenerateSrcModF32F16_Vec(PINTERMEDIATE_STATE	psState,
								  PCODEBLOCK			psCodeBlock,
								  IMG_UINT32			uSMod,
								  PARG					psGPIArg,
								  PARG					psUSArgs,
								  IMG_BOOL				bF16)
/*****************************************************************************
 FUNCTION	: GenerateSrcModF32F16_Vec
    
 PURPOSE	: Generate intermediate instructions to apply a source modifier.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uSMod				- Source modifier to generate code for.
			  psGPIArg			- Source data to apply the source modifier to.
			  psUSArgs	
			  bF16				- If TRUE generate F16 instructions else F32.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32		uSModResult;
	UF_REGFORMAT	eFmt;
	IMG_UINT32		uChan;
	IMG_UINT32		uUSWidth;

	uSModResult = GetNextRegister(psState);
	if (bF16)
	{
		eFmt = UF_REGFORMAT_F16;
		uUSWidth = 2;
	}
	else
	{
		eFmt = UF_REGFORMAT_F32;
		uUSWidth = 4;
	}

	switch (uSMod)
	{
		case UFREG_SMOD_COMPLEMENT:
		{
			PINST	psVADDInst;

			/*
				SOURCE = 1 - SOURCE
			*/
			psVADDInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psVADDInst, IVADD);
			psVADDInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psVADDInst->asDest[0].uNumber = uSModResult;
			psVADDInst->asDest[0].eFmt = eFmt;
			psVADDInst->asArg[0] = *psGPIArg;
			for (uChan = 0; uChan < uUSWidth; uChan++)
			{
				psVADDInst->asArg[1 + uChan] = psUSArgs[uChan];
			}
			psVADDInst->u.psVec->aeSrcFmt[0] = eFmt;
			psVADDInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			psVADDInst->u.psVec->asSrcMod[0].bNegate = IMG_TRUE;
			SetupImmediateSource(psVADDInst, 1 /* uArgIdx */, 1.0f, eFmt);
			AppendInst(psState, psCodeBlock, psVADDInst);
			break;
		}
		case UFREG_SMOD_BIAS:
		{
			PINST	psVADDInst;

			/*
				SOURCE = SOURCE - 0.5
			*/
			psVADDInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psVADDInst, IVADD);
			psVADDInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psVADDInst->asDest[0].uNumber = uSModResult;
			psVADDInst->asDest[0].eFmt = eFmt;
			psVADDInst->asArg[0] = *psGPIArg;
			for (uChan = 0; uChan < uUSWidth; uChan++)
			{
				psVADDInst->asArg[1 + uChan] = psUSArgs[uChan];
			}
			psVADDInst->u.psVec->aeSrcFmt[0] = eFmt;
			psVADDInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			SetupImmediateSource(psVADDInst, 1 /* uArgIdx */, 0.5f, eFmt);
			psVADDInst->u.psVec->asSrcMod[1].bNegate = IMG_TRUE;
			AppendInst(psState, psCodeBlock, psVADDInst);
			break;
		}
		case UFREG_SMOD_SIGNED:
		{
			PINST	psVMADInst;

			/*
				SOURCE = 2 * SOURCE - 1
			*/
			psVMADInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psVMADInst, IVMAD);
			psVMADInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psVMADInst->asDest[0].uNumber = uSModResult;
			psVMADInst->asDest[0].eFmt = eFmt;
			psVMADInst->asArg[0] = *psGPIArg;
			for (uChan = 0; uChan < uUSWidth; uChan++)
			{
				psVMADInst->asArg[1 + uChan] = psUSArgs[uChan];
			}
			psVMADInst->u.psVec->aeSrcFmt[0] = eFmt;
			psVMADInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			SetupImmediateSource(psVMADInst, 1 /* uArgIdx */, 2.0f, eFmt);
			SetupImmediateSource(psVMADInst, 2 /* uArgIdx */, 1.0f, eFmt);
			psVMADInst->u.psVec->asSrcMod[2].bNegate = IMG_TRUE;
			AppendInst(psState, psCodeBlock, psVMADInst);
			break;
		}
		case UFREG_SMOD_TIMES2:
		{
			PINST	psVMULInst;

			/*
				SOURCE = SOURCE * 2
			*/
			psVMULInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psVMULInst, IVMUL);
			psVMULInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psVMULInst->asDest[0].uNumber = uSModResult;
			psVMULInst->asDest[0].eFmt = eFmt;
			psVMULInst->asArg[0] = *psGPIArg;
			for (uChan = 0; uChan < uUSWidth; uChan++)
			{
				psVMULInst->asArg[1 + uChan] = psUSArgs[uChan];
			}
			psVMULInst->u.psVec->aeSrcFmt[0] = eFmt;
			psVMULInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			SetupImmediateSource(psVMULInst, 1 /* uArgIdx */, 2.0f, eFmt);
			AppendInst(psState, psCodeBlock, psVMULInst);
			break;
		}
		default: imgabort();
	}

	/*
		Clear the existing sources.	
	*/
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		InitInstArg(&psUSArgs[uChan]);
		psUSArgs[uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
	}

	/*
		Point the source to the result of applying the source modifiers.
	*/
	InitInstArg(psGPIArg);
	psGPIArg->uType = USEASM_REGTYPE_TEMP;
	psGPIArg->uNumber = uSModResult;
	psGPIArg->eFmt = eFmt;
}

static
IMG_VOID GenerateDestModF32F16_Vec(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psCodeBlock,
								   PUNIFLEX_INST		psInputInst,
								   IMG_BOOL				bF16)
/*****************************************************************************
 FUNCTION	: GenerateDestModF32F16_Vec
    
 PURPOSE	: Generate intermediate instructions to apply a destination modifier.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction whose destination will have the
								destination modifier applied.
			  bF16				- Format of the data to apply the destination
								modifier to.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32		uSat = (psInputInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32		uScale = (psInputInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32		uDestMask = psInputInst->sDest.u.byMask;
	UF_REGFORMAT	eFormat = bF16 ? UF_REGFORMAT_F16 : UF_REGFORMAT_F32;

	/* Apply a scale (just multiply the destination register by the appropriate constant. */
	if (uScale != UFREG_DMOD_SCALEMUL1)
	{
		PINST		psVMulInst;
		IMG_FLOAT	fConst;

		psVMulInst = AllocateInst(psState, NULL);
		SetOpcode(psState, psVMulInst, IVMUL);

		/*
			Set both the destination and the first source to the intermediate register
			corresponding to the input instruction's destination.
		*/
		if (bF16)
		{
			GetDestinationF16_Vec(psState, &psInputInst->sDest, &psVMulInst->asDest[0]);
			GetDestinationF16_Vec(psState, &psInputInst->sDest, &psVMulInst->asArg[0]);
			psVMulInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F16;
		}
		else
		{
			GetDestinationF32_Vec(psState, &psInputInst->sDest, &psVMulInst->asDest[0]);
			GetDestinationF32_Vec(psState, &psInputInst->sDest, &psVMulInst->asArg[0]);
			psVMulInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
		}
		psVMulInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psVMulInst->auDestMask[0] = uDestMask;

		/*
			Set the second source to an appropriate scale value.
		*/
		switch (uScale)
		{
			case UFREG_DMOD_SCALEMUL1: fConst = 1.0f; break;
			case UFREG_DMOD_SCALEMUL2: fConst = 2.0f; break;
			case UFREG_DMOD_SCALEMUL4: fConst = 4.0f; break;
			case UFREG_DMOD_SCALEMUL8: fConst = 8.0f; break;
			case UFREG_DMOD_SCALEDIV2: fConst = 0.5f; break;
			case UFREG_DMOD_SCALEDIV4: fConst = 0.25f; break;
			case UFREG_DMOD_SCALEDIV8: fConst = 0.125f; break;
			case UFREG_DMOD_SCALEDIV16: fConst = 0.0625f; break;
			default: imgabort();
		}
		SetupImmediateSource(psVMulInst, 1 /* uArgIdx */, fConst, eFormat);

		AppendInst(psState, psCodeBlock, psVMulInst);
	}

	/* Apply a saturation by doing a min and max. */
	switch (uSat)
	{
		case UFREG_DMOD_SATZEROONE:
		{
			/* DEST = MAX(DEST, 0.0f) */
			GenerateMinMax(psState, psCodeBlock, psInputInst, uDestMask, UFOP_MAX, eFormat, 0.0f);
			/* DEST = MIN(DEST, 1.0f) */
			GenerateMinMax(psState, psCodeBlock, psInputInst, uDestMask, UFOP_MIN, eFormat, 1.0f);
			break;
		}
		case UFREG_DMOD_SATNEGONEONE:
		{
			/* DEST = MAX(DEST, -1.0f) */
			GenerateMinMax(psState, psCodeBlock, psInputInst, uDestMask, UFOP_MAX, eFormat, -1.0f);
			/* DEST = MIN(DEST, 1.0f) */
			GenerateMinMax(psState, psCodeBlock, psInputInst, uDestMask, UFOP_MIN, eFormat, 1.0f);
			break;
		}
		case UFREG_DMOD_SATZEROMAX:
		{
			/* DEST = MAX(DEST, 0.0f) */
			GenerateMinMax(psState, psCodeBlock, psInputInst, uDestMask, UFOP_MAX, eFormat, 0.0f);
			break;
		}
		case UFREG_DMOD_SATNONE:
		{
			break;
		}
		default: imgabort();
	}
}

IMG_INTERNAL
IMG_VOID GetSourceF16_Vec(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psBlock,
						  PUF_REGISTER			psInputSrc,
						  IMG_UINT32			uUsedChanMask,
						  PINST					psInst,
						  IMG_UINT32			uSrcIdx)
/*****************************************************************************
 FUNCTION	: GetSourceF16_Vec
    
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
	IMG_UINT32				uBaseArgIdx = uSrcIdx * SOURCE_ARGUMENTS_PER_VECTOR;
	PARG					psQwordSrc = &psInst->asArg[uBaseArgIdx + 0];
	PARG					asDwordSrc = &psInst->asArg[uBaseArgIdx + 1];
	PVEC_SOURCE_MODIFIER	psSourceMod = &psInst->u.psVec->asSrcMod[uSrcIdx];
	PUF_REGFORMAT			peSourceFmt = &psInst->u.psVec->aeSrcFmt[uSrcIdx];

	InitInstArg(psQwordSrc);
	psQwordSrc->uType = USC_REGTYPE_UNUSEDSOURCE;
	for (uComp = 0; uComp < 4; uComp++)
	{
		InitInstArg(&asDwordSrc[uComp]);
		asDwordSrc[uComp].uType = USC_REGTYPE_UNUSEDSOURCE;
	}

	*peSourceFmt = UF_REGFORMAT_F16;

	switch (psInputSrc->eType)
	{
		case UFREG_TYPE_TEMP:
		{
			psQwordSrc->uType = USEASM_REGTYPE_TEMP;
			psQwordSrc->uNumber = ConvertTempRegisterNumberF32(psState, psInputSrc->uNum, 0);
			psQwordSrc->eFmt = UF_REGFORMAT_F16;
			break;
		}
		case UFREG_TYPE_COL:
		case UFREG_TYPE_TEXCOORD:
		case UFREG_TYPE_TEXCOORDP:
		case UFREG_TYPE_TEXCOORDPIFTC:
		case UFREG_TYPE_MISC:
		{
			GetIteratedSource_Vec(psState, psBlock, psInputSrc, psQwordSrc);
			break;
		}
		case UFREG_TYPE_CONST:
		{
			LoadConstantNoHWReg(psState,
								psBlock,
								NULL /* psInsertBeforeInst */,
								psInputSrc,
								0 /* uSrcChan */,
								psQwordSrc,
								NULL /* puComponent */,
								UF_REGFORMAT_F16,
								IMG_TRUE /* bVectorResult */,
								IMG_FALSE /* bC10Subcomponent */,
								0 /* uCompOffset */);
			break;
		}
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

			SetupImmediateSources(fValue, UF_REGFORMAT_F16, asDwordSrc);
			break;
		}
		case UFREG_TYPE_PSOUTPUT:
		{
			ConvertPixelShaderResultArg(psState, psInputSrc, psQwordSrc);
			psQwordSrc->eFmt = UF_REGFORMAT_F16;
			break;
		}
		case UFREG_TYPE_VSINPUT:
		case UFREG_TYPE_VSOUTPUT:
		{
			PINST		psPackInst;
			UF_REGISTER	sInputSrcF32;

			sInputSrcF32 = *psInputSrc;
			sInputSrcF32.eFormat = UF_REGFORMAT_F32;
			sInputSrcF32.byMod = UFREG_SMOD_NONE;

			psPackInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psPackInst, IVMOV);

			/*
				Set the destination to a temporary register.
			*/
			psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asDest[0].uNumber = GetNextRegister(psState);
			psPackInst->asDest[0].eFmt = UF_REGFORMAT_F16;

			/*
				Get the source data in F32 format.
			*/
			GetSourceF32_Vec(psState,
							 psBlock,
							 &sInputSrcF32,
							 uUsedChanMask,
							 psPackInst,
							 0 /* uSrcIdx */);

			psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			AppendInst(psState, psBlock, psPackInst);

			*psQwordSrc = psPackInst->asDest[0];
			break;
		}
		case UFREG_TYPE_INDEXABLETEMP:
		{
			IMG_UINT32	uChan;

			psQwordSrc->uType = USEASM_REGTYPE_TEMP;
			psQwordSrc->uNumber = GetNextRegister(psState);
			psQwordSrc->eFmt = UF_REGFORMAT_F16;

			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				IMG_UINT32	uLoadTemp;
				PINST		psVPCKInst;

				/*
					Generate instructions to load from the indexable temporary
					array into the allocated variable.
				*/
				uLoadTemp = GetNextRegister(psState);
				LoadStoreIndexableTemp(psState, 
									   psBlock, 
									   IMG_TRUE /* bLoad */, 
									   psInputSrc->eFormat, 
									   psInputSrc, 
									   1 << uChan, 
									   uLoadTemp, 
									   0 /* uDataReg */);

				/*
					Move the result of the load from the indexable temporary
					into the right channel in the source.
				*/
				psVPCKInst = AllocateInst(psState, NULL);
				SetOpcode(psState, psVPCKInst, IVPCKFLTFLT);
				psVPCKInst->asDest[0] = *psQwordSrc;
				psVPCKInst->auDestMask[0] = 1 << uChan;
				psVPCKInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;
				psVPCKInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
				psVPCKInst->asArg[1].uNumber = uLoadTemp;
				psVPCKInst->asArg[1].eFmt = UF_REGFORMAT_F16;
				psVPCKInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F16;
				if ((uChan % 2) == 0)
				{
					psVPCKInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
				}
				else
				{
					psVPCKInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Y, Y, Y, Y);
				}
				SetSrcUnused(psState, psVPCKInst, 2 /* uSrcIdx */);
				AppendInst(psState, psBlock, psVPCKInst);
			}
			break;
		}
		case UFREG_TYPE_IMMEDIATE:
		{
			SetupImmediateSources(*(IMG_PFLOAT)&psInputSrc->uNum, UF_REGFORMAT_F16, asDwordSrc);
			break;
		}
		default: imgabort();
	}

	if (uSMod != UFREG_SMOD_NONE)
	{
		GenerateSrcModF32F16_Vec(psState, psBlock, uSMod, psQwordSrc, asDwordSrc, IMG_TRUE /* bF16 */);
	}

	CopyVecSourceModifiers(psInputSrc, psSourceMod);
}

IMG_INTERNAL
IMG_VOID ConvertComplexOpInstructionF32F16_Vec(PINTERMEDIATE_STATE	psState,
											   PCODEBLOCK			psCodeBlock,
											   PUNIFLEX_INST		psInputInst,
											   IMG_BOOL				bF16)
/*****************************************************************************
 FUNCTION	: ConvertComplexOpInstructionF32F16_Vec
    
 PURPOSE	: Convert a RECIP/RSQRT/LOG/EXP input instruction to the
			  corresponding intermediate instruction.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Input instruction.
			  bF16				- If TRUE the input instruction works on F16
								format data. If FALSE the input instruction
								works on F32 format data.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IOPCODE		eHwOpcode;
	IMG_UINT32	uMaskToSwiz = MaskToSwiz(psInputInst->sDest.u.byMask);
	IMG_UINT32	uSwiz = psInputInst->asSrc[0].u.uSwiz;
	IMG_UINT32	uUsedChanMask = GetSourceLiveChans(psState, psInputInst, 0);

	/*
		Skip if no channels are modified in the destination.
	*/
	if (psInputInst->sDest.u.byMask == 0)
	{
		return;
	}

	/*
		Convert the input opcode to the corresponding intermediate
		opcode.
	*/
	switch (psInputInst->eOpCode)
	{
		case UFOP_RECIP: eHwOpcode = IVRCP; break;
		case UFOP_RSQRT: eHwOpcode = IVRSQ; break;
		case UFOP_EXP: eHwOpcode = IVEXP; break;
		case UFOP_LOG: eHwOpcode = IVLOG; break;
		default: imgabort();
	}

	if ((uSwiz & uMaskToSwiz) == (UFREG_SWIZ_XXXX & uMaskToSwiz) ||
		(uSwiz & uMaskToSwiz) == (UFREG_SWIZ_YYYY & uMaskToSwiz) ||
		(uSwiz & uMaskToSwiz) == (UFREG_SWIZ_ZZZZ & uMaskToSwiz) ||
		(uSwiz & uMaskToSwiz) == (UFREG_SWIZ_WWWW & uMaskToSwiz))
	{
		PINST		psComplexInst;
		IMG_UINT32	uSrcChan;
		IMG_UINT32	uChan;
		
		/*
			Get the source channel for the operation.
		*/
		uSrcChan = USC_UNDEF;
		for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
		{
			if (psInputInst->sDest.u.byMask & (1 << uChan))
			{
				uSrcChan = EXTRACT_CHAN(uSwiz, uChan);
				break;
			}
		}
		ASSERT(uSrcChan != USC_UNDEF);

		/*
			Create a new intermediate instruction.
		*/
		psComplexInst = AllocateInst(psState, NULL);
		SetOpcode(psState, psComplexInst, eHwOpcode);

		if (bF16)
		{
			GetDestinationF16_Vec(psState, &psInputInst->sDest, &psComplexInst->asDest[0]);
		}
		else
		{
			GetDestinationF32_Vec(psState, &psInputInst->sDest, &psComplexInst->asDest[0]);
		}
		psComplexInst->auDestMask[0] = psInputInst->sDest.u.byMask;

		GetVectorPredicateInst(psState, psComplexInst, psInputInst);

		if (bF16)
		{
			GetSourceF16_Vec(psState, 
							 psCodeBlock, 
							 &psInputInst->asSrc[0], 
							 uUsedChanMask,
							 psComplexInst,
							 0 /* uSrcIdx */);
		}
		else
		{
			GetSourceF32_Vec(psState, 
							 psCodeBlock, 
							 &psInputInst->asSrc[0],
							 uUsedChanMask,
							 psComplexInst,
							 0 /* uSrcIdx */);
		}
		switch (uSrcChan)
		{
			case UFREG_SWIZ_X: psComplexInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X); break;
			case UFREG_SWIZ_Y: psComplexInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Y, Y, Y, Y); break;
			case UFREG_SWIZ_Z: psComplexInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Z, Z, Z, Z); break;
			case UFREG_SWIZ_W: psComplexInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(W, W, W, W); break;
			default: imgabort();
		}
		AppendInst(psState, psCodeBlock, psComplexInst);
	}
	else
	{
		IMG_UINT32	uTempDest = GetNextRegister(psState);
		PINST		psMovInst;
		IMG_UINT32	uChan;

		/*
			Compute a result per-channel into a temporary.
		*/
		for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
		{
			PINST	psComplexInst;

			if ((psInputInst->sDest.u.byMask & (1 << uChan)) == 0)
			{
				continue;
			}

			psComplexInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psComplexInst, eHwOpcode);
			psComplexInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psComplexInst->asDest[0].uNumber = uTempDest;
			if (bF16)
			{
				psComplexInst->asDest[0].eFmt = UF_REGFORMAT_F16;
			}
			else
			{
				psComplexInst->asDest[0].eFmt = UF_REGFORMAT_F32;
			}
			psComplexInst->auDestMask[0] = 1 << uChan;

			/*
				Get an intermediate representation of the source data.
			*/
			if (bF16)
			{
				GetSourceF16_Vec(psState, 
								 psCodeBlock, 
								 &psInputInst->asSrc[0], 
								 uUsedChanMask,
								 psComplexInst,
								 0 /* uSrcIdx */);
			}
			else
			{		
				GetSourceF32_Vec(psState, 
								 psCodeBlock, 
								 &psInputInst->asSrc[0], 
								 uUsedChanMask,
								 psComplexInst,
								 0 /* uSrcIdx */);
			}
			switch (EXTRACT_CHAN(uSwiz, uChan))
			{
				case UFREG_SWIZ_X: psComplexInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X); break;
				case UFREG_SWIZ_Y: psComplexInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Y, Y, Y, Y); break;
				case UFREG_SWIZ_Z: psComplexInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Z, Z, Z, Z); break;
				case UFREG_SWIZ_W: psComplexInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(W, W, W, W); break;
				default: imgabort();
			}
			AppendInst(psState, psCodeBlock, psComplexInst);
		}

		/*
			Move the temporary into the real destination.
		*/
		psMovInst = AllocateInst(psState, NULL);
		SetOpcode(psState, psMovInst, IVMOV);
		if (bF16)
		{
			GetDestinationF16_Vec(psState, &psInputInst->sDest, &psMovInst->asDest[0]);
		}
		else
		{
			GetDestinationF32_Vec(psState, &psInputInst->sDest, &psMovInst->asDest[0]);
		}
		GetVectorPredicateInst(psState, psMovInst, psInputInst);
		psMovInst->auDestMask[0] = psInputInst->sDest.u.byMask;
		SetupTempVecSource(psState, psMovInst, 0 /* uSrcIdx */, uTempDest, bF16 ? UF_REGFORMAT_F16 : UF_REGFORMAT_F32);
		psMovInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		AppendInst(psState, psCodeBlock, psMovInst);
	}

	GenerateDestModF32F16_Vec(psState, psCodeBlock, psInputInst, bF16);

	if (bF16)
	{
		StoreDestinationF16(psState, psCodeBlock, psInputInst, &psInputInst->sDest);
	}
	else
	{
		StoreDestinationF32(psState, psCodeBlock, psInputInst, &psInputInst->sDest, USC_TEMPREG_CVTVECF32DSTOUT);
	}
}

IMG_INTERNAL
IMG_VOID ConvertTexkillInstructionFloatVec(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psInputInst, IMG_BOOL bConditional)
/*****************************************************************************
 FUNCTION	: ConvertTexkillInstructionFloatVec
    
 PURPOSE	: Convert a UFOP_KPLT/UFOP_KPLE instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	UFREG_COMPOP	eCompOp;
	UF_REGISTER		sZero;
	IMG_UINT32		uPredSrc;

	/*
		Convert the instruction to a comparison operator.
	*/
	if (psInputInst->eOpCode == UFOP_KPLT)
	{
		eCompOp = UFREG_COMPOP_GE;
	}
	else
	{
		ASSERT(psInputInst->eOpCode == UFOP_KPLE);
		eCompOp = UFREG_COMPOP_GT;
	}

	/*
		Do we need to combine the result from the instruction
		with previous texkill results.
	*/
	if (bConditional || (psState->uFlags & USC_FLAGS_TEXKILL_PRESENT) != 0)
	{
		uPredSrc = USC_PREDREG_TEXKILL;
	}
	else
	{
		uPredSrc = USC_PREDREG_NONE;
	}

	/*
		Creating an input source representing constant source.
	*/
	memset(&sZero, 0, sizeof(sZero));
	sZero.eType = UFREG_TYPE_HW_CONST;
	sZero.uNum = HW_CONST_0;
	sZero.u.uSwiz = UFREG_SWIZ_NONE;

	/*
		Generate a TEST instruction taking the logical-AND of the per-channel
		comparison results.
	*/
	GenerateComparisonF32F16_Vec(psState,
								 psCodeBlock,
								 USC_PREDREG_TEXKILL /* uPredDest */,
								 eCompOp,
								 &psInputInst->asSrc[0],
								 &sZero,
								 USEASM_TEST_CHANSEL_ANDALL,
								 uPredSrc,
								 IMG_FALSE /* bCompPredNegate */,
								 IMG_FALSE /* bInvert */);

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
}

IMG_INTERNAL
IMG_VOID TestVecFloat(PINTERMEDIATE_STATE	psState,
					  PCODEBLOCK			psBlock,
					  IMG_UINT32			uTempReg,
					  UFREG_COMPOP			eCompOp)
/*****************************************************************************
 FUNCTION	: TestVecFloat
    
 PURPOSE	: Generate instructions to set a predicate based on the value
			  of a floating point vector.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to insert the instructions in.
			  uTempReg			- Number of the temporary register containing
								the vector.
			  eCompOp			- Comparison type.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST	psUseInst;

	psUseInst = AllocateInst(psState, IMG_NULL);
	
	SetOpcodeAndDestCount(psState, psUseInst, IVTEST, VTEST_PREDICATE_ONLY_DEST_COUNT);

	psUseInst->u.psVec->sTest.eTestOpcode = IVADD;

	SetupTempVecSource(psState, psUseInst, 0 /* uSrcIdx */, uTempReg, UF_REGFORMAT_F32);
	psUseInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

	SetupImmediateSource(psUseInst, 1 /* uSrcIdx */, 0 /* uImmValue */, UF_REGFORMAT_F32);
	psUseInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);

	MakeVectorPredicateDest(psState, psUseInst, USC_PREDREG_TEMP);

	psUseInst->u.psVec->sTest.eType = ConvertCompOpToIntermediate(psState, eCompOp);

	AppendInst(psState, psBlock, psUseInst);
}

IMG_INTERNAL
IMG_VOID GenerateComparisonF32F16_Vec(PINTERMEDIATE_STATE	psState,
								      PCODEBLOCK			psCodeBlock,
									  IMG_UINT32			uPredDest,
									  UFREG_COMPOP			uCompOp,
									  PUF_REGISTER			psSrc1, 
									  PUF_REGISTER			psSrc2, 
									  USEASM_TEST_CHANSEL	eChanOp,
									  IMG_UINT32			uCompPredSrc,
									  IMG_BOOL				bCompPredNegate,
									  IMG_BOOL				bInvert)
/*****************************************************************************
 FUNCTION	: GenerateComparisonF32F16_Vec
    
 PURPOSE	: Convert a UFOP_KPLT/UFOP_KPLE instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uPredDest			- Predicate to write.
			  uCompOp			- Type of comparison to do.
			  psSrc1			- Source data to compare.
			  psSrc2
			  eChanOp			- Channel(s) in the source registers to compare.
			  uCompPredSrc		- Predicate to control update of the destination
			  bCompPredNegate	predicate.
			  bInvert			- If TRUE generate the logical negation of the
								comparison.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST			psTestInst;
	IMG_UINT32		uSrcArg;
	IMG_UINT32		uPreSwizUsedChanMask;
	VTEST_CHANSEL	eChanSel;
	IMG_UINT32		uExtraSwizzle;

	if (bInvert)
	{
		uCompOp = g_puInvertCompOp[uCompOp];
	}

	/*
		Create a vector TEST instruction.
	*/
	psTestInst = AllocateInst(psState, NULL);
	SetOpcodeAndDestCount(psState, psTestInst, IVTEST, VTEST_PREDICATE_ONLY_DEST_COUNT);
	psTestInst->u.psVec->sTest.eTestOpcode = IVADD;

	/*
		Set the destination predicate to write with the
		result of the test.
	*/
	MakeVectorPredicateDest(psState, psTestInst, uPredDest);

	/*
		Set the predicate to control update of the
		destination.
	*/
	SetPredicate(psState, psTestInst, uCompPredSrc, bCompPredNegate);

	/*
		Convert the comparison operator to a intermediate format test
		specification.
	*/
	psTestInst->u.psVec->sTest.eType = ConvertCompOpToIntermediate(psState, uCompOp);
	switch (eChanOp)
	{
		case USEASM_TEST_CHANSEL_C0: eChanSel = VTEST_CHANSEL_XCHAN; uExtraSwizzle = USEASM_SWIZZLE(X, X, X, X); break;
		case USEASM_TEST_CHANSEL_C1: eChanSel = VTEST_CHANSEL_XCHAN; uExtraSwizzle = USEASM_SWIZZLE(Y, Y, Y, Y); break;
		case USEASM_TEST_CHANSEL_C2: eChanSel = VTEST_CHANSEL_XCHAN; uExtraSwizzle = USEASM_SWIZZLE(Z, Z, Z, Z); break;
		case USEASM_TEST_CHANSEL_C3: eChanSel = VTEST_CHANSEL_XCHAN; uExtraSwizzle = USEASM_SWIZZLE(W, W, W, W); break;
		case USEASM_TEST_CHANSEL_ANDALL: eChanSel = VTEST_CHANSEL_ANDALL; uExtraSwizzle = USEASM_SWIZZLE(X, Y, Z, W); break;
		case USEASM_TEST_CHANSEL_ORALL: eChanSel = VTEST_CHANSEL_ORALL; uExtraSwizzle = USEASM_SWIZZLE(X, Y, Z, W); break;
		default: imgabort();
	}
	psTestInst->u.psVec->sTest.eChanSel = eChanSel;

	/*
		Get the channels used by the channel operator from the ALU result.
	*/
	uPreSwizUsedChanMask = GetVTestChanSelMask(psState, psTestInst);

	/*
		Get the intermediate representations of the source arguments.
	*/
	for (uSrcArg = 0; uSrcArg < 2; uSrcArg++)
	{
		PUF_REGISTER	psSrc;
		IMG_UINT32		uUsedChanMask;

		psSrc = (uSrcArg == 0) ? psSrc1 : psSrc2;
		uUsedChanMask = SwizzleMask(psSrc->u.uSwiz, uPreSwizUsedChanMask);
		if (psSrc->eFormat == UF_REGFORMAT_F16)
		{
			GetSourceF16_Vec(psState, 
							 psCodeBlock,
							 psSrc,
							 uUsedChanMask,
							 psTestInst,
							 uSrcArg);
		}
		else
		{
			GetSourceF32_Vec(psState, 
							 psCodeBlock, 
							 psSrc,
							 uUsedChanMask,
							 psTestInst,
							 uSrcArg);
		}
		psTestInst->u.psVec->auSwizzle[uSrcArg] = ConvertSwizzle(psState, psSrc->u.uSwiz);
		psTestInst->u.psVec->auSwizzle[uSrcArg] = CombineSwizzles(psTestInst->u.psVec->auSwizzle[uSrcArg], uExtraSwizzle);

		/*
			Invert the negate modifier on the second source so the TEST instruction calculations
				
				SRC1 - SRC2
		*/
		if (uSrcArg == 1)
		{
			psTestInst->u.psVec->asSrcMod[1].bNegate = psTestInst->u.psVec->asSrcMod[1].bNegate ? IMG_FALSE : IMG_TRUE;
		}
	}
	AppendInst(psState, psCodeBlock, psTestInst);
}

IMG_INTERNAL
IMG_VOID ConvertDOT2ADDInstructionF32F16_Vec(PINTERMEDIATE_STATE	psState,
											 PCODEBLOCK				psCodeBlock,
											 PUNIFLEX_INST			psInputInst,
											 IMG_BOOL				bF16)
/*****************************************************************************
 FUNCTION	: ConvertDOT2ADDInstructionF32F16_Vec
    
 PURPOSE	: Convert a UFOP_DOT2ADD instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  bF16				- Precision of the instruction.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psDPInst;
	PINST		psADDInst;
	IMG_UINT32	uDot2AddTempResult;
	IMG_UINT32	uArg;
	IMG_UINT32	uSrc2UsedChanMask;

	/*
		DOT2ADD		DEST, SRC1, SRC2, SRC3

		DEST.x = SRC1.x * SRC2.x + SRC1.y * SRC2.y + SRC3.z
		DEST.y = DEST.x
		DEST.z = DEST.x
		DEST.w = DEST.w
	*/

	/*
		Get a temporary register to hold the intermediate
		result of the calculation.
	*/
	uDot2AddTempResult = GetNextRegister(psState);

	/*
		Create an instruction to calculation

			TEMP = SRC1.x * SRC2.x + SRC1.y * SRC2.y
	*/
	psDPInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psDPInst, IVDP2);

	psDPInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psDPInst->asDest[0].uNumber = uDot2AddTempResult;
	psDPInst->asDest[0].eFmt = bF16 ? UF_REGFORMAT_F16 : UF_REGFORMAT_F32;

	psDPInst->auDestMask[0] = USC_RED_CHAN_MASK;

	/*
		Convert the first two sources to the intermediate format.
	*/
	for (uArg = 0; uArg < 2; uArg++)
	{
		IMG_UINT32	uUsedChanMask = GetSourceLiveChans(psState, psInputInst, uArg);

		if (bF16)
		{
			GetSourceF16_Vec(psState, 
							 psCodeBlock,
							 &psInputInst->asSrc[uArg],
							 uUsedChanMask,
							 psDPInst,
							 uArg);
		}
		else
		{
			GetSourceF32_Vec(psState, 
							 psCodeBlock, 
							 &psInputInst->asSrc[uArg], 
							 uUsedChanMask,
							 psDPInst,
							 uArg);
		}

		psDPInst->u.psVec->auSwizzle[uArg] = ConvertSwizzle(psState, psInputInst->asSrc[uArg].u.uSwiz);
	}
	AppendInst(psState, psCodeBlock, psDPInst);

	/*
		Create a second instruction to calculation

			DEST = TEMP.x + SRC2.z
	*/
	psADDInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psADDInst, IVADD);
	SetupTempVecSource(psState, psADDInst, 0 /* uSrcIdx */, uDot2AddTempResult, bF16 ? UF_REGFORMAT_F16 : UF_REGFORMAT_F32);
	psADDInst->auDestMask[0] = psInputInst->sDest.u.byMask;
	uSrc2UsedChanMask = GetSourceLiveChans(psState, psInputInst, 2 /* uArg */);
	if (bF16)
	{
		GetDestinationF16_Vec(psState, &psInputInst->sDest, &psADDInst->asDest[0]);

		GetSourceF16_Vec(psState, 
						 psCodeBlock,
						 &psInputInst->asSrc[2],
						 uSrc2UsedChanMask,
						 psADDInst,
						 1 /* uSrcIdx */);
	}
	else
	{
		GetDestinationF32_Vec(psState, &psInputInst->sDest, &psADDInst->asDest[0]);

		GetSourceF32_Vec(psState, 
						 psCodeBlock, 
						 &psInputInst->asSrc[2], 
						 uSrc2UsedChanMask,
						 psADDInst,
						 1 /* uSrcIdx */);
	}
	psADDInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
	psADDInst->u.psVec->auSwizzle[1] = ConvertSwizzle(psState, psInputInst->asSrc[2].u.uSwiz);
	psADDInst->u.psVec->auSwizzle[1] = CombineSwizzles(psADDInst->u.psVec->auSwizzle[1], USEASM_SWIZZLE(Z, Z, Z, Z));
	AppendInst(psState, psCodeBlock, psADDInst);

	/*
		Generate instructions to apply any destination modifier on the instructions.
	*/
	GenerateDestModF32F16_Vec(psState, psCodeBlock, psInputInst, bF16);

	/*
		Generate instructions to store into a non-temporary destination.
	*/
	if (bF16)
	{
		StoreDestinationF16(psState, psCodeBlock, psInputInst, &psInputInst->sDest);
	}
	else
	{
		StoreDestinationF32(psState, psCodeBlock, psInputInst, &psInputInst->sDest, USC_TEMPREG_CVTVECF32DSTOUT);
	}
}

IMG_INTERNAL
IMG_VOID GeneratePerChanPredicateMOV(PINTERMEDIATE_STATE	psState,
									 PCODEBLOCK				psCodeBlock,
									 PUF_REGISTER			psInputDest,
									 IMG_UINT32				uPredicate,
									 UF_REGFORMAT			eFormat,
									 IMG_UINT32				uTempDest)
/*****************************************************************************
 FUNCTION	: GeneratePerChanPredicateMOV
    
 PURPOSE	: Generate instructions to copy one vector register to another with
			  a different predicate controlling the update of each channel.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputDest		- Vector destination.
			  uPredicate		- Predicate to control update of the destination.
			  eFormat			- Format of the vector to move.
			  uTempDest			- Vector source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		/*
			Is predicated write required for this channel
		*/
		if (psInputDest->u.byMask & (1 << uChan))
		{
			PINST		psVMOVInst;

			psVMOVInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psVMOVInst, IVMOV);

			if (eFormat == UF_REGFORMAT_F16)
			{
				ASSERT(psInputDest->eType == UFREG_TYPE_TEMP);

				psVMOVInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psVMOVInst->asDest[0].uNumber = ConvertTempRegisterNumberF32(psState, psInputDest->uNum, 0);
				psVMOVInst->asDest[0].eFmt = UF_REGFORMAT_F16;
			}
			else
			{	
				GetBaseDestinationF32_Vec(psState, psInputDest, &psVMOVInst->asDest[0]);
			}	

			/*
				Update only one channel at a time.
			*/
			psVMOVInst->auDestMask[0] = 1 << uChan;

			/*
				Get the predicate that controls update of this channel.
			*/
			GetInputPredicateInst(psState, psVMOVInst, uPredicate, uChan);

			/*
				Use the temporary destination of the main instruction
				as the source.
			*/
			SetupTempVecSource(psState, psVMOVInst, 0 /* uSrcIdx */, uTempDest, eFormat);
			psVMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

			AppendInst(psState, psCodeBlock, psVMOVInst);
		}
	}
}

IMG_INTERNAL
IMG_VOID ConvertVectorInstructionF32F16_Vec(PINTERMEDIATE_STATE		psState,
											PCODEBLOCK				psCodeBlock,
											PUNIFLEX_INST			psInputInst,
											IMG_BOOL				bF16)
/*****************************************************************************
 FUNCTION	: ConvertVectorInstructionF32F16_Vec
    
 PURPOSE	: Convert a vector instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  bF16				- Precision of the instruction.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST			psInst;
	IOPCODE			eHwOpcode;
	IMG_UINT32		uOutArg;
	IMG_UINT32		uNumSrcArgs;
	IMG_BOOL		bNegateSource2 = IMG_FALSE;
	UF_REGFORMAT	eFormat = bF16 ? UF_REGFORMAT_F16 : UF_REGFORMAT_F32;
	IMG_UINT32		uExtraSwizzle = USEASM_SWIZZLE(X, Y, Z, W);

	/*
		Skip an instruction not updtaing any channels in its destination.
	*/
	if (psInputInst->sDest.u.byMask == 0)
	{
		return;
	}

	/*
		Get the intermediate opcode corresponding to the input instruction
		opcode.
	*/
	switch (psInputInst->eOpCode)
	{
		case UFOP_MAD: eHwOpcode = IVMAD; break;
		case UFOP_MUL: eHwOpcode = IVMUL; break;
		case UFOP_ADD: eHwOpcode = IVADD; break;
		case UFOP_MIN: eHwOpcode = IVMIN; break;
		case UFOP_MAX: eHwOpcode = IVMAX; break;
		case UFOP_FRC: eHwOpcode = IVFRC; break;
		case UFOP_DSX: eHwOpcode = IVDSX; break;
		case UFOP_DSY: eHwOpcode = IVDSY; break;
		case UFOP_DOT4: eHwOpcode = IVDP; break;
		case UFOP_DOT3: eHwOpcode = IVDP3; break;
		case UFOP_DOT4_CP: eHwOpcode = IVDP_CP; break;
		case UFOP_MOV: eHwOpcode = IVMOV; break;
		case UFOP_MOVCBIT: eHwOpcode = IVMOVCBIT; break;
		case UFOP_CMP: eHwOpcode = IVMOVC; break;
		case UFOP_CMPLT: eHwOpcode = IVMOVC; break;
		case UFOP_CEIL: eHwOpcode = IVFRC; break;
		case UFOP_TRC: eHwOpcode = IVTRC; break;
		case UFOP_MOV_CP: eHwOpcode = IVDP_CP; break;
		case UFOP_SETBGE:
		case UFOP_SETBNE:
		case UFOP_SETBEQ:
		case UFOP_SETBLT:
		case UFOP_SLT:
		case UFOP_SGE: eHwOpcode = IVTESTMASK; break;
		case UFOP_SETP: eHwOpcode = IVTEST; break;
		default: imgabort();
	}

	psInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psInst, eHwOpcode);

	uNumSrcArgs = g_asInputInstDesc[psInputInst->eOpCode].uNumSrcArgs;
	if (psInputInst->eOpCode == UFOP_FRC ||
		psInputInst->eOpCode == UFOP_CEIL)
	{
		uNumSrcArgs = 2;
	}

	if (psInputInst->eOpCode == UFOP_SETP)
	{
		IMG_UINT32			uCompParam;
		UFREG_COMPCHANOP	eChanOp;
		UFREG_COMPOP		eCompOp;

		/*
			Unpack the comparison operation from the second instruction source.
		*/
		ASSERT(psInputInst->asSrc[1].eType == UFREG_TYPE_COMPOP);
		uCompParam = psInputInst->asSrc[1].uNum;

		eCompOp = (UFREG_COMPOP)(uCompParam & (~UFREG_COMPCHANOP_MASK));
		eChanOp = (UFREG_COMPCHANOP)(((uCompParam & UFREG_COMPCHANOP_MASK) >> UFREG_COMPCHANOP_SHIFT));

		/*
			Generate ALU_RESULT = SOURCE1 - SOURCE2
		*/
		psInst->u.psVec->sTest.eTestOpcode = IVADD;
		bNegateSource2 = IMG_TRUE;

		/*
			Convert the type of test to perform on the ALU result.
		*/
		psInst->u.psVec->sTest.eType = ConvertCompOpToIntermediate(psState, eCompOp);

		/*
			Convert the operation to combine the tests on different channels
			of the ALU result.
		*/
		switch (eChanOp)
		{
			case UFREG_COMPCHANOP_ANDALL:
			{
				ASSERT(g_abSingleBitSet[psInputInst->sDest.u.byMask]);
				psInst->u.psVec->sTest.eChanSel = VTEST_CHANSEL_ANDALL;
				break;
			}
			case UFREG_COMPCHANOP_ORALL:
			{
				ASSERT(g_abSingleBitSet[psInputInst->sDest.u.byMask]);
				psInst->u.psVec->sTest.eChanSel = VTEST_CHANSEL_ORALL;
				break;
			}
			default:
			{
				if (g_abSingleBitSet[psInputInst->sDest.u.byMask])
				{
					psInst->u.psVec->sTest.eChanSel = VTEST_CHANSEL_XCHAN;
				}
				else
				{
					psInst->u.psVec->sTest.eChanSel = VTEST_CHANSEL_PERCHAN;
				}
				break;
			}
		}

		/*
			Ignore the comparison operation parameter when setting up the sources to
			the intermediate instruction.
		*/
		uNumSrcArgs = 2;
	}


	/*
		Get the intermediate representation of the destination.
	*/
	if (psInputInst->eOpCode == UFOP_SETP)
	{
		PUF_REGISTER	psInputDest = &psInputInst->sDest;
		IMG_UINT32		uChan;
		IMG_UINT32		uBasePredDest;

		ASSERT(psInputDest->eType == UFREG_TYPE_PREDICATE);
		ASSERT(psInputDest->uNum < psState->uInputPredicateRegisterCount);

		uBasePredDest = USC_PREDREG_P0X + psInputDest->uNum * CHANNELS_PER_INPUT_REGISTER;

		SetDestCount(psState, psInst, VTEST_PREDICATE_ONLY_DEST_COUNT);

		if (psInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_PERCHAN)
		{
			/*
				Write a vector of four predicates.
			*/

			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				PARG	psDest = &psInst->asDest[uChan];

				psDest->uType = USEASM_REGTYPE_PREDICATE;
				if ((psInputDest->u.byMask & (1 << uChan)) != 0)
				{
					psDest->uNumber = uBasePredDest + uChan;
				}
				else
				{
					/*
						Set a channel not written by the input instruction to
						an unused predicate register.
					*/
					psDest->uNumber = psState->uNumPredicates++;
				}
			}

			GetVectorPredicateInst(psState, psInst, psInputInst);
		}
		else
		{
			IMG_UINT32	uWrittenChan;

			/*
				Get the single channel written by the input instruction.
			*/
			ASSERT(g_abSingleBitSet[psInputDest->u.byMask]);
			uWrittenChan = (IMG_UINT32)g_aiSingleComponent[psInputDest->u.byMask];

			if (psInst->u.psVec->sTest.eChanSel == VTEST_CHANSEL_XCHAN)
			{
				/*
					Convert from the input instruction testing an arbitary channel
					to the intermediate instruction which tests the X channel by
					swizzling the sources.
				*/
				uExtraSwizzle = g_auReplicateSwizzles[uWrittenChan];
			}

			MakeVectorPredicateDest(psState, psInst, uBasePredDest + uWrittenChan);

			GetInputPredicateInst(psState, psInst, psInputInst->uPredicate, uWrittenChan);
		}
	}
	else
	{
		if (bF16)
		{
			GetDestinationF16_Vec(psState, &psInputInst->sDest, &psInst->asDest[0]);
		}
		else
		{	
			GetDestinationF32_Vec(psState, &psInputInst->sDest, &psInst->asDest[0]);
		}	

		GetVectorPredicateInst(psState, psInst, psInputInst);

		psInst->auDestMask[0] = psInputInst->sDest.u.byMask;
	}

	/*
		For CMP convert
			CMP	DEST, SRC0, SRC1, SRC2
				(DEST = (SRC0 >= 0) ? SRC1 : SRC2)
		to
			MOVC DEST, SRC0 < 0, SRC2, SRC1
				(DEST = (SRC0 < 0) ? SRC2 : SRC1)
	*/
	if (psInputInst->eOpCode == UFOP_CMP || psInputInst->eOpCode == UFOP_CMPLT)
	{
		psInst->u.psVec->eMOVCTestType = TEST_TYPE_LT_ZERO;
	}

	/*
		Store the clipplane to write.
	*/
	if (psInputInst->eOpCode == UFOP_DOT4_CP || psInputInst->eOpCode == UFOP_MOV_CP)
	{
		IMG_UINT32	uClipPlaneSrcNum;

		if (psInputInst->eOpCode == UFOP_DOT4_CP)
		{
			uClipPlaneSrcNum = 2;
		}
		else
		{
			ASSERT(psInputInst->eOpCode == UFOP_MOV_CP);
			uClipPlaneSrcNum = 1;
		}

		ASSERT(psInputInst->asSrc[uClipPlaneSrcNum].eType == UFREG_TYPE_CLIPPLANE);
		psInst->u.psVec->uClipPlaneOutput = psInputInst->asSrc[uClipPlaneSrcNum].uNum;

		/*
			Ignore the clipplane argument to the input instruction when
			converting the sources.
		*/
		uNumSrcArgs = 2;
	}

	/*
		Set up the test parameters for a SETBxx instruction.
	*/
	if (psInputInst->eOpCode == UFOP_SETBGE ||
		psInputInst->eOpCode == UFOP_SETBNE ||
		psInputInst->eOpCode == UFOP_SETBEQ ||
		psInputInst->eOpCode == UFOP_SETBLT ||
		psInputInst->eOpCode == UFOP_SLT ||
		psInputInst->eOpCode == UFOP_SGE)
	{
		/*
			Generate ALU_RESULT = SOURCE1 - SOURCE2
		*/
		psInst->u.psVec->sTest.eTestOpcode = IVADD;
		bNegateSource2 = IMG_TRUE;

		/*
			Test the ALU result against zero and produce 0xFFFFFFFF if the test passes or
			0x00000000 if the test fails (or 1.0f/0.0f for SETLT/SETGE).
		*/
		switch (psInputInst->eOpCode)
		{
			case UFOP_SETBEQ: psInst->u.psVec->sTest.eType = TEST_TYPE_EQ_ZERO; break;
			case UFOP_SETBNE: psInst->u.psVec->sTest.eType = TEST_TYPE_NEQ_ZERO; break;
			case UFOP_SETBGE: psInst->u.psVec->sTest.eType = TEST_TYPE_GTE_ZERO; break;
			case UFOP_SETBLT: psInst->u.psVec->sTest.eType = TEST_TYPE_LT_ZERO; break;
			case UFOP_SLT: psInst->u.psVec->sTest.eType = TEST_TYPE_LT_ZERO; break;
			case UFOP_SGE: psInst->u.psVec->sTest.eType = TEST_TYPE_GTE_ZERO; break;
			default: imgabort();
		}

		if (psInputInst->eOpCode == UFOP_SLT || psInputInst->eOpCode == UFOP_SGE)
		{
			psInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_NUM;
		}
		else
		{
			psInst->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_PREC;
		}
	}

	for (uOutArg = 0; uOutArg < uNumSrcArgs; uOutArg++)
	{
		IMG_UINT32	uInArg;
		IMG_UINT32	uUsedChanMask;

		if (psInputInst->eOpCode == UFOP_CMP)
		{
			/*
				For CMP swap the second and third sources.
			*/
			switch (uOutArg)
			{
				case 0: uInArg = 0; break;
				case 1: uInArg = 2; break;
				case 2: uInArg = 1; break;
				default: imgabort();
			}
		}
		else if (psInputInst->eOpCode == UFOP_FRC)
		{
			/*
				For FRC duplicate the single input source
				argument to two source arguments in the intermediate
				format.
			*/
			uInArg = 0;
		}
		else if (psInputInst->eOpCode == UFOP_CEIL)
		{
			/*
				Convert
					CEIL	DEST, SRC
				->
					SUBFLR	DEST, 0, -SRC		(DEST=0-FLOOR(-SRC))
			*/
			if (uOutArg == 0)
			{
				SetupImmediateSource(psInst, 0 /* uArgIdx */, 0.0f, eFormat);
				continue;
			}
			else
			{
				uInArg = 0;
			}
		}
		else if (psInputInst->eOpCode == UFOP_MOV_CP)
		{
			/*
				Convert
					MOVCP		DEST.X, SRC
				->
					VDP_CP		DEST.X, SRC.X000, (1, 1, 1, 1)
			*/
			if (uOutArg == 1)
			{
				SetupImmediateSource(psInst, 1 /* uSrcIdx */, 1.0f /* fImmValue */, eFormat);
				continue;
			}
			else
			{
				ASSERT(uOutArg == 0);
				uInArg = 0;
				uExtraSwizzle = USEASM_SWIZZLE(X, 0, 0, 0);
			}
		}
		else if (psInputInst->eOpCode == UFOP_SETP)
		{
			/*
				Ignore the second source which contains the comparison operator.
			*/
			if (uOutArg == 1)
			{
				uInArg = 2;
			}
			else
			{
				uInArg = 0;
			}
		}
		else
		{
			uInArg = uOutArg;
		}

		/*
			Get an intermediate representation of the source argument.
		*/
		uUsedChanMask = GetSourceLiveChans(psState, psInputInst, uInArg);
		if (bF16)
		{
			GetSourceF16_Vec(psState, 
							 psCodeBlock,
							 &psInputInst->asSrc[uInArg],
							 uUsedChanMask,
							 psInst,
							 uOutArg);
		}
		else
		{
			GetSourceF32_Vec(psState, 
							 psCodeBlock, 
							 &psInputInst->asSrc[uInArg], 
							 uUsedChanMask,
							 psInst,
							 uOutArg);
		}

		/*
			For CEIL invert the negate modifiers on the second source.
		*/
		if (psInputInst->eOpCode == UFOP_CEIL || (uOutArg == 1 && bNegateSource2))
		{
			psInst->u.psVec->asSrcMod[uOutArg].bNegate = !psInst->u.psVec->asSrcMod[uOutArg].bNegate;
		}

		/*
			Convert the source swizzle.
		*/
		psInst->u.psVec->auSwizzle[uOutArg] = ConvertSwizzle(psState, psInputInst->asSrc[uInArg].u.uSwiz);
		psInst->u.psVec->auSwizzle[uOutArg] = CombineSwizzles(psInst->u.psVec->auSwizzle[uOutArg], uExtraSwizzle);
	}
	AppendInst(psState, psCodeBlock, psInst);

	/*
		Generate instructions to apply any destination modifier on the instruction.
	*/
	GenerateDestModF32F16_Vec(psState, psCodeBlock, psInputInst, bF16);

	/*
		Generate instructions to store into a non-temporary destination.
	*/
	if (bF16)
	{
		StoreDestinationF16(psState, psCodeBlock, psInputInst, &psInputInst->sDest);
	}
	else
	{
		StoreDestinationF32(psState, psCodeBlock, psInputInst, &psInputInst->sDest, USC_TEMPREG_CVTVECF32DSTOUT);
	}
}

IMG_INTERNAL
IMG_VOID StoreIntoVectorDest(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 PUNIFLEX_INST			psInputInst,
							 PUF_REGISTER			psInputDest)
/*****************************************************************************
 FUNCTION	: StoreIntoVectorDest
    
 PURPOSE	: 

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Input instruction to store for.
			  psInputDest		- Input destination to store into.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psMkVecInst;
	IMG_UINT32	uChan;

	psMkVecInst = AllocateInst(psState, NULL);

	SetOpcode(psState, psMkVecInst, IVMOV);

	GetDestinationF32_Vec(psState, &psInputInst->sDest, &psMkVecInst->asDest[0]);
	psMkVecInst->auDestMask[0] = psInputInst->sDest.u.byMask;

	GetVectorPredicateInst(psState, psMkVecInst, psInputInst);

	psMkVecInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;

	for (uChan = 0; uChan < 4; uChan++)
	{
		psMkVecInst->asArg[1 + uChan].uType = USEASM_REGTYPE_TEMP;
		psMkVecInst->asArg[1 + uChan].uNumber = USC_TEMPREG_VECTEMPDEST + uChan;
	}

	psMkVecInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psMkVecInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;

	AppendInst(psState, psCodeBlock, psMkVecInst);

	StoreDestinationF32(psState, psCodeBlock, psInputInst, psInputDest, USC_TEMPREG_CVTVECF32DSTOUT);
}

static
IMG_VOID MoveVector(PINTERMEDIATE_STATE	psState,
				    PCODEBLOCK			psCodeBlock,
					PUNIFLEX_INST		psInputInst,
					IMG_UINT32			uInputSrcIdx,
					IMG_UINT32			uSrcRegNum,
					IMG_UINT32			uDestNum,
					IMG_UINT32			uDestMask,
					UF_REGFORMAT		eDestFmt,
					UF_REGFORMAT		eSrcFmt,
					IMG_UINT32			uSwizzle)
/*****************************************************************************
 FUNCTION	: MoveVector
    
 PURPOSE	: Move an input vector source into a temporary register applying
			  any swizzles or source modifiers with optional format conversion
			  and/or projection..

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  uInputSrcIdx		- Index of the input source to move.
			  uDestNum			- Temporary register to write.
			  uDestMask			- Mask of channnels to write.
			  eDestFmt			- Format of the temporary register.
			  eSrcFmt			- Format of the input source.
			  uSwizzle			- Swizzle to apply to the written data.
			  bProjected		- If TRUE apply projection to the written data.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST			psMOVInst;

	/*
		Move the intermediate vector into the destination register applying
		any source modifier or swizzle.
	*/
	psMOVInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psMOVInst, IVMOV);
	psMOVInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psMOVInst->asDest[0].uNumber = uDestNum;
	psMOVInst->asDest[0].eFmt = eDestFmt;
	psMOVInst->auDestMask[0] = uDestMask;

	if (uSrcRegNum == USC_UNDEF)
	{
		IMG_UINT32		uSourceLiveChans;
		PUF_REGISTER	psInputSrc;

		/*
			Get the argument representing the vector to unpack in the input format.
		*/
		psInputSrc = &psInputInst->asSrc[uInputSrcIdx];
		uSourceLiveChans = GetSourceLiveChans(psState, psInputInst, uInputSrcIdx);

		if (eSrcFmt == UF_REGFORMAT_F16)
		{
			GetSourceF16_Vec(psState, 
							 psCodeBlock, 
							 psInputSrc, 
							 uSourceLiveChans,
							 psMOVInst,
							 0 /* uSrcIdx */);
		}
		else
		{
			GetSourceF32_Vec(psState, 
							 psCodeBlock, 
							 psInputSrc,
							 uSourceLiveChans,
							 psMOVInst,
							 0 /* uSrcIdx */);
		}	
		psMOVInst->u.psVec->auSwizzle[0] = ConvertSwizzle(psState, psInputSrc->u.uSwiz);
	}
	else
	{
		SetupTempVecSource(psState, psMOVInst, 0 /* uSrcIdx */, uSrcRegNum, eSrcFmt);
		psMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	}
	psMOVInst->u.psVec->auSwizzle[0] = CombineSwizzles(psMOVInst->u.psVec->auSwizzle[0], uSwizzle);
	AppendInst(psState, psCodeBlock, psMOVInst);
}

IMG_INTERNAL
IMG_VOID GetProjectionF16F32Vec(PINTERMEDIATE_STATE			psState,
								PCODEBLOCK					psCodeBlock,
								PUNIFLEX_INST				psInputInst,
								PSAMPLE_PROJECTION			psProj,
								UF_REGFORMAT				eCoordFormat,
								IMG_BOOL					bF16)
{
	UF_REGFORMAT	eProjFormat;

	eProjFormat = bF16 ? UF_REGFORMAT_F16 : UF_REGFORMAT_F32;

	InitInstArg(&psProj->sProj);
	psProj->sProj.uType = USEASM_REGTYPE_TEMP;
	psProj->sProj.uNumber = GetNextRegister(psState);
	psProj->sProj.eFmt = eCoordFormat;

	psProj->uProjChannel = USC_W_CHAN;
	psProj->uProjMask = USC_X_CHAN_MASK;

	MoveVector(psState, 
			   psCodeBlock, 
			   psInputInst,
			   0 /* uInputSrcIdx */,
			   USC_UNDEF /* uSrcRegNum */,
			   psProj->sProj.uNumber, 
			   USC_W_CHAN_MASK, 
			   eCoordFormat, 
			   eProjFormat,
			   USEASM_SWIZZLE(X, Y, Z, W));
}

IMG_INTERNAL 
IMG_VOID GetSampleCoordinatesF16F32Vec(PINTERMEDIATE_STATE	psState,
									   PCODEBLOCK			psCodeBlock,
									   PUNIFLEX_INST		psInputInst,
									   IMG_UINT32			uDimensionality,
									   PSAMPLE_COORDINATES	psCoordinates,
									   IMG_BOOL				bPCF)
/*****************************************************************************
 FUNCTION	: GetSampleCoordinatesF16F32Vec
    
 PURPOSE	: Extract coordinates for a texture sample instruction

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psInputSrc	- Input texture sample instructions.
			  uDimensionality
							- Dimensionality of the texure sampled.
			  bPCF			- TRUE if the texture sample includes a PCF
							comparison.

 OUTPUT		: psCoordinates	- Returns details of the intermediate format
							texture coordinates.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uDimensionalityMask = (1 << uDimensionality) - 1;
	
	MoveVector(psState, 
			   psCodeBlock, 
			   psInputInst,
			   0 /* uInputSrcIdx */,
			   USC_UNDEF /* uSrcRegNum */,
			   psCoordinates->uCoordStart, 
			   uDimensionalityMask, 
			   psCoordinates->eCoordFormat, 
			   psInputInst->asSrc[0].eFormat,
			   USEASM_SWIZZLE(X, Y, Z, W));

	if (bPCF && (psInputInst->eOpCode == UFOP_LDC || psInputInst->eOpCode == UFOP_LDCLZ))
	{
		MoveVector(psState,
				   psCodeBlock,
				   psInputInst,
				   2 /* uInputSrcIdx */,
				   USC_UNDEF /* uSrcRegNum */,
				   psCoordinates->uCoordStart,
				   1 << (uDimensionality - 1),
				   psCoordinates->eCoordFormat,
				   psInputInst->asSrc[2].eFormat,
				   USEASM_SWIZZLE(W, W, W, W));
	}
}

IMG_INTERNAL
IMG_VOID GetLODAdjustmentF16F32Vec(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psCodeBlock,
								   PUNIFLEX_INST		psInputInst,
								   UF_REGFORMAT			eSrcFmt,
								   PSAMPLE_LOD_ADJUST	psLODAdjust)
/*****************************************************************************
 FUNCTION	: GetLODAdjustmentF16F32Vec

 PURPOSE	: Generate instructions to put the LOD adjustment value in
			  a fixed register.

 PARAMETERS	: psState		- Compiler state
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Input instruction.
			  eSrcFmt		- Format of the input LOD source.
			  eLODFmt		- Format of the LOD expected by the texture
							sample instruction.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uInputSrcIdx;

	if(psInputInst->eOpCode != UFOP_LD2DMS)
	{
		uInputSrcIdx = 2;
	}
	else
	{
		uInputSrcIdx = 3;
	}
	MoveVector(psState, 
			   psCodeBlock, 
			   psInputInst,
			   uInputSrcIdx /* uInputSrcIdx */, 
			   USC_UNDEF /* uSrcRegNum */,
			   psLODAdjust->uLODTemp,
			   USC_X_CHAN_MASK,
			   psLODAdjust->eLODFormat,
			   eSrcFmt,
			   USEASM_SWIZZLE(W, W, W, W));
}

IMG_INTERNAL
IMG_VOID UnpackGradients_Vec(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 PUNIFLEX_INST			psInputInst,
							 IMG_UINT32				uSrcRegNum,
							 IMG_UINT32				uGradDim,
							 IMG_UINT32				uAxis,
							 UF_REGFORMAT			eSourceFormat,
							 PSAMPLE_GRADIENTS		psGradients)
/*****************************************************************************
 FUNCTION	: UnpackGradients_Vec

 PURPOSE	: Generate instructions to put the gradients used by an input
			  LDD instruction into the format/layout expected by the hardware
			  texture sample instruction.

 PARAMETERS	: psState		- Compiler state
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- If non-NULL the input instruction whose sources
							are used for the gradients.
			  uSrcRegNum	- If not USC_UNDEF the intermediate temporary register
							which contains the gradient data.
			  uAxis			- 0: Get the gradients in the X direction.
							  1: Get the gradients in the Y direction.
			  eSourceFormat	- Format of the gradients source to the input instruction.
			  eGradFormat	- Format of the gradients expected by the intermediate
							texture sample instruction.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uDestMask;
	IMG_UINT32	uDestRegNum;
	IMG_UINT32	uSwizzle;

	switch (uGradDim)
	{
		case 1:
		{
			/*
				In the intermediate format the gradients
				are represented by a single vector register with
				contents
					(dUdX, dUdY, _, _)

				so generate
					MOV	GRAD.X, XSRC.X
					MOV GRAD.Y, YSRC.X
			*/
			if (uAxis == 0)
			{
				uDestMask = USC_X_CHAN_MASK;
			}
			else
			{
				uDestMask = USC_Y_CHAN_MASK;
			}
			uDestRegNum = psGradients->uGradStart;
			uSwizzle = USEASM_SWIZZLE(X, X, X, X);
			break;
		}
		case 2:
		{
			/*
				In the intermediate format the gradients are
				represented by a single vector register with
				contents
					(dUdX, dVdX, dUdY, dVdY)

				so generate
					MOV	GRAD.XY, XSRC.XY
					MOV	GRAD.ZW, YSRC.XY
			*/
			if (uAxis == 0)
			{
				uDestMask = USC_XY_CHAN_MASK;
			}
			else
			{
				uDestMask = USC_ZW_CHAN_MASK;
			}
			uDestRegNum = psGradients->uGradStart;
			uSwizzle = USEASM_SWIZZLE(X, Y, X, Y);
			break;
		}
		case 3:
		{
			/*
				In the intermediate format the gradients are
				represented by two vector registers with contents
					(dUdX, dVdX, dSdX, _)
					(dUdY, dVdY, dSdY, _)
				
				so generate
					MOV	GRAD0.XYZ, XSRC
					MOV GRAD1.XYZ, YSRC
			*/
			uDestMask = USC_XYZ_CHAN_MASK;
			uDestRegNum = psGradients->uGradStart + uAxis;
			uSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
			break;
		}
		default: imgabort();
	}

	MoveVector(psState,
			   psCodeBlock,
			   psInputInst,
			   2 + uAxis /* uInputSrc */,
			   uSrcRegNum,
			   uDestRegNum,
			   uDestMask,
			   psGradients->eGradFormat,
			   eSourceFormat,
			   uSwizzle);
}

IMG_INTERNAL
IMG_VOID GetGradientsF16F32Vec(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psCodeBlock,
							   PUNIFLEX_INST		psInputInst,
							   IMG_UINT32			uGradDim,
							   IMG_UINT32			uAxis,
							   UF_REGFORMAT			eSourceFormat,
							   PSAMPLE_GRADIENTS	psGradients)
/*****************************************************************************
 FUNCTION	: GetGradientsF16F32Vec

 PURPOSE	: Generate instructions to put the gradients used by an input
			  LDD instruction into the format/layout expected by the hardware
			  texture sample instruction.

 PARAMETERS	: psState		- Compiler state
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Input instruction.
			  uAxis			- 0: Get the gradients in the X direction.
							  1: Get the gradients in the Y direction.
			  eSourceFormat	- Format of the gradients source to the input instruction.
			  eGradFormat	- Format of the gradients expected by the intermediate
							texture sample instruction.

 RETURNS	: None.
*****************************************************************************/
{
	UnpackGradients_Vec(psState, 
						psCodeBlock, 
						psInputInst, 
						USC_UNDEF /* uSrcRegNum */, 
						uGradDim, 
						uAxis, 
						eSourceFormat, 
						psGradients);
}

IMG_INTERNAL
IMG_VOID ConvertTextureArrayIndexToU16_Vec(PINTERMEDIATE_STATE	psState,
										   PCODEBLOCK			psCodeBlock,
										   IMG_UINT32			uTexture,
										   IMG_UINT32			uFltArrayIndex,
										   UF_REGFORMAT			eArrayIndexFormat,
										   IMG_UINT32			uU16ArrayIndex)
/*****************************************************************************
 FUNCTION	: ConvertTextureArrayIndexToU16_Vec

 PURPOSE	: Convert an index into a texture array to U16 format.

 PARAMETERS	: psState		- Compiler state
			  psCodeBlock	- Block for any extra instructions.
			  uTexture		- Index of the texture to sample.
			  uFltArrayIndex
							- Temporary register containing the array index
							in floating point format.
			  eArrayIndexFormat
							- Format of the array index.
			  u16ArrayIndex	- Destination register for the converted array index.
			  

 RETURNS	: None.
*****************************************************************************/
{
	PINST		psVMinInst;
	PINST		psCvtInst;
	PINST		psMkvecInst;
	IMG_UINT32	uClampedTemp = GetNextRegister(psState);
	IMG_UINT32	uMaxIndexTemp = GetNextRegister(psState);
	IMG_UINT32	uChan;

	/*
		Load the maximum possible index into the array.
	*/
	psMkvecInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psMkvecInst, IVMOV);
	psMkvecInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psMkvecInst->asDest[0].uNumber = uMaxIndexTemp;
	psMkvecInst->asDest[0].eFmt = UF_REGFORMAT_F32;
	psMkvecInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psMkvecInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
	psMkvecInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;
	for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
	{
		PARG	psArg = &psMkvecInst->asArg[1 + uChan];

		if (uChan == 0)
		{
			LoadMaxTexarrayIndex(psState, psCodeBlock, uTexture, psArg);
		}
		else
		{
			psArg->uType = USC_REGTYPE_UNUSEDSOURCE;
		}
	}
	AppendInst(psState, psCodeBlock, psMkvecInst);

	/*
		Clamp the array index to the number of elements in the texture array.
	*/
	psVMinInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psVMinInst, IVMIN);
	psVMinInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psVMinInst->asDest[0].uNumber = uClampedTemp;
	SetupTempVecSource(psState, psVMinInst, 0 /* uSrcIdx */, uFltArrayIndex, eArrayIndexFormat);
	psVMinInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	SetupTempVecSource(psState, psVMinInst, 1 /* uSrcIdx */, uMaxIndexTemp, UF_REGFORMAT_F32);
	psVMinInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);
	AppendInst(psState, psCodeBlock, psVMinInst);

	/*
		Convert the array index to U16.
	*/
	psCvtInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psCvtInst, IVPCKU16FLT);
	psCvtInst->auDestMask[0] = USC_XY_CHAN_MASK;
	psCvtInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psCvtInst->asDest[0].uNumber = uU16ArrayIndex;
	SetupTempVecSource(psState, psCvtInst, 0 /* uSrcIdx */, uClampedTemp, UF_REGFORMAT_F32);
	psCvtInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	AppendInst(psState, psCodeBlock, psCvtInst);

	/*
		Set the top word of the array index to zero.
	*/
	psCvtInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psCvtInst, IUNPCKU16U16);
	psCvtInst->auDestMask[0] = USC_ZW_CHAN_MASK;
	psCvtInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psCvtInst->asDest[0].uNumber = uU16ArrayIndex;
	psCvtInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
	psCvtInst->asArg[0].uNumber = 0;
	AppendInst(psState, psCodeBlock, psCvtInst);
}

IMG_INTERNAL
IMG_VOID GetSampleCoordinateArrayIndexFlt_Vec(PINTERMEDIATE_STATE	psState, 
											  PCODEBLOCK			psCodeBlock, 
											  PUNIFLEX_INST			psInputInst, 
											  IMG_UINT32			uSrcChan, 
											  IMG_UINT32			uTexture,
											  IMG_UINT32			uArrayIndexTemp)
/*****************************************************************************
 FUNCTION	: GetSampleCoordinateArrayIndexF32
    
 PURPOSE	: Extract the source array index for a texture sample instruction

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Input texture sample instruction.
			  uSrcChan		- Channel in the texture coordinate source which is the
							array index.
			  uTexture		- Index of the texture to sample.
			  uArrayIndexTemp
							- Destination for the extracted index.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32		uCoordTemp = GetNextRegister(psState);
	UF_REGFORMAT	eCoordFmt = psInputInst->asSrc[0].eFormat;

	/*
		Take the array index from the next channel after the channels corresponding to the
		dimensionality of the texture.
	*/
	MoveVector(psState,
			   psCodeBlock,
			   psInputInst,
			   0 /* uInputSrcIdx */,
			   USC_UNDEF /* uSrcRegNum */,
			   uCoordTemp,
			   USC_X_CHAN_MASK,
			   eCoordFmt,
			   psInputInst->asSrc[0].eFormat,
			   g_auReplicateSwizzles[uSrcChan]);

	/*
		Convert the array index to U16 format.
	*/
	ConvertTextureArrayIndexToU16_Vec(psState, psCodeBlock, uTexture, uCoordTemp, eCoordFmt, uArrayIndexTemp);
}

static
IMG_VOID ConvertInteger16ToF32_Vec(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psCodeBlock,
								   PUNIFLEX_INST		psInputInst,
								   IMG_BOOL				bF16,
								   UF_REGFORMAT			eSrcFormat)
/*****************************************************************************
 FUNCTION	: ConvertInteger16ToF32_Vec
    
 PURPOSE	: Convert an input instruction converting from U16/I16 to F16/F32
			  to the intermediate format.

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Input MOV instruction.
			  bF16			- TRUE if the instruction destination is F16.
							  FALSE if the instruction destination is F16.
			  eSrcFormat	- Format of the move source.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	ARG			sDest;

	if (bF16)
	{
		GetDestinationF16_Vec(psState, &psInputInst->sDest, &sDest);
	}
	else
	{
		GetDestinationF32_Vec(psState, &psInputInst->sDest, &sDest);
	}

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		ARG						sSrcChan;
		FLOAT_SOURCE_MODIFIER	sSrcMod;
		PINST					psPackInst;

		if ((psInputInst->sDest.u.byMask & (1 << uChan)) == 0)
		{
			continue;
		}

		/*
			Convert the move source argument to the intermediate format.
		*/
		GetSourceTypeless(psState,
						  psCodeBlock,
						  &psInputInst->asSrc[0],
						  uChan,
						  &sSrcChan,
						  IMG_TRUE /* bAllowSourceMod */,
						  &sSrcMod);

		/*
			Apply any negate modifier to the source register.
		*/
		ASSERT(!sSrcMod.bAbsolute);
		if (sSrcMod.bNegate)
		{
			IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, &sSrcChan, &sSrcChan);
		}

		/*
			Create an instruction to convert the source register to F16/F32.
		*/
		psPackInst = AllocateInst(psState, IMG_NULL);
		if (eSrcFormat == UF_REGFORMAT_I16)
		{
			SetOpcode(psState, psPackInst, IVPCKFLTS16);
		}
		else
		{
			ASSERT(eSrcFormat == UF_REGFORMAT_U16);
			SetOpcode(psState, psPackInst, IVPCKFLTU16);
		}
		psPackInst->asDest[0] = sDest;
		psPackInst->auDestMask[0] = 1 << uChan;
		psPackInst->asArg[0] = sSrcChan;
		psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
		AppendInst(psState, psCodeBlock, psPackInst);
	}

	if (bF16)
	{
		StoreDestinationF16(psState, psCodeBlock, psInputInst, &psInputInst->sDest);
	}
	else
	{
		StoreDestinationF32(psState, psCodeBlock, psInputInst, &psInputInst->sDest, USC_TEMPREG_CVTVECF32DSTOUT);
	}
}

static
IMG_VOID ConvertInteger32ToF32_Vec(PINTERMEDIATE_STATE	psState,
								   PCODEBLOCK			psCodeBlock,
								   PUNIFLEX_INST		psInputInst,
								   IMG_BOOL				bF16,
								   UF_REGFORMAT			eSrcFormat)
/*****************************************************************************
 FUNCTION	: ConvertInteger32ToF32_Vec
    
 PURPOSE	: Convert an input instruction converting from U32/I32 to F16/F32
			  to the intermediate format.

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Input MOV instruction.
			  bF16			- TRUE if the instruction destination is F16.
							  FALSE if the instruction destination is F16.
			  eSrcFormat	- Format of the move source.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32		uChan;
	IMG_UINT32		uPackTemp;
	PINST			psVMADInst;
	UF_REGFORMAT	eDestFmt = bF16 ? UF_REGFORMAT_F16 : UF_REGFORMAT_F32;

	uPackTemp = GetNextRegisterCount(psState, 2);

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		ARG						sSrcChan;
		FLOAT_SOURCE_MODIFIER	sSrcMod;
		IMG_UINT32				uHalf;

		if ((psInputInst->sDest.u.byMask & (1 << uChan)) == 0)
		{
			continue;
		}

		/*
			Convert the move source argument to the intermediate format.
		*/
		GetSourceTypeless(psState,
						  psCodeBlock,
						  &psInputInst->asSrc[0],
						  uChan,
						  &sSrcChan,
						  IMG_TRUE /* bAllowSourceMod */,
						  &sSrcMod);

		/*
			Apply any negate modifier to the source register.
		*/
		ASSERT(!sSrcMod.bAbsolute);
		if (sSrcMod.bNegate)
		{
			IntegerNegate(psState, psCodeBlock, NULL /* psInsertInstBefore */, &sSrcChan, &sSrcChan);
		}

		/*
			Get two registers containing each word of the source converted to F32.
		*/
		for (uHalf = 0; uHalf < 2; uHalf++)
		{
			PINST		psPackInst;

			psPackInst = AllocateInst(psState, IMG_NULL);
			if (eSrcFormat == UF_REGFORMAT_I32 && uHalf == 1)
			{
				SetOpcode(psState, psPackInst, IVPCKFLTS16);
			}
			else
			{
				SetOpcode(psState, psPackInst, IVPCKFLTU16);
			}
			psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asDest[0].uNumber = uPackTemp + uHalf;
			psPackInst->asDest[0].eFmt = eDestFmt;
			psPackInst->auDestMask[0] = 1 << uChan;
			psPackInst->asArg[0] = sSrcChan;
			if (uHalf == 0)
			{
				psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
			}
			else
			{
				psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Y, Y, Y, Y);
			}
			AppendInst(psState, psCodeBlock, psPackInst);
		}
	}

	/*
		RESULT = HIGH_HALF * 65536 + LOW_HALF
	*/
	psVMADInst = AllocateInst(psState, NULL);

	SetOpcode(psState, psVMADInst, IVMAD);

	if (bF16)
	{
		GetDestinationF16_Vec(psState, &psInputInst->sDest, &psVMADInst->asDest[0]);
	}
	else
	{
		GetDestinationF32_Vec(psState, &psInputInst->sDest, &psVMADInst->asDest[0]);
	}

	psVMADInst->auDestMask[0] = psInputInst->sDest.u.byMask;

	GetVectorPredicateInst(psState, psVMADInst, psInputInst);

	SetupTempVecSource(psState, psVMADInst, 0 /* uSrcIdx */, uPackTemp + 1, eDestFmt);
	psVMADInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

	SetupImmediateSource(psVMADInst, 1 /* uSrcIdx */, 65536.f /* fImmValue */, eDestFmt);
	psVMADInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);

	SetupTempVecSource(psState, psVMADInst, 2 /* uSrcIdx */, uPackTemp + 0, eDestFmt);
	psVMADInst->u.psVec->auSwizzle[2] = USEASM_SWIZZLE(X, Y, Z, W);

	AppendInst(psState, psCodeBlock, psVMADInst);

	if (bF16)
	{
		StoreDestinationF16(psState, psCodeBlock, psInputInst, &psInputInst->sDest);
	}
	else
	{
		StoreDestinationF32(psState, psCodeBlock, psInputInst, &psInputInst->sDest, USC_TEMPREG_CVTVECF32DSTOUT);
	}
}

IMG_INTERNAL
IMG_VOID ConvertReformatInstructionF16F32_Vec(PINTERMEDIATE_STATE	psState,
											  PCODEBLOCK			psCodeBlock,
											  PUNIFLEX_INST			psInputInst,
											  IMG_BOOL				bF16)
/*****************************************************************************
 FUNCTION	: ConvertReformatInstructionF16F32_Vec
    
 PURPOSE	: Convert an intermediate format conversion instruction with an F32
			  or F16 format destination to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to be converted.egister.
			  bF16				- TRUE if the destination register is F16 format.
								  FALSE if the destination register is F32 format.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST			psPckInst;
	UF_REGFORMAT	eSrcFormat;
	IOPCODE			eOpcode;
	IMG_BOOL		bScale;
	IMG_UINT32		uSrcUsedChanMask = GetSourceLiveChans(psState, psInputInst, 0 /* uSrc */);

	eSrcFormat = psInputInst->asSrc[0].eFormat;

	if (eSrcFormat == UF_REGFORMAT_I32 || eSrcFormat == UF_REGFORMAT_U32)
	{
		ConvertInteger32ToF32_Vec(psState, psCodeBlock, psInputInst, bF16, eSrcFormat);
		return;
	}
	if (eSrcFormat == UF_REGFORMAT_I16 || eSrcFormat == UF_REGFORMAT_U16)
	{
		ConvertInteger16ToF32_Vec(psState, psCodeBlock, psInputInst, bF16, eSrcFormat);
		return;
	}

	psPckInst = AllocateInst(psState, NULL);
	switch (eSrcFormat)
	{
		case UF_REGFORMAT_U8: eOpcode = IVPCKFLTU8; bScale = IMG_TRUE; break;
		case UF_REGFORMAT_C10: eOpcode = IVPCKFLTC10; bScale = IMG_TRUE; break;
		case UF_REGFORMAT_F16:
		case UF_REGFORMAT_F32: 
		{
			eOpcode = IVMOV; 
			bScale = IMG_FALSE; 
			break;
		}
		default: imgabort();
	}

	SetOpcode(psState, psPckInst, eOpcode);
	if (bScale)
	{
		psPckInst->u.psVec->bPackScale = IMG_TRUE;
	}

	if (bF16)
	{
		GetDestinationF16_Vec(psState, &psInputInst->sDest, &psPckInst->asDest[0]);
	}
	else
	{
		GetDestinationF32_Vec(psState, &psInputInst->sDest, &psPckInst->asDest[0]);
	}

	GetVectorPredicateInst(psState, psPckInst, psInputInst);

	psPckInst->auDestMask[0] = psInputInst->sDest.u.byMask;

	switch (eSrcFormat)
	{
		case UF_REGFORMAT_U8:
		case UF_REGFORMAT_C10:
		{
			IMG_UINT32	uU8C10Swizzle;
			IMG_UINT32	uInputSwizzle;
			IMG_UINT32	uUsedMask;
			IMG_BOOL	bIgnoreSwiz;

			if (CanOverrideSwiz(&psInputInst->asSrc[0]))
			{
				uUsedMask = SwizzleMask(psInputInst->asSrc[0].u.uSwiz, psInputInst->sDest.u.byMask);
				uInputSwizzle = ConvertSwizzle(psState, psInputInst->asSrc[0].u.uSwiz);
				bIgnoreSwiz = IMG_TRUE;
			}
			else
			{
				uUsedMask = psInputInst->sDest.u.byMask;
				uInputSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
				bIgnoreSwiz = IMG_FALSE;
			}

			GetSourceC10(psState, 
						 psCodeBlock, 
						 &psInputInst->asSrc[0], 
						 psInputInst->asSrc[0].byMod, 
						 &psPckInst->asArg[0], 
						 uUsedMask, 
						 bIgnoreSwiz, 
						 IMG_FALSE /* bMultiFormat */, 
						 psInputInst->asSrc[0].eFormat);

			uU8C10Swizzle = ConvertSwizzle(psState, GetInputToU8C10IntermediateSwizzle(psState));
			psPckInst->u.psVec->aeSrcFmt[0] = eSrcFormat;
			psPckInst->u.psVec->auSwizzle[0] = CombineSwizzles(uInputSwizzle, uU8C10Swizzle);
			break;
		}
		case UF_REGFORMAT_F16:
		{
			GetSourceF16_Vec(psState, 
							 psCodeBlock, 
							 &psInputInst->asSrc[0], 
							 uSrcUsedChanMask,
							 psPckInst,
							 0 /* uSrcIdx */);
			psPckInst->u.psVec->auSwizzle[0] = ConvertSwizzle(psState, psInputInst->asSrc[0].u.uSwiz);
			break;
		}
		case UF_REGFORMAT_F32:
		{
			GetSourceF32_Vec(psState, 
							 psCodeBlock, 
							 &psInputInst->asSrc[0], 
							 uSrcUsedChanMask,
							 psPckInst,
							 0 /* uSrcIdx */);
			psPckInst->u.psVec->auSwizzle[0] = ConvertSwizzle(psState, psInputInst->asSrc[0].u.uSwiz);
			break;
		}
		default: imgabort();
	}
	AppendInst(psState, psCodeBlock, psPckInst);

	if (bF16)
	{
		StoreDestinationF16(psState, psCodeBlock, psInputInst, &psInputInst->sDest);
	}
	else
	{
		StoreDestinationF32(psState, psCodeBlock, psInputInst, &psInputInst->sDest, USC_TEMPREG_CVTVECF32DSTOUT);
	}
}

static
IMG_VOID ConvertFloatAddressValueVec(PINTERMEDIATE_STATE	psState,
									 PCODEBLOCK				psCodeBlock,
									 PUNIFLEX_INST			psInputInst,
									 IMG_UINT32				uChan,
									 IMG_BOOL				bF16,
									 IMG_UINT32				uRegIndexDest,
									 IMG_UINT32				uPredSrc,
									 IMG_BOOL				bPredSrcNegate)
/*****************************************************************************
 FUNCTION	: ConvertFloatAddressValueVec
    
 PURPOSE	: Convert a floating point value to the format to use for
			  indexing into memory/registers.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction whose source register is to be
								converted.
			  uChan				- Channel to convert in the source register.
			  bF16				- TRUE if the source register is F16 format.
								  FALSE if the source register is F32 format.
			  uRegIndexDest		- Destination register for the converted value.
			  uPredSrc			- Predicate to guard update of the destination.
			  bPredSrcNegate
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST			psInst;
	IMG_UINT32		uSrcUsedChanMask;
	PUF_REGISTER	psInputSrc;
	IMG_UINT32		uSwizzle;
	IMG_UINT32		uSelChan;

	psInputSrc = &psInputInst->asSrc[0];
	uSrcUsedChanMask = GetSourceLiveChans(psState, psInputInst, 0 /* uSrc */);

	psInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psInst, ICVTFLT2ADDR);
	psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psInst->asDest[0].uNumber = uRegIndexDest;
	SetPredicate(psState, psInst, uPredSrc, bPredSrcNegate);

	/*
		Convert the input swizzle.
	*/
	uSelChan = EXTRACT_CHAN(psInputSrc->u.uSwiz, uChan);
	switch (uSelChan)
	{
	case UFREG_SWIZ_X: uSwizzle = USEASM_SWIZZLE(X, X, X, X); break;
	case UFREG_SWIZ_Y: uSwizzle = USEASM_SWIZZLE(Y, Y, Y, Y); break;
	case UFREG_SWIZ_Z: uSwizzle = USEASM_SWIZZLE(Z, Z, Z, Z); break;
	case UFREG_SWIZ_W: uSwizzle = USEASM_SWIZZLE(W, W, W, W); break;
	case UFREG_SWIZ_0: uSwizzle = USEASM_SWIZZLE(0, 0, 0, 0); break;
	case UFREG_SWIZ_1: uSwizzle = USEASM_SWIZZLE(1, 1, 1, 1); break;
	case UFREG_SWIZ_2: uSwizzle = USEASM_SWIZZLE(2, 2, 2, 2); break;
	case UFREG_SWIZ_HALF: uSwizzle = USEASM_SWIZZLE(HALF, HALF, HALF, HALF); break;
	default: imgabort();
	}
	psInst->u.psVec->auSwizzle[0] = uSwizzle;

	if (bF16)
	{
		GetSourceF16_Vec(psState, 
						 psCodeBlock, 
						 psInputSrc, 
						 uSrcUsedChanMask,
						 psInst,
						 0 /* uSrcIdx */);
	}
	else
	{
		GetSourceF32_Vec(psState, 
						 psCodeBlock, 
						 psInputSrc, 
						 uSrcUsedChanMask,
						 psInst,
						 0 /* uSrcIdx */);
	}
	AppendInst(psState, psCodeBlock, psInst);
}

IMG_INTERNAL 
IMG_VOID ConvertMovaInstructionVecFloat(PINTERMEDIATE_STATE psState, 
										PCODEBLOCK psCodeBlock, 
										PUNIFLEX_INST psInputInst, 
										IMG_BOOL bF16)
/*****************************************************************************
 FUNCTION	: ConvertMovaInstructionVecFloat
    
 PURPOSE	: Convert an input MOVA instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  bFloat32			- Type of the MOVA source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < 4; uChan++)
	{
		if (psInputInst->sDest.u.byMask & (1 << uChan))
		{
			IMG_UINT32	uPredSrc;
			IMG_BOOL	bPredSrcNegate;
			IMG_UINT32	uRegIndexDest;

			/*
				Get any predicate for this channel.
			*/
			GetInputPredicate(psState, &uPredSrc, &bPredSrcNegate, psInputInst->uPredicate, uChan);

			uRegIndexDest = USC_TEMPREG_ADDRESS + uChan;

			/* Get the source for the address offset and convert to integer format. */
			ConvertFloatAddressValueVec(psState, 
										psCodeBlock, 
										psInputInst,
										uChan,
										bF16, 
										uRegIndexDest, 
										uPredSrc, 
										bPredSrcNegate);
		}
	}
}

static
IMG_VOID UnpackFixedPointTextureChannelFloatVec(PINTERMEDIATE_STATE	psState,
												PCODEBLOCK			psCodeBlock,
												PARG				psDest,
												IMG_UINT32			uDestChan,
												IMG_UINT32			uFormat,
												IMG_UINT32			uSrcRegType,
												IMG_UINT32			uSrcRegNum,
												IMG_UINT32			uSrcRegByteOffset)
/*****************************************************************************
 FUNCTION	: UnpackFixedPointTextureChannelFloatVec
    
 PURPOSE	: Generates code to convert a channel in the result of a texture sample
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psDest			- Intermediate destination to set.
			  uDestChan			- Channel in the destination to set.
			  uFormat			- Format of the channel.
			  uSrcRegType		- Register containing the data for the channel.
			  uSrcRegNum
			  uSrcRegByteOffset	- Byte offset of the data for the channel
								in the register.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST			psVPackInst;
	IOPCODE			eOpcode;
	IMG_BOOL		bScale;
	UF_REGFORMAT	eSrcFormat = UF_REGFORMAT_INVALID;

	psVPackInst = AllocateInst(psState, NULL);
	switch (uFormat)
	{
		case USC_CHANNELFORM_U8:	eOpcode = IVPCKFLTU8; bScale = IMG_TRUE; eSrcFormat = UF_REGFORMAT_U8; break;
		case USC_CHANNELFORM_DF32:
		{
			PINST psInst;							
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IAND);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[0].uNumber = 0x7FFFFFFF;
			psInst->asArg[1].uType = uSrcRegType;
			psInst->asArg[1].uNumber = uSrcRegNum;
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = GetNextRegister(psState);
			uSrcRegNum = psInst->asDest[0].uNumber;
			uSrcRegType = USEASM_REGTYPE_TEMP;
			AppendInst(psState, psCodeBlock, psInst);
		}
		case USC_CHANNELFORM_F32:
		case USC_CHANNELFORM_F16:	
		{
			eOpcode = IVMOV; 
			bScale = IMG_FALSE; 
			break;
		}
		case USC_CHANNELFORM_C10:	eOpcode = IVPCKFLTC10; bScale = IMG_TRUE; eSrcFormat = UF_REGFORMAT_C10; break;
		default: imgabort();
	}
	SetOpcode(psState, psVPackInst, eOpcode);
	if (bScale)
	{
		psVPackInst->u.psVec->bPackScale = IMG_TRUE;
	}

	psVPackInst->asDest[0] = *psDest;
	psVPackInst->auDestMask[0] = 1 << uDestChan;

	if (uFormat == USC_CHANNELFORM_F16 || uFormat == USC_CHANNELFORM_F32 || uFormat == USC_CHANNELFORM_DF32)
	{
		IMG_UINT32	uChan;

		psVPackInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;

		psVPackInst->asArg[1].uType = uSrcRegType;
		psVPackInst->asArg[1].uNumber = uSrcRegNum;

		for (uChan = 2; uChan < 5; uChan++)
		{
			psVPackInst->asArg[uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
		}

		if (uFormat == USC_CHANNELFORM_F16)
		{
			psVPackInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F16;
			psVPackInst->asArg[1].eFmt = UF_REGFORMAT_F16;
		}
		else
		{
			psVPackInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
			psVPackInst->asArg[1].eFmt = UF_REGFORMAT_F32;
		}

		if (uSrcRegByteOffset == 0)
		{
			psVPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
		}
		else
		{
			ASSERT(uFormat == USC_CHANNELFORM_F16 && uSrcRegByteOffset == 2);
			psVPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(Y, Y, Y, Y);
		}
	}
	else
	{
		psVPackInst->asArg[0].uType = uSrcRegType;
		psVPackInst->asArg[0].uNumber = uSrcRegNum;
		psVPackInst->asArg[0].eFmt = eSrcFormat;

		psVPackInst->u.psVec->auSwizzle[0] = (uSrcRegByteOffset << (USEASM_SWIZZLE_FIELD_SIZE * uDestChan));
		psVPackInst->u.psVec->aeSrcFmt[0] = eSrcFormat;
	}

	AppendInst(psState, psCodeBlock, psVPackInst);
}

static
IMG_VOID UnpackFixedPointTextureChannelFloatVec_Restricted(PINTERMEDIATE_STATE	psState,
														   PCODEBLOCK			psCodeBlock,
														   PARG					psDest,
														   IMG_UINT32			uDestChan,
														   IMG_UINT32			uFormat,
														   IMG_BOOL				bNormalise,
														   IMG_UINT32			uSrcRegType,
														   IMG_UINT32			uSrcRegNum,
														   IMG_UINT32			uSrcRegByteOffset)
/*****************************************************************************
 FUNCTION	: UnpackFixedPointTextureChannelFloatVec_Restricted
    
 PURPOSE	: Generates code to convert a channel in the result of a texture sample
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psDest			- Intermediate destination to set.
			  uDestChan			- Channel in the destination to set.
			  bNormalise		- Normalise the converted data.
			  uFormat			- Format of the channel.
			  uSrcRegType		- Register containing the data for the channel.
			  uSrcRegNum
			  uSrcRegByteOffset	- Byte offset of the data for the channel
								in the register.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psVPackInst;
	PINST		psVMovInst;
	IOPCODE		eOpcode;
	IMG_BOOL	bScale;
	IMG_UINT32	uSrcRegComponent;
	IMG_UINT32	uUnpackTemp = GetNextRegister(psState);

	/*
		Create an instruction to convert the texture sample result to floating
		point.
	*/
	psVPackInst = AllocateInst(psState, NULL);
	switch (uFormat)
	{
		case USC_CHANNELFORM_U8_UN:	eOpcode = IVPCKFLTU8; bScale = IMG_FALSE; break;
		case USC_CHANNELFORM_S8:	eOpcode = IVPCKFLTS8; bScale = IMG_TRUE; break;
		case USC_CHANNELFORM_U16:	eOpcode = IVPCKFLTU16; bScale = IMG_TRUE; break;
		case USC_CHANNELFORM_S16:	eOpcode = IVPCKFLTS16; bScale = IMG_TRUE; break;
		default: imgabort();
	}
	SetOpcode(psState, psVPackInst, eOpcode);
	if (bScale && bNormalise)
	{
		psVPackInst->u.psVec->bPackScale = IMG_TRUE;
	}

	psVPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psVPackInst->asDest[0].uNumber = uUnpackTemp;
	psVPackInst->asDest[0].eFmt = psDest->eFmt;
	psVPackInst->auDestMask[0] = USC_X_CHAN_MASK;
		
	if (uFormat == USC_CHANNELFORM_U16 || uFormat == USC_CHANNELFORM_S16)
	{
		/*
			Convert the byte offset to 16-bit units.
		*/
		if (uSrcRegByteOffset == 0)
		{
			uSrcRegComponent = 0;
		}
		else
		{
			ASSERT(uSrcRegByteOffset == 2);
			uSrcRegComponent = 1;
		}
	}
	else
	{
		uSrcRegComponent = uSrcRegByteOffset;
	}

	psVPackInst->asArg[0].uType = uSrcRegType;
	psVPackInst->asArg[0].uNumber = uSrcRegNum;
	psVPackInst->asArg[0].eFmt = UF_REGFORMAT_UNTYPED;

	psVPackInst->u.psVec->auSwizzle[0] = (uSrcRegComponent << (USEASM_SWIZZLE_FIELD_SIZE * USC_X_CHAN));

	AppendInst(psState, psCodeBlock, psVPackInst);

	/*
		Create a VMOV from the X channel of the temporary containing the converted texture sample result to the
		selected channel in the destination.
	*/
	psVMovInst = AllocateInst(psState, NULL);

	SetOpcode(psState, psVMovInst, IVMOV);

	psVMovInst->asDest[0] = *psDest;
	psVMovInst->auDestMask[0] = 1 << uDestChan;

	psVMovInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psVMovInst->asArg[0].uNumber = uUnpackTemp;
	psVMovInst->asArg[0].eFmt = psDest->eFmt;

	psVMovInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
	psVMovInst->u.psVec->aeSrcFmt[0] = psDest->eFmt;
	
	AppendInst(psState, psCodeBlock, psVMovInst);
}

static
IMG_VOID UnpackU24TextureChannelFloatVec(PINTERMEDIATE_STATE	psState,
										 PCODEBLOCK				psCodeBlock,
										 PARG					psDest,
										 IMG_UINT32				uDestChan,
										 IMG_UINT32				uSrcRegType,
										 IMG_UINT32				uSrcRegNum,
										 IMG_UINT32				uSrcRegByteOffset)
/*****************************************************************************
 FUNCTION	: UnpackU24TextureChannelFloatVec
    
 PURPOSE	: Generates code to convert a 24-bit channel in the result of a texture sample
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psDest			- Intermediate destination to set.
			  uDestChan			- Channel in the destination to set.
			  bNormalise		- Normalise the converted data.
			  uSrcRegType		- Register containing the data for the channel.
			  uSrcRegNum
			  uSrcRegByteOffset	- Byte offset of the data for the channel
								in the register.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uLastByteTemp = GetNextRegister(psState);
	IMG_UINT32	uFirstWordTemp = GetNextRegister(psState);
	IMG_UINT32	uUnscaledChanTemp = GetNextRegister(psState);
	ARG			sLastByteTemp;
	ARG			sFirstWordTemp;
	PINST		psVMULInst;
	PINST		psVMADInst;
	IMG_FLOAT	fU24Scale;

	/*
		Convert the last byte of the 24-bit value to an unnormalised float.
	*/
	InitInstArg(&sLastByteTemp);
	sLastByteTemp.uType = USEASM_REGTYPE_TEMP;
	sLastByteTemp.uNumber = uLastByteTemp;
	sLastByteTemp.eFmt = psDest->eFmt;

	UnpackFixedPointTextureChannelFloatVec_Restricted(psState,
													  psCodeBlock,
													  &sLastByteTemp,
													  USC_X_CHAN,
													  USC_CHANNELFORM_U8_UN,
													  IMG_FALSE /* bNormalise */,
													  uSrcRegType,
													  uSrcRegNum,
													  uSrcRegByteOffset + WORD_SIZE);

	/*
		Convert the second word of the 24-bit value to an unnormalised float.
	*/
	InitInstArg(&sFirstWordTemp);
	sFirstWordTemp.uType = USEASM_REGTYPE_TEMP;
	sFirstWordTemp.uNumber = uFirstWordTemp;
	sFirstWordTemp.eFmt = psDest->eFmt;

	UnpackFixedPointTextureChannelFloatVec_Restricted(psState,
													  psCodeBlock,
													  &sFirstWordTemp,
													  USC_X_CHAN,	
													  USC_CHANNELFORM_U16,
													  IMG_FALSE /* bNormalise */,
													  uSrcRegType,
													  uSrcRegNum,
													  uSrcRegByteOffset + 0);

	/*
		Combine two parts of a 24-bit value into a single float.

			UNSCALED_CHAN = FIRST_WORD + LAST_BYTE * 65536
	*/
	psVMADInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psVMADInst, IVMAD);

	psVMADInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psVMADInst->asDest[0].uNumber = uUnscaledChanTemp;
	psVMADInst->asDest[0].eFmt = psDest->eFmt;

	SetupTempVecSource(psState, psVMADInst, 0 /* uSrcIdx */, uLastByteTemp, psDest->eFmt);
	psVMADInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

	SetupImmediateSource(psVMADInst, 1 /* uSrcIdx */, 65536.f /* fImmValue */, psDest->eFmt);
	psVMADInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);

	SetupTempVecSource(psState, psVMADInst, 2 /* uSrcIdx */, uFirstWordTemp, psDest->eFmt);
	psVMADInst->u.psVec->auSwizzle[2] = USEASM_SWIZZLE(X, Y, Z, W);

	AppendInst(psState, psCodeBlock, psVMADInst);

	/*
		Normalise the 24-bit channel.

		DEST = UNSCALED_CHAN / ((1 << 23) - 1
	*/
	psVMULInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psVMULInst, IVMUL);

	psVMULInst->asDest[0] = *psDest;
	psVMULInst->auDestMask[0] = 1 << uDestChan;

	SetupTempVecSource(psState, psVMULInst, 0 /* uSrcIdx */, uUnscaledChanTemp, psDest->eFmt);
	psVMULInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);

	fU24Scale = 1.0f / (IMG_FLOAT)((1 << 23) - 1);
	SetupImmediateSource(psVMULInst, 1 /* uSrcIdx */, fU24Scale, psDest->eFmt);
	psVMULInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);

	AppendInst(psState, psCodeBlock, psVMULInst);
}


static IMG_VOID UnpackConstantTextureChannelFloatVec(PINTERMEDIATE_STATE	psState,
													 PCODEBLOCK				psCodeBlock,
													 PARG					psDest,
													 IMG_UINT32				uDestChan,
													 IMG_UINT32				uFormat)
/*****************************************************************************
 FUNCTION	: UnpackConstantTextureChannelFloatVec
    
 PURPOSE	: Generates code to set a channel result of a texture sample
			  instruction to a constant.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psDest			- Intermediate destination to set.
			  uDestChan			- Channel in the destination to set.
			  uFormat			- Constant to set.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psVMOVInst;
	IMG_FLOAT	fImmValue;

	psVMOVInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psVMOVInst, IVMOV);

	psVMOVInst->asDest[0] = *psDest;

	psVMOVInst->auDestMask[0] = 1 << uDestChan;

	fImmValue = (uFormat == USC_CHANNELFORM_ONE) ? 1.0f : 0.0f;
	SetupImmediateSource(psVMOVInst, 0 /* uArgIdx */, fImmValue, UF_REGFORMAT_F32);

	AppendInst(psState, psCodeBlock, psVMOVInst);
}

static
IMG_VOID UnpackIntegerTextureChannelFloatVec(PINTERMEDIATE_STATE	psState,
											 PCODEBLOCK				psCodeBlock,
											 PARG					psDest,
											 IMG_UINT32				uDestChan,
											 IMG_UINT32				uFormat,
											 IMG_UINT32				uSrcRegType,
											 IMG_UINT32				uSrcRegNum,
											 IMG_UINT32				uSrcRegByteOffset) 
/*****************************************************************************
 FUNCTION	: UnpackIntegerTextureChannelFloatVec
    
 PURPOSE	: Generates code to unpack the result of a texture sample of a
			  texture containing integer data.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psDest			- Intermediate destination to set.
			  uDestChan			- Channel in the destination to set.
			  uFormat			- Format of the integer data.
			  uSrcRegType		- Register containing the raw data from the
			  uSrcRegNum		  the texutre.
			  uSrcRegByteOffset
			  
 RETURNS	: None.
*****************************************************************************/
{
	ARG			sSrc;
	PINST		psVMOVInst;
	IMG_UINT32	uChan;

	/*
		Initialize an argument referencing the raw data for this channel from the texture sample.
	*/
	InitInstArg(&sSrc);
	sSrc.uType = uSrcRegType;
	sSrc.uNumber = uSrcRegNum;

	/*
		Convert the raw data to a 32-bit signed or unsigned integer.
	*/
	switch (uFormat)
	{
		case USC_CHANNELFORM_UINT8: 
		{
			ZeroExtendInteger(psState, psCodeBlock, 8 /* uSrcBitWidth */, &sSrc, uSrcRegByteOffset, &sSrc);
			break;
		}
		case USC_CHANNELFORM_SINT8: 
		{
			SignExtendInteger(psState, psCodeBlock, 8 /* uSrcBitWidth */, &sSrc, uSrcRegByteOffset, &sSrc);
			break;
		}
		case USC_CHANNELFORM_UINT16: 
		{
			ZeroExtendInteger(psState, psCodeBlock, 16 /* uSrcBitWidth */, &sSrc, uSrcRegByteOffset, &sSrc);
			break;
		}
		case USC_CHANNELFORM_SINT16: 
		{
			SignExtendInteger(psState, psCodeBlock, 16 /* uSrcBitWidth */, &sSrc, uSrcRegByteOffset, &sSrc);
			break;
		}
		case USC_CHANNELFORM_UINT32:
		case USC_CHANNELFORM_SINT32:
		{
			/* No conversion required. */
			break;
		}
		default: imgabort();
	}

	/*
		Move the converted data into the vector destination register.
	*/
	psVMOVInst = AllocateInst(psState, NULL);

	SetOpcode(psState, psVMOVInst, IVMOV);

	psVMOVInst->asDest[0] = *psDest;
	psVMOVInst->auDestMask[0] = 1 << uDestChan;

	psVMOVInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;

	psVMOVInst->asArg[1] = sSrc;
	for (uChan = 1; uChan < 4; uChan++)
	{
		psVMOVInst->asArg[1 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
	}

	psVMOVInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
	psVMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);

	AppendInst(psState, psCodeBlock, psVMOVInst);
}

IMG_INTERNAL
IMG_VOID UnpackTextureChanFloatVec(PINTERMEDIATE_STATE		psState,
								   PCODEBLOCK				psCodeBlock,
								   PARG						psDest,
								   IMG_UINT32				uDestChan,
								   USC_CHANNELFORM			eSrcFormat,
								   IMG_UINT32				uSrcRegNum,
								   IMG_UINT32				uSrcByteOffset)
/*****************************************************************************
 FUNCTION	: UnpackTextureChanFloatVec
    
 PURPOSE	: Generates code to unpack a single channel of a texture sample result 
			  to floating point  format on a core with vector instructions.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psDest			- Vector destination to write with the unpacked
								data.
			  uDestChan			- Channel to write in the vector destination.
			  eSrcFormat		- Format of the packed data.
			  uSrcRegNum		- Scalar register number containing the packed data.
			  uSrcByteOffset	- Offset of the packed data within the scalar register.
			  
 RETURNS	: None.
*****************************************************************************/
{
	switch (eSrcFormat)
	{
		case USC_CHANNELFORM_ONE:
		case USC_CHANNELFORM_ZERO:
		{
			UnpackConstantTextureChannelFloatVec(psState,
												 psCodeBlock,
												 psDest,
												 uDestChan,
												 eSrcFormat);
			break;
		}
		case USC_CHANNELFORM_U8:
		case USC_CHANNELFORM_F16:
		case USC_CHANNELFORM_C10:
		case USC_CHANNELFORM_F32:
		case USC_CHANNELFORM_DF32:
		{
			UnpackFixedPointTextureChannelFloatVec(psState,
												   psCodeBlock,
												   psDest,
												   uDestChan,
												   eSrcFormat,
												   USEASM_REGTYPE_TEMP,
												   uSrcRegNum,
												   uSrcByteOffset);
			break;
		}
		case USC_CHANNELFORM_U8_UN:
		case USC_CHANNELFORM_S8:
		case USC_CHANNELFORM_U16:
		case USC_CHANNELFORM_S16:
		{
			UnpackFixedPointTextureChannelFloatVec_Restricted(psState,
															  psCodeBlock,
															  psDest,
															  uDestChan,
															  eSrcFormat,
															  IMG_TRUE /* bNormalise */,
															  USEASM_REGTYPE_TEMP,
															  uSrcRegNum,
															  uSrcByteOffset);
			break;
		}
		case USC_CHANNELFORM_UINT32:
		case USC_CHANNELFORM_SINT32:
		case USC_CHANNELFORM_UINT8:
		case USC_CHANNELFORM_UINT16:
		case USC_CHANNELFORM_SINT8:
		case USC_CHANNELFORM_SINT16:
		{
			UnpackIntegerTextureChannelFloatVec(psState,
												psCodeBlock,
												psDest,
												uDestChan,
												eSrcFormat,
												USEASM_REGTYPE_TEMP,
												uSrcRegNum,
												uSrcByteOffset);
			break;
		}
		case USC_CHANNELFORM_U24:
		{
			UnpackU24TextureChannelFloatVec(psState,
											psCodeBlock,
											psDest,
											uDestChan,
											USEASM_REGTYPE_TEMP,
											uSrcRegNum,
											uSrcByteOffset);
			break;
		}
		default: imgabort();
	}
}

#if defined(OUTPUT_USCHW)
typedef struct _TEXTURE_SAMPLE_RESULT_CHAN
{
	USC_CHANNELFORM		eFormat;
	IMG_UINT32			uRegNum;
	IMG_UINT32			uChanOffset;
	IMG_BOOL			bVector;
} TEXTURE_SAMPLE_RESULT_CHAN, *PTEXTURE_SAMPLE_RESULT_CHAN;

static
IMG_UINT32 UnpackTextureVectorResults(PINTERMEDIATE_STATE			psState,
									  PCODEBLOCK					psBlock,
									  PARG							psDest,
									  IMG_UINT32					uMask,
									  TEXTURE_SAMPLE_RESULT_CHAN	asChans[])
/*****************************************************************************
 FUNCTION	: UnpackTextureVectorResults
    
 PURPOSE	: Check for simple cases where the results of a texture sample
			  can be unpacked in a single instruction.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psDest			- Destination for the unpacking data.
			  uMask				- Mask of channels to unpack.
			  asChans			- Details of the packed result of the texture.
			  
 RETURNS	: The mask of non-vector channels to unpack.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if ((uMask & (1 << uChan)) != 0 && asChans[uChan].bVector)
		{
			IMG_UINT32		uMaskToUnpack;
			IMG_UINT32		uSwizzle;
			IMG_UINT32		uNextChan;
			PINST			psVMOVInst;
			UF_REGFORMAT	eSrcFmt;

			uMaskToUnpack = 0;
			uSwizzle = 0;
			for (uNextChan = uChan; uNextChan < CHANNELS_PER_INPUT_REGISTER; uNextChan++)
			{
				if ((uMask & (1 << uNextChan)))
				{
					USC_CHANNELFORM	eChanFormat = asChans[uNextChan].eFormat;

					if (eChanFormat == USC_CHANNELFORM_ZERO ||
						eChanFormat == USC_CHANNELFORM_ONE)
					{
						USEASM_SWIZZLE_SEL	eConstSel;

						if (eChanFormat == USC_CHANNELFORM_ZERO)
						{
							eConstSel = USEASM_SWIZZLE_SEL_0;
						}
						else
						{
							ASSERT(eChanFormat == USC_CHANNELFORM_ONE);
							eConstSel = USEASM_SWIZZLE_SEL_1;
						}

						uSwizzle = SetChan(uSwizzle, uNextChan, eConstSel);

						uMaskToUnpack |= (1 << uNextChan);
					}
					else if (asChans[uNextChan].uRegNum == asChans[uChan].uRegNum)
					{
						ASSERT(asChans[uNextChan].bVector);
						ASSERT(asChans[uNextChan].eFormat == asChans[uChan].eFormat);

						uSwizzle = SetChan(uSwizzle, uNextChan, asChans[uNextChan].uChanOffset);

						uMaskToUnpack |= (1 << uNextChan);
					}
				}
			}

			if (asChans[uChan].eFormat == USC_CHANNELFORM_F32)
			{
				eSrcFmt = UF_REGFORMAT_F32;
			}
			else
			{
				ASSERT(asChans[uChan].eFormat == USC_CHANNELFORM_F16);
				eSrcFmt = UF_REGFORMAT_F16;
			}

			psVMOVInst = AllocateInst(psState, NULL /* psSrcLineInst */);

			SetOpcode(psState, psVMOVInst, IVMOV);

			psVMOVInst->asDest[0] = *psDest;
			psVMOVInst->auDestMask[0] = uMaskToUnpack;

			psVMOVInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psVMOVInst->asArg[0].uNumber = asChans[uChan].uRegNum;
			psVMOVInst->asArg[0].eFmt = eSrcFmt; 

			psVMOVInst->u.psVec->auSwizzle[0] = uSwizzle;
			psVMOVInst->u.psVec->aeSrcFmt[0] = eSrcFmt;

			AppendInst(psState, psBlock, psVMOVInst);

			uMask &= ~uMaskToUnpack;
		}
	}

	return uMask;
}

IMG_INTERNAL
IMG_VOID UnpackTextureFloatVec(PINTERMEDIATE_STATE		psState, 
							   PCODEBLOCK				psCodeBlock, 
							   PSAMPLE_RESULT_LAYOUT	psResultLayout,
							   PSAMPLE_RESULT			psResult,
							   PARG						psDest,
							   IMG_UINT32				uMask)
/*****************************************************************************
 FUNCTION	: UnpackTextureFloatVec
    
 PURPOSE	: Generates code to unpack a texture sample result to floating point 
			  format on a core with vector instructions.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uTexture			- Number of the texture to unpack.
			  uSrcRegType		- Register type of the data to unpack.
			  uSrcRegNum		- Register number of the data to unpack.
			  psInputInst		- Texture load instruction.
			  bF16				- Format to unpack to.
			  uMask				- Mask of channel to unpack.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32					uChan;
	TEXTURE_SAMPLE_RESULT_CHAN	asChans[CHANNELS_PER_INPUT_REGISTER];
	
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (uMask & (1 << uChan))
		{
			/*
				Get the offset and format of the packed data for this channel within the
				result of the texture sample.
			*/
			GetTextureSampleChannelLocation(psState, 
											psResultLayout, 
											psResult, 
											uChan, 
											&asChans[uChan].eFormat, 
											&asChans[uChan].uRegNum, 
											&asChans[uChan].bVector,
											&asChans[uChan].uChanOffset);
		}
	}

	/*
		Unpack parts of the texture sample result which have been mapped to vector registers.
	*/
	uMask = UnpackTextureVectorResults(psState, psCodeBlock, psDest, uMask, asChans);

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if ((uMask & (1 << uChan)) != 0)
		{	
			ASSERT(!asChans[uChan].bVector);

			/*
				Convert the packed data to F16 or F32 and write into the vector destination.
			*/
			UnpackTextureChanFloatVec(psState, 
									  psCodeBlock, 
									  psDest, 
									  uChan, 
									  asChans[uChan].eFormat,
									  asChans[uChan].uRegNum,
									  asChans[uChan].uChanOffset);
		}
	}
}

static
PCODEBLOCK ConvertSamplerInstructionFloatVecUSCHW(PINTERMEDIATE_STATE psState, 
												  PCODEBLOCK psCodeBlock, 
												  PUNIFLEX_INST psInputInst, 
												  IMG_BOOL bConditional, 
												  IMG_BOOL bF16)
/*****************************************************************************
 FUNCTION	: ConvertSamplerInstructionFloatVecUSCHW
    
 PURPOSE	: Convert an input texture sample instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  bConditional		- Whether the instruction is conditionally executed.
			  
 RETURNS	: PCODEBLOCK - Block to put the following instructions in.
*****************************************************************************/
{
	SAMPLE_RESULT_LAYOUT sSampleResultLayout;
	SAMPLE_RESULT sSampleResult;
	ARG sDest;	

	/* Get the raw results of the texture load in a register. */
 	psCodeBlock = ConvertSamplerInstructionCore(psState, 
												psCodeBlock, 
												psInputInst, 
												&sSampleResultLayout,
												&sSampleResult, 
												bConditional);	

	if (bF16)
	{
		GetDestinationF16_Vec(psState, &psInputInst->sDest, &sDest);
	}
	else
	{
		GetDestinationF32_Vec(psState, &psInputInst->sDest, &sDest);
	}
	
	/* Unpack the result to floating point. */
	UnpackTextureFloatVec(psState, 
						  psCodeBlock, 
						  &sSampleResultLayout,
						  &sSampleResult,
						  &sDest, 
						  psInputInst->sDest.u.byMask);

	/* Apply any destination modifiers. */
	GenerateDestModF32F16_Vec(psState, psCodeBlock, psInputInst, bF16);

	if (bF16)
	{
		StoreDestinationF16(psState, psCodeBlock, psInputInst, &psInputInst->sDest);
	}
	else
	{
		StoreDestinationF32(psState, psCodeBlock, psInputInst, &psInputInst->sDest, USC_TEMPREG_CVTVECF32DSTOUT);
	}

	/* Free memory allocated by ConvertSamplerInstructionCore. */
	FreeSampleResultLayout(psState, &sSampleResultLayout);

	return psCodeBlock;
}
#endif /* defined(OUTPUT_USCHW)*/

#if defined(OUTPUT_USPBIN)
static
PCODEBLOCK ConvertSamplerInstructionFloatVecUSPBIN(PINTERMEDIATE_STATE psState, 
												   PCODEBLOCK psCodeBlock, 
												   PUNIFLEX_INST psInputInst, 
												   IMG_BOOL bConditional, 
												   IMG_BOOL bF16)
/*****************************************************************************
 FUNCTION	: ConvertSamplerInstructionFloatVecUSPBIN
    
 PURPOSE	: Convert an input texture sample instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  bConditional		- Whether the instruction is conditionally executed.
			  
 RETURNS	: PCODEBLOCK - Block to put the following instructions in.
*****************************************************************************/
{
	IMG_UINT32		uChanMask;
	IMG_UINT32		uChanSwizzle;
	ARG				asDest[max(SCALAR_REGISTERS_PER_F16_VECTOR, SCALAR_REGISTERS_PER_F32_VECTOR)];
	IMG_UINT32		uSampleDest;
	PINST			psMovInst;
	IMG_UINT32		uChan;
	IMG_UINT32		uSampleResultSize;
	UF_REGFORMAT	eSampleResultFormat;

	uChanMask		= psInputInst->sDest.u.byMask;
	uChanSwizzle	= psInputInst->asSrc[1].u.uSwiz;

	/*
		What's the format of the returned data?
	*/
	if (bF16)
	{
		uSampleResultSize = SCALAR_REGISTERS_PER_F16_VECTOR;
		eSampleResultFormat = UF_REGFORMAT_F16;
	}
	else
	{
		uSampleResultSize = SCALAR_REGISTERS_PER_F32_VECTOR;
		eSampleResultFormat = UF_REGFORMAT_F32;
	}

	/* Get some temporary registers for the result of the sample. */
	uSampleDest		= GetNextRegisterCount(psState, uSampleResultSize);

	/* Setup the location where we want the data to be sampled */
	MakeArgumentSet(asDest, uSampleResultSize, USEASM_REGTYPE_TEMP, uSampleDest, eSampleResultFormat);

	/* Get the raw results of the texture load in a register. */
	psCodeBlock = ConvertSamplerInstructionCoreUSP(psState, 
												psCodeBlock, 
								 				psInputInst, 
												uChanMask,
												uChanSwizzle,
												asDest,
												bConditional);
	
	/* Move the texture load into the instruction destination. */
	psMovInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psMovInst, IVMOV);
	if (bF16)
	{
		GetDestinationF16_Vec(psState, &psInputInst->sDest, &psMovInst->asDest[0]);
	}
	else
	{
		GetDestinationF32_Vec(psState, &psInputInst->sDest, &psMovInst->asDest[0]);
	}
	GetVectorPredicateInst(psState, psMovInst, psInputInst);
	psMovInst->auDestMask[0] = psInputInst->sDest.u.byMask;

	psMovInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;
	for (uChan = 0; uChan < uSampleResultSize; uChan++)
	{
		psMovInst->asArg[1 + uChan].uType = USEASM_REGTYPE_TEMP;
		psMovInst->asArg[1 + uChan].uNumber = uSampleDest + uChan;
		psMovInst->asArg[1 + uChan].eFmt = eSampleResultFormat;
	}
	psMovInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	psMovInst->u.psVec->aeSrcFmt[0] = eSampleResultFormat;
	AppendInst(psState, psCodeBlock, psMovInst);

	GenerateDestModF32F16_Vec(psState, psCodeBlock, psInputInst, bF16);

	if (bF16)
	{
		StoreDestinationF16(psState, psCodeBlock, psInputInst, &psInputInst->sDest);
	}
	else
	{
		StoreDestinationF32(psState, psCodeBlock, psInputInst, &psInputInst->sDest, USC_TEMPREG_CVTVECF32DSTOUT);
	}
	return psCodeBlock;
}
#endif	/* #if defined(OUTPUT_USPBIN) */

IMG_INTERNAL
PCODEBLOCK ConvertSamplerInstructionFloatVec(	PINTERMEDIATE_STATE psState, 
												PCODEBLOCK psCodeBlock, 
												PUNIFLEX_INST psInputInst,
												IMG_BOOL bConditional,
												IMG_BOOL bF16)
/*****************************************************************************
 FUNCTION	: ConvertSamplerInstructionFloatVec
    
 PURPOSE	: Convert an input texture sample instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  bConditional		- Whether the instruction is conditionally executed.
			  
 RETURNS	: PCODEBLOCK - Block to put the following instructions in.
*****************************************************************************/
{
	return CHOOSE_USPBIN_OR_USCHW_FUNC(ConvertSamplerInstructionFloatVec,
									   (psState, psCodeBlock, psInputInst, bConditional, bF16));
}

IMG_INTERNAL 
PCODEBLOCK ConvertInstToIntermediateF16_Vec(	PINTERMEDIATE_STATE psState, 
												PCODEBLOCK psCodeBlock, 
												PUNIFLEX_INST psInputInst, 
												IMG_BOOL bConditional, 
												IMG_BOOL bStaticCond)
/*****************************************************************************
 FUNCTION	: ConvertInstToIntermediateF16_Vec
    
 PURPOSE	: Convert an input instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  bConditional		- Whether instruction appears in a conditional
			  bStaticCond		- Whether conditional instruction appears in is static.
			  
 RETURNS	: PCODEBLOCK - Block to put the following instructions in.
*****************************************************************************/
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
			psCodeBlock = ConvertSamplerInstructionFloatVec(psState, psCodeBlock, psInputInst, bConditional, IMG_TRUE /* bF16 */);
			break;
		}

		/* Scalar instructions. */
		case UFOP_RECIP:
		case UFOP_RSQRT:
		case UFOP_EXP:
		case UFOP_LOG:
		{
			ConvertComplexOpInstructionF32F16_Vec(psState, psCodeBlock, psInputInst, IMG_TRUE /* bF16 */);
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
		case UFOP_DOT4:
		case UFOP_DOT3:
		case UFOP_CMP:
		case UFOP_CMPLT:
		case UFOP_SLT:
		case UFOP_SGE:
		case UFOP_SETP:
		{
			ConvertVectorInstructionF32F16_Vec(psState, psCodeBlock, psInputInst, IMG_TRUE /* bF16 */);
			break;
		}

		case UFOP_DOT2ADD:
		{
			ConvertDOT2ADDInstructionF32F16_Vec(psState, psCodeBlock, psInputInst, IMG_TRUE /* bF16 */);
			break;
		}

		case UFOP_MOV:
		{
			if (GetRegisterFormat(psState, &psInputInst->asSrc[0]) == UF_REGFORMAT_F16)
			{
				ConvertVectorInstructionF32F16_Vec(psState, psCodeBlock, psInputInst, IMG_TRUE /* bF16 */);
			}
			else
			{
				ConvertReformatInstructionF16F32_Vec(psState, psCodeBlock, psInputInst, IMG_TRUE /* bF16 */);
			}
			break;
		}

		/*
			Not valid at F16 precision.
		*/
		case UFOP_DOT4_CP:
		{
			imgabort();
		}

		/* LIT/EXPP/LOGP/DST are invalid at medium precision */
		case UFOP_LIT:
		case UFOP_OGLLIT:
		case UFOP_EXPP:
		case UFOP_LOGP:
		case UFOP_DST:
		{
			imgabort();
			break;
		}

		/* Move to address register instruction. */
		case UFOP_MOVA_RNE:
		{
			ConvertMovaInstructionVecFloat(psState, psCodeBlock, psInputInst, IMG_TRUE /* bF16 */);
			break;
		}

		/* Kill instruction. */
		case UFOP_KPLT:
		case UFOP_KPLE:
		{
			ConvertTexkillInstructionFloatVec(psState, psCodeBlock, psInputInst,  bConditional);
			break;
		}

		/* Macro instruction. */
		case UFOP_SINCOS: 
		{
			imgabort();
			break;
		}

		/* Macro instruction. */
		case UFOP_SINCOS2: 
		{
			imgabort();
			break;
		}

		case UFOP_NOP: break;

		default: imgabort();
	}
	return psCodeBlock;
}
#endif  /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

/* EOF */

