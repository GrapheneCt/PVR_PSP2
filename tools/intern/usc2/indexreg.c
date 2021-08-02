/*************************************************************************
 * Name         : indexreg.c
 * Title        : Eurasia USE Compiler
 * Created      : June 2007
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
 * $Log: indexreg.c $
 **************************************************************************/

#include "uscshrd.h"
#include "reggroup.h"
#include "bitops.h"
#include "usedef.h"
#include <limits.h>

typedef enum _REF_TYPE
{
	REF_TYPE_DEST,
	REF_TYPE_SRC,
} REF_TYPE, *PREF_TYPE;

typedef struct _INDEX_REF
{
	/* Entry in the list of references to a specific interval. */
	USC_LIST_ENTRY		sListEntry;
	/* Instruction which contains the reference. */
	PINST				psInst;
	/* Reference type: source, destination or partially updated destination. */
	REF_TYPE			eType;
	/* Points to the first argument using the index. */
	PARG				psBaseArg;
	/* Index of the first argument using the index in the sources or destinations array. */
	IMG_UINT32			uBaseArgIdx;
	/* 
		Count of arguments using the index at consecutive offsets in the sources or destinations 
		array following the first argument. 
	*/
	IMG_UINT32			uArgCount;
	/*  Required alignment for the start of the group of registers.*/
	HWREG_ALIGNMENT		eArgAlign;
	/* Offset of the index in the array INDEXREGALLOC_CONTEXT.asIndices. */
	IMG_UINT32			uIndex;
} INDEX_REF, *PINDEX_REF;

typedef struct _INDEX_COPY
{
	/*
		Entry in the list of copies of the index.
	*/
	USC_LIST_ENTRY	sListEntry;

	/*
		Temporary register containing the copy of the index.
	*/
	IMG_UINT32		uTempNum;

	/*
		If this is a copy of an element of an indexable array then the offset of the element.
	*/
	IMG_UINT32		uArrayOffset;

	/*
		Instruction to insert the instructions to set up the copy before.
	*/
	PINST			psInsertBeforeInst;

	/*
		Range of possible offsets we could add to the index value when writing the hardware index register.
	*/
	IMG_UINT32		uBaseMin;
	IMG_UINT32		uBaseMax;

	/*
		Either USC_UNDEF or the required value for the offset to the index value MOD 4.
	*/
	IMG_UINT32		uBaseOffsetMod4;

	/* Multiplier for the index value when writing the hardware index register. */
	IMG_UINT32		uStride;

	/*
		TRUE if this interval is used with subcomponent accesses.
	*/
	IMG_BOOL		bSubcomponentIndex;

	/*
		TRUE if this interval is used with subcomponent accesses which span across two 32-bit registers.
	*/
	IMG_BOOL		bSubcomponentAccessesSpanDwords;

	/*
		Register number containing the offset into the first 32-bit register covered by the range of bits accessed
		by the index.
	*/
	IMG_UINT32		uLowDwordShiftTempNum;

	/*
		Register number containing the right shift for the second 32-bit register covered by the range of bits accessed
		by the index.
	*/
	IMG_UINT32		uHighDwordShiftTempNum;
} INDEX_COPY, *PINDEX_COPY;

/*
	Information about a temporary register used for indexing into array of hardware registers.
*/
typedef struct _INDEX
{
	/*
		List of indexed sources/destinations using this register as an index.
	*/
	USC_LIST		sRefList;

	/*
		Set to TRUE if this index is used in current instruction.
	*/
	IMG_BOOL		bUsedHere;

	/*
		Entry in the list of indices used in the current instruction.
	*/
	USC_LIST_ENTRY	sUsedHereListEntry;

	/*
		Points to the next use of this register.
	*/
	PINDEX_REF		psCurrentRef;

	/*
		USE-DEF information for the temporary register.
	*/
	PUSEDEF_CHAIN	psUseDef;

	/*
		If the indexing value is part of a register array then the offset in the
		array.
	*/
	IMG_UINT32		uArrayOffset;

	/*
		The hardware index register assigned to this temporary register or USC_UNDEF
		or one if isn't assigned.
	*/
	IMG_UINT32		uHwRegNum;
} INDEX, *PINDEX;

/*
	Information about a hardware index register.
*/
typedef struct _HW_INDEX
{
	/*
		Points to the temporary register currently assigned to this index register or
		NULL if it is unused.
	*/
	PINDEX			psIndex;

	/*
		Last instruction to use this index register in the current block.
	*/
	PINST			psLastUseInst;
} HW_INDEX, *PHW_INDEX;

typedef struct _INDEXREGALLOC_CONTEXT
{
	/*
		List of MOV instructions writing hardware index registers.
	*/
	USC_LIST	sIndexMoveInstList;

	/*
		Information about the hardware index registers.
	*/
	HW_INDEX	asHwRegs[EURASIA_USE_INDEX_BANK_SIZE];

	/*
		List of all temporary registers used as dynamic indices.
	*/
	PINDEX		asIndices;
} INDEXREGALLOC_CONTEXT, *PINDEXREGALLOC_CONTEXT;

IMG_INTERNAL
IMG_VOID SetSourceTempArg(PINTERMEDIATE_STATE	psState, 
						  PINST					psInst, 
						  IMG_UINT32			uSrcIdx, 
						  IMG_UINT32			uTempNum,
						  UF_REGFORMAT			eTempFmt)
/*****************************************************************************
 FUNCTION	: SetSourceTempArg
    
 PURPOSE	: Set an instruction source to a temporary register.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to set.
			  uSrcIdx		- Index of the source to set.
			  uTempNum		- Temporary register number.
			  eTempFmt		- Temporary register format.

 RETURNS	: Nothing.
*****************************************************************************/
{
	SetSrc(psState, psInst, uSrcIdx, USEASM_REGTYPE_TEMP, uTempNum, eTempFmt);
}

IMG_INTERNAL
IMG_VOID SetDestTempArg(PINTERMEDIATE_STATE	psState, 
					    PINST				psInst, 
						IMG_UINT32			uDestIdx, 
						IMG_UINT32			uTempNum,
						UF_REGFORMAT		eTempFmt)
/*****************************************************************************
 FUNCTION	: SetDestTempArg
    
 PURPOSE	: Set an instruction destination to a temporary register.

 PARAMETERS	: psState		- Compiler state.
			  psInst		- Instruction to set.
			  uDestIdx		- Index of the destination to set.
			  uTempNum		- Temporary register number.
			  eTempFmt		- Temporary register format.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	SetDest(psState, psInst, uDestIdx, USEASM_REGTYPE_TEMP, uTempNum, eTempFmt);
}

static
IMG_VOID SetTempArg(PINTERMEDIATE_STATE psState, 
					PINST				psInst, 
					REF_TYPE			eArgType,
					IMG_UINT32			uArgIdx, 
					IMG_UINT32			uTempNum,
					UF_REGFORMAT		eTempFmt)
/*****************************************************************************
 FUNCTION	: SetTempArg
    
 PURPOSE	: Set an instruction source/destination to a temporary register.

 PARAMETERS	: psState	- Compiler state.
			  psInst	- Instruction to set.
			  eArgType	- Type of argument to set.
			  uArgIdx	- Index of the source or destination to set.
			  uTempNum	- Temporary register number.
			  eTempFmt	- Temporary register format.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	switch (eArgType)
	{
		case REF_TYPE_DEST:
		{
			SetDest(psState, psInst, uArgIdx, USEASM_REGTYPE_TEMP, uTempNum, eTempFmt); 
			break;
		}
		case REF_TYPE_SRC:
		{
			SetSrc(psState, psInst, uArgIdx, USEASM_REGTYPE_TEMP, uTempNum, eTempFmt); 
			break;
		}
		default: imgabort();
	}
}

static
PINST MoveIndexReference(PINTERMEDIATE_STATE	psState,
						 PINST					psInsertBeforeInst,
						 IOPCODE				eNewRefOpcode,
						 IMG_UINT32				uSrcNum,
						 PINDEX_REF				psOldRef,
						 IMG_UINT32				uRefOffset,
						 PINDEX_REF				psNewRef)
/*****************************************************************************
 FUNCTION	: MoveIndexReference
    
 PURPOSE	: Move an indexed source or destination to a separate instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInsertBeforeInst
								- Location to insert the new instruction.
			  eNewRefOpcode		- Opcode for the new instruction.
			  uSrcNum			- Source to the new instruction to copy the
								indexed argument to.
			  psOldRef			- Index reference to move.
			  uRefOffset		- 
			  psNewRef			- Returns details of the new location of the
								indexed argument.
			  
 RETURNS	: A pointer to the new instruction.
*****************************************************************************/
{
	PINST			psMOVInst;
	PINST			psInst = psOldRef->psInst;
	IMG_UINT32		uOldIndexedArg = psOldRef->uBaseArgIdx + uRefOffset;
	PARG			psOldIndexedArg = psOldRef->psBaseArg + uRefOffset;
	UF_REGFORMAT	eIndexedFormat = psOldIndexedArg->eFmt;
	IMG_BOOL		bUnreferencedChannel;
	IMG_UINT32		uReplacementTemp = GetNextRegister(psState);

	/*
		Check for a reference to the index where the instruction doesn't actually reference the
		source or destination. These might be generated where an instruction's sources or
		destinations are required to all be the same register type and index.
	*/
	bUnreferencedChannel = IMG_FALSE;
	if (psOldRef->eType == REF_TYPE_DEST)
	{
		if (psInst->auLiveChansInDest[uOldIndexedArg] == 0)
		{
			bUnreferencedChannel = IMG_TRUE;
		}
	}
	else
	{
		ASSERT(psOldRef->eType == REF_TYPE_SRC);
		if (GetLiveChansInArg(psState,psInst, uOldIndexedArg) == 0)
		{
			bUnreferencedChannel = IMG_TRUE;
		}
	}
	if (bUnreferencedChannel)
	{
		/* 
			Replace the indexed argument by a never written temporary.
		*/
		SetTempArg(psState, psInst, psOldRef->eType, uOldIndexedArg, uReplacementTemp, eIndexedFormat); 
		return NULL;
	}

	/*
		Create a new MOV instruction copying to/from the indexed register.
	*/
	psMOVInst = AllocateInst(psState, psOldRef->psInst);
	SetOpcode(psState, psMOVInst, eNewRefOpcode);
	if (psOldRef->eType == REF_TYPE_DEST)
	{
		ASSERT(psInst->auDestMask[uOldIndexedArg] == USC_ALL_CHAN_MASK);

		/*
			Set the MOV source to the fresh temporary.
		*/
		SetSourceTempArg(psState, psMOVInst, uSrcNum, uReplacementTemp, eIndexedFormat);

		/*
			Move the index reference to the MOV destination.
		*/
		MoveDest(psState, psMOVInst, 0 /* uCopyToIdx */, psInst, uOldIndexedArg);
		CopyPartiallyWrittenDest(psState, psMOVInst, 0 /* uCopyToIdx */, psInst, uOldIndexedArg);

		/*
			Use the same predicate to control update of the destination as the original instruction.
		*/
		CopyPredicate(psState, psMOVInst, psInst);

		/*
			Insert the instruction after the original instruction.
		*/
		InsertInstBefore(psState, psInst->psBlock, psMOVInst, psInsertBeforeInst);
	}
	else
	{
		ASSERT(psOldRef->eType == REF_TYPE_SRC);

		/*
			Set the MOV destination to the fresh temporary.
		*/
		SetDestTempArg(psState, psMOVInst, 0 /* uDestIdx */, uReplacementTemp, eIndexedFormat);

		/*
			Move the index reference to the MOV source.
		*/
		MoveSrc(psState, psMOVInst, uSrcNum, psInst, uOldIndexedArg);

		/*
			Insert the instruction before the original instruction.
		*/
		InsertInstBefore(psState, psInst->psBlock, psMOVInst, psInsertBeforeInst);
	}
		
	/*
		Replace the indexed source/destination by the fresh temporary.
	*/
	SetTempArg(psState, psInst, psOldRef->eType, uOldIndexedArg, uReplacementTemp, eIndexedFormat);

	/*
		Update the location of the reference to the index.
	*/
	psNewRef->psInst = psMOVInst;
	psNewRef->uArgCount = 1;
	if (psOldRef->eType == REF_TYPE_DEST)
	{
		psNewRef->uBaseArgIdx = 0;
		psNewRef->eType = REF_TYPE_DEST;
		psNewRef->psBaseArg = psMOVInst->asDest;
	}
	else
	{
		psNewRef->uBaseArgIdx = uSrcNum;
		psNewRef->eType = REF_TYPE_SRC;
		psNewRef->psBaseArg = psMOVInst->asArg + uSrcNum;
	}

	return psMOVInst;
}

static
IMG_BOOL SpansTwoRegisters(IMG_UINT32 uUsedChanMask, IMG_UINT32 uStrideInBytes)
/*****************************************************************************
 FUNCTION	: SpansTwoRegisters
    
 PURPOSE	: Check if the range of bits selected by an index spans two
			  32-bit registers.

 PARAMETERS	: uUsedChanMask		- Mask of channels used from the indexed source.
			  uStrideInBytes	- Units of the index.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	if ((uUsedChanMask & (~USC_X_CHAN_MASK)) == 0)
	{
		/*
			Only a single byte is used.
		*/
		return IMG_FALSE;
	}
	else if ((uUsedChanMask & (~USC_XY_CHAN_MASK)) == 0 && (uStrideInBytes % 2) == 0)
	{
		/*
			A single word is used at a word aligned address.
		*/
		return IMG_FALSE;
	}
	else
	{
		return IMG_TRUE;
	}
}

static
PINST ExpandSubcomponentRead_NonIDXSC(PINTERMEDIATE_STATE	psState, 
									  PINST					psInsertBeforeInst,
									  IMG_BOOL				bSpansTwoRegisters,
									  PINST					psSourceInst,
									  IMG_UINT32			uSourceIdx,
									  PINDEX_COPY			psIndexCopy)
/*****************************************************************************
 FUNCTION	: ExpandSubcomponentRead_NonIDXSC
    
 PURPOSE	: Expand an instruction source indexing down to subcomponents of a 32-bit
			  register.

 PARAMETERS	: psState				- Compiler state.
			  psContext				- Index register allocator state.
			  psInsertBeforeInst	- Point to insert the new instructions.
			  bSpansTwoRegisters	- TRUE if the indexed data potentially spans
									two 32-bit registers.
			  psSourceInst			- Instruction containing the subcomponent
									indexed source.
			  uSourceIdx			- Offset of the subcomponent indexed source.
			  psIndexCopy			- Index used by the source.
			  
 RETURNS	: The first generated instruction.
*****************************************************************************/
{
	PINST			psSHRInst;
	IMG_UINT32		uReplacementTempNum = GetNextRegister(psState);
	IMG_UINT32		uFirstRegBitsTempNum = USC_UNDEF;
	UF_REGFORMAT	eIndexedFmt = psSourceInst->asArg[uSourceIdx].eFmt;

	/*
		Shift the start of the range of bits to the LSB.
	*/
	psSHRInst = AllocateInst(psState, psInsertBeforeInst);
	SetOpcode(psState, psSHRInst, ISHR);

	if (!bSpansTwoRegisters)
	{
		SetDestTempArg(psState, psSHRInst, 0 /* uDestIdx */, uReplacementTempNum, eIndexedFmt);
	}
	else
	{
		uFirstRegBitsTempNum = GetNextRegister(psState);
		SetDestTempArg(psState, psSHRInst, 0 /* uDestIdx */, uFirstRegBitsTempNum, UF_REGFORMAT_F32);
	}

	CopySrc(psState, psSHRInst, BITWISE_SHIFTEND_SOURCE, psSourceInst, uSourceIdx);
	SetSrcIndex(psState, 
				psSHRInst, 
				BITWISE_SHIFTEND_SOURCE, 
				USEASM_REGTYPE_TEMP, 
				psIndexCopy->uTempNum, 
				0 /* uNewIndexArrayOffset */, 
				LONG_SIZE /* uNewIndexRelativeStrideInBytes */);

	SetSourceTempArg(psState, psSHRInst, BITWISE_SHIFT_SOURCE, psIndexCopy->uLowDwordShiftTempNum, UF_REGFORMAT_F32);

	InsertInstBefore(psState, psInsertBeforeInst->psBlock, psSHRInst, psInsertBeforeInst);

	if (bSpansTwoRegisters)
	{
		PINST			psSHLInst;
		PINST			psORInst;
		IMG_UINT32		uSecondRegBitsTempNum = GetNextRegister(psState);

		/*
			Shift the bits accessed from the start of the second register to the
			bit position after the bits accessed from the first register.
		*/
		psSHLInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psSHLInst, ISHL);
		
		SetDestTempArg(psState, psSHLInst, 0 /* uDestIdx */, uSecondRegBitsTempNum, UF_REGFORMAT_F32);

		CopySrc(psState, psSHLInst, BITWISE_SHIFTEND_SOURCE, psSourceInst, uSourceIdx);
		SetSrcIndex(psState, 
					psSHLInst, 
					BITWISE_SHIFTEND_SOURCE, 
					USEASM_REGTYPE_TEMP, 
					psIndexCopy->uTempNum, 
					0 /* uNewIndexArrayOffset */, 
					LONG_SIZE /* uNewIndexRelativeStrideInBytes */);
		if (psSHLInst->asArg[0].uType == USC_REGTYPE_REGARRAY)
		{
			psSHLInst->asArg[BITWISE_SHIFTEND_SOURCE].uArrayOffset++;
		}
		else
		{
			psSHLInst->asArg[BITWISE_SHIFTEND_SOURCE].uNumber++;
		}

		SetSourceTempArg(psState, psSHLInst, BITWISE_SHIFT_SOURCE, psIndexCopy->uHighDwordShiftTempNum, UF_REGFORMAT_F32);

		InsertInstBefore(psState, psInsertBeforeInst->psBlock, psSHLInst, psInsertBeforeInst);

		/*
			Combine the two ranges of bits.
		*/
		psORInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psORInst, IOR);
		SetDestTempArg(psState, psORInst, 0 /* uDestIdx */, uReplacementTempNum, eIndexedFmt);
		SetSourceTempArg(psState, psORInst, 0 /* uSrcIdx */, uSecondRegBitsTempNum, UF_REGFORMAT_F32);
		SetSourceTempArg(psState, psORInst, 1 /* uSrcIdx */, uFirstRegBitsTempNum, UF_REGFORMAT_F32);
		InsertInstBefore(psState, psInsertBeforeInst->psBlock, psORInst, psInsertBeforeInst);
	}

	/*
		Replace the indexed source to the original instruction 
	*/
	SetSourceTempArg(psState, psSourceInst, uSourceIdx, uReplacementTempNum, eIndexedFmt);

	return psSHRInst;
}

static
IMG_VOID SetIndexSource(PINTERMEDIATE_STATE	psState,
						PINST				psInst,
						PUSEDEF_CHAIN		psIndex,
						IMG_UINT32			uIndexArrayOffset,
						IMG_UINT32			uArgIdx)
/*****************************************************************************
 FUNCTION	: SetIndexSource
    
 PURPOSE	: Set the intermediate register containing a value used for 
			  indexing into an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  psIndex			- Index to set.
			  uIndexArrayOffset	- If the index is an element in a register array
								then the offset of the element.
			  uArgIdx			- Instruction argument to set.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psIndex->uType == USC_REGTYPE_REGARRAY)
	{
		SetArraySrc(psState, 
					psInst, 
					uArgIdx, 
					psIndex->uNumber, 
					uIndexArrayOffset,
					psIndex->eFmt);
	}
	else
	{
		SetSrc(psState, 
			   psInst, 
			   uArgIdx, 
			   psIndex->uType, 
			   psIndex->uNumber,
			   psIndex->eFmt);
	}
}

static
IMG_VOID SetIndex(PINTERMEDIATE_STATE	psState,
				  PINST					psInst,
				  PUSEDEF_CHAIN			psIndex,
				  IMG_UINT32			uIndexArrayOffset,
				  IMG_UINT32			uArgIdx)
/*****************************************************************************
 FUNCTION	: SetIndex
    
 PURPOSE	: Set the intermediate register containing a value used for 
			  indexing into an instruction.

 PARAMETERS	: psState			- Compiler state.
			  psInst			- Instruction.
			  psIndex			- Index to set.
			  uIndexArrayOffset	- If the index is an element in a register array
								then the offset of the element.
			  uArgIdx			- Instruction argument to set.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32		uIndexHwType;

	uIndexHwType = GetPrecolouredRegisterType(psState, psInst, psIndex->uType, psIndex->uNumber);
	if (!CanUseSrc(psState, psInst, uArgIdx, uIndexHwType, USC_REGTYPE_NOINDEX))
	{
		IMG_UINT32	uIndexCopyTempNum;
		PINST		psMOVInst;

		/*
			Use a MOV to create a copy from the intermediate register to
			a newly allocated temporary.
		*/
		uIndexCopyTempNum = GetNextRegister(psState);

		psMOVInst = AllocateInst(psState, psInst);
		SetOpcode(psState, psMOVInst, IMOV);

		SetDestTempArg(psState, psMOVInst, 0 /* uDestIdx */, uIndexCopyTempNum, UF_REGFORMAT_F32);

		SetIndexSource(psState, psMOVInst, psIndex, uIndexArrayOffset, 0 /* uArgIdx */);

		InsertInstBefore(psState, psInst->psBlock, psMOVInst, psInst);

		/*
			Use the replacement temporary in the instruction.
		*/
		SetSourceTempArg(psState, psInst, uArgIdx, uIndexCopyTempNum, UF_REGFORMAT_F32);
	}
	else
	{
		/*
			Set the intermediate register directly into the instruction.
		*/
		SetIndexSource(psState, psInst, psIndex, uIndexArrayOffset, uArgIdx);
	}
}

static
IMG_VOID SetupSubcomponentSelectIndex(PINTERMEDIATE_STATE		psState,
									  PINST						psInsertBeforeInst,
									  PUSEDEF_CHAIN				psIndex,
									  PINDEX_COPY				psIndexCopy)
/*****************************************************************************
 FUNCTION	: SetupSubcomponentSelectIndex
    
 PURPOSE	: Set up the parameters needed by a subcomponent index.

 PARAMETERS	: psState			- Compiler state.
			  psInsertBeforeInst
								- Location to insert the setup instructions.
			  psIndex			- Source index value.
			  psIndexCopy		- Details of the ranges of the offsets used with
								the index.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uBitIndexTempNum = GetNextRegister(psState);

	/*
		Get the start of the range of bits selected by the index.
	*/
	{
		PINST		psIMAEInst;

		psIMAEInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psIMAEInst, IIMAE);
		InsertInstBefore(psState, psInsertBeforeInst->psBlock, psIMAEInst, psInsertBeforeInst);
		SetDestTempArg(psState, psIMAEInst, 0 /* uDestIdx */, uBitIndexTempNum, UF_REGFORMAT_F32);
		SetIndex(psState, psIMAEInst, psIndex, psIndexCopy->uArrayOffset, 0 /* uArgIdx */);
		LoadImmediate(psState, psIMAEInst, 1 /* uArgIdx */, psIndexCopy->uStride * BITS_PER_BYTE);
		LoadImmediate(psState, psIMAEInst, 2 /* uArgIdx */, psIndexCopy->uBaseMin * BITS_PER_BYTE * LONG_SIZE);
	}

	/*
		Calculate the offset of the first 32-bit register containing the range of bits
		selected by the index.

			INDEX = BIT_SHIFT >> 5
	*/
	{
		PINST	psSHRInst;

		psSHRInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psSHRInst, ISHR);
		InsertInstBefore(psState, psInsertBeforeInst->psBlock, psSHRInst, psInsertBeforeInst);
		SetDest(psState, psSHRInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, psIndexCopy->uTempNum, UF_REGFORMAT_F32);
		SetSourceTempArg(psState, psSHRInst, BITWISE_SHIFTEND_SOURCE, uBitIndexTempNum, UF_REGFORMAT_F32);
		LoadImmediate(psState, psSHRInst, BITWISE_SHIFT_SOURCE /* uArgIdx */, BITS_PER_UINT_LOG2);
	}

	{
		PINST	psANDInst;

		/*
			Calculate the offset within the 32-bit register of the start of the range of bits.

				LOW_DWORD_SHIFT = BIT_SHIFT & 31
		*/
		psANDInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psANDInst, IAND);
		InsertInstBefore(psState, psInsertBeforeInst->psBlock, psANDInst, psInsertBeforeInst);
		SetDestTempArg(psState, psANDInst, 0 /* uDestIdx */, psIndexCopy->uLowDwordShiftTempNum, UF_REGFORMAT_F32);
		SetSourceTempArg(psState, psANDInst, 0 /* uSrcIdx */, uBitIndexTempNum, UF_REGFORMAT_F32);
		LoadImmediate(psState, psANDInst, 1 /* uArgIdx */, BITS_PER_UINT - 1);
	}
	
	if (psIndexCopy->bSubcomponentAccessesSpanDwords)
	{
		PINST	psIMAEInst;

		/*
			Get the shift amount within the second 32-bit register.

				HIGH_DWORD_SHIFT = 32 - LOW_DWORD_SHIFT
		*/			
		psIMAEInst = AllocateInst(psState, psInsertBeforeInst);
		SetOpcode(psState, psIMAEInst, IIMAE);
		InsertInstBefore(psState, psInsertBeforeInst->psBlock, psIMAEInst, psInsertBeforeInst);
		SetDestTempArg(psState, psIMAEInst, 0 /* uDestIdx */, psIndexCopy->uHighDwordShiftTempNum, UF_REGFORMAT_F32);
		SetSourceTempArg(psState, psIMAEInst, 0 /* uSrcIdx */, psIndexCopy->uLowDwordShiftTempNum, UF_REGFORMAT_F32);
		LoadImmediate(psState, psIMAEInst, 1 /* uArgIdx */, UINT_MAX);
		LoadImmediate(psState, psIMAEInst, 2 /* uArgIdx */, BITS_PER_UINT);
	}
}

static
IMG_BOOL UsesFormatSelect(PINST	psInst, IMG_BOOL bDest)
/*****************************************************************************
 FUNCTION	: UsesFormatSelect
    
 PURPOSE	: Checks for an instruction where the top-bit of the register numbers
			  selects between two interpretations of the register contents.

 PARAMETERS	: psInst		- Instruction to check.
			  bDest			- If TRUE check for destinations.
							  If FALSE check for sources.
			  
 RETURNS	: TRUE or FALSE.
*****************************************************************************/
{
	UF_REGFORMAT	eAltFormat;
	IMG_UINT32		uSrcIdx;

	if (HasC10FmtControl(psInst))
	{
		eAltFormat = UF_REGFORMAT_C10;
	}
	else if (!bDest && InstUsesF16FmtControl(psInst))
	{
		eAltFormat = UF_REGFORMAT_F16;
	}
	else
	{
		return IMG_FALSE;
	}
	
	if (eAltFormat == UF_REGFORMAT_C10)
	{
		IMG_UINT32	uDestIdx;

		for (uDestIdx = 0; uDestIdx < psInst->uDestCount; uDestIdx++)
		{
			if (psInst->asDest[uDestIdx].eFmt == eAltFormat)
			{
				return IMG_TRUE;
			}
		}
	}
	for (uSrcIdx = 0; uSrcIdx < psInst->uArgumentCount; uSrcIdx++)
	{
		if (psInst->asArg[uSrcIdx].eFmt == eAltFormat)
		{
			return IMG_TRUE;
		}
	}

	return IMG_FALSE;
}

static
IMG_UINT32 GetMaximumIndexStaticOffset(PINST psInst, REF_TYPE eType, PARG psArg)
/*****************************************************************************
 FUNCTION	: GetMaximumIndexStaticOffset
    
 PURPOSE	: Get the maximum static offset which can be applied to an indexed
			  access by an instruction.

 PARAMETERS	: psInst		- Instruction to check.
			  eType			- Type of argument to check for.
			  psArg			- Argument to check for.
			  
 RETURNS	: The maximum static offset in bytes.
*****************************************************************************/
{
	IMG_BOOL	bDest;

	bDest = IMG_FALSE;
	if (eType == REF_TYPE_DEST)
	{
		bDest = IMG_TRUE;
	}

	if (UsesFormatSelect(psInst, bDest))
	{
		if (psArg->eFmt == UF_REGFORMAT_F16)
		{
			return EURASIA_USE_FCINDEX_MAXIMUM_OFFSET * WORD_SIZE;
		}
		else
		{
			return EURASIA_USE_FCINDEX_MAXIMUM_OFFSET * LONG_SIZE;
		}
	}
	else
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		IMG_BOOL	bDoubleRegisters;

		bDoubleRegisters = IMG_FALSE;
		if (bDest)
		{
			if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORDEST) != 0)
			{
				bDoubleRegisters = IMG_TRUE;
			}
		}
		else
		{
			if ((g_psInstDesc[psInst->eOpcode].uFlags & DESC_FLAGS_VECTORSRC) != 0)
			{
				bDoubleRegisters = IMG_TRUE;
			}
		}

		if (bDoubleRegisters)
		{
			return EURASIA_USE_FCINDEX_MAXIMUM_OFFSET * QWORD_SIZE;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

		return EURASIA_USE_INDEX_MAXIMUM_OFFSET * LONG_SIZE;
	}
}

static
PINDEX_COPY FindMatchingIndexCopy(PINTERMEDIATE_STATE	psState,
							      PUSC_LIST				psIndexCopyList,
								  IMG_UINT32			uArrayOffset,
								  PCODEBLOCK			psBlock,
							      IMG_UINT32			uBaseMin,
							      IMG_UINT32			uBaseMax,
							      IMG_UINT32			uBaseOffsetMod4,
							      IMG_UINT32			uStride,
							      IMG_BOOL				bSubcomponentIndex,
							      IMG_BOOL				bSpansTwoRegisters)
/*****************************************************************************
 FUNCTION	: FindMatchingIndexCopy
    
 PURPOSE	: Look for a copy of an index with parameters compatible with an indexed
			  access. If no compatible interval exists then create a new one.

 PARAMETERS	: psState			- Compiler state.
			  psIndexCopyList	- List of existing copies of the index.
			  uArrayOffset		- If the index is itself an array then the offset
								of the element to copy.
			  psBlock			- Flow control block where the index is used.
			  uBaseMin			- Minimum offset to apply to the index value.
			  uBaseMax			- Maximum offset to apply to the index value.
			  uBaseOffsetMod4	- Either USC_UNDEF or the required value for the
								index offset MOD 4
			  uStride			- Stride of the index value.
			  uSubcomponentIndex
								- TRUE if the indexed access is not 32-bit
								aligned.
			  bSpansTwoRegisters - For subcomponent accesses TRUE if the accessed
								dword spans two 32-bit registers.
			  
 RETURNS	: A pointer to the index copy.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psIndexCopyListEntry;
	PINDEX_COPY		psIndexCopy;

	for (psIndexCopyListEntry = psIndexCopyList->psHead; 
		 psIndexCopyListEntry != NULL; 
		 psIndexCopyListEntry = psIndexCopyListEntry->psNext)
	{
		psIndexCopy = IMG_CONTAINING_RECORD(psIndexCopyListEntry, PINDEX_COPY, sListEntry);

		if (
				psIndexCopy->uArrayOffset == uArrayOffset &&
				psIndexCopy->psInsertBeforeInst->psBlock == psBlock &&
				psIndexCopy->uBaseMax >= uBaseMin &&
				psIndexCopy->uBaseMin <= uBaseMax &&
				(
					uBaseOffsetMod4 == USC_UNDEF ||
					psIndexCopy->uBaseOffsetMod4 == USC_UNDEF ||
					uBaseOffsetMod4 == psIndexCopy->uBaseOffsetMod4
				) &&
				psIndexCopy->uStride == uStride &&
				psIndexCopy->bSubcomponentIndex == bSubcomponentIndex
		   )
		{
			if (bSpansTwoRegisters)
			{
				psIndexCopy->bSubcomponentAccessesSpanDwords = IMG_TRUE;
			}
			psIndexCopy->uBaseMax = min(psIndexCopy->uBaseMax, uBaseMax);
			psIndexCopy->uBaseMin = max(psIndexCopy->uBaseMin, uBaseMin);
			if (psIndexCopy->uBaseOffsetMod4 == USC_UNDEF)
			{
				psIndexCopy->uBaseOffsetMod4 = uBaseOffsetMod4;
			}
			return psIndexCopy;
		}
	}
	
	/*
		Create a new interval using the correct base/stride for this
		index reference.
	*/
	psIndexCopy = UscAlloc(psState, sizeof(*psIndexCopy));
	psIndexCopy->uBaseMax = uBaseMax;
	psIndexCopy->uBaseMin = uBaseMin;
	psIndexCopy->uBaseOffsetMod4 = uBaseOffsetMod4;
	psIndexCopy->uStride = uStride;	
	psIndexCopy->uArrayOffset = uArrayOffset;
	psIndexCopy->psInsertBeforeInst = NULL;

	/*
		Allocate a temporary register to hold the copy of the index.
	*/
	psIndexCopy->uTempNum = GetNextRegister(psState);
	
	/*
		Copy parameters for subcomponent indexing.
	*/
	psIndexCopy->bSubcomponentIndex = bSubcomponentIndex;
	psIndexCopy->bSubcomponentAccessesSpanDwords = bSpansTwoRegisters;
	psIndexCopy->uLowDwordShiftTempNum = USC_UNDEF;
	psIndexCopy->uHighDwordShiftTempNum = USC_UNDEF;

	/*
		Add to the list of copies of this index.
	*/
	AppendToList(psIndexCopyList, &psIndexCopy->sListEntry);		

	return psIndexCopy;
}

static
IMG_UINT32 GetArrayPrecolouredOffset(PINTERMEDIATE_STATE psState, PARG psArg)
/*****************************************************************************
 FUNCTION	: GetArrayPrecolouredOffset
    
 PURPOSE	: Gets the hardware register number of the base of a register array
			  (if known).

 PARAMETERS	: psState			- Compiler state.
			  psArg
			  
 RETURNS	: The hardware register number of the base of the array.
*****************************************************************************/
{
	PUSC_VEC_ARRAY_REG	psArray;

	ASSERT(psArg->uType == USC_REGTYPE_REGARRAY);

	ASSERT(psArg->uNumber < psState->uNumVecArrayRegs);
	psArray = psState->apsVecArrayReg[psArg->uNumber];

	if (psArray->eArrayType != ARRAY_TYPE_NORMAL)
	{
		PREGISTER_GROUP	psArrayBase;

		/*
			If this array is precoloured to a particular range of hardware registers
			then check post-register allocation static offset.
		*/
		psArrayBase = FindRegisterGroup(psState, psArray->uBaseReg);
		if (
				psArrayBase != NULL && 
				psArrayBase->psFixedReg != NULL &&
				psArrayBase->psFixedReg->sPReg.uNumber != USC_UNDEF
			)
		{
			return psArrayBase->psFixedReg->sPReg.uNumber;
		}
	}
	return 0;
}

static
IMG_UINT32 GetArrayOffset(PINTERMEDIATE_STATE	psState,
						  PARG					psArg)
/*****************************************************************************
 FUNCTION	: GetArrayOffset
    
 PURPOSE	: Get the static offset applied to an indexed value in an instruction
			  argument.

 PARAMETERS	: psState			- Compiler state.
			  psArg
			  
 RETURNS	: The static offset in dwords.
*****************************************************************************/
{
	IMG_UINT32	uArrayOffsetInDwords;

	if (psArg->uType == USC_REGTYPE_REGARRAY)
	{
		uArrayOffsetInDwords = psArg->uArrayOffset;
		uArrayOffsetInDwords += GetArrayPrecolouredOffset(psState, psArg);
	}
	else
	{
		uArrayOffsetInDwords = psArg->uNumber;
	}

	return uArrayOffsetInDwords;
}

static
IMG_VOID UpdateCopyInsertPoint(PINDEX_COPY psIndexCopy, PINST psUseInst)
/*****************************************************************************
 FUNCTION	: UpdateCopyInsertPoint
    
 PURPOSE	: Make sure the instructions to set up an index are inserted
			  before a particular instruction.

 PARAMETERS	: psIndexCopy		- Index copy to set the insertion point for.
			  psUseInst			- Instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	if (psIndexCopy->psInsertBeforeInst == NULL || psUseInst->uBlockIndex < psIndexCopy->psInsertBeforeInst->uBlockIndex)
	{
		psIndexCopy->psInsertBeforeInst = psUseInst;
	}
}

static
IMG_VOID SetupAlignedIndex(PINTERMEDIATE_STATE		psState,
						   PUSC_LIST				psIndexCopyList,
						   PINDEX_REF				psRef,
						   IMG_UINT32				uArrayOffsetInDwords,
						   IMG_UINT32				uInstStrideInBytes)
/*****************************************************************************
 FUNCTION	: SetupAlignedIndex
    
 PURPOSE	: Set up an index register for an index where the stride required
			  is a multiple of the stride used by the instruction.

 PARAMETERS	: psState			- Compiler state.
			  psIndexCopyList	- List of intervals generated for different
								offsets and strides.
			  psRef				- Source to fix.
			  uArrayOffsetInDwords
								- Static offset to the index value applied by the
								source.
			  uInstStrideInBytes
								- Scale applied to the index value by the 
								instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uStride;
	IMG_UINT32	uBaseMin;
	IMG_UINT32	uBaseMax;
	IMG_UINT32	uArrayOffsetInBytes;
	IMG_UINT32	uMaxStaticOffsetInBytes;
	PINDEX_COPY	psIndexCopy;

	/*
		When writing the hardware index register scale up the index value by the difference between
		the units of the index and the units of the instruction.
	*/
	ASSERT(psRef->psBaseArg->uIndexStrideInBytes >= uInstStrideInBytes);
	ASSERT((psRef->psBaseArg->uIndexStrideInBytes % uInstStrideInBytes) == 0);
	uStride = psRef->psBaseArg->uIndexStrideInBytes / uInstStrideInBytes;
		
	/*
		Calculate the range of offsets we could add to the hardware index register when setting
		it up so the static offset in this instruction fits into the range supported by the
		hardware.
	*/
	uArrayOffsetInBytes = uArrayOffsetInDwords * LONG_SIZE;
	if (InstUsesF16FmtControl(psRef->psInst))
	{
		uArrayOffsetInBytes += GetComponentSelect(psState, psRef->psInst, psRef->uBaseArgIdx);
	}

	uBaseMax = uArrayOffsetInBytes;
	uMaxStaticOffsetInBytes = GetMaximumIndexStaticOffset(psRef->psInst, psRef->eType, psRef->psBaseArg);
	if (uArrayOffsetInBytes < uMaxStaticOffsetInBytes)
	{
		uBaseMin = 0;
	}
	else
	{
		uBaseMin = uArrayOffsetInBytes - uMaxStaticOffsetInBytes;
	}

	uBaseMax /= uInstStrideInBytes;
	uBaseMin /= uInstStrideInBytes;

	/*
		Find or create an interval using the same stride and an overlapping range of offsets.
	*/
	psIndexCopy = 
		FindMatchingIndexCopy(psState, 
							  psIndexCopyList, 
							  psRef->psBaseArg->uIndexArrayOffset,
							  psRef->psInst->psBlock,
							  uBaseMin, 
							  uBaseMax,
							  USC_UNDEF /* uBaseOffsetMod4 */,
							  uStride,
							  IMG_FALSE /* bSubcomponentIndex */,
							  IMG_FALSE /* bSpansTwoRegisters */);

	/*
		Make sure the instructions to set up the index are inserted before the current instruction.
	*/
	UpdateCopyInsertPoint(psIndexCopy, psRef->psInst);

	/*
		Replace the index used at this instruction by the copy.
	*/
	if (psRef->eType == REF_TYPE_DEST)
	{
		IMG_UINT32	uArg;

		for (uArg = 0; uArg < psRef->uArgCount; uArg++)
		{
			SetDestIndex(psState, 
						 psRef->psInst, 
						 psRef->uBaseArgIdx + uArg, 
						 USEASM_REGTYPE_TEMP, 
						 psIndexCopy->uTempNum, 
						 0 /* uNewIndexArrayOffset */, 
						 uInstStrideInBytes);
		}
	}
	else
	{
		IMG_UINT32	uArg;

		for (uArg = 0; uArg < psRef->uArgCount; uArg++)
		{
			SetSrcIndex(psState, 
						psRef->psInst, 
						psRef->uBaseArgIdx + uArg, 
						USEASM_REGTYPE_TEMP, 
						psIndexCopy->uTempNum, 
						0 /* uNewIndexArrayOffset */, 
						uInstStrideInBytes);
		}
	}
}

static
IMG_VOID FixInvalidIndexStrideSource_NonIDXSC(PINTERMEDIATE_STATE		psState,
											  PUSC_LIST					psIndexCopyList,
											  PINDEX_REF				psOldRef,
											  IMG_UINT32				uOldRefOffset,
											  IMG_UINT32				uArrayOffsetInDwords)
/*****************************************************************************
 FUNCTION	: FixInvalidIndexStrideSource_NonIDXSC
    
 PURPOSE	: Fix an indexed source which uses a stride not supported by the
			  instruction on cores which don't support the IDXSCR instruction.

 PARAMETERS	: psState			- Compiler state.
			  sIndexCopyList	- List of intervals generated for different
								offsets and strides.
			  psInterval		- Index register.
			  psRef				- Source to fix.
			  uArrayOffsetInDwords
								- Static offset to the index value applied by the
								source.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_BOOL	bSpansTwoRegisters;
	IMG_UINT32	uBaseMin;
	IMG_UINT32	uBaseMax;
	PINDEX_COPY	psSCInterval;
	IMG_UINT32	uUsedChans;
	IMG_UINT32	uOldArgIdx = psOldRef->uBaseArgIdx + uOldRefOffset;
	PARG		psOldArg = psOldRef->psBaseArg + uOldRefOffset;
	IMG_UINT32	uMaxOffsetInDwords;
	PINST		psFirstSelInst;

	/*
		Get the mask of channels referenced from this source by the instruction.
	*/
	ASSERT(psOldRef->eType == REF_TYPE_SRC);
	uUsedChans = GetLiveChansInArg(psState, psOldRef->psInst, uOldArgIdx);

	/*
		Does the range of bits loaded by the index potentially span two 32-bit registers.
	*/
	bSpansTwoRegisters = SpansTwoRegisters(uUsedChans, psOldArg->uIndexStrideInBytes);

	/*
		Reduce the maximum offset if spanning between two dwords so we can add 1 to the source's
		static offset to reach the second dword.
	*/
	uMaxOffsetInDwords = EURASIA_USE_INDEX_MAXIMUM_OFFSET;
	if (bSpansTwoRegisters)
	{
		uMaxOffsetInDwords--;
	}

	/*
		Calculate the ranges for the static offset added onto the index value when writing
		the index register.
	*/
	uBaseMax = uArrayOffsetInDwords;
	if (uArrayOffsetInDwords < uMaxOffsetInDwords)
	{
		uBaseMin = 0;
	}
	else
	{
		uBaseMin = uArrayOffsetInDwords - uMaxOffsetInDwords;
	}

	/*
		Find or create a suitable index value for the subcomponent index.
	*/
	psSCInterval = FindMatchingIndexCopy(psState, 
										 psIndexCopyList,
										 psOldRef->psBaseArg->uIndexArrayOffset,
										 psOldRef->psInst->psBlock,
										 uBaseMin, 
										 uBaseMax,
										 USC_UNDEF /* uBaseOffsetMod4 */,
 										 psOldArg->uIndexStrideInBytes,
										 IMG_TRUE /* bSubcomponentIndex */,
										 bSpansTwoRegisters);

	/*
		Allocate temporary registers to hold the offset from the start of the register of the range of bits
		selected by the index.
	*/
	if (psSCInterval->uLowDwordShiftTempNum == USC_UNDEF)
	{
		psSCInterval->uLowDwordShiftTempNum = GetNextRegister(psState);
	}
	if (psSCInterval->uHighDwordShiftTempNum == USC_UNDEF)
	{
		psSCInterval->uHighDwordShiftTempNum = GetNextRegister(psState);
	}

	/*
		Generate the shift instructions to extract the range of bits selected by the index.
	*/
	psFirstSelInst = 
		ExpandSubcomponentRead_NonIDXSC(psState,
										psOldRef->psInst, 
										bSpansTwoRegisters,
										psOldRef->psInst,
										uOldArgIdx,
										psSCInterval);

	/*
		Make sure the instructions to set up the index are inserted before the shift
		instructions.
	*/
	UpdateCopyInsertPoint(psSCInterval, psFirstSelInst);
}

static
IMG_VOID FixInvalidIndexStrideSource(PINTERMEDIATE_STATE		psState,
									 PUSC_LIST					psIndexCopyList,
									 PINDEX_REF					psOldRef,
									 IMG_UINT32					uRefOffset)
/*****************************************************************************
 FUNCTION	: FixInvalidIndexStrideSource
    
 PURPOSE	: Fix an indexed source which uses a stride not supported by the
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psIndexCopyList	- List of intervals generated for different
								offsets and strides.
			  psInterval		- Index register.
			  psRef				- Source to fix.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PARG		psInstSource = psOldRef->psBaseArg + uRefOffset;
	IMG_UINT32	uArrayOffsetInDwords = GetArrayOffset(psState, psInstSource);

	/*
		If the desired stride is a multiple of a dword then use MOV (which has the
		right index stride).
	*/
	if ((psInstSource->uIndexStrideInBytes % LONG_SIZE) == 0)
	{
		INDEX_REF	sNewRef;
		PINST		psNewInst;
		PINST		psNewInstInsertPoint;

		if (psOldRef->eType == REF_TYPE_SRC)
		{
			psNewInstInsertPoint = psOldRef->psInst;
		}
		else
		{
			psNewInstInsertPoint = psOldRef->psInst->psNext;
		}

		psNewInst = 
			MoveIndexReference(psState,
							   psNewInstInsertPoint,
							   IMOV, 
							   0 /* uSrcNum */, 
							   psOldRef, 
							   uRefOffset, 
							   &sNewRef);
		if (psNewInst == NULL)
		{
			return;
		}

		SetupAlignedIndex(psState, 
						  psIndexCopyList,
						  &sNewRef,
						  uArrayOffsetInDwords,
						  LONG_SIZE /* uInstStrideInBytes */);
		return;
	}

	#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
	/*
		If the hardware supports it then use the IDXSCR instruction for word granularity accesses to
		registers.
	*/
	if (
			(psState->psTargetFeatures->ui32Flags2 & SGX_FEATURE_FLAGS2_USE_IDXSC) != 0 && 
			(psState->psTargetBugs->ui32Flags & SGX_BUG_FLAGS_FIX_HW_BRN_30871) == 0 &&
			(psInstSource->uIndexStrideInBytes % WORD_SIZE) == 0
	   )
	{
		IMG_UINT32		uUsedChans;
		IMG_BOOL		bLoadBothHalves;
		PINST			psInst = psOldRef->psInst;
		IMG_UINT32		uUpperHalfTemp = USC_UNDEF;
		IMG_UINT32		uLowerHalfTemp = USC_UNDEF;
		IMG_UINT32		uArrayOffsetInBytes;
		IMG_UINT32		uMaximumInstStaticOffsetInBytes;
		IMG_UINT32		uBaseMax;
		IMG_UINT32		uBaseMin;
		IMG_UINT32		uOffsetAlignment;
		IMG_UINT32		uHalf;
		IMG_UINT32		uReplacementTemp = GetNextRegister(psState);
		PARG			psOldArg = psOldRef->psBaseArg + uRefOffset;
		IMG_UINT32		uIndexArrayOffset = psOldArg->uIndexArrayOffset;
		UF_REGFORMAT	eOldArgFmt = psOldArg->eFmt;

		/*
			Get the mask of channels referenced from this source by the instruction.
		*/
		ASSERT(psOldRef->eType == REF_TYPE_SRC);
		uUsedChans = GetLiveChansInArg(psState, psOldRef->psInst, psOldRef->uBaseArgIdx + uRefOffset);

		/*
			Does the instruction need a whole dword to be loaded?
		*/
		bLoadBothHalves = IMG_FALSE;
		if ((uUsedChans & USC_XY_CHAN_MASK) != 0 && (uUsedChans & USC_ZW_CHAN_MASK) != 0)
		{
			bLoadBothHalves = IMG_TRUE;
		}

		/*
			Convert the static offset from the index into bytes.
		*/
		uArrayOffsetInBytes = uArrayOffsetInDwords * LONG_SIZE;

		/*
			Get the maximum static offset which can be encoded in the instruction.
		*/
		uMaximumInstStaticOffsetInBytes = SGXVEC_USE0_OTHER2_IDXSC_INDEXOFF_MAXIMUM * LONG_SIZE;

		/*
			The static offset encoded in the instruction is in units of dwords so any
			non-dword aligned part must be added onto the dynamic index.
		*/
		uOffsetAlignment = uArrayOffsetInBytes % LONG_SIZE;

		/*
			Calculate the maximum possible value for the offset added onto the dynamic index.
		*/
		uBaseMax = uArrayOffsetInBytes;

		/*
			Calculate the minimum possible value for the offset added onto the dynamic index.
		*/
		if (uArrayOffsetInBytes < uMaximumInstStaticOffsetInBytes)
		{
			uBaseMin = uOffsetAlignment;
		}
		else
		{
			uBaseMin = (uArrayOffsetInBytes - uMaximumInstStaticOffsetInBytes);
			ASSERT((uBaseMin % LONG_SIZE) == uOffsetAlignment);
		}

		/*
			Convert the offset range to word units.
		*/
		uOffsetAlignment /= WORD_SIZE;
		uBaseMax /= WORD_SIZE;
		uBaseMin /= WORD_SIZE;

		/*
			Create instructions to load each word from the source.
		*/
		for (uHalf = 0; uHalf < 2; uHalf++)
		{
			IMG_UINT32		uMask;
			PINST			psIDXSCRInst;
			IMG_UINT32		uIDXSCRDestTempNum;
			PINDEX_COPY		psNewIndexCopy;

			uMask = (uHalf == 0) ? USC_XY_CHAN_MASK : USC_ZW_CHAN_MASK;
			if ((uUsedChans & uMask) == 0)
			{
				/* Skip if this word is unreferenced. */
				continue;
			}

			psIDXSCRInst = AllocateInst(psState, psOldRef->psInst);
			SetOpcode(psState, psIDXSCRInst, IIDXSCR);
			psIDXSCRInst->u.psIdxsc->uChannelCount = 2;

			if (uHalf == 1)
			{
				uIDXSCRDestTempNum = uUpperHalfTemp = GetNextRegister(psState);
			}
			else
			{
				if (bLoadBothHalves)
				{
					uIDXSCRDestTempNum = uLowerHalfTemp = GetNextRegister(psState);
				}
				else
				{
					uIDXSCRDestTempNum = uReplacementTemp;
				}
			}
			SetDestTempArg(psState, psIDXSCRInst, 0 /* uDestIdx */, uIDXSCRDestTempNum, eOldArgFmt);

			/*
				Copy the indexed source into the IDXSCR instruction.
			*/
			CopySrc(psState, 
					psIDXSCRInst, 
					IDXSCR_INDEXEDVALUE_ARGINDEX,
					psOldRef->psInst,
					psOldRef->uBaseArgIdx + uRefOffset);

			psIDXSCRInst->u.psIdxsc->uChannelOffset = uHalf * WORD_SIZE;

			InsertInstBefore(psState, psOldRef->psInst->psBlock, psIDXSCRInst, psInst);

			/*
				Find or create an interval suitable for indexing with IDXSCR.
			*/
			psNewIndexCopy = 
				FindMatchingIndexCopy(psState, 
									  psIndexCopyList, 
									  uIndexArrayOffset,
									  psInst->psBlock,
									  uBaseMin + uHalf, 
									  uBaseMax + uHalf,
									  (uOffsetAlignment + uHalf) % 4,
									  psInstSource->uIndexStrideInBytes / WORD_SIZE,
									  IMG_FALSE /* bSubcomponentIndex */,
									  IMG_FALSE /* bSpansTwoRegisters */);

			/*
				Update the temporary register holding the dynamic index value.
			*/
			SetSrcIndex(psState, 
						psIDXSCRInst, 
						IDXSCR_INDEXEDVALUE_ARGINDEX, 
						USEASM_REGTYPE_TEMP, 
						psNewIndexCopy->uTempNum, 
						0 /* uNewIndexArrayOffset */, 
						WORD_SIZE /* uNewIndexRelativeStrideInBytes */);

			/*
				Make the instructions to set up the index are inserted before the IDXSCR
				instruction.
			*/
			UpdateCopyInsertPoint(psNewIndexCopy, psIDXSCRInst);
		}

		/*
			If we are loading two words then combine them into a single dword.
		*/
		if (bLoadBothHalves)
		{
			PINST	psCombInst;

			psCombInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psCombInst, IVPCKU16U16);

			SetDestTempArg(psState, psCombInst, 0 /* uDestIdx */, uReplacementTemp, eOldArgFmt);
			psCombInst->auDestMask[0] = USC_ZW_CHAN_MASK;

			SetPartialDest(psState, psCombInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uLowerHalfTemp, eOldArgFmt);

			SetSourceTempArg(psState, psCombInst, 0 /* uSrcIdx */, uUpperHalfTemp, eOldArgFmt);

			psCombInst->u.psVec->aeSrcFmt[0] = UF_REGFORMAT_U16;
			psCombInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, X, X, X);
			InsertInstBefore(psState, psInst->psBlock, psCombInst, psInst);
		}
		else if ((uUsedChans & USC_ZW_CHAN_MASK) != 0)
		{
			PINST	psSHLInst;

			/*
				IDXSCR loads the indexed data into the lower half of the register so
				shift it to the upper half (where the instruction expects to
				access it).
			*/
			psSHLInst = AllocateInst(psState, psInst);
			SetOpcode(psState, psSHLInst, ISHL);
			SetDestTempArg(psState, psSHLInst, 0 /* uDestIdx */, uReplacementTemp, eOldArgFmt);
			SetSourceTempArg(psState, psSHLInst, BITWISE_SHIFTEND_SOURCE, uUpperHalfTemp, eOldArgFmt);
			psSHLInst->asArg[BITWISE_SHIFT_SOURCE].uType = USEASM_REGTYPE_IMMEDIATE;
			psSHLInst->asArg[BITWISE_SHIFT_SOURCE].uNumber = 16;
			InsertInstBefore(psState, psInst->psBlock, psSHLInst, psInst);
		}

		/*
			Replace the instruction's source by the IDXSCR's result.
		*/
		SetSourceTempArg(psState, psOldRef->psInst, psOldRef->uBaseArgIdx + uRefOffset, uReplacementTemp, eOldArgFmt);
		return;
	}
	#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */

	/*
		Otherwise use shift instructions to extract the range of bits required.
	*/
	FixInvalidIndexStrideSource_NonIDXSC(psState,
										 psIndexCopyList,
										 psOldRef,
										 uRefOffset,
										 uArrayOffsetInDwords);
}

static
IMG_UINT32 GetIndexUnitsAtRefLog2(PINTERMEDIATE_STATE psState, PINDEX_REF psRef)
/*****************************************************************************
 FUNCTION	: GetIndexUnitsAtRefLog2

 PURPOSE	: Gets the units of dynamic indices used by an instruction at a
			  reference to a dynamic register index.

 PARAMETERS	: psState		- Compiler state.
			  psRef			- Dynamic register index.

 RETURNS	: The units in bytes log 2.
*****************************************************************************/
{
	IMG_BOOL	bDest;

	bDest = IMG_FALSE;
	if (psRef->eType == REF_TYPE_DEST)
	{
		bDest = IMG_TRUE;
	}
	return GetIndexUnitsLog2(psState, psRef->psInst, bDest, psRef->uBaseArgIdx);
}	

static
PINDEX_REF GetNextRef(PINDEX psIndex)
/*****************************************************************************
 FUNCTION	: GetNextRef
    
 PURPOSE	: Get the next reference to an index in a future instruction.

 PARAMETERS	: psIndex	- Index to get the next reference to.
			  
 RETURNS	: A pointer to the next reference or NULL if the index isn't
			  referenced in any future instructions.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psNextRefListEntry;
	PINDEX_REF	psCurrentRef = psIndex->psCurrentRef;

	for (;;)
	{
		PINDEX_REF	psNextRef;

		psNextRefListEntry = psCurrentRef->sListEntry.psNext;

		if (psNextRefListEntry == NULL)
		{
			return NULL;
		}
		psNextRef = IMG_CONTAINING_RECORD(psNextRefListEntry, PINDEX_REF, sListEntry);
		if (psNextRef->psInst != psCurrentRef->psInst)
		{
			return psNextRef;
		}
		psCurrentRef = psNextRef;
	}
}

static 
IMG_INT32 SortByNextRefCB(PUSC_LIST_ENTRY psListEntry1, PUSC_LIST_ENTRY psListEntry2)
/*****************************************************************************
 FUNCTION	: SortByNextRefCB
    
 PURPOSE	: Comparison function for indices.

 PARAMETERS	: psListEntry1		- Indices to compare.
			  psListEntry2
			  
 RETURNS	: <0	if INDEX1 < INDEX2
			  =0	if INDEX1 == INDEX2
			  >0	if INDEX1 > INDEX2
*****************************************************************************/
{
	PINDEX		psIndex1;
	PINDEX		psIndex2;
	PINDEX_REF	psNextRef1;
	PINDEX_REF	psNextRef2;

	psIndex1 = IMG_CONTAINING_RECORD(psListEntry1, PINDEX, sUsedHereListEntry);
	psNextRef1 = GetNextRef(psIndex1);

	psIndex2 = IMG_CONTAINING_RECORD(psListEntry2, PINDEX, sUsedHereListEntry);
	psNextRef2 = GetNextRef(psIndex2);

	/*
		Treat an interval with no next reference as larger than an interval which
		is used in future.
	*/
	if (psNextRef1 == NULL && psNextRef2 == NULL)
	{
		return 0;
	}
	else if (psNextRef1 == NULL)
	{
		return -1;
	}
	else if (psNextRef2 == NULL)
	{
		return 1;
	}
	else
	{
		return psNextRef2->psInst->uBlockIndex - psNextRef1->psInst->uBlockIndex;
	}
}

static
PINST MoveIndexReferenceToMatchingStrideInst(PINTERMEDIATE_STATE	psState,
											 PINDEX_REF				psRef,
											 PINST					psInsertBeforeInst,
											 IMG_UINT32				uRefOffset,
											 IMG_UINT32				uIndexStride)
/*****************************************************************************
 FUNCTION	: MoveIndexReferenceToMatchingStrideInst
    
 PURPOSE	: Moves a reference to an index in the current instruction
			  to separate MOV-type instruction with the same index
			  stride.

 PARAMETERS	: psState			- Compiler state.
			  psRef				- Index reference to move.
			  uRefOffset		- Offset in the group of indexed arguments
								to move.
			  uIndexStride		- Scale for indices applied by the current
								instruction.
			  
 RETURNS	: A pointer to the new instruction.
*****************************************************************************/
{
	switch (uIndexStride)
	{
		case LONG_SIZE:
		{
			/*
				Move the index reference to a separate MOV instruction.
			*/
			return MoveIndexReference(psState, 
									  psInsertBeforeInst, 
									  IMOV, 
									  0 /* uSrcNum */, 
									  psRef, 
									  uRefOffset, 
									  NULL /* psNewRef */);
		}
		case WORD_SIZE:
		{
			PINST	psFMovInst;

			ASSERT(psRef->uArgCount == 1);

			/*
				The only instructions which support a WORD units index are scalar floating point
				ones which support selecting F16 or F32 format for their sources using the top-bit
				of the encoded register when the source is F16 format. So generate a version of the
				same type of instruction which does SRC*1+0. These instructions always generate an
				F32 result so change the format of the original instruction source to F32.
			*/
			ASSERT(psRef->eType == REF_TYPE_SRC);
			ASSERT(InstUsesF16FmtControl(psRef->psInst));
			ASSERT(psRef->psBaseArg->eFmt == UF_REGFORMAT_F16);
	
			psFMovInst = MoveIndexReference(psState, 
											psInsertBeforeInst,
										    IFMOV, 
											0 /* uSrcNum */, 
											psRef, 
											uRefOffset,
											NULL /* psNewRef */);
			if (psFMovInst == NULL)
			{
				return NULL;
			}
		
			psFMovInst->asDest[0].eFmt = UF_REGFORMAT_F32;
			psRef->psBaseArg->eFmt = UF_REGFORMAT_F32;
			return psFMovInst;
		}
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		case QWORD_SIZE:
		{
			PINST			psVMOVInst;
			UF_REGFORMAT	eFmt = psRef->psBaseArg->eFmt;
			IMG_UINT32		uChan;

			psVMOVInst = MoveIndexReference(psState, 
											psInsertBeforeInst,
											IVMOV, 
											1 /* uSrcNum */, 
											psRef, 
											uRefOffset,
											NULL /* psNewRef */);
			if (psVMOVInst == NULL)
			{
				return NULL;
			}

			psVMOVInst->u.psVec->auSwizzle[0] = USEASM_SWIZZLE(X, Y, Z, W);
			psVMOVInst->u.psVec->aeSrcFmt[0] = eFmt;

			psVMOVInst->asArg[0].uType = USC_REGTYPE_UNUSEDSOURCE;
			for (uChan = 1; uChan < VECTOR_LENGTH; uChan++)
			{
				psVMOVInst->asArg[1 + uChan].uType = USC_REGTYPE_UNUSEDSOURCE;
			}
			return psVMOVInst;
		}
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		default: imgabort();
	}
}

static
IMG_VOID MoveIndexReferences(PINTERMEDIATE_STATE	psState,
							 PINST					psInsertBeforeInst,
							 PINDEX_REF				psRef,
							 PINST*					ppsLastUseInstId)
/*****************************************************************************
 FUNCTION	: MoveIndexReferences
    
 PURPOSE	: Moves a reference to an index to a separate MOV instruction.

 PARAMETERS	: psState				- Compiler state.
			  psInsertBeforeInst	- Location to insert the instructions.
			  psRef					- Reference to move.
			  ppsLastUseInstId		- Returns the last instruction
									using the index.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uIndexStrideLog2;
	IMG_UINT32	uIndexStride;
	IMG_UINT32	uArgIdx;

	/*
		Generate a MOV instruction using the same stride for the index as the modified
		instruction.
	*/
	uIndexStrideLog2 = GetIndexUnitsAtRefLog2(psState, psRef);
	uIndexStride = 1 << uIndexStrideLog2;

	for (uArgIdx = 0; uArgIdx < psRef->uArgCount; uArgIdx++)
	{
		PINST	psMOVInst;

		/*
			Create a move instruction for this indexed source/destination.
		*/
		psMOVInst = 
			MoveIndexReferenceToMatchingStrideInst(psState,
												   psRef,
												   psInsertBeforeInst,
												   uArgIdx,
												   uIndexStride);

		if (psMOVInst != NULL)
		{
			(*ppsLastUseInstId) = psMOVInst;
		}
				
		/*
			If we replaced a group of registers requiring consecutive hardware register numbers then
			mark the replacement temporaries as requiring consecutive numbers too.
		*/
		if (psRef->uArgCount > 0 || psRef->eArgAlign != HWREG_ALIGNMENT_NONE)
		{
			MakeGroup(psState, 
					  psRef->psBaseArg,
					  psRef->uArgCount,
					  psRef->eArgAlign);
		}
	}
}

static
IMG_VOID FixIndexedDeltaArguments(PINTERMEDIATE_STATE	psState,
								  PUSEDEF_CHAIN			psUseDefChain)
/*****************************************************************************
 FUNCTION	: FixIndexedDeltaArguments
    
 PURPOSE	: Move indexed sources to DELTA instructions to separate move
			  instructions in the predecessor blocks.

 PARAMETERS	: psState			- Compiler state.
			  psUseDefChain		- Indexed sources to fix.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry, psNextListEntry;

	for (psListEntry = psUseDefChain->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
	{
		PUSEDEF	psUse;
		PINST	psDeltaInst;

		psNextListEntry = psListEntry->psNext;
		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (!(psUse->eType == USE_TYPE_SRCIDX || psUse->eType == USE_TYPE_DESTIDX))
		{
			continue;
		}
			
		psDeltaInst = psUse->u.psInst;
		if (psDeltaInst->eOpcode != IDELTA)
		{
			continue;
		}

		if (psUse->eType == USE_TYPE_DESTIDX)
		{
			PINST			psMOVInst;
			IMG_UINT32		uTempDest = GetNextRegister(psState);
			IMG_UINT32		uDestIdx = psUse->uLocation;
			UF_REGFORMAT	eDestFmt;

			ASSERT(uDestIdx < psDeltaInst->uDestCount);
			eDestFmt = psDeltaInst->asDest[uDestIdx].eFmt;

			psMOVInst = AllocateInst(psState, psDeltaInst);
			SetOpcode(psState, psMOVInst, IMOV);
			MoveDest(psState, psMOVInst, 0 /* uDestIdx */, psDeltaInst, uDestIdx);
			SetSrc(psState, psMOVInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTempDest, eDestFmt);
			InsertInstAfter(psState, psMOVInst->psBlock, psMOVInst, psDeltaInst);

			SetDest(psState, psDeltaInst, uDestIdx, USEASM_REGTYPE_TEMP, uTempDest, eDestFmt);
		}
		else
		{
			PINST			psMOVInst;
			IMG_UINT32		uTempSrc = GetNextRegister(psState);
			IMG_UINT32		uSrcIdx = psUse->uLocation;
			UF_REGFORMAT	eSrcFmt;
			PCODEBLOCK		psInsertBlock;

			ASSERT(uSrcIdx < psDeltaInst->uArgumentCount);
			eSrcFmt = psDeltaInst->asArg[uSrcIdx].eFmt;

			ASSERT(uSrcIdx < psDeltaInst->psBlock->uNumPreds);
			psInsertBlock = psDeltaInst->psBlock->asPreds[uSrcIdx].psDest;

			psMOVInst = AllocateInst(psState, psDeltaInst);
			SetOpcode(psState, psMOVInst, IMOV);
			SetDest(psState, psMOVInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, uTempSrc, eSrcFmt);
			MoveSrc(psState, psMOVInst, 0 /* uSrcIdx */, psDeltaInst, uSrcIdx);
			AppendInst(psState, psInsertBlock, psMOVInst);

			SetSrc(psState, psDeltaInst, uSrcIdx, USEASM_REGTYPE_TEMP, uTempSrc, eSrcFmt);
		}
	}
}

static
IMG_VOID CheckAndFixIndex(PINTERMEDIATE_STATE	psState,
						  PUSC_LIST				psIndexCopyList,
						  PINDEX_REF			psRef)
/*****************************************************************************
 FUNCTION	: CheckAndFixIndex
    
 PURPOSE	: Check an indexed argument is compatible with the hardware
			  limitations on indexing.

 PARAMETERS	: psState			- Compiler state.
			  psIndexCopyList	- List of existing copies of the temporary
								used for indexing with different offsets/strides.
			  psRef				- Indexed argument.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArrayOffsetInDwords;
	IMG_UINT32	uInstStrideInBytesLog2;
	IMG_UINT32	uInstStrideInBytes;
	IMG_UINT32	uIndexStrideInBytes;

	/*
		Get the static offset to the index applied in the instruction.
	*/
	uArrayOffsetInDwords = GetArrayOffset(psState, psRef->psBaseArg);

	/*
		Get the value this instruction will scale the dynamic index value by.
	*/
	uInstStrideInBytesLog2 = GetIndexUnitsAtRefLog2(psState, psRef);
	uInstStrideInBytes = 1 << uInstStrideInBytesLog2;

	uIndexStrideInBytes = psRef->psBaseArg->uIndexStrideInBytes;

	/*
		Check for an index stride not supported by this instruction.
	*/
	if (
			uInstStrideInBytes > uIndexStrideInBytes || 
			(uIndexStrideInBytes % uInstStrideInBytes) != 0
	   )
	{
		IMG_UINT32	uArgIdx;

		ASSERT(psRef->eType == REF_TYPE_SRC);

		for (uArgIdx = 0; uArgIdx < psRef->uArgCount; uArgIdx++)
		{
			FixInvalidIndexStrideSource(psState,
										psIndexCopyList,
										psRef,
										uArgIdx);
		}

		/*
			If we replaced a group of registers requiring consecutive hardware register numbers then
			mark the replacement temporaries as requiring consecutive numbers too.
		*/
		if (psRef->uArgCount > 1 || psRef->eArgAlign != HWREG_ALIGNMENT_NONE)
		{
			MakeGroup(psState,
					  psRef->psBaseArg,
					  psRef->uArgCount,
					  psRef->eArgAlign);
		}
	}
	else
	{
		/*
			Set up an index register with the correct parameters to access the indexed value
			in this instruction source.
		*/
		SetupAlignedIndex(psState, 
						  psIndexCopyList, 
						  psRef,
						  uArrayOffsetInDwords,
						  uInstStrideInBytes);
	}
}

static
IMG_BOOL GetIndexNextRef(PINTERMEDIATE_STATE	psState,
						 PUSC_LIST_ENTRY*		ppsListEntry,
						 PINDEX_REF				psRef)
/*****************************************************************************
 FUNCTION	: GetIndexNextRef
    
 PURPOSE	: 

 PARAMETERS	: psState			- Compiler state.
			  ppsListEntry		- The next entry in the USE-DEF chain.
			  psRef				- Returns the details of the indexed argument.
			  
 RETURNS	: TRUE if the next use was as an index.
*****************************************************************************/
{
	REGISTER_GROUP_DESC		sGroup;
	PINST					psInst;
	PUSC_LIST_ENTRY			psListEntry;
	IMG_UINT32				uArg;
	PUSEDEF					psIndexUse;
	IMG_BOOL				bDest;

	psListEntry = *ppsListEntry;
	psIndexUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

	/*	
		Skip uses other than as an index.
	*/
	if (!(psIndexUse->eType == USE_TYPE_SRCIDX || psIndexUse->eType == USE_TYPE_DESTIDX))
	{
		(*ppsListEntry) = psListEntry->psNext;
		return IMG_FALSE;
	}

	psInst = psIndexUse->u.psInst;

	psRef->psInst = psInst;
	psRef->uBaseArgIdx = psIndexUse->uLocation;

	if (psIndexUse->eType == USE_TYPE_SRCIDX)
	{
		bDest = IMG_FALSE;
		psRef->eType = REF_TYPE_SRC;
		psRef->psBaseArg = psInst->asArg + psRef->uBaseArgIdx;
	}
	else
	{
		ASSERT(psIndexUse->eType == USE_TYPE_DESTIDX);
		
		bDest = IMG_FALSE;
		psRef->eType = REF_TYPE_DEST;
		psRef->psBaseArg = psInst->asDest + psRef->uBaseArgIdx;
	}

	/*
		Check if this argument is part of a group of arguments to the instruction which require consecutive
		hardware register numbers.
	*/
	if (GetRegisterGroupByArg(psState, psInst, bDest, psRef->uBaseArgIdx, &sGroup))
	{
		ASSERT(psRef->uBaseArgIdx == sGroup.uStart);
		psRef->uArgCount = sGroup.uCount;
		psRef->eArgAlign = sGroup.eAlign;
	}
	else
	{
		psRef->uArgCount = 1;
		psRef->eArgAlign = HWREG_ALIGNMENT_NONE;
	}

	/*
		Skip all the arguments in the same group.
	*/
	for (uArg = 0; uArg < psRef->uArgCount; uArg++, psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psNextUse;

		psNextUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
		ASSERT(psNextUse->eType == psIndexUse->eType);
		ASSERT(psNextUse->u.psInst == psInst);
		ASSERT(psNextUse->uLocation == (psRef->uBaseArgIdx + uArg));
	}

	(*ppsListEntry) = psListEntry;

	return IMG_TRUE;
}

static
IMG_VOID FixPartiallyWrittenArrays(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psIndexUseDef)
/*****************************************************************************
 FUNCTION	: FixPartiallyWrittenArrays
    
 PURPOSE	: Fix cases where indexed values are used in partially updated destinations
			  where the old and new values of the destination don't refer to the same value.

 PARAMETERS	: psState			- Compiler state.
			  psIndexUseDef		- Index value to fix uses of.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;

	for (psListEntry = psIndexUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psIndexUse;

		psIndexUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psIndexUse->eType == USE_TYPE_OLDDESTIDX || psIndexUse->eType == USE_TYPE_DESTIDX)
		{
			PARG			psOldDest;
			PARG			psDest;
			PINST			psInst;
			UF_REGFORMAT	eDestFormat;

			psInst = psIndexUse->u.psInst;
			ASSERT(psIndexUse->uLocation < psInst->uDestCount);
			psOldDest = psInst->apsOldDest[psIndexUse->uLocation];
			psDest = &psInst->asDest[psIndexUse->uLocation];

			eDestFormat = psDest->eFmt;
		
			if (psOldDest != NULL && !EqualArgs(psOldDest, psDest))
			{
				IMG_UINT32	uTempDest;

				uTempDest = GetNextRegister(psState);
				if (psOldDest->uType != USEASM_REGTYPE_TEMP)
				{
					PINST	psMOVInst;

					/*
						Convert
							ALU		DEST[=OLDDEST[IDX]], ....
						->
							MOV		TEMP, OLDDEST[IDX]
							ALU		DEST[=TEMP], ....
					*/
					psMOVInst = AllocateInst(psState, psInst);
					SetOpcode(psState, psMOVInst, IMOV);
					MovePartialDestToSrc(psState, psMOVInst, 0 /* uMoveToSrcIdx */, psInst, psIndexUse->uLocation);
					SetDest(psState, psMOVInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTempDest, eDestFormat);
					InsertInstAfter(psState, psInst->psBlock, psMOVInst, psInst);

					SetPartialDest(psState, 
								   psInst, 
								   psIndexUse->uLocation, 
								   USEASM_REGTYPE_TEMP, 
								   uTempDest, 
								   eDestFormat);
				}
				if (psDest->uType != USEASM_REGTYPE_TEMP)
				{
					PINST	psMOVInst;

					/*
						Convert
							ALU		DEST[IDX][=OLDDEST], ...
						->
							ALU		TEMP[=OLDDEST], ...
							MOV		DEST[IDX], TEMP
					*/
					psMOVInst = AllocateInst(psState, psInst);
					SetOpcode(psState, psMOVInst, IMOV);
					MoveDest(psState, psMOVInst, 0 /* uMoveToDestIdx */, psInst, psIndexUse->uLocation);
					SetSrc(psState, psMOVInst, 0 /* uSrcIdx */, USEASM_REGTYPE_TEMP, uTempDest, eDestFormat);
					InsertInstAfter(psState, psInst->psBlock, psMOVInst, psInst);

					SetSrc(psState,
						   psInst,
						   psIndexUse->uLocation,
						   USEASM_REGTYPE_TEMP,
						   uTempDest,
						   eDestFormat);
				}
			}
		}
	}
} 


static
IMG_VOID RebaseIndexReference(PINTERMEDIATE_STATE	psState, 
							  PINDEX_COPY			psIndexCopy, 
							  PINST					psInst, 
							  IMG_BOOL				bDest, 
							  IMG_UINT32			uIndexedArgIdx,
							  PARG					psIndexedArg)
/*****************************************************************************
 FUNCTION	: RebaseIndexReference
    
 PURPOSE	: Where an extra offset has been applied when writing an index subtract
			  the same offset from the static offset at a use of the index.

 PARAMETERS	: psState			- Compiler state.
			  psIndexCopy		- Details of the static offset applied to the
								index.
			  psInst			- Instruction whose argument is to be modified.
			  bDest				- TRUE if the argument to be modified is a destination.
								  FALSE if the argument to be modified is a source.
		      uIndexedArgIdx	- Index of the argument to modify in the instruction's
								array of sources or destinations.
			  psIndexedArg		- Argument to modify.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_PUINT32	puArrayOffset;
	IMG_INT32	iIndexOffsetInBytes;
	IMG_INT32	iArrayOffsetInBytes;
	IMG_INT32	iFinalOffsetInBytes;
	IMG_UINT32	uInstStrideInBytesLog2;
	IMG_UINT32	uInstStrideInBytes;

	/*
		Get the value this instruction will scale the dynamic index value by.
	*/
	uInstStrideInBytesLog2 = GetIndexUnitsLog2(psState,psInst, bDest, uIndexedArgIdx);
	uInstStrideInBytes = 1 << uInstStrideInBytesLog2;

	/*
		Subtract any offset applied when writing the index register from the static offset applied
		directly in the instruction.
	*/
	if (psIndexedArg->uType == USC_REGTYPE_REGARRAY)
	{
		puArrayOffset = &psIndexedArg->uArrayOffset;
	}
	else	
	{
		puArrayOffset = &psIndexedArg->uNumber;
	}

	iIndexOffsetInBytes = psIndexCopy->uBaseMin * uInstStrideInBytes;

	iArrayOffsetInBytes = (*puArrayOffset) * LONG_SIZE;
	iFinalOffsetInBytes = GetArrayOffset(psState, psIndexedArg) * LONG_SIZE;
	if (uInstStrideInBytes < LONG_SIZE)
	{
		IMG_UINT32	uComponent;

		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psInst->eOpcode == IIDXSCR || psInst->eOpcode == IIDXSCW)
		{
			uComponent = psInst->u.psIdxsc->uChannelOffset;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			uComponent = GetComponentSelect(psState, psInst, uIndexedArgIdx);
		}

		iArrayOffsetInBytes += uComponent;
		iFinalOffsetInBytes += uComponent;
	}

	ASSERT(iFinalOffsetInBytes >= iIndexOffsetInBytes);
	iArrayOffsetInBytes -= iIndexOffsetInBytes;

	(*puArrayOffset) = (IMG_UINT32)(iArrayOffsetInBytes / (IMG_INT32)LONG_SIZE);
	if (uInstStrideInBytes < LONG_SIZE)
	{
		#if defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554)
		if (psInst->eOpcode == IIDXSCR || psInst->eOpcode == IIDXSCW)
		{
			ASSERT((iArrayOffsetInBytes % LONG_SIZE) == 0);
			psInst->u.psIdxsc->uChannelOffset = 0;
		}
		else
		#endif /* defined(SUPPORT_SGX543) || defined(SUPPORT_SGX544) || defined(SUPPORT_SGX554) */
		{
			SetComponentSelect(psState, psInst, uIndexedArgIdx, iArrayOffsetInBytes % LONG_SIZE);
		}
	}
	else
	{
		ASSERT((iArrayOffsetInBytes % LONG_SIZE) == 0);
	}
}

static
IMG_VOID RebaseIndexReferences(PINTERMEDIATE_STATE psState, PINDEX_COPY psIndexCopy)
/*****************************************************************************
 FUNCTION	: RebaseIndexReferences
    
 PURPOSE	: Where an extra offset has been applied when writing an index subtract
			  the same offset from the static offset at each use of the index.

 PARAMETERS	: psState			- Compiler state.
			  psIndexCopy		- Details of the static offset applied to the
								index.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	PUSEDEF_CHAIN	psCopyUseDef;
	
	psCopyUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, psIndexCopy->uTempNum);
	for (psListEntry = psCopyUseDef->sList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF	psUse;

		psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);

		if (psUse->eType == USE_TYPE_SRCIDX ||
			psUse->eType == USE_TYPE_DESTIDX ||
			psUse->eType == USE_TYPE_OLDDESTIDX)
		{
			PARG		psIndexedArg;
			IMG_BOOL	bDest;

			psIndexedArg = UseDefGetInstUseLocation(psState, psUse, NULL /* ppsIndexType */);
			if (psUse->eType == USE_TYPE_DESTIDX || psUse->eType == USE_TYPE_OLDDESTIDX)
			{
				bDest = IMG_TRUE;
			}
			else
			{
				bDest = IMG_FALSE;
			}

			RebaseIndexReference(psState, psIndexCopy, psUse->u.psInst, bDest, psUse->uLocation, psIndexedArg);
		}
	}
}

static
IMG_VOID SetupIndexCopy(PINTERMEDIATE_STATE psState, PINDEX_COPY psIndexCopy, PUSEDEF_CHAIN psIndex)
/*****************************************************************************
 FUNCTION	: SetupIndexCopy
    
 PURPOSE	: Insert instructions to copy a value used for indexing.

 PARAMETERS	: psState			- Compiler state.
			  psIndexCopy		- Details of the offset/scale to apply when
								copying the index.
			  psIndex			- Index to copy.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINST	psInsertBeforeInst = psIndexCopy->psInsertBeforeInst;

	if (psIndexCopy->bSubcomponentIndex)
	{
		SetupSubcomponentSelectIndex(psState, psInsertBeforeInst, psIndex, psIndexCopy);
	}
	else if (psIndexCopy->uStride == 1 && psIndexCopy->uBaseMin == 0)
	{
		PUSEDEF_CHAIN		psUseDef;
		PUSC_LIST_ENTRY		psListEntry;
		PUSC_LIST_ENTRY		psNextListEntry;

		psUseDef = UseDefGet(psState, USEASM_REGTYPE_TEMP, psIndexCopy->uTempNum);
		for (psListEntry = psUseDef->sList.psHead; psListEntry != NULL; psListEntry = psNextListEntry)
		{
			PUSEDEF	psUse;

			psNextListEntry = psListEntry->psNext;
			psUse = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF, sListEntry);
			if (psUse == psUseDef->psDef)
			{
				continue;
			}
			switch (psUse->eType)
			{
				case USE_TYPE_SRCIDX: 
				{
					SetSrcIndex(psState,
								psUse->u.psInst,
								psUse->uLocation,
								psIndex->uType,
								psIndex->uNumber,
								psIndexCopy->uArrayOffset,
								USC_UNDEF /* uNewIndexRelativeStrideInBytes */);
					break;
				}
				case USE_TYPE_DESTIDX: 
				{
					SetDestIndex(psState,
								 psUse->u.psInst,
								 psUse->uLocation,
								 psIndex->uType,
								 psIndex->uNumber,
								 psIndexCopy->uArrayOffset,
								 USC_UNDEF /* uNewIndexRelativeStrideInBytes */);
					break;
				}
				case USE_TYPE_OLDDESTIDX: 
				{
					SetPartialDestIndex(psState,
										psUse->u.psInst,
										psUse->uLocation,
										psIndex->uType,
										psIndex->uNumber,
										psIndexCopy->uArrayOffset,
									    USC_UNDEF /* uNewIndexRelativeStrideInBytes */);
					break;
				}
				default: imgabort();
			}
		}
	}
	else
	{
		PINST	psIndexWriteInst;

		psIndexWriteInst = AllocateInst(psState, psInsertBeforeInst);

		InsertInstBefore(psState, 
						 psInsertBeforeInst->psBlock, 
						 psIndexWriteInst, 
						 psInsertBeforeInst);

		SetOpcode(psState, psIndexWriteInst, IIMA16);

		SetIndex(psState, psIndexWriteInst, psIndex, psIndexCopy->uArrayOffset, 0 /* uArgIdx */);

		LoadImmediate(psState, psIndexWriteInst, 1 /* uArgIdx */, psIndexCopy->uStride);

		LoadImmediate(psState, psIndexWriteInst, 2 /* uArgIdx */, psIndexCopy->uBaseMin);
	
		SetDest(psState, psIndexWriteInst, 0 /* uDestIdx */, USEASM_REGTYPE_TEMP, psIndexCopy->uTempNum, UF_REGFORMAT_F32);

		psIndexWriteInst->auLiveChansInDest[0] = USC_XY_CHAN_MASK;
	}
}

static
IMG_VOID FixInvalidIndex(PINTERMEDIATE_STATE psState, PUSEDEF_CHAIN psIndexUseDef)
/*****************************************************************************
 FUNCTION	: FixInvalidIndex
    
 PURPOSE	: Fix indexed arguments using a specific index where the static offset/stride 
			  is incompatible with the hardware limitation on addressing.

 PARAMETERS	: psState			- Compiler state.
			  psIndexUseDef		- Index to fix uses of.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry;
	USC_LIST		sIndexCopyList;

	InitializeList(&sIndexCopyList);

	/*
		Fix cases where instruction doing partial updates have either indexed
		destinations but non-indexed sources for the unwritten channel; or non-indexed
		destinations but indexed sources for the unwritten channels. 
	*/
	FixPartiallyWrittenArrays(psState, psIndexUseDef);

	/*
		Fix cases where indexed arguments are used in the sources or destinations
		of DELTA instructions.
	*/
	FixIndexedDeltaArguments(psState, psIndexUseDef);

	/*
		Check all uses of this index for either dynamic strides which don't match the
		stride applied by the instruction or uses of static offset out of the range
		supported by the instruction.
	*/
	psListEntry = psIndexUseDef->sList.psHead;
	while (psListEntry != NULL)
	{
		INDEX_REF	sRef;

		if (!GetIndexNextRef(psState, &psListEntry, &sRef))
		{
			continue;
		}
		CheckAndFixIndex(psState, &sIndexCopyList, &sRef);
	}

	/*
		If we needed to scale or apply a static offset to the index value at any of the uses
		to fit with hardware limitations then add instructions to do the modifications now.
	*/
	while ((psListEntry = RemoveListHead(&sIndexCopyList)) != NULL)
	{
		PINDEX_COPY	psIndexCopy;

		psIndexCopy = IMG_CONTAINING_RECORD(psListEntry, PINDEX_COPY, sListEntry);

		SetupIndexCopy(psState, psIndexCopy, psIndexUseDef);

		/*
			If a static offset was applying when copying the index value then reduce the
			offset applied at the uses of the index correspondingly.
		*/
		RebaseIndexReferences(psState, psIndexCopy);

		UscFree(psState, psIndexCopy);
	}
}

static
IMG_VOID FixInvalidIndices(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: FixInvalidIndices
    
 PURPOSE	: Fix indexed arguments where the static offset/stride is incompatible
			  with the hardware limitation on addressing.

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY	psListEntry, psPrevListEntry;

	/*
		Process starting from the end of the list so we skip any new copies of existing indices added.
	*/
	for (psListEntry = psState->sIndexUseTempList.psTail; psListEntry != NULL; psListEntry = psPrevListEntry)
	{
		PUSEDEF_CHAIN	psIndexUseDef;

		psPrevListEntry = psListEntry->psPrev;
		psIndexUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF_CHAIN, sIndexUseTempListEntry);
		FixInvalidIndex(psState, psIndexUseDef);
	}
}

static
IMG_VOID FinaliseIndexRef(PINTERMEDIATE_STATE	psState,
						  PINDEX				psIndex,
						  PINDEX_REF			psRef)
/*****************************************************************************
 FUNCTION	: FinaliseIndexRef
    
 PURPOSE	: Assign hardware index registers for a single indexed argument.

 PARAMETERS	: psState			- Compiler state.
			  psIndex			- Temporary register used for indexing.
			  psRef				- Details of the indexed argument.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	IMG_UINT32	uArg;
	PINST		psInst = psRef->psInst;

	/* 
		Set the hardware index register used for this argument.
	*/
	for (uArg = 0; uArg < psRef->uArgCount; uArg++)
	{
		PARG		psIndexedArg = psRef->psBaseArg + uArg;

		ASSERT(psIndexedArg->uIndexType == psIndex->psUseDef->uType);
		ASSERT(psIndexedArg->uIndexNumber == psIndex->psUseDef->uNumber);
		ASSERT(psIndexedArg->uIndexArrayOffset == psIndex->uArrayOffset);
		ASSERT(psIndex->uHwRegNum < EURASIA_USE_INDEX_BANK_SIZE);

		switch (psRef->eType)
		{
			case REF_TYPE_DEST: 
			{
				IMG_UINT32	uDestIdx = psRef->uBaseArgIdx + uArg;
			
				if (psInst->apsOldDest[uDestIdx] != NULL)
				{
					ASSERT(EqualArgs(psInst->apsOldDest[uDestIdx], &psInst->asDest[uDestIdx]));
					SetPartialDestIndex(psState,
										psInst,
										uDestIdx,
										USEASM_REGTYPE_INDEX,
										psIndex->uHwRegNum,
										0 /* uNewIndexArrayOffset */,
										USC_UNDEF /* uNewIndeRelativeStrideInBytes */);
				}
				SetDestIndex(psState,
							 psInst,
							 uDestIdx,
							 USEASM_REGTYPE_INDEX,
							 psIndex->uHwRegNum,
							 0 /* uNewIndexArrayOffset */,
							 USC_UNDEF /* uNewIndeRelativeStrideInBytes */);
				break;
			}
			case REF_TYPE_SRC:
			{
				SetSrcIndex(psState,
							psInst,
							psRef->uBaseArgIdx + uArg,
							USEASM_REGTYPE_INDEX,
							psIndex->uHwRegNum,
							0 /* uNewIndexArrayOffset */,
							USC_UNDEF /* uNewIndeRelativeStrideInBytes */);
				break;
			}
			default: imgabort();
		}
	}
}

static
IMG_VOID AssignHardwareIndexRegistersArgument(PINTERMEDIATE_STATE		psState, 
										      PINDEXREGALLOC_CONTEXT	psContext, 
											  PINST						psInsertAfterPoint,
										      PINDEX					psIndex, 
										      IMG_BOOL					bSpill)
/*****************************************************************************
 FUNCTION	: AssignHardwareIndexRegistersArgument
    
 PURPOSE	: Assign hardware index registers for a single indexed argument.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Index register allocator state.
			  psInsertAfterPoint
								- Instruction which was originally after the
								current instruction.
			  psIndex			- Assign a hardware index register for the
								next use of this temporary register as an index.
			  bSpill			- If TRUE then move the index argument to a separate
								move instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PINDEX_REF		psRef = psIndex->psCurrentRef;
	PUSC_LIST_ENTRY	psNextRefListEntry;
	PINDEX_REF		psNextRef;
	PHW_INDEX		psHwIndex;

	/*
		Is a hardware index register currently assigned to this interval?
	*/
	if (psIndex->uHwRegNum == USC_UNDEF)		
	{
		IMG_UINT32		uHwReg;
		PINDEX			psBestIndex;
		IMG_UINT32		uBestIndexNextRef;
		PUSEDEF_CHAIN	psIndexUseDef = psIndex->psUseDef;
		PINST			psIndexSetupInst;
		PINST			psIndexSetupInsertPoint;
		IMG_UINT32		uBestHwReg;
		IMG_BOOL		bPreviousIndexUse;

		/*
			Find a hardware index register to use.
		*/
		psBestIndex = NULL;
		uBestHwReg = USC_UNDEF;
		uBestIndexNextRef = USC_UNDEF;
		for (uHwReg = 0; uHwReg < EURASIA_USE_INDEX_BANK_SIZE; uHwReg++)
		{
			PINDEX		psCurrentIndex = psContext->asHwRegs[uHwReg].psIndex;
			IMG_UINT32	uCurrentIndexNextRef;

			/* 
				Always select an unused hardware index register. 
			*/
			if (psCurrentIndex == NULL)
			{
				uBestHwReg = uHwReg;
				break;
			}
			/*
				Skip hardware index registers in use for other arguments to the current
				instruction.
			*/
			if (!bSpill && psCurrentIndex->bUsedHere)
			{
				continue;
			}
			/*
				Always select a hardware index register either with no other uses or
				next used in a different block.
			*/
			if (psCurrentIndex->psCurrentRef == NULL || 
				psCurrentIndex->psCurrentRef->psInst->psBlock != psRef->psInst->psBlock)
			{
				uBestHwReg = uHwReg;
				break;
			}
			
			/*
				Replace the hardware index register whose next use is furthest away.
			*/
			uCurrentIndexNextRef = psCurrentIndex->psCurrentRef->psInst->uBlockIndex;
			if (psBestIndex == NULL || uBestIndexNextRef < uCurrentIndexNextRef)
			{
				psBestIndex = psCurrentIndex;
				uBestHwReg = uHwReg;
				uBestIndexNextRef = uCurrentIndexNextRef;
			}
		}
		ASSERT(uBestHwReg != USC_UNDEF);

		/*
			Mark the spilled temporary as no longer assigned a hardware index
			register.
		*/
		bPreviousIndexUse = IMG_FALSE;
		if (psContext->asHwRegs[uBestHwReg].psIndex != NULL)
		{
			bPreviousIndexUse = IMG_TRUE;
			psContext->asHwRegs[uBestHwReg].psIndex->uHwRegNum = USC_UNDEF;
		}

		/*
			Mark this temporary as assigned to the selected hardware index
			register.
		*/
		psIndex->uHwRegNum = uBestHwReg;
		psContext->asHwRegs[uBestHwReg].psIndex = psIndex;

		/*
			Insert an instruction to copy the temporary into the hardware index
			register.
		*/
		psIndexSetupInst = AllocateInst(psState, psRef->psInst);
		SetOpcode(psState, psIndexSetupInst, IMOV);
		SetDest(psState, psIndexSetupInst, 0 /* uDestIdx */, USEASM_REGTYPE_INDEX, psIndex->uHwRegNum, UF_REGFORMAT_F32);
		if (psIndexUseDef->uType == USC_REGTYPE_REGARRAY)
		{
			SetArraySrc(psState,
						psIndexSetupInst,
						0 /* uSrcIdx */,
						psIndexUseDef->uNumber,
						psIndex->uArrayOffset,
						psIndexUseDef->eFmt);
		}
		else
		{
			SetSrc(psState, 
				   psIndexSetupInst, 
				   0 /* uSrcIdx */, 
				   psIndexUseDef->uType, 
				   psIndexUseDef->uNumber, 
				   psIndexUseDef->eFmt);
		}
		if (psRef->eType == REF_TYPE_DEST && bSpill)
		{
			/*
				If the indexed arguments are going to be moved to separate move instructions
				after the current instruction then insert the copy to the hardware index register
				after the current instruction.
			*/
			psIndexSetupInsertPoint = psInsertAfterPoint;
		}
		else
		{
			psIndexSetupInsertPoint = psRef->psInst;
		}
		InsertInstBefore(psState, 
						 psRef->psInst->psBlock, 
						 psIndexSetupInst, 
						 psIndexSetupInsertPoint);

		/*
			Check where the temporary is defined.
		*/
		if (psIndexUseDef->uType == USEASM_REGTYPE_TEMP)
		{
			PINST		psDefInst;
			IMG_UINT32	uDefDestIdx;

			psDefInst = UseDefGetDefInst(psState, psIndexUseDef->uType, psIndexUseDef->uNumber, &uDefDestIdx);

			/*
				If the temporary is defined in the current block and is after the previous last use of the index register we
				are copying to then we might be able to write directly to the index register from the defining
				instruction. We can't actually do the replacement until we know we aren't going to spill and reload
				the temporary later on. 
			*/
			if (
					psDefInst != NULL &&
					psDefInst->psBlock == psRef->psInst->psBlock &&
					(
						!bPreviousIndexUse || 
						psDefInst->uBlockIndex >= psContext->asHwRegs[psIndex->uHwRegNum].psLastUseInst->uBlockIndex
					) &&
					psDefInst->apsOldDest[uDefDestIdx] == NULL
				)
			{
				AppendToList(&psContext->sIndexMoveInstList, &psIndexSetupInst->sAvailableListEntry);
			}
		}
	}

	/*
		Replace the temporary by the hardware index register in the current set of
		source/destination arguments.
	*/
	FinaliseIndexRef(psState, psIndex, psRef);

	/*
		Move the index arguments to move instructions inserted either before
		or after the current instruction and replace them by the destinations
		of the moves.
	*/
	psHwIndex = &psContext->asHwRegs[psIndex->uHwRegNum];
	if (bSpill)
	{
		PINST	psNewIndexRefInsertPoint;

		if (psRef->eType == REF_TYPE_SRC)
		{
			psNewIndexRefInsertPoint = psRef->psInst;
		}
		else
		{
			ASSERT(psRef->eType == REF_TYPE_DEST);
			psNewIndexRefInsertPoint = psInsertAfterPoint;
		}
		MoveIndexReferences(psState, psNewIndexRefInsertPoint, psRef, &psHwIndex->psLastUseInst);
	}
	else
	{
		psHwIndex->psLastUseInst = psRef->psInst;
	}

	/*
		Update the temporary to point to the next unassigned set of arguments.
	*/
	psNextRefListEntry = psIndex->psCurrentRef->sListEntry.psNext;
	psNextRef = IMG_CONTAINING_RECORD(psNextRefListEntry, PINDEX_REF, sListEntry);

	psIndex->psCurrentRef = psNextRef;
}

static
IMG_VOID AssignHardwareIndexRegistersForInst(PINTERMEDIATE_STATE	psState, 
											 PINDEXREGALLOC_CONTEXT	psContext,
											 PINST					psInst,
											 PINDEX_REF				psFirstRef, 
											 IMG_UINT32				uRefCount)
/*****************************************************************************
 FUNCTION	: AssignHardwareIndexRegistersForInst
    
 PURPOSE	: Assign hardware index registers for all indexed arguments to an
			  instruction.

 PARAMETERS	: psState			- Compiler state.
			  psContext			- Index register allocator state.
			  psInst			- Instruction to assign for.
			  psFirstRef		- Start of the details of the indexed arguments
								to the instruction.
			  uRefCount			- Count of indexed arguments to the instruction.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{	
	IMG_UINT32		uRef;
	IMG_UINT32		uUsedHereCount;
	USC_LIST		sUsedHereList;
	USC_LIST		sSpilledList;
	PUSC_LIST_ENTRY	psListEntry;
	PINST			psInsertAfterInst = psInst->psNext;

	/*
		Create a list of the indices used in the current instruction.
	*/
	uUsedHereCount = 0;
	InitializeList(&sUsedHereList);
	for (uRef = 0; uRef < uRefCount; uRef++)
	{
		PINDEX_REF	psRef = psFirstRef + uRef;
		PINDEX		psIndex = &psContext->asIndices[psRef->uIndex];

		if (!psIndex->bUsedHere)
		{
			AppendToList(&sUsedHereList, &psIndex->sUsedHereListEntry);
			uUsedHereCount++;
			psIndex->bUsedHere = IMG_TRUE;
		}
	}

	/*
		Check for an instruction referencing more index registers than the hardware supports.
	*/
	InitializeList(&sSpilledList);
	if (uUsedHereCount > EURASIA_USE_INDEX_BANK_SIZE)
	{
		/*
			Sort the list of referenced index registers by the distance to the next reference
			after this instruction.
		*/
		InsertSortList(&sUsedHereList, SortByNextRefCB);

		/*
			Choose an interval and move all references to it from this instruction to MOV instructions
			inserted before or after this instructions.
		*/
		do
		{
			PINDEX	psSpilledIndex;

			psListEntry = RemoveListHead(&sUsedHereList);
			psSpilledIndex = IMG_CONTAINING_RECORD(psListEntry, PINDEX, sUsedHereListEntry);
			psSpilledIndex->bUsedHere = IMG_FALSE;
			AppendToList(&sSpilledList, psListEntry);

			uUsedHereCount--;
		} while (uUsedHereCount > EURASIA_USE_INDEX_BANK_SIZE);
	}

	/*
		Assign hardware index registers for all indexed sources which are going to be moved to
		separate move instructions.
	*/
	for (psListEntry = sSpilledList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PINDEX	psIndex;

		psIndex = IMG_CONTAINING_RECORD(psListEntry, PINDEX, sUsedHereListEntry);
		while (psIndex->psCurrentRef != NULL &&
			   psIndex->psCurrentRef->eType == REF_TYPE_SRC && 
			   psIndex->psCurrentRef->psInst == psInst)
		{
			AssignHardwareIndexRegistersArgument(psState, psContext, psInsertAfterInst, psIndex, IMG_TRUE /* bSpill */);
		}
	}

	/*
		Assign hardware index registers for all indexed sources/destinations which are going to
		stay at the current instruction.
	*/
	for (psListEntry = sUsedHereList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PINDEX	psIndex;

		psIndex = IMG_CONTAINING_RECORD(psListEntry, PINDEX, sUsedHereListEntry);
		while (psIndex->psCurrentRef != NULL && psIndex->psCurrentRef->psInst == psInst)
		{
			AssignHardwareIndexRegistersArgument(psState, psContext, psInsertAfterInst, psIndex, IMG_FALSE /* bSpill */);
		}
	}

	/*
		Assign hardware index registers for all indexed destinations which are going to be moved
		to separate move instructions.
	*/
	for (psListEntry = sSpilledList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PINDEX	psIndex;

		psIndex = IMG_CONTAINING_RECORD(psListEntry, PINDEX, sUsedHereListEntry);
		while (psIndex->psCurrentRef != NULL &&
			   psIndex->psCurrentRef->psInst == psInst)
		{
			ASSERT(psIndex->psCurrentRef->eType == REF_TYPE_DEST);
			AssignHardwareIndexRegistersArgument(psState, psContext, psInsertAfterInst, psIndex, IMG_TRUE /* bSpill */);
		}
	}

	/*
		Clear the "bUsedHere" flag on all indices used in the current instruction in preparation
		for processing the next instruction.
	*/
	for (psListEntry = sUsedHereList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PINDEX	psIndex;

		psIndex = IMG_CONTAINING_RECORD(psListEntry, PINDEX, sUsedHereListEntry);
		psIndex->bUsedHere = IMG_FALSE;
	}
}

static
IMG_UINT32 GetArrayLength(PINTERMEDIATE_STATE psState, IMG_UINT32 uType, IMG_UINT32 uNumber)
/*****************************************************************************
 FUNCTION	: GetArrayLength
    
 PURPOSE	: 

 PARAMETERS	: psState			- Compiler state.
			  uType				- Virtual register.
			  uNumber
			  
 RETURNS	: The array length.
*****************************************************************************/
{
	if (uType == USEASM_REGTYPE_TEMP)
	{
		return 1;
	}
	else
	{
		PUSC_VEC_ARRAY_REG	psArray;

		ASSERT(uType == USC_REGTYPE_REGARRAY);

		ASSERT(uNumber < psState->uNumVecArrayRegs);
		psArray = psState->apsVecArrayReg[uNumber];

		return psArray->uRegs;
	}
}

static
IMG_INT32 CmpIndexRef(IMG_VOID const* pvElem1, IMG_VOID const* pvElem2)
/*****************************************************************************
 FUNCTION	: CmpIndexRef
    
 PURPOSE	: Helper function to sort indexed arguments.

 PARAMETERS	: pvElem1		- Indexed arguments to compare.
			  pvElem2
			  
 RETURNS	: -ve	if pvElem1 < pvElem2
				0	if pvElem1 == pvElem2
			  +ve	if pvElem1 > pvElem2;
*****************************************************************************/
{
	PINDEX_REF	psRef1 = (PINDEX_REF)pvElem1;
	PINDEX_REF	psRef2 = (PINDEX_REF)pvElem2;
	PINST		psInst1 = psRef1->psInst;
	PINST		psInst2 = psRef2->psInst;
	PCODEBLOCK	psBlock1 = psInst1->psBlock;
	PCODEBLOCK	psBlock2 = psInst2->psBlock;
	PFUNC		psFunc1 = psBlock1->psOwner->psFunc;
	PFUNC		psFunc2 = psBlock2->psOwner->psFunc;

	if (psFunc1 == psFunc2)
	{
		if (psBlock1 == psBlock2)
		{
			if (psInst1 == psInst2)
			{
				if (psRef1->eType == psRef2->eType)
				{
					return psRef1->uBaseArgIdx - psRef2->uBaseArgIdx;
				}
				else
				{
					return psRef1->eType - psRef2->eType;
				}
			}
			else
			{
				return psInst1->uBlockIndex - psInst2->uBlockIndex;
			}
		}
		else
		{
			return psBlock1->uIdx - psBlock2->uIdx;
		}
	}
	else
	{
		return psFunc1->uLabel - psFunc2->uLabel;
	}
}

static
IMG_VOID FoldIndexRegisterWrite(PINTERMEDIATE_STATE psState, PINST psIndexMoveInst)
/*****************************************************************************
 FUNCTION	: FoldIndexRegisterWrite
    
 PURPOSE	: For a copy into an index register try to write to the index register
			  directly from the instruction defining the source to the copy.

 PARAMETERS	: psState			- Compiler state.
			  psIndexMoveInst	- Instruction copying a temporary register into
								a hardware instruction register.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSEDEF_CHAIN	psMovSrcUseDef;
	PARG			psMovDest;
	PARG			psMovSrc;
	PINST			psMovSrcDefInst;
	IMG_UINT32		uMovSrcDefDestIdx;

	psMovDest = &psIndexMoveInst->asDest[0];
	ASSERT(psMovDest->uType == USEASM_REGTYPE_INDEX);

	psMovSrc = &psIndexMoveInst->asArg[0];

	if (psMovSrc->uType != USEASM_REGTYPE_TEMP)
	{
		return;
	}
			
	psMovSrcUseDef = UseDefGet(psState, psMovSrc->uType, psMovSrc->uNumber);
	ASSERT(psMovSrcUseDef != NULL);
		
	/*
		Check the move source is defined by an instruction (not in the driver prolog).
	*/
	psMovSrcDefInst = UseDefGetDefInst(psState, psMovSrc->uType, psMovSrc->uNumber, &uMovSrcDefDestIdx);
	if (psMovSrcDefInst == NULL)
	{
		return;
	}

	/*
		Check the defining instruction supports the index register bank.
	*/
	if (!CanUseDest(psState, psMovSrcDefInst, uMovSrcDefDestIdx, psMovDest->uType, USC_REGTYPE_NOINDEX))
	{
		return;
	}

	/*
		Check the defining instruction doesn't partially define the move source.
	*/
	if (psMovSrcDefInst->apsOldDest[uMovSrcDefDestIdx] != NULL)
	{
		return;
	}

	/*
		Check if the move source is used in other instructions as a source.
	*/
	if (!UseDefIsSingleSourceUse(psState, psIndexMoveInst, 0 /* uSrcIdx */, psMovSrc))
	{
		return;
	}

	/*
		Move the hardware index register write to the defining instruction. We checked when generating
		the move instruction that there weren't any uses/defines of the hardware index register in 
		between.
	*/
	MoveDest(psState, psMovSrcDefInst, uMovSrcDefDestIdx, psIndexMoveInst, 0 /* uMoveFromDestIdx */);

	/*
		Update the mask of channels written by the defining instruction to account for hardware restrictions
		on masking when writing index registers.
	*/
	psMovSrcDefInst->auDestMask[uMovSrcDefDestIdx] = 
			GetMinimalDestinationMask(psState, 
									  psMovSrcDefInst, 
									  uMovSrcDefDestIdx, 
									  psMovSrcDefInst->auDestMask[uMovSrcDefDestIdx]);

	/*
		Free the move instruction.
	*/
	RemoveInst(psState, psIndexMoveInst->psBlock, psIndexMoveInst);
	FreeInst(psState, psIndexMoveInst);
}

IMG_INTERNAL
IMG_VOID AssignHardwareIndexRegisters(PINTERMEDIATE_STATE psState)
/*****************************************************************************
 FUNCTION	: AssignHardwareIndexRegisters
    
 PURPOSE	: Map shadow index registers to hardware index registers.

 PARAMETERS	: psState			- Compiler state.
			  
 RETURNS	: Nothing.
*****************************************************************************/
{
	PUSC_LIST_ENTRY			psListEntry;
	IMG_UINT32				uMaxIndexedArgCount;
	IMG_UINT32				uIndexedArgCount;
	PINDEX_REF				asAllRefs;
	INDEXREGALLOC_CONTEXT	sContext;
	PCODEBLOCK				psLastBlock;
	IMG_UINT32				uIndexCount;
	IMG_UINT32				uIndex;
	IMG_UINT32				uRef;
	IMG_UINT32				uHwReg;

	/*
		Exit early if dynamic indices aren't used in the program.
	*/
	if (IsListEmpty(&psState->sIndexUseTempList))
	{
		return;
	}

	/*
		Fix cases where the stride or static offset used at a dynamically indexed argument isn't supported by the 
		hardware. After this step each temporary register used as an index can be directly replaced by a hardware index 
		register.
	*/
	FixInvalidIndices(psState);

	/*
		Count the number of indexed source or destination arguments.
	*/
	uIndexCount = 0;
	uMaxIndexedArgCount = 0;
	for (psListEntry = psState->sIndexUseTempList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF_CHAIN	psUseDef;
		
		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF_CHAIN, sIndexUseTempListEntry);
		
		uIndexCount += GetArrayLength(psState, psUseDef->uType, psUseDef->uNumber);
		uMaxIndexedArgCount += psUseDef->uIndexUseCount;
	}

	/*
		Allocate an array to hold details of all the indexed arguments.
	*/
	asAllRefs = UscAlloc(psState, sizeof(asAllRefs[0]) * uMaxIndexedArgCount);

	/*
		Allocate an array to hold information about each temporary used as an index.
	*/
	sContext.asIndices = UscAlloc(psState, sizeof(sContext.asIndices[0]) * uIndexCount);

	/*
		Fill out an array with details of all the indexed arguments.
	*/
	uIndexedArgCount = 0;
	uIndex = 0;
	for (psListEntry = psState->sIndexUseTempList.psHead; psListEntry != NULL; psListEntry = psListEntry->psNext)
	{
		PUSEDEF_CHAIN	psUseDef;
		PUSC_LIST_ENTRY	psUseListEntry;
		PINDEX			psIndex;
		IMG_UINT32		uArrayLength;
		IMG_UINT32		uArrayStart;
		IMG_UINT32		uArray;
		
		psUseDef = IMG_CONTAINING_RECORD(psListEntry, PUSEDEF_CHAIN, sIndexUseTempListEntry);

		uArrayLength = GetArrayLength(psState, psUseDef->uType, psUseDef->uNumber);

		uArrayStart = uIndex;

		/*
			Set up details for a temporary used as an array index.
		*/
		for (uArray = 0; uArray < uArrayLength; uArray++)
		{
			psIndex = &sContext.asIndices[uIndex];
			psIndex->uHwRegNum = USC_UNDEF;
			psIndex->psUseDef = psUseDef;
			InitializeList(&psIndex->sRefList);
			psIndex->psCurrentRef = NULL;
			psIndex->bUsedHere = IMG_FALSE;
			psIndex->uArrayOffset = uArray;

			uIndex++;
		}

		/*
			For all arguments using the temporary as an array index.
		*/
		psUseListEntry = psUseDef->sList.psHead;
		while (psUseListEntry != NULL)
		{
			PINDEX_REF	psRef = &asAllRefs[uIndexedArgCount];

			/*
				Get details of a set of arguments requiring consecutive hardware
				register numbers.
			*/
			if (!GetIndexNextRef(psState, &psUseListEntry, psRef))
			{
				continue;
			}

			/*
				Make a link from the indexed arguments back to the temporary used as an
				index.
			*/
			ASSERT(psRef->psBaseArg->uIndexArrayOffset <= uArrayLength);
			psRef->uIndex = uArrayStart + psRef->psBaseArg->uIndexArrayOffset;

			uIndexedArgCount++;
		}
	}
	ASSERT(uIndexedArgCount <= uMaxIndexedArgCount);

	/*
		Sort the indexed arguments by function, flow control block, instruction ID, whether the
		argument is a source or destination and the index of the argument.
	*/
	qsort(asAllRefs, uIndexedArgCount, sizeof(asAllRefs[0]), CmpIndexRef);

	/*
		Create lists of the indexed arguments using each index in the same order.
	*/
	for (uRef = 0; uRef < uIndexedArgCount; uRef++)
	{
		PINDEX_REF	psRef = &asAllRefs[uRef];
		PINDEX		psIndex = &sContext.asIndices[psRef->uIndex];

		AppendToList(&psIndex->sRefList, &psRef->sListEntry);
		if (psIndex->psCurrentRef == NULL)
		{
			psIndex->psCurrentRef = psRef;
		}
	}

	/*
		Assign hardware index registers for all indexed arguments.
	*/
	uRef = 0;
	psLastBlock = NULL;
	InitializeList(&sContext.sIndexMoveInstList);
	for (uHwReg = 0; uHwReg < EURASIA_USE_INDEX_BANK_SIZE; uHwReg++)
	{
		sContext.asHwRegs[uHwReg].psIndex = NULL;
	}

	while (uRef < uIndexedArgCount)
	{
		PINST		psInst;
		IMG_UINT32	uFirstRef;
		IMG_UINT32	uRefCount;

		uFirstRef = uRef;
		psInst = asAllRefs[uFirstRef].psInst;

		/*
			Find all of the indexed arguments in the same instruction.
		*/
		uRef++;
		while (uRef < uIndexedArgCount && asAllRefs[uRef].psInst == psInst)
		{
			uRef++;
		}

		if (psInst->psBlock != psLastBlock)
		{
			/*	
				Clear any temporary register assigned to hardware index registers
				when moving between flow control blocks.
			*/
			for (uHwReg = 0; uHwReg < EURASIA_USE_INDEX_BANK_SIZE; uHwReg++)
			{
				if (sContext.asHwRegs[uHwReg].psIndex != NULL)
				{
					sContext.asHwRegs[uHwReg].psIndex->uHwRegNum = USC_UNDEF;
				}
				sContext.asHwRegs[uHwReg].psIndex = NULL;
				sContext.asHwRegs[uHwReg].psLastUseInst = NULL;
			}

			psLastBlock = psInst->psBlock; 
		}

		/*
			Assign hardware index registers to the current instruction.
		*/
		uRefCount = uRef - uFirstRef;
		AssignHardwareIndexRegistersForInst(psState, &sContext, psInst, &asAllRefs[uFirstRef], uRefCount);
	}

	/*
		For all move instructions copying a temporary into a hardware index register check
		whether we can write the hardware index register from the instruction defining
		the temporary.
	*/
	while ((psListEntry = RemoveListHead(&sContext.sIndexMoveInstList)) != NULL)
	{
		PINST	psIndexMoveInst;

		psIndexMoveInst = IMG_CONTAINING_RECORD(psListEntry, PINST, sAvailableListEntry);
		FoldIndexRegisterWrite(psState, psIndexMoveInst);
	}

	/*
		Free used memory.
	*/
	UscFree(psState, sContext.asIndices);
	UscFree(psState, asAllRefs);
}

/* EOF */
