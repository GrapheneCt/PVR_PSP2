/******************************************************************************
 * Name         : icvt_core.c
 * Title        :  Core+common utils for UNIFLEX-to-intermediate code conversion
 * Created      : July 2006
 *
 * Copyright    : 2002-2008 by Imagination Technologies Limited.
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
 * Modifications:-
 * $Log: icvt_core.c $
 *****************************************************************************/

#include "uscshrd.h"
#include "usc_texstate.h"
#include "bitops.h"
#include "execpred.h"
#include <limits.h>

/* State used during CFG construction / input-to-intermediate code conversion */
typedef struct
{
	/* Max depth of call found so far within function currently being processed. */
	IMG_UINT32 uNestingLevel;
	/* Number of input labels encountered */
	IMG_UINT32				uNumLabels;
	/* The input labels, and their corresponding PFUNCs */
	IMG_PUINT32				puInputLabels;
	PFUNC					*ppsFuncs;
	
	/* The input program */
	INPUT_PROGRAM sProg;
} CALLDATA, *PCALLDATA;

#if defined(OUTPUT_USPBIN)
static 
PCODEBLOCK ConvertTextureWriteToIntermediate(PINTERMEDIATE_STATE psState, 
										   PCODEBLOCK psCodeBlock, 
										   PUNIFLEX_INST psUFInst);
#endif /* defined(OUTPUT_USPBIN) */

#if defined(OUTPUT_USCHW)
/* Relates the format of a texture to the size in bits. */
static const IMG_UINT32 g_puChannelFormBitCount[] =
{
	0, /* USC_CHANNELFORM_INVALID */
	8, /* USC_CHANNELFORM_U8 */
	8, /* USC_CHANNELFORM_S8 */
	16, /* USC_CHANNELFORM_U16 */
	16, /* USC_CHANNELFORM_S16 */
	16, /* USC_CHANNELFORM_F16 */
	32, /* USC_CHANNELFORM_F32 */
	0, /* USC_CHANNELFORM_ZERO */
	0, /* USC_CHANNELFORM_ONE */
	0, /* USC_CHANNELFORM_U24 */
	8, /* USC_CHANNELFORM_U8_UN */
	10, /* USC_CHANNELFORM_C10 */
	8,	/* USC_CHANNELFORM_UINT8 */
	16,	/* USC_CHANNELFORM_UINT16 */
	32,	/* USC_CHANNELFORM_UINT32 */
	8,	/* USC_CHANNELFORM_SINT8 */
	16,	/* USC_CHANNELFORM_SINT16 */
	32,	/* USC_CHANNELFORM_SINT32 */
	16, /* USC_CHANNELFORM_UINT16_UINT10 */
	16, /* USC_CHANNELFORM_UINT16_UINT2 */
	32, /* USC_CHANNELFORM_DF32 */
};
#endif /* defined(OUTPUT_USCHW) */

static
IMG_BOOL CheckForStaticConstOffset(PINTERMEDIATE_STATE	psState,
								   IMG_UINT32			uOffset,
								   IMG_UINT32			uBufferIdx)
/*****************************************************************************
 FUNCTION	: CheckForStaticConst

 PURPOSE	: Check if a particular offset into a constant buffer has a compile-time
			  known value.

 PARAMETERS	: psState			- Compiler state.
			  uOffset			- Without UF_CONSTEXPLICTADDRESSMODE: the offset
								in the constant buffer in channels.
								  With UF_CONSTEXPLICTADDRESSMODE: the offset
							    in the consant buffer in dwords.
			  uBufferIdx		- Constant buffer the constant is in.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		For multiple constant buffers only one constant buffer
		contains constants with static values.
	*/
	if (psState->uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS)
	{
		if (uBufferIdx != psState->uStaticConstsBuffer)
		{
			return IMG_FALSE;
		}
	}

	if (uOffset >= psState->psConstants->uCount)
	{
		return IMG_FALSE;
	}
	if (!GetBit(psState->psConstants->puConstStaticFlags, uOffset))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL GetStaticConstByteOffset(PINTERMEDIATE_STATE	psState, 
								  IMG_UINT32			uOffsetInBytes, 
								  IMG_UINT32			uLengthInBytes, 
								  IMG_UINT32			uBufferIdx,
								  IMG_PUINT32			puValue)
/*****************************************************************************
 FUNCTION	: GetStaticConstByteOffset

 PURPOSE	: Get the compile-time known value of a range of bytes in a constant
			  buffer.

 PARAMETERS	: psState			- Compiler state.
			  uOffsetInBytes	- Start of the range of bytes.
			  uLengthInBytes	- Length of the range (at most LONG_SIZE).
			  uBufferIdx		- Constant buffer the constant is in.
			  puValue			- On success returns the value of the range.

 RETURNS	: TRUE if the range has a compile-time known value; FALSE otherwise.
*****************************************************************************/
{
	IMG_UINT32	uRangeStartInBits;
	IMG_UINT32	uRangeStartInDwords;
	IMG_UINT32	uRangeEndInBits;
	IMG_UINT32	uRangeEndInDwords;
	IMG_UINT32	uLengthInBits;
	
	ASSERT((psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) != 0);

	/*
		Check if the first dword overlapping the range has a static value.
	*/
	uRangeStartInBits = uOffsetInBytes * BITS_PER_BYTE;
	uRangeStartInDwords = uRangeStartInBits / BITS_PER_UINT;
	if (!CheckForStaticConstOffset(psState, uRangeStartInDwords, uBufferIdx))
	{
		return IMG_FALSE;
	}

	/*
		Check if the second dword overlapping the range has a static value.
	*/
	uLengthInBits = uLengthInBytes * BITS_PER_BYTE;
	uRangeEndInBits = uRangeStartInBits + uLengthInBits - 1;
	uRangeEndInDwords = uRangeEndInBits / BITS_PER_UINT;
	if (uRangeEndInDwords != uRangeStartInDwords && !CheckForStaticConstOffset(psState, uRangeEndInDwords, uBufferIdx))
	{
		return IMG_FALSE;
	}

	*puValue = GetRange((IMG_PUINT32)psState->psConstants->pfConst, uRangeEndInBits, uRangeStartInBits);
	return IMG_TRUE;
}

IMG_INTERNAL
IMG_BOOL CheckForStaticConst(PINTERMEDIATE_STATE	psState,
							 IMG_UINT32				uNum,
							 IMG_UINT32				uChan,
							 IMG_UINT32				uBufferIdx,
							 IMG_PUINT32			puValue)
/*****************************************************************************
 FUNCTION	: CheckForStaticConst

 PURPOSE	: Check if a constant has a value known at compile time and if so
		      return the known value.

 PARAMETERS	: psState			- Compiler state.
			  uNum				- Constant register number.
			  uChan				- Constant register channel.
			  uBufferIdx		- Constant buffer the constant is in.
			  puValue			- On success returns the static value of the
								constant.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	IMG_UINT32	uOffset;

	if ((psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE) != 0)
	{
		return IMG_FALSE;
	}

	uOffset = uNum * CHANNELS_PER_INPUT_REGISTER + uChan;
	if (CheckForStaticConstOffset(psState, uOffset, uBufferIdx))
	{
		*puValue = *(IMG_PUINT32)&psState->psConstants->pfConst[uOffset];
		return IMG_TRUE;
	}
	else
	{
		return IMG_FALSE;
	}
}

static IMG_UINT32 CheckForHwConst(PINTERMEDIATE_STATE	psState, 
								  IMG_UINT32			uNum, 
								  IMG_UINT32			uChan, 
								  IMG_UINT32			uBufferIdx,
								  IMG_BOOL				bAllowSourceMod, 
								  IMG_PBOOL				pbNegate)
/*****************************************************************************
 FUNCTION	: CheckForHwConst

 PURPOSE	: Check if a constant is equal to a number directly available in hardware.

 PARAMETERS	: psState			- Compiler state.
			  uNum				- Constant register number.
			  dwChan			- Constant register channel.

 RETURNS	: The number of the corresponding hardware constant or -1 if none match.
*****************************************************************************/
{
	IMG_FLOAT	fValue;

	if (!CheckForStaticConst(psState, uNum, uChan, uBufferIdx, (IMG_PUINT32)&fValue))
	{
		return UINT_MAX;
	}

	if (fValue < 0 && bAllowSourceMod)
	{
		IMG_UINT32	uResult;

		uResult = FloatToCstIndex(-fValue);
		if (uResult != UINT_MAX)
		{
			*pbNegate = IMG_TRUE;
		}
		return uResult;
	}
	else
	{
		if (psState->uCompilerFlags & UF_OPENCL)
		{
			union { IMG_FLOAT fValue; IMG_UINT32 uValue; } u;
			u.fValue = fValue;

			return FindHardwareConstant (psState, u.uValue);
		}
		return FloatToCstIndex(fValue);
	}
}

static IMG_VOID ConvertRelativeOffset(PINTERMEDIATE_STATE		psState,
									  UFREG_RELATIVEINDEX		eRelativeIndex,
									  IMG_PUINT32				puIndexTempNum)
/*****************************************************************************
 FUNCTION	: ConvertRelativeOffset

 PURPOSE	: Convert a address register index to the temporary registers to
			  use for indexing in the hardware.

 PARAMETERS	: psState			- Compiler state.
			  eRelativeIndex	- Address register.
			  puIndexTempNum	- Returns the number of the register to use
								for indexing.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (eRelativeIndex == UFREG_RELATIVEINDEX_NONE)
	{
		*puIndexTempNum = UINT_MAX;
	}
	else if (eRelativeIndex == UFREG_RELATIVEINDEX_AL)
	{	
		ASSERT(psState->uD3DLoopIndex != (IMG_UINT32)-1);
		*puIndexTempNum = psState->uD3DLoopIndex;
	}
	else
	{
		IMG_UINT32	uAddressRegisterIndex = (IMG_UINT32)(eRelativeIndex - UFREG_RELATIVEINDEX_A0X);

		*puIndexTempNum = USC_TEMPREG_ADDRESS + uAddressRegisterIndex;
	}
}

static
IMG_UINT32 ConvertSourceRegisterToConstantOffset(PINTERMEDIATE_STATE	psState,
												 PUF_REGISTER			psSource,
												 IMG_UINT32				uSrcChan,
												 UF_REGFORMAT			eFormat)
/*****************************************************************************
 FUNCTION	: ConvertSourceRegisterToConstantOffset

 PURPOSE	: Convert a constant source argument to an offset for the
			  intermediate load memory constant instruction.

 PARAMETERS	: psState			- Compiler state.
			  psSource			- Input source argument.
			  uSrcChan			- Source channel to access from the source
								argument.
			  eFormat			- Format of the source argument. 

 RETURNS	: The offset.
*****************************************************************************/
{
	IMG_UINT32	uOffset;

	if (psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE)
	{
		/*
			The source register represents a vec4 of uniforms in a particular format
			starting at a byte offset in the constant buffer. We want to load a dwords
			worth of channels starting at a channel in the vector corresponding
			to a dword aligned offset.
		*/
		switch (eFormat)
		{
			case UF_REGFORMAT_F16:
			{
				/*
					Two channels per dword. For the F16 format we can be given a starting
					channel which doesn't correspond to a dword aligned offset - in that case
					only one channel is wanted by the caller and we load the whole dword containing
					the channel then use the component select on the source to select the right
					word within the dword.
				*/
				uOffset = psSource->uNum + (uSrcChan >> 1) * LONG_SIZE;
				break;
			}
			case UF_REGFORMAT_C10:
			{
				/*
					The vector is split across two dwords: the first contains the
					XYZ channels and the second the W channel.
				*/
				if (uSrcChan == 0)
				{
					uOffset = psSource->uNum;
				}
				else
				{
					ASSERT(uSrcChan == 3);
					uOffset = psSource->uNum + LONG_SIZE;
				}
				break;
			}
			case UF_REGFORMAT_U8:
			{
				/* Four channels per dword. */
				ASSERT(uSrcChan == 0);
				uOffset = psSource->uNum;
				break;
			}
			default:
			{
				/* One channel per dword. */
				uOffset = psSource->uNum + uSrcChan * LONG_SIZE;
				break;
			}
		}
	}
	else
	{
		/*
			 Convert to an offset in terms of channels but with an appropriate alignment
			 given how many constants of this format fit into a dword. 
		*/
		switch (eFormat)
		{
			case UF_REGFORMAT_F16:
			{
				uOffset = psSource->uNum * CHANNELS_PER_INPUT_REGISTER + (uSrcChan & ~1U);
				break;
			}
			case UF_REGFORMAT_C10:
			{
				ASSERT(uSrcChan == 0);
				uOffset = psSource->uNum * CHANNELS_PER_INPUT_REGISTER;
				break;
			}
			default:
			{
				uOffset = psSource->uNum * CHANNELS_PER_INPUT_REGISTER + uSrcChan;
				break;
			}
		}
	}

	return uOffset;
}

IMG_INTERNAL
IMG_UINT32 FindRange(PINTERMEDIATE_STATE psState, IMG_UINT32 uConstsBuffNum, IMG_UINT32 uConst, IMG_PUINT32 puRangeStart, IMG_PUINT32 puRangeEnd)
/*********************************************************************************
 Function			: FindRange

 Description		: Finds the range containing a constant.

 Parameters			: psState			- Current compiler state.
					  uConstsBuffNum	- Constants buffer to which constant belongs.
					  uConst			- Constant to find a range for.
					  puRangeStart		- Returns the start of the range.
					  puRangeEnd		- Returns the inclusive end of the range.

 Globals Effected	: None

 Return				: The range number.
*********************************************************************************/
{
	IMG_UINT32				uRange;
	PUNIFLEX_RANGES_LIST	psRangeList = &psState->psSAOffsets->asConstBuffDesc[uConstsBuffNum].sConstsBuffRanges;

	if ((psState->uCompilerFlags & UF_CONSTRANGES) == 0)
	{
		return UINT_MAX;
	}

	for (uRange = 0; uRange < psRangeList->uRangesCount; uRange++)
	{
		IMG_UINT32 uRangeStart, uRangeEnd;
	
		uRangeStart = psRangeList->psRanges[uRange].uRangeStart;
		uRangeEnd = psRangeList->psRanges[uRange].uRangeEnd;

		if (uRangeStart <= uConst && uRangeEnd > uConst)
		{
			if (puRangeStart != NULL) 
			{	
				if (psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE)
				{
					*puRangeStart = uRangeStart;
					*puRangeEnd = uRangeEnd - 1;
				}
				else
				{
					/*
						Convert the range start and end to be in terms of components.
					*/
					*puRangeStart = uRangeStart << 2;
					*puRangeEnd = ((uRangeEnd - 1) << 2) + 3;
				}
			}
			return uRange;
		}
	}
	return UINT_MAX;
}

static
IMG_INT32 CompareRange(PCONSTANT_RANGE psRange, IMG_UINT32 uBufferIdx, IMG_UINT32 uRangeIdx, IMG_BOOL bC10SubcomponentIndex)
/*********************************************************************************
 Function			: CompareRange

 Description		: Compare the parameters for two constant ranges.

 Parameters			: psRange		- First range to compare.
					  uBufferIdx	- Constant buffer for the second range.
					  uRangeIdx		- Range identifier for the second range.
					  bC10SubcomponentIndex
									- TRUE if the second range is used with
									subcomponent indexing into C10 vectors.

 Globals Effected	: None

 Return				: <0 if the first range is less than the second range.
					   0 if the two ranges are equal.
				      >0 if the second range is less than the first range.
*********************************************************************************/
{
	if (psRange->uBufferIdx == uBufferIdx)
	{
		if (psRange->uRangeIdx == uRangeIdx)
		{
			if (psRange->bC10SubcomponentIndex == bC10SubcomponentIndex)
			{
				return 0;
			}
			else
			{
				return psRange->bC10SubcomponentIndex - bC10SubcomponentIndex;
			}
		}
		else
		{
			return (IMG_INT32)(psRange->uRangeIdx - uRangeIdx);
		}
	}
	else
	{
		return (IMG_INT32)(psRange->uBufferIdx - uBufferIdx);
	}
}

static
IMG_INT32 SortByAddress(PUSC_LIST_ENTRY psListEntry1, PUSC_LIST_ENTRY psListEntry2)
/*****************************************************************************
 FUNCTION	: SortByAddress

 PURPOSE	: Helper function to sort the list of constants loaded from a particular
			  buffer by address.

 PARAMETERS	: psListEntry1			- Elements to compare.
			  psListEntry2

 RETURNS	: <0 if the first element is less than the second
			   0 if the two elements are equal.
		      >0 if the first element is greater than the second.
*****************************************************************************/
{
	PLOADCONST_PARAMS	psElement1 = IMG_CONTAINING_RECORD(psListEntry1, PLOADCONST_PARAMS, sBufferListEntry);
	PLOADCONST_PARAMS	psElement2 = IMG_CONTAINING_RECORD(psListEntry2, PLOADCONST_PARAMS, sBufferListEntry);
	PINST				psInst1 = psElement1->psInst;
	PINST				psInst2 = psElement2->psInst;
	IMG_UINT32			uOffset1 = psInst1->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uNumber;
	IMG_UINT32			uOffset2 = psInst2->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX].uNumber;

	return (IMG_INT32)(uOffset1 - uOffset2);
}

IMG_INTERNAL 
IMG_VOID LoadConstantNoHWReg(PINTERMEDIATE_STATE	psState,
							 PCODEBLOCK				psCodeBlock,
							 PINST					psInsertBeforeInst,
							 PUF_REGISTER			psSource,
							 IMG_UINT32				uSrcChan,
							 PARG					psHwSource,
							 IMG_PUINT32			puComponent,
							 UF_REGFORMAT			eFormat,
							 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
							 IMG_BOOL				bVectorResult,
							 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
							 IMG_BOOL				bC10SubcomponentIndex,
							 IMG_UINT32				uC10CompOffset)
/*****************************************************************************
 FUNCTION	: LoadConstantNoHWReg

 PURPOSE	: Generate code to load constant that is not available in a hardware register

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block where the instructions to load the constant should be added.
			  psInsertBeforeInst
								- Locatiotion to insert the instructions at.
			  psSource			- Input source.
			  uSrcChan			- Source channel.
			  psHwSource		- USE register to which the source modifier should be applied.
			  eFormat
			  bVectorResult		- If TRUE load a complete vector.
			  uC10CompOffset

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32					uConstOffset;
	PINST						psLoadInst;
	IMG_UINT32					uTemp = GetNextRegister(psState);
	IMG_UINT32					uStaticValue;
	UNIFLEX_CONST_FORMAT		eConstFormat;
	IMG_UINT32					uConstRange;
	IMG_UINT32					uRangeStart;
	IMG_UINT32					uRangeEnd;
	PARG						psStaticOffsetArg;
	PARG						psDynOffsetArg;
	IMG_UINT32					uBufferIdx;
	PLOADCONST_PARAMS			psLoadConst;

	uConstOffset = ConvertSourceRegisterToConstantOffset(psState, psSource, uSrcChan, eFormat);

	/*
		Map the register format to a constant format.
	*/
	switch (eFormat)
	{
		case UF_REGFORMAT_F32: eConstFormat = UNIFLEX_CONST_FORMAT_F32; break;
		case UF_REGFORMAT_F16: eConstFormat = UNIFLEX_CONST_FORMAT_F16; break;
		case UF_REGFORMAT_C10: eConstFormat = UNIFLEX_CONST_FORMAT_C10; break;
		case UF_REGFORMAT_U8: eConstFormat = UNIFLEX_CONST_FORMAT_U8; break;
		default: imgabort();
	}	

	psLoadInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psLoadInst, ILOADCONST);
	psLoadConst = psLoadInst->u.psLoadConst;
	
	/*
		Store a backpointer from the constant load parameters to the instruction itself.
	*/
	psLoadConst->psInst = psLoadInst;

	/*
		Set the constant load destination.
	*/
	SetDest(psState, psLoadInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTemp, eFormat);

	/*
		Set the constant buffer this instruction loads from.
	*/
	if ((psState->uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS) != 0)
	{
		uBufferIdx = psSource->uArrayTag;
	}
	else
	{
		uBufferIdx = UF_CONSTBUFFERID_LEGACY;		
	}
	psLoadConst->uBufferIdx = uBufferIdx;

	/*
	  Set the static part of the offset into the constants.
	*/
	psStaticOffsetArg = &psLoadInst->asArg[LOADCONST_STATIC_OFFSET_ARGINDEX];
	psStaticOffsetArg->uType = USEASM_REGTYPE_IMMEDIATE;
	psStaticOffsetArg->uNumber = uConstOffset;

	/*
	  Set the dynamic part of the offset into the constants.
	*/
	psDynOffsetArg = &psLoadInst->asArg[LOADCONST_DYNAMIC_OFFSET_ARGINDEX];
	if (psSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE)
	{
		psLoadInst->u.psLoadConst->bRelativeAddress = IMG_FALSE;

		psDynOffsetArg->uType = USEASM_REGTYPE_IMMEDIATE;
		psDynOffsetArg->uNumber = 0;
	}
	else
	{
		IMG_UINT32	uIndexTempNum;

		/*
		  Translate the input relative address to a internal temporary register number.
		*/
		ConvertRelativeOffset(psState, 
							  psSource->eRelativeIndex, 
							  &uIndexTempNum);

		psLoadInst->u.psLoadConst->bRelativeAddress = IMG_TRUE;

		psDynOffsetArg->uType = USEASM_REGTYPE_TEMP;
		psDynOffsetArg->uNumber = uIndexTempNum;
	}

	/*
		Format of the data to load.
	*/
	psLoadConst->eFormat = eConstFormat;

	/*
		Store the individual channel wanted from a C10 vector.
	*/
	psLoadConst->uC10CompOffset = uC10CompOffset;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Are we loading a complete vector of this type?
	*/
	psLoadConst->bVectorResult = bVectorResult;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Units (in bytes) of the dynamic index. Applies to explict constant addressing
		only.
	*/
	if (!psLoadConst->bRelativeAddress)
	{
		psLoadConst->uRelativeStrideInBytes = USC_UNDEF;
	}
	else if (psState->uCompilerFlags & UF_CONSTEXPLICTADDRESSMODE)
	{
		psLoadConst->uRelativeStrideInBytes = psSource->u1.uRelativeStrideInBytes;
	}
	else
	{
		if (psState->uCompilerFlags & UF_GLSL)
		{
			IMG_UINT32	uRelativeStrideInComponents;

			uRelativeStrideInComponents = psSource->u1.uRelativeStrideInBytes;
			switch (psSource->eFormat)
			{
				case UF_REGFORMAT_F32:
				case UF_REGFORMAT_I32:
				case UF_REGFORMAT_U32:
				{
					psLoadConst->uRelativeStrideInBytes = uRelativeStrideInComponents * LONG_SIZE;
					break;
				}
				case UF_REGFORMAT_F16:
				case UF_REGFORMAT_I16:
				case UF_REGFORMAT_U16:
				case UF_REGFORMAT_C10:
				{
					psLoadConst->uRelativeStrideInBytes = uRelativeStrideInComponents * WORD_SIZE;
					break;
				}
				case UF_REGFORMAT_U8:
				case UF_REGFORMAT_I8_UN:
				case UF_REGFORMAT_U8_UN:
				{
					psLoadConst->uRelativeStrideInBytes = uRelativeStrideInComponents * BYTE_SIZE;
					break;
				}
				default: imgabort();
			}
		}
		else
		{
			/*
				Index values are in units of vec4 of F32s.
			*/
			ASSERT(!(psLoadConst->bRelativeAddress && (psSource->eFormat == UF_REGFORMAT_F16 || psSource->eFormat == UF_REGFORMAT_C10)));
			psLoadConst->uRelativeStrideInBytes = DQWORD_SIZE;
		}
	}

	/*
		Store the index of the any dynamically indexed constant range that this constant
		belongs to.
	*/
	uConstRange = FindRange(psState, psSource->uArrayTag, psSource->uNum, &uRangeStart, &uRangeEnd);

	if (uConstRange != USC_UNDEF)
	{
		PUSC_LIST_ENTRY	psListEntry;
		PCONSTANT_RANGE	psRange;
		IMG_BOOL		bFoundMatch;

		/*
			Check if we already created a LOADCONST instruction loading a constant in
			this range.
		*/
		bFoundMatch = IMG_FALSE;
		psRange = NULL;
		for (psListEntry = psState->sSAProg.sConstantRangeList.psHead; 
			 psListEntry != NULL; 
			 psListEntry = psListEntry->psNext)
		{
			IMG_INT32	iCmp;

			psRange = IMG_CONTAINING_RECORD(psListEntry, PCONSTANT_RANGE, sListEntry);
				
			iCmp = CompareRange(psRange, psSource->uArrayTag, uConstRange, bC10SubcomponentIndex);
			if (iCmp == 0)
			{
				bFoundMatch = IMG_TRUE;
				break;
			}
			if (iCmp > 0)
			{
				break;
			}
		}

		if (!bFoundMatch)
		{
			/*
				Add a new entry to the list of constant ranges used in the program.
			*/
			psRange = UscAlloc(psState, sizeof(*psRange));
			psRange->eFormat = psLoadInst->u.psLoadConst->eFormat;
			psRange->psData = NULL;
			psRange->uBufferIdx = psSource->uArrayTag;
			psRange->uRangeIdx = uConstRange;
			psRange->bC10SubcomponentIndex = bC10SubcomponentIndex;
			psRange->uRangeStart = uRangeStart;
			psRange->uRangeEnd = uRangeEnd;
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			psRange->bVector = bVectorResult;
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
				
			if (psListEntry == NULL)
			{
				AppendToList(&psState->sSAProg.sConstantRangeList, &psRange->sListEntry);
			}
			else
			{
				InsertInList(&psState->sSAProg.sConstantRangeList, &psRange->sListEntry, psListEntry);
			}

			psState->sSAProg.uConstantRangeCount++;
		}
		else
		{
			ASSERT(psRange->eFormat == psLoadInst->u.psLoadConst->eFormat);
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			ASSERT(psRange->bVector == bVectorResult);
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		}

		psLoadConst->psRange = psRange;
	}
	else
	{
		psLoadConst->psRange = NULL;
	}

	/*
		Flag in the LOADCONST instruction if the value accessed is static.
	*/
	psLoadConst->bStaticConst = IMG_FALSE;
	if (
			!psLoadInst->u.psLoadConst->bRelativeAddress &&
			psLoadInst->asDest[0].eFmt == UF_REGFORMAT_F32 &&
			CheckForStaticConst(psState, psSource->uNum, uSrcChan, psSource->uArrayTag, &uStaticValue)
	   )
	{
		psLoadConst->bStaticConst = IMG_TRUE;
		psLoadConst->uStaticConstValue = uStaticValue;
	}

	/*
		Add the LOADCONST instructions to the list of instructions loading from the same
		constant buffer.
	*/
	InsertInListSorted(&psState->asConstantBuffer[uBufferIdx].sLoadConstList, 
					   SortByAddress,
					   &psLoadConst->sBufferListEntry);
			
	/*
		Add the LOADCONST instruction to the basic block.
	*/
	InsertInstBefore(psState, psCodeBlock, psLoadInst, psInsertBeforeInst);

	/*
	  Set the register to use to access the constant to the result of the LD.
	*/
	*psHwSource = psLoadInst->asDest[0];
	if (eFormat == UF_REGFORMAT_F16)
	{
		IMG_UINT32	uComponent;

		uComponent = (uSrcChan & 1) << 1;

		if (puComponent != NULL)
		{
			*puComponent = uComponent;
		}
		else
		{
			ASSERT(uComponent == 0);
		}
	}
}

IMG_INTERNAL
IMG_VOID LoadConstant(PINTERMEDIATE_STATE psState,
					  PCODEBLOCK psCodeBlock,
					  PUF_REGISTER psSource,
					  IMG_UINT32 uSrcChan,
					  PARG psHwSource,
					  IMG_PUINT32 puComponent,
					  IMG_BOOL bAllowSourceMod,
					  IMG_PBOOL pbNegate,
					  UF_REGFORMAT eFormat,
					  IMG_BOOL bC10SubcomponentIndex,
					  IMG_UINT32 uCompOffset)
/*****************************************************************************
 FUNCTION	: LoadConstant

 PURPOSE	: Generate code to load constant.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block where the instructions to load the constant should be added.
			  psSource			- Input source.
			  dwSrcChan			- Source channel.
			  psHwSource		- USE register to which the source modifier should be applied.
			  dwTemp			- Destination for the modifier result.
			  bSecond			- Use the alternative set of constants

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 uConstBuffNum;	
	ASSERT(uSrcChan <= UFREG_SWIZ_W);
	if((psState->uCompilerFlags & UF_MULTIPLECONSTANTSBUFFERS) == 0)
	{
		uConstBuffNum = UF_CONSTBUFFERID_LEGACY;
	}
	else
	{
		uConstBuffNum = psSource->uArrayTag;
	}
	/* 
		For constant buffers that are needed in SAs or in memory we will not convert them with hardware contants. 
	*/
	if(psState->psSAOffsets->asConstBuffDesc[uConstBuffNum].eConstBuffLocation == UF_CONSTBUFFERLOCATION_DONTCARE)
	{

		/* Test whether we can use a hardware constant directly. */
		if (
				psSource->eRelativeIndex == UFREG_RELATIVEINDEX_NONE &&
				eFormat == UF_REGFORMAT_F32
		   )
		{
			IMG_UINT32 uValue = CheckForHwConst(psState, 
												psSource->uNum, 
												uSrcChan,
												psSource->uArrayTag,
												bAllowSourceMod, 
												pbNegate);
			
			if (uValue != UINT_MAX)
			{
				/* Found a corresponding hardware constant. */
				psHwSource->uType = USEASM_REGTYPE_FPCONSTANT;
				psHwSource->uNumber = uValue;
				psHwSource->uIndexType = USC_REGTYPE_NOINDEX;
				psHwSource->uIndexNumber = USC_UNDEF;
				psHwSource->uIndexArrayOffset = USC_UNDEF;
				return;
			}
		}
	}

	/* 
	   Not a constant which is available directly in the hardware so load
	   it into a temporary register.
	*/
	LoadConstantNoHWReg(psState, 
						psCodeBlock, 
						NULL /* psInsertBeforeInst */,
						psSource, 
						uSrcChan, 
						psHwSource, 
						puComponent,
						eFormat, 
						#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
						IMG_FALSE /* bVectorResult */,
						#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						bC10SubcomponentIndex, 
						uCompOffset);
}

IMG_INTERNAL 
IMG_VOID UnpackU24Channel(PINTERMEDIATE_STATE		psState,
						  PCODEBLOCK				psCodeBlock,
						  IMG_UINT32				uSrcRegNum,
						  IMG_UINT32				uSrcByteOffset,
						  IMG_UINT32				uDestTemp)
/*****************************************************************************
 FUNCTION	: UnpackU24Channel

 PURPOSE	: Generates code to unpack a U24 format channel.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uSrcRegType		- Register type of the data to unpack.
			  uSrcRegNum		- Register number of the data to unpack.
			  uSample			- Register offset of the channel in the data.
			  uByteOffset		- Byte offset of the channel in the register.
			  uDestTemp			- Destination for the unpacking channel.

 RETURNS	: None.
*****************************************************************************/
{
	PINST			psScaleInst;
	IMG_UINT32		uSecondWordUnpackTemp = GetNextRegister(psState);
	IMG_UINT32		uFirstByteUnpackTemp = GetNextRegister(psState);
	IMG_UINT32		uUnscaledChanTemp = GetNextRegister(psState);
	IMG_FLOAT		fU24Scale;

	/*
		Unpack the source data in two steps.
	*/
	psScaleInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psScaleInst, IUNPCKF32U16);
	psScaleInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psScaleInst->asDest[0].uNumber = uSecondWordUnpackTemp;
	psScaleInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psScaleInst->asArg[0].uNumber = uSrcRegNum;
	SetPCKComponent(psState, psScaleInst, 0 /* uArgIdx */, uSrcByteOffset + 1);
	AppendInst(psState, psCodeBlock, psScaleInst);

	psScaleInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psScaleInst, IUNPCKF32U8);
	psScaleInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psScaleInst->asDest[0].uNumber = uFirstByteUnpackTemp;
	psScaleInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psScaleInst->asArg[0].uNumber = uSrcRegNum;
	SetPCKComponent(psState, psScaleInst, 0 /* uArgIdx */, uSrcByteOffset);
	AppendInst(psState, psCodeBlock, psScaleInst);

	/*
	  Combine the two channels into a 24-bit number.
	*/
	psScaleInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psScaleInst, IFMAD);
	psScaleInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psScaleInst->asDest[0].uNumber = uUnscaledChanTemp;
	psScaleInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psScaleInst->asArg[0].uNumber = uSecondWordUnpackTemp;
	psScaleInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
	psScaleInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT256;
	psScaleInst->asArg[2].uType = USEASM_REGTYPE_TEMP;
	psScaleInst->asArg[2].uNumber = uFirstByteUnpackTemp;
	AppendInst(psState, psCodeBlock, psScaleInst);

	/*
	  Normalise the 24-bit data.

		DEST = UNSCALEDCHAN / ((1U << 23) - 1)
	*/
	psScaleInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psScaleInst, IFMUL);

	psScaleInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psScaleInst->asDest[0].uNumber = uDestTemp;

	psScaleInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
	psScaleInst->asArg[0].uNumber = uUnscaledChanTemp;

	fU24Scale = 1.0f / (IMG_FLOAT)((1U << 23) - 1);
	psScaleInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psScaleInst->asArg[1].uNumber = *(IMG_PUINT32)&fU24Scale;

	AppendInst(psState, psCodeBlock, psScaleInst);
}

/********************************************************************************
 * Register arrays                                                              *
 ********************************************************************************/

IMG_INTERNAL
IMG_BOOL IsModifiableRegisterArray(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber)
/*****************************************************************************
 FUNCTION	: IsModifiableRegisterArray

 PURPOSE	: Check for a reference to an indexable array modified inside the shader.

 PARAMETERS	: psState			- Compiler state.
			  uType				- Register type.
			  uNumber			- Register number.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	PUSC_VEC_ARRAY_REG	psArray;

	if (uType != USC_REGTYPE_REGARRAY)
	{
		return IMG_FALSE;
	}

	ASSERT(uNumber < psState->uNumVecArrayRegs);
	psArray = psState->apsVecArrayReg[uNumber];
	switch (psArray->eArrayType)
	{
		case ARRAY_TYPE_NORMAL:
		case ARRAY_TYPE_VERTEX_SHADER_OUTPUT:
		{
			return IMG_TRUE;
		}

		case ARRAY_TYPE_VERTICES_BASE:
		case ARRAY_TYPE_TEXTURE_COORDINATE:
		case ARRAY_TYPE_VERTEX_SHADER_INPUT:
		case ARRAY_TYPE_DRIVER_LOADED_SECATTR:
		case ARRAY_TYPE_DIRECT_MAPPED_SECATTR:
		{
			return IMG_FALSE;
		}

		default: imgabort();
	}
}

static
PUSC_VEC_ARRAY_DATA NewVecArrayData(PINTERMEDIATE_STATE psState,
									IMG_UINT32 uArrayTag,
									IMG_UINT32 uVecs,
									IMG_UINT32 uSize,
									UF_REGFORMAT eFmt,
									IMG_UINT32 uLoads)
/*****************************************************************************
 FUNCTION	: NewVecArrayData

 PURPOSE	: Allocate and initialise temp vector array data

 PARAMETERS	: psState	- Compiler state.
			  uVecs		- Number of vectors
			  uSize		- Size in bytes
			  eFmt		- Format of the array
			  uLoads	- Number of loads from the array
			  eArrayType
						- Type of array.

 RETURNS	: A new vector array information structure.
*****************************************************************************/
{
	PUSC_VEC_ARRAY_DATA psRet;

	psRet = (PUSC_VEC_ARRAY_DATA)UscAlloc(psState, sizeof(USC_VEC_ARRAY_DATA));

	psRet->uArrayTag = uArrayTag;
	psRet->uSize = uSize;
	psRet->uVecs = uVecs;
	psRet->eFmt = eFmt;
	psRet->uLoads = uLoads;
	psRet->uStores = 0;
	psRet->uRegArray = USC_UNDEF;
	psRet->bStatic = IMG_TRUE;
	psRet->bInRegs = IMG_FALSE;

	return psRet;
}

IMG_INTERNAL 
IMG_VOID LoadStoreIndexableTemp(PINTERMEDIATE_STATE	psState,
								PCODEBLOCK			psCodeBlock,
								IMG_BOOL			bLoad,
								UF_REGFORMAT		eFmt,
								PUF_REGISTER		psSource,
								IMG_UINT32			uChanMask,
								IMG_UINT32			uTemp,
								IMG_UINT32			uDataReg)
/*****************************************************************************
 FUNCTION	: LoadStoreIndexableTemp

 PURPOSE	: Generates code to load or store an indexable temp.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to insert the code into.
			  bLoad				- Is this a load (IMG_TRUE) or a store (IMG_FALSE).
			  eFmt				- Format of the data to load/store
			  psSource			- Input source.
			  uChanMask			- Mask of channels in psSource to read/write.
			  uTemp				- Number of temp register to use as scratch data.
			  uDataReg			- Number of temp register with the data (ignore for loads).
 
RETURNS	: Nothing

NOTES	: For loads/stores in which components span more than one register,
		  the destination/source registers are the temporary registers 
		  numbered (uTemp+0) to (uTemp+3) with the zero'th component in 
		  (uTemp + 0) and the fourth component in (uTemp + 3).
*****************************************************************************/
{
	IMG_UINT32 i;
	PINST psInst;
	IOPCODE eOpcode;
	IMG_UINT32 uCompDataSize = 0;		// Size of each component (in bytes).
	IMG_UINT32 uDynOffsetReg;			// Number of the register with the dynamic offset
	IMG_UINT32 uChansPerInputReg;	    // Number of channels in each input register
	IMG_BOOL bHasDynOffset;
	IMG_UINT32 uArrayNum;			    // Index into vector arrays
	IMG_UINT32 uArrayTag;
	PUSC_VEC_ARRAY_DATA psVecData;

	/* 
	   Set the data size (in bytes) and channels per register.
	   (C10 is stored as F16 data.)
	*/
	if (psState->uCompilerFlags & UF_OPENCL)
	{
		if (eFmt == UF_REGFORMAT_F32 ||
			eFmt == UF_REGFORMAT_U32 ||
			eFmt == UF_REGFORMAT_I32)
		{
			uCompDataSize = LONG_SIZE;
		}
		else if (eFmt == UF_REGFORMAT_F16 ||
				 eFmt == UF_REGFORMAT_I16 ||
				 eFmt == UF_REGFORMAT_U16)
		{
			uCompDataSize = WORD_SIZE;
		}
		else if (eFmt == UF_REGFORMAT_I8_UN ||
				 eFmt == UF_REGFORMAT_U8_UN)
		{
			uCompDataSize = BYTE_SIZE;
		}
		else
		{
			/* Only f/u/i32, f/u/i16 and u/i8 formats are supported */
			imgabort();
		}

		uChansPerInputReg = 1;
		eOpcode = bLoad ? ILDARRF32 : ISTARRF32;
	}
	else if (eFmt == UF_REGFORMAT_F32)
	{
		uChansPerInputReg = 1;
		/* uChansPerOutputReg = 1; */
		uCompDataSize = LONG_SIZE;
		eOpcode = bLoad ? ILDARRF32 : ISTARRF32;
	}
	else if (eFmt == UF_REGFORMAT_F16)
	{
		uChansPerInputReg = 2;
		/* uChansPerOutputReg = 2; */
		uCompDataSize = WORD_SIZE;
		eOpcode = bLoad ? ILDARRF16 : ISTARRF16;
	}
	else if (eFmt == UF_REGFORMAT_C10)
	{
		uChansPerInputReg = 4;
		/* uChansPerOutputReg = 2; */
		uCompDataSize = WORD_SIZE;
		eOpcode = bLoad ? ILDARRC10 : ISTARRC10;
	}
	else
	{
		/* Only f32, f16, c10 and u8 formats are supported */
		imgabort();
	}

	/* Record the fact that temporaries are being used */
	psState->uFlags |= USC_FLAGS_INDEXABLETEMPS_USED;

	/* Get the total number of registers needed in the banks */
	for (i = 0; 
		 i < psState->uIndexableTempArrayCount; 
		 i++)
	{
		if (psState->psIndexableTempArraySizes[i].uTag == psSource->uArrayTag)
		{
			break;
		}
	}

	/* Get the dynamic offset register */
	uDynOffsetReg = 0;
	if (psSource->eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
	{
		/* 
		   Get the number of the register with the dynamic offset into the 
		   register bank. The offset is the number of components from the
		   base address + static offset to the required component.
		*/
		bHasDynOffset = IMG_TRUE;

		ASSERT(psSource->eRelativeIndex != UFREG_RELATIVEINDEX_AL);
		uDynOffsetReg = USC_TEMPREG_ADDRESS;
		uDynOffsetReg += (psSource->eRelativeIndex - UFREG_RELATIVEINDEX_A0X);
	}
	else
	{
		bHasDynOffset = IMG_FALSE;
	}

	/* Update the array data */
	uArrayNum = i;
	uArrayTag = psSource->uArrayTag;
	ASSERT(uArrayNum < psState->uIndexableTempArrayCount);
	{
		psVecData = psState->apsTempVecArray[uArrayNum];
		if (psVecData == NULL)
		{
			IMG_UINT32 uVecs = psState->psIndexableTempArraySizes[uArrayNum].uSize;
			IMG_UINT32 uSize = uVecs * CHANNELS_PER_INPUT_REGISTER * uCompDataSize;

			psVecData = NewVecArrayData(psState, uArrayTag, uVecs, uSize, eFmt, 0);
			psState->apsTempVecArray[i] = psVecData;
			psState->uIndexableTempArraySize += psVecData->uSize;
		}
		/* Record how many loads and stores there are for this array. */
		if (bLoad)
		{
			psVecData->uLoads += 1;
		}
		else
		{
			psVecData->uStores += 1;
		}

		/* Record whether array accesses are all static */
		psVecData->bStatic = (psVecData->bStatic && (!bHasDynOffset)) ? IMG_TRUE : IMG_FALSE;
	}

	/*
	 * Emit the code
	 */

	/* Set up the load/store instruction */
	psInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psInst, eOpcode);

	/* Set the array tag, offset and format */
	psInst->u.psLdStArray->uArrayNum = uArrayNum;
	psInst->u.psLdStArray->uArrayOffset = psSource->uNum;
	psInst->u.psLdStArray->eFmt = eFmt;
	psInst->u.psLdStArray->uRelativeStrideInComponents = psSource->u1.uRelativeStrideInComponents;

	/* Set the channel mask */
	psInst->u.psLdStArray->uLdStMask = uChanMask;

	/* Set the dynamic offset register */
	if (bHasDynOffset)
	{
		psInst->asArg[LDSTARR_DYNAMIC_OFFSET_ARGINDEX].uType = USEASM_REGTYPE_TEMP;
		psInst->asArg[LDSTARR_DYNAMIC_OFFSET_ARGINDEX].uNumber = uDynOffsetReg;
	}
	else
	{
		psInst->asArg[LDSTARR_DYNAMIC_OFFSET_ARGINDEX].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[LDSTARR_DYNAMIC_OFFSET_ARGINDEX].uNumber = 0;
	}

	/* Set the load/store operands */
	if (bLoad)
	{
		/* Set the destination for a load */
		psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psInst->asDest[0].uNumber = uTemp;
		
		if (!(psState->uCompilerFlags & UF_OPENCL))
			psInst->asDest[0].eFmt = eFmt;
	}
	else
	{
		/*
		  Set the sources for a store by reading from the input
		  instruction operands
		*/
		IMG_BOOL bFirstChan = IMG_TRUE;
		IMG_UINT32 uSrcChan, uRegOffset, uLastRegOffset;

		uLastRegOffset = 0;
		for (uSrcChan = 0; uSrcChan < CHANNELS_PER_INPUT_REGISTER; uSrcChan ++)
		{
			IMG_UINT32 uArgNum;

			/* Calculate the offset from the first operand of the input instruction */
			uRegOffset = uSrcChan / uChansPerInputReg;

			if ((uChanMask & (1U << uSrcChan)) == 0)
			{
				/* This channel is ignored. */
				if (uRegOffset != uLastRegOffset)
				{
					psInst->asArg[LDSTARR_STORE_DATA_ARGSTART + uRegOffset].uType = USEASM_REGTYPE_IMMEDIATE; 
					psInst->asArg[LDSTARR_STORE_DATA_ARGSTART + uRegOffset].uNumber = 0;
				}
				continue;
			}

			if ((!bFirstChan) && (uRegOffset == uLastRegOffset))
			{
				/* This channel is already represented */
				continue;
			}
			bFirstChan = IMG_FALSE;

			/* Set the source operand */
			uArgNum = LDSTARR_STORE_DATA_ARGSTART + uRegOffset;

			psInst->asArg[uArgNum].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[uArgNum].uNumber = uDataReg + uRegOffset;
			
			if (!(psState->uCompilerFlags & UF_OPENCL))
				psInst->asArg[uArgNum].eFmt = eFmt;

			uLastRegOffset = uRegOffset;
		}
	}

	/* Add the instruction to the code block */
	AppendInst(psState, psCodeBlock, psInst);
}

IMG_INTERNAL
IMG_VOID StoreIndexableTemp(PINTERMEDIATE_STATE psState, 
							PCODEBLOCK psCodeBlock, 
							PUF_REGISTER psDest,
							UF_REGFORMAT eFmt,
							IMG_UINT32 uStoreSrc)
/*****************************************************************************
 FUNCTION	: StoreIndexableTemp

 PURPOSE	: Generates the code to store into an indexable temporary register.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Code block to insert any extra instructions needed.
			  psDest			- Destination register in the input format.
			  uStoreSrc			- Location of the data to store.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChanMask;

	if (eFmt == UF_REGFORMAT_C10)
	{
		uChanMask = ConvertInputWriteMaskToIntermediate(psState, psDest->u.byMask);
	}
	else
	{
		uChanMask = psDest->u.byMask;
	}

	LoadStoreIndexableTemp(psState, psCodeBlock,
						   IMG_FALSE, eFmt,
						   psDest, 
						   uChanMask,
						   USC_TEMPREG_INDEXTEMP, 
						   uStoreSrc);
}


/********************************************************************************
 * END: Register arrays                                                         *
 ********************************************************************************/

typedef enum
{
	LOD_MODE_NORMAL,
	LOD_MODE_BIAS,
	LOD_MODE_REPLACE,
	LOD_MODE_GRADIENTS
} LOD_MODE, *PLOD_MODE;

IMG_INTERNAL
IMG_UINT32 GetTextureCoordinateUsedChanCount(PINTERMEDIATE_STATE psState, IMG_UINT32 uTexture)
/*********************************************************************************
 Function			: GetTextureCoordinateUsedChanCount

 Description		: Get the count of channels used from the texture coordinates
					  when sampling a specified texture.

 Parameters			: psState			- Compiler state.
					  uTexture			- Index of the texture.

 Globals Effected	: None

 Return				: The count of used channels.
*********************************************************************************/
{
	switch (psState->psSAOffsets->asTextureDimensionality[uTexture].eType)
	{
		case UNIFLEX_DIMENSIONALITY_TYPE_1D: return 1;
		case UNIFLEX_DIMENSIONALITY_TYPE_2D: return 2;
		case UNIFLEX_DIMENSIONALITY_TYPE_3D: return 3;
		case UNIFLEX_DIMENSIONALITY_TYPE_CUBEMAP: return 3;
		default: imgabort();
	}
}

#if defined(OUTPUT_USCHW)
/*********************************************************************************
 Function			: EncodeTextureStateRegNum

 Description		: Encode the offset in the secondary attribute bank of the 
					  state for a texture lookup.

 Parameters			: dwTextureState - Base of the texture state secondary attributes.
					  dwTexture		- Texture to get the encoding for
					  dwChunk

 Globals Effected	: None

 Return				: The register number.
*********************************************************************************/
static IMG_UINT32 EncodeTextureStateRegNum(PINTERMEDIATE_STATE	psState,
										   IMG_UINT32			uTexture,
										   IMG_UINT32			uChunk)
{
	IMG_UINT32			uRegOffset;
	IMG_UINT32			i, j;
	IMG_UINT32			uStateNum;

	/*
	  Calculate the set of state-words associated with this texture-chunk

	  NB: Currently, the compiler assumes all chunks for preceeding textures
	  are present, so we must account for the number of chunks required
	  for each of them.
	*/
	uStateNum = 0;
	for (i = 0; i < uTexture; i++)
	{
		IMG_UINT32			uBitCount;
		PUNIFLEX_TEXFORM	psTexture = &psState->psSAOffsets->asTextureParameters[i].sFormat;
		IMG_UINT32			uChunkSizeInBits = psTexture->uChunkSize * 8;

		if (psTexture->bTwoResult)
		{
			ASSERT(uChunkSizeInBits == 32);
			uStateNum++;
		}
		else
		{
			uBitCount = 0;
			for (j = 0; j < 4; j++)
			{
				IMG_UINT32 uFormat;

				uFormat = psTexture->peChannelForm[j];

				uBitCount += g_puChannelFormBitCount[uFormat];
			}
		
			uStateNum += (uBitCount + uChunkSizeInBits - 1) / uChunkSizeInBits;
		}
	}

	/*
	  Each chunk required 3 texture-state words
	*/
	uRegOffset = (uStateNum + uChunk) * psState->uTexStateSize;

	return psState->psSAOffsets->uTextureState + uRegOffset;
}
#endif /* defined(OUTPUT_USCHW) */

#if defined(OUTPUT_USCHW)
IMG_INTERNAL
USC_CHANNELFORM GetUnpackedChannelFormat(PINTERMEDIATE_STATE		psState,
										 IMG_UINT32					uSamplerIdx,
										 USC_CHANNELFORM			ePackedFormat)
/*********************************************************************************
 Function			: GetUnpackedChannelFormat

 Description		: Get the format of a texture channel in the input to the USE.

 Parameters			: psState		- Compiler state.
					  uSamplerIdx	- Index of the texture.
					  ePackedFormat	- Native format of the texture channel.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUNIFLEX_TEXTURE_PARAMETERS	psTexParams = &psState->psSAOffsets->asTextureParameters[uSamplerIdx];

	ASSERT(GetBit(psState->auTextureUnpackFormatSelectedMask, uSamplerIdx) == 1);

	if (ePackedFormat == USC_CHANNELFORM_ZERO || ePackedFormat == USC_CHANNELFORM_ONE)
	{
		return ePackedFormat;
	}

	switch (psState->asTextureUnpackFormat[uSamplerIdx].eFormat)
	{
		case UNIFLEX_TEXTURE_UNPACK_FORMAT_NATIVE:
		{
			if (psTexParams->bGamma)
			{
				ASSERT(ePackedFormat == USC_CHANNELFORM_U8);

				#if defined(SUPPORT_SGX545)
				if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_GAMMA_UNPACK_TO_F16)
				{
					ePackedFormat = USC_CHANNELFORM_F16;
				}
				else
				#endif /* defined(SUPPORT_SGX545) */
				{
					ePackedFormat = USC_CHANNELFORM_U16;
				}
			}
			return ePackedFormat;
		}
		case UNIFLEX_TEXTURE_UNPACK_FORMAT_C10:
		{
			return USC_CHANNELFORM_C10;
		}
		case UNIFLEX_TEXTURE_UNPACK_FORMAT_F16:
		{
			return USC_CHANNELFORM_F16;
		}
		case UNIFLEX_TEXTURE_UNPACK_FORMAT_F32:
		{
			return USC_CHANNELFORM_F32;
		}
		default:
		{
			imgabort();
		}
	}
}

#define UFREG_SWIZ_BGR1 UFREG_ENCODE_SWIZ(UFREG_SWIZ_B, UFREG_SWIZ_G, UFREG_SWIZ_R, UFREG_SWIZ_1)
#define UFREG_SWIZ_RGB1 UFREG_ENCODE_SWIZ(UFREG_SWIZ_R, UFREG_SWIZ_G, UFREG_SWIZ_B, UFREG_SWIZ_1)

IMG_INTERNAL
IMG_UINT32 GetTexformSwizzle(PINTERMEDIATE_STATE	psState,
							 IMG_UINT32				uSampler)
/*********************************************************************************
 Function			: GetTexformSwizzle

 Description		: Get the swizzle for a texture.

 Parameters			: psState		- Compiler state.
					  uSampler		- Sampler index to get the swizzle for.

 Globals Effected	: None

 Return				: The swizzle.
*********************************************************************************/
{
	PUNIFLEX_TEXTURE_PARAMETERS	psTexParams = &psState->psSAOffsets->asTextureParameters[uSampler];
	PUNIFLEX_TEXFORM			psTexform = &psTexParams->sFormat;

	ASSERT(GetBit(psState->auTextureUnpackFormatSelectedMask, uSampler) == 1);

	if (
		psState->asTextureUnpackFormat[uSampler].eFormat == UNIFLEX_TEXTURE_UNPACK_FORMAT_F16 &&
		psTexform->bTFSwapRedAndBlue &&
		!psTexParams->bGamma
		)
	{
		/* Swapping is only applicable to 3/4 channel formats. */
		ASSERT(psTexform->uSwizzle == UFREG_SWIZ_BGRA || psTexform->uSwizzle == UFREG_SWIZ_BGR1);

		/*
		  Swap the red and blue channels.
		*/
		if (psTexform->uSwizzle == UFREG_SWIZ_BGR1)
		{
			return UFREG_SWIZ_RGB1;
		}
		else
		{
			return UFREG_SWIZ_RGBA;
		}
	}
	else
	{
		return psTexform->uSwizzle;
	}
}
#endif /* defined(OUTPUT_USCHW) */

IMG_INTERNAL
IMG_VOID GetGradientDetails(PINTERMEDIATE_STATE	psState,
						    IMG_UINT32			uTextureDimensionality,
						    UF_REGFORMAT		eCoordFormat,
							PSAMPLE_GRADIENTS	psGradients)
/*****************************************************************************
 FUNCTION	: GetGradientDetails

 PURPOSE	: Gets the format and layout of the gradient data read by a 
			  hardware texture sample instruction.

 PARAMETERS	: psState			- Compiler state.
			  uTextureDimensionality
								- Dimensionality of the texture to be sampled.
			  psGradients		- Initialized with the gradient details.

 RETURNS	: None.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		if (eCoordFormat == UF_REGFORMAT_C10)
		{
			psGradients->eGradFormat = UF_REGFORMAT_F16;
		}
		else
		{
			ASSERT(eCoordFormat == UF_REGFORMAT_F16 || eCoordFormat == UF_REGFORMAT_F32);
			psGradients->eGradFormat = eCoordFormat;
		}

		switch (uTextureDimensionality)
		{
			/*
				One vector register containing
					(dUdX, dUdY, _, _)
			*/
			case 1: psGradients->uUsedGradChanMask = USC_XY_CHAN_MASK; psGradients->uGradSize = 1; break;

			/*
				One vector register containing
					(dUdX, dVdX, dUdY, dVdY)
			*/
			case 2: psGradients->uUsedGradChanMask = USC_XYZW_CHAN_MASK; psGradients->uGradSize = 1; break;

			/*
				Two vector registers containing
					(dUdX, dVdX, dSdX, _)
					(dUdY, dVdY, dSdY, _)
			*/
			case 3: psGradients->uUsedGradChanMask = USC_XYZ_CHAN_MASK; psGradients->uGradSize = 2; break;
			default: imgabort();
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		PVR_UNREFERENCED_PARAMETER(eCoordFormat);

		switch (uTextureDimensionality)
		{
			case 1: psGradients->uGradSize = 2; break;
			case 2: psGradients->uGradSize = 4; break;
			case 3: psGradients->uGradSize = 6; break;
			default: imgabort();
		}
		psGradients->eGradFormat = UF_REGFORMAT_F32;
		psGradients->uUsedGradChanMask = USC_ALL_CHAN_MASK;
	}

	/*
		Allocate temporary variables to hold the gradient arguments.
	*/
	psGradients->uGradStart = GetNextRegisterCount(psState, psGradients->uGradSize);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static IMG_VOID EmitGradCalc_Vec(PINTERMEDIATE_STATE	psState,
								 PCODEBLOCK				psCodeBlock,
								 PINST					psInsertBeforeInst, 
								 IMG_UINT32				uDims,
								 UF_REGFORMAT			eCoordFormat,
								 IMG_UINT32				uCoordTempStart,
								 PARG					asCoordArgs,
								 PARG					psLODBias,
								 PSAMPLE_GRADIENTS		psGradients)
/*****************************************************************************
 FUNCTION	: EmitGradCalc_Vec

 PURPOSE	: Emit DSX/DSY instructions to replace an SMP-LOD with an SMP-GRAD.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInsertBeforeInst	
								- Point in the block to insert the intermediate
								instructions at.
			  uDims				- Number of dimensions (1..3).
			  eCoordFormat		- Format of the texture coordinates.
			  uCoordTempStart	- If asCoordArgs is NULL the start of the
								temporary registers containing the coordinate
								data.
			  asCoordArgs		- If non-NULL the registers containing the
								coordinate data.
			  psLODBias			- The LOD bias argument to the SMP.
			  psGradients		- The gradient arguments to the SMP.

 RETURNS	: None.
*****************************************************************************/
{	
	IMG_UINT32		uC10CvtTemp;
	UF_REGFORMAT	eGradFormat;
	IMG_UINT32		uAxis;
	IMG_UINT32		uPartialGrads = USC_UNDEF;
	IMG_UINT32		uPreScaleGradTemp;

	/*
		Convert C10 texture coordinates to F16 format.
	*/
	if (eCoordFormat == UF_REGFORMAT_C10)
	{
		PINST	psConvertInst;

		uC10CvtTemp = GetNextRegister(psState);
		
		psConvertInst = AllocateInst(psState, psInsertBeforeInst);

		SetOpcode(psState, psConvertInst, IVPCKFLTC10);

		SetDest(psState, psConvertInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uC10CvtTemp, UF_REGFORMAT_F16);

		if (asCoordArgs == NULL)
		{
			SetSrc(psState, psConvertInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uCoordTempStart, UF_REGFORMAT_C10);
		}
		else
		{
			SetSrcFromArg(psState, psConvertInst, 0 /* uSrcIdx */, &asCoordArgs[0]);
		}

		psConvertInst->u.psVec->auSwizzle[0] = ConvertSwizzle(psState, GetInputToU8C10IntermediateSwizzle(psState));
		psConvertInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_C10;

		InsertInstBefore(psState, psCodeBlock, psConvertInst, psInsertBeforeInst);

		eGradFormat = UF_REGFORMAT_F16;
	}
	else
	{
		uC10CvtTemp = USC_UNDEF;
		eGradFormat = eCoordFormat;
	}

	if (psLODBias != NULL)
	{
		uPreScaleGradTemp = GetNextRegisterCount(psState, psGradients->uGradSize);
	}
	else
	{
		uPreScaleGradTemp = psGradients->uGradStart;
	}

	/*
		Calculate gradients in each direction.
	*/
	for (uAxis = 0; uAxis < 2; uAxis++)
	{
		PINST		psGradInst;
		IMG_UINT32	uSwizzle;

		psGradInst = AllocateInst(psState, psInsertBeforeInst);

		if (uAxis == 0)
		{
			SetOpcode(psState, psGradInst, IVDSX);
		}
		else
		{
			SetOpcode(psState, psGradInst, IVDSY);
		}

		CopyPredicate(psState, psGradInst, psInsertBeforeInst);

		if (uDims == 1 || uDims == 2)
		{
			if (uAxis == 0)
			{
				uPartialGrads = GetNextRegister(psState);
				SetDest(psState, psGradInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPartialGrads, eGradFormat);
			}
			else
			{
				SetDest(psState, psGradInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPreScaleGradTemp, eGradFormat);
				SetPartialDest(psState, psGradInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPartialGrads, eGradFormat);
			}
		}
		
		switch (uDims)
		{
			case 1:
			{
				if (uAxis == 0)
				{
					psGradInst->auDestMask[0] = USC_X_CHAN_MASK;
					psGradInst->auLiveChansInDest[0] = USC_XY_CHAN_MASK;
				}
				else
				{
					psGradInst->auDestMask[0] = USC_Y_CHAN_MASK;
					psGradInst->auLiveChansInDest[0] = USC_XY_CHAN_MASK;
				}
				uSwizzle = USEASM_SWIZZLE(X, X, X, X);
				break;
			}
			case 2:
			{
				if (uAxis == 0)
				{
					psGradInst->auDestMask[0] = USC_XY_CHAN_MASK;
					psGradInst->auLiveChansInDest[0] = USC_XY_CHAN_MASK;
				}
				else
				{
					psGradInst->auDestMask[0] = USC_ZW_CHAN_MASK;
					psGradInst->auLiveChansInDest[0] = USC_XYZW_CHAN_MASK;
				}
				uSwizzle = USEASM_SWIZZLE(X, Y, X, Y);
				break;
			}
			case 3:
			{
				SetDest(psState, 
						psGradInst, 
						0 /* uDestIdx */, 
						USEASM_REGTYPE_TEMP, 
						uPreScaleGradTemp + uAxis, 
						eGradFormat);
				psGradInst->auDestMask[0] = psGradInst->auLiveChansInDest[0] = USC_XYZ_CHAN_MASK;
				uSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
				break;
			}
			default: imgabort();
		}

		if (eCoordFormat == UF_REGFORMAT_C10)
		{
			SetupTempVecSource(psState, psGradInst, 0 /* uGradSrcIdx */, uC10CvtTemp, eGradFormat);
		}
		else
		{
			if (asCoordArgs != NULL)
			{
				IMG_UINT32	uChan;

				SetSrcFromArg(psState, psGradInst, 0 /* uSrcIdx */, &asCoordArgs[0]);
				psGradInst->u.psVec->aeSrcFmt[0] = eCoordFormat;

				for (uChan = 0; uChan < 4; uChan++)
				{
					psGradInst->asArg[1 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
				}
			}
			else
			{		
				SetupTempVecSource(psState, psGradInst, 0 /* uGradSrcIdx */, uCoordTempStart, eGradFormat);
			}
		}

		psGradInst->u.psVec->auSwizzle[0] = uSwizzle;

		InsertInstBefore(psState, psCodeBlock, psGradInst, psInsertBeforeInst);
	}

	if (psLODBias != NULL)
	{
		PINST		psExpInst;
		IMG_UINT32	uBiasTemp;
		IMG_UINT32	uChan;
		IMG_UINT32	uReg;

		/*
			Calculate
				TEMP = 2^LOD_BIAS
		*/
		uBiasTemp = GetNextRegister(psState);

		psExpInst = AllocateInst(psState, NULL);

		SetOpcode(psState, psExpInst, IVEXP);

		SetDest(psState, psExpInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uBiasTemp, eGradFormat);

		psExpInst->auDestMask[0] = psExpInst->auLiveChansInDest[0] = USC_W_CHAN_MASK;

		SetSrcFromArg(psState, psExpInst, 0 /* uSrcIdx */, psLODBias);

		for (uChan = 0; uChan < 4; uChan++)
		{
			psExpInst->asArg[1 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
		}

		psExpInst->u.psVec->aeSrcFmt[0] = psLODBias->eFmt;
		psExpInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);

		InsertInstBefore(psState, psCodeBlock, psExpInst, psInsertBeforeInst);

		/*
			Multiply each of the gradients by 2^LOD_BIAS.
		*/
		for (uReg = 0; uReg < psGradients->uGradSize; uReg++)
		{
			PINST		psMulInst;
			IMG_UINT32	uWriteMask;

			switch (uDims)
			{
				case 1: uWriteMask = USC_XY_CHAN_MASK; break;
				case 2: uWriteMask = USC_XYZW_CHAN_MASK; break;
				case 3: uWriteMask = USC_XYZ_CHAN_MASK; break;
				default: imgabort();
			}

			psMulInst = AllocateInst(psState, NULL);

			SetOpcode(psState, psMulInst, IVMUL);

			SetDest(psState, psMulInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, psGradients->uGradStart + uReg, eGradFormat);

			psMulInst->auDestMask[0] = psMulInst->auLiveChansInDest[0] = uWriteMask;

			SetupTempVecSource(psState, psMulInst, 0, uPreScaleGradTemp + uReg, eGradFormat);

			psMulInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
	
			SetupTempVecSource(psState, psMulInst, 1, uBiasTemp, eGradFormat);
	
			psMulInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(W, W, W, W);
	
			InsertInstBefore(psState, psCodeBlock, psMulInst, psInsertBeforeInst);
		}
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_VOID EmitGradCalc(PINTERMEDIATE_STATE	psState,
					  PCODEBLOCK			psCodeBlock,
					  PINST					psInsertBeforeInst,
					  IMG_UINT32			uDims,
					  UF_REGFORMAT			eCoordFormat,
					  IMG_UINT32			uCoordTempStart,
					  PARG					asCoordArgs,
					  PARG					psLODBias,
					  PSAMPLE_GRADIENTS		psGradients)
/*****************************************************************************
 FUNCTION	: EmitGradCalc

 PURPOSE	: Emit DSX/DSY instructions to replace an SMP-LOD with an SMP-GRAD.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInsertBeforeInst	
								- Point in the block to insert the intermediate
								instructions at.
			  uDims				- Number of dimensions (1..3).
			  eCoordFormat		- Format of the texture coordinates.
			  uCoordTempStart	- If asCoordArgs is NULL the start of the
								temporary registers containing the coordinate
								data.
			  asCoordArgs		- If non-NULL the registers containing the
								coordinate data.
			  psLODBias			- The LOD bias argument to the SMP.
			  psGradients		- The gradient arguments to the SMP.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32 uCtr;
	IMG_UINT32 uNum = 0;
	PINST psInst;
	PINST psPackInst;
	IMG_UINT32 uPow2LODBias;
	IMG_UINT32 uPreScaleGrad;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		EmitGradCalc_Vec(psState, 
						 psCodeBlock, 
						 psInsertBeforeInst,
						 uDims, 
						 eCoordFormat, 
						 uCoordTempStart, 
						 asCoordArgs, 
						 psLODBias, 
						 psGradients);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	if (psLODBias != NULL)
	{
		PINST	psExpInst;

		uPow2LODBias = GetNextRegister(psState);

		/*
		  Calculate 2^LOD_BIAS
		*/
		psExpInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psExpInst, IFEXP);

		SetDest(psState, psExpInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPow2LODBias, UF_REGFORMAT_F32);

		SetSrcFromArg(psState, psExpInst, 0 /* uSrcIdx */, psLODBias);

		InsertInstBefore(psState, psCodeBlock, psExpInst, psInsertBeforeInst);

		uPreScaleGrad = GetNextRegisterCount(psState, uDims * 2);
	}
	else
	{
		uPow2LODBias = USC_UNDEF;
		uPreScaleGrad = psGradients->uGradStart;
	}

	for(uCtr = 0; uCtr < uDims ; uCtr ++)
	{
		IMG_UINT32 uSrcType;
		IMG_UINT32 uSrcReg;
		UF_REGFORMAT eSrcFmt;
		IMG_UINT32 uChan;

		switch (eCoordFormat)
		{
			case UF_REGFORMAT_F16:
			{
				if (asCoordArgs == NULL)
				{
					uSrcType = USEASM_REGTYPE_TEMP;
					uSrcReg = uCoordTempStart + (uCtr / 2);
				}
				else
				{
					uSrcType = asCoordArgs[uCtr / 2].uType;
					uSrcReg = asCoordArgs[uCtr / 2].uNumber;
				}
				eSrcFmt = UF_REGFORMAT_F16;
				uChan = (uCtr % 2) * 2;
				break;
			}
			case UF_REGFORMAT_C10:
			case UF_REGFORMAT_U8:
			{
				/*
				  Convert the input register to float-32.  Put the converted
				  coordinate in the register that will passed as the
				  destination for the DSY (which will not be overwritten by
				  the DSX).

				  Coordinates are stored in a single register in the order SVU.
				*/
				uSrcType = USEASM_REGTYPE_TEMP;
				uSrcReg = GetNextRegister(psState);
				eSrcFmt = UF_REGFORMAT_F32;
				uChan = 0; 

				psPackInst = AllocateInst(psState, psInsertBeforeInst);
				SetOpcode(psState, psPackInst, IUNPCKF32C10);
				psPackInst->u.psPck->bScale = IMG_TRUE;

				SetDest(psState, psPackInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uSrcReg, UF_REGFORMAT_F32);

				if (asCoordArgs == NULL)
				{
					SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uCoordTempStart, eCoordFormat);
				}
				else
				{
					SetSrcFromArg(psState, psPackInst, 0 /* uSrcIdx */, &asCoordArgs[0]);
				}
				SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, 2 - uCtr);

				InsertInstBefore(psState, psCodeBlock, psPackInst, psInsertBeforeInst);

				break;
			}
			case UF_REGFORMAT_F32:
			default:
			{
				if (asCoordArgs == NULL)
				{
					uSrcType = USEASM_REGTYPE_TEMP;
					uSrcReg = uCoordTempStart + uCtr;
				}
				else
				{
					uSrcType = asCoordArgs[uCtr].uType;
					uSrcReg = asCoordArgs[uCtr].uNumber;
				}
				uChan = 0;
				eSrcFmt = UF_REGFORMAT_F32;
				break;
			}
		}

		/* Emit FDSX */
		psInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psInst, IFDSX);

		SetDest(psState, 
				psInst, 
				0 /* uDestIdx */, 
				USEASM_REGTYPE_TEMP, 
				uPreScaleGrad + uNum, 
				UF_REGFORMAT_F32);

		SetSrc(psState, psInst, 0 /* uSrcIdx */, uSrcType, uSrcReg, eSrcFmt);
		SetComponentSelect(psState, psInst, 0 /* uArgIdx */, uChan);
		SetSrc(psState, psInst, 1 /* uSrcIdx */, uSrcType, uSrcReg, eSrcFmt);
		SetComponentSelect(psState, psInst, 1 /* uArgIdx */, uChan);

		InsertInstBefore(psState, psCodeBlock, psInst, psInsertBeforeInst);

		/* Emit FDSY */
		psInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psInst, IFDSY);

		SetDest(psState, 
				psInst, 
				0 /* uDestIdx */, 
				USEASM_REGTYPE_TEMP, 
				uPreScaleGrad + uNum + 1, 
				UF_REGFORMAT_F32);

		SetSrc(psState, psInst, 0 /* uSrcIdx */, uSrcType, uSrcReg, eSrcFmt);
		SetComponentSelect(psState, psInst, 0 /* uArgIdx */, uChan);
		SetSrc(psState, psInst, 1 /* uSrcIdx */, uSrcType, uSrcReg, eSrcFmt);
		SetComponentSelect(psState, psInst, 1 /* uArgIdx */, uChan);

		InsertInstBefore(psState, psCodeBlock, psInst, psInsertBeforeInst);

		/*
		  Scale the gradients by 2^LOD_BIAS.
		*/
		if (psLODBias != NULL)
		{
			IMG_UINT32	uAxisIdx;

			/*
			  FMUL	GRADIENT, GRADIENT, 2^LOD_BIAS
			*/
			for (uAxisIdx = 0; uAxisIdx < 2; uAxisIdx++)
			{
				PINST	psMulInst;

				psMulInst = AllocateInst(psState, psInsertBeforeInst);
				SetOpcode(psState, psMulInst, IFMUL);

				SetDest(psState, 
					    psMulInst, 
						0 /* uDestIdx */, 
						USEASM_REGTYPE_TEMP, 
						psGradients->uGradStart + uNum + uAxisIdx, 
						UF_REGFORMAT_F32);

				SetSrc(psState, 
					   psMulInst, 
					   0 /* uSrcIdx */, 
					   USEASM_REGTYPE_TEMP, 
					   uPreScaleGrad + uNum + uAxisIdx, 
					   UF_REGFORMAT_F32);

				SetSrc(psState, psMulInst, 1 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uPow2LODBias, UF_REGFORMAT_F32);

				InsertInstBefore(psState, psCodeBlock, psMulInst, psInsertBeforeInst);
			}
		}

		uNum += 2;
	}
}

typedef struct _SAMPLE_INPUT_PARAMS
{	
	/*
		Immediate, texel-space offsets to apply the texture sample coordinates.
	*/
	SMP_IMM_OFFSETS				sOffsets;
	/*
		Special result to return from the texture sample.
	*/
	UF_LDPARAM_SPECIALRETURN	eSpecialReturnMode;
} SAMPLE_INPUT_PARAMS, *PSAMPLE_INPUT_PARAMS;

static
IMG_VOID SetupTextureSampleParameters(PINTERMEDIATE_STATE	psState,
									  PCODEBLOCK			psCodeBlock,
									  PUNIFLEX_INST			psInputInst,
									  UF_OPCODE				eOpCode,
									  IMG_UINT32			uTextureDimensionality,
									  IMG_BOOL				bPCF,
									  IMG_BOOL				bEmulatePCF,
									  IMG_BOOL				bProjected,
									  IMG_UINT32			uSamplerIdx,
									  PLOD_MODE				peLODMode,
									  PSAMPLE_COORDINATES	psCoordinates,
									  PSAMPLE_LOD_ADJUST	psLODAdjust,
									  PSAMPLE_GRADIENTS		psGradients,
									  PSAMPLE_PROJECTION	psProj,
									  PSAMPLE_INPUT_PARAMS	psInputParams,
									  IMG_PBOOL				pbSampleIdxPresent,
									  PARG					psSampleIdx,
									  PSMP_RETURNDATA		peSpecialReturnData)
/*****************************************************************************
 FUNCTION	: SetupTextureSampleParameters

 PURPOSE	: Converts the various parameters to an input texture sample to the
			  intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Input texture sample instruction.
			  eOpCode			- Type of texture sample.
			  uTextureDimensionality
								- Dimensionality of the texture being sampled.
			  bPCF				- TRUE if PCF is enabled for this texture sample.
			  bEmulatePCF		- TRUE if PCF is being emulated for this
								texture sample.
			  uSamplerIdx		- Index of the texture being sampled.
			  peLODMode			- Returns the LOD mode to use for the calculation.
			  psCoordinates		- Returns the details of the texture coordinate
								sources.
			  psLODAdjust		- Returns details of the LOD bias or replace source.
			  psGradients		- Returns details of the gradient sources.
			  psProj			- Returns details of the value to projection
								the texture coordinates by.
			  psImmOffsets		- Returns details of any immediate offsets.
			  pbUpdateSampleIdx	- Returns TRUE if the texture sample needs to 
								update the Sample Idx. field. 
			  psSampleIdx		- Returns details of Sample Idx. updates.
			  peSpecialReturnData
								- Returns any special texture sample return data.
								

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32		uCoordinateDimensionality;
	UF_REGFORMAT	eInputCoordFormat;
	
	/*
		The code which processes the input program should have already have expanded
		any texture samples using cases of immediate offsets not directly supported by
		the hardware.
	*/
	if (psInputParams != NULL && psInputParams->sOffsets.bPresent)
	{
		IMG_BOOL	bImmediateOffsetsValid;

		bImmediateOffsetsValid = IMG_FALSE;
		#if defined(SUPPORT_SGX545)
		if (
				(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_TAG_1D_2D_IMMEDIATE_COORDINATE_OFFSETS) &&
				uTextureDimensionality <= 2
		   )
		{
			/*
				The hardware supports this case of immediate offsets directly.
			*/
			bImmediateOffsetsValid = IMG_TRUE;
		}
		#endif /* defined(SUPPORT_SGX545) */

		ASSERT(bImmediateOffsetsValid);
	}

	/*
		Does the texture sample have an extra parameter for the
		LOD.
	*/
	psLODAdjust->bLODAdjustment = IMG_FALSE;
	if (eOpCode == UFOP_LDB || eOpCode == UFOP_LDL || eOpCode == UFOP_LD2DMS)
	{
		psLODAdjust->bLODAdjustment = IMG_TRUE;
	}

	/*
		If we are using PCF then the depth value for comparison is treated like
		an extra coordinate immediately after the coordinates corresponding to the
		dimensionality of the texture.
	*/
	uCoordinateDimensionality = uTextureDimensionality;
	if (bPCF)
	{
		uCoordinateDimensionality++;
	}

	/*
		Get the register format of the texture coordinate source to the
		input texture sample instruction.
	*/
	eInputCoordFormat = GetRegisterFormat(psState, &psInputInst->asSrc[0]);

	psCoordinates->eCoordFormat = eInputCoordFormat;
	/*
		The hardware doesn't texture coordinates in U8 formats.
	*/
	if (psCoordinates->eCoordFormat == UF_REGFORMAT_U8)
	{
		psCoordinates->eCoordFormat = UF_REGFORMAT_C10;
	}
	/*
		Up convert formats where we are modifying the texture coordinates to reduce
		the number of code paths we need.
	*/
	if (bEmulatePCF)
	{
		psCoordinates->eCoordFormat = UF_REGFORMAT_F32;
	}
	/*
		The hardware doesn't support the combination of PCF and texture coordinates
		in C10 format.
	*/
	if (bPCF && !bEmulatePCF && psCoordinates->eCoordFormat == UF_REGFORMAT_C10)
	{
		psCoordinates->eCoordFormat = UF_REGFORMAT_F32;
	}

	/*
		Calculate the size and layout of the texture coordinates in this format.
	*/
	if (
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 &&
			(
				psCoordinates->eCoordFormat == UF_REGFORMAT_F32 ||
				psCoordinates->eCoordFormat == UF_REGFORMAT_F16
			)
	   )
	{
		/*
			One vector register.
		*/
		psCoordinates->uCoordSize = 1;
		psCoordinates->uCoordStart = GetNextRegister(psState);
		psCoordinates->uUsedCoordChanMask = (1U << uCoordinateDimensionality) - 1;
	}
	else
	{
		switch (psCoordinates->eCoordFormat)
		{
			case UF_REGFORMAT_F32:
			{
				/*
					One scalar register for each channel of coordinate data.
				*/
				psCoordinates->uCoordSize = uCoordinateDimensionality;

				/*
					All registers containing coordinate data are fully referenced.
				*/
				psCoordinates->uUsedCoordChanMask = USC_XYZW_CHAN_MASK;
				break;
			}
			case UF_REGFORMAT_F16:
			{
				/*
					Calculate the number of 32-bit registers needed for the coordinates.
				*/
				psCoordinates->uCoordSize = 
					(uCoordinateDimensionality + F16_CHANNELS_PER_SCALAR_REGISTER - 1) / F16_CHANNELS_PER_SCALAR_REGISTER;

				/*
					If the number of coordinate channels is odd then only one F16 channel
					is used from the last register of coordinate data.
				*/
				if ((uCoordinateDimensionality % F16_CHANNELS_PER_SCALAR_REGISTER) == 1)
				{
					psCoordinates->uUsedCoordChanMask = USC_XY_CHAN_MASK;
				}
				else
				{
					/*
						Otherwise the last register is fully used.
					*/	
					psCoordinates->uUsedCoordChanMask = USC_ALL_CHAN_MASK;
				}
				break;
			}
			case UF_REGFORMAT_U8:
			case UF_REGFORMAT_C10:
			{
				psCoordinates->uCoordSize = 1;
				psCoordinates->uUsedCoordChanMask = 
					ConvertInputWriteMaskToIntermediate(psState, (1U << uCoordinateDimensionality) - 1);
				break;
			}
			default: imgabort();
		}
	}

	psCoordinates->uCoordType = USEASM_REGTYPE_TEMP;
	psCoordinates->uCoordStart = GetNextRegisterCount(psState, psCoordinates->uCoordSize);

	switch (eInputCoordFormat)
	{
		case UF_REGFORMAT_F32:
		{
			GetSampleCoordinatesF32(psState, 
									psCodeBlock, 
									psInputInst, 
									uCoordinateDimensionality,
									psCoordinates,
									bPCF);
			break;
		}
		case UF_REGFORMAT_F16:
		{
			GetSampleCoordinatesF16(psState, 
									psCodeBlock, 
									psInputInst, 
									uCoordinateDimensionality,
									psCoordinates,
									bPCF);
			break;
		}
		case UF_REGFORMAT_C10:
		case UF_REGFORMAT_U8:
		{
			GetSampleCoordinatesC10(psState, 
									psCodeBlock, 
									psInputInst, 
									uCoordinateDimensionality,
									psCoordinates,
									bPCF);
			break;
		}
		default: imgabort();
	}

	if (bProjected)
	{
		psProj->bProjected = IMG_TRUE;
		switch (eInputCoordFormat)
		{
			case UF_REGFORMAT_F32:
			{
				GetProjectionF32(psState, psCodeBlock, psInputInst, psProj, psCoordinates->eCoordFormat);
				break;
			}
			case UF_REGFORMAT_F16:
			{
				GetProjectionF16(psState, psCodeBlock, psInputInst, psProj, psCoordinates->eCoordFormat);
				break;
			}
			case UF_REGFORMAT_C10:
			case UF_REGFORMAT_U8:
			{
				GetProjectionC10(psState, psCodeBlock, psInputInst, psProj, psCoordinates->eCoordFormat);
				break;
			}
			default: imgabort();
		}
	}
	else
	{
		psProj->bProjected = IMG_FALSE;
		InitInstArg(&psProj->sProj);
	}

	if (psState->psSAOffsets->asTextureDimensionality[uSamplerIdx].bIsArray)
	{	
		UF_REGFORMAT	eCoordFormat = GetRegisterFormat(psState, &psInputInst->asSrc[0]);
		IMG_UINT32		uSrcChan;

		psCoordinates->bTextureArray = IMG_TRUE;
		psCoordinates->uArrayIndexTemp = GetNextRegister(psState);

		/*
			Take the array index from the next channel in the texture coordinate source after
			those corresponding to the dimensionality of the texture array elements.
		*/
		switch(uTextureDimensionality)
		{
			case 1:
			{
				uSrcChan = 1;
				break;
			}
			case 2:
			{
				uSrcChan = 2;
				break;	
			}
			default:
			{
				uSrcChan = 1;
				UscFail(psState, UF_ERR_GENERIC, "Texture array of arrays, dimensionality must be 1 or 2");
				break;
			}	
		}

		/*
			Unpack the array index from the texture coordinate source, clamp it to the maximum
			array index and convert it to U16.
		*/
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			switch (eCoordFormat)
			{
				case UF_REGFORMAT_F32:
				case UF_REGFORMAT_F16:
				{
					GetSampleCoordinateArrayIndexFlt_Vec(psState,
														 psCodeBlock,
														 psInputInst,
														 uSrcChan,
														 uSamplerIdx,
														 psCoordinates->uArrayIndexTemp);
					break;
				}
				case UF_REGFORMAT_U8:
				case UF_REGFORMAT_C10:
				{
					GetSampleCoordinateArrayIndexC10(psState, 
													 psCodeBlock, 
													 psInputInst, 
													 uTextureDimensionality, 
													 uSamplerIdx,
													 psCoordinates->uArrayIndexTemp);
					break;
				}
				default: imgabort();
			}
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			switch (eCoordFormat)
			{
				case UF_REGFORMAT_F32:
				{
					GetSampleCoordinateArrayIndexF32(psState, 
													 psCodeBlock, 
													 psInputInst, 
													 uSrcChan, 
													 uSamplerIdx,
													 psCoordinates->uArrayIndexTemp);
					break;
				}
				case UF_REGFORMAT_F16:
				{
					GetSampleCoordinateArrayIndexF16(psState, 
													 psCodeBlock, 
													 psInputInst, 
													 uSrcChan, 
													 uSamplerIdx,
													 psCoordinates->uArrayIndexTemp);
					break;
				}				
				case UF_REGFORMAT_U8:
				case UF_REGFORMAT_C10:
				{
					GetSampleCoordinateArrayIndexC10(psState, 
													 psCodeBlock, 
													 psInputInst, 
													 uSrcChan, 
													 uSamplerIdx,
													 psCoordinates->uArrayIndexTemp);
					break;	
				}
				default: imgabort();
			}
		}			
	}
	else
	{
		psCoordinates->bTextureArray = IMG_FALSE;
		psCoordinates->uArrayIndexTemp = USC_UNDEF;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		if (psCoordinates->eCoordFormat == UF_REGFORMAT_F32)
		{
			psLODAdjust->eLODFormat = UF_REGFORMAT_F32;
			psGradients->eGradFormat = UF_REGFORMAT_F32;
		}
		else
		{
			psLODAdjust->eLODFormat = UF_REGFORMAT_F16;
			psGradients->eGradFormat = UF_REGFORMAT_F16;
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		psLODAdjust->eLODFormat = UF_REGFORMAT_F32;
		psGradients->eGradFormat = UF_REGFORMAT_F32;
	}

	/*
	  Get the LOD adjustment source.
	*/
	if (psLODAdjust->bLODAdjustment)
	{
		UF_REGFORMAT eFmt;
		psLODAdjust->uLODTemp = GetNextRegister(psState);
		if(psInputInst->eOpCode != UFOP_LD2DMS)
		{
			eFmt = GetRegisterFormat(psState, &psInputInst->asSrc[2]);
		}
		else
		{
			eFmt = GetRegisterFormat(psState, &psInputInst->asSrc[3]);
		}
		switch (eFmt)
		{
			case UF_REGFORMAT_F32:
			{
				GetLODAdjustmentF32(psState, psCodeBlock, psInputInst, psLODAdjust);
				break;
			}
			case UF_REGFORMAT_F16:
			{
				GetLODAdjustmentF16(psState, psCodeBlock, psInputInst, psLODAdjust);
				break;
			}
			case UF_REGFORMAT_U8:
			case UF_REGFORMAT_C10:
			{
				GetLODAdjustmentC10(psState, psCodeBlock, psInputInst, psLODAdjust);
				break;
			}
			default: imgabort();
		}
	}
	else
	{
		psLODAdjust->eLODFormat = USC_UNDEF;
		psLODAdjust->uLODTemp = USC_UNDEF;
	}

	/*
		Set up the details of the gradient sources to the texture sample.
	*/
	if (eOpCode == UFOP_LDD)
	{
		GetGradientDetails(psState, 
						   uTextureDimensionality, 
						   psCoordinates->eCoordFormat,
						   psGradients);
	}
	else
	{
		psGradients->uGradSize = 0;
		psGradients->uUsedGradChanMask = 0;
		psGradients->eGradFormat = USC_UNDEF;
		psGradients->uGradStart = USC_UNDEF;
	}

	/*
		Get the gradient sources.
	*/
	if (psInputInst->eOpCode == UFOP_LDD)
	{
		IMG_UINT32	uCoord;

		for (uCoord = 0; uCoord < 2; uCoord++)
		{	
			switch (GetRegisterFormat(psState, &psInputInst->asSrc[2 + uCoord]))
			{
				case UF_REGFORMAT_F32:
				{
					GetGradientsF32(psState,
									psCodeBlock,
									psInputInst,
									uTextureDimensionality,
									uCoord,
									psGradients);
					break;
				}
				case UF_REGFORMAT_F16:
				{
					GetGradientsF16(psState,
									psCodeBlock,
									psInputInst,
									uTextureDimensionality,
									uCoord,
									psGradients);
					break;
				}
				case UF_REGFORMAT_U8:
				case UF_REGFORMAT_C10:
				{
					GetGradientsC10(psState,
									psCodeBlock,
									psInputInst,
									uTextureDimensionality,
									uCoord,
									psGradients);
					break;
				}
				default:
				{
					break;
				}
			}
		}
	}

	if (psInputInst->eOpCode == UFOP_LD2DMS)
	{	
		InitInstArg(psSampleIdx);
		psSampleIdx->uType = USEASM_REGTYPE_TEMP;
		psSampleIdx->uNumber = GetNextRegister(psState);
		psSampleIdx->eFmt = UF_REGFORMAT_F32;
		*pbSampleIdxPresent = IMG_TRUE;
		switch (GetRegisterFormat(psState, &psInputInst->asSrc[2]))
		{
			case UF_REGFORMAT_F32:
			{
				GetSampleIdxF32(psState,
								psCodeBlock,
								psInputInst,
								psSampleIdx);
				break;
			}
			case UF_REGFORMAT_F16:
			{
				GetSampleIdxF16(psState,
								psCodeBlock,
								psInputInst,
								psSampleIdx);
				break;
			}
			case UF_REGFORMAT_U8:
			case UF_REGFORMAT_C10:
			{
				GetSampleIdxC10(psState,
								psCodeBlock,
								psInputInst,								
								psSampleIdx);
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
		*pbSampleIdxPresent = IMG_FALSE;
	}

	/*
	  Get the dimensionality and lod mode for this lookup,
	  converting conditional LOD and BIAS to GRAD.
	*/
	switch (eOpCode)
	{
		case UFOP_LD:
		case UFOP_LDP:
		case UFOP_LDC:
		{
			*peLODMode = LOD_MODE_NORMAL;
			break;
		}
		case UFOP_LDB:
		{
			*peLODMode = LOD_MODE_BIAS; 
			break;
		}
		case UFOP_LDL:
		case UFOP_LDCLZ:
		case UFOP_LD2DMS:
		case UFOP_LDGATHER4:
		{
			*peLODMode = LOD_MODE_REPLACE; 
			break;
		}
		case UFOP_LDD: *peLODMode = LOD_MODE_GRADIENTS; break;
		default: imgabort();
	}

	/*
		Should the sample return special data?
	*/
	if (peSpecialReturnData != NULL)
	{
		if (eOpCode == UFOP_LDGATHER4)
		{
			ASSERT(psInputParams->eSpecialReturnMode == UF_LDPARAM_SPECIALRETURN_NONE);
			*peSpecialReturnData = SMP_RETURNDATA_RAWSAMPLES;
		}
		else
		{
			switch (psInputParams->eSpecialReturnMode)
			{
				case UF_LDPARAM_SPECIALRETURN_NONE:
				{
					*peSpecialReturnData = SMP_RETURNDATA_NORMAL;
					break;
				}
				case UF_LDPARAM_SPECIALRETURN_SAMPLEINFO:
				{
					*peSpecialReturnData = SMP_RETURNDATA_SAMPLEINFO;
					break;
				}
				case UF_LDPARAM_SPECIALRETURN_RAWSAMPLES:
				{
					*peSpecialReturnData = SMP_RETURNDATA_RAWSAMPLES;
					break;
				}
				default: imgabort();
			}
		}
	}
	else
	{
		ASSERT(psInputParams == NULL || psInputParams->eSpecialReturnMode == UF_LDPARAM_SPECIALRETURN_NONE);
		ASSERT(eOpCode != UFOP_LDGATHER4);
	}
}

/*****************************************************************************
 FUNCTION	: LoadTextureState

 PURPOSE	: Loads a dword of texture state.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInsertBeforeInst
								- Point to insert the intermediate instructions at.
			  uStateOffset		- Offset of the state in the constant buffer.
			  psArg				- Filled out on return with an intermediate
								register that contains the texture state.

 RETURNS	: None.
*****************************************************************************/
static IMG_VOID LoadTextureState(PINTERMEDIATE_STATE		psState,
								 PCODEBLOCK					psCodeBlock,
								 PINST						psInsertBeforeInst,
								 IMG_UINT32					uStateOffset,
								 PARG						psArg)
{
	UF_REGISTER sSource;

	/*
	  Offset by the start of texture state within the constant buffer.
	*/
	uStateOffset += psState->psSAOffsets->uTextureStateConstOffset;

	/*
	  Set up an input format source representing that offset.
	*/
	sSource.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	sSource.uNum = uStateOffset / CHANNELS_PER_INPUT_REGISTER;
	sSource.uArrayTag = psState->uTextureStateConstsBuffer;

	/*
	  Load the dword of constant data corresponding to the source.
	*/
	LoadConstantNoHWReg(psState,
						psCodeBlock,
						psInsertBeforeInst,
						&sSource,
						uStateOffset % CHANNELS_PER_INPUT_REGISTER,
						psArg,
						NULL /* puComponent */,
						UF_REGFORMAT_F32,
						#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
						IMG_FALSE /* bVectorResult */,
						#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
						IMG_FALSE /* bC10Subcomponent */,
						0);
}

/*****************************************************************************
 FUNCTION	: LoadMaxTexarrayIndex

 PURPOSE	: Loads the maximum array index for a texture array from the
			  constant buffer.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  uTexture			- Index of the texture to load for.
			  psArg				- Filled out on return with an intermediate
								register that contains the maximum array index.

 RETURNS	: None.
*****************************************************************************/
IMG_INTERNAL
IMG_VOID LoadMaxTexarrayIndex(PINTERMEDIATE_STATE			psState,
							  PCODEBLOCK					psCodeBlock,
							  IMG_UINT32					uTexture,
							  PARG							psArg)
{
	IMG_UINT32	uMaxTexArrayIndexOffset;

	uMaxTexArrayIndexOffset = _UNIFLEX_TEXSTATE_SAMPLERBASE(uTexture, psState->uTexStateSize);
	uMaxTexArrayIndexOffset += _UNIFLEX_TEXSTATE_MAXARRAYINDEX(psState->uTexStateSize);

	LoadTextureState(psState, psCodeBlock, NULL /* psInsertBeforeInst */, uMaxTexArrayIndexOffset, psArg);
}

IMG_INTERNAL
IMG_VOID SetSampleGradientsDetails(PINTERMEDIATE_STATE	psState,
								   PINST				psSmpInst,
								   PSAMPLE_GRADIENTS	psGradients)
/*****************************************************************************
 FUNCTION	: SetSampleGradientsDetails

 PURPOSE	: Copies the details of the registers holding the source gradients
			  into an SMP instruction.

 PARAMETERS	: psState			- Compiler state.
			  psSmpInst			- SMP instruction to copy into.
			  psGradients		- Details of the gradients.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uArg;

	if (psGradients != NULL)
	{
		psSmpInst->u.psSmp->uGradSize = psGradients->uGradSize;
		for (uArg = 0; uArg < psGradients->uGradSize; uArg++)
		{
			psSmpInst->u.psSmp->auUsedGradChanMask[uArg] = psGradients->uUsedGradChanMask;
			SetSrc(psState,
				   psSmpInst,
				   SMP_GRAD_ARG_START + uArg,
				   USEASM_REGTYPE_TEMP,
				   psGradients->uGradStart + uArg,
				   psGradients->eGradFormat);
			
		}
	}
	else
	{
		psSmpInst->u.psSmp->uGradSize = 0;
	}
	for (uArg = psSmpInst->u.psSmp->uGradSize; uArg < SMP_MAXIMUM_GRAD_ARG_COUNT; uArg++)
	{
		InitInstArg(&psSmpInst->asArg[SMP_GRAD_ARG_START + uArg]);
		psSmpInst->asArg[SMP_GRAD_ARG_START + uArg].uType = USC_REGTYPE_UNUSEDSOURCE;
	}
}

#if defined(OUTPUT_USPBIN)
static
PINST CreateUSPSampleUnpackInstruction(PINTERMEDIATE_STATE	psState, 
									   PINST				psDrInst, 
									   IMG_UINT32			uChanMask,
									   IMG_UINT32			uDestCount, 
									   ARG					asDest[], 
									   IMG_UINT32			auDestMask[])
/*****************************************************************************
 FUNCTION	: CreateUSPSampleUnpackInstruction

 PURPOSE	: Create an instruction representing the USP generated code to
			  unpack from the raw results of the texture sample to the
			  final destination format.

 PARAMETERS	: psState			- Compiler state.
			  psDrInst			- Instruction representing the texture sample.
			  uChanMask			- Write mask for the input texture sample instruction.
			  uDestCount		- Count of destinations in the final format.
			  asDest			- Array of destinations in the final format.
			  auDestMask		- Mask of channels to write in each destinations.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uSmpDestStart = GetNextRegisterCount(psState, UF_MAX_CHUNKS_PER_TEXTURE);
	IMG_UINT32  uLastSetArg;	
	PINST		psSmpUnpackInst;
	IMG_UINT32	uDestIdx;

	psSmpUnpackInst = AllocateInst(psState, IMG_NULL);	
	SetOpcodeAndDestCount(psState, psSmpUnpackInst, ISMPUNPACK_USP, uDestCount);
	psSmpUnpackInst->u.psSmpUnpack->uChanMask = uChanMask;
	psSmpUnpackInst->u.psSmpUnpack->uSmpID = (psState->uNextSmpID)++;
	psSmpUnpackInst->u.psSmpUnpack->psTextureSample = psDrInst;
	psDrInst->u.psSmp->sUSPSample.uSmpID = psSmpUnpackInst->u.psSmpUnpack->uSmpID;
	psDrInst->u.psSmp->sUSPSample.psSampleUnpack = psSmpUnpackInst;

	for (uDestIdx = 0; uDestIdx < UF_MAX_CHUNKS_PER_TEXTURE; uDestIdx++)
	{
		psDrInst->asDest[uDestIdx].uType = USEASM_REGTYPE_TEMP;
		psDrInst->asDest[uDestIdx].uNumber = uSmpDestStart + uDestIdx;
		psDrInst->asDest[uDestIdx].eFmt = UF_REGFORMAT_F32;
		psDrInst->auDestMask[uDestIdx] = USC_XYZW_CHAN_MASK;

		psSmpUnpackInst->asArg[uDestIdx].uType = USEASM_REGTYPE_TEMP;
		psSmpUnpackInst->asArg[uDestIdx].uNumber = uSmpDestStart + uDestIdx;
		psSmpUnpackInst->asArg[uDestIdx].eFmt = UF_REGFORMAT_F32;
	}
	uLastSetArg = uDestIdx;
	{
		IMG_UINT32	uTempsForDirectSmp = GetNextRegisterCount(psState, uDestCount);
		for (uDestIdx = 0; uDestIdx < psSmpUnpackInst->uDestCount; uDestIdx++)
		{						
			psDrInst->asDest[uLastSetArg + uDestIdx].uType = USEASM_REGTYPE_TEMP;
			psDrInst->asDest[uLastSetArg + uDestIdx].uNumber = uTempsForDirectSmp + uDestIdx;
			psDrInst->asDest[uLastSetArg + uDestIdx].eFmt = UF_REGFORMAT_UNTYPED;
			psDrInst->auDestMask[uLastSetArg + uDestIdx] = USC_XYZW_CHAN_MASK;

			psSmpUnpackInst->asDest[uDestIdx] = asDest[uDestIdx];
			if (auDestMask != NULL)
			{
				psSmpUnpackInst->auDestMask[uDestIdx] = auDestMask[uDestIdx];
			}

			psSmpUnpackInst->asArg[uLastSetArg + uDestIdx].uType = USEASM_REGTYPE_TEMP;
			psSmpUnpackInst->asArg[uLastSetArg + uDestIdx].uNumber = uTempsForDirectSmp + uDestIdx;
			psSmpUnpackInst->asArg[uLastSetArg + uDestIdx].eFmt = UF_REGFORMAT_UNTYPED;
		}
	}	

	return psSmpUnpackInst;
}
#endif /* defined(OUTPUT_USPBIN) */

static
IMG_VOID BaseEmitSampleInstructionWithState(PINTERMEDIATE_STATE	psState,
											PCODEBLOCK			psCodeBlock,
											LOD_MODE			eLODMode,
											IMG_BOOL			bInsideDynamicFlowControl,
											IMG_UINT32			uDestCount,
											PARG				asDest,
											IMG_UINT32			auDestMask[],
											IMG_UINT32			uSamplerIdx,
											IMG_UINT32			uFirstPlane,
											IMG_UINT32			uTextureDimensionality,
											IMG_BOOL			bUsePCF,
											PSAMPLE_COORDINATES	psCoordinates,
											PARG				psTextureState,
											IMG_UINT32			uPlaneCount,
											SAMPLE_RESULT_CHUNK	asPlanes[],
											PSMP_IMM_OFFSETS	psImmOffsets,
											PSAMPLE_LOD_ADJUST	psLODAdjust,
											PSAMPLE_GRADIENTS	psGradients,
											PSAMPLE_PROJECTION	psProjection,
											SMP_RETURNDATA		eReturnData,
											IMG_BOOL			bUSPSample,
											UF_REGFORMAT		eTexPrecision,
											IMG_UINT32			uChanMask,
											IMG_UINT32			uChanSwizzle,
											IMG_BOOL			bSampleIdxPresent,
											PARG				psSampleIdx)
/*****************************************************************************
 FUNCTION	: BaseEmitSampleInstructionWithState
    
 PURPOSE	: Emit an intermediate texture sample instruction specifying the
			  texture state directly.

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block to insert the intermediate instruction
							into.
			  eLODMode		- LOD calculation mode to use for the texture sample.
			  bInsideDynamicFlowControl
							- Is this texture sample inside dynamic flow control.
			  uDestCount	- Count of registers written by the texture sample.
			  asDest		- Array of intermediate registers to write with the
							results of the texture sample.
			  auDestMask
			  uSampleIdx	- Index of the texture to sample.
			  uTextureDimensionality
							- Number of dimensions for the texture to sample.
			  bUsePCF		- Does this texture sample use PCF.
			  psCoordinates	- The registers containing the texture coordinates
							for the sample.
			  psTextureState 
							- The registers containing the texture state for the
							sample.
			  psLODAdjust	- The registers containing the LOD bias/replace for
							the sample.
			  psGradients	- The registers containing the gradients for the
							sample.
			  psProjection	- Information about any projection to be applied to the
							texture coordinates.
			  eReturnData	- The type of data to return for the sample.
			  bUSPSample	- TRUE if this is a USP texture sample.
			  eTexPrecision	- For USP samples: the declared maximum precision
							of the texture.
			  uChanMask		- For USP samples: the mask of channels of the
							destination format written by the instruction.
			  uChanSwizzle	- For USP samples: the swizzle to apply to the result
							of the texture sample.
			  bSampleIdxPresent
							- Sample Idx. update present or not.
			  psSampleIdx	- Sample Idx. update related info.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	IMG_UINT32	uDestIdx;
	PINST		psDrInst;
	IOPCODE		eOpcode;
	PINST		psSmpUnpackInst;
	#if defined(OUTPUT_USCHW)
	static const IOPCODE	aeNonUSPOpcodes[] = 
	{
		ISMP,				/* LOD_MODE_NORMAL */
		ISMPBIAS,			/* LOD_MODE_BIAS */
		ISMPREPLACE,		/* LOD_MODE_REPLACE */
		ISMPGRAD,			/* LOD_MODE_GRADIENTS */
	};
	#endif /* defined(OUTPUT_USCHW) */
	#if defined(OUTPUT_USPBIN)
	static const IOPCODE	aeUSPOpcodes[] = 
	{
		ISMP_USP,			/* LOD_MODE_NORMAL */
		ISMPBIAS_USP,		/* LOD_MODE_BIAS */
		ISMPREPLACE_USP,	/* LOD_MODE_REPLACE */
		ISMPGRAD_USP,		/* LOD_MODE_GRADIENTS */
	};
	#endif /* defined(OUTPUT_USPBIN) */

	if (bUSPSample)
	{
		#if defined(OUTPUT_USPBIN)
		ASSERT(eLODMode < (sizeof(aeUSPOpcodes) / sizeof(aeUSPOpcodes[0])));
		eOpcode = aeUSPOpcodes[eLODMode];
		#else /* defined(OUTPUT_USPBIN) */
		imgabort();
		#endif /* defined(OUTPUT_USPBIN) */
	}
	else
	{
		#if defined(OUTPUT_USCHW)
		ASSERT(eLODMode < (sizeof(aeNonUSPOpcodes) / sizeof(aeNonUSPOpcodes[0])));
		eOpcode = aeNonUSPOpcodes[eLODMode];
		#else /* defined(OUTPUT_USCHW) */
		imgabort();
		#endif /* defined(OUTPUT_USCHW) */
	}
	
	psState->uOptimizationHint |= (USC_COMPILE_HINT_DETERMINE_NONANISO_TEX | USC_COMPILE_HINT_OPTIMISE_USP_SMP);

	/* Generate a dependent read. */
	psDrInst = AllocateInst(psState, IMG_NULL);
	if (!bUSPSample)
	{
		SetOpcodeAndDestCount(psState, psDrInst, eOpcode, uDestCount);
	}
	else
	{
		SetOpcodeAndDestCount(psState, psDrInst, eOpcode, UF_MAX_CHUNKS_PER_TEXTURE + uDestCount);
	}
	psDrInst->u.psSmp->uTextureStage = uSamplerIdx;
	psDrInst->u.psSmp->uFirstPlane = uFirstPlane;
	psDrInst->u.psSmp->uCoordSize = psCoordinates->uCoordSize;
	psDrInst->u.psSmp->bUsesPCF = bUsePCF;
	psDrInst->u.psSmp->uDimensionality = uTextureDimensionality;
	psDrInst->u.psSmp->uUsedCoordChanMask = psCoordinates->uUsedCoordChanMask;
	psDrInst->u.psSmp->eReturnData = eReturnData;
	psDrInst->u.psSmp->bInsideDynamicFlowControl = bInsideDynamicFlowControl;
	psDrInst->u.psSmp->bTextureArray = psCoordinates->bTextureArray;
	psDrInst->u.psSmp->uPlaneCount = uPlaneCount;
	if (asPlanes != NULL)
	{
		memcpy(psDrInst->u.psSmp->asPlanes, 
			   asPlanes, 
			   uPlaneCount * sizeof(psDrInst->u.psSmp->asPlanes[0]));
	}
	else
	{
		ASSERT(uPlaneCount == 1);
		psDrInst->u.psSmp->asPlanes[0].uSizeInRegs = uDestCount;
		psDrInst->u.psSmp->asPlanes[0].uSizeInDwords = uDestCount;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		psDrInst->u.psSmp->asPlanes[0].bVector = IMG_FALSE;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		psDrInst->u.psSmp->asPlanes[0].eFormat = USC_CHANNELFORM_INVALID;
	}
	if (psImmOffsets != NULL)
	{
		psDrInst->u.psSmp->sImmOffsets = *psImmOffsets;
	}
	else
	{
		psDrInst->u.psSmp->sImmOffsets.bPresent = IMG_FALSE;
	}
	psDrInst->u.psSmp->bSampleIdxPresent = bSampleIdxPresent;

	#if defined(OUTPUT_USPBIN)
	if (bUSPSample)
	{
		psDrInst->u.psSmp->sUSPSample.bNonDependent = IMG_FALSE;
		psDrInst->u.psSmp->sUSPSample.eTexPrecision = eTexPrecision;
		psDrInst->u.psSmp->sUSPSample.uChanMask		= uChanMask;
		psDrInst->u.psSmp->sUSPSample.uChanSwizzle	= uChanSwizzle;
	}
	#else /* defined(OUTPUT_USPBIN) */
	PVR_UNREFERENCED_PARAMETER(eTexPrecision);
	PVR_UNREFERENCED_PARAMETER(uChanMask);
	PVR_UNREFERENCED_PARAMETER(uChanSwizzle);
	#endif /* defined(OUTPUT_USPBIN) */

	if(!bUSPSample)
	{
		for (uDestIdx = 0; uDestIdx < psDrInst->uDestCount; uDestIdx++)
		{
			psDrInst->asDest[uDestIdx] = asDest[uDestIdx];
			if (auDestMask != NULL)
			{
				psDrInst->auDestMask[uDestIdx] = auDestMask[uDestIdx];
			}
		}
		psSmpUnpackInst = IMG_NULL;
	}
	#if defined(OUTPUT_USPBIN)
	else
	{
		psSmpUnpackInst = CreateUSPSampleUnpackInstruction(psState, psDrInst, uChanMask, uDestCount, asDest, auDestMask);		
	}
	#endif /* defined(OUTPUT_USPBIN) */
			
	for (uArg = 0; uArg < psCoordinates->uCoordSize; uArg++)
	{
		psDrInst->asArg[SMP_COORD_ARG_START + uArg].uType = psCoordinates->uCoordType;
		psDrInst->asArg[SMP_COORD_ARG_START + uArg].uNumber = psCoordinates->uCoordStart + uArg;
		psDrInst->asArg[SMP_COORD_ARG_START + uArg].eFmt = psCoordinates->eCoordFormat;
	}
	for (; uArg < SMP_MAX_COORD_SIZE; uArg++)
	{
		psDrInst->asArg[SMP_COORD_ARG_START + uArg].uType = USEASM_REGTYPE_IMMEDIATE;
		psDrInst->asArg[SMP_COORD_ARG_START + uArg].uNumber = 0;
	}

	if (psCoordinates->bTextureArray)
	{
		psDrInst->asArg[SMP_ARRAYINDEX_ARG].uType = USEASM_REGTYPE_TEMP;
		psDrInst->asArg[SMP_ARRAYINDEX_ARG].uNumber = psCoordinates->uArrayIndexTemp;
	}
	else
	{
		SetSrcUnused(psState, psDrInst, SMP_ARRAYINDEX_ARG);
	}

	/* Copy the information about projection. */
	if (psProjection != NULL && psProjection->bProjected)
	{
		psDrInst->u.psSmp->bProjected = IMG_TRUE;
		psDrInst->asArg[SMP_PROJ_ARG] = psProjection->sProj;
		psDrInst->u.psSmp->uProjChannel = psProjection->uProjChannel;
		psDrInst->u.psSmp->uProjMask = psProjection->uProjMask;
	}
	else
	{
		psDrInst->u.psSmp->bProjected = IMG_FALSE;
		InitInstArg(&psDrInst->asArg[SMP_PROJ_ARG]);
		psDrInst->asArg[SMP_PROJ_ARG].uType = USC_REGTYPE_UNUSEDSOURCE;
		psDrInst->u.psSmp->uProjChannel = USC_UNDEF;
		psDrInst->u.psSmp->uProjMask = USC_UNDEF;
	}

	if (psTextureState != NULL)
	{
		for (uArg = 0; uArg < psState->uTexStateSize; uArg++)
		{
			psDrInst->asArg[SMP_STATE_ARG_START + uArg] = psTextureState[uArg];
		}
	}
	else if (bUSPSample)
	{
		for (uArg = 0; uArg < SMP_MAX_STATE_SIZE; uArg++)
		{
			psDrInst->asArg[SMP_STATE_ARG_START + uArg].uType = USEASM_REGTYPE_IMMEDIATE;
		}
	}
	else
	{
		for (uArg = 0; uArg < SMP_MAX_STATE_SIZE; uArg++)
		{
			psDrInst->asArg[SMP_STATE_ARG_START + uArg].uType = USC_REGTYPE_UNUSEDSOURCE;
		}
	}
			
	psDrInst->asArg[SMP_DRC_ARG_START].uType = USEASM_REGTYPE_DRC;
	psDrInst->asArg[SMP_DRC_ARG_START].uNumber = 0;

	if (psLODAdjust != NULL && psLODAdjust->bLODAdjustment)
	{
		psDrInst->asArg[SMP_LOD_ARG_START].uType = USEASM_REGTYPE_TEMP;
		psDrInst->asArg[SMP_LOD_ARG_START].uNumber = psLODAdjust->uLODTemp;
		psDrInst->asArg[SMP_LOD_ARG_START].eFmt = psLODAdjust->eLODFormat;
	}
	else
	{
		psDrInst->asArg[SMP_LOD_ARG_START].uType = USEASM_REGTYPE_IMMEDIATE;
		psDrInst->asArg[SMP_LOD_ARG_START].uNumber = 0;
	}

	SetSampleGradientsDetails(psState, psDrInst, psGradients);

	if (bSampleIdxPresent)
	{
		psDrInst->asArg[SMP_SAMPLE_IDX_ARG_START] = *psSampleIdx;
	}
	else
	{
		psDrInst->asArg[SMP_SAMPLE_IDX_ARG_START].uType = USC_REGTYPE_UNUSEDSOURCE;
	}

	AppendInst(psState, psCodeBlock, psDrInst);
	#if defined(OUTPUT_USPBIN)
	if (bUSPSample)
	{
		AppendInst(psState, psCodeBlock, psSmpUnpackInst);
	}
	#endif /* defined(OUTPUT_USPBIN) */
}

static IMG_UINT32 GetSampleParametersSource(UF_OPCODE eOpCode)
/*****************************************************************************
 FUNCTION	: GetSampleParametersSource
    
 PURPOSE	: Get the offset of the input texture sample instruction source
			  which contains immediate parameters affecting the sample.

 PARAMETERS	: eOpCode		- Texture sample type.

 RETURNS	: The offset of the source argument.
*****************************************************************************/
{
	if (eOpCode == UFOP_LDD || eOpCode == UFOP_LD2DMS)
	{
		return 4;
	}
	else if (eOpCode == UFOP_LDB || eOpCode == UFOP_LDL || eOpCode == UFOP_LDC || eOpCode == UFOP_LDCLZ)
	{
		return 3;
	}
	else
	{
		return 2;
	}
}

#if defined(OUTPUT_USCHW)
/*****************************************************************************
 FUNCTION	: SelectTextureOutputFormat

 PURPOSE	: Selects the texture output format.

 PARAMETERS	: psState			- Compiler state.
			  uSamplerIdx		- Sampler to select for.
			  eResultPrecision	- Precision of the texture load instruction.

 RETURNS	: None.
*****************************************************************************/
static
IMG_VOID SelectTextureOutputFormat(PINTERMEDIATE_STATE		psState,
								   IMG_UINT32				uSamplerIdx,
								   UF_REGFORMAT				eResultPrecision)
{
	PUNIFLEX_TEXTURE_PARAMETERS	psTexParams = &psState->psSAOffsets->asTextureParameters[uSamplerIdx];
	PUNIFLEX_TEXFORM			psTexForm = &psTexParams->sFormat;
	IMG_BOOL					bAllU8, bAllNorm, bAllUnnorm;
	IMG_UINT32					uChan;
	IMG_UINT32					uExcludedFormats;
	IMG_UINT32					uLowerPrecisionFormats;
	IMG_UINT32					uIdx;
	static const struct
	{
		IMG_UINT32						uFeatureFlag;
		UNIFLEX_TEXTURE_UNPACK_FORMAT	eFormat;
	} asConversionSupport[] =
	{
		{SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_C10, UNIFLEX_TEXTURE_UNPACK_FORMAT_C10},
		{SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_F16, UNIFLEX_TEXTURE_UNPACK_FORMAT_F16},
		{SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_F32, UNIFLEX_TEXTURE_UNPACK_FORMAT_F32},
	};

	/*
		No choice of output formats for this core.
	*/
	if (!(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT))
	{
		psState->asTextureUnpackFormat[uSamplerIdx].eFormat = UNIFLEX_TEXTURE_UNPACK_FORMAT_NATIVE;
		psState->asTextureUnpackFormat[uSamplerIdx].bNormalise = IMG_FALSE;
		SetBit(psState->auTextureUnpackFormatSelectedMask, uSamplerIdx, 1);
		return;
	}

	/*
		Output format already selected.
	*/
	if (GetBit(psState->auTextureUnpackFormatSelectedMask, uSamplerIdx) == 1)
	{
		return;
	}

	/*
		Check the formats of the texture channels.
	*/
	bAllU8 = bAllNorm = bAllUnnorm = IMG_TRUE;
	uLowerPrecisionFormats = 0;
	uExcludedFormats = 0;
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		/*
			Are some of the channel formats in the texture result
			not U8.
		*/
		if (
				psTexForm->peChannelForm[uChan] != USC_CHANNELFORM_U8 ||
				psTexParams->bGamma
		   )
		{
			bAllU8 = IMG_FALSE;
		}

		/*
			Update whether either all of the channels in the texture
			result are normalised or all unnormalised.
		*/
		if (psTexForm->peChannelForm[uChan] == USC_CHANNELFORM_U8_UN)
		{
			bAllNorm = IMG_FALSE;
		}
		else
		{
			bAllUnnorm = IMG_FALSE;
		}

		/*
			Record which possible format conversions data types have
			a lower precision than the format of this channel.
		*/
		if (psTexParams->bGamma)
		{
			uLowerPrecisionFormats |= 
				(1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_C10);
		}
		switch (psTexForm->peChannelForm[uChan])
		{
			case USC_CHANNELFORM_U8: 
			case USC_CHANNELFORM_U8_UN:
			case USC_CHANNELFORM_S8:
			case USC_CHANNELFORM_C10:
			case USC_CHANNELFORM_ZERO:
			case USC_CHANNELFORM_ONE:
			{
				break;
			}
			case USC_CHANNELFORM_F16:
			{
				uLowerPrecisionFormats |= (1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_C10);
				break;
			}
			case USC_CHANNELFORM_U16:
			case USC_CHANNELFORM_S16:
			case USC_CHANNELFORM_F32:
			case USC_CHANNELFORM_U24:
			case USC_CHANNELFORM_DF32:
			{
				uLowerPrecisionFormats |= 
					(1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_C10) |
					(1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_F16);
				break;
			}
			case USC_CHANNELFORM_UINT8:
			case USC_CHANNELFORM_UINT16:
			case USC_CHANNELFORM_UINT32:
			case USC_CHANNELFORM_SINT8:
			case USC_CHANNELFORM_SINT16:
			case USC_CHANNELFORM_SINT32:
			case USC_CHANNELFORM_UINT16_UINT10:
			case USC_CHANNELFORM_UINT16_UINT2:
			{
				/*
					Never use the hardware's format conversion.
				*/
				uExcludedFormats |= 
					(1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_C10) |
					(1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_F16) |
					(1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_F32);
				break;
			}
			default:
			{
				imgabort();
			}
		}
	}

	/*
		Exclude format conversions not supported by the hardware.
	*/
	for (uIdx = 0; uIdx < (sizeof(asConversionSupport) / sizeof(asConversionSupport[0])); uIdx++)
	{
		if (!(psState->psTargetFeatures->ui32Flags2 & asConversionSupport[uIdx].uFeatureFlag))
		{
			uExcludedFormats |= (1U << asConversionSupport[uIdx].eFormat);
		}
	}

	/*
		If not supported by the hardware exclude format conversions to lower precision data types.
	*/
	if (!(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_LOWERPREC))
	{
		uExcludedFormats |= uLowerPrecisionFormats;
	}

	/*
		If not supported by the hardware don't do any format conversion if the texture sample result
		contains unnormalised data.
	*/
	if (!(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_TAG_UNPACK_RESULT_TO_UNORM) && !bAllNorm)
	{
		uExcludedFormats |=  
					(1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_C10) |
					(1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_F16) |
					(1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_F32);
	}

	/*
		Select an output format.	
	*/
	switch (eResultPrecision)
	{
		case UF_REGFORMAT_U8:
		{
			/*
			  Always native.
			*/
			psState->asTextureUnpackFormat[uSamplerIdx].eFormat = UNIFLEX_TEXTURE_UNPACK_FORMAT_NATIVE;
			psState->asTextureUnpackFormat[uSamplerIdx].bNormalise = IMG_FALSE;
			break;
		}
		case UF_REGFORMAT_C10:
		{
			/*
			  Use native if no unpacking is required to minimize the number of registers used.
			  Use native if both normalised and unnormalised channels to reduce the number of cases
			  the unpacking code needs to handle.
			*/
			if (
				bAllU8 || 
				(!bAllNorm && !bAllUnnorm) || 
				(uExcludedFormats & (1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_C10))
				)
			{
				psState->asTextureUnpackFormat[uSamplerIdx].eFormat = UNIFLEX_TEXTURE_UNPACK_FORMAT_NATIVE;
				psState->asTextureUnpackFormat[uSamplerIdx].bNormalise = IMG_FALSE;
			}
			else
			{
				psState->asTextureUnpackFormat[uSamplerIdx].eFormat = UNIFLEX_TEXTURE_UNPACK_FORMAT_C10;
				psState->asTextureUnpackFormat[uSamplerIdx].bNormalise = bAllNorm;
			}
			break;
		}
		case UF_REGFORMAT_F16:
		{
			/*
			  Use native if both normalised and unnormalised channels to reduce the number of cases
			  the unpacking code needs to handle.
			*/
			if (
				(!bAllNorm && !bAllUnnorm)  || 
				(uExcludedFormats & (1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_F16))
				)
			{
				psState->asTextureUnpackFormat[uSamplerIdx].eFormat = UNIFLEX_TEXTURE_UNPACK_FORMAT_NATIVE;
				psState->asTextureUnpackFormat[uSamplerIdx].bNormalise = IMG_FALSE;
			}
			else
			{
				psState->asTextureUnpackFormat[uSamplerIdx].eFormat = UNIFLEX_TEXTURE_UNPACK_FORMAT_F16;
				psState->asTextureUnpackFormat[uSamplerIdx].bNormalise = bAllNorm;
			}
			break;
		}
		case UF_REGFORMAT_F32:
		case UF_REGFORMAT_I32: /* Treat the integer formats exactly the same as F32. */
		case UF_REGFORMAT_U32:
		{
			/*
			  Use native if both normalised and unnormalised channels to reduce the number of cases
			  the unpacking code needs to handle.
			*/
			if (
				(!bAllNorm && !bAllUnnorm)  || 
				(uExcludedFormats & (1U << UNIFLEX_TEXTURE_UNPACK_FORMAT_F32))
				)
			{
				psState->asTextureUnpackFormat[uSamplerIdx].eFormat = UNIFLEX_TEXTURE_UNPACK_FORMAT_NATIVE;
				psState->asTextureUnpackFormat[uSamplerIdx].bNormalise = IMG_FALSE;
			}
			else
			{
				psState->asTextureUnpackFormat[uSamplerIdx].eFormat = UNIFLEX_TEXTURE_UNPACK_FORMAT_F32;
				psState->asTextureUnpackFormat[uSamplerIdx].bNormalise = bAllNorm;
			}
			break;
		}
		default:
		{
			imgabort();
		}
	}

	/*
	  Use the same output format for all texture loads of the same texture.
	*/
	SetBit(psState->auTextureUnpackFormatSelectedMask, uSamplerIdx, 1);
}

/*****************************************************************************
 FUNCTION	: GetSampleRegisterFormat

 PURPOSE	: Get the appropriate register format to represent the result of
			  sampling a texture.

 PARAMETERS	: psState			- Compiler state.
			  uSamplerIdx		- Sampler to get the register format for.
			  eReturnData		- Type of information returned by the sample.

 RETURNS	: The register format.
*****************************************************************************/
static UF_REGFORMAT GetSampleRegisterFormat(PINTERMEDIATE_STATE		psState,
											IMG_UINT32				uSamplerIdx,
											SMP_RETURNDATA			eReturnData)
{
	UNIFLEX_TEXTURE_UNPACK_FORMAT	eUnpackFormat;

	if (eReturnData != SMP_RETURNDATA_NORMAL)
	{
		return UF_REGFORMAT_UNTYPED;
	}

	ASSERT(uSamplerIdx < psState->psSAOffsets->uTextureCount);
	eUnpackFormat = psState->asTextureUnpackFormat[uSamplerIdx].eFormat;

	switch (eUnpackFormat)
	{
		/*
			Set the register format from the format the hardware is
			asked to unpack the texture result to.
		*/
		case UNIFLEX_TEXTURE_UNPACK_FORMAT_C10: return UF_REGFORMAT_C10;
		case UNIFLEX_TEXTURE_UNPACK_FORMAT_F16: return UF_REGFORMAT_F16;
		case UNIFLEX_TEXTURE_UNPACK_FORMAT_F32: return UF_REGFORMAT_F32;

		case UNIFLEX_TEXTURE_UNPACK_FORMAT_NATIVE:
		{
			IMG_UINT32					uChan;
			PUNIFLEX_TEXTURE_PARAMETERS	psTexParams = &psState->psSAOffsets->asTextureParameters[uSamplerIdx];
			PUNIFLEX_TEXFORM			psTexForm = &psTexParams->sFormat;

			if (!psTexParams->bGamma)
			{
				IMG_BOOL	bAllU8;
				IMG_BOOL	bAllF16;
				IMG_BOOL	bAllF32;

				bAllU8 = IMG_TRUE;
				bAllF16 = IMG_TRUE;
				bAllF32 = IMG_TRUE;
				for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
				{
					if (psTexForm->peChannelForm[uChan] == USC_CHANNELFORM_ZERO ||
						psTexForm->peChannelForm[uChan] == USC_CHANNELFORM_ONE)
					{
						continue;
					}
					if (psTexForm->peChannelForm[uChan] != USC_CHANNELFORM_U8)
					{
						bAllU8 = IMG_FALSE;
					}
					if (psTexForm->peChannelForm[uChan] != USC_CHANNELFORM_F16)
					{
						bAllF16 = IMG_FALSE;
					}
					if (psTexForm->peChannelForm[uChan] != USC_CHANNELFORM_F32)
					{
						bAllF32 = IMG_FALSE;
					}
				}

				if (bAllU8)
				{
					return UF_REGFORMAT_U8;
				}
				else if (bAllF16)
				{
					return UF_REGFORMAT_F16;
				}
				else if (bAllF32)
				{
					return UF_REGFORMAT_F32;
				}
			}

			return UF_REGFORMAT_UNTYPED;
		}
		default: imgabort();
	}
}

/*****************************************************************************
 FUNCTION	: SetupSmpStateArgument

 PURPOSE	: Set up the arguments to the SMP that contain the texture state.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInsertBeforeInst
								- Point to insert the intermediate instructions at.
			  asStateArgs		- Returns the details of the arguments containing
								the texture state.
			  uTexture			- Index of the texture to get the state for.
		      uChunk			- Index of the plane within the texture to get
								the state for.
			  bTextureArray		- TRUE if the texture is an array.
			  psTextureArrayIndex
								- Details of the unsigned 16-bit integer index
								into a texture array.
			  psImmOffsets		- Details of the immediate, texel space offsets
								to apply to the texture sample.
			  uTextureDimensionality
								- Number of dimensions of the texture.
			  bSampleIdxPresent	- Sample Idx. update present or not.
			  psSampleIdx		- Sample Idx. update related info.

 RETURNS	: None.
*****************************************************************************/
IMG_INTERNAL
IMG_VOID SetupSmpStateArgument(PINTERMEDIATE_STATE	psState,
							   PCODEBLOCK			psCodeBlock,
							   PINST				psInsertBeforeInst,
							   ARG					asStateArgs[],
							   IMG_UINT32			uTexture,
							   IMG_UINT32			uChunk,
							   IMG_BOOL				bTextureArray,
							   PARG					psTextureArrayIndex,
							   PSMP_IMM_OFFSETS		psImmOffsets,
							   IMG_UINT32			uTextureDimensionality,
							   IMG_BOOL				bSampleIdxPresent,
							   PARG					psSampleIdx)
{
	IMG_UINT32	uArg;

	if (psState->psSAOffsets->uTextureState == UINT_MAX)
	{
		/*
			Load each dword of texture state from the constant buffer.
		*/
		for (uArg = 0; uArg < psState->uTexStateSize; uArg++)
		{
			IMG_UINT32 uStateOffset;

			uStateOffset = _UNIFLEX_TEXSTATE_SAMPLERBASE(uTexture, psState->uTexStateSize);
			uStateOffset += _UNIFLEX_TEXSTATE_HWSTATE(uChunk, uArg, psState->uTexStateSize);
			
			LoadTextureState(psState, psCodeBlock, psInsertBeforeInst, uStateOffset, &asStateArgs[uArg]);
		}
		if (bTextureArray)
		{
			PARG psTexBase;
			IMG_UINT32 uStrideOffset;
			ARG sBaseAddressOfTexArray;
			ARG sTexarrayStride;

			/* SMP instruction source that points to the base address of the texture. */
			psTexBase = &asStateArgs[EURASIA_PDS_DOUTT_TEXADDR_WORD_INDEX];

			/* Base address of the texture array */
			sBaseAddressOfTexArray = *psTexBase;

			/*
			  Load the stride between elements in the texture array into a register.
			*/
			uStrideOffset = _UNIFLEX_TEXSTATE_SAMPLERBASE(uTexture, psState->uTexStateSize);
			uStrideOffset += _UNIFLEX_TEXSTATE_STRIDE(uChunk, psState->uTexStateSize);
		
			LoadTextureState(psState, psCodeBlock, psInsertBeforeInst, uStrideOffset, &sTexarrayStride);

			/*
			  Calculate

			  TEX_BASE = ARRAY_BASE + ARRAY_INDEX * ELEMENT_SIZE
			*/
			MakeNewTempArg(psState, UF_REGFORMAT_F32, psTexBase);

			GenerateIntegerArithmetic(psState,
									  psCodeBlock,
									  psInsertBeforeInst,
									  UFOP_MAD,
									  psTexBase,
									  NULL /* psDestHigh */,
									  USC_PREDREG_NONE,
									  IMG_FALSE /* bPredNegate */,
									  &sTexarrayStride,
									  IMG_FALSE /* bNegateA */,
									  psTextureArrayIndex,
									  IMG_FALSE /* bNegateB */,
									  &sBaseAddressOfTexArray,
									  IMG_FALSE /* bNegateC */,
									  IMG_FALSE /* bSigned */);
		}
		if (psImmOffsets != NULL && psImmOffsets->bPresent)
		{
			IMG_UINT32 uUVCoorOffsetBits = 0;
			PARG psTexExtendedData = &asStateArgs[EURASIA_PDS_DOUTT_EXTDDATA_WORD_INDEX];
			ARG sTexExtendedData = *psTexExtendedData;
			ASSERT(psState->uTexStateSize >= 3);
			#if defined(SUPPORT_SGX545)
			if(uTextureDimensionality >= 1)
			{
				uUVCoorOffsetBits = (((IMG_UINT32)(psImmOffsets->aiOffsets[0])) << EURASIA_PDS_DOUTT3_UOFFSET_SHIFT) & (~EURASIA_PDS_DOUTT3_UOFFSET_CLRMSK);
			}
			if(uTextureDimensionality >= 2)
			{
				uUVCoorOffsetBits = uUVCoorOffsetBits | ((((IMG_UINT32)(psImmOffsets->aiOffsets[1])) << EURASIA_PDS_DOUTT3_VOFFSET_SHIFT) & (~EURASIA_PDS_DOUTT3_VOFFSET_CLRMSK));
			}
			#else
			{
				PVR_UNREFERENCED_PARAMETER(uTextureDimensionality);
				PVR_UNREFERENCED_PARAMETER(psImmOffsets);
			}
			#endif /* defined(SUPPORT_SGX545) */
			psTexExtendedData->uType = USEASM_REGTYPE_TEMP;
			psTexExtendedData->uNumber = GetNextRegister(psState);
			{
				PINST psOrInst = AllocateInst(psState, psInsertBeforeInst);
				SetOpcode(psState, psOrInst, IOR);
				SetDest(psState, 
						psOrInst, 
						0 /* uDestIdx */, 
						USEASM_REGTYPE_TEMP, 
						psTexExtendedData->uNumber, 
						UF_REGFORMAT_F32);
				SetSrc(psState,
					   psOrInst,
					   0 /* uSrcIdx */,
					   sTexExtendedData.uType,
					   sTexExtendedData.uNumber,
					   UF_REGFORMAT_F32);
				psOrInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psOrInst->asArg[1].uNumber = uUVCoorOffsetBits;
				InsertInstBefore(psState, psCodeBlock, psOrInst, psInsertBeforeInst);
			}
		}

		#if defined(SUPPORT_SGX545)
		if (bSampleIdxPresent)
		{			
			ARG	sShiftedSampleIdx;
			ARG	sMaskedSampleIdx;

			ASSERT(psSampleIdx != NULL);
			{
				PINST	psShlInst;	

				MakeNewTempArg(psState, UF_REGFORMAT_F32, &sShiftedSampleIdx);

				psShlInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psShlInst, ISHL);
				SetDestFromArg(psState, psShlInst, 0 /* uDestIdx */, &sShiftedSampleIdx);
				SetSrcFromArg(psState, psShlInst, 0 /* uSrcIdx */, psSampleIdx);
				psShlInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psShlInst->asArg[1].uNumber = EURASIA_PDS_DOUTT3_SMPIDX_SHIFT;		
				InsertInstBefore(psState, psCodeBlock, psShlInst, psInsertBeforeInst);
			}			
			{
				PINST	psAndInst;		

				MakeNewTempArg(psState, UF_REGFORMAT_F32, &sMaskedSampleIdx);

				psAndInst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psAndInst, IAND);
				SetDestFromArg(psState, psAndInst, 0 /* uDestIdx */, &sMaskedSampleIdx);
				SetSrcFromArg(psState, psAndInst, 0 /* uSrcIdx */, &sShiftedSampleIdx);
				psAndInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psAndInst->asArg[1].uNumber = (~(EURASIA_PDS_DOUTT3_SMPIDX_CLRMSK));
				InsertInstBefore(psState, psCodeBlock, psAndInst, psInsertBeforeInst);
			}
			{
				PARG psTexExtendedData = &asStateArgs[EURASIA_PDS_DOUTT_EXTDDATA_WORD_INDEX];
				ARG sTexExtendedData;
				sTexExtendedData = *psTexExtendedData;

				MakeNewTempArg(psState, UF_REGFORMAT_F32, psTexExtendedData);

				{
					PINST psOrInst = AllocateInst(psState, psInsertBeforeInst);
					SetOpcode(psState, psOrInst, IOR);

					SetDestFromArg(psState, psOrInst, 0 /* uDestIdx */, psTexExtendedData);
					SetSrcFromArg(psState, psOrInst, 0 /* uSrcIdx */, &sTexExtendedData);
					SetSrcFromArg(psState, psOrInst, 1 /* uSrcIdx */, &sMaskedSampleIdx);

					InsertInstBefore(psState, psCodeBlock, psOrInst, psInsertBeforeInst);
				}
			}		
		}
		#else
		PVR_UNREFERENCED_PARAMETER(bSampleIdxPresent);
		PVR_UNREFERENCED_PARAMETER(psSampleIdx);
		#endif /* (SUPPORT_SGX545) */
		
		/*
		  Set the remaining state arguments to dummy values.
		*/
		for (; uArg < SMP_MAX_STATE_SIZE; uArg++)
		{
			PARG psArg = &asStateArgs[uArg];

			psArg->uType = USEASM_REGTYPE_IMMEDIATE;
			psArg->uNumber = 0;
		}
	}
	else
	{
		IMG_UINT32	uSecAttrNum;

		/*
		  Get the start of the secondary attributes that contain the texture state.
		*/
		uSecAttrNum = EncodeTextureStateRegNum(psState, uTexture, uChunk);

		/*
		  Set up the SMP sources to point to the secondary attributes.
		*/
		for (uArg = 0; uArg < psState->uTexStateSize; uArg++)
		{
			InitInstArg(&asStateArgs[uArg]);
			asStateArgs[uArg].uType = USEASM_REGTYPE_SECATTR;
			asStateArgs[uArg].uNumber = uSecAttrNum + uArg;
		}
	}
}

IMG_INTERNAL
IMG_VOID GetTextureSampleChannelLocation(PINTERMEDIATE_STATE	psState,
										 PSAMPLE_RESULT_LAYOUT	psResultLayout,
										 PSAMPLE_RESULT			psResult,
										 IMG_UINT32				uSrcChan,
										 USC_CHANNELFORM*		peFormat,
										 IMG_PUINT32			puSrcRegNum,
										 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
										 IMG_PBOOL				pbSrcIsVector,
										 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
										 IMG_PUINT32			puSrcChanOffset)
/*****************************************************************************
 FUNCTION	: GetTextureSampleChannelLocation
    
 PURPOSE	: Get the location of the packed data for a channel in a texture
			  sample result.

 PARAMETERS	: psResultLayout	- Information about the packing of the texture
								sample result.
			  psResult			- Details of the variable holding the packed
								result of the texture sample.
			  uSrcChan			- Channel to get the location for.
			  peFormat			- Returns the format of the packed data.
			  puSrcRegNum		- Returns the number of the register holding
								the packed data.
			  puSrcChanOffset	- Returns the offset (in either bytes or C10 channels) 
								of the packed data in the register.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PSAMPLE_RESULT_LOCATION	psLocation;

	/*
		Set default return values.
	*/
	*puSrcRegNum = USC_UNDEF;
	*puSrcChanOffset = USC_UNDEF;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (pbSrcIsVector != NULL)
	{
		*pbSrcIsVector = IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	ASSERT(uSrcChan < psResultLayout->uChanCount);
	psLocation = &psResultLayout->asChannelLocation[uSrcChan];

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psLocation->bVector)
	{
		ASSERT(pbSrcIsVector != NULL);
		*pbSrcIsVector = IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	*peFormat = psLocation->eFormat;
	if (psLocation->eFormat == USC_CHANNELFORM_ZERO ||
		psLocation->eFormat == USC_CHANNELFORM_ONE)
	{
		return;
	}

	ASSERT(psLocation->uChunkIdx < UF_MAX_CHUNKS_PER_TEXTURE);
	*puSrcRegNum = psResult->auRawDataPerChunk[psLocation->uChunkIdx];

	ASSERT(psLocation->uRegOffsetInChunk < psResultLayout->asChunk[psLocation->uChunkIdx].uSizeInRegs);
	*puSrcRegNum += psLocation->uRegOffsetInChunk;

	*puSrcChanOffset = psLocation->uChanOffset;
}

static
IMG_UINT32 CombineInputSwizzles(IMG_UINT32 uSwizzle1, IMG_UINT32 uSwizzle2)
/*****************************************************************************
 FUNCTION	: CombineInputSwizzles
    
 PURPOSE	: Generate a swizzle which is the combined effect of two other swizzles.

 PARAMETERS	: uSwizzle1		- First swizzle to be applied.
			  uSwizzle2		- Second swizzle to be applied.

 RETURNS	: The combined swizzle.
*****************************************************************************/
{
	IMG_UINT32	uChan;
	IMG_UINT32	uCombinedSwizzle;

	uCombinedSwizzle = 0;
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		IMG_UINT32	uSrcChan;

		uSrcChan = EXTRACT_CHAN(uSwizzle2, uChan);
		uSrcChan = EXTRACT_CHAN(uSwizzle1, uSrcChan);
		uCombinedSwizzle |= (uSrcChan << (uChan * 3));
	}
	return uCombinedSwizzle;
}

static
IMG_VOID ConvertSwizzleSelToChannelLocation(PINTERMEDIATE_STATE psState, PSAMPLE_RESULT_LOCATION psChannel, IMG_UINT32 uSel)
/*****************************************************************************
 FUNCTION	: ConvertSwizzleSelToChannelLocation
    
 PURPOSE	: Create a description of the contents of a channel in the result
			  of a texture sample instruction corresponding to a constant
			  swizzle selector.

 PARAMETERS	: psState		- Compiler state.
			  psChannel		- Channel to setup.
			  uSel			- Swizzle selector.

 RETURNS	: Nothing.
*****************************************************************************/
{
	psChannel->uChunkIdx = USC_UNDEF;
	psChannel->uRegOffsetInChunk = USC_UNDEF;
	psChannel->uChanOffset = USC_UNDEF;
	if (uSel == UFREG_SWIZ_0)
	{
		psChannel->eFormat = USC_CHANNELFORM_ZERO;
	}
	else
	{
		ASSERT(uSel == UFREG_SWIZ_1);
		psChannel->eFormat = USC_CHANNELFORM_ONE;
	}
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	psChannel->bVector = IMG_FALSE;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
typedef enum _HARDWARE_FILTER_MODE
{
	/* Only one sample needed so don't use IRSD. */
	HARDWARE_FILTER_MODE_POINT,
	/* Four samples/coefficents returned. */
	HARDWARE_FILTER_MODE_BILINEAR,
	/* Eight samples/coefficents returned. */
	HARDWARE_FILTER_MODE_TRILINEAR,
} HARDWARE_FILTER_MODE, *PHARDWARE_FILTER_MODE;

static
HARDWARE_FILTER_MODE GetFilterMode(PINTERMEDIATE_STATE psState, PUNIFLEX_TEXTURE_PARAMETERS	psParameters)
/*****************************************************************************
 FUNCTION	: GetFilterMode

 PURPOSE	: Map the complete texture filtering parameters to the type of data
			  returned from an SMP instruction using the .RSD/.IRSD mode.

 PARAMETERS	: psState			- Compiler state.
			  psParameters		- Texture filtering parameters.

 RETURNS	: The filtering mode.
*****************************************************************************/
{
	/*
		If we are emulating PCF then the driver must have supplied accurate information
		about the filtering modes used with the texture.
	*/
	ASSERT(psParameters->eTrilinearType != UNIFLEX_TEXTURE_TRILINEAR_TYPE_UNSPECIFIED);
	ASSERT(psParameters->eMinFilter != UNIFLEX_TEXTURE_FILTER_TYPE_UNSPECIFIED);
	ASSERT(psParameters->eMagFilter != UNIFLEX_TEXTURE_FILTER_TYPE_UNSPECIFIED);

	/*
		The hardware doesn't support returning the raw sample/coefficent inputs to the texture filter
		for anisotropic filtering. We could emulate the entire aniso calculation on the USE but for
		now just fail.
	*/
	ASSERT(psParameters->eMinFilter != UNIFLEX_TEXTURE_FILTER_TYPE_ANISOTROPIC);
	ASSERT(psParameters->eMagFilter != UNIFLEX_TEXTURE_FILTER_TYPE_ANISOTROPIC);

	if (psParameters->eTrilinearType == UNIFLEX_TEXTURE_TRILINEAR_TYPE_ON)
	{
		return HARDWARE_FILTER_MODE_TRILINEAR;
	}
	else
	{
		ASSERT(psParameters->eTrilinearType == UNIFLEX_TEXTURE_TRILINEAR_TYPE_OFF);

		if (psParameters->eMinFilter == UNIFLEX_TEXTURE_FILTER_TYPE_LINEAR ||
			psParameters->eMagFilter == UNIFLEX_TEXTURE_FILTER_TYPE_LINEAR)
		{
			return HARDWARE_FILTER_MODE_BILINEAR;
		}
		else
		{
			return HARDWARE_FILTER_MODE_POINT;
		}
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID CreateDummyResultLayout(PINTERMEDIATE_STATE	psState,
								 PSAMPLE_RESULT_LAYOUT	psResultLayout,
								 PSAMPLE_RESULT			psResult,
								 IMG_UINT32				uStartReg,
								 IMG_UINT32				uSamplerSwizzle,
								 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
								 IMG_BOOL				bVector,
								 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								 IMG_UINT32				uRegisterCount)
/*****************************************************************************
 FUNCTION	: CreateDummyResultLayout

 PURPOSE	: Set up information about the format/packing of the result of a
			  texture sample where the result is a set of F32 values.

 PARAMETERS	: psState			- Compiler state.
			  psResultLayout	- Written with information about the format
								of the result of the texture sample.
			  psResult			- Written with variables allocated for the
								result of the texture sample.
			  uStartReg			- Start of the registers containing the
								result of the texture sample.
			  uSamplerSwizzle	- Swizzle on the sampler parameter to the
								texture sample instruction.
			  uRegisterCount	- Count of values in the result.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChunk;
	IMG_UINT32	uChan;

	/*
		Setup one chunk containing all data from the result.
	*/
	psResultLayout->uChunkCount = 1;
	psResultLayout->uTexelSizeInBytes = USC_UNDEF;
	psResultLayout->asChunk[0].uSizeInRegs = uRegisterCount;
	psResultLayout->asChunk[0].uSizeInDwords = uRegisterCount;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	psResultLayout->asChunk[0].bVector = bVector;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	psResult->auRawDataPerChunk[0] = uStartReg;

	/*
		Set all the other chunks to undefined.
	*/
	for (uChunk = 1; uChunk < UF_MAX_CHUNKS_PER_TEXTURE; uChunk++)
	{
		psResultLayout->asChunk[0].uSizeInRegs = USC_UNDEF;
		psResultLayout->asChunk[0].uSizeInDwords = USC_UNDEF;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		psResultLayout->asChunk[0].bVector = IMG_FALSE;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		psResult->auRawDataPerChunk[uChunk] = USC_UNDEF;
	}

	/*	
		Set up the swizzle to replicate the defined channels to all channels.
	*/
	ASSERT(uRegisterCount > 0);
	ASSERT(uRegisterCount <= CHANNELS_PER_INPUT_REGISTER);

	psResultLayout->uChanCount = CHANNELS_PER_INPUT_REGISTER;
	psResultLayout->asChannelLocation = 
		UscAlloc(psState, sizeof(psResultLayout->asChannelLocation[0]) * psResultLayout->uChanCount);
	
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		IMG_UINT32	uSrcChan;

		uSrcChan = EXTRACT_CHAN(uSamplerSwizzle, uChan);
		if (uSrcChan <= UFREG_SWIZ_A && uSrcChan >= uRegisterCount)
		{
			uSrcChan = uRegisterCount - 1;
		}

		if (uSrcChan <= UFREG_SWIZ_A)
		{
			psResultLayout->asChannelLocation[uChan].uChunkIdx = 0;
			psResultLayout->asChannelLocation[uChan].uRegOffsetInChunk = uSrcChan;
			psResultLayout->asChannelLocation[uChan].uChanOffset = 0;
			psResultLayout->asChannelLocation[uChan].eFormat = USC_CHANNELFORM_F32;
			#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
			psResultLayout->asChannelLocation[uChan].bVector = bVector;
			#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		}
		else
		{
			ConvertSwizzleSelToChannelLocation(psState, &psResultLayout->asChannelLocation[uChan], uSrcChan);
		}
	}
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID CreateRawSamplesTextureResultLayout(PINTERMEDIATE_STATE		psState,
											 PSAMPLE_RESULT_LAYOUT		psLayout,
											 IMG_UINT32					uSamplerIdx)
/*****************************************************************************
 FUNCTION	: CreateRawSamplesTextureResultLayout

 PURPOSE	: Update the details of how to unpack the result of a texture
			  result for texture sample returning prefiltered bilinear/trilinear
			  samples.

 PARAMETERS	: psState			- Compiler state.
			  psLayout			- Default layout.
			  uSamplerIdx		- Identifies the texture to be sampled.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32					auPerSampleSize[UF_MAX_CHUNKS_PER_TEXTURE];
	IMG_UINT32					uChan;
	HARDWARE_FILTER_MODE		eFilterMode;
	IMG_UINT32					uSliceCount;
	PUNIFLEX_TEXTURE_PARAMETERS	psTextureParams = &psState->psSAOffsets->asTextureParameters[uSamplerIdx];
	PUNIFLEX_DIMENSIONALITY		psDim = &psState->psSAOffsets->asTextureDimensionality[uSamplerIdx];
	IMG_UINT32					uSampleCount;
	IMG_UINT32					uSample;
	IMG_UINT32					uChunk;

	if (psDim->eType == UNIFLEX_DIMENSIONALITY_TYPE_3D)
	{
		uSliceCount = 2;
	}
	else
	{
		uSliceCount = 1;
	}

	eFilterMode = GetFilterMode(psState, psTextureParams);
	if (eFilterMode == HARDWARE_FILTER_MODE_POINT)
	{
		/*
			For point filtering we don't need to do anything special: the hardware
			is returning a single, unfiltered sample anyway.
		*/
		return;
	}

	switch (eFilterMode)
	{
		case HARDWARE_FILTER_MODE_BILINEAR: 
		{
			/* Four/eight samples from one mipmap level. */
			uSampleCount = 4 * uSliceCount;
			break;
		}
		case HARDWARE_FILTER_MODE_TRILINEAR:
		{
			/* Eight/sixteen samples from each of two mipmap levels. */
			uSampleCount = 8 * uSliceCount;
			break;
		}
		default: imgabort();
	}
	
	/*
		Grow the size of the data returned by the sample instruction.
	*/
	for (uChunk = 0; uChunk < psLayout->uChunkCount; uChunk++)
	{
		auPerSampleSize[uChunk] = psLayout->asChunk[uChunk].uSizeInDwords;
		psLayout->asChunk[uChunk].uSizeInDwords *= uSampleCount;
		psLayout->asChunk[uChunk].uSizeInRegs *= uSampleCount;
	}
	
	ASSERT(psLayout->uChanCount == CHANNELS_PER_INPUT_REGISTER);
	psLayout->uChanCount *= uSampleCount;
	
	ResizeTypedArray(psState, 
					 psLayout->asChannelLocation, 
					 CHANNELS_PER_INPUT_REGISTER, 
					 CHANNELS_PER_INPUT_REGISTER * uSampleCount);
	for (uSample = 1; uSample < uSampleCount; uSample++)
	{
		IMG_UINT32	uChanBase = uSample * CHANNELS_PER_INPUT_REGISTER;
		for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
		{
			PSAMPLE_RESULT_LOCATION	psChan = &psLayout->asChannelLocation[uChanBase + uChan];
			IMG_UINT32				uPerSampleSize;

			*psChan = psLayout->asChannelLocation[uChan];

			ASSERT(psChan->uChunkIdx < psLayout->uChunkCount);
			uPerSampleSize = auPerSampleSize[psChan->uChunkIdx];

			psChan->uRegOffsetInChunk += uPerSampleSize * uSample;
		}
	}
}

static
IMG_VOID CreateGather4TextureResultLayout(PINTERMEDIATE_STATE		psState,
										  PSAMPLE_RESULT_LAYOUT		psLayout)
/*****************************************************************************
 FUNCTION	: CreateGather4TextureResultLayout

 PURPOSE	: Update the details of how to unpack the result of a texture
			  result for the special data return mode used for LDGATHER4.

 PARAMETERS	: psState			- Compiler state.
			  psLayout			- Default layout.

 RETURNS	: None.
*****************************************************************************/
{
	PSAMPLE_RESULT_LOCATION	psSourceChan;
	IMG_UINT32				uPerSampleSize;
	IMG_UINT32				uChan;
	/*
		The hardware returns

				X				Y,				Z,			W
			(top-left,		top-right,		bottom-left,	bottom-right)

		but the input instruction is defined as returning the data in the following order
			
			(bottom-left,	bottom-right,	top-right,		top-left)

		so apply a swizzle when unpacking.
	*/
	static const IMG_UINT32	auSwizzle[] = {USC_Z_CHAN, USC_W_CHAN, USC_Y_CHAN, USC_X_CHAN};

	/*
		For gather4 we can assume the driver has set up the following filtering parameters:-
			Minification filter: Linear
			Magnification filter: Linear
			Filtering between mipmaps: Off
	*/
	
	/*
		Get the source data for the X channel.
	*/
	ASSERT(psLayout->uChanCount == CHANNELS_PER_INPUT_REGISTER);
	psSourceChan = &psLayout->asChannelLocation[USC_X_CHAN];

	/*
		Sample only the plane corresponding to the X channel.
	*/
	psLayout->asChunk[0] = psLayout->asChunk[psSourceChan->uChunkIdx];
	psLayout->uChunkCount = 1;

	/*
		Data for four bilinear samples is returned by the sample instruction.
	*/
	uPerSampleSize = psLayout->asChunk[0].uSizeInDwords;
	psLayout->asChunk[0].uSizeInDwords *= 4;
	psLayout->asChunk[0].uSizeInRegs *= 4;
	
	/*
		Copy to each channel in the destination from the X channel of the corresponding bilinear sample.
	*/
	for (uChan = USC_Y_CHAN; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		PSAMPLE_RESULT_LOCATION	psChan = &psLayout->asChannelLocation[uChan];

		*psChan = *psSourceChan;
	}
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		PSAMPLE_RESULT_LOCATION	psChan = &psLayout->asChannelLocation[uChan];

		psChan->uRegOffsetInChunk += uPerSampleSize * auSwizzle[uChan];
	}
}

static
IMG_VOID SetupSINFChannelLocation(PINTERMEDIATE_STATE		psState, 
								  PSAMPLE_RESULT_LOCATION	psChannel, 
								  IMG_UINT32				uBitOffset,
								  IMG_BOOL					bNormalised)
{
	IMG_UINT32	uByteOffset;

	if (uBitOffset == USC_UNDEF)
	{
		psChannel->eFormat = USC_CHANNELFORM_ZERO;
		psChannel->uChanOffset = USC_UNDEF;
		psChannel->uChunkIdx = USC_UNDEF;
		psChannel->uRegOffsetInChunk = USC_UNDEF;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		psChannel->bVector = IMG_FALSE;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		return;
	}

	ASSERT((uBitOffset % BITS_PER_BYTE) == 0);
	uByteOffset = uBitOffset / BITS_PER_BYTE;

	psChannel->eFormat = bNormalised ? USC_CHANNELFORM_U8 : USC_CHANNELFORM_U8_UN;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	psChannel->bVector = IMG_FALSE;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	psChannel->uChunkIdx = 0;
	psChannel->uRegOffsetInChunk = uByteOffset / LONG_SIZE;
	psChannel->uChanOffset = uByteOffset % LONG_SIZE;
}

static
IMG_VOID CreateSINFTextureResultLayout(PINTERMEDIATE_STATE		psState,
									   PSAMPLE_RESULT_LAYOUT	psLayout,
									   IMG_UINT32				uSamplerSwizzle)
/*****************************************************************************
 FUNCTION	: CreateSINFTextureResultLayout

 PURPOSE	: Update the details of how to unpack the result of a texture
			  result for the special data return mode used for LDSINF.

 PARAMETERS	: psState			- Compiler state.
			  psLayout			- Default layout.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uResultSize;
	IMG_UINT32	uMipMapLevelBitOffset;
	IMG_UINT32	uTrilinearFractionBitOffset;
	IMG_UINT32	uUFractionBitOffset;
	IMG_UINT32	uVFractionBitOffset;
	IMG_UINT32	uSFractionBitOffset;

	/*
		Sampler swizzles aren't supported on a texture sample returning filtering coefficents.
	*/
	ASSERT(uSamplerSwizzle == UFREG_SWIZ_NONE);

	#if defined(SUPPORT_SGX545)
	if ((psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_TAG_VOLUME_TEXTURES) != 0)
	{
		uResultSize = SGX545_USE_SMP_SRD_COEFFS_RESULT_LENGTH;
		uMipMapLevelBitOffset = BITS_PER_UINT + SGX545_USE_SMP_SRD_COEFFS1_LOD_SHIFT;
		uTrilinearFractionBitOffset = SGX545_USE_SMP_SRD_COEFFS0_TRILINEARFRACTION_SHIFT;
		uUFractionBitOffset = SGX545_USE_SMP_SRD_COEFFS0_UFRACTION_SHIFT;
		uVFractionBitOffset = SGX545_USE_SMP_SRD_COEFFS0_VFRACTION_SHIFT;
		uSFractionBitOffset = SGX545_USE_SMP_SRD_COEFFS0_SFRACTION_SHIFT;
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	{
		uResultSize = SGXVEC_USE_SMP_SRD_COEFFS_RESULT_LENGTH;
		uMipMapLevelBitOffset = SGXVEC_USE_SMP_SRD_COEFFS0_LOD_SHIFT;
		uTrilinearFractionBitOffset = SGXVEC_USE_SMP_SRD_COEFFS0_TRILINEARFRACTION_SHIFT;
		uUFractionBitOffset = SGXVEC_USE_SMP_SRD_COEFFS0_UFRACTION_SHIFT;
		uVFractionBitOffset = SGXVEC_USE_SMP_SRD_COEFFS0_VFRACTION_SHIFT;
		uSFractionBitOffset = USC_UNDEF;
	}
	#else /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		imgabort();
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	
	/*
		The LOD is same for every plane so sample the first one only.
	*/
	psLayout->uChunkCount = 1;

	/*
		Create a chunk layout for the filtering parameters return data.
	*/
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	psLayout->asChunk[0].bVector = IMG_FALSE;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	psLayout->asChunk[0].eFormat = USC_CHANNELFORM_INVALID;
	psLayout->asChunk[0].uSizeInDwords = uResultSize;
	psLayout->asChunk[0].uSizeInRegs = psLayout->asChunk[0].uSizeInDwords;

	psLayout->uChanCount = 5;
	UscFree(psState, psLayout->asChannelLocation);
	psLayout->asChannelLocation = UscAlloc(psState, sizeof(psLayout->asChannelLocation[0]) * psLayout->uChanCount);

	/*
		Create a channel result for the mipmap level.
	*/
	SetupSINFChannelLocation(psState, &psLayout->asChannelLocation[0], uMipMapLevelBitOffset, IMG_FALSE /* bNormalised */);

	/*
		Create a channel result for the trilinear fraction.
	*/
	SetupSINFChannelLocation(psState, &psLayout->asChannelLocation[1], uTrilinearFractionBitOffset, IMG_TRUE /* bNormalised */);

	/*
		Create channel results for the bilinear fractions.
	*/
	SetupSINFChannelLocation(psState, &psLayout->asChannelLocation[2], uUFractionBitOffset, IMG_TRUE /* bNormalised */);
	SetupSINFChannelLocation(psState, &psLayout->asChannelLocation[3], uVFractionBitOffset, IMG_TRUE /* bNormalised */);
	SetupSINFChannelLocation(psState, &psLayout->asChannelLocation[4], uSFractionBitOffset, IMG_TRUE /* bNormalised */);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545) */

static
IMG_VOID CreateTextureResultChannelLocations(PINTERMEDIATE_STATE	psState,
											 IMG_UINT32				uSwizzle,
											 SAMPLE_RESULT_LOCATION	asChannelLocation[],
											 PSAMPLE_RESULT_LAYOUT	psLayout)
/*****************************************************************************
 FUNCTION	: CreateTextureResultChannelLocations

 PURPOSE	: 

 PARAMETERS	: psState		- Compiler state.
			  uSwizzle		- Swizzle on the texture sample results.
			  asChannelLocation
							- Layout of the result of the texture sample before
							the swizzle is applied.
			  psLayout		- Returns the format of the result of a texture
							sample.

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	psLayout->uChanCount = CHANNELS_PER_INPUT_REGISTER;
	psLayout->asChannelLocation = UscAlloc(psState, sizeof(psLayout->asChannelLocation[0]) * psLayout->uChanCount);
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		IMG_UINT32	uSrcSel = EXTRACT_CHAN(uSwizzle, uChan);

		if (uSrcSel <= UFREG_SWIZ_A)
		{
			psLayout->asChannelLocation[uChan] = asChannelLocation[uSrcSel];
		}
		else
		{
			ConvertSwizzleSelToChannelLocation(psState, &psLayout->asChannelLocation[uChan], uSrcSel);
		}
	}
}

/*****************************************************************************
 FUNCTION	: GetTextureResultCount

 PURPOSE	: Get the details of how the channels in a texture are packed into
			  the destination registers of a texture sample by the hardware.

 PARAMETERS	: psState		- Compiler state.
			  uSamplerIdx	- Index of the sampler to get the information for.
			  uSamplerSwizzle	
							- Swizzle on the sampler parameter to the texture
							sample instruction.
			  psLayout		- Returns the format of the result of a texture
							sample.

 RETURNS	: None.
*****************************************************************************/
static
IMG_VOID GetTextureResultCount(PINTERMEDIATE_STATE		psState,
							   IMG_UINT32				uSamplerIdx,
							   IMG_UINT32				uSamplerSwizzle,
							   PSAMPLE_RESULT_LAYOUT	psLayout)
{
	IMG_UINT32					uTotalBitCountBeforeExpansion;
	IMG_UINT32					uChunkBitCountBeforeExpansion, uChunkBitCountAfterExpansion;
	PUNIFLEX_TEXTURE_PARAMETERS	psTexParams = &psState->psSAOffsets->asTextureParameters[uSamplerIdx];
	PUNIFLEX_TEXFORM			psTexForm = &psTexParams->sFormat;
	IMG_UINT32					uBitsPerChunk;
	IMG_UINT32					uChan;
	IMG_UINT32					uMinimumChunkBitCount;
	IMG_UINT32					uBitsPerChan;
	IMG_UINT32					uTexformSwizzle;
	IMG_UINT32					uSwizzle;
	SAMPLE_RESULT_LOCATION		asChannelLocation[CHANNELS_PER_INPUT_REGISTER];
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	IMG_UINT32					uChunk;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	uBitsPerChunk = psTexForm->uChunkSize * 8;

	uMinimumChunkBitCount = 32;

	if (psState->asTextureUnpackFormat[uSamplerIdx].eFormat == UNIFLEX_TEXTURE_UNPACK_FORMAT_C10)
	{
		uBitsPerChan = 10;
	}
	else
	{
		uBitsPerChan = 8;
	}

	/*
		If gamma correction is enabled on a texture sample then the TAG/TF expands each channel
		to U16 format. In addition a texture sample always writes two 32-bit registers even for
		a texture format with only 1 or 2 channels of data.
	*/
	if (	
			!(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT) &&
			psTexParams->bGamma
		)
	{
		uMinimumChunkBitCount = 64;
	}

	/*
		If the TWORESULT flag is set then a texture sample on this formats always writes two
		32-bit registers regardless of the format of the channels. 
	*/
	if (psTexForm->bTwoResult)
	{
		uBitsPerChunk = 64;
		uMinimumChunkBitCount = 64;
	}

	uTotalBitCountBeforeExpansion = 0;
	uChunkBitCountBeforeExpansion = uChunkBitCountAfterExpansion = 0;
	psLayout->uChunkCount = 0;
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		USC_CHANNELFORM			eFormat;
		USC_CHANNELFORM			eUnpackedFormat;
		IMG_UINT32				uChanBitCountBeforeExpansion, uChanBitCountAfterExpansion;
		PSAMPLE_RESULT_LOCATION	psChannelLocation = &asChannelLocation[uChan];
		IMG_UINT32				uChanOffsetInChunk;
		PSAMPLE_RESULT_CHUNK	psChunk = &psLayout->asChunk[psLayout->uChunkCount];

		/*
			Get the raw format of this channel - before any conversion applied by the
			TAG/TF.
		*/
		eFormat = psTexForm->peChannelForm[uChan];
		uChanBitCountBeforeExpansion = g_puChannelFormBitCount[eFormat];

		/*
			Get the format after any conversion.
		*/
		eUnpackedFormat = GetUnpackedChannelFormat(psState, uSamplerIdx, eFormat);
		uChanBitCountAfterExpansion = g_puChannelFormBitCount[eUnpackedFormat];

		/*
			Store the format of this channel.
		*/
		psChannelLocation->eFormat = eUnpackedFormat;

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		/*
			By default the channel is not part of a vector register.
		*/
		psChannelLocation->bVector = IMG_FALSE;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		/*
			If every (non-constant) channel in this chunk has the same format then record
			the format.
		*/
		if (uChanBitCountAfterExpansion > 0)
		{
			if (uChunkBitCountAfterExpansion == 0)
			{
				psChunk->eFormat = eUnpackedFormat;
			}
			else
			{
				if (psChunk->eFormat != eUnpackedFormat)
				{
					psChunk->eFormat = USC_CHANNELFORM_INVALID;
				}
			}
		}

		/*
			Store the index of the chunk which contains the data for this channel.
		*/
		psChannelLocation->uChunkIdx = psLayout->uChunkCount;

		/*
			The hardware won't split a channel across two dword registers so check for this
			and round up the starting bit offset to the next dword.
		*/
		if (
				psState->asTextureUnpackFormat[uSamplerIdx].eFormat == UNIFLEX_TEXTURE_UNPACK_FORMAT_C10 && 
				uChunkBitCountAfterExpansion == 30
		   )
		{
			uChunkBitCountAfterExpansion = 40;
		}

		/*
			Store the offset of the data for this channel within the result of
			sampling the chunk.
		*/
		ASSERT((uChunkBitCountAfterExpansion % uBitsPerChan) == 0);
		uChanOffsetInChunk = uChunkBitCountAfterExpansion / uBitsPerChan;
		psChannelLocation->uRegOffsetInChunk = uChanOffsetInChunk >> 2;
		psChannelLocation->uChanOffset = uChanOffsetInChunk % 4;

		uTotalBitCountBeforeExpansion += uChanBitCountBeforeExpansion;

		uChunkBitCountBeforeExpansion += uChanBitCountBeforeExpansion;
		uChunkBitCountAfterExpansion += uChanBitCountAfterExpansion;

		ASSERT(uChunkBitCountBeforeExpansion <= uBitsPerChunk);
		if (
				uChunkBitCountBeforeExpansion == uBitsPerChunk || 
				(
					uChan == (CHANNELS_PER_INPUT_REGISTER - 1) &&
					uChunkBitCountBeforeExpansion > 0
				)
		   )

		{
			IMG_UINT32	uChunkResultSize;

			/*
				Store the number of 32-bit registers needed to hold the result of sampling
				this chunk.
			*/
			if (uChunkBitCountAfterExpansion < uMinimumChunkBitCount)
			{
				uChunkBitCountAfterExpansion = uMinimumChunkBitCount;
			}
			uChunkResultSize = (uChunkBitCountAfterExpansion + 31) / 32;
			psChunk->uSizeInDwords = psChunk->uSizeInRegs = uChunkResultSize;
			psLayout->uChunkCount++;

			uChunkBitCountBeforeExpansion = 0;
			uChunkBitCountAfterExpansion = 0;
		}	
	}

	/*
		Combine the swizzles from the instruction instruction sampler parameter and the swizzle
		implied by the texture format.
	*/
	uTexformSwizzle = GetTexformSwizzle(psState, uSamplerIdx);
	uSwizzle = CombineInputSwizzles(uTexformSwizzle, uSamplerSwizzle);

	CreateTextureResultChannelLocations(psState, uSwizzle, asChannelLocation, psLayout);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Check if we might want to treat the texture sample result as a vector register so it can
		be easily mapped onto an internal register.
	*/
	for (uChunk = 0; uChunk < psLayout->uChunkCount; uChunk++)
	{
		PSAMPLE_RESULT_CHUNK	psChunk = &psLayout->asChunk[uChunk];
		psChunk->bVector = IMG_FALSE;
	}
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		for (uChunk = 0; uChunk < psLayout->uChunkCount; uChunk++)
		{
			PSAMPLE_RESULT_CHUNK	psChunk = &psLayout->asChunk[uChunk];
			USC_CHANNELFORM			eFormat = psChunk->eFormat;

			/*
				Map sample results which are all F32 or all F16 to vector registers.
			*/
			if (eFormat == USC_CHANNELFORM_F32 || eFormat == USC_CHANNELFORM_F16)
			{ 
				IMG_UINT32	uBitsPerChan = g_puChannelFormBitCount[eFormat];
				IMG_UINT32	uBytesPerChan = uBitsPerChan / BITS_PER_BYTE;

				psChunk->bVector = IMG_TRUE;

				/*
					Modify all of the channels in the overall texture sample result which source
					data from this chunk result.
				*/
				for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
				{
					PSAMPLE_RESULT_LOCATION	psChanLocation = &psLayout->asChannelLocation[uChan];

					if (g_puChannelFormBitCount[psChanLocation->eFormat] > 0 && psChanLocation->uChunkIdx == uChunk)
					{
						IMG_UINT32	uByteOffset;
						IMG_UINT32	uBitOffset;

						ASSERT(psChanLocation->eFormat == eFormat);

						uByteOffset = psChanLocation->uRegOffsetInChunk * LONG_SIZE + psChanLocation->uChanOffset;
						uBitOffset = uByteOffset * BITS_PER_BYTE;

						ASSERT((uBitOffset % uBitsPerChan) == 0);
					
						psChanLocation->uChanOffset = uBitOffset / uBitsPerChan;
						ASSERT(psChanLocation->uChanOffset < VECTOR_LENGTH);
						psChanLocation->uRegOffsetInChunk = 0;

						psChanLocation->bVector = IMG_TRUE;
					}
				}

				psChunk->uSizeInRegs = (psChunk->uSizeInRegs + uBytesPerChan - 1) / uBytesPerChan;
				ASSERT(psChunk->uSizeInRegs == 1);
			}
		}
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	if (psTexForm->bTwoResult)
	{
		psLayout->uTexelSizeInBytes = 4;
	}
	else
	{
		psLayout->uTexelSizeInBytes = (uTotalBitCountBeforeExpansion + 7) / 8;
	}
}


static
IMG_VOID GetSampleInputParameters(PINTERMEDIATE_STATE	psState,
								  PUNIFLEX_INST			psInputInst,
								  UF_OPCODE				eOpCode, 									
								  IMG_UINT32			uTextureDimensionality, 
								  PSAMPLE_INPUT_PARAMS	psParams)
/*****************************************************************************
 FUNCTION	: GetSampleInputParameters
    
 PURPOSE	: Unpack instructions parameters from an input texture sample
			  argument.

 PARAMETERS	: psState 		- Compiler state.
			  psInputInst	- Uniflex input instruction.
			  eOpCode		- Currently selected opcode for input instruction. 			  
			  uTextureDimensionality
							- Dimensions of texture.
			  psParams		- Returns the input texture sample parameters

 RETURNS	: Nothing.
*****************************************************************************/
{	
	IMG_UINT32		uOffsetsArg;
	PUF_REGISTER	psOffsetsArg;
	IMG_UINT32		uDimIdx;
	IMG_UINT32		uOffsets;

	/*
		Set default return values.
	*/
	psParams->sOffsets.bPresent = IMG_FALSE;
	psParams->eSpecialReturnMode = UF_LDPARAM_SPECIALRETURN_NONE;

	/*	
		Get the source argument to the input instruction which contains the encoded offsets.
	*/
	uOffsetsArg = GetSampleParametersSource(eOpCode);
	psOffsetsArg = &psInputInst->asSrc[uOffsetsArg];	
	ASSERT((GetRegisterFormat(psState, psOffsetsArg) == UF_REGFORMAT_U32) && (psOffsetsArg->eType == UFREG_TYPE_IMMEDIATE));	

	/*
		Unpack the offsets for each dimension from the source argument.
	*/
	uOffsets = (psOffsetsArg->uNum & ~UF_LDPARAM_OFFSETS_CLRMSK) >> UF_LDPARAM_OFFSETS_SHIFT;
	for (uDimIdx = 0; uDimIdx < uTextureDimensionality; uDimIdx++)
	{
		/* Convert from 4-bit twos complement to 32-bit. */
		psParams->sOffsets.aiOffsets[uDimIdx] = (IMG_INT32)UF_LDPARAM_OFFSETS_GETOFF(uOffsets, uDimIdx);
		if (psParams->sOffsets.aiOffsets[uDimIdx] != 0)
		{
			psParams->sOffsets.bPresent = IMG_TRUE;
		}
	}

	/*
		Unpack the special return mode.
	*/
	psParams->eSpecialReturnMode = (UF_LDPARAM_SPECIALRETURN)((psOffsetsArg->uNum & ~UF_LDPARAM_SPECIALRETURN_CLRMSK) >> UF_LDPARAM_SPECIALRETURN_SHIFT);
}

static
IMG_VOID EmitSampleDataInstruction(PINTERMEDIATE_STATE		psState,
								   PCODEBLOCK				psCodeBlock,
								   LOD_MODE					eLODMode,
								   IMG_BOOL					bInsideDynamicFlowControl,
								   UF_REGFORMAT				eDestFormat,
								   PSAMPLE_RESULT_LAYOUT	psLayout,
								   PSAMPLE_RESULT			psResult,
								   IMG_UINT32				uSamplerIdx,
								   IMG_UINT32				uTextureDimensionality,
								   IMG_BOOL					bUsePCF,
								   PSAMPLE_COORDINATES		psCoordinates,
								   PSAMPLE_LOD_ADJUST		psLODAdjust,
								   PSAMPLE_GRADIENTS		psGradients,
								   PSAMPLE_PROJECTION		psProjection,
								   PSMP_IMM_OFFSETS			psImmOffsets,
								   IMG_BOOL					bSampleIdxPresent,
								   PARG						psSampleIdx,
								   SMP_RETURNDATA			eSpecialReturnData)
/*****************************************************************************
 FUNCTION	: EmitSampleDataInstruction
    
 PURPOSE	: Emit an intermediate texture sample instruction for each chunk
			  in a texture.

 PARAMETERS	: psState 		- Compiler state.
			  psCodeBlock	- Block to insert the intermediate instruction
							into.
			  eLODMode		- LOD calculation mode to use for the texture sample.
			  bInsideDynamicFlowControl
							- Is this texture sample inside dynamic flow control?
			  eDestFormat	- Format of the destination registers for the sample.
			  psLayout		- Format/packing of the results of the texture sample.
			  psResult		- OUTPUT: The temporary registers containing the
							results of the texture sample.
			  uSamplerIdx	- Index of the texture to sample.
			  uTextureDimensionality
							- Number of dimensions for the texture to sample.
			  bUsePCF		- If TRUE enable PCF for the texture sample.
			  uChunk		- Chunk within the texture to sample.
			  psCoordinates	- The registers containing the texture coordinates
							for the sample.
			  psLODAdjust	- The registers containing the LOD bias/replace for
							the sample.
			  psGradients	- The registers containing the gradients for the
							sample.
			  psProjection	- The registers containing a value to project the
							coordinates by.
			  psImmOffsets	- If the hardware supports it then intermediate offsets
							to encode into the state.
			  bSampleIdxPresent
							- Sample Idx. update present or not.
			  psSampleIdx	- Sample Idx. update related info.
			  eSpecialReturnData
							- Any special data to return from the texture
							sample (unfiltered samples, sampling parameters).

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uOverallResultCount;
	IMG_UINT32	uChunk;
	IMG_UINT32	uRawResultTempStart;
	ARG			asDest[USC_MAX_NONCALL_DEST_COUNT];
	
	/*
		Count the size of the result for all planes.
	*/
	uOverallResultCount = 0;
	for (uChunk = 0; uChunk < psLayout->uChunkCount; uChunk++)
	{
		uOverallResultCount += psLayout->asChunk[uChunk].uSizeInRegs;
	}

	/*
		Allocate temporary registers to hold the result.
	*/
	uRawResultTempStart = GetNextRegisterCount(psState, uOverallResultCount);

	/*
		Create an array of intermediate registers for the sample results.
	*/
	ASSERT(uOverallResultCount <= USC_MAX_NONCALL_DEST_COUNT);
	MakeArgumentSet(asDest, uOverallResultCount, USEASM_REGTYPE_TEMP, uRawResultTempStart, eDestFormat);

	/*
		Emit a sample instruction.
	*/
	BaseEmitSampleInstructionWithState(psState,
									   psCodeBlock,
									   eLODMode,
									   bInsideDynamicFlowControl,
									   uOverallResultCount,
									   asDest,
									   NULL /* auDestMask */,
									   uSamplerIdx,
									   0 /* uFirstPlane */,
									   uTextureDimensionality,
									   bUsePCF,
									   psCoordinates,
									   NULL /* psTextureState */,
									   psLayout->uChunkCount,
									   psLayout->asChunk,
									   psImmOffsets,
									   psLODAdjust,
									   psGradients,
									   psProjection,
									   eSpecialReturnData,
									   IMG_FALSE /* bUSPSample */,
									   UF_REGFORMAT_INVALID /* eTexPrecision */,
									   USC_UNDEF /* uChanMask */,
									   USC_UNDEF /* uChanSwizzle */,
									   bSampleIdxPresent,
									   psSampleIdx);

	/*
		Set the location of the result data for each plane.
	*/
	for (uChunk = 0; uChunk < psLayout->uChunkCount; uChunk++)
	{
		psResult->auRawDataPerChunk[uChunk] = uRawResultTempStart;
		uRawResultTempStart += psLayout->asChunk[uChunk].uSizeInRegs;
	}
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID EmitSpecialReturnSample(PINTERMEDIATE_STATE	psState,
							     PCODEBLOCK				psBlock,
								 LOD_MODE				eLODMode,
								 IMG_BOOL				bInsideDynamicFlowControl,
								 IMG_UINT32				uDestCount,
								 IMG_UINT32				uDestStartReg,
								 UF_REGFORMAT			eDestFormat,
								 UF_REGFORMAT*			aeDestFormats,
								 IMG_UINT32				uTexture,
								 IMG_UINT32				uTextureDimensionality,
								 IMG_BOOL				bUsePCF,
								 IMG_UINT32				uPlaneIdx,
								 PSAMPLE_COORDINATES	psCoordinates,
								 PSAMPLE_LOD_ADJUST		psLODAdjust,
								 PSAMPLE_GRADIENTS		psGradients,
								 PSMP_IMM_OFFSETS		psImmOffsets,
								 IMG_BOOL				bSampleIdxPresent,
								 PARG					psSampleIdx,
								 SMP_RETURNDATA			eReturnData)
/*****************************************************************************
 FUNCTION	: EmitSpecialReturnSample
    
 PURPOSE	: Emit code for a sample instruction which returns either the prefiltered
			  sample data or the coefficents for the filtering.

 PARAMETERS	: psState					- Compiler state.
			  psCodeBlock				- Flow control block to add the intermediate
										instructions to.
			  eLODMode					- LOD mode to use for the texture sample.
			  bInsideDynamicFlowControl	
										- TRUE if the texture sample might not
										be executed by all instances.
			  uDestCount				- Count of registers written by the texture
										sample.
			  uDestStartReg				- First temporary register to write with the
										result of the texture sample.
			  eDestFormat				- Format of the temporary registers to write the
										result of the texture sample.
			  aeDestFormats				- If non-NULL individual formats for each result.
			  uTexture					- Index of the texture to sample.
			  uTextureDimensionality	- Dimensionality of the texture to sample.
			  bUsePCF					- TRUE if PCF is enabled for this texture sample.
			  uPlaneIdx					- Index of the plane to sample.
			  psCoordinates				- Source for the texture coordinates.
			  psLODAdjust				- Source for any LOD bias or replace.
			  psImmOffset				- Immediate offsets to use for the texture sample.
			  bSampleIdxPresent			- Sample Idx. update present or not.
			  psSampleIdx				- Sample Idx. update related info.
			  eReturnData				- Data of special data to return.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uDest;
	ARG			asDest[USC_MAX_NONCALL_DEST_COUNT];

	/*
		Create an array of intermediate registers for the sample results.
	*/
	ASSERT(uDestCount <= USC_MAX_NONCALL_DEST_COUNT);
	for (uDest = 0; uDest < uDestCount; uDest++)
	{
		PARG	psDest = &asDest[uDest];

		InitInstArg(psDest);
		psDest->uType = USEASM_REGTYPE_TEMP;
		psDest->uNumber = uDestStartReg + uDest;
		if (aeDestFormats != NULL)
		{
			psDest->eFmt = aeDestFormats[uDest];
		}
		else
		{
			psDest->eFmt = eDestFormat;
		}
	}

	BaseEmitSampleInstructionWithState(psState,
									   psBlock,
									   eLODMode,
									   bInsideDynamicFlowControl,
									   uDestCount,
									   asDest,
									   NULL /* auDestMask */,
									   uTexture,
									   uPlaneIdx,
									   uTextureDimensionality,
									   bUsePCF,
									   psCoordinates,
									   NULL /* psTextureState */,
									   1 /* uPlaneCount */,
									   NULL /* auResultCountPerPlane */,
									   psImmOffsets,
									   psLODAdjust,
									   psGradients,
									   NULL /* psProjection */,
									   eReturnData,
									   IMG_FALSE /* bUSPSample */,
									   UF_REGFORMAT_INVALID /* eTexPrecision */,
									   USC_UNDEF /* uChanMask */,
									   USC_UNDEF /* uChanSwizzle */,
									   bSampleIdxPresent,
									   psSampleIdx);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID EmulatePCF_Vec(PINTERMEDIATE_STATE		psState,
						PCODEBLOCK				psCodeBlock,
						PSAMPLE_RESULT_LAYOUT	psResultLayout,
						PSAMPLE_RESULT			psResult,
						LOD_MODE				eLODMode,
						IMG_BOOL				bInsideDynamicFlowControl,
						UF_REGFORMAT			eSampleResultFormat,
						IMG_UINT32				uSamplerIdx,
						IMG_UINT32				uTextureDimensionality,
						PSAMPLE_COORDINATES		psCoordinates,
						PSAMPLE_LOD_ADJUST		psLODAdjust,
						PSAMPLE_GRADIENTS		psGradients,
						PSAMPLE_PROJECTION		psProjection,
						PSMP_IMM_OFFSETS		psImmOffsets,
						IMG_BOOL				bSampleIdxPresent,
						PARG					psSampleIdx,
						IMG_UINT32				uSamplerSwizzle)
/*****************************************************************************
 FUNCTION	: EmulatePCF_Vec

 PURPOSE	: Emulate PCF (percentage closer filtering) on cores which support
			  the vector instruction set.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psResultLayout	- Packing/formats of the data returned when
								sampling the texture.
			  psResult			- OUTPUT: The temporary registers containing the
								results of the sample (including PCF).
			  eLODMode			- Type of texture sample to perform.
			  bInsideDynamicFlowControl
								- Is this texture sample inside dynamic flow
								control.
			  eSampleResultFormat
								- Format of the data returned when sampling the
								the texture.
			  uSamplerIdx		- Index of the texture to sample.
			  uTextureDimensionality	
								- Number of dimensions of the texture to sample.
			  psCoordinates		- Texture coordinates to use when sampling.
			  psLODAdjust		- LOD bias/replace to use when sampling.
			  psGradients		- Gradients to use when sampling.
			  psProjection		- Value to project the texture coordinates by.
			  psImmOffsets		- Immediate offsets to use when sampling.
			  bSampleIdxPresent	- Sample Idx. update present or not.
			  psSampleIdx		- Sample Idx. update related info.
			  uSamplerSwizzle	- Swizzle on the sampler parameter to the 
								input texture instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32					uMipMapLevel;
	IMG_UINT32					uMipMapLevelCount;
	PUNIFLEX_TEXTURE_PARAMETERS	psTextureParams = &psState->psSAOffsets->asTextureParameters[uSamplerIdx];
	IMG_UINT32					auLevelResult[2];
	IMG_BOOL					bMultipleSamples;
	IMG_UINT32					uSampleResultSizeForCompChan;
	IMG_UINT32					uSmpResultSizePerLevel;
	IMG_UINT32					uSmpResultSize;
	SMP_RETURNDATA				eReturnData;
	IMG_UINT32					uSmpResult;
	PSAMPLE_RESULT_LOCATION		psCompChanLocation;
	IMG_UINT32					uFinalResult;
	HARDWARE_FILTER_MODE		eFilterMode;
	UFREG_COMPOP				ePCFComparison = psTextureParams->ePCFComparisonMode;
	UF_REGFORMAT				aeSmpDestFormats[SMP_MAX_SAMPLE_RESULT_DWORDS * SGXVEC_USE_SMP_IRSD_BILINEAR_SAMPLE_COUNT + SGXVEC_USE_SMP_IRSD_BILINEAR_FRACTIONS_SIZE];
	IMG_UINT32					uRawSampleSizePerLevel;

	/*
		For these trival comparison modes return a fixed result.
	*/
	if (ePCFComparison == UFREG_COMPOP_NEVER || ePCFComparison == UFREG_COMPOP_ALWAYS)
	{
		PINST		psMOVInst;
		IMG_FLOAT	fFixedResult;

		/*
			Get the fixed result of the comparisons.
		*/
		if (ePCFComparison == UFREG_COMPOP_ALWAYS)
		{
			fFixedResult = 1.0f;
		}
		else
		{
			ASSERT(ePCFComparison == UFREG_COMPOP_NEVER);
			fFixedResult = 0.0f;
		}

		/*
			Free details of the result of the original texture sample.
		*/
		FreeSampleResultLayout(psState, psResultLayout);

		/*
			Set up information about the result of a texture sample matching the
			result of the PCF filtering (a single channel in F32 format).
		*/
		CreateDummyResultLayout(psState, 
								psResultLayout, 
								psResult, 
								GetNextRegister(psState),
								uSamplerSwizzle,
								IMG_TRUE /* bVector */, 
								1 /* uRegisterCount */);

		/*
			Move the fixed result of the texture sample into the result register.
		*/
		psMOVInst = AllocateInst(psState, NULL);
		SetOpcode(psState, psMOVInst, IVMOV);
		psMOVInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psMOVInst->asDest[0].uNumber = psResult->auRawDataPerChunk[0];
		SetupImmediateSource(psMOVInst, 0 /* uArgIdx */, fFixedResult, UF_REGFORMAT_F32);
		psMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		AppendInst(psState, psCodeBlock, psMOVInst);

		return;
	}

	/*
		Get the location and format in the texture data of the channel to compare
		to the depth value.
	*/
	ASSERT(USC_X_CHAN < psResultLayout->uChanCount);
	psCompChanLocation = &psResultLayout->asChannelLocation[USC_X_CHAN];
	uSampleResultSizeForCompChan = psResultLayout->asChunk[psCompChanLocation->uChunkIdx].uSizeInDwords;

	eFilterMode = GetFilterMode(psState, psTextureParams);
	switch (eFilterMode)
	{
		case HARDWARE_FILTER_MODE_POINT:
		{
			/* One sample from one mipmap level. */
			bMultipleSamples = IMG_FALSE;
			uMipMapLevelCount = 1;
			break;
		}
		case HARDWARE_FILTER_MODE_BILINEAR: 
		{
			/* Four samples from one mipmap level. */
			bMultipleSamples = IMG_TRUE;
			uMipMapLevelCount = 1; 
			break;
		}
		case HARDWARE_FILTER_MODE_TRILINEAR:
		{
			/* Four samples from each of two mipmap levels. */
			bMultipleSamples = IMG_TRUE;
			uMipMapLevelCount = 2; 
			break;
		}
		default: imgabort();
	}

	if (bMultipleSamples)
	{
		/*
			Generate a special sample instruction returning both the raw samples and the
			bilinear/trilinear fractions.
		*/
		eReturnData = SMP_RETURNDATA_BOTH;
		uRawSampleSizePerLevel = uSampleResultSizeForCompChan * SGXVEC_USE_SMP_IRSD_BILINEAR_SAMPLE_COUNT;
		uSmpResultSizePerLevel = SGXVEC_USE_SMP_IRSD_BILINEAR_FRACTIONS_SIZE;
		uSmpResultSizePerLevel += uRawSampleSizePerLevel;
	}
	else
	{
		eReturnData = SMP_RETURNDATA_NORMAL;
		uRawSampleSizePerLevel = uSampleResultSizeForCompChan;
		uSmpResultSizePerLevel = uRawSampleSizePerLevel;
	}
	uSmpResultSize = uSmpResultSizePerLevel * uMipMapLevelCount;

	/*
		Project the texture coordinates (including the PCF comparison value). 
	*/
	if (psProjection->bProjected)
	{
		psCoordinates->uCoordStart =
			ProjectCoordinates(psState,
							   psCodeBlock,
							   NULL /* psInsertBeforeInst */,
							   &psProjection->sProj,
							   NULL /* psProjSrcInst */,
							   USC_UNDEF /* uProjSrcIdx */,
							   psProjection->uProjChannel,
							   psCoordinates->uCoordSize,
							   psCoordinates->uUsedCoordChanMask,
							   psCoordinates->eCoordFormat,
							   NULL /* psCoordSrcInst */,
							   USC_UNDEF /* uCoordSrcIdx */,
							   psCoordinates->uCoordStart);
	}

	/*
		Allocate registers for the result of the texture sample.
	*/
	uSmpResult = GetNextRegisterCount(psState, uSmpResultSize);

	/*
		Set the formats of the returned data.
	*/
	for (uMipMapLevel = 0; uMipMapLevel < uMipMapLevelCount; uMipMapLevel++)
	{
		IMG_UINT32	uDestIdx;

		for (uDestIdx = 0; uDestIdx < uRawSampleSizePerLevel; uDestIdx++)
		{
			aeSmpDestFormats[uMipMapLevel * uSmpResultSizePerLevel + uDestIdx] = eSampleResultFormat;
		}
		if (bMultipleSamples)
		{
			for (uDestIdx = 0; uDestIdx < SGXVEC_USE_SMP_IRSD_BILINEAR_FRACTIONS_SIZE; uDestIdx++)
			{
				aeSmpDestFormats[uMipMapLevel * uSmpResultSizePerLevel + uRawSampleSizePerLevel + uDestIdx] = UF_REGFORMAT_F16;
			}
		}
	}

	/*
		Emit a sample instruction to get both the bilinear/trilinear
		coefficents and the raw (prefiltered) sample data.
	*/
	EmitSpecialReturnSample(psState,
							psCodeBlock,
							eLODMode,
							bInsideDynamicFlowControl,
							uSmpResultSize,
							uSmpResult,
							UF_REGFORMAT_INVALID /* uDestFormat */,
							aeSmpDestFormats,
							uSamplerIdx,
							uTextureDimensionality,
							IMG_FALSE /* bUsePCF */,
							psCompChanLocation->uChunkIdx,
							psCoordinates,
							psLODAdjust,
							psGradients,
							psImmOffsets,
							bSampleIdxPresent,
							psSampleIdx,
							eReturnData);

	for (uMipMapLevel = 0; uMipMapLevel < uMipMapLevelCount; uMipMapLevel++)
	{
		PINST		psVTestMask;
		PINST		psVDP;
		IMG_UINT32	uCompResult;
		ARG			sLevelData;
		IMG_UINT32	uLevelDataNonVec;
		IMG_UINT32	uSampleIdx;
		IMG_UINT32	uSampleCount;

		/*
			Number of samples loaded from this mipmap level.
		*/
		if (bMultipleSamples)
		{
			uSampleCount = SGXVEC_USE_SMP_IRSD_BILINEAR_SAMPLE_COUNT;
		}
		else
		{
			uSampleCount = 1;
		}

		/*
			Get the start of the registers holding the unfiltered texture samples
			for this mipmap level.
		*/
		uLevelDataNonVec = uSmpResult + uSmpResultSizePerLevel * uMipMapLevel;

		/*
			Unpack to F32 the channel in the texture sample result which is to be compared.
		*/
		InitInstArg(&sLevelData);
		sLevelData.uType = USEASM_REGTYPE_TEMP;
		sLevelData.uNumber = GetNextRegister(psState);
		for (uSampleIdx = 0; uSampleIdx < uSampleCount; uSampleIdx++)
		{
			IMG_UINT32	uSampleDataNonVec;

			uSampleDataNonVec = uLevelDataNonVec + uSampleIdx * uSampleResultSizeForCompChan;
			uSampleDataNonVec += psCompChanLocation->uRegOffsetInChunk;

			UnpackTextureChanFloatVec(psState, 
									  psCodeBlock, 
									  &sLevelData, 
									  uSampleIdx /* uDestChan */, 
									  psCompChanLocation->eFormat /* eSrcFormat */,
									  uSampleDataNonVec /* uSrcRegNum */, 
									  psCompChanLocation->uChanOffset /* uSrcByteOffset */);
		}
		
		/*
			Allocate a temporary register for the result of the comparison
			with the depth value.
		*/
		uCompResult = GetNextRegister(psState);

		/*
			Compare the raw samples to the depth value.

				COMPRESULT.CHAN = SAMPLE.CHAN compop REFERENCE ? 1 : 0
		*/
		psVTestMask = AllocateInst(psState, NULL);
		SetOpcode(psState, psVTestMask, IVTESTMASK);
		psVTestMask->u.psVec->sTest.eTestOpcode = IVADD;
		psVTestMask->u.psVec->sTest.eType = ConvertCompOpToIntermediate(psState, ePCFComparison);
		psVTestMask->u.psVec->sTest.eMaskType = USEASM_TEST_MASK_NUM;
		psVTestMask->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psVTestMask->asDest[0].uNumber = uCompResult;
		if (bMultipleSamples)
		{
			psVTestMask->auDestMask[0] = USC_XYZW_CHAN_MASK;
		}
		else
		{
			psVTestMask->auDestMask[0] = USC_X_CHAN_MASK;
		}
		/* Argument containing the sample data. */
		psVTestMask->asArg[0] = sLevelData;
		psVTestMask->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psVTestMask->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F32;
		/* Argument containing the comparison value. */
		SetupTempVecSource(psState, psVTestMask, 1 /* uSrcIdx */, psCoordinates->uCoordStart, UF_REGFORMAT_F32);
		psVTestMask->u.psVec->auSwizzle[1] = g_auReplicateSwizzles[uTextureDimensionality];	
		psVTestMask->u.psVec->asSrcMod[1].bNegate = IMG_TRUE;
		AppendInst(psState, psCodeBlock, psVTestMask);

		if (bMultipleSamples)
		{
			IMG_UINT32	uFractionsNonVec;
			IMG_UINT32	uFractionsF16Vec;
			PINST		psMkvecInst;
			IMG_UINT32	uChan;

			/*
				Allocate a temporary register for the result of multiplying the comparison
				results by the filtering coefficents.
			*/
			auLevelResult[uMipMapLevel] = GetNextRegister(psState);

			/*
				Get the start of the registers holding the bilinear fractions
				for this mipmap level.
			*/
			uFractionsNonVec = uLevelDataNonVec + uSampleResultSizeForCompChan * SGXVEC_USE_SMP_IRSD_BILINEAR_SAMPLE_COUNT;

			/*
				Allocate a register for the bilinear fraction represented as a vector.
			*/
			uFractionsF16Vec = GetNextRegister(psState);

			/*
				Make a vector from the bilinear fractions.
			*/
			psMkvecInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psMkvecInst, IVMOV);
			psMkvecInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psMkvecInst->asDest[0].uNumber = uFractionsF16Vec;
			psMkvecInst->asDest[0].eFmt = UF_REGFORMAT_F16;
			psMkvecInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;
			for (uChan = 0; uChan < SGXVEC_USE_SMP_IRSD_BILINEAR_FRACTIONS_SIZE; uChan++)
			{
				psMkvecInst->asArg[1 + uChan].uType = USEASM_REGTYPE_TEMP;
				psMkvecInst->asArg[1 + uChan].uNumber = uFractionsNonVec + uChan;
				psMkvecInst->asArg[1 + uChan].eFmt = UF_REGFORMAT_F16;
			}
			for (; uChan < MAX_SCALAR_REGISTERS_PER_VECTOR; uChan++)
			{
				psMkvecInst->asArg[1 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
			}
			psMkvecInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_F16;
			psMkvecInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			AppendInst(psState, psCodeBlock, psMkvecInst);

			/*
				Multiply each comparison result by the corresponding bilinear/trilinear
				fraction and sum together.
			*/
			psVDP = AllocateInst(psState, NULL);
			SetOpcode(psState, psVDP, IVDP);
			psVDP->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psVDP->asDest[0].uNumber = auLevelResult[uMipMapLevel];
			psVDP->auDestMask[0] = USC_X_CHAN_MASK;
			/* Comparison result argument. */
			SetupTempVecSource(psState, psVDP, 0 /* uSrcIdx */, uCompResult, UF_REGFORMAT_F32);
			psVDP->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);	
			/* Bilinear/trilinear fractions argument. */
			SetupTempVecSource(psState, psVDP, 1 /* uSrcIdx */, uFractionsF16Vec, UF_REGFORMAT_F16);
			psVDP->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);	
			AppendInst(psState, psCodeBlock, psVDP);
		}
		else
		{
			auLevelResult[uMipMapLevel] = uCompResult;
		}
	}

	if (eFilterMode == HARDWARE_FILTER_MODE_TRILINEAR)
	{
		PINST		psVAdd;
		
		uFinalResult = GetNextRegister(psState);

		/*
			Sum the results for both mipmap levels.
		*/
		psVAdd = AllocateInst(psState, NULL);
		SetOpcode(psState, psVAdd, IVADD);
		psVAdd->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psVAdd->asDest[0].uNumber = uFinalResult;
		psVAdd->auDestMask[0] = USC_X_CHAN_MASK;
		/* Result for upper map. */
		SetupTempVecSource(psState, psVAdd, 0 /* uSrcIdx */, auLevelResult[0], UF_REGFORMAT_F32);
		psVAdd->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);	
		/* Result for lower map. */
		SetupTempVecSource(psState, psVAdd, 1 /* uSrcIdx */, auLevelResult[1], UF_REGFORMAT_F32);
		psVAdd->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);	
		AppendInst(psState, psCodeBlock, psVAdd);
	}
	else
	{
		uFinalResult = auLevelResult[0];
	}

	/*
		Free details of the result of the original texture sample.
	*/
	FreeSampleResultLayout(psState, psResultLayout);

	/*
		Set up information about the result of a texture sample matching the
		result of the PCF filtering (a single channel in F32 format).
	*/
	CreateDummyResultLayout(psState, 
							psResultLayout, 
							psResult, 
							uFinalResult, 
							uSamplerSwizzle,
							IMG_TRUE /* bVector */,
							1 /* uRegisterCount */);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

IMG_INTERNAL
IMG_VOID FreeSampleResultLayout(PINTERMEDIATE_STATE psState, PSAMPLE_RESULT_LAYOUT psSampleResultLayout)
/*****************************************************************************
 FUNCTION	: FreeSampleResultLayout

 PURPOSE	: Free memory allocated by ConvertSamplerInstructionCore.

 PARAMETERS	: psState				- Compiler state.
			  psSampleResultLayout	- Structure to free.	  

 RETURNS	: Nothing.
*****************************************************************************/
{
	UscFree(psState, psSampleResultLayout->asChannelLocation);
	psSampleResultLayout->asChannelLocation = NULL;
}

IMG_INTERNAL 
PCODEBLOCK ConvertSamplerInstructionCore(PINTERMEDIATE_STATE	psState,
										 PCODEBLOCK				psCodeBlock,
										 PUNIFLEX_INST			psSrc,
										 PSAMPLE_RESULT_LAYOUT	psLayout,
										 PSAMPLE_RESULT			psResult,
										 IMG_BOOL				bConditional)
/*****************************************************************************
 FUNCTION	: ConvertSamplerInstructionCore

 PURPOSE	: Convert an input texture sample instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  psResult			- The location of the raw data sampled from the
								texture by the generated instruction.
			  bConditional		- Whether the instruction appears in a conditional.			  

 RETURNS	: PCODEBLOCK - block to use for instructions after sampling 
			  instruction.
*****************************************************************************/
{
	IMG_BOOL					bProjected; 
	UF_OPCODE					eOpCode = psSrc->eOpCode;
	IMG_UINT32					uTexture = psSrc->asSrc[1].uNum;
	PUNIFLEX_TEXTURE_PARAMETERS	psTexParams = &psState->psSAOffsets->asTextureParameters[uTexture];
	PUNIFLEX_TEXFORM			psTexForm = &psTexParams->sFormat;
	IMG_UINT32					uTextureDimensionality;
	IMG_BOOL					bPCFTextureSample;
	IMG_BOOL					bUseHWPCF;
	IMG_BOOL					bEmulatePCF;
	UF_REGFORMAT				eDestRegisterFormat;
	SAMPLE_COORDINATES			sCoordinates;
	SAMPLE_PROJECTION			sProj;
	LOD_MODE					eLODMode;
	SAMPLE_LOD_ADJUST			sLODAdjust;
	SAMPLE_GRADIENTS			sGradients;
	IMG_BOOL					bSampleIdxPresent;
	ARG							sSampleIdx;
	SMP_RETURNDATA				eSpecialReturnData;
	IMG_UINT32					uSamplerSwizzle = psSrc->asSrc[1].u.uSwiz;
	SAMPLE_INPUT_PARAMS			sInputParams;

	ASSERT(uTexture < psState->psSAOffsets->uTextureCount);

	/*
	  Get the dimensionality of the texture we are looking up in.
	*/
	uTextureDimensionality = GetTextureCoordinateUsedChanCount(psState, uTexture);

	/*
	  Convert LDPIFTC to either LD or LDP depending on the texture coordinate.
	*/
	if (eOpCode == UFOP_LDPIFTC)
	{
		if (psSrc->asSrc[0].eType == UFREG_TYPE_TEXCOORD &&
			GetBit(psState->psSAOffsets->auProjectedCoordinateMask, psSrc->asSrc[0].uNum))
		{
			eOpCode = UFOP_LDP;
		}
		else
		{
			eOpCode = UFOP_LD;
		}
	}

	/*
		Do we want to use PCF (percentage closer filtering) for this texture sample?
	*/
	bPCFTextureSample = psTexForm->bUsePCF;
	if (eOpCode == UFOP_LDC || eOpCode == UFOP_LDCLZ)
	{
		bPCFTextureSample = IMG_TRUE;
	}

	bUseHWPCF = IMG_FALSE;
	bEmulatePCF = IMG_FALSE;
	if (bPCFTextureSample)
	{
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_PCF)
		{
			/*
				This core supports PCF directly.
			*/
			bUseHWPCF = IMG_TRUE;
		}
		else 
		if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_RAWSAMPLEBOTH)
		{
			/*
				This core has special features to accelerate PCF emulation.
			*/
			bEmulatePCF = IMG_TRUE;
		}
		else
		{
			/*
				Otherwise fail.
			*/
			UscFail(psState, UF_ERR_INTERNAL, "PCF isn't supported on this processor");
		}
	}

	/* Sanity check. */
	if (uTextureDimensionality != 1 &&
		uTextureDimensionality != 2 && 
		uTextureDimensionality != 3)
	{
		UscFail(psState, UF_ERR_GENERIC, "Texture dimensionality must be 1, 2, or 3");
	}

	/* Select the format for the TAG to unpack to. */
	SelectTextureOutputFormat(psState, uTexture, GetRegisterFormat(psState, &psSrc->sDest));

	/* Check for a projected lookup. */
	bProjected = IMG_FALSE;
	if (eOpCode == UFOP_LDP)
	{
		bProjected = IMG_TRUE;
	}

	/*
	  Calculate the number of underlying texture reads required.
	*/
	GetTextureResultCount(psState, uTexture, uSamplerSwizzle, psLayout);
	
	/*
	  Texture arrays require using different texture state for each instance. The hardware
	  only supports this if we are in per-instance mode (psState->uNumDynamicBranches > 0).
	*/
	if (psState->psSAOffsets->asTextureDimensionality[uTexture].bIsArray)
	{
		psState->uNumDynamicBranches++;
	}

	/* 
		Does the input instruction needs to add offset to coordinates.
	*/
	GetSampleInputParameters(psState, psSrc, eOpCode, uTextureDimensionality, &sInputParams);

	SetupTextureSampleParameters(psState,
								 psCodeBlock,
								 psSrc,
								 eOpCode,
								 uTextureDimensionality,
								 (bUseHWPCF || bEmulatePCF) ? IMG_TRUE : IMG_FALSE,
								 bEmulatePCF,
								 bProjected,
								 uTexture,
								 &eLODMode,
								 &sCoordinates,
								 &sLODAdjust,
								 &sGradients,
								 &sProj,
								 &sInputParams,
								 &bSampleIdxPresent,
								 &sSampleIdx,
								 &eSpecialReturnData);

	/*
		Get the register format of the destination.
	*/
	eDestRegisterFormat = GetSampleRegisterFormat(psState, uTexture, eSpecialReturnData);

	/*
		Set up information to unpack from the special results of these texture sample types.
	*/
	if (sInputParams.eSpecialReturnMode == UF_LDPARAM_SPECIALRETURN_SAMPLEINFO)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_RAWSAMPLE) != 0)
		{
			CreateSINFTextureResultLayout(psState, psLayout, uSamplerSwizzle);
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545) */
		{
			UscFail(psState, UF_ERR_GENERIC, "The LDLOD instruction isn't supported on this core");
		}
	}
	else if (eOpCode == UFOP_LDGATHER4)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_RAWSAMPLE) != 0)
		{
			CreateGather4TextureResultLayout(psState, psLayout);
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545) */
		{
			UscFail(psState, UF_ERR_GENERIC, "The LDGATHER4 instruction isn't supported on this core");
		}
	}
	else if (sInputParams.eSpecialReturnMode == UF_LDPARAM_SPECIALRETURN_RAWSAMPLES)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_RAWSAMPLE) != 0)
		{
			CreateRawSamplesTextureResultLayout(psState, psLayout, uTexture);
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545) */
		{
			UscFail(psState, UF_ERR_GENERIC, "A texture sample returning prefiltered samples isn't supported on this core");
		}
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (bEmulatePCF)
	{
		ASSERT(eSpecialReturnData == SMP_RETURNDATA_NORMAL);

		EmulatePCF_Vec(psState,
					   psCodeBlock,
					   psLayout,
					   psResult,
					   eLODMode,
					   bConditional,
					   eDestRegisterFormat,
					   uTexture,
					   uTextureDimensionality,
					   &sCoordinates,
					   &sLODAdjust,
					   &sGradients,
					   &sProj,
					   &sInputParams.sOffsets,
					   bSampleIdxPresent,
					   &sSampleIdx,
					   uSamplerSwizzle);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		/*
			Generate sample instructions to read each chunk of the texture.
		*/
		EmitSampleDataInstruction(psState,
								  psCodeBlock,
								  eLODMode,
								  bConditional,
								  eDestRegisterFormat,
								  psLayout,
								  psResult,
								  uTexture,
								  uTextureDimensionality,
								  bUseHWPCF,
								  &sCoordinates,
								  &sLODAdjust,
								  &sGradients,
								  &sProj,
								  &sInputParams.sOffsets,
								  bSampleIdxPresent,
								  &sSampleIdx,
								  eSpecialReturnData);
	}
	return psCodeBlock;
}
#endif /* defined(OUTPUT_USCHW) */

#if defined(OUTPUT_USPBIN)
static
IMG_VOID GetUSPTextureSampleDestinationDetails(UF_REGFORMAT			eInputDestFormat,
											   IMG_UINT32			uChanMask,
											   IMG_PUINT32			puDestCount,
											   IMG_PUINT32			auDestMask)
{
	IMG_UINT32	uDestCount;
	IMG_UINT32	uDestOffset;

	/*
		Set the destination count.
	*/
	switch (eInputDestFormat)
	{
		default:
		case UF_REGFORMAT_F32:
		{
			uDestCount = 4;
			break;
		}

		case UF_REGFORMAT_F16:
		{
			uDestCount = 2;
			break;
		}

		case UF_REGFORMAT_C10:
		case UF_REGFORMAT_U8:
		{
			uDestCount = 1;
			break;
		}
	}

	/*
	  Setup the destination masks for each destination register used, based upon
	  the channels that need to be written
	*/
	for	(uDestOffset = 0; uDestOffset < uDestCount; uDestOffset++)
	{
		IMG_UINT32	uDestMask;

		uDestMask = 0;

		switch (eInputDestFormat)
		{
			default:
			case UF_REGFORMAT_F32:
			{
				if	(uChanMask & (1U << uDestOffset))
				{
					uDestMask |= USC_DESTMASK_FULL;
				}
				break;
			}

			case UF_REGFORMAT_F16:
			{
				if	(uChanMask & (0x1U << (uDestOffset*2)))
				{
					uDestMask |= 0x3U;
				}
				if	(uChanMask & (0x2U << (uDestOffset*2)))
				{
					uDestMask |= 0xCU;
				}
				break;
			}

			case UF_REGFORMAT_C10:
			case UF_REGFORMAT_U8:
			{
				uDestMask |= uChanMask;
				break;
			}
		}

		auDestMask[uDestOffset]		 = uDestMask;
	}

	/*
	  Minimise the number of destination registers
	*/
	for	(uDestOffset = uDestCount; uDestOffset-- > 0;)
	{
		if	(!auDestMask[uDestOffset])
		{
			uDestCount--;
		}
		else
		{
			break;
		}
	}

	*puDestCount = uDestCount;
}

static
IMG_BOOL GenerateNonDependentTextureSampleUSP(PINTERMEDIATE_STATE	psState,
											  PCODEBLOCK			psCodeBlock,
											  UF_OPCODE				eUFOpcode,
											  PUF_REGISTER			psUFCoordSrc,
											  IMG_UINT32			uTextureIdx,
											  IMG_BOOL				bProjected,
											  IMG_UINT32			uTextureDim,
											  IMG_UINT32			uDestCount,
											  PARG					asDest,
											  IMG_UINT32			auDestMask[],
											  UF_REGFORMAT			eTexPrecision,
											  IMG_UINT32			uChanMask,
											  IMG_UINT32			uChanSwizzle,
											  IMG_BOOL				bConditional)
/*****************************************************************************
 FUNCTION	: GenerateNonDependentTextureSampleUSP

 PURPOSE	: Try to add a input texture sample as a USP non-dependent texture
			  sample.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  eUFOpcode			- Opcode of the input texture sample.
			  psUFCoordSrc		- Texture coordinate source to the input
								texture sample.
			  uTextureIdx		- Index of the texture to sample.
			  bProjected		- TRUE if the texture sample is projected.
			  uTextureDim		- Dimensionality of the texture being sampled.
			  uDestCount		- Count of destinations to write from the non-dependent
								texture sample.
			  asDest			- Array of destinations to write.
			  auDestMask		- Mask of channels to write in each destination.
			  eTexPrecision		- Hint of the maximum precision of the data in the
								texture.
			  uChanMask			- Input destination write mask.
			  uChanSwizzle		- Swizzle to apply to the data sampled from the
								texture.
			  bConditional		- TRUE if the sample is inside dynamic flow control.

 RETURNS	: TRUE if a non-dependent texture sample was generated.
*****************************************************************************/
{
	PPIXELSHADER_INPUT			psInput;
	UNIFLEX_TEXLOAD_FORMAT		eFormat;
	IMG_UINT32					uNumAttributes;
	PINST						psSmpInst;
	PINST						psSmpUnpackInst;
	IMG_UINT32					uArg;
	PARG						psArg;
	IMG_UINT32					uSCMask;
	IMG_UINT32					uCoordMask;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_TAG_UNPACK_RESULT) != 0)
	{
		switch (asDest[0].eFmt)
		{
			case UF_REGFORMAT_F32:
			{
				eFormat = UNIFLEX_TEXLOAD_FORMAT_F32;
				uNumAttributes = 4;
				break;
			}

			case UF_REGFORMAT_F16:
			{
				eFormat = UNIFLEX_TEXLOAD_FORMAT_F16;
				uNumAttributes = 2;
				break;
			}
			
			/* C10/U8 does not require any format conversion on SGX543 */
			default:
			{
				eFormat = UNIFLEX_TEXLOAD_FORMAT_UNDEF;
				uNumAttributes = 1;
				break;
			}
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) || defined(SUPPORT_SGX545) */
	{
		eFormat = UNIFLEX_TEXLOAD_FORMAT_UNDEF;
		uNumAttributes = 1;
	}

	/* 
	   Determine whether this sample is dependent upon data generated within
	   the shader
	*/
	uCoordMask = (1U << uTextureDim) - 1;
	if (bProjected)
	{
		uCoordMask |= (1U << UFREG_DMASK_A_SHIFT);
	}
	uSCMask = MaskToSwiz(uCoordMask);

	if	(
			!(
				(psUFCoordSrc->eType == UFREG_TYPE_TEXCOORD) &&
				(psUFCoordSrc->byMod == 0) &&
				(psUFCoordSrc->eRelativeIndex == UFREG_RELATIVEINDEX_NONE) &&
				(psUFCoordSrc->u.uSwiz & uSCMask) == (UFREG_SWIZ_NONE & uSCMask) &&
				(psUFCoordSrc->eFormat != UF_REGFORMAT_U8) &&
				(eUFOpcode != UFOP_LDB) &&
				(eUFOpcode != UFOP_LDL) &&
				(eUFOpcode != UFOP_LDD) &&
				(eUFOpcode != UFOP_LD2DMS) &&
				(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL) &&
				GetBit(psState->psSAOffsets->auFlatShadedTextureCoordinates, psUFCoordSrc->uNum) == 0
			 )
		)
	{
		return IMG_FALSE;
	}

	/*
		If the texture sample is inside dynamic flow control then don't generate a
		non-dependent sample. This is because the USP might need to convert a non-dependent
		sample back to a dependent sample but a dependent sample inside dynamic flow control
		requires extra USSE instructions to calculate gradients which the USP doesn't generate.
	*/
	if (bConditional)
	{
		return IMG_FALSE;
	}

	/*
	  If we have to add a new entry for a non-dependent read, make sure
	  that the maximum number hasn't been reached.
	*/
	{
		PUSC_LIST_ENTRY		psInputListEntry;
		PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

		/*
		  Determine whether we have seen an identical non-dependent read already
		*/
		for (psInputListEntry = psPS->sPixelShaderInputs.psHead;
			 psInputListEntry != NULL;
			 psInputListEntry = psInputListEntry->psNext)
		{
			PPIXELSHADER_INPUT		psInputL = IMG_CONTAINING_RECORD(psInputListEntry, PPIXELSHADER_INPUT, sListEntry);
			PUNIFLEX_TEXTURE_LOAD	psTexLoad = &psInputL->sLoad;

			if	(
				(psTexLoad->uTexture == uTextureIdx) &&
				(psTexLoad->uChunk == 0) &&
				(psTexLoad->eIterationType == UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE) &&
				(psTexLoad->uCoordinate	== psUFCoordSrc->uNum) &&
				(psTexLoad->uCoordinateDimension == uTextureDim) &&
				(psTexLoad->bProjected == bProjected) &&
				(psTexLoad->eFormat == eFormat)
				)
			{
				break;
			}
		}

		/*
		  If we have already added the maximum possible number of non-dependent
		  samples, handle this one as a dependent read instead.
		*/
		if	(psInputListEntry == NULL)
		{
			IMG_UINT32	uNumLoads;

			uNumLoads = psPS->uNrPixelShaderInputs;
			if	(
				(uNumLoads == USC_USPBIN_MAXIMUM_PDS_DOUTI_COMMANDS) ||
				(psPS->uIterationSize == EURASIA_USE_PRIMATTR_BANK_SIZE)
				)
			{
				return IMG_FALSE;
			}
		}
	}

	/*
	  Reserve a PA register for the first chunk of pre-sampled data.

	  NB:	All non-dependent samples require at-least 1 PA register,
	  although they may need more. To simplify the USP, additional
	  chunks must be sampled to PA registers after all the iterated
	  data (since the compiler does not know about them).
	*/

	/*
		Add an entry to list of pixel shader inputs representing the
		non-dependent texture sample.
	*/
	psInput = 
		AddOrCreateNonDependentTextureSample(psState,
											 uTextureIdx,
											 0 /* uChunk */,
											 psUFCoordSrc->uNum,
											 uTextureDim,
											 bProjected,
											 uNumAttributes, 
											 uNumAttributes,
											 UF_REGFORMAT_UNTYPED,
											 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
											 IMG_FALSE /* bVector */,
											 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
											 eFormat);

	/*
	  Setup basic details of an intermediate SMP instruction
	*/
	psSmpInst = AllocateInst(psState, IMG_NULL);
	SetOpcodeAndDestCount(psState, psSmpInst, ISMP_USP_NDR, UF_MAX_CHUNKS_PER_TEXTURE + uDestCount);
	
	psState->uOptimizationHint |= USC_COMPILE_HINT_OPTIMISE_USP_SMP;

	/*
		Create an instruction representing the conversion of the raw sample texture result to
		the destination format.
	*/
	psSmpUnpackInst = CreateUSPSampleUnpackInstruction(psState, psSmpInst, uChanMask, uDestCount, asDest, auDestMask);

	/*
	  Initialise all non-dependent samples with 0 sources
	*/
	for (uArg = 0; uArg < psSmpInst->uArgumentCount; uArg++)
	{
		psArg = &psSmpInst->asArg[uArg];

		psArg->uType	= USEASM_REGTYPE_IMMEDIATE;
		psArg->uNumber	= 0;
	}

	/*
	  Record the pre-sampled data as the only source
	*/
	ASSERT(uNumAttributes <= NON_DEP_SMP_TFDATA_ARG_MAX_COUNT);
	for (uArg = 0; uArg < uNumAttributes; uArg++)
	{
		psArg = &psSmpInst->asArg[NON_DEP_SMP_TFDATA_ARG_START + uArg];
		psArg->uType	= USEASM_REGTYPE_TEMP;
		psArg->uNumber	= psInput->psFixedReg->auVRegNum[uArg];
		psArg->eFmt		= UF_REGFORMAT_UNTYPED;
	}

	/*
	  Record information specific to sample instructions
	*/
	psSmpInst->u.psSmp->uTextureStage		= uTextureIdx;
	psSmpInst->u.psSmp->uGradSize			= 0;
	psSmpInst->u.psSmp->uCoordSize			= uTextureDim;
	psSmpInst->u.psSmp->uDimensionality		= uTextureDim;

	/*
		Not used for non-dependent reads.
	*/
	psSmpInst->u.psSmp->uUsedCoordChanMask	= 0;

	/*
	  Record information specific to USP pseudo-samples
	*/
	psSmpInst->u.psSmp->sUSPSample.bNonDependent	= IMG_TRUE;
	psSmpInst->u.psSmp->sUSPSample.uChanMask		= uChanMask;
	psSmpInst->u.psSmp->sUSPSample.uChanSwizzle		= uChanSwizzle;
	psSmpInst->u.psSmp->sUSPSample.eTexPrecision	= eTexPrecision;

	/*
	  Record data specific to non-dependent samples
	*/
	psSmpInst->u.psSmp->sUSPSample.bProjected	= bProjected;
	psSmpInst->u.psSmp->sUSPSample.bCentroid	= psInput->sLoad.bCentroid;
	psSmpInst->u.psSmp->sUSPSample.uNDRTexCoord	= psUFCoordSrc->uNum;

	/*
		Add the instruction to the flow control block.
	*/
	AppendInst(psState, psCodeBlock, psSmpInst);

	/*
		Add the sample unpack instruction to the flow control block.
	*/
	AppendInst(psState, psCodeBlock, psSmpUnpackInst);

	return IMG_TRUE;
}	

/*****************************************************************************
 FUNCTION	: ConvertSamplerInstructionCoreUSP

 PURPOSE	: Convert an input texture sample instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  uChanMask			- Mask indicating which destination channels
								  need to be written
			  uChanSwizzle		- Swizzle to be applied to the sampled data
			  asDest			- Array of destinations to write.
			  bConditional		- Whether the sample instruction is inside
								  a dynamic flow-control construct

 RETURNS	: Code block.
*****************************************************************************/
IMG_INTERNAL 
PCODEBLOCK ConvertSamplerInstructionCoreUSP(PINTERMEDIATE_STATE	psState,
											PCODEBLOCK			psCodeBlock,
											PUNIFLEX_INST		psInputInst,
											IMG_UINT32			uChanMask,
											IMG_UINT32			uChanSwizzle,
											PARG				asDest,
											IMG_BOOL			bConditional)
{
	PUF_REGISTER			psUFCoordSrc;
	UF_OPCODE				eUFOpcode;
	IMG_BOOL				bProjected; 
	IMG_UINT32				uTextureIdx;
	IMG_UINT32				uTextureDim;
	UF_REGFORMAT			eTexPrecision;
	IMG_UINT32				uDestCount;
	IMG_UINT32				auDestMask[CHANNELS_PER_INPUT_REGISTER];

	/*
	  Setup a pseudo sample instruction to sample all 4 texture-channels
	  into one or more consecutive registers, starting with the specified
	  destination.
	*/
	eUFOpcode		= psInputInst->eOpCode;
	psUFCoordSrc	= &psInputInst->asSrc[0];
	uTextureIdx		= psInputInst->asSrc[1].uNum;
	uTextureDim		= GetTextureCoordinateUsedChanCount(psState, uTextureIdx);

	/* Sanity check. */
	if	((uTextureDim != 1) && (uTextureDim != 2) && (uTextureDim != 3))
	{
		UscFail(psState, UF_ERR_INTERNAL, "Pixelflex only supports texture dimensions of 2 or 3");
	}

	/* Check for a projected lookup. */
	if	(eUFOpcode == UFOP_LDP)
	{
		bProjected = IMG_TRUE;
	}
	else
	{
		bProjected = IMG_FALSE;
	}

	/*
		Use the precision of the sampler arguments to the input texture sample instruction as
		a hint about the maximum precision of the data in the texture being sampled.
	*/
	eTexPrecision	= GetRegisterFormat(psState, &psInputInst->asSrc[1]);

	/*
		We can assume that there are no texture formats with channels whose precisions
		are greater than U8 but less than F16. Therefore if the texture precision is at
		most C10 in fact it is at most U8.
	*/
	if (eTexPrecision == UF_REGFORMAT_C10)
	{
		eTexPrecision = UF_REGFORMAT_U8;
	}

	/*
		Convert the input destination mask/format to a count of 32-bit registers and a
		mask for each register.
	*/
	GetUSPTextureSampleDestinationDetails(asDest[0].eFmt, uChanMask, &uDestCount, auDestMask);

	/*
		Try to generate a USP non-dependent texture sample.
	*/
	if	(!GenerateNonDependentTextureSampleUSP(psState,
											   psCodeBlock,
											   eUFOpcode,
											   psUFCoordSrc,
											   uTextureIdx,
											   bProjected,
											   uTextureDim,
											   uDestCount,
											   asDest,
											   auDestMask,
											   eTexPrecision,
											   uChanMask,
											   uChanSwizzle,
											   bConditional))
	{
		SAMPLE_COORDINATES	sCoordinates;
		SAMPLE_LOD_ADJUST	sLODAdjust;
		SAMPLE_GRADIENTS	sGradients;
		SAMPLE_PROJECTION	sProj;
		LOD_MODE			eLODMode;
		IMG_BOOL			bSampleIdxPresent;
		ARG					sSampleIdx;
		
		/*
			Fetch the coordinate, lod and gradient source values for dependent
			samples.
		*/
		SetupTextureSampleParameters(psState,
									 psCodeBlock,
									 psInputInst,
									 eUFOpcode,
									 uTextureDim,
									 IMG_FALSE /* bPCF */,
									 IMG_FALSE /* bEmulatePCF */,
									 bProjected,
									 uTextureIdx,
									 &eLODMode,
									 &sCoordinates,
									 &sLODAdjust,
									 &sGradients,
									 &sProj,
									 NULL /* psImmOffsets */,
									 &bSampleIdxPresent,
									 &sSampleIdx,
									 NULL /* peSpecialReturnData */);

		/*
			Generate instructions to apply projection to the texture coordinates.
		*/
		if (sProj.bProjected)
		{
			sCoordinates.uCoordStart = 
				ProjectCoordinates(psState,
								   psCodeBlock,
								   NULL /* psInsertBeforeInst */,
								   &sProj.sProj,
								   NULL /* psProjSrcInst */,
								   USC_UNDEF /* uProjSrcIdx */,
								   sProj.uProjChannel,
								   sCoordinates.uCoordSize,
								   sCoordinates.uUsedCoordChanMask,
								   sCoordinates.eCoordFormat,
								   NULL /* psCoordSrcInst */,
								   USC_UNDEF /* uCoordSrcIdx */,
								   sCoordinates.uCoordStart);
		}


		/*
			Create a USP pseudo-sample instruction.
		*/
		BaseEmitSampleInstructionWithState(psState,
										   psCodeBlock,
										   eLODMode,
										   bConditional,
										   uDestCount,
										   asDest,
										   auDestMask,
										   uTextureIdx,
										   0 /* uFirstPlane */,
										   uTextureDim,
										   IMG_FALSE /* bUsePCF */,
										   &sCoordinates,
										   NULL /* psTextureState */,
										   1 /* uPlaneCount */,
										   NULL /* auResultCountPerPlane */,
										   NULL /* psImmOffsets */,
										   &sLODAdjust,
										   &sGradients,
										   NULL /* psProjection */,
										   SMP_RETURNDATA_NORMAL,
										   IMG_TRUE /* bUSPSample */,
										   eTexPrecision,
										   uChanMask,
										   uChanSwizzle,
										   bSampleIdxPresent,
										   &sSampleIdx);
	}

	return psCodeBlock;
}
#endif	/* #if defined(OUTPUT_USPBIN) */

#if defined(OUTPUT_USCHW)
IMG_INTERNAL
IMG_VOID ConvertLoadTiledInstructionCore(PINTERMEDIATE_STATE	psState,
										 PCODEBLOCK				psCodeBlock,
										 PUNIFLEX_INST			psInputInst, 
										 PARG					psBase,
										 PARG					psStride,
										 PSAMPLE_RESULT_LAYOUT	psLayout,
										 PSAMPLE_RESULT			psResult)
/*****************************************************************************
 FUNCTION	: ConvertLoadTiledInstructionCore

 PURPOSE	: Convert an input tiled load instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psInputInst		- Instruction to convert.
			  psBase			- Argument containing the base address of the tile
								buffer in memory.
			  psStride			- Argument containing the packed X and Y strides of
								the tile buffer.
			  psLayout			- Returns details of the layout of the packed result
								of the memory load.
			  psResult			- Returns details of the intermediate registers containing
								

 RETURNS	: None.
*****************************************************************************/
{
	IMG_UINT32				uTexture;
	IMG_UINT32				uBitsPerTexel;
	IMG_UINT32				uChan;
	PUNIFLEX_TEXFORM		psTexForm;
	PINST					psLDTiledInst;
	IMG_UINT32				uDwordsPerTexel;
	IMG_UINT32				uRawDestBaseRegNum;
	SAMPLE_RESULT_LOCATION	asChanLocations[CHANNELS_PER_INPUT_REGISTER];
	IMG_UINT32				uDest;
	IMG_UINT32				uChunk;
	IMG_UINT32				uSwizzle;
	
	ASSERT(psInputInst->asSrc[2].eType == UFREG_TYPE_TEX);
	uTexture = psInputInst->asSrc[2].uNum;

	/*
		Get the number of bits to load.
	*/
	uBitsPerTexel = 0;
	psTexForm = &psState->psSAOffsets->asTextureParameters[uTexture].sFormat;
	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		USC_CHANNELFORM			eFormat;
		IMG_UINT32				uChanBitWidth;
		PSAMPLE_RESULT_LOCATION	psLocation;
		IMG_UINT32				uBitOffsetInReg;

		eFormat = psTexForm->peChannelForm[uChan];
		uChanBitWidth = g_puChannelFormBitCount[eFormat];

		/*
			Set the location of the raw data for this channel.
		*/
		psLocation = &asChanLocations[uChan];
		psLocation->uChunkIdx = 0;
		psLocation->eFormat = eFormat;
		psLocation->uRegOffsetInChunk = uBitsPerTexel / BITS_PER_UINT;
		uBitOffsetInReg = uBitsPerTexel % BITS_PER_UINT;
		ASSERT((uBitsPerTexel % BITS_PER_BYTE) == 0);
		psLocation->uChanOffset = uBitOffsetInReg / BITS_PER_BYTE;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		psLocation->bVector = IMG_FALSE;
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		uBitsPerTexel += uChanBitWidth;
	}
	uDwordsPerTexel = (uBitsPerTexel + BITS_PER_UINT - 1) / BITS_PER_UINT;

	/*
		Allocate new intermediate registers to hold the results of the memory load before
		unpacking.
	*/
	uRawDestBaseRegNum = GetNextRegisterCount(psState, uDwordsPerTexel);
	
	/*
		Create the memory load instruction.
	*/
	psLDTiledInst = AllocateInst(psState, NULL /* psSrcLineInst */);

	ASSERT(uDwordsPerTexel <= EURASIA_USE_MAXIMUM_REPEAT);

	SetOpcodeAndDestCount(psState, psLDTiledInst, ILDTD, uDwordsPerTexel);
	for (uDest = 0; uDest < uDwordsPerTexel; uDest++)
	{
		SetDest(psState, psLDTiledInst, uDest, USEASM_REGTYPE_TEMP, uRawDestBaseRegNum + uDest, UF_REGFORMAT_UNTYPED);
	}
	
	SetSrcFromArg(psState, psLDTiledInst, TILEDMEMLOAD_BASE_ARGINDEX, psBase);
	SetSrcFromArg(psState, psLDTiledInst, TILEDMEMLOAD_OFFSET_ARGINDEX, psStride);
	SetSrc(psState, psLDTiledInst, TILEDMEMLOAD_DRC_ARGINDEX, USEASM_REGTYPE_DRC, 0 /* uNewSrcNumber */, UF_REGFORMAT_F32);

	AppendInst(psState, psCodeBlock, psLDTiledInst);

	psResult->auRawDataPerChunk[0] = uRawDestBaseRegNum;
	for (uChunk = 1; uChunk < UF_MAX_CHUNKS_PER_TEXTURE; uChunk++)
	{
		psResult->auRawDataPerChunk[uChunk] = USC_UNDEF;
	}

	psLayout->uChunkCount = 1;
	psLayout->asChunk[0].eFormat = UF_REGFORMAT_UNTYPED;
	psLayout->asChunk[0].uSizeInDwords = psLayout->asChunk[0].uSizeInRegs = uDwordsPerTexel;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	psLayout->asChunk[0].bVector = IMG_FALSE;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Combine the swizzles from the instruction instruction sampler parameter and the swizzle
		implied by the texture format.
	*/
	uSwizzle = CombineInputSwizzles(psTexForm->uSwizzle, psInputInst->asSrc[2].u.uSwiz);

	CreateTextureResultChannelLocations(psState, uSwizzle, asChanLocations, psLayout);
}

/*****************************************************************************
 FUNCTION	: ConvertLoadBufferInstructionCore

 PURPOSE	: Convert an input load buffer instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psSrc				- Instruction to convert.
			  psRawResult		- Raw result loaded by sample instructions.
			  bConditional		- Whether the instruction appears in a conditional

 RETURNS	: None.
*****************************************************************************/
IMG_INTERNAL 
IMG_VOID ConvertLoadBufferInstructionCore(PINTERMEDIATE_STATE	psState, 
										  PCODEBLOCK			psCodeBlock, 
										  PUNIFLEX_INST			psSrc, 
										  PSAMPLE_RESULT_LAYOUT	psLayout,
										  PSAMPLE_RESULT		psResult,
										  IMG_BOOL				bConditional)
{		
	IMG_UINT32 uTexture = psSrc->asSrc[1].uNum;
	IMG_UINT32 uChunk;	
	PUNIFLEX_TEXFORM	psTexForm = &psState->psSAOffsets->asTextureParameters[uTexture].sFormat;

	PVR_UNREFERENCED_PARAMETER(bConditional);

	/* Select the format for the TAG to unpack to. */
	SelectTextureOutputFormat(psState, uTexture, GetRegisterFormat(psState, &psSrc->sDest));	
	
	/*
	  Calculate the number of underlying texture reads required.
	*/
	GetTextureResultCount(psState, uTexture, psSrc->asSrc[1].u.uSwiz, psLayout);
		
	/*
	  Load buffer require using different texture state for each instance. The hardware
	  only supports this if
	  (a) We are in per-instance mode (psState->uNumDynamicBranches > 0)
	  (b) LODMODE=REPLACE or LODMODE=GRADIENTS
	*/
	psState->uNumDynamicBranches++;

	{			
		UF_REGFORMAT	eDestRegisterFormat;
		
		/* 
			Offset we need to add to base address of texture. 
		*/
		ARG	sBaseAddressOffset;
		/* 
			Incremented base address of texture. 
		*/
		ARG	sIncrementedBaseAddress;
		/* 
			Base Address incremente per chunk. 
		*/
		ARG	sBaseAddIncrementPerChunk;
		/* 
			Buffer element's size. 
		*/
		ARG	sBuffElementSize;
		InitInstArg(&sBaseAddressOffset);
		InitInstArg(&sIncrementedBaseAddress);
		InitInstArg(&sBaseAddIncrementPerChunk);
		InitInstArg(&sBuffElementSize);
		ASSERT((psSrc->asSrc[0].eFormat == UF_REGFORMAT_U32));
		/*
			Get source representing the offset over base address. 
		*/
		GetSourceF32(psState, psCodeBlock, &(psSrc->asSrc[0]), 0, &sBaseAddressOffset, IMG_FALSE, NULL);
		sIncrementedBaseAddress.uType = USEASM_REGTYPE_TEMP;
		sIncrementedBaseAddress.uNumber = GetNextRegister(psState);
		sBaseAddIncrementPerChunk.uType = USEASM_REGTYPE_IMMEDIATE;
		sBaseAddIncrementPerChunk.uNumber = psTexForm->uChunkSize;	
		sBuffElementSize.uType = USEASM_REGTYPE_IMMEDIATE;
		sBuffElementSize.uNumber = psLayout->uTexelSizeInBytes;
		
		/*
		  Get the register format of the destination.
		*/
		eDestRegisterFormat = GetSampleRegisterFormat(psState, uTexture, SMP_RETURNDATA_NORMAL);

		/*
		  Generate sample instructions to read each chunk of the texture.
		*/
		for (uChunk = 0; uChunk < psLayout->uChunkCount; uChunk++)
		{
			ARG					asStateArgs[SMP_MAX_STATE_SIZE];
			ARG					asSampleDest[USC_MAX_NONCALL_DEST_COUNT];
			SAMPLE_COORDINATES	sCoordinates;

			/* Load the state data for the lookup. */
			SetupSmpStateArgument(psState, 
								  psCodeBlock, 
								  NULL /* psInsertInstBefore */, 
								  asStateArgs, 
								  uTexture, 
								  0 /* uChunk */, 
								  IMG_FALSE /* bTextureArray */,
								  NULL /* psTextureArrayIndex */,
								  NULL /* psImmOffsets */, 
								  1 /* uTextureDimensionality */,
								  IMG_FALSE /* bSampleIdxPresent */,
								  NULL /* psSampleIdx */);

			if(uChunk == 0)
			{	
				/* 
					Calculating new base address for buffer. 
				*/
				GenerateIntegerArithmetic(
											psState, 
											psCodeBlock, 
											NULL /* psInsertBeforeInst */,
											UFOP_MUL, 
											&sIncrementedBaseAddress, 
											NULL /* psDestHigh */, 
											USC_PREDREG_NONE, 
											IMG_FALSE /* bPredNegate */, 											
											&sBaseAddressOffset, 
											IMG_FALSE /* bNegateA */,
											&sBuffElementSize, 
											IMG_FALSE /* bNegateB */,
											IMG_NULL, 
											IMG_FALSE /* bNegateC */,
											IMG_FALSE /* bSigned */);
				if(
					!(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_BUFFER_LOAD) || 
					(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_27904))				
				{
					/* 
						Calculating new base address for current buffer chunk. 
					*/
					GenerateIntegerArithmetic(
												psState, 
												psCodeBlock, 
												NULL /* psInsertBeforeInst */,
												UFOP_ADD, 
												&sIncrementedBaseAddress, 
												NULL /* psDestHigh */, 
												USC_PREDREG_NONE, 
												IMG_FALSE /* bPredNegate */, 
												&asStateArgs[EURASIA_PDS_DOUTT_TEXADDR_WORD_INDEX] /* psArgA */, 
												IMG_FALSE /* bNegateA */,
												&sIncrementedBaseAddress /* psArgB */, 									
												IMG_FALSE /* bNegateB */,
												IMG_NULL /* psArgC */,
												IMG_FALSE /* bNegateC */,
												IMG_FALSE /* bSigned */);
				}
			}
			else
			{
				/* 
					Calculating new base address for current buffer chunk. 
				*/
				GenerateIntegerArithmetic(
											psState, 
											psCodeBlock, 
											NULL /* psInsertBeforeInst */,
											UFOP_ADD, 
											&sIncrementedBaseAddress, 
											NULL /* psDestHigh */, 
											USC_PREDREG_NONE, 
											IMG_FALSE /* bPredNegate */, 
											&sIncrementedBaseAddress /* psArgA */, 
											IMG_FALSE /* bNegateA */,
											&sBaseAddIncrementPerChunk /* psArgB */,
											IMG_FALSE /* bNegateB */,
											IMG_NULL /* psArgC */, 
											IMG_FALSE /* bNegateC */,
											IMG_FALSE /* bSigned */);
			}
			
			if(
				!(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_BUFFER_LOAD) || 
				(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_27904)
			  )
			{
				sCoordinates.uCoordSize = 1;
				sCoordinates.eCoordFormat = UF_REGFORMAT_F32;
				sCoordinates.uCoordType = USEASM_REGTYPE_IMMEDIATE;
				sCoordinates.uCoordStart = 0;
				sCoordinates.uUsedCoordChanMask = USC_XYZW_CHAN_MASK;

				InitInstArg(&asStateArgs[EURASIA_PDS_DOUTT_TEXADDR_WORD_INDEX]);
				asStateArgs[EURASIA_PDS_DOUTT_TEXADDR_WORD_INDEX] = sIncrementedBaseAddress;
			}
			else			
			{
				sCoordinates.uCoordSize = 1;
				sCoordinates.eCoordFormat = UF_REGFORMAT_F32;
				sCoordinates.uCoordType = USEASM_REGTYPE_TEMP;
				sCoordinates.uCoordStart = sIncrementedBaseAddress.uNumber;
				sCoordinates.uUsedCoordChanMask = USC_XYZW_CHAN_MASK;
			}
			sCoordinates.bTextureArray = IMG_FALSE;
	
			psResult->auRawDataPerChunk[uChunk] = 
				GetNextRegisterCount(psState, psLayout->asChunk[uChunk].uSizeInRegs);

			MakeArgumentSet(asSampleDest, 
							psLayout->asChunk[uChunk].uSizeInRegs, 
							USEASM_REGTYPE_TEMP, 
							psResult->auRawDataPerChunk[uChunk],
							eDestRegisterFormat);

			BaseEmitSampleInstructionWithState(psState,
											   psCodeBlock,
											   LOD_MODE_REPLACE,
											   bConditional,
											   psLayout->asChunk[uChunk].uSizeInRegs,
											   asSampleDest,
											   NULL,
											   uTexture,
											   0 /* uFirstPlane */,
											   1 /* uTextureDimensionality */,
											   IMG_FALSE /* bUsePCF */,
											   &sCoordinates /* psCoordinates */,
											   asStateArgs /* psTextureState */,
											   1 /* uPlaneCount */,
											   NULL /* auResultCountPerPlane */,
											   NULL /* psImmOffsets */,
											   NULL /* psLODAdjust */,
											   NULL /* psGradients */,
											   NULL /* psProjection */,
											   SMP_RETURNDATA_NORMAL,
											   IMG_FALSE /* bUSPSample */,
											   UF_REGFORMAT_INVALID /* eTexPrecision */,
											   USC_UNDEF /* uChanMask */,
											   USC_UNDEF /* uChanSwizzle */,
											   IMG_FALSE, /* bSampleIdxPresent */
											   NULL /* psSampleIdx */);
		}
	}
}
#endif /* defined(OUTPUT_USCHW) */

/******************************************************************************
 FUNCTION    : ConvertLockToIntermediate

 DESCRIPTION : Converts a LOCK instruction in the input program

 PARAMETERS  : psState    - Compiler intermediate state
			   psBlock    - Block to which converted ILOCK inst should be
							   appended, or NULL to skip it

 RETURNS     : Nothing
 ******************************************************************************/
static
IMG_VOID ConvertLockToIntermediate(PINTERMEDIATE_STATE psState,
								   PCODEBLOCK psBlock,
								   PUNIFLEX_INST psSrc)
{
	PINST psInst = AllocateInst(psState, IMG_NULL);
	
	if (!psBlock)
		return;

	SetOpcode(psState, psInst, ILOCK);
	InitInstArg(&psInst->asArg[0]);

	if (psSrc->asSrc[0].eType != UFREG_TYPE_IMMEDIATE)
	{
		UscAbort(psState, UF_ERR_INVALID_SRC_REG, "Argument for LOCK instruction must be an immediate", IMG_NULL, 0);
	}
	psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[0].uNumber = psSrc->asSrc[0].uNum;

	psState->uFlags2 |= USC_FLAGS2_USES_MUTEXES;

	AppendInst(psState, psBlock, psInst);
}

/******************************************************************************
 FUNCTION    : ConvertReleaseToIntermediate

 DESCRIPTION : Converts a RELEASE instruction in the input program

 PARAMETERS  : psState    - Compiler intermediate state
			   psBlock    - Block to which converted IRELEASE inst should be
							   appended, or NULL to skip it

 RETURNS     : Nothing
 ******************************************************************************/
static
IMG_VOID ConvertReleaseToIntermediate(PINTERMEDIATE_STATE psState,
									  PCODEBLOCK psBlock,
									  PUNIFLEX_INST psSrc)
{
	PINST psInst = AllocateInst(psState, IMG_NULL);

	if (!psBlock)
		return;

	SetOpcode(psState, psInst, IRELEASE);
	InitInstArg(&psInst->asArg[0]);

	if (psSrc->asSrc[0].eType != UFREG_TYPE_IMMEDIATE)
	{
		UscAbort(psState, UF_ERR_INVALID_SRC_REG, "Argument for RELEASE instruction must be an immediate", IMG_NULL, 0);
	}
	psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[0].uNumber = psSrc->asSrc[0].uNum;

	AppendInst(psState, psBlock, psInst);
}

/******************************************************************************
 FUNCTION    : IsInstructionAtomic

 DESCRIPTION : Checks if the given opcode corresponds to an atomic operation

 PARAMETERS  : psSrc    - Instruction to check

 RETURNS     : True if this is an atomic instruction
 ******************************************************************************/
IMG_INTERNAL IMG_BOOL IsInstructionAtomic(UF_OPCODE eOpcode)
{
	switch	(eOpcode)
	{
		case UFOP_ATOM_ADD:
		case UFOP_ATOM_INC:
		case UFOP_ATOM_SUB:
		case UFOP_ATOM_DEC:
		case UFOP_ATOM_XOR:
		case UFOP_ATOM_AND:
		case UFOP_ATOM_OR:
		case UFOP_ATOM_XCHG:
		case UFOP_ATOM_CMPXCHG:
		case UFOP_ATOM_MAX:
		case UFOP_ATOM_MIN:
			return IMG_TRUE;
		default:
			return IMG_FALSE;
	}
}

/******************************************************************************
 FUNCTION    : GenerateAtomicLock

 DESCRIPTION : Generates a mutex lock for atomic instructions

 PARAMETERS  : psState    - Compiler intermediate state
			   psBlock    - Block to insert ILOCK inst

 RETURNS     : Nothing
 ******************************************************************************/
IMG_INTERNAL IMG_VOID GenerateAtomicLock(PINTERMEDIATE_STATE psState,
                                         PCODEBLOCK psCodeBlock)
{
	PINST psInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psInst, ILOCK);
	InitInstArg(&psInst->asArg[0]);

	psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[0].uNumber = ATOM_MUTEX_INDEX;

	psState->uFlags2 |= USC_FLAGS2_USES_MUTEXES;

	AppendInst(psState, psCodeBlock, psInst);
}

/******************************************************************************
 FUNCTION    : GenerateAtomicUnlock

 DESCRIPTION : Generates a mutex release for atomic instructions

 PARAMETERS  : psState    - Compiler intermediate state
			   psBlock    - Block to insert IRELEASE inst

 RETURNS     : Nothing
 ******************************************************************************/
IMG_INTERNAL IMG_VOID GenerateAtomicRelease(PINTERMEDIATE_STATE psState,
                                            PCODEBLOCK psCodeBlock)
{
	PINST psInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psInst, IRELEASE);
	InitInstArg(&psInst->asArg[0]);

	psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[0].uNumber = ATOM_MUTEX_INDEX;

	AppendInst(psState, psCodeBlock, psInst);
}

/******************************************************************************
 FUNCTION    : GenerateAtomicLoad

 DESCRIPTION : Generates a memory load for an atomic operation

 PARAMETERS  : psState    - Compiler intermediate state
			   psBlock    - Block to insert load
               psAddress  - Argument containing the address to load from
               psTarget   - Register to load to

 RETURNS     : Nothing
 ******************************************************************************/
IMG_INTERNAL IMG_VOID GenerateAtomicLoad(PINTERMEDIATE_STATE psState,
                                         PCODEBLOCK psCodeBlock,
                                         PARG psAddress,
                                         PARG psTarget)
{
	PINST psInst = AllocateInst(psState, IMG_NULL);
	IMG_UINT uPrimSize;

	SetOpcode(psState, psInst, ILOADMEMCONST);

	switch (psTarget->eFmt)
	{
		case UF_REGFORMAT_F32:
		case UF_REGFORMAT_I32:
		case UF_REGFORMAT_U32:
		{
			uPrimSize = 4;
			break;
		}
		case UF_REGFORMAT_F16:
		case UF_REGFORMAT_U16:
		case UF_REGFORMAT_I16:
		{
			uPrimSize = 2;
			break;
		}
		case UF_REGFORMAT_I8_UN:
		case UF_REGFORMAT_U8_UN:
		{
			uPrimSize = 1;
			break;
		}
		default:
		{
			imgabort();
			return;
		}
	}

	/* Data size to load */
	psInst->u.psLoadMemConst->uDataSize = uPrimSize;

	/* Set the register to load to */
	psInst->asDest[0] = *psTarget;

	/* Set address */
	psInst->asArg[LOADMEMCONST_BASE_ARGINDEX] = *psAddress;

	/* We don't use dynamic offsets */
	psInst->u.psLoadMemConst->bRelativeAddress = IMG_FALSE;
	InitInstArg(&psInst->asArg[LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX]);
	psInst->asArg[LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[LOADMEMCONST_DYNAMIC_OFFSET_ARGINDEX].uNumber = 0;
	InitInstArg(&psInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX]);
	psInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[LOADMEMCONST_STRIDE_ARGINDEX].uNumber = 0;

	/* Atomic operations expand channels, so only load one element */
	InitInstArg(&psInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX]);
	psInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[LOADMEMCONST_STATIC_OFFSET_ARGINDEX].uNumber = 4;

	/* Atomic operations require loads to be synchronized with stores
	 to prevent memory races and cache incoherency */
	psInst->u.psLoadMemConst->bSynchronised = IMG_TRUE;
	psInst->u.psLoadMemConst->bBypassCache = BypassCacheForModifiableShaderMemory (psState);

	/* Finally, add the instruction */
	AppendInst(psState, psCodeBlock, psInst);
}

/******************************************************************************
 FUNCTION    : GenerateAtomicStore

 DESCRIPTION : Generates a memory store for an atomic operation

 PARAMETERS  : psState    - Compiler intermediate state
			   psBlock    - Block to insert store
               psAddress  - Argument containing the address to store to
               psTarget   - Register to store from

 RETURNS     : Nothing
 ******************************************************************************/
IMG_INTERNAL  IMG_VOID GenerateAtomicStore(PINTERMEDIATE_STATE psState,
                                           PCODEBLOCK psCodeBlock,
							               PARG psAddress,
							               PARG psSource)
{
	PINST psInst = AllocateInst(psState, IMG_NULL);
	IOPCODE opcode;
	IMG_UINT uPrimSize;
	IMG_UINT8 uAddrOffset;

	switch (psSource->eFmt)
	{
		case UF_REGFORMAT_F32:
		case UF_REGFORMAT_I32:
		case UF_REGFORMAT_U32:
		{
			uPrimSize = 4;
			uAddrOffset = 1;
			opcode = ISTAD;
			break;
		}
		case UF_REGFORMAT_F16:
		case UF_REGFORMAT_U16:
		case UF_REGFORMAT_I16:
		{
			uPrimSize = 2;
			uAddrOffset = 2;
			opcode = ISTAW;
			break;
		}
		case UF_REGFORMAT_I8_UN:
		case UF_REGFORMAT_U8_UN:
		{
			uPrimSize = 1;
			uAddrOffset = 4;
			opcode = ISTAB;
			break;
		}
		default:
		{
			imgabort();
			return;
		}
	}

	SetOpcode(psState, psInst, opcode);

	/* Set a dummy testination */
	psInst->asDest[0].uType = USC_REGTYPE_DUMMY;
	psInst->asDest[0].uNumber = 0;

	/* Set address to store to */
	psInst->asArg[0] = *psAddress;

	/* Set the static offset */
	InitInstArg(&psInst->asArg[1]);
	psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[1].uNumber = psState->uMemOffsetAdjust + uAddrOffset;
	psInst->asArg[1].uNumber |= (uPrimSize << 16);

	/* Data we want to store */
	psInst->asArg[2] = *psSource;
	
	/* Set synchronised flag */
	psInst->u.psLdSt->bSynchronised = IMG_TRUE;


	/* Finally, add the instruction */
	AppendInst(psState, psCodeBlock, psInst);
}

static PCODEBLOCK ConvertInstToIntermediate(PINTERMEDIATE_STATE psState,
											PCODEBLOCK psBlock,
											PUNIFLEX_INST psSrc,
											IMG_BOOL bConditional,
											IMG_BOOL bStaticCond)
/*****************************************************************************
 FUNCTION	: ConvertInstToIntermediate

 PURPOSE	: Convert an input instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psBlock	- Block to which intermediate instructions will be appended.
			  psSrc				- Instruction to convert.
			  bConditional		- Whether generating conditionally executed code
			  bStaticCond		- Whether condition is static.

 RETURNS	: Block to which subsequent instructions should be appended
	(different from psBlock iff the intermediate code uses flow control)
*****************************************************************************/
{
	UF_REGFORMAT	eFmt;
	
	switch (psSrc->eOpCode)
	{
		case UFOP_NOP:
		{
			/*
			  Format doesn't matter.
			*/
			eFmt = UF_REGFORMAT_F32;
			break;
		}
		case UFOP_KPLT:
		case UFOP_KPLE:
		case UFOP_KILLNZBIT:
		case UFOP_MOVA_RNE:
		{
			/*
			  Take the format from the only source argument.
			*/
			eFmt = GetRegisterFormat(psState, &psSrc->asSrc[0]);
			break;
		}
		case UFOP_SETP:
		{
			/*
			  Take the format from the first temporary register out of
			  the two source arguments to be compared.
			*/
			if (psSrc->asSrc[0].eType == UFREG_TYPE_TEMP)
			{
				eFmt = GetRegisterFormat(psState, &psSrc->asSrc[0]);
			}
			else
			{
				eFmt = GetRegisterFormat(psState, &psSrc->asSrc[2]);
			}
			break;
		}
		case UFOP_MOVA_INT:
		{
			/*
				The format of the source data is defined by the instruction.
			*/
			eFmt = UF_REGFORMAT_U32;
			break;
		}
		case UFOP_LOCK:
		{
			ConvertLockToIntermediate(psState, psBlock, psSrc);
			return psBlock;
		}
		case UFOP_RELEASE:
		{
			ConvertReleaseToIntermediate(psState, psBlock, psSrc);
			return psBlock;
		}
		case UFOP_MEMLD:
		case UFOP_MEMST:
		{
			psBlock = ConvertInstToIntermediateMem(psState, psBlock, psSrc, bConditional, bStaticCond);
			return psBlock;
		}
#if defined(OUTPUT_USPBIN)
		case UFOP_TEXWRITE:
		{
			psBlock = ConvertTextureWriteToIntermediate(psState, psBlock, psSrc);
			return psBlock;
		}
#endif /* defined(OUTPUT_USPBIN) */
		default:
		{
			IMG_UINT32	uNumDests = g_asInputInstDesc[psSrc->eOpCode].uNumDests;

			/*
			  Take the format from the second destination if one is present and written. 
			*/
			ASSERT(uNumDests > 0);
			if (uNumDests == 2 && psSrc->sDest2.u.byMask != 0)
			{
				eFmt = GetRegisterFormat(psState, &psSrc->sDest2);
				ASSERT(psSrc->sDest.u.byMask == 0 || eFmt == GetRegisterFormat(psState, &psSrc->sDest));
			}
			else
			{
				/*
				  Otherwise from the first destination.
				*/
				eFmt = GetRegisterFormat(psState, &psSrc->sDest);
			}
			break;
		}
	}

	switch (eFmt)
	{
		case UF_REGFORMAT_I32:
		case UF_REGFORMAT_U32:
		case UF_REGFORMAT_I16:
		case UF_REGFORMAT_U16:
		case UF_REGFORMAT_I8_UN:
		case UF_REGFORMAT_U8_UN:
			psBlock = ConvertInstToIntermediateInt32(psState, psBlock, psSrc, bConditional, bStaticCond);
			break;
		case UF_REGFORMAT_F32: 
			psBlock = ConvertInstToIntermediateF32(psState, psBlock, psSrc, 
										 bConditional, bStaticCond); 
			break;
		case UF_REGFORMAT_F16: 
			psBlock = ConvertInstToIntermediateF16(psState, psBlock, psSrc, 
										 bConditional, bStaticCond); 
			break;
		case UF_REGFORMAT_C10: 
		case UF_REGFORMAT_U8:
		{
			/* Flag we might need some extra optimization stages. */
			psState->uFlags |= USC_FLAGS_INTEGERUSED;
			psState->uFlags &= ~USC_FLAGS_INTEGERINLASTBLOCKONLY;
			psBlock = ConvertInstToIntermediateC10(psState, psBlock, psSrc, 
										 bConditional, bStaticCond); 
			break;
		}
		default: imgabort();
	}
	return psBlock;
}

//forward decl
static PCODEBLOCK DoConvert(PINTERMEDIATE_STATE psState,
							PCALLDATA psCallData,
							PFUNC psFunc,
							PUNIFLEX_INST *ppsInst, PCODEBLOCK psBlock,
							PCODEBLOCK psBreakTarget,
							PCODEBLOCK psContinueTarget,
							IMG_BOOL bInConditional /*of function or program*/, IMG_BOOL bStatic);

static IMG_BOOL GetInputConstU(PINTERMEDIATE_STATE psState,
							   UF_REGISTER* psReg,
							   IMG_UINT32 uSrcChan,
							   IMG_PUINT32 puValue)
/*****************************************************************************
 FUNCTION	: GetInputConst

 PURPOSE	: Try to get the value of a constant register

 PARAMETERS	: psState			- The compiler state
			  psReg				- The register to get
			  uSrcChan			- Channel to get from the register.

 OUTPUT     : puResult			- The value of the constant, if defined

 RETURNS	: True if constant is defined (static), false otherwise
*****************************************************************************/
{
	/* Most of this is from verif_input.c:GetNormalSourceChan */

	IMG_UINT32 uSwiz = psReg->u.uSwiz;
	IMG_UINT32 uMod = psReg->byMod;
	IMG_UINT32 eRelativeIndex = psReg->eRelativeIndex;
	IMG_UINT32 uChan;
	IMG_UINT32 uResult = 0;
	IMG_BOOL bDefined = IMG_FALSE;

	uChan = (uSwiz >> (uSrcChan * 3)) & 0x7;

	if (uChan <= UFREG_SWIZ_A)
	{
		switch(psReg->eType)
		{
			case UFREG_TYPE_CONST:
			{
				if (eRelativeIndex == UFREG_RELATIVEINDEX_NONE)
				{
					bDefined = CheckForStaticConst(psState, psReg->uNum, uChan, psReg->uArrayTag, &uResult);
				}
				break;
			}
			case UFREG_TYPE_HW_CONST:
			{
				static const IMG_UINT32 auConsts[] = {FLOAT32_ZERO, FLOAT32_ONE, FLOAT32_HALF, FLOAT32_TWO};

				if (eRelativeIndex == UFREG_RELATIVEINDEX_NONE &&
					psReg->uNum < 4)
				{
					uResult = auConsts[psReg->uNum];
					bDefined = IMG_TRUE;
				}
				break;
			}
			case UFREG_TYPE_IMMEDIATE:
			{
				bDefined = IMG_TRUE;
				uResult = psReg->uNum;
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
		static const IMG_UINT32 auSwizConsts[] = {FLOAT32_ONE, FLOAT32_ZERO, FLOAT32_HALF, FLOAT32_ZERO};

		uResult = auSwizConsts[uChan - UFREG_SWIZ_1];
		bDefined = IMG_TRUE;
	}

	/* 	
		Apply modifications. To avoid possible differences between the notion
		of floating point arithmetic used by the compiler and the hardware,
		any modification is treated as resulting in an undefined value.
	*/

	switch ((uMod >> UFREG_SMOD_SHIFT) & (UFREG_SMOD_MASK))
	{
		case UFREG_SMOD_COMPLEMENT:		/* (1-Src) */
		{
			/* fResult = (1.0 - fResult) + 1.0; */
			bDefined = IMG_FALSE;
			break;
		}
		case UFREG_SMOD_SIGNED:			/* (2*(Src-0.5)) */ 
		{
			/*
			  IMG_FLOAT fOneExpr = fResult;
			  fResult = (2.0 * fResult);
			  fResult = fResult - ftemp;
			*/
			bDefined = IMG_FALSE;
			break;
		}
		case UFREG_SMOD_BIAS:			/* (Src-0.5)) */
		{
			/* fResult = fResult - (0.5 * fResult); */
			bDefined = IMG_FALSE;
			break;
		}
		case UFREG_SMOD_TIMES2:
		{
			/* fResult = 2.0 * fResult; */
			bDefined = IMG_FALSE;
			break;
		}
	}
	if ((uMod & UFREG_SOURCE_NEGATE) != 0 || (uMod & UFREG_SOURCE_ABS) != 0)
	{
		bDefined = IMG_FALSE;
	}

	/* Record the constant and report back */
	if (bDefined && puValue != NULL)
	{
		(*puValue) = uResult;
	}
	return bDefined;
}

static IMG_BOOL GetInputConst(PINTERMEDIATE_STATE psState,
							  UF_REGISTER* psReg,
							  IMG_UINT32 uSrcChan,
							  IMG_PFLOAT pfValue)
/*****************************************************************************
 FUNCTION	: GetInputConst

 PURPOSE	: Try to get the value of a constant register

 PARAMETERS	: psState			- The compiler state
			  psReg				- The register to get
			  uSrcChan			- Channel to get from the register.

 OUTPUT     : pfResult			- The value of the constant, if defined

 RETURNS	: True if constant is defined (static), false otherwise
*****************************************************************************/
{
	return GetInputConstU(psState, psReg, uSrcChan, (IMG_PUINT32)pfValue);
}

static IMG_BOOL UFRegIsConst(UF_REGISTER* psReg)
/*****************************************************************************
 FUNCTION	: UFRegIsConst

 PURPOSE	: Test whether an input register is a constant.

 PARAMETERS	: psReg				- The register to test

 RETURNS	: True if psReg is constant, False otherwise.
*****************************************************************************/
{
	switch(psReg->eType)
	{
		case UFREG_TYPE_CONST:
		case UFREG_TYPE_HW_CONST:
		case UFREG_TYPE_ICONST:
		case UFREG_TYPE_BCONST:
		case UFREG_TYPE_IMMEDIATE:
		{
			return IMG_TRUE;
		}
		case UFREG_TYPE_MISC:
		{
			if (psReg->uNum == UF_MISC_FACETYPE || psReg->uNum == UF_MISC_FACETYPEBIT || psReg->uNum == UF_MISC_VPRIM || psReg->uNum == UF_MISC_RTAIDX)
				return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static IMG_BOOL UFConstComp(PINTERMEDIATE_STATE psState,
							PUNIFLEX_INST psInputInst,
							IMG_UINT32 uCompChan,
							IMG_PBOOL pbStatic,
							IMG_PBOOL pbResult)
/*****************************************************************************
 FUNCTION	: UFConstComp

 PURPOSE	: Try to compare two constants.

 PARAMETERS	: psState			- The compiler state
			  psReg1, psReg2	- The input registers to compare (must be constants) 
			  uTest				- The output comparison operator.

 OUTPUT     : pbResult			- Result of the comparison (if not undefined).

 RETURNS	: True if comparison could be carried out, false otherwise.
*****************************************************************************/
{
	IMG_BOOL bResult = IMG_FALSE;
	IMG_FLOAT fArg1 = 0.0, fArg2 = 0.0;
	PUF_REGISTER psReg1 = &psInputInst->asSrc[0],
		psReg2 = &psInputInst->asSrc[2];
	UFREG_COMPOP uCompOp = (UFREG_COMPOP)(psInputInst->asSrc[1].uNum);

	ASSERT (psInputInst->eOpCode == UFOP_IFC ||
			psInputInst->eOpCode == UFOP_BREAKC ||
			psInputInst->eOpCode == UFOP_SETP);

	*pbStatic = (UFRegIsConst(psReg1) && UFRegIsConst(psReg2)) ? IMG_TRUE : IMG_FALSE;
		
	if (pbStatic &&
		GetInputConst(psState, psReg1, uCompChan, &fArg1) &&
		GetInputConst(psState, psReg2, uCompChan, &fArg2))
	{
		/* Both arguments are constants */
		switch (uCompOp)
		{
			case UFREG_COMPOP_GT:
			{
				if (fArg1 > fArg2)
					bResult = IMG_TRUE;
				break;
			}
			case UFREG_COMPOP_EQ:
			{
				if (fArg1 == fArg2)
					bResult = IMG_TRUE;
				break;
			}
			case UFREG_COMPOP_GE:
			{
				if (fArg1 >= fArg2)
					bResult = IMG_TRUE;
				break;
			}
			case UFREG_COMPOP_LT:
			{
				if (fArg1 < fArg2)
					bResult = IMG_TRUE;
				break;
			}
			case UFREG_COMPOP_NE:
			{
				if (fArg1 != fArg2)
					bResult = IMG_TRUE;
				break;
			}
			case UFREG_COMPOP_LE:
			{
				if (fArg1 <= fArg2)
					bResult = IMG_TRUE;
				break;
			}
			default:
			{
				return IMG_FALSE;
			}
		}
	}
	else
	{
		/*
		  One or more arguments is not constant (or perhaps even static).
		  Try for a comparison between the same register.
		*/
		if (psReg1->uNum == psReg2->uNum &&
			psReg1->eType == psReg2->eType &&
			(EXTRACT_CHAN(psReg1->u.uSwiz, uCompChan) ==
			 EXTRACT_CHAN(psReg2->u.uSwiz, uCompChan)) &&
			psReg1->byMod == psReg2->byMod &&
			psReg1->eRelativeIndex == psReg2->eRelativeIndex &&
			psReg1->uArrayTag == psReg2->uArrayTag)
		{
			/* Arguments are the same register */
			switch(uCompOp)
			{
				case UFREG_COMPOP_GT:
				{
					bResult = IMG_FALSE;
					break;
				}
				case UFREG_COMPOP_EQ:
				{
					bResult = IMG_TRUE;
					break;
				}
				case UFREG_COMPOP_GE:
				{
					bResult = IMG_TRUE;
					break;
				}
				case UFREG_COMPOP_LT:
				{
					bResult = IMG_FALSE;
					break;
				}
				case UFREG_COMPOP_NE:
				{
					bResult = IMG_FALSE;
					break;
				}
				case UFREG_COMPOP_LE:
				{
					bResult = IMG_TRUE;
					break;
				}
				default:
				{
					return IMG_FALSE;
				}
			}
		}
		else return IMG_FALSE;

	}

	/* Record the result and return */
	*pbResult = bResult;
	return IMG_TRUE;
}

static IMG_BOOL SearchBackwardsSetp(PINTERMEDIATE_STATE psState,
									PUNIFLEX_INST psFirstInst,
									IMG_UINT32 uUFPredDest,
									IMG_UINT32 uUFPredChan,
									IMG_PBOOL pbStatic,
									IMG_PBOOL pbCompileTimeResult)
{
	PUNIFLEX_INST psPrevInst;
	for (psPrevInst = psFirstInst->psBLink;
		 psPrevInst && !IsInputInstFlowControl(psPrevInst);
		 psPrevInst = psPrevInst->psBLink)
	{
		if (psPrevInst->sDest.eType == UFREG_TYPE_PREDICATE &&
			psPrevInst->sDest.uNum == uUFPredDest &&
			(psPrevInst->sDest.u.byMask & (1U << uUFPredChan)) != 0)
		{
			if (psPrevInst->eOpCode == UFOP_SETP && 
				psPrevInst->uPredicate == UF_PRED_NONE)
			{
				return UFConstComp(psState, psPrevInst, uUFPredChan,
								   pbStatic, pbCompileTimeResult);
			}
			return IMG_FALSE;
		}
	}
	return IMG_FALSE;
}

IMG_INTERNAL
IMG_VOID CheckFaceType(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, IMG_UINT32 uPredDest, IMG_BOOL bFrontFace)
/*****************************************************************************
 FUNCTION	: CheckFaceType

 PURPOSE	: Generates intermediate instructions to set a predicate register
			  depending on whether the current triangle is frontfacing or backfacing.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the instructions to.
			  uPredDest			- Destination predicate.
			  bFrontFace		- If TRUE set the predicate result to TRUE if the triangle
								is front facing; otherwise set the predicate result to
								TRUE if the triangle is back facing.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psCmpInst;

	psCmpInst = AllocateInst(psState, IMG_NULL);
	SetOpcodeAndDestCount(psState, psCmpInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
	psCmpInst->u.psTest->eAluOpcode = IAND;
	MakePredicateArgument(&psCmpInst->asDest[TEST_PREDICATE_DESTIDX], uPredDest);
	psCmpInst->u.psTest->sTest.eType = bFrontFace ? TEST_TYPE_EQ_ZERO : TEST_TYPE_NEQ_ZERO;
	psCmpInst->asArg[0].uType = USEASM_REGTYPE_GLOBAL;
	psCmpInst->asArg[0].uNumber = EURASIA_USE_SPECIAL_BANK_BFCONTROL; 
	psCmpInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
	psCmpInst->asArg[1].uNumber = 1;
	AppendInst(psState, psCodeBlock, psCmpInst);
}

static
IMG_VOID IsBooleanConstSet(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, IMG_UINT32 uPredDest, IMG_UINT32 uBoolConst)
/*****************************************************************************
 FUNCTION	: IsBooleanConstSet

 PURPOSE	: Generates intermediate instructions to set a predicate register 
			  from a boolean constant.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the instructions to.
			  uPredDest			- Destination predicate.
			  uBoolConst		- Boolean constant to set the predicate from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInst;

	/* Check if the boolean constant is set or clear. */
	psInst = AllocateInst(psState, IMG_NULL);
	SetOpcodeAndDestCount(psState, psInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
	MakePredicateArgument(&psInst->asDest[TEST_PREDICATE_DESTIDX], uPredDest);
	
	psInst->u.psTest->eAluOpcode = ISHL;
	psInst->u.psTest->sTest.eType = TEST_TYPE_SIGN_BIT_SET;
	
	psInst->asArg[BITWISE_SHIFTEND_SOURCE].uType = USEASM_REGTYPE_SECATTR;
	psInst->asArg[BITWISE_SHIFTEND_SOURCE].uNumber = psState->psSAOffsets->uBooleanConsts;
	
	psInst->asArg[BITWISE_SHIFT_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
	psInst->asArg[BITWISE_SHIFT_SOURCE].uNumber = 31 - uBoolConst;

	AppendInst(psState, psCodeBlock, psInst);
}

static IMG_VOID DoFlowControlTest(	PINTERMEDIATE_STATE psState,
									PUNIFLEX_INST psFirstInst,
									PCODEBLOCK psBlock,
									PCODEBLOCK psTrueSucc,
									PCODEBLOCK psFalseSucc,
									IMG_BOOL bEndOfStaticLoop,
									IMG_PUINT32 puNumSucc,								  
									PCODEBLOCK* ppsTrueSucc,
									PCODEBLOCK* ppsFalseSucc,
									IMG_PUINT32 puPredSrc,
									IMG_PBOOL   pbPredSrcNegate,
									IMG_PBOOL	pbStatic)
/*****************************************************************************
 FUNCTION	: DoFlowControlTest

 PURPOSE	: Make a block perform a test and select it's successor according to
			  a uniflex flow-control instruction.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- block to which to add instructions and set successors
			  psFirstInst		- The input flow-control instruction.
			  psTrueSucc, psFalseSucc
								- Successor if branch is/not taken
			  bStaticLoop		- TRUE if this is the GLSLENDLOOP corresponding to
								a loop started with GLSLSTATICLOOP.
			  puNumSucc			- Returned number of successors required by flow 
			  					control test.
			  ppsTrueSucc		- Returned true successor for flow control test.
			  ppsFalseSucc		- Returned false successor for flow control test if
			  					number of required successors is 2.
			  puPredSrc			- Returned source predicate for flow control test if
			  					number of required successors is 2.
			  pbStatic			- Returned IMG_TRUE if flow control test is runtime 
			  					static.
*****************************************************************************/
{
	IMG_UINT32 uArg = 0; //default; also inspected after switch for negation
	PCODEBLOCK psCompileTimeSuccessor = NULL;
	IMG_BOOL bNegate = IMG_FALSE;
	IMG_UINT32 uPredSrc = USC_PREDREG_NONE;
	IMG_BOOL bStatic = IMG_FALSE;

	switch (psFirstInst->eOpCode)
	{
		case UFOP_CALLNZ: uArg=1; //and fall-through
		case UFOP_IF:
		{
			switch (psFirstInst->asSrc[uArg].eType)
			{
				case UFREG_TYPE_BCONST:
				{
					IMG_UINT32 uBoolConst = psFirstInst->asSrc[uArg].uNum;

					IsBooleanConstSet(psState, psBlock, USC_PREDREG_TEMP, uBoolConst);
					break;
				}
				case UFREG_TYPE_MISC:
					if (psFirstInst->asSrc[uArg].uNum == UF_MISC_FACETYPE)
					{
						/* Check the facetype. */
						CheckFaceType(psState, psBlock, USC_PREDREG_TEMP, IMG_TRUE /* bFrontFace */);
					}
					else
					{
						UscFail(psState, UF_ERR_GENERIC, "if source argument must be either boolean or facetype.\n");
					}
					break;
				default:
					UscFail(psState, UF_ERR_GENERIC, "if source argument must be a boolean type.\n");
			}
			uPredSrc = USC_PREDREG_TEMP;
			bStatic = IMG_TRUE;
			break; //code after switch will do negation, if necessary.
		}
		case UFOP_CALLP: uArg = 1; //and fall-through
		case UFOP_IFP:
		case UFOP_BREAKP:
		{
			static const IMG_UINT32 ufPreds[] = {UF_PRED_X, UF_PRED_Y, UF_PRED_Z, UF_PRED_W};
			IMG_UINT32 uPredChan = 0;
			IMG_BOOL bTestResult;
			bStatic = IMG_FALSE;
			switch (psFirstInst->asSrc[uArg].u.uSwiz)
			{
				case UFREG_SWIZ_RGBA:
				case UFREG_SWIZ_RRRR: uPredChan = 0; break;
				case UFREG_SWIZ_GGGG: uPredChan = 1; break;
				case UFREG_SWIZ_BBBB: uPredChan = 2; break;
				case UFREG_SWIZ_AAAA: uPredChan = 3; break;
				default: UscFail(psState, UF_ERR_GENERIC, "{IF/BREAK/CALL}P with invalid swizzle.\n");
			}
			/* Try to identify the corresponding SETP instruction */
			if (SearchBackwardsSetp(psState, psFirstInst, psFirstInst->asSrc[uArg].uNum, uPredChan, &bStatic, &bTestResult))
			{
				psCompileTimeSuccessor = bTestResult ? psTrueSucc : psFalseSucc;
			}
			else
			{
				/* Check the predicate register. */
				ASSERT(psFirstInst->asSrc[uArg].uNum < psState->uInputPredicateRegisterCount);
				uPredSrc = USC_PREDREG_P0X +
					(psFirstInst->asSrc[uArg].uNum * CHANNELS_PER_INPUT_REGISTER) +
					((ufPreds[uPredChan] - UF_PRED_X) >> UF_PRED_COMP_SHIFT);
			}
			break;
		}
		case UFOP_IFC:
		case UFOP_BREAKC:
		{
			IMG_BOOL bTestResult;
			//for IFC/BREAKC, never negate - don't check any argument
			uArg = UINT_MAX;

			if (UFConstComp(psState, psFirstInst, UFREG_SWIZ_Z, &bStatic, &bTestResult))
			{
				psCompileTimeSuccessor = bTestResult ? psTrueSucc : psFalseSucc;
				break;
			}
			/* Compare the two arguments. */
			switch (GetRegisterFormat(psState, &psFirstInst->asSrc[0]))
			{
				case UF_REGFORMAT_F32:
				case UF_REGFORMAT_F16:
				{
					CreateComparisonFloat(psState,
										  psBlock,
										  USC_PREDREG_TEMP,
										  (UFREG_COMPOP)psFirstInst->asSrc[1].uNum,
										  &psFirstInst->asSrc[0],
										  &psFirstInst->asSrc[2],
										  UFREG_SWIZ_Z,
										  UFREG_COMPCHANOP_NONE,
										  USC_PREDREG_NONE,
										  IMG_FALSE,
										  IMG_FALSE);
					break;
				}
				case UF_REGFORMAT_U8:
				case UF_REGFORMAT_C10:
				{
					ARG sSrc1, sSrc2;
					IMG_UINT32	uCompChannel = 2;
					IMG_UINT32	uSrc0Chan = EXTRACT_CHAN(psFirstInst->asSrc[0].u.uSwiz, uCompChannel);
					IMG_UINT32	uSrc2Chan = EXTRACT_CHAN(psFirstInst->asSrc[2].u.uSwiz, uCompChannel);

					/*
					  Check if we could generate better code by using another channel and avoiding
					  a swizzle.
					*/
					if (uSrc0Chan == uSrc2Chan &&
						uSrc0Chan <= UFREG_SWIZ_A &&
						EXTRACT_CHAN(psFirstInst->asSrc[0].u.uSwiz, uSrc0Chan) == uSrc0Chan &&
						EXTRACT_CHAN(psFirstInst->asSrc[2].u.uSwiz, uSrc0Chan) == uSrc0Chan)
					{
						uCompChannel = uSrc0Chan;
					}

					GetSourceC10(psState, psBlock, &psFirstInst->asSrc[0], psFirstInst->asSrc[0].byMod,// 0, 
						&sSrc1, 1U << uCompChannel, IMG_FALSE, IMG_TRUE, psFirstInst->asSrc[0].eFormat);
					GetSourceC10(psState, psBlock, &psFirstInst->asSrc[2], psFirstInst->asSrc[2].byMod,// 2, 
						&sSrc2, 1U << uCompChannel, IMG_FALSE, IMG_TRUE, psFirstInst->asSrc[2].eFormat);
					CreateComparisonC10(psState, 
										psBlock, 
										USC_PREDREG_TEMP, 
										(UFREG_COMPOP)psFirstInst->asSrc[1].uNum, 
										(IMG_UINT32)0, 
										&sSrc1, 
										&sSrc2, 
										uCompChannel, 
										USC_PREDREG_NONE, 
										IMG_FALSE,
										IMG_FALSE);
					break;
				}
				case UF_REGFORMAT_U32:
				case UF_REGFORMAT_I32:
				{
					CreateComparisonInt32(psState,
										  psBlock,
										  USC_PREDREG_TEMP,
										  (UFREG_COMPOP)psFirstInst->asSrc[1].uNum,
										  &psFirstInst->asSrc[0],
										  &psFirstInst->asSrc[2],
										  UFREG_SWIZ_Z,
										  UFREG_COMPCHANOP_NONE,
										  USC_PREDREG_NONE,
										  IMG_FALSE,
										  IMG_FALSE);
					break;
				}
				case UF_REGFORMAT_U16:
				case UF_REGFORMAT_I16:
				{
					CreateComparisonInt16(psState,
										  psBlock,
										  USC_PREDREG_TEMP,
										  (UFREG_COMPOP)psFirstInst->asSrc[1].uNum,
										  &psFirstInst->asSrc[0],
										  &psFirstInst->asSrc[2],
										  UFREG_SWIZ_Z,
										  UFREG_COMPCHANOP_NONE,
										  USC_PREDREG_NONE,
										  IMG_FALSE,
										  IMG_FALSE);
					break;
				}
				default:
				{
					/*
					  Invalid register format.
					*/
					imgabort();
				}
			}
			uPredSrc = USC_PREDREG_TEMP;
			break;
		}
		case UFOP_CALLNZBIT: uArg=1; //and fall-through
		case UFOP_BREAKNZBIT:
		case UFOP_RETNZBIT:
		case UFOP_CONTINUENZBIT:
		case UFOP_IFNZBIT:
		{
			PINST		psInst = AllocateInst(psState, IMG_NULL);
			ASSERT(GetRegisterFormat(psState, &psFirstInst->asSrc[uArg]) == UF_REGFORMAT_F32);

			/* Create a test instruction to check for non-zero bits. */
			SetOpcodeAndDestCount(psState, psInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
			psInst->u.psTest->eAluOpcode = IOR;
			MakePredicateArgument(&psInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
			psInst->u.psTest->sTest.eType = TEST_TYPE_NEQ_ZERO;
			/* Get the intermediate version of the source for comparison. */
			GetSourceTypeless(psState,
							  psBlock,
							  &psFirstInst->asSrc[uArg], 
						 	  //0,
							  UFREG_SWIZ_Z, 
							  &psInst->asArg[0], 
							  IMG_FALSE,
							  NULL);
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 0;
			AppendInst(psState, psBlock, psInst);
				
			uPredSrc = USC_PREDREG_TEMP;
			bStatic = UFRegIsConst(&psFirstInst->asSrc[uArg]);
			if (bStatic)
			{
				IMG_UINT32 uArgValue;
				if (GetInputConstU(psState, &psFirstInst->asSrc[uArg], UFREG_SWIZ_Z, &uArgValue))
				{
					psCompileTimeSuccessor = (uArgValue == 0) ? psFalseSucc : psTrueSucc;
				}
			}
			break; //negation handled at end
		}
		case UFOP_GLSLLOOP:
		case UFOP_GLSLSTATICLOOP:
		case UFOP_GLSLCONTINUE:
		case UFOP_GLSLENDLOOP:
			//in all cases, first successor is to be taken if predicate true.
			GetInputPredicate(psState,
							  &uPredSrc,
							  &bNegate,
							  psFirstInst->uPredicate,
							  0);
			uArg = UINT_MAX; //don't look for UFREG_SOURCE_NEGATE
			if (uPredSrc == USC_PREDREG_NONE)
			{
				ASSERT (bNegate == IMG_FALSE);
				psCompileTimeSuccessor = psTrueSucc;
			}
			else
			{
				IMG_UINT uPred = psFirstInst->uPredicate;
				IMG_BOOL bTestResult;
				if (SearchBackwardsSetp(psState,
										psFirstInst,
										(uPred & UF_PRED_IDX_MASK) >> UF_PRED_IDX_SHIFT,
										((uPred & UF_PRED_COMP_MASK) - UF_PRED_X) >> UF_PRED_COMP_SHIFT,
										&bStatic,
										&bTestResult))
				{
					psCompileTimeSuccessor = bTestResult ? psTrueSucc : psFalseSucc;
					//applied below, after bNegate.
				}

				/*
					Override the static flag where the user has told us the predicate
					used to control a loop are static.
				*/
				if (psFirstInst->eOpCode == UFOP_GLSLSTATICLOOP || bEndOfStaticLoop)
				{
					bStatic = IMG_TRUE;
				}
			}
			break;
		default:
			imgabort();
	}
	if (uArg!= UINT_MAX)
	{
		ASSERT (bNegate == IMG_FALSE);
		if (psFirstInst->asSrc[uArg].byMod & UFREG_SOURCE_NEGATE)
		{
			bNegate = IMG_TRUE;
		}
	}	
	if (bNegate)
	{
		PCODEBLOCK psTemp = psTrueSucc;
		psTrueSucc = psFalseSucc;
		psFalseSucc = psTemp;		
	}	
	if (psCompileTimeSuccessor)
	{
		*puNumSucc = 1;
		*ppsTrueSucc = psCompileTimeSuccessor;		
		//SetBlockUnconditional(psState, psBlock, psCompileTimeSuccessor);
	}
	else
	{
		if (psTrueSucc == psFalseSucc)
		{
			*puNumSucc = 1;
			*ppsTrueSucc = psCompileTimeSuccessor;			
		}
		else
		{
			*puNumSucc = 2;
			*ppsTrueSucc = psTrueSucc;		
			*ppsFalseSucc = psFalseSucc;
			*puPredSrc = uPredSrc;
			*pbPredSrcNegate = bNegate;
			*pbStatic = bStatic;
		}
		//SetBlockConditional(psState, psBlock, uPredSrc, psTrueSucc, psFalseSucc, bStatic);
	}
}

static IMG_VOID AddD3DLoopProlog(PINTERMEDIATE_STATE	psState,
								 PUNIFLEX_INST			psFirstInst,
								 PCODEBLOCK				psBlock,
								 IMG_UINT32				uLoopCounter)
/*****************************************************************************
 FUNCTION	: AddD3DLoopProlog

 PURPOSE	: Add the prolog needed before a D3D loop.

 PARAMETERS	: psState			- Compiler state.
			  psFirstInst		- Loop instruction.
			  psBlock			- Block to insert the prolog instructions
								into.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psUseInst;
	IMG_UINT32	uArg = psFirstInst->asSrc[0].uNum;

	/* Initialize a temporary with the number of loop iterations required. */
	psUseInst = AllocateInst(psState, IMG_NULL);
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		SetOpcode(psState, psUseInst, IVPCKFLTU8);
		psUseInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		psUseInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_U8;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		SetOpcode(psState, psUseInst, IUNPCKF32U8);
		SetPCKComponent(psState, psUseInst, 0 /* uArgIdx */, 0 /* uComponent */);
	}
	psUseInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psUseInst->asDest[0].uNumber = uLoopCounter;
	psUseInst->asArg[0].uType = USEASM_REGTYPE_SECATTR;
	psUseInst->asArg[0].uNumber = psState->psSAOffsets->uIntegerConsts + uArg;
	AppendInst(psState, psBlock, psUseInst);

	if (psFirstInst->eOpCode == UFOP_LOOP)
	{
		ARG		sLoopAddrStart;

		InitInstArg(&sLoopAddrStart);
		sLoopAddrStart.uType = USEASM_REGTYPE_SECATTR;
		sLoopAddrStart.uNumber = psState->psSAOffsets->uIntegerConsts + uArg;
		sLoopAddrStart.eFmt = UF_REGFORMAT_F32;

		/*
		  Get new registers for the index for this loop.
		*/
		psState->uD3DLoopIndex = GetNextRegister(psState);

		/*
		  Set the initial value of the loop index.
		*/
		ConvertAddressValue(psState,
							psBlock,
							psState->uD3DLoopIndex,
							&sLoopAddrStart,
							1 /* uSourceComponent */,
							IMG_TRUE /* bU8Src */,
							USC_PREDREG_NONE,
							IMG_FALSE);
	}

	/* Check if the loop body should be skipped entirely. */
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		TestVecFloat(psState, psBlock, uLoopCounter, UFREG_COMPOP_LE);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		psUseInst = AllocateInst(psState, IMG_NULL);

		SetOpcodeAndDestCount(psState, psUseInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
		psUseInst->u.psTest->eAluOpcode = IFSUB;
		psUseInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psUseInst->asArg[0].uNumber = uLoopCounter;
		psUseInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
		psUseInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_ZERO;
	
		MakePredicateArgument(&psUseInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
		CompOpToTest(psState, UFREG_COMPOP_LE, &psUseInst->u.psTest->sTest);
		AppendInst(psState, psBlock, psUseInst);
	}
}

static IMG_VOID AddD3DLoopEpilog(PINTERMEDIATE_STATE	psState,
								 PUNIFLEX_INST			psFirstInst,
								 PCODEBLOCK				psBlock,
								 IMG_UINT32				uLoopCounter)
/*****************************************************************************
 FUNCTION	: AddD3DLoopEpilog

 PURPOSE	: Add the epilog needed at the end of a D3D loop body.

 PARAMETERS	: psState			- Compiler state.
			  psFirstInst		- Loop instruction.
			  psLoopList		- Points to the list of blocks in 
								the loop body.
			  uLoopCounter		- Register which contains the loop counter.
			  uSavedMemIndex	- Register numbers for the loop index in
			  uSavedRegIndex	any loop this loop is nested inside.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST		psUseInst;
	IMG_UINT32	uArg = psFirstInst->asSrc[0].uNum;

	ASSERT(!(psState->uCompilerFlags & UF_GLSL));

	/* Insert code at the end of the loop body to determine if the loop should continue. */
	
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		PINST	psVSUBInst;

		/*
			Generate
				LOOP_COUNTER = LOOP_COUNTER - 1
		*/
		psVSUBInst = AllocateInst(psState, IMG_NULL);

		SetOpcode(psState, psVSUBInst, IVADD);

		psVSUBInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psVSUBInst->asDest[0].uNumber = uLoopCounter;

		/* Loop counter source. */
		SetupTempVecSource(psState, psVSUBInst, 0 /* uSrcIdx */, uLoopCounter, UF_REGFORMAT_F32);
		psVSUBInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);

		/* -1 source. */
		SetupImmediateSource(psVSUBInst, 1 /* uSrcIdx */, 1.0f /* uImmValue */, UF_REGFORMAT_F32);
		psVSUBInst->u.psVec->auSwizzle[1] = USEASM_SWIZZLE(X, Y, Z, W);
		psVSUBInst->u.psVec->asSrcMod[1].bNegate = IMG_TRUE;

		AppendInst(psState, psBlock, psVSUBInst);

		/* Check if the loop counter is >= 0. */
		TestVecFloat(psState, psBlock, uLoopCounter, UFREG_COMPOP_GT);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		psUseInst = AllocateInst(psState, IMG_NULL);

		SetOpcodeAndDestCount(psState, psUseInst, ITESTPRED, TEST_MAXIMUM_DEST_COUNT);
		psUseInst->u.psTest->eAluOpcode = IFSUB;
		psUseInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psUseInst->asArg[0].uNumber = uLoopCounter;
		psUseInst->asArg[1].uType = USEASM_REGTYPE_FPCONSTANT;
		psUseInst->asArg[1].uNumber = EURASIA_USE_SPECIAL_CONSTANT_FLOAT1;

		MakePredicateArgument(&psUseInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
		psUseInst->u.psTest->sTest.eType = TEST_TYPE_GT_ZERO;
		psUseInst->asDest[TEST_UNIFIEDSTORE_DESTIDX].uType = USEASM_REGTYPE_TEMP;
		psUseInst->asDest[TEST_UNIFIEDSTORE_DESTIDX].uNumber = uLoopCounter;
		AppendInst(psState, psBlock, psUseInst);
	}

	if (psFirstInst->eOpCode == UFOP_LOOP)
	{
		/* Initialize a temporary with the increment of the loop counter. */
		psUseInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psUseInst, IUNPCKS16S8);
		psUseInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psUseInst->asDest[0].uNumber = USC_TEMPREG_TEMPDEST;
		psUseInst->asArg[0].uType = USEASM_REGTYPE_SECATTR;
		psUseInst->asArg[0].uNumber = psState->psSAOffsets->uIntegerConsts + uArg;
		SetPCKComponent(psState, psUseInst, 0 /* uArgIdx */, 2 /* uComponent */);
		psUseInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psUseInst->asArg[1].uNumber = 0;
		AppendInst(psState, psBlock, psUseInst);

		/* Increment the loop counter used for relative indexing. */
		psUseInst = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psUseInst, IIMA16);
		psUseInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
		psUseInst->asDest[0].uNumber = psState->uD3DLoopIndex;
		psUseInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
		psUseInst->asArg[0].uNumber = USC_TEMPREG_TEMPDEST;
		psUseInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psUseInst->asArg[1].uNumber = 1;
		psUseInst->asArg[2].uType = USEASM_REGTYPE_TEMP;
		psUseInst->asArg[2].uNumber = psState->uD3DLoopIndex;
		AppendInst(psState, psBlock, psUseInst);
	}
}

static PCODEBLOCK ConvertLoop(PINTERMEDIATE_STATE psState,
								PCALLDATA psCallData,
								PFUNC psFunc,
								PUNIFLEX_INST *ppsInst,
								PCODEBLOCK psBlock,
								IMG_BOOL bStatic)
{
	UF_OPCODE eLoopEnd;
	IMG_BOOL bStaticLoop = IMG_TRUE;
	IMG_UINT32 uSavedD3DIndex = psState->uD3DLoopIndex; //only need to save for UFOP_LOOP,

	PCODEBLOCK psLoopHead, psLoopTail, psLoopExit, psLoopBodyEnd;
	if (psBlock)
	{
		psLoopHead = AllocateBlock(psState, &(psFunc->sCfg));
		psLoopTail = AllocateBlock(psState, &(psFunc->sCfg));
		psLoopExit = AllocateBlock(psState, &(psFunc->sCfg));

		if ((*ppsInst)->eOpCode == UFOP_LOOP || (*ppsInst)->eOpCode == UFOP_REP)
		{
			IMG_UINT32 uD3DLoopCounter = GetNextRegister(psState);
			eLoopEnd = ((*ppsInst)->eOpCode == UFOP_LOOP) ? UFOP_ENDLOOP : UFOP_ENDREP;
			
			/* Add code needed before a D3D loop. */
			AddD3DLoopProlog(psState, (*ppsInst), psBlock, uD3DLoopCounter);
			// if predicate is TRUE, we skip the loop by jumping to exit
			if (psState->uCompilerFlags & UF_ATLEASTONELOOP)
			{
				SetBlockUnconditional(psState, psBlock, psLoopHead);
			}
			else
			{
				SetBlockConditional(psState, psBlock, USC_PREDREG_TEMP, psLoopExit, psLoopHead, IMG_TRUE);
			}

			/* And add code to tail to test repeat */
			AddD3DLoopEpilog(psState, 
							 (*ppsInst), 
							 psLoopTail,
							 uD3DLoopCounter);
			SetBlockConditional(psState, psLoopTail, USC_PREDREG_TEMP, psLoopHead, psLoopExit, IMG_TRUE);
		}
		else
		{
			eLoopEnd = UFOP_GLSLENDLOOP;
			if ((*ppsInst)->eOpCode == UFOP_GLSLLOOP)
			{
				bStaticLoop = IMG_FALSE;
			}
			
			//if predicate is true, we enter the loop (branch to head)
			{
				IMG_UINT32 uNumSucc;
				PCODEBLOCK psTrueSucc;
				PCODEBLOCK psFalseSucc;
				IMG_UINT32 uPredSrc;
				IMG_BOOL bPredSrcNegate;
				IMG_BOOL bStaticL;				
				DoFlowControlTest(psState, *ppsInst, psBlock, psLoopHead, psLoopExit, IMG_FALSE /* bEndOfStaticLoop */, &uNumSucc, &psTrueSucc, &psFalseSucc, &uPredSrc, &bPredSrcNegate, &bStaticL);
				if(uNumSucc == 2)
				{
					SetBlockConditional(psState, psBlock, uPredSrc, psTrueSucc, psFalseSucc, bStaticL);					
				}
				else
				{
					SetBlockUnconditional(psState, psBlock, psTrueSucc);
				}
			}
		}
	}
	else
	{
		psLoopHead = psLoopExit = psLoopTail = NULL;
		eLoopEnd = ((*ppsInst)->eOpCode == UFOP_LOOP) ? UFOP_ENDLOOP
			: ((*ppsInst)->eOpCode == UFOP_REP) ? UFOP_ENDREP
			: UFOP_GLSLENDLOOP;
	}

	//done with begin-loop instruction
	*ppsInst = (*ppsInst)->psILink;

	bStatic = (bStatic && bStaticLoop) ? IMG_TRUE : IMG_FALSE;

	//convert the body
	psLoopBodyEnd =
		DoConvert(psState, psCallData, psFunc, ppsInst, psLoopHead,
				  psLoopExit, psLoopTail/*continue target - used iff GLSL*/,
				  IMG_TRUE, bStatic);

	//if there's any way to fall through the body without break/continue...
	if (psLoopBodyEnd) SetBlockUnconditional(psState, psLoopBodyEnd, psLoopTail);

	if (*ppsInst == NULL)
	{
		UscFail(psState, UF_ERR_GENERIC, "No loop terminator.\n");
	}

	if (eLoopEnd == UFOP_GLSLENDLOOP)
	{
		if (psLoopTail && psLoopTail->uNumPreds == 0) 
		{
			FreeBlock(psState, psLoopTail);
			psLoopTail = NULL; //tail unreachable, skip instructions in it
		}

		if ((*ppsInst)->eOpCode == UFOP_GLSLLOOPTAIL)
		{
			//Convert instructions in the loop tail.
			*ppsInst = (*ppsInst)->psILink;
			psLoopTail = DoConvert(psState, psCallData, psFunc, ppsInst, psLoopTail,
								   psLoopExit, NULL,
								   IMG_TRUE, bStatic);
		}

		ASSERT ((*ppsInst)->eOpCode == UFOP_GLSLENDLOOP);

		if (psLoopTail) //can fall through loop tail w/out break-ing
		{
			//if predicate true, we repeat (branch back to head)
			{
				IMG_UINT32 uNumSucc;
				PCODEBLOCK psTrueSucc;
				PCODEBLOCK psFalseSucc;
				IMG_UINT32 uPredSrc;
				IMG_BOOL bPredSrcNegate;
				IMG_BOOL bStaticL;				
				DoFlowControlTest(psState, *ppsInst, psLoopTail, psLoopHead, psLoopExit, bStaticLoop, &uNumSucc, &psTrueSucc, &psFalseSucc, &uPredSrc, &bPredSrcNegate, &bStaticL);
				if(uNumSucc == 2)
				{					
					SetBlockConditional(psState, psLoopTail, uPredSrc, psTrueSucc, psFalseSucc, bStaticL);
				}
				else
				{
					SetBlockUnconditional(psState, psLoopTail, psTrueSucc);
				}
			}
			//DoFlowControlTest(psState, *ppsInst, psLoopTail, psLoopHead, psLoopExit, bStaticLoop);
		}
	}
	else
	{
		ASSERT ((*ppsInst)->eOpCode == eLoopEnd);
		//conditional branch to repeat loop set up earlier.

		//only predecessor to tail should be the body (no continues).
		ASSERT (psLoopTail == NULL || psLoopTail->uNumPreds <= 1);
		
		/*
		  Restore the index registers for any enclosing loop now we've finished generating
		  code for this loop.
		*/
		psState->uD3DLoopIndex = uSavedD3DIndex;
	}

	return psLoopExit;
}

IMG_INTERNAL
PUNIFLEX_INST LabelToInst(PUNIFLEX_INST psProg, IMG_UINT32 uLabel)
/*****************************************************************************
 FUNCTION	: LabelToInst

 PURPOSE	: Find a label instruction with a specified label.

 PARAMETERS	: psProg		- Pointer to the first instruction in the program
			  dwLabel		- Label to find.

 RETURNS	: The instruction with that label or NULL if not found.
*****************************************************************************/
{
	for (; psProg != NULL; psProg = psProg->psILink)
	{
		if (psProg->eOpCode == UFOP_LABEL && psProg->asSrc[0].uNum == uLabel)
		{
			return psProg;
		}
	}
	return NULL;
}

static PUNIFLEX_INST BLabelToInst(PUNIFLEX_INST psProg, IMG_UINT32 uLabel)
/*****************************************************************************
 FUNCTION	: LabelToInst

 PURPOSE	: Find a label instruction with a specified label.

 PARAMETERS	: psProg		- Pointer to the first instruction in the program
			  dwLabel		- Label to find.

 RETURNS	: The instruction with that label or NULL if not found.
*****************************************************************************/
{
	for (; psProg != NULL; psProg = psProg->psILink)
	{
		if (psProg->eOpCode == UFOP_BLOCK && psProg->asSrc[0].uNum == uLabel)
		{
			return psProg;
		}
	}
	return NULL;
}

static PFUNC BuildCFG(PINTERMEDIATE_STATE psState, IMG_PCHAR pchEntryPointDesc, PCALLDATA psCallData,
					  IMG_UINT32 uInputLabel, IMG_BOOL bStatic)
/******************************************************************************
 FUNCTION	: BuildCFG

 PURPOSE	: Builds the CFG for a specified function in the input program

 PARAMETERS	: psState			- Compiler intermediate state
			  pchEntryPointDesc	- name by which function is externally visible
								  (or else NULL)
			  psCallData		- contains input program and conversion status
			  uInputLabel		- label in input program at start of function
			  bStatic			- whether all control flow paths by which the
								  function may be called are known to be static
								(i.e. the same for all pixels - so TRUE for the
								  main function, maybe not for callees)

 RETURNS	: a PFUNC structure for the function, containing its CFG.
******************************************************************************/
{
	PFUNC psFunc = AllocFunction(psState, pchEntryPointDesc);
	PCODEBLOCK psTemp;
	IMG_UINT32 uSavedNestingLevel = psCallData->uNestingLevel;
	PUNIFLEX_INST psLabelInst = LabelToInst(psCallData->sProg.psHead, uInputLabel);
	PUNIFLEX_INST psFirstInst;

	if (psLabelInst == NULL)
	{
		USC_ERROR(UF_ERR_GENERIC, "Destination label not found");
	}
	
	psFirstInst = psLabelInst->psILink;
	
	if (uInputLabel == USC_MAIN_LABEL_NUM) psState->psMainProg=psFunc;
	
	//make a new psEntry, so can use existing psExit as a target for (early) RETs.
	psFunc->sCfg.psEntry = AllocateBlock(psState, &(psFunc->sCfg));
	
	psFunc->uNestingLevel = UINT_MAX; //makes sure we can't recurse via ConvertCall
	psCallData->uNestingLevel = 0;
	psTemp = DoConvert(psState, psCallData,
					   psFunc, &psFirstInst, psFunc->sCfg.psEntry,
					   NULL, NULL,
					   IMG_FALSE, bStatic);
	psFunc->uNestingLevel = psCallData->uNestingLevel;
	psCallData->uNestingLevel = uSavedNestingLevel;

	/* Link non-linked blocks */
	if (psFunc->asUnlinkedJumps)
	{
		IMG_UINT32 i, j;
		for (i = 0; i < psFunc->uUnlinkedJumpCount; i++)
		{
			for (j = 0; j < psFunc->sCfg.uNumBlocks; j++)
			{
				if (psFunc->asUnlinkedJumps[i].uLabel == psFunc->sCfg.apsAllBlocks[j]->uLabel)
				{
					SetBlockUnconditional(psState, psFunc->asUnlinkedJumps[i].psBlock, psFunc->sCfg.apsAllBlocks[j]);
					break; /* Continue to next i */
				}
			}
		}
		ASSERT(i == psFunc->uUnlinkedJumpCount); // Not all unlinked jumps were processed!
		UscFree(psState, psFunc->asUnlinkedJumps);
		psFunc->asUnlinkedJumps = NULL;
	}
	
	ASSERT (!psTemp); //body must end with RET.
	/*
	  Don't call MergeBBs yet - wait until predecessor arrays are set
	  in FinaliseIntermediateCode, as it's rather expensive to do MergeBBs
	  before that!
	*/
	return psFunc;
}

static
PCODEBLOCK ConvertJump(PINTERMEDIATE_STATE psState,
						PCALLDATA psCallData,
						PFUNC psFunc,
						PCODEBLOCK psBlock,
						PUNIFLEX_INST psJumpInst)
/******************************************************************************
 FUNCTION		: ConvertJump

 DESCRIPTION	: Converts a JUMP instruction in the input program

 PARAMETERS		: psState			- Compiler intermediate state
								psCallData	- conversion state w/ input prog call/func info
								psBlock			- Block to which converted IJUMP inst should be
														  appended, or NULL to skip it
								psFunc			- The function where this instruction is placed
								psJumpInst	- The input call instruction

 RETURNS		: Block into/onto which subsequent instructions should be added

 NOTE			: Loosely based on ConvertCall
******************************************************************************/
{
	IMG_UINT32 uInputLabel = psJumpInst->asSrc[0].uNum;

#ifdef SRC_DEBUG
	IMG_UINT32 uJumpInstSrcLine = psState->uCurSrcLine;
#endif /* SRC_DEBUG */

	/* If no corresponding label is found, the front end is not doing a good job */
	ASSERT(BLabelToInst(psCallData->sProg.psHead, uInputLabel)); // Undefined label on JUMP instruction.

#ifdef SRC_DEBUG
	if(psJumpInst)
	{
		uJumpInstSrcLine = psJumpInst->uSrcLine;
		/* set psState->uCurSrcLine to current jump instruction source line. */
		psState->uCurSrcLine = uJumpInstSrcLine;
	}
#endif /* SRC_DEBUG */

    /* Write branch information */
    if (psBlock)
	{
		IMG_UINT32 i = 0;
		IMG_BOOL bFound = IMG_FALSE;

		/* Check if the destination label has been found already */
		for (; i < psFunc->sCfg.uNumBlocks;  i++)
		{
			if (psFunc->sCfg.apsAllBlocks[i]->uLabel == uInputLabel)
			{
				bFound = IMG_TRUE;
				break;
			}
		}
		
		/* If not found, add it to the post-CFG link step */
		if (!bFound)
		{
			IMG_UINT32 uAllocSize = ++psFunc->uUnlinkedJumpCount * sizeof(UNLINKEDJUMP);

/*			ResizeArray (psState, psFunc->asUnlinkedJumps,*/
/*				uAllocSize - sizeof(UNLINKEDJUMP), uAllocSize,*/
/*				(IMG_PVOID*)&psFunc->asUnlinkedJumps); */

			IncreaseArraySizeInPlace (psState, 
				uAllocSize - sizeof(UNLINKEDJUMP), uAllocSize,
				(IMG_PVOID*)&psFunc->asUnlinkedJumps); 

			psFunc->asUnlinkedJumps[psFunc->uUnlinkedJumpCount-1].psBlock = psBlock;
			psFunc->asUnlinkedJumps[psFunc->uUnlinkedJumpCount-1].uLabel = uInputLabel;
		}
		else /* Block was already created, link it on CFG */
		{
			/* Read: this block unconditionally follows to psFunc->apsAllBlocks[i] */
			SetBlockUnconditional(psState, psBlock, psFunc->sCfg.apsAllBlocks[i]);
		}
	}

	return psBlock;
}

static
PCODEBLOCK ConvertCall(PINTERMEDIATE_STATE psState,
					   PCALLDATA psCallData,
					   PCODEBLOCK psBlock,
					   PUNIFLEX_INST psCallInst)
/******************************************************************************
 FUNCTION		: ConvertCall

 DESCRIPTION	: Converts a CALL instruction in the input program

 PARAMETERS		: psState		- Compiler intermediate state
				  psCallData	- conversion state w/ input prog call/func info
				  psBlock		- Block to which converted ICALL inst should be
								  appended, or NULL to skip it
				  psCallInst	- The input call instruction

 RETURNS		: Block into/onto which subsequent instructions should be added

 NOTE			: includes converting called function _even if call is skipped_
******************************************************************************/
{
	IMG_UINT32 uInputLabel, i;
	PFUNC psFunc = NULL;
	PCODEBLOCK psCallBlock, psAfter; //CALLs are the only instructions in their basic blocks.
	PCODEBLOCK psLoopSetupBlock;

	#ifdef SRC_DEBUG
	IMG_UINT32 uCallInstSrcLine = psState->uCurSrcLine;
	#endif /* SRC_DEBUG */

	#ifdef SRC_DEBUG
	if(psCallInst)
	{
		uCallInstSrcLine = psCallInst->uSrcLine;

		/* set psState->uCurSrcLine to current call instruction source line. */
		psState->uCurSrcLine = uCallInstSrcLine;
	}
	#endif /* SRC_DEBUG */

	if (psBlock)
	{ 
		PCODEBLOCK	psFirstCallBlock;
		PCODEBLOCK	psLastCallBlock;

		if ((psState->uCompilerFlags & UF_GLSL) == 0 && psState->uD3DLoopIndex != USC_UNDEF)
		{
			/*
				Allocate two blocks. One for the call and one for instructions to set up the loop
				index parameter for the function.
			*/
			psLoopSetupBlock = AllocateBlock(psState, psBlock->psOwner);
			psCallBlock = AllocateBlock(psState, psBlock->psOwner);

			SetBlockUnconditional(psState, psLoopSetupBlock, psCallBlock);

			psFirstCallBlock = psLoopSetupBlock;
			psLastCallBlock = psCallBlock;
		}
		else
		{
			psCallBlock = AllocateBlock(psState, psBlock->psOwner);
			psLoopSetupBlock = NULL;

			psFirstCallBlock = psLastCallBlock = psCallBlock;
		}

		psAfter = AllocateBlock(psState, psBlock->psOwner);
		SetBlockUnconditional(psState, psLastCallBlock, psAfter);
	
		//1. Make an appropriate block
		ASSERT (psCallInst->uPredicate == UF_PRED_NONE);
		if (psCallInst->eOpCode == UFOP_CALL)
		{
			SetBlockUnconditional(psState, psBlock, psFirstCallBlock);
		}
		else
		{
			//conditionally skip round call block			
			{
				IMG_UINT32 uNumSucc;
				PCODEBLOCK psTrueSucc;
				PCODEBLOCK psFalseSucc;
				IMG_UINT32 uPredSrc;
				IMG_BOOL bPredSrcNegate;
				IMG_BOOL bStatic;				
				DoFlowControlTest(psState, psCallInst, psBlock, psFirstCallBlock, psAfter, IMG_FALSE, &uNumSucc, &psTrueSucc, &psFalseSucc, &uPredSrc, &bPredSrcNegate, &bStatic);
				if(uNumSucc == 2)
				{
					SetBlockConditional(psState, psBlock, uPredSrc, psTrueSucc, psFalseSucc, bStatic);
				}
				else
				{
					SetBlockUnconditional(psState, psBlock, psTrueSucc);
				}
			}
		}

		
	}
	else
	{
		psLoopSetupBlock = NULL;
		psCallBlock = NULL; //won't be read again, but req'd for compiler warnings
		psAfter = NULL; //return value
	}

	//2. Get the label, map it to a function
	uInputLabel = psCallInst->asSrc[0].uNum;
	for (i = 0; !psFunc && i < psCallData->uNumLabels; i++)
	{
		if (psCallData->puInputLabels[i] == uInputLabel)
		{
			psFunc = psCallData->ppsFuncs[i];
		}
	}

	if (psFunc)
	{
		//Got it - parsed already.
		ASSERT (psFunc->uNestingLevel != (IMG_UINT32)-1); //if fails...recursive!
	}
	else
	{
		IMG_UINT32 uLinkSaveTemp = GetNextRegister(psState);
		IMG_UINT32 uSavedD3DLoopIndex;

		/*
		  Note we parse the body and create a FUNC structure even if we are
		  skipping the CALL (this is to make sure the texkill and depth-feedback
		  flags are set if the (skipped) function contained such instructions).
		*/

		//add input label to map before we recurse

		IncreaseArraySizeInPlace(psState, 
					psCallData->uNumLabels * sizeof(IMG_UINT32),
					(psCallData->uNumLabels + 1) * sizeof(IMG_UINT32),
					(IMG_PVOID*)&psCallData->puInputLabels);
		IncreaseArraySizeInPlace(psState, 
					psCallData->uNumLabels * sizeof(PFUNC),
					(psCallData->uNumLabels + 1) * sizeof(PFUNC),
					(IMG_PVOID)&psCallData->ppsFuncs);

		if ((psState->uCompilerFlags & UF_GLSL) == 0)
		{	
			/*
				Save the register numbers containing the loop index for any D3D loop this particular
				call is nested inside.
			*/
			uSavedD3DLoopIndex = psState->uD3DLoopIndex;

			/*
				Allocate new register numbers. Instructions will be added at any call site to
				copy the value of the loop index at that point at these registers.
			*/
			psState->uD3DLoopIndex = GetNextRegister(psState);
		}
		else
		{
			uSavedD3DLoopIndex = USC_UNDEF;
		}

		psCallData->uNumLabels++;
		psCallData->puInputLabels[i] = uInputLabel;
		psFunc = BuildCFG(psState, NULL, psCallData, uInputLabel, IMG_FALSE);
        psCallData->ppsFuncs[i] = psFunc;

		#ifdef SRC_DEBUG
		/* set psState->uCurSrcLine to current call instruction source line. */
		psState->uCurSrcLine = uCallInstSrcLine;
		#endif /* SRC_DEBUG */
				
		//3e. Record depth of CALLs in function body
		if (psFunc->uNestingLevel > 0)
		{
			/*
			  body contained calls.
			  so save and then restore link register.
			  (note that the following'll be DCE'd if not required---e.g. for the main program!)
			*/
			PINST psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, ISAVL);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = uLinkSaveTemp;
			psInst->asArg[0].uType = USEASM_REGTYPE_LINK;
			psInst->asArg[0].uNumber=0;

			InsertInstBefore(psState, psFunc->sCfg.psEntry,
							 psInst, psFunc->sCfg.psEntry->psBody);

			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, ISETL);
			psInst->asDest[0].uType = USEASM_REGTYPE_LINK;
			psInst->asDest[0].uNumber = 0;
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = uLinkSaveTemp;
			AppendInst(psState, psFunc->sCfg.psExit, psInst);
		}

		if ((psState->uCompilerFlags & UF_GLSL) == 0)
		{
			/*
				Save the registers the call used for the index value so future calls copy the
				index value at that call site into the same registers.
			*/
			psFunc->uD3DLoopIndex = psState->uD3DLoopIndex;

			/*
				Restore the saved values of the loop index for any loop we are nested inside.
			*/
			psState->uD3DLoopIndex = uSavedD3DLoopIndex; 
		}
	}
	
	if (psBlock)
	{
		PINST	psCall;

		if (psLoopSetupBlock != NULL)
		{
			/*
				Generate instructions to copy the value of the loop index at this point to the
				registers where the generated code for the function expects it.
			*/
			PINST	psMOVInst;

			psMOVInst = AllocateInst(psState, NULL);
			SetOpcode(psState, psMOVInst, IMOV);
			psMOVInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psMOVInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psMOVInst->asDest[0].uNumber = psFunc->uD3DLoopIndex;
			psMOVInst->asArg[0].uNumber = psState->uD3DLoopIndex;
			AppendInst(psState, psLoopSetupBlock, psMOVInst);
		}

		//4. Make call instruction!
		psCall = AllocateInst(psState, IMG_NULL);
		SetOpcodeAndDestCount(psState, psCall, ICALL, 0 /* uDestCount */);
		psCall->u.psCall->psTarget = psFunc;
		psCall->u.psCall->psCallSiteNext = psFunc->psCallSiteHead;
		psFunc->psCallSiteHead = psCall;
		AppendInst(psState, psCallBlock, psCall);
		psCall->u.psCall->psBlock = psCallBlock;

		//5. Tidy up
		psCallData->uNestingLevel = max (psCallData->uNestingLevel, psFunc->uNestingLevel + 1);
	}

	return psAfter;
}

IMG_INTERNAL
IMG_BOOL IsInputInstFlowControl(PUNIFLEX_INST psInst) //used by precovr.c
/*****************************************************************************
 FUNCTION	: IsInputInstFlowControl
    
 PURPOSE	: Check if an input instruction is a branch.

 PARAMETERS	: psInst		- The instruction to check.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	switch (psInst->eOpCode)
	{
		case UFOP_CALL:
		case UFOP_CALLNZ:
		case UFOP_CALLP:
		case UFOP_LOOP:
		case UFOP_REP:
		case UFOP_GLSLLOOP:
		case UFOP_GLSLSTATICLOOP:
		case UFOP_GLSLLOOPTAIL:
		case UFOP_BREAK:
		case UFOP_BREAKC:
		case UFOP_BREAKP:
		case UFOP_GLSLCONTINUE:
		case UFOP_ENDLOOP:
		case UFOP_ENDREP:
		case UFOP_GLSLENDLOOP:
		case UFOP_IF:
		case UFOP_IFC:
		case UFOP_IFP:
		case UFOP_ELSE:
		case UFOP_ENDIF:
		case UFOP_BREAKNZBIT:
		case UFOP_CALLNZBIT:
		case UFOP_RETNZBIT:
		case UFOP_CONTINUENZBIT:
		case UFOP_IFNZBIT:
		case UFOP_SWITCH:
		case UFOP_CASE:
		case UFOP_DEFAULT:
		case UFOP_ENDSWITCH:
		case UFOP_JUMP:
		{
			return IMG_TRUE;
		}
		default:
		{
			return IMG_FALSE;
		}
	}
}

static
PCODEBLOCK ConvertConditional(PINTERMEDIATE_STATE psState,
							  PCALLDATA psCallData,
							  PFUNC psFunc,
							  PUNIFLEX_INST *ppsInst,
							  PCODEBLOCK psBlock,
							  PCODEBLOCK psBreakTarget,
							  PCODEBLOCK psContinueTarget,
							  IMG_BOOL bStatic)
/******************************************************************************
 FUNCTION		: ConvertConditional

 DESCRIPTION	: Converts an entire UNIFLEX conditional construct (IF...ENDIF)

 PARAMETERS		: psState		- Compiler intermediate state
				  psCallData	- Conversion status inc. input prog&calls/funcs
				  psFunc		- containing function in the intermediate code
				  ppsInst		- pointer to input instruction (IFC / IFP / etc.)
				  psBlock		- block before conditional (to which to append)
				  psBreakTarget	- block to which any input 'break' would jump
				  psContinueTarget
								- block whither any input 'continue' would jump
				  bStatic		- Whether all paths _to_ this conditional are
								  known to be the same for all pixel instances

 RETURNS		:  Block with which execution continues after the conditional
 
 OUTPUT			: ppsInst pointed at input inst _after_ the corresponding ENDIF
******************************************************************************/
{
	/* Get the comparison and arguments, test whether the arguments are constant */
	//true/false successors for branch...
	PCODEBLOCK psIfBlock, psTempBlock;
	IMG_UINT32 uIfNumSucc = 0;
	PCODEBLOCK psIfFalseSucc = NULL;
	IMG_UINT32 uIfPredSrc = USC_PREDREG_NONE;
	IMG_BOOL bIfPredSrcNegate = IMG_FALSE;
	IMG_BOOL bIfStatic = IMG_FALSE;
	if (psBlock)
	{		
		psIfBlock = AllocateBlock(psState, &(psFunc->sCfg));
		psTempBlock = AllocateBlock(psState, &(psFunc->sCfg));
		{			
			PCODEBLOCK psIfTrueSucc;
			DoFlowControlTest(psState, *ppsInst, psBlock, psIfBlock, psTempBlock, IMG_FALSE /* bEndOfStaticLoop */, &uIfNumSucc, &psIfTrueSucc, &psIfFalseSucc, &uIfPredSrc, &bIfPredSrcNegate, &bIfStatic);
			if(uIfNumSucc == 2)
			{					
				//not a compile time test! nested code is static only if this test is *also* static...
				//if (!psBlock->bStatic) bStatic = IMG_FALSE;
				bStatic = bIfStatic;
				SetBlockConditional(psState, psBlock, uIfPredSrc, psIfTrueSucc, psIfFalseSucc, bIfStatic);
			}
			else
			{				
				//compile time test - destroy the unreachable block...
				PCODEBLOCK *ppsUnreach;
				SetBlockUnconditional(psState, psBlock, psIfTrueSucc);
				ASSERT (psBlock->eType == CBTYPE_UNCOND);
				ppsUnreach = (psBlock->asSuccs[0].psDest == psIfBlock)
					? &psTempBlock : &psIfBlock;
				FreeBlock(psState, *ppsUnreach);
				*ppsUnreach = IMG_NULL;
			}
		}
		//DoFlowControlTest(psState, *ppsInst, psBlock, psIfBlock, psTempBlock, IMG_FALSE);
	}
	else
	{
		psIfBlock = psTempBlock = NULL; //skip sub-blocks
	}
	//move onto first instruction of body
	*ppsInst = (*ppsInst)->psILink;
	
	//1. "if" clause...
	psIfBlock = DoConvert(psState, psCallData, psFunc,
						  ppsInst, psIfBlock /*skipped if null*/,
						  psBreakTarget, psContinueTarget,
						  IMG_TRUE, bStatic);
	
	if ((*ppsInst)->eOpCode == UFOP_ELSE)
	{
		//2. "else" clause
		PCODEBLOCK psElseBlock;
		//skip the ELSE instruction
		*ppsInst = (*ppsInst)->psILink;
		
		//append the "else" block onto psTempBlock (the "other" branch dest)
		psElseBlock = DoConvert(psState, psCallData, psFunc,
								ppsInst, psTempBlock,
								psBreakTarget, psContinueTarget,
								IMG_TRUE, bStatic);
		if (psIfBlock)
		{
			if (psElseBlock)
			{
				//can fall through both halves=>into new CFG merge block
				psTempBlock = AllocateBlock(psState, &(psFunc->sCfg));
				SetBlockUnconditional(psState, psIfBlock, psTempBlock);
				SetBlockUnconditional(psState, psElseBlock, psTempBlock);				
				return psTempBlock;
			}
			//"else" branches off somewhere; continue with "if"			
			return psIfBlock;
		}
		//"if" branches off somewhere; continue with "else" (or null)		
		return psElseBlock;
	}
	
	/*
	  3. no "else" clause - can use existing psTempBlock as merge
	  (for the branch round then block)
	*/
	if (psIfBlock)
	{
		if (psTempBlock == NULL)
		{
			return psIfBlock; //compile-time TRUE
		}
		SetBlockUnconditional(psState, psIfBlock, psTempBlock);
	}
	return psTempBlock;
}

static
PCODEBLOCK ConvertSwitch(PINTERMEDIATE_STATE psState,
						 PCALLDATA psCallData,
						 PFUNC psFunc,
						 PUNIFLEX_INST *ppsInst,
						 PCODEBLOCK psBlock,
						 PCODEBLOCK psContinueTarget,
						 IMG_BOOL bStatic)
/*****************************************************************************
 FUNCTION	: ConvertSwitch

 PURPOSE	: Converts a switch structure and any nested substructures
              to the intermediate format..

 PARAMETERS	: psState			- Compiler state.
			  psCallData		- Conversion context.
              ppsInst           - points to input SWITCH instruction
              psBlock			- resulting intermediate code is appended here
              pcContinueTarget	- block whither any input 'continue' would jump
              bStatic           - Whether all paths to the switch are known to
								  be the same for all pixels

 RETURNS    : Block with which execution continues after the switch
 
 OUTPUT		: ppsInst set to the input instruction after the ENDSWITCH
 
 NOTE		: if psBlock is NULL, the input structure is skipped and
			no intermediate code is generated
*****************************************************************************/
{
	/*
		Array stores info about each case, alternate elements being:
		PCODEBLOCK psTarget;
		IMG_UINT32 uValue;
	*/
	USC_PARRAY psCases;
	IMG_UINT32 uNumCases = 0; //so psCases has *twice* this #elems (internally)
	PARG psSwitchArg;
	
	PCODEBLOCK psEndSwitch, psDefaultTarget;
	PCODEBLOCK psFallThroughFromLastCase = NULL;
	PUNIFLEX_INST psUFInst;
	ASSERT (ppsInst != NULL);

	psUFInst = (*ppsInst);
	ASSERT(psUFInst != NULL);

	ASSERT (psUFInst->eOpCode == UFOP_SWITCH);
	
	if (psBlock != NULL)
	{
		const IMG_UINT32 uSize = max(sizeof(PCODEBLOCK), sizeof(IMG_UINT32));
		FLOAT_SOURCE_MODIFIER sMod;
		psCases = NewArray(psState, USC_MIN_ARRAY_CHUNK, 0, uSize);
		psEndSwitch = AllocateBlock(psState, &(psFunc->sCfg));

		psSwitchArg = UscAlloc(psState, sizeof(ARG));
		InitInstArg(psSwitchArg);

		GetSourceTypeless(psState, psBlock, &(psUFInst->asSrc[0]), // 0,
					 UFREG_SWIZ_Z, psSwitchArg, IMG_TRUE, &sMod);
		ApplyFloatSourceModifier(psState, 
								 psBlock, 
								 NULL /* psInsertBeforeInst */,
								 psSwitchArg, 
								 psSwitchArg, 
								 UF_REGFORMAT_F32, 
								 &sMod);
	}
	else
	{
		psCases = NULL;
		psEndSwitch = NULL;
		psSwitchArg = NULL;
	}
	psDefaultTarget = psEndSwitch; //if no 'default', go to the end!
	
	if (!UFRegIsConst(&psUFInst->asSrc[0]))
		bStatic = IMG_FALSE;
	psUFInst = psUFInst->psILink;
	
	//build list of cases as we go.
	while (psUFInst->eOpCode != UFOP_ENDSWITCH)
	{
		PCODEBLOCK psCaseBody = (psBlock) ? AllocateBlock(psState, &(psFunc->sCfg)) : NULL;
		if (psFallThroughFromLastCase)
		{
			SetBlockUnconditional(psState, psFallThroughFromLastCase, psCaseBody);
		}
		while (psUFInst->eOpCode == UFOP_CASE || psUFInst->eOpCode == UFOP_DEFAULT)
		{
			if (psUFInst->eOpCode == UFOP_DEFAULT)
			{
				ASSERT (psDefaultTarget == psEndSwitch);
				psDefaultTarget = psCaseBody;
			}
			else if (psBlock)
			{
				IMG_UINT32	uCaseValue;

				/*
					The case value can either be specified directly as an immediate or
					as the static value of a uniform constant.
				*/
				if (psUFInst->asSrc[0].eType == UFREG_TYPE_IMMEDIATE)
				{
					uCaseValue = psUFInst->asSrc[0].uNum;
				}
				else
				{
					IMG_BOOL	bIsStaticConst;

					ASSERT(psUFInst->asSrc[0].eType == UFREG_TYPE_CONST);
					bIsStaticConst = 
						CheckForStaticConst(psState, 
											psUFInst->asSrc[0].uNum, 
											EXTRACT_CHAN(psUFInst->asSrc[0].u.uSwiz, UFREG_SWIZ_X), 
											psUFInst->asSrc[0].uArrayTag, 
											&uCaseValue);
					ASSERT(bIsStaticConst);
				}

				//note we increment uNumCases
				ArraySet(psState, psCases, uNumCases*2, psCaseBody); //psTarget
				ArraySet(psState, psCases, uNumCases*2 + 1, (IMG_PVOID)(IMG_UINTPTR_T)uCaseValue);
				uNumCases++;
			}
			psUFInst = psUFInst->psILink;
		}
		//convert the body (perhaps empty!)
		psFallThroughFromLastCase =	DoConvert(psState, psCallData, psFunc,
											  &psUFInst, psCaseBody,
											  psEndSwitch, psContinueTarget,
											  IMG_TRUE, bStatic);
	}
	//reached end of switch.
	if (psFallThroughFromLastCase)
	{
		//last case doesn't (or might not) end with a break
		SetBlockUnconditional(psState, psFallThroughFromLastCase, psEndSwitch);
	}
	//now set up switch structures...
	/*
		NOTE: we assume the cases are non-exhaustive
	   (and thus always generate a SWITCH with a default target).
	*/
	if (psBlock)
	{
		ASSERT(psSwitchArg);

		if (uNumCases == 0)
		{
			/*
				In the degenerate case of no case bodies at all make the default case
				the unconditional successor.
			*/
			SetBlockUnconditional(psState, psBlock, psDefaultTarget);
			UscFree(psState, psSwitchArg);
		}
		else
		{
			IMG_PUINT32 auCaseValues = UscAlloc(psState, uNumCases * sizeof(IMG_UINT32));
			PCODEBLOCK *apsSuccs = UscAlloc(psState, (uNumCases + 1) * sizeof(PCODEBLOCK));
			IMG_UINT32 uCase;

			apsSuccs[uNumCases] = psDefaultTarget;
			for (uCase = 0; uCase < uNumCases; uCase++)
			{
				apsSuccs[uCase] = (PCODEBLOCK)ArrayGet(psState, psCases, uCase * 2);
				auCaseValues[uCase] = (IMG_UINT32)(IMG_UINTPTR_T)ArrayGet(psState, psCases, uCase * 2 + 1);
			}
			SetBlockSwitch(psState, psBlock, /* #succs inc default*/ uNumCases + 1,
						   apsSuccs, psSwitchArg, IMG_TRUE, auCaseValues);
			UscFree(psState, apsSuccs);
		}
		
		FreeArray(psState, &psCases);
	}

	/* Update input instruction pointer */
	(*ppsInst) = psUFInst;

	return psEndSwitch;
}

static 
IMG_VOID GetSourceConvertToU16(PINTERMEDIATE_STATE psState, 
							   PCODEBLOCK psCodeBlock, 
							   PUF_REGISTER psSource, 
							   IMG_UINT32 uChan, 
							   PARG psHwSource, 
							   IMG_UINT32 uPredicate)
/*****************************************************************************
 FUNCTION	: GetSourceConvertToU16

 PURPOSE	: Gets any format input source and convert it to U16 format in 
			  intermeidate form.

 PARAMETERS	: psState			- Compiler state.              
              psCodeBlock		- Resulting intermediate code is appended here.
			  psSource			- Uniflex input form source register.
              uChan				- Channel to get from input source.
			  psHwSource		- Will contain the source made to use in 
								  intermeidate form for this source operand.
			  uPredicate		- Predicate of source instrucion.

 RETURNS    : Nothing.
 
 OUTPUT		: psHwSource		- Will contain the source made to use in 
								  intermeidate form for this source operand.
*****************************************************************************/
{
	switch(GetRegisterFormat(psState, psSource))
	{		
		case UF_REGFORMAT_F32:
		{
			PINST psF32ToU16Inst;
			GetSourceF32(psState, 
							 psCodeBlock, 
							 psSource, 
							 uChan, 
							 psHwSource, 
							 IMG_FALSE,
							 NULL);
			psF32ToU16Inst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psF32ToU16Inst, IPCKU16F32);
			GetInputPredicateInst(psState, psF32ToU16Inst, uPredicate, uChan);
			psF32ToU16Inst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psF32ToU16Inst->asDest[0].uNumber = GetNextRegister(psState);
			psF32ToU16Inst->asArg[0] = (*psHwSource);
			psF32ToU16Inst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psF32ToU16Inst->asArg[1].uNumber = 0;
			InitInstArg(psHwSource);
			psHwSource->uType = psF32ToU16Inst->asDest[0].uType;
			psHwSource->uNumber = psF32ToU16Inst->asDest[0].uNumber;
			AppendInst(psState, psCodeBlock, psF32ToU16Inst);
			break;
		}
		case UF_REGFORMAT_F16:
		{
			PINST psF16ToU16Inst;
			IMG_UINT32	uComponent;
			GetSingleSourceF16(	
								psState, 
								psCodeBlock, 
								psSource, 
								psHwSource, 
								&uComponent,
								0, 
								IMG_FALSE,
								NULL,
								IMG_FALSE);
			psF16ToU16Inst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psF16ToU16Inst, IUNPCKU16F16);
			GetInputPredicateInst(psState, psF16ToU16Inst, uPredicate, uChan);
			psF16ToU16Inst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psF16ToU16Inst->asDest[0].uNumber = GetNextRegister(psState);
			psF16ToU16Inst->asArg[0] = (*psHwSource);
			SetPCKComponent(psState, psF16ToU16Inst, 0 /* uArgIdx */, uComponent);
			psF16ToU16Inst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psF16ToU16Inst->asArg[1].uNumber = 0;
			InitInstArg(psHwSource);
			psHwSource->uType = psF16ToU16Inst->asDest[0].uType;
			psHwSource->uNumber = psF16ToU16Inst->asDest[0].uNumber;
			AppendInst(psState, psCodeBlock, psF16ToU16Inst);
			break;
		}
		case UF_REGFORMAT_C10:
		case UF_REGFORMAT_U8:
		{					
			IMG_UINT32	uSrcChan;
			IMG_BOOL	bIgnoreSwiz;					
												
			if (CanOverrideSwiz(psSource))
			{
				uSrcChan = EXTRACT_CHAN(psSource->u.uSwiz, uChan);
				bIgnoreSwiz = IMG_TRUE;
			}
			else
			{
				uSrcChan = uChan;
				bIgnoreSwiz = IMG_FALSE;
			}							
			GetSourceC10(psState, psCodeBlock, psSource, psSource->byMod, psHwSource, 1U << uSrcChan, 
				bIgnoreSwiz, IMG_FALSE, psSource->eFormat);
			if(psHwSource->eFmt == UF_REGFORMAT_C10)
			{						
				{
					PINST psC10ToF32Inst;
					psC10ToF32Inst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psC10ToF32Inst, IUNPCKF32C10);
					GetInputPredicateInst(psState, psC10ToF32Inst, uPredicate, uChan);
					psC10ToF32Inst->asDest[0].uType = USEASM_REGTYPE_TEMP;
					psC10ToF32Inst->asDest[0].uNumber = GetNextRegister(psState);
					psC10ToF32Inst->asArg[0] = (*psHwSource);					
					InitInstArg(psHwSource);
					psHwSource->uType = psC10ToF32Inst->asDest[0].uType;
					psHwSource->uNumber = psC10ToF32Inst->asDest[0].uNumber;
					AppendInst(psState, psCodeBlock, psC10ToF32Inst);
				}						
				{
					PINST psF32ToU16Inst = AllocateInst(psState, IMG_NULL);
					SetOpcode(psState, psF32ToU16Inst, IPCKU16F32);
					GetInputPredicateInst(psState, psF32ToU16Inst, uPredicate, uChan);
					psF32ToU16Inst->asDest[0].uType = USEASM_REGTYPE_TEMP;
					psF32ToU16Inst->asDest[0].uNumber = GetNextRegister(psState);
					psF32ToU16Inst->asArg[0] = (*psHwSource);
					psF32ToU16Inst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
					psF32ToU16Inst->asArg[1].uNumber = 0;
					InitInstArg(psHwSource);
					psHwSource->uType = psF32ToU16Inst->asDest[0].uType;
					psHwSource->uNumber = psF32ToU16Inst->asDest[0].uNumber;
				}				
			}
			else
			{
				PINST psU8ToU16Inst;
				psU8ToU16Inst = AllocateInst(psState, IMG_NULL);
				SetOpcode(psState, psU8ToU16Inst, IUNPCKU16U8);
				GetInputPredicateInst(psState, psU8ToU16Inst, uPredicate, uChan);
				psU8ToU16Inst->asDest[0].uType = USEASM_REGTYPE_TEMP;
				psU8ToU16Inst->asDest[0].uNumber = GetNextRegister(psState);
				psU8ToU16Inst->asArg[0] = (*psHwSource);				
				psU8ToU16Inst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
				psU8ToU16Inst->asArg[1].uNumber = 0;
				InitInstArg(psHwSource);
				psHwSource->uType = psU8ToU16Inst->asDest[0].uType;
				psHwSource->uNumber = psU8ToU16Inst->asDest[0].uNumber;
				AppendInst(psState, psCodeBlock, psU8ToU16Inst);				
			}
			break;
		}
		case UF_REGFORMAT_I32:
		case UF_REGFORMAT_U32:
		{
			if((psSource->eType == UFREG_TYPE_IMMEDIATE) && (psSource->uNum < 128))
			{
				psHwSource->uType = USEASM_REGTYPE_IMMEDIATE;
				psHwSource->uNumber = psSource->uNum;
			}
			else
			{
				GetSourceF32(psState, 
							psCodeBlock, 
							psSource, 
							uChan, 
							psHwSource, 
							IMG_FALSE,
							NULL);
			}
			break;
		}
		default: imgabort();
	}
}

IMG_INTERNAL 
IMG_VOID ConvertVertexInputInstructionCore(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSrc, IMG_UINT32 uPredicate, IMG_UINT32 *puIndexReg, IMG_UINT32 *puStaticIdx)
/*****************************************************************************
 FUNCTION	: ConvertVertexInputInstructionCore

 PURPOSE	: Converts vertex input instruction to intermediate form.

 PARAMETERS	: psState			- Compiler state.              
              psCodeBlock		- Resulting intermediate code is appended here.
              psSrc				- points to input Vertex Input instruction.
			  uPredicate		- Predicate of source instrucion.
              puIndexReg        - Will contain the number of Temp Register to 
								  be used as index of Vertices Base register 
								  type.

 RETURNS    : Nothing.
 
 OUTPUT		: puIndexReg        - Will contain the number of Temp Register to 
								  be used as index of Vertices Base register 
								  type.
*****************************************************************************/
{	
	ARG			sInVertNum;	/* Input vertex number. */
	ARG					sVertBase;
	IMG_UINT32			uVertBaseTempNum;
	PINST				psMovInst;
	PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;

	ASSERT(psSrc->sDest.byMod == 0);
	ASSERT(psSrc->sDest.u.byMask);

	/*
		Add a structure representing the temporary registers containing
		the offset of the data for each input vertex in the primary
		attributes.
	*/
	if (psVS->uVerticesBaseInternArrayIdx == USC_UNDEF)
	{
		psVS->uVerticesBaseInternArrayIdx = 
			AddNewRegisterArray(psState, 
							    ARRAY_TYPE_VERTICES_BASE, 
								USC_UNDEF /* uArrayNum */,
								LONG_SIZE /* uChannelsPerDword */,
								psState->psSAOffsets->uInputVerticesCount);
	}

	/*
		Convert to the intermediate format the input source representing the vertex number to load an input for.
	*/
	InitInstArg(&sInVertNum);
	GetSourceConvertToU16(psState, psCodeBlock, &(psSrc->asSrc[0]), 0, &sInVertNum, uPredicate);

	/*
		Index into the array of registers containing the offset of the data for each
		vertex in the primary attribute using the converted source.
	*/
	InitInstArg(&sVertBase);
	sVertBase.uType = USC_REGTYPE_REGARRAY;
	sVertBase.uNumber = psVS->uVerticesBaseInternArrayIdx;
	if (sInVertNum.uType == USEASM_REGTYPE_IMMEDIATE)
	{
		/*
			VERTICES_BASE[N]
		*/
		sVertBase.uArrayOffset = sInVertNum.uNumber;
	}
	else
	{
		/*
			VERTICES_BASE[rM + 0]
		*/
		ASSERT(sInVertNum.uType == USEASM_REGTYPE_TEMP);
		sVertBase.uArrayOffset = 0;
		sVertBase.uIndexType = USEASM_REGTYPE_TEMP;
		sVertBase.uIndexNumber = sInVertNum.uNumber;
		sVertBase.uIndexArrayOffset = 0;
		sVertBase.uIndexStrideInBytes = LONG_SIZE;
	}

	/*
		Move the offset of the data for the selected vertex into a temporary
		register so we can use it for indexing.
	*/
	uVertBaseTempNum = GetNextRegister(psState);
	psMovInst = AllocateInst(psState, NULL);
	SetOpcode(psState, psMovInst, IMOV);
	psMovInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
	psMovInst->asDest[0].uNumber = uVertBaseTempNum;
	psMovInst->asArg[0] = sVertBase;
	AppendInst(psState, psCodeBlock, psMovInst);

	{
		ARG	sInElmNum;	/* Input element number. */
		InitInstArg(&sInElmNum);
		GetSourceConvertToU16(psState, 
					 psCodeBlock, 
					 &psSrc->asSrc[1], 
					 0, 
					 &sInElmNum, 
					 uPredicate);
		if((sInElmNum.uType == USEASM_REGTYPE_IMMEDIATE) && (sInElmNum.uNumber<128))
		{
			*puIndexReg = uVertBaseTempNum;
			*puStaticIdx = sInElmNum.uNumber;
		}
		else
		{
			PINST psInVertElmIndxLoadInstr;
			psInVertElmIndxLoadInstr = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInVertElmIndxLoadInstr, IIMA16);
			GetInputPredicateInst(psState, psInVertElmIndxLoadInstr, uPredicate, 0);
			psInVertElmIndxLoadInstr->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInVertElmIndxLoadInstr->asDest[0].uNumber = uVertBaseTempNum;
			psInVertElmIndxLoadInstr->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInVertElmIndxLoadInstr->asArg[0].uNumber = uVertBaseTempNum;
			psInVertElmIndxLoadInstr->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInVertElmIndxLoadInstr->asArg[1].uNumber = 1;
			psInVertElmIndxLoadInstr->asArg[2].uType = sInElmNum.uType;
			psInVertElmIndxLoadInstr->asArg[2].uNumber = sInElmNum.uNumber;
			AppendInst(psState, psCodeBlock, psInVertElmIndxLoadInstr);
			*puIndexReg = psInVertElmIndxLoadInstr->asDest[0].uNumber;
			*puStaticIdx = 0;
		}
	}
	return;
}


static 
PCODEBLOCK ConvertEmitInstruction(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock)
/*****************************************************************************
 FUNCTION	: ConvertEmitInstruction

 PURPOSE	: Converts input emit instruction to intermediate form.

 PARAMETERS	: psState			- Compiler state.              
              psCodeBlock		- Resulting intermediate code is appended here.

 RETURNS    : NONE.
 
 OUTPUT		: NONE.
*****************************************************************************/
{		
	PCODEBLOCK psCondTrueBlock;
	PCODEBLOCK psCondExitBlock;

	if (psState->psTargetDesc->eCoreType != SGX_CORE_ID_545)
	{
		UscFail(psState, UF_ERR_GENERIC, "Geometry shaders aren't supported on this core.\n");
	}

	{
		/* 
			EMIT instruction. 
		*/
		PINST psEmitInstruction;
		psEmitInstruction = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psEmitInstruction, IEMIT);
		{
			/*
				Write to the output registers.
			*/
			psEmitInstruction->asDest[0].uType = USC_REGTYPE_REGARRAY;
			psEmitInstruction->asDest[0].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
			psEmitInstruction->asDest[0].uArrayOffset = 0;			
		}
		{
			/*
				Read output registers.
			*/
			psEmitInstruction->asArg[EMIT_VSOUT_SOURCE].uType = USC_REGTYPE_REGARRAY;
			psEmitInstruction->asArg[EMIT_VSOUT_SOURCE].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
			psEmitInstruction->asArg[EMIT_VSOUT_SOURCE].uArrayOffset = 0;			
		}
		AppendInst(psState, psCodeBlock, psEmitInstruction);
	}
	if((psState->uFlags2 & USC_FLAGS2_TWO_PARTITION_MODE) == 0)
	{
		{	
			PINST psOutPutBufNewIdx;
			psOutPutBufNewIdx = AllocateInst(psState, IMG_NULL);
			SetOpcodeAndDestCount(psState, psOutPutBufNewIdx, ITESTPRED, TEST_MAXIMUM_DEST_COUNT);
			psOutPutBufNewIdx->u.psTest->eAluOpcode = IIADD16;
			MakePredicateArgument(&psOutPutBufNewIdx->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEMP);
			CompOpToTest(psState, UFREG_COMPOP_EQ, &psOutPutBufNewIdx->u.psTest->sTest);
			psOutPutBufNewIdx->u.psTest->sTest.eChanSel = USEASM_TEST_CHANSEL_C2;
			psOutPutBufNewIdx->asDest[TEST_UNIFIEDSTORE_DESTIDX].uType = USEASM_REGTYPE_TEMP;		
			psOutPutBufNewIdx->asDest[TEST_UNIFIEDSTORE_DESTIDX].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;		
			psOutPutBufNewIdx->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psOutPutBufNewIdx->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;		
			psOutPutBufNewIdx->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psOutPutBufNewIdx->asArg[1].uNumber = 
				/* 
					-1 in top 16 bits. 
				*/
				(UINT_MAX << 16) 
				| 
				/* 
					Output vertex stride in low 16 bits. 
				*/
				psState->sShader.psVS->uVertexShaderNumOutputs;
			AppendInst(psState, psCodeBlock, psOutPutBufNewIdx);
		}
		{
			psCondTrueBlock = AllocateBlock(psState, psCodeBlock->psOwner);
			psCondExitBlock = AllocateBlock(psState, psCodeBlock->psOwner);
			SetBlockConditional(psState, psCodeBlock, USC_PREDREG_TEMP, psCondTrueBlock, psCondExitBlock, IMG_FALSE);
		}	
		{
			PINST psInst;
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IIMA16);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;		
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;		
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 1;
			psInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[2].uNumber = 
				/* 
					Number of vertices per partition in top 16 bits. 
				*/
				((EURASIA_OUTPUT_PARTITION_SIZE / psState->sShader.psVS->uVertexShaderNumOutputs) << 16)
				| 
				/* 
					New partition offset in low 16 bits. 
				*/
				(EURASIA_OUTPUT_PARTITION_SIZE % psState->sShader.psVS->uVertexShaderNumOutputs);
			AppendInst(psState, psCondTrueBlock, psInst);
		}
		{
			/* 
				Offset and remaning vertices masking instruction. 
			*/
			PINST psInst;
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IAND);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;		
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;		
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;		
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 			
				/* 
					0xFFFF in top 16 bits. 
				*/
				0xFFFF0000
				| 
				/* 
					Max vertex offset mask in low 16 bits. 
				*/
				((psState->psSAOffsets->uOutPutBuffersCount * EURASIA_OUTPUT_PARTITION_SIZE ) - 1);
			AppendInst(psState, psCondExitBlock, psInst);
		}
		{
			/* 
				Buffer_index_reg <- (0 | index_calc_reg.low)
			*/
			PINST psInst;
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IUNPCKU16U16);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;		
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 0;
			AppendInst(psState, psCondExitBlock, psInst);		
		}
		{
			/* 
				WOP instruction. 
			*/
			PINST psWopInst;
			psWopInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psWopInst, IWOP);
			{
				/*
					Write to the output registers.
				*/
				psWopInst->asDest[0].uType = USC_REGTYPE_REGARRAY;
				psWopInst->asDest[0].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
				psWopInst->asDest[0].uArrayOffset = 0;			
			}		
			psWopInst->asArg[WOP_PTNB_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
			psWopInst->asArg[WOP_PTNB_SOURCE].uNumber = 0;
			{
				/*
					Read output registers.
				*/
				psWopInst->asArg[WOP_VSOUT_SOURCE].uType = USC_REGTYPE_REGARRAY;
				psWopInst->asArg[WOP_VSOUT_SOURCE].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
				psWopInst->asArg[WOP_VSOUT_SOURCE].uArrayOffset = 0;						
			}
			AppendInst(psState, psCondExitBlock, psWopInst);
		}
		SetBlockUnconditional(psState, psCondTrueBlock, psCondExitBlock);
		return psCondExitBlock;
	}
	else
	{
		/*
			Two Partition mode.
		*/
		{	
			PINST psOutPutBufNewIdx;
			psOutPutBufNewIdx = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psOutPutBufNewIdx, IIADD16);			
			psOutPutBufNewIdx->asDest[0].uType = USEASM_REGTYPE_TEMP;		
			psOutPutBufNewIdx->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;		
			psOutPutBufNewIdx->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psOutPutBufNewIdx->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;
			psOutPutBufNewIdx->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psOutPutBufNewIdx->asArg[1].uNumber = 				 
				/* 
					Offset of the next two partitions set. 
				*/
				2 * EURASIA_OUTPUT_PARTITION_SIZE;
			AppendInst(psState, psCodeBlock, psOutPutBufNewIdx);
		}
		{
			/* 
				Offset and remaning vertices masking instruction. 
			*/
			PINST psInst;
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IAND);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;		
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 				 
				/* 
					Max vertex offset mask in low 16 bits. 
				*/
				((psState->psSAOffsets->uOutPutBuffersCount * EURASIA_OUTPUT_PARTITION_SIZE ) - 1);
			AppendInst(psState, psCodeBlock, psInst);
		}
		{
			/* 
				WOP instruction. 
			*/
			PINST psWopInst;
			psWopInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psWopInst, IWOP);
			{
				/*
					Write to the output registers.
				*/
				psWopInst->asDest[0].uType = USC_REGTYPE_REGARRAY;
				psWopInst->asDest[0].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
				psWopInst->asDest[0].uArrayOffset = 0;			
			}		
			psWopInst->asArg[WOP_PTNB_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
			psWopInst->asArg[WOP_PTNB_SOURCE].uNumber = 0;
			{
				/*
					Read output registers.
				*/
				psWopInst->asArg[WOP_VSOUT_SOURCE].uType = USC_REGTYPE_REGARRAY;
				psWopInst->asArg[WOP_VSOUT_SOURCE].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
				psWopInst->asArg[WOP_VSOUT_SOURCE].uArrayOffset = 0;						
			}
			AppendInst(psState, psCodeBlock, psWopInst);
		}
		return psCodeBlock;
	}
}

static 
PCODEBLOCK ConvertCutInstruction(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock)
/*****************************************************************************
 FUNCTION	: ConvertCutInstruction

 PURPOSE	: Converts input cut instruction to intermediate form.

 PARAMETERS	: psState			- Compiler state.              
              psCodeBlock		- Resulting intermediate code is appended here.

 RETURNS    : NONE.
 
 OUTPUT		: NONE.
*****************************************************************************/
{
	if (psState->psTargetDesc->eCoreType != SGX_CORE_ID_545)
	{
		UscFail(psState, UF_ERR_GENERIC, "Geometry shaders aren't supported on this core.\n");
	}

	{
		PINST psCutInstruction;
		psCutInstruction = AllocateInst(psState, IMG_NULL);
		SetOpcode(psState, psCutInstruction, IEMIT);
		psCutInstruction->u.sEmit.bCut = IMG_TRUE;
		{
			/*
				Write to the output registers.
			*/
			psCutInstruction->asDest[0].uType = USC_REGTYPE_REGARRAY;
			psCutInstruction->asDest[0].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
			psCutInstruction->asDest[0].uArrayOffset = 0;
		}
		{
			/*
				Read output registers.
			*/
			psCutInstruction->asArg[EMIT_VSOUT_SOURCE].uType = USC_REGTYPE_REGARRAY;
			psCutInstruction->asArg[EMIT_VSOUT_SOURCE].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
			psCutInstruction->asArg[EMIT_VSOUT_SOURCE].uArrayOffset = 0;
		}
		AppendInst(psState, psCodeBlock, psCutInstruction);
	}
	if((psState->uFlags2 & USC_FLAGS2_TWO_PARTITION_MODE) == 0)
	{
		{
			/*	
				calc_reg <- (remaning_vertices_in_partition * vertex_size) + current_partition_offset
			*/
			PINST psInst;
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IIMAE);
			psInst->u.psImae->bSigned = IMG_FALSE;
			psInst->u.psImae->uSrc2Type = USEASM_INTSRCSEL_Z16;
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;
			psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[0].uNumber = psState->sShader.psVS->uVertexShaderNumOutputs;
			psInst->asArg[1].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[1].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;
			SetComponentSelect(psState, psInst, 1 /* uArgIdx */, 2 /* uComponent */);
			psInst->asArg[2].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[2].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;
			AppendInst(psState, psCodeBlock, psInst);
		}
		{
			PINST psInst;
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IIMA16);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;		
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;		
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 1;
			psInst->asArg[2].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[2].uNumber = 
				/* 
					Number of vertices per partition in top 16 bits. 
				*/
				((EURASIA_OUTPUT_PARTITION_SIZE / psState->sShader.psVS->uVertexShaderNumOutputs) << 16)
				| 
				/* 
					New partition offset in low 16 bits. 
				*/
				(EURASIA_OUTPUT_PARTITION_SIZE % psState->sShader.psVS->uVertexShaderNumOutputs);
			AppendInst(psState, psCodeBlock, psInst);
		}
		{
			/* 
				Offset and remaning vertices count masking instruction. 
			*/
			PINST psInst;
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IAND);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;		
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;		
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;		
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 			
				/* 
					0xFFFF in top 16 bits. 
				*/
				0xFFFF0000
				| 
				/* 
					Max vertex offset mask in low 16 bits. 
				*/
				((psState->psSAOffsets->uOutPutBuffersCount * EURASIA_OUTPUT_PARTITION_SIZE ) - 1);
			AppendInst(psState, psCodeBlock, psInst);
		}
		{
			/* 
				Buffer_index_reg <- (0 | index_calc_reg.low)
			*/
			PINST psInst;
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IUNPCKU16U16);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;		
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 0;
			AppendInst(psState, psCodeBlock, psInst);		
		}
		{
			/* 
				WOP instruction. 
			*/
			PINST psWopInst;
			psWopInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psWopInst, IWOP);
			{
				/*
					Write to the output registers.
				*/
				psWopInst->asDest[0].uType = USC_REGTYPE_REGARRAY;
				psWopInst->asDest[0].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
				psWopInst->asDest[0].uArrayOffset = 0;			
			}		
			psWopInst->asArg[WOP_PTNB_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
			psWopInst->asArg[WOP_PTNB_SOURCE].uNumber = 0;
			{
				/*
					Read output registers.
				*/
				psWopInst->asArg[WOP_VSOUT_SOURCE].uType = USC_REGTYPE_REGARRAY;
				psWopInst->asArg[WOP_VSOUT_SOURCE].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
				psWopInst->asArg[WOP_VSOUT_SOURCE].uArrayOffset = 0;						
			}
			AppendInst(psState, psCodeBlock, psWopInst);
		}	
		return psCodeBlock;
	}
	else
	{
		/*
			Two Partition mode.
		*/
		{
			PINST psOutPutBufNewIdx;
			psOutPutBufNewIdx = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psOutPutBufNewIdx, IIADD16);			
			psOutPutBufNewIdx->asDest[0].uType = USEASM_REGTYPE_TEMP;		
			psOutPutBufNewIdx->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;		
			psOutPutBufNewIdx->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psOutPutBufNewIdx->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;
			psOutPutBufNewIdx->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psOutPutBufNewIdx->asArg[1].uNumber = 				 
				/* 
					Offset of the next two partitions set. 
				*/
				2 * EURASIA_OUTPUT_PARTITION_SIZE;
			AppendInst(psState, psCodeBlock, psOutPutBufNewIdx);
		}
		{
			/* 
				Offset and remaning vertices masking instruction. 
			*/
			PINST psInst;
			psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IAND);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;		
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;
			psInst->asArg[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asArg[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;
			psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[1].uNumber = 				 
				/* 
					Max vertex offset mask in low 16 bits. 
				*/
				((psState->psSAOffsets->uOutPutBuffersCount * EURASIA_OUTPUT_PARTITION_SIZE ) - 1);
			AppendInst(psState, psCodeBlock, psInst);
		}
		{
			/* 
				WOP instruction. 
			*/
			PINST psWopInst;
			psWopInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psWopInst, IWOP);
			{
				/*
					Write to the output registers.
				*/
				psWopInst->asDest[0].uType = USC_REGTYPE_REGARRAY;
				psWopInst->asDest[0].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
				psWopInst->asDest[0].uArrayOffset = 0;			
			}		
			psWopInst->asArg[WOP_PTNB_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
			psWopInst->asArg[WOP_PTNB_SOURCE].uNumber = 0;
			{
				/*
					Read output registers.
				*/
				psWopInst->asArg[WOP_VSOUT_SOURCE].uType = USC_REGTYPE_REGARRAY;
				psWopInst->asArg[WOP_VSOUT_SOURCE].uNumber = psState->sShader.psVS->uVertexShaderOutputsArrayIdx;
				psWopInst->asArg[WOP_VSOUT_SOURCE].uArrayOffset = 0;						
			}
			AppendInst(psState, psCodeBlock, psWopInst);
		}
		return psCodeBlock;
	}
}

static 
PCODEBLOCK ConvertEmitCutInstruction(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock)
/*****************************************************************************
 FUNCTION	: ConvertEmitCutInstruction

 PURPOSE	: Converts input emit then cut instruction to intermediate form.

 PARAMETERS	: psState			- Compiler state.              
              psCodeBlock		- Resulting intermediate code is appended here.

 RETURNS    : NONE.
 
 OUTPUT		: NONE.
*****************************************************************************/
{	
	psCodeBlock = ConvertEmitInstruction(psState, psCodeBlock);
	return ConvertCutInstruction(psState, psCodeBlock);
}

static 
PCODEBLOCK ConvertSplitInstruction(PINTERMEDIATE_STATE psState, PCODEBLOCK psCodeBlock, PUNIFLEX_INST psSplitInst)
/*****************************************************************************
 FUNCTION	: ConvertSplitInstruction

 PURPOSE	: Converts input split instruction to intermediate form.

 PARAMETERS	: psState			- Compiler state.              
              psCodeBlock		- Resulting intermediate code is appended here.
			  psSplitInst		- Input Split instruction.

 RETURNS    : NONE.
 
 OUTPUT		: NONE.
*****************************************************************************/
{	
	if((psState->uFlags2 & USC_FLAGS2_SPLITCALC)!=0)
	{
		USC_ERROR(UF_ERR_INVALID_PROG_STRUCT, "More than one split points in program");
	}
	ASSERT((GetRegisterFormat(psState, &(psSplitInst->asSrc[0])) == UF_REGFORMAT_U32) && ((psSplitInst->asSrc[0].eType) == UFREG_TYPE_IMMEDIATE));
	psState->sShader.psPS->uPostSplitRate = psSplitInst->asSrc[0].uNum;
	psState->psPreSplitBlock = psCodeBlock;
	psCodeBlock = AllocateBlock(psState, psCodeBlock->psOwner);	
	SetBlockUnconditional(psState, psState->psPreSplitBlock, psCodeBlock);
	(psState->uFlags2) |= USC_FLAGS2_SPLITCALC;
	return psCodeBlock;
}

static
PCODEBLOCK DoConvert(PINTERMEDIATE_STATE psState,
					 PCALLDATA psCallData,
					 PFUNC psFunc,
					 PUNIFLEX_INST *ppsInst,
					 PCODEBLOCK psBlock,
					 PCODEBLOCK psBreakTarget,
					 PCODEBLOCK psContinueTarget,
					 IMG_BOOL bInConditional, //of function or program
					 IMG_BOOL bStatic)
/******************************************************************************
 FUNCTION	: DoConvert

 PURPOSE	: Converts input instructions and nested if/loop constructs until
			an instruction ending the containing structure is reached

 PARAMETERS	: psState		- Compiler intermediate state
			  psCallData		- Conversion status inc. input prog&calls/funcs
			  psFunc			- Containing function in intermediate code
			  ppsInst			- points to inst with which to begin conversion
			  psBlock			- block to which to append converted insts/code
			  psBreakTarget		- block whither any 'break' at this level jumps
			  psContinueTarget	- same but for 'continue' instructions
			  bInConditional	- whether this code is conditionally executed
								  relative to containing function (/main prog)
			  bStatic			- whether all paths to this point are known to
								  be the same for all pixels

 OUTPUT		: ppsInst set to the next (not yet converted) inst which ends the
			  _containing_ control flow structure - e.g. ELSE, ENDIF, ENDLOOP

 RETURNS	: The intermediate code block to/through whose end execution will
			  flow if it did not hit any input instructions which jumped out of
			  the code converted (i.e. break/continue/return)
******************************************************************************/
{
	for (; *ppsInst; *ppsInst = (*ppsInst)->psILink)
	{
#ifdef SRC_DEBUG
		psState->uCurSrcLine = (*ppsInst)->uSrcLine;
#endif /* SRC_DEBUG */

		switch ((*ppsInst)->eOpCode)
		{
			case UFOP_ENDLOOP:
			case UFOP_ENDREP:
			case UFOP_GLSLENDLOOP:
			case UFOP_GLSLLOOPTAIL:
			case UFOP_ELSE:
			case UFOP_ENDIF:
			case UFOP_CASE:
			case UFOP_DEFAULT:
			case UFOP_ENDSWITCH:
			case UFOP_LABEL:
				/*
				  Statement indicates end of some flow control construct.
				  (A label is the end of the *previous* function!)
				  Keep exitting recursive calls until we hit corresponding
				  ConvertLoop/ConvertIf/ConvertSwitch/etc.
				*/
				return psBlock;
			/* The modification to UFOP_LABEL works only when the input is well-formed. */
			case UFOP_BLOCK:
				{
					/* Start a new block and link it */
					PCODEBLOCK psNewBlock = AllocateBlock(psState, &psFunc->sCfg);

					if (psBlock)
					  SetBlockUnconditional(psState, psBlock, psNewBlock);

					psBlock = psNewBlock;

					/* Label the block */
					psBlock->uLabel = (*ppsInst)->asSrc[0].uNum;
					break; /* Continue processing instructions */
				}
			case UFOP_BREAK:
				if (psBlock)
				{
					SetBlockUnconditional(psState, psBlock, psBreakTarget);
					psBlock = NULL; //rest unreachable
				}
				break;
			case UFOP_RET:
				if (psBlock) SetBlockUnconditional(psState, psBlock, psFunc->sCfg.psExit);
				psBlock = NULL; //skip rest as unreachable
				break;
			case UFOP_RETNZBIT:
			case UFOP_BREAKC:
			case UFOP_BREAKP:
			case UFOP_BREAKNZBIT:
			{
				PCODEBLOCK psAfter = AllocateBlock(psState, &(psFunc->sCfg));
				PCODEBLOCK psTargetBlock = NULL;

                //1st succ = "taken"
				psTargetBlock = (((*ppsInst)->eOpCode == UFOP_RETNZBIT) ?
								 psFunc->sCfg.psExit :
								 psBreakTarget);

				if (psBlock != NULL)
				{
					{
						IMG_UINT32 uNumSucc;
						PCODEBLOCK psTrueSucc;
						PCODEBLOCK psFalseSucc;
						IMG_UINT32 uPredSrc;
						IMG_BOOL bPredSrcNegate;
						IMG_BOOL bStaticL;				
						DoFlowControlTest(psState, *ppsInst, psBlock, psTargetBlock, psAfter, IMG_FALSE /* bEndOfStaticLoop */, &uNumSucc, &psTrueSucc, &psFalseSucc, &uPredSrc, &bPredSrcNegate, &bStaticL);
						if(uNumSucc == 2)
						{
							SetBlockConditional(psState, psBlock, uPredSrc, psTrueSucc, psFalseSucc, bStaticL);					
						}
						else
						{
							SetBlockUnconditional(psState, psBlock, psTrueSucc);
						}
					}					
					psBlock = psAfter;
				}
				break;
			}
			case UFOP_IF:
			case UFOP_IFC:
			case UFOP_IFP:
			case UFOP_IFNZBIT:
			{
				psBlock = ConvertConditional(psState, psCallData,
											 psFunc, ppsInst, psBlock,
											 psBreakTarget, psContinueTarget,
											 bStatic);
				break;
			}
			case UFOP_SWITCH:
			{
				psBlock = ConvertSwitch(psState, psCallData,
										psFunc, ppsInst, psBlock,
										psContinueTarget,
										bStatic);
				break;
			}
			case UFOP_CALLP:
			case UFOP_CALL:
			case UFOP_CALLNZ:
			case UFOP_CALLNZBIT:
			{
				psBlock = ConvertCall(psState, psCallData, psBlock, *ppsInst);
				break;
			}
			case UFOP_JUMP:
			{
				psBlock = ConvertJump(psState, psCallData, psFunc, psBlock, *ppsInst);
				/* non conditional jumps are like RET, they should mark the end of a block */
				psBlock = NULL; /* skip rest as unreachable */
				break;
			}
			case UFOP_LOOP:
			case UFOP_REP:
			case UFOP_GLSLLOOP:
			case UFOP_GLSLSTATICLOOP:
			{
				psBlock = ConvertLoop(psState, psCallData,
									  psFunc, ppsInst, psBlock,
									  bStatic);
				break;
			}
			case UFOP_GLSLCONTINUE:
			{
				if (psBlock == NULL) break;
				if ((*ppsInst)->uPredicate != UF_PRED_NONE)
				{
					IMG_UINT32 uUFPredReg;
					IMG_UINT32 uPredCompMask;
					IMG_UINT32 uPredChan;
					IMG_BOOL bStaticPred = IMG_FALSE;
					IMG_BOOL bTaken = IMG_FALSE;

					uUFPredReg = ((*ppsInst)->uPredicate & UF_PRED_IDX_MASK) >> UF_PRED_IDX_SHIFT;
					uPredCompMask = ((*ppsInst)->uPredicate) & UF_PRED_COMP_MASK;
					uPredChan = ((uPredCompMask == UF_PRED_XYZW) ?
								 0 :
								 ((uPredCompMask - UF_PRED_X) >> UF_PRED_COMP_SHIFT));
					if (!SearchBackwardsSetp(psState, *ppsInst,
											 uUFPredReg,
											 uPredChan,
											 &bStaticPred, &bTaken))
					{
						//not known at compile time. but bStaticPred *may* be set.
						PCODEBLOCK psAfter = AllocateBlock(psState, &(psFunc->sCfg));
						IMG_UINT32 uPredNum;

						ASSERT(uUFPredReg < psState->uInputPredicateRegisterCount);
						uPredNum = USC_PREDREG_P0X + uPredChan;
						uPredNum += (uUFPredReg * CHANNELS_PER_INPUT_REGISTER) ;

						SetBlockConditional(psState, psBlock, uPredNum,
											psContinueTarget, psAfter, bStaticPred);
						bStatic = (bStatic && bStaticPred) ? IMG_TRUE : IMG_FALSE;
						psBlock = psAfter;
						break;
					}
					//SETP found and known at compile-time ==> bTaken filled in.
					if ((*ppsInst)->uPredicate & UF_PRED_NEGFLAG)
						bTaken = (!bTaken) ? IMG_TRUE : IMG_FALSE;

					if (!bTaken)
						break; //instruction effectively does nothing!
				}
				//either an unpredicated continue, or predicate true at compile-time
				SetBlockUnconditional(psState, psBlock, psContinueTarget);
				psBlock = NULL; //skip until we exit conditional
				break;
			}
			case UFOP_CONTINUENZBIT:
			{
				if (psBlock)
				{
					PCODEBLOCK psAfter = AllocateBlock(psState, &(psFunc->sCfg));
					{
						IMG_UINT32 uNumSucc;
						PCODEBLOCK psTrueSucc;
						PCODEBLOCK psFalseSucc;
						IMG_UINT32 uPredSrc;
						IMG_BOOL bPredSrcNegate;
						IMG_BOOL bStaticL;
						DoFlowControlTest(psState, *ppsInst, psBlock, psContinueTarget, psAfter, IMG_FALSE /* bEndOfStaticLoop */, &uNumSucc, &psTrueSucc, &psFalseSucc, &uPredSrc, &bPredSrcNegate, &bStaticL);
						if(uNumSucc == 2)
						{
							SetBlockConditional(psState, psBlock, uPredSrc, psTrueSucc, psFalseSucc, bStaticL);					
						}
						else
						{
							SetBlockUnconditional(psState, psBlock, psTrueSucc);
						}
					}
					psBlock = psAfter;
				}
				break;
			}
			case UFOP_EMIT:
			{			
				psBlock = ConvertEmitInstruction(psState, psBlock);				
				break;
			}
			case UFOP_CUT:
			{			
				psBlock = ConvertCutInstruction(psState, psBlock);				
				break;
			}
			case UFOP_EMITCUT:
			{
				psBlock = ConvertEmitCutInstruction(psState, psBlock);
				break;
			}
			case UFOP_SPLIT:
			{			
				psBlock = ConvertSplitInstruction(psState, psBlock, *ppsInst);				
				break;
			}
			case UFOP_KPLE:
			case UFOP_KPLT:
			case UFOP_KILLNZBIT:
			{
				if (psBlock == NULL)
				{
					/*
					  Instruction is to be skipped - hopefully, it's
					  part of a conditional that we've determined at
					  compile-time. So set texkill flag.  (It's also
					  possible that it's inside an indeterminate if,
					  but after a break/ret/continue - in which case
					  this isn't really necessary. Hoping that's
					  sufficiently rare not to worry!)
					*/
					psState->uFlags |= USC_FLAGS_TEXKILL_PRESENT;
					psState->uFlags |= USC_FLAGS_INITIALISE_TEXKILL;
					break;
				}
				//else fall through.
			}
			default:
			{
				IMG_BOOL bInFunction;
				if (psBlock == NULL)
				{
					IMG_UINT32 uDest = g_asInputInstDesc[(*ppsInst)->eOpCode].uNumDests;
					while (uDest-- > 0) //NOTE DECREMENT
					{
						PUF_REGISTER psDest;
						psDest = (uDest==2) ? &(*ppsInst)->sDest2 : &(*ppsInst)->sDest;

						if (psDest->u.byMask &&
							psDest->eType == UFREG_TYPE_PSOUTPUT &&
							psDest->uNum == UFREG_OUTPUT_Z)
						{
							psState->uFlags |= USC_FLAGS_DEPTHFEEDBACKPRESENT;
						}
						else if (psDest->u.byMask &&
							psDest->eType == UFREG_TYPE_PSOUTPUT &&
							psDest->uNum == UFREG_OUTPUT_OMASK)
						{
							psState->uFlags |= USC_FLAGS_OMASKFEEDBACKPRESENT;
						}						
					}
					break; //skip instruction
				}
                //in function
				bInFunction = psFunc == psState->psMainProg ? bInConditional : IMG_TRUE;
				psBlock = ConvertInstToIntermediate(psState, psBlock, *ppsInst,
													bInFunction, bStatic);
			}
		}
	}
	return psBlock;
}

static IMG_VOID ApplyAbsoluteModifier(PUF_REGISTER	psSrc)
/*****************************************************************************
 FUNCTION	: ApplyAbsoluteModifier

 PURPOSE	: Change an input source to have an absolute modifier.

 PARAMETERS	: psSrc		- Points to the source to change.

 RETURNS	: None.
*****************************************************************************/
{
	psSrc->byMod = (IMG_BYTE)(psSrc->byMod & ~UFREG_SOURCE_NEGATE);
	psSrc->byMod |= UFREG_SOURCE_ABS;
}

/*****************************************************************************
 FUNCTION	: RemapInputSwizzle

 PURPOSE	: Remap the elements of a input instruction swizzle according
			  to another swizzle.

 PARAMETERS	: uOrgSwizzle	- The original swizzle to be remapped
			  uRemapSwizzle	- Swizzle specifying the remapped order of the
							  swizzle elements

 RETURNS	: The remapped swizzle
*****************************************************************************/
static IMG_UINT16 RemapInputSwizzle(IMG_UINT16	uOrgSwizzle,
									IMG_UINT16	uRemapSwizzle)
{
	IMG_UINT16	uNewIdx;
	IMG_UINT16	uNewSwizzle;

	uNewSwizzle = 0;
	for	(uNewIdx = 0; uNewIdx < CHANNELS_PER_INPUT_REGISTER; uNewIdx++)
	{
		IMG_UINT16	uOrgIdx;
		IMG_UINT16	uOrgChan;

		uOrgIdx = (IMG_UINT16)((uRemapSwizzle >> (uNewIdx * 3)) & 0x7U);
		if	(uOrgIdx <= UFREG_SWIZ_A)
		{
			uOrgChan = (IMG_UINT16)((uOrgSwizzle >> (uOrgIdx * 3)) & 0x7U);
		}
		else
		{
			uOrgChan = uOrgIdx;
		}
		uNewSwizzle = (IMG_UINT16)((IMG_UINT32)uNewSwizzle | ((IMG_UINT32)uOrgChan << (uNewIdx * 3U)));
	}
	return uNewSwizzle;
}

#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
static IMG_BOOL UseFNormalise(PINTERMEDIATE_STATE	psState,
							  PUNIFLEX_INST			psOrigInst)
/*****************************************************************************
 FUNCTION	: UseFNormalise

 PURPOSE	: Check if an input NRM instruction should use the FNRM16/FNRM32
			  hardware instruction.

 PARAMETERS	: psState		- Compiler state.
			  psOrigInst	- NRM instruction.

 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	/*
		Is a normalise unit present in the hardware?
	*/
	if (!(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FNORMALISE))
	{
		return IMG_FALSE;
	}
	/*
		Don't use the normalise unit if either of BRN25580 or BRN25582 are present.
	*/
	if (psState->psTargetBugs->ui32Flags & (SGX_BUG_FLAGS_FIX_HW_BRN_25580 | SGX_BUG_FLAGS_FIX_HW_BRN_25582))
	{
		return IMG_FALSE;
	}
	/*
		Don't use the normalise unit if compiling with MSAA with translucency and BRN27723 is present.
	*/
	if (
			(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_27723) &&
			(psState->uCompilerFlags & UF_MSAATRANS)
	   )
	{
		return IMG_FALSE;
	}
	/*
		The hardware normalise doesn't give the right result for the W channel of
		the destination.
	*/
	if (psOrigInst->sDest.u.byMask & (1U << UFREG_SWIZ_W))
	{
		return IMG_FALSE;
	}
	return IMG_TRUE;
}
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX5450) || defined(SUPPORT_SGX541) */

static IMG_VOID BuildInputDest(PUF_REGISTER	psDest,
							   UF_REGTYPE	eRegType,
							   IMG_UINT32	uRegNum,
							   UF_REGFORMAT	eRegFormat,
							   IMG_UINT32	uMask)
/*****************************************************************************
 FUNCTION	: BuildInputDest

 PURPOSE	: Set up the structure representing an input format destination.

 PARAMETERS	: psSrc			- Structure to set up.
			  eRegType		- Register type.
			  uRegNum		- Register number.
			  eRegFormat	- Register format.
			  uMask			- Channels to write in the register.

 RETURNS	: None.
*****************************************************************************/
{
	psDest->eType = eRegType;
	psDest->uNum = uRegNum;
	psDest->eFormat = eRegFormat;
	psDest->u.byMask = (IMG_UINT8)uMask;
	psDest->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psDest->byMod = (UFREG_DMOD_SATNONE		<< UFREG_DMOD_SAT_SHIFT) |
					(UFREG_DMOD_SCALEMUL1	<< UFREG_DMOD_SCALE_SHIFT);
}


static IMG_VOID BuildInputSrc(PUF_REGISTER	psSrc,
							  UF_REGTYPE	eRegType,
						      IMG_UINT32	uRegNum,
							  UF_REGFORMAT	eRegFormat,
							  IMG_UINT32	uSwiz)
/*****************************************************************************
 FUNCTION	: BuildInputSrc

 PURPOSE	: Set up the structure representing a input format source.

 PARAMETERS	: psSrc			- Structure to set up.
			  eRegType		- Register type.
			  uRegNum		- Register number.
			  eRegFormat	- Register format.
			  uSwiz			- Swizzle to apply to the register.

 RETURNS	: None.
*****************************************************************************/
{
	psSrc->eType = eRegType;
	psSrc->uNum = uRegNum;
	psSrc->eFormat = eRegFormat;
	psSrc->u.uSwiz = (IMG_UINT16)uSwiz;
	psSrc->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psSrc->byMod = UFREG_SMOD_NONE;
}

static IMG_VOID BuildInputPredDest(PUF_REGISTER	psDest,
								   IMG_UINT32	uPredNum,
								   IMG_UINT32	uMask)
/*****************************************************************************
 FUNCTION	: BuildInputPredDest

 PURPOSE	: Set up the structure representing a predicate register destination.

 PARAMETERS	: psSrc			- Structure to set up.
			  uPredNum		- Predicate register number.
			  uMask			- Channels to write in the predicate register.

 RETURNS	: None.
*****************************************************************************/
{
	psDest->eType = UFREG_TYPE_PREDICATE;
	psDest->uNum = uPredNum;
	psDest->eFormat = UF_REGFORMAT_F32;
	psDest->u.byMask = (IMG_UINT8)uMask;
	psDest->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psDest->byMod = (UFREG_DMOD_SATNONE		<< UFREG_DMOD_SAT_SHIFT) |
					(UFREG_DMOD_SCALEMUL1	<< UFREG_DMOD_SCALE_SHIFT);
}

static IMG_VOID BuildInputCompOp(PUF_REGISTER	psSrc,
								 UFREG_COMPOP	eCompOp)
/*****************************************************************************
 FUNCTION	: BuildInputCompOp

 PURPOSE	: Set up the structure representing a comparison operator source.

 PARAMETERS	: psSrc			- Structure to set up.
			  eCompOp		- Comparison operator.

 RETURNS	: None.
*****************************************************************************/
{
	psSrc->eType = UFREG_TYPE_COMPOP;
	psSrc->uNum = eCompOp;
	psSrc->eFormat = UF_REGFORMAT_F32;
	psSrc->u.uSwiz = UFREG_SWIZ_NONE;
	psSrc->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psSrc->byMod = UFREG_SMOD_NONE;
}

static IMG_VOID BuildInputPredicate(PUNIFLEX_INST	psInst,
									IMG_UINT32		uPredNum,
									IMG_UINT32		uPredComp,
									IMG_BOOL		bPredNegate)
/*****************************************************************************
 FUNCTION	: BuildInputPredicate

 PURPOSE	: Set up the structure representing a predicate controlling
			  instruction execution.

 PARAMETERS	: psInst		- Instruction to apply the predicate to.
			  uPredNum		- Predicate register number.
			  uPredComp		- Predicate register component.
			  bPredNegate	- If TRUE the predicate register is logically
							inverted.

 RETURNS	: None.
*****************************************************************************/
{
	psInst->uPredicate = (uPredNum << UF_PRED_IDX_SHIFT) | 
						 uPredComp |
						 (bPredNegate ? UF_PRED_NEGFLAG : 0);
}

static IMG_VOID ExpandTRC(PINTERMEDIATE_STATE	psState,
						  PUNIFLEX_INST			psOrigInst,
						  PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandTRC

 PURPOSE	: Expand an input TRC instruction.

 PARAMETERS	: psState		- Compiler state.
			  psOriginst	- Input instruction to expand.
			  psProg		- 
			  uTempReg		- Index of the temporary register to use when
							expanding the instruction.

 RETURNS	: None.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;

	#if defined(SUPPORT_SGX545)
	if (
			psOrigInst->sDest.eFormat == UF_REGFORMAT_F32 &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_PACK_MULTIPLE_ROUNDING_MODES)
	   )
	{
		/*
			The hardware supports this instruction directly.
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psInst, psOrigInst);
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	{
		UF_REGFORMAT	eTempRegFormat;
		IMG_UINT32		uTempReg = psState->uInputTempRegisterCount++;

		/* Internal results for U8 is calculated in C10 format. */
		if (psOrigInst->sDest.eFormat == UF_REGFORMAT_U8)
		{
			eTempRegFormat = UF_REGFORMAT_C10;
		}
		else
		{
			eTempRegFormat = psOrigInst->sDest.eFormat;
		}

		/*
			  TEMP = FRC(ABS(SRC))
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_FRC;
		psInst->sDest.byMod = 0;
		psInst->sDest.eType = UFREG_TYPE_TEMP;
		psInst->sDest.uNum = uTempReg;
		psInst->sDest.u.byMask = psOrigInst->sDest.u.byMask;
		psInst->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->sDest.eFormat = eTempRegFormat;

		psInst->asSrc[0] = psOrigInst->asSrc[0];
		ApplyAbsoluteModifier(&psInst->asSrc[0]);
		psInst->uPredicate = UF_PRED_NONE;

		/*
		  TEMP = ABS(SRC) - TEMP
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_ADD;
		psInst->sDest.byMod = 0;
		psInst->sDest.eType = UFREG_TYPE_TEMP;
		psInst->sDest.uNum = uTempReg;
		psInst->sDest.u.byMask = psOrigInst->sDest.u.byMask;
		psInst->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->sDest.eFormat = eTempRegFormat;
				
		psInst->asSrc[0] = psOrigInst->asSrc[0];
		ApplyAbsoluteModifier(&psInst->asSrc[0]);
		psInst->asSrc[1].byMod = UFREG_SOURCE_NEGATE;
		psInst->asSrc[1].eType = UFREG_TYPE_TEMP;
		psInst->asSrc[1].uNum = uTempReg;
		psInst->asSrc[1].u.uSwiz = UFREG_SWIZ_NONE;
		psInst->asSrc[1].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->asSrc[1].eFormat = eTempRegFormat;

		psInst->uPredicate = UF_PRED_NONE;

		/*
		  DEST = S >= 0 ? TEMP : -TEMP
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_CMP;
		psInst->sDest = psOrigInst->sDest;
		psInst->asSrc[0] = psOrigInst->asSrc[0];
		psInst->asSrc[1].byMod = 0;
		psInst->asSrc[1].eType = UFREG_TYPE_TEMP;

		psInst->asSrc[1].uNum = uTempReg;
		psInst->asSrc[1].u.uSwiz = UFREG_SWIZ_NONE;
		psInst->asSrc[1].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->asSrc[1].eFormat = eTempRegFormat;

		psInst->asSrc[2].byMod = UFREG_SOURCE_NEGATE;
		psInst->asSrc[2].eType = UFREG_TYPE_TEMP;
		psInst->asSrc[2].uNum = uTempReg;
		psInst->asSrc[2].u.uSwiz = UFREG_SWIZ_NONE;
		psInst->asSrc[2].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->asSrc[2].eFormat = eTempRegFormat;

		psInst->uPredicate = psOrigInst->uPredicate;
	}
}

static IMG_VOID ExpandRNE(PINTERMEDIATE_STATE	psState,
						  PUNIFLEX_INST			psOrigInst,
						  PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandRNE

 PURPOSE	: Expand an input RNE instruction.

 PARAMETERS	: psState		- Compiler state.
			  psOriginst	- Input instruction to expand.
			  psProg		- 

 RETURNS	: None.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;

	#if defined(SUPPORT_SGX545)
	if (
			psOrigInst->sDest.eFormat == UF_REGFORMAT_F32 &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_PACK_MULTIPLE_ROUNDING_MODES)
	   )
	{
		/*
			The hardware supports this instruction directly.
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psInst, psOrigInst);
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	{
		IMG_UINT32	uTempReg = psState->uInputTempRegisterCount++;
		IMG_UINT32	uTemp2 = psState->uInputTempRegisterCount++;

		/*
		  TEMP1 = SRC + 0.5
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_ADD;
		psInst->sDest.byMod = 0;
		psInst->sDest.eType = UFREG_TYPE_TEMP;
		psInst->sDest.uNum = uTempReg;
		psInst->sDest.u.byMask = psOrigInst->sDest.u.byMask;
		psInst->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->sDest.eFormat = psOrigInst->sDest.eFormat;
		psInst->asSrc[0] = psOrigInst->asSrc[0];
		psInst->asSrc[1].eType = UFREG_TYPE_HW_CONST;
		psInst->asSrc[1].uNum = HW_CONST_HALF;
		psInst->asSrc[1].u.uSwiz = UFREG_SWIZ_NONE;
		psInst->asSrc[1].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->asSrc[1].eFormat = psOrigInst->sDest.eFormat;
		psInst->asSrc[1].byMod = 0;
		psInst->uPredicate = UF_PRED_NONE;

		/*	
			TEMP2 = FRC(TEMP1)
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_FRC;
		psInst->sDest.byMod = 0;
		psInst->sDest.eType = UFREG_TYPE_TEMP;
		psInst->sDest.uNum = uTemp2;
		psInst->sDest.u.byMask = psOrigInst->sDest.u.byMask;
		psInst->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->sDest.eFormat = psOrigInst->sDest.eFormat;
		psInst->asSrc[0].byMod = 0;
		psInst->asSrc[0].eType = UFREG_TYPE_TEMP;
		psInst->asSrc[0].uNum = uTempReg;
		psInst->asSrc[0].u.uSwiz = UFREG_SWIZ_NONE;
		psInst->asSrc[0].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->asSrc[0].eFormat = psOrigInst->sDest.eFormat;
		psInst->uPredicate = UF_PRED_NONE;

		/*
		  DEST = TEMP1 - TEMP2
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_ADD;
		psInst->sDest = psOrigInst->sDest;
		psInst->asSrc[0].byMod = 0;
		psInst->asSrc[0].eType = UFREG_TYPE_TEMP;
		psInst->asSrc[0].uNum = uTempReg;
		psInst->asSrc[0].u.uSwiz = UFREG_SWIZ_NONE;
		psInst->asSrc[0].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->asSrc[0].eFormat = psOrigInst->sDest.eFormat;
		psInst->asSrc[1].byMod = UFREG_SOURCE_NEGATE;
		psInst->asSrc[1].eType = UFREG_TYPE_TEMP;
		psInst->asSrc[1].uNum = uTemp2;
		psInst->asSrc[1].u.uSwiz = UFREG_SWIZ_NONE;
		psInst->asSrc[1].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psInst->asSrc[1].eFormat = psOrigInst->sDest.eFormat;
		psInst->uPredicate = psOrigInst->uPredicate;
	}
}

static IMG_VOID BuildInputTempRegDest(PUF_REGISTER	psDest,
									  IMG_UINT32	uTempNum,
									  UF_REGFORMAT	eTempFormat,
									  IMG_UINT32	uMask)
/*****************************************************************************
 FUNCTION	: BuildInputTempRegDest

 PURPOSE	: Set up the structure representing a temporary register destination.

 PARAMETERS	: psSrc			- Structure to set up.
			  uTempNum		- Temporary register number.
			  eTempFormat	- Temporary register format.
			  uMask			- Channels to write in the temporary register.

 RETURNS	: None.
*****************************************************************************/
{
	psDest->eType = UFREG_TYPE_TEMP;
	psDest->uNum = uTempNum;
	psDest->eFormat = eTempFormat;
	psDest->u.byMask = (IMG_UINT8)uMask;
	psDest->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psDest->byMod = (UFREG_DMOD_SATNONE		<< UFREG_DMOD_SAT_SHIFT) |
					(UFREG_DMOD_SCALEMUL1	<< UFREG_DMOD_SCALE_SHIFT);
}


static IMG_VOID BuildInputTempRegSrc(PUF_REGISTER	psSrc,
									  IMG_UINT32	uTempNum,
									  UF_REGFORMAT	eTempFormat,
									  IMG_UINT32	uSwiz)
/*****************************************************************************
 FUNCTION	: BuildInputTempRegSrc

 PURPOSE	: Set up the structure representing a temporary register source.

 PARAMETERS	: psSrc			- Structure to set up.
			  uTempNum		- Temporary register number.
			  eTempFormat	- Temporary register format.
			  uSwiz			- Swizzle to apply to the temporary register.

 RETURNS	: None.
*****************************************************************************/
{
	psSrc->eType = UFREG_TYPE_TEMP;
	psSrc->uNum = uTempNum;
	psSrc->eFormat = eTempFormat;
	psSrc->u.uSwiz = (IMG_UINT16)uSwiz;
	psSrc->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psSrc->byMod = UFREG_SMOD_NONE;
}

static IMG_VOID BuildInputImmediateSrc(PUF_REGISTER	psSrc,
									   const IMG_UINT32	uImmVal,
									   UF_REGFORMAT	eImmFormat)
/*****************************************************************************
 FUNCTION	: BuildInputImmediateSrc

 PURPOSE	: Set up the structure representing a immediate source.

 PARAMETERS	: psSrc			- Structure to set up.
			  uImmVal		- Immediate source value.
			  eImmFormat	- Immediate format.

 RETURNS	: None.
*****************************************************************************/
{
	psSrc->eType = UFREG_TYPE_IMMEDIATE;
	psSrc->uNum = uImmVal;
	psSrc->eFormat = eImmFormat;
	psSrc->u.uSwiz = UFREG_SWIZ_NONE;
	psSrc->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psSrc->byMod = UFREG_SMOD_NONE;
}

static IMG_VOID BuildInputHwConstSrc(PUF_REGISTER	psSrc,
									 IMG_UINT32		uHwConstIdx,
									 UF_REGFORMAT	eFormat)
/*****************************************************************************
 FUNCTION	: BuildInputImmediateSrc

 PURPOSE	: Set up the structure representing a hardware constant.

 PARAMETERS	: psSrc			- Structure to set up.
			  uHwConstIdx	- Index of the hardware constant.
			  eFormat		- Format for the hardware constant.

 RETURNS	: None.
*****************************************************************************/
{
	psSrc->eType = UFREG_TYPE_HW_CONST;
	psSrc->uNum = uHwConstIdx;
	psSrc->eFormat = eFormat;
	psSrc->u.uSwiz = UFREG_SWIZ_NONE;
	psSrc->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psSrc->byMod = UFREG_SMOD_NONE;
}

static IMG_VOID BuildInputFloatImmediateSrc(PUF_REGISTER	psSrc,
											IMG_FLOAT		fImmVal,
											UF_REGFORMAT	eImmFormat)
/*****************************************************************************
 FUNCTION	: BuildInputFloatImmediateSrc

 PURPOSE	: Set up the structure representing a immediate source.

 PARAMETERS	: psSrc			- Structure to set up.
			  fImmVal		- Immediate source value.
			  eImmFormat	- Immediate format.

 RETURNS	: None.
*****************************************************************************/
{
	psSrc->eType = UFREG_TYPE_IMMEDIATE;
	psSrc->uNum = *(IMG_PUINT32)&fImmVal;
	psSrc->eFormat = eImmFormat;
	psSrc->u.uSwiz = UFREG_SWIZ_NONE;
	psSrc->eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
	psSrc->byMod = UFREG_SMOD_NONE;
}

static IMG_VOID ExpandCEIL(PINTERMEDIATE_STATE	psState,
						   PUNIFLEX_INST		psOrigInst,
						   PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandCEIL

 PURPOSE	: Expand an input CEIL instruction.

 PARAMETERS	: psState		- Compiler state.
			  psOriginst	- Input instruction to expand.
			  psProg		- 

 RETURNS	: None.
*****************************************************************************/
{
	if (psOrigInst->sDest.eFormat == UF_REGFORMAT_F32)
	{
		PUNIFLEX_INST	psInst;

		psInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psInst, psOrigInst);
	}
	else
	{
		IMG_UINT32		uInTempReg = psState->uInputTempRegisterCount++;
		IMG_UINT32		uOutTempReg = psState->uInputTempRegisterCount++;
		PUNIFLEX_INST	psCvtToInst, psCeilInst, psCvtFromInst;

		/*
			Convert the source argument to F32, do the ceil operation then convert the
			result to the destination format.
		*/

		/* MOV	INTEMP_f32, ORIGSRC */
		psCvtToInst = AllocInputInst(psState, psProg, psOrigInst);
		psCvtToInst->eOpCode = UFOP_MOV;
		psCvtToInst->uPredicate = UF_PRED_NONE;
		BuildInputTempRegDest(&psCvtToInst->sDest, uInTempReg, UF_REGFORMAT_F32, psOrigInst->sDest.u.byMask);
		psCvtToInst->asSrc[0] = psOrigInst->asSrc[0];

		/* CEIL	OUTTEMP_f32, INTEMP_f32 */
		psCeilInst = AllocInputInst(psState, psProg, psOrigInst);
		psCeilInst->eOpCode = UFOP_CEIL;
		psCeilInst->uPredicate = UF_PRED_NONE;
		BuildInputTempRegDest(&psCeilInst->sDest, uOutTempReg, UF_REGFORMAT_F32, psOrigInst->sDest.u.byMask);
		BuildInputTempRegSrc(&psCeilInst->asSrc[0], uInTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);

		/* MOV ORIGDEST, OUTTEMP_f32 */
		psCvtFromInst = AllocInputInst(psState, psProg, psOrigInst);
		psCvtFromInst->eOpCode = UFOP_MOV;
		psCvtFromInst->sDest = psOrigInst->sDest;
		BuildInputTempRegSrc(&psCvtFromInst->asSrc[0], uOutTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		psCvtFromInst->uPredicate = psOrigInst->uPredicate;
	}
}

static IMG_VOID ExpandSLT_SGE(PINTERMEDIATE_STATE	psState,
							  PUNIFLEX_INST			psOrigInst,
							  PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandSLT_SGE

 PURPOSE	: Expand an input SLT or SGE instruction.

 PARAMETERS	: psState		- Compiler state.
			  psOriginst	- Input instruction to expand.
			  psProg		- 
			  uTempReg		- Index of the temporary register to use when
							expanding the instruction.

 RETURNS	: None.
*****************************************************************************/
{
	UF_REGFORMAT	eFormat = psOrigInst->sDest.eFormat;

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0 &&
			(
				eFormat == UF_REGFORMAT_F32 ||
				eFormat == UF_REGFORMAT_F16
			)
		)
	{
		PUNIFLEX_INST	psInst;

		psInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psInst, psOrigInst);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		PUNIFLEX_INST	psInst;
		IMG_UINT32		uTempReg1 = psState->uInputTempRegisterCount++;

		/* ADD TEMP1, SRC0, -SRC1 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_ADD;
		psInst->uPredicate = UF_PRED_NONE;
		BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, USC_DESTMASK_FULL);
		psInst->asSrc[0] = psOrigInst->asSrc[0];
		psInst->asSrc[1] = psOrigInst->asSrc[1];
		psInst->asSrc[1].byMod ^= UFREG_SOURCE_NEGATE; 
		
		/* CMP DEST, TEMP1, 1, 0 / CMPLT DEST, TEMP1, 1, 0 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = (psOrigInst->eOpCode == UFOP_SGE) ? UFOP_CMP : UFOP_CMPLT;
		psInst->uPredicate = psOrigInst->uPredicate;
		psInst->sDest = psOrigInst->sDest;
		BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg1, eFormat, UFREG_SWIZ_NONE);
		BuildInputHwConstSrc(&psInst->asSrc[1], HW_CONST_1, eFormat);
		BuildInputHwConstSrc(&psInst->asSrc[2], HW_CONST_0, eFormat);
	}
}

static IMG_VOID ExpandCND(PINTERMEDIATE_STATE	psState,
						  PUNIFLEX_INST			psOrigInst,
						  PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandCND

 PURPOSE	: Expand an input CND instruction.

 PARAMETERS	: psState		- Compiler state.
			  psOriginst	- Input instruction to expand.
			  psProg		- 

 RETURNS	: None.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;
	IMG_UINT32		uTempReg1 = psState->uInputTempRegisterCount++;
	UF_REGFORMAT	eFormat = psOrigInst->sDest.eFormat;

	/* ADD TEMP1, -SRC0, 0.5 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_ADD;
	psInst->uPredicate = UF_PRED_NONE;
	BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, USC_DESTMASK_FULL);
	psInst->asSrc[0] = psOrigInst->asSrc[0];
	psInst->asSrc[0].byMod ^= UFREG_SOURCE_NEGATE; 
	BuildInputHwConstSrc(&psInst->asSrc[1], HW_CONST_HALF, eFormat);

	/* CMP DEST, TEMP1, SRC2, SRC1 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_CMP;
	psInst->uPredicate = psOrigInst->uPredicate;
	psInst->sDest = psOrigInst->sDest;
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg1, eFormat, UFREG_SWIZ_NONE);
	psInst->asSrc[1] = psOrigInst->asSrc[2];
	psInst->asSrc[2] = psOrigInst->asSrc[1];
}

static IMG_VOID ExpandLIT(PINTERMEDIATE_STATE	psState,
						  PUNIFLEX_INST			psOrigInst,
						  PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandLIT

 PURPOSE	: Expand an input LIT or OGLLIT instruction.

 PARAMETERS	: psState		- Compiler state.
			  psOriginst	- Input instruction to expand.
			  psProg		- List of instructions to add the expanded
							instructions to.

 RETURNS	: None.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;
	UF_REGFORMAT	eFormat = psOrigInst->sDest.eFormat;
	IMG_UINT32		uTempReg1 = psState->uInputTempRegisterCount++;
	IMG_UINT32		uTempReg2 = psState->uInputTempRegisterCount++;
	IMG_UINT32		uPredTempReg = psState->uInputPredicateRegisterCount++;

	/*
		LIT	DEST, SRC ->

		[ D3D + OGL ]
		MOV DEST.x, 1					
		MOV DEST.w, 1
		SETP SRC.x, >, 0
		p MOV DEST.y, SRC.x
		!p MOV DEST.y, 0
										
		[ OGL only ]
		!p MOV DEST.z, 0				
		p MOV DEST.z, 1		
		p SETP p, SRC.w, !=, 0
		SETP p2, FALSE
		p SETP p2, SRC.y, <=, 0			

		[ D3D + OGL ]
		p SETP p, SRC.y, >, 0
		p MAX TEMP, SRC.w, -128
		p MIN TEMP, TEMP, 128
		p LOG DEST.z, SRC.y
		p MUL DEST.z, TEMP, DEST.z
		p EXP DEST.z, DEST.z

		[ D3D only ]
		!p MOV DEST.z, 0

		[ OGL only ]
		p2 MOV DEST.z, 0
	*/

	/* MOV TEMP1.x, 1 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOV;
	psInst->uPredicate = UF_PRED_NONE;
	BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, 1 << UFREG_DMASK_X_SHIFT);
	BuildInputFloatImmediateSrc(&psInst->asSrc[0], 1.0f, eFormat);

	/* MOV TEMP1.w, 1 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOV;
	psInst->uPredicate = UF_PRED_NONE;
	BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, 1 << UFREG_DMASK_W_SHIFT);
	BuildInputFloatImmediateSrc(&psInst->asSrc[0], 1.0f, eFormat);

	/* SETP SRC.x, >, 0 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_SETP;
	psInst->uPredicate = UF_PRED_NONE;
	BuildInputPredDest(&psInst->sDest, uPredTempReg, 1 << UFREG_DMASK_X_SHIFT);
	psInst->asSrc[0] = psOrigInst->asSrc[0];
	BuildInputCompOp(&psInst->asSrc[1], UFREG_COMPOP_GT);
	BuildInputFloatImmediateSrc(&psInst->asSrc[2], 0.0f, eFormat);

	/* p MOV TEMP1.y, SRC.x */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOV;
	BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_FALSE /* bNegate */);
	BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, 1 << UFREG_DMASK_Y_SHIFT);
	psInst->asSrc[0] = psOrigInst->asSrc[0];
	psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_XXXX);

	/* !p MOV TEMP1.y, 0 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOV;
	BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_TRUE /* bNegate */);
	BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, 1 << UFREG_DMASK_Y_SHIFT);
	BuildInputFloatImmediateSrc(&psInst->asSrc[0], 0.0f, eFormat);

	if (psOrigInst->eOpCode == UFOP_OGLLIT)
	{
		/* !p MOV TEMP1.z, 0	*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MOV;
		BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_TRUE /* bNegate */);
		BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, 1 << UFREG_DMASK_Z_SHIFT);
		BuildInputFloatImmediateSrc(&psInst->asSrc[0], 0.0f, eFormat);

		/* p MOV TEMP1.z, 1 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MOV;
		BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_FALSE /* bNegate */);
		BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, 1 << UFREG_DMASK_Z_SHIFT);
		BuildInputFloatImmediateSrc(&psInst->asSrc[0], 1.0f, eFormat);

		/* p SETP p, SRC.w, !=, 0 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_SETP;
		BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_FALSE /* bNegate */);
		BuildInputPredDest(&psInst->sDest, uPredTempReg, 1 << UFREG_DMASK_X_SHIFT);
		psInst->asSrc[0] = psOrigInst->asSrc[0];
		psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_WWWW);
		BuildInputCompOp(&psInst->asSrc[1], UFREG_COMPOP_NE);
		BuildInputFloatImmediateSrc(&psInst->asSrc[2], 0.0f, eFormat);

		/* SETP p2, FALSE */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_SETP;
		BuildInputPredDest(&psInst->sDest, uPredTempReg, 1 << UFREG_DMASK_Y_SHIFT);
		psInst->uPredicate = UF_PRED_NONE;
		BuildInputFloatImmediateSrc(&psInst->asSrc[0], 0.0f, eFormat);
		BuildInputCompOp(&psInst->asSrc[1], UFREG_COMPOP_NE);
		BuildInputFloatImmediateSrc(&psInst->asSrc[2], 0.0f, eFormat);

		/* p SETP p2, SRC.y, <=, 0 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_SETP;
		BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_FALSE /* bNegate */);
		BuildInputPredDest(&psInst->sDest, uPredTempReg, 1 << UFREG_DMASK_Y_SHIFT);
		psInst->asSrc[0] = psOrigInst->asSrc[0];
		psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_YYYY);
		BuildInputCompOp(&psInst->asSrc[1], UFREG_COMPOP_LE);
		BuildInputFloatImmediateSrc(&psInst->asSrc[2], 0.0f, eFormat);
	}

	/* p SETP p, SRC.y, >, 0 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_SETP;
	BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_FALSE /* bNegate */);
	BuildInputPredDest(&psInst->sDest, uPredTempReg, 1 << UFREG_DMASK_X_SHIFT);
	psInst->asSrc[0] = psOrigInst->asSrc[0];
	psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_YYYY);
	BuildInputCompOp(&psInst->asSrc[1], UFREG_COMPOP_GT);
	BuildInputFloatImmediateSrc(&psInst->asSrc[2], 0.0f, eFormat);

	/* p MAX TEMP2, SRC.w, -128 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MAX;
	BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_FALSE /* bNegate */);
	BuildInputTempRegDest(&psInst->sDest, uTempReg2, eFormat, 1 << UFREG_DMASK_X_SHIFT);
	psInst->asSrc[0] = psOrigInst->asSrc[0];
	psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_WWWW);
	BuildInputFloatImmediateSrc(&psInst->asSrc[1], -128.0f, eFormat);

	/* p MIN TEMP2, TEMP2, 128 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MIN;
	BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_FALSE /* bNegate */);
	BuildInputTempRegDest(&psInst->sDest, uTempReg2, eFormat, 1 << UFREG_DMASK_X_SHIFT);
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg2, eFormat, UFREG_SWIZ_XXXX);
	BuildInputFloatImmediateSrc(&psInst->asSrc[1], 128.0f, eFormat);

	/* p LOG TEMP1.z, SRC.y */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_LOG;
	BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_FALSE /* bNegate */);
	BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, 1 << UFREG_DMASK_Z_SHIFT);
	psInst->asSrc[0] = psOrigInst->asSrc[0];
	psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_YYYY);

	/* p MUL TEMP1.z, TEMP2, TEMP1.z */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MUL;
	BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_FALSE /* bNegate */);
	BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, 1 << UFREG_DMASK_Z_SHIFT);
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg2, eFormat, UFREG_SWIZ_XXXX);
	BuildInputTempRegSrc(&psInst->asSrc[1], uTempReg1, eFormat, UFREG_SWIZ_ZZZZ);

	/* p EXP TEMP1.z, TEMP1.z */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_EXP;
	BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_FALSE /* bNegate */);
	BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, 1 << UFREG_DMASK_Z_SHIFT);
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg1, eFormat, UFREG_SWIZ_ZZZZ);

	/* !p MOV TEMP1.z, 0 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOV;
	if (psOrigInst->eOpCode == UFOP_LIT)
	{
		BuildInputPredicate(psInst, uPredTempReg, UF_PRED_X, IMG_TRUE /* bNegate */);
	}
	else
	{
		BuildInputPredicate(psInst, uPredTempReg, UF_PRED_Y, IMG_FALSE /* bNegate */);
	}
	BuildInputTempRegDest(&psInst->sDest, uTempReg1, eFormat, 1 << UFREG_DMASK_Z_SHIFT);
	BuildInputFloatImmediateSrc(&psInst->asSrc[0], 0.0f, eFormat);

	/* MOV DEST, TEMP1 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOV;
	psInst->uPredicate = psOrigInst->uPredicate;
	psInst->sDest = psOrigInst->sDest;
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg1, eFormat, UFREG_SWIZ_NONE);
}

static IMG_VOID ExpandPOW(PINTERMEDIATE_STATE	psState,
						  PUNIFLEX_INST			psOrigInst,
						  PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandPOW

 PURPOSE	: Expand a POW instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UF_REGFORMAT	eIntermediateFormat;
	PUNIFLEX_INST	psLOGInst;
	PUNIFLEX_INST	psMULInst;
	PUNIFLEX_INST	psEXPInst;
	IMG_UINT32		uTempReg = psState->uInputTempRegisterCount++;

	/* Intermediate results for U8 are calculated in C10 format. */
	if (psOrigInst->sDest.eFormat == UF_REGFORMAT_U8)
	{
		eIntermediateFormat = UF_REGFORMAT_C10;
	}
	else
	{
		eIntermediateFormat = psOrigInst->sDest.eFormat;
	}

	/*
		LOG		TEMP, ABS(SRC0)
	*/
	psLOGInst = AllocInputInst(psState, psProg, psOrigInst);
	psLOGInst->eOpCode = UFOP_LOG;
	psLOGInst->uPredicate = UF_PRED_NONE;

	BuildInputTempRegDest(&psLOGInst->sDest, uTempReg, eIntermediateFormat, psOrigInst->sDest.u.byMask);

	psLOGInst->asSrc[0] = psOrigInst->asSrc[0];
	psLOGInst->asSrc[0].byMod = (IMG_BYTE)(psLOGInst->asSrc[0].byMod & ~UFREG_SOURCE_NEGATE);
	psLOGInst->asSrc[0].byMod |= UFREG_SOURCE_ABS;

	/*
		MUL		TEMP, TEMP, SRC1
	*/
	psMULInst = AllocInputInst(psState, psProg, psOrigInst);
	psMULInst->eOpCode = UFOP_MUL;
	psMULInst->uPredicate = UF_PRED_NONE;

	BuildInputTempRegDest(&psMULInst->sDest, uTempReg, eIntermediateFormat, psOrigInst->sDest.u.byMask);
	BuildInputTempRegSrc(&psMULInst->asSrc[0], uTempReg, eIntermediateFormat, UFREG_SWIZ_RGBA);
	psMULInst->asSrc[1] = psOrigInst->asSrc[1];

	/*
		EXP DEST, TEMP
	*/
	psEXPInst = AllocInputInst(psState, psProg, psOrigInst);
	psEXPInst->eOpCode = UFOP_EXP;
	psEXPInst->uPredicate = psOrigInst->uPredicate;
	psEXPInst->sDest = psOrigInst->sDest;
	BuildInputTempRegSrc(&psEXPInst->asSrc[0], uTempReg, eIntermediateFormat, UFREG_SWIZ_RGBA);
}

static IMG_VOID ExpandSUB(PINTERMEDIATE_STATE	psState,
						  PUNIFLEX_INST			psOrigInst,
						  PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandSUB

 PURPOSE	: Expand a SUB instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;

	/*
		Convert
			SUB		D, A, B
		->
			ADD		D, A, -B
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	CopyInputInst(psInst, psOrigInst);
	psInst->eOpCode = UFOP_ADD;
	psInst->asSrc[1].byMod ^= UFREG_SOURCE_NEGATE;
}

static IMG_VOID ExpandAtomicSUB(PINTERMEDIATE_STATE	psState,
                                PUNIFLEX_INST       psOrigInst,
                                PINPUT_PROGRAM      psProg)
/*****************************************************************************
 FUNCTION	: ExpandAtomicSUB

 PURPOSE	: Expand a ATOM_SUB instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;

	/*
		Convert
			ATOM_SUB		D, A, B
		->
			ATOM_ADD		D, A, -B
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	CopyInputInst(psInst, psOrigInst);
	psInst->eOpCode = UFOP_ATOM_ADD;
	psInst->asSrc[1].byMod ^= UFREG_SOURCE_NEGATE;
}

static IMG_VOID LoadImmediateVector(PINTERMEDIATE_STATE	psState,
									PINPUT_PROGRAM		psProg,
									PUNIFLEX_INST		psOrigInst,
									IMG_UINT32			uDestNum,
									IMG_UINT32			uVectorLen,
									IMG_FLOAT const		afVector[])
/*****************************************************************************
 FUNCTION	: LoadImmediateVector

 PURPOSE	: Generates instruction to load a vector of immediate values
			  into a temporary register.

 PARAMETERS	: psState			- Compiler state.
			  psProg			- Program to add the expanded instruction to.
			  psOrigInst		- Original instruction.
			  uDestNum			- Index of the temporary register to write.
			  uVectorLen		- Number of immediate scalars to load.
			  afVector			- Points to the immediate values to load.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uChan;

	/* Unused if source line information is turned off. */
	PVR_UNREFERENCED_PARAMETER(psOrigInst);

	for (uChan = 0; uChan < uVectorLen; uChan++)
	{
		PUNIFLEX_INST	psMOVInst;

		/*
			MOV	DEST.chan, IMM[chan]
		*/
		psMOVInst = AllocInputInst(psState, psProg, psOrigInst);
		psMOVInst->eOpCode = UFOP_MOV;
		BuildInputTempRegDest(&psMOVInst->sDest, uDestNum, UF_REGFORMAT_F32, 1U << uChan);
		BuildInputFloatImmediateSrc(&psMOVInst->asSrc[0], afVector[uChan], UF_REGFORMAT_F32);
		psMOVInst->uPredicate = UF_PRED_NONE;
	}
}

static IMG_VOID MapAngleToMinusPiToPi(PINTERMEDIATE_STATE	psState,
									  PINPUT_PROGRAM		psProg,
									  PUNIFLEX_INST			psOrigInst,
									  IMG_UINT32			uTempReg,
									  IMG_UINT32			uDestMask)
/*****************************************************************************
 FUNCTION	: MapAngleToMinusPiToPi

 PURPOSE	: Generate input instructions to map an angle to -PI..PI.

 PARAMETERS	: psState			- Compiler state.
			  psProg			- Program to add the expanded instruction to.
			  psOrigInst		- Original instruction.
			  uTempReg			- Register containing the angle.
			  uDestMask			- Mask of channels to map.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;

	PVR_UNREFERENCED_PARAMETER(psOrigInst);

	/*
		MUL		TEMP, TEMP, 1/2PI
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MUL;
	BuildInputTempRegDest(&psInst->sDest, uTempReg, UF_REGFORMAT_F32, uDestMask);
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
	BuildInputImmediateSrc(&psInst->asSrc[1], 0x3E22F983 /* 1 / (2 * PI)*/, UF_REGFORMAT_F32);
	psInst->uPredicate = UF_PRED_NONE;

	/*
		ADD		TEMP, TEMP, 0.5
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_ADD;
	BuildInputTempRegDest(&psInst->sDest, uTempReg, UF_REGFORMAT_F32, uDestMask);
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
	BuildInputFloatImmediateSrc(&psInst->asSrc[1], 0.5f, UF_REGFORMAT_F32);
	psInst->uPredicate = UF_PRED_NONE;

	/*
		FRC		TEMP, TEMP
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_FRC;
	BuildInputTempRegDest(&psInst->sDest, uTempReg, UF_REGFORMAT_F32, uDestMask);
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
	psInst->uPredicate = UF_PRED_NONE;

	/*
		MAD		TEMP, TEMP, 2*PI, -PI
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MAD;
	BuildInputTempRegDest(&psInst->sDest, uTempReg, UF_REGFORMAT_F32, uDestMask);
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
	BuildInputFloatImmediateSrc(&psInst->asSrc[1], 6.283185307f /* 2 * PI */, UF_REGFORMAT_F32);
	BuildInputFloatImmediateSrc(&psInst->asSrc[2], 3.141592654f /* PI */, UF_REGFORMAT_F32);
	psInst->asSrc[2].byMod = UFREG_SOURCE_NEGATE;
	psInst->uPredicate = UF_PRED_NONE;
}

static IMG_VOID ExpandSINCOS2(PINTERMEDIATE_STATE	psState,
							  PUNIFLEX_INST			psOrigInst,
							  PINPUT_PROGRAM			psProg)
/*****************************************************************************
 FUNCTION	: ExpandSINCOS2

 PURPOSE	: Expand a SINCOS2 instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;

	#if defined(SUPPORT_SGX545)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_SQRT_SIN_COS)
	{
		/*
			If the hardware supports SIN and COS instructions directly then don't
			expand the instruction.
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psInst, psOrigInst);
		return;
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	{
		static const IMG_FLOAT afTaylorSeriesFirstTerms[] = 
		{
			0.000000000012232474581874708263740103575401008129119873046875f,	/* 1/(11! * 2048) */
			0.000000000269114452944307913639931939542293548583984375f,			/* 1/(10! * 1024) */
			0.000000005382288836841553347767330706119537353515625f,				/* 1/(9! * 512) */
			0.00000009688120172768321936018764972686767578125f,					/* 1/(8! * 256) */
		};	
		static const IMG_FLOAT afTaylorSeries[] = 
		{
			0.000001550099206f,		/* 1/(7!*128) */ 
			0.00002170138889f,		/* 1/(6!*64) */ 
			0.0002604166667f,		/* 1/(5!*32) */ 
			0.002604166667f,		/* 1/(4!*16) */
			0.0208333333f,			/* 1/(3!*8) */
			0.125f,					/* 1/(2!*4) */
			0.5f,					/* 0.5 */
			1.0f,					/* 1 */
		};
		IMG_UINT32	uTemp1, uTemp2, uTemp3, uTemp4;
		IMG_UINT32	uDestMask;
		IMG_UINT32	uIdx, uIter;

		uDestMask = psOrigInst->sDest.u.byMask | psOrigInst->sDest2.u.byMask;

		/*
			Get temporary registers for intermediate data.
		*/
		uTemp1 = psState->uInputTempRegisterCount++;
		uTemp2 = psState->uInputTempRegisterCount++;
		uTemp3 = psState->uInputTempRegisterCount++;
		uTemp4 = psState->uInputTempRegisterCount++;

		/* MOV	TEMP1, SRC1 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MOV;
		BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_F32, uDestMask);
		psInst->asSrc[0] = psOrigInst->asSrc[0];
		psInst->uPredicate = UF_PRED_NONE;

		/*
			Map the input angle to the range -PI..PI
		*/
		MapAngleToMinusPiToPi(psState, psProg, psOrigInst, uTemp1, uDestMask);

		/* MUL	TEMP2, TEMP1, TEMP1 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MUL;
		BuildInputTempRegDest(&psInst->sDest, uTemp2, UF_REGFORMAT_F32, uDestMask);
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		psInst->uPredicate = UF_PRED_NONE;

		for (uIdx = 0; uIdx < 2; uIdx++)
		{
			IMG_UINT32	uDest = (uIdx == 0) ? uTemp3 : uTemp4;

			/* 
				MAD TEMP3, TEMP2, -1/(11! * 2048), 1/(9! * 512) / 
				MAD TEMP4, TEMP2, -1/(10! * 1024), 1/(8! * 256) 
			*/
			psInst = AllocInputInst(psState, psProg, psOrigInst);
			psInst->eOpCode = UFOP_MAD;
			BuildInputTempRegDest(&psInst->sDest, uDest, UF_REGFORMAT_F32, uDestMask);
			BuildInputTempRegSrc(&psInst->asSrc[0], uTemp2, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
			BuildInputFloatImmediateSrc(&psInst->asSrc[1], afTaylorSeriesFirstTerms[0 + uIdx], UF_REGFORMAT_F32);
			psInst->asSrc[1].byMod = UFREG_SOURCE_NEGATE;
			BuildInputFloatImmediateSrc(&psInst->asSrc[2], afTaylorSeriesFirstTerms[2 + uIdx], UF_REGFORMAT_F32);
			psInst->uPredicate = UF_PRED_NONE;
		}

		for (uIter = 0; uIter < (sizeof(afTaylorSeries) / (sizeof(afTaylorSeries[0]) * 2)); uIter++)
		{
			for (uIdx = 0; uIdx < 2; uIdx++)
			{
				IMG_UINT32	uDest = (uIdx == 0) ? uTemp3 : uTemp4;

				/*
					MAD	TEMP2, TEMP2, TEMP1, CONST /
					MAD TEMP3, TEMP3, TEMP1, CONST
				*/
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				psInst->eOpCode = UFOP_MAD;
				BuildInputTempRegDest(&psInst->sDest, uDest, UF_REGFORMAT_F32, uDestMask);
				BuildInputTempRegSrc(&psInst->asSrc[0], uTemp2, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
				BuildInputTempRegSrc(&psInst->asSrc[1], uDest, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
				BuildInputImmediateSrc(&psInst->asSrc[2], *(const IMG_UINT32*)&afTaylorSeries[(uIter * 2) + uIdx], UF_REGFORMAT_F32);
				if ((uIter % 2) == 0)
				{
					psInst->asSrc[2].byMod = UFREG_SOURCE_NEGATE;
				}
				psInst->uPredicate = UF_PRED_NONE;
			}
		}

		/* MUL	TEMP3, TEMP3, TEMP1 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MUL;
		BuildInputTempRegDest(&psInst->sDest, uTemp3, UF_REGFORMAT_F32, uDestMask);
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp3, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		psInst->uPredicate = UF_PRED_NONE;

		/* MUL	TEMP4, TEMP4, TEMP3 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MUL;
		BuildInputTempRegDest(&psInst->sDest, uTemp4, UF_REGFORMAT_F32, uDestMask);
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp4, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp3, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		psInst->uPredicate = UF_PRED_NONE;

		/* MUL	TEMP3, TEMP3, TEMP3 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MUL;
		BuildInputTempRegDest(&psInst->sDest, uTemp3, UF_REGFORMAT_F32, uDestMask);
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp3, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp3, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		psInst->uPredicate = UF_PRED_NONE;

		/* MUL	TEMP3, TEMP3, TEMP3 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_ADD;
		BuildInputTempRegDest(&psInst->sDest, uTemp3, UF_REGFORMAT_F32, uDestMask);
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp3, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp3, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		psInst->uPredicate = UF_PRED_NONE;

		/* ADD	TEMP4, TEMP4, TEMP4 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_ADD;
		BuildInputTempRegDest(&psInst->sDest, uTemp4, UF_REGFORMAT_F32, uDestMask);
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp4, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp4, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		psInst->uPredicate = UF_PRED_NONE;

		/* ADD	TEMP3, -TEMP3, 1 */
		if (psOrigInst->sDest2.u.byMask)
		{
			psInst = AllocInputInst(psState, psProg, psOrigInst);
			psInst->eOpCode = UFOP_ADD;
			BuildInputTempRegDest(&psInst->sDest, uTemp3, UF_REGFORMAT_F32, psOrigInst->sDest2.u.byMask);
			BuildInputTempRegSrc(&psInst->asSrc[0], uTemp3, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
			psInst->asSrc[0].byMod = UFREG_SOURCE_NEGATE;
			BuildInputHwConstSrc(&psInst->asSrc[1], HW_CONST_1, UF_REGFORMAT_F32);
			psInst->uPredicate = UF_PRED_NONE;
		}

		/* MOV	DEST1, TEMP4 */
		if (psOrigInst->sDest.u.byMask != 0)
		{
			psInst = AllocInputInst(psState, psProg, psOrigInst);
			psInst->eOpCode = UFOP_MOV;
			psInst->sDest = psOrigInst->sDest;
			BuildInputTempRegSrc(&psInst->asSrc[0], uTemp4, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
			psInst->uPredicate = psOrigInst->uPredicate;
		}

		/* MOV	DEST2, TEMP3 */
		if (psOrigInst->sDest2.u.byMask != 0)
		{
			psInst = AllocInputInst(psState, psProg, psOrigInst);
			psInst->eOpCode = UFOP_MOV;
			psInst->sDest = psOrigInst->sDest2;
			BuildInputTempRegSrc(&psInst->asSrc[0], uTemp3, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
			psInst->uPredicate = psOrigInst->uPredicate;
		}
	}
}

static IMG_VOID ExpandSINCOS(PINTERMEDIATE_STATE	psState,
							 PUNIFLEX_INST			psOrigInst,
							 PINPUT_PROGRAM			psProg)
/*****************************************************************************
 FUNCTION	: ExpandSINCOS

 PURPOSE	: Expand a SINCOS instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;

	#if defined(SUPPORT_SGX545)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_SQRT_SIN_COS)
	{
		/*
			If the hardware supports SIN and COS instructions directly then don't
			expand the instruction.
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psInst, psOrigInst);
		return;
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	{
		static const IMG_FLOAT afTaylorSeries_Row1[] = 
		{
			0.000001550099206f,	/* 1/(7!*128) */ 
			0.00002170138889f,	/* 1/(6!*64) */ 
			0.0002604166667f,	/* 1/(5!*32) */ 
			0.002604166667f,	/* 1/(4!*16) */
		};
		static const IMG_FLOAT afTaylorSeries_Row2[] = 
		{
			0.0208333333f,		/* 1/(3!*8) */
			0.125f,				/* 1/(2!*4) */
			0.5f,				/* 0.5 */
			1.0f,				/* 1 */
		};
		IMG_UINT32	uTemp1, uTemp2, uTemp3;

		/*
			Get temporary registers for intermediate data.
		*/
		uTemp1 = psState->uInputTempRegisterCount++;
		uTemp2 = psState->uInputTempRegisterCount++;
		uTemp3 = psState->uInputTempRegisterCount++;

		/* MOV	TEMP1.w, SRC1.w */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MOV;
		BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_F32, 1 << UFREG_DMASK_W_SHIFT);
		psInst->asSrc[0] = psOrigInst->asSrc[0];
		psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_WWWW);
		psInst->uPredicate = UF_PRED_NONE;

		/* MUL	TEMP1.z, TEMP1.z, TEMP1.z */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MUL;
		BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_F32, 1 << UFREG_DMASK_Z_SHIFT);
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_WWWW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_WWWW);
		psInst->uPredicate = UF_PRED_NONE;

		/* MOV TEMP2, [1/(7!*128, 1/(6!*64)] */
		LoadImmediateVector(psState, psProg, psOrigInst, uTemp2, 2, afTaylorSeries_Row1 + 0);

		/* MOV TEMP3, [1/(5!*32), 1/(4!*16)] */
		LoadImmediateVector(psState, psProg, psOrigInst, uTemp3, 2, afTaylorSeries_Row1 + 2);

		/* MAD	TEMP1.xy, TEMP1.z, -TEMP2, TEMP3 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MAD;
		BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_F32, (1U << UFREG_DMASK_X_SHIFT) | (1U << UFREG_DMASK_Y_SHIFT));
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_ZZZZ);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp2, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		psInst->asSrc[1].byMod = UFREG_SOURCE_NEGATE;
		BuildInputTempRegSrc(&psInst->asSrc[2], uTemp3, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		psInst->uPredicate = UF_PRED_NONE;
	
		/* MOV TEMP2, [1/(3!*8), 1/(2!*4)] */
		LoadImmediateVector(psState, psProg, psOrigInst, uTemp2, 2, afTaylorSeries_Row2 + 0);

		/* MAD	TEMP1.xy, TEMP1.xy, TEMP1.z, -TEMP2 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MAD;
		BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_F32, (1U << UFREG_DMASK_X_SHIFT) | (1U << UFREG_DMASK_Y_SHIFT));
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_ZZZZ);
		BuildInputTempRegSrc(&psInst->asSrc[2], uTemp2, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		psInst->asSrc[2].byMod = UFREG_SOURCE_NEGATE;
		psInst->uPredicate = UF_PRED_NONE;

		/* MOV TEMP2, [0.5, 1] */
		LoadImmediateVector(psState, psProg, psOrigInst, uTemp2, 2, afTaylorSeries_Row2 + 2);

		/* MAD	TEMP1.xy, TEMP1.xy, TEMP1.z, TEMP2 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MAD;
		BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_F32, (1U << UFREG_DMASK_X_SHIFT) | (1U << UFREG_DMASK_Y_SHIFT));
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_ZZZZ);
		BuildInputTempRegSrc(&psInst->asSrc[2], uTemp2, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		psInst->uPredicate = UF_PRED_NONE;

		/* MUL	TEMP1.x, TEMP1.x, SRC1.w */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MUL;
		BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_F32, (1U << UFREG_DMASK_X_SHIFT));
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_WWWW);
		psInst->uPredicate = UF_PRED_NONE;

		/* MUL	TEMP.xy, TEMP.xy, TEMP.x */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MUL;
		BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_F32, (1U << UFREG_DMASK_X_SHIFT) | (1U << UFREG_DMASK_Y_SHIFT));
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_XXXX);
		psInst->uPredicate = UF_PRED_NONE;

		/* ADD	TEMP.xy, TEMP.xy, TEMP.xy */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_ADD;
		BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_F32, (1U << UFREG_DMASK_X_SHIFT) | (1U << UFREG_DMASK_Y_SHIFT));
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		psInst->uPredicate = UF_PRED_NONE;

		/* ADD	TEMP.x, -DEST.x, 1 */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_ADD;
		BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_F32, (1U << UFREG_DMASK_X_SHIFT));
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
		psInst->asSrc[0].byMod = UFREG_SOURCE_NEGATE;
		BuildInputHwConstSrc(&psInst->asSrc[1], HW_CONST_1, UF_REGFORMAT_F32);
		psInst->uPredicate = UF_PRED_NONE;

		/* MOV	DEST, TEMP */
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MOV;
		psInst->uPredicate = psOrigInst->uPredicate;
		psInst->sDest = psOrigInst->sDest;
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_F32, UFREG_SWIZ_XYZW);
	}
}

static IMG_VOID ExpandFloatDIV(PINTERMEDIATE_STATE	psState,
							   PUNIFLEX_INST		psOrigInst,
							   PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandFloatDIV

 PURPOSE	: Expand a DIV instruction on floating point data.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UF_REGFORMAT	eIntermediateFormat;
	IMG_UINT32		uTemp = psState->uInputTempRegisterCount++;
	PUNIFLEX_INST	psInst;

	/* Internal results for U8 is calculated in C10 format. */
	if (psOrigInst->asSrc[1].eFormat == UF_REGFORMAT_U8)
	{
		eIntermediateFormat = UF_REGFORMAT_C10;
	}
	else
	{
		eIntermediateFormat = psOrigInst->sDest.eFormat;
	}

	/*
		TEMP = 1 / SRC1
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_RECIP;
	BuildInputTempRegDest(&psInst->sDest, uTemp, eIntermediateFormat, psOrigInst->sDest.u.byMask);
	psInst->asSrc[0] = psOrigInst->asSrc[1];
	psInst->uPredicate = UF_PRED_NONE;

	/*
		DEST = SRC0 * TEMP(=1/SRC1)
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MUL;
	psInst->sDest = psOrigInst->sDest;
	psInst->uPredicate = psOrigInst->uPredicate;
	BuildInputTempRegSrc(&psInst->asSrc[0], uTemp, eIntermediateFormat, UFREG_SWIZ_RGBA);
	psInst->asSrc[1] = psOrigInst->asSrc[0];
}

static IMG_VOID ExpandInt16DIV(PINTERMEDIATE_STATE	psState,
							   PUNIFLEX_INST		psOrigInst,
							   PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandInt16DIV

 PURPOSE	: Expand a DIV instruction on 16-bit integer data.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uFDividend = psState->uInputTempRegisterCount++;
	IMG_UINT32	uFDivisor = psState->uInputTempRegisterCount++;
	IMG_UINT32	uRecip = psState->uInputTempRegisterCount++;
	IMG_UINT32	uFQuotient = psState->uInputTempRegisterCount++;
	IMG_UINT32	uFRCQuotient = psState->uInputTempRegisterCount++;
	IMG_UINT32	uFRemainder = psState->uInputTempRegisterCount++;
	IMG_UINT32	uU16EqualZ = psState->uInputTempRegisterCount++;
	IMG_UINT32	uU32EqualZ = psState->uInputTempRegisterCount++;
	IMG_UINT32	uURemainder = psState->uInputTempRegisterCount++;
	IMG_UINT32	uDestMask;
	IMG_UINT32	i;
	IMG_UINT32	uDest2Mask;

	/*
		No second destination on DIV.
	*/
	if (psOrigInst->eOpCode == UFOP_DIV)
	{
		uDest2Mask = 0;
	}
	else
	{
		ASSERT(psOrigInst->eOpCode == UFOP_DIV2);
		uDest2Mask = psOrigInst->sDest2.u.byMask;
	}

	/*
		Do the divide using the floating point reciprocal instruction.
	*/

	/*
		Combine the masks for both destinations.
	*/
	uDestMask = psOrigInst->sDest.u.byMask;
	if (psOrigInst->eOpCode == UFOP_DIV2)
	{
		uDestMask |= psOrigInst->sDest2.u.byMask;
	}

	/*
		SETBEQ	U16EQUALZ.X, DIVISOR == 0
	*/
	{
		PUNIFLEX_INST	psSetBInst;

		psSetBInst = AllocInputInst(psState, psProg, psOrigInst);
		psSetBInst->eOpCode = UFOP_SETBEQ;
		BuildInputTempRegDest(&psSetBInst->sDest, uU16EqualZ, UF_REGFORMAT_U16, uDestMask);
		psSetBInst->asSrc[0] = psOrigInst->asSrc[1];
		BuildInputImmediateSrc(&psSetBInst->asSrc[1], 0 /* uImmVal */, UF_REGFORMAT_U16);
		psSetBInst->uPredicate = UF_PRED_NONE;
	}

	/*
		U32EQUALZ = (UINT32)U16EQUALZ
	*/
	{
		PUNIFLEX_INST	psConvertInst;

		psConvertInst = AllocInputInst(psState, psProg, psOrigInst);
		psConvertInst->eOpCode = UFOP_MOV;
		BuildInputTempRegDest(&psConvertInst->sDest, uU32EqualZ, UF_REGFORMAT_U32, uDestMask);
		BuildInputTempRegSrc(&psConvertInst->asSrc[0], uU16EqualZ, UF_REGFORMAT_U16, UFREG_SWIZ_NONE);
		psConvertInst->uPredicate = UF_PRED_NONE;
	}

	/*	
		FDIVIDEND = (FLOAT)DIVIDEND
		FDIVISOR = (FLOAT)DIVISOR
	*/
	for (i = 0; i < 2; i++)
	{
		PUNIFLEX_INST	psConvertInst;
		IMG_UINT32		uDest = (i == 0) ? uFDividend : uFDivisor;

		psConvertInst = AllocInputInst(psState, psProg, psOrigInst);
		psConvertInst->eOpCode = UFOP_MOV;
		BuildInputTempRegDest(&psConvertInst->sDest, uDest, UF_REGFORMAT_F32, uDestMask);
		psConvertInst->asSrc[0] = psOrigInst->asSrc[i];
		psConvertInst->uPredicate = UF_PRED_NONE;
	}

	/*
		RECIP = 1 / FDIVISOR
	*/
	{
		PUNIFLEX_INST	psRecipInst;

		psRecipInst = AllocInputInst(psState, psProg, psOrigInst);
		psRecipInst->eOpCode = UFOP_RECIP;
		BuildInputTempRegDest(&psRecipInst->sDest, uRecip, UF_REGFORMAT_F32, uDestMask);
		BuildInputTempRegSrc(&psRecipInst->asSrc[0], uFDivisor, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		psRecipInst->uPredicate = UF_PRED_NONE;
	}

	/*
		FQUOTIENT = RECIP * FDIVIDEND
	*/
	{
		PUNIFLEX_INST	psMULInst;

		psMULInst = AllocInputInst(psState, psProg, psOrigInst);
		psMULInst->eOpCode = UFOP_MUL;
		BuildInputTempRegDest(&psMULInst->sDest, uFQuotient, UF_REGFORMAT_F32, uDestMask);
		BuildInputTempRegSrc(&psMULInst->asSrc[0], uRecip, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		BuildInputTempRegSrc(&psMULInst->asSrc[1], uFDividend, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		psMULInst->uPredicate = UF_PRED_NONE;
	}

	/*
		QUOTIENT = (INT)FQUOTIENT
	*/
	{
		PUNIFLEX_INST	psConvertInst;

		psConvertInst = AllocInputInst(psState, psProg, psOrigInst);
		psConvertInst->eOpCode = UFOP_FTOI;
		psConvertInst->sDest = psOrigInst->sDest;
		BuildInputTempRegSrc(&psConvertInst->asSrc[0], uFQuotient, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		psConvertInst->uPredicate = psOrigInst->uPredicate;
	}

	if (uDest2Mask != 0)
	{
		/*
			FRCQUOTIENT = FQUOTIENT - floor(FQUOTIENT)
		*/
		{
			PUNIFLEX_INST	psFRCInst;

			psFRCInst = AllocInputInst(psState, psProg, psOrigInst);
			psFRCInst->eOpCode = UFOP_FRC;
			BuildInputTempRegDest(&psFRCInst->sDest, uFRCQuotient, UF_REGFORMAT_F32, uDest2Mask);
			BuildInputTempRegSrc(&psFRCInst->asSrc[0], uFQuotient, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
			psFRCInst->uPredicate = UF_PRED_NONE;
		}

		/*
			FREMAINDER = FRCQUOTIENT * FDIVISOR;
		*/
		{
			PUNIFLEX_INST	psMULInst;

			psMULInst = AllocInputInst(psState, psProg, psOrigInst);
			psMULInst->eOpCode = UFOP_MUL;
			BuildInputTempRegDest(&psMULInst->sDest, uFRemainder, UF_REGFORMAT_F32, uDest2Mask);
			BuildInputTempRegSrc(&psMULInst->asSrc[0], uFRCQuotient, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
			BuildInputTempRegSrc(&psMULInst->asSrc[1], uFDivisor, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
			psMULInst->uPredicate = UF_PRED_NONE;
		}

		/*
			FREMAINDER = FREMAINDER + 0.5
		*/
		{
			PUNIFLEX_INST	psADDInst;

			psADDInst = AllocInputInst(psState, psProg, psOrigInst);
			psADDInst->eOpCode = UFOP_ADD;
			BuildInputTempRegDest(&psADDInst->sDest, uFRemainder, UF_REGFORMAT_F32, uDest2Mask);
			BuildInputTempRegSrc(&psADDInst->asSrc[0], uFRemainder, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
			BuildInputFloatImmediateSrc(&psADDInst->asSrc[1], 0.5 /* fImmVal */, UF_REGFORMAT_F32);
			psADDInst->uPredicate = UF_PRED_NONE;
		}

		/*	
			UREMAINDER = (INT)FREMAINDER
		*/
		{
			PUNIFLEX_INST	psConvertInst;

			psConvertInst = AllocInputInst(psState, psProg, psOrigInst);
			psConvertInst->eOpCode = UFOP_FTOI;
			BuildInputTempRegDest(&psConvertInst->sDest, uURemainder, UF_REGFORMAT_U16, uDest2Mask);
			BuildInputTempRegSrc(&psConvertInst->asSrc[0], uFRemainder, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
			psConvertInst->uPredicate = UF_PRED_NONE;
		}

		/*
			REMAINDER = (DIVISOR == 0) ? 0xFFFF : UREMAINDER
		*/
		{
			PUNIFLEX_INST	psMovcInst;

			psMovcInst = AllocInputInst(psState, psProg, psOrigInst);
			psMovcInst->eOpCode = UFOP_MOVCBIT;
			psMovcInst->sDest = psOrigInst->sDest2;
			psMovcInst->uPredicate = psOrigInst->uPredicate;
			BuildInputTempRegSrc(&psMovcInst->asSrc[0], uU32EqualZ, UF_REGFORMAT_U32, UFREG_SWIZ_NONE);
			BuildInputImmediateSrc(&psMovcInst->asSrc[1], 0xFFFF, UF_REGFORMAT_U16);
			BuildInputTempRegSrc(&psMovcInst->asSrc[2], uURemainder, UF_REGFORMAT_U16, UFREG_SWIZ_NONE);
		}
	}
}

static IMG_VOID ExpandIntegerDIV(PINTERMEDIATE_STATE	psState,
								 PUNIFLEX_INST			psOrigInst,
								 PINPUT_PROGRAM			psProg)
/*****************************************************************************
 FUNCTION	: ExpandIntegerDIV

 PURPOSE	: Expand a DIV instruction on integer data.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uTemp1;
	IMG_UINT32		uTemp2;
	IMG_UINT32		uQTemp;
	IMG_UINT32		uRTemp;
	IMG_UINT32		uTemp3;
	IMG_UINT32		uTemp4;
	IMG_UINT32		uTemp5;
	IMG_UINT32		uTemp6;
	PUNIFLEX_INST	psInst;
	IMG_UINT32		uDestMask;

	if (psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_U32)
	{
		/*
		  Integer divide - in DX10, this has two destinations
		  (quotient and remainder); hence we turn it into a DIV2
		  leave alone for icvt_i32.c
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psInst, psOrigInst);
		psInst->eOpCode = UFOP_DIV2;
		psInst->sDest2.u.byMask = 0;
		return;
	}
	
	ASSERT(psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_I32);

	/*
		Convert a signed divide to an unsigned divide.

		TEMP1 = ABS(DIVIDEND)
		TEMP2 = ABS(DIVISOR)
		QUOTIENT_TEMP = TEMP1 / TEMP2
		REMAINDER_TEMP = TEMP1 / TEMP2
		TEMP3 = SIGN(DIVIDEND) / SIGN(DIVISOR)
		QUOTIENT = QUOTIENT_TEMP * TEMP3
		REMAINDER = REMAINDER_TEMP * SIGN(DIVIDEND)
	*/

	/*
		Get temporary registers for intermediate data.
	*/
	uTemp1 = psState->uInputTempRegisterCount++;
	uTemp2 = psState->uInputTempRegisterCount++;
	uQTemp = psState->uInputTempRegisterCount++;
	uRTemp = psState->uInputTempRegisterCount++;
	uTemp3 = psState->uInputTempRegisterCount++;
	uTemp4 = psState->uInputTempRegisterCount++;
	uTemp5 = psState->uInputTempRegisterCount++;
	uTemp6 = psState->uInputTempRegisterCount++;

	/*
		Combine the masks for both destinations.
	*/
	uDestMask = psOrigInst->sDest.u.byMask;
	if (psOrigInst->eOpCode == UFOP_DIV2)
	{
		uDestMask |= psOrigInst->sDest2.u.byMask;
	}

	/*	
		MOV	TEMP1, ABS(DIVIDEND)
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOV;
	BuildInputTempRegDest(&psInst->sDest, uTemp1, UF_REGFORMAT_U32, uDestMask);
	psInst->asSrc[0] = psOrigInst->asSrc[0];
	psInst->uPredicate = UF_PRED_NONE;
	psInst->asSrc[0].byMod = (IMG_BYTE)(psInst->asSrc[0].byMod & ~UFREG_SOURCE_NEGATE);
	psInst->asSrc[0].byMod |= UFREG_SOURCE_ABS;

	/*
		MOV TEMP2, ABS(DIVISOR)
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOV;
	BuildInputTempRegDest(&psInst->sDest, uTemp2, UF_REGFORMAT_U32, uDestMask);
	psInst->asSrc[0] = psOrigInst->asSrc[1];
	psInst->uPredicate = UF_PRED_NONE;
	psInst->asSrc[0].byMod = (IMG_BYTE)(psInst->asSrc[0].byMod & ~UFREG_SOURCE_NEGATE);
	psInst->asSrc[0].byMod |= UFREG_SOURCE_ABS;

	/*
		DIV2 QTEMP+RTEMP, TEMP1, TEMP2
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_DIV2;
	BuildInputTempRegDest(&psInst->sDest, uQTemp, UF_REGFORMAT_U32, uDestMask);
	BuildInputTempRegDest(&psInst->sDest2, uRTemp, UF_REGFORMAT_U32, uDestMask);
	BuildInputTempRegSrc(&psInst->asSrc[0], uTemp1, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);
	BuildInputTempRegSrc(&psInst->asSrc[1], uTemp2, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);
	psInst->uPredicate = UF_PRED_NONE;

	/*
		Set TEMP3 to the sign bit of the QUOTIENT
	*/
	/*
		XOR	TEMP3, DIVIDEND, DIVISOR
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_XOR;
	BuildInputTempRegDest(&psInst->sDest, uTemp3, UF_REGFORMAT_U32, uDestMask);
	psInst->asSrc[0] = psOrigInst->asSrc[0];
	psInst->asSrc[1] = psOrigInst->asSrc[1];
	psInst->uPredicate = UF_PRED_NONE;

	/*
		AND TEMP3, TEMP3, 0x80000000
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_AND;
	BuildInputTempRegDest(&psInst->sDest, uTemp3, UF_REGFORMAT_U32, uDestMask);
	BuildInputTempRegSrc(&psInst->asSrc[0], uTemp3, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);
	BuildInputImmediateSrc(&psInst->asSrc[1], 0x80000000, UF_REGFORMAT_U32);
	psInst->uPredicate = UF_PRED_NONE;

	/*
		MOV	TEMP5, QTEMP.NEG
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOV;
	BuildInputTempRegDest(&psInst->sDest, uTemp5, UF_REGFORMAT_U32, uDestMask);
	BuildInputTempRegSrc(&psInst->asSrc[0], uQTemp, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);
	psInst->asSrc[0].byMod = UFREG_SOURCE_NEGATE;
	psInst->uPredicate = UF_PRED_NONE;

	/*
		MOVCNZBIT	ORIGDEST1, TEMP3, TEMP5, QTEMP
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOVCBIT;
	psInst->sDest = psOrigInst->sDest;
	psInst->uPredicate = psOrigInst->uPredicate;
	BuildInputTempRegSrc(&psInst->asSrc[0], uTemp3, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);
	BuildInputTempRegSrc(&psInst->asSrc[1], uTemp5, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);
	BuildInputTempRegSrc(&psInst->asSrc[2], uQTemp, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);

	if (g_asInputInstDesc[psOrigInst->eOpCode].uNumDests > 1)
	{
		/*
			Set TEMP4 to the sign bit of the REMAINDER.
		*/
		/*
			AND TEMP4, DIVISOR, 0x80000000
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_AND;
		BuildInputTempRegDest(&psInst->sDest, uTemp4, UF_REGFORMAT_U32, uDestMask);
		psInst->asSrc[0] = psOrigInst->asSrc[0];
		BuildInputImmediateSrc(&psInst->asSrc[1], 0x80000000, UF_REGFORMAT_U32);
		psInst->uPredicate = UF_PRED_NONE;

		/*
			MOV	TEMP6, RTEMP.NEG
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MOV;
		BuildInputTempRegDest(&psInst->sDest, uTemp6, UF_REGFORMAT_U32, uDestMask);
		BuildInputTempRegSrc(&psInst->asSrc[0], uRTemp, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);
		psInst->asSrc[0].byMod = UFREG_SOURCE_NEGATE;
		psInst->uPredicate = UF_PRED_NONE;

		/*
			MOVCNZBIT	ORIGDEST2, TEMP4, TEMP6, RTEMP
		*/
		psInst = AllocInputInst(psState, psProg, psOrigInst);
		psInst->eOpCode = UFOP_MOVCBIT;
		psInst->sDest = psOrigInst->sDest2;
		psInst->uPredicate = psOrigInst->uPredicate;
		BuildInputTempRegSrc(&psInst->asSrc[0], uTemp4, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);
		BuildInputTempRegSrc(&psInst->asSrc[1], uTemp6, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);
		BuildInputTempRegSrc(&psInst->asSrc[2], uRTemp, UF_REGFORMAT_U32, UFREG_SWIZ_RGBA);
	}
}

static
IMG_VOID GenerateFloor(PINTERMEDIATE_STATE	psState,
					   PINPUT_PROGRAM		psProg,
					   PUF_REGISTER			psDest,
					   PUF_REGISTER			psSrc,
					   IMG_UINT32			uPredicate,
					   PUNIFLEX_INST		psSrcLineInst)
/*****************************************************************************
 FUNCTION	: GenerateFloor

 PURPOSE	: Generate input instructions to do a floor calculation.

 PARAMETERS	: psState			- Compiler state.
			  psProg			- Program to add the expanded instruction to.
			  psDest			- Destination register for the floored value.
			  psSrc				- Value to floor.
			  uPredicate		- Predicate to control update of the destination.
			  psSrcLineInst		- Instruction to copy source line information
								from.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UF_REGFORMAT	eFormat = psDest->eFormat;
	IMG_UINT32		uTempReg = psState->uInputTempRegisterCount++;
	PUNIFLEX_INST	psInst;

	/*
		DEST = FLR(SRC)
	*/
	psInst = AllocInputInst(psState, psProg, psSrcLineInst);
	psInst->eOpCode = UFOP_FRC;
	BuildInputTempRegDest(&psInst->sDest, uTempReg, eFormat, psDest->u.byMask);
	psInst->asSrc[0] = *psSrc;
	psInst->uPredicate = UF_PRED_NONE;

	/*
		DEST = SRC - TEMP
	*/
	psInst = AllocInputInst(psState, psProg, psSrcLineInst);
	psInst->eOpCode = UFOP_ADD;
	psInst->sDest = *psDest;
	psInst->asSrc[0] = *psSrc;
	BuildInputTempRegSrc(&psInst->asSrc[1], uTempReg, eFormat, UFREG_SWIZ_NONE);
	psInst->asSrc[1].byMod = UFREG_SOURCE_NEGATE;
	psInst->uPredicate = uPredicate;
}

static IMG_VOID ExpandFLR(PINTERMEDIATE_STATE	psState, 
						  PUNIFLEX_INST			psOrigInst,
						  PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandFLR

 PURPOSE	: Expand a FLR instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	GenerateFloor(psState, psProg, &psOrigInst->sDest, &psOrigInst->asSrc[0], psOrigInst->uPredicate, psOrigInst);
}

static IMG_VOID ExpandEXPP(PINTERMEDIATE_STATE	psState,
						   PUNIFLEX_INST		psOrigInst,
						   PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandEXPP

 PURPOSE	: Expand a EXPP instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uChan;
	IMG_UINT32		uTempNum = psState->uInputTempRegisterCount++;
	UF_REGFORMAT	eTempFormat = psOrigInst->sDest.eFormat;
	PUNIFLEX_INST	psInst;

	for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
	{
		/*
			Skip channels not written by the instruction.
		*/
		if ((psOrigInst->sDest.u.byMask & (1U << uChan)) == 0)
		{
			continue;
		}

		switch (uChan)
		{
			case UFREG_SWIZ_X:
			{
				UF_REGISTER		sFlrSrc;
				UF_REGISTER		sFlrDest;

				/*
					TEMP.x = POW(2, FLR(SRC.W))
				*/
				/* TEMP.X = FLR(SRC.W) */
				sFlrSrc = psOrigInst->asSrc[0];
				sFlrSrc.u.uSwiz = RemapInputSwizzle(sFlrSrc.u.uSwiz, UFREG_SWIZ_AAAA);
				BuildInputTempRegDest(&sFlrDest, uTempNum, eTempFormat, 1U << uChan);
				GenerateFloor(psState, psProg, &sFlrDest, &sFlrSrc, UF_PRED_NONE, psOrigInst);

				/* TEMP.X = POW(2, TEMP.X) */
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				psInst->eOpCode = UFOP_EXP;
				BuildInputTempRegDest(&psInst->sDest, uTempNum, eTempFormat, 1U << uChan);
				BuildInputTempRegSrc(&psInst->asSrc[0], uTempNum, eTempFormat, UFREG_SWIZ_NONE);
				psInst->uPredicate = UF_PRED_NONE;
				break;
			}
			case UFREG_SWIZ_Y:
			{
				/*
					TEMP.y = FRC(SRC.W)
				*/
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				psInst->eOpCode = UFOP_FRC;
				BuildInputTempRegDest(&psInst->sDest, uTempNum, eTempFormat, 1U << uChan);
				psInst->asSrc[0] = psOrigInst->asSrc[0];
				psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_AAAA);
				psInst->uPredicate = UF_PRED_NONE;
				break;
			}
			case UFREG_SWIZ_Z:
			{
				/*
					TEMP.Z = POW(2, SRC.W)
				*/
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				psInst->eOpCode = UFOP_EXP;
				BuildInputTempRegDest(&psInst->sDest, uTempNum, eTempFormat, 1U << uChan);
				psInst->asSrc[0] = psOrigInst->asSrc[0];
				psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_AAAA);
				psInst->uPredicate = UF_PRED_NONE;
				break;
			}
			case UFREG_SWIZ_W:
			{
				/*
					TEMP.W = 1
				*/
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				psInst->eOpCode = UFOP_MOV;
				BuildInputTempRegDest(&psInst->sDest, uTempNum, eTempFormat, 1U << uChan);
				BuildInputHwConstSrc(&psInst->asSrc[0], HW_CONST_1, eTempFormat);
				psInst->uPredicate = UF_PRED_NONE;
				break;
			}
		}
	}

	/*
		MOV	DEST, TEMP
	*/
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOV;
	psInst->sDest = psOrigInst->sDest;
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempNum, eTempFormat, UFREG_SWIZ_NONE);
	psInst->uPredicate = psOrigInst->uPredicate;
}

static IMG_VOID ExpandMOVA_TRC(PINTERMEDIATE_STATE	psState, 
							   PUNIFLEX_INST		psOrigInst,
							   PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandMOVA_TRC

 PURPOSE	: Expand a MOVA_TRC instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uTempReg = psState->uInputTempRegisterCount++;
	UF_REGFORMAT	eTempFormat = psOrigInst->asSrc[0].eFormat;
	UF_REGISTER		sDest;
	PUNIFLEX_INST	psInst;

	/*
		MOVA_TRC	DEST, SRC
			->
		FLR			TEMP, DEST
		MOVA		DEST, TEMP
	*/
	
	BuildInputTempRegDest(&sDest, uTempReg, eTempFormat, psOrigInst->sDest.u.byMask);
	GenerateFloor(psState, psProg, &sDest, &psOrigInst->asSrc[0], UF_PRED_NONE, psOrigInst);

	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_MOVA_RNE;
	psInst->sDest = psOrigInst->sDest;
	psInst->uPredicate = psOrigInst->uPredicate;
	BuildInputTempRegSrc(&psInst->asSrc[0], uTempReg, eTempFormat, UFREG_SWIZ_NONE);
}

static IMG_VOID ExpandSGN_Int(PINTERMEDIATE_STATE	psState, 
							  PUNIFLEX_INST			psOrigInst,
							  PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandSGN_Int

 PURPOSE	: Expand a SGN instruction on I32 or U32 format data.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psSETBLTInst;
	PUNIFLEX_INST	psMOVCInst;
	PUNIFLEX_INST	psSETBEQInst;
	IMG_UINT32		uTempReg1, uTempReg2, uTempReg3;
	UF_REGFORMAT	eTempFormat = psOrigInst->sDest.eFormat;

	/*
		SGN		D, S

		->

		SETBLT		T1, S, 0
		MOVCNZBIT	T2, T1, -1, 1
		SETBEQ		T3, S, 0
		MOVCNZBIT	D, T3, 0, T2

		S<0
			T1 = TRUE
			T2 = -1
			T3 = FALSE
			D = T2 = -1
		S=0
			T1 = FALSE
			T2 = 1
			T3 = TRUE
			D = 0
		S>0
			T1 = FALSE
			T2 = 1
			T3 = FALSE
			D = T2 = 1
	*/

	/*
		Get temporary registers for the intermediate result in
		the expansion.
	*/
	uTempReg1 = psState->uInputTempRegisterCount++;
	uTempReg2 = psState->uInputTempRegisterCount++;
	uTempReg3 = psState->uInputTempRegisterCount++;

	/* SETBLT		T1, S, 0 */
	psSETBLTInst = AllocInputInst(psState, psProg, psOrigInst);
	if (psOrigInst->asSrc[0].byMod & UFREG_SOURCE_NEGATE)
	{
		psSETBLTInst->eOpCode = UFOP_SETBGE;
	}
	else
	{
		psSETBLTInst->eOpCode = UFOP_SETBLT;
	}
	BuildInputTempRegDest(&psSETBLTInst->sDest, uTempReg1, eTempFormat, psOrigInst->sDest.u.byMask);
	psSETBLTInst->asSrc[0] = psOrigInst->asSrc[0];
	psSETBLTInst->asSrc[0].byMod = (IMG_BYTE)(psSETBLTInst->asSrc[0].byMod & ~UFREG_SOURCE_NEGATE);
	BuildInputImmediateSrc(&psSETBLTInst->asSrc[1], 0, eTempFormat);
	psSETBLTInst->uPredicate = UF_PRED_NONE;

	/* MOVCNZBIT	T2, T1, -1, 1 */
	psMOVCInst = AllocInputInst(psState, psProg, psOrigInst);
	psMOVCInst->eOpCode = UFOP_MOVCBIT;
	BuildInputTempRegDest(&psMOVCInst->sDest, uTempReg2, eTempFormat, psOrigInst->sDest.u.byMask);
	BuildInputTempRegSrc(&psMOVCInst->asSrc[0], uTempReg1, eTempFormat, UFREG_SWIZ_NONE);
	BuildInputImmediateSrc(&psMOVCInst->asSrc[1], UINT_MAX, eTempFormat);
	BuildInputImmediateSrc(&psMOVCInst->asSrc[2], 1, eTempFormat);
	psMOVCInst->uPredicate = UF_PRED_NONE;

	/* SETBEQ		T3, S, 0 */
	psSETBEQInst = AllocInputInst(psState, psProg, psOrigInst);
	psSETBEQInst->eOpCode = UFOP_SETBEQ;
	BuildInputTempRegDest(&psSETBEQInst->sDest, uTempReg3, eTempFormat, psOrigInst->sDest.u.byMask);
	psSETBEQInst->asSrc[0] = psOrigInst->asSrc[0];
	psSETBEQInst->asSrc[0].byMod = (IMG_BYTE)(psSETBEQInst->asSrc[0].byMod & ~UFREG_SOURCE_NEGATE);
	BuildInputImmediateSrc(&psSETBEQInst->asSrc[1], 0, eTempFormat);
	psSETBEQInst->uPredicate = UF_PRED_NONE;

	/* MOVCNZBIT	D, T3, 0, T2 */
	psMOVCInst = AllocInputInst(psState, psProg, psOrigInst);
	psMOVCInst->eOpCode = UFOP_MOVCBIT;
	psMOVCInst->sDest = psOrigInst->sDest;
	BuildInputTempRegSrc(&psMOVCInst->asSrc[0], uTempReg3, eTempFormat, UFREG_SWIZ_NONE);
	BuildInputImmediateSrc(&psMOVCInst->asSrc[1], 0, eTempFormat);
	BuildInputTempRegSrc(&psMOVCInst->asSrc[2], uTempReg2, eTempFormat, UFREG_SWIZ_NONE);
	psMOVCInst->uPredicate = psOrigInst->uPredicate;
}

static IMG_VOID ExpandSGN(PINTERMEDIATE_STATE	psState, 
						  PUNIFLEX_INST			psOrigInst,
						  PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandSGN

 PURPOSE	: Expand a SGN instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;
	IMG_UINT32		uTempReg;

	if (psOrigInst->sDest.eFormat == UF_REGFORMAT_U32 ||
		psOrigInst->sDest.eFormat == UF_REGFORMAT_I32)
	{
		ExpandSGN_Int(psState, psOrigInst, psProg);
		return;
	}

	/*
		Get a temporary register for the intermediate result in
		the expansion.
	*/
	uTempReg = psState->uInputTempRegisterCount++;

	/* CMP T, A, 0, -1 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_CMP;
	BuildInputTempRegDest(&psInst->sDest, uTempReg, psOrigInst->sDest.eFormat, psOrigInst->sDest.u.byMask);
	psInst->asSrc[0] = psOrigInst->asSrc[0];
	BuildInputHwConstSrc(&psInst->asSrc[1], HW_CONST_0, psOrigInst->sDest.eFormat);
	BuildInputHwConstSrc(&psInst->asSrc[2], HW_CONST_1, psOrigInst->sDest.eFormat);
	psInst->asSrc[2].byMod = UFREG_SOURCE_NEGATE;
	psInst->uPredicate = UF_PRED_NONE;

	/* CMP D, -A, T, 1 */
	psInst = AllocInputInst(psState, psProg, psOrigInst);
	psInst->eOpCode = UFOP_CMP;
	psInst->sDest = psOrigInst->sDest;
	psInst->asSrc[0] = psOrigInst->asSrc[0];
	psInst->asSrc[0].byMod ^= UFREG_SOURCE_NEGATE;
	BuildInputTempRegSrc(&psInst->asSrc[1], uTempReg, psOrigInst->sDest.eFormat, UFREG_SWIZ_NONE);
	BuildInputHwConstSrc(&psInst->asSrc[2], HW_CONST_1, psOrigInst->sDest.eFormat);
	psInst->uPredicate = UF_PRED_NONE;
}

static IMG_VOID ExpandSQRT(PINTERMEDIATE_STATE psState, PUNIFLEX_INST psOrigInst, PINPUT_PROGRAM psProg)
/*****************************************************************************
 FUNCTION	: ExpandSGRT

 PURPOSE	: Expand a SQRT instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	#if defined(SUPPORT_SGX545)
	/*
		Expand to a RECIP/RSQRT sequence unless the hardware has a SQRT instruction.
	*/
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_SQRT_SIN_COS)
	{
		PUNIFLEX_INST	psInst;

		psInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psInst, psOrigInst);
	}
	else
	#endif /* defined(SUPPORT_SGX545) */
	{
		UF_REGFORMAT	eIntermediateFormat;
		IMG_UINT32		uTempReg;
		PUNIFLEX_INST	psRSQRTInst;
		PUNIFLEX_INST	psRCPInst;
		IMG_BOOL		bChangedFormat;
		IMG_UINT32		uConvertedSourceReg = USC_UNDEF;
		IMG_UINT32		uConvertedDestReg = USC_UNDEF;

		/*
			Get a temporary register for the intermediate result in
			the expansion.
		*/
		uTempReg = psState->uInputTempRegisterCount++;

		/*
			Use F16 precision for intermediate results for C10/U8 SQRT to avoid 
			out of range value.
		*/
		if (psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_U8 ||
			psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_C10)
		{
			eIntermediateFormat = UF_REGFORMAT_F16;
			bChangedFormat = IMG_TRUE;

			uConvertedSourceReg = psState->uInputTempRegisterCount++;
			uConvertedDestReg = psState->uInputTempRegisterCount++;
		}
		else
		{
			eIntermediateFormat = psOrigInst->asSrc[0].eFormat;
			bChangedFormat = IMG_FALSE;
		}

		/*
			If we are using a higher precision intermediate format then
			convert the source to that format.
		*/
		if (bChangedFormat)
		{
			PUNIFLEX_INST	psMOVInst;

			psMOVInst = AllocInputInst(psState, psProg, psOrigInst);
			psMOVInst->eOpCode = UFOP_MOV;
			BuildInputTempRegDest(&psMOVInst->sDest, uConvertedSourceReg, eIntermediateFormat, psOrigInst->sDest.u.byMask);
			psMOVInst->asSrc[0] = psOrigInst->asSrc[0];
			psMOVInst->uPredicate = UF_PRED_NONE;
		}

		/*
			TEMP = 1 / RSQRT(SRC1)
		*/
		psRSQRTInst = AllocInputInst(psState, psProg, psOrigInst);
		psRSQRTInst->eOpCode = UFOP_RSQRT;
		BuildInputTempRegDest(&psRSQRTInst->sDest, uTempReg, eIntermediateFormat, psOrigInst->sDest.u.byMask);
		if (bChangedFormat)
		{
			BuildInputTempRegSrc(&psRSQRTInst->asSrc[0], uConvertedSourceReg, eIntermediateFormat, UFREG_SWIZ_NONE);
		}
		else
		{
			psRSQRTInst->asSrc[0] = psOrigInst->asSrc[0];
		}
		psRSQRTInst->uPredicate = UF_PRED_NONE;

		/*
			DEST = 1/TEMP
		*/
		psRCPInst = AllocInputInst(psState, psProg, psOrigInst);
		psRCPInst->eOpCode = UFOP_RECIP;
		if (bChangedFormat)
		{
			BuildInputTempRegDest(&psRCPInst->sDest, uConvertedDestReg, eIntermediateFormat, psOrigInst->sDest.u.byMask);
			psRCPInst->uPredicate = UF_PRED_NONE;
		}
		else
		{
			psRCPInst->sDest = psOrigInst->sDest;
			psRCPInst->uPredicate = psOrigInst->uPredicate;
		}
		BuildInputTempRegSrc(&psRCPInst->asSrc[0], uTempReg, eIntermediateFormat, UFREG_SWIZ_NONE);

		/*
			Convert a higher precision result back to the original destination format.
		*/
		if (bChangedFormat)
		{
			PUNIFLEX_INST	psMOVInst;

			psMOVInst = AllocInputInst(psState, psProg, psOrigInst);
			psMOVInst->eOpCode = UFOP_MOV;
			psMOVInst->sDest = psOrigInst->sDest;
			BuildInputTempRegSrc(&psMOVInst->asSrc[0], uConvertedDestReg, eIntermediateFormat, UFREG_SWIZ_NONE);
			psMOVInst->uPredicate = psOrigInst->uPredicate;
		}
	}
}

static IMG_VOID ExpandMOV(PINTERMEDIATE_STATE psState, PUNIFLEX_INST psOrigInst, PINPUT_PROGRAM psProg)
/*****************************************************************************
 FUNCTION	: ExpandMOV

 PURPOSE	: Expand a MOV instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UF_REGFORMAT	eSourceFormat = GetRegisterFormat(psState, &psOrigInst->asSrc[0]);
	UF_REGFORMAT	eDestFormat = GetRegisterFormat(psState, &psOrigInst->sDest);

	if (
			eSourceFormat == UF_REGFORMAT_F32 &&
			(
				eDestFormat == UF_REGFORMAT_I32			||
				eDestFormat == UF_REGFORMAT_U32			||
				eDestFormat == UF_REGFORMAT_I16			||
				eDestFormat == UF_REGFORMAT_U16			||
				eDestFormat == UF_REGFORMAT_I8_UN		||
				eDestFormat == UF_REGFORMAT_U8_UN
			)
		)
	{
		IMG_UINT32		uRoundedTempReg;
		PUNIFLEX_INST	psMOVInst;
		UF_REGISTER		sFlrDest;

		/*
			Get a temporary register to hold the source rounded towards zero.
		*/
		uRoundedTempReg = psState->uInputTempRegisterCount++;
		BuildInputTempRegDest(&sFlrDest, uRoundedTempReg, eSourceFormat, psOrigInst->sDest.u.byMask);

		if (eDestFormat == UF_REGFORMAT_U32 || eDestFormat == UF_REGFORMAT_U16 || eDestFormat == UF_REGFORMAT_U8_UN)
		{
			/*
				Round towards negative infinity (since source values less than zero will be
				clamped to zero away).
			*/
			GenerateFloor(psState, psProg, &sFlrDest, &psOrigInst->asSrc[0], psOrigInst->uPredicate, psOrigInst);
		}
		else
		{
			PUNIFLEX_INST	psRoundInst;

			/*
				Insert an instruction to round the source argument towards zero.
			*/
			psRoundInst = AllocInputInst(psState, psProg, psOrigInst);
			psRoundInst->eOpCode = UFOP_TRC;
			psRoundInst->sDest = sFlrDest;
			psRoundInst->asSrc[0] = psOrigInst->asSrc[0];
			psRoundInst->uPredicate = psOrigInst->uPredicate;
		}

		/*
			Insert an instruction to convert the source argument to integer.
		*/
		psMOVInst = AllocInputInst(psState, psProg, psOrigInst);
		psMOVInst->eOpCode = UFOP_FTOI;
		psMOVInst->sDest = psOrigInst->sDest;
		BuildInputTempRegSrc(&psMOVInst->asSrc[0], uRoundedTempReg, eSourceFormat, UFREG_SWIZ_NONE);
		psMOVInst->uPredicate = psOrigInst->uPredicate;
	}
	else
	{
		PUNIFLEX_INST	psInst;

		psInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psInst, psOrigInst);
	}
}

static IMG_VOID ExpandREINTERP(PINTERMEDIATE_STATE psState, PUNIFLEX_INST psOrigInst, PINPUT_PROGRAM psProg)
/*****************************************************************************
 FUNCTION	: ExpandREINTERP

 PURPOSE	: Expand a REINTERP instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Original instruction.
			  psProg			- Program to add the expanded instruction to.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UF_REGFORMAT	eDestFormat = GetRegisterFormat(psState, &psOrigInst->sDest);
	UF_REGFORMAT	eSourceFormat = GetRegisterFormat(psState, &psOrigInst->asSrc[0]);
	PUNIFLEX_INST	psMOVInst;
	
	/*
		Insert an instruction to convert the source argument to integer.
	*/
	psMOVInst = AllocInputInst(psState, psProg, psOrigInst);
	psMOVInst->eOpCode = UFOP_MOV;
	psMOVInst->sDest = psOrigInst->sDest;
	psMOVInst->asSrc[0] = psOrigInst->asSrc[0];
	psMOVInst->uPredicate = psOrigInst->uPredicate;
	if	(
			eDestFormat == UF_REGFORMAT_U32 ||
			eDestFormat == UF_REGFORMAT_I32
		)
	{
		ASSERT((eSourceFormat == UF_REGFORMAT_F32) || (eSourceFormat == UF_REGFORMAT_F16) || (eSourceFormat == UF_REGFORMAT_C10));
		psMOVInst->sDest.eFormat = UF_REGFORMAT_F32;
	}
	else
	{
		ASSERT((eDestFormat == UF_REGFORMAT_F32) || (eDestFormat == UF_REGFORMAT_F16) || (eDestFormat == UF_REGFORMAT_C10));
		ASSERT((eSourceFormat == UF_REGFORMAT_U32) || (eSourceFormat == UF_REGFORMAT_I32));
		psMOVInst->asSrc[0].eFormat = UF_REGFORMAT_F32;
	}
}

static
IMG_VOID MakeLRP(PINTERMEDIATE_STATE	psState,
				 PINPUT_PROGRAM			psProg,
				 PUNIFLEX_INST			psSrcLineInst,
				 PUF_REGISTER			psDest,
				 IMG_UINT32				uPredicate,
				 PUF_REGISTER			psSrcA,
				 PUF_REGISTER			psSrcB,
				 PUF_REGISTER			psSrcC)
/*****************************************************************************
 FUNCTION	: MakeLRP

 PURPOSE	: Expand a linear interpolate input instruction.

 PARAMETERS	: psState			- Compiler state.
			  psProg			- The new instructions are appended to this
								program.
			  psSrcLineInst		- Source for the associated source program line.
			  psDest			- Destination for the linear interpolate.
			  psSrcA			- Values to interpolate between.
			  psSrcB
			  psSrcC			- Interpolant.
	
 RETURNS	: ANothing.
*****************************************************************************/
{
	IMG_UINT32		uTempReg = psState->uInputTempRegisterCount++;
	PUNIFLEX_INST	psInst;

	psInst = AllocInputInst(psState, psProg, psSrcLineInst);
	psInst->eOpCode = UFOP_MAD;
	BuildInputTempRegDest(&psInst->sDest, uTempReg, psDest->eFormat, psDest->u.byMask);
	psInst->asSrc[0] = *psSrcA;
	psInst->asSrc[1] = *psSrcC;
	psInst->asSrc[2] = *psSrcB;
	psInst->uPredicate = UF_PRED_NONE;

	psInst = AllocInputInst(psState, psProg, psSrcLineInst);
	psInst->eOpCode = UFOP_MAD;
	psInst->sDest = *psDest;
	psInst->asSrc[0] = *psSrcB;
	if (psInst->asSrc[0].byMod & UFREG_SOURCE_NEGATE)
	{
		psInst->asSrc[0].byMod = (IMG_BYTE)(psInst->asSrc[0].byMod & ~UFREG_SOURCE_NEGATE);
	}
	else
	{
		psInst->asSrc[0].byMod |= UFREG_SOURCE_NEGATE;
	}
	psInst->asSrc[1] = *psSrcC;
	BuildInputTempRegSrc(&psInst->asSrc[2], uTempReg, psDest->eFormat, UFREG_SWIZ_NONE);
	psInst->uPredicate = uPredicate;
}

#if defined(OUTPUT_USCHW)
static 
IMG_VOID GetTextureDimensionConstant(PINTERMEDIATE_STATE	psState, 
								     IMG_UINT32				uSampler, 
								     IMG_UINT32				uDimIdx,
								     PUF_REGISTER			psDimensionConstant)
/*****************************************************************************
 FUNCTION	: GetTextureDimensionConstant

 PURPOSE	: Create an input source argument structure for the constant 
			  containing the size of a dimension of a texture.

 PARAMETERS	: psState			- Compiler state.
			  uSamplerIdx		- Index of the sampler.
			  uDimIdx			- Index of the dimension.
			  psDimensionConstant
								- Returns the details of the constant.
	
 RETURNS	: ANothing.
*****************************************************************************/
{
	IMG_UINT32				uTexSizeOffset;
	IMG_UINT32				uConstNum;
	IMG_UINT32				uConstChan;
	static const IMG_UINT32	auInputReplicateSwizzle[] =
	{
		UFREG_SWIZ_XXXX,
		UFREG_SWIZ_YYYY,
		UFREG_SWIZ_ZZZZ,
		UFREG_SWIZ_WWWW,
	};

	uTexSizeOffset = _UNIFLEX_TEXSTATE_SAMPLERBASE(uSampler, psState->uTexStateSize);
	switch (uDimIdx)
	{
		case 0: uTexSizeOffset += _UNIFLEX_TEXSTATE_WIDTH(psState->uTexStateSize); break;
		case 1: uTexSizeOffset += _UNIFLEX_TEXSTATE_HEIGHT(psState->uTexStateSize); break;
		case 2: uTexSizeOffset += _UNIFLEX_TEXSTATE_DEPTH(psState->uTexStateSize); break;
		default: imgabort();
	}

	/*
		Offset by the start of texture state within the constant buffer.
	*/
	uTexSizeOffset += psState->psSAOffsets->uTextureStateConstOffset;

	uConstNum = uTexSizeOffset / CHANNELS_PER_INPUT_REGISTER;
	uConstChan = uTexSizeOffset % CHANNELS_PER_INPUT_REGISTER;

	BuildInputSrc(psDimensionConstant,
				  UFREG_TYPE_CONST,
				  uConstNum,
				  UF_REGFORMAT_F32,
				  auInputReplicateSwizzle[uConstChan]);
	psDimensionConstant->uArrayTag = psState->uTextureStateConstsBuffer;
}

static IMG_UINT32 OffsetSampleCoordinates(PINTERMEDIATE_STATE	psState,
										  PINPUT_PROGRAM		psProg,
										  PUNIFLEX_INST			psOrigInst,
										  IMG_UINT32			uDimensionMask,
										  IMG_UINT32			uImmediateOffsetsTempReg,
										  PUF_REGISTER			psF32CoordSrc)
/*****************************************************************************
 FUNCTION	: OffsetSampleCoordinates

 PURPOSE	: Generate instructions to add a set of immediate, texel space
			  offsets onto texture sample coordinates.

 PARAMETERS	: psState			- Compiler state.
			  psProg			- The new instructions are appended to this
								program.
			  psOrigInst		- Original texture sample instruction.
			  uImmediateOffsetsTempReg
								- Temporary register containing the immediate
								offsets.
			  psF32CoordSrc		- Texture sample coordinates.
	
 RETURNS	: ANothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psADDInst;
	IMG_UINT32		uOffsetCoordinateTempReg = psState->uInputTempRegisterCount++;
	
	/*
		Add the scaled offsets on to the supplied coordinates.
	*/
	psADDInst = AllocInputInst(psState, psProg, psOrigInst);
	psADDInst->eOpCode = UFOP_ADD;
	BuildInputTempRegDest(&psADDInst->sDest, uOffsetCoordinateTempReg, UF_REGFORMAT_F32, uDimensionMask);
	BuildInputTempRegDest(&psADDInst->asSrc[0], uImmediateOffsetsTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
	psADDInst->asSrc[1] = *psF32CoordSrc;
	psADDInst->uPredicate = UF_PRED_NONE;

	return uOffsetCoordinateTempReg;
}

static IMG_VOID SampleWithOffsetsAtMipMapLevel(PINTERMEDIATE_STATE	psState,
											   PINPUT_PROGRAM		psProg,
											   PUNIFLEX_INST		psOrigInst,
											   IMG_UINT32			uDimensionMask,
											   PUF_REGISTER			psSampleDest,
											   IMG_UINT32			uSamplePredicate,
											   IMG_UINT32			uScaledOffsets,
											   IMG_UINT32			uMipMapLevel,
											   PUF_REGISTER			psF32CoordSrc)
/*****************************************************************************
 FUNCTION	: SampleWithOffsetsAtMipMapLevel

 PURPOSE	: Generate instructions to sample with texel space immediate offsets
			  at a particular mipmap level.

 PARAMETERS	: psState			- Compiler state.
			  psProg			- The new instructions are appended to this
								program.
			  psOrigInst		- Original texture sample instruction.
			  uDimensionMask	- Destination write-mask corresponding to
								the dimensionality of the sampled texture.
			  psSampleDest		- Destination for the sample result.
			  uSamplePredicate	- Predicate for the sample instruction.
			  uScaledOffsets	- Temporary register containing the texel space 
								immediate offsets scaled by the size of the top-map level.
			  uMipMapLevel		- Temporary register containing the mip-map level
								to sample.
			  psF32CoordSrc		- Texture sample coordinates.
	
 RETURNS	: ANothing.
*****************************************************************************/
{
	IMG_UINT32		uLevelScaledOffsets = psState->uInputTempRegisterCount++;
	IMG_UINT32		uLODLevelScale = psState->uInputTempRegisterCount++;
	PUNIFLEX_INST	psEXPInst;
	PUNIFLEX_INST	psMULInst;
	PUNIFLEX_INST	psOffsetSampleInst;
	IMG_UINT32		uOffsetCoordinateTempReg;

	/*	
		Calculate:
			POW(2, Sampled mip-map level)
	*/
	psEXPInst = AllocInputInst(psState, psProg, psOrigInst);
	psEXPInst->eOpCode = UFOP_EXP;
	BuildInputTempRegDest(&psEXPInst->sDest, uLODLevelScale, UF_REGFORMAT_F32, 1 << UFREG_DMASK_X_SHIFT);
	BuildInputTempRegSrc(&psEXPInst->asSrc[0], uMipMapLevel, UF_REGFORMAT_F32, UFREG_SWIZ_XXXX);
	psEXPInst->uPredicate = UF_PRED_NONE;

	/*
		Multiply the scaled immediate offsets by POW(2, Sampled mip-map level).
	*/
	psMULInst = AllocInputInst(psState, psProg, psOrigInst);
	psMULInst->eOpCode = UFOP_MUL;
	BuildInputTempRegDest(&psMULInst->sDest, uLevelScaledOffsets, UF_REGFORMAT_F32, uDimensionMask);
	BuildInputTempRegSrc(&psMULInst->asSrc[0], uLODLevelScale, UF_REGFORMAT_F32, UFREG_SWIZ_XXXX);
	BuildInputTempRegSrc(&psMULInst->asSrc[1], uScaledOffsets, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
	psMULInst->uPredicate = UF_PRED_NONE;

	/*
		Add the scaled immediate offsets onto the original texture coordinates.
	*/
	uOffsetCoordinateTempReg = 
		OffsetSampleCoordinates(psState,
								psProg,
								psOrigInst,
								uDimensionMask,
								uLevelScaledOffsets,
								psF32CoordSrc);

	/*
		Create a copy of the original texture sample instruction using the offset coordinates and sampling
		at a specified mipmap level.
	*/
	psOffsetSampleInst = AllocInputInst(psState, psProg, psOrigInst);
	CopyInputInst(psOffsetSampleInst, psOrigInst);
	psOffsetSampleInst->eOpCode = UFOP_LDL;
	psOffsetSampleInst->sDest = *psSampleDest;
	psOffsetSampleInst->uPredicate = uSamplePredicate;
	BuildInputTempRegSrc(&psOffsetSampleInst->asSrc[0], uOffsetCoordinateTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
	BuildInputTempRegSrc(&psOffsetSampleInst->asSrc[2], uMipMapLevel, UF_REGFORMAT_F32, UFREG_SWIZ_XXXX);
	BuildInputImmediateSrc(&psOffsetSampleInst->asSrc[3], 0 /* uImmVal */, UF_REGFORMAT_U32);
}

static IMG_BOOL ExpandLDWithImmediateOffsets(PINTERMEDIATE_STATE	psState, 
											 PINPUT_PROGRAM			psProg, 
											 PUNIFLEX_INST			psOrigInst,
											 PUF_REGISTER			psInstDest)
/*****************************************************************************
 FUNCTION	: ExpandLDWithImmediateOffsets

 PURPOSE	: Generate instructions to apply immediate, texel-space offsets
			  to a texture sample on cores which don't support this feature
			  in hardware.

 PARAMETERS	: psState			- Compiler state.
			  psProg			- The new instructions are appended to this
								program.
			  psOrigInst		- Texture sample instruction.
			  psInstDest		- Destination for the texture sample.

 RETURNS	: TRUE if the instruction was expanded.
*****************************************************************************/
{
	PUF_REGISTER				psCoordSrc;
	PUF_REGISTER				psSamplerSrc;
	IMG_UINT32					uSampler;
	IMG_UINT32					uTextureDimensionality;
	PUNIFLEX_TEXTURE_PARAMETERS	psSamplerParams;
	UF_REGISTER					sF32CoordSrc;
	UF_REGISTER					asDimensionConstant[UF_MAX_TEXTURE_DIMENSIONALITY];
	IMG_UINT32					uDimIdx;
	UF_OPCODE					eOrigInstOpCode;
	IMG_BOOL					bProjected;
	IMG_UINT32					uOneOverTextureDimension;
	IMG_UINT32					uImmOffsetsInFloat;
	IMG_UINT32					uScaledOffsets;
	IMG_UINT32					uDimensionMask;
	SAMPLE_INPUT_PARAMS			sSmpParams;
	IMG_UINT32					uParamSource;
	static const IMG_UINT32 auDimMask[] = 
	{
		1 << UFREG_DMASK_X_SHIFT,
		1 << UFREG_DMASK_Y_SHIFT,
		1 << UFREG_DMASK_Z_SHIFT,
	};

	if ((psState->uCompilerFlags & UF_SMPIMMCOORDOFFSETS) == 0)
	{
		return IMG_FALSE;
	}

	/*
		Get the index of the sampler this instruction samples.
	*/
	psSamplerSrc = &psOrigInst->asSrc[1];
	ASSERT(psSamplerSrc->eType == UFREG_TYPE_TEX);
	uSampler = psSamplerSrc->uNum;
	ASSERT(uSampler < psState->psSAOffsets->uTextureCount);

	/*
		Get the dimensionality of the texutre.
	*/
	uTextureDimensionality = GetTextureCoordinateUsedChanCount(psState, uSampler);
	ASSERT(uTextureDimensionality <= UF_MAX_TEXTURE_DIMENSIONALITY);
	uDimensionMask = (1U << uTextureDimensionality) - 1;

	psSamplerParams = &psState->psSAOffsets->asTextureParameters[uSampler];

	/*
		Check for a sampler instruction with immediate, texel space offsets.
	*/
	GetSampleInputParameters(psState, psOrigInst, psOrigInst->eOpCode, uTextureDimensionality, &sSmpParams);

	/*
		If this instruction doesn't use immediate texel offsets then nothing more to do.
	*/
	if (!sSmpParams.sOffsets.bPresent)
	{
		return IMG_FALSE;
	}

	/*
		Do we need to expand the texture sample instruction to emulate applying 
		immediate offsets to the texture coordinates?
	*/
	#if defined(SUPPORT_SGX545)
	if (
			(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_TAG_1D_2D_IMMEDIATE_COORDINATE_OFFSETS) &&
			uTextureDimensionality <= 2
	   )
	{
		/*
			The hardware supports this case of immediate offsets directly.
		*/
		return IMG_FALSE;
	}
	#endif /* defined(SUPPORT_SGX545) */

	/*
		Convert the texture coordinates to F32 format.
	*/
	psCoordSrc = &psOrigInst->asSrc[0];
	if (psCoordSrc->eFormat == UF_REGFORMAT_F32)
	{
		sF32CoordSrc = *psCoordSrc;
	}
	else
	{
		IMG_UINT32		uUpconvertedCoordTempNum;
		PUNIFLEX_INST	psCvtToInst;

		uUpconvertedCoordTempNum = psState->uInputTempRegisterCount++;

		psCvtToInst = AllocInputInst(psState, psProg, psOrigInst);
		psCvtToInst->eOpCode = UFOP_MOV;
		psCvtToInst->uPredicate = UF_PRED_NONE;
		BuildInputTempRegDest(&psCvtToInst->sDest, uUpconvertedCoordTempNum, UF_REGFORMAT_F32, USC_ALL_CHAN_MASK);
		psCvtToInst->asSrc[0] = psOrigInst->asSrc[0];

		BuildInputTempRegSrc(&sF32CoordSrc, uUpconvertedCoordTempNum, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
	}

	eOrigInstOpCode = psOrigInst->eOpCode;

	/*
		Check for a texture sample using projection. We need to apply projection before adding on the
		immediate offsets.
	*/
	bProjected = IMG_FALSE;
	if (psOrigInst->eOpCode == UFOP_LDP)
	{
		bProjected = IMG_TRUE;
	}
	if (
			psOrigInst->eOpCode == UFOP_LDPIFTC && 
			psOrigInst->asSrc[0].eType == UFREG_TYPE_TEXCOORD &&
			GetBit(psState->psSAOffsets->auProjectedCoordinateMask, psOrigInst->asSrc[0].uNum)
		)
	{
		bProjected = IMG_TRUE;
	}

	if (bProjected)
	{
		PUNIFLEX_INST	psRCPInst;
		PUNIFLEX_INST	psMULInst;
		IMG_UINT32		uOneOverW = psState->uInputTempRegisterCount++;
		IMG_UINT32		uProjectedCoords = psState->uInputTempRegisterCount++;

		eOrigInstOpCode = UFOP_LD;

		/*
			Calculate one over the sample coordinates W channel.
		*/
		psRCPInst = AllocInputInst(psState, psProg, psOrigInst);
		psRCPInst->eOpCode = UFOP_RECIP;
		BuildInputTempRegDest(&psRCPInst->sDest, uOneOverW, UF_REGFORMAT_F32, 1 << UFREG_DMASK_W_SHIFT);
		psRCPInst->asSrc[0] = sF32CoordSrc;

		/*
			Divice the other sample coordinates by the W channel.
		*/
		psMULInst = AllocInputInst(psState, psProg, psOrigInst);
		psMULInst->eOpCode = UFOP_MUL;
		BuildInputTempRegDest(&psMULInst->sDest, uProjectedCoords, UF_REGFORMAT_F32, uDimensionMask);
		BuildInputTempRegSrc(&psMULInst->asSrc[0], uOneOverW, UF_REGFORMAT_F32, UFREG_SWIZ_WWWW);
		psMULInst->asSrc[1] = sF32CoordSrc;

		/*
			Replace the sample coordinates by the projected version.
		*/
		BuildInputTempRegSrc(&sF32CoordSrc, uProjectedCoords, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
	}

	/*
		Create argument structures referencing a constant containing the size of each dimension.
	*/
	for (uDimIdx = 0; uDimIdx < uTextureDimensionality; uDimIdx++)
	{
		GetTextureDimensionConstant(psState, uSampler, uDimIdx, &asDimensionConstant[uDimIdx]);
	}

	/*
		Create a temporary register containing, for each dimension, the reciprocal of the dimension length
		and another temporary register containing the immediate offsets for each dimension in floating point format.
	*/
	uOneOverTextureDimension = psState->uInputTempRegisterCount++;
	uImmOffsetsInFloat = psState->uInputTempRegisterCount++;
	for (uDimIdx = 0; uDimIdx < uTextureDimensionality; uDimIdx++)
	{
		PUNIFLEX_INST	psRCPInst;
		PUNIFLEX_INST	psMOVInst;
		IMG_FLOAT		fOffset;

		/* Calculate 1/Texture Dimension */
		psRCPInst = AllocInputInst(psState, psProg, psOrigInst);
		psRCPInst->eOpCode = UFOP_RECIP;
		BuildInputTempRegDest(&psRCPInst->sDest, uOneOverTextureDimension, UF_REGFORMAT_F32, auDimMask[uDimIdx]);
		psRCPInst->asSrc[0] = asDimensionConstant[uDimIdx];
		psRCPInst->uPredicate = UF_PRED_NONE;

		fOffset = (IMG_FLOAT)sSmpParams.sOffsets.aiOffsets[uDimIdx];

		/*
			Load the immediate offset (in floating point) into a register.
		*/
		psMOVInst = AllocInputInst(psState, psProg, psOrigInst);
		psMOVInst->eOpCode = UFOP_MOV;
		BuildInputTempRegDest(&psMOVInst->sDest, uImmOffsetsInFloat, UF_REGFORMAT_F32, auDimMask[uDimIdx]);
		BuildInputImmediateSrc(&psMOVInst->asSrc[0], *(IMG_PUINT32)&fOffset, UF_REGFORMAT_F32);
		psMOVInst->uPredicate = UF_PRED_NONE;
	}

	/*
		Scale the immediate offsets by the dimensions of the texture.
	*/
	{
		PUNIFLEX_INST	psMULInst;

		uScaledOffsets = psState->uInputTempRegisterCount++;

		psMULInst = AllocInputInst(psState, psProg, psOrigInst);
		psMULInst->eOpCode = UFOP_MUL;
		BuildInputTempRegDest(&psMULInst->sDest, uScaledOffsets, UF_REGFORMAT_F32, uDimensionMask);
		BuildInputTempRegSrc(&psMULInst->asSrc[0], uOneOverTextureDimension, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		BuildInputTempRegSrc(&psMULInst->asSrc[1], uImmOffsetsInFloat, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		psMULInst->uPredicate = UF_PRED_NONE;
	}

	/*
		Get the index of the source argument to the original instruction which contains sampling options.
	*/
	uParamSource = GetSampleParametersSource(psOrigInst->eOpCode);

	if (!psSamplerParams->bMipMap)
	{
		PUNIFLEX_INST	psOffsetSampleInst;
		IMG_UINT32		uOffsetCoordinateTempReg;

		/*
			Add the scaled immediate offsets onto the sample coordinates.
		*/
		uOffsetCoordinateTempReg = 
			OffsetSampleCoordinates(psState,
									psProg,
									psOrigInst,
									uDimensionMask,
									uScaledOffsets,
									&sF32CoordSrc);

		/*
			Create a copy of the original sample instruction using the offset coordinates.
		*/
		psOffsetSampleInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psOffsetSampleInst, psOrigInst);
		psOffsetSampleInst->eOpCode = eOrigInstOpCode;
		psOffsetSampleInst->sDest = *psInstDest;
		BuildInputTempRegSrc(&psOffsetSampleInst->asSrc[0], uOffsetCoordinateTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		/* Mark the new sample instruction as not applying immediate offsets. */
		BuildInputImmediateSrc(&psOffsetSampleInst->asSrc[uParamSource], 0 /* uImmValue */, UF_REGFORMAT_U32);
		psOffsetSampleInst->uPredicate = UF_PRED_NONE;
	}
	else
	{
		PUNIFLEX_INST	psLDSINFInst;
		PUNIFLEX_INST	psIFCInst;
		IMG_UINT32		uSampleInfoTempReg;
		IMG_UINT32		uParam;
		IMG_UINT32		uMipMapLevel;
		IMG_UINT32		auSampleDest[2];
		IMG_UINT32		uBlendResult;
		UF_REGISTER		sBlendResult;
		UF_REGISTER		asLevelResult[2];
		UF_REGISTER		sTrilinearFraction;
		PUNIFLEX_INST	psMOVInst;
		PUNIFLEX_INST	psELSEInst;
		PUNIFLEX_INST	psENDIFInst;

		/*
			Allocate temporary registers for the result of texture sample returning sampling
			information.
		*/
		uSampleInfoTempReg = psState->uInputTempRegisterCount;
		psState->uInputTempRegisterCount += 2;

		/*
			TEMP.X = Mip-map level used for the sample.
			TEMP.Y = Trilinear fraction used for the sample.
			TEMP.Z = U bilinear fraction
			TEMP.W = V bilinear fraction
			(TEMP + 1).X = S bilinear fraction
			(TEMP + 1).YZW = Undefined
		*/
		psLDSINFInst = AllocInputInst(psState, psProg, psOrigInst);
		CopyInputInst(psLDSINFInst, psOrigInst);
		psLDSINFInst->uPredicate = UF_PRED_NONE;
		BuildInputTempRegDest(&psLDSINFInst->sDest, 
							  uSampleInfoTempReg, 
							  UF_REGFORMAT_F32, 
							  (1U << UFREG_DMASK_R_SHIFT) | 
							  (1U << UFREG_DMASK_G_SHIFT) |
							  (1U << UFREG_DMASK_B_SHIFT) |
							  (1U << UFREG_DMASK_A_SHIFT));
		uParam = UF_LDPARAM_SPECIALRETURN_SAMPLEINFO << UF_LDPARAM_SPECIALRETURN_SHIFT;
		BuildInputImmediateSrc(&psLDSINFInst->asSrc[uParamSource], uParam, UF_REGFORMAT_U32);
		psLDSINFInst->uPredicate = UF_PRED_NONE;

		/* 
			Create an IF statement: choose the first clause if the trilinear fraction is greater than
			zero and the second if it is zero.
		*/
		psIFCInst = AllocInputInst(psState, psProg, psOrigInst);
		psIFCInst->eOpCode = UFOP_IFC;
		BuildInputTempRegSrc(&psIFCInst->asSrc[0], uSampleInfoTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_YYYY);
		BuildInputCompOp(&psIFCInst->asSrc[1], UFREG_COMPOP_GT);
		BuildInputImmediateSrc(&psIFCInst->asSrc[2], 0 /* uImmValue */, UF_REGFORMAT_F32);
		psIFCInst->uPredicate = UF_PRED_NONE;

		/*
			Trilinear fraction greater than zero case:
		*/

		for (uMipMapLevel = 0; uMipMapLevel < 2; uMipMapLevel++)
		{
			IMG_UINT32	uLevelToSample;
			UF_REGISTER	sLevelResult;

			/*
				Allocate a temporary register for the results of sampling
				this mipmap level.
			*/
			auSampleDest[uMipMapLevel] = psState->uInputTempRegisterCount++;

			if (uMipMapLevel == 0)
			{
				uLevelToSample = uSampleInfoTempReg;
			}
			else
			{
				PUNIFLEX_INST	psADDInst;
				
				uLevelToSample = psState->uInputTempRegisterCount++;

				/*
					Add 1 onto the mipmap level (to make the level of the second mipmap sampled as 
					part of a trilinear blend).
				*/
				psADDInst = AllocInputInst(psState, psProg, psOrigInst);
				psADDInst->eOpCode = UFOP_ADD;
				BuildInputTempRegDest(&psADDInst->sDest, uLevelToSample, UF_REGFORMAT_F32, 1 << UFREG_DMASK_X_SHIFT);
				BuildInputTempRegSrc(&psADDInst->asSrc[0], uSampleInfoTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_XXXX);
				BuildInputHwConstSrc(&psADDInst->asSrc[1], HW_CONST_1, UF_REGFORMAT_F32);
				psADDInst->uPredicate = UF_PRED_NONE;
			}

			/*
				Sample the first mipmap level using sample coordinates with the immediate offsets
				scaled by the mipmap dimensions added on.
			*/
			BuildInputTempRegDest(&sLevelResult, auSampleDest[uMipMapLevel], UF_REGFORMAT_F32, psInstDest->u.byMask);
			SampleWithOffsetsAtMipMapLevel(psState,
										   psProg,
										   psOrigInst,
										   uDimensionMask,
										   &sLevelResult,
										   UF_PRED_NONE,
										   uScaledOffsets,
										   uLevelToSample,
										   &sF32CoordSrc);
		}

		/*
			Interpolate between the samples for the two mipmap levels using the trilinear fraction.
		*/
		uBlendResult = psState->uInputTempRegisterCount++;
		BuildInputTempRegDest(&sBlendResult, uBlendResult, UF_REGFORMAT_F32, psInstDest->u.byMask);
		for (uMipMapLevel = 0; uMipMapLevel < 2; uMipMapLevel++)
		{
			BuildInputTempRegSrc(&asLevelResult[uMipMapLevel], auSampleDest[uMipMapLevel], UF_REGFORMAT_F32, UFREG_SWIZ_NONE);
		}
		BuildInputTempRegSrc(&sTrilinearFraction, uSampleInfoTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_YYYY);
		MakeLRP(psState, 
				psProg, 
				psOrigInst,
				&sBlendResult,
				UF_PRED_NONE,
				&asLevelResult[1],
				&asLevelResult[0],
				&sTrilinearFraction);

		/*	
			Copy the interpolated result to the original destination of the texture sample.
		*/
		psMOVInst = AllocInputInst(psState, psProg, psOrigInst);
		psMOVInst->eOpCode = UFOP_MOV;
		psMOVInst->sDest = *psInstDest;
		psMOVInst->uPredicate = psOrigInst->uPredicate;
		BuildInputTempRegSrc(&psMOVInst->asSrc[0], uBlendResult, UF_REGFORMAT_F32, UFREG_SWIZ_NONE);

		psELSEInst = AllocInputInst(psState, psProg, psOrigInst);
		psELSEInst->eOpCode = UFOP_ELSE;
		psELSEInst->uPredicate = UF_PRED_NONE;

		/*
			Trilinear fraction equal to zero case.
		*/

		/*
			Create a texture sample instruction with the sample coordinate adjusted by the
			immediate offsets scaled to the dimensions of the sampled mipmap level.
		*/
		SampleWithOffsetsAtMipMapLevel(psState,
									   psProg,
									   psOrigInst,
									   uDimensionMask,
									   psInstDest,
									   psOrigInst->uPredicate,
									   uScaledOffsets,
									   uSampleInfoTempReg,
									   &sF32CoordSrc);

		psENDIFInst = AllocInputInst(psState, psProg, psOrigInst);
		psENDIFInst->eOpCode = UFOP_ENDIF;
		psENDIFInst->uPredicate = UF_PRED_NONE;
	}

	return IMG_TRUE;
}
#endif /* defined(OUTPUT_USCHW) */

static IMG_VOID CopyInputTextureSample(PINTERMEDIATE_STATE	psState, 
									   PINPUT_PROGRAM		psProg, 
									   PUNIFLEX_INST		psOrigInst,
									   PUF_REGISTER			psInstDest)
/*****************************************************************************
 FUNCTION	: CopyInputTextureSample

 PURPOSE	: Create a copy of an input texture sample instruction.

 PARAMETERS	: psState			- Compiler state.
			  psOrigInst		- Instruction to copy.
			  psProg			- The new instruction is appended to this
								program.
			  psInstDest		- Destination for the instruction.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST	psInst;

	#if defined(OUTPUT_USCHW)
	if (ExpandLDWithImmediateOffsets(psState, psProg, psOrigInst, psInstDest))
	{
		return;
	}
	#endif /* defined(OUTPUT_USCHW) */

	psInst = AllocInputInst(psState, psProg, psOrigInst);
	CopyInputInst(psInst, psOrigInst);
	psInst->sDest = *psInstDest;

	/*
		For backwards compatibility we don't require the drivers to initialize the texture
		sample parameters argument. So set it to a default value now.
	*/
	if ((psState->uCompilerFlags & UF_SMPIMMCOORDOFFSETS) == 0)
	{
		IMG_UINT32	uParamSource;

		uParamSource = GetSampleParametersSource(psOrigInst->eOpCode);
		BuildInputImmediateSrc(&psInst->asSrc[uParamSource], 0 /* uImmVal */, UF_REGFORMAT_U32);
	}
}

static IMG_VOID ExpandMacro(PINTERMEDIATE_STATE	psState, 
							PUNIFLEX_INST		psOrigInst, 
							PINPUT_PROGRAM		psProg)
/*****************************************************************************
 FUNCTION	: ExpandMacro

 PURPOSE	: Expand a macro instruction.

 PARAMETERS	: psOrigInst		- Instruction to expand..
			  apsProg			- Array to hold the expanded instructions.
			  uIdx				- Next free element in the array
			  uLast				- Last instruction added to the array.

 RETURNS	: The number of instructions in the expanded form.
*****************************************************************************/
{
	IMG_UINT32 i;
	static const IMG_UINT8 abyMask[4] = {UFREG_DMASK_R_SHIFT, 
										 UFREG_DMASK_G_SHIFT,
										 UFREG_DMASK_B_SHIFT, 
										 UFREG_DMASK_A_SHIFT};
	PUNIFLEX_INST	psInst;

	if (psOrigInst->eOpCode == UFOP_M4X4 || psOrigInst->eOpCode == UFOP_M4X3 ||
		psOrigInst->eOpCode == UFOP_M3X4 || psOrigInst->eOpCode == UFOP_M3X3 ||
		psOrigInst->eOpCode == UFOP_M3X2)
	{
		UF_OPCODE eOpCode = 
			(psOrigInst->eOpCode == UFOP_M4X4 || psOrigInst->eOpCode == UFOP_M4X3) ? UFOP_DOT4 : UFOP_DOT3;
		IMG_UINT32 uLimit;
		if (psOrigInst->eOpCode == UFOP_M4X4 || psOrigInst->eOpCode == UFOP_M3X4)
		{
			uLimit = 4;
		}
		else 
		{
			if (psOrigInst->eOpCode == UFOP_M4X3 || psOrigInst->eOpCode == UFOP_M3X3)
			{
				uLimit = 3;
			}
			else
			{
				uLimit = 2;
			}
		}
		for (i = 0; i < uLimit; i++)
		{
			if (psOrigInst->sDest.u.byMask & (1U << abyMask[i]))
			{
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				psInst->eOpCode = eOpCode;
				psInst->sDest = psOrigInst->sDest;
				psInst->sDest.u.byMask = (IMG_UINT8)(1U << abyMask[i]);
				psInst->asSrc[0] = psOrigInst->asSrc[0];
				psInst->asSrc[1] = psOrigInst->asSrc[1];
				psInst->asSrc[1].uNum = (IMG_UINT8)(psInst->asSrc[1].uNum + i);
				psInst->uPredicate = psOrigInst->uPredicate;
			}
		}
	}
	else
	{
		switch (psOrigInst->eOpCode)
		{
			case UFOP_LD:
			case UFOP_LDB:
			case UFOP_LDC:
			case UFOP_LDCLZ:
			case UFOP_LDL:
			case UFOP_LDD:
			case UFOP_LDP:
			case UFOP_LDPIFTC:
			case UFOP_LD2DMS:
			case UFOP_LDGATHER4:
			{
				#if defined(OUTPUT_USCHW)
				if (!(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN))
				{
					IMG_UINT32			uTexture = psOrigInst->asSrc[1].uNum;
					PUNIFLEX_TEXFORM	psTexForm;

					ASSERT(uTexture < psState->psSAOffsets->uTextureCount);
					psTexForm = &psState->psSAOffsets->asTextureParameters[uTexture].sFormat;

					if (psTexForm->uCSCConst != UINT_MAX)
					{
						IMG_UINT32	uSampleResultTempReg = psState->uInputTempRegisterCount++;
						UF_REGISTER	sSampleResult;

						/*
						  Do the original texture sample into a temporary register.
						*/
						BuildInputTempRegDest(&sSampleResult, uSampleResultTempReg, psOrigInst->sDest.eFormat, USC_DESTMASK_FULL);
						CopyInputTextureSample(psState, psProg, psOrigInst, &sSampleResult);

						/*
						  Do a matrix multiply between the texture sample register
						  and the colour space conversion constants.
						*/
						for (i = 0; i < 4; i++)
						{
							IMG_UINT8 byMask = (IMG_UINT8)(1U << abyMask[i]);
							if (psOrigInst->sDest.u.byMask & byMask)
							{
								psInst = AllocInputInst(psState, psProg, psOrigInst);
								psInst->eOpCode = UFOP_DOT4;
								psInst->uPredicate = psOrigInst->uPredicate;
								psInst->sDest = psOrigInst->sDest;
								psInst->sDest.u.byMask = byMask;
								/*
								  Force a saturation on the result.
								*/
								psInst->sDest.byMod = (IMG_BYTE)(psInst->sDest.byMod & ~UFREG_DMOD_SAT_MASK);
								psInst->sDest.byMod |= (UFREG_DMOD_SATZEROONE << UFREG_DMOD_SAT_SHIFT);
								/*
								  Put the texture sample in source 0.
								*/
								psInst->asSrc[0].eType = UFREG_TYPE_TEMP;
								psInst->asSrc[0].byMod = 0;
								psInst->asSrc[0].u.uSwiz = UFREG_SWIZ_RGBA;
								psInst->asSrc[0].uNum = uSampleResultTempReg;
								psInst->asSrc[0].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
								psInst->asSrc[0].eFormat = psOrigInst->sDest.eFormat;
								/*
								  Put the CSC constant in source 1.
								*/
								psInst->asSrc[1].eType = UFREG_TYPE_CONST;
								psInst->asSrc[1].byMod = 0;
								psInst->asSrc[1].u.uSwiz = UFREG_SWIZ_RGBA;
								psInst->asSrc[1].uNum = psTexForm->uCSCConst + i;
								psInst->asSrc[1].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
								psInst->asSrc[1].eFormat = psOrigInst->sDest.eFormat;
							}
						}
					}
					else
					{
						CopyInputTextureSample(psState, psProg, psOrigInst, &psOrigInst->sDest);
					}
				}
				else
				#endif	/* #if defined(OUTPUT_USCHW) */
				{
					CopyInputTextureSample(psState, psProg, psOrigInst, &psOrigInst->sDest);
				}
				break;
			}
			case UFOP_LDLOD:
			{
				IMG_UINT32		uTempReg = psState->uInputTempRegisterCount += 2;
				UF_REGFORMAT	eDestFormat = psOrigInst->sDest.eFormat;
				PUNIFLEX_INST	psLDSINFInst, psADDInst, psMOVInst;
				IMG_UINT32		uParam;

				/*
					TEMP.X = Mip-map level used for the sample.
					TEMP.Y = Trilinear fraction used for the sample.
					TEMP.Z = U bilinear fraction
					TEMP.W = V bilinear fraction
					(TEMP + 1).X = S bilinear fraction
					(TEMP + 1).YZW = Undefined
				*/
				psLDSINFInst = AllocInputInst(psState, psProg, psOrigInst);
				CopyInputInst(psLDSINFInst, psOrigInst);
				psLDSINFInst->eOpCode = UFOP_LD;
				BuildInputTempRegDest(&psLDSINFInst->sDest, 
									  uTempReg, 
									  UF_REGFORMAT_F32, 
									  (1U << UFREG_DMASK_R_SHIFT) | (1U << UFREG_DMASK_G_SHIFT));
				uParam = UF_LDPARAM_SPECIALRETURN_SAMPLEINFO << UF_LDPARAM_SPECIALRETURN_SHIFT;
				BuildInputImmediateSrc(&psLDSINFInst->asSrc[2], uParam, UF_REGFORMAT_U32);
				

				/*
					ORIG_DEST.X = TEMP.X + TEMP.Y
				*/
				psADDInst = AllocInputInst(psState, psProg, psOrigInst);
				psADDInst->eOpCode = UFOP_ADD;
				psADDInst->uPredicate = psOrigInst->uPredicate;
				psADDInst->sDest = psOrigInst->sDest;
				psADDInst->sDest.u.byMask &= (1U << UFREG_DMASK_R_SHIFT);
				BuildInputTempRegSrc(&psADDInst->asSrc[0], uTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_XXXX);
				BuildInputTempRegSrc(&psADDInst->asSrc[1], uTempReg, UF_REGFORMAT_F32, UFREG_SWIZ_YYYY);

				/*
					ORIG_DEST.YZW = 0
				*/
				psMOVInst = AllocInputInst(psState, psProg, psOrigInst);
				psMOVInst->eOpCode = UFOP_MOV;
				psMOVInst->uPredicate = psOrigInst->uPredicate;
				psMOVInst->sDest = psOrigInst->sDest;
				psMOVInst->sDest.u.byMask &= (1U << UFREG_DMASK_G_SHIFT) |
											 (1U << UFREG_DMASK_B_SHIFT) |
										     (1U << UFREG_DMASK_A_SHIFT);
				BuildInputImmediateSrc(&psMOVInst->asSrc[0], 0 /* uImmVal */, eDestFormat);
				break;
			}
			case UFOP_ADD:
			{
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				CopyInputInst(psInst, psOrigInst);
				if (psOrigInst->asSrc[1].eType == UFREG_TYPE_HW_CONST &&
					psOrigInst->asSrc[1].uNum == HW_CONST_0 &&
					psOrigInst->asSrc[1].byMod == UFREG_SMOD_NONE)
				{
					psInst->eOpCode = UFOP_MOV;
				}
				break;
			}
			case UFOP_DST:
			{
				IMG_UINT32	uChan;
				IMG_UINT32	uTempMask;
				IMG_UINT32	uTempDest;

				uTempMask = 0;
				uTempDest = USC_UNDEF;
				for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
				{
					IMG_BOOL	bTempDest;
					IMG_UINT32	uSrc;

					if ((psOrigInst->sDest.u.byMask & (1U << abyMask[uChan])) == 0)
					{
						continue;
					}

					/*
					  Check if writing this channel will overwrite a value used in
					  calculating a later channel.
					*/
					bTempDest = IMG_FALSE;
					for (uSrc = 0; uSrc < 2; uSrc++)
					{
						if (psOrigInst->asSrc[uSrc].eType == psOrigInst->sDest.eType &&
							psOrigInst->asSrc[uSrc].uNum == psOrigInst->sDest.uNum)
						{
							IMG_UINT32	uNextChan;

							for (uNextChan = uChan + 1; uNextChan < CHANNELS_PER_INPUT_REGISTER; uNextChan++)
							{
								if ((psOrigInst->sDest.u.byMask & (1U << abyMask[uNextChan])) == 0)
								{
									continue;
								}
								if ((uNextChan == 2 && uSrc == 1) || (uNextChan == 3 && uSrc == 0))
								{
									continue;
								}
								if (EXTRACT_CHAN(psOrigInst->asSrc[uSrc].u.uSwiz, uNextChan) == uChan)
								{
									bTempDest = IMG_TRUE;
									break;
								}
							}
							if (bTempDest)
							{
								break;
							}
						}
					}

					psInst = AllocInputInst(psState, psProg, psOrigInst);
					if (bTempDest)
					{
						if (uTempDest == USC_UNDEF)
						{
							uTempDest = psState->uInputTempRegisterCount++;
						}

						psInst->sDest.u.byMask = psOrigInst->sDest.u.byMask;
						psInst->sDest.byMod = 0;
						psInst->sDest.eType = UFREG_TYPE_TEMP;
						psInst->sDest.uNum = uTempDest;
						psInst->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
						psInst->sDest.eFormat = psOrigInst->sDest.eFormat;

						uTempMask |= (1U << abyMask[uChan]);
					}
					else
					{
						psInst->sDest = psOrigInst->sDest;
					}

					psInst->sDest.u.byMask = (IMG_UINT8)(1U << abyMask[uChan]);
					psInst->uPredicate = psOrigInst->uPredicate;
					psInst->psILink = NULL;

					switch (uChan)
					{
						case 0:
						{
							psInst->eOpCode = UFOP_MOV;
							psInst->asSrc[0].eType = UFREG_TYPE_HW_CONST;
							psInst->asSrc[0].uNum = HW_CONST_1;
							psInst->asSrc[0].u.uSwiz = UFREG_SWIZ_NONE;
							psInst->asSrc[0].byMod = UFREG_SMOD_NONE;
							psInst->asSrc[0].eFormat = psOrigInst->sDest.eFormat;
							psInst->asSrc[0].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
							break;
						}
						case 1:
						{
							psInst->eOpCode = UFOP_MUL;
							psInst->asSrc[0] = psOrigInst->asSrc[0];
							psInst->asSrc[1] = psOrigInst->asSrc[1];
							break;
						}
						case 2:
						{
							psInst->eOpCode = UFOP_MOV;
							psInst->asSrc[0] = psOrigInst->asSrc[0];
							break;
						}
						case 3:
						{
							psInst->eOpCode = UFOP_MOV;
							psInst->asSrc[0] = psOrigInst->asSrc[1];
							break;
						}
					}
				}

				/*
				  For any channels written to a temporary register move them
				  into the instruction destination.
				*/
				if (uTempMask != 0)
				{
					psInst = AllocInputInst(psState, psProg, psOrigInst);
					psInst->eOpCode = UFOP_MOV;
					psInst->sDest = psOrigInst->sDest;
					psInst->uPredicate = psOrigInst->uPredicate;
					psInst->sDest.u.byMask = (IMG_UINT8)uTempMask;
					psInst->asSrc[0].eType = UFREG_TYPE_TEMP;
					psInst->asSrc[0].byMod = 0;
					psInst->asSrc[0].u.uSwiz = UFREG_SWIZ_NONE;
					psInst->asSrc[0].uNum = uTempDest;
					psInst->asSrc[0].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
					psInst->asSrc[0].eFormat = psOrigInst->sDest.eFormat;
				}
				break;
			}
			case UFOP_ABS:
			{
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				psInst->eOpCode = UFOP_MOV;
				psInst->sDest = psOrigInst->sDest;
				psInst->asSrc[0] = psOrigInst->asSrc[0];
				psInst->asSrc[0].byMod |= UFREG_SOURCE_ABS;
				psInst->asSrc[0].byMod = (IMG_BYTE)(psInst->asSrc[0].byMod & ~UFREG_SOURCE_NEGATE);
				psInst->uPredicate = psOrigInst->uPredicate;
				break;
			}

			case UFOP_SUB:
			{
				ExpandSUB(psState, psOrigInst, psProg);
				break;
			}
			case UFOP_ATOM_SUB:
			{
				ExpandAtomicSUB(psState, psOrigInst, psProg);
				break;
			}
			case UFOP_LIT:
			case UFOP_OGLLIT:
			{
				ExpandLIT(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_LRP:
			{
				UF_REGFORMAT eFmt = GetRegisterFormat(psState, &psOrigInst->sDest);
				if ((eFmt == UF_REGFORMAT_C10) || (eFmt == UF_REGFORMAT_U8))
				{
					psInst = AllocInputInst(psState, psProg, psOrigInst);
					CopyInputInst(psInst, psOrigInst);
				}
				else
				{
					MakeLRP(psState,
							psProg,
							psOrigInst,
							&psOrigInst->sDest,
							psOrigInst->uPredicate,
							&psOrigInst->asSrc[0],
							&psOrigInst->asSrc[1],
							&psOrigInst->asSrc[2]);
				}
				break;
			}

			case UFOP_CRS:
			{
				IMG_UINT32	uTempReg = psState->uInputTempRegisterCount++;

				/* MUL rT, rA.BRGA, rB.GBRA */
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				psInst->eOpCode = UFOP_MUL;
				psInst->sDest.byMod = 0;
				psInst->sDest.eType = UFREG_TYPE_TEMP;
				psInst->sDest.uNum = uTempReg;
				psInst->sDest.u.byMask = 
					((1U << UFREG_DMASK_B_SHIFT) | (1U << UFREG_DMASK_G_SHIFT) | (1U << UFREG_DMASK_R_SHIFT));
				psInst->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
				
				/* Internal results for U8 is calculated in C10 format. */
				if(psOrigInst->sDest.eFormat == UF_REGFORMAT_U8)
				{
					psInst->sDest.eFormat = UF_REGFORMAT_C10;
				}
				else
				{
					psInst->sDest.eFormat = psOrigInst->sDest.eFormat;
				}

				psInst->uPredicate = UF_PRED_NONE;
				psInst->asSrc[0] = psOrigInst->asSrc[0];
				psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_BRGA);
				psInst->asSrc[1] = psOrigInst->asSrc[1];
				psInst->asSrc[1].u.uSwiz = RemapInputSwizzle(psInst->asSrc[1].u.uSwiz, UFREG_SWIZ_GBRA);

				/* MAD rD, rA.GBRA, rB.BRGA, -T */
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				psInst->eOpCode = UFOP_MAD;
				psInst->sDest = psOrigInst->sDest;
				psInst->uPredicate = psOrigInst->uPredicate;
				psInst->asSrc[0] = psOrigInst->asSrc[0];
				psInst->asSrc[0].u.uSwiz = RemapInputSwizzle(psInst->asSrc[0].u.uSwiz, UFREG_SWIZ_GBRA);
				psInst->asSrc[1] = psOrigInst->asSrc[1];
				psInst->asSrc[1].u.uSwiz = RemapInputSwizzle(psInst->asSrc[1].u.uSwiz, UFREG_SWIZ_BRGA);
				psInst->asSrc[2].byMod = UFREG_SOURCE_NEGATE;
				psInst->asSrc[2].u.uSwiz = UFREG_SWIZ_RGBA;
				psInst->asSrc[2].eType = UFREG_TYPE_TEMP;
				psInst->asSrc[2].uNum = uTempReg;
				psInst->asSrc[2].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
				
				/* Internal results for U8 is calculated in C10 format. */
				if(psOrigInst->sDest.eFormat == UF_REGFORMAT_U8)
				{
					psInst->asSrc[2].eFormat = UF_REGFORMAT_C10;
				}
				else
				{
					psInst->asSrc[2].eFormat = psOrigInst->sDest.eFormat;
				}

				break;
			}

			case UFOP_NRM:
			{
				/* Expand macro to normalise operations */
#if defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541)
				if (UseFNormalise(psState, psOrigInst))
				{
					psInst = AllocInputInst(psState, psProg, psOrigInst);
					CopyInputInst(psInst, psOrigInst);
				}
				else
#endif /* defined(SUPPORT_SGX531) || defined(SUPPORT_SGX540) || defined(SUPPORT_SGX541) */
				{
					IMG_UINT32	uTempReg = psState->uInputTempRegisterCount++;

					psInst = AllocInputInst(psState, psProg, psOrigInst);
					psInst->eOpCode = UFOP_DOT3;
					psInst->sDest.byMod = 0;
					psInst->sDest.eType = UFREG_TYPE_TEMP;
					psInst->sDest.uNum = uTempReg;
					psInst->sDest.u.byMask = 1 << UFREG_DMASK_B_SHIFT;
					psInst->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
					psInst->sDest.eFormat = psOrigInst->sDest.eFormat;
					psInst->asSrc[0] = psOrigInst->asSrc[0];
					psInst->asSrc[1] = psOrigInst->asSrc[0];
					psInst->uPredicate = UF_PRED_NONE;

					psInst = AllocInputInst(psState, psProg, psOrigInst);
					psInst->eOpCode = UFOP_RSQRT;
					psInst->sDest.byMod = 0;
					psInst->sDest.eType = UFREG_TYPE_TEMP;
					psInst->sDest.uNum = uTempReg;
					psInst->sDest.u.byMask = 1 << UFREG_DMASK_B_SHIFT;
					psInst->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
					psInst->sDest.eFormat = psOrigInst->sDest.eFormat;
					psInst->asSrc[0].byMod = UFREG_SMOD_NONE;
					psInst->asSrc[0].u.uSwiz = UFREG_SWIZ_BBBB;
					psInst->asSrc[0].eType = UFREG_TYPE_TEMP;
					psInst->asSrc[0].uNum = uTempReg;
					psInst->asSrc[0].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
					psInst->asSrc[0].eFormat = psOrigInst->sDest.eFormat;
					psInst->uPredicate = UF_PRED_NONE;

					psInst = AllocInputInst(psState, psProg, psOrigInst);
					psInst->eOpCode = UFOP_MUL;
					psInst->sDest = psOrigInst->sDest;
					psInst->asSrc[0] = psOrigInst->asSrc[0];
					psInst->asSrc[1].byMod = 0;
					psInst->asSrc[1].u.uSwiz = UFREG_SWIZ_BBBB;
					psInst->asSrc[1].eType = UFREG_TYPE_TEMP;
					psInst->asSrc[1].uNum = uTempReg;
					psInst->asSrc[1].eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
					psInst->asSrc[1].eFormat = psOrigInst->sDest.eFormat;
					psInst->uPredicate = psOrigInst->uPredicate;
				}
				break;
			}
			case UFOP_MOVA_TRC:
			{
				ExpandMOVA_TRC(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_POW:
			{
				ExpandPOW(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_EXPP:
			{
				ExpandEXPP(psState, psOrigInst, psProg);
				break;
			}
			case UFOP_SINCOS:
			{
				ExpandSINCOS(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_SINCOS2:
			{
				ExpandSINCOS2(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_CND:
			{
				ExpandCND(psState, psOrigInst, psProg);
				break;
			}
			
			case UFOP_SLT:
			case UFOP_SGE:
			{
				ExpandSLT_SGE(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_SGN:
			{
				ExpandSGN(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_RNE:
			case UFOP_RND:
			{
				ExpandRNE(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_TRC:
			{
				ExpandTRC(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_CEIL:
			{
				ExpandCEIL(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_FLR:
			{
				ExpandFLR(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_DIV:
			{
				if (psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_I32 ||
					psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_U32)
				{
					ExpandIntegerDIV(psState, psOrigInst, psProg);
				}
				else if (psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_I16 ||
						 psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_U16)
				{
					ExpandInt16DIV(psState, psOrigInst, psProg);
				}
				else
				{
					ExpandFloatDIV(psState, psOrigInst, psProg);
				}
				break;
			}
			case UFOP_DIV2:
			{
				if (psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_I32)
				{
					ExpandIntegerDIV(psState, psOrigInst, psProg);
				}
				else if (psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_I16 ||
						 psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_U16)
				{
					ExpandInt16DIV(psState, psOrigInst, psProg);
				}
				else
				{
					ASSERT(psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_U32);
					psInst = AllocInputInst(psState, psProg, psOrigInst);
					CopyInputInst(psInst, psOrigInst);
				}
				break;
			}

			case UFOP_MUL:
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				CopyInputInst(psInst, psOrigInst);
				if (psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_I32	||
					psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_U32	||
					psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_I16	||
					psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_U16	||
					psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_I8_UN	||
					psOrigInst->asSrc[0].eFormat == UF_REGFORMAT_U8_UN )
				{
					psInst->eOpCode = UFOP_MUL2;
					psInst->sDest2.u.byMask = 0;
				}
				break;

			case UFOP_SQRT:
			{
				ExpandSQRT(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_MOV:
			{
				ExpandMOV(psState, psOrigInst, psProg);
				break;
			}

			case UFOP_REINTERP:
			{
				ExpandREINTERP(psState, psOrigInst, psProg);
				break;
			}
			
			default:
			{
				psInst = AllocInputInst(psState, psProg, psOrigInst);
				CopyInputInst(psInst, psOrigInst);
				break;
			}
		}
	}
}

IMG_INTERNAL 
IMG_VOID GetInputPredicateInst(PINTERMEDIATE_STATE psState, PINST psInst, IMG_UINT32 uPredicate, IMG_UINT32 uChan)
/*****************************************************************************
 FUNCTION	: GetInputPredicateInst

 PURPOSE	: Gets the predicate for an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction to set the predicate on.
			  uPredicate		- The input predicate to set.
			  uChan				- The input register channel.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bPredNegate;
	IMG_UINT32	uPredSrc;

	GetInputPredicate(psState, &uPredSrc, &bPredNegate, uPredicate, uChan);
	SetPredicate(psState, psInst, uPredSrc, bPredNegate);
}

IMG_INTERNAL 
UF_REGFORMAT GetRegisterFormat(PINTERMEDIATE_STATE psState, PUF_REGISTER psSrc)
/*****************************************************************************
 FUNCTION	: GetRegisterFormat

 PURPOSE	: Get the format of an input register.

 PARAMETERS	: psState			- Compiler state.
			  psSrc				- Register to get the format of.

 RETURNS	: The format.
*****************************************************************************/
{
	PVR_UNREFERENCED_PARAMETER(psState);
	return psSrc->eFormat;
}

typedef struct _RELATIVE_INDEX_EXPAND_STATE
{
	IMG_UINT32	puLoadedIndxReg[4];
	IMG_UINT8	pbyLoadedIndxRegChan[4];
	IMG_UINT32	uLoadedChansCount;
} RELATIVE_INDEX_EXPAND_STATE, *PRELATIVE_INDEX_EXPAND_STATE;

static IMG_BOOL IsChanAlreadyLoaded(PRELATIVE_INDEX_EXPAND_STATE	psExpandState,
									IMG_UINT32						uRelativeReg, 
									IMG_BYTE						byRelativeRegChan, 
									IMG_PUINT32						puAddressChanLoadedIn)
/*****************************************************************************
 FUNCTION	: IsChanAlreadyLoaded

 PURPOSE	: Tells whether the relatively indexing channel is already 
			  loaded or not.

 PARAMETERS	: puLoadedIndxReg	- Record of already loaded index registers.
			  pbyLoadedIndxRegChan	- Record of already loaded index register 
									  channels.
			  uLoadedChansCount	- Count of already loaded channels.
			  uRelativeReg	- Relative register to search.
			  byRelativeRegChan	- Relative register's channel to search.
			  puAddressChanLoadedIn	- To return address channel in which 
									  channel is loaded.

 RETURNS	: IMG_TRUE	- If channel is already loaded.
			  IMG_FALSE	- If channel is not already loaded.
*****************************************************************************/
{
	IMG_UINT32 uI = 0;
	for(; uI<psExpandState->uLoadedChansCount; uI++)
	{
		if(psExpandState->puLoadedIndxReg[uI] == uRelativeReg && 
			psExpandState->pbyLoadedIndxRegChan[uI] == byRelativeRegChan)
		{
			*puAddressChanLoadedIn = uI;
			return IMG_TRUE;
		}
	}
	return IMG_FALSE;
}

static IMG_VOID ExpandRelativeIndex(PINTERMEDIATE_STATE				psState,
									PRELATIVE_INDEX_EXPAND_STATE	psExpandState,
									PINPUT_PROGRAM					psProg,
									PUNIFLEX_INST					psMOVAInsertPoint,
									PUNIFLEX_INST					psOrigInst,
									PUF_REGISTER					psReg, 
									IMG_BOOL*						abAcesedShdrOutputRanges)
/*****************************************************************************
 FUNCTION	: ExpandRelativeIndex

 PURPOSE	: Replaces any register accesses relative to a temporary register
			  by a MOVA instruction.

 PARAMETERS	: psState			- Compiler state.
			  psExpandState		- Information about replacements already done.
			  psProg			- Input program.
			  psMOVAInsertPoint	- Instruction to insert any MOVA instruction before.
			  psOrigInst		- Instruction to expand.
			  psReg				- Destination or source register to expand.
			  abAcesedShdrOutputRanges 
								- Represents Shader Output Ranges that are 
								  accessed.

 RETURNS	: Nothing.
			  abAcesedShdrOutputRanges
								- IMG_TRUE will be set for those Shader 
								Output Ranges that are accessed in the Shader.
*****************************************************************************/
{
	IMG_BOOL	bAlreadyLoaded;
	static const IMG_UINT16 REPLICATE_SWIZ[4] = {UFREG_SWIZ_RRRR, UFREG_SWIZ_GGGG, UFREG_SWIZ_BBBB, UFREG_SWIZ_AAAA};
	static const UFREG_RELATIVEINDEX RELATIVEINDEX_BY_CHAN[4] = {UFREG_RELATIVEINDEX_A0X, UFREG_RELATIVEINDEX_A0Y, UFREG_RELATIVEINDEX_A0Z, UFREG_RELATIVEINDEX_A0W};
	IMG_UINT32	uAddrRegChan;

	if((psReg->eType == UFREG_TYPE_VSOUTPUT) && (psReg->eRelativeIndex != UFREG_RELATIVEINDEX_NONE))
	{
		IMG_UINT32 uRangeIdx;
		for(uRangeIdx = 0; uRangeIdx < psState->psSAOffsets->sShaderOutPutRanges.uRangesCount; uRangeIdx++)
		{
			if(
				psReg->uNum >= psState->psSAOffsets->sShaderOutPutRanges.psRanges[uRangeIdx].uRangeStart && 
				psReg->uNum < psState->psSAOffsets->sShaderOutPutRanges.psRanges[uRangeIdx].uRangeEnd
			)
			{
				abAcesedShdrOutputRanges[uRangeIdx] = IMG_TRUE;
			}
		}
	}
	/*
		Is this a relative access using a temporary register as an index.
	*/
	if (psReg->eRelativeIndex != UFREG_RELATIVEINDEX_TEMP_REG) 
	{
		return;
	}

	/*
		Have we already set up an address register containing the
		contents of the temporary register index.
	*/
	bAlreadyLoaded = IsChanAlreadyLoaded(psExpandState,
										 psReg->uRelativeNum, 
										 psReg->byRelativeChan, 
										 &uAddrRegChan);

	if (!bAlreadyLoaded)
	{
		/* Create a new MOVA instruction */
		PUNIFLEX_INST psMovInst;
		
		psMovInst = UscAlloc(psState, sizeof(UNIFLEX_INST));
		psMovInst->eOpCode = UFOP_MOVA_INT;
		psMovInst->uPredicate = psOrigInst->uPredicate;
#ifdef SRC_DEBUG
		psMovInst->uSrcLine = psOrigInst->uSrcLine;
#endif /* SRC_DEBUG */

		psMovInst->sDest.eType = UFREG_TYPE_ADDRESS;
		psMovInst->sDest.uNum = 0;				
		psMovInst->sDest.byMod = (UFREG_DMOD_SATNONE << UFREG_DMOD_SAT_SHIFT) | (UFREG_DMOD_SCALEMUL1 << UFREG_DMOD_SCALE_SHIFT);
		psMovInst->sDest.eRelativeIndex = UFREG_RELATIVEINDEX_NONE;
		psMovInst->sDest.uArrayTag = 0;
		psMovInst->sDest.eFormat = UF_REGFORMAT_F32;
		psMovInst->sDest.u.byMask = 0;				

		uAddrRegChan = psExpandState->uLoadedChansCount;
		switch(uAddrRegChan)
		{
			case 0:
				psMovInst->sDest.u.byMask |= (1U << UFREG_DMASK_R_SHIFT);
				break;
			case 1:
				psMovInst->sDest.u.byMask |= (1U << UFREG_DMASK_G_SHIFT);
				break;
			case 2:
				psMovInst->sDest.u.byMask |= (1U << UFREG_DMASK_B_SHIFT);
				break;
			case 3:
				psMovInst->sDest.u.byMask |= (1U << UFREG_DMASK_A_SHIFT);
				break;
			default:
				/*
				  too many operands to psOrigInst were indexed (by different temporaries)
				  - we've run out of address registers!!
				*/
				imgabort();
		}

		BuildInputSrc(&psMovInst->asSrc[0], 
					  UFREG_TYPE_TEMP, 
					  psReg->uRelativeNum, 
					  UF_REGFORMAT_F32, 
					  REPLICATE_SWIZ[psReg->byRelativeChan]);

		/* save it */
		psExpandState->puLoadedIndxReg[psExpandState->uLoadedChansCount] = psReg->uRelativeNum;
		psExpandState->pbyLoadedIndxRegChan[psExpandState->uLoadedChansCount] = psReg->byRelativeChan;
		psExpandState->uLoadedChansCount++;

		/*
			Insert the MOVA instruction in the program.
		*/
		psMovInst->psBLink = psMOVAInsertPoint->psBLink;
		psMovInst->psILink = psMOVAInsertPoint;

		if (psMOVAInsertPoint->psBLink != NULL)
		{
			psMOVAInsertPoint->psBLink->psILink = psMovInst;
		}
		else
		{
			psProg->psHead = psMovInst;
		}
		psMOVAInsertPoint->psBLink = psMovInst;
	}
	/*
	  ok - at this point, the desired temporary register has been loaded into
	  the address register indicated by uAddrRegChan
	*/
	psReg->eRelativeIndex = RELATIVEINDEX_BY_CHAN[uAddrRegChan];
}

static
IMG_VOID ReplacePSOutputByIndexableTemporary(PINTERMEDIATE_STATE	psState,
											 PINPUT_PROGRAM			psProg)
/*****************************************************************************
 FUNCTION	: ReplacePSOutputByIndexableTemporary

 PURPOSE	: Replaces all pixel shader colour outputs arguments in the input program 
			  by a newly created indexable temporary array.

 PARAMETERS	: psState			- Compiler state.
			  psProg			- Input program.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32						uArray;
	IMG_UINT32						uMaxTag;
	PUNIFLEX_INDEXABLE_TEMP_SIZE	psNewArray;
	PPIXELSHADER_STATE				psPS = psState->sShader.psPS;
	PUNIFLEX_INST					psInst;
	IMG_UINT32						uSurface;
	UF_REGFORMAT					eSurfaceFormat;
	IMG_UINT32						uValidSurfaceCount;

	/*
		All pixel shader outputs should use the same format if they are dynamically indexed.
	*/
	eSurfaceFormat = UF_REGFORMAT_UNTYPED;
	for (uSurface = 0; uSurface < UNIFLEX_MAX_OUT_SURFACES; uSurface++)
	{
		if (psPS->aeColourResultFormat[uSurface] != UF_REGFORMAT_UNTYPED)
		{
			ASSERT(eSurfaceFormat == UF_REGFORMAT_UNTYPED || eSurfaceFormat == psPS->aeColourResultFormat[uSurface]);
			eSurfaceFormat = psPS->aeColourResultFormat[uSurface];
		}
	}

	uValidSurfaceCount = 1;
	if (psState->psSAOffsets->ePackDestFormat != UF_REGFORMAT_U8)
	{
		for (uSurface = 0; uSurface < UNIFLEX_MAX_OUT_SURFACES; uSurface++)
		{
			if (GetRegMask(psState->psSAOffsets->puValidShaderOutputs, uSurface) != 0)
			{
				psPS->aeColourResultFormat[uSurface] = eSurfaceFormat;
				psState->sShader.psPS->uEmitsPresent |= (1 << uSurface);
				uValidSurfaceCount = uSurface + 1;
			}
		}
	}

	/*
		Find the maximum array tag already used in the program.
	*/
	uMaxTag = 0;
	for (uArray = 0; uArray < psState->uIndexableTempArrayCount; uArray++)
	{
		uMaxTag = max(psState->psIndexableTempArraySizes[uArray].uTag, uMaxTag);
	}

	/*
		Grow the array of indexable temporary array sizes.
	*/
	ResizeArray(psState,
				psState->psIndexableTempArraySizes,
				(psState->uIndexableTempArrayCount + 0) * sizeof(psState->psIndexableTempArraySizes[0]),
				(psState->uIndexableTempArrayCount + 1) * sizeof(psState->psIndexableTempArraySizes[1]),
				(IMG_PVOID*)&psState->psIndexableTempArraySizes);

	/*
		Create a new indexable temporary array.
	*/
	psNewArray = &psState->psIndexableTempArraySizes[psState->uIndexableTempArrayCount];

	psNewArray->uTag = uMaxTag + 1;
	psNewArray->uSize = uValidSurfaceCount;

	psState->uIndexableTempArrayCount++;
					
	/*
		Set where the shader epilog should read the pixel shader colour output from.
	*/
	psPS->ePSOutputRegType = UFREG_TYPE_INDEXABLETEMP;
	psPS->uPSOutputRegNum = 0;
	psPS->uPSOutputRegArrayTag = uMaxTag + 1;

	/*
		Replace all references to the shader colour outputs in instructions in the input program.
	*/
	for (psInst = psProg->psHead; psInst != NULL; psInst = psInst->psILink)
	{
		IMG_UINT32 i;
		for (i = g_asInputInstDesc[psInst->eOpCode].uNumDests; i > 0; i--)
		{
			PUF_REGISTER psDest = (i == 2) ? &(psInst->sDest2) : &(psInst->sDest);
			if (psDest->u.byMask && psDest->eType == UFREG_TYPE_PSOUTPUT && psDest->uNum < UNIFLEX_MAX_OUT_SURFACES)
			{
				psDest->eType = psPS->ePSOutputRegType;
				psDest->uArrayTag = psPS->uPSOutputRegArrayTag;
			}
		}
		for (i = 0; i < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; i++)
		{
			PUF_REGISTER	psSrc = &psInst->asSrc[i];

			if (psSrc->eType == UFREG_TYPE_PSOUTPUT && psSrc->uNum < UNIFLEX_MAX_OUT_SURFACES)
			{
				psSrc->eType = psPS->ePSOutputRegType;
				psSrc->uArrayTag = psPS->uPSOutputRegArrayTag;
			}
		}
	}
}

static IMG_VOID RecordPSOutput(PINTERMEDIATE_STATE	psState,
							   PUF_REGISTER			psInputArg,
							   IMG_BOOL				bDest)
/*****************************************************************************
 FUNCTION	: RecordPSOutput

 PURPOSE	: Record information about pixel shader outputs from a source/destination
			  argument in the input program.

 PARAMETERS	: psState			- Compiler state.
			  psInputArg		- Source or destination argument.
			  bDest				- TRUE if the argument is a destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	UF_REGFORMAT		eArgFormat;
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

	/*
		Record the program uses dynamic indexing into the pixel shader outputs.
	*/
	if (psInputArg->eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
	{
		psState->uFlags2 |= USC_FLAGS2_PSOUTPUTRELATIVEADDRESSING;
	}

	/*
		Treat all 32-bits per channel formats as equivalent.
	*/
	eArgFormat = psInputArg->eFormat;
	if (eArgFormat == UF_REGFORMAT_I32 || eArgFormat == UF_REGFORMAT_U32)
	{
		eArgFormat = UF_REGFORMAT_F32;
	}

	/*
		Record the precision of the pixel shader output.
	*/
	switch (psInputArg->uNum)
	{
		case UFREG_OUTPUT_Z:
		{
			if (bDest)
			{
				psState->uFlags |= USC_FLAGS_DEPTHFEEDBACKPRESENT;
			}

			/* Record the format used by the shader for the Z result. */
			ASSERT(psPS->eZFormat == UF_REGFORMAT_UNTYPED || psPS->eZFormat == eArgFormat);
			psPS->eZFormat = eArgFormat;
			break;
		}
		case UFREG_OUTPUT_OMASK:
		{
			if (bDest)
			{
				psState->uFlags |= USC_FLAGS_OMASKFEEDBACKPRESENT;
			}

			/* Record the format used by the shader for the coverage mask result. */
			ASSERT(psPS->eOMaskFormat == UF_REGFORMAT_UNTYPED || psPS->eOMaskFormat == eArgFormat);
			psPS->eOMaskFormat = eArgFormat;
			break;
		}
		default:
		{
			ASSERT(psInputArg->uNum < UNIFLEX_MAX_OUT_SURFACES);
			if (bDest)
			{
				psState->sShader.psPS->uEmitsPresent |= (1U << psInputArg->uNum);
			}

			/* Record the format used by the shader for the colour result. */
			ASSERT(psPS->aeColourResultFormat[psInputArg->uNum] == UF_REGFORMAT_UNTYPED ||
				   psPS->aeColourResultFormat[psInputArg->uNum] == eArgFormat);
			psPS->aeColourResultFormat[psInputArg->uNum] = eArgFormat;

			/*
				Record the mask of channels ever written in the pixel shader output.
			*/
			if (bDest && psInputArg->uNum == UFREG_OUTPUT_MC0)
			{
				IMG_UINT32	uWrittenMask;

				if (psInputArg->eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
				{
					uWrittenMask = (1 << UFREG_DMASK_X_SHIFT) |
								   (1 << UFREG_DMASK_Y_SHIFT) |
								   (1 << UFREG_DMASK_Z_SHIFT) |
								   (1 << UFREG_DMASK_W_SHIFT);
				}
				else
				{
					uWrittenMask = psInputArg->u.byMask;
				}
				psPS->uInputOrderPackedPSOutputMask |= uWrittenMask;
				psPS->uHwOrderPackedPSOutputMask |= ConvertInputWriteMaskToIntermediate(psState, uWrittenMask);
			}
			break;
		}
	}
}

static IMG_VOID ExpandRelativeIndexAndMacro(PINTERMEDIATE_STATE	psState, 
											PUNIFLEX_INST		psOrigInst, 
											PINPUT_PROGRAM		psProg,
											IMG_BOOL*			abAcesedShdrOutputRanges)
/*****************************************************************************
 FUNCTION	: ExpandRelativeIndexAndMacro

 PURPOSE	: Expands instructions relatively indexed by temporary registers
			  and expands macro instruction.

 PARAMETERS	: psOrigInst		- Instruction to expand.
			  psProg			- List to hold the expanded instructions.			  
			  abAcesedShdrOutputRanges 
								- Represents Shader Output Ranges that are 
								  accessed.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PUNIFLEX_INST				psOldTail;
	PUNIFLEX_INST				psExpandedInst;
	PUNIFLEX_INST				psFirstExpandedInst;
	RELATIVE_INDEX_EXPAND_STATE	sExpandState;

	sExpandState.uLoadedChansCount = 0;

	/*
		Save the old end of the program.
	*/
	psOldTail = psProg->psTail;

	/*
		Add the input instruction to the end of the program; expanding it
		if it is a macro.
	*/
	ExpandMacro(psState, psOrigInst, psProg);

	if (psOldTail == NULL)
	{
		psFirstExpandedInst = psProg->psHead;
	}
	else
	{
		psFirstExpandedInst = psOldTail->psILink;
	}
	for (psExpandedInst = psFirstExpandedInst; psExpandedInst != NULL; psExpandedInst = psExpandedInst->psILink)
	{
		IMG_UINT32	uDestIdx;
		IMG_UINT32	uSrcIdx;

		for (uDestIdx = 0; uDestIdx < g_asInputInstDesc[psExpandedInst->eOpCode].uNumDests; uDestIdx++)
		{
			PUF_REGISTER	psDest = (uDestIdx == 0) ? &psExpandedInst->sDest : &psExpandedInst->sDest2;

			/*
				Replace relative accesses using temporary registers as indexes.
			*/
			ExpandRelativeIndex(psState, &sExpandState, psProg, psFirstExpandedInst, psExpandedInst, psDest, abAcesedShdrOutputRanges);

			/*
				Flag where a vertex shader contains dynamic indexes into its outputs.
			*/
			if (psDest->eRelativeIndex != UFREG_RELATIVEINDEX_NONE && psDest->eType == UFREG_TYPE_VSOUTPUT)
			{
				psState->uFlags |= USC_FLAGS_OUTPUTRELATIVEADDRESSING;
			}

			if (psDest->eType == UFREG_TYPE_PSOUTPUT)
			{
				RecordPSOutput(psState, psDest, IMG_TRUE /* bDest */);
			}
		}
		for (uSrcIdx = 0; uSrcIdx < g_asInputInstDesc[psExpandedInst->eOpCode].uNumSrcArgs; uSrcIdx++)
		{
			PUF_REGISTER	psSrc = &psExpandedInst->asSrc[uSrcIdx];

			/*
				Replace relative accesses using temporary registers as indexes.
			*/
			ExpandRelativeIndex(psState, &sExpandState, psProg, psFirstExpandedInst, psExpandedInst, psSrc, abAcesedShdrOutputRanges);

			/*
				Flag where a pixel shader contains dynamic indexes into the texture coordinates.
			*/
			if (psSrc->eType == UFREG_TYPE_TEXCOORD && psSrc->eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
			{
				psState->uFlags |= USC_FLAGS_INPUTRELATIVEADDRESSING;
			}

			/*
				Flag where a vertex shader contains dynamic indexes into its inputs.
			*/
			if (psSrc->eType == UFREG_TYPE_VSINPUT && psSrc->eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
			{
				psState->uFlags |= USC_FLAGS_INPUTRELATIVEADDRESSING;
			}

			/*
				Flag where a vertex shader contains dynamic indexes into its outputs.
			*/
			if (psSrc->eType == UFREG_TYPE_VSOUTPUT && psSrc->eRelativeIndex != UFREG_RELATIVEINDEX_NONE)
			{
				psState->uFlags |= USC_FLAGS_OUTPUTRELATIVEADDRESSING;
			}

			if (psSrc->eType == UFREG_TYPE_PSOUTPUT)
			{
				RecordPSOutput(psState, psSrc, IMG_FALSE /* bDest */);
			}
		}
	}
}

static IMG_UINT32 GetDestinationWriteCount(PINTERMEDIATE_STATE psState, PUNIFLEX_INST psInst)
/*********************************************************************************
 Function			: GetDestinationWriteCount

 Description		: Get the count of consecutive destination registers written
					  by an instruction.
 
 Parameters			: psState	- The intermediate state.
				      psInst	- Instruction.

 Globals Effected	: None

 Return				: The count of destinations written.
*********************************************************************************/
{
	if (psInst->eOpCode >= UFOP_FIRST_LD_OPCODE && psInst->eOpCode <= UFOP_LAST_LD_OPCODE)
	{
		if ((psState->uCompilerFlags & UF_SMPIMMCOORDOFFSETS) != 0)
		{
			IMG_UINT32					uOffsetsSource;
			PUF_REGISTER				psOffsetsSource;
			IMG_UINT32					uParam;
			UF_LDPARAM_SPECIALRETURN	eReturnMode;

			uOffsetsSource = GetSampleParametersSource(psInst->eOpCode);
			psOffsetsSource = &psInst->asSrc[uOffsetsSource];
			ASSERT(psOffsetsSource->eType == UFREG_TYPE_IMMEDIATE);
			uParam = psOffsetsSource->uNum;
			eReturnMode = (UF_LDPARAM_SPECIALRETURN)((uParam & ~UF_LDPARAM_SPECIALRETURN_CLRMSK) >> UF_LDPARAM_SPECIALRETURN_SHIFT);
			switch (eReturnMode)
			{
				case UF_LDPARAM_SPECIALRETURN_SAMPLEINFO:	return UF_LD_SAMPLEINFO_RESULT_REGISTER_COUNT;
				case UF_LDPARAM_SPECIALRETURN_RAWSAMPLES:	return UF_LD_RAWSAMPLES_MAXIMUM_RESULT_REGISTER_COUNT;
				case UF_LDPARAM_SPECIALRETURN_NONE:			return 1;
				default: imgabort();
			}
		}
	}
	return 1;
}

static IMG_VOID GetMaximumInputRegisterNumber(PINTERMEDIATE_STATE	psState,
											  PUNIFLEX_INST			psProg)
/*********************************************************************************
 Function			: GetMaximumInputRegisterNumber

 Description		: Get the maximum number of temporary registers used in
					  the input program.
 
 Parameters			: psOrigProg - The input program.
					  psState - The intermediate state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32		uInputTempRegisterCount;
	IMG_UINT32		uInputPredicateRegisterCount;
	PUNIFLEX_INST	psInst;

	/*
	  Find the maximum temporary register and predicate register used.
	*/
	uInputTempRegisterCount = 0;
	uInputPredicateRegisterCount = 0;
	for (psInst = psProg; psInst != NULL; psInst = psInst->psILink)
	{
		IMG_UINT32	uDest, uArg;

		for (uDest = 0; uDest < g_asInputInstDesc[psInst->eOpCode].uNumDests; uDest++)
		{
			PUF_REGISTER	psDest;

			psDest = (uDest == 0) ? &psInst->sDest : &psInst->sDest2;
			if (psDest->u.byMask != 0)
			{
				if (psDest->eType == UFREG_TYPE_TEMP)
				{
					IMG_UINT32	uDestinationWriteCount;

					uDestinationWriteCount = GetDestinationWriteCount(psState, psInst);
					uInputTempRegisterCount = max(psDest->uNum + uDestinationWriteCount, uInputTempRegisterCount);
				}
				if (psDest->eType == UFREG_TYPE_PREDICATE)
				{
					uInputPredicateRegisterCount = max(psDest->uNum + 1, uInputPredicateRegisterCount);
				}
			}
		}

		if (psInst->uPredicate != UF_PRED_NONE)
		{
			IMG_UINT32	uPredIdx;

			uPredIdx = (psInst->uPredicate & UF_PRED_IDX_MASK) >> UF_PRED_IDX_SHIFT;
			uInputPredicateRegisterCount = max(uPredIdx + 1, uInputPredicateRegisterCount);
		}

		for (uArg = 0; uArg < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; uArg++)
		{
			if (psInst->asSrc[uArg].eType == UFREG_TYPE_TEMP)
			{
				IMG_UINT32	uLimit;

				uLimit = 1;
				if (uArg == 1)
				{
					switch (psInst->eOpCode)
					{
						case UFOP_M4X4:
						case UFOP_M3X4:
						{
							uLimit = 4;
							break;
						}
						case UFOP_M4X3:
						case UFOP_M3X3:
						{
							uLimit = 3;
							break;
						}
						case UFOP_M3X2:
						{
							uLimit = 2;
							break;
						}
						default:
						{
							break;
						}
					}
				}

				uInputTempRegisterCount = max(psInst->asSrc[uArg].uNum + uLimit, uInputTempRegisterCount);
			}
		}
	}

	psState->uInputTempRegisterCount = uInputTempRegisterCount;
	psState->uInputPredicateRegisterCount = uInputPredicateRegisterCount;
}

static IMG_BOOL IsIncompatibleFormat(UF_REGFORMAT eFormat1, UF_REGFORMAT eFormat2)
/*********************************************************************************
 Function			: IsIncompatibleFormat

 Description		: Check whether two register formats (of the same register number)
					  are incompatible.
 
 Parameters			: eFormat1 - First format
					  eFormat2 - Second format

 Globals Effected	: None

 Return				: TRUE if the same register cannot have the two different formats
					  in the same shader.
*********************************************************************************/
{
	if ((eFormat1 == UF_REGFORMAT_F32 && eFormat2 == UF_REGFORMAT_F16) ||
		(eFormat1 == UF_REGFORMAT_F16 && eFormat2 == UF_REGFORMAT_F32) ||
		(eFormat1 == UF_REGFORMAT_I32 && eFormat2 == UF_REGFORMAT_I16) ||
		(eFormat1 == UF_REGFORMAT_I16 && eFormat2 == UF_REGFORMAT_I32) ||
		(eFormat1 == UF_REGFORMAT_U32 && eFormat2 == UF_REGFORMAT_U16) ||
		(eFormat1 == UF_REGFORMAT_U32 && eFormat2 == UF_REGFORMAT_U8) ||
		(eFormat1 == UF_REGFORMAT_U16 && eFormat2 == UF_REGFORMAT_U32) ||
		(eFormat1 == UF_REGFORMAT_U16 && eFormat2 == UF_REGFORMAT_U8) ||
		(eFormat1 == UF_REGFORMAT_U8 && eFormat2 == UF_REGFORMAT_U32) ||
		(eFormat1 == UF_REGFORMAT_U8 && eFormat2 == UF_REGFORMAT_U16))
	{
		return IMG_TRUE;
	}
	return IMG_FALSE;
}

static IMG_VOID CheckConsistentRegFormats(PINTERMEDIATE_STATE	psState,
										  PUNIFLEX_INST			psProg)
/*********************************************************************************
 Function			: CheckConsistentRegFormats

 Description		: Check that the formats of registers are consistent, i.e the
					  same register is not used with two different formats.
 
 Parameters			: psOrigProg - The input program.
					  psState - The intermediate state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUNIFLEX_INST	psInst;
	UF_REGFORMAT*	peRegFormats;

	peRegFormats = UscAlloc(psState, sizeof(UF_REGFORMAT) * psState->uInputTempRegisterCount);
	memset(peRegFormats, UF_REGFORMAT_INVALID, sizeof(UF_REGFORMAT) * psState->uInputTempRegisterCount);

	for (psInst = psProg; psInst != NULL; psInst = psInst->psILink)
	{
		IMG_UINT32	uDest, uArg;

		for (uDest = 0; uDest < g_asInputInstDesc[psInst->eOpCode].uNumDests; uDest++)
		{
			PUF_REGISTER	psDest;

			psDest = (uDest == 0) ? &psInst->sDest : &psInst->sDest2;
			if (psDest->u.byMask != 0)
			{
				if (psDest->eType == UFREG_TYPE_TEMP)
				{
					ASSERT(psDest->uNum < psState->uInputTempRegisterCount);

					if ((peRegFormats[psDest->uNum] != UF_REGFORMAT_INVALID) &&
						IsIncompatibleFormat(peRegFormats[psDest->uNum], psDest->eFormat))
					{
						UscFail(psState, UF_ERR_INVALID_DST_REG, "Using register with inconcistent formatting");
					}

					peRegFormats[psDest->uNum] = psDest->eFormat;
				}
			}
		}

		for (uArg = 0; uArg < g_asInputInstDesc[psInst->eOpCode].uNumSrcArgs; uArg++)
		{
			if (psInst->asSrc[uArg].eType == UFREG_TYPE_TEMP)
			{
				ASSERT(psInst->asSrc[uArg].uNum < psState->uInputTempRegisterCount);

				if ((peRegFormats[psInst->asSrc[uArg].uNum] != UF_REGFORMAT_INVALID) && 
					IsIncompatibleFormat(peRegFormats[psInst->asSrc[uArg].uNum], psInst->asSrc[uArg].eFormat))
				{
					UscFail(psState, UF_ERR_INVALID_DST_REG, "Using register with inconcistent formatting");
				}

				peRegFormats[psInst->asSrc[uArg].uNum] = psInst->asSrc[uArg].eFormat;
			}
		}
	}
	
	UscFree(psState, peRegFormats);
}

static IMG_VOID SetupVertexShaderOutputs(PINTERMEDIATE_STATE psState)
{
	IMG_UINT32			uInitialNumShaderOutputs;
	PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;

	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY)
	{
		uInitialNumShaderOutputs = psVS->uVertexShaderNumOutputs;
	}
	else
	{
		uInitialNumShaderOutputs = USC_MAX_SHADER_OUTPUTS;
	}

	/*
		For geometry shaders the outputs are implicitly indexed.
	*/
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY)
	{
		psState->uFlags |= USC_FLAGS_OUTPUTRELATIVEADDRESSING;
	}

	/*
		If dynamic indexing into the vertex shader outputs is in use then create
		an array representing the registers which can be accessed by the index.
	*/
	if (psState->uFlags & USC_FLAGS_OUTPUTRELATIVEADDRESSING)
	{
		psVS->uVertexShaderOutputsArrayIdx = 
			AddNewRegisterArray(psState, 
								ARRAY_TYPE_VERTEX_SHADER_OUTPUT, 
								USC_UNDEF /* uArrayNum */, 
								LONG_SIZE /* uChannelsPerDword */,
								uInitialNumShaderOutputs);

		psVS->uVertexShaderOutputsFirstRegNum =
			psState->apsVecArrayReg[psVS->uVertexShaderOutputsArrayIdx]->uBaseReg;
	}
	else
	{
		psVS->uVertexShaderOutputsArrayIdx = USC_UNDEF;

		/*
			Allocate some temporary registers to represent the vertex shader outputs.
		*/
		psVS->uVertexShaderOutputsFirstRegNum = GetNextRegisterCount(psState, uInitialNumShaderOutputs);
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) && !(psState->uFlags & USC_FLAGS_OUTPUTRELATIVEADDRESSING))
	{
		psVS->auVSOutputToVecReg =
			UscAlloc(psState, sizeof(psVS->auVSOutputToVecReg[0]) * uInitialNumShaderOutputs);
		memset(psVS->auVSOutputToVecReg, 0xFF, sizeof(psVS->auVSOutputToVecReg[0]) * uInitialNumShaderOutputs);
	}
	else
	{
		psVS->auVSOutputToVecReg = NULL;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
}

static IMG_VOID SetupVertexShaderInputs(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: SetupVertexShaderInputs

 PURPOSE	: Allocate variables to represent the vertex shader inputs in
			  the intermediate code..

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32			uNumInputs;
	PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;
	PFIXED_REG_DATA		psOut;
	IMG_UINT32			uRegIdx;
	IMG_UINT32			uVSInputsFirstRegNum;
	IMG_UINT32			uVSInputsArrayIdx;
	IMG_UINT32			uChannelsPerDword;

	if (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_VERTEX)
	{
		/*
			For geometry shaders the input registers can never be modified
			by the shader (so they can't be reused for temporary data). So
			represent them as a special, constant register type.
		*/
		ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY);
		return;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		/*
			Represent the vertex shader inputs by vec4 registers at this
			point.
		*/
		uNumInputs = EURASIA_USE_PRIMATTR_BANK_SIZE / SCALAR_REGISTERS_PER_F32_VECTOR;
		uChannelsPerDword = F32_CHANNELS_PER_SCALAR_REGISTER;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		uNumInputs = EURASIA_USE_PRIMATTR_BANK_SIZE;
		uChannelsPerDword = LONG_SIZE;
	}

	/*
		If dynamic indexing into the vertex shader inputs is in use then create
		an array representing the registers which can be accessed by the index.
	*/
	if (psState->uFlags & USC_FLAGS_INPUTRELATIVEADDRESSING)
	{
		uVSInputsArrayIdx = 
			AddNewRegisterArray(psState, 
								ARRAY_TYPE_VERTEX_SHADER_INPUT, 
								USC_UNDEF /* uArrayNum */, 
								uChannelsPerDword,
								uNumInputs);

		uVSInputsFirstRegNum =
			psState->apsVecArrayReg[uVSInputsArrayIdx]->uBaseReg;
	}
	else
	{
		uVSInputsArrayIdx = USC_UNDEF;
		uVSInputsFirstRegNum = GetNextRegisterCount(psState, uNumInputs);
	}

	/*
		Create an entry in the array of fixed registers to represent the vertex shader inputs. 
		Map the vertex shader inputs to pa0 - paN in the register allocator.
	*/
#ifdef NON_F32_VERTEX_INPUT
	psVS->uVertexShaderInputCount = 1;
	psVS->aVertexShaderInputsFixedRegs[0] = psOut = 
#else
	psVS->psVertexShaderInputsFixedReg = psOut = 
#endif
			AddFixedReg(psState, 
					IMG_TRUE /* bPrimary */,
					IMG_FALSE /* bLiveAtShaderEnd */,
					USEASM_REGTYPE_PRIMATTR,
					0 /* uPhysicalRegNum */,
					uNumInputs /* uConsecutiveRegsCount */);

	/*
		Allocate memory to store the channels live in each vertex shader input at 
		the start of the main function. 
	*/
	psOut->puUsedChans = 
		UscAlloc(psState, UINTS_TO_SPAN_BITS(EURASIA_USE_PRIMATTR_BANK_SIZE * CHANS_PER_REGISTER) * sizeof(IMG_UINT32));

	/*
		Link the entry to the data structure representing the vertex shader inputs as a dynamically
		indexed array (if it exists).
	*/
	psOut->uRegArrayIdx = uVSInputsArrayIdx;
	psOut->uRegArrayOffset = 0;

	psOut->uVRegType = USEASM_REGTYPE_TEMP;
	for (uRegIdx = 0; uRegIdx < uNumInputs; uRegIdx++)
	{
		psOut->auVRegNum[uRegIdx] = uVSInputsFirstRegNum + uRegIdx;
		psOut->aeVRegFmt[uRegIdx] = UF_REGFORMAT_F32;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		Treat vertex shader inputs as vector sized registers.
	*/
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		psOut->bVector = IMG_TRUE;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
}

IMG_INTERNAL
IMG_VOID ConvertPixelShaderResultArg(PINTERMEDIATE_STATE	psState, 
									 PUF_REGISTER			psInputArg, 
									 PARG					psArg)
/*****************************************************************************
 FUNCTION	: ConvertPixelShaderResultArg

 PURPOSE	: Set up an intermediate format source or destination argument to
			  refer to a pixel shader result.

 PARAMETERS	: psState			- Compiler state.
			  psInputArg		- Input format pixel shader result.
			  psArg				- Intermediate format source or destination.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PPIXELSHADER_STATE	psPS;
	UF_REGFORMAT		eResultFormat;

	ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL);
	psPS = psState->sShader.psPS;

	eResultFormat = psInputArg->eFormat;
	/*
		Treat all 32-bits per channel formats as equivalent.
	*/
	if (eResultFormat == UF_REGFORMAT_I32 || eResultFormat == UF_REGFORMAT_U32)
	{
		eResultFormat = UF_REGFORMAT_F32;
	}

	psArg->uType = USEASM_REGTYPE_TEMP;
	if (psInputArg->uNum == UFREG_OUTPUT_Z)
	{
		ASSERT(psPS->eZFormat == eResultFormat);
		psArg->uNumber = psPS->uNativeZTempRegNum;
	}
	else if (psInputArg->uNum == UFREG_OUTPUT_OMASK)
	{
		ASSERT(psPS->eOMaskFormat == eResultFormat);
		psArg->uNumber = psPS->uNativeOMaskTempRegNum;
	}
	else
	{
		ASSERT(psInputArg->uNum < UNIFLEX_MAX_OUT_SURFACES);
		ASSERT(psPS->aeColourResultFormat[psInputArg->uNum] == eResultFormat);
		psArg->uNumber = psPS->uNativeColourResultBaseTempRegNum + psInputArg->uNum * CHANNELS_PER_INPUT_REGISTER;
	}
}

static IMG_BOOL SetupIterations(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: SetupIterations

 PURPOSE	: Setup iterated values.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: IMG_FALSE on error, IMG_TRUE otherwise
*****************************************************************************/
{
	IMG_UINT32			i;
	IMG_UINT32			uIterationResultSize;
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;
	IMG_UINT32			uChannelsPerDword;

	/*
		Initialize the count of registers written with pixel shader
		inputs.
	*/
	psPS->uIterationSize = 0;
	InitializeList(&psPS->sPixelShaderInputs);
	psPS->uNrPixelShaderInputs = 0;

	/*
		Initialize the number of PDS constants available in the primary PDS program.
	*/
	psPS->uNumPDSConstantsAvailable = EURASIA_PDS_DATASTORE_BANKCOUNT * EURASIA_PDS_DATASTORE_CONSTANTCOUNT;

	/*
		Subtract constants reserved by the driver.
	*/
	ASSERT(psPS->uNumPDSConstantsAvailable >= psState->psSAOffsets->uNumPDSPrimaryConstantsReserved); 
	psPS->uNumPDSConstantsAvailable -= psState->psSAOffsets->uNumPDSPrimaryConstantsReserved;

	/*
		Calculate the number of PDS constants required by a non-dependent texture sample. This is space
		for the iteration (DOUTI) command; space for the texture state and any padding.
	*/
	psPS->uNumPDSConstantsPerTextureSample = psState->psTargetFeatures->ui32TextureStateSize;
	psPS->uNumPDSConstantsPerTextureSample += psState->psTargetFeatures->ui32IterationStateSize;
	if (psState->psTargetFeatures->ui32TextureStateSize > 3)
	{
		psPS->uNumPDSConstantsPerTextureSample++;
	}

	/*
		If dynamic indexing into the texture coordinates is in use then create
		an array representing for indexable range.
	*/
	psPS->uTextureCoordinateArrayCount = psState->psSAOffsets->sShaderInputRanges.uRangesCount;
	psPS->asTextureCoordinateArrays = 
		UscAlloc(psState, psPS->uTextureCoordinateArrayCount * sizeof(psPS->asTextureCoordinateArrays[0]));


	/*
		How many intermediate registers are needed to hold the result of an F32 iteration?
	*/
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if (psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34)
	{
		/*
			One vector register.
		*/
		uIterationResultSize = 1;
		uChannelsPerDword = F32_CHANNELS_PER_SCALAR_REGISTER;
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		/*
			Four scalar registers.
		*/
		uIterationResultSize = SCALAR_REGISTERS_PER_F32_VECTOR;
		uChannelsPerDword = LONG_SIZE;
	}

	for (i = 0; i < psState->psSAOffsets->sShaderInputRanges.uRangesCount; i++)
	{	
		PUNIFLEX_RANGE				psRange = &psState->psSAOffsets->sShaderInputRanges.psRanges[i];
		IMG_UINT32					uArraySize;
		PUSC_VEC_ARRAY_REG			psTextureCoordinateArray;
		PTEXTURE_COORDINATE_ARRAY	psTCArray = &psPS->asTextureCoordinateArrays[i];
		IMG_UINT32					uRegArrayIdx;
		IMG_UINT32					uCoordinate;

		uArraySize = psRange->uRangeEnd - psRange->uRangeStart;
		uRegArrayIdx = AddNewRegisterArray(psState, 
										   ARRAY_TYPE_TEXTURE_COORDINATE, 
										   USC_UNDEF /* uArrayNum */, 
										   uChannelsPerDword,
										   uArraySize * uIterationResultSize);

		psTCArray->uRegArrayIdx = uRegArrayIdx;

		psTCArray->apsIterations = UscAlloc(psState, sizeof(psTCArray->apsIterations[0]) * uArraySize);
		memset(psTCArray->apsIterations, 0, sizeof(psTCArray->apsIterations[0]) * uArraySize);

		psTCArray->psRange = psRange;

		psTextureCoordinateArray = psState->apsVecArrayReg[uRegArrayIdx];
		psTextureCoordinateArray->u.psTextureCoordinateArray = psTCArray;

		for (uCoordinate = 0; uCoordinate < uArraySize; uCoordinate++)
		{
			PPIXELSHADER_INPUT	psElement;

			/*
				Add the iteration 
			*/
			psElement = AddIteratedValue(psState, 
										 UNIFLEX_ITERATION_TYPE_TEXTURE_COORDINATE,
										 psRange->uRangeStart + uCoordinate,
										 UNIFLEX_TEXLOAD_FORMAT_F32, 
										 uIterationResultSize,
										 psTextureCoordinateArray->uBaseReg + uCoordinate * uIterationResultSize);

			psElement->psFixedReg->uRegArrayIdx = uRegArrayIdx;
			psElement->psFixedReg->uRegArrayOffset = uCoordinate * uIterationResultSize;

			psElement->uFlags |= PIXELSHADER_INPUT_FLAG_PART_OF_ARRAY;
			psTCArray->apsIterations[uCoordinate] = psElement;
		}
	}

	return IMG_TRUE;
}

static
IMG_VOID SetScalarDest(PINTERMEDIATE_STATE	psState, 
					   PINST				psPackInst, 
					   IMG_UINT32			uDestRegNum, 
					   UF_REGFORMAT			eDestFormat, 
					   IMG_UINT32			uChan)
/*****************************************************************************
 FUNCTION	: SetScalarDest

 PURPOSE	: Set the destination of an instruction to write into a particular
			  channel of a vector.

 PARAMETERS	: psState			- Compiler state.
			  psPackInst		- Instruction to set the destination for.
			  uDestRegNum		- Register number of the start of the vector.
			  eDestFormat		- Format of the vector.
			  uChan				- Channel to set.

 RETURNS	: Nothing.
*****************************************************************************/
{
	if (eDestFormat == UF_REGFORMAT_F32)
	{
		SetDest(psState, psPackInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uDestRegNum + uChan, UF_REGFORMAT_F32);
	}
	else
	{
		IMG_UINT32	uByteOffset;

		ASSERT(eDestFormat == UF_REGFORMAT_F16);

		SetDest(psState, 
				psPackInst, 
				0 /* uDestIdx */, 
				USEASM_REGTYPE_TEMP, 
				uDestRegNum + (uChan / F16_CHANNELS_PER_SCALAR_REGISTER),
				UF_REGFORMAT_F16);

		uByteOffset = (uChan % F16_CHANNELS_PER_SCALAR_REGISTER) * WORD_SIZE;
		psPackInst->auDestMask[0] = USC_XY_CHAN_MASK << uByteOffset;
	}
}

static
IMG_VOID ConvertPixelShaderResult(PINTERMEDIATE_STATE	psState,
								  PCODEBLOCK			psBlock,
								  IMG_UINT32			uSrcRegNum,
								  IMG_UINT32			uDestRegNum,
								  UF_REGFORMAT			eSrcFormat,
								  UF_REGFORMAT			eDestFormat,
								  #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
								  IMG_BOOL				bDestIsVector,
								  #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								  IMG_UINT32			uChanMask)
/****************************************************************************
 FUNCTION	: ConvertPixelShaderResults

 PURPOSE	: Insert instructions to convert from the format of a pixel shader
			  output as written by the shader to the format expected by
			  the hardware.

 PARAMETERS	: psState			- Compiler state.
			  psBlock			- Block to insert the instructions into.
			  uSrcRegNum		- Start of the intermediate registers containing
								the output in the shader format.
			  uDestRegNum		- Start of the intermediate registers to write the
								converted output into.
			  eSrcFormat		- Format of the output as written by the shader.
			  eDestFormat		- Format to convert to.
			  bDestIsVector		-
			  uChanMask			- 

 RETURNS	: Nothing.
*****************************************************************************/
{
	ASSERT(eDestFormat == UF_REGFORMAT_F16 || eDestFormat == UF_REGFORMAT_F32);

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
	{
		PINST		psPackInst;
		IMG_UINT32	uPackDestRegNum;
		IMG_UINT32	uPackSwizzle;
		IMG_UINT32	uPackDestMask;

		if (bDestIsVector)
		{
			IMG_UINT32	uValidChanCount;
			IMG_UINT32	uChan;

			uPackDestRegNum = uDestRegNum;

			uPackSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
			uValidChanCount = 0;
			for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
			{
				if ((uChanMask & (1U << uChan)) != 0)
				{
					uPackSwizzle = SetChan(uPackSwizzle, uValidChanCount, g_aeChanToSwizzleSel[uChan]);
					uValidChanCount++;
				}
			}
			uPackDestMask = (1U << uValidChanCount) - 1;
		}
		else
		{
			uPackDestRegNum = GetNextRegister(psState);
			uPackSwizzle = USEASM_SWIZZLE(X, Y, Z, W);
			uPackDestMask = uChanMask;
		}

		psPackInst = AllocateInst(psState, NULL /* psSrcLineInst */);
		switch (eSrcFormat)
		{
			case UF_REGFORMAT_U8: 
			{
				SetOpcode(psState, psPackInst, IVPCKFLTU8); 
				psPackInst->u.psVec->bPackScale = IMG_TRUE;
				break;
			}
			case UF_REGFORMAT_C10: 
			{
				SetOpcode(psState, psPackInst, IVPCKFLTC10); 
				psPackInst->u.psVec->bPackScale = IMG_TRUE;
				break;
			}
			case UF_REGFORMAT_F16:
			case UF_REGFORMAT_F32:
			{
				SetOpcode(psState, psPackInst, IVMOV); 
				break;
			}
			default: imgabort();
		}

		SetDest(psState, psPackInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uPackDestRegNum, eDestFormat);
		psPackInst->auDestMask[0] = uPackDestMask;

		SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSrcRegNum, eSrcFormat);
		psPackInst->u.psVec->aeSrcFmt[0] = eSrcFormat;

		if (eSrcFormat == UF_REGFORMAT_U8 || eSrcFormat == UF_REGFORMAT_C10)
		{
			psPackInst->u.psVec->auSwizzle[0] = GetInputToU8C10IntermediateSwizzle(psState);
		}
		else
		{
			psPackInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
		}
		psPackInst->u.psVec->auSwizzle[0] = CombineSwizzles(psPackInst->u.psVec->auSwizzle[0], uPackSwizzle);

		AppendInst(psState, psBlock, psPackInst);

		if (!bDestIsVector)
		{
			IMG_UINT32	uChan;
			IMG_UINT32	uValidChanCount;

			uValidChanCount = 0;
			for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
			{
				if ((uChanMask & (1U << uChan)) != 0)
				{
					PINST	psUnpackInst;

					psUnpackInst = AllocateInst(psState, NULL /* psSrcLineInst */);
					SetOpcode(psState, psUnpackInst, IUNPCKVEC);
					SetScalarDest(psState, psUnpackInst, uDestRegNum, eDestFormat, uValidChanCount);
					SetSrc(psState, psUnpackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uPackDestRegNum, eDestFormat);
					psUnpackInst->u.psVec->auSwizzle[0] = g_auReplicateSwizzles[uChan];
					psUnpackInst->u.psVec->aeSrcFmt[0] = eDestFormat;
					AppendInst(psState, psBlock, psUnpackInst);

					uValidChanCount++;
				}
			}
		}
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	{
		IMG_UINT32	uChan;
		IMG_UINT32	uValidChanCount;
		static const IOPCODE	aeConversions[UF_REGFORMAT_F16 + 1][UF_REGFORMAT_U8 + 1] =
		{
			/* UF_REGFORMAT_F32		UF_REGFORMAT_F16		UF_REGFORMAT_C10	UF_REGFORMAT_U8 */
			{	IMOV,				IUNPCKF32F16,			IUNPCKF32C10,		IUNPCKF32U8},		/* UF_REGFORMAT_F32 */
			{	IPCKF16F32,			IPCKF16F16,				IUNPCKF16C10,		IUNPCKF16U8},		/* UF_REGFORMAT_F16 */
		};

		uValidChanCount = 0;
		for (uChan = 0; uChan < VECTOR_LENGTH; uChan++)
		{
			if ((uChanMask & (1U << uChan)) != 0)
			{
				PINST	psPackInst;

				psPackInst = AllocateInst(psState, NULL /* psSrcLineInst */);

				ASSERT(eDestFormat < (sizeof(aeConversions) / sizeof(aeConversions[0])));
				ASSERT(eSrcFormat < (sizeof(aeConversions[0]) / sizeof(aeConversions[0][0])));

				SetOpcode(psState, psPackInst, aeConversions[eDestFormat][eSrcFormat]);

				SetScalarDest(psState, psPackInst, uDestRegNum, eDestFormat, uValidChanCount);

				if (eSrcFormat == UF_REGFORMAT_U8 || eSrcFormat == UF_REGFORMAT_C10)
				{
					ASSERT(g_psInstDesc[psPackInst->eOpcode].eType == INST_TYPE_PCK);
					psPackInst->u.psPck->bScale = IMG_TRUE;
					
					SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSrcRegNum, eSrcFormat);
					SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, ConvertInputChannelToIntermediate(psState, uChan));
				}
				else if (eSrcFormat == UF_REGFORMAT_F16)
				{
					SetSrc(psState, 
						   psPackInst, 
						   0 /* uSrcIdx */, 
						   USEASM_REGTYPE_TEMP, 
						   uSrcRegNum + (uChan / F16_CHANNELS_PER_SCALAR_REGISTER), 
						   eSrcFormat);
					SetPCKComponent(psState, psPackInst, 0 /* uArgIdx */, (uChan % F16_CHANNELS_PER_SCALAR_REGISTER) * WORD_SIZE);
				}
				else
				{
					ASSERT(eSrcFormat == UF_REGFORMAT_F32);
					SetSrc(psState, psPackInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uSrcRegNum + uChan, eSrcFormat);
				}
				if (psPackInst->uArgumentCount > 1)
				{
					ASSERT(psPackInst->uArgumentCount == 2);
					SetSrc(psState, 
						   psPackInst, 
						   1 /* uSrcIdx */, 
						   USEASM_REGTYPE_IMMEDIATE, 
						   0 /* uNewSrcNumber */, 
						   UF_REGFORMAT_F32);
				}

				AppendInst(psState, psBlock, psPackInst);

				uValidChanCount++;
			}
		}
	}
}

static
IMG_VOID ConvertPixelShaderResults(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: ConvertPixelShaderResults

 PURPOSE	: Insert instructions to convert from the format of pixel shader
			  outputs as written by the shader to the format expected by
			  the hardware.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

	if ((psState->uFlags & USC_FLAGS_DEPTHFEEDBACKPRESENT) != 0)
	{
		/*
			Convert the Z result to F32.	
		*/
		ConvertPixelShaderResult(psState, 
								 psState->psMainProg->sCfg.psExit,
								 psPS->uNativeZTempRegNum, 
								 psPS->uZTempRegNum, 
								 psPS->eZFormat, 
								 UF_REGFORMAT_F32,
								 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
								 IMG_TRUE /* bVector */,
								 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								 USC_X_CHAN_MASK);
	}

	if ((psState->uFlags & USC_FLAGS_OMASKFEEDBACKPRESENT) != 0)
	{
		/*
			Convert the coverage mask result to F32.
		*/
		ConvertPixelShaderResult(psState, 
								 psState->psMainProg->sCfg.psExit,
								 psPS->uNativeOMaskTempRegNum, 
								 psPS->uOMaskTempRegNum, 
								 psPS->eOMaskFormat,
								 UF_REGFORMAT_F32,
								 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
								 IMG_FALSE /* bVector */,
								 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
								 USC_X_CHAN_MASK);
	}

	/*
		Convert the colour result to the format requested by the caller.
	*/
	if (psState->psSAOffsets->ePackDestFormat != UF_REGFORMAT_U8)
	{
		IMG_UINT32	uSurf;

		for (uSurf = 0; uSurf < UNIFLEX_MAX_OUT_SURFACES; uSurf++)
		{
			IMG_UINT32	uSurfStart;
			IMG_UINT32	uValidChansFromSurf;

			if ((psPS->uEmitsPresent & (1U << uSurf)) == 0)
			{
				continue;
			}

			uSurfStart = uSurf * CHANNELS_PER_INPUT_REGISTER;
			uValidChansFromSurf = GetRange(psState->psSAOffsets->puValidShaderOutputs, 
										   uSurfStart + CHANNELS_PER_INPUT_REGISTER - 1, 
										   uSurfStart);
			ConvertPixelShaderResult(psState,
									 psState->psMainProg->sCfg.psExit,
									 psPS->uNativeColourResultBaseTempRegNum + uSurf * CHANNELS_PER_INPUT_REGISTER,
									 psPS->uColourResultBaseTempRegNum + uSurf * CHANNELS_PER_INPUT_REGISTER,
									 psPS->aeColourResultFormat[uSurf],
									 psState->psSAOffsets->ePackDestFormat,
									 #if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
									 IMG_TRUE /* bVector */,
									 #endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
									 uValidChansFromSurf);
		}
	}
}

static
IMG_VOID SetupPixelShader(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: SetupPixelShader

 PURPOSE	: Initialize compiler state specific to pixel shaders.

 PARAMETERS	: psState			- Compiler state.

 RETURNS	: Nothing.
*****************************************************************************/
{
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;
	IMG_UINT32			uColourResultSize;
	
	/* Setup initial iterations. */
	SetupIterations(psState);

	/*
		Allocate temporary registers to hold the shader Z result.
	*/
	psPS->uNativeZTempRegNum = GetNextRegister(psState);
	psPS->uZTempRegNum = GetNextRegister(psState);

	/*
		Allocate temporary registers to hold the shader coverage mask result.
	*/
	psPS->uNativeOMaskTempRegNum = GetNextRegister(psState);
	psPS->uOMaskTempRegNum = GetNextRegister(psState);
	
	/*
		Allocate temporary registers to hold the shader colour result.
	*/
	uColourResultSize = UNIFLEX_MAX_OUT_SURFACES * CHANNELS_PER_INPUT_REGISTER;
	psPS->uNativeColourResultBaseTempRegNum = GetNextRegisterCount(psState, uColourResultSize);
	psPS->uColourResultBaseTempRegNum = GetNextRegisterCount(psState, uColourResultSize);
}

static
IMG_VOID SetValidSahderOutputsData(
									PINTERMEDIATE_STATE psState, 
									const IMG_BOOL* abAcesedShdrOutputRanges)
/*********************************************************************************
 Function			: SetValidSahderOutputsData

 Description		: Sets valid shader outputs and valid output vertex ranges
					  out of given output vertex ranges.

 Parameters			: psState			- The current compilation context
					  abAcesedShdrOutputRanges 
										- Shader Output Ranges that are actually 
										accessed in the Input Program.

 Return				: Nothing.
*********************************************************************************/
{
	{
		IMG_UINT32	uDwordIdx;
		for(uDwordIdx = 0; uDwordIdx < USC_SHADER_OUTPUT_MASK_DWORD_COUNT; uDwordIdx++)
		{
			psState->puPackedShaderOutputs[uDwordIdx] = psState->psSAOffsets->puValidShaderOutputs[uDwordIdx];
		}
	}
	{
		IMG_UINT32 	uRangesCount = (psState->psSAOffsets->sShaderOutPutRanges.uRangesCount);
		psState->sValidShaderOutPutRanges.psRanges = NULL;
		psState->sValidShaderOutPutRanges.uRangesCount = 0;
		if(((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX) || (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY)) &&
			(uRangesCount > 0))
		{
			IMG_UINT32	uRangeIdx;
			IMG_UINT32	uValidRangesCount = 0;
			/*
				Indexes of valid ranges.
			*/
			IMG_UINT32*	auIdxsOfValidRanges = UscAlloc(psState, sizeof(auIdxsOfValidRanges[0]) * uRangesCount);
			for(uRangeIdx = 0; uRangeIdx < uRangesCount; uRangeIdx++)
			{				
				IMG_BOOL	bValidRange = IMG_FALSE;
				if(abAcesedShdrOutputRanges[uRangeIdx] == IMG_TRUE)
				{
					IMG_UINT32	uInRangeIdx;
					IMG_UINT32	uRangeStart = psState->psSAOffsets->sShaderOutPutRanges.psRanges[uRangeIdx].uRangeStart;
					IMG_UINT32	uRangeEnd = psState->psSAOffsets->sShaderOutPutRanges.psRanges[uRangeIdx].uRangeEnd;
					for(uInRangeIdx=uRangeStart; (uInRangeIdx < uRangeEnd) && (bValidRange == IMG_FALSE); uInRangeIdx++)
					{
						if(GetBit(psState->psSAOffsets->puValidShaderOutputs, uInRangeIdx))
						{
							bValidRange = IMG_TRUE;
						}
					}
				}
				if(bValidRange)
				{
					auIdxsOfValidRanges[uValidRangesCount] = uRangeIdx;
					uValidRangesCount++;
				}
			}			
			if(uValidRangesCount > 0)
			{
				psState->sValidShaderOutPutRanges.psRanges = UscAlloc(psState, sizeof(psState->sValidShaderOutPutRanges.psRanges[0]) * uValidRangesCount);
				for(uRangeIdx = 0; uRangeIdx < uValidRangesCount; uRangeIdx++)
				{
					psState->sValidShaderOutPutRanges.psRanges[uRangeIdx] = psState->psSAOffsets->sShaderOutPutRanges.psRanges[auIdxsOfValidRanges[uRangeIdx]];
					{
						IMG_UINT32	uInRangeIdx;
						IMG_UINT32	uRangeStart = psState->sValidShaderOutPutRanges.psRanges[uRangeIdx].uRangeStart;
						IMG_UINT32	uRangeEnd = psState->sValidShaderOutPutRanges.psRanges[uRangeIdx].uRangeEnd;
						for(uInRangeIdx=uRangeStart; uInRangeIdx < uRangeEnd; uInRangeIdx++)
						{
							SetBit(psState->puPackedShaderOutputs, uInRangeIdx, 1);
						}
					}
				}
			}
			psState->sValidShaderOutPutRanges.uRangesCount = uValidRangesCount;
			UscFree(psState, auIdxsOfValidRanges);
		}
	}	
	if(psState->sValidShaderOutPutRanges.uRangesCount > 0)
	{
		IMG_UINT32	uRangeIdx;
		psState->psPackedValidOutPutRanges = UscAlloc(psState, sizeof(psState->sValidShaderOutPutRanges.psRanges[0]) * psState->sValidShaderOutPutRanges.uRangesCount);
		for(uRangeIdx = 0; uRangeIdx < (psState->sValidShaderOutPutRanges.uRangesCount); uRangeIdx++)
		{
			IMG_UINT32	uPackedRangeStart = 0;
			IMG_UINT32	uInRangeIdx;
			IMG_UINT32	uOriginalRangeStart = psState->sValidShaderOutPutRanges.psRanges[uRangeIdx].uRangeStart;
			IMG_UINT32	uOriginalRangeEnd = psState->sValidShaderOutPutRanges.psRanges[uRangeIdx].uRangeEnd;

			for (uInRangeIdx = 0; uInRangeIdx < uOriginalRangeStart; uInRangeIdx++)
			{
				if (GetBit(psState->puPackedShaderOutputs, uInRangeIdx))
				{
					uPackedRangeStart++;
				}
			}
			psState->psPackedValidOutPutRanges[uRangeIdx].uRangeStart = uPackedRangeStart;
			psState->psPackedValidOutPutRanges[uRangeIdx].uRangeEnd = (uPackedRangeStart + (uOriginalRangeEnd - uOriginalRangeStart));
		}
	}


	/*
		Count the number of vertex/geometry shader outputs.
	*/
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY)
	{
		IMG_UINT32			uOutputIdx;
		PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;

		psVS->uVertexShaderNumOutputs = 0;
		for (uOutputIdx = 0; uOutputIdx < USC_MAX_GS_OUTPUTS; uOutputIdx++)
		{
			if (GetBit(psState->psSAOffsets->puValidShaderOutputs, uOutputIdx))
			{
				psVS->uVertexShaderNumOutputs++;
			}
		}
		{
			IMG_UINT32 uAlign = (psVS->uVertexShaderNumOutputs % 4);
			if(uAlign != 0)
			{
				psVS->uVertexShaderNumOutputs += (4 - uAlign);
			}
		}
		if(psVS->uVertexShaderNumOutputs > EURASIA_OUTPUT_PARTITION_SIZE)
		{
			if(psVS->uVertexShaderNumOutputs > (EURASIA_OUTPUT_PARTITION_SIZE * 2))
			{
				USC_ERROR(UF_ERR_GENERIC, "Output Vertex size is more than 2 output partitions");
			}
			if(psState->psSAOffsets->uOutPutBuffersCount < 2)
			{
				USC_ERROR(UF_ERR_GENERIC, "At least 2 Output Partitions are required to accmodate one Vertex Output");
			}
			(psState->uFlags2) |= USC_FLAGS2_TWO_PARTITION_MODE;
		}
	}
}

static
IMG_INT32 CmpFixedReg(PUSC_LIST_ENTRY psFixedReg1ListEntry, PUSC_LIST_ENTRY psFixedReg2ListEntry)
/*****************************************************************************
 FUNCTION	: CmpFixedReg

 PURPOSE	: Comparison function for sorting the list of fixed registers.

 PARAMETERS	: psFixedReg1ListEntry		- Two fixed registers to compare.
			  psFixedReg2ListEntry

 RETURNS	: -1 if the first fixed register is less than the second.
			   0 if the two fixed registers are the same.
			   1 if the first fixed register is greater than the second.
*****************************************************************************/
{
	PFIXED_REG_DATA	psFixedReg1 = IMG_CONTAINING_RECORD(psFixedReg1ListEntry, PFIXED_REG_DATA, sListEntry);
	PFIXED_REG_DATA	psFixedReg2 = IMG_CONTAINING_RECORD(psFixedReg2ListEntry, PFIXED_REG_DATA, sListEntry);

	if (
			(psFixedReg1->bLiveAtShaderEnd && !psFixedReg2->bLiveAtShaderEnd) ||
			(!psFixedReg1->bLiveAtShaderEnd && psFixedReg2->bLiveAtShaderEnd)
	   )
	{
		if (psFixedReg2->bLiveAtShaderEnd)
		{
			return -1;
		}
		else
		{
			return 1;
		}
	} 
	if (psFixedReg1->sPReg.uType != psFixedReg2->sPReg.uType)
	{
		return (IMG_INT32)(psFixedReg1->sPReg.uType - psFixedReg2->sPReg.uType);
	}
	if (psFixedReg1->sPReg.uNumber == psFixedReg2->sPReg.uNumber)
	{
		return 0;
	}
	if (psFixedReg1->sPReg.uNumber < psFixedReg2->sPReg.uNumber)
	{
		return -1;
	}
	else
	{
		return 1;
	}
}

IMG_INTERNAL
IMG_VOID ModifyFixedRegPhysicalReg(PUSC_LIST		psFixedRegList,
								   PFIXED_REG_DATA	psFixedReg,
								   IMG_UINT32		uPhysicalRegType,
								   IMG_UINT32		uPhysicalRegNum)
/*****************************************************************************
 FUNCTION	: ModifyFixedRegPhysicalReg

 PURPOSE	: Change the physical register assigned to a group of intermediate
			  registers.

 PARAMETERS	: psFixedRegList		- List of fixed registers.
			  psFixedReg			- Fixed register to modify.
			  uPhysicalRegType		- New physical register type.
			  uPhysicalRegNum		- New physical register number.

 RETURNS	: None.
*****************************************************************************/
{
	/*
		Remove the fixed register to modify from the list of fixed registers.
	*/
	RemoveFromList(psFixedRegList, &psFixedReg->sListEntry);

	/*
		Update the assigned physical register.
	*/
	InitInstArg(&psFixedReg->sPReg);
	psFixedReg->sPReg.uType = uPhysicalRegType;
	psFixedReg->sPReg.uNumber = uPhysicalRegNum;

	/*
		Reinsert the fixed register keeping the list sorted.
	*/
	InsertInListSorted(psFixedRegList, CmpFixedReg, &psFixedReg->sListEntry);
}

IMG_INTERNAL
PCODEBLOCK GetShaderEndBlock(PINTERMEDIATE_STATE psState, PFIXED_REG_DATA psFixedVReg, IMG_UINT32 uRegIdx)
/*****************************************************************************
 FUNCTION	: GetShaderEndBlock

 PURPOSE	: Get the appropriate block to insert an instruction to copy data 
			  into a shader output in.

 PARAMETERS	: psState			- Compiler state.
			  psFixedVReg		- Shader output.
			  uRegIdx

 RETURNS	: A pointer to the block.
*****************************************************************************/
{
	if (psFixedVReg->bPrimary)
	{
		if (
				psFixedVReg->aeUsedForFeedback != NULL && 
				psFixedVReg->aeUsedForFeedback[uRegIdx] != FEEDBACK_USE_TYPE_NONE &&
				(psState->uFlags & USC_FLAGS_SPLITFEEDBACKCALC) != 0
		   )
		{
			return psState->psPreFeedbackBlock;
		}
		else
		{
			return psState->psMainProg->sCfg.psExit;
		}
	}
	else
	{
		ASSERT(psFixedVReg->aeUsedForFeedback == NULL);
		return psState->psSecAttrProg->sCfg.psExit;
	}
}

IMG_INTERNAL
PFIXED_REG_DATA AddFixedReg(PINTERMEDIATE_STATE	psState, 
							IMG_BOOL bPrimary,
							IMG_BOOL bLiveAtShaderEnd,
							IMG_UINT32 uPhysicalRegType,
							IMG_UINT32 uPhysicalRegNum,
							IMG_UINT32 uConsecutiveRegsCount)
/*********************************************************************************
 Function			: AddFixedReg

 Description		: Add a new entry to a list of fixed registers.
 
 Parameters			: psState			- Compiler state.
					  bPrimary			- TRUE if the fixed register is an input/output of the
										primary program.
										  FALSE if the fixed register is an input/output of the
										second program.
					  bLiveAtShaderEnd	- TRUE if the fixed register is a shader output.
										  FALSE if the fixed register is a shader input.
					  uPhysicalRegType	- Hardware register type for the fixed registers.
					  uPhysicalRegNum	- Starting hardware register number for the fixed
										registers.
					  uConsecutiveRegsCount
										- Number of virtual registers.

 Globals Effected	: None

 Return				: A pointer to the newly created fixed register structure.
*********************************************************************************/
{
	PFIXED_REG_DATA		psFixedReg;
	PUSC_LIST			psFixedRegList;
	USEDEF_TYPE			eUseDefType;
	IMG_UINT32			uRegIdx;

	psFixedReg = UscAlloc(psState, sizeof(*psFixedReg));

	psFixedReg->uVRegType = USC_UNDEF;
	psFixedReg->auVRegNum = UscAlloc(psState, sizeof(psFixedReg->auVRegNum[0]) * uConsecutiveRegsCount);
	psFixedReg->aeVRegFmt = UscAlloc(psState, sizeof(psFixedReg->aeVRegFmt[0]) * uConsecutiveRegsCount);
	psFixedReg->uConsecutiveRegsCount = uConsecutiveRegsCount;
	psFixedReg->puUsedChans = NULL;
	psFixedReg->aeUsedForFeedback = NULL;

	psFixedReg->asVRegUseDef = UscAlloc(psState, sizeof(psFixedReg->asVRegUseDef[0]) * uConsecutiveRegsCount);
	eUseDefType = bLiveAtShaderEnd ? USE_TYPE_FIXEDREG : DEF_TYPE_FIXEDREG;
	for (uRegIdx = 0; uRegIdx < uConsecutiveRegsCount; uRegIdx++)
	{
		UseDefReset(&psFixedReg->asVRegUseDef[uRegIdx], eUseDefType, uRegIdx, psFixedReg);
	}

	psFixedReg->uRegArrayIdx = USC_UNDEF;
	psFixedReg->uRegArrayOffset = USC_UNDEF;

	psFixedReg->sPReg.uType = USC_UNDEF;
	psFixedReg->sPReg.uNumber = USC_UNDEF;

	psFixedReg->auMask = NULL;
	psFixedReg->bLiveAtShaderEnd = bLiveAtShaderEnd;

	InitInstArg(&psFixedReg->sPReg);
	psFixedReg->sPReg.uType = uPhysicalRegType;
	psFixedReg->sPReg.uNumber = uPhysicalRegNum;

	#if defined(OUTPUT_USPBIN)
	psFixedReg->uNumAltPRegs = 0;
	#endif /* defined(OUTPUT_USPBIN) */

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	psFixedReg->bVector = IMG_FALSE;
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	psFixedReg->bPrimary = bPrimary;

	if (psFixedReg->bPrimary)
	{
		psFixedRegList = &psState->sFixedRegList;
	}
	else
	{
		psFixedRegList = &psState->sSAProg.sFixedRegList;
	}
	InsertInListSorted(psFixedRegList, CmpFixedReg, &psFixedReg->sListEntry);

	psFixedReg->uGlobalId = psState->uGlobalFixedRegCount++;

	return psFixedReg;
}

static
IMG_VOID GetColourResultPhysicalLocation(PINTERMEDIATE_STATE	psState, 
										 IMG_PUINT32			puPhysicalRegType, 
										 IMG_PUINT32			puPhysicalRegNum)
{
	IMG_BOOL	bUndefPRegNum;

	/*
		Check for the special case of the shader output going to the
		primary attributes but where the register number(s) won't be
		chosen until the register allocator.
	*/
	bUndefPRegNum = IMG_FALSE;
	if (psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_PRIMATTR)
	{
		#if defined(OUTPUT_USPBIN)
		if(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN)
		{
			bUndefPRegNum = IMG_TRUE;
		}
		#endif /* defined(OUTPUT_USPBIN) */
		#if defined(OUTPUT_USPBIN) && defined(OUTPUT_USCHW)
		else
		#endif /* defined(OUTPUT_USPBIN) && defined(OUTPUT_USCHW) */
		#if defined(OUTPUT_USCHW)
		{
			if (psState->psSAOffsets->ePackDestFormat == UF_REGFORMAT_U8)
			{
				bUndefPRegNum = IMG_TRUE;
			}
		}
		#endif /* defined(OUTPUT_USCHW) */
	}

	/*
		Store the start of the physical registers which will hold the
		shader output in the hardware code.
	*/
	*puPhysicalRegType = psState->psSAOffsets->uPackDestType;
	if (bUndefPRegNum)
	{
		*puPhysicalRegNum = USC_UNDEF;
	}
	else
	{
		*puPhysicalRegNum = 0;
	}
}

static
PFIXED_REG_DATA AddColourResultFixedReg(PINTERMEDIATE_STATE psState, IMG_UINT32 uShaderOutputCount, IMG_BOOL bNonFullMask)
/*********************************************************************************
 Function			: AddColourResultFixedReg

 Description		: Allocate and partially set up the structure representing the
					  colour result of a pixel shader.
 
 Parameters			: psState		- The intermediate state.
					  uColOutput	- Count of intermediate registers holding
									the colour result.
					  bNonFullMask	- If TRUE then not all channels from the
					  result are used in driver epilog.

 Globals Effected	: None

 Return				: A pointer to the allocated structure.
*********************************************************************************/
{
	IMG_UINT32			uPhysicalRegType;
	IMG_UINT32			uPhysicalRegNum;
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;
	PFIXED_REG_DATA		psOut;

	/*
		Get the start of the hardware registers where the driver epilog expects to
		find the shader colour result.
	*/
	GetColourResultPhysicalLocation(psState, &uPhysicalRegType, &uPhysicalRegNum);

	/*
		Save the index of the fixed register corresponding to the 
		shader colour output.
	*/
	psPS->psColFixedReg = psOut = 
		AddFixedReg(psState, 
					IMG_TRUE /* bPrimary */,
					IMG_TRUE /* bLiveAtShaderEnd */,
					uPhysicalRegType,
					uPhysicalRegNum,
					uShaderOutputCount);
	if (uPhysicalRegType == USEASM_REGTYPE_PRIMATTR && uPhysicalRegNum == USC_UNDEF)
	{
		psPS->psFixedHwPARegReg = psPS->psColFixedReg;
	}

	psOut->uVRegType = USEASM_REGTYPE_TEMP;

	if (bNonFullMask)
	{
		psOut->auMask = UscAlloc(psState, sizeof(psOut->auMask[0]) * psOut->uConsecutiveRegsCount);
	}
	else
	{
		psOut->auMask = NULL;
	}

	return psOut;
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_UINT32 ProcessShaderValidOutputs(PINTERMEDIATE_STATE	psState, 
									 IMG_PBOOL				pbNotFullColourOutputMask, 
									 PFIXED_REG_DATA		psFixedReg)
/*********************************************************************************
 Function			: ProcessShaderValidOutputs

 Description		: 
 
 Parameters			: psState		- The intermediate state.
					  pbNotFullColourOutputMask
									- Returns TRUE if some vector registers are
									only partially used.
					  psFixedReg	- If non-NULL then initialized with the
									details of intermediate registers holding the colour result.

 Globals Effected	: None

 Return				: The number of vector registers used by the driver epilog.
*********************************************************************************/
{
	IMG_UINT32			uColOutputCount;
	IMG_UINT32			uSurf;
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

	uColOutputCount = 0;
	if (pbNotFullColourOutputMask != NULL)
	{
		*pbNotFullColourOutputMask = IMG_FALSE;
	}
	for (uSurf = 0; uSurf < UNIFLEX_MAX_OUT_SURFACES; uSurf++)
	{
		IMG_UINT32	uSurfStart;
		IMG_UINT32	uValidChansFromSurf;

		if ((psPS->uEmitsPresent >> uSurf) == 0)
		{
			break;
		}

		uSurfStart = uSurf * CHANNELS_PER_INPUT_REGISTER;
		uValidChansFromSurf = GetRange(psState->psSAOffsets->puValidShaderOutputs, 
									   uSurfStart + CHANNELS_PER_INPUT_REGISTER - 1, 
									   uSurfStart);
		if (uValidChansFromSurf != 0)
		{
			if (psFixedReg != NULL)
			{
				psFixedReg->auVRegNum[uColOutputCount] = psPS->uColourResultBaseTempRegNum + uSurf * CHANNELS_PER_INPUT_REGISTER;
				psFixedReg->aeVRegFmt[uColOutputCount] = psState->psSAOffsets->ePackDestFormat;
				if (psFixedReg->auMask != NULL)
				{
					IMG_UINT32	uValidChanCount;

					uValidChanCount = g_auSetBitCount[uValidChansFromSurf];
					psFixedReg->auMask[uColOutputCount] = (1U << uValidChanCount) - 1;
				}
			}
			uColOutputCount++;

			if (uValidChansFromSurf != USC_ALL_CHAN_MASK)
			{
				if (pbNotFullColourOutputMask != NULL)
				{
					*pbNotFullColourOutputMask = IMG_TRUE;
				}
			}
		}
	}

	return uColOutputCount;
}

static
IMG_VOID SetupUnpackedColourOutput_Vec(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: SetupUnpackedColourOutput_Vec

 Description		: Set up the structure representing the colour result of a pixel 
					  shader when the result is in F32 format.
 
 Parameters			: psState		- The intermediate state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_BOOL			bNonFullColourOutputMask;
	PFIXED_REG_DATA		psOut;
	IMG_UINT32			uColOutputCount;

	/*
		Count the number of valid colour result channels.
	*/
	uColOutputCount = ProcessShaderValidOutputs(psState, &bNonFullColourOutputMask, NULL /* psFixedReg */);
	
	/*
		Create a structure to map the intermediate registers holding the pixel shader colour result to
		fixed hardware registers.
	*/
	psOut = AddColourResultFixedReg(psState, uColOutputCount, bNonFullColourOutputMask);
	psOut->bVector = IMG_TRUE;

	/*
		Initialize the created structure.
	*/
	ProcessShaderValidOutputs(psState, NULL /* pbNotFullColourOutputMask */, psOut);
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_UINT32 CountValidColourOutputChannels(PINTERMEDIATE_STATE psState, IMG_PUINT32 auTempReg)
{
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;
	IMG_UINT32			uColOutputCount;
	IMG_UINT32			uSurf;			

	uColOutputCount = 0;
	for (uSurf = 0; uSurf < UNIFLEX_MAX_OUT_SURFACES; uSurf++)
	{
		IMG_UINT32	uSurfStart;
		IMG_UINT32	uChan;
		IMG_UINT32	uValidChanCount;
		IMG_UINT32	uSurfRegCount;
	
		if ((psPS->uEmitsPresent >> uSurf) == 0)
		{
			break;
		}

		uSurfStart = uSurf * CHANNELS_PER_INPUT_REGISTER;
		uValidChanCount = 0;
		for (uChan = 0; uChan < CHANNELS_PER_INPUT_REGISTER; uChan++)
		{
			if (GetBit(psState->psSAOffsets->puValidShaderOutputs, uSurfStart + uChan))
			{
				uValidChanCount++;
			}
		}

		if (psState->psSAOffsets->ePackDestFormat == UF_REGFORMAT_F32)
		{
			uSurfRegCount = uValidChanCount;
		}
		else
		{
			ASSERT(psState->psSAOffsets->ePackDestFormat == UF_REGFORMAT_F16);
			uSurfRegCount = (uValidChanCount + F16_CHANNELS_PER_SCALAR_REGISTER - 1) / F16_CHANNELS_PER_SCALAR_REGISTER;
		}

		if (auTempReg != NULL)
		{
			IMG_UINT32	uReg;

			for (uReg = 0; uReg < uSurfRegCount; uReg++)
			{
				auTempReg[uColOutputCount + uReg] = psPS->uColourResultBaseTempRegNum + uSurfStart + uReg;
			}
		}
		uColOutputCount += uSurfRegCount;
	}
	return uColOutputCount;
}

static
IMG_VOID SetupUnpackedColourOutput(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: SetupUnpackedColourOutput

 Description		: Set up the structure representing the colour result of a pixel 
					  shader when the result is in F32 format.
 
 Parameters			: psState		- The intermediate state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;
	PFIXED_REG_DATA		psOut;
	IMG_UINT32			uRegIdx;

	/*
		Create a entry in the list of program results.
	*/
	psOut = AddColourResultFixedReg(psState, psPS->uColOutputCount, IMG_FALSE /* bNonFullMask */);

	/*
		Initialize the created entry with details of the intermediate registers.
	*/
	CountValidColourOutputChannels(psState, psOut->auVRegNum);
	for (uRegIdx = 0; uRegIdx < psPS->uColOutputCount; uRegIdx++)
	{
		psOut->aeVRegFmt[uRegIdx] = psState->psSAOffsets->ePackDestFormat;
	}

	ASSERT(psOut->auMask == NULL);
}

static
IMG_VOID SetupPixelShaderFixedRegs(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: SetupPixelShaderFixedRegs

 Description		: Set up the array representing pixel shader fixed inputs
					  and outputs.
 
 Parameters			: psState - The intermediate state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32			uIdx;
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

	/*
		Set up fixed registers representing the shader colour outputs.
	*/
	if (psState->psSAOffsets->ePackDestFormat == UF_REGFORMAT_U8)
	{
		IMG_BOOL		bNotFullColourOutputMask;
		PFIXED_REG_DATA	psOut;

		psPS->uColOutputCount = 1;

		bNotFullColourOutputMask = IMG_FALSE;
		if (psPS->uHwOrderPackedPSOutputMask != USC_ALL_CHAN_MASK)
		{
			bNotFullColourOutputMask = IMG_TRUE;
		}

		psOut = AddColourResultFixedReg(psState, 1 /* uConsecutiveRegsCount */, bNotFullColourOutputMask);

		psOut->auVRegNum[0] = USC_TEMPREG_MAXIMUM + psPS->uPackedPSOutputTempRegNum * CHANNELS_PER_INPUT_REGISTER;
		psOut->aeVRegFmt[0] = UF_REGFORMAT_U8;
		if (psOut->auMask != NULL)
		{
			psOut->auMask[0] = psPS->uHwOrderPackedPSOutputMask;
		}
	}
	else
	{
		/*
			Count the number of valid colour result channels.
		*/
		psPS->uColOutputCount = CountValidColourOutputChannels(psState, NULL /* auTempNum */);

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			SetupUnpackedColourOutput_Vec(psState);
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			SetupUnpackedColourOutput(psState);
		}
	}

	/*
		Set up a fixed register representing extra iterations inserted
		by the caller.
	*/
	if (psState->psSAOffsets->uExtraPARegisters > 0)
	{
		PFIXED_REG_DATA psOut;

		psPS->psExtraPAFixedReg = psOut = 
			AddFixedReg(psState, 
						IMG_TRUE /* bPrimary */,
						IMG_TRUE /* bLiveAtShaderEnd */,
						USEASM_REGTYPE_PRIMATTR,
						psPS->uIterationSize,
						psState->psSAOffsets->uExtraPARegisters);

		psOut->uVRegType = USEASM_REGTYPE_TEMP;
		for (uIdx = 0; uIdx < psState->psSAOffsets->uExtraPARegisters; uIdx++)
		{
			psOut->auVRegNum[uIdx] = GetNextRegister(psState);
			psOut->aeVRegFmt[uIdx] = UF_REGFORMAT_UNTYPED;
		}
	}
	else
	{
		psPS->psExtraPAFixedReg = IMG_NULL;
	}

	/*
		Set up a fixed register representing the shader depth output.
	*/
	if (psState->uFlags & USC_FLAGS_DEPTHFEEDBACKPRESENT)
	{
		PFIXED_REG_DATA psOut;
		IMG_UINT32		uPhysicalRegNum;

		/*
			The depth feedback uses the next temporary register after
			those which contain the colour results (if any).
		*/
		if (psState->psSAOffsets->uPackDestType == USEASM_REGTYPE_TEMP)
		{
			if (!(psState->uFlags & USC_FLAGS_OMASKFEEDBACKPRESENT))
			{
				uPhysicalRegNum = psPS->uColOutputCount;
			}
			else
			{
				uPhysicalRegNum = psPS->uColOutputCount + 1;
			}
		}
		else
		{
			uPhysicalRegNum = 0;
		}

		psPS->psDepthOutput = psOut = 
			AddFixedReg(psState, 
						IMG_TRUE /* bPrimary */,
						IMG_TRUE /* bLiveAtShaderEnd */,
						USEASM_REGTYPE_TEMP,
						uPhysicalRegNum,
						1 /* uConsecutiveRegsCount */);

		psOut->uVRegType = USEASM_REGTYPE_TEMP;
		psOut->auVRegNum[0] = psPS->uZTempRegNum;
		psOut->aeVRegFmt[0] = UF_REGFORMAT_F32;
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) != 0)
		{
			psOut->bVector = IMG_TRUE;
			psOut->auMask = UscAlloc(psState, sizeof(psOut->auMask[0]));
			psOut->auMask[0] = USC_X_CHAN_MASK;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	}

	/*
		If a texkill instruction is present in the input program then add a predicate containing
		the kill result as an output.
	*/
	if (psState->uFlags & USC_FLAGS_TEXKILL_PRESENT)
	{
		PFIXED_REG_DATA psOut;

		psPS->psTexkillOutput = psOut = 
			AddFixedReg(psState, 
						IMG_TRUE /* bPrimary */,
						IMG_TRUE /* bLiveAtShaderEnd */,
						USEASM_REGTYPE_PREDICATE,
						USC_OUTPUT_TEXKILL_PRED,
						1 /* uConsecutiveRegsCount */);

		psOut->uVRegType = USEASM_REGTYPE_PREDICATE;
		psOut->auVRegNum[0] = USC_PREDREG_TEXKILL;
		psOut->aeVRegFmt[0] = UF_REGFORMAT_UNTYPED;
	}

	if ((psState->uFlags & USC_FLAGS_OMASKFEEDBACKPRESENT) != 0)
	{
		IMG_UINT32		uPhysicalRegType;
		IMG_UINT32		uPhysicalRegNum;
		PFIXED_REG_DATA	psOut;

		/*
			Place the oMask result immediately after the registers containing
			the colour result.
		*/
		GetColourResultPhysicalLocation(psState, &uPhysicalRegType, &uPhysicalRegNum);
		if (uPhysicalRegNum != USC_UNDEF)
		{
			uPhysicalRegNum += psPS->uColOutputCount;
		}

		psPS->psOMaskOutput = psOut = 
			AddFixedReg(psState, 
						IMG_TRUE /* bPrimary */,
						IMG_TRUE /* bLiveAtShaderEnd */,
						uPhysicalRegType,
						uPhysicalRegNum,
						1 /* uConsecutiveRegsCount */);

		psOut->uVRegType = USEASM_REGTYPE_TEMP;
		psOut->auVRegNum[0] = psPS->uOMaskTempRegNum;
		psOut->aeVRegFmt[0] = UF_REGFORMAT_F32;
	}
}

static
IMG_VOID GenerateShaderEndClamp(PINTERMEDIATE_STATE	psState,
								UF_REGTYPE			eDestRegType,
								IMG_UINT32			uDestRegNum,
								UF_REGFORMAT		eDestFormat,
								IMG_UINT32			uClampValue)
/*********************************************************************************
 Function			: GenerateShaderEndClamp

 Description		: Create an input format MAX and add the equivalent
				      intermediate instructions to the end of the program.
 
 Parameters			: psState		- The intermediate state.
					  eDestRegType	- Register to clamp.
					  uDestRegNum
					  eDestFormat
					  uClampValue	- Value to clamp to.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUNIFLEX_INST	psMAXInst;

	/*
		Create a MOV instruction.
	*/
	psMAXInst = UscAlloc(psState, sizeof(UNIFLEX_INST));
	psMAXInst->eOpCode = UFOP_MAX;
	BuildInputDest(&psMAXInst->sDest, 
				   eDestRegType, 
				   uDestRegNum, 
				   eDestFormat, 
				   (1U << UFREG_DMASK_B_SHIFT) | (1U << UFREG_DMASK_G_SHIFT) | (1U << UFREG_DMASK_R_SHIFT) | (1U << UFREG_DMASK_A_SHIFT));
	BuildInputSrc(&psMAXInst->asSrc[0], 
				  eDestRegType, 
				  uDestRegNum, 
				  eDestFormat, 
				  UFREG_SWIZ_NONE);
	BuildInputImmediateSrc(&psMAXInst->asSrc[1], uClampValue, eDestFormat);
	psMAXInst->uPredicate = UF_PRED_NONE;

	/*
		Generate the equivalent intermediate code in the final block of the
		main function.
	*/
	ConvertInstToIntermediate(psState, 
							  psState->psMainProg->sCfg.psExit, 
							  psMAXInst, 
							  IMG_FALSE /* bConditional */, 
							  IMG_FALSE /* bStaticCond */);

	/*
		Free the created instruction.
	*/
	UscFree(psState, psMAXInst);
}

static
IMG_VOID GenerateShaderMOV(PINTERMEDIATE_STATE	psState,
						   PCODEBLOCK			psBlock,
						   UF_REGTYPE			eDestRegType,
						   IMG_UINT32			uDestRegNum,
						   UF_REGFORMAT			eDestFormat,
						   IMG_UINT32			uDestSat,
						   UF_REGTYPE			eSrcRegType,
						   IMG_UINT32			uSrcRegNum,
						   IMG_UINT32			uSrcRegArrayTag,
						   IMG_UINT32			uSrcSwizzle,
						   UF_REGFORMAT			eSrcFormat)
/*********************************************************************************
 Function			: GenerateShaderMOV

 Description		: Create an input format MOV and add the equivalent
				      intermediate instructions to the program.
 
 Parameters			: psState		- The intermediate state.
					  psBlock		- Block to insert the new instructions
									into.
					  eDestRegType	- Destination register for the MOV.
					  uDestRegNum
					  eDestFormat
					  uDestSat		- Destination saturation modifier.
					  eSrcRegType	- Source register for the MOV.
					  uSrcRegNum
					  uSrcRegArrayTag
					  uSrcSwizzle
					  eSrcFormat

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PUNIFLEX_INST	psMOVInst;

	/*
		Create a MOV instruction.
	*/
	psMOVInst = UscAlloc(psState, sizeof(UNIFLEX_INST));
	psMOVInst->eOpCode = UFOP_MOV;
	BuildInputDest(&psMOVInst->sDest, 
				   eDestRegType, 
				   uDestRegNum, 
				   eDestFormat, 
				   (1U << UFREG_DMASK_B_SHIFT) | (1U << UFREG_DMASK_G_SHIFT) | (1U << UFREG_DMASK_R_SHIFT) | (1U << UFREG_DMASK_A_SHIFT));
	psMOVInst->sDest.byMod = (IMG_BYTE)(uDestSat << UFREG_DMOD_SAT_SHIFT);
	BuildInputSrc(&psMOVInst->asSrc[0], 
				  eSrcRegType, 
				  uSrcRegNum, 
				  eSrcFormat, 
				  uSrcSwizzle);
	psMOVInst->asSrc[0].uArrayTag = uSrcRegArrayTag;
	psMOVInst->uPredicate = UF_PRED_NONE;

	/*
		Generate the equivalent intermediate code.
	*/
	ConvertInstToIntermediate(psState, 
							  psBlock, 
							  psMOVInst, 
							  IMG_FALSE /* bConditional */, 
							  IMG_FALSE /* bStaticCond */);

	/*
		Free the created instruction.
	*/
	UscFree(psState, psMOVInst);
}

static
IMG_VOID FinalisePixelShaderIntermediateCode(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: FinalisePixelShaderIntermediateCode

 Description		: Finalise the set up of the intermediate code for a pixel
					  shader after converting the input program.
 
 Parameters			: psState - The intermediate state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PPIXELSHADER_STATE	psPS = psState->sShader.psPS;

	ASSERT(psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL);

	if ((psState->uFlags2 & USC_FLAGS2_PSOUTPUTRELATIVEADDRESSING) != 0)
	{
		IMG_UINT32	uOutput;

		for (uOutput = 0; uOutput < UNIFLEX_MAX_OUT_SURFACES; uOutput++)
		{
			if (psPS->uEmitsPresent & (1U << uOutput))
			{
				UF_REGTYPE		eOutputRegType;
				IMG_UINT32		uOutputRegNum;
				UF_REGFORMAT	eOutputRegFormat;

				if (psState->psSAOffsets->ePackDestFormat != UF_REGFORMAT_U8)
				{
					eOutputRegType = UFREG_TYPE_PSOUTPUT;
					uOutputRegNum = uOutput;
					eOutputRegFormat = psPS->aeColourResultFormat[uOutput];
				}
				else
				{
					eOutputRegType = UFREG_TYPE_TEMP;
					uOutputRegNum = psPS->uPackedPSOutputTempRegNum;
					eOutputRegFormat = UF_REGFORMAT_U8;
				}

				GenerateShaderMOV(psState,
								  psState->psMainProg->sCfg.psExit,
							  	  eOutputRegType,
								  uOutputRegNum,
								  eOutputRegFormat,
								  UFREG_DMOD_SATNONE,
								  psPS->ePSOutputRegType,
								  uOutput,
								  psPS->uPSOutputRegArrayTag,
								  UFREG_SWIZ_NONE,
								  psPS->aeColourResultFormat[uOutput]);
			}
		}
	}

	/*
		Saturate the pixel shader output if required.
	*/
	if (psState->psSAOffsets->ePackDestFormat != UF_REGFORMAT_U8)
	{
		IMG_UINT32	uOutput;

		for (uOutput = 0; uOutput < UNIFLEX_MAX_OUT_SURFACES; uOutput++)
		{
			IMG_UINT32	uSat = psState->psSAOffsets->puOutputSaturate[uOutput];

			if (psPS->uEmitsPresent & (1U << uOutput))
			{
				/*
					Generate an instruction of the form:
						MOV	MCn_sat01, MCn
				*/
				GenerateShaderMOV(psState,
								  psState->psMainProg->sCfg.psExit,
							  	  UFREG_TYPE_PSOUTPUT,
								  uOutput,
								  psPS->aeColourResultFormat[uOutput],
								  uSat,
								  UFREG_TYPE_PSOUTPUT,
								  uOutput,
								  USC_UNDEF /* uSrcArrayTag */,
								  UFREG_SWIZ_NONE,
								  psPS->aeColourResultFormat[uOutput]);
			}
		}
	}

	/*	
		If the program uses texkills then initialize the predicate register
		with the texkill flag at the start of the program.
	*/
	if ((psState->uFlags & USC_FLAGS_TEXKILL_PRESENT) != 0 &&
		(psState->uFlags & USC_FLAGS_INITIALISE_TEXKILL) != 0)
	{
		PINST psInst;

		psInst = AllocateInst(psState, IMG_NULL);
		SetOpcodeAndDestCount(psState, psInst, ITESTPRED, TEST_PREDICATE_ONLY_DEST_COUNT);
		psInst->u.psTest->eAluOpcode = IOR;
		MakePredicateArgument(&psInst->asDest[TEST_PREDICATE_DESTIDX], USC_PREDREG_TEXKILL);
		psInst->u.psTest->sTest.eType = TEST_TYPE_ALWAYS_TRUE;
		psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[0].uNumber = 0;
		psInst->asArg[1].uType = USEASM_REGTYPE_IMMEDIATE;
		psInst->asArg[1].uNumber = 0;
		InsertInstBefore(psState, psState->psMainProg->sCfg.psEntry,
						 psInst,
						 psState->psMainProg->sCfg.psEntry->psBody);
	}

	/*	
		If the program writes the depth then initialize the depth output register.
	*/
	if ((psState->uFlags & USC_FLAGS_DEPTHFEEDBACKPRESENT))
	{
		PCODEBLOCK	psNewEntry;

		psNewEntry = AllocateBlock(psState, &(psState->psMainProg->sCfg));
		SetBlockUnconditional(psState, psNewEntry, psState->psMainProg->sCfg.psEntry);
		psState->psMainProg->sCfg.psEntry = psNewEntry;

		GenerateShaderMOV(psState,
						  psNewEntry,
						  UFREG_TYPE_PSOUTPUT, 
						  UFREG_OUTPUT_Z, 
						  psState->sShader.psPS->eZFormat,
						  UFREG_DMOD_SATNONE,
						  UFREG_TYPE_MISC,
						  UF_MISC_POSITION,
						  USC_UNDEF /* uSrcArrayTag */,
						  UFREG_SWIZ_ZZZZ,
						  psState->sShader.psPS->eZFormat);
	}

	/*
		If the program has multiple return points from main then insert code to pack
		the shader result to U8 in the last block of the main function.
	*/
	if (psState->uFlags & USC_FLAGS_MULTIPLERETSFROMMAIN)
	{
		/*
			Create an instruction:
				MOV	MP0, MC0
		*/
		GenerateShaderMOV(psState,
						  psState->psMainProg->sCfg.psExit,
						  UFREG_TYPE_TEMP, 
						  psPS->uPackedPSOutputTempRegNum /* uDestRegNum */, 
						  UF_REGFORMAT_U8,
						  UFREG_DMOD_SATNONE,
						  psPS->ePSOutputRegType,
						  psPS->uPSOutputRegNum,
						  psPS->uPSOutputRegArrayTag,
						  UFREG_SWIZ_NONE,
						  psPS->aeColourResultFormat[0]);
	}

	/*
		Insert instructions to convert shader results from the format/precision used by the
		shader code to the format expected by the hardware.
	*/
	ConvertPixelShaderResults(psState);

	/*
		Set up so the register allocator assigns intermediate registers
		containing shader results to fixed hardware registers.
	*/
	SetupPixelShaderFixedRegs(psState);
}

#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
static
IMG_VOID SetupVertexShaderOutputs_Vec(PINTERMEDIATE_STATE	psState)
/*********************************************************************************
 Function			: SetupVertexShaderOutputs_Vec

 Description		: Set up the structures representing vertex shader outputs
					  on cores which support the vector instruction.
 
 Parameters			: psState - The intermediate state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;
	IMG_UINT32			uNumOutputs;
	IMG_UINT32			uInputRegIdx;
	IMG_UINT32			uVecRegIdx;
	PFIXED_REG_DATA		psVSOut;
	IMG_UINT32			uMaxOutputWritten;
	IMG_UINT32			uPhysicalRegType;
	IMG_PUINT32			auVRegNum;
	IMG_PUINT32			auMask;

	/*
		Get the maximum vertex shader output written.
	*/
	uMaxOutputWritten = USC_MAX_SHADER_OUTPUTS;
	while (uMaxOutputWritten > 0)
	{
		if (psVS->auVSOutputToVecReg[uMaxOutputWritten - 1] != USC_UNDEF)
		{
			break;
		}
		uMaxOutputWritten--;
	}

	/*
		Get the physical register type which vertex shader outputs map to.
	*/
	if ((psState->uCompilerFlags & UF_REDIRECTVSOUTPUTS) == 0 || (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY))
	{
		uPhysicalRegType = USEASM_REGTYPE_OUTPUT;
	}
	else
	{
		uPhysicalRegType = USEASM_REGTYPE_PRIMATTR;
	}

	/*
		Allocate space for the maximum possible number of vertex shader outputs.
	*/
	auVRegNum = UscAlloc(psState, sizeof(auVRegNum[0]) * USC_MAX_SHADER_OUTPUTS);
	auMask = UscAlloc(psState, sizeof(auMask[0]) * USC_MAX_SHADER_OUTPUTS);
	
	uInputRegIdx = 0;
	uVecRegIdx = 0;
	uNumOutputs = 0;
	while (uInputRegIdx < uMaxOutputWritten)
	{
		IMG_UINT32					uTempReg;
		IMG_UINT32					uBaseInputRegIdx;
		IMG_UINT32					uChanCount;
		IMG_UINT32					uValidChanMask;
		IMG_UINT32					uChanIdx;

		uTempReg = psVS->auVSOutputToVecReg[uInputRegIdx];

		/*
			Skip vertex shader outputs which we've never written.
		*/
		if (uTempReg == USC_UNDEF)
		{
			/*
				If this vertex shader output is unreferenced but still read after
				the shader has executed then include it in the fixed register so we
				don't try to use it for temporary data.
			*/
			if (GetBit(psState->psSAOffsets->puValidShaderOutputs, uInputRegIdx))
			{
				auMask[uVecRegIdx] = USC_X_CHAN_MASK;
				auVRegNum[uVecRegIdx] = psVS->uVertexShaderOutputsFirstRegNum + uInputRegIdx;
				uVecRegIdx++;
				uNumOutputs++;
			}

			uInputRegIdx++;
			continue;
		}

		/*
			Count the number of vertex shader outputs which were read/written together with
			the base output. We can treat them as a single vector quantity in the intermediate
			code.
		*/
		uBaseInputRegIdx = uInputRegIdx;
		uChanCount = 1;
		uInputRegIdx++;
		while (uInputRegIdx < uMaxOutputWritten && psVS->auVSOutputToVecReg[uInputRegIdx] == uTempReg)
		{
			ASSERT(uChanCount < CHANNELS_PER_INPUT_REGISTER);
			uChanCount++;
			uInputRegIdx++;
		}

		/*
			Get a mask of the channels which are read in the vertex shader output after the shader
			has finished executing.
		*/
		uValidChanMask = 0;
		for (uChanIdx = 0; uChanIdx < uChanCount; uChanIdx++)
		{
			if (GetBit(psState->psSAOffsets->puValidShaderOutputs, uBaseInputRegIdx + uChanIdx))
			{
				uNumOutputs++;
				uValidChanMask |= (1U << uChanIdx);
			}
		}
		if (uValidChanMask == 0)
		{
			continue;
		}

		/*
			Add a single variable representing a vector of vertex shader outputs which are always read/written
			together.
		*/
		auMask[uVecRegIdx] = uValidChanMask;
		auVRegNum[uVecRegIdx] = uTempReg;
		uVecRegIdx++;
	}
	
	/*
		Allocate a fixed register for the vertex shader outputs.
	*/
	ASSERT(uVecRegIdx < USC_MAX_SHADER_OUTPUTS);
	psVS->psVertexShaderOutputsFixedReg = psVSOut = 
		AddFixedReg(psState, 
					IMG_TRUE /* bPrimary */,
					IMG_TRUE /* bLiveAtShaderEnd */,
					uPhysicalRegType,
					0 /* uPhysicalRegNum */,
					uVecRegIdx);

	/*
		Represent vertex shader outputs by temporary variables in the intermediate
		code.
	*/
	psVSOut->uVRegType = USEASM_REGTYPE_TEMP;

	/*
		Treat the vertex shader outputs as vector sized registers.
	*/
	psVSOut->bVector = IMG_TRUE;

	/*
		The vertex shader outputs are always in F32 format.
	*/
	for (uVecRegIdx = 0; uVecRegIdx < psVSOut->uConsecutiveRegsCount; uVecRegIdx++)
	{
		psVSOut->aeVRegFmt[uVecRegIdx] = UF_REGFORMAT_F32;
	}

	/*
		Store the virtual registers of the vertex shader outputs.
	*/
	memcpy(psVSOut->auVRegNum, auVRegNum, sizeof(psVSOut->auVRegNum[0]) * psVSOut->uConsecutiveRegsCount);
	UscFree(psState, auVRegNum);

	/*
		Store the masks of channels used from each vertex shader output.
	*/
	psVSOut->auMask = UscAlloc(psState, sizeof(psVSOut->auMask[0]) * psVSOut->uConsecutiveRegsCount);
	memcpy(psVSOut->auMask, auMask, sizeof(psVSOut->auMask[0]) * psVSOut->uConsecutiveRegsCount);
	UscFree(psState, auMask);

	/*
		Store the maximum (packed) vertex shader output written.
	*/
	psState->sShader.psVS->uVertexShaderNumOutputs = uNumOutputs;

	/*
		Release the information about vector registers.
	*/
	UscFree(psState, psVS->auVSOutputToVecReg);
	psVS->auVSOutputToVecReg = NULL;
}
#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

static
IMG_VOID SetupVertexShaderFixedRegs(PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: SetupVertexShaderFixedRegs

 Description		: Set up the array representing vertex or geometry shader fixed inputs
					  and outputs.
 
 Parameters			: psState - The intermediate state.

 Globals Effected	: None

 Return				: Nothing.
*********************************************************************************/
{
	IMG_UINT32			uIdx;
	PVERTEXSHADER_STATE	psVS = psState->sShader.psVS;

	/*
		Create an entry representing the vertex shader outputs.
	*/
	psVS->psVertexShaderOutputsFixedReg = NULL;
	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	if ((psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) && !(psState->uFlags & USC_FLAGS_OUTPUTRELATIVEADDRESSING))
	{
		SetupVertexShaderOutputs_Vec(psState);
	}
	else
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
	if (psVS->uVertexShaderNumOutputs > 0)
	{
		PFIXED_REG_DATA psOut;
		IMG_UINT32		uRegIdx;
		IMG_UINT32		uPhysicalRegType;

		/*
			Set the hardware registers used for the vertex shader outputs.
		*/
		if ((psState->uCompilerFlags & UF_REDIRECTVSOUTPUTS) == 0 || (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY))
		{
			uPhysicalRegType = USEASM_REGTYPE_OUTPUT;
		}
		else
		{
			uPhysicalRegType = USEASM_REGTYPE_PRIMATTR;
		}

		psVS->psVertexShaderOutputsFixedReg = psOut = 
			AddFixedReg(psState, 
						IMG_TRUE /* bPrimary */,
						IMG_TRUE /* bLiveAtShaderEnd */,
						uPhysicalRegType,
						0 /* uPhysicalRegNum */,
						psVS->uVertexShaderNumOutputs);

		/*
			Link the fixed register to the data structure representing the
			vertex shader outputs as an indexable object.
		*/
		psOut->uRegArrayIdx = psVS->uVertexShaderOutputsArrayIdx;
		psOut->uRegArrayOffset = 0;

		/*
			Store the numbers of the variables representing the vertex shader outputs.
		*/
		psOut->uVRegType = USEASM_REGTYPE_TEMP;
		for (uRegIdx = 0; uRegIdx < psVS->uVertexShaderNumOutputs; uRegIdx++)
		{
			psOut->auVRegNum[uRegIdx] = psVS->uVertexShaderOutputsFirstRegNum + uRegIdx;
			psOut->aeVRegFmt[uRegIdx] = UF_REGFORMAT_F32;
		}
	}

	/*
		Reduce the size of the array representing the vertex shader
		outputs.
	*/
	if (psVS->uVertexShaderOutputsArrayIdx != USC_UNDEF)
	{
		PUSC_VEC_ARRAY_REG	psVSOutputArray;

		ASSERT(psVS->uVertexShaderOutputsArrayIdx < psState->uNumVecArrayRegs);
		psVSOutputArray = psState->apsVecArrayReg[psVS->uVertexShaderOutputsArrayIdx];

		if (psVS->uVertexShaderNumOutputs == 0)
		{
			UscFree(psState, psVSOutputArray);
			psState->apsVecArrayReg[psVS->uVertexShaderOutputsArrayIdx] = NULL;
			psVS->uVertexShaderOutputsArrayIdx = USC_UNDEF;
		}
		else
		{
			ASSERT(psVSOutputArray->uRegs >= psVS->uVertexShaderNumOutputs);
			psVSOutputArray->uRegs = psVS->uVertexShaderNumOutputs;
		}
	}

	/*
		Set up a fixed register for the vertex shader clip position output.
	*/
	if ((psState->uFlags & USC_FLAGS_VSCLIPPOS_USED) != 0 && (psState->psSAOffsets->eShaderType != USC_SHADERTYPE_GEOMETRY))
	{
		for (uIdx = 0; uIdx < 4; uIdx++)
		{
			PFIXED_REG_DATA psOut;
			
			psOut = 
				AddFixedReg(psState, 
							IMG_TRUE /* bPrimary */,
							IMG_TRUE /* bLiveAtShaderEnd */,
							USEASM_REGTYPE_TEMP,
							uIdx,
							1 /* uConsecutiveRegsCount */);

			psOut->uVRegType = USEASM_REGTYPE_TEMP;
			psOut->auVRegNum[0] = USC_TEMPREG_VSCLIPPOS + uIdx;
			psOut->aeVRegFmt[0] = UF_REGFORMAT_F32;
		}
	}

	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY)
	{
		/*	
			Set up a fixed register representing the primitive ID input.
		*/
		{
			PFIXED_REG_DATA psOut;
			
			psOut = 
				AddFixedReg(psState, 
							IMG_TRUE /* bPrimary */,
							IMG_FALSE /* bLiveAtShaderEnd */,
							USEASM_REGTYPE_TEMP,
							psState->psSAOffsets->uInputVerticesCount,
							1 /* uConsecutiveRegsCount */);

			psOut->uVRegType = USEASM_REGTYPE_TEMP;
			psOut->auVRegNum[0] = USC_TEMPREG_PRIMITIVE_ID;
			psOut->aeVRegFmt[0] = UF_REGFORMAT_F32;
		}

		/*
			Set up a fixed register representing the input registers containing the
			offset of each input vertex in the primary attributes.
		*/
		if (psVS->uVerticesBaseInternArrayIdx != USC_UNDEF)
		{
			PFIXED_REG_DATA		psOut;
			IMG_UINT32			uRegIdx;
			PUSC_VEC_ARRAY_REG	psVBaseArray;

			psVBaseArray = psState->apsVecArrayReg[psVS->uVerticesBaseInternArrayIdx];

			/*
				Map these variables to r0-rN in the register allocator.
			*/
			psOut = AddFixedReg(psState, 
								IMG_TRUE /* bPrimary */,
								IMG_FALSE /* bLiveAtShaderEnd */, 
								USEASM_REGTYPE_TEMP,
								0 /* uPhysicalRegNum */,
								psState->psSAOffsets->uInputVerticesCount);

			/*
				Link the fixed register to the register array for the vertex bases.
			*/
			psOut->uRegArrayIdx = psVS->uVerticesBaseInternArrayIdx;
			psOut->uRegArrayOffset = 0;

			/*
				Copy the virtual register numbers from the register array.
			*/
			psOut->uVRegType = USEASM_REGTYPE_TEMP;
			for (uRegIdx = 0; uRegIdx < psState->psSAOffsets->uInputVerticesCount; uRegIdx++)
			{
				psOut->auVRegNum[uRegIdx] = psVBaseArray->uBaseReg + uRegIdx;
				psOut->aeVRegFmt[uRegIdx] = UF_REGFORMAT_F32;
			}
				
			psOut->uConsecutiveRegsCount = psState->psSAOffsets->uInputVerticesCount;
		}
	}
}

static
IMG_VOID FinaliseVertexShaderIntermediateCode(PINTERMEDIATE_STATE psState)
{
	/*
		Apply the fix for BRN27325 by clamping the vertex shader diffuse
		and specular outputs.
	*/
	if (
			(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_27235) != 0 && 
			(psState->uCompilerFlags & UF_CLAMPVSCOLOUROUTPUTS) != 0
	   )
	{
		IMG_UINT32	uOutputType;

		for (uOutputType = 0; uOutputType < 2; uOutputType++)
		{
			IMG_UINT32	uOutputOffset;

			if (uOutputType == 0)
			{
				uOutputOffset = psState->psSAOffsets->uVSDiffuseOutputNum;
			}
			else
			{
				uOutputOffset = psState->psSAOffsets->uVSSpecularOutputNum;
			}

			/*
				Skip if this colour output isn't used by the shader.
			*/
			if (uOutputOffset == USC_UNDEF)
			{
				continue;
			}

			/*
				oN = MAX(oN, 0x30000000)
			*/
			GenerateShaderEndClamp(psState,
								   UFREG_TYPE_VSOUTPUT,
								   uOutputOffset,
								   UF_REGFORMAT_F32,
								   0x30000000);
		}
	}

	SetupVertexShaderFixedRegs(psState);
}

IMG_INTERNAL
IMG_VOID ConvertToIntermediate(const PUNIFLEX_INST psOrigProg, 
							   PINTERMEDIATE_STATE psState)
/*********************************************************************************
 Function			: ConvertToIntermediate

 Description		: Convert a list of instructions in the input function to the
					  intermediate format.
 
 Parameters			: psOrigProg - The program to be converted.
					  psState - The intermediate state.

 Globals Effected	: None

 Return				: Compilation status.
*********************************************************************************/
{
	PUNIFLEX_INST	psOrigInst;
	PCALLDATA		psCallData;
	/* 
		Shader Output Ranges which are accessed. 
	*/
	IMG_BOOL*		abAcesedShdrOutputRanges;

#ifdef SRC_DEBUG
	IMG_UINT32 uMaxLine = 0;
#endif /* SRC_DEBUG */

	/* Initialize call data */
	psCallData = UscAlloc(psState, sizeof(CALLDATA));
	memset(psCallData, 0, sizeof(*psCallData));

	/* Get the maximum temporary register used in the input program. */
	GetMaximumInputRegisterNumber(psState, psOrigProg);

	/* Copy the input program expanding macros as we go. */
	psOrigInst = psOrigProg;

	{
		abAcesedShdrOutputRanges = IMG_NULL;
		if((psState->psSAOffsets->sShaderOutPutRanges.uRangesCount) > 0)
		{
			IMG_UINT32 uRangeIdx;
			abAcesedShdrOutputRanges = UscAlloc(psState, sizeof(abAcesedShdrOutputRanges[0]) * (psState->psSAOffsets->sShaderOutPutRanges.uRangesCount));
			for(uRangeIdx = 0; uRangeIdx < (psState->psSAOffsets->sShaderOutPutRanges.uRangesCount); uRangeIdx++)
			{
				abAcesedShdrOutputRanges[uRangeIdx] = IMG_FALSE;
			}
		}
	}
	while (psOrigInst != NULL)
	{
		/* Get the maximum number of lines to allocate buffer */
#ifdef SRC_DEBUG
		if((psOrigInst->uSrcLine) > uMaxLine)
		{
			uMaxLine = psOrigInst->uSrcLine;
		}
#endif /* SRC_DEBUG */

		ExpandRelativeIndexAndMacro(psState, psOrigInst, &(psCallData->sProg), abAcesedShdrOutputRanges);
		psOrigInst = psOrigInst->psILink;
	}
	SetValidSahderOutputsData(psState, abAcesedShdrOutputRanges);
	{
		if(abAcesedShdrOutputRanges != IMG_NULL)
		{
			UscFree(psState, abAcesedShdrOutputRanges);
			abAcesedShdrOutputRanges = IMG_NULL;
		}
	}

	/* Allocate and initalize the cost counter table */
#ifdef SRC_DEBUG
	{
		IMG_UINT32 i;
		psState->uTotalLines = uMaxLine + 1;
		psState->puSrcLineCost = UscAlloc(psState, 
										  (sizeof(IMG_UINT32)*((psState->uTotalLines)+1)));
		psState->uCurSrcLine = (IMG_UINT32)UNDEFINED_SOURCE_LINE;
		for(i=0;i<=(psState->uTotalLines);i++)
		{
			psState->puSrcLineCost[i] = 0; 
		}
	}
#endif /* SRC_DEBUG */

	/*
		If indexable addressing is used with the pixel shader outputs then replace the
		pixel shader output everywhere by a new indexable temporary array.
	*/
	if ((psState->uFlags2 & USC_FLAGS2_PSOUTPUTRELATIVEADDRESSING) != 0)
	{
		ReplacePSOutputByIndexableTemporary(psState, &psCallData->sProg);
	}

	/* Initialise compiler data */
	InitialiseIndexableTemps(psState);

	/*
	  Check if we want to override F16 precision calculations to F32.
	*/
	if (
			!psState->bInvariantShader &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_FMAD16_SWIZZLES) == 0 &&
			(psState->psTargetFeatures->ui32Flags & SGX_FEATURE_FLAGS_USE_VEC34) == 0
	   )
	{
		/* Don't do a precision override for OpenCL */
		if (!(psState->uCompilerFlags & UF_OPENCL))
		{
			if (!(psState->uCompilerFlags & UF_SGX540_VECTORISE_F16))
			{
				OverrideFloat16Precisions(psState, &(psCallData->sProg));
				TESTONLY(PrintInput(psState, 
									"------ Input After F16 Conversion -------\r\n", 
									"------ End Input After F16 Conversion ----\r\n", 
									(psCallData->sProg).psHead, 
									psState->psConstants, 
									psState->uCompilerFlags, 
									psState->uCompilerFlags2, 
									psState->psSAOffsets,
									(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) ? IMG_TRUE : IMG_FALSE));
			}
		}
	}

	/*
	  Try converting instructions to integer.
	*/
	if (psState->psSAOffsets->ePackDestFormat == UF_REGFORMAT_U8)
	{
		ConvertOperationsToInteger(psState, &psCallData->sProg);
	}
	
	/* Check the formats of registers are consistent throughout the program. */
	CheckConsistentRegFormats(psState, psCallData->sProg.psHead);

	/*
	  Dump the result.
	*/
	TESTONLY(PrintInput(psState, 
						"------ Expanded Input -------\r\n", 
						"------ End Expanded Input ----\r\n", 
						psCallData->sProg.psHead, 
						psState->psConstants, 
						psState->uCompilerFlags, 
						psState->uCompilerFlags2, 
						psState->psSAOffsets,
						(psState->uFlags & USC_FLAGS_COMPILE_FOR_USPBIN) ? IMG_TRUE : IMG_FALSE));

	/*
	  Convert the expanded program into the intermediate format.
	*/
	psState->uD3DLoopIndex = USC_UNDEF;
	psState->uFlags &= ~USC_FLAGS_HAS_EARLY_EXIT;

	/*
	  Reserve space for all registers in the input program.
	*/
	psState->uNumRegisters = USC_TEMPREG_MAXIMUM;
	psState->uNumRegisters += (psState->uInputTempRegisterCount * CHANNELS_PER_INPUT_REGISTER);

	/*
		Similarly for predicate registers.
	*/
	psState->uNumPredicates = USC_NUM_PREDREGS + psState->uInputPredicateRegisterCount * CHANNELS_PER_INPUT_REGISTER;

	switch (psState->psSAOffsets->eShaderType)
	{
		case USC_SHADERTYPE_PIXEL:
		{
			SetupPixelShader(psState);
			break;
		}

		case USC_SHADERTYPE_VERTEX:
		case USC_SHADERTYPE_GEOMETRY:
		{
			/* Set up vertex shader inputs. */
			SetupVertexShaderInputs(psState);

			/* Set up vertex shader outputs. */
			SetupVertexShaderOutputs(psState);
			break;
		}

		case USC_SHADERTYPE_COMPUTE:
		{
			/* No inputs/outputs defined yet. */
			break;
		}
	}

	/* Build main program (also functions, according to labels + calls) */
	BuildCFG(psState, "MAIN FUNCTION", psCallData, USC_MAIN_LABEL_NUM, IMG_TRUE);

	/* 
	 *	Release program memory
	 */ 
	{
		PUNIFLEX_INST psCurr, psNext;
		/* Release instruction memory */
		for (psCurr = psCallData->sProg.psHead; psCurr; psCurr = psNext)
		{
			psNext = psCurr->psILink;
			UscFree(psState, psCurr);
		}
	}

	/* Release memory */
	UscFree(psState, psCallData->puInputLabels);
	UscFree(psState, psCallData->ppsFuncs);
	UscFree(psState, psCallData);

	/*	
		If the program is a geometry shader than initialize the output buffer 
		number register and output buffer index register.
	*/
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY)
	{
		{		
			PINST psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IMOV);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_IDX;
			psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[0].uNumber = 0;
			InsertInstBefore(psState, psState->psMainProg->sCfg.psEntry,
							 psInst,
							 psState->psMainProg->sCfg.psEntry->psBody);
		}
		{		
			PINST psInst = AllocateInst(psState, IMG_NULL);
			SetOpcode(psState, psInst, IMOV);
			psInst->asDest[0].uType = USEASM_REGTYPE_TEMP;
			psInst->asDest[0].uNumber = USC_TEMPREG_OUTPUT_BUFF_NUM;
			psInst->asArg[0].uType = USEASM_REGTYPE_IMMEDIATE;
			psInst->asArg[0].uNumber = 
				/* 
					Number of vertices the partition can still accomdate. 
				*/
				(((IMG_UINT32)(EURASIA_OUTPUT_PARTITION_SIZE / psState->sShader.psVS->uVertexShaderNumOutputs)) << 16);
			InsertInstBefore(psState, psState->psMainProg->sCfg.psEntry,
							 psInst,
							 psState->psMainProg->sCfg.psEntry->psBody);
		}
	}

	/* Set up information about fixed hardware registers. */
	if (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_PIXEL)
	{
		FinalisePixelShaderIntermediateCode(psState);
	}
	else if ((psState->psSAOffsets->eShaderType == USC_SHADERTYPE_VERTEX) || (psState->psSAOffsets->eShaderType == USC_SHADERTYPE_GEOMETRY))
	{
		FinaliseVertexShaderIntermediateCode(psState);
	}
}

#if defined(OUTPUT_USPBIN)
static 
PCODEBLOCK ConvertTextureWriteToIntermediate(PINTERMEDIATE_STATE psState, 
											 PCODEBLOCK psCodeBlock, 
											 PUNIFLEX_INST psUFInst)
/*****************************************************************************
 FUNCTION	: ConvertTextureWriteToIntermediate

 PURPOSE	: Convert an texture write instruction to the intermediate format.

 PARAMETERS	: psState			- Compiler state.
			  psCodeBlock		- Block to add the intermediate instructions to.
			  psUFInst				- Instruction to convert.

 RETURNS	: None.
*****************************************************************************/
{
	PINST			   	  psTexWriteInst;
	FLOAT_SOURCE_MODIFIER sArgMod;

	/* Try using the new TEXWRITE intermediate instruction */
	psTexWriteInst = AllocateInst(psState, IMG_NULL);
	SetOpcode(psState, psTexWriteInst, ITEXWRITE);

	/* Base address and stride */
	GetSourceTypeless (psState, psCodeBlock, &psUFInst->asSrc[0], 0, &psTexWriteInst->asArg[0], IMG_TRUE, &sArgMod);
	GetSourceTypeless (psState, psCodeBlock, &psUFInst->asSrc[1], 1, &psTexWriteInst->asArg[1], IMG_TRUE, &sArgMod);

	/* Co-ordinates */
	GetSourceTypeless (psState, psCodeBlock, &psUFInst->asSrc[2], 0, &psTexWriteInst->asArg[2], IMG_TRUE, &sArgMod);
	GetSourceTypeless (psState, psCodeBlock, &psUFInst->asSrc[2], 1, &psTexWriteInst->asArg[3], IMG_TRUE, &sArgMod);

	/* Colour */
	GetSourceTypeless (psState, psCodeBlock, &psUFInst->asSrc[3], 0, &psTexWriteInst->asArg[4], IMG_TRUE, &sArgMod);
	GetSourceTypeless (psState, psCodeBlock, &psUFInst->asSrc[3], 1, &psTexWriteInst->asArg[5], IMG_TRUE, &sArgMod);
	GetSourceTypeless (psState, psCodeBlock, &psUFInst->asSrc[3], 2, &psTexWriteInst->asArg[6], IMG_TRUE, &sArgMod);
	GetSourceTypeless (psState, psCodeBlock, &psUFInst->asSrc[3], 3, &psTexWriteInst->asArg[7], IMG_TRUE, &sArgMod);

	/*
	   FIXME: Using 16-bit data for stride and co-ords may cause
	   problems for large texture sizes but it saves implementing
	   I32 multiplication and addition in USP
	*/

	/* Set the ID of the write instruction */
	psTexWriteInst->u.psTexWrite->uWriteID = psUFInst->asSrc[4].uNum;
	psTexWriteInst->u.psTexWrite->uChannelRegFmt  = psUFInst->asSrc[3].eFormat;

	AppendInst(psState, psCodeBlock, psTexWriteInst);

	return psCodeBlock;
}
#endif /* defined(OUTPUT_USPBIN) */

/******************************************************************************
 End of file (icvt_core.c)
******************************************************************************/
