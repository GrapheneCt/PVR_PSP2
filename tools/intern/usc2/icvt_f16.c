/*************************************************************************
 * Name         : icvt_f16.c
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
 * $Log: icvt_f16.c $
 **************************************************************************/

#include "uscshrd.h"
#include "bitops.h"

static const IMG_UINT32 g_puChanToMask[] = {0x3, 0xC, 0x3, 0xC};

static IMG_VOID GetF16Source(PINTERMEDIATE_STATE psState, 
							 PCODEBLOCK psCodeBlock, 
							 PUF_REGISTER psSource, 
							 PARG psHwSource, 
							 IMG_PUINT32 puComponent,
							 IMG_UINT32 uChan,
							 IMG_BOOL bAllowSourceMod,
							 PFLOAT_SOURCE_MODIFIER psSourceMod,
							 IMG_BOOL bNoF16HWConsts);

static IMG_VOID GetIteratedValueF16(PINTERMEDIATE_STATE psState, 
									PCODEBLOCK psCodeBlock, 
									UNIFLEX_ITERATION_TYPE eIterationType,
									IMG_UINT32 uCoordinate,
									UF_REGFORMAT eFormat,
									PARG psHwSource, 
									IMG_PUINT32 puComponent,
									IMG_UINT32 uChan,
									IMG_BOOL bProjected)
/*****************************************************************************
 FUNCTION	: GetIteratedValueF16
    
 PURPOSE	: Load an iterated value in F16 format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block where any extras instructions needed should be added.
			  eIterationType	- Type of the iteration to add.
			  uCoordinate		- Index of the texture coordinate or colour to iteration.
			  eFormat			- Format for the iterated value.
			  uSrc				- Source number.
			  psHwSource		- Output where the hardware format source is
								stored.
			  uMask				- Mask of the channels required in the source.
			  bProjected		- TRUE if the iterated value should be
								projected.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uIterTempNum;
	IMG_UINT32	uComponent;

	ASSERT(eFormat == UF_REGFORMAT_F16);

	uIterTempNum = 
		GetIteratedValue(psState, eIterationType, uCoordinate, UNIFLEX_TEXLOAD_FORMAT_F16, 2 /* uNumAttributes */);

	psHwSource->uType = USEASM_REGTYPE_TEMP;
	psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
	psHwSource->uIndexNumber = USC_UNDEF;
	psHwSource->uIndexArrayOffset = USC_UNDEF;
	psHwSource->eFmt = UF_REGFORMAT_F16;
	psHwSource->uNumber = uIterTempNum;

	if (bProjected)
	{
		PINST psProjInst, psPackInst;
		IMG_UINT32 uReg;
		IMG_UINT32 uOneOverT = GetNextRegister(psState);
		IMG_UINT32 uProjectedCoord = GetNextRegisterCount(psState, 2);

		/*
			Get 1/T
		*/
		psProjInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psProjInst, IFRCP);
		psProjInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psProjInst->asDest[0].uNumber = uOneOverT;
		psProjInst->asDest[0].eFmt = UF_REGFORMAT_F16;
		SetBit(psProjInst->auFlag, INST_TYPE_PRESERVE, 1);
		psProjInst->asArg[0] = *psHwSource;
		psProjInst->asArg[0].uNumber++;
		SetComponentSelect(psState, psProjInst, 0 /* uArgIdx */, 2 /* uComponent */);
		AppendInst(psState, psCodeBlock, psProjInst);

		/*
			Replicate the result to both channels.
		*/
		psPackInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psPackInst, IPCKF16F16);
		psPackInst->asDest[0] = psProjInst->asDest[0];
		psPackInst->asArg[0] = psProjInst->asDest[0];
		SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, 2 /* uComponent */);
		psPackInst->asArg[1] = psProjInst->asDest[0];
		SetPCKComponent(psState, psPackInst, 1 /* uArgIdx */, 2 /* uComponent */);
		AppendInst(psState, psCodeBlock, psPackInst);

		/*
			Divide the other coordinate by T.
		*/
		for (uReg = 0; uReg < 2; uReg++)
		{
			psProjInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psProjInst, IFMUL16);
			psProjInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psProjInst->asDest[0].uNumber = uProjectedCoord + uReg;
			psProjInst->asDest[0].eFmt = UF_REGFORMAT_F16;
			psProjInst->asArg[0] = *psHwSource;
			psProjInst->asArg[0].uNumber += uReg;
			psProjInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
			psProjInst->asArg[1].uNumber = uOneOverT;
			psProjInst->asArg[1].eFmt = UF_REGFORMAT_F16;
			AppendInst(psState, psCodeBlock, psProjInst);
		}

		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber = uProjectedCoord;
	}

	psHwSource->uNumber += (uChan >> 1);
	uComponent = (uChan & 1) << 1;
	if (puComponent != NULL)
	{
		*puComponent = uComponent;
	}
	else
	{
		ASSERT(uComponent == 0);
	}
}

static IMG_VOID ConvertDestinationF16(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUF_REGISTER psDest)
/*****************************************************************************
 FUNCTION	: ConvertDestinationF16
    
 PURPOSE	: Do conversion for a destination register which is always in F32 format.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Code block to insert any extra instructions needed.
			  psDest				- Destination register in the input format.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	ASSERT(psDest->eType == UFREG_TYPE_VSOUTPUT);

	for (uChan = 0; uChan < 4; uChan++)
	{
		PINST	psPackInst;

		if ((psDest->u.byMask & (1U << uChan)) == 0)
		{
			continue;
		}

		psPackInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psPackInst, IUNPCKF32F16);
		GetDestinationF32(psState, psCodeBlock, psDest, uChan, &psPackInst->asDest[0]);
		psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psPackInst->asArg[0].uNumber = USC_TEMPREG_CVTF16DSTOUT + (uChan >> 1);
		SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, (uChan & 1) << 1);
		psPackInst->asArg[0].eFmt = UF_REGFORMAT_F16;
		AppendInst(psState, psCodeBlock, psPackInst);
	}
}

static IMG_UINT32 ConvertTempRegisterNumberF16(PINTERMEDIATE_STATE	psState,
											   PUF_REGISTER			psRegister,
											   IMG_UINT32			uChan)
/*****************************************************************************
 FUNCTION	: ConvertTempRegisterNumberF16
    
 PURPOSE	: Convert an input temporary register to a temporary register
			  number in the intermediate format..

 PARAMETERS	: psRegister					- The input register.
			  uChan							- The channel within the input register.
			  
 RETURNS	: The converted number.
*****************************************************************************/
{
	IMG_UINT32	uRegNum;
	
	ASSERT(psRegister->uNum < psState->uInputTempRegisterCount);
	uRegNum = USC_TEMPREG_MAXIMUM + psRegister->uNum * CHANNELS_PER_INPUT_REGISTER + (uChan >> 1);
	ASSERT(uRegNum < psState->uNumRegisters);

	return uRegNum;
}

static IMG_VOID GetDestinationF16(PINTERMEDIATE_STATE psState, PUF_REGISTER psDest, PARG psHwSource, IMG_UINT32 uChan)
/*****************************************************************************
 FUNCTION	: GetDestinationF16
    
 PURPOSE	: Convert a destination register in the input format into the intermediate format.

 PARAMETERS	: psState				- Compiler state.
			  psDest				- Destination register in the input format.
			  psHwSource			- Pointer to the structure to fill out with the register in
									  the intermediate format.
			  
 RETURNS	: None.
*****************************************************************************/
{
	InitInstArg(psHwSource);

	if (psDest->eType == UFREG_TYPE_PSOUTPUT)
	{
		ConvertPixelShaderResultArg(psState, psDest, psHwSource);
		psHwSource->uNumber += (uChan >> 1);
		psHwSource->eFmt = UF_REGFORMAT_F16;
	}
	else if (psDest->eType == UFREG_TYPE_VSOUTPUT)
	{
		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber = USC_TEMPREG_CVTF16DSTOUT + (uChan >> 1);
		psHwSource->eFmt = UF_REGFORMAT_F16;
	}
	else if (psDest->eType == UFREG_TYPE_INDEXABLETEMP)
	{
		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber = USC_TEMPREG_F16INDEXTEMPDEST + (uChan >> 1);
		psHwSource->eFmt = UF_REGFORMAT_F16;
	}
	else
	{
		ASSERT(psDest->eType == UFREG_TYPE_TEMP);

		if (psDest->eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
		{
			imgabort();
		}
		psHwSource->uType = USEASM_REGTYPE_TEMP;
		psHwSource->uNumber =  ConvertTempRegisterNumberF16(psState, psDest, uChan);
		psHwSource->eFmt = UF_REGFORMAT_F16;
	}
}

static
IMG_VOID CopySourceModifiers(PUF_REGISTER			psInputSource,
							 PFLOAT_SOURCE_MODIFIER	psSourceMod)
/*****************************************************************************
 FUNCTION	: CopySourceModifiers
    
 PURPOSE	: Copy source modifiers from an input source argument to an 
			  intermediate source modifier.

 PARAMETERS	: psInputSource			- Input source register.
			  psSourceMod			- Intermediate source modifier
			  
 RETURNS	: None.
*****************************************************************************/
{
	psSourceMod->bAbsolute = psSourceMod->bNegate = IMG_FALSE;
	if (psInputSource->byMod & UFREG_SOURCE_ABS)
	{
		psSourceMod->bAbsolute = IMG_TRUE;
	}
	if (psInputSource->byMod & UFREG_SOURCE_NEGATE)
	{
		psSourceMod->bNegate = IMG_TRUE;
	}
}

static IMG_BOOL GetF16StaticDoubleConst(PINTERMEDIATE_STATE psState,
										PUF_REGISTER psSource,
										PARG psHwSource,
										IMG_UINT32 uSrcChan1,
										IMG_UINT32 uSrcChan2)
/*****************************************************************************
 FUNCTION	: GetF16StaticDoubleConst
    
 PURPOSE	: Check if a instruction using two f16 channels can use a hardware
			  constant.

 PARAMETERS	: psState				- Compiler state.
			  psSource				- Source register in the input format.
			  psHwSource			- Pointer to where to store the result.
			  uSrcChan1				- Channels used from the source.
			  uSrcChan2
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uOffset1 = psSource->uNum * CHANNELS_PER_INPUT_REGISTER + uSrcChan1;
	IMG_UINT32	uOffset2 = psSource->uNum * CHANNELS_PER_INPUT_REGISTER + uSrcChan2;
		
	if (psSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE)
	{
		if (uOffset1 < psState->psConstants->uCount && GetBit(psState->psConstants->puConstStaticFlags, uOffset1) &&
			uOffset2 < psState->psConstants->uCount && GetBit(psState->psConstants->puConstStaticFlags, uOffset2) &&
			psState->psConstants->pfConst[uOffset1] == psState->psConstants->pfConst[uOffset2])
		{
			IMG_FLOAT	fValue;

			fValue = psState->psConstants->pfConst[uOffset1];

			if (fValue == 0.0f)
			{
				psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
				psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
				psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
				psHwSource->uIndexArrayOffset = USC_UNDEF;
				psHwSource->uIndexNumber = USC_UNDEF;
				psHwSource->eFmt = UF_REGFORMAT_F16;
				return IMG_TRUE;
			}
			else if (fValue == 1.0f)
			{
				psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
				psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE;
				psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
				psHwSource->uIndexNumber = USC_UNDEF;
				psHwSource->uIndexArrayOffset = USC_UNDEF;
				psHwSource->eFmt = UF_REGFORMAT_F16;
				return IMG_TRUE;
			}
		}
	}
	return IMG_FALSE;
}

static 
IMG_VOID GetDoubleSourceF16(PINTERMEDIATE_STATE psState, 
							PCODEBLOCK psCodeBlock, 
							PUF_REGISTER psSource, 
							IMG_UINT32 uSrc, 
							PARG psHwSource, 
							PFMAD16_SWIZZLE peHwSwizzle,
							IMG_UINT32 uHalf,
							IMG_BOOL bAllowSourceMod,
							PFLOAT_SOURCE_MODIFIER psSourceMod,
							IMG_UINT32 uHalfMask)
/*****************************************************************************
 FUNCTION	: GetDoubleSourceF16
    
 PURPOSE	: Get a 32-bit register with two channels of an f16 register in the
			  upper and lower words.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Code block to insert any extra instructions needed.
			  psSource				- Source register in the input format.
			  psHwSource			- Pointer to where to store the result.
			  uHalf					- Which half of the source register is wanted.
			  bAllowSourceMod		- Allow source modifiers on the source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 uSrcChan1 = EXTRACT_CHAN(psSource->u.uSwiz, (uHalf * 2) + 0);
	IMG_UINT32 uSrcChan2 = EXTRACT_CHAN(psSource->u.uSwiz, (uHalf * 2) + 1);
	IMG_UINT32 uChan;
	IMG_BOOL bSimpleSwizzle;
	IMG_UINT32 uSrcChan = uSrcChan1 & ~1U;
	IMG_BOOL bSrcChan1ConstSwiz;
	IMG_BOOL bSrcChan2ConstSwiz;

	InitInstArg(psHwSource);
	*peHwSwizzle = FMAD16_SWIZZLE_LOWHIGH;

	/*
		Check for swizzles selecting constants 0, 1, 0.5 and 2. 
	*/
	bSrcChan1ConstSwiz = ((uSrcChan1 >= UFREG_SWIZ_1) && (uSrcChan1 <= UFREG_SWIZ_2)) ? IMG_TRUE : IMG_FALSE;
	bSrcChan2ConstSwiz = ((uSrcChan2 >= UFREG_SWIZ_1) && (uSrcChan2 <= UFREG_SWIZ_2)) ? IMG_TRUE : IMG_FALSE;

	if	(bSrcChan1ConstSwiz || bSrcChan2ConstSwiz)
	{
		IMG_BOOL bHandled = IMG_FALSE;

		/*
			If the two channels are referencing the same constant, then use the
			appropriate HW-constant to provide both.
		*/
		if	(bSrcChan1ConstSwiz && bSrcChan2ConstSwiz && (uSrcChan1 == uSrcChan2))
		{
			switch (uSrcChan1)
			{
				case UFREG_SWIZ_1:
				{
					psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
					psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE;
					psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
					psHwSource->uIndexNumber = USC_UNDEF;
					psHwSource->uIndexArrayOffset = USC_UNDEF;
					psHwSource->eFmt = UF_REGFORMAT_F16;

					bHandled = IMG_TRUE;
					break;
				}

				case UFREG_SWIZ_0:
				{
					psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
					psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
					psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
					psHwSource->uIndexNumber = USC_UNDEF;
					psHwSource->uIndexArrayOffset = USC_UNDEF;
					psHwSource->eFmt = UF_REGFORMAT_F16;

					bHandled = IMG_TRUE;
					break;
				}

				default:
				{
					break;
				}
			}
		}

		/*
			Handle any other cases as two pack instructions

			NB: GetF16Source and GetSourceF32 can both handle 0, 1, 0.5 and 2 swizzle
				selections
		*/
		if	(!bHandled)
		{
			PINST		psPackInst;
			IMG_BOOL	bF16Source;

			psPackInst = AllocateInst(psState, IMG_NULL);
			if (psSource->eType == UFREG_TYPE_TEMP || 
				psSource->eType == UFREG_TYPE_CONST || 
				psSource->eType == UFREG_TYPE_COL ||
				psSource->eType == UFREG_TYPE_TEXCOORD || 
				psSource->eType == UFREG_TYPE_TEXCOORDP || 
				psSource->eType == UFREG_TYPE_TEXCOORDPIFTC ||
				psSource->eType == UFREG_TYPE_PSOUTPUT)
			{
				SetOpcode(psState, psPackInst, IPCKF16F16);
				bF16Source = IMG_TRUE;
			}
			else
			{
				SetOpcode(psState, psPackInst, IPCKF16F32);
				bF16Source = IMG_FALSE;
			}
			psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asDest[0].uNumber = USC_TEMPREG_F16TEMP + uSrc;
			psPackInst->asDest[0].eFmt = UF_REGFORMAT_F16;
			psPackInst->auDestMask[0] = USC_DESTMASK_FULL;
			for (uChan = 0; uChan < 2; uChan++)
			{
				if (bF16Source)
				{
					IMG_UINT32	uComponent;

					GetF16Source(psState, 
								 psCodeBlock, 
								 psSource, 
								 &psPackInst->asArg[uChan], 
								 &uComponent,
								 uHalf * 2 + uChan, 
								 IMG_FALSE, 
								 NULL,
								 IMG_FALSE);
					SetPCKComponent(psState, psPackInst, uChan, uComponent);
				}
				else
				{
					GetSourceF32(psState, 
								 psCodeBlock, 
								 psSource, 
								 uHalf * 2 + uChan, 
								 &psPackInst->asArg[uChan], 
								 IMG_FALSE,
								 NULL);
				}
			}
			AppendInst(psState, psCodeBlock, psPackInst);
		
			*psHwSource = psPackInst->asDest[0];
		}
	}
	else
	{
		FMAD16_SWIZZLE	eHwSwizzle = FMAD16_SWIZZLE_LOWHIGH;

		/*	
			For a component granular relative access the swizzle is treated as part of the offset
			into memory.
		*/
		if (
				psSource->eType == UFREG_TYPE_CONST &&
				psSource->eRelativeIndex != UFREG_RELATIVEINDEX_NONE &&
				psSource->u1.uRelativeStrideInComponents < 4
		   ) 
		{
			bSimpleSwizzle = IMG_FALSE;
		}
		/*
			We can only check for a simple swizzle if neither component in the pair
			is using a swizzle that references a constant
		*/
		else if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES)
		{
			/*
				Check for needing to swizzle together two different registers or a
				HIGHLOW swizzle.
			*/
			if (
					(uHalfMask == 3) && 
					(
						((uSrcChan1 >> 1) != (uSrcChan2 >> 1)) ||
						(((uSrcChan1 % 2) == 1) && ((uSrcChan2 % 2) == 0))
					)
			   )
			{
				bSimpleSwizzle = IMG_FALSE;
			}
			else
			{
				bSimpleSwizzle = IMG_TRUE;

				/*
					Which register are we accessing?
				*/
				if (uHalfMask == 2)
				{
					uSrcChan = uSrcChan2 & ~1U;
				}
				else
				{
					uSrcChan = uSrcChan1 & ~1U;
				}

				if (uHalfMask != 3)
				{
					/*
						Check for accessing the high half of a register in
						the low calculation or vice-versa.
					*/
					if (uHalfMask == 1 && (uSrcChan1 % 2) == 1)
					{
						eHwSwizzle = FMAD16_SWIZZLE_HIGHHIGH;
					}
					else if (uHalfMask == 2 && (uSrcChan2 % 2) == 0)
					{
						eHwSwizzle = FMAD16_SWIZZLE_LOWLOW;
					}
					else
					{
						eHwSwizzle = FMAD16_SWIZZLE_LOWHIGH;
					}
				}
				else if ((uSrcChan1 % 2) == 0 && (uSrcChan2 % 2) == 0)
				{
					eHwSwizzle = FMAD16_SWIZZLE_LOWLOW;
				}
				else if ((uSrcChan1 % 2) == 1 && (uSrcChan2 % 2) == 1)
				{
					eHwSwizzle = FMAD16_SWIZZLE_HIGHHIGH;
				}
				else
				{
					eHwSwizzle = FMAD16_SWIZZLE_LOWHIGH;
				}
			}
		}
		else
		{
			bSimpleSwizzle = IMG_FALSE;
			if ((uHalfMask == 3 && ((uSrcChan1 % 2) == 0 && (uSrcChan1 + 1) == uSrcChan2)) ||
				(uHalfMask == 1 && (uSrcChan1 % 2) == 0) ||
				(uHalfMask == 2 && (uSrcChan2 % 2) == 1))
			{
				if (uHalfMask == 2)
				{
					uSrcChan = uSrcChan2 & ~1U;
				}
				else
				{
					uSrcChan = uSrcChan1 & ~1U;
				}
				bSimpleSwizzle = IMG_TRUE;
			}
		}

		/*
			Check if the source register is already in the desired format.
		*/
		if	(psSource->eType == UFREG_TYPE_CONST && GetF16StaticDoubleConst(psState, psSource, psHwSource, uSrcChan1, uSrcChan2))
		{
			/* GetF16StaticDoubleConst has already set up the hardware source. */
		}
		else if (psSource->eType == UFREG_TYPE_TEMP && bSimpleSwizzle)
		{
			psHwSource->uType = USEASM_REGTYPE_TEMP;
			psHwSource->uNumber = ConvertTempRegisterNumberF16(psState, psSource, uSrcChan);
			psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
			psHwSource->uIndexNumber = USC_UNDEF;
			psHwSource->uIndexArrayOffset = USC_UNDEF;
			psHwSource->eFmt = UF_REGFORMAT_F16;
			*peHwSwizzle = eHwSwizzle;
		}
		else if (psSource->eType == UFREG_TYPE_CONST && bSimpleSwizzle)
		{
			LoadConstant(psState,	
						 psCodeBlock, 
						 psSource, 
						 uSrcChan, 
						 psHwSource, 
						 NULL,
						 IMG_FALSE, 
						 IMG_NULL, 
						 UF_REGFORMAT_F16, 
						 IMG_FALSE /* bC10Subcomponent */,
						 0);
			*peHwSwizzle = eHwSwizzle;
		}
		else if (psSource->eType == UFREG_TYPE_COL && bSimpleSwizzle)
		{
			GetIteratedValueF16(psState, 
								psCodeBlock, 
								UNIFLEX_ITERATION_TYPE_COLOUR,
								psSource->uNum,
								psSource->eFormat,
								psHwSource,
								NULL,
								uSrcChan,
								IMG_FALSE);
			*peHwSwizzle = eHwSwizzle;
		}
		else if ((psSource->eType == UFREG_TYPE_TEXCOORD || 
				  psSource->eType == UFREG_TYPE_TEXCOORDP || 
				  psSource->eType == UFREG_TYPE_TEXCOORDPIFTC) &&
				 bSimpleSwizzle)
		{
			IMG_BOOL	bProjected = (psSource->eType == UFREG_TYPE_TEXCOORDP) ? IMG_TRUE : IMG_FALSE;
			if (psSource->eType == UFREG_TYPE_TEXCOORDPIFTC)
			{
				ASSERT(psSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE);
				if (GetBit(psState->psSAOffsets->auProjectedCoordinateMask, psSource->uNum))
				{
					bProjected = IMG_TRUE;
				}
			}
			GetIteratedValueF16(psState,
								psCodeBlock,
								UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE,
								psSource->uNum,
								psSource->eFormat,
								psHwSource,
								NULL,
								uSrcChan,
								bProjected);
			*peHwSwizzle = eHwSwizzle;
		}
		else
		{
			PINST		psPackInst;
			IMG_BOOL	bF16Source;
			UF_REGISTER	sF32Source;

			/*
				Otherwise swizzle the register using pack instructions.
			*/
			psPackInst = AllocateInst(psState, IMG_NULL);
			if (psSource->eType == UFREG_TYPE_TEMP || 
				psSource->eType == UFREG_TYPE_CONST || 
				psSource->eType == UFREG_TYPE_COL ||
				psSource->eType == UFREG_TYPE_TEXCOORD || 
				psSource->eType == UFREG_TYPE_TEXCOORDP || 
				psSource->eType == UFREG_TYPE_TEXCOORDPIFTC ||
				psSource->eType == UFREG_TYPE_INDEXABLETEMP)
			{
				SetOpcode(psState, psPackInst, IPCKF16F16);
				bF16Source = IMG_TRUE;
			}
			else
			{
				SetOpcode(psState, psPackInst, IPCKF16F32);
				bF16Source = IMG_FALSE;

				sF32Source = *psSource;
				sF32Source.eFormat = UF_REGFORMAT_F32;
			}
			psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asDest[0].uNumber = USC_TEMPREG_F16TEMP + uSrc;
			psPackInst->asDest[0].eFmt = UF_REGFORMAT_F16;
			psPackInst->auDestMask[0] = USC_DESTMASK_FULL;
			for (uChan = 0; uChan < 2; uChan++)
			{
				if (bF16Source)
				{
					IMG_UINT32	uComponent;

					GetF16Source(psState, 
								 psCodeBlock, 
								 psSource, 
								 &psPackInst->asArg[uChan], 
								 &uComponent,
								 uHalf * 2 + uChan, 
								 IMG_FALSE, 
								 NULL,
								 IMG_FALSE);
					SetPCKComponent(psState, psPackInst, uChan, uComponent);
				}
				else
				{
					GetSourceF32(psState, 
								 psCodeBlock, 
								 &sF32Source, 
								 uHalf * 2 + uChan, 
								 &psPackInst->asArg[uChan], 
								 IMG_FALSE,
								 NULL);
				}
			}
			AppendInst(psState, psCodeBlock, psPackInst);
		
			*psHwSource = psPackInst->asDest[0];
			*peHwSwizzle = FMAD16_SWIZZLE_LOWHIGH;
		}
	}

	/*
		Set up modifiers we can do directly in the instruction.
	*/
	if (bAllowSourceMod)
	{
		psSourceMod->uComponent = 0;
		CopySourceModifiers(psSource, psSourceMod);
	}
}

static IMG_BOOL GetF16StaticConstant(PINTERMEDIATE_STATE psState, 
									 PUF_REGISTER psSource,
									 IMG_UINT32 uSrcChan,
									 IMG_BOOL bNoF16HWConsts,
									 PARG psHwSource,
									 IMG_PUINT32 puComponent)
/*****************************************************************************
 FUNCTION	: GetF16StaticConstant
    
 PURPOSE	: Check for a constant value which is available directly as an f16 value.

 PARAMETERS	: psState			- Compiler state.
			  psSource			- Source register to check.
			  uSrcChan			- Source channel to check.
			  bSecond			- Is this the primary or secondary constants.
			  bNoF16HWConsts	- If set then F32 HW constants must be selected
			  psHwSource		- If the constant value exists then on return
								  psHwSource is set up to use it.
			  puComponent		- If the constant value exists then returns
								  the component select to use with it.
			  
 RETURNS	: TRUE if the constant value is available directly.
*****************************************************************************/
{
	if (psSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE)
	{
		IMG_UINT32	uOffset = psSource->uNum * CHANNELS_PER_INPUT_REGISTER + uSrcChan;
				
		if (uOffset < psState->psConstants->uCount && GetBit(psState->psConstants->puConstStaticFlags, uOffset))
		{
			IMG_FLOAT	fValue;

			fValue = psState->psConstants->pfConst[uOffset];

			if (fValue == 0.0f)
			{
				psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
				psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
				psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
				psHwSource->uIndexNumber = USC_UNDEF;
				psHwSource->uIndexArrayOffset = USC_UNDEF;
				psHwSource->eFmt = UF_REGFORMAT_F16;
				*puComponent = 0;
				return IMG_TRUE;
			}
			else if (fValue == 1.0f)
			{
				if	(bNoF16HWConsts)
				{
					psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
					psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
					psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
					psHwSource->uIndexNumber = USC_UNDEF;
					psHwSource->uIndexArrayOffset = USC_UNDEF;
					psHwSource->eFmt = UF_REGFORMAT_F32;
					*puComponent = 0;
				}
				else
				{
					psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
					psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE;
					psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
					psHwSource->uIndexNumber = USC_UNDEF;
					psHwSource->uIndexArrayOffset = USC_UNDEF;
					psHwSource->eFmt = UF_REGFORMAT_F16;
					*puComponent = 0;
				}
				return IMG_TRUE;
			}
		}	
	}
	return IMG_FALSE;
}

static IMG_VOID GetF16Source(PINTERMEDIATE_STATE psState, 
							 PCODEBLOCK psCodeBlock, 
							 PUF_REGISTER psSource, 
							 PARG psHwSource, 
							 IMG_PUINT32 puComponent,
							 IMG_UINT32 uChan,
							 IMG_BOOL bAllowSourceMod,
							 PFLOAT_SOURCE_MODIFIER psSourceMod,
							 IMG_BOOL bNoF16HWConsts)
/*****************************************************************************
 FUNCTION	: GetF16Source
    
 PURPOSE	: Get a register containing a single channel of an F16 source.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block to insert any extra instructions needed.
			  psSource			- Source register in the input format.
			  psHwSource		- Pointer to where to store the result.
			  puComponent		- Returns the component select.
			  uChan				- Which channel of the source register is wanted.
			  bAllowSourceMod	- Allow source modifiers on the source.
			  psSourceMod		- Returns the source modifiers.
			  bNoF16HWConsts	- If set then F32 HW constants must be selected
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 uSMod = (psSource->byMod & UFREG_SMOD_MASK) >> UFREG_SMOD_SHIFT;
	IMG_UINT32 uSrcChan = EXTRACT_CHAN(psSource->u.uSwiz, uChan);
	IMG_BOOL bSModAlreadyApplied = IMG_FALSE;

	InitInstArg(psHwSource);

	if	((uSrcChan >= UFREG_SWIZ_1) && (uSrcChan <= UFREG_SWIZ_2))
	{
		switch (uSrcChan)
		{
			case UFREG_SWIZ_1:
			{
				if	(bNoF16HWConsts)
				{
					psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
					psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
					psHwSource->eFmt = UF_REGFORMAT_F32;
					*puComponent = 0;
				}
				else
				{
					psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
					psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE;
					psHwSource->eFmt = UF_REGFORMAT_F16;
					*puComponent = 0;
				}
				break;
			}

			case UFREG_SWIZ_0:
			{
				psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
				psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
				psHwSource->eFmt = UF_REGFORMAT_F16;
				*puComponent = 0;
				break;
			}

			case UFREG_SWIZ_HALF:
			case UFREG_SWIZ_2:
			{
				PINST	psPackInst;

				/*
					Pack from the F32 format constant to F16.
				*/
				psPackInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psPackInst, IPCKF16F32);
				psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psPackInst->asDest[0].uNumber = GetNextRegister(psState);
				psPackInst->asDest[0].eFmt = UF_REGFORMAT_F16;
				psPackInst->auDestMask[0] = USC_DESTMASK_FULL;
				psPackInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
				if	(uSrcChan == UFREG_SWIZ_HALF)
				{
					psPackInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1OVER2;
				}
				else
				{
					psPackInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2;
				}
				psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psPackInst->asArg[1].uNumber = 0;
				AppendInst(psState, psCodeBlock, psPackInst);
		
				/*
					Make the intermediate source the result of the PCK instruction.
				*/
				*psHwSource = psPackInst->asDest[0];
				*puComponent = 0;

				bSModAlreadyApplied = IMG_TRUE;
				break;
			}

			default:
			{
				break;
			}
		}
	}
	else
	{
		switch (psSource->eType)
		{
			case UFREG_TYPE_TEMP:
			{
				ASSERT(psSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE);
				psHwSource->uType = USEASM_REGTYPE_TEMP;
				psHwSource->uNumber = ConvertTempRegisterNumberF16(psState, psSource, uSrcChan);
				psHwSource->eFmt = UF_REGFORMAT_F16;
				*puComponent = (uSrcChan & 1) << 1; 
				break;
			}
			case UFREG_TYPE_PSOUTPUT:
			{
				ConvertPixelShaderResultArg(psState, psSource, psHwSource);
				psHwSource->eFmt = UF_REGFORMAT_F16;
				psHwSource->uNumber += (uSrcChan >> 1);
				*puComponent = (uSrcChan & 1) << 1; 
				break;
			}
			case UFREG_TYPE_CONST:
			{
				if (!GetF16StaticConstant(psState, 
										  psSource, 
										  uSrcChan, 
										  bNoF16HWConsts, 
										  psHwSource,
										  puComponent))
				{
					LoadConstant(psState, 
								 psCodeBlock, 
								 psSource, 
								 uSrcChan, 
								 psHwSource, 
								 puComponent,
								 IMG_FALSE,
								 NULL, 
								 UF_REGFORMAT_F16, 
								 IMG_FALSE /* bC10Subcomponent */,
								 0 /* uCompOffset */);
				}
				break;
			}
			case UFREG_TYPE_COL:
			{
				GetIteratedValueF16(psState,
									psCodeBlock,
									UNIFLEX_ITERATION_TYPE_COLOUR,
									psSource->uNum,
									psSource->eFormat,
									psHwSource,
									puComponent,
									uSrcChan,
									IMG_FALSE);
				break;
			}
			case UFREG_TYPE_TEXCOORD:
			case UFREG_TYPE_TEXCOORDP:
			case UFREG_TYPE_TEXCOORDPIFTC:
			{
				IMG_BOOL	bProjected = (psSource->eType == UFREG_TYPE_TEXCOORDP) ? IMG_TRUE : IMG_FALSE;
				if (psSource->eType == UFREG_TYPE_TEXCOORDPIFTC)
				{
					ASSERT(psSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE);
					if (GetBit(psState->psSAOffsets->auProjectedCoordinateMask, psSource->uNum))
					{
						bProjected = IMG_TRUE;
					}
				}
				GetIteratedValueF16(psState,
									psCodeBlock,
									UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE,
									psSource->uNum,
									psSource->eFormat,
									psHwSource,
									puComponent,
									uSrcChan,
									bProjected);
				break;
			}
			case UFREG_TYPE_INDEXABLETEMP:
			{
				IMG_UINT32	uTemp = GetNextRegister(psState);

				LoadStoreIndexableTemp(psState, 
									   psCodeBlock, 
									   IMG_TRUE, 
									   UF_REGFORMAT_F16, 
									   psSource, 
									   (1U << uSrcChan), 
									   uTemp, 
									   0);

				psHwSource->uType = USEASM_REGTYPE_TEMP;
				psHwSource->uNumber = uTemp;
				if (!(psState->uCompilerFlags & UF_OPENCL))
					psHwSource->eFmt = UF_REGFORMAT_F16;
				*puComponent = (uSrcChan & 1) << 1;
				break;
			}
			default:
			{
				PINST		psPackInst;
				UF_REGISTER	sF32Source;

				sF32Source = *psSource;
				sF32Source.eFormat = UF_REGFORMAT_F32;

				psPackInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psPackInst, IPCKF16F32);
				psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psPackInst->asDest[0].uNumber = GetNextRegister(psState);
				psPackInst->asDest[0].eFmt = UF_REGFORMAT_F16;
				psPackInst->auDestMask[0] = USC_DESTMASK_FULL;
				GetSourceF32(psState, psCodeBlock, &sF32Source, uChan, &psPackInst->asArg[0], IMG_FALSE, NULL);
				psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psPackInst->asArg[1].uNumber = 0;
				AppendInst(psState, psCodeBlock, psPackInst);
		
				*psHwSource = psPackInst->asDest[0];
				*puComponent = 0;

				bSModAlreadyApplied = IMG_TRUE;
				break;
			}
		}
	}

	if (!bSModAlreadyApplied && uSMod != UFREG_SMOD_NONE)
	{
		PINST	psPackInst;

		/*
			Apply the D3D source modifier using the F32 function. 
		*/
		ApplySourceModF32(psState, psCodeBlock, psHwSource, puComponent, uSMod);

		/*
			Convert the result back to F16 format.
		*/
		psPackInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psPackInst, IPCKF16F32);
		psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psPackInst->asDest[0].uNumber = GetNextRegister(psState);
		psPackInst->asDest[0].eFmt = UF_REGFORMAT_F16;
		psPackInst->auDestMask[0] = USC_DESTMASK_FULL;
		psPackInst->asArg[0] = *psHwSource;
		psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psPackInst->asArg[1].uNumber = 0;
		AppendInst(psState, psCodeBlock, psPackInst);

		*psHwSource = psPackInst->asDest[0];
		*puComponent = 0;
	}

	if (bAllowSourceMod)
	{
		psSourceMod->uComponent = 0;
		CopySourceModifiers(psSource, psSourceMod);
	}
}

IMG_INTERNAL 
IMG_VOID GetSingleSourceF16(PINTERMEDIATE_STATE psState, 
							PCODEBLOCK psCodeBlock, 
							PUF_REGISTER psSource, 
							PARG psHwSource, 
							IMG_PUINT32 puComponent,
							IMG_UINT32 uChan,
							IMG_BOOL bAllowSourceMod,
							PFLOAT_SOURCE_MODIFIER psMod,
							IMG_BOOL bNoF16HWConsts)
/*****************************************************************************
 FUNCTION	: GetSingleSourceF16
    
 PURPOSE	: Get a register containing a single channel of an F16 source.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Code block to insert any extra instructions needed.
			  psSource				- Source register in the input format.
			  psHwSource			- Pointer to where to store the result.
			  uChan					- Which channel of the source register is wanted.
			  bAllowSourceMod		- Allow source modifiers on the source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	GetF16Source(psState, 
				 psCodeBlock, 
				 psSource, 
				 psHwSource, 
				 puComponent,
				 uChan, 
				 bAllowSourceMod, 
				 psMod,
				 bNoF16HWConsts);
}

IMG_INTERNAL
IMG_VOID GetSingleSourceConvertToF32(PINTERMEDIATE_STATE psState, 
									 PCODEBLOCK psCodeBlock, 
									 PUF_REGISTER psSource, 
									 PARG psHwSource, 
									 IMG_UINT32 uChan,
									 IMG_BOOL bAllowSourceMod,
									 PFLOAT_SOURCE_MODIFIER psSourceMod)
/*****************************************************************************
 FUNCTION	: GetSingleSourceConvertToF32
    
 PURPOSE	: Get a register containing a single channel of an F16 source converted
			  to F32 format.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Code block to insert any extra instructions needed.
			  psSource				- Source register in the input format.
			  psHwSource			- Pointer to where to store the result.
			  uChan					- Which channel of the source register is wanted.
			  bAllowSourceMod		- Allow source modifiers on the source.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psPackInst;
	IMG_UINT32	uComponent;

	/*
		If the source is constant 1 or constant 0 then use the corresponding f32 constant
		directly.
	*/
	if (psSource->eType == UFREG_TYPE_CONST &&
		psSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE)
	{
		IMG_UINT32	uSrcChan = EXTRACT_CHAN(psSource->u.uSwiz, uChan);
		IMG_UINT32	uOffset = psSource->uNum * CHANNELS_PER_INPUT_REGISTER + uSrcChan;
				
		if (uOffset < psState->psConstants->uCount && GetBit(psState->psConstants->puConstStaticFlags, uOffset))
		{
			IMG_FLOAT	fValue;

			fValue = psState->psConstants->pfConst[uOffset];

			if (fValue == 0.0f)
			{
				InitInstArg(psHwSource);
				psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
				psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
				psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
				psHwSource->uIndexNumber = USC_UNDEF;
				psHwSource->uIndexArrayOffset = USC_UNDEF;
				psHwSource->eFmt = UF_REGFORMAT_F32;

				if (bAllowSourceMod)
				{
					psSourceMod->uComponent = 0;
					psSourceMod->bAbsolute = IMG_FALSE;
					psSourceMod->bNegate = IMG_FALSE;
				}
				return;
			}
			else if (fValue == 1.0f)
			{
				InitInstArg(psHwSource);
				psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
				psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
				psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
				psHwSource->uIndexNumber = USC_UNDEF;
				psHwSource->uIndexArrayOffset = USC_UNDEF;
				psHwSource->eFmt = UF_REGFORMAT_F32;

				if (bAllowSourceMod)
				{
					psSourceMod->uComponent = 0;
					CopySourceModifiers(psSource, psSourceMod);
				}
				return;
			}
			else if (fValue == -1.0f && bAllowSourceMod)
			{
				InitInstArg(psHwSource);
				psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
				psHwSource->uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;
				psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
				psHwSource->uIndexNumber = USC_UNDEF;
				psHwSource->uIndexArrayOffset = USC_UNDEF;
				psHwSource->eFmt = UF_REGFORMAT_F32;

				if (bAllowSourceMod)
				{
					psSourceMod->uComponent = 0;
					CopySourceModifiers(psSource, psSourceMod);
					if (!psSourceMod->bAbsolute)
					{
						psSourceMod->bNegate = !psSourceMod->bNegate;
					}
				}
				return;
			}
		}
	}


	/*
		Get the source register.
	*/
	GetSingleSourceF16(psState, 
					   psCodeBlock, 
					   psSource, 
					   psHwSource, 
					   &uComponent, 
					   uChan, 
					   bAllowSourceMod, 
					   psSourceMod, 
					   IMG_FALSE);

	/*
		Convert to F32 using a pack instruction.
	*/
	psPackInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psPackInst, IUNPCKF32F16);
	psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psPackInst->asDest[0].uNumber = GetNextRegister(psState);
	psPackInst->asDest[0].eFmt = UF_REGFORMAT_F32;
	psPackInst->asArg[0] = *psHwSource;
	SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uComponent);
	AppendInst(psState, psCodeBlock, psPackInst);

	/*
		Set up the intermediate source to point to the
		destination of the PACK instruction.
	*/
	InitInstArg(psHwSource);
	psHwSource->uType = psPackInst->asDest[0].uType;
	psHwSource->uNumber = psPackInst->asDest[0].uNumber;
	psHwSource->eFmt = UF_REGFORMAT_F32;
}

static 
IMG_VOID GenerateDestModF16(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PARG psDest, IMG_UINT32 uSat, IMG_UINT32 uScale, IMG_UINT32 uChan, IMG_UINT32 uChanCount)
/*****************************************************************************
 FUNCTION	: GenerateDestModF16
    
 PURPOSE	: Create instructions to a destination modifier.

 PARAMETERS	: psState				- Compiler state.
			  psCodeBlock			- Block to insert the instructions in.
			  psDest				- The destination register.
			  dwSat					- Saturation type to apply.
			  dwScale				- Scale to apply.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST		psInst;
	ARG			sTemp;
	IMG_UINT32	uOffset;
	IMG_UINT32	uTemps = GetNextRegisterCount(psState, 2);

	for (uOffset = 0; uOffset < uChanCount; uOffset++)
	{
		IMG_UINT32	uSrcComponent;

		sTemp = *psDest;
		uSrcComponent = ((uChan + uOffset) % 2) * 2;

		/* Apply a scale (just multiply the destination register by the appropriate constant. */
		if (uScale != UFREG_DMOD_SCALEMUL1)
		{
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IFMUL);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = uTemps + uOffset;
			psInst->asArg[0] = sTemp;
			SetComponentSelect(psState, psInst, 0 /* uArgIdx */, uSrcComponent);
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
	
			sTemp = psInst->asDest[0];
			uSrcComponent = 0;
		}

		/* Apply a saturation by doing a min and max. */
		if (uSat != UFREG_DMOD_SATNONE)
		{
			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FCLAMP)
			{
				/* 
				   On sgx545, use clamping:
				   ifclampmin dest, sTemp, x0, x1 
				   or 
				   ifmax dest, sTemp, 0.0  (for UFREG_DMOD_SATZEROMAX)
				*/
				psInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psInst, INVALID_MAXARG);

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
						/* ifmax dest, sTemp, 0.0 */
						SetOpcode(psState, psInst, IFMAX);
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

				psInst->asArg[0] = sTemp;
				SetComponentSelect(psState, psInst, 0 /* uArgIdx */, uSrcComponent);

				psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psInst->asDest[0].uNumber = uTemps + uOffset;

				AppendInst(psState, psCodeBlock, psInst);
				sTemp = psInst->asDest[0]; 
				uSrcComponent = 0;
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				/* 
				   Non-sgx545 code: 
				   max dest, sTemp, x0
				   min dest, sTemp, x1
				*/
				IMG_UINT32 i;
				for (i = 0; i < 2; i++)
				{
					psInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psInst, (i == 0) ? IFMAX : IFMIN);
					psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
					psInst->asDest[0].uNumber = uTemps + uOffset;
					psInst->asArg[0] = sTemp;
					SetComponentSelect(psState, psInst, 0 /* uArgIdx */, uSrcComponent);
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
					sTemp = psInst->asDest[0];	
					uSrcComponent = 0;
					if (uSat == UFREG_DMOD_SATZEROMAX)
					{
						break;
					}
				}
			}
		}
	}

	for (uOffset = 0; uOffset < uChanCount; uOffset++)
	{
		/* Convert back to F16 format. */
		if (uSat != UFREG_DMOD_SATNONE || uScale != UFREG_DMOD_SCALEMUL1)
		{
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IPCKF16F32);
			psInst->asDest[0] = *psDest;
			psInst->auDestMask[0] = g_puChanToMask[uChan + uOffset];
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = uTemps + uOffset;
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 0;
			AppendInst(psState, psCodeBlock, psInst);
		}
	}
}

static IMG_VOID ConvertScalarInstructionF16(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertScalarInstructionF16
    
 PURPOSE	: Convert an input scalar instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uChan;
	IOPCODE		eHwOpcode;
	IMG_UINT32	auDestChan[CHANNELS_PER_INPUT_REGISTER];
	IMG_UINT32	auDestComponent[CHANNELS_PER_INPUT_REGISTER];
	IMG_UINT32	auTempDest[CHANNELS_PER_INPUT_REGISTER];
	
	/* Nothing to do if the destination isn't updated. */
	if (psSrc->sDest.u.byMask == 0)
	{
		return;
	}
	
	/* Select the hardware opcode. */
	switch (psSrc->eOpCode)
	{
		case UFOP_RECIP: eHwOpcode = IFRCP; break;
		case UFOP_RSQRT: eHwOpcode = IFRSQ; break;
		case UFOP_EXP: eHwOpcode = IFEXP; break;
		case UFOP_LOG: eHwOpcode = IFLOG; break;
		#if defined(SUPPORT_SGX545)
		case UFOP_SQRT: eHwOpcode = IFSQRT; break;
		#endif /* defined(SUPPORT_SGX545) */
		default: return;
	}
	
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (psSrc->sDest.u.byMask & (1U << uChan))
		{	
			PINST		psInst;
			IMG_UINT32	uComponent;

			/*
				Does this channel have the same value as a previous channel?
			*/
			if ((auDestChan[uChan] = CheckDuplicateChannel(psSrc, uChan)) < uChan)
			{
				continue;
			}

			psInst = AllocateInst(psState, IMG_NULL);

			SetOpcode(psState, psInst, eHwOpcode);

			/* Write to a temporary destination. */
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = auTempDest[uChan] = GetNextRegister(psState);
	
			/* Get the source register. */
			GetSingleSourceF16(psState, 
							   psCodeBlock, 
							   &psSrc->asSrc[0], 
							   &psInst->asArg[0],
							   &uComponent,
							   uChan, 
							   IMG_TRUE, 
							   &psInst->u.psFloat->asSrcMod[0],
							   IMG_FALSE);
			psInst->u.psFloat->asSrcMod[0].uComponent = uComponent;

			/* No format conversion. */
			psInst->asDest[0].eFmt = psInst->asArg[0].eFmt;
			SetBit(psInst->auFlag, INST_TYPE_PRESERVE, 1);

			/* Insert the new instruction in the current block. */
			AppendInst(psState, psCodeBlock, psInst);

			/*
				The component the destination is produced in is the same as the source component.
			*/
			auDestComponent[uChan] = uComponent;
		}
	}

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (psSrc->sDest.u.byMask & (1U << uChan))
		{
			PINST		psPackInst;
			IMG_UINT32	uDestChan = auDestChan[uChan];

			/* Move the temporary result into the destination. */
			psPackInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psPackInst, IPCKF16F16);
			psPackInst->auDestMask[0] = g_puChanToMask[uChan];
			GetDestinationF16(psState, &psSrc->sDest, &psPackInst->asDest[0], uChan);
			GetInputPredicateInst(psState, psPackInst, psSrc->uPredicate, uChan);
			psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asArg[0].uNumber = auTempDest[uDestChan];
			psPackInst->asArg[0].eFmt = UF_REGFORMAT_F16;
			SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, auDestComponent[uDestChan]);
			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			AppendInst(psState, psCodeBlock, psPackInst);

			/* Do any destination modifiers. */
			GenerateDestModF16(psState, psCodeBlock, &psPackInst->asDest[0], uSat, uScale, uChan, 1);
		}
	}

	/* Store into an indexable temporary register. */
	if (psSrc->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psCodeBlock, &psSrc->sDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);
	}
	else if (psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT)
	{
		ConvertDestinationF16(psState, psCodeBlock, &psSrc->sDest);
	}
}

static IMG_VOID ConvertMadInstructionF16(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertMadInstructionF16
    
 PURPOSE	: Convert an input MAD type (ADD/SUB/MUL/MAD) instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uHalf;
	IMG_UINT32	auTempDest[2] = {USC_UNDEF, USC_UNDEF};
	IMG_UINT32	uPredType = psSrc->uPredicate & UF_PRED_COMP_MASK;
	IMG_UINT32	uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;

	for (uHalf = 0; uHalf < 2; uHalf++)
	{
		PINST		psMadInst;
		IMG_UINT32	uHalfMask = (psSrc->sDest.u.byMask >> (uHalf * 2)) & 3;

		if (uHalfMask == 0)
		{
			continue;
		}

		psMadInst = AllocateInst(psState, IMG_NULL);

		/*
			Select the right opcode.
		*/
		switch (psSrc->eOpCode)
		{
			case UFOP_ADD: SetOpcode(psState, psMadInst, IFADD16); break;
			case UFOP_MUL: SetOpcode(psState, psMadInst, IFMUL16); break;
			case UFOP_MAD: SetOpcode(psState, psMadInst, IFMAD16); break;
			case UFOP_MOV: SetOpcode(psState, psMadInst, IFMOV16); break;
			default: imgabort();
		}

		/*
			Check if we need to write a temporary destination. We do if each channel has
			an individual predicate or we need to preserve one of the channels or we
			would overwrite a source to the next half of the mad.
		*/
		if (
				(uHalfMask != 3) || 
				(uPredType == UF_PRED_XYZW) || 
				(
					(uHalf == 0) && 
					(
						DestChanOverlapsSrc(psState, psSrc, 0) || 
						DestChanOverlapsSrc(psState, psSrc, 1)
					)
				)
			)
		{
			psMadInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psMadInst->asDest[0].uNumber = auTempDest[uHalf] = GetNextRegister(psState);
			psMadInst->asDest[0].eFmt = UF_REGFORMAT_F16;
		}
		else
		{
			GetDestinationF16(psState, &psSrc->sDest, &psMadInst->asDest[0], uHalf * 2);

			if (uPredType != UF_PRED_NONE)
			{
				GetInputPredicateInst(psState, psMadInst, psSrc->uPredicate, uHalf * 2);
			}
		}

		/*
			Get the instruction sources.
		*/
		GetDoubleSourceF16(psState, 
						   psCodeBlock, 
						   &psSrc->asSrc[0], 
						   0, 
						   &psMadInst->asArg[0], 
						   &psMadInst->u.psArith16->aeSwizzle[0],
						   uHalf, 
						   IMG_TRUE, 
						   &psMadInst->u.psArith16->sFloat.asSrcMod[0],
						   uHalfMask);
		if (psSrc->eOpCode != UFOP_MOV)
		{
			GetDoubleSourceF16(psState, 
							   psCodeBlock, 
							   &psSrc->asSrc[1], 
							   1, 
							   &psMadInst->asArg[1], 
							   &psMadInst->u.psArith16->aeSwizzle[1],
							   uHalf, 
							   IMG_TRUE, 
							   &psMadInst->u.psArith16->sFloat.asSrcMod[1],
							   uHalfMask);
		}
		if (psSrc->eOpCode == UFOP_MAD)
		{
			GetDoubleSourceF16(psState, 
							   psCodeBlock, 
							   &psSrc->asSrc[2], 
							   2, 
							   &psMadInst->asArg[2], 
							   &psMadInst->u.psArith16->aeSwizzle[2],
							   uHalf, 
							   IMG_TRUE, 
							   &psMadInst->u.psArith16->sFloat.asSrcMod[2],
							   uHalfMask);
		}

		AppendInst(psState, psCodeBlock, psMadInst);

		/* Do any destination modifiers. */
		GenerateDestModF16(psState, psCodeBlock, &psMadInst->asDest[0], uSat, uScale, uHalf * 2, 2);
	}

	/*
		If we wrote to a temporary destination then do the writes to the
		real destination.
	*/
	for (uHalf = 0; uHalf < 2; uHalf++)
	{
		IMG_UINT32	uHalfMask = (psSrc->sDest.u.byMask >> (uHalf * 2)) & 3;

		if (uHalfMask == 0)
		{
			continue;
		}

		if (auTempDest[uHalf] != USC_UNDEF)
		{
			/*
				Check if we need to update each channel individually.
			*/
			if (uPredType == UF_PRED_XYZW || uHalfMask != 3)
			{
				IMG_UINT32	uChan;

				for (uChan = 0; uChan < 2; uChan++)
				{
					PINST	psPackInst;
					IMG_UINT32 uDestChan = (uHalf * 2) + uChan;

					if ((uHalfMask & (1U << uChan)) == 0)
					{
						continue;
					}

					psPackInst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psPackInst, IPCKF16F16);
					psPackInst->auDestMask[0] = g_puChanToMask[uDestChan];
					GetDestinationF16(psState, &psSrc->sDest, &psPackInst->asDest[0], uDestChan);
					if (uPredType != UF_PRED_NONE)
					{
						GetInputPredicateInst(psState, psPackInst, psSrc->uPredicate, uDestChan);
					}
					psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
					psPackInst->asArg[0].uNumber = auTempDest[uHalf];
					SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uChan << 1);
					psPackInst->asArg[0].eFmt = UF_REGFORMAT_F16;
					psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psPackInst->asArg[1].uNumber = 0;
					AppendInst(psState, psCodeBlock, psPackInst);
				}
			}
			else
			{
				PINST		psMoveInst;
				IMG_UINT32	uDestChan = uHalf * 2;

				psMoveInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMoveInst, IMOV);
				GetDestinationF16(psState, &psSrc->sDest, &psMoveInst->asDest[0], uDestChan);
				if (uPredType != UF_PRED_NONE)
				{
					GetInputPredicateInst(psState, psMoveInst, psSrc->uPredicate, uDestChan);
				}
				psMoveInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
				psMoveInst->asArg[0].uNumber = auTempDest[uHalf];
				psMoveInst->asArg[0].eFmt = UF_REGFORMAT_F16;
				AppendInst(psState, psCodeBlock, psMoveInst);
			}
		}
	}

	/* Store into an indexable temporary register. */
	if (psSrc->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psCodeBlock, &psSrc->sDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);
	}
	else if (psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT)
	{
		ConvertDestinationF16(psState, psCodeBlock, &psSrc->sDest);
	}
}

static IMG_VOID ConvertF32OnlyInstructionF16(PINTERMEDIATE_STATE psState, 
											 PCODEBLOCK psCodeBlock, 
											 PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertF32OnlyInstructionF16
    
 PURPOSE	: Convert an input instruction which can only write in F32 format to 
			  the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uPredType = psSrc->uPredicate & UF_PRED_COMP_MASK;
	IMG_UINT32	uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	auTempDest[CHANNELS_PER_INPUT_REGISTER];

	/*
		Do the actual operation for each channel.
	*/
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		PINST		psInst;
		IMG_UINT32	uArg;
		IMG_UINT32	uArg0Component;

		if ((psSrc->sDest.u.byMask & (1U << uChan)) == 0)
		{
			continue;
		}

		psInst = AllocateInst(psState, IMG_NULL);
		switch (psSrc->eOpCode)
		{
			case UFOP_MIN: SetOpcode(psState, psInst, IFMIN); break;
			case UFOP_MAX: SetOpcode(psState, psInst, IFMAX); break;
			case UFOP_DSX: SetOpcode(psState, psInst, IFDSX); break;
			case UFOP_DSY: SetOpcode(psState, psInst, IFDSY); break;
			case UFOP_FRC: SetOpcode(psState, psInst, IFSUBFLR); break;
			default: imgabort();
		}
		psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asDest[0].uNumber = auTempDest[uChan] = GetNextRegister(psState);

		GetSingleSourceF16(psState, 
						   psCodeBlock, 
						   &psSrc->asSrc[0], 
						   &psInst->asArg[0], 
						   &uArg0Component,
						   uChan, 
						   IMG_TRUE, 
						   &psInst->u.psFloat->asSrcMod[0],
						   IMG_TRUE);
		psInst->u.psFloat->asSrcMod[0].uComponent = uArg0Component;

		if (psSrc->eOpCode == UFOP_MIN || psSrc->eOpCode == UFOP_MAX)
		{
			IMG_UINT32	uArg1Component;

			GetSingleSourceF16(psState, 
							   psCodeBlock, 
							   &psSrc->asSrc[1], 	
							   &psInst->asArg[1], 
							   &uArg1Component,
							   uChan, 
							   IMG_TRUE, 
							   &psInst->u.psFloat->asSrcMod[1],
							   IMG_TRUE);
			psInst->u.psFloat->asSrcMod[1].uComponent = uArg1Component;
		}
		if (psSrc->eOpCode == UFOP_DSX || psSrc->eOpCode == UFOP_DSY || psSrc->eOpCode == UFOP_FRC)
		{
			psInst->asArg[1] = psInst->asArg[0];
			psInst->u.psFloat->asSrcMod[1] = psInst->u.psFloat->asSrcMod[0];
		}

		/*
			Check for sources which don't fit in the restrictions on banks to these instructions.
		*/
		for (uArg = 0; uArg < psInst->uArgumentCount; uArg++)
		{
			PFLOAT_SOURCE_MODIFIER	psMod = &psInst->u.psFloat->asSrcMod[uArg];

			/*
				Check for using a negate modifier when that isn't supported in the instruction.
			*/
			if (!CanHaveSourceModifier(psState, psInst, uArg, psMod->bNegate, psMod->bAbsolute))
			{
				PINST	psMoveInst;

				/*
					Insert an FMOV to apply source modifiers.
				*/
				psMoveInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psMoveInst, IFMOV);
				psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psMoveInst->asDest[0].uNumber = USC_TEMPREG_TEMPSRC + uArg;
				psMoveInst->asArg[0] = psInst->asArg[uArg];
				psMoveInst->u.psFloat->asSrcMod[0] = *psMod;
				AppendInst(psState, psCodeBlock, psMoveInst);

				ASSERT(CanUseSrc(psState, 
							     psMoveInst, 
								 0, 
								 psMoveInst->asArg[0].uType, 
								 psMoveInst->asArg[0].uIndexType));

				/*
					Replace the source by the result of the FMOV.
				*/
				psInst->asArg[uArg].uType = psMoveInst->asDest[0].uType;
				psInst->asArg[uArg].uNumber = psMoveInst->asDest[0].uNumber;
				psInst->asArg[uArg].eFmt = UF_REGFORMAT_F32;

				/*
					Clear the source modifiers.
				*/
				psMod->bNegate = IMG_FALSE;
				psMod->bAbsolute = IMG_FALSE;
			}
		}

		AppendInst(psState, psCodeBlock, psInst);

		GenerateDestModF32(psState, psCodeBlock, &psInst->asDest[0], uSat, uScale, 
						   USC_PREDREG_NONE, IMG_FALSE);
	}

	/*
		Convert back to F16 format.
	*/
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		PINST		psInst;

		if ((psSrc->sDest.u.byMask & (1U << uChan)) == 0)
		{
			continue;
		}

		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IPCKF16F32);
		psInst->auDestMask[0] = g_puChanToMask[uChan];
		GetDestinationF16(psState, &psSrc->sDest, &psInst->asDest[0], uChan);
		if (uPredType != UF_PRED_NONE)
		{
			GetInputPredicateInst(psState, psInst, psSrc->uPredicate, uChan);
		}
		psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asArg[0].uNumber = auTempDest[uChan];
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
		AppendInst(psState, psCodeBlock, psInst);
	}

	/* Store into an indexable temporary register. */
	if (psSrc->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psCodeBlock, &psSrc->sDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);
	}
	else if (psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT || 
			 psSrc->sDest.eType == UFREG_TYPE_PSOUTPUT)
	{
		ConvertDestinationF16(psState, psCodeBlock, &psSrc->sDest);
	}
}

#if defined(SUPPORT_SGX545)
/*****************************************************************************
 FUNCTION	: ConvertSincosF16HWSupport
    
 PURPOSE	: Convert an sincos instruction to the intermediate format where the
			  target processor directly supports sin and cos instruction..

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
static IMG_VOID ConvertSincosF16HWSupport(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
{
	IMG_UINT32	uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uChan, uSrcComponent = 0;
	PINST		psMulInst;

	/*
		TEMP = SRC * (2/PI)
	*/
	psMulInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psMulInst, IFMUL16);
	psMulInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psMulInst->asDest[0].uNumber = USC_TEMPREG_TEMPSRC;
	psMulInst->asDest[0].eFmt = UF_REGFORMAT_F16;
	GetSingleSourceF16(psState,	
					   psCodeBlock, 
					   &psSrc->asSrc[0], 
					   &psMulInst->asArg[0], 
					   &uSrcComponent,
					   3, 
					   IMG_TRUE, 
					   &psMulInst->u.psFloat->asSrcMod[0],
					   IMG_FALSE);
	psMulInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
	psMulInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2OVERPI;
	psMulInst->u.psArith16->aeSwizzle[1] = FMAD16_SWIZZLE_CVTFROMF32;
	AppendInst(psState, psCodeBlock, psMulInst);

	for (uChan = 0; uChan < 2; uChan++)
	{
		PINST	psInst;

		if (!(psSrc->sDest.u.byMask & (1U << uChan)))
		{
			continue;
		}

		psInst = AllocateInst(psState, IMG_NULL);

		SetOpcode(psState, psInst, (uChan == 0) ? IFCOS : IFSIN);

		psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asDest[0].uNumber = USC_TEMPREG_TEMPDEST + uChan;
		psInst->asArg[0] = psMulInst->asDest[0];
		SetComponentSelect(psState, psInst, 0 /* uArgIdx */, uSrcComponent);

		/* No format conversion. */
		psInst->asDest[0].eFmt = psInst->asArg[0].eFmt;
		SetBit(psInst->auFlag, INST_TYPE_PRESERVE, 1);

		AppendInst(psState, psCodeBlock, psInst);
	}

	/* Move from the temporaries into the destination. */
	for (uChan = 0; uChan < 2; uChan++)
	{
		PINST	psPackInst;

		if (!(psSrc->sDest.u.byMask & (1U << uChan)))
		{
			continue;
		}

		psPackInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psPackInst, IPCKF16F16);
		psPackInst->auDestMask[0] = g_puChanToMask[uChan];
		GetDestinationF16(psState, &psSrc->sDest, &psPackInst->asDest[0], uChan);
		GetInputPredicateInst(psState, psPackInst, psSrc->uPredicate, 0);
		psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psPackInst->asArg[0].uNumber = USC_TEMPREG_TEMPDEST + uChan;
		psPackInst->asArg[0].eFmt = UF_REGFORMAT_F16;
		SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uSrcComponent);
		psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psPackInst->asArg[1].uNumber = 0;
		AppendInst(psState, psCodeBlock, psPackInst);

		GenerateDestModF16(psState, psCodeBlock, &psPackInst->asDest[0], uSat, uScale, 
						    uChan, 
							1);
	}
}

/*****************************************************************************
 FUNCTION	: ConvertSincos2F16HWSupport
    
 PURPOSE	: Convert an sincos2 instruction to the intermediate format where the
			  target processor directly supports sin and cos instruction..

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
static IMG_VOID ConvertSincos2F16HWSupport(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
{
	IMG_UINT32	uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uSat2 = (psSrc->sDest2.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uScale2 = (psSrc->sDest2.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uChan, uSrcComponent = 0;
	for(uChan=0; uChan<CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		PINST		psMulInst;

		/*
			TEMP = SRC * (2/PI)
		*/
		psMulInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMulInst, IFMUL16);
		psMulInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMulInst->asDest[0].uNumber = USC_TEMPREG_TEMPSRC;
		psMulInst->asDest[0].eFmt = UF_REGFORMAT_F16;
		GetSingleSourceF16(psState, 
						   psCodeBlock, 
						   &psSrc->asSrc[0], 
						   &psMulInst->asArg[0], 
						   &uSrcComponent,
						   uChan, 
						   IMG_TRUE, 
						   &psMulInst->u.psArith16->sFloat.asSrcMod[0],
						   IMG_FALSE);
		psMulInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
		psMulInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT2OVERPI;
		psMulInst->u.psArith16->aeSwizzle[1] = FMAD16_SWIZZLE_CVTFROMF32;
		psMulInst->u.psArith16->sFloat.asSrcMod[1].bNegate = IMG_FALSE;
		psMulInst->u.psArith16->sFloat.asSrcMod[1].bAbsolute = IMG_FALSE;
		AppendInst(psState, psCodeBlock, psMulInst);
		
		if ((psSrc->sDest.u.byMask & (1U << uChan)))
		{
			PINST	psInst;

			psInst = AllocateInst(psState, IMG_NULL);

			SetOpcode(psState, psInst, IFSIN);

			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = USC_TEMPREG_TEMPDEST + uChan;
			psInst->asArg[0] = psMulInst->asDest[0];
			SetComponentSelect(psState, psInst, 0 /* uArgIdx */, uSrcComponent);

			/* No format conversion. */
			psInst->asDest[0].eFmt = psInst->asArg[0].eFmt;
			SetBit(psInst->auFlag, INST_TYPE_PRESERVE, 1);

			AppendInst(psState, psCodeBlock, psInst);
		}

		/* Move from the temporaries into the destination. */
		if ((psSrc->sDest.u.byMask & (1U << uChan)))
		{
			PINST	psPackInst;

			psPackInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psPackInst, IPCKF16F16);
			psPackInst->auDestMask[0] = g_puChanToMask[uChan];
			GetDestinationF16(psState, &psSrc->sDest, &psPackInst->asDest[0], uChan);
			GetInputPredicateInst(psState, psPackInst, psSrc->uPredicate, 0);
			psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asArg[0].uNumber = USC_TEMPREG_TEMPDEST + uChan;
			psPackInst->asArg[0].eFmt = UF_REGFORMAT_F16;
			SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uSrcComponent);
			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			AppendInst(psState, psCodeBlock, psPackInst);

			GenerateDestModF16(psState, psCodeBlock, &psPackInst->asDest[0], uSat, uScale, 
							    uChan, 
								1);
		}
		
		if ((psSrc->sDest2.u.byMask & (1U << uChan)))
		{
			PINST	psInst;

			psInst = AllocateInst(psState, IMG_NULL);

			SetOpcode(psState, psInst, IFCOS);

			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = USC_TEMPREG_TEMPDEST + uChan;
			psInst->asArg[0] = psMulInst->asDest[0];
			SetComponentSelect(psState, psInst, 0 /* uArgIdx */, uSrcComponent);

			/* No format conversion. */
			psInst->asDest[0].eFmt = psInst->asArg[0].eFmt;
			SetBit(psInst->auFlag, INST_TYPE_PRESERVE, 1);

			AppendInst(psState, psCodeBlock, psInst);
		}

		/* Move from the temporaries into the destination. */
		if ((psSrc->sDest2.u.byMask & (1U << uChan)))
		{
			PINST	psPackInst;

			psPackInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psPackInst, IPCKF16F16);
			psPackInst->auDestMask[0] = g_puChanToMask[uChan];
			GetDestinationF16(psState, &psSrc->sDest2, &psPackInst->asDest[0], uChan);
			GetInputPredicateInst(psState, psPackInst, psSrc->uPredicate, 0);
			psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asArg[0].uNumber = USC_TEMPREG_TEMPDEST + uChan;
			psPackInst->asArg[0].eFmt = UF_REGFORMAT_F16;
			SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uSrcComponent);
			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			AppendInst(psState, psCodeBlock, psPackInst);

			GenerateDestModF16(psState, psCodeBlock, &psPackInst->asDest[0], uSat2, uScale2, 
							    uChan, 
								1);
		}
	}
}
#endif /* defined(SUPPORT_SGX545) */

IMG_INTERNAL 
IMG_VOID GetGradientsF16(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 PUNIFLEX_INST			psInputInst,
						 IMG_UINT32				uGradDim,
						 IMG_UINT32				uCoord,
						 PSAMPLE_GRADIENTS		psGradients)
/*****************************************************************************
 FUNCTION	: GetGradientsF16
    
 PURPOSE	: Extract gradients for a texture sample instruction

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Texture load instruction.
			  uGradDim		- Dimension of gradients to get.
			  uCoord		- Coordinate to get gradients for.

 OUTPUT		: None.
			  
 RETURNS	: None.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		GetGradientsF16F32Vec(psState, psCodeBlock, psInputInst, uGradDim, uCoord, UF_REGFORMAT_F16, psGradients);
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
			PINST					psPackInst;
			IMG_UINT32				uDestOffset = uChan * 2 + uCoord;
			FLOAT_SOURCE_MODIFIER	sMod;
			IMG_UINT32				uComponent;

			psPackInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psPackInst, IUNPCKF32F16);
			psPackInst->auDestMask[0] = USC_DESTMASK_FULL;
			psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asDest[0].uNumber = psGradients->uGradStart + uDestOffset;
			psPackInst->asDest[0].eFmt = UF_REGFORMAT_F32;
			GetSingleSourceF16(psState, 
							   psCodeBlock, 
							   &psInputInst->asSrc[2 + uCoord], 
							   &psPackInst->asArg[0], 
							   &uComponent,
							   uChan, 
							   IMG_TRUE,
							   &sMod,
							   IMG_FALSE);
			SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uComponent);

			ApplyFloatSourceModifier(psState, 
									 psCodeBlock, 
									 NULL /* psInsertBeforeInst */,
									 &psPackInst->asArg[0], 
									 &psPackInst->asArg[0], 
									 UF_REGFORMAT_F32, 
									 &sMod);

			AppendInst(psState, psCodeBlock, psPackInst);
		}
	}
}

IMG_INTERNAL
IMG_VOID GetSampleIdxF16(PINTERMEDIATE_STATE	psState,
						 PCODEBLOCK				psCodeBlock,
						 PUNIFLEX_INST			psInputInst,
						 PARG					psSampleIdx)
/*****************************************************************************
 FUNCTION	: GetSampleIdxF16
    
 PURPOSE	: Extract sample idx for a texture sample instruction

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Texture load instruction.
			  psSampleIdx	- Sample Idx. update related info.

 OUTPUT		: None.
			  
 RETURNS	: None.
*****************************************************************************/
{		
	PINST					psPackInst;		
	FLOAT_SOURCE_MODIFIER	sMod;
	IMG_UINT32				uComponent;

	psPackInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psPackInst, IUNPCKF32F16);
	psPackInst->asDest[0] = *psSampleIdx;
	psPackInst->auDestMask[0] = USC_DESTMASK_FULL;	
	GetSingleSourceF16(psState, 
					   psCodeBlock, 
					   &psInputInst->asSrc[2], 
					   &psPackInst->asArg[0], 
					   &uComponent,
					   0, 
					   IMG_TRUE,
					   &sMod,
					   IMG_FALSE);
	SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uComponent);

	ApplyFloatSourceModifier(psState, 
							 psCodeBlock, 
							 NULL /* psInsertBeforeInst */,
							 &psPackInst->asArg[0], 
							 &psPackInst->asArg[0], 
							 UF_REGFORMAT_F32, 
							 &sMod);

	AppendInst(psState, psCodeBlock, psPackInst);
}

IMG_INTERNAL
IMG_VOID GetLODAdjustmentF16(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 PUNIFLEX_INST			psInputInst,
							 PSAMPLE_LOD_ADJUST		psLODAdjust)
/*****************************************************************************
 FUNCTION	: GetLODAdjustmentF16

 PURPOSE	: Generate instructions to put the LOD adjustment value in
			  a fixed register.

 PARAMETERS	: psState		- Compiler state
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Input instruction.

 RETURNS	: None.
*****************************************************************************/
{
	PINST					psPackInst;
	IMG_UINT32				uComponent;
	FLOAT_SOURCE_MODIFIER	sMod;
	PUF_REGISTER			psSrcArg;

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
		GetLODAdjustmentF16F32Vec(psState, psCodeBlock, psInputInst, UF_REGFORMAT_F16, psLODAdjust);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ASSERT(psLODAdjust->eLODFormat == UF_REGFORMAT_F32);

	/*
		Unpack the LOD to F32 format.
	*/
	psPackInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psPackInst, IUNPCKF32F16);
	psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psPackInst->asDest[0].uNumber = psLODAdjust->uLODTemp;
	psPackInst->asDest[0].eFmt = UF_REGFORMAT_F32;
	GetSingleSourceF16(psState, 
					   psCodeBlock, 
					   psSrcArg, 
					   &psPackInst->asArg[0], 
					   &uComponent,
					   3, 
					   IMG_TRUE, 
					   &sMod,
					   IMG_FALSE);
	SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uComponent);

	/*
		Apply any source modifiers.
	*/
	ApplyFloatSourceModifier(psState, 
							 psCodeBlock, 
							 NULL /* psInsertBeforeInst */,
							 &psPackInst->asArg[0], 
							 &psPackInst->asArg[0], 
							 UF_REGFORMAT_F32, 
							 &sMod);

	AppendInst(psState, psCodeBlock, psPackInst);
}

IMG_INTERNAL 
IMG_VOID GetSampleCoordinateArrayIndexF16(PINTERMEDIATE_STATE	psState, 
										  PCODEBLOCK			psCodeBlock, 
										  PUNIFLEX_INST			psInputInst, 
										  IMG_UINT32			uSrcChan, 
										  IMG_UINT32			uTexture,
										  IMG_UINT32			uArrayIndexTemp)									  
/*****************************************************************************
 FUNCTION	: GetSampleCoordinatesF16
    
 PURPOSE	: Extract coordinates or gradients for a texture sample instruction

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psSrc			-
			  uCoordMask	- Indicate which coordinates to use.
			  uGradMask		- Indicate which gradients to use.
			  bProjected	- Whether a projected sample.

 OUTPUT		: peTCFormat	- Format of texture coordinates.
			  puCoordSize	- Number of coordinates.
			  psCbase		- Base register of coordinates.

 RETURNS	: None.
*****************************************************************************/
{
	ARG			sF16Arg;
	IMG_UINT32	uComponent;

	GetSingleSourceF16(psState,
					   psCodeBlock,
					   &psInputInst->asSrc[0],
					   &sF16Arg,
					   &uComponent,
					   uSrcChan,
					   IMG_FALSE /* bAllowSourceMod */,
					   NULL /* psMod */,
					   IMG_FALSE);
	ConvertTextureArrayIndexToU16(psState, psCodeBlock, uTexture, &sF16Arg, uComponent, uArrayIndexTemp);
}

IMG_INTERNAL 
IMG_VOID GetProjectionF16(PINTERMEDIATE_STATE	psState,
						  PCODEBLOCK			psCodeBlock,
						  PUNIFLEX_INST			psInputInst,
						  PSAMPLE_PROJECTION	psProj,
						  UF_REGFORMAT			eCoordFormat)
/*****************************************************************************
 FUNCTION	: GetProjectionF16

 PURPOSE	: Extract the location of the value to project the texture coordinates
			  by from an input texture sample instruction.

 PARAMETERS	: psState		- Compiler state
			  psCodeBlock	- Block for any extra instructions.
			  psInputInst	- Input texture sample instruction.
			  psProj		- Returns detail of the projection value.

 RETURNS	: None.
*****************************************************************************/
{
	FLOAT_SOURCE_MODIFIER	sProjMod;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		GetProjectionF16F32Vec(psState, psCodeBlock, psInputInst, psProj, eCoordFormat, IMG_TRUE /* bF16 */);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	if (eCoordFormat == UF_REGFORMAT_F32)
	{
		GetProjectionF32(psState, psCodeBlock, psInputInst, psProj, eCoordFormat);
		return;
	}

	ASSERT(eCoordFormat == UF_REGFORMAT_F16);

	GetSingleSourceF16(psState, 
					   psCodeBlock, 
					   &psInputInst->asSrc[0], 
					   &psProj->sProj, 
					   &psProj->uProjChannel,
					   USC_W_CHAN, 
					   IMG_TRUE /* bAllowSourceMod */, 
					   &sProjMod,
					   IMG_FALSE);

	psProj->uProjMask = USC_XY_CHAN_MASK;

	/*
		The projection source has a negate or absolute source modifier emit extra instructions
		to apply it.
	*/
	ApplyFloatSourceModifier(psState, 
							 psCodeBlock, 
							 NULL /* psInsertBeforeInst */,
							 &psProj->sProj, 
							 &psProj->sProj, 
							 UF_REGFORMAT_F16, 
							 &sProjMod);
}

IMG_INTERNAL 
IMG_VOID GetSampleCoordinatesF16(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psCodeBlock,
								 PUNIFLEX_INST			psInputInst,
								 IMG_UINT32				uDimensionality,
								 PSAMPLE_COORDINATES	psSampleCoordinates,
								 IMG_BOOL				bPCF)
/*****************************************************************************
 FUNCTION	: GetSampleCoordinatesF16
    
 PURPOSE	: Extract coordinates or gradients for a texture sample instruction

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psSrc			-
			  uCoordMask	- Indicate which coordinates to use.
			  uGradMask		- Indicate which gradients to use.
			  bProjected	- Whether a projected sample.

 OUTPUT		: peTCFormat	- Format of texture coordinates.
			  puCoordSize	- Number of coordinates.
			  psCbase		- Base register of coordinates.
			  bCordsHaveOffsets
							- Coordinates have immediate offsets.

 RETURNS	: None.
*****************************************************************************/
{
	PINST		psMoveInst;
	IMG_UINT32	uReg;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		GetSampleCoordinatesF16F32Vec(psState, 
									  psCodeBlock, 
									  psInputInst, 
									  uDimensionality, 
									  psSampleCoordinates,
									  bPCF);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		If the PCF comparison source is F32 then expand the texture coordinates
		to F32.
	*/
	if (psSampleCoordinates->eCoordFormat == UF_REGFORMAT_F32)
	{
		GetSampleCoordinatesF32(psState,
								psCodeBlock,
								psInputInst,
								uDimensionality,
								psSampleCoordinates,
								bPCF);
		return;
	}

	ASSERT(psSampleCoordinates->eCoordFormat == UF_REGFORMAT_F16);

	/* Get the coordinates for this access. */
	for (uReg = 0; uReg < psSampleCoordinates->uCoordSize; uReg++)
	{
		IMG_UINT32				uChanMask;
		FMAD16_SWIZZLE			eSwizzle;
		FLOAT_SOURCE_MODIFIER	sMod;

		/*
			Mask of the channels used from this texture
			coordinate register.
		*/
		uChanMask = (uReg == 0) ? 3 : 1;

		/*
			Get the texture coordinate source.
		*/
		psMoveInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMoveInst, IMOV);
		psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMoveInst->asDest[0].uNumber = psSampleCoordinates->uCoordStart + uReg;
		psMoveInst->asDest[0].eFmt = UF_REGFORMAT_F16;
		GetDoubleSourceF16(psState, 
						   psCodeBlock, 
						   &psInputInst->asSrc[0], 
						   0, 
						   &psMoveInst->asArg[0], 
						   &eSwizzle, 
						   uReg, 
						   IMG_TRUE,
						   &sMod,
						   uChanMask);

		/*
			Switch to FMOV16 if the texture coordinate has a source modifier.
		*/
		if (sMod.bNegate ||
			sMod.bAbsolute ||
			eSwizzle != FMAD16_SWIZZLE_LOWHIGH)
		{
			SetOpcode(psState, psMoveInst, IFMOV16);
			psMoveInst->u.psArith16->aeSwizzle[0] = eSwizzle;
			psMoveInst->u.psArith16->sFloat.asSrcMod[0] = sMod;
		}
		AppendInst(psState, psCodeBlock, psMoveInst);
	}	

	if (bPCF && (psInputInst->eOpCode == UFOP_LDC || psInputInst->eOpCode == UFOP_LDCLZ))
	{
		PINST					psPackInst;
		IMG_UINT32				uPCFOff;
		IMG_UINT32				uPCFMask;
		PUF_REGISTER			psCompSrcArg = &psInputInst->asSrc[2];
		FLOAT_SOURCE_MODIFIER	sSourceMod;

		/*
			Where does the PCF comparison value come from in the texture coordinates.
		*/
		switch (uDimensionality)
		{
			case 2: uPCFOff = 0; uPCFMask = 0xC; break;
			case 3: uPCFOff = 1; uPCFMask = 0x3; break;
			case 4: uPCFOff = 1; uPCFMask = 0xC; break;
			default: imgabort();
		}

		/*
			Move the PCF comparison value from SRC2.W into the texture coordinates.
		*/
		psPackInst = AllocateInst(psState, IMG_NULL);
		if (psCompSrcArg->eFormat == UF_REGFORMAT_F16)
		{
			IMG_UINT32	uSourceComponent;

			SetOpcode(psState, psPackInst, IPCKF16F16);
			GetSingleSourceF16(psState, 
							   psCodeBlock,
							   psCompSrcArg,
							   &psPackInst->asArg[0],
							   &uSourceComponent,
							   USC_W_CHAN,
							   IMG_TRUE,
							   &sSourceMod /* bAllowSourceMod */,
							   IMG_FALSE);
			SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uSourceComponent);
		}
		else
		{
			ASSERT(psCompSrcArg->eFormat == UF_REGFORMAT_F32);
			SetOpcode(psState, psPackInst, IPCKF16F32);
			GetSourceF32(psState,
						 psCodeBlock,
						 psCompSrcArg,
						 USC_W_CHAN,
						 &psPackInst->asArg[0],
						 IMG_TRUE /* bAllowSourceMod */,
						 &sSourceMod);
		}
		psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psPackInst->asDest[0].uNumber = psSampleCoordinates->uCoordStart + uPCFOff;
		psPackInst->asDest[0].eFmt = UF_REGFORMAT_F16;
		psPackInst->auDestMask[0] = uPCFMask;
		psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psPackInst->asArg[1].uNumber = 0;
		AppendInst(psState, psCodeBlock, psPackInst);

		/*
			Apply any source modifiers from SRC2.
		*/
		ApplyFloatSourceModifier(psState, 
								 psCodeBlock, 
								 NULL /* psInsertBeforeInst */,
								 &psPackInst->asArg[0], 
								 &psPackInst->asArg[0], 
								 UF_REGFORMAT_F16, 
								 &sSourceMod);
	}
}

#if defined(OUTPUT_USCHW)
static IMG_VOID UnpackTextureF16(PINTERMEDIATE_STATE	psState, 
								 PCODEBLOCK				psCodeBlock, 
								 PSAMPLE_RESULT_LAYOUT	psSampleResultLayout,
								 PSAMPLE_RESULT			psSampleResult,
								 PUNIFLEX_INST			psInputInst,
								 IMG_UINT32				uMask)
/*****************************************************************************
 FUNCTION	: UnpackTextureF16
    
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
	IMG_UINT32 uChan;
	IMG_UINT32 uSat = (psInputInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32 uScale = (psInputInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;

	ASSERT(psSampleResultLayout->uChanCount == CHANNELS_PER_INPUT_REGISTER);

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (uMask & (1U << uChan))
		{
			IMG_UINT32			uSrcRegNum, uSrcChanOffset;
			USC_CHANNELFORM		eFormat;
			PINST				psInst;

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

			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, INVALID_MAXARG);
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 0;

			if ((psInputInst->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_NONE)
			{
				GetInputPredicateInst(psState, psInst, psInputInst->uPredicate, uChan);
			}

			if (eFormat == USC_CHANNELFORM_ONE || eFormat == USC_CHANNELFORM_ZERO)
			{
				SetOpcode(psState, psInst, IPCKF16F16);
				psInst->asArg[0].uType = USEASM_REGTYPE_FPCONSTANT;
				if (eFormat == USC_CHANNELFORM_ZERO)
				{
					psInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
				}
				else
				{
					psInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT16ONE;
				}
				psInst->asArg[0].eFmt = UF_REGFORMAT_F16;
			}
			else if (eFormat == USC_CHANNELFORM_U24)
			{
				IMG_UINT32	uUnpackDest = GetNextRegister(psState);

				UnpackU24Channel(psState, psCodeBlock, uSrcRegNum, uSrcChanOffset, uUnpackDest);
				
				SetOpcode(psState, psInst, IPCKF16F32);
				psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
				psInst->asArg[0].uNumber = uUnpackDest;
			}
			else
			{
				UF_REGFORMAT	eFmt = UF_REGFORMAT_F32;
				switch (eFormat)
				{
					case USC_CHANNELFORM_U8: 
						SetOpcode(psState, psInst, IUNPCKF16U8); 
						psInst->u.psPck->bScale = IMG_TRUE;
						eFmt = UF_REGFORMAT_U8;
						break;
					case USC_CHANNELFORM_S8: 
						SetOpcode(psState, psInst, IUNPCKF16S8); 	
						psInst->u.psPck->bScale = IMG_TRUE;
						break;
					case USC_CHANNELFORM_U16: 
						SetOpcode(psState, psInst, IUNPCKF16U16); 
						psInst->u.psPck->bScale = IMG_TRUE;
						break;
					case USC_CHANNELFORM_S16: 
						SetOpcode(psState, psInst, IUNPCKF16S16); 
						psInst->u.psPck->bScale = IMG_TRUE;
						break;
					case USC_CHANNELFORM_F16: 
						SetOpcode(psState, psInst, IPCKF16F16); 
						eFmt = UF_REGFORMAT_F16; 
						break;
					case USC_CHANNELFORM_DF32: 
						{
							PINST psInstDF32;							
							psInstDF32 = AllocateInst(psState, IMG_NULL);
							SetOpcode(psState, psInstDF32, IAND);
							psInstDF32->asDest[0].uType = USEASM_REGTYPE_TEMP;
							psInstDF32->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
							psInstDF32->asArg[0].uNumber = 0x7FFFFFFF;
							psInstDF32->asArg[1].uType = USEASM_REGTYPE_TEMP;
							psInstDF32->asArg[1].uNumber = uSrcRegNum;
							psInstDF32->asDest[0].uType = USEASM_REGTYPE_TEMP;
							psInstDF32->asDest[0].uNumber = GetNextRegister(psState);
							uSrcRegNum = psInstDF32->asDest[0].uNumber;
							AppendInst(psState, psCodeBlock, psInstDF32);
						}
					case USC_CHANNELFORM_F32: 
						SetOpcode(psState, psInst, IPCKF16F32); 
						psInst->u.psPck->bScale = IMG_TRUE;
						break;
					case USC_CHANNELFORM_U8_UN: 
						SetOpcode(psState, psInst, IUNPCKF16U8); 
						break;
					case USC_CHANNELFORM_C10: 
						SetOpcode(psState, psInst, IUNPCKF16C10); 
						eFmt = UF_REGFORMAT_C10;
						break;
					default: imgabort();
				}
				psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
				psInst->asArg[0].uNumber = uSrcRegNum;
				SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uSrcChanOffset);
				psInst->asArg[0].eFmt = eFmt;
				psInst->asArg[1] = psInst->asArg[0];
				SetPCKComponent(psState, psInst, 1 /* uArgIdx */, uSrcChanOffset);
			}

			psInst->auDestMask[0] = g_puChanToMask[uChan];
			GetDestinationF16(psState, &psInputInst->sDest, &psInst->asDest[0], uChan); 

			AppendInst(psState, psCodeBlock, psInst);

			/*
				Generate code to apply destination modifiers 
			*/
			GenerateDestModF16(psState, psCodeBlock, &psInst->asDest[0], uSat, uScale, uChan, 1 /* uChanCount */);
		}
	}
}

static 
PCODEBLOCK ConvertSamplerInstructionF16USCHW(PINTERMEDIATE_STATE psState, 
									  		 PCODEBLOCK psCodeBlock, 
									  		 PUNIFLEX_INST psSrc,
									  		 IMG_BOOL bConditional)
/*****************************************************************************
 FUNCTION	: ConvertSamplerInstructionF16USCHW
    
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

	/* We might need to optimize the unpacking code. */
	psState->uFlags &= ~USC_FLAGS_INTEGERINLASTBLOCKONLY;

	/* Get the raw results of the texture load in a register. */
	psCodeBlock = ConvertSamplerInstructionCore(psState, 
												psCodeBlock, 
												psSrc, 
												&sSampleResultLayout,
												&sSampleResult, 
												bConditional);	

	/* Unpack the texture data, handling write-masking, predication and swizzle */
	UnpackTextureF16(psState,
					 psCodeBlock, 
					 &sSampleResultLayout,
					 &sSampleResult,
					 psSrc, 
					 psSrc->sDest.u.byMask);

	/* Store into an indexable temporary register. */
	if (psSrc->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psCodeBlock, &psSrc->sDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);
	}
	else if (psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT)
	{
		ConvertDestinationF16(psState, psCodeBlock, &psSrc->sDest);
	}

	/* Free memory allocated by ConvertSamplerInstructionCore. */
	FreeSampleResultLayout(psState, &sSampleResultLayout);

	return psCodeBlock;
}

static IMG_VOID ConvertLoadTiledInstructionF16(PINTERMEDIATE_STATE	psState, 
											   PCODEBLOCK			psCodeBlock, 
											   PUNIFLEX_INST		psInputInst)
/*****************************************************************************
 FUNCTION	: ConvertLoadTiledInstructionF16
    
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
	UnpackTextureF16(psState,
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
#endif /* defined(OUTPUT_USCHW) */


static IMG_VOID MoveF16VectorToInputDest(PINTERMEDIATE_STATE	psState,
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
			PINST		psPackInst;
			IMG_UINT32	uSrcChan;

			uSrcChan = EXTRACT_CHAN(uSwizzle, uChan);
			ASSERT(uSrcChan >= UFREG_SWIZ_X && uSrcChan <= UFREG_SWIZ_W);

			psPackInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psPackInst, IPCKF16F16);
			psPackInst->auDestMask[0] = g_puChanToMask[uChan];
			GetDestinationF16(psState, &psInputInst->sDest, &psPackInst->asDest[0], uChan);
			psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asArg[0].uNumber = uVecStartReg + (uSrcChan >> 1);
			SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, (uSrcChan & 1) << 1);
			psPackInst->asArg[0].eFmt = UF_REGFORMAT_F16;
			if ((psInputInst->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_NONE)
			{
				GetInputPredicateInst(psState, psPackInst, psInputInst->uPredicate, uChan);
			}
			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			AppendInst(psState, psCodeBlock, psPackInst);
		}
	}
}

#if defined(OUTPUT_USPBIN)
static 
PCODEBLOCK ConvertSamplerInstructionF16USPBIN(PINTERMEDIATE_STATE psState, 
											  PCODEBLOCK psCodeBlock, 
											  PUNIFLEX_INST psInputInst, 
											  IMG_BOOL bConditional)
/*****************************************************************************
 FUNCTION	: ConvertSamplerInstructionF16USPBIN
    
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
	ARG			asDest[SCALAR_REGISTERS_PER_F16_VECTOR];

	uChanMask		= psInputInst->sDest.u.byMask;
	uChanSwizzle	= psInputInst->asSrc[1].u.uSwiz;

	/* Setup the location where we want the data to be sampled */
	if	((psInputInst->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_NONE || psInputInst->sDest.byMod != 0)
	{
		MakeArgumentSet(asDest, 
						SCALAR_REGISTERS_PER_F16_VECTOR, 
						USEASM_REGTYPE_TEMP, 
						GetNextRegisterCount(psState, SCALAR_REGISTERS_PER_F16_VECTOR), 
						UF_REGFORMAT_F16);

		uChanSwizzle = UFREG_SWIZ_NONE;
	}
	else
	{
		IMG_UINT32	uDest;

		for (uDest = 0; uDest < SCALAR_REGISTERS_PER_F16_VECTOR; uDest++)
		{
			GetDestinationF16(psState, &psInputInst->sDest, &asDest[uDest], uDest * F16_CHANNELS_PER_SCALAR_REGISTER); 
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

	/* Unpack to f16 format. */
	if	((psInputInst->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_NONE || psInputInst->sDest.byMod != 0)
	{
		IMG_UINT32	uChan;
		IMG_UINT32	uSat = (psInputInst->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
		IMG_UINT32	uScale = (psInputInst->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;

		if (uSat != UFREG_DMOD_SATNONE || uScale != UFREG_DMOD_SCALEMUL1)
		{
			for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
			{
				if ((psInputInst->sDest.u.byMask & (1U << uChan)) != 0)
				{
					ARG	sDestChan;

					sDestChan = asDest[uChan / F16_CHANNELS_PER_SCALAR_REGISTER];
	
					GenerateDestModF16(psState, 
									   psCodeBlock, 
									   &sDestChan, 
									   uSat, 
									   uScale, 	
									   uChan,
									   1 /* uChanCount */);
				}
			}	
		}

		MoveF16VectorToInputDest(psState, psCodeBlock, psInputInst, psInputInst->asSrc[1].u.uSwiz, asDest[0].uNumber);
	}

	/* Store into an indexable temporary register. */
	if (psInputInst->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psCodeBlock, &psInputInst->sDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);
	}
	else if (psInputInst->sDest.eType == UFREG_TYPE_VSOUTPUT || psInputInst->sDest.eType == UFREG_TYPE_PSOUTPUT)
	{
		ConvertDestinationF16(psState, psCodeBlock, &psInputInst->sDest);
	}
	return psCodeBlock;
}
#endif	/* #if defined(OUTPUT_USPBIN) */

IMG_INTERNAL
IMG_VOID ApplyFloatSourceModifier(PINTERMEDIATE_STATE		psState,
								  PCODEBLOCK				psBlock,
								  PINST						psInsertBeforeInst,
								  PARG						psSrcArg,
								  PARG						psDestArg,
								  UF_REGFORMAT				eSrcFmt,
								  PFLOAT_SOURCE_MODIFIER	psMod)
/*****************************************************************************
 FUNCTION	: ApplyFloatSourceModifier
    
 PURPOSE	: If a source argument has a negate or absolute modifier then
			  emit a separate instruction to generate the value of the
			  argument with the modifier applied.

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block for any extra instructions.
			  psInsertBeforeInst
							- Point to insert any new instructions.
			  psArg			- The argument to apply the source modifiers to.
			  psMod			- Source modifiers to apply.
			  eSrcFmt		- Format of the argument.

 OUTPUT		: psArg			- A new variable whose value is the source argument
							with its source modifiers applied.

 RETURNS	: None.
*****************************************************************************/
{
	if (psMod->bNegate || psMod->bAbsolute)
	{
		PINST		psMoveInst;

		psMoveInst = AllocateInst(psState, psInsertBeforeInst);
		if (eSrcFmt == UF_REGFORMAT_F32)
		{
			SetOpcode(psState, psMoveInst, IFMOV);
			psMoveInst->u.psFloat->asSrcMod[0] = *psMod;
			psMoveInst->u.psFloat->asSrcMod[0].uComponent = 0;
		}
		else
		{
			ASSERT(eSrcFmt == UF_REGFORMAT_F16);
			SetOpcode(psState, psMoveInst, IFMOV16);
			psMoveInst->u.psArith16->sFloat.asSrcMod[0] = *psMod;
			psMoveInst->u.psArith16->sFloat.asSrcMod[0].uComponent = 0;
		}
		psMoveInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMoveInst->asDest[0].uNumber = GetNextRegister(psState);
		psMoveInst->asDest[0].eFmt = eSrcFmt;
		psMoveInst->asArg[0] = *psSrcArg; 
		InsertInstBefore(psState, psBlock, psMoveInst, psInsertBeforeInst);
		
		*psDestArg = psMoveInst->asDest[0];
	}
}

static IMG_VOID ConvertMoveInstructionF16(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertMoveInstructionF16
    
 PURPOSE	: Convert an input mov instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	UF_REGFORMAT	eSrcFmt = GetRegisterFormat(psState, &psSrc->asSrc[0]);
	ARG				sC10Src = USC_ARG_DEFAULT_VALUE;
	IMG_UINT32		uChan;
	IMG_UINT32		uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32		uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32		uTempDest;

	if	((eSrcFmt == UF_REGFORMAT_C10) || (eSrcFmt == UF_REGFORMAT_U8))
	{
		GetSourceC10(psState, psCodeBlock, &psSrc->asSrc[0], psSrc->asSrc[0].byMod, &sC10Src, psSrc->sDest.u.byMask, 
			IMG_FALSE, IMG_FALSE, psSrc->asSrc[0].eFormat);
	}

	/*
		Use a temporary for the destination to avoid conversion instructions for earlier channels overwriting 
		source data for later channels.
	*/
	uTempDest = USC_UNDEF;
	if (psSrc->sDest.eType == psSrc->asSrc[0].eType &&
		psSrc->sDest.uNum == psSrc->asSrc[0].uNum)
	{
		uTempDest = GetNextRegisterCount(psState, SCALAR_REGISTERS_PER_F16_VECTOR);
	}

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		PINST		psPackInst;

		if ((psSrc->sDest.u.byMask & (1U << uChan)) == 0)
		{
			continue;
		}

		psPackInst = AllocateInst(psState, IMG_NULL);
		switch (eSrcFmt)
		{
			case UF_REGFORMAT_C10: 
			{
				SetOpcode(psState, psPackInst, IUNPCKF16C10); 
				psPackInst->u.psPck->bScale = IMG_TRUE;
				psPackInst->asArg[0] = sC10Src;
				SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, ConvertInputChannelToIntermediate(psState, uChan));
				break;
			}
			case UF_REGFORMAT_U8: 
			{
				SetOpcode(psState, psPackInst, IUNPCKF16U8); 
				psPackInst->u.psPck->bScale = IMG_TRUE;
				psPackInst->asArg[0] = sC10Src;
				SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, ConvertInputChannelToIntermediate(psState, uChan));
				break;
			}
			case UF_REGFORMAT_F16: 
			{
				FLOAT_SOURCE_MODIFIER	sMod;
				IMG_UINT32				uComponent;
				
				SetOpcode(psState, psPackInst, IPCKF16F16); 
				GetSingleSourceF16(psState, 
								   psCodeBlock, 
								   &psSrc->asSrc[0], 
								   &psPackInst->asArg[0], 
								   &uComponent,
								   uChan, 
								   IMG_TRUE, 
								   &sMod,
								   IMG_FALSE);
				ApplyFloatSourceModifier(psState, 
										 psCodeBlock, 
										 NULL /* psInsertBeforeInst */,
										 &psPackInst->asArg[0], 
										 &psPackInst->asArg[0], 
										 UF_REGFORMAT_F16, 
										 &sMod);
				SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, uComponent);
				break;
			}
			case UF_REGFORMAT_F32: 
			{
				FLOAT_SOURCE_MODIFIER	sMod;

				SetOpcode(psState, psPackInst, IPCKF16F32); 
				GetSourceF32(psState, 
							 psCodeBlock, 
							 &psSrc->asSrc[0], 
							 uChan, 
							 &psPackInst->asArg[0], 
							 IMG_TRUE, 
							 &sMod);
				ApplyFloatSourceModifier(psState, 
										 psCodeBlock, 
										 NULL /* psInsertBeforeInst */,
										 &psPackInst->asArg[0], 
										 &psPackInst->asArg[0], 
										 UF_REGFORMAT_F32, 
										 &sMod);
				break;
			}
			default:
			{
				imgabort();
				break;
			}
		}
		psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psPackInst->asArg[1].uNumber = 0;

		psPackInst->auDestMask[0] = g_puChanToMask[uChan];
		if (uTempDest != USC_UNDEF)
		{
			psPackInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asDest[0].uNumber = uTempDest + (uChan >> 1);
			psPackInst->asDest[0].eFmt = UF_REGFORMAT_F16;
		}
		else
		{
			if ((psSrc->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_NONE)
			{
				GetInputPredicateInst(psState, psPackInst, psSrc->uPredicate, uChan);
			}
			GetDestinationF16(psState, &psSrc->sDest, &psPackInst->asDest[0], uChan);
		}

		AppendInst(psState, psCodeBlock, psPackInst);

		GenerateDestModF16(psState, psCodeBlock, &psPackInst->asDest[0], uSat, uScale, uChan, 1);
	}

	if (uTempDest != USC_UNDEF)
	{
		MoveF16VectorToInputDest(psState, psCodeBlock, psSrc, UFREG_SWIZ_NONE, uTempDest);
	}

	/* Store into an indexable temporary register. */
	if (psSrc->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psCodeBlock, &psSrc->sDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);
	}
	else if (psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT)
	{
		ConvertDestinationF16(psState, psCodeBlock, &psSrc->sDest);
	}
}

static IMG_VOID ConvertComparisonInstructionF16(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertComparisonInstructionF16
    
 PURPOSE	: Convert an input comparison instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	PINST psInst;
	IMG_UINT32 uChan;
	IMG_UINT32 uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32 uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_BOOL bSrc0Neg, bSrc0Abs;
	ARG sSrc1 = USC_ARG_DEFAULT_VALUE;
	ARG sSrc2 = USC_ARG_DEFAULT_VALUE;
	TEST_TYPE eTest;
	IMG_UINT32 uTrueSrc;

	bSrc0Neg = (psSrc->asSrc[0].byMod & UFREG_SOURCE_NEGATE) ? IMG_TRUE : IMG_FALSE;
	bSrc0Abs = (psSrc->asSrc[0].byMod & UFREG_SOURCE_ABS) ? IMG_TRUE : IMG_FALSE;

	/*
		Convert the instruction type (CMP or CMPLT) and the source modifier on
		source 0 to a MOVC test type.
	*/
	GetCMPTestType(psState, psSrc, bSrc0Neg, bSrc0Abs, &eTest, &uTrueSrc);

	for (uChan = 0; uChan < 4; uChan++)
	{
		IMG_UINT32	uSrc;

		/* Ignore this component if it isn't included in the instruction mask. */
		if ((psSrc->sDest.u.byMask & (1U << uChan)) == 0)
		{
			continue;
		}

		/*
			For get the additional sources and apply any source modifiers.
		*/
		for (uSrc = 1; uSrc < 3; uSrc++)
		{
			PARG					psHwSource = (uSrc == 1) ? &sSrc1 : &sSrc2;
			FLOAT_SOURCE_MODIFIER	sMod;

			GetSingleSourceConvertToF32(psState, psCodeBlock, &psSrc->asSrc[uSrc], psHwSource, uChan, IMG_TRUE, &sMod);

			if (sMod.bNegate || sMod.bAbsolute)
			{
				psInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psInst, IFMOV);
				psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psInst->asDest[0].uNumber = USC_TEMPREG_TEMPSRC + (uSrc * 4) + uChan;
				psInst->asArg[0] = *psHwSource;
				psInst->u.psFloat->asSrcMod[0] = sMod;

				AppendInst(psState, psCodeBlock, psInst);

				*psHwSource = psInst->asDest[0];
			}
		}

		/* 
			Apply any source modifier
		*/
		if (bSrc0Neg && !bSrc0Abs)
		{
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IFMOV);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = USC_TEMPREG_DRDEST;
			GetSingleSourceConvertToF32(psState, 
										psCodeBlock, 
										&psSrc->asSrc[0], 
										&psInst->asArg[0], 
										uChan, 
										IMG_TRUE,
										&psInst->u.psFloat->asSrcMod[0]);
			AppendInst(psState, psCodeBlock, psInst);
		}

		/*
			Do test instruction itself.
		*/
		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, INVALID_MAXARG);
		psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asDest[0].uNumber = USC_TEMPREG_TEMPDEST + uChan;
		/* ABS(SRC0) >= 0 -> TRUE */
		if (eTest == TEST_TYPE_ALWAYS_TRUE)
		{
			SetOpcode(psState, psInst, IMOV);
			psInst->asArg[0] = (uTrueSrc == 1) ? sSrc1 : sSrc2;
		}
		else
		{
			SetOpcode(psState, psInst, IMOVC);
			if (bSrc0Neg && !bSrc0Abs)
			{
				psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
				psInst->asArg[0].uNumber = USC_TEMPREG_DRDEST;
			}
			else
			{
				GetSingleSourceConvertToF32(psState, 
											psCodeBlock, 
											&psSrc->asSrc[0], 
											&psInst->asArg[0], 
											uChan, 
											IMG_FALSE,
											NULL);
			}
			psInst->u.psMovc->eTest = eTest;
			
			psInst->asArg[1] = sSrc1;
			psInst->asArg[2] = sSrc2;
		}
		AppendInst(psState, psCodeBlock, psInst);

		/* Do any destination modifiers. */
		GenerateDestModF32(psState, psCodeBlock, &psInst->asDest[0], uSat, uScale, USC_PREDREG_NONE, IMG_FALSE);
	}

	for (uChan = 0; uChan < 4; uChan++)
	{
		if (psSrc->sDest.u.byMask & (1U << uChan))
		{
			PINST		psPackInst;

			psPackInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psPackInst, IPCKF16F32);
			psPackInst->auDestMask[0] = g_puChanToMask[uChan];
			GetDestinationF16(psState, &psSrc->sDest, &psPackInst->asDest[0], uChan);
			psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asArg[0].uNumber = USC_TEMPREG_TEMPDEST + uChan;
			if ((psSrc->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_NONE)
			{
				GetInputPredicateInst(psState, psPackInst, psSrc->uPredicate, uChan);
			}
			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			AppendInst(psState, psCodeBlock, psPackInst);
		}
	}

	/* Store into an indexable temporary register. */
	if (psSrc->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psCodeBlock, &psSrc->sDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);
	}
	else if (psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT)
	{
		ConvertDestinationF16(psState, psCodeBlock, &psSrc->sDest);
	}
}

static IMG_VOID ConvertDotProductInstructionF16(PINTERMEDIATE_STATE psState, 
												PCODEBLOCK psCodeBlock, 
												PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertDotProductInstructionF16
    
 PURPOSE	: Convert a DOT2ADD/DOT3 instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uDotProductTemp = GetNextRegister(psState);

	/*	
	  Calculate the result in F32 format.
	*/
	for (uChan = 0; uChan < 3; uChan++)
	{
		PINST	psMadInst;

		psMadInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMadInst, (uChan == 0) ? IFMUL : IFMAD);
		psMadInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMadInst->asDest[0].uNumber = uDotProductTemp;
		if (psSrc->eOpCode == UFOP_DOT2ADD && uChan == 2)
		{
			IMG_UINT32	uArg2Component;

			SetOpcode(psState, psMadInst, IFADD);
			GetSingleSourceF16(psState, 
							   psCodeBlock, 
							   &psSrc->asSrc[2], 
							   &psMadInst->asArg[0], 
							   &uArg2Component,
							   uChan, 
							   IMG_TRUE, 
							   &psMadInst->u.psFloat->asSrcMod[0],
							   IMG_TRUE);
			psMadInst->u.psFloat->asSrcMod[0].uComponent = uArg2Component;

			psMadInst->asArg[1] = psMadInst->asDest[0];
		}
		else
		{
			IMG_UINT32	uArg0Component;
			IMG_UINT32	uArg1Component;

			GetSingleSourceF16(psState, 
							   psCodeBlock, 
							   &psSrc->asSrc[0], 
							   &psMadInst->asArg[0], 
							   &uArg0Component,
							   uChan, 
							   IMG_TRUE, 
							   &psMadInst->u.psFloat->asSrcMod[0],
							   IMG_TRUE);
			psMadInst->u.psFloat->asSrcMod[0].uComponent = uArg0Component;

			GetSingleSourceF16(psState, 
							   psCodeBlock, 
							   &psSrc->asSrc[1], 
							   &psMadInst->asArg[1], 
							   &uArg1Component,
							   uChan, 
							   IMG_TRUE, 
							   &psMadInst->u.psFloat->asSrcMod[1],
							   IMG_TRUE);
			psMadInst->u.psFloat->asSrcMod[1].uComponent = uArg1Component;

			if (uChan > 0)
			{
				psMadInst->asArg[2] = psMadInst->asDest[0];
			}
		}

		AppendInst(psState, psCodeBlock, psMadInst);

		if (uChan == 2)
		{
			/* Do any destination modifiers. */
			GenerateDestModF32(psState, psCodeBlock, &psMadInst->asDest[0], uSat, 
							   uScale, USC_PREDREG_NONE, IMG_FALSE);
		}
	}
	/*
		Convert back to F16 format and broadcast to all channels of the destination.
	*/
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (psSrc->sDest.u.byMask & (1U << uChan))
		{
			PINST		psPackInst;

			psPackInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psPackInst, IPCKF16F32);
			psPackInst->auDestMask[0] = g_puChanToMask[uChan];
			GetDestinationF16(psState,  &psSrc->sDest, &psPackInst->asDest[0], uChan);
			if ((psSrc->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_NONE)
			{
				GetInputPredicateInst(psState, psPackInst, psSrc->uPredicate, uChan);
			}
			psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asArg[0].uNumber = uDotProductTemp;
			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			AppendInst(psState, psCodeBlock, psPackInst);
		}
	}

	/* Store into an indexable temporary register. */
	if (psSrc->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psCodeBlock, &psSrc->sDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);
	}
	else if (psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT || 
			 psSrc->sDest.eType == UFREG_TYPE_PSOUTPUT)
	{
		ConvertDestinationF16(psState, psCodeBlock, &psSrc->sDest);
	}
}

static IMG_VOID ConvertDot4InstructionF16(PINTERMEDIATE_STATE psState, 
										  PCODEBLOCK psCodeBlock, 
										  PUNIFLEX_INST psSrc)
/*****************************************************************************
 FUNCTION	: ConvertDot4InstructionF16
    
 PURPOSE	: Convert a DOT4 instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  
 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uHalf;
	PINST		psAddInst;
	IMG_UINT32	uChan;
	IMG_UINT32	uSat = (psSrc->sDest.byMod & UFREG_DMOD_SAT_MASK) >> UFREG_DMOD_SAT_SHIFT;
	IMG_UINT32	uScale = (psSrc->sDest.byMod & UFREG_DMOD_SCALE_MASK) >> UFREG_DMOD_SCALE_SHIFT;
	IMG_UINT32	uDPHalfResults = GetNextRegister(psState);
	IMG_UINT32	uDPFullResult = GetNextRegister(psState);

	/*
		Calculate (SRC0X * SRC1X + SRC0Z * SRC1Z, SRC0Y * SRC1Y + SRC0W * SRC0W)
	*/
	for (uHalf = 0; uHalf < 2; uHalf++)
	{
		PINST	psMadInst;

		psMadInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psMadInst, (uHalf == 0) ? IFMUL16 : IFMAD16);
		psMadInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMadInst->asDest[0].uNumber = uDPHalfResults;
		psMadInst->asDest[0].eFmt = UF_REGFORMAT_F16;
		GetDoubleSourceF16(psState, 
						   psCodeBlock, 
						   &psSrc->asSrc[0], 
						   0, 
						   &psMadInst->asArg[0], 
						   &psMadInst->u.psArith16->aeSwizzle[0], 
						   uHalf, 
						   IMG_TRUE,
						   &psMadInst->u.psArith16->sFloat.asSrcMod[0],
						   3);
		GetDoubleSourceF16(psState, 
						   psCodeBlock, 
						   &psSrc->asSrc[1], 
						   1, 
						   &psMadInst->asArg[1], 
						   &psMadInst->u.psArith16->aeSwizzle[1], 
						   uHalf, 
						   IMG_TRUE, 
						   &psMadInst->u.psArith16->sFloat.asSrcMod[1],
						   3);

		if (uHalf == 1)
		{
			psMadInst->asArg[2] = psMadInst->asDest[0];
		}
		AppendInst(psState, psCodeBlock, psMadInst);
	}

	/*
		Add the two partial sums.
	*/
	psAddInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psAddInst, IFADD);
	psAddInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psAddInst->asDest[0].uNumber = uDPFullResult;
	psAddInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psAddInst->asArg[0].uNumber = uDPHalfResults;
	psAddInst->asArg[0].eFmt = UF_REGFORMAT_F16;
	psAddInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
	psAddInst->asArg[1].uNumber = uDPHalfResults;
	SetComponentSelect(psState, psAddInst, 1 /* uArgIdx */, 2 /* uComponent */);
	psAddInst->asArg[1].eFmt = UF_REGFORMAT_F16;
	AppendInst(psState, psCodeBlock, psAddInst);

	/* Do any destination modifiers. */
	GenerateDestModF32(psState, psCodeBlock, &psAddInst->asDest[0], uSat, uScale, 
					   USC_PREDREG_NONE, IMG_FALSE);

	/*
		Convert the result back to F16 format and broadcast to all
		channels of the destination.
	*/
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		if (psSrc->sDest.u.byMask & (1U << uChan))
		{
			PINST		psPackInst;

			psPackInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psPackInst, IPCKF16F32);
			psPackInst->auDestMask[0] = g_puChanToMask[uChan];
			GetDestinationF16(psState, &psSrc->sDest, &psPackInst->asDest[0], uChan);
			if ((psSrc->uPredicate & UF_PRED_COMP_MASK) != UF_PRED_NONE)
			{
				GetInputPredicateInst(psState, psPackInst, psSrc->uPredicate, uChan);
			}
			psPackInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psPackInst->asArg[0].uNumber = uDPFullResult;
			psPackInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psPackInst->asArg[1].uNumber = 0;
			AppendInst(psState, psCodeBlock, psPackInst);
		}
	}

	/* Store into an indexable temporary register. */
	if (psSrc->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psCodeBlock, &psSrc->sDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);
	}
	else if (psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT)
	{
		ConvertDestinationF16(psState, psCodeBlock, &psSrc->sDest);
	}
}


#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
/*****************************************************************************
 FUNCTION	: SwizzleToMask
    
 PURPOSE	: Get the channel mask referred to by an input swizzle.
`
 PARAMETERS	: uSwizzle     - Input swizzle
              uComp        - Component of the swizzle to decode.
			  
 RETURNS	: Mask of swizzled component
*****************************************************************************/
static
IMG_UINT32 SwizzleToMask(IMG_UINT32 uSwizzle, IMG_UINT32 uComp)
{
	IMG_UINT32 uSrcChan = EXTRACT_CHAN(uSwizzle, uComp);
	if (uSrcChan < CHANNELS_PER_INPUT_REGISTER)
		return (1U << uSrcChan);
	else
		return 0;
}

/*****************************************************************************
 FUNCTION	: EmitNormaliseF16
    
 PURPOSE	: Emit an F16 normalise instruction to the intermediate format.
`
 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block to add the intermediate instructions to.
              psDest        - Destination register
              apsOperand    - Source registers
              uDestMask     - Destination channels to update
              bVec4         - Whether a vec4 (true) or vec3 (false)
              uInputPred    - The predicate on the input instruction
			  
 RETURNS	: None.
*****************************************************************************/
IMG_INTERNAL
IMG_VOID EmitNormaliseF16(PINTERMEDIATE_STATE psState, 
						  PCODEBLOCK psCodeBlock, 
						  PARG psDest,
 						  ARG asOperand[],
						  IMG_UINT32 uSrcMask,
						  IMG_BOOL bVec4,
						  IMG_UINT32 uInputPred,
						  IMG_BOOL bSrcModNegate,
						  IMG_BOOL bSrcModAbsolute)
{
	const IMG_UINT32 uNumSrcRegs = 2;
	IMG_UINT32 uNumComps;
	PINST psInst = NULL;
	IMG_UINT32 uDrc = 0;
	IMG_UINT32 uCtr;

	ASSERT(psState->psTargetFeatures->ui32Flags & 
		   SGX_FEATURE_FLAGS_USE_FNORMALISE);

	/* Set the number of components */
	uNumComps = bVec4 ? 4: 3;

	/* 
	   ufop_nrm dst, src
	   --> 
	   fnrm dst, src.xy, src.yz, swizzle drc
	*/

	/* Set up the normalise */
	psInst = AllocateInst(psState, IMG_NULL);
	SetOpcodeAndDestCount(psState, psInst, IFNRMF16, 2 /* uDestCount */);

	/* Set up the source modifiers. */
	psInst->u.psNrm->bSrcModNegate = bSrcModNegate;
	psInst->u.psNrm->bSrcModAbsolute = bSrcModAbsolute;

	/* Set up the destination */
	for (uCtr = 0; uCtr < 2; uCtr++)
	{
		psInst->asDest[uCtr] = *psDest;
		psInst->asDest[uCtr].uNumber += uCtr;
		psInst->asDest[uCtr].eFmt = UF_REGFORMAT_F16;
	}

	/* Set up the sources */
	for (uCtr = 0; uCtr < uNumSrcRegs; uCtr++)
	{
		psInst->asArg[uCtr] = asOperand[uCtr];
	}

	/* Set the swizzle */
	{
		IMG_UINT32 uSwizzle = 0;
		IMG_UINT32 auSwiz [] =
		{
			USEASM_SWIZZLE_SEL_X << (USEASM_SWIZZLE_FIELD_SIZE * 0),
			USEASM_SWIZZLE_SEL_Y << (USEASM_SWIZZLE_FIELD_SIZE * 1),
			USEASM_SWIZZLE_SEL_Z << (USEASM_SWIZZLE_FIELD_SIZE * 2),
			USEASM_SWIZZLE_SEL_W << (USEASM_SWIZZLE_FIELD_SIZE * 3),
		};

		/* Calculate the swizzle, always use first three components */
		for (uCtr = 0; uCtr < 3; uCtr ++)
		{
			if (uSrcMask & (1U << uCtr))
				uSwizzle |= auSwiz[uCtr];
			else
				uSwizzle |= USEASM_SWIZZLE_SEL_0 << (USEASM_SWIZZLE_FIELD_SIZE * uCtr);
		}

		if ((uSrcMask & (1U << 3)) && bVec4)
			/* Use all components on vec4 normalise */
			uSwizzle |= auSwiz[3];
		else
			uSwizzle |= USEASM_SWIZZLE_SEL_0 << (USEASM_SWIZZLE_FIELD_SIZE * 3);


		psInst->asArg[2].uType = USEASM_REGTYPE_SWIZZLE;
		psInst->asArg[2].uNumber = uSwizzle;
	}

	/* Set the predicate */
	GetInputPredicateInst(psState, psInst, uInputPred, 0);

	/* Set the drc register */
	psInst->asArg[NRM_DRC_SOURCE].uType = USEASM_REGTYPE_DRC;
	psInst->asArg[NRM_DRC_SOURCE].uNumber = uDrc;

	/* Add instruction to codeblock */
	AppendInst(psState, psCodeBlock, psInst);
}

/*****************************************************************************
 FUNCTION	: ConvertNormaliseF16
    
 PURPOSE	: Convert a normalise instruction to the intermediate format.
`
 PARAMETERS	: psState		- Compiler state.
			  psCodeBlock	- Block to add the intermediate instructions to.
			  psSrc			- Instruction to convert.
              bVec4         - Whether a vec4 (true) or vec3 (false)
			  
 RETURNS	: None.
*****************************************************************************/
static
IMG_VOID ConvertNormaliseF16(PINTERMEDIATE_STATE psState, 
							 PCODEBLOCK psCodeBlock, 
							 PUNIFLEX_INST psSrc,
							 IMG_BOOL bVec4)
{
	const IMG_UINT32 uNumChans = CHANNELS_PER_INPUT_REGISTER;
	const IMG_UINT32 uChansPerReg = 2;
	IMG_UINT32 uNumComps;
	IMG_UINT32 uDestMask, uSrcMask;
	PINST psInst;
	ARG sDest, sResult;
	ARG asArg[2];
	ARG sTempReg;
	IMG_UINT32 uCtr;
	IMG_UINT32 uSwizzle;
	IMG_UINT32 uTempSrcNum = USC_TEMPREG_TEMPSRC;
	const IMG_UINT32 uTempRegNum = USC_TEMPREG_TEMPDEST;
	IMG_BOOL bSrcModNegate;
	IMG_BOOL bSrcModAbsolute;

	ASSERT(psState->psTargetFeatures->ui32Flags & 
		   SGX_FEATURE_FLAGS_USE_FNORMALISE);

	/* Set the number of components */
	uNumComps = bVec4 ? 4: 3;

	/* 
	   ufop_nrm dst, src
	   --> 
	   fnrm dst, src.xy, src.yz, swizzle drc
	*/

	/* Get the sources */
	uSwizzle = psSrc->asSrc[0].u.uSwiz;
	uSrcMask = 0;

	for (uCtr = 0; uCtr < 2; uCtr ++)
	{
		InitInstArg(&asArg[uCtr]);

		asArg[uCtr].uType = USEASM_REGTYPE_TEMP;
		asArg[uCtr].uNumber = uTempSrcNum + uCtr + 1;
		asArg[uCtr].eFmt = UF_REGFORMAT_F16;
	}

	/*
		Get the source modifiers.
	*/
	bSrcModNegate = IMG_FALSE;
	if (psSrc->asSrc[0].byMod & UFREG_SOURCE_NEGATE)
	{
		bSrcModNegate = IMG_TRUE;
	}
	bSrcModAbsolute = IMG_FALSE;
	if (psSrc->asSrc[0].byMod & UFREG_SOURCE_ABS)
	{
		bSrcModAbsolute = IMG_TRUE;
	}

	for (uCtr = 0; uCtr < CHANNELS_PER_INPUT_REGISTER; uCtr ++)
	{
		IMG_UINT32 uReg = uCtr / uChansPerReg;
		IMG_UINT32 uSrcChan = EXTRACT_CHAN(uSwizzle, uCtr);
		uSrcChan = uCtr;

		if (uSrcChan < CHANNELS_PER_INPUT_REGISTER)
		{
			IMG_UINT32 uHalf = (uCtr % uChansPerReg) * 2;
			IMG_UINT32 uComponent;

			GetF16Source(psState, 
						 psCodeBlock, 
						 &psSrc->asSrc[0], 
						 &sTempReg,
						 &uComponent,
						 uSrcChan,
						 IMG_FALSE, 
						 NULL,
						 IMG_FALSE);
			uSrcMask |= (1U << uSrcChan);
			
			/* Pack the channel to the source for the normalise instruction */
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IPCKF16F16);
			psInst->auDestMask[0] = ((1U << (uHalf + 1)) | (1U << uHalf));

			psInst->asDest[0] = asArg[uReg];
			psInst->asDest[0].eFmt = UF_REGFORMAT_F16;

			psInst->asArg[0] = sTempReg;
			psInst->asArg[0].eFmt = UF_REGFORMAT_F16;
			SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uComponent);

			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 0;
			psInst->asArg[1].eFmt = UF_REGFORMAT_F16;

			AppendInst(psState, psCodeBlock, psInst);
		}
	}

	/* Calculate the destination and source masks */
	uDestMask = 0; 
	for (uCtr = 0; uCtr < uNumComps; uCtr ++)
	{
		uSrcMask |= SwizzleToMask(uSwizzle, uCtr);
	}

	InitInstArg(&sResult);
	sResult.uType = USEASM_REGTYPE_TEMP;
	sResult.uNumber = uTempRegNum;

	EmitNormaliseF16(psState, 
					 psCodeBlock, 
					 &sResult, 
					 asArg,
					 uSrcMask,
					 bVec4, 
					 psSrc->uPredicate,
					 bSrcModNegate,
					 bSrcModAbsolute);

	/* Get the result */
	GetDestinationF16(psState, &psSrc->sDest, &sDest, 0);

	/* Move the result to the real destination */
	uDestMask = psSrc->sDest.u.byMask;
	for (uCtr = 0; uCtr < uNumComps; uCtr ++)
	{
		IMG_UINT32 uReg = uCtr / uChansPerReg;
		IMG_UINT32 uChan = (uCtr % uChansPerReg) * uChansPerReg;
		
		if (!(uDestMask & (1U << uCtr)))
			continue;

		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IPCKF16F16);
		psInst->auDestMask[0] = (3 << uChan);

		psInst->asDest[0] = sDest;
		psInst->asDest[0].uNumber += uReg;

		/* Src0 = sResult */
		psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asArg[0].uNumber = uTempRegNum + uReg;
		psInst->asArg[0].eFmt = UF_REGFORMAT_F16;
		SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uChan);

		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
		psInst->asArg[1].eFmt = UF_REGFORMAT_F16;

		/* Set the predicate */
		GetInputPredicateInst(psState, psInst, psSrc->uPredicate, uChan);

		AppendInst(psState, psCodeBlock, psInst);
	}
	/* Replicate the last channel for all missing source components */
	for ( /* skip */ ; uCtr < uNumChans; uCtr ++)
	{
		IMG_UINT32 uLastChan = (uNumComps - 1);
		IMG_UINT32 uReg =  uLastChan / uChansPerReg;
		IMG_UINT32 uChan = (uLastChan % uChansPerReg) * uChansPerReg;
		
		if (!(uDestMask & (1U << uCtr)))
			continue;

		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psInst, IPCKF16F16);
		psInst->auDestMask[0] = (3 << ((uCtr % uChansPerReg) * uChansPerReg));

		psInst->asDest[0] = sDest;
		psInst->asDest[0].uNumber += uReg;

		/* Src0 = sResult */
		psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asArg[0].uNumber = uTempRegNum + uReg;
		psInst->asArg[0].eFmt = UF_REGFORMAT_F16;
		SetPCKComponent(psState, psInst, 0 /* uArgIdx */, uChan);

		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
		psInst->asArg[1].eFmt = UF_REGFORMAT_F16;

		/* Set the predicate */
		GetInputPredicateInst(psState, psInst, psSrc->uPredicate, uChan);

		AppendInst(psState, psCodeBlock, psInst);
	}

	/* Store into an indexable temporary register. */
	if (psSrc->sDest.eType == UFREG_TYPE_INDEXABLETEMP)
	{
		StoreIndexableTemp(psState, psCodeBlock, &psSrc->sDest, UF_REGFORMAT_F16, USC_TEMPREG_F16INDEXTEMPDEST);
	}
	else if (psSrc->sDest.eType == UFREG_TYPE_VSOUTPUT || psSrc->sDest.eType == UFREG_TYPE_PSOUTPUT)
	{
		ConvertDestinationF16(psState, psCodeBlock, &psSrc->sDest);
	}
}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

IMG_INTERNAL 
PCODEBLOCK ConvertInstToIntermediateF16(PINTERMEDIATE_STATE psState, 
									  PCODEBLOCK psCodeBlock, 
									  PUNIFLEX_INST psSrc, 
									  IMG_BOOL bConditional, 
									  IMG_BOOL bStaticCond)
/*****************************************************************************
 FUNCTION	: ConvertInstToIntermediateF16
    
 PURPOSE	: Convert an input instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  bConditional		- Whether instruction appears in a conditional
			  bStaticCond		- Whether conditional instruction appears in is static.
			  
 RETURNS	: None.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		psCodeBlock = ConvertInstToIntermediateF16_Vec(	psState,
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
		case UFOP_LDPIFTC:
		case UFOP_LDC:
		case UFOP_LDCLZ:
		case UFOP_LD2DMS:
		case UFOP_LDGATHER4:
		{
			psCodeBlock = CHOOSE_USPBIN_OR_USCHW_FUNC(ConvertSamplerInstructionF16, 
													  (psState, psCodeBlock, psSrc, bConditional && !bStaticCond));
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
				ConvertLoadTiledInstructionF16(psState, psCodeBlock, psSrc);
			}
			#endif /* defined(OUTPUT_USCHW) */

			break;
		}

		/* Scalar instructions. */
		case UFOP_RECIP:
		case UFOP_RSQRT:
		case UFOP_EXP:
		case UFOP_LOG:
		case UFOP_SQRT:
		{
			ConvertScalarInstructionF16(psState, psCodeBlock, psSrc);
			break;
		}

		case UFOP_ADD:
		case UFOP_MUL:
		case UFOP_MAD:
		{
			ConvertMadInstructionF16(psState, psCodeBlock, psSrc);
			break;
		}

		case UFOP_MIN:
		case UFOP_MAX:
		case UFOP_FRC:
		case UFOP_DSX:
		case UFOP_DSY:
		{
			ConvertF32OnlyInstructionF16(psState, psCodeBlock, psSrc);
			break;
		}

		case UFOP_MOV:
		{
			if (
					GetRegisterFormat(psState, &psSrc->asSrc[0]) == UF_REGFORMAT_F16 &&
					(psSrc->asSrc[0].byMod & (UFREG_SOURCE_NEGATE | UFREG_SOURCE_ABS)) != 0
			   )
			{
				ConvertMadInstructionF16(psState, psCodeBlock, psSrc);
			}
			else
			{
				ConvertMoveInstructionF16(psState, psCodeBlock, psSrc);
			}
			break;
		}

		/* Dot products. */
		case UFOP_DOT2ADD:
		case UFOP_DOT3:
		{
			ConvertDotProductInstructionF16(psState, psCodeBlock, psSrc);
			break;
		}
		case UFOP_DOT4:
		case UFOP_DOT4_CP:
		{
			ConvertDot4InstructionF16(psState, psCodeBlock, psSrc);
			break;
		}

		/* Vector comparison instructions. */
		case UFOP_CMP:
		case UFOP_CMPLT:
		{
			ConvertComparisonInstructionF16(psState, psCodeBlock, psSrc);
			break;
		}
		case UFOP_SETP:
		{
			ConvertSetpInstructionNonC10(psState, psCodeBlock, psSrc);
			break;
		}

		/* LIT/EXPP/LOGP/DST are invalid at low precision */
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
			ConvertMovaInstructionFloat(psState, psCodeBlock, psSrc, IMG_FALSE);
			break;
		}

		/* Kill instruction. */
		case UFOP_KPLT:
		case UFOP_KPLE:
		{
			ConvertTexkillInstructionFloat(psState, psCodeBlock, psSrc, bConditional, IMG_FALSE);
			break;
		}

		#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
		case UFOP_NRM:
		{
			ConvertNormaliseF16(psState, psCodeBlock, psSrc, IMG_FALSE);
			break;
		}
		#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */

		/* Macro instruction. */
		case UFOP_SINCOS: 
		{
			#if defined(SUPPORT_SGX545)
			if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_SQRT_SIN_COS)
			{
				ConvertSincosF16HWSupport(psState, psCodeBlock, psSrc);
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
				ConvertSincos2F16HWSupport(psState, psCodeBlock, psSrc);
			}
			else
			#endif /* defined(SUPPORT_SGX545) */
			{
				imgabort();
			}
			break;
		}

		case UFOP_NOP: break;

		default: imgabort();
	}
	return psCodeBlock;
}

/* EOF */
